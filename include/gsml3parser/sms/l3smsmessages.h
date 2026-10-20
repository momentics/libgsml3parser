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

// SMS CP (Control Part) & RP (Relay Part) Messages - GSM L3 SMS layer
// Spec: 3GPP TS 24.008 sections 9.6, Table 10.6a and 3GPP TS 24.011
// sections 7-8 for the CP messages; 3GPP TS 24.079 sections 7-8 for the RP
// messages. TPDU structures per 3GPP TS 23.040.
//
// L3 header (per 24.008 10.5.1):
//   Byte 0: TI(3) | TIF(1) | PD(4)=0x09(SMS)
//   Byte 1: CP-MTI in the low five bits; the three high bits are spare
//     (TS 24.011 section 8.2) - no NSD field
//   Body: CP message body (contains RPDU which may contain TPDU)
//
// CP-MTI values (Table 10.6a):
//   CP-DATA  = 0x01, CP-ACK  = 0x04, CP-ERROR = 0x10
//   CP-STATUS = 0x12, CP-SMT = 0x13
//
// RP header octet: reserved(5)=0 | RP-MTI(3) (TS 24.079 section 7.2.1,
// Table 8.3). The MTI is direction dependent; a downlink (network to MS)
// message carries the uplink (MS to network) value plus one:
//   RP-DATA:  uplink=0, downlink=1
//   RP-ACK:   uplink=2, downlink=3
//   RP-ERROR: uplink=4, downlink=5
//   RP-SMMA:  uplink=6, downlink=7

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

    friend struct Builder;
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

    struct Builder {
        std::vector<uint8_t> mRpdu;

        /// Set the RPDU data.
        Builder& rpdu(std::vector<uint8_t> v) { mRpdu = std::move(v); return *this; }
        /// Build the final message.
        [[nodiscard]] L3CPData build() const {
            L3CPData msg;
            msg.mRpdu = mRpdu;
            return msg;
        }
    };

    static Builder builder() { return Builder{}; }
};

// ── CP-ACK (GSM 24.011 8.1.3) ─────────────────────────────────────────
// CP-MTI=0x04: No body
// Bidirectional: MS<->SC

class L3CPAck {
    friend struct Builder;
public:
    static constexpr int MTI = 0x04;

    size_t bodyLength() const { return 0; }
    [[nodiscard]] static Expected<L3CPAck> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::SMS; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }

    struct Builder {
        /// Build the final message.
        [[nodiscard]] L3CPAck build() const {
            return L3CPAck{};
        }
    };

    static Builder builder() { return Builder{}; }
};

// ── CP-ERROR (GSM 24.011 8.1.4) ───────────────────────────────────────
// CP-MTI=0x10: CP-Cause(1 octet, 7-bit value + extension bit)
// Bidirectional: MS<->SC

class L3CPErr {
    CPCause mCause{CPCause::Unspecified};

    friend struct Builder;
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

    struct Builder {
        CPCause mCause{CPCause::Unspecified};

        /// Set the CP cause.
        Builder& cause(CPCause v) { mCause = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3CPErr build() const {
            L3CPErr msg;
            msg.mCause = mCause;
            return msg;
        }
    };

    static Builder builder() { return Builder{}; }
};

// ── CP-STATUS (GSM 24.011 8.1.5) ──────────────────────────────────────
// CP-MTI=0x12: TP-OI(1) | MTI(1) | [TP-Message-Reference(1)]
// MT: SC->MS

class L3CPStatus {
    uint8_t mTpOi{0};
    uint8_t mMti{0};
    bool mHaveMessageRef{false};
    uint8_t mMessageRef{0};

    friend struct Builder;
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

    struct Builder {
        uint8_t mTpOi{0};
        uint8_t mMti{0};
        bool mHaveMessageRef{false};
        uint8_t mMessageRef{0};

        /// Set the TP-OI field.
        Builder& tpOi(uint8_t v) { mTpOi = v; return *this; }
        /// Set the MTI value field.
        Builder& mtiValue(uint8_t v) { mMti = v; return *this; }
        /// Set whether a message reference is present.
        Builder& haveMessageRef(bool v) { mHaveMessageRef = v; return *this; }
        /// Set the message reference value.
        Builder& messageRef(uint8_t v) { mMessageRef = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3CPStatus build() const {
            L3CPStatus msg;
            msg.mTpOi = mTpOi;
            msg.mMti = mMti;
            msg.mHaveMessageRef = mHaveMessageRef;
            msg.mMessageRef = mMessageRef;
            return msg;
        }
    };

    static Builder builder() { return Builder{}; }
};

// ── CP-SMT (GSM 24.011 8.1.6) ─────────────────────────────────────────
// CP-MTI=0x13: Length(1) | RPDU(variable)
// MT: SC->MS (Short Message to Telephony)

class L3CPSMT {
    std::vector<uint8_t> mRpdu;

    friend struct Builder;
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

    struct Builder {
        std::vector<uint8_t> mRpdu;

        /// Set the RPDU data.
        Builder& rpdu(std::vector<uint8_t> v) { mRpdu = std::move(v); return *this; }
        /// Build the final message.
        [[nodiscard]] L3CPSMT build() const {
            L3CPSMT msg;
            msg.mRpdu = mRpdu;
            return msg;
        }
    };

    static Builder builder() { return Builder{}; }
};

// ── Relay Part MTI (TS 24.079 section 7.2.1, Table 8.3) ───────────────

// Relay layer message type indicator values (TS 24.079 section 7.2.1,
// Table 8.3). The MTI is direction dependent: downlink messages carry the
// uplink value plus one.
enum class RPMTI : uint8_t {
    DataUplink = 0x00,
    DataDownlink = 0x01,
    AckUplink = 0x02,
    AckDownlink = 0x03,
    ErrorUplink = 0x04,
    ErrorDownlink = 0x05,
    SMMAUplink = 0x06,
    SMMADownlink = 0x07,
};

// ── RP-DATA (GSM 24.011 7.3.1) ────────────────────────────────────────
// Header: reserved(5)=0 | RP-MTI(3) | RP-Message-Reference(1). The MTI is
// direction dependent (TS 24.079 section 7.2.1, Table 8.3): uplink Data = 0,
// downlink Data = 1.
// Body: RP-Originator-SM-SC-Address(LV) | RP-Destination-SM-SC-Address(LV)
//       | RP-User-Data(LV). Both SMSC addresses are always present; an
//       empty address is a single zero length octet.

class L3RPData {
    // Raw RP-MTI value (0..7); even = uplink, odd = downlink (Table 8.3).
    uint8_t mRpMti{static_cast<uint8_t>(RPMTI::DataUplink)};
    uint8_t mMessageRef{0};
    std::optional<L3TPAddress> mOriginatorAddress;
    std::optional<L3TPAddress> mDestinationAddress;
    std::vector<uint8_t> mUserData;

    friend struct Builder;
public:
    /// Raw RP-MTI value from the header octet (TS 24.079 Table 8.3).
    [[nodiscard]] RPMTI mti() const { return static_cast<RPMTI>(mRpMti); }
    /// True for the downlink (network to MS) direction; bit 0 of the MTI.
    [[nodiscard]] bool isDownlink() const { return (mRpMti & 0x01u) != 0u; }
    /// Direction-independent message type: the uplink MTI value of this
    /// message kind (Data = 0, Ack = 2, Error = 4, SMMA = 6).
    [[nodiscard]] RPMTI messageType() const { return static_cast<RPMTI>(mRpMti & 0xFEu); }
    uint8_t messageRef() const { return mMessageRef; }
    const std::optional<L3TPAddress>& originatorAddress() const { return mOriginatorAddress; }
    const std::optional<L3TPAddress>& destinationAddress() const { return mDestinationAddress; }
    const std::vector<uint8_t>& userData() const { return mUserData; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3RPData> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;

    struct Builder {
        // Default MTI: uplink Data (TS 24.079 Table 8.3).
        uint8_t mRpMti{static_cast<uint8_t>(RPMTI::DataUplink)};
        uint8_t mMessageRef{0};
        std::optional<L3TPAddress> mOriginatorAddress;
        std::optional<L3TPAddress> mDestinationAddress;
        std::vector<uint8_t> mUserData;

        /// Set the RP-MTI value: an uplink or downlink code (TS 24.079
        /// Table 8.3); a downlink sender must use the downlink code.
        Builder& mti(RPMTI v) { mRpMti = static_cast<uint8_t>(v); return *this; }
        /// Set the message reference.
        Builder& messageRef(uint8_t v) { mMessageRef = v; return *this; }
        /// Set the originator address.
        Builder& originatorAddress(L3TPAddress v) { mOriginatorAddress = v; return *this; }
        /// Set the destination address.
        Builder& destinationAddress(L3TPAddress v) { mDestinationAddress = v; return *this; }
        /// Set the user data.
        Builder& userData(std::vector<uint8_t> v) { mUserData = std::move(v); return *this; }
        /// Build the final message.
        [[nodiscard]] L3RPData build() const {
            L3RPData msg;
            msg.mRpMti = mRpMti;
            msg.mMessageRef = mMessageRef;
            msg.mOriginatorAddress = mOriginatorAddress;
            msg.mDestinationAddress = mDestinationAddress;
            msg.mUserData = mUserData;
            return msg;
        }
    };

    static Builder builder() { return Builder{}; }
};

// ── RP-ACK (GSM 24.011 7.3.2) ─────────────────────────────────────────
// Header: reserved(5)=0 | RP-MTI(3) | RP-Message-Reference(1). The MTI is
// direction dependent (TS 24.079 section 7.2.1, Table 8.3): uplink Ack = 2,
// downlink Ack = 3.
// No body

class L3RPAck {
    // Raw RP-MTI value (0..7); even = uplink, odd = downlink (Table 8.3).
    uint8_t mRpMti{static_cast<uint8_t>(RPMTI::AckUplink)};
    uint8_t mMessageRef{0};

    friend struct Builder;
public:
    /// Set the RP-MTI value (TS 24.079 Table 8.3).
    void setRpMti(RPMTI v) { mRpMti = static_cast<uint8_t>(v); }
    void setMessageRef(uint8_t v) { mMessageRef = v; }
    /// Raw RP-MTI value from the header octet (TS 24.079 Table 8.3).
    [[nodiscard]] RPMTI mti() const { return static_cast<RPMTI>(mRpMti); }
    /// True for the downlink (network to MS) direction; bit 0 of the MTI.
    [[nodiscard]] bool isDownlink() const { return (mRpMti & 0x01u) != 0u; }
    /// Direction-independent message type: the uplink MTI value of this
    /// message kind (Data = 0, Ack = 2, Error = 4, SMMA = 6).
    [[nodiscard]] RPMTI messageType() const { return static_cast<RPMTI>(mRpMti & 0xFEu); }
    uint8_t messageRef() const { return mMessageRef; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3RPAck> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;

    struct Builder {
        // Default MTI: uplink Ack (TS 24.079 Table 8.3).
        uint8_t mRpMti{static_cast<uint8_t>(RPMTI::AckUplink)};
        uint8_t mMessageRef{0};

        /// Set the RP-MTI value: an uplink or downlink code (TS 24.079
        /// Table 8.3); a downlink sender must use the downlink code.
        Builder& mti(RPMTI v) { mRpMti = static_cast<uint8_t>(v); return *this; }
        /// Set the message reference.
        Builder& messageRef(uint8_t v) { mMessageRef = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3RPAck build() const {
            L3RPAck msg;
            msg.mRpMti = mRpMti;
            msg.mMessageRef = mMessageRef;
            return msg;
        }
    };

    static Builder builder() { return Builder{}; }
};

// ── RP-ERROR (GSM 24.011 7.3.4) ───────────────────────────────────────
// Header: reserved(5)=0 | RP-MTI(3) | RP-Message-Reference(1). The MTI is
// direction dependent (TS 24.079 section 7.2.1, Table 8.3): uplink Error = 4,
// downlink Error = 5.
// Body: RP-Cause(LV): Length(1) | CauseValue(7 bits + extension bit)

class L3RPError {
    // Raw RP-MTI value (0..7); even = uplink, odd = downlink (Table 8.3).
    uint8_t mRpMti{static_cast<uint8_t>(RPMTI::ErrorUplink)};
    uint8_t mMessageRef{0};
    CPCause mCause{CPCause::Unspecified};

    friend struct Builder;
public:
    /// Set the RP-MTI value (TS 24.079 Table 8.3).
    void setRpMti(RPMTI v) { mRpMti = static_cast<uint8_t>(v); }
    void setMessageRef(uint8_t v) { mMessageRef = v; }
    void setCause(CPCause v) { mCause = v; }
    /// Raw RP-MTI value from the header octet (TS 24.079 Table 8.3).
    [[nodiscard]] RPMTI mti() const { return static_cast<RPMTI>(mRpMti); }
    /// True for the downlink (network to MS) direction; bit 0 of the MTI.
    [[nodiscard]] bool isDownlink() const { return (mRpMti & 0x01u) != 0u; }
    /// Direction-independent message type: the uplink MTI value of this
    /// message kind (Data = 0, Ack = 2, Error = 4, SMMA = 6).
    [[nodiscard]] RPMTI messageType() const { return static_cast<RPMTI>(mRpMti & 0xFEu); }
    uint8_t messageRef() const { return mMessageRef; }
    CPCause cause() const { return mCause; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3RPError> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;

    struct Builder {
        // Default MTI: uplink Error (TS 24.079 Table 8.3).
        uint8_t mRpMti{static_cast<uint8_t>(RPMTI::ErrorUplink)};
        uint8_t mMessageRef{0};
        CPCause mCause{CPCause::Unspecified};

        /// Set the RP-MTI value: an uplink or downlink code (TS 24.079
        /// Table 8.3); a downlink sender must use the downlink code.
        Builder& mti(RPMTI v) { mRpMti = static_cast<uint8_t>(v); return *this; }
        /// Set the message reference.
        Builder& messageRef(uint8_t v) { mMessageRef = v; return *this; }
        /// Set the CP cause.
        Builder& cause(CPCause v) { mCause = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3RPError build() const {
            L3RPError msg;
            msg.mRpMti = mRpMti;
            msg.mMessageRef = mMessageRef;
            msg.mCause = mCause;
            return msg;
        }
    };

    static Builder builder() { return Builder{}; }
};

// ── RP-SMMA (GSM 24.011 7.3.3) ────────────────────────────────────────
// Header: reserved(5)=0 | RP-MTI(3) | RP-Message-Reference(1). The MTI is
// direction dependent (TS 24.079 section 7.2.1, Table 8.3): uplink SMMA = 6,
// downlink SMMA = 7.
// No body

class L3RPSMMA {
    // Raw RP-MTI value (0..7); even = uplink, odd = downlink (Table 8.3).
    uint8_t mRpMti{static_cast<uint8_t>(RPMTI::SMMAUplink)};
    uint8_t mMessageRef{0};

    friend struct Builder;
public:
    /// Set the RP-MTI value (TS 24.079 Table 8.3).
    void setRpMti(RPMTI v) { mRpMti = static_cast<uint8_t>(v); }
    void setMessageRef(uint8_t v) { mMessageRef = v; }
    /// Raw RP-MTI value from the header octet (TS 24.079 Table 8.3).
    [[nodiscard]] RPMTI mti() const { return static_cast<RPMTI>(mRpMti); }
    /// True for the downlink (network to MS) direction; bit 0 of the MTI.
    [[nodiscard]] bool isDownlink() const { return (mRpMti & 0x01u) != 0u; }
    /// Direction-independent message type: the uplink MTI value of this
    /// message kind (Data = 0, Ack = 2, Error = 4, SMMA = 6).
    [[nodiscard]] RPMTI messageType() const { return static_cast<RPMTI>(mRpMti & 0xFEu); }
    uint8_t messageRef() const { return mMessageRef; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3RPSMMA> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;

    struct Builder {
        // Default MTI: uplink SMMA (TS 24.079 Table 8.3).
        uint8_t mRpMti{static_cast<uint8_t>(RPMTI::SMMAUplink)};
        uint8_t mMessageRef{0};

        /// Set the RP-MTI value: an uplink or downlink code (TS 24.079
        /// Table 8.3); a downlink sender must use the downlink code.
        Builder& mti(RPMTI v) { mRpMti = static_cast<uint8_t>(v); return *this; }
        /// Set the message reference.
        Builder& messageRef(uint8_t v) { mMessageRef = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3RPSMMA build() const {
            L3RPSMMA msg;
            msg.mRpMti = mRpMti;
            msg.mMessageRef = mMessageRef;
            return msg;
        }
    };

    static Builder builder() { return Builder{}; }
};

// SMS message type names for text output.
const char* smsMessageName(int mti);

} // namespace gsml3parser
