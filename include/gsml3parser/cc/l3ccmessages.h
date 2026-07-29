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

#ifndef GSML3PARSER_CC_L3CCMESSAGES_H
#define GSML3PARSER_CC_L3CCMESSAGES_H

#include <cstdint>
#include <memory>
#include <ostream>
#include <string>

#include "../l3message.h"
#include "../l3frame.h"
#include "../types.h"
#include "../enums.h"
#include "../common/l3common.h"
#include "l3cclements.h"

namespace gsml3parser {

// ── L3CCMessage ─────────────────────────────────────────────────────────

class L3CCMessage : public L3Message {
private:
    unsigned mTI;
public:
    enum MessageType : int {
        Alerting           = 0x01,
        CallConfirmed      = 0x08,
        CallProceeding     = 0x02,
        Connect            = 0x07,
        Setup              = 0x05,
        EmergencySetup     = 0x0e,
        ConnectAcknowledge = 0x0f,
        Progress           = 0x03,
        Disconnect         = 0x25,
        Release            = 0x2d,
        ReleaseComplete    = 0x2a,
        StartDTMF          = 0x35,
        StopDTMF           = 0x31,
        StopDTMFAcknowledge = 0x32,
        StartDTMFAcknowledge = 0x36,
        StartDTMFReject    = 0x37,
        Hold               = 0x18,
        HoldReject         = 0x1a,
        CCStatus           = 0x3d
    };

    explicit L3CCMessage(unsigned wTI = 7) : mTI(wTI) {}

    size_t fullBodyLength() const override { return l2BodyLength(); }
    void write(L3Frame& dest) const override;
    L3PD PD() const override { return L3PD::CallControl; }
    unsigned TI() const override { return mTI; }
    void TI(unsigned wTI) { mTI = wTI; }
    void text(std::ostream& os) const override;
};

std::ostream& operator<<(std::ostream& os, L3CCMessage::MessageType MTI);

// ── Setup (GSM 04.08 9.3.19) ──────────────────────────────────────────

class L3Setup : public L3CCMessage, public L3CCCapabilities, public L3CCCommonIEs {
private:
    bool mHaveCalledParty;
    L3CalledPartyBCDNumber mCalledPartyBCDNumber;
    bool mHaveCallingParty;
    L3CallingPartyBCDNumber mCallingPartyBCDNumber;
    bool mHaveSignal;
    L3Signal mSignal;
public:
    explicit L3Setup(unsigned wTI = 7)
        : L3CCMessage(wTI), mHaveCalledParty(false), mHaveCallingParty(false), mHaveSignal(false) {}
    L3Setup(unsigned wTI, const L3CalledPartyBCDNumber& wCalled)
        : L3CCMessage(wTI), mHaveCalledParty(true), mCalledPartyBCDNumber(wCalled),
          mHaveCallingParty(false), mHaveSignal(false) {}

    bool haveCalledParty() const { return mHaveCalledParty; }
    const L3CalledPartyBCDNumber& calledPartyBCDNumber() const { return mCalledPartyBCDNumber; }
    const std::string& digits() const { return mCalledPartyBCDNumber.digits(); }
    TypeOfNumber typeOfNumber() const { return mCalledPartyBCDNumber.type(); }
    NumberingPlan numberingPlan() const { return mCalledPartyBCDNumber.plan(); }

    int MTI() const override { return Setup; }
    void writeBody(L3Frame& dest, size_t& wp) const override;
    void parseBody(const L3Frame& src, size_t& rp) override;
    size_t l2BodyLength() const override;
    void text(std::ostream& os) const override;
};

// ── Emergency Setup (GSM 04.08 9.3.8) ─────────────────────────────────

class L3EmergencySetup : public L3Setup {
public:
    explicit L3EmergencySetup(unsigned wTI = 7) : L3Setup(wTI) {}
    int MTI() const override { return EmergencySetup; }
    size_t l2BodyLength() const override { return 0; }
};

// ── Call Proceeding (GSM 04.08 9.3.3) ─────────────────────────────────

class L3CallProceeding : public L3CCMessage {
public:
    explicit L3CallProceeding(unsigned wTI = 7) : L3CCMessage(wTI) {}
    int MTI() const override { return CallProceeding; }
    void writeBody(L3Frame& dest, size_t& wp) const override;
    void parseBody(const L3Frame& src, size_t& rp) override;
    size_t l2BodyLength() const override;
    void text(std::ostream& os) const override;
};

// ── Alerting (GSM 04.08 9.3.1) ────────────────────────────────────────

class L3Alerting : public L3CCMessage {
public:
    explicit L3Alerting(unsigned wTI = 7) : L3CCMessage(wTI) {}
    int MTI() const override { return Alerting; }
    void writeBody(L3Frame& dest, size_t& wp) const override;
    void parseBody(const L3Frame& src, size_t& rp) override;
    size_t l2BodyLength() const override;
    void text(std::ostream& os) const override;
};

// ── Connect (GSM 04.08 9.3.5) ─────────────────────────────────────────

class L3Connect : public L3CCMessage {
public:
    explicit L3Connect(unsigned wTI = 7) : L3CCMessage(wTI) {}
    int MTI() const override { return Connect; }
    void writeBody(L3Frame& dest, size_t& wp) const override;
    void parseBody(const L3Frame& src, size_t& rp) override;
    size_t l2BodyLength() const override;
    void text(std::ostream& os) const override;
};

// ── Connect Acknowledge (GSM 04.08 9.3.6) ─────────────────────────────

class L3ConnectAcknowledge : public L3CCMessage {
public:
    explicit L3ConnectAcknowledge(unsigned wTI = 7) : L3CCMessage(wTI) {}
    int MTI() const override { return ConnectAcknowledge; }
    size_t l2BodyLength() const override { return 0; }
    void writeBody(L3Frame&, size_t&) const override {}
    void parseBody(const L3Frame&, size_t&) override {}
    void text(std::ostream& os) const override;
};

// ── Call Confirmed (GSM 04.08 9.3.2) ──────────────────────────────────

class L3CallConfirmed : public L3CCMessage {
public:
    explicit L3CallConfirmed(unsigned wTI = 7) : L3CCMessage(wTI) {}
    int MTI() const override { return CallConfirmed; }
    void parseBody(const L3Frame& src, size_t& rp) override;
    void writeBody(L3Frame& dest, size_t& wp) const override;
    size_t l2BodyLength() const override;
    void text(std::ostream& os) const override;
};

// ── Disconnect (GSM 04.08 9.3.7) ──────────────────────────────────────

class L3Disconnect : public L3CCMessage {
private:
    CCCause mCause;
    CCCauseLocation mLocation;
public:
    L3Disconnect(unsigned wTI = 7, CCCause cause = CCCause::Normal_Call_Clearing,
                 CCCauseLocation loc = CCCauseLocation::Private_Serving_Local)
        : L3CCMessage(wTI), mCause(cause), mLocation(loc) {}

    CCCause cause() const { return mCause; }
    CCCauseLocation location() const { return mLocation; }
    int MTI() const override { return Disconnect; }
    void writeBody(L3Frame& dest, size_t& wp) const override;
    void parseBody(const L3Frame& src, size_t& rp) override;
    size_t l2BodyLength() const override { return 2; }
    void text(std::ostream& os) const override;
};

// ── Release (GSM 04.08 9.3.19) ────────────────────────────────────────

class L3Release : public L3CCMessage {
private:
    bool mHaveCause;
    CCCause mCause;
public:
    explicit L3Release(unsigned wTI = 7) : L3CCMessage(wTI), mHaveCause(false) {}
    L3Release(unsigned wTI, CCCause cause) : L3CCMessage(wTI), mHaveCause(true), mCause(cause) {}

    bool haveCause() const { return mHaveCause; }
    CCCause cause() const { return mCause; }
    int MTI() const override { return Release; }
    void writeBody(L3Frame& dest, size_t& wp) const override;
    void parseBody(const L3Frame& src, size_t& rp) override;
    size_t l2BodyLength() const override;
    void text(std::ostream& os) const override;
};

// ── Release Complete (GSM 04.08 9.3.19) ───────────────────────────────

class L3ReleaseComplete : public L3CCMessage {
private:
    bool mHaveCause;
    CCCause mCause;
public:
    explicit L3ReleaseComplete(unsigned wTI = 7) : L3CCMessage(wTI), mHaveCause(false) {}
    L3ReleaseComplete(unsigned wTI, CCCause cause) : L3CCMessage(wTI), mHaveCause(true), mCause(cause) {}

    int MTI() const override { return ReleaseComplete; }
    void writeBody(L3Frame& dest, size_t& wp) const override;
    void parseBody(const L3Frame& src, size_t& rp) override;
    size_t l2BodyLength() const override;
    void text(std::ostream& os) const override;
};

// ── CC Status (GSM 04.08 9.3.19) ──────────────────────────────────────

class L3CCStatus : public L3CCMessage {
private:
    CCCause mCause;
    unsigned mCallState;
public:
    explicit L3CCStatus(unsigned wTI = 7) : L3CCMessage(wTI) {}
    L3CCStatus(unsigned wTI, CCCause cause, unsigned callState)
        : L3CCMessage(wTI), mCause(cause), mCallState(callState) {}

    CCCause cause() const { return mCause; }
    unsigned callState() const { return mCallState; }
    int MTI() const override { return CCStatus; }
    void writeBody(L3Frame& dest, size_t& wp) const override;
    void parseBody(const L3Frame& src, size_t& rp) override;
    size_t l2BodyLength() const override { return 4; }
    void text(std::ostream& os) const override;
};

// ── DTMF messages ──────────────────────────────────────────────────────

class L3StartDTMF : public L3CCMessage {
private:
    char mKey;
public:
    explicit L3StartDTMF(unsigned wTI = 7) : L3CCMessage(wTI), mKey(0) {}
    char key() const { return mKey; }
    int MTI() const override { return StartDTMF; }
    void parseBody(const L3Frame& src, size_t& rp) override;
    void writeBody(L3Frame& dest, size_t& wp) const override;
    size_t l2BodyLength() const override { return 1; }
    void text(std::ostream& os) const override;
};

class L3StartDTMFAcknowledge : public L3CCMessage {
private:
    char mKey;
public:
    L3StartDTMFAcknowledge(unsigned wTI, char key) : L3CCMessage(wTI), mKey(key) {}
    int MTI() const override { return StartDTMFAcknowledge; }
    void writeBody(L3Frame& dest, size_t& wp) const override;
    size_t l2BodyLength() const override { return 1; }
    void text(std::ostream& os) const override;
};

class L3StartDTMFReject : public L3CCMessage {
private:
    CCCause mCause;
public:
    L3StartDTMFReject(unsigned wTI, CCCause cause) : L3CCMessage(wTI), mCause(cause) {}
    int MTI() const override { return StartDTMFReject; }
    void parseBody(const L3Frame& src, size_t& rp) override;
    void writeBody(L3Frame& dest, size_t& wp) const override;
    size_t l2BodyLength() const override { return 2; }
    void text(std::ostream& os) const override;
};

class L3StopDTMF : public L3CCMessage {
public:
    explicit L3StopDTMF(unsigned wTI = 7) : L3CCMessage(wTI) {}
    int MTI() const override { return StopDTMF; }
    void parseBody(const L3Frame&, size_t&) override {}
    void writeBody(L3Frame&, size_t&) const override {}
    size_t l2BodyLength() const override { return 0; }
    void text(std::ostream& os) const override;
};

class L3StopDTMFAcknowledge : public L3CCMessage {
public:
    explicit L3StopDTMFAcknowledge(unsigned wTI) : L3CCMessage(wTI) {}
    int MTI() const override { return StopDTMFAcknowledge; }
    void writeBody(L3Frame&, size_t&) const override {}
    void parseBody(const L3Frame&, size_t&) override {}
    size_t l2BodyLength() const override { return 0; }
    void text(std::ostream& os) const override;
};

// ── Hold ───────────────────────────────────────────────────────────────

class L3Hold : public L3CCMessage {
public:
    explicit L3Hold(unsigned wTI = 7) : L3CCMessage(wTI) {}
    int MTI() const override { return Hold; }
    void writeBody(L3Frame&, size_t&) const override {}
    void parseBody(const L3Frame&, size_t&) override {}
    size_t l2BodyLength() const override { return 0; }
    void text(std::ostream& os) const override;
};

class L3HoldReject : public L3CCMessage {
private:
    CCCause mCause;
public:
    L3HoldReject(unsigned wTI, CCCause cause) : L3CCMessage(wTI), mCause(cause) {}
    int MTI() const override { return HoldReject; }
    void parseBody(const L3Frame& src, size_t& rp) override;
    void writeBody(L3Frame& dest, size_t& wp) const override;
    size_t l2BodyLength() const override { return 2; }
    void text(std::ostream& os) const override;
};

// ── Progress (GSM 04.08 9.3.17) ───────────────────────────────────────

class L3Progress : public L3CCMessage {
private:
    unsigned mProgress;
public:
    explicit L3Progress(unsigned wTI) : L3CCMessage(wTI), mProgress(0) {}
    int MTI() const override { return Progress; }
    void writeBody(L3Frame& dest, size_t& wp) const override;
    void parseBody(const L3Frame& src, size_t& rp) override;
    size_t l2BodyLength() const override;
    void text(std::ostream& os) const override;
};

} // namespace gsml3parser

#endif // GSML3PARSER_CC_L3CCMESSAGES_H
