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

// SMS L3 Messages (TS 24.008 9.6) - parse/write implementation
// Spec: 3GPP TS 24.008 sections 9.6.1-9.6.14, Table 10.6a
// Reference: ref/osmo-ttcn3-hacks/library/L3_Templates.ttcn - SMS-TS-* templates

#include "gsml3parser/sms/l3smsl3messages.h"
#include <sstream>
#include <iomanip>

namespace gsml3parser {

// ── Enum string converters ─────────────────────────────────────────────

const char* TPStatus2Str(TPStatus st) {
    switch (st) {
        case TPStatus::Delivered: return "Delivered";
        case TPStatus::DeliveryAttempted: return "DeliveryAttempted";
        case TPStatus::ErasedAtMS: return "ErasedAtMS";
        case TPStatus::DeliveryNotPossible: return "DeliveryNotPossible";
        case TPStatus::Decrypted: return "Decrypted";
    }
    return "Unknown";
}

const char* RPDisposalType2Str(RPDisposalType disp) {
    switch (disp) {
        case RPDisposalType::NoFurtherAction: return "NoFurtherAction";
        case RPDisposalType::DisplayToUser: return "DisplayToUser";
        case RPDisposalType::StoreInSIM: return "StoreInSIM";
        case RPDisposalType::DeleteFromMS: return "DeleteFromMS";
    }
    return "Unknown";
}

const char* SMSCause2Str(SMSCause cause) {
    switch (cause) {
        case SMSCause::NoCause: return "NoCause";
        case SMSCause::SMSSystemFailure: return "SMSSystemFailure";
        case SMSCause::OperatorsDeterminationBarred: return "OperatorsDeterminationBarred";
        case SMSCause::PagingRevocationDataError: return "PagingRevocationDataError";
        case SMSCause::UESMSFunctionalityNotSupported: return "UESMSFunctionalityNotSupported";
        case SMSCause::InvalidSourceAddressSubsystem: return "InvalidSourceAddressSubsystem";
        case SMSCause::CUGRejectDueToInvalidTGroupID: return "CUGRejectDueToInvalidTGroupID";
        case SMSCause::AdditionalCUGRestrictionsApply: return "AdditionalCUGRestrictionsApply";
    }
    return "Unknown";
}

// ── Helper: read remaining body bytes into a buffer ────────────────────

static std::vector<uint8_t> readRemainingBytes(BitReader& br) {
    std::vector<uint8_t> bytes;
    while (br.hasMore()) {
        auto b = br.readField(8);
        if (!b) break;
        bytes.push_back(static_cast<uint8_t>(b.value()));
    }
    return bytes;
}

// ── Helper: write optional TP-PID, TP-DCS, TP-Ud pattern ──────────────

static void writePidDcsUd(BitWriter& bw, bool havePid, TPPID pid, TPDCS dcs, bool haveUd, const std::vector<uint8_t>& ud) {
    if (havePid) {
        bw.writeField(static_cast<uint32_t>(pid), 8);
    }
    bw.writeField(static_cast<uint32_t>(dcs), 8);
    if (haveUd) {
        bw.writeField(static_cast<uint32_t>(ud.size()), 8);
        for (uint8_t b : ud) {
            bw.writeField(b, 8);
        }
    }
}

// ── L3SMSStatusReport (24.008 9.6.1) ──────────────────────────────────
// Body: TP-MR(1) | RP-Disp(1) | [TP-DA(LV)] | [TP-OA(LV)] | [SCTS(7)] | [MT-StartTime(7)] | TP-ST(1)

size_t L3SMSStatusReport::bodyLength() const {
    size_t len = 2; // TP-MR + RP-Disp
    if (mHaveTpDa) len += mTpDa.totalLength();
    if (mHaveTpOa) len += mTpOa.totalLength();
    if (mScts) len += 7;
    if (mMtStartTime) len += 7;
    len += 1; // TP-ST
    return len;
}

Expected<L3SMSStatusReport> L3SMSStatusReport::parse(BitReader& br) {
    L3SMSStatusReport msg;
    // Read TP-MR(1 octet)
    auto tpMr = br.readField(8);
    if (!tpMr) return Expected<L3SMSStatusReport>::error(tpMr.error());
    msg.mTpMr = static_cast<uint8_t>(tpMr.value());

    // Read RP-Disp(1 octet, lower 4 bits)
    auto rpDisp = br.readField(8);
    if (!rpDisp) return Expected<L3SMSStatusReport>::error(rpDisp.error());
    msg.mRpDisp = static_cast<RPDisposalType>(rpDisp.value() & 0x0F);

    // Collect remaining bytes for variable-length parsing
    std::vector<uint8_t> remaining = readRemainingBytes(br);
    size_t pos = 0;

    // Parse optional LV elements: TP-DA, TP-OA, SCTS(7), MT-StartTime(7), TP-ST(1)
    // The last byte is always TP-ST. Before that, we have optional fields.
    if (remaining.empty()) {
        return Expected<L3SMSStatusReport>::error(
            ParseError{ParseError::Code::TruncatedInput, "SMS Status Report: missing TP-ST"});
    }

    // Last byte is TP-ST
    msg.mTpSt = static_cast<TPStatus>(remaining.back() & 0x0F);
    size_t endPos = remaining.size() - 1;

    // Parse from the end backwards to identify fixed-size fields
    // SCTS is 7 bytes, MT-StartTime is 7 bytes, addresses are LV
    pos = 0;
    while (pos < endPos) {
        uint8_t lenByte = remaining[pos];
        if (lenByte == 0) {
            pos++;
            continue;
        }
        // Check if this looks like an LV address (length byte followed by TON/NPI)
        if (pos + 1 + lenByte <= endPos) {
            // Try parsing as TP Address
            std::vector<uint8_t> addrBytes(remaining.begin() + pos, remaining.begin() + pos + 1 + lenByte);
            BitReader addrBr(addrBytes.data(), addrBytes.size() * 8);
            auto addrResult = L3TPAddress::parse(addrBr);
            if (addrResult) {
                if (!msg.mHaveTpDa) {
                    msg.mHaveTpDa = true;
                    msg.mTpDa = std::move(addrResult.value());
                } else if (!msg.mHaveTpOa) {
                    msg.mHaveTpOa = true;
                    // Re-parse since we moved above
                    BitReader addrBr2(addrBytes.data(), addrBytes.size() * 8);
                    auto addrResult2 = L3TPAddress::parse(addrBr2);
                    if (addrResult2) msg.mTpOa = std::move(addrResult2.value());
                }
                pos += 1 + lenByte;
                continue;
            }
        }
        // Check for 7-byte SCTS blocks
        if (pos + 7 <= endPos) {
            std::vector<uint8_t> sctsBytes(remaining.begin() + pos, remaining.begin() + pos + 7);
            BitReader sctsBr(sctsBytes.data(), 56);
            auto sctsResult = TPSCTimeStamp::parse(sctsBr);
            if (sctsResult) {
                if (!msg.mScts) {
                    msg.mScts = std::move(sctsResult.value());
                } else if (!msg.mMtStartTime) {
                    // Re-parse since we moved above
                    BitReader sctsBr2(sctsBytes.data(), 56);
                    auto sctsResult2 = TPSCTimeStamp::parse(sctsBr2);
                    if (sctsResult2) msg.mMtStartTime = std::move(sctsResult2.value());
                }
                pos += 7;
                continue;
            }
        }
        break;
    }

    return Expected<L3SMSStatusReport>::hold(std::move(msg));
}

void L3SMSStatusReport::write(BitWriter& bw) const {
    // Write TP-MR(1 octet)
    bw.writeField(mTpMr, 8);
    // Write RP-Disp(1 octet)
    bw.writeField(static_cast<uint32_t>(mRpDisp) & 0x0F, 8);
    // Write optional TP-DA(LV)
    if (mHaveTpDa) mTpDa.write(bw);
    // Write optional TP-OA(LV)
    if (mHaveTpOa) mTpOa.write(bw);
    // Write optional SCTS(7 octets)
    if (mScts) mScts->write(bw);
    // Write optional MT-StartTime(7 octets)
    if (mMtStartTime) mMtStartTime->write(bw);
    // Write TP-ST(1 octet)
    bw.writeField(static_cast<uint32_t>(mTpSt) & 0x0F, 8);
}

void L3SMSStatusReport::text(std::ostream& os) const {
    os << "SMS-StatusReport(tpMr=" << static_cast<int>(mTpMr)
       << ",rpDisp=" << RPDisposalType2Str(mRpDisp)
       << ",tpSt=" << TPStatus2Str(mTpSt) << ")";
}

// ── L3SMSProvidedReplyExpected (24.008 9.6.2) ─────────────────────────
// Body: [TP-PID(1)] | TP-DCS(1) | [TP-Ud(LV)]

size_t L3SMSProvidedReplyExpected::bodyLength() const {
    size_t len = 0;
    if (mHaveTpPid) len += 1;
    len += 1; // TP-DCS always present
    if (mHaveTpUd) len += 1 + mTpUd.size();
    return len;
}

Expected<L3SMSProvidedReplyExpected> L3SMSProvidedReplyExpected::parse(BitReader& br) {
    L3SMSProvidedReplyExpected msg;
    size_t bodyBits = br.remainingBits();
    size_t bodyBytes = bodyBits / 8;

    if (bodyBytes == 0) return Expected<L3SMSProvidedReplyExpected>::error(
        ParseError{ParseError::Code::TruncatedInput, "SMS Provided Reply Expected: empty body"});

    // Read all remaining bytes
    std::vector<uint8_t> bytes = readRemainingBytes(br);
    size_t pos = 0;

    // Heuristic: if more than 1 byte, first is TP-PID, second is TP-DCS
    if (bytes.size() >= 2) {
        msg.mHaveTpPid = true;
        msg.mTpPid = static_cast<TPPID>(bytes[pos++]);
        msg.mTpDcs = static_cast<TPDCS>(bytes[pos++]);
    } else {
        msg.mTpDcs = static_cast<TPDCS>(bytes[pos++]);
    }

    // Remaining bytes: TP-Ud length + data
    if (pos < bytes.size()) {
        uint8_t udLen = bytes[pos++];
        if (pos + udLen <= bytes.size()) {
            msg.mHaveTpUd = true;
            msg.mTpUd.assign(bytes.begin() + pos, bytes.begin() + pos + udLen);
        }
    }

    return Expected<L3SMSProvidedReplyExpected>::hold(std::move(msg));
}

void L3SMSProvidedReplyExpected::write(BitWriter& bw) const {
    writePidDcsUd(bw, mHaveTpPid, mTpPid, mTpDcs, mHaveTpUd, mTpUd);
}

void L3SMSProvidedReplyExpected::text(std::ostream& os) const {
    os << "SMS-ProvidedReplyExpected(dcs=" << static_cast<int>(static_cast<uint8_t>(mTpDcs))
       << (mHaveTpUd ? ",udLen=" + std::to_string(mTpUd.size()) : "") << ")";
}

// ── L3SMSSubmitRep (24.008 9.6.3) ─────────────────────────────────────
// Body: [TP-PID(1)] | TP-DCS(1) | [TP-Ud(LV)] - same structure as ProvidedReplyExpected

size_t L3SMSSubmitRep::bodyLength() const {
    size_t len = 0;
    if (mHaveTpPid) len += 1;
    len += 1;
    if (mHaveTpUd) len += 1 + mTpUd.size();
    return len;
}

Expected<L3SMSSubmitRep> L3SMSSubmitRep::parse(BitReader& br) {
    L3SMSSubmitRep msg;
    std::vector<uint8_t> bytes = readRemainingBytes(br);
    size_t pos = 0;

    if (bytes.empty()) return Expected<L3SMSSubmitRep>::error(
        ParseError{ParseError::Code::TruncatedInput, "SMS Submit Reply: empty body"});

    if (bytes.size() >= 2) {
        msg.mHaveTpPid = true;
        msg.mTpPid = static_cast<TPPID>(bytes[pos++]);
        msg.mTpDcs = static_cast<TPDCS>(bytes[pos++]);
    } else {
        msg.mTpDcs = static_cast<TPDCS>(bytes[pos++]);
    }

    if (pos < bytes.size()) {
        uint8_t udLen = bytes[pos++];
        if (pos + udLen <= bytes.size()) {
            msg.mHaveTpUd = true;
            msg.mTpUd.assign(bytes.begin() + pos, bytes.begin() + pos + udLen);
        }
    }

    return Expected<L3SMSSubmitRep>::hold(std::move(msg));
}

void L3SMSSubmitRep::write(BitWriter& bw) const {
    writePidDcsUd(bw, mHaveTpPid, mTpPid, mTpDcs, mHaveTpUd, mTpUd);
}

void L3SMSSubmitRep::text(std::ostream& os) const {
    os << "SMS-SubmitReply(dcs=" << static_cast<int>(static_cast<uint8_t>(mTpDcs))
       << (mHaveTpUd ? ",udLen=" + std::to_string(mTpUd.size()) : "") << ")";
}

// ── L3SMSDeliver (24.008 9.6.4) ───────────────────────────────────────
// Body: TP-MTI(4)|TP-MR(1)|[TP-OA(LV)]|TP-PID(1)|TP-DCS(1)|SCTS(7)|[TP-Ud(LV)]

size_t L3SMSDeliver::bodyLength() const {
    size_t len = 2; // TP-MTI(4bits) + TP-MR(1) -> we store as 2 bytes
    if (mHaveTpOa) len += mTpOa.totalLength();
    len += 1; // TP-PID
    len += 1; // TP-DCS
    len += 7; // SCTS
    if (mHaveTpUd) len += 1 + mTpUd.size();
    return len;
}

Expected<L3SMSDeliver> L3SMSDeliver::parse(BitReader& br) {
    L3SMSDeliver msg;

    // Read TP-MTI(4 bits) | spare(4 bits) as first byte
    auto hdr = br.readField(8);
    if (!hdr) return Expected<L3SMSDeliver>::error(hdr.error());
    msg.mTpMti = static_cast<uint8_t>(hdr.value() >> 4);

    // Read TP-MR(1 octet)
    auto tpMr = br.readField(8);
    if (!tpMr) return Expected<L3SMSDeliver>::error(tpMr.error());
    msg.mTpMr = static_cast<uint8_t>(tpMr.value());

    // Optional TP-OA (LV) - only parse if enough bytes remain for required fields after it.
    // Required after TP-OA: TP-PID(1) + TP-DCS(1) + SCTS(7) = 9 bytes minimum.
    size_t remainingBytes = br.remainingBits() / 8;
    if (remainingBytes >= 10) {
        uint8_t lenByte;
        {
            auto lb = br.readField(8);
            if (!lb) return Expected<L3SMSDeliver>::error(lb.error());
            lenByte = static_cast<uint8_t>(lb.value());
        }
        if (lenByte >= 1 && remainingBytes >= 9 + 1 + static_cast<size_t>(lenByte)) {
            std::vector<uint8_t> addrBytes;
            addrBytes.push_back(lenByte);
            for (size_t i = 0; i < lenByte; ++i) {
                auto ab = br.readField(8);
                if (!ab) return Expected<L3SMSDeliver>::error(ab.error());
                addrBytes.push_back(static_cast<uint8_t>(ab.value()));
            }
            BitReader addrBr(addrBytes.data(), addrBytes.size() * 8);
            auto oaResult = L3TPAddress::parse(addrBr);
            if (oaResult) {
                msg.mHaveTpOa = true;
                msg.mTpOa = std::move(oaResult.value());
            } else {
                for (size_t j = 0; j < addrBytes.size(); ++j) {
                    auto _ = br.readField(8);
                    (void)_;
                }
            }
        }
    }

    // TP-PID(1 octet)
    auto pid = br.readField(8);
    if (!pid) return Expected<L3SMSDeliver>::error(pid.error());
    msg.mTpPid = static_cast<TPPID>(pid.value());

    // TP-DCS(1 octet)
    auto dcs = br.readField(8);
    if (!dcs) return Expected<L3SMSDeliver>::error(dcs.error());
    msg.mTpDcs = static_cast<TPDCS>(dcs.value());

    // SCTS(7 octets)
    {
        auto sctsResult = TPSCTimeStamp::parse(br);
        if (!sctsResult) return Expected<L3SMSDeliver>::error(sctsResult.error());
        msg.mScts = std::move(sctsResult.value());
    }

    // Optional TP-Ud (LV)
    if (br.hasMore()) {
        auto udLen = br.readField(8);
        if (udLen) {
            size_t len = udLen.value();
            if (len > 0 && len <= static_cast<size_t>(br.remainingBits() / 8)) {
                msg.mHaveTpUd = true;
                for (size_t i = 0; i < len; ++i) {
                    auto b = br.readField(8);
                    if (b) msg.mTpUd.push_back(static_cast<uint8_t>(b.value()));
                    else break;
                }
            }
        }
    }

    return Expected<L3SMSDeliver>::hold(std::move(msg));
}

void L3SMSDeliver::write(BitWriter& bw) const {
    // Write TP-MTI(4 bits)|spare(4 bits) as first byte
    bw.writeField((mTpMti & 0x0F) << 4, 8);
    // Write TP-MR(1 octet)
    bw.writeField(mTpMr, 8);
    // Write optional TP-OA(LV)
    if (mHaveTpOa) mTpOa.write(bw);
    // Write TP-PID(1 octet)
    bw.writeField(static_cast<uint32_t>(mTpPid), 8);
    // Write TP-DCS(1 octet)
    bw.writeField(static_cast<uint32_t>(mTpDcs), 8);
    // Write SCTS(7 octets)
    mScts.write(bw);
    // Write optional TP-Ud(LV)
    if (mHaveTpUd) {
        bw.writeField(static_cast<uint32_t>(mTpUd.size()), 8);
        for (uint8_t b : mTpUd) bw.writeField(b, 8);
    }
}

void L3SMSDeliver::text(std::ostream& os) const {
    os << "SMS-Deliver(tpMr=" << static_cast<int>(mTpMr)
       << ",pid=" << static_cast<int>(static_cast<uint8_t>(mTpPid))
       << ",dcs=" << static_cast<int>(static_cast<uint8_t>(mTpDcs))
       << (mHaveTpUd ? ",udLen=" + std::to_string(mTpUd.size()) : "") << ")";
}

// ── L3SMSDeliverRep (24.008 9.6.5) ────────────────────────────────────
// Body: TP-MTI(4)|TP-MR(1)|[TP-DA(LV)]|TP-PID(1)|TP-DCS(1)|[TP-Ud(LV)]

size_t L3SMSDeliverRep::bodyLength() const {
    size_t len = 2; // TP-MTI + TP-MR
    if (mHaveTpDa) len += mTpDa.totalLength();
    len += 2; // TP-PID + TP-DCS
    if (mHaveTpUd) len += 1 + mTpUd.size();
    return len;
}

Expected<L3SMSDeliverRep> L3SMSDeliverRep::parse(BitReader& br) {
    L3SMSDeliverRep msg;

    // Read TP-MTI(4 bits)|spare(4 bits)
    auto hdr = br.readField(8);
    if (!hdr) return Expected<L3SMSDeliverRep>::error(hdr.error());
    msg.mTpMti = static_cast<uint8_t>(hdr.value() >> 4);

    // Read TP-MR(1 octet)
    auto tpMr = br.readField(8);
    if (!tpMr) return Expected<L3SMSDeliverRep>::error(tpMr.error());
    msg.mTpMr = static_cast<uint8_t>(tpMr.value());

    // Optional TP-DA (LV) - only parse if enough bytes remain for required fields after it.
    // Required after TP-DA: TP-PID(1) + TP-DCS(1) = 2 bytes minimum.
    size_t remainingBytes = br.remainingBits() / 8;
    if (remainingBytes >= 3) {
        uint8_t lenByte;
        {
            auto lb = br.readField(8);
            if (!lb) return Expected<L3SMSDeliverRep>::error(lb.error());
            lenByte = static_cast<uint8_t>(lb.value());
        }
        if (lenByte >= 1 && remainingBytes >= 2 + 1 + static_cast<size_t>(lenByte)) {
            std::vector<uint8_t> addrBytes;
            addrBytes.push_back(lenByte);
            for (size_t i = 0; i < lenByte; ++i) {
                auto ab = br.readField(8);
                if (!ab) return Expected<L3SMSDeliverRep>::error(ab.error());
                addrBytes.push_back(static_cast<uint8_t>(ab.value()));
            }
            BitReader addrBr(addrBytes.data(), addrBytes.size() * 8);
            auto daResult = L3TPAddress::parse(addrBr);
            if (daResult) {
                msg.mHaveTpDa = true;
                msg.mTpDa = std::move(daResult.value());
            } else {
                for (size_t j = 0; j < addrBytes.size(); ++j) {
                    auto _ = br.readField(8);
                    (void)_;
                }
            }
        }
    }

    // TP-PID(1 octet)
    auto pid = br.readField(8);
    if (!pid) return Expected<L3SMSDeliverRep>::error(pid.error());
    msg.mTpPid = static_cast<TPPID>(pid.value());

    // TP-DCS(1 octet)
    auto dcs = br.readField(8);
    if (!dcs) return Expected<L3SMSDeliverRep>::error(dcs.error());
    msg.mTpDcs = static_cast<TPDCS>(dcs.value());

    // Optional TP-Ud (LV)
    if (br.hasMore()) {
        auto udLen = br.readField(8);
        if (udLen) {
            size_t len = udLen.value();
            if (len > 0 && len <= static_cast<size_t>(br.remainingBits() / 8)) {
                msg.mHaveTpUd = true;
                for (size_t i = 0; i < len; ++i) {
                    auto b = br.readField(8);
                    if (b) msg.mTpUd.push_back(static_cast<uint8_t>(b.value()));
                    else break;
                }
            }
        }
    }

    return Expected<L3SMSDeliverRep>::hold(std::move(msg));
}

void L3SMSDeliverRep::write(BitWriter& bw) const {
    bw.writeField((mTpMti & 0x0F) << 4, 8);
    bw.writeField(mTpMr, 8);
    if (mHaveTpDa) mTpDa.write(bw);
    bw.writeField(static_cast<uint32_t>(mTpPid), 8);
    bw.writeField(static_cast<uint32_t>(mTpDcs), 8);
    if (mHaveTpUd) {
        bw.writeField(static_cast<uint32_t>(mTpUd.size()), 8);
        for (uint8_t b : mTpUd) bw.writeField(b, 8);
    }
}

void L3SMSDeliverRep::text(std::ostream& os) const {
    os << "SMS-DeliverReply(tpMr=" << static_cast<int>(mTpMr)
       << ",pid=" << static_cast<int>(static_cast<uint8_t>(mTpPid))
       << (mHaveTpUd ? ",udLen=" + std::to_string(mTpUd.size()) : "") << ")";
}

// ── L3SMSStatusReportAck (24.008 9.6.6) ───────────────────────────────
// Body: TP-MR(1)

Expected<L3SMSStatusReportAck> L3SMSStatusReportAck::parse(BitReader& br) {
    L3SMSStatusReportAck msg;

    auto tpMr = br.readField(8);
    if (!tpMr) return Expected<L3SMSStatusReportAck>::error(tpMr.error());
    msg.mTpMr = static_cast<uint8_t>(tpMr.value());

    return Expected<L3SMSStatusReportAck>::hold(std::move(msg));
}

void L3SMSStatusReportAck::write(BitWriter& bw) const {
    bw.writeField(mTpMr, 8);
}

void L3SMSStatusReportAck::text(std::ostream& os) const {
    os << "SMS-StatusReportAck(tpMr=" << static_cast<int>(mTpMr) << ")";
}

// ── L3SMSStatusReportReject (24.008 9.6.7) ────────────────────────────
// Body: TP-MR(1) | SM-Cause(1)

Expected<L3SMSStatusReportReject> L3SMSStatusReportReject::parse(BitReader& br) {
    L3SMSStatusReportReject msg;

    auto tpMr = br.readField(8);
    if (!tpMr) return Expected<L3SMSStatusReportReject>::error(tpMr.error());
    msg.mTpMr = static_cast<uint8_t>(tpMr.value());

    auto cause = br.readField(8);
    if (!cause) return Expected<L3SMSStatusReportReject>::error(cause.error());
    msg.mSmCause = static_cast<SMSCause>(cause.value() & 0x7F);

    return Expected<L3SMSStatusReportReject>::hold(std::move(msg));
}

void L3SMSStatusReportReject::write(BitWriter& bw) const {
    bw.writeField(mTpMr, 8);
    bw.writeField(static_cast<uint32_t>(mSmCause), 8);
}

void L3SMSStatusReportReject::text(std::ostream& os) const {
    os << "SMS-StatusReportReject(tpMr=" << static_cast<int>(mTpMr)
       << ",cause=" << SMSCause2Str(mSmCause) << ")";
}

// ── L3SMSTSReject (24.008 9.6.8) ──────────────────────────────────────
// Body: SM-Cause(1)

Expected<L3SMSTSReject> L3SMSTSReject::parse(BitReader& br) {
    L3SMSTSReject msg;

    auto cause = br.readField(8);
    if (!cause) return Expected<L3SMSTSReject>::error(cause.error());
    msg.mSmCause = static_cast<SMSCause>(cause.value() & 0x7F);

    return Expected<L3SMSTSReject>::hold(std::move(msg));
}

void L3SMSTSReject::write(BitWriter& bw) const {
    bw.writeField(static_cast<uint32_t>(mSmCause), 8);
}

void L3SMSTSReject::text(std::ostream& os) const {
    os << "SMS-TSReject(cause=" << SMSCause2Str(mSmCause) << ")";
}

// ── L3SMSSubmitDeferred (24.008 9.6.9) ────────────────────────────────
// Body: [TP-PID(1)] | TP-DCS(1) | [TP-Ud(LV)]

size_t L3SMSSubmitDeferred::bodyLength() const {
    size_t len = 0;
    if (mHaveTpPid) len += 1;
    len += 1;
    if (mHaveTpUd) len += 1 + mTpUd.size();
    return len;
}

Expected<L3SMSSubmitDeferred> L3SMSSubmitDeferred::parse(BitReader& br) {
    L3SMSSubmitDeferred msg;
    std::vector<uint8_t> bytes = readRemainingBytes(br);
    size_t pos = 0;

    if (bytes.empty()) return Expected<L3SMSSubmitDeferred>::error(
        ParseError{ParseError::Code::TruncatedInput, "SMS Submit Deferred: empty body"});

    if (bytes.size() >= 2) {
        msg.mHaveTpPid = true;
        msg.mTpPid = static_cast<TPPID>(bytes[pos++]);
        msg.mTpDcs = static_cast<TPDCS>(bytes[pos++]);
    } else {
        msg.mTpDcs = static_cast<TPDCS>(bytes[pos++]);
    }

    if (pos < bytes.size()) {
        uint8_t udLen = bytes[pos++];
        if (pos + udLen <= bytes.size()) {
            msg.mHaveTpUd = true;
            msg.mTpUd.assign(bytes.begin() + pos, bytes.begin() + pos + udLen);
        }
    }

    return Expected<L3SMSSubmitDeferred>::hold(std::move(msg));
}

void L3SMSSubmitDeferred::write(BitWriter& bw) const {
    writePidDcsUd(bw, mHaveTpPid, mTpPid, mTpDcs, mHaveTpUd, mTpUd);
}

void L3SMSSubmitDeferred::text(std::ostream& os) const {
    os << "SMS-SubmitDeferred(dcs=" << static_cast<int>(static_cast<uint8_t>(mTpDcs))
       << (mHaveTpUd ? ",udLen=" + std::to_string(mTpUd.size()) : "") << ")";
}

// ── L3SMSSubmitReject (24.008 9.6.10) ─────────────────────────────────
// Body: SM-Cause(1)

Expected<L3SMSSubmitReject> L3SMSSubmitReject::parse(BitReader& br) {
    L3SMSSubmitReject msg;

    auto cause = br.readField(8);
    if (!cause) return Expected<L3SMSSubmitReject>::error(cause.error());
    msg.mSmCause = static_cast<SMSCause>(cause.value() & 0x7F);

    return Expected<L3SMSSubmitReject>::hold(std::move(msg));
}

void L3SMSSubmitReject::write(BitWriter& bw) const {
    bw.writeField(static_cast<uint32_t>(mSmCause), 8);
}

void L3SMSSubmitReject::text(std::ostream& os) const {
    os << "SMS-SubmitReject(cause=" << SMSCause2Str(mSmCause) << ")";
}

// ── L3SMSSFProvidedRep (24.008 9.6.11) ────────────────────────────────
// Body: [TP-PID(1)] | TP-DCS(1) | [TP-Ud(LV)]

size_t L3SMSSFProvidedRep::bodyLength() const {
    size_t len = 0;
    if (mHaveTpPid) len += 1;
    len += 1;
    if (mHaveTpUd) len += 1 + mTpUd.size();
    return len;
}

Expected<L3SMSSFProvidedRep> L3SMSSFProvidedRep::parse(BitReader& br) {
    L3SMSSFProvidedRep msg;
    std::vector<uint8_t> bytes = readRemainingBytes(br);
    size_t pos = 0;

    if (bytes.empty()) return Expected<L3SMSSFProvidedRep>::error(
        ParseError{ParseError::Code::TruncatedInput, "SMS SSF Provided Reply: empty body"});

    if (bytes.size() >= 2) {
        msg.mHaveTpPid = true;
        msg.mTpPid = static_cast<TPPID>(bytes[pos++]);
        msg.mTpDcs = static_cast<TPDCS>(bytes[pos++]);
    } else {
        msg.mTpDcs = static_cast<TPDCS>(bytes[pos++]);
    }

    if (pos < bytes.size()) {
        uint8_t udLen = bytes[pos++];
        if (pos + udLen <= bytes.size()) {
            msg.mHaveTpUd = true;
            msg.mTpUd.assign(bytes.begin() + pos, bytes.begin() + pos + udLen);
        }
    }

    return Expected<L3SMSSFProvidedRep>::hold(std::move(msg));
}

void L3SMSSFProvidedRep::write(BitWriter& bw) const {
    writePidDcsUd(bw, mHaveTpPid, mTpPid, mTpDcs, mHaveTpUd, mTpUd);
}

void L3SMSSFProvidedRep::text(std::ostream& os) const {
    os << "SMS-SSFProvidedReply(dcs=" << static_cast<int>(static_cast<uint8_t>(mTpDcs))
       << (mHaveTpUd ? ",udLen=" + std::to_string(mTpUd.size()) : "") << ")";
}

// ── L3SMSSFProvidedRepAck (24.008 9.6.12) ─────────────────────────────
// Body: empty

Expected<L3SMSSFProvidedRepAck> L3SMSSFProvidedRepAck::parse(BitReader&) {
    return Expected<L3SMSSFProvidedRepAck>::hold(L3SMSSFProvidedRepAck{});
}

void L3SMSSFProvidedRepAck::write(BitWriter&) const {}

void L3SMSSFProvidedRepAck::text(std::ostream& os) const {
    os << "SMS-SSFProvidedReplyAck";
}

// ── L3SMSNotification (24.008 9.6.13) ─────────────────────────────────
// Body: [TP-PID(1)] | TP-DCS(1) | [TP-Ud(LV)]

size_t L3SMSNotification::bodyLength() const {
    size_t len = 0;
    if (mHaveTpPid) len += 1;
    len += 1;
    if (mHaveTpUd) len += 1 + mTpUd.size();
    return len;
}

Expected<L3SMSNotification> L3SMSNotification::parse(BitReader& br) {
    L3SMSNotification msg;
    std::vector<uint8_t> bytes = readRemainingBytes(br);
    size_t pos = 0;

    if (bytes.empty()) return Expected<L3SMSNotification>::error(
        ParseError{ParseError::Code::TruncatedInput, "SMS Notification: empty body"});

    if (bytes.size() >= 2) {
        msg.mHaveTpPid = true;
        msg.mTpPid = static_cast<TPPID>(bytes[pos++]);
        msg.mTpDcs = static_cast<TPDCS>(bytes[pos++]);
    } else {
        msg.mTpDcs = static_cast<TPDCS>(bytes[pos++]);
    }

    if (pos < bytes.size()) {
        uint8_t udLen = bytes[pos++];
        if (pos + udLen <= bytes.size()) {
            msg.mHaveTpUd = true;
            msg.mTpUd.assign(bytes.begin() + pos, bytes.begin() + pos + udLen);
        }
    }

    return Expected<L3SMSNotification>::hold(std::move(msg));
}

void L3SMSNotification::write(BitWriter& bw) const {
    writePidDcsUd(bw, mHaveTpPid, mTpPid, mTpDcs, mHaveTpUd, mTpUd);
}

void L3SMSNotification::text(std::ostream& os) const {
    os << "SMS-Notification(dcs=" << static_cast<int>(static_cast<uint8_t>(mTpDcs))
       << (mHaveTpUd ? ",udLen=" + std::to_string(mTpUd.size()) : "") << ")";
}

// ── L3SMSShortCodeInfo (24.008 9.6.14) ────────────────────────────────
// Body: ShortCodeType(1) | [ShortCode(LV)]

size_t L3SMSShortCodeInfo::bodyLength() const {
    size_t len = 1; // ShortCodeType
    if (mHaveShortCode) len += 1 + mShortCode.size(); // LV format
    return len;
}

Expected<L3SMSShortCodeInfo> L3SMSShortCodeInfo::parse(BitReader& br) {
    L3SMSShortCodeInfo msg;

    auto scType = br.readField(8);
    if (!scType) return Expected<L3SMSShortCodeInfo>::error(scType.error());
    msg.mShortCodeType = static_cast<uint8_t>(scType.value() & 0x0F);

    // Optional ShortCode (LV)
    if (br.hasMore()) {
        auto scLen = br.readField(8);
        if (scLen) {
            size_t len = scLen.value();
            if (len > 0 && len <= static_cast<size_t>(br.remainingBits() / 8)) {
                msg.mHaveShortCode = true;
                for (size_t i = 0; i < len; ++i) {
                    auto b = br.readField(8);
                    if (b) msg.mShortCode.push_back(static_cast<uint8_t>(b.value()));
                    else break;
                }
            }
        }
    }

    return Expected<L3SMSShortCodeInfo>::hold(std::move(msg));
}

void L3SMSShortCodeInfo::write(BitWriter& bw) const {
    bw.writeField(mShortCodeType & 0x0F, 8);
    if (mHaveShortCode) {
        bw.writeField(static_cast<uint32_t>(mShortCode.size()), 8);
        for (uint8_t b : mShortCode) bw.writeField(b, 8);
    }
}

void L3SMSShortCodeInfo::text(std::ostream& os) const {
    os << "SMS-ShortCodeInfo(scType=" << static_cast<int>(mShortCodeType)
       << (mHaveShortCode ? ",scLen=" + std::to_string(mShortCode.size()) : "") << ")";
}

// ── smsL3MessageName: names for L3 SMS messages ────────────────────────

const char* smsL3MessageName(int mti) {
    switch (mti) {
        case L3SMSStatusReport::MTI: return "SMSStatusReport";
        case L3SMSProvidedReplyExpected::MTI: return "SMSProvidedReplyExpected";
        case L3SMSSubmitRep::MTI: return "SMSSubmitReply";
        case L3SMSDeliver::MTI: return "SMSDeliver";
        case L3SMSDeliverRep::MTI: return "SMSDeliverReply";
        case L3SMSStatusReportAck::MTI: return "SMSStatusReportAck";
        case L3SMSStatusReportReject::MTI: return "SMSStatusReportReject";
        case L3SMSTSReject::MTI: return "SMSTSReject";
        case L3SMSSubmitDeferred::MTI: return "SMSSubmitDeferred";
        case L3SMSSubmitReject::MTI: return "SMSSubmitReject";
        case L3SMSSFProvidedRep::MTI: return "SMSSFProvidedReply";
        case L3SMSSFProvidedRepAck::MTI: return "SMSSFProvidedReplyAck";
        case L3SMSNotification::MTI: return "SMSNotification";
        case L3SMSShortCodeInfo::MTI: return "SMSShortCodeInfo";
        default: return nullptr;
    }
}

} // namespace gsml3parser
