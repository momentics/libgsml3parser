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

#include "gsml3parser/lapdm_frame.h"

namespace gsml3parser::lapdm {

namespace detail {

// Control octets for the five LAPDm U-frame types, index [type][P/F]
// (TS 44.064 U-format control field).
inline constexpr uint8_t kUControl[5][2] = {
    {0x03, 0x13}, // UI    U3='000', U2='00'
    {0x2F, 0x3F}, // SABME U3='001', U2='11'
    {0x0F, 0x1F}, // DM    U3='000', U2='11'
    {0x43, 0x53}, // DISC  U3='010', U2='00'
    {0x63, 0x73}  // UA    U3='011', U2='00'; low two bits are fixed to '11'.
};

// Locate a control byte in the U-frame table; returns -1 when it matches no
// canonical U-format octet.
inline int matchUControl(uint8_t ctrl) {
    for (int type = 0; type < 5; ++type) {
        for (int pf = 0; pf < 2; ++pf) {
            if (kUControl[type][static_cast<size_t>(pf)] == ctrl) return type * 2 + pf;
        }
    }
    return -1;
}

} // namespace detail

uint8_t uFrameControlByte(LAPDmUFrameType type, bool pf) noexcept {
    return detail::kUControl[static_cast<size_t>(type)][pf ? 1u : 0u];
}

Expected<LAPDmFrame> LAPDmFrame::decode(std::span<const uint8_t> data) {
    // Format B (TS 44.064): every frame carries three header octets —
    // address, control and the L/M/'1' header octet — before the info field.
    if (data.size() < 3) {
        return Expected<LAPDmFrame>::error(
            ParseError(ParseError::Code::TruncatedInput, "LAPDm frame too short"));
    }

    // Address octet: the high three bits are spare + GSM service access point
    // discriminator and must be zero; EA (bit 0) must be set on Um.
    if ((data[0] & 0xE0u) != 0) {
        return Expected<LAPDmFrame>::error(
            ParseError(ParseError::Code::InvalidValue, "Non-GSM service access point"));
    }
    if ((data[0] & 0x01u) == 0) {
        return Expected<LAPDmFrame>::error(
            ParseError(ParseError::Code::InvalidValue, "LAPDm EA bit not set"));
    }
    LAPDmAddressField addr = LAPDmAddressField::decode(data[0]);
    // Only SAPI 0 and SAPI 3 are defined on Um; any other three-bit value is
    // reported as Undefined so the entity can drop the frame by SAPI.
    const unsigned rawSapi = (data[0] >> 2) & 0x07u;
    if (rawSapi != 0 && rawSapi != 3) {
        addr.sapi = SAPI::Undefined;
    }

    // Header octet: bit 0 is the fixed '1' (frame octet / extension-length
    // indicator), bits 5:2 are L, bit 1 is M.
    if ((data[2] & 0x01u) == 0) {
        return Expected<LAPDmFrame>::error(
            ParseError(ParseError::Code::InvalidValue, "LAPDm header octet fixed bit not set"));
    }
    auto lenField = LAPDmLengthField::decode(data[2]);
    const size_t payloadLen = static_cast<size_t>(lenField.length);
    if (data.size() < 3 + payloadLen) {
        return Expected<LAPDmFrame>::error(
            ParseError(ParseError::Code::TruncatedInput, "LAPDm info field shorter than declared length"));
    }
    if (data.size() > 3 + payloadLen) {
        return Expected<LAPDmFrame>::error(
            ParseError(ParseError::Code::InvalidValue, "LAPDm frame has trailing bytes"));
    }

    LAPDmFrame frame;
    frame.address = addr;
    frame.more = lenField.more;
    frame.info = std::span<const uint8_t>(data.data() + 3, payloadLen);

    // Frame format from the low two bits of the control octet.
    const uint8_t ctrl = data[1];
    switch (ctrl & 0x03u) {
        case 0x00u:
        case 0x02u: {
            // I-frame: [NR(7:5)][P/F(4)][NS(3:1)][Fixed(0)=0]; the second bit is
            // the LSB of N(S), so both '00' and '10' are legal.
            frame.format = LAPDmControlFormat::I_Format;
            frame.iCtrl = LAPDmIControlField::decode(ctrl);
            break;
        }
        case 0x01u: {
            // S-frame: the header octet must be exactly 0x01 (L=0, M=0) since
            // S-frames carry no info field.
            if (data[2] != 0x01u) {
                return Expected<LAPDmFrame>::error(
                    ParseError(ParseError::Code::InvalidValue, "S-frame must have an empty header octet"));
            }
            frame.format = LAPDmControlFormat::S_Format;
            auto sCtrl = LAPDmSControlField::decode(ctrl);
            frame.nr = sCtrl.nr;
            frame.sType = sCtrl.type;
            frame.pf = sCtrl.pf;
            break;
        }
        case 0x03u: {
            // U-frame: the control octet must be one of the ten canonical
            // bytes (five types x two P/F values).
            int matched = detail::matchUControl(ctrl);
            if (matched < 0) {
                return Expected<LAPDmFrame>::error(
                    ParseError(ParseError::Code::InvalidValue, "Unknown U-frame control octet"));
            }
            frame.format = LAPDmControlFormat::U_Format;
            frame.uType = static_cast<LAPDmUFrameType>(matched / 2);
            frame.pf = (matched & 1) != 0;
            if ((frame.uType == LAPDmUFrameType::DM || frame.uType == LAPDmUFrameType::DISC) &&
                lenField.length != 0) {
                return Expected<LAPDmFrame>::error(
                    ParseError(ParseError::Code::InvalidValue, "DM/DISC frames carry no info field"));
            }
            break;
        }
    }

    return Expected<LAPDmFrame>::hold(frame);
}

size_t encodedFrameSize(const LAPDmFrame& frame) noexcept {
    // Format B: address + control + header octet, plus the info field. S-frames
    // carry no info field and are exactly three octets long.
    if (frame.format == LAPDmControlFormat::S_Format) return 3;
    return 3 + frame.info.size();
}

std::vector<uint8_t> encodeFrame(const LAPDmFrame& frame) {
    std::vector<uint8_t> out(encodedFrameSize(frame));
    size_t written = encodeFrameToBuffer(frame, out.data(), out.size());
    out.resize(written);
    return out;
}

size_t encodeFrameToBuffer(const LAPDmFrame& frame, uint8_t* out, size_t outSize) {
    // Compute the full frame size up front so that an undersized buffer is
    // rejected before anything is written.
    const size_t needed = encodedFrameSize(frame);
    if (outSize < needed) return 0;

    size_t offset = 0;

    // ── Address octet ──
    out[offset++] = frame.address.encode();

    // ── Control octet ──
    switch (frame.format) {
        case LAPDmControlFormat::I_Format:
            out[offset++] = frame.iCtrl.encode();
            break;
        case LAPDmControlFormat::S_Format:
            out[offset++] = LAPDmSControlField(frame.nr, frame.sType, frame.pf).encode();
            break;
        case LAPDmControlFormat::U_Format:
            out[offset++] = uFrameControlByte(frame.uType, frame.pf);
            break;
    }

    // ── Header octet (L/M/'1') and info field ──
    if (frame.format == LAPDmControlFormat::S_Format) {
        out[offset++] = 0x01u; // L=0, M=0, fixed '1'
        return offset;
    }
    auto lenField = LAPDmLengthField(frame.more, static_cast<uint8_t>(frame.info.size()));
    out[offset++] = lenField.encode();
    for (size_t i = 0; i < frame.info.size(); ++i) {
        out[offset++] = frame.info[i];
    }
    return offset;
}

} // namespace gsml3parser::lapdm
