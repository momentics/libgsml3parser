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

#include "gsml3parser/stack/procedures/ciphering_mode.h"

#include "gsml3parser/stack/subscriber_registry.h"
#include "gsml3parser/rr/l3rrmessages.h"

namespace gsml3parser {

CipheringModeProcedure::CipheringModeProcedure(uint8_t algo)
    : mCipherAlgo(algo) {}

procedure::ProcedureType CipheringModeProcedure::type() const {
    return procedure::ProcedureType::CipheringMode;
}

procedure::ProcedureState CipheringModeProcedure::state() const {
    return mProcState;
}

void CipheringModeProcedure::transitionTo(State s) {
    mCurrentState = s;
    if (s == State::COMPLETED) {
        mProcState = procedure::ProcedureState::Completed;
    } else if (s == State::FAILED) {
        mProcState = procedure::ProcedureState::Failed;
    } else {
        mProcState = procedure::ProcedureState::InProgress;
    }
}

void CipheringModeProcedure::fail(const std::string_view& reason) {
    stopTimer();
    transitionTo(State::FAILED);
}

void CipheringModeProcedure::complete() {
    stopTimer();
    transitionTo(State::COMPLETED);
}

void CipheringModeProcedure::startTimer(L3TimerId id, std::chrono::milliseconds duration) {
    mCurrentTimer = id;
    mTimerRemaining = duration;
    mTimerRunning = true;
}

void CipheringModeProcedure::stopTimer() noexcept {
    mTimerRunning = false;
    mCurrentTimer = L3TimerId::Unknown;
    mTimerRemaining = std::chrono::milliseconds(0);
}

ProcedureStepResult CipheringModeProcedure::feed(const ParsedMessage& msg,
    SubscriberSession* session, ResponseSink&& sink) {
    (void)session;
    ProcedureStepResult result;

    auto pd = messagePD(msg);
    auto mti = messageMTI(msg);

    switch (mCurrentState) {
        case State::INIT:
            // Wait for feedExternal to provide cipher parameters
            break;

        case State::SEND_COMMAND:
            result.action = ProcedureStepResult::Action::SendResponse;
            startTimer(L3TimerId::T3101, std::chrono::milliseconds(3000));
            transitionTo(State::WAIT_COMPLETE);
            if (sink) sink(SMAction::SendResponse, msg, session);
            break;

        case State::WAIT_COMPLETE:
            if (pd == L3PD::RadioResource && mti == L3CipheringModeComplete::MTI) {
                complete();
                result.action = ProcedureStepResult::Action::Completed;
                result.finalResult = {type(), mProcState, "ciphering_activated"};
            }
            break;

        case State::COMPLETED:
        case State::FAILED:
            break;
    }

    return result;
}

ProcedureStepResult CipheringModeProcedure::feedExternal(
    std::span<const uint8_t> data, ResponseSink&& sink) {
    (void)data;
    ProcedureStepResult result;

    if (mCurrentState == State::INIT) {
        transitionTo(State::SEND_COMMAND);
        result.action = ProcedureStepResult::Action::SendResponse;
        startTimer(L3TimerId::T3101, std::chrono::milliseconds(3000));
        if (sink) sink(SMAction::SendResponse, ParsedMessage{RRM{L3ChannelRequest{}}}, nullptr);
    }

    return result;
}

ProcedureStepResult CipheringModeProcedure::tick(std::chrono::milliseconds delta) {
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

void CipheringModeProcedure::cancel() noexcept {
    stopTimer();
    fail("cancelled");
}

} // namespace gsml3parser
