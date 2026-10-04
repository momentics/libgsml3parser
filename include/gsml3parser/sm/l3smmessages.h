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
// Spec: 3GPP TS 44.068 (GSM 24.008) section 9.5.
//
// L3 header (per TS 24.008 section 10.4a):
//   Byte 0: TI(3) | TIF(1) | PD(4)=0x0A(SM)
//   Byte 1: MessageType(8 bits, raw - no NSD field)
//   Body: [message-specific fields]

#pragma once

#include <cstdint>
#include <ostream>
#include <span>
#include <string>
#include <vector>

#include "../expected.h"
#include "../bitreader.h"
#include "../bitwriter.h"
#include "../types.h"
#include "l3smelements.h"

namespace gsml3parser {

// ── Activate PDP Context Request (TS 44.068 section 9.5) ──────────────
// MS->SGSN: requestedNSAPI(4)|spare(4) | requestedLLCSAPI(4)|spare(4) |
// requestedQoS(LV, mandatory) | requestedPDPaddress(LV, mandatory) |
// accessPointName(TLV, IEI=0x28, mandatory) | [protocolConfigOpts(TLV,
// IEI=0x27)] | [requestType TV: one octet, high nibble '1010'B + value] |
// [opaque optional IEs].
// Unrecognized optional information elements are kept as an opaque
// sequence and re-emitted verbatim (TS 24.068 optional IEs).

class L3ActivatePDPContextRequest {
    uint8_t mNSapi{0};
    uint8_t mLLcSapi{0};
    L3QoS mQoS;
    L3PDPAddress mPDPAddress;
    L3AccessPointName mAPN;
    bool mHavePCO{false};
    L3ProtocolConfigOptions mPCO;
    bool mHasRequestType{false};
    uint8_t mRequestType{0};
    std::vector<uint8_t> mAdditionalIes;

    friend struct Builder;
public:
    static constexpr int MTI = 0x41;

    struct Builder {
        uint8_t m_nsapi{0};
        uint8_t m_llcSapi{0};
        L3QoS m_qos;
        L3PDPAddress m_pdpAddress;
        L3AccessPointName m_apn;
        bool m_havePCO{false};
        L3ProtocolConfigOptions m_pco;
        bool m_hasRequestType{false};
        uint8_t m_requestType{0};
        std::vector<uint8_t> m_additionalIes;

        /// Set the requested network layer service access point identifier (four bits).
        Builder& nsapi(unsigned v) { m_nsapi = static_cast<uint8_t>(v & 0x0Fu); return *this; }
        /// Set the requested LLC service access point identifier (four bits).
        Builder& llcSapi(unsigned v) { m_llcSapi = static_cast<uint8_t>(v & 0x0Fu); return *this; }
        /// Set the requested QoS profile (mandatory LV on the wire).
        Builder& qos(L3QoS v) { m_qos = v; return *this; }
        /// Set the requested PDP type and address (mandatory LV on the wire).
        Builder& pdpAddress(L3PDPAddress v) { m_pdpAddress = v; return *this; }
        /// Set the access point name (mandatory TLV on the wire).
        Builder& apn(L3AccessPointName v) { m_apn = v; return *this; }
        /// Set protocol configuration options (optional TLV).
        Builder& pco(L3ProtocolConfigOptions v) { m_pco = v; m_havePCO = true; return *this; }
        /// Set the request type value (TS 44.068), written as one octet '1010'B|value.
        Builder& requestType(unsigned t) { m_requestType = static_cast<uint8_t>(t & 0x0Fu); m_hasRequestType = true; return *this; }
        /// Set the opaque sequence of additional optional IEs.
        Builder& additionalIes(std::span<const uint8_t> v) {
            m_additionalIes.assign(v.begin(), v.end());
            return *this;
        }
        /// Build the final message.
        [[nodiscard]] L3ActivatePDPContextRequest build() const;
    };

    static Builder builder();

    /// Requested NSAPI (four bits).
    uint8_t nsapi() const { return mNSapi; }
    /// Requested LLC SAPI (four bits).
    uint8_t llcSapi() const { return mLLcSapi; }
    const L3QoS& qos() const { return mQoS; }
    const L3PDPAddress& pdpAddress() const { return mPDPAddress; }
    const L3AccessPointName& apn() const { return mAPN; }
    bool hasPCO() const { return mHavePCO; }
    const L3ProtocolConfigOptions& pco() const { return mPCO; }
    /// True when the request type TV octet was present.
    [[nodiscard]] bool hasRequestType() const { return mHasRequestType; }
    /// Request type value (low nibble of the TV octet, TS 44.068).
    [[nodiscard]] unsigned requestType() const { return mRequestType & 0x0Fu; }
    /// Opaque sequence of additional optional IEs, re-emitted verbatim.
    [[nodiscard]] const std::vector<uint8_t>& additionalIes() const { return mAdditionalIes; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3ActivatePDPContextRequest> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSSessionManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Activate PDP Context Accept (TS 44.068 section 9.5) ───────────────
// SGSN->MS: negotiatedLLCSAPI(4)|spare(4) | negotiatedQoS(LV) |
// radioPriorityForSMS(4)|spare(4) | [PDP type and address (TLV, IEI=0x2B)] |
// [protocolConfigOpts(TLV, IEI=0x27)] | [opaque optional IEs].

class L3ActivatePDPContextAccept {
    uint8_t mLLcSapi{0};
    L3QoS mQoS;
    uint8_t mRadioPriority{0};
    bool mHavePDPAddress{false};
    L3PDPAddress mPDPAddress;
    bool mHavePCO{false};
    L3ProtocolConfigOptions mPCO;
    std::vector<uint8_t> mAdditionalIes;

    friend struct Builder;
public:
    static constexpr int MTI = 0x42;

    struct Builder {
        uint8_t m_llcSapi{0};
        L3QoS m_qos;
        uint8_t m_radioPriority{0};
        bool m_havePDPAddress{false};
        L3PDPAddress m_pdpAddress;
        bool m_havePCO{false};
        L3ProtocolConfigOptions m_pco;
        std::vector<uint8_t> m_additionalIes;

        /// Set the negotiated LLC SAPI (four bits).
        Builder& llcSapi(unsigned v) { m_llcSapi = static_cast<uint8_t>(v & 0x0Fu); return *this; }
        /// Set the negotiated QoS profile (LV on the wire).
        Builder& qos(L3QoS v) { m_qos = v; return *this; }
        /// Set the radio priority for SMS (four bits).
        Builder& radioPriority(unsigned v) { m_radioPriority = static_cast<uint8_t>(v & 0x0Fu); return *this; }
        /// Set PDP type and address (optional TLV, sets mHavePDPAddress flag).
        Builder& pdpAddress(L3PDPAddress v) { m_pdpAddress = v; m_havePDPAddress = true; return *this; }
        /// Set PCO (sets mHavePCO flag).
        Builder& pco(L3ProtocolConfigOptions v) { m_pco = v; m_havePCO = true; return *this; }
        /// Set the opaque sequence of additional optional IEs.
        Builder& additionalIes(std::span<const uint8_t> v) {
            m_additionalIes.assign(v.begin(), v.end());
            return *this;
        }
        /// Build the final message.
        [[nodiscard]] L3ActivatePDPContextAccept build() const;
    };

    static Builder builder();

    /// Negotiated LLC SAPI (four bits).
    uint8_t llcSapi() const { return mLLcSapi; }
    const L3QoS& qos() const { return mQoS; }
    /// Radio priority for SMS (four bits).
    uint8_t radioPriority() const { return mRadioPriority; }
    bool hasPDPAddress() const { return mHavePDPAddress; }
    const L3PDPAddress& pdpAddress() const { return mPDPAddress; }
    bool hasPCO() const { return mHavePCO; }
    const L3ProtocolConfigOptions& pco() const { return mPCO; }
    /// Opaque sequence of additional optional IEs, re-emitted verbatim.
    [[nodiscard]] const std::vector<uint8_t>& additionalIes() const { return mAdditionalIes; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3ActivatePDPContextAccept> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSSessionManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Activate PDP Context Reject (TS 44.068 section 9.5) ───────────────
// SGSN->MS: smCause(value octet, no identifier) | [opaque optional IEs
// (protocol configuration options, back-off timer, re-attempt indicator)].

class L3ActivatePDPContextReject {
    SMCause mCause{SMCause::Unsupported_PDP_Address_Type};
    std::vector<uint8_t> mAdditionalIes;

    friend struct Builder;
public:
    static constexpr int MTI = 0x43;

    struct Builder {
        SMCause m_cause{SMCause::Unsupported_PDP_Address_Type};
        std::vector<uint8_t> m_additionalIes;

        /// Set SM cause (first body octet on the wire).
        Builder& cause(SMCause v) { m_cause = v; return *this; }
        /// Set the opaque sequence of additional optional IEs.
        Builder& additionalIes(std::span<const uint8_t> v) {
            m_additionalIes.assign(v.begin(), v.end());
            return *this;
        }
        /// Build the final message.
        [[nodiscard]] L3ActivatePDPContextReject build() const;
    };

    static Builder builder();

    SMCause cause() const { return mCause; }
    /// Opaque sequence of additional optional IEs, re-emitted verbatim.
    [[nodiscard]] const std::vector<uint8_t>& additionalIes() const { return mAdditionalIes; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3ActivatePDPContextReject> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSSessionManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Deactivate PDP Context Request (TS 44.068 section 9.5) ────────────
// Bidirectional: smCause(value octet, no identifier, mandatory) |
// [tearDownIndicator TV: one octet '1001'B + [flag(1)|spare(3)]] |
// [protocolConfigOpts(TLV, IEI=0x27)] | [opaque optional IEs (MBMS PCO,
// T3396 timer value, WLAN offload indication)].

class L3DeactivatePDPContextRequest {
    SMCause mCause{SMCause::ReqAccepted};
    bool mHasTearDownIndicator{false};
    L3TearDownIndicator mTearDownIndicator;
    bool mHavePCO{false};
    L3ProtocolConfigOptions mPCO;
    std::vector<uint8_t> mAdditionalIes;

    friend struct Builder;
public:
    static constexpr int MTI = 0x46;

    struct Builder {
        SMCause m_cause{SMCause::ReqAccepted};
        bool m_hasTearDownIndicator{false};
        L3TearDownIndicator m_tearDownIndicator;
        bool m_havePCO{false};
        L3ProtocolConfigOptions m_pco;
        std::vector<uint8_t> m_additionalIes;

        /// Set SM cause (first body octet on the wire).
        Builder& cause(SMCause v) { m_cause = v; return *this; }
        /// Set the tear-down indicator TV (optional).
        Builder& tearDownIndicator(L3TearDownIndicator v) { m_tearDownIndicator = v; m_hasTearDownIndicator = true; return *this; }
        /// Convenience: set the tear-down flag.
        Builder& tearDownIndicator(bool flag) { m_tearDownIndicator = L3TearDownIndicator{flag}; m_hasTearDownIndicator = true; return *this; }
        /// Set protocol configuration options (optional TLV).
        Builder& pco(L3ProtocolConfigOptions v) { m_pco = v; m_havePCO = true; return *this; }
        /// Set the opaque sequence of additional optional IEs.
        Builder& additionalIes(std::span<const uint8_t> v) {
            m_additionalIes.assign(v.begin(), v.end());
            return *this;
        }
        /// Build the final message.
        [[nodiscard]] L3DeactivatePDPContextRequest build() const;
    };

    static Builder builder();

    SMCause cause() const { return mCause; }
    /// True when the tear-down indicator TV octet is present.
    [[nodiscard]] bool hasTearDownIndicator() const { return mHasTearDownIndicator; }
    /// Tear-down indicator value (TS 44.068 section 10.5.6.10).
    bool tearDownIndicator() const { return mTearDownIndicator.flag(); }
    bool hasPCO() const { return mHavePCO; }
    const L3ProtocolConfigOptions& protocolConfigOptions() const { return mPCO; }
    /// Opaque sequence of additional optional IEs, re-emitted verbatim.
    [[nodiscard]] const std::vector<uint8_t>& additionalIes() const { return mAdditionalIes; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3DeactivatePDPContextRequest> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSSessionManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Deactivate PDP Context Accept (TS 44.068 section 9.5) ─────────────
// Bidirectional: no fixed body fields; optional protocol configuration
// options and other IEs are kept as an opaque sequence.

class L3DeactivatePDPContextAccept {
    std::vector<uint8_t> mAdditionalIes;

    friend struct Builder;
public:
    static constexpr int MTI = 0x47;

    struct Builder {
        std::vector<uint8_t> m_additionalIes;

        /// Set the opaque sequence of optional IEs.
        Builder& additionalIes(std::span<const uint8_t> v) {
            m_additionalIes.assign(v.begin(), v.end());
            return *this;
        }
        /// Build the final message.
        [[nodiscard]] L3DeactivatePDPContextAccept build() const;
    };

    static Builder builder();

    /// Opaque sequence of optional IEs, re-emitted verbatim.
    [[nodiscard]] const std::vector<uint8_t>& additionalIes() const { return mAdditionalIes; }

    size_t bodyLength() const { return mAdditionalIes.size(); }
    [[nodiscard]] static Expected<L3DeactivatePDPContextAccept> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSSessionManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Modify PDP Context Request (TS 44.068 section 9.5) ────────────────
// SGSN->MS: pdpHandle(4)|spare(4) | QoS(LV) | [PCO(TLV, IEI=0x27)] |
// [opaque optional IEs].

class L3ModifyPDPContextRequest {
    uint8_t mPDPHandle{0};
    L3QoS mQoS;
    bool mHavePCO{false};
    L3ProtocolConfigOptions mPCO;
    std::vector<uint8_t> mAdditionalIes;

    friend struct Builder;
public:
    static constexpr int MTI = 0x48;

    struct Builder {
        uint8_t m_pdpHandle{0};
        L3QoS m_qos;
        bool m_havePCO{false};
        L3ProtocolConfigOptions m_pco;
        std::vector<uint8_t> m_additionalIes;

        /// Set PDP handle (four bits).
        Builder& pdpHandle(unsigned v) { m_pdpHandle = static_cast<uint8_t>(v & 0x0Fu); return *this; }
        /// Set QoS (LV on the wire).
        Builder& qos(L3QoS v) { m_qos = v; return *this; }
        /// Set PCO (sets mHavePCO flag).
        Builder& pco(L3ProtocolConfigOptions v) { m_pco = v; m_havePCO = true; return *this; }
        /// Set the opaque sequence of additional optional IEs.
        Builder& additionalIes(std::span<const uint8_t> v) {
            m_additionalIes.assign(v.begin(), v.end());
            return *this;
        }
        /// Build the final message.
        [[nodiscard]] L3ModifyPDPContextRequest build() const;
    };

    static Builder builder();

    uint8_t pdpHandle() const { return mPDPHandle; }
    const L3QoS& qos() const { return mQoS; }
    bool hasPCO() const { return mHavePCO; }
    const L3ProtocolConfigOptions& pco() const { return mPCO; }
    /// Opaque sequence of additional optional IEs, re-emitted verbatim.
    [[nodiscard]] const std::vector<uint8_t>& additionalIes() const { return mAdditionalIes; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3ModifyPDPContextRequest> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSSessionManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Modify PDP Context Accept (TS 44.068 section 9.5) ─────────────────
// MS->SGSN: pdpHandle(4)|spare(4) | QoS(LV) | [PCO(TLV, IEI=0x27)] |
// [opaque optional IEs].

class L3ModifyPDPContextAccept {
    uint8_t mPDPHandle{0};
    L3QoS mQoS;
    bool mHavePCO{false};
    L3ProtocolConfigOptions mPCO;
    std::vector<uint8_t> mAdditionalIes;

    friend struct Builder;
public:
    static constexpr int MTI = 0x49;

    struct Builder {
        uint8_t m_pdpHandle{0};
        L3QoS m_qos;
        bool m_havePCO{false};
        L3ProtocolConfigOptions m_pco;
        std::vector<uint8_t> m_additionalIes;

        /// Set PDP handle (four bits).
        Builder& pdpHandle(unsigned v) { m_pdpHandle = static_cast<uint8_t>(v & 0x0Fu); return *this; }
        /// Set QoS (LV on the wire).
        Builder& qos(L3QoS v) { m_qos = v; return *this; }
        /// Set PCO (sets mHavePCO flag).
        Builder& pco(L3ProtocolConfigOptions v) { m_pco = v; m_havePCO = true; return *this; }
        /// Set the opaque sequence of additional optional IEs.
        Builder& additionalIes(std::span<const uint8_t> v) {
            m_additionalIes.assign(v.begin(), v.end());
            return *this;
        }
        /// Build the final message.
        [[nodiscard]] L3ModifyPDPContextAccept build() const;
    };

    static Builder builder();

    uint8_t pdpHandle() const { return mPDPHandle; }
    const L3QoS& qos() const { return mQoS; }
    bool hasPCO() const { return mHavePCO; }
    const L3ProtocolConfigOptions& pco() const { return mPCO; }
    /// Opaque sequence of additional optional IEs, re-emitted verbatim.
    [[nodiscard]] const std::vector<uint8_t>& additionalIes() const { return mAdditionalIes; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3ModifyPDPContextAccept> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSSessionManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Modify PDP Context Reject (TS 44.068 section 9.5) ─────────────────
// Bidirectional: smCause(value octet, no identifier) | [opaque optional
// IEs (back-off timer and others)].

class L3ModifyPDPContextReject {
    SMCause mCause{SMCause::Unsupported_PDP_Address_Type};
    std::vector<uint8_t> mAdditionalIes;

    friend struct Builder;
public:
    static constexpr int MTI = 0x4c;

    struct Builder {
        SMCause m_cause{SMCause::Unsupported_PDP_Address_Type};
        std::vector<uint8_t> m_additionalIes;

        /// Set SM cause (first body octet on the wire).
        Builder& cause(SMCause v) { m_cause = v; return *this; }
        /// Set the opaque sequence of additional optional IEs.
        Builder& additionalIes(std::span<const uint8_t> v) {
            m_additionalIes.assign(v.begin(), v.end());
            return *this;
        }
        /// Build the final message.
        [[nodiscard]] L3ModifyPDPContextReject build() const;
    };

    static Builder builder();

    SMCause cause() const { return mCause; }
    /// Opaque sequence of additional optional IEs, re-emitted verbatim.
    [[nodiscard]] const std::vector<uint8_t>& additionalIes() const { return mAdditionalIes; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3ModifyPDPContextReject> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSSessionManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── SM Status (TS 44.068 section 9.5) ─────────────────────────────────
// Bidirectional: smCause(value octet, no identifier) | [opaque optional IEs].

class L3SMStatus {
    SMCause mCause{SMCause::ReqAccepted};
    std::vector<uint8_t> mAdditionalIes;

    friend struct Builder;
public:
    static constexpr int MTI = 0x55;
    L3SMStatus() = default;
    explicit L3SMStatus(SMCause cause) : mCause(cause) {}

    struct Builder {
        SMCause m_cause{SMCause::ReqAccepted};
        std::vector<uint8_t> m_additionalIes;

        /// Set SM cause (first body octet on the wire).
        Builder& cause(SMCause v) { m_cause = v; return *this; }
        /// Set the opaque sequence of additional optional IEs.
        Builder& additionalIes(std::span<const uint8_t> v) {
            m_additionalIes.assign(v.begin(), v.end());
            return *this;
        }
        /// Build the final message.
        [[nodiscard]] L3SMStatus build() const;
    };

    static Builder builder();

    SMCause cause() const { return mCause; }
    /// Opaque sequence of additional optional IEs, re-emitted verbatim.
    [[nodiscard]] const std::vector<uint8_t>& additionalIes() const { return mAdditionalIes; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3SMStatus> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSSessionManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Request PDP Context Activation (TS 44.068 section 9.5) ────────────
// Net->MS: pdpHandle(4)|spare(4) | QoS(LV) | PDP address(LV) |
// APN(TLV, IEI=0x28) | [PCO(TLV, IEI=0x27)] | [opaque optional IEs].

class L3RequestPDPContextActivation {
    uint8_t mPDPHandle{0};
    L3QoS mQoS;
    L3PDPAddress mPDPAddress;
    L3AccessPointName mAPN;
    bool mHavePCO{false};
    L3ProtocolConfigOptions mPCO;
    std::vector<uint8_t> mAdditionalIes;

    friend struct Builder;
public:
    static constexpr int MTI = 0x44;

    struct Builder {
        uint8_t m_pdpHandle{0};
        L3QoS m_qos;
        L3PDPAddress m_pdpAddress;
        L3AccessPointName m_apn;
        bool m_havePCO{false};
        L3ProtocolConfigOptions m_pco;
        std::vector<uint8_t> m_additionalIes;

        /// Set PDP handle (four bits).
        Builder& pdpHandle(unsigned v) { m_pdpHandle = static_cast<uint8_t>(v & 0x0Fu); return *this; }
        /// Set QoS (LV on the wire).
        Builder& qos(L3QoS v) { m_qos = v; return *this; }
        /// Set PDP type and address (LV on the wire).
        Builder& pdpAddress(L3PDPAddress v) { m_pdpAddress = v; return *this; }
        /// Set APN (TLV on the wire).
        Builder& apn(L3AccessPointName v) { m_apn = v; return *this; }
        /// Set PCO (sets mHavePCO flag).
        Builder& pco(L3ProtocolConfigOptions v) { m_pco = v; m_havePCO = true; return *this; }
        /// Set the opaque sequence of additional optional IEs.
        Builder& additionalIes(std::span<const uint8_t> v) {
            m_additionalIes.assign(v.begin(), v.end());
            return *this;
        }
        /// Build the final message.
        [[nodiscard]] L3RequestPDPContextActivation build() const;
    };

    static Builder builder();

    uint8_t pdpHandle() const { return mPDPHandle; }
    const L3QoS& qos() const { return mQoS; }
    const L3PDPAddress& pdpAddress() const { return mPDPAddress; }
    const L3AccessPointName& apn() const { return mAPN; }
    bool hasPCO() const { return mHavePCO; }
    const L3ProtocolConfigOptions& pco() const { return mPCO; }
    /// Opaque sequence of additional optional IEs, re-emitted verbatim.
    [[nodiscard]] const std::vector<uint8_t>& additionalIes() const { return mAdditionalIes; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3RequestPDPContextActivation> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSSessionManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Request PDP Context Activation Reject (TS 44.068 section 9.5) ─────
// MS->Net: smCause(value octet, no identifier) | [opaque optional IEs].

class L3RequestPDPContextActivationReject {
    SMCause mCause{SMCause::Unsupported_PDP_Address_Type};
    std::vector<uint8_t> mAdditionalIes;

    friend struct Builder;
public:
    static constexpr int MTI = 0x45;

    struct Builder {
        SMCause m_cause{SMCause::Unsupported_PDP_Address_Type};
        std::vector<uint8_t> m_additionalIes;

        /// Set SM cause (first body octet on the wire).
        Builder& cause(SMCause v) { m_cause = v; return *this; }
        /// Set the opaque sequence of additional optional IEs.
        Builder& additionalIes(std::span<const uint8_t> v) {
            m_additionalIes.assign(v.begin(), v.end());
            return *this;
        }
        /// Build the final message.
        [[nodiscard]] L3RequestPDPContextActivationReject build() const;
    };

    static Builder builder();

    SMCause cause() const { return mCause; }
    /// Opaque sequence of additional optional IEs, re-emitted verbatim.
    [[nodiscard]] const std::vector<uint8_t>& additionalIes() const { return mAdditionalIes; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3RequestPDPContextActivationReject> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSSessionManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Modify PDP Context Request (MS->Net) (TS 44.068 section 9.5) ──────
// MS->Net: pdpHandle(4)|spare(4) | QoS(LV) | [PCO(TLV, IEI=0x27)] |
// [opaque optional IEs].

class L3ModifyPDPContextRequestMS {
    uint8_t mPDPHandle{0};
    L3QoS mQoS;
    bool mHavePCO{false};
    L3ProtocolConfigOptions mPCO;
    std::vector<uint8_t> mAdditionalIes;

    friend struct Builder;
public:
    static constexpr int MTI = 0x4A;

    struct Builder {
        uint8_t m_pdpHandle{0};
        L3QoS m_qos;
        bool m_havePCO{false};
        L3ProtocolConfigOptions m_pco;
        std::vector<uint8_t> m_additionalIes;

        /// Set PDP handle (four bits).
        Builder& pdpHandle(unsigned v) { m_pdpHandle = static_cast<uint8_t>(v & 0x0Fu); return *this; }
        /// Set QoS (LV on the wire).
        Builder& qos(L3QoS v) { m_qos = v; return *this; }
        /// Set PCO (sets mHavePCO flag).
        Builder& pco(L3ProtocolConfigOptions v) { m_pco = v; m_havePCO = true; return *this; }
        /// Set the opaque sequence of additional optional IEs.
        Builder& additionalIes(std::span<const uint8_t> v) {
            m_additionalIes.assign(v.begin(), v.end());
            return *this;
        }
        /// Build the final message.
        [[nodiscard]] L3ModifyPDPContextRequestMS build() const;
    };

    static Builder builder();

    uint8_t pdpHandle() const { return mPDPHandle; }
    const L3QoS& qos() const { return mQoS; }
    bool hasPCO() const { return mHavePCO; }
    const L3ProtocolConfigOptions& pco() const { return mPCO; }
    /// Opaque sequence of additional optional IEs, re-emitted verbatim.
    [[nodiscard]] const std::vector<uint8_t>& additionalIes() const { return mAdditionalIes; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3ModifyPDPContextRequestMS> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSSessionManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Modify PDP Context Accept (Net->MS) (TS 44.068 section 9.5) ───────
// Net->MS: pdpHandle(4)|spare(4) | QoS(LV) | [PCO(TLV, IEI=0x27)] |
// [opaque optional IEs].

class L3ModifyPDPContextAcceptNet {
    uint8_t mPDPHandle{0};
    L3QoS mQoS;
    bool mHavePCO{false};
    L3ProtocolConfigOptions mPCO;
    std::vector<uint8_t> mAdditionalIes;

    friend struct Builder;
public:
    static constexpr int MTI = 0x4B;

    struct Builder {
        uint8_t m_pdpHandle{0};
        L3QoS m_qos;
        bool m_havePCO{false};
        L3ProtocolConfigOptions m_pco;
        std::vector<uint8_t> m_additionalIes;

        /// Set PDP handle (four bits).
        Builder& pdpHandle(unsigned v) { m_pdpHandle = static_cast<uint8_t>(v & 0x0Fu); return *this; }
        /// Set QoS (LV on the wire).
        Builder& qos(L3QoS v) { m_qos = v; return *this; }
        /// Set PCO (sets mHavePCO flag).
        Builder& pco(L3ProtocolConfigOptions v) { m_pco = v; m_havePCO = true; return *this; }
        /// Set the opaque sequence of additional optional IEs.
        Builder& additionalIes(std::span<const uint8_t> v) {
            m_additionalIes.assign(v.begin(), v.end());
            return *this;
        }
        /// Build the final message.
        [[nodiscard]] L3ModifyPDPContextAcceptNet build() const;
    };

    static Builder builder();

    uint8_t pdpHandle() const { return mPDPHandle; }
    const L3QoS& qos() const { return mQoS; }
    bool hasPCO() const { return mHavePCO; }
    const L3ProtocolConfigOptions& pco() const { return mPCO; }
    /// Opaque sequence of additional optional IEs, re-emitted verbatim.
    [[nodiscard]] const std::vector<uint8_t>& additionalIes() const { return mAdditionalIes; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3ModifyPDPContextAcceptNet> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSSessionManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Activate Secondary PDP Context Request (TS 44.068 section 9.5) ────
// Net->MS: pdpHandle(4)|spare(4) | QoS(LV) | PDP address(LV) |
// APN(TLV, IEI=0x28) | [PCO(TLV, IEI=0x27)] | [opaque optional IEs].

class L3ActivateSecondaryPDPContextRequest {
    uint8_t mPDPHandle{0};
    L3QoS mQoS;
    L3PDPAddress mPDPAddress;
    L3AccessPointName mAPN;
    bool mHavePCO{false};
    L3ProtocolConfigOptions mPCO;
    std::vector<uint8_t> mAdditionalIes;

    friend struct Builder;
public:
    static constexpr int MTI = 0x4D;

    struct Builder {
        uint8_t m_pdpHandle{0};
        L3QoS m_qos;
        L3PDPAddress m_pdpAddress;
        L3AccessPointName m_apn;
        bool m_havePCO{false};
        L3ProtocolConfigOptions m_pco;
        std::vector<uint8_t> m_additionalIes;

        /// Set PDP handle (four bits).
        Builder& pdpHandle(unsigned v) { m_pdpHandle = static_cast<uint8_t>(v & 0x0Fu); return *this; }
        /// Set QoS (LV on the wire).
        Builder& qos(L3QoS v) { m_qos = v; return *this; }
        /// Set PDP type and address (LV on the wire).
        Builder& pdpAddress(L3PDPAddress v) { m_pdpAddress = v; return *this; }
        /// Set APN (TLV on the wire).
        Builder& apn(L3AccessPointName v) { m_apn = v; return *this; }
        /// Set PCO (sets mHavePCO flag).
        Builder& pco(L3ProtocolConfigOptions v) { m_pco = v; m_havePCO = true; return *this; }
        /// Set the opaque sequence of additional optional IEs.
        Builder& additionalIes(std::span<const uint8_t> v) {
            m_additionalIes.assign(v.begin(), v.end());
            return *this;
        }
        /// Build the final message.
        [[nodiscard]] L3ActivateSecondaryPDPContextRequest build() const;
    };

    static Builder builder();

    uint8_t pdpHandle() const { return mPDPHandle; }
    const L3QoS& qos() const { return mQoS; }
    const L3PDPAddress& pdpAddress() const { return mPDPAddress; }
    const L3AccessPointName& apn() const { return mAPN; }
    bool hasPCO() const { return mHavePCO; }
    const L3ProtocolConfigOptions& pco() const { return mPCO; }
    /// Opaque sequence of additional optional IEs, re-emitted verbatim.
    [[nodiscard]] const std::vector<uint8_t>& additionalIes() const { return mAdditionalIes; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3ActivateSecondaryPDPContextRequest> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSSessionManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Activate Secondary PDP Context Accept (TS 44.068 section 9.5) ─────
// MS->Net: pdpHandle(4)|spare(4) | QoS(LV) | [PDP type and address
// (TLV, IEI=0x2B)] | [PCO(TLV, IEI=0x27)] | [opaque optional IEs].

class L3ActivateSecondaryPDPContextAccept {
    uint8_t mPDPHandle{0};
    L3QoS mQoS;
    bool mHavePDPAddress{false};
    L3PDPAddress mPDPAddress;
    bool mHavePCO{false};
    L3ProtocolConfigOptions mPCO;
    std::vector<uint8_t> mAdditionalIes;

    friend struct Builder;
public:
    static constexpr int MTI = 0x4E;

    struct Builder {
        uint8_t m_pdpHandle{0};
        L3QoS m_qos;
        bool m_havePDPAddress{false};
        L3PDPAddress m_pdpAddress;
        bool m_havePCO{false};
        L3ProtocolConfigOptions m_pco;
        std::vector<uint8_t> m_additionalIes;

        /// Set PDP handle (four bits).
        Builder& pdpHandle(unsigned v) { m_pdpHandle = static_cast<uint8_t>(v & 0x0Fu); return *this; }
        /// Set QoS (LV on the wire).
        Builder& qos(L3QoS v) { m_qos = v; return *this; }
        /// Set PDP type and address (optional TLV, sets mHavePDPAddress flag).
        Builder& pdpAddress(L3PDPAddress v) { m_pdpAddress = v; m_havePDPAddress = true; return *this; }
        /// Set PCO (sets mHavePCO flag).
        Builder& pco(L3ProtocolConfigOptions v) { m_pco = v; m_havePCO = true; return *this; }
        /// Set the opaque sequence of additional optional IEs.
        Builder& additionalIes(std::span<const uint8_t> v) {
            m_additionalIes.assign(v.begin(), v.end());
            return *this;
        }
        /// Build the final message.
        [[nodiscard]] L3ActivateSecondaryPDPContextAccept build() const;
    };

    static Builder builder();

    uint8_t pdpHandle() const { return mPDPHandle; }
    const L3QoS& qos() const { return mQoS; }
    bool hasPDPAddress() const { return mHavePDPAddress; }
    const L3PDPAddress& pdpAddress() const { return mPDPAddress; }
    bool hasPCO() const { return mHavePCO; }
    const L3ProtocolConfigOptions& pco() const { return mPCO; }
    /// Opaque sequence of additional optional IEs, re-emitted verbatim.
    [[nodiscard]] const std::vector<uint8_t>& additionalIes() const { return mAdditionalIes; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3ActivateSecondaryPDPContextAccept> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSSessionManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Activate Secondary PDP Context Reject (TS 44.068 section 9.5) ─────
// MS->Net: smCause(value octet, no identifier) | [opaque optional IEs].

class L3ActivateSecondaryPDPContextReject {
    SMCause mCause{SMCause::Unsupported_PDP_Address_Type};
    std::vector<uint8_t> mAdditionalIes;

    friend struct Builder;
public:
    static constexpr int MTI = 0x4F;

    struct Builder {
        SMCause m_cause{SMCause::Unsupported_PDP_Address_Type};
        std::vector<uint8_t> m_additionalIes;

        /// Set SM cause (first body octet on the wire).
        Builder& cause(SMCause v) { m_cause = v; return *this; }
        /// Set the opaque sequence of additional optional IEs.
        Builder& additionalIes(std::span<const uint8_t> v) {
            m_additionalIes.assign(v.begin(), v.end());
            return *this;
        }
        /// Build the final message.
        [[nodiscard]] L3ActivateSecondaryPDPContextReject build() const;
    };

    static Builder builder();

    SMCause cause() const { return mCause; }
    /// Opaque sequence of additional optional IEs, re-emitted verbatim.
    [[nodiscard]] const std::vector<uint8_t>& additionalIes() const { return mAdditionalIes; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3ActivateSecondaryPDPContextReject> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSSessionManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Activate MBMS Context Request (TS 44.068 section 9.5) ─────────────
// MS->Net: QoS(LV) | [PCO(TLV, IEI=0x27)] | [opaque optional IEs; the
// TMGI TLV is preserved in the opaque sequence].

class L3ActivateMBMSContextRequest {
    L3QoS mQoS;
    bool mHavePCO{false};
    L3ProtocolConfigOptions mPCO;
    std::vector<uint8_t> mAdditionalIes;

    friend struct Builder;
public:
    static constexpr int MTI = 0x56;

    struct Builder {
        L3QoS m_qos;
        bool m_havePCO{false};
        L3ProtocolConfigOptions m_pco;
        std::vector<uint8_t> m_additionalIes;

        /// Set QoS (LV on the wire).
        Builder& qos(L3QoS v) { m_qos = v; return *this; }
        /// Set PCO (sets mHavePCO flag).
        Builder& pco(L3ProtocolConfigOptions v) { m_pco = v; m_havePCO = true; return *this; }
        /// Set the opaque sequence of additional optional IEs.
        Builder& additionalIes(std::span<const uint8_t> v) {
            m_additionalIes.assign(v.begin(), v.end());
            return *this;
        }
        /// Build the final message.
        [[nodiscard]] L3ActivateMBMSContextRequest build() const;
    };

    static Builder builder();

    const L3QoS& qos() const { return mQoS; }
    bool hasPCO() const { return mHavePCO; }
    const L3ProtocolConfigOptions& pco() const { return mPCO; }
    /// Opaque sequence of additional optional IEs, re-emitted verbatim.
    [[nodiscard]] const std::vector<uint8_t>& additionalIes() const { return mAdditionalIes; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3ActivateMBMSContextRequest> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSSessionManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Activate MBMS Context Accept (TS 44.068 section 9.5) ──────────────
// Net->MS: pdpHandle(4)|spare(4) | QoS(LV) | [PCO(TLV, IEI=0x27)] |
// [opaque optional IEs].

class L3ActivateMBMSContextAccept {
    uint8_t mPDPHandle{0};
    L3QoS mQoS;
    bool mHavePCO{false};
    L3ProtocolConfigOptions mPCO;
    std::vector<uint8_t> mAdditionalIes;

    friend struct Builder;
public:
    static constexpr int MTI = 0x57;

    struct Builder {
        uint8_t m_pdpHandle{0};
        L3QoS m_qos;
        bool m_havePCO{false};
        L3ProtocolConfigOptions m_pco;
        std::vector<uint8_t> m_additionalIes;

        /// Set PDP handle (four bits).
        Builder& pdpHandle(unsigned v) { m_pdpHandle = static_cast<uint8_t>(v & 0x0Fu); return *this; }
        /// Set QoS (LV on the wire).
        Builder& qos(L3QoS v) { m_qos = v; return *this; }
        /// Set PCO (sets mHavePCO flag).
        Builder& pco(L3ProtocolConfigOptions v) { m_pco = v; m_havePCO = true; return *this; }
        /// Set the opaque sequence of additional optional IEs.
        Builder& additionalIes(std::span<const uint8_t> v) {
            m_additionalIes.assign(v.begin(), v.end());
            return *this;
        }
        /// Build the final message.
        [[nodiscard]] L3ActivateMBMSContextAccept build() const;
    };

    static Builder builder();

    uint8_t pdpHandle() const { return mPDPHandle; }
    const L3QoS& qos() const { return mQoS; }
    bool hasPCO() const { return mHavePCO; }
    const L3ProtocolConfigOptions& pco() const { return mPCO; }
    /// Opaque sequence of additional optional IEs, re-emitted verbatim.
    [[nodiscard]] const std::vector<uint8_t>& additionalIes() const { return mAdditionalIes; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3ActivateMBMSContextAccept> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSSessionManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Activate MBMS Context Reject (TS 44.068 section 9.5) ──────────────
// Net->MS: smCause(value octet, no identifier) | [opaque optional IEs].

class L3ActivateMBMSContextReject {
    SMCause mCause{SMCause::Unsupported_PDP_Address_Type};
    std::vector<uint8_t> mAdditionalIes;

    friend struct Builder;
public:
    static constexpr int MTI = 0x58;

    struct Builder {
        SMCause m_cause{SMCause::Unsupported_PDP_Address_Type};
        std::vector<uint8_t> m_additionalIes;

        /// Set SM cause (first body octet on the wire).
        Builder& cause(SMCause v) { m_cause = v; return *this; }
        /// Set the opaque sequence of additional optional IEs.
        Builder& additionalIes(std::span<const uint8_t> v) {
            m_additionalIes.assign(v.begin(), v.end());
            return *this;
        }
        /// Build the final message.
        [[nodiscard]] L3ActivateMBMSContextReject build() const;
    };

    static Builder builder();

    SMCause cause() const { return mCause; }
    /// Opaque sequence of additional optional IEs, re-emitted verbatim.
    [[nodiscard]] const std::vector<uint8_t>& additionalIes() const { return mAdditionalIes; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3ActivateMBMSContextReject> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSSessionManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Request MBMS Context Activation (TS 44.068 section 9.5) ───────────
// Net->MS: QoS(LV) | [PCO(TLV, IEI=0x27)] | [opaque optional IEs; the
// TMGI TLV is preserved in the opaque sequence].

class L3RequestMBMSContextActivation {
    L3QoS mQoS;
    bool mHavePCO{false};
    L3ProtocolConfigOptions mPCO;
    std::vector<uint8_t> mAdditionalIes;

    friend struct Builder;
public:
    static constexpr int MTI = 0x59;

    struct Builder {
        L3QoS m_qos;
        bool m_havePCO{false};
        L3ProtocolConfigOptions m_pco;
        std::vector<uint8_t> m_additionalIes;

        /// Set QoS (LV on the wire).
        Builder& qos(L3QoS v) { m_qos = v; return *this; }
        /// Set PCO (sets mHavePCO flag).
        Builder& pco(L3ProtocolConfigOptions v) { m_pco = v; m_havePCO = true; return *this; }
        /// Set the opaque sequence of additional optional IEs.
        Builder& additionalIes(std::span<const uint8_t> v) {
            m_additionalIes.assign(v.begin(), v.end());
            return *this;
        }
        /// Build the final message.
        [[nodiscard]] L3RequestMBMSContextActivation build() const;
    };

    static Builder builder();

    const L3QoS& qos() const { return mQoS; }
    bool hasPCO() const { return mHavePCO; }
    const L3ProtocolConfigOptions& pco() const { return mPCO; }
    /// Opaque sequence of additional optional IEs, re-emitted verbatim.
    [[nodiscard]] const std::vector<uint8_t>& additionalIes() const { return mAdditionalIes; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3RequestMBMSContextActivation> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSSessionManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Request MBMS Context Activation Reject (TS 44.068 section 9.5) ────
// MS->Net: smCause(value octet, no identifier) | [opaque optional IEs].

class L3RequestMBMSContextActivationReject {
    SMCause mCause{SMCause::Unsupported_PDP_Address_Type};
    std::vector<uint8_t> mAdditionalIes;

    friend struct Builder;
public:
    static constexpr int MTI = 0x5A;

    struct Builder {
        SMCause m_cause{SMCause::Unsupported_PDP_Address_Type};
        std::vector<uint8_t> m_additionalIes;

        /// Set SM cause (first body octet on the wire).
        Builder& cause(SMCause v) { m_cause = v; return *this; }
        /// Set the opaque sequence of additional optional IEs.
        Builder& additionalIes(std::span<const uint8_t> v) {
            m_additionalIes.assign(v.begin(), v.end());
            return *this;
        }
        /// Build the final message.
        [[nodiscard]] L3RequestMBMSContextActivationReject build() const;
    };

    static Builder builder();

    SMCause cause() const { return mCause; }
    /// Opaque sequence of additional optional IEs, re-emitted verbatim.
    [[nodiscard]] const std::vector<uint8_t>& additionalIes() const { return mAdditionalIes; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3RequestMBMSContextActivationReject> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSSessionManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Request Secondary PDP Context Activation (TS 44.068 section 9.5) ──
// Net->MS: pdpHandle(4)|spare(4) | QoS(LV) | PDP address(LV) |
// APN(TLV, IEI=0x28) | [PCO(TLV, IEI=0x27)] | [opaque optional IEs].

class L3RequestSecondaryPDPContextActivation {
    uint8_t mPDPHandle{0};
    L3QoS mQoS;
    L3PDPAddress mPDPAddress;
    L3AccessPointName mAPN;
    bool mHavePCO{false};
    L3ProtocolConfigOptions mPCO;
    std::vector<uint8_t> mAdditionalIes;

    friend struct Builder;
public:
    static constexpr int MTI = 0x5B;

    struct Builder {
        uint8_t m_pdpHandle{0};
        L3QoS m_qos;
        L3PDPAddress m_pdpAddress;
        L3AccessPointName m_apn;
        bool m_havePCO{false};
        L3ProtocolConfigOptions m_pco;
        std::vector<uint8_t> m_additionalIes;

        /// Set PDP handle (four bits).
        Builder& pdpHandle(unsigned v) { m_pdpHandle = static_cast<uint8_t>(v & 0x0Fu); return *this; }
        /// Set QoS (LV on the wire).
        Builder& qos(L3QoS v) { m_qos = v; return *this; }
        /// Set PDP type and address (LV on the wire).
        Builder& pdpAddress(L3PDPAddress v) { m_pdpAddress = v; return *this; }
        /// Set APN (TLV on the wire).
        Builder& apn(L3AccessPointName v) { m_apn = v; return *this; }
        /// Set PCO (sets mHavePCO flag).
        Builder& pco(L3ProtocolConfigOptions v) { m_pco = v; m_havePCO = true; return *this; }
        /// Set the opaque sequence of additional optional IEs.
        Builder& additionalIes(std::span<const uint8_t> v) {
            m_additionalIes.assign(v.begin(), v.end());
            return *this;
        }
        /// Build the final message.
        [[nodiscard]] L3RequestSecondaryPDPContextActivation build() const;
    };

    static Builder builder();

    uint8_t pdpHandle() const { return mPDPHandle; }
    const L3QoS& qos() const { return mQoS; }
    const L3PDPAddress& pdpAddress() const { return mPDPAddress; }
    const L3AccessPointName& apn() const { return mAPN; }
    bool hasPCO() const { return mHavePCO; }
    const L3ProtocolConfigOptions& pco() const { return mPCO; }
    /// Opaque sequence of additional optional IEs, re-emitted verbatim.
    [[nodiscard]] const std::vector<uint8_t>& additionalIes() const { return mAdditionalIes; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3RequestSecondaryPDPContextActivation> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSSessionManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Request Secondary PDP Context Activation Reject (TS 44.068 9.5) ───
// MS->Net: smCause(value octet, no identifier) | [opaque optional IEs].

class L3RequestSecondaryPDPContextActivationReject {
    SMCause mCause{SMCause::Unsupported_PDP_Address_Type};
    std::vector<uint8_t> mAdditionalIes;

    friend struct Builder;
public:
    static constexpr int MTI = 0x5C;

    struct Builder {
        SMCause m_cause{SMCause::Unsupported_PDP_Address_Type};
        std::vector<uint8_t> m_additionalIes;

        /// Set SM cause (first body octet on the wire).
        Builder& cause(SMCause v) { m_cause = v; return *this; }
        /// Set the opaque sequence of additional optional IEs.
        Builder& additionalIes(std::span<const uint8_t> v) {
            m_additionalIes.assign(v.begin(), v.end());
            return *this;
        }
        /// Build the final message.
        [[nodiscard]] L3RequestSecondaryPDPContextActivationReject build() const;
    };

    static Builder builder();

    SMCause cause() const { return mCause; }
    /// Opaque sequence of additional optional IEs, re-emitted verbatim.
    [[nodiscard]] const std::vector<uint8_t>& additionalIes() const { return mAdditionalIes; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3RequestSecondaryPDPContextActivationReject> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GPRSSessionManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── SM Notification (TS 44.068 section 9.5) ───────────────────────────
// Net->MS: pdpHandle(4)|spare(4) | [opaque optional IEs].

class L3SMNotification {
    uint8_t mPDPHandle{0};
    std::vector<uint8_t> mAdditionalIes;

    friend struct Builder;
public:
    static constexpr int MTI = 0x5D;

    struct Builder {
        uint8_t m_pdpHandle{0};
        std::vector<uint8_t> m_additionalIes;

        /// Set PDP handle (four bits).
        Builder& pdpHandle(unsigned v) { m_pdpHandle = static_cast<uint8_t>(v & 0x0Fu); return *this; }
        /// Set the opaque sequence of additional optional IEs.
        Builder& additionalIes(std::span<const uint8_t> v) {
            m_additionalIes.assign(v.begin(), v.end());
            return *this;
        }
        /// Build the final message.
        [[nodiscard]] L3SMNotification build() const;
    };

    static Builder builder();

    uint8_t pdpHandle() const { return mPDPHandle; }
    /// Opaque sequence of additional optional IEs, re-emitted verbatim.
    [[nodiscard]] const std::vector<uint8_t>& additionalIes() const { return mAdditionalIes; }

    size_t bodyLength() const { return 1 + mAdditionalIes.size(); }
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
