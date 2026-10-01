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

// GMM Messages - parse/write/text implementation
// Spec: 3GPP TS 24.008 sections 9.4, Table 10.4
// Wire encodings per 3GPP TS 24.008 section 10 (GMM message set).

#include "gsml3parser/gmm/l3gmmmessages.h"
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

} // anonymous namespace

// ── L3AttachRequest (TS 44.068 section 9.5) ───────────────────────────

size_t L3AttachRequest::bodyLength() const {
    size_t len = 0;
    // msNetworkCapability: LV format (length + value)
    len += lvLen(mMsNetworkCapability.lengthV());
    // attachType(3)|forL3(1) | gprsCKSN(3)|spare(1) = 1 octet
    len += 1;
    // drxParam: V format (two value octets, no identifier)
    len += L3DRXParameter::lengthV();
    // mobileIdentity: LV
    len += lvLen(mMobileIdentity.lengthV());
    // oldRoutingAreaID: fixed six value octets
    len += L3RoutingAreaIdentification::lengthV();
    // msRACap: LV (mandatory on the wire)
    len += lvLen(mMsRACap.size());
    // additional optional IEs, kept opaque
    len += mAdditionalIes.size();
    return len;
}

Expected<L3AttachRequest> L3AttachRequest::parse(BitReader& br) {
    L3AttachRequest msg;

    // GMM ATTACH REQUEST (TS 44.068): MS network capability (LV), attach
    // type + GMM CKSN in one octet, DRX parameter (two value octets, no
    // IEI), mobile identity (LV), old routing area identity (six value
    // octets), MS radio access capabilities (LV, mandatory); any further
    // optional IEs are kept opaque.
    {
        auto len = br.readField(8);
        if (!len) return Expected<L3AttachRequest>::error(len.error());
        auto cap = L3MSNetworkCapability::parse(br, len.value());
        if (!cap) return Expected<L3AttachRequest>::error(cap.error());
        msg.mMsNetworkCapability = std::move(cap).value();
    }

    // attachType(3)|forL3(1) in the high half-octet, gprsCKSN(3)|spare(1)
    // in the low one.
    {
        auto o = br.readField(8);
        if (!o) return Expected<L3AttachRequest>::error(o.error());
        msg.mAttachType = static_cast<GMMAttachType>((o.value() >> 5) & 0x07u);
        msg.mForL3 = ((o.value() >> 4) & 0x01u) != 0;
        msg.mCKSN = static_cast<uint8_t>((o.value() >> 1) & 0x07u);
    }

    // drxParam: exactly two value octets, no identifier.
    {
        auto drx = L3DRXParameter::parse(br);
        if (!drx) return Expected<L3AttachRequest>::error(drx.error());
        msg.mDRXParam = std::move(drx).value();
    }

    // mobileIdentity (LV format)
    {
        auto mi = parseLVMI(br);
        if (!mi) return Expected<L3AttachRequest>::error(mi.error());
        msg.mMobileIdentity = std::move(mi).value();
    }

    // oldRoutingAreaID (fixed six value octets)
    {
        auto rai = L3RoutingAreaIdentification::parse(br);
        if (!rai) return Expected<L3AttachRequest>::error(rai.error());
        msg.mOldRAI = std::move(rai).value();
    }

    // msRACap (LV, mandatory on the wire; no identifier)
    {
        auto len = br.readField(8);
        if (!len) return Expected<L3AttachRequest>::error(len.error());
        size_t vLen = len.value();
        if (vLen * 8 > br.remainingBits()) {
            return Expected<L3AttachRequest>::error(
                ParseError{ParseError::Code::TruncatedInput, "truncated msRACap", br.position() - 8});
        }
        msg.mMsRACap.resize(vLen);
        if (vLen > 0) {
            auto r = br.readBytes(msg.mMsRACap.data(), vLen);
            if (!r) return Expected<L3AttachRequest>::error(r.error());
        }
    }

    // Unrecognized optional IEs are kept opaque and re-emitted verbatim.
    if (!detail::readOpaqueTail(br, msg.mAdditionalIes)) {
        return Expected<L3AttachRequest>::error(
            ParseError{ParseError::Code::TruncatedInput, "truncated optional IEs"});
    }

    return Expected<L3AttachRequest>::hold(std::move(msg));
}

void L3AttachRequest::write(BitWriter& bw) const {
    // msNetworkCapability: LV
    bw.writeField(static_cast<uint32_t>(mMsNetworkCapability.lengthV()), 8);
    mMsNetworkCapability.write(bw);

    // attachType(3)|forL3(1) | gprsCKSN(3)|spare(1)
    bw.writeField(((static_cast<uint8_t>(mAttachType) & 0x07u) << 5) |
                  ((mForL3 ? 1u : 0u) << 4) | ((mCKSN & 0x07u) << 1), 8);

    // drxParam: two value octets, no identifier
    mDRXParam.write(bw);

    // mobileIdentity: LV
    writeLVMI(mMobileIdentity, bw);

    // oldRoutingAreaID: fixed six value octets
    mOldRAI.write(bw);

    // msRACap: LV (mandatory on the wire)
    bw.writeField(static_cast<uint32_t>(mMsRACap.size()), 8);
    if (!mMsRACap.empty()) bw.writeBytes(mMsRACap.data(), mMsRACap.size());

    detail::writeOpaqueTail(mAdditionalIes, bw);
}

void L3AttachRequest::text(std::ostream& os) const {
    os << "AttachRequest(type=";
    switch (mAttachType) {
        case GMMAttachType::GPRSAttach: os << "GPRS"; break;
        case GMMAttachType::CombinedGPRSAndIMSIAttach: os << "Combined"; break;
    }
    os << ",CKSN=" << static_cast<int>(mCKSN) << ",forL3=" << mForL3 << ")";
    mMobileIdentity.text(os);
}

L3AttachRequest L3AttachRequest::Builder::build() const {
    L3AttachRequest msg;
    msg.mMsNetworkCapability = m_msNetworkCapability;
    msg.mAttachType = m_attachType;
    msg.mForL3 = m_forL3;
    msg.mCKSN = static_cast<uint8_t>(mCKSN & 0x07u);
    msg.mDRXParam = m_drxParam;
    msg.mMobileIdentity = m_mobileIdentity;
    msg.mOldRAI = m_oldRAI;
    msg.mMsRACap = m_msRACap;
    msg.mAdditionalIes = m_additionalIes;
    return msg;
}

L3AttachRequest::Builder L3AttachRequest::builder() {
    return Builder{};
}

// ── L3AttachAccept (GSM 24.008 9.4.2) ────────────────────────────────

size_t L3AttachAccept::bodyLength() const {
    size_t len = 1; // attachResult|forceToStandby|updateTimer|radioPriority
    len += 6;       // routingAreaIdentification (raw)
    if (mHavePTMSI) len += tlvLen(mPTMSI.lengthV());
    len += mAdditionalIes.size();
    return len;
}

Expected<L3AttachAccept> L3AttachAccept::parse(BitReader& br) {
    L3AttachAccept msg;

    // attachResult(3)|spare(1)|forceToStandby(1)|updateTimer(2)|radioPriority(1)
    // = 1 octet (TS 44.068 section 9.5).
    {
        auto o = br.readField(8);
        if (!o) return Expected<L3AttachAccept>::error(o.error());
        msg.mAttachResult = static_cast<GMMAttachType>((o.value() >> 5) & 0x07u);
        msg.mForceToStandby = ((o.value() >> 3) & 0x01u) != 0;
        msg.mUpdateTimer = (o.value() >> 1) & 0x03u;
        msg.mRadioPriority = o.value() & 0x01u;
    }

    // routingAreaIdentification (raw, 6 octets)
    {
        auto rai = L3RoutingAreaIdentification::parse(br);
        if (!rai) return Expected<L3AttachAccept>::error(rai.error());
        msg.mRAI = std::move(rai).value();
    }

    // Optional P-TMSI (TLV, element identifier 0x18), then any remaining
    // optional information elements kept opaque and re-emitted verbatim
    // (TS 44.068 section 9.5).
    if (br.hasMore()) {
        uint8_t raw = static_cast<uint8_t>(br.peekField(8));
        if ((raw & 0x7Fu) == 0x18u) {
            auto type = br.readField(8);
            if (!type) return Expected<L3AttachAccept>::error(type.error());
            auto len = br.readField(8);
            if (!len) return Expected<L3AttachAccept>::error(len.error());
            auto mi = L3MobileIdentity::parse(br, len.value());
            if (!mi) return Expected<L3AttachAccept>::error(mi.error());
            msg.mHavePTMSI = true;
            msg.mPTMSI = std::move(mi).value();
        }
    }
    if (!detail::readOpaqueTail(br, msg.mAdditionalIes)) {
        return Expected<L3AttachAccept>::error(
            ParseError{ParseError::Code::TruncatedInput, "truncated optional IEs"});
    }

    return Expected<L3AttachAccept>::hold(std::move(msg));
}

void L3AttachAccept::write(BitWriter& bw) const {
    bw.writeField(((static_cast<uint8_t>(mAttachResult) & 0x07u) << 5) |
                  (mForceToStandby ? 0x08u : 0u) |
                  ((mUpdateTimer & 0x03u) << 1) | (mRadioPriority & 0x01u), 8);
    mRAI.write(bw);
    if (mHavePTMSI) {
        // allocated P-TMSI: element identifier, length, value (TLV).
        bw.writeField(0x18u, 8);
        bw.writeField(static_cast<uint32_t>(mPTMSI.lengthV()), 8);
        mPTMSI.write(bw);
    }
    detail::writeOpaqueTail(mAdditionalIes, bw);
}

void L3AttachAccept::text(std::ostream& os) const {
    os << "AttachAccept(result=";
    switch (mAttachResult) {
        case GMMAttachType::GPRSAttach: os << "GPRS"; break;
        case GMMAttachType::CombinedGPRSAndIMSIAttach: os << "Combined"; break;
    }
    os << ",forceStandby=" << mForceToStandby << ")";
}

L3AttachAccept L3AttachAccept::Builder::build() const {
    L3AttachAccept msg;
    msg.mAttachResult = m_attachResult;
    msg.mForceToStandby = m_forceToStandby;
    msg.mUpdateTimer = m_updateTimer;
    msg.mRadioPriority = m_radioPriority;
    msg.mRAI = m_rai;
    msg.mHavePTMSI = m_havePTMSI;
    msg.mPTMSI = m_ptmsi;
    msg.mAdditionalIes = m_additionalIes;
    return msg;
}

L3AttachAccept::Builder L3AttachAccept::builder() {
    return Builder{};
}

// ── L3AttachComplete (GSM 24.008 9.4.3) ──────────────────────────────

Expected<L3AttachComplete> L3AttachComplete::parse(BitReader&) {
    return Expected<L3AttachComplete>::hold(L3AttachComplete{});
}

void L3AttachComplete::write(BitWriter&) const {}

void L3AttachComplete::text(std::ostream& os) const {
    os << "AttachComplete";
}

L3AttachComplete L3AttachComplete::Builder::build() const {
    return L3AttachComplete{};
}

L3AttachComplete::Builder L3AttachComplete::builder() {
    return Builder{};
}

// ── L3AttachReject (TS 44.068 section 9.5) ────────────────────────────

size_t L3AttachReject::bodyLength() const {
    // gmmCause (one value octet) + opaque optional IEs
    return 1 + mAdditionalIes.size();
}

Expected<L3AttachReject> L3AttachReject::parse(BitReader& br) {
    L3AttachReject msg;

    // The body starts with the GMM cause as a single value octet carried
    // without an identifier (TS 44.068 section 9.5).
    auto r = br.readField(8);
    if (!r) return Expected<L3AttachReject>::error(r.error());
    msg.mCause = static_cast<GMMCause>(r.value());

    // Unrecognized optional IEs are kept opaque and re-emitted verbatim.
    if (!detail::readOpaqueTail(br, msg.mAdditionalIes)) {
        return Expected<L3AttachReject>::error(
            ParseError{ParseError::Code::TruncatedInput, "truncated optional IEs"});
    }

    return Expected<L3AttachReject>::hold(std::move(msg));
}

void L3AttachReject::write(BitWriter& bw) const {
    bw.writeField(static_cast<uint8_t>(mCause), 8);
    detail::writeOpaqueTail(mAdditionalIes, bw);
}

void L3AttachReject::text(std::ostream& os) const {
    os << "AttachReject(cause=" << GMMCause2Str(mCause) << ")";
}

L3AttachReject L3AttachReject::Builder::build() const {
    L3AttachReject msg;
    msg.mCause = m_cause;
    msg.mAdditionalIes = m_additionalIes;
    return msg;
}

L3AttachReject::Builder L3AttachReject::builder() {
    return Builder{};
}

// ── L3DetachRequest (TS 44.068 section 9.5) ───────────────────────────

size_t L3DetachRequest::bodyLength() const {
    size_t len = 1; // detachType(3)|powerOff/forceToStandby(1)|spare(4)
    if (mHavePTMSI) len += tlvLen(mPTMSI.lengthV());
    if (mHaveCause) len += 2; // gmmCause TV: IEI + one value octet
    len += mAdditionalIes.size();
    return len;
}

Expected<L3DetachRequest> L3DetachRequest::parse(BitReader& br) {
    L3DetachRequest msg;

    auto o = br.readField(8);
    if (!o) return Expected<L3DetachRequest>::error(o.error());
    msg.mDetachType = (o.value() >> 4) & 0x07u;
    msg.mPowerOff = ((o.value() >> 3) & 0x01u) != 0;

    // Optional IEs: the P-TMSI as a TLV (element identifier 0x18 + LV value)
    // and, in the network-to-MS direction, the GMM cause as a type-value pair
    // (IEI 0x25 + one value octet); any other IE is kept opaque
    // (TS 44.068 section 9.5).
    while (br.hasMore()) {
        uint8_t raw = static_cast<uint8_t>(br.peekField(8));
        if ((raw & 0x7Fu) == 0x18u) {
            auto type = br.readField(8);
            if (!type) return Expected<L3DetachRequest>::error(type.error());
            auto len = br.readField(8);
            if (!len) return Expected<L3DetachRequest>::error(len.error());
            auto mi = L3MobileIdentity::parse(br, len.value());
            if (!mi) return Expected<L3DetachRequest>::error(mi.error());
            msg.mHavePTMSI = true;
            msg.mPTMSI = std::move(mi).value();
        } else if ((raw & 0x7Fu) == 0x25u) {
            auto type = br.readField(8);
            if (!type) return Expected<L3DetachRequest>::error(type.error());
            auto cause = br.readField(8);
            if (!cause) return Expected<L3DetachRequest>::error(cause.error());
            msg.mHaveCause = true;
            msg.mCause = static_cast<GMMCause>(cause.value());
        } else {
            break; // unknown IE: the remainder is kept opaque
        }
    }
    if (!detail::readOpaqueTail(br, msg.mAdditionalIes)) {
        return Expected<L3DetachRequest>::error(
            ParseError{ParseError::Code::TruncatedInput, "truncated optional IEs"});
    }

    return Expected<L3DetachRequest>::hold(std::move(msg));
}

void L3DetachRequest::write(BitWriter& bw) const {
    bw.writeField(((mDetachType & 0x07u) << 4) | (mPowerOff ? 0x08u : 0u), 8);
    if (mHavePTMSI) {
        // allocated P-TMSI: element identifier, length, value (TLV).
        bw.writeField(0x18u, 8);
        bw.writeField(static_cast<uint32_t>(mPTMSI.lengthV()), 8);
        mPTMSI.write(bw);
    }
    if (mHaveCause) {
        bw.writeField(0x25u, 8);
        bw.writeField(static_cast<uint8_t>(mCause), 8);
    }
    detail::writeOpaqueTail(mAdditionalIes, bw);
}

void L3DetachRequest::text(std::ostream& os) const {
    os << "DetachRequest(type=" << mDetachType << ",powerOff=" << mPowerOff << ")";
}

L3DetachRequest L3DetachRequest::Builder::build() const {
    L3DetachRequest msg;
    msg.mDetachType = m_detachType;
    msg.mPowerOff = m_powerOff;
    msg.mForceToStandby = m_forceToStandby;
    msg.mHavePTMSI = m_havePTMSI;
    msg.mPTMSI = m_ptmsi;
    msg.mHaveCause = m_haveCause;
    msg.mCause = m_cause;
    msg.mAdditionalIes = m_additionalIes;
    return msg;
}

L3DetachRequest::Builder L3DetachRequest::builder() {
    return Builder{};
}

// ── L3DetachAccept (GSM 24.008 9.4.6) ────────────────────────────────

size_t L3DetachAccept::bodyLength() const {
    return mForceToStandby ? 1 : 0;
}

Expected<L3DetachAccept> L3DetachAccept::parse(BitReader& br) {
    L3DetachAccept msg;
    if (br.hasMore()) {
        auto o = br.readField(8);
        if (o) msg.mForceToStandby = ((o.value() >> 7) & 0x01) != 0;
    }
    return Expected<L3DetachAccept>::hold(std::move(msg));
}

void L3DetachAccept::write(BitWriter& bw) const {
    if (mForceToStandby) bw.writeField(0x80, 8);
}

void L3DetachAccept::text(std::ostream& os) const {
    os << "DetachAccept(forceStandby=" << mForceToStandby << ")";
}

L3DetachAccept L3DetachAccept::Builder::build() const {
    L3DetachAccept msg;
    msg.mForceToStandby = m_forceToStandby;
    return msg;
}

L3DetachAccept::Builder L3DetachAccept::builder() {
    return Builder{};
}

// ── L3RoutingAreaUpdateRequest (TS 44.068 section 9.5) ────────────────

size_t L3RoutingAreaUpdateRequest::bodyLength() const {
    size_t len = 1; // updateType(3)|forL3(1) | gprsCKSN(3)|spare(1)
    len += L3RoutingAreaIdentification::lengthV(); // oldRoutingAreaID (six value octets)
    len += lvLen(mMsRACap.size());                 // msRACap LV (mandatory on the wire)
    len += mAdditionalIes.size();                  // opaque optional IEs
    return len;
}

Expected<L3RoutingAreaUpdateRequest> L3RoutingAreaUpdateRequest::parse(BitReader& br) {
    L3RoutingAreaUpdateRequest msg;

    // GMM ROUTING AREA UPDATE REQUEST (TS 44.068): update type + forL3 in
    // the high half-octet and the GMM CKSN in bits 3:1 of the first octet,
    // old routing area identity (six value octets), MS radio access
    // capabilities (LV, mandatory); any further optional IEs are kept opaque.
    {
        auto o = br.readField(8);
        if (!o) return Expected<L3RoutingAreaUpdateRequest>::error(o.error());
        msg.mUpdateType = static_cast<GMMUpdateType>((o.value() >> 5) & 0x07u);
        msg.mForL3 = ((o.value() >> 4) & 0x01u) != 0;
        msg.mCKSN = static_cast<uint8_t>((o.value() >> 1) & 0x07u);
    }

    // oldRoutingAreaID (fixed six value octets)
    {
        auto rai = L3RoutingAreaIdentification::parse(br);
        if (!rai) return Expected<L3RoutingAreaUpdateRequest>::error(rai.error());
        msg.mOldRAI = std::move(rai).value();
    }

    // msRACap (LV, mandatory on the wire; no identifier)
    {
        auto len = br.readField(8);
        if (!len) return Expected<L3RoutingAreaUpdateRequest>::error(len.error());
        size_t vLen = len.value();
        if (vLen * 8 > br.remainingBits()) {
            return Expected<L3RoutingAreaUpdateRequest>::error(
                ParseError{ParseError::Code::TruncatedInput, "truncated msRACap", br.position() - 8});
        }
        msg.mMsRACap.resize(vLen);
        if (vLen > 0) {
            auto r = br.readBytes(msg.mMsRACap.data(), vLen);
            if (!r) return Expected<L3RoutingAreaUpdateRequest>::error(r.error());
        }
    }

    // Unrecognized optional IEs are kept opaque and re-emitted verbatim.
    if (!detail::readOpaqueTail(br, msg.mAdditionalIes)) {
        return Expected<L3RoutingAreaUpdateRequest>::error(
            ParseError{ParseError::Code::TruncatedInput, "truncated optional IEs"});
    }

    return Expected<L3RoutingAreaUpdateRequest>::hold(std::move(msg));
}

void L3RoutingAreaUpdateRequest::write(BitWriter& bw) const {
    bw.writeField(((static_cast<uint8_t>(mUpdateType) & 0x07u) << 5) |
                  ((mForL3 ? 1u : 0u) << 4) | ((mCKSN & 0x07u) << 1), 8);
    mOldRAI.write(bw);
    bw.writeField(static_cast<uint32_t>(mMsRACap.size()), 8);
    if (!mMsRACap.empty()) bw.writeBytes(mMsRACap.data(), mMsRACap.size());
    detail::writeOpaqueTail(mAdditionalIes, bw);
}

void L3RoutingAreaUpdateRequest::text(std::ostream& os) const {
    os << "RAUpdateReq(type=";
    switch (mUpdateType) {
        case GMMUpdateType::RAUpdated: os << "RA"; break;
        case GMMUpdateType::CombinedRALAUpdated: os << "CombinedRA-LA"; break;
        case GMMUpdateType::CombinedRALAWithImsiAttach: os << "CombinedRA-LA-IMSI"; break;
        case GMMUpdateType::PeriodicUpdating: os << "Periodic"; break;
    }
    os << ",CKSN=" << static_cast<int>(mCKSN) << ")";
}

L3RoutingAreaUpdateRequest L3RoutingAreaUpdateRequest::Builder::build() const {
    L3RoutingAreaUpdateRequest msg;
    msg.mUpdateType = m_updateType;
    msg.mForL3 = m_forL3;
    msg.mCKSN = static_cast<uint8_t>(mCKSN & 0x07u);
    msg.mOldRAI = m_oldRAI;
    msg.mMsRACap = m_msRACap;
    msg.mAdditionalIes = m_additionalIes;
    return msg;
}

L3RoutingAreaUpdateRequest::Builder L3RoutingAreaUpdateRequest::builder() {
    return Builder{};
}

// ── L3RoutingAreaUpdateAccept (GSM 24.008 9.4.15) ────────────────────

size_t L3RoutingAreaUpdateAccept::bodyLength() const {
    size_t len = 1; // forceToStandby|updateResult|...
    len += 6;       // routingAreaId (raw)
    if (mHavePTMSI) len += tlvLen(mPTMSI.lengthV());
    len += mAdditionalIes.size();
    return len;
}

Expected<L3RoutingAreaUpdateAccept> L3RoutingAreaUpdateAccept::parse(BitReader& br) {
    L3RoutingAreaUpdateAccept msg;

    // forceToStandby(1)|updateResult(3)|spare(1)|raUpdateTimer(2)|radioPriority(1)
    // = 1 octet (TS 44.068 section 9.5).
    {
        auto o = br.readField(8);
        if (!o) return Expected<L3RoutingAreaUpdateAccept>::error(o.error());
        msg.mForceToStandby = ((o.value() >> 7) & 0x01u) != 0;
        msg.mUpdateResult = static_cast<GMMUpdateType>((o.value() >> 4) & 0x07u);
        msg.mRAUpdateTimer = (o.value() >> 2) & 0x03u;
        msg.mRadioPriority = o.value() & 0x01u;
    }

    // routingAreaId (raw, 6 octets)
    {
        auto rai = L3RoutingAreaIdentification::parse(br);
        if (!rai) return Expected<L3RoutingAreaUpdateAccept>::error(rai.error());
        msg.mRAI = std::move(rai).value();
    }

    // Optional P-TMSI (TLV, element identifier 0x18), then any remaining
    // optional information elements kept opaque and re-emitted verbatim
    // (TS 44.068 section 9.5).
    if (br.hasMore()) {
        uint8_t raw = static_cast<uint8_t>(br.peekField(8));
        if ((raw & 0x7Fu) == 0x18u) {
            auto type = br.readField(8);
            if (!type) return Expected<L3RoutingAreaUpdateAccept>::error(type.error());
            auto len = br.readField(8);
            if (!len) return Expected<L3RoutingAreaUpdateAccept>::error(len.error());
            auto mi = L3MobileIdentity::parse(br, len.value());
            if (!mi) return Expected<L3RoutingAreaUpdateAccept>::error(mi.error());
            msg.mHavePTMSI = true;
            msg.mPTMSI = std::move(mi).value();
        }
    }
    if (!detail::readOpaqueTail(br, msg.mAdditionalIes)) {
        return Expected<L3RoutingAreaUpdateAccept>::error(
            ParseError{ParseError::Code::TruncatedInput, "truncated optional IEs"});
    }

    return Expected<L3RoutingAreaUpdateAccept>::hold(std::move(msg));
}

void L3RoutingAreaUpdateAccept::write(BitWriter& bw) const {
    bw.writeField((mForceToStandby ? 0x80u : 0u) |
                  ((static_cast<uint8_t>(mUpdateResult) & 0x07u) << 4) |
                  ((mRAUpdateTimer & 0x03u) << 2) | (mRadioPriority & 0x01u), 8);
    mRAI.write(bw);
    if (mHavePTMSI) {
        // allocated P-TMSI: element identifier, length, value (TLV).
        bw.writeField(0x18u, 8);
        bw.writeField(static_cast<uint32_t>(mPTMSI.lengthV()), 8);
        mPTMSI.write(bw);
    }
    detail::writeOpaqueTail(mAdditionalIes, bw);
}

void L3RoutingAreaUpdateAccept::text(std::ostream& os) const {
    os << "RAUpdateAccept(result=";
    switch (mUpdateResult) {
        case GMMUpdateType::RAUpdated: os << "RA"; break;
        case GMMUpdateType::CombinedRALAUpdated: os << "CombinedRA-LA"; break;
        case GMMUpdateType::CombinedRALAWithImsiAttach: os << "CombinedRA-LA-IMSI"; break;
        case GMMUpdateType::PeriodicUpdating: os << "Periodic"; break;
    }
    os << ")";
}

L3RoutingAreaUpdateAccept L3RoutingAreaUpdateAccept::Builder::build() const {
    L3RoutingAreaUpdateAccept msg;
    msg.mForceToStandby = m_forceToStandby;
    msg.mUpdateResult = m_updateResult;
    msg.mRAUpdateTimer = m_raUpdateTimer;
    msg.mRadioPriority = m_radioPriority;
    msg.mRAI = m_rai;
    msg.mHavePTMSI = m_havePTMSI;
    msg.mPTMSI = m_ptmsi;
    msg.mAdditionalIes = m_additionalIes;
    return msg;
}

L3RoutingAreaUpdateAccept::Builder L3RoutingAreaUpdateAccept::builder() {
    return Builder{};
}

// ── L3RoutingAreaUpdateComplete (GSM 24.008 9.4.16) ──────────────────

Expected<L3RoutingAreaUpdateComplete> L3RoutingAreaUpdateComplete::parse(BitReader&) {
    return Expected<L3RoutingAreaUpdateComplete>::hold(L3RoutingAreaUpdateComplete{});
}

void L3RoutingAreaUpdateComplete::write(BitWriter&) const {}

void L3RoutingAreaUpdateComplete::text(std::ostream& os) const {
    os << "RAUpdateComplete";
}

L3RoutingAreaUpdateComplete L3RoutingAreaUpdateComplete::Builder::build() const {
    return L3RoutingAreaUpdateComplete{};
}

L3RoutingAreaUpdateComplete::Builder L3RoutingAreaUpdateComplete::builder() {
    return Builder{};
}

// ── L3RoutingAreaUpdateReject (TS 44.068 section 9.5) ─────────────────

size_t L3RoutingAreaUpdateReject::bodyLength() const {
    // gmmCause (one value octet) + opaque optional IEs
    return 1 + mAdditionalIes.size();
}

Expected<L3RoutingAreaUpdateReject> L3RoutingAreaUpdateReject::parse(BitReader& br) {
    L3RoutingAreaUpdateReject msg;

    // The body starts with the GMM cause as a single value octet carried
    // without an identifier (TS 44.068 section 9.5).
    auto r = br.readField(8);
    if (!r) return Expected<L3RoutingAreaUpdateReject>::error(r.error());
    msg.mCause = static_cast<GMMCause>(r.value());

    // Unrecognized optional IEs are kept opaque and re-emitted verbatim.
    if (!detail::readOpaqueTail(br, msg.mAdditionalIes)) {
        return Expected<L3RoutingAreaUpdateReject>::error(
            ParseError{ParseError::Code::TruncatedInput, "truncated optional IEs"});
    }

    return Expected<L3RoutingAreaUpdateReject>::hold(std::move(msg));
}

void L3RoutingAreaUpdateReject::write(BitWriter& bw) const {
    bw.writeField(static_cast<uint8_t>(mCause), 8);
    detail::writeOpaqueTail(mAdditionalIes, bw);
}

void L3RoutingAreaUpdateReject::text(std::ostream& os) const {
    os << "RAUpdateReject(cause=" << GMMCause2Str(mCause) << ")";
}

L3RoutingAreaUpdateReject L3RoutingAreaUpdateReject::Builder::build() const {
    L3RoutingAreaUpdateReject msg;
    msg.mCause = m_cause;
    msg.mAdditionalIes = m_additionalIes;
    return msg;
}

L3RoutingAreaUpdateReject::Builder L3RoutingAreaUpdateReject::builder() {
    return Builder{};
}

// ── L3ServiceRequest (GSM 24.008 9.4.20) ─────────────────────────────

size_t L3ServiceRequest::bodyLength() const {
    size_t len = 1; // CKSN|serviceType
    len += lvLen(mPTMSI.lengthV());
    return len;
}

Expected<L3ServiceRequest> L3ServiceRequest::parse(BitReader& br) {
    L3ServiceRequest msg;

    // CKSN(3)|spare(1)|serviceType(3)|spare(1) = 1 octet
    {
        auto o = br.readField(8);
        if (!o) return Expected<L3ServiceRequest>::error(o.error());
        msg.mCKSN = (o.value() >> 4) & 0x0F;
        msg.mServiceType = o.value() & 0x0F;
    }

    // PTMSI (LV)
    {
        auto mi = parseLVMI(br);
        if (!mi) return Expected<L3ServiceRequest>::error(mi.error());
        msg.mPTMSI = std::move(mi).value();
    }

    return Expected<L3ServiceRequest>::hold(std::move(msg));
}

void L3ServiceRequest::write(BitWriter& bw) const {
    bw.writeField((mCKSN << 4) | mServiceType, 8);
    writeLVMI(mPTMSI, bw);
}

void L3ServiceRequest::text(std::ostream& os) const {
    os << "ServiceRequest(CKSN=" << (mCKSN >> 1 & 0x07) << ",type=" << mServiceType << ")";
}

L3ServiceRequest L3ServiceRequest::Builder::build() const {
    L3ServiceRequest msg;
    msg.mCKSN = mCKSN;
    msg.mServiceType = m_serviceType;
    msg.mPTMSI = m_ptmsi;
    return msg;
}

L3ServiceRequest::Builder L3ServiceRequest::builder() {
    return Builder{};
}

// ── L3ServiceAccept (GSM 24.008 9.4.21) ──────────────────────────────

Expected<L3ServiceAccept> L3ServiceAccept::parse(BitReader&) {
    return Expected<L3ServiceAccept>::hold(L3ServiceAccept{});
}

void L3ServiceAccept::write(BitWriter&) const {}

void L3ServiceAccept::text(std::ostream& os) const {
    os << "ServiceAccept";
}

L3ServiceAccept L3ServiceAccept::Builder::build() const {
    return L3ServiceAccept{};
}

L3ServiceAccept::Builder L3ServiceAccept::builder() {
    return Builder{};
}

// ── L3ServiceReject (TS 44.068 section 9.5) ───────────────────────────

size_t L3ServiceReject::bodyLength() const {
    // gmmCause (one value octet) + opaque optional IEs
    return 1 + mAdditionalIes.size();
}

Expected<L3ServiceReject> L3ServiceReject::parse(BitReader& br) {
    L3ServiceReject msg;

    // The body starts with the GMM cause as a single value octet carried
    // without an identifier (TS 44.068 section 9.5).
    auto r = br.readField(8);
    if (!r) return Expected<L3ServiceReject>::error(r.error());
    msg.mCause = static_cast<GMMCause>(r.value());

    // Unrecognized optional IEs are kept opaque and re-emitted verbatim.
    if (!detail::readOpaqueTail(br, msg.mAdditionalIes)) {
        return Expected<L3ServiceReject>::error(
            ParseError{ParseError::Code::TruncatedInput, "truncated optional IEs"});
    }

    return Expected<L3ServiceReject>::hold(std::move(msg));
}

void L3ServiceReject::write(BitWriter& bw) const {
    bw.writeField(static_cast<uint8_t>(mCause), 8);
    detail::writeOpaqueTail(mAdditionalIes, bw);
}

void L3ServiceReject::text(std::ostream& os) const {
    os << "ServiceReject(cause=" << GMMCause2Str(mCause) << ")";
}

L3ServiceReject L3ServiceReject::Builder::build() const {
    L3ServiceReject msg;
    msg.mCause = m_cause;
    msg.mAdditionalIes = m_additionalIes;
    return msg;
}

L3ServiceReject::Builder L3ServiceReject::builder() {
    return Builder{};
}

// ── L3P_TMSIReallocationCommand (GSM 24.008 9.4.8) ───────────────────

size_t L3P_TMSIReallocationCommand::bodyLength() const {
    size_t len = 1; // PTMSI_Type|forceToStandby
    len += 6;       // routingAreaId (raw)
    if (mHavePTMSI) len += tlvLen(mPTMSI.lengthV());
    len += mAdditionalIes.size();
    return len;
}

Expected<L3P_TMSIReallocationCommand> L3P_TMSIReallocationCommand::parse(BitReader& br) {
    L3P_TMSIReallocationCommand msg;

    // P-TMSI type in bit 7, force to standby in bit 4 (TS 44.068 section 9.5).
    {
        auto o = br.readField(8);
        if (!o) return Expected<L3P_TMSIReallocationCommand>::error(o.error());
        msg.mPTMSIType = static_cast<GMMPTMSIType>((o.value() >> 7) & 0x01u);
        msg.mForceToStandby = ((o.value() >> 4) & 0x01u) != 0;
    }

    // routingAreaId (raw, 6 octets)
    {
        auto rai = L3RoutingAreaIdentification::parse(br);
        if (!rai) return Expected<L3P_TMSIReallocationCommand>::error(rai.error());
        msg.mRAI = std::move(rai).value();
    }

    // Optional P-TMSI (TLV, element identifier 0x18), then any remaining
    // optional information elements kept opaque and re-emitted verbatim
    // (TS 44.068 section 9.5).
    if (br.hasMore()) {
        uint8_t raw = static_cast<uint8_t>(br.peekField(8));
        if ((raw & 0x7Fu) == 0x18u) {
            auto type = br.readField(8);
            if (!type) return Expected<L3P_TMSIReallocationCommand>::error(type.error());
            auto len = br.readField(8);
            if (!len) return Expected<L3P_TMSIReallocationCommand>::error(len.error());
            auto mi = L3MobileIdentity::parse(br, len.value());
            if (!mi) return Expected<L3P_TMSIReallocationCommand>::error(mi.error());
            msg.mHavePTMSI = true;
            msg.mPTMSI = std::move(mi).value();
        }
    }
    if (!detail::readOpaqueTail(br, msg.mAdditionalIes)) {
        return Expected<L3P_TMSIReallocationCommand>::error(
            ParseError{ParseError::Code::TruncatedInput, "truncated optional IEs"});
    }

    return Expected<L3P_TMSIReallocationCommand>::hold(std::move(msg));
}

void L3P_TMSIReallocationCommand::write(BitWriter& bw) const {
    bw.writeField((static_cast<uint8_t>(mPTMSIType) << 7) | (mForceToStandby ? 0x10u : 0u), 8);
    mRAI.write(bw);
    if (mHavePTMSI) {
        // allocated P-TMSI: element identifier, length, value (TLV).
        bw.writeField(0x18u, 8);
        bw.writeField(static_cast<uint32_t>(mPTMSI.lengthV()), 8);
        mPTMSI.write(bw);
    }
    detail::writeOpaqueTail(mAdditionalIes, bw);
}

void L3P_TMSIReallocationCommand::text(std::ostream& os) const {
    os << "P_TMSIRreallocCmd(type=" << mPTMSIType << ")";
}

L3P_TMSIReallocationCommand L3P_TMSIReallocationCommand::Builder::build() const {
    L3P_TMSIReallocationCommand msg;
    msg.mPTMSIType = m_ptmsiType;
    msg.mForceToStandby = m_forceToStandby;
    msg.mRAI = m_rai;
    msg.mHavePTMSI = m_havePTMSI;
    msg.mPTMSI = m_ptmsi;
    msg.mAdditionalIes = m_additionalIes;
    return msg;
}

L3P_TMSIReallocationCommand::Builder L3P_TMSIReallocationCommand::builder() {
    return Builder{};
}

// ── L3P_TMSIReallocationComplete (GSM 24.008 9.4.8) ──────────────────

Expected<L3P_TMSIReallocationComplete> L3P_TMSIReallocationComplete::parse(BitReader&) {
    return Expected<L3P_TMSIReallocationComplete>::hold(L3P_TMSIReallocationComplete{});
}

void L3P_TMSIReallocationComplete::write(BitWriter&) const {}

void L3P_TMSIReallocationComplete::text(std::ostream& os) const {
    os << "P_TMSIRreallocComplete";
}

L3P_TMSIReallocationComplete L3P_TMSIReallocationComplete::Builder::build() const {
    return L3P_TMSIReallocationComplete{};
}

L3P_TMSIReallocationComplete::Builder L3P_TMSIReallocationComplete::builder() {
    return Builder{};
}

// ── L3AuthenticationAndCipheringRequest (TS 44.068 section 9.5) ───────

size_t L3AuthenticationAndCipheringRequest::bodyLength() const {
    // Ciphering algorithm / IMEISV request / force-to-standby octet, the AC
    // reference number octet, then the TV-formatted RAND (identifier + 16
    // value octets).
    return 2 + tvLen(16);
}

Expected<L3AuthenticationAndCipheringRequest> L3AuthenticationAndCipheringRequest::parse(BitReader& br) {
    L3AuthenticationAndCipheringRequest msg;

    // cipheringAlgorithm(3)|spare(1)|imeisvRequest(1)|forceToStandby(1)|spare(2)
    // = 1 octet (TS 44.068 section 9.5).
    {
        auto o = br.readField(8);
        if (!o) return Expected<L3AuthenticationAndCipheringRequest>::error(o.error());
        msg.mCipheringAlgorithm = (o.value() >> 5) & 0x07u;
        msg.mImeisvRequest = ((o.value() >> 3) & 0x01u) != 0;
        msg.mForceToStandby = ((o.value() >> 2) & 0x01u) != 0;
    }

    // acReferenceNumber(4)|spare(4) = 1 octet
    {
        auto o = br.readField(8);
        if (!o) return Expected<L3AuthenticationAndCipheringRequest>::error(o.error());
        msg.mACReferenceNumber = (o.value() >> 4) & 0x0Fu;
    }

    // authenticationParameterRAND: TV format, the identifier octet is followed
    // by the sixteen value octets (TS 44.068 section 9.5).
    {
        auto type = br.readField(8);
        if (!type) return Expected<L3AuthenticationAndCipheringRequest>::error(type.error());
        if ((type.value() & 0x7Fu) != L3AuthRAND::IEI) {
            return Expected<L3AuthenticationAndCipheringRequest>::error(
                ParseError{ParseError::Code::InvalidIE, "unexpected RAND element identifier"});
        }
        auto rand = L3AuthRAND::parse(br);
        if (!rand) return Expected<L3AuthenticationAndCipheringRequest>::error(rand.error());
        msg.mRAND = std::move(rand).value();
    }

    return Expected<L3AuthenticationAndCipheringRequest>::hold(std::move(msg));
}

void L3AuthenticationAndCipheringRequest::write(BitWriter& bw) const {
    bw.writeField((static_cast<uint8_t>(mCipheringAlgorithm) << 5) | (mImeisvRequest ? 0x08u : 0u) |
                  (mForceToStandby ? 0x04u : 0u), 8);
    bw.writeField((static_cast<uint8_t>(mACReferenceNumber) & 0x0Fu) << 4, 8);
    // authenticationParameterRAND: identifier octet followed by the value (TV).
    bw.writeField(L3AuthRAND::IEI, 8);
    mRAND.write(bw);
}

void L3AuthenticationAndCipheringRequest::text(std::ostream& os) const {
    os << "AuthCipherReq(alg=" << mCipheringAlgorithm << ",acRef=" << mACReferenceNumber << ")";
}

L3AuthenticationAndCipheringRequest L3AuthenticationAndCipheringRequest::Builder::build() const {
    L3AuthenticationAndCipheringRequest msg;
    msg.mCipheringAlgorithm = m_cipheringAlgorithm;
    msg.mImeisvRequest = m_imeisvRequest;
    msg.mForceToStandby = m_forceToStandby;
    msg.mACReferenceNumber = m_acReferenceNumber;
    msg.mRAND = m_rand;
    return msg;
}

L3AuthenticationAndCipheringRequest::Builder L3AuthenticationAndCipheringRequest::builder() {
    return Builder{};
}

// ── L3AuthenticationAndCipheringResponse (TS 44.068 section 9.5) ──────

size_t L3AuthenticationAndCipheringResponse::bodyLength() const {
    // AC reference number octet, then the TV-formatted RES (identifier + four
    // value octets).
    return 1 + tvLen(4);
}

Expected<L3AuthenticationAndCipheringResponse> L3AuthenticationAndCipheringResponse::parse(BitReader& br) {
    L3AuthenticationAndCipheringResponse msg;

    // acReferenceNumber(4)|spare(4) = 1 octet (TS 44.068 section 9.5).
    {
        auto o = br.readField(8);
        if (!o) return Expected<L3AuthenticationAndCipheringResponse>::error(o.error());
        msg.mACReferenceNumber = (o.value() >> 4) & 0x0Fu;
    }

    // authenticationParameterResponse: TV format, the identifier octet is
    // followed by the four value octets.
    {
        auto type = br.readField(8);
        if (!type) return Expected<L3AuthenticationAndCipheringResponse>::error(type.error());
        if ((type.value() & 0x7Fu) != L3AuthRES::IEI) {
            return Expected<L3AuthenticationAndCipheringResponse>::error(
                ParseError{ParseError::Code::InvalidIE, "unexpected RES element identifier"});
        }
        auto res = L3AuthRES::parse(br);
        if (!res) return Expected<L3AuthenticationAndCipheringResponse>::error(res.error());
        msg.mRES = std::move(res).value();
    }

    return Expected<L3AuthenticationAndCipheringResponse>::hold(std::move(msg));
}

void L3AuthenticationAndCipheringResponse::write(BitWriter& bw) const {
    bw.writeField((static_cast<uint8_t>(mACReferenceNumber) & 0x0Fu) << 4, 8);
    // authenticationParameterResponse: identifier octet followed by the value (TV).
    bw.writeField(L3AuthRES::IEI, 8);
    mRES.write(bw);
}

void L3AuthenticationAndCipheringResponse::text(std::ostream& os) const {
    os << "AuthCipherResp(acRef=" << mACReferenceNumber << ")";
}

L3AuthenticationAndCipheringResponse L3AuthenticationAndCipheringResponse::Builder::build() const {
    L3AuthenticationAndCipheringResponse msg;
    msg.mACReferenceNumber = m_acReferenceNumber;
    msg.mRES = m_res;
    return msg;
}

L3AuthenticationAndCipheringResponse::Builder L3AuthenticationAndCipheringResponse::builder() {
    return Builder{};
}

// ── L3AuthenticationAndCipheringReject (GSM 24.008 9.4.9) ─────────────

Expected<L3AuthenticationAndCipheringReject> L3AuthenticationAndCipheringReject::parse(BitReader&) {
    return Expected<L3AuthenticationAndCipheringReject>::hold(L3AuthenticationAndCipheringReject{});
}

void L3AuthenticationAndCipheringReject::write(BitWriter&) const {}

void L3AuthenticationAndCipheringReject::text(std::ostream& os) const {
    os << "AuthCipherReject";
}

L3AuthenticationAndCipheringReject L3AuthenticationAndCipheringReject::Builder::build() const {
    return L3AuthenticationAndCipheringReject{};
}

L3AuthenticationAndCipheringReject::Builder L3AuthenticationAndCipheringReject::builder() {
    return Builder{};
}

// ── L3GMMIdentityRequest (GSM 24.008 9.4.7) ──────────────────────────

Expected<L3GMMIdentityRequest> L3GMMIdentityRequest::parse(BitReader& br) {
    L3GMMIdentityRequest msg;

    // identityType(3)|spare(1)|forceToStandby(1)|spare(4) in the first octet of
    // the two-octet value part (GMM Identity Request, TS 24.008 9.4.7).
    auto o1 = br.readField(8);
    if (!o1) return Expected<L3GMMIdentityRequest>::error(o1.error());
    msg.mIdentityType = static_cast<MobileIDType>((o1.value() >> 5) & 0x07);
    msg.mForceToStandby = ((o1.value() >> 4) & 0x01) != 0;

    // Second (spare) octet of the two-octet value part: written by write()
    // (bodyLength() == 2); it must be consumed so the parse is the exact
    // inverse of the write (a 4-byte frame whose standard parse leaves a tail
    // is treated as a short message).
    auto o2 = br.readField(8);
    if (!o2) return Expected<L3GMMIdentityRequest>::error(o2.error());

    return Expected<L3GMMIdentityRequest>::hold(std::move(msg));
}

void L3GMMIdentityRequest::write(BitWriter& bw) const {
    bw.writeField(((static_cast<uint8_t>(mIdentityType) & 0x07) << 5) | (mForceToStandby ? 0x10 : 0), 8);
    bw.writeField(0, 8); // spare
}

void L3GMMIdentityRequest::text(std::ostream& os) const {
    os << "GMMIdentityReq(type=" << mIdentityType << ")";
}

L3GMMIdentityRequest L3GMMIdentityRequest::Builder::build() const {
    L3GMMIdentityRequest msg;
    msg.mIdentityType = m_identityType;
    msg.mForceToStandby = m_forceToStandby;
    return msg;
}

L3GMMIdentityRequest::Builder L3GMMIdentityRequest::builder() {
    return Builder{};
}

// ── L3GMMIdentityResponse (GSM 24.008 9.4.10) ────────────────────────

size_t L3GMMIdentityResponse::bodyLength() const {
    return lvLen(mMobileIdentity.lengthV());
}

Expected<L3GMMIdentityResponse> L3GMMIdentityResponse::parse(BitReader& br) {
    L3GMMIdentityResponse msg;
    auto mi = parseLVMI(br);
    if (!mi) return Expected<L3GMMIdentityResponse>::error(mi.error());
    msg.mMobileIdentity = std::move(mi).value();
    return Expected<L3GMMIdentityResponse>::hold(std::move(msg));
}

void L3GMMIdentityResponse::write(BitWriter& bw) const {
    writeLVMI(mMobileIdentity, bw);
}

void L3GMMIdentityResponse::text(std::ostream& os) const {
    os << "GMMIdentityResp(";
    mMobileIdentity.text(os);
    os << ")";
}

L3GMMIdentityResponse L3GMMIdentityResponse::Builder::build() const {
    L3GMMIdentityResponse msg;
    msg.mMobileIdentity = m_mobileIdentity;
    return msg;
}

L3GMMIdentityResponse::Builder L3GMMIdentityResponse::builder() {
    return Builder{};
}

// ── L3AuthenticationAndCipheringFailure (TS 44.068 section 9.5) ───────

size_t L3AuthenticationAndCipheringFailure::bodyLength() const {
    // gmmCause (one value octet) + authenticationFailureParameter TLV
    return 1 + tlvLen(mAuthFailureParam.lengthV()) + mAdditionalIes.size();
}

Expected<L3AuthenticationAndCipheringFailure> L3AuthenticationAndCipheringFailure::parse(BitReader& br) {
    L3AuthenticationAndCipheringFailure msg;

    // The body starts with the GMM cause as a single value octet carried
    // without an identifier (TS 44.068 section 9.5).
    auto r = br.readField(8);
    if (!r) return Expected<L3AuthenticationAndCipheringFailure>::error(r.error());
    msg.mCause = static_cast<GMMCause>(r.value());

    // authenticationFailureParameter (TLV, IEI 0x30), when present.
    if (br.hasMore()) {
        uint8_t raw = static_cast<uint8_t>(br.peekField(8));
        if ((raw & 0x7Fu) == L3AuthFailureParam::IEI) {
            auto type = br.readField(8);
            if (!type) return Expected<L3AuthenticationAndCipheringFailure>::error(type.error());
            auto param = L3AuthFailureParam::parse(br);
            if (!param) return Expected<L3AuthenticationAndCipheringFailure>::error(param.error());
            msg.mAuthFailureParam = std::move(param).value();
        }
    }

    // Unrecognized optional IEs are kept opaque and re-emitted verbatim.
    if (!detail::readOpaqueTail(br, msg.mAdditionalIes)) {
        return Expected<L3AuthenticationAndCipheringFailure>::error(
            ParseError{ParseError::Code::TruncatedInput, "truncated optional IEs"});
    }

    return Expected<L3AuthenticationAndCipheringFailure>::hold(std::move(msg));
}

void L3AuthenticationAndCipheringFailure::write(BitWriter& bw) const {
    bw.writeField(static_cast<uint8_t>(mCause), 8);

    // authenticationFailureParameter: IEI octet + length + AUTS value.
    bw.writeField(L3AuthFailureParam::IEI, 8);
    mAuthFailureParam.write(bw);

    detail::writeOpaqueTail(mAdditionalIes, bw);
}

void L3AuthenticationAndCipheringFailure::text(std::ostream& os) const {
    os << "AuthCipherFail(cause=" << GMMCause2Str(mCause) << ")";
}

L3AuthenticationAndCipheringFailure L3AuthenticationAndCipheringFailure::Builder::build() const {
    L3AuthenticationAndCipheringFailure msg;
    msg.mCause = m_cause;
    msg.mAuthFailureParam = m_authFailureParam;
    msg.mAdditionalIes = m_additionalIes;
    return msg;
}

L3AuthenticationAndCipheringFailure::Builder L3AuthenticationAndCipheringFailure::builder() {
    return Builder{};
}

// ── L3GMMStatus (GSM 24.008 9.4.24) ──────────────────────────────────

Expected<L3GMMStatus> L3GMMStatus::parse(BitReader& br) {
    auto o = br.readField(8);
    if (!o) return Expected<L3GMMStatus>::error(o.error());
    L3GMMStatus msg;
    msg.mCause = static_cast<GMMCause>(o.value());
    return Expected<L3GMMStatus>::hold(std::move(msg));
}

void L3GMMStatus::write(BitWriter& bw) const {
    bw.writeField(static_cast<uint8_t>(mCause), 8);
}

void L3GMMStatus::text(std::ostream& os) const {
    os << "GMMStatus(cause=" << GMMCause2Str(mCause) << ")";
}

L3GMMStatus L3GMMStatus::Builder::build() const {
    L3GMMStatus msg;
    msg.mCause = m_cause;
    return msg;
}

L3GMMStatus::Builder L3GMMStatus::builder() {
    return Builder{};
}

// ── L3GMMInformation (GSM 24.008) ─────────────────────────────────────

Expected<L3GMMInformation> L3GMMInformation::parse(BitReader&) {
    return Expected<L3GMMInformation>::hold(L3GMMInformation{});
}

void L3GMMInformation::write(BitWriter&) const {}

void L3GMMInformation::text(std::ostream& os) const {
    os << "GMMInformation";
}

L3GMMInformation L3GMMInformation::Builder::build() const {
    return L3GMMInformation{};
}

L3GMMInformation::Builder L3GMMInformation::builder() {
    return Builder{};
}

// ── gmmMessageName ──────────────────────────────────────────────────────

const char* gmmMessageName(int mti) {
    switch (mti) {
        case L3AttachRequest::MTI:                        return "AttachRequest";
        case L3AttachAccept::MTI:                         return "AttachAccept";
        case L3AttachComplete::MTI:                       return "AttachComplete";
        case L3AttachReject::MTI:                         return "AttachReject";
        case L3DetachRequest::MTI:                        return "DetachRequest";
        case L3DetachAccept::MTI:                         return "DetachAccept";
        case L3RoutingAreaUpdateRequest::MTI:             return "RoutingAreaUpdateRequest";
        case L3RoutingAreaUpdateAccept::MTI:              return "RoutingAreaUpdateAccept";
        case L3RoutingAreaUpdateComplete::MTI:            return "RoutingAreaUpdateComplete";
        case L3RoutingAreaUpdateReject::MTI:              return "RoutingAreaUpdateReject";
        case L3ServiceRequest::MTI:                       return "ServiceRequest";
        case L3ServiceAccept::MTI:                        return "ServiceAccept";
        case L3ServiceReject::MTI:                        return "ServiceReject";
        case L3P_TMSIReallocationCommand::MTI:            return "P_TMSIReallocationCommand";
        case L3P_TMSIReallocationComplete::MTI:           return "P_TMSIReallocationComplete";
        case L3AuthenticationAndCipheringRequest::MTI:    return "AuthAndCipheringRequest";
        case L3AuthenticationAndCipheringResponse::MTI:   return "AuthAndCipheringResponse";
        case L3AuthenticationAndCipheringReject::MTI:     return "AuthAndCipheringReject";
        case L3GMMIdentityRequest::MTI:                   return "GMMIdentityRequest";
        case L3GMMIdentityResponse::MTI:                  return "GMMIdentityResponse";
        case L3AuthenticationAndCipheringFailure::MTI:    return "AuthAndCipheringFailure";
        case L3GMMStatus::MTI:                            return "GMMStatus";
        case L3GMMInformation::MTI:                       return "GMMInformation";
        default:                                          return "Unknown_GMM";
    }
}

} // namespace gsml3parser
