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

#include "gsml3parser/stack/state_machine.h"

namespace gsml3parser {

// ── ProtocolStateMachine base implementation ─────────────────────────────

void ProtocolStateMachine::setState(int state) noexcept {
    mCurrentState = state;
}

int ProtocolStateMachine::state() const noexcept {
    return mCurrentState;
}

SMResult ProtocolStateMachine::processMessage(const ParsedMessage& msg) {
    SMResult result = handle_message_impl(mCurrentState, msg);
    if (result.causesTransition() && result.nextState.has_value()) {
        mCurrentState = result.nextState.value();
    }
    return result;
}

SMResult ProtocolStateMachine::processTimer(L3TimerId timerId) {
    SMResult result = handle_timer_impl(mCurrentState, timerId);
    if (result.causesTransition() && result.nextState.has_value()) {
        mCurrentState = result.nextState.value();
    }
    return result;
}

// ── RR State Machine ─────────────────────────────────────────────────────

SMResult RRStateMachine::handle_message_impl(int state, const ParsedMessage& msg) {
    L3PD pd = messagePD(msg);
    int mti = messageMTI(msg);

    if (pd != L3PD::RadioResource) {
        switch (state) {
            case State::LINK_ESTABLISHED:
                if (pd == L3PD::MobilityManagement) {
                    return {SMAction::Transition, static_cast<int>(State::WAITING_MM)};
                }
                break;
            case State::WAITING_MM:
                if (pd == L3PD::MobilityManagement && mti == L3CMServiceAccept::MTI) {
                    return {SMAction::Transition, static_cast<int>(State::ACTIVE)};
                }
                if (pd == L3PD::MobilityManagement && mti == L3CMServiceRequest::MTI) {
                    return {SMAction::SendResponse, static_cast<int>(State::ACTIVE)};
                }
                break;
            case State::ACTIVE:
                if (pd == L3PD::CallControl && mti == L3Setup::MTI) {
                    return {SMAction::SendResponse, State::ACTIVE};
                }
                break;
            default:
                break;
        }
        return {SMAction::None, std::nullopt};
    }

    switch (state) {
        case State::IDLE:
            if (mti == L3ChannelRequest::MTI) {
                return {SMAction::SendResponse, static_cast<int>(State::CHANNEL_REQUESTED)};
            }
            break;

        case State::CHANNEL_ASSIGNED:
            if (mti == L3PagingResponse::MTI) {
                return {SMAction::Transition, static_cast<int>(State::WAITING_MM)};
            }
            break;

        case State::LINK_ESTABLISHED:
            if (pd == L3PD::MobilityManagement) {
                return {SMAction::Transition, static_cast<int>(State::WAITING_MM)};
            }
            break;

        case State::WAITING_MM:
            if (pd == L3PD::MobilityManagement && mti == L3CMServiceAccept::MTI) {
                return {SMAction::Transition, static_cast<int>(State::ACTIVE)};
            }
            if (pd == L3PD::MobilityManagement && mti == L3CMServiceRequest::MTI) {
                return {SMAction::SendResponse, static_cast<int>(State::ACTIVE)};
            }
            break;

        case State::ACTIVE:
            switch (mti) {
                case L3CipheringModeComplete::MTI:
                    return {SMAction::None, std::nullopt};
                case L3ChannelRelease::MTI:
                    return {SMAction::SendResponse, static_cast<int>(State::CHANNEL_RELEASE)};
                case L3MeasurementReport::MTI:
                    return {SMAction::None, std::nullopt};
                case L3HandoverCommand::MTI:
                    return {SMAction::Transition, static_cast<int>(State::HANDOVER)};
                default:
                    break;
            }
            break;

        case State::CIPHER_MODE:
            if (mti == L3CipheringModeComplete::MTI) {
                return {SMAction::Transition, static_cast<int>(State::ACTIVE)};
            }
            break;

        case State::HANDOVER:
            break;

        case State::CHANNEL_RELEASE:
            return {SMAction::ReleaseChannel, std::nullopt};
    }

    return {SMAction::None, std::nullopt};
}

SMResult RRStateMachine::handle_timer_impl(int state, L3TimerId timerId) {
    if (timerId == L3TimerId::T3109) {
        switch (state) {
            case State::CHANNEL_ASSIGNED:
            case State::WAITING_MM:
                return {SMAction::Transition, static_cast<int>(State::CHANNEL_RELEASE)};
            default:
                break;
        }
    }

    return {SMAction::None, std::nullopt};
}

// ── MM State Machine ─────────────────────────────────────────────────────

SMResult MMStateMachine::handle_message_impl(int state, const ParsedMessage& msg) {
    L3PD pd = messagePD(msg);
    int mti = messageMTI(msg);

    if (pd != L3PD::MobilityManagement) {
        return {SMAction::None, std::nullopt};
    }

    switch (state) {
        case State::DEREGISTERED:
            if (mti == L3CMServiceRequest::MTI) {
                return {SMAction::SendResponse, static_cast<int>(State::WAITING_IDENTITY)};
            }
            break;

        case State::SERVICE_REQUEST:
            if (mti == L3IdentityResponse::MTI) {
                return {SMAction::SendResponse, static_cast<int>(State::IDENTITY_VERIFIED)};
            }
            break;

        case State::WAITING_IDENTITY:
            if (mti == L3IdentityResponse::MTI) {
                return {SMAction::SendResponse, static_cast<int>(State::IDENTITY_VERIFIED)};
            }
            break;

        case State::IDENTITY_VERIFIED:
            if (mti == L3AuthenticationResponse::MTI) {
                return {SMAction::SendResponse, static_cast<int>(State::AUTHENTICATED)};
            }
            break;

        case State::AUTHENTICATION:
            if (mti == L3AuthenticationResponse::MTI) {
                return {SMAction::SendResponse, static_cast<int>(State::AUTHENTICATED)};
            }
            break;

        case State::AUTHENTICATED:
            if (mti == L3LocationUpdatingRequest::MTI) {
                return {SMAction::Transition, static_cast<int>(State::LOCATION_UPDATE)};
            }
            break;

        case State::LOCATION_UPDATE:
            if (mti == L3CMServiceAccept::MTI) {
                return {SMAction::SendResponse, static_cast<int>(State::REGISTERED)};
            }
            break;

        case State::REGISTERED:
            break;
    }

    return {SMAction::None, std::nullopt};
}

SMResult MMStateMachine::handle_timer_impl(int state, L3TimerId timerId) {
    if (timerId == L3TimerId::T3101 || timerId == L3TimerId::T3102) {
        switch (state) {
            case State::SERVICE_REQUEST:
            case State::WAITING_IDENTITY:
                return {SMAction::Transition, static_cast<int>(State::DEREGISTERED)};
            default:
                break;
        }
    }

    if (timerId == L3TimerId::T3106) {
        switch (state) {
            case State::AUTHENTICATION:
                return {SMAction::Transition, static_cast<int>(State::DEREGISTERED)};
            default:
                break;
        }
    }

    return {SMAction::None, std::nullopt};
}

// ── CC State Machine ─────────────────────────────────────────────────────

SMResult CCStateMachine::handle_message_impl(int state, const ParsedMessage& msg) {
    L3PD pd = messagePD(msg);
    int mti = messageMTI(msg);

    if (pd != L3PD::CallControl) {
        return {SMAction::None, std::nullopt};
    }

    switch (state) {
        case State::IDLE:
            if (mti == L3Setup::MTI) {
                return {SMAction::Transition, static_cast<int>(State::SETUP_RECEIVED)};
            }
            break;

        case State::SETUP_RECEIVED:
            return {SMAction::SendResponse, static_cast<int>(State::PROCEEDING)};

        case State::PROCEEDING:
            if (mti == L3Alerting::MTI) {
                return {SMAction::SendResponse, static_cast<int>(State::ALERTING)};
            }
            break;

        case State::ALERTING:
            if (mti == L3Connect::MTI) {
                return {SMAction::SendResponse, static_cast<int>(State::CONNECT)};
            }
            break;

        case State::CONNECT:
            if (mti == L3CallConfirmed::MTI) {
                return {SMAction::Transition, static_cast<int>(State::ACTIVE)};
            }
            break;

        case State::ACTIVE:
            if (mti == L3Disconnect::MTI) {
                return {SMAction::SendResponse, static_cast<int>(State::DISCONNECT_RECEIVED)};
            }
            if (mti == L3ConnectAcknowledge::MTI) {
                return {SMAction::Transition, static_cast<int>(State::ACTIVE)};
            }
            break;

        case State::DISCONNECT_RECEIVED:
            return {SMAction::SendResponse, static_cast<int>(State::RELEASE)};

        case State::RELEASE:
            if (mti == L3Release::MTI) {
                return {SMAction::SendResponse, static_cast<int>(State::IDLE)};
            }
            break;
    }

    return {SMAction::None, std::nullopt};
}

SMResult CCStateMachine::handle_timer_impl(int state, L3TimerId timerId) {
    if (timerId == L3TimerId::T3101) {
        switch (state) {
            case State::SETUP_RECEIVED:
            case State::PROCEEDING:
            case State::ALERTING:
                return {SMAction::Transition, static_cast<int>(State::IDLE)};
            default:
                break;
        }
    }

    return {SMAction::None, std::nullopt};
}

} // namespace gsml3parser
