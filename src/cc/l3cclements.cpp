#include "gsml3parser/cc/l3cclements.h"
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
    mOctet3a.push_back(static_cast<uint8_t>(src.readField(rp, 8)));
}

void L3BearerCapability::parseV(const L3Frame& src, size_t& rp, size_t expectedLength) {
    mPresent = true;
    mOctet3 = static_cast<uint8_t>(src.readField(rp, 8));
    mOctet3a.clear();
    for (size_t i = 1; i < expectedLength; ++i) {
        mOctet3a.push_back(static_cast<uint8_t>(src.readField(rp, 8)));
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

L3SupportedCodecList::L3SupportedCodecList()
    : mGsmPresent(false), mUmtsPresent(false) {}

size_t L3SupportedCodecList::lengthV() const {
    return (mGsmPresent ? 4 : 0) + (mUmtsPresent ? 4 : 0);
}

void L3SupportedCodecList::writeV(L3Frame& dest, size_t& wp) const {
    if (mGsmPresent) {
        dest.writeField(wp, 0, 8);
        for (const auto& b : mGsmCodecs) {
            dest.writeField(wp, b, 8);
        }
    }
    if (mUmtsPresent) {
        dest.writeField(wp, 4, 8);
        for (const auto& b : mUmtsCodecs) {
            dest.writeField(wp, b, 8);
        }
    }
}

void L3SupportedCodecList::parseV(const L3Frame& src, size_t& rp) {
    mGsmCodecs.clear();
    mUmtsCodecs.clear();
    while (rp < src.size()) {
        unsigned sysId = src.readField(rp, 8);
        if (sysId == 0) {
            mGsmPresent = true;
            for (size_t i = 0; i < 3 && rp < src.size(); ++i) {
                mGsmCodecs.push_back(static_cast<uint8_t>(src.readField(rp, 8)));
            }
        } else if (sysId == 4) {
            mUmtsPresent = true;
            for (size_t i = 0; i < 3 && rp < src.size(); ++i) {
                mUmtsCodecs.push_back(static_cast<uint8_t>(src.readField(rp, 8)));
            }
        } else {
            rp += 24;
        }
    }
}

void L3SupportedCodecList::parseV(const L3Frame& src, size_t& rp, size_t expectedLength) {
    mGsmCodecs.clear();
    mUmtsCodecs.clear();
    size_t end = rp + 8 * expectedLength;
    while (rp < end) {
        unsigned sysId = src.readField(rp, 8);
        if (sysId == 0) {
            mGsmPresent = true;
            for (size_t i = 0; i < 3 && rp < end; ++i) {
                mGsmCodecs.push_back(static_cast<uint8_t>(src.readField(rp, 8)));
            }
        } else if (sysId == 4) {
            mUmtsPresent = true;
            for (size_t i = 0; i < 3 && rp < end; ++i) {
                mUmtsCodecs.push_back(static_cast<uint8_t>(src.readField(rp, 8)));
            }
        } else {
            rp += 24;
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

void L3BCDDigits::parse(const L3Frame& src, size_t& rp, size_t numOctets, bool international) {
    (void)international;
    size_t maxDigits = numOctets * 2;
    size_t idx = 0;
    for (size_t i = 0; i < numOctets && idx < maxDigits; ++i) {
        unsigned bcd = src.readField(rp, 8);
        unsigned hi = bcd / 10;
        unsigned lo = bcd % 10;
        if (hi != 0xf && idx < maxDigits) {
            mDigits[idx++] = static_cast<char>('0' + hi);
        }
        if (lo != 0xf && idx < maxDigits) {
            mDigits[idx++] = static_cast<char>('0' + lo);
        }
    }
    mDigits[idx] = '\0';
}

void L3BCDDigits::write(L3Frame& dest, size_t& wp) const {
    size_t dlen = strlen(mDigits);
    for (size_t i = 0; i < dlen; i += 2) {
        unsigned hi = mDigits[i] - '0';
        unsigned lo = (i + 1 < dlen) ? mDigits[i + 1] - '0' : 0xf;
        dest.writeField(wp, hi * 10 + lo, 8);
    }
}

size_t L3BCDDigits::lengthV() const {
    return (strlen(mDigits) + 1) / 2;
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
    dest.writeField(wp, static_cast<unsigned>(mType), 3);
    dest.writeField(wp, static_cast<unsigned>(mPlan), 4);
    dest.writeField(wp, 0, 1);
    mDigits.write(dest, wp);
}

void L3CalledPartyBCDNumber::parseV(const L3Frame& src, size_t& rp) {
    mType = static_cast<TypeOfNumber>(src.readField(rp, 3));
    mPlan = static_cast<NumberingPlan>(src.readField(rp, 4));
    src.readField(rp, 1);
    size_t remaining = (src.size() - rp) / 8;
    mDigits.parse(src, rp, remaining);
}

void L3CalledPartyBCDNumber::parseV(const L3Frame& src, size_t& rp, size_t expectedLength) {
    mType = static_cast<TypeOfNumber>(src.readField(rp, 3));
    mPlan = static_cast<NumberingPlan>(src.readField(rp, 4));
    src.readField(rp, 1);
    mDigits.parse(src, rp, expectedLength - 1);
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
    dest.writeField(wp, static_cast<unsigned>(mType), 3);
    dest.writeField(wp, static_cast<unsigned>(mPlan), 4);
    dest.writeField(wp, 0, 1);
    mDigits.write(dest, wp);
    if (mHaveOctet3a) {
        dest.writeField(wp, mPresentationIndicator, 2);
        dest.writeField(wp, mScreeningIndicator, 2);
        dest.writeField(wp, 0, 4);
    }
}

void L3CallingPartyBCDNumber::parseV(const L3Frame& src, size_t& rp) {
    mType = static_cast<TypeOfNumber>(src.readField(rp, 3));
    mPlan = static_cast<NumberingPlan>(src.readField(rp, 4));
    src.readField(rp, 1);
    size_t remaining = (src.size() - rp) / 8;
    mDigits.parse(src, rp, remaining);
}

void L3CallingPartyBCDNumber::parseV(const L3Frame& src, size_t& rp, size_t expectedLength) {
    mType = static_cast<TypeOfNumber>(src.readField(rp, 3));
    mPlan = static_cast<NumberingPlan>(src.readField(rp, 4));
    src.readField(rp, 1);
    size_t digitOctets = expectedLength - 1;
    if (digitOctets > 1 && (src.peekField(rp + mDigits.lengthV() * 8, 8) & 0x80)) {
        mHaveOctet3a = true;
        mPresentationIndicator = src.readField(rp, 2);
        mScreeningIndicator = src.readField(rp, 2);
        src.readField(rp, 4);
        digitOctets--;
    }
    mDigits.parse(src, rp, digitOctets);
}

void L3CallingPartyBCDNumber::text(std::ostream& os) const {
    os << "CallingParty[" << mDigits.digits() << "]";
}

// ── L3CauseElement ─────────────────────────────────────────────────────

L3CauseElement::L3CauseElement(Cause wCause, Location wLocation)
    : mLocation(wLocation), mCause(wCause) {}

void L3CauseElement::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, static_cast<unsigned>(mLocation), 4);
    dest.writeField(wp, static_cast<unsigned>(mCause), 7);
    dest.writeField(wp, 0, 1);
}

void L3CauseElement::parseV(const L3Frame& src, size_t& rp) {
    mLocation = static_cast<Location>(src.readField(rp, 4));
    mCause = static_cast<Cause>(src.readField(rp, 7));
    src.readField(rp, 1);
}

void L3CauseElement::parseV(const L3Frame& src, size_t& rp, size_t expectedLength) {
    (void)expectedLength;
    mLocation = static_cast<Location>(src.readField(rp, 4));
    mCause = static_cast<Cause>(src.readField(rp, 7));
    src.readField(rp, 1);
}

void L3CauseElement::text(std::ostream& os) const {
    os << "Cause[" << CCCause2Str(mCause) << "]";
}

// ── L3CallState ─────────────────────────────────────────────────────────

L3CallState::L3CallState(unsigned wCallState) : mCallState(wCallState) {}

void L3CallState::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, mCallState, 8);
}

void L3CallState::parseV(const L3Frame& src, size_t& rp) {
    mCallState = src.readField(rp, 8);
}

void L3CallState::parseV(const L3Frame& src, size_t& rp, size_t expectedLength) {
    (void)expectedLength;
    mCallState = src.readField(rp, 8);
}

void L3CallState::text(std::ostream& os) const {
    os << "CallState[" << mCallState << "]";
}

// ── L3ProgressIndicator ────────────────────────────────────────────────

L3ProgressIndicator::L3ProgressIndicator(Progress wProgress, Location wLocation)
    : mLocation(wLocation), mProgress(wProgress) {}

void L3ProgressIndicator::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, static_cast<unsigned>(mLocation), 4);
    dest.writeField(wp, static_cast<unsigned>(mProgress), 7);
    dest.writeField(wp, 0, 1);
}

void L3ProgressIndicator::parseV(const L3Frame& src, size_t& rp) {
    mLocation = static_cast<Location>(src.readField(rp, 4));
    mProgress = static_cast<Progress>(src.readField(rp, 7));
    src.readField(rp, 1);
}

void L3ProgressIndicator::parseV(const L3Frame& src, size_t& rp, size_t expectedLength) {
    (void)expectedLength;
    mLocation = static_cast<Location>(src.readField(rp, 4));
    mProgress = static_cast<Progress>(src.readField(rp, 7));
    src.readField(rp, 1);
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

} // namespace gsml3parser
