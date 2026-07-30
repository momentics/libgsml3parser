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
#include "gsml3parser/logger.h"
#include <sstream>
#include <iomanip>

namespace gsml3parser {

// ── L3MMMessage ─────────────────────────────────────────────────────────

void L3MMMessage::text(std::ostream& os) const {
    L3Message::text(os);
}

std::ostream& operator<<(std::ostream& os, L3MMMessage::MessageType val) {
    switch (val) {
        case L3MMMessage::IMSIDetachIndication:     os << "IMSIDetach"; break;
        case L3MMMessage::CMServiceAccept:          os << "CMServiceAccept"; break;
        case L3MMMessage::CMServiceReject:          os << "CMServiceReject"; break;
        case L3MMMessage::CMServiceAbort:           os << "CMServiceAbort"; break;
        case L3MMMessage::CMServiceRequest:         os << "CMServiceRequest"; break;
        case L3MMMessage::CMReestablishmentRequest: os << "CMReestablishment"; break;
        case L3MMMessage::IdentityResponse:         os << "IdentityResponse"; break;
        case L3MMMessage::IdentityRequest:          os << "IdentityRequest"; break;
        case L3MMMessage::MMInformation:            os << "MMInformation"; break;
        case L3MMMessage::LocationUpdatingAccept:   os << "LocationUpdatingAccept"; break;
        case L3MMMessage::LocationUpdatingReject:   os << "LocationUpdatingReject"; break;
        case L3MMMessage::LocationUpdatingRequest:  os << "LocationUpdatingRequest"; break;
        case L3MMMessage::TMSIReallocationCommand:  os << "TMSIReallocationCmd"; break;
        case L3MMMessage::TMSIReallocationComplete: os << "TMSIReallocationComplete"; break;
        case L3MMMessage::MMStatus:                 os << "MMStatus"; break;
        case L3MMMessage::AuthenticationRequest:    os << "AuthenticationRequest"; break;
        case L3MMMessage::AuthenticationResponse:   os << "AuthenticationResponse"; break;
        case L3MMMessage::AuthenticationReject:     os << "AuthenticationReject"; break;
        default:                                     os << "Unknown_MM(" << val << ")"; break;
    }
    return os;
}

// ── L3LocationUpdatingRequest ──────────────────────────────────────────

size_t L3LocationUpdatingRequest::l2BodyLength() const {
    return 1 + mLAI.lengthV() + mClassmark.lengthV() + mMobileIdentity.lengthLV();
}

void L3LocationUpdatingRequest::parseBody(const L3Frame& src, size_t& rp) {
    mCKSN = src.readField(rp, 4);
    mUpdateType = src.readField(rp, 4);
    mLAI.parseV(src, rp);
    mClassmark.parseV(src, rp);
    mMobileIdentity.parseLV(src, rp);
}

void L3LocationUpdatingRequest::writeBody(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, mCKSN, 4);
    dest.writeField(wp, mUpdateType, 4);
    mLAI.writeV(dest, wp);
    mClassmark.writeV(dest, wp);
    mMobileIdentity.writeLV(dest, wp);
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

// ── L3LocationUpdatingAccept ───────────────────────────────────────────

L3LocationUpdatingAccept::L3LocationUpdatingAccept(const L3LocationAreaIdentity& wLAI, bool wFollowOn)
    : mLAI(wLAI), mFollowOnProceed(wFollowOn), mHaveMobileIdentity(false) {}

L3LocationUpdatingAccept::L3LocationUpdatingAccept(const L3LocationAreaIdentity& wLAI,
                                                    const L3MobileIdentity& wID, bool wFollowOn)
    : mLAI(wLAI), mFollowOnProceed(wFollowOn), mHaveMobileIdentity(true), mMobileIdentity(wID) {}

size_t L3LocationUpdatingAccept::l2BodyLength() const {
    size_t result = mLAI.lengthV();
    if (mHaveMobileIdentity) result += mMobileIdentity.lengthTLV();
    if (mFollowOnProceed) result += 1;
    return result;
}

void L3LocationUpdatingAccept::parseBody(const L3Frame& src, size_t& rp) {
    mLAI.parseV(src, rp);
    mHaveMobileIdentity = mMobileIdentity.parseTLV(0x17, src, rp);
    mFollowOnProceed = (src.peekField(rp, 8) == 0xa1);
    if (mFollowOnProceed) rp += 8;
}

void L3LocationUpdatingAccept::writeBody(L3Frame& dest, size_t& wp) const {
    mLAI.writeV(dest, wp);
    if (mHaveMobileIdentity) mMobileIdentity.writeTLV(0x17, dest, wp);
    if (mFollowOnProceed) dest.writeField(wp, 0xa1, 8);
}

void L3LocationUpdatingAccept::text(std::ostream& os) const {
    os << "LocationUpdatingAccept: ";
    mLAI.text(os);
    if (mHaveMobileIdentity) {
        os << " ";
        mMobileIdentity.text(os);
    }
}

// ── L3LocationUpdatingReject ───────────────────────────────────────────

void L3LocationUpdatingReject::parseBody(const L3Frame& src, size_t& rp) {
    mCause = static_cast<MMRejectCause>(src.readField(rp, 8));
}

void L3LocationUpdatingReject::writeBody(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, static_cast<unsigned>(mCause), 8);
}

void L3LocationUpdatingReject::text(std::ostream& os) const {
    os << "LocationUpdatingReject: " << MMRejectCause2Str(mCause);
}

// ── L3IMSIDetachIndication ─────────────────────────────────────────────

size_t L3IMSIDetachIndication::l2BodyLength() const {
    return 1 + mMobileIdentity.lengthLV();
}

void L3IMSIDetachIndication::parseBody(const L3Frame& src, size_t& rp) {
    mClassmark.parseV(src, rp);
    mMobileIdentity.parseLV(src, rp);
}

void L3IMSIDetachIndication::writeBody(L3Frame& dest, size_t& wp) const {
    mClassmark.writeV(dest, wp);
    mMobileIdentity.writeLV(dest, wp);
}

void L3IMSIDetachIndication::text(std::ostream& os) const {
    os << "IMSIDetachIndication: ";
    mMobileIdentity.text(os);
}

// ── L3CMServiceAbort ───────────────────────────────────────────────────

void L3CMServiceAbort::parseBody(const L3Frame& src, size_t& rp) {
    if (rp + 8 <= src.size()) {
        mCause = static_cast<MMRejectCause>(src.readField(rp, 8));
        mHaveCause = true;
    }
}

void L3CMServiceAbort::writeBody(L3Frame& dest, size_t& wp) const {
    if (mHaveCause) {
        dest.writeField(wp, static_cast<unsigned>(mCause), 8);
    }
}

void L3CMServiceAbort::text(std::ostream& os) const {
    os << "CMServiceAbort";
    if (mHaveCause) {
        os << ": " << MMRejectCause2Str(mCause);
    }
}

// ── L3CMServiceReject ──────────────────────────────────────────────────

void L3CMServiceReject::writeBody(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, static_cast<unsigned>(mCause), 8);
}

void L3CMServiceReject::text(std::ostream& os) const {
    os << "CMServiceReject: " << MMRejectCause2Str(mCause);
}

// ── L3CMServiceRequest ─────────────────────────────────────────────────

size_t L3CMServiceRequest::l2BodyLength() const {
    // ciphering(4) + serviceType(4) + classmark(LV) + mobileID(LV)
    return 1 + mClassmark.lengthLV() + mMobileIdentity.lengthLV();
}

void L3CMServiceRequest::parseBody(const L3Frame& src, size_t& rp) {
    // GSM 04.08 9.2.9: skip ciphering(4), serviceType(4), classmark(LV), mobileID(LV)
    src.readField(rp, 4);  // skip ciphering key sequence number
    mServiceType.parseV(src, rp);
    mClassmark.parseLV(src, rp);
    mMobileIdentity.parseLV(src, rp);
}

void L3CMServiceRequest::writeBody(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, 0, 4);  // spare ciphering
    mServiceType.writeV(dest, wp);
    mClassmark.writeLV(dest, wp);
    mMobileIdentity.writeLV(dest, wp);
}

void L3CMServiceRequest::text(std::ostream& os) const {
    os << "CMServiceRequest: type=";
    mServiceType.text(os);
    os << " ";
    mMobileIdentity.text(os);
}

// ── L3CMReestablishmentRequest ─────────────────────────────────────────

size_t L3CMReestablishmentRequest::l2BodyLength() const {
    // ciphering(8) + classmark(LV) + mobileID(LV) + optional LAI(TLV)
    return 1 + mClassmark.lengthLV() + mMobileID.lengthLV() + (mHaveLAI ? mLAI.lengthTLV() : 0);
}

void L3CMReestablishmentRequest::parseBody(const L3Frame& src, size_t& rp) {
    // GSM 04.08 9.2.4: skip ciphering(8), classmark(LV), mobileID(LV), optional LAI(TLV 0x13)
    src.readField(rp, 8);  // skip ciphering key sequence number + spare
    mClassmark.parseLV(src, rp);
    mMobileID.parseLV(src, rp);
    mHaveLAI = mLAI.parseTLV(0x13, src, rp);
}

void L3CMReestablishmentRequest::writeBody(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, 0, 8);  // spare ciphering
    mClassmark.writeLV(dest, wp);
    mMobileID.writeLV(dest, wp);
    if (mHaveLAI) {
        mLAI.writeTLV(0x13, dest, wp);
    }
}

void L3CMReestablishmentRequest::text(std::ostream& os) const {
    os << "CMReestablishmentRequest: ";
    mMobileID.text(os);
}

// ── L3MMInformation ────────────────────────────────────────────────────

L3MMInformation::L3MMInformation() {}

size_t L3MMInformation::l2BodyLength() const {
    size_t len = 0;
    if (mShortName.lengthV() > 1) len += mShortName.lengthTLV();
    len += mTime.lengthTV();
    return len;
}

void L3MMInformation::parseBody(const L3Frame& src, size_t& rp) {
    // GSM 04.08 9.2.15a: shortName(TLV 0x45), time(TV 0x47)
    mShortName.parseTLV(0x45, src, rp);
    mTime.parseTV(0x47, src, rp);
}

void L3MMInformation::writeBody(L3Frame& dest, size_t& wp) const {
    if (mShortName.lengthV() > 1) mShortName.writeTLV(0x45, dest, wp);
    mTime.writeTV(0x47, dest, wp);
}

void L3MMInformation::text(std::ostream& os) const {
    os << "MMInformation:";
    if (mShortName.lengthV() > 1) {
        os << " shortName=(" << mShortName << ")";
    }
    os << " time=(" << mTime << ")";
}

// ── L3IdentityRequest ──────────────────────────────────────────────────

void L3IdentityRequest::parseBody(const L3Frame& src, size_t& rp) {
    src.readField(rp, 4);  // spare
    mType = static_cast<MobileIDType>(src.readField(rp, 4));
}

void L3IdentityRequest::writeBody(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, 0, 4);  // spare half octet
    dest.writeField(wp, static_cast<unsigned>(mType), 4);
}

void L3IdentityRequest::text(std::ostream& os) const {
    os << "IdentityRequest: type=" << static_cast<int>(mType);
}

// ── L3IdentityResponse ─────────────────────────────────────────────────

size_t L3IdentityResponse::l2BodyLength() const {
    return mMobileID.lengthLV();
}

void L3IdentityResponse::parseBody(const L3Frame& src, size_t& rp) {
    mMobileID.parseLV(src, rp);
}

void L3IdentityResponse::writeBody(L3Frame& dest, size_t& wp) const {
    mMobileID.writeLV(dest, wp);
}

void L3IdentityResponse::text(std::ostream& os) const {
    os << "IdentityResponse: ";
    mMobileID.text(os);
}

// ── L3CMServiceAccept ──────────────────────────────────────────────────

void L3CMServiceAccept::text(std::ostream& os) const {
    os << "CMServiceAccept";
}

// ── L3AuthenticationRequest ────────────────────────────────────────────

L3AuthenticationRequest::L3AuthenticationRequest(unsigned ckSN, const std::vector<uint8_t>& rand)
    : mCKSN(ckSN), mRAND(rand) {}

void L3AuthenticationRequest::parseBody(const L3Frame& src, size_t& rp) {
    mCKSN = src.readField(rp, 4);
    src.readField(rp, 4);  // spare
    mRAND.resize(16);
    for (size_t i = 0; i < 16; ++i) {
        mRAND[i] = static_cast<uint8_t>(src.readField(rp, 8));
    }
}

void L3AuthenticationRequest::writeBody(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, mCKSN, 4);
    dest.writeField(wp, 0, 4);  // spare half octet
    for (const auto& b : mRAND) {
        dest.writeField(wp, b, 8);
    }
}

void L3AuthenticationRequest::text(std::ostream& os) const {
    os << "AuthenticationRequest: CKSN=" << mCKSN;
    os << " RAND=";
    for (const auto& b : mRAND) {
        os << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
    }
}

// ── L3AuthenticationResponse ───────────────────────────────────────────

void L3AuthenticationResponse::parseBody(const L3Frame& src, size_t& rp) {
    mSRES = src.readField(rp, 32);
}

void L3AuthenticationResponse::writeBody(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, mSRES, 32);
}

void L3AuthenticationResponse::text(std::ostream& os) const {
    os << "AuthenticationResponse: SRES=0x" << std::hex << mSRES;
}

// ── L3AuthenticationReject ─────────────────────────────────────────────

void L3AuthenticationReject::text(std::ostream& os) const {
    os << "AuthenticationReject";
}

// ── L3TMSIReallocationCommand ──────────────────────────────────────────

L3TMSIReallocationCommand::L3TMSIReallocationCommand(const L3LocationAreaIdentity& wLAI, const L3MobileIdentity& wTMSI, bool wFollowOn)
    : mLAI(wLAI), mTMSI(wTMSI), mFollowOnProceed(wFollowOn) {}

void L3TMSIReallocationCommand::writeBody(L3Frame& dest, size_t& wp) const {
    mLAI.writeV(dest, wp);
    mTMSI.writeLV(dest, wp);
    dest.writeField(wp, mFollowOnProceed ? 1 : 0, 1);
    dest.writeField(wp, 0, 7);
}

void L3TMSIReallocationCommand::parseBody(const L3Frame& src, size_t& rp) {
    mLAI.parseV(src, rp);
    mTMSI.parseLV(src, rp);
    mFollowOnProceed = src.readField(rp, 1);
    src.readField(rp, 7);
}

void L3TMSIReallocationCommand::text(std::ostream& os) const {
    os << "TMSIReallocationCommand: ";
    mLAI.text(os);
    os << " ";
    mTMSI.text(os);
    os << " followOn=" << (mFollowOnProceed ? "1" : "0");
}

// ── L3TMSIReallocationComplete ─────────────────────────────────────────

void L3TMSIReallocationComplete::writeBody(L3Frame&, size_t&) const {}

void L3TMSIReallocationComplete::text(std::ostream& os) const {
    os << "TMSIReallocationComplete";
}

// ── L3MMStatus ─────────────────────────────────────────────────────────

void L3MMStatus::parseBody(const L3Frame& src, size_t& rp) {
    mCause = static_cast<MMRejectCause>(src.readField(rp, 8));
    src.readField(rp, 16);  // spare
}

void L3MMStatus::writeBody(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, static_cast<unsigned>(mCause), 8);
    dest.writeField(wp, 0, 16);  // spare
}

void L3MMStatus::text(std::ostream& os) const {
    os << "MMStatus: " << MMRejectCause2Str(mCause);
}

// ── Factory ─────────────────────────────────────────────────────────────

L3MMMessage* L3MMFactory(int mti) {
    switch (mti) {
        case L3MMMessage::IMSIDetachIndication:     return new L3IMSIDetachIndication();
        case L3MMMessage::CMServiceAccept:          return new L3CMServiceAccept();
        case L3MMMessage::CMServiceAbort:           return new L3CMServiceAbort();
        case L3MMMessage::CMServiceReject:          return new L3CMServiceReject(MMRejectCause::Zero);
        case L3MMMessage::CMServiceRequest:         return new L3CMServiceRequest();
        case L3MMMessage::CMReestablishmentRequest: return new L3CMReestablishmentRequest();
        case L3MMMessage::IdentityResponse:         return new L3IdentityResponse();
        case L3MMMessage::IdentityRequest:          return new L3IdentityRequest(MobileIDType::NoID);
        case L3MMMessage::MMInformation:            return new L3MMInformation();
        case L3MMMessage::LocationUpdatingAccept:   return new L3LocationUpdatingAccept(L3LocationAreaIdentity());
        case L3MMMessage::LocationUpdatingReject:   return new L3LocationUpdatingReject(MMRejectCause::Zero);
        case L3MMMessage::LocationUpdatingRequest:  return new L3LocationUpdatingRequest();
        case L3MMMessage::TMSIReallocationCommand:  return new L3TMSIReallocationCommand(L3LocationAreaIdentity(), L3MobileIdentity());
        case L3MMMessage::TMSIReallocationComplete: return new L3TMSIReallocationComplete();
        case L3MMMessage::MMStatus:                 return new L3MMStatus();
        case L3MMMessage::AuthenticationRequest:    return new L3AuthenticationRequest(0, {});
        case L3MMMessage::AuthenticationResponse:   return new L3AuthenticationResponse();
        case L3MMMessage::AuthenticationReject:     return new L3AuthenticationReject();
        default:                                    return nullptr;
    }
}

// ── Parser ──────────────────────────────────────────────────────────────

std::unique_ptr<L3MMMessage> parseL3MM(const L3Frame& source) {
    if (source.size() < 16) return nullptr;

    // Bit 6 (MSB-1) is "don't care" for MM messages — mask it
    unsigned mti = source.MTI() & 0xbf;
    L3MMMessage* msg = L3MMFactory(static_cast<L3MMMessage::MessageType>(mti));
    if (!msg) {
        GSML3PARSER_LOG_WARN("Unknown MM MTI: 0x%02x", mti);
        return nullptr;
    }
    try {
        msg->parse(source);
    } catch (const ParseError&) {
        GSML3PARSER_LOG_WARN("MM parse failed for MTI=0x%02x", mti);
        delete msg;
        return nullptr;
    }
    return std::unique_ptr<L3MMMessage>(msg);
}

} // namespace gsml3parser
