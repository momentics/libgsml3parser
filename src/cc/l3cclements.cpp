#include "gsml3parser/cc/l3cclements.h"
#include "gsml3parser/common/l3common.h"
#include <cstring>
#include <sstream>
#include <iomanip>

namespace gsml3parser {

// ── L3BearerCapability ──────────────────────────────────────────────────

L3BearerCapability::L3BearerCapability()
    : mOctet3(0x0f), mPresent(false) {
    mOctet3a.push_back(0x80);
}

size_t L3BearerCapability::lengthV() const {
    return 1 + mOctet3a.size();
}

void L3BearerCapability::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, mOctet3, 8);
    for (const auto& b : mOctet3a) {
        dest.writeField(wp, b, 8);
    }
}

void L3BearerCapability::parseV(const L3Frame& src, size_t& rp) {
    mPresent = true;
    mOctet3 = static_cast<uint8_t>(src.readField(rp, 8));
    mOctet3a.clear();
    if (mOctet3 & 0x10) {
        mOctet3a.push_back(static_cast<uint8_t>(src.readField(rp, 8)));
        mOctet3a.push_back(static_cast<uint8_t>(src.readField(rp, 8)));
    }
}

void L3BearerCapability::parseV(const L3Frame& src, size_t& rp, size_t expectedLength) {
    mPresent = true;
    if (expectedLength == 0) return;
    size_t end = rp + 8 * expectedLength;
    unsigned octet3 = src.readField(rp, 8);
    if ((octet3 & 7) == 0) {
        mOctet3 = static_cast<uint8_t>(octet3);
        mOctet3a.clear();
        while (rp < end) {
            mOctet3a.push_back(static_cast<uint8_t>(src.readField(rp, 8)));
        }
    }
    rp = end;
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

L3SupportedCodecList::L3SupportedCodecList()
    : mGsmPresent(false), mUmtsPresent(false) {}

size_t L3SupportedCodecList::lengthV() const {
    return (mGsmPresent ? 4 : 0) + (mUmtsPresent ? 4 : 0);
}

void L3SupportedCodecList::writeV(L3Frame& dest, size_t& wp) const {
    if (mGsmPresent) {
        dest.writeField(wp, 0, 8);
        dest.writeField(wp, static_cast<unsigned>(mGsmCodecs.size()), 8);
        for (const auto& b : mGsmCodecs) {
            dest.writeField(wp, b, 8);
        }
    }
    if (mUmtsPresent) {
        dest.writeField(wp, 4, 8);
        dest.writeField(wp, static_cast<unsigned>(mUmtsCodecs.size()), 8);
        for (const auto& b : mUmtsCodecs) {
            dest.writeField(wp, b, 8);
        }
    }
}

void L3SupportedCodecList::parseV(const L3Frame& src, size_t& rp) {
    mGsmCodecs.clear();
    mUmtsCodecs.clear();
    while (rp + 16 <= src.size()) {
        unsigned sysId = src.readField(rp, 8);
        unsigned bitmapLen = src.readField(rp, 8);
        unsigned fixedLen = bitmapLen;
        if (fixedLen > (src.size() - rp) / 8) fixedLen = (src.size() - rp) / 8;
        for (unsigned i = 0; i < fixedLen; ++i) {
            unsigned byte = src.readField(rp, 8);
            if (sysId == 0) mGsmCodecs.push_back(static_cast<uint8_t>(byte));
            else if (sysId == 4) mUmtsCodecs.push_back(static_cast<uint8_t>(byte));
        }
    }
}

void L3SupportedCodecList::parseV(const L3Frame& src, size_t& rp, size_t expectedLength) {
    mGsmCodecs.clear();
    mUmtsCodecs.clear();
    size_t end = rp + 8 * expectedLength;
    while (rp + 16 <= end) {
        unsigned sysId = src.readField(rp, 8);
        unsigned bitmapLen = src.readField(rp, 8);
        unsigned fixedLen = bitmapLen;
        if (fixedLen > (end - rp) / 8) fixedLen = (end - rp) / 8;
        if (sysId == 0) {
            mGsmPresent = true;
            for (unsigned i = 0; i < fixedLen; ++i) {
                mGsmCodecs.push_back(static_cast<uint8_t>(src.readField(rp, 8)));
            }
        } else if (sysId == 4) {
            mUmtsPresent = true;
            for (unsigned i = 0; i < fixedLen; ++i) {
                mUmtsCodecs.push_back(static_cast<uint8_t>(src.readField(rp, 8)));
            }
        } else {
            rp += 8 * fixedLen;
        }
    }
}

void L3SupportedCodecList::text(std::ostream& os) const {
    os << "CodecList";
    if (mGsmPresent) {
        os << " GSM=";
        for (const auto& b : mGsmCodecs) {
            os << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
        }
    }
    if (mUmtsPresent) {
        os << " UMTS=";
        for (const auto& b : mUmtsCodecs) {
            os << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
        }
    }
}

// ── L3BCDDigits ─────────────────────────────────────────────────────────

L3BCDDigits::L3BCDDigits() {
    mDigits[0] = '\0';
}

L3BCDDigits::L3BCDDigits(const char* wDigits) {
    strncpy(mDigits, wDigits, sizeof(mDigits) - 1);
    mDigits[sizeof(mDigits) - 1] = '\0';
}

L3BCDDigits::L3BCDDigits(const L3BCDDigits& other) {
    memcpy(mDigits, other.mDigits, sizeof(mDigits));
}

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

void L3BCDDigits::parse(const L3Frame& src, size_t& rp, size_t numOctets, bool international) {
    unsigned i = 0;
    size_t readOctets = 0;
    if (international) mDigits[i++] = '+';
    while (readOctets < numOctets && i < maxDigits) {
        unsigned d2 = src.readField(rp, 4);
        unsigned d1 = src.readField(rp, 4);
        readOctets++;
        mDigits[i++] = bcdDecode(d1);
        if (d2 != 0x0f && i < maxDigits) mDigits[i++] = bcdDecode(d2);
    }
    mDigits[i] = '\0';
}

void L3BCDDigits::write(L3Frame& dest, size_t& wp) const {
    unsigned index = 0;
    unsigned numDigits = strlen(mDigits);
    if (index < numDigits && mDigits[index] == '+') index++;
    while (index < numDigits) {
        if ((index + 1) < numDigits)
            dest.writeField(wp, bcdEncode(mDigits[index + 1]), 4);
        else
            dest.writeField(wp, 0x0f, 4);
        dest.writeField(wp, bcdEncode(mDigits[index]), 4);
        index += 2;
    }
}

size_t L3BCDDigits::lengthV() const {
    unsigned sz = strlen(mDigits);
    if (sz > 0 && mDigits[0] == '+') sz--;
    return (sz / 2) + (sz % 2);
}

std::ostream& operator<<(std::ostream& os, const L3BCDDigits& digits) {
    os << digits.digits();
    return os;
}

// ── L3CalledPartyBCDNumber ─────────────────────────────────────────────

L3CalledPartyBCDNumber::L3CalledPartyBCDNumber()
    : mType(TypeOfNumber::Unknown), mPlan(NumberingPlan::Unknown) {}

L3CalledPartyBCDNumber::L3CalledPartyBCDNumber(const char* wDigits)
    : mPlan(NumberingPlan::E164), mDigits(wDigits) {
    mType = (wDigits[0] == '+') ? TypeOfNumber::International : TypeOfNumber::National;
}

size_t L3CalledPartyBCDNumber::lengthV() const {
    return 1 + mDigits.lengthV();
}

void L3CalledPartyBCDNumber::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, 1, 1);
    dest.writeField(wp, static_cast<unsigned>(mType), 3);
    dest.writeField(wp, static_cast<unsigned>(mPlan), 4);
    mDigits.write(dest, wp);
}

void L3CalledPartyBCDNumber::parseV(const L3Frame& src, size_t& rp) {
    src.readField(rp, 1);
    mType = static_cast<TypeOfNumber>(src.readField(rp, 3));
    mPlan = static_cast<NumberingPlan>(src.readField(rp, 4));
    size_t remaining = (src.size() - rp) / 8;
    mDigits.parse(src, rp, remaining, mType == TypeOfNumber::International);
}

void L3CalledPartyBCDNumber::parseV(const L3Frame& src, size_t& rp, size_t expectedLength) {
    src.readField(rp, 1);
    mType = static_cast<TypeOfNumber>(src.readField(rp, 3));
    mPlan = static_cast<NumberingPlan>(src.readField(rp, 4));
    mDigits.parse(src, rp, expectedLength - 1, mType == TypeOfNumber::International);
}

void L3CalledPartyBCDNumber::text(std::ostream& os) const {
    os << "CalledParty[" << mDigits.digits() << "]";
}

// ── L3CallingPartyBCDNumber ─────────────────────────────────────────────

L3CallingPartyBCDNumber::L3CallingPartyBCDNumber()
    : mType(TypeOfNumber::Unknown), mPlan(NumberingPlan::Unknown),
      mHaveOctet3a(false), mPresentationIndicator(0), mScreeningIndicator(0) {}

L3CallingPartyBCDNumber::L3CallingPartyBCDNumber(const char* wDigits)
    : mPlan(NumberingPlan::E164), mDigits(wDigits),
      mHaveOctet3a(false), mPresentationIndicator(0), mScreeningIndicator(0) {
    mType = (wDigits[0] == '+') ? TypeOfNumber::International : TypeOfNumber::National;
}

size_t L3CallingPartyBCDNumber::lengthV() const {
    return 1 + mDigits.lengthV() + (mHaveOctet3a ? 1 : 0);
}

void L3CallingPartyBCDNumber::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, mHaveOctet3a ? 0 : 1, 1);
    dest.writeField(wp, static_cast<unsigned>(mType), 3);
    dest.writeField(wp, static_cast<unsigned>(mPlan), 4);
    if (mHaveOctet3a) {
        dest.writeField(wp, 1, 1);
        dest.writeField(wp, mPresentationIndicator, 2);
        dest.writeField(wp, 0, 3);
        dest.writeField(wp, mScreeningIndicator, 2);
    }
    mDigits.write(dest, wp);
}

void L3CallingPartyBCDNumber::parseV(const L3Frame& src, size_t& rp) {
    mHaveOctet3a = !src.readField(rp, 1);
    mType = static_cast<TypeOfNumber>(src.readField(rp, 3));
    mPlan = static_cast<NumberingPlan>(src.readField(rp, 4));
    if (mHaveOctet3a) {
        src.readField(rp, 1);
        mPresentationIndicator = src.readField(rp, 2);
        src.readField(rp, 3);
        mScreeningIndicator = src.readField(rp, 2);
    }
    size_t remaining = (src.size() - rp) / 8;
    mDigits.parse(src, rp, remaining, mType == TypeOfNumber::International);
}

void L3CallingPartyBCDNumber::parseV(const L3Frame& src, size_t& rp, size_t expectedLength) {
    size_t remainingLength = expectedLength;
    mHaveOctet3a = !src.readField(rp, 1);
    mType = static_cast<TypeOfNumber>(src.readField(rp, 3));
    mPlan = static_cast<NumberingPlan>(src.readField(rp, 4));
    remainingLength -= 1;
    if (mHaveOctet3a) {
        src.readField(rp, 1);
        mPresentationIndicator = src.readField(rp, 2);
        src.readField(rp, 3);
        mScreeningIndicator = src.readField(rp, 2);
        remainingLength -= 1;
    }
    mDigits.parse(src, rp, remainingLength, mType == TypeOfNumber::International);
}

void L3CallingPartyBCDNumber::text(std::ostream& os) const {
    os << "CallingParty[" << mDigits.digits() << "]";
}

// ── L3CauseElement ─────────────────────────────────────────────────────

L3CauseElement::L3CauseElement(Cause wCause, Location wLocation)
    : mLocation(wLocation), mCause(wCause) {}

void L3CauseElement::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, 0x0e, 4);
    dest.writeField(wp, static_cast<unsigned>(mLocation), 4);
    dest.writeField(wp, 1, 1);
    dest.writeField(wp, static_cast<unsigned>(mCause), 7);
}

void L3CauseElement::parseV(const L3Frame& src, size_t& rp) {
    src.readField(rp, 4);
    mLocation = static_cast<Location>(src.readField(rp, 4));
    src.readField(rp, 1);
    mCause = static_cast<Cause>(src.readField(rp, 7));
}

void L3CauseElement::parseV(const L3Frame& src, size_t& rp, size_t expectedLength) {
    size_t pos = rp;
    rp += 8 * expectedLength;
    src.readField(pos, 4);
    mLocation = static_cast<Location>(src.readField(pos, 4));
    src.readField(pos, 1);
    mCause = static_cast<Cause>(src.readField(pos, 7));
}

void L3CauseElement::text(std::ostream& os) const {
    os << "Cause[" << CCCause2Str(mCause) << "]";
}

// ── L3CallState ─────────────────────────────────────────────────────────

L3CallState::L3CallState(unsigned wCallState) : mCallState(wCallState) {}

void L3CallState::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, 3, 2);
    dest.writeField(wp, mCallState, 6);
}

void L3CallState::parseV(const L3Frame& src, size_t& rp) {
    rp += 2;
    mCallState = src.readField(rp, 6);
}

void L3CallState::parseV(const L3Frame& src, size_t& rp, size_t expectedLength) {
    (void)expectedLength;
    rp += 2;
    mCallState = src.readField(rp, 6);
}

void L3CallState::text(std::ostream& os) const {
    os << "CallState[" << mCallState << "]";
}

// ── L3ProgressIndicator ────────────────────────────────────────────────

L3ProgressIndicator::L3ProgressIndicator(Progress wProgress, Location wLocation)
    : mLocation(wLocation), mProgress(wProgress) {}

void L3ProgressIndicator::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, 0x0e, 4);
    dest.writeField(wp, static_cast<unsigned>(mLocation), 4);
    dest.writeField(wp, 1, 1);
    dest.writeField(wp, static_cast<unsigned>(mProgress), 7);
}

void L3ProgressIndicator::parseV(const L3Frame& src, size_t& rp) {
    src.readField(rp, 4);
    mLocation = static_cast<Location>(src.readField(rp, 4));
    src.readField(rp, 1);
    mProgress = static_cast<Progress>(src.readField(rp, 7));
}

void L3ProgressIndicator::parseV(const L3Frame& src, size_t& rp, size_t expectedLength) {
    (void)expectedLength;
    src.readField(rp, 4);
    mLocation = static_cast<Location>(src.readField(rp, 4));
    src.readField(rp, 1);
    mProgress = static_cast<Progress>(src.readField(rp, 7));
}

void L3ProgressIndicator::text(std::ostream& os) const {
    os << "Progress[location=" << static_cast<int>(mLocation)
       << " progress=" << static_cast<int>(mProgress) << "]";
}

// ── L3KeypadFacility ───────────────────────────────────────────────────

L3KeypadFacility::L3KeypadFacility(char wIA5) : mIA5(wIA5) {}

void L3KeypadFacility::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, static_cast<unsigned>(mIA5), 8);
}

void L3KeypadFacility::parseV(const L3Frame& src, size_t& rp) {
    mIA5 = static_cast<char>(src.readField(rp, 8));
}

void L3KeypadFacility::parseV(const L3Frame& src, size_t& rp, size_t expectedLength) {
    (void)expectedLength;
    mIA5 = static_cast<char>(src.readField(rp, 8));
}

void L3KeypadFacility::text(std::ostream& os) const {
    os << "Keypad[" << mIA5 << "]";
}

// ── L3Signal ────────────────────────────────────────────────────────────

L3Signal::L3Signal(SignalValues tone) : mSignalValue(tone) {}

void L3Signal::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, static_cast<unsigned>(mSignalValue), 8);
}

void L3Signal::parseV(const L3Frame& src, size_t& rp) {
    mSignalValue = static_cast<SignalValues>(src.readField(rp, 8));
}

void L3Signal::parseV(const L3Frame& src, size_t& rp, size_t expectedLength) {
    (void)expectedLength;
    mSignalValue = static_cast<SignalValues>(src.readField(rp, 8));
}

void L3Signal::text(std::ostream& os) const {
    os << "Signal[0x" << std::hex << std::setw(2) << std::setfill('0')
        << static_cast<int>(mSignalValue) << "]";
}

// ── L3RepeatIndicator ─────────────────────────────────────────────────

L3RepeatIndicator::L3RepeatIndicator(unsigned wValue) : mValue(wValue) {}

void L3RepeatIndicator::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, mValue, 4);
}

void L3RepeatIndicator::parseV(const L3Frame& src, size_t& rp) {
    mValue = src.readField(rp, 4);
}

void L3RepeatIndicator::parseV(const L3Frame& src, size_t& rp, size_t expectedLength) {
    (void)expectedLength;
    mValue = src.readField(rp, 4);
}

void L3RepeatIndicator::text(std::ostream& os) const {
    os << "RepeatIndicator: " << mValue;
}

// ── L3CCCapabilities ───────────────────────────────────────────────────

std::string L3CCCapabilities::getCodecSet() const {
    std::ostringstream oss;
    if (mBearerCapability.isPresent()) {
        oss << "Bearer:";
        mBearerCapability.text(oss);
    }
    if (mSupportedCodecs.isGsmPresent() || mSupportedCodecs.isUmtsPresent()) {
        oss << " Codecs:";
        mSupportedCodecs.text(oss);
    }
    return oss.str();
}

// ── L3SupServFacilityIE ─────────────────────────────────────────────────

L3SupServFacilityIE::L3SupServFacilityIE() {}

L3SupServFacilityIE::L3SupServFacilityIE(const std::string& wData)
    : mData(wData) {}

size_t L3SupServFacilityIE::lengthV() const {
    return mData.size();
}

void L3SupServFacilityIE::writeV(L3Frame& dest, size_t& wp) const {
    for (size_t i = 0; i < mData.size(); ++i) {
        dest.writeField(wp, static_cast<unsigned char>(mData[i]), 8);
    }
}

void L3SupServFacilityIE::parseV(const L3Frame& src, size_t& rp) {
    throw ParseError("parseV not valid for SupServFacilityIE, use expectedLength");
}

void L3SupServFacilityIE::parseV(const L3Frame& src, size_t& rp, size_t expectedLength) {
    mData.clear();
    for (size_t i = 0; i < expectedLength; ++i) {
        mData.push_back(static_cast<char>(src.readField(rp, 8)));
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

L3SupServVersionIndicator::L3SupServVersionIndicator()
    : mVersion(0) {}

void L3SupServVersionIndicator::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, mVersion, 8);
}

void L3SupServVersionIndicator::parseV(const L3Frame& src, size_t& rp) {
    mVersion = src.readField(rp, 8);
}

void L3SupServVersionIndicator::parseV(const L3Frame& src, size_t& rp, size_t) {
    parseV(src, rp);
}

void L3SupServVersionIndicator::text(std::ostream& os) const {
    os << "SSVersion[" << mVersion << "]";
}

// ── L3CCCommonIEs ──────────────────────────────────────────────────────

L3CCCommonIEs::L3CCCommonIEs()
    : mHaveFacility(false), mHaveSSVersion(false) {}

void L3CCCommonIEs::ccCommonText(std::ostream& os) const {
    if (mHaveFacility) os << " facility=(" << mFacility << ")";
    if (mHaveSSVersion) os << " SSVersion=(" << mSSVersion << ")";
}

size_t L3CCCommonIEs::ccCommonLength() const {
    size_t result = 0;
    if (mHaveFacility) result += mFacility.lengthTLV();
    if (mHaveSSVersion) result += mSSVersion.lengthTLV();
    return result;
}

void L3CCCommonIEs::ccCommonParse(const L3Frame& src, size_t& rp) {
    while (rp + 8 <= src.size()) {
        unsigned thisIEI = src.peekField(rp, 8);
        switch (thisIEI) {
            case 0x1c: mHaveFacility = mFacility.parseTLV(0x1c, src, rp); continue;
            case 0x7f: mHaveSSVersion = mSSVersion.parseTLV(0x7f, src, rp); continue;
            default: return;
        }
    }
}

void L3CCCommonIEs::ccCommonWrite(L3Frame& dest, size_t& wp) const {
    if (mHaveFacility) mFacility.writeTLV(0x1c, dest, wp);
    if (mHaveSSVersion) mSSVersion.writeTLV(0x7f, dest, wp);
}

} // namespace gsml3parser
