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

// SMS CP (Control Part) & RP (Relay Part) Messages — GSM L3 SMS layer
// Spec: 3GPP TS 24.008 sections 9.6, Table 10.6a; 3GPP TS 24.011 sections 7-8
// Reference: ref/osmo-ttcn3-hacks/library/L3_Templates.ttcn — SMS templates (lines 3513-3739)
//            ref/OpenBTS/SMS/SMSMessages.h — CP/RP/TP message classes
//
// L3 header (per 24.008 10.5.1):
//   Byte 0: PD(4)=0x09(SMS) | Skip(4)
//   Byte 1: CP-MTI(8 bits, raw — no NSD field)
//   Body: CP message body (contains RPDU which may contain TPDU)
//
// CP-MTI values (Table 10.6a):
//   CP-DATA  = 0x01, CP-ACK  = 0x04, CP-ERROR = 0x10
//   CP-STATUS = 0x12, CP-SMT = 0x13
//
// RP-MTI values (3-bit field in RP header byte):
//   RP-DATA: MO='000'B(0), MT='001'B(1)
//   RP-ACK:  MO='010'B(2), MT='011'B(3)
//   RP-ERROR: MO='100'B(4), MT='101'B(5)
//   RP-SMMA:  MO='110'B(6), MT='111'B(7)

#pragma once

#include <cstdint>
#include <optional>
#include <ostream>
#include <vector>

#include "../expected.h"
#include "../bitreader.h"
#include "../bitwriter.h"
#include "../types.h"
#include "l3smselements.h"

namespace gsml3parser {

// ── CP Cause (GSM 24.011 10.5.1) ──────────────────────────────────────

enum class CPCause : uint8_t {
    Unspecified = 0,
    CpusNotSupported = 1,
    NoRPLPDU = 2,
    UnknownRPMessageType = 3,
    InvalidRPMessageReference = 4,
    RPUserBusy = 5,
    UnknownRPOriginatorAddress = 6,
    UnknownRPDestinationAddress = 7,
    RPLinkNotAvailable = 8,
    NoRPResponse = 9,
};

const char* CPCause2Str(CPCause cause);

// ── CP-DATA (GSM 24.011 8.1.2) ────────────────────────────────────────
// CP-MTI=0x01: Length(1) | RPDU(variable)
// Bidirectional: MS<->SC

class L3CPData {
    std::vector<uint8_t> mRpdu;
public:
    void setRpdu(std::vector<uint8_t> rpdu) { mRpdu = std::move(rpdu); }
    static constexpr int MTI = 0x01;

    const std::vector<uint8_t>& rpdu() const { return mRpdu; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3CPData> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::SMS; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── CP-ACK (GSM 24.011 8.1.3) ─────────────────────────────────────────
// CP-MTI=0x04: No body
// Bidirectional: MS<->SC

class L3CPAck {
public:
    static constexpr int MTI = 0x04;

    size_t bodyLength() const { return 0; }
    [[nodiscard]] static Expected<L3CPAck> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::SMS; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── CP-ERROR (GSM 24.011 8.1.4) ───────────────────────────────────────
// CP-MTI=0x10: CP-Cause(1 octet, 7-bit value + extension bit)
// Bidirectional: MS<->SC

class L3CPErr {
    CPCause mCause{CPCause::Unspecified};
public:
    static constexpr int MTI = 0x10;

    CPCause cause() const { return mCause; }

    size_t bodyLength() const { return 1; }
    [[nodiscard]] static Expected<L3CPErr> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::SMS; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── CP-STATUS (GSM 24.011 8.1.5) ──────────────────────────────────────
// CP-MTI=0x12: TP-OI(1) | MTI(1) | [TP-Message-Reference(1)]
// MT: SC->MS

class L3CPStatus {
    uint8_t mTpOi{0};
    uint8_t mMti{0};
    bool mHaveMessageRef{false};
    uint8_t mMessageRef{0};
public:
    static constexpr int MTI = 0x12;

    uint8_t tpOi() const { return mTpOi; }
    uint8_t mtiValue() const { return mMti; }
    bool hasMessageRef() const { return mHaveMessageRef; }
    uint8_t messageRef() const { return mMessageRef; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3CPStatus> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::SMS; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── CP-SMT (GSM 24.011 8.1.6) ─────────────────────────────────────────
// CP-MTI=0x13: Length(1) | RPDU(variable)
// MT: SC->MS (Short Message to Telephony)

class L3CPSMT {
    std::vector<uint8_t> mRpdu;
public:
    void setRpdu(std::vector<uint8_t> rpdu) { mRpdu = std::move(rpdu); }
    static constexpr int MTI = 0x13;

    const std::vector<uint8_t>& rpdu() const { return mRpdu; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3CPSMT> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::SMS; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── RP-DATA (GSM 24.011 7.3.1) ────────────────────────────────────────
// RP-MTI: MO='000'B(0), MT='001'B(1)
// Header: Spare(5)|RP-MTI(3) | RP-Message-Reference(1)
// Body: [RP-Originator-Address(LV)] | [RP-Destination-Address(LV)] | RP-User-Data(LV)

class L3RPData {
    uint8_t mRpMti{0};
    uint8_t mMessageRef{0};
    std::optional<L3TPAddress> mOriginatorAddress;
    std::optional<L3TPAddress> mDestinationAddress;
    std::vector<uint8_t> mUserData;
public:
    static constexpr int RP_MTI_MO = 0x00;
    static constexpr int RP_MTI_MT = 0x01;

    uint8_t rpMti() const { return mRpMti; }
    bool isMo() const { return mRpMti == RP_MTI_MO; }
    uint8_t messageRef() const { return mMessageRef; }
    const std::optional<L3TPAddress>& originatorAddress() const { return mOriginatorAddress; }
    const std::optional<L3TPAddress>& destinationAddress() const { return mDestinationAddress; }
    const std::vector<uint8_t>& userData() const { return mUserData; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3RPData> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── RP-ACK (GSM 24.011 7.3.2) ─────────────────────────────────────────
// RP-MTI: MO='010'B(2), MT='011'B(3)
// Header: Spare(5)|RP-MTI(3) | RP-Message-Reference(1)
// No body

class L3RPAck {
    uint8_t mRpMti{0};
    uint8_t mMessageRef{0};
public:
    static constexpr int RP_MTI_MO = 0x02;
    static constexpr int RP_MTI_MT = 0x03;

    void setRpMti(uint8_t v) { mRpMti = v; }
    void setMessageRef(uint8_t v) { mMessageRef = v; }
    uint8_t rpMti() const { return mRpMti; }
    bool isMo() const { return mRpMti == RP_MTI_MO; }
    uint8_t messageRef() const { return mMessageRef; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3RPAck> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── RP-ERROR (GSM 24.011 7.3.4) ───────────────────────────────────────
// RP-MTI: MO='100'B(4), MT='101'B(5)
// Header: Spare(5)|RP-MTI(3) | RP-Message-Reference(1)
// Body: RP-Cause(LV): Length(1) | CauseValue(7 bits + extension bit)

class L3RPError {
    uint8_t mRpMti{0};
    uint8_t mMessageRef{0};
    CPCause mCause{CPCause::Unspecified};
public:
    static constexpr int RP_MTI_MO = 0x04;
    static constexpr int RP_MTI_MT = 0x05;

    void setRpMti(uint8_t v) { mRpMti = v; }
    void setMessageRef(uint8_t v) { mMessageRef = v; }
    void setCause(CPCause v) { mCause = v; }
    uint8_t rpMti() const { return mRpMti; }
    bool isMo() const { return mRpMti == RP_MTI_MO; }
    uint8_t messageRef() const { return mMessageRef; }
    CPCause cause() const { return mCause; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3RPError> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── RP-SMMA (GSM 24.011 7.3.3) ────────────────────────────────────────
// RP-MTI: MO='110'B(6), MT='111'B(7)
// Header: Spare(5)|RP-MTI(3) | RP-Message-Reference(1)
// No body

class L3RPSMMA {
    uint8_t mRpMti{0};
    uint8_t mMessageRef{0};
public:
    static constexpr int RP_MTI_MO = 0x06;
    static constexpr int RP_MTI_MT = 0x07;

    void setRpMti(uint8_t v) { mRpMti = v; }
    void setMessageRef(uint8_t v) { mMessageRef = v; }
    uint8_t rpMti() const { return mRpMti; }
    bool isMo() const { return mRpMti == RP_MTI_MO; }
    uint8_t messageRef() const { return mMessageRef; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3RPSMMA> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// SMS message type names for text output.
const char* smsMessageName(int mti);

} // namespace gsml3parser
