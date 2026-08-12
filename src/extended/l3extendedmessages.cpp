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

#include "gsml3parser/extended/l3extendedmessages.h"
#include <iomanip>

namespace gsml3parser {

// ── L3ExtendedMessage (GSM 04.08 PD=0x0e) ────────────────────────────

Expected<L3ExtendedMessage> L3ExtendedMessage::parse(BitReader& br, uint8_t parsedMti) {
    L3ExtendedMessage msg(static_cast<uint8_t>(parsedMti));
    // Read remaining body octets
    while (br.hasMore()) {
        auto b = br.readField(8);
        if (!b) return Expected<L3ExtendedMessage>::error(b.error());
        msg.mBody.push_back(static_cast<uint8_t>(b.value()));
    }
    return Expected<L3ExtendedMessage>::hold(std::move(msg));
}

void L3ExtendedMessage::write(BitWriter& bw) const {
    // Write body octets
    for (uint8_t b : mBody) bw.writeField(b, 8);
}

void L3ExtendedMessage::text(std::ostream& os) const {
    os << "ExtendedMessage(MTI=0x" << std::hex << static_cast<int>(mMti) << std::dec;
    if (!mBody.empty()) {
        os << ", body=" << mBody.size() << " octets";
    }
    os << ")";
}

} // namespace gsml3parser
