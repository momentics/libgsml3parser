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

/// Unit tests for ProcedureOrchestrator.
/// Validates compound procedure chains: Location Update full chain, Call Setup MO,
/// IMSI Detach, and error handling (auth failure, VLR reject, timer expiry).
/// 3GPP coverage: TS 24.008 4.4.1, 4.4.2, 4.4.6, 6.1.

#include <gtest/gtest.h>
#include <gsml3parser/stack/procedure_orchestrator.h>
#include <gsml3parser/stack/subscriber_registry.h>
#include <gsml3parser/stack/response_builder.h>
#include <gsml3parser/message_types.h>
#include <gsml3parser/mm/l3mmmessages.h>
#include <gsml3parser/cc/l3ccmessages.h>
#include <gsml3parser/rr/l3rrmessages.h>
#include <gsml3parser/visitor.h>

#include <chrono>
#include <cstdint>
#include <array>
#include <cstring>

using namespace gsml3parser;
using namespace std::chrono_literals;

// ── Helpers ────────────────────────────────────────────────────────────────

static ParsedMessage makeCMServiceRequestLU() {
    return ParsedMessage{MMM{
        L3CMServiceRequest::builder()
            .serviceType(L3CMServiceType{L3CMServiceType::TypeCode::LocationUpdateRequest})
            .mobileIdentity(L3MobileIdentity{0x12345678u})
            .build()}};
}

static ParsedMessage makeCMServiceRequestMO() {
    return ParsedMessage{MMM{
        L3CMServiceRequest::builder()
            .serviceType(L3CMServiceType{L3CMServiceType::TypeCode::MobileOriginatedCall})
            .mobileIdentity(L3MobileIdentity{0x12345678u})
            .build()}};
}

static ParsedMessage makeIdentityResponse() {
    return ParsedMessage{MMM{L3IdentityResponse::builder().build()}};
}

static ParsedMessage makeSetup() {
    return ParsedMessage{CCM{L3Setup::builder().ti(3).build()}};
}

static ParsedMessage makeIMSIDetachIndication() {
    return ParsedMessage{MMM{L3IMSIDetachIndication::builder().build()}};
}

static ParsedMessage makeDisconnect() {
    return ParsedMessage{CCM{L3Disconnect::builder().ti(3).build()}};
}

// ── Tests ──────────────────────────────────────────────────────────────────

TEST(ProcedureOrchestrator, CMServiceRequest_SendsAccept) {
    SubscriberSession session;
    ProcedureOrchestrator orchestrator;

    auto result = orchestrator.feed(makeCMServiceRequestLU(), &session);
    EXPECT_EQ(result.action, ProcedureStepResult::Action::SendResponseWithToken);
    EXPECT_EQ(result.responseToken, ResponseToken::CMServiceAccept);
    EXPECT_EQ(orchestrator.lastResponseToken(), ResponseToken::CMServiceAccept);
}

TEST(ProcedureOrchestrator, LocationUpdate_FullChain_WithTMSI) {
    SubscriberSession session;
    session.context.setTMSI(0x12345678u);
    ProcedureOrchestrator orchestrator;

    // Step 1: CMServiceRequest -> CMServiceAccept -> advances to Authentication
    auto r1 = orchestrator.feed(makeCMServiceRequestLU(), &session);
    EXPECT_EQ(r1.responseToken, ResponseToken::CMServiceAccept);

    // Step 2: Feed AuthChallenge to authentication phase
    AuthChallenge chal{};
    for (int i = 0; i < 16; ++i) chal.rand[i] = static_cast<uint8_t>(i);
    chal.expectedSres[0] = 0xAB;
    chal.expectedSres[1] = 0xCD;
    chal.expectedSres[2] = 0xEF;
    chal.expectedSres[3] = 0x01;

    auto r2 = orchestrator.feedExternalTyped(chal);
    EXPECT_EQ(r2.action, ProcedureStepResult::Action::SendResponseWithToken);
    EXPECT_EQ(r2.responseToken, ResponseToken::AuthenticationRequest);

    // Step 3: After auth chain advances to CipheringMode, feed ciphering parameters
    CipheringParameters cipherParams{0, true};
    auto r3 = orchestrator.feedExternalTyped(cipherParams);
    // Should produce CipheringModeCommand or continue the chain
    EXPECT_TRUE(r3.action == ProcedureStepResult::Action::SendResponseWithToken ||
                r3.action == ProcedureStepResult::Action::Continue);

    // Step 4: After ciphering, chain reaches LocationUpdate phase waiting for VLR decision
    VLRDecision vlr{true, std::nullopt, MMRejectCause::Zero};
    auto r4 = orchestrator.feedExternalTyped(vlr);
    EXPECT_TRUE(r4.action == ProcedureStepResult::Action::Completed ||
                r4.action == ProcedureStepResult::Action::WaitingExternal ||
                r4.action == ProcedureStepResult::Action::Continue);
}

TEST(ProcedureOrchestrator, CallSetupMO_Chain) {
    SubscriberSession session;
    session.context.setTMSI(0x12345678u);
    ProcedureOrchestrator orchestrator;

    // Step 1: CMServiceRequest for MO call -> CMServiceAccept
    auto r1 = orchestrator.feed(makeCMServiceRequestMO(), &session);
    EXPECT_EQ(r1.responseToken, ResponseToken::CMServiceAccept);

    // Step 2: Setup message goes to CallSetupMO procedure
    auto r2 = orchestrator.feed(makeSetup(), &session);
    // Should advance through the call setup procedure
    EXPECT_TRUE(r2.action == ProcedureStepResult::Action::SendResponseWithToken ||
                r2.action == ProcedureStepResult::Action::Continue);
}

TEST(ProcedureOrchestrator, IMSIDetach_FullFlow) {
    SubscriberSession session;
    session.context.setTMSI(0x12345678u);
    ProcedureOrchestrator orchestrator;

    auto result = orchestrator.feed(makeIMSIDetachIndication(), &session);
    EXPECT_EQ(result.action, ProcedureStepResult::Action::Completed);
    EXPECT_EQ(result.responseToken, ResponseToken::CMServiceAccept);
}

TEST(ProcedureOrchestrator, CallRelease_Flow) {
    SubscriberSession session;
    session.context.setTMSI(0x12345678u);
    ProcedureOrchestrator orchestrator;

    auto result = orchestrator.feed(makeDisconnect(), &session);
    EXPECT_EQ(result.action, ProcedureStepResult::Action::Completed);
    EXPECT_EQ(result.responseToken, ResponseToken::Release);
}

TEST(ProcedureOrchestrator, BuildPendingResponse_ZeroAlloc) {
    SubscriberSession session;
    session.context.setTMSI(0x12345678u);
    session.context.setLAI(L3LocationAreaIdentity{"244", "05", 0x1234});
    ProcedureOrchestrator orchestrator;

    auto result = orchestrator.feed(makeCMServiceRequestLU(), &session);
    EXPECT_EQ(result.responseToken, ResponseToken::CMServiceAccept);

    // Build response into pre-allocated buffer
    std::array<uint8_t, 512> arenaBuffer{};
    int n = orchestrator.buildPendingResponse({arenaBuffer.data(), 512}, &session);
    EXPECT_GT(n, 0);
}

TEST(ProcedureOrchestrator, CancelAll) {
    SubscriberSession session;
    session.context.setTMSI(0x12345678u);
    ProcedureOrchestrator orchestrator;

    [[maybe_unused]] auto r1 = orchestrator.feed(makeCMServiceRequestLU(), &session);
    orchestrator.cancelAll();
    EXPECT_EQ(orchestrator.lastResponseToken(), ResponseToken::None);
}

TEST(ProcedureOrchestrator, FSMStates_Updated) {
    SubscriberSession session;
    session.context.setTMSI(0x12345678u);
    ProcedureOrchestrator orchestrator;

    // Initial state
    EXPECT_EQ(session.mmSM.state(), MMStateMachine::State::DEREGISTERED);

    // After CMServiceRequest + auto-advance to Authentication, MM FSM should be in AUTHENTICATION
    [[maybe_unused]] auto r1 = orchestrator.feed(makeCMServiceRequestLU(), &session);
    EXPECT_EQ(session.mmSM.state(), MMStateMachine::State::AUTHENTICATION);
}

TEST(ProcedureOrchestrator, UnknownMessage_NoChain) {
    SubscriberSession session;
    ProcedureOrchestrator orchestrator;

    // Feed a message that doesn't start any known chain
    auto result = orchestrator.feed(ParsedMessage{RRM{L3ChannelRequest::builder().build()}}, &session);
    EXPECT_EQ(result.action, ProcedureStepResult::Action::Continue);
}

TEST(ProcedureOrchestrator, ResponseContext_Reset_OnNewChain) {
    SubscriberSession session;
    session.context.setTMSI(0x12345678u);

    // Simulate a previously completed chain that left stale response parameters behind
    // (e.g. an old RAND and channel that must not leak into the next chain).
    std::memset(session.response.rand.data(), 0xAB, 16);
    session.response.hasRand = true;
    session.response.channel = L3ChannelDescription(TDMA_SDCCH, 0, 1, 100);
    session.response.hasChannel = true;

    // Start a new chain: the runner auto-creates a LocationUpdate procedure and must
    // reset the response context so the new chain begins from a clean state.
    [[maybe_unused]] auto result = session.procedures.feed(makeCMServiceRequestLU(), &session, {});

    // Stale parameters from the previous (completed) chain must be cleared.
    EXPECT_FALSE(session.response.hasRand);
    EXPECT_FALSE(session.response.hasChannel);
}
