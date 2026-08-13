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

#include "gsml3parser/lapdm.h"

namespace gsml3parser::lapdm {

uint8_t makeAddress(SAPI sapi, bool cr, bool ea) {
    return (static_cast<uint8_t>(sapi) << 4) | (cr ? 0x08 : 0x00) | (ea ? 0x01 : 0x00);
}

std::vector<uint8_t> wrapL3(std::span<const uint8_t> l3Body, SAPI sapi, bool cr) {
    std::vector<uint8_t> frame;
    frame.reserve(l3Body.size() + 2);
    frame.push_back(makeAddress(sapi, cr, true));
    frame.push_back(static_cast<uint8_t>(ControlField::UI));
    frame.insert(frame.end(), l3Body.begin(), l3Body.end());
    return frame;
}

Expected<std::vector<uint8_t>> unwrapL3(std::span<const uint8_t> lapdmFrame) {
    if (lapdmFrame.size() < 2) {
        return Expected<std::vector<uint8_t>>::error(
            ParseError(ParseError::Code::TruncatedInput, "LAPDm frame too short"));
    }
    std::vector<uint8_t> payload(lapdmFrame.begin() + 2, lapdmFrame.end());
    return Expected<std::vector<uint8_t>>::hold(std::move(payload));
}

SAPI extractSAPI(uint8_t addrByte) {
    return static_cast<SAPI>((addrByte >> 4) & 0x0F);
}

bool extractCR(uint8_t addrByte) {
    return (addrByte & 0x08) != 0;
}

bool isUIFrame(std::span<const uint8_t> lapdmFrame) {
    return lapdmFrame.size() >= 2 && lapdmFrame[1] == static_cast<uint8_t>(ControlField::UI);
}

} // namespace gsml3parser::lapdm
