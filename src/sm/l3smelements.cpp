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

// SM IE - parse/write/text implementation
// Spec: 3GPP TS 44.068 (GSM 24.008) sections 9.5 and 10.5.6.

#include "gsml3parser/sm/l3smelements.h"
#include <sstream>
#include <iomanip>

namespace gsml3parser {

const char* SMCause2Str(SMCause cause) {
    switch (cause) {
        case SMCause::Unspecified: return "Unspecified";
        case SMCause::Operator_Determined_Barring: return "Operator determined barring";
        case SMCause::MBMS_Bearer_Caps_Insufficient: return "MBMS bearer capabilities insufficient for the service";
        case SMCause::LLC_Or_SNDPC_Failure: return "LLC or SNDCP failure";
        case SMCause::Insufficient_Resources: return "Insufficient resources";
        case SMCause::Missing_Or_Unknown_APN: return "Missing or unknown APN";
        case SMCause::Unknown_PDP_Address_Or_Type: return "Unknown PDP address or PDP type";
        case SMCause::User_Authentication_Failed: return "User authentication failed";
        case SMCause::Activation_Rejected_By_GGSN: return "Activation rejected by GGSN";
        case SMCause::Activation_Rejected_Unspecified: return "Activation rejected, unspecified reason";
        case SMCause::Service_Option_Not_Supported: return "Service option not supported";
        case SMCause::Requested_Service_Option_Not_Subscribed: return "Requested service option not subscribed";
        case SMCause::Service_Option_Temporarily_Out_Of_Order: return "Service option temporarily out of order";
        case SMCause::NSAPI_Already_Used: return "NSAPI already used";
        case SMCause::Regular_Deactivation: return "Regular deactivation";
        case SMCause::QoS_Not_Accepted: return "Requested QoS not accepted";
        case SMCause::Network_Failure: return "Network failure";
        case SMCause::Reactivation_Required: return "Reactivation required";
        case SMCause::Feature_Not_Supported: return "Requested feature not supported";
        case SMCause::Semantic_Error_In_TFT: return "Semantic error in TFT operation";
        case SMCause::Syntactical_Error_In_TFT: return "Syntactical error in TFT operation";
        case SMCause::Unknown_PDP_Context: return "Unknown PDP context";
        case SMCause::Semantic_Errors_In_Packet_Filter: return "Semantic errors in packet filter";
        case SMCause::Syntactical_Errors_In_Packet_Filter: return "Syntactical errors in packet filter";
        case SMCause::PDP_Context_Without_TFT_Already_Activated: return "PDP context without TFT already activated";
        case SMCause::Multicast_Group_Membership_Timeout: return "Multicast group membership timeout";
        case SMCause::Activation_Rejected_BCM_Violation: return "Activation rejected due to BCM violation";
        case SMCause::PDP_Type_IPv4_Only_Allowed: return "Only IPv4 allowed as PDP type";
        case SMCause::PDP_Type_IPv6_Only_Allowed: return "Only IPv6 allowed as PDP type";
        case SMCause::Single_Address_Bearers_Only_Allowed: return "Single address bearers only allowed";
        case SMCause::Collision_With_Network_Initiated_Request: return "Collision with network initiated request";
        case SMCause::Invalid_Transaction_Identifier_Value: return "Invalid transaction identifier value";
        case SMCause::Semantically_Incorrect_Message: return "Semantically incorrect message";
        case SMCause::Invalid_Mandatory_Information: return "Invalid mandatory information";
        case SMCause::Message_Type_Not_Existent: return "Message type nonexistent or not implemented";
        case SMCause::Message_Type_Not_Compatible_With_State: return "Message type not compatible with state";
        case SMCause::IE_Not_Existent: return "Information element nonexistent or not implemented";
        case SMCause::Conditional_IE_Error: return "Conditional IE error";
        case SMCause::Message_Not_Compatible: return "Message not compatible with state";
        case SMCause::Protocol_Error_Unspecified: return "Protocol error unspecified";
        case SMCause::APN_Restriction_Value_Incompatible: return "APN restriction value incompatible with active PDP context";
    }
    return "Unknown";
}

// ── L3PDPAddress (TS 24.008 section 10.5.6.4) ─────────────────────────

Expected<L3PDPAddress> L3PDPAddress::parse(BitReader& br, size_t lengthBytes) {
    // The value always carries the type organization octet and the PDP type
    // number octet; address information follows when statically assigned.
    if (lengthBytes < 2) {
        return Expected<L3PDPAddress>::error(
            ParseError{ParseError::Code::TruncatedInput, "PDP address too short", br.position()});
    }

    auto o1 = br.readField(8);
    if (!o1) return Expected<L3PDPAddress>::error(o1.error());
    const uint8_t firstOctet = static_cast<uint8_t>(o1.value());
    // The type organization nibble is the low nibble; only the IETF
    // organization (0x01) is defined, but the raw value is preserved. The
    // high nibble is spare and must be zero.
    if ((firstOctet & 0xF0u) != 0) {
        return Expected<L3PDPAddress>::error(
            ParseError{ParseError::Code::InvalidValue, "PDP type organization", br.position()});
    }

    auto o2 = br.readField(8);
    if (!o2) return Expected<L3PDPAddress>::error(o2.error());
    const uint8_t typeNumber = static_cast<uint8_t>(o2.value());
    L3PDPAddress addr;
    addr.mTypeOrg = static_cast<uint8_t>(firstOctet & 0x0Fu);
    if (typeNumber == 0x21) {
        addr.mTypeNumber = PdpTypeNumber::IPv4;
    } else if (typeNumber == 0x53) {
        addr.mTypeNumber = PdpTypeNumber::IPv6;
    } else {
        return Expected<L3PDPAddress>::error(
            ParseError{ParseError::Code::InvalidValue, "PDP type number", br.position()});
    }

    const size_t addrSize = (addr.mTypeNumber == PdpTypeNumber::IPv4) ? 4u : 16u;
    if (lengthBytes == 2) {
        // Dynamically assigned: no address octets.
    } else if (lengthBytes == 2 + addrSize) {
        addr.mStaticAddress = true;
        auto r = br.readBytes(addr.mAddress.data(), addrSize);
        if (!r) return Expected<L3PDPAddress>::error(r.error());
    } else {
        return Expected<L3PDPAddress>::error(
            ParseError{ParseError::Code::InvalidValue, "PDP address length", br.position()});
    }

    return Expected<L3PDPAddress>::hold(std::move(addr));
}

void L3PDPAddress::write(BitWriter& bw) const {
    // Octet 1: spare high nibble = 0 | type organization low nibble;
    // octet 2: PDP type number.
    bw.writeField(static_cast<uint32_t>(mTypeOrg & 0x0Fu), 8);
    bw.writeField(static_cast<uint32_t>(mTypeNumber), 8);
    if (mStaticAddress) {
        bw.writeBytes(mAddress.data(), addressSize());
    }
}

void L3PDPAddress::text(std::ostream& os) const {
    auto addr = address();
    os << "PDPAddr(org=" << std::hex << static_cast<int>(mTypeOrg) << std::dec;
    os << ",type=";
    if (mTypeNumber == PdpTypeNumber::IPv4) os << "IPv4";
    else os << "IPv6";
    if (!addr.empty()) {
        if (addr.size() == 4) {
            os << ",addr=" << static_cast<int>(addr[0]) << "."
                       << static_cast<int>(addr[1]) << "."
                       << static_cast<int>(addr[2]) << "."
                       << static_cast<int>(addr[3]);
        } else {
            os << ",addr=";
            for (size_t i = 0; i < addr.size(); ++i) {
                if (i > 0) os << ":";
                os << std::hex << static_cast<int>(addr[i]);
            }
            os << std::dec;
        }
    } else {
        os << ",dynamic";
    }
    os << ")";
}

// ── L3QoS (TS 24.008 section 10.5.6.5) ────────────────────────────────

L3QoS L3QoS::defaultProfile() noexcept {
    // Default profile (TS 24.008 Table 10.5.156): reliability class '011'B,
    // delay class '100'B, precedence class '010'B, peak and mean throughput
    // codes 0; value octets 23 02 00.
    L3QoS qos;
    qos.mReliabilityClass = 3;
    qos.mDelayClass = 4;
    qos.mPrecedenceClass = 2;
    return qos;
}

Expected<L3QoS> L3QoS::parse(BitReader& br, size_t lengthBytes) {
    // The first three value octets carry the mandatory characteristics; any
    // further octets hold optional characteristics and are preserved verbatim.
    if (lengthBytes < 3) {
        return Expected<L3QoS>::error(
            ParseError{ParseError::Code::TruncatedInput, "QoS too short", br.position()});
    }

    auto o1 = br.readField(8);
    if (!o1) return Expected<L3QoS>::error(o1.error());
    const uint8_t octet1 = static_cast<uint8_t>(o1.value());
    if ((octet1 & 0xC0u) != 0) {
        return Expected<L3QoS>::error(
            ParseError{ParseError::Code::InvalidValue, "QoS reserved bits", br.position()});
    }

    auto o2 = br.readField(8);
    if (!o2) return Expected<L3QoS>::error(o2.error());
    const uint8_t octet2 = static_cast<uint8_t>(o2.value());
    if ((octet2 & 0x08u) != 0) {
        return Expected<L3QoS>::error(
            ParseError{ParseError::Code::InvalidValue, "QoS reserved bits", br.position()});
    }

    auto o3 = br.readField(8);
    if (!o3) return Expected<L3QoS>::error(o3.error());
    const uint8_t octet3 = static_cast<uint8_t>(o3.value());
    if ((octet3 & 0xE0u) != 0) {
        return Expected<L3QoS>::error(
            ParseError{ParseError::Code::InvalidValue, "QoS reserved bits", br.position()});
    }

    L3QoS qos;
    qos.mDelayClass = static_cast<uint8_t>((octet1 >> 3) & 0x07u);
    qos.mReliabilityClass = static_cast<uint8_t>(octet1 & 0x07u);
    qos.mPeakThroughput = static_cast<uint8_t>((octet2 >> 4) & 0x0Fu);
    qos.mPrecedenceClass = static_cast<uint8_t>(octet2 & 0x07u);
    qos.mMeanThroughput = static_cast<uint8_t>(octet3 & 0x1Fu);

    const size_t tailLen = lengthBytes - 3;
    if (tailLen > 0) {
        qos.mAdditionalCharacteristics.resize(tailLen);
        auto r = br.readBytes(qos.mAdditionalCharacteristics.data(), tailLen);
        if (!r) return Expected<L3QoS>::error(r.error());
    }

    return Expected<L3QoS>::hold(std::move(qos));
}

void L3QoS::write(BitWriter& bw) const {
    // Octet 1: [spare(2)=0][delay class(3)][reliability class(3)].
    bw.writeField((static_cast<uint32_t>(mDelayClass & 0x07u) << 3) |
                  (static_cast<uint32_t>(mReliabilityClass & 0x07u)), 8);
    // Octet 2: [peak throughput(4)][spare(1)=0][precedence class(3)].
    bw.writeField((static_cast<uint32_t>(mPeakThroughput & 0x0Fu) << 4) |
                  (static_cast<uint32_t>(mPrecedenceClass & 0x07u)), 8);
    // Octet 3: [spare(3)=0][mean throughput(5)].
    bw.writeField(static_cast<uint32_t>(mMeanThroughput & 0x1Fu), 8);
    if (!mAdditionalCharacteristics.empty()) {
        bw.writeBytes(mAdditionalCharacteristics.data(), mAdditionalCharacteristics.size());
    }
}

void L3QoS::text(std::ostream& os) const {
    os << "QoS(delay=" << static_cast<int>(mDelayClass)
       << ",reliability=" << static_cast<int>(mReliabilityClass)
       << ",precedence=" << static_cast<int>(mPrecedenceClass)
       << ",peak=" << static_cast<int>(mPeakThroughput)
       << ",mean=" << static_cast<int>(mMeanThroughput);
    if (!mAdditionalCharacteristics.empty()) {
        os << ",additional=";
        for (size_t i = 0; i < mAdditionalCharacteristics.size(); ++i) {
            if (i > 0) os << ":";
            os << std::hex << static_cast<int>(mAdditionalCharacteristics[i]);
        }
        os << std::dec;
    }
    os << ")";
}

// ── L3AccessPointName (TS 44.068 section 10.5.6.1) ───────────────────

Expected<L3AccessPointName> L3AccessPointName::parse(BitReader& br, size_t lengthBytes) {
    L3AccessPointName apn;
    apn.mValue.resize(lengthBytes);
    if (lengthBytes > 0) {
        auto r = br.readBytes(reinterpret_cast<uint8_t*>(const_cast<char*>(apn.mValue.data())), lengthBytes);
        if (!r) return Expected<L3AccessPointName>::error(r.error());
    }
    return Expected<L3AccessPointName>::hold(std::move(apn));
}

void L3AccessPointName::write(BitWriter& bw) const {
    if (!mValue.empty()) {
        bw.writeBytes(reinterpret_cast<const uint8_t*>(mValue.data()), mValue.size());
    }
}

void L3AccessPointName::text(std::ostream& os) const {
    os << "APN(" << mValue << ")";
}

// ── L3ProtocolConfigOptions (TS 44.068 section 10.5.6.3) ──────────────

Expected<L3ProtocolConfigOptions> L3ProtocolConfigOptions::parse(BitReader& br, size_t lengthBytes) {
    L3ProtocolConfigOptions pco;

    if (lengthBytes > 0) {
        pco.mValue.resize(lengthBytes);
        auto r = br.readBytes(pco.mValue.data(), lengthBytes);
        if (!r) return Expected<L3ProtocolConfigOptions>::error(r.error());
    }

    return Expected<L3ProtocolConfigOptions>::hold(std::move(pco));
}

void L3ProtocolConfigOptions::write(BitWriter& bw) const {
    if (!mValue.empty()) {
        bw.writeBytes(mValue.data(), mValue.size());
    }
}

void L3ProtocolConfigOptions::text(std::ostream& os) const {
    os << "PCO(";
    for (size_t i = 0; i < mValue.size(); ++i) {
        if (i > 0) os << ":";
        os << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(mValue[i]);
    }
    os << ")";
}

// ── L3SMCauseIE (TS 44.068 section 9.5) ───────────────────────────────

Expected<L3SMCauseIE> L3SMCauseIE::parse(BitReader& br) {
    auto val = br.readField(8);
    if (!val) return Expected<L3SMCauseIE>::error(val.error());
    return Expected<L3SMCauseIE>::hold(L3SMCauseIE{static_cast<SMCause>(val.value())});
}

void L3SMCauseIE::write(BitWriter& bw) const {
    bw.writeField(static_cast<uint8_t>(mCause), 8);
}

void L3SMCauseIE::text(std::ostream& os) const {
    os << "SMCause(" << SMCause2Str(mCause) << ")";
}

// ── L3BackOffTimer (TS 44.068 section 10.5.6.x) ───────────────────────

Expected<L3BackOffTimer> L3BackOffTimer::parse(BitReader& br) {
    auto val = br.readField(8);
    if (!val) return Expected<L3BackOffTimer>::error(val.error());
    return Expected<L3BackOffTimer>::hold(L3BackOffTimer{static_cast<uint8_t>(val.value())});
}

void L3BackOffTimer::write(BitWriter& bw) const {
    bw.writeField(mValue, 8);
}

void L3BackOffTimer::text(std::ostream& os) const {
    os << "BackOffTimer(0x" << std::hex << static_cast<int>(mValue) << ")";
}

// ── L3TearDownIndicator (TS 44.068 section 10.5.6.10) ────────────────

Expected<L3TearDownIndicator> L3TearDownIndicator::parse(BitReader& br) {
    auto o = br.readField(8);
    if (!o) return Expected<L3TearDownIndicator>::error(o.error());
    uint8_t octet = static_cast<uint8_t>(o.value());
    if ((octet & 0xF0u) != (IEI << 4)) {
        return Expected<L3TearDownIndicator>::error(
            ParseError{ParseError::Code::InvalidIE, "tear-down indicator identifier", br.position()});
    }
    // The flag is bit 0 of the value octet; the remaining low-nibble bits are
    // spare.
    return Expected<L3TearDownIndicator>::hold(L3TearDownIndicator{(octet & 0x01u) != 0});
}

void L3TearDownIndicator::write(BitWriter& bw) const {
    bw.writeField(0x90u | (mFlag ? 1u : 0u), 8);
}

void L3TearDownIndicator::text(std::ostream& os) const {
    os << "TearDownIndicator(" << (mFlag ? "true" : "false") << ")";
}

// ── L3PDPHandle (TS 44.068 section 9.5) ───────────────────────────────

Expected<L3PDPHandle> L3PDPHandle::parse(BitReader& br) {
    auto val = br.readField(8);
    if (!val) return Expected<L3PDPHandle>::error(val.error());
    uint8_t octet = static_cast<uint8_t>(val.value());
    // The handle value is carried in the low nibble; the high nibble is
    // spare and must be zero.
    if ((octet & 0xF0u) != 0) {
        return Expected<L3PDPHandle>::error(
            ParseError{ParseError::Code::InvalidValue, "PDP handle", br.position()});
    }
    return Expected<L3PDPHandle>::hold(L3PDPHandle{octet & 0x0Fu});
}

void L3PDPHandle::write(BitWriter& bw) const {
    bw.writeField(static_cast<uint32_t>(mValue & 0x0Fu), 8);
}

void L3PDPHandle::text(std::ostream& os) const {
    os << "PDPHandle(" << static_cast<int>(mValue) << ")";
}

// ── L3TMGI (GSM 24.008 10.5.6.x) ──────────────────────────────────────

Expected<L3TMGI> L3TMGI::parse(BitReader& br, size_t lengthBytes) {
    L3TMGI tmgi;

    if (lengthBytes < 6) {
        return Expected<L3TMGI>::error(
            ParseError{ParseError::Code::TruncatedInput, "TMGI too short", br.position()});
    }

    // PLMN Identity (3 octets)
    auto r = br.readBytes(tmgi.mPLMN.data(), 3);
    if (!r) return Expected<L3TMGI>::error(r.error());

    // Service ID (2 octets, MSB first)
    auto sHi = br.readField(8);
    if (!sHi) return Expected<L3TMGI>::error(sHi.error());
    auto sLo = br.readField(8);
    if (!sLo) return Expected<L3TMGI>::error(sLo.error());
    tmgi.mServiceId = static_cast<uint16_t>((sHi.value() << 8) | sLo.value());

    // Session ID (1 octet)
    auto sid = br.readField(8);
    if (!sid) return Expected<L3TMGI>::error(sid.error());
    tmgi.mSessionId = static_cast<uint8_t>(sid.value());

    return Expected<L3TMGI>::hold(std::move(tmgi));
}

void L3TMGI::write(BitWriter& bw) const {
    // PLMN Identity (3 octets)
    bw.writeBytes(mPLMN.data(), 3);
    // Service ID (2 octets, MSB first)
    bw.writeField((mServiceId >> 8) & 0xFF, 8);
    bw.writeField(mServiceId & 0xFF, 8);
    // Session ID (1 octet)
    bw.writeField(mSessionId, 8);
}

void L3TMGI::text(std::ostream& os) const {
    os << "TMGI(plmn=" << std::hex << static_cast<int>(mPLMN[0]) << ":"
                           << static_cast<int>(mPLMN[1]) << ":"
                           << static_cast<int>(mPLMN[2])
       << ",serviceId=" << mServiceId
       << ",sessionId=" << static_cast<int>(mSessionId) << ")";
}

} // namespace gsml3parser
