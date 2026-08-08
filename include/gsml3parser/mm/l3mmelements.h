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

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ostream>
#include <vector>

#include "../expected.h"
#include "../bitreader.h"
#include "../bitwriter.h"
#include "../types.h"
#include "../enums.h"

namespace gsml3parser {

// ── CM Service Type (GSM 04.08 10.5.3.3) ───────────────────────────────

class L3CMServiceType {
public:
    enum TypeCode : uint8_t {
        UndefinedType = 0,
        MobileOriginatedCall = 1,
        EmergencyCall = 2,
        ShortMessage = 4,
        SupplementaryService = 8,
        VoiceCallGroup = 9,
        VoiceBroadcast = 10,
        LocationService = 11,
        MobileTerminatedCall = 100,
        MobileTerminatedShortMessage = 101,
        HandoverCall = 103,
        LocationUpdateRequest = 105
    };

private:
    TypeCode mType{UndefinedType};

public:
    L3CMServiceType() = default;
    explicit L3CMServiceType(TypeCode wType) : mType(wType) {}

    TypeCode type() const { return mType; }
    bool isCC() const { return mType == MobileOriginatedCall || mType == EmergencyCall || mType == MobileTerminatedCall || mType == HandoverCall; }
    bool isSMS() const { return mType == ShortMessage || mType == MobileTerminatedShortMessage; }
    bool isMM() const { return mType == LocationUpdateRequest; }
    static constexpr size_t lengthV() { return 0; }

    [[nodiscard]] static Expected<L3CMServiceType> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    bool operator==(const L3CMServiceType&) const = default;
};

// ── Reject Cause IE (GSM 04.08 10.5.3.6) ───────────────────────────────

class L3RejectCauseIE {
private:
    MMRejectCause mRejectCause{MMRejectCause::Zero};

public:
    L3RejectCauseIE() = default;
    explicit L3RejectCauseIE(MMRejectCause wCause) : mRejectCause(wCause) {}

    MMRejectCause rejectCause() const { return mRejectCause; }
    static constexpr size_t lengthV() { return 1; }

    [[nodiscard]] static Expected<L3RejectCauseIE> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    bool operator==(const L3RejectCauseIE&) const = default;
};

// ── Network Name (GSM 04.08 10.5.3.5a) ─────────────────────────────────

class L3NetworkName {
private:
    static constexpr size_t maxLen = 93;
    GSMAlphabet mAlphabet{GSMAlphabet::ALPHABET_7BIT};
    char mName[maxLen + 1]{};
    int mCI{0};

public:
    L3NetworkName() = default;
    L3NetworkName(const char* wName, GSMAlphabet alphabet = GSMAlphabet::ALPHABET_7BIT, int wCI = 0);

    const char* name() const { return mName; }
    GSMAlphabet alphabet() const { return mAlphabet; }
    int ci() const { return mCI; }

    size_t lengthV() const;
    [[nodiscard]] static Expected<L3NetworkName> parse(BitReader& br);
    [[nodiscard]] static Expected<L3NetworkName> parse(BitReader& br, size_t expectedLength);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── Time Zone And Time (GSM 04.08 10.5.3.9) ───────────────────────────

class L3TimeZoneAndTime {
public:
    enum TimeType : uint8_t {
        LOCAL_TIME,
        UTC_TIME
    };

private:
    uint8_t mYear{0};
    uint8_t mMonth{1};
    uint8_t mDay{1};
    uint8_t mHour{0};
    uint8_t mMinute{0};
    uint8_t mSecond{0};
    uint8_t mTimezone{0};
    TimeType mType{UTC_TIME};

public:
    L3TimeZoneAndTime() = default;
    explicit L3TimeZoneAndTime(TimeType type) : mType(type) {}

    uint8_t year() const { return mYear; }
    uint8_t month() const { return mMonth; }
    uint8_t day() const { return mDay; }
    uint8_t hour() const { return mHour; }
    uint8_t minute() const { return mMinute; }
    uint8_t second() const { return mSecond; }
    uint8_t timezone() const { return mTimezone; }
    TimeType type() const { return mType; }
    static constexpr size_t lengthV() { return 7; }

    [[nodiscard]] static Expected<L3TimeZoneAndTime> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    bool operator==(const L3TimeZoneAndTime&) const = default;
};

// ── RAND (GSM 04.08 10.5.3.1) ──────────────────────────────────────────

class L3RAND {
private:
    std::array<uint8_t, 16> mRAND{};

public:
    L3RAND() = default;
    explicit L3RAND(const std::array<uint8_t, 16>& rand) : mRAND(rand) {}
    explicit L3RAND(const std::vector<uint8_t>& rand);

    const std::array<uint8_t, 16>& rand() const { return mRAND; }
    static constexpr size_t lengthV() { return 16; }

    [[nodiscard]] static Expected<L3RAND> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    bool operator==(const L3RAND&) const = default;
};

// ── SRES (GSM 04.08 10.5.3.2) ──────────────────────────────────────────

class L3SRES {
private:
    uint32_t mValue{0};

public:
    L3SRES() = default;
    explicit L3SRES(uint32_t wValue) : mValue(wValue) {}

    uint32_t value() const { return mValue; }
    static constexpr size_t lengthV() { return 4; }

    [[nodiscard]] static Expected<L3SRES> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    bool operator==(const L3SRES&) const = default;
};

} // namespace gsml3parser
