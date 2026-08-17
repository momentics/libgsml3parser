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

#include "gsml3parser/stack/procedures/call_setup_mt.h"

#include "gsml3parser/stack/subscriber_registry.h"
#include "gsml3parser/cc/l3ccmessages.h"
#include "gsml3parser/mm/l3mmmessages.h"
#include "gsml3parser/rr/l3rrmessages.h"

namespace gsml3parser {

CallSetupMTPercedure::CallSetupMTPercedure(std::string calledNumber)
    : mCalledNumber(std::move(calledNumber)) {}

procedure::ProcedureType CallSetupMTPercedure::type() const {
    return procedure::ProcedureType::CallSetup_MT;
}

procedure::ProcedureState CallSetupMTPercedure::state() const {
    return mProcState;
}

void CallSetupMTPercedure::doTransitionTo(State s) {
    mCurrentState = s;
    if (s == State::COMPLETED) mProcState = procedure::ProcedureState::Completed;
    else if (s == State::FAILED) mProcState = procedure::ProcedureState::Failed;
    else mProcState = procedure::ProcedureState::InProgress;
}

void CallSetupMTPercedure::doFail(std::string_view reason) {
    (void)reason;
    mCurrentState = State::FAILED;
    mProcState = procedure::ProcedureState::Failed;
}

void CallSetupMTPercedure::doComplete() {
    mCurrentState = State::COMPLETED;
    mProcState = procedure::ProcedureState::Completed;
}

ProcedureStepResult CallSetupMTPercedure::feed(const ParsedMessage& msg,
    SubscriberSession* session, ResponseSink&& sink) {
    (void)session;
    ProcedureStepResult result;

    auto pd = messagePD(msg);
    auto mti = messageMTI(msg);

    switch (mCurrentState) {
        case State::INIT:
            break;

        case State::PAGE: {
            result.action = ProcedureStepResult::Action::SendResponseWithToken;
            result.responseToken = ResponseToken::PagingRequestType1;
            startTimer(L3TimerId::T3109, std::chrono::milliseconds(5000));
            transitionTo(State::WAIT_PAGE_RESPONSE);
            if (sink) sink(SMAction::SendResponse, msg, session);
            break;
        }

        case State::WAIT_PAGE_RESPONSE:
            if (pd == L3PD::RadioResource && mti == L3PagingResponse::MTI) {
                stopTimer();
                transitionTo(State::ASSIGN_SDCCH);
                result.action = ProcedureStepResult::Action::SendResponseWithToken;
                result.responseToken = ResponseToken::ImmediateAssignment;
                if (sink) sink(SMAction::SendResponse, msg, session);
            }
            break;

        case State::ASSIGN_SDCCH:
            transitionTo(State::SEND_SETUP);
            result.action = ProcedureStepResult::Action::SendResponseWithToken;
            result.responseToken = ResponseToken::Setup;
            if (sink) sink(SMAction::SendResponse, msg, session);
            break;

        case State::SEND_SETUP:
            startTimer(L3TimerId::T3101, std::chrono::milliseconds(3000));
            transitionTo(State::WAIT_CONFIRMED);
            result.action = ProcedureStepResult::Action::SendResponseWithToken;
            result.responseToken = ResponseToken::Setup;
            if (sink) sink(SMAction::SendResponse, msg, session);
            break;

        case State::WAIT_CONFIRMED:
            if (pd == L3PD::CallControl && mti == L3CallConfirmed::MTI) {
                stopTimer();
                transitionTo(State::ASSIGN_TCH);
            }
            break;

        case State::ASSIGN_TCH:
            startTimer(L3TimerId::T3101, std::chrono::milliseconds(3000));
            transitionTo(State::WAIT_ASSIGN_COMPLETE);
            result.action = ProcedureStepResult::Action::SendResponseWithToken;
            result.responseToken = ResponseToken::AssignmentCommand;
            if (sink) sink(SMAction::SendResponse, msg, session);
            break;

        case State::WAIT_ASSIGN_COMPLETE:
            if (pd == L3PD::RadioResource && mti == L3AssignmentComplete::MTI) {
                stopTimer();
                transitionTo(State::ALERTING);
                result.action = ProcedureStepResult::Action::SendResponseWithToken;
                result.responseToken = ResponseToken::Alerting;
                if (sink) sink(SMAction::SendResponse, msg, session);
            }
            break;

        case State::ALERTING:
            transitionTo(State::CONNECT);
            result.action = ProcedureStepResult::Action::SendResponseWithToken;
            result.responseToken = ResponseToken::Connect;
            if (sink) sink(SMAction::SendResponse, msg, session);
            break;

        case State::CONNECT:
            transitionTo(State::ACTIVE);
            result.action = ProcedureStepResult::Action::SendResponseWithToken;
            result.responseToken = ResponseToken::ConnectAcknowledge;
            if (sink) sink(SMAction::SendResponse, msg, session);
            break;

        case State::ACTIVE:
            if (pd == L3PD::CallControl && mti == L3ConnectAcknowledge::MTI) {
                complete();
                result.action = ProcedureStepResult::Action::Completed;
                result.finalResult = {type(), mProcState, "call_active"};
            }
            break;

        case State::COMPLETED:
        case State::FAILED:
            break;
    }

    return result;
}

ProcedureStepResult CallSetupMTPercedure::feedExternalTyped(
    const ExternalData& data, ResponseSink&& sink) {
    (void)data;
    ProcedureStepResult result;

    if (mCurrentState == State::INIT) {
        transitionTo(State::PAGE);
        result.action = ProcedureStepResult::Action::SendResponseWithToken;
        result.responseToken = ResponseToken::PagingRequestType1;
        startTimer(L3TimerId::T3109, std::chrono::milliseconds(5000));
        mPageAttempt = 1;
        if (sink) sink(SMAction::SendResponse, ParsedMessage{RRM{L3PagingRequestType1{}}}, nullptr);
    }

    return result;
}

ProcedureStepResult CallSetupMTPercedure::tick(std::chrono::milliseconds delta) {
    if (!mTimerRunning) {
        return {ProcedureStepResult::Action::Continue};
    }

    mTimerRemaining -= delta;
    if (mTimerRemaining <= std::chrono::milliseconds(0)) {
        stopTimer();
        if (mCurrentState == State::WAIT_PAGE_RESPONSE && mPageAttempt < MAX_PAGE_ATTEMPTS) {
            ++mPageAttempt;
            startTimer(L3TimerId::T3109, std::chrono::milliseconds(5000));
            return {ProcedureStepResult::Action::Continue};
        }
        fail("timer_expired");
        ProcedureStepResult result;
        result.action = ProcedureStepResult::Action::Failed;
        result.finalResult = {type(), mProcState, "timer_expired"};
        return result;
    }

    return {ProcedureStepResult::Action::Continue};
}

void CallSetupMTPercedure::cancel() noexcept {
    static_cast<ProcedureStateMixin<CallSetupMTPercedure, State>&>(*this).doCancel();
}

} // namespace gsml3parser
