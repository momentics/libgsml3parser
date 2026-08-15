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

// Performance benchmark for parseL3() and L3StreamProcessor across all 12 PD
// domains (RR, MM, CC, SS, GMM, SM, SMS, BCC, GCC, LS, EXT, TST).  Also includes a mixed-
// domain stream benchmark with all message types interleaved.

#include <chrono>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "gsml3parser/parser.h"
#include "gsml3parser/bitstream/stream_processor.h"
#include "gsml3parser/bitstream/zero_copy_processor.h"
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

    printf("  %-40s %" PRIu64 " msgs  %8.4f s  %" PRIu64 " msg/s  (ok=%" PRIu64 " err=%" PRIu64 ")\n",
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

    printf("  %-40s %" PRIu64 " msgs  %8.4f s  %" PRIu64 " msg/s\n",
            label, handler.count, secs, perSec);
}

// Build an L2-length-prefixed stream for zero-copy benchmarks.
static std::vector<uint8_t> buildL2Stream(const uint8_t* msgs[], size_t nMsgs, uint64_t iterations) {
    // Compute total cycle size (length byte + each message).
    size_t cycleSize = 0;
    for (size_t i = 0; i < nMsgs; ++i) {
        auto result = parseL3(std::span<const uint8_t>(msgs[i], 10)); // rough probe
        cycleSize += 1; // length byte
        // Find actual message size by scanning for known lengths.
    }
    // Simpler: just build from raw data with known sizes.
    (void)nMsgs;
    return {};
}

static void runZeroCopyBenchmark(const char* label, std::span<const uint8_t> data) {
    auto start = std::chrono::high_resolution_clock::now();

    ZeroCopyStreamProcessor proc(data, true);
    uint64_t count = 0;
    while (auto msg = proc.nextMessage()) {
        (void)msg;
        ++count;
    }

    auto end = std::chrono::high_resolution_clock::now();

    double secs = std::chrono::duration<double>(end - start).count();
    uint64_t perSec = secs > 0 ? static_cast<uint64_t>(count / secs) : 0;

    printf("  %-40s %" PRIu64 " msgs  %8.4f s  %" PRIu64 " msg/s  (ok=%" PRIu64 " err=%" PRIu64 ")\n",
            label, count + proc.stats().parseErrors, secs, perSec,
            proc.stats().parsedOk, proc.stats().parseErrors);
}

// Build L2-framed data: each message preceded by its length byte.
static std::vector<uint8_t> buildL2Data(const std::vector<std::pair<const uint8_t*, size_t>>& msgs, uint64_t iterations) {
    size_t cycleSize = 0;
    for (const auto& [p, n] : msgs) {
        (void)p;
        cycleSize += 1 + n; // length byte + message
    }
    std::vector<uint8_t> data(cycleSize * iterations);
    uint8_t* ptr = data.data();
    for (uint64_t i = 0; i < iterations; ++i) {
        for (const auto& [p, n] : msgs) {
            *ptr = static_cast<uint8_t>(n);
            ++ptr;
            std::memcpy(ptr, p, n);
            ptr += n;
        }
    }
    return data;
}

int main() {
    printf("=== libgsml3parser Benchmark (12 PD Domains) ===\n\n");

    // Representative messages for each PD domain.
    // RR: Channel Release (3 bytes) - PD=0x6, MTI=0x0D
    uint8_t rrMsg[] = {0x60, 0x0D, 0x00};

    // MM: CM Service Accept (2 bytes) - PD=0x5, MTI=0x21
    uint8_t mmMsg[] = {0x50, 0x84};

    // CC: Disconnect (6 bytes) - PD=0x3, TI=7, TIF=0, MTI=0x25
    uint8_t ccMsg[] = {0x3E, 0x94, 0x08, 0x02, 0x16, 0x21};

    // SS: SupServ Facility (2 bytes) - PD=0xB, MTI=0x3A
    uint8_t ssMsg[] = {0xB0, 0xE8};

    // GMM: GMM Status (3 bytes) - PD=0x8, MTI=0x20
    uint8_t gmmMsg[] = {0x80, 0x20, 0x05};

    // SM: SM Status (4 bytes) - PD=0xA, MTI=0x55
    uint8_t smMsg[] = {0xA0, 0x55, 0x32, 0x01};

    // SMS: CP Ack (4 bytes) - PD=0x9, MTI=0x04
    uint8_t smsMsg[] = {0x90, 0x04, 0x01, 0x02};

    // BCC: Setup (2 bytes) - PD=0x1, MTI=0x01
    uint8_t bccMsg[] = {0x10, 0x01};

    // GCC: Setup (3 bytes) - PD=0x0, MTI=0x01
    uint8_t gccMsg[] = {0x00, 0x01, 0x02};

    // LS: LocationServiceRequest (2 bytes) - PD=0x0c, MTI=0x01
    uint8_t lsMsg[] = {0xC0, 0x01};

    // EXT: ExtendedMessage (2 bytes) - PD=0x0e, MTI=0x01
    uint8_t extMsg[] = {0xE0, 0x01};

    // TST: TestProcedureMessage (2 bytes) - PD=0x0f, MTI=0x01
    uint8_t tstMsg[] = {0xF0, 0x01};

    uint64_t iterations = 500000;

    printf("--- parseL3() Benchmark (%" PRIu64 " iterations each) ---\n", iterations);
    runParseBenchmark("RR ChannelRelease", rrMsg, iterations);
    runParseBenchmark("MM CMServiceAccept", mmMsg, iterations);
    runParseBenchmark("CC Disconnect", ccMsg, iterations);
    runParseBenchmark("SS SupServFacility", ssMsg, iterations);
    runParseBenchmark("GMM GMMStatus", gmmMsg, iterations);
    runParseBenchmark("SM SMStatus", smMsg, iterations);
    runParseBenchmark("SMS CPAck", smsMsg, iterations);
    runParseBenchmark("BCC Setup", bccMsg, iterations);
    runParseBenchmark("GCC Setup", gccMsg, iterations);
    runParseBenchmark("LS LocationServiceRequest", lsMsg, iterations);
    runParseBenchmark("EXT ExtendedMessage", extMsg, iterations);
    runParseBenchmark("TST TestProcedureMessage", tstMsg, iterations);

    printf("\n--- L3StreamProcessor Benchmark (%" PRIu64 " iterations each) ---\n", iterations);
    runStreamBenchmark("RR ChannelRelease", rrMsg, iterations);
    runStreamBenchmark("MM CMServiceAccept", mmMsg, iterations);
    runStreamBenchmark("CC Disconnect", ccMsg, iterations);
    runStreamBenchmark("SS SupServFacility", ssMsg, iterations);
    runStreamBenchmark("GMM GMMStatus", gmmMsg, iterations);
    runStreamBenchmark("SM SMStatus", smMsg, iterations);
    runStreamBenchmark("SMS CPAck", smsMsg, iterations);
    runStreamBenchmark("BCC Setup", bccMsg, iterations);
    runStreamBenchmark("GCC Setup", gccMsg, iterations);
    runStreamBenchmark("LS LocationServiceRequest", lsMsg, iterations);
    runStreamBenchmark("EXT ExtendedMessage", extMsg, iterations);
    runStreamBenchmark("TST TestProcedureMessage", tstMsg, iterations);

    printf("\n--- Mixed stream Benchmark (All 12 PD Domains) ---\n");
    // Build a mixed stream with all 9 message types interleaved.
    {
        size_t singleCycle = sizeof(rrMsg) + sizeof(mmMsg) + sizeof(ccMsg) +
                              sizeof(ssMsg) + sizeof(gmmMsg) + sizeof(smMsg) +
                              sizeof(smsMsg) + sizeof(bccMsg) + sizeof(gccMsg) +
                              sizeof(lsMsg) + sizeof(extMsg) + sizeof(tstMsg);
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
            std::memcpy(p, lsMsg, sizeof(lsMsg));    p += sizeof(lsMsg);
            std::memcpy(p, extMsg, sizeof(extMsg));   p += sizeof(extMsg);
            std::memcpy(p, tstMsg, sizeof(tstMsg));   p += sizeof(tstMsg);
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

        printf("  %-40s %" PRIu64 " msgs  %8.4f s  %" PRIu64 " msg/s\n",
                "Mixed (all 12 PD domains)", handler.count, secs, perSec);
    }

    // Zero-copy benchmark: compare against L3StreamProcessor for mixed stream.
    printf("\n--- ZeroCopyStreamProcessor Benchmark (All 12 PD Domains) ---\n");
    {
        std::vector<std::pair<const uint8_t*, size_t>> allMsgs{
            {rrMsg, sizeof(rrMsg)},   {mmMsg, sizeof(mmMsg)},   {ccMsg, sizeof(ccMsg)},
            {ssMsg, sizeof(ssMsg)},   {gmmMsg, sizeof(gmmMsg)}, {smMsg, sizeof(smMsg)},
            {smsMsg, sizeof(smsMsg)}, {bccMsg, sizeof(bccMsg)}, {gccMsg, sizeof(gccMsg)},
            {lsMsg, sizeof(lsMsg)},   {extMsg, sizeof(extMsg)}, {tstMsg, sizeof(tstMsg)},
        };
        uint64_t mixedIters = iterations / 12;
        auto l2Data = buildL2Data(allMsgs, mixedIters);

        runZeroCopyBenchmark("Zero-copy (all 12 PD domains)", std::span<const uint8_t>(l2Data));
    }

    printf("\n=== Benchmark complete ===\n");
    return 0;
}
