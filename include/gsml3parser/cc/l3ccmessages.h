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

#pragma once

#include <cstddef>
#include <cstdint>
#include <ostream>

#include "../expected.h"
#include "../bitreader.h"
#include "../bitwriter.h"
#include "../types.h"
#include "../enums.h"
#include "l3ccelements.h"

namespace gsml3parser {

// ── CC Message Type identifiers ─────────────────────────────────────────

enum class CCMessageType : uint8_t {
    Alerting           = 0x01,
    CallProceeding     = 0x02,
    Progress           = 0x03,
    Setup              = 0x05,
    Connect            = 0x07,
    CallConfirmed      = 0x08,
    EmergencySetup     = 0x0e,
    ConnectAcknowledge = 0x0f,
    Hold               = 0x18,
    HoldReject         = 0x1a,
    Disconnect         = 0x25,
    ReleaseComplete    = 0x2a,
    StartDTMF          = 0x35,
    StopDTMF           = 0x31,
    StopDTMFAcknowledge = 0x32,
    StartDTMFAcknowledge = 0x36,
    StartDTMFReject    = 0x37,
    Release            = 0x2d,
    CCStatus           = 0x3d
};

std::ostream& operator<<(std::ostream& os, CCMessageType mti);

// ── Setup (GSM 04.08 9.3.19) ──────────────────────────────────────────

class L3Setup {
    static constexpr uint8_t spareBit() { return 1; }
    unsigned mTI{7};
    bool mHaveCalledParty{false};
    L3CalledPartyBCDNumber mCalledParty;
    bool mHaveCallingParty{false};
    L3CallingPartyBCDNumber mCallingParty;
    bool mHaveSignal{false};
    L3Signal mSignal;
    bool mHaveBearerCapability{false};
    L3BearerCapability mBearerCapability;
    bool mHaveSupportedCodecs{false};
    L3SupportedCodecList mSupportedCodecs;
    bool mHaveFacility{false};
    L3SupServFacilityIE mFacility;
    bool mHaveSSVersion{false};
    L3SupServVersionIndicator mSSVersion;
    bool mHaveSubAddress{false};
    L3SubAddress mSubAddress;
    bool mHaveLowLayerCompat{false};
    L3LowLayerCompatibility mLowLayerCompat;
    bool mHaveHighLayerCompat{false};
    L3HighLayerCompatibility mHighLayerCompat;
    bool mHaveUserUser{false};
    L3UserUser mUserUser;
    bool mHaveCLIRSuppression{false};
    L3CLIRSuppression mCLIRSuppression;
    bool mHaveCLIRInvocation{false};
    L3CLIRInvocation mCLIRInvocation;
    bool mHaveCCCapabilities{false};
    L3CCCapabilities mCCCapabilities;
    bool mHaveStreamIdentifier{false};
    L3StreamIdentifier mStreamIdentifier;
    friend struct Builder;

public:
    static constexpr int MTI = 0x05;

    L3Setup() = default;

    unsigned ti() const { return mTI; }
    void ti(unsigned v) { mTI = v; }

    bool haveCalledParty() const { return mHaveCalledParty; }
    const L3CalledPartyBCDNumber& calledPartyBCDNumber() const { return mCalledParty; }
    const char* digits() const { return mCalledParty.digits(); }
    TypeOfNumber typeOfNumber() const { return mCalledParty.type(); }
    NumberingPlan numberingPlan() const { return mCalledParty.plan(); }

    bool haveCallingParty() const { return mHaveCallingParty; }
    const L3CallingPartyBCDNumber& callingPartyBCDNumber() const { return mCallingParty; }

    bool haveSignal() const { return mHaveSignal; }
    const L3Signal& signal() const { return mSignal; }

    bool haveBearerCapability() const { return mHaveBearerCapability; }
    const L3BearerCapability& bearerCapability() const { return mBearerCapability; }

    bool haveSupportedCodecs() const { return mHaveSupportedCodecs; }
    const L3SupportedCodecList& supportedCodecs() const { return mSupportedCodecs; }

    bool haveFacility() const { return mHaveFacility; }
    const L3SupServFacilityIE& facility() const { return mFacility; }

    bool haveSSVersion() const { return mHaveSSVersion; }
    const L3SupServVersionIndicator& ssVersion() const { return mSSVersion; }

    bool haveSubAddress() const { return mHaveSubAddress; }
    const L3SubAddress& subAddress() const { return mSubAddress; }

    bool haveLowLayerCompat() const { return mHaveLowLayerCompat; }
    const L3LowLayerCompatibility& lowLayerCompat() const { return mLowLayerCompat; }

    bool haveHighLayerCompat() const { return mHaveHighLayerCompat; }
    const L3HighLayerCompatibility& highLayerCompat() const { return mHighLayerCompat; }

    bool haveUserUser() const { return mHaveUserUser; }
    const L3UserUser& userUser() const { return mUserUser; }

    bool haveCLIRSuppression() const { return mHaveCLIRSuppression; }
    const L3CLIRSuppression& clirSuppression() const { return mCLIRSuppression; }

    bool haveCLIRInvocation() const { return mHaveCLIRInvocation; }
    const L3CLIRInvocation& clirInvocation() const { return mCLIRInvocation; }

    bool haveCCCapabilities() const { return mHaveCCCapabilities; }
    const L3CCCapabilities& ccCapabilities() const { return mCCCapabilities; }

    bool haveStreamIdentifier() const { return mHaveStreamIdentifier; }
    const L3StreamIdentifier& streamIdentifier() const { return mStreamIdentifier; }

    struct Builder {
        unsigned m_ti{7};
        bool m_haveCalled{false};
        L3CalledPartyBCDNumber m_called;
        Builder& ti(unsigned v) { m_ti = v; return *this; }
        Builder& calledParty(const L3CalledPartyBCDNumber& v) { m_called = v; m_haveCalled = true; return *this; }
        [[nodiscard]] L3Setup build() const;
    };
    static Builder builder();

    [[nodiscard]] static Expected<L3Setup> parse(BitReader& br);
    void write(BitWriter& bw) const;
    size_t bodyLength() const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::CallControl; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Emergency Setup (GSM 04.08 9.3.8) ─────────────────────────────────

class L3EmergencySetup {
    unsigned mTI{7};
public:
    static constexpr int MTI = 0x0e;

    L3EmergencySetup() = default;

    unsigned ti() const { return mTI; }
    void ti(unsigned v) { mTI = v; }

    [[nodiscard]] static Expected<L3EmergencySetup> parse(BitReader&);
    void write(BitWriter&) const;
    size_t bodyLength() const { return 0; }
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::CallControl; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Call Proceeding (GSM 04.08 9.3.3) ─────────────────────────────────

class L3CallProceeding {
    unsigned mTI{7};
    bool mHaveBearerCapability{false};
    L3BearerCapability mBearerCapability;
    bool mHaveProgress{false};
    L3ProgressIndicator mProgress;
    bool mHavePriority{false};
    L3Priority mPriority;
    bool mHaveNetworkCCCapabilities{false};
    L3NetworkCCCapabilities mNetworkCCCapabilities;

public:
    static constexpr int MTI = 0x02;

    L3CallProceeding() = default;

    unsigned ti() const { return mTI; }
    void ti(unsigned v) { mTI = v; }

    bool hasProgress() const { return mHaveProgress; }
    const L3ProgressIndicator& progress() const { return mProgress; }

    bool haveBearerCapability() const { return mHaveBearerCapability; }
    const L3BearerCapability& bearerCapability() const { return mBearerCapability; }

    bool havePriority() const { return mHavePriority; }
    const L3Priority& priority() const { return mPriority; }

    bool haveNetworkCCCapabilities() const { return mHaveNetworkCCCapabilities; }
    const L3NetworkCCCapabilities& networkCCCapabilities() const { return mNetworkCCCapabilities; }

    [[nodiscard]] static Expected<L3CallProceeding> parse(BitReader& br);
    void write(BitWriter& bw) const;
    size_t bodyLength() const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::CallControl; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Alerting (GSM 04.08 9.3.1) ────────────────────────────────────────

class L3Alerting {
    unsigned mTI{7};
    bool mHaveProgress{false};
    L3ProgressIndicator mProgress;
    bool mHaveFacility{false};
    L3SupServFacilityIE mFacility;
    bool mHaveSSVersion{false};
    L3SupServVersionIndicator mSSVersion;
    bool mHaveUserUser{false};
    L3UserUser mUserUser;

public:
    static constexpr int MTI = 0x01;

    L3Alerting() = default;

    unsigned ti() const { return mTI; }
    void ti(unsigned v) { mTI = v; }

    bool hasProgress() const { return mHaveProgress; }
    const L3ProgressIndicator& progress() const { return mProgress; }

    bool haveFacility() const { return mHaveFacility; }
    const L3SupServFacilityIE& facility() const { return mFacility; }

    bool haveSSVersion() const { return mHaveSSVersion; }
    const L3SupServVersionIndicator& ssVersion() const { return mSSVersion; }

    bool haveUserUser() const { return mHaveUserUser; }
    const L3UserUser& userUser() const { return mUserUser; }

    [[nodiscard]] static Expected<L3Alerting> parse(BitReader& br);
    void write(BitWriter& bw) const;
    size_t bodyLength() const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::CallControl; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Connect (GSM 04.08 9.3.5) ─────────────────────────────────────────

class L3Connect {
    unsigned mTI{7};
    bool mHaveProgress{false};
    L3ProgressIndicator mProgress;
    bool mHaveConnectedNumber{false};
    L3ConnectedNumber mConnectedNumber;
    bool mHaveConnectedSubAddress{false};
    L3SubAddress mConnectedSubAddress;
    bool mHaveUserUser{false};
    L3UserUser mUserUser;
    bool mHaveStreamIdentifier{false};
    L3StreamIdentifier mStreamIdentifier;

public:
    static constexpr int MTI = 0x07;

    L3Connect() = default;

    unsigned ti() const { return mTI; }
    void ti(unsigned v) { mTI = v; }

    bool hasProgress() const { return mHaveProgress; }
    const L3ProgressIndicator& progress() const { return mProgress; }

    bool haveConnectedNumber() const { return mHaveConnectedNumber; }
    const L3ConnectedNumber& connectedNumber() const { return mConnectedNumber; }

    bool haveConnectedSubAddress() const { return mHaveConnectedSubAddress; }
    const L3SubAddress& connectedSubAddress() const { return mConnectedSubAddress; }

    bool haveUserUser() const { return mHaveUserUser; }
    const L3UserUser& userUser() const { return mUserUser; }

    bool haveStreamIdentifier() const { return mHaveStreamIdentifier; }
    const L3StreamIdentifier& streamIdentifier() const { return mStreamIdentifier; }

    [[nodiscard]] static Expected<L3Connect> parse(BitReader& br);
    void write(BitWriter& bw) const;
    size_t bodyLength() const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::CallControl; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Connect Acknowledge (GSM 04.08 9.3.6) ─────────────────────────────

class L3ConnectAcknowledge {
    unsigned mTI{7};
public:
    static constexpr int MTI = 0x0f;

    L3ConnectAcknowledge() = default;

    unsigned ti() const { return mTI; }
    void ti(unsigned v) { mTI = v; }

    [[nodiscard]] static Expected<L3ConnectAcknowledge> parse(BitReader&);
    void write(BitWriter&) const;
    size_t bodyLength() const { return 0; }
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::CallControl; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Call Confirmed (GSM 04.08 9.3.2) ──────────────────────────────────

class L3CallConfirmed {
    unsigned mTI{7};
    bool mHaveBearerCapability{false};
    L3BearerCapability mBearerCapability;
    bool mHaveSupportedCodecs{false};
    L3SupportedCodecList mSupportedCodecs;
    bool mHaveCause{false};
    L3CauseElement mCause;
    bool mHaveUserUser{false};
    L3UserUser mUserUser;

public:
    static constexpr int MTI = 0x08;

    L3CallConfirmed() = default;

    unsigned ti() const { return mTI; }
    void ti(unsigned v) { mTI = v; }

    bool hasCause() const { return mHaveCause; }
    const L3CauseElement& cause() const { return mCause; }

    bool haveBearerCapability() const { return mHaveBearerCapability; }
    const L3BearerCapability& bearerCapability() const { return mBearerCapability; }

    bool haveSupportedCodecs() const { return mHaveSupportedCodecs; }
    const L3SupportedCodecList& supportedCodecs() const { return mSupportedCodecs; }

    bool haveUserUser() const { return mHaveUserUser; }
    const L3UserUser& userUser() const { return mUserUser; }

    [[nodiscard]] static Expected<L3CallConfirmed> parse(BitReader& br);
    void write(BitWriter& bw) const;
    size_t bodyLength() const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::CallControl; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Disconnect (GSM 04.08 9.3.7) ──────────────────────────────────────

class L3Disconnect {
    unsigned mTI{7};
    CCCause mCause{CCCause::Normal_Call_Clearing};
    CCCauseLocation mLocation{CCCauseLocation::Private_Serving_Local};

public:
    static constexpr int MTI = 0x25;

    L3Disconnect() = default;
    explicit L3Disconnect(CCCause cause)
        : mCause(cause) {}
    L3Disconnect(CCCause cause, CCCauseLocation loc)
        : mCause(cause), mLocation(loc) {}

    unsigned ti() const { return mTI; }
    void ti(unsigned v) { mTI = v; }

    CCCause cause() const { return mCause; }
    CCCauseLocation location() const { return mLocation; }

    [[nodiscard]] static Expected<L3Disconnect> parse(BitReader& br);
    void write(BitWriter& bw) const;
    size_t bodyLength() const { return 4; }
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::CallControl; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Release (GSM 04.08 9.3.19) ────────────────────────────────────────

class L3Release {
    unsigned mTI{7};
    bool mHaveCause{false};
    CCCause mCause{CCCause::Normal_Call_Clearing};
    bool mHaveFacility{false};
    L3SupServFacilityIE mFacility;
    bool mHaveSSVersion{false};
    L3SupServVersionIndicator mSSVersion;
    friend struct Builder;

public:
    static constexpr int MTI = 0x2d;

    L3Release() = default;

    unsigned ti() const { return mTI; }
    void ti(unsigned v) { mTI = v; }

    bool haveCause() const { return mHaveCause; }
    CCCause cause() const { return mCause; }

    bool haveFacility() const { return mHaveFacility; }
    const L3SupServFacilityIE& facility() const { return mFacility; }

    bool haveSSVersion() const { return mHaveSSVersion; }
    const L3SupServVersionIndicator& ssVersion() const { return mSSVersion; }

    struct Builder {
        unsigned m_ti{7};
        bool m_haveCause{false};
        CCCause m_cause{CCCause::Unknown_L3_Cause};
        Builder& ti(unsigned v) { m_ti = v; return *this; }
        Builder& cause(CCCause v) { m_cause = v; m_haveCause = true; return *this; }
        [[nodiscard]] L3Release build() const;
    };
    static Builder builder();

    [[nodiscard]] static Expected<L3Release> parse(BitReader& br);
    void write(BitWriter& bw) const;
    size_t bodyLength() const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::CallControl; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Release Complete (GSM 04.08 9.3.19) ───────────────────────────────

class L3ReleaseComplete {
    unsigned mTI{7};
    bool mHaveCause{false};
    CCCause mCause{CCCause::Normal_Call_Clearing};
    bool mHaveFacility{false};
    L3SupServFacilityIE mFacility;
    bool mHaveSSVersion{false};
    L3SupServVersionIndicator mSSVersion;
    friend struct Builder;

public:
    static constexpr int MTI = 0x2a;

    L3ReleaseComplete() = default;

    unsigned ti() const { return mTI; }
    void ti(unsigned v) { mTI = v; }

    bool haveCause() const { return mHaveCause; }
    CCCause cause() const { return mCause; }

    bool haveFacility() const { return mHaveFacility; }
    const L3SupServFacilityIE& facility() const { return mFacility; }

    bool haveSSVersion() const { return mHaveSSVersion; }
    const L3SupServVersionIndicator& ssVersion() const { return mSSVersion; }

    struct Builder {
        unsigned m_ti{7};
        bool m_haveCause{false};
        CCCause m_cause{CCCause::Unknown_L3_Cause};
        Builder& ti(unsigned v) { m_ti = v; return *this; }
        Builder& cause(CCCause v) { m_cause = v; m_haveCause = true; return *this; }
        [[nodiscard]] L3ReleaseComplete build() const;
    };
    static Builder builder();

    [[nodiscard]] static Expected<L3ReleaseComplete> parse(BitReader& br);
    void write(BitWriter& bw) const;
    size_t bodyLength() const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::CallControl; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── CC Status (GSM 04.08 9.3.19) ──────────────────────────────────────

class L3CCStatus {
    unsigned mTI{7};
    CCCause mCause{CCCause::Normal_Call_Clearing};
    unsigned mCallState{0};

public:
    struct Builder {
        unsigned m_ti{7};
        bool m_haveCause{false};
        CCCause m_cause{CCCause::Unknown_L3_Cause};
        unsigned m_callState{0};
        Builder& ti(unsigned v) { m_ti = v; return *this; }
        Builder& cause(CCCause v) { m_cause = v; m_haveCause = true; return *this; }
        Builder& callState(unsigned v) { m_callState = v; return *this; }
        [[nodiscard]] L3CCStatus build() const;
    };
    static Builder builder();

private:
    friend struct Builder;

public:
    static constexpr int MTI = 0x3d;

    L3CCStatus() = default;

    unsigned ti() const { return mTI; }
    void ti(unsigned v) { mTI = v; }

    CCCause cause() const { return mCause; }
    unsigned callState() const { return mCallState; }

    [[nodiscard]] static Expected<L3CCStatus> parse(BitReader& br);
    void write(BitWriter& bw) const;
    size_t bodyLength() const { return 5; }
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::CallControl; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── DTMF messages ──────────────────────────────────────────────────────

class L3StartDTMF {
    unsigned mTI{7};
    char mKey{0};
public:
    static constexpr int MTI = 0x35;

    L3StartDTMF() = default;

    unsigned ti() const { return mTI; }
    void ti(unsigned v) { mTI = v; }

    char key() const { return mKey; }

    [[nodiscard]] static Expected<L3StartDTMF> parse(BitReader& br);
    void write(BitWriter& bw) const;
    size_t bodyLength() const { return 2; }
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::CallControl; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

class L3StopDTMF {
    unsigned mTI{7};
public:
    static constexpr int MTI = 0x31;

    L3StopDTMF() = default;

    unsigned ti() const { return mTI; }
    void ti(unsigned v) { mTI = v; }

    [[nodiscard]] static Expected<L3StopDTMF> parse(BitReader&);
    void write(BitWriter&) const;
    size_t bodyLength() const { return 0; }
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::CallControl; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

class L3StopDTMFAcknowledge {
    unsigned mTI{7};
public:
    static constexpr int MTI = 0x32;

    L3StopDTMFAcknowledge() = default;

    unsigned ti() const { return mTI; }
    void ti(unsigned v) { mTI = v; }

    [[nodiscard]] static Expected<L3StopDTMFAcknowledge> parse(BitReader&);
    void write(BitWriter&) const;
    size_t bodyLength() const { return 0; }
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::CallControl; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

class L3StartDTMFAcknowledge {
    unsigned mTI{7};
    char mKey{0};
public:
    static constexpr int MTI = 0x36;

    L3StartDTMFAcknowledge() = default;
    explicit L3StartDTMFAcknowledge(char keypad) : mKey(keypad) {}

    unsigned ti() const { return mTI; }
    void ti(unsigned v) { mTI = v; }

    char key() const { return mKey; }

    [[nodiscard]] static Expected<L3StartDTMFAcknowledge> parse(BitReader& br);
    void write(BitWriter& bw) const;
    size_t bodyLength() const { return 2; }
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::CallControl; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

class L3StartDTMFReject {
    unsigned mTI{7};
    CCCause mCause{CCCause::Unknown_L3_Cause};
public:
    static constexpr int MTI = 0x37;

    L3StartDTMFReject() = default;
    explicit L3StartDTMFReject(CCCause cause) : mCause(cause) {}

    unsigned ti() const { return mTI; }
    void ti(unsigned v) { mTI = v; }

    CCCause cause() const { return mCause; }

    [[nodiscard]] static Expected<L3StartDTMFReject> parse(BitReader& br);
    void write(BitWriter& bw) const;
    size_t bodyLength() const { return 3; }
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::CallControl; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Hold ───────────────────────────────────────────────────────────────

class L3Hold {
    unsigned mTI{7};
public:
    static constexpr int MTI = 0x18;

    L3Hold() = default;

    unsigned ti() const { return mTI; }
    void ti(unsigned v) { mTI = v; }

    [[nodiscard]] static Expected<L3Hold> parse(BitReader&);
    void write(BitWriter&) const;
    size_t bodyLength() const { return 0; }
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::CallControl; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

class L3HoldReject {
    unsigned mTI{7};
    CCCause mCause{CCCause::Unknown_L3_Cause};
public:
    static constexpr int MTI = 0x1a;

    L3HoldReject() = default;
    explicit L3HoldReject(CCCause cause) : mCause(cause) {}

    unsigned ti() const { return mTI; }
    void ti(unsigned v) { mTI = v; }

    CCCause cause() const { return mCause; }

    [[nodiscard]] static Expected<L3HoldReject> parse(BitReader& br);
    void write(BitWriter& bw) const;
    size_t bodyLength() const { return 3; }
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::CallControl; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Progress (GSM 04.08 9.3.17) ───────────────────────────────────────

class L3Progress {
    unsigned mTI{7};
    L3ProgressIndicator mProgress;
public:
    static constexpr int MTI = 0x03;

    L3Progress() = default;

    unsigned ti() const { return mTI; }
    void ti(unsigned v) { mTI = v; }

    const L3ProgressIndicator& progress() const { return mProgress; }

    [[nodiscard]] static Expected<L3Progress> parse(BitReader&);
    void write(BitWriter&) const;
    size_t bodyLength() const { return 0; }
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::CallControl; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// CC message type names for text output.
const char* ccMessageName(int mti);

} // namespace gsml3parser
