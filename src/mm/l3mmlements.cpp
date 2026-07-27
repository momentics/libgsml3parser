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

#include "gsml3parser/mm/l3mmlements.h"
#include "gsml3parser/gsm_common.h"
#include <cstring>
#include <sstream>
#include <iomanip>

namespace gsml3parser {

// ── L3CMServiceType (GSM 04.08 10.5.3.3) ────────────────────────────────

L3CMServiceType::L3CMServiceType(TypeCode wType)
    : mType(wType) {}

void L3CMServiceType::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, static_cast<unsigned>(mType), 4);
}

void L3CMServiceType::parseV(const L3Frame& src, size_t& rp) {
    mType = static_cast<TypeCode>(src.readField(rp, 4));
}

void L3CMServiceType::parseV(const L3Frame& src, size_t& rp, size_t) {
    parseV(src, rp);
}

void L3CMServiceType::text(std::ostream& os) const {
    switch (mType) {
        case MobileOriginatedCall:  os << "MO_Call"; break;
        case EmergencyCall:         os << "Emergency"; break;
        case ShortMessage:          os << "SMS"; break;
        case SupplementaryService:  os << "SS"; break;
        case VoiceCallGroup:        os << "VCG"; break;
        case VoiceBroadcast:        os << "VB"; break;
        case LocationService:       os << "Location"; break;
        default:                    os << "Unknown(" << static_cast<int>(mType) << ")"; break;
    }
}

// ── L3RejectCauseIE (GSM 04.08 10.5.3.6) ───────────────────────────────

L3RejectCauseIE::L3RejectCauseIE(MMRejectCause wCause)
    : mRejectCause(wCause) {}

void L3RejectCauseIE::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, static_cast<unsigned>(mRejectCause), 8);
}

void L3RejectCauseIE::parseV(const L3Frame& src, size_t& rp) {
    mRejectCause = static_cast<MMRejectCause>(src.readField(rp, 8));
}

void L3RejectCauseIE::parseV(const L3Frame& src, size_t& rp, size_t) {
    parseV(src, rp);
}

void L3RejectCauseIE::text(std::ostream& os) const {
    os << "RejectCause[" << MMRejectCause2Str(mRejectCause) << "]";
}

// ── L3NetworkName (GSM 04.08 10.5.3.5a) ────────────────────────────────

L3NetworkName::L3NetworkName(const char* wName, GSMAlphabet alphabet, int wCI)
    : mAlphabet(alphabet), mCI(wCI)
{
    std::strncpy(mName, wName, maxLen);
    mName[maxLen] = '\0';
}

size_t L3NetworkName::lengthV() const {
    size_t nlen = std::strlen(mName);
    if (mAlphabet == GSMAlphabet::ALPHABET_7BIT) {
        return 1 + (nlen * 7 + 7) / 8 + (mCI ? 1 : 0);
    } else if (mAlphabet == GSMAlphabet::ALPHABET_8BIT) {
        return 1 + nlen + (mCI ? 1 : 0);
    } else {
        return 1 + nlen * 2 + (mCI ? 1 : 0);
    }
}

void L3NetworkName::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, static_cast<unsigned>(mAlphabet), 4);
    dest.writeField(wp, 0, 4);
    size_t nlen = std::strlen(mName);

    if (mAlphabet == GSMAlphabet::ALPHABET_7BIT) {
        size_t totalBits = nlen * 7;
        size_t bytesNeeded = (totalBits + 7) / 8;
        dest.writeField(wp, static_cast<unsigned>(bytesNeeded), 8);
        for (size_t i = 0; i < nlen; ++i) {
            unsigned char gsmChar = encodeGSMChar(static_cast<unsigned char>(mName[i]));
            dest.writeField(wp, gsmChar, 7);
        }
    } else if (mAlphabet == GSMAlphabet::ALPHABET_8BIT) {
        dest.writeField(wp, static_cast<unsigned>(nlen), 8);
        for (size_t i = 0; i < nlen; ++i) {
            dest.writeField(wp, static_cast<unsigned char>(mName[i]), 8);
        }
    } else {
        dest.writeField(wp, static_cast<unsigned>(nlen), 8);
        for (size_t i = 0; i < nlen; ++i) {
            dest.writeField(wp, static_cast<unsigned char>(mName[i]), 8);
        }
    }

    if (mCI) {
        dest.writeField(wp, mCI, 16);
    }
}

void L3NetworkName::parseV(const L3Frame& src, size_t& rp) {
    mAlphabet = static_cast<GSMAlphabet>(src.readField(rp, 4));
    src.readField(rp, 4);
    unsigned nlen = src.readField(rp, 8);
    size_t idx = 0;

    if (mAlphabet == GSMAlphabet::ALPHABET_7BIT) {
        for (size_t i = 0; i < nlen && idx < maxLen; ++i) {
            unsigned char gsmChar = static_cast<unsigned char>(src.readField(rp, 7));
            mName[idx++] = static_cast<char>(decodeGSMChar(gsmChar));
        }
    } else {
        for (size_t i = 0; i < nlen && idx < maxLen; ++i) {
            mName[idx++] = static_cast<char>(src.readField(rp, 8));
        }
    }
    mName[idx] = '\0';
    mCI = 0;
}

void L3NetworkName::parseV(const L3Frame& src, size_t& rp, size_t expectedLength) {
    mAlphabet = static_cast<GSMAlphabet>(src.readField(rp, 4));
    src.readField(rp, 4);
    unsigned nlen = src.readField(rp, 8);
    size_t idx = 0;

    if (mAlphabet == GSMAlphabet::ALPHABET_7BIT) {
        for (size_t i = 0; i < nlen && idx < maxLen; ++i) {
            unsigned char gsmChar = static_cast<unsigned char>(src.readField(rp, 7));
            mName[idx++] = static_cast<char>(decodeGSMChar(gsmChar));
        }
    } else {
        for (size_t i = 0; i < nlen && idx < maxLen; ++i) {
            mName[idx++] = static_cast<char>(src.readField(rp, 8));
        }
    }
    mName[idx] = '\0';

    size_t remainingBytes = expectedLength - 2 - ((mAlphabet == GSMAlphabet::ALPHABET_7BIT) ? (nlen * 7 + 7) / 8 : nlen);
    if (remainingBytes >= 2) {
        mCI = src.readField(rp, 16);
    } else {
        mCI = 0;
    }
}

void L3NetworkName::text(std::ostream& os) const {
    os << "NetworkName[\"" << mName << "\" alphabet=" << static_cast<int>(mAlphabet);
    if (mCI) {
        os << " CI=" << mCI;
    }
    os << "]";
}

// ── L3TimeZoneAndTime (GSM 04.08 10.5.3.9) ─────────────────────────────

L3TimeZoneAndTime::L3TimeZoneAndTime(TimeType type)
    : mYear(0), mMonth(1), mDay(1), mHour(0), mMinute(0), mTimezone(0), mType(type) {}

void L3TimeZoneAndTime::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, mYear, 8);
    dest.writeField(wp, mMonth, 4);
    dest.writeField(wp, mDay, 5);
    dest.writeField(wp, mHour, 5);
    dest.writeField(wp, mMinute, 6);
    dest.writeField(wp, mTimezone, 8);
    dest.writeField(wp, static_cast<unsigned>(mType), 1);
    dest.writeField(wp, 0, 7);
}

void L3TimeZoneAndTime::parseV(const L3Frame& src, size_t& rp) {
    mYear = static_cast<uint8_t>(src.readField(rp, 8));
    mMonth = static_cast<uint8_t>(src.readField(rp, 4));
    mDay = static_cast<uint8_t>(src.readField(rp, 5));
    mHour = static_cast<uint8_t>(src.readField(rp, 5));
    mMinute = static_cast<uint8_t>(src.readField(rp, 6));
    mTimezone = static_cast<uint8_t>(src.readField(rp, 8));
    mType = static_cast<TimeType>(src.readField(rp, 1));
    src.readField(rp, 7);
}

void L3TimeZoneAndTime::parseV(const L3Frame& src, size_t& rp, size_t) {
    parseV(src, rp);
}

void L3TimeZoneAndTime::text(std::ostream& os) const {
    os << "TimeZoneAndTime["
       << static_cast<int>(mYear) << "/"
       << std::setw(2) << std::setfill('0') << static_cast<int>(mMonth) << "/"
       << std::setw(2) << std::setfill('0') << static_cast<int>(mDay) << " "
       << std::setw(2) << std::setfill('0') << static_cast<int>(mHour) << ":"
       << std::setw(2) << std::setfill('0') << static_cast<int>(mMinute)
       << " TZ=" << static_cast<int>(mTimezone)
       << " type=" << (mType == UTC_TIME ? "UTC" : "Local") << "]";
}

// ── L3RAND (GSM 04.08 10.5.3.1) ────────────────────────────────────────

L3RAND::L3RAND() {
    mRAND.resize(16, 0);
}

L3RAND::L3RAND(const std::vector<uint8_t>& rand)
    : mRAND(rand) {}

void L3RAND::writeV(L3Frame& dest, size_t& wp) const {
    for (size_t i = 0; i < 16 && i < mRAND.size(); ++i) {
        dest.writeField(wp, mRAND[i], 8);
    }
}

void L3RAND::parseV(const L3Frame& src, size_t& rp) {
    mRAND.resize(16);
    for (size_t i = 0; i < 16; ++i) {
        mRAND[i] = static_cast<uint8_t>(src.readField(rp, 8));
    }
}

void L3RAND::parseV(const L3Frame& src, size_t& rp, size_t) {
    parseV(src, rp);
}

void L3RAND::text(std::ostream& os) const {
    os << "RAND[";
    for (size_t i = 0; i < mRAND.size(); ++i) {
        os << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(mRAND[i]);
    }
    os << "]";
}

// ── L3SRES (GSM 04.08 10.5.3.2) ────────────────────────────────────────

L3SRES::L3SRES(uint32_t wValue)
    : mValue(wValue) {}

void L3SRES::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, mValue, 32);
}

void L3SRES::parseV(const L3Frame& src, size_t& rp) {
    mValue = src.readField(rp, 32);
}

void L3SRES::parseV(const L3Frame& src, size_t& rp, size_t) {
    parseV(src, rp);
}

void L3SRES::text(std::ostream& os) const {
    os << "SRES[0x" << std::hex << std::setw(8) << std::setfill('0') << mValue << "]";
}

} // namespace gsml3parser
