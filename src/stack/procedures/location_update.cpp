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

namespace gsml3parser {

procedure::ProcedureType LocationUpdateProcedure::type() const {
    return procedure::ProcedureType::LocationUpdate;
}

procedure::ProcedureState LocationUpdateProcedure::state() const {
    return mProcState;
}

void LocationUpdateProcedure::doTransitionTo(State s) {
    mCurrentState = s;
    if (s == State::COMPLETED) mProcState = procedure::ProcedureState::Completed;
    else if (s == State::FAILED) mProcState = procedure::ProcedureState::Failed;
    else if (s == State::WAITING_EXTERNAL) mProcState = procedure::ProcedureState::WaitingExternal;
    else mProcState = procedure::ProcedureState::InProgress;
}

void LocationUpdateProcedure::doFail(std::string_view reason) {
    (void)reason;
    mCurrentState = State::FAILED;
    mProcState = procedure::ProcedureState::Failed;
}

void LocationUpdateProcedure::doComplete() {
    mCurrentState = State::COMPLETED;
    mProcState = procedure::ProcedureState::Completed;
}

ProcedureStepResult LocationUpdateProcedure::feed(const ParsedMessage& msg,
    SubscriberSession* session, ResponseSink sink) {
    (void)session;

    ProcedureStepResult result;

    switch (mCurrentState) {
        case State::INIT: {
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
            if (session && session->context.identity().isTMSI()) {
                transitionTo(State::AUTH_CHECK);
            } else {
                transitionTo(State::REQUEST_IDENTITY);
                result.action = ProcedureStepResult::Action::SendResponseWithToken;
                result.responseToken = ResponseToken::IdentityRequest;
                if (sink) sink(SMAction::SendResponse, msg, session);
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
                result.action = ProcedureStepResult::Action::SendResponseWithToken;
                result.responseToken = ResponseToken::AuthenticationRequest;
                startTimer(L3TimerId::T3106, std::chrono::milliseconds(3000));
                if (sink) sink(SMAction::SendResponse, msg, session);
            } else {
                transitionTo(State::LU_REQUEST);
            }
            break;
        }

        case State::SEND_AUTH: {
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
                        // SRES is compared big-endian: expectedSres[0] is the MSB,
                        // matching the 32-bit SRES IE encoding of TS 24.008 10.5.1.22
                        // (first octet on the wire is the most significant).
                        uint32_t expected = (static_cast<uint32_t>(mExpectedSRES[0]) << 24) |
                                           (static_cast<uint32_t>(mExpectedSRES[1]) << 16) |
                                           (static_cast<uint32_t>(mExpectedSRES[2]) << 8) |
                                            static_cast<uint32_t>(mExpectedSRES[3]);
                        if (authResp->sres() == expected) {
                            transitionTo(State::LU_REQUEST);
                        } else {
                            mRejectCause = MMRejectCause::MAC_Failure;
                            transitionTo(State::SEND_REJECT);
                            result.action = ProcedureStepResult::Action::SendResponseWithToken;
                            result.responseToken = ResponseToken::LocationUpdatingReject;
                            if (sink) sink(SMAction::SendResponse, msg, session);
                            break;
                        }
                    } else {
                        mRejectCause = MMRejectCause::MAC_Failure;
                        transitionTo(State::SEND_REJECT);
                        result.action = ProcedureStepResult::Action::SendResponseWithToken;
                        result.responseToken = ResponseToken::LocationUpdatingReject;
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
            break;

        case State::LU_REQUEST: {
            if (session) {
                mLAI = session->context.lai().value_or(L3LocationAreaIdentity{});
            }
            transitionTo(State::WAITING_EXTERNAL);
            result.action = ProcedureStepResult::Action::WaitingExternal;
            startTimer(L3TimerId::T3103, std::chrono::milliseconds(5000));
            break;
        }

        case State::WAITING_EXTERNAL:
            break;

        case State::SEND_ACCEPT: {
            // Terminal with response: keep action == SendResponseWithToken per the
            // response/terminal rule (procedure.h); the terminal state is reported
            // via finalResult only.
            result.action = ProcedureStepResult::Action::SendResponseWithToken;
            result.responseToken = ResponseToken::LocationUpdatingAccept;
            if (sink) sink(SMAction::SendResponse, msg, session);
            complete();
            result.finalResult = {type(), mProcState, "accept_sent"};
            break;
        }

        case State::SEND_REJECT: {
            // Terminal with response (reject): keep SendResponseWithToken; report the
            // terminal Failed state via finalResult only.
            result.action = ProcedureStepResult::Action::SendResponseWithToken;
            result.responseToken = ResponseToken::LocationUpdatingReject;
            if (sink) sink(SMAction::SendResponse, msg, session);
            fail("reject_sent");
            result.finalResult = {type(), mProcState, "reject_sent"};
            break;
        }

        case State::COMPLETED:
        case State::FAILED:
            break;
    }

    // Terminal-state reporting: when the procedure finished WITH a response in this
    // step (responseToken set), keep action == SendResponseWithToken per the
    // response/terminal rule (procedure.h) — only report the terminal state via
    // finalResult. Otherwise report the terminal action itself.
    if (mProcState == procedure::ProcedureState::Completed) {
        if (result.responseToken == ResponseToken::None) {
            result.action = ProcedureStepResult::Action::Completed;
        }
        if (result.finalResult.state == procedure::ProcedureState::Initiated) {
            result.finalResult = {type(), mProcState, "ok"};
        }
    } else if (mProcState == procedure::ProcedureState::Failed) {
        if (result.responseToken == ResponseToken::None) {
            result.action = ProcedureStepResult::Action::Failed;
        }
        if (result.finalResult.state == procedure::ProcedureState::Initiated) {
            result.finalResult = {type(), mProcState, "procedure_failed"};
        }
    }

    return result;
}

ProcedureStepResult LocationUpdateProcedure::feedExternalTyped(
    const ExternalData& data, SubscriberSession* session, ResponseSink sink) {
    // The sink is never invoked here: feedExternalTyped has no incoming L3 message,
    // so the response is signaled solely by the token in the returned result
    // (see response_sink.h).
    (void)sink;
    ProcedureStepResult result;

    std::visit([&](const auto& typedData) {
        using T = std::decay_t<decltype(typedData)>;
        if constexpr (std::is_same_v<T, AuthChallenge>) {
            std::memcpy(mRandBuffer.data(), typedData.rand.data(), 16);
            mHasRand = true;
            std::memcpy(mExpectedSRES.data(), typedData.expectedSres.data(), 4);
            mHasExpectedSRES = true;

            // Expose the RAND on the session (real parameters for the builder).
            if (session) {
                std::memcpy(session->response.rand.data(), typedData.rand.data(), 16);
                session->response.hasRand = true;
            }

            if (mCurrentState == State::AUTH_CHECK && mHasRand) {
                transitionTo(State::SEND_AUTH);
                // The sink is an observability hook invoked only from feed() with the
                // real incoming message; on this external-data path the response is
                // signaled by the token in the result (see response_sink.h).
                result.action = ProcedureStepResult::Action::SendResponseWithToken;
                result.responseToken = ResponseToken::AuthenticationRequest;
                startTimer(L3TimerId::T3106, std::chrono::milliseconds(3000));
            }
        } else if constexpr (std::is_same_v<T, VLRDecision>) {
            if (mCurrentState == State::WAITING_EXTERNAL) {
                stopTimer();
                // Expose the VLR decision on the session so the builder uses the
                // real new TMSI / reject cause (never fabricated values).
                if (session) {
                    session->response.newTmsi = typedData.newTmsi;
                    session->response.mmCause = typedData.rejectCause;
                }
                if (typedData.accept) {
                    mNewTmsi = typedData.newTmsi;
                    transitionTo(State::SEND_ACCEPT);
                    // Terminal with response: keep action == SendResponseWithToken per
                    // the response/terminal rule (procedure.h); the terminal state is
                    // reported via finalResult only. The sink is not invoked on the
                    // external-data path (no incoming L3 message; see response_sink.h).
                    result.action = ProcedureStepResult::Action::SendResponseWithToken;
                    result.responseToken = ResponseToken::LocationUpdatingAccept;
                    complete();
                    result.finalResult = {type(), mProcState, "vlr_accept"};
                } else {
                    mRejectCause = typedData.rejectCause;
                    transitionTo(State::SEND_REJECT);
                    // Terminal with response (reject): keep SendResponseWithToken;
                    // report the terminal Failed state via finalResult only.
                    result.action = ProcedureStepResult::Action::SendResponseWithToken;
                    result.responseToken = ResponseToken::LocationUpdatingReject;
                    fail("vlr_reject");
                    result.finalResult = {type(), mProcState, "vlr_reject"};
                }
            }
        }
        // Other ExternalData types are not handled by this procedure.
    }, data);

    return result;
}

ProcedureStepResult LocationUpdateProcedure::tick(std::chrono::milliseconds delta) {
    return static_cast<ProcedureStateMixin<LocationUpdateProcedure, State>&>(*this).doTick(delta);
}

void LocationUpdateProcedure::cancel() noexcept {
    static_cast<ProcedureStateMixin<LocationUpdateProcedure, State>&>(*this).doCancel();
}

} // namespace gsml3parser
