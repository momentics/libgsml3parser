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

/// Unit tests for ChannelAssignmentProcedure (TS 04.08 9.1.2 / 9.1.35).
/// Validates channel allocation, ImmediateAssignment, and seizure detection.

#include <gtest/gtest.h>
#include <gsml3parser/stack/procedures/channel_assignment.h>
#include <gsml3parser/message_types.h>
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

static ParsedMessage makeChannelRequest() {
    return ParsedMessage{RRM{L3ChannelRequest::builder().build()}};
}

static ParsedMessage makePagingResponse() {
    return ParsedMessage{RRM{L3PagingResponse::builder().build()}};
}

static ParsedMessage makeDummyMsg() {
    return ParsedMessage{RRM{L3MeasurementReport::builder().build()}};
}

// ── Tests ──────────────────────────────────────────────────────────────────

// [TS 04.08 9.1.2] ChannelRequest in INIT advances to ALLOCATE_CHANNEL.
TEST(ChannelAssignmentProcedureTest, CAP_Init_ChannelRequest_Allocates) {
    ChannelAssignmentProcedure proc(ChannelType::SDCCHType);

    EXPECT_EQ(proc.targetChannelType(), ChannelType::SDCCHType);
    EXPECT_EQ(proc.type(), procedure::ProcedureType::ChannelAssignment);

    auto res = proc.feed(makeChannelRequest(), nullptr, nullptr);

    EXPECT_EQ(res.action, ProcedureStepResult::Action::Continue);
    EXPECT_EQ(proc.state(), procedure::ProcedureState::InProgress);
}

// [TS 04.08 9.1.35] ALLOCATE_CHANNEL -> SEND_IMMEDIATE_ASSIGNMENT sends ImmediateAssignment and waits for seizure.
TEST(ChannelAssignmentProcedureTest, CAP_SendImmediateAssignment_WaitSeizure) {
    ChannelAssignmentProcedure proc(ChannelType::TCHFType);

    feedStep(proc, makeChannelRequest());

    bool sinkCalled = false;
    auto res = proc.feed(makeDummyMsg(), nullptr,
        [&sinkCalled](SMAction, const ParsedMessage&, const SubscriberSession*) {
            sinkCalled = true;
        });

    EXPECT_EQ(res.action, ProcedureStepResult::Action::SendResponse);
    EXPECT_TRUE(sinkCalled);

    // SEND_IMMEDIATE_ASSIGNMENT -> WAIT_SEIZURE
    res = proc.feed(makeDummyMsg(), nullptr, nullptr);
    EXPECT_EQ(res.action, ProcedureStepResult::Action::Continue);
}

// [TS 04.08 9.1.2] Any L3 message in WAIT_SEIZURE completes the procedure (channel seized).
TEST(ChannelAssignmentProcedureTest, CAP_Seizure_Completes) {
    ChannelAssignmentProcedure proc(ChannelType::SDCCHType);

    feedStep(proc, makeChannelRequest());
    feedStep(proc, makeDummyMsg());
    feedStep(proc, makeDummyMsg());

    // Any message on new channel indicates seizure.
    auto res = proc.feed(makeDummyMsg(), nullptr, nullptr);

    EXPECT_EQ(res.action, ProcedureStepResult::Action::Completed);
    EXPECT_EQ(proc.state(), procedure::ProcedureState::Completed);
}

// [TS 04.08 9.1.2] T3101 timer expiry during WAIT_SEIZURE causes failure.
TEST(ChannelAssignmentProcedureTest, CAP_Tick_TimerExpired_Fails) {
    ChannelAssignmentProcedure proc(ChannelType::SDCCHType);

    feedStep(proc, makeChannelRequest());
    feedStep(proc, makeDummyMsg());
    feedStep(proc, makeDummyMsg());

    // Timer is running (3000ms). Tick past expiry.
    auto res = proc.tick(4000ms);

    EXPECT_EQ(res.action, ProcedureStepResult::Action::Failed);
    EXPECT_EQ(proc.state(), procedure::ProcedureState::Failed);
}

// [TS 04.08 9.1.2] cancel() aborts the channel assignment and sets Failed state.
TEST(ChannelAssignmentProcedureTest, CAP_Cancel_Aborts) {
    ChannelAssignmentProcedure proc(ChannelType::TCHHType);

    feedStep(proc, makeChannelRequest());
    proc.cancel();

    EXPECT_EQ(proc.state(), procedure::ProcedureState::Failed);

    auto res = proc.tick(1000ms);
    EXPECT_EQ(res.action, ProcedureStepResult::Action::Continue);
}

// [TS 04.08 9.1.2] PagingResponse also triggers channel assignment from INIT state.
TEST(ChannelAssignmentProcedureTest, CAP_PagingResponse_Allocates) {
    ChannelAssignmentProcedure proc(ChannelType::SDCCHType);

    auto res = proc.feed(makePagingResponse(), nullptr, nullptr);

    EXPECT_EQ(res.action, ProcedureStepResult::Action::Continue);
    EXPECT_EQ(proc.state(), procedure::ProcedureState::InProgress);

    // Advance through ALLOCATE_CHANNEL and SEND_IMMEDIATE_ASSIGNMENT.
    feedStep(proc, makeDummyMsg());
    feedStep(proc, makeDummyMsg());

    // Seizure completes.
    auto res2 = proc.feed(makeDummyMsg(), nullptr, nullptr);
    EXPECT_EQ(res2.action, ProcedureStepResult::Action::Completed);
}
