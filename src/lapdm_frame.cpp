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

Expected<LAPDmFrame> LAPDmFrame::decode(std::span<const uint8_t> data) {
    // Minimum frame: address (1 byte) + control (1 byte) = 2 bytes
    if (data.size() < 2) {
        return Expected<LAPDmFrame>::error(
            ParseError(ParseError::Code::TruncatedInput, "LAPDm frame too short"));
    }

    LAPDmAddressField addr = LAPDmAddressField::decode(data[0]);
    uint8_t ctrl = data[1];

    // Check for DISC before I-frame check, since DISC (0x08/0x0C) has bit 0 = 0.
    if (ctrl == 0x08 || ctrl == 0x0C) {
        bool pf = (ctrl == 0x08);
        return Expected<LAPDmFrame>::hold(LAPDmFrame{
            addr, LAPDmControlFormat::U_Format, LAPDmUFrameType::DISC,
            0, 0, pf, false, LAPDmSFrameType::RR, std::span<const uint8_t>{}
        });
    }

    // ── I-frame: Fixed bit 0 = 0 ──
    // GSM 04.06 4.4.1: Control layout [NR(7:5)][P/F(4)][NS(3:1)][Fixed(0)=0]
    if ((ctrl & 0x01u) == 0) {
        if (data.size() < 3) {
            return Expected<LAPDmFrame>::error(
                ParseError(ParseError::Code::TruncatedInput, "I-frame missing length byte"));
        }

        auto iCtrl = LAPDmIControlField::decode(ctrl);
        auto lenField = LAPDmLengthField::decode(data[2]);
        size_t payloadLen = static_cast<size_t>(lenField.length);

        if (data.size() < 3 + payloadLen) {
            return Expected<LAPDmFrame>::error(
                ParseError(ParseError::Code::TruncatedInput,
                           "I-frame payload shorter than declared length"));
        }

        return Expected<LAPDmFrame>::hold(LAPDmFrame{
            addr, LAPDmControlFormat::I_Format, LAPDmUFrameType::UI,
            iCtrl.nr, iCtrl.ns, iCtrl.pf, lenField.m,
            LAPDmSFrameType::RR,
            std::span<const uint8_t>(data.data() + 3, payloadLen)
        });
    }

    // ── U-frame or S-frame: Fixed bit 0 = 1 ──
    // Try matching known U-frame control bytes first (all F=0/F=1 variants).
    if (ctrl == 0x03 || ctrl == 0x07) {
        // UI frame: [Address][Control][Info...]
        bool pf = (ctrl == 0x07);
        std::span<const uint8_t> info{};
        if (data.size() > 2) {
            info = std::span<const uint8_t>(data.data() + 2, data.size() - 2);
        }
        return Expected<LAPDmFrame>::hold(LAPDmFrame{
            addr, LAPDmControlFormat::U_Format, LAPDmUFrameType::UI,
            0, 0, pf, false, LAPDmSFrameType::RR, info
        });
    }

    if (ctrl == 0x2B || ctrl == 0x2F) {
        // SABME frame: may carry payload with length byte (GSM 04.06 5.4.1.4)
        bool pf = (ctrl == 0x2F);
        std::span<const uint8_t> info{};
        if (data.size() >= 3) {
            auto lenField = LAPDmLengthField::decode(data[2]);
            size_t payloadLen = static_cast<size_t>(lenField.length);
            if (data.size() < 3 + payloadLen) {
                return Expected<LAPDmFrame>::error(
                    ParseError(ParseError::Code::TruncatedInput,
                               "SABME payload shorter than declared length"));
            }
            info = std::span<const uint8_t>(data.data() + 3, payloadLen);
        }
        return Expected<LAPDmFrame>::hold(LAPDmFrame{
            addr, LAPDmControlFormat::U_Format, LAPDmUFrameType::SABME,
            0, 0, pf, false, LAPDmSFrameType::RR, info
        });
    }

    if (ctrl == 0x5F || ctrl == 0x63) {
        // UA frame: may carry echo payload with length byte
        bool pf = (ctrl == 0x63);
        std::span<const uint8_t> info{};
        if (data.size() >= 3) {
            auto lenField = LAPDmLengthField::decode(data[2]);
            size_t payloadLen = static_cast<size_t>(lenField.length);
            if (data.size() < 3 + payloadLen) {
                return Expected<LAPDmFrame>::error(
                    ParseError(ParseError::Code::TruncatedInput,
                               "UA payload shorter than declared length"));
            }
            info = std::span<const uint8_t>(data.data() + 3, payloadLen);
        }
        return Expected<LAPDmFrame>::hold(LAPDmFrame{
            addr, LAPDmControlFormat::U_Format, LAPDmUFrameType::UA,
            0, 0, pf, false, LAPDmSFrameType::RR, info
        });
    }

    if (ctrl == 0x0B || ctrl == 0x0F) {
        // DM frame: [Address][Control] — no payload
        bool pf = (ctrl == 0x0F);
        return Expected<LAPDmFrame>::hold(LAPDmFrame{
            addr, LAPDmControlFormat::U_Format, LAPDmUFrameType::DM,
            0, 0, pf, false, LAPDmSFrameType::RR, std::span<const uint8_t>{}
        });
    }

    // ── S-frame: [Address][Control] — no payload ──
    // GSM 04.06 4.4.2.1: [NR(7:5)][P/F(4)][Fixed(3)=1][Function(2:1)][Fixed(0)=1]
    auto sCtrl = LAPDmSControlField::decode(ctrl);

    return Expected<LAPDmFrame>::hold(LAPDmFrame{
        addr, LAPDmControlFormat::S_Format, LAPDmUFrameType::UI,
        sCtrl.nr, 0, sCtrl.pf, false, sCtrl.type, std::span<const uint8_t>{}
    });
}

std::vector<uint8_t> encodeFrame(const LAPDmFrame& frame) {
    // Calculate required buffer size
    size_t needed = 2; // address + control (minimum)

    if (frame.format == LAPDmControlFormat::I_Format) {
        needed += 1 + frame.info.size(); // length byte + payload
    } else if (frame.format == LAPDmControlFormat::U_Format) {
        if (frame.uType == LAPDmUFrameType::SABME ||
            frame.uType == LAPDmUFrameType::UA) {
            if (!frame.info.empty()) {
                needed += 1 + frame.info.size(); // length byte + info
            }
        } else {
            needed += frame.info.size(); // raw info bytes
        }
    }
    // S-frames: no extra bytes beyond address + control

    std::vector<uint8_t> out(needed);
    size_t written = encodeFrameToBuffer(frame, out.data(), out.size());
    out.resize(written);
    return out;
}

size_t encodeFrameToBuffer(const LAPDmFrame& frame, uint8_t* out, size_t outSize) {
    size_t offset = 0;

    // ── Address byte ──
    if (outSize < offset + 1) return 0;
    out[offset++] = frame.address.encode();

    // ── Control byte and payload ──
    switch (frame.format) {
        case LAPDmControlFormat::I_Format: {
            // I-frame: [Address][Control][Length][Info...]
            if (outSize < offset + 2) return 0;
            out[offset++] = LAPDmIControlField(frame.nr, frame.ns, frame.pf).encode();

            auto lenField = LAPDmLengthField(frame.m, static_cast<uint8_t>(frame.info.size()));
            out[offset++] = lenField.encode();

            if (outSize < offset + frame.info.size()) return 0;
            for (size_t i = 0; i < frame.info.size(); ++i) {
                out[offset++] = frame.info[i];
            }
            break;
        }

        case LAPDmControlFormat::S_Format: {
            // S-frame: [Address][Control] — no payload
            if (outSize < offset + 1) return 0;
            out[offset++] = LAPDmSControlField(frame.nr, frame.pf, frame.sType).encode();
            break;
        }

        case LAPDmControlFormat::U_Format: {
            // U-frame control byte: each type has specific values for pf=true and pf=false.
            uint8_t ctrl;
            if (frame.pf) {
                switch (frame.uType) {
                    case LAPDmUFrameType::UI:    ctrl = 0x07; break;
                    case LAPDmUFrameType::SABME: ctrl = 0x2F; break;
                    case LAPDmUFrameType::UA:    ctrl = 0x63; break;
                    case LAPDmUFrameType::DM:    ctrl = 0x0F; break;
                    case LAPDmUFrameType::DISC:  ctrl = 0x08; break;
                    default:                     ctrl = 0x00; break;
                }
            } else {
                switch (frame.uType) {
                    case LAPDmUFrameType::UI:    ctrl = 0x03; break;
                    case LAPDmUFrameType::SABME: ctrl = 0x2B; break;
                    case LAPDmUFrameType::UA:    ctrl = 0x5F; break;
                    case LAPDmUFrameType::DM:    ctrl = 0x0B; break;
                    case LAPDmUFrameType::DISC:  ctrl = 0x0C; break;
                    default:                     ctrl = 0x00; break;
                }
            }

            if (outSize < offset + 1) return 0;
            out[offset++] = ctrl;

            // SABME and UA with payload use length byte encoding (GSM 04.06 5.4.1.4)
            if ((frame.uType == LAPDmUFrameType::SABME ||
                 frame.uType == LAPDmUFrameType::UA) && !frame.info.empty()) {
                if (outSize < offset + 1) return 0;
                out[offset++] = LAPDmLengthField(false,
                               static_cast<uint8_t>(frame.info.size())).encode();

                if (outSize < offset + frame.info.size()) return 0;
                for (size_t i = 0; i < frame.info.size(); ++i) {
                    out[offset++] = frame.info[i];
                }
            } else if (frame.uType == LAPDmUFrameType::UI) {
                // UI: raw info bytes after control, no length byte
                if (outSize < offset + frame.info.size()) return 0;
                for (size_t i = 0; i < frame.info.size(); ++i) {
                    out[offset++] = frame.info[i];
                }
            }
            // DM and DISC: no info field
            break;
        }
    }

    return offset;
}

} // namespace gsml3parser::lapdm
