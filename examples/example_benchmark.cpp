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
    uint64_t perSec = static_cast<uint64_t>(iterations / secs);

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
    uint64_t perSec = static_cast<uint64_t>(handler.count / secs);

    printf("  %-40s %llu msgs  %8.4f s  %llu msg/s\n",
           label, handler.count, secs, perSec);
}

int main() {
    printf("=== libgsml3parser Benchmark ===\n\n");

    // Representative messages for each PD domain.
    // RR: Channel Release (3 bytes)
    uint8_t rrMsg[] = {0x60, 0x0D, 0x00};

    // MM: CM Service Accept (2 bytes)
    uint8_t mmMsg[] = {0x50, 0x84};

    // CC: Call Proceeding (2 bytes) — PD=3(CC), TI=7, TIF=0, MTI=0x02(CallProceeding)
    uint8_t ccMsg[] = {0x3E, 0x08};

    // SS: SupServ Facility (2 bytes) — PD=0xB(NonCallSS), TI=7, TIF=0, MTI=0x3A(Facility)
    uint8_t ssMsg[] = {0xBE, 0xE8};

    uint64_t iterations = 500000;

    printf("--- parseL3() Benchmark (%llu iterations each) ---\n", iterations);
    runParseBenchmark("RR ChannelRelease", rrMsg, iterations);
    runParseBenchmark("MM CMServiceAccept", mmMsg, iterations);
    runParseBenchmark("CC CallProceeding", ccMsg, iterations);
    runParseBenchmark("SS SupServFacility", ssMsg, iterations);

    printf("\n--- L3StreamProcessor Benchmark (%llu iterations each) ---\n", iterations);
    runStreamBenchmark("RR ChannelRelease", rrMsg, iterations);
    runStreamBenchmark("MM CMServiceAccept", mmMsg, iterations);
    runStreamBenchmark("CC CallProceeding", ccMsg, iterations);
    runStreamBenchmark("SS SupServFacility", ssMsg, iterations);

    printf("\n--- Mixed stream Benchmark ---\n");
    // Build a mixed stream with all 4 message types interleaved.
    {
        size_t singleCycle = sizeof(rrMsg) + sizeof(mmMsg) + sizeof(ccMsg) + sizeof(ssMsg);
        uint64_t mixedIters = iterations / 4;
        std::vector<uint8_t> data(singleCycle * mixedIters);
        uint8_t* p = data.data();
        for (uint64_t i = 0; i < mixedIters; ++i) {
            std::memcpy(p, rrMsg, sizeof(rrMsg)); p += sizeof(rrMsg);
            std::memcpy(p, mmMsg, sizeof(mmMsg)); p += sizeof(mmMsg);
            std::memcpy(p, ccMsg, sizeof(ccMsg)); p += sizeof(ccMsg);
            std::memcpy(p, ssMsg, sizeof(ssMsg)); p += sizeof(ssMsg);
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
        uint64_t perSec = static_cast<uint64_t>(handler.count / secs);

        printf("  %-40s %llu msgs  %8.4f s  %llu msg/s\n",
               "Mixed RR+MM+CC+SS", handler.count, secs, perSec);
    }

    printf("\n=== Benchmark complete ===\n");
    return 0;
}
