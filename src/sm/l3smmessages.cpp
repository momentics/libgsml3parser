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

// ── L3RequestPDPContextActivation (GSM 24.008 9.5.10) ─────────────────

size_t L3RequestPDPContextActivation::bodyLength() const {
    size_t len = 1;
    if (mHavePDPAddress) len += tlvLen(mPDPAddress.lengthV());
    len += tlvLen(mAPN.lengthV());
    len += tlvLen(mQoS.lengthV());
    if (mHavePCO) len += tlvLen(mPCO.lengthV());
    return len;
}

Expected<L3RequestPDPContextActivation> L3RequestPDPContextActivation::parse(BitReader& br) {
    L3RequestPDPContextActivation msg;
    // Read pdpHandle(4)|spare(4)
    {
        auto o = br.readField(4);
        if (!o) return Expected<L3RequestPDPContextActivation>::error(o.error());
        msg.mPDPHandle = static_cast<uint8_t>(o.value());
    }
    {
        auto spare = br.readField(4);
        if (!spare) return Expected<L3RequestPDPContextActivation>::error(spare.error());
    }
    // Parse TLV: PDPAddress | APN | QoS | PCO
    while (br.hasMore()) {
        size_t vLen = 0;
        auto iei = readTLVHeader(br, vLen);
        if (!iei) return Expected<L3RequestPDPContextActivation>::error(iei.error());
        if (iei.value() == L3PDPAddress::IEI) {
            auto addr = L3PDPAddress::parse(br, vLen);
            if (addr) { msg.mHavePDPAddress = true; msg.mPDPAddress = std::move(addr).value(); }
            else skipValue(br, vLen);
        } else if (iei.value() == L3AccessPointName::IEI) {
            auto apn = L3AccessPointName::parse(br, vLen);
            if (apn) msg.mAPN = std::move(apn).value();
            else skipValue(br, vLen);
        } else if (iei.value() == L3QoS::IEI) {
            auto qos = L3QoS::parse(br, vLen);
            if (qos) msg.mQoS = std::move(qos).value();
            else skipValue(br, vLen);
        } else if (iei.value() == L3ProtocolConfigOptions::IEI) {
            auto pco = L3ProtocolConfigOptions::parse(br, vLen);
            if (pco) { msg.mHavePCO = true; msg.mPCO = std::move(pco).value(); }
            else skipValue(br, vLen);
        } else {
            skipValue(br, vLen);
        }
    }
    return Expected<L3RequestPDPContextActivation>::hold(std::move(msg));
}

void L3RequestPDPContextActivation::write(BitWriter& bw) const {
    // Write pdpHandle(4)|spare(4)
    bw.writeField(mPDPHandle & 0x0F, 4);
    bw.writeField(0, 4);
    // Write PDPAddress: TLV (optional)
    if (mHavePDPAddress) {
        bw.writeField(0x80 | L3PDPAddress::IEI, 8);
        bw.writeField(static_cast<uint32_t>(mPDPAddress.lengthV()), 8);
        mPDPAddress.write(bw);
    }
    // Write APN: TLV (mandatory)
    bw.writeField(0x80 | L3AccessPointName::IEI, 8);
    bw.writeField(static_cast<uint32_t>(mAPN.lengthV()), 8);
    mAPN.write(bw);
    // Write QoS: TLV (mandatory)
    bw.writeField(0x80 | L3QoS::IEI, 8);
    bw.writeField(static_cast<uint32_t>(mQoS.lengthV()), 8);
    mQoS.write(bw);
    // Write PCO: TLV (optional)
    if (mHavePCO) {
        bw.writeField(0x80 | L3ProtocolConfigOptions::IEI, 8);
        bw.writeField(static_cast<uint32_t>(mPCO.lengthV()), 8);
        mPCO.write(bw);
    }
}

void L3RequestPDPContextActivation::text(std::ostream& os) const {
    os << "RequestPDPAct(handle=" << static_cast<int>(mPDPHandle);
    if (mHavePDPAddress) { os << ", "; mPDPAddress.text(os); }
    os << ", "; mAPN.text(os);
    os << ", "; mQoS.text(os);
    if (mHavePCO) { os << ", "; mPCO.text(os); }
    os << ")";
}

// ── L3RequestPDPContextActivationReject (GSM 24.008 9.5.10) ───────────

size_t L3RequestPDPContextActivationReject::bodyLength() const {
    return 1 + tlvLen(1);
}

Expected<L3RequestPDPContextActivationReject> L3RequestPDPContextActivationReject::parse(BitReader& br) {
    L3RequestPDPContextActivationReject msg;
    // Read pdpHandle(4)|spare(4)
    {
        auto o = br.readField(4);
        if (!o) return Expected<L3RequestPDPContextActivationReject>::error(o.error());
        msg.mPDPHandle = static_cast<uint8_t>(o.value());
    }
    {
        auto spare = br.readField(4);
        if (!spare) return Expected<L3RequestPDPContextActivationReject>::error(spare.error());
    }
    // Parse TLV: smCause
    while (br.hasMore()) {
        size_t vLen = 0;
        auto iei = readTLVHeader(br, vLen);
        if (!iei) return Expected<L3RequestPDPContextActivationReject>::error(iei.error());
        if (iei.value() == L3SMCauseIE::IEI && vLen >= 1) {
            auto cause = br.readField(8);
            if (cause) msg.mCause = static_cast<SMCause>(cause.value());
        } else {
            skipValue(br, vLen);
        }
    }
    return Expected<L3RequestPDPContextActivationReject>::hold(std::move(msg));
}

void L3RequestPDPContextActivationReject::write(BitWriter& bw) const {
    // Write pdpHandle(4)|spare(4)
    bw.writeField(mPDPHandle & 0x0F, 4);
    bw.writeField(0, 4);
    // Write smCause: TLV
    bw.writeField(0x80 | L3SMCauseIE::IEI, 8);
    bw.writeField(1, 8);
    bw.writeField(static_cast<uint8_t>(mCause), 8);
}

void L3RequestPDPContextActivationReject::text(std::ostream& os) const {
    os << "RequestPDPActRej(handle=" << static_cast<int>(mPDPHandle)
       << ",cause=" << SMCause2Str(mCause) << ")";
}

// ── L3ModifyPDPContextRequestMS (GSM 24.008 9.5.6) ────────────────────

size_t L3ModifyPDPContextRequestMS::bodyLength() const {
    size_t len = 1;
    len += tlvLen(mQoS.lengthV());
    if (mHavePCO) len += tlvLen(mPCO.lengthV());
    return len;
}

Expected<L3ModifyPDPContextRequestMS> L3ModifyPDPContextRequestMS::parse(BitReader& br) {
    L3ModifyPDPContextRequestMS msg;
    // Read pdpHandle(4)|spare(4)
    {
        auto o = br.readField(4);
        if (!o) return Expected<L3ModifyPDPContextRequestMS>::error(o.error());
        msg.mPDPHandle = static_cast<uint8_t>(o.value());
    }
    {
        auto spare = br.readField(4);
        if (!spare) return Expected<L3ModifyPDPContextRequestMS>::error(spare.error());
    }
    // Parse TLV: QoS | PCO
    while (br.hasMore()) {
        size_t vLen = 0;
        auto iei = readTLVHeader(br, vLen);
        if (!iei) return Expected<L3ModifyPDPContextRequestMS>::error(iei.error());
        if (iei.value() == L3QoS::IEI) {
            auto qos = L3QoS::parse(br, vLen);
            if (qos) msg.mQoS = std::move(qos).value();
            else skipValue(br, vLen);
        } else if (iei.value() == L3ProtocolConfigOptions::IEI) {
            auto pco = L3ProtocolConfigOptions::parse(br, vLen);
            if (pco) { msg.mHavePCO = true; msg.mPCO = std::move(pco).value(); }
            else skipValue(br, vLen);
        } else {
            skipValue(br, vLen);
        }
    }
    return Expected<L3ModifyPDPContextRequestMS>::hold(std::move(msg));
}

void L3ModifyPDPContextRequestMS::write(BitWriter& bw) const {
    // Write pdpHandle(4)|spare(4)
    bw.writeField(mPDPHandle & 0x0F, 4);
    bw.writeField(0, 4);
    // Write QoS: TLV (mandatory)
    bw.writeField(0x80 | L3QoS::IEI, 8);
    bw.writeField(static_cast<uint32_t>(mQoS.lengthV()), 8);
    mQoS.write(bw);
    // Write PCO: TLV (optional)
    if (mHavePCO) {
        bw.writeField(0x80 | L3ProtocolConfigOptions::IEI, 8);
        bw.writeField(static_cast<uint32_t>(mPCO.lengthV()), 8);
        mPCO.write(bw);
    }
}

void L3ModifyPDPContextRequestMS::text(std::ostream& os) const {
    os << "ModifyPDPReqMS(handle=" << static_cast<int>(mPDPHandle);
    os << ", "; mQoS.text(os);
    if (mHavePCO) { os << ", "; mPCO.text(os); }
    os << ")";
}

// ── L3ModifyPDPContextAcceptNet (GSM 24.008 9.5.7) ────────────────────

size_t L3ModifyPDPContextAcceptNet::bodyLength() const {
    size_t len = 1;
    len += tlvLen(mQoS.lengthV());
    if (mHavePCO) len += tlvLen(mPCO.lengthV());
    return len;
}

Expected<L3ModifyPDPContextAcceptNet> L3ModifyPDPContextAcceptNet::parse(BitReader& br) {
    L3ModifyPDPContextAcceptNet msg;
    // Read pdpHandle(4)|spare(4)
    {
        auto o = br.readField(4);
        if (!o) return Expected<L3ModifyPDPContextAcceptNet>::error(o.error());
        msg.mPDPHandle = static_cast<uint8_t>(o.value());
    }
    {
        auto spare = br.readField(4);
        if (!spare) return Expected<L3ModifyPDPContextAcceptNet>::error(spare.error());
    }
    // Parse TLV: QoS | PCO
    while (br.hasMore()) {
        size_t vLen = 0;
        auto iei = readTLVHeader(br, vLen);
        if (!iei) return Expected<L3ModifyPDPContextAcceptNet>::error(iei.error());
        if (iei.value() == L3QoS::IEI) {
            auto qos = L3QoS::parse(br, vLen);
            if (qos) msg.mQoS = std::move(qos).value();
            else skipValue(br, vLen);
        } else if (iei.value() == L3ProtocolConfigOptions::IEI) {
            auto pco = L3ProtocolConfigOptions::parse(br, vLen);
            if (pco) { msg.mHavePCO = true; msg.mPCO = std::move(pco).value(); }
            else skipValue(br, vLen);
        } else {
            skipValue(br, vLen);
        }
    }
    return Expected<L3ModifyPDPContextAcceptNet>::hold(std::move(msg));
}

void L3ModifyPDPContextAcceptNet::write(BitWriter& bw) const {
    // Write pdpHandle(4)|spare(4)
    bw.writeField(mPDPHandle & 0x0F, 4);
    bw.writeField(0, 4);
    // Write QoS: TLV (mandatory)
    bw.writeField(0x80 | L3QoS::IEI, 8);
    bw.writeField(static_cast<uint32_t>(mQoS.lengthV()), 8);
    mQoS.write(bw);
    // Write PCO: TLV (optional)
    if (mHavePCO) {
        bw.writeField(0x80 | L3ProtocolConfigOptions::IEI, 8);
        bw.writeField(static_cast<uint32_t>(mPCO.lengthV()), 8);
        mPCO.write(bw);
    }
}

void L3ModifyPDPContextAcceptNet::text(std::ostream& os) const {
    os << "ModifyPDPAccNet(handle=" << static_cast<int>(mPDPHandle);
    os << ", "; mQoS.text(os);
    if (mHavePCO) { os << ", "; mPCO.text(os); }
    os << ")";
}

// ── L3ActivateSecondaryPDPContextRequest (GSM 24.008 9.5.11) ──────────

size_t L3ActivateSecondaryPDPContextRequest::bodyLength() const {
    size_t len = 1;
    if (mHavePDPAddress) len += tlvLen(mPDPAddress.lengthV());
    len += tlvLen(mAPN.lengthV());
    len += tlvLen(mQoS.lengthV());
    if (mHavePCO) len += tlvLen(mPCO.lengthV());
    return len;
}

Expected<L3ActivateSecondaryPDPContextRequest> L3ActivateSecondaryPDPContextRequest::parse(BitReader& br) {
    L3ActivateSecondaryPDPContextRequest msg;
    // Read pdpHandle(4)|spare(4)
    {
        auto o = br.readField(4);
        if (!o) return Expected<L3ActivateSecondaryPDPContextRequest>::error(o.error());
        msg.mPDPHandle = static_cast<uint8_t>(o.value());
    }
    {
        auto spare = br.readField(4);
        if (!spare) return Expected<L3ActivateSecondaryPDPContextRequest>::error(spare.error());
    }
    // Parse TLV: PDPAddress | APN | QoS | PCO
    while (br.hasMore()) {
        size_t vLen = 0;
        auto iei = readTLVHeader(br, vLen);
        if (!iei) return Expected<L3ActivateSecondaryPDPContextRequest>::error(iei.error());
        if (iei.value() == L3PDPAddress::IEI) {
            auto addr = L3PDPAddress::parse(br, vLen);
            if (addr) { msg.mHavePDPAddress = true; msg.mPDPAddress = std::move(addr).value(); }
            else skipValue(br, vLen);
        } else if (iei.value() == L3AccessPointName::IEI) {
            auto apn = L3AccessPointName::parse(br, vLen);
            if (apn) msg.mAPN = std::move(apn).value();
            else skipValue(br, vLen);
        } else if (iei.value() == L3QoS::IEI) {
            auto qos = L3QoS::parse(br, vLen);
            if (qos) msg.mQoS = std::move(qos).value();
            else skipValue(br, vLen);
        } else if (iei.value() == L3ProtocolConfigOptions::IEI) {
            auto pco = L3ProtocolConfigOptions::parse(br, vLen);
            if (pco) { msg.mHavePCO = true; msg.mPCO = std::move(pco).value(); }
            else skipValue(br, vLen);
        } else {
            skipValue(br, vLen);
        }
    }
    return Expected<L3ActivateSecondaryPDPContextRequest>::hold(std::move(msg));
}

void L3ActivateSecondaryPDPContextRequest::write(BitWriter& bw) const {
    // Write pdpHandle(4)|spare(4)
    bw.writeField(mPDPHandle & 0x0F, 4);
    bw.writeField(0, 4);
    // Write PDPAddress: TLV (optional)
    if (mHavePDPAddress) {
        bw.writeField(0x80 | L3PDPAddress::IEI, 8);
        bw.writeField(static_cast<uint32_t>(mPDPAddress.lengthV()), 8);
        mPDPAddress.write(bw);
    }
    // Write APN: TLV (mandatory)
    bw.writeField(0x80 | L3AccessPointName::IEI, 8);
    bw.writeField(static_cast<uint32_t>(mAPN.lengthV()), 8);
    mAPN.write(bw);
    // Write QoS: TLV (mandatory)
    bw.writeField(0x80 | L3QoS::IEI, 8);
    bw.writeField(static_cast<uint32_t>(mQoS.lengthV()), 8);
    mQoS.write(bw);
    // Write PCO: TLV (optional)
    if (mHavePCO) {
        bw.writeField(0x80 | L3ProtocolConfigOptions::IEI, 8);
        bw.writeField(static_cast<uint32_t>(mPCO.lengthV()), 8);
        mPCO.write(bw);
    }
}

void L3ActivateSecondaryPDPContextRequest::text(std::ostream& os) const {
    os << "ActSecPDPReq(handle=" << static_cast<int>(mPDPHandle);
    if (mHavePDPAddress) { os << ", "; mPDPAddress.text(os); }
    os << ", "; mAPN.text(os);
    os << ", "; mQoS.text(os);
    if (mHavePCO) { os << ", "; mPCO.text(os); }
    os << ")";
}

// ── L3ActivateSecondaryPDPContextAccept (GSM 24.008 9.5.12) ───────────

size_t L3ActivateSecondaryPDPContextAccept::bodyLength() const {
    size_t len = 1;
    if (mHavePDPAddress) len += tlvLen(mPDPAddress.lengthV());
    len += tlvLen(mQoS.lengthV());
    if (mHavePCO) len += tlvLen(mPCO.lengthV());
    return len;
}

Expected<L3ActivateSecondaryPDPContextAccept> L3ActivateSecondaryPDPContextAccept::parse(BitReader& br) {
    L3ActivateSecondaryPDPContextAccept msg;
    // Read pdpHandle(4)|spare(4)
    {
        auto o = br.readField(4);
        if (!o) return Expected<L3ActivateSecondaryPDPContextAccept>::error(o.error());
        msg.mPDPHandle = static_cast<uint8_t>(o.value());
    }
    {
        auto spare = br.readField(4);
        if (!spare) return Expected<L3ActivateSecondaryPDPContextAccept>::error(spare.error());
    }
    // Parse TLV: PDPAddress | QoS | PCO
    while (br.hasMore()) {
        size_t vLen = 0;
        auto iei = readTLVHeader(br, vLen);
        if (!iei) return Expected<L3ActivateSecondaryPDPContextAccept>::error(iei.error());
        if (iei.value() == L3PDPAddress::IEI) {
            auto addr = L3PDPAddress::parse(br, vLen);
            if (addr) { msg.mHavePDPAddress = true; msg.mPDPAddress = std::move(addr).value(); }
            else skipValue(br, vLen);
        } else if (iei.value() == L3QoS::IEI) {
            auto qos = L3QoS::parse(br, vLen);
            if (qos) msg.mQoS = std::move(qos).value();
            else skipValue(br, vLen);
        } else if (iei.value() == L3ProtocolConfigOptions::IEI) {
            auto pco = L3ProtocolConfigOptions::parse(br, vLen);
            if (pco) { msg.mHavePCO = true; msg.mPCO = std::move(pco).value(); }
            else skipValue(br, vLen);
        } else {
            skipValue(br, vLen);
        }
    }
    return Expected<L3ActivateSecondaryPDPContextAccept>::hold(std::move(msg));
}

void L3ActivateSecondaryPDPContextAccept::write(BitWriter& bw) const {
    // Write pdpHandle(4)|spare(4)
    bw.writeField(mPDPHandle & 0x0F, 4);
    bw.writeField(0, 4);
    // Write PDPAddress: TLV (optional)
    if (mHavePDPAddress) {
        bw.writeField(0x80 | L3PDPAddress::IEI, 8);
        bw.writeField(static_cast<uint32_t>(mPDPAddress.lengthV()), 8);
        mPDPAddress.write(bw);
    }
    // Write QoS: TLV (mandatory)
    bw.writeField(0x80 | L3QoS::IEI, 8);
    bw.writeField(static_cast<uint32_t>(mQoS.lengthV()), 8);
    mQoS.write(bw);
    // Write PCO: TLV (optional)
    if (mHavePCO) {
        bw.writeField(0x80 | L3ProtocolConfigOptions::IEI, 8);
        bw.writeField(static_cast<uint32_t>(mPCO.lengthV()), 8);
        mPCO.write(bw);
    }
}

void L3ActivateSecondaryPDPContextAccept::text(std::ostream& os) const {
    os << "ActSecPDPAcc(handle=" << static_cast<int>(mPDPHandle);
    if (mHavePDPAddress) { os << ", "; mPDPAddress.text(os); }
    os << ", "; mQoS.text(os);
    if (mHavePCO) { os << ", "; mPCO.text(os); }
    os << ")";
}

// ── L3ActivateSecondaryPDPContextReject (GSM 24.008 9.5.13) ───────────

size_t L3ActivateSecondaryPDPContextReject::bodyLength() const {
    return 1 + tlvLen(1);
}

Expected<L3ActivateSecondaryPDPContextReject> L3ActivateSecondaryPDPContextReject::parse(BitReader& br) {
    L3ActivateSecondaryPDPContextReject msg;
    // Read pdpHandle(4)|spare(4)
    {
        auto o = br.readField(4);
        if (!o) return Expected<L3ActivateSecondaryPDPContextReject>::error(o.error());
        msg.mPDPHandle = static_cast<uint8_t>(o.value());
    }
    {
        auto spare = br.readField(4);
        if (!spare) return Expected<L3ActivateSecondaryPDPContextReject>::error(spare.error());
    }
    // Parse TLV: smCause
    while (br.hasMore()) {
        size_t vLen = 0;
        auto iei = readTLVHeader(br, vLen);
        if (!iei) return Expected<L3ActivateSecondaryPDPContextReject>::error(iei.error());
        if (iei.value() == L3SMCauseIE::IEI && vLen >= 1) {
            auto cause = br.readField(8);
            if (cause) msg.mCause = static_cast<SMCause>(cause.value());
        } else {
            skipValue(br, vLen);
        }
    }
    return Expected<L3ActivateSecondaryPDPContextReject>::hold(std::move(msg));
}

void L3ActivateSecondaryPDPContextReject::write(BitWriter& bw) const {
    // Write pdpHandle(4)|spare(4)
    bw.writeField(mPDPHandle & 0x0F, 4);
    bw.writeField(0, 4);
    // Write smCause: TLV
    bw.writeField(0x80 | L3SMCauseIE::IEI, 8);
    bw.writeField(1, 8);
    bw.writeField(static_cast<uint8_t>(mCause), 8);
}

void L3ActivateSecondaryPDPContextReject::text(std::ostream& os) const {
    os << "ActSecPDPRej(handle=" << static_cast<int>(mPDPHandle)
       << ",cause=" << SMCause2Str(mCause) << ")";
}

// ── L3ActivateAAPDPContextRequest (GSM 24.008 9.5.14) ─────────────────

size_t L3ActivateAAPDPContextRequest::bodyLength() const {
    size_t len = 1;
    if (mHavePDPAddress) len += tlvLen(mPDPAddress.lengthV());
    len += tlvLen(mAPN.lengthV());
    len += tlvLen(mQoS.lengthV());
    if (mHavePCO) len += tlvLen(mPCO.lengthV());
    return len;
}

Expected<L3ActivateAAPDPContextRequest> L3ActivateAAPDPContextRequest::parse(BitReader& br) {
    L3ActivateAAPDPContextRequest msg;
    // Read pdpHandle(4)|spare(4)
    {
        auto o = br.readField(4);
        if (!o) return Expected<L3ActivateAAPDPContextRequest>::error(o.error());
        msg.mPDPHandle = static_cast<uint8_t>(o.value());
    }
    {
        auto spare = br.readField(4);
        if (!spare) return Expected<L3ActivateAAPDPContextRequest>::error(spare.error());
    }
    // Parse TLV: PDPAddress | APN | QoS | PCO
    while (br.hasMore()) {
        size_t vLen = 0;
        auto iei = readTLVHeader(br, vLen);
        if (!iei) return Expected<L3ActivateAAPDPContextRequest>::error(iei.error());
        if (iei.value() == L3PDPAddress::IEI) {
            auto addr = L3PDPAddress::parse(br, vLen);
            if (addr) { msg.mHavePDPAddress = true; msg.mPDPAddress = std::move(addr).value(); }
            else skipValue(br, vLen);
        } else if (iei.value() == L3AccessPointName::IEI) {
            auto apn = L3AccessPointName::parse(br, vLen);
            if (apn) msg.mAPN = std::move(apn).value();
            else skipValue(br, vLen);
        } else if (iei.value() == L3QoS::IEI) {
            auto qos = L3QoS::parse(br, vLen);
            if (qos) msg.mQoS = std::move(qos).value();
            else skipValue(br, vLen);
        } else if (iei.value() == L3ProtocolConfigOptions::IEI) {
            auto pco = L3ProtocolConfigOptions::parse(br, vLen);
            if (pco) { msg.mHavePCO = true; msg.mPCO = std::move(pco).value(); }
            else skipValue(br, vLen);
        } else {
            skipValue(br, vLen);
        }
    }
    return Expected<L3ActivateAAPDPContextRequest>::hold(std::move(msg));
}

void L3ActivateAAPDPContextRequest::write(BitWriter& bw) const {
    // Write pdpHandle(4)|spare(4)
    bw.writeField(mPDPHandle & 0x0F, 4);
    bw.writeField(0, 4);
    // Write PDPAddress: TLV (optional)
    if (mHavePDPAddress) {
        bw.writeField(0x80 | L3PDPAddress::IEI, 8);
        bw.writeField(static_cast<uint32_t>(mPDPAddress.lengthV()), 8);
        mPDPAddress.write(bw);
    }
    // Write APN: TLV (mandatory)
    bw.writeField(0x80 | L3AccessPointName::IEI, 8);
    bw.writeField(static_cast<uint32_t>(mAPN.lengthV()), 8);
    mAPN.write(bw);
    // Write QoS: TLV (mandatory)
    bw.writeField(0x80 | L3QoS::IEI, 8);
    bw.writeField(static_cast<uint32_t>(mQoS.lengthV()), 8);
    mQoS.write(bw);
    // Write PCO: TLV (optional)
    if (mHavePCO) {
        bw.writeField(0x80 | L3ProtocolConfigOptions::IEI, 8);
        bw.writeField(static_cast<uint32_t>(mPCO.lengthV()), 8);
        mPCO.write(bw);
    }
}

void L3ActivateAAPDPContextRequest::text(std::ostream& os) const {
    os << "ActAAPDPReq(handle=" << static_cast<int>(mPDPHandle);
    if (mHavePDPAddress) { os << ", "; mPDPAddress.text(os); }
    os << ", "; mAPN.text(os);
    os << ", "; mQoS.text(os);
    if (mHavePCO) { os << ", "; mPCO.text(os); }
    os << ")";
}

// ── L3ActivateAAPDPContextAccept (GSM 24.008 9.5.15) ──────────────────

size_t L3ActivateAAPDPContextAccept::bodyLength() const {
    size_t len = 1;
    if (mHavePDPAddress) len += tlvLen(mPDPAddress.lengthV());
    len += tlvLen(mQoS.lengthV());
    if (mHavePCO) len += tlvLen(mPCO.lengthV());
    return len;
}

Expected<L3ActivateAAPDPContextAccept> L3ActivateAAPDPContextAccept::parse(BitReader& br) {
    L3ActivateAAPDPContextAccept msg;
    // Read pdpHandle(4)|spare(4)
    {
        auto o = br.readField(4);
        if (!o) return Expected<L3ActivateAAPDPContextAccept>::error(o.error());
        msg.mPDPHandle = static_cast<uint8_t>(o.value());
    }
    {
        auto spare = br.readField(4);
        if (!spare) return Expected<L3ActivateAAPDPContextAccept>::error(spare.error());
    }
    // Parse TLV: PDPAddress | QoS | PCO
    while (br.hasMore()) {
        size_t vLen = 0;
        auto iei = readTLVHeader(br, vLen);
        if (!iei) return Expected<L3ActivateAAPDPContextAccept>::error(iei.error());
        if (iei.value() == L3PDPAddress::IEI) {
            auto addr = L3PDPAddress::parse(br, vLen);
            if (addr) { msg.mHavePDPAddress = true; msg.mPDPAddress = std::move(addr).value(); }
            else skipValue(br, vLen);
        } else if (iei.value() == L3QoS::IEI) {
            auto qos = L3QoS::parse(br, vLen);
            if (qos) msg.mQoS = std::move(qos).value();
            else skipValue(br, vLen);
        } else if (iei.value() == L3ProtocolConfigOptions::IEI) {
            auto pco = L3ProtocolConfigOptions::parse(br, vLen);
            if (pco) { msg.mHavePCO = true; msg.mPCO = std::move(pco).value(); }
            else skipValue(br, vLen);
        } else {
            skipValue(br, vLen);
        }
    }
    return Expected<L3ActivateAAPDPContextAccept>::hold(std::move(msg));
}

void L3ActivateAAPDPContextAccept::write(BitWriter& bw) const {
    // Write pdpHandle(4)|spare(4)
    bw.writeField(mPDPHandle & 0x0F, 4);
    bw.writeField(0, 4);
    // Write PDPAddress: TLV (optional)
    if (mHavePDPAddress) {
        bw.writeField(0x80 | L3PDPAddress::IEI, 8);
        bw.writeField(static_cast<uint32_t>(mPDPAddress.lengthV()), 8);
        mPDPAddress.write(bw);
    }
    // Write QoS: TLV (mandatory)
    bw.writeField(0x80 | L3QoS::IEI, 8);
    bw.writeField(static_cast<uint32_t>(mQoS.lengthV()), 8);
    mQoS.write(bw);
    // Write PCO: TLV (optional)
    if (mHavePCO) {
        bw.writeField(0x80 | L3ProtocolConfigOptions::IEI, 8);
        bw.writeField(static_cast<uint32_t>(mPCO.lengthV()), 8);
        mPCO.write(bw);
    }
}

void L3ActivateAAPDPContextAccept::text(std::ostream& os) const {
    os << "ActAAPDPAcc(handle=" << static_cast<int>(mPDPHandle);
    if (mHavePDPAddress) { os << ", "; mPDPAddress.text(os); }
    os << ", "; mQoS.text(os);
    if (mHavePCO) { os << ", "; mPCO.text(os); }
    os << ")";
}

// ── L3ActivateAAPDPContextReject (GSM 24.008 9.5.16) ──────────────────

size_t L3ActivateAAPDPContextReject::bodyLength() const {
    return 1 + tlvLen(1);
}

Expected<L3ActivateAAPDPContextReject> L3ActivateAAPDPContextReject::parse(BitReader& br) {
    L3ActivateAAPDPContextReject msg;
    // Read pdpHandle(4)|spare(4)
    {
        auto o = br.readField(4);
        if (!o) return Expected<L3ActivateAAPDPContextReject>::error(o.error());
        msg.mPDPHandle = static_cast<uint8_t>(o.value());
    }
    {
        auto spare = br.readField(4);
        if (!spare) return Expected<L3ActivateAAPDPContextReject>::error(spare.error());
    }
    // Parse TLV: smCause
    while (br.hasMore()) {
        size_t vLen = 0;
        auto iei = readTLVHeader(br, vLen);
        if (!iei) return Expected<L3ActivateAAPDPContextReject>::error(iei.error());
        if (iei.value() == L3SMCauseIE::IEI && vLen >= 1) {
            auto cause = br.readField(8);
            if (cause) msg.mCause = static_cast<SMCause>(cause.value());
        } else {
            skipValue(br, vLen);
        }
    }
    return Expected<L3ActivateAAPDPContextReject>::hold(std::move(msg));
}

void L3ActivateAAPDPContextReject::write(BitWriter& bw) const {
    // Write pdpHandle(4)|spare(4)
    bw.writeField(mPDPHandle & 0x0F, 4);
    bw.writeField(0, 4);
    // Write smCause: TLV
    bw.writeField(0x80 | L3SMCauseIE::IEI, 8);
    bw.writeField(1, 8);
    bw.writeField(static_cast<uint8_t>(mCause), 8);
}

void L3ActivateAAPDPContextReject::text(std::ostream& os) const {
    os << "ActAAPDPRej(handle=" << static_cast<int>(mPDPHandle)
       << ",cause=" << SMCause2Str(mCause) << ")";
}

// ── L3DeactivateAAPDPContextRequest (GSM 24.008 9.5.17) ───────────────

Expected<L3DeactivateAAPDPContextRequest> L3DeactivateAAPDPContextRequest::parse(BitReader& br) {
    L3DeactivateAAPDPContextRequest msg;
    // Read pdpHandle(4)|spare(4)
    auto o = br.readField(4);
    if (!o) return Expected<L3DeactivateAAPDPContextRequest>::error(o.error());
    msg.mPDPHandle = static_cast<uint8_t>(o.value());
    auto spare = br.readField(4);
    if (!spare) return Expected<L3DeactivateAAPDPContextRequest>::error(spare.error());
    return Expected<L3DeactivateAAPDPContextRequest>::hold(std::move(msg));
}

void L3DeactivateAAPDPContextRequest::write(BitWriter& bw) const {
    // Write pdpHandle(4)|spare(4)
    bw.writeField(mPDPHandle & 0x0F, 4);
    bw.writeField(0, 4);
}

void L3DeactivateAAPDPContextRequest::text(std::ostream& os) const {
    os << "DeactAAPDPReq(handle=" << static_cast<int>(mPDPHandle) << ")";
}

// ── L3DeactivateAAPDPContextAccept (GSM 24.008 9.5.17) ────────────────

Expected<L3DeactivateAAPDPContextAccept> L3DeactivateAAPDPContextAccept::parse(BitReader& br) {
    L3DeactivateAAPDPContextAccept msg;
    // Read pdpHandle(4)|spare(4)
    auto o = br.readField(4);
    if (!o) return Expected<L3DeactivateAAPDPContextAccept>::error(o.error());
    msg.mPDPHandle = static_cast<uint8_t>(o.value());
    auto spare = br.readField(4);
    if (!spare) return Expected<L3DeactivateAAPDPContextAccept>::error(spare.error());
    return Expected<L3DeactivateAAPDPContextAccept>::hold(std::move(msg));
}

void L3DeactivateAAPDPContextAccept::write(BitWriter& bw) const {
    // Write pdpHandle(4)|spare(4)
    bw.writeField(mPDPHandle & 0x0F, 4);
    bw.writeField(0, 4);
}

void L3DeactivateAAPDPContextAccept::text(std::ostream& os) const {
    os << "DeactAAPDPAcc(handle=" << static_cast<int>(mPDPHandle) << ")";
}

// ── L3ActivateMBMSContextRequest (GSM 24.008 9.5.18) ──────────────────

size_t L3ActivateMBMSContextRequest::bodyLength() const {
    size_t len = tlvLen(L3TMGI::lengthV());
    len += tlvLen(mQoS.lengthV());
    if (mHavePCO) len += tlvLen(mPCO.lengthV());
    return len;
}

Expected<L3ActivateMBMSContextRequest> L3ActivateMBMSContextRequest::parse(BitReader& br) {
    L3ActivateMBMSContextRequest msg;
    // Parse TLV: TMGI | QoS | PCO
    while (br.hasMore()) {
        size_t vLen = 0;
        auto iei = readTLVHeader(br, vLen);
        if (!iei) return Expected<L3ActivateMBMSContextRequest>::error(iei.error());
        if (iei.value() == L3TMGI::IEI) {
            auto tmgi = L3TMGI::parse(br, vLen);
            if (tmgi) msg.mTMGI = std::move(tmgi).value();
            else skipValue(br, vLen);
        } else if (iei.value() == L3QoS::IEI) {
            auto qos = L3QoS::parse(br, vLen);
            if (qos) msg.mQoS = std::move(qos).value();
            else skipValue(br, vLen);
        } else if (iei.value() == L3ProtocolConfigOptions::IEI) {
            auto pco = L3ProtocolConfigOptions::parse(br, vLen);
            if (pco) { msg.mHavePCO = true; msg.mPCO = std::move(pco).value(); }
            else skipValue(br, vLen);
        } else {
            skipValue(br, vLen);
        }
    }
    return Expected<L3ActivateMBMSContextRequest>::hold(std::move(msg));
}

void L3ActivateMBMSContextRequest::write(BitWriter& bw) const {
    // Write TMGI: TLV (mandatory)
    bw.writeField(0x80 | L3TMGI::IEI, 8);
    bw.writeField(static_cast<uint32_t>(L3TMGI::lengthV()), 8);
    mTMGI.write(bw);
    // Write QoS: TLV (mandatory)
    bw.writeField(0x80 | L3QoS::IEI, 8);
    bw.writeField(static_cast<uint32_t>(mQoS.lengthV()), 8);
    mQoS.write(bw);
    // Write PCO: TLV (optional)
    if (mHavePCO) {
        bw.writeField(0x80 | L3ProtocolConfigOptions::IEI, 8);
        bw.writeField(static_cast<uint32_t>(mPCO.lengthV()), 8);
        mPCO.write(bw);
    }
}

void L3ActivateMBMSContextRequest::text(std::ostream& os) const {
    os << "ActMBMSReq("; mTMGI.text(os);
    os << ", "; mQoS.text(os);
    if (mHavePCO) { os << ", "; mPCO.text(os); }
    os << ")";
}

// ── L3ActivateMBMSContextAccept (GSM 24.008 9.5.19) ───────────────────

size_t L3ActivateMBMSContextAccept::bodyLength() const {
    size_t len = 1;
    len += tlvLen(mQoS.lengthV());
    if (mHavePCO) len += tlvLen(mPCO.lengthV());
    return len;
}

Expected<L3ActivateMBMSContextAccept> L3ActivateMBMSContextAccept::parse(BitReader& br) {
    L3ActivateMBMSContextAccept msg;
    // Read pdpHandle(4)|spare(4)
    {
        auto o = br.readField(4);
        if (!o) return Expected<L3ActivateMBMSContextAccept>::error(o.error());
        msg.mPDPHandle = static_cast<uint8_t>(o.value());
    }
    {
        auto spare = br.readField(4);
        if (!spare) return Expected<L3ActivateMBMSContextAccept>::error(spare.error());
    }
    // Parse TLV: QoS | PCO
    while (br.hasMore()) {
        size_t vLen = 0;
        auto iei = readTLVHeader(br, vLen);
        if (!iei) return Expected<L3ActivateMBMSContextAccept>::error(iei.error());
        if (iei.value() == L3QoS::IEI) {
            auto qos = L3QoS::parse(br, vLen);
            if (qos) msg.mQoS = std::move(qos).value();
            else skipValue(br, vLen);
        } else if (iei.value() == L3ProtocolConfigOptions::IEI) {
            auto pco = L3ProtocolConfigOptions::parse(br, vLen);
            if (pco) { msg.mHavePCO = true; msg.mPCO = std::move(pco).value(); }
            else skipValue(br, vLen);
        } else {
            skipValue(br, vLen);
        }
    }
    return Expected<L3ActivateMBMSContextAccept>::hold(std::move(msg));
}

void L3ActivateMBMSContextAccept::write(BitWriter& bw) const {
    // Write pdpHandle(4)|spare(4)
    bw.writeField(mPDPHandle & 0x0F, 4);
    bw.writeField(0, 4);
    // Write QoS: TLV (mandatory)
    bw.writeField(0x80 | L3QoS::IEI, 8);
    bw.writeField(static_cast<uint32_t>(mQoS.lengthV()), 8);
    mQoS.write(bw);
    // Write PCO: TLV (optional)
    if (mHavePCO) {
        bw.writeField(0x80 | L3ProtocolConfigOptions::IEI, 8);
        bw.writeField(static_cast<uint32_t>(mPCO.lengthV()), 8);
        mPCO.write(bw);
    }
}

void L3ActivateMBMSContextAccept::text(std::ostream& os) const {
    os << "ActMBMSAcc(handle=" << static_cast<int>(mPDPHandle);
    os << ", "; mQoS.text(os);
    if (mHavePCO) { os << ", "; mPCO.text(os); }
    os << ")";
}

// ── L3ActivateMBMSContextReject (GSM 24.008 9.5.20) ───────────────────

size_t L3ActivateMBMSContextReject::bodyLength() const {
    return tlvLen(1);
}

Expected<L3ActivateMBMSContextReject> L3ActivateMBMSContextReject::parse(BitReader& br) {
    L3ActivateMBMSContextReject msg;
    // Parse TLV: smCause
    while (br.hasMore()) {
        size_t vLen = 0;
        auto iei = readTLVHeader(br, vLen);
        if (!iei) return Expected<L3ActivateMBMSContextReject>::error(iei.error());
        if (iei.value() == L3SMCauseIE::IEI && vLen >= 1) {
            auto cause = br.readField(8);
            if (cause) msg.mCause = static_cast<SMCause>(cause.value());
        } else {
            skipValue(br, vLen);
        }
    }
    return Expected<L3ActivateMBMSContextReject>::hold(std::move(msg));
}

void L3ActivateMBMSContextReject::write(BitWriter& bw) const {
    // Write smCause: TLV
    bw.writeField(0x80 | L3SMCauseIE::IEI, 8);
    bw.writeField(1, 8);
    bw.writeField(static_cast<uint8_t>(mCause), 8);
}

void L3ActivateMBMSContextReject::text(std::ostream& os) const {
    os << "ActMBMSRej(cause=" << SMCause2Str(mCause) << ")";
}

// ── L3RequestMBMSContextActivation (GSM 24.008 9.5.21) ────────────────

size_t L3RequestMBMSContextActivation::bodyLength() const {
    size_t len = tlvLen(L3TMGI::lengthV());
    len += tlvLen(mQoS.lengthV());
    if (mHavePCO) len += tlvLen(mPCO.lengthV());
    return len;
}

Expected<L3RequestMBMSContextActivation> L3RequestMBMSContextActivation::parse(BitReader& br) {
    L3RequestMBMSContextActivation msg;
    // Parse TLV: TMGI | QoS | PCO
    while (br.hasMore()) {
        size_t vLen = 0;
        auto iei = readTLVHeader(br, vLen);
        if (!iei) return Expected<L3RequestMBMSContextActivation>::error(iei.error());
        if (iei.value() == L3TMGI::IEI) {
            auto tmgi = L3TMGI::parse(br, vLen);
            if (tmgi) msg.mTMGI = std::move(tmgi).value();
            else skipValue(br, vLen);
        } else if (iei.value() == L3QoS::IEI) {
            auto qos = L3QoS::parse(br, vLen);
            if (qos) msg.mQoS = std::move(qos).value();
            else skipValue(br, vLen);
        } else if (iei.value() == L3ProtocolConfigOptions::IEI) {
            auto pco = L3ProtocolConfigOptions::parse(br, vLen);
            if (pco) { msg.mHavePCO = true; msg.mPCO = std::move(pco).value(); }
            else skipValue(br, vLen);
        } else {
            skipValue(br, vLen);
        }
    }
    return Expected<L3RequestMBMSContextActivation>::hold(std::move(msg));
}

void L3RequestMBMSContextActivation::write(BitWriter& bw) const {
    // Write TMGI: TLV (mandatory)
    bw.writeField(0x80 | L3TMGI::IEI, 8);
    bw.writeField(static_cast<uint32_t>(L3TMGI::lengthV()), 8);
    mTMGI.write(bw);
    // Write QoS: TLV (mandatory)
    bw.writeField(0x80 | L3QoS::IEI, 8);
    bw.writeField(static_cast<uint32_t>(mQoS.lengthV()), 8);
    mQoS.write(bw);
    // Write PCO: TLV (optional)
    if (mHavePCO) {
        bw.writeField(0x80 | L3ProtocolConfigOptions::IEI, 8);
        bw.writeField(static_cast<uint32_t>(mPCO.lengthV()), 8);
        mPCO.write(bw);
    }
}

void L3RequestMBMSContextActivation::text(std::ostream& os) const {
    os << "ReqMBMSAct("; mTMGI.text(os);
    os << ", "; mQoS.text(os);
    if (mHavePCO) { os << ", "; mPCO.text(os); }
    os << ")";
}

// ── L3RequestMBMSContextActivationReject (GSM 24.008 9.5.22) ──────────

size_t L3RequestMBMSContextActivationReject::bodyLength() const {
    return tlvLen(1);
}

Expected<L3RequestMBMSContextActivationReject> L3RequestMBMSContextActivationReject::parse(BitReader& br) {
    L3RequestMBMSContextActivationReject msg;
    // Parse TLV: smCause
    while (br.hasMore()) {
        size_t vLen = 0;
        auto iei = readTLVHeader(br, vLen);
        if (!iei) return Expected<L3RequestMBMSContextActivationReject>::error(iei.error());
        if (iei.value() == L3SMCauseIE::IEI && vLen >= 1) {
            auto cause = br.readField(8);
            if (cause) msg.mCause = static_cast<SMCause>(cause.value());
        } else {
            skipValue(br, vLen);
        }
    }
    return Expected<L3RequestMBMSContextActivationReject>::hold(std::move(msg));
}

void L3RequestMBMSContextActivationReject::write(BitWriter& bw) const {
    // Write smCause: TLV
    bw.writeField(0x80 | L3SMCauseIE::IEI, 8);
    bw.writeField(1, 8);
    bw.writeField(static_cast<uint8_t>(mCause), 8);
}

void L3RequestMBMSContextActivationReject::text(std::ostream& os) const {
    os << "ReqMBMSActRej(cause=" << SMCause2Str(mCause) << ")";
}

// ── L3RequestSecondaryPDPContextActivation (GSM 24.008 9.5.23) ────────

size_t L3RequestSecondaryPDPContextActivation::bodyLength() const {
    size_t len = 1;
    if (mHavePDPAddress) len += tlvLen(mPDPAddress.lengthV());
    len += tlvLen(mAPN.lengthV());
    len += tlvLen(mQoS.lengthV());
    if (mHavePCO) len += tlvLen(mPCO.lengthV());
    return len;
}

Expected<L3RequestSecondaryPDPContextActivation> L3RequestSecondaryPDPContextActivation::parse(BitReader& br) {
    L3RequestSecondaryPDPContextActivation msg;
    // Read pdpHandle(4)|spare(4)
    {
        auto o = br.readField(4);
        if (!o) return Expected<L3RequestSecondaryPDPContextActivation>::error(o.error());
        msg.mPDPHandle = static_cast<uint8_t>(o.value());
    }
    {
        auto spare = br.readField(4);
        if (!spare) return Expected<L3RequestSecondaryPDPContextActivation>::error(spare.error());
    }
    // Parse TLV: PDPAddress | APN | QoS | PCO
    while (br.hasMore()) {
        size_t vLen = 0;
        auto iei = readTLVHeader(br, vLen);
        if (!iei) return Expected<L3RequestSecondaryPDPContextActivation>::error(iei.error());
        if (iei.value() == L3PDPAddress::IEI) {
            auto addr = L3PDPAddress::parse(br, vLen);
            if (addr) { msg.mHavePDPAddress = true; msg.mPDPAddress = std::move(addr).value(); }
            else skipValue(br, vLen);
        } else if (iei.value() == L3AccessPointName::IEI) {
            auto apn = L3AccessPointName::parse(br, vLen);
            if (apn) msg.mAPN = std::move(apn).value();
            else skipValue(br, vLen);
        } else if (iei.value() == L3QoS::IEI) {
            auto qos = L3QoS::parse(br, vLen);
            if (qos) msg.mQoS = std::move(qos).value();
            else skipValue(br, vLen);
        } else if (iei.value() == L3ProtocolConfigOptions::IEI) {
            auto pco = L3ProtocolConfigOptions::parse(br, vLen);
            if (pco) { msg.mHavePCO = true; msg.mPCO = std::move(pco).value(); }
            else skipValue(br, vLen);
        } else {
            skipValue(br, vLen);
        }
    }
    return Expected<L3RequestSecondaryPDPContextActivation>::hold(std::move(msg));
}

void L3RequestSecondaryPDPContextActivation::write(BitWriter& bw) const {
    // Write pdpHandle(4)|spare(4)
    bw.writeField(mPDPHandle & 0x0F, 4);
    bw.writeField(0, 4);
    // Write PDPAddress: TLV (optional)
    if (mHavePDPAddress) {
        bw.writeField(0x80 | L3PDPAddress::IEI, 8);
        bw.writeField(static_cast<uint32_t>(mPDPAddress.lengthV()), 8);
        mPDPAddress.write(bw);
    }
    // Write APN: TLV (mandatory)
    bw.writeField(0x80 | L3AccessPointName::IEI, 8);
    bw.writeField(static_cast<uint32_t>(mAPN.lengthV()), 8);
    mAPN.write(bw);
    // Write QoS: TLV (mandatory)
    bw.writeField(0x80 | L3QoS::IEI, 8);
    bw.writeField(static_cast<uint32_t>(mQoS.lengthV()), 8);
    mQoS.write(bw);
    // Write PCO: TLV (optional)
    if (mHavePCO) {
        bw.writeField(0x80 | L3ProtocolConfigOptions::IEI, 8);
        bw.writeField(static_cast<uint32_t>(mPCO.lengthV()), 8);
        mPCO.write(bw);
    }
}

void L3RequestSecondaryPDPContextActivation::text(std::ostream& os) const {
    os << "ReqSecPDPAct(handle=" << static_cast<int>(mPDPHandle);
    if (mHavePDPAddress) { os << ", "; mPDPAddress.text(os); }
    os << ", "; mAPN.text(os);
    os << ", "; mQoS.text(os);
    if (mHavePCO) { os << ", "; mPCO.text(os); }
    os << ")";
}

// ── L3RequestSecondaryPDPContextActivationReject (GSM 24.008 9.5.24) ──

size_t L3RequestSecondaryPDPContextActivationReject::bodyLength() const {
    return 1 + tlvLen(1);
}

Expected<L3RequestSecondaryPDPContextActivationReject> L3RequestSecondaryPDPContextActivationReject::parse(BitReader& br) {
    L3RequestSecondaryPDPContextActivationReject msg;
    // Read pdpHandle(4)|spare(4)
    {
        auto o = br.readField(4);
        if (!o) return Expected<L3RequestSecondaryPDPContextActivationReject>::error(o.error());
        msg.mPDPHandle = static_cast<uint8_t>(o.value());
    }
    {
        auto spare = br.readField(4);
        if (!spare) return Expected<L3RequestSecondaryPDPContextActivationReject>::error(spare.error());
    }
    // Parse TLV: smCause
    while (br.hasMore()) {
        size_t vLen = 0;
        auto iei = readTLVHeader(br, vLen);
        if (!iei) return Expected<L3RequestSecondaryPDPContextActivationReject>::error(iei.error());
        if (iei.value() == L3SMCauseIE::IEI && vLen >= 1) {
            auto cause = br.readField(8);
            if (cause) msg.mCause = static_cast<SMCause>(cause.value());
        } else {
            skipValue(br, vLen);
        }
    }
    return Expected<L3RequestSecondaryPDPContextActivationReject>::hold(std::move(msg));
}

void L3RequestSecondaryPDPContextActivationReject::write(BitWriter& bw) const {
    // Write pdpHandle(4)|spare(4)
    bw.writeField(mPDPHandle & 0x0F, 4);
    bw.writeField(0, 4);
    // Write smCause: TLV
    bw.writeField(0x80 | L3SMCauseIE::IEI, 8);
    bw.writeField(1, 8);
    bw.writeField(static_cast<uint8_t>(mCause), 8);
}

void L3RequestSecondaryPDPContextActivationReject::text(std::ostream& os) const {
    os << "ReqSecPDPActRej(handle=" << static_cast<int>(mPDPHandle)
       << ",cause=" << SMCause2Str(mCause) << ")";
}

// ── L3SMNotification (GSM 24.008 9.5.25) ──────────────────────────────

Expected<L3SMNotification> L3SMNotification::parse(BitReader& br) {
    L3SMNotification msg;
    // Read pdpHandle(4)|spare(4)
    auto o = br.readField(4);
    if (!o) return Expected<L3SMNotification>::error(o.error());
    msg.mPDPHandle = static_cast<uint8_t>(o.value());
    auto spare = br.readField(4);
    if (!spare) return Expected<L3SMNotification>::error(spare.error());
    return Expected<L3SMNotification>::hold(std::move(msg));
}

void L3SMNotification::write(BitWriter& bw) const {
    // Write pdpHandle(4)|spare(4)
    bw.writeField(mPDPHandle & 0x0F, 4);
    bw.writeField(0, 4);
}

void L3SMNotification::text(std::ostream& os) const {
    os << "SMNotification(handle=" << static_cast<int>(mPDPHandle) << ")";
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
        case L3RequestPDPContextActivation::MTI: return "RequestPDPContextActivation";
        case L3RequestPDPContextActivationReject::MTI: return "RequestPDPContextActivationReject";
        case L3ModifyPDPContextRequestMS::MTI:  return "ModifyPDPContextRequestMS";
        case L3ModifyPDPContextAcceptNet::MTI:  return "ModifyPDPContextAcceptNet";
        case L3ActivateSecondaryPDPContextRequest::MTI: return "ActivateSecondaryPDPContextRequest";
        case L3ActivateSecondaryPDPContextAccept::MTI: return "ActivateSecondaryPDPContextAccept";
        case L3ActivateSecondaryPDPContextReject::MTI: return "ActivateSecondaryPDPContextReject";
        case L3ActivateAAPDPContextRequest::MTI: return "ActivateAAPDPContextRequest";
        case L3ActivateAAPDPContextAccept::MTI: return "ActivateAAPDPContextAccept";
        case L3ActivateAAPDPContextReject::MTI: return "ActivateAAPDPContextReject";
        case L3DeactivateAAPDPContextRequest::MTI: return "DeactivateAAPDPContextRequest";
        case L3DeactivateAAPDPContextAccept::MTI: return "DeactivateAAPDPContextAccept";
        case L3ActivateMBMSContextRequest::MTI: return "ActivateMBMSContextRequest";
        case L3ActivateMBMSContextAccept::MTI:  return "ActivateMBMSContextAccept";
        case L3ActivateMBMSContextReject::MTI:  return "ActivateMBMSContextReject";
        case L3RequestMBMSContextActivation::MTI: return "RequestMBMSContextActivation";
        case L3RequestMBMSContextActivationReject::MTI: return "RequestMBMSContextActivationReject";
        case L3RequestSecondaryPDPContextActivation::MTI: return "RequestSecondaryPDPContextActivation";
        case L3RequestSecondaryPDPContextActivationReject::MTI: return "RequestSecondaryPDPContextActivationReject";
        case L3SMNotification::MTI:             return "SMNotification";
        default:                                return "Unknown_SM";
    }
}

} // namespace gsml3parser
