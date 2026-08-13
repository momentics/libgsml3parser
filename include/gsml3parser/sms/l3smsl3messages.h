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

// SMS L3 Messages (TS 24.008 9.6) — TE-to-MS SMS primitives
// Spec: 3GPP TS 24.008 sections 9.6.1-9.6.14, Table 10.6a
// Reference: ref/osmo-ttcn3-hacks/library/L3_Templates.ttcn — SMS-TS-* templates
//
// These are L3-level SMS primitives used for SMS-on-CS fallback and status
// reporting. They share PD=0x09 with CP-layer messages but operate in a
// different context. MTI 0x12 and 0x13 overlap with CP-STATUS and CP-SMT;
// the parser resolves overlaps by preferring CP messages for backward compat.
//
// L3 header (per 24.008 10.5.1):
//   Byte 0: PD(4)=0x09(SMS) | Skip(4)
//   Byte 1: MTI(8 bits, raw — no NSD field)

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

// ── TP Status (GSM 23.040 9.2.2.3) ────────────────────────────────────
// 4-bit delivery status indicator

enum class TPStatus : uint8_t {
    Delivered = 0,
    DeliveryAttempted = 1,
    ErasedAtMS = 2,
    DeliveryNotPossible = 3,
    Decrypted = 4,
};

const char* TPStatus2Str(TPStatus st);

// ── RP Disposal Type (GSM 24.011 7.3.5) ───────────────────────────────
// 4-bit disposal instruction for status reports

enum class RPDisposalType : uint8_t {
    NoFurtherAction = 0,
    DisplayToUser = 1,
    StoreInSIM = 2,
    DeleteFromMS = 3,
};

const char* RPDisposalType2Str(RPDisposalType disp);

// ── SMS Cause (GSM 24.008 10.5.2) ─────────────────────────────────────
// 7-bit cause value for SMS L3 reject/deferred messages

enum class SMSCause : uint8_t {
    NoCause = 0,
    SMSSystemFailure = 12,
    OperatorsDeterminationBarred = 13,
    PagingRevocationDataError = 15,
    UESMSFunctionalityNotSupported = 25,
    InvalidSourceAddressSubsystem = 28,
    CUGRejectDueToInvalidTGroupID = 39,
    AdditionalCUGRestrictionsApply = 40,
};

const char* SMSCause2Str(SMSCause cause);

// ── SMS Status Report (GSM 24.008 9.6.1) ──────────────────────────────
// Bidirectional: TP-MR(1) | RP-Disp(1) | [TP-DA(LV)] | [TP-OA(LV)] | [SCTS(7)] | [MT-StartTime(7)] | TP-ST(1)

class L3SMSStatusReport {
    uint8_t mTpMr{0};
    RPDisposalType mRpDisp{RPDisposalType::NoFurtherAction};
    bool mHaveTpDa{false};
    L3TPAddress mTpDa;
    bool mHaveTpOa{false};
    L3TPAddress mTpOa;
    std::optional<TPSCTimeStamp> mScts;
    std::optional<TPSCTimeStamp> mMtStartTime;
    TPStatus mTpSt{TPStatus::Delivered};

    friend struct Builder;
public:
    static constexpr int MTI = 0x11;

    uint8_t tpMr() const { return mTpMr; }
    RPDisposalType rpDisp() const { return mRpDisp; }
    bool hasTpDa() const { return mHaveTpDa; }
    const L3TPAddress& tpDa() const { return mTpDa; }
    bool hasTpOa() const { return mHaveTpOa; }
    const L3TPAddress& tpOa() const { return mTpOa; }
    const std::optional<TPSCTimeStamp>& scts() const { return mScts; }
    const std::optional<TPSCTimeStamp>& mtStartTime() const { return mMtStartTime; }
    TPStatus tpSt() const { return mTpSt; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3SMSStatusReport> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::SMS; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }

    struct Builder {
        uint8_t mTpMr{0};
        RPDisposalType mRpDisp{RPDisposalType::NoFurtherAction};
        bool mHaveTpDa{false};
        L3TPAddress mTpDa;
        bool mHaveTpOa{false};
        L3TPAddress mTpOa;
        std::optional<TPSCTimeStamp> mScts;
        std::optional<TPSCTimeStamp> mMtStartTime;
        TPStatus mTpSt{TPStatus::Delivered};

        /// Set the TP-MR field.
        Builder& tpMr(uint8_t v) { mTpMr = v; return *this; }
        /// Set the RP-Disp field.
        Builder& rpDisp(RPDisposalType v) { mRpDisp = v; return *this; }
        /// Set whether TP-DA is present.
        Builder& haveTpDa(bool v) { mHaveTpDa = v; return *this; }
        /// Set the TP-DA address.
        Builder& tpDa(L3TPAddress v) { mTpDa = v; return *this; }
        /// Set whether TP-OA is present.
        Builder& haveTpOa(bool v) { mHaveTpOa = v; return *this; }
        /// Set the TP-OA address.
        Builder& tpOa(L3TPAddress v) { mTpOa = v; return *this; }
        /// Set the SCTS timestamp.
        Builder& scts(TPSCTimeStamp v) { mScts = v; return *this; }
        /// Set the MT start time.
        Builder& mtStartTime(TPSCTimeStamp v) { mMtStartTime = v; return *this; }
        /// Set the TP-ST field.
        Builder& tpSt(TPStatus v) { mTpSt = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3SMSStatusReport build() const {
            L3SMSStatusReport msg;
            msg.mTpMr = mTpMr;
            msg.mRpDisp = mRpDisp;
            msg.mHaveTpDa = mHaveTpDa;
            msg.mTpDa = mTpDa;
            msg.mHaveTpOa = mHaveTpOa;
            msg.mTpOa = mTpOa;
            msg.mScts = mScts;
            msg.mMtStartTime = mMtStartTime;
            msg.mTpSt = mTpSt;
            return msg;
        }
    };

    static Builder builder() { return Builder{}; }
};

// ── SMS Provided Reply Expected (GSM 24.008 9.6.2) ────────────────────
// Net->MS: [TP-PID(1)] | TP-DCS(1) | [TP-Ud(LV)]

class L3SMSProvidedReplyExpected {
    bool mHaveTpPid{false};
    TPPID mTpPid{TPPID::Default};
    TPDCS mTpDcs{TPDCS::Default_Alphabet};
    bool mHaveTpUd{false};
    std::vector<uint8_t> mTpUd;

    friend struct Builder;
public:
    static constexpr int MTI = 0x12;

    bool hasTpPid() const { return mHaveTpPid; }
    TPPID tpPid() const { return mTpPid; }
    TPDCS tpDcs() const { return mTpDcs; }
    bool hasTpUd() const { return mHaveTpUd; }
    const std::vector<uint8_t>& tpUd() const { return mTpUd; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3SMSProvidedReplyExpected> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::SMS; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }

    struct Builder {
        bool mHaveTpPid{false};
        TPPID mTpPid{TPPID::Default};
        TPDCS mTpDcs{TPDCS::Default_Alphabet};
        bool mHaveTpUd{false};
        std::vector<uint8_t> mTpUd;

        /// Set whether TP-PID is present.
        Builder& haveTpPid(bool v) { mHaveTpPid = v; return *this; }
        /// Set the TP-PID value.
        Builder& tpPid(TPPID v) { mTpPid = v; return *this; }
        /// Set the TP-DCS value.
        Builder& tpDcs(TPDCS v) { mTpDcs = v; return *this; }
        /// Set whether TP-Ud is present.
        Builder& haveTpUd(bool v) { mHaveTpUd = v; return *this; }
        /// Set the TP-Ud data.
        Builder& tpUd(std::vector<uint8_t> v) { mTpUd = std::move(v); return *this; }
        /// Build the final message.
        [[nodiscard]] L3SMSProvidedReplyExpected build() const {
            L3SMSProvidedReplyExpected msg;
            msg.mHaveTpPid = mHaveTpPid;
            msg.mTpPid = mTpPid;
            msg.mTpDcs = mTpDcs;
            msg.mHaveTpUd = mHaveTpUd;
            msg.mTpUd = mTpUd;
            return msg;
        }
    };

    static Builder builder() { return Builder{}; }
};

// ── SMS Submit Reply (GSM 24.008 9.6.3) ───────────────────────────────
// Net->MS: [TP-PID(1)] | TP-DCS(1) | [TP-Ud(LV)]

class L3SMSSubmitRep {
    bool mHaveTpPid{false};
    TPPID mTpPid{TPPID::Default};
    TPDCS mTpDcs{TPDCS::Default_Alphabet};
    bool mHaveTpUd{false};
    std::vector<uint8_t> mTpUd;

    friend struct Builder;
public:
    static constexpr int MTI = 0x13;

    bool hasTpPid() const { return mHaveTpPid; }
    TPPID tpPid() const { return mTpPid; }
    TPDCS tpDcs() const { return mTpDcs; }
    bool hasTpUd() const { return mHaveTpUd; }
    const std::vector<uint8_t>& tpUd() const { return mTpUd; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3SMSSubmitRep> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::SMS; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }

    struct Builder {
        bool mHaveTpPid{false};
        TPPID mTpPid{TPPID::Default};
        TPDCS mTpDcs{TPDCS::Default_Alphabet};
        bool mHaveTpUd{false};
        std::vector<uint8_t> mTpUd;

        /// Set whether TP-PID is present.
        Builder& haveTpPid(bool v) { mHaveTpPid = v; return *this; }
        /// Set the TP-PID value.
        Builder& tpPid(TPPID v) { mTpPid = v; return *this; }
        /// Set the TP-DCS value.
        Builder& tpDcs(TPDCS v) { mTpDcs = v; return *this; }
        /// Set whether TP-Ud is present.
        Builder& haveTpUd(bool v) { mHaveTpUd = v; return *this; }
        /// Set the TP-Ud data.
        Builder& tpUd(std::vector<uint8_t> v) { mTpUd = std::move(v); return *this; }
        /// Build the final message.
        [[nodiscard]] L3SMSSubmitRep build() const {
            L3SMSSubmitRep msg;
            msg.mHaveTpPid = mHaveTpPid;
            msg.mTpPid = mTpPid;
            msg.mTpDcs = mTpDcs;
            msg.mHaveTpUd = mHaveTpUd;
            msg.mTpUd = mTpUd;
            return msg;
        }
    };

    static Builder builder() { return Builder{}; }
};

// ── SMS Deliver (GSM 24.008 9.6.4) ────────────────────────────────────
// Net->MS: TP-MTI(4)|TP-MR(1)|[TP-OA(LV)]|TP-PID(1)|TP-DCS(1)|SCTS(7)|[TP-Ud(LV)]

class L3SMSDeliver {
    uint8_t mTpMti{0};
    uint8_t mTpMr{0};
    bool mHaveTpOa{false};
    L3TPAddress mTpOa;
    TPPID mTpPid{TPPID::Default};
    TPDCS mTpDcs{TPDCS::Default_Alphabet};
    TPSCTimeStamp mScts;
    bool mHaveTpUd{false};
    std::vector<uint8_t> mTpUd;

    friend struct Builder;
public:
    static constexpr int MTI = 0x14;

    uint8_t tpMti() const { return mTpMti; }
    uint8_t tpMr() const { return mTpMr; }
    bool hasTpOa() const { return mHaveTpOa; }
    const L3TPAddress& tpOa() const { return mTpOa; }
    TPPID tpPid() const { return mTpPid; }
    TPDCS tpDcs() const { return mTpDcs; }
    const TPSCTimeStamp& scts() const { return mScts; }
    bool hasTpUd() const { return mHaveTpUd; }
    const std::vector<uint8_t>& tpUd() const { return mTpUd; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3SMSDeliver> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::SMS; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }

    struct Builder {
        uint8_t mTpMti{0};
        uint8_t mTpMr{0};
        bool mHaveTpOa{false};
        L3TPAddress mTpOa;
        TPPID mTpPid{TPPID::Default};
        TPDCS mTpDcs{TPDCS::Default_Alphabet};
        TPSCTimeStamp mScts;
        bool mHaveTpUd{false};
        std::vector<uint8_t> mTpUd;

        /// Set the TP-MTI field.
        Builder& tpMti(uint8_t v) { mTpMti = v; return *this; }
        /// Set the TP-MR field.
        Builder& tpMr(uint8_t v) { mTpMr = v; return *this; }
        /// Set whether TP-OA is present.
        Builder& haveTpOa(bool v) { mHaveTpOa = v; return *this; }
        /// Set the TP-OA address.
        Builder& tpOa(L3TPAddress v) { mTpOa = v; return *this; }
        /// Set the TP-PID value.
        Builder& tpPid(TPPID v) { mTpPid = v; return *this; }
        /// Set the TP-DCS value.
        Builder& tpDcs(TPDCS v) { mTpDcs = v; return *this; }
        /// Set the SCTS timestamp.
        Builder& scts(TPSCTimeStamp v) { mScts = v; return *this; }
        /// Set whether TP-Ud is present.
        Builder& haveTpUd(bool v) { mHaveTpUd = v; return *this; }
        /// Set the TP-Ud data.
        Builder& tpUd(std::vector<uint8_t> v) { mTpUd = std::move(v); return *this; }
        /// Build the final message.
        [[nodiscard]] L3SMSDeliver build() const {
            L3SMSDeliver msg;
            msg.mTpMti = mTpMti;
            msg.mTpMr = mTpMr;
            msg.mHaveTpOa = mHaveTpOa;
            msg.mTpOa = mTpOa;
            msg.mTpPid = mTpPid;
            msg.mTpDcs = mTpDcs;
            msg.mScts = mScts;
            msg.mHaveTpUd = mHaveTpUd;
            msg.mTpUd = mTpUd;
            return msg;
        }
    };

    static Builder builder() { return Builder{}; }
};

// ── SMS Deliver Reply (GSM 24.008 9.6.5) ──────────────────────────────
// MS->Net: TP-MTI(4)|TP-MR(1)|[TP-DA(LV)]|TP-PID(1)|TP-DCS(1)|[TP-Ud(LV)]

class L3SMSDeliverRep {
    uint8_t mTpMti{0};
    uint8_t mTpMr{0};
    bool mHaveTpDa{false};
    L3TPAddress mTpDa;
    TPPID mTpPid{TPPID::Default};
    TPDCS mTpDcs{TPDCS::Default_Alphabet};
    bool mHaveTpUd{false};
    std::vector<uint8_t> mTpUd;

    friend struct Builder;
public:
    static constexpr int MTI = 0x15;

    uint8_t tpMti() const { return mTpMti; }
    uint8_t tpMr() const { return mTpMr; }
    bool hasTpDa() const { return mHaveTpDa; }
    const L3TPAddress& tpDa() const { return mTpDa; }
    TPPID tpPid() const { return mTpPid; }
    TPDCS tpDcs() const { return mTpDcs; }
    bool hasTpUd() const { return mHaveTpUd; }
    const std::vector<uint8_t>& tpUd() const { return mTpUd; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3SMSDeliverRep> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::SMS; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }

    struct Builder {
        uint8_t mTpMti{0};
        uint8_t mTpMr{0};
        bool mHaveTpDa{false};
        L3TPAddress mTpDa;
        TPPID mTpPid{TPPID::Default};
        TPDCS mTpDcs{TPDCS::Default_Alphabet};
        bool mHaveTpUd{false};
        std::vector<uint8_t> mTpUd;

        /// Set the TP-MTI field.
        Builder& tpMti(uint8_t v) { mTpMti = v; return *this; }
        /// Set the TP-MR field.
        Builder& tpMr(uint8_t v) { mTpMr = v; return *this; }
        /// Set whether TP-DA is present.
        Builder& haveTpDa(bool v) { mHaveTpDa = v; return *this; }
        /// Set the TP-DA address.
        Builder& tpDa(L3TPAddress v) { mTpDa = v; return *this; }
        /// Set the TP-PID value.
        Builder& tpPid(TPPID v) { mTpPid = v; return *this; }
        /// Set the TP-DCS value.
        Builder& tpDcs(TPDCS v) { mTpDcs = v; return *this; }
        /// Set whether TP-Ud is present.
        Builder& haveTpUd(bool v) { mHaveTpUd = v; return *this; }
        /// Set the TP-Ud data.
        Builder& tpUd(std::vector<uint8_t> v) { mTpUd = std::move(v); return *this; }
        /// Build the final message.
        [[nodiscard]] L3SMSDeliverRep build() const {
            L3SMSDeliverRep msg;
            msg.mTpMti = mTpMti;
            msg.mTpMr = mTpMr;
            msg.mHaveTpDa = mHaveTpDa;
            msg.mTpDa = mTpDa;
            msg.mTpPid = mTpPid;
            msg.mTpDcs = mTpDcs;
            msg.mHaveTpUd = mHaveTpUd;
            msg.mTpUd = mTpUd;
            return msg;
        }
    };

    static Builder builder() { return Builder{}; }
};

// ── SMS Status Report Ack (GSM 24.008 9.6.6) ──────────────────────────
// MS->Net: TP-MR(1)

class L3SMSStatusReportAck {
    uint8_t mTpMr{0};

    friend struct Builder;
public:
    static constexpr int MTI = 0x16;

    uint8_t tpMr() const { return mTpMr; }

    size_t bodyLength() const { return 1; }
    [[nodiscard]] static Expected<L3SMSStatusReportAck> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::SMS; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }

    struct Builder {
        uint8_t mTpMr{0};

        /// Set the TP-MR field.
        Builder& tpMr(uint8_t v) { mTpMr = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3SMSStatusReportAck build() const {
            L3SMSStatusReportAck msg;
            msg.mTpMr = mTpMr;
            return msg;
        }
    };

    static Builder builder() { return Builder{}; }
};

// ── SMS Status Report Reject (GSM 24.008 9.6.7) ───────────────────────
// Net->MS: TP-MR(1) | SM-Cause(1)

class L3SMSStatusReportReject {
    uint8_t mTpMr{0};
    SMSCause mSmCause{SMSCause::NoCause};

    friend struct Builder;
public:
    static constexpr int MTI = 0x17;

    uint8_t tpMr() const { return mTpMr; }
    SMSCause smCause() const { return mSmCause; }

    size_t bodyLength() const { return 2; }
    [[nodiscard]] static Expected<L3SMSStatusReportReject> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::SMS; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }

    struct Builder {
        uint8_t mTpMr{0};
        SMSCause mSmCause{SMSCause::NoCause};

        /// Set the TP-MR field.
        Builder& tpMr(uint8_t v) { mTpMr = v; return *this; }
        /// Set the SM-Cause value.
        Builder& smCause(SMSCause v) { mSmCause = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3SMSStatusReportReject build() const {
            L3SMSStatusReportReject msg;
            msg.mTpMr = mTpMr;
            msg.mSmCause = mSmCause;
            return msg;
        }
    };

    static Builder builder() { return Builder{}; }
};

// ── SMS TS Reject (GSM 24.008 9.6.8) ──────────────────────────────────
// Net->MS: SM-Cause(1)

class L3SMSTSReject {
    SMSCause mSmCause{SMSCause::NoCause};

    friend struct Builder;
public:
    static constexpr int MTI = 0x18;

    SMSCause smCause() const { return mSmCause; }

    size_t bodyLength() const { return 1; }
    [[nodiscard]] static Expected<L3SMSTSReject> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::SMS; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }

    struct Builder {
        SMSCause mSmCause{SMSCause::NoCause};

        /// Set the SM-Cause value.
        Builder& smCause(SMSCause v) { mSmCause = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3SMSTSReject build() const {
            L3SMSTSReject msg;
            msg.mSmCause = mSmCause;
            return msg;
        }
    };

    static Builder builder() { return Builder{}; }
};

// ── SMS Submit Deferred (GSM 24.008 9.6.9) ────────────────────────────
// Net->MS: [TP-PID(1)] | TP-DCS(1) | [TP-Ud(LV)]

class L3SMSSubmitDeferred {
    bool mHaveTpPid{false};
    TPPID mTpPid{TPPID::Default};
    TPDCS mTpDcs{TPDCS::Default_Alphabet};
    bool mHaveTpUd{false};
    std::vector<uint8_t> mTpUd;

    friend struct Builder;
public:
    static constexpr int MTI = 0x19;

    bool hasTpPid() const { return mHaveTpPid; }
    TPPID tpPid() const { return mTpPid; }
    TPDCS tpDcs() const { return mTpDcs; }
    bool hasTpUd() const { return mHaveTpUd; }
    const std::vector<uint8_t>& tpUd() const { return mTpUd; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3SMSSubmitDeferred> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::SMS; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }

    struct Builder {
        bool mHaveTpPid{false};
        TPPID mTpPid{TPPID::Default};
        TPDCS mTpDcs{TPDCS::Default_Alphabet};
        bool mHaveTpUd{false};
        std::vector<uint8_t> mTpUd;

        /// Set whether TP-PID is present.
        Builder& haveTpPid(bool v) { mHaveTpPid = v; return *this; }
        /// Set the TP-PID value.
        Builder& tpPid(TPPID v) { mTpPid = v; return *this; }
        /// Set the TP-DCS value.
        Builder& tpDcs(TPDCS v) { mTpDcs = v; return *this; }
        /// Set whether TP-Ud is present.
        Builder& haveTpUd(bool v) { mHaveTpUd = v; return *this; }
        /// Set the TP-Ud data.
        Builder& tpUd(std::vector<uint8_t> v) { mTpUd = std::move(v); return *this; }
        /// Build the final message.
        [[nodiscard]] L3SMSSubmitDeferred build() const {
            L3SMSSubmitDeferred msg;
            msg.mHaveTpPid = mHaveTpPid;
            msg.mTpPid = mTpPid;
            msg.mTpDcs = mTpDcs;
            msg.mHaveTpUd = mHaveTpUd;
            msg.mTpUd = mTpUd;
            return msg;
        }
    };

    static Builder builder() { return Builder{}; }
};

// ── SMS Submit Reject (GSM 24.008 9.6.10) ─────────────────────────────
// Net->MS: SM-Cause(1)

class L3SMSSubmitReject {
    SMSCause mSmCause{SMSCause::NoCause};

    friend struct Builder;
public:
    static constexpr int MTI = 0x1A;

    SMSCause smCause() const { return mSmCause; }

    size_t bodyLength() const { return 1; }
    [[nodiscard]] static Expected<L3SMSSubmitReject> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::SMS; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }

    struct Builder {
        SMSCause mSmCause{SMSCause::NoCause};

        /// Set the SM-Cause value.
        Builder& smCause(SMSCause v) { mSmCause = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3SMSSubmitReject build() const {
            L3SMSSubmitReject msg;
            msg.mSmCause = mSmCause;
            return msg;
        }
    };

    static Builder builder() { return Builder{}; }
};

// ── SMS SSF Provided Reply (GSM 24.008 9.6.11) ────────────────────────
// MS->Net: [TP-PID(1)] | TP-DCS(1) | [TP-Ud(LV)]

class L3SMSSFProvidedRep {
    bool mHaveTpPid{false};
    TPPID mTpPid{TPPID::Default};
    TPDCS mTpDcs{TPDCS::Default_Alphabet};
    bool mHaveTpUd{false};
    std::vector<uint8_t> mTpUd;

    friend struct Builder;
public:
    static constexpr int MTI = 0x1B;

    bool hasTpPid() const { return mHaveTpPid; }
    TPPID tpPid() const { return mTpPid; }
    TPDCS tpDcs() const { return mTpDcs; }
    bool hasTpUd() const { return mHaveTpUd; }
    const std::vector<uint8_t>& tpUd() const { return mTpUd; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3SMSSFProvidedRep> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::SMS; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }

    struct Builder {
        bool mHaveTpPid{false};
        TPPID mTpPid{TPPID::Default};
        TPDCS mTpDcs{TPDCS::Default_Alphabet};
        bool mHaveTpUd{false};
        std::vector<uint8_t> mTpUd;

        /// Set whether TP-PID is present.
        Builder& haveTpPid(bool v) { mHaveTpPid = v; return *this; }
        /// Set the TP-PID value.
        Builder& tpPid(TPPID v) { mTpPid = v; return *this; }
        /// Set the TP-DCS value.
        Builder& tpDcs(TPDCS v) { mTpDcs = v; return *this; }
        /// Set whether TP-Ud is present.
        Builder& haveTpUd(bool v) { mHaveTpUd = v; return *this; }
        /// Set the TP-Ud data.
        Builder& tpUd(std::vector<uint8_t> v) { mTpUd = std::move(v); return *this; }
        /// Build the final message.
        [[nodiscard]] L3SMSSFProvidedRep build() const {
            L3SMSSFProvidedRep msg;
            msg.mHaveTpPid = mHaveTpPid;
            msg.mTpPid = mTpPid;
            msg.mTpDcs = mTpDcs;
            msg.mHaveTpUd = mHaveTpUd;
            msg.mTpUd = mTpUd;
            return msg;
        }
    };

    static Builder builder() { return Builder{}; }
};

// ── SMS SSF Provided Reply Ack (GSM 24.008 9.6.12) ────────────────────
// Net->MS: empty body

class L3SMSSFProvidedRepAck {
    friend struct Builder;
public:
    static constexpr int MTI = 0x1C;

    size_t bodyLength() const { return 0; }
    [[nodiscard]] static Expected<L3SMSSFProvidedRepAck> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::SMS; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }

    struct Builder {
        /// Build the final message.
        [[nodiscard]] L3SMSSFProvidedRepAck build() const {
            return L3SMSSFProvidedRepAck{};
        }
    };

    static Builder builder() { return Builder{}; }
};

// ── SMS Notification (GSM 24.008 9.6.13) ──────────────────────────────
// Bidirectional: [TP-PID(1)] | TP-DCS(1) | [TP-Ud(LV)]

class L3SMSNotification {
    bool mHaveTpPid{false};
    TPPID mTpPid{TPPID::Default};
    TPDCS mTpDcs{TPDCS::Default_Alphabet};
    bool mHaveTpUd{false};
    std::vector<uint8_t> mTpUd;

    friend struct Builder;
public:
    static constexpr int MTI = 0x1D;

    bool hasTpPid() const { return mHaveTpPid; }
    TPPID tpPid() const { return mTpPid; }
    TPDCS tpDcs() const { return mTpDcs; }
    bool hasTpUd() const { return mHaveTpUd; }
    const std::vector<uint8_t>& tpUd() const { return mTpUd; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3SMSNotification> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::SMS; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }

    struct Builder {
        bool mHaveTpPid{false};
        TPPID mTpPid{TPPID::Default};
        TPDCS mTpDcs{TPDCS::Default_Alphabet};
        bool mHaveTpUd{false};
        std::vector<uint8_t> mTpUd;

        /// Set whether TP-PID is present.
        Builder& haveTpPid(bool v) { mHaveTpPid = v; return *this; }
        /// Set the TP-PID value.
        Builder& tpPid(TPPID v) { mTpPid = v; return *this; }
        /// Set the TP-DCS value.
        Builder& tpDcs(TPDCS v) { mTpDcs = v; return *this; }
        /// Set whether TP-Ud is present.
        Builder& haveTpUd(bool v) { mHaveTpUd = v; return *this; }
        /// Set the TP-Ud data.
        Builder& tpUd(std::vector<uint8_t> v) { mTpUd = std::move(v); return *this; }
        /// Build the final message.
        [[nodiscard]] L3SMSNotification build() const {
            L3SMSNotification msg;
            msg.mHaveTpPid = mHaveTpPid;
            msg.mTpPid = mTpPid;
            msg.mTpDcs = mTpDcs;
            msg.mHaveTpUd = mHaveTpUd;
            msg.mTpUd = mTpUd;
            return msg;
        }
    };

    static Builder builder() { return Builder{}; }
};

// ── SMS Short Code Info (GSM 24.008 9.6.14) ───────────────────────────
// Bidirectional: ShortCodeType(1) | [ShortCode(LV)]

class L3SMSShortCodeInfo {
    uint8_t mShortCodeType{0};
    bool mHaveShortCode{false};
    std::vector<uint8_t> mShortCode;

    friend struct Builder;
public:
    static constexpr int MTI = 0x1E;

    uint8_t shortCodeType() const { return mShortCodeType; }
    bool hasShortCode() const { return mHaveShortCode; }
    const std::vector<uint8_t>& shortCode() const { return mShortCode; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3SMSShortCodeInfo> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::SMS; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }

    struct Builder {
        uint8_t mShortCodeType{0};
        bool mHaveShortCode{false};
        std::vector<uint8_t> mShortCode;

        /// Set the short code type.
        Builder& shortCodeType(uint8_t v) { mShortCodeType = v; return *this; }
        /// Set whether short code is present.
        Builder& haveShortCode(bool v) { mHaveShortCode = v; return *this; }
        /// Set the short code data.
        Builder& shortCode(std::vector<uint8_t> v) { mShortCode = std::move(v); return *this; }
        /// Build the final message.
        [[nodiscard]] L3SMSShortCodeInfo build() const {
            L3SMSShortCodeInfo msg;
            msg.mShortCodeType = mShortCodeType;
            msg.mHaveShortCode = mHaveShortCode;
            msg.mShortCode = mShortCode;
            return msg;
        }
    };

    static Builder builder() { return Builder{}; }
};

} // namespace gsml3parser
