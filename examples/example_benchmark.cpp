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
#include "gsml3parser/benchmark_hw.h"

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

static uint64_t runStreamBenchmark(const char* label, std::span<const uint8_t> singleMsg, uint64_t iterations) {
    // L2-length framing: deterministic boundaries for any message type
    // (audit P3-5: the previous header-based framing silently dropped
    // frames for variable-length messages and the benchmark never
    // checked the count).
    std::vector<std::pair<const uint8_t*, size_t>> one{ {singleMsg.data(), singleMsg.size()} };
    auto data = buildL2Data(one, iterations);

    SpanByteSource src(data);

    auto start = std::chrono::high_resolution_clock::now();

    struct CounterHandler : public FrameHandler {
        uint64_t count{0};
        void onFrame(const ParsedMessage&, const ExtractedFrame&) override { count++; }
        void onError(const ParseError&, std::span<const uint8_t>) override {}
    };
    CounterHandler handler;
    FrameConfig fcfg;
    fcfg.useL2Length = true;
    L3StreamProcessor proc(src, {}, fcfg);
    proc.processUntilEOF(handler);

    auto end = std::chrono::high_resolution_clock::now();

    double secs = std::chrono::duration<double>(end - start).count();
    uint64_t perSec = secs > 0 ? static_cast<uint64_t>(handler.count / secs) : 0;

    printf("  %-40s %" PRIu64 " msgs  %8.4f s  %" PRIu64 " msg/s\n",
            label, handler.count, secs, perSec);
    return handler.count;
}

static uint64_t runZeroCopyBenchmark(const char* label, std::span<const uint8_t> data) {
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
    // Count includes parse errors so the caller can verify that every
    // framed message was observed (audit P3-5).
    return count + proc.stats().parseErrors;
}

int main() {
    printf("=== libgsml3parser Benchmark (12 PD Domains) ===\n");
    // Attribute results to the machine: performance depends on CPU/RAM/OS.
    printf("Hardware: %s\n\n", benchmark::hardwareId().c_str());

    // Representative messages for each PD domain.
    // RR: Channel Release (3 bytes) - PD=0x6, MTI=0x0D
    uint8_t rrMsg[] = {0x60, 0x0D, 0x00};

    // MM: CM Service Accept (2 bytes) - PD=0x5, MTI=0x21
    uint8_t mmMsg[] = {0x50, 0x84};

    // CC: Disconnect — built wire-exact via the message builder (audit
    // P3-5: the stream benchmark now uses L2-length framing, so every
    // vector must be a valid complete message; the builder guarantees
    // that).
    auto ccBuilt = writeL3Bytes(ParsedMessage{CCM{L3Disconnect::builder().ti(3).build()}});
    std::vector<uint8_t> ccMsg = *ccBuilt;

    // SS: SupServ Facility (2 bytes) - PD=0xB, MTI=0x3A
    uint8_t ssMsg[] = {0xB0, 0xE8};

    // GMM: GMM Status (3 bytes) - PD=0x8, MTI=0x20
    uint8_t gmmMsg[] = {0x80, 0x20, 0x05};

    // SM: SM Status (4 bytes) - PD=0xA, MTI=0x55
    uint8_t smMsg[] = {0xA0, 0x55, 0x32, 0x01};

    // SMS: CP Ack (2 bytes) — CP-ACK has no body (24.011 8.1.3; audit
    // P3-5: the previous 4-byte vector was not a valid CP-ACK and
    // parsed as a HandoverAccess via the 4-byte short-message path).
    uint8_t smsMsg[] = {0x90, 0x04};

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
    uint64_t streamCount;
    streamCount = runStreamBenchmark("RR ChannelRelease", rrMsg, iterations);
    if (streamCount != iterations) { printf("FAIL: RR stream lost frames (%" PRIu64 "/%" PRIu64 ")\n", streamCount, iterations); return 1; }
    streamCount = runStreamBenchmark("MM CMServiceAccept", mmMsg, iterations);
    if (streamCount != iterations) { printf("FAIL: MM stream lost frames (%" PRIu64 "/%" PRIu64 ")\n", streamCount, iterations); return 1; }
    streamCount = runStreamBenchmark("CC Disconnect", ccMsg, iterations);
    if (streamCount != iterations) { printf("FAIL: CC stream lost frames (%" PRIu64 "/%" PRIu64 ")\n", streamCount, iterations); return 1; }
    streamCount = runStreamBenchmark("SS SupServFacility", ssMsg, iterations);
    if (streamCount != iterations) { printf("FAIL: SS stream lost frames (%" PRIu64 "/%" PRIu64 ")\n", streamCount, iterations); return 1; }
    streamCount = runStreamBenchmark("GMM GMMStatus", gmmMsg, iterations);
    if (streamCount != iterations) { printf("FAIL: GMM stream lost frames (%" PRIu64 "/%" PRIu64 ")\n", streamCount, iterations); return 1; }
    streamCount = runStreamBenchmark("SM SMStatus", smMsg, iterations);
    if (streamCount != iterations) { printf("FAIL: SM stream lost frames (%" PRIu64 "/%" PRIu64 ")\n", streamCount, iterations); return 1; }
    streamCount = runStreamBenchmark("SMS CPAck", smsMsg, iterations);
    if (streamCount != iterations) { printf("FAIL: SMS stream lost frames (%" PRIu64 "/%" PRIu64 ")\n", streamCount, iterations); return 1; }
    streamCount = runStreamBenchmark("BCC Setup", bccMsg, iterations);
    if (streamCount != iterations) { printf("FAIL: BCC stream lost frames (%" PRIu64 "/%" PRIu64 ")\n", streamCount, iterations); return 1; }
    streamCount = runStreamBenchmark("GCC Setup", gccMsg, iterations);
    if (streamCount != iterations) { printf("FAIL: GCC stream lost frames (%" PRIu64 "/%" PRIu64 ")\n", streamCount, iterations); return 1; }
    streamCount = runStreamBenchmark("LS LocationServiceRequest", lsMsg, iterations);
    if (streamCount != iterations) { printf("FAIL: LS stream lost frames (%" PRIu64 "/%" PRIu64 ")\n", streamCount, iterations); return 1; }
    streamCount = runStreamBenchmark("EXT ExtendedMessage", extMsg, iterations);
    if (streamCount != iterations) { printf("FAIL: EXT stream lost frames (%" PRIu64 "/%" PRIu64 ")\n", streamCount, iterations); return 1; }
    streamCount = runStreamBenchmark("TST TestProcedureMessage", tstMsg, iterations);
    if (streamCount != iterations) { printf("FAIL: TST stream lost frames (%" PRIu64 "/%" PRIu64 ")\n", streamCount, iterations); return 1; }

    printf("\n--- Mixed stream Benchmark (All 12 PD Domains) ---\n");
    // Build a mixed stream with all 12 message types interleaved
    // (L2-length framing, audit P3-5).
    std::vector<std::pair<const uint8_t*, size_t>> allMsgs{
        {rrMsg, sizeof(rrMsg)},   {mmMsg, sizeof(mmMsg)},   {ccMsg.data(), ccMsg.size()},
        {ssMsg, sizeof(ssMsg)},   {gmmMsg, sizeof(gmmMsg)}, {smMsg, sizeof(smMsg)},
        {smsMsg, sizeof(smsMsg)}, {bccMsg, sizeof(bccMsg)}, {gccMsg, sizeof(gccMsg)},
        {lsMsg, sizeof(lsMsg)},   {extMsg, sizeof(extMsg)}, {tstMsg, sizeof(tstMsg)},
    };
    {
        uint64_t mixedIters = iterations / 12;
        auto data = buildL2Data(allMsgs, mixedIters);

        SpanByteSource src(data);

        auto start = std::chrono::high_resolution_clock::now();

        struct CounterHandler : public FrameHandler {
            uint64_t count{0};
            void onFrame(const ParsedMessage&, const ExtractedFrame&) override { count++; }
            void onError(const ParseError&, std::span<const uint8_t>) override {}
        };
        CounterHandler handler;
        FrameConfig fcfg;
        fcfg.useL2Length = true;
        L3StreamProcessor proc(src, {}, fcfg);
        proc.processUntilEOF(handler);

        auto end = std::chrono::high_resolution_clock::now();

        double secs = std::chrono::duration<double>(end - start).count();
        uint64_t perSec = secs > 0 ? static_cast<uint64_t>(handler.count / secs) : 0;

        printf("  %-40s %" PRIu64 " msgs  %8.4f s  %" PRIu64 " msg/s\n",
                "Mixed (all 12 PD domains)", handler.count, secs, perSec);

        if (handler.count != mixedIters * 12) {
            printf("FAIL: mixed stream lost frames (%" PRIu64 "/%" PRIu64 ")\n",
                   handler.count, mixedIters * 12);
            return 1;
        }
    }

    // Zero-copy benchmark: compare against L3StreamProcessor for mixed stream.
    printf("\n--- ZeroCopyStreamProcessor Benchmark (All 12 PD Domains) ---\n");
    {
        uint64_t mixedIters = iterations / 12;
        auto l2Data = buildL2Data(allMsgs, mixedIters);

        uint64_t zcCount = runZeroCopyBenchmark("Zero-copy (all 12 PD domains)", std::span<const uint8_t>(l2Data));
        if (zcCount != mixedIters * 12) { printf("FAIL: zero-copy stream lost frames\n"); return 1; }
    }

    printf("\n=== Benchmark complete ===\n");
    return 0;
}
