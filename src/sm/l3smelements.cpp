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

// SM IE — parse/write/text implementation
// Spec: 3GPP TS 24.008 section 10.5.8
// Reference: ref/osmo-ttcn3-hacks/library/L3_Templates.ttcn — SM IE templates
//            ts_PdpType, ts_ApnTLV, ts_QoS_Elt, ts_PcoTLV

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

// ── L3PDPAddress (GSM 24.008 10.5.8.1) ───────────────────────────────

Expected<L3PDPAddress> L3PDPAddress::parse(BitReader& br, size_t lengthBytes) {
    L3PDPAddress addr;

    if (lengthBytes < 1) {
        return Expected<L3PDPAddress>::error(
            ParseError{ParseError::Code::TruncatedInput, "PDP address too short", br.position()});
    }

    auto type = br.readField(8);
    if (!type) return Expected<L3PDPAddress>::error(type.error());
    addr.mType = static_cast<PDPType>(type.value());

    size_t addrLen = lengthBytes - 1;
    if (addrLen > 0) {
        addr.mAddress.resize(addrLen);
        auto r = br.readBytes(addr.mAddress.data(), addrLen);
        if (!r) return Expected<L3PDPAddress>::error(r.error());
    }

    return Expected<L3PDPAddress>::hold(std::move(addr));
}

void L3PDPAddress::write(BitWriter& bw) const {
    bw.writeField(static_cast<uint8_t>(mType), 8);
    if (!mAddress.empty()) {
        bw.writeBytes(mAddress.data(), mAddress.size());
    }
}

void L3PDPAddress::text(std::ostream& os) const {
    os << "PDPAddr(type=" << static_cast<int>(mType);
    if (mType == PDPType::IPv4 && mAddress.size() == 4) {
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
    }
    os << ")";
}

// ── L3QoS (GSM 24.008 10.5.8.2) ──────────────────────────────────────

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

// ── L3AccessPointName (GSM 24.008 10.5.8.3) ──────────────────────────

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

// ── L3ProtocolConfigOptions (GSM 24.008 10.5.8.4) ────────────────────

Expected<L3ProtocolConfigOptions> L3ProtocolConfigOptions::parse(BitReader& br, size_t lengthBytes) {
    L3ProtocolConfigOptions pco;

    if (lengthBytes < 1) {
        return Expected<L3ProtocolConfigOptions>::error(
            ParseError{ParseError::Code::TruncatedInput, "PCO too short", br.position()});
    }

    auto type = br.readField(8);
    if (!type) return Expected<L3ProtocolConfigOptions>::error(type.error());
    pco.mType = static_cast<uint8_t>(type.value());

    size_t dataLen = lengthBytes - 1;
    if (dataLen > 0) {
        pco.mData.resize(dataLen);
        auto r = br.readBytes(pco.mData.data(), dataLen);
        if (!r) return Expected<L3ProtocolConfigOptions>::error(r.error());
    }

    return Expected<L3ProtocolConfigOptions>::hold(std::move(pco));
}

void L3ProtocolConfigOptions::write(BitWriter& bw) const {
    bw.writeField(mType, 8);
    if (!mData.empty()) {
        bw.writeBytes(mData.data(), mData.size());
    }
}

void L3ProtocolConfigOptions::text(std::ostream& os) const {
    os << "PCO(type=0x" << std::hex << static_cast<int>(mType);
    if (!mData.empty()) {
        os << ",data=";
        for (size_t i = 0; i < mData.size(); ++i) {
            if (i > 0) os << ":";
            os << std::hex << static_cast<int>(mData[i]);
        }
    }
    os << ")";
}

// ── L3SMCauseIE (GSM 24.008 10.5.3.2.3) ──────────────────────────────

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

// ── L3BackOffTimer (GSM 24.008 10.5.8.6) ─────────────────────────────

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

// ── L3PDPHandle (GSM 24.008 10.5.8.7) ────────────────────────────────

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

} // namespace gsml3parser
