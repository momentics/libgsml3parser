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

/// Unit tests for CallSetupMOPercedure (TS 24.008 6.1).
/// Validates MO call setup state machine: CMServiceRequest through ConnectAcknowledge.

#include <gtest/gtest.h>
#include <gsml3parser/stack/procedures/call_setup_mo.h>
#include <gsml3parser/message_types.h>
#include <gsml3parser/mm/l3mmmessages.h>
#include <gsml3parser/cc/l3ccmessages.h>
#include <gsml3parser/rr/l3rrmessages.h>
#include <gsml3parser/visitor.h>

#include <chrono>
#include <cstdint>

using namespace gsml3parser;
using namespace std::chrono_literals;

/// Discard feed() result to suppress [[nodiscard]] warning on intermediate steps.
inline void feedStep(Procedure& p, const ParsedMessage& msg) {
    [[maybe_unused]] auto r = p.feed(msg, nullptr, nullptr);
}

// ── Helpers ────────────────────────────────────────────────────────────────

static ParsedMessage makeCMServiceRequest() {
    return ParsedMessage{MMM{L3CMServiceRequest::builder().build()}};
}

static ParsedMessage makeSetup() {
    return ParsedMessage{CCM{L3Setup::builder().ti(3).build()}};
}

static ParsedMessage makeAssignmentComplete() {
    return ParsedMessage{RRM{L3AssignmentComplete::builder().build()}};
}

static ParsedMessage makeConnectAcknowledge() {
    return ParsedMessage{CCM{L3ConnectAcknowledge::builder().build()}};
}

// Dummy message to advance intermediate states that don't inspect content.
static ParsedMessage makeDummyMsg() {
    return ParsedMessage{RRM{L3ChannelRequest::builder().build()}};
}

// ── Tests ──────────────────────────────────────────────────────────────────

// [TS 24.008 6.1] CMServiceRequest in INIT triggers SendResponse (CMServiceAccept).
TEST(CallSetupMOPercedureTest, MOC_Init_CMServiceRequest_SendsAccept) {
    CallSetupMOPercedure proc;

    bool sinkCalled = false;
    auto res = proc.feed(makeCMServiceRequest(), nullptr,
        [&sinkCalled](SMAction action, const ParsedMessage&, const SubscriberSession*) {
            EXPECT_EQ(action, SMAction::SendResponse);
            sinkCalled = true;
        });

    EXPECT_EQ(res.action, ProcedureStepResult::Action::SendResponse);
    EXPECT_TRUE(sinkCalled);
    EXPECT_EQ(proc.state(), procedure::ProcedureState::InProgress);
}

// [TS 24.008 6.1] Setup message in WAIT_SETUP advances to PROCEEDING with SendResponse.
TEST(CallSetupMOPercedureTest, MOC_WaitSetup_Setup_Proceeding) {
    CallSetupMOPercedure proc;

    // INIT -> SERVICE_ACCEPT
    feedStep(proc, makeCMServiceRequest());
    // SERVICE_ACCEPT -> WAIT_SETUP
    feedStep(proc, makeDummyMsg());

    bool sinkCalled = false;
    auto res = proc.feed(makeSetup(), nullptr,
        [&sinkCalled](SMAction, const ParsedMessage&, const SubscriberSession*) {
            sinkCalled = true;
        });

    EXPECT_EQ(res.action, ProcedureStepResult::Action::SendResponse);
    EXPECT_TRUE(sinkCalled);
    EXPECT_EQ(proc.state(), procedure::ProcedureState::InProgress);
}

// [TS 24.008 6.1] PROCEEDING state advances to ASSIGN_TCH on next feed.
TEST(CallSetupMOPercedureTest, MOC_Proceeding_AssignTCH) {
    CallSetupMOPercedure proc;

    feedStep(proc, makeCMServiceRequest());
    feedStep(proc, makeDummyMsg());
    feedStep(proc, makeSetup());

    // PROCEEDING -> ASSIGN_TCH
    bool sinkCalled = false;
    auto res = proc.feed(makeDummyMsg(), nullptr,
        [&sinkCalled](SMAction, const ParsedMessage&, const SubscriberSession*) {
            sinkCalled = true;
        });

    EXPECT_EQ(res.action, ProcedureStepResult::Action::SendResponse);
    EXPECT_TRUE(sinkCalled);
}

// [TS 24.008 6.1] AssignmentComplete in WAIT_ASSIGN_COMPLETE advances to ALERTING.
TEST(CallSetupMOPercedureTest, MOC_AssignComplete_Alerting) {
    CallSetupMOPercedure proc;

    feedStep(proc, makeCMServiceRequest());
    feedStep(proc, makeDummyMsg());
    feedStep(proc, makeSetup());
    feedStep(proc, makeDummyMsg());
    feedStep(proc, makeDummyMsg());

    // WAIT_ASSIGN_COMPLETE -> ALERTING
    bool sinkCalled = false;
    auto res = proc.feed(makeAssignmentComplete(), nullptr,
        [&sinkCalled](SMAction, const ParsedMessage&, const SubscriberSession*) {
            sinkCalled = true;
        });

    EXPECT_EQ(res.action, ProcedureStepResult::Action::SendResponse);
    EXPECT_TRUE(sinkCalled);
}

// [TS 24.008 6.1] ConnectAcknowledge in ACTIVE state completes the procedure.
TEST(CallSetupMOPercedureTest, MOC_ConnectAck_Completes) {
    CallSetupMOPercedure proc;

    feedStep(proc, makeCMServiceRequest());
    feedStep(proc, makeDummyMsg());
    feedStep(proc, makeSetup());
    feedStep(proc, makeDummyMsg());
    feedStep(proc, makeDummyMsg());
    feedStep(proc, makeAssignmentComplete());
    feedStep(proc, makeDummyMsg());
    feedStep(proc, makeDummyMsg());
    feedStep(proc, makeDummyMsg());

    // ACTIVE -> COMPLETED on ConnectAcknowledge
    auto res = proc.feed(makeConnectAcknowledge(), nullptr, nullptr);

    EXPECT_EQ(res.action, ProcedureStepResult::Action::Completed);
    EXPECT_EQ(proc.state(), procedure::ProcedureState::Completed);
}

// [TS 24.008 6.1] Full MO call flow from CMServiceRequest to ConnectAcknowledge.
TEST(CallSetupMOPercedureTest, MOC_FullFlow_Completes) {
    CallSetupMOPercedure proc;

    // 1. INIT -> SERVICE_ACCEPT (CMServiceRequest)
    {
        auto r = proc.feed(makeCMServiceRequest(), nullptr, nullptr);
        EXPECT_EQ(r.action, ProcedureStepResult::Action::SendResponse);
    }

    // 2. SERVICE_ACCEPT -> WAIT_SETUP
    {
        auto r = proc.feed(makeDummyMsg(), nullptr, nullptr);
        EXPECT_EQ(r.action, ProcedureStepResult::Action::Continue);
    }

    // 3. WAIT_SETUP -> PROCEEDING (Setup)
    {
        auto r = proc.feed(makeSetup(), nullptr, nullptr);
        EXPECT_EQ(r.action, ProcedureStepResult::Action::SendResponse);
    }

    // 4. PROCEEDING -> ASSIGN_TCH
    {
        auto r = proc.feed(makeDummyMsg(), nullptr, nullptr);
        EXPECT_EQ(r.action, ProcedureStepResult::Action::SendResponse);
    }

    // 5. ASSIGN_TCH -> WAIT_ASSIGN_COMPLETE
    {
        auto r = proc.feed(makeDummyMsg(), nullptr, nullptr);
        EXPECT_EQ(r.action, ProcedureStepResult::Action::Continue);
    }

    // 6. WAIT_ASSIGN_COMPLETE -> ALERTING (AssignmentComplete)
    {
        auto r = proc.feed(makeAssignmentComplete(), nullptr, nullptr);
        EXPECT_EQ(r.action, ProcedureStepResult::Action::SendResponse);
    }

    // 7. ALERTING -> CONNECT
    {
        auto r = proc.feed(makeDummyMsg(), nullptr, nullptr);
        EXPECT_EQ(r.action, ProcedureStepResult::Action::SendResponse);
    }

    // 8. CONNECT -> ACTIVE
    {
        auto r = proc.feed(makeDummyMsg(), nullptr, nullptr);
        EXPECT_EQ(r.action, ProcedureStepResult::Action::SendResponse);
    }

    // 9. ACTIVE -> (advance to next feed)
    {
        auto r = proc.feed(makeDummyMsg(), nullptr, nullptr);
        EXPECT_EQ(r.action, ProcedureStepResult::Action::Continue);
    }

    // 10. ACTIVE -> COMPLETED (ConnectAcknowledge)
    {
        auto r = proc.feed(makeConnectAcknowledge(), nullptr, nullptr);
        EXPECT_EQ(r.action, ProcedureStepResult::Action::Completed);
    }

    EXPECT_EQ(proc.state(), procedure::ProcedureState::Completed);
    EXPECT_EQ(proc.type(), procedure::ProcedureType::CallSetup_MO);
}

// [TS 24.008 6.1] T3101 timer expiry during call setup causes failure.
TEST(CallSetupMOPercedureTest, MOC_Tick_TimerExpired_Fails) {
    CallSetupMOPercedure proc;

    feedStep(proc, makeCMServiceRequest());
    feedStep(proc, makeDummyMsg());

    // Setup triggers T3101 start (3000ms).
    feedStep(proc, makeSetup());

    // Tick past timer expiry.
    auto res = proc.tick(4000ms);

    EXPECT_EQ(res.action, ProcedureStepResult::Action::Failed);
    EXPECT_EQ(proc.state(), procedure::ProcedureState::Failed);
}

// [TS 24.008 6.1] cancel() aborts the MO call setup and sets Failed state.
TEST(CallSetupMOPercedureTest, MOC_Cancel_Aborts) {
    CallSetupMOPercedure proc;

    feedStep(proc, makeCMServiceRequest());
    proc.cancel();

    EXPECT_EQ(proc.state(), procedure::ProcedureState::Failed);

    // Ticks after cancel are harmless.
    auto res = proc.tick(1000ms);
    EXPECT_EQ(res.action, ProcedureStepResult::Action::Continue);
}
