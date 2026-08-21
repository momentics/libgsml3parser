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

#include "gsml3parser/stack/procedures/paging.h"

#include "gsml3parser/stack/subscriber_registry.h"
#include "gsml3parser/rr/l3rrmessages.h"

namespace gsml3parser {

PagingProcedure::PagingProcedure(L3MobileIdentity identity)
    : mIdentity(std::move(identity)) {}

procedure::ProcedureType PagingProcedure::type() const {
    return procedure::ProcedureType::Paging;
}

procedure::ProcedureState PagingProcedure::state() const {
    return mProcState;
}

void PagingProcedure::doTransitionTo(State s) {
    mCurrentState = s;
    if (s == State::COMPLETED) mProcState = procedure::ProcedureState::Completed;
    else if (s == State::FAILED) mProcState = procedure::ProcedureState::Failed;
    else mProcState = procedure::ProcedureState::InProgress;
}

void PagingProcedure::doFail(std::string_view reason) {
    (void)reason;
    mCurrentState = State::FAILED;
    mProcState = procedure::ProcedureState::Failed;
}

void PagingProcedure::doComplete() {
    mCurrentState = State::COMPLETED;
    mProcState = procedure::ProcedureState::Completed;
}

ProcedureStepResult PagingProcedure::feed(const ParsedMessage& msg,
    SubscriberSession* session, ResponseSink sink) {
    (void)session;
    ProcedureStepResult result;

    auto pd = messagePD(msg);
    auto mti = messageMTI(msg);

    switch (mCurrentState) {
        case State::INIT:
            break;

        case State::SEND_PAGE1: {
            result.action = ProcedureStepResult::Action::SendResponseWithToken;
            result.responseToken = ResponseToken::PagingRequestType1;
            startTimer(L3TimerId::T3109, std::chrono::milliseconds(5000));
            transitionTo(State::WAIT_PAGE1);
            mPageAttempt = 1;
            if (sink) sink(SMAction::SendResponse, msg, session);
            break;
        }

        case State::WAIT_PAGE1:
            if (pd == L3PD::RadioResource && mti == L3PagingResponse::MTI) {
                complete();
                result.action = ProcedureStepResult::Action::Completed;
                result.finalResult = {type(), mProcState, "page_response_received"};
            }
            break;

        case State::SEND_PAGE2: {
            result.action = ProcedureStepResult::Action::SendResponseWithToken;
            result.responseToken = ResponseToken::PagingRequestType2;
            startTimer(L3TimerId::T3109, std::chrono::milliseconds(5000));
            transitionTo(State::WAIT_PAGE2);
            mPageAttempt = 2;
            if (sink) sink(SMAction::SendResponse, msg, session);
            break;
        }

        case State::WAIT_PAGE2:
            if (pd == L3PD::RadioResource && mti == L3PagingResponse::MTI) {
                complete();
                result.action = ProcedureStepResult::Action::Completed;
                result.finalResult = {type(), mProcState, "page_response_received"};
            }
            break;

        case State::SEND_PAGE3: {
            result.action = ProcedureStepResult::Action::SendResponseWithToken;
            result.responseToken = ResponseToken::PagingRequestType3;
            startTimer(L3TimerId::T3109, std::chrono::milliseconds(5000));
            transitionTo(State::WAIT_PAGE3);
            mPageAttempt = 3;
            if (sink) sink(SMAction::SendResponse, msg, session);
            break;
        }

        case State::WAIT_PAGE3:
            if (pd == L3PD::RadioResource && mti == L3PagingResponse::MTI) {
                complete();
                result.action = ProcedureStepResult::Action::Completed;
                result.finalResult = {type(), mProcState, "page_response_received"};
            }
            break;

        case State::COMPLETED:
        case State::FAILED:
            break;
    }

    return result;
}

ProcedureStepResult PagingProcedure::feedExternalTyped(
    const ExternalData& data, SubscriberSession* session, ResponseSink sink) {
    ProcedureStepResult result;

    if (const auto* trigger = std::get_if<PagingTrigger>(&data)) {
        if (mCurrentState == State::INIT) {
            transitionTo(State::SEND_PAGE1);
            result.action = ProcedureStepResult::Action::SendResponseWithToken;
            result.responseToken = ResponseToken::PagingRequestType1;
            startTimer(L3TimerId::T3109, std::chrono::milliseconds(5000));
            mPageAttempt = 1;
            // Expose the paged identity on the session (real parameter).
            if (session) {
                session->response.identity = trigger->identity;
                session->response.hasIdentity = true;
            }
            if (sink) sink(SMAction::SendResponse, ParsedMessage{RRM{L3PagingRequestType1{}}}, nullptr);
        }
    }

    return result;
}

ProcedureStepResult PagingProcedure::tick(std::chrono::milliseconds delta) {
    if (!mTimerRunning) {
        return {ProcedureStepResult::Action::Continue};
    }

    mTimerRemaining -= delta;
    if (mTimerRemaining <= std::chrono::milliseconds(0)) {
        stopTimer();

        switch (mCurrentState) {
            case State::WAIT_PAGE1:
                transitionTo(State::SEND_PAGE2);
                return {ProcedureStepResult::Action::Continue};
            case State::WAIT_PAGE2:
                transitionTo(State::SEND_PAGE3);
                return {ProcedureStepResult::Action::Continue};
            case State::WAIT_PAGE3:
                fail("no_page_response");
                break;
            default:
                break;
        }

        if (mProcState == procedure::ProcedureState::Failed) {
            ProcedureStepResult result2;
            result2.action = ProcedureStepResult::Action::Failed;
            result2.finalResult = {type(), mProcState, "no_page_response"};
            return result2;
        }

        return {ProcedureStepResult::Action::Continue};
    }

    return {ProcedureStepResult::Action::Continue};
}

void PagingProcedure::cancel() noexcept {
    static_cast<ProcedureStateMixin<PagingProcedure, State>&>(*this).doCancel();
}

} // namespace gsml3parser
