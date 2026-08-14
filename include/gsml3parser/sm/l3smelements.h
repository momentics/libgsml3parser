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

// SM Information Elements - GSM L3 GPRS Session Management IE definitions
// Spec: 3GPP TS 24.008 section 10.5.8, Table 10.5a
// Reference: ref/osmo-ttcn3-hacks/library/L3_Templates.ttcn - SM IE templates
//
// Bit layout (per spec):
//   PDPAddress: TLV(IEI=0x08) | Length(1) | PDPType(1) | Address(variable)
//   QoS: TLV(IEI=0x09) | Length(1) | QoSElement(...)
//   AccessPointName: TLV(IEI=0x2F) | Length(1) | APNString(variable UTF-8)
//   ProtocolConfigOptions: TLV(IEI=0x3C) | Length(1) | Type(1) | ConfigData(variable)
//   SMCause: TV(IEI=0x27) | CauseValue(1)
//   BackOffTimer: TV(IEI=0x28) | TimerValue(1)

#pragma once

#include <array>
#include <cstdint>
#include <ostream>
#include <string>
#include <vector>

#include "../expected.h"
#include "../bitreader.h"
#include "../bitwriter.h"
#include "../types.h"

namespace gsml3parser {

// ── PDP Type (GSM 24.008 10.5.8.1) ────────────────────────────────────
// Identifies the protocol for which a PDP context is activated.

enum class PDPType : uint8_t {
    IPv4      = 0,
    IPv6      = 2,
    IPsecAH   = 3,
    PPP       = 6,
    Private   = 0xF0,
    Unknown   = 0xFF
};

// ── QoS Type (GSM 24.008 10.5.8.2) ────────────────────────────────────
// Indicates the nature of the QoS profile.

enum class QoSType : uint8_t {
    Requested = 0,
    Default   = 1,
    Teardown  = 3
};

// ── SM Cause (GSM 24.008 10.5.3.2.3) ──────────────────────────────────
// Cause values specific to Session Management procedures.

enum class SMCause : uint8_t {
    ReqAccepted                     = 1,
    Unsupported_PDP_Address_Type    = 19,
    Service_Opcode_NotSupported     = 20,
    Multicast_Context_Ack           = 22,
    Multicast_Context_Reject        = 23,
    Multicast_Context_Deactivate    = 24,
    Invalid_Flow_Desc               = 26,
    Multicast_PDP_No_Bearer         = 27,
    PDP_Auth_Failed_Primary_PDN     = 39,
    PDP_Auth_Failed_Secondary_PDN   = 40,
    Semantically_Incorrect_Message  = 95,
    Invalid_Mandatory_Information   = 96,
    Message_Type_Invalid            = 97,
    Message_Type_Not_Compatible     = 98,
    IE_Invalid                      = 99,
    Conditional_IE_Error            = 100,
    Message_Not_Compatible          = 101,
    Protocol_Error_Unspecified      = 111
};

const char* SMCause2Str(SMCause cause);

// ── PDP Address (GSM 24.008 10.5.8.1) ─────────────────────────────────
// TLV format: IEI=0x08 | Length(1) | PDPType(1) | Address(variable)
// For IPv4: 4 bytes; for IPv6: 16 bytes; PPP: variable; Private: variable

class L3PDPAddress {
    PDPType mType{PDPType::IPv4};
    std::vector<uint8_t> mAddress;
public:
    static constexpr uint8_t IEI = 0x08;
    L3PDPAddress() = default;
    L3PDPAddress(PDPType type, std::vector<uint8_t> addr)
        : mType(type), mAddress(std::move(addr)) {}

    bool operator==(const L3PDPAddress&) const = default;

    PDPType type() const { return mType; }
    const std::vector<uint8_t>& address() const { return mAddress; }
    size_t lengthV() const { return 1 + mAddress.size(); }

    [[nodiscard]] static Expected<L3PDPAddress> parse(BitReader& br, size_t lengthBytes);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── QoS Element (GSM 24.008 10.5.8.2) ─────────────────────────────────
// TLV format: IEI=0x09 | Length(1) | Type(1) | QoSElement(...)
// Each QoS element is a sub-structure with its own type and value.

enum class QoSElementType : uint8_t {
    QoSClass            = 1,
    MaxBitRateUL        = 2,
    MaxBitRateDL        = 3,
    Delay               = 4,
    DeliveryOrder       = 5,
    DeliveryOfExcessPackets = 6,
    SopClass            = 7,
    ResidualErrorRate   = 8,
    PeakThroughput      = 9,
    MeanThroughputDL    = 10,
    MeanThroughputUL    = 11,
    TrafficClass        = 12,
    GuaranteedBitRateDL = 13,
    GuaranteedBitRateUL = 14,
    MaxBitRateDL_SRB    = 15,
    MaxBitRateUL_SRB    = 16,
    GPRS_Priority       = 17,
    ExternalPriority    = 18
};

// ── QoS Profile (GSM 24.008 10.5.8.2) ─────────────────────────────────
// Contains the QoS type and a collection of QoS elements.

class L3QoS {
    QoSType mType{QoSType::Requested};
    std::vector<uint8_t> mElements;
public:
    static constexpr uint8_t IEI = 0x09;
    L3QoS() = default;
    L3QoS(QoSType type, std::vector<uint8_t> elements)
        : mType(type), mElements(std::move(elements)) {}

    bool operator==(const L3QoS&) const = default;

    QoSType type() const { return mType; }
    const std::vector<uint8_t>& elements() const { return mElements; }
    size_t lengthV() const { return 1 + mElements.size(); }

    [[nodiscard]] static Expected<L3QoS> parse(BitReader& br, size_t lengthBytes);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── Access Point Name (GSM 24.008 10.5.8.3) ───────────────────────────
// TLV format: IEI=0x2F | Length(1) | APNString(variable, UTF-8)

class L3AccessPointName {
    std::string mValue;
public:
    static constexpr uint8_t IEI = 0x2F;
    L3AccessPointName() = default;
    explicit L3AccessPointName(std::string value) : mValue(std::move(value)) {}

    bool operator==(const L3AccessPointName&) const = default;

    const std::string& value() const { return mValue; }
    size_t lengthV() const { return mValue.size(); }

    [[nodiscard]] static Expected<L3AccessPointName> parse(BitReader& br, size_t lengthBytes);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── Protocol Configuration Options (GSM 24.008 10.5.8.4) ──────────────
// TLV format: IEI=0x3C | Length(1) | Type(1) | ConfigData(variable)
// Type identifies the protocol family (e.g., IPCP=0xC029 for IPv4).

class L3ProtocolConfigOptions {
    uint8_t mType{0};
    std::vector<uint8_t> mData;
public:
    static constexpr uint8_t IEI = 0x3C;
    L3ProtocolConfigOptions() = default;
    L3ProtocolConfigOptions(uint8_t type, std::vector<uint8_t> data)
        : mType(type), mData(std::move(data)) {}

    bool operator==(const L3ProtocolConfigOptions&) const = default;

    uint8_t type() const { return mType; }
    const std::vector<uint8_t>& data() const { return mData; }
    size_t lengthV() const { return 1 + mData.size(); }

    [[nodiscard]] static Expected<L3ProtocolConfigOptions> parse(BitReader& br, size_t lengthBytes);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── SM Cause IE (GSM 24.008 10.5.3.2.3) ───────────────────────────────
// TV format: IEI=0x27 | CauseValue(1 octet)

class L3SMCauseIE {
    SMCause mCause{SMCause::ReqAccepted};
public:
    static constexpr uint8_t IEI = 0x27;
    L3SMCauseIE() = default;
    explicit L3SMCauseIE(SMCause cause) : mCause(cause) {}

    bool operator==(const L3SMCauseIE&) const = default;

    SMCause cause() const { return mCause; }
    static constexpr size_t lengthV() { return 1; }

    [[nodiscard]] static Expected<L3SMCauseIE> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── Back-Off Timer (GSM 24.008 10.5.8.6) ──────────────────────────────
// TV format: IEI=0x28 | TimerValue(1 octet)
// Encodes the timer value using GPRS Timer 2 encoding.

class L3BackOffTimer {
    uint8_t mValue{0};
public:
    static constexpr uint8_t IEI = 0x28;
    L3BackOffTimer() = default;
    explicit L3BackOffTimer(uint8_t value) : mValue(value) {}

    bool operator==(const L3BackOffTimer&) const = default;

    uint8_t value() const { return mValue; }
    static constexpr size_t lengthV() { return 1; }

    [[nodiscard]] static Expected<L3BackOffTimer> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── PDP Handle (GSM 24.008 10.5.8.7) ──────────────────────────────────
// 4-bit field identifying an activated PDP context (0-15).

class L3PDPHandle {
    uint8_t mValue{0};
public:
    L3PDPHandle() = default;
    explicit L3PDPHandle(uint8_t value) : mValue(value & 0x0F) {}

    bool operator==(const L3PDPHandle&) const = default;

    uint8_t value() const { return mValue; }
    static constexpr size_t lengthV() { return 0; } // bit-level field, no separate length

    [[nodiscard]] static Expected<L3PDPHandle> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── TMGI - Temporary Mobile Group Identity (GSM 24.008 10.5.6.14) ──────
// TLV format: IEI=0x42 | Length(1)=6 | PLMN Identity(3) | Service ID(2) | Session ID(1)

class L3TMGI {
    std::array<uint8_t, 3> mPLMN{0, 0, 0};
    uint16_t mServiceId{0};
    uint8_t mSessionId{0};
public:
    static constexpr uint8_t IEI = 0x42;
    L3TMGI() = default;
    L3TMGI(std::array<uint8_t, 3> plmn, uint16_t serviceId, uint8_t sessionId)
        : mPLMN(std::move(plmn)), mServiceId(serviceId), mSessionId(sessionId) {}

    bool operator==(const L3TMGI&) const = default;

    const std::array<uint8_t, 3>& plmn() const { return mPLMN; }
    uint16_t serviceId() const { return mServiceId; }
    uint8_t sessionId() const { return mSessionId; }
    static constexpr size_t lengthV() { return 6; }

    [[nodiscard]] static Expected<L3TMGI> parse(BitReader& br, size_t lengthBytes);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

} // namespace gsml3parser
