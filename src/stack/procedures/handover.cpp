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

#include "gsml3parser/stack/procedures/handover.h"

#include "gsml3parser/stack/subscriber_registry.h"
#include "gsml3parser/rr/l3rrmessages.h"

namespace gsml3parser {

HandoverProcedure::HandoverProcedure(L3ChannelDescription target)
    : mTargetChannel(std::move(target)) {}

procedure::ProcedureType HandoverProcedure::type() const {
    return procedure::ProcedureType::Handover;
}

procedure::ProcedureState HandoverProcedure::state() const {
    return mProcState;
}

bool HandoverProcedure::matches(const ParsedMessage& msg) const {
    // This procedure waits for the MS Handover Complete or Handover Failure
    // (TS 04.08 9.1.40).
    if (messagePD(msg) != L3PD::RadioResource) return false;
    const int mti = messageMTI(msg);
    return mti == L3HandoverComplete::MTI || mti == L3HandoverFailure::MTI;
}

void HandoverProcedure::doTransitionTo(State s) {
    mCurrentState = s;
    if (s == State::COMPLETED) mProcState = procedure::ProcedureState::Completed;
    else if (s == State::FAILED) mProcState = procedure::ProcedureState::Failed;
    else mProcState = procedure::ProcedureState::InProgress;
}

void HandoverProcedure::doFail(std::string_view reason) {
    (void)reason;
    mCurrentState = State::FAILED;
    mProcState = procedure::ProcedureState::Failed;
}

void HandoverProcedure::doComplete() {
    mCurrentState = State::COMPLETED;
    mProcState = procedure::ProcedureState::Completed;
}

ProcedureStepResult HandoverProcedure::feed(const ParsedMessage& msg,
    SubscriberSession* session, ResponseSink sink) {
    (void)session;
    ProcedureStepResult result;

    auto pd = messagePD(msg);
    auto mti = messageMTI(msg);

    switch (mCurrentState) {
        case State::INIT:
            break;

        case State::SEND_HO_CMD:
            result.action = ProcedureStepResult::Action::SendResponseWithToken;
            result.responseToken = ResponseToken::HandoverCommand;
            startTimer(L3TimerId::T3101, std::chrono::milliseconds(3000));
            transitionTo(State::WAIT_HO_COMPLETE);
            if (sink) sink(SMAction::SendResponse, msg, session);
            break;

        case State::WAIT_HO_COMPLETE:
            if (pd == L3PD::RadioResource && mti == L3HandoverComplete::MTI) {
                complete();
                result.action = ProcedureStepResult::Action::Completed;
                result.finalResult = {type(), mProcState, "handover_complete"};
            } else if (pd == L3PD::RadioResource && mti == L3HandoverFailure::MTI) {
                fail("ho_failure_from_ms");
                result.action = ProcedureStepResult::Action::Failed;
                result.finalResult = {type(), mProcState, "handover_failed"};
            }
            break;

        case State::COMPLETED:
        case State::FAILED:
            break;
    }

    return result;
}

ProcedureStepResult HandoverProcedure::feedExternalTyped(
    const ExternalData& data, SubscriberSession* session, ResponseSink sink) {
    ProcedureStepResult result;

    // The sink is never invoked here: feedExternalTyped has no incoming L3 message,
    // so the response is signaled solely by the token in the returned result
    // (see response_sink.h).
    (void)sink;

    if (const auto* target = std::get_if<HandoverTarget>(&data)) {
        if (mCurrentState == State::INIT) {
            transitionTo(State::SEND_HO_CMD);
            result.action = ProcedureStepResult::Action::SendResponseWithToken;
            result.responseToken = ResponseToken::HandoverCommand;
            startTimer(L3TimerId::T3101, std::chrono::milliseconds(3000));
            // Expose the handover target channel on the session (real parameter).
            if (session) {
                session->response.hoChannel = target->channel;
                session->response.hasHoChannel = true;
            }
        }
    }

    return result;
}

ProcedureStepResult HandoverProcedure::tick(std::chrono::milliseconds delta) {
    return static_cast<ProcedureStateMixin<HandoverProcedure, State>&>(*this).doTick(delta);
}

void HandoverProcedure::cancel() noexcept {
    static_cast<ProcedureStateMixin<HandoverProcedure, State>&>(*this).doCancel();
}

} // namespace gsml3parser
