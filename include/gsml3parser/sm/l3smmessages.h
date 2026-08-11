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

// SM Message Classes — GSM L3 GPRS Session Management messages
// Spec: 3GPP TS 24.008 sections 9.5, Table 10.4a
// Reference: ref/osmo-ttcn3-hacks/library/L3_Templates.ttcn — SM message templates
//            ref/OpenBTS/SGSNGGSN/GPRSL3Messages.h — L3SmMsg::MessageType enum
//
// L3 header (per 24.008 10.4a):
//   Byte 0: PD(4)=0x0A(SM) | Skip(4)
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
public:
    static constexpr int MTI = 0x41;

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
public:
    static constexpr int MTI = 0x42;

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
public:
    static constexpr int MTI = 0x43;

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
public:
    static constexpr int MTI = 0x46;

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
public:
    static constexpr int MTI = 0x47;

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
public:
    static constexpr int MTI = 0x48;

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
public:
    static constexpr int MTI = 0x49;

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
public:
    static constexpr int MTI = 0x4c;

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
public:
    static constexpr int MTI = 0x55;
    L3SMStatus() = default;
    explicit L3SMStatus(SMCause cause) : mCause(cause) {}

    SMCause cause() const { return mCause; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3SMStatus> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSSessionManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// SM message type names for text output.
const char* smMessageName(int mti);

} // namespace gsml3parser
