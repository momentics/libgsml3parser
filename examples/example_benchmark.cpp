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

// Performance benchmark for parseL3() and L3StreamProcessor across all 9 PD
// domains (RR, MM, CC, SS, GMM, SM, SMS, BCC, GCC).  Also includes a mixed-
// domain stream benchmark with all message types interleaved.

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "gsml3parser/parser.h"
#include "gsml3parser/bitstream/stream_processor.h"
#include "gsml3parser/visitor.h"

using namespace gsml3parser;

struct BenchmarkResult {
    const char* name;
    uint64_t messages;
    double seconds;
    uint64_t perSecond;
};

static void runParseBenchmark(const char* label, std::span<const uint8_t> singleMsg, uint64_t iterations) {
    size_t totalSize = singleMsg.size() * iterations;
    std::vector<uint8_t> data(totalSize);
    uint8_t* ptr = data.data();
    for (uint64_t i = 0; i < iterations; ++i) {
        std::memcpy(ptr, singleMsg.data(), singleMsg.size());
        ptr += singleMsg.size();
    }

    auto start = std::chrono::high_resolution_clock::now();
    uint64_t ok = 0;
    uint64_t errs = 0;

    for (uint64_t i = 0; i < iterations; ++i) {
        auto result = parseL3(std::span<const uint8_t>(data).subspan(i * singleMsg.size(), singleMsg.size()));
        if (result) {
            ok++;
        } else {
            errs++;
        }
    }
    auto end = std::chrono::high_resolution_clock::now();

    double secs = std::chrono::duration<double>(end - start).count();
    uint64_t perSec = secs > 0 ? static_cast<uint64_t>(iterations / secs) : 0;

    printf("  %-40s %llu msgs  %8.4f s  %llu msg/s  (ok=%llu err=%llu)\n",
           label, iterations, secs, perSec, ok, errs);
}

static void runStreamBenchmark(const char* label, std::span<const uint8_t> singleMsg, uint64_t iterations) {
    size_t totalSize = singleMsg.size() * iterations;
    std::vector<uint8_t> data(totalSize);
    uint8_t* ptr = data.data();
    for (uint64_t i = 0; i < iterations; ++i) {
        std::memcpy(ptr, singleMsg.data(), singleMsg.size());
        ptr += singleMsg.size();
    }

    SpanByteSource src(data);

    auto start = std::chrono::high_resolution_clock::now();

    struct CounterHandler : public FrameHandler {
        uint64_t count{0};
        void onFrame(const ParsedMessage&, const ExtractedFrame&) override { count++; }
        void onError(const ParseError&, std::span<const uint8_t>) override {}
    };
    CounterHandler handler;
    L3StreamProcessor proc(src);
    proc.processUntilEOF(handler);

    auto end = std::chrono::high_resolution_clock::now();

    double secs = std::chrono::duration<double>(end - start).count();
    uint64_t perSec = secs > 0 ? static_cast<uint64_t>(handler.count / secs) : 0;

    printf("  %-40s %llu msgs  %8.4f s  %llu msg/s\n",
           label, handler.count, secs, perSec);
}

int main() {
    printf("=== libgsml3parser Benchmark (9 PD Domains) ===\n\n");

    // Representative messages for each PD domain.
    // RR: Channel Release (3 bytes) — PD=0x6, MTI=0x0D
    uint8_t rrMsg[] = {0x60, 0x0D, 0x00};

    // MM: CM Service Accept (2 bytes) — PD=0x5, MTI=0x21
    uint8_t mmMsg[] = {0x50, 0x84};

    // CC: Disconnect (6 bytes) — PD=0x3, TI=7, TIF=0, MTI=0x25
    uint8_t ccMsg[] = {0x3E, 0x94, 0x08, 0x02, 0x16, 0x21};

    // SS: SupServ Facility (2 bytes) — PD=0xB, MTI=0x3A
    uint8_t ssMsg[] = {0xB0, 0xE8};

    // GMM: GMM Status (3 bytes) — PD=0x8, MTI=0x20
    uint8_t gmmMsg[] = {0x80, 0x20, 0x05};

    // SM: SM Status (4 bytes) — PD=0xA, MTI=0x55
    uint8_t smMsg[] = {0xA0, 0x55, 0x32, 0x01};

    // SMS: CP Ack (4 bytes) — PD=0x9, MTI=0x04
    uint8_t smsMsg[] = {0x90, 0x04, 0x01, 0x02};

    // BCC: Setup (2 bytes) — PD=0x1, MTI=0x01
    uint8_t bccMsg[] = {0x10, 0x01};

    // GCC: Setup (3 bytes) — PD=0x0, MTI=0x01
    uint8_t gccMsg[] = {0x00, 0x01, 0x02};

    uint64_t iterations = 500000;

    printf("--- parseL3() Benchmark (%llu iterations each) ---\n", iterations);
    runParseBenchmark("RR ChannelRelease", rrMsg, iterations);
    runParseBenchmark("MM CMServiceAccept", mmMsg, iterations);
    runParseBenchmark("CC Disconnect", ccMsg, iterations);
    runParseBenchmark("SS SupServFacility", ssMsg, iterations);
    runParseBenchmark("GMM GMMStatus", gmmMsg, iterations);
    runParseBenchmark("SM SMStatus", smMsg, iterations);
    runParseBenchmark("SMS CPAck", smsMsg, iterations);
    runParseBenchmark("BCC Setup", bccMsg, iterations);
    runParseBenchmark("GCC Setup", gccMsg, iterations);

    printf("\n--- L3StreamProcessor Benchmark (%llu iterations each) ---\n", iterations);
    runStreamBenchmark("RR ChannelRelease", rrMsg, iterations);
    runStreamBenchmark("MM CMServiceAccept", mmMsg, iterations);
    runStreamBenchmark("CC Disconnect", ccMsg, iterations);
    runStreamBenchmark("SS SupServFacility", ssMsg, iterations);
    runStreamBenchmark("GMM GMMStatus", gmmMsg, iterations);
    runStreamBenchmark("SM SMStatus", smMsg, iterations);
    runStreamBenchmark("SMS CPAck", smsMsg, iterations);
    runStreamBenchmark("BCC Setup", bccMsg, iterations);
    runStreamBenchmark("GCC Setup", gccMsg, iterations);

    printf("\n--- Mixed stream Benchmark (All 9 PD Domains) ---\n");
    // Build a mixed stream with all 9 message types interleaved.
    {
        size_t singleCycle = sizeof(rrMsg) + sizeof(mmMsg) + sizeof(ccMsg) +
                             sizeof(ssMsg) + sizeof(gmmMsg) + sizeof(smMsg) +
                             sizeof(smsMsg) + sizeof(bccMsg) + sizeof(gccMsg);
        uint64_t mixedIters = iterations / 9;
        std::vector<uint8_t> data(singleCycle * mixedIters);
        uint8_t* p = data.data();
        for (uint64_t i = 0; i < mixedIters; ++i) {
            std::memcpy(p, rrMsg, sizeof(rrMsg));  p += sizeof(rrMsg);
            std::memcpy(p, mmMsg, sizeof(mmMsg));  p += sizeof(mmMsg);
            std::memcpy(p, ccMsg, sizeof(ccMsg));  p += sizeof(ccMsg);
            std::memcpy(p, ssMsg, sizeof(ssMsg));  p += sizeof(ssMsg);
            std::memcpy(p, gmmMsg, sizeof(gmmMsg)); p += sizeof(gmmMsg);
            std::memcpy(p, smMsg, sizeof(smMsg));  p += sizeof(smMsg);
            std::memcpy(p, smsMsg, sizeof(smsMsg)); p += sizeof(smsMsg);
            std::memcpy(p, bccMsg, sizeof(bccMsg)); p += sizeof(bccMsg);
            std::memcpy(p, gccMsg, sizeof(gccMsg)); p += sizeof(gccMsg);
        }

        SpanByteSource src(data);

        auto start = std::chrono::high_resolution_clock::now();

        struct CounterHandler : public FrameHandler {
            uint64_t count{0};
            void onFrame(const ParsedMessage&, const ExtractedFrame&) override { count++; }
            void onError(const ParseError&, std::span<const uint8_t>) override {}
        };
        CounterHandler handler;
        L3StreamProcessor proc(src);
        proc.processUntilEOF(handler);

        auto end = std::chrono::high_resolution_clock::now();

        double secs = std::chrono::duration<double>(end - start).count();
        uint64_t perSec = secs > 0 ? static_cast<uint64_t>(handler.count / secs) : 0;

        printf("  %-40s %llu msgs  %8.4f s  %llu msg/s\n",
               "Mixed (all 9 PD domains)", handler.count, secs, perSec);
    }

    printf("\n=== Benchmark complete ===\n");
    return 0;
}
