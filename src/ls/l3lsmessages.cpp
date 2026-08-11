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

#include "gsml3parser/ls/l3lsmessages.h"
#include <iomanip>

namespace gsml3parser {

// ── L3LocationServiceRequest (TS 44.031 §9.1.2, MTI=0x01) ─────────────

Expected<L3LocationServiceRequest> L3LocationServiceRequest::parse(BitReader& br) {
    L3LocationServiceRequest msg;
    while (br.hasMore()) {
        auto b = br.readField(8);
        if (!b) return Expected<L3LocationServiceRequest>::error(b.error());
        msg.mBody.push_back(static_cast<uint8_t>(b.value()));
    }
    return Expected<L3LocationServiceRequest>::hold(std::move(msg));
}

void L3LocationServiceRequest::write(BitWriter& bw) const {
    for (uint8_t b : mBody) bw.writeField(b, 8);
}

void L3LocationServiceRequest::text(std::ostream& os) const {
    os << "LocationServiceRequest";
    if (!mBody.empty()) {
        os << " [" << mBody.size() << " octets]";
    }
}

// ── L3LocationServiceProviderMessage (TS 44.031 §9.1.3, MTI=0x02) ──────

Expected<L3LocationServiceProviderMessage> L3LocationServiceProviderMessage::parse(BitReader& br) {
    L3LocationServiceProviderMessage msg;
    while (br.hasMore()) {
        auto b = br.readField(8);
        if (!b) return Expected<L3LocationServiceProviderMessage>::error(b.error());
        msg.mBody.push_back(static_cast<uint8_t>(b.value()));
    }
    return Expected<L3LocationServiceProviderMessage>::hold(std::move(msg));
}

void L3LocationServiceProviderMessage::write(BitWriter& bw) const {
    for (uint8_t b : mBody) bw.writeField(b, 8);
}

void L3LocationServiceProviderMessage::text(std::ostream& os) const {
    os << "LocationServiceProviderMessage";
    if (!mBody.empty()) {
        os << " [" << mBody.size() << " octets]";
    }
}

} // namespace gsml3parser
