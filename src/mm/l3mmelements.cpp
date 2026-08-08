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

#include "gsml3parser/mm/l3mmelements.h"
#include "gsml3parser/gsm_common.h"
#include <cstring>
#include <sstream>
#include <iomanip>
#include <vector>

namespace gsml3parser {

static uint8_t reverseBits8(uint8_t b) {
    uint8_t r = 0;
    for (int i = 0; i < 8; i++) {
        r = (r << 1) | (b & 1);
        b >>= 1;
    }
    return r;
}

// ── L3CMServiceType (GSM 04.08 10.5.3.3) ────────────────────────────────

Expected<L3CMServiceType> L3CMServiceType::parse(BitReader& br) {
    auto val = br.readField(4);
    if (!val) return Expected<L3CMServiceType>::error(val.error());
    return Expected<L3CMServiceType>::hold(L3CMServiceType(static_cast<TypeCode>(val.value())));
}

void L3CMServiceType::write(BitWriter& bw) const {
    bw.writeField(static_cast<uint32_t>(mType), 4);
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

Expected<L3RejectCauseIE> L3RejectCauseIE::parse(BitReader& br) {
    auto val = br.readField(8);
    if (!val) return Expected<L3RejectCauseIE>::error(val.error());
    return Expected<L3RejectCauseIE>::hold(L3RejectCauseIE(static_cast<MMRejectCause>(val.value())));
}

void L3RejectCauseIE::write(BitWriter& bw) const {
    bw.writeField(static_cast<uint32_t>(mRejectCause), 8);
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
        size_t nameBytes = (nlen * 7 + 7) / 8;
        return 1 + nameBytes + (mCI ? 2 : 0);
    } else if (mAlphabet == GSMAlphabet::ALPHABET_UCS2) {
        return 1 + nlen * 2 + (mCI ? 2 : 0);
    } else {
        return 1 + nlen + (mCI ? 2 : 0);
    }
}

void L3NetworkName::write(BitWriter& bw) const {
    unsigned sz = static_cast<unsigned>(std::strlen(mName));

    if (mAlphabet == GSMAlphabet::ALPHABET_UCS2) {
        bw.writeField(1, 1);    // ext
        bw.writeField(1, 3);    // coding: UCS2
        bw.writeField(mCI ? 1 : 0, 1);  // CI
        bw.writeField(0, 3);    // spare
        for (unsigned i = 0; i < sz; i++) {
            bw.writeField(static_cast<uint32_t>(mName[i]), 16);
        }
    } else {
        // GSM 7-bit default alphabet with LSB8MSB
        size_t nameBits = sz * 7;
        size_t nameBytes = (nameBits + 7) / 8;
        size_t spareBits = nameBytes * 8 - nameBits;

        bw.writeField(1, 1);           // ext
        bw.writeField(0, 3);           // coding: GSM 7-bit
        bw.writeField(mCI ? 1 : 0, 1); // CI
        bw.writeField(static_cast<uint32_t>(spareBits), 3);

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
            bw.writeField(buffer[i], 8);
        }
    }

    if (mCI) {
        bw.writeField(static_cast<uint32_t>(mCI), 16);
    }
}

Expected<L3NetworkName> L3NetworkName::parse(BitReader& br) {
    auto r = br.readField(1); if (!r) return Expected<L3NetworkName>::error(r.error()); unsigned ext = r.value();
    (void)ext;
    r = br.readField(3); if (!r) return Expected<L3NetworkName>::error(r.error()); unsigned coding = r.value();
    r = br.readField(1); if (!r) return Expected<L3NetworkName>::error(r.error()); int ciFlag = r.value();
    r = br.readField(3); if (!r) return Expected<L3NetworkName>::error(r.error()); unsigned spareBits = r.value();
    (void)spareBits;

    L3NetworkName result;
    result.mCI = ciFlag;
    size_t idx = 0;

    if (coding == 1) {
        // UCS2
        result.mAlphabet = GSMAlphabet::ALPHABET_UCS2;
        while (br.hasMore() && idx < maxLen) {
            r = br.readField(16);
            if (!r) break;
            uint16_t ch = static_cast<uint16_t>(r.value());
            result.mName[idx++] = static_cast<char>(ch & 0xFF);
        }
    } else {
        // GSM 7-bit with LSB8MSB
        result.mAlphabet = GSMAlphabet::ALPHABET_7BIT;
        size_t remainingBytes = br.remainingBits() / 8;
        if (ciFlag) {
            if (remainingBytes < 2) {
                result.mName[0] = '\0';
                return Expected<L3NetworkName>::hold(std::move(result));
            }
            remainingBytes -= 2;
        }
        if (remainingBytes == 0) {
            result.mName[0] = '\0';
            return Expected<L3NetworkName>::hold(std::move(result));
        }

        // Read bytes and undo LSB8MSB
        std::vector<uint8_t> buffer(remainingBytes);
        for (size_t i = 0; i < remainingBytes; i++) {
            r = br.readField(8);
            if (!r) return Expected<L3NetworkName>::error(r.error());
            buffer[i] = reverseBits8(static_cast<uint8_t>(r.value()));
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
            result.mName[idx++] = static_cast<char>(decodeGSMChar(val));
        }
    }
    result.mName[idx] = '\0';

    if (ciFlag && br.hasMore()) {
        r = br.readField(16);
        if (!r) return Expected<L3NetworkName>::error(r.error());
        result.mCI = static_cast<int>(r.value());
    }

    return Expected<L3NetworkName>::hold(std::move(result));
}

Expected<L3NetworkName> L3NetworkName::parse(BitReader& br, size_t expectedLength) {
    auto r = br.readField(1); if (!r) return Expected<L3NetworkName>::error(r.error()); unsigned ext = r.value();
    (void)ext;
    r = br.readField(3); if (!r) return Expected<L3NetworkName>::error(r.error()); unsigned coding = r.value();
    r = br.readField(1); if (!r) return Expected<L3NetworkName>::error(r.error()); int ciFlag = r.value();
    r = br.readField(3); if (!r) return Expected<L3NetworkName>::error(r.error()); unsigned spareBits = r.value();

    L3NetworkName result;
    result.mCI = ciFlag;
    size_t idx = 0;
    size_t dataBytes = expectedLength - 1; // minus header

    if (coding == 1) {
        // UCS2
        result.mAlphabet = GSMAlphabet::ALPHABET_UCS2;
        size_t ciBytes = ciFlag ? 2 : 0;
        size_t numChars = (dataBytes - ciBytes) / 2;
        for (size_t i = 0; i < numChars && idx < maxLen; i++) {
            r = br.readField(16);
            if (!r) return Expected<L3NetworkName>::error(r.error());
            uint16_t ch = static_cast<uint16_t>(r.value());
            result.mName[idx++] = static_cast<char>(ch & 0xFF);
        }
    } else {
        // GSM 7-bit with LSB8MSB
        result.mAlphabet = GSMAlphabet::ALPHABET_7BIT;
        size_t ciBytes = ciFlag ? 2 : 0;
        size_t nameBytes = dataBytes - ciBytes;
        if (nameBytes == 0) {
            result.mName[0] = '\0';
            return Expected<L3NetworkName>::hold(std::move(result));
        }

        std::vector<uint8_t> buffer(nameBytes);
        for (size_t i = 0; i < nameBytes; i++) {
            r = br.readField(8);
            if (!r) return Expected<L3NetworkName>::error(r.error());
            buffer[i] = reverseBits8(static_cast<uint8_t>(r.value()));
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
            result.mName[idx++] = static_cast<char>(decodeGSMChar(val));
        }
    }
    result.mName[idx] = '\0';

    if (ciFlag && br.hasMore()) {
        r = br.readField(16);
        if (!r) return Expected<L3NetworkName>::error(r.error());
        result.mCI = static_cast<int>(r.value());
    }

    return Expected<L3NetworkName>::hold(std::move(result));
}

void L3NetworkName::text(std::ostream& os) const {
    os << "NetworkName[\"" << mName << "\" alphabet=" << static_cast<int>(mAlphabet);
    if (mCI) {
        os << " CI=" << mCI;
    }
    os << "]";
}

// ── L3TimeZoneAndTime (GSM 04.08 10.5.3.9) ─────────────────────────────

Expected<L3TimeZoneAndTime> L3TimeZoneAndTime::parse(BitReader& br) {
    L3TimeZoneAndTime result;

    // GSM 03.40 9.2.3.11: BCD encoding, low nibble first
    auto r = br.readField(4); if (!r) return Expected<L3TimeZoneAndTime>::error(r.error()); unsigned yLo = r.value();
    r = br.readField(4); if (!r) return Expected<L3TimeZoneAndTime>::error(r.error()); unsigned yHi = r.value();
    result.mYear = static_cast<uint8_t>(yLo + yHi * 10);

    r = br.readField(4); if (!r) return Expected<L3TimeZoneAndTime>::error(r.error()); unsigned mLo = r.value();
    r = br.readField(4); if (!r) return Expected<L3TimeZoneAndTime>::error(r.error()); unsigned mHi = r.value();
    result.mMonth = static_cast<uint8_t>(mLo + mHi * 10);

    r = br.readField(4); if (!r) return Expected<L3TimeZoneAndTime>::error(r.error()); unsigned dLo = r.value();
    r = br.readField(4); if (!r) return Expected<L3TimeZoneAndTime>::error(r.error()); unsigned dHi = r.value();
    result.mDay = static_cast<uint8_t>(dLo + dHi * 10);

    r = br.readField(4); if (!r) return Expected<L3TimeZoneAndTime>::error(r.error()); unsigned hLo = r.value();
    r = br.readField(4); if (!r) return Expected<L3TimeZoneAndTime>::error(r.error()); unsigned hHi = r.value();
    result.mHour = static_cast<uint8_t>(hLo + hHi * 10);

    r = br.readField(4); if (!r) return Expected<L3TimeZoneAndTime>::error(r.error()); unsigned minLo = r.value();
    r = br.readField(4); if (!r) return Expected<L3TimeZoneAndTime>::error(r.error()); unsigned minHi = r.value();
    result.mMinute = static_cast<uint8_t>(minLo + minHi * 10);

    r = br.readField(4); if (!r) return Expected<L3TimeZoneAndTime>::error(r.error()); unsigned sLo = r.value();
    r = br.readField(4); if (!r) return Expected<L3TimeZoneAndTime>::error(r.error()); unsigned sHi = r.value();
    result.mSecond = static_cast<uint8_t>(sLo + sHi * 10);

    // Timezone: low digit(4) + sign(1) + high digit(3)
    r = br.readField(4); if (!r) return Expected<L3TimeZoneAndTime>::error(r.error()); unsigned tz = r.value();
    r = br.readField(1); if (!r) return Expected<L3TimeZoneAndTime>::error(r.error()); unsigned tzSign = r.value();
    r = br.readField(3); if (!r) return Expected<L3TimeZoneAndTime>::error(r.error());
    tz += r.value() * 10;
    if (tzSign) tz = (tz | 0x80) & 0xFF;
    result.mTimezone = static_cast<uint8_t>(tz);

    return Expected<L3TimeZoneAndTime>::hold(std::move(result));
}

void L3TimeZoneAndTime::write(BitWriter& bw) const {
    // GSM 03.40 9.2.3.11: BCD encoding, low nibble first
    bw.writeField(mYear % 10, 4);
    bw.writeField(mYear / 10, 4);
    bw.writeField(mMonth % 10, 4);
    bw.writeField(mMonth / 10, 4);
    bw.writeField(mDay % 10, 4);
    bw.writeField(mDay / 10, 4);
    bw.writeField(mHour % 10, 4);
    bw.writeField(mHour / 10, 4);
    bw.writeField(mMinute % 10, 4);
    bw.writeField(mMinute / 10, 4);
    bw.writeField(mSecond % 10, 4);
    bw.writeField(mSecond / 10, 4);
    // Timezone: low digit(4) + sign(1) + high digit(3)
    unsigned tz = mTimezone;
    unsigned tzSign = 0;
    if (tz >= 0x80) {
        tzSign = 1;
        tz = tz ^ 0x80;
    }
    bw.writeField(tz % 10, 4);
    bw.writeField(tzSign, 1);
    bw.writeField(tz / 10, 3);
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

Expected<L3RAND> L3RAND::parse(BitReader& br) {
    L3RAND result;
    for (size_t i = 0; i < 16; ++i) {
        auto r = br.readField(8);
        if (!r) return Expected<L3RAND>::error(r.error());
        result.mRAND[i] = static_cast<uint8_t>(r.value());
    }
    return Expected<L3RAND>::hold(std::move(result));
}

void L3RAND::write(BitWriter& bw) const {
    for (size_t i = 0; i < 16; ++i) {
        bw.writeField(mRAND[i], 8);
    }
}

void L3RAND::text(std::ostream& os) const {
    os << "RAND[";
    for (size_t i = 0; i < mRAND.size(); ++i) {
        os << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(mRAND[i]);
    }
    os << "]";
}

// ── L3SRES (GSM 04.08 10.5.3.2) ────────────────────────────────────────

Expected<L3SRES> L3SRES::parse(BitReader& br) {
    auto val = br.readField(32);
    if (!val) return Expected<L3SRES>::error(val.error());
    return Expected<L3SRES>::hold(L3SRES(static_cast<uint32_t>(val.value())));
}

void L3SRES::write(BitWriter& bw) const {
    bw.writeField(mValue, 32);
}

void L3SRES::text(std::ostream& os) const {
    os << "SRES[0x" << std::hex << std::setw(8) << std::setfill('0') << mValue << "]";
}

} // namespace gsml3parser
