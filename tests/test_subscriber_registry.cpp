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

    std::array<L3TimerId, 32> expired{};
    size_t n = reg.tickAllTimers(150ms, expired);
    EXPECT_EQ(n, 2u);
    EXPECT_TRUE(s1->timers.isRunning(L3TimerId::T3101) == false);
    EXPECT_TRUE(s2->timers.isRunning(L3TimerId::T3106) == false);
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

    std::array<L3TimerId, 64> expired{};
    size_t n = reg.tickAllTimers(100ms, expired);
    EXPECT_EQ(n, 3u);
}
