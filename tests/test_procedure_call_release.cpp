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

/// Unit tests for CallReleaseProcedure (TS 24.008 6.1).
/// Validates call release state machine: Disconnect -> Release -> ReleaseComplete.

#include <gtest/gtest.h>
#include <gsml3parser/stack/procedures/call_release.h>
#include <gsml3parser/message_types.h>
#include <gsml3parser/cc/l3ccmessages.h>
#include <gsml3parser/visitor.h>

#include <chrono>
#include <cstdint>

using namespace gsml3parser;
using namespace std::chrono_literals;

// ── Helpers ────────────────────────────────────────────────────────────────

static ParsedMessage makeDisconnect() {
    return ParsedMessage{CCM{L3Disconnect::builder().ti(3).build()}};
}

static ParsedMessage makeRelease() {
    return ParsedMessage{CCM{L3Release::builder().ti(3).build()}};
}

// ── Tests ──────────────────────────────────────────────────────────────────

TEST(CallReleaseProcedure, NormalFlow) {
    CallReleaseProcedure proc(3, CCCause::Normal_Call_Clearing);

    // Feed Disconnect message - triggers SEND_DISCONNECT -> WAIT_RELEASE
    auto r1 = proc.feed(makeDisconnect(), nullptr, {});
    EXPECT_EQ(r1.action, ProcedureStepResult::Action::SendResponseWithToken);
    EXPECT_EQ(r1.responseToken, ResponseToken::Disconnect);

    // Feed Release message - triggers SEND_RELEASE_COMPLETE -> COMPLETED.
    // Terminal with response: action stays SendResponseWithToken (rule in
    // procedure.h) so the caller still builds the ReleaseComplete; the terminal
    // state is reported via finalResult.
    auto r2 = proc.feed(makeRelease(), nullptr, {});
    EXPECT_EQ(r2.action, ProcedureStepResult::Action::SendResponseWithToken);
    EXPECT_EQ(r2.responseToken, ResponseToken::ReleaseComplete);
    EXPECT_EQ(r2.finalResult.state, procedure::ProcedureState::Completed);
    EXPECT_EQ(proc.state(), procedure::ProcedureState::Completed);
}

TEST(CallReleaseProcedure, TimerExpiry) {
    CallReleaseProcedure proc(3, CCCause::Normal_Call_Clearing);

    // Start the procedure
    [[maybe_unused]] auto r1 = proc.feed(makeDisconnect(), nullptr, {});

    // Tick beyond timer expiry (T3101 = 12s default, tick 15s)
    auto r2 = proc.tick(15s);
    EXPECT_EQ(r2.action, ProcedureStepResult::Action::Failed);
    EXPECT_EQ(proc.state(), procedure::ProcedureState::Failed);
}

TEST(CallReleaseProcedure, ResponseTokens) {
    CallReleaseProcedure proc(5, CCCause::User_Busy);

    // First step should produce Disconnect token
    auto r1 = proc.feed(makeDisconnect(), nullptr, {});
    EXPECT_EQ(r1.responseToken, ResponseToken::Disconnect);

    // Second step should produce ReleaseComplete token
    auto r2 = proc.feed(makeRelease(), nullptr, {});
    EXPECT_EQ(r2.responseToken, ResponseToken::ReleaseComplete);
}

TEST(CallReleaseProcedure, TypeAndState) {
    CallReleaseProcedure proc(0, CCCause::Normal_Call_Clearing);
    EXPECT_EQ(proc.type(), procedure::ProcedureType::CallRelease);
    EXPECT_EQ(proc.state(), procedure::ProcedureState::Initiated);
    EXPECT_EQ(proc.ti(), 0u);
    EXPECT_EQ(proc.cause(), CCCause::Normal_Call_Clearing);
}

TEST(CallReleaseProcedure, Cancel) {
    CallReleaseProcedure proc(3, CCCause::Normal_Call_Clearing);
    [[maybe_unused]] auto r1 = proc.feed(makeDisconnect(), nullptr, {});
    proc.cancel();
    EXPECT_EQ(proc.state(), procedure::ProcedureState::Failed);
}
