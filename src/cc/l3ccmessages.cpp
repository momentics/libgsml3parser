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

L3Setup::L3Setup(unsigned wTI) : L3CCMessage(wTI), mHaveCalledParty(false) {}

L3Setup::L3Setup(unsigned wTI, TypeOfNumber ton, NumberingPlan np, const std::string& digits)
    : L3CCMessage(wTI), mHaveCalledParty(true), mTON(ton), mNP(np), mDigits(digits) {}

size_t L3Setup::l2BodyLength() const {
    if (!mHaveCalledParty) return 0;
    size_t len = 1; // TON/NP
    size_t nDigits = mDigits.size();
    len += (nDigits + 1) / 2; // BCD encoded
    return len;
}

void L3Setup::writeBody(L3Frame& dest, size_t& wp) const {
    if (!mHaveCalledParty) return;
    dest.writeField(wp, static_cast<unsigned>(mTON), 3);
    dest.writeField(wp, static_cast<unsigned>(mNP), 4);
    // BCD encode
    size_t nDigits = mDigits.size();
    for (size_t i = 0; i < nDigits; i += 2) {
        unsigned hi = mDigits[i] - '0';
        unsigned lo = (i + 1 < nDigits) ? mDigits[i + 1] - '0' : 0xf;
        dest.writeField(wp, hi * 10 + lo, 8);
    }
}

void L3Setup::parseBody(const L3Frame& src, size_t& rp) {
    mTON = static_cast<TypeOfNumber>(src.readField(rp, 3));
    mNP = static_cast<NumberingPlan>(src.readField(rp, 4));
    // BCD decode
    size_t remaining = (src.size() - rp) / 8;
    for (size_t i = 0; i < remaining && i < 10; ++i) {
        unsigned bcd = src.readField(rp, 8);
        if (bcd == 0xf) break;
        mDigits += static_cast<char>('0' + (bcd / 10));
        unsigned lo = bcd % 10;
        if (lo != 0xf) mDigits += static_cast<char>('0' + lo);
    }
    mHaveCalledParty = true;
}

void L3Setup::text(std::ostream& os) const {
    os << "Setup: ";
    if (mHaveCalledParty) {
        os << mDigits;
    }
}

// ── L3CallProceeding ───────────────────────────────────────────────────

void L3CallProceeding::writeBody(L3Frame&, size_t&) const {}

void L3CallProceeding::parseBody(const L3Frame& src, size_t& rp) {
    (void)src; (void)rp;
}

size_t L3CallProceeding::l2BodyLength() const { return 0; }

void L3CallProceeding::text(std::ostream& os) const {
    os << "CallProceeding";
}

// ── L3Alerting ─────────────────────────────────────────────────────────

void L3Alerting::writeBody(L3Frame&, size_t&) const {}

void L3Alerting::parseBody(const L3Frame& src, size_t& rp) {
    (void)src; (void)rp;
}

size_t L3Alerting::l2BodyLength() const { return 0; }

void L3Alerting::text(std::ostream& os) const {
    os << "Alerting";
}

// ── L3Connect ──────────────────────────────────────────────────────────

void L3Connect::writeBody(L3Frame&, size_t&) const {}

void L3Connect::parseBody(const L3Frame& src, size_t& rp) {
    (void)src; (void)rp;
}

size_t L3Connect::l2BodyLength() const { return 0; }

void L3Connect::text(std::ostream& os) const {
    os << "Connect";
}

// ── L3CallConfirmed ────────────────────────────────────────────────────

void L3CallConfirmed::parseBody(const L3Frame& src, size_t& rp) {
    (void)src; (void)rp;
}

size_t L3CallConfirmed::l2BodyLength() const { return 0; }

void L3CallConfirmed::text(std::ostream& os) const {
    os << "CallConfirmed";
}

// ── L3Disconnect ───────────────────────────────────────────────────────

void L3Disconnect::writeBody(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, static_cast<unsigned>(mLocation), 4);
    dest.writeField(wp, static_cast<unsigned>(mCause), 7);
}

void L3Disconnect::parseBody(const L3Frame& src, size_t& rp) {
    mLocation = static_cast<CCCauseLocation>(src.readField(rp, 4));
    mCause = static_cast<CCCause>(src.readField(rp, 7));
}

void L3Disconnect::text(std::ostream& os) const {
    os << "Disconnect: cause=" << CCCause2Str(mCause);
}

// ── L3Release ──────────────────────────────────────────────────────────

void L3Release::writeBody(L3Frame& dest, size_t& wp) const {
    if (mHaveCause) {
        dest.writeField(wp, static_cast<unsigned>(mCause), 8);
    }
}

void L3Release::parseBody(const L3Frame& src, size_t& rp) {
    if (src.size() > rp / 8 + 2) {
        mHaveCause = true;
        mCause = static_cast<CCCause>(src.readField(rp, 8));
    }
}

size_t L3Release::l2BodyLength() const {
    return mHaveCause ? 1 : 0;
}

void L3Release::text(std::ostream& os) const {
    os << "Release";
    if (mHaveCause) os << ": " << CCCause2Str(mCause);
}

// ── L3ReleaseComplete ──────────────────────────────────────────────────

void L3ReleaseComplete::writeBody(L3Frame& dest, size_t& wp) const {
    if (mHaveCause) {
        dest.writeField(wp, static_cast<unsigned>(mCause), 8);
    }
}

void L3ReleaseComplete::parseBody(const L3Frame& src, size_t& rp) {
    if (src.size() > rp / 8 + 2) {
        mHaveCause = true;
        mCause = static_cast<CCCause>(src.readField(rp, 8));
    }
}

size_t L3ReleaseComplete::l2BodyLength() const {
    return mHaveCause ? 1 : 0;
}

void L3ReleaseComplete::text(std::ostream& os) const {
    os << "ReleaseComplete";
    if (mHaveCause) os << ": " << CCCause2Str(mCause);
}

// ── L3CCStatus ─────────────────────────────────────────────────────────

void L3CCStatus::writeBody(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, static_cast<unsigned>(mCause), 8);
    dest.writeField(wp, mCallState, 8);
}

void L3CCStatus::parseBody(const L3Frame& src, size_t& rp) {
    mCause = static_cast<CCCause>(src.readField(rp, 8));
    mCallState = src.readField(rp, 8);
}

void L3CCStatus::text(std::ostream& os) const {
    os << "CCStatus: cause=" << CCCause2Str(mCause) << " state=" << mCallState;
}

// ── DTMF ────────────────────────────────────────────────────────────────

void L3StartDTMF::parseBody(const L3Frame& src, size_t& rp) {
    mKey = static_cast<char>(src.readField(rp, 8));
}

void L3StartDTMF::text(std::ostream& os) const {
    os << "StartDTMF: key=" << mKey;
}

void L3StartDTMFAcknowledge::writeBody(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, static_cast<unsigned>(mKey), 8);
}

void L3StartDTMFAcknowledge::text(std::ostream& os) const {
    os << "StartDTMFAck: key=" << mKey;
}

void L3StartDTMFReject::writeBody(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, static_cast<unsigned>(mCause), 8);
}

void L3StartDTMFReject::text(std::ostream& os) const {
    os << "StartDTMFReject: " << CCCause2Str(mCause);
}

// ── Hold ───────────────────────────────────────────────────────────────

void L3HoldReject::writeBody(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, static_cast<unsigned>(mCause), 8);
}

void L3HoldReject::text(std::ostream& os) const {
    os << "HoldReject: " << CCCause2Str(mCause);
}

// ── L3Progress ─────────────────────────────────────────────────────────

void L3Progress::writeBody(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, mProgress, 8);
}

void L3Progress::parseBody(const L3Frame& src, size_t& rp) {
    mProgress = src.readField(rp, 8);
}

size_t L3Progress::l2BodyLength() const { return 1; }

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

    unsigned mti = source.MTI();
    L3CCMessage* msg = L3CCFactory(static_cast<L3CCMessage::MessageType>(mti));
    if (!msg) {
        GSML3PARSER_LOG_WARN("Unknown CC MTI: 0x%02x", mti);
        return nullptr;
    }
    try {
        msg->parse(source);
    } catch (const ParseError&) {
        GSML3PARSER_LOG_WARN("CC parse failed for MTI=0x%02x", mti);
        delete msg;
        return nullptr;
    }
    return std::unique_ptr<L3CCMessage>(msg);
}

} // namespace gsml3parser
