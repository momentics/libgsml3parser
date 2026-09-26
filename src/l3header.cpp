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
    const uint8_t byte0 = data[0];
    const uint8_t byte1 = data[1];

    // Protocol discriminator: low nibble of the first L3 octet. The four
    // high bits carry TI (3 bits) and the transaction indicator flag
    // (TS 24.007 Table 11.3).
    hdr.pd = static_cast<L3PD>(byte0 & 0x0F);
    if (static_cast<uint8_t>(hdr.pd) == 0x02 || static_cast<uint8_t>(hdr.pd) == 0x04 ||
        static_cast<uint8_t>(hdr.pd) == 0x07 || static_cast<uint8_t>(hdr.pd) == 0x0D) {
        // 0x02, 0x04, 0x07 and 0x0D are reserved PDs (TS 44.018 section 10.2);
        // parseL3Header rejects them with InvalidPD.
        return Expected<L3Header>::error(
            {ParseError::Code::InvalidPD, "Reserved Protocol Discriminator"});
    }
    hdr.ti = (byte0 >> 5) & 0x07;
    hdr.tif = ((byte0 >> 4) & 0x01) != 0;

    // Message type octet, per protocol discriminator.
    if (hdr.pd == L3PD::MobilityManagement || hdr.pd == L3PD::CallControl ||
        hdr.pd == L3PD::NonCallSS || hdr.pd == L3PD::BroadcastCallControl ||
        hdr.pd == L3PD::GroupCallControl) {
        // 6-bit message type in the low half-octet; the two high bits are
        // the network signalling indicator (informational, not exposed).
        hdr.mti = byte1 & 0x3F;
    } else if (hdr.pd == L3PD::RadioResource && hdr.tif) {
        // RR short message: 5-bit code in the low half-octet of a
        // reserved field (TS 44.018 Table 9.x short messages). The high
        // three bits are reserved and are masked away.
        hdr.mti = kRRTifShortBase | (byte1 & 0x1F);
    } else {
        // RR normal, GMM, SM, SMS, LCS and the extended/test PDs carry the
        // message type as a full octet.
        hdr.mti = byte1;
    }

    return Expected<L3Header>::hold(hdr);
}

} // namespace gsml3parser
