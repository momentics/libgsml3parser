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

// Integration tests for ProcedureRunner with SubscriberRegistry: validates full
// procedure flows end-to-end including session management, concurrent procedures,
// timer expiry across multiple subscribers, and sequential procedure chaining.
// 3GPP coverage: TS 24.008 chapters 4.x (MM), 6.x (CC), TS 04.08 chapter 9 (RR).

#include <gtest/gtest.h>
#include "gsml3parser/stack/procedure_runner.h"
#include "gsml3parser/stack/subscriber_registry.h"
#include "gsml3parser/mm/l3mmmessages.h"
#include "gsml3parser/rr/l3rrmessages.h"
#include "gsml3parser/cc/l3ccmessages.h"

#include <chrono>
#include <array>
#include <memory>
#include <vector>

using namespace gsml3parser;
using namespace std::chrono_literals;

// Helper: build a CMServiceRequest with the given service type code.
static ParsedMessage makeCMServiceRequest(L3CMServiceType::TypeCode svcType) {
    auto cmReq = L3CMServiceRequest::builder()
        .serviceType(L3CMServiceType{svcType})
        .build();
    return ParsedMessage{MMM{std::move(cmReq)}};
}

// Helper: generic empty response sink used in integration tests.
static inline void noopSink(SMAction, const ParsedMessage&, const SubscriberSession*) {}

// Test: Full Location Update flow through ProcedureRunner with SubscriberRegistry.
// Creates a session via registry, feeds CMServiceRequest to auto-create LU procedure,
// advances through identity check (TMSI known), auth check (no RAND), LU request,
// and finally feeds external VLR accept data to complete the procedure.
// 3GPP: TS 24.008 4.4.1 - Normal location updating procedure with VLR acceptance.
TEST(Integration, LocationUpdate_FullFlow_WithRegistry) {
    SubscriberRegistry registry;
    auto* session = registry.createByTMSI(0x12345678u);
    ASSERT_NE(session, nullptr);
    EXPECT_EQ(session->context.identity().tmsi(), 0x12345678u);

    ProcedureRunner& runner = session->procedures;

    // Step 1: Feed CMServiceRequest(LocationUpdateRequest) -> auto-creates LU procedure
    //         INIT -> IDENTITY_CHECK
    auto cmReq = makeCMServiceRequest(L3CMServiceType::LocationUpdateRequest);
    auto result1 = runner.feed(cmReq, session, noopSink);
    EXPECT_EQ(result1.action, ProcedureStepResult::Action::Continue);
    EXPECT_NE(runner.getActive(procedure::ProcedureType::LocationUpdate), nullptr);
    EXPECT_EQ(runner.activeCount(), 1u);

    // Step 2: Feed MM message -> IDENTITY_CHECK -> AUTH_CHECK (TMSI known, skip REQUEST_IDENTITY)
    ParsedMessage mmMsg{MMM{L3MMStatus{}}};
    auto result2 = runner.feed(mmMsg, session, noopSink);
    EXPECT_EQ(result2.action, ProcedureStepResult::Action::Continue);

    // Step 3: Feed MM message -> AUTH_CHECK -> LU_REQUEST (no RAND set, skip authentication)
    auto result3 = runner.feed(mmMsg, session, noopSink);
    EXPECT_EQ(result3.action, ProcedureStepResult::Action::Continue);

    // Step 4: Feed MM message -> LU_REQUEST -> WAITING_EXTERNAL + timer T3103
    auto result4 = runner.feed(mmMsg, session, noopSink);
    EXPECT_EQ(result4.action, ProcedureStepResult::Action::Continue);

    // Verify procedure is in WaitingExternal state
    auto* luProc = runner.getActive(procedure::ProcedureType::LocationUpdate);
    ASSERT_NE(luProc, nullptr);
    EXPECT_EQ(luProc->state(), procedure::ProcedureState::WaitingExternal);

    // Step 5: Feed external VLR accept data -> WAITING_EXTERNAL -> SEND_ACCEPT -> Completed
    // Convention: first byte = 1 for Accept, followed by optional TMSI (4 bytes)
    std::array<uint8_t, 5> acceptData{1, 0x78, 0x56, 0x34, 0x12};
    auto result5 = runner.feedExternal(procedure::ProcedureType::LocationUpdate, acceptData,
                                        noopSink);

    // The accept path leads to SEND_ACCEPT which transitions to COMPLETED
    EXPECT_TRUE(result5.action == ProcedureStepResult::Action::Completed ||
                result5.action == ProcedureStepResult::Action::SendResponse);

    // If it returned SendResponse (SEND_ACCEPT state), feed one more time to reach COMPLETED
    if (result5.action == ProcedureStepResult::Action::SendResponse) {
        auto result6 = runner.feed(mmMsg, session, noopSink);
        EXPECT_EQ(result6.action, ProcedureStepResult::Action::Completed);
    }

    // After completion, slot is auto-cleaned
    EXPECT_EQ(runner.activeCount(), 0u);
}

// Test: Full Mobile-Originated Call Setup flow using direct procedure feed with
// real SubscriberSession context. Tests the complete state machine from INIT through
// SERVICE_ACCEPT, WAIT_SETUP, PROCEEDING, ASSIGN_TCH, WAIT_ASSIGN_COMPLETE,
// ALERTING, CONNECT, ACTIVE to COMPLETED.
// 3GPP: TS 24.008 6.1 - Mobile Originated Call establishment sequence.
TEST(Integration, CallSetupMO_FullFlow_WithRegistry) {
    SubscriberRegistry registry;
    auto* session = registry.createByTMSI(0xDEADBEEFu);
    ASSERT_NE(session, nullptr);

    // Create CallSetupMO procedure via factory and feed messages directly
    auto proc = ProcedureFactory::createCallSetupMO();
    ASSERT_NE(proc, nullptr);
    EXPECT_EQ(proc->type(), procedure::ProcedureType::CallSetup_MO);
    EXPECT_EQ(proc->state(), procedure::ProcedureState::Initiated);

    // Step 1: Feed CMServiceRequest (MM PD) -> INIT -> SERVICE_ACCEPT + SendResponse
    auto cmReq = makeCMServiceRequest(L3CMServiceType::MobileOriginatedCall);
    auto result1 = proc->feed(cmReq, session, noopSink);
    EXPECT_EQ(result1.action, ProcedureStepResult::Action::SendResponse);

    // Step 2: Advance SERVICE_ACCEPT -> WAIT_SETUP
    ParsedMessage emptyMM{MMM{L3MMStatus{}}};
    auto result2 = proc->feed(emptyMM, session, noopSink);
    EXPECT_EQ(result2.action, ProcedureStepResult::Action::Continue);

    // Step 3: Feed Setup (CC PD) -> WAIT_SETUP -> PROCEEDING + SendResponse + timer T3101
    ParsedMessage setupMsg{CCM{L3Setup{}}};
    auto result3 = proc->feed(setupMsg, session, noopSink);
    EXPECT_EQ(result3.action, ProcedureStepResult::Action::SendResponse);

    // Step 4: Advance PROCEEDING -> ASSIGN_TCH + SendResponse + timer T3101
    auto result4 = proc->feed(setupMsg, session, noopSink);
    EXPECT_EQ(result4.action, ProcedureStepResult::Action::SendResponse);

    // Step 5: Advance ASSIGN_TCH -> WAIT_ASSIGN_COMPLETE
    auto result5 = proc->feed(setupMsg, session, noopSink);
    EXPECT_EQ(result5.action, ProcedureStepResult::Action::Continue);

    // Step 6: Feed AssignmentComplete (RR PD) -> WAIT_ASSIGN_COMPLETE -> ALERTING + SendResponse
    ParsedMessage assignComplete{RRM{L3AssignmentComplete{}}};
    auto result6 = proc->feed(assignComplete, session, noopSink);
    EXPECT_EQ(result6.action, ProcedureStepResult::Action::SendResponse);

    // Step 7: Advance ALERTING -> CONNECT + SendResponse
    auto result7 = proc->feed(setupMsg, session, noopSink);
    EXPECT_EQ(result7.action, ProcedureStepResult::Action::SendResponse);

    // Step 8: Advance CONNECT -> ACTIVE + SendResponse
    auto result8 = proc->feed(setupMsg, session, noopSink);
    EXPECT_EQ(result8.action, ProcedureStepResult::Action::SendResponse);

    // Step 9: Feed ConnectAcknowledge (CC PD) -> ACTIVE -> COMPLETED
    ParsedMessage connAck{CCM{L3ConnectAcknowledge{}}};
    auto result9 = proc->feed(connAck, session, noopSink);
    EXPECT_EQ(result9.action, ProcedureStepResult::Action::Completed);
    EXPECT_EQ(proc->state(), procedure::ProcedureState::Completed);
}

// Test: Three concurrent subscriber sessions each running different procedures
// simultaneously. Validates that ProcedureRunner instances are independent per session
// and do not interfere with each other.
// 3GPP: TS 24.008 - Independent procedure management per subscriber context.
TEST(Integration, MultipleSubscribers_ConcurrentProcedures) {
    SubscriberRegistry registry;

    // Create three sessions
    auto* sess1 = registry.createByTMSI(0x11111111u);
    auto* sess2 = registry.createByTMSI(0x22222222u);
    auto* sess3 = registry.createByTMSI(0x33333333u);
    ASSERT_NE(sess1, nullptr);
    ASSERT_NE(sess2, nullptr);
    ASSERT_NE(sess3, nullptr);
    EXPECT_EQ(registry.count(), 3u);

    // Session 1: ChannelAssignment procedure (RR)
    ParsedMessage chReq{RRM{L3ChannelRequest{0}}};
    sess1->procedures.feed(chReq, sess1, noopSink);
    EXPECT_EQ(sess1->procedures.activeCount(), 1u);

    // Session 2: LocationUpdate procedure (MM)
    auto cmReq = makeCMServiceRequest(L3CMServiceType::LocationUpdateRequest);
    sess2->procedures.feed(cmReq, sess2, noopSink);
    EXPECT_EQ(sess2->procedures.activeCount(), 1u);

    // Session 3: CallSetupMO procedure (CC)
    ParsedMessage setupMsg{CCM{L3Setup{}}};
    sess3->procedures.feed(setupMsg, sess3, noopSink);
    EXPECT_EQ(sess3->procedures.activeCount(), 1u);

    // Verify each session has exactly one active procedure of the correct type
    EXPECT_NE(sess1->procedures.getActive(procedure::ProcedureType::ChannelAssignment), nullptr);
    EXPECT_NE(sess2->procedures.getActive(procedure::ProcedureType::LocationUpdate), nullptr);
    EXPECT_NE(sess3->procedures.getActive(procedure::ProcedureType::CallSetup_MO), nullptr);

    // Cross-contamination check: each session should only see its own procedures
    EXPECT_EQ(sess1->procedures.getActive(procedure::ProcedureType::LocationUpdate), nullptr);
    EXPECT_EQ(sess2->procedures.getActive(procedure::ProcedureType::ChannelAssignment), nullptr);
    EXPECT_EQ(sess3->procedures.getActive(procedure::ProcedureType::ChannelAssignment), nullptr);

    // Advance all procedures by ticking (no timers should expire yet)
    auto f1 = sess1->procedures.tickAll(100ms);
    auto f2 = sess2->procedures.tickAll(100ms);
    auto f3 = sess3->procedures.tickAll(100ms);
    EXPECT_EQ(f1, 0u);
    EXPECT_EQ(f2, 0u);
    EXPECT_EQ(f3, 0u);

    // All procedures should still be active after short tick
    EXPECT_EQ(sess1->procedures.activeCount(), 1u);
    EXPECT_EQ(sess2->procedures.activeCount(), 1u);
    EXPECT_EQ(sess3->procedures.activeCount(), 1u);
}

// Test: Multiple procedures with timers on the same session; tickAll advances all
// timers simultaneously and returns the total count of expired procedures.
// 3GPP: TS 24.008 - Multiple protocol timers may run concurrently per MS.
TEST(Integration, TimerExpiry_AcrossProcedures) {
    SubscriberRegistry registry;
    auto* session = registry.createByTMSI(0xAABBCCDDu);
    ASSERT_NE(session, nullptr);

    ProcedureRunner& runner = session->procedures;

    // Create ChannelAssignment and advance to state with timer
    ParsedMessage chReq{RRM{L3ChannelRequest{0}}};
    runner.feed(chReq, session, noopSink);

    // Second feed -> ALLOCATE_CHANNEL -> SEND_IMMEDIATE_ASSIGNMENT + timer T3101 (3s)
    ParsedMessage rrMsg{RRM{L3ClassmarkChange{}}};
    runner.feed(rrMsg, session, noopSink);

    EXPECT_EQ(runner.activeCount(), 1u);

    // Create LocationUpdate and advance to WAITING_EXTERNAL with timer T3103 (5s)
    auto cmReq = makeCMServiceRequest(L3CMServiceType::LocationUpdateRequest);
    runner.feed(cmReq, session, noopSink);

    // Advance through IDENTITY_CHECK -> AUTH_CHECK -> LU_REQUEST -> WAITING_EXTERNAL
    ParsedMessage mmMsg{MMM{L3MMStatus{}}};
    runner.feed(mmMsg, session, noopSink);  // IDENTITY_CHECK -> AUTH_CHECK
    runner.feed(mmMsg, session, noopSink);  // AUTH_CHECK -> LU_REQUEST
    runner.feed(mmMsg, session, noopSink);  // LU_REQUEST -> WAITING_EXTERNAL + T3103

    EXPECT_EQ(runner.activeCount(), 2u);

    // Tick 4s: ChannelAssignment timer (3s) expires, LocationUpdate timer (5s) still running
    size_t failed1 = runner.tickAll(4000ms);
    EXPECT_GE(failed1, 1u);  // At least ChannelAssignment should have expired
    EXPECT_EQ(runner.activeCount(), 1u);  // Only LocationUpdate remains

    // Tick another 4s: LocationUpdate timer (5s total) also expires
    size_t failed2 = runner.tickAll(4000ms);
    EXPECT_GE(failed2, 1u);  // LocationUpdate should expire
    EXPECT_EQ(runner.activeCount(), 0u);  // All slots freed
}

// Test: ChannelAssignment procedure completes first, freeing its slot, then a Setup
// message triggers creation of a new CallSetupMO procedure in the freed slot.
// Validates slot reuse and sequential procedure chaining.
// 3GPP: TS 04.08 9.1.2 (Channel Assignment) followed by TS 24.008 6.1 (Call Setup).
TEST(Integration, ChannelAssignment_Then_CallSetup) {
    SubscriberRegistry registry;
    auto* session = registry.createByTMSI(0xABCDEF01u);
    ASSERT_NE(session, nullptr);

    ProcedureRunner& runner = session->procedures;

    // Phase 1: ChannelAssignment procedure
    // Feed ChannelRequest -> INIT -> ALLOCATE_CHANNEL
    ParsedMessage chReq{RRM{L3ChannelRequest{0}}};
    runner.feed(chReq, session, noopSink);
    EXPECT_EQ(runner.activeCount(), 1u);

    // Feed RR msg -> ALLOCATE_CHANNEL -> SEND_IMMEDIATE_ASSIGNMENT + timer
    ParsedMessage rrMsg{RRM{L3ClassmarkChange{}}};
    runner.feed(rrMsg, session, noopSink);

    // Feed RR msg -> SEND_IMMEDIATE_ASSIGNMENT -> WAIT_SEIZURE
    runner.feed(rrMsg, session, noopSink);

    // Feed RR msg -> WAIT_SEIZURE -> COMPLETED (auto-cleaned)
    auto result = runner.feed(rrMsg, session, noopSink);
    EXPECT_EQ(result.action, ProcedureStepResult::Action::Completed);
    EXPECT_EQ(runner.activeCount(), 0u);

    // Phase 2: Setup triggers CallSetupMO in the now-free slot
    ParsedMessage setupMsg{CCM{L3Setup{}}};
    runner.feed(setupMsg, session, noopSink);
    EXPECT_EQ(runner.activeCount(), 1u);
    EXPECT_NE(runner.getActive(procedure::ProcedureType::CallSetup_MO), nullptr);

    // Verify no leftover ChannelAssignment
    EXPECT_EQ(runner.getActive(procedure::ProcedureType::ChannelAssignment), nullptr);
}
