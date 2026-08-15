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

/// LAPDm frame types and encoding/decoding (GSM 04.06).
/// Provides zero-copy frame parsing via non-owning std::span views, constexpr
/// field encode/decode for compile-time constant evaluation, and factory
/// functions for constructing all LAPDm frame types (I, S, U).
///
/// Reference: GSM 04.06 / 3GPP TS 45.006
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "gsml3parser/expected.h"
#include "gsml3parser/types.h"

namespace gsml3parser::lapdm {

/// LAPDm control field format discriminator (GSM 04.06 4.4).
/// Bit 3 of the control octet determines the format:
///   0 = I-format (Information frame)
///   1 = S-format (Supervisory frame) or U-format (Unnumbered frame)
enum class LAPDmControlFormat : uint8_t {
    I_Format, ///< Information frame (carries user data)
    S_Format, ///< Supervisory frame (RR, REJ — flow control)
    U_Format  ///< Unnumbered frame (UI, SABME, UA, DM, DISC)
};

/// Unnumbered frame types (GSM 04.06 4.4.2.2).
/// Values are the canonical control byte with F/PF=1. The encode() function
/// adjusts bit 2 (F) based on the pf field, so both PF=0 and PF=1 variants
/// map to the same type when decoded.
enum class LAPDmUFrameType : uint8_t {
    UI    = 0x03, ///< Unnumbered Information — carries unacknowledged data
    SABME = 0x2F, ///< Set Asynchronous Balanced Mode Extended — link establishment
    UA    = 0x63, ///< Unnumbered Acknowledgement — response to SABME/DISC
    DM    = 0x0F, ///< Disconnected Mode — reject when no link available
    DISC  = 0x08  ///< Disconnect — normal link release (F=1)
};

/// Supervisory frame types (GSM 04.06 4.4.2.1).
/// Values are the base control byte with NR=0 and P/F=0.
enum class LAPDmSFrameType : uint8_t {
    RR  = 0x01, ///< Receive Ready — base: 0000 0001
    REJ = 0x0D  ///< Reject — base: 0000 1101
};

/// LAPDm address field (GSM 04.06 4.2.1).
/// Bit layout: [SAPI(7:4)][C/R(3)][Reserved(2:1)=00][EA(0)]
struct LAPDmAddressField {
    SAPI sapi;   ///< Service Access Point Indicator (bits 7-4)
    bool command; ///< C/R bit: true=Command, false=Response (bit 3)
    bool ea;      ///< Extended Address bit (bit 0); normally 1

    constexpr LAPDmAddressField() noexcept : sapi(SAPI::SAPI0), command(false), ea(true) {}
    constexpr LAPDmAddressField(SAPI s, bool c, bool e) noexcept : sapi(s), command(c), ea(e) {}

    /// Encode address field to a single byte.
    constexpr uint8_t encode() const {
        return (static_cast<uint8_t>(sapi) << 4)
             | (command ? 0x08u : 0x00u)
             | (ea      ? 0x01u : 0x00u);
    }

    /// Decode a single address byte into its component fields.
    constexpr static LAPDmAddressField decode(uint8_t byte) {
        return LAPDmAddressField{
            static_cast<SAPI>((byte >> 4) & 0x0Fu),
            (byte & 0x08u) != 0,
            (byte & 0x01u) != 0
        };
    }
};

/// I-frame control field (GSM 04.06 4.4.1).
/// Bit layout: [NR(7:5)][P/F(4)][NS(2:0)][Fixed(3)=0]
struct LAPDmIControlField {
    uint8_t nr; ///< Receive sequence number (mod 8, bits 7-5)
    uint8_t ns; ///< Send sequence number (mod 8, bits 2-0)
    bool pf;    ///< Poll/Final bit (bit 4)

    constexpr LAPDmIControlField() noexcept : nr(0), ns(0), pf(false) {}
    constexpr LAPDmIControlField(uint8_t n, uint8_t s, bool p) noexcept : nr(n & 0x07u), ns(s & 0x07u), pf(p) {}

    /// Encode I-frame control field to a single byte.
    /// Layout: [NR(7:5)][P/F(4)][NS(3:1)][Fixed(0)=0]
    constexpr uint8_t encode() const {
        return (static_cast<uint8_t>(nr & 0x07u) << 5)
             | (pf            ? 0x10u : 0x00u)
             | ((static_cast<uint8_t>(ns & 0x07u)) << 1);
    }

    /// Decode an I-format control byte. Bit 0 must be 0.
    constexpr static LAPDmIControlField decode(uint8_t byte) {
        return LAPDmIControlField{
            static_cast<uint8_t>((byte >> 5) & 0x07u),
            static_cast<uint8_t>((byte >> 1) & 0x07u),
            (byte & 0x10u) != 0
        };
    }
};

/// S-frame control field (GSM 04.06 4.4.2.1).
/// Bit layout: [NR(7:5)][P/F(4)][Function(1:0)][Fixed(3)=1]
struct LAPDmSControlField {
    uint8_t nr;              ///< Receive sequence number (mod 8, bits 7-5)
    bool pf;                   ///< Poll/Final bit (bit 4)
    LAPDmSFrameType type;      ///< Function: RR(00) or REJ(10) (bits 1-0)

    constexpr LAPDmSControlField() noexcept : nr(0), pf(false), type(LAPDmSFrameType::RR) {}
    constexpr LAPDmSControlField(uint8_t n, bool p, LAPDmSFrameType t) noexcept
        : nr(n & 0x07u), pf(p), type(t) {}

    /// Encode S-frame control field to a single byte.
    /// NR occupies bits 7:5, P/F occupies bit 4, type provides the lower 4 bits.
    constexpr uint8_t encode() const {
        return (static_cast<uint8_t>(nr & 0x07u) << 5)
             | (pf                      ? 0x10u : 0x00u)
             | static_cast<uint8_t>(type);
    }

    /// Decode an S-format control byte.
    /// Extracts NR from bits 7:5, P/F from bit 4, and identifies type from lower nibble.
    constexpr static LAPDmSControlField decode(uint8_t byte) {
        uint8_t base = byte & 0x0Fu; // Lower 4 bits identify the S-frame type
        return LAPDmSControlField{
            static_cast<uint8_t>((byte >> 5) & 0x07u),
            (byte & 0x10u) != 0,
            (base == 0x0Du) ? LAPDmSFrameType::REJ : LAPDmSFrameType::RR
        };
    }
};

/// LAPDm length field for I-frames (GSM 04.06 5.5.2).
/// Bit layout: [M(7)][Reserved(6)=0][Length(5:0)]
/// M=1 indicates Message complete (last segment), M=0 means more segments follow.
struct LAPDmLengthField {
    bool m;          ///< Message complete bit: true=last segment, false=more follow
    uint8_t length;  ///< Info field length in octets (0-63)

    constexpr LAPDmLengthField() noexcept : m(false), length(0) {}
    constexpr LAPDmLengthField(bool msgComplete, uint8_t len) noexcept : m(msgComplete), length(len & 0x3Fu) {}

    /// Encode length field to a single byte.
    constexpr uint8_t encode() const {
        return (m       ? 0x80u : 0x00u)
             | (static_cast<uint8_t>(length & 0x3Fu));
    }

    /// Decode a length byte into M-bit and length value.
    constexpr static LAPDmLengthField decode(uint8_t byte) {
        return LAPDmLengthField{
            (byte & 0x80u) != 0,
            static_cast<uint8_t>(byte & 0x3Fu)
        };
    }
};

/// Decoded LAPDm frame — a non-owning, zero-copy view over the input buffer.
///
/// The `info` span points into the original buffer passed to decode(). The caller
/// must guarantee that the input buffer outlives any use of this struct. Never
/// store LAPDmFrame as a class member; use it only within the scope of receiveFrame().
///
/// Example:
/// \code
/// auto result = LAPDmFrame::decode(rawBytes);
/// if (result) {
///     auto& frame = *result;
///     if (frame.format == LAPDmControlFormat::U_Format &&
///         frame.uType == LAPDmUFrameType::UI) {
///         // Process UI payload from frame.info
///     }
/// }
/// \endcode
struct LAPDmFrame {
    LAPDmAddressField address;       ///< Decoded address field (SAPI, C/R, EA)
    LAPDmControlFormat format;       ///< Frame format: I, S, or U

    // U-frame specific fields
    LAPDmUFrameType uType;           ///< U-frame type (valid when format == U_Format)

    // Sequence numbers (valid for I and S frames)
    uint8_t nr;                      ///< Receive sequence number (NR, mod 8)
    uint8_t ns;                      ///< Send sequence number (NS, I-frames only, mod 8)

    // Control bits
    bool pf;                         ///< Poll/Final bit
    bool m;                          ///< Message complete bit (I-frames: M=1 = last segment)

    // S-frame specific
    LAPDmSFrameType sType;           ///< S-frame type (valid when format == S_Format)

    // Payload — zero-copy span into the original buffer
    std::span<const uint8_t> info;   ///< Info field bytes (empty if no payload)

    constexpr LAPDmFrame() noexcept
        : address{}, format(LAPDmControlFormat::U_Format),
          uType(LAPDmUFrameType::UI), nr(0), ns(0), pf(false), m(false),
          sType(LAPDmSFrameType::RR), info{} {}

    constexpr LAPDmFrame(LAPDmAddressField addr, LAPDmControlFormat fmt,
                         LAPDmUFrameType utype, uint8_t nrr, uint8_t nss,
                         bool pff, bool mm, LAPDmSFrameType stype,
                         std::span<const uint8_t> inf) noexcept
        : address(addr), format(fmt), uType(utype), nr(nrr & 0x07u), ns(nss & 0x07u),
          pf(pff), m(mm), sType(stype), info(inf) {}

    /// Get SAPI value from the address field.
    [[nodiscard]] constexpr SAPI sapi() const noexcept { return address.sapi; }

    /// Check if this is a command frame (C/R = 1).
    [[nodiscard]] constexpr bool isCommand() const noexcept { return address.command; }

    /// Check if the frame carries an info (payload) field.
    [[nodiscard]] constexpr bool hasInfo() const noexcept { return !info.empty(); }

    /// Return the number of payload octets.
    [[nodiscard]] constexpr size_t infoSize() const noexcept { return info.size(); }

    /// Decode a raw LAPDm frame from bytes.
    /// Returns a zero-copy view; the input span must remain valid for the lifetime
    /// of the returned LAPDmFrame.
    /// Minimum valid frame: 2 bytes (address + control). I-frames require 3+ bytes.
    [[nodiscard]] static Expected<LAPDmFrame> decode(std::span<const uint8_t> data);
};

/// Construct a UI (Unnumbered Information) frame.
/// GSM 04.06 5.2.1: carries unacknowledged L3 data.
[[nodiscard]] constexpr LAPDmFrame makeUIFrame(SAPI sapi, bool command,
                                                std::span<const uint8_t> info) {
    return LAPDmFrame{
        LAPDmAddressField{sapi, command, true},
        LAPDmControlFormat::U_Format,
        LAPDmUFrameType::UI,
        0, 0, false, false,
        LAPDmSFrameType::RR,
        info
    };
}

/// Construct a SABME (Set Asynchronous Balanced Mode Extended) frame.
/// GSM 04.06 5.4.1: initiates link establishment. When `info` is non-empty,
/// the encoded frame uses a length byte between control and payload
/// (contention resolution per GSM 04.06 5.4.1.4).
[[nodiscard]] constexpr LAPDmFrame makeSABMEFrame(SAPI sapi, bool command,
                                                   std::span<const uint8_t> info) {
    return LAPDmFrame{
        LAPDmAddressField{sapi, command, true},
        LAPDmControlFormat::U_Format,
        LAPDmUFrameType::SABME,
        0, 0, true, false,
        LAPDmSFrameType::RR,
        info
    };
}

/// Construct a UA (Unnumbered Acknowledgement) frame.
/// GSM 04.06 5.4.1.2: responds to SABME or DISC. When `info` is non-empty,
/// the encoded frame uses a length byte (echo for contention resolution).
[[nodiscard]] constexpr LAPDmFrame makeUAFrame(SAPI sapi, bool pf,
                                                std::span<const uint8_t> info) {
    return LAPDmFrame{
        LAPDmAddressField{sapi, false, true},
        LAPDmControlFormat::U_Format,
        LAPDmUFrameType::UA,
        0, 0, pf, false,
        LAPDmSFrameType::RR,
        info
    };
}

/// Construct a DM (Disconnected Mode) frame.
/// GSM 04.06 5.4.6: indicates the entity is not available.
[[nodiscard]] constexpr LAPDmFrame makeDMFrame(SAPI sapi, bool pf) {
    return LAPDmFrame{
        LAPDmAddressField{sapi, false, true},
        LAPDmControlFormat::U_Format,
        LAPDmUFrameType::DM,
        0, 0, pf, false,
        LAPDmSFrameType::RR,
        {}
    };
}

/// Construct a DISC (Disconnect) frame.
/// GSM 04.06 5.4.4: initiates normal link release.
[[nodiscard]] constexpr LAPDmFrame makeDISCFrame(SAPI sapi, bool command) {
    return LAPDmFrame{
        LAPDmAddressField{sapi, command, true},
        LAPDmControlFormat::U_Format,
        LAPDmUFrameType::DISC,
        0, 0, true, false,
        LAPDmSFrameType::RR,
        {}
    };
}

/// Construct an I-frame (Information frame).
/// GSM 04.06 5.5.2: carries acknowledged data with segmentation support.
/// The `m` parameter controls the M-bit: true = Message complete (last segment),
/// false = more segments follow.
[[nodiscard]] constexpr LAPDmFrame makeIFrame(SAPI sapi, bool command, uint8_t nr,
                                               uint8_t ns, bool pf, bool m,
                                               std::span<const uint8_t> info) {
    return LAPDmFrame{
        LAPDmAddressField{sapi, command, true},
        LAPDmControlFormat::I_Format,
        LAPDmUFrameType::UI,
        static_cast<uint8_t>(nr & 0x07u), static_cast<uint8_t>(ns & 0x07u), pf, m,
        LAPDmSFrameType::RR,
        info
    };
}

/// Construct an RR (Receive Ready) frame.
/// GSM 04.06 5.3.2: acknowledges received I-frames up to NR-1.
[[nodiscard]] constexpr LAPDmFrame makeRRFrame(SAPI sapi, uint8_t nr, bool pf) {
    return LAPDmFrame{
        LAPDmAddressField{sapi, false, true},
        LAPDmControlFormat::S_Format,
        LAPDmUFrameType::UI,
        static_cast<uint8_t>(nr & 0x07u), 0, pf, false,
        LAPDmSFrameType::RR,
        {}
    };
}

/// Construct a REJ (Reject) frame.
/// GSM 04.06 5.3.3: requests retransmission starting from NR.
[[nodiscard]] constexpr LAPDmFrame makeREJFrame(SAPI sapi, uint8_t nr, bool pf) {
    return LAPDmFrame{
        LAPDmAddressField{sapi, false, true},
        LAPDmControlFormat::S_Format,
        LAPDmUFrameType::UI,
        static_cast<uint8_t>(nr & 0x07u), 0, pf, false,
        LAPDmSFrameType::REJ,
        {}
    };
}

/// Encode a LAPDm frame to bytes (heap allocation).
/// Convenience function; for zero-allocation encoding on the hot path, use
/// encodeFrameToBuffer() with a pre-allocated buffer.
[[nodiscard]] std::vector<uint8_t> encodeFrame(const LAPDmFrame& frame);

/// Encode a LAPDm frame into a pre-allocated buffer (zero allocation).
/// Returns the number of bytes written, or 0 if the buffer is too small.
/// The caller must ensure `out` has at least `outSize` bytes available.
[[nodiscard]] size_t encodeFrameToBuffer(const LAPDmFrame& frame, uint8_t* out,
                                         size_t outSize);

} // namespace gsml3parser::lapdm
