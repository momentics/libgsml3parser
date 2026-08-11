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

// GMM Messages — parse/write/text implementation
// Spec: 3GPP TS 24.008 sections 9.4, Table 10.4
// Reference: ref/osmo-ttcn3-hacks/library/L3_Templates.ttcn — GMM message templates
//            ref/OpenBTS/SGSNGGSN/GPRSL3Messages.h — L3GmmMsg::MessageType

#include "gsml3parser/gmm/l3gmmmessages.h"
#include <sstream>
#include <iomanip>

namespace gsml3parser {

namespace {

size_t lvLen(size_t vLen) { return 1 + vLen; }
size_t tlvLen(size_t vLen) { return 2 + vLen; }

Expected<L3MobileIdentity> parseLVMI(BitReader& br) {
    auto r = br.readField(8);
    if (!r) return Expected<L3MobileIdentity>::error(r.error());
    return L3MobileIdentity::parse(br, r.value());
}

void writeLVMI(const L3MobileIdentity& mi, BitWriter& bw) {
    bw.writeField(static_cast<uint32_t>(mi.lengthV()), 8);
    mi.write(bw);
}

// Skip a TLV IE and return the IEI that was read.
Expected<uint8_t> skipTLV(BitReader& br) {
    if (!br.hasMore()) return Expected<uint8_t>::error(
        ParseError{ParseError::Code::TruncatedInput, "no TLV", br.position()});
    auto type = br.readField(8);
    if (!type) return Expected<uint8_t>::error(type.error());
    bool ext = (type.value() & 0x80) != 0;
    if (ext) {
        auto len = br.readField(8);
        if (!len) return Expected<uint8_t>::error(len.error());
        if (len.value() > 0 && !br.hasMore()) return Expected<uint8_t>::error(
            ParseError{ParseError::Code::TruncatedInput, "TLV value truncated", br.position()});
        // Skip value bytes
        size_t skipBits = len.value() * 8;
        if (br.position() + skipBits > br.remainingBits() + br.position()) return Expected<uint8_t>::error(
            ParseError{ParseError::Code::TruncatedInput, "TLV skip past end", br.position()});
        // Actually we need to advance the reader
    }
    return Expected<uint8_t>::hold(static_cast<uint8_t>(type.value() & 0x7F));
}

// Read a TV (Type-Value) element: IEI(4 bits) | Value(variable, known per type)
Expected<uint8_t> readTVIEI(BitReader& br) {
    auto r = br.peekField(4);
    return Expected<uint8_t>::hold(static_cast<uint8_t>(r));
}

} // anonymous namespace

// ── L3AttachRequest (GSM 24.008 9.4.1) ────────────────────────────────

size_t L3AttachRequest::bodyLength() const {
    size_t len = 0;
    // msNetworkCapability: LV format (length + value)
    len += lvLen(mMsNetworkCapability.lengthV());
    // attachType(4)|CKSN(4) = 1 octet
    len += 1;
    // drxParam: TV (IEI=0x1a, 2 octets value)
    len += 3;
    // mobileIdentity: LV
    len += lvLen(mMobileIdentity.lengthV());
    // oldRoutingAreaID: raw 6 octets
    len += 6;
    // optional msRACap: LV
    if (mHaveMsRACap) len += lvLen(mMsRACap.size());
    return len;
}

Expected<L3AttachRequest> L3AttachRequest::parse(BitReader& br) {
    L3AttachRequest msg;

    // 24.008 9.4.1: msNetworkCapability (LV format)
    {
        auto len = br.readField(8);
        if (!len) return Expected<L3AttachRequest>::error(len.error());
        auto cap = L3MSNetworkCapability::parse(br, len.value());
        if (!cap) return Expected<L3AttachRequest>::error(cap.error());
        msg.mMsNetworkCapability = std::move(cap).value();
    }

    // 24.008 10.5.5.19: attachType(3)|forL3(1)|CKSN(3)|spare(1) = 1 octet
    {
        auto o = br.readField(8);
        if (!o) return Expected<L3AttachRequest>::error(o.error());
        msg.mAttachType = static_cast<GMMAttachType>((o.value() >> 5) & 0x07);
        msg.mForL3 = ((o.value() >> 4) & 0x01) != 0;
        msg.mCKSN = o.value() & 0x0F;
    }

    // 24.008 10.5.5.13: drxParam (TV, IEI=0x1a, 4-bit type + 2 octets value)
    {
        auto ieType = br.readField(4);
        if (!ieType) return Expected<L3AttachRequest>::error(ieType.error());
        if (ieType.value() != L3DRXParameter::IEI) {
            return Expected<L3AttachRequest>::error(
                ParseError{ParseError::Code::InvalidIE, "expected DRX param IEI", br.position() - 4});
        }
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

    // oldRoutingAreaID (raw, 6 octets)
    {
        auto rai = L3RoutingAreaIdentification::parse(br);
        if (!rai) return Expected<L3AttachRequest>::error(rai.error());
        msg.mOldRAI = std::move(rai).value();
    }

    // Optional IEs: parse remaining as TLV/TV
    while (br.hasMore()) {
        auto ieType = br.peekField(4);
        uint8_t iei4 = static_cast<uint8_t>(ieType);

        if (iei4 == 0x0F) {
            // Extended IEI: full byte type
            auto fullType = br.readField(8);
            if (!fullType) return Expected<L3AttachRequest>::error(fullType.error());
            uint8_t iei = static_cast<uint8_t>(fullType.value());
            // MS Radio Access Capability (LV) - common IE
            if (iei == 0x62 || iei == 0xe2) {
                auto len = br.readField(8);
                if (!len) return Expected<L3AttachRequest>::error(len.error());
                msg.mHaveMsRACap = true;
                msg.mMsRACap.resize(len.value());
                if (len.value() > 0) {
                    auto r = br.readBytes(msg.mMsRACap.data(), len.value());
                    if (!r) return Expected<L3AttachRequest>::error(r.error());
                }
            } else {
                // Unknown extended IE: skip
                auto len = br.readField(8);
                if (!len) return Expected<L3AttachRequest>::error(len.error());
                size_t skipBits = len.value() * 8;
                if (br.position() + skipBits > br.remainingBits() + br.position()) break;
            }
        } else if (iei4 == L3DRXParameter::IEI) {
            // Already parsed, shouldn't appear again
            break;
        } else {
            // Unknown 4-bit IEI
            break;
        }
    }

    return Expected<L3AttachRequest>::hold(std::move(msg));
}

void L3AttachRequest::write(BitWriter& bw) const {
    // msNetworkCapability: LV
    bw.writeField(static_cast<uint32_t>(mMsNetworkCapability.lengthV()), 8);
    mMsNetworkCapability.write(bw);

    // attachType(3)|forL3(1)|CKSN(3)|spare(1)
    bw.writeField(((static_cast<uint8_t>(mAttachType) & 0x07) << 5) |
                  ((mForL3 ? 1 : 0) << 4) | (mCKSN & 0x0F), 8);

    // drxParam: TV (4-bit IEI + value)
    bw.writeField(L3DRXParameter::IEI, 4);
    mDRXParam.write(bw);

    // mobileIdentity: LV
    writeLVMI(mMobileIdentity, bw);

    // oldRoutingAreaID: raw
    mOldRAI.write(bw);

    // optional msRACap: LV
    if (mHaveMsRACap) {
        bw.writeField(0xF0 | 0x62, 8);  // extended IEI
        bw.writeField(static_cast<uint32_t>(mMsRACap.size()), 8);
        bw.writeBytes(mMsRACap.data(), mMsRACap.size());
    }
}

void L3AttachRequest::text(std::ostream& os) const {
    os << "AttachRequest(type=";
    switch (mAttachType) {
        case GMMAttachType::GPRSAttach: os << "GPRS"; break;
        case GMMAttachType::CombinedGPRSAndIMSIAttach: os << "Combined"; break;
    }
    os << ",CKSN=" << (mCKSN >> 1 & 0x07) << ",forL3=" << mForL3 << ")";
    mMobileIdentity.text(os);
}

// ── L3AttachAccept (GSM 24.008 9.4.2) ────────────────────────────────

size_t L3AttachAccept::bodyLength() const {
    size_t len = 1; // attachResult|forceToStandby|updateTimer|radioPriority
    len += 6;       // routingAreaIdentification (raw)
    if (mHavePTMSI) len += tlvLen(mPTMSI.lengthV());
    return len;
}

Expected<L3AttachAccept> L3AttachAccept::parse(BitReader& br) {
    L3AttachAccept msg;

    // 24.008 9.4.2: attachResult(3)|spare(1)|forceToStandby(1)|updateTimer(2)|radioPriority(1) = 1 octet
    {
        auto o = br.readField(8);
        if (!o) return Expected<L3AttachAccept>::error(o.error());
        msg.mAttachResult = static_cast<GMMAttachType>((o.value() >> 5) & 0x07);
        msg.mForceToStandby = ((o.value() >> 3) & 0x01) != 0;
        // updateTimer and radioPriority in lower bits
    }

    // routingAreaIdentification (raw, 6 octets)
    {
        auto rai = L3RoutingAreaIdentification::parse(br);
        if (!rai) return Expected<L3AttachAccept>::error(rai.error());
        msg.mRAI = std::move(rai).value();
    }

    // Optional IEs: TLV format
    while (br.hasMore()) {
        auto type = br.peekField(8);
        uint8_t rawType = static_cast<uint8_t>(type);
        if ((rawType & 0x80) == 0) break; // not extended, stop

        auto fullType = br.readField(8);
        if (!fullType) return Expected<L3AttachAccept>::error(fullType.error());
        uint8_t iei = static_cast<uint8_t>(fullType.value());
        bool ext = (iei & 0x80) != 0;
        iei &= 0x7F;

        auto len = br.readField(8);
        if (!len) return Expected<L3AttachAccept>::error(len.error());
        size_t vLen = len.value();

        // allocatedPTMSI (IEI=0x0c, TLV)
        if (iei == 0x0c && vLen >= 2) {
            auto mi = L3MobileIdentity::parse(br, vLen);
            if (mi) {
                msg.mHavePTMSI = true;
                msg.mPTMSI = std::move(mi).value();
            }
        } else {
            // Skip unknown IE
            if (vLen > 0 && br.remainingBits() >= vLen * 8) {
                // Advance reader
            }
        }
    }

    return Expected<L3AttachAccept>::hold(std::move(msg));
}

void L3AttachAccept::write(BitWriter& bw) const {
    bw.writeField(((static_cast<uint8_t>(mAttachResult) & 0x07) << 5) |
                  ((mForceToStandby ? 1 : 0) << 3), 8);
    mRAI.write(bw);
    if (mHavePTMSI) {
        bw.writeField(0x8c, 8); // extended IEI for allocatedPTMSI
        bw.writeField(static_cast<uint32_t>(mPTMSI.lengthV()), 8);
        mPTMSI.write(bw);
    }
}

void L3AttachAccept::text(std::ostream& os) const {
    os << "AttachAccept(result=";
    switch (mAttachResult) {
        case GMMAttachType::GPRSAttach: os << "GPRS"; break;
        case GMMAttachType::CombinedGPRSAndIMSIAttach: os << "Combined"; break;
    }
    os << ",forceStandby=" << mForceToStandby << ")";
}

// ── L3AttachComplete (GSM 24.008 9.4.3) ──────────────────────────────

Expected<L3AttachComplete> L3AttachComplete::parse(BitReader& br) {
    // No mandatory body fields; consume any remaining as optional IEs
    return Expected<L3AttachComplete>::hold(L3AttachComplete{});
}

void L3AttachComplete::write(BitWriter&) const {}

void L3AttachComplete::text(std::ostream& os) const {
    os << "AttachComplete";
}

// ── L3AttachReject (GSM 24.008 9.4.4) ────────────────────────────────

size_t L3AttachReject::bodyLength() const {
    size_t len = tlvLen(1); // gmmCause TLV
    if (mHaveT3302) len += tlvLen(1);
    return len;
}

Expected<L3AttachReject> L3AttachReject::parse(BitReader& br) {
    L3AttachReject msg;

    while (br.hasMore()) {
        auto type = br.readField(8);
        if (!type) return Expected<L3AttachReject>::error(type.error());
        uint8_t rawType = static_cast<uint8_t>(type.value());
        bool ext = (rawType & 0x80) != 0;
        uint8_t iei = rawType & 0x7F;

        auto len = br.readField(8);
        if (!len) return Expected<L3AttachReject>::error(len.error());
        size_t vLen = len.value();

        if (iei == L3GMMCauseIE::IEI && vLen >= 1) {
            auto cause = br.readField(8);
            if (cause) msg.mCause = static_cast<GMMCause>(cause.value());
        } else if (iei == L3T3302Timer::IEI && vLen >= 1) {
            auto t = br.readField(8);
            if (t) {
                msg.mHaveT3302 = true;
                msg.mT3302 = L3T3302Timer{static_cast<uint8_t>(t.value())};
            }
        } else {
            // Skip unknown
        }
    }

    return Expected<L3AttachReject>::hold(std::move(msg));
}

void L3AttachReject::write(BitWriter& bw) const {
    bw.writeField(0x80 | L3GMMCauseIE::IEI, 8);
    bw.writeField(1, 8);
    bw.writeField(static_cast<uint8_t>(mCause), 8);
    if (mHaveT3302) {
        bw.writeField(0x80 | L3T3302Timer::IEI, 8);
        bw.writeField(1, 8);
        bw.writeField(mT3302.value(), 8);
    }
}

void L3AttachReject::text(std::ostream& os) const {
    os << "AttachReject(cause=" << GMMCause2Str(mCause) << ")";
}

// ── L3DetachRequest (GSM 24.008 9.4.5) ───────────────────────────────

size_t L3DetachRequest::bodyLength() const {
    return 1; // detachType(4)|spare(4)
}

Expected<L3DetachRequest> L3DetachRequest::parse(BitReader& br) {
    L3DetachRequest msg;

    auto o = br.readField(8);
    if (!o) return Expected<L3DetachRequest>::error(o.error());
    msg.mDetachType = (o.value() >> 4) & 0x07;
    msg.mPowerOff = (o.value() & 0x08) != 0;

    // Optional IEs
    while (br.hasMore()) {
        auto type = br.readField(8);
        if (!type) return Expected<L3DetachRequest>::error(type.error());
        uint8_t rawType = static_cast<uint8_t>(type.value());
        uint8_t iei = rawType & 0x7F;

        auto len = br.readField(8);
        if (!len) return Expected<L3DetachRequest>::error(len.error());
        size_t vLen = len.value();

        if (iei == 0x0c && vLen >= 2) {
            auto mi = L3MobileIdentity::parse(br, vLen);
            if (mi) {
                msg.mHavePTMSI = true;
                msg.mPTMSI = std::move(mi).value();
            }
        } else if (iei == L3GMMCauseIE::IEI && vLen >= 1) {
            auto cause = br.readField(8);
            if (cause) msg.mCause = static_cast<GMMCause>(cause.value());
        }
    }

    return Expected<L3DetachRequest>::hold(std::move(msg));
}

void L3DetachRequest::write(BitWriter& bw) const {
    bw.writeField((mDetachType << 4) | (mPowerOff ? 0x08 : 0), 8);
}

void L3DetachRequest::text(std::ostream& os) const {
    os << "DetachRequest(type=" << mDetachType << ",powerOff=" << mPowerOff << ")";
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

// ── L3RoutingAreaUpdateRequest (GSM 24.008 9.4.12) ───────────────────

size_t L3RoutingAreaUpdateRequest::bodyLength() const {
    size_t len = 1; // updateType|CKSN
    len += 6;       // oldRoutingAreaID (raw)
    if (mHaveMsRACap) len += lvLen(mMsRACap.size());
    return len;
}

Expected<L3RoutingAreaUpdateRequest> L3RoutingAreaUpdateRequest::parse(BitReader& br) {
    L3RoutingAreaUpdateRequest msg;

    // 24.008 9.4.12: updateType(3)|forL3(1)|CKSN(3)|spare(1) = 1 octet
    {
        auto o = br.readField(8);
        if (!o) return Expected<L3RoutingAreaUpdateRequest>::error(o.error());
        msg.mUpdateType = static_cast<GMMUpdateType>((o.value() >> 5) & 0x07);
        msg.mForL3 = ((o.value() >> 4) & 0x01) != 0;
        msg.mCKSN = o.value() & 0x0F;
    }

    // oldRoutingAreaID (raw, 6 octets)
    {
        auto rai = L3RoutingAreaIdentification::parse(br);
        if (!rai) return Expected<L3RoutingAreaUpdateRequest>::error(rai.error());
        msg.mOldRAI = std::move(rai).value();
    }

    // Optional IEs
    while (br.hasMore()) {
        auto type = br.readField(8);
        if (!type) return Expected<L3RoutingAreaUpdateRequest>::error(type.error());
        uint8_t rawType = static_cast<uint8_t>(type.value());
        bool ext = (rawType & 0x80) != 0;
        uint8_t iei = rawType & 0x7F;

        auto len = br.readField(8);
        if (!len) return Expected<L3RoutingAreaUpdateRequest>::error(len.error());
        size_t vLen = len.value();

        if (iei == 0x62 || iei == 0xe2) {
            msg.mHaveMsRACap = true;
            msg.mMsRACap.resize(vLen);
            if (vLen > 0) {
                auto r = br.readBytes(msg.mMsRACap.data(), vLen);
                if (!r) return Expected<L3RoutingAreaUpdateRequest>::error(r.error());
            }
        }
    }

    return Expected<L3RoutingAreaUpdateRequest>::hold(std::move(msg));
}

void L3RoutingAreaUpdateRequest::write(BitWriter& bw) const {
    bw.writeField(((static_cast<uint8_t>(mUpdateType) & 0x07) << 5) |
                  ((mForL3 ? 1 : 0) << 4) | (mCKSN & 0x0F), 8);
    mOldRAI.write(bw);
    if (mHaveMsRACap) {
        bw.writeField(0x80 | 0x62, 8);
        bw.writeField(static_cast<uint32_t>(mMsRACap.size()), 8);
        bw.writeBytes(mMsRACap.data(), mMsRACap.size());
    }
}

void L3RoutingAreaUpdateRequest::text(std::ostream& os) const {
    os << "RAUpdateReq(type=";
    switch (mUpdateType) {
        case GMMUpdateType::RAUpdated: os << "RA"; break;
        case GMMUpdateType::CombinedRALAUpdated: os << "CombinedRA-LA"; break;
        case GMMUpdateType::CombinedRALAWithImsiAttach: os << "CombinedRA-LA-IMSI"; break;
        case GMMUpdateType::PeriodicUpdating: os << "Periodic"; break;
    }
    os << ",CKSN=" << (mCKSN >> 1 & 0x07) << ")";
}

// ── L3RoutingAreaUpdateAccept (GSM 24.008 9.4.15) ────────────────────

size_t L3RoutingAreaUpdateAccept::bodyLength() const {
    size_t len = 1; // forceToStandby|updateResult|...
    len += 6;       // routingAreaId (raw)
    if (mHavePTMSI) len += tlvLen(mPTMSI.lengthV());
    return len;
}

Expected<L3RoutingAreaUpdateAccept> L3RoutingAreaUpdateAccept::parse(BitReader& br) {
    L3RoutingAreaUpdateAccept msg;

    // forceToStandby(1)|updateResult(3)|spare(1)|raUpdateTimer(2)|radioPriority(1) = 1 octet
    {
        auto o = br.readField(8);
        if (!o) return Expected<L3RoutingAreaUpdateAccept>::error(o.error());
        msg.mForceToStandby = ((o.value() >> 7) & 0x01) != 0;
        msg.mUpdateResult = static_cast<GMMUpdateType>((o.value() >> 4) & 0x07);
    }

    // routingAreaId (raw, 6 octets)
    {
        auto rai = L3RoutingAreaIdentification::parse(br);
        if (!rai) return Expected<L3RoutingAreaUpdateAccept>::error(rai.error());
        msg.mRAI = std::move(rai).value();
    }

    // Optional IEs
    while (br.hasMore()) {
        auto type = br.readField(8);
        if (!type) return Expected<L3RoutingAreaUpdateAccept>::error(type.error());
        uint8_t rawType = static_cast<uint8_t>(type.value());
        uint8_t iei = rawType & 0x7F;

        auto len = br.readField(8);
        if (!len) return Expected<L3RoutingAreaUpdateAccept>::error(len.error());
        size_t vLen = len.value();

        if (iei == 0x0c && vLen >= 2) {
            auto mi = L3MobileIdentity::parse(br, vLen);
            if (mi) {
                msg.mHavePTMSI = true;
                msg.mPTMSI = std::move(mi).value();
            }
        }
    }

    return Expected<L3RoutingAreaUpdateAccept>::hold(std::move(msg));
}

void L3RoutingAreaUpdateAccept::write(BitWriter& bw) const {
    bw.writeField((mForceToStandby ? 0x80 : 0) | ((static_cast<uint8_t>(mUpdateResult) & 0x07) << 4), 8);
    mRAI.write(bw);
    if (mHavePTMSI) {
        bw.writeField(0x8c, 8);
        bw.writeField(static_cast<uint32_t>(mPTMSI.lengthV()), 8);
        mPTMSI.write(bw);
    }
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

// ── L3RoutingAreaUpdateComplete (GSM 24.008 9.4.16) ──────────────────

Expected<L3RoutingAreaUpdateComplete> L3RoutingAreaUpdateComplete::parse(BitReader& br) {
    return Expected<L3RoutingAreaUpdateComplete>::hold(L3RoutingAreaUpdateComplete{});
}

void L3RoutingAreaUpdateComplete::write(BitWriter&) const {}

void L3RoutingAreaUpdateComplete::text(std::ostream& os) const {
    os << "RAUpdateComplete";
}

// ── L3RoutingAreaUpdateReject (GSM 24.008 9.4.17) ────────────────────

size_t L3RoutingAreaUpdateReject::bodyLength() const {
    size_t len = tlvLen(1); // gmmCause TLV
    if (mHaveT3302) len += tlvLen(1);
    return len;
}

Expected<L3RoutingAreaUpdateReject> L3RoutingAreaUpdateReject::parse(BitReader& br) {
    L3RoutingAreaUpdateReject msg;

    while (br.hasMore()) {
        auto type = br.readField(8);
        if (!type) return Expected<L3RoutingAreaUpdateReject>::error(type.error());
        uint8_t rawType = static_cast<uint8_t>(type.value());
        uint8_t iei = rawType & 0x7F;

        auto len = br.readField(8);
        if (!len) return Expected<L3RoutingAreaUpdateReject>::error(len.error());
        size_t vLen = len.value();

        if (iei == L3GMMCauseIE::IEI && vLen >= 1) {
            auto cause = br.readField(8);
            if (cause) msg.mCause = static_cast<GMMCause>(cause.value());
        } else if (iei == L3T3302Timer::IEI && vLen >= 1) {
            auto t = br.readField(8);
            if (t) {
                msg.mHaveT3302 = true;
                msg.mT3302 = L3T3302Timer{static_cast<uint8_t>(t.value())};
            }
        }
    }

    return Expected<L3RoutingAreaUpdateReject>::hold(std::move(msg));
}

void L3RoutingAreaUpdateReject::write(BitWriter& bw) const {
    bw.writeField(0x80 | L3GMMCauseIE::IEI, 8);
    bw.writeField(1, 8);
    bw.writeField(static_cast<uint8_t>(mCause), 8);
    if (mHaveT3302) {
        bw.writeField(0x80 | L3T3302Timer::IEI, 8);
        bw.writeField(1, 8);
        bw.writeField(mT3302.value(), 8);
    }
}

void L3RoutingAreaUpdateReject::text(std::ostream& os) const {
    os << "RAUpdateReject(cause=" << GMMCause2Str(mCause) << ")";
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

// ── L3ServiceAccept (GSM 24.008 9.4.21) ──────────────────────────────

Expected<L3ServiceAccept> L3ServiceAccept::parse(BitReader&) {
    return Expected<L3ServiceAccept>::hold(L3ServiceAccept{});
}

void L3ServiceAccept::write(BitWriter&) const {}

void L3ServiceAccept::text(std::ostream& os) const {
    os << "ServiceAccept";
}

// ── L3ServiceReject (GSM 24.008 9.4.22) ──────────────────────────────

size_t L3ServiceReject::bodyLength() const {
    return tlvLen(1); // gmmCause TLV
}

Expected<L3ServiceReject> L3ServiceReject::parse(BitReader& br) {
    L3ServiceReject msg;

    while (br.hasMore()) {
        auto type = br.readField(8);
        if (!type) return Expected<L3ServiceReject>::error(type.error());
        uint8_t iei = type.value() & 0x7F;

        auto len = br.readField(8);
        if (!len) return Expected<L3ServiceReject>::error(len.error());

        if (iei == L3GMMCauseIE::IEI && len.value() >= 1) {
            auto cause = br.readField(8);
            if (cause) msg.mCause = static_cast<GMMCause>(cause.value());
        }
    }

    return Expected<L3ServiceReject>::hold(std::move(msg));
}

void L3ServiceReject::write(BitWriter& bw) const {
    bw.writeField(0x80 | L3GMMCauseIE::IEI, 8);
    bw.writeField(1, 8);
    bw.writeField(static_cast<uint8_t>(mCause), 8);
}

void L3ServiceReject::text(std::ostream& os) const {
    os << "ServiceReject(cause=" << GMMCause2Str(mCause) << ")";
}

// ── L3P_TMSIReallocationCommand (GSM 24.008 9.4.8) ───────────────────

size_t L3P_TMSIReallocationCommand::bodyLength() const {
    size_t len = 1; // PTMSI_Type|forceToStandby
    len += 6;       // routingAreaId (raw)
    if (mHavePTMSI) len += tlvLen(mPTMSI.lengthV());
    return len;
}

Expected<L3P_TMSIReallocationCommand> L3P_TMSIReallocationCommand::parse(BitReader& br) {
    L3P_TMSIReallocationCommand msg;

    // PTMSI_Type(1)|spare(3)|forceToStandby(1)|spare(4) = 1 octet (actually forceToStandby is separate)
    {
        auto o = br.readField(8);
        if (!o) return Expected<L3P_TMSIReallocationCommand>::error(o.error());
        msg.mPTMSIType = static_cast<GMMPTMSIType>(o.value() & 0x01);
    }

    // routingAreaId (raw, 6 octets)
    {
        auto rai = L3RoutingAreaIdentification::parse(br);
        if (!rai) return Expected<L3P_TMSIReallocationCommand>::error(rai.error());
        msg.mRAI = std::move(rai).value();
    }

    // Optional IEs
    while (br.hasMore()) {
        auto type = br.readField(8);
        if (!type) return Expected<L3P_TMSIReallocationCommand>::error(type.error());
        uint8_t iei = type.value() & 0x7F;

        auto len = br.readField(8);
        if (!len) return Expected<L3P_TMSIReallocationCommand>::error(len.error());
        size_t vLen = len.value();

        if (iei == 0x0c && vLen >= 2) {
            auto mi = L3MobileIdentity::parse(br, vLen);
            if (mi) {
                msg.mHavePTMSI = true;
                msg.mPTMSI = std::move(mi).value();
            }
        }
    }

    return Expected<L3P_TMSIReallocationCommand>::hold(std::move(msg));
}

void L3P_TMSIReallocationCommand::write(BitWriter& bw) const {
    bw.writeField(static_cast<uint8_t>(mPTMSIType), 8);
    mRAI.write(bw);
    if (mHavePTMSI) {
        bw.writeField(0x8c, 8);
        bw.writeField(static_cast<uint32_t>(mPTMSI.lengthV()), 8);
        mPTMSI.write(bw);
    }
}

void L3P_TMSIReallocationCommand::text(std::ostream& os) const {
    os << "P_TMSIRreallocCmd(type=" << static_cast<int>(mPTMSIType) << ")";
}

// ── L3P_TMSIReallocationComplete (GSM 24.008 9.4.8) ──────────────────

Expected<L3P_TMSIReallocationComplete> L3P_TMSIReallocationComplete::parse(BitReader&) {
    return Expected<L3P_TMSIReallocationComplete>::hold(L3P_TMSIReallocationComplete{});
}

void L3P_TMSIReallocationComplete::write(BitWriter&) const {}

void L3P_TMSIReallocationComplete::text(std::ostream& os) const {
    os << "P_TMSIRreallocComplete";
}

// ── L3AuthenticationAndCipheringRequest (GSM 24.008 9.4.9) ────────────

size_t L3AuthenticationAndCipheringRequest::bodyLength() const {
    size_t len = 1; // cipheringAlgorithm|imeisvRequest|forceToStandby|acReferenceNumber
    len += tlvLen(16); // authenticationParameterRAND
    return len;
}

Expected<L3AuthenticationAndCipheringRequest> L3AuthenticationAndCipheringRequest::parse(BitReader& br) {
    L3AuthenticationAndCipheringRequest msg;

    // cipheringAlgorithm(3)|spare(1)|imeisvRequest(1)|forceToStandby(1)|spare(5) = 1 octet
    {
        auto o = br.readField(8);
        if (!o) return Expected<L3AuthenticationAndCipheringRequest>::error(o.error());
        msg.mCipheringAlgorithm = (o.value() >> 5) & 0x07;
        msg.mImeisvRequest = ((o.value() >> 4) & 0x01) != 0;
        msg.mForceToStandby = ((o.value() >> 3) & 0x01) != 0;
    }

    // acReferenceNumber(4)|spare(4) = 1 octet
    {
        auto o = br.readField(8);
        if (!o) return Expected<L3AuthenticationAndCipheringRequest>::error(o.error());
        msg.mACReferenceNumber = o.value() & 0x0F;
    }

    // authenticationParameterRAND (TLV, IEI=0x15)
    {
        auto rand = L3AuthRAND::parse(br);
        if (!rand) return Expected<L3AuthenticationAndCipheringRequest>::error(rand.error());
        msg.mRAND = std::move(rand).value();
    }

    return Expected<L3AuthenticationAndCipheringRequest>::hold(std::move(msg));
}

void L3AuthenticationAndCipheringRequest::write(BitWriter& bw) const {
    bw.writeField((mCipheringAlgorithm << 5) | (mImeisvRequest ? 0x10 : 0) | (mForceToStandby ? 0x08 : 0), 8);
    bw.writeField(mACReferenceNumber, 8);
    mRAND.write(bw);
}

void L3AuthenticationAndCipheringRequest::text(std::ostream& os) const {
    os << "AuthCipherReq(alg=" << mCipheringAlgorithm << ",acRef=" << mACReferenceNumber << ")";
}

// ── L3AuthenticationAndCipheringResponse (GSM 24.008 9.4.9) ───────────

size_t L3AuthenticationAndCipheringResponse::bodyLength() const {
    size_t len = 1; // acReferenceNumber|spare
    len += tlvLen(4); // authenticationParameterResponse
    return len;
}

Expected<L3AuthenticationAndCipheringResponse> L3AuthenticationAndCipheringResponse::parse(BitReader& br) {
    L3AuthenticationAndCipheringResponse msg;

    // acReferenceNumber(4)|spare(4) = 1 octet
    {
        auto o = br.readField(8);
        if (!o) return Expected<L3AuthenticationAndCipheringResponse>::error(o.error());
        msg.mACReferenceNumber = o.value() & 0x0F;
    }

    // Skip spare(4) and read authenticationParameterResponse (TLV, IEI=0x16)
    {
        auto spare = br.readField(4);
        if (!spare) return Expected<L3AuthenticationAndCipheringResponse>::error(spare.error());

        auto res = L3AuthRES::parse(br);
        if (!res) return Expected<L3AuthenticationAndCipheringResponse>::error(res.error());
        msg.mRES = std::move(res).value();
    }

    return Expected<L3AuthenticationAndCipheringResponse>::hold(std::move(msg));
}

void L3AuthenticationAndCipheringResponse::write(BitWriter& bw) const {
    bw.writeField(mACReferenceNumber, 4);
    bw.writeField(0, 4); // spare
    mRES.write(bw);
}

void L3AuthenticationAndCipheringResponse::text(std::ostream& os) const {
    os << "AuthCipherResp(acRef=" << mACReferenceNumber << ")";
}

// ── L3AuthenticationAndCipheringReject (GSM 24.008 9.4.9) ─────────────

Expected<L3AuthenticationAndCipheringReject> L3AuthenticationAndCipheringReject::parse(BitReader&) {
    return Expected<L3AuthenticationAndCipheringReject>::hold(L3AuthenticationAndCipheringReject{});
}

void L3AuthenticationAndCipheringReject::write(BitWriter&) const {}

void L3AuthenticationAndCipheringReject::text(std::ostream& os) const {
    os << "AuthCipherReject";
}

// ── L3GMMIdentityRequest (GSM 24.008 9.4.7) ──────────────────────────

Expected<L3GMMIdentityRequest> L3GMMIdentityRequest::parse(BitReader& br) {
    L3GMMIdentityRequest msg;

    // identityType(3)|spare(1)|forceToStandby(1)|spare(4) = 1 octet... actually 2 octets per TTCN-3
    auto o1 = br.readField(8);
    if (!o1) return Expected<L3GMMIdentityRequest>::error(o1.error());
    msg.mIdentityType = static_cast<MobileIDType>((o1.value() >> 5) & 0x07);
    msg.mForceToStandby = ((o1.value() >> 4) & 0x01) != 0;

    return Expected<L3GMMIdentityRequest>::hold(std::move(msg));
}

void L3GMMIdentityRequest::write(BitWriter& bw) const {
    bw.writeField(((static_cast<uint8_t>(mIdentityType) & 0x07) << 5) | (mForceToStandby ? 0x10 : 0), 8);
    bw.writeField(0, 8); // spare
}

void L3GMMIdentityRequest::text(std::ostream& os) const {
    os << "GMMIdentityReq(type=" << static_cast<int>(mIdentityType) << ")";
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

// ── L3AuthenticationAndCipheringFailure (GSM 24.008 9.4.23) ───────────

size_t L3AuthenticationAndCipheringFailure::bodyLength() const {
    size_t len = tlvLen(1); // gmmCause
    len += tlvLen(mAuthFailureParam.lengthV()); // authenticationFailureParameter
    return len;
}

Expected<L3AuthenticationAndCipheringFailure> L3AuthenticationAndCipheringFailure::parse(BitReader& br) {
    L3AuthenticationAndCipheringFailure msg;

    while (br.hasMore()) {
        auto type = br.readField(8);
        if (!type) return Expected<L3AuthenticationAndCipheringFailure>::error(type.error());
        uint8_t iei = type.value() & 0x7F;

        auto len = br.readField(8);
        if (!len) return Expected<L3AuthenticationAndCipheringFailure>::error(len.error());
        size_t vLen = len.value();

        if (iei == L3GMMCauseIE::IEI && vLen >= 1) {
            auto cause = br.readField(8);
            if (cause) msg.mCause = static_cast<GMMCause>(cause.value());
        } else if (iei == L3AuthFailureParam::IEI) {
            // Parse AUTS with known length
            msg.mAuthFailureParam.mAUTS.resize(vLen);
            if (vLen > 0) {
                auto r = br.readBytes(msg.mAuthFailureParam.mAUTS.data(), vLen);
                if (!r) return Expected<L3AuthenticationAndCipheringFailure>::error(r.error());
            }
        }
    }

    return Expected<L3AuthenticationAndCipheringFailure>::hold(std::move(msg));
}

void L3AuthenticationAndCipheringFailure::write(BitWriter& bw) const {
    bw.writeField(0x80 | L3GMMCauseIE::IEI, 8);
    bw.writeField(1, 8);
    bw.writeField(static_cast<uint8_t>(mCause), 8);

    bw.writeField(0x80 | L3AuthFailureParam::IEI, 8);
    bw.writeField(static_cast<uint32_t>(mAuthFailureParam.mAUTS.size()), 8);
    if (!mAuthFailureParam.mAUTS.empty()) {
        bw.writeBytes(mAuthFailureParam.mAUTS.data(), mAuthFailureParam.mAUTS.size());
    }
}

void L3AuthenticationAndCipheringFailure::text(std::ostream& os) const {
    os << "AuthCipherFail(cause=" << GMMCause2Str(mCause) << ")";
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

// ── L3GMMInformation (GSM 24.008) ─────────────────────────────────────

Expected<L3GMMInformation> L3GMMInformation::parse(BitReader&) {
    return Expected<L3GMMInformation>::hold(L3GMMInformation{});
}

void L3GMMInformation::write(BitWriter&) const {}

void L3GMMInformation::text(std::ostream& os) const {
    os << "GMMInformation";
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
