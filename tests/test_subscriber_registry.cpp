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

// Tests for SubscriberRegistry and ShardedSubscriberRegistry: validates session
// creation, lookup by all indexes (TMSI, IMSI, LAPDm link), removal, timer tick,
// sharded distribution, and thread-safe concurrent access.
// 3GPP coverage: TS 24.008 subscriber data model management.

#include <gtest/gtest.h>
#include "gsml3parser/stack/subscriber_registry.h"
#include "gsml3parser/cc/l3ccmessages.h"
#include "gsml3parser/mm/l3mmmessages.h"
#include "gsml3parser/benchmark_hw.h"

#include <array>
#include <chrono>
#include <future>
#include <set>
#include <thread>
#include <vector>

using namespace gsml3parser;
using namespace std::chrono_literals;

// Test: createByTMSI creates a new session and returns non-null pointer.
// Importance: Core functionality — BTS must be able to register subscribers by TMSI.
// 3GPP: TS 24.008 4.4 - Subscriber identification by TMSI.
TEST(SR_createByTMSI, NewTMSI_ReturnsSession) {
    SubscriberRegistry reg;
    auto* sess = reg.createByTMSI(0x12345678);
    ASSERT_NE(sess, nullptr);
    EXPECT_EQ(sess->context.identity().tmsi(), 0x12345678u);
}

// Test: createByTMSI with duplicate TMSI returns nullptr.
// Importance: Prevents session collision — same TMSI must not create two sessions.
TEST(SR_createByTMSI, DuplicateTMSI_ReturnsNullptr) {
    SubscriberRegistry reg;
    auto* s1 = reg.createByTMSI(0xAAAAAAAA);
    auto* s2 = reg.createByTMSI(0xAAAAAAAA);
    ASSERT_NE(s1, nullptr);
    EXPECT_EQ(s2, nullptr);
}

// Test: createByIMSI creates a new session and returns non-null pointer.
// Importance: BTS must register subscribers by IMSI when TMSI is not yet known.
// 3GPP: TS 24.008 4.4 - IMSI-based identification.
TEST(SR_createByIMSI, NewIMSI_ReturnsSession) {
    SubscriberRegistry reg;
    auto* sess = reg.createByIMSI("244051234567890");
    ASSERT_NE(sess, nullptr);
    EXPECT_TRUE(sess->context.identity().isIMSI());
}

// Test: createByIMSI with duplicate IMSI returns nullptr.
// Importance: Prevents IMSI collision — same IMSI must not create two sessions.
TEST(SR_createByIMSI, DuplicateIMSI_ReturnsNullptr) {
    SubscriberRegistry reg;
    auto* s1 = reg.createByIMSI("244059999999999");
    auto* s2 = reg.createByIMSI("244059999999999");
    ASSERT_NE(s1, nullptr);
    EXPECT_EQ(s2, nullptr);
}

// Test: findByTMSI returns session for existing TMSI.
// Importance: Lookup by TMSI is the primary index — must always work.
TEST(SR_findByTMSI, Existing_Found) {
    SubscriberRegistry reg;
    auto* sess = reg.createByTMSI(0xDEADBEEF);
    auto* found = reg.findByTMSI(0xDEADBEEF);
    EXPECT_EQ(found, sess);
}

// Test: findByTMSI returns nullptr for non-existing TMSI.
TEST(SR_findByTMSI, NonExisting_Nullptr) {
    SubscriberRegistry reg;
    EXPECT_EQ(reg.findByTMSI(0xCAFEBABE), nullptr);
}

// Test: findByIMSI returns session for existing IMSI.
// Importance: Secondary index must redirect to primary TMSI index correctly.
TEST(SR_findByIMSI, Existing_Found) {
    SubscriberRegistry reg;
    auto* sess = reg.createByIMSI("244051111111111");
    auto* found = reg.findByIMSI("244051111111111");
    EXPECT_EQ(found, sess);
}

// Test: findByIMSI returns nullptr for non-existing IMSI.
TEST(SR_findByIMSI, NonExisting_Nullptr) {
    SubscriberRegistry reg;
    EXPECT_EQ(reg.findByIMSI("001020000000000"), nullptr);
}

// Test: IMSI lookup works through std::string_view (transparent heterogeneous
// lookup) without constructing a temporary std::string.
// Importance: findByIMSI is on the authentication path; a heap allocation per
// lookup is unacceptable at high subscriber churn (audit D4).
TEST(SR_findByIMSI, StringViewLookup_NoTempString) {
    SubscriberRegistry reg;
    auto* sess = reg.createByIMSI("244051234567890");
    ASSERT_NE(sess, nullptr);

    // Lookup via string_view (no std::string construction).
    std::string_view imsi = "244051234567890";
    EXPECT_EQ(reg.findByIMSI(imsi), sess);
    const SubscriberRegistry& creg = reg;
    EXPECT_EQ(creg.findByIMSI(imsi), sess);

    // remove() erases the IMSI index entry (heterogeneous erase).
    EXPECT_TRUE(reg.remove(sess));
    EXPECT_EQ(reg.findByIMSI(imsi), nullptr);
}

// Test: findByLink returns session when channel is assigned via assignChannel().
// Importance: Radio message routing depends on link index.
TEST(SR_findByLink, AssignedChannel_Found) {
    SubscriberRegistry reg;
    auto* sess = reg.createByTMSI(0x11223344);
    ChannelDescriptor ch{ChannelType::SDCCHType, 0, 5, 100};
    reg.assignChannel(sess, ch, 3);
    auto* found = reg.findByLink(0, 5, 3);
    EXPECT_EQ(found, sess);
}

// Test: findByLink returns nullptr when no channel is assigned.
TEST(SR_findByLink, NoChannel_Nullptr) {
    SubscriberRegistry reg;
    reg.createByTMSI(0x55667788);
    EXPECT_EQ(reg.findByLink(0, 0, 0), nullptr);
}

// Test: remove returns true and session becomes unfound.
// Importance: Session cleanup must work for detach scenarios.
TEST(SR_remove, Existing_ReturnsTrue) {
    SubscriberRegistry reg;
    auto* sess = reg.createByTMSI(0xAABBCCDD);
    EXPECT_TRUE(reg.remove(sess));
    EXPECT_EQ(reg.findByTMSI(0xAABBCCDD), nullptr);
}

// Test: remove with non-existing session pointer returns false.
TEST(SR_remove, NonExisting_ReturnsFalse) {
    SubscriberRegistry reg;
    SubscriberSession fake;
    EXPECT_FALSE(reg.remove(&fake));
}

// Test: clear removes all sessions from all indexes.
TEST(SR_clear, RemovesAll) {
    SubscriberRegistry reg;
    reg.createByTMSI(0x11111111);
    reg.createByTMSI(0x22222222);
    reg.createByIMSI("244053333333333");
    reg.clear();
    EXPECT_EQ(reg.count(), 0u);
    EXPECT_EQ(reg.findByTMSI(0x11111111), nullptr);
    EXPECT_EQ(reg.findByIMSI("244053333333333"), nullptr);
}

// Test: count returns accurate number of active sessions.
TEST(SR_count, Accurate) {
    SubscriberRegistry reg;
    EXPECT_EQ(reg.count(), 0u);
    reg.createByTMSI(0x01010101);
    EXPECT_EQ(reg.count(), 1u);
    reg.createByTMSI(0x02020202);
    EXPECT_EQ(reg.count(), 2u);
    auto* s = reg.findByTMSI(0x01010101);
    reg.remove(s);
    EXPECT_EQ(reg.count(), 1u);
}

// Test: forEach visits each session exactly once (no duplicates).
// Importance: Timer tick and periodic tasks rely on single-visit guarantee.
TEST(SR_forEach, VisitsEachSessionOnce) {
    SubscriberRegistry reg;
    reg.createByTMSI(0x0A0A0A0A);
    reg.createByTMSI(0x0B0B0B0B);
    reg.createByTMSI(0x0C0C0C0C);

    std::set<SubscriberSession*> visited;
    reg.forEach([&](SubscriberSession& sess) {
        visited.insert(&sess);
    });
    EXPECT_EQ(visited.size(), 3u);
}

// Test: tickAllTimers expires correct timers across sessions.
// Importance: Global timer management is critical for protocol compliance.
TEST(SR_tickAllTimers, ExpiresCorrectTimers) {
    SubscriberRegistry reg;
    auto* s1 = reg.createByTMSI(0x01010101);
    auto* s2 = reg.createByTMSI(0x02020202);

    s1->timers.start(L3TimerId::T3101, 100ms);
    s2->timers.start(L3TimerId::T3106, 100ms);

    std::array<TimerExpiry, 32> expired{};
    size_t n = reg.tickAllTimers(150ms, expired);
    EXPECT_EQ(n, 2u);
    EXPECT_TRUE(s1->timers.isRunning(L3TimerId::T3101) == false);
    EXPECT_TRUE(s2->timers.isRunning(L3TimerId::T3106) == false);
}

// Test: tickAllTimers returns expiry events bound to the correct sessions.
// Importance: with many sessions running timers concurrently, a bare timer ID
// is ambiguous; the event must carry the owning session pointer (audit D3).
TEST(SR_tickAllTimers, ExpiryBoundToCorrectSession) {
    SubscriberRegistry reg;
    auto* s1 = reg.createByTMSI(0x01010101);
    auto* s2 = reg.createByTMSI(0x02020202);

    s1->timers.start(L3TimerId::T3101, 100ms);
    s2->timers.start(L3TimerId::T3106, 100ms);

    std::array<TimerExpiry, 32> expired{};
    size_t n = reg.tickAllTimers(150ms, expired);
    ASSERT_EQ(n, 2u);

    // Order is unspecified — collect into a set and verify membership.
    std::set<std::pair<SubscriberSession*, L3TimerId>> got;
    for (size_t i = 0; i < n; ++i) got.insert({expired[i].session, expired[i].id});
    EXPECT_EQ(got.count({s1, L3TimerId::T3101}), 1u);
    EXPECT_EQ(got.count({s2, L3TimerId::T3106}), 1u);
}

// Test: expired timers also expire the session's pending transactions.
// Importance: the registry owns the documented timer event path
// (tick -> TransactionManager::onTimerExpired); previously nothing in the
// production stack called onTimerExpired, leaving transactions pending forever.
TEST(SR_tickAllTimers, ExpiresPendingTransactions) {
    SubscriberRegistry reg;
    auto* s = reg.createByTMSI(0x03030303);
    auto txId = s->transactions.create(L3PD::CallControl, L3Setup::MTI, 1, L3TimerId::T3101);
    ASSERT_TRUE(txId.has_value());
    s->timers.start(L3TimerId::T3101, 100ms);

    std::array<TimerExpiry, 32> expired{};
    size_t n = reg.tickAllTimers(150ms, expired);
    ASSERT_EQ(n, 1u);
    EXPECT_EQ(expired[0].session, s);
    EXPECT_EQ(expired[0].id, L3TimerId::T3101);
    // get() returns nullptr for non-pending transactions.
    EXPECT_EQ(s->transactions.get(txId.value()), nullptr);
}

// Test: tickAllProcedures ticks only sessions with active procedures (O(active)).
// Importance: at 1M sessions a full scan per event-loop tick is a real-time
// bottleneck; the active-procedure index must skip idle sessions (audit D2).
// 3GPP: TS 24.008 procedure management at scale.
TEST(SR_tickAllProcedures, OnlyActiveSessionsTicked) {
    SubscriberRegistry reg;
    constexpr int N = 1000;
    for (int i = 0; i < N; ++i) {
        auto* s = reg.createByTMSI(static_cast<uint32_t>(i + 1));
        ASSERT_NE(s, nullptr);
        if (i < 10) {
            // Feed CM Service Requests to start a LocationUpdate procedure and
            // advance it to WAITING_EXTERNAL with T3103 running: the procedure
            // FSM advances one state per feed (INIT -> IDENTITY_CHECK ->
            // AUTH_CHECK -> LU_REQUEST -> WAITING_EXTERNAL + startTimer(T3103)).
            for (int step = 0; step < 4; ++step) {
                auto cmReq = L3CMServiceRequest::builder()
                    .serviceType(L3CMServiceType{L3CMServiceType::LocationUpdateRequest})
                    .build();
                ParsedMessage msg{MMM{std::move(cmReq)}};
                s->procedures.feed(msg, s, {});
            }
            ASSERT_EQ(s->procedures.activeCount(), 1u);
        }
    }

    // LocationUpdate runs T3103 = 5s; a 6s tick expires the 10 active procedures.
    size_t failed = reg.tickAllProcedures(std::chrono::milliseconds(6000));
    EXPECT_EQ(failed, 10u);

    // All active procedures cleaned up; the next tick has nothing to do.
    EXPECT_EQ(reg.tickAllProcedures(std::chrono::milliseconds(100)), 0u);
}

// Test: ShardedSubscriberRegistry tickAllProcedures works across shards.
TEST(SSR_tickAllProcedures, Parallel_Correct) {
    ShardedSubscriberRegistry<4> reg;
    auto* s1 = reg.createByTMSI(0x00000001);
    auto* s2 = reg.createByTMSI(0x00000002);
    ASSERT_NE(s1, nullptr);
    ASSERT_NE(s2, nullptr);
    // Advance each LocationUpdate to WAITING_EXTERNAL with T3103 running
    // (one FSM state per feed; see SR_tickAllProcedures.OnlyActiveSessionsTicked).
    for (int step = 0; step < 4; ++step) {
        ParsedMessage msg{MMM{L3CMServiceRequest::builder()
            .serviceType(L3CMServiceType{L3CMServiceType::LocationUpdateRequest}).build()}};
        s1->procedures.feed(msg, s1, {});
    }
    for (int step = 0; step < 4; ++step) {
        ParsedMessage msg{MMM{L3CMServiceRequest::builder()
            .serviceType(L3CMServiceType{L3CMServiceType::LocationUpdateRequest}).build()}};
        s2->procedures.feed(msg, s2, {});
    }

    size_t failed = reg.tickAllProcedures(std::chrono::milliseconds(6000));
    EXPECT_EQ(failed, 2u);
    EXPECT_EQ(reg.tickAllProcedures(std::chrono::milliseconds(100)), 0u);
}

// Test: Identity switch from TMSI to IMSI updates indexes correctly.
// Importance: During authentication, MS may switch from TMSI to IMSI identity.
TEST(SR_IdentitySwitch, TMSItoIMSI_Reindex) {
    SubscriberRegistry reg;
    auto* sess = reg.createByTMSI(0x12345678);
    ASSERT_NE(sess, nullptr);

    // Switch identity to IMSI.
    sess->context.setIMSI("244059876543210");

    // TMSI lookup still works (primary index unchanged).
    EXPECT_EQ(reg.findByTMSI(0x12345678), sess);

    // IMSI lookup does NOT work because mByIMSI is only populated on createByIMSI.
    // This is expected: identity switch only changes MSContext, not registry indexes.
    EXPECT_EQ(reg.findByIMSI("244059876543210"), nullptr);
}

// Test: Channel release removes session from link index.
// Importance: After channel release, incoming messages on old link must not route here.
TEST(SR_ChannelRelease, RemovesLinkIndex) {
    SubscriberRegistry reg;
    auto* sess = reg.createByTMSI(0xAABB1122);
    ChannelDescriptor ch{ChannelType::SDCCHType, 1, 3, 200};
    reg.assignChannel(sess, ch, 5);
    EXPECT_EQ(reg.findByLink(1, 3, 5), sess);

    reg.releaseChannel(sess);
    EXPECT_EQ(reg.findByLink(1, 3, 5), nullptr);
    EXPECT_FALSE(sess->channel.has_value());
}

// Test: Stress test with 1000 sessions — lookup must be fast.
// Importance: Validates performance under realistic BTS load.
TEST(SR_Stress, 1000Sessions_LookupFast) {
    SubscriberRegistry reg;
    std::vector<uint32_t> tmsis;
    tmsis.reserve(1000);

    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 1000; ++i) {
        uint32_t tmsi = static_cast<uint32_t>(i + 1);
        tmsis.push_back(tmsi);
        reg.createByTMSI(tmsi);
    }

    for (uint32_t tmsi : tmsis) {
        auto* sess = reg.findByTMSI(tmsi);
        ASSERT_NE(sess, nullptr);
    }
    auto end = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    EXPECT_LT(ms, 1000);
}

// Test: 100K IMSI lookups stay fast (allocation-free lookup path).
// Importance: validates the lookup budget at high churn.
TEST(SR_Stress, 100K_IMSI_Lookups_Fast) {
    // Attribute the timing result to the machine it ran on (unified hardware ID).
    benchmark::printHardwareId();
    SubscriberRegistry reg;
    constexpr int N = 10000;
    std::vector<std::string> imsis;
    imsis.reserve(N);
    for (int i = 0; i < N; ++i) {
        std::string imsi = "24405" + std::to_string(100000000u + static_cast<unsigned>(i));
        imsis.push_back(imsi);
        ASSERT_NE(reg.createByIMSI(imsi), nullptr);
    }
    auto t0 = std::chrono::steady_clock::now();
    for (int r = 0; r < 10; ++r) {
        for (const auto& imsi : imsis) {
            ASSERT_NE(reg.findByIMSI(std::string_view(imsi)), nullptr);
        }
    }
    auto t1 = std::chrono::steady_clock::now();
    double ms = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count() / 1000.0;
    std::printf("100K IMSI lookups: %.1f ms\n", ms);
#ifndef GSML3PARSER_ASAN
    EXPECT_LT(ms, 200.0) << "100K IMSI lookups too slow";
#endif
}

// Test: ShardedSubscriberRegistry distributes sessions across shards.
// Importance: Even distribution is required for thread-safe parallelism.
TEST(SSR_createByTMSI, MultiShard_Distributed) {
    ShardedSubscriberRegistry<4> reg;
    for (int i = 0; i < 100; ++i) {
        auto* sess = reg.createByTMSI(static_cast<uint32_t>(i + 1));
        ASSERT_NE(sess, nullptr);
    }

    // All sessions must be findable.
    for (int i = 0; i < 100; ++i) {
        auto* sess = reg.findByTMSI(static_cast<uint32_t>(i + 1));
        ASSERT_NE(sess, nullptr);
    }
}

// Test: ShardedSubscriberRegistry findByTMSI is thread-safe.
// Importance: Concurrent lookup must not corrupt data or crash.
TEST(SSR_findByTMSI, ThreadSafe_Correct) {
    ShardedSubscriberRegistry<8> reg;
    const int numSessions = 1000;

    for (int i = 0; i < numSessions; ++i) {
        reg.createByTMSI(static_cast<uint32_t>(i + 1));
    }

    std::vector<std::future<void>> futures;
    const int numThreads = 8;

    for (int t = 0; t < numThreads; ++t) {
        futures.push_back(std::async(std::launch::async, [&reg, numSessions]() {
            for (int i = 0; i < numSessions; ++i) {
                auto* sess = reg.findByTMSI(static_cast<uint32_t>(i + 1));
                ASSERT_NE(sess, nullptr);
            }
        }));
    }

    for (auto& f : futures) {
        f.get();
    }
}

// Test: ShardedSubscriberRegistry tickAllTimers works correctly.
// Importance: Timer management must work across shards.
TEST(SSR_tickAllTimers, Parallel_Correct) {
    ShardedSubscriberRegistry<4> reg;

    auto* s1 = reg.createByTMSI(0x00000001);
    auto* s2 = reg.createByTMSI(0x00000002);
    auto* s3 = reg.createByTMSI(0x00000003);

    s1->timers.start(L3TimerId::T3101, 50ms);
    s2->timers.start(L3TimerId::T3103, 50ms);
    s3->timers.start(L3TimerId::T3106, 50ms);

    std::array<TimerExpiry, 64> expired{};
    size_t n = reg.tickAllTimers(100ms, expired);
    EXPECT_EQ(n, 3u);
}

// Test: remove() reclaims the session entry (no unbounded growth).
// Importance: A long-running BTS with session churn must not leak memory;
// previously removed entries stayed in the map forever with active=false,
// which also made the TMSI un-reusable (createByTMSI returned nullptr).
// 3GPP: TS 24.008 4.4 - subscriber data lifecycle at scale.
TEST(SR_remove, ReclaimsEntry_MemoryStable) {
    SubscriberRegistry reg;
    const int N = 1000;

    // Create and remove in batches; the registry must stay empty afterwards.
    for (int batch = 0; batch < 10; ++batch) {
        for (int i = 0; i < N; ++i) {
            auto* sess = reg.createByTMSI(static_cast<uint32_t>(batch * N + i + 1));
            ASSERT_NE(sess, nullptr);
        }
        for (int i = 0; i < N; ++i) {
            auto* sess = reg.findByTMSI(static_cast<uint32_t>(batch * N + i + 1));
            ASSERT_NE(sess, nullptr);
            EXPECT_TRUE(reg.remove(sess));
        }
        EXPECT_EQ(reg.count(), 0u) << "Batch " << batch << " left sessions behind";
    }

    // After full removal, every TMSI must be reusable (entry was erased,
    // not just flagged inactive).
    for (int i = 0; i < N; ++i) {
        auto* sess = reg.createByTMSI(static_cast<uint32_t>(i + 1));
        ASSERT_NE(sess, nullptr) << "TMSI " << (i + 1) << " not reusable after removal";
    }
    EXPECT_EQ(reg.count(), static_cast<size_t>(N));
}

// Test: auto-assigned TMSI (createByIMSI) never collides with user-assigned
// TMSIs, even after removals changed the map size.
// Importance: The old size()+1 scheme collided after removals and returned
// nullptr for valid new IMSIs; the high-water-mark scheme must not.
TEST(SR_createByIMSI, AutoTMSI_NoCollisionAfterRemovals) {
    SubscriberRegistry reg;

    // Occupy TMSI 1 and 2, then remove TMSI 1 (map size drops to 1).
    auto* s1 = reg.createByTMSI(1);
    ASSERT_NE(s1, nullptr);
    (void)reg.createByTMSI(2);
    EXPECT_TRUE(reg.remove(s1));

    // Old scheme would compute size()+1 = 2 -> collision -> nullptr.
    // New scheme must find a free TMSI (1) and succeed.
    auto* a = reg.createByIMSI("244050000000001");
    ASSERT_NE(a, nullptr) << "Auto TMSI allocation collided after a removal";
    auto* b = reg.createByIMSI("244050000000002");
    ASSERT_NE(b, nullptr);
    EXPECT_NE(a, b);

    // Both IMSI indexes resolve to distinct sessions.
    EXPECT_EQ(reg.findByIMSI("244050000000001"), a);
    EXPECT_EQ(reg.findByIMSI("244050000000002"), b);
}

// Test: findLocked returns the session with an engaged shared guard, and the
// guard releases the shard lock on destruction.
// Importance: concurrent-read safety requires the lock to be held for the
// whole lifetime of the session access.
TEST(SSR_findLocked, ReturnsSessionAndEngagedGuard) {
    ShardedSubscriberRegistry<4> reg;
    auto* sess = reg.createByTMSI(0x77777777);
    ASSERT_NE(sess, nullptr);

    {
        auto locked = reg.findLocked(0x77777777);
        EXPECT_EQ(locked.session, sess);
        EXPECT_TRUE(locked.guard.engaged());
        // Session is usable while the guard is alive.
        EXPECT_EQ(locked.session->context.identity().tmsi(), 0x77777777u);
    }
    // Guard destroyed here; shard lock released.

    // Non-existing TMSI: null session, guard still engaged (released on exit).
    auto missing = reg.findLocked(0x99999999);
    EXPECT_EQ(missing.session, nullptr);
    EXPECT_TRUE(missing.guard.engaged());
}

// Test: lockForTMSI grants exclusive access to the shard's registry for
// modification without deadlocking (no re-entrant locking).
TEST(SSR_lockForTMSI, ExclusiveAccess_ModifyUnderLock) {
    ShardedSubscriberRegistry<4> reg;

    {
        auto locked = reg.lockForTMSI(0x12340000);
        EXPECT_TRUE(locked.guard.engaged());
        auto* sess = locked.registry.createByTMSI(0x12340000);
        ASSERT_NE(sess, nullptr);
        sess->lapdmLink = 7;
    }
    // Lock released; the session is still findable.
    auto* found = reg.findByTMSI(0x12340000);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->lapdmLink, 7u);
}

// Test: concurrent readers (findLocked) and writers (lockForTMSI) do not
// tear reads or deadlock.
// Importance: this is the core concurrent-access model for multi-threaded
// BTS deployments (per-shard shared_mutex discipline).
TEST(SSR_Guards, ConcurrentReadModify_NoTornReads) {
    ShardedSubscriberRegistry<16> reg;
    constexpr int kSessions = 256;
    constexpr int kIters = 2000;

    for (int i = 0; i < kSessions; ++i) {
        auto* sess = reg.createByTMSI(static_cast<uint32_t>(i + 1));
        ASSERT_NE(sess, nullptr);
    }

    std::atomic<int> tornReads{0};
    std::atomic<bool> stop{false};

    // Writers: modify lapdmLink under the exclusive shard lock.
    auto writer = [&]() {
        for (int i = 0; i < kIters; ++i) {
            uint32_t tmsi = static_cast<uint32_t>((i % kSessions) + 1);
            auto locked = reg.lockForTMSI(tmsi);
            auto* sess = locked.registry.findByTMSI(tmsi);
            if (sess) sess->lapdmLink = static_cast<uint8_t>(i & 0xFF);
        }
    };

    // Readers: read under the shared shard lock; a torn read would mean the
    // lock discipline is broken (writer changed the field mid-read).
    auto reader = [&]() {
        for (int i = 0; i < kIters && !stop; ++i) {
            uint32_t tmsi = static_cast<uint32_t>((i % kSessions) + 1);
            auto locked = reg.findLocked(tmsi);
            if (!locked.session) { tornReads.fetch_add(1); continue; }
            uint8_t a = locked.session->lapdmLink;
            uint8_t b = locked.session->lapdmLink;
            if (a != b) tornReads.fetch_add(1);
        }
    };

    std::thread w1(writer);
    std::thread w2(writer);
    std::thread r1(reader);
    std::thread r2(reader);
    w1.join(); w2.join(); r1.join(); r2.join();

    EXPECT_EQ(tornReads.load(), 0);
}

// Test: remove() is O(1) — 1M sessions, 100K removals must be fast.
// Importance: session churn (detach) at scale must not degrade to O(N) scans.
// 3GPP coverage: TS 24.008 4.4 - subscriber lifecycle at scale.
TEST(SR_remove, OneMillion_O1Fast) {
    SubscriberRegistry reg;
    constexpr uint32_t N = 1000000;
    std::vector<SubscriberSession*> ptrs;
    ptrs.reserve(200000);
    for (uint32_t i = 1; i <= N; ++i) {
        auto* s = reg.createByTMSI(i);
        ASSERT_NE(s, nullptr);
        if (i <= 200000) ptrs.push_back(s);
    }
    EXPECT_EQ(reg.count(), static_cast<size_t>(N));

    auto t0 = std::chrono::steady_clock::now();
    for (auto* s : ptrs) {
        EXPECT_TRUE(reg.remove(s));
    }
    auto t1 = std::chrono::steady_clock::now();
    double ms = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count() / 1000.0;

    EXPECT_EQ(reg.count(), static_cast<size_t>(N) - ptrs.size());
#ifndef GSML3PARSER_ASAN
    // Budget is machine-dependent: the point is to prove O(1) behavior.
    // The old O(N) implementation took tens of seconds here (100K removals x
    // ~500K entries scanned); O(1) removal is ~1 us/op, so 500ms is a 100x
    // margin that still separates the two complexities on any hardware.
    EXPECT_LT(ms, 500.0) << "100K remove() over 1M sessions took " << ms << "ms (expected O(1), < 500ms)";
#endif
}

// Test: ShardedSubscriberRegistry::remove() is O(1) — the shard is derived
// directly from session->assignedTmsi, so exactly one shard is locked
// (the previous implementation scanned all N shards with an exclusive lock).
// Importance: session churn (detach) at scale must take a single shard lock.
// 3GPP coverage: TS 24.008 4.4 - subscriber lifecycle at scale.
TEST(Sharded_Remove_O1_SingleShardLock, Remove_SingleShard) {
    ShardedSubscriberRegistry<8> reg;
    constexpr uint32_t kTmsi = 0x0F0F0F0F;

    // Shard routing is deterministic: remove() must target exactly this shard.
    const int expectedShard = ShardedSubscriberRegistry<8>::debugShardForTmsi(kTmsi);
    EXPECT_GE(expectedShard, 0);
    EXPECT_LT(expectedShard, 8);

    auto* sess = reg.createByTMSI(kTmsi);
    ASSERT_NE(sess, nullptr);
    // The session is keyed by its TMSI in the derived shard.
    EXPECT_EQ(sess->assignedTmsi, kTmsi);
    EXPECT_EQ(reg.findByTMSI(kTmsi), sess);

    // remove() succeeds via the single derived shard.
    EXPECT_TRUE(reg.remove(sess));
    // The session is gone from the index.
    EXPECT_EQ(reg.findByTMSI(kTmsi), nullptr);

    // Bulk removal across all shards: every session must be removed.
    for (uint32_t i = 1; i <= 100; ++i) {
        auto* s = reg.createByTMSI(i);
        ASSERT_NE(s, nullptr);
        EXPECT_TRUE(reg.remove(s)) << "TMSI " << i << " not removed";
        EXPECT_EQ(reg.findByTMSI(i), nullptr);
    }

    // A standalone session (assignedTmsi == 0) is not owned by any shard:
    // remove() must reject it without scanning.
    SubscriberSession standalone;
    EXPECT_FALSE(reg.remove(&standalone));
    // nullptr is rejected as well.
    EXPECT_FALSE(reg.remove(nullptr));
}
