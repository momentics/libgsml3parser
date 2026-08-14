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

#include <gtest/gtest.h>
#include <gsml3parser/stack/state_machine.h>
#include <gsml3parser/rr/l3rrmessages.h>
#include <gsml3parser/mm/l3mmmessages.h>
#include <gsml3parser/cc/l3ccmessages.h>
#include <gsml3parser/visitor.h>

using namespace gsml3parser;

// ── Helper: build ParsedMessage instances for tests ──────────────────────

static ParsedMessage makeRRChannelRequest() {
    return ParsedMessage{RRM{L3ChannelRequest::builder().build()}};
}

static ParsedMessage makeRRPagingResponse() {
    return ParsedMessage{RRM{L3PagingResponse::builder().build()}};
}

static ParsedMessage makeRRMeasurementReport() {
    return ParsedMessage{RRM{L3MeasurementReport::builder().build()}};
}

static ParsedMessage makeRRChannelRelease() {
    return ParsedMessage{RRM{L3ChannelRelease::builder().build()}};
}

static ParsedMessage makeRRCipheringModeComplete() {
    return ParsedMessage{RRM{L3CipheringModeComplete::builder().build()}};
}

static ParsedMessage makeRRHandoverCommand() {
    return ParsedMessage{RRM{L3HandoverCommand::builder().build()}};
}

static ParsedMessage makeMMCMServiceRequest() {
    return ParsedMessage{MMM{L3CMServiceRequest::builder().build()}};
}

static ParsedMessage makeMMCMServiceAccept() {
    return ParsedMessage{MMM{L3CMServiceAccept::builder().build()}};
}

static ParsedMessage makeMMIdentityResponse() {
    return ParsedMessage{MMM{L3IdentityResponse::builder().build()}};
}

static ParsedMessage makeMMAuthenticationResponse() {
    return ParsedMessage{MMM{L3AuthenticationResponse::builder().build()}};
}

static ParsedMessage makeMMLocationUpdatingRequest() {
    return ParsedMessage{MMM{L3LocationUpdatingRequest::builder().build()}};
}

static ParsedMessage makeCCSetup() {
    return ParsedMessage{CCM{L3Setup::builder().build()}};
}

static ParsedMessage makeCCAlerting() {
    return ParsedMessage{CCM{L3Alerting::builder().build()}};
}

static ParsedMessage makeCCConnect() {
    return ParsedMessage{CCM{L3Connect::builder().build()}};
}

static ParsedMessage makeCCDisconnect() {
    return ParsedMessage{CCM{L3Disconnect::builder().build()}};
}

// ── Helper: concrete test FSM for ProtocolStateMachine base tests ───────

class TestFSM : public ProtocolStateMachine {
public:
    bool messageHandled{false};
    bool timerHandled{false};
    int lastState{0};
    ParsedMessage lastMessage;

    std::string_view debugName() const override { return "TestFSM"; }

protected:
    SMResult handle_message_impl(int state, const ParsedMessage& msg) override {
        messageHandled = true;
        lastState = state;
        lastMessage = msg;
        return {SMAction::Transition, state + 1};
    }

    SMResult handle_timer_impl(int state, L3TimerId) override {
        timerHandled = true;
        lastState = state;
        return {SMAction::None, std::nullopt};
    }
};

// ── ProtocolStateMachine base tests ──────────────────────────────────────

// setState() and state() work correctly
TEST(ProtocolStateMachineTest, SetAndGetState) {
    TestFSM fsm;
    fsm.setState(5);
    EXPECT_EQ(fsm.state(), 5);
    fsm.setState(0);
    EXPECT_EQ(fsm.state(), 0);
}

// processMessage() delegates to handle_message_impl and updates state on transition
TEST(ProtocolStateMachineTest, ProcessMessage_callsImpl) {
    TestFSM fsm;
    fsm.setState(3);
    auto msg = makeCCSetup();
    SMResult result = fsm.processMessage(msg);

    EXPECT_TRUE(fsm.messageHandled);
    EXPECT_EQ(fsm.lastState, 3);
    EXPECT_EQ(result.action, SMAction::Transition);
    EXPECT_EQ(result.nextState, 4);
    EXPECT_EQ(fsm.state(), 4);
}

// processTimer() delegates to handle_timer_impl; no state change when action is None
TEST(ProtocolStateMachineTest, ProcessTimer_callsImpl) {
    TestFSM fsm;
    fsm.setState(7);
    SMResult result = fsm.processTimer(L3TimerId::T3101);

    EXPECT_TRUE(fsm.timerHandled);
    EXPECT_EQ(fsm.lastState, 7);
    EXPECT_EQ(result.action, SMAction::None);
    EXPECT_EQ(fsm.state(), 7);
}

// Default SMResult has action None and no next state
TEST(ProtocolStateMachineTest, DefaultResult_isNone) {
    SMResult result;
    EXPECT_EQ(result.action, SMAction::None);
    EXPECT_FALSE(result.nextState.has_value());
    EXPECT_FALSE(result.causesTransition());
}

// SMResult is small enough for efficient stack use
TEST(ProtocolStateMachineTest, SMResult_SizeIsSmall) {
    EXPECT_LE(sizeof(SMResult), 16u);
}

// causesTransition() returns true for Transition and SendResponse actions
TEST(ProtocolStateMachineTest, CausesTransition_check) {
    SMResult r1{SMAction::Transition, 5};
    EXPECT_TRUE(r1.causesTransition());

    SMResult r2{SMAction::SendResponse, 3};
    EXPECT_TRUE(r2.causesTransition());

    SMResult r3{SMAction::None, std::nullopt};
    EXPECT_FALSE(r3.causesTransition());

    SMResult r4{SMAction::Reject, std::nullopt};
    EXPECT_FALSE(r4.causesTransition());
}

// ── RR State Machine tests ───────────────────────────────────────────────

// IDLE + ChannelRequest -> CHANNEL_REQUESTED
TEST(RRStateMachineTest, Idle_receivesChannelRequest_transitionsToRequested) {
    RRStateMachine fsm;
    fsm.setState(RRStateMachine::State::IDLE);
    auto msg = makeRRChannelRequest();
    SMResult result = fsm.processMessage(msg);

    EXPECT_EQ(result.action, SMAction::Transition);
    EXPECT_EQ(result.nextState, static_cast<int>(RRStateMachine::State::CHANNEL_REQUESTED));
    EXPECT_EQ(fsm.state(), RRStateMachine::State::CHANNEL_REQUESTED);
}

// CHANNEL_ASSIGNED + PagingResponse -> WAITING_MM
TEST(RRStateMachineTest, ChannelAssigned_receivesPagingResponse_transitionsToWaitingMM) {
    RRStateMachine fsm;
    fsm.setState(RRStateMachine::State::CHANNEL_ASSIGNED);
    auto msg = makeRRPagingResponse();
    SMResult result = fsm.processMessage(msg);

    EXPECT_EQ(result.action, SMAction::Transition);
    EXPECT_EQ(result.nextState, static_cast<int>(RRStateMachine::State::WAITING_MM));
    EXPECT_EQ(fsm.state(), RRStateMachine::State::WAITING_MM);
}

// ACTIVE + MeasurementReport -> stays ACTIVE (no transition)
TEST(RRStateMachineTest, Active_receivesMeasurementReport_staysActive) {
    RRStateMachine fsm;
    fsm.setState(RRStateMachine::State::ACTIVE);
    auto msg = makeRRMeasurementReport();
    SMResult result = fsm.processMessage(msg);

    EXPECT_EQ(result.action, SMAction::None);
    EXPECT_FALSE(result.nextState.has_value());
    EXPECT_EQ(fsm.state(), RRStateMachine::State::ACTIVE);
}

// debugName() returns "RRStateMachine"
TEST(RRStateMachineTest, DebugName_returnsCorrectString) {
    RRStateMachine fsm;
    EXPECT_EQ(fsm.debugName(), "RRStateMachine");
}

// ACTIVE + unexpected message type -> no transition
TEST(RRStateMachineTest, UnexpectedMessage_noTransition) {
    RRStateMachine fsm;
    fsm.setState(RRStateMachine::State::ACTIVE);
    auto msg = makeCCSetup();
    SMResult result = fsm.processMessage(msg);

    EXPECT_EQ(result.action, SMAction::None);
    EXPECT_EQ(fsm.state(), RRStateMachine::State::ACTIVE);
}

// ACTIVE + CipheringModeComplete -> stays ACTIVE (no transition)
TEST(RRStateMachineTest, CipheringModeComplete_staysActive) {
    RRStateMachine fsm;
    fsm.setState(RRStateMachine::State::ACTIVE);
    auto msg = makeRRCipheringModeComplete();
    SMResult result = fsm.processMessage(msg);

    EXPECT_EQ(result.action, SMAction::None);
    EXPECT_EQ(fsm.state(), RRStateMachine::State::ACTIVE);
}

// ACTIVE + ChannelRelease -> CHANNEL_RELEASE
TEST(RRStateMachineTest, Active_receivesChannelRelease_transitionsToRelease) {
    RRStateMachine fsm;
    fsm.setState(RRStateMachine::State::ACTIVE);
    auto msg = makeRRChannelRelease();
    SMResult result = fsm.processMessage(msg);

    EXPECT_EQ(result.action, SMAction::Transition);
    EXPECT_EQ(result.nextState, static_cast<int>(RRStateMachine::State::CHANNEL_RELEASE));
    EXPECT_EQ(fsm.state(), RRStateMachine::State::CHANNEL_RELEASE);
}

// ACTIVE + HandoverCommand -> HANDOVER
TEST(RRStateMachineTest, Active_receivesHandoverCommand_transitionsToHandover) {
    RRStateMachine fsm;
    fsm.setState(RRStateMachine::State::ACTIVE);
    auto msg = makeRRHandoverCommand();
    SMResult result = fsm.processMessage(msg);

    EXPECT_EQ(result.action, SMAction::Transition);
    EXPECT_EQ(result.nextState, static_cast<int>(RRStateMachine::State::HANDOVER));
    EXPECT_EQ(fsm.state(), RRStateMachine::State::HANDOVER);
}

// LINK_ESTABLISHED + MM message -> WAITING_MM
TEST(RRStateMachineTest, LinkEstablished_receivesMMMessage_transitionsToWaitingMM) {
    RRStateMachine fsm;
    fsm.setState(RRStateMachine::State::LINK_ESTABLISHED);
    auto msg = makeMMCMServiceRequest();
    SMResult result = fsm.processMessage(msg);

    EXPECT_EQ(result.action, SMAction::Transition);
    EXPECT_EQ(result.nextState, static_cast<int>(RRStateMachine::State::WAITING_MM));
    EXPECT_EQ(fsm.state(), RRStateMachine::State::WAITING_MM);
}

// WAITING_MM + CMServiceAccept -> ACTIVE
TEST(RRStateMachineTest, WaitingMM_receivesCMServiceAccept_transitionsToActive) {
    RRStateMachine fsm;
    fsm.setState(RRStateMachine::State::WAITING_MM);
    auto msg = makeMMCMServiceAccept();
    SMResult result = fsm.processMessage(msg);

    EXPECT_EQ(result.action, SMAction::Transition);
    EXPECT_EQ(result.nextState, static_cast<int>(RRStateMachine::State::ACTIVE));
    EXPECT_EQ(fsm.state(), RRStateMachine::State::ACTIVE);
}

// CIPHER_MODE + CipheringModeComplete -> ACTIVE
TEST(RRStateMachineTest, CipherMode_receivesCipheringModeComplete_transitionsToActive) {
    RRStateMachine fsm;
    fsm.setState(RRStateMachine::State::CIPHER_MODE);
    auto msg = makeRRCipheringModeComplete();
    SMResult result = fsm.processMessage(msg);

    EXPECT_EQ(result.action, SMAction::Transition);
    EXPECT_EQ(result.nextState, static_cast<int>(RRStateMachine::State::ACTIVE));
    EXPECT_EQ(fsm.state(), RRStateMachine::State::ACTIVE);
}

// CHANNEL_ASSIGNED + T3109 expiry -> CHANNEL_RELEASE
TEST(RRStateMachineTest, TimerExpiry_channelAssigned_releasesChannel) {
    RRStateMachine fsm;
    fsm.setState(RRStateMachine::State::CHANNEL_ASSIGNED);
    SMResult result = fsm.processTimer(L3TimerId::T3109);

    EXPECT_EQ(result.action, SMAction::Transition);
    EXPECT_EQ(result.nextState, static_cast<int>(RRStateMachine::State::CHANNEL_RELEASE));
    EXPECT_EQ(fsm.state(), RRStateMachine::State::CHANNEL_RELEASE);
}

// WAITING_MM + T3109 expiry -> CHANNEL_RELEASE
TEST(RRStateMachineTest, TimerExpiry_waitingMM_releasesChannel) {
    RRStateMachine fsm;
    fsm.setState(RRStateMachine::State::WAITING_MM);
    SMResult result = fsm.processTimer(L3TimerId::T3109);

    EXPECT_EQ(result.action, SMAction::Transition);
    EXPECT_EQ(result.nextState, static_cast<int>(RRStateMachine::State::CHANNEL_RELEASE));
    EXPECT_EQ(fsm.state(), RRStateMachine::State::CHANNEL_RELEASE);
}

// ACTIVE + unknown timer -> no transition
TEST(RRStateMachineTest, TimerExpiry_active_noTransition) {
    RRStateMachine fsm;
    fsm.setState(RRStateMachine::State::ACTIVE);
    SMResult result = fsm.processTimer(L3TimerId::T3101);

    EXPECT_EQ(result.action, SMAction::None);
    EXPECT_EQ(fsm.state(), RRStateMachine::State::ACTIVE);
}

// ── MM State Machine tests ───────────────────────────────────────────────

// DEREGISTERED + CMServiceRequest -> SERVICE_REQUEST
TEST(MMStateMachineTest, Deregistered_receivesCMServiceRequest_transitionsToServiceRequest) {
    MMStateMachine fsm;
    fsm.setState(MMStateMachine::State::DEREGISTERED);
    auto msg = makeMMCMServiceRequest();
    SMResult result = fsm.processMessage(msg);

    EXPECT_EQ(result.action, SMAction::Transition);
    EXPECT_EQ(result.nextState, static_cast<int>(MMStateMachine::State::SERVICE_REQUEST));
    EXPECT_EQ(fsm.state(), MMStateMachine::State::SERVICE_REQUEST);
}

// debugName() returns "MMStateMachine"
TEST(MMStateMachineTest, DebugName_returnsCorrectString) {
    MMStateMachine fsm;
    EXPECT_EQ(fsm.debugName(), "MMStateMachine");
}

// DEREGISTERED + unexpected (non-MM) message -> no transition
TEST(MMStateMachineTest, UnexpectedMessage_noTransition) {
    MMStateMachine fsm;
    fsm.setState(MMStateMachine::State::DEREGISTERED);
    auto msg = makeCCSetup();
    SMResult result = fsm.processMessage(msg);

    EXPECT_EQ(result.action, SMAction::None);
    EXPECT_EQ(fsm.state(), MMStateMachine::State::DEREGISTERED);
}

// SERVICE_REQUEST + IdentityResponse -> IDENTITY_VERIFIED
TEST(MMStateMachineTest, ServiceRequest_receivesIdentityResponse_transitionsToVerified) {
    MMStateMachine fsm;
    fsm.setState(MMStateMachine::State::SERVICE_REQUEST);
    auto msg = makeMMIdentityResponse();
    SMResult result = fsm.processMessage(msg);

    EXPECT_EQ(result.action, SMAction::Transition);
    EXPECT_EQ(result.nextState, static_cast<int>(MMStateMachine::State::IDENTITY_VERIFIED));
    EXPECT_EQ(fsm.state(), MMStateMachine::State::IDENTITY_VERIFIED);
}

// IDENTITY_VERIFIED + AuthenticationResponse -> AUTHENTICATED
TEST(MMStateMachineTest, IdentityVerified_receivesAuthResponse_transitionsToAuthenticated) {
    MMStateMachine fsm;
    fsm.setState(MMStateMachine::State::IDENTITY_VERIFIED);
    auto msg = makeMMAuthenticationResponse();
    SMResult result = fsm.processMessage(msg);

    EXPECT_EQ(result.action, SMAction::Transition);
    EXPECT_EQ(result.nextState, static_cast<int>(MMStateMachine::State::AUTHENTICATED));
    EXPECT_EQ(fsm.state(), MMStateMachine::State::AUTHENTICATED);
}

// AUTHENTICATION + AuthenticationResponse -> AUTHENTICATED
TEST(MMStateMachineTest, Authentication_receivesAuthResponse_transitionsToAuthenticated) {
    MMStateMachine fsm;
    fsm.setState(MMStateMachine::State::AUTHENTICATION);
    auto msg = makeMMAuthenticationResponse();
    SMResult result = fsm.processMessage(msg);

    EXPECT_EQ(result.action, SMAction::Transition);
    EXPECT_EQ(result.nextState, static_cast<int>(MMStateMachine::State::AUTHENTICATED));
    EXPECT_EQ(fsm.state(), MMStateMachine::State::AUTHENTICATED);
}

// AUTHENTICATED + LocationUpdatingRequest -> LOCATION_UPDATE
TEST(MMStateMachineTest, Authenticated_receivesLocationUpdate_transitionsToUpdate) {
    MMStateMachine fsm;
    fsm.setState(MMStateMachine::State::AUTHENTICATED);
    auto msg = makeMMLocationUpdatingRequest();
    SMResult result = fsm.processMessage(msg);

    EXPECT_EQ(result.action, SMAction::Transition);
    EXPECT_EQ(result.nextState, static_cast<int>(MMStateMachine::State::LOCATION_UPDATE));
    EXPECT_EQ(fsm.state(), MMStateMachine::State::LOCATION_UPDATE);
}

// LOCATION_UPDATE + CMServiceAccept -> REGISTERED
TEST(MMStateMachineTest, LocationUpdate_receivesAccept_transitionsToRegistered) {
    MMStateMachine fsm;
    fsm.setState(MMStateMachine::State::LOCATION_UPDATE);
    auto msg = makeMMCMServiceAccept();
    SMResult result = fsm.processMessage(msg);

    EXPECT_EQ(result.action, SMAction::Transition);
    EXPECT_EQ(result.nextState, static_cast<int>(MMStateMachine::State::REGISTERED));
    EXPECT_EQ(fsm.state(), MMStateMachine::State::REGISTERED);
}

// WAITING_IDENTITY + IdentityResponse -> IDENTITY_VERIFIED
TEST(MMStateMachineTest, WaitingIdentity_receivesIdentityResponse_transitionsToVerified) {
    MMStateMachine fsm;
    fsm.setState(MMStateMachine::State::WAITING_IDENTITY);
    auto msg = makeMMIdentityResponse();
    SMResult result = fsm.processMessage(msg);

    EXPECT_EQ(result.action, SMAction::Transition);
    EXPECT_EQ(result.nextState, static_cast<int>(MMStateMachine::State::IDENTITY_VERIFIED));
    EXPECT_EQ(fsm.state(), MMStateMachine::State::IDENTITY_VERIFIED);
}

// SERVICE_REQUEST + T3101 expiry -> DEREGISTERED
TEST(MMStateMachineTest, TimerExpiry_serviceRequest_deregisters) {
    MMStateMachine fsm;
    fsm.setState(MMStateMachine::State::SERVICE_REQUEST);
    SMResult result = fsm.processTimer(L3TimerId::T3101);

    EXPECT_EQ(result.action, SMAction::Transition);
    EXPECT_EQ(result.nextState, static_cast<int>(MMStateMachine::State::DEREGISTERED));
    EXPECT_EQ(fsm.state(), MMStateMachine::State::DEREGISTERED);
}

// AUTHENTICATION + T3106 expiry -> DEREGISTERED
TEST(MMStateMachineTest, TimerExpiry_authentication_deregisters) {
    MMStateMachine fsm;
    fsm.setState(MMStateMachine::State::AUTHENTICATION);
    SMResult result = fsm.processTimer(L3TimerId::T3106);

    EXPECT_EQ(result.action, SMAction::Transition);
    EXPECT_EQ(result.nextState, static_cast<int>(MMStateMachine::State::DEREGISTERED));
    EXPECT_EQ(fsm.state(), MMStateMachine::State::DEREGISTERED);
}

// REGISTERED + unknown timer -> no transition
TEST(MMStateMachineTest, TimerExpiry_registered_noTransition) {
    MMStateMachine fsm;
    fsm.setState(MMStateMachine::State::REGISTERED);
    SMResult result = fsm.processTimer(L3TimerId::T3101);

    EXPECT_EQ(result.action, SMAction::None);
    EXPECT_EQ(fsm.state(), MMStateMachine::State::REGISTERED);
}

// ── CC State Machine tests ───────────────────────────────────────────────

// IDLE + Setup -> SETUP_RECEIVED
TEST(CCStateMachineTest, Idle_receivesSetup_transitionsToSetupReceived) {
    CCStateMachine fsm;
    fsm.setState(CCStateMachine::State::IDLE);
    auto msg = makeCCSetup();
    SMResult result = fsm.processMessage(msg);

    EXPECT_EQ(result.action, SMAction::Transition);
    EXPECT_EQ(result.nextState, static_cast<int>(CCStateMachine::State::SETUP_RECEIVED));
    EXPECT_EQ(fsm.state(), CCStateMachine::State::SETUP_RECEIVED);
}

// debugName() returns "CCStateMachine"
TEST(CCStateMachineTest, DebugName_returnsCorrectString) {
    CCStateMachine fsm;
    EXPECT_EQ(fsm.debugName(), "CCStateMachine");
}

// IDLE + unexpected (non-CC) message -> no transition
TEST(CCStateMachineTest, UnexpectedMessage_noTransition) {
    CCStateMachine fsm;
    fsm.setState(CCStateMachine::State::IDLE);
    auto msg = makeMMCMServiceRequest();
    SMResult result = fsm.processMessage(msg);

    EXPECT_EQ(result.action, SMAction::None);
    EXPECT_EQ(fsm.state(), CCStateMachine::State::IDLE);
}

// SETUP_RECEIVED -> PROCEEDING (automatic transition on any message processing)
TEST(CCStateMachineTest, SetupReceived_transitionsToProceeding) {
    CCStateMachine fsm;
    fsm.setState(CCStateMachine::State::SETUP_RECEIVED);
    auto msg = makeCCSetup();
    SMResult result = fsm.processMessage(msg);

    EXPECT_EQ(result.action, SMAction::Transition);
    EXPECT_EQ(result.nextState, static_cast<int>(CCStateMachine::State::PROCEEDING));
    EXPECT_EQ(fsm.state(), CCStateMachine::State::PROCEEDING);
}

// PROCEEDING + Alerting -> ALERTING
TEST(CCStateMachineTest, Proceeding_receivesAlerting_transitionsToAlerting) {
    CCStateMachine fsm;
    fsm.setState(CCStateMachine::State::PROCEEDING);
    auto msg = makeCCAlerting();
    SMResult result = fsm.processMessage(msg);

    EXPECT_EQ(result.action, SMAction::Transition);
    EXPECT_EQ(result.nextState, static_cast<int>(CCStateMachine::State::ALERTING));
    EXPECT_EQ(fsm.state(), CCStateMachine::State::ALERTING);
}

// ALERTING + Connect -> CONNECT
TEST(CCStateMachineTest, Alerting_receivesConnect_transitionsToConnect) {
    CCStateMachine fsm;
    fsm.setState(CCStateMachine::State::ALERTING);
    auto msg = makeCCConnect();
    SMResult result = fsm.processMessage(msg);

    EXPECT_EQ(result.action, SMAction::Transition);
    EXPECT_EQ(result.nextState, static_cast<int>(CCStateMachine::State::CONNECT));
    EXPECT_EQ(fsm.state(), CCStateMachine::State::CONNECT);
}

// ACTIVE + Disconnect -> DISCONNECT_RECEIVED
TEST(CCStateMachineTest, Active_receivesDisconnect_transitionsToDisconnectReceived) {
    CCStateMachine fsm;
    fsm.setState(CCStateMachine::State::ACTIVE);
    auto msg = makeCCDisconnect();
    SMResult result = fsm.processMessage(msg);

    EXPECT_EQ(result.action, SMAction::Transition);
    EXPECT_EQ(result.nextState, static_cast<int>(CCStateMachine::State::DISCONNECT_RECEIVED));
    EXPECT_EQ(fsm.state(), CCStateMachine::State::DISCONNECT_RECEIVED);
}

// DISCONNECT_RECEIVED -> RELEASE (automatic transition)
TEST(CCStateMachineTest, DisconnectReceived_transitionsToRelease) {
    CCStateMachine fsm;
    fsm.setState(CCStateMachine::State::DISCONNECT_RECEIVED);
    auto msg = makeCCDisconnect();
    SMResult result = fsm.processMessage(msg);

    EXPECT_EQ(result.action, SMAction::Transition);
    EXPECT_EQ(result.nextState, static_cast<int>(CCStateMachine::State::RELEASE));
    EXPECT_EQ(fsm.state(), CCStateMachine::State::RELEASE);
}

// PROCEEDING + T3101 expiry -> IDLE
TEST(CCStateMachineTest, TimerExpiry_proceeding_returnsToIdle) {
    CCStateMachine fsm;
    fsm.setState(CCStateMachine::State::PROCEEDING);
    SMResult result = fsm.processTimer(L3TimerId::T3101);

    EXPECT_EQ(result.action, SMAction::Transition);
    EXPECT_EQ(result.nextState, static_cast<int>(CCStateMachine::State::IDLE));
    EXPECT_EQ(fsm.state(), CCStateMachine::State::IDLE);
}

// ACTIVE + unknown timer -> no transition
TEST(CCStateMachineTest, TimerExpiry_active_noTransition) {
    CCStateMachine fsm;
    fsm.setState(CCStateMachine::State::ACTIVE);
    SMResult result = fsm.processTimer(L3TimerId::T3101);

    EXPECT_EQ(result.action, SMAction::None);
    EXPECT_EQ(fsm.state(), CCStateMachine::State::ACTIVE);
}

// ── Full transition sequence tests ───────────────────────────────────────

// Full RR flow: IDLE -> CHANNEL_REQUESTED (simulated) -> CHANNEL_ASSIGNED -> WAITING_MM -> ACTIVE
TEST(RRStateMachineTest, FullTransitionSequence) {
    RRStateMachine fsm;
    fsm.setState(RRStateMachine::State::IDLE);

    // Channel Request
    auto chReq = makeRRChannelRequest();
    auto r1 = fsm.processMessage(chReq);
    EXPECT_EQ(fsm.state(), RRStateMachine::State::CHANNEL_REQUESTED);
    EXPECT_TRUE(r1.causesTransition());

    // Simulate channel assignment (BTS sends ImmediateAssignment externally)
    fsm.setState(RRStateMachine::State::CHANNEL_ASSIGNED);

    // Paging Response
    auto pagResp = makeRRPagingResponse();
    auto r2 = fsm.processMessage(pagResp);
    EXPECT_EQ(fsm.state(), RRStateMachine::State::WAITING_MM);
    EXPECT_TRUE(r2.causesTransition());

    // CM Service Accept (from MM layer)
    auto cmAccept = makeMMCMServiceAccept();
    auto r3 = fsm.processMessage(cmAccept);
    EXPECT_EQ(fsm.state(), RRStateMachine::State::ACTIVE);
    EXPECT_TRUE(r3.causesTransition());
}

// Full CC flow: IDLE -> SETUP_RECEIVED -> PROCEEDING -> ALERTING -> CONNECT -> ACTIVE
TEST(CCStateMachineTest, FullCallSetupSequence) {
    CCStateMachine fsm;
    fsm.setState(CCStateMachine::State::IDLE);

    // Setup
    auto setup = makeCCSetup();
    auto r1 = fsm.processMessage(setup);
    EXPECT_EQ(fsm.state(), CCStateMachine::State::SETUP_RECEIVED);

    // SETUP_RECEIVED auto-transitions to PROCEEDING on next message
    auto r2 = fsm.processMessage(setup);
    EXPECT_EQ(fsm.state(), CCStateMachine::State::PROCEEDING);

    // Alerting
    auto alerting = makeCCAlerting();
    auto r3 = fsm.processMessage(alerting);
    EXPECT_EQ(fsm.state(), CCStateMachine::State::ALERTING);

    // Connect
    auto connect = makeCCConnect();
    auto r4 = fsm.processMessage(connect);
    EXPECT_EQ(fsm.state(), CCStateMachine::State::CONNECT);
}

// Full MM flow: DEREGISTERED -> SERVICE_REQUEST -> IDENTITY_VERIFIED -> AUTHENTICATED -> REGISTERED
TEST(MMStateMachineTest, FullRegistrationSequence) {
    MMStateMachine fsm;
    fsm.setState(MMStateMachine::State::DEREGISTERED);

    // CM Service Request
    auto cmReq = makeMMCMServiceRequest();
    auto r1 = fsm.processMessage(cmReq);
    EXPECT_EQ(fsm.state(), MMStateMachine::State::SERVICE_REQUEST);

    // Identity Response
    auto idResp = makeMMIdentityResponse();
    auto r2 = fsm.processMessage(idResp);
    EXPECT_EQ(fsm.state(), MMStateMachine::State::IDENTITY_VERIFIED);

    // Authentication Response (skips AUTHENTICATION state, goes directly)
    auto authResp = makeMMAuthenticationResponse();
    auto r3 = fsm.processMessage(authResp);
    EXPECT_EQ(fsm.state(), MMStateMachine::State::AUTHENTICATED);

    // Location Updating Request
    auto locReq = makeMMLocationUpdatingRequest();
    auto r4 = fsm.processMessage(locReq);
    EXPECT_EQ(fsm.state(), MMStateMachine::State::LOCATION_UPDATE);

    // CM Service Accept (location update accepted)
    auto cmAccept = makeMMCMServiceAccept();
    auto r5 = fsm.processMessage(cmAccept);
    EXPECT_EQ(fsm.state(), MMStateMachine::State::REGISTERED);
}
