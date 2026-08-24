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

/// Strongly-typed external data structures for protocol procedures.
///
/// Replaces the opaque `std::span<const uint8_t>` parameter previously passed to
/// feedExternal with named, self-documenting structures. Each structure corresponds
/// to a specific type of external data that a BTS application feeds into a procedure
/// via feedExternalTyped().
/// All structures are small (<= 64 bytes) and passed by const reference to avoid copies.
///
/// 3GPP specifications: TS 24.008 (authentication, location update), TS 04.08 (paging, ciphering).
/// Thread safety: all types are trivially copyable for their underlying members.
/// Memory: sizeof(ExternalData) ~64 bytes (dominated by largest alternative PagingTrigger).
///
/// Example:
/// @code
///   // Feed authentication challenge from AuC:
///   AuthChallenge chal{};
///   std::memcpy(chal.rand.data(), aucRand, 16);
///   std::memcpy(chal.expectedSres.data(), aucSres, 4);
///   auto result = proc->feedExternalTyped(chal, sink);
///
///   // Feed VLR accept decision:
///   VLRDecision vlr{true, 0x12345678u, MMRejectCause::Zero};
///   auto result2 = proc->feedExternalTyped(vlr, sink);
/// @endcode
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>

#include "gsml3parser/common/l3common.h"
#include "gsml3parser/enums.h"

namespace gsml3parser {

/// Authentication challenge data from AuC.
/// Contains RAND (128-bit per TS 24.008) and expected SRES for verification.
///
/// Byte order (CRITICAL for integration):
///   - rand: 16 octets in wire order (octet 0 is the first octet of the
///     RAND IE, TS 24.008 10.5.1.21).
///   - expectedSres: 4 octets, BIG-ENDIAN — octet 0 is the most significant
///     byte, matching the 32-bit SRES IE encoding of TS 24.008 10.5.1.22
///     (the same order the MS sends in Authentication Response).
///     Example: SRES 0xABCD1234 -> expectedSres = {0xAB, 0xCD, 0x12, 0x34}.
struct AuthChallenge {
    std::array<uint8_t, 16> rand{};       // 128-bit RAND per GSM spec (wire order)
    std::array<uint8_t, 4> expectedSres{}; // Expected SRES, big-endian (octet 0 = MSB)
};
static_assert(sizeof(AuthChallenge) <= 32);

/// VLR decision for location update accept/reject.
/// When accept is true, optional newTmsi may be assigned to the MS.
/// When accept is false, rejectCause indicates the reason.
struct VLRDecision {
    bool accept{false};
    std::optional<uint32_t> newTmsi;
    MMRejectCause rejectCause{MMRejectCause::Zero};
};

/// Trigger data for network-initiated paging.
/// Contains the mobile identity to page and the target channel type.
struct PagingTrigger {
    L3MobileIdentity identity;
    ChannelType targetChannel{ChannelType::SDCCHType};
};

/// Ciphering activation parameters.
/// algorithmSelector determines the ciphering algorithm (0=A5/0, 1=A5/1, etc.).
struct CipheringParameters {
    uint8_t algorithmSelector{0};
    bool enableCiphering{true};
};
static_assert(sizeof(CipheringParameters) <= 16);

/// Handover target cell and channel description.
/// Used by the network to instruct MS to switch to a new channel.
struct HandoverTarget {
    L3ChannelDescription channel;
    L3CellDescription cell;
};

/// Unified variant for all external data types.
/// IMPORTANT: Always pass by const& to avoid copying the variant (~64 bytes).
using ExternalData = std::variant<
    AuthChallenge,
    VLRDecision,
    PagingTrigger,
    CipheringParameters,
    HandoverTarget
>;

} // namespace gsml3parser
