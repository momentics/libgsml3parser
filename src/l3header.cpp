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

#include "gsml3parser/l3header.h"

namespace gsml3parser {

Expected<L3Header> parseL3Header(std::span<const uint8_t> data) {
    if (data.size() < 2) {
        return Expected<L3Header>::error(
            ParseError{ParseError::Code::TruncatedInput, "L3 header requires at least 2 bytes"});
    }

    L3Header hdr;

    // Byte 0: bits 7-4 = PD (high nibble)
    uint8_t byte0 = data[0];
    hdr.pd = static_cast<L3PD>((byte0 >> 4) & 0x0F);

    if (hdr.pd == L3PD::Undefined) {
        return Expected<L3Header>::error(
            ParseError{ParseError::Code::InvalidPD, "Invalid Protocol Discriminator"});
    }

    // Byte 0 low nibble: bits 2-4 = TI (3 bits), bit 0 = TIF (1 bit)
    // Matches BitVector layout: peekField(4,3) for TI, peekField(7,1) for TIF
    hdr.ti = (byte0 >> 1) & 0x07;
    hdr.tif = (byte0 & 0x01) != 0;

    // Byte 1: raw MTI
    uint8_t rawMti = data[1];

    // MM, CC, SS: byte 1 = messageType(6)|NSD(2), mask and shift
    if (hdr.pd == L3PD::MobilityManagement || hdr.pd == L3PD::CallControl ||
        hdr.pd == L3PD::NonCallSS) {
        hdr.mti = (rawMti & 0xFC) >> 2;
    }
    // GMM, SMS, SM: byte 1 = raw messageType(8), no NSD field
    else if (hdr.pd == L3PD::GPRSMobilityManagement || hdr.pd == L3PD::SMS ||
             hdr.pd == L3PD::GPRSSessionManagement) {
        hdr.mti = rawMti;
    }
    // RR short messages: TIF=1 indicates MTI >= 0x100
    else if (hdr.pd == L3PD::RadioResource && hdr.tif) {
        hdr.mti = 0x100 + (rawMti & 0xFF);
    }
    // RR normal: raw byte directly
    else {
        hdr.mti = rawMti;
    }

    return Expected<L3Header>::hold(hdr);
}

} // namespace gsml3parser
