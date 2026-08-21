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

#include "gsml3parser/stack/procedures/call_release.h"

#include "gsml3parser/stack/subscriber_registry.h"
#include "gsml3parser/stack/response_builder.h"
#include "gsml3parser/cc/l3ccmessages.h"

namespace gsml3parser {

CallReleaseProcedure::CallReleaseProcedure(uint8_t ti, CCCause cause)
    : mTI(ti), mCause(cause) {}

procedure::ProcedureType CallReleaseProcedure::type() const {
    return procedure::ProcedureType::CallRelease;
}

procedure::ProcedureState CallReleaseProcedure::state() const {
    return mProcState;
}

uint8_t CallReleaseProcedure::ti() const noexcept {
    return mTI;
}

CCCause CallReleaseProcedure::cause() const noexcept {
    return mCause;
}

void CallReleaseProcedure::doTransitionTo(State s) {
    mCurrentState = s;
    if (s == State::COMPLETED) mProcState = procedure::ProcedureState::Completed;
    else if (s == State::FAILED) mProcState = procedure::ProcedureState::Failed;
    else mProcState = procedure::ProcedureState::InProgress;
}

void CallReleaseProcedure::doFail(std::string_view reason) {
    (void)reason;
    mCurrentState = State::FAILED;
    mProcState = procedure::ProcedureState::Failed;
}

void CallReleaseProcedure::doComplete() {
    mCurrentState = State::COMPLETED;
    mProcState = procedure::ProcedureState::Completed;
}

ProcedureStepResult CallReleaseProcedure::feed(const ParsedMessage& msg,
    SubscriberSession* session, ResponseSink sink) {
    ProcedureStepResult result;

    // Expose the release parameters on the session (real TI/cause) so the
    // builder sends the correct Disconnect / ReleaseComplete (no fabrication).
    if (session) {
        session->response.ti = mTI;
        session->response.ccCause = mCause;
    }

    switch (mCurrentState) {
        case State::INIT: {
            auto pd = messagePD(msg);
            if (pd == L3PD::CallControl) {
                transitionTo(State::SEND_DISCONNECT);
            } else {
                result.action = ProcedureStepResult::Action::Continue;
                break;
            }
            [[fallthrough]];
        }

        case State::SEND_DISCONNECT: {
            result.action = ProcedureStepResult::Action::SendResponseWithToken;
            result.responseToken = ResponseToken::Disconnect;
            if (sink) sink(SMAction::SendResponse, msg, session);
            transitionTo(State::WAIT_RELEASE);
            startTimer(L3TimerId::T3101, std::chrono::milliseconds(12000));
            break;
        }

        case State::WAIT_RELEASE: {
            auto pd = messagePD(msg);
            auto mti = messageMTI(msg);
            if (pd == L3PD::CallControl && mti == L3Release::MTI) {
                result.action = ProcedureStepResult::Action::SendResponseWithToken;
                result.responseToken = ResponseToken::ReleaseComplete;
                if (sink) sink(SMAction::SendResponse, msg, session);
                complete();
                result.action = ProcedureStepResult::Action::Completed;
                result.finalResult = {type(), mProcState, "release_complete_sent"};
            }
            break;
        }

        case State::SEND_RELEASE_COMPLETE: {
            result.action = ProcedureStepResult::Action::SendResponseWithToken;
            result.responseToken = ResponseToken::ReleaseComplete;
            if (sink) sink(SMAction::SendResponse, msg, session);
            complete();
            result.action = ProcedureStepResult::Action::Completed;
            result.finalResult = {type(), mProcState, "release_complete_sent"};
            break;
        }

        case State::COMPLETED:
        case State::FAILED:
            break;
    }

    if (mProcState == procedure::ProcedureState::Completed) {
        result.action = ProcedureStepResult::Action::Completed;
        result.finalResult = {type(), mProcState, "ok"};
    } else if (mProcState == procedure::ProcedureState::Failed) {
        result.action = ProcedureStepResult::Action::Failed;
        result.finalResult = {type(), mProcState, "procedure_failed"};
    }

    return result;
}

ProcedureStepResult CallReleaseProcedure::feedExternalTyped(
    const ExternalData& data, SubscriberSession* session, ResponseSink sink) {
    (void)data;
    (void)session;
    (void)sink;
    return {ProcedureStepResult::Action::Continue};
}

ProcedureStepResult CallReleaseProcedure::tick(std::chrono::milliseconds delta) {
    return static_cast<ProcedureStateMixin<CallReleaseProcedure, State>&>(*this).doTick(delta);
}

void CallReleaseProcedure::cancel() noexcept {
    static_cast<ProcedureStateMixin<CallReleaseProcedure, State>&>(*this).doCancel();
}

} // namespace gsml3parser
