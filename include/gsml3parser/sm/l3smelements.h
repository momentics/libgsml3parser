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
//               spare(4)=0|typeOrg(4)=0x01 | pdpTypeNumber(1; IETF IPv4=0x21,
//               IETF IPv6=0x53) | addressInfo (absent when dynamically
//               assigned: 4 octets for IPv4, 16 octets for IPv6)
//   QoS: positional LV (no identifier): Length(1) | QoS value octets; the
//        first three value octets carry the mandatory characteristics, any
//        further octets are optional characteristics
//   AccessPointName: TLV, IEI=0x28: IEI(1) | Length(1) | APN string
//   ProtocolConfigOptions: TLV, IEI=0x27: IEI(1) | Length(1) | option data
//   SMCause: bare value octet without an identifier; the first body octet of
//            SM reject, failure and status messages
//   TearDownIndicator: TV in a single octet: identifier '1001'B (high nibble)
//            + tear-down flag in bit 0 (low nibble)

#pragma once

#include <array>
#include <cstdint>
#include <ostream>
#include <span>
#include <string>
#include <vector>

#include "../expected.h"
#include "../bitreader.h"
#include "../bitwriter.h"
#include "../types.h"

namespace gsml3parser {

// ── PDP Type Number (TS 24.008 section 10.5.6.4) ──────────────────────
// PDP type numbers carried in the second octet of the PDP address value;
// only IETF-defined types are recognized on the wire.

enum class PdpTypeNumber : uint8_t {
    IPv4 = 0x21,
    IPv6 = 0x53
};

// ── SM Cause (TS 44.068 / TS 24.008 section 10.5.5.6) ─────────────────
// Cause values specific to Session Management procedures, carried as a
// single value octet at the start of the body of reject and failure
// messages.

enum class SMCause : uint8_t {
    Unspecified = 0x00,                     // builder sentinel only; must be replaced with a defined cause before wire emission
    Operator_Determined_Barring = 0x08,
    MBMS_Bearer_Caps_Insufficient = 0x18,   LLC_Or_SNDPC_Failure = 0x19,
    Insufficient_Resources = 0x1A,          Missing_Or_Unknown_APN = 0x1B,
    Unknown_PDP_Address_Or_Type = 0x1C,     User_Authentication_Failed = 0x1D,
    Activation_Rejected_By_GGSN = 0x1E,     Activation_Rejected_Unspecified = 0x1F,
    Service_Option_Not_Supported = 0x20,    Requested_Service_Option_Not_Subscribed = 0x21,
    Service_Option_Temporarily_Out_Of_Order = 0x22,
    NSAPI_Already_Used = 0x23,              Regular_Deactivation = 0x24,
    QoS_Not_Accepted = 0x25,                Network_Failure = 0x26,
    Reactivation_Required = 0x27,           Feature_Not_Supported = 0x28,
    Semantic_Error_In_TFT = 0x29,           Syntactical_Error_In_TFT = 0x2A,
    Unknown_PDP_Context = 0x2B,             Semantic_Errors_In_Packet_Filter = 0x2C,
    Syntactical_Errors_In_Packet_Filter = 0x2D,
    PDP_Context_Without_TFT_Already_Activated = 0x2E,
    Multicast_Group_Membership_Timeout = 0x2F,
    Activation_Rejected_BCM_Violation = 0x30,
    PDP_Type_IPv4_Only_Allowed = 0x32,      PDP_Type_IPv6_Only_Allowed = 0x33,
    Single_Address_Bearers_Only_Allowed = 0x34,
    Collision_With_Network_Initiated_Request = 0x38,
    Invalid_Transaction_Identifier_Value = 0x51,
    Semantically_Incorrect_Message = 0x5F,  Invalid_Mandatory_Information = 0x60,
    Message_Type_Not_Existent = 0x61,       Message_Type_Not_Compatible_With_State = 0x62,
    IE_Not_Existent = 0x63,                 Conditional_IE_Error = 0x64,
    Message_Not_Compatible = 0x65,          Protocol_Error_Unspecified = 0x6F,
    APN_Restriction_Value_Incompatible = 0xF0
};

const char* SMCause2Str(SMCause cause);

// ── PDP Address (TS 24.008 section 10.5.6.4) ──────────────────────────
// Positional LV value (no information element identifier):
//   octet 1: spare(4)=0 high | typeOrg(4) low   (typeOrg 0x01 = IETF; stored raw)
//   octet 2: pdpTypeNumber            (IETF IPv4 = 0x21, IETF IPv6 = 0x53)
//   octets 3..: address information   (absent when dynamically assigned)

class L3PDPAddress {
    uint8_t mTypeOrg{0x01};                     ///< PDP type organization nibble, low half of the first value octet (TS 24.008 section 10.5.6.4)
    PdpTypeNumber mTypeNumber{PdpTypeNumber::IPv4};
    bool mStaticAddress{false};
    std::array<uint8_t, 16> mAddress{};         ///< 4 octets (IPv4) or 16 octets (IPv6), most significant first

public:
    L3PDPAddress() = default;

    /// Dynamic IETF IPv4 address assignment (no address octets on the wire).
    [[nodiscard]] static constexpr L3PDPAddress ipv4Dynamic() noexcept {
        return L3PDPAddress{};
    }
    /// Static IETF IPv4 address; \p addrBe holds the four octets with the
    /// most significant one first (192.168.1.100 is 0xC0A80164).
    [[nodiscard]] static constexpr L3PDPAddress ipv4Static(uint32_t addrBe) noexcept {
        L3PDPAddress a;
        a.mTypeNumber = PdpTypeNumber::IPv4;
        a.mStaticAddress = true;
        a.mAddress[0] = static_cast<uint8_t>((addrBe >> 24) & 0xFFu);
        a.mAddress[1] = static_cast<uint8_t>((addrBe >> 16) & 0xFFu);
        a.mAddress[2] = static_cast<uint8_t>((addrBe >> 8) & 0xFFu);
        a.mAddress[3] = static_cast<uint8_t>(addrBe & 0xFFu);
        return a;
    }
    /// Dynamic IETF IPv6 address assignment (no address octets on the wire).
    [[nodiscard]] static constexpr L3PDPAddress ipv6Dynamic() noexcept {
        L3PDPAddress a;
        a.mTypeNumber = PdpTypeNumber::IPv6;
        return a;
    }
    /// Static IETF IPv6 address (sixteen octets, most significant first).
    [[nodiscard]] static constexpr L3PDPAddress ipv6Static(std::span<const uint8_t, 16> addr) noexcept {
        L3PDPAddress a;
        a.mTypeNumber = PdpTypeNumber::IPv6;
        a.mStaticAddress = true;
        for (size_t i = 0; i < 16; ++i) a.mAddress[i] = addr[i];
        return a;
    }

    bool operator==(const L3PDPAddress&) const = default;

    /// PDP type organization nibble, stored raw (only 0x01 = IETF is defined).
    uint8_t typeOrg() const { return mTypeOrg; }
    PdpTypeNumber typeNumber() const { return mTypeNumber; }
    /// True when a static address is carried; the octet count is implied by
    /// the type number (4 for IPv4, 16 for IPv6).
    bool staticAddress() const { return mStaticAddress; }
    /// Number of address octets carried on the wire (0 = dynamically assigned).
    [[nodiscard]] size_t addressSize() const noexcept {
        if (!mStaticAddress) return 0;
        return mTypeNumber == PdpTypeNumber::IPv4 ? 4u : 16u;
    }
    /// The address octets actually carried (empty when dynamically assigned).
    [[nodiscard]] std::span<const uint8_t> address() const noexcept {
        return std::span<const uint8_t>{mAddress.data(), addressSize()};
    }
    /// Value part size in octets: 2 when dynamically assigned, 6 for a static
    /// IPv4 address, 18 for a static IPv6 address.
    [[nodiscard]] size_t lengthV() const noexcept { return 2 + addressSize(); }

    [[nodiscard]] static Expected<L3PDPAddress> parse(BitReader& br, size_t lengthBytes);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── QoS Profile (TS 24.008 section 10.5.6.5) ──────────────────────────
// Positional LV value (no information element identifier): the first three
// octets carry the mandatory characteristics; any further octets hold
// optional characteristics and are preserved verbatim.

class L3QoS {
    uint8_t mDelayClass{0};        ///< bits 3-5 of octet 1 (7 = not defined)
    uint8_t mReliabilityClass{0};  ///< bits 0-2 of octet 1 (7 = not defined)
    uint8_t mPrecedenceClass{0};   ///< bits 0-2 of octet 2 (2 = normal)
    uint8_t mPeakThroughput{0};    ///< bits 4-7 of octet 2
    uint8_t mMeanThroughput{0};    ///< bits 0-4 of octet 3
    std::vector<uint8_t> mAdditionalCharacteristics; // opaque, empty by default

public:
    L3QoS() = default;

    /// Default profile: reliability class '011'B (acknowledged RLC), delay
    /// class '100'B (best effort), precedence class '010'B (normal), peak
    /// and mean throughput codes 0; value octets 23 02 00.
    [[nodiscard]] static L3QoS defaultProfile() noexcept;

    bool operator==(const L3QoS&) const = default;

    /// Delay class code (three bits).
    uint8_t delayClass() const { return mDelayClass; }
    /// Reliability class code (three bits).
    uint8_t reliabilityClass() const { return mReliabilityClass; }
    /// Precedence class code (three bits).
    uint8_t precedenceClass() const { return mPrecedenceClass; }
    /// Peak throughput code (four bits).
    uint8_t peakThroughput() const { return mPeakThroughput; }
    /// Mean throughput code (five bits).
    uint8_t meanThroughput() const { return mMeanThroughput; }
    /// Optional characteristics from the fourth value octet on, re-emitted
    /// verbatim.
    const std::vector<uint8_t>& additionalCharacteristics() const { return mAdditionalCharacteristics; }

    /// Set the delay class code (three bits).
    L3QoS& delayClass(uint8_t v) { mDelayClass = v & 0x07u; return *this; }
    /// Set the reliability class code (three bits).
    L3QoS& reliabilityClass(uint8_t v) { mReliabilityClass = v & 0x07u; return *this; }
    /// Set the precedence class code (three bits).
    L3QoS& precedenceClass(uint8_t v) { mPrecedenceClass = v & 0x07u; return *this; }
    /// Set the peak throughput code (four bits).
    L3QoS& peakThroughput(uint8_t v) { mPeakThroughput = v & 0x0Fu; return *this; }
    /// Set the mean throughput code (five bits).
    L3QoS& meanThroughput(uint8_t v) { mMeanThroughput = v & 0x1Fu; return *this; }
    /// Set the optional characteristics (from the fourth value octet on).
    L3QoS& additionalCharacteristics(std::span<const uint8_t> v) {
        mAdditionalCharacteristics.assign(v.begin(), v.end());
        return *this;
    }

    /// Value part size in octets: three mandatory characteristics plus the
    /// preserved optional characteristics.
    [[nodiscard]] size_t lengthV() const noexcept { return 3 + mAdditionalCharacteristics.size(); }

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
    SMCause mCause{SMCause::Unspecified};
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

// ── Tear-Down Indicator (TS 24.008 section 10.5.6.10) ─────────────────
// TV information element carried in a single octet: the high nibble is the
// four-bit identifier '1001'B, bit 0 of the low nibble holds the tear-down
// flag and the remaining bits are spare.

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
// Four-bit value identifying an activated PDP context, carried in one
// octet: [spare(4)=0][value(4)].

class L3PDPHandle {
    uint8_t mValue{0};
public:
    L3PDPHandle() = default;
    explicit L3PDPHandle(uint8_t value) : mValue(value & 0x0F) {}

    bool operator==(const L3PDPHandle&) const = default;

    uint8_t value() const { return mValue; }
    static constexpr size_t lengthV() { return 1; } // one octet on the wire

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
