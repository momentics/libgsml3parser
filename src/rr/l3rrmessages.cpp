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
            size_t len = 0;
            if (ext) {
                auto lR = br.readField(8); if (!lR) return Expected<L3AssignmentCommand>::error(lR.error());
                len = lR.value();
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
            size_t len = 0;
            if (ext) {
                auto lR = br.readField(8); if (!lR) return Expected<L3ClassmarkChange>::error(lR.error());
                len = lR.value();
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

// ── L3CipheringModeComplete ────────────────────────────────────────────

Expected<L3CipheringModeComplete> L3CipheringModeComplete::parse(BitReader&) {
    return Expected<L3CipheringModeComplete>::hold(L3CipheringModeComplete{});
}

void L3CipheringModeComplete::write(BitWriter&) const {}

void L3CipheringModeComplete::text(std::ostream& os) const {
    os << "CipheringModeComplete";
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
            size_t len = 0;
            if (ext) {
                auto lR = br.readField(8); if (!lR) return Expected<L3ChannelModeModify>::error(lR.error());
                len = lR.value();
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
    msg.mSuspensionCause = r.value();

    if (br.hasMore()) {
        unsigned peek = br.peekField(8);
        if (peek == 0x01) {
            { auto _ = br.readField(8); if (!_) return Expected<L3GPRSSuspensionRequest>::error(_.error()); }
            r = br.readField(8); if (!r) return Expected<L3GPRSSuspensionRequest>::error(r.error());
            msg.mServiceSupport = r.value();
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

} // namespace gsml3parser
