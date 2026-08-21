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

/// Unit tests for CipheringModeProcedure (TS 24.008 4.4.3 / TS 04.08 9.1.37).
/// Validates ciphering activation: feedExternal, CipheringModeCommand, CipheringModeComplete.

#include <gtest/gtest.h>
#include <gsml3parser/stack/procedures/ciphering_mode.h>
#include <gsml3parser/message_types.h>
#include <gsml3parser/rr/l3rrmessages.h>
#include <gsml3parser/visitor.h>
#include <gsml3parser/stack/typed_external_data.h>

#include <chrono>
#include <cstdint>

using namespace gsml3parser;
using namespace std::chrono_literals;

inline void feedStep(Procedure& p, const ParsedMessage& msg) {
    [[maybe_unused]] auto r = p.feed(msg, nullptr, ResponseSink{});
}

// ── Helpers ────────────────────────────────────────────────────────────────

static ParsedMessage makeCipheringModeComplete() {
    return ParsedMessage{RRM{L3CipheringModeComplete::builder().build()}};
}

static ParsedMessage makeDummyMsg() {
    return ParsedMessage{RRM{L3ChannelRequest::builder().build()}};
}

// ── Tests ──────────────────────────────────────────────────────────────────

// [TS 24.008 4.4.3] feedExternal from INIT triggers SendResponse (CipheringModeCommand).
TEST(CipheringModeProcedureTest, CMP_Init_FeedExternal_SendsCommand) {
    CipheringModeProcedure proc(1); // A5/1 algorithm

    EXPECT_EQ(proc.cipherAlgo(), static_cast<uint8_t>(1));
    EXPECT_EQ(proc.type(), procedure::ProcedureType::CipheringMode);
    EXPECT_EQ(proc.state(), procedure::ProcedureState::Initiated);

    bool sinkCalled = false;
    auto res = proc.feedExternalTyped(CipheringParameters{}, nullptr,
        makeResponseSink([&sinkCalled](SMAction action, const ParsedMessage&, const SubscriberSession*) {
            EXPECT_EQ(action, SMAction::SendResponse);
            sinkCalled = true;
        }));

    EXPECT_EQ(res.action, ProcedureStepResult::Action::SendResponseWithToken);
    EXPECT_TRUE(sinkCalled);
    EXPECT_EQ(proc.state(), procedure::ProcedureState::InProgress);
}

// [TS 24.008 4.4.3] CipheringModeComplete in WAIT_COMPLETE completes the procedure.
TEST(CipheringModeProcedureTest, CMP_CipheringModeComplete_Completes) {
    CipheringModeProcedure proc(2); // A5/2 algorithm

    [[maybe_unused]] auto _r = proc.feedExternalTyped(CipheringParameters{}, nullptr);
    feedStep(proc, makeDummyMsg());

    auto res = proc.feed(makeCipheringModeComplete(), nullptr, ResponseSink{});

    EXPECT_EQ(res.action, ProcedureStepResult::Action::Completed);
    EXPECT_EQ(proc.state(), procedure::ProcedureState::Completed);
    EXPECT_EQ(res.finalResult.type, procedure::ProcedureType::CipheringMode);
}

// [TS 24.008 4.4.3] T3101 timer expiry during ciphering causes failure.
TEST(CipheringModeProcedureTest, CMP_Tick_TimerExpired_Fails) {
    CipheringModeProcedure proc(3); // A5/3 (GEA/1) algorithm

    [[maybe_unused]] auto _r = proc.feedExternalTyped(CipheringParameters{}, nullptr);
    feedStep(proc, makeDummyMsg());

    // Timer is running at 3000ms. Tick past expiry.
    auto res = proc.tick(4000ms);

    EXPECT_EQ(res.action, ProcedureStepResult::Action::Failed);
    EXPECT_EQ(proc.state(), procedure::ProcedureState::Failed);
}

// [TS 24.008 4.4.3] cancel() aborts the ciphering mode procedure and sets Failed state.
TEST(CipheringModeProcedureTest, CMP_Cancel_Aborts) {
    CipheringModeProcedure proc(0); // No ciphering (A5/0)

    [[maybe_unused]] auto _r = proc.feedExternalTyped(CipheringParameters{}, nullptr);
    proc.cancel();

    EXPECT_EQ(proc.state(), procedure::ProcedureState::Failed);

    auto res = proc.tick(1000ms);
    EXPECT_EQ(res.action, ProcedureStepResult::Action::Continue);
}
