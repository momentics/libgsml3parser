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

// Demonstrates zero-copy L3 message parsing from a contiguous buffer.
// Use case: memory-mapped PCAP file or DMA-received data.

#include <gsml3parser/bitstream/inline_framer.h>
#include <gsml3parser/bitstream/zero_copy_processor.h>
#include <gsml3parser/parser.h>
#include <gsml3parser/visitor.h>
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <span>
#include <vector>

using namespace gsml3parser;

namespace {

// Build L2-framed test data: each message preceded by its length byte.
std::vector<uint8_t> buildL2Data(const std::vector<std::span<const uint8_t>>& msgs, uint64_t iterations) {
    size_t cycleSize = 0;
    for (auto m : msgs) {
        cycleSize += 1 + m.size(); // length byte + message
    }
    std::vector<uint8_t> data(cycleSize * iterations);
    uint8_t* ptr = data.data();
    for (uint64_t i = 0; i < iterations; ++i) {
        for (auto m : msgs) {
            *ptr = static_cast<uint8_t>(m.size());
            ++ptr;
            std::memcpy(ptr, m.data(), m.size());
            ptr += m.size();
        }
    }
    return data;
}

} // anonymous namespace

int main() {
    printf("=== Zero-Copy L3 Parsing Demo ===\n\n");

    // Representative messages from multiple PD domains.
    uint8_t rrMsg[] = {0x60, 0x0D, 0x00};   // RR: Channel Release
    uint8_t mmMsg[] = {0x50, 0x84};          // MM: CM Service Accept
    uint8_t ccMsg[] = {0x3E, 0x94, 0x08, 0x02, 0x16, 0x21}; // CC: Disconnect

    std::vector<std::span<const uint8_t>> msgs{
        std::span{rrMsg}, std::span{mmMsg}, std::span{ccMsg},
    };

    // --- Demo 1: Parse a few frames with ZeroCopyStreamProcessor ---
    printf("--- Small Buffer Demo ---\n");
    {
        auto data = buildL2Data(msgs, 1);
        ZeroCopyStreamProcessor proc(std::span<const uint8_t>(data), true /* L2 length */);

        size_t count = 0;
        while (auto msg = proc.nextMessage()) {
            printf("  Frame %zu: %s (PD=%d, MTI=0x%02X)\n",
                   count++,
                   messageName(*msg).data(),
                   static_cast<int>(messagePD(*msg)),
                   messageMTI(*msg));
        }

        const auto& stats = proc.stats();
        printf("  Parsed %zu messages, %" PRIu64 " bytes remaining.\n", count, proc.remaining());
        printf("  Stats: ok=%" PRIu64 " err=%" PRIu64 "\n\n", stats.parsedOk, stats.parseErrors);
    }

    // --- Demo 2: forEach bulk processing ---
    printf("--- forEach Bulk Processing ---\n");
    {
        auto data = buildL2Data(msgs, 100);
        ZeroCopyStreamProcessor proc(std::span<const uint8_t>(data), true);

        size_t count = 0;
        proc.forEach([&count](const ParsedMessage& msg) {
            (void)msg;
            ++count;
        });

        printf("  forEach processed %zu messages\n", count);
        printf("  Stats: bytes=%" PRIu64 " frames=%" PRIu64 " ok=%" PRIu64 "\n\n",
               proc.stats().totalBytes, proc.stats().totalFrames, proc.stats().parsedOk);
    }

    // --- Demo 3: Performance comparison vs L3StreamProcessor ---
    printf("--- Performance Comparison ---\n");
    {
        uint64_t iterations = 50000;
        auto data = buildL2Data(msgs, iterations);

        // Zero-copy processor.
        auto t1Start = std::chrono::high_resolution_clock::now();
        ZeroCopyStreamProcessor zc(std::span<const uint8_t>(data), true);
        size_t zcCount = 0;
        while (auto msg = zc.nextMessage()) {
            (void)msg;
            ++zcCount;
        }
        auto t1End = std::chrono::high_resolution_clock::now();
        double zcTime = std::chrono::duration<double>(t1End - t1Start).count();

        printf("  ZeroCopyStreamProcessor:  %zu msgs  %.4f s  %" PRIu64 " msg/s\n",
               zcCount, zcTime,
               zcTime > 0 ? static_cast<uint64_t>(zcCount / zcTime) : 0);

        // Reference: standard L3StreamProcessor.
        SpanByteSource src(data);
        struct CounterHandler : public FrameHandler {
            size_t count{0};
            void onFrame(const ParsedMessage&, const ExtractedFrame&) override { ++count; }
            void onError(const ParseError&, std::span<const uint8_t>) override {}
        };
        CounterHandler handler;

        auto t2Start = std::chrono::high_resolution_clock::now();
        L3StreamProcessor proc(src, {}, FrameConfig{.useL2Length = true});
        proc.processUntilEOF(handler);
        auto t2End = std::chrono::high_resolution_clock::now();
        double refTime = std::chrono::duration<double>(t2End - t2Start).count();

        printf("  L3StreamProcessor:        %zu msgs  %.4f s  %" PRIu64 " msg/s\n",
               handler.count, refTime,
               refTime > 0 ? static_cast<uint64_t>(handler.count / refTime) : 0);

        if (refTime > 0 && zcTime > 0) {
            printf("  Speedup:                  %.2fx\n", refTime / zcTime);
        }
    }

    printf("\n=== Demo complete ===\n");
    return 0;
}
