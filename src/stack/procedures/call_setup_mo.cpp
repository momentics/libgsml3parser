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

#include "gsml3parser/stack/procedures/call_setup_mo.h"

#include "gsml3parser/stack/subscriber_registry.h"
#include "gsml3parser/cc/l3ccmessages.h"
#include "gsml3parser/mm/l3mmmessages.h"
#include "gsml3parser/rr/l3rrmessages.h"

namespace gsml3parser {

procedure::ProcedureType CallSetupMOPercedure::type() const {
    return procedure::ProcedureType::CallSetup_MO;
}

procedure::ProcedureState CallSetupMOPercedure::state() const {
    return mProcState;
}

void CallSetupMOPercedure::transitionTo(State s) {
    mCurrentState = s;
    if (s == State::COMPLETED) {
        mProcState = procedure::ProcedureState::Completed;
    } else if (s == State::FAILED) {
        mProcState = procedure::ProcedureState::Failed;
    } else {
        mProcState = procedure::ProcedureState::InProgress;
    }
}

void CallSetupMOPercedure::fail(const std::string_view& reason) {
    stopTimer();
    transitionTo(State::FAILED);
}

void CallSetupMOPercedure::complete() {
    stopTimer();
    transitionTo(State::COMPLETED);
}

void CallSetupMOPercedure::startTimer(L3TimerId id, std::chrono::milliseconds duration) {
    mCurrentTimer = id;
    mTimerRemaining = duration;
    mTimerRunning = true;
}

void CallSetupMOPercedure::stopTimer() noexcept {
    mTimerRunning = false;
    mCurrentTimer = L3TimerId::Unknown;
    mTimerRemaining = std::chrono::milliseconds(0);
}

ProcedureStepResult CallSetupMOPercedure::feed(const ParsedMessage& msg,
    SubscriberSession* session, ResponseSink&& sink) {
    (void)session;
    ProcedureStepResult result;

    auto pd = messagePD(msg);
    auto mti = messageMTI(msg);

    switch (mCurrentState) {
        case State::INIT:
            // Accept CMServiceRequest or Setup directly (auto-created from Setup by ProcedureRunner).
            if (pd == L3PD::MobilityManagement && mti == L3CMServiceRequest::MTI) {
                transitionTo(State::SERVICE_ACCEPT);
                result.action = ProcedureStepResult::Action::SendResponse;
                if (sink) sink(SMAction::SendResponse, msg, session);
            } else if (pd == L3PD::CallControl && mti == L3Setup::MTI) {
                const auto* setup = tryGet<L3Setup>(msg);
                if (setup) mTI = static_cast<uint8_t>(setup->ti());
                startTimer(L3TimerId::T3101, std::chrono::milliseconds(3000));
                transitionTo(State::PROCEEDING);
                result.action = ProcedureStepResult::Action::SendResponse;
                if (sink) sink(SMAction::SendResponse, msg, session);
            }
            break;

        case State::SERVICE_ACCEPT:
            transitionTo(State::WAIT_SETUP);
            break;

        case State::WAIT_SETUP:
            if (pd == L3PD::CallControl && mti == L3Setup::MTI) {
                const auto* setup = tryGet<L3Setup>(msg);
                if (setup) mTI = static_cast<uint8_t>(setup->ti());
                startTimer(L3TimerId::T3101, std::chrono::milliseconds(3000));
                transitionTo(State::PROCEEDING);
                result.action = ProcedureStepResult::Action::SendResponse;
                if (sink) sink(SMAction::SendResponse, msg, session);
            }
            break;

        case State::PROCEEDING:
            transitionTo(State::ASSIGN_TCH);
            result.action = ProcedureStepResult::Action::SendResponse;
            startTimer(L3TimerId::T3101, std::chrono::milliseconds(3000));
            if (sink) sink(SMAction::SendResponse, msg, session);
            break;

        case State::ASSIGN_TCH:
            transitionTo(State::WAIT_ASSIGN_COMPLETE);
            break;

        case State::WAIT_ASSIGN_COMPLETE:
            if (pd == L3PD::RadioResource && mti == L3AssignmentComplete::MTI) {
                transitionTo(State::ALERTING);
                result.action = ProcedureStepResult::Action::SendResponse;
                if (sink) sink(SMAction::SendResponse, msg, session);
            }
            break;

        case State::ALERTING:
            transitionTo(State::CONNECT);
            result.action = ProcedureStepResult::Action::SendResponse;
            if (sink) sink(SMAction::SendResponse, msg, session);
            break;

        case State::CONNECT:
            transitionTo(State::ACTIVE);
            result.action = ProcedureStepResult::Action::SendResponse;
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

ProcedureStepResult CallSetupMOPercedure::tick(std::chrono::milliseconds delta) {
    if (!mTimerRunning) {
        return {ProcedureStepResult::Action::Continue};
    }

    mTimerRemaining -= delta;
    if (mTimerRemaining <= std::chrono::milliseconds(0)) {
        stopTimer();
        fail("timer_expired");
        ProcedureStepResult result;
        result.action = ProcedureStepResult::Action::Failed;
        result.finalResult = {type(), mProcState, "timer_expired"};
        return result;
    }

    return {ProcedureStepResult::Action::Continue};
}

void CallSetupMOPercedure::cancel() noexcept {
    stopTimer();
    fail("cancelled");
}

} // namespace gsml3parser
