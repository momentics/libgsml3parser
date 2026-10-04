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

// SM Messages - parse/write/text implementation
// Spec: 3GPP TS 44.068 (GSM 24.008) section 9.5.

#include "gsml3parser/sm/l3smmessages.h"
#include "gsml3parser/common/l3common.h"
#include <sstream>
#include <iomanip>

namespace gsml3parser {

namespace {

size_t tlvLen(size_t vLen) { return 2 + vLen; }

// Information element identifier of the "PDP type and address" TLV used in
// SM accept messages (TS 44.068 section 9.5).
constexpr uint8_t kPdpAddressTlvIEI = 0x2Bu;

// Consume a TLV header: reads Type(1) | Length(1). Returns the identifier
// octet (extension bit masked off) and the value length.
Expected<uint8_t> readTLVHeader(BitReader& br, size_t& outLen) {
    auto type = br.readField(8);
    if (!type) return Expected<uint8_t>::error(type.error());
    uint8_t rawType = static_cast<uint8_t>(type.value());

    auto len = br.readField(8);
    if (!len) return Expected<uint8_t>::error(len.error());
    outLen = len.value();

    return Expected<uint8_t>::hold(static_cast<uint8_t>(rawType & 0x7F));
}

// Append a TLV whose header was already consumed (identifier \p iei, value
// length \p lenBytes) to the opaque tail verbatim; the value octets are read
// from the stream.
Expected<void> appendTlvToTail(BitReader& br, std::vector<uint8_t>& tail, uint8_t iei, size_t lenBytes) {
    tail.push_back(iei);
    tail.push_back(static_cast<uint8_t>(lenBytes));
    if (lenBytes > 0) {
        for (size_t i = 0; i < lenBytes; ++i) {
            auto b = br.readField(8);
            if (!b) return Expected<void>::error(b.error());
            tail.push_back(static_cast<uint8_t>(b.value()));
        }
    }
    return Expected<void>::hold();
}

} // anonymous namespace

// ── L3ActivatePDPContextRequest (TS 44.068 section 9.5) ───────────────

size_t L3ActivatePDPContextRequest::bodyLength() const {
    size_t len = 2; // NSAPI octet + LLC SAPI octet
    len += 1 + mQoS.lengthV();         // requested QoS (LV)
    len += 1 + mPDPAddress.lengthV();  // requested PDP address (LV)
    len += tlvLen(mAPN.lengthV());     // APN (TLV, mandatory)
    if (mHavePCO) len += tlvLen(mPCO.lengthV());
    if (mHasRequestType) len += 1;     // request type TV octet
    len += mAdditionalIes.size();
    return len;
}

Expected<L3ActivatePDPContextRequest> L3ActivatePDPContextRequest::parse(BitReader& br) {
    // SM ACTIVATE PDP CONTEXT REQUEST (TS 44.068): NSAPI and the requested
    // LLC SAPI in the first two octets, requested QoS (LV) and requested
    // PDP type and address (LV), access point name (TLV, IEI 0x28,
    // mandatory); protocol configuration options (TLV, IEI 0x27), the
    // request type TV and any further optional IEs follow when present.
    L3ActivatePDPContextRequest msg;

    auto o1 = br.readField(8);
    if (!o1) return Expected<L3ActivatePDPContextRequest>::error(o1.error());
    msg.mNSapi = static_cast<uint8_t>((o1.value() >> 4) & 0x0Fu);

    auto o2 = br.readField(8);
    if (!o2) return Expected<L3ActivatePDPContextRequest>::error(o2.error());
    msg.mLLcSapi = static_cast<uint8_t>((o2.value() >> 4) & 0x0Fu);

    // Requested QoS: LV (mandatory).
    auto qlen = br.readField(8);
    if (!qlen) return Expected<L3ActivatePDPContextRequest>::error(qlen.error());
    {
        auto qos = L3QoS::parse(br, qlen.value());
        if (!qos) return Expected<L3ActivatePDPContextRequest>::error(qos.error());
        msg.mQoS = std::move(qos).value();
    }

    // Requested PDP type and address: LV (mandatory).
    auto alen = br.readField(8);
    if (!alen) return Expected<L3ActivatePDPContextRequest>::error(alen.error());
    {
        auto addr = L3PDPAddress::parse(br, alen.value());
        if (!addr) return Expected<L3ActivatePDPContextRequest>::error(addr.error());
        msg.mPDPAddress = std::move(addr).value();
    }

    // Access point name: TLV 0x28 (mandatory).
    {
        size_t vLen = 0;
        auto iei = readTLVHeader(br, vLen);
        if (!iei) return Expected<L3ActivatePDPContextRequest>::error(iei.error());
        if (iei.value() != L3AccessPointName::IEI) {
            return Expected<L3ActivatePDPContextRequest>::error(
                ParseError{ParseError::Code::InvalidIE, "activate PDP context request: missing access point name", br.position()});
        }
        auto apn = L3AccessPointName::parse(br, vLen);
        if (!apn) return Expected<L3ActivatePDPContextRequest>::error(apn.error());
        msg.mAPN = std::move(apn).value();
    }

    // Optional IE region: request type TV (one octet '1010'B|value), PCO
    // TLV 0x27, everything else preserved in the opaque tail.
    while (br.remainingBits() >= 8) {
        uint32_t next = br.peekField(8);
        if ((next & kSMRequestTypeMask) == (static_cast<uint32_t>(kSMRequestTypeIdentifier) << 4)) {
            auto o = br.readField(8);
            if (!o) return Expected<L3ActivatePDPContextRequest>::error(o.error());
            if (!msg.mHasRequestType) {
                msg.mHasRequestType = true;
                msg.mRequestType = static_cast<uint8_t>(o.value() & 0x0Fu);
            } else {
                msg.mAdditionalIes.push_back(static_cast<uint8_t>(o.value()));
            }
            continue;
        }
        if (br.remainingBits() < 16) break; // lone octet without a length: malformed
        size_t vLen = 0;
        auto iei = readTLVHeader(br, vLen);
        if (!iei) return Expected<L3ActivatePDPContextRequest>::error(iei.error());
        bool typed = false;
        if (iei.value() == L3ProtocolConfigOptions::IEI && !msg.mHavePCO) {
            if (auto pco = L3ProtocolConfigOptions::parse(br, vLen)) {
                msg.mHavePCO = true;
                msg.mPCO = std::move(pco).value();
                typed = true;
            }
        }
        if (!typed) {
            auto t = appendTlvToTail(br, msg.mAdditionalIes, iei.value(), vLen);
            if (!t) return Expected<L3ActivatePDPContextRequest>::error(t.error());
        }
    }
    if (!detail::readOpaqueTail(br, msg.mAdditionalIes)) {
        return Expected<L3ActivatePDPContextRequest>::error(
            ParseError{ParseError::Code::TruncatedInput, "activate PDP context request: truncated optional IEs", br.position()});
    }

    return Expected<L3ActivatePDPContextRequest>::hold(std::move(msg));
}

void L3ActivatePDPContextRequest::write(BitWriter& bw) const {
    // NSAPI(4)|spare(4), LLC SAPI(4)|spare(4).
    bw.writeField((static_cast<uint32_t>(mNSapi) & 0x0Fu) << 4, 8);
    bw.writeField((static_cast<uint32_t>(mLLcSapi) & 0x0Fu) << 4, 8);

    // Requested QoS: LV (mandatory).
    bw.writeField(static_cast<uint32_t>(mQoS.lengthV()), 8);
    mQoS.write(bw);

    // Requested PDP type and address: LV (mandatory).
    bw.writeField(static_cast<uint32_t>(mPDPAddress.lengthV()), 8);
    mPDPAddress.write(bw);

    // Access point name: TLV 0x28 (mandatory).
    bw.writeField(L3AccessPointName::IEI, 8);
    bw.writeField(static_cast<uint32_t>(mAPN.lengthV()), 8);
    mAPN.write(bw);

    // Protocol configuration options: TLV 0x27 (optional).
    if (mHavePCO) {
        bw.writeField(L3ProtocolConfigOptions::IEI, 8);
        bw.writeField(static_cast<uint32_t>(mPCO.lengthV()), 8);
        mPCO.write(bw);
    }

    // Request type TV: one octet '1010'B|value (optional).
    if (mHasRequestType) {
        bw.writeField((static_cast<uint32_t>(kSMRequestTypeIdentifier) << 4) | (mRequestType & 0x0Fu), 8);
    }

    detail::writeOpaqueTail(mAdditionalIes, bw);
}

void L3ActivatePDPContextRequest::text(std::ostream& os) const {
    os << "ActivatePDPReq(nsapi=" << static_cast<int>(mNSapi)
       << ",sapi=" << static_cast<int>(mLLcSapi);
    os << ", ";
    mQoS.text(os);
    os << ", ";
    mPDPAddress.text(os);
    os << ", ";
    mAPN.text(os);
    if (mHavePCO) {
        os << ", ";
        mPCO.text(os);
    }
    if (mHasRequestType) {
        os << ",reqType=" << static_cast<int>(requestType());
    }
    os << ")";
}

L3ActivatePDPContextRequest L3ActivatePDPContextRequest::Builder::build() const {
    L3ActivatePDPContextRequest msg;
    msg.mNSapi = m_nsapi & 0x0Fu;
    msg.mLLcSapi = m_llcSapi & 0x0Fu;
    msg.mQoS = m_qos;
    msg.mPDPAddress = m_pdpAddress;
    msg.mAPN = m_apn;
    msg.mHavePCO = m_havePCO;
    msg.mPCO = m_pco;
    msg.mHasRequestType = m_hasRequestType;
    msg.mRequestType = m_requestType & 0x0Fu;
    msg.mAdditionalIes = m_additionalIes;
    return msg;
}

L3ActivatePDPContextRequest::Builder L3ActivatePDPContextRequest::builder() {
    return Builder{};
}

// ── L3ActivatePDPContextAccept (TS 44.068 section 9.5) ────────────────

size_t L3ActivatePDPContextAccept::bodyLength() const {
    size_t len = 1; // LLC SAPI(4)|spare(4)
    len += 1 + mQoS.lengthV();         // negotiated QoS (LV)
    len += 1;                          // radio priority for SMS(4)|spare(4)
    if (mHavePDPAddress) len += tlvLen(mPDPAddress.lengthV());
    if (mHavePCO) len += tlvLen(mPCO.lengthV());
    len += mAdditionalIes.size();
    return len;
}

Expected<L3ActivatePDPContextAccept> L3ActivatePDPContextAccept::parse(BitReader& br) {
    // SM ACTIVATE PDP CONTEXT ACCEPT (TS 44.068): negotiated LLC SAPI in the
    // first octet, negotiated QoS (LV), radio priority for SMS (four bits)
    // with three spare bits, then optional IEs: PDP type and address
    // (TLV, IEI 0x2B), protocol configuration options (TLV, IEI 0x27); any
    // further optional IEs are kept opaque.
    L3ActivatePDPContextAccept msg;

    auto o = br.readField(8);
    if (!o) return Expected<L3ActivatePDPContextAccept>::error(o.error());
    msg.mLLcSapi = static_cast<uint8_t>((o.value() >> 4) & 0x0Fu);

    // Negotiated QoS: LV (mandatory).
    auto qlen = br.readField(8);
    if (!qlen) return Expected<L3ActivatePDPContextAccept>::error(qlen.error());
    {
        auto qos = L3QoS::parse(br, qlen.value());
        if (!qos) return Expected<L3ActivatePDPContextAccept>::error(qos.error());
        msg.mQoS = std::move(qos).value();
    }

    // Radio priority for SMS(4)|spare(4).
    auto rp = br.readField(8);
    if (!rp) return Expected<L3ActivatePDPContextAccept>::error(rp.error());
    msg.mRadioPriority = static_cast<uint8_t>((rp.value() >> 4) & 0x0Fu);

    // Optional IE region: PDP address TLV 0x2B, PCO TLV 0x27, rest opaque.
    while (br.remainingBits() >= 16) {
        size_t vLen = 0;
        auto iei = readTLVHeader(br, vLen);
        if (!iei) return Expected<L3ActivatePDPContextAccept>::error(iei.error());
        bool typed = false;
        if (iei.value() == kPdpAddressTlvIEI && !msg.mHavePDPAddress) {
            if (auto addr = L3PDPAddress::parse(br, vLen)) {
                msg.mHavePDPAddress = true;
                msg.mPDPAddress = std::move(addr).value();
                typed = true;
            }
        } else if (iei.value() == L3ProtocolConfigOptions::IEI && !msg.mHavePCO) {
            if (auto pco = L3ProtocolConfigOptions::parse(br, vLen)) {
                msg.mHavePCO = true;
                msg.mPCO = std::move(pco).value();
                typed = true;
            }
        }
        if (!typed) {
            auto t = appendTlvToTail(br, msg.mAdditionalIes, iei.value(), vLen);
            if (!t) return Expected<L3ActivatePDPContextAccept>::error(t.error());
        }
    }
    if (!detail::readOpaqueTail(br, msg.mAdditionalIes)) {
        return Expected<L3ActivatePDPContextAccept>::error(
            ParseError{ParseError::Code::TruncatedInput, "activate PDP context accept: truncated optional IEs", br.position()});
    }

    return Expected<L3ActivatePDPContextAccept>::hold(std::move(msg));
}

void L3ActivatePDPContextAccept::write(BitWriter& bw) const {
    // LLC SAPI(4)|spare(4).
    bw.writeField((static_cast<uint32_t>(mLLcSapi) & 0x0Fu) << 4, 8);

    // Negotiated QoS: LV (mandatory).
    bw.writeField(static_cast<uint32_t>(mQoS.lengthV()), 8);
    mQoS.write(bw);

    // Radio priority for SMS(4)|spare(4).
    bw.writeField((static_cast<uint32_t>(mRadioPriority) & 0x0Fu) << 4, 8);

    // PDP type and address: TLV 0x2B (optional).
    if (mHavePDPAddress) {
        bw.writeField(kPdpAddressTlvIEI, 8);
        bw.writeField(static_cast<uint32_t>(mPDPAddress.lengthV()), 8);
        mPDPAddress.write(bw);
    }

    // Protocol configuration options: TLV 0x27 (optional).
    if (mHavePCO) {
        bw.writeField(L3ProtocolConfigOptions::IEI, 8);
        bw.writeField(static_cast<uint32_t>(mPCO.lengthV()), 8);
        mPCO.write(bw);
    }

    detail::writeOpaqueTail(mAdditionalIes, bw);
}

void L3ActivatePDPContextAccept::text(std::ostream& os) const {
    os << "ActivatePDPAcc(sapi=" << static_cast<int>(mLLcSapi);
    os << ",radioPriority=" << static_cast<int>(mRadioPriority);
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

L3ActivatePDPContextAccept L3ActivatePDPContextAccept::Builder::build() const {
    L3ActivatePDPContextAccept msg;
    msg.mLLcSapi = m_llcSapi & 0x0Fu;
    msg.mQoS = m_qos;
    msg.mRadioPriority = m_radioPriority & 0x0Fu;
    msg.mHavePDPAddress = m_havePDPAddress;
    msg.mPDPAddress = m_pdpAddress;
    msg.mHavePCO = m_havePCO;
    msg.mPCO = m_pco;
    msg.mAdditionalIes = m_additionalIes;
    return msg;
}

L3ActivatePDPContextAccept::Builder L3ActivatePDPContextAccept::builder() {
    return Builder{};
}

// ── L3ActivatePDPContextReject (TS 44.068 section 9.5) ────────────────

size_t L3ActivatePDPContextReject::bodyLength() const {
    size_t len = 1; // smCause value octet
    len += mAdditionalIes.size();
    return len;
}

Expected<L3ActivatePDPContextReject> L3ActivatePDPContextReject::parse(BitReader& br) {
    // SM ACTIVATE PDP CONTEXT REJECT (TS 44.068): the body starts with the
    // SM cause value octet (no identifier); any further optional IEs
    // (protocol configuration options, back-off timer, re-attempt indicator)
    // are kept opaque.
    L3ActivatePDPContextReject msg;

    auto c = br.readField(8);
    if (!c) return Expected<L3ActivatePDPContextReject>::error(c.error());
    msg.mCause = static_cast<SMCause>(c.value());

    if (!detail::readOpaqueTail(br, msg.mAdditionalIes)) {
        return Expected<L3ActivatePDPContextReject>::error(
            ParseError{ParseError::Code::TruncatedInput, "activate PDP context reject: truncated optional IEs", br.position()});
    }

    return Expected<L3ActivatePDPContextReject>::hold(std::move(msg));
}

void L3ActivatePDPContextReject::write(BitWriter& bw) const {
    // smCause: value octet (no identifier).
    bw.writeField(static_cast<uint8_t>(mCause), 8);
    detail::writeOpaqueTail(mAdditionalIes, bw);
}

void L3ActivatePDPContextReject::text(std::ostream& os) const {
    os << "ActivatePDPRej(cause=" << SMCause2Str(mCause) << ")";
}

L3ActivatePDPContextReject L3ActivatePDPContextReject::Builder::build() const {
    L3ActivatePDPContextReject msg;
    msg.mCause = m_cause;
    msg.mAdditionalIes = m_additionalIes;
    return msg;
}

L3ActivatePDPContextReject::Builder L3ActivatePDPContextReject::builder() {
    return Builder{};
}

// ── L3DeactivatePDPContextRequest (TS 44.068 section 9.5) ─────────────

size_t L3DeactivatePDPContextRequest::bodyLength() const {
    size_t len = 1; // smCause value octet
    if (mHasTearDownIndicator) len += 1; // tear-down indicator TV octet
    if (mHavePCO) len += tlvLen(mPCO.lengthV());
    len += mAdditionalIes.size();
    return len;
}

Expected<L3DeactivatePDPContextRequest> L3DeactivatePDPContextRequest::parse(BitReader& br) {
    // SM DEACTIVATE PDP CONTEXT REQUEST (TS 44.068): the body starts with
    // the SM cause value octet (no identifier); an optional tear-down
    // indicator TV octet ('1001'B + [flag(1)|spare(3)]), an optional
    // protocol configuration options TLV (IEI 0x27) and any further
    // optional IEs (MBMS PCO, T3396 timer value, WLAN offload indication)
    // follow.
    L3DeactivatePDPContextRequest msg;

    auto c = br.readField(8);
    if (!c) return Expected<L3DeactivatePDPContextRequest>::error(c.error());
    msg.mCause = static_cast<SMCause>(c.value());

    while (br.remainingBits() >= 8) {
        uint32_t next = br.peekField(8);
        if ((next & 0xF0u) == (static_cast<uint32_t>(L3TearDownIndicator::IEI) << 4)) {
            auto tdi = L3TearDownIndicator::parse(br);
            if (!tdi) return Expected<L3DeactivatePDPContextRequest>::error(tdi.error());
            msg.mTearDownIndicator = std::move(tdi).value();
            msg.mHasTearDownIndicator = true;
            continue;
        }
        if (br.remainingBits() < 16) break; // lone octet without a length: malformed
        size_t vLen = 0;
        auto iei = readTLVHeader(br, vLen);
        if (!iei) return Expected<L3DeactivatePDPContextRequest>::error(iei.error());
        bool typed = false;
        if (iei.value() == L3ProtocolConfigOptions::IEI && !msg.mHavePCO) {
            if (auto pco = L3ProtocolConfigOptions::parse(br, vLen)) {
                msg.mHavePCO = true;
                msg.mPCO = std::move(pco).value();
                typed = true;
            }
        }
        if (!typed) {
            auto t = appendTlvToTail(br, msg.mAdditionalIes, iei.value(), vLen);
            if (!t) return Expected<L3DeactivatePDPContextRequest>::error(t.error());
        }
    }
    if (!detail::readOpaqueTail(br, msg.mAdditionalIes)) {
        return Expected<L3DeactivatePDPContextRequest>::error(
            ParseError{ParseError::Code::TruncatedInput, "deactivate PDP context request: truncated optional IEs", br.position()});
    }

    return Expected<L3DeactivatePDPContextRequest>::hold(std::move(msg));
}

void L3DeactivatePDPContextRequest::write(BitWriter& bw) const {
    // smCause: value octet (no identifier).
    bw.writeField(static_cast<uint8_t>(mCause), 8);

    // Tear-down indicator: TV in one octet (optional).
    if (mHasTearDownIndicator) {
        mTearDownIndicator.write(bw);
    }

    // Protocol configuration options: TLV 0x27 (optional).
    if (mHavePCO) {
        bw.writeField(L3ProtocolConfigOptions::IEI, 8);
        bw.writeField(static_cast<uint32_t>(mPCO.lengthV()), 8);
        mPCO.write(bw);
    }

    detail::writeOpaqueTail(mAdditionalIes, bw);
}

void L3DeactivatePDPContextRequest::text(std::ostream& os) const {
    os << "DeactivatePDPReq(cause=" << SMCause2Str(mCause);
    if (mHasTearDownIndicator) {
        os << ",tearDown=" << (mTearDownIndicator.flag() ? "true" : "false");
    }
    if (mHavePCO) {
        os << ", ";
        mPCO.text(os);
    }
    os << ")";
}

L3DeactivatePDPContextRequest L3DeactivatePDPContextRequest::Builder::build() const {
    L3DeactivatePDPContextRequest msg;
    msg.mCause = m_cause;
    msg.mHasTearDownIndicator = m_hasTearDownIndicator;
    msg.mTearDownIndicator = m_tearDownIndicator;
    msg.mHavePCO = m_havePCO;
    msg.mPCO = m_pco;
    msg.mAdditionalIes = m_additionalIes;
    return msg;
}

L3DeactivatePDPContextRequest::Builder L3DeactivatePDPContextRequest::builder() {
    return Builder{};
}

// ── L3DeactivatePDPContextAccept (TS 44.068 section 9.5) ──────────────

Expected<L3DeactivatePDPContextAccept> L3DeactivatePDPContextAccept::parse(BitReader& br) {
    // SM DEACTIVATE PDP CONTEXT ACCEPT (TS 44.068): carries no fixed body
    // fields; optional IEs (protocol configuration options and others) are
    // kept as an opaque sequence.
    L3DeactivatePDPContextAccept msg;

    if (!detail::readOpaqueTail(br, msg.mAdditionalIes)) {
        return Expected<L3DeactivatePDPContextAccept>::error(
            ParseError{ParseError::Code::TruncatedInput, "deactivate PDP context accept: truncated optional IEs", br.position()});
    }

    return Expected<L3DeactivatePDPContextAccept>::hold(std::move(msg));
}

void L3DeactivatePDPContextAccept::write(BitWriter& bw) const {
    detail::writeOpaqueTail(mAdditionalIes, bw);
}

void L3DeactivatePDPContextAccept::text(std::ostream& os) const {
    os << "DeactivatePDPAcc()";
}

L3DeactivatePDPContextAccept L3DeactivatePDPContextAccept::Builder::build() const {
    L3DeactivatePDPContextAccept msg;
    msg.mAdditionalIes = m_additionalIes;
    return msg;
}

L3DeactivatePDPContextAccept::Builder L3DeactivatePDPContextAccept::builder() {
    return Builder{};
}

// ── L3ModifyPDPContextRequest (TS 44.068 section 9.5) ─────────────────

size_t L3ModifyPDPContextRequest::bodyLength() const {
    size_t len = 1; // pdpHandle(4)|spare(4)
    len += 1 + mQoS.lengthV(); // QoS (LV)
    if (mHavePCO) len += tlvLen(mPCO.lengthV());
    len += mAdditionalIes.size();
    return len;
}

Expected<L3ModifyPDPContextRequest> L3ModifyPDPContextRequest::parse(BitReader& br) {
    // SM MODIFY PDP CONTEXT REQUEST (TS 44.068): PDP handle in the first
    // octet, requested QoS (LV); optional IEs follow (protocol configuration
    // options TLV 0x27 and others, kept opaque).
    L3ModifyPDPContextRequest msg;

    auto o = br.readField(8);
    if (!o) return Expected<L3ModifyPDPContextRequest>::error(o.error());
    msg.mPDPHandle = static_cast<uint8_t>((o.value() >> 4) & 0x0Fu);

    // QoS: LV (mandatory).
    auto qlen = br.readField(8);
    if (!qlen) return Expected<L3ModifyPDPContextRequest>::error(qlen.error());
    {
        auto qos = L3QoS::parse(br, qlen.value());
        if (!qos) return Expected<L3ModifyPDPContextRequest>::error(qos.error());
        msg.mQoS = std::move(qos).value();
    }

    while (br.remainingBits() >= 16) {
        size_t vLen = 0;
        auto iei = readTLVHeader(br, vLen);
        if (!iei) return Expected<L3ModifyPDPContextRequest>::error(iei.error());
        bool typed = false;
        if (iei.value() == L3ProtocolConfigOptions::IEI && !msg.mHavePCO) {
            if (auto pco = L3ProtocolConfigOptions::parse(br, vLen)) {
                msg.mHavePCO = true;
                msg.mPCO = std::move(pco).value();
                typed = true;
            }
        }
        if (!typed) {
            auto t = appendTlvToTail(br, msg.mAdditionalIes, iei.value(), vLen);
            if (!t) return Expected<L3ModifyPDPContextRequest>::error(t.error());
        }
    }
    if (!detail::readOpaqueTail(br, msg.mAdditionalIes)) {
        return Expected<L3ModifyPDPContextRequest>::error(
            ParseError{ParseError::Code::TruncatedInput, "modify PDP context request: truncated optional IEs", br.position()});
    }

    return Expected<L3ModifyPDPContextRequest>::hold(std::move(msg));
}

void L3ModifyPDPContextRequest::write(BitWriter& bw) const {
    // pdpHandle(4)|spare(4).
    bw.writeField((static_cast<uint32_t>(mPDPHandle) & 0x0Fu) << 4, 8);

    // QoS: LV (mandatory).
    bw.writeField(static_cast<uint32_t>(mQoS.lengthV()), 8);
    mQoS.write(bw);

    // PCO: TLV 0x27 (optional).
    if (mHavePCO) {
        bw.writeField(L3ProtocolConfigOptions::IEI, 8);
        bw.writeField(static_cast<uint32_t>(mPCO.lengthV()), 8);
        mPCO.write(bw);
    }

    detail::writeOpaqueTail(mAdditionalIes, bw);
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

L3ModifyPDPContextRequest L3ModifyPDPContextRequest::Builder::build() const {
    L3ModifyPDPContextRequest msg;
    msg.mPDPHandle = m_pdpHandle & 0x0Fu;
    msg.mQoS = m_qos;
    msg.mHavePCO = m_havePCO;
    msg.mPCO = m_pco;
    msg.mAdditionalIes = m_additionalIes;
    return msg;
}

L3ModifyPDPContextRequest::Builder L3ModifyPDPContextRequest::builder() {
    return Builder{};
}

// ── L3ModifyPDPContextAccept (TS 44.068 section 9.5) ──────────────────

size_t L3ModifyPDPContextAccept::bodyLength() const {
    size_t len = 1; // pdpHandle(4)|spare(4)
    len += 1 + mQoS.lengthV(); // QoS (LV)
    if (mHavePCO) len += tlvLen(mPCO.lengthV());
    len += mAdditionalIes.size();
    return len;
}

Expected<L3ModifyPDPContextAccept> L3ModifyPDPContextAccept::parse(BitReader& br) {
    // SM MODIFY PDP CONTEXT ACCEPT (TS 44.068): PDP handle in the first
    // octet, QoS (LV); optional IEs follow (kept opaque except PCO TLV 0x27).
    L3ModifyPDPContextAccept msg;

    auto o = br.readField(8);
    if (!o) return Expected<L3ModifyPDPContextAccept>::error(o.error());
    msg.mPDPHandle = static_cast<uint8_t>((o.value() >> 4) & 0x0Fu);

    // QoS: LV (mandatory).
    auto qlen = br.readField(8);
    if (!qlen) return Expected<L3ModifyPDPContextAccept>::error(qlen.error());
    {
        auto qos = L3QoS::parse(br, qlen.value());
        if (!qos) return Expected<L3ModifyPDPContextAccept>::error(qos.error());
        msg.mQoS = std::move(qos).value();
    }

    while (br.remainingBits() >= 16) {
        size_t vLen = 0;
        auto iei = readTLVHeader(br, vLen);
        if (!iei) return Expected<L3ModifyPDPContextAccept>::error(iei.error());
        bool typed = false;
        if (iei.value() == L3ProtocolConfigOptions::IEI && !msg.mHavePCO) {
            if (auto pco = L3ProtocolConfigOptions::parse(br, vLen)) {
                msg.mHavePCO = true;
                msg.mPCO = std::move(pco).value();
                typed = true;
            }
        }
        if (!typed) {
            auto t = appendTlvToTail(br, msg.mAdditionalIes, iei.value(), vLen);
            if (!t) return Expected<L3ModifyPDPContextAccept>::error(t.error());
        }
    }
    if (!detail::readOpaqueTail(br, msg.mAdditionalIes)) {
        return Expected<L3ModifyPDPContextAccept>::error(
            ParseError{ParseError::Code::TruncatedInput, "modify PDP context accept: truncated optional IEs", br.position()});
    }

    return Expected<L3ModifyPDPContextAccept>::hold(std::move(msg));
}

void L3ModifyPDPContextAccept::write(BitWriter& bw) const {
    // pdpHandle(4)|spare(4).
    bw.writeField((static_cast<uint32_t>(mPDPHandle) & 0x0Fu) << 4, 8);

    // QoS: LV (mandatory).
    bw.writeField(static_cast<uint32_t>(mQoS.lengthV()), 8);
    mQoS.write(bw);

    // PCO: TLV 0x27 (optional).
    if (mHavePCO) {
        bw.writeField(L3ProtocolConfigOptions::IEI, 8);
        bw.writeField(static_cast<uint32_t>(mPCO.lengthV()), 8);
        mPCO.write(bw);
    }

    detail::writeOpaqueTail(mAdditionalIes, bw);
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

L3ModifyPDPContextAccept L3ModifyPDPContextAccept::Builder::build() const {
    L3ModifyPDPContextAccept msg;
    msg.mPDPHandle = m_pdpHandle & 0x0Fu;
    msg.mQoS = m_qos;
    msg.mHavePCO = m_havePCO;
    msg.mPCO = m_pco;
    msg.mAdditionalIes = m_additionalIes;
    return msg;
}

L3ModifyPDPContextAccept::Builder L3ModifyPDPContextAccept::builder() {
    return Builder{};
}

// ── L3ModifyPDPContextReject (TS 44.068 section 9.5) ──────────────────

size_t L3ModifyPDPContextReject::bodyLength() const {
    size_t len = 1; // smCause value octet
    len += mAdditionalIes.size();
    return len;
}

Expected<L3ModifyPDPContextReject> L3ModifyPDPContextReject::parse(BitReader& br) {
    // SM MODIFY PDP CONTEXT REJECT (TS 44.068): the body starts with the
    // SM cause value octet (no identifier); any further optional IEs
    // (back-off timer and others) are kept opaque.
    L3ModifyPDPContextReject msg;

    auto c = br.readField(8);
    if (!c) return Expected<L3ModifyPDPContextReject>::error(c.error());
    msg.mCause = static_cast<SMCause>(c.value());

    if (!detail::readOpaqueTail(br, msg.mAdditionalIes)) {
        return Expected<L3ModifyPDPContextReject>::error(
            ParseError{ParseError::Code::TruncatedInput, "modify PDP context reject: truncated optional IEs", br.position()});
    }

    return Expected<L3ModifyPDPContextReject>::hold(std::move(msg));
}

void L3ModifyPDPContextReject::write(BitWriter& bw) const {
    // smCause: value octet (no identifier).
    bw.writeField(static_cast<uint8_t>(mCause), 8);
    detail::writeOpaqueTail(mAdditionalIes, bw);
}

void L3ModifyPDPContextReject::text(std::ostream& os) const {
    os << "ModifyPDPRej(cause=" << SMCause2Str(mCause) << ")";
}

L3ModifyPDPContextReject L3ModifyPDPContextReject::Builder::build() const {
    L3ModifyPDPContextReject msg;
    msg.mCause = m_cause;
    msg.mAdditionalIes = m_additionalIes;
    return msg;
}

L3ModifyPDPContextReject::Builder L3ModifyPDPContextReject::builder() {
    return Builder{};
}

// ── L3SMStatus (TS 44.068 section 9.5) ────────────────────────────────

size_t L3SMStatus::bodyLength() const {
    size_t len = 1; // smCause value octet
    len += mAdditionalIes.size();
    return len;
}

Expected<L3SMStatus> L3SMStatus::parse(BitReader& br) {
    // SM STATUS (TS 44.068): the body is the SM cause value octet without
    // an identifier; any further optional IEs are kept opaque.
    L3SMStatus msg;

    auto c = br.readField(8);
    if (!c) return Expected<L3SMStatus>::error(c.error());
    msg.mCause = static_cast<SMCause>(c.value());

    if (!detail::readOpaqueTail(br, msg.mAdditionalIes)) {
        return Expected<L3SMStatus>::error(
            ParseError{ParseError::Code::TruncatedInput, "SM status: truncated optional IEs", br.position()});
    }

    return Expected<L3SMStatus>::hold(std::move(msg));
}

void L3SMStatus::write(BitWriter& bw) const {
    // smCause: value octet (no identifier).
    bw.writeField(static_cast<uint8_t>(mCause), 8);
    detail::writeOpaqueTail(mAdditionalIes, bw);
}

void L3SMStatus::text(std::ostream& os) const {
    os << "SMStatus(cause=" << SMCause2Str(mCause) << ")";
}

L3SMStatus L3SMStatus::Builder::build() const {
    L3SMStatus msg;
    msg.mCause = m_cause;
    msg.mAdditionalIes = m_additionalIes;
    return msg;
}

L3SMStatus::Builder L3SMStatus::builder() {
    return Builder{};
}

// ── L3RequestPDPContextActivation (TS 44.068 section 9.5) ─────────────

size_t L3RequestPDPContextActivation::bodyLength() const {
    size_t len = 1; // pdpHandle(4)|spare(4)
    len += 1 + mQoS.lengthV();         // QoS (LV)
    len += 1 + mPDPAddress.lengthV();  // PDP address (LV)
    len += tlvLen(mAPN.lengthV());     // APN (TLV, mandatory)
    if (mHavePCO) len += tlvLen(mPCO.lengthV());
    len += mAdditionalIes.size();
    return len;
}

Expected<L3RequestPDPContextActivation> L3RequestPDPContextActivation::parse(BitReader& br) {
    // SM REQUEST PDP CONTEXT ACTIVATION (TS 44.068): PDP handle in the first
    // octet, QoS (LV), PDP type and address (LV), access point name (TLV,
    // IEI 0x28); protocol configuration options (TLV, IEI 0x27) and any
    // further optional IEs follow.
    L3RequestPDPContextActivation msg;

    auto o = br.readField(8);
    if (!o) return Expected<L3RequestPDPContextActivation>::error(o.error());
    msg.mPDPHandle = static_cast<uint8_t>((o.value() >> 4) & 0x0Fu);

    // QoS: LV (mandatory).
    auto qlen = br.readField(8);
    if (!qlen) return Expected<L3RequestPDPContextActivation>::error(qlen.error());
    {
        auto qos = L3QoS::parse(br, qlen.value());
        if (!qos) return Expected<L3RequestPDPContextActivation>::error(qos.error());
        msg.mQoS = std::move(qos).value();
    }

    // PDP type and address: LV (mandatory).
    auto alen = br.readField(8);
    if (!alen) return Expected<L3RequestPDPContextActivation>::error(alen.error());
    {
        auto addr = L3PDPAddress::parse(br, alen.value());
        if (!addr) return Expected<L3RequestPDPContextActivation>::error(addr.error());
        msg.mPDPAddress = std::move(addr).value();
    }

    // APN: TLV 0x28 (mandatory).
    {
        size_t vLen = 0;
        auto iei = readTLVHeader(br, vLen);
        if (!iei) return Expected<L3RequestPDPContextActivation>::error(iei.error());
        if (iei.value() != L3AccessPointName::IEI) {
            return Expected<L3RequestPDPContextActivation>::error(
                ParseError{ParseError::Code::InvalidIE, "request PDP context activation: missing access point name", br.position()});
        }
        auto apn = L3AccessPointName::parse(br, vLen);
        if (!apn) return Expected<L3RequestPDPContextActivation>::error(apn.error());
        msg.mAPN = std::move(apn).value();
    }

    while (br.remainingBits() >= 16) {
        size_t vLen = 0;
        auto iei = readTLVHeader(br, vLen);
        if (!iei) return Expected<L3RequestPDPContextActivation>::error(iei.error());
        bool typed = false;
        if (iei.value() == L3ProtocolConfigOptions::IEI && !msg.mHavePCO) {
            if (auto pco = L3ProtocolConfigOptions::parse(br, vLen)) {
                msg.mHavePCO = true;
                msg.mPCO = std::move(pco).value();
                typed = true;
            }
        }
        if (!typed) {
            auto t = appendTlvToTail(br, msg.mAdditionalIes, iei.value(), vLen);
            if (!t) return Expected<L3RequestPDPContextActivation>::error(t.error());
        }
    }
    if (!detail::readOpaqueTail(br, msg.mAdditionalIes)) {
        return Expected<L3RequestPDPContextActivation>::error(
            ParseError{ParseError::Code::TruncatedInput, "request PDP context activation: truncated optional IEs", br.position()});
    }

    return Expected<L3RequestPDPContextActivation>::hold(std::move(msg));
}

void L3RequestPDPContextActivation::write(BitWriter& bw) const {
    // pdpHandle(4)|spare(4).
    bw.writeField((static_cast<uint32_t>(mPDPHandle) & 0x0Fu) << 4, 8);

    // QoS: LV (mandatory).
    bw.writeField(static_cast<uint32_t>(mQoS.lengthV()), 8);
    mQoS.write(bw);

    // PDP type and address: LV (mandatory).
    bw.writeField(static_cast<uint32_t>(mPDPAddress.lengthV()), 8);
    mPDPAddress.write(bw);

    // APN: TLV 0x28 (mandatory).
    bw.writeField(L3AccessPointName::IEI, 8);
    bw.writeField(static_cast<uint32_t>(mAPN.lengthV()), 8);
    mAPN.write(bw);

    // PCO: TLV 0x27 (optional).
    if (mHavePCO) {
        bw.writeField(L3ProtocolConfigOptions::IEI, 8);
        bw.writeField(static_cast<uint32_t>(mPCO.lengthV()), 8);
        mPCO.write(bw);
    }

    detail::writeOpaqueTail(mAdditionalIes, bw);
}

void L3RequestPDPContextActivation::text(std::ostream& os) const {
    os << "RequestPDPAct(handle=" << static_cast<int>(mPDPHandle);
    os << ", "; mQoS.text(os);
    os << ", "; mPDPAddress.text(os);
    os << ", "; mAPN.text(os);
    if (mHavePCO) { os << ", "; mPCO.text(os); }
    os << ")";
}

L3RequestPDPContextActivation L3RequestPDPContextActivation::Builder::build() const {
    L3RequestPDPContextActivation msg;
    msg.mPDPHandle = m_pdpHandle & 0x0Fu;
    msg.mQoS = m_qos;
    msg.mPDPAddress = m_pdpAddress;
    msg.mAPN = m_apn;
    msg.mHavePCO = m_havePCO;
    msg.mPCO = m_pco;
    msg.mAdditionalIes = m_additionalIes;
    return msg;
}

L3RequestPDPContextActivation::Builder L3RequestPDPContextActivation::builder() {
    return Builder{};
}

// ── L3RequestPDPContextActivationReject (TS 44.068 section 9.5) ───────

size_t L3RequestPDPContextActivationReject::bodyLength() const {
    size_t len = 1; // smCause value octet
    len += mAdditionalIes.size();
    return len;
}

Expected<L3RequestPDPContextActivationReject> L3RequestPDPContextActivationReject::parse(BitReader& br) {
    // SM REQUEST PDP CONTEXT ACTIVATION REJECT (TS 44.068): the body starts
    // with the SM cause value octet (no identifier); any further optional
    // IEs are kept opaque.
    L3RequestPDPContextActivationReject msg;

    auto c = br.readField(8);
    if (!c) return Expected<L3RequestPDPContextActivationReject>::error(c.error());
    msg.mCause = static_cast<SMCause>(c.value());

    if (!detail::readOpaqueTail(br, msg.mAdditionalIes)) {
        return Expected<L3RequestPDPContextActivationReject>::error(
            ParseError{ParseError::Code::TruncatedInput, "request PDP context activation reject: truncated optional IEs", br.position()});
    }

    return Expected<L3RequestPDPContextActivationReject>::hold(std::move(msg));
}

void L3RequestPDPContextActivationReject::write(BitWriter& bw) const {
    // smCause: value octet (no identifier).
    bw.writeField(static_cast<uint8_t>(mCause), 8);
    detail::writeOpaqueTail(mAdditionalIes, bw);
}

void L3RequestPDPContextActivationReject::text(std::ostream& os) const {
    os << "RequestPDPActRej(cause=" << SMCause2Str(mCause) << ")";
}

L3RequestPDPContextActivationReject L3RequestPDPContextActivationReject::Builder::build() const {
    L3RequestPDPContextActivationReject msg;
    msg.mCause = m_cause;
    msg.mAdditionalIes = m_additionalIes;
    return msg;
}

L3RequestPDPContextActivationReject::Builder L3RequestPDPContextActivationReject::builder() {
    return Builder{};
}

// ── L3ModifyPDPContextRequestMS (TS 44.068 section 9.5) ───────────────

size_t L3ModifyPDPContextRequestMS::bodyLength() const {
    size_t len = 1; // pdpHandle(4)|spare(4)
    len += 1 + mQoS.lengthV(); // QoS (LV)
    if (mHavePCO) len += tlvLen(mPCO.lengthV());
    len += mAdditionalIes.size();
    return len;
}

Expected<L3ModifyPDPContextRequestMS> L3ModifyPDPContextRequestMS::parse(BitReader& br) {
    // SM MODIFY PDP CONTEXT REQUEST, MS->SGSN (TS 44.068): PDP handle in
    // the first octet, requested QoS (LV); optional IEs follow (kept opaque
    // except PCO TLV 0x27).
    L3ModifyPDPContextRequestMS msg;

    auto o = br.readField(8);
    if (!o) return Expected<L3ModifyPDPContextRequestMS>::error(o.error());
    msg.mPDPHandle = static_cast<uint8_t>((o.value() >> 4) & 0x0Fu);

    // QoS: LV (mandatory).
    auto qlen = br.readField(8);
    if (!qlen) return Expected<L3ModifyPDPContextRequestMS>::error(qlen.error());
    {
        auto qos = L3QoS::parse(br, qlen.value());
        if (!qos) return Expected<L3ModifyPDPContextRequestMS>::error(qos.error());
        msg.mQoS = std::move(qos).value();
    }

    while (br.remainingBits() >= 16) {
        size_t vLen = 0;
        auto iei = readTLVHeader(br, vLen);
        if (!iei) return Expected<L3ModifyPDPContextRequestMS>::error(iei.error());
        bool typed = false;
        if (iei.value() == L3ProtocolConfigOptions::IEI && !msg.mHavePCO) {
            if (auto pco = L3ProtocolConfigOptions::parse(br, vLen)) {
                msg.mHavePCO = true;
                msg.mPCO = std::move(pco).value();
                typed = true;
            }
        }
        if (!typed) {
            auto t = appendTlvToTail(br, msg.mAdditionalIes, iei.value(), vLen);
            if (!t) return Expected<L3ModifyPDPContextRequestMS>::error(t.error());
        }
    }
    if (!detail::readOpaqueTail(br, msg.mAdditionalIes)) {
        return Expected<L3ModifyPDPContextRequestMS>::error(
            ParseError{ParseError::Code::TruncatedInput, "modify PDP context request (MS): truncated optional IEs", br.position()});
    }

    return Expected<L3ModifyPDPContextRequestMS>::hold(std::move(msg));
}

void L3ModifyPDPContextRequestMS::write(BitWriter& bw) const {
    // pdpHandle(4)|spare(4).
    bw.writeField((static_cast<uint32_t>(mPDPHandle) & 0x0Fu) << 4, 8);

    // QoS: LV (mandatory).
    bw.writeField(static_cast<uint32_t>(mQoS.lengthV()), 8);
    mQoS.write(bw);

    // PCO: TLV 0x27 (optional).
    if (mHavePCO) {
        bw.writeField(L3ProtocolConfigOptions::IEI, 8);
        bw.writeField(static_cast<uint32_t>(mPCO.lengthV()), 8);
        mPCO.write(bw);
    }

    detail::writeOpaqueTail(mAdditionalIes, bw);
}

void L3ModifyPDPContextRequestMS::text(std::ostream& os) const {
    os << "ModifyPDPReqMS(handle=" << static_cast<int>(mPDPHandle);
    os << ", "; mQoS.text(os);
    if (mHavePCO) { os << ", "; mPCO.text(os); }
    os << ")";
}

L3ModifyPDPContextRequestMS L3ModifyPDPContextRequestMS::Builder::build() const {
    L3ModifyPDPContextRequestMS msg;
    msg.mPDPHandle = m_pdpHandle & 0x0Fu;
    msg.mQoS = m_qos;
    msg.mHavePCO = m_havePCO;
    msg.mPCO = m_pco;
    msg.mAdditionalIes = m_additionalIes;
    return msg;
}

L3ModifyPDPContextRequestMS::Builder L3ModifyPDPContextRequestMS::builder() {
    return Builder{};
}

// ── L3ModifyPDPContextAcceptNet (TS 44.068 section 9.5) ───────────────

size_t L3ModifyPDPContextAcceptNet::bodyLength() const {
    size_t len = 1; // pdpHandle(4)|spare(4)
    len += 1 + mQoS.lengthV(); // QoS (LV)
    if (mHavePCO) len += tlvLen(mPCO.lengthV());
    len += mAdditionalIes.size();
    return len;
}

Expected<L3ModifyPDPContextAcceptNet> L3ModifyPDPContextAcceptNet::parse(BitReader& br) {
    // SM MODIFY PDP CONTEXT ACCEPT, SGSN->MS (TS 44.068): PDP handle in
    // the first octet, QoS (LV); optional IEs follow (kept opaque except
    // PCO TLV 0x27).
    L3ModifyPDPContextAcceptNet msg;

    auto o = br.readField(8);
    if (!o) return Expected<L3ModifyPDPContextAcceptNet>::error(o.error());
    msg.mPDPHandle = static_cast<uint8_t>((o.value() >> 4) & 0x0Fu);

    // QoS: LV (mandatory).
    auto qlen = br.readField(8);
    if (!qlen) return Expected<L3ModifyPDPContextAcceptNet>::error(qlen.error());
    {
        auto qos = L3QoS::parse(br, qlen.value());
        if (!qos) return Expected<L3ModifyPDPContextAcceptNet>::error(qos.error());
        msg.mQoS = std::move(qos).value();
    }

    while (br.remainingBits() >= 16) {
        size_t vLen = 0;
        auto iei = readTLVHeader(br, vLen);
        if (!iei) return Expected<L3ModifyPDPContextAcceptNet>::error(iei.error());
        bool typed = false;
        if (iei.value() == L3ProtocolConfigOptions::IEI && !msg.mHavePCO) {
            if (auto pco = L3ProtocolConfigOptions::parse(br, vLen)) {
                msg.mHavePCO = true;
                msg.mPCO = std::move(pco).value();
                typed = true;
            }
        }
        if (!typed) {
            auto t = appendTlvToTail(br, msg.mAdditionalIes, iei.value(), vLen);
            if (!t) return Expected<L3ModifyPDPContextAcceptNet>::error(t.error());
        }
    }
    if (!detail::readOpaqueTail(br, msg.mAdditionalIes)) {
        return Expected<L3ModifyPDPContextAcceptNet>::error(
            ParseError{ParseError::Code::TruncatedInput, "modify PDP context accept (SGSN): truncated optional IEs", br.position()});
    }

    return Expected<L3ModifyPDPContextAcceptNet>::hold(std::move(msg));
}

void L3ModifyPDPContextAcceptNet::write(BitWriter& bw) const {
    // pdpHandle(4)|spare(4).
    bw.writeField((static_cast<uint32_t>(mPDPHandle) & 0x0Fu) << 4, 8);

    // QoS: LV (mandatory).
    bw.writeField(static_cast<uint32_t>(mQoS.lengthV()), 8);
    mQoS.write(bw);

    // PCO: TLV 0x27 (optional).
    if (mHavePCO) {
        bw.writeField(L3ProtocolConfigOptions::IEI, 8);
        bw.writeField(static_cast<uint32_t>(mPCO.lengthV()), 8);
        mPCO.write(bw);
    }

    detail::writeOpaqueTail(mAdditionalIes, bw);
}

void L3ModifyPDPContextAcceptNet::text(std::ostream& os) const {
    os << "ModifyPDPAccNet(handle=" << static_cast<int>(mPDPHandle);
    os << ", "; mQoS.text(os);
    if (mHavePCO) { os << ", "; mPCO.text(os); }
    os << ")";
}

L3ModifyPDPContextAcceptNet L3ModifyPDPContextAcceptNet::Builder::build() const {
    L3ModifyPDPContextAcceptNet msg;
    msg.mPDPHandle = m_pdpHandle & 0x0Fu;
    msg.mQoS = m_qos;
    msg.mHavePCO = m_havePCO;
    msg.mPCO = m_pco;
    msg.mAdditionalIes = m_additionalIes;
    return msg;
}

L3ModifyPDPContextAcceptNet::Builder L3ModifyPDPContextAcceptNet::builder() {
    return Builder{};
}

// ── L3ActivateSecondaryPDPContextRequest (TS 44.068 section 9.5) ──────

size_t L3ActivateSecondaryPDPContextRequest::bodyLength() const {
    size_t len = 1; // pdpHandle(4)|spare(4)
    len += 1 + mQoS.lengthV();         // QoS (LV)
    len += 1 + mPDPAddress.lengthV();  // PDP address (LV)
    len += tlvLen(mAPN.lengthV());     // APN (TLV, mandatory)
    if (mHavePCO) len += tlvLen(mPCO.lengthV());
    len += mAdditionalIes.size();
    return len;
}

Expected<L3ActivateSecondaryPDPContextRequest> L3ActivateSecondaryPDPContextRequest::parse(BitReader& br) {
    // SM ACTIVATE SECONDARY PDP CONTEXT REQUEST (TS 44.068): PDP handle in
    // the first octet, requested QoS (LV), requested PDP type and address
    // (LV), access point name (TLV, IEI 0x28); protocol configuration
    // options (TLV, IEI 0x27) and any further optional IEs follow.
    L3ActivateSecondaryPDPContextRequest msg;

    auto o = br.readField(8);
    if (!o) return Expected<L3ActivateSecondaryPDPContextRequest>::error(o.error());
    msg.mPDPHandle = static_cast<uint8_t>((o.value() >> 4) & 0x0Fu);

    // QoS: LV (mandatory).
    auto qlen = br.readField(8);
    if (!qlen) return Expected<L3ActivateSecondaryPDPContextRequest>::error(qlen.error());
    {
        auto qos = L3QoS::parse(br, qlen.value());
        if (!qos) return Expected<L3ActivateSecondaryPDPContextRequest>::error(qos.error());
        msg.mQoS = std::move(qos).value();
    }

    // PDP type and address: LV (mandatory).
    auto alen = br.readField(8);
    if (!alen) return Expected<L3ActivateSecondaryPDPContextRequest>::error(alen.error());
    {
        auto addr = L3PDPAddress::parse(br, alen.value());
        if (!addr) return Expected<L3ActivateSecondaryPDPContextRequest>::error(addr.error());
        msg.mPDPAddress = std::move(addr).value();
    }

    // APN: TLV 0x28 (mandatory).
    {
        size_t vLen = 0;
        auto iei = readTLVHeader(br, vLen);
        if (!iei) return Expected<L3ActivateSecondaryPDPContextRequest>::error(iei.error());
        if (iei.value() != L3AccessPointName::IEI) {
            return Expected<L3ActivateSecondaryPDPContextRequest>::error(
                ParseError{ParseError::Code::InvalidIE, "activate secondary PDP context request: missing access point name", br.position()});
        }
        auto apn = L3AccessPointName::parse(br, vLen);
        if (!apn) return Expected<L3ActivateSecondaryPDPContextRequest>::error(apn.error());
        msg.mAPN = std::move(apn).value();
    }

    while (br.remainingBits() >= 16) {
        size_t vLen = 0;
        auto iei = readTLVHeader(br, vLen);
        if (!iei) return Expected<L3ActivateSecondaryPDPContextRequest>::error(iei.error());
        bool typed = false;
        if (iei.value() == L3ProtocolConfigOptions::IEI && !msg.mHavePCO) {
            if (auto pco = L3ProtocolConfigOptions::parse(br, vLen)) {
                msg.mHavePCO = true;
                msg.mPCO = std::move(pco).value();
                typed = true;
            }
        }
        if (!typed) {
            auto t = appendTlvToTail(br, msg.mAdditionalIes, iei.value(), vLen);
            if (!t) return Expected<L3ActivateSecondaryPDPContextRequest>::error(t.error());
        }
    }
    if (!detail::readOpaqueTail(br, msg.mAdditionalIes)) {
        return Expected<L3ActivateSecondaryPDPContextRequest>::error(
            ParseError{ParseError::Code::TruncatedInput, "activate secondary PDP context request: truncated optional IEs", br.position()});
    }

    return Expected<L3ActivateSecondaryPDPContextRequest>::hold(std::move(msg));
}

void L3ActivateSecondaryPDPContextRequest::write(BitWriter& bw) const {
    // pdpHandle(4)|spare(4).
    bw.writeField((static_cast<uint32_t>(mPDPHandle) & 0x0Fu) << 4, 8);

    // QoS: LV (mandatory).
    bw.writeField(static_cast<uint32_t>(mQoS.lengthV()), 8);
    mQoS.write(bw);

    // PDP type and address: LV (mandatory).
    bw.writeField(static_cast<uint32_t>(mPDPAddress.lengthV()), 8);
    mPDPAddress.write(bw);

    // APN: TLV 0x28 (mandatory).
    bw.writeField(L3AccessPointName::IEI, 8);
    bw.writeField(static_cast<uint32_t>(mAPN.lengthV()), 8);
    mAPN.write(bw);

    // PCO: TLV 0x27 (optional).
    if (mHavePCO) {
        bw.writeField(L3ProtocolConfigOptions::IEI, 8);
        bw.writeField(static_cast<uint32_t>(mPCO.lengthV()), 8);
        mPCO.write(bw);
    }

    detail::writeOpaqueTail(mAdditionalIes, bw);
}

void L3ActivateSecondaryPDPContextRequest::text(std::ostream& os) const {
    os << "ActSecPDPReq(handle=" << static_cast<int>(mPDPHandle);
    os << ", "; mQoS.text(os);
    os << ", "; mPDPAddress.text(os);
    os << ", "; mAPN.text(os);
    if (mHavePCO) { os << ", "; mPCO.text(os); }
    os << ")";
}

L3ActivateSecondaryPDPContextRequest L3ActivateSecondaryPDPContextRequest::Builder::build() const {
    L3ActivateSecondaryPDPContextRequest msg;
    msg.mPDPHandle = m_pdpHandle & 0x0Fu;
    msg.mQoS = m_qos;
    msg.mPDPAddress = m_pdpAddress;
    msg.mAPN = m_apn;
    msg.mHavePCO = m_havePCO;
    msg.mPCO = m_pco;
    msg.mAdditionalIes = m_additionalIes;
    return msg;
}

L3ActivateSecondaryPDPContextRequest::Builder L3ActivateSecondaryPDPContextRequest::builder() {
    return Builder{};
}

// ── L3ActivateSecondaryPDPContextAccept (TS 44.068 section 9.5) ───────

size_t L3ActivateSecondaryPDPContextAccept::bodyLength() const {
    size_t len = 1; // pdpHandle(4)|spare(4)
    len += 1 + mQoS.lengthV(); // QoS (LV)
    if (mHavePDPAddress) len += tlvLen(mPDPAddress.lengthV());
    if (mHavePCO) len += tlvLen(mPCO.lengthV());
    len += mAdditionalIes.size();
    return len;
}

Expected<L3ActivateSecondaryPDPContextAccept> L3ActivateSecondaryPDPContextAccept::parse(BitReader& br) {
    // SM ACTIVATE SECONDARY PDP CONTEXT ACCEPT (TS 44.068): PDP handle in
    // the first octet, negotiated QoS (LV); optional IEs follow: PDP type
    // and address (TLV, IEI 0x2B), protocol configuration options (TLV,
    // IEI 0x27); any further optional IEs are kept opaque.
    L3ActivateSecondaryPDPContextAccept msg;

    auto o = br.readField(8);
    if (!o) return Expected<L3ActivateSecondaryPDPContextAccept>::error(o.error());
    msg.mPDPHandle = static_cast<uint8_t>((o.value() >> 4) & 0x0Fu);

    // QoS: LV (mandatory).
    auto qlen = br.readField(8);
    if (!qlen) return Expected<L3ActivateSecondaryPDPContextAccept>::error(qlen.error());
    {
        auto qos = L3QoS::parse(br, qlen.value());
        if (!qos) return Expected<L3ActivateSecondaryPDPContextAccept>::error(qos.error());
        msg.mQoS = std::move(qos).value();
    }

    while (br.remainingBits() >= 16) {
        size_t vLen = 0;
        auto iei = readTLVHeader(br, vLen);
        if (!iei) return Expected<L3ActivateSecondaryPDPContextAccept>::error(iei.error());
        bool typed = false;
        if (iei.value() == kPdpAddressTlvIEI && !msg.mHavePDPAddress) {
            if (auto addr = L3PDPAddress::parse(br, vLen)) {
                msg.mHavePDPAddress = true;
                msg.mPDPAddress = std::move(addr).value();
                typed = true;
            }
        } else if (iei.value() == L3ProtocolConfigOptions::IEI && !msg.mHavePCO) {
            if (auto pco = L3ProtocolConfigOptions::parse(br, vLen)) {
                msg.mHavePCO = true;
                msg.mPCO = std::move(pco).value();
                typed = true;
            }
        }
        if (!typed) {
            auto t = appendTlvToTail(br, msg.mAdditionalIes, iei.value(), vLen);
            if (!t) return Expected<L3ActivateSecondaryPDPContextAccept>::error(t.error());
        }
    }
    if (!detail::readOpaqueTail(br, msg.mAdditionalIes)) {
        return Expected<L3ActivateSecondaryPDPContextAccept>::error(
            ParseError{ParseError::Code::TruncatedInput, "activate secondary PDP context accept: truncated optional IEs", br.position()});
    }

    return Expected<L3ActivateSecondaryPDPContextAccept>::hold(std::move(msg));
}

void L3ActivateSecondaryPDPContextAccept::write(BitWriter& bw) const {
    // pdpHandle(4)|spare(4).
    bw.writeField((static_cast<uint32_t>(mPDPHandle) & 0x0Fu) << 4, 8);

    // QoS: LV (mandatory).
    bw.writeField(static_cast<uint32_t>(mQoS.lengthV()), 8);
    mQoS.write(bw);

    // PDP type and address: TLV 0x2B (optional).
    if (mHavePDPAddress) {
        bw.writeField(kPdpAddressTlvIEI, 8);
        bw.writeField(static_cast<uint32_t>(mPDPAddress.lengthV()), 8);
        mPDPAddress.write(bw);
    }

    // PCO: TLV 0x27 (optional).
    if (mHavePCO) {
        bw.writeField(L3ProtocolConfigOptions::IEI, 8);
        bw.writeField(static_cast<uint32_t>(mPCO.lengthV()), 8);
        mPCO.write(bw);
    }

    detail::writeOpaqueTail(mAdditionalIes, bw);
}

void L3ActivateSecondaryPDPContextAccept::text(std::ostream& os) const {
    os << "ActSecPDPAcc(handle=" << static_cast<int>(mPDPHandle);
    if (mHavePDPAddress) { os << ", "; mPDPAddress.text(os); }
    os << ", "; mQoS.text(os);
    if (mHavePCO) { os << ", "; mPCO.text(os); }
    os << ")";
}

L3ActivateSecondaryPDPContextAccept L3ActivateSecondaryPDPContextAccept::Builder::build() const {
    L3ActivateSecondaryPDPContextAccept msg;
    msg.mPDPHandle = m_pdpHandle & 0x0Fu;
    msg.mQoS = m_qos;
    msg.mHavePDPAddress = m_havePDPAddress;
    msg.mPDPAddress = m_pdpAddress;
    msg.mHavePCO = m_havePCO;
    msg.mPCO = m_pco;
    msg.mAdditionalIes = m_additionalIes;
    return msg;
}

L3ActivateSecondaryPDPContextAccept::Builder L3ActivateSecondaryPDPContextAccept::builder() {
    return Builder{};
}

// ── L3ActivateSecondaryPDPContextReject (TS 44.068 section 9.5) ───────

size_t L3ActivateSecondaryPDPContextReject::bodyLength() const {
    size_t len = 1; // smCause value octet
    len += mAdditionalIes.size();
    return len;
}

Expected<L3ActivateSecondaryPDPContextReject> L3ActivateSecondaryPDPContextReject::parse(BitReader& br) {
    // SM ACTIVATE SECONDARY PDP CONTEXT REJECT (TS 44.068): the body starts
    // with the SM cause value octet (no identifier); any further optional
    // IEs are kept opaque.
    L3ActivateSecondaryPDPContextReject msg;

    auto c = br.readField(8);
    if (!c) return Expected<L3ActivateSecondaryPDPContextReject>::error(c.error());
    msg.mCause = static_cast<SMCause>(c.value());

    if (!detail::readOpaqueTail(br, msg.mAdditionalIes)) {
        return Expected<L3ActivateSecondaryPDPContextReject>::error(
            ParseError{ParseError::Code::TruncatedInput, "activate secondary PDP context reject: truncated optional IEs", br.position()});
    }

    return Expected<L3ActivateSecondaryPDPContextReject>::hold(std::move(msg));
}

void L3ActivateSecondaryPDPContextReject::write(BitWriter& bw) const {
    // smCause: value octet (no identifier).
    bw.writeField(static_cast<uint8_t>(mCause), 8);
    detail::writeOpaqueTail(mAdditionalIes, bw);
}

void L3ActivateSecondaryPDPContextReject::text(std::ostream& os) const {
    os << "ActSecPDPRej(cause=" << SMCause2Str(mCause) << ")";
}

L3ActivateSecondaryPDPContextReject L3ActivateSecondaryPDPContextReject::Builder::build() const {
    L3ActivateSecondaryPDPContextReject msg;
    msg.mCause = m_cause;
    msg.mAdditionalIes = m_additionalIes;
    return msg;
}

L3ActivateSecondaryPDPContextReject::Builder L3ActivateSecondaryPDPContextReject::builder() {
    return Builder{};
}

// ── L3ActivateMBMSContextRequest (TS 44.068 section 9.5) ──────────────

size_t L3ActivateMBMSContextRequest::bodyLength() const {
    size_t len = 1 + mQoS.lengthV(); // QoS (LV)
    if (mHavePCO) len += tlvLen(mPCO.lengthV());
    len += mAdditionalIes.size();
    return len;
}

Expected<L3ActivateMBMSContextRequest> L3ActivateMBMSContextRequest::parse(BitReader& br) {
    // SM ACTIVATE MBMS CONTEXT REQUEST (TS 44.068): requested QoS (LV);
    // protocol configuration options (TLV, IEI 0x27) and any further
    // optional IEs (including the TMGI TLV) are kept opaque.
    L3ActivateMBMSContextRequest msg;

    // QoS: LV (mandatory).
    auto qlen = br.readField(8);
    if (!qlen) return Expected<L3ActivateMBMSContextRequest>::error(qlen.error());
    {
        auto qos = L3QoS::parse(br, qlen.value());
        if (!qos) return Expected<L3ActivateMBMSContextRequest>::error(qos.error());
        msg.mQoS = std::move(qos).value();
    }

    while (br.remainingBits() >= 16) {
        size_t vLen = 0;
        auto iei = readTLVHeader(br, vLen);
        if (!iei) return Expected<L3ActivateMBMSContextRequest>::error(iei.error());
        bool typed = false;
        if (iei.value() == L3ProtocolConfigOptions::IEI && !msg.mHavePCO) {
            if (auto pco = L3ProtocolConfigOptions::parse(br, vLen)) {
                msg.mHavePCO = true;
                msg.mPCO = std::move(pco).value();
                typed = true;
            }
        }
        if (!typed) {
            auto t = appendTlvToTail(br, msg.mAdditionalIes, iei.value(), vLen);
            if (!t) return Expected<L3ActivateMBMSContextRequest>::error(t.error());
        }
    }
    if (!detail::readOpaqueTail(br, msg.mAdditionalIes)) {
        return Expected<L3ActivateMBMSContextRequest>::error(
            ParseError{ParseError::Code::TruncatedInput, "activate MBMS context request: truncated optional IEs", br.position()});
    }

    return Expected<L3ActivateMBMSContextRequest>::hold(std::move(msg));
}

void L3ActivateMBMSContextRequest::write(BitWriter& bw) const {
    // QoS: LV (mandatory).
    bw.writeField(static_cast<uint32_t>(mQoS.lengthV()), 8);
    mQoS.write(bw);

    // PCO: TLV 0x27 (optional).
    if (mHavePCO) {
        bw.writeField(L3ProtocolConfigOptions::IEI, 8);
        bw.writeField(static_cast<uint32_t>(mPCO.lengthV()), 8);
        mPCO.write(bw);
    }

    detail::writeOpaqueTail(mAdditionalIes, bw);
}

void L3ActivateMBMSContextRequest::text(std::ostream& os) const {
    os << "ActMBMSReq(";
    mQoS.text(os);
    if (mHavePCO) { os << ", "; mPCO.text(os); }
    os << ")";
}

L3ActivateMBMSContextRequest L3ActivateMBMSContextRequest::Builder::build() const {
    L3ActivateMBMSContextRequest msg;
    msg.mQoS = m_qos;
    msg.mHavePCO = m_havePCO;
    msg.mPCO = m_pco;
    msg.mAdditionalIes = m_additionalIes;
    return msg;
}

L3ActivateMBMSContextRequest::Builder L3ActivateMBMSContextRequest::builder() {
    return Builder{};
}

// ── L3ActivateMBMSContextAccept (TS 44.068 section 9.5) ───────────────

size_t L3ActivateMBMSContextAccept::bodyLength() const {
    size_t len = 1; // pdpHandle(4)|spare(4)
    len += 1 + mQoS.lengthV(); // QoS (LV)
    if (mHavePCO) len += tlvLen(mPCO.lengthV());
    len += mAdditionalIes.size();
    return len;
}

Expected<L3ActivateMBMSContextAccept> L3ActivateMBMSContextAccept::parse(BitReader& br) {
    // SM ACTIVATE MBMS CONTEXT ACCEPT (TS 44.068): PDP handle in the first
    // octet, negotiated QoS (LV); optional IEs follow (kept opaque except
    // PCO TLV 0x27).
    L3ActivateMBMSContextAccept msg;

    auto o = br.readField(8);
    if (!o) return Expected<L3ActivateMBMSContextAccept>::error(o.error());
    msg.mPDPHandle = static_cast<uint8_t>((o.value() >> 4) & 0x0Fu);

    // QoS: LV (mandatory).
    auto qlen = br.readField(8);
    if (!qlen) return Expected<L3ActivateMBMSContextAccept>::error(qlen.error());
    {
        auto qos = L3QoS::parse(br, qlen.value());
        if (!qos) return Expected<L3ActivateMBMSContextAccept>::error(qos.error());
        msg.mQoS = std::move(qos).value();
    }

    while (br.remainingBits() >= 16) {
        size_t vLen = 0;
        auto iei = readTLVHeader(br, vLen);
        if (!iei) return Expected<L3ActivateMBMSContextAccept>::error(iei.error());
        bool typed = false;
        if (iei.value() == L3ProtocolConfigOptions::IEI && !msg.mHavePCO) {
            if (auto pco = L3ProtocolConfigOptions::parse(br, vLen)) {
                msg.mHavePCO = true;
                msg.mPCO = std::move(pco).value();
                typed = true;
            }
        }
        if (!typed) {
            auto t = appendTlvToTail(br, msg.mAdditionalIes, iei.value(), vLen);
            if (!t) return Expected<L3ActivateMBMSContextAccept>::error(t.error());
        }
    }
    if (!detail::readOpaqueTail(br, msg.mAdditionalIes)) {
        return Expected<L3ActivateMBMSContextAccept>::error(
            ParseError{ParseError::Code::TruncatedInput, "activate MBMS context accept: truncated optional IEs", br.position()});
    }

    return Expected<L3ActivateMBMSContextAccept>::hold(std::move(msg));
}

void L3ActivateMBMSContextAccept::write(BitWriter& bw) const {
    // pdpHandle(4)|spare(4).
    bw.writeField((static_cast<uint32_t>(mPDPHandle) & 0x0Fu) << 4, 8);

    // QoS: LV (mandatory).
    bw.writeField(static_cast<uint32_t>(mQoS.lengthV()), 8);
    mQoS.write(bw);

    // PCO: TLV 0x27 (optional).
    if (mHavePCO) {
        bw.writeField(L3ProtocolConfigOptions::IEI, 8);
        bw.writeField(static_cast<uint32_t>(mPCO.lengthV()), 8);
        mPCO.write(bw);
    }

    detail::writeOpaqueTail(mAdditionalIes, bw);
}

void L3ActivateMBMSContextAccept::text(std::ostream& os) const {
    os << "ActMBMSAcc(handle=" << static_cast<int>(mPDPHandle);
    os << ", "; mQoS.text(os);
    if (mHavePCO) { os << ", "; mPCO.text(os); }
    os << ")";
}

L3ActivateMBMSContextAccept L3ActivateMBMSContextAccept::Builder::build() const {
    L3ActivateMBMSContextAccept msg;
    msg.mPDPHandle = m_pdpHandle & 0x0Fu;
    msg.mQoS = m_qos;
    msg.mHavePCO = m_havePCO;
    msg.mPCO = m_pco;
    msg.mAdditionalIes = m_additionalIes;
    return msg;
}

L3ActivateMBMSContextAccept::Builder L3ActivateMBMSContextAccept::builder() {
    return Builder{};
}

// ── L3ActivateMBMSContextReject (TS 44.068 section 9.5) ───────────────

size_t L3ActivateMBMSContextReject::bodyLength() const {
    size_t len = 1; // smCause value octet
    len += mAdditionalIes.size();
    return len;
}

Expected<L3ActivateMBMSContextReject> L3ActivateMBMSContextReject::parse(BitReader& br) {
    // SM ACTIVATE MBMS CONTEXT REJECT (TS 44.068): the body starts with
    // the SM cause value octet (no identifier); any further optional IEs
    // are kept opaque.
    L3ActivateMBMSContextReject msg;

    auto c = br.readField(8);
    if (!c) return Expected<L3ActivateMBMSContextReject>::error(c.error());
    msg.mCause = static_cast<SMCause>(c.value());

    if (!detail::readOpaqueTail(br, msg.mAdditionalIes)) {
        return Expected<L3ActivateMBMSContextReject>::error(
            ParseError{ParseError::Code::TruncatedInput, "activate MBMS context reject: truncated optional IEs", br.position()});
    }

    return Expected<L3ActivateMBMSContextReject>::hold(std::move(msg));
}

void L3ActivateMBMSContextReject::write(BitWriter& bw) const {
    // smCause: value octet (no identifier).
    bw.writeField(static_cast<uint8_t>(mCause), 8);
    detail::writeOpaqueTail(mAdditionalIes, bw);
}

void L3ActivateMBMSContextReject::text(std::ostream& os) const {
    os << "ActMBMSRej(cause=" << SMCause2Str(mCause) << ")";
}

L3ActivateMBMSContextReject L3ActivateMBMSContextReject::Builder::build() const {
    L3ActivateMBMSContextReject msg;
    msg.mCause = m_cause;
    msg.mAdditionalIes = m_additionalIes;
    return msg;
}

L3ActivateMBMSContextReject::Builder L3ActivateMBMSContextReject::builder() {
    return Builder{};
}

// ── L3RequestMBMSContextActivation (TS 44.068 section 9.5) ────────────

size_t L3RequestMBMSContextActivation::bodyLength() const {
    size_t len = 1 + mQoS.lengthV(); // QoS (LV)
    if (mHavePCO) len += tlvLen(mPCO.lengthV());
    len += mAdditionalIes.size();
    return len;
}

Expected<L3RequestMBMSContextActivation> L3RequestMBMSContextActivation::parse(BitReader& br) {
    // SM REQUEST MBMS CONTEXT ACTIVATION (TS 44.068): requested QoS (LV);
    // protocol configuration options (TLV, IEI 0x27) and any further
    // optional IEs (including the TMGI TLV) are kept opaque.
    L3RequestMBMSContextActivation msg;

    // QoS: LV (mandatory).
    auto qlen = br.readField(8);
    if (!qlen) return Expected<L3RequestMBMSContextActivation>::error(qlen.error());
    {
        auto qos = L3QoS::parse(br, qlen.value());
        if (!qos) return Expected<L3RequestMBMSContextActivation>::error(qos.error());
        msg.mQoS = std::move(qos).value();
    }

    while (br.remainingBits() >= 16) {
        size_t vLen = 0;
        auto iei = readTLVHeader(br, vLen);
        if (!iei) return Expected<L3RequestMBMSContextActivation>::error(iei.error());
        bool typed = false;
        if (iei.value() == L3ProtocolConfigOptions::IEI && !msg.mHavePCO) {
            if (auto pco = L3ProtocolConfigOptions::parse(br, vLen)) {
                msg.mHavePCO = true;
                msg.mPCO = std::move(pco).value();
                typed = true;
            }
        }
        if (!typed) {
            auto t = appendTlvToTail(br, msg.mAdditionalIes, iei.value(), vLen);
            if (!t) return Expected<L3RequestMBMSContextActivation>::error(t.error());
        }
    }
    if (!detail::readOpaqueTail(br, msg.mAdditionalIes)) {
        return Expected<L3RequestMBMSContextActivation>::error(
            ParseError{ParseError::Code::TruncatedInput, "request MBMS context activation: truncated optional IEs", br.position()});
    }

    return Expected<L3RequestMBMSContextActivation>::hold(std::move(msg));
}

void L3RequestMBMSContextActivation::write(BitWriter& bw) const {
    // QoS: LV (mandatory).
    bw.writeField(static_cast<uint32_t>(mQoS.lengthV()), 8);
    mQoS.write(bw);

    // PCO: TLV 0x27 (optional).
    if (mHavePCO) {
        bw.writeField(L3ProtocolConfigOptions::IEI, 8);
        bw.writeField(static_cast<uint32_t>(mPCO.lengthV()), 8);
        mPCO.write(bw);
    }

    detail::writeOpaqueTail(mAdditionalIes, bw);
}

void L3RequestMBMSContextActivation::text(std::ostream& os) const {
    os << "ReqMBMSAct(";
    mQoS.text(os);
    if (mHavePCO) { os << ", "; mPCO.text(os); }
    os << ")";
}

L3RequestMBMSContextActivation L3RequestMBMSContextActivation::Builder::build() const {
    L3RequestMBMSContextActivation msg;
    msg.mQoS = m_qos;
    msg.mHavePCO = m_havePCO;
    msg.mPCO = m_pco;
    msg.mAdditionalIes = m_additionalIes;
    return msg;
}

L3RequestMBMSContextActivation::Builder L3RequestMBMSContextActivation::builder() {
    return Builder{};
}

// ── L3RequestMBMSContextActivationReject (TS 44.068 section 9.5) ──────

size_t L3RequestMBMSContextActivationReject::bodyLength() const {
    size_t len = 1; // smCause value octet
    len += mAdditionalIes.size();
    return len;
}

Expected<L3RequestMBMSContextActivationReject> L3RequestMBMSContextActivationReject::parse(BitReader& br) {
    // SM REQUEST MBMS CONTEXT ACTIVATION REJECT (TS 44.068): the body
    // starts with the SM cause value octet (no identifier); any further
    // optional IEs are kept opaque.
    L3RequestMBMSContextActivationReject msg;

    auto c = br.readField(8);
    if (!c) return Expected<L3RequestMBMSContextActivationReject>::error(c.error());
    msg.mCause = static_cast<SMCause>(c.value());

    if (!detail::readOpaqueTail(br, msg.mAdditionalIes)) {
        return Expected<L3RequestMBMSContextActivationReject>::error(
            ParseError{ParseError::Code::TruncatedInput, "request MBMS context activation reject: truncated optional IEs", br.position()});
    }

    return Expected<L3RequestMBMSContextActivationReject>::hold(std::move(msg));
}

void L3RequestMBMSContextActivationReject::write(BitWriter& bw) const {
    // smCause: value octet (no identifier).
    bw.writeField(static_cast<uint8_t>(mCause), 8);
    detail::writeOpaqueTail(mAdditionalIes, bw);
}

void L3RequestMBMSContextActivationReject::text(std::ostream& os) const {
    os << "ReqMBMSActRej(cause=" << SMCause2Str(mCause) << ")";
}

L3RequestMBMSContextActivationReject L3RequestMBMSContextActivationReject::Builder::build() const {
    L3RequestMBMSContextActivationReject msg;
    msg.mCause = m_cause;
    msg.mAdditionalIes = m_additionalIes;
    return msg;
}

L3RequestMBMSContextActivationReject::Builder L3RequestMBMSContextActivationReject::builder() {
    return Builder{};
}

// ── L3RequestSecondaryPDPContextActivation (TS 44.068 section 9.5) ────

size_t L3RequestSecondaryPDPContextActivation::bodyLength() const {
    size_t len = 1; // pdpHandle(4)|spare(4)
    len += 1 + mQoS.lengthV();         // QoS (LV)
    len += 1 + mPDPAddress.lengthV();  // PDP address (LV)
    len += tlvLen(mAPN.lengthV());     // APN (TLV, mandatory)
    if (mHavePCO) len += tlvLen(mPCO.lengthV());
    len += mAdditionalIes.size();
    return len;
}

Expected<L3RequestSecondaryPDPContextActivation> L3RequestSecondaryPDPContextActivation::parse(BitReader& br) {
    // SM REQUEST SECONDARY PDP CONTEXT ACTIVATION (TS 44.068): PDP handle
    // in the first octet, requested QoS (LV), requested PDP type and
    // address (LV), access point name (TLV, IEI 0x28); protocol
    // configuration options (TLV, IEI 0x27) and any further optional IEs
    // follow.
    L3RequestSecondaryPDPContextActivation msg;

    auto o = br.readField(8);
    if (!o) return Expected<L3RequestSecondaryPDPContextActivation>::error(o.error());
    msg.mPDPHandle = static_cast<uint8_t>((o.value() >> 4) & 0x0Fu);

    // QoS: LV (mandatory).
    auto qlen = br.readField(8);
    if (!qlen) return Expected<L3RequestSecondaryPDPContextActivation>::error(qlen.error());
    {
        auto qos = L3QoS::parse(br, qlen.value());
        if (!qos) return Expected<L3RequestSecondaryPDPContextActivation>::error(qos.error());
        msg.mQoS = std::move(qos).value();
    }

    // PDP type and address: LV (mandatory).
    auto alen = br.readField(8);
    if (!alen) return Expected<L3RequestSecondaryPDPContextActivation>::error(alen.error());
    {
        auto addr = L3PDPAddress::parse(br, alen.value());
        if (!addr) return Expected<L3RequestSecondaryPDPContextActivation>::error(addr.error());
        msg.mPDPAddress = std::move(addr).value();
    }

    // APN: TLV 0x28 (mandatory).
    {
        size_t vLen = 0;
        auto iei = readTLVHeader(br, vLen);
        if (!iei) return Expected<L3RequestSecondaryPDPContextActivation>::error(iei.error());
        if (iei.value() != L3AccessPointName::IEI) {
            return Expected<L3RequestSecondaryPDPContextActivation>::error(
                ParseError{ParseError::Code::InvalidIE, "request secondary PDP context activation: missing access point name", br.position()});
        }
        auto apn = L3AccessPointName::parse(br, vLen);
        if (!apn) return Expected<L3RequestSecondaryPDPContextActivation>::error(apn.error());
        msg.mAPN = std::move(apn).value();
    }

    while (br.remainingBits() >= 16) {
        size_t vLen = 0;
        auto iei = readTLVHeader(br, vLen);
        if (!iei) return Expected<L3RequestSecondaryPDPContextActivation>::error(iei.error());
        bool typed = false;
        if (iei.value() == L3ProtocolConfigOptions::IEI && !msg.mHavePCO) {
            if (auto pco = L3ProtocolConfigOptions::parse(br, vLen)) {
                msg.mHavePCO = true;
                msg.mPCO = std::move(pco).value();
                typed = true;
            }
        }
        if (!typed) {
            auto t = appendTlvToTail(br, msg.mAdditionalIes, iei.value(), vLen);
            if (!t) return Expected<L3RequestSecondaryPDPContextActivation>::error(t.error());
        }
    }
    if (!detail::readOpaqueTail(br, msg.mAdditionalIes)) {
        return Expected<L3RequestSecondaryPDPContextActivation>::error(
            ParseError{ParseError::Code::TruncatedInput, "request secondary PDP context activation: truncated optional IEs", br.position()});
    }

    return Expected<L3RequestSecondaryPDPContextActivation>::hold(std::move(msg));
}

void L3RequestSecondaryPDPContextActivation::write(BitWriter& bw) const {
    // pdpHandle(4)|spare(4).
    bw.writeField((static_cast<uint32_t>(mPDPHandle) & 0x0Fu) << 4, 8);

    // QoS: LV (mandatory).
    bw.writeField(static_cast<uint32_t>(mQoS.lengthV()), 8);
    mQoS.write(bw);

    // PDP type and address: LV (mandatory).
    bw.writeField(static_cast<uint32_t>(mPDPAddress.lengthV()), 8);
    mPDPAddress.write(bw);

    // APN: TLV 0x28 (mandatory).
    bw.writeField(L3AccessPointName::IEI, 8);
    bw.writeField(static_cast<uint32_t>(mAPN.lengthV()), 8);
    mAPN.write(bw);

    // PCO: TLV 0x27 (optional).
    if (mHavePCO) {
        bw.writeField(L3ProtocolConfigOptions::IEI, 8);
        bw.writeField(static_cast<uint32_t>(mPCO.lengthV()), 8);
        mPCO.write(bw);
    }

    detail::writeOpaqueTail(mAdditionalIes, bw);
}

void L3RequestSecondaryPDPContextActivation::text(std::ostream& os) const {
    os << "ReqSecPDPAct(handle=" << static_cast<int>(mPDPHandle);
    os << ", "; mQoS.text(os);
    os << ", "; mPDPAddress.text(os);
    os << ", "; mAPN.text(os);
    if (mHavePCO) { os << ", "; mPCO.text(os); }
    os << ")";
}

L3RequestSecondaryPDPContextActivation L3RequestSecondaryPDPContextActivation::Builder::build() const {
    L3RequestSecondaryPDPContextActivation msg;
    msg.mPDPHandle = m_pdpHandle & 0x0Fu;
    msg.mQoS = m_qos;
    msg.mPDPAddress = m_pdpAddress;
    msg.mAPN = m_apn;
    msg.mHavePCO = m_havePCO;
    msg.mPCO = m_pco;
    msg.mAdditionalIes = m_additionalIes;
    return msg;
}

L3RequestSecondaryPDPContextActivation::Builder L3RequestSecondaryPDPContextActivation::builder() {
    return Builder{};
}

// ── L3RequestSecondaryPDPContextActivationReject (TS 44.068 9.5) ──────

size_t L3RequestSecondaryPDPContextActivationReject::bodyLength() const {
    size_t len = 1; // smCause value octet
    len += mAdditionalIes.size();
    return len;
}

Expected<L3RequestSecondaryPDPContextActivationReject> L3RequestSecondaryPDPContextActivationReject::parse(BitReader& br) {
    // SM REQUEST SECONDARY PDP CONTEXT ACTIVATION REJECT (TS 44.068): the
    // body starts with the SM cause value octet (no identifier); any
    // further optional IEs are kept opaque.
    L3RequestSecondaryPDPContextActivationReject msg;

    auto c = br.readField(8);
    if (!c) return Expected<L3RequestSecondaryPDPContextActivationReject>::error(c.error());
    msg.mCause = static_cast<SMCause>(c.value());

    if (!detail::readOpaqueTail(br, msg.mAdditionalIes)) {
        return Expected<L3RequestSecondaryPDPContextActivationReject>::error(
            ParseError{ParseError::Code::TruncatedInput, "request secondary PDP context activation reject: truncated optional IEs", br.position()});
    }

    return Expected<L3RequestSecondaryPDPContextActivationReject>::hold(std::move(msg));
}

void L3RequestSecondaryPDPContextActivationReject::write(BitWriter& bw) const {
    // smCause: value octet (no identifier).
    bw.writeField(static_cast<uint8_t>(mCause), 8);
    detail::writeOpaqueTail(mAdditionalIes, bw);
}

void L3RequestSecondaryPDPContextActivationReject::text(std::ostream& os) const {
    os << "ReqSecPDPActRej(cause=" << SMCause2Str(mCause) << ")";
}

L3RequestSecondaryPDPContextActivationReject L3RequestSecondaryPDPContextActivationReject::Builder::build() const {
    L3RequestSecondaryPDPContextActivationReject msg;
    msg.mCause = m_cause;
    msg.mAdditionalIes = m_additionalIes;
    return msg;
}

L3RequestSecondaryPDPContextActivationReject::Builder L3RequestSecondaryPDPContextActivationReject::builder() {
    return Builder{};
}

// ── L3SMNotification (TS 44.068 section 9.5) ──────────────────────────

Expected<L3SMNotification> L3SMNotification::parse(BitReader& br) {
    // SM NOTIFICATION (TS 44.068): PDP handle in the first octet; any
    // further optional IEs are kept opaque.
    L3SMNotification msg;

    auto o = br.readField(8);
    if (!o) return Expected<L3SMNotification>::error(o.error());
    msg.mPDPHandle = static_cast<uint8_t>((o.value() >> 4) & 0x0Fu);

    if (!detail::readOpaqueTail(br, msg.mAdditionalIes)) {
        return Expected<L3SMNotification>::error(
            ParseError{ParseError::Code::TruncatedInput, "SM notification: truncated optional IEs", br.position()});
    }

    return Expected<L3SMNotification>::hold(std::move(msg));
}

void L3SMNotification::write(BitWriter& bw) const {
    // pdpHandle(4)|spare(4).
    bw.writeField((static_cast<uint32_t>(mPDPHandle) & 0x0Fu) << 4, 8);
    detail::writeOpaqueTail(mAdditionalIes, bw);
}

void L3SMNotification::text(std::ostream& os) const {
    os << "SMNotification(handle=" << static_cast<int>(mPDPHandle) << ")";
}

L3SMNotification L3SMNotification::Builder::build() const {
    L3SMNotification msg;
    msg.mPDPHandle = m_pdpHandle & 0x0Fu;
    msg.mAdditionalIes = m_additionalIes;
    return msg;
}

L3SMNotification::Builder L3SMNotification::builder() {
    return Builder{};
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
