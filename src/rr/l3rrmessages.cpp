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
    os << " (" << name(static_cast<MessageType>(MTI())) << ")";
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

L3PagingRequestType1::L3PagingRequestType1(const L3MobileIdentity& wId, ChannelType wType) {
    mMobileIDs.push_back(wId);
    mChannelsNeeded[0] = wType;
    mChannelsNeeded[1] = ChannelType::AnyDCCHType;
}

L3PagingRequestType1::L3PagingRequestType1(const L3MobileIdentity& wId1, ChannelType wType1,
                                           const L3MobileIdentity& wId2, ChannelType wType2) {
    mMobileIDs.push_back(wId1);
    mChannelsNeeded[0] = wType1;
    mMobileIDs.push_back(wId2);
    mChannelsNeeded[1] = wType2;
}

size_t L3PagingRequestType1::l2BodyLength() const {
    int sz = static_cast<int>(mMobileIDs.size());
    size_t len = 1;
    len += mMobileIDs[0].lengthLV();
    if (sz > 1) len += mMobileIDs[1].lengthTLV();
    return len;
}

static unsigned channelNeededCode(ChannelType wType) {
    switch (wType) {
        case ChannelType::AnyDCCHType: return 0;
        case ChannelType::SDCCHType: return 1;
        case ChannelType::TCHFType: return 2;
        case ChannelType::AnyTCHType: return 3;
        default: return 0;
    }
}

static ChannelType channelNeededType(unsigned code) {
    switch (code) {
        case 0: return ChannelType::AnyDCCHType;
        case 1: return ChannelType::SDCCHType;
        case 2: return ChannelType::TCHFType;
        case 3: return ChannelType::AnyTCHType;
        default: return ChannelType::AnyDCCHType;
    }
}

void L3PagingRequestType1::writeBody(L3Frame& dest, size_t& wp) const {
    // GSM 04.08 9.1.22: reverse order for half-octet fields
    int sz = static_cast<int>(mMobileIDs.size());
    dest.writeField(wp, channelNeededCode(mChannelsNeeded[sz > 1 ? 1 : 0]), 2);
    dest.writeField(wp, channelNeededCode(mChannelsNeeded[0]), 2);
    dest.writeField(wp, 0x0, 4);  // page mode: normal paging
    mMobileIDs[0].writeLV(dest, wp);
    if (sz > 1) mMobileIDs[1].writeTLV(0x17, dest, wp);
}

void L3PagingRequestType1::parseBody(const L3Frame& src, size_t& rp) {
    mChannelsNeeded[1] = channelNeededType(src.readField(rp, 2));
    mChannelsNeeded[0] = channelNeededType(src.readField(rp, 2));
    src.readField(rp, 4);  // page mode
    mMobileIDs.clear();
    L3MobileIdentity id;
    id.parseLV(src, rp);
    mMobileIDs.push_back(id);
    // Second mobile identity is TLV with IEI=0x17
    if (rp + 16 <= src.size() && src.peekField(rp, 8) == 0x17) {
        rp += 8;  // skip IEI
        L3MobileIdentity id2;
        id2.parseLV(src, rp);
        mMobileIDs.push_back(id2);
    }
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

void L3PagingResponse::parseBody(const L3Frame& source, size_t& rp) {
    rp += 8;  // skip cipher key seq # (4) and spare (4)
    mClassmark.parseLV(source, rp);
    mMobileID.parseLV(source, rp);
}

void L3PagingResponse::writeBody(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, 0, 8);  // spare cipher key seq # + spare
    mClassmark.writeLV(dest, wp);
    mMobileID.writeLV(dest, wp);
}

void L3PagingResponse::text(std::ostream& os) const {
    os << "PagingResponse: ";
    mMobileID.text(os);
    os << " ";
    mClassmark.text(os);
}

// ── L3SystemInformationType1 ────────────────────────────────────────────

L3SystemInformationType1::L3SystemInformationType1() {}

void L3SystemInformationType1::parseBody(const L3Frame& src, size_t& rp) {
    mCellChannelDescription.parseV(src, rp);
    mRACHControlParameters.parseV(src, rp);
}

void L3SystemInformationType1::writeBody(L3Frame& dest, size_t& wp) const {
    mCellChannelDescription.writeV(dest, wp);
    mRACHControlParameters.writeV(dest, wp);
}

void L3SystemInformationType1::text(std::ostream& os) const {
    os << "SystemInformationType1: ";
    mCellChannelDescription.text(os);
    os << " ";
    mRACHControlParameters.text(os);
}

// ── L3ChannelRelease ───────────────────────────────────────────────────

void L3ChannelRelease::parseBody(const L3Frame& src, size_t& rp) {
    mCause = static_cast<RRCause>(src.readField(rp, 8));
    // Optional GPRS Resumption IEI=0x01, 1 bit
    if (rp + 8 <= src.size() && src.peekField(rp, 8) == 0x01) {
        rp += 8; // skip IEI
        mGprsResumptionPresent = true;
        mGprsResumptionBit = src.readField(rp, 1);
        src.readField(rp, 7); // spare
    }
}

void L3ChannelRelease::writeBody(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, static_cast<unsigned>(mCause), 8);
    if (mGprsResumptionPresent) {
        dest.writeField(wp, 0x01, 8); // IEI
        dest.writeField(wp, mGprsResumptionBit ? 1 : 0, 1);
        dest.writeField(wp, 0, 7); // spare
    }
}

void L3ChannelRelease::text(std::ostream& os) const {
    os << "ChannelRelease: cause=" << RRCause2Str(mCause);
    if (mGprsResumptionPresent) {
        os << " gprsResumption=" << (mGprsResumptionBit ? "on" : "off");
    }
}

// ── L3RRStatus ──────────────────────────────────────────────────────────

void L3RRStatus::parseBody(const L3Frame& src, size_t& rp) {
    mCause = static_cast<RRCause>(src.readField(rp, 8));
}

void L3RRStatus::writeBody(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, static_cast<unsigned>(mCause), 8);
}

void L3RRStatus::text(std::ostream& os) const {
    os << "RRStatus: cause=" << RRCause2Str(mCause);
}

// ── L3AssignmentCommand ─────────────────────────────────────────────────

L3AssignmentCommand::L3AssignmentCommand()
    : mHaveMode1(false) {}

size_t L3AssignmentCommand::l2BodyLength() const {
    size_t len = mChannel.lengthV() + mPowerCommand.lengthV();
    if (mHaveMode1) len += mMode1.lengthV();
    if (isAMR()) len += mMultiRate.lengthTLV();
    return len;
}

void L3AssignmentCommand::parseBody(const L3Frame& src, size_t& rp) {
    mChannel.parseV(src, rp);
    mPowerCommand.parseV(src, rp);
    // Optional Mode 1
    if (rp + 8 <= src.size() && (src.peekField(rp, 8) & 0xf8) == 0x08) {
        mHaveMode1 = true;
        mMode1.parseV(src, rp);
        // Optional Multi Rate Configuration for AMR
        if (isAMR() && rp + 16 <= src.size() && src.peekField(rp, 8) == 0x15) {
            mMultiRate.parseTLV(0x15, src, rp);
        }
    }
}

void L3AssignmentCommand::writeBody(L3Frame& dest, size_t& wp) const {
    mChannel.writeV(dest, wp);
    mPowerCommand.writeV(dest, wp);
    if (mHaveMode1) {
        mMode1.writeV(dest, wp);
        if (isAMR()) {
            mMultiRate.writeTLV(0x15, dest, wp);
        }
    }
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

// ── L3ClassmarkEnquiry ──────────────────────────────────────────────────

void L3ClassmarkEnquiry::text(std::ostream& os) const {
    os << "ClassmarkEnquiry";
}

// ── L3AssignmentComplete ───────────────────────────────────────────────

void L3AssignmentComplete::parseBody(const L3Frame& src, size_t& rp) {
    mCause = static_cast<RRCause>(src.readField(rp, 8));
}

void L3AssignmentComplete::writeBody(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, static_cast<unsigned>(mCause), 8);
}

void L3AssignmentComplete::text(std::ostream& os) const {
    os << "AssignmentComplete: cause=" << RRCause2Str(mCause);
}

// ── L3AssignmentFailure ────────────────────────────────────────────────

void L3AssignmentFailure::parseBody(const L3Frame& src, size_t& rp) {
    mCause = static_cast<RRCause>(src.readField(rp, 8));
}

void L3AssignmentFailure::writeBody(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, static_cast<unsigned>(mCause), 8);
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

void L3ClassmarkChange::parseBody(const L3Frame& src, size_t& rp) {
    // GSM 04.08 9.1.11: classmark(LV), additionalClassmark(TLV 0x20)
    mClassmark.parseLV(src, rp);
    mHaveAdditionalClassmark = mAdditionalClassmark.parseTLV(0x20, src, rp);
}

void L3ClassmarkChange::writeBody(L3Frame& dest, size_t& wp) const {
    mClassmark.writeLV(dest, wp);
    if (mHaveAdditionalClassmark) {
        mAdditionalClassmark.writeTLV(0x20, dest, wp);
    }
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

void L3MeasurementReport::parseBody(const L3Frame& src, size_t& rp) {
    mMeasurementResults.parseV(src, rp);
}

void L3MeasurementReport::writeBody(L3Frame& dest, size_t& wp) const {
    mMeasurementResults.writeV(dest, wp);
}

void L3MeasurementReport::text(std::ostream& os) const {
    os << "MeasurementReport: ";
    mMeasurementResults.text(os);
}

// ── L3CipheringModeCommand ─────────────────────────────────────────────

int L3CipheringModeCommand::MTI() const { return CipheringModeCommand; }

void L3CipheringModeCommand::parseBody(const L3Frame& src, size_t& rp) {
    L3CipheringModeSetting cms;
    cms.parseV(src, rp);
    mCiphering = cms.ciphering();
    mAlgorithm = cms.algorithm();
    mCipheringModeResponse.parseV(src, rp);
}

void L3CipheringModeCommand::writeBody(L3Frame& dest, size_t& wp) const {
    L3CipheringModeSetting cms(mCiphering, mAlgorithm);
    cms.writeV(dest, wp);
    mCipheringModeResponse.writeV(dest, wp);
}

void L3CipheringModeCommand::text(std::ostream& os) const {
    os << "CipheringModeCommand: ciphering=" << mCiphering
       << " algorithm=A5/" << mAlgorithm
       << " includeIMEISV=" << mCipheringModeResponse.includeIMEISV();
}

// ── L3CipheringModeComplete ────────────────────────────────────────────

int L3CipheringModeComplete::MTI() const { return CipheringModeComplete; }

void L3CipheringModeComplete::text(std::ostream& os) const {
    os << "CipheringModeComplete";
}

// ── L3HandoverComplete ─────────────────────────────────────────────────

void L3HandoverComplete::parseBody(const L3Frame& src, size_t& rp) {
    mCause = static_cast<RRCause>(src.readField(rp, 8));
}

void L3HandoverComplete::writeBody(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, static_cast<unsigned>(mCause), 8);
}

void L3HandoverComplete::text(std::ostream& os) const {
    os << "HandoverComplete: cause=" << RRCause2Str(mCause);
}

// ── L3HandoverFailure ──────────────────────────────────────────────────

void L3HandoverFailure::parseBody(const L3Frame& src, size_t& rp) {
    mCause = static_cast<RRCause>(src.readField(rp, 8));
}

void L3HandoverFailure::writeBody(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, static_cast<unsigned>(mCause), 8);
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

void L3ChannelModeModify::parseBody(const L3Frame& src, size_t& rp) {
    mDescription.parseV(src, rp);
    mMode.parseV(src, rp);
    if (isAMR()) {
        mMultiRate.parseTLV(0x15, src, rp);
    }
}

void L3ChannelModeModify::writeBody(L3Frame& dest, size_t& wp) const {
    mDescription.writeV(dest, wp);
    mMode.writeV(dest, wp);
    if (isAMR()) {
        mMultiRate.writeTLV(0x15, dest, wp);
    }
}

void L3ChannelModeModify::text(std::ostream& os) const {
    os << "ChannelModeModify: ";
    mDescription.text(os);
    os << " ";
    mMode.text(os);
}

// ── L3ChannelModeModifyAcknowledge ─────────────────────────────────────

void L3ChannelModeModifyAcknowledge::parseBody(const L3Frame& src, size_t& rp) {
    mDescription.parseV(src, rp);
    mMode.parseV(src, rp);
}

size_t L3ChannelModeModifyAcknowledge::l2BodyLength() const {
    return mDescription.lengthV() + mMode.lengthV();
}

void L3ChannelModeModifyAcknowledge::writeBody(L3Frame& dest, size_t& wp) const {
    mDescription.writeV(dest, wp);
    mMode.writeV(dest, wp);
}

void L3ChannelModeModifyAcknowledge::text(std::ostream& os) const {
    os << "ChannelModeModifyAcknowledge: ";
    mDescription.text(os);
    os << " ";
    mMode.text(os);
}

// ── L3GPRSSuspensionRequest ────────────────────────────────────────────

void L3GPRSSuspensionRequest::parseBody(const L3Frame& src, size_t& rp) {
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
}

void L3GPRSSuspensionRequest::writeBody(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, mTLLI, 32);
    for (size_t i = 0; i < 6; ++i) {
        dest.writeField(wp, mRaId[i], 8);
    }
    dest.writeField(wp, mSuspensionCause, 8);
    if (mServiceSupport) {
        dest.writeField(wp, 0x01, 8);  // IEI for service support
        dest.writeField(wp, mServiceSupport, 8);
    }
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
    // APDU ID (4 bits) + APDU Flags (4 bits) + APDU Data (LV)
    size_t dataLen = (mData.size() + 7) / 8;
    return 1 + 1 + dataLen;  // 1 byte for ID+Flags, 1 byte for LV length, data
}

void L3ApplicationInformation::writeBody(L3Frame& dest, size_t& wp) const {
    // Write APDU Flags first (half-octet reverse order), then APDU ID
    dest.writeField(wp, mLastSegment, 1);
    dest.writeField(wp, mFirstSegment, 1);
    dest.writeField(wp, mCR, 1);
    dest.writeField(wp, 0, 1);  // spare
    dest.writeField(wp, mProtocolIdentifier, 4);
    // Write APDU Data as LV
    size_t dataLen = (mData.size() + 7) / 8;
    dest.writeField(wp, dataLen, 8);  // length in bytes
    size_t wp2 = 0;
    for (size_t i = 0; i < mData.size(); ++i) {
        unsigned bit = mData.readField(wp2, 1);
        dest.writeField(wp, bit, 1);
    }
    // Pad to byte boundary
    while (wp % 8 != 0) {
        dest.writeField(wp, 0, 1);
    }
}

void L3ApplicationInformation::parseBody(const L3Frame& src, size_t& rp) {
    // Parse APDU Flags first (half-octet reverse order), then APDU ID
    mLastSegment = src.readField(rp, 1);
    mFirstSegment = src.readField(rp, 1);
    mCR = src.readField(rp, 1);
    src.readField(rp, 1);  // spare
    mProtocolIdentifier = src.readField(rp, 4);
    // Parse APDU Data as LV
    size_t dataLen = src.readField(rp, 8);  // length in bytes
    size_t bitsLen = dataLen * 8;
    mData = BitVector(bitsLen);
    size_t wp2 = 0;
    for (size_t i = 0; i < bitsLen && rp + 1 <= src.size(); ++i) {
        unsigned bit = src.readField(rp, 1);
        mData.writeField(wp2, bit, 1);
    }
}

void L3ApplicationInformation::text(std::ostream& os) const {
    os << "ApplicationInformation: PID=" << mProtocolIdentifier
       << " CR=" << mCR << " first=" << mFirstSegment << " last=" << mLastSegment
       << " len=" << mData.size() << "bits";
}

// ── L3SystemInformationType3 ───────────────────────────────────────────

void L3SystemInformationType3::parseBody(const L3Frame& src, size_t& rp) {
    mCI.parseV(src, rp);
    mLAI.parseV(src, rp);
    mControlChannelDescription.parseV(src, rp);
    mCellOptions.parseV(src, rp);
    mCellSelectionParameters.parseV(src, rp);
    mRACHControlParameters.parseV(src, rp);
    mRestOctets.parseV(src, rp);
}

void L3SystemInformationType3::writeBody(L3Frame& dest, size_t& wp) const {
    mCI.writeV(dest, wp);
    mLAI.writeV(dest, wp);
    mControlChannelDescription.writeV(dest, wp);
    mCellOptions.writeV(dest, wp);
    mCellSelectionParameters.writeV(dest, wp);
    mRACHControlParameters.writeV(dest, wp);
    mRestOctets.writeV(dest, wp);
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

void L3SystemInformationType13::parseBody(const L3Frame& src, size_t& rp) {
    mRestOctets.parseV(src, rp);
}

void L3SystemInformationType13::writeBody(L3Frame& dest, size_t& wp) const {
    mRestOctets.writeV(dest, wp);
}

void L3SystemInformationType13::text(std::ostream& os) const {
    os << "SystemInformationType13: ";
    mRestOctets.text(os);
}

// ── L3ImmediateAssignment ──────────────────────────────────────────────

L3ImmediateAssignment::L3ImmediateAssignment()
    : mDedicatedModeOrTBF(false, false), mStartTimePresent(false), mStartTimeFrame(0) {}

size_t L3ImmediateAssignment::l2BodyLength() const {
    // PageMode(1/2) + DedicatedModeOrTBF(1/2) + RequestRef(3) + ChannelDesc(3) + TimingAdv(1) + MobileAlloc(LV) + StartTime(opt)
    size_t len = 1 + 3 + 3 + 1;
    if (!mMobileAllocation.empty()) len += 1 + mMobileAllocation.size();
    if (mStartTimePresent) len += 3;
    return len;
}

void L3ImmediateAssignment::parseBody(const L3Frame& src, size_t& rp) {
    mPageMode.parseV(src, rp);
    mDedicatedModeOrTBF.parseV(src, rp);
    mRequestReference.parseV(src, rp);
    mChannelDescription.parseV(src, rp);
    mTimingAdvance.parseV(src, rp);
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
    // StartTime: 1 bit flag + 23 bits frame number
    if (rp + 24 <= src.size()) {
        mStartTimePresent = src.readField(rp, 1);
        if (mStartTimePresent) {
            mStartTimeFrame = src.readField(rp, 23);
        } else {
            rp += 7; // spare
        }
    }
}

void L3ImmediateAssignment::writeBody(L3Frame& dest, size_t& wp) const {
    mPageMode.writeV(dest, wp);
    mDedicatedModeOrTBF.writeV(dest, wp);
    mRequestReference.writeV(dest, wp);
    mChannelDescription.writeV(dest, wp);
    mTimingAdvance.writeV(dest, wp);
    // Mobile Allocation: LV format - length byte + data
    dest.writeField(wp, static_cast<uint8_t>(mMobileAllocation.size()), 8);
    for (const auto& b : mMobileAllocation) {
        dest.writeField(wp, b, 8);
    }
    if (mStartTimePresent) {
        dest.writeField(wp, 1, 1);  // start time present
        dest.writeField(wp, mStartTimeFrame, 23);
    } else {
        dest.writeField(wp, 0, 1);
    }
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
    size_t len = 1 + 3 + 3 + 1;
    if (!mMobileAllocation.empty()) len += 1 + mMobileAllocation.size();
    if (mStartTimePresent) len += 3;
    if (mHaveAdditionalChannel) len += mAdditionalChannel.lengthV();
    return len;
}

void L3ImmediateAssignmentExtended::parseBody(const L3Frame& src, size_t& rp) {
    mPageMode.parseV(src, rp);
    mDedicatedModeOrTBF.parseV(src, rp);
    mRequestReference.parseV(src, rp);
    mChannelDescription.parseV(src, rp);
    mTimingAdvance.parseV(src, rp);
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
    // StartTime: 1 bit flag + 23 bits frame number
    if (rp + 24 <= src.size()) {
        mStartTimePresent = src.readField(rp, 1);
        if (mStartTimePresent) {
            mStartTimeFrame = src.readField(rp, 23);
        } else {
            rp += 7; // spare
        }
    }
}

void L3ImmediateAssignmentExtended::writeBody(L3Frame& dest, size_t& wp) const {
    mPageMode.writeV(dest, wp);
    mDedicatedModeOrTBF.writeV(dest, wp);
    mRequestReference.writeV(dest, wp);
    mChannelDescription.writeV(dest, wp);
    mTimingAdvance.writeV(dest, wp);
    // Mobile Allocation: LV format - length byte + data
    dest.writeField(wp, static_cast<uint8_t>(mMobileAllocation.size()), 8);
    for (const auto& b : mMobileAllocation) {
        dest.writeField(wp, b, 8);
    }
    if (mStartTimePresent) {
        dest.writeField(wp, 1, 1);
        dest.writeField(wp, mStartTimeFrame, 23);
    } else {
        dest.writeField(wp, 0, 1);
    }
    if (mHaveAdditionalChannel) {
        mAdditionalChannel.writeV(dest, wp);
    }
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
    : mPageMode(0) {}

L3ImmediateAssignmentReject::L3ImmediateAssignmentReject(unsigned waitSeconds)
    : mPageMode(0) {
    mWaitIndications.resize(4, L3WaitIndication(waitSeconds));
}

size_t L3ImmediateAssignmentReject::l2BodyLength() const {
    return 17;  // Fixed: 1 byte PageMode + 4 * (3 bytes RequestRef + 1 byte WaitInd) = 17
}

void L3ImmediateAssignmentReject::parseBody(const L3Frame& src, size_t& rp) {
    mPageMode.parseV(src, rp);
    // 4 pairs of (RequestReference + WaitIndication)
    for (int i = 0; i < 4; ++i) {
        if (rp + 32 <= src.size()) {
            L3RequestReference rr;
            rr.parseV(src, rp);
            mRequestReferences.push_back(rr);
            L3WaitIndication wi;
            wi.parseV(src, rp);
            mWaitIndications.push_back(wi);
        }
    }
}

void L3ImmediateAssignmentReject::writeBody(L3Frame& dest, size_t& wp) const {
    mPageMode.writeV(dest, wp);
    // Write up to 4 pairs, padding with last entry if fewer
    int count = static_cast<int>(mRequestReferences.size());
    if (count <= 0) count = 1;
    for (int i = 0; i < 4; ++i) {
        int idx = std::min(i, count - 1);
        mRequestReferences[idx].writeV(dest, wp);
        mWaitIndications[idx].writeV(dest, wp);
    }
}

void L3ImmediateAssignmentReject::text(std::ostream& os) const {
    os << "ImmediateAssignmentReject: refs=" << mRequestReferences.size();
    for (size_t i = 0; i < mWaitIndications.size(); ++i) {
        os << " wait[" << i << "]=" << mWaitIndications[i].value() << "s";
    }
}

// ── L3PagingRequestType2 ───────────────────────────────────────────────

L3PagingRequestType2::L3PagingRequestType2() {
    mMobileIDs.emplace_back();
    mChannelsNeeded[0] = ChannelType::AnyDCCHType;
    mChannelsNeeded[1] = ChannelType::AnyDCCHType;
}

L3PagingRequestType2::L3PagingRequestType2(const L3MobileIdentity& wId, ChannelType wType) {
    mMobileIDs.push_back(wId);
    mChannelsNeeded[0] = wType;
    mChannelsNeeded[1] = ChannelType::AnyDCCHType;
}

size_t L3PagingRequestType2::l2BodyLength() const {
    int sz = static_cast<int>(mMobileIDs.size());
    size_t len = 1;
    len += mMobileIDs[0].lengthLV();
    if (sz > 1) len += mMobileIDs[1].lengthTLV();
    return len;
}

void L3PagingRequestType2::writeBody(L3Frame& dest, size_t& wp) const {
    int sz = static_cast<int>(mMobileIDs.size());
    dest.writeField(wp, channelNeededCode(mChannelsNeeded[sz > 1 ? 1 : 0]), 2);
    dest.writeField(wp, channelNeededCode(mChannelsNeeded[0]), 2);
    dest.writeField(wp, 0x0, 4);
    mMobileIDs[0].writeLV(dest, wp);
    if (sz > 1) mMobileIDs[1].writeTLV(0x17, dest, wp);
}

void L3PagingRequestType2::parseBody(const L3Frame& src, size_t& rp) {
    mChannelsNeeded[1] = channelNeededType(src.readField(rp, 2));
    mChannelsNeeded[0] = channelNeededType(src.readField(rp, 2));
    src.readField(rp, 4);
    mMobileIDs.clear();
    L3MobileIdentity id;
    id.parseLV(src, rp);
    mMobileIDs.push_back(id);
    if (rp + 16 <= src.size() && src.peekField(rp, 8) == 0x17) {
        rp += 8;
        L3MobileIdentity id2;
        id2.parseLV(src, rp);
        mMobileIDs.push_back(id2);
    }
}

void L3PagingRequestType2::text(std::ostream& os) const {
    os << "PagingRequestType2: ";
    for (const auto& id : mMobileIDs) {
        id.text(os);
    }
}

// ── L3PagingRequestType3 ───────────────────────────────────────────────

L3PagingRequestType3::L3PagingRequestType3() {
    mMobileIDs.emplace_back();
    mChannelsNeeded[0] = ChannelType::AnyDCCHType;
    mChannelsNeeded[1] = ChannelType::AnyDCCHType;
}

L3PagingRequestType3::L3PagingRequestType3(const L3MobileIdentity& wId, ChannelType wType) {
    mMobileIDs.push_back(wId);
    mChannelsNeeded[0] = wType;
    mChannelsNeeded[1] = ChannelType::AnyDCCHType;
}

size_t L3PagingRequestType3::l2BodyLength() const {
    int sz = static_cast<int>(mMobileIDs.size());
    size_t len = 1;
    len += mMobileIDs[0].lengthLV();
    if (sz > 1) len += mMobileIDs[1].lengthTLV();
    return len;
}

void L3PagingRequestType3::writeBody(L3Frame& dest, size_t& wp) const {
    int sz = static_cast<int>(mMobileIDs.size());
    dest.writeField(wp, channelNeededCode(mChannelsNeeded[sz > 1 ? 1 : 0]), 2);
    dest.writeField(wp, channelNeededCode(mChannelsNeeded[0]), 2);
    dest.writeField(wp, 0x0, 4);
    mMobileIDs[0].writeLV(dest, wp);
    if (sz > 1) mMobileIDs[1].writeTLV(0x17, dest, wp);
}

void L3PagingRequestType3::parseBody(const L3Frame& src, size_t& rp) {
    mChannelsNeeded[1] = channelNeededType(src.readField(rp, 2));
    mChannelsNeeded[0] = channelNeededType(src.readField(rp, 2));
    src.readField(rp, 4);
    mMobileIDs.clear();
    L3MobileIdentity id;
    id.parseLV(src, rp);
    mMobileIDs.push_back(id);
    if (rp + 16 <= src.size() && src.peekField(rp, 8) == 0x17) {
        rp += 8;
        L3MobileIdentity id2;
        id2.parseLV(src, rp);
        mMobileIDs.push_back(id2);
    }
}

void L3PagingRequestType3::text(std::ostream& os) const {
    os << "PagingRequestType3: ";
    for (const auto& id : mMobileIDs) {
        id.text(os);
    }
}

// ── L3PhysicalInformation ──────────────────────────────────────────────

L3PhysicalInformation::L3PhysicalInformation() {}

void L3PhysicalInformation::parseBody(const L3Frame& src, size_t& rp) {
    mTA.parseV(src, rp);
}

void L3PhysicalInformation::writeBody(L3Frame& dest, size_t& wp) const {
    mTA.writeV(dest, wp);
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

void L3HandoverCommand::parseBody(const L3Frame& src, size_t& rp) {
    mCellDescription.parseV(src, rp);
    mChannelDescriptionAfter.parseV(src, rp);
    mHandoverReference.parseV(src, rp);
    mPowerCommandAccessType.parseV(src, rp);
    mSynchronizationIndication.parseV(src, rp);
}

void L3HandoverCommand::writeBody(L3Frame& dest, size_t& wp) const {
    mCellDescription.writeV(dest, wp);
    mChannelDescriptionAfter.writeV(dest, wp);
    mHandoverReference.writeV(dest, wp);
    mPowerCommandAccessType.writeV(dest, wp);
    mSynchronizationIndication.writeV(dest, wp);
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

// ── L3AdditionalAssignment ─────────────────────────────────────────────

L3AdditionalAssignment::L3AdditionalAssignment()
    : mHavePowerCommand(false) {}

size_t L3AdditionalAssignment::l2BodyLength() const {
    size_t len = mAdditionalChannel.lengthV();
    if (mHavePowerCommand) len += mPowerCommand.lengthV();
    return len;
}

void L3AdditionalAssignment::parseBody(const L3Frame& src, size_t& rp) {
    mAdditionalChannel.parseV(src, rp);
    if (rp + 8 <= src.size()) {
        mHavePowerCommand = true;
        mPowerCommand.parseV(src, rp);
    }
}

void L3AdditionalAssignment::writeBody(L3Frame& dest, size_t& wp) const {
    mAdditionalChannel.writeV(dest, wp);
    if (mHavePowerCommand) {
        mPowerCommand.writeV(dest, wp);
    }
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

size_t L3SystemInformationType2::restOctetsLength() const { return 0; }

void L3SystemInformationType2::parseBody(const L3Frame& src, size_t& rp) {
    mBCCHFrequencyList.parseV(src, rp);
    mNCCPermitted.parseV(src, rp);
    mRACHControlParameters.parseV(src, rp);
}

void L3SystemInformationType2::writeBody(L3Frame& dest, size_t& wp) const {
    mBCCHFrequencyList.writeV(dest, wp);
    mNCCPermitted.writeV(dest, wp);
    mRACHControlParameters.writeV(dest, wp);
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

size_t L3SystemInformationType2bis::restOctetsLength() const { return 0; }

void L3SystemInformationType2bis::parseBody(const L3Frame& src, size_t& rp) {
    mBCCHFrequencyList.parseV(src, rp);
    mNCCPermitted.parseV(src, rp);
    mRACHControlParameters.parseV(src, rp);
}

void L3SystemInformationType2bis::writeBody(L3Frame& dest, size_t& wp) const {
    mBCCHFrequencyList.writeV(dest, wp);
    mNCCPermitted.writeV(dest, wp);
    mRACHControlParameters.writeV(dest, wp);
}

void L3SystemInformationType2bis::text(std::ostream& os) const {
    os << "SystemInformationType2bis: ";
    mBCCHFrequencyList.text(os);
    os << " ";
    mNCCPermitted.text(os);
    os << " ";
    mRACHControlParameters.text(os);
}

// ── L3SystemInformationType2ter ────────────────────────────────────────

L3SystemInformationType2ter::L3SystemInformationType2ter() {}

size_t L3SystemInformationType2ter::restOctetsLength() const { return 0; }

void L3SystemInformationType2ter::parseBody(const L3Frame& src, size_t& rp) {
    mBCCHFrequencyList.parseV(src, rp);
    mNCCPermitted.parseV(src, rp);
    mRACHControlParameters.parseV(src, rp);
}

void L3SystemInformationType2ter::writeBody(L3Frame& dest, size_t& wp) const {
    mBCCHFrequencyList.writeV(dest, wp);
    mNCCPermitted.writeV(dest, wp);
    mRACHControlParameters.writeV(dest, wp);
}

void L3SystemInformationType2ter::text(std::ostream& os) const {
    os << "SystemInformationType2ter: ";
    mBCCHFrequencyList.text(os);
    os << " ";
    mNCCPermitted.text(os);
    os << " ";
    mRACHControlParameters.text(os);
}

// ── L3SystemInformationType4 ───────────────────────────────────────────

L3SystemInformationType4::L3SystemInformationType4() : mHaveCBCH(false) {}

size_t L3SystemInformationType4::restOctetsLength() const {
    size_t len = 0;
    if (mHaveCBCH) len += mCBCHChannelDescription.lengthV();
    len += mRestOctets.lengthV();
    return len;
}

void L3SystemInformationType4::parseBody(const L3Frame& src, size_t& rp) {
    mLAI.parseV(src, rp);
    mCI.parseV(src, rp);
    mCellSelectionParameters.parseV(src, rp);
    mCellOptions.parseV(src, rp);
    mRACHControlParameters.parseV(src, rp);
    // Rest octets: optional CBCH Channel Description + SI4 Rest Octets
    if (rp + 8 <= src.size() && (src.peekField(rp, 8) & 0xf0) == 0x20) {
        mHaveCBCH = true;
        rp += 8; // skip CBCH type
        mCBCHChannelDescription.parseV(src, rp);
    }
    // Parse remaining as SI4 rest octets
    mRestOctets.parseV(src, rp);
}

void L3SystemInformationType4::writeBody(L3Frame& dest, size_t& wp) const {
    mLAI.writeV(dest, wp);
    mCI.writeV(dest, wp);
    mCellSelectionParameters.writeV(dest, wp);
    mCellOptions.writeV(dest, wp);
    mRACHControlParameters.writeV(dest, wp);
    if (mHaveCBCH) {
        dest.writeField(wp, 0x22, 8);
        mCBCHChannelDescription.writeV(dest, wp);
    }
    mRestOctets.writeV(dest, wp);
}

void L3SystemInformationType4::text(std::ostream& os) const {
    os << "SystemInformationType4: ";
    mLAI.text(os);
    os << " ";
    mCI.text(os);
    os << " ";
    mCellSelectionParameters.text(os);
    os << " ";
    mCellOptions.text(os);
    os << " ";
    mRACHControlParameters.text(os);
}

// ── L3SystemInformationType5 ───────────────────────────────────────────

L3SystemInformationType5::L3SystemInformationType5() {}

void L3SystemInformationType5::parseBody(const L3Frame& src, size_t& rp) {
    mBCCHFrequencyList.parseV(src, rp);
}

void L3SystemInformationType5::writeBody(L3Frame& dest, size_t& wp) const {
    mBCCHFrequencyList.writeV(dest, wp);
}

void L3SystemInformationType5::text(std::ostream& os) const {
    os << "SystemInformationType5: ";
    mBCCHFrequencyList.text(os);
}

// ── L3SystemInformationType5bis ────────────────────────────────────────

L3SystemInformationType5bis::L3SystemInformationType5bis() {}

void L3SystemInformationType5bis::parseBody(const L3Frame& src, size_t& rp) {
    mBCCHFrequencyList.parseV(src, rp);
}

void L3SystemInformationType5bis::writeBody(L3Frame& dest, size_t& wp) const {
    mBCCHFrequencyList.writeV(dest, wp);
}

void L3SystemInformationType5bis::text(std::ostream& os) const {
    os << "SystemInformationType5bis: ";
    mBCCHFrequencyList.text(os);
}

// ── L3SystemInformationType5ter ────────────────────────────────────────

L3SystemInformationType5ter::L3SystemInformationType5ter() {}

void L3SystemInformationType5ter::parseBody(const L3Frame& src, size_t& rp) {
    mBCCHFrequencyList.parseV(src, rp);
}

void L3SystemInformationType5ter::writeBody(L3Frame& dest, size_t& wp) const {
    mBCCHFrequencyList.writeV(dest, wp);
}

void L3SystemInformationType5ter::text(std::ostream& os) const {
    os << "SystemInformationType5ter: ";
    mBCCHFrequencyList.text(os);
}

// ── L3SystemInformationType6 ───────────────────────────────────────────

L3SystemInformationType6::L3SystemInformationType6() {}

void L3SystemInformationType6::parseBody(const L3Frame& src, size_t& rp) {
    mCI.parseV(src, rp);
    mLAI.parseV(src, rp);
    mCellOptions.parseV(src, rp);
    mNCCPermitted.parseV(src, rp);
}

void L3SystemInformationType6::writeBody(L3Frame& dest, size_t& wp) const {
    mCI.writeV(dest, wp);
    mLAI.writeV(dest, wp);
    mCellOptions.writeV(dest, wp);
    mNCCPermitted.writeV(dest, wp);
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

void L3SystemInformationType7::parseBody(const L3Frame& src, size_t& rp) {
    mRACHControl.parseTV(0x28, src, rp);
    while (rp + 8 <= src.size() && parseHasT(0x21, src, rp)) {
        L3CellChannelDescription ch;
        ch.parseTV(0x21, src, rp);
        mCellChannelDescriptions.push_back(ch);
    }
}

void L3SystemInformationType7::writeBody(L3Frame& dest, size_t& wp) const {
    mRACHControl.writeTV(0x28, dest, wp);
    for (const auto& ch : mCellChannelDescriptions) {
        ch.writeTV(0x21, dest, wp);
    }
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

void L3SystemInformationType8::parseBody(const L3Frame& src, size_t& rp) {
    mNCCPermitted.parseTV(0x27, src, rp);
    mRACHControl.parseTV(0x28, src, rp);
    while (rp + 8 <= src.size() && parseHasT(0x21, src, rp)) {
        L3CellChannelDescription ch;
        ch.parseTV(0x21, src, rp);
        mCellChannelDescriptions.push_back(ch);
    }
}

void L3SystemInformationType8::writeBody(L3Frame& dest, size_t& wp) const {
    mNCCPermitted.writeTV(0x27, dest, wp);
    mRACHControl.writeTV(0x28, dest, wp);
    for (const auto& ch : mCellChannelDescriptions) {
        ch.writeTV(0x21, dest, wp);
    }
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

void L3SystemInformationType9::parseBody(const L3Frame& src, size_t& rp) {
    mCI.parseV(src, rp);
    mCellSelectionParameters.parseV(src, rp);
    mCellOptions.parseV(src, rp);
}

void L3SystemInformationType9::writeBody(L3Frame& dest, size_t& wp) const {
    mCI.writeV(dest, wp);
    mCellSelectionParameters.writeV(dest, wp);
    mCellOptions.writeV(dest, wp);
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

void L3SystemInformationType16::parseBody(const L3Frame& src, size_t& rp) {
    mCI.parseV(src, rp);
    mCellSelectionParameters.parseV(src, rp);
    mCellOptions.parseV(src, rp);
}

void L3SystemInformationType16::writeBody(L3Frame& dest, size_t& wp) const {
    mCI.writeV(dest, wp);
    mCellSelectionParameters.writeV(dest, wp);
    mCellOptions.writeV(dest, wp);
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

void L3SystemInformationType17::parseBody(const L3Frame& src, size_t& rp) {
    mRACHControl.parseTV(0x28, src, rp);
    while (rp + 8 <= src.size() && parseHasT(0x21, src, rp)) {
        L3CellChannelDescription ch;
        ch.parseTV(0x21, src, rp);
        mCellChannelDescriptions.push_back(ch);
    }
}

void L3SystemInformationType17::writeBody(L3Frame& dest, size_t& wp) const {
    mRACHControl.writeTV(0x28, dest, wp);
    for (const auto& ch : mCellChannelDescriptions) {
        ch.writeTV(0x21, dest, wp);
    }
}

void L3SystemInformationType17::text(std::ostream& os) const {
    os << "SystemInformationType17: ";
    mRACHControl.text(os);
    os << " cells=" << mCellChannelDescriptions.size();
}

// ── L3SynchronizationChannelInformation ────────────────────────────────

L3SynchronizationChannelInformation::L3SynchronizationChannelInformation() {}

void L3SynchronizationChannelInformation::parseBody(const L3Frame& src, size_t& rp) {
    mCellIdentity.parseV(src, rp);
    mLocationAreaIdentity.parseV(src, rp);
}

void L3SynchronizationChannelInformation::writeBody(L3Frame& dest, size_t& wp) const {
    mCellIdentity.writeV(dest, wp);
    mLocationAreaIdentity.writeV(dest, wp);
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

void L3ChannelRequest::parseBody(const L3Frame& src, size_t& rp) {
    mRequestReference = src.readField(rp, 4);
    src.readField(rp, 4);
}

void L3ChannelRequest::writeBody(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, mRequestReference, 4);
    dest.writeField(wp, 0, 4);
}

void L3ChannelRequest::text(std::ostream& os) const {
    os << "ChannelRequest: RR=" << mRequestReference;
}

// ── L3HandoverAccess ───────────────────────────────────────────────────

L3HandoverAccess::L3HandoverAccess(unsigned wNumber)
    : mHandoverNumber(wNumber) {}

void L3HandoverAccess::parseBody(const L3Frame& src, size_t& rp) {
    mHandoverNumber = src.readField(rp, 27);
    src.readField(rp, 5);
}

void L3HandoverAccess::writeBody(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, mHandoverNumber, 27);
    dest.writeField(wp, 0, 5);
}

void L3HandoverAccess::text(std::ostream& os) const {
    os << "HandoverAccess: handoverNumber=" << mHandoverNumber;
}

// ── Factory ─────────────────────────────────────────────────────────────

L3RRMessage* L3RRFactory(int mti) {
    switch (mti) {
        case L3RRMessage::PagingRequestType1:          return new L3PagingRequestType1();
        case L3RRMessage::PagingRequestType2:          return new L3PagingRequestType2();
        case L3RRMessage::PagingRequestType3:          return new L3PagingRequestType3();
        case L3RRMessage::PagingResponse:              return new L3PagingResponse();
        case L3RRMessage::SystemInformationType1:      return new L3SystemInformationType1();
        case L3RRMessage::SystemInformationType2:      return new L3SystemInformationType2();
        case L3RRMessage::SystemInformationType2bis:   return new L3SystemInformationType2bis();
        case L3RRMessage::SystemInformationType2ter:   return new L3SystemInformationType2ter();
        case L3RRMessage::SystemInformationType3:      return new L3SystemInformationType3();
        case L3RRMessage::SystemInformationType4:      return new L3SystemInformationType4();
        case L3RRMessage::SystemInformationType5:      return new L3SystemInformationType5();
        case L3RRMessage::SystemInformationType5bis:   return new L3SystemInformationType5bis();
        case L3RRMessage::SystemInformationType5ter:   return new L3SystemInformationType5ter();
        case L3RRMessage::SystemInformationType6:      return new L3SystemInformationType6();
        case L3RRMessage::SystemInformationType7:      return new L3SystemInformationType7();
        case L3RRMessage::SystemInformationType8:      return new L3SystemInformationType8();
        case L3RRMessage::SystemInformationType9:      return new L3SystemInformationType9();
        case L3RRMessage::SystemInformationType13:     return new L3SystemInformationType13();
        case L3RRMessage::SystemInformationType16:     return new L3SystemInformationType16();
        case L3RRMessage::SystemInformationType17:     return new L3SystemInformationType17();
        case L3RRMessage::ChannelRelease:              return new L3ChannelRelease();
        case L3RRMessage::ImmediateAssignment:         return new L3ImmediateAssignment();
        case L3RRMessage::ImmediateAssignmentExtended: return new L3ImmediateAssignmentExtended();
        case L3RRMessage::ImmediateAssignmentReject:   return new L3ImmediateAssignmentReject();
        case L3RRMessage::AdditionalAssignment:        return new L3AdditionalAssignment();
        case L3RRMessage::PhysicalInformation:         return new L3PhysicalInformation();
        case L3RRMessage::HandoverCommand:             return new L3HandoverCommand();
        case L3RRMessage::HandoverComplete:            return new L3HandoverComplete();
        case L3RRMessage::HandoverFailure:             return new L3HandoverFailure();
        case L3RRMessage::AssignmentCommand:           return new L3AssignmentCommand();
        case L3RRMessage::AssignmentComplete:          return new L3AssignmentComplete();
        case L3RRMessage::AssignmentFailure:           return new L3AssignmentFailure();
        case L3RRMessage::ClassmarkEnquiry:            return new L3ClassmarkEnquiry();
        case L3RRMessage::ClassmarkChange:             return new L3ClassmarkChange();
        case L3RRMessage::MeasurementReport:           return new L3MeasurementReport();
        case L3RRMessage::CipheringModeCommand:        return new L3CipheringModeCommand(false, 0);
        case L3RRMessage::CipheringModeComplete:       return new L3CipheringModeComplete();
        case L3RRMessage::ChannelModeModify:           return new L3ChannelModeModify();
        case L3RRMessage::ChannelModeModifyAcknowledge: return new L3ChannelModeModifyAcknowledge();
        case L3RRMessage::GPRSSuspensionRequest:       return new L3GPRSSuspensionRequest();
        case L3RRMessage::ApplicationInformation:      return new L3ApplicationInformation();
        case L3RRMessage::RRStatus:                    return new L3RRStatus();
        case L3RRMessage::SynchronizationChannelInformation: return new L3SynchronizationChannelInformation();
        case L3RRMessage::ChannelRequest:              return new L3ChannelRequest();
        case L3RRMessage::HandoverAccess:              return new L3HandoverAccess();
        default:                                       return nullptr;
    }
}

// ── Parser ──────────────────────────────────────────────────────────────

std::unique_ptr<L3RRMessage> parseL3RR(const L3Frame& source) {
    if (source.size() < 16) return nullptr;

    unsigned mti = source.MTI();
    L3RRMessage* msg = L3RRFactory(static_cast<L3RRMessage::MessageType>(mti));
    if (!msg) {
        GSML3PARSER_LOG_WARN("Unknown RR MTI: 0x%02x", mti);
        return nullptr;
    }
    try {
        msg->parse(source);
    } catch (const ParseError&) {
        GSML3PARSER_LOG_WARN("RR parse failed for MTI=0x%02x", mti);
        delete msg;
        return nullptr;
    }
    return std::unique_ptr<L3RRMessage>(msg);
}

} // namespace gsml3parser
