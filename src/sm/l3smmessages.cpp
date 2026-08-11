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

// SM Messages — parse/write/text implementation
// Spec: 3GPP TS 24.008 sections 9.5, Table 10.4a
// Reference: ref/osmo-ttcn3-hacks/library/L3_Templates.ttcn — SM message templates
//            ref/OpenBTS/SGSNGGSN/GPRSL3Messages.h — L3SmMsg::MessageType

#include "gsml3parser/sm/l3smmessages.h"
#include <sstream>
#include <iomanip>

namespace gsml3parser {

namespace {

size_t tlvLen(size_t vLen) { return 2 + vLen; }

// Parse a TLV IE: reads Type(1) | Length(1) | Value(length bytes)
// Returns the raw IEI (masked from extended bit) and the value length.
Expected<uint8_t> readTLVHeader(BitReader& br, size_t& outLen) {
    auto type = br.readField(8);
    if (!type) return Expected<uint8_t>::error(type.error());
    uint8_t rawType = static_cast<uint8_t>(type.value());

    auto len = br.readField(8);
    if (!len) return Expected<uint8_t>::error(len.error());
    outLen = len.value();

    return Expected<uint8_t>::hold(static_cast<uint8_t>(rawType & 0x7F));
}

// Skip value bytes of known length.
Expected<void> skipValue(BitReader& br, size_t lenBytes) {
    if (lenBytes > 0) {
        std::vector<uint8_t> tmp(lenBytes);
        auto r = br.readBytes(tmp.data(), lenBytes);
        if (!r) return Expected<void>::error(r.error());
    }
    return Expected<void>::hold();
}

} // anonymous namespace

// ── L3ActivatePDPContextRequest (GSM 24.008 9.5.1) ────────────────────

size_t L3ActivatePDPContextRequest::bodyLength() const {
    size_t len = 1; // pdpType(4)|spare(4)
    if (mHavePDPAddress) len += tlvLen(mPDPAddress.lengthV());
    len += tlvLen(mAPN.lengthV());
    len += tlvLen(mQoS.lengthV());
    if (mHavePCO) len += tlvLen(mPCO.lengthV());
    return len;
}

Expected<L3ActivatePDPContextRequest> L3ActivatePDPContextRequest::parse(BitReader& br) {
    L3ActivatePDPContextRequest msg;

    // 24.008 9.5.1: pdpType(4)|spare(4) = 1 octet
    {
        auto o = br.readField(4);
        if (!o) return Expected<L3ActivatePDPContextRequest>::error(o.error());
        msg.mPDPType = static_cast<PDPType>(o.value());
    }

    // Spare 4 bits
    {
        auto spare = br.readField(4);
        if (!spare) return Expected<L3ActivatePDPContextRequest>::error(spare.error());
    }

    // Parse TLV IEs: PDPAddress(TLV,IEI=0x08), APN(TLV,IEI=0x2F), QoS(TLV,IEI=0x09), PCO(TLV,IEI=0x3C)
    while (br.hasMore()) {
        size_t vLen = 0;
        auto iei = readTLVHeader(br, vLen);
        if (!iei) return Expected<L3ActivatePDPContextRequest>::error(iei.error());

        if (iei.value() == L3PDPAddress::IEI) {
            auto addr = L3PDPAddress::parse(br, vLen);
            if (addr) {
                msg.mHavePDPAddress = true;
                msg.mPDPAddress = std::move(addr).value();
            } else {
                skipValue(br, vLen);
            }
        } else if (iei.value() == L3AccessPointName::IEI) {
            auto apn = L3AccessPointName::parse(br, vLen);
            if (apn) {
                msg.mAPN = std::move(apn).value();
            } else {
                skipValue(br, vLen);
            }
        } else if (iei.value() == L3QoS::IEI) {
            auto qos = L3QoS::parse(br, vLen);
            if (qos) {
                msg.mQoS = std::move(qos).value();
            } else {
                skipValue(br, vLen);
            }
        } else if (iei.value() == L3ProtocolConfigOptions::IEI) {
            auto pco = L3ProtocolConfigOptions::parse(br, vLen);
            if (pco) {
                msg.mHavePCO = true;
                msg.mPCO = std::move(pco).value();
            } else {
                skipValue(br, vLen);
            }
        } else {
            skipValue(br, vLen);
        }
    }

    return Expected<L3ActivatePDPContextRequest>::hold(std::move(msg));
}

void L3ActivatePDPContextRequest::write(BitWriter& bw) const {
    // pdpType(4)|spare(4)
    bw.writeField(static_cast<uint8_t>(mPDPType) & 0x0F, 4);
    bw.writeField(0, 4);

    // PDPAddress: TLV (optional)
    if (mHavePDPAddress) {
        bw.writeField(0x80 | L3PDPAddress::IEI, 8);
        bw.writeField(static_cast<uint32_t>(mPDPAddress.lengthV()), 8);
        mPDPAddress.write(bw);
    }

    // APN: TLV (mandatory)
    bw.writeField(0x80 | L3AccessPointName::IEI, 8);
    bw.writeField(static_cast<uint32_t>(mAPN.lengthV()), 8);
    mAPN.write(bw);

    // QoS: TLV (mandatory)
    bw.writeField(0x80 | L3QoS::IEI, 8);
    bw.writeField(static_cast<uint32_t>(mQoS.lengthV()), 8);
    mQoS.write(bw);

    // PCO: TLV (optional)
    if (mHavePCO) {
        bw.writeField(0x80 | L3ProtocolConfigOptions::IEI, 8);
        bw.writeField(static_cast<uint32_t>(mPCO.lengthV()), 8);
        mPCO.write(bw);
    }
}

void L3ActivatePDPContextRequest::text(std::ostream& os) const {
    os << "ActivatePDPReq(pdpType=" << static_cast<int>(mPDPType);
    if (mHavePDPAddress) {
        os << ", ";
        mPDPAddress.text(os);
    }
    os << ", ";
    mAPN.text(os);
    os << ", ";
    mQoS.text(os);
    if (mHavePCO) {
        os << ", ";
        mPCO.text(os);
    }
    os << ")";
}

// ── L3ActivatePDPContextAccept (GSM 24.008 9.5.2) ────────────────────

size_t L3ActivatePDPContextAccept::bodyLength() const {
    size_t len = 1; // pdpHandle(4)|spare(4)
    if (mHavePDPAddress) len += tlvLen(mPDPAddress.lengthV());
    len += tlvLen(mQoS.lengthV());
    if (mHavePCO) len += tlvLen(mPCO.lengthV());
    return len;
}

Expected<L3ActivatePDPContextAccept> L3ActivatePDPContextAccept::parse(BitReader& br) {
    L3ActivatePDPContextAccept msg;

    // 24.008 9.5.2: pdpHandle(4)|spare(4) = 1 octet
    {
        auto o = br.readField(4);
        if (!o) return Expected<L3ActivatePDPContextAccept>::error(o.error());
        msg.mPDPHandle = static_cast<uint8_t>(o.value());
    }

    // Spare 4 bits
    {
        auto spare = br.readField(4);
        if (!spare) return Expected<L3ActivatePDPContextAccept>::error(spare.error());
    }

    // Parse TLV IEs: PDPAddress(TLV,IEI=0x08), QoS(TLV,IEI=0x09), PCO(TLV,IEI=0x3C)
    while (br.hasMore()) {
        size_t vLen = 0;
        auto iei = readTLVHeader(br, vLen);
        if (!iei) return Expected<L3ActivatePDPContextAccept>::error(iei.error());

        if (iei.value() == L3PDPAddress::IEI) {
            auto addr = L3PDPAddress::parse(br, vLen);
            if (addr) {
                msg.mHavePDPAddress = true;
                msg.mPDPAddress = std::move(addr).value();
            } else {
                skipValue(br, vLen);
            }
        } else if (iei.value() == L3QoS::IEI) {
            auto qos = L3QoS::parse(br, vLen);
            if (qos) {
                msg.mQoS = std::move(qos).value();
            } else {
                skipValue(br, vLen);
            }
        } else if (iei.value() == L3ProtocolConfigOptions::IEI) {
            auto pco = L3ProtocolConfigOptions::parse(br, vLen);
            if (pco) {
                msg.mHavePCO = true;
                msg.mPCO = std::move(pco).value();
            } else {
                skipValue(br, vLen);
            }
        } else {
            skipValue(br, vLen);
        }
    }

    return Expected<L3ActivatePDPContextAccept>::hold(std::move(msg));
}

void L3ActivatePDPContextAccept::write(BitWriter& bw) const {
    // pdpHandle(4)|spare(4)
    bw.writeField(mPDPHandle & 0x0F, 4);
    bw.writeField(0, 4);

    // PDPAddress: TLV (optional)
    if (mHavePDPAddress) {
        bw.writeField(0x80 | L3PDPAddress::IEI, 8);
        bw.writeField(static_cast<uint32_t>(mPDPAddress.lengthV()), 8);
        mPDPAddress.write(bw);
    }

    // QoS: TLV (mandatory)
    bw.writeField(0x80 | L3QoS::IEI, 8);
    bw.writeField(static_cast<uint32_t>(mQoS.lengthV()), 8);
    mQoS.write(bw);

    // PCO: TLV (optional)
    if (mHavePCO) {
        bw.writeField(0x80 | L3ProtocolConfigOptions::IEI, 8);
        bw.writeField(static_cast<uint32_t>(mPCO.lengthV()), 8);
        mPCO.write(bw);
    }
}

void L3ActivatePDPContextAccept::text(std::ostream& os) const {
    os << "ActivatePDPAcc(handle=" << static_cast<int>(mPDPHandle);
    if (mHavePDPAddress) {
        os << ", ";
        mPDPAddress.text(os);
    }
    os << ", ";
    mQoS.text(os);
    if (mHavePCO) {
        os << ", ";
        mPCO.text(os);
    }
    os << ")";
}

// ── L3ActivatePDPContextReject (GSM 24.008 9.5.3) ────────────────────

size_t L3ActivatePDPContextReject::bodyLength() const {
    size_t len = tlvLen(1); // smCause TLV
    if (mHaveBackOffTimer) len += tlvLen(1);
    return len;
}

Expected<L3ActivatePDPContextReject> L3ActivatePDPContextReject::parse(BitReader& br) {
    L3ActivatePDPContextReject msg;

    while (br.hasMore()) {
        size_t vLen = 0;
        auto iei = readTLVHeader(br, vLen);
        if (!iei) return Expected<L3ActivatePDPContextReject>::error(iei.error());

        if (iei.value() == L3SMCauseIE::IEI && vLen >= 1) {
            auto cause = br.readField(8);
            if (cause) msg.mCause = static_cast<SMCause>(cause.value());
        } else if (iei.value() == L3BackOffTimer::IEI && vLen >= 1) {
            auto t = br.readField(8);
            if (t) {
                msg.mHaveBackOffTimer = true;
                msg.mBackOffTimer = L3BackOffTimer{static_cast<uint8_t>(t.value())};
            }
        } else {
            skipValue(br, vLen);
        }
    }

    return Expected<L3ActivatePDPContextReject>::hold(std::move(msg));
}

void L3ActivatePDPContextReject::write(BitWriter& bw) const {
    // smCause: TLV
    bw.writeField(0x80 | L3SMCauseIE::IEI, 8);
    bw.writeField(1, 8);
    bw.writeField(static_cast<uint8_t>(mCause), 8);

    // BackOffTimer: TLV (optional)
    if (mHaveBackOffTimer) {
        bw.writeField(0x80 | L3BackOffTimer::IEI, 8);
        bw.writeField(1, 8);
        bw.writeField(mBackOffTimer.value(), 8);
    }
}

void L3ActivatePDPContextReject::text(std::ostream& os) const {
    os << "ActivatePDPRej(cause=" << SMCause2Str(mCause) << ")";
}

// ── L3DeactivatePDPContextRequest (GSM 24.008 9.5.4) ─────────────────

size_t L3DeactivatePDPContextRequest::bodyLength() const {
    size_t len = 1; // pdpHandle(4)|spare(4)
    if (mHavePDPType) len += 1; // PDPType TV
    if (mHavePDPAddress) len += tlvLen(mPDPAddress.lengthV());
    return len;
}

Expected<L3DeactivatePDPContextRequest> L3DeactivatePDPContextRequest::parse(BitReader& br) {
    L3DeactivatePDPContextRequest msg;

    // 24.008 9.5.4: pdpHandle(4)|spare(4) = 1 octet
    {
        auto o = br.readField(4);
        if (!o) return Expected<L3DeactivatePDPContextRequest>::error(o.error());
        msg.mPDPHandle = static_cast<uint8_t>(o.value());
    }

    // Spare 4 bits
    {
        auto spare = br.readField(4);
        if (!spare) return Expected<L3DeactivatePDPContextRequest>::error(spare.error());
    }

    while (br.hasMore()) {
        size_t vLen = 0;
        auto iei = readTLVHeader(br, vLen);
        if (!iei) return Expected<L3DeactivatePDPContextRequest>::error(iei.error());

        if (iei.value() == L3PDPAddress::IEI) {
            auto addr = L3PDPAddress::parse(br, vLen);
            if (addr) {
                msg.mHavePDPAddress = true;
                msg.mPDPAddress = std::move(addr).value();
            } else {
                skipValue(br, vLen);
            }
        } else {
            skipValue(br, vLen);
        }
    }

    return Expected<L3DeactivatePDPContextRequest>::hold(std::move(msg));
}

void L3DeactivatePDPContextRequest::write(BitWriter& bw) const {
    // pdpHandle(4)|spare(4)
    bw.writeField(mPDPHandle & 0x0F, 4);
    bw.writeField(0, 4);

    // PDPType: TV (optional)
    if (mHavePDPType) {
        bw.writeField(static_cast<uint8_t>(mPDPType), 8);
    }

    // PDPAddress: TLV (optional)
    if (mHavePDPAddress) {
        bw.writeField(0x80 | L3PDPAddress::IEI, 8);
        bw.writeField(static_cast<uint32_t>(mPDPAddress.lengthV()), 8);
        mPDPAddress.write(bw);
    }
}

void L3DeactivatePDPContextRequest::text(std::ostream& os) const {
    os << "DeactivatePDPReq(handle=" << static_cast<int>(mPDPHandle);
    if (mHavePDPType) {
        os << ",pdpType=" << static_cast<int>(mPDPType);
    }
    if (mHavePDPAddress) {
        os << ", ";
        mPDPAddress.text(os);
    }
    os << ")";
}

// ── L3DeactivatePDPContextAccept (GSM 24.008 9.5.5) ──────────────────

Expected<L3DeactivatePDPContextAccept> L3DeactivatePDPContextAccept::parse(BitReader& br) {
    L3DeactivatePDPContextAccept msg;

    // pdpHandle(4)|spare(4) = 1 octet
    auto o = br.readField(4);
    if (!o) return Expected<L3DeactivatePDPContextAccept>::error(o.error());
    msg.mPDPHandle = static_cast<uint8_t>(o.value());

    // Spare 4 bits
    auto spare = br.readField(4);
    if (!spare) return Expected<L3DeactivatePDPContextAccept>::error(spare.error());

    return Expected<L3DeactivatePDPContextAccept>::hold(std::move(msg));
}

void L3DeactivatePDPContextAccept::write(BitWriter& bw) const {
    bw.writeField(mPDPHandle & 0x0F, 4);
    bw.writeField(0, 4);
}

void L3DeactivatePDPContextAccept::text(std::ostream& os) const {
    os << "DeactivatePDPAcc(handle=" << static_cast<int>(mPDPHandle) << ")";
}

// ── L3ModifyPDPContextRequest (GSM 24.008 9.5.6) ─────────────────────

size_t L3ModifyPDPContextRequest::bodyLength() const {
    size_t len = 1; // pdpHandle(4)|spare(4)
    len += tlvLen(mQoS.lengthV());
    if (mHavePCO) len += tlvLen(mPCO.lengthV());
    return len;
}

Expected<L3ModifyPDPContextRequest> L3ModifyPDPContextRequest::parse(BitReader& br) {
    L3ModifyPDPContextRequest msg;

    // pdpHandle(4)|spare(4) = 1 octet
    {
        auto o = br.readField(4);
        if (!o) return Expected<L3ModifyPDPContextRequest>::error(o.error());
        msg.mPDPHandle = static_cast<uint8_t>(o.value());
    }

    // Spare 4 bits
    {
        auto spare = br.readField(4);
        if (!spare) return Expected<L3ModifyPDPContextRequest>::error(spare.error());
    }

    while (br.hasMore()) {
        size_t vLen = 0;
        auto iei = readTLVHeader(br, vLen);
        if (!iei) return Expected<L3ModifyPDPContextRequest>::error(iei.error());

        if (iei.value() == L3QoS::IEI) {
            auto qos = L3QoS::parse(br, vLen);
            if (qos) {
                msg.mQoS = std::move(qos).value();
            } else {
                skipValue(br, vLen);
            }
        } else if (iei.value() == L3ProtocolConfigOptions::IEI) {
            auto pco = L3ProtocolConfigOptions::parse(br, vLen);
            if (pco) {
                msg.mHavePCO = true;
                msg.mPCO = std::move(pco).value();
            } else {
                skipValue(br, vLen);
            }
        } else {
            skipValue(br, vLen);
        }
    }

    return Expected<L3ModifyPDPContextRequest>::hold(std::move(msg));
}

void L3ModifyPDPContextRequest::write(BitWriter& bw) const {
    bw.writeField(mPDPHandle & 0x0F, 4);
    bw.writeField(0, 4);

    // QoS: TLV (mandatory)
    bw.writeField(0x80 | L3QoS::IEI, 8);
    bw.writeField(static_cast<uint32_t>(mQoS.lengthV()), 8);
    mQoS.write(bw);

    // PCO: TLV (optional)
    if (mHavePCO) {
        bw.writeField(0x80 | L3ProtocolConfigOptions::IEI, 8);
        bw.writeField(static_cast<uint32_t>(mPCO.lengthV()), 8);
        mPCO.write(bw);
    }
}

void L3ModifyPDPContextRequest::text(std::ostream& os) const {
    os << "ModifyPDPReq(handle=" << static_cast<int>(mPDPHandle);
    os << ", ";
    mQoS.text(os);
    if (mHavePCO) {
        os << ", ";
        mPCO.text(os);
    }
    os << ")";
}

// ── L3ModifyPDPContextAccept (GSM 24.008 9.5.7) ──────────────────────

size_t L3ModifyPDPContextAccept::bodyLength() const {
    size_t len = 1; // pdpHandle(4)|spare(4)
    len += tlvLen(mQoS.lengthV());
    if (mHavePCO) len += tlvLen(mPCO.lengthV());
    return len;
}

Expected<L3ModifyPDPContextAccept> L3ModifyPDPContextAccept::parse(BitReader& br) {
    L3ModifyPDPContextAccept msg;

    // pdpHandle(4)|spare(4) = 1 octet
    {
        auto o = br.readField(4);
        if (!o) return Expected<L3ModifyPDPContextAccept>::error(o.error());
        msg.mPDPHandle = static_cast<uint8_t>(o.value());
    }

    // Spare 4 bits
    {
        auto spare = br.readField(4);
        if (!spare) return Expected<L3ModifyPDPContextAccept>::error(spare.error());
    }

    while (br.hasMore()) {
        size_t vLen = 0;
        auto iei = readTLVHeader(br, vLen);
        if (!iei) return Expected<L3ModifyPDPContextAccept>::error(iei.error());

        if (iei.value() == L3QoS::IEI) {
            auto qos = L3QoS::parse(br, vLen);
            if (qos) {
                msg.mQoS = std::move(qos).value();
            } else {
                skipValue(br, vLen);
            }
        } else if (iei.value() == L3ProtocolConfigOptions::IEI) {
            auto pco = L3ProtocolConfigOptions::parse(br, vLen);
            if (pco) {
                msg.mHavePCO = true;
                msg.mPCO = std::move(pco).value();
            } else {
                skipValue(br, vLen);
            }
        } else {
            skipValue(br, vLen);
        }
    }

    return Expected<L3ModifyPDPContextAccept>::hold(std::move(msg));
}

void L3ModifyPDPContextAccept::write(BitWriter& bw) const {
    bw.writeField(mPDPHandle & 0x0F, 4);
    bw.writeField(0, 4);

    // QoS: TLV (mandatory)
    bw.writeField(0x80 | L3QoS::IEI, 8);
    bw.writeField(static_cast<uint32_t>(mQoS.lengthV()), 8);
    mQoS.write(bw);

    // PCO: TLV (optional)
    if (mHavePCO) {
        bw.writeField(0x80 | L3ProtocolConfigOptions::IEI, 8);
        bw.writeField(static_cast<uint32_t>(mPCO.lengthV()), 8);
        mPCO.write(bw);
    }
}

void L3ModifyPDPContextAccept::text(std::ostream& os) const {
    os << "ModifyPDPAcc(handle=" << static_cast<int>(mPDPHandle);
    os << ", ";
    mQoS.text(os);
    if (mHavePCO) {
        os << ", ";
        mPCO.text(os);
    }
    os << ")";
}

// ── L3ModifyPDPContextReject (GSM 24.008 9.5.8) ──────────────────────

size_t L3ModifyPDPContextReject::bodyLength() const {
    size_t len = 1; // pdpHandle(4)|spare(4)
    len += tlvLen(1); // smCause TLV
    if (mHaveBackOffTimer) len += tlvLen(1);
    return len;
}

Expected<L3ModifyPDPContextReject> L3ModifyPDPContextReject::parse(BitReader& br) {
    L3ModifyPDPContextReject msg;

    // pdpHandle(4)|spare(4) = 1 octet
    {
        auto o = br.readField(4);
        if (!o) return Expected<L3ModifyPDPContextReject>::error(o.error());
        msg.mPDPHandle = static_cast<uint8_t>(o.value());
    }

    // Spare 4 bits
    {
        auto spare = br.readField(4);
        if (!spare) return Expected<L3ModifyPDPContextReject>::error(spare.error());
    }

    while (br.hasMore()) {
        size_t vLen = 0;
        auto iei = readTLVHeader(br, vLen);
        if (!iei) return Expected<L3ModifyPDPContextReject>::error(iei.error());

        if (iei.value() == L3SMCauseIE::IEI && vLen >= 1) {
            auto cause = br.readField(8);
            if (cause) msg.mCause = static_cast<SMCause>(cause.value());
        } else if (iei.value() == L3BackOffTimer::IEI && vLen >= 1) {
            auto t = br.readField(8);
            if (t) {
                msg.mHaveBackOffTimer = true;
                msg.mBackOffTimer = L3BackOffTimer{static_cast<uint8_t>(t.value())};
            }
        } else {
            skipValue(br, vLen);
        }
    }

    return Expected<L3ModifyPDPContextReject>::hold(std::move(msg));
}

void L3ModifyPDPContextReject::write(BitWriter& bw) const {
    bw.writeField(mPDPHandle & 0x0F, 4);
    bw.writeField(0, 4);

    // smCause: TLV
    bw.writeField(0x80 | L3SMCauseIE::IEI, 8);
    bw.writeField(1, 8);
    bw.writeField(static_cast<uint8_t>(mCause), 8);

    // BackOffTimer: TLV (optional)
    if (mHaveBackOffTimer) {
        bw.writeField(0x80 | L3BackOffTimer::IEI, 8);
        bw.writeField(1, 8);
        bw.writeField(mBackOffTimer.value(), 8);
    }
}

void L3ModifyPDPContextReject::text(std::ostream& os) const {
    os << "ModifyPDPRej(handle=" << static_cast<int>(mPDPHandle)
       << ",cause=" << SMCause2Str(mCause) << ")";
}

// ── L3SMStatus (GSM 24.008 9.5.9) ────────────────────────────────────

size_t L3SMStatus::bodyLength() const {
    return tlvLen(1); // smCause TLV
}

Expected<L3SMStatus> L3SMStatus::parse(BitReader& br) {
    L3SMStatus msg;

    while (br.hasMore()) {
        size_t vLen = 0;
        auto iei = readTLVHeader(br, vLen);
        if (!iei) return Expected<L3SMStatus>::error(iei.error());

        if (iei.value() == L3SMCauseIE::IEI && vLen >= 1) {
            auto cause = br.readField(8);
            if (cause) msg.mCause = static_cast<SMCause>(cause.value());
        } else {
            skipValue(br, vLen);
        }
    }

    return Expected<L3SMStatus>::hold(std::move(msg));
}

void L3SMStatus::write(BitWriter& bw) const {
    bw.writeField(0x80 | L3SMCauseIE::IEI, 8);
    bw.writeField(1, 8);
    bw.writeField(static_cast<uint8_t>(mCause), 8);
}

void L3SMStatus::text(std::ostream& os) const {
    os << "SMStatus(cause=" << SMCause2Str(mCause) << ")";
}

// ── smMessageName ───────────────────────────────────────────────────────

const char* smMessageName(int mti) {
    switch (mti) {
        case L3ActivatePDPContextRequest::MTI:  return "ActivatePDPContextRequest";
        case L3ActivatePDPContextAccept::MTI:   return "ActivatePDPContextAccept";
        case L3ActivatePDPContextReject::MTI:   return "ActivatePDPContextReject";
        case L3DeactivatePDPContextRequest::MTI: return "DeactivatePDPContextRequest";
        case L3DeactivatePDPContextAccept::MTI: return "DeactivatePDPContextAccept";
        case L3ModifyPDPContextRequest::MTI:    return "ModifyPDPContextRequest";
        case L3ModifyPDPContextAccept::MTI:     return "ModifyPDPContextAccept";
        case L3ModifyPDPContextReject::MTI:     return "ModifyPDPContextReject";
        case L3SMStatus::MTI:                   return "SMStatus";
        default:                                return "Unknown_SM";
    }
}

} // namespace gsml3parser
