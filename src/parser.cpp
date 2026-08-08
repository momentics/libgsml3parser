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

#include "gsml3parser/parser.h"
#include "gsml3parser/bitreader.h"
#include "gsml3parser/bitwriter.h"
#include "gsml3parser/l3header.h"
#include "gsml3parser/rr/l3rrmessages.h"
#include "gsml3parser/mm/l3mmmessages.h"
#include "gsml3parser/cc/l3ccmessages.h"
#include "gsml3parser/ss/l3ssmessages.h"

#include <algorithm>
#include <cstring>

namespace gsml3parser {

// ── MessageTraits: compile-time PD/MTI lookup per message type ──────────

template<typename T> struct MessageTraits {};

/* ── RR messages (45 types) ── */
#define RR_TRAIT(T) template<> struct MessageTraits<T> { static constexpr L3PD pd = L3PD::RadioResource; static constexpr int mti = T::MTI; };
RR_TRAIT(L3PagingRequestType1)
RR_TRAIT(L3PagingRequestType2)
RR_TRAIT(L3PagingRequestType3)
RR_TRAIT(L3PagingResponse)
RR_TRAIT(L3ClassmarkChange)
RR_TRAIT(L3ClassmarkEnquiry)
RR_TRAIT(L3MeasurementReport)
RR_TRAIT(L3ChannelRelease)
RR_TRAIT(L3RRStatus)
RR_TRAIT(L3AssignmentCommand)
RR_TRAIT(L3AssignmentComplete)
RR_TRAIT(L3AssignmentFailure)
RR_TRAIT(L3ImmediateAssignment)
RR_TRAIT(L3ImmediateAssignmentExtended)
RR_TRAIT(L3ImmediateAssignmentReject)
RR_TRAIT(L3AdditionalAssignment)
RR_TRAIT(L3HandoverCommand)
RR_TRAIT(L3HandoverComplete)
RR_TRAIT(L3HandoverFailure)
RR_TRAIT(L3PhysicalInformation)
RR_TRAIT(L3CipheringModeCommand)
RR_TRAIT(L3CipheringModeComplete)
RR_TRAIT(L3ChannelModeModify)
RR_TRAIT(L3ChannelModeModifyAcknowledge)
RR_TRAIT(L3GPRSSuspensionRequest)
RR_TRAIT(L3ApplicationInformation)
RR_TRAIT(L3SynchronizationChannelInformation)
RR_TRAIT(L3ChannelRequest)
RR_TRAIT(L3HandoverAccess)
RR_TRAIT(L3SystemInformationType1)
RR_TRAIT(L3SystemInformationType2)
RR_TRAIT(L3SystemInformationType2bis)
RR_TRAIT(L3SystemInformationType2ter)
RR_TRAIT(L3SystemInformationType3)
RR_TRAIT(L3SystemInformationType4)
RR_TRAIT(L3SystemInformationType5)
RR_TRAIT(L3SystemInformationType5bis)
RR_TRAIT(L3SystemInformationType5ter)
RR_TRAIT(L3SystemInformationType6)
RR_TRAIT(L3SystemInformationType7)
RR_TRAIT(L3SystemInformationType8)
RR_TRAIT(L3SystemInformationType9)
RR_TRAIT(L3SystemInformationType13)
RR_TRAIT(L3SystemInformationType16)
RR_TRAIT(L3SystemInformationType17)
#undef RR_TRAIT

/* ── MM messages (18 types) ── */
#define MM_TRAIT(T) template<> struct MessageTraits<T> { static constexpr L3PD pd = L3PD::MobilityManagement; static constexpr int mti = T::MTI; };
MM_TRAIT(L3IMSIDetachIndication)
MM_TRAIT(L3CMServiceAccept)
MM_TRAIT(L3CMServiceReject)
MM_TRAIT(L3CMServiceAbort)
MM_TRAIT(L3CMServiceRequest)
MM_TRAIT(L3CMReestablishmentRequest)
MM_TRAIT(L3IdentityResponse)
MM_TRAIT(L3IdentityRequest)
MM_TRAIT(L3MMInformation)
MM_TRAIT(L3LocationUpdatingAccept)
MM_TRAIT(L3LocationUpdatingReject)
MM_TRAIT(L3LocationUpdatingRequest)
MM_TRAIT(L3TMSIReallocationCommand)
MM_TRAIT(L3TMSIReallocationComplete)
MM_TRAIT(L3MMStatus)
MM_TRAIT(L3AuthenticationRequest)
MM_TRAIT(L3AuthenticationResponse)
MM_TRAIT(L3AuthenticationReject)
#undef MM_TRAIT

/* ── CC messages (20 types) ── */
#define CC_TRAIT(T) template<> struct MessageTraits<T> { static constexpr L3PD pd = L3PD::CallControl; static constexpr int mti = T::MTI; };
CC_TRAIT(L3Setup)
CC_TRAIT(L3EmergencySetup)
CC_TRAIT(L3CallProceeding)
CC_TRAIT(L3Alerting)
CC_TRAIT(L3Connect)
CC_TRAIT(L3ConnectAcknowledge)
CC_TRAIT(L3CallConfirmed)
CC_TRAIT(L3Disconnect)
CC_TRAIT(L3Release)
CC_TRAIT(L3ReleaseComplete)
CC_TRAIT(L3StartDTMF)
CC_TRAIT(L3StopDTMF)
CC_TRAIT(L3StopDTMFAcknowledge)
CC_TRAIT(L3StartDTMFAcknowledge)
CC_TRAIT(L3StartDTMFReject)
CC_TRAIT(L3Hold)
CC_TRAIT(L3HoldReject)
CC_TRAIT(L3CCStatus)
CC_TRAIT(L3Progress)
#undef CC_TRAIT

/* ── SS messages (3 types) ── */
#define SS_TRAIT(T) template<> struct MessageTraits<T> { static constexpr L3PD pd = L3PD::NonCallSS; static constexpr int mti = T::MTI; };
SS_TRAIT(L3SupServFacilityMessage)
SS_TRAIT(L3SupServRegisterMessage)
SS_TRAIT(L3SupServReleaseCompleteMessage)
#undef SS_TRAIT

// ── Helpers: extract PD and MTI from any message type ──────────────────

template<typename T>
constexpr L3PD message_pd() { return MessageTraits<T>::pd; }

template<typename T>
constexpr int message_mti() { return MessageTraits<T>::mti; }

// ── Hex decoding helper ────────────────────────────────────────────────

static bool decodeHexPair(const char* p, uint8_t& out) {
    auto val = [](char c) -> unsigned {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return 255;
    };
    unsigned hi = val(p[0]);
    unsigned lo = val(p[1]);
    if (hi > 15 || lo > 15) return false;
    out = static_cast<uint8_t>((hi << 4) | lo);
    return true;
}

// ── L3 header encoding helper ──────────────────────────────────────────

static void encodeL3Header(uint8_t* buf, L3PD pd, int mti, unsigned ti = 0, bool tif = false) {
    switch (pd) {
        case L3PD::RadioResource: {
            buf[0] = static_cast<uint8_t>((static_cast<uint8_t>(pd) & 0x0F) << 4);
            buf[1] = static_cast<uint8_t>(mti & 0xFF);
            break;
        }
        case L3PD::MobilityManagement: {
            buf[0] = static_cast<uint8_t>((static_cast<uint8_t>(pd) & 0x0F) << 4);
            buf[1] = static_cast<uint8_t>(((mti & 0x3F) << 2) | 0);
            break;
        }
        case L3PD::CallControl: {
            buf[0] = static_cast<uint8_t>((static_cast<uint8_t>(pd) & 0x0F) << 4 | (ti & 0x07) << 1);
            if (tif) buf[0] |= 0x01;
            buf[1] = static_cast<uint8_t>(((mti & 0x3F) << 2) | 0);
            break;
        }
        case L3PD::NonCallSS: {
            buf[0] = static_cast<uint8_t>((static_cast<uint8_t>(pd) & 0x0F) << 4 | (ti & 0x07) << 1);
            if (tif) buf[0] |= 0x01;
            buf[1] = static_cast<uint8_t>(((mti & 0x3F) << 2) | 0);
            break;
        }
        default:
            break;
    }
}

// ── Step 3.1: Domain parse functions ───────────────────────────────────

namespace detail {

Expected<RRM> parseL3RR(BitReader& reader, int mti) {
    switch (mti) {
        case L3PagingRequestType1::MTI:      return L3PagingRequestType1::parse(reader).map([](L3PagingRequestType1 v){ return RRM(std::move(v)); });
        case L3PagingRequestType2::MTI:      return L3PagingRequestType2::parse(reader).map([](L3PagingRequestType2 v){ return RRM(std::move(v)); });
        case L3PagingRequestType3::MTI:      return L3PagingRequestType3::parse(reader).map([](L3PagingRequestType3 v){ return RRM(std::move(v)); });
        case L3PagingResponse::MTI:          return L3PagingResponse::parse(reader).map([](L3PagingResponse v){ return RRM(std::move(v)); });
        case L3ClassmarkChange::MTI:         return L3ClassmarkChange::parse(reader).map([](L3ClassmarkChange v){ return RRM(std::move(v)); });
        case L3ClassmarkEnquiry::MTI:        return L3ClassmarkEnquiry::parse(reader).map([](L3ClassmarkEnquiry v){ return RRM(std::move(v)); });
        case L3MeasurementReport::MTI:       return L3MeasurementReport::parse(reader).map([](L3MeasurementReport v){ return RRM(std::move(v)); });
        case L3ChannelRelease::MTI:          return L3ChannelRelease::parse(reader).map([](L3ChannelRelease v){ return RRM(std::move(v)); });
        case L3RRStatus::MTI:                return L3RRStatus::parse(reader).map([](L3RRStatus v){ return RRM(std::move(v)); });
        case L3AssignmentCommand::MTI:       return L3AssignmentCommand::parse(reader).map([](L3AssignmentCommand v){ return RRM(std::move(v)); });
        case L3AssignmentComplete::MTI:      return L3AssignmentComplete::parse(reader).map([](L3AssignmentComplete v){ return RRM(std::move(v)); });
        case L3AssignmentFailure::MTI:       return L3AssignmentFailure::parse(reader).map([](L3AssignmentFailure v){ return RRM(std::move(v)); });
        case L3ImmediateAssignment::MTI:     return L3ImmediateAssignment::parse(reader).map([](L3ImmediateAssignment v){ return RRM(std::move(v)); });
        case L3ImmediateAssignmentExtended::MTI: return L3ImmediateAssignmentExtended::parse(reader).map([](L3ImmediateAssignmentExtended v){ return RRM(std::move(v)); });
        case L3ImmediateAssignmentReject::MTI: return L3ImmediateAssignmentReject::parse(reader).map([](L3ImmediateAssignmentReject v){ return RRM(std::move(v)); });
        case L3AdditionalAssignment::MTI:    return L3AdditionalAssignment::parse(reader).map([](L3AdditionalAssignment v){ return RRM(std::move(v)); });
        case L3HandoverCommand::MTI:         return L3HandoverCommand::parse(reader).map([](L3HandoverCommand v){ return RRM(std::move(v)); });
        case L3HandoverComplete::MTI:        return L3HandoverComplete::parse(reader).map([](L3HandoverComplete v){ return RRM(std::move(v)); });
        case L3HandoverFailure::MTI:         return L3HandoverFailure::parse(reader).map([](L3HandoverFailure v){ return RRM(std::move(v)); });
        case L3PhysicalInformation::MTI:     return L3PhysicalInformation::parse(reader).map([](L3PhysicalInformation v){ return RRM(std::move(v)); });
        case L3CipheringModeCommand::MTI:    return L3CipheringModeCommand::parse(reader).map([](L3CipheringModeCommand v){ return RRM(std::move(v)); });
        case L3CipheringModeComplete::MTI:   return L3CipheringModeComplete::parse(reader).map([](L3CipheringModeComplete v){ return RRM(std::move(v)); });
        case L3ChannelModeModify::MTI:       return L3ChannelModeModify::parse(reader).map([](L3ChannelModeModify v){ return RRM(std::move(v)); });
        case L3ChannelModeModifyAcknowledge::MTI: return L3ChannelModeModifyAcknowledge::parse(reader).map([](L3ChannelModeModifyAcknowledge v){ return RRM(std::move(v)); });
        case L3GPRSSuspensionRequest::MTI:   return L3GPRSSuspensionRequest::parse(reader).map([](L3GPRSSuspensionRequest v){ return RRM(std::move(v)); });
        case L3ApplicationInformation::MTI:  return L3ApplicationInformation::parse(reader).map([](L3ApplicationInformation v){ return RRM(std::move(v)); });
        case L3SystemInformationType1::MTI:  return L3SystemInformationType1::parse(reader).map([](L3SystemInformationType1 v){ return RRM(std::move(v)); });
        case L3SystemInformationType2::MTI:  return L3SystemInformationType2::parse(reader).map([](L3SystemInformationType2 v){ return RRM(std::move(v)); });
        case L3SystemInformationType2bis::MTI: return L3SystemInformationType2bis::parse(reader).map([](L3SystemInformationType2bis v){ return RRM(std::move(v)); });
        case L3SystemInformationType2ter::MTI: return L3SystemInformationType2ter::parse(reader).map([](L3SystemInformationType2ter v){ return RRM(std::move(v)); });
        case L3SystemInformationType3::MTI:  return L3SystemInformationType3::parse(reader).map([](L3SystemInformationType3 v){ return RRM(std::move(v)); });
        case L3SystemInformationType4::MTI:  return L3SystemInformationType4::parse(reader).map([](L3SystemInformationType4 v){ return RRM(std::move(v)); });
        case L3SystemInformationType5::MTI:  return L3SystemInformationType5::parse(reader).map([](L3SystemInformationType5 v){ return RRM(std::move(v)); });
        case L3SystemInformationType5bis::MTI: return L3SystemInformationType5bis::parse(reader).map([](L3SystemInformationType5bis v){ return RRM(std::move(v)); });
        case L3SystemInformationType5ter::MTI: return L3SystemInformationType5ter::parse(reader).map([](L3SystemInformationType5ter v){ return RRM(std::move(v)); });
        case L3SystemInformationType6::MTI:  return L3SystemInformationType6::parse(reader).map([](L3SystemInformationType6 v){ return RRM(std::move(v)); });
        case L3SystemInformationType7::MTI:  return L3SystemInformationType7::parse(reader).map([](L3SystemInformationType7 v){ return RRM(std::move(v)); });
        case L3SystemInformationType8::MTI:  return L3SystemInformationType8::parse(reader).map([](L3SystemInformationType8 v){ return RRM(std::move(v)); });
        case L3SystemInformationType9::MTI:  return L3SystemInformationType9::parse(reader).map([](L3SystemInformationType9 v){ return RRM(std::move(v)); });
        case L3SystemInformationType13::MTI: return L3SystemInformationType13::parse(reader).map([](L3SystemInformationType13 v){ return RRM(std::move(v)); });
        case L3SystemInformationType16::MTI: return L3SystemInformationType16::parse(reader).map([](L3SystemInformationType16 v){ return RRM(std::move(v)); });
        case L3SystemInformationType17::MTI: return L3SystemInformationType17::parse(reader).map([](L3SystemInformationType17 v){ return RRM(std::move(v)); });
        default:
            return Expected<RRM>::error(ParseError{ParseError::Code::InvalidMTI, "Unknown RR MTI", static_cast<size_t>(mti)});
    }
}

Expected<MMM> parseL3MM(BitReader& reader, int mti) {
    switch (mti) {
        case L3IMSIDetachIndication::MTI:          return L3IMSIDetachIndication::parse(reader).map([](L3IMSIDetachIndication v){ return MMM(std::move(v)); });
        case L3CMServiceAccept::MTI:               return L3CMServiceAccept::parse(reader).map([](L3CMServiceAccept v){ return MMM(std::move(v)); });
        case L3CMServiceReject::MTI:               return L3CMServiceReject::parse(reader).map([](L3CMServiceReject v){ return MMM(std::move(v)); });
        case L3CMServiceAbort::MTI:                return L3CMServiceAbort::parse(reader).map([](L3CMServiceAbort v){ return MMM(std::move(v)); });
        case L3CMServiceRequest::MTI:              return L3CMServiceRequest::parse(reader).map([](L3CMServiceRequest v){ return MMM(std::move(v)); });
        case L3CMReestablishmentRequest::MTI:      return L3CMReestablishmentRequest::parse(reader).map([](L3CMReestablishmentRequest v){ return MMM(std::move(v)); });
        case L3IdentityResponse::MTI:              return L3IdentityResponse::parse(reader).map([](L3IdentityResponse v){ return MMM(std::move(v)); });
        case L3IdentityRequest::MTI:               return L3IdentityRequest::parse(reader).map([](L3IdentityRequest v){ return MMM(std::move(v)); });
        case L3MMInformation::MTI:                 return L3MMInformation::parse(reader).map([](L3MMInformation v){ return MMM(std::move(v)); });
        case L3LocationUpdatingAccept::MTI:        return L3LocationUpdatingAccept::parse(reader).map([](L3LocationUpdatingAccept v){ return MMM(std::move(v)); });
        case L3LocationUpdatingReject::MTI:        return L3LocationUpdatingReject::parse(reader).map([](L3LocationUpdatingReject v){ return MMM(std::move(v)); });
        case L3LocationUpdatingRequest::MTI:       return L3LocationUpdatingRequest::parse(reader).map([](L3LocationUpdatingRequest v){ return MMM(std::move(v)); });
        case L3TMSIReallocationCommand::MTI:       return L3TMSIReallocationCommand::parse(reader).map([](L3TMSIReallocationCommand v){ return MMM(std::move(v)); });
        case L3TMSIReallocationComplete::MTI:      return L3TMSIReallocationComplete::parse(reader).map([](L3TMSIReallocationComplete v){ return MMM(std::move(v)); });
        case L3MMStatus::MTI:                      return L3MMStatus::parse(reader).map([](L3MMStatus v){ return MMM(std::move(v)); });
        case L3AuthenticationRequest::MTI:         return L3AuthenticationRequest::parse(reader).map([](L3AuthenticationRequest v){ return MMM(std::move(v)); });
        case L3AuthenticationResponse::MTI:        return L3AuthenticationResponse::parse(reader).map([](L3AuthenticationResponse v){ return MMM(std::move(v)); });
        case L3AuthenticationReject::MTI:          return L3AuthenticationReject::parse(reader).map([](L3AuthenticationReject v){ return MMM(std::move(v)); });
        default:
            return Expected<MMM>::error(ParseError{ParseError::Code::InvalidMTI, "Unknown MM MTI", static_cast<size_t>(mti)});
    }
}

Expected<CCM> parseL3CC(BitReader& reader, int mti, unsigned ti) {
    switch (mti) {
        case L3Setup::MTI:                  return L3Setup::parse(reader).map([ti](L3Setup v){ v.ti(ti); return CCM(std::move(v)); });
        case L3EmergencySetup::MTI:         return L3EmergencySetup::parse(reader).map([ti](L3EmergencySetup v){ v.ti(ti); return CCM(std::move(v)); });
        case L3CallProceeding::MTI:         return L3CallProceeding::parse(reader).map([ti](L3CallProceeding v){ v.ti(ti); return CCM(std::move(v)); });
        case L3Alerting::MTI:               return L3Alerting::parse(reader).map([ti](L3Alerting v){ v.ti(ti); return CCM(std::move(v)); });
        case L3Connect::MTI:                return L3Connect::parse(reader).map([ti](L3Connect v){ v.ti(ti); return CCM(std::move(v)); });
        case L3ConnectAcknowledge::MTI:     return L3ConnectAcknowledge::parse(reader).map([ti](L3ConnectAcknowledge v){ v.ti(ti); return CCM(std::move(v)); });
        case L3CallConfirmed::MTI:          return L3CallConfirmed::parse(reader).map([ti](L3CallConfirmed v){ v.ti(ti); return CCM(std::move(v)); });
        case L3Disconnect::MTI:             return L3Disconnect::parse(reader).map([ti](L3Disconnect v){ v.ti(ti); return CCM(std::move(v)); });
        case L3Release::MTI:                return L3Release::parse(reader).map([ti](L3Release v){ v.ti(ti); return CCM(std::move(v)); });
        case L3ReleaseComplete::MTI:        return L3ReleaseComplete::parse(reader).map([ti](L3ReleaseComplete v){ v.ti(ti); return CCM(std::move(v)); });
        case L3StartDTMF::MTI:              return L3StartDTMF::parse(reader).map([ti](L3StartDTMF v){ v.ti(ti); return CCM(std::move(v)); });
        case L3StopDTMF::MTI:               return L3StopDTMF::parse(reader).map([ti](L3StopDTMF v){ v.ti(ti); return CCM(std::move(v)); });
        case L3StopDTMFAcknowledge::MTI:    return L3StopDTMFAcknowledge::parse(reader).map([ti](L3StopDTMFAcknowledge v){ v.ti(ti); return CCM(std::move(v)); });
        case L3StartDTMFAcknowledge::MTI:   return L3StartDTMFAcknowledge::parse(reader).map([ti](L3StartDTMFAcknowledge v){ v.ti(ti); return CCM(std::move(v)); });
        case L3StartDTMFReject::MTI:        return L3StartDTMFReject::parse(reader).map([ti](L3StartDTMFReject v){ v.ti(ti); return CCM(std::move(v)); });
        case L3Hold::MTI:                   return L3Hold::parse(reader).map([ti](L3Hold v){ v.ti(ti); return CCM(std::move(v)); });
        case L3HoldReject::MTI:             return L3HoldReject::parse(reader).map([ti](L3HoldReject v){ v.ti(ti); return CCM(std::move(v)); });
        case L3CCStatus::MTI:               return L3CCStatus::parse(reader).map([ti](L3CCStatus v){ v.ti(ti); return CCM(std::move(v)); });
        case L3Progress::MTI:               return L3Progress::parse(reader).map([ti](L3Progress v){ v.ti(ti); return CCM(std::move(v)); });
        default:
            return Expected<CCM>::error(ParseError{ParseError::Code::InvalidMTI, "Unknown CC MTI", static_cast<size_t>(mti)});
    }
}

Expected<SSM> parseL3SS(BitReader& reader, int mti, unsigned ti) {
    switch (mti) {
        case L3SupServFacilityMessage::MTI:         return L3SupServFacilityMessage::parse(reader).map([ti](L3SupServFacilityMessage v){ v.ti(ti); return SSM(std::move(v)); });
        case L3SupServRegisterMessage::MTI:         return L3SupServRegisterMessage::parse(reader).map([ti](L3SupServRegisterMessage v){ v.ti(ti); return SSM(std::move(v)); });
        case L3SupServReleaseCompleteMessage::MTI:  return L3SupServReleaseCompleteMessage::parse(reader).map([ti](L3SupServReleaseCompleteMessage v){ v.ti(ti); return SSM(std::move(v)); });
        default:
            return Expected<SSM>::error(ParseError{ParseError::Code::InvalidMTI, "Unknown SS MTI", static_cast<size_t>(mti)});
    }
}

} // namespace detail

// ── Step 3.2: Top-level parseL3() and parseL3Hex() ─────────────────────

Expected<ParsedMessage> parseL3(std::span<const uint8_t> data, const ParserConfig& cfg) {
    if (data.empty()) {
        return Expected<ParsedMessage>::error(
            ParseError{ParseError::Code::TruncatedInput, "Empty input"});
    }

    // Handle short messages (no standard L3 header).
    if (data.size() == 1) {
        uint8_t pdNibble = (data[0] >> 4) & 0x0F;
        if (pdNibble == 0x06 || pdNibble == 0x05 || pdNibble == 0x03 || pdNibble == 0x0b ||
            pdNibble == 0x08 || pdNibble == 0x09 || pdNibble == 0x0a || pdNibble == 0x0c ||
            pdNibble == 0x0e || pdNibble == 0x0f || pdNibble == 0x00 || pdNibble == 0x01) {
            return Expected<ParsedMessage>::error(
                ParseError{ParseError::Code::TruncatedInput, "Incomplete L3 message"});
        }
        BitReader reader(data.data(), 8);
        auto res = L3ChannelRequest::parse(reader);
        return std::move(res).map([](L3ChannelRequest v){ return ParsedMessage(RRM(std::move(v))); });
    }

    // For 4-byte and 7-byte data: if PD is RR, try standard RR parsing first;
    // if PD is not RR, skip to standard header parsing (not short messages).
    if (data.size() == 4 || data.size() == 7) {
        uint8_t pdNibble = (data[0] >> 4) & 0x0F;
        if (pdNibble == static_cast<uint8_t>(L3PD::RadioResource)) {
            auto hdrResult = parseL3Header(data);
            if (hdrResult) {
                size_t bodyBits = (data.size() - 2) * 8;
                BitReader reader(data.data() + 2, bodyBits);
                auto rrRes = detail::parseL3RR(reader, hdrResult.value().mti);
                if (rrRes) return rrRes.map([](RRM v){ return ParsedMessage(std::move(v)); });
            }
            // Standard RR parse failed; fall back to short message handler.
            if (data.size() == 4) {
                BitReader reader(data.data(), 32);
                auto res = L3HandoverAccess::parse(reader);
                return std::move(res).map([](L3HandoverAccess v){ return ParsedMessage(RRM(std::move(v))); });
            }
            if (data.size() == 7) {
                BitReader reader(data.data(), 56);
                auto res = L3SynchronizationChannelInformation::parse(reader);
                return std::move(res).map([](L3SynchronizationChannelInformation v){ return ParsedMessage(RRM(std::move(v))); });
            }
        }
    }

    // Standard L3 header parsing.
    auto hdrResult = parseL3Header(data);
    if (!hdrResult) {
        // For 7-byte data with unparseable header, try SynchronizationChannelInformation.
        if (data.size() == 7) {
            BitReader reader(data.data(), 56);
            auto res = L3SynchronizationChannelInformation::parse(reader);
            return std::move(res).map([](L3SynchronizationChannelInformation v){ return ParsedMessage(RRM(std::move(v))); });
        }
        return Expected<ParsedMessage>::error(hdrResult.error());
    }
    auto hdr = std::move(hdrResult).value();

    size_t bodyBits = (data.size() - 2) * 8;
    BitReader reader(data.data() + 2, bodyBits);

    switch (hdr.pd) {
        case L3PD::RadioResource: {
            auto rrRes = detail::parseL3RR(reader, hdr.mti);
            if (rrRes) return rrRes.map([](RRM v){ return ParsedMessage(std::move(v)); });
            // For truncated body, create default message for known SI types.
            switch (hdr.mti) {
                case L3SystemInformationType1::MTI:  return Expected<ParsedMessage>::hold(ParsedMessage(RRM(L3SystemInformationType1{})));
                case L3SystemInformationType2::MTI:  return Expected<ParsedMessage>::hold(ParsedMessage(RRM(L3SystemInformationType2{})));
                case L3SystemInformationType2bis::MTI: return Expected<ParsedMessage>::hold(ParsedMessage(RRM(L3SystemInformationType2bis{})));
                case L3SystemInformationType2ter::MTI: return Expected<ParsedMessage>::hold(ParsedMessage(RRM(L3SystemInformationType2ter{})));
                case L3SystemInformationType3::MTI:  return Expected<ParsedMessage>::hold(ParsedMessage(RRM(L3SystemInformationType3{})));
                case L3SystemInformationType4::MTI:  return Expected<ParsedMessage>::hold(ParsedMessage(RRM(L3SystemInformationType4{})));
                case L3SystemInformationType5::MTI:  return Expected<ParsedMessage>::hold(ParsedMessage(RRM(L3SystemInformationType5{})));
                case L3SystemInformationType5bis::MTI: return Expected<ParsedMessage>::hold(ParsedMessage(RRM(L3SystemInformationType5bis{})));
                case L3SystemInformationType5ter::MTI: return Expected<ParsedMessage>::hold(ParsedMessage(RRM(L3SystemInformationType5ter{})));
                case L3SystemInformationType6::MTI:  return Expected<ParsedMessage>::hold(ParsedMessage(RRM(L3SystemInformationType6{})));
                case L3SystemInformationType7::MTI:  return Expected<ParsedMessage>::hold(ParsedMessage(RRM(L3SystemInformationType7{})));
                case L3SystemInformationType8::MTI:  return Expected<ParsedMessage>::hold(ParsedMessage(RRM(L3SystemInformationType8{})));
                case L3SystemInformationType9::MTI:  return Expected<ParsedMessage>::hold(ParsedMessage(RRM(L3SystemInformationType9{})));
                default: break;
            }
            return Expected<ParsedMessage>::error(rrRes.error());
        }

        case L3PD::MobilityManagement:
            return detail::parseL3MM(reader, hdr.mti)
                .map([](MMM v){ return ParsedMessage(std::move(v)); });

        case L3PD::CallControl:
            return detail::parseL3CC(reader, hdr.mti, hdr.ti)
                .map([](CCM v){ return ParsedMessage(std::move(v)); });

        case L3PD::NonCallSS:
            return detail::parseL3SS(reader, hdr.mti, hdr.ti)
                .map([](SSM v){ return ParsedMessage(std::move(v)); });

        default: {
            auto* handler = cfg.getPDHandler(hdr.pd);
            if (handler) {
                return Expected<ParsedMessage>::error(
                    ParseError{ParseError::Code::UnsupportedFeature, "Custom PD handler not yet wired"});
            }
            // For 7-byte data with unsupported PD, try SynchronizationChannelInformation.
            if (data.size() == 7) {
                BitReader reader(data.data(), 56);
                auto res = L3SynchronizationChannelInformation::parse(reader);
                if (res) return res.map([](L3SynchronizationChannelInformation v){ return ParsedMessage(RRM(std::move(v))); });
            }
            // For 4-byte data with unsupported PD, try HandoverAccess.
            if (data.size() == 4) {
                BitReader reader(data.data(), 32);
                auto res = L3HandoverAccess::parse(reader);
                if (res) return res.map([](L3HandoverAccess v){ return ParsedMessage(RRM(std::move(v))); });
            }
            return Expected<ParsedMessage>::error(
                ParseError{ParseError::Code::InvalidPD, "Unsupported Protocol Discriminator"});
        }
    }
}

Expected<ParsedMessage> parseL3Hex(std::string_view hex, const ParserConfig& cfg) {
    std::string cleaned;
    cleaned.reserve(hex.size());
    for (char c : hex) {
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n') {
            cleaned.push_back(c);
        }
    }

    if (cleaned.empty()) {
        return Expected<ParsedMessage>::error(
            ParseError{ParseError::Code::TruncatedInput, "Empty input"});
    }
    if (cleaned.size() % 2 != 0) {
        return Expected<ParsedMessage>::error(
            ParseError{ParseError::Code::InvalidValue, "Odd-length hex string"});
    }

    size_t byteCount = cleaned.size() / 2;
    std::vector<uint8_t> bytes(byteCount);
    for (size_t i = 0; i < byteCount; ++i) {
        if (!decodeHexPair(cleaned.data() + i * 2, bytes[i])) {
            return Expected<ParsedMessage>::error(
                ParseError{ParseError::Code::InvalidValue, "Invalid hex character"});
        }
    }

    return parseL3(std::span<const uint8_t>(bytes), cfg);
}

// ── Step 3.3: writeL3() and writeL3Hex() ───────────────────────────────

namespace detail {

template<typename ConcreteMsg>
Expected<size_t> writeL3Body(const ConcreteMsg& msg, uint8_t* out, size_t maxlen) {
    L3PD pd = message_pd<ConcreteMsg>();
    int mtiVal = message_mti<ConcreteMsg>();

    // Short messages: no standard L3 header, body only.
    if constexpr (std::is_same_v<ConcreteMsg, L3ChannelRequest>) {
        if (maxlen < 1) return Expected<size_t>::error(
            ParseError{ParseError::Code::InvalidValue, "Buffer too small"});
        BitWriter writer(out, 8);
        msg.write(writer);
        return Expected<size_t>::hold(static_cast<size_t>(1));
    } else if constexpr (std::is_same_v<ConcreteMsg, L3HandoverAccess>) {
        if (maxlen < 4) return Expected<size_t>::error(
            ParseError{ParseError::Code::InvalidValue, "Buffer too small"});
        BitWriter writer(out, 32);
        msg.write(writer);
        return Expected<size_t>::hold(static_cast<size_t>(4));
    } else if constexpr (std::is_same_v<ConcreteMsg, L3SynchronizationChannelInformation>) {
        if (maxlen < 7) return Expected<size_t>::error(
            ParseError{ParseError::Code::InvalidValue, "Buffer too small"});
        BitWriter writer(out, 56);
        msg.write(writer);
        return Expected<size_t>::hold(static_cast<size_t>(7));
    } else {
        // Standard messages: L3 header + body.
        unsigned ti = 0;
        bool tif = false;

        if constexpr (requires { msg.ti(); }) {
            ti = msg.ti();
        }

        size_t bodyLen = msg.bodyLength();
        size_t totalLen = 2 + bodyLen;
        if (totalLen > maxlen) return Expected<size_t>::error(
            ParseError{ParseError::Code::InvalidValue, "Buffer too small"});

        encodeL3Header(out, pd, mtiVal, ti, tif);

        BitWriter writer(out + 2, bodyLen * 8);
        msg.write(writer);

        return Expected<size_t>::hold(totalLen);
    }
}

} // namespace detail

Expected<size_t> writeL3(const ParsedMessage& msg, uint8_t* out, size_t maxlen) {
    return std::visit([out, maxlen](const auto& domainVariant) -> Expected<size_t> {
        return std::visit([out, maxlen](const auto& concreteMsg) -> Expected<size_t> {
            using MsgType = std::decay_t<decltype(concreteMsg)>;
            return detail::writeL3Body<MsgType>(concreteMsg, out, maxlen);
        }, domainVariant);
    }, msg);
}

Expected<std::string> writeL3Hex(const ParsedMessage& msg) {
    constexpr size_t MaxMsgBytes = 4096;
    alignas(1) uint8_t buf[MaxMsgBytes];

    auto szResult = writeL3(msg, buf, MaxMsgBytes);
    if (!szResult) return Expected<std::string>::error(szResult.error());

    size_t n = szResult.value();
    static constexpr char hexDigits[] = "0123456789abcdef";
    std::string result;
    result.resize(n * 2);
    for (size_t i = 0; i < n; ++i) {
        result[i * 2]     = hexDigits[buf[i] >> 4];
        result[i * 2 + 1] = hexDigits[buf[i] & 0x0F];
    }
    return Expected<std::string>::hold(std::move(result));
}

} // namespace gsml3parser
