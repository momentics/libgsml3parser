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
#include <gsml3parser/stack/l3_timer.h>

#include <array>
#include <chrono>
#include <set>
#include <vector>

using namespace gsml3parser;
using namespace std::chrono_literals;

// l3TimerDefault returns correct durations per 3GPP TS 24.008 spec
TEST(L3TimerTest, DefaultValues_returnSpecDurations) {
    EXPECT_EQ(l3TimerDefault(L3TimerId::T3101), 3000ms);
    EXPECT_EQ(l3TimerDefault(L3TimerId::T3102), 3000ms);
    EXPECT_EQ(l3TimerDefault(L3TimerId::T3103), 5000ms);
    EXPECT_EQ(l3TimerDefault(L3TimerId::T3106), 3000ms);
    EXPECT_EQ(l3TimerDefault(L3TimerId::T3108), 3000ms);
    EXPECT_EQ(l3TimerDefault(L3TimerId::T3109), 30000ms);
    EXPECT_EQ(l3TimerDefault(L3TimerId::T3111), 3000ms);
    EXPECT_EQ(l3TimerDefault(L3TimerId::T3112), 3000ms);
    EXPECT_EQ(l3TimerDefault(L3TimerId::T3113), 3000ms);
    EXPECT_EQ(l3TimerDefault(L3TimerId::T3310), 5000ms);
    EXPECT_EQ(l3TimerDefault(L3TimerId::T3311), 30000ms);
    EXPECT_EQ(l3TimerDefault(L3TimerId::T3312), 3000ms);
    EXPECT_EQ(l3TimerDefault(L3TimerId::T3314), 3000ms);
    EXPECT_EQ(l3TimerDefault(L3TimerId::T3315), 3000ms);
    EXPECT_EQ(l3TimerDefault(L3TimerId::T3320), 3000ms);
    EXPECT_EQ(l3TimerDefault(L3TimerId::T3321), 3000ms);
    EXPECT_EQ(l3TimerDefault(L3TimerId::T3322), 3000ms);
    EXPECT_EQ(l3TimerDefault(L3TimerId::T3334), 3000ms);
    EXPECT_EQ(l3TimerDefault(L3TimerId::T3395), 3000ms);
    // Unknown timer returns sensible default
    EXPECT_GT(l3TimerDefault(L3TimerId::Unknown), 0ms);
}

// l3TimerName returns correct string for each timer ID
TEST(L3TimerTest, NameLookup_returnsCorrectNames) {
    EXPECT_EQ(l3TimerName(L3TimerId::T3101), "T3101");
    EXPECT_EQ(l3TimerName(L3TimerId::T3102), "T3102");
    EXPECT_EQ(l3TimerName(L3TimerId::T3103), "T3103");
    EXPECT_EQ(l3TimerName(L3TimerId::T3106), "T3106");
    EXPECT_EQ(l3TimerName(L3TimerId::T3108), "T3108");
    EXPECT_EQ(l3TimerName(L3TimerId::T3109), "T3109");
    EXPECT_EQ(l3TimerName(L3TimerId::T3111), "T3111");
    EXPECT_EQ(l3TimerName(L3TimerId::T3112), "T3112");
    EXPECT_EQ(l3TimerName(L3TimerId::T3113), "T3113");
    EXPECT_EQ(l3TimerName(L3TimerId::T3310), "T3310");
    EXPECT_EQ(l3TimerName(L3TimerId::T3311), "T3311");
    EXPECT_EQ(l3TimerName(L3TimerId::T3312), "T3312");
    EXPECT_EQ(l3TimerName(L3TimerId::T3314), "T3314");
    EXPECT_EQ(l3TimerName(L3TimerId::T3315), "T3315");
    EXPECT_EQ(l3TimerName(L3TimerId::T3320), "T3320");
    EXPECT_EQ(l3TimerName(L3TimerId::T3321), "T3321");
    EXPECT_EQ(l3TimerName(L3TimerId::T3322), "T3322");
    EXPECT_EQ(l3TimerName(L3TimerId::T3334), "T3334");
    EXPECT_EQ(l3TimerName(L3TimerId::T3395), "T3395");
    EXPECT_EQ(l3TimerName(L3TimerId::Unknown), "Unknown");
}

// Timer fires after its configured duration has elapsed via tick()
TEST(L3TimerTest, StartAndExpired_firesAfterDuration) {
    L3Timer timer(L3TimerId::T3101, 100ms);
    EXPECT_FALSE(timer.isRunning());

    timer.start();
    EXPECT_TRUE(timer.isRunning());

    // Tick 99ms - should not expire yet
    EXPECT_FALSE(timer.tick(99ms));
    EXPECT_TRUE(timer.isRunning());

    // Tick 2ms more - should expire (total 101ms > 100ms)
    EXPECT_TRUE(timer.tick(2ms));
    EXPECT_FALSE(timer.isRunning());
}

// Restarting a running timer resets its remaining time to full expiry
TEST(L3TimerTest, Restart_resetsExpiry) {
    L3Timer timer(L3TimerId::T3102, 1000ms);
    timer.start();

    // Advance 900ms
    EXPECT_FALSE(timer.tick(900ms));
    EXPECT_EQ(timer.remaining(), 100ms);

    // Restart resets to full expiry
    EXPECT_FALSE(timer.start()); // false = not first start
    EXPECT_EQ(timer.remaining(), 1000ms);
    EXPECT_TRUE(timer.isRunning());
}

// Stopping a timer prevents it from expiring on subsequent ticks
TEST(L3TimerTest, Stop_preventsExpiration) {
    L3Timer timer(L3TimerId::T3103, 5000ms);
    timer.start();
    timer.tick(4000ms);

    EXPECT_TRUE(timer.isRunning());
    timer.stop();
    EXPECT_FALSE(timer.isRunning());

    // Even though 5000ms hasn't elapsed, timer is stopped
    EXPECT_FALSE(timer.tick(2000ms));
    EXPECT_FALSE(timer.isRunning());
}

// Remaining time decreases correctly with each tick
TEST(L3TimerTest, Remaining_decreasesOnTick) {
    L3Timer timer(L3TimerId::T3106, 3000ms);
    timer.start();

    EXPECT_EQ(timer.remaining(), 3000ms);
    timer.tick(500ms);
    EXPECT_EQ(timer.remaining(), 2500ms);
    timer.tick(500ms);
    EXPECT_EQ(timer.remaining(), 2000ms);
    timer.tick(500ms);
    EXPECT_EQ(timer.remaining(), 1500ms);
}

// Non-running timer returns zero remaining time
TEST(L3TimerTest, NotRunning_returnsZeroRemaining) {
    L3Timer timer(L3TimerId::T3108);
    EXPECT_EQ(timer.remaining(), 0ms);

    timer.start();
    timer.tick(timer.expiry() + 1ms); // let it expire
    EXPECT_EQ(timer.remaining(), 0ms);
}

// TimerManager can start multiple timers and tracks all as running
TEST(TimerManagerTest, StartMultipleTimers_allRunning) {
    TimerManager tm;
    EXPECT_EQ(tm.runningCount(), 0u);

    tm.start(L3TimerId::T3101);
    EXPECT_EQ(tm.runningCount(), 1u);

    tm.start(L3TimerId::T3102);
    EXPECT_EQ(tm.runningCount(), 2u);

    tm.start(L3TimerId::T3103, 1000ms);
    EXPECT_EQ(tm.runningCount(), 3u);

    EXPECT_TRUE(tm.isRunning(L3TimerId::T3101));
    EXPECT_TRUE(tm.isRunning(L3TimerId::T3102));
    EXPECT_TRUE(tm.isRunning(L3TimerId::T3103));
}

// tick() with callback invokes onExpired for each timer that expires
TEST(TimerManagerTest, TickWithCallback_invokesForExpired) {
    TimerManager tm;
    tm.start(L3TimerId::T3101, 1000ms);
    tm.start(L3TimerId::T3102, 2000ms);
    tm.start(L3TimerId::T3103, 3000ms);

    std::set<L3TimerId> expired;
    tm.tick(1500ms, [&](L3TimerId id) {
        expired.insert(id);
    });

    // Only T3101 (1000ms) expires within 1500ms. T3102 (2000ms) and T3103 (3000ms) still running.
    EXPECT_EQ(expired.size(), 1u);
    EXPECT_TRUE(expired.count(L3TimerId::T3101));
    EXPECT_FALSE(expired.count(L3TimerId::T3102));
    EXPECT_FALSE(expired.count(L3TimerId::T3103));

    EXPECT_EQ(tm.runningCount(), 2u); // T3102 and T3103 still running
}

// tick() with span fills the output buffer with expired timer IDs
TEST(TimerManagerTest, TickWithSpan_fillsBuffer) {
    TimerManager tm;
    tm.start(L3TimerId::T3101, 500ms);
    tm.start(L3TimerId::T3102, 1000ms);
    tm.start(L3TimerId::T3106, 1500ms);

    std::array<L3TimerId, 32> expired;
    size_t count = tm.tick(1200ms, std::span<L3TimerId>(expired));

    EXPECT_EQ(count, 2u);
    EXPECT_EQ(expired[0], L3TimerId::T3101);
    EXPECT_EQ(expired[1], L3TimerId::T3102);
}

// tick() with an undersized span buffer returns the number of IDs actually written
TEST(TimerManagerTest, TickSpan_BufferFull_ReturnsWritten) {
    TimerManager tm;
    tm.start(L3TimerId::T3101, 100ms);
    tm.start(L3TimerId::T3102, 100ms);

    // Buffer holds only 1 entry, but 2 timers expire on this tick.
    std::array<L3TimerId, 1> expired;
    size_t count = tm.tick(200ms, std::span<L3TimerId>(expired));

    // Contract: return the number of IDs written (1), not the number expired (2).
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(expired[0], L3TimerId::T3101);

    // Both timers still expired: their running state is cleared regardless
    // of whether their ID fit into the output buffer.
    EXPECT_FALSE(tm.isRunning(L3TimerId::T3101));
    EXPECT_FALSE(tm.isRunning(L3TimerId::T3102));
    EXPECT_EQ(tm.runningCount(), 0u);
}

// start() reports a fresh start (true) when restarting an expired or stopped timer
TEST(TimerManagerTest, StartAfterExpiry_reportsFreshStart) {
    TimerManager tm;
    EXPECT_TRUE(tm.start(L3TimerId::T3101, 100ms)); // fresh start: was not running

    tm.tick(200ms, [](L3TimerId) {}); // let it expire
    EXPECT_FALSE(tm.isRunning(L3TimerId::T3101));

    // The expired timer is not running, so this is a fresh start again.
    EXPECT_TRUE(tm.start(L3TimerId::T3101, 100ms));
    EXPECT_TRUE(tm.isRunning(L3TimerId::T3101));
}

// stopAll() stops every running timer in the manager
TEST(TimerManagerTest, StopAll_clearsAllTimers) {
    TimerManager tm;
    tm.start(L3TimerId::T3101);
    tm.start(L3TimerId::T3102);
    tm.start(L3TimerId::T3103);
    EXPECT_EQ(tm.runningCount(), 3u);

    tm.stopAll();
    EXPECT_EQ(tm.runningCount(), 0u);
    EXPECT_FALSE(tm.isRunning(L3TimerId::T3101));
    EXPECT_FALSE(tm.isRunning(L3TimerId::T3102));
    EXPECT_FALSE(tm.isRunning(L3TimerId::T3103));
}

// stop() for a single timer only affects that timer, others keep running
TEST(TimerManagerTest, StopSingle_onlyAffectsOne) {
    TimerManager tm;
    tm.start(L3TimerId::T3101);
    tm.start(L3TimerId::T3102);
    tm.start(L3TimerId::T3103);

    tm.stop(L3TimerId::T3102);
    EXPECT_TRUE(tm.isRunning(L3TimerId::T3101));
    EXPECT_FALSE(tm.isRunning(L3TimerId::T3102));
    EXPECT_TRUE(tm.isRunning(L3TimerId::T3103));
    EXPECT_EQ(tm.runningCount(), 2u);
}

// isRunning() returns correct state for started, stopped, and expired timers
TEST(TimerManagerTest, IsRunning_correctState) {
    TimerManager tm;
    EXPECT_FALSE(tm.isRunning(L3TimerId::T3101)); // not started yet

    tm.start(L3TimerId::T3101, 100ms);
    EXPECT_TRUE(tm.isRunning(L3TimerId::T3101));

    tm.tick(200ms, [](L3TimerId) {}); // let it expire
    EXPECT_FALSE(tm.isRunning(L3TimerId::T3101));
}

// remaining() returns correct remaining time for a running timer
TEST(TimerManagerTest, Remaining_correctDuration) {
    TimerManager tm;
    tm.start(L3TimerId::T3101, 3000ms);

    EXPECT_EQ(tm.remaining(L3TimerId::T3101), 3000ms);

    tm.tick(1000ms, [](L3TimerId) {});
    EXPECT_EQ(tm.remaining(L3TimerId::T3101), 2000ms);

    tm.tick(1500ms, [](L3TimerId) {});
    // After 2500ms total, T3101 (3000ms expiry) has 500ms remaining
    EXPECT_EQ(tm.remaining(L3TimerId::T3101), 500ms);

    tm.tick(600ms, [](L3TimerId) {});
    // After 3100ms total, T3101 has expired
    EXPECT_EQ(tm.remaining(L3TimerId::T3101), 0ms);
}

// runningCount() accurately reflects the number of active timers
TEST(TimerManagerTest, RunningCount_accurate) {
    TimerManager tm;
    EXPECT_EQ(tm.runningCount(), 0u);

    tm.start(L3TimerId::T3101);
    tm.start(L3TimerId::T3102);
    tm.start(L3TimerId::T3106);
    EXPECT_EQ(tm.runningCount(), 3u);

    tm.stop(L3TimerId::T3102);
    EXPECT_EQ(tm.runningCount(), 2u);

    tm.start(L3TimerId::T3103);
    EXPECT_EQ(tm.runningCount(), 3u);
}

// Starting an already-running timer restarts it (returns false for firstStart)
TEST(TimerManagerTest, StartRestart_sameTimerRestarted) {
    TimerManager tm;
    bool first = tm.start(L3TimerId::T3101, 1000ms);
    EXPECT_TRUE(first);

    tm.tick(500ms, [](L3TimerId) {});
    EXPECT_EQ(tm.remaining(L3TimerId::T3101), 500ms);

    bool restarted = tm.start(L3TimerId::T3101, 2000ms);
    EXPECT_FALSE(restarted); // not a first start
    EXPECT_EQ(tm.remaining(L3TimerId::T3101), 2000ms); // reset with new expiry
}

// tick() with callback and span performs zero heap allocations
TEST(TimerManagerTest, NoHeapAllocationsDuringTick) {
    TimerManager tm;

    // Start all known timers
    tm.start(L3TimerId::T3101, 100ms);
    tm.start(L3TimerId::T3102, 200ms);
    tm.start(L3TimerId::T3103, 300ms);
    tm.start(L3TimerId::T3106, 400ms);
    tm.start(L3TimerId::T3108, 500ms);

    // Tick with callback - no allocations
    std::vector<L3TimerId> callbackExpired;
    tm.tick(150ms, [&](L3TimerId id) {
        callbackExpired.push_back(id);
    });

    // T3101 (100ms) should have expired
    EXPECT_EQ(callbackExpired.size(), 1u);
    EXPECT_EQ(callbackExpired[0], L3TimerId::T3101);

    // Tick with span - no allocations
    std::array<L3TimerId, 32> spanExpired;
    size_t count = tm.tick(300ms, std::span<L3TimerId>(spanExpired));
    // After 1800ms total: T3103 (300ms), T3106 (400ms), T3108 (500ms) all expire
    EXPECT_EQ(count, 3u);

    // Verify the tick operations completed without errors
    EXPECT_EQ(tm.runningCount(), 1u); // T3108 (500ms) still has 50ms remaining
}

// TimerManager::get() returns pointer to configured timer
TEST(TimerManagerTest, Get_returnsTimerPointer) {
    TimerManager tm;
    EXPECT_EQ(tm.get(L3TimerId::T3101), nullptr);

    tm.start(L3TimerId::T3101, 5000ms);
    const L3Timer* timer = tm.get(L3TimerId::T3101);
    EXPECT_NE(timer, nullptr);
    EXPECT_EQ(timer->id(), L3TimerId::T3101);
    EXPECT_EQ(timer->expiry(), 5000ms);
    EXPECT_TRUE(timer->isRunning());
}

// Timer with custom expiry uses the provided duration, not default
TEST(L3TimerTest, CustomExpiry_usesProvidedDuration) {
    L3Timer timer(L3TimerId::T3101, 750ms); // default is 3000ms
    EXPECT_EQ(timer.expiry(), 750ms);

    timer.start();
    EXPECT_FALSE(timer.tick(700ms));
    EXPECT_TRUE(timer.tick(100ms));
}

// Timer ID is preserved after construction
TEST(L3TimerTest, IdPreserved_afterConstruction) {
    L3Timer timer(L3TimerId::T3395);
    EXPECT_EQ(timer.id(), L3TimerId::T3395);
}

// Test: a timer started by the expiry callback is NOT ticked in the same
// pass (audit N4).
TEST(TimerManagerTest, Tick_CallbackStartedTimer_NotTickedSamePass) {
    TimerManager tm;
    tm.start(L3TimerId::T3101, std::chrono::milliseconds(100));
    std::vector<L3TimerId> fired;
    tm.tick(std::chrono::milliseconds(150), [&](L3TimerId id) {
        fired.push_back(id);
        // Start a new timer with a short expiry DURING the callback.
        tm.start(L3TimerId::T3102, std::chrono::milliseconds(1));
    });
    // Only T3101 fired; T3102 (started in the callback) did not.
    ASSERT_EQ(fired.size(), 1u);
    EXPECT_EQ(fired[0], L3TimerId::T3101);
    EXPECT_TRUE(tm.isRunning(L3TimerId::T3102));
    // Next pass: T3102 expires.
    std::vector<L3TimerId> fired2;
    tm.tick(std::chrono::milliseconds(5), [&](L3TimerId id) { fired2.push_back(id); });
    ASSERT_EQ(fired2.size(), 1u);
    EXPECT_EQ(fired2[0], L3TimerId::T3102);
}
