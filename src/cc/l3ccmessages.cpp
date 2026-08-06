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

#include "gsml3parser/cc/l3ccmessages.h"
#include "gsml3parser/logger.h"
#include <sstream>
#include <iomanip>

namespace gsml3parser {

// ── L3CCMessage ─────────────────────────────────────────────────────────

ParseResult<void> L3CCMessage::write(L3Frame& dest) const {
    size_t l3len = bitsNeeded();
    if (dest.size() != l3len) dest.resize(l3len);
    size_t wp = 0;
    dest.writeField(wp, static_cast<unsigned>(pd()), 4);
    dest.writeField(wp, mTI, 3);
    dest.writeField(wp, 0, 1);
    dest.writeField(wp, mti() << 2, 8);
    auto res = try_writeBody(dest, wp);
    if (!res.has_value()) return res;
    dest.l2Length(l2Length());
    return ParseResult<void>();
}

void L3CCMessage::text(std::ostream& os) const {
    L3Message::text(os);
    os << " TI=" << mTI;
}

std::ostream& operator<<(std::ostream& os, L3CCMessage::MessageType mti) {
    switch (mti) {
        case L3CCMessage::Alerting:            os << "Alerting"; break;
        case L3CCMessage::CallConfirmed:       os << "CallConfirmed"; break;
        case L3CCMessage::CallProceeding:      os << "CallProceeding"; break;
        case L3CCMessage::Connect:             os << "Connect"; break;
        case L3CCMessage::Setup:               os << "Setup"; break;
        case L3CCMessage::EmergencySetup:      os << "EmergencySetup"; break;
        case L3CCMessage::ConnectAcknowledge:  os << "ConnectAcknowledge"; break;
        case L3CCMessage::Progress:            os << "Progress"; break;
        case L3CCMessage::Disconnect:          os << "Disconnect"; break;
        case L3CCMessage::Release:             os << "Release"; break;
        case L3CCMessage::ReleaseComplete:     os << "ReleaseComplete"; break;
        case L3CCMessage::StartDTMF:           os << "StartDTMF"; break;
        case L3CCMessage::StopDTMF:            os << "StopDTMF"; break;
        case L3CCMessage::StopDTMFAcknowledge: os << "StopDTMFAck"; break;
        case L3CCMessage::StartDTMFAcknowledge: os << "StartDTMFAck"; break;
        case L3CCMessage::StartDTMFReject:     os << "StartDTMFReject"; break;
        case L3CCMessage::Hold:                os << "Hold"; break;
        case L3CCMessage::HoldReject:          os << "HoldReject"; break;
        case L3CCMessage::CCStatus:            os << "CCStatus"; break;
        default:                                os << "Unknown_CC(" << mti << ")"; break;
    }
    return os;
}

// ── L3Setup ─────────────────────────────────────────────────────────────

size_t L3Setup::l2BodyLength() const {
    int len = 0;
    if (mBearerCapability.isPresent()) len += mBearerCapability.lengthTLV();
    if (mHaveCalledParty) len += mCalledPartyBCDNumber.lengthTLV();
    if (mHaveCallingParty) len += mCallingPartyBCDNumber.lengthTLV();
    if (mSupportedCodecs.isGsmPresent() || mSupportedCodecs.isUmtsPresent())
        len += mSupportedCodecs.lengthTLV();
    if (mHaveSignal) len += mSignal.lengthTV();
    len += ccCommonLength();
    return len;
}

ParseResult<void> L3Setup::try_writeBody(L3Frame& dest, size_t& wp) const {
    if (mBearerCapability.isPresent()) mBearerCapability.writeTLV(0x04, dest, wp);
    if (mHaveCalledParty) mCalledPartyBCDNumber.writeTLV(0x5e, dest, wp);
    if (mHaveCallingParty) mCallingPartyBCDNumber.writeTLV(0x5c, dest, wp);
    if (mSupportedCodecs.isGsmPresent() || mSupportedCodecs.isUmtsPresent())
        mSupportedCodecs.writeTLV(0x40, dest, wp);
    if (mHaveSignal) mSignal.writeTV(0x34, dest, wp);
    ccCommonWrite(dest, wp);
    return ParseResult<void>();
}

ParseResult<void> L3Setup::try_parseBody(const L3Frame& src, size_t& rp) {
    while (rp < src.size()) {
        unsigned iei = src.readField(rp, 8);
        if ((iei & 0xf0) == 0xd0) continue;
        if ((iei & 0xf0) == 0x80) continue;
        switch (iei) {
        case 4: {
            auto res = mBearerCapability.try_parseLV(src, rp);
            if (!res.has_value()) return res;
            continue;
        }
        case 0x1c: {
            auto res = try_ccCommonParse(src, rp);
            if (!res.has_value()) return res;
            continue;
        }
        case 0x1e:
            skipLV(src, rp);
            continue;
        case 0x34: {
            mHaveSignal = true;
            auto res = mSignal.try_parseV(src, rp);
            if (!res.has_value()) return res;
            continue;
        }
        case 0x5c: {
            mHaveCallingParty = true;
            auto res = mCallingPartyBCDNumber.try_parseLV(src, rp);
            if (!res.has_value()) return res;
            continue;
        }
        case 0x5d:
            skipLV(src, rp);
            continue;
        case 0x5e: {
            mHaveCalledParty = true;
            auto res = mCalledPartyBCDNumber.try_parseLV(src, rp);
            if (!res.has_value()) return res;
            continue;
        }
        case 0x6d:
        case 0x74:
        case 0x75:
        case 0x7c:
        case 0x7d:
        case 0x7e:
            skipLV(src, rp);
            continue;
        case 0x7f: {
            auto res = try_ccCommonParse(src, rp);
            if (!res.has_value()) return res;
            continue;
        }
        case 0xa1:
        case 0xa2:
            continue;
        case 0x15:
        case 0x1d:
        case 0x1b:
        case 0x2d:
        case 0x2e:
        case 0x19:
        case 0x2f:
        case 0x3a:
        case 0x41:
            skipLV(src, rp);
            continue;
        case 0x40: {
            auto res = mSupportedCodecs.try_parseLV(src, rp);
            if (!res.has_value()) return res;
            continue;
        }
        case 0xa3:
            continue;
        default:
            skipLV(src, rp);
            continue;
        }
    }
    return ParseResult<void>();
}

void L3Setup::text(std::ostream& os) const {
    os << "Setup:";
    if (mHaveCalledParty) os << " CalledParty=(" << mCalledPartyBCDNumber << ")";
    if (mHaveCallingParty) os << " CallingParty=(" << mCallingPartyBCDNumber << ")";
    if (mBearerCapability.isPresent()) os << " BearerCapability=(" << mBearerCapability << ")";
    if (mSupportedCodecs.isGsmPresent() || mSupportedCodecs.isUmtsPresent()) os << " SupportedCodecList=(" << mSupportedCodecs << ")";
    if (mHaveSignal) { os << " "; mSignal.text(os); }
    ccCommonText(os);
}

// ── L3Setup Builder ────────────────────────────────────────────────────

L3Setup::Builder::Builder(unsigned wTI) : mTI(wTI) {}

L3Setup::Builder& L3Setup::Builder::calledParty(const L3CalledPartyBCDNumber& cp) {
    mHaveCalledParty = true;
    mCalledPartyBCDNumber = cp;
    return *this;
}

L3Setup::Builder& L3Setup::Builder::callingParty(const L3CallingPartyBCDNumber& cp) {
    mHaveCallingParty = true;
    mCallingPartyBCDNumber = cp;
    return *this;
}

L3Setup::Builder& L3Setup::Builder::signal(const L3Signal& sig) {
    mHaveSignal = true;
    mSignal = sig;
    return *this;
}

L3Setup::Builder& L3Setup::Builder::bearerCapability(const L3BearerCapability& bc) {
    mBearerCapability = bc;
    return *this;
}

L3Setup::Builder& L3Setup::Builder::supportedCodecs(const L3SupportedCodecList& sc) {
    mSupportedCodecs = sc;
    return *this;
}

L3Setup L3Setup::Builder::build() {
    L3Setup msg(mTI);
    msg.mHaveCalledParty = mHaveCalledParty;
    msg.mCalledPartyBCDNumber = mCalledPartyBCDNumber;
    msg.mHaveCallingParty = mHaveCallingParty;
    msg.mCallingPartyBCDNumber = mCallingPartyBCDNumber;
    msg.mHaveSignal = mHaveSignal;
    msg.mSignal = mSignal;
    msg.mBearerCapability = mBearerCapability;
    msg.mSupportedCodecs = mSupportedCodecs;
    return msg;
}

// ── L3CallProceeding ───────────────────────────────────────────────────

ParseResult<void> L3CallProceeding::try_writeBody(L3Frame& dest, size_t& wp) const {
    if (mBearerCapability.isPresent()) mBearerCapability.writeTLV(0x04, dest, wp);
    if (mHaveProgress) mProgress.writeTLV(0x1e, dest, wp);
    return ParseResult<void>();
}

ParseResult<void> L3CallProceeding::try_parseBody(const L3Frame& src, size_t& rp) {
    // GSM 04.08 9.3.3: skip repeat indicator, bearer capability x2, facility, parse progress
    skipTV(0x0d, 4, src, rp);
    skipTLV(0x04, src, rp);
    skipTLV(0x04, src, rp);
    skipTLV(0x1c, src, rp);
    auto tlRes = mProgress.try_parseTLV(0x1e, src, rp);
    if (!tlRes.has_value()) return tlRes;
    mHaveProgress = tlRes.value();
    return ParseResult<void>();
}

size_t L3CallProceeding::l2BodyLength() const {
    size_t sum = 0;
    if (mBearerCapability.isPresent()) sum += mBearerCapability.lengthTLV();
    if (mHaveProgress) sum += mProgress.lengthTLV();
    return sum;
}

void L3CallProceeding::text(std::ostream& os) const {
    os << "CallProceeding";
    if (mBearerCapability.isPresent()) os << " BearerCapability=(" << mBearerCapability << ")";
    if (mHaveProgress) os << " Progress=(" << mProgress << ")";
}

// ── L3Alerting ─────────────────────────────────────────────────────────

ParseResult<void> L3Alerting::try_writeBody(L3Frame& dest, size_t& wp) const {
    if (mHaveProgress) mProgress.writeTLV(0x1e, dest, wp);
    ccCommonWrite(dest, wp);
    return ParseResult<void>();
}

ParseResult<void> L3Alerting::try_parseBody(const L3Frame& src, size_t& rp) {
    // GSM 04.08 9.3.1: ccCommon, progress(TLV 0x1E), ccCommon again
    auto res = try_ccCommonParse(src, rp);
    if (!res.has_value()) return res;
    auto tlRes = mProgress.try_parseTLV(0x1e, src, rp);
    if (!tlRes.has_value()) return tlRes;
    mHaveProgress = tlRes.value();
    res = try_ccCommonParse(src, rp);
    if (!res.has_value()) return res;
    return ParseResult<void>();
}

size_t L3Alerting::l2BodyLength() const {
    size_t sum = 0;
    if (mHaveProgress) sum += mProgress.lengthTLV();
    sum += ccCommonLength();
    return sum;
}

void L3Alerting::text(std::ostream& os) const {
    os << "Alerting";
    if (mHaveProgress) os << " Progress=(" << mProgress << ")";
    ccCommonText(os);
}

// ── L3Connect ──────────────────────────────────────────────────────────

ParseResult<void> L3Connect::try_writeBody(L3Frame& dest, size_t& wp) const {
    if (mHaveProgress) mProgress.writeTLV(0x1e, dest, wp);
    return ParseResult<void>();
}

ParseResult<void> L3Connect::try_parseBody(const L3Frame& src, size_t& rp) {
    // GSM 04.08 9.3.5: skip facility, parse progress
    skipTLV(0x1c, src, rp);
    auto tlRes = mProgress.try_parseTLV(0x1e, src, rp);
    if (!tlRes.has_value()) return tlRes;
    mHaveProgress = tlRes.value();
    return ParseResult<void>();
}

size_t L3Connect::l2BodyLength() const {
    size_t len = 0;
    if (mHaveProgress) len += mProgress.lengthTLV();
    return len;
}

void L3Connect::text(std::ostream& os) const {
    os << "Connect";
    if (mHaveProgress) os << " Progress=(" << mProgress << ")";
}

// ── L3CallConfirmed ────────────────────────────────────────────────────

ParseResult<void> L3CallConfirmed::try_parseBody(const L3Frame& src, size_t& rp) {
    while (rp < src.size()) {
        unsigned iei = src.readField(rp, 8);
        if ((iei & 0xf0) == 0xd0) continue;
        switch (iei) {
        case 8: {
            mHaveCause = true;
            auto res = mCause.try_parseLV(src, rp);
            if (!res.has_value()) return res;
            continue;
        }
        case 0x40: {
            auto res = mSupportedCodecs.try_parseLV(src, rp);
            if (!res.has_value()) return res;
            continue;
        }
        case 4: {
            auto res = mBearerCapability.try_parseLV(src, rp);
            if (!res.has_value()) return res;
            continue;
        }
        case 0x15:
        case 0x2d:
            skipLV(src, rp);
            continue;
        default:
            skipLV(src, rp);
            continue;
        }
    }
    return ParseResult<void>();
}

ParseResult<void> L3CallConfirmed::try_writeBody(L3Frame& dest, size_t& wp) const {
    if (mBearerCapability.isPresent()) mBearerCapability.writeTLV(0x04, dest, wp);
    if (mHaveCause) mCause.writeTLV(0x08, dest, wp);
    if (mSupportedCodecs.isGsmPresent() || mSupportedCodecs.isUmtsPresent()) mSupportedCodecs.writeTLV(0x40, dest, wp);
    return ParseResult<void>();
}

size_t L3CallConfirmed::l2BodyLength() const {
    size_t sum = 0;
    if (mBearerCapability.isPresent()) sum += mBearerCapability.lengthTLV();
    if (mHaveCause) sum += mCause.lengthTLV();
    if (mSupportedCodecs.isGsmPresent() || mSupportedCodecs.isUmtsPresent()) sum += mSupportedCodecs.lengthTLV();
    return sum;
}

void L3CallConfirmed::text(std::ostream& os) const {
    os << "CallConfirmed";
    if (mBearerCapability.isPresent()) os << " BearerCapability=(" << mBearerCapability << ")";
    if (mSupportedCodecs.isGsmPresent() || mSupportedCodecs.isUmtsPresent()) os << " SupportedCodecList=(" << mSupportedCodecs << ")";
    if (mHaveCause) os << " Cause=(" << mCause << ")";
}

// ── L3ConnectAcknowledge ───────────────────────────────────────────────

void L3ConnectAcknowledge::text(std::ostream& os) const {
    os << "ConnectAcknowledge";
}

// ── L3Disconnect ───────────────────────────────────────────────────────

ParseResult<void> L3Disconnect::try_writeBody(L3Frame& dest, size_t& wp) const {
    L3CauseElement cause(mCause, mLocation);
    cause.writeTLV(0x08, dest, wp);
    return ParseResult<void>();
}

ParseResult<void> L3Disconnect::try_parseBody(const L3Frame& src, size_t& rp) {
    L3CauseElement cause;
    auto tlRes = cause.try_parseTLV(0x08, src, rp);
    if (!tlRes.has_value()) return tlRes;
    mCause = cause.cause();
    mLocation = cause.location();
    return ParseResult<void>();
}

void L3Disconnect::text(std::ostream& os) const {
    os << "Disconnect: cause=" << CCCause2Str(mCause) << " loc=" << static_cast<int>(mLocation);
}

// ── L3Release ──────────────────────────────────────────────────────────

ParseResult<void> L3Release::try_writeBody(L3Frame& dest, size_t& wp) const {
    if (mHaveCause) {
        L3CauseElement cause(mCause, CCCauseLocation::Private_Serving_Local);
        cause.writeTLV(0x08, dest, wp);
    }
    ccCommonWrite(dest, wp);
    return ParseResult<void>();
}

ParseResult<void> L3Release::try_parseBody(const L3Frame& src, size_t& rp) {
    L3CauseElement cause;
    auto tlRes = cause.try_parseTLV(0x08, src, rp);
    if (!tlRes.has_value()) return tlRes;
    mHaveCause = tlRes.value();
    if (mHaveCause) mCause = cause.cause();
    auto res = try_ccCommonParse(src, rp);
    if (!res.has_value()) return res;
    return ParseResult<void>();
}

size_t L3Release::l2BodyLength() const {
    size_t sum = 0;
    if (mHaveCause) sum += L3CauseElement(mCause).lengthTLV();
    sum += ccCommonLength();
    return sum;
}

void L3Release::text(std::ostream& os) const {
    os << "Release";
    if (mHaveCause) os << ": cause=" << CCCause2Str(mCause);
    ccCommonText(os);
}

// ── L3Release Builder ──────────────────────────────────────────────────

L3Release::Builder::Builder(unsigned wTI) : mTI(wTI) {}

L3Release::Builder& L3Release::Builder::cause(CCCause c) {
    mHaveCause = true;
    mCause = c;
    return *this;
}

L3Release L3Release::Builder::build() {
    L3Release msg(mTI);
    msg.mHaveCause = mHaveCause;
    msg.mCause = mCause;
    return msg;
}

// ── L3ReleaseComplete ──────────────────────────────────────────────────

ParseResult<void> L3ReleaseComplete::try_writeBody(L3Frame& dest, size_t& wp) const {
    if (mHaveCause) {
        L3CauseElement cause(mCause, CCCauseLocation::Private_Serving_Local);
        cause.writeTLV(0x08, dest, wp);
    }
    ccCommonWrite(dest, wp);
    return ParseResult<void>();
}

ParseResult<void> L3ReleaseComplete::try_parseBody(const L3Frame& src, size_t& rp) {
    L3CauseElement cause;
    auto tlRes = cause.try_parseTLV(0x08, src, rp);
    if (!tlRes.has_value()) return tlRes;
    mHaveCause = tlRes.value();
    if (mHaveCause) mCause = cause.cause();
    auto res = try_ccCommonParse(src, rp);
    if (!res.has_value()) return res;
    return ParseResult<void>();
}

size_t L3ReleaseComplete::l2BodyLength() const {
    size_t sum = 0;
    if (mHaveCause) sum += L3CauseElement(mCause).lengthTLV();
    sum += ccCommonLength();
    return sum;
}

void L3ReleaseComplete::text(std::ostream& os) const {
    os << "ReleaseComplete";
    if (mHaveCause) os << ": cause=" << CCCause2Str(mCause);
    ccCommonText(os);
}

// ── L3ReleaseComplete Builder ──────────────────────────────────────────

L3ReleaseComplete::Builder::Builder(unsigned wTI) : mTI(wTI) {}

L3ReleaseComplete::Builder& L3ReleaseComplete::Builder::cause(CCCause c) {
    mHaveCause = true;
    mCause = c;
    return *this;
}

L3ReleaseComplete L3ReleaseComplete::Builder::build() {
    L3ReleaseComplete msg(mTI);
    msg.mHaveCause = mHaveCause;
    msg.mCause = mCause;
    return msg;
}

// ── L3CCStatus ─────────────────────────────────────────────────────────

ParseResult<void> L3CCStatus::try_writeBody(L3Frame& dest, size_t& wp) const {
    L3CauseElement cause(mCause, CCCauseLocation::Private_Serving_Local);
    cause.writeTLV(0x08, dest, wp);
    L3CallState state(mCallState);
    state.writeV(dest, wp);
    return ParseResult<void>();
}

ParseResult<void> L3CCStatus::try_parseBody(const L3Frame& src, size_t& rp) {
    L3CauseElement cause;
    auto tlRes = cause.try_parseTLV(0x08, src, rp);
    if (!tlRes.has_value()) return tlRes;
    mCause = cause.cause();
    L3CallState state;
    auto res = state.try_parseV(src, rp);
    if (!res.has_value()) return res;
    mCallState = state.callState();
    return ParseResult<void>();
}

void L3CCStatus::text(std::ostream& os) const {
    os << "CCStatus: cause=" << CCCause2Str(mCause) << " state=" << mCallState;
}

// ── L3CCStatus Builder ─────────────────────────────────────────────────

L3CCStatus::Builder::Builder(unsigned wTI) : mTI(wTI) {}

L3CCStatus::Builder& L3CCStatus::Builder::cause(CCCause c) {
    mCause = c;
    return *this;
}

L3CCStatus::Builder& L3CCStatus::Builder::callState(unsigned cs) {
    mCallState = cs;
    return *this;
}

L3CCStatus L3CCStatus::Builder::build() {
    L3CCStatus msg(mTI);
    msg.mCause = mCause;
    msg.mCallState = mCallState;
    return msg;
}

// ── DTMF ────────────────────────────────────────────────────────────────

ParseResult<void> L3StartDTMF::try_parseBody(const L3Frame& src, size_t& rp) {
    mKey = static_cast<char>(src.readField(rp, 8));
    return ParseResult<void>();
}

ParseResult<void> L3StartDTMF::try_writeBody(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, static_cast<unsigned>(mKey), 8);
    return ParseResult<void>();
}

void L3StartDTMF::text(std::ostream& os) const {
    os << "StartDTMF: key=" << mKey;
}

ParseResult<void> L3StartDTMFAcknowledge::try_parseBody(const L3Frame& src, size_t& rp) {
    L3KeypadFacility kf;
    auto tvRes = kf.try_parseTV(0x2c, src, rp);
    if (!tvRes.has_value()) return tvRes;
    mKey = kf.ia5();
    return ParseResult<void>();
}

ParseResult<void> L3StartDTMFAcknowledge::try_writeBody(L3Frame& dest, size_t& wp) const {
    L3KeypadFacility kf(mKey);
    kf.writeTV(0x2c, dest, wp);
    return ParseResult<void>();
}

void L3StartDTMFAcknowledge::text(std::ostream& os) const {
    os << "StartDTMFAck: key=" << mKey;
}

ParseResult<void> L3StartDTMFReject::try_parseBody(const L3Frame& src, size_t& rp) {
    L3CauseElement cause;
    auto res = cause.try_parseLV(src, rp);
    if (!res.has_value()) return res;
    mCause = cause.cause();
    return ParseResult<void>();
}

ParseResult<void> L3StartDTMFReject::try_writeBody(L3Frame& dest, size_t& wp) const {
    L3CauseElement cause(mCause, CCCauseLocation::Private_Serving_Local);
    cause.writeLV(dest, wp);
    return ParseResult<void>();
}

void L3StartDTMFReject::text(std::ostream& os) const {
    os << "StartDTMFReject: " << CCCause2Str(mCause);
}

void L3StopDTMF::text(std::ostream& os) const {
    os << "StopDTMF";
}

void L3StopDTMFAcknowledge::text(std::ostream& os) const {
    os << "StopDTMFAck";
}

// ── Hold ───────────────────────────────────────────────────────────────

void L3Hold::text(std::ostream& os) const {
    os << "Hold";
}

ParseResult<void> L3HoldReject::try_parseBody(const L3Frame& src, size_t& rp) {
    L3CauseElement cause;
    auto res = cause.try_parseLV(src, rp);
    if (!res.has_value()) return res;
    mCause = cause.cause();
    return ParseResult<void>();
}

ParseResult<void> L3HoldReject::try_writeBody(L3Frame& dest, size_t& wp) const {
    L3CauseElement cause(mCause, CCCauseLocation::Private_Serving_Local);
    cause.writeLV(dest, wp);
    return ParseResult<void>();
}

void L3HoldReject::text(std::ostream& os) const {
    os << "HoldReject: " << CCCause2Str(mCause);
}

// ── L3Progress ─────────────────────────────────────────────────────────

ParseResult<void> L3Progress::try_writeBody(L3Frame& dest, size_t& wp) const {
    L3ProgressIndicator pi(static_cast<L3ProgressIndicator::Progress>(mProgress),
                            L3ProgressIndicator::Location::PrivateServingLocal);
    pi.writeLV(dest, wp);
    return ParseResult<void>();
}

ParseResult<void> L3Progress::try_parseBody(const L3Frame& src, size_t& rp) {
    L3ProgressIndicator pi;
    auto res = pi.try_parseLV(src, rp);
    if (!res.has_value()) return res;
    mProgress = static_cast<unsigned>(pi.progress());
    return ParseResult<void>();
}

size_t L3Progress::l2BodyLength() const { return 3; }

void L3Progress::text(std::ostream& os) const {
    os << "Progress: " << mProgress;
}

// ── Factory & Parser (internal) ────────────────────────────────────────

namespace detail {

ParseResult<std::unique_ptr<L3CCMessage>> L3CCFactory(int mti) {
    switch (mti) {
        case L3CCMessage::Alerting:            return std::make_unique<L3Alerting>();
        case L3CCMessage::CallConfirmed:       return std::make_unique<L3CallConfirmed>();
        case L3CCMessage::CallProceeding:      return std::make_unique<L3CallProceeding>();
        case L3CCMessage::Connect:             return std::make_unique<L3Connect>();
        case L3CCMessage::Setup:               return std::make_unique<L3Setup>();
        case L3CCMessage::EmergencySetup:      return std::make_unique<L3EmergencySetup>();
        case L3CCMessage::ConnectAcknowledge:  return std::make_unique<L3ConnectAcknowledge>();
        case L3CCMessage::Progress:            return std::make_unique<L3Progress>(0);
        case L3CCMessage::Disconnect:          return std::make_unique<L3Disconnect>();
        case L3CCMessage::Release:             return std::make_unique<L3Release>();
        case L3CCMessage::ReleaseComplete:     return std::make_unique<L3ReleaseComplete>();
        case L3CCMessage::StartDTMF:           return std::make_unique<L3StartDTMF>();
        case L3CCMessage::StopDTMF:            return std::make_unique<L3StopDTMF>();
        case L3CCMessage::StopDTMFAcknowledge: return std::make_unique<L3StopDTMFAcknowledge>(0);
        case L3CCMessage::StartDTMFAcknowledge: return std::make_unique<L3StartDTMFAcknowledge>(0, 0);
        case L3CCMessage::StartDTMFReject:     return std::make_unique<L3StartDTMFReject>(0, CCCause::Unknown_L3_Cause);
        case L3CCMessage::Hold:                return std::make_unique<L3Hold>();
        case L3CCMessage::HoldReject:          return std::make_unique<L3HoldReject>(0, CCCause::Unknown_L3_Cause);
        case L3CCMessage::CCStatus:            return std::make_unique<L3CCStatus>();
        default:
            return ParseResult<std::unique_ptr<L3CCMessage>>(
                ParseErrorCode::InvalidMTI, "Unknown CC message type: 0x" + std::to_string(mti & 0xFF));
    }
}

ParseResult<std::unique_ptr<L3CCMessage>> parseL3CC(const L3Frame& source) {
    if (source.size() < 16) {
        return ParseResult<std::unique_ptr<L3CCMessage>>(
            ParseErrorCode::TruncatedInput, "Frame too short for L3 header");
    }

    unsigned mti = source.mti();
    auto factoryResult = L3CCFactory(static_cast<L3CCMessage::MessageType>(mti));
    if (!factoryResult.has_value()) {
        GSML3PARSER_LOG_WARN("Unknown CC MTI: 0x%02x", mti);
        return ParseResult<std::unique_ptr<L3CCMessage>>(factoryResult.error());
    }

    auto& msg = factoryResult.value();
    msg->ti(source.ti());
    auto parseResult = msg->parse(source);
    if (!parseResult.has_value()) {
        GSML3PARSER_LOG_WARN("CC parse failed for MTI=0x%02x", mti);
        return ParseResult<std::unique_ptr<L3CCMessage>>(parseResult.error());
    }

    return ParseResult<std::unique_ptr<L3CCMessage>>(std::move(msg));
}

} // namespace detail

} // namespace gsml3parser
