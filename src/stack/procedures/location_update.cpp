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

#include "gsml3parser/stack/procedures/location_update.h"

#include <cstring>

#include "gsml3parser/stack/subscriber_registry.h"
#include "gsml3parser/stack/response_builder.h"
#include "gsml3parser/mm/l3mmmessages.h"
#include "gsml3parser/rr/l3rrmessages.h"

namespace gsml3parser {

procedure::ProcedureType LocationUpdateProcedure::type() const {
    return procedure::ProcedureType::LocationUpdate;
}

procedure::ProcedureState LocationUpdateProcedure::state() const {
    return mProcState;
}

void LocationUpdateProcedure::transitionTo(State s) {
    mCurrentState = s;
    if (s == State::COMPLETED) {
        mProcState = procedure::ProcedureState::Completed;
    } else if (s == State::FAILED) {
        mProcState = procedure::ProcedureState::Failed;
    } else if (s == State::WAITING_EXTERNAL) {
        mProcState = procedure::ProcedureState::WaitingExternal;
    } else {
        mProcState = procedure::ProcedureState::InProgress;
    }
}

void LocationUpdateProcedure::fail(const std::string_view& reason) {
    stopTimer();
    transitionTo(State::FAILED);
}

void LocationUpdateProcedure::complete() {
    stopTimer();
    transitionTo(State::COMPLETED);
}

void LocationUpdateProcedure::startTimer(L3TimerId id, std::chrono::milliseconds duration) {
    mCurrentTimer = id;
    mTimerRemaining = duration;
    mTimerRunning = true;
}

void LocationUpdateProcedure::stopTimer() noexcept {
    mTimerRunning = false;
    mCurrentTimer = L3TimerId::Unknown;
    mTimerRemaining = std::chrono::milliseconds(0);
}

ProcedureStepResult LocationUpdateProcedure::feed(const ParsedMessage& msg,
    SubscriberSession* session, ResponseSink&& sink) {
    (void)session;

    ProcedureStepResult result;

    switch (mCurrentState) {
        case State::INIT: {
            // Accept CMServiceRequest or PagingResponse to start the procedure
            auto pd = messagePD(msg);
            if (pd == L3PD::MobilityManagement || pd == L3PD::RadioResource) {
                transitionTo(State::IDENTITY_CHECK);
                result.action = ProcedureStepResult::Action::Continue;
            } else {
                result.action = ProcedureStepResult::Action::Continue;
            }
            break;
        }

        case State::IDENTITY_CHECK: {
            // Check if TMSI is known from session context
            if (session && session->context.identity().isTMSI()) {
                transitionTo(State::AUTH_CHECK);
            } else {
                transitionTo(State::REQUEST_IDENTITY);
                result.action = ProcedureStepResult::Action::SendResponse;
                if (sink) {
                    sink(SMAction::SendResponse, msg, session);
                }
            }
            break;
        }

        case State::REQUEST_IDENTITY: {
            auto pd = messagePD(msg);
            auto mti = messageMTI(msg);
            if (pd == L3PD::MobilityManagement && mti == L3IdentityResponse::MTI) {
                transitionTo(State::AUTH_CHECK);
            }
            break;
        }

        case State::AUTH_CHECK: {
            if (mHasRand) {
                transitionTo(State::SEND_AUTH);
                result.action = ProcedureStepResult::Action::SendResponse;
                startTimer(L3TimerId::T3106, std::chrono::milliseconds(3000));
                if (sink) {
                    sink(SMAction::SendResponse, msg, session);
                }
            } else {
                transitionTo(State::LU_REQUEST);
            }
            break;
        }

        case State::SEND_AUTH: {
            // After auth request sent, wait for MS response
            transitionTo(State::WAIT_AUTH);
            break;
        }

        case State::WAIT_AUTH: {
            auto pd = messagePD(msg);
            auto mti = messageMTI(msg);
            if (pd == L3PD::MobilityManagement && mti == L3AuthenticationResponse::MTI) {
                transitionTo(State::VERIFY_AUTH);
                if (mHasExpectedSRES) {
                    const auto* authResp = tryGet<L3AuthenticationResponse>(msg);
                    if (authResp) {
                        uint32_t expected = mExpectedSRES[0] | (mExpectedSRES[1] << 8) |
                                           (mExpectedSRES[2] << 16) | (mExpectedSRES[3] << 24);
                        if (authResp->sres() == expected) {
                            transitionTo(State::LU_REQUEST);
                        } else {
                            mRejectCause = MMRejectCause::MAC_Failure;
                            transitionTo(State::SEND_REJECT);
                            result.action = ProcedureStepResult::Action::SendResponse;
                            if (sink) sink(SMAction::SendResponse, msg, session);
                            break;
                        }
                    } else {
                        mRejectCause = MMRejectCause::MAC_Failure;
                        transitionTo(State::SEND_REJECT);
                        result.action = ProcedureStepResult::Action::SendResponse;
                        if (sink) sink(SMAction::SendResponse, msg, session);
                        break;
                    }
                } else {
                    transitionTo(State::LU_REQUEST);
                }
            }
            break;
        }

        case State::VERIFY_AUTH:
            // Transitions handled in WAIT_AUTH
            break;

        case State::LU_REQUEST: {
            if (session) {
                mLAI = session->context.lai().value_or(L3LocationAreaIdentity{});
            }
            transitionTo(State::WAITING_EXTERNAL);
            startTimer(L3TimerId::T3103, std::chrono::milliseconds(5000));
            break;
        }

        case State::WAITING_EXTERNAL:
            // Waiting for feedExternal() call from BTS application
            break;

        case State::SEND_ACCEPT:
            complete();
            result.action = ProcedureStepResult::Action::Completed;
            result.finalResult = {type(), mProcState, "accept_sent"};
            break;

        case State::SEND_REJECT:
            fail("reject_sent");
            result.action = ProcedureStepResult::Action::Failed;
            result.finalResult = {type(), mProcState, "reject_sent"};
            break;

        case State::COMPLETED:
        case State::FAILED:
            // Terminal states — no further processing
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

ProcedureStepResult LocationUpdateProcedure::feedExternal(
    std::span<const uint8_t> data, ResponseSink&& sink) {
    ProcedureStepResult result;

    // Handle RAND + SRES data (first 16 bytes = RAND, next 4 bytes = expected SRES)
    if (mCurrentState == State::AUTH_CHECK || mCurrentState == State::INIT) {
        if (data.size() >= 16) {
            std::memcpy(mRandBuffer.data(), data.data(), std::min(data.size(), mRandBuffer.size()));
            mHasRand = true;
        }
        if (data.size() >= 20) {
            std::memcpy(mExpectedSRES.data(), data.data() + 16, 4);
            mHasExpectedSRES = true;
        }
        // If we're in AUTH_CHECK and now have RAND, transition to SEND_AUTH
        if (mCurrentState == State::AUTH_CHECK && mHasRand) {
            mCurrentState = State::SEND_AUTH;
            result.action = ProcedureStepResult::Action::SendResponse;
            startTimer(L3TimerId::T3106, std::chrono::milliseconds(3000));
            if (sink) {
                sink(SMAction::SendResponse, ParsedMessage{RRM{L3ChannelRequest{}}}, nullptr);
            }
        }
        return result;
    }

    // Handle VLR Accept/Reject decision
    // Convention: first byte = 0 for Reject, 1 for Accept
    // Remaining bytes: for Accept, optional TMSI (4 bytes) + LAI data
    if (mCurrentState == State::WAITING_EXTERNAL && !data.empty()) {
        bool accept = (data[0] != 0);
        stopTimer();

        if (accept) {
            if (data.size() >= 5) {
                mNewTmsi = static_cast<uint32_t>(data[1]) |
                           (static_cast<uint32_t>(data[2]) << 8) |
                           (static_cast<uint32_t>(data[3]) << 16) |
                           (static_cast<uint32_t>(data[4]) << 24);
            }
            result.action = ProcedureStepResult::Action::SendResponse;
            if (sink) {
                sink(SMAction::SendResponse, ParsedMessage{RRM{L3ChannelRequest{}}}, nullptr);
            }
            complete();
            result.action = ProcedureStepResult::Action::Completed;
            result.finalResult = {type(), mProcState, "vlr_accept"};
        } else {
            if (data.size() >= 2) {
                mRejectCause = static_cast<MMRejectCause>(data[1]);
            }
            transitionTo(State::SEND_REJECT);
            result.action = ProcedureStepResult::Action::SendResponse;
            if (sink) {
                sink(SMAction::SendResponse, ParsedMessage{RRM{L3ChannelRequest{}}}, nullptr);
            }
        }

        if (mProcState == procedure::ProcedureState::Completed) {
            result.action = ProcedureStepResult::Action::Completed;
            result.finalResult = {type(), mProcState, "vlr_accept"};
        } else if (mProcState == procedure::ProcedureState::Failed) {
            result.action = ProcedureStepResult::Action::Failed;
            result.finalResult = {type(), mProcState, "vlr_reject"};
        }

        return result;
    }

    return {ProcedureStepResult::Action::Continue};
}

ProcedureStepResult LocationUpdateProcedure::tick(std::chrono::milliseconds delta) {
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

void LocationUpdateProcedure::cancel() noexcept {
    stopTimer();
    fail("cancelled");
}

} // namespace gsml3parser
