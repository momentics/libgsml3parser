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
public:
    static constexpr int MTI = 0x01;

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
public:
    static constexpr int MTI = 0x02;

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
public:
    static constexpr int MTI = 0x04;

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
public:
    static constexpr int MTI = 0x05;

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
public:
    static constexpr int MTI = 0x06;

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
public:
    static constexpr int MTI = 0x08;

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
public:
    static constexpr int MTI = 0x09;

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
public:
    static constexpr int MTI = 0x0b;

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
public:
    static constexpr int MTI = 0x0c;

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
public:
    static constexpr int MTI = 0x0e;

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
public:
    static constexpr int MTI = 0x10;

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
public:
    static constexpr int MTI = 0x12;

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
public:
    static constexpr int MTI = 0x13;

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
public:
    static constexpr int MTI = 0x15;

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
public:
    static constexpr int MTI = 0x16;

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
public:
    static constexpr int MTI = 0x1c;

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
public:
    static constexpr int MTI = 0x20;
    L3GMMStatus() = default;
    explicit L3GMMStatus(GMMCause cause) : mCause(cause) {}

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

    size_t bodyLength() const { return 0; }
    [[nodiscard]] static Expected<L3GMMInformation> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSMobilityManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

} // namespace gsml3parser
