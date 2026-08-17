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

/// Procedure type definitions for the GSM L3 Protocol Procedure Framework.
///
/// Provides enumerations and structures that describe protocol-level procedures
/// (Location Update, Authentication, Call Setup, etc.) and their lifecycle states.
/// These types form the foundation for the ProcedureRunner API, allowing a BTS
/// application to track which procedure is active, its current state, and the
/// outcome when it completes.
///
/// 3GPP specifications: TS 24.008 (MM/CC procedures), TS 04.08 (RR procedures).
/// Thread safety: all types are trivially copyable and thread-safe for concurrent read.
/// Memory: sizeof(ProcedureResult) <= 64 bytes (string_view = 2 pointers on 64-bit).
///
/// Example:
/// @code
///   ProcedureResult res{ProcedureType::LocationUpdate, ProcedureState::Completed, "ok"};
///   std::cout << procedureTypeName(res.type) << " -> "
///             << procedureStateName(res.state) << "\n";
/// @endcode
#pragma once

#include <cstdint>
#include <string_view>

namespace gsml3parser::procedure {

/// Protocol procedure types mapped to 3GPP TS 24.008 / TS 04.08 chapters.
/// Each value corresponds to a distinct, well-defined message exchange sequence
/// between MS and network that the ProcedureRunner can manage automatically.
enum class ProcedureType : uint8_t {
    LocationUpdate = 0x01,    ///< TS 24.008 4.4.1 - Normal/IMSI-attached location updating
    Authentication = 0x02,    ///< TS 24.008 4.4.2 - Authentication and ciphering key setup
    CipheringMode = 0x03,     ///< TS 24.008 4.4.3 / TS 04.08 9.1.37 - Ciphering activation
    CallSetup_MO = 0x04,      ///< TS 24.008 6.1 - Mobile Originated Call setup
    CallSetup_MT = 0x05,      ///< TS 24.008 6.1 - Mobile Terminated Call setup
    ChannelAssignment = 0x06, ///< TS 04.08 9.1.2 / 9.1.35 - Channel assignment procedure
    Handover = 0x07,          ///< TS 04.08 9.1.40 - Handover command and completion
    Paging = 0x08,            ///< TS 04.08 9.1.25 - Network-initiated paging of MS
    CMServiceRequest = 0x09,  ///< TS 24.008 4.7 - CM service request procedure
    IMSIDetach = 0x0A,        ///< TS 24.008 4.4.6 - IMSI detach procedure
    CallRelease = 0x0B,       ///< TS 24.008 6.1 - Call release (disconnect -> release complete)
    PeriodicLocationUpdate = 0x0C, ///< TS 24.008 4.4.1 - Periodic location updating
    Unknown = 0xFF            ///< Unrecognized or unsupported procedure type
};

/// Lifecycle state of a managed procedure instance.
/// Transitions: Initiated -> InProgress -> (WaitingExternal | Completed | Failed | TimedOut).
enum class ProcedureState : uint8_t {
    Initiated,       ///< Procedure started; awaiting first protocol message
    InProgress,      ///< Active message exchange in progress
    WaitingExternal, ///< Blocked on external data (RAND from AuC, BSC decision, etc.)
    Completed,       ///< Procedure finished successfully
    Failed,          ///< Procedure terminated with an error condition
    TimedOut         ///< Procedure exceeded its allowed duration without completion
};

/// Result structure returned when a procedure reaches a terminal state
/// (Completed, Failed, or TimedOut). Carries the procedure type, final state,
/// and a human-readable reason string for logging and diagnostics.
struct ProcedureResult {
    ProcedureType type{ProcedureType::Unknown};
    ProcedureState state{ProcedureState::Initiated};
    std::string_view reason{};  ///< Completion reason: "ok", "timeout", "reject", etc.
};

static_assert(sizeof(ProcedureResult) <= 64,
    "ProcedureResult must remain cache-friendly (<= 64 bytes)");

/// Return a human-readable name for the given procedure type.
/// @param type The procedure type enum value.
/// @return Non-empty string identifier suitable for logging. Returns "?" for Unknown.
[[nodiscard]] std::string_view procedureTypeName(ProcedureType type);

/// Return a human-readable name for the given procedure state.
/// @param state The procedure state enum value.
/// @return Non-empty string identifier suitable for logging.
[[nodiscard]] std::string_view procedureStateName(ProcedureState state);

} // namespace gsml3parser::procedure
