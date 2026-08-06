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
    // CKSN(1) + LAI(5, mandatory raw) + CM1(LV) + MI(LV)
    return 1 + mLAI.lengthV() + mClassmark.lengthLV() + mMobileIdentity.lengthLV();
}

ParseResult<void> L3LocationUpdatingRequest::try_parseBody(const L3Frame& src, size_t& rp) {
    // GSM 04.08 9.2.15: LU_Type(2)|spare(2)|CKSN(4), LAI(5 raw), CM1 LV, MI LV
    // Reference: ts_LU_REQ has locationUpdatingType, cipheringKeySequenceNumber,
    //   locationAreaIdentification (RAW, not LV!), mobileStationClassmark1 (LV), mobileIdentityLV (LV)
    mUpdateType = src.readField(rp, 2);
    src.readField(rp, 2);  // spare
    mCKSN = src.readField(rp, 4);
    // LAI is mandatory and raw (NOT LV-prefixed!) per GSM 24.008 9.2.15
    auto res = mLAI.try_parseV(src, rp);
    if (!res.has_value()) return res;
    // classmark(LV) + mobileIdentity(LV)
    res = mClassmark.try_parseLV(src, rp);
    if (!res.has_value()) return res;
    res = mMobileIdentity.try_parseLV(src, rp);
    if (!res.has_value()) return res;
    return ParseResult<void>();
}

ParseResult<void> L3LocationUpdatingRequest::try_writeBody(L3Frame& dest, size_t& wp) const {
    // GSM 04.08 9.2.15: LU_Type(2)|spare(2)|CKSN(4), LAI(raw), CM1 LV, MI LV
    dest.writeField(wp, mUpdateType & 0x03, 2);
    dest.writeField(wp, 0, 2);  // spare
    dest.writeField(wp, mCKSN & 0x0F, 4);
    mLAI.writeV(dest, wp);
    mClassmark.writeLV(dest, wp);
    mMobileIdentity.writeLV(dest, wp);
    return ParseResult<void>();
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

L3LocationUpdatingAccept::L3LocationUpdatingAccept()
    : mFollowOnProceed(false), mHaveMobileIdentity(false) {}

// ── L3LocationUpdatingAccept Builder ───────────────────────────────────

L3LocationUpdatingAccept::Builder L3LocationUpdatingAccept::builder() { return Builder{}; }

L3LocationUpdatingAccept::Builder& L3LocationUpdatingAccept::Builder::lai(const L3LocationAreaIdentity& lai) {
    mLAI = lai;
    return *this;
}

L3LocationUpdatingAccept::Builder& L3LocationUpdatingAccept::Builder::mobileIdentity(const L3MobileIdentity& id) {
    mHaveMobileIdentity = true;
    mMobileIdentity = id;
    return *this;
}

L3LocationUpdatingAccept::Builder& L3LocationUpdatingAccept::Builder::followOn(bool fo) {
    mFollowOnProceed = fo;
    return *this;
}

L3LocationUpdatingAccept L3LocationUpdatingAccept::Builder::build() {
    L3LocationUpdatingAccept msg;
    msg.mLAI = mLAI;
    msg.mFollowOnProceed = mFollowOnProceed;
    msg.mHaveMobileIdentity = mHaveMobileIdentity;
    msg.mMobileIdentity = mMobileIdentity;
    return msg;
}

size_t L3LocationUpdatingAccept::l2BodyLength() const {
    size_t result = mLAI.lengthV();
    if (mHaveMobileIdentity) result += mMobileIdentity.lengthTLV();
    if (mFollowOnProceed) result += 1;
    return result;
}

ParseResult<void> L3LocationUpdatingAccept::try_parseBody(const L3Frame& src, size_t& rp) {
    auto res = mLAI.try_parseV(src, rp);
    if (!res.has_value()) return res;
    auto tlRes = mMobileIdentity.try_parseTLV(0x17, src, rp);
    if (!tlRes.has_value()) return tlRes;
    mHaveMobileIdentity = tlRes.value();
    mFollowOnProceed = (src.peekField(rp, 8) == 0xa1);
    if (mFollowOnProceed) rp += 8;
    return ParseResult<void>();
}

ParseResult<void> L3LocationUpdatingAccept::try_writeBody(L3Frame& dest, size_t& wp) const {
    mLAI.writeV(dest, wp);
    if (mHaveMobileIdentity) mMobileIdentity.writeTLV(0x17, dest, wp);
    if (mFollowOnProceed) dest.writeField(wp, 0xa1, 8);
    return ParseResult<void>();
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

ParseResult<void> L3LocationUpdatingReject::try_parseBody(const L3Frame& src, size_t& rp) {
    mCause = static_cast<MMRejectCause>(src.readField(rp, 8));
    return ParseResult<void>();
}

ParseResult<void> L3LocationUpdatingReject::try_writeBody(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, static_cast<unsigned>(mCause), 8);
    return ParseResult<void>();
}

void L3LocationUpdatingReject::text(std::ostream& os) const {
    os << "LocationUpdatingReject: " << MMRejectCause2Str(mCause);
}

// ── L3IMSIDetachIndication ─────────────────────────────────────────────

size_t L3IMSIDetachIndication::l2BodyLength() const {
    return 1 + mMobileIdentity.lengthLV();
}

ParseResult<void> L3IMSIDetachIndication::try_parseBody(const L3Frame& src, size_t& rp) {
    auto res = mClassmark.try_parseV(src, rp);
    if (!res.has_value()) return res;
    res = mMobileIdentity.try_parseLV(src, rp);
    if (!res.has_value()) return res;
    return ParseResult<void>();
}

ParseResult<void> L3IMSIDetachIndication::try_writeBody(L3Frame& dest, size_t& wp) const {
    mClassmark.writeV(dest, wp);
    mMobileIdentity.writeLV(dest, wp);
    return ParseResult<void>();
}

void L3IMSIDetachIndication::text(std::ostream& os) const {
    os << "IMSIDetachIndication: ";
    mMobileIdentity.text(os);
}

// ── L3CMServiceAbort ───────────────────────────────────────────────────

ParseResult<void> L3CMServiceAbort::try_parseBody(const L3Frame&, size_t&) {
    // Nothing to parse - empty body per GSM 04.08 9.2.7
    return ParseResult<void>();
}

void L3CMServiceAbort::text(std::ostream& os) const {
    os << "CMServiceAbort";
}

// ── L3CMServiceReject ──────────────────────────────────────────────────

ParseResult<void> L3CMServiceReject::try_parseBody(const L3Frame& src, size_t& rp) {
    mCause = static_cast<MMRejectCause>(src.readField(rp, 8));
    return ParseResult<void>();
}

ParseResult<void> L3CMServiceReject::try_writeBody(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, static_cast<unsigned>(mCause), 8);
    return ParseResult<void>();
}

void L3CMServiceReject::text(std::ostream& os) const {
    os << "CMServiceReject: " << MMRejectCause2Str(mCause);
}

// ── L3CMServiceRequest ─────────────────────────────────────────────────

size_t L3CMServiceRequest::l2BodyLength() const {
    // ciphering(4) + serviceType(4) + classmark(LV) + mobileID(LV)
    return 1 + mClassmark.lengthLV() + mMobileIdentity.lengthLV();
}

ParseResult<void> L3CMServiceRequest::try_parseBody(const L3Frame& src, size_t& rp) {
    // GSM 04.08 9.2.9: skip ciphering(4), serviceType(4), classmark(LV), mobileID(LV)
    src.readField(rp, 4);  // skip ciphering key sequence number
    auto res = mServiceType.try_parseV(src, rp);
    if (!res.has_value()) return res;
    res = mClassmark.try_parseLV(src, rp);
    if (!res.has_value()) return res;
    res = mMobileIdentity.try_parseLV(src, rp);
    if (!res.has_value()) return res;
    return ParseResult<void>();
}

ParseResult<void> L3CMServiceRequest::try_writeBody(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, 0, 4);  // spare ciphering
    mServiceType.writeV(dest, wp);
    mClassmark.writeLV(dest, wp);
    mMobileIdentity.writeLV(dest, wp);
    return ParseResult<void>();
}

void L3CMServiceRequest::text(std::ostream& os) const {
    os << "CMServiceRequest: type=";
    mServiceType.text(os);
    os << " ";
    mMobileIdentity.text(os);
}

// ── L3CMReestablishmentRequest ─────────────────────────────────────────

size_t L3CMReestablishmentRequest::l2BodyLength() const {
    // CKSN(1) + classmark(LV) + mobileID(LV) + optional LAI(TLV)
    return 1 + mClassmark.lengthLV() + mMobileID.lengthLV() + (mHaveLAI ? mLAI.lengthTLV() : 0);
}

ParseResult<void> L3CMReestablishmentRequest::try_parseBody(const L3Frame& src, size_t& rp) {
    // GSM 04.08 9.2.4: CKSN(4)|spare(4), CM2 LV, MI LV, optional LAI(TLV 0x13)
    // Reference: ts_CM_REESTABL_REQ has cipheringKeySequenceNumber, mobileStationClassmark2, mobileIdentityLV
    mCKSN = src.readField(rp, 4);
    src.readField(rp, 4);  // spare
    auto res = mClassmark.try_parseLV(src, rp);
    if (!res.has_value()) return res;
    res = mMobileID.try_parseLV(src, rp);
    if (!res.has_value()) return res;
    auto tlRes = mLAI.try_parseTLV(0x13, src, rp);
    if (!tlRes.has_value()) return tlRes;
    mHaveLAI = tlRes.value();
    return ParseResult<void>();
}

ParseResult<void> L3CMReestablishmentRequest::try_writeBody(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, mCKSN & 0x0F, 4);
    dest.writeField(wp, 0, 4);  // spare
    mClassmark.writeLV(dest, wp);
    mMobileID.writeLV(dest, wp);
    if (mHaveLAI) {
        mLAI.writeTLV(0x13, dest, wp);
    }
    return ParseResult<void>();
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

ParseResult<void> L3MMInformation::try_parseBody(const L3Frame& src, size_t& rp) {
    // GSM 04.08 9.2.15a: shortName(TLV 0x45), time(TV 0x47)
    auto tlRes = mShortName.try_parseTLV(0x45, src, rp);
    if (!tlRes.has_value()) return tlRes;
    auto tvRes = mTime.try_parseTV(0x47, src, rp);
    if (!tvRes.has_value()) return tvRes;
    return ParseResult<void>();
}

ParseResult<void> L3MMInformation::try_writeBody(L3Frame& dest, size_t& wp) const {
    if (mShortName.lengthV() > 1) mShortName.writeTLV(0x45, dest, wp);
    mTime.writeTV(0x47, dest, wp);
    return ParseResult<void>();
}

void L3MMInformation::text(std::ostream& os) const {
    os << "MMInformation:";
    if (mShortName.lengthV() > 1) {
        os << " shortName=(" << mShortName << ")";
    }
    os << " time=(" << mTime << ")";
}

// ── L3IdentityRequest ──────────────────────────────────────────────────

ParseResult<void> L3IdentityRequest::try_parseBody(const L3Frame& src, size_t& rp) {
    src.readField(rp, 4);  // spare
    mType = static_cast<MobileIDType>(src.readField(rp, 4));
    return ParseResult<void>();
}

ParseResult<void> L3IdentityRequest::try_writeBody(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, 0, 4);  // spare half octet
    dest.writeField(wp, static_cast<unsigned>(mType), 4);
    return ParseResult<void>();
}

void L3IdentityRequest::text(std::ostream& os) const {
    os << "IdentityRequest: type=" << static_cast<int>(mType);
}

// ── L3IdentityResponse ─────────────────────────────────────────────────

size_t L3IdentityResponse::l2BodyLength() const {
    return mMobileID.lengthLV();
}

ParseResult<void> L3IdentityResponse::try_parseBody(const L3Frame& src, size_t& rp) {
    auto res = mMobileID.try_parseLV(src, rp);
    if (!res.has_value()) return res;
    return ParseResult<void>();
}

ParseResult<void> L3IdentityResponse::try_writeBody(L3Frame& dest, size_t& wp) const {
    mMobileID.writeLV(dest, wp);
    return ParseResult<void>();
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

ParseResult<void> L3AuthenticationRequest::try_parseBody(const L3Frame& src, size_t& rp) {
    src.readField(rp, 4);  // spare
    mCKSN = src.readField(rp, 4);
    mRAND.resize(16);
    for (size_t i = 0; i < 16; ++i) {
        mRAND[i] = static_cast<uint8_t>(src.readField(rp, 8));
    }
    return ParseResult<void>();
}

ParseResult<void> L3AuthenticationRequest::try_writeBody(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, 0, 4);  // spare half octet
    dest.writeField(wp, mCKSN, 4);
    for (const auto& b : mRAND) {
        dest.writeField(wp, b, 8);
    }
    return ParseResult<void>();
}

void L3AuthenticationRequest::text(std::ostream& os) const {
    os << "AuthenticationRequest: CKSN=" << mCKSN;
    os << " RAND=";
    for (const auto& b : mRAND) {
        os << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
    }
}

// ── L3AuthenticationResponse ───────────────────────────────────────────

ParseResult<void> L3AuthenticationResponse::try_parseBody(const L3Frame& src, size_t& rp) {
    mSRES = src.readField(rp, 32);
    return ParseResult<void>();
}

ParseResult<void> L3AuthenticationResponse::try_writeBody(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, mSRES, 32);
    return ParseResult<void>();
}

void L3AuthenticationResponse::text(std::ostream& os) const {
    os << "AuthenticationResponse: SRES=0x" << std::hex << mSRES;
}

// ── L3AuthenticationReject ─────────────────────────────────────────────

void L3AuthenticationReject::text(std::ostream& os) const {
    os << "AuthenticationReject";
}

// ── L3TMSIReallocationCommand ──────────────────────────────────────────

L3TMSIReallocationCommand::L3TMSIReallocationCommand()
    : mFollowOnProceed(false) {}

// ── L3TMSIReallocationCommand Builder ──────────────────────────────────

L3TMSIReallocationCommand::Builder L3TMSIReallocationCommand::builder() { return Builder{}; }

L3TMSIReallocationCommand::Builder& L3TMSIReallocationCommand::Builder::lai(const L3LocationAreaIdentity& lai) {
    mLAI = lai;
    return *this;
}

L3TMSIReallocationCommand::Builder& L3TMSIReallocationCommand::Builder::tmsi(const L3MobileIdentity& t) {
    mTMSI = t;
    return *this;
}

L3TMSIReallocationCommand::Builder& L3TMSIReallocationCommand::Builder::followOn(bool fo) {
    mFollowOnProceed = fo;
    return *this;
}

L3TMSIReallocationCommand L3TMSIReallocationCommand::Builder::build() {
    L3TMSIReallocationCommand msg;
    msg.mLAI = mLAI;
    msg.mTMSI = mTMSI;
    msg.mFollowOnProceed = mFollowOnProceed;
    return msg;
}

ParseResult<void> L3TMSIReallocationCommand::try_writeBody(L3Frame& dest, size_t& wp) const {
    mLAI.writeV(dest, wp);
    mTMSI.writeLV(dest, wp);
    dest.writeField(wp, mFollowOnProceed ? 1 : 0, 1);
    dest.writeField(wp, 0, 7);
    return ParseResult<void>();
}

ParseResult<void> L3TMSIReallocationCommand::try_parseBody(const L3Frame& src, size_t& rp) {
    auto res = mLAI.try_parseV(src, rp);
    if (!res.has_value()) return res;
    res = mTMSI.try_parseLV(src, rp);
    if (!res.has_value()) return res;
    mFollowOnProceed = src.readField(rp, 1);
    src.readField(rp, 7);
    return ParseResult<void>();
}

void L3TMSIReallocationCommand::text(std::ostream& os) const {
    os << "TMSIReallocationCommand: ";
    mLAI.text(os);
    os << " ";
    mTMSI.text(os);
    os << " followOn=" << (mFollowOnProceed ? "1" : "0");
}

// ── L3TMSIReallocationComplete ─────────────────────────────────────────

ParseResult<void> L3TMSIReallocationComplete::try_writeBody(L3Frame&, size_t&) const { return ParseResult<void>(); }

void L3TMSIReallocationComplete::text(std::ostream& os) const {
    os << "TMSIReallocationComplete";
}

// ── L3MMStatus ─────────────────────────────────────────────────────────

ParseResult<void> L3MMStatus::try_parseBody(const L3Frame& src, size_t& rp) {
    mCause = static_cast<MMRejectCause>(src.readField(rp, 8));
    src.readField(rp, 16);  // spare
    return ParseResult<void>();
}

ParseResult<void> L3MMStatus::try_writeBody(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, static_cast<unsigned>(mCause), 8);
    dest.writeField(wp, 0, 16);  // spare
    return ParseResult<void>();
}

void L3MMStatus::text(std::ostream& os) const {
    os << "MMStatus: " << MMRejectCause2Str(mCause);
}

// ── Factory & Parser (internal) ────────────────────────────────────────

namespace detail {

ParseResult<std::unique_ptr<L3MMMessage>> L3MMFactory(int mti) {
    switch (mti) {
        case L3MMMessage::IMSIDetachIndication:     return std::make_unique<L3IMSIDetachIndication>();
        case L3MMMessage::CMServiceAccept:          return std::make_unique<L3CMServiceAccept>();
        case L3MMMessage::CMServiceAbort:           return std::make_unique<L3CMServiceAbort>();
        case L3MMMessage::CMServiceReject:          return std::make_unique<L3CMServiceReject>(MMRejectCause::Zero);
        case L3MMMessage::CMServiceRequest:         return std::make_unique<L3CMServiceRequest>();
        case L3MMMessage::CMReestablishmentRequest: return std::make_unique<L3CMReestablishmentRequest>();
        case L3MMMessage::IdentityResponse:         return std::make_unique<L3IdentityResponse>();
        case L3MMMessage::IdentityRequest:          return std::make_unique<L3IdentityRequest>(MobileIDType::NoID);
        case L3MMMessage::MMInformation:            return std::make_unique<L3MMInformation>();
        case L3MMMessage::LocationUpdatingAccept:   return std::make_unique<L3LocationUpdatingAccept>();
        case L3MMMessage::LocationUpdatingReject:   return std::make_unique<L3LocationUpdatingReject>(MMRejectCause::Zero);
        case L3MMMessage::LocationUpdatingRequest:  return std::make_unique<L3LocationUpdatingRequest>();
        case L3MMMessage::TMSIReallocationCommand:  return std::make_unique<L3TMSIReallocationCommand>();
        case L3MMMessage::TMSIReallocationComplete: return std::make_unique<L3TMSIReallocationComplete>();
        case L3MMMessage::MMStatus:                 return std::make_unique<L3MMStatus>();
        case L3MMMessage::AuthenticationRequest:    return std::make_unique<L3AuthenticationRequest>(0, std::vector<uint8_t>());
        case L3MMMessage::AuthenticationResponse:   return std::make_unique<L3AuthenticationResponse>();
        case L3MMMessage::AuthenticationReject:     return std::make_unique<L3AuthenticationReject>();
        default:
            return ParseResult<std::unique_ptr<L3MMMessage>>(
                ParseErrorCode::InvalidMTI, "Unknown MM message type: 0x" + std::to_string(mti & 0xFF));
    }
}

ParseResult<std::unique_ptr<L3MMMessage>> parseL3MM(const L3Frame& source) {
    if (source.size() < 16) {
        return ParseResult<std::unique_ptr<L3MMMessage>>(
            ParseErrorCode::TruncatedInput, "Frame too short for L3 header");
    }

    unsigned mti = source.mti();
    auto factoryResult = L3MMFactory(static_cast<L3MMMessage::MessageType>(mti));
    if (!factoryResult.has_value()) {
        GSML3PARSER_LOG_WARN("Unknown MM MTI: 0x%02x", mti);
        return ParseResult<std::unique_ptr<L3MMMessage>>(factoryResult.error());
    }

    auto parseResult = factoryResult.value()->parse(source);
    if (!parseResult.has_value()) {
        GSML3PARSER_LOG_WARN("MM parse failed for MTI=0x%02x", mti);
        return ParseResult<std::unique_ptr<L3MMMessage>>(parseResult.error());
    }

    return ParseResult<std::unique_ptr<L3MMMessage>>(std::move(factoryResult).value());
}

} // namespace detail

} // namespace gsml3parser
