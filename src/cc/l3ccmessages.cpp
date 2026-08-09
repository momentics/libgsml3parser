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

#include "gsml3parser/cc/l3ccmessages.h"
#include <sstream>
#include <iomanip>
#include <cstring>

namespace gsml3parser {

// ── Internal TLV helpers ────────────────────────────────────────────────

namespace detail {

// Read one IEI octet. Returns error if no data left.
inline Expected<uint8_t> readIEI(BitReader& br) {
    auto r = br.readField(8);
    if (!r) return Expected<uint8_t>::error(r.error());
    return Expected<uint8_t>::hold(static_cast<uint8_t>(r.value()));
}

// Read length octet (for unstructured IE, IEI < 0x80).
inline Expected<size_t> readLength(BitReader& br) {
    auto r = br.readField(8);
    if (!r) return Expected<size_t>::error(r.error());
    return Expected<size_t>::hold(r.value());
}

// Read structured length (IEI >= 0x80): first octet low-7-bits = num length octets.
inline Expected<size_t> readStructuredLength(BitReader& br) {
    auto r = br.readField(7);
    if (!r) return Expected<size_t>::error(r.error());
    size_t numLenOctets = r.value();
    size_t len = 0;
    for (size_t i = 0; i < numLenOctets; ++i) {
        r = br.readField(8);
        if (!r) return Expected<size_t>::error(r.error());
        len = (len << 8) | r.value();
    }
    return Expected<size_t>::hold(len);
}

// Skip 'len' bytes from BitReader.
inline Expected<void> skipBytes(BitReader& br, size_t len) {
    size_t bits = len * 8;
    if (br.position() + bits > br.position() + br.remainingBits()) {
        // Just advance as far as possible
    }
    for (size_t i = 0; i < len; ++i) {
        auto r = br.readField(8);
        if (!r) return Expected<void>::error(r.error());
    }
    return Expected<void>::hold();
}

// Skip a TLV element: read IEI, determine length format, skip value.
inline Expected<void> skipTLV(BitReader& br) {
    auto ieiRes = readIEI(br);
    if (!ieiRes) return Expected<void>::error(ieiRes.error());
    uint8_t iei = ieiRes.value();

    size_t len;
    if (iei >= 0x80) {
        auto l = readStructuredLength(br);
        if (!l) return Expected<void>::error(l.error());
        len = l.value();
    } else {
        auto l = readLength(br);
        if (!l) return Expected<void>::error(l.error());
        len = l.value();
    }
    return skipBytes(br, len);
}

// Skip a TV element: IEI already consumed, read 1 value octet.
inline Expected<void> skipTV(BitReader& br) {
    auto r = br.readField(8);
    if (!r) return Expected<void>::error(r.error());
    return Expected<void>::hold();
}

// Parse optional TLV with given IEI. Returns true if found and parsed.
template<typename T>
inline Expected<bool> parseOptionalTLV(BitReader& br, uint8_t iei, T& out, bool& have) {
    auto ieiRes = readIEI(br);
    if (!ieiRes) return Expected<bool>::error(ieiRes.error());
    if (ieiRes.value() != iei) return Expected<bool>::hold(false);
    auto lenRes = readLength(br);
    if (!lenRes) return Expected<bool>::error(lenRes.error());
    auto p = T::parse(br, lenRes.value());
    if (!p) return Expected<bool>::error(p.error());
    out = std::move(p.value());
    have = true;
    return Expected<bool>::hold(true);
}

// Parse optional TV with given IEI. Returns true if found and parsed.
template<typename T>
inline Expected<bool> parseOptionalTV(BitReader& br, uint8_t iei, T& out, bool& have) {
    auto ieiRes = readIEI(br);
    if (!ieiRes) return Expected<bool>::error(ieiRes.error());
    if (ieiRes.value() != iei) return Expected<bool>::hold(false);
    auto p = T::parse(br);
    if (!p) return Expected<bool>::error(p.error());
    out = std::move(p.value());
    have = true;
    return Expected<bool>::hold(true);
}

// Parse optional TLV with fixed-size value (no length param on parse).
template<typename T>
inline Expected<bool> parseOptionalTLVFixed(BitReader& br, uint8_t iei, T& out, bool& have) {
    auto ieiRes = readIEI(br);
    if (!ieiRes) return Expected<bool>::error(ieiRes.error());
    if (ieiRes.value() != iei) return Expected<bool>::hold(false);
    auto lenRes = readLength(br);
    if (!lenRes) return Expected<bool>::error(lenRes.error());
    // Consume the value bytes according to length
    size_t valBits = lenRes.value() * 8;
    // For fixed-size elements, just parse directly
    auto p = T::parse(br);
    if (!p) return Expected<bool>::error(p.error());
    out = std::move(p.value());
    have = true;
    return Expected<bool>::hold(true);
}

// ── ccCommon helpers (replaces L3CCCommonIEs mixin) ───────────────────

inline Expected<void> ccCommonParse(BitReader& br,
                                     bool& haveFacility, L3SupServFacilityIE& facility,
                                     bool& haveSSVersion, L3SupServVersionIndicator& ssVersion) {
    while (br.hasMore()) {
        uint8_t peek = static_cast<uint8_t>(br.peekField(8));
        if (peek == 0x1c) {
            auto ieiRes = readIEI(br);
            if (!ieiRes) return Expected<void>::error(ieiRes.error());
            auto f = L3SupServFacilityIE::parse(br, 1);
            if (!f) return Expected<void>::error(f.error());
            facility = std::move(f.value());
            haveFacility = true;
        } else if (peek == 0x7f) {
            auto ieiRes = readIEI(br);
            if (!ieiRes) return Expected<void>::error(ieiRes.error());
            auto lenRes = readStructuredLength(br);
            if (!lenRes) return Expected<void>::error(lenRes.error());
            size_t innerLen = lenRes.value();
            if (innerLen >= 1) {
                auto innerIEI = br.readField(8);
                if (!innerIEI) return Expected<void>::error(innerIEI.error());
                if (innerIEI.value() == 0x1c) {
                    auto v = L3SupServVersionIndicator::parse(br);
                    if (!v) return Expected<void>::error(v.error());
                    ssVersion = std::move(v.value());
                    haveSSVersion = true;
                } else {
                    auto skipRes = skipBytes(br, innerLen - 1);
                    if (!skipRes) return Expected<void>::error(skipRes.error());
                }
            }
        } else {
            break;
        }
    }
    return Expected<void>::hold();
}

inline void ccCommonWrite(BitWriter& bw,
                           bool haveFacility, const L3SupServFacilityIE& facility,
                           bool haveSSVersion, const L3SupServVersionIndicator& ssVersion) {
    if (haveFacility) {
        bw.writeField(0x1c, 8);
        bw.writeField(static_cast<uint32_t>(facility.lengthV()), 8);
        facility.write(bw);
    }
    if (haveSSVersion) {
        bw.writeField(0x7f, 8);
        bw.writeField(1, 7);
        bw.writeField(0x1c, 8);
        ssVersion.write(bw);
    }
}

inline size_t ccCommonLength(bool haveFacility, const L3SupServFacilityIE& facility,
                               bool haveSSVersion) {
    size_t len = 0;
    if (haveFacility) len += 2 + facility.lengthV();
    if (haveSSVersion) len += 3;
    return len;
}

inline void ccCommonText(std::ostream& os,
                          bool haveFacility, const L3SupServFacilityIE& facility,
                          bool haveSSVersion, const L3SupServVersionIndicator& ssVersion) {
    if (haveFacility) { os << " Facility=("; facility.text(os); os << ")"; }
    if (haveSSVersion) { os << " SSVersion=("; ssVersion.text(os); os << ")"; }
}

} // namespace detail

// ── operator<< for CCMessageType ────────────────────────────────────────

std::ostream& operator<<(std::ostream& os, CCMessageType mti) {
    switch (mti) {
        case CCMessageType::Alerting:            os << "Alerting"; break;
        case CCMessageType::CallConfirmed:       os << "CallConfirmed"; break;
        case CCMessageType::CallProceeding:      os << "CallProceeding"; break;
        case CCMessageType::Connect:             os << "Connect"; break;
        case CCMessageType::Setup:               os << "Setup"; break;
        case CCMessageType::EmergencySetup:      os << "EmergencySetup"; break;
        case CCMessageType::ConnectAcknowledge:  os << "ConnectAcknowledge"; break;
        case CCMessageType::Progress:            os << "Progress"; break;
        case CCMessageType::Disconnect:          os << "Disconnect"; break;
        case CCMessageType::Release:             os << "Release"; break;
        case CCMessageType::ReleaseComplete:     os << "ReleaseComplete"; break;
        case CCMessageType::StartDTMF:           os << "StartDTMF"; break;
        case CCMessageType::StopDTMF:            os << "StopDTMF"; break;
        case CCMessageType::StopDTMFAcknowledge: os << "StopDTMFAck"; break;
        case CCMessageType::StartDTMFAcknowledge: os << "StartDTMFAck"; break;
        case CCMessageType::StartDTMFReject:     os << "StartDTMFReject"; break;
        case CCMessageType::Hold:                os << "Hold"; break;
        case CCMessageType::HoldReject:          os << "HoldReject"; break;
        case CCMessageType::CCStatus:            os << "CCStatus"; break;
        default:                                  os << "Unknown_CC(" << static_cast<uint8_t>(mti) << ")"; break;
    }
    return os;
}

// ── L3Setup ─────────────────────────────────────────────────────────────

L3Setup::Builder L3Setup::builder(unsigned ti) {
    return Builder{ti};
}

L3Setup L3Setup::Builder::build() const {
    L3Setup msg(m_ti);
    if (m_haveCalled) {
        msg.mHaveCalledParty = true;
        msg.mCalledParty = m_called;
    }
    return msg;
}

Expected<L3Setup> L3Setup::parse(BitReader& br) {
    L3Setup msg;

    while (br.hasMore()) {
        auto ieiRes = detail::readIEI(br);
        if (!ieiRes) return Expected<L3Setup>::error(ieiRes.error());
        uint8_t iei = ieiRes.value();

        // Structured elements: skip
        if ((iei & 0xf0) == 0xd0) {
            auto skipRes = detail::skipTLV(br);
            if (!skipRes) return Expected<L3Setup>::error(skipRes.error());
            continue;
        }
        if ((iei & 0xf0) == 0x80) {
            auto skipRes = detail::skipTLV(br);
            if (!skipRes) return Expected<L3Setup>::error(skipRes.error());
            continue;
        }

        switch (iei) {
        case 0x04: { // BearerCapability TLV
            auto lenRes = detail::readLength(br);
            if (!lenRes) return Expected<L3Setup>::error(lenRes.error());
            // BearerCapability parse doesn't take length, reads self-delimiting
            auto p = L3BearerCapability::parse(br);
            if (!p) return Expected<L3Setup>::error(p.error());
            msg.mBearerCapability = std::move(p.value());
            msg.mHaveBearerCapability = true;
            continue;
        }
        case 0x1c: { // Facility TLV (ccCommon)
            auto lenRes = detail::readLength(br);
            if (!lenRes) return Expected<L3Setup>::error(lenRes.error());
            auto f = L3SupServFacilityIE::parse(br, lenRes.value());
            if (!f) return Expected<L3Setup>::error(f.error());
            msg.mFacility = std::move(f.value());
            msg.mHaveFacility = true;
            continue;
        }
        case 0x1e: { // Skip TLV
            auto skipRes = detail::skipTLV(br);
            if (!skipRes) return Expected<L3Setup>::error(skipRes.error());
            continue;
        }
        case 0x34: { // Signal TV
            auto p = L3Signal::parse(br);
            if (!p) return Expected<L3Setup>::error(p.error());
            msg.mSignal = std::move(p.value());
            msg.mHaveSignal = true;
            continue;
        }
        case 0x5c: { // CallingParty TLV
            auto lenRes = detail::readLength(br);
            if (!lenRes) return Expected<L3Setup>::error(lenRes.error());
            auto p = L3CallingPartyBCDNumber::parse(br, lenRes.value());
            if (!p) return Expected<L3Setup>::error(p.error());
            msg.mCallingParty = std::move(p.value());
            msg.mHaveCallingParty = true;
            continue;
        }
        case 0x5d: { // Skip TLV
            auto skipRes = detail::skipTLV(br);
            if (!skipRes) return Expected<L3Setup>::error(skipRes.error());
            continue;
        }
        case 0x5e: { // CalledParty TLV
            auto lenRes = detail::readLength(br);
            if (!lenRes) return Expected<L3Setup>::error(lenRes.error());
            auto p = L3CalledPartyBCDNumber::parse(br, lenRes.value());
            if (!p) return Expected<L3Setup>::error(p.error());
            msg.mCalledParty = std::move(p.value());
            msg.mHaveCalledParty = true;
            continue;
        }
        case 0x6d: case 0x74: case 0x75:
        case 0x7c: case 0x7d: case 0x7e: { // Skip TLV
            auto skipRes = detail::skipTLV(br);
            if (!skipRes) return Expected<L3Setup>::error(skipRes.error());
            continue;
        }
        case 0x7f: { // Structured element - may contain SSVersion
            auto lenRes = detail::readStructuredLength(br);
            if (!lenRes) return Expected<L3Setup>::error(lenRes.error());
            size_t innerLen = lenRes.value();
            if (innerLen >= 1) {
                auto innerIEI = br.readField(8);
                if (!innerIEI) return Expected<L3Setup>::error(innerIEI.error());
                if (innerIEI.value() == 0x1c) {
                    auto v = L3SupServVersionIndicator::parse(br);
                    if (!v) return Expected<L3Setup>::error(v.error());
                    msg.mSSVersion = std::move(v.value());
                    msg.mHaveSSVersion = true;
                } else {
                    auto skipRes = detail::skipBytes(br, innerLen - 1);
                    if (!skipRes) return Expected<L3Setup>::error(skipRes.error());
                }
            }
            continue;
        }
        case 0xa1: case 0xa2: { // Skip TV (1 value octet each)
            auto skipRes = detail::skipTV(br);
            if (!skipRes) return Expected<L3Setup>::error(skipRes.error());
            continue;
        }
        case 0x15: case 0x1d: case 0x1b: case 0x2d: case 0x2e:
        case 0x19: case 0x2f: case 0x3a: case 0x41: { // Skip TLV
            auto skipRes = detail::skipTLV(br);
            if (!skipRes) return Expected<L3Setup>::error(skipRes.error());
            continue;
        }
        case 0x40: { // SupportedCodecs TLV
            auto lenRes = detail::readLength(br);
            if (!lenRes) return Expected<L3Setup>::error(lenRes.error());
            auto p = L3SupportedCodecList::parse(br, lenRes.value());
            if (!p) return Expected<L3Setup>::error(p.error());
            msg.mSupportedCodecs = std::move(p.value());
            msg.mHaveSupportedCodecs = true;
            continue;
        }
        case 0xa3: { // Skip TV
            auto skipRes = detail::skipTV(br);
            if (!skipRes) return Expected<L3Setup>::error(skipRes.error());
            continue;
        }
        default: { // Unknown IE - skip TLV
            auto skipRes = detail::skipTLV(br);
            if (!skipRes) return Expected<L3Setup>::error(skipRes.error());
            continue;
        }
        }
    }

    return Expected<L3Setup>::hold(std::move(msg));
}

void L3Setup::write(BitWriter& bw) const {
    if (mHaveBearerCapability) {
        bw.writeField(0x04, 8);
        bw.writeField(static_cast<uint32_t>(mBearerCapability.lengthV()), 8);
        mBearerCapability.write(bw);
    }
    if (mHaveCalledParty) {
        bw.writeField(0x5e, 8);
        bw.writeField(static_cast<uint32_t>(mCalledParty.lengthV()), 8);
        mCalledParty.write(bw);
    }
    if (mHaveCallingParty) {
        bw.writeField(0x5c, 8);
        bw.writeField(static_cast<uint32_t>(mCallingParty.lengthV()), 8);
        mCallingParty.write(bw);
    }
    if (mHaveSupportedCodecs && (mSupportedCodecs.isGsmPresent() || mSupportedCodecs.isUmtsPresent())) {
        bw.writeField(0x40, 8);
        bw.writeField(static_cast<uint32_t>(mSupportedCodecs.lengthV()), 8);
        mSupportedCodecs.write(bw);
    }
    if (mHaveSignal) {
        bw.writeField(0x34, 8);
        mSignal.write(bw);
    }
    detail::ccCommonWrite(bw, mHaveFacility, mFacility, mHaveSSVersion, mSSVersion);
}

size_t L3Setup::bodyLength() const {
    size_t len = 0;
    if (mHaveBearerCapability) len += 2 + mBearerCapability.lengthV();
    if (mHaveCalledParty) len += 2 + mCalledParty.lengthV();
    if (mHaveCallingParty) len += 2 + mCallingParty.lengthV();
    if (mHaveSupportedCodecs && (mSupportedCodecs.isGsmPresent() || mSupportedCodecs.isUmtsPresent()))
        len += 2 + mSupportedCodecs.lengthV();
    if (mHaveSignal) len += 1 + L3Signal::lengthV();
    len += detail::ccCommonLength(mHaveFacility, mFacility, mHaveSSVersion);
    return len;
}

void L3Setup::text(std::ostream& os) const {
    os << "Setup: TI=" << mTI;
    if (mHaveCalledParty) { os << " CalledParty=(" << mCalledParty.digits() << ")"; }
    if (mHaveCallingParty) { os << " CallingParty=(" << mCallingParty.digits() << ")"; }
    if (mHaveBearerCapability) { os << " BearerCapability=("; mBearerCapability.text(os); os << ")"; }
    if (mHaveSupportedCodecs && (mSupportedCodecs.isGsmPresent() || mSupportedCodecs.isUmtsPresent())) {
        os << " SupportedCodecList=("; mSupportedCodecs.text(os); os << ")";
    }
    if (mHaveSignal) { os << " "; mSignal.text(os); }
    detail::ccCommonText(os, mHaveFacility, mFacility, mHaveSSVersion, mSSVersion);
}

// ── L3EmergencySetup ────────────────────────────────────────────────────

Expected<L3EmergencySetup> L3EmergencySetup::parse(BitReader&) {
    return Expected<L3EmergencySetup>::hold(L3EmergencySetup());
}

void L3EmergencySetup::write(BitWriter&) const {}

void L3EmergencySetup::text(std::ostream& os) const {
    os << "EmergencySetup: TI=" << mTI;
}

// ── L3CallProceeding ───────────────────────────────────────────────────

Expected<L3CallProceeding> L3CallProceeding::parse(BitReader& br) {
    L3CallProceeding msg;

    // GSM 04.08 9.3.3: skip repeat indicator(TV 0x0d), bearer capability x2(TLV 0x04), facility(TLV 0x1c), parse progress(TLV 0x1e)
    while (br.hasMore()) {
        auto ieiRes = detail::readIEI(br);
        if (!ieiRes) return Expected<L3CallProceeding>::error(ieiRes.error());
        uint8_t iei = ieiRes.value();

        switch (iei) {
        case 0x0d: { // Repeat indicator TV - skip 4 bits
            auto r = br.readField(4);
            if (!r) return Expected<L3CallProceeding>::error(r.error());
            continue;
        }
        case 0x04: { // Bearer capability TLV - skip (may appear twice)
            auto lenRes = detail::readLength(br);
            if (!lenRes) return Expected<L3CallProceeding>::error(lenRes.error());
            auto skipRes = detail::skipBytes(br, lenRes.value());
            if (!skipRes) return Expected<L3CallProceeding>::error(skipRes.error());
            continue;
        }
        case 0x1c: { // Facility TLV - skip
            auto lenRes = detail::readLength(br);
            if (!lenRes) return Expected<L3CallProceeding>::error(lenRes.error());
            auto skipRes = detail::skipBytes(br, lenRes.value());
            if (!skipRes) return Expected<L3CallProceeding>::error(skipRes.error());
            continue;
        }
        case 0x1e: { // Progress indicator TLV
            auto lenRes = detail::readLength(br);
            if (!lenRes) return Expected<L3CallProceeding>::error(lenRes.error());
            auto p = L3ProgressIndicator::parse(br);
            if (!p) return Expected<L3CallProceeding>::error(p.error());
            msg.mProgress = std::move(p.value());
            msg.mHaveProgress = true;
            continue;
        }
        default: {
            auto lenRes = detail::readLength(br);
            if (!lenRes) return Expected<L3CallProceeding>::error(lenRes.error());
            auto skipRes = detail::skipBytes(br, lenRes.value());
            if (!skipRes) return Expected<L3CallProceeding>::error(skipRes.error());
            continue;
        }
        }
    }

    return Expected<L3CallProceeding>::hold(std::move(msg));
}

void L3CallProceeding::write(BitWriter& bw) const {
    if (mHaveBearerCapability) {
        bw.writeField(0x04, 8);
        bw.writeField(static_cast<uint32_t>(mBearerCapability.lengthV()), 8);
        mBearerCapability.write(bw);
    }
    if (mHaveProgress) {
        bw.writeField(0x1e, 8);
        bw.writeField(static_cast<uint32_t>(L3ProgressIndicator::lengthV()), 8);
        mProgress.write(bw);
    }
}

size_t L3CallProceeding::bodyLength() const {
    size_t sum = 0;
    if (mHaveBearerCapability) sum += 2 + mBearerCapability.lengthV();
    if (mHaveProgress) sum += 2 + L3ProgressIndicator::lengthV();
    return sum;
}

void L3CallProceeding::text(std::ostream& os) const {
    os << "CallProceeding: TI=" << mTI;
    if (mHaveBearerCapability) { os << " BearerCapability=("; mBearerCapability.text(os); os << ")"; }
    if (mHaveProgress) { os << " Progress=("; mProgress.text(os); os << ")"; }
}

// ── L3Alerting ─────────────────────────────────────────────────────────

Expected<L3Alerting> L3Alerting::parse(BitReader& br) {
    L3Alerting msg;

    // GSM 04.08 9.3.1: ccCommon, progress(TLV 0x1E), ccCommon again
    auto ccRes = detail::ccCommonParse(br, msg.mHaveFacility, msg.mFacility, msg.mHaveSSVersion, msg.mSSVersion);
    if (!ccRes) return Expected<L3Alerting>::error(ccRes.error());

    while (br.hasMore()) {
        uint8_t peek = static_cast<uint8_t>(br.peekField(8));
        if (peek == 0x1e) {
            auto ieiRes = detail::readIEI(br);
            if (!ieiRes) return Expected<L3Alerting>::error(ieiRes.error());
            auto lenRes = detail::readLength(br);
            if (!lenRes) return Expected<L3Alerting>::error(lenRes.error());
            auto p = L3ProgressIndicator::parse(br);
            if (!p) return Expected<L3Alerting>::error(p.error());
            msg.mProgress = std::move(p.value());
            msg.mHaveProgress = true;
        } else {
            break;
        }
    }

    ccRes = detail::ccCommonParse(br, msg.mHaveFacility, msg.mFacility, msg.mHaveSSVersion, msg.mSSVersion);
    if (!ccRes) return Expected<L3Alerting>::error(ccRes.error());

    return Expected<L3Alerting>::hold(std::move(msg));
}

void L3Alerting::write(BitWriter& bw) const {
    detail::ccCommonWrite(bw, mHaveFacility, mFacility, mHaveSSVersion, mSSVersion);
    if (mHaveProgress) {
        bw.writeField(0x1e, 8);
        bw.writeField(static_cast<uint32_t>(L3ProgressIndicator::lengthV()), 8);
        mProgress.write(bw);
    }
}

size_t L3Alerting::bodyLength() const {
    size_t sum = 0;
    if (mHaveProgress) sum += 2 + L3ProgressIndicator::lengthV();
    sum += detail::ccCommonLength(mHaveFacility, mFacility, mHaveSSVersion);
    return sum;
}

void L3Alerting::text(std::ostream& os) const {
    os << "Alerting: TI=" << mTI;
    if (mHaveProgress) { os << " Progress=("; mProgress.text(os); os << ")"; }
    detail::ccCommonText(os, mHaveFacility, mFacility, mHaveSSVersion, mSSVersion);
}

// ── L3Connect ──────────────────────────────────────────────────────────

Expected<L3Connect> L3Connect::parse(BitReader& br) {
    L3Connect msg;

    // GSM 04.08 9.3.5: skip facility(TLV 0x1c), parse progress(TLV 0x1e)
    while (br.hasMore()) {
        auto ieiRes = detail::readIEI(br);
        if (!ieiRes) return Expected<L3Connect>::error(ieiRes.error());
        uint8_t iei = ieiRes.value();

        switch (iei) {
        case 0x1c: { // Facility - skip
            auto lenRes = detail::readLength(br);
            if (!lenRes) return Expected<L3Connect>::error(lenRes.error());
            auto skipRes = detail::skipBytes(br, lenRes.value());
            if (!skipRes) return Expected<L3Connect>::error(skipRes.error());
            continue;
        }
        case 0x1e: { // Progress indicator
            auto lenRes = detail::readLength(br);
            if (!lenRes) return Expected<L3Connect>::error(lenRes.error());
            auto p = L3ProgressIndicator::parse(br);
            if (!p) return Expected<L3Connect>::error(p.error());
            msg.mProgress = std::move(p.value());
            msg.mHaveProgress = true;
            continue;
        }
        default: {
            auto lenRes = detail::readLength(br);
            if (!lenRes) return Expected<L3Connect>::error(lenRes.error());
            auto skipRes = detail::skipBytes(br, lenRes.value());
            if (!skipRes) return Expected<L3Connect>::error(skipRes.error());
            continue;
        }
        }
    }

    return Expected<L3Connect>::hold(std::move(msg));
}

void L3Connect::write(BitWriter& bw) const {
    if (mHaveProgress) {
        bw.writeField(0x1e, 8);
        bw.writeField(static_cast<uint32_t>(L3ProgressIndicator::lengthV()), 8);
        mProgress.write(bw);
    }
}

size_t L3Connect::bodyLength() const {
    size_t len = 0;
    if (mHaveProgress) len += 2 + L3ProgressIndicator::lengthV();
    return len;
}

void L3Connect::text(std::ostream& os) const {
    os << "Connect: TI=" << mTI;
    if (mHaveProgress) { os << " Progress=("; mProgress.text(os); os << ")"; }
}

// ── L3ConnectAcknowledge ───────────────────────────────────────────────

Expected<L3ConnectAcknowledge> L3ConnectAcknowledge::parse(BitReader&) {
    return Expected<L3ConnectAcknowledge>::hold(L3ConnectAcknowledge());
}

void L3ConnectAcknowledge::write(BitWriter&) const {}

void L3ConnectAcknowledge::text(std::ostream& os) const {
    os << "ConnectAcknowledge: TI=" << mTI;
}

// ── L3CallConfirmed ────────────────────────────────────────────────────

Expected<L3CallConfirmed> L3CallConfirmed::parse(BitReader& br) {
    L3CallConfirmed msg;

    while (br.hasMore()) {
        auto ieiRes = detail::readIEI(br);
        if (!ieiRes) return Expected<L3CallConfirmed>::error(ieiRes.error());
        uint8_t iei = ieiRes.value();

        if ((iei & 0xf0) == 0xd0) {
            auto skipRes = detail::skipTLV(br);
            if (!skipRes) return Expected<L3CallConfirmed>::error(skipRes.error());
            continue;
        }

        switch (iei) {
        case 0x08: { // Cause TLV
            auto lenRes = detail::readLength(br);
            if (!lenRes) return Expected<L3CallConfirmed>::error(lenRes.error());
            auto p = L3CauseElement::parse(br);
            if (!p) return Expected<L3CallConfirmed>::error(p.error());
            msg.mCause = std::move(p.value());
            msg.mHaveCause = true;
            continue;
        }
        case 0x40: { // SupportedCodecs TLV
            auto lenRes = detail::readLength(br);
            if (!lenRes) return Expected<L3CallConfirmed>::error(lenRes.error());
            auto p = L3SupportedCodecList::parse(br, lenRes.value());
            if (!p) return Expected<L3CallConfirmed>::error(p.error());
            msg.mSupportedCodecs = std::move(p.value());
            msg.mHaveSupportedCodecs = true;
            continue;
        }
        case 0x04: { // BearerCapability TLV
            auto p = L3BearerCapability::parse(br);
            if (!p) return Expected<L3CallConfirmed>::error(p.error());
            msg.mBearerCapability = std::move(p.value());
            msg.mHaveBearerCapability = true;
            continue;
        }
        case 0x15: case 0x2d: { // Skip TLV
            auto skipRes = detail::skipTLV(br);
            if (!skipRes) return Expected<L3CallConfirmed>::error(skipRes.error());
            continue;
        }
        default: {
            auto skipRes = detail::skipTLV(br);
            if (!skipRes) return Expected<L3CallConfirmed>::error(skipRes.error());
            continue;
        }
        }
    }

    return Expected<L3CallConfirmed>::hold(std::move(msg));
}

void L3CallConfirmed::write(BitWriter& bw) const {
    if (mHaveBearerCapability) {
        bw.writeField(0x04, 8);
        bw.writeField(static_cast<uint32_t>(mBearerCapability.lengthV()), 8);
        mBearerCapability.write(bw);
    }
    if (mHaveCause) {
        bw.writeField(0x08, 8);
        bw.writeField(static_cast<uint32_t>(L3CauseElement::lengthV()), 8);
        mCause.write(bw);
    }
    if (mHaveSupportedCodecs && (mSupportedCodecs.isGsmPresent() || mSupportedCodecs.isUmtsPresent())) {
        bw.writeField(0x40, 8);
        bw.writeField(static_cast<uint32_t>(mSupportedCodecs.lengthV()), 8);
        mSupportedCodecs.write(bw);
    }
}

size_t L3CallConfirmed::bodyLength() const {
    size_t sum = 0;
    if (mHaveBearerCapability) sum += 2 + mBearerCapability.lengthV();
    if (mHaveCause) sum += 2 + L3CauseElement::lengthV();
    if (mHaveSupportedCodecs && (mSupportedCodecs.isGsmPresent() || mSupportedCodecs.isUmtsPresent()))
        sum += 2 + mSupportedCodecs.lengthV();
    return sum;
}

void L3CallConfirmed::text(std::ostream& os) const {
    os << "CallConfirmed: TI=" << mTI;
    if (mHaveBearerCapability) { os << " BearerCapability=("; mBearerCapability.text(os); os << ")"; }
    if (mHaveSupportedCodecs && (mSupportedCodecs.isGsmPresent() || mSupportedCodecs.isUmtsPresent())) {
        os << " SupportedCodecList=("; mSupportedCodecs.text(os); os << ")";
    }
    if (mHaveCause) { os << " Cause=("; mCause.text(os); os << ")"; }
}

// ── L3Disconnect ───────────────────────────────────────────────────────

Expected<L3Disconnect> L3Disconnect::parse(BitReader& br) {
    L3Disconnect msg;

    // Cause TLV (IEI=0x08, but old code used try_parseTLV which reads IEI+length+value)
    auto ieiRes = detail::readIEI(br);
    if (!ieiRes) return Expected<L3Disconnect>::error(ieiRes.error());
    // Old code: L3CauseElement cause; cause.try_parseTLV(0x08, src, rp);
    // The try_parseTLV reads IEI (checking 0x08), then length, then value.
    // We already read IEI above, so just read length + value.
    auto lenRes = detail::readLength(br);
    if (!lenRes) return Expected<L3Disconnect>::error(lenRes.error());
    auto p = L3CauseElement::parse(br);
    if (!p) return Expected<L3Disconnect>::error(p.error());
    msg.mCause = p.value().cause();
    msg.mLocation = p.value().location();

    return Expected<L3Disconnect>::hold(std::move(msg));
}

void L3Disconnect::write(BitWriter& bw) const {
    bw.writeField(0x08, 8);
    bw.writeField(static_cast<uint32_t>(L3CauseElement::lengthV()), 8);
    L3CauseElement cause(mCause, mLocation);
    cause.write(bw);
}

void L3Disconnect::text(std::ostream& os) const {
    os << "Disconnect: TI=" << mTI << " cause=" << CCCause2Str(mCause) << " loc=" << static_cast<int>(mLocation);
}

// ── L3Release ──────────────────────────────────────────────────────────

L3Release::Builder L3Release::builder(unsigned ti) {
    return Builder{ti};
}

L3Release L3Release::Builder::build() const {
    L3Release msg(m_ti);
    if (m_haveCause) {
        msg.mHaveCause = true;
        msg.mCause = m_cause;
    }
    return msg;
}

Expected<L3Release> L3Release::parse(BitReader& br) {
    L3Release msg;

    // Cause TLV (IEI=0x08)
    while (br.hasMore()) {
        uint8_t peek = static_cast<uint8_t>(br.peekField(8));
        if (peek == 0x08) {
            auto ieiRes = detail::readIEI(br);
            if (!ieiRes) return Expected<L3Release>::error(ieiRes.error());
            auto lenRes = detail::readLength(br);
            if (!lenRes) return Expected<L3Release>::error(lenRes.error());
            auto p = L3CauseElement::parse(br);
            if (!p) return Expected<L3Release>::error(p.error());
            msg.mCause = p.value().cause();
            msg.mHaveCause = true;
        } else {
            break;
        }
    }

    auto ccRes = detail::ccCommonParse(br, msg.mHaveFacility, msg.mFacility, msg.mHaveSSVersion, msg.mSSVersion);
    if (!ccRes) return Expected<L3Release>::error(ccRes.error());

    return Expected<L3Release>::hold(std::move(msg));
}

void L3Release::write(BitWriter& bw) const {
    if (mHaveCause) {
        bw.writeField(0x08, 8);
        bw.writeField(static_cast<uint32_t>(L3CauseElement::lengthV()), 8);
        L3CauseElement cause(mCause, CCCauseLocation::Private_Serving_Local);
        cause.write(bw);
    }
    detail::ccCommonWrite(bw, mHaveFacility, mFacility, mHaveSSVersion, mSSVersion);
}

size_t L3Release::bodyLength() const {
    size_t sum = 0;
    if (mHaveCause) sum += 2 + L3CauseElement::lengthV();
    sum += detail::ccCommonLength(mHaveFacility, mFacility, mHaveSSVersion);
    return sum;
}

void L3Release::text(std::ostream& os) const {
    os << "Release: TI=" << mTI;
    if (mHaveCause) os << " cause=" << CCCause2Str(mCause);
    detail::ccCommonText(os, mHaveFacility, mFacility, mHaveSSVersion, mSSVersion);
}

// ── L3ReleaseComplete ──────────────────────────────────────────────────

L3ReleaseComplete::Builder L3ReleaseComplete::builder(unsigned ti) {
    return Builder{ti};
}

L3ReleaseComplete L3ReleaseComplete::Builder::build() const {
    L3ReleaseComplete msg(m_ti);
    if (m_haveCause) {
        msg.mHaveCause = true;
        msg.mCause = m_cause;
    }
    return msg;
}

Expected<L3ReleaseComplete> L3ReleaseComplete::parse(BitReader& br) {
    L3ReleaseComplete msg;

    while (br.hasMore()) {
        uint8_t peek = static_cast<uint8_t>(br.peekField(8));
        if (peek == 0x08) {
            auto ieiRes = detail::readIEI(br);
            if (!ieiRes) return Expected<L3ReleaseComplete>::error(ieiRes.error());
            auto lenRes = detail::readLength(br);
            if (!lenRes) return Expected<L3ReleaseComplete>::error(lenRes.error());
            auto p = L3CauseElement::parse(br);
            if (!p) return Expected<L3ReleaseComplete>::error(p.error());
            msg.mCause = p.value().cause();
            msg.mHaveCause = true;
        } else {
            break;
        }
    }

    auto ccRes = detail::ccCommonParse(br, msg.mHaveFacility, msg.mFacility, msg.mHaveSSVersion, msg.mSSVersion);
    if (!ccRes) return Expected<L3ReleaseComplete>::error(ccRes.error());

    return Expected<L3ReleaseComplete>::hold(std::move(msg));
}

void L3ReleaseComplete::write(BitWriter& bw) const {
    if (mHaveCause) {
        bw.writeField(0x08, 8);
        bw.writeField(static_cast<uint32_t>(L3CauseElement::lengthV()), 8);
        L3CauseElement cause(mCause, CCCauseLocation::Private_Serving_Local);
        cause.write(bw);
    }
    detail::ccCommonWrite(bw, mHaveFacility, mFacility, mHaveSSVersion, mSSVersion);
}

size_t L3ReleaseComplete::bodyLength() const {
    size_t sum = 0;
    if (mHaveCause) sum += 2 + L3CauseElement::lengthV();
    sum += detail::ccCommonLength(mHaveFacility, mFacility, mHaveSSVersion);
    return sum;
}

void L3ReleaseComplete::text(std::ostream& os) const {
    os << "ReleaseComplete: TI=" << mTI;
    if (mHaveCause) os << " cause=" << CCCause2Str(mCause);
    detail::ccCommonText(os, mHaveFacility, mFacility, mHaveSSVersion, mSSVersion);
}

// ── L3CCStatus ─────────────────────────────────────────────────────────

L3CCStatus::Builder L3CCStatus::builder(unsigned ti) {
    return Builder{ti};
}

L3CCStatus L3CCStatus::Builder::build() const {
    L3CCStatus msg;
    msg.mTI = m_ti;
    if (m_haveCause) {
        msg.mCause = m_cause;
    }
    msg.mCallState = m_callState;
    return msg;
}

Expected<L3CCStatus> L3CCStatus::parse(BitReader& br) {
    L3CCStatus msg;

    // Cause TLV (IEI=0x08)
    auto ieiRes = detail::readIEI(br);
    if (!ieiRes) return Expected<L3CCStatus>::error(ieiRes.error());
    auto lenRes = detail::readLength(br);
    if (!lenRes) return Expected<L3CCStatus>::error(lenRes.error());
    auto p = L3CauseElement::parse(br);
    if (!p) return Expected<L3CCStatus>::error(p.error());
    msg.mCause = p.value().cause();

    // CallState V (no IEI, just value)
    auto cs = L3CallState::parse(br);
    if (!cs) return Expected<L3CCStatus>::error(cs.error());
    msg.mCallState = cs.value().callState();

    return Expected<L3CCStatus>::hold(std::move(msg));
}

void L3CCStatus::write(BitWriter& bw) const {
    bw.writeField(0x08, 8);
    bw.writeField(static_cast<uint32_t>(L3CauseElement::lengthV()), 8);
    L3CauseElement cause(mCause, CCCauseLocation::Private_Serving_Local);
    cause.write(bw);
    L3CallState state(mCallState);
    state.write(bw);
}

void L3CCStatus::text(std::ostream& os) const {
    os << "CCStatus: TI=" << mTI << " cause=" << CCCause2Str(mCause) << " state=" << mCallState;
}

// ── L3StartDTMF ────────────────────────────────────────────────────────

Expected<L3StartDTMF> L3StartDTMF::parse(BitReader& br) {
    L3StartDTMF msg;
    // KeypadFacility TV (IEI=0x2c)
    auto ieiRes = detail::readIEI(br);
    if (!ieiRes) return Expected<L3StartDTMF>::error(ieiRes.error());
    auto r = br.readField(8);
    if (!r) return Expected<L3StartDTMF>::error(r.error());
    msg.mKey = static_cast<char>(r.value());
    return Expected<L3StartDTMF>::hold(std::move(msg));
}

void L3StartDTMF::write(BitWriter& bw) const {
    bw.writeField(0x2c, 8);
    bw.writeField(static_cast<uint32_t>(static_cast<unsigned char>(mKey)), 8);
}

void L3StartDTMF::text(std::ostream& os) const {
    os << "StartDTMF: TI=" << mTI << " key=" << mKey;
}

// ── L3StopDTMF ─────────────────────────────────────────────────────────

Expected<L3StopDTMF> L3StopDTMF::parse(BitReader&) {
    return Expected<L3StopDTMF>::hold(L3StopDTMF());
}

void L3StopDTMF::write(BitWriter&) const {}

void L3StopDTMF::text(std::ostream& os) const {
    os << "StopDTMF: TI=" << mTI;
}

// ── L3StopDTMFAcknowledge ──────────────────────────────────────────────

Expected<L3StopDTMFAcknowledge> L3StopDTMFAcknowledge::parse(BitReader&) {
    return Expected<L3StopDTMFAcknowledge>::hold(L3StopDTMFAcknowledge());
}

void L3StopDTMFAcknowledge::write(BitWriter&) const {}

void L3StopDTMFAcknowledge::text(std::ostream& os) const {
    os << "StopDTMFAck: TI=" << mTI;
}

// ── L3StartDTMFAcknowledge ─────────────────────────────────────────────

Expected<L3StartDTMFAcknowledge> L3StartDTMFAcknowledge::parse(BitReader& br) {
    L3StartDTMFAcknowledge msg;
    // KeypadFacility TV (IEI=0x2c)
    auto ieiRes = detail::readIEI(br);
    if (!ieiRes) return Expected<L3StartDTMFAcknowledge>::error(ieiRes.error());
    auto kf = L3KeypadFacility::parse(br);
    if (!kf) return Expected<L3StartDTMFAcknowledge>::error(kf.error());
    msg.mKey = kf.value().ia5();
    return Expected<L3StartDTMFAcknowledge>::hold(std::move(msg));
}

void L3StartDTMFAcknowledge::write(BitWriter& bw) const {
    bw.writeField(0x2c, 8);
    L3KeypadFacility kf(mKey);
    kf.write(bw);
}

void L3StartDTMFAcknowledge::text(std::ostream& os) const {
    os << "StartDTMFAck: TI=" << mTI << " key=" << mKey;
}

// ── L3StartDTMFReject ──────────────────────────────────────────────────

Expected<L3StartDTMFReject> L3StartDTMFReject::parse(BitReader& br) {
    L3StartDTMFReject msg;
    // Cause LV (no IEI in old code - try_parseLV reads length + value)
    auto lenRes = detail::readLength(br);
    if (!lenRes) return Expected<L3StartDTMFReject>::error(lenRes.error());
    auto p = L3CauseElement::parse(br);
    if (!p) return Expected<L3StartDTMFReject>::error(p.error());
    msg.mCause = p.value().cause();
    return Expected<L3StartDTMFReject>::hold(std::move(msg));
}

void L3StartDTMFReject::write(BitWriter& bw) const {
    bw.writeField(static_cast<uint32_t>(L3CauseElement::lengthV()), 8);
    L3CauseElement cause(mCause, CCCauseLocation::Private_Serving_Local);
    cause.write(bw);
}

void L3StartDTMFReject::text(std::ostream& os) const {
    os << "StartDTMFReject: TI=" << mTI << " " << CCCause2Str(mCause);
}

// ── L3Hold ─────────────────────────────────────────────────────────────

Expected<L3Hold> L3Hold::parse(BitReader&) {
    return Expected<L3Hold>::hold(L3Hold());
}

void L3Hold::write(BitWriter&) const {}

void L3Hold::text(std::ostream& os) const {
    os << "Hold: TI=" << mTI;
}

// ── L3HoldReject ───────────────────────────────────────────────────────

Expected<L3HoldReject> L3HoldReject::parse(BitReader& br) {
    L3HoldReject msg;
    // Cause LV (no IEI - old code used try_parseLV)
    auto lenRes = detail::readLength(br);
    if (!lenRes) return Expected<L3HoldReject>::error(lenRes.error());
    auto p = L3CauseElement::parse(br);
    if (!p) return Expected<L3HoldReject>::error(p.error());
    msg.mCause = p.value().cause();
    return Expected<L3HoldReject>::hold(std::move(msg));
}

void L3HoldReject::write(BitWriter& bw) const {
    bw.writeField(static_cast<uint32_t>(L3CauseElement::lengthV()), 8);
    L3CauseElement cause(mCause, CCCauseLocation::Private_Serving_Local);
    cause.write(bw);
}

void L3HoldReject::text(std::ostream& os) const {
    os << "HoldReject: TI=" << mTI << " " << CCCause2Str(mCause);
}

// ── L3Progress ─────────────────────────────────────────────────────────

Expected<L3Progress> L3Progress::parse(BitReader&) {
    return Expected<L3Progress>::hold(L3Progress());
}

void L3Progress::write(BitWriter&) const {}

void L3Progress::text(std::ostream& os) const {
    os << "Progress: TI=" << mTI;
    mProgress.text(os);
}

} // namespace gsml3parser
