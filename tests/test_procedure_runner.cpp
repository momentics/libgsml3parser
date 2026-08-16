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

// Tests for ProcedureRunner and ProcedureFactory: validates procedure auto-creation,
// message routing by PD, timer management, cancel semantics, active count tracking,
// external data routing, and ResponseSink callback invocation.
// 3GPP coverage: TS 24.008 chapters 4.x (MM procedures), TS 04.08 chapter 9 (RR procedures).

#include <gtest/gtest.h>
#include "gsml3parser/stack/procedure_runner.h"
#include "gsml3parser/stack/subscriber_registry.h"
#include "gsml3parser/mm/l3mmmessages.h"
#include "gsml3parser/rr/l3rrmessages.h"
#include "gsml3parser/cc/l3ccmessages.h"

#include <chrono>
#include <array>
#include <atomic>

using namespace gsml3parser;
using namespace std::chrono_literals;

// Test: Every ProcedureFactory method returns a non-null unique_ptr.
// Importance: Factory is the sole entry point for creating managed procedures;
// null returns would cause ProcedureRunner to silently drop procedures.
// 3GPP: TS 24.008 procedure types form a closed, well-defined set.
TEST(PR_Factory, AllProcedures_Created) {
    auto lu = ProcedureFactory::createLocationUpdate();
    ASSERT_NE(lu, nullptr);
    EXPECT_EQ(lu->type(), procedure::ProcedureType::LocationUpdate);

    auto auth = ProcedureFactory::createAuthentication();
    ASSERT_NE(auth, nullptr);
    EXPECT_EQ(auth->type(), procedure::ProcedureType::Authentication);

    auto cipher = ProcedureFactory::createCipheringMode(0);
    ASSERT_NE(cipher, nullptr);
    EXPECT_EQ(cipher->type(), procedure::ProcedureType::CipheringMode);

    auto moc = ProcedureFactory::createCallSetupMO();
    ASSERT_NE(moc, nullptr);
    EXPECT_EQ(moc->type(), procedure::ProcedureType::CallSetup_MO);

    auto mtc = ProcedureFactory::createCallSetupMT("12345");
    ASSERT_NE(mtc, nullptr);
    EXPECT_EQ(mtc->type(), procedure::ProcedureType::CallSetup_MT);

    auto chAssign = ProcedureFactory::createChannelAssignment(ChannelType::SDCCHType);
    ASSERT_NE(chAssign, nullptr);
    EXPECT_EQ(chAssign->type(), procedure::ProcedureType::ChannelAssignment);

    auto paging = ProcedureFactory::createPaging(L3MobileIdentity{0x12345678u});
    ASSERT_NE(paging, nullptr);
    EXPECT_EQ(paging->type(), procedure::ProcedureType::Paging);

    auto ho = ProcedureFactory::createHandover(L3ChannelDescription{});
    ASSERT_NE(ho, nullptr);
    EXPECT_EQ(ho->type(), procedure::ProcedureType::Handover);
}

// Test: Feeding a ChannelRequest message auto-creates a ChannelAssignment procedure.
// Importance: The RR channel request is the trigger for channel assignment; the runner
// must detect the message type and instantiate the correct procedure automatically.
// 3GPP: TS 04.08 9.1.2 - Assignment procedure triggered by Channel Request.
TEST(PR_Feed, ChannelRequest_CreatesChannelAssignment) {
    ProcedureRunner runner;
    SubscriberSession session;

    ParsedMessage msg{RRM{L3ChannelRequest{0}}};
    auto result = runner.feed(msg, &session, {});

    EXPECT_EQ(result.action, ProcedureStepResult::Action::Continue);
    EXPECT_NE(runner.getActive(procedure::ProcedureType::ChannelAssignment), nullptr);
    EXPECT_EQ(runner.activeCount(), 1u);
}

// Test: Feeding a CMServiceRequest with LocationUpdateRequest type auto-creates
// a LocationUpdate procedure.
// Importance: Location update is the most common MM procedure; the runner must
// inspect the CM service type field to decide which procedure to create.
// 3GPP: TS 24.008 4.4.1 - Normal location updating procedure.
TEST(PR_Feed, CMServiceRequest_LU_CreatesLocationUpdate) {
    ProcedureRunner runner;
    SubscriberSession session;

    auto cmReq = L3CMServiceRequest::builder()
        .serviceType(L3CMServiceType{L3CMServiceType::LocationUpdateRequest})
        .build();
    ParsedMessage msg{MMM{std::move(cmReq)}};
    auto result = runner.feed(msg, &session, {});

    EXPECT_EQ(result.action, ProcedureStepResult::Action::Continue);
    EXPECT_NE(runner.getActive(procedure::ProcedureType::LocationUpdate), nullptr);
    EXPECT_EQ(runner.activeCount(), 1u);
}

// Test: Feeding a Setup message auto-creates a CallSetupMO procedure.
// Importance: The CC Setup message initiates mobile-originated calls; the runner
// must recognize the CC PD and Setup MTI to create the correct procedure.
// 3GPP: TS 24.008 6.1 - Mobile Originated Call establishment.
TEST(PR_Feed, Setup_CreatesCallSetupMO) {
    ProcedureRunner runner;
    SubscriberSession session;

    ParsedMessage msg{CCM{L3Setup{}}};
    auto result = runner.feed(msg, &session, {});

    EXPECT_EQ(result.action, ProcedureStepResult::Action::SendResponse);
    EXPECT_NE(runner.getActive(procedure::ProcedureType::CallSetup_MO), nullptr);
    EXPECT_EQ(runner.activeCount(), 1u);
}

// Test: A second message with the same PD routes to the existing active procedure
// rather than creating a duplicate.
// Importance: ProcedureRunner must maintain exactly one active procedure per PD;
// duplicate creation would waste slots and cause state machine confusion.
// 3GPP: TS 04.08 - Single assignment procedure per logical channel.
TEST(PR_Feed, RoutesToActiveProcedure) {
    ProcedureRunner runner;
    SubscriberSession session;

    // First feed creates ChannelAssignment
    ParsedMessage msg1{RRM{L3ChannelRequest{0}}};
    runner.feed(msg1, &session, {});
    EXPECT_EQ(runner.activeCount(), 1u);

    auto* firstProc = runner.getActive(procedure::ProcedureType::ChannelAssignment);
    ASSERT_NE(firstProc, nullptr);

    // Second feed with same PD routes to existing procedure
    ParsedMessage msg2{RRM{L3ClassmarkChange{}}};
    auto result = runner.feed(msg2, &session, {});

    EXPECT_EQ(runner.activeCount(), 1u);
    EXPECT_EQ(runner.getActive(procedure::ProcedureType::ChannelAssignment), firstProc);
    // Second feed advances from ALLOCATE_CHANNEL to SEND_IMMEDIATE_ASSIGNMENT
    EXPECT_EQ(result.action, ProcedureStepResult::Action::SendResponse);
}

// Test: tickAll advances timers and returns the count of procedures that failed
// due to timeout.
// Importance: Timer management is critical for protocol compliance; expired procedures
// must be cleaned up automatically to free slots for new procedures.
// 3GPP: TS 24.008 - Protocol timers T3101, T3103, etc. with defined expiry durations.
TEST(PR_TickAll, ExpiresTimedOutProcedures) {
    ProcedureRunner runner;
    SubscriberSession session;

    // Feed ChannelRequest -> INIT->ALLOCATE_CHANNEL
    ParsedMessage msg1{RRM{L3ChannelRequest{0}}};
    runner.feed(msg1, &session, {});

    // Second feed -> ALLOCATE_CHANNEL->SEND_IMMEDIATE_ASSIGNMENT + timer T3101 (3s)
    ParsedMessage msg2{RRM{L3ClassmarkChange{}}};
    runner.feed(msg2, &session, {});

    EXPECT_EQ(runner.activeCount(), 1u);

    // Tick past the 3s timer -> should fail
    size_t failed = runner.tickAll(4000ms);
    EXPECT_GE(failed, 1u);
    EXPECT_EQ(runner.activeCount(), 0u);
}

// Test: cancelAll stops all active procedures and frees all slots.
// Importance: Emergency cancellation must release all resources; leftover active
// procedures would prevent new procedure creation on full slot arrays.
// 3GPP: TS 24.008 - Procedure cancellation on radio link failure or detach.
TEST(PR_CancelAll, StopsAllProcedures) {
    ProcedureRunner runner;
    SubscriberSession session;

    // Create a ChannelAssignment procedure
    ParsedMessage msg1{RRM{L3ChannelRequest{0}}};
    runner.feed(msg1, &session, {});
    EXPECT_EQ(runner.activeCount(), 1u);

    runner.cancelAll();
    EXPECT_EQ(runner.activeCount(), 0u);
    EXPECT_EQ(runner.getActive(procedure::ProcedureType::ChannelAssignment), nullptr);
}

// Test: activeCount accurately reflects the number of currently active procedures
// after creation, completion, and cancellation.
// Importance: BTS applications use activeCount for load monitoring and slot management.
// 3GPP: TS 24.008 - Maximum concurrent procedures per subscriber.
TEST(PR_ActiveCount, Accurate) {
    ProcedureRunner runner;
    SubscriberSession session;

    EXPECT_EQ(runner.activeCount(), 0u);

    // Create ChannelAssignment
    ParsedMessage msg1{RRM{L3ChannelRequest{0}}};
    runner.feed(msg1, &session, {});
    EXPECT_EQ(runner.activeCount(), 1u);

    // Advance ChannelAssignment to completion:
    // Feed 2 -> ALLOCATE_CHANNEL->SEND_IMMEDIATE_ASSIGNMENT
    ParsedMessage msg2{RRM{L3ClassmarkChange{}}};
    runner.feed(msg2, &session, {});
    EXPECT_EQ(runner.activeCount(), 1u);

    // Feed 3 -> SEND_IMMEDIATE_ASSIGNMENT->WAIT_SEIZURE
    ParsedMessage msg3{RRM{L3ClassmarkChange{}}};
    runner.feed(msg3, &session, {});
    EXPECT_EQ(runner.activeCount(), 1u);

    // Feed 4 -> WAIT_SEIZURE->COMPLETED (auto-cleaned)
    ParsedMessage msg4{RRM{L3ClassmarkChange{}}};
    runner.feed(msg4, &session, {});
    EXPECT_EQ(runner.activeCount(), 0u);
}

// Test: feedExternal by ProcedureType routes data to the correct active procedure.
// Importance: External data (RAND from AuC, VLR decisions) must reach the intended
// procedure; misrouting would cause authentication failures or incorrect responses.
// 3GPP: TS 24.008 4.4.2 - Authentication data fed externally to Location Update procedure.
TEST(PR_FeedExternal, RoutesToCorrectProcedure) {
    ProcedureRunner runner;
    SubscriberSession session;

    // Create a ChannelAssignment (RR PD)
    ParsedMessage msg1{RRM{L3ChannelRequest{0}}};
    runner.feed(msg1, &session, {});
    EXPECT_EQ(runner.activeCount(), 1u);

    // feedExternal for LocationUpdate type when no LU is active should return Continue
    std::array<uint8_t, 1> dummyData{0x00};
    auto result = runner.feedExternal(procedure::ProcedureType::LocationUpdate, dummyData);
    EXPECT_EQ(result.action, ProcedureStepResult::Action::Continue);

    // ChannelAssignment should still be active (wasn't affected)
    EXPECT_EQ(runner.activeCount(), 1u);
    EXPECT_NE(runner.getActive(procedure::ProcedureType::ChannelAssignment), nullptr);
}

// Test: ResponseSink callback is invoked when a procedure returns SendResponse action.
// Importance: The sink mechanism is how procedures trigger message generation without
// heap allocation; if it's not called, responses are silently dropped.
// 3GPP: TS 04.08 - Immediate Assignment response sent during channel assignment.
TEST(PR_ResponseSink, CalledOnSendResponse) {
    ProcedureRunner runner;
    SubscriberSession session;

    // Feed ChannelRequest -> INIT->ALLOCATE_CHANNEL (no response yet)
    ParsedMessage msg1{RRM{L3ChannelRequest{0}}};
    runner.feed(msg1, &session, {});

    // Track sink invocations
    std::atomic<bool> sinkCalled{false};
    auto sink = [&sinkCalled](SMAction action, const ParsedMessage& incomingMsg,
                              const SubscriberSession* sess) {
        (void)action;
        (void)incomingMsg;
        (void)sess;
        sinkCalled.store(true);
    };

    // Second feed -> ALLOCATE_CHANNEL->SEND_IMMEDIATE_ASSIGNMENT + SendResponse
    ParsedMessage msg2{RRM{L3ClassmarkChange{}}};
    auto result = runner.feed(msg2, &session, std::move(sink));

    EXPECT_EQ(result.action, ProcedureStepResult::Action::SendResponse);
    EXPECT_TRUE(sinkCalled.load());
}
