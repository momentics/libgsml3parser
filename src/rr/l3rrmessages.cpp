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

#include "gsml3parser/rr/l3rrmessages.h"
#include "gsml3parser/logger.h"
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace gsml3parser {

// ── L3RRMessage ─────────────────────────────────────────────────────────

const char* L3RRMessage::name(MessageType mt) {
    switch (mt) {
        case SystemInformationType1:  return "SystemInformationType1";
        case SystemInformationType2:  return "SystemInformationType2";
        case SystemInformationType2bis: return "SystemInformationType2bis";
        case SystemInformationType2ter: return "SystemInformationType2ter";
        case SystemInformationType3:  return "SystemInformationType3";
        case SystemInformationType4:  return "SystemInformationType4";
        case SystemInformationType5:  return "SystemInformationType5";
        case SystemInformationType5bis: return "SystemInformationType5bis";
        case SystemInformationType5ter: return "SystemInformationType5ter";
        case SystemInformationType6:  return "SystemInformationType6";
        case SystemInformationType7:  return "SystemInformationType7";
        case SystemInformationType8:  return "SystemInformationType8";
        case SystemInformationType9:  return "SystemInformationType9";
        case SystemInformationType13: return "SystemInformationType13";
        case SystemInformationType16: return "SystemInformationType16";
        case SystemInformationType17: return "SystemInformationType17";
        case AssignmentCommand:       return "AssignmentCommand";
        case AssignmentComplete:      return "AssignmentComplete";
        case AssignmentFailure:       return "AssignmentFailure";
        case ChannelRelease:          return "ChannelRelease";
        case ImmediateAssignment:     return "ImmediateAssignment";
        case ImmediateAssignmentExtended: return "ImmediateAssignmentExtended";
        case ImmediateAssignmentReject: return "ImmediateAssignmentReject";
        case AdditionalAssignment:    return "AdditionalAssignment";
        case PagingRequestType1:      return "PagingRequestType1";
        case PagingRequestType2:      return "PagingRequestType2";
        case PagingRequestType3:      return "PagingRequestType3";
        case PagingResponse:          return "PagingResponse";
        case HandoverCommand:         return "HandoverCommand";
        case HandoverComplete:        return "HandoverComplete";
        case HandoverFailure:         return "HandoverFailure";
        case PhysicalInformation:     return "PhysicalInformation";
        case CipheringModeCommand:    return "CipheringModeCommand";
        case CipheringModeComplete:   return "CipheringModeComplete";
        case ChannelModeModify:       return "ChannelModeModify";
        case ChannelModeModifyAcknowledge: return "ChannelModeModifyAcknowledge";
        case RRStatus:                return "RRStatus";
        case ClassmarkChange:         return "ClassmarkChange";
        case ClassmarkEnquiry:        return "ClassmarkEnquiry";
        case MeasurementReport:       return "MeasurementReport";
        case GPRSSuspensionRequest:   return "GPRSSuspensionRequest";
        case ApplicationInformation:  return "ApplicationInformation";
        case SynchronizationChannelInformation: return "SynchronizationChannelInformation";
        case ChannelRequest:          return "ChannelRequest";
        case HandoverAccess:          return "HandoverAccess";
        default:                      return "Unknown_RR";
    }
}

void L3RRMessage::text(std::ostream& os) const {
    L3Message::text(os);
    os << " (" << name(static_cast<MessageType>(mti())) << ")";
}

std::ostream& operator<<(std::ostream& os, L3RRMessage::MessageType mt) {
    os << L3RRMessage::name(mt);
    return os;
}

// ── L3PagingRequestType1 ────────────────────────────────────────────────

L3PagingRequestType1::L3PagingRequestType1() {
    mMobileIDs.emplace_back();
    mChannelsNeeded[0] = ChannelType::AnyDCCHType;
    mChannelsNeeded[1] = ChannelType::AnyDCCHType;
}

// ── Builder ────────────────────────────────────────────────────────────

L3PagingRequestType1::Builder L3PagingRequestType1::builder() { return Builder{}; }

L3PagingRequestType1::Builder& L3PagingRequestType1::Builder::addMobileId(const L3MobileIdentity& id, ChannelType type) {
    mMobileIds.push_back(id);
    if (mMobileIds.size() <= 2) {
        mChannelsNeeded[mMobileIds.size() - 1] = type;
    }
    return *this;
}

L3PagingRequestType1 L3PagingRequestType1::Builder::build() {
    L3PagingRequestType1 msg;
    if (mMobileIds.empty()) {
        msg.mMobileIDs.emplace_back();
    } else {
        msg.mMobileIDs = std::move(mMobileIds);
    }
    msg.mChannelsNeeded = mChannelsNeeded;
    return msg;
}

size_t L3PagingRequestType1::l2BodyLength() const {
    int sz = static_cast<int>(mMobileIDs.size());
    size_t len = 1;
    len += mMobileIDs[0].lengthLV();
    if (sz > 1) len += mMobileIDs[1].lengthTLV();
    return len;
}

constexpr unsigned channelNeededCode(ChannelType wType) {
    switch (wType) {
        case ChannelType::AnyDCCHType: return 0;
        case ChannelType::SDCCHType: return 1;
        case ChannelType::TCHFType: return 2;
        case ChannelType::AnyTCHType: return 3;
        default: return 0;
    }
}

constexpr ChannelType channelNeededType(unsigned code) {
    switch (code) {
        case 0: return ChannelType::AnyDCCHType;
        case 1: return ChannelType::SDCCHType;
        case 2: return ChannelType::TCHFType;
        case 3: return ChannelType::AnyTCHType;
        default: return ChannelType::AnyDCCHType;
    }
}

ParseResult<void> L3PagingRequestType1::try_writeBody(L3Frame& dest, size_t& wp) const {
    // GSM 04.08 9.1.22: reverse order for half-octet fields
    int sz = static_cast<int>(mMobileIDs.size());
    dest.writeField(wp, channelNeededCode(mChannelsNeeded[sz > 1 ? 1 : 0]), 2);
    dest.writeField(wp, channelNeededCode(mChannelsNeeded[0]), 2);
    dest.writeField(wp, 0x0, 4);  // page mode: normal paging
    mMobileIDs[0].writeLV(dest, wp);
    if (sz > 1) mMobileIDs[1].writeTLV(0x17, dest, wp);
    return ParseResult<void>();
}

ParseResult<void> L3PagingRequestType1::try_parseBody(const L3Frame& src, size_t& rp) {
    mChannelsNeeded[1] = channelNeededType(src.readField(rp, 2));
    mChannelsNeeded[0] = channelNeededType(src.readField(rp, 2));
    src.readField(rp, 4);  // page mode
    mMobileIDs.clear();
    L3MobileIdentity id;
    {
        auto res = id.try_parseLV(src, rp);
        if (!res.has_value()) return res;
    }
    mMobileIDs.push_back(id);
    // Second mobile identity is TLV with IEI=0x17
    if (rp + 16 <= src.size() && src.peekField(rp, 8) == 0x17) {
        rp += 8;  // skip IEI
        L3MobileIdentity id2;
        {
            auto res = id2.try_parseLV(src, rp);
            if (!res.has_value()) return res;
        }
        mMobileIDs.push_back(id2);
    }
    return ParseResult<void>();
}

void L3PagingRequestType1::text(std::ostream& os) const {
    os << "PagingRequestType1: ";
    for (const auto& id : mMobileIDs) {
        id.text(os);
    }
}

// ── L3PagingResponse ───────────────────────────────────────────────────

size_t L3PagingResponse::l2BodyLength() const {
    return 1 + mClassmark.lengthLV() + mMobileID.lengthLV();
}

ParseResult<void> L3PagingResponse::try_parseBody(const L3Frame& source, size_t& rp) {
    // GSM 24.008 9.1.25: cipheringKeySequenceNumber(4)|spare1_4(4) = 1 octet, then CM2 LV, MI LV
    // Reference: L3_Templates.ttcn ts_PAG_RESP: cipheringKeySequenceNumber + spare1_4 pack into 1 octet
    // CKSN is high nibble (keySequence 3 bits + spare 1 bit), spare1_4 is low nibble
    mCKSN = source.readField(rp, 4);  // cipheringKeySequenceNumber (high 4 bits)
    rp += 4;  // spare1_4 (low 4 bits)
    {
        auto res = mClassmark.try_parseLV(source, rp);
        if (!res.has_value()) return res;
    }
    {
        auto res = mMobileID.try_parseLV(source, rp);
        if (!res.has_value()) return res;
    }
    return ParseResult<void>();
}

ParseResult<void> L3PagingResponse::try_writeBody(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, mCKSN & 0x0F, 4);  // cipheringKeySequenceNumber (high nibble)
    dest.writeField(wp, 0, 4);              // spare1_4 (low nibble)
    mClassmark.writeLV(dest, wp);
    mMobileID.writeLV(dest, wp);
    return ParseResult<void>();
}

void L3PagingResponse::text(std::ostream& os) const {
    os << "PagingResponse: ";
    mMobileID.text(os);
    os << " ";
    mClassmark.text(os);
}

// ── L3SystemInformationType1 ────────────────────────────────────────────

L3SystemInformationType1::L3SystemInformationType1()
    : mHaveRestOctets(false), mRestOctet(0x2b) {}

ParseResult<void> L3SystemInformationType1::try_parseBody(const L3Frame& src, size_t& rp) {
    {
        auto res = mCellChannelDescription.try_parseV(src, rp);
        if (!res.has_value()) return res;
    }
    {
        auto res = mRACHControlParameters.try_parseV(src, rp);
        if (!res.has_value()) return res;
    }
    // Optional Rest Octets (GSM 44.018 9.1.31, 10.5.2.32)
    if (rp + 8 <= src.size()) {
        mHaveRestOctets = true;
        mRestOctet = static_cast<uint8_t>(src.readField(rp, 8));
    }
    return ParseResult<void>();
}

ParseResult<void> L3SystemInformationType1::try_writeBody(L3Frame& dest, size_t& wp) const {
    mCellChannelDescription.writeV(dest, wp);
    mRACHControlParameters.writeV(dest, wp);
    if (mHaveRestOctets) {
        dest.writeField(wp, mRestOctet, 8);
    }
    return ParseResult<void>();
}

void L3SystemInformationType1::text(std::ostream& os) const {
    os << "SystemInformationType1: ";
    mCellChannelDescription.text(os);
    os << " ";
    mRACHControlParameters.text(os);
    if (mHaveRestOctets) {
        os << " RestOctet=0x" << std::hex << std::setw(2) << std::setfill('0')
           << static_cast<int>(mRestOctet) << std::dec;
    }
}

// ── L3ChannelRelease ───────────────────────────────────────────────────

ParseResult<void> L3ChannelRelease::try_parseBody(const L3Frame& src, size_t& rp) {
    mCause = static_cast<RRCause>(src.readField(rp, 8));
    // Optional GPRS Resumption IEI=0x01, 1 bit
    if (rp + 8 <= src.size() && src.peekField(rp, 8) == 0x01) {
        rp += 8; // skip IEI
        mGprsResumptionPresent = true;
        mGprsResumptionBit = src.readField(rp, 1);
        src.readField(rp, 7); // spare
    }
    return ParseResult<void>();
}

ParseResult<void> L3ChannelRelease::try_writeBody(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, static_cast<unsigned>(mCause), 8);
    if (mGprsResumptionPresent) {
        dest.writeField(wp, 0x01, 8); // IEI
        dest.writeField(wp, mGprsResumptionBit ? 1 : 0, 1);
        dest.writeField(wp, 0, 7); // spare
    }
    return ParseResult<void>();
}

void L3ChannelRelease::text(std::ostream& os) const {
    os << "ChannelRelease: cause=" << RRCause2Str(mCause);
    if (mGprsResumptionPresent) {
        os << " gprsResumption=" << (mGprsResumptionBit ? "on" : "off");
    }
}

// ── L3RRStatus ──────────────────────────────────────────────────────────

ParseResult<void> L3RRStatus::try_parseBody(const L3Frame& src, size_t& rp) {
    mCause = static_cast<RRCause>(src.readField(rp, 8));
    return ParseResult<void>();
}

ParseResult<void> L3RRStatus::try_writeBody(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, static_cast<unsigned>(mCause), 8);
    return ParseResult<void>();
}

void L3RRStatus::text(std::ostream& os) const {
    os << "RRStatus: cause=" << RRCause2Str(mCause);
}

// ── L3AssignmentCommand ─────────────────────────────────────────────────

L3AssignmentCommand::L3AssignmentCommand()
    : mHaveMode1(false) {}

size_t L3AssignmentCommand::l2BodyLength() const {
    size_t len = mChannel.lengthV() + mPowerCommand.lengthV();
    if (mHaveMode1) len += mMode1.lengthTV();
    if (isAMR()) len += mMultiRate.lengthTLV();
    return len;
}

ParseResult<void> L3AssignmentCommand::try_parseBody(const L3Frame& src, size_t& rp) {
    {
        auto res = mChannel.try_parseV(src, rp);
        if (!res.has_value()) return res;
    }
    {
        auto res = mPowerCommand.try_parseV(src, rp);
        if (!res.has_value()) return res;
    }
    // Optional Mode 1 (TV, IEI=0x63)
    {
        auto res = mMode1.try_parseTV(0x63, src, rp);
        if (!res.has_value()) return res;
        mHaveMode1 = res.value();
    }
    // Optional Multi Rate Configuration for AMR (TLV, IEI=0x15)
    if (isAMR()) {
        auto res = mMultiRate.try_parseTLV(0x15, src, rp);
        if (!res.has_value()) return res;
    }
    return ParseResult<void>();
}

ParseResult<void> L3AssignmentCommand::try_writeBody(L3Frame& dest, size_t& wp) const {
    mChannel.writeV(dest, wp);
    mPowerCommand.writeV(dest, wp);
    if (mHaveMode1) {
        mMode1.writeTV(0x63, dest, wp);
    }
    if (isAMR()) {
        mMultiRate.writeTLV(0x15, dest, wp);
    }
    return ParseResult<void>();
}

void L3AssignmentCommand::text(std::ostream& os) const {
    os << "AssignmentCommand: ";
    mChannel.text(os);
    os << " ";
    mPowerCommand.text(os);
    if (mHaveMode1) {
        os << " ";
        mMode1.text(os);
    }
}

// ── L3AssignmentCommand Builder ────────────────────────────────────────

L3AssignmentCommand::Builder L3AssignmentCommand::builder() { return Builder{}; }

L3AssignmentCommand::Builder& L3AssignmentCommand::Builder::channel(const L3ChannelDescription& ch) {
    mChannel = ch;
    return *this;
}

L3AssignmentCommand::Builder& L3AssignmentCommand::Builder::powerCommand(const L3PowerCommand& pc) {
    mPowerCommand = pc;
    return *this;
}

L3AssignmentCommand::Builder& L3AssignmentCommand::Builder::mode1(const L3ChannelMode& mode) {
    mHaveMode1 = true;
    mMode1 = mode;
    return *this;
}

L3AssignmentCommand::Builder& L3AssignmentCommand::Builder::multiRate(const L3MultiRateConfiguration& mr) {
    mMultiRate = mr;
    return *this;
}

L3AssignmentCommand L3AssignmentCommand::Builder::build() {
    L3AssignmentCommand msg;
    msg.mChannel = mChannel;
    msg.mPowerCommand = mPowerCommand;
    msg.mHaveMode1 = mHaveMode1;
    msg.mMode1 = mMode1;
    msg.mMultiRate = mMultiRate;
    return msg;
}

// ── L3ClassmarkEnquiry ──────────────────────────────────────────────────

void L3ClassmarkEnquiry::text(std::ostream& os) const {
    os << "ClassmarkEnquiry";
}

// ── L3AssignmentComplete ───────────────────────────────────────────────

ParseResult<void> L3AssignmentComplete::try_parseBody(const L3Frame& src, size_t& rp) {
    mCause = static_cast<RRCause>(src.readField(rp, 8));
    return ParseResult<void>();
}

ParseResult<void> L3AssignmentComplete::try_writeBody(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, static_cast<unsigned>(mCause), 8);
    return ParseResult<void>();
}

void L3AssignmentComplete::text(std::ostream& os) const {
    os << "AssignmentComplete: cause=" << RRCause2Str(mCause);
}

// ── L3AssignmentFailure ────────────────────────────────────────────────

ParseResult<void> L3AssignmentFailure::try_parseBody(const L3Frame& src, size_t& rp) {
    mCause = static_cast<RRCause>(src.readField(rp, 8));
    return ParseResult<void>();
}

ParseResult<void> L3AssignmentFailure::try_writeBody(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, static_cast<unsigned>(mCause), 8);
    return ParseResult<void>();
}

void L3AssignmentFailure::text(std::ostream& os) const {
    os << "AssignmentFailure: cause=" << RRCause2Str(mCause);
}

// ── L3ClassmarkChange ──────────────────────────────────────────────────

size_t L3ClassmarkChange::l2BodyLength() const {
    size_t len = mClassmark.lengthLV();
    if (mHaveAdditionalClassmark) len += mAdditionalClassmark.lengthTLV();
    return len;
}

ParseResult<void> L3ClassmarkChange::try_parseBody(const L3Frame& src, size_t& rp) {
    // GSM 04.08 9.1.11: classmark(LV), additionalClassmark(TLV 0x20)
    {
        auto res = mClassmark.try_parseLV(src, rp);
        if (!res.has_value()) return res;
    }
    {
        auto res = mAdditionalClassmark.try_parseTLV(0x20, src, rp);
        if (!res.has_value()) return res;
        mHaveAdditionalClassmark = res.value();
    }
    return ParseResult<void>();
}

ParseResult<void> L3ClassmarkChange::try_writeBody(L3Frame& dest, size_t& wp) const {
    mClassmark.writeLV(dest, wp);
    if (mHaveAdditionalClassmark) {
        mAdditionalClassmark.writeTLV(0x20, dest, wp);
    }
    return ParseResult<void>();
}

void L3ClassmarkChange::text(std::ostream& os) const {
    os << "ClassmarkChange: ";
    mClassmark.text(os);
    if (mHaveAdditionalClassmark) {
        os << " ";
        mAdditionalClassmark.text(os);
    }
}

// ── L3MeasurementReport ────────────────────────────────────────────────

ParseResult<void> L3MeasurementReport::try_parseBody(const L3Frame& src, size_t& rp) {
    {
        auto res = mMeasurementResults.try_parseV(src, rp);
        if (!res.has_value()) return res;
    }
    return ParseResult<void>();
}

ParseResult<void> L3MeasurementReport::try_writeBody(L3Frame& dest, size_t& wp) const {
    mMeasurementResults.writeV(dest, wp);
    return ParseResult<void>();
}

void L3MeasurementReport::text(std::ostream& os) const {
    os << "MeasurementReport: ";
    mMeasurementResults.text(os);
}

// ── L3CipheringModeCommand ─────────────────────────────────────────────

int L3CipheringModeCommand::mti() const { return CipheringModeCommand; }

ParseResult<void> L3CipheringModeCommand::try_parseBody(const L3Frame& src, size_t& rp) {
    // Half-octet reverse order: Response first, then Setting
    {
        auto res = mCipheringModeResponse.try_parseV(src, rp);
        if (!res.has_value()) return res;
    }
    L3CipheringModeSetting cms;
    {
        auto res = cms.try_parseV(src, rp);
        if (!res.has_value()) return res;
    }
    mCiphering = cms.ciphering();
    mAlgorithm = cms.algorithm();
    return ParseResult<void>();
}

ParseResult<void> L3CipheringModeCommand::try_writeBody(L3Frame& dest, size_t& wp) const {
    // Half-octet reverse order: Response first, then Setting
    mCipheringModeResponse.writeV(dest, wp);
    L3CipheringModeSetting cms(mCiphering, mAlgorithm);
    cms.writeV(dest, wp);
    return ParseResult<void>();
}

void L3CipheringModeCommand::text(std::ostream& os) const {
    os << "CipheringModeCommand: ciphering=" << mCiphering
       << " algorithm=A5/" << mAlgorithm
       << " includeIMEISV=" << mCipheringModeResponse.includeIMEISV();
}

// ── L3CipheringModeComplete ────────────────────────────────────────────

int L3CipheringModeComplete::mti() const { return CipheringModeComplete; }

void L3CipheringModeComplete::text(std::ostream& os) const {
    os << "CipheringModeComplete";
}

// ── L3HandoverComplete ─────────────────────────────────────────────────

ParseResult<void> L3HandoverComplete::try_parseBody(const L3Frame& src, size_t& rp) {
    mCause = static_cast<RRCause>(src.readField(rp, 8));
    return ParseResult<void>();
}

ParseResult<void> L3HandoverComplete::try_writeBody(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, static_cast<unsigned>(mCause), 8);
    return ParseResult<void>();
}

void L3HandoverComplete::text(std::ostream& os) const {
    os << "HandoverComplete: cause=" << RRCause2Str(mCause);
}

// ── L3HandoverFailure ──────────────────────────────────────────────────

ParseResult<void> L3HandoverFailure::try_parseBody(const L3Frame& src, size_t& rp) {
    mCause = static_cast<RRCause>(src.readField(rp, 8));
    return ParseResult<void>();
}

ParseResult<void> L3HandoverFailure::try_writeBody(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, static_cast<unsigned>(mCause), 8);
    return ParseResult<void>();
}

void L3HandoverFailure::text(std::ostream& os) const {
    os << "HandoverFailure: cause=" << RRCause2Str(mCause);
}

// ── L3ChannelModeModify ────────────────────────────────────────────────

L3ChannelModeModify::L3ChannelModeModify() {}

L3ChannelModeModify::L3ChannelModeModify(const L3ChannelDescription& wDesc, const L3ChannelMode& wMode)
    : mDescription(wDesc), mMode(wMode) {}

size_t L3ChannelModeModify::l2BodyLength() const {
    return mDescription.lengthV() + mMode.lengthV() + (isAMR() ? mMultiRate.lengthTLV() : 0);
}

ParseResult<void> L3ChannelModeModify::try_parseBody(const L3Frame& src, size_t& rp) {
    {
        auto res = mDescription.try_parseV(src, rp);
        if (!res.has_value()) return res;
    }
    {
        auto res = mMode.try_parseV(src, rp);
        if (!res.has_value()) return res;
    }
    if (isAMR()) {
        auto res = mMultiRate.try_parseTLV(0x15, src, rp);
        if (!res.has_value()) return res;
    }
    return ParseResult<void>();
}

ParseResult<void> L3ChannelModeModify::try_writeBody(L3Frame& dest, size_t& wp) const {
    mDescription.writeV(dest, wp);
    mMode.writeV(dest, wp);
    if (isAMR()) {
        mMultiRate.writeTLV(0x15, dest, wp);
    }
    return ParseResult<void>();
}

void L3ChannelModeModify::text(std::ostream& os) const {
    os << "ChannelModeModify: ";
    mDescription.text(os);
    os << " ";
    mMode.text(os);
}

// ── L3ChannelModeModifyAcknowledge ─────────────────────────────────────

ParseResult<void> L3ChannelModeModifyAcknowledge::try_parseBody(const L3Frame& src, size_t& rp) {
    {
        auto res = mDescription.try_parseV(src, rp);
        if (!res.has_value()) return res;
    }
    {
        auto res = mMode.try_parseV(src, rp);
        if (!res.has_value()) return res;
    }
    return ParseResult<void>();
}

size_t L3ChannelModeModifyAcknowledge::l2BodyLength() const {
    return mDescription.lengthV() + mMode.lengthV();
}

ParseResult<void> L3ChannelModeModifyAcknowledge::try_writeBody(L3Frame& dest, size_t& wp) const {
    mDescription.writeV(dest, wp);
    mMode.writeV(dest, wp);
    return ParseResult<void>();
}

void L3ChannelModeModifyAcknowledge::text(std::ostream& os) const {
    os << "ChannelModeModifyAcknowledge: ";
    mDescription.text(os);
    os << " ";
    mMode.text(os);
}

// ── L3GPRSSuspensionRequest ────────────────────────────────────────────

ParseResult<void> L3GPRSSuspensionRequest::try_parseBody(const L3Frame& src, size_t& rp) {
    // GSM 04.08 9.1.13b: TLLI(32), RaId(48), suspensionCause(8), optional serviceSupport(TLV 0x01)
    mTLLI = src.readField(rp, 32);
    mRaId.resize(6);
    for (size_t i = 0; i < 6; ++i) {
        mRaId[i] = static_cast<uint8_t>(src.readField(rp, 8));
    }
    mSuspensionCause = src.readField(rp, 8);
    // Optional service support, IEI=0x01
    if (rp + 16 <= src.size() && src.peekField(rp, 8) == 0x01) {
        rp += 8;  // skip IEI
        mServiceSupport = src.readField(rp, 8);
    }
    return ParseResult<void>();
}

ParseResult<void> L3GPRSSuspensionRequest::try_writeBody(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, mTLLI, 32);
    for (size_t i = 0; i < 6; ++i) {
        dest.writeField(wp, mRaId[i], 8);
    }
    dest.writeField(wp, mSuspensionCause, 8);
    if (mServiceSupport) {
        dest.writeField(wp, 0x01, 8);  // IEI for service support
        dest.writeField(wp, mServiceSupport, 8);
    }
    return ParseResult<void>();
}

void L3GPRSSuspensionRequest::text(std::ostream& os) const {
    os << "GPRSSuspensionRequest: TLLI=0x" << std::hex << mTLLI
        << " cause=" << static_cast<int>(mSuspensionCause);
}

// ── L3ApplicationInformation ───────────────────────────────────────────

L3ApplicationInformation::L3ApplicationInformation()
    : mProtocolIdentifier(0), mCR(0), mFirstSegment(0), mLastSegment(0) {}

L3ApplicationInformation::L3ApplicationInformation(BitVector data, unsigned protocolId,
                                                     unsigned cr, unsigned first, unsigned last)
    : mProtocolIdentifier(protocolId), mCR(cr), mFirstSegment(first),
      mLastSegment(last), mData(std::move(data)) {}

size_t L3ApplicationInformation::l2BodyLength() const {
    // APDU ID (4 bits) + APDU Flags (4 bits) = 1 byte, then APDU Data (LV)
    size_t dataLen = (mData.size() + 7) / 8;
    return 1 + 1 + dataLen;
}

ParseResult<void> L3ApplicationInformation::try_writeBody(L3Frame& dest, size_t& wp) const {
    // Half-octet reverse order: APDU Flags first, then APDU ID
    dest.writeField(wp, 0, 1);
    dest.writeField(wp, mCR, 1);
    dest.writeField(wp, mFirstSegment, 1);
    dest.writeField(wp, mLastSegment, 1);
    dest.writeField(wp, mProtocolIdentifier, 4);
    // APDU Data as LV
    size_t dataLen = (mData.size() + 7) / 8;
    dest.writeField(wp, dataLen, 8);
    size_t wp2 = 0;
    for (size_t i = 0; i < mData.size(); ++i) {
        unsigned bit = mData.readField(wp2, 1);
        dest.writeField(wp, bit, 1);
    }
    while (wp % 8 != 0) {
        dest.writeField(wp, 0, 1);
    }
    return ParseResult<void>();
}

ParseResult<void> L3ApplicationInformation::try_parseBody(const L3Frame& src, size_t& rp) {
    // Half-octet reverse order: APDU Flags first, then APDU ID
    src.readField(rp, 1);
    mCR = src.readField(rp, 1);
    mFirstSegment = src.readField(rp, 1);
    mLastSegment = src.readField(rp, 1);
    mProtocolIdentifier = src.readField(rp, 4);
    // APDU Data as LV
    size_t dataLen = src.readField(rp, 8);
    size_t bitsLen = dataLen * 8;
    mData = BitVector(bitsLen);
    size_t wp2 = 0;
    for (size_t i = 0; i < bitsLen && rp + 1 <= src.size(); ++i) {
        unsigned bit = src.readField(rp, 1);
        mData.writeField(wp2, bit, 1);
    }
    return ParseResult<void>();
}

void L3ApplicationInformation::text(std::ostream& os) const {
    os << "ApplicationInformation: PID=" << mProtocolIdentifier
       << " CR=" << mCR << " first=" << mFirstSegment << " last=" << mLastSegment
       << " len=" << mData.size() << "bits";
}

// ── L3SystemInformationType3 ───────────────────────────────────────────

ParseResult<void> L3SystemInformationType3::try_parseBody(const L3Frame& src, size_t& rp) {
    { auto res = mCI.try_parseV(src, rp); if (!res.has_value()) return res; }
    { auto res = mLAI.try_parseV(src, rp); if (!res.has_value()) return res; }
    { auto res = mControlChannelDescription.try_parseV(src, rp); if (!res.has_value()) return res; }
    { auto res = mCellOptions.try_parseV(src, rp); if (!res.has_value()) return res; }
    { auto res = mCellSelectionParameters.try_parseV(src, rp); if (!res.has_value()) return res; }
    { auto res = mRACHControlParameters.try_parseV(src, rp); if (!res.has_value()) return res; }
    { auto res = mRestOctets.try_parseV(src, rp); if (!res.has_value()) return res; }
    return ParseResult<void>();
}

ParseResult<void> L3SystemInformationType3::try_writeBody(L3Frame& dest, size_t& wp) const {
    mCI.writeV(dest, wp);
    mLAI.writeV(dest, wp);
    mControlChannelDescription.writeV(dest, wp);
    mCellOptions.writeV(dest, wp);
    mCellSelectionParameters.writeV(dest, wp);
    mRACHControlParameters.writeV(dest, wp);
    mRestOctets.writeV(dest, wp);
    return ParseResult<void>();
}

void L3SystemInformationType3::text(std::ostream& os) const {
    os << "SystemInformationType3: ";
    mCI.text(os);
    os << " ";
    mLAI.text(os);
    os << " ";
    mControlChannelDescription.text(os);
    os << " ";
    mCellOptions.text(os);
    os << " ";
    mCellSelectionParameters.text(os);
    os << " ";
    mRACHControlParameters.text(os);
    mRestOctets.text(os);
}

// ── L3SystemInformationType13 ──────────────────────────────────────────

ParseResult<void> L3SystemInformationType13::try_parseBody(const L3Frame& src, size_t& rp) {
    { auto res = mRestOctets.try_parseV(src, rp); if (!res.has_value()) return res; }
    return ParseResult<void>();
}

ParseResult<void> L3SystemInformationType13::try_writeBody(L3Frame& dest, size_t& wp) const {
    mRestOctets.writeV(dest, wp);
    return ParseResult<void>();
}

void L3SystemInformationType13::text(std::ostream& os) const {
    os << "SystemInformationType13: ";
    mRestOctets.text(os);
}

// ── L3ImmediateAssignment ──────────────────────────────────────────────

L3ImmediateAssignment::L3ImmediateAssignment()
    : mDedicatedModeOrTBF(false, false), mStartTimePresent(false), mStartTimeFrame(0) {}

size_t L3ImmediateAssignment::l2BodyLength() const {
    // PageMode(1) + DedicatedModeOrTBF(1) + RequestRef(3) + ChannelDesc(3) + TimingAdv(1) + MobileAlloc(LV) + StartTime(opt)
    size_t len = 1 + 1 + 3 + 3 + 1;
    if (!mMobileAllocation.empty()) len += 1 + mMobileAllocation.size();
    if (mStartTimePresent) len += 3;
    return len;
}

ParseResult<void> L3ImmediateAssignment::try_parseBody(const L3Frame& src, size_t& rp) {
    // Half-octet reverse order: DedicatedModeOrTBF first, then PageMode
    { auto res = mDedicatedModeOrTBF.try_parseV(src, rp); if (!res.has_value()) return res; }
    { auto res = mPageMode.try_parseV(src, rp); if (!res.has_value()) return res; }
    { auto res = mRequestReference.try_parseV(src, rp); if (!res.has_value()) return res; }
    { auto res = mChannelDescription.try_parseV(src, rp); if (!res.has_value()) return res; }
    { auto res = mTimingAdvance.try_parseV(src, rp); if (!res.has_value()) return res; }
    // Mobile Allocation: LV format - length byte + data
    if (rp + 8 <= src.size()) {
        size_t maLen = src.readField(rp, 8);
        if (maLen > 0 && rp + maLen * 8 <= src.size()) {
            mMobileAllocation.resize(maLen);
            for (size_t i = 0; i < maLen; ++i) {
                mMobileAllocation[i] = static_cast<uint8_t>(src.readField(rp, 8));
            }
        }
    }
    // StartTime: TLV format (IEI=0x7c)
    if (rp + 8 <= src.size() && src.peekField(rp, 8) == 0x7c) {
        rp += 8; // skip IEI
        mStartTimePresent = true;
        mStartTimeFrame = src.readField(rp, 23);
    }
    return ParseResult<void>();
}

ParseResult<void> L3ImmediateAssignment::try_writeBody(L3Frame& dest, size_t& wp) const {
    // Half-octet reverse order: DedicatedModeOrTBF first, then PageMode
    mDedicatedModeOrTBF.writeV(dest, wp);
    mPageMode.writeV(dest, wp);
    mRequestReference.writeV(dest, wp);
    mChannelDescription.writeV(dest, wp);
    mTimingAdvance.writeV(dest, wp);
    // Mobile Allocation: LV format - length byte + data
    dest.writeField(wp, static_cast<uint8_t>(mMobileAllocation.size()), 8);
    for (const auto& b : mMobileAllocation) {
        dest.writeField(wp, b, 8);
    }
    if (mStartTimePresent) {
        dest.writeField(wp, 0x7c, 8);  // IEI for StartTime
        dest.writeField(wp, mStartTimeFrame, 23);
    }
    return ParseResult<void>();
}

void L3ImmediateAssignment::text(std::ostream& os) const {
    os << "ImmediateAssignment: ";
    mChannelDescription.text(os);
    os << " TA=";
    mTimingAdvance.text(os);
}

// ── L3ImmediateAssignmentExtended ──────────────────────────────────────

L3ImmediateAssignmentExtended::L3ImmediateAssignmentExtended()
    : mDedicatedModeOrTBF(false, false), mStartTimePresent(false), mStartTimeFrame(0),
      mHaveAdditionalChannel(false) {}

size_t L3ImmediateAssignmentExtended::l2BodyLength() const {
    // PageMode(1) + DedicatedModeOrTBF(1) + RequestRef(3) + ChannelDesc(3) + TimingAdv(1) + MobileAlloc(LV) + StartTime(opt) + AdditionalChannel(opt)
    size_t len = 1 + 1 + 3 + 3 + 1;
    if (!mMobileAllocation.empty()) len += 1 + mMobileAllocation.size();
    if (mStartTimePresent) len += 3;
    if (mHaveAdditionalChannel) len += mAdditionalChannel.lengthV();
    return len;
}

ParseResult<void> L3ImmediateAssignmentExtended::try_parseBody(const L3Frame& src, size_t& rp) {
    // Half-octet reverse order: DedicatedModeOrTBF first, then PageMode
    { auto res = mDedicatedModeOrTBF.try_parseV(src, rp); if (!res.has_value()) return res; }
    { auto res = mPageMode.try_parseV(src, rp); if (!res.has_value()) return res; }
    { auto res = mRequestReference.try_parseV(src, rp); if (!res.has_value()) return res; }
    { auto res = mChannelDescription.try_parseV(src, rp); if (!res.has_value()) return res; }
    { auto res = mTimingAdvance.try_parseV(src, rp); if (!res.has_value()) return res; }
    // Mobile Allocation: LV format - length byte + data
    if (rp + 8 <= src.size()) {
        size_t maLen = src.readField(rp, 8);
        if (maLen > 0 && rp + maLen * 8 <= src.size()) {
            mMobileAllocation.resize(maLen);
            for (size_t i = 0; i < maLen; ++i) {
                mMobileAllocation[i] = static_cast<uint8_t>(src.readField(rp, 8));
            }
        }
    }
    // StartTime: TLV format (IEI=0x7c)
    if (rp + 8 <= src.size() && src.peekField(rp, 8) == 0x7c) {
        rp += 8;
        mStartTimePresent = true;
        mStartTimeFrame = src.readField(rp, 23);
    }
    // Additional Channel Description (optional)
    if (rp + 24 <= src.size()) {
        mHaveAdditionalChannel = true;
        { auto res = mAdditionalChannel.try_parseV(src, rp); if (!res.has_value()) return res; }
    }
    return ParseResult<void>();
}

ParseResult<void> L3ImmediateAssignmentExtended::try_writeBody(L3Frame& dest, size_t& wp) const {
    // Half-octet reverse order: DedicatedModeOrTBF first, then PageMode
    mDedicatedModeOrTBF.writeV(dest, wp);
    mPageMode.writeV(dest, wp);
    mRequestReference.writeV(dest, wp);
    mChannelDescription.writeV(dest, wp);
    mTimingAdvance.writeV(dest, wp);
    // Mobile Allocation: LV format - length byte + data
    dest.writeField(wp, static_cast<uint8_t>(mMobileAllocation.size()), 8);
    for (const auto& b : mMobileAllocation) {
        dest.writeField(wp, b, 8);
    }
    if (mStartTimePresent) {
        dest.writeField(wp, 0x7c, 8);
        dest.writeField(wp, mStartTimeFrame, 23);
    }
    if (mHaveAdditionalChannel) {
        mAdditionalChannel.writeV(dest, wp);
    }
    return ParseResult<void>();
}

void L3ImmediateAssignmentExtended::text(std::ostream& os) const {
    os << "ImmediateAssignmentExtended: ";
    mChannelDescription.text(os);
    os << " TA=";
    mTimingAdvance.text(os);
    if (mHaveAdditionalChannel) {
        os << " ";
        mAdditionalChannel.text(os);
    }
}

// ── L3ImmediateAssignmentReject ────────────────────────────────────────

L3ImmediateAssignmentReject::L3ImmediateAssignmentReject()
    : mFeatureIndicator(0), mPageMode(0), mWaitIndication(0) {}

L3ImmediateAssignmentReject::L3ImmediateAssignmentReject(unsigned waitSeconds)
    : mFeatureIndicator(0), mPageMode(0), mWaitIndication(waitSeconds) {}

size_t L3ImmediateAssignmentReject::l2BodyLength() const {
    return 1 + static_cast<size_t>(mRequestReferences.size()) * 4;
}

ParseResult<void> L3ImmediateAssignmentReject::try_parseBody(const L3Frame& src, size_t& rp) {
    // GSM 04.08 9.1.20: FeatureIndicator(4)|PageMode(4), then optional ReqRefWaitInd4
    // Reference: GSM_RR_Types.ttcn ImmediateAssignmentReject {
    //   FeatureIndicator feature_ind, PageMode page_mode, ReqRefWaitInd4 payload }
    mFeatureIndicator = src.readField(rp, 4);
    mPageMode = src.readField(rp, 4);
    // Optional: up to 4 pairs of (RequestReference(24 bits) + WaitIndication(8 bits))
    for (int i = 0; i < 4; ++i) {
        if (rp + 32 <= src.size()) {
            L3RequestReference rr;
            { auto res = rr.try_parseV(src, rp); if (!res.has_value()) return res; }
            mRequestReferences.push_back(rr);
            unsigned waitInd = src.readField(rp, 8);
            mWaitIndications.push_back(waitInd);
        }
    }
    if (mRequestReferences.empty()) {
        mWaitIndication = 0;
    } else {
        mWaitIndication = mWaitIndications.empty() ? 0 : mWaitIndications.back();
    }
    return ParseResult<void>();
}

ParseResult<void> L3ImmediateAssignmentReject::try_writeBody(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, mFeatureIndicator & 0x0F, 4);
    dest.writeField(wp, mPageMode & 0x0F, 4);
    for (size_t i = 0; i < mRequestReferences.size(); ++i) {
        mRequestReferences[i].writeV(dest, wp);
        dest.writeField(wp, i < mWaitIndications.size() ? mWaitIndications[i] : mWaitIndication, 8);
    }
    return ParseResult<void>();
}

void L3ImmediateAssignmentReject::text(std::ostream& os) const {
    os << "ImmediateAssignmentReject: pageMode=" << mPageMode
       << " T3122=" << mWaitIndication;
    os << " requestReferences=(" << mRequestReferences.size() << ")";
}

// ── L3PagingRequestType2 ───────────────────────────────────────────────

L3PagingRequestType2::L3PagingRequestType2() {
    mTMSIs.push_back(0);
    mTMSIs.push_back(0);
    mChannelsNeeded[0] = ChannelType::AnyDCCHType;
    mChannelsNeeded[1] = ChannelType::AnyDCCHType;
}

// ── L3PagingRequestType2 Builder ───────────────────────────────────────

L3PagingRequestType2::Builder L3PagingRequestType2::builder() { return Builder{}; }

L3PagingRequestType2::Builder& L3PagingRequestType2::Builder::addTMSI(uint32_t tmsi, ChannelType type) {
    mTMSIs.push_back(tmsi);
    if (mTMSIs.size() <= 2) {
        mChannelsNeeded[mTMSIs.size() - 1] = type;
    }
    return *this;
}

L3PagingRequestType2 L3PagingRequestType2::Builder::build() {
    L3PagingRequestType2 msg;
    if (mTMSIs.empty()) {
        msg.mTMSIs.push_back(0);
        msg.mTMSIs.push_back(0);
    } else {
        msg.mTMSIs = std::move(mTMSIs);
        while (msg.mTMSIs.size() < 2) msg.mTMSIs.push_back(0);
    }
    msg.mChannelsNeeded = mChannelsNeeded;
    return msg;
}

size_t L3PagingRequestType2::l2BodyLength() const {
    // ChannelNeeded(4)|PageMode(4) + GsmTmsi mi1(4) + GsmTmsi mi2(4) = 9 bytes
    // Reference: GSM_RR_Types.ttcn: GsmTmsi = uint32_t (raw, NOT LV!)
    return 1 + mTMSIs.size() * 4;
}

ParseResult<void> L3PagingRequestType2::try_writeBody(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, channelNeededCode(mChannelsNeeded[1]), 2);
    dest.writeField(wp, channelNeededCode(mChannelsNeeded[0]), 2);
    dest.writeField(wp, 0x0, 4);
    // Raw GsmTmsi values (4 bytes each, MSB first)
    for (const auto& tmsi : mTMSIs) {
        dest.writeField(wp, tmsi, 32);
    }
    return ParseResult<void>();
}

ParseResult<void> L3PagingRequestType2::try_parseBody(const L3Frame& src, size_t& rp) {
    mChannelsNeeded[1] = channelNeededType(src.readField(rp, 2));
    mChannelsNeeded[0] = channelNeededType(src.readField(rp, 2));
    src.readField(rp, 4);  // page mode
    mTMSIs.clear();
    // Two raw GsmTmsi (uint32_t each, NOT LV-prefixed!)
    // Reference: GSM_RR_Types.ttcn PagingRequestType2 { GsmTmsi mi1, GsmTmsi mi2 }
    for (int i = 0; i < 2; ++i) {
        if (rp + 32 <= src.size()) {
            uint32_t tmsi = static_cast<uint32_t>(src.readField(rp, 32));
            mTMSIs.push_back(tmsi);
        }
    }
    return ParseResult<void>();
}

void L3PagingRequestType2::text(std::ostream& os) const {
    os << "PagingRequestType2: ";
    for (const auto& tmsi : mTMSIs) {
        os << "TMSI=0x" << std::hex << tmsi << std::dec;
    }
}

// ── L3PagingRequestType3 ───────────────────────────────────────────────

L3PagingRequestType3::L3PagingRequestType3() {
    mTMSIs.push_back(0);
    mTMSIs.push_back(0);
    mTMSIs.push_back(0);
    mTMSIs.push_back(0);
    mChannelsNeeded[0] = ChannelType::AnyDCCHType;
    mChannelsNeeded[1] = ChannelType::AnyDCCHType;
}

// ── L3PagingRequestType3 Builder ───────────────────────────────────────

L3PagingRequestType3::Builder L3PagingRequestType3::builder() { return Builder{}; }

L3PagingRequestType3::Builder& L3PagingRequestType3::Builder::addTMSI(uint32_t tmsi, ChannelType type) {
    mTMSIs.push_back(tmsi);
    if (mTMSIs.size() <= 2) {
        mChannelsNeeded[mTMSIs.size() - 1] = type;
    }
    return *this;
}

L3PagingRequestType3 L3PagingRequestType3::Builder::build() {
    L3PagingRequestType3 msg;
    if (mTMSIs.empty()) {
        for (int i = 0; i < 4; i++) msg.mTMSIs.push_back(0);
    } else {
        msg.mTMSIs = std::move(mTMSIs);
        while (msg.mTMSIs.size() < 4) msg.mTMSIs.push_back(0);
    }
    msg.mChannelsNeeded = mChannelsNeeded;
    return msg;
}

size_t L3PagingRequestType3::l2BodyLength() const {
    // ChannelNeeded(4)|PageMode(4) + 4x GsmTmsi(4 each) = 17 bytes
    // Reference: GSM_RR_Types.ttcn: GsmTmsi4 = record length(4) of GsmTmsi (raw!)
    return 1 + mTMSIs.size() * 4;
}

ParseResult<void> L3PagingRequestType3::try_writeBody(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, channelNeededCode(mChannelsNeeded[1]), 2);
    dest.writeField(wp, channelNeededCode(mChannelsNeeded[0]), 2);
    dest.writeField(wp, 0x0, 4);
    // Raw GsmTmsi values (4 bytes each, MSB first)
    for (const auto& tmsi : mTMSIs) {
        dest.writeField(wp, tmsi, 32);
    }
    return ParseResult<void>();
}

ParseResult<void> L3PagingRequestType3::try_parseBody(const L3Frame& src, size_t& rp) {
    mChannelsNeeded[1] = channelNeededType(src.readField(rp, 2));
    mChannelsNeeded[0] = channelNeededType(src.readField(rp, 2));
    src.readField(rp, 4);  // page mode
    mTMSIs.clear();
    // Four raw GsmTmsi (uint32_t each, NOT LV-prefixed!)
    // Reference: GSM_RR_Types.ttcn PagingRequestType3 { GsmTmsi4 mi }
    for (int i = 0; i < 4; ++i) {
        if (rp + 32 <= src.size()) {
            uint32_t tmsi = static_cast<uint32_t>(src.readField(rp, 32));
            mTMSIs.push_back(tmsi);
        }
    }
    return ParseResult<void>();
}

void L3PagingRequestType3::text(std::ostream& os) const {
    os << "PagingRequestType3: ";
    for (const auto& tmsi : mTMSIs) {
        os << "TMSI=0x" << std::hex << tmsi << std::dec;
    }
}

// ── L3PhysicalInformation ──────────────────────────────────────────────

L3PhysicalInformation::L3PhysicalInformation() {}

ParseResult<void> L3PhysicalInformation::try_parseBody(const L3Frame& src, size_t& rp) {
    { auto res = mTA.try_parseV(src, rp); if (!res.has_value()) return res; }
    return ParseResult<void>();
}

ParseResult<void> L3PhysicalInformation::try_writeBody(L3Frame& dest, size_t& wp) const {
    mTA.writeV(dest, wp);
    return ParseResult<void>();
}

void L3PhysicalInformation::text(std::ostream& os) const {
    os << "PhysicalInformation: ";
    mTA.text(os);
}

// ── L3HandoverCommand ──────────────────────────────────────────────────

L3HandoverCommand::L3HandoverCommand() {}

size_t L3HandoverCommand::l2BodyLength() const {
    return mCellDescription.lengthV() +
           mChannelDescriptionAfter.lengthV() +
           mHandoverReference.lengthV() +
           mPowerCommandAccessType.lengthV() +
           mSynchronizationIndication.lengthV();
}

ParseResult<void> L3HandoverCommand::try_parseBody(const L3Frame& src, size_t& rp) {
    { auto res = mCellDescription.try_parseV(src, rp); if (!res.has_value()) return res; }
    { auto res = mChannelDescriptionAfter.try_parseV(src, rp); if (!res.has_value()) return res; }
    { auto res = mHandoverReference.try_parseV(src, rp); if (!res.has_value()) return res; }
    { auto res = mPowerCommandAccessType.try_parseV(src, rp); if (!res.has_value()) return res; }
    { auto res = mSynchronizationIndication.try_parseV(src, rp); if (!res.has_value()) return res; }
    return ParseResult<void>();
}

ParseResult<void> L3HandoverCommand::try_writeBody(L3Frame& dest, size_t& wp) const {
    mCellDescription.writeV(dest, wp);
    mChannelDescriptionAfter.writeV(dest, wp);
    mHandoverReference.writeV(dest, wp);
    mPowerCommandAccessType.writeV(dest, wp);
    mSynchronizationIndication.writeV(dest, wp);
    return ParseResult<void>();
}

void L3HandoverCommand::text(std::ostream& os) const {
    os << "HandoverCommand: ";
    mCellDescription.text(os);
    os << " ";
    mChannelDescriptionAfter.text(os);
    os << " HORef=";
    mHandoverReference.text(os);
    os << " SyncInd=";
    mSynchronizationIndication.text(os);
}

// ── L3HandoverCommand Builder ──────────────────────────────────────────

L3HandoverCommand::Builder L3HandoverCommand::builder() { return Builder{}; }

L3HandoverCommand::Builder& L3HandoverCommand::Builder::cellDescription(const L3CellDescription& cd) {
    mCellDescription = cd;
    return *this;
}

L3HandoverCommand::Builder& L3HandoverCommand::Builder::channelDescriptionAfter(const L3ChannelDescription2& cda) {
    mChannelDescriptionAfter = cda;
    return *this;
}

L3HandoverCommand::Builder& L3HandoverCommand::Builder::handoverReference(const L3HandoverReference& hr) {
    mHandoverReference = hr;
    return *this;
}

L3HandoverCommand::Builder& L3HandoverCommand::Builder::powerCommandAccessType(const L3PowerCommandAndAccessType& pcat) {
    mPowerCommandAccessType = pcat;
    return *this;
}

L3HandoverCommand::Builder& L3HandoverCommand::Builder::syncIndication(const L3SynchronizationIndication& si) {
    mSynchronizationIndication = si;
    return *this;
}

L3HandoverCommand L3HandoverCommand::Builder::build() {
    L3HandoverCommand msg;
    msg.mCellDescription = mCellDescription;
    msg.mChannelDescriptionAfter = mChannelDescriptionAfter;
    msg.mHandoverReference = mHandoverReference;
    msg.mPowerCommandAccessType = mPowerCommandAccessType;
    msg.mSynchronizationIndication = mSynchronizationIndication;
    return msg;
}

// ── L3AdditionalAssignment ─────────────────────────────────────────────

L3AdditionalAssignment::L3AdditionalAssignment()
    : mHavePowerCommand(false) {}

size_t L3AdditionalAssignment::l2BodyLength() const {
    size_t len = mAdditionalChannel.lengthV();
    if (mHavePowerCommand) len += mPowerCommand.lengthV();
    return len;
}

ParseResult<void> L3AdditionalAssignment::try_parseBody(const L3Frame& src, size_t& rp) {
    { auto res = mAdditionalChannel.try_parseV(src, rp); if (!res.has_value()) return res; }
    if (rp + 8 <= src.size()) {
        mHavePowerCommand = true;
        { auto res = mPowerCommand.try_parseV(src, rp); if (!res.has_value()) return res; }
    }
    return ParseResult<void>();
}

ParseResult<void> L3AdditionalAssignment::try_writeBody(L3Frame& dest, size_t& wp) const {
    mAdditionalChannel.writeV(dest, wp);
    if (mHavePowerCommand) {
        mPowerCommand.writeV(dest, wp);
    }
    return ParseResult<void>();
}

void L3AdditionalAssignment::text(std::ostream& os) const {
    os << "AdditionalAssignment: ";
    mAdditionalChannel.text(os);
    if (mHavePowerCommand) {
        os << " ";
        mPowerCommand.text(os);
    }
}

// ── L3SystemInformationType2 ───────────────────────────────────────────

L3SystemInformationType2::L3SystemInformationType2() {}

ParseResult<void> L3SystemInformationType2::try_parseBody(const L3Frame& src, size_t& rp) {
    { auto res = mBCCHFrequencyList.try_parseV(src, rp); if (!res.has_value()) return res; }
    { auto res = mNCCPermitted.try_parseV(src, rp); if (!res.has_value()) return res; }
    { auto res = mRACHControlParameters.try_parseV(src, rp); if (!res.has_value()) return res; }
    return ParseResult<void>();
}

ParseResult<void> L3SystemInformationType2::try_writeBody(L3Frame& dest, size_t& wp) const {
    mBCCHFrequencyList.writeV(dest, wp);
    mNCCPermitted.writeV(dest, wp);
    mRACHControlParameters.writeV(dest, wp);
    return ParseResult<void>();
}

void L3SystemInformationType2::text(std::ostream& os) const {
    os << "SystemInformationType2: ";
    mBCCHFrequencyList.text(os);
    os << " ";
    mNCCPermitted.text(os);
    os << " ";
    mRACHControlParameters.text(os);
}

// ── L3SystemInformationType2bis ────────────────────────────────────────

L3SystemInformationType2bis::L3SystemInformationType2bis() {}

ParseResult<void> L3SystemInformationType2bis::try_parseBody(const L3Frame& src, size_t& rp) {
    { auto res = mBCCHFrequencyList.try_parseV(src, rp); if (!res.has_value()) return res; }
    { auto res = mRACHControlParameters.try_parseV(src, rp); if (!res.has_value()) return res; }
    return ParseResult<void>();
}

ParseResult<void> L3SystemInformationType2bis::try_writeBody(L3Frame& dest, size_t& wp) const {
    mBCCHFrequencyList.writeV(dest, wp);
    mRACHControlParameters.writeV(dest, wp);
    return ParseResult<void>();
}

void L3SystemInformationType2bis::text(std::ostream& os) const {
    os << "SystemInformationType2bis: ";
    mBCCHFrequencyList.text(os);
    os << " ";
    mRACHControlParameters.text(os);
}

// ── L3SystemInformationType2ter ────────────────────────────────────────

L3SystemInformationType2ter::L3SystemInformationType2ter() {}

ParseResult<void> L3SystemInformationType2ter::try_parseBody(const L3Frame& src, size_t& rp) {
    { auto res = mBCCHFrequencyList.try_parseV(src, rp); if (!res.has_value()) return res; }
    return ParseResult<void>();
}

ParseResult<void> L3SystemInformationType2ter::try_writeBody(L3Frame& dest, size_t& wp) const {
    mBCCHFrequencyList.writeV(dest, wp);
    return ParseResult<void>();
}

void L3SystemInformationType2ter::text(std::ostream& os) const {
    os << "SystemInformationType2ter: ";
    mBCCHFrequencyList.text(os);
}

// ── L3SystemInformationType4 ───────────────────────────────────────────

L3SystemInformationType4::L3SystemInformationType4() : mHaveCBCH(false) {}

size_t L3SystemInformationType4::l2BodyLength() const {
    // LAI(5) + CellSelectionParameters(2) + RACHControlParameters(3) = 10
    // + optional CBCH Channel Description (TV, IEI=0x64, 4 bytes)
    size_t len = mLAI.lengthV() + mCellSelectionParameters.lengthV() + mRACHControlParameters.lengthV();
    if (mHaveCBCH) len += mCBCHChannelDescription.lengthTV();
    return len;
}

size_t L3SystemInformationType4::restOctetsLength() const {
    return mRestOctets.lengthV();
}

ParseResult<void> L3SystemInformationType4::try_parseBody(const L3Frame& src, size_t& rp) {
    { auto res = mLAI.try_parseV(src, rp); if (!res.has_value()) return res; }
    { auto res = mCellSelectionParameters.try_parseV(src, rp); if (!res.has_value()) return res; }
    { auto res = mRACHControlParameters.try_parseV(src, rp); if (!res.has_value()) return res; }
    // Optional CBCH Channel Description (TV, IEI=0x64)
    if (rp + 8 <= src.size() && src.peekField(rp, 8) == 0x64) {
        mHaveCBCH = true;
        { auto res = mCBCHChannelDescription.try_parseTV(0x64, src, rp); if (!res.has_value()) return res; }
    }
    { auto res = mRestOctets.try_parseV(src, rp); if (!res.has_value()) return res; }
    return ParseResult<void>();
}

ParseResult<void> L3SystemInformationType4::try_writeBody(L3Frame& dest, size_t& wp) const {
    mLAI.writeV(dest, wp);
    mCellSelectionParameters.writeV(dest, wp);
    mRACHControlParameters.writeV(dest, wp);
    if (mHaveCBCH) {
        mCBCHChannelDescription.writeTV(0x64, dest, wp);
    }
    mRestOctets.writeV(dest, wp);
    return ParseResult<void>();
}

void L3SystemInformationType4::text(std::ostream& os) const {
    os << "SystemInformationType4: ";
    mLAI.text(os);
    os << " ";
    mCellSelectionParameters.text(os);
    os << " ";
    mRACHControlParameters.text(os);
}

// ── L3SystemInformationType5 ───────────────────────────────────────────

L3SystemInformationType5::L3SystemInformationType5() {}

ParseResult<void> L3SystemInformationType5::try_parseBody(const L3Frame& src, size_t& rp) {
    { auto res = mBCCHFrequencyList.try_parseV(src, rp); if (!res.has_value()) return res; }
    return ParseResult<void>();
}

ParseResult<void> L3SystemInformationType5::try_writeBody(L3Frame& dest, size_t& wp) const {
    mBCCHFrequencyList.writeV(dest, wp);
    return ParseResult<void>();
}

void L3SystemInformationType5::text(std::ostream& os) const {
    os << "SystemInformationType5: ";
    mBCCHFrequencyList.text(os);
}

// ── L3SystemInformationType5bis ────────────────────────────────────────

L3SystemInformationType5bis::L3SystemInformationType5bis() {}

ParseResult<void> L3SystemInformationType5bis::try_parseBody(const L3Frame& src, size_t& rp) {
    { auto res = mBCCHFrequencyList.try_parseV(src, rp); if (!res.has_value()) return res; }
    return ParseResult<void>();
}

ParseResult<void> L3SystemInformationType5bis::try_writeBody(L3Frame& dest, size_t& wp) const {
    mBCCHFrequencyList.writeV(dest, wp);
    return ParseResult<void>();
}

void L3SystemInformationType5bis::text(std::ostream& os) const {
    os << "SystemInformationType5bis: ";
    mBCCHFrequencyList.text(os);
}

// ── L3SystemInformationType5ter ────────────────────────────────────────

L3SystemInformationType5ter::L3SystemInformationType5ter() {}

ParseResult<void> L3SystemInformationType5ter::try_parseBody(const L3Frame& src, size_t& rp) {
    { auto res = mBCCHFrequencyList.try_parseV(src, rp); if (!res.has_value()) return res; }
    return ParseResult<void>();
}

ParseResult<void> L3SystemInformationType5ter::try_writeBody(L3Frame& dest, size_t& wp) const {
    mBCCHFrequencyList.writeV(dest, wp);
    return ParseResult<void>();
}

void L3SystemInformationType5ter::text(std::ostream& os) const {
    os << "SystemInformationType5ter: ";
    mBCCHFrequencyList.text(os);
}

// ── L3SystemInformationType6 ───────────────────────────────────────────

L3SystemInformationType6::L3SystemInformationType6() {}

ParseResult<void> L3SystemInformationType6::try_parseBody(const L3Frame& src, size_t& rp) {
    { auto res = mCI.try_parseV(src, rp); if (!res.has_value()) return res; }
    { auto res = mLAI.try_parseV(src, rp); if (!res.has_value()) return res; }
    { auto res = mCellOptions.try_parseV(src, rp); if (!res.has_value()) return res; }
    { auto res = mNCCPermitted.try_parseV(src, rp); if (!res.has_value()) return res; }
    return ParseResult<void>();
}

ParseResult<void> L3SystemInformationType6::try_writeBody(L3Frame& dest, size_t& wp) const {
    mCI.writeV(dest, wp);
    mLAI.writeV(dest, wp);
    mCellOptions.writeV(dest, wp);
    mNCCPermitted.writeV(dest, wp);
    return ParseResult<void>();
}

void L3SystemInformationType6::text(std::ostream& os) const {
    os << "SystemInformationType6: ";
    mCI.text(os);
    os << " ";
    mLAI.text(os);
    os << " ";
    mCellOptions.text(os);
    os << " ";
    mNCCPermitted.text(os);
}

// ── L3SystemInformationType7 ───────────────────────────────────────────

L3SystemInformationType7::L3SystemInformationType7() {}

size_t L3SystemInformationType7::l2BodyLength() const {
    size_t len = mRACHControl.lengthTV();
    for (const auto& ch : mCellChannelDescriptions) {
        len += ch.lengthTV();
    }
    return len;
}

ParseResult<void> L3SystemInformationType7::try_parseBody(const L3Frame& src, size_t& rp) {
    { auto res = mRACHControl.try_parseTV(0x28, src, rp); if (!res.has_value()) return res; }
    while (rp + 8 <= src.size() && parseHasT(0x21, src, rp)) {
        L3CellChannelDescription ch;
        { auto res = ch.try_parseTV(0x21, src, rp); if (!res.has_value()) return res; }
        mCellChannelDescriptions.push_back(ch);
    }
    return ParseResult<void>();
}

ParseResult<void> L3SystemInformationType7::try_writeBody(L3Frame& dest, size_t& wp) const {
    mRACHControl.writeTV(0x28, dest, wp);
    for (const auto& ch : mCellChannelDescriptions) {
        ch.writeTV(0x21, dest, wp);
    }
    return ParseResult<void>();
}

void L3SystemInformationType7::text(std::ostream& os) const {
    os << "SystemInformationType7: ";
    mRACHControl.text(os);
    os << " cells=" << mCellChannelDescriptions.size();
}

// ── L3SystemInformationType8 ───────────────────────────────────────────

L3SystemInformationType8::L3SystemInformationType8() {}

size_t L3SystemInformationType8::l2BodyLength() const {
    size_t len = mNCCPermitted.lengthTV() + mRACHControl.lengthTV();
    for (const auto& ch : mCellChannelDescriptions) {
        len += ch.lengthTV();
    }
    return len;
}

ParseResult<void> L3SystemInformationType8::try_parseBody(const L3Frame& src, size_t& rp) {
    { auto res = mNCCPermitted.try_parseTV(0x27, src, rp); if (!res.has_value()) return res; }
    { auto res = mRACHControl.try_parseTV(0x28, src, rp); if (!res.has_value()) return res; }
    while (rp + 8 <= src.size() && parseHasT(0x21, src, rp)) {
        L3CellChannelDescription ch;
        { auto res = ch.try_parseTV(0x21, src, rp); if (!res.has_value()) return res; }
        mCellChannelDescriptions.push_back(ch);
    }
    return ParseResult<void>();
}

ParseResult<void> L3SystemInformationType8::try_writeBody(L3Frame& dest, size_t& wp) const {
    mNCCPermitted.writeTV(0x27, dest, wp);
    mRACHControl.writeTV(0x28, dest, wp);
    for (const auto& ch : mCellChannelDescriptions) {
        ch.writeTV(0x21, dest, wp);
    }
    return ParseResult<void>();
}

void L3SystemInformationType8::text(std::ostream& os) const {
    os << "SystemInformationType8: ";
    mNCCPermitted.text(os);
    os << " ";
    mRACHControl.text(os);
    os << " cells=" << mCellChannelDescriptions.size();
}

// ── L3SystemInformationType9 ───────────────────────────────────────────

L3SystemInformationType9::L3SystemInformationType9() {}

ParseResult<void> L3SystemInformationType9::try_parseBody(const L3Frame& src, size_t& rp) {
    { auto res = mCI.try_parseV(src, rp); if (!res.has_value()) return res; }
    { auto res = mCellSelectionParameters.try_parseV(src, rp); if (!res.has_value()) return res; }
    { auto res = mCellOptions.try_parseV(src, rp); if (!res.has_value()) return res; }
    return ParseResult<void>();
}

ParseResult<void> L3SystemInformationType9::try_writeBody(L3Frame& dest, size_t& wp) const {
    mCI.writeV(dest, wp);
    mCellSelectionParameters.writeV(dest, wp);
    mCellOptions.writeV(dest, wp);
    return ParseResult<void>();
}

void L3SystemInformationType9::text(std::ostream& os) const {
    os << "SystemInformationType9: ";
    mCI.text(os);
    os << " ";
    mCellSelectionParameters.text(os);
    os << " ";
    mCellOptions.text(os);
}

// ── L3SystemInformationType16 ──────────────────────────────────────────

L3SystemInformationType16::L3SystemInformationType16() {}

ParseResult<void> L3SystemInformationType16::try_parseBody(const L3Frame& src, size_t& rp) {
    { auto res = mCI.try_parseV(src, rp); if (!res.has_value()) return res; }
    { auto res = mCellSelectionParameters.try_parseV(src, rp); if (!res.has_value()) return res; }
    { auto res = mCellOptions.try_parseV(src, rp); if (!res.has_value()) return res; }
    return ParseResult<void>();
}

ParseResult<void> L3SystemInformationType16::try_writeBody(L3Frame& dest, size_t& wp) const {
    mCI.writeV(dest, wp);
    mCellSelectionParameters.writeV(dest, wp);
    mCellOptions.writeV(dest, wp);
    return ParseResult<void>();
}

void L3SystemInformationType16::text(std::ostream& os) const {
    os << "SystemInformationType16: ";
    mCI.text(os);
    os << " ";
    mCellSelectionParameters.text(os);
    os << " ";
    mCellOptions.text(os);
}

// ── L3SystemInformationType17 ──────────────────────────────────────────

L3SystemInformationType17::L3SystemInformationType17() {}

size_t L3SystemInformationType17::l2BodyLength() const {
    size_t len = mRACHControl.lengthTV();
    for (const auto& ch : mCellChannelDescriptions) {
        len += ch.lengthTV();
    }
    return len;
}

ParseResult<void> L3SystemInformationType17::try_parseBody(const L3Frame& src, size_t& rp) {
    { auto res = mRACHControl.try_parseTV(0x28, src, rp); if (!res.has_value()) return res; }
    while (rp + 8 <= src.size() && parseHasT(0x21, src, rp)) {
        L3CellChannelDescription ch;
        { auto res = ch.try_parseTV(0x21, src, rp); if (!res.has_value()) return res; }
        mCellChannelDescriptions.push_back(ch);
    }
    return ParseResult<void>();
}

ParseResult<void> L3SystemInformationType17::try_writeBody(L3Frame& dest, size_t& wp) const {
    mRACHControl.writeTV(0x28, dest, wp);
    for (const auto& ch : mCellChannelDescriptions) {
        ch.writeTV(0x21, dest, wp);
    }
    return ParseResult<void>();
}

void L3SystemInformationType17::text(std::ostream& os) const {
    os << "SystemInformationType17: ";
    mRACHControl.text(os);
    os << " cells=" << mCellChannelDescriptions.size();
}

// ── L3SynchronizationChannelInformation ────────────────────────────────

L3SynchronizationChannelInformation::L3SynchronizationChannelInformation() {}

ParseResult<void> L3SynchronizationChannelInformation::try_parseBody(const L3Frame& src, size_t& rp) {
    { auto res = mCellIdentity.try_parseV(src, rp); if (!res.has_value()) return res; }
    { auto res = mLocationAreaIdentity.try_parseV(src, rp); if (!res.has_value()) return res; }
    return ParseResult<void>();
}

ParseResult<void> L3SynchronizationChannelInformation::try_writeBody(L3Frame& dest, size_t& wp) const {
    mCellIdentity.writeV(dest, wp);
    mLocationAreaIdentity.writeV(dest, wp);
    return ParseResult<void>();
}

void L3SynchronizationChannelInformation::text(std::ostream& os) const {
    os << "SynchronizationChannelInformation: ";
    mCellIdentity.text(os);
    os << " ";
    mLocationAreaIdentity.text(os);
}

// ── L3ChannelRequest ───────────────────────────────────────────────────

L3ChannelRequest::L3ChannelRequest(unsigned wRef)
    : mRequestReference(wRef) {}

ParseResult<void> L3ChannelRequest::try_parseBody(const L3Frame& src, size_t& rp) {
    mRequestReference = src.readField(rp, 4);
    src.readField(rp, 4);
    return ParseResult<void>();
}

ParseResult<void> L3ChannelRequest::try_writeBody(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, mRequestReference, 4);
    dest.writeField(wp, 0, 4);
    return ParseResult<void>();
}

void L3ChannelRequest::text(std::ostream& os) const {
    os << "ChannelRequest: RR=" << mRequestReference;
}

// ── L3HandoverAccess ───────────────────────────────────────────────────

L3HandoverAccess::L3HandoverAccess(unsigned wNumber)
    : mHandoverNumber(wNumber) {}

ParseResult<void> L3HandoverAccess::try_parseBody(const L3Frame& src, size_t& rp) {
    mHandoverNumber = src.readField(rp, 27);
    src.readField(rp, 5);
    return ParseResult<void>();
}

ParseResult<void> L3HandoverAccess::try_writeBody(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, mHandoverNumber, 27);
    dest.writeField(wp, 0, 5);
    return ParseResult<void>();
}

void L3HandoverAccess::text(std::ostream& os) const {
    os << "HandoverAccess: handoverNumber=" << mHandoverNumber;
}

// ── Factory & Parser (internal) ────────────────────────────────────────

namespace detail {

ParseResult<std::unique_ptr<L3RRMessage>> L3RRFactory(int mti) {
    switch (mti) {
        case L3RRMessage::PagingRequestType1:          return std::make_unique<L3PagingRequestType1>();
        case L3RRMessage::PagingRequestType2:          return std::make_unique<L3PagingRequestType2>();
        case L3RRMessage::PagingRequestType3:          return std::make_unique<L3PagingRequestType3>();
        case L3RRMessage::PagingResponse:              return std::make_unique<L3PagingResponse>();
        case L3RRMessage::SystemInformationType1:      return std::make_unique<L3SystemInformationType1>();
        case L3RRMessage::SystemInformationType2:      return std::make_unique<L3SystemInformationType2>();
        case L3RRMessage::SystemInformationType2bis:   return std::make_unique<L3SystemInformationType2bis>();
        case L3RRMessage::SystemInformationType2ter:   return std::make_unique<L3SystemInformationType2ter>();
        case L3RRMessage::SystemInformationType3:      return std::make_unique<L3SystemInformationType3>();
        case L3RRMessage::SystemInformationType4:      return std::make_unique<L3SystemInformationType4>();
        case L3RRMessage::SystemInformationType5:      return std::make_unique<L3SystemInformationType5>();
        case L3RRMessage::SystemInformationType5bis:   return std::make_unique<L3SystemInformationType5bis>();
        case L3RRMessage::SystemInformationType5ter:   return std::make_unique<L3SystemInformationType5ter>();
        case L3RRMessage::SystemInformationType6:      return std::make_unique<L3SystemInformationType6>();
        case L3RRMessage::SystemInformationType7:      return std::make_unique<L3SystemInformationType7>();
        case L3RRMessage::SystemInformationType8:      return std::make_unique<L3SystemInformationType8>();
        case L3RRMessage::SystemInformationType9:      return std::make_unique<L3SystemInformationType9>();
        case L3RRMessage::SystemInformationType13:     return std::make_unique<L3SystemInformationType13>();
        case L3RRMessage::SystemInformationType16:     return std::make_unique<L3SystemInformationType16>();
        case L3RRMessage::SystemInformationType17:     return std::make_unique<L3SystemInformationType17>();
        case L3RRMessage::ChannelRelease:              return std::make_unique<L3ChannelRelease>();
        case L3RRMessage::ImmediateAssignment:         return std::make_unique<L3ImmediateAssignment>();
        case L3RRMessage::ImmediateAssignmentExtended: return std::make_unique<L3ImmediateAssignmentExtended>();
        case L3RRMessage::ImmediateAssignmentReject:   return std::make_unique<L3ImmediateAssignmentReject>();
        case L3RRMessage::AdditionalAssignment:        return std::make_unique<L3AdditionalAssignment>();
        case L3RRMessage::PhysicalInformation:         return std::make_unique<L3PhysicalInformation>();
        case L3RRMessage::HandoverCommand:             return std::make_unique<L3HandoverCommand>();
        case L3RRMessage::HandoverComplete:            return std::make_unique<L3HandoverComplete>();
        case L3RRMessage::HandoverFailure:             return std::make_unique<L3HandoverFailure>();
        case L3RRMessage::AssignmentCommand:           return std::make_unique<L3AssignmentCommand>();
        case L3RRMessage::AssignmentComplete:          return std::make_unique<L3AssignmentComplete>();
        case L3RRMessage::AssignmentFailure:           return std::make_unique<L3AssignmentFailure>();
        case L3RRMessage::ClassmarkEnquiry:            return std::make_unique<L3ClassmarkEnquiry>();
        case L3RRMessage::ClassmarkChange:             return std::make_unique<L3ClassmarkChange>();
        case L3RRMessage::MeasurementReport:           return std::make_unique<L3MeasurementReport>();
        case L3RRMessage::CipheringModeCommand:        return std::make_unique<L3CipheringModeCommand>(false, 0);
        case L3RRMessage::CipheringModeComplete:       return std::make_unique<L3CipheringModeComplete>();
        case L3RRMessage::ChannelModeModify:           return std::make_unique<L3ChannelModeModify>();
        case L3RRMessage::ChannelModeModifyAcknowledge: return std::make_unique<L3ChannelModeModifyAcknowledge>();
        case L3RRMessage::GPRSSuspensionRequest:       return std::make_unique<L3GPRSSuspensionRequest>();
        case L3RRMessage::ApplicationInformation:      return std::make_unique<L3ApplicationInformation>();
        case L3RRMessage::RRStatus:                    return std::make_unique<L3RRStatus>();
        case L3RRMessage::SynchronizationChannelInformation: return std::make_unique<L3SynchronizationChannelInformation>();
        case L3RRMessage::ChannelRequest:              return std::make_unique<L3ChannelRequest>();
        case L3RRMessage::HandoverAccess:              return std::make_unique<L3HandoverAccess>();
        default:
            return ParseResult<std::unique_ptr<L3RRMessage>>(
                ParseErrorCode::InvalidMTI, "Unknown RR message type: 0x" + std::to_string(mti & 0xFF));
    }
}

ParseResult<std::unique_ptr<L3RRMessage>> parseL3RR(const L3Frame& source) {
    if (source.size() < 16) {
        return ParseResult<std::unique_ptr<L3RRMessage>>(
            ParseErrorCode::TruncatedInput, "Frame too short for L3 header");
    }

    unsigned mti = source.mti();
    auto factoryResult = L3RRFactory(static_cast<L3RRMessage::MessageType>(mti));
    if (!factoryResult.has_value()) {
        GSML3PARSER_LOG_WARN("Unknown RR MTI: 0x%02x", mti);
        return ParseResult<std::unique_ptr<L3RRMessage>>(factoryResult.error());
    }

    auto parseResult = factoryResult.value()->parse(source);
    if (!parseResult.has_value()) {
        GSML3PARSER_LOG_WARN("RR parse failed for MTI=0x%02x", mti);
        return ParseResult<std::unique_ptr<L3RRMessage>>(parseResult.error());
    }

    return ParseResult<std::unique_ptr<L3RRMessage>>(std::move(factoryResult).value());
}

} // namespace detail

} // namespace gsml3parser
