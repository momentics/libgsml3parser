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

/// LAPDm (Link Access Protocol for DM channel) framing utilities.
/// Provides functions to wrap L3 messages in LAPDm headers for transmission
/// over the Um interface, and to unwrap incoming LAPDm frames.
/// Reference: GSM 04.06 / 3GPP TS 24.022
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "gsml3parser/expected.h"
#include "gsml3parser/types.h"

namespace gsml3parser::lapdm {

/// LAPDm address field encoding.
/// Encodes SAPI (Service Access Point Indicator), C/R (Command/Response),
/// and EA (Extended Address) bits per GSM 04.06 4.2.1.
/// Bit layout: [SAPI(7:4)][C/R(3)][reserved(2:1)][EA(0)]
/// @param sapi Service Access Point (SAPI0=signaling, SAPI3=data)
/// @param cr Command (0) or Response (1) bit
/// @param ea Extended Address bit (normally 1 for L3 messages)
/// @return Single byte address field
[[nodiscard]] uint8_t makeAddress(SAPI sapi, bool cr, bool ea = true);

/// LAPDm control field values.
enum class ControlField : uint8_t {
    UI = 0x03,       /// Unnumbered Information (most L3 messages)
    SABME = 0x2F,    /// Set Asynchronous Balanced Mode Extended
    UA = 0x63,       /// Unnumbered Acknowledgement
    DM = 0x0F,       /// Disconnected Mode
};
// NOTE: DISC shares the same bit pattern as UI (0x03). They are distinguished
// by protocol context, not by the control field value alone.

/// Wrap an L3 message body in a LAPDm UI frame header.
/// The output is: [address_field][control_field][l3_body...].
/// @param l3Body Raw L3 message bytes (from writeL3Bytes).
/// @param sapi Service Access Point Indicator.
/// @param cr Command/Response bit.
/// @return Complete LAPDm frame ready for transmission to PHY layer.
[[nodiscard]] std::vector<uint8_t> wrapL3(std::span<const uint8_t> l3Body, SAPI sapi = SAPI::SAPI0, bool cr = false);

/// Unwrap a LAPDm frame and extract the L3 payload.
/// Validates that the frame is at least 2 bytes (address + control).
/// @param lapdmFrame Complete LAPDm frame bytes.
/// @return L3 payload bytes (without LAPDm headers).
[[nodiscard]] Expected<std::vector<uint8_t>> unwrapL3(std::span<const uint8_t> lapdmFrame);

/// Extract SAPI from a LAPDm address field byte.
[[nodiscard]] SAPI extractSAPI(uint8_t addrByte);

/// Extract C/R bit from a LAPDm address field byte.
[[nodiscard]] bool extractCR(uint8_t addrByte);

/// Check if a LAPDm frame has a UI control field.
[[nodiscard]] bool isUIFrame(std::span<const uint8_t> lapdmFrame);

} // namespace gsml3parser::lapdm
