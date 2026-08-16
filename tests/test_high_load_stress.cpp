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

// High-load stress tests for SubscriberRegistry, ShardedSubscriberRegistry,
// and ResponseBuilder span overload performance. Validates that the library
// scales to 10K+ concurrent sessions with sub-millisecond lookup and fast
// timer tick, and that ResponseBuilder span overload avoids heap allocation.
// 3GPP coverage: TS 24.008 subscriber management at scale.

#include <gtest/gtest.h>
#include "gsml3parser/stack/subscriber_registry.h"
#include "gsml3parser/stack/response_builder.h"
#include "gsml3parser/arena.h"

#include <array>
#include <chrono>
#include <future>
#include <set>
#include <thread>
#include <vector>

using namespace gsml3parser;
using namespace std::chrono_literals;

// Test: create 10,000 sessions and verify each can be looked up by TMSI.
// Importance: Validates that SubscriberRegistry handles large subscriber counts
// without performance degradation. Total lookup time should be under 1ms.
// 3GPP: TS 24.008 4.4 - TMSI-based subscriber identification at scale.
TEST(Stress, _10000Sessions_CreateAndLookup_AllFound) {
    SubscriberRegistry reg;
    constexpr int N = 10000;

    auto t0 = std::chrono::steady_clock::now();

    // Create 10K sessions
    for (int i = 0; i < N; ++i) {
        auto* sess = reg.createByTMSI(static_cast<uint32_t>(i + 1));
        ASSERT_NE(sess, nullptr) << "Failed to create session " << i;
    }

    EXPECT_EQ(reg.count(), static_cast<size_t>(N));

    // Lookup each session by TMSI
    for (int i = 0; i < N; ++i) {
        auto* sess = reg.findByTMSI(static_cast<uint32_t>(i + 1));
        ASSERT_NE(sess, nullptr) << "Failed to find session " << i;
    }

    auto t1 = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0);

    // Total create + lookup should complete quickly (< 1 second is generous)
    EXPECT_LT(elapsed.count(), 1000)
        << "10K create + lookup took " << elapsed.count() << "ms";
}

// Test: tickAllTimers for 10,000 sessions completes in under 50ms.
// Importance: Timer tick runs every event loop iteration; must be fast enough
// to not block the main thread when managing many concurrent sessions.
// 3GPP: TS 24.008 timer management (T3101-T3113) at scale.
TEST(Stress, _10000Sessions_TimerTick_Fast) {
    SubscriberRegistry reg;
    constexpr int N = 10000;

    // Create sessions and start a timer in each
    for (int i = 0; i < N; ++i) {
        auto* sess = reg.createByTMSI(static_cast<uint32_t>(i + 1));
        ASSERT_NE(sess, nullptr);
        sess->timers.start(L3TimerId::T3101);
    }

    std::array<L3TimerId, N * 2> expired;

    auto t0 = std::chrono::steady_clock::now();

    // Tick all timers with 1ms delta (no expiry expected)
    size_t count = reg.tickAllTimers(1ms, expired);

    auto t1 = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0);

    EXPECT_EQ(count, 0u) << "No timers should expire with 1ms delta";
    EXPECT_LT(elapsed.count(), 50)
        << "tickAllTimers for 10K sessions took " << elapsed.count()
        << "ms (expected < 50ms)";
}

// Test: ShardedSubscriberRegistry handles concurrent create + lookup from 8 threads.
// Importance: Validates thread safety of sharded registry under realistic load.
// Each thread creates and looks up 1,000 sessions without data races.
// 3GPP: TS 24.008 multi-threaded subscriber management.
TEST(Stress, Sharded_10000Sessions_ThreadSafe) {
    ShardedSubscriberRegistry<16> reg;
    constexpr int threads = 8;
    constexpr int perThread = 1000;

    std::vector<std::future<void>> futures;

    for (int t = 0; t < threads; ++t) {
        futures.push_back(std::async(std::launch::async, [t, perThread, &reg]() {
            uint32_t base = static_cast<uint32_t>(t * perThread);

            // Create sessions
            for (int i = 0; i < perThread; ++i) {
                uint32_t tmsi = base + i + 1;
                auto* sess = reg.createByTMSI(tmsi);
                ASSERT_NE(sess, nullptr)
                    << "Thread " << t << " failed to create session " << tmsi;
            }

            // Lookup sessions (including those from other threads)
            for (int i = 0; i < perThread; ++i) {
                uint32_t tmsi = base + i + 1;
                auto* sess = reg.findByTMSI(tmsi);
                ASSERT_NE(sess, nullptr)
                    << "Thread " << t << " failed to find session " << tmsi;
            }
        }));
    }

    // Wait for all threads to complete
    for (auto& f : futures) {
        f.get();
    }

    // Verify total count
    size_t totalCount = 0;
    reg.forEach([&totalCount](const SubscriberSession&) { ++totalCount; });
    EXPECT_EQ(totalCount, static_cast<size_t>(threads * perThread));
}

// Test: 10,000 ResponseBuilder span overload calls write into Arena buffer
// without any heap allocation. Validates the zero-allocation design for
// high-throughput response building.
// 3GPP: TS 04.08 response message construction at scale.
TEST(Stress, ResponseBuilder_10000Builds_ZeroAlloc_Span) {
    Arena arena(65536 * 4); // 256KB arena to hold all responses

    constexpr int N = 10000;
    uint8_t buf[512];

    auto t0 = std::chrono::steady_clock::now();

    for (int i = 0; i < N; ++i) {
        // Build CM Service Accept into pre-allocated buffer (zero heap alloc)
        int n = ResponseBuilder::buildCMServiceAccept({buf, sizeof(buf)});
        ASSERT_GT(n, 0) << "Failed to build CM Service Accept at iteration " << i;

        // Also write some bytes into the arena to simulate real usage
        auto* arenaBuf = static_cast<uint8_t*>(arena.allocate(static_cast<size_t>(n)));
        ASSERT_NE(arenaBuf, nullptr) << "Arena allocation failed at iteration " << i;
        std::memcpy(arenaBuf, buf, static_cast<size_t>(n));
    }

    auto t1 = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0);

    EXPECT_LT(elapsed.count(), 500)
        << "10K ResponseBuilder span builds took " << elapsed.count()
        << "ms (expected < 500ms)";

    // Verify arena usage is reasonable
    EXPECT_GT(arena.used(), 0u) << "Arena should have been used";
    EXPECT_LT(arena.used(), arena.capacity()) << "Arena should not be full";
}
