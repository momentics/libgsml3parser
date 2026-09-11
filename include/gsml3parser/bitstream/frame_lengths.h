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
// {0xFF, 0, 0}. pd is the 4-bit protocol discriminator; mti is the
// message type in the SAME encoding the framers use (raw 8-bit for
// RR/LS, shifted 6-bit for MM/CC/SS/BCC/GCC); totalWireLength = 2-byte
// L3 header + constant body.
//
// Audit P1-1: the previous hand-written tables in framer.cpp and
// inline_framer.h used WRONG MTI values (e.g. 0x0E for Paging Response,
// which is 0x27 per TS 44.018 / GSM_RR_Types.ttcn) and wrong lengths
// (e.g. RR Status 0 instead of 1, Assignment Complete 4 instead of 1),
// so header-based framing corrupted streams containing those messages.
// Only messages with a CONSTANT body length are listed; variable-body
// messages are intentionally absent (header-based mode falls back to the
// boundary heuristic; deterministic framing requires L2-length mode).
// test_frame_lengths.cpp cross-checks every entry against the message
// definitions (2 + T{}.bodyLength()).
inline constexpr size_t kFixedFrameEntries[] = {
    // Radio Resource (pd 0x06, raw 8-bit MTI)
    0x06, 0x12, 3,  // RR Status (1-byte body)
    0x06, 0x13, 3,  // Classmark Enquiry (1-byte body, audit SPEC-1)
    0x06, 0x28, 3,  // Handover Failure (1-byte body)
    0x06, 0x29, 3,  // Assignment Complete (1-byte body)
    0x06, 0x2C, 3,  // Handover Complete (1-byte body)
    0x06, 0x2F, 3,  // Assignment Failure (1-byte body)
    // Mobility Management (pd 0x05, shifted 6-bit MTI)
    0x05, 0x21, 2,  // CM Service Accept (no body)
    0x05, 0x22, 3,  // CM Service Reject (1-byte body)
    0x05, 0x23, 3,  // CM Service Abort (1-byte cause body, TS 24.008 9.2.3 — audit D5)
    0x05, 0x31, 3,  // MM Status (1-byte body)
    // Call Control (pd 0x03, shifted 6-bit MTI)
    0x03, 0x3D, 7,  // CC Status (5-byte body: ti|cause|callState per the
                    // fixed 5-byte implementation, l3ccmessages.h:793)
    // Broadcast Call Control (pd 0x01, shifted 6-bit MTI)
    0x01, 0x04, 2,  // BCC Call Confirmed (no body)
    0x01, 0x09, 2,  // BCC Connect Acknowledge (no body)
    // Group Call Control (pd 0x00, shifted 6-bit MTI)
    0x00, 0x03, 2,  // GCC Call Confirmed (no body)
    // NOT listed (variable bodies): MM 0x01 IMSI Detach
    // Indication (LV classmark + LV mobile identity), LS 0x01 Location
    // Service Request (opaque body), RR 0x27 Paging Response (7–15),
    // RR 0x32 Ciphering Mode Complete (1 or 9 after SPEC-2).
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
