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

#include <cstdint>
#include <string>
#include <vector>

#include "../l3message.h"
#include "../types.h"
#include "../enums.h"

namespace gsml3parser {

// ── CM Service Type (GSM 04.08 10.5.3.3) ───────────────────────────────

class L3CMServiceType : public L3ProtocolElement {
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
    TypeCode mType;
public:
    explicit L3CMServiceType(TypeCode wType = UndefinedType);
    TypeCode type() const { return mType; }
    bool isCC() const { return mType == MobileOriginatedCall || mType == EmergencyCall || mType == MobileTerminatedCall || mType == HandoverCall; }
    bool isSMS() const { return mType == ShortMessage || mType == MobileTerminatedShortMessage; }
    bool isMM() const { return mType == LocationUpdateRequest; }
    size_t lengthV() const override { return 0; }
    void writeV(L3Frame& dest, size_t& wp) const override;
    ParseResult<void> try_parseV(const L3Frame& src, size_t& rp) override;
    ParseResult<void> try_parseV(const L3Frame& src, size_t& rp, size_t expectedLength) override;
    void text(std::ostream& os) const override;
};

// ── Reject Cause IE (GSM 04.08 10.5.3.6) ───────────────────────────────

class L3RejectCauseIE : public L3ProtocolElement {
private:
    MMRejectCause mRejectCause;
public:
    explicit L3RejectCauseIE(MMRejectCause wCause = MMRejectCause::Zero);
    MMRejectCause rejectCause() const { return mRejectCause; }
    size_t lengthV() const override { return 1; }
    void writeV(L3Frame& dest, size_t& wp) const override;
    ParseResult<void> try_parseV(const L3Frame& src, size_t& rp) override;
    ParseResult<void> try_parseV(const L3Frame& src, size_t& rp, size_t expectedLength) override;
    void text(std::ostream& os) const override;
};

// ── Network Name (GSM 04.08 10.5.3.5a) ─────────────────────────────────

class L3NetworkName : public L3ProtocolElement {
private:
    static const size_t maxLen = 93;
    GSMAlphabet mAlphabet;
    char mName[maxLen + 1];
    int mCI;
public:
    L3NetworkName(const char* wName = "", GSMAlphabet alphabet = GSMAlphabet::ALPHABET_7BIT, int wCI = 0);
    size_t lengthV() const override;
    void writeV(L3Frame& dest, size_t& wp) const override;
    ParseResult<void> try_parseV(const L3Frame& src, size_t& rp) override;
    ParseResult<void> try_parseV(const L3Frame& src, size_t& rp, size_t expectedLength) override;
    void text(std::ostream& os) const override;
    const char* name() const { return mName; }
    GSMAlphabet alphabet() const { return mAlphabet; }
};

// ── Time Zone And Time (GSM 04.08 10.5.3.9) ───────────────────────────

class L3TimeZoneAndTime : public L3ProtocolElement {
public:
    enum TimeType : uint8_t {
        LOCAL_TIME,
        UTC_TIME
    };
private:
    uint8_t mYear;
    uint8_t mMonth;
    uint8_t mDay;
    uint8_t mHour;
    uint8_t mMinute;
    uint8_t mSecond;
    uint8_t mTimezone;
    TimeType mType;
public:
    L3TimeZoneAndTime(TimeType type = UTC_TIME);
    uint8_t year() const { return mYear; }
    uint8_t month() const { return mMonth; }
    uint8_t day() const { return mDay; }
    uint8_t hour() const { return mHour; }
    uint8_t minute() const { return mMinute; }
    uint8_t second() const { return mSecond; }
    uint8_t timezone() const { return mTimezone; }
    TimeType type() const { return mType; }
    size_t lengthV() const override { return 7; }
    void writeV(L3Frame& dest, size_t& wp) const override;
    ParseResult<void> try_parseV(const L3Frame& src, size_t& rp) override;
    ParseResult<void> try_parseV(const L3Frame& src, size_t& rp, size_t expectedLength) override;
    void text(std::ostream& os) const override;
};

// ── RAND (GSM 04.08 10.5.3.1) ──────────────────────────────────────────

class L3RAND : public L3ProtocolElement {
private:
    std::vector<uint8_t> mRAND;
public:
    L3RAND();
    explicit L3RAND(const std::vector<uint8_t>& rand);
    const std::vector<uint8_t>& rand() const { return mRAND; }
    size_t lengthV() const override { return 16; }
    void writeV(L3Frame& dest, size_t& wp) const override;
    ParseResult<void> try_parseV(const L3Frame& src, size_t& rp) override;
    ParseResult<void> try_parseV(const L3Frame& src, size_t& rp, size_t expectedLength) override;
    void text(std::ostream& os) const override;
};

// ── SRES (GSM 04.08 10.5.3.2) ──────────────────────────────────────────

class L3SRES : public L3ProtocolElement {
private:
    uint32_t mValue;
public:
    explicit L3SRES(uint32_t wValue = 0);
    uint32_t value() const { return mValue; }
    size_t lengthV() const override { return 4; }
    void writeV(L3Frame& dest, size_t& wp) const override;
    ParseResult<void> try_parseV(const L3Frame& src, size_t& rp) override;
    ParseResult<void> try_parseV(const L3Frame& src, size_t& rp, size_t expectedLength) override;
    void text(std::ostream& os) const override;
};

} // namespace gsml3parser


