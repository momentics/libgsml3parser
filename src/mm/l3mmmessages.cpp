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

#include "gsml3parser/mm/l3mmmessages.h"
#include <sstream>
#include <iomanip>

namespace gsml3parser {

namespace {

size_t lvLen(size_t vLen) { return 1 + vLen; }
size_t tlvLen(size_t vLen) { return 2 + vLen; }
size_t tvLen(size_t vLen) { return 1 + vLen; }

Expected<L3MobileIdentity> parseLVMI(BitReader& br) {
    auto r = br.readField(8);
    if (!r) return Expected<L3MobileIdentity>::error(r.error());
    return L3MobileIdentity::parse(br, r.value());
}

void writeLVMI(const L3MobileIdentity& mi, BitWriter& bw) {
    bw.writeField(static_cast<uint32_t>(mi.lengthV()), 8);
    mi.write(bw);
}

Expected<L3MobileIdentity> parseTLVMI(BitReader& br, unsigned expectedIEI) {
    if (!br.hasMore()) return Expected<L3MobileIdentity>::error(
        ParseError{ParseError::Code::TruncatedInput, "no TLV for MI", br.position()});
    auto r = br.readField(8);
    if (!r) return Expected<L3MobileIdentity>::error(r.error());
    if ((r.value() & 0x7F) != expectedIEI) {
        return Expected<L3MobileIdentity>::error(
            ParseError{ParseError::Code::InvalidIE, "unexpected TLV type", br.position() - 8});
    }
    bool ext = (r.value() & 0x80) != 0;
    size_t len = 0;
    if (ext) {
        auto l = br.readField(8);
        if (!l) return Expected<L3MobileIdentity>::error(l.error());
        len = l.value();
    }
    return L3MobileIdentity::parse(br, len);
}

void writeTLVMI(const L3MobileIdentity& mi, uint8_t type, BitWriter& bw) {
    bw.writeField(0x80u | type, 8);
    bw.writeField(static_cast<uint32_t>(mi.lengthV()), 8);
    mi.write(bw);
}

Expected<L3LocationAreaIdentity> parseTLVLAI(BitReader& br, unsigned expectedIEI) {
    if (!br.hasMore()) return Expected<L3LocationAreaIdentity>::error(
        ParseError{ParseError::Code::TruncatedInput, "no TLV for LAI", br.position()});
    auto r = br.readField(8);
    if (!r) return Expected<L3LocationAreaIdentity>::error(r.error());
    if ((r.value() & 0x7F) != expectedIEI) {
        return Expected<L3LocationAreaIdentity>::error(
            ParseError{ParseError::Code::InvalidIE, "unexpected TLV type", br.position() - 8});
    }
    bool ext = (r.value() & 0x80) != 0;
    if (ext) {
        auto l = br.readField(8);
        if (!l) return Expected<L3LocationAreaIdentity>::error(l.error());
    }
    return L3LocationAreaIdentity::parse(br);
}

void writeTLVLAI(const L3LocationAreaIdentity& lai, uint8_t type, BitWriter& bw) {
    bw.writeField(0x80u | type, 8);
    bw.writeField(static_cast<uint32_t>(lai.lengthV()), 8);
    lai.write(bw);
}

Expected<L3NetworkName> parseTLVNN(BitReader& br, unsigned expectedIEI) {
    if (!br.hasMore()) return Expected<L3NetworkName>::error(
        ParseError{ParseError::Code::TruncatedInput, "no TLV for NN", br.position()});
    auto r = br.readField(8);
    if (!r) return Expected<L3NetworkName>::error(r.error());
    if ((r.value() & 0x7F) != expectedIEI) {
        return Expected<L3NetworkName>::error(
            ParseError{ParseError::Code::InvalidIE, "unexpected TLV type", br.position() - 8});
    }
    bool ext = (r.value() & 0x80) != 0;
    size_t len = 0;
    if (ext) {
        auto l = br.readField(8);
        if (!l) return Expected<L3NetworkName>::error(l.error());
        len = l.value();
    }
    return L3NetworkName::parse(br, len);
}

void writeTLVNN(const L3NetworkName& nn, uint8_t type, BitWriter& bw) {
    bw.writeField(0x80u | type, 8);
    bw.writeField(static_cast<uint32_t>(nn.lengthV()), 8);
    nn.write(bw);
}

Expected<L3TimeZoneAndTime> parseTVTZ(BitReader& br, unsigned expectedType) {
    auto r = br.readField(8);
    if (!r) return Expected<L3TimeZoneAndTime>::error(r.error());
    if (r.value() != expectedType) {
        return Expected<L3TimeZoneAndTime>::error(
            ParseError{ParseError::Code::InvalidIE, "unexpected TV type", br.position() - 8});
    }
    return L3TimeZoneAndTime::parse(br);
}

void writeTVTZ(const L3TimeZoneAndTime& tz, uint8_t type, BitWriter& bw) {
    bw.writeField(type, 8);
    tz.write(bw);
}

bool peekTLVType(BitReader& br, unsigned expectedIEI) {
    if (!br.hasMore()) return false;
    unsigned t = br.peekField(8);
    return (t & 0x7F) == expectedIEI;
}

} // anonymous namespace

// ── L3IMSIDetachIndication (MTI=0x01) ──────────────────────────────────

size_t L3IMSIDetachIndication::bodyLength() const {
    return lvLen(mClassmark.lengthV()) + lvLen(mMobileIdentity.lengthV());
}

Expected<L3IMSIDetachIndication> L3IMSIDetachIndication::parse(BitReader& br) {
    L3IMSIDetachIndication msg;
    // CM1 is LV-encoded: length byte + value
    {
        auto lenR = br.readField(8);
        if (!lenR) return Expected<L3IMSIDetachIndication>::error(lenR.error());
    }
    {
        auto cmRes = L3MobileStationClassmark1::parse(br);
        if (!cmRes) return Expected<L3IMSIDetachIndication>::error(cmRes.error());
        msg.mClassmark = cmRes.value();
    }
    {
        auto miRes = parseLVMI(br);
        if (!miRes) return Expected<L3IMSIDetachIndication>::error(miRes.error());
        msg.mMobileIdentity = miRes.value();
    }
    return Expected<L3IMSIDetachIndication>::hold(msg);
}

void L3IMSIDetachIndication::write(BitWriter& bw) const {
    bw.writeField(static_cast<uint32_t>(mClassmark.lengthV()), 8);
    mClassmark.write(bw);
    writeLVMI(mMobileIdentity, bw);
}

void L3IMSIDetachIndication::text(std::ostream& os) const {
    os << "IMSIDetachIndication: ";
    mMobileIdentity.text(os);
}

// ── L3CMServiceAccept (MTI=0x21, empty body) ───────────────────────────

Expected<L3CMServiceAccept> L3CMServiceAccept::parse(BitReader&) {
    return Expected<L3CMServiceAccept>::hold(L3CMServiceAccept{});
}

void L3CMServiceAccept::write(BitWriter&) const {}

void L3CMServiceAccept::text(std::ostream& os) const {
    os << "CMServiceAccept";
}

// ── L3CMServiceReject (MTI=0x22) ───────────────────────────────────────

Expected<L3CMServiceReject> L3CMServiceReject::parse(BitReader& br) {
    auto r = br.readField(8);
    if (!r) return Expected<L3CMServiceReject>::error(r.error());
    return Expected<L3CMServiceReject>::hold(
        L3CMServiceReject(static_cast<MMRejectCause>(r.value())));
}

void L3CMServiceReject::write(BitWriter& bw) const {
    bw.writeField(static_cast<uint32_t>(mCause), 8);
}

void L3CMServiceReject::text(std::ostream& os) const {
    os << "CMServiceReject: " << MMRejectCause2Str(mCause);
}

// ── L3CMServiceAbort (MTI=0x23, empty body) ────────────────────────────

Expected<L3CMServiceAbort> L3CMServiceAbort::parse(BitReader&) {
    return Expected<L3CMServiceAbort>::hold(L3CMServiceAbort{});
}

void L3CMServiceAbort::write(BitWriter&) const {}

void L3CMServiceAbort::text(std::ostream& os) const {
    os << "CMServiceAbort";
}

// ── L3CMServiceRequest (MTI=0x24) ──────────────────────────────────────

size_t L3CMServiceRequest::bodyLength() const {
    return 1 + lvLen(mClassmark.lengthV()) + lvLen(mMobileIdentity.lengthV());
}

Expected<L3CMServiceRequest> L3CMServiceRequest::parse(BitReader& br) {
    L3CMServiceRequest msg;
    // Spare ciphering key sequence number (4 bits)
    {
        auto r = br.readField(4);
        if (!r) return Expected<L3CMServiceRequest>::error(r.error());
    }
    // Service type (4 bits)
    {
        auto stRes = L3CMServiceType::parse(br);
        if (!stRes) return Expected<L3CMServiceRequest>::error(stRes.error());
        msg.mServiceType = stRes.value();
    }
    // Classmark2 (LV: length octet + 3 bytes value)
    {
        auto lenR = br.readField(8);
        if (!lenR) return Expected<L3CMServiceRequest>::error(lenR.error());
    }
    {
        auto cmRes = L3MobileStationClassmark2::parse(br);
        if (!cmRes) return Expected<L3CMServiceRequest>::error(cmRes.error());
        msg.mClassmark = cmRes.value();
    }
    // MobileIdentity (LV)
    {
        auto miRes = parseLVMI(br);
        if (!miRes) return Expected<L3CMServiceRequest>::error(miRes.error());
        msg.mMobileIdentity = miRes.value();
    }
    return Expected<L3CMServiceRequest>::hold(msg);
}

void L3CMServiceRequest::write(BitWriter& bw) const {
    bw.writeField(0, 4);
    mServiceType.write(bw);
    bw.writeField(static_cast<uint32_t>(mClassmark.lengthV()), 8);
    mClassmark.write(bw);
    writeLVMI(mMobileIdentity, bw);
}

void L3CMServiceRequest::text(std::ostream& os) const {
    os << "CMServiceRequest: type=";
    mServiceType.text(os);
    os << " ";
    mMobileIdentity.text(os);
}

// ── L3CMReestablishmentRequest (MTI=0x28) ──────────────────────────────

size_t L3CMReestablishmentRequest::bodyLength() const {
    size_t len = 1 + lvLen(mClassmark.lengthV()) + lvLen(mMobileID.lengthV());
    if (mHaveLAI) len += tlvLen(mLAI.lengthV());
    return len;
}

Expected<L3CMReestablishmentRequest> L3CMReestablishmentRequest::parse(BitReader& br) {
    L3CMReestablishmentRequest msg;
    // CKSN(4)|spare(4)
    {
        auto ck = br.readField(4);
        if (!ck) return Expected<L3CMReestablishmentRequest>::error(ck.error());
        msg.mCKSN = ck.value();
    }
    {
        auto sp = br.readField(4);
        if (!sp) return Expected<L3CMReestablishmentRequest>::error(sp.error());
    }
    // Classmark2 (LV: length octet + 3 bytes value)
    {
        auto lenR = br.readField(8);
        if (!lenR) return Expected<L3CMReestablishmentRequest>::error(lenR.error());
    }
    {
        auto cmRes = L3MobileStationClassmark2::parse(br);
        if (!cmRes) return Expected<L3CMReestablishmentRequest>::error(cmRes.error());
        msg.mClassmark = cmRes.value();
    }
    // MobileIdentity (LV)
    {
        auto miRes = parseLVMI(br);
        if (!miRes) return Expected<L3CMReestablishmentRequest>::error(miRes.error());
        msg.mMobileID = miRes.value();
    }
    // Optional LAI (TLV, IEI=0x13)
    if (peekTLVType(br, 0x13)) {
        auto laiRes = parseTLVLAI(br, 0x13);
        if (!laiRes) return Expected<L3CMReestablishmentRequest>::error(laiRes.error());
        msg.mLAI = laiRes.value();
        msg.mHaveLAI = true;
    }
    return Expected<L3CMReestablishmentRequest>::hold(msg);
}

void L3CMReestablishmentRequest::write(BitWriter& bw) const {
    bw.writeField(mCKSN & 0x0F, 4);
    bw.writeField(0, 4);
    bw.writeField(static_cast<uint32_t>(mClassmark.lengthV()), 8);
    mClassmark.write(bw);
    writeLVMI(mMobileID, bw);
    if (mHaveLAI) {
        writeTLVLAI(mLAI, 0x13, bw);
    }
}

void L3CMReestablishmentRequest::text(std::ostream& os) const {
    os << "CMReestablishmentRequest: ";
    mMobileID.text(os);
}

// ── L3IdentityResponse (MTI=0x19) ──────────────────────────────────────

size_t L3IdentityResponse::bodyLength() const {
    return lvLen(mMobileID.lengthV());
}

Expected<L3IdentityResponse> L3IdentityResponse::parse(BitReader& br) {
    L3IdentityResponse msg;
    auto miRes = parseLVMI(br);
    if (!miRes) return Expected<L3IdentityResponse>::error(miRes.error());
    msg.mMobileID = miRes.value();
    return Expected<L3IdentityResponse>::hold(msg);
}

void L3IdentityResponse::write(BitWriter& bw) const {
    writeLVMI(mMobileID, bw);
}

void L3IdentityResponse::text(std::ostream& os) const {
    os << "IdentityResponse: ";
    mMobileID.text(os);
}

// ── L3IdentityRequest (MTI=0x18) ───────────────────────────────────────

Expected<L3IdentityRequest> L3IdentityRequest::parse(BitReader& br) {
    auto sp = br.readField(4);
    if (!sp) return Expected<L3IdentityRequest>::error(sp.error());
    auto ty = br.readField(4);
    if (!ty) return Expected<L3IdentityRequest>::error(ty.error());
    return Expected<L3IdentityRequest>::hold(
        L3IdentityRequest(static_cast<MobileIDType>(ty.value())));
}

void L3IdentityRequest::write(BitWriter& bw) const {
    bw.writeField(0, 4);
    bw.writeField(static_cast<uint32_t>(mType), 4);
}

void L3IdentityRequest::text(std::ostream& os) const {
    os << "IdentityRequest: type=" << static_cast<int>(mType);
}

// ── L3MMInformation (MTI=0x32) ─────────────────────────────────────────

size_t L3MMInformation::bodyLength() const {
    size_t len = 0;
    if (mShortName.lengthV() > 1) len += tlvLen(mShortName.lengthV());
    len += tvLen(L3TimeZoneAndTime::lengthV());
    return len;
}

Expected<L3MMInformation> L3MMInformation::parse(BitReader& br) {
    L3MMInformation msg;
    // Optional shortName (TLV, IEI=0x45)
    if (peekTLVType(br, 0x45)) {
        auto nnRes = parseTLVNN(br, 0x45);
        if (!nnRes) return Expected<L3MMInformation>::error(nnRes.error());
        msg.mShortName = nnRes.value();
    }
    // Mandatory time (TV, type=0x47)
    auto tzRes = parseTVTZ(br, 0x47);
    if (!tzRes) return Expected<L3MMInformation>::error(tzRes.error());
    msg.mTime = tzRes.value();
    return Expected<L3MMInformation>::hold(msg);
}

void L3MMInformation::write(BitWriter& bw) const {
    if (mShortName.lengthV() > 1) writeTLVNN(mShortName, 0x45, bw);
    writeTVTZ(mTime, 0x47, bw);
}

void L3MMInformation::text(std::ostream& os) const {
    os << "MMInformation:";
    if (mShortName.lengthV() > 1) {
        os << " shortName=(";
        mShortName.text(os);
        os << ")";
    }
    os << " time=(";
    mTime.text(os);
    os << ")";
}

// ── L3LocationUpdatingAccept (MTI=0x02) ────────────────────────────────

L3LocationUpdatingAccept::Builder L3LocationUpdatingAccept::builder() {
    return Builder{};
}

L3LocationUpdatingAccept L3LocationUpdatingAccept::Builder::build() const {
    L3LocationUpdatingAccept msg;
    msg.mLAI = m_lai;
    msg.mFollowOnProceed = m_followOn;
    msg.mHaveMobileIdentity = m_haveMI;
    msg.mMobileIdentity = m_mi;
    return msg;
}

size_t L3LocationUpdatingAccept::bodyLength() const {
    size_t result = mLAI.lengthV();
    if (mHaveMobileIdentity) result += tlvLen(mMobileIdentity.lengthV());
    if (mFollowOnProceed) result += 1;
    return result;
}

Expected<L3LocationUpdatingAccept> L3LocationUpdatingAccept::parse(BitReader& br) {
    L3LocationUpdatingAccept msg;
    // Mandatory LAI (raw V, no length prefix)
    {
        auto laiRes = L3LocationAreaIdentity::parse(br);
        if (!laiRes) return Expected<L3LocationUpdatingAccept>::error(laiRes.error());
        msg.mLAI = laiRes.value();
    }
    // Optional MobileIdentity (TLV, IEI=0x17)
    if (peekTLVType(br, 0x17)) {
        auto miRes = parseTLVMI(br, 0x17);
        if (!miRes) return Expected<L3LocationUpdatingAccept>::error(miRes.error());
        msg.mMobileIdentity = miRes.value();
        msg.mHaveMobileIdentity = true;
    }
    // Optional FollowOnProceed flag (0xa1)
    msg.mFollowOnProceed = (br.peekField(8) == 0xa1);
    if (msg.mFollowOnProceed) {
        auto fo = br.readField(8);
        (void)fo;
    }
    return Expected<L3LocationUpdatingAccept>::hold(msg);
}

void L3LocationUpdatingAccept::write(BitWriter& bw) const {
    mLAI.write(bw);
    if (mHaveMobileIdentity) writeTLVMI(mMobileIdentity, 0x17, bw);
    if (mFollowOnProceed) bw.writeField(0xa1, 8);
}

void L3LocationUpdatingAccept::text(std::ostream& os) const {
    os << "LocationUpdatingAccept: ";
    mLAI.text(os);
    if (mHaveMobileIdentity) {
        os << " ";
        mMobileIdentity.text(os);
    }
}

// ── L3LocationUpdatingReject (MTI=0x04) ────────────────────────────────

Expected<L3LocationUpdatingReject> L3LocationUpdatingReject::parse(BitReader& br) {
    auto r = br.readField(8);
    if (!r) return Expected<L3LocationUpdatingReject>::error(r.error());
    return Expected<L3LocationUpdatingReject>::hold(
        L3LocationUpdatingReject(static_cast<MMRejectCause>(r.value())));
}

void L3LocationUpdatingReject::write(BitWriter& bw) const {
    bw.writeField(static_cast<uint32_t>(mCause), 8);
}

void L3LocationUpdatingReject::text(std::ostream& os) const {
    os << "LocationUpdatingReject: " << MMRejectCause2Str(mCause);
}

// ── L3LocationUpdatingRequest (MTI=0x08) ───────────────────────────────

size_t L3LocationUpdatingRequest::bodyLength() const {
    return 1 + mLAI.lengthV() + lvLen(mClassmark.lengthV()) + lvLen(mMobileIdentity.lengthV());
}

Expected<L3LocationUpdatingRequest> L3LocationUpdatingRequest::parse(BitReader& br) {
    L3LocationUpdatingRequest msg;
    // LU_Type(2)|spare(2)|CKSN(4)
    {
        auto ut = br.readField(2);
        if (!ut) return Expected<L3LocationUpdatingRequest>::error(ut.error());
        msg.mUpdateType = ut.value();
    }
    {
        auto sp = br.readField(2);
        if (!sp) return Expected<L3LocationUpdatingRequest>::error(sp.error());
    }
    {
        auto ck = br.readField(4);
        if (!ck) return Expected<L3LocationUpdatingRequest>::error(ck.error());
        msg.mCKSN = ck.value();
    }
    // LAI (raw V, 5 bytes mandatory, NOT LV-prefixed)
    {
        auto laiRes = L3LocationAreaIdentity::parse(br);
        if (!laiRes) return Expected<L3LocationUpdatingRequest>::error(laiRes.error());
        msg.mLAI = laiRes.value();
    }
    // Classmark1 (LV: length octet + 1 byte value)
    {
        auto lenR = br.readField(8);
        if (!lenR) return Expected<L3LocationUpdatingRequest>::error(lenR.error());
    }
    {
        auto cmRes = L3MobileStationClassmark1::parse(br);
        if (!cmRes) return Expected<L3LocationUpdatingRequest>::error(cmRes.error());
        msg.mClassmark = cmRes.value();
    }
    // MobileIdentity (LV)
    {
        auto miRes = parseLVMI(br);
        if (!miRes) return Expected<L3LocationUpdatingRequest>::error(miRes.error());
        msg.mMobileIdentity = miRes.value();
    }
    return Expected<L3LocationUpdatingRequest>::hold(msg);
}

void L3LocationUpdatingRequest::write(BitWriter& bw) const {
    bw.writeField(mUpdateType & 0x03, 2);
    bw.writeField(0, 2);
    bw.writeField(mCKSN & 0x0F, 4);
    mLAI.write(bw);
    bw.writeField(static_cast<uint32_t>(mClassmark.lengthV()), 8);
    mClassmark.write(bw);
    writeLVMI(mMobileIdentity, bw);
}

void L3LocationUpdatingRequest::text(std::ostream& os) const {
    os << "LocationUpdatingRequest: type=" << (mUpdateType & 0x3)
       << " CKSN=" << mCKSN;
    mMobileIdentity.text(os);
    os << " ";
    mLAI.text(os);
    os << " ";
    mClassmark.text(os);
}

// ── L3TMSIReallocationCommand (MTI=0x1a) ───────────────────────────────

L3TMSIReallocationCommand::Builder L3TMSIReallocationCommand::builder() {
    return Builder{};
}

L3TMSIReallocationCommand L3TMSIReallocationCommand::Builder::build() const {
    L3TMSIReallocationCommand msg;
    msg.mLAI = m_lai;
    msg.mTMSI = m_tmsi;
    msg.mFollowOnProceed = m_followOn;
    return msg;
}

size_t L3TMSIReallocationCommand::bodyLength() const {
    return mLAI.lengthV() + lvLen(mTMSI.lengthV()) + 1;
}

Expected<L3TMSIReallocationCommand> L3TMSIReallocationCommand::parse(BitReader& br) {
    L3TMSIReallocationCommand msg;
    {
        auto laiRes = L3LocationAreaIdentity::parse(br);
        if (!laiRes) return Expected<L3TMSIReallocationCommand>::error(laiRes.error());
        msg.mLAI = laiRes.value();
    }
    {
        auto miRes = parseLVMI(br);
        if (!miRes) return Expected<L3TMSIReallocationCommand>::error(miRes.error());
        msg.mTMSI = miRes.value();
    }
    {
        auto fo = br.readField(1);
        if (!fo) return Expected<L3TMSIReallocationCommand>::error(fo.error());
        msg.mFollowOnProceed = (fo.value() != 0);
    }
    {
        auto sp = br.readField(7);
        if (!sp) return Expected<L3TMSIReallocationCommand>::error(sp.error());
    }
    return Expected<L3TMSIReallocationCommand>::hold(msg);
}

void L3TMSIReallocationCommand::write(BitWriter& bw) const {
    mLAI.write(bw);
    writeLVMI(mTMSI, bw);
    bw.writeField(mFollowOnProceed ? 1u : 0u, 1);
    bw.writeField(0, 7);
}

void L3TMSIReallocationCommand::text(std::ostream& os) const {
    os << "TMSIReallocationCommand: ";
    mLAI.text(os);
    os << " ";
    mTMSI.text(os);
    os << " followOn=" << (mFollowOnProceed ? "1" : "0");
}

// ── L3TMSIReallocationComplete (MTI=0x1b, empty body) ──────────────────

Expected<L3TMSIReallocationComplete> L3TMSIReallocationComplete::parse(BitReader&) {
    return Expected<L3TMSIReallocationComplete>::hold(L3TMSIReallocationComplete{});
}

void L3TMSIReallocationComplete::write(BitWriter&) const {}

void L3TMSIReallocationComplete::text(std::ostream& os) const {
    os << "TMSIReallocationComplete";
}

// ── L3MMStatus (MTI=0x31) ──────────────────────────────────────────────

Expected<L3MMStatus> L3MMStatus::parse(BitReader& br) {
    L3MMStatus msg;
    {
        auto ca = br.readField(8);
        if (!ca) return Expected<L3MMStatus>::error(ca.error());
        msg.mCause = static_cast<MMRejectCause>(ca.value());
    }
    return Expected<L3MMStatus>::hold(msg);
}

void L3MMStatus::write(BitWriter& bw) const {
    bw.writeField(static_cast<uint32_t>(mCause), 8);
}

void L3MMStatus::text(std::ostream& os) const {
    os << "MMStatus: " << MMRejectCause2Str(mCause);
}

// ── L3AuthenticationRequest (MTI=0x12) ─────────────────────────────────

Expected<L3AuthenticationRequest> L3AuthenticationRequest::parse(BitReader& br) {
    L3AuthenticationRequest msg;
    {
        auto sp = br.readField(4);
        if (!sp) return Expected<L3AuthenticationRequest>::error(sp.error());
    }
    {
        auto ck = br.readField(4);
        if (!ck) return Expected<L3AuthenticationRequest>::error(ck.error());
        msg.mCKSN = ck.value();
    }
    msg.mRAND.resize(16);
    for (size_t i = 0; i < 16; ++i) {
        auto rb = br.readField(8);
        if (!rb) return Expected<L3AuthenticationRequest>::error(rb.error());
        msg.mRAND[i] = static_cast<uint8_t>(rb.value());
    }
    return Expected<L3AuthenticationRequest>::hold(msg);
}

void L3AuthenticationRequest::write(BitWriter& bw) const {
    bw.writeField(0, 4);
    bw.writeField(mCKSN, 4);
    for (const auto& b : mRAND) {
        bw.writeField(b, 8);
    }
}

void L3AuthenticationRequest::text(std::ostream& os) const {
    os << "AuthenticationRequest: CKSN=" << mCKSN;
    os << " RAND=";
    for (const auto& b : mRAND) {
        os << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
    }
}

// ── L3AuthenticationResponse (MTI=0x14) ────────────────────────────────

Expected<L3AuthenticationResponse> L3AuthenticationResponse::parse(BitReader& br) {
    auto r = br.readField(32);
    if (!r) return Expected<L3AuthenticationResponse>::error(r.error());
    return Expected<L3AuthenticationResponse>::hold(L3AuthenticationResponse{r.value()});
}

void L3AuthenticationResponse::write(BitWriter& bw) const {
    bw.writeField(mSRES, 32);
}

void L3AuthenticationResponse::text(std::ostream& os) const {
    os << "AuthenticationResponse: SRES=0x" << std::hex << mSRES;
}

// ── L3AuthenticationReject (MTI=0x11, empty body) ──────────────────────

Expected<L3AuthenticationReject> L3AuthenticationReject::parse(BitReader&) {
    return Expected<L3AuthenticationReject>::hold(L3AuthenticationReject{});
}

void L3AuthenticationReject::write(BitWriter&) const {}

void L3AuthenticationReject::text(std::ostream& os) const {
    os << "AuthenticationReject";
}

} // namespace gsml3parser
