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

// SMS TP (Transport Part) Elements — GSM L3 SMS TPDU structures
// Spec: 3GPP TS 23.040 sections 7.2, 9.2
// Reference: ref/osmo-ttcn3-hacks/library/L3_Templates.ttcn — TPDU templates
//            ref/OpenBTS/SMS/SMSMessages.h — TP layer classes
//
// Bit layout (per GSM 03.40):
//   TP-Header-Octet: TP-MTI(2) | direction-specific fields(6)
//   TP-DA/TP-OA: length(1) | TON_NPI(1) | BCD-digits(variable)
//   TP-PID: 1 octet
//   TP-DCS: 1 octet
//   TP-UD: length(1) | user-data(variable, up to 140 octets)

#pragma once

#include <cstdint>
#include <optional>
#include <ostream>
#include <vector>

#include "../expected.h"
#include "../bitreader.h"
#include "../bitwriter.h"
#include "../types.h"

namespace gsml3parser {

// ── TP Data Coding Scheme (GSM 03.40 9.2.3.10) ────────────────────────

enum class TPDCS : uint8_t {
    Default_Alphabet = 0x00,
    Default_8bit     = 0x08,
    UCS2             = 0x0C,
    Range_Indicator  = 0x40,
    RLA_64           = 0x44,
    RLA_128          = 0x45,
};

const char* TPDCS2Str(TPDCS dcs);

// ── TP Protocol Identifier (GSM 03.40 9.2.3.9) ────────────────────────

enum class TPPID : uint8_t {
    Default               = 0x00,
    GSM                   = 0x01,
    X121                  = 0x03,
    Telex                 = 0x04,
    LandLine              = 0x06,
    SS7_DestinationAccess = 0x0A,
    TeX_Page              = 0x0B,
    Packet_Switched_64k   = 0x0E,
    TeX_Information       = 0x10,
    Packet_Switched_1200  = 0x11,
    SS7_Telephone_User    = 0x12,
    SS7_Telex_User        = 0x13,
    SS7_Direct_Connection = 0x14,
    SS7_MAP               = 0x15,
    SNA                   = 0x16,
    X400_FTAM             = 0x17,
    Telematic_Application = 0x18,
    SCF_Access            = 0x19,
    H323_Video            = 0x1A,
    Internet_ST_FIP       = 0x1B,
    CAP                   = 0x1C,
    SS7_SCCP              = 0x1D,
    X25_Packet_Switched   = 0x1E,
};

const char* TPPID2Str(TPPID pid);

// ── TP Service Centre Time Stamp (GSM 03.40 9.2.4.4) ──────────────────
// 7 octets: year(1) | month(1) | day(1) | hour(1) | minute(1) | second(1) | timezone(1)

struct TPSCTimeStamp {
    uint8_t year{0};
    uint8_t month{0};
    uint8_t day{0};
    uint8_t hour{0};
    uint8_t minute{0};
    uint8_t second{0};
    int8_t  timezone{0};

    [[nodiscard]] static Expected<TPSCTimeStamp> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── TP Address (GSM 03.40 9.1.2.4) — used for TP-DA and TP-OA ─────────
// LV format: Length(1) | TON_NPI(1) | BCD-digits(variable)
// Length = number of following octets (TON_NPI + digits), excluding length byte itself

class L3TPAddress {
    TypeOfNumber mTon{TypeOfNumber::International};
    NumberingPlan mNpi{NumberingPlan::E164};
    std::vector<uint8_t> mDigits;
    uint8_t mLength{0};

public:
    L3TPAddress() = default;
    L3TPAddress(TypeOfNumber ton, NumberingPlan npi, std::vector<uint8_t> digits);

    TypeOfNumber ton() const { return mTon; }
    NumberingPlan npi() const { return mNpi; }
    const std::vector<uint8_t>& digits() const { return mDigits; }
    uint8_t lengthV() const { return mLength; }
    size_t totalLength() const { return 1 + mLength; }

    [[nodiscard]] static Expected<L3TPAddress> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── TP Deliver (GSM 03.40 9.2.2.1) ────────────────────────────────────
// TP-MTI='00'B: mms(1)|lp(1)|spare(1)|sri(1)|udhi(1)|rp(1) | TP-OA | TP-PID | TP-DCS
//   | [TP-SCTS] | TP-UDL | TP-UD

class L3TPDeliver {
    bool mMms{false};
    bool mSri{false};
    bool mUdhi{false};
    bool mRp{false};
    L3TPAddress mOriginatingAddress;
    TPPID mPid{TPPID::Default};
    TPDCS mDcs{TPDCS::Default_Alphabet};
    std::optional<TPSCTimeStamp> mScts;
    uint8_t mUdl{0};
    std::vector<uint8_t> mUserData;

public:
    static constexpr int TP_MTI = 0x00;

    bool mms() const { return mMms; }
    bool sri() const { return mSri; }
    bool udhi() const { return mUdhi; }
    bool rp() const { return mRp; }
    const L3TPAddress& originatingAddress() const { return mOriginatingAddress; }
    TPPID pid() const { return mPid; }
    TPDCS dcs() const { return mDcs; }
    const std::optional<TPSCTimeStamp>& scts() const { return mScts; }
    uint8_t udl() const { return mUdl; }
    const std::vector<uint8_t>& userData() const { return mUserData; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3TPDeliver> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── TP Submit (GSM 03.40 9.2.2.2) ─────────────────────────────────────
// TP-MTI='01'B: rd(1)|vpf(2)|srr(1)|udhi(1)|rp(1) | TP-MR | TP-DA | TP-PID | TP-DCS
//   | [TP-VP] | TP-UDL | TP-UD

class L3TPSubmit {
    bool mRd{true};
    uint8_t mVpf{0};
    bool mSrr{false};
    bool mUdhi{false};
    bool mRp{false};
    uint8_t mMr{0};
    L3TPAddress mDestinationAddress;
    TPPID mPid{TPPID::Default};
    TPDCS mDcs{TPDCS::Default_Alphabet};
    std::vector<uint8_t> mUserData;

public:
    static constexpr int TP_MTI = 0x01;

    bool rd() const { return mRd; }
    uint8_t vpf() const { return mVpf; }
    bool srr() const { return mSrr; }
    bool udhi() const { return mUdhi; }
    bool rp() const { return mRp; }
    uint8_t messageReference() const { return mMr; }
    const L3TPAddress& destinationAddress() const { return mDestinationAddress; }
    TPPID pid() const { return mPid; }
    TPDCS dcs() const { return mDcs; }
    const std::vector<uint8_t>& userData() const { return mUserData; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3TPSubmit> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── TP Status Report (GSM 03.40 9.2.2.3) ──────────────────────────────
// TP-MTI='10'B: spare(6) | TP-MR | TP-DA | TP-PID | TP-DCS | TP-SCTS | TP-STS

class L3TPStatusReport {
    uint8_t mMr{0};
    L3TPAddress mDestinationAddress;
    TPPID mPid{TPPID::Default};
    TPDCS mDcs{TPDCS::Default_Alphabet};
    std::optional<TPSCTimeStamp> mScts;
    uint8_t mSts{0};

public:
    static constexpr int TP_MTI = 0x02;

    uint8_t messageReference() const { return mMr; }
    const L3TPAddress& destinationAddress() const { return mDestinationAddress; }
    TPPID pid() const { return mPid; }
    TPDCS dcs() const { return mDcs; }
    const std::optional<TPSCTimeStamp>& scts() const { return mScts; }
    uint8_t sts() const { return mSts; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3TPStatusReport> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── TP Command (GSM 03.40 9.2.2.5) ────────────────────────────────────
// TP-MTI='11'B: spare(6) | TP-MR | TP-PID | TP-DCS | TP-CMD | [TP-DA]

class L3TPCommand {
    uint8_t mMr{0};
    TPPID mPid{TPPID::Default};
    TPDCS mDcs{TPDCS::Default_Alphabet};
    uint8_t mCmd{0};
    std::optional<L3TPAddress> mAddress;

public:
    static constexpr int TP_MTI = 0x03;

    uint8_t messageReference() const { return mMr; }
    TPPID pid() const { return mPid; }
    TPDCS dcs() const { return mDcs; }
    uint8_t cmd() const { return mCmd; }
    const std::optional<L3TPAddress>& address() const { return mAddress; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3TPCommand> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

} // namespace gsml3parser
