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

void L3CCMessage::write(L3Frame& dest) const {
    size_t l3len = bitsNeeded();
    if (dest.size() != l3len) dest.resize(l3len);
    size_t wp = 0;
    dest.writeField(wp, mTI, 4);
    dest.writeField(wp, static_cast<unsigned>(PD()), 4);
    dest.writeField(wp, MTI(), 8);
    writeBody(dest, wp);
    dest.L2Length(l2Length());
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
    if (mHaveSignal) len += mSignal.lengthTV();
    len += ccCommonLength();
    return len;
}

void L3Setup::writeBody(L3Frame& dest, size_t& wp) const {
    if (mBearerCapability.isPresent()) mBearerCapability.writeTLV(0x04, dest, wp);
    if (mHaveCalledParty) mCalledPartyBCDNumber.writeTLV(0x5e, dest, wp);
    if (mHaveCallingParty) mCallingPartyBCDNumber.writeTLV(0x5c, dest, wp);
    if (mHaveSignal) mSignal.writeTV(0x34, dest, wp);
    ccCommonWrite(dest, wp);
}

void L3Setup::parseBody(const L3Frame& src, size_t& rp) {
    while (rp < src.size()) {
        unsigned iei = src.readField(rp, 8);
        if ((iei & 0xf0) == 0xd0) continue;
        if ((iei & 0xf0) == 0x80) continue;
        switch (iei) {
        case 4:
            mBearerCapability.parseLV(src, rp);
            continue;
        case 0x1c:
            ccCommonParse(src, rp);
            continue;
        case 0x1e:
            skipLV(src, rp);
            continue;
        case 0x34:
            mHaveSignal = true;
            mSignal.parseV(src, rp);
            continue;
        case 0x5c:
            mHaveCallingParty = true;
            mCallingPartyBCDNumber.parseLV(src, rp);
            continue;
        case 0x5d:
            skipLV(src, rp);
            continue;
        case 0x5e:
            mHaveCalledParty = true;
            mCalledPartyBCDNumber.parseLV(src, rp);
            continue;
        case 0x6d:
        case 0x74:
        case 0x75:
        case 0x7c:
        case 0x7d:
        case 0x7e:
            skipLV(src, rp);
            continue;
        case 0x7f:
            ccCommonParse(src, rp);
            continue;
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
        case 0x40:
            mSupportedCodecs.parseLV(src, rp);
            continue;
        case 0xa3:
            continue;
        default:
            skipLV(src, rp);
            continue;
        }
    }
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

// ── L3CallProceeding ───────────────────────────────────────────────────

void L3CallProceeding::writeBody(L3Frame& dest, size_t& wp) const {
    if (mBearerCapability.isPresent()) mBearerCapability.writeTLV(0x04, dest, wp);
    if (mHaveProgress) mProgress.writeTLV(0x1e, dest, wp);
}

void L3CallProceeding::parseBody(const L3Frame& src, size_t& rp) {
    // GSM 04.08 9.3.3: skip repeat indicator, bearer capability x2, facility, parse progress
    skipTV(0x0d, 4, src, rp);
    skipTLV(0x04, src, rp);
    skipTLV(0x04, src, rp);
    skipTLV(0x1c, src, rp);
    mHaveProgress = mProgress.parseTLV(0x1e, src, rp);
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

void L3Alerting::writeBody(L3Frame& dest, size_t& wp) const {
    if (mHaveProgress) mProgress.writeTLV(0x1e, dest, wp);
    ccCommonWrite(dest, wp);
}

void L3Alerting::parseBody(const L3Frame& src, size_t& rp) {
    // GSM 04.08 9.3.1: ccCommon, progress(TLV 0x1E), ccCommon again
    ccCommonParse(src, rp);
    mHaveProgress = mProgress.parseTLV(0x1e, src, rp);
    ccCommonParse(src, rp);
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

void L3Connect::writeBody(L3Frame& dest, size_t& wp) const {
    if (mHaveProgress) mProgress.writeTLV(0x1e, dest, wp);
}

void L3Connect::parseBody(const L3Frame& src, size_t& rp) {
    // GSM 04.08 9.3.5: skip facility, parse progress
    skipTLV(0x1c, src, rp);
    mHaveProgress = mProgress.parseTLV(0x1e, src, rp);
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

void L3CallConfirmed::parseBody(const L3Frame& src, size_t& rp) {
    while (rp < src.size()) {
        unsigned iei = src.readField(rp, 8);
        if ((iei & 0xf0) == 0xd0) continue;
        switch (iei) {
        case 8:
            mHaveCause = true;
            mCause.parseLV(src, rp);
            continue;
        case 0x40:
            mSupportedCodecs.parseLV(src, rp);
            continue;
        case 4:
            mBearerCapability.parseLV(src, rp);
            continue;
        case 0x15:
        case 0x2d:
            skipLV(src, rp);
            continue;
        default:
            skipLV(src, rp);
            continue;
        }
    }
}

void L3CallConfirmed::writeBody(L3Frame& dest, size_t& wp) const {
    if (mBearerCapability.isPresent()) mBearerCapability.writeTLV(0x04, dest, wp);
}

size_t L3CallConfirmed::l2BodyLength() const {
    size_t sum = 0;
    if (mBearerCapability.isPresent()) sum += mBearerCapability.lengthTLV();
    return sum;
}

void L3CallConfirmed::text(std::ostream& os) const {
    os << "CallConfirmed";
    if (mBearerCapability.isPresent()) os << " BearerCapability=(" << mBearerCapability << ")";
    if (mSupportedCodecs.isGsmPresent() || mSupportedCodecs.isUmtsPresent()) os << " SupportedCodecList=(" << mSupportedCodecs << ")";
}

// ── L3ConnectAcknowledge ───────────────────────────────────────────────

void L3ConnectAcknowledge::text(std::ostream& os) const {
    os << "ConnectAcknowledge";
}

// ── L3Disconnect ───────────────────────────────────────────────────────

void L3Disconnect::writeBody(L3Frame& dest, size_t& wp) const {
    L3CauseElement cause(mCause, mLocation);
    cause.writeLV(dest, wp);
}

void L3Disconnect::parseBody(const L3Frame& src, size_t& rp) {
    L3CauseElement cause;
    cause.parseLV(src, rp);
    mCause = cause.cause();
    mLocation = cause.location();
}

void L3Disconnect::text(std::ostream& os) const {
    os << "Disconnect: cause=" << CCCause2Str(mCause) << " loc=" << static_cast<int>(mLocation);
}

// ── L3Release ──────────────────────────────────────────────────────────

void L3Release::writeBody(L3Frame& dest, size_t& wp) const {
    if (mHaveCause) {
        L3CauseElement cause(mCause, CCCauseLocation::Private_Serving_Local);
        cause.writeTLV(0x08, dest, wp);
    }
    ccCommonWrite(dest, wp);
}

void L3Release::parseBody(const L3Frame& src, size_t& rp) {
    L3CauseElement cause;
    mHaveCause = cause.parseTLV(0x08, src, rp);
    if (mHaveCause) mCause = cause.cause();
    ccCommonParse(src, rp);
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

// ── L3ReleaseComplete ──────────────────────────────────────────────────

void L3ReleaseComplete::writeBody(L3Frame& dest, size_t& wp) const {
    if (mHaveCause) {
        L3CauseElement cause(mCause, CCCauseLocation::Private_Serving_Local);
        cause.writeTLV(0x08, dest, wp);
    }
    ccCommonWrite(dest, wp);
}

void L3ReleaseComplete::parseBody(const L3Frame& src, size_t& rp) {
    L3CauseElement cause;
    mHaveCause = cause.parseTLV(0x08, src, rp);
    if (mHaveCause) mCause = cause.cause();
    ccCommonParse(src, rp);
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

// ── L3CCStatus ─────────────────────────────────────────────────────────

void L3CCStatus::writeBody(L3Frame& dest, size_t& wp) const {
    L3CauseElement cause(mCause, CCCauseLocation::Private_Serving_Local);
    cause.writeLV(dest, wp);
    L3CallState state(mCallState);
    state.writeV(dest, wp);
}

void L3CCStatus::parseBody(const L3Frame& src, size_t& rp) {
    L3CauseElement cause;
    cause.parseLV(src, rp);
    mCause = cause.cause();
    L3CallState state;
    state.parseV(src, rp);
    mCallState = state.callState();
}

void L3CCStatus::text(std::ostream& os) const {
    os << "CCStatus: cause=" << CCCause2Str(mCause) << " state=" << mCallState;
}

// ── DTMF ────────────────────────────────────────────────────────────────

void L3StartDTMF::parseBody(const L3Frame& src, size_t& rp) {
    L3KeypadFacility kf;
    kf.parseTV(0x2c, src, rp);
    mKey = kf.IA5();
}

void L3StartDTMF::writeBody(L3Frame& dest, size_t& wp) const {
    L3KeypadFacility kf(mKey);
    kf.writeTV(0x2c, dest, wp);
}

void L3StartDTMF::text(std::ostream& os) const {
    os << "StartDTMF: key=" << mKey;
}

void L3StartDTMFAcknowledge::writeBody(L3Frame& dest, size_t& wp) const {
    L3KeypadFacility kf(mKey);
    kf.writeTV(0x2c, dest, wp);
}

void L3StartDTMFAcknowledge::text(std::ostream& os) const {
    os << "StartDTMFAck: key=" << mKey;
}

void L3StartDTMFReject::parseBody(const L3Frame& src, size_t& rp) {
    L3CauseElement cause;
    cause.parseLV(src, rp);
    mCause = cause.cause();
}

void L3StartDTMFReject::writeBody(L3Frame& dest, size_t& wp) const {
    L3CauseElement cause(mCause, CCCauseLocation::Private_Serving_Local);
    cause.writeLV(dest, wp);
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

void L3HoldReject::parseBody(const L3Frame& src, size_t& rp) {
    L3CauseElement cause;
    cause.parseLV(src, rp);
    mCause = cause.cause();
}

void L3HoldReject::writeBody(L3Frame& dest, size_t& wp) const {
    L3CauseElement cause(mCause, CCCauseLocation::Private_Serving_Local);
    cause.writeLV(dest, wp);
}

void L3HoldReject::text(std::ostream& os) const {
    os << "HoldReject: " << CCCause2Str(mCause);
}

// ── L3Progress ─────────────────────────────────────────────────────────

void L3Progress::writeBody(L3Frame& dest, size_t& wp) const {
    L3ProgressIndicator pi(static_cast<L3ProgressIndicator::Progress>(mProgress),
                           L3ProgressIndicator::Location::PrivateServingLocal);
    pi.writeLV(dest, wp);
}

void L3Progress::parseBody(const L3Frame& src, size_t& rp) {
    L3ProgressIndicator pi;
    pi.parseLV(src, rp);
    mProgress = static_cast<unsigned>(pi.progress());
}

size_t L3Progress::l2BodyLength() const { return 3; }

void L3Progress::text(std::ostream& os) const {
    os << "Progress: " << mProgress;
}

// ── Factory ─────────────────────────────────────────────────────────────

L3CCMessage* L3CCFactory(int mti) {
    switch (mti) {
        case L3CCMessage::Alerting:            return new L3Alerting();
        case L3CCMessage::CallConfirmed:       return new L3CallConfirmed();
        case L3CCMessage::CallProceeding:      return new L3CallProceeding();
        case L3CCMessage::Connect:             return new L3Connect();
        case L3CCMessage::Setup:               return new L3Setup();
        case L3CCMessage::EmergencySetup:      return new L3EmergencySetup();
        case L3CCMessage::ConnectAcknowledge:  return new L3ConnectAcknowledge();
        case L3CCMessage::Progress:            return new L3Progress(0);
        case L3CCMessage::Disconnect:          return new L3Disconnect();
        case L3CCMessage::Release:             return new L3Release();
        case L3CCMessage::ReleaseComplete:     return new L3ReleaseComplete();
        case L3CCMessage::StartDTMF:           return new L3StartDTMF();
        case L3CCMessage::StopDTMF:            return new L3StopDTMF();
        case L3CCMessage::StopDTMFAcknowledge: return new L3StopDTMFAcknowledge(0);
        case L3CCMessage::StartDTMFAcknowledge: return new L3StartDTMFAcknowledge(0, 0);
        case L3CCMessage::StartDTMFReject:     return new L3StartDTMFReject(0, CCCause::Unknown_L3_Cause);
        case L3CCMessage::Hold:                return new L3Hold();
        case L3CCMessage::HoldReject:          return new L3HoldReject(0, CCCause::Unknown_L3_Cause);
        case L3CCMessage::CCStatus:            return new L3CCStatus();
        default:                               return nullptr;
    }
}

// ── Parser ──────────────────────────────────────────────────────────────

std::unique_ptr<L3CCMessage> parseL3CC(const L3Frame& source) {
    if (source.size() < 16) return nullptr;

    // Mask out bit 6 (0xbf), see GSM 04.08 Table 10.3/3
    unsigned mti = source.MTI() & 0xbf;
    L3CCMessage* msg = L3CCFactory(static_cast<L3CCMessage::MessageType>(mti));
    if (!msg) {
        GSML3PARSER_LOG_WARN("Unknown CC MTI: 0x%02x", mti);
        return nullptr;
    }
    try {
        msg->TI(source.TI());
        msg->parse(source);
    } catch (const ParseError&) {
        GSML3PARSER_LOG_WARN("CC parse failed for MTI=0x%02x", mti);
        delete msg;
        return nullptr;
    }
    return std::unique_ptr<L3CCMessage>(msg);
}

} // namespace gsml3parser
