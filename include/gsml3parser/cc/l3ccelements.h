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

    friend std::ostream& operator<<(std::ostream& os, Location loc);
    friend std::ostream& operator<<(std::ostream& os, Progress prog);
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

// ── Connected Number (GSM 04.08 10.5.4.7) ──────────────────────────────
// TLV format: IEI=0x9c, Length(1) | TypeOctet(1) | Digits...
// Structure identical to L3CallingPartyBCDNumber but without presentation/screening

#ifndef GSML3PARSER_L3CONNECTED_NUMBER_DEFINED
#define GSML3PARSER_L3CONNECTED_NUMBER_DEFINED
class L3ConnectedNumber {
    TypeOfNumber mType{TypeOfNumber::Unknown};
    NumberingPlan mPlan{NumberingPlan::Unknown};
    L3BCDDigits mDigits{};
public:
    static constexpr uint8_t IEI = 0x9c;

    L3ConnectedNumber() = default;
    explicit L3ConnectedNumber(const char* wDigits);

    TypeOfNumber type() const { return mType; }
    NumberingPlan plan() const { return mPlan; }
    const char* digits() const { return mDigits.digits(); }

    size_t lengthV() const;
    [[nodiscard]] static Expected<L3ConnectedNumber> parse(BitReader& br, size_t lengthBytes);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};
#endif

// ── Sub Address (GSM 04.08 10.5.4.3) ───────────────────────────────────
// TLV format: IEI=0x9a(CallingParty), 0x9b(CalledParty), Length(1) | NumItems(1) | SubAddressItem...

#ifndef GSML3PARSER_L3SUB_ADDRESS_DEFINED
#define GSML3PARSER_L3SUB_ADDRESS_DEFINED
class L3SubAddress {
public:
    enum Selector : uint8_t {
        ISDN = 0,
        Data = 2,
        Telex = 3
    };

    struct SubAddressItem {
        Selector sel{Selector::ISDN};
        uint8_t len{0};
        std::vector<uint8_t> data;
    };

private:
    std::vector<SubAddressItem> mItems;
public:
    L3SubAddress() = default;

    size_t lengthV() const;
    [[nodiscard]] static Expected<L3SubAddress> parse(BitReader& br, size_t lengthBytes);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;

    const std::vector<SubAddressItem>& items() const { return mItems; }
};
#endif

// ── Redirecting Number (GSM 04.08 10.5.4.13) ──────────────────────────
// TLV format: IEI=0x97, Length(1) | TypeOctet(1) | Digits... | [Reason(1)]

#ifndef GSML3PARSER_L3REDIRECTING_NUMBER_DEFINED
#define GSML3PARSER_L3REDIRECTING_NUMBER_DEFINED
class L3RedirectingNumber {
public:
    static constexpr uint8_t IEI = 0x97;
    enum RedirectReason : uint8_t {
        NoReason = 0,
        UserNotSorted = 1,
       ForwardingAll = 2,
        Conditional = 3
    };

private:
    TypeOfNumber mType{TypeOfNumber::Unknown};
    NumberingPlan mPlan{NumberingPlan::Unknown};
    L3BCDDigits mDigits{};
    bool mHaveReason{false};
    RedirectReason mReason{RedirectReason::NoReason};
public:
    L3RedirectingNumber() = default;
    explicit L3RedirectingNumber(const char* wDigits);

    TypeOfNumber type() const { return mType; }
    NumberingPlan plan() const { return mPlan; }
    const char* digits() const { return mDigits.digits(); }
    bool hasReason() const { return mHaveReason; }
    RedirectReason reason() const { return mReason; }

    size_t lengthV() const;
    [[nodiscard]] static Expected<L3RedirectingNumber> parse(BitReader& br, size_t lengthBytes);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};
#endif

// ── CLIR Suppression (GSM 04.08 10.5.4.16) ────────────────────────────
// TV format: IEI=0xc1, Value(1 octet)

#ifndef GSML3PARSER_L3CLIR_SUPPRESSION_DEFINED
#define GSML3PARSER_L3CLIR_SUPPRESSION_DEFINED
class L3CLIRSuppression {
public:
    static constexpr uint8_t IEI = 0xc1;
private:
    unsigned mValue{0};
public:
    L3CLIRSuppression() = default;
    explicit L3CLIRSuppression(unsigned wValue);

    unsigned value() const { return mValue; }
    static constexpr size_t lengthV() { return 1; }

    [[nodiscard]] static Expected<L3CLIRSuppression> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    bool operator==(const L3CLIRSuppression&) const = default;
};
#endif

// ── CLIR Invocation (GSM 04.08 10.5.4.17) ─────────────────────────────
// TV format: IEI=0xc2, Value(1 octet)

#ifndef GSML3PARSER_L3CLIR_INVOCATION_DEFINED
#define GSML3PARSER_L3CLIR_INVOCATION_DEFINED
class L3CLIRInvocation {
public:
    static constexpr uint8_t IEI = 0xc2;
private:
    unsigned mValue{0};
public:
    L3CLIRInvocation() = default;
    explicit L3CLIRInvocation(unsigned wValue);

    unsigned value() const { return mValue; }
    static constexpr size_t lengthV() { return 1; }

    [[nodiscard]] static Expected<L3CLIRInvocation> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    bool operator==(const L3CLIRInvocation&) const = default;
};
#endif

// ── Network CC Capabilities (GSM 04.08 10.5.4.15) ─────────────────────
// TLV format: IEI=0x7a, Length(1) | CapabilityBits(2 octets min)

#ifndef GSML3PARSER_L3NETWORK_CC_CAPABILITIES_DEFINED
#define GSML3PARSER_L3NETWORK_CC_CAPABILITIES_DEFINED
class L3NetworkCCCapabilities {
public:
    static constexpr uint8_t IEI = 0x7a;
private:
    std::vector<uint8_t> mCapabilities;
public:
    L3NetworkCCCapabilities() = default;

    size_t lengthV() const;
    [[nodiscard]] static Expected<L3NetworkCCCapabilities> parse(BitReader& br, size_t lengthBytes);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};
#endif

// ── Low Layer Compatibility (GSM 04.08 10.5.4.14) ─────────────────────
// TLV format: IEI=0x86, variable length

#ifndef GSML3PARSER_L3LOW_LAYER_COMPATIBILITY_DEFINED
#define GSML3PARSER_L3LOW_LAYER_COMPATIBILITY_DEFINED
class L3LowLayerCompatibility {
public:
    static constexpr uint8_t IEI = 0x86;
private:
    std::vector<uint8_t> mData;
public:
    L3LowLayerCompatibility() = default;

    size_t lengthV() const;
    [[nodiscard]] static Expected<L3LowLayerCompatibility> parse(BitReader& br, size_t lengthBytes);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};
#endif

// ── High Layer Compatibility (GSM 04.08 10.5.4.14) ────────────────────
// TLV format: IEI=0x87, variable length

#ifndef GSML3PARSER_L3HIGH_LAYER_COMPATIBILITY_DEFINED
#define GSML3PARSER_L3HIGH_LAYER_COMPATIBILITY_DEFINED
class L3HighLayerCompatibility {
public:
    static constexpr uint8_t IEI = 0x87;
private:
    std::vector<uint8_t> mData;
public:
    L3HighLayerCompatibility() = default;

    size_t lengthV() const;
    [[nodiscard]] static Expected<L3HighLayerCompatibility> parse(BitReader& br, size_t lengthBytes);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};
#endif

// ── User-User (GSM 04.08 10.5.4.27) ───────────────────────────────────
// TLV format: IEI=0x75, variable length

#ifndef GSML3PARSER_L3USER_USER_DEFINED
#define GSML3PARSER_L3USER_USER_DEFINED
class L3UserUser {
public:
    static constexpr uint8_t IEI = 0x75;
private:
    std::vector<uint8_t> mData;
public:
    L3UserUser() = default;

    size_t lengthV() const;
    [[nodiscard]] static Expected<L3UserUser> parse(BitReader& br, size_t lengthBytes);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};
#endif

// ── Priority (GSM 04.08 10.5.4.19) ────────────────────────────────────
// TV format: IEI=0x88, Value(1 octet): spare(1)|request(1)|priorityLevel(3)|spare(3)

#ifndef GSML3PARSER_L3PRIORITY_DEFINED
#define GSML3PARSER_L3PRIORITY_DEFINED
class L3Priority {
public:
    static constexpr uint8_t IEI = 0x88;
private:
    unsigned mPriorityLevel{0};
    bool mRequest{false};
public:
    L3Priority() = default;
    L3Priority(unsigned level, bool request);

    unsigned priorityLevel() const { return mPriorityLevel; }
    bool request() const { return mRequest; }
    static constexpr size_t lengthV() { return 1; }

    [[nodiscard]] static Expected<L3Priority> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    bool operator==(const L3Priority&) const = default;
};
#endif

// ── Stream Identifier (GSM 04.08 10.5.4.29) ───────────────────────────
// TV format: IEI=0x8e, Value(1 octet): spare(3)|VBS/VGCS(1)|stream ID(4)

#ifndef GSML3PARSER_L3STREAM_IDENTIFIER_DEFINED
#define GSML3PARSER_L3STREAM_IDENTIFIER_DEFINED
class L3StreamIdentifier {
public:
    static constexpr uint8_t IEI = 0x8e;
private:
    unsigned mStreamId{0};
    bool mVBS{false};
public:
    L3StreamIdentifier() = default;
    L3StreamIdentifier(unsigned id, bool vbs);

    unsigned streamId() const { return mStreamId; }
    bool vbs() const { return mVBS; }
    static constexpr size_t lengthV() { return 1; }

    [[nodiscard]] static Expected<L3StreamIdentifier> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    bool operator==(const L3StreamIdentifier&) const = default;
};
#endif

// ── Allowed Actions (GSM 04.08 10.5.4.2) ──────────────────────────────
// TLV format: IEI=0x92, Length(1) | Flags(2 octets): spare(5)|action(11)

#ifndef GSML3PARSER_L3ALLOWED_ACTIONS_DEFINED
#define GSML3PARSER_L3ALLOWED_ACTIONS_DEFINED
class L3AllowedActions {
public:
    static constexpr uint8_t IEI = 0x92;
private:
    uint16_t mFlags{0};
public:
    L3AllowedActions() = default;
    explicit L3AllowedActions(uint16_t flags);

    uint16_t flags() const { return mFlags; }
    static constexpr size_t lengthV() { return 2; }

    [[nodiscard]] static Expected<L3AllowedActions> parse(BitReader& br, size_t lengthBytes);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    bool operator==(const L3AllowedActions&) const = default;
};
#endif

// ── CC Capabilities (GSM 04.08 10.5.4.4) ──────────────────────────────
// TLV format: IEI=0x51, Length(1) | CapabilityBits(1 octet min): ext(1)|cap(7)

#ifndef GSML3PARSER_L3CC_CAPABILITIES_DEFINED
#define GSML3PARSER_L3CC_CAPABILITIES_DEFINED
class L3CCCapabilities {
public:
    static constexpr uint8_t IEI = 0x51;
private:
    std::vector<uint8_t> mCapabilities;
public:
    L3CCCapabilities() = default;

    size_t lengthV() const;
    [[nodiscard]] static Expected<L3CCCapabilities> parse(BitReader& br, size_t lengthBytes);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};
#endif

// ── Backup Bearer Capability (GSM 04.08 10.5.4.5b) ────────────────────
// TLV format: IEI=0x7c, similar to L3BearerCapability

#ifndef GSML3PARSER_L3BACKUP_BEARER_CAPABILITY_DEFINED
#define GSML3PARSER_L3BACKUP_BEARER_CAPABILITY_DEFINED
class L3BackupBearerCapability {
public:
    static constexpr uint8_t IEI = 0x7c;
private:
    uint8_t mOctet3{0x0f};
    std::vector<uint8_t> mOctet3a;
public:
    L3BackupBearerCapability() = default;

    size_t lengthV() const;
    [[nodiscard]] static Expected<L3BackupBearerCapability> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;

    uint8_t octet3() const { return mOctet3; }
    const std::vector<uint8_t>& octet3a() const { return mOctet3a; }
};
#endif

// ── SS Operation Codes (GSM 04.80 section 4.5) ────────────────────────
// Reference: ref/osmo-ttcn3-hacks/library/SS_Templates.ttcn SS_Op_Code enum

#ifndef GSML3PARSER_SS_OPCODE_DEFINED
#define GSML3PARSER_SS_OPCODE_DEFINED
enum class SSOpCode : uint8_t {
    RegisterSS = 0x0A,
    EraseSS = 0x0B,
    ActivateSS = 0x0C,
    DeactivateSS = 0x0D,
    InterrogateSS = 0x0E,
    NotifySS = 0x10,
    RegisterPassword = 0x11,
    GetPassword = 0x12,
    ProcessUSSData = 0x13,
    ForwardCheckSSInd = 0x26,
    ProcessUSSReq = 0x3B,
    USSRequest = 0x3C,
    USSNotify = 0x3D,
    ForwardCUGInfo = 0x78,
    SplitMPTY = 0x79,
    RetrieveMPTY = 0x7A,
    HoldMPTY = 0x7B,
    BuildMPTY = 0x7C,
    ForwardChargeAdvice = 0x7D
};

[[nodiscard]] std::string_view ssOpCodeName(SSOpCode code);
#endif

// ── SS Error Codes (GSM 04.80 section 4.5) ────────────────────────────
// Reference: ref/osmo-ttcn3-hacks/library/SS_Templates.ttcn SS_Err_Code enum

#ifndef GSML3PARSER_SS_ERROR_CODE_DEFINED
#define GSML3PARSER_SS_ERROR_CODE_DEFINED
enum class SSErrorCode : uint8_t {
    UnknownSubscriber = 0x01,
    IllegalSubscriber = 0x09,
    BearerServiceNotProvisioned = 0x0A,
    TeleserviceNotProvisioned = 0x0B,
    IllegalEquipment = 0x0C,
    CallBarred = 0x0D,
    IllegalSSOperation = 0x10,
    SSErrorStatus = 0x11,
    SSNotAvailable = 0x12,
    SSSubscriptionViolation = 0x13,
    SSIncompatibility = 0x14,
    FacilityNotSupported = 0x15,
    AbsentSubscriber = 0x1B,
    SystemFailure = 0x22,
    DataMissing = 0x23,
    UnexpectedDataValue = 0x24,
    PWRegistrationFailure = 0x25,
    NegativePWCheck = 0x26,
    NumPWAttemptsViolation = 0x2B,
    UnknownAlphabet = 0x47,
    USSDBusy = 0x48,
    MaxMPTYParticipants = 0x7E,
    ResourcesNotAvailable = 0x7F
};

[[nodiscard]] std::string_view ssErrorCodeName(SSErrorCode code);
#endif

// ── SS Facility OpCode IE (TCAP component parser) ─────────────────────
// Parses the TCAP-level op_code from SS Facility data.
// TCAP component tags: Invoke=0x81, ReturnResult=0x82, ReturnError=0x83, Reject=0x84

#ifndef GSML3PARSER_L3FACILITY_OPCODE_DEFINED
#define GSML3PARSER_L3FACILITY_OPCODE_DEFINED
class L3FacilityOpCode {
public:
    enum ComponentType : uint8_t {
        Invoke = 0x81,
        ReturnResult = 0x82,
        ReturnError = 0x83,
        Reject = 0x84
    };

private:
    ComponentType mComponent{ComponentType::Invoke};
    int8_t mInvokeId{0};
    SSOpCode mOpCode{SSOpCode::RegisterSS};
    SSErrorCode mErrorCode{SSErrorCode::UnknownSubscriber};
    bool mHasErrorCode{false};
    std::vector<uint8_t> mParameters;

public:
    L3FacilityOpCode() = default;
    L3FacilityOpCode(ComponentType comp, int8_t invokeId, SSOpCode op, std::vector<uint8_t> params);
    L3FacilityOpCode(ComponentType comp, int8_t invokeId, SSErrorCode err);

    ComponentType component() const { return mComponent; }
    int8_t invokeId() const { return mInvokeId; }
    SSOpCode opCode() const { return mOpCode; }
    bool hasErrorCode() const { return mHasErrorCode; }
    SSErrorCode errorCode() const { return mErrorCode; }
    const std::vector<uint8_t>& parameters() const { return mParameters; }

    [[nodiscard]] static Expected<L3FacilityOpCode> parse(const std::string& facilityData);
    void write(BitWriter& bw) const;
    size_t lengthV() const;
    void text(std::ostream& os) const;
};
#endif

// ── USSD Data IE (GSM 02.90 / GSM 04.80) ──────────────────────────────
// Parses USSD-specific content from SS Facility data.
// Structure: InvokeId(1) | OpCode(1) | DCS(1) | USSDString(7-bit packed)...
// DCS per GSM 02.90 section 4.1.1: alphabet(4) | language(4)

#ifndef GSML3PARSER_L3USSDDATA_DEFINED
#define GSML3PARSER_L3USSDDATA_DEFINED
class L3USSDData {
public:
    enum Alphabet : uint8_t {
        DefaultAlphabet = 0,
        ExtendedCIDAlphabet = 1,
        ShiftGSMtoUCS2 = 4,
        UCS2 = 6
    };

private:
    int8_t mInvokeId{0};
    SSOpCode mOpCode{SSOpCode::ProcessUSSData};
    uint8_t mDcs{0x0F};
    std::vector<uint8_t> mRawUssdString;
    bool mIsResult{false};

public:
    L3USSDData() = default;
    L3USSDData(int8_t invokeId, SSOpCode op, uint8_t dcs, const std::vector<uint8_t>& ussdString, bool isResult = false);

    int8_t invokeId() const { return mInvokeId; }
    SSOpCode opCode() const { return mOpCode; }
    uint8_t dcs() const { return mDcs; }
    Alphabet alphabet() const { return static_cast<Alphabet>(mDcs & 0x0F); }
    unsigned language() const { return (mDcs >> 4) & 0x0F; }
    const std::vector<uint8_t>& rawUssdString() const { return mRawUssdString; }
    bool isResult() const { return mIsResult; }

    [[nodiscard]] static Expected<L3USSDData> parse(const std::string& facilityData, SSOpCode opCode);
    void write(BitWriter& bw) const;
    size_t lengthV() const;
    void text(std::ostream& os) const;

    [[nodiscard]] std::string decodeUssdString() const;
    static std::vector<uint8_t> encodeUssdString(const std::string& text);
};
#endif

} // namespace gsml3parser
