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

#include "gsml3parser/stack/procedures/channel_assignment.h"

#include "gsml3parser/stack/subscriber_registry.h"
#include "gsml3parser/rr/l3rrmessages.h"

namespace gsml3parser {

ChannelAssignmentProcedure::ChannelAssignmentProcedure(ChannelType target)
    : mTargetType(target) {}

procedure::ProcedureType ChannelAssignmentProcedure::type() const {
    return procedure::ProcedureType::ChannelAssignment;
}

procedure::ProcedureState ChannelAssignmentProcedure::state() const {
    return mProcState;
}

void ChannelAssignmentProcedure::doTransitionTo(State s) {
    mCurrentState = s;
    if (s == State::COMPLETED) mProcState = procedure::ProcedureState::Completed;
    else if (s == State::FAILED) mProcState = procedure::ProcedureState::Failed;
    else mProcState = procedure::ProcedureState::InProgress;
}

void ChannelAssignmentProcedure::doFail(std::string_view reason) {
    (void)reason;
    mCurrentState = State::FAILED;
    mProcState = procedure::ProcedureState::Failed;
}

void ChannelAssignmentProcedure::doComplete() {
    mCurrentState = State::COMPLETED;
    mProcState = procedure::ProcedureState::Completed;
}

ProcedureStepResult ChannelAssignmentProcedure::feed(const ParsedMessage& msg,
    SubscriberSession* session, ResponseSink sink) {
    (void)session;
    ProcedureStepResult result;

    auto pd = messagePD(msg);
    auto mti = messageMTI(msg);

    switch (mCurrentState) {
        case State::INIT:
            if (pd == L3PD::RadioResource &&
                (mti == L3ChannelRequest::MTI || mti == L3PagingResponse::MTI)) {
                transitionTo(State::ALLOCATE_CHANNEL);
            }
            break;

        case State::ALLOCATE_CHANNEL:
            transitionTo(State::SEND_IMMEDIATE_ASSIGNMENT);
            result.action = ProcedureStepResult::Action::SendResponseWithToken;
            result.responseToken = ResponseToken::ImmediateAssignment;
            startTimer(L3TimerId::T3101, std::chrono::milliseconds(3000));
            if (sink) sink(SMAction::SendResponse, msg, session);
            break;

        case State::SEND_IMMEDIATE_ASSIGNMENT:
            transitionTo(State::WAIT_SEIZURE);
            break;

        case State::WAIT_SEIZURE:
            complete();
            result.action = ProcedureStepResult::Action::Completed;
            result.finalResult = {type(), mProcState, "channel_seized"};
            break;

        case State::COMPLETED:
        case State::FAILED:
            break;
    }

    return result;
}

ProcedureStepResult ChannelAssignmentProcedure::tick(std::chrono::milliseconds delta) {
    return static_cast<ProcedureStateMixin<ChannelAssignmentProcedure, State>&>(*this).doTick(delta);
}

void ChannelAssignmentProcedure::cancel() noexcept {
    static_cast<ProcedureStateMixin<ChannelAssignmentProcedure, State>&>(*this).doCancel();
}

} // namespace gsml3parser
