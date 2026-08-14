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
#include <gsml3parser/stack/transaction.h>
#include <gsml3parser/cc/l3ccmessages.h>
#include <gsml3parser/rr/l3rrmessages.h>
#include <gsml3parser/mm/l3mmmessages.h>
#include <gsml3parser/visitor.h>

#include <chrono>

using namespace gsml3parser;
using namespace std::chrono_literals;

// Helper: build a CC Setup message as ParsedMessage
static ParsedMessage makeCCSetup() {
    return ParsedMessage{CCM{L3Setup::builder().build()}};
}

// Helper: build a CC Connect message as ParsedMessage
static ParsedMessage makeCCConnect() {
    return ParsedMessage{CCM{L3Connect::builder().build()}};
}

// Helper: build an RR PagingResponse message as ParsedMessage
static ParsedMessage makeRRPagingResponse() {
    return ParsedMessage{RRM{L3PagingResponse::builder().build()}};
}

// Helper: build an MM CMServiceRequest message as ParsedMessage
static ParsedMessage makeMMCMServiceRequest() {
    return ParsedMessage{MMM{L3CMServiceRequest::builder().build()}};
}

// Helper: build a minimal L3Header
static L3Header makeHeader(L3PD pd, int mti, unsigned ti, bool tif = true) {
    return L3Header{pd, mti, ti, tif};
}

// Transaction constructor initializes all fields correctly
TEST(TransactionTest, Constructor_setsFields) {
    Transaction tx(L3PD::CallControl, L3Setup::MTI, 3, L3TimerId::T3101);

    EXPECT_EQ(tx.requestPD(), L3PD::CallControl);
    EXPECT_EQ(tx.requestMTI(), L3Setup::MTI);
    EXPECT_EQ(tx.ti(), 3);
    EXPECT_EQ(tx.timerId(), L3TimerId::T3101);
    EXPECT_EQ(tx.state(), TransactionState::Pending);
    EXPECT_FALSE((tx.createdAt() == std::chrono::steady_clock::time_point{}));
}

// complete() transitions to Completed state only
TEST(TransactionTest, Complete_setsState_only) {
    Transaction tx(L3PD::CallControl, L3Setup::MTI, 1, L3TimerId::T3101);
    EXPECT_EQ(tx.state(), TransactionState::Pending);
    tx.complete();
    EXPECT_EQ(tx.state(), TransactionState::Completed);
}

// expire() transitions to Expired state
TEST(TransactionTest, Expire_setsState) {
    Transaction tx(L3PD::CallControl, L3Setup::MTI, 1, L3TimerId::T3101);
    tx.expire();
    EXPECT_EQ(tx.state(), TransactionState::Expired);
}

// cancel() transitions to Cancelled state
TEST(TransactionTest, Cancel_setsState) {
    Transaction tx(L3PD::CallControl, L3Setup::MTI, 1, L3TimerId::T3101);
    tx.cancel();
    EXPECT_EQ(tx.state(), TransactionState::Cancelled);
}

// CC transaction matches by TI when PD is CallControl
TEST(TransactionTest, Matches_byTI_forCC) {
    Transaction tx(L3PD::CallControl, L3Setup::MTI, 2, L3TimerId::T3101);

    // Same TI — should match regardless of MTI difference
    ParsedMessage connectMsg = makeCCConnect();
    EXPECT_TRUE(tx.matches(connectMsg, 2));

    // Different TI — should not match
    EXPECT_FALSE(tx.matches(connectMsg, 5));
}

// Non-CC transaction matches by PD + MTI
TEST(TransactionTest, Matches_byPD_MTI_forRR) {
    Transaction tx(L3PD::RadioResource, L3PagingResponse::MTI, 0, L3TimerId::T3113);

    ParsedMessage msg = makeRRPagingResponse();
    EXPECT_TRUE(tx.matches(msg));

    // Different PD — should not match
    ParsedMessage ccMsg = makeCCSetup();
    EXPECT_FALSE(tx.matches(ccMsg));
}

// Non-CC transaction does not match when MTI differs
TEST(TransactionTest, NoMatch_wrongMTI) {
    Transaction tx(L3PD::MobilityManagement, L3CMServiceRequest::MTI, 0, L3TimerId::T3101);

    // Different PD entirely — no match
    ParsedMessage rrMsg = makeRRPagingResponse();
    EXPECT_FALSE(tx.matches(rrMsg));
}

// Non-CC transaction does not match when PD differs
TEST(TransactionTest, NoMatch_wrongPD) {
    Transaction tx(L3PD::CallControl, L3Setup::MTI, 1, L3TimerId::T3101);

    ParsedMessage mmMsg = makeMMCMServiceRequest();
    EXPECT_FALSE(tx.matches(mmMsg));
}

// Finished transactions do not match incoming messages
TEST(TransactionTest, NoMatch_whenNotPending) {
    Transaction tx(L3PD::CallControl, L3Setup::MTI, 1, L3TimerId::T3101);
    ParsedMessage msg = makeCCSetup();

    tx.complete();
    EXPECT_FALSE(tx.matches(msg, 1));
    EXPECT_FALSE(tx.matches(msg));

    Transaction tx2(L3PD::RadioResource, L3PagingResponse::MTI, 0, L3TimerId::T3113);
    tx2.expire();
    EXPECT_FALSE(tx2.matches(makeRRPagingResponse()));
}

// createdAt() returns a valid time point
TEST(TransactionTest, CreatedAt_returnsTime) {
    auto before = std::chrono::steady_clock::now();
    Transaction tx(L3PD::CallControl, L3Setup::MTI, 1, L3TimerId::T3101);
    auto after = std::chrono::steady_clock::now();

    auto created = tx.createdAt();
    EXPECT_FALSE((created < before));
    EXPECT_FALSE((created > after));
}

// Transaction struct is compact for cache efficiency
TEST(TransactionTest, SizeIsCompact) {
    EXPECT_LE(sizeof(Transaction), 48u);
}

// create() returns unique transaction IDs
TEST(TransactionManagerTest, Create_returnsUniqueID) {
    TransactionManager tm;
    auto id1 = tm.create(L3PD::CallControl, L3Setup::MTI, 1, L3TimerId::T3101);
    auto id2 = tm.create(L3PD::CallControl, L3Connect::MTI, 2, L3TimerId::T3101);

    ASSERT_TRUE(id1.has_value());
    ASSERT_TRUE(id2.has_value());
    EXPECT_NE(*id1, *id2);
}

// match() with header finds CC transaction by TI (O(1) lookup)
TEST(TransactionManagerTest, Match_findsByTI) {
    TransactionManager tm;
    tm.create(L3PD::CallControl, L3Setup::MTI, 3, L3TimerId::T3101);

    ParsedMessage msg = makeCCConnect(); // Different MTI, same PD
    L3Header header = makeHeader(L3PD::CallControl, L3Connect::MTI, 3);

    Transaction* result = tm.match(header, msg);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->ti(), 3);
    EXPECT_EQ(result->state(), TransactionState::Pending);
}

// match() without header finds non-CC transaction by PD+MTI
TEST(TransactionManagerTest, Match_findsByPD_MTI) {
    TransactionManager tm;
    tm.create(L3PD::RadioResource, L3PagingResponse::MTI, 0, L3TimerId::T3113);

    ParsedMessage msg = makeRRPagingResponse();
    Transaction* result = tm.match(msg);

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->requestPD(), L3PD::RadioResource);
    EXPECT_EQ(result->requestMTI(), L3PagingResponse::MTI);
}

// match() returns nullptr when no transaction matches
TEST(TransactionManagerTest, NoMatch_returnsNullopt) {
    TransactionManager tm;
    tm.create(L3PD::CallControl, L3Setup::MTI, 1, L3TimerId::T3101);

    // Different TI — no match for CC
    ParsedMessage msg = makeCCSetup();
    L3Header header = makeHeader(L3PD::CallControl, L3Setup::MTI, 5);
    EXPECT_EQ(tm.match(header, msg), nullptr);

    // Different PD — no match for non-CC path
    EXPECT_EQ(tm.match(makeRRPagingResponse()), nullptr);
}

// onTimerExpired() marks matching transactions as Expired
TEST(TransactionManagerTest, OnTimerExpired_marksExpired) {
    TransactionManager tm;
    auto id1 = tm.create(L3PD::CallControl, L3Setup::MTI, 1, L3TimerId::T3101);
    auto id2 = tm.create(L3PD::CallControl, L3Connect::MTI, 2, L3TimerId::T3106);

    tm.onTimerExpired(L3TimerId::T3101);

    // T3101 transaction should be expired (no longer pending, so get returns nullptr)
    EXPECT_EQ(tm.get(*id1), nullptr);
    // T3106 transaction should still be pending
    Transaction* tx2 = tm.get(*id2);
    ASSERT_NE(tx2, nullptr);
    EXPECT_EQ(tx2->state(), TransactionState::Pending);
}

// cleanup() removes finished transactions and rebuilds TI index
TEST(TransactionManagerTest, Cleanup_removesFinished) {
    TransactionManager tm;
    auto id1 = tm.create(L3PD::CallControl, L3Setup::MTI, 1, L3TimerId::T3101);
    auto id2 = tm.create(L3PD::CallControl, L3Connect::MTI, 2, L3TimerId::T3101);

    // Complete one transaction
    if (Transaction* tx = tm.get(*id1)) tx->complete();

    EXPECT_EQ(tm.totalCount(), 2u);
    EXPECT_EQ(tm.pendingCount(), 1u);

    size_t removed = tm.cleanup();
    EXPECT_EQ(removed, 1u);
    EXPECT_EQ(tm.totalCount(), 1u);
    EXPECT_EQ(tm.pendingCount(), 1u);

    // The completed transaction should no longer be accessible
    EXPECT_EQ(tm.get(*id1), nullptr);
}

// pendingCount() accurately reflects active transactions
TEST(TransactionManagerTest, PendingCount_accurate) {
    TransactionManager tm;
    EXPECT_EQ(tm.pendingCount(), 0u);

    tm.create(L3PD::CallControl, L3Setup::MTI, 1, L3TimerId::T3101);
    EXPECT_EQ(tm.pendingCount(), 1u);

    tm.create(L3PD::NonCallSS, 1, 2, L3TimerId::T3101);
    EXPECT_EQ(tm.pendingCount(), 2u);

    tm.create(L3PD::RadioResource, L3PagingResponse::MTI, 0, L3TimerId::T3113);
    EXPECT_EQ(tm.pendingCount(), 3u);
}

// totalCount() includes all transactions regardless of state
TEST(TransactionManagerTest, TotalCount_includesAll) {
    TransactionManager tm;
    auto id = tm.create(L3PD::CallControl, L3Setup::MTI, 1, L3TimerId::T3101);

    EXPECT_EQ(tm.totalCount(), 1u);

    if (Transaction* tx = tm.get(*id)) tx->complete();
    EXPECT_EQ(tm.totalCount(), 1u); // Still counts finished transactions

    tm.cleanup();
    EXPECT_EQ(tm.totalCount(), 0u); // Removed after cleanup
}

// After cleanup, TI index is rebuilt and match still works
TEST(TransactionManagerTest, TIIndex_rebuiltAfterCleanup) {
    TransactionManager tm;

    // Create and complete a transaction with TI=3
    auto id1 = tm.create(L3PD::CallControl, L3Setup::MTI, 3, L3TimerId::T3101);
    if (Transaction* tx = tm.get(*id1)) tx->complete();

    // Create another transaction with TI=3
    auto id2 = tm.create(L3PD::CallControl, L3Connect::MTI, 3, L3TimerId::T3101);

    // Cleanup removes the completed one
    tm.cleanup();

    // The new transaction with TI=3 should still be findable
    ParsedMessage msg = makeCCSetup();
    L3Header header = makeHeader(L3PD::CallControl, L3Setup::MTI, 3);
    Transaction* result = tm.match(header, msg);

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->ti(), 3);
    EXPECT_EQ(result->requestMTI(), L3Connect::MTI);
}

// get() returns transaction by ID
TEST(TransactionManagerTest, Get_returnsTransaction) {
    TransactionManager tm;
    auto id = tm.create(L3PD::CallControl, L3Setup::MTI, 5, L3TimerId::T3101);

    ASSERT_TRUE(id.has_value());
    Transaction* tx = tm.get(*id);
    ASSERT_NE(tx, nullptr);
    EXPECT_EQ(tx->ti(), 5);
    EXPECT_EQ(tx->requestPD(), L3PD::CallControl);
}

// get() returns nullptr for invalid or non-pending IDs
TEST(TransactionManagerTest, Get_returnsNulloptForFinished) {
    TransactionManager tm;
    auto id = tm.create(L3PD::CallControl, L3Setup::MTI, 1, L3TimerId::T3101);

    if (Transaction* tx = tm.get(*id)) tx->cancel();

    // Cancelled transaction is no longer pending — get returns nullptr
    EXPECT_EQ(tm.get(*id), nullptr);
}

// TransactionManager handles NonCallSS protocol with TI-based matching
TEST(TransactionManagerTest, Match_NonCallSS_byTI) {
    TransactionManager tm;
    tm.create(L3PD::NonCallSS, 1, 4, L3TimerId::T3101);

    ParsedMessage msg = makeCCSetup(); // Doesn't matter what message body is
    L3Header header = makeHeader(L3PD::NonCallSS, 1, 4);

    Transaction* result = tm.match(header, msg);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->ti(), 4);
}

// Multiple transactions with different TIs are matched independently
TEST(TransactionManagerTest, MultipleTI_independent) {
    TransactionManager tm;
    tm.create(L3PD::CallControl, L3Setup::MTI, 1, L3TimerId::T3101);
    tm.create(L3PD::CallControl, L3Setup::MTI, 2, L3TimerId::T3101);
    tm.create(L3PD::CallControl, L3Setup::MTI, 3, L3TimerId::T3101);

    ParsedMessage msg = makeCCConnect();

    // Match TI=2 only
    L3Header header = makeHeader(L3PD::CallControl, L3Connect::MTI, 2);
    Transaction* result = tm.match(header, msg);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->ti(), 2);

    // TI=1 and TI=3 should still be pending
    EXPECT_EQ(tm.pendingCount(), 3u);
}

// match() without header for CC falls back to scanning by PD
TEST(TransactionManagerTest, Match_CC_withoutHeader_scansByPD) {
    TransactionManager tm;
    tm.create(L3PD::CallControl, L3Setup::MTI, 1, L3TimerId::T3101);

    ParsedMessage msg = makeCCConnect();
    Transaction* result = tm.match(msg);

    // Should find the CC transaction by PD scan
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->requestPD(), L3PD::CallControl);
}

// onTimerExpired() only affects transactions with matching timer ID
TEST(TransactionManagerTest, OnTimerExpired_onlyMatchingTimer) {
    TransactionManager tm;
    tm.create(L3PD::CallControl, L3Setup::MTI, 1, L3TimerId::T3101);
    tm.create(L3PD::CallControl, L3Connect::MTI, 2, L3TimerId::T3106);
    tm.create(L3PD::RadioResource, L3PagingResponse::MTI, 0, L3TimerId::T3113);

    tm.onTimerExpired(L3TimerId::T3106);

    // Only the T3106 transaction should be expired
    EXPECT_EQ(tm.pendingCount(), 2u);
}

// Cleanup removes expired, cancelled, and completed transactions
TEST(TransactionManagerTest, Cleanup_removesAllFinishedStates) {
    TransactionManager tm;
    auto id1 = tm.create(L3PD::CallControl, L3Setup::MTI, 1, L3TimerId::T3101);
    auto id2 = tm.create(L3PD::CallControl, L3Connect::MTI, 2, L3TimerId::T3101);
    auto id3 = tm.create(L3PD::RadioResource, L3PagingResponse::MTI, 0, L3TimerId::T3113);

    if (Transaction* tx = tm.get(*id1)) tx->complete();
    if (Transaction* tx = tm.get(*id2)) tx->expire();
    if (Transaction* tx = tm.get(*id3)) tx->cancel();

    size_t removed = tm.cleanup();
    EXPECT_EQ(removed, 3u);
    EXPECT_EQ(tm.totalCount(), 0u);
    EXPECT_EQ(tm.pendingCount(), 0u);
}
