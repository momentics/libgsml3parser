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

// High-load validation of the C ABI: 1M sessions through the C registry
// API (the same budgets as the C++ Stress._1MSession test — the C
// boundary adds one call indirection, no allocations), parse-in-into
// overhead (bounded vs an identical C++ path that does the same work
// without crossing the ABI), and concurrent independent handles.

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

#include <gsml3parser/gsml3parser_c.h>
#include <gsml3parser/benchmark_hw.h>
#include <gsml3parser/parser.h>
#include <gsml3parser/visitor.h>
#include <gsml3parser/rr/l3rrmessages.h>
#include <gsml3parser/mm/l3mmmessages.h>
#include <gsml3parser/cc/l3ccmessages.h>

using namespace gsml3parser;

namespace {

double msBetween(std::chrono::steady_clock::time_point a,
                 std::chrono::steady_clock::time_point b) {
    return std::chrono::duration_cast<std::chrono::microseconds>(b - a).count() / 1000.0;
}

} // namespace

// Test: 1M sessions created/looked up/ticked through the C API with the
// same budgets as Stress._1MSession_Create_Lookup_Tick_Scale (sharded-32
// registry, matching the C++ baseline).
TEST(CApiStress, _1MSession_Create_Lookup_Tick_Scale) {
    benchmark::printHardwareId();
    gsml3_registry* r = gsml3_registry_new(32);
    ASSERT_NE(r, nullptr);

    constexpr uint32_t N = 1'000'000;
    auto t0 = std::chrono::steady_clock::now();
    uint32_t created = 0;
    for (uint32_t i = 1; i <= N; ++i)
        if (gsml3_registry_create_by_tmsi(r, i)) ++created;
    auto t1 = std::chrono::steady_clock::now();
    EXPECT_EQ(created, N);
    EXPECT_EQ(gsml3_registry_count(r), static_cast<size_t>(N));

    uint32_t found = 0;
    for (uint32_t i = 1; i <= N; ++i)
        if (gsml3_registry_find_by_tmsi(r, i)) ++found;
    auto t2 = std::chrono::steady_clock::now();
    EXPECT_EQ(found, N);

    // Start a timer in 10K sessions, tick all (O(active) path).
    for (uint32_t i = 1; i <= 10000; ++i) {
        gsml3_session* s = gsml3_registry_find_by_tmsi(r, i);
        EXPECT_EQ(gsml3_session_timer_start(s, GSML3_TIMER_T3101), 1);
    }
    std::vector<gsml3_timer_expiry> expired(N);
    auto t3 = std::chrono::steady_clock::now();
    size_t n = gsml3_registry_tick_timers(r, 3000, expired.data(), expired.size());
    auto t4 = std::chrono::steady_clock::now();
    EXPECT_EQ(n, 10000u) << "only the 10K active timers should expire";

    const double createMs = msBetween(t0, t1);
    const double lookupMs = msBetween(t1, t2);
    const double tickMs = msBetween(t3, t4);
    std::printf("C API 1M: create %.1f ms, lookup %.1f ms, tick(10K active) %.1f ms\n",
                createMs, lookupMs, tickMs);

#if !defined(GSML3PARSER_ASAN) && !defined(GSML3PARSER_DEBUG)
    EXPECT_LT(createMs, 5000.0) << "1M create too slow";
    EXPECT_LT(lookupMs, 2000.0) << "1M lookup too slow";
    EXPECT_LT(tickMs, 50.0) << "tick(10K active) too slow";
#endif
    gsml3_registry_free(r);
}

// Test: the zero-allocation reparse path (gsml3_parse_l3_into) costs no
// more than a small constant boundary overhead over doing exactly the same
// work in C++ without crossing the ABI: parseL3 + error check + move into
// a persistent ParsedMessage (which is what gsml3_parse_l3_into itself
// does, so the comparison isolates the boundary — the call, the
// thread-local error state and the try/catch guard). Both sides run in
// interleaved rounds; the medians are compared, so isolated OS load spikes
// cannot flake the budget. The asserted 10% covers that boundary at this
// message scale; a per-call allocation or an extra copy would add tens of
// percent and fail.
TEST(CApiStress, ParseInto_Throughput_COverheadBounded) {
    benchmark::printHardwareId();
    // A mixed set of typical messages (RR + MM + CC), serialized once.
    std::vector<std::vector<uint8_t>> frames;
    auto add = [&](auto build) {
        auto bytes = writeL3Bytes(build());
        ASSERT_TRUE(bytes);
        frames.push_back(std::move(*bytes));
    };
    add([] { return ParsedMessage{RRM(L3ChannelRelease{RRCause::Normal_Event})}; });
    add([] { return ParsedMessage{MMM(L3CMServiceAccept{})}; });
    add([] { return ParsedMessage{CCM(L3Disconnect::builder().ti(1).build())}; });
    add([] { return ParsedMessage{RRM(L3AssignmentComplete::builder().build())}; });

    constexpr int kItersPerRound = 200'000;
    constexpr int kRounds = 8;
    double refMs[kRounds] = {};
    double cApiMs[kRounds] = {};
    ParsedMessage into{};

    gsml3_message* m = gsml3_parse_l3(frames[0].data(), frames[0].size(), nullptr);
    ASSERT_NE(m, nullptr);
    for (int round = 0; round < kRounds; ++round) {
        // C++ reference: identical work, no C boundary.
        auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < kItersPerRound; ++i) {
            const auto& f = frames[i % frames.size()];
            auto r = parseL3(f);
            if (!r) break;
            into = std::move(r.value());
        }
        auto t1 = std::chrono::steady_clock::now();

        // C API: one reused handle, parse_into per message (hot path).
        for (int i = 0; i < kItersPerRound; ++i) {
            const auto& f = frames[i % frames.size()];
            if (gsml3_parse_l3_into(m, f.data(), f.size(), nullptr) != GSML3_OK) break;
        }
        auto t2 = std::chrono::steady_clock::now();

        refMs[round] = msBetween(t0, t1);
        cApiMs[round] = msBetween(t1, t2);
    }
    gsml3_message_free(m);
    // Keep `into` observable so the reference loops cannot be eliminated.
    volatile int sink = messageMTI(into);
    (void)sink;

    std::sort(refMs, refMs + kRounds);
    std::sort(cApiMs, cApiMs + kRounds);
    const double cppMs = refMs[kRounds / 2];   // medians over the rounds
    const double cMs = cApiMs[kRounds / 2];
    std::printf("parse ref %.1f ms vs C parse_into %.1f ms (%.1f%%, median of %d rounds)\n",
                cppMs, cMs, cMs * 100.0 / cppMs, kRounds);

#if !defined(GSML3PARSER_ASAN) && !defined(GSML3PARSER_DEBUG)
    EXPECT_LE(cMs, cppMs * 1.10) << "C parse_into overhead over budget";
#endif
}

// Test: 8 threads x 1000 iterations of parse -> query -> serialize ->
// free on independent handles (the name contains "Concurrent" so the
// TSan CI filter picks it up).
TEST(CApiStress, Concurrent_IndependentHandles) {
    benchmark::printHardwareId();
    std::vector<std::thread> threads;
    std::atomic<int> failures{0};
    for (int t = 0; t < 8; ++t) {
        threads.emplace_back([&failures]() {
            for (int i = 0; i < 1000; ++i) {
                gsml3_message* m = gsml3_parse_l3_hex("60 0D 00", nullptr);
                if (!m || gsml3_message_pd(m) != GSML3_PD_RR ||
                        gsml3_message_mti(m) != 0x0D) {
                    ++failures;
                }
                uint8_t buf[64];
                if (m && gsml3_message_write(m, buf, sizeof(buf)) != 3) ++failures;
                gsml3_message_free(m);
            }
        });
    }
    for (auto& th : threads) th.join();
    EXPECT_EQ(failures.load(), 0);
}
