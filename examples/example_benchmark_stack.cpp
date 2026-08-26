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

// Performance benchmark for GSM L3 stack components.
// Measures throughput and memory footprint of MSContext, TimerManager,
// TransactionManager, ChannelPool, and state machines under load.
// Uses std::chrono::high_resolution_clock for timing.
// Benchmark: 10K context allocations < 50ms on modern x86_64

#include <gsml3parser/gsml3parser.hpp>
#include <gsml3parser/benchmark_hw.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>
#include <span>
#include <string>
#include <vector>

using namespace gsml3parser;
using Clock = std::chrono::high_resolution_clock;

namespace {

std::string durationMs(std::chrono::nanoseconds ns) {
    double ms = static_cast<double>(ns.count()) / 1'000'000.0;
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << ms << " ms";
    return oss.str();
}

void printHeader(const std::string& name) {
    std::cout << "\n=== " << name << " ===\n";
}

// Benchmark 1: MSContext allocation and access.
void benchmarkMSContext() {
    printHeader("MSContext - 10K allocations");

    constexpr int N = 10000;

    std::cout << "  sizeof(MSContext) = " << sizeof(MSContext) << " bytes\n";
    std::cout << "  Memory for " << N << " contexts: "
              << (sizeof(MSContext) * N / 1024) << " KB\n";

    auto t0 = Clock::now();
    std::vector<MSContext> contexts;
    contexts.reserve(N);
    for (int i = 0; i < N; ++i) {
        contexts.push_back(MSContext::createWithTMSI(static_cast<uint32_t>(i + 1)));
        contexts.back().assignChannel(ChannelType::SDCCHType, 0, static_cast<uint8_t>(i % 16),
                                      static_cast<uint16_t>(100 + (i % 10)));
        contexts.back().setTimingAdvance(static_cast<uint8_t>(i % 64));
        contexts.back().setAuthenticated(i % 3 == 0);
        contexts.back().setRegistered(i % 2 == 0);
        contexts.back().setCiphered(i % 5 == 0);
    }
    auto t1 = Clock::now();

    volatile uint32_t sum = 0;
    volatile int authCount = 0;
    auto t2 = Clock::now();
    for (int i = 0; i < N; ++i) {
        sum += contexts[i].identity().tmsi();
        if (contexts[i].isAuthenticated()) authCount = authCount + 1;
    }
    auto t3 = Clock::now();

    std::cout << "  Allocation + init: " << durationMs(t1 - t0) << "\n";
    std::cout << "  Hot-path read (" << N << " contexts): "
              << durationMs(t3 - t2) << "\n";
    std::cout << "  Per-context read: " << std::fixed << std::setprecision(3)
              << (static_cast<double>((t3 - t2).count()) / N / 1000.0) << " us\n";
    std::cout << "  [sanity] sum=" << sum << " authCount=" << authCount << "\n";

    auto t4 = Clock::now();
    for (int i = 0; i < N; ++i) {
        contexts[i].releaseChannel();
    }
    auto t5 = Clock::now();
    std::cout << "  Release all channels: " << durationMs(t5 - t4) << "\n";
}

// Benchmark 2: TimerManager tick throughput.
void benchmarkTimerManager() {
    printHeader("TimerManager - tick throughput");

    constexpr int N = 1000;
    constexpr int TICKS = 10000;

    std::vector<TimerManager> managers(N);
    auto t0 = Clock::now();
    for (int i = 0; i < N; ++i) {
        managers[i].start(L3TimerId::T3101);
        managers[i].start(L3TimerId::T3102);
        managers[i].start(L3TimerId::T3106);
    }
    auto t1 = Clock::now();

    volatile int expiredCount = 0;
    auto t2 = Clock::now();
    for (int tick = 0; tick < TICKS; ++tick) {
        for (int i = 0; i < N; ++i) {
            managers[i].tick(std::chrono::milliseconds(1), [&expiredCount](L3TimerId) {
                expiredCount = expiredCount + 1;
            });
        }
    }
    auto t3 = Clock::now();

    std::array<L3TimerId, 32> expiredBuf;
    volatile size_t totalExpired = 0;
    auto t4 = Clock::now();
    for (int tick = 0; tick < TICKS; ++tick) {
        for (int i = 0; i < N; ++i) {
            totalExpired += managers[i].tick(std::chrono::milliseconds(1), std::span<L3TimerId>(expiredBuf));
        }
    }
    auto t5 = Clock::now();

    std::cout << "  Start 3 timers x " << N << " managers: " << durationMs(t1 - t0) << "\n";
    std::cout << "  Callback tick (" << TICKS << " x " << N << "): "
              << durationMs(t3 - t2) << "\n";
    std::cout << "  Span tick (" << TICKS << " x " << N << "): "
              << durationMs(t5 - t4) << "\n";
    double opsPerSec = static_cast<double>(TICKS * N) /
                       (static_cast<double>((t5 - t4).count()) / 1'000'000'000.0);
    std::cout << "  Throughput: " << std::fixed << std::setprecision(0)
              << opsPerSec << " ticks/sec\n";
    std::cout << "  [sanity] callback expired=" << expiredCount
              << " span expired=" << totalExpired << "\n";
}

// Benchmark 3: TransactionManager create/match/cleanup.
void benchmarkTransactionManager() {
    printHeader("TransactionManager - O(1) TI lookup");

    constexpr int N = 1000;

    std::vector<TransactionManager> managers(N);

    auto t0 = Clock::now();
    for (int i = 0; i < N; ++i) {
        uint8_t ti = static_cast<uint8_t>(i % 8);
        managers[i].create(L3PD::CallControl, L3Setup::MTI, ti, L3TimerId::T3101);
    }
    auto t1 = Clock::now();

    L3CalledPartyBCDNumber calledNum("123456");
    auto setup = L3Setup::builder().calledParty(calledNum).ti(1).build();
    ParsedMessage testMsg{CCM{std::move(setup)}};

    L3Header header{L3PD::CallControl, L3Setup::MTI, 1, true};
    auto t2 = Clock::now();
    volatile int matched = 0;
    for (int i = 0; i < N; ++i) {
        if (managers[i].match(header, testMsg)) {
            matched = matched + 1;
        }
    }
    auto t3 = Clock::now();

    for (int i = 0; i < N; ++i) {
        Transaction* txn = managers[i].get(1);
        if (txn) txn->complete();
        managers[i].cleanup();
    }

    std::cout << "  Create " << N << " transactions: " << durationMs(t1 - t0) << "\n";
    std::cout << "  Match (" << N << " lookups): " << durationMs(t3 - t2) << "\n";
    std::cout << "  Per-match time: " << std::fixed << std::setprecision(3)
              << (static_cast<double>((t3 - t2).count()) / N / 1000.0) << " us\n";
    std::cout << "  [sanity] matched=" << matched << "\n";

    std::cout << "  sizeof(Transaction) = " << sizeof(Transaction) << " bytes\n";
}

// Benchmark 4: ChannelPool allocate/release cycles.
void benchmarkChannelPool() {
    printHeader("ChannelPool - allocate/release stress");

    constexpr int NUM_CHANNELS = 100;
    constexpr int CYCLES = 10000;

    ChannelPool pool;

    for (int i = 0; i < NUM_CHANNELS / 3; ++i) {
        pool.addChannel({ChannelType::SDCCHType, 0, static_cast<uint8_t>(i),
                         static_cast<uint16_t>(100 + i)});
    }
    for (int i = 0; i < NUM_CHANNELS / 3; ++i) {
        pool.addChannel({ChannelType::TCHFType, 1, static_cast<uint8_t>(i),
                         static_cast<uint16_t>(200 + i)});
    }
    for (int i = 0; i < NUM_CHANNELS / 3; ++i) {
        pool.addChannel({ChannelType::TCHHType, 1, static_cast<uint8_t>(i),
                         static_cast<uint16_t>(300 + i)});
    }

    std::cout << "  Pool: " << pool.totalCount() << " channels\n";

    auto t0 = Clock::now();
    for (int cycle = 0; cycle < CYCLES; ++cycle) {
        ChannelType type;
        switch (cycle % 3) {
            case 0: type = ChannelType::SDCCHType; break;
            case 1: type = ChannelType::TCHFType; break;
            default: type = ChannelType::TCHHType; break;
        }
        auto ch = pool.allocate(type);
        if (ch) {
            pool.release(*ch);
        }
    }
    auto t1 = Clock::now();

    std::cout << "  Allocate + release (" << CYCLES << " cycles): "
              << durationMs(t1 - t0) << "\n";
    std::cout << "  Per-cycle time: " << std::fixed << std::setprecision(3)
              << (static_cast<double>((t1 - t0).count()) / CYCLES / 1000.0) << " us\n";
    std::cout << "  [sanity] free SDCCH=" << pool.freeCount(ChannelType::SDCCHType)
              << " free TCHF=" << pool.freeCount(ChannelType::TCHFType) << "\n";

    auto t2 = Clock::now();
    for (int cycle = 0; cycle < CYCLES; ++cycle) {
        uint8_t ra = static_cast<uint8_t>(cycle % 4);
        auto ch = pool.allocateVEA(ra);
        if (ch) {
            pool.release(*ch);
        }
    }
    auto t3 = Clock::now();
    std::cout << "  VEA allocate + release (" << CYCLES << " cycles): "
              << durationMs(t3 - t2) << "\n";
}

// Benchmark 5: State machine dispatch throughput.
void benchmarkStateMachine() {
    printHeader("State Machine - message dispatch");

    constexpr int N = 10000;

    RRStateMachine rrSM;
    rrSM.setState(RRStateMachine::State::ACTIVE);

    auto cipherComplete = L3CipheringModeComplete::builder().build();
    ParsedMessage cipherMsg{RRM{std::move(cipherComplete)}};

    auto measReport = L3MeasurementReport::builder()
        .measurementResults(L3MeasurementResults{})
        .build();
    ParsedMessage measMsg{RRM{std::move(measReport)}};

    auto t0 = Clock::now();
    for (int i = 0; i < N; ++i) {
        (void)rrSM.processMessage(cipherMsg);
    }
    auto t1 = Clock::now();

    auto t2 = Clock::now();
    for (int i = 0; i < N; ++i) {
        (void)rrSM.processMessage(measMsg);
    }
    auto t3 = Clock::now();

    std::cout << "  CipheringModeComplete x " << N << ": " << durationMs(t1 - t0) << "\n";
    std::cout << "  MeasurementReport x " << N << ": " << durationMs(t3 - t2) << "\n";
    std::cout << "  Per-message dispatch: " << std::fixed << std::setprecision(3)
              << (static_cast<double>((t3 - t2).count()) / N / 1000.0) << " us\n";

    MMStateMachine mmSM;
    mmSM.setState(MMStateMachine::State::REGISTERED);

    auto cmr = L3CMServiceRequest::builder().classmark(L3MobileStationClassmark2{}).build();
    ParsedMessage cmMsg{MMM{std::move(cmr)}};

    auto t4 = Clock::now();
    for (int i = 0; i < N; ++i) {
        (void)mmSM.processMessage(cmMsg);
    }
    auto t5 = Clock::now();
    std::cout << "  MM CMServiceRequest x " << N << ": " << durationMs(t5 - t4) << "\n";

    CCStateMachine ccSM;
    ccSM.setState(CCStateMachine::State::ACTIVE);

    auto disconnect = L3Disconnect::builder()
        .cause(CCCause::Normal_Call_Clearing)
        .build();
    ParsedMessage discMsg{CCM{std::move(disconnect)}};

    auto t6 = Clock::now();
    for (int i = 0; i < N; ++i) {
        (void)ccSM.processMessage(discMsg);
    }
    auto t7 = Clock::now();
    std::cout << "  CC Disconnect x " << N << ": " << durationMs(t7 - t6) << "\n";

    std::cout << "  sizeof(SMResult) = " << sizeof(SMResult) << " bytes\n";
}

// Benchmark 6: ProtocolDispatcher throughput.
void benchmarkDispatcher() {
    printHeader("ProtocolDispatcher - message routing");

    constexpr int N = 10000;

    ProtocolDispatcher dispatcher;
    volatile int handled = 0;

    dispatcher.registerHandler(L3PD::RadioResource, L3PagingResponse::MTI,
        makeSharedHandler([&handled](const ParsedMessage&, void*) { handled = handled + 1; }));

    dispatcher.registerHandler(L3PD::MobilityManagement, L3CMServiceRequest::MTI,
        makeSharedHandler([&handled](const ParsedMessage&, void*) { handled = handled + 1; }));

    dispatcher.registerHandler(L3PD::CallControl, L3Setup::MTI,
        makeSharedHandler([&handled](const ParsedMessage&, void*) { handled = handled + 1; }));

    auto pr = L3PagingResponse::builder().mobileId(L3MobileIdentity(0x12345678)).build();
    ParsedMessage prMsg{RRM{std::move(pr)}};

    auto cmr = L3CMServiceRequest::builder().classmark(L3MobileStationClassmark2{}).build();
    ParsedMessage cmMsg{MMM{std::move(cmr)}};

    L3CalledPartyBCDNumber calledNum("123456");
    auto setup = L3Setup::builder().calledParty(calledNum).ti(1).build();
    ParsedMessage setupMsg{CCM{std::move(setup)}};

    auto t0 = Clock::now();
    for (int i = 0; i < N; ++i) {
        switch (i % 3) {
            case 0: dispatcher.dispatch(prMsg, nullptr); break;
            case 1: dispatcher.dispatch(cmMsg, nullptr); break;
            default: dispatcher.dispatch(setupMsg, nullptr); break;
        }
    }
    auto t1 = Clock::now();

    std::cout << "  Dispatch " << N << " messages: " << durationMs(t1 - t0) << "\n";
    std::cout << "  Per-message dispatch: " << std::fixed << std::setprecision(3)
              << (static_cast<double>((t1 - t0).count()) / N / 1000.0) << " us\n";
    std::cout << "  [sanity] handled=" << handled << "\n";
}

// Benchmark 7: Combined full-stack stress test.
void benchmarkFullStack() {
    printHeader("Full Stack - combined stress test");

    constexpr int NUM_MS = 1000;
    constexpr int MSGS_PER_MS = 10;

    auto t0 = Clock::now();
    std::vector<MSContext> contexts;
    contexts.reserve(NUM_MS);
    std::vector<TimerManager> timers;
    timers.reserve(NUM_MS);
    std::vector<TransactionManager> txns;
    txns.reserve(NUM_MS);

    for (int i = 0; i < NUM_MS; ++i) {
        contexts.emplace_back(MSContext::createWithTMSI(static_cast<uint32_t>(i + 1)));
        contexts.back().assignChannel(ChannelType::SDCCHType, 0, static_cast<uint8_t>(i % 16),
                                      static_cast<uint16_t>(100));
        contexts.back().setTimingAdvance(static_cast<uint8_t>(i % 64));

        timers.emplace_back();
        timers.back().start(L3TimerId::T3101);
        timers.back().start(L3TimerId::T3106);

        txns.emplace_back();
        txns.back().create(L3PD::CallControl, L3Setup::MTI, static_cast<uint8_t>(i % 8),
                           L3TimerId::T3101);
    }
    auto t1 = Clock::now();

    auto cipherComplete = L3CipheringModeComplete::builder().build();
    ParsedMessage cipherMsg{RRM{std::move(cipherComplete)}};

    auto t2 = Clock::now();
    for (int i = 0; i < NUM_MS; ++i) {
        RRStateMachine rrSM;
        rrSM.setState(RRStateMachine::State::ACTIVE);
        for (int j = 0; j < MSGS_PER_MS; ++j) {
            (void)rrSM.processMessage(cipherMsg);
        }

        std::array<L3TimerId, 32> expired;
        (void)timers[i].tick(std::chrono::milliseconds(100), std::span<L3TimerId>(expired));
    }
    auto t3 = Clock::now();

    for (int i = 0; i < NUM_MS; ++i) {
        Transaction* txn = txns[i].get(1);
        if (txn) txn->complete();
        txns[i].cleanup();
    }

    std::cout << "  Setup " << NUM_MS << " MS (context+timers+txns): "
              << durationMs(t1 - t0) << "\n";
    std::cout << "  Process " << (NUM_MS * MSGS_PER_MS) << " messages + tick timers: "
              << durationMs(t3 - t2) << "\n";
    double totalOps = NUM_MS * MSGS_PER_MS;
    double opsPerSec = totalOps / (static_cast<double>((t3 - t2).count()) / 1'000'000'000.0);
    std::cout << "  Throughput: " << std::fixed << std::setprecision(0)
              << opsPerSec << " msg/sec\n";
    std::cout << "  Memory per MS (sizeof(SubscriberSession)): "
              << sizeof(SubscriberSession) << " bytes\n";
}

} // anonymous namespace

int main() {
    std::cout << "GSM L3 Stack Component Performance Benchmarks\n";
    std::cout << "================================================\n";
    // Attribute results to the machine: performance depends on CPU/RAM/OS.
    std::cout << "Hardware: " << benchmark::hardwareId() << "\n";

    benchmarkMSContext();
    benchmarkTimerManager();
    benchmarkTransactionManager();
    benchmarkChannelPool();
    benchmarkStateMachine();
    benchmarkDispatcher();
    benchmarkFullStack();

    std::cout << "\nAll benchmarks completed successfully.\n";
    return 0;
}
