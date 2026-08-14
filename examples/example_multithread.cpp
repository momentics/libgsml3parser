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

// Multi-threaded parse benchmark across all 12 PD domains (RR, MM, CC, SS,
// GMM, SM, SMS, BCC, GCC, LS, EXT, TST).  Each thread uses its own immutable ParserConfig
// - no mutex needed for configuration.  Round-trip serialization is verified
// per message.

#include <gsml3parser/gsml3parser.hpp>
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

using namespace gsml3parser;

namespace {

// Shared counters - each thread uses its own ParserConfig (immutable, no mutex).
struct ThreadStats {
    std::atomic<uint64_t> parsed{0};
    std::atomic<uint64_t> errors{0};
    std::atomic<uint64_t> rrCount{0};
    std::atomic<uint64_t> mmCount{0};
    std::atomic<uint64_t> ccCount{0};
    std::atomic<uint64_t> ssCount{0};
    std::atomic<uint64_t> gmmCount{0};
    std::atomic<uint64_t> smCount{0};
    std::atomic<uint64_t> smsCount{0};
    std::atomic<uint64_t> bccCount{0};
    std::atomic<uint64_t> gccCount{0};
    std::atomic<uint64_t> lsCount{0};
    std::atomic<uint64_t> extCount{0};
    std::atomic<uint64_t> tstCount{0};
};

// Example messages for all 9 PD domains.
std::vector<std::string> sExampleHexes = {
    "600D00",                    // RR: Channel Release
    "5084",                      // MM: CM Service Accept
    "3E9408021621",              // CC: Disconnect (TI=7)
    "B0E8",                      // SS: Facility
    "802005",                     // GMM: GMM Status (cause=5)
    "A055320105",                 // SM: SM Status (cause=5)
    "90040102",                   // SMS: CP Ack (ref=2)
    "1001",                       // BCC: Setup
    "000102",                     // GCC: Setup
    "C001",                       // LS: LocationServiceRequest
    "E001",                       // EXT: ExtendedMessage
    "F001",                       // TST: TestProcedureMessage
    };

void workerThread(int id, ThreadStats& stats, int iterations) {
    // Each thread creates its own immutable ParserConfig - no mutex needed.
    ParserConfig cfg;
    cfg = cfg.withLogLevel(LogLevel::ERR);

    uint64_t localParsed = 0;
    uint64_t localErrors = 0;
    uint64_t localRR = 0, localMM = 0, localCC = 0, localSS = 0;
    uint64_t localGMM = 0, localSM = 0, localSMS = 0, localBCC = 0, localGCC = 0;
    uint64_t localLS = 0, localEXT = 0, localTST = 0;

    for (int i = 0; i < iterations; ++i) {
        const auto& hex = sExampleHexes[i % static_cast<int>(sExampleHexes.size())];

        auto result = parseL3Hex(hex, cfg);
        if (result) {
            ++localParsed;
            const auto& msg = *result;

            // Count by domain.
            switch (messagePD(msg)) {
                case L3PD::RadioResource:          ++localRR; break;
                case L3PD::MobilityManagement:     ++localMM; break;
                case L3PD::CallControl:            ++localCC; break;
                case L3PD::NonCallSS:              ++localSS; break;
                case L3PD::GPRSMobilityManagement: ++localGMM; break;
                case L3PD::GPRSSessionManagement:  ++localSM; break;
                case L3PD::SMS:                    ++localSMS; break;
                case L3PD::BroadcastCallControl:   ++localBCC; break;
                case L3PD::GroupCallControl:       ++localGCC; break;
                case L3PD::Location:               ++localLS; break;
                case L3PD::Extended:               ++localEXT; break;
                case L3PD::TestProcedure:          ++localTST; break;
                default: break;
            }

            // Round-trip: serialize and re-parse.
            auto reHex = writeL3Hex(msg);
            if (reHex) {
                auto reParsed = parseL3Hex(*reHex, cfg);
                if (!reParsed) {
                    ++localErrors;
                }
            }
        } else {
            ++localErrors;
        }
    }

    // Update shared counters atomically.
    stats.parsed.fetch_add(localParsed, std::memory_order_relaxed);
    stats.errors.fetch_add(localErrors, std::memory_order_relaxed);
    stats.rrCount.fetch_add(localRR, std::memory_order_relaxed);
    stats.mmCount.fetch_add(localMM, std::memory_order_relaxed);
    stats.ccCount.fetch_add(localCC, std::memory_order_relaxed);
    stats.ssCount.fetch_add(localSS, std::memory_order_relaxed);
    stats.gmmCount.fetch_add(localGMM, std::memory_order_relaxed);
    stats.smCount.fetch_add(localSM, std::memory_order_relaxed);
    stats.smsCount.fetch_add(localSMS, std::memory_order_relaxed);
    stats.bccCount.fetch_add(localBCC, std::memory_order_relaxed);
    stats.gccCount.fetch_add(localGCC, std::memory_order_relaxed);
    stats.lsCount.fetch_add(localLS, std::memory_order_relaxed);
    stats.extCount.fetch_add(localEXT, std::memory_order_relaxed);
    stats.tstCount.fetch_add(localTST, std::memory_order_relaxed);
}

} // anonymous namespace

int main(int argc, char* argv[]) {
    int numThreads = 4;
    int iterations = 100000;

    if (argc > 1) numThreads = std::stoi(argv[1]);
    if (argc > 2) iterations = std::stoi(argv[2]);

    std::cout << "Multi-threaded parse benchmark (12 PD domains)\n";
    std::cout << "  Threads:    " << numThreads << "\n";
    std::cout << "  Iterations: " << iterations << " / thread\n";
    std::cout << "  Total msgs: " << (static_cast<int64_t>(numThreads) * iterations) << "\n\n";

    ThreadStats stats;
    auto start = std::chrono::steady_clock::now();

    std::vector<std::thread> threads;
    threads.reserve(numThreads);

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back(workerThread, i, std::ref(stats), iterations);
    }

    for (auto& t : threads) {
        t.join();
    }

    auto end = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(end - start).count();

    std::cout << "--- Results ---\n";
    std::cout << "  Parsed:  " << stats.parsed.load() << "\n";
    std::cout << "  Errors:  " << stats.errors.load() << "\n";
    std::cout << "  RR msgs: " << stats.rrCount.load() << "\n";
    std::cout << "  MM msgs: " << stats.mmCount.load() << "\n";
    std::cout << "  CC msgs: " << stats.ccCount.load() << "\n";
    std::cout << "  SS msgs: " << stats.ssCount.load() << "\n";
    std::cout << "  GMM msgs: " << stats.gmmCount.load() << "\n";
    std::cout << "  SM msgs: " << stats.smCount.load() << "\n";
    std::cout << "  SMS msgs: " << stats.smsCount.load() << "\n";
    std::cout << "  BCC msgs: " << stats.bccCount.load() << "\n";
    std::cout << "  GCC msgs: " << stats.gccCount.load() << "\n";
    std::cout << "  LS msgs:  " << stats.lsCount.load() << "\n";
    std::cout << "  EXT msgs: " << stats.extCount.load() << "\n";
    std::cout << "  TST msgs: " << stats.tstCount.load() << "\n";
    std::cout << "  Time:    " << elapsed << "s\n";

    if (elapsed > 0) {
        double throughput = stats.parsed.load() / elapsed;
        std::cout << "  Throughput: " << static_cast<int64_t>(throughput) << " msgs/sec\n";
    }

    return 0;
}
