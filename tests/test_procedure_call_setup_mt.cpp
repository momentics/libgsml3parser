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

/// Unit tests for CallSetupMTPercedure (TS 24.008 6.1).
/// Validates MT call setup: paging, PagingResponse, Setup delivery, ConnectAcknowledge.

#include <gtest/gtest.h>
#include <gsml3parser/stack/procedures/call_setup_mt.h>
#include <gsml3parser/message_types.h>
#include <gsml3parser/mm/l3mmmessages.h>
#include <gsml3parser/cc/l3ccmessages.h>
#include <gsml3parser/rr/l3rrmessages.h>
#include <gsml3parser/visitor.h>
#include <gsml3parser/stack/typed_external_data.h>

#include <chrono>
#include <cstdint>

using namespace gsml3parser;
using namespace std::chrono_literals;

inline void feedStep(Procedure& p, const ParsedMessage& msg) {
    [[maybe_unused]] auto r = p.feed(msg, nullptr, nullptr);
}

// ── Helpers ────────────────────────────────────────────────────────────────

static ParsedMessage makePagingResponse() {
    return ParsedMessage{RRM{L3PagingResponse::builder().build()}};
}

static ParsedMessage makeCallConfirmed() {
    return ParsedMessage{CCM{L3CallConfirmed::builder().build()}};
}

static ParsedMessage makeAssignmentComplete() {
    return ParsedMessage{RRM{L3AssignmentComplete::builder().build()}};
}

static ParsedMessage makeConnectAcknowledge() {
    return ParsedMessage{CCM{L3ConnectAcknowledge::builder().build()}};
}

static ParsedMessage makeDummyMsg() {
    return ParsedMessage{RRM{L3ChannelRequest::builder().build()}};
}

// ── Tests ──────────────────────────────────────────────────────────────────

// [TS 24.008 6.1] feedExternal triggers paging SendResponse from INIT state.
TEST(CallSetupMTPercedureTest, MTC_Init_FeedExternal_StartsPaging) {
    CallSetupMTPercedure proc("1234567890");

    EXPECT_EQ(proc.calledNumber(), "1234567890");
    EXPECT_EQ(proc.type(), procedure::ProcedureType::CallSetup_MT);

    bool sinkCalled = false;
    auto res = proc.feedExternalTyped(PagingTrigger{},
        [&sinkCalled](SMAction action, const ParsedMessage&, const SubscriberSession*) {
            EXPECT_EQ(action, SMAction::SendResponse);
            sinkCalled = true;
        });

    EXPECT_EQ(res.action, ProcedureStepResult::Action::SendResponseWithToken);
    EXPECT_TRUE(sinkCalled);
    EXPECT_EQ(proc.state(), procedure::ProcedureState::InProgress);
}

// [TS 24.008 6.1] PagingResponse in WAIT_PAGE_RESPONSE advances to ASSIGN_SDCCH.
TEST(CallSetupMTPercedureTest, MTC_PageResponse_AssignSDCCH) {
    CallSetupMTPercedure proc("9876543210");

    [[maybe_unused]] auto _r = proc.feedExternalTyped(PagingTrigger{});
    feedStep(proc, makeDummyMsg());

    bool sinkCalled = false;
    auto res = proc.feed(makePagingResponse(), nullptr,
        [&sinkCalled](SMAction, const ParsedMessage&, const SubscriberSession*) {
            sinkCalled = true;
        });

    EXPECT_EQ(res.action, ProcedureStepResult::Action::SendResponseWithToken);
    EXPECT_TRUE(sinkCalled);
}

// [TS 24.008 6.1] CallConfirmed in WAIT_CONFIRMED advances to ASSIGN_TCH.
TEST(CallSetupMTPercedureTest, MTC_CallConfirmed_AssignTCH) {
    CallSetupMTPercedure proc("5555555555");

    [[maybe_unused]] auto _r = proc.feedExternalTyped(PagingTrigger{});
    feedStep(proc, makeDummyMsg());
    feedStep(proc, makePagingResponse());
    feedStep(proc, makeDummyMsg());
    feedStep(proc, makeDummyMsg());

    auto res = proc.feed(makeCallConfirmed(), nullptr, nullptr);

    EXPECT_EQ(res.action, ProcedureStepResult::Action::Continue);
}

// [TS 24.008 6.1] ConnectAcknowledge in ACTIVE state completes the MT call setup.
TEST(CallSetupMTPercedureTest, MTC_ConnectAck_Completes) {
    CallSetupMTPercedure proc("1111111111");

    [[maybe_unused]] auto _r = proc.feedExternalTyped(PagingTrigger{});
    feedStep(proc, makeDummyMsg());
    feedStep(proc, makePagingResponse());
    feedStep(proc, makeDummyMsg());
    feedStep(proc, makeDummyMsg());
    feedStep(proc, makeCallConfirmed());
    feedStep(proc, makeDummyMsg());
    feedStep(proc, makeDummyMsg());
    feedStep(proc, makeAssignmentComplete());
    feedStep(proc, makeDummyMsg());
    feedStep(proc, makeDummyMsg());
    feedStep(proc, makeDummyMsg());

    auto res = proc.feed(makeConnectAcknowledge(), nullptr, nullptr);

    EXPECT_EQ(res.action, ProcedureStepResult::Action::Completed);
    EXPECT_EQ(proc.state(), procedure::ProcedureState::Completed);
}

// [TS 24.008 6.1] T3109 expiry during WAIT_PAGE_RESPONSE retries paging up to MAX_PAGE_ATTEMPTS.
TEST(CallSetupMTPercedureTest, MTC_Tick_PagingRetry_Retries) {
    CallSetupMTPercedure proc("2222222222");

    [[maybe_unused]] auto _r = proc.feedExternalTyped(PagingTrigger{});
    feedStep(proc, makeDummyMsg());

    // Now in WAIT_PAGE_RESPONSE with T3109 running (5000ms).
    // Tick past first expiry -> retry (attempt 2).
    auto r1 = proc.tick(6000ms);
    EXPECT_EQ(r1.action, ProcedureStepResult::Action::Continue);

    // Tick past second expiry -> retry (attempt 3).
    auto r2 = proc.tick(6000ms);
    EXPECT_EQ(r2.action, ProcedureStepResult::Action::Continue);

    // Tick past third expiry -> fail (exceeded MAX_PAGE_ATTEMPTS = 3).
    auto r3 = proc.tick(6000ms);
    EXPECT_EQ(r3.action, ProcedureStepResult::Action::Failed);
    EXPECT_EQ(proc.state(), procedure::ProcedureState::Failed);
}

// [TS 24.008 6.1] cancel() aborts the MT call setup and sets Failed state.
TEST(CallSetupMTPercedureTest, MTC_Cancel_Aborts) {
    CallSetupMTPercedure proc("3333333333");

    [[maybe_unused]] auto _r = proc.feedExternalTyped(PagingTrigger{});
    proc.cancel();

    EXPECT_EQ(proc.state(), procedure::ProcedureState::Failed);

    auto res = proc.tick(1000ms);
    EXPECT_EQ(res.action, ProcedureStepResult::Action::Continue);
}
