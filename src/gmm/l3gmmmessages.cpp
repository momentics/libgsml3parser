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
    // gprsCKSN(3)|spare(1) high, attachType(3)|forL3(1) low = 1 octet
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

    // per TS 24.068 section 9.5 the attach type (three bits plus the forL3
    // indicator) occupies the low half-octet and the GMM CKSN (three bits
    // plus one reserved) the high half-octet.
    {
        auto o = br.readField(8);
        if (!o) return Expected<L3AttachRequest>::error(o.error());
        msg.mCKSN = static_cast<uint8_t>((o.value() >> 4) & 0x07u);
        msg.mForL3 = ((o.value() >> 3) & 0x01u) != 0;
        msg.mAttachType = static_cast<GMMAttachType>(o.value() & 0x07u);
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

    // gprsCKSN(3)|spare(1) high, attachType(3)|forL3(1) low (TS 24.068 section 9.5)
    bw.writeField(((mCKSN & 0x07u) << 4) | (mForL3 ? 0x08u : 0u) |
                  (static_cast<uint8_t>(mAttachType) & 0x07u), 8);

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

// ── L3AttachAccept (TS 44.068 section 9.5) ────────────────────────────

size_t L3AttachAccept::bodyLength() const {
    size_t len = 3; // leading part: result/FTS octet, timer IE octet, priority octet
    len += 6;       // routingAreaIdentification (raw)
    if (mHavePtmsiSignature) len += 4; // P-TMSI signature TV: IEI + three octets
    if (mHaveReadyTimer) len += 3;     // ready timer TV: IEI + two octets
    if (mHavePTMSI) len += tlvLen(mPTMSI.lengthV());
    len += mAdditionalIes.size();
    return len;
}

Expected<L3AttachAccept> L3AttachAccept::parse(BitReader& br) {
    L3AttachAccept msg;

    // Three-octet leading part (TS 44.068 section 9.5): first octet is
    // forceToStandby in the high nibble and the attach result (a raw 3-bit
    // value) in the low one; second octet is the periodic RA update timer
    // (units(3) high|value(5) low, TS 24.008 section 10.5.7.3); third octet
    // is the radio priority for SMS (high nibble) and TOM8 (low nibble).
    {
        auto o = br.readField(8);
        if (!o) return Expected<L3AttachAccept>::error(o.error());
        msg.mForceToStandby = ((o.value() >> 4) & 0x01u) != 0;
        msg.mAttachResult = static_cast<GMMAttachType>(o.value() & 0x0Fu);
    }
    {
        auto o = br.readField(8);
        if (!o) return Expected<L3AttachAccept>::error(o.error());
        msg.mRaUpdateTimer.units = static_cast<uint8_t>((o.value() >> 5) & 0x07u);
        msg.mRaUpdateTimer.value = static_cast<uint8_t>(o.value() & 0x1Fu);
    }
    {
        auto o = br.readField(8);
        if (!o) return Expected<L3AttachAccept>::error(o.error());
        msg.mRadioPrioritySms = static_cast<uint8_t>((o.value() >> 4) & 0x0Fu);
        msg.mRadioPriorityTom8 = static_cast<uint8_t>(o.value() & 0x0Fu);
    }

    // routingAreaIdentification (raw, 6 octets)
    {
        auto rai = L3RoutingAreaIdentification::parse(br);
        if (!rai) return Expected<L3AttachAccept>::error(rai.error());
        msg.mRAI = std::move(rai).value();
    }

    // Optional IEs in any order: P-TMSI signature (TV, element identifier
    // 0x19 + three value octets), ready timer (TV, element identifier 0x17
    // + two value octets) and the allocated P-TMSI (TLV, element identifier
    // 0x18); any remaining optional information elements are kept opaque
    // and re-emitted verbatim (TS 44.068 section 9.5).
    while (br.hasMore()) {
        uint8_t raw = static_cast<uint8_t>(br.peekField(8));
        uint8_t iei = static_cast<uint8_t>(raw & 0x7Fu);
        if (iei == 0x19u && !msg.mHavePtmsiSignature) {
            auto type = br.readField(8);
            if (!type) return Expected<L3AttachAccept>::error(type.error());
            auto sig = br.readBytes(msg.mPtmsiSignature.data(), 3);
            if (!sig) return Expected<L3AttachAccept>::error(sig.error());
            msg.mHavePtmsiSignature = true;
        } else if (iei == 0x17u && !msg.mHaveReadyTimer) {
            auto type = br.readField(8);
            if (!type) return Expected<L3AttachAccept>::error(type.error());
            auto rt = br.readBytes(msg.mReadyTimer.data(), 2);
            if (!rt) return Expected<L3AttachAccept>::error(rt.error());
            msg.mHaveReadyTimer = true;
        } else if (iei == 0x18u && !msg.mHavePTMSI) {
            auto type = br.readField(8);
            if (!type) return Expected<L3AttachAccept>::error(type.error());
            auto len = br.readField(8);
            if (!len) return Expected<L3AttachAccept>::error(len.error());
            auto mi = L3MobileIdentity::parse(br, len.value());
            if (!mi) return Expected<L3AttachAccept>::error(mi.error());
            msg.mHavePTMSI = true;
            msg.mPTMSI = std::move(mi).value();
        } else {
            break; // unknown IE: the remainder is kept opaque
        }
    }
    if (!detail::readOpaqueTail(br, msg.mAdditionalIes)) {
        return Expected<L3AttachAccept>::error(
            ParseError{ParseError::Code::TruncatedInput, "truncated optional IEs"});
    }

    return Expected<L3AttachAccept>::hold(std::move(msg));
}

void L3AttachAccept::write(BitWriter& bw) const {
    // forceToStandby(4) high nibble | attachResult(4) low nibble; the result
    // is a raw 3-bit value stored in the four-bit field (TS 44.068 section 9.5).
    bw.writeField(((mForceToStandby ? 1u : 0u) << 4) |
                  (static_cast<uint8_t>(mAttachResult) & 0x0Fu), 8);
    // periodic RA update timer value part (TS 24.008 section 10.5.7.3).
    bw.writeField(mRaUpdateTimer.octet(), 8);
    // radio priority for SMS (high nibble) and TOM8 (low nibble).
    bw.writeField(((mRadioPrioritySms & 0x0Fu) << 4) | (mRadioPriorityTom8 & 0x0Fu), 8);
    mRAI.write(bw);
    if (mHavePtmsiSignature) {
        // P-TMSI signature: element identifier followed by the value (TV).
        bw.writeField(0x19u, 8);
        bw.writeBytes(mPtmsiSignature.data(), 3);
    }
    if (mHaveReadyTimer) {
        // ready timer: element identifier followed by the two value octets (TV).
        bw.writeField(0x17u, 8);
        bw.writeBytes(mReadyTimer.data(), 2);
    }
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
    os << ",forceStandby=" << mForceToStandby
       << ",timer=" << static_cast<int>(mRaUpdateTimer.octet()) << ")";
}

L3AttachAccept L3AttachAccept::Builder::build() const {
    L3AttachAccept msg;
    msg.mAttachResult = m_attachResult;
    msg.mForceToStandby = m_forceToStandby;
    msg.mRaUpdateTimer = m_raUpdateTimer;
    msg.mRadioPrioritySms = static_cast<uint8_t>(m_radioPrioritySms & 0x0Fu);
    msg.mRadioPriorityTom8 = static_cast<uint8_t>(m_radioPriorityTom8 & 0x0Fu);
    msg.mRAI = m_rai;
    msg.mHavePtmsiSignature = m_havePtmsiSignature;
    msg.mPtmsiSignature = m_ptmsiSignature;
    msg.mHaveReadyTimer = m_haveReadyTimer;
    msg.mReadyTimer = m_readyTimer;
    msg.mHavePTMSI = m_havePTMSI;
    msg.mPTMSI = m_ptmsi;
    msg.mAdditionalIes = m_additionalIes;
    return msg;
}

L3AttachAccept::Builder L3AttachAccept::builder() {
    return Builder{};
}

// ── L3AttachComplete (GSM 24.008 9.4.3) ──────────────────────────────

Expected<L3AttachComplete> L3AttachComplete::parse(BitReader& br) {
    L3AttachComplete msg;
    // No mandatory body fields; any optional IEs are kept opaque and
    // re-emitted verbatim (TS 44.068 section 9.5).
    if (!detail::readOpaqueTail(br, msg.mAdditionalIes)) {
        return Expected<L3AttachComplete>::error(
            ParseError{ParseError::Code::TruncatedInput, "truncated optional IEs"});
    }
    return Expected<L3AttachComplete>::hold(std::move(msg));
}

void L3AttachComplete::write(BitWriter& bw) const {
    detail::writeOpaqueTail(mAdditionalIes, bw);
}

void L3AttachComplete::text(std::ostream& os) const {
    os << "AttachComplete";
}

L3AttachComplete L3AttachComplete::Builder::build() const {
    L3AttachComplete msg;
    msg.mAdditionalIes = m_additionalIes;
    return msg;
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
    size_t len = 1; // detachType(3)|powerOffFlag/forceToStandby(1)|spare
    if (mHavePTMSI) len += tlvLen(mPTMSI.lengthV());
    if (mHaveCause) len += 2; // gmmCause TV: IEI + one value octet
    len += mAdditionalIes.size();
    return len;
}

Expected<L3DetachRequest> L3DetachRequest::parse(BitReader& br) {
    L3DetachRequest msg;

    // detachType(3)|powerOffFlag(1, MS->SGSN) / forceToStandby(1,
    // SGSN->MS)|spare = one octet, the type in the low three bits
    // (TS 44.068 section 9.5).
    auto o = br.readField(8);
    if (!o) return Expected<L3DetachRequest>::error(o.error());
    msg.mDetachType = o.value() & 0x07u;
    msg.mPowerOff = ((o.value() >> 3) & 0x01u) != 0;
    msg.mForceToStandby = ((o.value() >> 4) & 0x01u) != 0;

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
    // The detach type occupies the low three bits of the octet value; the
    // power-off flag (MS->SGSN) is bit 3 and force to standby (SGSN->MS)
    // is bit 4 (TS 44.068 section 9.5).
    bw.writeField((mDetachType & 0x07u) | (mPowerOff ? 0x08u : 0u) |
                  (mForceToStandby ? 0x10u : 0u), 8);
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
    msg.mDetachType = static_cast<uint8_t>(m_detachType & 0x07u);
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
    // Exactly one octet, always present (TS 44.068 section 9.5).
    return 1;
}

Expected<L3DetachAccept> L3DetachAccept::parse(BitReader& br) {
    L3DetachAccept msg;
    // One-octet body: forceToStandby in bit 0 of the octet value, the
    // remaining bits spare (TS 44.068 section 9.5).
    auto o = br.readField(8);
    if (!o) return Expected<L3DetachAccept>::error(o.error());
    msg.mForceToStandby = (o.value() & 0x01u) != 0;
    return Expected<L3DetachAccept>::hold(std::move(msg));
}

void L3DetachAccept::write(BitWriter& bw) const {
    // Spare high bits are zero; the force-to-standby indicator is bit 0.
    bw.writeField(mForceToStandby ? 1u : 0u, 8);
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
    size_t len = 1; // gprsCKSN(3)|spare(1) high, updateType(3)|forL3(1) low
    len += L3RoutingAreaIdentification::lengthV(); // oldRoutingAreaID (six value octets)
    len += lvLen(mMsRACap.size());                 // msRACap LV (mandatory on the wire)
    len += mAdditionalIes.size();                  // opaque optional IEs
    return len;
}

Expected<L3RoutingAreaUpdateRequest> L3RoutingAreaUpdateRequest::parse(BitReader& br) {
    L3RoutingAreaUpdateRequest msg;

    // GMM ROUTING AREA UPDATE REQUEST (TS 24.068 section 9.5): update type
    // + forL3 in the low half-octet and the GMM CKSN in the high one,
    // old routing area identity (six value octets), MS radio access
    // capabilities (LV, mandatory); any further optional IEs are kept opaque.
    {
        auto o = br.readField(8);
        if (!o) return Expected<L3RoutingAreaUpdateRequest>::error(o.error());
        msg.mCKSN = static_cast<uint8_t>((o.value() >> 4) & 0x07u);
        msg.mForL3 = ((o.value() >> 3) & 0x01u) != 0;
        msg.mUpdateType = static_cast<GMMUpdateType>(o.value() & 0x07u);
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
    // gprsCKSN(3)|spare(1) high, updateType(3)|forL3(1) low (TS 24.068 section 9.5)
    bw.writeField(((mCKSN & 0x07u) << 4) | (mForL3 ? 0x08u : 0u) |
                  (static_cast<uint8_t>(mUpdateType) & 0x07u), 8);
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

// ── L3RoutingAreaUpdateAccept (TS 44.068 section 9.5) ─────────────────

size_t L3RoutingAreaUpdateAccept::bodyLength() const {
    size_t len = 2; // leading part: result/FTS octet + timer IE octet
    len += 6;       // routingAreaId (raw)
    if (mHavePtmsiSignature) len += 4; // P-TMSI signature TV: IEI + three octets
    if (mHavePTMSI) len += tlvLen(mPTMSI.lengthV());
    if (mHaveReadyTimer) len += 3;     // ready timer TV: IEI + two octets
    if (mHavePdpContextStatus) len += 4; // PDP context status TLV: IEI + length + two octets
    len += mAdditionalIes.size();
    return len;
}

Expected<L3RoutingAreaUpdateAccept> L3RoutingAreaUpdateAccept::parse(BitReader& br) {
    L3RoutingAreaUpdateAccept msg;

    // Two-octet leading part (TS 44.068 section 9.5): first octet carries
    // the update result in bits 1-3 and force to standby in bit 0 of the
    // octet value; second octet is the periodic RA update timer (units(3)
    // high|value(5) low, TS 24.008 section 10.5.7.3).
    {
        auto o = br.readField(8);
        if (!o) return Expected<L3RoutingAreaUpdateAccept>::error(o.error());
        msg.mUpdateResult = static_cast<GMMUpdateType>((o.value() >> 1) & 0x07u);
        msg.mForceToStandby = (o.value() & 0x01u) != 0;
    }
    {
        auto o = br.readField(8);
        if (!o) return Expected<L3RoutingAreaUpdateAccept>::error(o.error());
        msg.mRaUpdateTimer.units = static_cast<uint8_t>((o.value() >> 5) & 0x07u);
        msg.mRaUpdateTimer.value = static_cast<uint8_t>(o.value() & 0x1Fu);
    }

    // routingAreaId (raw, 6 octets)
    {
        auto rai = L3RoutingAreaIdentification::parse(br);
        if (!rai) return Expected<L3RoutingAreaUpdateAccept>::error(rai.error());
        msg.mRAI = std::move(rai).value();
    }

    // Optional IEs in any order: P-TMSI signature (TV, element identifier
    // 0x19 + three value octets), allocated P-TMSI (TLV, element identifier
    // 0x18), ready timer (TV, element identifier 0x17 + two value octets)
    // and the PDP context status included by the network (TLV, element
    // identifier 0x32 with length two); any remaining optional information
    // elements are kept opaque and re-emitted verbatim
    // (TS 44.068 section 9.5).
    while (br.hasMore()) {
        uint8_t raw = static_cast<uint8_t>(br.peekField(8));
        uint8_t iei = static_cast<uint8_t>(raw & 0x7Fu);
        if (iei == 0x19u && !msg.mHavePtmsiSignature) {
            auto type = br.readField(8);
            if (!type) return Expected<L3RoutingAreaUpdateAccept>::error(type.error());
            auto sig = br.readBytes(msg.mPtmsiSignature.data(), 3);
            if (!sig) return Expected<L3RoutingAreaUpdateAccept>::error(sig.error());
            msg.mHavePtmsiSignature = true;
        } else if (iei == 0x18u && !msg.mHavePTMSI) {
            auto type = br.readField(8);
            if (!type) return Expected<L3RoutingAreaUpdateAccept>::error(type.error());
            auto len = br.readField(8);
            if (!len) return Expected<L3RoutingAreaUpdateAccept>::error(len.error());
            auto mi = L3MobileIdentity::parse(br, len.value());
            if (!mi) return Expected<L3RoutingAreaUpdateAccept>::error(mi.error());
            msg.mHavePTMSI = true;
            msg.mPTMSI = std::move(mi).value();
        } else if (iei == 0x17u && !msg.mHaveReadyTimer) {
            auto type = br.readField(8);
            if (!type) return Expected<L3RoutingAreaUpdateAccept>::error(type.error());
            auto rt = br.readBytes(msg.mReadyTimer.data(), 2);
            if (!rt) return Expected<L3RoutingAreaUpdateAccept>::error(rt.error());
            msg.mHaveReadyTimer = true;
        } else if (iei == L3PDPContextStatus::IEI && !msg.mHavePdpContextStatus) {
            auto type = br.readField(8);
            if (!type) return Expected<L3RoutingAreaUpdateAccept>::error(type.error());
            auto len = br.readField(8);
            if (!len) return Expected<L3RoutingAreaUpdateAccept>::error(len.error());
            if (len.value() != L3PDPContextStatus::lengthV()) {
                return Expected<L3RoutingAreaUpdateAccept>::error(
                    ParseError{ParseError::Code::InvalidValue, "invalid PDP context status length"});
            }
            auto st = br.readBytes(msg.mPdpContextStatus.data(), 2);
            if (!st) return Expected<L3RoutingAreaUpdateAccept>::error(st.error());
            msg.mHavePdpContextStatus = true;
        } else {
            break; // unknown IE: the remainder is kept opaque
        }
    }
    if (!detail::readOpaqueTail(br, msg.mAdditionalIes)) {
        return Expected<L3RoutingAreaUpdateAccept>::error(
            ParseError{ParseError::Code::TruncatedInput, "truncated optional IEs"});
    }

    return Expected<L3RoutingAreaUpdateAccept>::hold(std::move(msg));
}

void L3RoutingAreaUpdateAccept::write(BitWriter& bw) const {
    // Update result in bits 1-3, force to standby in bit 0 of the octet
    // value (TS 44.068 section 9.5).
    bw.writeField(((static_cast<uint8_t>(mUpdateResult) & 0x07u) << 1) |
                  (mForceToStandby ? 1u : 0u), 8);
    // periodic RA update timer value part (TS 24.008 section 10.5.7.3).
    bw.writeField(mRaUpdateTimer.octet(), 8);
    mRAI.write(bw);
    if (mHavePtmsiSignature) {
        // P-TMSI signature: element identifier followed by the value (TV).
        bw.writeField(0x19u, 8);
        bw.writeBytes(mPtmsiSignature.data(), 3);
    }
    if (mHavePTMSI) {
        // allocated P-TMSI: element identifier, length, value (TLV).
        bw.writeField(0x18u, 8);
        bw.writeField(static_cast<uint32_t>(mPTMSI.lengthV()), 8);
        mPTMSI.write(bw);
    }
    if (mHaveReadyTimer) {
        // ready timer: element identifier followed by the two value octets (TV).
        bw.writeField(0x17u, 8);
        bw.writeBytes(mReadyTimer.data(), 2);
    }
    if (mHavePdpContextStatus) {
        // PDP context status: element identifier, length, two-octet value (TLV).
        bw.writeField(L3PDPContextStatus::IEI, 8);
        bw.writeField(static_cast<uint32_t>(L3PDPContextStatus::lengthV()), 8);
        bw.writeBytes(mPdpContextStatus.data(), 2);
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
    msg.mRaUpdateTimer = m_raUpdateTimer;
    msg.mRAI = m_rai;
    msg.mHavePtmsiSignature = m_havePtmsiSignature;
    msg.mPtmsiSignature = m_ptmsiSignature;
    msg.mHavePTMSI = m_havePTMSI;
    msg.mPTMSI = m_ptmsi;
    msg.mHaveReadyTimer = m_haveReadyTimer;
    msg.mReadyTimer = m_readyTimer;
    msg.mHavePdpContextStatus = m_havePdpContextStatus;
    msg.mPdpContextStatus = m_pdpContextStatus;
    msg.mAdditionalIes = m_additionalIes;
    return msg;
}

L3RoutingAreaUpdateAccept::Builder L3RoutingAreaUpdateAccept::builder() {
    return Builder{};
}

// ── L3RoutingAreaUpdateComplete (GSM 24.008 9.4.16) ──────────────────

Expected<L3RoutingAreaUpdateComplete> L3RoutingAreaUpdateComplete::parse(BitReader& br) {
    L3RoutingAreaUpdateComplete msg;
    // No mandatory body fields; any optional IEs are kept opaque and
    // re-emitted verbatim (TS 44.068 section 9.5).
    if (!detail::readOpaqueTail(br, msg.mAdditionalIes)) {
        return Expected<L3RoutingAreaUpdateComplete>::error(
            ParseError{ParseError::Code::TruncatedInput, "truncated optional IEs"});
    }
    return Expected<L3RoutingAreaUpdateComplete>::hold(std::move(msg));
}

void L3RoutingAreaUpdateComplete::write(BitWriter& bw) const {
    detail::writeOpaqueTail(mAdditionalIes, bw);
}

void L3RoutingAreaUpdateComplete::text(std::ostream& os) const {
    os << "RAUpdateComplete";
}

L3RoutingAreaUpdateComplete L3RoutingAreaUpdateComplete::Builder::build() const {
    L3RoutingAreaUpdateComplete msg;
    msg.mAdditionalIes = m_additionalIes;
    return msg;
}

L3RoutingAreaUpdateComplete::Builder L3RoutingAreaUpdateComplete::builder() {
    return Builder{};
}

// ── L3RoutingAreaUpdateReject (TS 44.068 section 9.5) ─────────────────

size_t L3RoutingAreaUpdateReject::bodyLength() const {
    // gmmCause (one value octet) + force-to-standby octet + opaque optional IEs
    return 2 + mAdditionalIes.size();
}

Expected<L3RoutingAreaUpdateReject> L3RoutingAreaUpdateReject::parse(BitReader& br) {
    L3RoutingAreaUpdateReject msg;

    // The body starts with the GMM cause as a single value octet carried
    // without an identifier (TS 44.068 section 9.5).
    auto r = br.readField(8);
    if (!r) return Expected<L3RoutingAreaUpdateReject>::error(r.error());
    msg.mCause = static_cast<GMMCause>(r.value());

    // Second octet, always present: forceToStandby in bit 0 of the octet
    // value, the remaining bits spare (TS 44.068 section 9.5).
    auto o = br.readField(8);
    if (!o) return Expected<L3RoutingAreaUpdateReject>::error(o.error());
    msg.mForceToStandby = (o.value() & 0x01u) != 0;

    // Unrecognized optional IEs are kept opaque and re-emitted verbatim.
    if (!detail::readOpaqueTail(br, msg.mAdditionalIes)) {
        return Expected<L3RoutingAreaUpdateReject>::error(
            ParseError{ParseError::Code::TruncatedInput, "truncated optional IEs"});
    }

    return Expected<L3RoutingAreaUpdateReject>::hold(std::move(msg));
}

void L3RoutingAreaUpdateReject::write(BitWriter& bw) const {
    bw.writeField(static_cast<uint8_t>(mCause), 8);
    // Spare high bits are zero; the force-to-standby indicator is bit 0.
    bw.writeField(mForceToStandby ? 1u : 0u, 8);
    detail::writeOpaqueTail(mAdditionalIes, bw);
}

void L3RoutingAreaUpdateReject::text(std::ostream& os) const {
    os << "RAUpdateReject(cause=" << GMMCause2Str(mCause) << ")";
}

L3RoutingAreaUpdateReject L3RoutingAreaUpdateReject::Builder::build() const {
    L3RoutingAreaUpdateReject msg;
    msg.mCause = m_cause;
    msg.mForceToStandby = m_forceToStandby;
    msg.mAdditionalIes = m_additionalIes;
    return msg;
}

L3RoutingAreaUpdateReject::Builder L3RoutingAreaUpdateReject::builder() {
    return Builder{};
}

// ── L3ServiceRequest (TS 44.068 section 9.5) ──────────────────────────

size_t L3ServiceRequest::bodyLength() const {
    size_t len = 1; // gprsCKSN|serviceType
    len += lvLen(mPTMSI.lengthV());
    len += mAdditionalIes.size();
    return len;
}

Expected<L3ServiceRequest> L3ServiceRequest::parse(BitReader& br) {
    L3ServiceRequest msg;

    // gprsCKSN(3)|spare(1) in the low half-octet, serviceType(3)|spare(1)
    // in the high one (TS 44.068 section 9.5).
    {
        auto o = br.readField(8);
        if (!o) return Expected<L3ServiceRequest>::error(o.error());
        msg.mCKSN = o.value() & 0x07u;
        msg.mServiceType = (o.value() >> 4) & 0x07u;
    }

    // PTMSI (LV)
    {
        auto mi = parseLVMI(br);
        if (!mi) return Expected<L3ServiceRequest>::error(mi.error());
        msg.mPTMSI = std::move(mi).value();
    }

    // Remaining optional IEs (conforming MSes append the PDP context status
    // TLV 0x32 here) are kept opaque and re-emitted verbatim
    // (TS 44.068 section 9.5).
    if (!detail::readOpaqueTail(br, msg.mAdditionalIes)) {
        return Expected<L3ServiceRequest>::error(
            ParseError{ParseError::Code::TruncatedInput, "truncated optional IEs"});
    }

    return Expected<L3ServiceRequest>::hold(std::move(msg));
}

void L3ServiceRequest::write(BitWriter& bw) const {
    bw.writeField(((mServiceType & 0x07u) << 4) | (mCKSN & 0x07u), 8);
    writeLVMI(mPTMSI, bw);
    detail::writeOpaqueTail(mAdditionalIes, bw);
}

void L3ServiceRequest::text(std::ostream& os) const {
    os << "ServiceRequest(CKSN=" << mCKSN << ",type=" << mServiceType << ")";
}

L3ServiceRequest L3ServiceRequest::Builder::build() const {
    L3ServiceRequest msg;
    msg.mCKSN = static_cast<uint8_t>(mCKSN & 0x07u);
    msg.mServiceType = static_cast<uint8_t>(m_serviceType & 0x07u);
    msg.mPTMSI = m_ptmsi;
    msg.mAdditionalIes = m_additionalIes;
    return msg;
}

L3ServiceRequest::Builder L3ServiceRequest::builder() {
    return Builder{};
}

// ── L3ServiceAccept (TS 44.068 section 9.5) ───────────────────────────

size_t L3ServiceAccept::bodyLength() const {
    size_t len = 0;
    if (mHavePdpContextStatus) len += tlvLen(mPdpContextStatus.lengthV());
    len += mAdditionalIes.size();
    return len;
}

Expected<L3ServiceAccept> L3ServiceAccept::parse(BitReader& br) {
    L3ServiceAccept msg;

    // Optional PDP context status (TLV, element identifier 0x32 + length +
    // two-octet bitmap), then any remaining optional information elements
    // kept opaque and re-emitted verbatim (TS 44.068 section 9.5).
    if (br.hasMore()) {
        uint8_t raw = static_cast<uint8_t>(br.peekField(8));
        if ((raw & 0x7Fu) == L3PDPContextStatus::IEI) {
            auto type = br.readField(8);
            if (!type) return Expected<L3ServiceAccept>::error(type.error());
            auto len = br.readField(8);
            if (!len) return Expected<L3ServiceAccept>::error(len.error());
            if (len.value() == L3PDPContextStatus::lengthV()) {
                auto status = L3PDPContextStatus::parse(br);
                if (!status) return Expected<L3ServiceAccept>::error(status.error());
                msg.mHavePdpContextStatus = true;
                msg.mPdpContextStatus = std::move(status).value();
            } else {
                // Non-standard value length: keep the TLV opaque.
                msg.mAdditionalIes.push_back(static_cast<uint8_t>(type.value()));
                msg.mAdditionalIes.push_back(static_cast<uint8_t>(len.value()));
            }
        }
    }
    if (!detail::readOpaqueTail(br, msg.mAdditionalIes)) {
        return Expected<L3ServiceAccept>::error(
            ParseError{ParseError::Code::TruncatedInput, "truncated optional IEs"});
    }

    return Expected<L3ServiceAccept>::hold(std::move(msg));
}

void L3ServiceAccept::write(BitWriter& bw) const {
    if (mHavePdpContextStatus) {
        // PDP context status: element identifier, length, value (TLV).
        bw.writeField(L3PDPContextStatus::IEI, 8);
        bw.writeField(static_cast<uint32_t>(mPdpContextStatus.lengthV()), 8);
        mPdpContextStatus.write(bw);
    }
    detail::writeOpaqueTail(mAdditionalIes, bw);
}

void L3ServiceAccept::text(std::ostream& os) const {
    os << "ServiceAccept";
}

L3ServiceAccept L3ServiceAccept::Builder::build() const {
    L3ServiceAccept msg;
    msg.mHavePdpContextStatus = m_havePdpContextStatus;
    msg.mPdpContextStatus = m_pdpContextStatus;
    msg.mAdditionalIes = m_additionalIes;
    return msg;
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
    size_t len = lvLen(mPTMSI.lengthV()); // allocated P-TMSI (positional LV, first)
    len += 6;                            // routingAreaId (raw)
    len += 1;                            // force-to-standby octet
    if (mHavePtmsiSignature) len += 4;   // P-TMSI signature TV: IEI + three octets
    len += mAdditionalIes.size();
    return len;
}

Expected<L3P_TMSIReallocationCommand> L3P_TMSIReallocationCommand::parse(BitReader& br) {
    L3P_TMSIReallocationCommand msg;

    // The body starts with the allocated P-TMSI as a positional LV in the
    // TMSI mobile identity form: length five, first value octet 0xF4
    // followed by four big-endian octets (TS 44.068 section 9.5).
    {
        auto len = br.readField(8);
        if (!len) return Expected<L3P_TMSIReallocationCommand>::error(len.error());
        size_t vLen = len.value();
        if (vLen * 8 > br.remainingBits()) {
            return Expected<L3P_TMSIReallocationCommand>::error(
                ParseError{ParseError::Code::TruncatedInput, "truncated P-TMSI value", br.position() - 8});
        }
        auto mi = L3MobileIdentity::parse(br, vLen);
        if (!mi) return Expected<L3P_TMSIReallocationCommand>::error(mi.error());
        if (!mi.value().isTMSI()) {
            return Expected<L3P_TMSIReallocationCommand>::error(
                ParseError{ParseError::Code::InvalidValue, "allocated P-TMSI must be in the TMSI form"});
        }
        msg.mPTMSI = std::move(mi).value();
    }

    // routingAreaId (raw, 6 octets)
    {
        auto rai = L3RoutingAreaIdentification::parse(br);
        if (!rai) return Expected<L3P_TMSIReallocationCommand>::error(rai.error());
        msg.mRAI = std::move(rai).value();
    }

    // Force-to-standby octet: bit 0 of the octet value, the remaining bits
    // spare (TS 44.068 section 9.5).
    {
        auto o = br.readField(8);
        if (!o) return Expected<L3P_TMSIReallocationCommand>::error(o.error());
        msg.mForceToStandby = (o.value() & 0x01u) != 0;
    }

    // Optional P-TMSI signature (TV, element identifier 0x19 + three value
    // octets), then any remaining optional information elements kept opaque
    // and re-emitted verbatim (TS 44.068 section 9.5).
    if (br.hasMore()) {
        uint8_t raw = static_cast<uint8_t>(br.peekField(8));
        if ((raw & 0x7Fu) == 0x19u) {
            auto type = br.readField(8);
            if (!type) return Expected<L3P_TMSIReallocationCommand>::error(type.error());
            auto sig = br.readBytes(msg.mPtmsiSignature.data(), 3);
            if (!sig) return Expected<L3P_TMSIReallocationCommand>::error(sig.error());
            msg.mHavePtmsiSignature = true;
        }
    }
    if (!detail::readOpaqueTail(br, msg.mAdditionalIes)) {
        return Expected<L3P_TMSIReallocationCommand>::error(
            ParseError{ParseError::Code::TruncatedInput, "truncated optional IEs"});
    }

    return Expected<L3P_TMSIReallocationCommand>::hold(std::move(msg));
}

void L3P_TMSIReallocationCommand::write(BitWriter& bw) const {
    // allocated P-TMSI: positional LV in the TMSI mobile identity form.
    writeLVMI(mPTMSI, bw);
    mRAI.write(bw);
    // Spare high bits are zero; the force-to-standby indicator is bit 0.
    bw.writeField(mForceToStandby ? 1u : 0u, 8);
    if (mHavePtmsiSignature) {
        // P-TMSI signature: element identifier followed by the value (TV).
        bw.writeField(0x19u, 8);
        bw.writeBytes(mPtmsiSignature.data(), 3);
    }
    detail::writeOpaqueTail(mAdditionalIes, bw);
}

void L3P_TMSIReallocationCommand::text(std::ostream& os) const {
    os << "P_TMSIRreallocCmd(forceStandby=" << mForceToStandby << ")";
}

L3P_TMSIReallocationCommand L3P_TMSIReallocationCommand::Builder::build() const {
    L3P_TMSIReallocationCommand msg;
    msg.mPTMSI = m_ptmsi;
    msg.mRAI = m_rai;
    msg.mForceToStandby = m_forceToStandby;
    msg.mHavePtmsiSignature = m_havePtmsiSignature;
    msg.mPtmsiSignature = m_ptmsiSignature;
    msg.mAdditionalIes = m_additionalIes;
    return msg;
}

L3P_TMSIReallocationCommand::Builder L3P_TMSIReallocationCommand::builder() {
    return Builder{};
}

// ── L3P_TMSIReallocationComplete (GSM 24.008 9.4.8) ──────────────────

Expected<L3P_TMSIReallocationComplete> L3P_TMSIReallocationComplete::parse(BitReader& br) {
    L3P_TMSIReallocationComplete msg;
    // No mandatory body fields; any optional IEs are kept opaque and
    // re-emitted verbatim (TS 44.068 section 9.5).
    if (!detail::readOpaqueTail(br, msg.mAdditionalIes)) {
        return Expected<L3P_TMSIReallocationComplete>::error(
            ParseError{ParseError::Code::TruncatedInput, "truncated optional IEs"});
    }
    return Expected<L3P_TMSIReallocationComplete>::hold(std::move(msg));
}

void L3P_TMSIReallocationComplete::write(BitWriter& bw) const {
    detail::writeOpaqueTail(mAdditionalIes, bw);
}

void L3P_TMSIReallocationComplete::text(std::ostream& os) const {
    os << "P_TMSIRreallocComplete";
}

L3P_TMSIReallocationComplete L3P_TMSIReallocationComplete::Builder::build() const {
    L3P_TMSIReallocationComplete msg;
    msg.mAdditionalIes = m_additionalIes;
    return msg;
}

L3P_TMSIReallocationComplete::Builder L3P_TMSIReallocationComplete::builder() {
    return Builder{};
}

// ── L3AuthenticationAndCipheringRequest (TS 44.068 section 9.5) ───────

size_t L3AuthenticationAndCipheringRequest::bodyLength() const {
    // Ciphering algorithm / IMEISV request / force-to-standby octet, the AC
    // reference number octet, then the TV-formatted RAND (identifier + 16
    // value octets) and the opaque optional IEs.
    return 2 + tvLen(16) + mAdditionalIes.size();
}

Expected<L3AuthenticationAndCipheringRequest> L3AuthenticationAndCipheringRequest::parse(BitReader& br) {
    L3AuthenticationAndCipheringRequest msg;

    // cipheringAlgorithm(3)|spare(1)|imeisvRequest(1)|forceToStandby(1)|spare(2),
    // the algorithm in the low three bits of the octet value (TS 44.068 section 9.5).
    {
        auto o = br.readField(8);
        if (!o) return Expected<L3AuthenticationAndCipheringRequest>::error(o.error());
        msg.mCipheringAlgorithm = o.value() & 0x07u;
        msg.mImeisvRequest = ((o.value() >> 4) & 0x01u) != 0;
        msg.mForceToStandby = ((o.value() >> 5) & 0x01u) != 0;
    }

    // acReferenceNumber(4)|spare(4), the reference number in the low
    // half-octet.
    {
        auto o = br.readField(8);
        if (!o) return Expected<L3AuthenticationAndCipheringRequest>::error(o.error());
        msg.mACReferenceNumber = o.value() & 0x0Fu;
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

    // Any further optional information elements are kept opaque and
    // re-emitted verbatim (TS 44.068 section 9.5).
    if (!detail::readOpaqueTail(br, msg.mAdditionalIes)) {
        return Expected<L3AuthenticationAndCipheringRequest>::error(
            ParseError{ParseError::Code::TruncatedInput, "truncated optional IEs"});
    }

    return Expected<L3AuthenticationAndCipheringRequest>::hold(std::move(msg));
}

void L3AuthenticationAndCipheringRequest::write(BitWriter& bw) const {
    bw.writeField((mCipheringAlgorithm & 0x07u) | (mImeisvRequest ? 0x10u : 0u) |
                  (mForceToStandby ? 0x20u : 0u), 8);
    bw.writeField(mACReferenceNumber & 0x0Fu, 8);
    // authenticationParameterRAND: identifier octet followed by the value (TV).
    bw.writeField(L3AuthRAND::IEI, 8);
    mRAND.write(bw);
    detail::writeOpaqueTail(mAdditionalIes, bw);
}

void L3AuthenticationAndCipheringRequest::text(std::ostream& os) const {
    os << "AuthCipherReq(alg=" << mCipheringAlgorithm << ",acRef=" << mACReferenceNumber << ")";
}

L3AuthenticationAndCipheringRequest L3AuthenticationAndCipheringRequest::Builder::build() const {
    L3AuthenticationAndCipheringRequest msg;
    msg.mCipheringAlgorithm = static_cast<uint8_t>(m_cipheringAlgorithm & 0x07u);
    msg.mImeisvRequest = m_imeisvRequest;
    msg.mForceToStandby = m_forceToStandby;
    msg.mACReferenceNumber = static_cast<uint8_t>(m_acReferenceNumber & 0x0Fu);
    msg.mRAND = m_rand;
    msg.mAdditionalIes = m_additionalIes;
    return msg;
}

L3AuthenticationAndCipheringRequest::Builder L3AuthenticationAndCipheringRequest::builder() {
    return Builder{};
}

// ── L3AuthenticationAndCipheringResponse (TS 44.068 section 9.5) ──────

size_t L3AuthenticationAndCipheringResponse::bodyLength() const {
    // AC reference number octet, then the TV-formatted RES (identifier + four
    // value octets), the optional IMEISV TLV and the opaque optional IEs.
    size_t len = 1 + tvLen(4);
    if (mHaveImeisv) len += tlvLen(mImeisv.lengthV());
    len += mAdditionalIes.size();
    return len;
}

Expected<L3AuthenticationAndCipheringResponse> L3AuthenticationAndCipheringResponse::parse(BitReader& br) {
    L3AuthenticationAndCipheringResponse msg;

    // acReferenceNumber(4)|spare(4), the reference number in the low
    // half-octet (TS 44.068 section 9.5).
    {
        auto o = br.readField(8);
        if (!o) return Expected<L3AuthenticationAndCipheringResponse>::error(o.error());
        msg.mACReferenceNumber = o.value() & 0x0Fu;
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

    // Optional IMEISV (TLV, element identifier 0x23), then any remaining
    // optional information elements kept opaque and re-emitted verbatim
    // (TS 44.068 section 9.5).
    if (br.hasMore()) {
        uint8_t raw = static_cast<uint8_t>(br.peekField(8));
        if ((raw & 0x7Fu) == 0x23u) {
            auto type = br.readField(8);
            if (!type) return Expected<L3AuthenticationAndCipheringResponse>::error(type.error());
            auto len = br.readField(8);
            if (!len) return Expected<L3AuthenticationAndCipheringResponse>::error(len.error());
            auto mi = L3MobileIdentity::parse(br, len.value());
            if (!mi) return Expected<L3AuthenticationAndCipheringResponse>::error(mi.error());
            msg.mHaveImeisv = true;
            msg.mImeisv = std::move(mi).value();
        }
    }
    if (!detail::readOpaqueTail(br, msg.mAdditionalIes)) {
        return Expected<L3AuthenticationAndCipheringResponse>::error(
            ParseError{ParseError::Code::TruncatedInput, "truncated optional IEs"});
    }

    return Expected<L3AuthenticationAndCipheringResponse>::hold(std::move(msg));
}

void L3AuthenticationAndCipheringResponse::write(BitWriter& bw) const {
    bw.writeField(mACReferenceNumber & 0x0Fu, 8);
    // authenticationParameterResponse: identifier octet followed by the value (TV).
    bw.writeField(L3AuthRES::IEI, 8);
    mRES.write(bw);
    if (mHaveImeisv) {
        // IMEISV: element identifier, length, value (TLV).
        bw.writeField(0x23u, 8);
        bw.writeField(static_cast<uint32_t>(mImeisv.lengthV()), 8);
        mImeisv.write(bw);
    }
    detail::writeOpaqueTail(mAdditionalIes, bw);
}

void L3AuthenticationAndCipheringResponse::text(std::ostream& os) const {
    os << "AuthCipherResp(acRef=" << mACReferenceNumber << ")";
}

L3AuthenticationAndCipheringResponse L3AuthenticationAndCipheringResponse::Builder::build() const {
    L3AuthenticationAndCipheringResponse msg;
    msg.mACReferenceNumber = static_cast<uint8_t>(m_acReferenceNumber & 0x0Fu);
    msg.mRES = m_res;
    msg.mHaveImeisv = m_haveImeisv;
    msg.mImeisv = m_imeisv;
    msg.mAdditionalIes = m_additionalIes;
    return msg;
}

L3AuthenticationAndCipheringResponse::Builder L3AuthenticationAndCipheringResponse::builder() {
    return Builder{};
}

// ── L3AuthenticationAndCipheringReject (GSM 24.008 9.4.9) ─────────────

Expected<L3AuthenticationAndCipheringReject> L3AuthenticationAndCipheringReject::parse(BitReader& br) {
    L3AuthenticationAndCipheringReject msg;
    // No mandatory body fields; any optional IEs are kept opaque and
    // re-emitted verbatim (TS 44.068 section 9.5).
    if (!detail::readOpaqueTail(br, msg.mAdditionalIes)) {
        return Expected<L3AuthenticationAndCipheringReject>::error(
            ParseError{ParseError::Code::TruncatedInput, "truncated optional IEs"});
    }
    return Expected<L3AuthenticationAndCipheringReject>::hold(std::move(msg));
}

void L3AuthenticationAndCipheringReject::write(BitWriter& bw) const {
    detail::writeOpaqueTail(mAdditionalIes, bw);
}

void L3AuthenticationAndCipheringReject::text(std::ostream& os) const {
    os << "AuthCipherReject";
}

L3AuthenticationAndCipheringReject L3AuthenticationAndCipheringReject::Builder::build() const {
    L3AuthenticationAndCipheringReject msg;
    msg.mAdditionalIes = m_additionalIes;
    return msg;
}

L3AuthenticationAndCipheringReject::Builder L3AuthenticationAndCipheringReject::builder() {
    return Builder{};
}

// ── L3GMMIdentityRequest (TS 44.068 section 9.5) ──────────────────────

Expected<L3GMMIdentityRequest> L3GMMIdentityRequest::parse(BitReader& br) {
    L3GMMIdentityRequest msg;

    // One-octet body: forceToStandby(1) in bit 4, identityType(3) in the
    // low three bits of the octet value; bit 3 is spare (TS 44.068 section 9.5).
    auto o = br.readField(8);
    if (!o) return Expected<L3GMMIdentityRequest>::error(o.error());
    if ((o.value() & 0x08u) != 0) {
        return Expected<L3GMMIdentityRequest>::error(
            ParseError{ParseError::Code::InvalidValue, "non-zero spare bit in the identity request octet"});
    }
    msg.mForceToStandby = ((o.value() >> 4) & 0x01u) != 0;
    msg.mIdentityType = static_cast<MobileIDType>(o.value() & 0x07u);

    return Expected<L3GMMIdentityRequest>::hold(std::move(msg));
}

void L3GMMIdentityRequest::write(BitWriter& bw) const {
    bw.writeField(((mForceToStandby ? 1u : 0u) << 4) |
                  (static_cast<uint8_t>(mIdentityType) & 0x07u), 8);
}

void L3GMMIdentityRequest::text(std::ostream& os) const {
    os << "GMMIdentityReq(type=" << mIdentityType
       << ",forceStandby=" << mForceToStandby << ")";
}

L3GMMIdentityRequest L3GMMIdentityRequest::Builder::build() const {
    L3GMMIdentityRequest msg;
    msg.mIdentityType = static_cast<MobileIDType>(static_cast<uint8_t>(m_identityType) & 0x07u);
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

Expected<L3GMMInformation> L3GMMInformation::parse(BitReader& br) {
    L3GMMInformation msg;
    // The whole body (network information) is kept opaque and re-emitted
    // verbatim (TS 44.068 section 9.5).
    if (!detail::readOpaqueTail(br, msg.mAdditionalIes)) {
        return Expected<L3GMMInformation>::error(
            ParseError{ParseError::Code::TruncatedInput, "truncated optional IEs"});
    }
    return Expected<L3GMMInformation>::hold(std::move(msg));
}

void L3GMMInformation::write(BitWriter& bw) const {
    detail::writeOpaqueTail(mAdditionalIes, bw);
}

void L3GMMInformation::text(std::ostream& os) const {
    os << "GMMInformation";
}

L3GMMInformation L3GMMInformation::Builder::build() const {
    L3GMMInformation msg;
    msg.mAdditionalIes = m_additionalIes;
    return msg;
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
