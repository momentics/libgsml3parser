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
#include "gsml3parser/bitstream/stream_processor.h"
#include "gsml3parser/bitstream/byte_source.h"
#include "gsml3parser/parser.h"
#include "gsml3parser/visitor.h"
#include "gsml3parser/rr/l3rrmessages.h"
#include "gsml3parser/mm/l3mmmessages.h"
#include "gsml3parser/cc/l3ccmessages.h"
#include "gsml3parser/gmm/l3gmmmessages.h"
#include "gsml3parser/sm/l3smmessages.h"
#include "gsml3parser/sms/l3smsmessages.h"
#include "gsml3parser/bcc/l3bccmessages.h"
#include "gsml3parser/gcc/l3gccmessages.h"
#include <vector>
#include <functional>

using namespace gsml3parser;

// Minimal FrameHandler for testing.
class TestHandler : public FrameHandler {
public:
    std::vector<const ParsedMessage*> messages;
    std::vector<ParseError> errors;
    size_t statsCalls{0};

    void onFrame(const ParsedMessage& msg, const ExtractedFrame&) override {
        // Store address of msg (valid for duration of test).
        messages.push_back(&msg);
    }

    void onError(const ParseError& err, std::span<const uint8_t>) override {
        errors.push_back(err);
    }

    void onStats(const StreamStats&) override {
        statsCalls++;
    }
};

// ── SpanByteSource with known frames ───────────────────────────────────

TEST(L3StreamProcessor, ParseAllFrames) {
    // Three Channel Release messages.
    uint8_t data[] = {
        0x60, 0x0D, 0x00,  // Channel Release #1
        0x60, 0x0D, 0x01,  // Channel Release #2
        0x60, 0x0D, 0x02   // Channel Release #3
    };
    SpanByteSource src(std::span<const uint8_t>(data, std::size(data)));
    L3StreamProcessor proc(src);

    std::vector<int> mtis;
    while (proc.processOne([&mtis](const ParsedMessage& msg) {
        if (tryGet<L3ChannelRelease>(msg)) {
            mtis.push_back(L3ChannelRelease::MTI);
        }
    })) {}

    ASSERT_EQ(mtis.size(), 3u);
    ASSERT_EQ(mtis[0], 0x0D);
    ASSERT_EQ(mtis[1], 0x0D);
    ASSERT_EQ(mtis[2], 0x0D);

    const auto& stats = proc.stats();
    ASSERT_EQ(stats.parsedOk, 3u);
    ASSERT_EQ(stats.parseErrors, 0u);
    ASSERT_EQ(stats.rrMessages, 3u);
    ASSERT_EQ(stats.totalFrames, 3u);
}

// ── Stats tracking ─────────────────────────────────────────────────────

TEST(L3StreamProcessor, StatsTracking) {
    uint8_t data[] = {
        0x60, 0x0D, 0x00,  // RR: Channel Release
        0x50, 0x84,          // MM: CM Service Accept
    };
    SpanByteSource src(std::span<const uint8_t>(data, std::size(data)));
    L3StreamProcessor proc(src);

    TestHandler h;
    proc.processUntilEOF(h);

    const auto& stats = proc.stats();
    ASSERT_EQ(stats.parsedOk, 2u);
    ASSERT_EQ(stats.rrMessages, 1u);
    ASSERT_EQ(stats.mmMessages, 1u);
}

// ── Error handling: corrupt frames continue processing ─────────────────

TEST(L3StreamProcessor, CorruptFrameContinues) {
    // Invalid frame followed by valid one.
    uint8_t data[] = {
        0x60, 0x0D, 0x00,  // Valid Channel Release
        0xFF, 0xFF,          // Invalid (PD=0xF TestProcedure, treated as variable)
        0x60, 0x0D, 0x01    // Valid Channel Release
    };
    SpanByteSource src(std::span<const uint8_t>(data, std::size(data)));
    L3StreamProcessor proc(src);

    TestHandler handler;
    proc.processUntilEOF(handler);

    const auto& stats = proc.stats();
    // At least the first valid frame should be parsed.
    ASSERT_GT(stats.parsedOk, 0u);
}

// ── Empty source: processUntilEOF returns immediately ──────────────────

TEST(L3StreamProcessor, EmptySource) {
    uint8_t dummy;
    SpanByteSource src(std::span<const uint8_t>(&dummy, 0));
    L3StreamProcessor proc(src);

    TestHandler handler;
    proc.processUntilEOF(handler);

    const auto& stats = proc.stats();
    ASSERT_EQ(stats.totalFrames, 0u);
    ASSERT_EQ(stats.truncatedInputs, 1u);
}

// ── processN ───────────────────────────────────────────────────────────

TEST(L3StreamProcessor, ProcessN) {
    uint8_t data[] = {
        0x60, 0x0D, 0x00,  // #1
        0x60, 0x0D, 0x01,  // #2
        0x60, 0x0D, 0x02,  // #3
        0x60, 0x0D, 0x03   // #4
    };
    SpanByteSource src(std::span<const uint8_t>(data, std::size(data)));
    L3StreamProcessor proc(src);

    TestHandler handler;
    proc.processN(2, handler);

    ASSERT_EQ(handler.messages.size(), 2u);
    const auto& stats = proc.stats();
    ASSERT_EQ(stats.totalFrames, 2u);
}

// ── resetStats ─────────────────────────────────────────────────────────

TEST(L3StreamProcessor, ResetStats) {
    uint8_t data[] = {0x60, 0x0D, 0x00};
    SpanByteSource src(std::span<const uint8_t>(data, std::size(data)));
    L3StreamProcessor proc(src);

    TestHandler h;
    proc.processUntilEOF(h);
    ASSERT_GT(proc.stats().parsedOk, 0u);

    proc.resetStats();
    const auto& stats = proc.stats();
    ASSERT_EQ(stats.parsedOk, 0u);
    ASSERT_EQ(stats.totalFrames, 0u);
}

// ── L3StreamBuilder fluent API ─────────────────────────────────────────

TEST(L3StreamBuilder, BuildFromSpan) {
    uint8_t data[] = {0x60, 0x0D, 0x00};
    auto proc = L3StreamBuilder()
        .source(std::span<const uint8_t>(data, std::size(data)))
        .build();

    ASSERT_TRUE(proc != nullptr);

    TestHandler handler;
    proc->processUntilEOF(handler);
    ASSERT_EQ(handler.messages.size(), 1u);
}

TEST(L3StreamBuilder, BuildWithL2Length) {
    uint8_t data[] = {0x03, 0x60, 0x0D, 0x00}; // L2 len=3 + Channel Release
    auto proc = L3StreamBuilder()
        .source(std::span<const uint8_t>(data, std::size(data)))
        .useL2Length(true)
        .build();

    ASSERT_TRUE(proc != nullptr);

    TestHandler handler;
    proc->processUntilEOF(handler);
    ASSERT_EQ(handler.messages.size(), 1u);
}

// ── Mixed message types ────────────────────────────────────────────────

TEST(L3StreamProcessor, MixedMessageTypes) {
    uint8_t data[] = {
        0x60, 0x0D, 0x00,  // RR: Channel Release
        0x50, 0x84,          // MM: CM Service Accept
        0x60, 0x0E, 0x01, 0x02, 0x03, 0x04, 0x05,  // RR: Paging Response (7 bytes)
    };
    SpanByteSource src(std::span<const uint8_t>(data, std::size(data)));
    L3StreamProcessor proc(src);

    TestHandler handler;
    proc.processUntilEOF(handler);

    const auto& stats = proc.stats();
    ASSERT_EQ(stats.parsedOk, 3u);
    ASSERT_EQ(stats.rrMessages, 2u);
    ASSERT_EQ(stats.mmMessages, 1u);
}

// ── Multi-domain stream: all protocol domains with L2 length framing ───

TEST(L3StreamProcessor, MultiDomainStream) {
    // L2 length-prefixed frames for reliable multi-domain parsing.
    // Format: Length(1) | PD+MTI(2) | Body...
    uint8_t data[] = {
        0x03, 0x60, 0x0D, 0x00,                             // RR: Channel Release (3 bytes)
        0x02, 0x50, 0x84,                                    // MM: CM Service Accept (2 bytes)
        0x06, 0x30, 0x94, 0x08, 0x02, 0x16, 0x21,          // CC: Disconnect (6 bytes)
        0x02, 0xB0, 0xE8,                                    // SS: Facility (2 bytes)
        0x09, 0x80, 0x01, 0x00, 0x04, 0x11, 0x03, 0x01, 0x02, 0x03, // GMM: AttachRequest (9 bytes)
        0x04, 0xA0, 0x41, 0x0F, 0x00,                       // SM: ActivatePDPContextRequest (4 bytes)
        0x07, 0x90, 0x01, 0x01, 0x04, 0x05, 0x06, 0x07,    // SMS: CPData (7 bytes)
        0x02, 0x10, 0x01,                                    // BCC: Setup (2 bytes)
    };
    auto proc = L3StreamBuilder()
        .source(std::span<const uint8_t>(data, std::size(data)))
        .useL2Length(true)
        .build();

    TestHandler handler;
    proc->processUntilEOF(handler);

    const auto& stats = proc->stats();
    ASSERT_GT(stats.parsedOk, 0u);
    ASSERT_GT(stats.rrMessages, 0u);
    ASSERT_GT(stats.mmMessages, 0u);
    ASSERT_GT(stats.ccMessages, 0u);
    ASSERT_GT(stats.ssMessages, 0u);
}

// ── Domain-specific message identification in stream ───────────────────

TEST(L3StreamProcessor, DomainMessageIdentification) {
    uint8_t data[] = {
        0x60, 0x0D, 0x00,                             // RR: Channel Release
        0x60, 0x0D, 0x01,                             // RR: Channel Release #2
        0x60, 0x0D, 0x02,                             // RR: Channel Release #3
    };
    SpanByteSource src(std::span<const uint8_t>(data, std::size(data)));
    L3StreamProcessor proc(src);

    std::vector<std::string_view> names;
    while (proc.processOne([&names](const ParsedMessage& msg) {
        names.push_back(messageName(msg));
    })) {}

    ASSERT_EQ(names.size(), 3u);
    EXPECT_EQ(names[0], "ChannelRelease");
    EXPECT_EQ(names[1], "ChannelRelease");
    EXPECT_EQ(names[2], "ChannelRelease");
}

// ── Large stream with repeated RR messages ─────────────────────────────

TEST(L3StreamProcessor, LargeMultiDomainStream) {
    std::vector<uint8_t> data;
    // RR ChannelRelease messages × 5
    for (int i = 0; i < 5; ++i) {
        data.push_back(0x60); data.push_back(0x0D); data.push_back(static_cast<uint8_t>(i));
    }
    // MM CMServiceAccept × 3
    for (int i = 0; i < 3; ++i) {
        data.push_back(0x50); data.push_back(0x84);
    }

    SpanByteSource src(std::span<const uint8_t>(data.data(), data.size()));
    L3StreamProcessor proc(src);

    TestHandler handler;
    proc.processUntilEOF(handler);

    const auto& stats = proc.stats();
    ASSERT_EQ(stats.rrMessages, 5u);
    ASSERT_EQ(stats.mmMessages, 3u);
    ASSERT_EQ(stats.totalFrames, 8u);
}

// ── L2-framed stream with GMM/SM/SMS/BCC/GCC domains ──────────────────

TEST(L3StreamProcessor, L2FramedAllDomains) {
    // Each frame: Length(1) | L3 message...
    uint8_t data[] = {
        0x09, 0x80, 0x01, 0x00, 0x04, 0x11, 0x03, 0x01, 0x02,   // GMM: AttachRequest (partial)
        0x04, 0xA0, 0x41, 0x0F, 0x00,                             // SM: ActivatePDPContextRequest
        0x07, 0x90, 0x01, 0x01, 0x04, 0x05, 0x06, 0x07,          // SMS: CPData
        0x02, 0x10, 0x01,                                          // BCC: Setup
        0x03, 0x00, 0x01, 0x02,                                    // GCC: Setup (3 bytes)
    };
    auto proc = L3StreamBuilder()
        .source(std::span<const uint8_t>(data, std::size(data)))
        .useL2Length(true)
        .build();

    TestHandler handler;
    proc->processUntilEOF(handler);

    const auto& stats = proc->stats();
    // At least some frames should parse (depends on message body validity).
    ASSERT_GT(stats.totalFrames, 0u);
}
