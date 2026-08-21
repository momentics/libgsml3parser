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

#include "gsml3parser/stack/procedure_orchestrator.h"

#include "gsml3parser/stack/subscriber_registry.h"
#include "gsml3parser/stack/response_builder.h"
#include "gsml3parser/stack/procedures/location_update.h"
#include "gsml3parser/stack/procedures/authentication.h"
#include "gsml3parser/stack/procedures/ciphering_mode.h"
#include "gsml3parser/stack/procedures/call_setup_mo.h"
#include "gsml3parser/stack/procedures/call_setup_mt.h"
#include "gsml3parser/stack/procedures/channel_assignment.h"
#include "gsml3parser/stack/procedures/paging.h"
#include "gsml3parser/stack/procedures/handover.h"
#include "gsml3parser/stack/procedures/call_release.h"
#include "gsml3parser/stack/procedures/imsi_detach.h"
#include "gsml3parser/mm/l3mmmessages.h"
#include "gsml3parser/cc/l3ccmessages.h"
#include "gsml3parser/rr/l3rrmessages.h"

namespace gsml3parser {

// ── Phase detection ──────────────────────────────────────────────────────

ProcedureOrchestrator::ChainPhase ProcedureOrchestrator::detectChainPhase(
    const ParsedMessage& msg) const {
    auto pd = messagePD(msg);
    auto mti = messageMTI(msg);

    if (pd == L3PD::MobilityManagement) {
        if (mti == L3CMServiceRequest::MTI) {
            const auto* cmReq = tryGet<L3CMServiceRequest>(msg);
            if (cmReq) {
                switch (cmReq->serviceType()) {
                    case L3CMServiceType::TypeCode::LocationUpdateRequest:
                        return ChainPhase::CMServiceRequest;
                    case L3CMServiceType::TypeCode::MobileOriginatedCall:
                        return ChainPhase::CMServiceRequest;
                    default:
                        break;
                }
            }
        }
        if (mti == L3IMSIDetachIndication::MTI) {
            return ChainPhase::IMSIDetach;
        }
    }

    if (pd == L3PD::CallControl) {
        if (mti == L3Disconnect::MTI) {
            return ChainPhase::CallRelease;
        }
    }

    return ChainPhase::None;
}

// ── Phase transitions ────────────────────────────────────────────────────

void ProcedureOrchestrator::transitionToPhase(ChainPhase phase) {
    mCurrentPhase = phase;

    // Update FSM states based on phase transitions
    if (mSession) {
        switch (phase) {
            case ChainPhase::CMServiceRequest:
                mSession->mmSM.setState(MMStateMachine::State::SERVICE_REQUEST);
                break;
            case ChainPhase::IdentityVerification:
                mSession->mmSM.setState(MMStateMachine::State::WAITING_IDENTITY);
                break;
            case ChainPhase::Authentication:
                mSession->mmSM.setState(MMStateMachine::State::AUTHENTICATION);
                break;
            case ChainPhase::CipheringMode:
                mSession->rrSM.setState(RRStateMachine::State::CIPHER_MODE);
                break;
            case ChainPhase::LocationUpdate:
                mSession->mmSM.setState(MMStateMachine::State::LOCATION_UPDATE);
                break;
            case ChainPhase::CallSetupMO:
                mSession->ccSM.setState(CCStateMachine::State::SETUP_RECEIVED);
                break;
            case ChainPhase::CallSetupMT:
                mSession->ccSM.setState(CCStateMachine::State::SETUP_RECEIVED);
                break;
            case ChainPhase::IMSIDetach:
                mSession->mmSM.setState(MMStateMachine::State::SERVICE_REQUEST);
                break;
            case ChainPhase::CallRelease:
                mSession->ccSM.setState(CCStateMachine::State::DISCONNECT_RECEIVED);
                break;
            default:
                break;
        }
    }

    // Create procedure object for phases that need one
    if (phase == ChainPhase::Authentication ||
        phase == ChainPhase::CipheringMode ||
        phase == ChainPhase::CallSetupMO ||
        phase == ChainPhase::CallSetupMT ||
        phase == ChainPhase::ChannelAssignment ||
        phase == ChainPhase::Paging ||
        phase == ChainPhase::Handover) {
        mCurrentProcedure = createProcedureForPhase(phase);
    } else {
        mCurrentProcedure.reset();
    }
}

std::unique_ptr<Procedure> ProcedureOrchestrator::createProcedureForPhase(ChainPhase phase) {
    switch (phase) {
        case ChainPhase::Authentication:
            return ProcedureFactory::createAuthentication();
        case ChainPhase::CipheringMode:
            return ProcedureFactory::createCipheringMode(0);
        case ChainPhase::CallSetupMO:
            return ProcedureFactory::createCallSetupMO();
        case ChainPhase::CallSetupMT:
            return ProcedureFactory::createCallSetupMT("");
        case ChainPhase::ChannelAssignment:
            return ProcedureFactory::createChannelAssignment(ChannelType::SDCCHType);
        case ChainPhase::Paging:
            return ProcedureFactory::createPaging(L3MobileIdentity{});
        case ChainPhase::Handover:
            return ProcedureFactory::createHandover(L3ChannelDescription{});
        default:
            return nullptr;
    }
}

void ProcedureOrchestrator::onProcedureCompleted(const ProcedureStepResult& result) {
    (void)result;
    switch (mCurrentPhase) {
        case ChainPhase::Authentication:
            if (mSession) mSession->mmSM.setState(MMStateMachine::State::AUTHENTICATED);
            transitionToPhase(ChainPhase::CipheringMode);
            break;
        case ChainPhase::CipheringMode:
            if (mSession) {
                mSession->rrSM.setState(RRStateMachine::State::ACTIVE);
                mSession->context.setCiphered(true);
            }
            transitionToPhase(ChainPhase::LocationUpdate);
            break;
        case ChainPhase::LocationUpdate:
            if (mSession) mSession->mmSM.setState(MMStateMachine::State::REGISTERED);
            break;
        case ChainPhase::CallSetupMO:
        case ChainPhase::CallSetupMT:
            break;
        default:
            break;
    }
}

void ProcedureOrchestrator::onProcedureFailed(const ProcedureStepResult& result) {
    (void)result;
    cancelAll();
}

// ── Public API ───────────────────────────────────────────────────────────

ProcedureStepResult ProcedureOrchestrator::feed(const ParsedMessage& msg,
                                                 SubscriberSession* session) {
    mSession = session;

    // If no active chain, detect the chain from the message
    if (mCurrentPhase == ChainPhase::None) {
        auto detected = detectChainPhase(msg);
        if (detected == ChainPhase::None) {
            return {ProcedureStepResult::Action::Continue};
        }

        // Start a new chain
        mChainType = procedure::ProcedureType::Unknown;

        if (detected == ChainPhase::IMSIDetach) {
            transitionToPhase(ChainPhase::IMSIDetach);
            mChainType = procedure::ProcedureType::IMSIDetach;
            return handleIMSIDetachPhase(msg, session);
        }

        if (detected == ChainPhase::CallRelease) {
            transitionToPhase(ChainPhase::CallRelease);
            mChainType = procedure::ProcedureType::CallRelease;
            return handleCallReleasePhase(msg, session);
        }

        // CMServiceRequest chains: determine type from service request
        auto pd = messagePD(msg);
        auto mti = messageMTI(msg);
        bool isMOCall = false;

        if (pd == L3PD::MobilityManagement && mti == L3CMServiceRequest::MTI) {
            const auto* cmReq = tryGet<L3CMServiceRequest>(msg);
            if (cmReq) {
                if (cmReq->serviceType() == L3CMServiceType::TypeCode::MobileOriginatedCall) {
                    isMOCall = true;
                    mChainType = procedure::ProcedureType::CallSetup_MO;
                } else {
                    mChainType = procedure::ProcedureType::LocationUpdate;
                }
            }
        }

        // Phase 1: CMServiceRequest - send accept, then determine next phase
        transitionToPhase(ChainPhase::CMServiceRequest);
        auto result = handleCMServiceRequest(msg, session);

        // Auto-advance to next phase after CMServiceRequest
        if (isMOCall) {
            transitionToPhase(ChainPhase::CallSetupMO);
        } else {
            // For location update chain: check identity -> auth -> ciphering -> LU
            if (session && session->context.identity().isTMSI()) {
                transitionToPhase(ChainPhase::Authentication);
            } else {
                transitionToPhase(ChainPhase::IdentityVerification);
            }
        }

        return result;
    }

    // Active chain: route to current phase handler
    switch (mCurrentPhase) {
        case ChainPhase::CMServiceRequest:
            return handleCMServiceRequest(msg, session);
        case ChainPhase::IdentityVerification:
            return handleIdentityVerification(msg, session);
        case ChainPhase::LocationUpdate:
            return handleLocationUpdatePhase(msg, session);
        case ChainPhase::IMSIDetach:
            return handleIMSIDetachPhase(msg, session);
        case ChainPhase::CallRelease:
            return handleCallReleasePhase(msg, session);
        default:
            // Phases with Procedure objects
            if (mCurrentProcedure) {
                ProcedureStepResult result = mCurrentProcedure->feed(msg, session, {});
                if (result.action == ProcedureStepResult::Action::SendResponseWithToken) {
                    mLastToken = result.responseToken;
                }
                // Terminal state is reported via finalResult (see the response/terminal
                // rule in procedure.h): a procedure may finish WITH a pending response
                // token in the same step, so action alone is not a terminal indicator.
                if (result.finalResult.state == procedure::ProcedureState::Completed) {
                    onProcedureCompleted(result);
                } else if (result.finalResult.state == procedure::ProcedureState::Failed ||
                           result.finalResult.state == procedure::ProcedureState::TimedOut) {
                    onProcedureFailed(result);
                }
                return result;
            }
            break;
    }

    return {ProcedureStepResult::Action::Continue};
}

ProcedureStepResult ProcedureOrchestrator::feedExternalTyped(const ExternalData& data,
                                                               ResponseSink sink) {
    if (mCurrentProcedure) {
        // The orchestrator owns the session (mSession); pass it so the active
        // procedure can populate session->response with real parameters.
        ProcedureStepResult result =
            mCurrentProcedure->feedExternalTyped(data, mSession, std::move(sink));
        if (result.action == ProcedureStepResult::Action::SendResponseWithToken) {
            mLastToken = result.responseToken;
        }
        // Terminal state is reported via finalResult (see the response/terminal
        // rule in procedure.h): a procedure may finish WITH a pending response
        // token in the same step, so action alone is not a terminal indicator.
        if (result.finalResult.state == procedure::ProcedureState::Completed) {
            onProcedureCompleted(result);
        } else if (result.finalResult.state == procedure::ProcedureState::Failed ||
                   result.finalResult.state == procedure::ProcedureState::TimedOut) {
            onProcedureFailed(result);
        }
        return result;
    }

    // Inline phase handlers
    switch (mCurrentPhase) {
        case ChainPhase::LocationUpdate:
            return handleExternalDataLocationUpdate(data, std::move(sink));
        case ChainPhase::IMSIDetach:
            return handleExternalDataIMSIDetach(data, std::move(sink));
        case ChainPhase::CallRelease:
            return handleExternalDataCallRelease(data, std::move(sink));
        default:
            break;
    }

    return {ProcedureStepResult::Action::Continue};
}

size_t ProcedureOrchestrator::tickAll(std::chrono::milliseconds delta) {
    if (!mCurrentProcedure) return 0;
    ProcedureStepResult result = mCurrentProcedure->tick(delta);
    if (result.action == ProcedureStepResult::Action::Failed) {
        onProcedureFailed(result);
        return 1;
    }
    return 0;
}

void ProcedureOrchestrator::cancelAll() noexcept {
    if (mCurrentProcedure) {
        mCurrentProcedure->cancel();
        mCurrentProcedure.reset();
    }
    mCurrentPhase = ChainPhase::None;
    // Clear pending response parameters so a later chain never reuses stale values
    // (e.g. an old RAND or channel from the cancelled chain). Must run before
    // mSession is cleared.
    if (mSession) mSession->response.reset();
    mSession = nullptr;
    mLastToken = ResponseToken::None;
    mChainType = procedure::ProcedureType::Unknown;
}

Procedure* ProcedureOrchestrator::activeProcedure() noexcept {
    return mCurrentProcedure.get();
}

const Procedure* ProcedureOrchestrator::activeProcedure() const noexcept {
    return mCurrentProcedure.get();
}

ResponseToken ProcedureOrchestrator::lastResponseToken() const noexcept {
    return mLastToken;
}

int ProcedureOrchestrator::buildPendingResponse(std::span<uint8_t> out,
                                                 const SubscriberSession* session) const {
    if (mLastToken == ResponseToken::None) return -1;
    return ResponseBuilder::buildResponseFromToken(mLastToken, out, session);
}

procedure::ProcedureType ProcedureOrchestrator::chainPhase() const noexcept {
    return mChainType;
}

// ── Inline phase handlers ────────────────────────────────────────────────

ProcedureStepResult ProcedureOrchestrator::handleCMServiceRequest(
    const ParsedMessage& msg, SubscriberSession* session) {
    (void)msg;
    (void)session;

    ProcedureStepResult result;
    result.action = ProcedureStepResult::Action::SendResponseWithToken;
    result.responseToken = ResponseToken::CMServiceAccept;
    mLastToken = ResponseToken::CMServiceAccept;
    return result;
}

ProcedureStepResult ProcedureOrchestrator::handleIdentityVerification(
    const ParsedMessage& msg, SubscriberSession* session) {
    auto pd = messagePD(msg);
    auto mti = messageMTI(msg);

    if (pd == L3PD::MobilityManagement && mti == L3IdentityResponse::MTI) {
        // Identity received; advance to authentication
        if (session) {
            session->mmSM.setState(MMStateMachine::State::IDENTITY_VERIFIED);
        }
        transitionToPhase(ChainPhase::Authentication);
    }

    ProcedureStepResult result;
    result.action = ProcedureStepResult::Action::SendResponseWithToken;
    result.responseToken = ResponseToken::IdentityRequest;
    mLastToken = ResponseToken::IdentityRequest;
    return result;
}

ProcedureStepResult ProcedureOrchestrator::handleLocationUpdatePhase(
    const ParsedMessage& msg, SubscriberSession* session) {
    (void)msg; (void)session;
    // In the orchestrator flow, LocationUpdate phase waits for external VLR decision.
    // The feed() path just returns WaitingExternal.
    ProcedureStepResult result;
    result.action = ProcedureStepResult::Action::WaitingExternal;
    return result;
}

ProcedureStepResult ProcedureOrchestrator::handleIMSIDetachPhase(
    const ParsedMessage& msg, SubscriberSession* session) {
    (void)msg;

    ProcedureStepResult result;
    // Terminal with response: keep action == SendResponseWithToken per the
    // response/terminal rule (procedure.h) so the caller still builds the
    // CMServiceAccept; the terminal state is reported via finalResult only.
    result.action = ProcedureStepResult::Action::SendResponseWithToken;
    result.responseToken = ResponseToken::CMServiceAccept;
    mLastToken = ResponseToken::CMServiceAccept;

    if (session) {
        session->mmSM.setState(MMStateMachine::State::DEREGISTERED);
    }

    result.finalResult = {procedure::ProcedureType::IMSIDetach,
                          procedure::ProcedureState::Completed, "imsi_detach_accept"};
    return result;   // action stays SendResponseWithToken
}

ProcedureStepResult ProcedureOrchestrator::handleCallReleasePhase(
    const ParsedMessage& msg, SubscriberSession* session) {
    (void)msg;

    ProcedureStepResult result;
    // Terminal with response: keep SendResponseWithToken so the caller still
    // builds the Release; the terminal state is reported via finalResult only.
    result.action = ProcedureStepResult::Action::SendResponseWithToken;
    result.responseToken = ResponseToken::Release;
    mLastToken = ResponseToken::Release;

    if (session) {
        session->ccSM.setState(CCStateMachine::State::RELEASE);
    }

    result.finalResult = {procedure::ProcedureType::CallRelease,
                          procedure::ProcedureState::Completed, "release_sent"};
    return result;   // action stays SendResponseWithToken
}

// ── External data handlers for inline phases ─────────────────────────────

ProcedureStepResult ProcedureOrchestrator::handleExternalDataLocationUpdate(
    const ExternalData& data, ResponseSink sink) {
    (void)sink;
    ProcedureStepResult result;

    if (const auto* vlr = std::get_if<VLRDecision>(&data)) {
        // Expose the VLR decision on the session so the builder uses the real
        // new TMSI / reject cause (never fabricated values).
        if (mSession) {
            mSession->response.newTmsi = vlr->newTmsi;
            mSession->response.mmCause = vlr->rejectCause;
        }
        if (vlr->accept) {
            // Terminal with response: keep action == SendResponseWithToken per the
            // response/terminal rule (procedure.h) so the caller still builds the
            // LocationUpdatingAccept; the terminal state is reported via finalResult.
            result.action = ProcedureStepResult::Action::SendResponseWithToken;
            result.responseToken = ResponseToken::LocationUpdatingAccept;
            mLastToken = ResponseToken::LocationUpdatingAccept;

            if (mSession) {
                mSession->mmSM.setState(MMStateMachine::State::REGISTERED);
            }

            result.finalResult = {procedure::ProcedureType::LocationUpdate,
                                  procedure::ProcedureState::Completed, "vlr_accept"};
        } else {
            // Terminal with response (reject): keep SendResponseWithToken; report the
            // terminal Failed state via finalResult only.
            result.action = ProcedureStepResult::Action::SendResponseWithToken;
            result.responseToken = ResponseToken::LocationUpdatingReject;
            mLastToken = ResponseToken::LocationUpdatingReject;

            if (mSession) {
                mSession->mmSM.setState(MMStateMachine::State::DEREGISTERED);
            }

            result.finalResult = {procedure::ProcedureType::LocationUpdate,
                                  procedure::ProcedureState::Failed, "vlr_reject"};
        }
    }

    return result;
}

ProcedureStepResult ProcedureOrchestrator::handleExternalDataIMSIDetach(
    const ExternalData& data, ResponseSink sink) {
    (void)data;
    (void)sink;
    return {ProcedureStepResult::Action::Continue};
}

ProcedureStepResult ProcedureOrchestrator::handleExternalDataCallRelease(
    const ExternalData& data, ResponseSink sink) {
    (void)data;
    (void)sink;
    return {ProcedureStepResult::Action::Continue};
}

} // namespace gsml3parser
