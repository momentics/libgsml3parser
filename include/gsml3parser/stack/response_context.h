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

/// Per-session response parameters (single source of truth for response building).
///
/// Defines ResponseContext, the fixed-size struct that the active procedure
/// populates as it progresses and that ResponseBuilder consumes to build the
/// exact bytes to transmit. See response_context.h struct docs for details.
///
/// 3GPP: TS 24.008 (MM/CC), TS 04.08 (RR).
#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include "gsml3parser/enums.h"
#include "gsml3parser/common/l3common.h"

namespace gsml3parser {

/// Per-session parameters required to construct protocol responses.
///
/// Populated by the active procedure as it progresses (via feed()/feedExternalTyped());
/// consumed by ResponseBuilder::buildResponseFromToken() to build the exact bytes to
/// transmit. This makes the session the single source of truth for response parameters,
/// eliminating fabricated/hardcoded values on the response path.
///
/// 3GPP: TS 24.008 (MM/CC), TS 04.08 (RR).
/// Thread safety: NOT thread-safe. One instance per SubscriberSession, single-threaded.
/// Memory: sizeof(ResponseContext) <= 160 bytes, zero heap allocation (fixed arrays).
struct ResponseContext {
    // ── Authentication (TS 24.008 10.5.1.21) ─────────────────────────
    std::array<uint8_t, 16> rand{};   ///< 128-bit RAND in wire order (octet 0 first).
    bool hasRand{false};              ///< True once an AuthChallenge has been fed.

    // ── Call Control (TS 24.008 9.3) ─────────────────────────────────
    uint8_t ti{0};                    ///< Transaction Identifier (0-7) for CC/SS.
    CCCause ccCause{CCCause::Normal_Call_Clearing}; ///< Default cause for Disconnect/Release.
    std::array<char, 20> calledNumber{};           ///< BCD digits for Setup (max 16 digits).
    uint8_t calledNumberLen{0};      ///< Number of valid BCD digits in calledNumber.
    bool hasCalledNumber{false};

    // ── Mobility Management (TS 24.008 9.2) ──────────────────────────
    MMRejectCause mmCause{MMRejectCause::Zero};    ///< Cause for LocationUpdatingReject.
    std::optional<uint32_t> newTmsi;               ///< New TMSI for LocationUpdatingAccept.

    // ── Radio Resource (TS 04.08 9.1) ────────────────────────────────
    L3ChannelDescription channel{};   ///< Target channel for Immediate/Assignment.
    bool hasChannel{false};
    uint8_t cipherAlgo{0};            ///< Ciphering algorithm selector (0=A5/0, 1=A5/1).
    bool hasCipherAlgo{false};

    // ── Paging (TS 04.08 9.1.25) ─────────────────────────────────────
    L3MobileIdentity identity{};      ///< Identity to page (TMSI or IMSI).
    bool hasIdentity{false};

    // ── Handover (TS 04.08 9.1.40) ───────────────────────────────────
    L3ChannelDescription hoChannel{}; ///< Target channel for HandoverCommand.
    bool hasHoChannel{false};

    /// Reset all pending parameters (call on procedure cancel/complete).
    void reset() noexcept;
};

/// Reset all pending parameters (call on procedure cancel/complete).
/// Restores the default-constructed state: all flags cleared, counters zeroed,
/// causes back to their defaults. Zero heap allocation.
inline void ResponseContext::reset() noexcept {
    *this = ResponseContext{};
}

static_assert(sizeof(ResponseContext) <= 160, "ResponseContext must fit in 160 bytes");

} // namespace gsml3parser
