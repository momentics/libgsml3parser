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

// SMS CP & RP Messages - parse/write implementation
// Spec: 3GPP TS 24.008 section 9.6, Table 10.6a; 3GPP TS 24.011 sections 7-8
// Wire encodings per 3GPP TS 24.011 (CP/RP message set).

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
// Body: RP-Originator-SM-SC-Address(LV) | RP-Destination-SM-SC-Address(LV)
//       | RP-User-Data(LV). Both SMSC addresses are always present; an
//       empty address is a single zero length octet (TS 24.011 section 7.3.1).

namespace {

size_t rpAddressWireLength(const std::optional<L3TPAddress>& addr) {
    return addr ? addr->totalLength() : 1;  // empty address = one zero octet
}

} // namespace

size_t L3RPData::bodyLength() const {
    size_t len = 2; // header: spare+mti + message-ref
    len += rpAddressWireLength(mOriginatorAddress);   // always present LV
    len += rpAddressWireLength(mDestinationAddress);  // always present LV
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

    // Both SMSC addresses are always present as LV elements; an empty
    // address is a single zero length octet (TS 24.011 section 7.3.1).
    auto oaResult = L3TPAddress::parse(br);
    if (!oaResult) return Expected<L3RPData>::error(oaResult.error());
    if (oaResult.value().lengthV() > 0) {
        msg.mOriginatorAddress = std::move(oaResult.value());
    }

    // RP-Destination-SM-SC-Address: always present as an LV element; an
    // empty address is a single zero length octet (TS 24.011 section 7.3.1).
    auto daResult = L3TPAddress::parse(br);
    if (!daResult) return Expected<L3RPData>::error(daResult.error());
    if (daResult.value().lengthV() > 0) {
        msg.mDestinationAddress = std::move(daResult.value());
    }

    // RP-User-Data: length byte + data.
    auto udLen = br.readField(8);
    if (!udLen) return Expected<L3RPData>::error(udLen.error());
    for (size_t i = 0; i < udLen.value() && br.hasMore(); ++i) {
        auto octet = br.readField(8);
        if (!octet) return Expected<L3RPData>::error(octet.error());
        msg.mUserData.push_back(static_cast<uint8_t>(octet.value()));
    }

    return Expected<L3RPData>::hold(std::move(msg));
}

void L3RPData::write(BitWriter& bw) const {
    bw.writeField(mRpMti & 0x07, 8);
    bw.writeField(mMessageRef, 8);

    // Both SMSC addresses are always written as LV elements; an empty
    // address is a single zero length octet (TS 24.011 section 7.3.1).
    if (mOriginatorAddress) {
        mOriginatorAddress->write(bw);
    } else {
        bw.writeField(0, 8);
    }
    if (mDestinationAddress) {
        mDestinationAddress->write(bw);
    } else {
        bw.writeField(0, 8);
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

// ── smsMessageName ──────────────────────────────────────────────────────

const char* smsMessageName(int mti) {
    switch (mti) {
        case L3CPData::MTI:   return "CPData";
        case L3CPAck::MTI:    return "CPAck";
        case L3CPErr::MTI:    return "CPErr";
        case L3CPStatus::MTI: return "CPStatus";
        case L3CPSMT::MTI:    return "CPSMT";
        default:              return "Unknown_SMS";
    }
}

} // namespace gsml3parser
