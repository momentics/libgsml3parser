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

void CipheringModeProcedure::doTransitionTo(State s) {
    mCurrentState = s;
    if (s == State::COMPLETED) mProcState = procedure::ProcedureState::Completed;
    else if (s == State::FAILED) mProcState = procedure::ProcedureState::Failed;
    else mProcState = procedure::ProcedureState::InProgress;
}

void CipheringModeProcedure::doFail(std::string_view reason) {
    (void)reason;
    mCurrentState = State::FAILED;
    mProcState = procedure::ProcedureState::Failed;
}

void CipheringModeProcedure::doComplete() {
    mCurrentState = State::COMPLETED;
    mProcState = procedure::ProcedureState::Completed;
}

ProcedureStepResult CipheringModeProcedure::feed(const ParsedMessage& msg,
    SubscriberSession* session, ResponseSink sink) {
    (void)session;
    ProcedureStepResult result;

    auto pd = messagePD(msg);
    auto mti = messageMTI(msg);

    switch (mCurrentState) {
        case State::INIT:
            break;

        case State::SEND_COMMAND:
            result.action = ProcedureStepResult::Action::SendResponseWithToken;
            result.responseToken = ResponseToken::CipheringModeCommand;
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

ProcedureStepResult CipheringModeProcedure::feedExternalTyped(
    const ExternalData& data, SubscriberSession* session, ResponseSink sink) {
    ProcedureStepResult result;

    if (const auto* params = std::get_if<CipheringParameters>(&data)) {
        if (mCurrentState == State::INIT) {
            transitionTo(State::SEND_COMMAND);
            result.action = ProcedureStepResult::Action::SendResponseWithToken;
            result.responseToken = ResponseToken::CipheringModeCommand;
            startTimer(L3TimerId::T3101, std::chrono::milliseconds(3000));
            // Expose the ciphering algorithm on the session (real parameter).
            if (session) {
                session->response.cipherAlgo = params->algorithmSelector;
                session->response.hasCipherAlgo = true;
            }
            if (sink) sink(SMAction::SendResponse, ParsedMessage{RRM{L3ChannelRequest{}}}, nullptr);
        }
    }

    return result;
}

ProcedureStepResult CipheringModeProcedure::tick(std::chrono::milliseconds delta) {
    return static_cast<ProcedureStateMixin<CipheringModeProcedure, State>&>(*this).doTick(delta);
}

void CipheringModeProcedure::cancel() noexcept {
    static_cast<ProcedureStateMixin<CipheringModeProcedure, State>&>(*this).doCancel();
}

} // namespace gsml3parser
