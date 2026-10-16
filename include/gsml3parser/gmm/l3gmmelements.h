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

// GMM Information Elements - GSM L3 GPRS Mobility Management IE definitions
// Spec: 3GPP TS 24.008 section 10.5.7, Table 10.5a
// IE layouts per 3GPP TS 24.008 section 10.5.7 (Table 10.5a).
//
// Bit layout (per spec):
//   PDPContextStatus: TLV(IEI=0x32) | Length(1) | StatusBitmap(2)
//   T3302: TLV(IEI=0x2a) | Length(1) | TimerValue(1)
//   MSNetworkCapability: V-format (variable length, bit fields per 10.5.7.3)
//   RoutingAreaIdentification: MCC/MNC BCD(3) | LAC(2) | RAC(1) = 6 octets
//   DRXParameter: V-format (two value octets, no identifier, per 10.5.5.13)

#pragma once

#include <array>
#include <cstdint>
#include <ostream>
#include <vector>

#include "../expected.h"
#include "../bitreader.h"
#include "../bitwriter.h"
#include "../types.h"

namespace gsml3parser {

// ── GMM Cause (TS 44.068 / TS 24.008 section 10.5.5.14) ───────────────
// Cause values specific to GMM procedures, carried as a single value
// octet at the start of the body of reject and failure messages.

enum class GMMCause : uint8_t {
    Unspecified = 0x00,             // builder sentinel only; must be replaced with a defined cause before wire emission
    IMSI_Unknown_In_HLR = 0x02,     Illegal_MS = 0x03,
    IMEI_Not_Accepted = 0x05,       Illegal_ME = 0x06,
    GPRS_Services_Not_Allowed = 0x07,
    GPRS_And_Non_GPRS_Services_Not_Allowed = 0x08,
    MS_Identity_Cannot_Be_Derived = 0x09,
    Implicitly_Detached = 0x0A,
    PLMN_Not_Allowed = 0x0B,        Location_Area_Not_Allowed = 0x0C,
    Roaming_Not_Allowed_In_Location_Area = 0x0D,
    GPRS_Services_Not_Allowed_In_PLMN = 0x0E,
    No_Suitable_Cells_In_Location_Routing_Area = 0x0F,
    MSC_Temporarily_Not_Reachable = 0x10,
    Network_Failure = 0x11,
    MAC_Failure = 0x14,             Synch_Failure = 0x15,
    Congestion = 0x16,              GSM_Authentication_Unacceptable = 0x17,
    Not_Authorized_For_This_CSG = 0x19,
    No_PDP_Context_Activated = 0x28,
    Semantically_Incorrect_Message = 0x5F,
    Invalid_Mandatory_Information = 0x60,
    Message_Type_Not_Implemented = 0x61,
    Message_Type_Not_Compatible_With_State = 0x62,
    Protocol_Error_Unspecified = 0x6F
};

const char* GMMCause2Str(GMMCause cause);

namespace detail {

// GPRS timer value part, one octet (TS 24.008 section 10.5.7.3):
// high three bits are the time unit, low five bits the value within that
// unit. Units: 0 = 2 s interval, 1 = minutes, 2 = deci-hours, 7 = deactivated.
struct GprsTimerValue {
    uint8_t units{7};
    uint8_t value{0};

    bool operator==(const GprsTimerValue&) const = default;

    [[nodiscard]] constexpr uint8_t octet() const noexcept {
        return static_cast<uint8_t>((units & 0x07u) << 5 | (value & 0x1Fu));
    }

    // Convert a duration in seconds to the unit/value coding: a zero
    // duration deactivates the timer; values up to 62 s use the 2-second
    // interval unit; larger values are converted to minutes and then to
    // deci-hours, with durations that do not fit clamped to the deactivated
    // coding (TS 24.008 section 10.5.7.3).
    [[nodiscard]] static constexpr GprsTimerValue fromSeconds(unsigned s) noexcept {
        if (s == 0) return GprsTimerValue{7, 0};
        if (s <= 62) return GprsTimerValue{0, static_cast<uint8_t>(s / 2)};
        unsigned m = (s + 59) / 60;
        if (m >= 200) return GprsTimerValue{7, 0};
        if (m > 186) return GprsTimerValue{2, 31};
        if (m > 31) return GprsTimerValue{2, static_cast<uint8_t>((m + 5) / 6)};
        return GprsTimerValue{1, static_cast<uint8_t>(m)};
    }
};

} // namespace detail

// ── Attach Type (GSM 24.008 10.5.5.19) ────────────────────────────────

enum class GMMAttachType : uint8_t {
    GPRSAttach = 1,           // '001'B
    CombinedGPRSAndIMSIAttach = 3  // '011'B
};

// ── Update Type (GSM 24.008 10.5.5.18) ────────────────────────────────

enum class GMMUpdateType : uint8_t {
    RAUpdated = 0,                     // '000'B
    CombinedRALAUpdated = 1,           // '001'B
    CombinedRALAWithImsiAttach = 2,    // '010'B
    PeriodicUpdating = 3               // '011'B
};

// ── Detach Type (GSM 24.08 10.5.5.20) ─────────────────────────────────

enum class GMMDetachTypeMO : uint8_t {
    GPRS = 1,              // '001'B
    IMSI = 2,              // '010'B
    CombinedGPRSIMSI = 3   // '011'B
};

enum class GMMDetachTypeMT : uint8_t {
    ReattachRequired = 1,      // '001'B
    ReattachNotRequired = 2,   // '010'B
    IMSIDetach = 3             // '011'B
};

// ── PDP Context Status (GSM 24.008 10.5.7.1) ──────────────────────────
// TLV format: IEI=0x32 | Length(1) | Value(2 octets bitmap)
// Each bit represents a PDP context identifier (1-16).

class L3PDPContextStatus {
    std::array<uint8_t, 2> mValue{};
public:
    static constexpr uint8_t IEI = 0x32;
    L3PDPContextStatus() = default;
    explicit L3PDPContextStatus(std::array<uint8_t, 2> value) : mValue(value) {}

    bool operator==(const L3PDPContextStatus&) const = default;

    uint8_t context(unsigned idx) const {
        if (idx < 1 || idx > 16) return 0;
        --idx;
        return (mValue[idx / 8] >> (idx % 8)) & 1;
    }

    static constexpr size_t lengthV() { return 2; }

    [[nodiscard]] static Expected<L3PDPContextStatus> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── T3302 Timer (TS 44.068 section 9.5) ───────────────────────────────
// TLV format: IEI=0x2a | Length(1) | Value(1 octet)

class L3T3302Timer {
    uint8_t mValue{};
public:
    static constexpr uint8_t IEI = 0x2a;
    L3T3302Timer() = default;
    explicit L3T3302Timer(uint8_t value) : mValue(value) {}

    bool operator==(const L3T3302Timer&) const = default;

    uint8_t value() const { return mValue; }
    static constexpr size_t lengthV() { return 1; }

    [[nodiscard]] static Expected<L3T3302Timer> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── MS Network Capability (GSM 24.008 10.5.7.3) ───────────────────────
// Variable-length bit string. First octet contains fixed fields:
//   GEA1(1)|SMS_via_dedicated(1)|SMS_via_GPRS(1)|UCS2(1)|SS_screening(2)|
//   SOL-SA(1)|Revision_Level(1)
// Subsequent octets encode extended capability bits.

class L3MSNetworkCapability {
    std::vector<uint8_t> mValue;
public:
    L3MSNetworkCapability() = default;
    explicit L3MSNetworkCapability(std::vector<uint8_t> value) : mValue(std::move(value)) {}

    bool operator==(const L3MSNetworkCapability&) const = default;

    unsigned gea1() const;
    unsigned smsViaDedicated() const;
    unsigned smsViaGprs() const;
    unsigned ucs2() const;
    unsigned ssScreening() const;
    unsigned solSa() const;
    unsigned revisionLevel() const;

    size_t lengthV() const { return mValue.size(); }
    const std::vector<uint8_t>& raw() const { return mValue; }

    [[nodiscard]] static Expected<L3MSNetworkCapability> parse(BitReader& br, size_t lengthBytes);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── Routing Area Identification (GSM 24.008 10.5.6.2) ─────────────────
// Fixed: MCC/MNC BCD(3) | LAC(2) | RAC(1) = 6 octets total. The PLMN is
// packed as [MCC2|MCC1][MNC3/F|MCC3][MNC2|MNC1] BCD octets (F fill for a
// 2-digit MNC); digits are stored and exposed in natural written order
// (TS 24.008 section 10.5.1.3).

class L3RoutingAreaIdentification {
    std::array<unsigned, 3> mMCC{};
    std::array<unsigned, 3> mMNC{};
    uint16_t mLAC{};
    uint8_t mRAC{};
public:
    L3RoutingAreaIdentification() = default;
    L3RoutingAreaIdentification(const char* wMCC, const char* wMNC, unsigned wLAC, unsigned wRAC);

    bool operator==(const L3RoutingAreaIdentification&) const = default;

    int mcc() const;
    int mnc() const;
    int lac() const { return mLAC; }
    uint8_t rac() const { return mRAC; }
    static constexpr size_t lengthV() { return 6; }

    [[nodiscard]] static Expected<L3RoutingAreaIdentification> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── DRX Parameter (TS 44.068 section 10.5.5.13) ───────────────────────
// Value format: two octets carried without an identifier.
// Octet 1: splitPGCycleCode(8)
// Octet 2: nonDRXTimer(3)|splitOnCCCH(1)|cnSpecificDRXCycleLength(4)

class L3DRXParameter {
    uint8_t mSplitPGCycleCode{};
    unsigned mNonDRXTimer{};
    unsigned mSplitOnCCCH{};
    unsigned mCNSpecificDRXCycleLength{};
public:
    L3DRXParameter() = default;

    bool operator==(const L3DRXParameter&) const = default;

    uint8_t splitPGCycleCode() const { return mSplitPGCycleCode; }
    unsigned nonDRXTimer() const { return mNonDRXTimer; }
    unsigned splitOnCCCH() const { return mSplitOnCCCH; }
    unsigned cnSpecificDRXCycleLength() const { return mCNSpecificDRXCycleLength; }
    static constexpr size_t lengthV() { return 2; }

    [[nodiscard]] static Expected<L3DRXParameter> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── GMM Cause (TS 44.068 section 9.5) ─────────────────────────────────
// The GMM cause is a single value octet carried without an identifier at
// the start of the body of reject and failure messages.

class L3GMMCauseIE {
    GMMCause mCause{GMMCause::Unspecified};
public:
    L3GMMCauseIE() = default;
    explicit L3GMMCauseIE(GMMCause cause) : mCause(cause) {}

    bool operator==(const L3GMMCauseIE&) const = default;

    GMMCause cause() const { return mCause; }
    static constexpr size_t lengthV() { return 1; }

    [[nodiscard]] static Expected<L3GMMCauseIE> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── Authentication Parameter RAND (GSM 24.008 10.5.6.7) ───────────────
// TV format, IEI=0x21 | RAND(16 octets). The class holds the value only;
// the message writes and reads the identifier octet (TS 44.068 section 9.5).

class L3AuthRAND {
    std::array<uint8_t, 16> mValue{};
public:
    static constexpr uint8_t IEI = 0x21;
    L3AuthRAND() = default;

    bool operator==(const L3AuthRAND&) const = default;

    const std::array<uint8_t, 16>& value() const { return mValue; }
    static constexpr size_t lengthV() { return 16; }

    [[nodiscard]] static Expected<L3AuthRAND> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── Authentication Parameter Response (GSM 24.008 10.5.6.8) ───────────
// TV format, IEI=0x22 | RES(4 octets). The class holds the value only;
// the message writes and reads the identifier octet (TS 44.068 section 9.5).

class L3AuthRES {
    std::array<uint8_t, 4> mValue{};
public:
    static constexpr uint8_t IEI = 0x22;
    L3AuthRES() = default;

    bool operator==(const L3AuthRES&) const = default;

    const std::array<uint8_t, 4>& value() const { return mValue; }
    static constexpr size_t lengthV() { return 4; }

    [[nodiscard]] static Expected<L3AuthRES> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── Authentication Failure Parameter (GSM 24.008 10.5.6.9) ────────────
// TLV: IEI=0x30 | Length(1) | AUTS(variable, typically 14 octets)

class L3AuthFailureParam {
public:
    static constexpr uint8_t IEI = 0x30;
    std::vector<uint8_t> mAUTS;
    L3AuthFailureParam() = default;

    bool operator==(const L3AuthFailureParam&) const = default;

    const std::vector<uint8_t>& auts() const { return mAUTS; }
    size_t lengthV() const { return mAUTS.size(); }

    [[nodiscard]] static Expected<L3AuthFailureParam> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── P-TMSI Signature (GSM 24.008) ─────────────────────────────────────
// TV format, IEI=0x19 | Value(3 octets), TS 44.068 section 9.5.

class L3PTMSISignature {
    std::array<uint8_t, 3> mValue{};
public:
    static constexpr uint8_t IEI = 0x19;
    L3PTMSISignature() = default;

    bool operator==(const L3PTMSISignature&) const = default;

    const std::array<uint8_t, 3>& value() const { return mValue; }
    static constexpr size_t lengthV() { return 3; }

    [[nodiscard]] static Expected<L3PTMSISignature> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── GMM Status (GSM 24.008 10.5.6.11) ─────────────────────────────────
// Single octet: direction(1)|spare(3)|cause(4) ... but actually full byte cause

class L3GMMStatusCause {
    GMMCause mCause{GMMCause::Unspecified};
public:
    L3GMMStatusCause() = default;
    explicit L3GMMStatusCause(GMMCause cause) : mCause(cause) {}

    bool operator==(const L3GMMStatusCause&) const = default;

    GMMCause cause() const { return mCause; }
    static constexpr size_t lengthV() { return 1; }

    [[nodiscard]] static Expected<L3GMMStatusCause> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

} // namespace gsml3parser
