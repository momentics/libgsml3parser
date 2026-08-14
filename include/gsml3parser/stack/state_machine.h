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

/// Protocol state machine framework for RR, MM, and CC sublayers.
///
/// Provides a base class for protocol state machines with message and timer
/// event processing, plus concrete skeleton implementations for the three
/// main GSM Layer 3 sublayers: Radio Resource (RR), Mobility Management (MM),
/// and Call Control (CC).
///
/// The FSM returns SMResult (action + optional next state) without storing
/// messages. The caller builds response messages externally via Builder API.
/// This keeps SMResult small (~16 bytes) and copyable.
///
/// 3GPP TS 24.008 — Layer 3 specification.
/// 3GPP TS 44.018 — Mobile radio interface layer 3 specification.
///
/// Thread safety: NOT thread-safe. One instance per MS, accessed from a single thread.
/// Performance: handle_message_impl() uses switch(PD) + switch(MTI) dispatch —
/// compile-time resolved, no vtable lookup on the critical path.
/// Memory: SMResult contains no heap-allocated data (~16 bytes).
///
/// Example:
/// @code
///   class MyRRSM : public RRStateMachine {
///   protected:
///       SMResult handle_message_impl(int state, const ParsedMessage& msg) override {
///           // Override specific transitions; fall back to base for defaults.
///           if (state == State::ACTIVE && messagePD(msg) == L3PD::MobilityManagement) {
///               return {SMAction::Transition, static_cast<int>(State::CIPHER_MODE)};
///           }
///           return RRStateMachine::handle_message_impl(state, msg);
///       }
///   };
/// @endcode
#pragma once

#include <optional>
#include <string_view>

#include "gsml3parser/message_types.h"
#include "gsml3parser/visitor.h"
#include "gsml3parser/stack/l3_timer.h"

namespace gsml3parser {

/// Result of processing a message or timer event in a state machine.
/// Each action determines whether the FSM should transition, send a response,
/// or perform other side effects.
enum class SMAction : uint8_t {
    None,                /// Stay in current state, no side effect
    Transition,          /// Move to the next state specified by nextState
    SendResponse,        /// Transition and signal that a response message should be built externally
    Reject,              /// Signal that a reject/status message should be sent; stay in current state
    ReleaseChannel,      /// Signal that the logical channel should be released
    PushSubstate,        /// Push a sub-state machine onto the stack (for nested procedures)
    PopSubstate          /// Pop back to the parent state machine
};

/// Result returned from handle_message / handle_timer callbacks.
///
/// Contains only the action and optional target state index. Does NOT contain
/// ParsedMessage (~8 KB variant). The FSM returns action + nextState; the caller
/// is responsible for building and sending response messages based on the action.
/// This keeps SMResult small and copyable, suitable for stack allocation.
///
/// Memory: sizeof(SMResult) <= 16 bytes, zero heap allocations.
struct SMResult {
    SMAction action{SMAction::None};
    std::optional<int> nextState;

    /// Convenience: check if action causes a state transition.
    /// @return True if the action is Transition or SendResponse.
    [[nodiscard]] bool causesTransition() const noexcept {
        return action == SMAction::Transition ||
               action == SMAction::SendResponse;
    }
};

static_assert(sizeof(SMResult) <= 16, "SMResult must remain small for stack efficiency");

/// Base class for protocol state machines.
///
/// Subclasses define states as an enum and implement handle_message_impl() and
/// handle_timer_impl() to define transitions. The base class provides a generic
/// processMessage/processTimer interface that delegates to the derived impl.
///
/// 3GPP TS 24.008 — State machine behavior for RR, MM, CC procedures.
///
/// Thread safety: NOT thread-safe. One instance per MS, accessed from a single thread.
/// Performance: processMessage() dispatches via virtual call to derived impl, but the
/// derived impl should use switch(PD) + switch(MTI) for O(1) compile-time resolved routing.
class ProtocolStateMachine {
public:
    virtual ~ProtocolStateMachine() = default;

    /// Set the current state. Call before entering the processing loop.
    /// @param state The state index to set.
    void setState(int state) noexcept;

    /// Get the current state.
    /// @return The current state index.
    [[nodiscard]] int state() const noexcept;

    /// Process an incoming L3 message in the current state.
    /// Delegates to the derived class's handle_message_impl().
    /// @param msg The parsed L3 message to process.
    /// @return SMResult describing the action and optional next state.
    [[nodiscard]] SMResult processMessage(const ParsedMessage& msg);

    /// Process a timer expiry event in the current state.
    /// Delegates to the derived class's handle_timer_impl().
    /// @param timerId The timer that has expired.
    /// @return SMResult describing the action and optional next state.
    [[nodiscard]] SMResult processTimer(L3TimerId timerId);

    /// Get the debug name of this state machine.
    /// @return Human-readable name (e.g. "RRStateMachine").
    [[nodiscard]] virtual std::string_view debugName() const = 0;

protected:
    /// Override this to define message handling per state.
    /// The implementation should dispatch on PD + MTI using switch statements
    /// for O(1) compile-time resolved routing (no virtual calls, no hash map).
    /// @param state Current state index.
    /// @param msg The parsed L3 message to handle.
    /// @return SMResult with the action and optional target state.
    [[nodiscard]] virtual SMResult handle_message_impl(int state, const ParsedMessage& msg) = 0;

    /// Override this to define timer handling per state.
    /// @param state Current state index.
    /// @param timerId The expired timer identifier.
    /// @return SMResult with the action and optional target state.
    [[nodiscard]] virtual SMResult handle_timer_impl(int state, L3TimerId timerId) = 0;

    int mCurrentState{};
};

/// RR (Radio Resource) state machine skeleton.
///
/// Defines standard RR states per 3GPP TS 24.008 Chapter 4 and provides default
/// transition logic. BTS developers inherit from this class and override
/// handle_message_impl() to customize behavior for specific scenarios.
///
/// Default transitions implemented:
///   IDLE + ChannelRequest    -> CHANNEL_REQUESTED
///   CHANNEL_ASSIGNED + PagingResponse -> WAITING_MM
///   LINK_ESTABLISHED + (any MM msg)   -> WAITING_MM
///   ACTIVE + CipheringModeComplete    -> ACTIVE (ciphering done, no transition)
///   ACTIVE + ChannelRelease(incoming) -> CHANNEL_RELEASE
///   ACTIVE + MeasurementReport        -> ACTIVE (no transition, for developer handling)
///   Any state + unexpected message    -> None (stay in current state)
///
/// 3GPP TS 24.008 4.1 — Radio Resource Management procedures.
///
/// Thread safety: NOT thread-safe. One instance per MS, accessed from a single thread.
class RRStateMachine : public ProtocolStateMachine {
public:
    /// RR protocol states.
    enum State {
        IDLE,                   /// No dedicated channel assigned
        CHANNEL_REQUESTED,      /// RACH received, waiting for AGCH / Immediate Assignment
        CHANNEL_ASSIGNED,       /// Immediate Assignment sent, waiting for SABM / Paging Response
        LINK_ESTABLISHED,       /// LAPDm link established (UA received)
        WAITING_MM,             /// Waiting for MM procedure (CM Service Request)
        ACTIVE,                 /// Full communication on DCCH
        CIPHER_MODE,            /// Ciphering mode procedure in progress
        HANDOVER,               /// Handover procedure in progress
        CHANNEL_RELEASE         /// Channel release in progress
    };

    RRStateMachine() = default;

    /// Returns "RRStateMachine" for diagnostics.
    [[nodiscard]] std::string_view debugName() const override { return "RRStateMachine"; }

protected:
    /// Default RR message handler with switch(PD) + switch(MTI) dispatch.
    /// Override to add custom transitions or responses.
    [[nodiscard]] SMResult handle_message_impl(int state, const ParsedMessage& msg) override;

    /// Default RR timer handler. Timer expiry typically causes reject or release.
    [[nodiscard]] SMResult handle_timer_impl(int state, L3TimerId timerId) override;
};

/// MM (Mobility Management) state machine skeleton.
///
/// Defines standard MM states per 3GPP TS 24.008 Chapter 4 and provides default
/// transition logic for registration, authentication, and identity procedures.
///
/// Default transitions implemented:
///   DEREGISTERED + CMServiceRequest     -> SERVICE_REQUEST
///   SERVICE_REQUEST + IdentityResponse  -> IDENTITY_VERIFIED
///   IDENTITY_VERIFIED + (auth trigger)  -> AUTHENTICATION
///   AUTHENTICATION + AuthenticationResp -> AUTHENTICATED
///   AUTHENTICATED + LocationUpdReq      -> LOCATION_UPDATE
///   LOCATION_UPDATE + (accept trigger)  -> REGISTERED
///   Any state + unexpected message      -> None (stay in current state)
///
/// 3GPP TS 24.008 4.2 — Mobility Management procedures.
///
/// Thread safety: NOT thread-safe. One instance per MS, accessed from a single thread.
class MMStateMachine : public ProtocolStateMachine {
public:
    /// MM protocol states.
    enum State {
        DEREGISTERED,           /// MS not registered in VLR
        SERVICE_REQUEST,        /// CM Service Request received
        WAITING_IDENTITY,       /// Identity Request sent, awaiting response
        IDENTITY_VERIFIED,      /// IMSI/TMSI known and verified
        AUTHENTICATION,         /// Authentication Request sent, awaiting response
        AUTHENTICATED,          /// Authentication complete
        LOCATION_UPDATE,        /// Location Updating in progress
        REGISTERED              /// Fully registered, ready for calls
    };

    MMStateMachine() = default;

    /// Returns "MMStateMachine" for diagnostics.
    [[nodiscard]] std::string_view debugName() const override { return "MMStateMachine"; }

protected:
    /// Default MM message handler with switch(PD) + switch(MTI) dispatch.
    /// Override to add custom transitions or responses.
    [[nodiscard]] SMResult handle_message_impl(int state, const ParsedMessage& msg) override;

    /// Default MM timer handler. Timer expiry may cause deregistration or retry.
    [[nodiscard]] SMResult handle_timer_impl(int state, L3TimerId timerId) override;
};

/// CC (Call Control) state machine skeleton.
///
/// Defines standard CC states per 3GPP TS 24.008 Chapter 6 and provides default
/// transition logic for call setup, maintenance, and teardown.
///
/// Default transitions implemented:
///   IDLE + Setup              -> SETUP_RECEIVED
///   SETUP_RECEIVED            -> PROCEEDING (developer sends Proceeding)
///   PROCEEDING + Alerting     -> ALERTING
///   ALERTING + Connect        -> CONNECT
///   CONNECT + CallConfirmed   -> ACTIVE
///   ACTIVE + Disconnect       -> DISCONNECT_RECEIVED
///   DISCONNECT_RECEIVED       -> RELEASE (developer sends Release)
///   Any state + unexpected    -> None (stay in current state)
///
/// 3GPP TS 24.008 6.1 — Call Control procedures.
///
/// Thread safety: NOT thread-safe. One instance per MS, accessed from a single thread.
class CCStateMachine : public ProtocolStateMachine {
public:
    /// CC protocol states.
    enum State {
        IDLE,                   /// No call in progress
        SETUP_RECEIVED,         /// Setup message received
        PROCEEDING,             /// Call Proceeding sent
        ALERTING,               /// Alerting sent
        CONNECT,                /// Connect received
        ACTIVE,                 /// Bidirectional speech path established
        DISCONNECT_RECEIVED,    /// Disconnect received
        RELEASE                 /// Release in progress
    };

    CCStateMachine() = default;

    /// Returns "CCStateMachine" for diagnostics.
    [[nodiscard]] std::string_view debugName() const override { return "CCStateMachine"; }

protected:
    /// Default CC message handler with switch(PD) + switch(MTI) dispatch.
    /// Override to add custom transitions or responses.
    [[nodiscard]] SMResult handle_message_impl(int state, const ParsedMessage& msg) override;

    /// Default CC timer handler. Timer expiry may cause call release.
    [[nodiscard]] SMResult handle_timer_impl(int state, L3TimerId timerId) override;
};

} // namespace gsml3parser
