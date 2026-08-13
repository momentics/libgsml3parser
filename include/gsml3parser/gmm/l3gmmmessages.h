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

// GMM Message Classes — GSM L3 GPRS Mobility Management messages
// Spec: 3GPP TS 24.008 sections 9.4, Table 10.4
// Reference: ref/osmo-ttcn3-hacks/library/L3_Templates.ttcn — GMM message templates
//            ref/OpenBTS/SGSNGGSN/GPRSL3Messages.h — L3GmmMsg::MessageType enum
//
// L3 header (per 24.008 10.4):
//   Byte 0: PD(4)=0x08(GMM) | Skip(4)
//   Byte 1: MessageType(8 bits, raw — no NSD field)
//   Body: [message-specific fields]

#pragma once

#include <cstdint>
#include <ostream>
#include <string>
#include <vector>

#include "../expected.h"
#include "../bitreader.h"
#include "../bitwriter.h"
#include "../types.h"
#include "../enums.h"
#include "../common/l3common.h"
#include "l3gmmelements.h"

namespace gsml3parser {

// ── Attach Request (GSM 24.008 9.4.1) ─────────────────────────────────
// MS->SGSN: msNetworkCapability(LV) | attachType(4 bits) | CKSN(4 bits) |
//           drxParam(TV) | mobileIdentity(LV) | oldRoutingAreaID(raw) |
//           [msRACap(LV)] | [pTMSISignature(TV)] | ...

class L3AttachRequest {
    L3MSNetworkCapability mMsNetworkCapability;
    GMMAttachType mAttachType{GMMAttachType::GPRSAttach};
    bool mForL3{false};
    uint8_t mCKSN{0};
    L3DRXParameter mDRXParam;
    L3MobileIdentity mMobileIdentity;
    L3RoutingAreaIdentification mOldRAI;
    bool mHaveMsRACap{false};
    std::vector<uint8_t> mMsRACap;

    friend struct Builder;
public:
    static constexpr int MTI = 0x01;

    struct Builder {
        L3MSNetworkCapability m_msNetworkCapability;
        GMMAttachType m_attachType{GMMAttachType::GPRSAttach};
        bool m_forL3{false};
        uint8_t mCKSN{0};
        L3DRXParameter m_drxParam;
        L3MobileIdentity m_mobileIdentity;
        L3RoutingAreaIdentification m_oldRAI;
        bool m_haveMsRACap{false};
        std::vector<uint8_t> m_msRACap;

        /// Set MS network capability.
        Builder& msNetworkCapability(L3MSNetworkCapability v) { m_msNetworkCapability = v; return *this; }
        /// Set attach type.
        Builder& attachType(GMMAttachType v) { m_attachType = v; return *this; }
        /// Set forL3 flag.
        Builder& forL3(bool v) { m_forL3 = v; return *this; }
        /// Set CKSN value.
        Builder& cksn(uint8_t v) { mCKSN = v; return *this; }
        /// Set DRX parameter.
        Builder& drxParam(L3DRXParameter v) { m_drxParam = v; return *this; }
        /// Set mobile identity.
        Builder& mobileIdentity(L3MobileIdentity v) { m_mobileIdentity = v; return *this; }
        /// Set old routing area identification.
        Builder& oldRAI(L3RoutingAreaIdentification v) { m_oldRAI = v; return *this; }
        /// Set MS radio access capability with flag.
        Builder& msRACap(std::vector<uint8_t> v) { m_haveMsRACap = true; m_msRACap = std::move(v); return *this; }

        /// Build the final message.
        [[nodiscard]] L3AttachRequest build() const;
    };

    static Builder builder();

    GMMAttachType attachType() const { return mAttachType; }
    bool forL3() const { return mForL3; }
    uint8_t cksn() const { return mCKSN; }
    const L3MobileIdentity& mobileId() const { return mMobileIdentity; }
    const L3RoutingAreaIdentification& oldRAI() const { return mOldRAI; }
    const L3DRXParameter& drxParam() const { return mDRXParam; }
    const L3MSNetworkCapability& msNetworkCapability() const { return mMsNetworkCapability; }
    bool hasMsRACap() const { return mHaveMsRACap; }
    const std::vector<uint8_t>& msRACap() const { return mMsRACap; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3AttachRequest> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSMobilityManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Attach Accept (GSM 24.008 9.4.2) ──────────────────────────────────
// SGSN->MS: attachResult(4 bits) | forceToStandby(1) | updateTimer(2) | radioPriority(1) |
//           routingAreaIdentification(raw) | [PTMSI(TLV)] | [msIdentity(TLV)] | ...

class L3AttachAccept {
    GMMAttachType mAttachResult{GMMAttachType::GPRSAttach};
    bool mForceToStandby{false};
    uint8_t mUpdateTimer{0};
    uint8_t mRadioPriority{0};
    L3RoutingAreaIdentification mRAI;
    bool mHavePTMSI{false};
    L3MobileIdentity mPTMSI;

    friend struct Builder;
public:
    static constexpr int MTI = 0x02;

    struct Builder {
        GMMAttachType m_attachResult{GMMAttachType::GPRSAttach};
        bool m_forceToStandby{false};
        uint8_t m_updateTimer{0};
        uint8_t m_radioPriority{0};
        L3RoutingAreaIdentification m_rai;
        bool m_havePTMSI{false};
        L3MobileIdentity m_ptmsi;

        /// Set attach result type.
        Builder& attachResult(GMMAttachType v) { m_attachResult = v; return *this; }
        /// Set force to standby flag.
        Builder& forceToStandby(bool v) { m_forceToStandby = v; return *this; }
        /// Set update timer value.
        Builder& updateTimer(uint8_t v) { m_updateTimer = v; return *this; }
        /// Set radio priority value.
        Builder& radioPriority(uint8_t v) { m_radioPriority = v; return *this; }
        /// Set routing area identification.
        Builder& rai(L3RoutingAreaIdentification v) { m_rai = v; return *this; }
        /// Set PTMSI with flag.
        Builder& ptmsi(L3MobileIdentity v) { m_havePTMSI = true; m_ptmsi = std::move(v); return *this; }

        /// Build the final message.
        [[nodiscard]] L3AttachAccept build() const;
    };

    static Builder builder();

    GMMAttachType attachResult() const { return mAttachResult; }
    bool forceToStandby() const { return mForceToStandby; }
    uint8_t updateTimer() const { return mUpdateTimer; }
    uint8_t radioPriority() const { return mRadioPriority; }
    const L3RoutingAreaIdentification& rai() const { return mRAI; }
    bool hasPTMSI() const { return mHavePTMSI; }
    const L3MobileIdentity& ptmsi() const { return mPTMSI; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3AttachAccept> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSMobilityManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Attach Complete (GSM 24.008 9.4.3) ────────────────────────────────
// MS->SGSN: no mandatory body fields (may contain optional IEs)

class L3AttachComplete {
public:
    static constexpr int MTI = 0x03;

    struct Builder {
        /// Build the final message.
        [[nodiscard]] L3AttachComplete build() const;
    };

    static Builder builder();

    size_t bodyLength() const { return 0; }
    [[nodiscard]] static Expected<L3AttachComplete> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSMobilityManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Attach Reject (GSM 24.008 9.4.4) ──────────────────────────────────
// SGSN->MS: gmmCause(TLV) | [T3302(TLV)] | [T3346(TLV)]

class L3AttachReject {
    GMMCause mCause{GMMCause::Unspecified};
    bool mHaveT3302{false};
    L3T3302Timer mT3302;

    friend struct Builder;
public:
    static constexpr int MTI = 0x04;

    struct Builder {
        GMMCause m_cause{GMMCause::Unspecified};
        bool m_haveT3302{false};
        L3T3302Timer m_t3302;

        /// Set GMM cause.
        Builder& cause(GMMCause v) { m_cause = v; return *this; }
        /// Set T3302 timer with flag.
        Builder& t3302(L3T3302Timer v) { m_haveT3302 = true; m_t3302 = v; return *this; }

        /// Build the final message.
        [[nodiscard]] L3AttachReject build() const;
    };

    static Builder builder();

    GMMCause cause() const { return mCause; }
    bool hasT3302() const { return mHaveT3302; }
    const L3T3302Timer& t3302() const { return mT3302; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3AttachReject> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSMobilityManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Detach Request (GSM 24.008 9.4.5) ─────────────────────────────────
// MS->SGSN or SGSN->MS: detachType(4 bits) | spare(4) | [PTMSI(TLV)] | [PTMSISignature(TLV)]
// SGSN->MS additionally: forceToStandby(1)|spare(3) | gmmCause(TLV)

class L3DetachRequest {
    uint8_t mDetachType{0};
    bool mPowerOff{false};
    bool mForceToStandby{false};
    bool mHavePTMSI{false};
    L3MobileIdentity mPTMSI;
    GMMCause mCause{GMMCause::Unspecified};

    friend struct Builder;
public:
    static constexpr int MTI = 0x05;

    struct Builder {
        uint8_t m_detachType{0};
        bool m_powerOff{false};
        bool m_forceToStandby{false};
        bool m_havePTMSI{false};
        L3MobileIdentity m_ptmsi;
        GMMCause m_cause{GMMCause::Unspecified};

        /// Set detach type.
        Builder& detachType(uint8_t v) { m_detachType = v; return *this; }
        /// Set power off flag.
        Builder& powerOff(bool v) { m_powerOff = v; return *this; }
        /// Set force to standby flag.
        Builder& forceToStandby(bool v) { m_forceToStandby = v; return *this; }
        /// Set PTMSI with flag.
        Builder& ptmsi(L3MobileIdentity v) { m_havePTMSI = true; m_ptmsi = std::move(v); return *this; }
        /// Set GMM cause.
        Builder& cause(GMMCause v) { m_cause = v; return *this; }

        /// Build the final message.
        [[nodiscard]] L3DetachRequest build() const;
    };

    static Builder builder();

    uint8_t detachType() const { return mDetachType & 0x07; }
    bool powerOff() const { return mPowerOff; }
    bool forceToStandby() const { return mForceToStandby; }
    bool hasPTMSI() const { return mHavePTMSI; }
    const L3MobileIdentity& ptmsi() const { return mPTMSI; }
    GMMCause cause() const { return mCause; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3DetachRequest> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSMobilityManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Detach Accept (GSM 24.008 9.4.6) ──────────────────────────────────
// SGSN->MS: forceToStandby(1)|spare(7)
// MS->SGSN: no mandatory body

class L3DetachAccept {
    bool mForceToStandby{false};

    friend struct Builder;
public:
    static constexpr int MTI = 0x06;

    struct Builder {
        bool m_forceToStandby{false};

        /// Set force to standby flag.
        Builder& forceToStandby(bool v) { m_forceToStandby = v; return *this; }

        /// Build the final message.
        [[nodiscard]] L3DetachAccept build() const;
    };

    static Builder builder();

    bool forceToStandby() const { return mForceToStandby; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3DetachAccept> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSMobilityManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Routing Area Update Request (GSM 24.008 9.4.12) ───────────────────
// MS->SGSN: updateType(4 bits) | CKSN(4 bits) | oldRoutingAreaID(raw) |
//           [msRACap(LV)] | [oldPTMSISignature(TV)] | ...

class L3RoutingAreaUpdateRequest {
    GMMUpdateType mUpdateType{GMMUpdateType::RAUpdated};
    bool mForL3{false};
    uint8_t mCKSN{0};
    L3RoutingAreaIdentification mOldRAI;
    bool mHaveMsRACap{false};
    std::vector<uint8_t> mMsRACap;

    friend struct Builder;
public:
    static constexpr int MTI = 0x08;

    struct Builder {
        GMMUpdateType m_updateType{GMMUpdateType::RAUpdated};
        bool m_forL3{false};
        uint8_t mCKSN{0};
        L3RoutingAreaIdentification m_oldRAI;
        bool m_haveMsRACap{false};
        std::vector<uint8_t> m_msRACap;

        /// Set update type.
        Builder& updateType(GMMUpdateType v) { m_updateType = v; return *this; }
        /// Set forL3 flag.
        Builder& forL3(bool v) { m_forL3 = v; return *this; }
        /// Set CKSN value.
        Builder& cksn(uint8_t v) { mCKSN = v; return *this; }
        /// Set old routing area identification.
        Builder& oldRAI(L3RoutingAreaIdentification v) { m_oldRAI = v; return *this; }
        /// Set MS radio access capability with flag.
        Builder& msRACap(std::vector<uint8_t> v) { m_haveMsRACap = true; m_msRACap = std::move(v); return *this; }

        /// Build the final message.
        [[nodiscard]] L3RoutingAreaUpdateRequest build() const;
    };

    static Builder builder();

    GMMUpdateType updateType() const { return mUpdateType; }
    bool forL3() const { return mForL3; }
    uint8_t cksn() const { return mCKSN; }
    const L3RoutingAreaIdentification& oldRAI() const { return mOldRAI; }
    bool hasMsRACap() const { return mHaveMsRACap; }
    const std::vector<uint8_t>& msRACap() const { return mMsRACap; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3RoutingAreaUpdateRequest> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSMobilityManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Routing Area Update Accept (GSM 24.008 9.4.15) ────────────────────
// SGSN->MS: forceToStandby(1)|updateResult(3)|spare(1)|raUpdateTimer(2)|radioPriority(1) |
//           routingAreaId(raw) | [allocatedPTMSI(TLV)] | ...

class L3RoutingAreaUpdateAccept {
    bool mForceToStandby{false};
    GMMUpdateType mUpdateResult{GMMUpdateType::RAUpdated};
    uint8_t mRAUpdateTimer{0};
    uint8_t mRadioPriority{0};
    L3RoutingAreaIdentification mRAI;
    bool mHavePTMSI{false};
    L3MobileIdentity mPTMSI;

    friend struct Builder;
public:
    static constexpr int MTI = 0x09;

    struct Builder {
        bool m_forceToStandby{false};
        GMMUpdateType m_updateResult{GMMUpdateType::RAUpdated};
        uint8_t m_raUpdateTimer{0};
        uint8_t m_radioPriority{0};
        L3RoutingAreaIdentification m_rai;
        bool m_havePTMSI{false};
        L3MobileIdentity m_ptmsi;

        /// Set force to standby flag.
        Builder& forceToStandby(bool v) { m_forceToStandby = v; return *this; }
        /// Set update result type.
        Builder& updateResult(GMMUpdateType v) { m_updateResult = v; return *this; }
        /// Set RA update timer value.
        Builder& raUpdateTimer(uint8_t v) { m_raUpdateTimer = v; return *this; }
        /// Set radio priority value.
        Builder& radioPriority(uint8_t v) { m_radioPriority = v; return *this; }
        /// Set routing area identification.
        Builder& rai(L3RoutingAreaIdentification v) { m_rai = v; return *this; }
        /// Set PTMSI with flag.
        Builder& ptmsi(L3MobileIdentity v) { m_havePTMSI = true; m_ptmsi = std::move(v); return *this; }

        /// Build the final message.
        [[nodiscard]] L3RoutingAreaUpdateAccept build() const;
    };

    static Builder builder();

    bool forceToStandby() const { return mForceToStandby; }
    GMMUpdateType updateResult() const { return mUpdateResult; }
    uint8_t raUpdateTimer() const { return mRAUpdateTimer; }
    uint8_t radioPriority() const { return mRadioPriority; }
    const L3RoutingAreaIdentification& rai() const { return mRAI; }
    bool hasPTMSI() const { return mHavePTMSI; }
    const L3MobileIdentity& ptmsi() const { return mPTMSI; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3RoutingAreaUpdateAccept> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSMobilityManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Routing Area Update Complete (GSM 24.008 9.4.16) ──────────────────
// MS->SGSN: no mandatory body fields

class L3RoutingAreaUpdateComplete {
public:
    static constexpr int MTI = 0x0a;

    struct Builder {
        /// Build the final message.
        [[nodiscard]] L3RoutingAreaUpdateComplete build() const;
    };

    static Builder builder();

    size_t bodyLength() const { return 0; }
    [[nodiscard]] static Expected<L3RoutingAreaUpdateComplete> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSMobilityManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Routing Area Update Reject (GSM 24.008 9.4.17) ────────────────────
// SGSN->MS: forceToStandby(1)|spare(3)|gmmCause(4) — actually cause is TLV

class L3RoutingAreaUpdateReject {
    bool mForceToStandby{false};
    GMMCause mCause{GMMCause::Unspecified};
    bool mHaveT3302{false};
    L3T3302Timer mT3302;

    friend struct Builder;
public:
    static constexpr int MTI = 0x0b;

    struct Builder {
        bool m_forceToStandby{false};
        GMMCause m_cause{GMMCause::Unspecified};
        bool m_haveT3302{false};
        L3T3302Timer m_t3302;

        /// Set force to standby flag.
        Builder& forceToStandby(bool v) { m_forceToStandby = v; return *this; }
        /// Set GMM cause.
        Builder& cause(GMMCause v) { m_cause = v; return *this; }
        /// Set T3302 timer with flag.
        Builder& t3302(L3T3302Timer v) { m_haveT3302 = true; m_t3302 = v; return *this; }

        /// Build the final message.
        [[nodiscard]] L3RoutingAreaUpdateReject build() const;
    };

    static Builder builder();

    bool forceToStandby() const { return mForceToStandby; }
    GMMCause cause() const { return mCause; }
    bool hasT3302() const { return mHaveT3302; }
    const L3T3302Timer& t3302() const { return mT3302; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3RoutingAreaUpdateReject> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSMobilityManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Service Request (GSM 24.008 9.4.20) ───────────────────────────────
// MS->SGSN: CKSN(3)|spare(1) | serviceType(3)|spare(1) | PTMSI(LV) | [PDPContextStatus(TLV)]

class L3ServiceRequest {
    uint8_t mCKSN{0};
    uint8_t mServiceType{0};
    L3MobileIdentity mPTMSI;

    friend struct Builder;
public:
    static constexpr int MTI = 0x0c;

    struct Builder {
        uint8_t mCKSN{0};
        uint8_t m_serviceType{0};
        L3MobileIdentity m_ptmsi;

        /// Set CKSN value.
        Builder& cksn(uint8_t v) { mCKSN = v; return *this; }
        /// Set service type.
        Builder& serviceType(uint8_t v) { m_serviceType = v; return *this; }
        /// Set PTMSI.
        Builder& ptmsi(L3MobileIdentity v) { m_ptmsi = std::move(v); return *this; }

        /// Build the final message.
        [[nodiscard]] L3ServiceRequest build() const;
    };

    static Builder builder();

    uint8_t cksn() const { return mCKSN; }
    uint8_t serviceType() const { return mServiceType; }
    const L3MobileIdentity& ptmsi() const { return mPTMSI; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3ServiceRequest> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSMobilityManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Service Accept (GSM 24.008 9.4.21) ────────────────────────────────
// SGSN->MS: [PDPContextStatus(TLV)]

class L3ServiceAccept {
public:
    static constexpr int MTI = 0x0d;

    struct Builder {
        /// Build the final message.
        [[nodiscard]] L3ServiceAccept build() const;
    };

    static Builder builder();

    size_t bodyLength() const { return 0; }
    [[nodiscard]] static Expected<L3ServiceAccept> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSMobilityManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Service Reject (GSM 24.008 9.4.22) ────────────────────────────────
// SGSN->MS: gmmCause(TLV) | [T3346(TLV)]

class L3ServiceReject {
    GMMCause mCause{GMMCause::Unspecified};

    friend struct Builder;
public:
    static constexpr int MTI = 0x0e;

    struct Builder {
        GMMCause m_cause{GMMCause::Unspecified};

        /// Set GMM cause.
        Builder& cause(GMMCause v) { m_cause = v; return *this; }

        /// Build the final message.
        [[nodiscard]] L3ServiceReject build() const;
    };

    static Builder builder();

    GMMCause cause() const { return mCause; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3ServiceReject> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSMobilityManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── P-TMSI Reallocation Command (GSM 24.008 9.4.8) ────────────────────
// SGSN->MS: PTMSI_Type(1)|spare(3) | forceToStandby(1)|spare(6) |
//           routingAreaId(raw 6 octets) | [allocatedPTMSI(TLV)]

class L3P_TMSIReallocationCommand {
    GMMPTMSIType mPTMSIType{GMMPTMSIType::Native};
    bool mForceToStandby{false};
    L3RoutingAreaIdentification mRAI;
    bool mHavePTMSI{false};
    L3MobileIdentity mPTMSI;

    friend struct Builder;
public:
    static constexpr int MTI = 0x10;

    struct Builder {
        GMMPTMSIType m_ptmsiType{GMMPTMSIType::Native};
        bool m_forceToStandby{false};
        L3RoutingAreaIdentification m_rai;
        bool m_havePTMSI{false};
        L3MobileIdentity m_ptmsi;

        /// Set PTMSI type.
        Builder& ptmsiType(GMMPTMSIType v) { m_ptmsiType = v; return *this; }
        /// Set force to standby flag.
        Builder& forceToStandby(bool v) { m_forceToStandby = v; return *this; }
        /// Set routing area identification.
        Builder& rai(L3RoutingAreaIdentification v) { m_rai = v; return *this; }
        /// Set PTMSI with flag.
        Builder& ptmsi(L3MobileIdentity v) { m_havePTMSI = true; m_ptmsi = std::move(v); return *this; }

        /// Build the final message.
        [[nodiscard]] L3P_TMSIReallocationCommand build() const;
    };

    static Builder builder();

    GMMPTMSIType ptmsiType() const { return mPTMSIType; }
    bool forceToStandby() const { return mForceToStandby; }
    const L3RoutingAreaIdentification& rai() const { return mRAI; }
    bool hasPTMSI() const { return mHavePTMSI; }
    const L3MobileIdentity& ptmsi() const { return mPTMSI; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3P_TMSIReallocationCommand> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSMobilityManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── P-TMSI Reallocation Complete (GSM 24.008 9.4.8) ───────────────────
// MS->SGSN: no mandatory body fields

class L3P_TMSIReallocationComplete {
public:
    static constexpr int MTI = 0x11;

    struct Builder {
        /// Build the final message.
        [[nodiscard]] L3P_TMSIReallocationComplete build() const;
    };

    static Builder builder();

    size_t bodyLength() const { return 0; }
    [[nodiscard]] static Expected<L3P_TMSIReallocationComplete> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSMobilityManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Authentication And Ciphering Request (GSM 24.008 9.4.9) ───────────
// SGSN->MS: cipheringAlgorithm(3)|spare(1) | imeisvRequest(1)|forceToStandby(1)|spare(6) |
//           acReferenceNumber(4) | authenticationParameterRAND(TLV) | [CKSN(TV)] | [AUTN(TLV)]

class L3AuthenticationAndCipheringRequest {
    uint8_t mCipheringAlgorithm{0};
    bool mImeisvRequest{false};
    bool mForceToStandby{false};
    uint8_t mACReferenceNumber{0};
    L3AuthRAND mRAND;

    friend struct Builder;
public:
    static constexpr int MTI = 0x12;

    struct Builder {
        uint8_t m_cipheringAlgorithm{0};
        bool m_imeisvRequest{false};
        bool m_forceToStandby{false};
        uint8_t m_acReferenceNumber{0};
        L3AuthRAND m_rand;

        /// Set ciphering algorithm.
        Builder& cipheringAlgorithm(uint8_t v) { m_cipheringAlgorithm = v; return *this; }
        /// Set IMEISV request flag.
        Builder& imeisvRequest(bool v) { m_imeisvRequest = v; return *this; }
        /// Set force to standby flag.
        Builder& forceToStandby(bool v) { m_forceToStandby = v; return *this; }
        /// Set AC reference number.
        Builder& acReferenceNumber(uint8_t v) { m_acReferenceNumber = v; return *this; }
        /// Set authentication RAND.
        Builder& rand(L3AuthRAND v) { m_rand = std::move(v); return *this; }

        /// Build the final message.
        [[nodiscard]] L3AuthenticationAndCipheringRequest build() const;
    };

    static Builder builder();

    uint8_t cipheringAlgorithm() const { return mCipheringAlgorithm; }
    bool imeisvRequest() const { return mImeisvRequest; }
    bool forceToStandby() const { return mForceToStandby; }
    uint8_t acReferenceNumber() const { return mACReferenceNumber; }
    const L3AuthRAND& rand() const { return mRAND; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3AuthenticationAndCipheringRequest> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSMobilityManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Authentication And Ciphering Response (GSM 24.008 9.4.9) ──────────
// MS->SGSN: acReferenceNumber(4)|spare(4) | authenticationParameterResponse(TLV) | ...

class L3AuthenticationAndCipheringResponse {
    uint8_t mACReferenceNumber{0};
    L3AuthRES mRES;

    friend struct Builder;
public:
    static constexpr int MTI = 0x13;

    struct Builder {
        uint8_t m_acReferenceNumber{0};
        L3AuthRES m_res;

        /// Set AC reference number.
        Builder& acReferenceNumber(uint8_t v) { m_acReferenceNumber = v; return *this; }
        /// Set authentication response.
        Builder& res(L3AuthRES v) { m_res = std::move(v); return *this; }

        /// Build the final message.
        [[nodiscard]] L3AuthenticationAndCipheringResponse build() const;
    };

    static Builder builder();

    uint8_t acReferenceNumber() const { return mACReferenceNumber; }
    const L3AuthRES& res() const { return mRES; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3AuthenticationAndCipheringResponse> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSMobilityManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Authentication And Ciphering Reject (GSM 24.008 9.4.9) ────────────
// SGSN->MS: no mandatory body

class L3AuthenticationAndCipheringReject {
public:
    static constexpr int MTI = 0x14;

    struct Builder {
        /// Build the final message.
        [[nodiscard]] L3AuthenticationAndCipheringReject build() const;
    };

    static Builder builder();

    size_t bodyLength() const { return 0; }
    [[nodiscard]] static Expected<L3AuthenticationAndCipheringReject> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSMobilityManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Identity Request (GSM 24.008 9.4.7) ───────────────────────────────
// SGSN->MS: identityType(3)|spare(1) | forceToStandby(1)|spare(7)

class L3GMMIdentityRequest {
    MobileIDType mIdentityType{MobileIDType::NoID};
    bool mForceToStandby{false};

    friend struct Builder;
public:
    static constexpr int MTI = 0x15;

    struct Builder {
        MobileIDType m_identityType{MobileIDType::NoID};
        bool m_forceToStandby{false};

        /// Set identity type.
        Builder& identityType(MobileIDType v) { m_identityType = v; return *this; }
        /// Set force to standby flag.
        Builder& forceToStandby(bool v) { m_forceToStandby = v; return *this; }

        /// Build the final message.
        [[nodiscard]] L3GMMIdentityRequest build() const;
    };

    static Builder builder();

    MobileIDType identityType() const { return mIdentityType; }
    bool forceToStandby() const { return mForceToStandby; }

    size_t bodyLength() const { return 2; }
    [[nodiscard]] static Expected<L3GMMIdentityRequest> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSMobilityManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Identity Response (GSM 24.008 9.4.10) ─────────────────────────────
// MS->SGSN: mobileIdentity(LV)

class L3GMMIdentityResponse {
    L3MobileIdentity mMobileIdentity;

    friend struct Builder;
public:
    static constexpr int MTI = 0x16;

    struct Builder {
        L3MobileIdentity m_mobileIdentity;

        /// Set mobile identity.
        Builder& mobileIdentity(L3MobileIdentity v) { m_mobileIdentity = std::move(v); return *this; }

        /// Build the final message.
        [[nodiscard]] L3GMMIdentityResponse build() const;
    };

    static Builder builder();

    const L3MobileIdentity& mobileId() const { return mMobileIdentity; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3GMMIdentityResponse> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSMobilityManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Authentication And Ciphering Failure (GSM 24.008 9.4.23) ──────────
// MS->SGSN: gmmCause(TLV) | authenticationFailureParameter(TLV)

class L3AuthenticationAndCipheringFailure {
    GMMCause mCause{GMMCause::Synch_Failure};
    L3AuthFailureParam mAuthFailureParam;

    friend struct Builder;
public:
    static constexpr int MTI = 0x1c;

    struct Builder {
        GMMCause m_cause{GMMCause::Synch_Failure};
        L3AuthFailureParam m_authFailureParam;

        /// Set GMM cause.
        Builder& cause(GMMCause v) { m_cause = v; return *this; }
        /// Set authentication failure parameter.
        Builder& authFailureParam(L3AuthFailureParam v) { m_authFailureParam = std::move(v); return *this; }

        /// Build the final message.
        [[nodiscard]] L3AuthenticationAndCipheringFailure build() const;
    };

    static Builder builder();

    GMMCause cause() const { return mCause; }
    const L3AuthFailureParam& authFailureParam() const { return mAuthFailureParam; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3AuthenticationAndCipheringFailure> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSMobilityManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── GMM Status (GSM 24.008 9.4.24) ────────────────────────────────────
// Bidirectional: direction(1)|spare(3)|cause(4) — or full cause byte

class L3GMMStatus {
    GMMCause mCause{GMMCause::Unspecified};

    friend struct Builder;
public:
    static constexpr int MTI = 0x20;
    L3GMMStatus() = default;
    explicit L3GMMStatus(GMMCause cause) : mCause(cause) {}

    struct Builder {
        GMMCause m_cause{GMMCause::Unspecified};

        /// Set GMM cause.
        Builder& cause(GMMCause v) { m_cause = v; return *this; }

        /// Build the final message.
        [[nodiscard]] L3GMMStatus build() const;
    };

    static Builder builder();

    GMMCause cause() const { return mCause; }

    size_t bodyLength() const { return 1; }
    [[nodiscard]] static Expected<L3GMMStatus> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSMobilityManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── GMM Information (GSM 24.008) ──────────────────────────────────────
// SGSN->MS: variable body, network information

class L3GMMInformation {
public:
    static constexpr int MTI = 0x21;

    struct Builder {
        /// Build the final message.
        [[nodiscard]] L3GMMInformation build() const;
    };

    static Builder builder();

    size_t bodyLength() const { return 0; }
    [[nodiscard]] static Expected<L3GMMInformation> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSMobilityManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// GMM message type names for text output.
const char* gmmMessageName(int mti);

} // namespace gsml3parser
