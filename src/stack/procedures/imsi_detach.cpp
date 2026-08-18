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

#include "gsml3parser/stack/procedures/imsi_detach.h"

#include "gsml3parser/stack/subscriber_registry.h"
#include "gsml3parser/stack/response_builder.h"
#include "gsml3parser/mm/l3mmmessages.h"

namespace gsml3parser {

procedure::ProcedureType IMSIDetachProcedure::type() const {
    return procedure::ProcedureType::IMSIDetach;
}

procedure::ProcedureState IMSIDetachProcedure::state() const {
    return mProcState;
}

void IMSIDetachProcedure::doTransitionTo(State s) {
    mCurrentState = s;
    if (s == State::COMPLETED) mProcState = procedure::ProcedureState::Completed;
    else if (s == State::FAILED) mProcState = procedure::ProcedureState::Failed;
    else mProcState = procedure::ProcedureState::InProgress;
}

void IMSIDetachProcedure::doFail(std::string_view reason) {
    (void)reason;
    mCurrentState = State::FAILED;
    mProcState = procedure::ProcedureState::Failed;
}

void IMSIDetachProcedure::doComplete() {
    mCurrentState = State::COMPLETED;
    mProcState = procedure::ProcedureState::Completed;
}

ProcedureStepResult IMSIDetachProcedure::feed(const ParsedMessage& msg,
    SubscriberSession* session, ResponseSink sink) {
    ProcedureStepResult result;

    switch (mCurrentState) {
        case State::INIT: {
            auto pd = messagePD(msg);
            if (pd == L3PD::MobilityManagement) {
                transitionTo(State::SEND_CM_SERVICE_ACCEPT);
            } else {
                result.action = ProcedureStepResult::Action::Continue;
                break;
            }
            [[fallthrough]];
        }

        case State::SEND_CM_SERVICE_ACCEPT: {
            result.action = ProcedureStepResult::Action::SendResponseWithToken;
            result.responseToken = ResponseToken::CMServiceAccept;
            if (sink) sink(SMAction::SendResponse, msg, session);
            transitionTo(State::WAIT_DETACH_COMPLETE);
            startTimer(L3TimerId::T3112, std::chrono::milliseconds(5000));
            break;
        }

        case State::WAIT_DETACH_COMPLETE: {
            auto pd = messagePD(msg);
            if (pd == L3PD::MobilityManagement) {
                // Any MM message after CMServiceAccept for IMSI detach is treated as complete
                complete();
                result.action = ProcedureStepResult::Action::Completed;
                result.finalResult = {type(), mProcState, "detach_complete"};
            }
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

ProcedureStepResult IMSIDetachProcedure::feedExternalTyped(
    const ExternalData& data, ResponseSink sink) {
    (void)data;
    (void)sink;
    return {ProcedureStepResult::Action::Continue};
}

ProcedureStepResult IMSIDetachProcedure::tick(std::chrono::milliseconds delta) {
    return static_cast<ProcedureStateMixin<IMSIDetachProcedure, State>&>(*this).doTick(delta);
}

void IMSIDetachProcedure::cancel() noexcept {
    static_cast<ProcedureStateMixin<IMSIDetachProcedure, State>&>(*this).doCancel();
}

} // namespace gsml3parser
