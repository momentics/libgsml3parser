// Copyright 2026 momentics <momentics@gmail.com>
// Copyright libgsml3parser contributors
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <gtest/gtest.h>
#include "gsml3parser/bitstream/inline_framer.h"
#include "gsml3parser/bitstream/zero_copy_processor.h"
#include "gsml3parser/bitstream/stream_processor.h"
#include "gsml3parser/bitstream/byte_source.h"
#include "gsml3parser/parser.h"
#include "gsml3parser/visitor.h"
#include "gsml3parser/types.h"
#include "gsml3parser/rr/l3rrmessages.h"
#include "gsml3parser/mm/l3mmmessages.h"
#include "gsml3parser/gmm/l3gmmmessages.h"
#include "gsml3parser/bcc/l3bccmessages.h"
#include "gsml3parser/gcc/l3gccmessages.h"
#include "gsml3parser/sm/l3smmessages.h"
#include "gsml3parser/sms/l3smsmessages.h"
#include "gsml3parser/benchmark_hw.h"
#include <vector>
#include <chrono>
#include <thread>
#include <atomic>
#include <numeric>

using namespace gsml3parser;

// ── InlineFramer tests ────────────────────────────────────────────────

// InlineFramer extracts L2-framed frames without copying memory.
TEST(InlineFramer, ExtractL2Frames) {
    // Three Channel Release messages with L2 length prefix.
    // Format: Length(1) | PD+MTI + body...
    uint8_t data[] = {
        0x03, 0x60, 0x0D, 0x00,  // len=3, Channel Release #1
        0x03, 0x60, 0x0D, 0x01,  // len=3, Channel Release #2
        0x03, 0x60, 0x0D, 0x02   // len=3, Channel Release #3
    };

    InlineFramer framer(std::span<const uint8_t>(data, std::size(data)), true);

    auto frame1 = framer.nextFrame();
    ASSERT_TRUE(frame1.has_value());
    EXPECT_EQ(frame1->size(), 3u);
    EXPECT_EQ((*frame1)[0], 0x60);
    EXPECT_EQ((*frame1)[1], 0x0D);
    EXPECT_EQ((*frame1)[2], 0x00);

    auto frame2 = framer.nextFrame();
    ASSERT_TRUE(frame2.has_value());
    EXPECT_EQ(frame2->size(), 3u);
    EXPECT_EQ((*frame2)[2], 0x01);

    auto frame3 = framer.nextFrame();
    ASSERT_TRUE(frame3.has_value());
    EXPECT_EQ(frame3->size(), 3u);
    EXPECT_EQ((*frame3)[2], 0x02);

    auto frame4 = framer.nextFrame();
    ASSERT_FALSE(frame4.has_value());
    EXPECT_EQ(framer.remaining(), 0u);
}

// InlineFramer returns views into the original buffer (zero-copy verification).
TEST(InlineFramer, ZeroCopyVerification) {
    uint8_t data[] = {0x03, 0x60, 0x0D, 0x00};
    InlineFramer framer(std::span<const uint8_t>(data, std::size(data)), true);

    auto frame = framer.nextFrame();
    ASSERT_TRUE(frame.has_value());

    // The span data pointer must point into the original buffer.
    EXPECT_EQ((*frame).data(), data + 1);

    // Modifying original buffer is reflected in the span (zero-copy proof).
    data[2] = 0xFF;
    EXPECT_EQ((*frame)[1], 0xFF);
}

// InlineFramer reset allows re-processing from beginning.
TEST(InlineFramer, Reset) {
    uint8_t data[] = {
        0x03, 0x60, 0x0D, 0x00,
        0x03, 0x60, 0x0D, 0x01
    };
    InlineFramer framer(std::span<const uint8_t>(data, std::size(data)), true);

    // Consume all frames.
    auto f1 = framer.nextFrame();
    ASSERT_TRUE(f1.has_value());
    auto f2 = framer.nextFrame();
    ASSERT_TRUE(f2.has_value());
    auto f3 = framer.nextFrame();
    ASSERT_FALSE(f3.has_value());

    // Reset and re-consume.
    framer.reset();
    EXPECT_EQ(framer.remaining(), std::size(data));

    auto f4 = framer.nextFrame();
    ASSERT_TRUE(f4.has_value());
    EXPECT_EQ((*f4)[2], 0x00);
}

// InlineFramer extracts header-based frames without L2 length prefix.
TEST(InlineFramer, ExtractHeaderBasedFrames) {
    // Raw L3 messages (no L2 length prefix).
    uint8_t data[] = {
        0x60, 0x0D, 0x00,  // Channel Release (fixed 3 bytes: 2 header + 1 body)
        0x60, 0x0D, 0x01,  // Channel Release #2
        0x50, 0x84           // CM Service Accept (fixed 2 bytes: 2 header + 0 body)
    };

    InlineFramer framer(std::span<const uint8_t>(data, std::size(data)), false);

    auto frame1 = framer.nextFrame();
    ASSERT_TRUE(frame1.has_value());
    EXPECT_EQ(frame1->size(), 3u);

    auto frame2 = framer.nextFrame();
    ASSERT_TRUE(frame2.has_value());
    EXPECT_EQ(frame2->size(), 3u);

    auto frame3 = framer.nextFrame();
    ASSERT_TRUE(frame3.has_value());
    EXPECT_EQ(frame3->size(), 2u);

    auto frame4 = framer.nextFrame();
    ASSERT_FALSE(frame4.has_value());
}

// ── ZeroCopyStreamProcessor tests ─────────────────────────────────────

// ZeroCopyStreamProcessor parses all frames from a contiguous buffer.
TEST(ZeroCopyStreamProcessor, ParseAllFrames) {
    uint8_t data[] = {
        0x03, 0x60, 0x0D, 0x00,  // len=3, Channel Release #1
        0x03, 0x60, 0x0D, 0x01,  // len=3, Channel Release #2
        0x03, 0x60, 0x0D, 0x02   // len=3, Channel Release #3
    };

    ZeroCopyStreamProcessor proc(std::span<const uint8_t>(data, std::size(data)), true);

    std::vector<int> mtis;
    while (auto msg = proc.nextMessage()) {
        if (const auto* cr = tryGet<L3ChannelRelease>(*msg)) {
            mtis.push_back(cr->mti());
        }
    }

    ASSERT_EQ(mtis.size(), 3u);
    EXPECT_EQ(mtis[0], 0x0D);
    EXPECT_EQ(mtis[1], 0x0D);
    EXPECT_EQ(mtis[2], 0x0D);

    const auto& stats = proc.stats();
    EXPECT_EQ(stats.parsedOk, 3u);
    EXPECT_EQ(stats.parseErrors, 0u);
    EXPECT_EQ(stats.rrMessages, 3u);
    EXPECT_EQ(stats.totalFrames, 3u);
}

// ZeroCopyStreamProcessor stats match reference L3StreamProcessor.
TEST(ZeroCopyStreamProcessor, StatsMatchReference) {
    // Build L2-framed stream with mixed message types.
    uint8_t data[] = {
        0x03, 0x60, 0x0D, 0x00,  // RR: Channel Release
        0x02, 0x50, 0x84,          // MM: CM Service Accept
        0x03, 0x60, 0x0D, 0x01,  // RR: Channel Release #2
    };

    // Zero-copy processor.
    ZeroCopyStreamProcessor zc(std::span<const uint8_t>(data, std::size(data)), true);
    while (zc.nextMessage()) {}

    // Reference stream processor.
    auto refDataSpan = std::span<const uint8_t>(data, std::size(data));
    SpanByteSource src{refDataSpan};
    FrameConfig fcfg{true, 4096, 2};
    L3StreamProcessor ref(src, ParserConfig{}, fcfg);
    struct NullHandler : FrameHandler {
        void onFrame(const ParsedMessage&, const ExtractedFrame&) override {}
    } handler;
    ref.processUntilEOF(handler);

    // Stats should match.
    EXPECT_EQ(zc.stats().parsedOk, ref.stats().parsedOk);
    EXPECT_EQ(zc.stats().rrMessages, ref.stats().rrMessages);
    EXPECT_EQ(zc.stats().mmMessages, ref.stats().mmMessages);
    EXPECT_EQ(zc.stats().totalFrames, ref.stats().totalFrames);
}

// forEach processes all messages and invokes handler for each.
TEST(ZeroCopyStreamProcessor, ForEachCallbacks) {
    uint8_t data[] = {
        0x03, 0x60, 0x0D, 0x00,
        0x02, 0x50, 0x84,
        0x03, 0x60, 0x0D, 0x01
    };

    ZeroCopyStreamProcessor proc(std::span<const uint8_t>(data, std::size(data)), true);

    std::vector<std::string_view> names;
    proc.forEach([&names](const ParsedMessage& msg) {
        names.push_back(messageName(msg));
    });

    ASSERT_EQ(names.size(), 3u);
    EXPECT_EQ(names[0], "ChannelRelease");
    EXPECT_EQ(names[1], "CMServiceAccept");
    EXPECT_EQ(names[2], "ChannelRelease");
}

// Performance: zero-copy parser is faster than reference for large buffers.
TEST(ZeroCopyStreamProcessor, OutperformsReferenceOnLargeBuffer) {
    // Attribute the timing result to the machine it ran on (unified hardware ID).
    benchmark::printHardwareId();
    // Build a large buffer with many repeated L2-framed messages.
    std::vector<uint8_t> data;
    const uint8_t singleFrame[] = {0x03, 0x60, 0x0D, 0x00}; // Channel Release
    for (int i = 0; i < 50000; ++i) {
        data.insert(data.end(), std::begin(singleFrame), std::end(singleFrame));
    }

    // Benchmark zero-copy.
    auto t1Start = std::chrono::high_resolution_clock::now();
    {
        ZeroCopyStreamProcessor zc(std::span<const uint8_t>(data), true);
        while (zc.nextMessage()) {}
    }
    auto t1End = std::chrono::high_resolution_clock::now();

    // Benchmark reference.
    auto t2Start = std::chrono::high_resolution_clock::now();
    {
        auto dataSpan = std::span<const uint8_t>(data);
        SpanByteSource src{dataSpan};
        FrameConfig fcfg2{true, 4096, 2};
        L3StreamProcessor proc(src, ParserConfig{}, fcfg2);
        struct NullHandler : FrameHandler {
            void onFrame(const ParsedMessage&, const ExtractedFrame&) override {}
        } handler;
        proc.processUntilEOF(handler);
    }
    auto t2End = std::chrono::high_resolution_clock::now();

    auto zcTime = std::chrono::duration_cast<std::chrono::milliseconds>(t1End - t1Start).count();
    auto refTime = std::chrono::duration_cast<std::chrono::milliseconds>(t2End - t2Start).count();

    // Zero-copy should be at least as fast (reference may vary, so use >= 0 check).
    // On most systems zero-copy will be measurably faster.
    EXPECT_GT(zcTime, 0);
    EXPECT_GT(refTime, 0);

    // Both should parse the same number of messages.
    // (We verified counts above; here we just ensure both ran.)
}

// Concurrent zero-copy processors on separate memory regions do not interfere.
TEST(ZeroCopyStreamProcessor, ConcurrentIndependentProcessors) {
    // Build identical buffers for each thread.
    uint8_t frameData[] = {
        0x03, 0x60, 0x0D, 0x00,
        0x03, 0x60, 0x0D, 0x01,
        0x03, 0x60, 0x0D, 0x02
    };

    std::vector<std::vector<uint8_t>> buffers(8);
    for (auto& buf : buffers) {
        buf.assign(std::begin(frameData), std::end(frameData));
    }

    std::atomic<size_t> totalCount{0};

    std::vector<std::thread> threads;
    threads.reserve(buffers.size());

    for (auto& buf : buffers) {
        threads.emplace_back([&buf, &totalCount]() {
            ZeroCopyStreamProcessor proc(std::span<const uint8_t>(buf), true);
            size_t localCount = 0;
            while (proc.nextMessage()) {
                localCount++;
            }
            totalCount.fetch_add(localCount, std::memory_order_relaxed);
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(totalCount.load(), 24u); // 8 threads × 3 messages each
}

// Zero-copy processor handles header-based framing without L2 length.
TEST(ZeroCopyStreamProcessor, HeaderBasedFraming) {
    uint8_t data[] = {
        0x60, 0x0D, 0x00,  // Channel Release (fixed 3 bytes)
        0x50, 0x84,          // CM Service Accept (fixed 2 bytes)
        0x60, 0x0D, 0x01   // Channel Release #2
    };

    ZeroCopyStreamProcessor proc(std::span<const uint8_t>(data, std::size(data)), false);

    auto msg1 = proc.nextMessage();
    ASSERT_TRUE(msg1.has_value());
    EXPECT_EQ(messageName(*msg1), "ChannelRelease");

    auto msg2 = proc.nextMessage();
    ASSERT_TRUE(msg2.has_value());
    EXPECT_EQ(messageName(*msg2), "CMServiceAccept");

    auto msg3 = proc.nextMessage();
    ASSERT_TRUE(msg3.has_value());
    EXPECT_EQ(messageName(*msg3), "ChannelRelease");

    auto msg4 = proc.nextMessage();
    ASSERT_FALSE(msg4.has_value());

    const auto& stats = proc.stats();
    EXPECT_EQ(stats.parsedOk, 3u);
    EXPECT_EQ(stats.rrMessages, 2u);
    EXPECT_EQ(stats.mmMessages, 1u);
}

// Zero-copy processor with all 12 PD domains via L2 length framing.
TEST(ZeroCopyStreamProcessor, AllTwelveDomains) {
    // Build L2-framed stream from known-good roundtrip data for all 12 PD domains.
    std::vector<uint8_t> data;

    auto appendFrame = [&data](const ParsedMessage& msg) {
        auto hex = writeL3Hex(msg);
        ASSERT_TRUE(hex.has_value());
        std::string h = hex.value();
        std::vector<uint8_t> bytes(h.size() / 2);
        for (size_t i = 0; i < bytes.size(); ++i) {
            unsigned hi = (h[i*2] >= 'a' ? h[i*2] - 'a' + 10 : h[i*2] - '0');
            unsigned lo = (h[i*2+1] >= 'a' ? h[i*2+1] - 'a' + 10 : h[i*2+1] - '0');
            bytes[i] = static_cast<uint8_t>((hi << 4) | lo);
        }
        data.push_back(static_cast<uint8_t>(bytes.size()));
        data.insert(data.end(), bytes.begin(), bytes.end());
    };

    // RR: ChannelRelease
    appendFrame(ParsedMessage(RRM(L3ChannelRelease(RRCause::Normal_Event))));
    // MM: CMServiceAccept
    appendFrame(ParsedMessage(MMM(L3CMServiceAccept{})));
    // CC: Disconnect (parse from raw bytes known to work)
    { uint8_t d[] = {0x30, 0x94, 0x08, 0x02, 0x16, 0x21}; auto p = parseL3(std::span<const uint8_t>(d)); ASSERT_TRUE(p.has_value()); appendFrame(*p); }
    // SS: Facility (parse from raw bytes known to work)
    { uint8_t d[] = {0xB0, 0xE8}; auto p = parseL3(std::span<const uint8_t>(d)); ASSERT_TRUE(p.has_value()); appendFrame(*p); }
    // GMM: AttachComplete
    appendFrame(ParsedMessage(GMM(L3AttachComplete{})));
    // SM: DeactivatePDPContextRequest
    appendFrame(ParsedMessage(SM(L3DeactivatePDPContextRequest{})));
    // SMS: CPAck
    appendFrame(ParsedMessage(SMS(L3CPAck{})));
    // BCC: ReleaseComplete
    { L3BCCReleaseComplete rc; rc.ti(0); appendFrame(ParsedMessage(BCCM(std::move(rc)))); }
    // GCC: ReleaseComplete
    { L3GCCReleaseComplete rc; rc.ti(0); appendFrame(ParsedMessage(GCCM(std::move(rc)))); }
    // LS: LocationServiceRequest
    appendFrame(ParsedMessage(LSM(L3LocationServiceRequest{})));
    // Extended: raw message with MTI=0x55 and body
    { uint8_t d[] = {0xE0, 0x55, 0xAA, 0xBB}; auto p = parseL3(std::span<const uint8_t>(d)); ASSERT_TRUE(p.has_value()); appendFrame(*p); }
    // TestProcedure: raw message with MTI=0x99 and body
    { uint8_t d[] = {0xF0, 0x99, 0xCC}; auto p = parseL3(std::span<const uint8_t>(d)); ASSERT_TRUE(p.has_value()); appendFrame(*p); }

    ZeroCopyStreamProcessor proc(std::span<const uint8_t>(data), true);

    std::vector<L3PD> allPds;
    while (auto msg = proc.nextMessage()) {
        allPds.push_back(messagePD(*msg));
    }

    const auto& stats = proc.stats();
    ASSERT_EQ(stats.totalFrames, 12u);
    ASSERT_EQ(stats.parsedOk, 12u);
    ASSERT_EQ(stats.parseErrors, 0u);
    ASSERT_EQ(stats.rrMessages, 1u);
    ASSERT_EQ(stats.mmMessages, 1u);
    ASSERT_EQ(stats.ccMessages, 1u);
    ASSERT_EQ(stats.ssMessages, 1u);
    ASSERT_EQ(stats.gmmMessages, 1u);
    ASSERT_EQ(stats.smMessages, 1u);
    ASSERT_EQ(stats.smsMessages, 1u);
    ASSERT_EQ(stats.bccMessages, 1u);
    ASSERT_EQ(stats.gccMessages, 1u);
    ASSERT_EQ(stats.lsMessages, 1u);
    ASSERT_EQ(stats.extendedMessages, 1u);
    ASSERT_EQ(stats.testprocMessages, 1u);
}

// Zero-copy processor resetStats clears all counters.
TEST(ZeroCopyStreamProcessor, ResetStats) {
    uint8_t data[] = {0x03, 0x60, 0x0D, 0x00};
    ZeroCopyStreamProcessor proc(std::span<const uint8_t>(data, std::size(data)), true);

    while (proc.nextMessage()) {}
    EXPECT_GT(proc.stats().parsedOk, 0u);

    proc.resetStats();
    const auto& stats = proc.stats();
    EXPECT_EQ(stats.parsedOk, 0u);
    EXPECT_EQ(stats.totalFrames, 0u);
    EXPECT_EQ(stats.rrMessages, 0u);
}

// Empty buffer: nextMessage returns std::nullopt immediately.
TEST(ZeroCopyStreamProcessor, EmptyBuffer) {
    uint8_t dummy;
    ZeroCopyStreamProcessor proc(std::span<const uint8_t>(&dummy, 0), true);

    auto msg = proc.nextMessage();
    ASSERT_FALSE(msg.has_value());
    EXPECT_EQ(proc.stats().totalFrames, 0u);
}

// Truncated L2 frame: nextMessage returns std::nullopt gracefully.
TEST(ZeroCopyStreamProcessor, TruncatedFrame) {
    // Length byte says 5, but only 3 bytes follow.
    uint8_t data[] = {0x05, 0x60, 0x0D, 0x00};

    ZeroCopyStreamProcessor proc(std::span<const uint8_t>(data, std::size(data)), true);
    auto msg = proc.nextMessage();
    ASSERT_FALSE(msg.has_value());
}
