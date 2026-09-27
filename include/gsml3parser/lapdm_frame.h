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

/// LAPDm frame types and encoding/decoding (TS 44.064, format B on dedicated
/// channels). Provides zero-copy frame parsing via non-owning std::span views,
/// constexpr field encode/decode for compile-time constant evaluation, and
/// factory functions for constructing all LAPDm frame types (I, S, U).
///
/// Reference: GSM 04.06 / 3GPP TS 44.064
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "gsml3parser/expected.h"
#include "gsml3parser/types.h"

namespace gsml3parser::lapdm {

/// LAPDm control field format discriminator (TS 44.064 4.4).
/// The low two bits of the control octet determine the format:
///   '0x' or '10' = I-format (Information frame)
///   '01' = S-format (Supervisory frame)
///   '11' = U-format (Unnumbered frame)
enum class LAPDmControlFormat : uint8_t {
    I_Format, ///< Information frame (carries user data)
    S_Format, ///< Supervisory frame (RR, RNR, REJ — flow control)
    U_Format  ///< Unnumbered frame (UI, SABME, DM, DISC, UA)
};

/// Unnumbered frame types (TS 44.064 4.4.2.2). The canonical control octet of
/// each type is selected from the kUControl[type][P/F] table in the
/// implementation; both P/F variants decode to the same type.
enum class LAPDmUFrameType {
    UI,    ///< Unnumbered Information — carries unacknowledged data
    SABME, ///< Set Asynchronous Balanced Mode Extended — link establishment
    DM,    ///< Disconnected Mode — reject when no link available
    DISC,  ///< Disconnect — normal link release
    UA     ///< Unnumbered Acknowledgement — response to SABME/DISC
};

/// Supervisory frame types (TS 44.064 4.4.2.1): the two-bit S function code
/// carried in bits 3:2 of the control octet.
enum class LAPDmSFrameType : uint8_t {
    RR  = 0, ///< '00' — Receive Ready
    RNR = 1, ///< '01' — Receive Not Ready
    REJ = 2  ///< '10' — Reject
};

/// LAPDm address octet (TS 44.064, LAPDm frame header): the high three bits
/// are spare + GSM service access point discriminator and are zero; SAPI is
/// in the middle three bits; bit 1 is C/R; bit 0 is EA (always set on Um).
struct LAPDmAddressField {
    SAPI sapi;    ///< Service access point indicator; only 0 and 3 are defined.
    bool command; ///< C/R bit: true for command frames.
    bool ea;      ///< Extended address bit, must be set.

    constexpr LAPDmAddressField() noexcept : sapi(SAPI::Undefined), command(false), ea(true) {}
    constexpr LAPDmAddressField(SAPI s, bool c, bool e) noexcept : sapi(s), command(c), ea(e) {}

    [[nodiscard]] constexpr uint8_t encode() const noexcept {
        return static_cast<uint8_t>((static_cast<unsigned>(sapi) & 0x07u) << 2)
             | (command ? 0x02u : 0x00u)
             | (ea ? 0x01u : 0x00u);
    }

    [[nodiscard]] constexpr static LAPDmAddressField decode(uint8_t byte) noexcept {
        return LAPDmAddressField{
            static_cast<SAPI>((byte >> 2) & 0x07u),
            (byte & 0x02u) != 0,
            (byte & 0x01u) != 0};
    }

    [[nodiscard]] constexpr bool isGsm() const noexcept { return true; }
};

/// I-frame control field (GSM 04.06 4.4.1).
/// Bit layout: [NR(7:5)][P/F(4)][NS(3:1)][Fixed(0)=0]
struct LAPDmIControlField {
    uint8_t nr; ///< Receive sequence number (mod 8, bits 7-5)
    uint8_t ns; ///< Send sequence number (mod 8, bits 3-1)
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

/// S-format control octet (TS 44.064): [NR(7:5)][P/F(4)][S(3:2)] with the low
/// two bits fixed to '01'. S: RR = '00', RNR = '01', REJ = '10'.
struct LAPDmSControlField {
    uint8_t nr{0};
    LAPDmSFrameType type{LAPDmSFrameType::RR};
    bool pf{false};

    constexpr LAPDmSControlField() noexcept = default;
    constexpr LAPDmSControlField(uint8_t n, LAPDmSFrameType t, bool p) noexcept
        : nr(n & 0x07u), type(t), pf(p) {}

    [[nodiscard]] constexpr uint8_t encode() const noexcept {
        return static_cast<uint8_t>((nr & 0x07u) << 5 | (pf ? 0x10u : 0x00u)
                                    | (static_cast<uint8_t>(type) << 2) | 0x01u);
    }

    [[nodiscard]] constexpr static LAPDmSControlField decode(uint8_t byte) noexcept {
        return LAPDmSControlField{static_cast<uint8_t>((byte >> 5) & 0x07u),
                                  static_cast<LAPDmSFrameType>((byte >> 2) & 0x03u),
                                  (byte & 0x10u) != 0};
    }
};

/// Header octet carried by every format-B LAPDm frame (TS 44.064): six
/// length bits, the M bit (M=1 -> further I-frames of the same message
/// follow, M=0 -> final or only segment) and a fixed '1' in bit 0.
struct LAPDmLengthField {
    bool more{false};
    uint8_t length{0};

    constexpr LAPDmLengthField() noexcept = default;
    constexpr LAPDmLengthField(bool m, uint8_t l) noexcept : more(m), length(l & 0x3Fu) {}

    [[nodiscard]] constexpr uint8_t encode() const noexcept {
        return static_cast<uint8_t>((length & 0x3Fu) << 2 | (more ? 0x02u : 0x00u) | 0x01u);
    }

    [[nodiscard]] constexpr static LAPDmLengthField decode(uint8_t byte) noexcept {
        return LAPDmLengthField{(byte & 0x02u) != 0, static_cast<uint8_t>((byte >> 2) & 0x3Fu)};
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
    LAPDmAddressField address;                          ///< Decoded address field (SAPI, C/R, EA)
    LAPDmControlFormat format{LAPDmControlFormat::U_Format}; ///< Frame format: I, S, or U

    // I-frame specific
    LAPDmIControlField iCtrl{};                         ///< Control field (valid when format == I_Format)

    // S-frame specific
    uint8_t nr{0};                   ///< Receive sequence number (valid for S frames, mod 8)
    LAPDmSFrameType sType{LAPDmSFrameType::RR}; ///< S-frame type (valid when format == S_Format)

    // U-frame specific
    LAPDmUFrameType uType{LAPDmUFrameType::UI}; ///< U-frame type (valid when format == U_Format)

    // Control bits (valid for S and U frames; I-frames use iCtrl.pf)
    bool pf{false};                  ///< Poll/Final bit

    // Header octet
    bool more{false};                ///< M bit: true when further segments of the same
                                     ///< message follow, false for the final/only segment

    // Payload — zero-copy span into the original buffer
    std::span<const uint8_t> info{}; ///< Info field bytes (empty if no payload)

    constexpr LAPDmFrame() noexcept = default;

    /// Get SAPI value from the address field.
    [[nodiscard]] constexpr SAPI sapi() const noexcept { return address.sapi; }

    /// Check if this is a command frame (C/R = 1).
    [[nodiscard]] constexpr bool isCommand() const noexcept { return address.command; }

    /// Check if the frame carries an info (payload) field.
    [[nodiscard]] constexpr bool hasInfo() const noexcept { return !info.empty(); }

    /// Return the number of payload octets.
    [[nodiscard]] constexpr size_t infoSize() const noexcept { return info.size(); }

    /// Decode a raw LAPDm frame (format B, TS 44.064) from bytes:
    /// [address][control][header octet (L/M/'1')][info (L octets)].
    /// Returns a zero-copy view; the input span must remain valid for the lifetime
    /// of the returned LAPDmFrame. Every frame is at least 3 bytes.
    [[nodiscard]] static Expected<LAPDmFrame> decode(std::span<const uint8_t> data);
};

/// Canonical control octet for a U-frame type with the given P/F bit, per the
/// TS 44.064 U-format table (UI/SABME/DM/DISC/UA).
[[nodiscard]] uint8_t uFrameControlByte(LAPDmUFrameType type, bool pf) noexcept;

/// Construct a UI (Unnumbered Information) frame.
/// TS 44.064: carries unacknowledged L3 data.
[[nodiscard]] constexpr LAPDmFrame makeUIFrame(SAPI sapi, bool command,
                                                std::span<const uint8_t> info) {
    LAPDmFrame frame;
    frame.address = LAPDmAddressField{sapi, command, true};
    frame.format = LAPDmControlFormat::U_Format;
    frame.uType = LAPDmUFrameType::UI;
    frame.info = info;
    return frame;
}

/// Construct a SABME (Set Asynchronous Balanced Mode Extended) frame.
/// TS 44.064: initiates link establishment (P/F set, per the establishment
/// procedure). When `info` is non-empty it carries the contention-resolution
/// payload.
[[nodiscard]] constexpr LAPDmFrame makeSABMEFrame(SAPI sapi, bool command,
                                                    std::span<const uint8_t> info) {
    LAPDmFrame frame;
    frame.address = LAPDmAddressField{sapi, command, true};
    frame.format = LAPDmControlFormat::U_Format;
    frame.uType = LAPDmUFrameType::SABME;
    frame.pf = true;
    frame.info = info;
    return frame;
}

/// Construct a UA (Unnumbered Acknowledgement) frame.
/// TS 44.064: responds to SABME or DISC. When `info` is non-empty it echoes
/// the contention-resolution payload.
[[nodiscard]] constexpr LAPDmFrame makeUAFrame(SAPI sapi, bool pf,
                                                std::span<const uint8_t> info) {
    LAPDmFrame frame;
    frame.address = LAPDmAddressField{sapi, false, true};
    frame.format = LAPDmControlFormat::U_Format;
    frame.uType = LAPDmUFrameType::UA;
    frame.pf = pf;
    frame.info = info;
    return frame;
}

/// Construct a DM (Disconnected Mode) frame.
/// TS 44.064: indicates the entity is not available.
[[nodiscard]] constexpr LAPDmFrame makeDMFrame(SAPI sapi, bool pf) {
    LAPDmFrame frame;
    frame.address = LAPDmAddressField{sapi, false, true};
    frame.format = LAPDmControlFormat::U_Format;
    frame.uType = LAPDmUFrameType::DM;
    frame.pf = pf;
    return frame;
}

/// Construct a DISC (Disconnect) frame.
/// TS 44.064: initiates normal link release (P/F set, per the release
/// procedure).
[[nodiscard]] constexpr LAPDmFrame makeDISCFrame(SAPI sapi, bool command) {
    LAPDmFrame frame;
    frame.address = LAPDmAddressField{sapi, command, true};
    frame.format = LAPDmControlFormat::U_Format;
    frame.uType = LAPDmUFrameType::DISC;
    frame.pf = true;
    return frame;
}

/// Construct an I-frame (Information frame).
/// TS 44.064: carries acknowledged data with segmentation support. The `more`
/// parameter is the M bit of the header octet: true when further segments of
/// the same message follow, false for the final or only segment.
[[nodiscard]] constexpr LAPDmFrame makeIFrame(SAPI sapi, bool command, uint8_t nr,
                                                uint8_t ns, bool pf, bool more,
                                                std::span<const uint8_t> info) {
    LAPDmFrame frame;
    frame.address = LAPDmAddressField{sapi, command, true};
    frame.format = LAPDmControlFormat::I_Format;
    frame.iCtrl = LAPDmIControlField{nr, ns, pf};
    frame.more = more;
    frame.info = info;
    return frame;
}

/// Construct an RR (Receive Ready) frame.
/// TS 44.064: acknowledges received I-frames up to NR-1.
[[nodiscard]] constexpr LAPDmFrame makeRRFrame(SAPI sapi, uint8_t nr, bool pf) {
    LAPDmFrame frame;
    frame.address = LAPDmAddressField{sapi, false, true};
    frame.format = LAPDmControlFormat::S_Format;
    frame.nr = nr & 0x07u;
    frame.pf = pf;
    frame.sType = LAPDmSFrameType::RR;
    return frame;
}

/// Construct an RNR (Receive Not Ready) frame.
/// TS 44.064: signals that the receiver cannot accept I-frames at the moment;
/// no response is required.
[[nodiscard]] constexpr LAPDmFrame makeRNRFrame(SAPI sapi, uint8_t nr, bool pf) {
    LAPDmFrame frame;
    frame.address = LAPDmAddressField{sapi, false, true};
    frame.format = LAPDmControlFormat::S_Format;
    frame.nr = nr & 0x07u;
    frame.pf = pf;
    frame.sType = LAPDmSFrameType::RNR;
    return frame;
}

/// Construct a REJ (Reject) frame.
/// TS 44.064: requests retransmission starting from NR.
[[nodiscard]] constexpr LAPDmFrame makeREJFrame(SAPI sapi, uint8_t nr, bool pf) {
    LAPDmFrame frame;
    frame.address = LAPDmAddressField{sapi, false, true};
    frame.format = LAPDmControlFormat::S_Format;
    frame.nr = nr & 0x07u;
    frame.pf = pf;
    frame.sType = LAPDmSFrameType::REJ;
    return frame;
}

/// Encode a LAPDm frame to bytes (heap allocation).
/// Convenience function; for zero-allocation encoding on the hot path, use
/// encodeFrameToBuffer() with a pre-allocated buffer.
[[nodiscard]] std::vector<uint8_t> encodeFrame(const LAPDmFrame& frame);

/// Encode a LAPDm frame into a pre-allocated buffer (zero allocation).
/// The encoded frame is exactly 3 + info.size() bytes: address, control and
/// the header octet (L/M/'1'), then the info field. Returns the number of
/// bytes written, or 0 (without writing) when the buffer is too small.
[[nodiscard]] size_t encodeFrameToBuffer(const LAPDmFrame& frame, uint8_t* out,
                                          size_t outSize);

} // namespace gsml3parser::lapdm
