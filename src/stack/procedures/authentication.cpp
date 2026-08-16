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

#include "gsml3parser/stack/procedures/authentication.h"

#include <cstring>

#include "gsml3parser/stack/subscriber_registry.h"
#include "gsml3parser/mm/l3mmmessages.h"
#include "gsml3parser/rr/l3rrmessages.h"

namespace gsml3parser {

procedure::ProcedureType AuthenticationProcedure::type() const {
    return procedure::ProcedureType::Authentication;
}

procedure::ProcedureState AuthenticationProcedure::state() const {
    return mProcState;
}

void AuthenticationProcedure::transitionTo(State s) {
    mCurrentState = s;
    if (s == State::COMPLETED) {
        mProcState = procedure::ProcedureState::Completed;
    } else if (s == State::FAILED) {
        mProcState = procedure::ProcedureState::Failed;
    } else {
        mProcState = procedure::ProcedureState::InProgress;
    }
}

void AuthenticationProcedure::fail(const std::string_view& reason) {
    (void)reason;
    stopTimer();
    transitionTo(State::FAILED);
}

void AuthenticationProcedure::complete() {
    stopTimer();
    transitionTo(State::COMPLETED);
}

void AuthenticationProcedure::startTimer(L3TimerId id, std::chrono::milliseconds duration) {
    mCurrentTimer = id;
    mTimerRemaining = duration;
    mTimerRunning = true;
}

void AuthenticationProcedure::stopTimer() noexcept {
    mTimerRunning = false;
    mCurrentTimer = L3TimerId::Unknown;
    mTimerRemaining = std::chrono::milliseconds(0);
}

ProcedureStepResult AuthenticationProcedure::feed(const ParsedMessage& msg,
    SubscriberSession* session, ResponseSink&& sink) {
    (void)session;
    ProcedureStepResult result;

    switch (mCurrentState) {
        case State::INIT:
            // Wait for feedExternal to provide RAND+SRES first
            break;

        case State::SEND_AUTH_REQ: {
            result.action = ProcedureStepResult::Action::SendResponse;
            startTimer(L3TimerId::T3106, std::chrono::milliseconds(3000));
            transitionTo(State::WAIT_RESPONSE);
            if (sink) {
                sink(SMAction::SendResponse, msg, session);
            }
            break;
        }

        case State::WAIT_RESPONSE: {
            auto pd = messagePD(msg);
            auto mti = messageMTI(msg);
            if (pd == L3PD::MobilityManagement && mti == L3AuthenticationResponse::MTI) {
                transitionTo(State::VERIFY_SRES);

                if (mHasExpectedSRES) {
                    const auto* authResp = tryGet<L3AuthenticationResponse>(msg);
                    if (authResp) {
                        uint32_t expected = mExpectedSRES[0] | (mExpectedSRES[1] << 8) |
                                           (mExpectedSRES[2] << 16) | (mExpectedSRES[3] << 24);
                        if (authResp->sres() == expected) {
                            complete();
                            result.action = ProcedureStepResult::Action::Completed;
                            result.finalResult = {type(), mProcState, "auth_success"};
                        } else {
                            fail("sres_mismatch");
                            result.action = ProcedureStepResult::Action::Failed;
                            result.finalResult = {type(), mProcState, "sres_mismatch"};
                        }
                    } else {
                        fail("invalid_response");
                        result.action = ProcedureStepResult::Action::Failed;
                        result.finalResult = {type(), mProcState, "invalid_response"};
                    }
                } else {
                    // No expected SRES — trust the response (for testing)
                    complete();
                    result.action = ProcedureStepResult::Action::Completed;
                    result.finalResult = {type(), mProcState, "auth_no_verify"};
                }
            }
            break;
        }

        case State::VERIFY_SRES:
            // Handled in WAIT_RESPONSE
            break;

        case State::COMPLETED:
        case State::FAILED:
            break;
    }

    return result;
}

ProcedureStepResult AuthenticationProcedure::feedExternal(
    std::span<const uint8_t> data, ResponseSink&& sink) {
    ProcedureStepResult result;

    if (data.size() >= 16) {
        std::memcpy(mRandBuffer.data(), data.data(), std::min(data.size(), mRandBuffer.size()));
        mHasRand = true;
    }
    if (data.size() >= 20) {
        std::memcpy(mExpectedSRES.data(), data.data() + 16, 4);
        mHasExpectedSRES = true;
    }

    if (mCurrentState == State::INIT && mHasRand) {
        transitionTo(State::SEND_AUTH_REQ);
        result.action = ProcedureStepResult::Action::SendResponse;
        if (sink) {
            sink(SMAction::SendResponse, ParsedMessage{RRM{L3ChannelRequest{}}}, nullptr);
        }
    }

    return result;
}

ProcedureStepResult AuthenticationProcedure::tick(std::chrono::milliseconds delta) {
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

void AuthenticationProcedure::cancel() noexcept {
    stopTimer();
    fail("cancelled");
}

} // namespace gsml3parser
