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

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ostream>
#include <string>
#include <vector>

#include "../expected.h"
#include "../bitreader.h"
#include "../bitwriter.h"
#include "../types.h"
#include "../enums.h"

namespace gsml3parser {

// ── Bearer Capability (GSM 04.08 10.5.4.5) ──────────────────────────────

#ifndef GSML3PARSER_L3BEARER_CAPABILITY_DEFINED
#define GSML3PARSER_L3BEARER_CAPABILITY_DEFINED
class L3BearerCapability {
    uint8_t mOctet3{0x0f};
    std::vector<uint8_t> mOctet3a;
public:
    L3BearerCapability() = default;

    size_t lengthV() const;
    [[nodiscard]] static Expected<L3BearerCapability> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;

    uint8_t octet3() const { return mOctet3; }
    const std::vector<uint8_t>& octet3a() const { return mOctet3a; }
    bool getHalfRateSupport() const { return mOctet3 & 0x40; }
};
#endif

// ── Supported Codec List (GSM 04.08 10.5.4.32) ─────────────────────────

#ifndef GSML3PARSER_L3SUPPORTED_CODEC_LIST_DEFINED
#define GSML3PARSER_L3SUPPORTED_CODEC_LIST_DEFINED
class L3SupportedCodecList {
    std::vector<uint8_t> mGsmCodecs;
    std::vector<uint8_t> mUmtsCodecs;
public:
    L3SupportedCodecList() = default;

    size_t lengthV() const;
    [[nodiscard]] static Expected<L3SupportedCodecList> parse(BitReader& br, size_t lengthBytes);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;

    bool isGsmPresent() const { return !mGsmCodecs.empty(); }
    bool isUmtsPresent() const { return !mUmtsCodecs.empty(); }
    const std::vector<uint8_t>& gsmCodecs() const { return mGsmCodecs; }
    const std::vector<uint8_t>& umtsCodecs() const { return mUmtsCodecs; }
};
#endif

// ── BCD Digits utility ──────────────────────────────────────────────────

#ifndef GSML3PARSER_L3BCD_DIGITS_DEFINED
#define GSML3PARSER_L3BCD_DIGITS_DEFINED
class L3BCDDigits {
    static constexpr size_t maxDigits = 20;
    char mDigits[maxDigits + 1]{};
public:
    L3BCDDigits() = default;
    explicit L3BCDDigits(const char* wDigits);
    L3BCDDigits(const L3BCDDigits& other);
    L3BCDDigits& operator=(const L3BCDDigits& other);

    [[nodiscard]] Expected<void> parse(BitReader& br, size_t numOctets, bool international = false);
    void write(BitWriter& bw) const;
    size_t lengthV() const;
    size_t size() const { return std::strlen(mDigits); }
    const char* digits() const { return mDigits; }
};

std::ostream& operator<<(std::ostream& os, const L3BCDDigits& digits);
#endif

// ── Called Party BCD Number (GSM 04.08 10.5.4.7) ───────────────────────

#ifndef GSML3PARSER_L3CALLED_PARTY_BCD_NUMBER_DEFINED
#define GSML3PARSER_L3CALLED_PARTY_BCD_NUMBER_DEFINED
class L3CalledPartyBCDNumber {
    TypeOfNumber mType{TypeOfNumber::Unknown};
    NumberingPlan mPlan{NumberingPlan::Unknown};
    L3BCDDigits mDigits{};
public:
    L3CalledPartyBCDNumber() = default;
    explicit L3CalledPartyBCDNumber(const char* wDigits);

    TypeOfNumber type() const { return mType; }
    NumberingPlan plan() const { return mPlan; }
    const char* digits() const { return mDigits.digits(); }

    size_t lengthV() const;
    [[nodiscard]] static Expected<L3CalledPartyBCDNumber> parse(BitReader& br, size_t lengthBytes);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};
#endif

// ── Calling Party BCD Number (GSM 04.08 10.5.4.9) ──────────────────────

#ifndef GSML3PARSER_L3CALLING_PARTY_BCD_NUMBER_DEFINED
#define GSML3PARSER_L3CALLING_PARTY_BCD_NUMBER_DEFINED
class L3CallingPartyBCDNumber {
    TypeOfNumber mType{TypeOfNumber::Unknown};
    NumberingPlan mPlan{NumberingPlan::Unknown};
    L3BCDDigits mDigits{};
    bool mHaveOctet3a{false};
    int mPresentationIndicator{0};
    int mScreeningIndicator{0};
public:
    L3CallingPartyBCDNumber() = default;
    explicit L3CallingPartyBCDNumber(const char* wDigits);

    TypeOfNumber type() const { return mType; }
    NumberingPlan plan() const { return mPlan; }
    const char* digits() const { return mDigits.digits(); }

    size_t lengthV() const;
    [[nodiscard]] static Expected<L3CallingPartyBCDNumber> parse(BitReader& br, size_t lengthBytes);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};
#endif

// ── Cause Element (GSM 04.08 10.5.4.11) ────────────────────────────────

#ifndef GSML3PARSER_L3CAUSE_ELEMENT_DEFINED
#define GSML3PARSER_L3CAUSE_ELEMENT_DEFINED
class L3CauseElement {
public:
    using Location = CCCauseLocation;
    using Cause = CCCause;
private:
    Location mLocation{Location::Private_Serving_Local};
    Cause mCause{Cause::Normal_Call_Clearing};
public:
    L3CauseElement() = default;
    L3CauseElement(Cause wCause, Location wLocation);

    Location location() const { return mLocation; }
    Cause cause() const { return mCause; }
    static constexpr size_t lengthV() { return 2; }

    [[nodiscard]] static Expected<L3CauseElement> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    bool operator==(const L3CauseElement&) const = default;
};
#endif

// ── Call State (GSM 04.08 10.5.4.6) ────────────────────────────────────

#ifndef GSML3PARSER_L3CALL_STATE_DEFINED
#define GSML3PARSER_L3CALL_STATE_DEFINED
class L3CallState {
    unsigned mCallState{0};
public:
    L3CallState() = default;
    explicit L3CallState(unsigned wCallState);

    unsigned callState() const { return mCallState; }
    static constexpr size_t lengthV() { return 1; }

    [[nodiscard]] static Expected<L3CallState> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    bool operator==(const L3CallState&) const = default;
};
#endif

// ── Progress Indicator (GSM 04.08 10.5.4.21) ───────────────────────────

#ifndef GSML3PARSER_L3PROGRESS_INDICATOR_DEFINED
#define GSML3PARSER_L3PROGRESS_INDICATOR_DEFINED
class L3ProgressIndicator {
public:
    enum Location : uint8_t {
        User = 0,
        PrivateServingLocal = 1,
        PublicServingLocal = 2,
        PublicServingRemote = 4,
        PrivateServingRemote = 5,
        BeyondInternetworking = 10
    };
    enum Progress : uint8_t {
        Unspecified = 0,
        NotISDN = 1,
        DestinationNotISDN = 2,
        OriginationNotISDN = 3,
        ReturnedToISDN = 4,
        InBandAvailable = 8,
        EndToEndISDN = 0x20,
        Queuing = 0x40
    };
private:
    Location mLocation{Location::PrivateServingLocal};
    Progress mProgress{Progress::Unspecified};
public:
    L3ProgressIndicator() = default;
    L3ProgressIndicator(Progress wProgress, Location wLocation);

    Location location() const { return mLocation; }
    Progress progress() const { return mProgress; }
    static constexpr size_t lengthV() { return 2; }

    [[nodiscard]] static Expected<L3ProgressIndicator> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    bool operator==(const L3ProgressIndicator&) const = default;
};
#endif

// ── Keypad Facility (GSM 04.08 10.5.4.17) ──────────────────────────────

#ifndef GSML3PARSER_L3KEYPAD_FACILITY_DEFINED
#define GSML3PARSER_L3KEYPAD_FACILITY_DEFINED
class L3KeypadFacility {
    char mIA5{0};
public:
    L3KeypadFacility() = default;
    explicit L3KeypadFacility(char wIA5);

    char ia5() const { return mIA5; }
    static constexpr size_t lengthV() { return 1; }

    [[nodiscard]] static Expected<L3KeypadFacility> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    bool operator==(const L3KeypadFacility&) const = default;
};
#endif

// ── Signal (GSM 04.08 10.5.4.23) ───────────────────────────────────────

#ifndef GSML3PARSER_L3SIGNAL_DEFINED
#define GSML3PARSER_L3SIGNAL_DEFINED
class L3Signal {
public:
    enum SignalValues : uint8_t {
        SignalDialToneOn = 0,
        SignalRingBackToneOn = 1,
        SignalInterceptToneOn = 2,
        SignalNetworkCongestionToneOn = 3,
        SignalBusyToneOn = 4,
        SignalConfirmToneOn = 5,
        SignalAnswerToneOn = 6,
        SignalCallWaitingToneOn = 7,
        SignalTonesOff = 0x3f,
        SignalAlertingOff = 0x4f
    };
private:
    SignalValues mSignalValue{SignalValues::SignalRingBackToneOn};
public:
    L3Signal() = default;
    explicit L3Signal(SignalValues tone);

    SignalValues signalValue() const { return mSignalValue; }
    static constexpr size_t lengthV() { return 1; }

    [[nodiscard]] static Expected<L3Signal> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    bool operator==(const L3Signal&) const = default;
};
#endif

// ── Repeat Indicator (GSM 04.08 10.5.4.4) ──────────────────────────────
// TV format: IEI=0x0d, Value=4 bits

#ifndef GSML3PARSER_L3REPEAT_INDICATOR_DEFINED
#define GSML3PARSER_L3REPEAT_INDICATOR_DEFINED
class L3RepeatIndicator {
    unsigned mValue{0};
public:
    L3RepeatIndicator() = default;
    explicit L3RepeatIndicator(unsigned wValue);

    unsigned value() const { return mValue; }
    static constexpr size_t lengthV() { return 0; }

    [[nodiscard]] static Expected<L3RepeatIndicator> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    bool operator==(const L3RepeatIndicator&) const = default;
};
#endif

// ── Supplementary Service Facility IE (GSM 04.08 10.5.4.1) ─────────────

#ifndef GSML3PARSER_L3SUPSERV_FACILITY_IE_DEFINED
#define GSML3PARSER_L3SUPSERV_FACILITY_IE_DEFINED
class L3SupServFacilityIE {
    std::string mData;
public:
    L3SupServFacilityIE() = default;
    explicit L3SupServFacilityIE(const std::string& wData);

    const std::string& data() const { return mData; }
    size_t lengthV() const;

    [[nodiscard]] static Expected<L3SupServFacilityIE> parse(BitReader& br, size_t lengthBytes);
    [[nodiscard]] static Expected<L3SupServFacilityIE> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};
#endif

// ── Supplementary Service Version Indicator (24.008 10.5.4.24) ─────────

#ifndef GSML3PARSER_L3SUPSERV_VERSION_INDICATOR_DEFINED
#define GSML3PARSER_L3SUPSERV_VERSION_INDICATOR_DEFINED
class L3SupServVersionIndicator {
    unsigned mVersion{0};
public:
    L3SupServVersionIndicator() = default;

    unsigned version() const { return mVersion; }
    static constexpr size_t lengthV() { return 1; }

    [[nodiscard]] static Expected<L3SupServVersionIndicator> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    bool operator==(const L3SupServVersionIndicator&) const = default;
};
#endif

} // namespace gsml3parser
