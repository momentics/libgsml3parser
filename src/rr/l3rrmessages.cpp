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
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace gsml3parser {

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

// ── rrMessageName ───────────────────────────────────────────────────────

const char* rrMessageName(int mti) {
    switch (mti) {
        case L3SystemInformationType1::MTI:  return "SystemInformationType1";
        case L3SystemInformationType2::MTI:  return "SystemInformationType2";
        case L3SystemInformationType2bis::MTI: return "SystemInformationType2bis";
        case L3SystemInformationType2ter::MTI: return "SystemInformationType2ter";
        case L3SystemInformationType3::MTI:  return "SystemInformationType3";
        case L3SystemInformationType4::MTI:  return "SystemInformationType4";
        case L3SystemInformationType5::MTI:  return "SystemInformationType5";
        case L3SystemInformationType5bis::MTI: return "SystemInformationType5bis";
        case L3SystemInformationType5ter::MTI: return "SystemInformationType5ter";
        case L3SystemInformationType6::MTI:  return "SystemInformationType6";
        case L3SystemInformationType7::MTI:  return "SystemInformationType7";
        case L3SystemInformationType8::MTI:  return "SystemInformationType8";
        case L3SystemInformationType9::MTI:  return "SystemInformationType9";
        case L3SystemInformationType13::MTI: return "SystemInformationType13";
        case L3SystemInformationType16::MTI: return "SystemInformationType16";
        case L3SystemInformationType17::MTI: return "SystemInformationType17";
        case L3AssignmentCommand::MTI:       return "AssignmentCommand";
        case L3AssignmentComplete::MTI:      return "AssignmentComplete";
        case L3AssignmentFailure::MTI:       return "AssignmentFailure";
        case L3ChannelRelease::MTI:          return "ChannelRelease";
        case L3ImmediateAssignment::MTI:     return "ImmediateAssignment";
        case L3ImmediateAssignmentExtended::MTI: return "ImmediateAssignmentExtended";
        case L3ImmediateAssignmentReject::MTI: return "ImmediateAssignmentReject";
        case L3AdditionalAssignment::MTI:    return "AdditionalAssignment";
        case L3PagingRequestType1::MTI:      return "PagingRequestType1";
        case L3PagingRequestType2::MTI:      return "PagingRequestType2";
        case L3PagingRequestType3::MTI:      return "PagingRequestType3";
        case L3PagingResponse::MTI:          return "PagingResponse";
        case L3HandoverCommand::MTI:         return "HandoverCommand";
        case L3HandoverComplete::MTI:        return "HandoverComplete";
        case L3HandoverFailure::MTI:         return "HandoverFailure";
        case L3PhysicalInformation::MTI:     return "PhysicalInformation";
        case L3CipheringModeCommand::MTI:    return "CipheringModeCommand";
        case L3CipheringModeComplete::MTI:   return "CipheringModeComplete";
        case L3ChannelModeModify::MTI:       return "ChannelModeModify";
        case L3ChannelModeModifyAcknowledge::MTI: return "ChannelModeModifyAcknowledge";
        case L3RRStatus::MTI:                return "RRStatus";
        case L3ClassmarkChange::MTI:         return "ClassmarkChange";
        case L3ClassmarkEnquiry::MTI:        return "ClassmarkEnquiry";
        case L3MeasurementReport::MTI:       return "MeasurementReport";
        case L3GPRSSuspensionRequest::MTI:   return "GPRSSuspensionRequest";
        case L3ApplicationInformation::MTI:  return "ApplicationInformation";
        case L3SynchronizationChannelInformation::MTI: return "SynchronizationChannelInformation";
        case L3ChannelRequest::MTI:          return "ChannelRequest";
        case L3HandoverAccess::MTI:          return "HandoverAccess";
        case L3ConfigurationChangeCommand::MTI: return "ConfigurationChangeCommand";
        case L3ConfigurationChangeAcknowledge::MTI: return "ConfigurationChangeAcknowledge";
        case L3ConfigurationChangeReject::MTI: return "ConfigurationChangeReject";
        case L3PartialRelease::MTI:          return "PartialRelease";
        case L3PartialReleaseComplete::MTI:  return "PartialReleaseComplete";
        case L3ExtendedMeasurementReport::MTI: return "ExtendedMeasurementReport";
        case L3ExtendedMeasurementOrder::MTI: return "ExtendedMeasurementOrder";
        case L3FrequencyRedefinition::MTI:   return "FrequencyRedefinition";
        case L3NotificationNCH::MTI:         return "NotificationNCH";
        case L3NotificationResponse::MTI:    return "NotificationResponse";
        case L3VGCSUplinkGrant::MTI:         return "VGCSUplinkGrant";
        case L3UplinkRelease::MTI:           return "UplinkRelease";
        case L3UplinkBusy::MTI:              return "UplinkBusy";
        case L3TalkerIndication::MTI:        return "TalkerIndication";
        case L3PriorityUplinkRequest::MTI:   return "PriorityUplinkRequest";
        case L3DataIndication::MTI:          return "DataIndication";
        case L3DataIndication2::MTI:         return "DataIndication2";
        case L3DTMAssignmentFailure::MTI:    return "DTMAssignmentFailure";
        case L3DTMReject::MTI:               return "DTMReject";
        case L3DTMRequest::MTI:              return "DTMRequest";
        case L3PacketAssignment::MTI:        return "PacketAssignment";
        case L3DTMAssignmentCommand::MTI:    return "DTMAssignmentCommand";
        case L3DTMInformation::MTI:          return "DTMInformation";
        case L3PacketInformation::MTI:       return "PacketInformation";
        case L3UTRANClassmarkChange::MTI:    return "UTRANClassmarkChange";
        case L3CDMA2000ClassmarkChange::MTI: return "CDMA2000ClassmarkChange";
        case L3IntersysToUTRANHOCommand::MTI: return "IntersysToUTRANHOCommand";
        case L3IntersysToCDMA2000HOCommand::MTI: return "IntersysToCDMA2000HOCommand";
        case L3GERANIUClassmarkChange::MTI:  return "GERANIUClassmarkChange";
        case L3SystemInformationType14::MTI: return "SystemInformationType14";
        case L3SystemInformationType15::MTI: return "SystemInformationType15";
        case L3SystemInformationType18::MTI: return "SystemInformationType18";
        case L3SystemInformationType19::MTI: return "SystemInformationType19";
        case L3SystemInformationType20::MTI: return "SystemInformationType20";
        case L3SystemInformationType13alt::MTI: return "SystemInformationType13alt";
        case L3SystemInformationType2n::MTI: return "SystemInformationType2n";
        case L3SystemInformationType21::MTI: return "SystemInformationType21";
        case L3SystemInformationType22::MTI: return "SystemInformationType22";
        case L3SystemInformationType23::MTI: return "SystemInformationType23";
        case L3SystemInformationType10::MTI: return "SystemInformationType10";
        case L3SystemInformationType10bis::MTI: return "SystemInformationType10bis";
        case L3SystemInformationType10ter::MTI: return "SystemInformationType10ter";
        case L3NotificationFACCH::MTI:       return "NotificationFACCH";
        case L3UplinkFree::MTI:              return "UplinkFree";
        case L3EnhancedMeasurementRepUL::MTI: return "EnhancedMeasurementRepUL";
        case L3MeasurementInfoDL::MTI:       return "MeasurementInfoDL";
        case L3VBSVGCSRecon::MTI:            return "VBSVGCSRecon";
        case L3VBSVGCSRecon2::MTI:           return "VBSVGCSRecon2";
        case L3VGCSAddInfo::MTI:             return "VGCSAddInfo";
        case L3VGCSMSInfo::MTI:              return "VGCSMSInfo";
        case L3VGCSSNeighCellInfo::MTI:      return "VGCSSNeighCellInfo";
        case L3NotifyAppData::MTI:           return "NotifyAppData";
        default:                      return "Unknown_RR";
    }
}

// ── L3PagingRequestType1 ────────────────────────────────────────────────

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

size_t L3PagingRequestType1::bodyLength() const {
    int sz = static_cast<int>(mMobileIDs.size());
    size_t len = 1;
    len += mMobileIDs[0].lengthV() + 1;
    if (sz > 1) len += 2 + mMobileIDs[1].lengthV();
    return len;
}

Expected<L3PagingRequestType1> L3PagingRequestType1::parse(BitReader& br) {
    L3PagingRequestType1 msg;
    auto r = br.readField(2); if (!r) return Expected<L3PagingRequestType1>::error(r.error());
    msg.mChannelsNeeded[1] = channelNeededType(r.value());
    r = br.readField(2); if (!r) return Expected<L3PagingRequestType1>::error(r.error());
    msg.mChannelsNeeded[0] = channelNeededType(r.value());
    r = br.readField(4); if (!r) return Expected<L3PagingRequestType1>::error(r.error());

    L3MobileIdentity id;
    {
        auto lenR = br.readField(8); if (!lenR) return Expected<L3PagingRequestType1>::error(lenR.error());
        auto res = L3MobileIdentity::parse(br, lenR.value());
        if (!res) return Expected<L3PagingRequestType1>::error(res.error());
        msg.mMobileIDs.push_back(std::move(res.value()));
    }

    if (br.hasMore()) {
        unsigned peek = br.peekField(8);
        if (peek == 0x17) {
            { auto _ = br.readField(8); if (!_) return Expected<L3PagingRequestType1>::error(_.error()); }
            auto lenR = br.readField(8); if (!lenR) return Expected<L3PagingRequestType1>::error(lenR.error());
            auto res = L3MobileIdentity::parse(br, lenR.value());
            if (!res) return Expected<L3PagingRequestType1>::error(res.error());
            msg.mMobileIDs.push_back(std::move(res.value()));
        }
    }

    return Expected<L3PagingRequestType1>::hold(std::move(msg));
}

void L3PagingRequestType1::write(BitWriter& bw) const {
    int sz = static_cast<int>(mMobileIDs.size());
    bw.writeField(channelNeededCode(mChannelsNeeded[sz > 1 ? 1 : 0]), 2);
    bw.writeField(channelNeededCode(mChannelsNeeded[0]), 2);
    bw.writeField(0x0, 4);
    bw.writeField(static_cast<uint32_t>(mMobileIDs[0].lengthV()), 8);
    mMobileIDs[0].write(bw);
    if (sz > 1) {
        bw.writeField(0x97, 8);
        bw.writeField(static_cast<uint32_t>(mMobileIDs[1].lengthV()), 8);
        mMobileIDs[1].write(bw);
    }
}

void L3PagingRequestType1::text(std::ostream& os) const {
    os << "PagingRequestType1: ";
    for (const auto& id : mMobileIDs) {
        id.text(os);
    }
}

// ── L3PagingRequestType2 ───────────────────────────────────────────────

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

size_t L3PagingRequestType2::bodyLength() const {
    return 1 + mTMSIs.size() * 4;
}

Expected<L3PagingRequestType2> L3PagingRequestType2::parse(BitReader& br) {
    L3PagingRequestType2 msg;
    auto r = br.readField(2); if (!r) return Expected<L3PagingRequestType2>::error(r.error());
    msg.mChannelsNeeded[1] = channelNeededType(r.value());
    r = br.readField(2); if (!r) return Expected<L3PagingRequestType2>::error(r.error());
    msg.mChannelsNeeded[0] = channelNeededType(r.value());
    r = br.readField(4); if (!r) return Expected<L3PagingRequestType2>::error(r.error());

    for (int i = 0; i < 2; ++i) {
        r = br.readField(32); if (!r) return Expected<L3PagingRequestType2>::error(r.error());
        msg.mTMSIs.push_back(r.value());
    }

    return Expected<L3PagingRequestType2>::hold(std::move(msg));
}

void L3PagingRequestType2::write(BitWriter& bw) const {
    bw.writeField(channelNeededCode(mChannelsNeeded[1]), 2);
    bw.writeField(channelNeededCode(mChannelsNeeded[0]), 2);
    bw.writeField(0x0, 4);
    for (const auto& tmsi : mTMSIs) {
        bw.writeField(tmsi, 32);
    }
}

void L3PagingRequestType2::text(std::ostream& os) const {
    os << "PagingRequestType2: ";
    for (const auto& tmsi : mTMSIs) {
        os << "TMSI=0x" << std::hex << tmsi << std::dec;
    }
}

// ── L3PagingRequestType3 ───────────────────────────────────────────────

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

size_t L3PagingRequestType3::bodyLength() const {
    return 1 + mTMSIs.size() * 4;
}

Expected<L3PagingRequestType3> L3PagingRequestType3::parse(BitReader& br) {
    L3PagingRequestType3 msg;
    auto r = br.readField(2); if (!r) return Expected<L3PagingRequestType3>::error(r.error());
    msg.mChannelsNeeded[1] = channelNeededType(r.value());
    r = br.readField(2); if (!r) return Expected<L3PagingRequestType3>::error(r.error());
    msg.mChannelsNeeded[0] = channelNeededType(r.value());
    r = br.readField(4); if (!r) return Expected<L3PagingRequestType3>::error(r.error());

    for (int i = 0; i < 4; ++i) {
        r = br.readField(32); if (!r) return Expected<L3PagingRequestType3>::error(r.error());
        msg.mTMSIs.push_back(r.value());
    }

    return Expected<L3PagingRequestType3>::hold(std::move(msg));
}

void L3PagingRequestType3::write(BitWriter& bw) const {
    bw.writeField(channelNeededCode(mChannelsNeeded[1]), 2);
    bw.writeField(channelNeededCode(mChannelsNeeded[0]), 2);
    bw.writeField(0x0, 4);
    for (const auto& tmsi : mTMSIs) {
        bw.writeField(tmsi, 32);
    }
}

void L3PagingRequestType3::text(std::ostream& os) const {
    os << "PagingRequestType3: ";
    for (const auto& tmsi : mTMSIs) {
        os << "TMSI=0x" << std::hex << tmsi << std::dec;
    }
}

// ── L3PagingResponse ───────────────────────────────────────────────────

size_t L3PagingResponse::bodyLength() const {
    return 1 + 1 + mClassmark.lengthV() + 1 + mMobileID.lengthV();
}

Expected<L3PagingResponse> L3PagingResponse::parse(BitReader& br) {
    L3PagingResponse msg;
    auto r = br.readField(4); if (!r) return Expected<L3PagingResponse>::error(r.error());
    msg.mCKSN = r.value();
    r = br.readField(4); if (!r) return Expected<L3PagingResponse>::error(r.error());

    {
        auto lenR = br.readField(8); if (!lenR) return Expected<L3PagingResponse>::error(lenR.error());
        auto res = L3MobileStationClassmark2::parse(br);
        if (!res) return Expected<L3PagingResponse>::error(res.error());
        msg.mClassmark = std::move(res.value());
    }
    {
        auto lenR = br.readField(8); if (!lenR) return Expected<L3PagingResponse>::error(lenR.error());
        auto res = L3MobileIdentity::parse(br, lenR.value());
        if (!res) return Expected<L3PagingResponse>::error(res.error());
        msg.mMobileID = std::move(res.value());
    }

    return Expected<L3PagingResponse>::hold(std::move(msg));
}

void L3PagingResponse::write(BitWriter& bw) const {
    bw.writeField(mCKSN & 0x0F, 4);
    bw.writeField(0, 4);
    bw.writeField(static_cast<uint32_t>(L3MobileStationClassmark2::lengthV()), 8);
    mClassmark.write(bw);
    bw.writeField(static_cast<uint32_t>(mMobileID.lengthV()), 8);
    mMobileID.write(bw);
}

void L3PagingResponse::text(std::ostream& os) const {
    os << "PagingResponse: ";
    mMobileID.text(os);
    os << " ";
    mClassmark.text(os);
}

// ── L3ChannelRelease ───────────────────────────────────────────────────

size_t L3ChannelRelease::bodyLength() const {
    return 1 + (mGprsResumptionPresent ? 1 : 0);
}

Expected<L3ChannelRelease> L3ChannelRelease::parse(BitReader& br) {
    L3ChannelRelease msg;
    auto r = br.readField(8); if (!r) return Expected<L3ChannelRelease>::error(r.error());
    msg.mCause = static_cast<RRCause>(r.value());

    if (br.hasMore()) {
        unsigned peek = br.peekField(8);
        if (peek == 0x01) {
            { auto _ = br.readField(8); if (!_) return Expected<L3ChannelRelease>::error(_.error()); }
            msg.mGprsResumptionPresent = true;
            r = br.readField(1); if (!r) return Expected<L3ChannelRelease>::error(r.error());
            msg.mGprsResumptionBit = r.value() != 0;
            r = br.readField(7); if (!r) return Expected<L3ChannelRelease>::error(r.error());
        }
    }

    return Expected<L3ChannelRelease>::hold(std::move(msg));
}

void L3ChannelRelease::write(BitWriter& bw) const {
    bw.writeField(static_cast<uint32_t>(mCause), 8);
    if (mGprsResumptionPresent) {
        bw.writeField(0x01, 8);
        bw.writeField(mGprsResumptionBit ? 1 : 0, 1);
        bw.writeField(0, 7);
    }
}

void L3ChannelRelease::text(std::ostream& os) const {
    os << "ChannelRelease: cause=" << RRCause2Str(mCause);
    if (mGprsResumptionPresent) {
        os << " gprsResumption=" << (mGprsResumptionBit ? "on" : "off");
    }
}

L3ChannelRelease L3ChannelRelease::Builder::build() const {
    L3ChannelRelease msg;
    msg.mCause = mCause;
    msg.mGprsResumptionPresent = mGprsResumptionPresent;
    msg.mGprsResumptionBit = mGprsResumptionBit;
    return msg;
}

L3ChannelRelease::Builder L3ChannelRelease::builder() {
    return Builder{};
}

// ── L3RRStatus ──────────────────────────────────────────────────────────

Expected<L3RRStatus> L3RRStatus::parse(BitReader& br) {
    L3RRStatus msg;
    auto r = br.readField(8); if (!r) return Expected<L3RRStatus>::error(r.error());
    msg.mCause = static_cast<RRCause>(r.value());
    return Expected<L3RRStatus>::hold(std::move(msg));
}

void L3RRStatus::write(BitWriter& bw) const {
    bw.writeField(static_cast<uint32_t>(mCause), 8);
}

void L3RRStatus::text(std::ostream& os) const {
    os << "RRStatus: cause=" << RRCause2Str(mCause);
}

L3RRStatus L3RRStatus::Builder::build() const {
    L3RRStatus msg;
    msg.mCause = mCause;
    return msg;
}

L3RRStatus::Builder L3RRStatus::builder() {
    return Builder{};
}

// ── L3AssignmentCommand ─────────────────────────────────────────────────

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

size_t L3AssignmentCommand::bodyLength() const {
    size_t len = mChannel.lengthV() + mPowerCommand.lengthV();
    if (mHaveMode1) len += 1 + mMode1.lengthV();
    if (isAMR()) len += 1 + 1 + mMultiRate.lengthV();
    return len;
}

Expected<L3AssignmentCommand> L3AssignmentCommand::parse(BitReader& br) {
    L3AssignmentCommand msg;
    {
        auto res = L3ChannelDescription::parse(br);
        if (!res) return Expected<L3AssignmentCommand>::error(res.error());
        msg.mChannel = std::move(res.value());
    }
    {
        auto res = L3PowerCommand::parse(br);
        if (!res) return Expected<L3AssignmentCommand>::error(res.error());
        msg.mPowerCommand = std::move(res.value());
    }

    if (br.hasMore()) {
        unsigned peek = br.peekField(8);
        if (peek == 0x63) {
            { auto _ = br.readField(8); if (!_) return Expected<L3AssignmentCommand>::error(_.error()); }
            auto res = L3ChannelMode::parse(br);
            if (!res) return Expected<L3AssignmentCommand>::error(res.error());
            msg.mMode1 = std::move(res.value());
            msg.mHaveMode1 = true;
        }
    }

    if (msg.isAMR() && br.hasMore()) {
        unsigned peek = br.peekField(8);
        if ((peek & 0x7F) == 0x15) {
            auto ieiR = br.readField(8); if (!ieiR) return Expected<L3AssignmentCommand>::error(ieiR.error());
            bool ext = (ieiR.value() & 0x80) != 0;
            if (ext) {
                auto lR = br.readField(8); if (!lR) return Expected<L3AssignmentCommand>::error(lR.error());
                (void)lR;
            }
            auto res = L3MultiRateConfiguration::parse(br);
            if (!res) return Expected<L3AssignmentCommand>::error(res.error());
            msg.mMultiRate = std::move(res.value());
        }
    }

    return Expected<L3AssignmentCommand>::hold(std::move(msg));
}

void L3AssignmentCommand::write(BitWriter& bw) const {
    mChannel.write(bw);
    mPowerCommand.write(bw);
    if (mHaveMode1) {
        bw.writeField(0x63, 8);
        mMode1.write(bw);
    }
    if (isAMR()) {
        bw.writeField(0x95, 8);
        bw.writeField(static_cast<uint32_t>(mMultiRate.lengthV()), 8);
        mMultiRate.write(bw);
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

// ── L3AssignmentComplete ───────────────────────────────────────────────

Expected<L3AssignmentComplete> L3AssignmentComplete::parse(BitReader& br) {
    L3AssignmentComplete msg;
    auto r = br.readField(8); if (!r) return Expected<L3AssignmentComplete>::error(r.error());
    msg.mCause = static_cast<RRCause>(r.value());
    return Expected<L3AssignmentComplete>::hold(std::move(msg));
}

void L3AssignmentComplete::write(BitWriter& bw) const {
    bw.writeField(static_cast<uint32_t>(mCause), 8);
}

void L3AssignmentComplete::text(std::ostream& os) const {
    os << "AssignmentComplete: cause=" << RRCause2Str(mCause);
}

L3AssignmentComplete L3AssignmentComplete::Builder::build() const {
    L3AssignmentComplete msg;
    msg.mCause = mCause;
    return msg;
}

L3AssignmentComplete::Builder L3AssignmentComplete::builder() {
    return Builder{};
}

// ── L3AssignmentFailure ────────────────────────────────────────────────

Expected<L3AssignmentFailure> L3AssignmentFailure::parse(BitReader& br) {
    L3AssignmentFailure msg;
    auto r = br.readField(8); if (!r) return Expected<L3AssignmentFailure>::error(r.error());
    msg.mCause = static_cast<RRCause>(r.value());
    return Expected<L3AssignmentFailure>::hold(std::move(msg));
}

void L3AssignmentFailure::write(BitWriter& bw) const {
    bw.writeField(static_cast<uint32_t>(mCause), 8);
}

void L3AssignmentFailure::text(std::ostream& os) const {
    os << "AssignmentFailure: cause=" << RRCause2Str(mCause);
}

L3AssignmentFailure L3AssignmentFailure::Builder::build() const {
    L3AssignmentFailure msg;
    msg.mCause = mCause;
    return msg;
}

L3AssignmentFailure::Builder L3AssignmentFailure::builder() {
    return Builder{};
}

// ── L3ClassmarkEnquiry ──────────────────────────────────────────────────

Expected<L3ClassmarkEnquiry> L3ClassmarkEnquiry::parse(BitReader&) {
    return Expected<L3ClassmarkEnquiry>::hold(L3ClassmarkEnquiry{});
}

void L3ClassmarkEnquiry::write(BitWriter&) const {}

void L3ClassmarkEnquiry::text(std::ostream& os) const {
    os << "ClassmarkEnquiry";
}

// ── L3ClassmarkChange ──────────────────────────────────────────────────

size_t L3ClassmarkChange::bodyLength() const {
    size_t len = 1 + mClassmark.lengthV();
    if (mHaveAdditionalClassmark) len += 1 + 1 + mAdditionalClassmark.lengthV();
    return len;
}

Expected<L3ClassmarkChange> L3ClassmarkChange::parse(BitReader& br) {
    L3ClassmarkChange msg;
    {
        auto lenR = br.readField(8); if (!lenR) return Expected<L3ClassmarkChange>::error(lenR.error());
        auto res = L3MobileStationClassmark2::parse(br);
        if (!res) return Expected<L3ClassmarkChange>::error(res.error());
        msg.mClassmark = std::move(res.value());
    }

    if (br.hasMore()) {
        unsigned peek = br.peekField(8);
        if ((peek & 0x7F) == 0x20) {
            auto ieiR = br.readField(8); if (!ieiR) return Expected<L3ClassmarkChange>::error(ieiR.error());
            bool ext = (ieiR.value() & 0x80) != 0;
            if (ext) {
                auto lR = br.readField(8); if (!lR) return Expected<L3ClassmarkChange>::error(lR.error());
                (void)lR;
            }
            auto res = L3MobileStationClassmark3::parse(br);
            if (!res) return Expected<L3ClassmarkChange>::error(res.error());
            msg.mAdditionalClassmark = std::move(res.value());
            msg.mHaveAdditionalClassmark = true;
        }
    }

    return Expected<L3ClassmarkChange>::hold(std::move(msg));
}

void L3ClassmarkChange::write(BitWriter& bw) const {
    bw.writeField(static_cast<uint32_t>(L3MobileStationClassmark2::lengthV()), 8);
    mClassmark.write(bw);
    if (mHaveAdditionalClassmark) {
        bw.writeField(0xA0, 8);
        bw.writeField(static_cast<uint32_t>(L3MobileStationClassmark3::lengthV()), 8);
        mAdditionalClassmark.write(bw);
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

Expected<L3MeasurementReport> L3MeasurementReport::parse(BitReader& br) {
    L3MeasurementReport msg;
    {
        auto res = L3MeasurementResults::parse(br);
        if (!res) return Expected<L3MeasurementReport>::error(res.error());
        msg.mMeasurementResults = std::move(res.value());
    }
    return Expected<L3MeasurementReport>::hold(std::move(msg));
}

void L3MeasurementReport::write(BitWriter& bw) const {
    mMeasurementResults.write(bw);
}

void L3MeasurementReport::text(std::ostream& os) const {
    os << "MeasurementReport: ";
    mMeasurementResults.text(os);
}

// ── L3CipheringModeCommand ─────────────────────────────────────────────

Expected<L3CipheringModeCommand> L3CipheringModeCommand::parse(BitReader& br) {
    L3CipheringModeCommand msg;
    {
        auto res = L3CipheringModeResponse::parse(br);
        if (!res) return Expected<L3CipheringModeCommand>::error(res.error());
        msg.mCipheringModeResponse = std::move(res.value());
    }
    {
        auto res = L3CipheringModeSetting::parse(br);
        if (!res) return Expected<L3CipheringModeCommand>::error(res.error());
        auto cms = res.value();
        msg.mCiphering = cms.ciphering();
        msg.mAlgorithm = cms.algorithm();
    }
    return Expected<L3CipheringModeCommand>::hold(std::move(msg));
}

void L3CipheringModeCommand::write(BitWriter& bw) const {
    mCipheringModeResponse.write(bw);
    L3CipheringModeSetting cms(mCiphering, mAlgorithm);
    cms.write(bw);
}

void L3CipheringModeCommand::text(std::ostream& os) const {
    os << "CipheringModeCommand: ciphering=" << mCiphering
       << " algorithm=A5/" << mAlgorithm
       << " includeIMEISV=" << mCipheringModeResponse.includeIMEISV();
}

L3CipheringModeCommand L3CipheringModeCommand::Builder::build() const {
    L3CipheringModeCommand msg;
    msg.mCiphering = mCiphering;
    msg.mAlgorithm = mAlgorithm;
    msg.mCipheringModeResponse = mCipheringModeResponse;
    return msg;
}

L3CipheringModeCommand::Builder L3CipheringModeCommand::builder() {
    return Builder{};
}

// ── L3CipheringModeComplete ────────────────────────────────────────────

Expected<L3CipheringModeComplete> L3CipheringModeComplete::parse(BitReader&) {
    return Expected<L3CipheringModeComplete>::hold(L3CipheringModeComplete{});
}

void L3CipheringModeComplete::write(BitWriter&) const {}

void L3CipheringModeComplete::text(std::ostream& os) const {
    os << "CipheringModeComplete";
}

L3CipheringModeComplete L3CipheringModeComplete::Builder::build() const {
    return L3CipheringModeComplete{};
}

L3CipheringModeComplete::Builder L3CipheringModeComplete::builder() {
    return Builder{};
}

// ── L3HandoverComplete ─────────────────────────────────────────────────

Expected<L3HandoverComplete> L3HandoverComplete::parse(BitReader& br) {
    L3HandoverComplete msg;
    auto r = br.readField(8); if (!r) return Expected<L3HandoverComplete>::error(r.error());
    msg.mCause = static_cast<RRCause>(r.value());
    return Expected<L3HandoverComplete>::hold(std::move(msg));
}

void L3HandoverComplete::write(BitWriter& bw) const {
    bw.writeField(static_cast<uint32_t>(mCause), 8);
}

void L3HandoverComplete::text(std::ostream& os) const {
    os << "HandoverComplete: cause=" << RRCause2Str(mCause);
}

L3HandoverComplete L3HandoverComplete::Builder::build() const {
    L3HandoverComplete msg;
    msg.mCause = mCause;
    return msg;
}

L3HandoverComplete::Builder L3HandoverComplete::builder() {
    return Builder{};
}

// ── L3HandoverFailure ──────────────────────────────────────────────────

Expected<L3HandoverFailure> L3HandoverFailure::parse(BitReader& br) {
    L3HandoverFailure msg;
    auto r = br.readField(8); if (!r) return Expected<L3HandoverFailure>::error(r.error());
    msg.mCause = static_cast<RRCause>(r.value());
    return Expected<L3HandoverFailure>::hold(std::move(msg));
}

void L3HandoverFailure::write(BitWriter& bw) const {
    bw.writeField(static_cast<uint32_t>(mCause), 8);
}

void L3HandoverFailure::text(std::ostream& os) const {
    os << "HandoverFailure: cause=" << RRCause2Str(mCause);
}

L3HandoverFailure L3HandoverFailure::Builder::build() const {
    L3HandoverFailure msg;
    msg.mCause = mCause;
    return msg;
}

L3HandoverFailure::Builder L3HandoverFailure::builder() {
    return Builder{};
}

// ── L3ChannelModeModify ────────────────────────────────────────────────

size_t L3ChannelModeModify::bodyLength() const {
    return mDescription.lengthV() + mMode.lengthV() + (isAMR() ? (1 + 1 + mMultiRate.lengthV()) : 0);
}

Expected<L3ChannelModeModify> L3ChannelModeModify::parse(BitReader& br) {
    L3ChannelModeModify msg;
    {
        auto res = L3ChannelDescription::parse(br);
        if (!res) return Expected<L3ChannelModeModify>::error(res.error());
        msg.mDescription = std::move(res.value());
    }
    {
        auto res = L3ChannelMode::parse(br);
        if (!res) return Expected<L3ChannelModeModify>::error(res.error());
        msg.mMode = std::move(res.value());
    }
    if (msg.isAMR() && br.hasMore()) {
        unsigned peek = br.peekField(8);
        if ((peek & 0x7F) == 0x15) {
            auto ieiR = br.readField(8); if (!ieiR) return Expected<L3ChannelModeModify>::error(ieiR.error());
            bool ext = (ieiR.value() & 0x80) != 0;
            if (ext) {
                auto lR = br.readField(8); if (!lR) return Expected<L3ChannelModeModify>::error(lR.error());
                (void)lR;
            }
            auto res = L3MultiRateConfiguration::parse(br);
            if (!res) return Expected<L3ChannelModeModify>::error(res.error());
            msg.mMultiRate = std::move(res.value());
        }
    }
    return Expected<L3ChannelModeModify>::hold(std::move(msg));
}

void L3ChannelModeModify::write(BitWriter& bw) const {
    mDescription.write(bw);
    mMode.write(bw);
    if (isAMR()) {
        bw.writeField(0x95, 8);
        bw.writeField(static_cast<uint32_t>(mMultiRate.lengthV()), 8);
        mMultiRate.write(bw);
    }
}

void L3ChannelModeModify::text(std::ostream& os) const {
    os << "ChannelModeModify: ";
    mDescription.text(os);
    os << " ";
    mMode.text(os);
}

L3ChannelModeModify L3ChannelModeModify::Builder::build() const {
    L3ChannelModeModify msg;
    msg.mDescription = mDescription;
    msg.mMode = mMode;
    msg.mMultiRate = mMultiRate;
    return msg;
}

L3ChannelModeModify::Builder L3ChannelModeModify::builder() {
    return Builder{};
}

// ── L3ChannelModeModifyAcknowledge ─────────────────────────────────────

size_t L3ChannelModeModifyAcknowledge::bodyLength() const {
    return mDescription.lengthV() + mMode.lengthV();
}

Expected<L3ChannelModeModifyAcknowledge> L3ChannelModeModifyAcknowledge::parse(BitReader& br) {
    L3ChannelModeModifyAcknowledge msg;
    {
        auto res = L3ChannelDescription::parse(br);
        if (!res) return Expected<L3ChannelModeModifyAcknowledge>::error(res.error());
        msg.mDescription = std::move(res.value());
    }
    {
        auto res = L3ChannelMode::parse(br);
        if (!res) return Expected<L3ChannelModeModifyAcknowledge>::error(res.error());
        msg.mMode = std::move(res.value());
    }
    return Expected<L3ChannelModeModifyAcknowledge>::hold(std::move(msg));
}

void L3ChannelModeModifyAcknowledge::write(BitWriter& bw) const {
    mDescription.write(bw);
    mMode.write(bw);
}

void L3ChannelModeModifyAcknowledge::text(std::ostream& os) const {
    os << "ChannelModeModifyAcknowledge: ";
    mDescription.text(os);
    os << " ";
    mMode.text(os);
}

L3ChannelModeModifyAcknowledge L3ChannelModeModifyAcknowledge::Builder::build() const {
    L3ChannelModeModifyAcknowledge msg;
    msg.mDescription = mDescription;
    msg.mMode = mMode;
    return msg;
}

L3ChannelModeModifyAcknowledge::Builder L3ChannelModeModifyAcknowledge::builder() {
    return Builder{};
}

// ── L3GPRSSuspensionRequest ────────────────────────────────────────────

Expected<L3GPRSSuspensionRequest> L3GPRSSuspensionRequest::parse(BitReader& br) {
    L3GPRSSuspensionRequest msg;
    auto r = br.readField(32); if (!r) return Expected<L3GPRSSuspensionRequest>::error(r.error());
    msg.mTLLI = r.value();
    msg.mRaId.resize(6);
    for (size_t i = 0; i < 6; ++i) {
        r = br.readField(8); if (!r) return Expected<L3GPRSSuspensionRequest>::error(r.error());
        msg.mRaId[i] = static_cast<uint8_t>(r.value());
    }
    r = br.readField(8); if (!r) return Expected<L3GPRSSuspensionRequest>::error(r.error());
    msg.mSuspensionCause = static_cast<uint8_t>(r.value());

    if (br.hasMore()) {
        unsigned peek = br.peekField(8);
        if (peek == 0x01) {
            { auto _ = br.readField(8); if (!_) return Expected<L3GPRSSuspensionRequest>::error(_.error()); }
            r = br.readField(8); if (!r) return Expected<L3GPRSSuspensionRequest>::error(r.error());
            msg.mServiceSupport = static_cast<uint8_t>(r.value());
        }
    }

    return Expected<L3GPRSSuspensionRequest>::hold(std::move(msg));
}

void L3GPRSSuspensionRequest::write(BitWriter& bw) const {
    bw.writeField(mTLLI, 32);
    for (size_t i = 0; i < 6; ++i) {
        bw.writeField(mRaId[i], 8);
    }
    bw.writeField(mSuspensionCause, 8);
    if (mServiceSupport) {
        bw.writeField(0x01, 8);
        bw.writeField(mServiceSupport, 8);
    }
}

void L3GPRSSuspensionRequest::text(std::ostream& os) const {
    os << "GPRSSuspensionRequest: TLLI=0x" << std::hex << mTLLI
       << " cause=" << static_cast<int>(mSuspensionCause);
}

// ── L3ApplicationInformation ───────────────────────────────────────────

size_t L3ApplicationInformation::bodyLength() const {
    size_t dataLen = (mData.size() + 7) / 8;
    return 1 + dataLen;
}

Expected<L3ApplicationInformation> L3ApplicationInformation::parse(BitReader& br) {
    L3ApplicationInformation msg;

    auto r = br.readField(4); if (!r) return Expected<L3ApplicationInformation>::error(r.error());
    msg.mProtocolIdentifier = r.value();
    r = br.readField(1); if (!r) return Expected<L3ApplicationInformation>::error(r.error());
    msg.mCR = r.value();
    r = br.readField(1); if (!r) return Expected<L3ApplicationInformation>::error(r.error());
    msg.mFirstSegment = r.value();
    r = br.readField(1); if (!r) return Expected<L3ApplicationInformation>::error(r.error());
    msg.mLastSegment = r.value();
    r = br.readField(1); if (!r) return Expected<L3ApplicationInformation>::error(r.error());

    while (br.hasMore()) {
        r = br.readField(8); if (!r) return Expected<L3ApplicationInformation>::error(r.error());
        uint8_t byte = static_cast<uint8_t>(r.value());
        for (int bit = 7; bit >= 0; --bit) {
            msg.mData.push_back((byte >> bit) & 1);
        }
    }

    return Expected<L3ApplicationInformation>::hold(std::move(msg));
}

void L3ApplicationInformation::write(BitWriter& bw) const {
    bw.writeField(mProtocolIdentifier, 4);
    bw.writeField(mCR, 1);
    bw.writeField(mFirstSegment, 1);
    bw.writeField(mLastSegment, 1);
    bw.writeField(0, 1);

    size_t dataLen = (mData.size() + 7) / 8;
    for (size_t i = 0; i < dataLen; ++i) {
        uint8_t byte = 0;
        for (int bit = 0; bit < 8; ++bit) {
            size_t idx = i * 8 + bit;
            if (idx < mData.size()) {
                byte |= (mData[idx] << (7 - bit));
            }
        }
        bw.writeField(byte, 8);
    }
}

void L3ApplicationInformation::text(std::ostream& os) const {
    os << "ApplicationInformation: PID=" << mProtocolIdentifier
       << " CR=" << mCR << " first=" << mFirstSegment << " last=" << mLastSegment
       << " len=" << mData.size() << "bits";
}

// ── System Information Types ────────────────────────────────────────────

// ── L3SystemInformationType1 ────────────────────────────────────────────

Expected<L3SystemInformationType1> L3SystemInformationType1::parse(BitReader& br) {
    L3SystemInformationType1 msg;
    {
        auto res = L3FrequencyList::parse(br);
        if (!res) return Expected<L3SystemInformationType1>::error(res.error());
        msg.mCellChannelDescription = std::move(res.value());
    }
    {
        auto res = L3RACHControlParameters::parse(br);
        if (!res) return Expected<L3SystemInformationType1>::error(res.error());
        msg.mRACHControlParameters = std::move(res.value());
    }

    if (br.hasMore()) {
        msg.mHaveRestOctets = true;
        auto r = br.readField(8); if (!r) return Expected<L3SystemInformationType1>::error(r.error());
        msg.mRestOctet = static_cast<uint8_t>(r.value());
    }

    return Expected<L3SystemInformationType1>::hold(std::move(msg));
}

void L3SystemInformationType1::write(BitWriter& bw) const {
    mCellChannelDescription.write(bw);
    mRACHControlParameters.write(bw);
    if (mHaveRestOctets) {
        bw.writeField(mRestOctet, 8);
    }
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

// ── L3SystemInformationType1 Builder ───────────────────────────────────

L3SystemInformationType1 L3SystemInformationType1::Builder::build() const {
    L3SystemInformationType1 msg;
    msg.mCellChannelDescription = mCellChannelDescription;
    msg.mRACHControlParameters = mRACHControlParameters;
    msg.mHaveRestOctets = mHaveRestOctets;
    msg.mRestOctet = mRestOctet;
    return msg;
}

L3SystemInformationType1::Builder L3SystemInformationType1::builder() {
    return Builder{};
}

// ── L3SystemInformationType2 ───────────────────────────────────────────

Expected<L3SystemInformationType2> L3SystemInformationType2::parse(BitReader& br) {
    L3SystemInformationType2 msg;
    {
        auto res = L3BCCHFrequencyList::parse(br);
        if (!res) return Expected<L3SystemInformationType2>::error(res.error());
        msg.mBCCHFrequencyList = std::move(res.value());
    }
    {
        auto res = L3NCCPermitted::parse(br);
        if (!res) return Expected<L3SystemInformationType2>::error(res.error());
        msg.mNCCPermitted = std::move(res.value());
    }
    {
        auto res = L3RACHControlParameters::parse(br);
        if (!res) return Expected<L3SystemInformationType2>::error(res.error());
        msg.mRACHControlParameters = std::move(res.value());
    }
    return Expected<L3SystemInformationType2>::hold(std::move(msg));
}

void L3SystemInformationType2::write(BitWriter& bw) const {
    mBCCHFrequencyList.write(bw);
    mNCCPermitted.write(bw);
    mRACHControlParameters.write(bw);
}

void L3SystemInformationType2::text(std::ostream& os) const {
    os << "SystemInformationType2: ";
    mBCCHFrequencyList.text(os);
    os << " ";
    mNCCPermitted.text(os);
    os << " ";
    mRACHControlParameters.text(os);
}

// ── L3SystemInformationType2 Builder ───────────────────────────────────

L3SystemInformationType2 L3SystemInformationType2::Builder::build() const {
    L3SystemInformationType2 msg;
    msg.mBCCHFrequencyList = mBCCHFrequencyList;
    msg.mNCCPermitted = mNCCPermitted;
    msg.mRACHControlParameters = mRACHControlParameters;
    return msg;
}

L3SystemInformationType2::Builder L3SystemInformationType2::builder() {
    return Builder{};
}

// ── L3SystemInformationType2bis ────────────────────────────────────────

Expected<L3SystemInformationType2bis> L3SystemInformationType2bis::parse(BitReader& br) {
    L3SystemInformationType2bis msg;
    {
        auto res = L3BCCHFrequencyList::parse(br);
        if (!res) return Expected<L3SystemInformationType2bis>::error(res.error());
        msg.mBCCHFrequencyList = std::move(res.value());
    }
    {
        auto res = L3RACHControlParameters::parse(br);
        if (!res) return Expected<L3SystemInformationType2bis>::error(res.error());
        msg.mRACHControlParameters = std::move(res.value());
    }
    return Expected<L3SystemInformationType2bis>::hold(std::move(msg));
}

void L3SystemInformationType2bis::write(BitWriter& bw) const {
    mBCCHFrequencyList.write(bw);
    mRACHControlParameters.write(bw);
}

void L3SystemInformationType2bis::text(std::ostream& os) const {
    os << "SystemInformationType2bis: ";
    mBCCHFrequencyList.text(os);
    os << " ";
    mRACHControlParameters.text(os);
}

// ── L3SystemInformationType2bis Builder ────────────────────────────────

L3SystemInformationType2bis L3SystemInformationType2bis::Builder::build() const {
    L3SystemInformationType2bis msg;
    msg.mBCCHFrequencyList = mBCCHFrequencyList;
    msg.mRACHControlParameters = mRACHControlParameters;
    return msg;
}

L3SystemInformationType2bis::Builder L3SystemInformationType2bis::builder() {
    return Builder{};
}

// ── L3SystemInformationType2ter ────────────────────────────────────────

Expected<L3SystemInformationType2ter> L3SystemInformationType2ter::parse(BitReader& br) {
    L3SystemInformationType2ter msg;
    {
        auto res = L3BCCHFrequencyList::parse(br);
        if (!res) return Expected<L3SystemInformationType2ter>::error(res.error());
        msg.mBCCHFrequencyList = std::move(res.value());
    }
    return Expected<L3SystemInformationType2ter>::hold(std::move(msg));
}

void L3SystemInformationType2ter::write(BitWriter& bw) const {
    mBCCHFrequencyList.write(bw);
}

void L3SystemInformationType2ter::text(std::ostream& os) const {
    os << "SystemInformationType2ter: ";
    mBCCHFrequencyList.text(os);
}

// ── L3SystemInformationType2ter Builder ────────────────────────────────

L3SystemInformationType2ter L3SystemInformationType2ter::Builder::build() const {
    L3SystemInformationType2ter msg;
    msg.mBCCHFrequencyList = mBCCHFrequencyList;
    return msg;
}

L3SystemInformationType2ter::Builder L3SystemInformationType2ter::builder() {
    return Builder{};
}

// ── L3SystemInformationType3 ───────────────────────────────────────────

Expected<L3SystemInformationType3> L3SystemInformationType3::parse(BitReader& br) {
    L3SystemInformationType3 msg;
    { auto res = L3CellIdentity::parse(br); if (!res) return Expected<L3SystemInformationType3>::error(res.error()); msg.mCI = std::move(res.value()); }
    { auto res = L3LocationAreaIdentity::parse(br); if (!res) return Expected<L3SystemInformationType3>::error(res.error()); msg.mLAI = std::move(res.value()); }
    { auto res = L3ControlChannelDescription::parse(br); if (!res) return Expected<L3SystemInformationType3>::error(res.error()); msg.mControlChannelDescription = std::move(res.value()); }
    { auto res = L3CellOptionsBCCH::parse(br); if (!res) return Expected<L3SystemInformationType3>::error(res.error()); msg.mCellOptions = std::move(res.value()); }
    { auto res = L3CellSelectionParameters::parse(br); if (!res) return Expected<L3SystemInformationType3>::error(res.error()); msg.mCellSelectionParameters = std::move(res.value()); }
    { auto res = L3RACHControlParameters::parse(br); if (!res) return Expected<L3SystemInformationType3>::error(res.error()); msg.mRACHControlParameters = std::move(res.value()); }
    {
        L3SI3RestOctets rest;
        auto r = br.readField(1); if (!r) return Expected<L3SystemInformationType3>::error(r.error());
        if (r.value() != 0) {
            rest.mHaveSI3RestOctets = true;
            r = br.readField(1); if (!r) return Expected<L3SystemInformationType3>::error(r.error());
            if (r.value()) {
                rest.mHaveSelectionParameters = true;
                r = br.readField(1); if (!r) return Expected<L3SystemInformationType3>::error(r.error()); rest.mCBQ = r.value();
                r = br.readField(6); if (!r) return Expected<L3SystemInformationType3>::error(r.error()); rest.mCELL_RESELECT_OFFSET = r.value();
                r = br.readField(3); if (!r) return Expected<L3SystemInformationType3>::error(r.error()); rest.mTEMPORARY_OFFSET = r.value();
                r = br.readField(5); if (!r) return Expected<L3SystemInformationType3>::error(r.error()); rest.mPENALTY_TIME = r.value();
            }
            r = br.readField(4); if (!r) return Expected<L3SystemInformationType3>::error(r.error());
            r = br.readField(1); if (!r) return Expected<L3SystemInformationType3>::error(r.error());
            if (r.value()) {
                rest.mHaveGPRS = true;
                r = br.readField(3); if (!r) return Expected<L3SystemInformationType3>::error(r.error()); rest.mRA_COLOUR = r.value();
                r = br.readField(1); if (!r) return Expected<L3SystemInformationType3>::error(r.error());
            }
        }
        msg.mRestOctets = std::move(rest);
    }
    return Expected<L3SystemInformationType3>::hold(std::move(msg));
}

void L3SystemInformationType3::write(BitWriter& bw) const {
    mCI.write(bw);
    mLAI.write(bw);
    mControlChannelDescription.write(bw);
    mCellOptions.write(bw);
    mCellSelectionParameters.write(bw);
    mRACHControlParameters.write(bw);
    mRestOctets.write(bw);
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

// ── L3SystemInformationType3 Builder ───────────────────────────────────

L3SystemInformationType3 L3SystemInformationType3::Builder::build() const {
    L3SystemInformationType3 msg;
    msg.mCI = mCI;
    msg.mLAI = mLAI;
    msg.mControlChannelDescription = mControlChannelDescription;
    msg.mCellOptions = mCellOptions;
    msg.mCellSelectionParameters = mCellSelectionParameters;
    msg.mRACHControlParameters = mRACHControlParameters;
    msg.mRestOctets = mRestOctets;
    return msg;
}

L3SystemInformationType3::Builder L3SystemInformationType3::builder() {
    return Builder{};
}

// ── L3SystemInformationType4 ───────────────────────────────────────────

size_t L3SystemInformationType4::bodyLength() const {
    size_t len = mLAI.lengthV() + mCellSelectionParameters.lengthV() + mRACHControlParameters.lengthV();
    if (mHaveCBCH) len += 1 + mCBCHChannelDescription.lengthV();
    len += mRestOctets.lengthV();
    return len;
}

size_t L3SystemInformationType4::restOctetsLength() const {
    return mRestOctets.lengthV();
}

Expected<L3SystemInformationType4> L3SystemInformationType4::parse(BitReader& br) {
    L3SystemInformationType4 msg;
    { auto res = L3LocationAreaIdentity::parse(br); if (!res) return Expected<L3SystemInformationType4>::error(res.error()); msg.mLAI = std::move(res.value()); }
    { auto res = L3CellSelectionParameters::parse(br); if (!res) return Expected<L3SystemInformationType4>::error(res.error()); msg.mCellSelectionParameters = std::move(res.value()); }
    { auto res = L3RACHControlParameters::parse(br); if (!res) return Expected<L3SystemInformationType4>::error(res.error()); msg.mRACHControlParameters = std::move(res.value()); }

    if (br.hasMore()) {
        unsigned peek = br.peekField(8);
        if (peek == 0x64) {
            { auto _ = br.readField(8); if (!_) return Expected<L3SystemInformationType4>::error(_.error()); }
            msg.mHaveCBCH = true;
            auto res = L3ChannelDescription::parse(br);
            if (!res) return Expected<L3SystemInformationType4>::error(res.error());
            msg.mCBCHChannelDescription = std::move(res.value());
        }
    }

    {
        L3SIType4RestOctets rest;
        auto r = br.readField(2); if (!r) return Expected<L3SystemInformationType4>::error(r.error());
        r = br.readField(1); if (!r) return Expected<L3SystemInformationType4>::error(r.error());
        if (r.value()) {
            rest.mHaveGPRS = true;
            r = br.readField(3); if (!r) return Expected<L3SystemInformationType4>::error(r.error()); rest.mRA_COLOUR = r.value();
            r = br.readField(1); if (!r) return Expected<L3SystemInformationType4>::error(r.error());
        }
        msg.mRestOctets = std::move(rest);
    }

    return Expected<L3SystemInformationType4>::hold(std::move(msg));
}

void L3SystemInformationType4::write(BitWriter& bw) const {
    mLAI.write(bw);
    mCellSelectionParameters.write(bw);
    mRACHControlParameters.write(bw);
    if (mHaveCBCH) {
        bw.writeField(0x64, 8);
        mCBCHChannelDescription.write(bw);
    }
    mRestOctets.write(bw);
}

void L3SystemInformationType4::text(std::ostream& os) const {
    os << "SystemInformationType4: ";
    mLAI.text(os);
    os << " ";
    mCellSelectionParameters.text(os);
    os << " ";
    mRACHControlParameters.text(os);
}

// ── L3SystemInformationType4 Builder ───────────────────────────────────

L3SystemInformationType4 L3SystemInformationType4::Builder::build() const {
    L3SystemInformationType4 msg;
    msg.mLAI = mLAI;
    msg.mCellSelectionParameters = mCellSelectionParameters;
    msg.mRACHControlParameters = mRACHControlParameters;
    msg.mHaveCBCH = mHaveCBCH;
    msg.mCBCHChannelDescription = mCBCHChannelDescription;
    msg.mRestOctets = mRestOctets;
    return msg;
}

L3SystemInformationType4::Builder L3SystemInformationType4::builder() {
    return Builder{};
}

// ── L3SystemInformationType5 ───────────────────────────────────────────

Expected<L3SystemInformationType5> L3SystemInformationType5::parse(BitReader& br) {
    L3SystemInformationType5 msg;
    {
        auto res = L3BCCHFrequencyList::parse(br);
        if (!res) return Expected<L3SystemInformationType5>::error(res.error());
        msg.mBCCHFrequencyList = std::move(res.value());
    }
    return Expected<L3SystemInformationType5>::hold(std::move(msg));
}

void L3SystemInformationType5::write(BitWriter& bw) const {
    mBCCHFrequencyList.write(bw);
}

void L3SystemInformationType5::text(std::ostream& os) const {
    os << "SystemInformationType5: ";
    mBCCHFrequencyList.text(os);
}

// ── L3SystemInformationType5 Builder ───────────────────────────────────

L3SystemInformationType5 L3SystemInformationType5::Builder::build() const {
    L3SystemInformationType5 msg;
    msg.mBCCHFrequencyList = mBCCHFrequencyList;
    return msg;
}

L3SystemInformationType5::Builder L3SystemInformationType5::builder() {
    return Builder{};
}

// ── L3SystemInformationType5bis ────────────────────────────────────────

Expected<L3SystemInformationType5bis> L3SystemInformationType5bis::parse(BitReader& br) {
    L3SystemInformationType5bis msg;
    {
        auto res = L3BCCHFrequencyList::parse(br);
        if (!res) return Expected<L3SystemInformationType5bis>::error(res.error());
        msg.mBCCHFrequencyList = std::move(res.value());
    }
    return Expected<L3SystemInformationType5bis>::hold(std::move(msg));
}

void L3SystemInformationType5bis::write(BitWriter& bw) const {
    mBCCHFrequencyList.write(bw);
}

void L3SystemInformationType5bis::text(std::ostream& os) const {
    os << "SystemInformationType5bis: ";
    mBCCHFrequencyList.text(os);
}

// ── L3SystemInformationType5bis Builder ────────────────────────────────

L3SystemInformationType5bis L3SystemInformationType5bis::Builder::build() const {
    L3SystemInformationType5bis msg;
    msg.mBCCHFrequencyList = mBCCHFrequencyList;
    return msg;
}

L3SystemInformationType5bis::Builder L3SystemInformationType5bis::builder() {
    return Builder{};
}

// ── L3SystemInformationType5ter ────────────────────────────────────────

Expected<L3SystemInformationType5ter> L3SystemInformationType5ter::parse(BitReader& br) {
    L3SystemInformationType5ter msg;
    {
        auto res = L3BCCHFrequencyList::parse(br);
        if (!res) return Expected<L3SystemInformationType5ter>::error(res.error());
        msg.mBCCHFrequencyList = std::move(res.value());
    }
    return Expected<L3SystemInformationType5ter>::hold(std::move(msg));
}

void L3SystemInformationType5ter::write(BitWriter& bw) const {
    mBCCHFrequencyList.write(bw);
}

void L3SystemInformationType5ter::text(std::ostream& os) const {
    os << "SystemInformationType5ter: ";
    mBCCHFrequencyList.text(os);
}

// ── L3SystemInformationType5ter Builder ────────────────────────────────

L3SystemInformationType5ter L3SystemInformationType5ter::Builder::build() const {
    L3SystemInformationType5ter msg;
    msg.mBCCHFrequencyList = mBCCHFrequencyList;
    return msg;
}

L3SystemInformationType5ter::Builder L3SystemInformationType5ter::builder() {
    return Builder{};
}

// ── L3SystemInformationType6 ───────────────────────────────────────────

Expected<L3SystemInformationType6> L3SystemInformationType6::parse(BitReader& br) {
    L3SystemInformationType6 msg;
    { auto res = L3CellIdentity::parse(br); if (!res) return Expected<L3SystemInformationType6>::error(res.error()); msg.mCI = std::move(res.value()); }
    { auto res = L3LocationAreaIdentity::parse(br); if (!res) return Expected<L3SystemInformationType6>::error(res.error()); msg.mLAI = std::move(res.value()); }
    { auto res = L3CellOptionsSACCH::parse(br); if (!res) return Expected<L3SystemInformationType6>::error(res.error()); msg.mCellOptions = std::move(res.value()); }
    { auto res = L3NCCPermitted::parse(br); if (!res) return Expected<L3SystemInformationType6>::error(res.error()); msg.mNCCPermitted = std::move(res.value()); }
    return Expected<L3SystemInformationType6>::hold(std::move(msg));
}

void L3SystemInformationType6::write(BitWriter& bw) const {
    mCI.write(bw);
    mLAI.write(bw);
    mCellOptions.write(bw);
    mNCCPermitted.write(bw);
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

// ── L3SystemInformationType6 Builder ───────────────────────────────────

L3SystemInformationType6 L3SystemInformationType6::Builder::build() const {
    L3SystemInformationType6 msg;
    msg.mCI = mCI;
    msg.mLAI = mLAI;
    msg.mCellOptions = mCellOptions;
    msg.mNCCPermitted = mNCCPermitted;
    return msg;
}

L3SystemInformationType6::Builder L3SystemInformationType6::builder() {
    return Builder{};
}

// ── L3SystemInformationType7 ───────────────────────────────────────────

size_t L3SystemInformationType7::bodyLength() const {
    size_t len = 1 + mRACHControl.lengthV();
    for (const auto& ch : mCellChannelDescriptions) {
        len += 1 + ch.lengthV();
    }
    return len;
}

Expected<L3SystemInformationType7> L3SystemInformationType7::parse(BitReader& br) {
    L3SystemInformationType7 msg;
    {
        auto ieiR = br.readField(8); if (!ieiR) return Expected<L3SystemInformationType7>::error(ieiR.error());
        if (ieiR.value() != 0x28) {
            return Expected<L3SystemInformationType7>::error(ParseError{ParseError::Code::InvalidIE, "SI7: expected IEI 0x28"});
        }
        auto res = L3RACHControlParameters::parse(br);
        if (!res) return Expected<L3SystemInformationType7>::error(res.error());
        msg.mRACHControl = std::move(res.value());
    }

    while (br.hasMore()) {
        unsigned peek = br.peekField(8);
        if (peek == 0x21) {
            { auto _ = br.readField(8); if (!_) return Expected<L3SystemInformationType7>::error(_.error()); }
            auto res = L3CellChannelDescription::parse(br);
            if (!res) return Expected<L3SystemInformationType7>::error(res.error());
            msg.mCellChannelDescriptions.push_back(std::move(res.value()));
        } else {
            break;
        }
    }

    return Expected<L3SystemInformationType7>::hold(std::move(msg));
}

void L3SystemInformationType7::write(BitWriter& bw) const {
    bw.writeField(0x28, 8);
    mRACHControl.write(bw);
    for (const auto& ch : mCellChannelDescriptions) {
        bw.writeField(0x21, 8);
        ch.write(bw);
    }
}

void L3SystemInformationType7::text(std::ostream& os) const {
    os << "SystemInformationType7: ";
    mRACHControl.text(os);
    os << " cells=" << mCellChannelDescriptions.size();
}

// ── L3SystemInformationType7 Builder ───────────────────────────────────

L3SystemInformationType7 L3SystemInformationType7::Builder::build() const {
    L3SystemInformationType7 msg;
    msg.mRACHControl = mRACHControl;
    msg.mCellChannelDescriptions = mCellChannelDescriptions;
    return msg;
}

L3SystemInformationType7::Builder L3SystemInformationType7::builder() {
    return Builder{};
}

// ── L3SystemInformationType8 ───────────────────────────────────────────

size_t L3SystemInformationType8::bodyLength() const {
    size_t len = 1 + mNCCPermitted.lengthV() + 1 + mRACHControl.lengthV();
    for (const auto& ch : mCellChannelDescriptions) {
        len += 1 + ch.lengthV();
    }
    return len;
}

Expected<L3SystemInformationType8> L3SystemInformationType8::parse(BitReader& br) {
    L3SystemInformationType8 msg;
    {
        auto ieiR = br.readField(8); if (!ieiR) return Expected<L3SystemInformationType8>::error(ieiR.error());
        if (ieiR.value() != 0x27) {
            return Expected<L3SystemInformationType8>::error(ParseError{ParseError::Code::InvalidIE, "SI8: expected IEI 0x27"});
        }
        auto res = L3NCCPermitted::parse(br);
        if (!res) return Expected<L3SystemInformationType8>::error(res.error());
        msg.mNCCPermitted = std::move(res.value());
    }
    {
        auto ieiR = br.readField(8); if (!ieiR) return Expected<L3SystemInformationType8>::error(ieiR.error());
        if (ieiR.value() != 0x28) {
            return Expected<L3SystemInformationType8>::error(ParseError{ParseError::Code::InvalidIE, "SI8: expected IEI 0x28"});
        }
        auto res = L3RACHControlParameters::parse(br);
        if (!res) return Expected<L3SystemInformationType8>::error(res.error());
        msg.mRACHControl = std::move(res.value());
    }

    while (br.hasMore()) {
        unsigned peek = br.peekField(8);
        if (peek == 0x21) {
            { auto _ = br.readField(8); if (!_) return Expected<L3SystemInformationType8>::error(_.error()); }
            auto res = L3CellChannelDescription::parse(br);
            if (!res) return Expected<L3SystemInformationType8>::error(res.error());
            msg.mCellChannelDescriptions.push_back(std::move(res.value()));
        } else {
            break;
        }
    }

    return Expected<L3SystemInformationType8>::hold(std::move(msg));
}

void L3SystemInformationType8::write(BitWriter& bw) const {
    bw.writeField(0x27, 8);
    mNCCPermitted.write(bw);
    bw.writeField(0x28, 8);
    mRACHControl.write(bw);
    for (const auto& ch : mCellChannelDescriptions) {
        bw.writeField(0x21, 8);
        ch.write(bw);
    }
}

void L3SystemInformationType8::text(std::ostream& os) const {
    os << "SystemInformationType8: ";
    mNCCPermitted.text(os);
    os << " ";
    mRACHControl.text(os);
    os << " cells=" << mCellChannelDescriptions.size();
}

// ── L3SystemInformationType8 Builder ───────────────────────────────────

L3SystemInformationType8 L3SystemInformationType8::Builder::build() const {
    L3SystemInformationType8 msg;
    msg.mNCCPermitted = mNCCPermitted;
    msg.mRACHControl = mRACHControl;
    msg.mCellChannelDescriptions = mCellChannelDescriptions;
    return msg;
}

L3SystemInformationType8::Builder L3SystemInformationType8::builder() {
    return Builder{};
}

// ── L3SystemInformationType9 ───────────────────────────────────────────

Expected<L3SystemInformationType9> L3SystemInformationType9::parse(BitReader& br) {
    L3SystemInformationType9 msg;
    { auto res = L3CellIdentity::parse(br); if (!res) return Expected<L3SystemInformationType9>::error(res.error()); msg.mCI = std::move(res.value()); }
    { auto res = L3CellSelectionParameters::parse(br); if (!res) return Expected<L3SystemInformationType9>::error(res.error()); msg.mCellSelectionParameters = std::move(res.value()); }
    { auto res = L3CellOptionsBCCH::parse(br); if (!res) return Expected<L3SystemInformationType9>::error(res.error()); msg.mCellOptions = std::move(res.value()); }
    return Expected<L3SystemInformationType9>::hold(std::move(msg));
}

void L3SystemInformationType9::write(BitWriter& bw) const {
    mCI.write(bw);
    mCellSelectionParameters.write(bw);
    mCellOptions.write(bw);
}

void L3SystemInformationType9::text(std::ostream& os) const {
    os << "SystemInformationType9: ";
    mCI.text(os);
    os << " ";
    mCellSelectionParameters.text(os);
    os << " ";
    mCellOptions.text(os);
}

// ── L3SystemInformationType9 Builder ───────────────────────────────────

L3SystemInformationType9 L3SystemInformationType9::Builder::build() const {
    L3SystemInformationType9 msg;
    msg.mCI = mCI;
    msg.mCellSelectionParameters = mCellSelectionParameters;
    msg.mCellOptions = mCellOptions;
    return msg;
}

L3SystemInformationType9::Builder L3SystemInformationType9::builder() {
    return Builder{};
}

// ── L3SystemInformationType13 ──────────────────────────────────────────

Expected<L3SystemInformationType13> L3SystemInformationType13::parse(BitReader& br) {
    L3SystemInformationType13 msg;
    auto r = br.readField(1); if (!r) return Expected<L3SystemInformationType13>::error(r.error());
    r = br.readField(3); if (!r) return Expected<L3SystemInformationType13>::error(r.error());
    r = br.readField(4); if (!r) return Expected<L3SystemInformationType13>::error(r.error());
    r = br.readField(1); if (!r) return Expected<L3SystemInformationType13>::error(r.error());
    r = br.readField(1); if (!r) return Expected<L3SystemInformationType13>::error(r.error());
    r = br.readField(8); if (!r) return Expected<L3SystemInformationType13>::error(r.error()); msg.mRestOctets.mRAC = r.value();
    r = br.readField(1); if (!r) return Expected<L3SystemInformationType13>::error(r.error()); msg.mRestOctets.mSPGC_CCCH_SUP = r.value() != 0;
    r = br.readField(3); if (!r) return Expected<L3SystemInformationType13>::error(r.error()); msg.mRestOctets.mPRIORITY_ACCESS_THR = r.value();
    r = br.readField(2); if (!r) return Expected<L3SystemInformationType13>::error(r.error()); msg.mRestOctets.mNETWORK_CONTROL_ORDER = r.value();
    return Expected<L3SystemInformationType13>::hold(std::move(msg));
}

void L3SystemInformationType13::write(BitWriter& bw) const {
    mRestOctets.write(bw);
}

void L3SystemInformationType13::text(std::ostream& os) const {
    os << "SystemInformationType13: ";
    mRestOctets.text(os);
}

// ── L3SystemInformationType13 Builder ──────────────────────────────────

L3SystemInformationType13 L3SystemInformationType13::Builder::build() const {
    L3SystemInformationType13 msg;
    msg.mRestOctets = mRestOctets;
    return msg;
}

L3SystemInformationType13::Builder L3SystemInformationType13::builder() {
    return Builder{};
}

// ── L3SystemInformationType16 ──────────────────────────────────────────

Expected<L3SystemInformationType16> L3SystemInformationType16::parse(BitReader& br) {
    L3SystemInformationType16 msg;
    { auto res = L3CellIdentity::parse(br); if (!res) return Expected<L3SystemInformationType16>::error(res.error()); msg.mCI = std::move(res.value()); }
    { auto res = L3CellSelectionParameters::parse(br); if (!res) return Expected<L3SystemInformationType16>::error(res.error()); msg.mCellSelectionParameters = std::move(res.value()); }
    { auto res = L3CellOptionsBCCH::parse(br); if (!res) return Expected<L3SystemInformationType16>::error(res.error()); msg.mCellOptions = std::move(res.value()); }
    return Expected<L3SystemInformationType16>::hold(std::move(msg));
}

void L3SystemInformationType16::write(BitWriter& bw) const {
    mCI.write(bw);
    mCellSelectionParameters.write(bw);
    mCellOptions.write(bw);
}

void L3SystemInformationType16::text(std::ostream& os) const {
    os << "SystemInformationType16: ";
    mCI.text(os);
    os << " ";
    mCellSelectionParameters.text(os);
    os << " ";
    mCellOptions.text(os);
}

// ── L3SystemInformationType16 Builder ──────────────────────────────────

L3SystemInformationType16 L3SystemInformationType16::Builder::build() const {
    L3SystemInformationType16 msg;
    msg.mCI = mCI;
    msg.mCellSelectionParameters = mCellSelectionParameters;
    msg.mCellOptions = mCellOptions;
    return msg;
}

L3SystemInformationType16::Builder L3SystemInformationType16::builder() {
    return Builder{};
}

// ── L3SystemInformationType17 ──────────────────────────────────────────

size_t L3SystemInformationType17::bodyLength() const {
    size_t len = 1 + mRACHControl.lengthV();
    for (const auto& ch : mCellChannelDescriptions) {
        len += 1 + ch.lengthV();
    }
    return len;
}

Expected<L3SystemInformationType17> L3SystemInformationType17::parse(BitReader& br) {
    L3SystemInformationType17 msg;
    {
        auto ieiR = br.readField(8); if (!ieiR) return Expected<L3SystemInformationType17>::error(ieiR.error());
        if (ieiR.value() != 0x28) {
            return Expected<L3SystemInformationType17>::error(ParseError{ParseError::Code::InvalidIE, "SI17: expected IEI 0x28"});
        }
        auto res = L3RACHControlParameters::parse(br);
        if (!res) return Expected<L3SystemInformationType17>::error(res.error());
        msg.mRACHControl = std::move(res.value());
    }

    while (br.hasMore()) {
        unsigned peek = br.peekField(8);
        if (peek == 0x21) {
            { auto _ = br.readField(8); if (!_) return Expected<L3SystemInformationType17>::error(_.error()); }
            auto res = L3CellChannelDescription::parse(br);
            if (!res) return Expected<L3SystemInformationType17>::error(res.error());
            msg.mCellChannelDescriptions.push_back(std::move(res.value()));
        } else {
            break;
        }
    }

    return Expected<L3SystemInformationType17>::hold(std::move(msg));
}

void L3SystemInformationType17::write(BitWriter& bw) const {
    bw.writeField(0x28, 8);
    mRACHControl.write(bw);
    for (const auto& ch : mCellChannelDescriptions) {
        bw.writeField(0x21, 8);
        ch.write(bw);
    }
}

void L3SystemInformationType17::text(std::ostream& os) const {
    os << "SystemInformationType17: ";
    mRACHControl.text(os);
    os << " cells=" << mCellChannelDescriptions.size();
}

// ── L3SystemInformationType17 Builder ──────────────────────────────────

L3SystemInformationType17 L3SystemInformationType17::Builder::build() const {
    L3SystemInformationType17 msg;
    msg.mRACHControl = mRACHControl;
    msg.mCellChannelDescriptions = mCellChannelDescriptions;
    return msg;
}

L3SystemInformationType17::Builder L3SystemInformationType17::builder() {
    return Builder{};
}

// ── L3ImmediateAssignment ──────────────────────────────────────────────

size_t L3ImmediateAssignment::bodyLength() const {
    size_t len = 1 + 1 + mRequestReference.lengthV() + mChannelDescription.lengthV() + mTimingAdvance.lengthV();
    if (!mMobileAllocation.empty()) len += 1 + mMobileAllocation.size();
    if (mStartTimePresent) len += 1 + 3;
    return len;
}

Expected<L3ImmediateAssignment> L3ImmediateAssignment::parse(BitReader& br) {
    L3ImmediateAssignment msg;

    { auto res = L3DedicatedModeOrTBF::parse(br); if (!res) return Expected<L3ImmediateAssignment>::error(res.error()); msg.mDedicatedModeOrTBF = std::move(res.value()); }
    { auto res = L3PageMode::parse(br); if (!res) return Expected<L3ImmediateAssignment>::error(res.error()); msg.mPageMode = std::move(res.value()); }
    { auto res = L3RequestReference::parse(br); if (!res) return Expected<L3ImmediateAssignment>::error(res.error()); msg.mRequestReference = std::move(res.value()); }
    { auto res = L3ChannelDescription::parse(br); if (!res) return Expected<L3ImmediateAssignment>::error(res.error()); msg.mChannelDescription = std::move(res.value()); }
    { auto res = L3TimingAdvance::parse(br); if (!res) return Expected<L3ImmediateAssignment>::error(res.error()); msg.mTimingAdvance = std::move(res.value()); }

    if (br.hasMore()) {
        auto lenR = br.readField(8); if (!lenR) return Expected<L3ImmediateAssignment>::error(lenR.error());
        size_t maLen = lenR.value();
        if (maLen > 0) {
            msg.mMobileAllocation.resize(maLen);
            for (size_t i = 0; i < maLen; ++i) {
                auto r2 = br.readField(8); if (!r2) return Expected<L3ImmediateAssignment>::error(r2.error());
                msg.mMobileAllocation[i] = static_cast<uint8_t>(r2.value());
            }
        }
    }

    if (br.hasMore()) {
        unsigned peek = br.peekField(8);
        if (peek == 0x7c) {
            { auto _ = br.readField(8); if (!_) return Expected<L3ImmediateAssignment>::error(_.error()); }
            msg.mStartTimePresent = true;
            auto r2 = br.readField(23); if (!r2) return Expected<L3ImmediateAssignment>::error(r2.error());
            msg.mStartTimeFrame = r2.value();
        }
    }

    return Expected<L3ImmediateAssignment>::hold(std::move(msg));
}

void L3ImmediateAssignment::write(BitWriter& bw) const {
    mDedicatedModeOrTBF.write(bw);
    mPageMode.write(bw);
    mRequestReference.write(bw);
    mChannelDescription.write(bw);
    mTimingAdvance.write(bw);

    bw.writeField(static_cast<uint32_t>(mMobileAllocation.size()), 8);
    for (const auto& b : mMobileAllocation) {
        bw.writeField(b, 8);
    }

    if (mStartTimePresent) {
        bw.writeField(0x7c, 8);
        bw.writeField(mStartTimeFrame, 23);
    }
}

void L3ImmediateAssignment::text(std::ostream& os) const {
    os << "ImmediateAssignment: ";
    mChannelDescription.text(os);
    os << " TA=";
    mTimingAdvance.text(os);
}

L3ImmediateAssignment L3ImmediateAssignment::Builder::build() const {
    L3ImmediateAssignment msg;
    msg.mPageMode = mPageMode;
    msg.mDedicatedModeOrTBF = mDedicatedModeOrTBF;
    msg.mRequestReference = mRequestReference;
    msg.mChannelDescription = mChannelDescription;
    msg.mTimingAdvance = mTimingAdvance;
    msg.mMobileAllocation = mMobileAllocation;
    msg.mStartTimePresent = mStartTimePresent;
    msg.mStartTimeFrame = mStartTimeFrame;
    return msg;
}

L3ImmediateAssignment::Builder L3ImmediateAssignment::builder() {
    return Builder{};
}

// ── L3ImmediateAssignmentExtended ──────────────────────────────────────

size_t L3ImmediateAssignmentExtended::bodyLength() const {
    size_t len = 1 + 1 + mRequestReference.lengthV() + mChannelDescription.lengthV() + mTimingAdvance.lengthV();
    if (!mMobileAllocation.empty()) len += 1 + mMobileAllocation.size();
    if (mStartTimePresent) len += 1 + 3;
    if (mHaveAdditionalChannel) len += mAdditionalChannel.lengthV();
    return len;
}

Expected<L3ImmediateAssignmentExtended> L3ImmediateAssignmentExtended::parse(BitReader& br) {
    L3ImmediateAssignmentExtended msg;

    { auto res = L3DedicatedModeOrTBF::parse(br); if (!res) return Expected<L3ImmediateAssignmentExtended>::error(res.error()); msg.mDedicatedModeOrTBF = std::move(res.value()); }
    { auto res = L3PageMode::parse(br); if (!res) return Expected<L3ImmediateAssignmentExtended>::error(res.error()); msg.mPageMode = std::move(res.value()); }
    { auto res = L3RequestReference::parse(br); if (!res) return Expected<L3ImmediateAssignmentExtended>::error(res.error()); msg.mRequestReference = std::move(res.value()); }
    { auto res = L3ChannelDescription::parse(br); if (!res) return Expected<L3ImmediateAssignmentExtended>::error(res.error()); msg.mChannelDescription = std::move(res.value()); }
    { auto res = L3TimingAdvance::parse(br); if (!res) return Expected<L3ImmediateAssignmentExtended>::error(res.error()); msg.mTimingAdvance = std::move(res.value()); }

    if (br.hasMore()) {
        auto lenR = br.readField(8); if (!lenR) return Expected<L3ImmediateAssignmentExtended>::error(lenR.error());
        size_t maLen = lenR.value();
        if (maLen > 0) {
            msg.mMobileAllocation.resize(maLen);
            for (size_t i = 0; i < maLen; ++i) {
                auto r2 = br.readField(8); if (!r2) return Expected<L3ImmediateAssignmentExtended>::error(r2.error());
                msg.mMobileAllocation[i] = static_cast<uint8_t>(r2.value());
            }
        }
    }

    if (br.hasMore()) {
        unsigned peek = br.peekField(8);
        if (peek == 0x7c) {
            { auto _ = br.readField(8); if (!_) return Expected<L3ImmediateAssignmentExtended>::error(_.error()); }
            msg.mStartTimePresent = true;
            auto r = br.readField(23); if (!r) return Expected<L3ImmediateAssignmentExtended>::error(r.error());
            msg.mStartTimeFrame = r.value();
        }
    }

    if (br.hasMore()) {
        msg.mHaveAdditionalChannel = true;
        auto res = L3AdditionalChannelDescription::parse(br);
        if (!res) return Expected<L3ImmediateAssignmentExtended>::error(res.error());
        msg.mAdditionalChannel = std::move(res.value());
    }

    return Expected<L3ImmediateAssignmentExtended>::hold(std::move(msg));
}

void L3ImmediateAssignmentExtended::write(BitWriter& bw) const {
    mDedicatedModeOrTBF.write(bw);
    mPageMode.write(bw);
    mRequestReference.write(bw);
    mChannelDescription.write(bw);
    mTimingAdvance.write(bw);

    bw.writeField(static_cast<uint32_t>(mMobileAllocation.size()), 8);
    for (const auto& b : mMobileAllocation) {
        bw.writeField(b, 8);
    }

    if (mStartTimePresent) {
        bw.writeField(0x7c, 8);
        bw.writeField(mStartTimeFrame, 23);
    }

    if (mHaveAdditionalChannel) {
        mAdditionalChannel.write(bw);
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

L3ImmediateAssignmentExtended L3ImmediateAssignmentExtended::Builder::build() const {
    L3ImmediateAssignmentExtended msg;
    msg.mPageMode = mPageMode;
    msg.mDedicatedModeOrTBF = mDedicatedModeOrTBF;
    msg.mRequestReference = mRequestReference;
    msg.mChannelDescription = mChannelDescription;
    msg.mTimingAdvance = mTimingAdvance;
    msg.mMobileAllocation = mMobileAllocation;
    msg.mStartTimePresent = mStartTimePresent;
    msg.mStartTimeFrame = mStartTimeFrame;
    msg.mHaveAdditionalChannel = mHaveAdditionalChannel;
    msg.mAdditionalChannel = mAdditionalChannel;
    return msg;
}

L3ImmediateAssignmentExtended::Builder L3ImmediateAssignmentExtended::builder() {
    return Builder{};
}

// ── L3ImmediateAssignmentReject ────────────────────────────────────────

L3ImmediateAssignmentReject::L3ImmediateAssignmentReject(unsigned waitSeconds)
    : mFeatureIndicator(0), mPageMode(0), mWaitIndication(waitSeconds) {}

size_t L3ImmediateAssignmentReject::bodyLength() const {
    return 1 + static_cast<size_t>(mRequestReferences.size()) * 4;
}

Expected<L3ImmediateAssignmentReject> L3ImmediateAssignmentReject::parse(BitReader& br) {
    L3ImmediateAssignmentReject msg;
    auto r = br.readField(4); if (!r) return Expected<L3ImmediateAssignmentReject>::error(r.error());
    msg.mFeatureIndicator = r.value();
    r = br.readField(4); if (!r) return Expected<L3ImmediateAssignmentReject>::error(r.error());
    msg.mPageMode = r.value();

    for (int i = 0; i < 4; ++i) {
        if (!br.hasMore()) break;
        auto res = L3RequestReference::parse(br);
        if (!res) return Expected<L3ImmediateAssignmentReject>::error(res.error());
        msg.mRequestReferences.push_back(std::move(res.value()));
        r = br.readField(8); if (!r) return Expected<L3ImmediateAssignmentReject>::error(r.error());
        msg.mWaitIndications.push_back(r.value());
    }

    if (msg.mRequestReferences.empty()) {
        msg.mWaitIndication = 0;
    } else {
        msg.mWaitIndication = msg.mWaitIndications.empty() ? 0 : msg.mWaitIndications.back();
    }

    return Expected<L3ImmediateAssignmentReject>::hold(std::move(msg));
}

void L3ImmediateAssignmentReject::write(BitWriter& bw) const {
    bw.writeField(mFeatureIndicator & 0x0F, 4);
    bw.writeField(mPageMode & 0x0F, 4);
    for (size_t i = 0; i < mRequestReferences.size(); ++i) {
        mRequestReferences[i].write(bw);
        bw.writeField(i < mWaitIndications.size() ? mWaitIndications[i] : mWaitIndication, 8);
    }
}

void L3ImmediateAssignmentReject::text(std::ostream& os) const {
    os << "ImmediateAssignmentReject: pageMode=" << mPageMode
       << " T3122=" << mWaitIndication;
    os << " requestReferences=(" << mRequestReferences.size() << ")";
}

L3ImmediateAssignmentReject L3ImmediateAssignmentReject::Builder::build() const {
    L3ImmediateAssignmentReject msg;
    msg.mFeatureIndicator = mFeatureIndicator;
    msg.mPageMode = mPageMode;
    msg.mRequestReferences = mRequestReferences;
    msg.mWaitIndications = mWaitIndications;
    msg.mWaitIndication = mWaitIndication;
    return msg;
}

L3ImmediateAssignmentReject::Builder L3ImmediateAssignmentReject::builder() {
    return Builder{};
}

L3ImmediateAssignmentReject::Builder& L3ImmediateAssignmentReject::Builder::addWaitIndication(
    L3RequestReference ref, unsigned waitSeconds) {
    mRequestReferences.push_back(std::move(ref));
    mWaitIndications.push_back(waitSeconds);
    return *this;
}

// ── L3AdditionalAssignment ─────────────────────────────────────────────

size_t L3AdditionalAssignment::bodyLength() const {
    size_t len = mAdditionalChannel.lengthV();
    if (mHavePowerCommand) len += mPowerCommand.lengthV();
    return len;
}

Expected<L3AdditionalAssignment> L3AdditionalAssignment::parse(BitReader& br) {
    L3AdditionalAssignment msg;
    {
        auto res = L3AdditionalChannelDescription::parse(br);
        if (!res) return Expected<L3AdditionalAssignment>::error(res.error());
        msg.mAdditionalChannel = std::move(res.value());
    }
    if (br.hasMore()) {
        msg.mHavePowerCommand = true;
        auto res = L3PowerCommand::parse(br);
        if (!res) return Expected<L3AdditionalAssignment>::error(res.error());
        msg.mPowerCommand = std::move(res.value());
    }
    return Expected<L3AdditionalAssignment>::hold(std::move(msg));
}

void L3AdditionalAssignment::write(BitWriter& bw) const {
    mAdditionalChannel.write(bw);
    if (mHavePowerCommand) {
        mPowerCommand.write(bw);
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

L3AdditionalAssignment L3AdditionalAssignment::Builder::build() const {
    L3AdditionalAssignment msg;
    msg.mAdditionalChannel = mAdditionalChannel;
    msg.mHavePowerCommand = mHavePowerCommand;
    msg.mPowerCommand = mPowerCommand;
    return msg;
}

L3AdditionalAssignment::Builder L3AdditionalAssignment::builder() {
    return Builder{};
}

// ── L3PhysicalInformation ──────────────────────────────────────────────

Expected<L3PhysicalInformation> L3PhysicalInformation::parse(BitReader& br) {
    L3PhysicalInformation msg;
    {
        auto res = L3TimingAdvance::parse(br);
        if (!res) return Expected<L3PhysicalInformation>::error(res.error());
        msg.mTA = std::move(res.value());
    }
    return Expected<L3PhysicalInformation>::hold(std::move(msg));
}

void L3PhysicalInformation::write(BitWriter& bw) const {
    mTA.write(bw);
}

void L3PhysicalInformation::text(std::ostream& os) const {
    os << "PhysicalInformation: ";
    mTA.text(os);
}

L3PhysicalInformation L3PhysicalInformation::Builder::build() const {
    L3PhysicalInformation msg;
    msg.mTA = mTA;
    return msg;
}

L3PhysicalInformation::Builder L3PhysicalInformation::builder() {
    return Builder{};
}

// ── L3HandoverCommand ──────────────────────────────────────────────────

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

size_t L3HandoverCommand::bodyLength() const {
    return mCellDescription.lengthV() +
           mChannelDescriptionAfter.lengthV() +
           mHandoverReference.lengthV() +
           mPowerCommandAccessType.lengthV() +
           mSynchronizationIndication.lengthV();
}

Expected<L3HandoverCommand> L3HandoverCommand::parse(BitReader& br) {
    L3HandoverCommand msg;
    { auto res = L3CellDescription::parse(br); if (!res) return Expected<L3HandoverCommand>::error(res.error()); msg.mCellDescription = std::move(res.value()); }
    { auto res = L3ChannelDescription2::parse(br); if (!res) return Expected<L3HandoverCommand>::error(res.error()); msg.mChannelDescriptionAfter = std::move(res.value()); }
    { auto res = L3HandoverReference::parse(br); if (!res) return Expected<L3HandoverCommand>::error(res.error()); msg.mHandoverReference = std::move(res.value()); }
    { auto res = L3PowerCommandAndAccessType::parse(br); if (!res) return Expected<L3HandoverCommand>::error(res.error()); msg.mPowerCommandAccessType = std::move(res.value()); }
    { auto res = L3SynchronizationIndication::parse(br); if (!res) return Expected<L3HandoverCommand>::error(res.error()); msg.mSynchronizationIndication = std::move(res.value()); }
    return Expected<L3HandoverCommand>::hold(std::move(msg));
}

void L3HandoverCommand::write(BitWriter& bw) const {
    mCellDescription.write(bw);
    mChannelDescriptionAfter.write(bw);
    mHandoverReference.write(bw);
    mPowerCommandAccessType.write(bw);
    mSynchronizationIndication.write(bw);
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

// ── L3SynchronizationChannelInformation ────────────────────────────────

Expected<L3SynchronizationChannelInformation> L3SynchronizationChannelInformation::parse(BitReader& br) {
    L3SynchronizationChannelInformation msg;
    { auto res = L3CellIdentity::parse(br); if (!res) return Expected<L3SynchronizationChannelInformation>::error(res.error()); msg.mCellIdentity = std::move(res.value()); }
    { auto res = L3LocationAreaIdentity::parse(br); if (!res) return Expected<L3SynchronizationChannelInformation>::error(res.error()); msg.mLocationAreaIdentity = std::move(res.value()); }
    return Expected<L3SynchronizationChannelInformation>::hold(std::move(msg));
}

void L3SynchronizationChannelInformation::write(BitWriter& bw) const {
    mCellIdentity.write(bw);
    mLocationAreaIdentity.write(bw);
}

void L3SynchronizationChannelInformation::text(std::ostream& os) const {
    os << "SynchronizationChannelInformation: ";
    mCellIdentity.text(os);
    os << " ";
    mLocationAreaIdentity.text(os);
}

// ── L3ChannelRequest ───────────────────────────────────────────────────

Expected<L3ChannelRequest> L3ChannelRequest::parse(BitReader& br) {
    L3ChannelRequest msg;
    auto r = br.readField(4); if (!r) return Expected<L3ChannelRequest>::error(r.error());
    msg.mRequestReference = r.value();
    r = br.readField(4); if (!r) return Expected<L3ChannelRequest>::error(r.error());
    return Expected<L3ChannelRequest>::hold(std::move(msg));
}

void L3ChannelRequest::write(BitWriter& bw) const {
    bw.writeField(mRequestReference, 4);
    bw.writeField(0, 4);
}

void L3ChannelRequest::text(std::ostream& os) const {
    os << "ChannelRequest: RR=" << mRequestReference;
}

// ── L3HandoverAccess ───────────────────────────────────────────────────

Expected<L3HandoverAccess> L3HandoverAccess::parse(BitReader& br) {
    L3HandoverAccess msg;
    auto r = br.readField(27); if (!r) return Expected<L3HandoverAccess>::error(r.error());
    msg.mHandoverNumber = r.value();
    r = br.readField(5); if (!r) return Expected<L3HandoverAccess>::error(r.error());
    return Expected<L3HandoverAccess>::hold(std::move(msg));
}

void L3HandoverAccess::write(BitWriter& bw) const {
    bw.writeField(mHandoverNumber, 27);
    bw.writeField(0, 5);
}

void L3HandoverAccess::text(std::ostream& os) const {
    os << "HandoverAccess: handoverNumber=" << mHandoverNumber;
}

// ── L3ConfigurationChangeCommand ───────────────────────────────────────

size_t L3ConfigurationChangeCommand::bodyLength() const {
    size_t len = 0;
    if (mHaveChanDesc) len += mChanDesc.lengthV();
    if (mHavePowerCmd) len += 1 + mPowerCmd.lengthV();
    return len;
}

Expected<L3ConfigurationChangeCommand> L3ConfigurationChangeCommand::parse(BitReader& br) {
    L3ConfigurationChangeCommand msg;

    while (br.hasMore()) {
        unsigned peek = br.peekField(8);
        if (peek == 0x64) {
            { auto _ = br.readField(8); if (!_) return Expected<L3ConfigurationChangeCommand>::error(_.error()); }
            auto res = L3ChannelDescription::parse(br);
            if (!res) return Expected<L3ConfigurationChangeCommand>::error(res.error());
            msg.mChanDesc = std::move(res.value());
            msg.mHaveChanDesc = true;
        } else if (peek == 0x65) {
            { auto _ = br.readField(8); if (!_) return Expected<L3ConfigurationChangeCommand>::error(_.error()); }
            auto res = L3PowerCommand::parse(br);
            if (!res) return Expected<L3ConfigurationChangeCommand>::error(res.error());
            msg.mPowerCmd = std::move(res.value());
            msg.mHavePowerCmd = true;
        } else {
            break;
        }
    }

    return Expected<L3ConfigurationChangeCommand>::hold(std::move(msg));
}

void L3ConfigurationChangeCommand::write(BitWriter& bw) const {
    if (mHaveChanDesc) {
        bw.writeField(0x64, 8);
        mChanDesc.write(bw);
    }
    if (mHavePowerCmd) {
        bw.writeField(0x65, 8);
        mPowerCmd.write(bw);
    }
}

void L3ConfigurationChangeCommand::text(std::ostream& os) const {
    os << "ConfigurationChangeCommand";
    if (mHaveChanDesc) {
        os << " ";
        mChanDesc.text(os);
    }
}

L3ConfigurationChangeCommand L3ConfigurationChangeCommand::Builder::build() const {
    L3ConfigurationChangeCommand msg;
    msg.mHaveChanDesc = mHaveChanDesc;
    msg.mChanDesc = mChanDesc;
    msg.mHavePowerCmd = mHavePowerCmd;
    msg.mPowerCmd = mPowerCmd;
    return msg;
}

L3ConfigurationChangeCommand::Builder L3ConfigurationChangeCommand::builder() {
    return Builder{};
}

// ── L3ConfigurationChangeAcknowledge ───────────────────────────────────

Expected<L3ConfigurationChangeAcknowledge> L3ConfigurationChangeAcknowledge::parse(BitReader&) {
    return Expected<L3ConfigurationChangeAcknowledge>::hold(L3ConfigurationChangeAcknowledge{});
}

void L3ConfigurationChangeAcknowledge::write(BitWriter&) const {}

void L3ConfigurationChangeAcknowledge::text(std::ostream& os) const {
    os << "ConfigurationChangeAcknowledge";
}

L3ConfigurationChangeAcknowledge L3ConfigurationChangeAcknowledge::Builder::build() const {
    return L3ConfigurationChangeAcknowledge{};
}

L3ConfigurationChangeAcknowledge::Builder L3ConfigurationChangeAcknowledge::builder() {
    return Builder{};
}

// ── L3ConfigurationChangeReject ────────────────────────────────────────

Expected<L3ConfigurationChangeReject> L3ConfigurationChangeReject::parse(BitReader& br) {
    L3ConfigurationChangeReject msg;
    auto r = br.readField(8); if (!r) return Expected<L3ConfigurationChangeReject>::error(r.error());
    msg.mCause = static_cast<RRCause>(r.value());
    return Expected<L3ConfigurationChangeReject>::hold(std::move(msg));
}

void L3ConfigurationChangeReject::write(BitWriter& bw) const {
    bw.writeField(static_cast<uint32_t>(mCause), 8);
}

void L3ConfigurationChangeReject::text(std::ostream& os) const {
    os << "ConfigurationChangeReject: cause=" << RRCause2Str(mCause);
}

L3ConfigurationChangeReject L3ConfigurationChangeReject::Builder::build() const {
    L3ConfigurationChangeReject msg;
    msg.mCause = mCause;
    return msg;
}

L3ConfigurationChangeReject::Builder L3ConfigurationChangeReject::builder() {
    return Builder{};
}

// ── L3PartialRelease ───────────────────────────────────────────────────

Expected<L3PartialRelease> L3PartialRelease::parse(BitReader& br) {
    L3PartialRelease msg;
    {
        auto res = L3ChannelDescription::parse(br);
        if (!res) return Expected<L3PartialRelease>::error(res.error());
        msg.mChanDesc = std::move(res.value());
    }
    return Expected<L3PartialRelease>::hold(std::move(msg));
}

void L3PartialRelease::write(BitWriter& bw) const {
    mChanDesc.write(bw);
}

void L3PartialRelease::text(std::ostream& os) const {
    os << "PartialRelease: ";
    mChanDesc.text(os);
}

L3PartialRelease L3PartialRelease::Builder::build() const {
    L3PartialRelease msg;
    msg.mChanDesc = mChanDesc;
    return msg;
}

L3PartialRelease::Builder L3PartialRelease::builder() {
    return Builder{};
}

// ── L3PartialReleaseComplete ───────────────────────────────────────────

Expected<L3PartialReleaseComplete> L3PartialReleaseComplete::parse(BitReader&) {
    return Expected<L3PartialReleaseComplete>::hold(L3PartialReleaseComplete{});
}

void L3PartialReleaseComplete::write(BitWriter&) const {}

void L3PartialReleaseComplete::text(std::ostream& os) const {
    os << "PartialReleaseComplete";
}

L3PartialReleaseComplete L3PartialReleaseComplete::Builder::build() const {
    return L3PartialReleaseComplete{};
}

L3PartialReleaseComplete::Builder L3PartialReleaseComplete::builder() {
    return Builder{};
}

// ── L3ExtendedMeasurementReport ────────────────────────────────────────

Expected<L3ExtendedMeasurementReport> L3ExtendedMeasurementReport::parse(BitReader& br) {
    L3ExtendedMeasurementReport msg;
    {
        auto res = L3MeasurementResults::parse(br);
        if (!res) return Expected<L3ExtendedMeasurementReport>::error(res.error());
        msg.mMeasurementResults = std::move(res.value());
    }
    return Expected<L3ExtendedMeasurementReport>::hold(std::move(msg));
}

void L3ExtendedMeasurementReport::write(BitWriter& bw) const {
    mMeasurementResults.write(bw);
}

void L3ExtendedMeasurementReport::text(std::ostream& os) const {
    os << "ExtendedMeasurementReport: ";
    mMeasurementResults.text(os);
}

// ── L3ExtendedMeasurementOrder ─────────────────────────────────────────

Expected<L3ExtendedMeasurementOrder> L3ExtendedMeasurementOrder::parse(BitReader& br) {
    L3ExtendedMeasurementOrder msg;
    while (br.hasMore()) {
        auto r = br.readField(8); if (!r) return Expected<L3ExtendedMeasurementOrder>::error(r.error());
        msg.mData.push_back(static_cast<uint8_t>(r.value()));
    }
    return Expected<L3ExtendedMeasurementOrder>::hold(std::move(msg));
}

void L3ExtendedMeasurementOrder::write(BitWriter& bw) const {
    for (const auto& b : mData) {
        bw.writeField(b, 8);
    }
}

void L3ExtendedMeasurementOrder::text(std::ostream& os) const {
    os << "ExtendedMeasurementOrder: len=" << mData.size();
}

// ── L3FrequencyRedefinition ────────────────────────────────────────────

Expected<L3FrequencyRedefinition> L3FrequencyRedefinition::parse(BitReader& br) {
    L3FrequencyRedefinition msg;
    {
        auto res = L3FrequencyList::parse(br);
        if (!res) return Expected<L3FrequencyRedefinition>::error(res.error());
        msg.mCellChannelDescription = std::move(res.value());
    }
    {
        auto res = L3RACHControlParameters::parse(br);
        if (!res) return Expected<L3FrequencyRedefinition>::error(res.error());
        msg.mRACHControlParameters = std::move(res.value());
    }
    return Expected<L3FrequencyRedefinition>::hold(std::move(msg));
}

void L3FrequencyRedefinition::write(BitWriter& bw) const {
    mCellChannelDescription.write(bw);
    mRACHControlParameters.write(bw);
}

void L3FrequencyRedefinition::text(std::ostream& os) const {
    os << "FrequencyRedefinition: ";
    mCellChannelDescription.text(os);
    os << " ";
    mRACHControlParameters.text(os);
}

// ── L3NotificationNCH ──────────────────────────────────────────────────

Expected<L3NotificationNCH> L3NotificationNCH::parse(BitReader& br) {
    L3NotificationNCH msg;
    while (br.hasMore()) {
        auto r = br.readField(8); if (!r) return Expected<L3NotificationNCH>::error(r.error());
        msg.mData.push_back(static_cast<uint8_t>(r.value()));
    }
    return Expected<L3NotificationNCH>::hold(std::move(msg));
}

void L3NotificationNCH::write(BitWriter& bw) const {
    for (const auto& b : mData) {
        bw.writeField(b, 8);
    }
}

void L3NotificationNCH::text(std::ostream& os) const {
    os << "NotificationNCH: len=" << mData.size();
}

// ── L3NotificationResponse ─────────────────────────────────────────────

Expected<L3NotificationResponse> L3NotificationResponse::parse(BitReader& br) {
    L3NotificationResponse msg;
    while (br.hasMore()) {
        auto r = br.readField(8); if (!r) return Expected<L3NotificationResponse>::error(r.error());
        msg.mData.push_back(static_cast<uint8_t>(r.value()));
    }
    return Expected<L3NotificationResponse>::hold(std::move(msg));
}

void L3NotificationResponse::write(BitWriter& bw) const {
    for (const auto& b : mData) {
        bw.writeField(b, 8);
    }
}

void L3NotificationResponse::text(std::ostream& os) const {
    os << "NotificationResponse: len=" << mData.size();
}

// ── L3VGCSUplinkGrant ──────────────────────────────────────────────────

Expected<L3VGCSUplinkGrant> L3VGCSUplinkGrant::parse(BitReader&) {
    return Expected<L3VGCSUplinkGrant>::hold(L3VGCSUplinkGrant{});
}

void L3VGCSUplinkGrant::write(BitWriter&) const {}

void L3VGCSUplinkGrant::text(std::ostream& os) const {
    os << "VGCSUplinkGrant";
}

// ── L3UplinkRelease ────────────────────────────────────────────────────

Expected<L3UplinkRelease> L3UplinkRelease::parse(BitReader&) {
    return Expected<L3UplinkRelease>::hold(L3UplinkRelease{});
}

void L3UplinkRelease::write(BitWriter&) const {}

void L3UplinkRelease::text(std::ostream& os) const {
    os << "UplinkRelease";
}

// ── L3UplinkBusy ───────────────────────────────────────────────────────

Expected<L3UplinkBusy> L3UplinkBusy::parse(BitReader&) {
    return Expected<L3UplinkBusy>::hold(L3UplinkBusy{});
}

void L3UplinkBusy::write(BitWriter&) const {}

void L3UplinkBusy::text(std::ostream& os) const {
    os << "UplinkBusy";
}

// ── L3TalkerIndication ─────────────────────────────────────────────────

Expected<L3TalkerIndication> L3TalkerIndication::parse(BitReader&) {
    return Expected<L3TalkerIndication>::hold(L3TalkerIndication{});
}

void L3TalkerIndication::write(BitWriter&) const {}

void L3TalkerIndication::text(std::ostream& os) const {
    os << "TalkerIndication";
}

// ── L3PriorityUplinkRequest ────────────────────────────────────────────

Expected<L3PriorityUplinkRequest> L3PriorityUplinkRequest::parse(BitReader& br) {
    L3PriorityUplinkRequest msg;
    auto r = br.readField(32); if (!r) return Expected<L3PriorityUplinkRequest>::error(r.error());
    msg.mTMSI = r.value();
    return Expected<L3PriorityUplinkRequest>::hold(std::move(msg));
}

void L3PriorityUplinkRequest::write(BitWriter& bw) const {
    bw.writeField(mTMSI, 32);
}

void L3PriorityUplinkRequest::text(std::ostream& os) const {
    os << "PriorityUplinkRequest: TMSI=0x" << std::hex << mTMSI << std::dec;
}

// ── L3DataIndication ───────────────────────────────────────────────────

Expected<L3DataIndication> L3DataIndication::parse(BitReader& br) {
    L3DataIndication msg;
    while (br.hasMore()) {
        auto r = br.readField(8); if (!r) return Expected<L3DataIndication>::error(r.error());
        msg.mData.push_back(static_cast<uint8_t>(r.value()));
    }
    return Expected<L3DataIndication>::hold(std::move(msg));
}

void L3DataIndication::write(BitWriter& bw) const {
    for (const auto& b : mData) {
        bw.writeField(b, 8);
    }
}

void L3DataIndication::text(std::ostream& os) const {
    os << "DataIndication: len=" << mData.size();
}

// ── L3DataIndication2 ──────────────────────────────────────────────────

Expected<L3DataIndication2> L3DataIndication2::parse(BitReader& br) {
    L3DataIndication2 msg;
    while (br.hasMore()) {
        auto r = br.readField(8); if (!r) return Expected<L3DataIndication2>::error(r.error());
        msg.mData.push_back(static_cast<uint8_t>(r.value()));
    }
    return Expected<L3DataIndication2>::hold(std::move(msg));
}

void L3DataIndication2::write(BitWriter& bw) const {
    for (const auto& b : mData) {
        bw.writeField(b, 8);
    }
}

void L3DataIndication2::text(std::ostream& os) const {
    os << "DataIndication2: len=" << mData.size();
}

// ── L3DTMAssignmentFailure ─────────────────────────────────────────────

Expected<L3DTMAssignmentFailure> L3DTMAssignmentFailure::parse(BitReader& br) {
    L3DTMAssignmentFailure msg;
    auto r = br.readField(8); if (!r) return Expected<L3DTMAssignmentFailure>::error(r.error());
    msg.mCause = static_cast<RRCause>(r.value());
    return Expected<L3DTMAssignmentFailure>::hold(std::move(msg));
}

void L3DTMAssignmentFailure::write(BitWriter& bw) const {
    bw.writeField(static_cast<uint32_t>(mCause), 8);
}

void L3DTMAssignmentFailure::text(std::ostream& os) const {
    os << "DTMAssignmentFailure: cause=" << RRCause2Str(mCause);
}

// ── L3DTMReject ────────────────────────────────────────────────────────

Expected<L3DTMReject> L3DTMReject::parse(BitReader&) {
    return Expected<L3DTMReject>::hold(L3DTMReject{});
}

void L3DTMReject::write(BitWriter&) const {}

void L3DTMReject::text(std::ostream& os) const {
    os << "DTMReject";
}

// ── L3DTMRequest ───────────────────────────────────────────────────────

Expected<L3DTMRequest> L3DTMRequest::parse(BitReader&) {
    return Expected<L3DTMRequest>::hold(L3DTMRequest{});
}

void L3DTMRequest::write(BitWriter&) const {}

void L3DTMRequest::text(std::ostream& os) const {
    os << "DTMRequest";
}

// ── L3PacketAssignment ─────────────────────────────────────────────────

Expected<L3PacketAssignment> L3PacketAssignment::parse(BitReader& br) {
    L3PacketAssignment msg;
    {
        auto res = L3ChannelDescription::parse(br);
        if (!res) return Expected<L3PacketAssignment>::error(res.error());
        msg.mChanDesc = std::move(res.value());
    }
    {
        auto res = L3TimingAdvance::parse(br);
        if (!res) return Expected<L3PacketAssignment>::error(res.error());
        msg.mTA = std::move(res.value());
    }
    return Expected<L3PacketAssignment>::hold(std::move(msg));
}

void L3PacketAssignment::write(BitWriter& bw) const {
    mChanDesc.write(bw);
    mTA.write(bw);
}

void L3PacketAssignment::text(std::ostream& os) const {
    os << "PacketAssignment: ";
    mChanDesc.text(os);
    os << " TA=";
    mTA.text(os);
}

// ── L3DTMAssignmentCommand ─────────────────────────────────────────────

Expected<L3DTMAssignmentCommand> L3DTMAssignmentCommand::parse(BitReader&) {
    return Expected<L3DTMAssignmentCommand>::hold(L3DTMAssignmentCommand{});
}

void L3DTMAssignmentCommand::write(BitWriter&) const {}

void L3DTMAssignmentCommand::text(std::ostream& os) const {
    os << "DTMAssignmentCommand";
}

// ── L3DTMInformation ───────────────────────────────────────────────────

Expected<L3DTMInformation> L3DTMInformation::parse(BitReader&) {
    return Expected<L3DTMInformation>::hold(L3DTMInformation{});
}

void L3DTMInformation::write(BitWriter&) const {}

void L3DTMInformation::text(std::ostream& os) const {
    os << "DTMInformation";
}

// ── L3PacketInformation ────────────────────────────────────────────────

Expected<L3PacketInformation> L3PacketInformation::parse(BitReader&) {
    return Expected<L3PacketInformation>::hold(L3PacketInformation{});
}

void L3PacketInformation::write(BitWriter&) const {}

void L3PacketInformation::text(std::ostream& os) const {
    os << "PacketInformation";
}

// ── L3UTRANClassmarkChange ─────────────────────────────────────────────

Expected<L3UTRANClassmarkChange> L3UTRANClassmarkChange::parse(BitReader& br) {
    L3UTRANClassmarkChange msg;
    while (br.hasMore()) {
        auto r = br.readField(8); if (!r) return Expected<L3UTRANClassmarkChange>::error(r.error());
        msg.mClassmark.push_back(static_cast<uint8_t>(r.value()));
    }
    return Expected<L3UTRANClassmarkChange>::hold(std::move(msg));
}

void L3UTRANClassmarkChange::write(BitWriter& bw) const {
    for (const auto& b : mClassmark) {
        bw.writeField(b, 8);
    }
}

void L3UTRANClassmarkChange::text(std::ostream& os) const {
    os << "UTRANClassmarkChange: len=" << mClassmark.size();
}

// ── L3CDMA2000ClassmarkChange ──────────────────────────────────────────

Expected<L3CDMA2000ClassmarkChange> L3CDMA2000ClassmarkChange::parse(BitReader& br) {
    L3CDMA2000ClassmarkChange msg;
    while (br.hasMore()) {
        auto r = br.readField(8); if (!r) return Expected<L3CDMA2000ClassmarkChange>::error(r.error());
        msg.mClassmark.push_back(static_cast<uint8_t>(r.value()));
    }
    return Expected<L3CDMA2000ClassmarkChange>::hold(std::move(msg));
}

void L3CDMA2000ClassmarkChange::write(BitWriter& bw) const {
    for (const auto& b : mClassmark) {
        bw.writeField(b, 8);
    }
}

void L3CDMA2000ClassmarkChange::text(std::ostream& os) const {
    os << "CDMA2000ClassmarkChange: len=" << mClassmark.size();
}

// ── L3IntersysToUTRANHOCommand ─────────────────────────────────────────

Expected<L3IntersysToUTRANHOCommand> L3IntersysToUTRANHOCommand::parse(BitReader& br) {
    L3IntersysToUTRANHOCommand msg;
    while (br.hasMore()) {
        auto r = br.readField(8); if (!r) return Expected<L3IntersysToUTRANHOCommand>::error(r.error());
        msg.mData.push_back(static_cast<uint8_t>(r.value()));
    }
    return Expected<L3IntersysToUTRANHOCommand>::hold(std::move(msg));
}

void L3IntersysToUTRANHOCommand::write(BitWriter& bw) const {
    for (const auto& b : mData) {
        bw.writeField(b, 8);
    }
}

void L3IntersysToUTRANHOCommand::text(std::ostream& os) const {
    os << "IntersysToUTRANHOCommand: len=" << mData.size();
}

// ── L3IntersysToCDMA2000HOCommand ──────────────────────────────────────

Expected<L3IntersysToCDMA2000HOCommand> L3IntersysToCDMA2000HOCommand::parse(BitReader& br) {
    L3IntersysToCDMA2000HOCommand msg;
    while (br.hasMore()) {
        auto r = br.readField(8); if (!r) return Expected<L3IntersysToCDMA2000HOCommand>::error(r.error());
        msg.mData.push_back(static_cast<uint8_t>(r.value()));
    }
    return Expected<L3IntersysToCDMA2000HOCommand>::hold(std::move(msg));
}

void L3IntersysToCDMA2000HOCommand::write(BitWriter& bw) const {
    for (const auto& b : mData) {
        bw.writeField(b, 8);
    }
}

void L3IntersysToCDMA2000HOCommand::text(std::ostream& os) const {
    os << "IntersysToCDMA2000HOCommand: len=" << mData.size();
}

// ── L3GERANIUClassmarkChange ───────────────────────────────────────────

Expected<L3GERANIUClassmarkChange> L3GERANIUClassmarkChange::parse(BitReader& br) {
    L3GERANIUClassmarkChange msg;
    while (br.hasMore()) {
        auto r = br.readField(8); if (!r) return Expected<L3GERANIUClassmarkChange>::error(r.error());
        msg.mClassmark.push_back(static_cast<uint8_t>(r.value()));
    }
    return Expected<L3GERANIUClassmarkChange>::hold(std::move(msg));
}

void L3GERANIUClassmarkChange::write(BitWriter& bw) const {
    for (const auto& b : mClassmark) {
        bw.writeField(b, 8);
    }
}

void L3GERANIUClassmarkChange::text(std::ostream& os) const {
    os << "GERANIUClassmarkChange: len=" << mClassmark.size();
}

// ── L3SystemInformationType14 ──────────────────────────────────────────

Expected<L3SystemInformationType14> L3SystemInformationType14::parse(BitReader& br) {
    L3SystemInformationType14 msg;
    { auto res = L3CellIdentity::parse(br); if (!res) return Expected<L3SystemInformationType14>::error(res.error()); msg.mCI = std::move(res.value()); }
    { auto res = L3CellSelectionParameters::parse(br); if (!res) return Expected<L3SystemInformationType14>::error(res.error()); msg.mCellSelectionParameters = std::move(res.value()); }
    return Expected<L3SystemInformationType14>::hold(std::move(msg));
}

void L3SystemInformationType14::write(BitWriter& bw) const {
    mCI.write(bw);
    mCellSelectionParameters.write(bw);
}

void L3SystemInformationType14::text(std::ostream& os) const {
    os << "SystemInformationType14: ";
    mCI.text(os);
    os << " ";
    mCellSelectionParameters.text(os);
}

// ── L3SystemInformationType15 ──────────────────────────────────────────

Expected<L3SystemInformationType15> L3SystemInformationType15::parse(BitReader&) {
    return Expected<L3SystemInformationType15>::hold(L3SystemInformationType15{});
}

void L3SystemInformationType15::write(BitWriter&) const {}

void L3SystemInformationType15::text(std::ostream& os) const {
    os << "SystemInformationType15";
}

// ── L3SystemInformationType18 ──────────────────────────────────────────

size_t L3SystemInformationType18::bodyLength() const {
    size_t len = 1 + mRACHControl.lengthV();
    for (const auto& ch : mCellChannelDescriptions) {
        len += 1 + ch.lengthV();
    }
    return len;
}

Expected<L3SystemInformationType18> L3SystemInformationType18::parse(BitReader& br) {
    L3SystemInformationType18 msg;
    {
        auto ieiR = br.readField(8); if (!ieiR) return Expected<L3SystemInformationType18>::error(ieiR.error());
        auto res = L3RACHControlParameters::parse(br);
        if (!res) return Expected<L3SystemInformationType18>::error(res.error());
        msg.mRACHControl = std::move(res.value());
    }
    while (br.hasMore()) {
        unsigned peek = br.peekField(8);
        if (peek == 0x21) {
            { auto _ = br.readField(8); if (!_) return Expected<L3SystemInformationType18>::error(_.error()); }
            auto res = L3CellChannelDescription::parse(br);
            if (!res) return Expected<L3SystemInformationType18>::error(res.error());
            msg.mCellChannelDescriptions.push_back(std::move(res.value()));
        } else {
            break;
        }
    }
    return Expected<L3SystemInformationType18>::hold(std::move(msg));
}

void L3SystemInformationType18::write(BitWriter& bw) const {
    bw.writeField(0x28, 8);
    mRACHControl.write(bw);
    for (const auto& ch : mCellChannelDescriptions) {
        bw.writeField(0x21, 8);
        ch.write(bw);
    }
}

void L3SystemInformationType18::text(std::ostream& os) const {
    os << "SystemInformationType18: ";
    mRACHControl.text(os);
    os << " cells=" << mCellChannelDescriptions.size();
}

// ── L3SystemInformationType19 ──────────────────────────────────────────

size_t L3SystemInformationType19::bodyLength() const {
    size_t len = 1 + mRACHControl.lengthV();
    for (const auto& ch : mCellChannelDescriptions) {
        len += 1 + ch.lengthV();
    }
    return len;
}

Expected<L3SystemInformationType19> L3SystemInformationType19::parse(BitReader& br) {
    L3SystemInformationType19 msg;
    {
        auto ieiR = br.readField(8); if (!ieiR) return Expected<L3SystemInformationType19>::error(ieiR.error());
        auto res = L3RACHControlParameters::parse(br);
        if (!res) return Expected<L3SystemInformationType19>::error(res.error());
        msg.mRACHControl = std::move(res.value());
    }
    while (br.hasMore()) {
        unsigned peek = br.peekField(8);
        if (peek == 0x21) {
            { auto _ = br.readField(8); if (!_) return Expected<L3SystemInformationType19>::error(_.error()); }
            auto res = L3CellChannelDescription::parse(br);
            if (!res) return Expected<L3SystemInformationType19>::error(res.error());
            msg.mCellChannelDescriptions.push_back(std::move(res.value()));
        } else {
            break;
        }
    }
    return Expected<L3SystemInformationType19>::hold(std::move(msg));
}

void L3SystemInformationType19::write(BitWriter& bw) const {
    bw.writeField(0x28, 8);
    mRACHControl.write(bw);
    for (const auto& ch : mCellChannelDescriptions) {
        bw.writeField(0x21, 8);
        ch.write(bw);
    }
}

void L3SystemInformationType19::text(std::ostream& os) const {
    os << "SystemInformationType19: ";
    mRACHControl.text(os);
    os << " cells=" << mCellChannelDescriptions.size();
}

// ── L3SystemInformationType20 ──────────────────────────────────────────

size_t L3SystemInformationType20::bodyLength() const {
    size_t len = 1 + mRACHControl.lengthV();
    for (const auto& ch : mCellChannelDescriptions) {
        len += 1 + ch.lengthV();
    }
    return len;
}

Expected<L3SystemInformationType20> L3SystemInformationType20::parse(BitReader& br) {
    L3SystemInformationType20 msg;
    {
        auto ieiR = br.readField(8); if (!ieiR) return Expected<L3SystemInformationType20>::error(ieiR.error());
        auto res = L3RACHControlParameters::parse(br);
        if (!res) return Expected<L3SystemInformationType20>::error(res.error());
        msg.mRACHControl = std::move(res.value());
    }
    while (br.hasMore()) {
        unsigned peek = br.peekField(8);
        if (peek == 0x21) {
            { auto _ = br.readField(8); if (!_) return Expected<L3SystemInformationType20>::error(_.error()); }
            auto res = L3CellChannelDescription::parse(br);
            if (!res) return Expected<L3SystemInformationType20>::error(res.error());
            msg.mCellChannelDescriptions.push_back(std::move(res.value()));
        } else {
            break;
        }
    }
    return Expected<L3SystemInformationType20>::hold(std::move(msg));
}

void L3SystemInformationType20::write(BitWriter& bw) const {
    bw.writeField(0x28, 8);
    mRACHControl.write(bw);
    for (const auto& ch : mCellChannelDescriptions) {
        bw.writeField(0x21, 8);
        ch.write(bw);
    }
}

void L3SystemInformationType20::text(std::ostream& os) const {
    os << "SystemInformationType20: ";
    mRACHControl.text(os);
    os << " cells=" << mCellChannelDescriptions.size();
}

// ── L3SystemInformationType13alt ───────────────────────────────────────

Expected<L3SystemInformationType13alt> L3SystemInformationType13alt::parse(BitReader&) {
    return Expected<L3SystemInformationType13alt>::hold(L3SystemInformationType13alt{});
}

void L3SystemInformationType13alt::write(BitWriter&) const {}

void L3SystemInformationType13alt::text(std::ostream& os) const {
    os << "SystemInformationType13alt";
}

// ── L3SystemInformationType2n ──────────────────────────────────────────

Expected<L3SystemInformationType2n> L3SystemInformationType2n::parse(BitReader&) {
    return Expected<L3SystemInformationType2n>::hold(L3SystemInformationType2n{});
}

void L3SystemInformationType2n::write(BitWriter&) const {}

void L3SystemInformationType2n::text(std::ostream& os) const {
    os << "SystemInformationType2n";
}

// ── L3SystemInformationType21 ──────────────────────────────────────────

Expected<L3SystemInformationType21> L3SystemInformationType21::parse(BitReader&) {
    return Expected<L3SystemInformationType21>::hold(L3SystemInformationType21{});
}

void L3SystemInformationType21::write(BitWriter&) const {}

void L3SystemInformationType21::text(std::ostream& os) const {
    os << "SystemInformationType21";
}

// ── L3SystemInformationType22 ──────────────────────────────────────────

Expected<L3SystemInformationType22> L3SystemInformationType22::parse(BitReader&) {
    return Expected<L3SystemInformationType22>::hold(L3SystemInformationType22{});
}

void L3SystemInformationType22::write(BitWriter&) const {}

void L3SystemInformationType22::text(std::ostream& os) const {
    os << "SystemInformationType22";
}

// ── L3SystemInformationType23 ──────────────────────────────────────────

Expected<L3SystemInformationType23> L3SystemInformationType23::parse(BitReader&) {
    return Expected<L3SystemInformationType23>::hold(L3SystemInformationType23{});
}

void L3SystemInformationType23::write(BitWriter&) const {}

void L3SystemInformationType23::text(std::ostream& os) const {
    os << "SystemInformationType23";
}

// ── L3SystemInformationType10 ──────────────────────────────────────────

Expected<L3SystemInformationType10> L3SystemInformationType10::parse(BitReader& br) {
    L3SystemInformationType10 msg;
    { auto res = L3CellIdentity::parse(br); if (!res) return Expected<L3SystemInformationType10>::error(res.error()); msg.mCI = std::move(res.value()); }
    { auto res = L3LocationAreaIdentity::parse(br); if (!res) return Expected<L3SystemInformationType10>::error(res.error()); msg.mLAI = std::move(res.value()); }
    { auto res = L3CellOptionsBCCH::parse(br); if (!res) return Expected<L3SystemInformationType10>::error(res.error()); msg.mCellOptions = std::move(res.value()); }
    { auto res = L3CellSelectionParameters::parse(br); if (!res) return Expected<L3SystemInformationType10>::error(res.error()); msg.mCellSelectionParameters = std::move(res.value()); }
    return Expected<L3SystemInformationType10>::hold(std::move(msg));
}

void L3SystemInformationType10::write(BitWriter& bw) const {
    mCI.write(bw);
    mLAI.write(bw);
    mCellOptions.write(bw);
    mCellSelectionParameters.write(bw);
}

void L3SystemInformationType10::text(std::ostream& os) const {
    os << "SystemInformationType10: ";
    mCI.text(os);
    os << " ";
    mLAI.text(os);
    os << " ";
    mCellOptions.text(os);
    os << " ";
    mCellSelectionParameters.text(os);
}

// ── L3SystemInformationType10bis ───────────────────────────────────────

Expected<L3SystemInformationType10bis> L3SystemInformationType10bis::parse(BitReader& br) {
    L3SystemInformationType10bis msg;
    { auto res = L3CellIdentity::parse(br); if (!res) return Expected<L3SystemInformationType10bis>::error(res.error()); msg.mCI = std::move(res.value()); }
    { auto res = L3LocationAreaIdentity::parse(br); if (!res) return Expected<L3SystemInformationType10bis>::error(res.error()); msg.mLAI = std::move(res.value()); }
    { auto res = L3CellOptionsBCCH::parse(br); if (!res) return Expected<L3SystemInformationType10bis>::error(res.error()); msg.mCellOptions = std::move(res.value()); }
    { auto res = L3CellSelectionParameters::parse(br); if (!res) return Expected<L3SystemInformationType10bis>::error(res.error()); msg.mCellSelectionParameters = std::move(res.value()); }
    return Expected<L3SystemInformationType10bis>::hold(std::move(msg));
}

void L3SystemInformationType10bis::write(BitWriter& bw) const {
    mCI.write(bw);
    mLAI.write(bw);
    mCellOptions.write(bw);
    mCellSelectionParameters.write(bw);
}

void L3SystemInformationType10bis::text(std::ostream& os) const {
    os << "SystemInformationType10bis: ";
    mCI.text(os);
    os << " ";
    mLAI.text(os);
    os << " ";
    mCellOptions.text(os);
    os << " ";
    mCellSelectionParameters.text(os);
}

// ── L3SystemInformationType10ter ───────────────────────────────────────

Expected<L3SystemInformationType10ter> L3SystemInformationType10ter::parse(BitReader& br) {
    L3SystemInformationType10ter msg;
    { auto res = L3CellIdentity::parse(br); if (!res) return Expected<L3SystemInformationType10ter>::error(res.error()); msg.mCI = std::move(res.value()); }
    { auto res = L3LocationAreaIdentity::parse(br); if (!res) return Expected<L3SystemInformationType10ter>::error(res.error()); msg.mLAI = std::move(res.value()); }
    { auto res = L3CellOptionsBCCH::parse(br); if (!res) return Expected<L3SystemInformationType10ter>::error(res.error()); msg.mCellOptions = std::move(res.value()); }
    { auto res = L3CellSelectionParameters::parse(br); if (!res) return Expected<L3SystemInformationType10ter>::error(res.error()); msg.mCellSelectionParameters = std::move(res.value()); }
    return Expected<L3SystemInformationType10ter>::hold(std::move(msg));
}

void L3SystemInformationType10ter::write(BitWriter& bw) const {
    mCI.write(bw);
    mLAI.write(bw);
    mCellOptions.write(bw);
    mCellSelectionParameters.write(bw);
}

void L3SystemInformationType10ter::text(std::ostream& os) const {
    os << "SystemInformationType10ter: ";
    mCI.text(os);
    os << " ";
    mLAI.text(os);
    os << " ";
    mCellOptions.text(os);
    os << " ";
    mCellSelectionParameters.text(os);
}

// ── L3NotificationFACCH ────────────────────────────────────────────────

Expected<L3NotificationFACCH> L3NotificationFACCH::parse(BitReader&) {
    return Expected<L3NotificationFACCH>::hold(L3NotificationFACCH{});
}

void L3NotificationFACCH::write(BitWriter&) const {}

void L3NotificationFACCH::text(std::ostream& os) const {
    os << "NotificationFACCH";
}

// ── L3UplinkFree ───────────────────────────────────────────────────────

Expected<L3UplinkFree> L3UplinkFree::parse(BitReader&) {
    return Expected<L3UplinkFree>::hold(L3UplinkFree{});
}

void L3UplinkFree::write(BitWriter&) const {}

void L3UplinkFree::text(std::ostream& os) const {
    os << "UplinkFree";
}

// ── L3EnhancedMeasurementRepUL ─────────────────────────────────────────

Expected<L3EnhancedMeasurementRepUL> L3EnhancedMeasurementRepUL::parse(BitReader& br) {
    L3EnhancedMeasurementRepUL msg;
    while (br.hasMore()) {
        auto r = br.readField(8); if (!r) return Expected<L3EnhancedMeasurementRepUL>::error(r.error());
        msg.mData.push_back(static_cast<uint8_t>(r.value()));
    }
    return Expected<L3EnhancedMeasurementRepUL>::hold(std::move(msg));
}

void L3EnhancedMeasurementRepUL::write(BitWriter& bw) const {
    for (const auto& b : mData) {
        bw.writeField(b, 8);
    }
}

void L3EnhancedMeasurementRepUL::text(std::ostream& os) const {
    os << "EnhancedMeasurementRepUL: len=" << mData.size();
}

// ── L3MeasurementInfoDL ────────────────────────────────────────────────

Expected<L3MeasurementInfoDL> L3MeasurementInfoDL::parse(BitReader& br) {
    L3MeasurementInfoDL msg;
    while (br.hasMore()) {
        auto r = br.readField(8); if (!r) return Expected<L3MeasurementInfoDL>::error(r.error());
        msg.mData.push_back(static_cast<uint8_t>(r.value()));
    }
    return Expected<L3MeasurementInfoDL>::hold(std::move(msg));
}

void L3MeasurementInfoDL::write(BitWriter& bw) const {
    for (const auto& b : mData) {
        bw.writeField(b, 8);
    }
}

void L3MeasurementInfoDL::text(std::ostream& os) const {
    os << "MeasurementInfoDL: len=" << mData.size();
}

// ── L3VBSVGCSRecon ─────────────────────────────────────────────────────

Expected<L3VBSVGCSRecon> L3VBSVGCSRecon::parse(BitReader&) {
    return Expected<L3VBSVGCSRecon>::hold(L3VBSVGCSRecon{});
}

void L3VBSVGCSRecon::write(BitWriter&) const {}

void L3VBSVGCSRecon::text(std::ostream& os) const {
    os << "VBSVGCSRecon";
}

// ── L3VBSVGCSRecon2 ────────────────────────────────────────────────────

Expected<L3VBSVGCSRecon2> L3VBSVGCSRecon2::parse(BitReader&) {
    return Expected<L3VBSVGCSRecon2>::hold(L3VBSVGCSRecon2{});
}

void L3VBSVGCSRecon2::write(BitWriter&) const {}

void L3VBSVGCSRecon2::text(std::ostream& os) const {
    os << "VBSVGCSRecon2";
}

// ── L3VGCSAddInfo ──────────────────────────────────────────────────────

Expected<L3VGCSAddInfo> L3VGCSAddInfo::parse(BitReader&) {
    return Expected<L3VGCSAddInfo>::hold(L3VGCSAddInfo{});
}

void L3VGCSAddInfo::write(BitWriter&) const {}

void L3VGCSAddInfo::text(std::ostream& os) const {
    os << "VGCSAddInfo";
}

// ── L3VGCSMSInfo ───────────────────────────────────────────────────────

Expected<L3VGCSMSInfo> L3VGCSMSInfo::parse(BitReader&) {
    return Expected<L3VGCSMSInfo>::hold(L3VGCSMSInfo{});
}

void L3VGCSMSInfo::write(BitWriter&) const {}

void L3VGCSMSInfo::text(std::ostream& os) const {
    os << "VGCSMSInfo";
}

// ── L3VGCSSNeighCellInfo ───────────────────────────────────────────────

Expected<L3VGCSSNeighCellInfo> L3VGCSSNeighCellInfo::parse(BitReader&) {
    return Expected<L3VGCSSNeighCellInfo>::hold(L3VGCSSNeighCellInfo{});
}

void L3VGCSSNeighCellInfo::write(BitWriter&) const {}

void L3VGCSSNeighCellInfo::text(std::ostream& os) const {
    os << "VGCSSNeighCellInfo";
}

// ── L3NotifyAppData ────────────────────────────────────────────────────

Expected<L3NotifyAppData> L3NotifyAppData::parse(BitReader&) {
    return Expected<L3NotifyAppData>::hold(L3NotifyAppData{});
}

void L3NotifyAppData::write(BitWriter&) const {}

void L3NotifyAppData::text(std::ostream& os) const {
    os << "NotifyAppData";
}

// ── L3SystemInformationType2quater (GSM 04.08 §9.1.34a, MTI=0x4e) ─────

Expected<L3SystemInformationType2quater> L3SystemInformationType2quater::parse(BitReader& br) {
    L3SystemInformationType2quater msg;
    while (br.hasMore()) {
        auto b = br.readField(8);
        if (!b) return Expected<L3SystemInformationType2quater>::error(b.error());
        msg.mBody.push_back(static_cast<uint8_t>(b.value()));
    }
    return Expected<L3SystemInformationType2quater>::hold(std::move(msg));
}

void L3SystemInformationType2quater::write(BitWriter& bw) const {
    for (uint8_t b : mBody) bw.writeField(b, 8);
}

void L3SystemInformationType2quater::text(std::ostream& os) const {
    os << "SystemInformationType2quater";
    if (!mBody.empty()) os << " [" << mBody.size() << " octets]";
}

} // namespace gsml3parser
