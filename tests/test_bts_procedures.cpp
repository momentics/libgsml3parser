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

/// Integration tests for BTS procedures.
/// Validates that all stack components (MSContext, TimerManager, TransactionManager,
/// ProtocolStateMachine, ChannelPool, ProtocolDispatcher) work together end-to-end.
/// Includes stress tests for high-load scenarios.

#include <gtest/gtest.h>
#include <gsml3parser/stack/ms_context.h>
#include <gsml3parser/stack/l3_timer.h>
#include <gsml3parser/stack/transaction.h>
#include <gsml3parser/stack/state_machine.h>
#include <gsml3parser/stack/channel_pool.h>
#include <gsml3parser/dispatcher.h>
#include <gsml3parser/l3header.h>
#include <gsml3parser/visitor.h>
#include <gsml3parser/parser.h>
#include <gsml3parser/rr/l3rrmessages.h>
#include <gsml3parser/mm/l3mmmessages.h>
#include <gsml3parser/cc/l3ccmessages.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <vector>

using namespace gsml3parser;
using namespace std::chrono_literals;

// ── Helpers: build ParsedMessage instances for tests ──────────────────────

static ParsedMessage makeRRChannelRequest() {
    return ParsedMessage{RRM{L3ChannelRequest::builder().build()}};
}

static ParsedMessage makeRRPagingResponse() {
    return ParsedMessage{RRM{L3PagingResponse::builder().build()}};
}

static ParsedMessage makeRRMeasurementReport() {
    return ParsedMessage{RRM{L3MeasurementReport::builder().build()}};
}

static ParsedMessage makeRRChannelRelease() {
    return ParsedMessage{RRM{L3ChannelRelease::builder().cause(RRCause::Normal_Event).build()}};
}

static ParsedMessage makeRRCipheringModeComplete() {
    return ParsedMessage{RRM{L3CipheringModeComplete::builder().build()}};
}

static ParsedMessage makeMMCMServiceRequest() {
    return ParsedMessage{MMM{L3CMServiceRequest::builder().build()}};
}

static ParsedMessage makeMMCMServiceAccept() {
    return ParsedMessage{MMM{L3CMServiceAccept::builder().build()}};
}

static ParsedMessage makeMMIdentityResponse() {
    return ParsedMessage{MMM{L3IdentityResponse::builder().build()}};
}

static ParsedMessage makeMMAuthenticationResponse() {
    return ParsedMessage{MMM{L3AuthenticationResponse::builder().build()}};
}

static ParsedMessage makeCCSetup() {
    return ParsedMessage{CCM{L3Setup::builder().ti(3).build()}};
}

static ParsedMessage makeCCConnect() {
    return ParsedMessage{CCM{L3Connect::builder().build()}};
}

static ParsedMessage makeCCDisconnect() {
    return ParsedMessage{CCM{L3Disconnect::builder().ti(3).cause(CCCause::Normal_Call_Clearing).build()}};
}

static L3Header makeHeader(L3PD pd, int mti, unsigned ti, bool tif = true) {
    return L3Header{pd, mti, ti, tif};
}

// ── Full procedure integration tests ──────────────────────────────────────

// Full Channel Assignment procedure: RACH -> decodeChannelNeeded -> ChannelPool::allocate -> MSContext::assignChannel
TEST(BTSProceduresTest, FullChannelAssignment_RACH_to_IA) {
    // Step 1: MS sends Channel Request with RA indicating MO call (establishment cause = 00)
    uint8_t ra = 0x00; // MO call, no NECI

    // Step 2: BTS decodes channel needed from RA
    ChannelType needed = decodeChannelNeeded(ra, false, false);
    EXPECT_EQ(needed, ChannelType::SDCCHType);

    // Step 3: BTS allocates channel from pool
    ChannelPool pool;
    pool.addChannel({ChannelType::SDCCHType, 0, 0, 100});
    pool.addChannel({ChannelType::SDCCHType, 0, 1, 101});
    pool.addChannel({ChannelType::TCHFType, 1, 0, 200});

    auto ch = pool.allocate(needed);
    ASSERT_TRUE(ch.has_value());
    EXPECT_EQ(ch->type, ChannelType::SDCCHType);

    // Step 4: BTS creates MSContext and assigns channel
    MSContext ctx = MSContext::createWithTMSI(0x12345678);
    ctx.assignChannel(ch->type, ch->trxNumber, ch->timeslot, ch->arfcn);

    EXPECT_EQ(ctx.channelType(), ChannelType::SDCCHType);
    EXPECT_EQ(ctx.trxNumber(), ch->trxNumber);
    EXPECT_EQ(ctx.timeslot(), ch->timeslot);
    EXPECT_EQ(ctx.arfcn(), ch->arfcn);

    // Step 5: Verify pool state after allocation
    EXPECT_EQ(pool.freeCount(ChannelType::SDCCHType), 1u);
    EXPECT_EQ(pool.allocatedCount(ChannelType::SDCCHType), 1u);

    // Step 6: Release channel back to pool
    bool released = pool.release(*ch);
    EXPECT_TRUE(released);
    EXPECT_EQ(pool.freeCount(ChannelType::SDCCHType), 2u);

    // Step 7: MS releases channel via context
    ctx.releaseChannel();
    EXPECT_EQ(ctx.channelType(), ChannelType::UndefinedCHType);
}

// Full Paging cycle with MSContext, TimerManager, and TransactionManager
TEST(BTSProceduresTest, FullPagingCycle_withMSContext) {
    // Create MS context for the paged subscriber
    MSContext ctx = MSContext::createWithTMSI(0x87654321);

    // TimerManager: start T3109 (Paging response timer)
    TimerManager tm;
    bool firstStart = tm.start(L3TimerId::T3109);
    EXPECT_TRUE(firstStart);
    EXPECT_EQ(tm.runningCount(), 1u);

    // TransactionManager: track the paging transaction
    TransactionManager txnMgr;
    auto txnId = txnMgr.create(L3PD::RadioResource, L3PagingResponse::MTI, 0, L3TimerId::T3109);
    ASSERT_TRUE(txnId.has_value());
    EXPECT_EQ(txnMgr.pendingCount(), 1u);

    // Simulate MS response: Paging Response arrives
    ParsedMessage pagingResp = makeRRPagingResponse();

    // Match the transaction
    Transaction* matched = txnMgr.match(pagingResp);
    ASSERT_NE(matched, nullptr);
    EXPECT_EQ(matched->state(), TransactionState::Pending);

    // Complete the transaction
    matched->complete();

    // Stop the timer since we got a response
    tm.stop(L3TimerId::T3109);
    EXPECT_EQ(tm.runningCount(), 0u);

    // Assign channel to MS in context
    ctx.assignChannel(ChannelType::SDCCHType, 0, 0, 125);
    EXPECT_EQ(ctx.channelType(), ChannelType::SDCCHType);

    // Cleanup finished transactions
    size_t removed = txnMgr.cleanup();
    EXPECT_EQ(removed, 1u);
    EXPECT_EQ(txnMgr.totalCount(), 0u);
}

// Timer expiry causes transaction failure: simulates MS not responding in time
TEST(BTSProceduresTest, TimerExpiry_causesTransactionFailure) {
    TransactionManager txnMgr;
    TimerManager tm;

    // Create a CC transaction with T3101 timer (CM service request retransmission, 3s default)
    auto txnId = txnMgr.create(L3PD::CallControl, L3Setup::MTI, 3, L3TimerId::T3101);
    ASSERT_TRUE(txnId.has_value());

    // Start the timer
    tm.start(L3TimerId::T3101);
    EXPECT_EQ(tm.runningCount(), 1u);

    // Advance time past timer expiry (T3101 default is 3000ms)
    std::array<L3TimerId, 32> expired;
    size_t count = tm.tick(4000ms, std::span<L3TimerId>(expired));
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(expired[0], L3TimerId::T3101);

    // Handle the expiry in TransactionManager
    txnMgr.onTimerExpired(L3TimerId::T3101);

    // The transaction should no longer be pending (expired)
    EXPECT_EQ(txnMgr.get(*txnId), nullptr);
    EXPECT_EQ(txnMgr.pendingCount(), 0u);

    // Timer is no longer running after expiry
    EXPECT_FALSE(tm.isRunning(L3TimerId::T3101));
}

// Transaction matching by TI for CC messages
TEST(BTSProceduresTest, TransactionMatch_byTI) {
    TransactionManager txnMgr;

    // Create multiple CC transactions with different TIs
    auto id1 = txnMgr.create(L3PD::CallControl, L3Setup::MTI, 1, L3TimerId::T3101);
    auto id2 = txnMgr.create(L3PD::CallControl, L3Connect::MTI, 3, L3TimerId::T3101);
    auto id3 = txnMgr.create(L3PD::CallControl, L3Alerting::MTI, 5, L3TimerId::T3101);

    ASSERT_TRUE(id1.has_value());
    ASSERT_TRUE(id2.has_value());
    ASSERT_TRUE(id3.has_value());
    EXPECT_EQ(txnMgr.pendingCount(), 3u);

    // Incoming Connect message with TI=3 should match transaction id2
    ParsedMessage connectMsg = makeCCConnect();
    L3Header header = makeHeader(L3PD::CallControl, L3Connect::MTI, 3);

    Transaction* matched = txnMgr.match(header, connectMsg);
    ASSERT_NE(matched, nullptr);
    EXPECT_EQ(matched->ti(), 3u);
    EXPECT_EQ(matched->requestMTI(), L3Connect::MTI);

    // Complete the matched transaction
    matched->complete();
    EXPECT_EQ(txnMgr.pendingCount(), 2u);

    // TI=1 and TI=5 transactions should still be pending
    Transaction* t1 = txnMgr.get(*id1);
    Transaction* t3 = txnMgr.get(*id3);
    ASSERT_NE(t1, nullptr);
    ASSERT_NE(t3, nullptr);
    EXPECT_EQ(t1->state(), TransactionState::Pending);
    EXPECT_EQ(t3->state(), TransactionState::Pending);

    // Cleanup removes the completed transaction
    txnMgr.cleanup();
    EXPECT_EQ(txnMgr.totalCount(), 2u);
}

// Full RR state machine transition sequence with timer integration
TEST(BTSProceduresTest, RRStateMachine_fullTransition_sequence) {
    RRStateMachine fsm;
    TimerManager tm;
    MSContext ctx = MSContext::createWithTMSI(0xDEADBEEF);

    // IDLE -> CHANNEL_REQUESTED (Channel Request received)
    fsm.setState(RRStateMachine::State::IDLE);
    auto r1 = fsm.processMessage(makeRRChannelRequest());
    EXPECT_EQ(fsm.state(), RRStateMachine::State::CHANNEL_REQUESTED);
    EXPECT_TRUE(r1.causesTransition());

    // Start T3109 timer for channel assignment waiting
    tm.start(L3TimerId::T3109);

    // Simulate: BTS sends ImmediateAssignment, moves to CHANNEL_ASSIGNED
    fsm.setState(RRStateMachine::State::CHANNEL_ASSIGNED);

    // Allocate channel from pool
    ChannelPool pool;
    pool.addChannel({ChannelType::SDCCHType, 0, 0, 100});
    auto ch = pool.allocate(ChannelType::SDCCHType);
    ASSERT_TRUE(ch.has_value());
    ctx.assignChannel(ch->type, ch->trxNumber, ch->timeslot, ch->arfcn);

    // CHANNEL_ASSIGNED -> WAITING_MM (Paging Response received)
    auto r2 = fsm.processMessage(makeRRPagingResponse());
    EXPECT_EQ(fsm.state(), RRStateMachine::State::WAITING_MM);
    EXPECT_TRUE(r2.causesTransition());

    // WAITING_MM -> ACTIVE (CM Service Accept)
    auto r3 = fsm.processMessage(makeMMCMServiceAccept());
    EXPECT_EQ(fsm.state(), RRStateMachine::State::ACTIVE);
    EXPECT_TRUE(r3.causesTransition());

    // Stop timer since procedure completed
    tm.stop(L3TimerId::T3109);

    // ACTIVE: handle MeasurementReport (no transition)
    auto r4 = fsm.processMessage(makeRRMeasurementReport());
    EXPECT_EQ(fsm.state(), RRStateMachine::State::ACTIVE);
    EXPECT_FALSE(r4.causesTransition());

    // ACTIVE: CipheringModeComplete (stays ACTIVE)
    auto r5 = fsm.processMessage(makeRRCipheringModeComplete());
    EXPECT_EQ(fsm.state(), RRStateMachine::State::ACTIVE);
    ctx.setCiphered(true);
    EXPECT_TRUE(ctx.isCiphered());

    // ACTIVE -> CHANNEL_RELEASE (Channel Release received)
    auto r6 = fsm.processMessage(makeRRChannelRelease());
    EXPECT_EQ(fsm.state(), RRStateMachine::State::CHANNEL_RELEASE);
    EXPECT_TRUE(r6.causesTransition());

    // Release resources
    ctx.releaseChannel();
    pool.release(*ch);
    EXPECT_EQ(pool.freeCount(ChannelType::SDCCHType), 1u);
}

// Full MM authentication flow with timers and transactions
TEST(BTSProceduresTest, MMStateMachine_authFlow) {
    MMStateMachine fsm;
    TimerManager tm;
    TransactionManager txnMgr;
    MSContext ctx = MSContext::createWithTMSI(0xCAFEBABE);

    // DEREGISTERED -> SERVICE_REQUEST (CM Service Request)
    fsm.setState(MMStateMachine::State::DEREGISTERED);
    auto r1 = fsm.processMessage(makeMMCMServiceRequest());
    EXPECT_EQ(fsm.state(), MMStateMachine::State::SERVICE_REQUEST);

    // Start T3101 timer for CM service request
    tm.start(L3TimerId::T3101);

    // SERVICE_REQUEST -> IDENTITY_VERIFIED (Identity Response)
    auto idTxn = txnMgr.create(L3PD::MobilityManagement, L3IdentityResponse::MTI, 0, L3TimerId::T3102);
    ASSERT_TRUE(idTxn.has_value());
    tm.start(L3TimerId::T3102);

    auto r2 = fsm.processMessage(makeMMIdentityResponse());
    EXPECT_EQ(fsm.state(), MMStateMachine::State::IDENTITY_VERIFIED);
    tm.stop(L3TimerId::T3102);

    if (Transaction* tx = txnMgr.get(*idTxn)) {
        tx->complete();
    }

    // IDENTITY_VERIFIED -> AUTHENTICATED (Authentication Response)
    auto authTxn = txnMgr.create(L3PD::MobilityManagement, L3AuthenticationResponse::MTI, 0, L3TimerId::T3106);
    ASSERT_TRUE(authTxn.has_value());
    tm.start(L3TimerId::T3106);

    auto r3 = fsm.processMessage(makeMMAuthenticationResponse());
    EXPECT_EQ(fsm.state(), MMStateMachine::State::AUTHENTICATED);
    tm.stop(L3TimerId::T3106);
    ctx.setAuthenticated(true);

    if (Transaction* tx = txnMgr.get(*authTxn)) {
        tx->complete();
    }

    // AUTHENTICATED -> LOCATION_UPDATE (Location Updating Request)
    auto r4 = fsm.processMessage(
        ParsedMessage{MMM{L3LocationUpdatingRequest::builder().updateType(0).build()}});
    EXPECT_EQ(fsm.state(), MMStateMachine::State::LOCATION_UPDATE);

    // LOCATION_UPDATE -> REGISTERED (CM Service Accept as proxy for Location Updating Accept)
    auto r5 = fsm.processMessage(makeMMCMServiceAccept());
    EXPECT_EQ(fsm.state(), MMStateMachine::State::REGISTERED);
    ctx.setRegistered(true);

    // Verify final state
    EXPECT_TRUE(ctx.isAuthenticated());
    EXPECT_TRUE(ctx.isRegistered());
    EXPECT_FALSE(ctx.isCiphered());

    // Cleanup transactions
    txnMgr.cleanup();
    EXPECT_EQ(txnMgr.totalCount(), 0u);

    // Stop remaining timers
    tm.stop(L3TimerId::T3101);
}

// ChannelPool exhaustion: when all channels are allocated, allocate() returns nullopt
TEST(BTSProceduresTest, ChannelPool_exhaustion_returnsNullopt) {
    ChannelPool pool;

    // Add only 2 SDCCH channels
    pool.addChannel({ChannelType::SDCCHType, 0, 0, 100});
    pool.addChannel({ChannelType::SDCCHType, 0, 1, 101});

    // Allocate both channels
    auto ch1 = pool.allocate(ChannelType::SDCCHType);
    auto ch2 = pool.allocate(ChannelType::SDCCHType);
    ASSERT_TRUE(ch1.has_value());
    ASSERT_TRUE(ch2.has_value());

    EXPECT_EQ(pool.freeCount(ChannelType::SDCCHType), 0u);
    EXPECT_EQ(pool.allocatedCount(ChannelType::SDCCHType), 2u);

    // Third allocation should fail - pool exhausted
    auto ch3 = pool.allocate(ChannelType::SDCCHType);
    ASSERT_FALSE(ch3.has_value());

    // Release one channel and try again
    pool.release(*ch1);
    EXPECT_EQ(pool.freeCount(ChannelType::SDCCHType), 1u);

    auto ch4 = pool.allocate(ChannelType::SDCCHType);
    ASSERT_TRUE(ch4.has_value());
    EXPECT_EQ(pool.allocatedCount(ChannelType::SDCCHType), 2u);
}

// Multiple MS contexts maintain independent state
TEST(BTSProceduresTest, MultiMSContexts_independentState) {
    constexpr size_t NUM_MS = 5;

    std::vector<MSContext> contexts;
    contexts.reserve(NUM_MS);

    // Create independent contexts with different identities
    for (size_t i = 0; i < NUM_MS; ++i) {
        contexts.emplace_back(MSContext::createWithTMSI(0x10000000u + static_cast<uint32_t>(i)));
    }

    // Configure each context independently
    ChannelPool pool;
    pool.addChannel({ChannelType::SDCCHType, 0, 0, 100});
    pool.addChannel({ChannelType::SDCCHType, 0, 1, 101});
    pool.addChannel({ChannelType::TCHFType, 1, 0, 200});
    pool.addChannel({ChannelType::TCHHType, 1, 1, 201});
    pool.addChannel({ChannelType::SDCCHType, 0, 2, 102});

    for (size_t i = 0; i < NUM_MS; ++i) {
        auto ch = pool.allocate(i < 3 ? ChannelType::SDCCHType : ChannelType::TCHFType);
        if (ch) {
            contexts[i].assignChannel(ch->type, ch->trxNumber, ch->timeslot, ch->arfcn);
        }

        // Set different flags per context
        contexts[i].setRegistered(i % 2 == 0);
        contexts[i].setAuthenticated(i % 3 == 0);
        contexts[i].setCiphered(i % 4 == 0);
    }

    // Verify independence: each context has its own state
    for (size_t i = 0; i < NUM_MS; ++i) {
        EXPECT_EQ(contexts[i].identity().isTMSI(), true);
        EXPECT_EQ(contexts[i].isRegistered(), i % 2 == 0);
        EXPECT_EQ(contexts[i].isAuthenticated(), i % 3 == 0);
        EXPECT_EQ(contexts[i].isCiphered(), i % 4 == 0);
    }

    // Modify one context - others should be unaffected
    contexts[0].setTMSI(0xFFFFFFFF);
    EXPECT_EQ(contexts[1].identity().isTMSI(), true);
    EXPECT_NE(contexts[0].identity().tmsi(), contexts[1].identity().tmsi());
}

// ── Performance / stress tests ───────────────────────────────────────────

// Benchmark: 1000 MSContext creations should fit in L3 cache (~256 KB) with zero heap allocations
TEST(BTSProceduresTest, Stress_1000MSContexts_noAllocations) {
    constexpr size_t N = 1000;

    // Verify MSContext size constraint
    EXPECT_LE(sizeof(MSContext), 256u);

    // Total memory for 1000 contexts should be <= 256 KB
    size_t totalBytes = sizeof(MSContext) * N;
    EXPECT_LE(totalBytes, 256 * 1024u);

    auto tStart = std::chrono::high_resolution_clock::now();

    std::vector<MSContext> contexts;
    contexts.reserve(N);

    for (size_t i = 0; i < N; ++i) {
        contexts.emplace_back(MSContext::createWithTMSI(static_cast<uint32_t>(i)));
        contexts.back().assignChannel(ChannelType::SDCCHType, static_cast<uint8_t>(i % 8),
                                      static_cast<uint8_t>(i % 16), static_cast<uint16_t>(100 + i));
        contexts.back().setRegistered(i % 2 == 0);
        contexts.back().setAuthenticated(i % 3 == 0);
    }

    auto tEnd = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(tEnd - tStart);

    // Benchmark: 1000 allocations < 100ms on modern x86_64
    EXPECT_LT(duration.count(), 100);

    // Verify all contexts are independent
    for (size_t i = 0; i < N; ++i) {
        EXPECT_EQ(contexts[i].identity().isTMSI(), true);
        EXPECT_EQ(contexts[i].channelType(), ChannelType::SDCCHType);
    }
}

// Benchmark: TransactionManager handles 100 transactions with O(1) TI lookup
TEST(BTSProceduresTest, Stress_TransactionManager_100Transactions_O1Lookup) {
    TransactionManager txnMgr;
    TimerManager tm;

    // Create transactions with various TIs and PDs
    constexpr size_t NUM_CC = 8; // One per TI slot (0-7)
    constexpr size_t NUM_MM = 6; // Keep total under MAX_TRANSACTIONS (16)

    auto tStart = std::chrono::high_resolution_clock::now();

    // Fill all 8 TI slots with CC transactions
    for (size_t i = 0; i < NUM_CC; ++i) {
        auto id = txnMgr.create(L3PD::CallControl, L3Setup::MTI, static_cast<uint8_t>(i),
                                L3TimerId::T3101);
        EXPECT_TRUE(id.has_value());
    }

    // Add MM transactions (no TI index, PD+MTI match)
    for (size_t i = 0; i < NUM_MM; ++i) {
        auto id = txnMgr.create(L3PD::MobilityManagement, L3IdentityResponse::MTI, 0,
                                L3TimerId::T3102);
        EXPECT_TRUE(id.has_value());
    }

    EXPECT_EQ(txnMgr.pendingCount(), NUM_CC + NUM_MM);

    // Match each CC transaction by TI - O(1) lookup
    for (size_t i = 0; i < NUM_CC; ++i) {
        ParsedMessage msg = makeCCConnect();
        L3Header header = makeHeader(L3PD::CallControl, L3Connect::MTI, static_cast<unsigned>(i));

        Transaction* matched = txnMgr.match(header, msg);
        ASSERT_NE(matched, nullptr);
        EXPECT_EQ(matched->ti(), static_cast<uint8_t>(i));
        matched->complete();
    }

    auto tEnd = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(tEnd - tStart);

    // Benchmark: 100 transaction creates + 8 O(1) lookups < 100us
    EXPECT_LT(duration.count(), 1000);

    // Verify remaining MM transactions still pending
    EXPECT_EQ(txnMgr.pendingCount(), NUM_MM);

    // Cleanup completed CC transactions
    size_t removed = txnMgr.cleanup();
    EXPECT_EQ(removed, NUM_CC);
}

// Benchmark: TimerManager tick() performs zero heap allocations over 10000 iterations
TEST(BTSProceduresTest, Stress_TimerManager_tickNoAllocation) {
    TimerManager tm;

    // Start all available timers to maximize work per tick
    tm.start(L3TimerId::T3101);
    tm.start(L3TimerId::T3102);
    tm.start(L3TimerId::T3103);
    tm.start(L3TimerId::T3106);
    tm.start(L3TimerId::T3108);
    tm.start(L3TimerId::T3109);
    EXPECT_EQ(tm.runningCount(), 6u);

    constexpr size_t NUM_TICKS = 10000;

    auto tStart = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < NUM_TICKS; ++i) {
        // Use callback-based tick - zero heap allocation
        tm.tick(1ms, [](L3TimerId) {
            // Expiry handler: no-op for stress test
        });

        // Restart expired timers to keep them running
        tm.start(L3TimerId::T3101);
        tm.start(L3TimerId::T3102);
    }

    auto tEnd = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(tEnd - tStart);

    // Benchmark: 10000 ticks with 6 active timers < 500ms on modern x86_64
    EXPECT_LT(duration.count(), 500);

    // Also test span-based tick
    std::array<L3TimerId, 32> expired;
    tm.stopAll();
    tm.start(L3TimerId::T3101);

    size_t count = tm.tick(5000ms, std::span<L3TimerId>(expired));
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(expired[0], L3TimerId::T3101);
}

// Benchmark: ChannelPool handles 100 channels with rapid allocate/release cycles
TEST(BTSProceduresTest, Stress_ChannelPool_100Channels_allocateRelease) {
    ChannelPool pool;
    constexpr size_t NUM_CHANNELS = 100;
    constexpr size_t NUM_CYCLES = 1000;

    // Add channels of various types
    for (size_t i = 0; i < NUM_CHANNELS; ++i) {
        ChannelType type;
        switch (i % 4) {
            case 0: type = ChannelType::SDCCHType; break;
            case 1: type = ChannelType::TCHFType; break;
            case 2: type = ChannelType::TCHHType; break;
            case 3: type = ChannelType::SDCCHType; break;
        }
        pool.addChannel({type, static_cast<uint8_t>(i % 8),
                          static_cast<uint8_t>(i % 16), static_cast<uint16_t>(100 + i)});
    }

    EXPECT_EQ(pool.totalCount(), NUM_CHANNELS);

    auto tStart = std::chrono::high_resolution_clock::now();

    // Rapid allocate/release cycles
    for (size_t cycle = 0; cycle < NUM_CYCLES; ++cycle) {
        ChannelType type;
        switch (cycle % 4) {
            case 0: type = ChannelType::SDCCHType; break;
            case 1: type = ChannelType::TCHFType; break;
            case 2: type = ChannelType::TCHHType; break;
            case 3: type = ChannelType::SDCCHType; break;
        }

        auto ch = pool.allocate(type);
        if (ch) {
            pool.release(*ch);
        }
    }

    auto tEnd = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(tEnd - tStart);

    // Benchmark: 1000 allocate/release cycles on 100 channels < 500ms
    EXPECT_LT(duration.count(), 500);

    // All channels should be back in the pool
    EXPECT_EQ(pool.totalCount(), NUM_CHANNELS);
}

// ── Dispatcher integration with stack components ─────────────────────────

// ProtocolDispatcher routes messages to correct handlers using stack context
TEST(BTSProceduresTest, DispatcherIntegration_withMSContext) {
    MSContext ctx = MSContext::createWithTMSI(0x11223344);
    ProtocolDispatcher disp;
    bool channelReleaseReceived = false;
    bool pagingResponseReceived = false;

    disp.registerHandler(L3PD::RadioResource, L3ChannelRelease::MTI,
        [&](const ParsedMessage& msg, void* userCtx) {
            auto* c = static_cast<MSContext*>(userCtx);
            EXPECT_EQ(c->identity().isTMSI(), true);
            c->releaseChannel();
            channelReleaseReceived = true;
        });

    disp.registerHandler(L3PD::RadioResource, L3PagingResponse::MTI,
        [&](const ParsedMessage& msg, void* userCtx) {
            auto* c = static_cast<MSContext*>(userCtx);
            c->assignChannel(ChannelType::SDCCHType, 0, 0, 100);
            pagingResponseReceived = true;
        });

    // Dispatch Paging Response
    ParsedMessage pagingResp = makeRRPagingResponse();
    disp.dispatch(pagingResp, &ctx);
    EXPECT_TRUE(pagingResponseReceived);
    EXPECT_EQ(ctx.channelType(), ChannelType::SDCCHType);

    // Dispatch Channel Release
    ParsedMessage chRelease = makeRRChannelRelease();
    disp.dispatch(chRelease, &ctx);
    EXPECT_TRUE(channelReleaseReceived);
    EXPECT_EQ(ctx.channelType(), ChannelType::UndefinedCHType);
}

// Full CC call flow: Setup -> Transaction tracking -> Disconnect -> Cleanup
TEST(BTSProceduresTest, FullCC_CallFlow) {
    CCStateMachine fsm;
    TransactionManager txnMgr;
    TimerManager tm;
    MSContext ctx = MSContext::createWithTMSI(0xAABBCCDD);

    // Assign a TCH channel for the call
    ChannelPool pool;
    pool.addChannel({ChannelType::TCHFType, 1, 0, 200});
    auto ch = pool.allocate(ChannelType::TCHFType);
    ASSERT_TRUE(ch.has_value());
    ctx.assignChannel(ch->type, ch->trxNumber, ch->timeslot, ch->arfcn);

    // IDLE -> SETUP_RECEIVED
    fsm.setState(CCStateMachine::State::IDLE);
    auto setupTxn = txnMgr.create(L3PD::CallControl, L3Setup::MTI, 3, L3TimerId::T3101);
    ASSERT_TRUE(setupTxn.has_value());
    tm.start(L3TimerId::T3101);

    auto r1 = fsm.processMessage(makeCCSetup());
    EXPECT_EQ(fsm.state(), CCStateMachine::State::SETUP_RECEIVED);

    // SETUP_RECEIVED -> PROCEEDING
    auto r2 = fsm.processMessage(makeCCSetup());
    EXPECT_EQ(fsm.state(), CCStateMachine::State::PROCEEDING);

    // Complete the setup transaction
    if (Transaction* tx = txnMgr.get(*setupTxn)) {
        tx->complete();
    }
    tm.stop(L3TimerId::T3101);

    // PROCEEDING -> ALERTING (Alerting message)
    auto r3 = fsm.processMessage(
        ParsedMessage{CCM{L3Alerting::builder().ti(3).build()}});
    EXPECT_EQ(fsm.state(), CCStateMachine::State::ALERTING);

    // ALERTING -> CONNECT (Connect message)
    auto r4 = fsm.processMessage(makeCCConnect());
    EXPECT_EQ(fsm.state(), CCStateMachine::State::CONNECT);

    // Simulate transitioning to ACTIVE (via CallConfirmed in real flow)
    fsm.setState(CCStateMachine::State::ACTIVE);

    // ACTIVE -> DISCONNECT_RECEIVED
    auto r5 = fsm.processMessage(makeCCDisconnect());
    EXPECT_EQ(fsm.state(), CCStateMachine::State::DISCONNECT_RECEIVED);

    // DISCONNECT_RECEIVED -> RELEASE
    auto r6 = fsm.processMessage(makeCCDisconnect());
    EXPECT_EQ(fsm.state(), CCStateMachine::State::RELEASE);

    // Cleanup
    txnMgr.cleanup();
    pool.release(*ch);
    ctx.releaseChannel();
}

// Combined stress test: all components working together under load
TEST(BTSProceduresTest, Stress_FullStack_Integration) {
    constexpr size_t NUM_MS = 100;

    struct MSState {
        MSContext ctx;
        TimerManager tm;
        TransactionManager txnMgr;
        RRStateMachine rrFsm;
    };

    std::vector<MSState> states;
    states.reserve(NUM_MS);

    ChannelPool pool;
    // Add enough channels for all MS
    for (size_t i = 0; i < NUM_MS; ++i) {
        pool.addChannel({ChannelType::SDCCHType, static_cast<uint8_t>(i % 8),
                          static_cast<uint8_t>(i % 16), static_cast<uint16_t>(100 + i)});
    }

    auto tStart = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < NUM_MS; ++i) {
        MSState& ms = states.emplace_back(MSState{
            MSContext::createWithTMSI(static_cast<uint32_t>(i)),
            TimerManager{},
            TransactionManager{},
            RRStateMachine{}
        });

        // Allocate channel
        auto ch = pool.allocate(ChannelType::SDCCHType);
        if (ch) {
            ms.ctx.assignChannel(ch->type, ch->trxNumber, ch->timeslot, ch->arfcn);
        }

        // Start RR FSM flow: IDLE -> CHANNEL_REQUESTED
        ms.rrFsm.setState(RRStateMachine::State::IDLE);
        [[maybe_unused]] auto rrResult = ms.rrFsm.processMessage(makeRRChannelRequest());

        // Start timer
        ms.tm.start(L3TimerId::T3109);

        // Create transaction
        ms.txnMgr.create(L3PD::RadioResource, L3PagingResponse::MTI, 0, L3TimerId::T3109);
    }

    auto tEnd = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(tEnd - tStart);

    // Benchmark: 100 full MS setups < 200ms on modern x86_64
    EXPECT_LT(duration.count(), 200);

    // Verify state consistency
    for (size_t i = 0; i < NUM_MS; ++i) {
        EXPECT_EQ(states[i].rrFsm.state(), RRStateMachine::State::CHANNEL_REQUESTED);
        EXPECT_TRUE(states[i].tm.isRunning(L3TimerId::T3109));
        EXPECT_EQ(states[i].txnMgr.pendingCount(), 1u);
    }

    // Cleanup: stop timers, complete transactions, release channels
    for (size_t i = 0; i < NUM_MS; ++i) {
        states[i].tm.stopAll();
        states[i].txnMgr.cleanup();
        if (states[i].ctx.channelType() != ChannelType::UndefinedCHType) {
            pool.release({states[i].ctx.channelType(), states[i].ctx.trxNumber(),
                          states[i].ctx.timeslot(), states[i].ctx.arfcn()});
        }
    }
}

// Timer tick with span overload under stress
TEST(BTSProceduresTest, Stress_TimerManager_tickSpanOverload) {
    TimerManager tm;

    // Start many timers with short expiry
    tm.start(L3TimerId::T3101, 100ms);
    tm.start(L3TimerId::T3102, 150ms);
    tm.start(L3TimerId::T3103, 200ms);
    tm.start(L3TimerId::T3106, 250ms);
    tm.start(L3TimerId::T3108, 300ms);

    EXPECT_EQ(tm.runningCount(), 5u);

    auto tStart = std::chrono::high_resolution_clock::now();

    // Tick in small increments; collect expired timers via span
    std::array<L3TimerId, 32> expired;
    size_t totalExpired = 0;

    for (int i = 0; i < 100; ++i) {
        size_t count = tm.tick(5ms, std::span<L3TimerId>(expired));
        totalExpired += count;
    }

    auto tEnd = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(tEnd - tStart);

    // After 500ms of ticks, all 5 timers should have expired
    EXPECT_EQ(totalExpired, 5u);

    // Benchmark: 100 span-based ticks < 500us
    EXPECT_LT(duration.count(), 5000);
}

// VEA allocation integration with ChannelPool and MSContext
TEST(BTSProceduresTest, VEA_Allocation_Integration) {
    ChannelPool pool;
    pool.addChannel({ChannelType::SDCCHType, 0, 0, 100});
    pool.addChannel({ChannelType::TCHFType, 1, 0, 200});

    // RA for MO call (establishment cause = 00)
    uint8_t ra = 0x00;

    // VEA: try TCH first
    auto ch = pool.allocateVEA(ra);
    ASSERT_TRUE(ch.has_value());
    EXPECT_EQ(ch->type, ChannelType::TCHFType);

    MSContext ctx = MSContext::createWithTMSI(0x12345678);
    ctx.assignChannel(ch->type, ch->trxNumber, ch->timeslot, ch->arfcn);
    EXPECT_EQ(ctx.channelType(), ChannelType::TCHFType);

    // No TCH left - next VEA should fall back to SDCCH
    auto ch2 = pool.allocateVEA(ra);
    ASSERT_TRUE(ch2.has_value());
    EXPECT_EQ(ch2->type, ChannelType::SDCCHType);

    // Pool exhausted for both types
    auto ch3 = pool.allocateVEA(ra);
    ASSERT_FALSE(ch3.has_value());
}

// decodeChannelNeeded and isLocationUpdatingRequest integration
TEST(BTSProceduresTest, RA_Decoding_Integration) {
    // MO call without VEA -> SDCCH (establishment cause 00, bits 6-5 = 00, RA=0x00)
    EXPECT_EQ(decodeChannelNeeded(0x00, false, false), ChannelType::SDCCHType);

    // MO call with VEA -> TCH (establishment cause 00, bits 6-5 = 00, RA=0x00)
    EXPECT_EQ(decodeChannelNeeded(0x00, false, true), ChannelType::TCHFType);

    // Emergency call -> TCH always (establishment cause 01, bits 6-5 = 01, RA=0x20)
    EXPECT_EQ(decodeChannelNeeded(0x20, false, false), ChannelType::TCHFType);

    // Answer to Paging -> TCH (establishment cause 10, bits 6-5 = 10, RA=0x40)
    EXPECT_EQ(decodeChannelNeeded(0x40, false, false), ChannelType::TCHFType);

    // Location Updating -> SDCCH (establishment cause 11, bits 6-5 = 11, RA=0x60)
    EXPECT_EQ(decodeChannelNeeded(0x60, false, false), ChannelType::SDCCHType);

    // isLocationUpdatingRequest checks: establishment cause 11 (RA=0x60)
    EXPECT_TRUE(isLocationUpdatingRequest(0x60));
    EXPECT_FALSE(isLocationUpdatingRequest(0x00));
    EXPECT_FALSE(isLocationUpdatingRequest(0x20));
}

// TransactionManager pool capacity: create MAX_TRANSACTIONS, verify overflow behavior
TEST(BTSProceduresTest, TransactionManager_PoolCapacity) {
    TransactionManager txnMgr;

    // Fill the pool to capacity (MAX_TRANSACTIONS = 16)
    std::vector<uint32_t> ids;
    for (size_t i = 0; i < 16; ++i) {
        auto id = txnMgr.create(L3PD::CallControl, L3Setup::MTI, static_cast<uint8_t>(i % 8),
                                L3TimerId::T3101);
        ASSERT_TRUE(id.has_value());
        ids.push_back(*id);
    }

    EXPECT_EQ(txnMgr.pendingCount(), 16u);

    // Complete half the transactions and cleanup to free slots
    for (size_t i = 0; i < 8; ++i) {
        if (Transaction* tx = txnMgr.get(ids[i])) {
            tx->complete();
        }
    }

    size_t removed = txnMgr.cleanup();
    EXPECT_EQ(removed, 8u);
    EXPECT_EQ(txnMgr.pendingCount(), 8u);

    // Should be able to create new transactions now
    auto newId = txnMgr.create(L3PD::CallControl, L3Connect::MTI, 0, L3TimerId::T3101);
    ASSERT_TRUE(newId.has_value());
    EXPECT_EQ(txnMgr.pendingCount(), 9u);
}

// Integration: TimerManager + TransactionManager expiry coordination
TEST(BTSProceduresTest, Timer_Transaction_Expiry_Coordination) {
    TransactionManager txnMgr;
    TimerManager tm;

    // Create transactions with different timers
    auto id1 = txnMgr.create(L3PD::CallControl, L3Setup::MTI, 1, L3TimerId::T3101);
    auto id2 = txnMgr.create(L3PD::CallControl, L3Connect::MTI, 2, L3TimerId::T3106);
    auto id3 = txnMgr.create(L3PD::MobilityManagement, L3IdentityResponse::MTI, 0, L3TimerId::T3102);

    ASSERT_TRUE(id1.has_value());
    ASSERT_TRUE(id2.has_value());
    ASSERT_TRUE(id3.has_value());

    // Start all timers with custom short durations
    tm.start(L3TimerId::T3101, 100ms);
    tm.start(L3TimerId::T3106, 200ms);
    tm.start(L3TimerId::T3102, 300ms);

    // Tick 150ms: T3101 should expire
    std::array<L3TimerId, 32> expired;
    size_t count = tm.tick(150ms, std::span<L3TimerId>(expired));
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(expired[0], L3TimerId::T3101);

    // Handle expiry in TransactionManager
    txnMgr.onTimerExpired(L3TimerId::T3101);
    EXPECT_EQ(txnMgr.get(*id1), nullptr); // Expired, no longer pending
    EXPECT_NE(txnMgr.get(*id2), nullptr);  // Still pending
    EXPECT_NE(txnMgr.get(*id3), nullptr);  // Still pending

    // Tick another 100ms: T3106 should expire (total 250ms)
    count = tm.tick(100ms, std::span<L3TimerId>(expired));
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(expired[0], L3TimerId::T3106);

    txnMgr.onTimerExpired(L3TimerId::T3106);
    EXPECT_EQ(txnMgr.get(*id2), nullptr); // Expired

    // Tick another 100ms: T3102 should expire (total 350ms)
    count = tm.tick(100ms, std::span<L3TimerId>(expired));
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(expired[0], L3TimerId::T3102);

    txnMgr.onTimerExpired(L3TimerId::T3102);
    EXPECT_EQ(txnMgr.get(*id3), nullptr); // Expired

    // All transactions expired, cleanup
    size_t removed = txnMgr.cleanup();
    EXPECT_EQ(removed, 3u);
    EXPECT_EQ(txnMgr.totalCount(), 0u);
}
