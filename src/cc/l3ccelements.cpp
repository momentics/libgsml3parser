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

#include "gsml3parser/cc/l3ccelements.h"
#include <sstream>
#include <iomanip>

namespace gsml3parser {

// ── L3BearerCapability ──────────────────────────────────────────────────

size_t L3BearerCapability::lengthV() const {
    return 1 + mOctet3a.size();
}

Expected<L3BearerCapability> L3BearerCapability::parse(BitReader& br) {
    L3BearerCapability result;

    auto r = br.readField(8); if (!r) return Expected<L3BearerCapability>::error(r.error());
    result.mOctet3 = static_cast<uint8_t>(r.value());

    if (result.mOctet3 & 0x10) {
        r = br.readField(8); if (!r) return Expected<L3BearerCapability>::error(r.error());
        result.mOctet3a.push_back(static_cast<uint8_t>(r.value()));
        r = br.readField(8); if (!r) return Expected<L3BearerCapability>::error(r.error());
        result.mOctet3a.push_back(static_cast<uint8_t>(r.value()));
    }

    return Expected<L3BearerCapability>::hold(std::move(result));
}

void L3BearerCapability::write(BitWriter& bw) const {
    bw.writeField(mOctet3, 8);
    for (const auto& b : mOctet3a) {
        bw.writeField(b, 8);
    }
}

void L3BearerCapability::text(std::ostream& os) const {
    os << "BearerCap[0x" << std::hex << std::setw(2) << std::setfill('0')
       << static_cast<int>(mOctet3);
    for (const auto& b : mOctet3a) {
        os << " 0x" << std::setw(2) << std::setfill('0') << static_cast<int>(b);
    }
    os << "]";
}

// ── L3SupportedCodecList ───────────────────────────────────────────────

size_t L3SupportedCodecList::lengthV() const {
    return (mGsmCodecs.empty() ? 0 : 2 + mGsmCodecs.size()) +
           (mUmtsCodecs.empty() ? 0 : 2 + mUmtsCodecs.size());
}

Expected<L3SupportedCodecList> L3SupportedCodecList::parse(BitReader& br, size_t lengthBytes) {
    L3SupportedCodecList result;
    size_t end = br.position() + lengthBytes * 8;

    while (br.position() + 16 <= end) {
        auto r = br.readField(8); if (!r) return Expected<L3SupportedCodecList>::error(r.error());
        unsigned sysId = r.value();
        r = br.readField(8); if (!r) return Expected<L3SupportedCodecList>::error(r.error());
        unsigned bitmapLen = r.value();

        size_t remainingBytes = (end - br.position()) / 8;
        unsigned fixedLen = bitmapLen;
        if (fixedLen > remainingBytes) fixedLen = static_cast<unsigned>(remainingBytes);

        if (sysId == 0) {
            for (unsigned i = 0; i < fixedLen; ++i) {
                r = br.readField(8); if (!r) return Expected<L3SupportedCodecList>::error(r.error());
                result.mGsmCodecs.push_back(static_cast<uint8_t>(r.value()));
            }
        } else if (sysId == 4) {
            for (unsigned i = 0; i < fixedLen; ++i) {
                r = br.readField(8); if (!r) return Expected<L3SupportedCodecList>::error(r.error());
                result.mUmtsCodecs.push_back(static_cast<uint8_t>(r.value()));
            }
        } else {
            for (unsigned i = 0; i < fixedLen; ++i) {
                r = br.readField(8); if (!r) return Expected<L3SupportedCodecList>::error(r.error());
            }
        }
    }

    return Expected<L3SupportedCodecList>::hold(std::move(result));
}

void L3SupportedCodecList::write(BitWriter& bw) const {
    if (!mGsmCodecs.empty()) {
        bw.writeField(0, 8);
        bw.writeField(static_cast<uint32_t>(mGsmCodecs.size()), 8);
        for (const auto& b : mGsmCodecs) {
            bw.writeField(b, 8);
        }
    }
    if (!mUmtsCodecs.empty()) {
        bw.writeField(4, 8);
        bw.writeField(static_cast<uint32_t>(mUmtsCodecs.size()), 8);
        for (const auto& b : mUmtsCodecs) {
            bw.writeField(b, 8);
        }
    }
}

void L3SupportedCodecList::text(std::ostream& os) const {
    os << "CodecList";
    if (!mGsmCodecs.empty()) {
        os << " GSM=";
        for (const auto& b : mGsmCodecs) {
            os << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
        }
    }
    if (!mUmtsCodecs.empty()) {
        os << " UMTS=";
        for (const auto& b : mUmtsCodecs) {
            os << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
        }
    }
}

// ── L3BCDDigits ─────────────────────────────────────────────────────────

static int bcdEncode(char c) {
    if (c == '*') return 10;
    if (c == '#') return 11;
    if (c >= '0' && c <= '9') return c - '0';
    return 0;
}

static char bcdDecode(int d) {
    if (d == 10) return '*';
    if (d == 11) return '#';
    if (d >= 0 && d <= 9) return static_cast<char>('0' + d);
    return '0';
}

L3BCDDigits::L3BCDDigits(const char* wDigits) {
    std::strncpy(mDigits, wDigits, maxDigits);
    mDigits[maxDigits] = '\0';
}

L3BCDDigits::L3BCDDigits(const L3BCDDigits& other) {
    std::memcpy(mDigits, other.mDigits, sizeof(mDigits));
}

L3BCDDigits& L3BCDDigits::operator=(const L3BCDDigits& other) {
    if (this != &other) {
        std::memcpy(mDigits, other.mDigits, sizeof(mDigits));
    }
    return *this;
}

Expected<void> L3BCDDigits::parse(BitReader& br, size_t numOctets, bool international) {
    mDigits[0] = '\0';
    size_t i = 0;
    if (international && i < maxDigits) mDigits[i++] = '+';

    for (size_t readOctets = 0; readOctets < numOctets && i < maxDigits; ++readOctets) {
        auto r = br.readField(4); if (!r) return Expected<void>::error(r.error()); unsigned d2 = r.value();
        r = br.readField(4); if (!r) return Expected<void>::error(r.error()); unsigned d1 = r.value();
        mDigits[i++] = bcdDecode(d1);
        if (d2 != 0x0f && i < maxDigits) mDigits[i++] = bcdDecode(d2);
    }
    mDigits[i] = '\0';

    return Expected<void>::hold();
}

void L3BCDDigits::write(BitWriter& bw) const {
    unsigned index = 0;
    unsigned numDigits = static_cast<unsigned>(std::strlen(mDigits));
    if (index < numDigits && mDigits[index] == '+') index++;
    while (index < numDigits) {
        if ((index + 1) < numDigits)
            bw.writeField(bcdEncode(mDigits[index + 1]), 4);
        else
            bw.writeField(0x0f, 4);
        bw.writeField(bcdEncode(mDigits[index]), 4);
        index += 2;
    }
}

size_t L3BCDDigits::lengthV() const {
    unsigned sz = static_cast<unsigned>(std::strlen(mDigits));
    if (sz > 0 && mDigits[0] == '+') sz--;
    return (sz / 2) + (sz % 2);
}

std::ostream& operator<<(std::ostream& os, const L3BCDDigits& digits) {
    os << digits.digits();
    return os;
}

// ── L3CalledPartyBCDNumber ─────────────────────────────────────────────

L3CalledPartyBCDNumber::L3CalledPartyBCDNumber(const char* wDigits)
    : mPlan(NumberingPlan::E164) {
    if (wDigits[0] == '+') {
        mType = TypeOfNumber::International;
        mDigits = L3BCDDigits(wDigits + 1);
    } else {
        mType = TypeOfNumber::Unknown;
        mPlan = NumberingPlan::Unknown;
        mDigits = L3BCDDigits(wDigits);
    }
}

size_t L3CalledPartyBCDNumber::lengthV() const {
    return 1 + mDigits.lengthV();
}

Expected<L3CalledPartyBCDNumber> L3CalledPartyBCDNumber::parse(BitReader& br, size_t lengthBytes) {
    L3CalledPartyBCDNumber result;

    auto r = br.readField(1); if (!r) return Expected<L3CalledPartyBCDNumber>::error(r.error()); // spare
    r = br.readField(3); if (!r) return Expected<L3CalledPartyBCDNumber>::error(r.error());
    result.mType = static_cast<TypeOfNumber>(r.value());
    r = br.readField(4); if (!r) return Expected<L3CalledPartyBCDNumber>::error(r.error());
    result.mPlan = static_cast<NumberingPlan>(r.value());

    size_t digitBytes = lengthBytes - 1;
    auto res = result.mDigits.parse(br, digitBytes, result.mType == TypeOfNumber::International);
    if (!res) return Expected<L3CalledPartyBCDNumber>::error(res.error());

    return Expected<L3CalledPartyBCDNumber>::hold(std::move(result));
}

void L3CalledPartyBCDNumber::write(BitWriter& bw) const {
    bw.writeField(1, 1);
    bw.writeField(static_cast<uint32_t>(mType), 3);
    bw.writeField(static_cast<uint32_t>(mPlan), 4);
    mDigits.write(bw);
}

void L3CalledPartyBCDNumber::text(std::ostream& os) const {
    os << "CalledParty[" << mDigits.digits() << "]";
}

// ── L3CallingPartyBCDNumber ─────────────────────────────────────────────

L3CallingPartyBCDNumber::L3CallingPartyBCDNumber(const char* wDigits)
    : mPlan(NumberingPlan::E164), mDigits(wDigits) {
    mType = (wDigits[0] == '+') ? TypeOfNumber::International : TypeOfNumber::National;
}

size_t L3CallingPartyBCDNumber::lengthV() const {
    return 1 + mDigits.lengthV() + (mHaveOctet3a ? 1 : 0);
}

Expected<L3CallingPartyBCDNumber> L3CallingPartyBCDNumber::parse(BitReader& br, size_t lengthBytes) {
    L3CallingPartyBCDNumber result;

    auto r = br.readField(1); if (!r) return Expected<L3CallingPartyBCDNumber>::error(r.error());
    result.mHaveOctet3a = !r.value();
    r = br.readField(3); if (!r) return Expected<L3CallingPartyBCDNumber>::error(r.error());
    result.mType = static_cast<TypeOfNumber>(r.value());
    r = br.readField(4); if (!r) return Expected<L3CallingPartyBCDNumber>::error(r.error());
    result.mPlan = static_cast<NumberingPlan>(r.value());

    size_t remainingLength = lengthBytes - 1;
    if (result.mHaveOctet3a) {
        r = br.readField(1); if (!r) return Expected<L3CallingPartyBCDNumber>::error(r.error()); // spare
        r = br.readField(2); if (!r) return Expected<L3CallingPartyBCDNumber>::error(r.error());
        result.mPresentationIndicator = r.value();
        r = br.readField(3); if (!r) return Expected<L3CallingPartyBCDNumber>::error(r.error()); // spare
        r = br.readField(2); if (!r) return Expected<L3CallingPartyBCDNumber>::error(r.error());
        result.mScreeningIndicator = r.value();
        remainingLength -= 1;
    }

    auto res = result.mDigits.parse(br, remainingLength, result.mType == TypeOfNumber::International);
    if (!res) return Expected<L3CallingPartyBCDNumber>::error(res.error());

    return Expected<L3CallingPartyBCDNumber>::hold(std::move(result));
}

void L3CallingPartyBCDNumber::write(BitWriter& bw) const {
    bw.writeField(mHaveOctet3a ? 0 : 1, 1);
    bw.writeField(static_cast<uint32_t>(mType), 3);
    bw.writeField(static_cast<uint32_t>(mPlan), 4);
    if (mHaveOctet3a) {
        bw.writeField(1, 1);
        bw.writeField(mPresentationIndicator, 2);
        bw.writeField(0, 3);
        bw.writeField(mScreeningIndicator, 2);
    }
    mDigits.write(bw);
}

void L3CallingPartyBCDNumber::text(std::ostream& os) const {
    os << "CallingParty[" << mDigits.digits() << "]";
}

// ── L3CauseElement ─────────────────────────────────────────────────────

L3CauseElement::L3CauseElement(Cause wCause, Location wLocation)
    : mLocation(wLocation), mCause(wCause) {}

Expected<L3CauseElement> L3CauseElement::parse(BitReader& br) {
    L3CauseElement result;

    auto r = br.readField(4); if (!r) return Expected<L3CauseElement>::error(r.error());
    result.mLocation = static_cast<Location>(r.value());
    r = br.readField(1); if (!r) return Expected<L3CauseElement>::error(r.error()); // spare
    r = br.readField(2); if (!r) return Expected<L3CauseElement>::error(r.error()); // coding standard
    r = br.readField(1); if (!r) return Expected<L3CauseElement>::error(r.error()); // ext
    r = br.readField(7); if (!r) return Expected<L3CauseElement>::error(r.error());
    result.mCause = static_cast<Cause>(r.value());
    r = br.readField(1); if (!r) return Expected<L3CauseElement>::error(r.error()); // ext

    return Expected<L3CauseElement>::hold(std::move(result));
}

void L3CauseElement::write(BitWriter& bw) const {
    bw.writeField(static_cast<uint32_t>(mLocation), 4);
    bw.writeField(0, 1);
    bw.writeField(0x3, 2);
    bw.writeField(0, 1);
    bw.writeField(static_cast<uint32_t>(mCause), 7);
    bw.writeField(1, 1);
}

void L3CauseElement::text(std::ostream& os) const {
    os << "Cause[" << CCCause2Str(mCause) << "]";
}

// ── L3CallState ─────────────────────────────────────────────────────────

L3CallState::L3CallState(unsigned wCallState) : mCallState(wCallState) {}

Expected<L3CallState> L3CallState::parse(BitReader& br) {
    L3CallState result;

    auto r = br.readField(2); if (!r) return Expected<L3CallState>::error(r.error()); // spare
    r = br.readField(6); if (!r) return Expected<L3CallState>::error(r.error());
    result.mCallState = r.value();

    return Expected<L3CallState>::hold(std::move(result));
}

void L3CallState::write(BitWriter& bw) const {
    bw.writeField(3, 2);
    bw.writeField(mCallState, 6);
}

void L3CallState::text(std::ostream& os) const {
    os << "CallState[" << mCallState << "]";
}

// ── L3ProgressIndicator ────────────────────────────────────────────────

L3ProgressIndicator::L3ProgressIndicator(Progress wProgress, Location wLocation)
    : mLocation(wLocation), mProgress(wProgress) {}

Expected<L3ProgressIndicator> L3ProgressIndicator::parse(BitReader& br) {
    L3ProgressIndicator result;

    auto r = br.readField(4); if (!r) return Expected<L3ProgressIndicator>::error(r.error()); // IEI
    r = br.readField(4); if (!r) return Expected<L3ProgressIndicator>::error(r.error());
    result.mLocation = static_cast<Location>(r.value());
    r = br.readField(1); if (!r) return Expected<L3ProgressIndicator>::error(r.error()); // spare
    r = br.readField(7); if (!r) return Expected<L3ProgressIndicator>::error(r.error());
    result.mProgress = static_cast<Progress>(r.value());

    return Expected<L3ProgressIndicator>::hold(std::move(result));
}

void L3ProgressIndicator::write(BitWriter& bw) const {
    bw.writeField(0x0e, 4);
    bw.writeField(static_cast<uint32_t>(mLocation), 4);
    bw.writeField(1, 1);
    bw.writeField(static_cast<uint32_t>(mProgress), 7);
}

void L3ProgressIndicator::text(std::ostream& os) const {
    os << "Progress[location=" << mLocation
        << " progress=" << mProgress << "]";
}

// ── L3KeypadFacility ───────────────────────────────────────────────────

L3KeypadFacility::L3KeypadFacility(char wIA5) : mIA5(wIA5) {}

Expected<L3KeypadFacility> L3KeypadFacility::parse(BitReader& br) {
    auto r = br.readField(8); if (!r) return Expected<L3KeypadFacility>::error(r.error());
    return Expected<L3KeypadFacility>::hold(L3KeypadFacility(static_cast<char>(r.value())));
}

void L3KeypadFacility::write(BitWriter& bw) const {
    bw.writeField(static_cast<uint32_t>(mIA5), 8);
}

void L3KeypadFacility::text(std::ostream& os) const {
    os << "Keypad[" << mIA5 << "]";
}

// ── L3Signal ────────────────────────────────────────────────────────────

L3Signal::L3Signal(SignalValues tone) : mSignalValue(tone) {}

Expected<L3Signal> L3Signal::parse(BitReader& br) {
    auto r = br.readField(8); if (!r) return Expected<L3Signal>::error(r.error());
    return Expected<L3Signal>::hold(L3Signal(static_cast<SignalValues>(r.value())));
}

void L3Signal::write(BitWriter& bw) const {
    bw.writeField(static_cast<uint32_t>(mSignalValue), 8);
}

void L3Signal::text(std::ostream& os) const {
    os << "Signal[0x" << std::hex << std::setw(2) << std::setfill('0')
        << static_cast<int>(mSignalValue) << "]";
}

// ── L3RepeatIndicator ─────────────────────────────────────────────────

L3RepeatIndicator::L3RepeatIndicator(unsigned wValue) : mValue(wValue) {}

Expected<L3RepeatIndicator> L3RepeatIndicator::parse(BitReader& br) {
    auto r = br.readField(4); if (!r) return Expected<L3RepeatIndicator>::error(r.error());
    return Expected<L3RepeatIndicator>::hold(L3RepeatIndicator(r.value()));
}

void L3RepeatIndicator::write(BitWriter& bw) const {
    bw.writeField(mValue, 4);
}

void L3RepeatIndicator::text(std::ostream& os) const {
    os << "RepeatIndicator: " << mValue;
}

// ── L3SupServFacilityIE ─────────────────────────────────────────────────

L3SupServFacilityIE::L3SupServFacilityIE(const std::string& wData)
    : mData(wData) {}

size_t L3SupServFacilityIE::lengthV() const {
    return mData.size();
}

Expected<L3SupServFacilityIE> L3SupServFacilityIE::parse(BitReader& br, size_t lengthBytes) {
    std::string data;
    data.reserve(lengthBytes);
    for (size_t i = 0; i < lengthBytes; ++i) {
        auto r = br.readField(8); if (!r) return Expected<L3SupServFacilityIE>::error(r.error());
        data.push_back(static_cast<char>(r.value()));
    }
    return Expected<L3SupServFacilityIE>::hold(L3SupServFacilityIE(data));
}

Expected<L3SupServFacilityIE> L3SupServFacilityIE::parse(BitReader& br) {
    size_t remainingBits = br.remainingBits();
    size_t lengthBytes = remainingBits / 8;
    return parse(br, lengthBytes);
}

void L3SupServFacilityIE::write(BitWriter& bw) const {
    for (size_t i = 0; i < mData.size(); ++i) {
        bw.writeField(static_cast<uint32_t>(static_cast<unsigned char>(mData[i])), 8);
    }
}

void L3SupServFacilityIE::text(std::ostream& os) const {
    os << "Facility[";
    for (size_t i = 0; i < mData.size(); ++i) {
        os << std::hex << std::setw(2) << std::setfill('0')
           << static_cast<int>(static_cast<unsigned char>(mData[i]));
    }
    os << "]";
}

// ── L3SupServVersionIndicator ───────────────────────────────────────────

Expected<L3SupServVersionIndicator> L3SupServVersionIndicator::parse(BitReader& br) {
    auto r = br.readField(8); if (!r) return Expected<L3SupServVersionIndicator>::error(r.error());
    L3SupServVersionIndicator result;
    result.mVersion = r.value();
    return Expected<L3SupServVersionIndicator>::hold(std::move(result));
}

void L3SupServVersionIndicator::write(BitWriter& bw) const {
    bw.writeField(mVersion, 8);
}

void L3SupServVersionIndicator::text(std::ostream& os) const {
    os << "SSVersion[" << mVersion << "]";
}

// ── L3ConnectedNumber ──────────────────────────────────────────────────

L3ConnectedNumber::L3ConnectedNumber(const char* wDigits)
    : mPlan(NumberingPlan::E164) {
    if (wDigits[0] == '+') {
        mType = TypeOfNumber::International;
        mDigits = L3BCDDigits(wDigits + 1);
    } else {
        mType = TypeOfNumber::Unknown;
        mPlan = NumberingPlan::Unknown;
        mDigits = L3BCDDigits(wDigits);
    }
}

size_t L3ConnectedNumber::lengthV() const {
    return 1 + mDigits.lengthV();
}

Expected<L3ConnectedNumber> L3ConnectedNumber::parse(BitReader& br, size_t lengthBytes) {
    L3ConnectedNumber result;

    auto r = br.readField(1); if (!r) return Expected<L3ConnectedNumber>::error(r.error()); // spare
    r = br.readField(3); if (!r) return Expected<L3ConnectedNumber>::error(r.error());
    result.mType = static_cast<TypeOfNumber>(r.value());
    r = br.readField(4); if (!r) return Expected<L3ConnectedNumber>::error(r.error());
    result.mPlan = static_cast<NumberingPlan>(r.value());

    size_t digitBytes = lengthBytes - 1;
    auto res = result.mDigits.parse(br, digitBytes, result.mType == TypeOfNumber::International);
    if (!res) return Expected<L3ConnectedNumber>::error(res.error());

    return Expected<L3ConnectedNumber>::hold(std::move(result));
}

void L3ConnectedNumber::write(BitWriter& bw) const {
    bw.writeField(1, 1);
    bw.writeField(static_cast<uint32_t>(mType), 3);
    bw.writeField(static_cast<uint32_t>(mPlan), 4);
    mDigits.write(bw);
}

void L3ConnectedNumber::text(std::ostream& os) const {
    os << "ConnectedNumber[" << mDigits.digits() << "]";
}

// ── L3SubAddress ───────────────────────────────────────────────────────

size_t L3SubAddress::lengthV() const {
    size_t len = 1;
    for (const auto& item : mItems) {
        len += 2 + item.len;
    }
    return len;
}

Expected<L3SubAddress> L3SubAddress::parse(BitReader& br, size_t lengthBytes) {
    L3SubAddress result;

    auto r = br.readField(8); if (!r) return Expected<L3SubAddress>::error(r.error());
    unsigned numItems = r.value();

    size_t remaining = lengthBytes - 1;
    for (unsigned i = 0; i < numItems && remaining >= 2; ++i) {
        SubAddressItem item;
        r = br.readField(8); if (!r) return Expected<L3SubAddress>::error(r.error());
        unsigned octet = r.value();
        item.sel = static_cast<Selector>((octet >> 5) & 0x07);
        item.len = static_cast<uint8_t>(octet & 0x1f);
        remaining--;

        if (item.len > remaining) item.len = static_cast<uint8_t>(remaining);
        item.data.resize(item.len);
        for (size_t j = 0; j < item.len; ++j) {
            r = br.readField(8); if (!r) return Expected<L3SubAddress>::error(r.error());
            item.data[j] = static_cast<uint8_t>(r.value());
        }
        remaining -= item.len;

        result.mItems.push_back(std::move(item));
    }

    return Expected<L3SubAddress>::hold(std::move(result));
}

void L3SubAddress::write(BitWriter& bw) const {
    bw.writeField(static_cast<uint32_t>(mItems.size()), 8);
    for (const auto& item : mItems) {
        bw.writeField(((item.sel & 0x07) << 5) | (item.len & 0x1f), 8);
        for (const auto& b : item.data) {
            bw.writeField(b, 8);
        }
    }
}

void L3SubAddress::text(std::ostream& os) const {
    os << "SubAddress[" << mItems.size() << " items]";
}

// ── L3RedirectingNumber ────────────────────────────────────────────────

L3RedirectingNumber::L3RedirectingNumber(const char* wDigits)
    : mPlan(NumberingPlan::E164) {
    if (wDigits[0] == '+') {
        mType = TypeOfNumber::International;
        mDigits = L3BCDDigits(wDigits + 1);
    } else {
        mType = TypeOfNumber::Unknown;
        mPlan = NumberingPlan::Unknown;
        mDigits = L3BCDDigits(wDigits);
    }
}

size_t L3RedirectingNumber::lengthV() const {
    return 1 + mDigits.lengthV() + (mHaveReason ? 1 : 0);
}

Expected<L3RedirectingNumber> L3RedirectingNumber::parse(BitReader& br, size_t lengthBytes) {
    L3RedirectingNumber result;

    auto r = br.readField(1); if (!r) return Expected<L3RedirectingNumber>::error(r.error());
    bool haveReasonBit = !r.value();
    r = br.readField(3); if (!r) return Expected<L3RedirectingNumber>::error(r.error());
    result.mType = static_cast<TypeOfNumber>(r.value());
    r = br.readField(4); if (!r) return Expected<L3RedirectingNumber>::error(r.error());
    result.mPlan = static_cast<NumberingPlan>(r.value());

    size_t remaining = lengthBytes - 1;
    if (haveReasonBit && remaining >= 2) {
        r = br.readField(8); if (!r) return Expected<L3RedirectingNumber>::error(r.error());
        result.mReason = static_cast<RedirectReason>(r.value() & 0x03);
        result.mHaveReason = true;
        remaining--;
    }

    auto res = result.mDigits.parse(br, remaining, result.mType == TypeOfNumber::International);
    if (!res) return Expected<L3RedirectingNumber>::error(res.error());

    return Expected<L3RedirectingNumber>::hold(std::move(result));
}

void L3RedirectingNumber::write(BitWriter& bw) const {
    bw.writeField(mHaveReason ? 0 : 1, 1);
    bw.writeField(static_cast<uint32_t>(mType), 3);
    bw.writeField(static_cast<uint32_t>(mPlan), 4);
    if (mHaveReason) {
        bw.writeField(static_cast<uint32_t>(mReason), 8);
    }
    mDigits.write(bw);
}

void L3RedirectingNumber::text(std::ostream& os) const {
    os << "RedirectingNumber[" << mDigits.digits() << "]";
    if (mHaveReason) os << " reason=" << static_cast<int>(mReason);
}

// ── L3CLIRSuppression ──────────────────────────────────────────────────

L3CLIRSuppression::L3CLIRSuppression(unsigned wValue) : mValue(wValue & 0x07) {}

Expected<L3CLIRSuppression> L3CLIRSuppression::parse(BitReader& br) {
    auto r = br.readField(8); if (!r) return Expected<L3CLIRSuppression>::error(r.error());
    return Expected<L3CLIRSuppression>::hold(L3CLIRSuppression(r.value()));
}

void L3CLIRSuppression::write(BitWriter& bw) const {
    bw.writeField(mValue, 8);
}

void L3CLIRSuppression::text(std::ostream& os) const {
    os << "CLIRSuppression[" << mValue << "]";
}

// ── L3CLIRInvocation ───────────────────────────────────────────────────

L3CLIRInvocation::L3CLIRInvocation(unsigned wValue) : mValue(wValue & 0x07) {}

Expected<L3CLIRInvocation> L3CLIRInvocation::parse(BitReader& br) {
    auto r = br.readField(8); if (!r) return Expected<L3CLIRInvocation>::error(r.error());
    return Expected<L3CLIRInvocation>::hold(L3CLIRInvocation(r.value()));
}

void L3CLIRInvocation::write(BitWriter& bw) const {
    bw.writeField(mValue, 8);
}

void L3CLIRInvocation::text(std::ostream& os) const {
    os << "CLIRInvocation[" << mValue << "]";
}

// ── L3NetworkCCCapabilities ────────────────────────────────────────────

size_t L3NetworkCCCapabilities::lengthV() const {
    return mCapabilities.size();
}

Expected<L3NetworkCCCapabilities> L3NetworkCCCapabilities::parse(BitReader& br, size_t lengthBytes) {
    L3NetworkCCCapabilities result;
    result.mCapabilities.resize(lengthBytes);
    for (size_t i = 0; i < lengthBytes; ++i) {
        auto r = br.readField(8); if (!r) return Expected<L3NetworkCCCapabilities>::error(r.error());
        result.mCapabilities[i] = static_cast<uint8_t>(r.value());
    }
    return Expected<L3NetworkCCCapabilities>::hold(std::move(result));
}

void L3NetworkCCCapabilities::write(BitWriter& bw) const {
    for (const auto& b : mCapabilities) {
        bw.writeField(b, 8);
    }
}

void L3NetworkCCCapabilities::text(std::ostream& os) const {
    os << "NetCCCaps[";
    for (size_t i = 0; i < mCapabilities.size(); ++i) {
        if (i > 0) os << " ";
        os << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(mCapabilities[i]);
    }
    os << "]";
}

// ── L3LowLayerCompatibility ────────────────────────────────────────────

size_t L3LowLayerCompatibility::lengthV() const {
    return mData.size();
}

Expected<L3LowLayerCompatibility> L3LowLayerCompatibility::parse(BitReader& br, size_t lengthBytes) {
    L3LowLayerCompatibility result;
    result.mData.resize(lengthBytes);
    for (size_t i = 0; i < lengthBytes; ++i) {
        auto r = br.readField(8); if (!r) return Expected<L3LowLayerCompatibility>::error(r.error());
        result.mData[i] = static_cast<uint8_t>(r.value());
    }
    return Expected<L3LowLayerCompatibility>::hold(std::move(result));
}

void L3LowLayerCompatibility::write(BitWriter& bw) const {
    for (const auto& b : mData) {
        bw.writeField(b, 8);
    }
}

void L3LowLayerCompatibility::text(std::ostream& os) const {
    os << "LLCompat[";
    for (size_t i = 0; i < mData.size(); ++i) {
        if (i > 0) os << " ";
        os << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(mData[i]);
    }
    os << "]";
}

// ── L3HighLayerCompatibility ───────────────────────────────────────────

size_t L3HighLayerCompatibility::lengthV() const {
    return mData.size();
}

Expected<L3HighLayerCompatibility> L3HighLayerCompatibility::parse(BitReader& br, size_t lengthBytes) {
    L3HighLayerCompatibility result;
    result.mData.resize(lengthBytes);
    for (size_t i = 0; i < lengthBytes; ++i) {
        auto r = br.readField(8); if (!r) return Expected<L3HighLayerCompatibility>::error(r.error());
        result.mData[i] = static_cast<uint8_t>(r.value());
    }
    return Expected<L3HighLayerCompatibility>::hold(std::move(result));
}

void L3HighLayerCompatibility::write(BitWriter& bw) const {
    for (const auto& b : mData) {
        bw.writeField(b, 8);
    }
}

void L3HighLayerCompatibility::text(std::ostream& os) const {
    os << "HLCompat[";
    for (size_t i = 0; i < mData.size(); ++i) {
        if (i > 0) os << " ";
        os << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(mData[i]);
    }
    os << "]";
}

// ── L3UserUser ─────────────────────────────────────────────────────────

size_t L3UserUser::lengthV() const {
    return mData.size();
}

Expected<L3UserUser> L3UserUser::parse(BitReader& br, size_t lengthBytes) {
    L3UserUser result;
    result.mData.resize(lengthBytes);
    for (size_t i = 0; i < lengthBytes; ++i) {
        auto r = br.readField(8); if (!r) return Expected<L3UserUser>::error(r.error());
        result.mData[i] = static_cast<uint8_t>(r.value());
    }
    return Expected<L3UserUser>::hold(std::move(result));
}

void L3UserUser::write(BitWriter& bw) const {
    for (const auto& b : mData) {
        bw.writeField(b, 8);
    }
}

void L3UserUser::text(std::ostream& os) const {
    os << "UserUser[";
    for (size_t i = 0; i < mData.size(); ++i) {
        if (i > 0) os << " ";
        os << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(mData[i]);
    }
    os << "]";
}

// ── L3Priority ─────────────────────────────────────────────────────────

L3Priority::L3Priority(unsigned level, bool request)
    : mPriorityLevel(level & 0x07), mRequest(request) {}

Expected<L3Priority> L3Priority::parse(BitReader& br) {
    auto r = br.readField(8); if (!r) return Expected<L3Priority>::error(r.error());
    unsigned val = r.value();
    L3Priority result;
    result.mRequest = !!(val & 0x40);
    result.mPriorityLevel = (val >> 3) & 0x07;
    return Expected<L3Priority>::hold(std::move(result));
}

void L3Priority::write(BitWriter& bw) const {
    uint8_t val = static_cast<uint8_t>((mPriorityLevel & 0x07) << 3 | (mRequest ? 0x40 : 0));
    bw.writeField(val, 8);
}

void L3Priority::text(std::ostream& os) const {
    os << "Priority[level=" << mPriorityLevel << " request=" << (mRequest ? 1 : 0) << "]";
}

// ── L3StreamIdentifier ─────────────────────────────────────────────────

L3StreamIdentifier::L3StreamIdentifier(unsigned id, bool vbs)
    : mStreamId(id & 0x0f), mVBS(vbs) {}

Expected<L3StreamIdentifier> L3StreamIdentifier::parse(BitReader& br) {
    auto r = br.readField(8); if (!r) return Expected<L3StreamIdentifier>::error(r.error());
    unsigned val = r.value();
    L3StreamIdentifier result;
    result.mVBS = !!(val & 0x10);
    result.mStreamId = val & 0x0f;
    return Expected<L3StreamIdentifier>::hold(std::move(result));
}

void L3StreamIdentifier::write(BitWriter& bw) const {
    uint8_t val = static_cast<uint8_t>((mVBS ? 0x10 : 0) | (mStreamId & 0x0f));
    bw.writeField(val, 8);
}

void L3StreamIdentifier::text(std::ostream& os) const {
    os << "StreamID[" << mStreamId << (mVBS ? " VBS" : " VGCS") << "]";
}

// ── L3AllowedActions ───────────────────────────────────────────────────

L3AllowedActions::L3AllowedActions(uint16_t flags) : mFlags(flags & 0x07ff) {}

Expected<L3AllowedActions> L3AllowedActions::parse(BitReader& br, size_t lengthBytes) {
    if (lengthBytes < 2) return Expected<L3AllowedActions>::error(
        ParseError{ParseError::Code::TruncatedInput, "AllowedActions requires at least 2 octets"});

    auto r = br.readField(8); if (!r) return Expected<L3AllowedActions>::error(r.error());
    uint16_t high = static_cast<uint16_t>(r.value());
    r = br.readField(8); if (!r) return Expected<L3AllowedActions>::error(r.error());
    uint16_t low = static_cast<uint16_t>(r.value());

    L3AllowedActions result;
    result.mFlags = (high << 8) | low;
    return Expected<L3AllowedActions>::hold(std::move(result));
}

void L3AllowedActions::write(BitWriter& bw) const {
    bw.writeField((mFlags >> 8) & 0xff, 8);
    bw.writeField(mFlags & 0xff, 8);
}

void L3AllowedActions::text(std::ostream& os) const {
    os << "AllowedActions[0x" << std::hex << std::setw(4) << std::setfill('0') << mFlags << "]";
}

// ── L3CCCapabilities ───────────────────────────────────────────────────

size_t L3CCCapabilities::lengthV() const {
    return mCapabilities.size();
}

Expected<L3CCCapabilities> L3CCCapabilities::parse(BitReader& br, size_t lengthBytes) {
    L3CCCapabilities result;
    result.mCapabilities.resize(lengthBytes);
    for (size_t i = 0; i < lengthBytes; ++i) {
        auto r = br.readField(8); if (!r) return Expected<L3CCCapabilities>::error(r.error());
        result.mCapabilities[i] = static_cast<uint8_t>(r.value());
    }
    return Expected<L3CCCapabilities>::hold(std::move(result));
}

void L3CCCapabilities::write(BitWriter& bw) const {
    for (const auto& b : mCapabilities) {
        bw.writeField(b, 8);
    }
}

void L3CCCapabilities::text(std::ostream& os) const {
    os << "CCCaps[";
    for (size_t i = 0; i < mCapabilities.size(); ++i) {
        if (i > 0) os << " ";
        os << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(mCapabilities[i]);
    }
    os << "]";
}

// ── L3BackupBearerCapability ───────────────────────────────────────────

size_t L3BackupBearerCapability::lengthV() const {
    return 1 + mOctet3a.size();
}

Expected<L3BackupBearerCapability> L3BackupBearerCapability::parse(BitReader& br) {
    L3BackupBearerCapability result;

    auto r = br.readField(8); if (!r) return Expected<L3BackupBearerCapability>::error(r.error());
    result.mOctet3 = static_cast<uint8_t>(r.value());

    if (result.mOctet3 & 0x10) {
        r = br.readField(8); if (!r) return Expected<L3BackupBearerCapability>::error(r.error());
        result.mOctet3a.push_back(static_cast<uint8_t>(r.value()));
        r = br.readField(8); if (!r) return Expected<L3BackupBearerCapability>::error(r.error());
        result.mOctet3a.push_back(static_cast<uint8_t>(r.value()));
    }

    return Expected<L3BackupBearerCapability>::hold(std::move(result));
}

void L3BackupBearerCapability::write(BitWriter& bw) const {
    bw.writeField(mOctet3, 8);
    for (const auto& b : mOctet3a) {
        bw.writeField(b, 8);
    }
}

void L3BackupBearerCapability::text(std::ostream& os) const {
    os << "BackupBearerCap[0x" << std::hex << std::setw(2) << std::setfill('0')
       << static_cast<int>(mOctet3);
    for (const auto& b : mOctet3a) {
        os << " 0x" << std::setw(2) << std::setfill('0') << static_cast<int>(b);
    }
    os << "]";
}

// ── SSOpCode name helper ───────────────────────────────────────────────

std::string_view ssOpCodeName(SSOpCode code) {
    switch (code) {
        case SSOpCode::RegisterSS: return "RegisterSS";
        case SSOpCode::EraseSS: return "EraseSS";
        case SSOpCode::ActivateSS: return "ActivateSS";
        case SSOpCode::DeactivateSS: return "DeactivateSS";
        case SSOpCode::InterrogateSS: return "InterrogateSS";
        case SSOpCode::NotifySS: return "NotifySS";
        case SSOpCode::RegisterPassword: return "RegisterPassword";
        case SSOpCode::GetPassword: return "GetPassword";
        case SSOpCode::ProcessUSSData: return "ProcessUSSData";
        case SSOpCode::ForwardCheckSSInd: return "ForwardCheckSSInd";
        case SSOpCode::ProcessUSSReq: return "ProcessUSSReq";
        case SSOpCode::USSRequest: return "USSRequest";
        case SSOpCode::USSNotify: return "USSNotify";
        case SSOpCode::ForwardCUGInfo: return "ForwardCUGInfo";
        case SSOpCode::SplitMPTY: return "SplitMPTY";
        case SSOpCode::RetrieveMPTY: return "RetrieveMPTY";
        case SSOpCode::HoldMPTY: return "HoldMPTY";
        case SSOpCode::BuildMPTY: return "BuildMPTY";
        case SSOpCode::ForwardChargeAdvice: return "ForwardChargeAdvice";
    }
    return "Unknown";
}

// ── SSErrorCode name helper ────────────────────────────────────────────

std::string_view ssErrorCodeName(SSErrorCode code) {
    switch (code) {
        case SSErrorCode::UnknownSubscriber: return "UnknownSubscriber";
        case SSErrorCode::IllegalSubscriber: return "IllegalSubscriber";
        case SSErrorCode::BearerServiceNotProvisioned: return "BearerServiceNotProvisioned";
        case SSErrorCode::TeleserviceNotProvisioned: return "TeleserviceNotProvisioned";
        case SSErrorCode::IllegalEquipment: return "IllegalEquipment";
        case SSErrorCode::CallBarred: return "CallBarred";
        case SSErrorCode::IllegalSSOperation: return "IllegalSSOperation";
        case SSErrorCode::SSErrorStatus: return "SSErrorStatus";
        case SSErrorCode::SSNotAvailable: return "SSNotAvailable";
        case SSErrorCode::SSSubscriptionViolation: return "SSSubscriptionViolation";
        case SSErrorCode::SSIncompatibility: return "SSIncompatibility";
        case SSErrorCode::FacilityNotSupported: return "FacilityNotSupported";
        case SSErrorCode::AbsentSubscriber: return "AbsentSubscriber";
        case SSErrorCode::SystemFailure: return "SystemFailure";
        case SSErrorCode::DataMissing: return "DataMissing";
        case SSErrorCode::UnexpectedDataValue: return "UnexpectedDataValue";
        case SSErrorCode::PWRegistrationFailure: return "PWRegistrationFailure";
        case SSErrorCode::NegativePWCheck: return "NegativePWCheck";
        case SSErrorCode::NumPWAttemptsViolation: return "NumPWAttemptsViolation";
        case SSErrorCode::UnknownAlphabet: return "UnknownAlphabet";
        case SSErrorCode::USSDBusy: return "USSDBusy";
        case SSErrorCode::MaxMPTYParticipants: return "MaxMPTYParticipants";
        case SSErrorCode::ResourcesNotAvailable: return "ResourcesNotAvailable";
    }
    return "Unknown";
}

// ── L3FacilityOpCode ───────────────────────────────────────────────────

L3FacilityOpCode::L3FacilityOpCode(ComponentType comp, int8_t invokeId, SSOpCode op, std::vector<uint8_t> params)
    : mComponent(comp), mInvokeId(invokeId), mOpCode(op), mParameters(std::move(params)) {}

L3FacilityOpCode::L3FacilityOpCode(ComponentType comp, int8_t invokeId, SSErrorCode err)
    : mComponent(comp), mInvokeId(invokeId), mErrorCode(err), mHasErrorCode(true) {}

size_t L3FacilityOpCode::lengthV() const {
    return 1 + 1 + (mParameters.empty() ? 0 : 1 + mParameters.size());
}

Expected<L3FacilityOpCode> L3FacilityOpCode::parse(const std::string& facilityData) {
    if (facilityData.size() < 2) {
        return Expected<L3FacilityOpCode>::error(
            ParseError{ParseError::Code::TruncatedInput, "SS Facility data too short for op_code"});
    }

    L3FacilityOpCode result;
    size_t pos = 0;

    result.mComponent = static_cast<ComponentType>(static_cast<uint8_t>(facilityData[pos++]));

    if (result.mComponent != Invoke && result.mComponent != ReturnResult &&
        result.mComponent != ReturnError && result.mComponent != Reject) {
        return Expected<L3FacilityOpCode>::error(
            ParseError{ParseError::Code::InvalidValue, "Unknown TCAP component tag in SS Facility"});
    }

    if (facilityData.size() < pos + 1) {
        return Expected<L3FacilityOpCode>::error(
            ParseError{ParseError::Code::TruncatedInput, "SS Facility data truncated at invoke ID"});
    }
    result.mInvokeId = static_cast<int8_t>(static_cast<uint8_t>(facilityData[pos++]));

    if (result.mComponent == Invoke) {
        if (facilityData.size() < pos + 1) {
            return Expected<L3FacilityOpCode>::error(
                ParseError{ParseError::Code::TruncatedInput, "SS Facility data truncated at op_code"});
        }
        result.mOpCode = static_cast<SSOpCode>(static_cast<uint8_t>(facilityData[pos++]));

        if (pos < facilityData.size()) {
            result.mParameters.assign(facilityData.begin() + pos, facilityData.end());
        }
    } else if (result.mComponent == ReturnError) {
        if (facilityData.size() < pos + 1) {
            return Expected<L3FacilityOpCode>::error(
                ParseError{ParseError::Code::TruncatedInput, "SS Facility data truncated at error code"});
        }
        result.mErrorCode = static_cast<SSErrorCode>(static_cast<uint8_t>(facilityData[pos++]));
        result.mHasErrorCode = true;

        if (pos < facilityData.size()) {
            result.mParameters.assign(facilityData.begin() + pos, facilityData.end());
        }
    } else if (result.mComponent == ReturnResult) {
        if (pos < facilityData.size()) {
            result.mParameters.assign(facilityData.begin() + pos, facilityData.end());
        }
    }

    return Expected<L3FacilityOpCode>::hold(std::move(result));
}

void L3FacilityOpCode::write(BitWriter& bw) const {
    bw.writeField(static_cast<uint32_t>(mComponent), 8);
    bw.writeField(mInvokeId & 0xFF, 8);

    if (mComponent == Invoke) {
        bw.writeField(static_cast<uint32_t>(mOpCode), 8);
        for (const auto& b : mParameters) {
            bw.writeField(b, 8);
        }
    } else if (mComponent == ReturnError) {
        if (mHasErrorCode) {
            bw.writeField(static_cast<uint32_t>(mErrorCode), 8);
        }
        for (const auto& b : mParameters) {
            bw.writeField(b, 8);
        }
    } else if (mComponent == ReturnResult) {
        for (const auto& b : mParameters) {
            bw.writeField(b, 8);
        }
    }
}

void L3FacilityOpCode::text(std::ostream& os) const {
    os << "SSFacility[";
    switch (mComponent) {
        case Invoke: os << "INVOKE"; break;
        case ReturnResult: os << "RETURN-RESULT"; break;
        case ReturnError: os << "RETURN-ERROR"; break;
        case Reject: os << "REJECT"; break;
    }
    os << " id=" << static_cast<int>(mInvokeId);
    if (mComponent == Invoke) {
        os << " op=" << ssOpCodeName(mOpCode);
    } else if (mComponent == ReturnError && mHasErrorCode) {
        os << " err=" << ssErrorCodeName(mErrorCode);
    }
    if (!mParameters.empty()) {
        os << " params=[";
        for (size_t i = 0; i < mParameters.size(); ++i) {
            if (i > 0) os << " ";
            os << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(mParameters[i]);
        }
        os << "]";
    }
    os << "]";
}

// ── GSM 7-bit alphabet decode/encode helpers ───────────────────────────

static std::string gsm7bitDecode(const std::vector<uint8_t>& data) {
    std::string result;
    uint32_t accumulator = 0;
    int bitCount = 0;

    for (size_t i = 0; i < data.size(); ++i) {
        accumulator |= (static_cast<uint32_t>(data[i]) << bitCount);
        bitCount += 8;

        while (bitCount >= 7) {
            unsigned c = accumulator & 0x7F;
            result += static_cast<char>(c);
            accumulator >>= 7;
            bitCount -= 7;
        }
    }

    return result;
}

static std::vector<uint8_t> gsm7bitEncode(const std::string& text) {
    std::vector<uint8_t> result;
    int bitPos = 0;
    uint32_t accumulator = 0;

    for (char c : text) {
        accumulator |= (static_cast<uint32_t>(c & 0x7F) << bitPos);
        bitPos += 7;

        while (bitPos >= 8) {
            result.push_back(static_cast<uint8_t>(accumulator & 0xFF));
            accumulator >>= 8;
            bitPos -= 8;
        }
    }

    if (bitPos > 0) {
        result.push_back(static_cast<uint8_t>(accumulator & 0xFF));
    }

    return result;
}

// ── L3USSDData ─────────────────────────────────────────────────────────

L3USSDData::L3USSDData(int8_t invokeId, SSOpCode op, uint8_t dcs, const std::vector<uint8_t>& ussdString, bool isResult)
    : mInvokeId(invokeId), mOpCode(op), mDcs(dcs), mRawUssdString(ussdString), mIsResult(isResult) {}

size_t L3USSDData::lengthV() const {
    return 1 + 1 + 1 + mRawUssdString.size();
}

Expected<L3USSDData> L3USSDData::parse(const std::string& facilityData, SSOpCode opCode) {
    (void)opCode; // op_code is read from data; parameter kept for API compatibility
    if (facilityData.size() < 3) {
        return Expected<L3USSDData>::error(
            ParseError{ParseError::Code::TruncatedInput, "SS Facility data too short for USSD"});
    }

    L3USSDData result;
    size_t pos = 0;

    result.mInvokeId = static_cast<int8_t>(static_cast<uint8_t>(facilityData[pos++]));
    result.mOpCode = static_cast<SSOpCode>(static_cast<uint8_t>(facilityData[pos++]));
    result.mDcs = static_cast<uint8_t>(facilityData[pos++]);

    if (pos < facilityData.size()) {
        result.mRawUssdString.assign(facilityData.begin() + pos, facilityData.end());
    }

    if (result.mOpCode == SSOpCode::USSNotify) {
        result.mIsResult = true;
    }

    return Expected<L3USSDData>::hold(std::move(result));
}

void L3USSDData::write(BitWriter& bw) const {
    bw.writeField(mInvokeId & 0xFF, 8);
    bw.writeField(static_cast<uint32_t>(mOpCode), 8);
    bw.writeField(mDcs, 8);
    for (const auto& b : mRawUssdString) {
        bw.writeField(b, 8);
    }
}

void L3USSDData::text(std::ostream& os) const {
    os << "USSD[id=" << static_cast<int>(mInvokeId)
       << " op=" << ssOpCodeName(mOpCode)
       << " dcs=0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(mDcs)
       << " text=\"" << decodeUssdString() << "\"]";
}

std::string L3USSDData::decodeUssdString() const {
    if (alphabet() == UCS2) {
        std::string result;
        for (size_t i = 0; i + 1 < mRawUssdString.size(); i += 2) {
            uint16_t cp = (static_cast<uint16_t>(mRawUssdString[i]) << 8) | mRawUssdString[i + 1];
            if (cp < 128) result += static_cast<char>(cp);
            else result += '?';
        }
        return result;
    }
    return gsm7bitDecode(mRawUssdString);
}

std::vector<uint8_t> L3USSDData::encodeUssdString(const std::string& text) {
    return gsm7bitEncode(text);
}

std::ostream& operator<<(std::ostream& os, L3ProgressIndicator::Location loc) {
    switch (loc) {
        case L3ProgressIndicator::Location::User:                 os << "User"; break;
        case L3ProgressIndicator::Location::PrivateServingLocal:  os << "PrivateServingLocal"; break;
        case L3ProgressIndicator::Location::PublicServingLocal:   os << "PublicServingLocal"; break;
        case L3ProgressIndicator::Location::PublicServingRemote:  os << "PublicServingRemote"; break;
        case L3ProgressIndicator::Location::PrivateServingRemote: os << "PrivateServingRemote"; break;
        case L3ProgressIndicator::Location::BeyondInternetworking:os << "BeyondInternetworking"; break;
        default: os << "Loc(" << static_cast<int>(loc) << ")"; break;
    }
    return os;
}

std::ostream& operator<<(std::ostream& os, L3ProgressIndicator::Progress prog) {
    switch (prog) {
        case L3ProgressIndicator::Progress::Unspecified:         os << "Unspecified"; break;
        case L3ProgressIndicator::Progress::NotISDN:             os << "NotISDN"; break;
        case L3ProgressIndicator::Progress::DestinationNotISDN:  os << "DestinationNotISDN"; break;
        case L3ProgressIndicator::Progress::OriginationNotISDN:  os << "OriginationNotISDN"; break;
        case L3ProgressIndicator::Progress::ReturnedToISDN:      os << "ReturnedToISDN"; break;
        case L3ProgressIndicator::Progress::InBandAvailable:     os << "InBandAvailable"; break;
        case L3ProgressIndicator::Progress::EndToEndISDN:        os << "EndToEndISDN"; break;
        case L3ProgressIndicator::Progress::Queuing:             os << "Queuing"; break;
        default: os << "Prog(" << static_cast<int>(prog) << ")"; break;
    }
    return os;
}

} // namespace gsml3parser
