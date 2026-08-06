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

#include "gsml3parser/l3message.h"
#include <sstream>
#include <iomanip>

namespace gsml3parser {

ParseResult<void> L3Message::parse(const L3Frame& source) {
    size_t rp = 16;  // skip L3 header (4 bits TI + 4 bits PD + 8 bits MTI)
    return try_parseBody(source, rp);
}

ParseResult<void> L3Message::write(L3Frame& dest) const {
    size_t l3len = bitsNeeded();
    if (dest.size() != l3len) dest.resize(l3len);
    size_t wp = 0;
    dest.writeField(wp, static_cast<unsigned>(pd()), 4);  // PD: high nibble
    dest.writeField(wp, ti(), 3);                          // TIO: 3 bits
    L3PD pdVal = pd();
    int mtiVal = mti();
    bool isShort = (pdVal == L3PD::RadioResource && mtiVal >= 0x100);
    dest.writeField(wp, isShort ? 1u : 0u, 1);            // TIF: 1 bit (1 = short message)
    if (pdVal == L3PD::RadioResource) {
        dest.writeField(wp, isShort ? (mtiVal & 0xFF) : mtiVal, 8);  // RR: 8-bit MTI
    } else {
        dest.writeField(wp, mtiVal << 2, 8);               // MM/CC/SS: messageType(6)|NSD(2)
    }
    auto res = try_writeBody(dest, wp);
    if (!res.has_value()) return res;
    dest.l2Length(l2Length());
    return ParseResult<void>();
}

std::unique_ptr<L3Frame> L3Message::frame(Primitive prim) const {
    auto newFrame = std::make_unique<L3Frame>(prim, bitsNeeded());
    write(*newFrame);
    return newFrame;
}

void L3Message::text(std::ostream& os) const {
    os << "PD=" << pd() << " MTI=" << mti();
}

std::string L3Message::text() const {
    std::ostringstream ss;
    text(ss);
    return ss.str();
}

ParseResult<void> L3Message::try_writeBody(L3Frame&, size_t&) const {
    return ParseResult<void>(ParseErrorCode::UnsupportedFeature, "writeBody not implemented for this message type");
}

ParseResult<void> L3Message::try_parseBody(const L3Frame&, size_t&) {
    return ParseResult<void>(ParseErrorCode::UnsupportedFeature, "parseBody not implemented for this message type");
}

// ── Utility functions ───────────────────────────────────────────────────

size_t skipLV(const L3Frame& source, size_t& rp) {
    if (rp == source.size()) return 0;
    size_t base = rp;
    size_t length = 8 * source.readField(rp, 8);
    rp += length;
    return rp - base;
}

size_t skipTLV(unsigned IEI, const L3Frame& source, size_t& rp) {
    if (rp == source.size()) return 0;
    size_t base = rp;
    unsigned thisIEI = source.peekField(rp, 8);
    if (thisIEI != IEI) return 0;
    rp += 8;
    size_t length = 8 * source.readField(rp, 8);
    rp += length;
    return rp - base;
}

size_t skipTV(unsigned IEI, size_t numBits, const L3Frame& source, size_t& rp) {
    if (rp == source.size()) return 0;
    size_t base = rp;
    size_t ieSize = (numBits > 4) ? 8 : 4;
    unsigned thisIEI = source.peekField(rp, static_cast<unsigned>(ieSize));
    if (thisIEI != IEI) return 0;
    rp += ieSize;
    rp += numBits;
    return rp - base;
}

bool parseHasT(unsigned IEI, const L3Frame& source, size_t& rp) {
    if (rp == source.size()) return false;
    unsigned thisIEI = source.peekField(rp, 8);
    return thisIEI == IEI;
}

// ── L3ProtocolElement ───────────────────────────────────────────────────

ParseResult<void> L3ProtocolElement::try_parseLV(const L3Frame& source, size_t& rp) {
    size_t expectedLength = source.readField(rp, 8);
    if (expectedLength == 0) return ParseResult<void>();
    size_t rpEnd = rp + 8 * expectedLength;
    auto res = try_parseV(source, rp, expectedLength);
    if (!res.has_value()) return res;
    if (rpEnd != rp) {
        return ParseResult<void>(ParseErrorCode::LengthMismatch,
            "LV element length mismatch: " + std::to_string(rpEnd) + "!=" + std::to_string(rp), rp);
    }
    return ParseResult<void>();
}

ParseResult<bool> L3ProtocolElement::try_parseTV(unsigned IEI, const L3Frame& source, size_t& rp) {
    if (rp == source.size()) return ParseResult<bool>(false);
    if (lengthV() == 0) {
        unsigned thisIEI = source.peekField(rp, 4);
        if (thisIEI != IEI) return ParseResult<bool>(false);
        rp += 4;
        auto res = try_parseV(source, rp);
        if (!res.has_value()) return ParseResult<bool>(res.error());
        return ParseResult<bool>(true);
    }
    unsigned thisIEI = source.peekField(rp, 8);
    if (thisIEI != IEI) return ParseResult<bool>(false);
    rp += 8;
    auto res = try_parseV(source, rp);
    if (!res.has_value()) return ParseResult<bool>(res.error());
    return ParseResult<bool>(true);
}

ParseResult<bool> L3ProtocolElement::try_parseTLV(unsigned IEI, const L3Frame& source, size_t& rp) {
    if (rp == source.size()) return ParseResult<bool>(false);
    unsigned thisIEI = source.peekField(rp, 8);
    if (thisIEI != IEI) return ParseResult<bool>(false);
    rp += 8;
    auto res = try_parseLV(source, rp);
    if (!res.has_value()) return ParseResult<bool>(res.error());
    return ParseResult<bool>(true);
}

void L3ProtocolElement::writeLV(L3Frame& dest, size_t& wp) const {
    unsigned len = static_cast<unsigned>(lengthV());
    dest.writeField(wp, len, 8);
    if (len) writeV(dest, wp);
}

void L3ProtocolElement::writeTV(unsigned IEI, L3Frame& dest, size_t& wp) const {
    if (lengthV() == 0) {
        dest.writeField(wp, IEI, 4);
        writeV(dest, wp);
        return;
    }
    dest.writeField(wp, IEI, 8);
    writeV(dest, wp);
}

void L3ProtocolElement::writeTLV(unsigned IEI, L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, IEI, 8);
    writeLV(dest, wp);
}

void L3ProtocolElement::skipExtendedOctets(const L3Frame& source, size_t& rp) {
    if (rp == source.size()) return;
    int endbit = 0;
    while (!endbit) {
        endbit = source.readField(rp, 1);
        rp += 7;
    }
}

// ── L3OctetAlignedProtocolElement ──────────────────────────────────────

void L3OctetAlignedProtocolElement::writeV(L3Frame& dest, size_t& wp) const {
    const unsigned char* data = peData();
    for (size_t i = 0; i < mData.size(); ++i) {
        dest.writeField(wp, data[i], 8);
    }
}

ParseResult<void> L3OctetAlignedProtocolElement::try_parseV(const L3Frame& src, size_t& rp, size_t expectedLength) {
    mExtant = true;
    if (!expectedLength) return ParseResult<void>();
    std::vector<uint8_t> tmp(expectedLength);
    for (size_t i = 0; i < expectedLength; ++i) {
        tmp[i] = static_cast<uint8_t>(src.readField(rp, 8));
    }
    mData.assign(tmp.begin(), tmp.end());
    return ParseResult<void>();
}

void L3OctetAlignedProtocolElement::text(std::ostream& os) const {
    if (!mExtant) return;
    const unsigned char* d = peData();
    for (size_t i = 0; i < mData.size(); ++i) {
        os << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(d[i]);
    }
}

// ── Stream operators ────────────────────────────────────────────────────

std::ostream& operator<<(std::ostream& os, const L3Message& msg) {
    msg.text(os);
    return os;
}

std::ostream& operator<<(std::ostream& os, const L3Message* msg) {
    if (msg) msg->text(os);
    else os << "null";
    return os;
}

std::ostream& operator<<(std::ostream& os, const L3ProtocolElement& elem) {
    elem.text(os);
    return os;
}

std::ostream& operator<<(std::ostream& os, const GenericMessageElement& elem) {
    elem.text(os);
    return os;
}

} // namespace gsml3parser
