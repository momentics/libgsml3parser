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

/// Exact wire lengths for fixed-body L3 messages (single source of truth
/// for the framers' header-based mode).
#pragma once

#include <cstddef>
#include <cstdint>

namespace gsml3parser::detail {

// Flat triple list: { pd, mti, totalWireLength, ... } terminated by
// {0xFF, 0, 0}. Constant-body messages with exact wire length:
// { pd, internal MTI, total bytes }. Variable-body messages are
// intentionally absent. pd is the 4-bit protocol discriminator (low
// nibble of L3 octet 0); mti is the internal message type in the same
// encoding the framers compute from the header (six low bits of octet
// 1 for MM/CC/NC-SS/GCC/BCC, kRRTifShortBase | code for RR short
// messages with TIF set, raw 8-bit otherwise); totalWireLength =
// 2-byte L3 header + constant body.
// For a pair not listed here fixedFrameLength() returns 0 and the
// header-based framer falls back to its boundary heuristic;
// deterministic framing of variable bodies requires L2-length mode.
// test_frame_lengths.cpp cross-checks every entry against the message
// definitions (2 + T{}.bodyLength()).
inline constexpr size_t kFixedFrameEntries[] = {
    // Radio Resource (pd 0x06, raw 8-bit MTI)
    0x06, 0x12, 3,  // RR Status (1-byte body)
    0x06, 0x13, 2,  // Classmark Enquiry (no value part, TS 44.018 9.1.14)
    0x06, 0x28, 3,  // Handover Failure (1-byte body)
    0x06, 0x29, 3,  // Assignment Complete (1-byte body)
    0x06, 0x2C, 3,  // Handover Complete (1-byte body)
    0x06, 0x2F, 3,  // Assignment Failure (1-byte body)
    0x06, 0x35, 3,  // Ciphering Mode Command (1-byte body, TS 44.018)
    // Mobility Management (pd 0x05, MT in the six low bits of octet 1)
    0x05, 0x21, 2,  // CM Service Accept (no body)
    0x05, 0x22, 3,  // CM Service Reject (1-byte body)
    0x05, 0x23, 2,  // CM Service Abort (no value part, TS 24.008 9.2.7)
    0x05, 0x29, 2,  // MM Abort (no value part, TS 24.008)
    0x05, 0x31, 3,  // MM Status (1-byte body)
    // Call Control (pd 0x03, MT in the six low bits of octet 1)
    0x03, 0x3D, 6,  // CC Status (4-byte body: cause IE identifier 0x11 +
                    // two cause octets + call state, TS 24.078 9.3.19)
    0x03, 0x3E, 3,  // CC Notify (1-byte cause)
    // Broadcast Call Control (pd 0x01, MT in the six low bits of octet 1)
    0x01, 0x04, 2,  // BCC Call Confirmed (no body)
    0x01, 0x09, 2,  // BCC Connect Acknowledge (no body)
    // Group Call Control (pd 0x00, MT in the six low bits of octet 1)
    0x00, 0x03, 2,  // GCC Call Confirmed (no body)
    // Not listed (variable bodies on the air interface, TS 44.018 /
    // 24.008 / 24.078): IMSI Detach Indication (LV classmark + LV mobile
    // identity), Location Service Request/Response (opaque body), Paging
    // Response, Ciphering Mode Complete, all System Information messages,
    // IE-bearing CC messages such as Setup/Connect, GMM, SM and SMS.
    0xFF, 0, 0      // terminator
};

/// Exact wire length for a fixed-body message, or 0 when the (pd, mti)
/// pair is not fixed-length (caller falls back to the variable-length
/// path). O(entries) linear scan: ~14 entries, negligible against the
/// boundary scan; kept constexpr so both framers share one table.
inline constexpr size_t fixedFrameLength(int pd, int mti) noexcept {
    for (size_t i = 0; i + 2 < sizeof(kFixedFrameEntries) / sizeof(kFixedFrameEntries[0]); i += 3) {
        if (kFixedFrameEntries[i] == static_cast<size_t>(pd) &&
            kFixedFrameEntries[i + 1] == static_cast<size_t>(mti)) {
            return kFixedFrameEntries[i + 2];
        }
    }
    return 0;
}

} // namespace gsml3parser::detail
