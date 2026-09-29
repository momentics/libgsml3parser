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
// Spec: 3GPP TS 44.068 (GSM 24.008) sections 9.5 and 10.5.6.
//
// IE encodings on the wire:
//   PDPAddress: positional LV (no identifier): Length(1) |
//               [spare(4)|pdpTypeOrigin(4)] | pdpTypeNumber(1) |
//               AddressInfo(variable; empty when dynamically assigned)
//   QoS: positional LV (no identifier): Length(1) | QoS value octets
//   AccessPointName: TLV, IEI=0x28: IEI(1) | Length(1) | APN string
//   ProtocolConfigOptions: TLV, IEI=0x27: IEI(1) | Length(1) | option data
//   SMCause: bare value octet without an identifier; the first body octet of
//            SM reject, failure and status messages
//   TearDownIndicator: TV in a single octet: identifier '1001'B (high nibble)
//            + [tear-down flag(1)|spare(3)] (low nibble)

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

// ── PDP Type (TS 44.068 section 10.5.6.4) ─────────────────────────────
// PDP type numbers for the IETF origin, carried in the second octet of the
// PDP address value. The origin itself ('0001'B = IETF) is stored separately.

enum class PDPType : uint8_t {
    IPv4      = 0x21,
    IPv6      = 0x53,
    Unknown   = 0xFF
};

// ── QoS Type (TS 44.068 section 10.5.6.5) ─────────────────────────────
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

// ── PDP Address (TS 44.068 section 10.5.6.4) ──────────────────────────
// Positional LV value (no information element identifier):
//   octet 1: [spare(4) | pdpTypeOrigin(4)]   ('0001'B = IETF origin -> 0x01)
//   octet 2: pdpTypeNumber                   (IPv4 = 0x21, IPv6 = 0x53)
//   octets 3..: address information          (empty when dynamically assigned)

class L3PDPAddress {
    uint8_t mOrigin{0x01};          ///< PDP type origin, four bits (IETF = '0001'B)
    uint8_t mTypeNumber{0x21};      ///< PDP type number octet
    std::vector<uint8_t> mAddress;  ///< Address information; empty = dynamic allocation

public:
    static constexpr uint8_t kOriginIETF = 0x01u; ///< PDP type origin '0001'B (IETF)

    L3PDPAddress() = default;
    explicit L3PDPAddress(PDPType type, std::vector<uint8_t> addr)
        : mOrigin(kOriginIETF), mTypeNumber(static_cast<uint8_t>(type)), mAddress(std::move(addr)) {}
    /// Raw form for type numbers not covered by the PDPType enum.
    L3PDPAddress(uint8_t origin, uint8_t typeNumber, std::vector<uint8_t> addr)
        : mOrigin(origin & 0x0Fu), mTypeNumber(typeNumber), mAddress(std::move(addr)) {}

    bool operator==(const L3PDPAddress&) const = default;

    /// Semantic PDP type (IETF origin): IPv4, IPv6 or Unknown.
    PDPType type() const {
        if (mOrigin == kOriginIETF && mTypeNumber == static_cast<uint8_t>(PDPType::IPv4)) return PDPType::IPv4;
        if (mOrigin == kOriginIETF && mTypeNumber == static_cast<uint8_t>(PDPType::IPv6)) return PDPType::IPv6;
        return PDPType::Unknown;
    }
    uint8_t origin() const { return mOrigin; }
    uint8_t typeNumber() const { return mTypeNumber; }
    const std::vector<uint8_t>& address() const { return mAddress; }
    size_t lengthV() const { return 2 + mAddress.size(); }

    [[nodiscard]] static Expected<L3PDPAddress> parse(BitReader& br, size_t lengthBytes);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── QoS Element (TS 44.068 section 10.5.6.5) ──────────────────────────
// The QoS profile value octets following the LV length octet.

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

// ── QoS Profile (TS 44.068 section 10.5.6.5) ──────────────────────────
// Positional LV value (no information element identifier): a QoS type octet
// followed by the remaining QoS profile octets, kept opaque.

class L3QoS {
    QoSType mType{QoSType::Requested};
    std::vector<uint8_t> mElements;
public:
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

// ── Access Point Name (TS 44.068 section 10.5.6.1) ────────────────────
// TLV format: IEI=0x28 | Length(1) | APNString(variable, UTF-8)

class L3AccessPointName {
    std::string mValue;
public:
    static constexpr uint8_t IEI = 0x28;
    L3AccessPointName() = default;
    explicit L3AccessPointName(std::string value) : mValue(std::move(value)) {}

    bool operator==(const L3AccessPointName&) const = default;

    const std::string& value() const { return mValue; }
    size_t lengthV() const { return mValue.size(); }

    [[nodiscard]] static Expected<L3AccessPointName> parse(BitReader& br, size_t lengthBytes);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── Protocol Configuration Options (TS 44.068 section 10.5.6.3) ───────
// TLV format: IEI=0x27 | Length(1) | PCO option data (opaque list of PPP
// IPCP-style options).

class L3ProtocolConfigOptions {
    std::vector<uint8_t> mValue;
public:
    static constexpr uint8_t IEI = 0x27;
    L3ProtocolConfigOptions() = default;
    explicit L3ProtocolConfigOptions(std::vector<uint8_t> value)
        : mValue(std::move(value)) {}

    bool operator==(const L3ProtocolConfigOptions&) const = default;

    const std::vector<uint8_t>& value() const { return mValue; }
    size_t lengthV() const { return mValue.size(); }

    [[nodiscard]] static Expected<L3ProtocolConfigOptions> parse(BitReader& br, size_t lengthBytes);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── SM Cause (TS 44.068 section 9.5) ──────────────────────────────────
// The SM cause is a single value octet carried without an identifier at the
// start of the body of SM reject, failure and status messages.

class L3SMCauseIE {
    SMCause mCause{SMCause::ReqAccepted};
public:
    L3SMCauseIE() = default;
    explicit L3SMCauseIE(SMCause cause) : mCause(cause) {}

    bool operator==(const L3SMCauseIE&) const = default;

    SMCause cause() const { return mCause; }
    static constexpr size_t lengthV() { return 1; }

    [[nodiscard]] static Expected<L3SMCauseIE> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── Back-Off Timer (TS 44.068 section 10.5.6.x) ───────────────────────
// Optional TV information element of SM reject messages; it is not modelled
// as a typed body field and is preserved in the opaque additional-IE
// sequence of the message that carries it.

class L3BackOffTimer {
    uint8_t mValue{0};
public:
    L3BackOffTimer() = default;
    explicit L3BackOffTimer(uint8_t value) : mValue(value) {}

    bool operator==(const L3BackOffTimer&) const = default;

    uint8_t value() const { return mValue; }
    static constexpr size_t lengthV() { return 1; }

    [[nodiscard]] static Expected<L3BackOffTimer> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── Request type (TS 44.068 section 10.5.6.17) ────────────────────────
// TV information element carried in a single octet: the high nibble '1010'B
// identifies the IE and the low nibble holds the request type value.
constexpr uint8_t kSMRequestTypeIdentifier = 0xAu; ///< identifier nibble '1010'B
constexpr uint8_t kSMRequestTypeMask = 0xF0u;      ///< mask selecting the identifier nibble

// ── Tear-Down Indicator (TS 44.068 section 10.5.6.10) ─────────────────
// TV information element carried in a single octet: the high nibble is the
// four-bit identifier '1001'B and the low nibble holds the tear-down flag
// (most significant bit of the nibble) followed by three spare bits.

class L3TearDownIndicator {
    bool mFlag{false};
public:
    static constexpr uint8_t IEI = 0x09u; ///< identifier nibble '1001'B (high half of the octet)

    L3TearDownIndicator() = default;
    explicit L3TearDownIndicator(bool flag) : mFlag(flag) {}

    bool operator==(const L3TearDownIndicator&) const = default;

    bool flag() const { return mFlag; }
    static constexpr size_t lengthV() { return 1; } ///< one octet: identifier + value nibble

    /// Reads the single octet and verifies the '1001'B identifier nibble.
    [[nodiscard]] static Expected<L3TearDownIndicator> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── PDP Handle (TS 44.068 section 9.5) ────────────────────────────────
// Four-bit value identifying an activated PDP context.

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

// ── TMGI - Temporary Mobile Group Identity (GSM 24.008 10.5.6.x) ───────
// TLV value: Length(1)=6 | PLMN Identity(3) | Service ID(2) | Session ID(1).
// In SM MBMS messages the TMGI IE is preserved in the opaque additional-IE
// sequence of the message; this class decodes its six value octets.

class L3TMGI {
    std::array<uint8_t, 3> mPLMN{0, 0, 0};
    uint16_t mServiceId{0};
    uint8_t mSessionId{0};
public:
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
