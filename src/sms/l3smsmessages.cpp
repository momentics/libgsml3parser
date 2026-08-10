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

// SMS CP & RP Messages — parse/write implementation
// Spec: 3GPP TS 24.008 section 9.6, Table 10.6a; 3GPP TS 24.011 sections 7-8
// Reference: ref/osmo-ttcn3-hacks/library/L3_Templates.ttcn — SMS templates (lines 3513-3739)
//            ref/OpenBTS/SMS/SMSMessages.h — CP/RP message classes

#include "gsml3parser/sms/l3smsmessages.h"
#include <sstream>
#include <iomanip>

namespace gsml3parser {

// ── CP Cause string converter ──────────────────────────────────────────

const char* CPCause2Str(CPCause cause) {
    switch (cause) {
        case CPCause::Unspecified: return "Unspecified";
        case CPCause::CpusNotSupported: return "CP-User-Part not supported";
        case CPCause::NoRPLPDU: return "No RP-LPDU";
        case CPCause::UnknownRPMessageType: return "Unknown RP message type";
        case CPCause::InvalidRPMessageReference: return "Invalid RP message reference";
        case CPCause::RPUserBusy: return "RP-User busy";
        case CPCause::UnknownRPOriginatorAddress: return "Unknown RP originator address";
        case CPCause::UnknownRPDestinationAddress: return "Unknown RP destination address";
        case CPCause::RPLinkNotAvailable: return "RP-Link not available";
        case CPCause::NoRPResponse: return "No RP response";
    }
    return "Unknown";
}

// ── L3CPData (24.011 8.1.2) ───────────────────────────────────────────
// Body: CP-User-Data-Length(1) | CP-User-Data(variable) = RPDU

size_t L3CPData::bodyLength() const {
    return 1 + mRpdu.size();
}

Expected<L3CPData> L3CPData::parse(BitReader& br) {
    L3CPData msg;

    auto len = br.readField(8);
    if (!len) return Expected<L3CPData>::error(len.error());
    size_t rpduLen = len.value();

    for (size_t i = 0; i < rpduLen && br.hasMore(); ++i) {
        auto octet = br.readField(8);
        if (!octet) return Expected<L3CPData>::error(octet.error());
        msg.mRpdu.push_back(static_cast<uint8_t>(octet.value()));
    }

    return Expected<L3CPData>::hold(std::move(msg));
}

void L3CPData::write(BitWriter& bw) const {
    bw.writeField(static_cast<uint32_t>(mRpdu.size()), 8);
    for (uint8_t b : mRpdu) {
        bw.writeField(b, 8);
    }
}

void L3CPData::text(std::ostream& os) const {
    os << "CP-Data(rpdu-len=" << mRpdu.size() << ")";
}

// ── L3CPAck (24.011 8.1.3) ────────────────────────────────────────────

Expected<L3CPAck> L3CPAck::parse(BitReader&) {
    return Expected<L3CPAck>::hold(L3CPAck{});
}

void L3CPAck::write(BitWriter&) const {}

void L3CPAck::text(std::ostream& os) const {
    os << "CP-ACK";
}

// ── L3CPErr (24.011 8.1.4) ────────────────────────────────────────────

Expected<L3CPErr> L3CPErr::parse(BitReader& br) {
    L3CPErr msg;

    auto cause = br.readField(8);
    if (!cause) return Expected<L3CPErr>::error(cause.error());
    msg.mCause = static_cast<CPCause>(cause.value() & 0x7F);

    return Expected<L3CPErr>::hold(std::move(msg));
}

void L3CPErr::write(BitWriter& bw) const {
    bw.writeField(static_cast<uint8_t>(mCause), 8);
}

void L3CPErr::text(std::ostream& os) const {
    os << "CP-ERROR(cause=" << CPCause2Str(mCause) << ")";
}

// ── L3CPStatus (24.011 8.1.5) ─────────────────────────────────────────

size_t L3CPStatus::bodyLength() const {
    return 2 + (mHaveMessageRef ? 1 : 0);
}

Expected<L3CPStatus> L3CPStatus::parse(BitReader& br) {
    L3CPStatus msg;

    auto tpOi = br.readField(8);
    if (!tpOi) return Expected<L3CPStatus>::error(tpOi.error());
    msg.mTpOi = static_cast<uint8_t>(tpOi.value());

    auto mti = br.readField(8);
    if (!mti) return Expected<L3CPStatus>::error(mti.error());
    msg.mMti = static_cast<uint8_t>(mti.value());

    if (msg.mMti & 0x02) {
        auto ref = br.readField(8);
        if (!ref) return Expected<L3CPStatus>::error(ref.error());
        msg.mHaveMessageRef = true;
        msg.mMessageRef = static_cast<uint8_t>(ref.value());
    }

    return Expected<L3CPStatus>::hold(std::move(msg));
}

void L3CPStatus::write(BitWriter& bw) const {
    bw.writeField(mTpOi, 8);
    bw.writeField(mMti, 8);
    if (mHaveMessageRef) {
        bw.writeField(mMessageRef, 8);
    }
}

void L3CPStatus::text(std::ostream& os) const {
    os << "CP-STATUS(tpOi=" << static_cast<int>(mTpOi)
       << ",mti=" << static_cast<int>(mMti);
    if (mHaveMessageRef) {
        os << ",ref=" << static_cast<int>(mMessageRef);
    }
    os << ")";
}

// ── L3CPSMT (24.011 8.1.6) ───────────────────────────────────────────

size_t L3CPSMT::bodyLength() const {
    return 1 + mRpdu.size();
}

Expected<L3CPSMT> L3CPSMT::parse(BitReader& br) {
    L3CPSMT msg;

    auto len = br.readField(8);
    if (!len) return Expected<L3CPSMT>::error(len.error());
    size_t rpduLen = len.value();

    for (size_t i = 0; i < rpduLen && br.hasMore(); ++i) {
        auto octet = br.readField(8);
        if (!octet) return Expected<L3CPSMT>::error(octet.error());
        msg.mRpdu.push_back(static_cast<uint8_t>(octet.value()));
    }

    return Expected<L3CPSMT>::hold(std::move(msg));
}

void L3CPSMT::write(BitWriter& bw) const {
    bw.writeField(static_cast<uint32_t>(mRpdu.size()), 8);
    for (uint8_t b : mRpdu) {
        bw.writeField(b, 8);
    }
}

void L3CPSMT::text(std::ostream& os) const {
    os << "CP-SMT(rpdu-len=" << mRpdu.size() << ")";
}

// ── L3RPData (24.011 7.3.1) ───────────────────────────────────────────
// Header: Spare(5)|RP-MTI(3) | RP-Message-Reference(1)
// Body: [RP-Originator-Address(LV)] | [RP-Destination-Address(LV)] | RP-User-Data(LV)

size_t L3RPData::bodyLength() const {
    size_t len = 2; // header: spare+mti + message-ref

    if (mOriginatorAddress) {
        len += mOriginatorAddress->totalLength();
    }
    if (mDestinationAddress) {
        len += mDestinationAddress->totalLength();
    }
    len += 1 + mUserData.size(); // RP-User-Data: length byte + data

    return len;
}

Expected<L3RPData> L3RPData::parse(BitReader& br) {
    L3RPData msg;

    auto hdr = br.readField(8);
    if (!hdr) return Expected<L3RPData>::error(hdr.error());
    msg.mRpMti = static_cast<uint8_t>(hdr.value() & 0x07);

    auto ref = br.readField(8);
    if (!ref) return Expected<L3RPData>::error(ref.error());
    msg.mMessageRef = static_cast<uint8_t>(ref.value());

    // Read remaining body as LV elements.
    // Collect all remaining bytes into a buffer, then parse LV elements.
    std::vector<uint8_t> bodyBytes;
    while (br.hasMore()) {
        auto b = br.readField(8);
        if (!b) return Expected<L3RPData>::error(b.error());
        bodyBytes.push_back(static_cast<uint8_t>(b.value()));
    }

    // Parse LV elements from bodyBytes
    size_t pos = 0;
    std::vector<std::pair<size_t, size_t>> lvElements;

    while (pos < bodyBytes.size()) {
        uint8_t lenByte = bodyBytes[pos];
        if (pos + 1 + lenByte > bodyBytes.size()) break;
        lvElements.push_back({pos, static_cast<size_t>(1 + lenByte)});
        pos += 1 + lenByte;
    }

    // Assign LV elements: originator, destination, user-data
    if (lvElements.size() >= 3) {
        BitReader addrBr(&bodyBytes[lvElements[0].first], lvElements[0].second * 8);
        auto oaResult = L3TPAddress::parse(addrBr);
        if (!oaResult) return Expected<L3RPData>::error(oaResult.error());
        msg.mOriginatorAddress = std::move(oaResult.value());

        BitReader dstBr(&bodyBytes[lvElements[1].first], lvElements[1].second * 8);
        auto daResult = L3TPAddress::parse(dstBr);
        if (!daResult) return Expected<L3RPData>::error(daResult.error());
        msg.mDestinationAddress = std::move(daResult.value());

        size_t udStart = lvElements.back().first + 1;
        msg.mUserData.assign(bodyBytes.begin() + udStart, bodyBytes.begin() + udStart + bodyBytes[udStart - 1]);
    } else if (lvElements.size() == 2) {
        BitReader addrBr(&bodyBytes[lvElements[0].first], lvElements[0].second * 8);
        auto oaResult = L3TPAddress::parse(addrBr);
        if (oaResult) msg.mOriginatorAddress = std::move(oaResult.value());

        size_t udStart = lvElements.back().first + 1;
        msg.mUserData.assign(bodyBytes.begin() + udStart, bodyBytes.begin() + udStart + bodyBytes[udStart - 1]);
    } else if (lvElements.size() >= 1) {
        size_t udStart = lvElements.back().first + 1;
        msg.mUserData.assign(bodyBytes.begin() + udStart, bodyBytes.begin() + udStart + bodyBytes[udStart - 1]);
    }

    return Expected<L3RPData>::hold(std::move(msg));
}

void L3RPData::write(BitWriter& bw) const {
    bw.writeField(mRpMti & 0x07, 8);
    bw.writeField(mMessageRef, 8);

    if (mOriginatorAddress) {
        mOriginatorAddress->write(bw);
    }
    if (mDestinationAddress) {
        mDestinationAddress->write(bw);
    }

    bw.writeField(static_cast<uint32_t>(mUserData.size()), 8);
    for (uint8_t b : mUserData) {
        bw.writeField(b, 8);
    }
}

void L3RPData::text(std::ostream& os) const {
    os << "RP-Data(mti=" << static_cast<int>(mRpMti)
       << ",ref=" << static_cast<int>(mMessageRef)
       << ",ud-len=" << mUserData.size() << ")";
}

// ── L3RPAck (24.011 7.3.2) ────────────────────────────────────────────

size_t L3RPAck::bodyLength() const {
    return 2;
}

Expected<L3RPAck> L3RPAck::parse(BitReader& br) {
    L3RPAck msg;

    auto hdr = br.readField(8);
    if (!hdr) return Expected<L3RPAck>::error(hdr.error());
    msg.mRpMti = static_cast<uint8_t>(hdr.value() & 0x07);

    auto ref = br.readField(8);
    if (!ref) return Expected<L3RPAck>::error(ref.error());
    msg.mMessageRef = static_cast<uint8_t>(ref.value());

    return Expected<L3RPAck>::hold(std::move(msg));
}

void L3RPAck::write(BitWriter& bw) const {
    bw.writeField(mRpMti & 0x07, 8);
    bw.writeField(mMessageRef, 8);
}

void L3RPAck::text(std::ostream& os) const {
    os << "RP-ACK(mti=" << static_cast<int>(mRpMti)
       << ",ref=" << static_cast<int>(mMessageRef) << ")";
}

// ── L3RPError (24.011 7.3.4) ──────────────────────────────────────────

size_t L3RPError::bodyLength() const {
    return 2 + 2;
}

Expected<L3RPError> L3RPError::parse(BitReader& br) {
    L3RPError msg;

    auto hdr = br.readField(8);
    if (!hdr) return Expected<L3RPError>::error(hdr.error());
    msg.mRpMti = static_cast<uint8_t>(hdr.value() & 0x07);

    auto ref = br.readField(8);
    if (!ref) return Expected<L3RPError>::error(ref.error());
    msg.mMessageRef = static_cast<uint8_t>(ref.value());

    auto causeLen = br.readField(8);
    if (!causeLen) return Expected<L3RPError>::error(causeLen.error());
    if (causeLen.value() >= 1) {
        auto causeVal = br.readField(8);
        if (!causeVal) return Expected<L3RPError>::error(causeVal.error());
        msg.mCause = static_cast<CPCause>(causeVal.value() & 0x7F);
    }

    return Expected<L3RPError>::hold(std::move(msg));
}

void L3RPError::write(BitWriter& bw) const {
    bw.writeField(mRpMti & 0x07, 8);
    bw.writeField(mMessageRef, 8);
    bw.writeField(1, 8);
    bw.writeField(static_cast<uint8_t>(mCause), 8);
}

void L3RPError::text(std::ostream& os) const {
    os << "RP-ERROR(mti=" << static_cast<int>(mRpMti)
       << ",ref=" << static_cast<int>(mMessageRef)
       << ",cause=" << CPCause2Str(mCause) << ")";
}

// ── L3RPSMMA (24.011 7.3.3) ───────────────────────────────────────────

size_t L3RPSMMA::bodyLength() const {
    return 2;
}

Expected<L3RPSMMA> L3RPSMMA::parse(BitReader& br) {
    L3RPSMMA msg;

    auto hdr = br.readField(8);
    if (!hdr) return Expected<L3RPSMMA>::error(hdr.error());
    msg.mRpMti = static_cast<uint8_t>(hdr.value() & 0x07);

    auto ref = br.readField(8);
    if (!ref) return Expected<L3RPSMMA>::error(ref.error());
    msg.mMessageRef = static_cast<uint8_t>(ref.value());

    return Expected<L3RPSMMA>::hold(std::move(msg));
}

void L3RPSMMA::write(BitWriter& bw) const {
    bw.writeField(mRpMti & 0x07, 8);
    bw.writeField(mMessageRef, 8);
}

void L3RPSMMA::text(std::ostream& os) const {
    os << "RP-SMMA(mti=" << static_cast<int>(mRpMti)
       << ",ref=" << static_cast<int>(mMessageRef) << ")";
}

} // namespace gsml3parser
