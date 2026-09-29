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
        case SMCause::ReqAccepted: return "Request accepted";
        case SMCause::Unsupported_PDP_Address_Type: return "Unsupported PDP address type";
        case SMCause::Service_Opcode_NotSupported: return "Service opcode not supported";
        case SMCause::Multicast_Context_Ack: return "Multicast context acknowledged";
        case SMCause::Multicast_Context_Reject: return "Multicast context rejected";
        case SMCause::Multicast_Context_Deactivate: return "Multicast context deactivated";
        case SMCause::Invalid_Flow_Desc: return "Invalid flow description";
        case SMCause::Multicast_PDP_No_Bearer: return "Multicast PDP no bearer";
        case SMCause::PDP_Auth_Failed_Primary_PDN: return "PDP auth failed primary PDN";
        case SMCause::PDP_Auth_Failed_Secondary_PDN: return "PDP auth failed secondary PDN";
        case SMCause::Semantically_Incorrect_Message: return "Semantically incorrect message";
        case SMCause::Invalid_Mandatory_Information: return "Invalid mandatory information";
        case SMCause::Message_Type_Invalid: return "Message type invalid";
        case SMCause::Message_Type_Not_Compatible: return "Message type not compatible";
        case SMCause::IE_Invalid: return "IE invalid";
        case SMCause::Conditional_IE_Error: return "Conditional IE error";
        case SMCause::Message_Not_Compatible: return "Message not compatible";
        case SMCause::Protocol_Error_Unspecified: return "Protocol error unspecified";
    }
    return "Unknown";
}

// ── L3PDPAddress (TS 44.068 section 10.5.6.4) ─────────────────────────

Expected<L3PDPAddress> L3PDPAddress::parse(BitReader& br, size_t lengthBytes) {
    L3PDPAddress addr;

    // The value always carries the origin/type octet and the type number.
    if (lengthBytes < 2) {
        return Expected<L3PDPAddress>::error(
            ParseError{ParseError::Code::TruncatedInput, "PDP address too short", br.position()});
    }

    auto o1 = br.readField(8);
    if (!o1) return Expected<L3PDPAddress>::error(o1.error());
    addr.mOrigin = static_cast<uint8_t>(o1.value()) & 0x0Fu; // high nibble is spare
    auto o2 = br.readField(8);
    if (!o2) return Expected<L3PDPAddress>::error(o2.error());
    addr.mTypeNumber = static_cast<uint8_t>(o2.value());

    size_t addrLen = lengthBytes - 2;
    if (addrLen > 0) {
        addr.mAddress.resize(addrLen);
        auto r = br.readBytes(addr.mAddress.data(), addrLen);
        if (!r) return Expected<L3PDPAddress>::error(r.error());
    }

    return Expected<L3PDPAddress>::hold(std::move(addr));
}

void L3PDPAddress::write(BitWriter& bw) const {
    // Octet 1: spare(4) | pdpTypeOrigin(4); octet 2: pdpTypeNumber.
    bw.writeField((mOrigin & 0x0Fu), 8);
    bw.writeField(mTypeNumber, 8);
    if (!mAddress.empty()) {
        bw.writeBytes(mAddress.data(), mAddress.size());
    }
}

void L3PDPAddress::text(std::ostream& os) const {
    PDPType t = type();
    os << "PDPAddr(type=";
    if (t == PDPType::IPv4) os << "IPv4";
    else if (t == PDPType::IPv6) os << "IPv6";
    else os << "0x" << std::hex << static_cast<int>(mTypeNumber) << std::dec;
    if (t == PDPType::IPv4 && mAddress.size() == 4) {
        os << ",addr=" << static_cast<int>(mAddress[0]) << "."
                   << static_cast<int>(mAddress[1]) << "."
                   << static_cast<int>(mAddress[2]) << "."
                   << static_cast<int>(mAddress[3]);
    } else if (!mAddress.empty()) {
        os << ",addr=";
        for (size_t i = 0; i < mAddress.size(); ++i) {
            if (i > 0) os << ":";
            os << std::hex << static_cast<int>(mAddress[i]);
        }
        os << std::dec;
    }
    os << ")";
}

// ── L3QoS (TS 44.068 section 10.5.6.5) ────────────────────────────────

Expected<L3QoS> L3QoS::parse(BitReader& br, size_t lengthBytes) {
    L3QoS qos;

    if (lengthBytes < 1) {
        return Expected<L3QoS>::error(
            ParseError{ParseError::Code::TruncatedInput, "QoS too short", br.position()});
    }

    auto type = br.readField(8);
    if (!type) return Expected<L3QoS>::error(type.error());
    qos.mType = static_cast<QoSType>(type.value());

    size_t elemLen = lengthBytes - 1;
    if (elemLen > 0) {
        qos.mElements.resize(elemLen);
        auto r = br.readBytes(qos.mElements.data(), elemLen);
        if (!r) return Expected<L3QoS>::error(r.error());
    }

    return Expected<L3QoS>::hold(std::move(qos));
}

void L3QoS::write(BitWriter& bw) const {
    bw.writeField(static_cast<uint8_t>(mType), 8);
    if (!mElements.empty()) {
        bw.writeBytes(mElements.data(), mElements.size());
    }
}

void L3QoS::text(std::ostream& os) const {
    os << "QoS(type=";
    switch (mType) {
        case QoSType::Requested: os << "requested"; break;
        case QoSType::Default: os << "default"; break;
        case QoSType::Teardown: os << "teardown"; break;
    }
    if (!mElements.empty()) {
        os << ",elements=";
        for (size_t i = 0; i < mElements.size(); ++i) {
            if (i > 0) os << ":";
            os << std::hex << static_cast<int>(mElements[i]);
        }
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
    // The flag is the most significant bit of the value nibble.
    return Expected<L3TearDownIndicator>::hold(L3TearDownIndicator{(octet & 0x08u) != 0});
}

void L3TearDownIndicator::write(BitWriter& bw) const {
    bw.writeField((IEI << 4) | (mFlag ? 0x08u : 0x00u), 8);
}

void L3TearDownIndicator::text(std::ostream& os) const {
    os << "TearDownIndicator(" << (mFlag ? "true" : "false") << ")";
}

// ── L3PDPHandle (TS 44.068 section 9.5) ───────────────────────────────

Expected<L3PDPHandle> L3PDPHandle::parse(BitReader& br) {
    auto val = br.readField(4);
    if (!val) return Expected<L3PDPHandle>::error(val.error());
    return Expected<L3PDPHandle>::hold(L3PDPHandle{static_cast<uint8_t>(val.value())});
}

void L3PDPHandle::write(BitWriter& bw) const {
    bw.writeField(mValue, 4);
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
