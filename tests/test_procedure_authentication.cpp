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

/// Unit tests for AuthenticationProcedure (TS 24.008 4.4.2).
/// Validates state machine transitions, SRES verification, timer expiry, and cancellation.

#include <gtest/gtest.h>
#include <gsml3parser/stack/procedures/authentication.h>
#include <gsml3parser/stack/typed_external_data.h>
#include <gsml3parser/message_types.h>
#include <gsml3parser/mm/l3mmmessages.h>
#include <gsml3parser/visitor.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>

using namespace gsml3parser;
using namespace std::chrono_literals;

inline void feedStep(Procedure& p, const ParsedMessage& msg) {
    [[maybe_unused]] auto r = p.feed(msg, nullptr, ResponseSink{});
}

// ── Helpers ────────────────────────────────────────────────────────────────

static ParsedMessage makeAuthResponse(uint32_t sres) {
    return ParsedMessage{MMM{L3AuthenticationResponse::builder().sres(sres).build()}};
}

// Build AuthChallenge: 16-byte RAND + 4-byte SRES
// SRES bytes are stored big-endian (octet 0 = MSB), matching the wire
// encoding of the SRES IE (TS 24.008 10.5.1.22).
static AuthChallenge makeAuthChallenge(uint32_t sres) {
    AuthChallenge chal{};
    for (int i = 0; i < 16; ++i) chal.rand[static_cast<size_t>(i)] = static_cast<uint8_t>(i);
    chal.expectedSres[0] = static_cast<uint8_t>((sres >> 24) & 0xFF);
    chal.expectedSres[1] = static_cast<uint8_t>((sres >> 16) & 0xFF);
    chal.expectedSres[2] = static_cast<uint8_t>((sres >> 8) & 0xFF);
    chal.expectedSres[3] = static_cast<uint8_t>(sres & 0xFF);
    return chal;
}

// ── Tests ──────────────────────────────────────────────────────────────────

// [TS 24.008 4.4.2] INIT state with no external data stays idle; feed() returns Continue.
TEST(AuthenticationProcedureTest, AuthP_Init_NoData_StaysInit) {
    AuthenticationProcedure proc;

    EXPECT_EQ(proc.type(), procedure::ProcedureType::Authentication);
    EXPECT_EQ(proc.state(), procedure::ProcedureState::Initiated);

    bool sinkCalled = false;
    auto res = proc.feed(makeAuthResponse(0x12345678), nullptr,
        makeResponseSink([&sinkCalled](SMAction, const ParsedMessage&, const SubscriberSession*) {
            sinkCalled = true;
        }));

    EXPECT_EQ(res.action, ProcedureStepResult::Action::Continue);
    EXPECT_FALSE(sinkCalled);
    EXPECT_EQ(proc.state(), procedure::ProcedureState::Initiated);
}

// [TS 24.008 4.4.2] feedExternalTyped with AuthChallenge triggers SendResponseWithToken (AuthenticationRequest).
TEST(AuthenticationProcedureTest, AuthP_FeedExternal_RAND_SendsAuthRequest) {
    AuthenticationProcedure proc;
    auto chal = makeAuthChallenge(0xDEADBEEF);

    bool sinkCalled = false;
    auto res = proc.feedExternalTyped(chal, nullptr,
        makeResponseSink([&sinkCalled](SMAction action, const ParsedMessage&, const SubscriberSession*) {
            EXPECT_EQ(action, SMAction::SendResponse);
            sinkCalled = true;
        }));

    EXPECT_EQ(res.action, ProcedureStepResult::Action::SendResponseWithToken);
    EXPECT_EQ(res.responseToken, ResponseToken::AuthenticationRequest);
    EXPECT_TRUE(sinkCalled);
    EXPECT_EQ(proc.state(), procedure::ProcedureState::InProgress);
}

// [TS 24.008 4.4.2] Correct SRES in AuthenticationResponse completes the procedure.
TEST(AuthenticationProcedureTest, AuthP_AuthResponse_ValidSRES_Completes) {
    AuthenticationProcedure proc;
    constexpr uint32_t kSRES = 0x12345678u;
    auto chal = makeAuthChallenge(kSRES);

    // Phase 1: feedExternalTyped to load RAND+SRES -> SEND_AUTH_REQ
    [[maybe_unused]] auto _r1 = proc.feedExternalTyped(chal, nullptr);

    // Phase 2: feed to advance SEND_AUTH_REQ -> WAIT_RESPONSE (starts T3106)
    feedStep(proc, makeAuthResponse(0));

    // Phase 3: feed correct AuthenticationResponse -> COMPLETE
    auto res = proc.feed(makeAuthResponse(kSRES), nullptr, ResponseSink{});

    EXPECT_EQ(res.action, ProcedureStepResult::Action::Completed);
    EXPECT_EQ(proc.state(), procedure::ProcedureState::Completed);
    EXPECT_EQ(res.finalResult.type, procedure::ProcedureType::Authentication);
}

// [TS 24.008 4.4.2] Wrong SRES in AuthenticationResponse fails the procedure.
TEST(AuthenticationProcedureTest, AuthP_AuthResponse_InvalidSRES_Fails) {
    AuthenticationProcedure proc;
    constexpr uint32_t kExpectedSRES = 0x12345678u;
    constexpr uint32_t kWrongSRES = 0xDEADBEEFu;
    auto chal = makeAuthChallenge(kExpectedSRES);

    [[maybe_unused]] auto _r1 = proc.feedExternalTyped(chal, nullptr);
    feedStep(proc, makeAuthResponse(0));

    auto res = proc.feed(makeAuthResponse(kWrongSRES), nullptr, ResponseSink{});

    EXPECT_EQ(res.action, ProcedureStepResult::Action::Failed);
    EXPECT_EQ(proc.state(), procedure::ProcedureState::Failed);
}

// [TS 24.008 4.4.2] T3106 timer expiry causes authentication failure.
TEST(AuthenticationProcedureTest, AuthP_Tick_TimerExpired_Fails) {
    AuthenticationProcedure proc;
    auto chal = makeAuthChallenge(0x12345678u);

    [[maybe_unused]] auto _r1 = proc.feedExternalTyped(chal, nullptr);
    feedStep(proc, makeAuthResponse(0));

    // Timer starts at 3000ms; tick past it.
    auto res = proc.tick(4000ms);

    EXPECT_EQ(res.action, ProcedureStepResult::Action::Failed);
    EXPECT_EQ(proc.state(), procedure::ProcedureState::Failed);
}

// [TS 24.008 4.4.2] cancel() aborts the procedure and sets Failed state.
TEST(AuthenticationProcedureTest, AuthP_Cancel_Aborts) {
    AuthenticationProcedure proc;
    auto chal = makeAuthChallenge(0x12345678u);

    [[maybe_unused]] auto _r1 = proc.feedExternalTyped(chal, nullptr);
    proc.cancel();

    EXPECT_EQ(proc.state(), procedure::ProcedureState::Failed);

    // Further ticks are no-ops after cancellation.
    auto res = proc.tick(1000ms);
    EXPECT_EQ(res.action, ProcedureStepResult::Action::Continue);
}
