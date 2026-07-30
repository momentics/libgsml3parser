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
#include <vector>

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

static uint8_t reverseBits8(uint8_t b) {
    uint8_t r = 0;
    for (int i = 0; i < 8; i++) {
        r = (r << 1) | (b & 1);
        b >>= 1;
    }
    return r;
}

L3NetworkName::L3NetworkName(const char* wName, GSMAlphabet alphabet, int wCI)
    : mAlphabet(alphabet), mCI(wCI)
{
    std::strncpy(mName, wName, maxLen);
    mName[maxLen] = '\0';
}

size_t L3NetworkName::lengthV() const {
    size_t nlen = std::strlen(mName);
    if (mAlphabet == GSMAlphabet::ALPHABET_7BIT) {
        size_t nameBytes = (nlen * 7 + 7) / 8;
        return 1 + nameBytes + (mCI ? 2 : 0);
    } else if (mAlphabet == GSMAlphabet::ALPHABET_UCS2) {
        return 1 + nlen * 2 + (mCI ? 2 : 0);
    } else {
        return 1 + nlen + (mCI ? 2 : 0);
    }
}

void L3NetworkName::writeV(L3Frame& dest, size_t& wp) const {
    unsigned sz = static_cast<unsigned>(std::strlen(mName));

    if (mAlphabet == GSMAlphabet::ALPHABET_UCS2) {
        dest.writeField(wp, 1, 1);    // ext
        dest.writeField(wp, 1, 3);    // coding: UCS2
        dest.writeField(wp, mCI ? 1 : 0, 1);  // CI
        dest.writeField(wp, 0, 3);    // spare
        for (unsigned i = 0; i < sz; i++) {
            dest.writeField(wp, static_cast<unsigned>(mName[i]), 16);
        }
    } else {
        // GSM 7-bit default alphabet with LSB8MSB
        size_t nameBits = sz * 7;
        size_t nameBytes = (nameBits + 7) / 8;
        size_t spareBits = nameBytes * 8 - nameBits;

        dest.writeField(wp, 1, 1);           // ext
        dest.writeField(wp, 0, 3);           // coding: GSM 7-bit
        dest.writeField(wp, mCI ? 1 : 0, 1); // CI
        dest.writeField(wp, static_cast<unsigned>(spareBits), 3);

        // Encode 7-bit chars LSB-first into buffer
        std::vector<uint8_t> buffer(nameBytes, 0);
        size_t bitPos = 0;
        for (unsigned i = 0; i < sz; i++) {
            unsigned char gsmChar = encodeGSMChar(static_cast<unsigned char>(mName[i]));
            for (int bit = 0; bit < 7; bit++) {
                uint8_t b = (gsmChar >> bit) & 1;
                size_t byteIdx = bitPos / 8;
                size_t bitIdx = bitPos % 8;
                if (byteIdx < nameBytes) {
                    buffer[byteIdx] |= (b << static_cast<unsigned>(bitIdx));
                }
                bitPos++;
            }
        }

        // LSB8MSB: reverse bits within each byte
        for (size_t i = 0; i < nameBytes; i++) {
            buffer[i] = reverseBits8(buffer[i]);
        }

        // Write to destination
        for (size_t i = 0; i < nameBytes; i++) {
            dest.writeField(wp, buffer[i], 8);
        }
    }

    if (mCI) {
        dest.writeField(wp, static_cast<unsigned>(mCI), 16);
    }
}

void L3NetworkName::parseV(const L3Frame& src, size_t& rp) {
    unsigned ext = src.readField(rp, 1);
    (void)ext;
    unsigned coding = src.readField(rp, 3);
    mCI = src.readField(rp, 1);
    unsigned spareBits = src.readField(rp, 3);
    (void)spareBits;

    size_t idx = 0;

    if (coding == 1) {
        // UCS2
        mAlphabet = GSMAlphabet::ALPHABET_UCS2;
        while (rp + 16 <= src.size() && idx < maxLen) {
            uint16_t ch = src.readField(rp, 16);
            mName[idx++] = static_cast<char>(ch & 0xFF);
        }
    } else {
        // GSM 7-bit with LSB8MSB
        mAlphabet = GSMAlphabet::ALPHABET_7BIT;
        size_t remainingBits = src.size() - rp;
        size_t remainingBytes = remainingBits / 8;
        if (mCI) remainingBytes -= 2;
        if (remainingBytes == 0) { mName[0] = '\0'; return; }

        // Read bytes and undo LSB8MSB
        std::vector<uint8_t> buffer(remainingBytes);
        for (size_t i = 0; i < remainingBytes; i++) {
            buffer[i] = reverseBits8(static_cast<uint8_t>(src.readField(rp, 8)));
        }

        // Extract 7-bit values LSB-first
        size_t nameBits = remainingBytes * 8 - spareBits;
        size_t numChars = nameBits / 7;
        size_t bitPos = 0;
        for (size_t i = 0; i < numChars && idx < maxLen; i++) {
            uint8_t val = 0;
            for (int bit = 0; bit < 7; bit++) {
                size_t byteIdx = bitPos / 8;
                size_t bitIdx = bitPos % 8;
                if (byteIdx < remainingBytes) {
                    val |= ((buffer[byteIdx] >> bitIdx) & 1) << bit;
                }
                bitPos++;
            }
            mName[idx++] = static_cast<char>(decodeGSMChar(val));
        }
    }
    mName[idx] = '\0';

    if (mCI && rp + 16 <= src.size()) {
        mCI = src.readField(rp, 16);
    }
}

void L3NetworkName::parseV(const L3Frame& src, size_t& rp, size_t expectedLength) {
    unsigned ext = src.readField(rp, 1);
    (void)ext;
    unsigned coding = src.readField(rp, 3);
    mCI = src.readField(rp, 1);
    unsigned spareBits = src.readField(rp, 3);

    size_t idx = 0;
    size_t dataBytes = expectedLength - 1;  // minus header

    if (coding == 1) {
        // UCS2
        mAlphabet = GSMAlphabet::ALPHABET_UCS2;
        size_t ciBytes = mCI ? 2 : 0;
        size_t numChars = (dataBytes - ciBytes) / 2;
        for (size_t i = 0; i < numChars && idx < maxLen; i++) {
            uint16_t ch = src.readField(rp, 16);
            mName[idx++] = static_cast<char>(ch & 0xFF);
        }
    } else {
        // GSM 7-bit with LSB8MSB
        mAlphabet = GSMAlphabet::ALPHABET_7BIT;
        size_t ciBytes = mCI ? 2 : 0;
        size_t nameBytes = dataBytes - ciBytes;
        if (nameBytes == 0) { mName[0] = '\0'; return; }

        std::vector<uint8_t> buffer(nameBytes);
        for (size_t i = 0; i < nameBytes; i++) {
            buffer[i] = reverseBits8(static_cast<uint8_t>(src.readField(rp, 8)));
        }

        size_t nameBits = nameBytes * 8 - spareBits;
        size_t numChars = nameBits / 7;
        size_t bitPos = 0;
        for (size_t i = 0; i < numChars && idx < maxLen; i++) {
            uint8_t val = 0;
            for (int bit = 0; bit < 7; bit++) {
                size_t byteIdx = bitPos / 8;
                size_t bitIdx = bitPos % 8;
                if (byteIdx < nameBytes) {
                    val |= ((buffer[byteIdx] >> bitIdx) & 1) << bit;
                }
                bitPos++;
            }
            mName[idx++] = static_cast<char>(decodeGSMChar(val));
        }
    }
    mName[idx] = '\0';

    if (mCI && rp + 16 <= src.size()) {
        mCI = src.readField(rp, 16);
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
    : mYear(0), mMonth(1), mDay(1), mHour(0), mMinute(0), mSecond(0), mTimezone(0), mType(type) {}

void L3TimeZoneAndTime::writeV(L3Frame& dest, size_t& wp) const {
    // GSM 03.40 9.2.3.11: BCD encoding, low nibble first
    dest.writeField(wp, mYear % 10, 4);
    dest.writeField(wp, mYear / 10, 4);
    dest.writeField(wp, mMonth % 10, 4);
    dest.writeField(wp, mMonth / 10, 4);
    dest.writeField(wp, mDay % 10, 4);
    dest.writeField(wp, mDay / 10, 4);
    dest.writeField(wp, mHour % 10, 4);
    dest.writeField(wp, mHour / 10, 4);
    dest.writeField(wp, mMinute % 10, 4);
    dest.writeField(wp, mMinute / 10, 4);
    dest.writeField(wp, mSecond % 10, 4);
    dest.writeField(wp, mSecond / 10, 4);
    // Timezone: low digit(4) + sign(1) + high digit(3)
    unsigned tz = mTimezone;
    unsigned tzSign = 0;
    if (tz >= 0x80) {
        tzSign = 1;
        tz = tz ^ 0x80;
    }
    dest.writeField(wp, tz % 10, 4);
    dest.writeField(wp, tzSign, 1);
    dest.writeField(wp, tz / 10, 3);
}

void L3TimeZoneAndTime::parseV(const L3Frame& src, size_t& rp) {
    // GSM 03.40 9.2.3.11: BCD encoding, low nibble first
    unsigned yLo = src.readField(rp, 4);
    unsigned yHi = src.readField(rp, 4);
    mYear = static_cast<uint8_t>(yLo + yHi * 10);
    unsigned mLo = src.readField(rp, 4);
    unsigned mHi = src.readField(rp, 4);
    mMonth = static_cast<uint8_t>(mLo + mHi * 10);
    unsigned dLo = src.readField(rp, 4);
    unsigned dHi = src.readField(rp, 4);
    mDay = static_cast<uint8_t>(dLo + dHi * 10);
    unsigned hLo = src.readField(rp, 4);
    unsigned hHi = src.readField(rp, 4);
    mHour = static_cast<uint8_t>(hLo + hHi * 10);
    unsigned minLo = src.readField(rp, 4);
    unsigned minHi = src.readField(rp, 4);
    mMinute = static_cast<uint8_t>(minLo + minHi * 10);
    unsigned sLo = src.readField(rp, 4);
    unsigned sHi = src.readField(rp, 4);
    mSecond = static_cast<uint8_t>(sLo + sHi * 10);
    // Timezone: low digit(4) + sign(1) + high digit(3)
    unsigned tz = src.readField(rp, 4);
    unsigned tzSign = src.readField(rp, 1);
    tz += src.readField(rp, 3) * 10;
    if (tzSign) tz = (tz | 0x80) & 0xFF;
    mTimezone = static_cast<uint8_t>(tz);
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
        << std::setw(2) << std::setfill('0') << static_cast<int>(mMinute) << ":"
        << std::setw(2) << std::setfill('0') << static_cast<int>(mSecond)
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
