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

/// Unit tests for IMSIDetachProcedure (TS 24.008 4.4.6).
/// Validates IMSI detach flow: CMServiceAccept -> Detach complete.

#include <gtest/gtest.h>
#include <gsml3parser/stack/procedures/imsi_detach.h>
#include <gsml3parser/message_types.h>
#include <gsml3parser/mm/l3mmmessages.h>
#include <gsml3parser/visitor.h>

#include <chrono>
#include <cstdint>

using namespace gsml3parser;
using namespace std::chrono_literals;

// ── Helpers ────────────────────────────────────────────────────────────────

static ParsedMessage makeIMSIDetachIndication() {
    return ParsedMessage{MMM{L3IMSIDetachIndication::builder().build()}};
}

static ParsedMessage makeCMServiceAccept() {
    return ParsedMessage{MMM{L3CMServiceAccept::builder().build()}};
}

// ── Tests ──────────────────────────────────────────────────────────────────

TEST(IMSIDetachProcedure, NormalFlow) {
    IMSIDetachProcedure proc;

    // Feed IMSI Detach Indication - triggers SEND_CM_SERVICE_ACCEPT -> WAIT_DETACH_COMPLETE
    auto r1 = proc.feed(makeIMSIDetachIndication(), nullptr, {});
    EXPECT_EQ(r1.action, ProcedureStepResult::Action::SendResponseWithToken);
    EXPECT_EQ(r1.responseToken, ResponseToken::CMServiceAccept);

    // Feed any MM message to complete the detach
    auto r2 = proc.feed(makeCMServiceAccept(), nullptr, {});
    EXPECT_EQ(r2.action, ProcedureStepResult::Action::Completed);
    EXPECT_EQ(proc.state(), procedure::ProcedureState::Completed);
}

TEST(IMSIDetachProcedure, TimerExpiry) {
    IMSIDetachProcedure proc;

    // Start the procedure
    [[maybe_unused]] auto r1 = proc.feed(makeIMSIDetachIndication(), nullptr, {});

    // Tick beyond timer expiry (T3112 = 5s default, tick 6s)
    auto r2 = proc.tick(6s);
    EXPECT_EQ(r2.action, ProcedureStepResult::Action::Failed);
    EXPECT_EQ(proc.state(), procedure::ProcedureState::Failed);
}

TEST(IMSIDetachProcedure, TypeAndState) {
    IMSIDetachProcedure proc;
    EXPECT_EQ(proc.type(), procedure::ProcedureType::IMSIDetach);
    EXPECT_EQ(proc.state(), procedure::ProcedureState::Initiated);
}

TEST(IMSIDetachProcedure, Cancel) {
    IMSIDetachProcedure proc;
    [[maybe_unused]] auto r1 = proc.feed(makeIMSIDetachIndication(), nullptr, {});
    proc.cancel();
    EXPECT_EQ(proc.state(), procedure::ProcedureState::Failed);
}
