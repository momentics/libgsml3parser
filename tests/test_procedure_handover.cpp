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

/// Unit tests for HandoverProcedure (TS 04.08 9.1.40).
/// Validates handover: feedExternal, HandoverCommand, HandoverComplete/Failure.

#include <gtest/gtest.h>
#include <gsml3parser/stack/procedures/handover.h>
#include <gsml3parser/message_types.h>
#include <gsml3parser/common/l3common.h>
#include <gsml3parser/rr/l3rrmessages.h>
#include <gsml3parser/visitor.h>

#include <chrono>
#include <cstdint>

using namespace gsml3parser;
using namespace std::chrono_literals;

inline void feedStep(Procedure& p, const ParsedMessage& msg) {
    [[maybe_unused]] auto r = p.feed(msg, nullptr, nullptr);
}

// ── Helpers ────────────────────────────────────────────────────────────────

static ParsedMessage makeHandoverComplete() {
    return ParsedMessage{RRM{L3HandoverComplete::builder().build()}};
}

static ParsedMessage makeHandoverFailure() {
    return ParsedMessage{RRM{L3HandoverFailure::builder().build()}};
}

static ParsedMessage makeDummyMsg() {
    return ParsedMessage{RRM{L3ChannelRequest::builder().build()}};
}

static L3ChannelDescription makeTargetChannel() {
    return L3ChannelDescription(TDMA_TCHF, 0, 0, 150);
}

// ── Tests ──────────────────────────────────────────────────────────────────

// [TS 04.08 9.1.40] feedExternal from INIT triggers SendResponse (HandoverCommand).
TEST(HandoverProcedureTest, HO_Init_FeedExternal_SendsCommand) {
    HandoverProcedure proc(makeTargetChannel());

    EXPECT_EQ(proc.type(), procedure::ProcedureType::Handover);
    EXPECT_EQ(proc.state(), procedure::ProcedureState::Initiated);

    bool sinkCalled = false;
    auto res = proc.feedExternal({},
        [&sinkCalled](SMAction action, const ParsedMessage&, const SubscriberSession*) {
            EXPECT_EQ(action, SMAction::SendResponse);
            sinkCalled = true;
        });

    EXPECT_EQ(res.action, ProcedureStepResult::Action::SendResponse);
    EXPECT_TRUE(sinkCalled);
    EXPECT_EQ(proc.state(), procedure::ProcedureState::InProgress);
}

// [TS 04.08 9.1.40] HandoverComplete in WAIT_HO_COMPLETE completes the procedure.
TEST(HandoverProcedureTest, HO_HandoverComplete_Completes) {
    HandoverProcedure proc(makeTargetChannel());

    [[maybe_unused]] auto _r = proc.feedExternal({});
    feedStep(proc, makeDummyMsg());

    auto res = proc.feed(makeHandoverComplete(), nullptr, nullptr);

    EXPECT_EQ(res.action, ProcedureStepResult::Action::Completed);
    EXPECT_EQ(proc.state(), procedure::ProcedureState::Completed);
    EXPECT_EQ(res.finalResult.type, procedure::ProcedureType::Handover);
}

// [TS 04.08 9.1.40] HandoverFailure in WAIT_HO_COMPLETE fails the procedure.
TEST(HandoverProcedureTest, HO_HandoverFailure_Fails) {
    HandoverProcedure proc(makeTargetChannel());

    [[maybe_unused]] auto _r = proc.feedExternal({});
    feedStep(proc, makeDummyMsg());

    auto res = proc.feed(makeHandoverFailure(), nullptr, nullptr);

    EXPECT_EQ(res.action, ProcedureStepResult::Action::Failed);
    EXPECT_EQ(proc.state(), procedure::ProcedureState::Failed);
}

// [TS 04.08 9.1.40] cancel() aborts the handover and sets Failed state.
TEST(HandoverProcedureTest, HO_Cancel_Aborts) {
    HandoverProcedure proc(makeTargetChannel());

    [[maybe_unused]] auto _r = proc.feedExternal({});
    proc.cancel();

    EXPECT_EQ(proc.state(), procedure::ProcedureState::Failed);

    auto res = proc.tick(1000ms);
    EXPECT_EQ(res.action, ProcedureStepResult::Action::Continue);
}
