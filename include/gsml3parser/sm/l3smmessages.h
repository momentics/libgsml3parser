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

// SM Message Classes - GSM L3 GPRS Session Management messages
// Spec: 3GPP TS 24.008 sections 9.5, Table 10.4a
// Reference: ref/osmo-ttcn3-hacks/library/L3_Templates.ttcn - SM message templates
//            ref/OpenBTS/SGSNGGSN/GPRSL3Messages.h - L3SmMsg::MessageType enum
//
// L3 header (per 24.008 10.4a):
//   Byte 0: PD(4)=0x0A(SM) | Skip(4)
//   Byte 1: MessageType(8 bits, raw - no NSD field)
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
#include "l3smelements.h"

namespace gsml3parser {

// ── Activate PDP Context Request (GSM 24.008 9.5.1) ───────────────────
// MS->SGSN: pdpType(4)|spare(4) | [PDPAddress(TLV)] | APN(TLV) | QoS(TLV) | [PCO(TLV)]

class L3ActivatePDPContextRequest {
    PDPType mPDPType{PDPType::IPv4};
    bool mHavePDPAddress{false};
    L3PDPAddress mPDPAddress;
    L3AccessPointName mAPN;
    L3QoS mQoS;
    bool mHavePCO{false};
    L3ProtocolConfigOptions mPCO;

    friend struct Builder;
public:
    static constexpr int MTI = 0x41;

    struct Builder {
        PDPType m_pdpType{PDPType::IPv4};
        bool m_havePDPAddress{false};
        L3PDPAddress m_pdpAddress;
        L3AccessPointName m_apn;
        L3QoS m_qos;
        bool m_havePCO{false};
        L3ProtocolConfigOptions m_pco;

        /// Set PDP type.
        Builder& pdpType(PDPType v) { m_pdpType = v; return *this; }
        /// Set PDP address (sets mHavePDPAddress flag).
        Builder& pdpAddress(L3PDPAddress v) { m_pdpAddress = v; m_havePDPAddress = true; return *this; }
        /// Set APN.
        Builder& apn(L3AccessPointName v) { m_apn = v; return *this; }
        /// Set QoS.
        Builder& qos(L3QoS v) { m_qos = v; return *this; }
        /// Set PCO (sets mHavePCO flag).
        Builder& pco(L3ProtocolConfigOptions v) { m_pco = v; m_havePCO = true; return *this; }
        /// Build the final message.
        [[nodiscard]] L3ActivatePDPContextRequest build() const;
    };

    static Builder builder();

    PDPType pdpType() const { return mPDPType; }
    bool hasPDPAddress() const { return mHavePDPAddress; }
    const L3PDPAddress& pdpAddress() const { return mPDPAddress; }
    const L3AccessPointName& apn() const { return mAPN; }
    const L3QoS& qos() const { return mQoS; }
    bool hasPCO() const { return mHavePCO; }
    const L3ProtocolConfigOptions& pco() const { return mPCO; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3ActivatePDPContextRequest> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSSessionManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Activate PDP Context Accept (GSM 24.008 9.5.2) ────────────────────
// SGSN->MS: pdpHandle(4)|spare(4) | [PDPAddress(TLV)] | QoS(TLV) | [PCO(TLV)]

class L3ActivatePDPContextAccept {
    uint8_t mPDPHandle{0};
    bool mHavePDPAddress{false};
    L3PDPAddress mPDPAddress;
    L3QoS mQoS;
    bool mHavePCO{false};
    L3ProtocolConfigOptions mPCO;

    friend struct Builder;
public:
    static constexpr int MTI = 0x42;

    struct Builder {
        uint8_t m_pdpHandle{0};
        bool m_havePDPAddress{false};
        L3PDPAddress m_pdpAddress;
        L3QoS m_qos;
        bool m_havePCO{false};
        L3ProtocolConfigOptions m_pco;

        /// Set PDP handle.
        Builder& pdpHandle(uint8_t v) { m_pdpHandle = v; return *this; }
        /// Set PDP address (sets mHavePDPAddress flag).
        Builder& pdpAddress(L3PDPAddress v) { m_pdpAddress = v; m_havePDPAddress = true; return *this; }
        /// Set QoS.
        Builder& qos(L3QoS v) { m_qos = v; return *this; }
        /// Set PCO (sets mHavePCO flag).
        Builder& pco(L3ProtocolConfigOptions v) { m_pco = v; m_havePCO = true; return *this; }
        /// Build the final message.
        [[nodiscard]] L3ActivatePDPContextAccept build() const;
    };

    static Builder builder();

    uint8_t pdpHandle() const { return mPDPHandle; }
    bool hasPDPAddress() const { return mHavePDPAddress; }
    const L3PDPAddress& pdpAddress() const { return mPDPAddress; }
    const L3QoS& qos() const { return mQoS; }
    bool hasPCO() const { return mHavePCO; }
    const L3ProtocolConfigOptions& pco() const { return mPCO; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3ActivatePDPContextAccept> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSSessionManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Activate PDP Context Reject (GSM 24.008 9.5.3) ────────────────────
// SGSN->MS: smCause(TLV) | [BackOffTimer(TLV)]

class L3ActivatePDPContextReject {
    SMCause mCause{SMCause::Unsupported_PDP_Address_Type};
    bool mHaveBackOffTimer{false};
    L3BackOffTimer mBackOffTimer;

    friend struct Builder;
public:
    static constexpr int MTI = 0x43;

    struct Builder {
        SMCause m_cause{SMCause::Unsupported_PDP_Address_Type};
        bool m_haveBackOffTimer{false};
        L3BackOffTimer m_backOffTimer;

        /// Set SM cause.
        Builder& cause(SMCause v) { m_cause = v; return *this; }
        /// Set back-off timer (sets mHaveBackOffTimer flag).
        Builder& backOffTimer(L3BackOffTimer v) { m_backOffTimer = v; m_haveBackOffTimer = true; return *this; }
        /// Build the final message.
        [[nodiscard]] L3ActivatePDPContextReject build() const;
    };

    static Builder builder();

    SMCause cause() const { return mCause; }
    bool hasBackOffTimer() const { return mHaveBackOffTimer; }
    const L3BackOffTimer& backOffTimer() const { return mBackOffTimer; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3ActivatePDPContextReject> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSSessionManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Deactivate PDP Context Request (GSM 24.008 9.5.4) ─────────────────
// Bidirectional: pdpHandle(4)|spare(4) | [PDPType(TV)] | [PDPAddress(TLV)]

class L3DeactivatePDPContextRequest {
    uint8_t mPDPHandle{0};
    bool mHavePDPType{false};
    PDPType mPDPType{PDPType::IPv4};
    bool mHavePDPAddress{false};
    L3PDPAddress mPDPAddress;

    friend struct Builder;
public:
    static constexpr int MTI = 0x46;

    struct Builder {
        uint8_t m_pdpHandle{0};
        bool m_havePDPType{false};
        PDPType m_pdpType{PDPType::IPv4};
        bool m_havePDPAddress{false};
        L3PDPAddress m_pdpAddress;

        /// Set PDP handle.
        Builder& pdpHandle(uint8_t v) { m_pdpHandle = v; return *this; }
        /// Set PDP type (sets mHavePDPType flag).
        Builder& pdpType(PDPType v) { m_pdpType = v; m_havePDPType = true; return *this; }
        /// Set PDP address (sets mHavePDPAddress flag).
        Builder& pdpAddress(L3PDPAddress v) { m_pdpAddress = v; m_havePDPAddress = true; return *this; }
        /// Build the final message.
        [[nodiscard]] L3DeactivatePDPContextRequest build() const;
    };

    static Builder builder();

    uint8_t pdpHandle() const { return mPDPHandle; }
    bool hasPDPType() const { return mHavePDPType; }
    PDPType pdpType() const { return mPDPType; }
    bool hasPDPAddress() const { return mHavePDPAddress; }
    const L3PDPAddress& pdpAddress() const { return mPDPAddress; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3DeactivatePDPContextRequest> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSSessionManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Deactivate PDP Context Accept (GSM 24.008 9.5.5) ──────────────────
// Bidirectional: pdpHandle(4)|spare(4)

class L3DeactivatePDPContextAccept {
    uint8_t mPDPHandle{0};

    friend struct Builder;
public:
    static constexpr int MTI = 0x47;

    struct Builder {
        uint8_t m_pdpHandle{0};

        /// Set PDP handle.
        Builder& pdpHandle(uint8_t v) { m_pdpHandle = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3DeactivatePDPContextAccept build() const;
    };

    static Builder builder();

    uint8_t pdpHandle() const { return mPDPHandle; }

    size_t bodyLength() const { return 1; }
    [[nodiscard]] static Expected<L3DeactivatePDPContextAccept> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSSessionManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Modify PDP Context Request (GSM 24.008 9.5.6) ─────────────────────
// SGSN->MS: pdpHandle(4)|spare(4) | QoS(TLV) | [PCO(TLV)]

class L3ModifyPDPContextRequest {
    uint8_t mPDPHandle{0};
    L3QoS mQoS;
    bool mHavePCO{false};
    L3ProtocolConfigOptions mPCO;

    friend struct Builder;
public:
    static constexpr int MTI = 0x48;

    struct Builder {
        uint8_t m_pdpHandle{0};
        L3QoS m_qos;
        bool m_havePCO{false};
        L3ProtocolConfigOptions m_pco;

        /// Set PDP handle.
        Builder& pdpHandle(uint8_t v) { m_pdpHandle = v; return *this; }
        /// Set QoS.
        Builder& qos(L3QoS v) { m_qos = v; return *this; }
        /// Set PCO (sets mHavePCO flag).
        Builder& pco(L3ProtocolConfigOptions v) { m_pco = v; m_havePCO = true; return *this; }
        /// Build the final message.
        [[nodiscard]] L3ModifyPDPContextRequest build() const;
    };

    static Builder builder();

    uint8_t pdpHandle() const { return mPDPHandle; }
    const L3QoS& qos() const { return mQoS; }
    bool hasPCO() const { return mHavePCO; }
    const L3ProtocolConfigOptions& pco() const { return mPCO; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3ModifyPDPContextRequest> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSSessionManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Modify PDP Context Accept (GSM 24.008 9.5.7) ──────────────────────
// MS->SGSN: pdpHandle(4)|spare(4) | QoS(TLV) | [PCO(TLV)]

class L3ModifyPDPContextAccept {
    uint8_t mPDPHandle{0};
    L3QoS mQoS;
    bool mHavePCO{false};
    L3ProtocolConfigOptions mPCO;

    friend struct Builder;
public:
    static constexpr int MTI = 0x49;

    struct Builder {
        uint8_t m_pdpHandle{0};
        L3QoS m_qos;
        bool m_havePCO{false};
        L3ProtocolConfigOptions m_pco;

        /// Set PDP handle.
        Builder& pdpHandle(uint8_t v) { m_pdpHandle = v; return *this; }
        /// Set QoS.
        Builder& qos(L3QoS v) { m_qos = v; return *this; }
        /// Set PCO (sets mHavePCO flag).
        Builder& pco(L3ProtocolConfigOptions v) { m_pco = v; m_havePCO = true; return *this; }
        /// Build the final message.
        [[nodiscard]] L3ModifyPDPContextAccept build() const;
    };

    static Builder builder();

    uint8_t pdpHandle() const { return mPDPHandle; }
    const L3QoS& qos() const { return mQoS; }
    bool hasPCO() const { return mHavePCO; }
    const L3ProtocolConfigOptions& pco() const { return mPCO; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3ModifyPDPContextAccept> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSSessionManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Modify PDP Context Reject (GSM 24.008 9.5.8) ──────────────────────
// Bidirectional: pdpHandle(4)|spare(4) | smCause(TLV) | [BackOffTimer(TLV)]

class L3ModifyPDPContextReject {
    uint8_t mPDPHandle{0};
    SMCause mCause{SMCause::Unsupported_PDP_Address_Type};
    bool mHaveBackOffTimer{false};
    L3BackOffTimer mBackOffTimer;

    friend struct Builder;
public:
    static constexpr int MTI = 0x4c;

    struct Builder {
        uint8_t m_pdpHandle{0};
        SMCause m_cause{SMCause::Unsupported_PDP_Address_Type};
        bool m_haveBackOffTimer{false};
        L3BackOffTimer m_backOffTimer;

        /// Set PDP handle.
        Builder& pdpHandle(uint8_t v) { m_pdpHandle = v; return *this; }
        /// Set SM cause.
        Builder& cause(SMCause v) { m_cause = v; return *this; }
        /// Set back-off timer (sets mHaveBackOffTimer flag).
        Builder& backOffTimer(L3BackOffTimer v) { m_backOffTimer = v; m_haveBackOffTimer = true; return *this; }
        /// Build the final message.
        [[nodiscard]] L3ModifyPDPContextReject build() const;
    };

    static Builder builder();

    uint8_t pdpHandle() const { return mPDPHandle; }
    SMCause cause() const { return mCause; }
    bool hasBackOffTimer() const { return mHaveBackOffTimer; }
    const L3BackOffTimer& backOffTimer() const { return mBackOffTimer; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3ModifyPDPContextReject> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSSessionManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── SM Status (GSM 24.008 9.5.9) ──────────────────────────────────────
// Bidirectional: smCause(TLV)

class L3SMStatus {
    SMCause mCause{SMCause::ReqAccepted};

    friend struct Builder;
public:
    static constexpr int MTI = 0x55;
    L3SMStatus() = default;
    explicit L3SMStatus(SMCause cause) : mCause(cause) {}

    struct Builder {
        SMCause m_cause{SMCause::ReqAccepted};

        /// Set SM cause.
        Builder& cause(SMCause v) { m_cause = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3SMStatus build() const;
    };

    static Builder builder();

    SMCause cause() const { return mCause; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3SMStatus> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSSessionManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Request PDP Context Activation (GSM 24.008 9.5.10) ────────────────
// Net->MS: pdpHandle(4)|spare(4) | [PDPAddress(TLV)] | APN(TLV) | QoS(TLV) | [PCO(TLV)]

class L3RequestPDPContextActivation {
    uint8_t mPDPHandle{0};
    bool mHavePDPAddress{false};
    L3PDPAddress mPDPAddress;
    L3AccessPointName mAPN;
    L3QoS mQoS;
    bool mHavePCO{false};
    L3ProtocolConfigOptions mPCO;

    friend struct Builder;
public:
    static constexpr int MTI = 0x44;

    struct Builder {
        uint8_t m_pdpHandle{0};
        bool m_havePDPAddress{false};
        L3PDPAddress m_pdpAddress;
        L3AccessPointName m_apn;
        L3QoS m_qos;
        bool m_havePCO{false};
        L3ProtocolConfigOptions m_pco;

        /// Set PDP handle.
        Builder& pdpHandle(uint8_t v) { m_pdpHandle = v; return *this; }
        /// Set PDP address (sets mHavePDPAddress flag).
        Builder& pdpAddress(L3PDPAddress v) { m_pdpAddress = v; m_havePDPAddress = true; return *this; }
        /// Set APN.
        Builder& apn(L3AccessPointName v) { m_apn = v; return *this; }
        /// Set QoS.
        Builder& qos(L3QoS v) { m_qos = v; return *this; }
        /// Set PCO (sets mHavePCO flag).
        Builder& pco(L3ProtocolConfigOptions v) { m_pco = v; m_havePCO = true; return *this; }
        /// Build the final message.
        [[nodiscard]] L3RequestPDPContextActivation build() const;
    };

    static Builder builder();

    uint8_t pdpHandle() const { return mPDPHandle; }
    bool hasPDPAddress() const { return mHavePDPAddress; }
    const L3PDPAddress& pdpAddress() const { return mPDPAddress; }
    const L3AccessPointName& apn() const { return mAPN; }
    const L3QoS& qos() const { return mQoS; }
    bool hasPCO() const { return mHavePCO; }
    const L3ProtocolConfigOptions& pco() const { return mPCO; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3RequestPDPContextActivation> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSSessionManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Request PDP Context Activation Reject (GSM 24.008 9.5.10) ─────────
// MS->Net: pdpHandle(4)|spare(4) | smCause(TLV)

class L3RequestPDPContextActivationReject {
    uint8_t mPDPHandle{0};
    SMCause mCause{SMCause::Unsupported_PDP_Address_Type};

    friend struct Builder;
public:
    static constexpr int MTI = 0x45;

    struct Builder {
        uint8_t m_pdpHandle{0};
        SMCause m_cause{SMCause::Unsupported_PDP_Address_Type};

        /// Set PDP handle.
        Builder& pdpHandle(uint8_t v) { m_pdpHandle = v; return *this; }
        /// Set SM cause.
        Builder& cause(SMCause v) { m_cause = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3RequestPDPContextActivationReject build() const;
    };

    static Builder builder();

    uint8_t pdpHandle() const { return mPDPHandle; }
    SMCause cause() const { return mCause; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3RequestPDPContextActivationReject> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSSessionManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Modify PDP Context Request (MS->Net) (GSM 24.008 9.5.6) ───────────
// MS->Net: pdpHandle(4)|spare(4) | QoS(TLV) | [PCO(TLV)]

class L3ModifyPDPContextRequestMS {
    uint8_t mPDPHandle{0};
    L3QoS mQoS;
    bool mHavePCO{false};
    L3ProtocolConfigOptions mPCO;

    friend struct Builder;
public:
    static constexpr int MTI = 0x4A;

    struct Builder {
        uint8_t m_pdpHandle{0};
        L3QoS m_qos;
        bool m_havePCO{false};
        L3ProtocolConfigOptions m_pco;

        /// Set PDP handle.
        Builder& pdpHandle(uint8_t v) { m_pdpHandle = v; return *this; }
        /// Set QoS.
        Builder& qos(L3QoS v) { m_qos = v; return *this; }
        /// Set PCO (sets mHavePCO flag).
        Builder& pco(L3ProtocolConfigOptions v) { m_pco = v; m_havePCO = true; return *this; }
        /// Build the final message.
        [[nodiscard]] L3ModifyPDPContextRequestMS build() const;
    };

    static Builder builder();

    uint8_t pdpHandle() const { return mPDPHandle; }
    const L3QoS& qos() const { return mQoS; }
    bool hasPCO() const { return mHavePCO; }
    const L3ProtocolConfigOptions& pco() const { return mPCO; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3ModifyPDPContextRequestMS> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSSessionManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Modify PDP Context Accept (Net->MS) (GSM 24.008 9.5.7) ────────────
// Net->MS: pdpHandle(4)|spare(4) | QoS(TLV) | [PCO(TLV)]

class L3ModifyPDPContextAcceptNet {
    uint8_t mPDPHandle{0};
    L3QoS mQoS;
    bool mHavePCO{false};
    L3ProtocolConfigOptions mPCO;

    friend struct Builder;
public:
    static constexpr int MTI = 0x4B;

    struct Builder {
        uint8_t m_pdpHandle{0};
        L3QoS m_qos;
        bool m_havePCO{false};
        L3ProtocolConfigOptions m_pco;

        /// Set PDP handle.
        Builder& pdpHandle(uint8_t v) { m_pdpHandle = v; return *this; }
        /// Set QoS.
        Builder& qos(L3QoS v) { m_qos = v; return *this; }
        /// Set PCO (sets mHavePCO flag).
        Builder& pco(L3ProtocolConfigOptions v) { m_pco = v; m_havePCO = true; return *this; }
        /// Build the final message.
        [[nodiscard]] L3ModifyPDPContextAcceptNet build() const;
    };

    static Builder builder();

    uint8_t pdpHandle() const { return mPDPHandle; }
    const L3QoS& qos() const { return mQoS; }
    bool hasPCO() const { return mHavePCO; }
    const L3ProtocolConfigOptions& pco() const { return mPCO; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3ModifyPDPContextAcceptNet> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSSessionManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Activate Secondary PDP Context Request (GSM 24.008 9.5.11) ────────
// Net->MS: pdpHandle(4)|spare(4) | [PDPAddress(TLV)] | APN(TLV) | QoS(TLV) | [PCO(TLV)]

class L3ActivateSecondaryPDPContextRequest {
    uint8_t mPDPHandle{0};
    bool mHavePDPAddress{false};
    L3PDPAddress mPDPAddress;
    L3AccessPointName mAPN;
    L3QoS mQoS;
    bool mHavePCO{false};
    L3ProtocolConfigOptions mPCO;

    friend struct Builder;
public:
    static constexpr int MTI = 0x4D;

    struct Builder {
        uint8_t m_pdpHandle{0};
        bool m_havePDPAddress{false};
        L3PDPAddress m_pdpAddress;
        L3AccessPointName m_apn;
        L3QoS m_qos;
        bool m_havePCO{false};
        L3ProtocolConfigOptions m_pco;

        /// Set PDP handle.
        Builder& pdpHandle(uint8_t v) { m_pdpHandle = v; return *this; }
        /// Set PDP address (sets mHavePDPAddress flag).
        Builder& pdpAddress(L3PDPAddress v) { m_pdpAddress = v; m_havePDPAddress = true; return *this; }
        /// Set APN.
        Builder& apn(L3AccessPointName v) { m_apn = v; return *this; }
        /// Set QoS.
        Builder& qos(L3QoS v) { m_qos = v; return *this; }
        /// Set PCO (sets mHavePCO flag).
        Builder& pco(L3ProtocolConfigOptions v) { m_pco = v; m_havePCO = true; return *this; }
        /// Build the final message.
        [[nodiscard]] L3ActivateSecondaryPDPContextRequest build() const;
    };

    static Builder builder();

    uint8_t pdpHandle() const { return mPDPHandle; }
    bool hasPDPAddress() const { return mHavePDPAddress; }
    const L3PDPAddress& pdpAddress() const { return mPDPAddress; }
    const L3AccessPointName& apn() const { return mAPN; }
    const L3QoS& qos() const { return mQoS; }
    bool hasPCO() const { return mHavePCO; }
    const L3ProtocolConfigOptions& pco() const { return mPCO; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3ActivateSecondaryPDPContextRequest> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSSessionManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Activate Secondary PDP Context Accept (GSM 24.008 9.5.12) ─────────
// MS->Net: pdpHandle(4)|spare(4) | [PDPAddress(TLV)] | QoS(TLV) | [PCO(TLV)]

class L3ActivateSecondaryPDPContextAccept {
    uint8_t mPDPHandle{0};
    bool mHavePDPAddress{false};
    L3PDPAddress mPDPAddress;
    L3QoS mQoS;
    bool mHavePCO{false};
    L3ProtocolConfigOptions mPCO;

    friend struct Builder;
public:
    static constexpr int MTI = 0x4E;

    struct Builder {
        uint8_t m_pdpHandle{0};
        bool m_havePDPAddress{false};
        L3PDPAddress m_pdpAddress;
        L3QoS m_qos;
        bool m_havePCO{false};
        L3ProtocolConfigOptions m_pco;

        /// Set PDP handle.
        Builder& pdpHandle(uint8_t v) { m_pdpHandle = v; return *this; }
        /// Set PDP address (sets mHavePDPAddress flag).
        Builder& pdpAddress(L3PDPAddress v) { m_pdpAddress = v; m_havePDPAddress = true; return *this; }
        /// Set QoS.
        Builder& qos(L3QoS v) { m_qos = v; return *this; }
        /// Set PCO (sets mHavePCO flag).
        Builder& pco(L3ProtocolConfigOptions v) { m_pco = v; m_havePCO = true; return *this; }
        /// Build the final message.
        [[nodiscard]] L3ActivateSecondaryPDPContextAccept build() const;
    };

    static Builder builder();

    uint8_t pdpHandle() const { return mPDPHandle; }
    bool hasPDPAddress() const { return mHavePDPAddress; }
    const L3PDPAddress& pdpAddress() const { return mPDPAddress; }
    const L3QoS& qos() const { return mQoS; }
    bool hasPCO() const { return mHavePCO; }
    const L3ProtocolConfigOptions& pco() const { return mPCO; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3ActivateSecondaryPDPContextAccept> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSSessionManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Activate Secondary PDP Context Reject (GSM 24.008 9.5.13) ─────────
// MS->Net: pdpHandle(4)|spare(4) | smCause(TLV)

class L3ActivateSecondaryPDPContextReject {
    uint8_t mPDPHandle{0};
    SMCause mCause{SMCause::Unsupported_PDP_Address_Type};

    friend struct Builder;
public:
    static constexpr int MTI = 0x4F;

    struct Builder {
        uint8_t m_pdpHandle{0};
        SMCause m_cause{SMCause::Unsupported_PDP_Address_Type};

        /// Set PDP handle.
        Builder& pdpHandle(uint8_t v) { m_pdpHandle = v; return *this; }
        /// Set SM cause.
        Builder& cause(SMCause v) { m_cause = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3ActivateSecondaryPDPContextReject build() const;
    };

    static Builder builder();

    uint8_t pdpHandle() const { return mPDPHandle; }
    SMCause cause() const { return mCause; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3ActivateSecondaryPDPContextReject> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSSessionManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Activate AA PDP Context Request (GSM 24.008 9.5.14) ───────────────
// Net->MS: pdpHandle(4)|spare(4) | [PDPAddress(TLV)] | APN(TLV) | QoS(TLV) | [PCO(TLV)]

class L3ActivateAAPDPContextRequest {
    uint8_t mPDPHandle{0};
    bool mHavePDPAddress{false};
    L3PDPAddress mPDPAddress;
    L3AccessPointName mAPN;
    L3QoS mQoS;
    bool mHavePCO{false};
    L3ProtocolConfigOptions mPCO;

    friend struct Builder;
public:
    static constexpr int MTI = 0x50;

    struct Builder {
        uint8_t m_pdpHandle{0};
        bool m_havePDPAddress{false};
        L3PDPAddress m_pdpAddress;
        L3AccessPointName m_apn;
        L3QoS m_qos;
        bool m_havePCO{false};
        L3ProtocolConfigOptions m_pco;

        /// Set PDP handle.
        Builder& pdpHandle(uint8_t v) { m_pdpHandle = v; return *this; }
        /// Set PDP address (sets mHavePDPAddress flag).
        Builder& pdpAddress(L3PDPAddress v) { m_pdpAddress = v; m_havePDPAddress = true; return *this; }
        /// Set APN.
        Builder& apn(L3AccessPointName v) { m_apn = v; return *this; }
        /// Set QoS.
        Builder& qos(L3QoS v) { m_qos = v; return *this; }
        /// Set PCO (sets mHavePCO flag).
        Builder& pco(L3ProtocolConfigOptions v) { m_pco = v; m_havePCO = true; return *this; }
        /// Build the final message.
        [[nodiscard]] L3ActivateAAPDPContextRequest build() const;
    };

    static Builder builder();

    uint8_t pdpHandle() const { return mPDPHandle; }
    bool hasPDPAddress() const { return mHavePDPAddress; }
    const L3PDPAddress& pdpAddress() const { return mPDPAddress; }
    const L3AccessPointName& apn() const { return mAPN; }
    const L3QoS& qos() const { return mQoS; }
    bool hasPCO() const { return mHavePCO; }
    const L3ProtocolConfigOptions& pco() const { return mPCO; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3ActivateAAPDPContextRequest> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSSessionManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Activate AA PDP Context Accept (GSM 24.008 9.5.15) ────────────────
// MS->Net: pdpHandle(4)|spare(4) | [PDPAddress(TLV)] | QoS(TLV) | [PCO(TLV)]

class L3ActivateAAPDPContextAccept {
    uint8_t mPDPHandle{0};
    bool mHavePDPAddress{false};
    L3PDPAddress mPDPAddress;
    L3QoS mQoS;
    bool mHavePCO{false};
    L3ProtocolConfigOptions mPCO;

    friend struct Builder;
public:
    static constexpr int MTI = 0x51;

    struct Builder {
        uint8_t m_pdpHandle{0};
        bool m_havePDPAddress{false};
        L3PDPAddress m_pdpAddress;
        L3QoS m_qos;
        bool m_havePCO{false};
        L3ProtocolConfigOptions m_pco;

        /// Set PDP handle.
        Builder& pdpHandle(uint8_t v) { m_pdpHandle = v; return *this; }
        /// Set PDP address (sets mHavePDPAddress flag).
        Builder& pdpAddress(L3PDPAddress v) { m_pdpAddress = v; m_havePDPAddress = true; return *this; }
        /// Set QoS.
        Builder& qos(L3QoS v) { m_qos = v; return *this; }
        /// Set PCO (sets mHavePCO flag).
        Builder& pco(L3ProtocolConfigOptions v) { m_pco = v; m_havePCO = true; return *this; }
        /// Build the final message.
        [[nodiscard]] L3ActivateAAPDPContextAccept build() const;
    };

    static Builder builder();

    uint8_t pdpHandle() const { return mPDPHandle; }
    bool hasPDPAddress() const { return mHavePDPAddress; }
    const L3PDPAddress& pdpAddress() const { return mPDPAddress; }
    const L3QoS& qos() const { return mQoS; }
    bool hasPCO() const { return mHavePCO; }
    const L3ProtocolConfigOptions& pco() const { return mPCO; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3ActivateAAPDPContextAccept> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSSessionManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Activate AA PDP Context Reject (GSM 24.008 9.5.16) ────────────────
// MS->Net: pdpHandle(4)|spare(4) | smCause(TLV)

class L3ActivateAAPDPContextReject {
    uint8_t mPDPHandle{0};
    SMCause mCause{SMCause::Unsupported_PDP_Address_Type};

    friend struct Builder;
public:
    static constexpr int MTI = 0x52;

    struct Builder {
        uint8_t m_pdpHandle{0};
        SMCause m_cause{SMCause::Unsupported_PDP_Address_Type};

        /// Set PDP handle.
        Builder& pdpHandle(uint8_t v) { m_pdpHandle = v; return *this; }
        /// Set SM cause.
        Builder& cause(SMCause v) { m_cause = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3ActivateAAPDPContextReject build() const;
    };

    static Builder builder();

    uint8_t pdpHandle() const { return mPDPHandle; }
    SMCause cause() const { return mCause; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3ActivateAAPDPContextReject> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSSessionManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Deactivate AA PDP Context Request (GSM 24.008 9.5.17) ─────────────
// Net->MS: pdpHandle(4)|spare(4)

class L3DeactivateAAPDPContextRequest {
    uint8_t mPDPHandle{0};

    friend struct Builder;
public:
    static constexpr int MTI = 0x53;

    struct Builder {
        uint8_t m_pdpHandle{0};

        /// Set PDP handle.
        Builder& pdpHandle(uint8_t v) { m_pdpHandle = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3DeactivateAAPDPContextRequest build() const;
    };

    static Builder builder();

    uint8_t pdpHandle() const { return mPDPHandle; }

    size_t bodyLength() const { return 1; }
    [[nodiscard]] static Expected<L3DeactivateAAPDPContextRequest> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSSessionManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Deactivate AA PDP Context Accept (GSM 24.008 9.5.17) ──────────────
// MS->Net: pdpHandle(4)|spare(4)

class L3DeactivateAAPDPContextAccept {
    uint8_t mPDPHandle{0};

    friend struct Builder;
public:
    static constexpr int MTI = 0x54;

    struct Builder {
        uint8_t m_pdpHandle{0};

        /// Set PDP handle.
        Builder& pdpHandle(uint8_t v) { m_pdpHandle = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3DeactivateAAPDPContextAccept build() const;
    };

    static Builder builder();

    uint8_t pdpHandle() const { return mPDPHandle; }

    size_t bodyLength() const { return 1; }
    [[nodiscard]] static Expected<L3DeactivateAAPDPContextAccept> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSSessionManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Activate MBMS Context Request (GSM 24.008 9.5.18) ─────────────────
// MS->Net: TMGI(TLV) | QoS(TLV) | [PCO(TLV)]

class L3ActivateMBMSContextRequest {
    L3TMGI mTMGI;
    L3QoS mQoS;
    bool mHavePCO{false};
    L3ProtocolConfigOptions mPCO;

    friend struct Builder;
public:
    static constexpr int MTI = 0x56;

    struct Builder {
        L3TMGI m_tmgi;
        L3QoS m_qos;
        bool m_havePCO{false};
        L3ProtocolConfigOptions m_pco;

        /// Set TMGI.
        Builder& tmgi(L3TMGI v) { m_tmgi = v; return *this; }
        /// Set QoS.
        Builder& qos(L3QoS v) { m_qos = v; return *this; }
        /// Set PCO (sets mHavePCO flag).
        Builder& pco(L3ProtocolConfigOptions v) { m_pco = v; m_havePCO = true; return *this; }
        /// Build the final message.
        [[nodiscard]] L3ActivateMBMSContextRequest build() const;
    };

    static Builder builder();

    const L3TMGI& tmgi() const { return mTMGI; }
    const L3QoS& qos() const { return mQoS; }
    bool hasPCO() const { return mHavePCO; }
    const L3ProtocolConfigOptions& pco() const { return mPCO; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3ActivateMBMSContextRequest> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSSessionManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Activate MBMS Context Accept (GSM 24.008 9.5.19) ──────────────────
// Net->MS: pdpHandle(4)|spare(4) | QoS(TLV) | [PCO(TLV)]

class L3ActivateMBMSContextAccept {
    uint8_t mPDPHandle{0};
    L3QoS mQoS;
    bool mHavePCO{false};
    L3ProtocolConfigOptions mPCO;

    friend struct Builder;
public:
    static constexpr int MTI = 0x57;

    struct Builder {
        uint8_t m_pdpHandle{0};
        L3QoS m_qos;
        bool m_havePCO{false};
        L3ProtocolConfigOptions m_pco;

        /// Set PDP handle.
        Builder& pdpHandle(uint8_t v) { m_pdpHandle = v; return *this; }
        /// Set QoS.
        Builder& qos(L3QoS v) { m_qos = v; return *this; }
        /// Set PCO (sets mHavePCO flag).
        Builder& pco(L3ProtocolConfigOptions v) { m_pco = v; m_havePCO = true; return *this; }
        /// Build the final message.
        [[nodiscard]] L3ActivateMBMSContextAccept build() const;
    };

    static Builder builder();

    uint8_t pdpHandle() const { return mPDPHandle; }
    const L3QoS& qos() const { return mQoS; }
    bool hasPCO() const { return mHavePCO; }
    const L3ProtocolConfigOptions& pco() const { return mPCO; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3ActivateMBMSContextAccept> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSSessionManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Activate MBMS Context Reject (GSM 24.008 9.5.20) ──────────────────
// Net->MS: smCause(TLV)

class L3ActivateMBMSContextReject {
    SMCause mCause{SMCause::Unsupported_PDP_Address_Type};

    friend struct Builder;
public:
    static constexpr int MTI = 0x58;

    struct Builder {
        SMCause m_cause{SMCause::Unsupported_PDP_Address_Type};

        /// Set SM cause.
        Builder& cause(SMCause v) { m_cause = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3ActivateMBMSContextReject build() const;
    };

    static Builder builder();

    SMCause cause() const { return mCause; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3ActivateMBMSContextReject> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSSessionManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Request MBMS Context Activation (GSM 24.008 9.5.21) ───────────────
// Net->MS: TMGI(TLV) | QoS(TLV) | [PCO(TLV)]

class L3RequestMBMSContextActivation {
    L3TMGI mTMGI;
    L3QoS mQoS;
    bool mHavePCO{false};
    L3ProtocolConfigOptions mPCO;

    friend struct Builder;
public:
    static constexpr int MTI = 0x59;

    struct Builder {
        L3TMGI m_tmgi;
        L3QoS m_qos;
        bool m_havePCO{false};
        L3ProtocolConfigOptions m_pco;

        /// Set TMGI.
        Builder& tmgi(L3TMGI v) { m_tmgi = v; return *this; }
        /// Set QoS.
        Builder& qos(L3QoS v) { m_qos = v; return *this; }
        /// Set PCO (sets mHavePCO flag).
        Builder& pco(L3ProtocolConfigOptions v) { m_pco = v; m_havePCO = true; return *this; }
        /// Build the final message.
        [[nodiscard]] L3RequestMBMSContextActivation build() const;
    };

    static Builder builder();

    const L3TMGI& tmgi() const { return mTMGI; }
    const L3QoS& qos() const { return mQoS; }
    bool hasPCO() const { return mHavePCO; }
    const L3ProtocolConfigOptions& pco() const { return mPCO; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3RequestMBMSContextActivation> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSSessionManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Request MBMS Context Activation Reject (GSM 24.008 9.5.22) ────────
// MS->Net: smCause(TLV)

class L3RequestMBMSContextActivationReject {
    SMCause mCause{SMCause::Unsupported_PDP_Address_Type};

    friend struct Builder;
public:
    static constexpr int MTI = 0x5A;

    struct Builder {
        SMCause m_cause{SMCause::Unsupported_PDP_Address_Type};

        /// Set SM cause.
        Builder& cause(SMCause v) { m_cause = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3RequestMBMSContextActivationReject build() const;
    };

    static Builder builder();

    SMCause cause() const { return mCause; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3RequestMBMSContextActivationReject> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSSessionManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Request Secondary PDP Context Activation (GSM 24.008 9.5.23) ───────
// Net->MS: pdpHandle(4)|spare(4) | [PDPAddress(TLV)] | APN(TLV) | QoS(TLV) | [PCO(TLV)]

class L3RequestSecondaryPDPContextActivation {
    uint8_t mPDPHandle{0};
    bool mHavePDPAddress{false};
    L3PDPAddress mPDPAddress;
    L3AccessPointName mAPN;
    L3QoS mQoS;
    bool mHavePCO{false};
    L3ProtocolConfigOptions mPCO;

    friend struct Builder;
public:
    static constexpr int MTI = 0x5B;

    struct Builder {
        uint8_t m_pdpHandle{0};
        bool m_havePDPAddress{false};
        L3PDPAddress m_pdpAddress;
        L3AccessPointName m_apn;
        L3QoS m_qos;
        bool m_havePCO{false};
        L3ProtocolConfigOptions m_pco;

        /// Set PDP handle.
        Builder& pdpHandle(uint8_t v) { m_pdpHandle = v; return *this; }
        /// Set PDP address (sets mHavePDPAddress flag).
        Builder& pdpAddress(L3PDPAddress v) { m_pdpAddress = v; m_havePDPAddress = true; return *this; }
        /// Set APN.
        Builder& apn(L3AccessPointName v) { m_apn = v; return *this; }
        /// Set QoS.
        Builder& qos(L3QoS v) { m_qos = v; return *this; }
        /// Set PCO (sets mHavePCO flag).
        Builder& pco(L3ProtocolConfigOptions v) { m_pco = v; m_havePCO = true; return *this; }
        /// Build the final message.
        [[nodiscard]] L3RequestSecondaryPDPContextActivation build() const;
    };

    static Builder builder();

    uint8_t pdpHandle() const { return mPDPHandle; }
    bool hasPDPAddress() const { return mHavePDPAddress; }
    const L3PDPAddress& pdpAddress() const { return mPDPAddress; }
    const L3AccessPointName& apn() const { return mAPN; }
    const L3QoS& qos() const { return mQoS; }
    bool hasPCO() const { return mHavePCO; }
    const L3ProtocolConfigOptions& pco() const { return mPCO; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3RequestSecondaryPDPContextActivation> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSSessionManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Request Secondary PDP Context Activation Reject (GSM 24.008 9.5.24) ─
// MS->Net: pdpHandle(4)|spare(4) | smCause(TLV)

class L3RequestSecondaryPDPContextActivationReject {
    uint8_t mPDPHandle{0};
    SMCause mCause{SMCause::Unsupported_PDP_Address_Type};

    friend struct Builder;
public:
    static constexpr int MTI = 0x5C;

    struct Builder {
        uint8_t m_pdpHandle{0};
        SMCause m_cause{SMCause::Unsupported_PDP_Address_Type};

        /// Set PDP handle.
        Builder& pdpHandle(uint8_t v) { m_pdpHandle = v; return *this; }
        /// Set SM cause.
        Builder& cause(SMCause v) { m_cause = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3RequestSecondaryPDPContextActivationReject build() const;
    };

    static Builder builder();

    uint8_t pdpHandle() const { return mPDPHandle; }
    SMCause cause() const { return mCause; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3RequestSecondaryPDPContextActivationReject> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSSessionManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── SM Notification (GSM 24.008 9.5.25) ───────────────────────────────
// Net->MS: pdpHandle(4)|spare(4)

class L3SMNotification {
    uint8_t mPDPHandle{0};

    friend struct Builder;
public:
    static constexpr int MTI = 0x5D;

    struct Builder {
        uint8_t m_pdpHandle{0};

        /// Set PDP handle.
        Builder& pdpHandle(uint8_t v) { m_pdpHandle = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3SMNotification build() const;
    };

    static Builder builder();

    uint8_t pdpHandle() const { return mPDPHandle; }

    size_t bodyLength() const { return 1; }
    [[nodiscard]] static Expected<L3SMNotification> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSSessionManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// SM message type names for text output.
const char* smMessageName(int mti);

} // namespace gsml3parser
