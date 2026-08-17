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

/// Base class for protocol procedures and response callback mechanism.
///
/// Provides the Procedure abstract base class that encapsulates a complete
/// GSM Layer 3 protocol procedure (Location Update, Authentication, Call Setup,
/// etc.) with its own internal state machine, timers, and message sequence logic.
/// BTS applications feed incoming L3 messages via feed(), receive external data
/// via feedExternal(), and manage timeouts via tick().
///
/// ResponseSink callback allows procedures to trigger response message generation
/// without heap allocation on the hot path. The caller provides a lambda that
/// invokes ResponseBuilder and writes into its own pre-allocated buffer (Arena).
///
/// 3GPP specifications: TS 24.008 (MM/CC procedures), TS 04.08 (RR procedures).
/// Thread safety: NOT thread-safe. One Procedure instance per logical procedure.
/// Memory: sizeof(ProcedureStepResult) <= 32 bytes, zero heap allocations.
///
/// Example:
/// @code
///   auto proc = ProcedureFactory::createLocationUpdate();
///   auto result = proc->feed(incomingMsg, session,
///       [](SMAction action, const ParsedMessage& msg, const SubscriberSession* sess) {
///           // Build response via ResponseBuilder into Arena buffer
///       });
///   if (result.action == ProcedureStepResult::Action::Completed) {
///       std::cout << "Procedure finished: " << result.finalResult.reason << "\n";
///   }
/// @endcode
#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <variant>

#include "gsml3parser/stack/procedure_types.h"
#include "gsml3parser/stack/state_machine.h"
#include "gsml3parser/stack/typed_external_data.h"

namespace gsml3parser {

// Forward declaration
class SubscriberSession;

/// Callback for building protocol responses. Invoked by a Procedure when it
/// determines that a response message should be sent to the MS. This design
/// avoids storing response bytes in ProcedureStepResult (zero heap allocation).
///
/// The BTS application provides this callback when calling Procedure::feed().
/// Inside the callback, invoke ResponseBuilder and write bytes into your buffer
/// (Arena, socket buffer, etc.).
///
/// PERFORMANCE NOTE: std::function uses Small Buffer Optimization (SBO) for
/// lambdas with small capture (~20-32 bytes). For zero-allocation on the hot
/// path ensure your lambda closure fits in SBO. If the closure is too large,
/// std::function will heap-allocate. Alternative: use a function pointer
/// (cannot capture context) or pass Arena as a separate parameter.
using ResponseSink = std::function<void(SMAction action, const ParsedMessage& incomingMsg,
                                          const SubscriberSession* session)>;

/// Response token indicates which L3 message type the caller should build.
/// Used together with ResponseBuilder::buildResponseFromToken() to generate
/// response bytes in a pre-allocated Arena buffer (zero heap allocation).
///
/// Each value maps to a specific GSM L3 message type defined in 3GPP TS 04.08.
/// The token is small (uint8_t) so it fits inline in ProcedureStepResult without
/// increasing its size beyond the 32-byte cache-line budget.
enum class ResponseToken : uint8_t {
    None = 0,
    // RR responses:
    ImmediateAssignment,
    AssignmentCommand,
    ChannelRelease,
    CipheringModeCommand,
    PhysicalInformation,
    HandoverCommand,
    PagingRequestType1,
    PagingRequestType2,
    PagingRequestType3,
    // MM responses:
    CMServiceAccept,
    CMServiceReject,
    IdentityRequest,
    AuthenticationRequest,
    LocationUpdatingAccept,
    LocationUpdatingReject,
    TMSIReallocationCommand,
    // CC responses:
    CallProceeding,
    Alerting,
    Connect,
    ConnectAcknowledge,
    Disconnect,
    Release,
    ReleaseComplete,
    Setup,
};

/// Result of processing a single message step within a procedure. Reports
/// whether the procedure should continue, send a response, wait for external
/// data, or has reached a terminal state. Does NOT contain response bytes —
/// responses are generated via the ResponseToken + ResponseBuilder pattern.
struct ProcedureStepResult {
    enum class Action : uint8_t {
        Continue,               ///< Procedure continues; awaiting next message
        SendResponseWithToken,  ///< Build response using responseToken + ResponseBuilder
        WaitingExternal,        ///< Needs external data (RAND from AuC, BSC decision)
        Completed,              ///< Procedure finished successfully
        Failed                  ///< Procedure terminated with an error
    };

    Action action{Action::Continue};
    ResponseToken responseToken{ResponseToken::None};  ///< Which message to build when action == SendResponseWithToken
    procedure::ProcedureResult finalResult{};  ///< Populated when action is Completed or Failed
};

static_assert(sizeof(ProcedureStepResult) <= 32, "ProcedureStepResult must stay small");

/// Abstract base class for a protocol procedure. Each procedure instance
/// encapsulates a known message sequence with its own state machine, timers,
/// and response logic. The BTS application calls feed() for each incoming L3
/// message from the MS.
///
/// 3GPP TS 24.008 - Protocol procedure lifecycle.
///
/// Thread safety: NOT thread-safe. One instance per logical procedure, accessed
/// from a single thread (the event loop or subscriber handler thread).
class Procedure {
public:
    virtual ~Procedure() = default;

    /// Get the procedure type identifier.
    /// @return The ProcedureType enum value for this procedure.
    [[nodiscard]] virtual procedure::ProcedureType type() const = 0;

    /// Get the current lifecycle state of the procedure.
    /// @return The ProcedureState (Initiated, InProgress, WaitingExternal, etc.).
    [[nodiscard]] virtual procedure::ProcedureState state() const = 0;

    /// Feed an incoming L3 message from the MS into the procedure.
    /// @param msg The parsed L3 message to process.
    /// @param session Pointer to the subscriber session (for context access).
    /// @param sink Callback invoked when the procedure needs to send a response.
    /// @return ProcedureStepResult indicating Continue, SendResponse, WaitingExternal,
    ///         Completed, or Failed.
    [[nodiscard]] virtual ProcedureStepResult feed(const ParsedMessage& msg,
                                                      SubscriberSession* session,
                                                      ResponseSink&& sink) = 0;

    /// Feed typed external data into the procedure (e.g., AuthChallenge from AuC, VLRDecision from BSC).
    /// Call this when the procedure has entered WaitingExternal state.
    /// @param data Strongly-typed external data variant (AuthChallenge, VLRDecision, etc.).
    /// @param sink Optional callback for generating responses after external data is received.
    /// @return Continue if the procedure resumed, or Completed/Failed if it reached a terminal state.
    [[nodiscard]] virtual ProcedureStepResult feedExternalTyped(
        const ExternalData& data, ResponseSink&& sink = {});

    /// Tick procedure timers. Call periodically from the event loop.
    /// @param delta Elapsed time in milliseconds since last tick.
    /// @return Usually Continue, but may return Failed if a timer expired.
    [[nodiscard]] virtual ProcedureStepResult tick(std::chrono::milliseconds delta) = 0;

    /// Cancel the procedure explicitly. Use when aborting due to external conditions.
    virtual void cancel() noexcept = 0;
};

} // namespace gsml3parser
