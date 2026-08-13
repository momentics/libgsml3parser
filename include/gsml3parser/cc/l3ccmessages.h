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
    Modify             = 0x19,
    HoldReject         = 0x1a,
    Disconnect         = 0x25,
    UnitData           = 0x27,
    UnitDataAck        = 0x28,
    ReleaseComplete    = 0x2a,
    ErrorIndication    = 0x2b,
    Release            = 0x2d,
    StartDTMF          = 0x35,
    StopDTMF           = 0x31,
    StopDTMFAcknowledge = 0x32,
    StartDTMFAcknowledge = 0x36,
    StartDTMFReject    = 0x37,
    CCStatus           = 0x3d,
    Facility           = 0x3a
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
        bool m_haveCallingParty{false};
        L3CallingPartyBCDNumber m_callingParty;
        bool m_haveSignal{false};
        L3Signal m_signal;
        bool m_haveBearerCapability{false};
        L3BearerCapability m_bearerCapability;
        bool m_haveSupportedCodecs{false};
        L3SupportedCodecList m_supportedCodecs;
        bool m_haveFacility{false};
        L3SupServFacilityIE m_facility;
        bool m_haveSSVersion{false};
        L3SupServVersionIndicator m_ssVersion;
        bool m_haveSubAddress{false};
        L3SubAddress m_subAddress;
        bool m_haveLowLayerCompat{false};
        L3LowLayerCompatibility m_lowLayerCompat;
        bool m_haveHighLayerCompat{false};
        L3HighLayerCompatibility m_highLayerCompat;
        bool m_haveUserUser{false};
        L3UserUser m_userUser;
        bool m_haveCLIRSuppression{false};
        L3CLIRSuppression m_clirSuppression;
        bool m_haveCLIRInvocation{false};
        L3CLIRInvocation m_clirInvocation;
        bool m_haveCCCapabilities{false};
        L3CCCapabilities m_ccCapabilities;
        bool m_haveStreamIdentifier{false};
        L3StreamIdentifier m_streamIdentifier;

        /// Set transaction identifier.
        Builder& ti(unsigned v) { m_ti = v; return *this; }
        /// Set called party BCD number (sets m_haveCalled flag).
        Builder& calledParty(const L3CalledPartyBCDNumber& v) { m_called = v; m_haveCalled = true; return *this; }
        /// Set calling party BCD number (sets m_haveCallingParty flag).
        Builder& callingParty(const L3CallingPartyBCDNumber& v) { m_callingParty = v; m_haveCallingParty = true; return *this; }
        /// Set signal IE (sets m_haveSignal flag).
        Builder& signal(const L3Signal& v) { m_signal = v; m_haveSignal = true; return *this; }
        /// Set bearer capability (sets m_haveBearerCapability flag).
        Builder& bearerCapability(const L3BearerCapability& v) { m_bearerCapability = v; m_haveBearerCapability = true; return *this; }
        /// Set supported codec list (sets m_haveSupportedCodecs flag).
        Builder& supportedCodecs(const L3SupportedCodecList& v) { m_supportedCodecs = v; m_haveSupportedCodecs = true; return *this; }
        /// Set facility IE (sets m_haveFacility flag).
        Builder& facility(const L3SupServFacilityIE& v) { m_facility = v; m_haveFacility = true; return *this; }
        /// Set supplementary service version indicator (sets m_haveSSVersion flag).
        Builder& ssVersion(const L3SupServVersionIndicator& v) { m_ssVersion = v; m_haveSSVersion = true; return *this; }
        /// Set sub-address (sets m_haveSubAddress flag).
        Builder& subAddress(const L3SubAddress& v) { m_subAddress = v; m_haveSubAddress = true; return *this; }
        /// Set low layer compatibility (sets m_haveLowLayerCompat flag).
        Builder& lowLayerCompat(const L3LowLayerCompatibility& v) { m_lowLayerCompat = v; m_haveLowLayerCompat = true; return *this; }
        /// Set high layer compatibility (sets m_haveHighLayerCompat flag).
        Builder& highLayerCompat(const L3HighLayerCompatibility& v) { m_highLayerCompat = v; m_haveHighLayerCompat = true; return *this; }
        /// Set user-user IE (sets m_haveUserUser flag).
        Builder& userUser(const L3UserUser& v) { m_userUser = v; m_haveUserUser = true; return *this; }
        /// Set CLIR suppression (sets m_haveCLIRSuppression flag).
        Builder& clirSuppression(const L3CLIRSuppression& v) { m_clirSuppression = v; m_haveCLIRSuppression = true; return *this; }
        /// Set CLIR invocation (sets m_haveCLIRInvocation flag).
        Builder& clirInvocation(const L3CLIRInvocation& v) { m_clirInvocation = v; m_haveCLIRInvocation = true; return *this; }
        /// Set CC capabilities (sets m_haveCCCapabilities flag).
        Builder& ccCapabilities(const L3CCCapabilities& v) { m_ccCapabilities = v; m_haveCCCapabilities = true; return *this; }
        /// Set stream identifier (sets m_haveStreamIdentifier flag).
        Builder& streamIdentifier(const L3StreamIdentifier& v) { m_streamIdentifier = v; m_haveStreamIdentifier = true; return *this; }
        /// Build the final message.
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
    friend struct Builder;
public:
    static constexpr int MTI = 0x0e;

    L3EmergencySetup() = default;

    unsigned ti() const { return mTI; }
    void ti(unsigned v) { mTI = v; }

    struct Builder {
        unsigned m_ti{7};

        /// Set transaction identifier.
        Builder& ti(unsigned v) { m_ti = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3EmergencySetup build() const;
    };
    static Builder builder();

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
    friend struct Builder;

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

    struct Builder {
        unsigned m_ti{7};
        bool m_haveBearerCapability{false};
        L3BearerCapability m_bearerCapability;
        bool m_haveProgress{false};
        L3ProgressIndicator m_progress;
        bool m_havePriority{false};
        L3Priority m_priority;
        bool m_haveNetworkCCCapabilities{false};
        L3NetworkCCCapabilities m_networkCCCapabilities;

        /// Set transaction identifier.
        Builder& ti(unsigned v) { m_ti = v; return *this; }
        /// Set bearer capability (sets m_haveBearerCapability flag).
        Builder& bearerCapability(const L3BearerCapability& v) { m_bearerCapability = v; m_haveBearerCapability = true; return *this; }
        /// Set progress indicator (sets m_haveProgress flag).
        Builder& progress(const L3ProgressIndicator& v) { m_progress = v; m_haveProgress = true; return *this; }
        /// Set priority (sets m_havePriority flag).
        Builder& priority(const L3Priority& v) { m_priority = v; m_havePriority = true; return *this; }
        /// Set network CC capabilities (sets m_haveNetworkCCCapabilities flag).
        Builder& networkCCCapabilities(const L3NetworkCCCapabilities& v) { m_networkCCCapabilities = v; m_haveNetworkCCCapabilities = true; return *this; }
        /// Build the final message.
        [[nodiscard]] L3CallProceeding build() const;
    };
    static Builder builder();

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
    friend struct Builder;

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

    struct Builder {
        unsigned m_ti{7};
        bool m_haveProgress{false};
        L3ProgressIndicator m_progress;
        bool m_haveFacility{false};
        L3SupServFacilityIE m_facility;
        bool m_haveSSVersion{false};
        L3SupServVersionIndicator m_ssVersion;
        bool m_haveUserUser{false};
        L3UserUser m_userUser;

        /// Set transaction identifier.
        Builder& ti(unsigned v) { m_ti = v; return *this; }
        /// Set progress indicator (sets m_haveProgress flag).
        Builder& progress(const L3ProgressIndicator& v) { m_progress = v; m_haveProgress = true; return *this; }
        /// Set facility IE (sets m_haveFacility flag).
        Builder& facility(const L3SupServFacilityIE& v) { m_facility = v; m_haveFacility = true; return *this; }
        /// Set SS version indicator (sets m_haveSSVersion flag).
        Builder& ssVersion(const L3SupServVersionIndicator& v) { m_ssVersion = v; m_haveSSVersion = true; return *this; }
        /// Set user-user IE (sets m_haveUserUser flag).
        Builder& userUser(const L3UserUser& v) { m_userUser = v; m_haveUserUser = true; return *this; }
        /// Build the final message.
        [[nodiscard]] L3Alerting build() const;
    };
    static Builder builder();

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
    friend struct Builder;

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

    struct Builder {
        unsigned m_ti{7};
        bool m_haveProgress{false};
        L3ProgressIndicator m_progress;
        bool m_haveConnectedNumber{false};
        L3ConnectedNumber m_connectedNumber;
        bool m_haveConnectedSubAddress{false};
        L3SubAddress m_connectedSubAddress;
        bool m_haveUserUser{false};
        L3UserUser m_userUser;
        bool m_haveStreamIdentifier{false};
        L3StreamIdentifier m_streamIdentifier;

        /// Set transaction identifier.
        Builder& ti(unsigned v) { m_ti = v; return *this; }
        /// Set progress indicator (sets m_haveProgress flag).
        Builder& progress(const L3ProgressIndicator& v) { m_progress = v; m_haveProgress = true; return *this; }
        /// Set connected number (sets m_haveConnectedNumber flag).
        Builder& connectedNumber(const L3ConnectedNumber& v) { m_connectedNumber = v; m_haveConnectedNumber = true; return *this; }
        /// Set connected sub-address (sets m_haveConnectedSubAddress flag).
        Builder& connectedSubAddress(const L3SubAddress& v) { m_connectedSubAddress = v; m_haveConnectedSubAddress = true; return *this; }
        /// Set user-user IE (sets m_haveUserUser flag).
        Builder& userUser(const L3UserUser& v) { m_userUser = v; m_haveUserUser = true; return *this; }
        /// Set stream identifier (sets m_haveStreamIdentifier flag).
        Builder& streamIdentifier(const L3StreamIdentifier& v) { m_streamIdentifier = v; m_haveStreamIdentifier = true; return *this; }
        /// Build the final message.
        [[nodiscard]] L3Connect build() const;
    };
    static Builder builder();

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
    friend struct Builder;
public:
    static constexpr int MTI = 0x0f;

    L3ConnectAcknowledge() = default;

    unsigned ti() const { return mTI; }
    void ti(unsigned v) { mTI = v; }

    struct Builder {
        unsigned m_ti{7};

        /// Set transaction identifier.
        Builder& ti(unsigned v) { m_ti = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3ConnectAcknowledge build() const;
    };
    static Builder builder();

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
    friend struct Builder;

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

    struct Builder {
        unsigned m_ti{7};
        bool m_haveBearerCapability{false};
        L3BearerCapability m_bearerCapability;
        bool m_haveSupportedCodecs{false};
        L3SupportedCodecList m_supportedCodecs;
        bool m_haveCause{false};
        L3CauseElement m_cause;
        bool m_haveUserUser{false};
        L3UserUser m_userUser;

        /// Set transaction identifier.
        Builder& ti(unsigned v) { m_ti = v; return *this; }
        /// Set bearer capability (sets m_haveBearerCapability flag).
        Builder& bearerCapability(const L3BearerCapability& v) { m_bearerCapability = v; m_haveBearerCapability = true; return *this; }
        /// Set supported codec list (sets m_haveSupportedCodecs flag).
        Builder& supportedCodecs(const L3SupportedCodecList& v) { m_supportedCodecs = v; m_haveSupportedCodecs = true; return *this; }
        /// Set cause element (sets m_haveCause flag).
        Builder& cause(const L3CauseElement& v) { m_cause = v; m_haveCause = true; return *this; }
        /// Set user-user IE (sets m_haveUserUser flag).
        Builder& userUser(const L3UserUser& v) { m_userUser = v; m_haveUserUser = true; return *this; }
        /// Build the final message.
        [[nodiscard]] L3CallConfirmed build() const;
    };
    static Builder builder();

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
    friend struct Builder;

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

    struct Builder {
        unsigned m_ti{7};
        CCCause m_cause{CCCause::Normal_Call_Clearing};
        CCCauseLocation m_location{CCCauseLocation::Private_Serving_Local};

        /// Set transaction identifier.
        Builder& ti(unsigned v) { m_ti = v; return *this; }
        /// Set CC cause.
        Builder& cause(CCCause v) { m_cause = v; return *this; }
        /// Set cause location.
        Builder& location(CCCauseLocation v) { m_location = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3Disconnect build() const;
    };
    static Builder builder();

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
        bool m_haveFacility{false};
        L3SupServFacilityIE m_facility;
        bool m_haveSSVersion{false};
        L3SupServVersionIndicator m_ssVersion;

        /// Set transaction identifier.
        Builder& ti(unsigned v) { m_ti = v; return *this; }
        /// Set CC cause (sets m_haveCause flag).
        Builder& cause(CCCause v) { m_cause = v; m_haveCause = true; return *this; }
        /// Set facility IE (sets m_haveFacility flag).
        Builder& facility(const L3SupServFacilityIE& v) { m_facility = v; m_haveFacility = true; return *this; }
        /// Set SS version indicator (sets m_haveSSVersion flag).
        Builder& ssVersion(const L3SupServVersionIndicator& v) { m_ssVersion = v; m_haveSSVersion = true; return *this; }
        /// Build the final message.
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
        bool m_haveFacility{false};
        L3SupServFacilityIE m_facility;
        bool m_haveSSVersion{false};
        L3SupServVersionIndicator m_ssVersion;

        /// Set transaction identifier.
        Builder& ti(unsigned v) { m_ti = v; return *this; }
        /// Set CC cause (sets m_haveCause flag).
        Builder& cause(CCCause v) { m_cause = v; m_haveCause = true; return *this; }
        /// Set facility IE (sets m_haveFacility flag).
        Builder& facility(const L3SupServFacilityIE& v) { m_facility = v; m_haveFacility = true; return *this; }
        /// Set SS version indicator (sets m_haveSSVersion flag).
        Builder& ssVersion(const L3SupServVersionIndicator& v) { m_ssVersion = v; m_haveSSVersion = true; return *this; }
        /// Build the final message.
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
    friend struct Builder;
public:
    static constexpr int MTI = 0x35;

    L3StartDTMF() = default;

    unsigned ti() const { return mTI; }
    void ti(unsigned v) { mTI = v; }

    char key() const { return mKey; }

    struct Builder {
        unsigned m_ti{7};
        char m_key{0};

        /// Set transaction identifier.
        Builder& ti(unsigned v) { m_ti = v; return *this; }
        /// Set DTMF key character.
        Builder& key(char v) { m_key = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3StartDTMF build() const;
    };
    static Builder builder();

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
    friend struct Builder;
public:
    static constexpr int MTI = 0x31;

    L3StopDTMF() = default;

    unsigned ti() const { return mTI; }
    void ti(unsigned v) { mTI = v; }

    struct Builder {
        unsigned m_ti{7};

        /// Set transaction identifier.
        Builder& ti(unsigned v) { m_ti = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3StopDTMF build() const;
    };
    static Builder builder();

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
    friend struct Builder;
public:
    static constexpr int MTI = 0x32;

    L3StopDTMFAcknowledge() = default;

    unsigned ti() const { return mTI; }
    void ti(unsigned v) { mTI = v; }

    struct Builder {
        unsigned m_ti{7};

        /// Set transaction identifier.
        Builder& ti(unsigned v) { m_ti = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3StopDTMFAcknowledge build() const;
    };
    static Builder builder();

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
    friend struct Builder;
public:
    static constexpr int MTI = 0x36;

    L3StartDTMFAcknowledge() = default;
    explicit L3StartDTMFAcknowledge(char keypad) : mKey(keypad) {}

    unsigned ti() const { return mTI; }
    void ti(unsigned v) { mTI = v; }

    char key() const { return mKey; }

    struct Builder {
        unsigned m_ti{7};
        char m_key{0};

        /// Set transaction identifier.
        Builder& ti(unsigned v) { m_ti = v; return *this; }
        /// Set DTMF key character.
        Builder& key(char v) { m_key = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3StartDTMFAcknowledge build() const;
    };
    static Builder builder();

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
    friend struct Builder;
public:
    static constexpr int MTI = 0x37;

    L3StartDTMFReject() = default;
    explicit L3StartDTMFReject(CCCause cause) : mCause(cause) {}

    unsigned ti() const { return mTI; }
    void ti(unsigned v) { mTI = v; }

    CCCause cause() const { return mCause; }

    struct Builder {
        unsigned m_ti{7};
        CCCause m_cause{CCCause::Unknown_L3_Cause};

        /// Set transaction identifier.
        Builder& ti(unsigned v) { m_ti = v; return *this; }
        /// Set CC cause.
        Builder& cause(CCCause v) { m_cause = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3StartDTMFReject build() const;
    };
    static Builder builder();

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
    friend struct Builder;
public:
    static constexpr int MTI = 0x18;

    L3Hold() = default;

    unsigned ti() const { return mTI; }
    void ti(unsigned v) { mTI = v; }

    struct Builder {
        unsigned m_ti{7};

        /// Set transaction identifier.
        Builder& ti(unsigned v) { m_ti = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3Hold build() const;
    };
    static Builder builder();

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
    friend struct Builder;
public:
    static constexpr int MTI = 0x1a;

    L3HoldReject() = default;
    explicit L3HoldReject(CCCause cause) : mCause(cause) {}

    unsigned ti() const { return mTI; }
    void ti(unsigned v) { mTI = v; }

    CCCause cause() const { return mCause; }

    struct Builder {
        unsigned m_ti{7};
        CCCause m_cause{CCCause::Unknown_L3_Cause};

        /// Set transaction identifier.
        Builder& ti(unsigned v) { m_ti = v; return *this; }
        /// Set CC cause.
        Builder& cause(CCCause v) { m_cause = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3HoldReject build() const;
    };
    static Builder builder();

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
    friend struct Builder;
public:
    static constexpr int MTI = 0x03;

    L3Progress() = default;

    unsigned ti() const { return mTI; }
    void ti(unsigned v) { mTI = v; }

    const L3ProgressIndicator& progress() const { return mProgress; }

    struct Builder {
        unsigned m_ti{7};
        L3ProgressIndicator m_progress;

        /// Set transaction identifier.
        Builder& ti(unsigned v) { m_ti = v; return *this; }
        /// Set progress indicator.
        Builder& progress(const L3ProgressIndicator& v) { m_progress = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3Progress build() const;
    };
    static Builder builder();

    [[nodiscard]] static Expected<L3Progress> parse(BitReader&);
    void write(BitWriter&) const;
    size_t bodyLength() const { return 0; }
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::CallControl; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// CC Facility — 3GPP TS 24.008 §9.3.21
// Direction: Both
// Carries: TI, facility body (TCAP components for supplementary services)
class L3Facility {
    unsigned mTI{7};
    std::vector<uint8_t> mFacilityBody;
    friend struct Builder;
public:
    static constexpr int MTI = 0x3a;
    L3Facility() = default;
    unsigned ti() const { return mTI; }
    void ti(unsigned v) { mTI = v; }
    const std::vector<uint8_t>& facilityBody() const { return mFacilityBody; }

    struct Builder {
        unsigned m_ti{7};
        std::vector<uint8_t> m_facilityBody;

        /// Set transaction identifier.
        Builder& ti(unsigned v) { m_ti = v; return *this; }
        /// Set facility body bytes.
        Builder& facilityBody(std::vector<uint8_t> v) { m_facilityBody = std::move(v); return *this; }
        /// Build the final message.
        [[nodiscard]] L3Facility build() const;
    };
    static Builder builder();

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3Facility> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::CallControl; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// CC Modify — 3GPP TS 24.008 §9.3.15
// Direction: Both
// Carries: TI, Bearer Capability, Called/Calling Party Number, etc.
class L3Modify {
    unsigned mTI{7};
    bool mHaveBearerCapability{false};
    L3BearerCapability mBearerCapability;
    bool mHaveCalledParty{false};
    L3CalledPartyBCDNumber mCalledParty;
    bool mHaveCallingParty{false};
    L3CallingPartyBCDNumber mCallingParty;
    friend struct Builder;
public:
    static constexpr int MTI = 0x19;
    L3Modify() = default;
    unsigned ti() const { return mTI; }
    void ti(unsigned v) { mTI = v; }
    bool haveBearerCapability() const { return mHaveBearerCapability; }
    const L3BearerCapability& bearerCapability() const { return mBearerCapability; }
    bool haveCalledParty() const { return mHaveCalledParty; }
    const L3CalledPartyBCDNumber& calledParty() const { return mCalledParty; }
    bool haveCallingParty() const { return mHaveCallingParty; }
    const L3CallingPartyBCDNumber& callingParty() const { return mCallingParty; }

    struct Builder {
        unsigned m_ti{7};
        bool m_haveBearerCapability{false};
        L3BearerCapability m_bearerCapability;
        bool m_haveCalledParty{false};
        L3CalledPartyBCDNumber m_calledParty;
        bool m_haveCallingParty{false};
        L3CallingPartyBCDNumber m_callingParty;

        /// Set transaction identifier.
        Builder& ti(unsigned v) { m_ti = v; return *this; }
        /// Set bearer capability (sets m_haveBearerCapability flag).
        Builder& bearerCapability(const L3BearerCapability& v) { m_bearerCapability = v; m_haveBearerCapability = true; return *this; }
        /// Set called party BCD number (sets m_haveCalledParty flag).
        Builder& calledParty(const L3CalledPartyBCDNumber& v) { m_calledParty = v; m_haveCalledParty = true; return *this; }
        /// Set calling party BCD number (sets m_haveCallingParty flag).
        Builder& callingParty(const L3CallingPartyBCDNumber& v) { m_callingParty = v; m_haveCallingParty = true; return *this; }
        /// Build the final message.
        [[nodiscard]] L3Modify build() const;
    };
    static Builder builder();

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3Modify> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::CallControl; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// CC UnitData — 3GPP TS 24.008 §9.3.16
// Direction: Both
// Carries: TI, User Data, Bearer Capability
class L3UnitData {
    unsigned mTI{7};
    bool mHaveBearerCapability{false};
    L3BearerCapability mBearerCapability;
    std::vector<uint8_t> mUserData;
    friend struct Builder;
public:
    static constexpr int MTI = 0x27;
    L3UnitData() = default;
    unsigned ti() const { return mTI; }
    void ti(unsigned v) { mTI = v; }
    bool haveBearerCapability() const { return mHaveBearerCapability; }
    const L3BearerCapability& bearerCapability() const { return mBearerCapability; }
    const std::vector<uint8_t>& userData() const { return mUserData; }

    struct Builder {
        unsigned m_ti{7};
        bool m_haveBearerCapability{false};
        L3BearerCapability m_bearerCapability;
        std::vector<uint8_t> m_userData;

        /// Set transaction identifier.
        Builder& ti(unsigned v) { m_ti = v; return *this; }
        /// Set bearer capability (sets m_haveBearerCapability flag).
        Builder& bearerCapability(const L3BearerCapability& v) { m_bearerCapability = v; m_haveBearerCapability = true; return *this; }
        /// Set user data bytes.
        Builder& userData(std::vector<uint8_t> v) { m_userData = std::move(v); return *this; }
        /// Build the final message.
        [[nodiscard]] L3UnitData build() const;
    };
    static Builder builder();

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3UnitData> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::CallControl; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// CC UnitDataAck — 3GPP TS 24.008 §9.3.16a
// Direction: MT
// Carries: TI
class L3UnitDataAck {
    unsigned mTI{7};
    friend struct Builder;
public:
    static constexpr int MTI = 0x28;
    L3UnitDataAck() = default;
    unsigned ti() const { return mTI; }
    void ti(unsigned v) { mTI = v; }

    struct Builder {
        unsigned m_ti{7};

        /// Set transaction identifier.
        Builder& ti(unsigned v) { m_ti = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3UnitDataAck build() const;
    };
    static Builder builder();

    size_t bodyLength() const { return 0; }
    [[nodiscard]] static Expected<L3UnitDataAck> parse(BitReader&);
    void write(BitWriter&) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::CallControl; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// CC ErrorIndication — 3GPP TS 24.008 §9.3.16b
// Direction: Both
// Carries: TI, Diagnostic
class L3ErrorIndication {
    unsigned mTI{7};
    CCCause mCause{CCCause::Normal_Call_Clearing};
    friend struct Builder;
public:
    static constexpr int MTI = 0x2b;
    L3ErrorIndication() = default;
    unsigned ti() const { return mTI; }
    void ti(unsigned v) { mTI = v; }
    CCCause cause() const { return mCause; }

    struct Builder {
        unsigned m_ti{7};
        CCCause m_cause{CCCause::Normal_Call_Clearing};

        /// Set transaction identifier.
        Builder& ti(unsigned v) { m_ti = v; return *this; }
        /// Set CC cause.
        Builder& cause(CCCause v) { m_cause = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3ErrorIndication build() const;
    };
    static Builder builder();

    size_t bodyLength() const { return 1; }
    [[nodiscard]] static Expected<L3ErrorIndication> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::CallControl; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// CC message type names for text output.
const char* ccMessageName(int mti);

} // namespace gsml3parser
