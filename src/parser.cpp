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
#include "gsml3parser/gmm/l3gmmmessages.h"
#include "gsml3parser/sms/l3smsmessages.h"
#include "gsml3parser/sms/l3smsl3messages.h"
#include "gsml3parser/sm/l3smmessages.h"
#include "gsml3parser/bcc/l3bccmessages.h"
#include "gsml3parser/gcc/l3gccmessages.h"
#include "gsml3parser/ls/l3lsmessages.h"

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
RR_TRAIT(L3ConfigurationChangeCommand)
RR_TRAIT(L3ConfigurationChangeAcknowledge)
RR_TRAIT(L3ConfigurationChangeReject)
RR_TRAIT(L3PartialRelease)
RR_TRAIT(L3PartialReleaseComplete)
RR_TRAIT(L3ExtendedMeasurementReport)
RR_TRAIT(L3ExtendedMeasurementOrder)
RR_TRAIT(L3FrequencyRedefinition)
RR_TRAIT(L3NotificationNCH)
RR_TRAIT(L3NotificationResponse)
RR_TRAIT(L3VGCSUplinkGrant)
RR_TRAIT(L3UplinkRelease)
RR_TRAIT(L3UplinkBusy)
RR_TRAIT(L3TalkerIndication)
RR_TRAIT(L3PriorityUplinkRequest)
RR_TRAIT(L3DataIndication)
RR_TRAIT(L3DataIndication2)
RR_TRAIT(L3DTMAssignmentFailure)
RR_TRAIT(L3DTMReject)
RR_TRAIT(L3DTMRequest)
RR_TRAIT(L3PacketAssignment)
RR_TRAIT(L3DTMAssignmentCommand)
RR_TRAIT(L3DTMInformation)
RR_TRAIT(L3PacketInformation)
RR_TRAIT(L3UTRANClassmarkChange)
RR_TRAIT(L3CDMA2000ClassmarkChange)
RR_TRAIT(L3IntersysToUTRANHOCommand)
RR_TRAIT(L3IntersysToCDMA2000HOCommand)
RR_TRAIT(L3GERANIUClassmarkChange)
RR_TRAIT(L3SystemInformationType14)
RR_TRAIT(L3SystemInformationType15)
RR_TRAIT(L3SystemInformationType18)
RR_TRAIT(L3SystemInformationType19)
RR_TRAIT(L3SystemInformationType20)
RR_TRAIT(L3SystemInformationType13alt)
RR_TRAIT(L3SystemInformationType2n)
RR_TRAIT(L3SystemInformationType21)
RR_TRAIT(L3SystemInformationType22)
RR_TRAIT(L3SystemInformationType23)
RR_TRAIT(L3SystemInformationType10)
RR_TRAIT(L3SystemInformationType10bis)
RR_TRAIT(L3SystemInformationType10ter)
RR_TRAIT(L3NotificationFACCH)
RR_TRAIT(L3UplinkFree)
RR_TRAIT(L3EnhancedMeasurementRepUL)
RR_TRAIT(L3MeasurementInfoDL)
RR_TRAIT(L3VBSVGCSRecon)
RR_TRAIT(L3VBSVGCSRecon2)
RR_TRAIT(L3VGCSAddInfo)
RR_TRAIT(L3VGCSMSInfo)
RR_TRAIT(L3VGCSSNeighCellInfo)
RR_TRAIT(L3NotifyAppData)
RR_TRAIT(L3SystemInformationType2quater)
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
MM_TRAIT(L3CMRequest)
MM_TRAIT(L3PagingMM)
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
CC_TRAIT(L3Facility)
CC_TRAIT(L3Modify)
CC_TRAIT(L3UnitData)
CC_TRAIT(L3UnitDataAck)
CC_TRAIT(L3ErrorIndication)
#undef CC_TRAIT

/* ── SS messages (3 types) ── */
#define SS_TRAIT(T) template<> struct MessageTraits<T> { static constexpr L3PD pd = L3PD::NonCallSS; static constexpr int mti = T::MTI; };
SS_TRAIT(L3SupServFacilityMessage)
SS_TRAIT(L3SupServRegisterMessage)
SS_TRAIT(L3SupServReleaseCompleteMessage)
#undef SS_TRAIT

/* ── GMM messages (23 types) ── */
#define GMM_TRAIT(T) template<> struct MessageTraits<T> { static constexpr L3PD pd = L3PD::GPRSMobilityManagement; static constexpr int mti = T::MTI; };
GMM_TRAIT(L3AttachRequest)
GMM_TRAIT(L3AttachAccept)
GMM_TRAIT(L3AttachComplete)
GMM_TRAIT(L3AttachReject)
GMM_TRAIT(L3DetachRequest)
GMM_TRAIT(L3DetachAccept)
GMM_TRAIT(L3RoutingAreaUpdateRequest)
GMM_TRAIT(L3RoutingAreaUpdateAccept)
GMM_TRAIT(L3RoutingAreaUpdateComplete)
GMM_TRAIT(L3RoutingAreaUpdateReject)
GMM_TRAIT(L3ServiceRequest)
GMM_TRAIT(L3ServiceAccept)
GMM_TRAIT(L3ServiceReject)
GMM_TRAIT(L3P_TMSIReallocationCommand)
GMM_TRAIT(L3P_TMSIReallocationComplete)
GMM_TRAIT(L3AuthenticationAndCipheringRequest)
GMM_TRAIT(L3AuthenticationAndCipheringResponse)
GMM_TRAIT(L3AuthenticationAndCipheringReject)
GMM_TRAIT(L3GMMIdentityRequest)
GMM_TRAIT(L3GMMIdentityResponse)
GMM_TRAIT(L3AuthenticationAndCipheringFailure)
GMM_TRAIT(L3GMMStatus)
GMM_TRAIT(L3GMMInformation)
#undef GMM_TRAIT

/* ── SM messages (29 types) ── */
#define SM_TRAIT(T) template<> struct MessageTraits<T> { static constexpr L3PD pd = L3PD::GPRSSessionManagement; static constexpr int mti = T::MTI; };
SM_TRAIT(L3ActivatePDPContextRequest)
SM_TRAIT(L3ActivatePDPContextAccept)
SM_TRAIT(L3ActivatePDPContextReject)
SM_TRAIT(L3DeactivatePDPContextRequest)
SM_TRAIT(L3DeactivatePDPContextAccept)
SM_TRAIT(L3ModifyPDPContextRequest)
SM_TRAIT(L3ModifyPDPContextAccept)
SM_TRAIT(L3ModifyPDPContextReject)
SM_TRAIT(L3SMStatus)
SM_TRAIT(L3RequestPDPContextActivation)
SM_TRAIT(L3RequestPDPContextActivationReject)
SM_TRAIT(L3ModifyPDPContextRequestMS)
SM_TRAIT(L3ModifyPDPContextAcceptNet)
SM_TRAIT(L3ActivateSecondaryPDPContextRequest)
SM_TRAIT(L3ActivateSecondaryPDPContextAccept)
SM_TRAIT(L3ActivateSecondaryPDPContextReject)
SM_TRAIT(L3ActivateAAPDPContextRequest)
SM_TRAIT(L3ActivateAAPDPContextAccept)
SM_TRAIT(L3ActivateAAPDPContextReject)
SM_TRAIT(L3DeactivateAAPDPContextRequest)
SM_TRAIT(L3DeactivateAAPDPContextAccept)
SM_TRAIT(L3ActivateMBMSContextRequest)
SM_TRAIT(L3ActivateMBMSContextAccept)
SM_TRAIT(L3ActivateMBMSContextReject)
SM_TRAIT(L3RequestMBMSContextActivation)
SM_TRAIT(L3RequestMBMSContextActivationReject)
SM_TRAIT(L3RequestSecondaryPDPContextActivation)
SM_TRAIT(L3RequestSecondaryPDPContextActivationReject)
SM_TRAIT(L3SMNotification)
#undef SM_TRAIT

/* ── SMS messages (19 types: 5 CP + 14 L3) ── */
#define SMS_TRAIT(T) template<> struct MessageTraits<T> { static constexpr L3PD pd = L3PD::SMS; static constexpr int mti = T::MTI; };
SMS_TRAIT(L3CPData)
SMS_TRAIT(L3CPAck)
SMS_TRAIT(L3CPErr)
SMS_TRAIT(L3CPStatus)
SMS_TRAIT(L3CPSMT)
SMS_TRAIT(L3SMSStatusReport)
SMS_TRAIT(L3SMSProvidedReplyExpected)
SMS_TRAIT(L3SMSSubmitRep)
SMS_TRAIT(L3SMSDeliver)
SMS_TRAIT(L3SMSDeliverRep)
SMS_TRAIT(L3SMSStatusReportAck)
SMS_TRAIT(L3SMSStatusReportReject)
SMS_TRAIT(L3SMSTSReject)
SMS_TRAIT(L3SMSSubmitDeferred)
SMS_TRAIT(L3SMSSubmitReject)
SMS_TRAIT(L3SMSSFProvidedRep)
SMS_TRAIT(L3SMSSFProvidedRepAck)
SMS_TRAIT(L3SMSNotification)
SMS_TRAIT(L3SMSShortCodeInfo)
#undef SMS_TRAIT

/* ── BCC messages (6 types) ── */
#define BCC_TRAIT(T) template<> struct MessageTraits<T> { static constexpr L3PD pd = L3PD::BroadcastCallControl; static constexpr int mti = T::MTI; };
BCC_TRAIT(L3BCCSetup)
BCC_TRAIT(L3BCCProceeding)
BCC_TRAIT(L3BCCConnect)
BCC_TRAIT(L3BCCDisconnect)
BCC_TRAIT(L3BCCRelease)
BCC_TRAIT(L3BCCReleaseComplete)
BCC_TRAIT(L3BCCCallConfirmed)
BCC_TRAIT(L3BCCConnectAcknowledge)
#undef BCC_TRAIT

/* ── GCC messages (7 types) ── */
#define GCC_TRAIT(T) template<> struct MessageTraits<T> { static constexpr L3PD pd = L3PD::GroupCallControl; static constexpr int mti = T::MTI; };
GCC_TRAIT(L3GCCSetup)
GCC_TRAIT(L3GCCAcknowledge)
GCC_TRAIT(L3GCCProceeding)
GCC_TRAIT(L3GCCConnect)
GCC_TRAIT(L3GCCDisconnect)
GCC_TRAIT(L3GCCRelease)
GCC_TRAIT(L3GCCReleaseComplete)
GCC_TRAIT(L3GCCCallConfirmed)
#undef GCC_TRAIT

/* ── LS messages (2 types) ── */
#define LS_TRAIT(T) template<> struct MessageTraits<T> { static constexpr L3PD pd = L3PD::Location; static constexpr int mti = T::MTI; };
LS_TRAIT(L3LocationServiceRequest)
LS_TRAIT(L3LocationServiceProviderMessage)
#undef LS_TRAIT

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
        case L3PD::GPRSMobilityManagement: {
            buf[0] = static_cast<uint8_t>((static_cast<uint8_t>(pd) & 0x0F) << 4);
            buf[1] = static_cast<uint8_t>(mti & 0xFF);
            break;
        }
        case L3PD::GPRSSessionManagement: {
            buf[0] = static_cast<uint8_t>((static_cast<uint8_t>(pd) & 0x0F) << 4);
            buf[1] = static_cast<uint8_t>(mti & 0xFF);
            break;
        }
        case L3PD::SMS: {
            buf[0] = static_cast<uint8_t>((static_cast<uint8_t>(pd) & 0x0F) << 4);
            buf[1] = static_cast<uint8_t>(mti & 0xFF);
            break;
        }
        case L3PD::BroadcastCallControl: {
            buf[0] = static_cast<uint8_t>((static_cast<uint8_t>(pd) & 0x0F) << 4 | (ti & 0x07) << 1);
            if (tif) buf[0] |= 0x01;
            buf[1] = static_cast<uint8_t>(((mti & 0x3F) << 2) | 0);
            break;
        }
        case L3PD::GroupCallControl: {
            buf[0] = static_cast<uint8_t>((static_cast<uint8_t>(pd) & 0x0F) << 4 | (ti & 0x07) << 1);
            if (tif) buf[0] |= 0x01;
            buf[1] = static_cast<uint8_t>(((mti & 0x3F) << 2) | 0);
            break;
        }
        case L3PD::Location: {
            buf[0] = static_cast<uint8_t>((static_cast<uint8_t>(pd) & 0x0F) << 4);
            buf[1] = static_cast<uint8_t>(mti & 0xFF);
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
        case L3ConfigurationChangeCommand::MTI: return L3ConfigurationChangeCommand::parse(reader).map([](L3ConfigurationChangeCommand v){ return RRM(std::move(v)); });
        case L3ConfigurationChangeAcknowledge::MTI: return L3ConfigurationChangeAcknowledge::parse(reader).map([](L3ConfigurationChangeAcknowledge v){ return RRM(std::move(v)); });
        case L3ConfigurationChangeReject::MTI: return L3ConfigurationChangeReject::parse(reader).map([](L3ConfigurationChangeReject v){ return RRM(std::move(v)); });
        case L3PartialRelease::MTI: return L3PartialRelease::parse(reader).map([](L3PartialRelease v){ return RRM(std::move(v)); });
        case L3PartialReleaseComplete::MTI: return L3PartialReleaseComplete::parse(reader).map([](L3PartialReleaseComplete v){ return RRM(std::move(v)); });
        case L3ExtendedMeasurementReport::MTI: return L3ExtendedMeasurementReport::parse(reader).map([](L3ExtendedMeasurementReport v){ return RRM(std::move(v)); });
        case L3ExtendedMeasurementOrder::MTI: return L3ExtendedMeasurementOrder::parse(reader).map([](L3ExtendedMeasurementOrder v){ return RRM(std::move(v)); });
        case L3FrequencyRedefinition::MTI: return L3FrequencyRedefinition::parse(reader).map([](L3FrequencyRedefinition v){ return RRM(std::move(v)); });
        case L3NotificationResponse::MTI: return L3NotificationResponse::parse(reader).map([](L3NotificationResponse v){ return RRM(std::move(v)); });
        case L3VGCSUplinkGrant::MTI: return L3VGCSUplinkGrant::parse(reader).map([](L3VGCSUplinkGrant v){ return RRM(std::move(v)); });
        case L3NotificationNCH::MTI: return L3NotificationNCH::parse(reader).map([](L3NotificationNCH v){ return RRM(std::move(v)); });
        case L3TalkerIndication::MTI: return L3TalkerIndication::parse(reader).map([](L3TalkerIndication v){ return RRM(std::move(v)); });
        case L3UplinkRelease::MTI: return L3UplinkRelease::parse(reader).map([](L3UplinkRelease v){ return RRM(std::move(v)); });
        case L3UplinkBusy::MTI: return L3UplinkBusy::parse(reader).map([](L3UplinkBusy v){ return RRM(std::move(v)); });
        case L3PriorityUplinkRequest::MTI: return L3PriorityUplinkRequest::parse(reader).map([](L3PriorityUplinkRequest v){ return RRM(std::move(v)); });
        case L3DataIndication::MTI: return L3DataIndication::parse(reader).map([](L3DataIndication v){ return RRM(std::move(v)); });
        case L3DataIndication2::MTI: return L3DataIndication2::parse(reader).map([](L3DataIndication2 v){ return RRM(std::move(v)); });
        case L3DTMAssignmentFailure::MTI: return L3DTMAssignmentFailure::parse(reader).map([](L3DTMAssignmentFailure v){ return RRM(std::move(v)); });
        case L3DTMReject::MTI: return L3DTMReject::parse(reader).map([](L3DTMReject v){ return RRM(std::move(v)); });
        case L3DTMRequest::MTI: return L3DTMRequest::parse(reader).map([](L3DTMRequest v){ return RRM(std::move(v)); });
        case L3PacketAssignment::MTI: return L3PacketAssignment::parse(reader).map([](L3PacketAssignment v){ return RRM(std::move(v)); });
        case L3DTMAssignmentCommand::MTI: return L3DTMAssignmentCommand::parse(reader).map([](L3DTMAssignmentCommand v){ return RRM(std::move(v)); });
        case L3DTMInformation::MTI: return L3DTMInformation::parse(reader).map([](L3DTMInformation v){ return RRM(std::move(v)); });
        case L3PacketInformation::MTI: return L3PacketInformation::parse(reader).map([](L3PacketInformation v){ return RRM(std::move(v)); });
        case L3UTRANClassmarkChange::MTI: return L3UTRANClassmarkChange::parse(reader).map([](L3UTRANClassmarkChange v){ return RRM(std::move(v)); });
        case L3CDMA2000ClassmarkChange::MTI: return L3CDMA2000ClassmarkChange::parse(reader).map([](L3CDMA2000ClassmarkChange v){ return RRM(std::move(v)); });
        case L3IntersysToUTRANHOCommand::MTI: return L3IntersysToUTRANHOCommand::parse(reader).map([](L3IntersysToUTRANHOCommand v){ return RRM(std::move(v)); });
        case L3IntersysToCDMA2000HOCommand::MTI: return L3IntersysToCDMA2000HOCommand::parse(reader).map([](L3IntersysToCDMA2000HOCommand v){ return RRM(std::move(v)); });
        case L3GERANIUClassmarkChange::MTI: return L3GERANIUClassmarkChange::parse(reader).map([](L3GERANIUClassmarkChange v){ return RRM(std::move(v)); });
        case L3SystemInformationType14::MTI: return L3SystemInformationType14::parse(reader).map([](L3SystemInformationType14 v){ return RRM(std::move(v)); });
        case L3SystemInformationType15::MTI: return L3SystemInformationType15::parse(reader).map([](L3SystemInformationType15 v){ return RRM(std::move(v)); });
        case L3SystemInformationType18::MTI: return L3SystemInformationType18::parse(reader).map([](L3SystemInformationType18 v){ return RRM(std::move(v)); });
        case L3SystemInformationType19::MTI: return L3SystemInformationType19::parse(reader).map([](L3SystemInformationType19 v){ return RRM(std::move(v)); });
        case L3SystemInformationType20::MTI: return L3SystemInformationType20::parse(reader).map([](L3SystemInformationType20 v){ return RRM(std::move(v)); });
        case L3SystemInformationType13alt::MTI: return L3SystemInformationType13alt::parse(reader).map([](L3SystemInformationType13alt v){ return RRM(std::move(v)); });
        case L3SystemInformationType2n::MTI: return L3SystemInformationType2n::parse(reader).map([](L3SystemInformationType2n v){ return RRM(std::move(v)); });
        case L3SystemInformationType21::MTI: return L3SystemInformationType21::parse(reader).map([](L3SystemInformationType21 v){ return RRM(std::move(v)); });
        case L3SystemInformationType22::MTI: return L3SystemInformationType22::parse(reader).map([](L3SystemInformationType22 v){ return RRM(std::move(v)); });
        case L3SystemInformationType23::MTI: return L3SystemInformationType23::parse(reader).map([](L3SystemInformationType23 v){ return RRM(std::move(v)); });
        // System Information Type 2quater (GSM 04.08 9.1.34a, MTI=0x07)
        case L3SystemInformationType2quater::MTI: return L3SystemInformationType2quater::parse(reader).map([](L3SystemInformationType2quater v){ return RRM(std::move(v)); });
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
        // CM-Request (24.008 9.2.8)
        case L3CMRequest::MTI:                     return L3CMRequest::parse(reader).map([](L3CMRequest v){ return MMM(std::move(v)); });
        // MM-Paging (24.008 9.2.12)
        case L3PagingMM::MTI:                      return L3PagingMM::parse(reader).map([](L3PagingMM v){ return MMM(std::move(v)); });
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
        // Facility (24.008 9.3.21)
        case L3Facility::MTI:               return L3Facility::parse(reader).map([ti](L3Facility v){ v.ti(ti); return CCM(std::move(v)); });
        // Modify (24.008 9.3.15)
        case L3Modify::MTI:                 return L3Modify::parse(reader).map([ti](L3Modify v){ v.ti(ti); return CCM(std::move(v)); });
        // UnitData (24.008 9.3.16)
        case L3UnitData::MTI:               return L3UnitData::parse(reader).map([ti](L3UnitData v){ v.ti(ti); return CCM(std::move(v)); });
        // UnitDataAck (24.008 9.3.16a)
        case L3UnitDataAck::MTI:            return L3UnitDataAck::parse(reader).map([ti](L3UnitDataAck v){ v.ti(ti); return CCM(std::move(v)); });
        // ErrorIndication (24.008 9.3.16b)
        case L3ErrorIndication::MTI:        return L3ErrorIndication::parse(reader).map([ti](L3ErrorIndication v){ v.ti(ti); return CCM(std::move(v)); });
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

// GMM messages (24.008 Table 10.4)
Expected<GMM> parseL3GMM(BitReader& reader, int mti) {
    switch (mti) {
        // Attach procedure (24.008 9.4.1-9.4.4)
        case L3AttachRequest::MTI:                  return L3AttachRequest::parse(reader).map([](L3AttachRequest v){ return GMM(std::move(v)); });
        case L3AttachAccept::MTI:                   return L3AttachAccept::parse(reader).map([](L3AttachAccept v){ return GMM(std::move(v)); });
        case L3AttachComplete::MTI:                 return L3AttachComplete::parse(reader).map([](L3AttachComplete v){ return GMM(std::move(v)); });
        case L3AttachReject::MTI:                   return L3AttachReject::parse(reader).map([](L3AttachReject v){ return GMM(std::move(v)); });
        // Detach procedure (24.008 9.4.5-9.4.6)
        case L3DetachRequest::MTI:                  return L3DetachRequest::parse(reader).map([](L3DetachRequest v){ return GMM(std::move(v)); });
        case L3DetachAccept::MTI:                   return L3DetachAccept::parse(reader).map([](L3DetachAccept v){ return GMM(std::move(v)); });
        // Routing Area Update (24.008 9.4.12-9.4.17)
        case L3RoutingAreaUpdateRequest::MTI:       return L3RoutingAreaUpdateRequest::parse(reader).map([](L3RoutingAreaUpdateRequest v){ return GMM(std::move(v)); });
        case L3RoutingAreaUpdateAccept::MTI:        return L3RoutingAreaUpdateAccept::parse(reader).map([](L3RoutingAreaUpdateAccept v){ return GMM(std::move(v)); });
        case L3RoutingAreaUpdateComplete::MTI:      return L3RoutingAreaUpdateComplete::parse(reader).map([](L3RoutingAreaUpdateComplete v){ return GMM(std::move(v)); });
        case L3RoutingAreaUpdateReject::MTI:        return L3RoutingAreaUpdateReject::parse(reader).map([](L3RoutingAreaUpdateReject v){ return GMM(std::move(v)); });
        // Service Request (24.008 9.4.20-9.4.22)
        case L3ServiceRequest::MTI:                 return L3ServiceRequest::parse(reader).map([](L3ServiceRequest v){ return GMM(std::move(v)); });
        case L3ServiceAccept::MTI:                  return L3ServiceAccept::parse(reader).map([](L3ServiceAccept v){ return GMM(std::move(v)); });
        case L3ServiceReject::MTI:                  return L3ServiceReject::parse(reader).map([](L3ServiceReject v){ return GMM(std::move(v)); });
        // P-TMSI Reallocation (24.008 9.4.8)
        case L3P_TMSIReallocationCommand::MTI:      return L3P_TMSIReallocationCommand::parse(reader).map([](L3P_TMSIReallocationCommand v){ return GMM(std::move(v)); });
        case L3P_TMSIReallocationComplete::MTI:     return L3P_TMSIReallocationComplete::parse(reader).map([](L3P_TMSIReallocationComplete v){ return GMM(std::move(v)); });
        // Authentication and Ciphering (24.008 9.4.9)
        case L3AuthenticationAndCipheringRequest::MTI: return L3AuthenticationAndCipheringRequest::parse(reader).map([](L3AuthenticationAndCipheringRequest v){ return GMM(std::move(v)); });
        case L3AuthenticationAndCipheringResponse::MTI: return L3AuthenticationAndCipheringResponse::parse(reader).map([](L3AuthenticationAndCipheringResponse v){ return GMM(std::move(v)); });
        case L3AuthenticationAndCipheringReject::MTI:  return L3AuthenticationAndCipheringReject::parse(reader).map([](L3AuthenticationAndCipheringReject v){ return GMM(std::move(v)); });
        // Identity (24.008 9.4.7, 9.4.10)
        case L3GMMIdentityRequest::MTI:             return L3GMMIdentityRequest::parse(reader).map([](L3GMMIdentityRequest v){ return GMM(std::move(v)); });
        case L3GMMIdentityResponse::MTI:            return L3GMMIdentityResponse::parse(reader).map([](L3GMMIdentityResponse v){ return GMM(std::move(v)); });
        // Auth Failure (24.008 9.4.23)
        case L3AuthenticationAndCipheringFailure::MTI: return L3AuthenticationAndCipheringFailure::parse(reader).map([](L3AuthenticationAndCipheringFailure v){ return GMM(std::move(v)); });
        // Status/Information (24.008 9.4.24)
        case L3GMMStatus::MTI:                      return L3GMMStatus::parse(reader).map([](L3GMMStatus v){ return GMM(std::move(v)); });
        case L3GMMInformation::MTI:                 return L3GMMInformation::parse(reader).map([](L3GMMInformation v){ return GMM(std::move(v)); });
        default:
            return Expected<GMM>::error(ParseError{ParseError::Code::InvalidMTI, "Unknown GMM MTI", static_cast<size_t>(mti)});
    }
}

// SM messages (24.008 Table 10.4a)
Expected<SM> parseL3SM(BitReader& reader, int mti) {
    switch (mti) {
        // Activate PDP Context (24.008 9.5.1-9.5.3)
        case L3ActivatePDPContextRequest::MTI:  return L3ActivatePDPContextRequest::parse(reader).map([](L3ActivatePDPContextRequest v){ return SM(std::move(v)); });
        case L3ActivatePDPContextAccept::MTI:   return L3ActivatePDPContextAccept::parse(reader).map([](L3ActivatePDPContextAccept v){ return SM(std::move(v)); });
        case L3ActivatePDPContextReject::MTI:   return L3ActivatePDPContextReject::parse(reader).map([](L3ActivatePDPContextReject v){ return SM(std::move(v)); });
        // Request PDP Context Activation (24.008 9.5.10)
        case L3RequestPDPContextActivation::MTI: return L3RequestPDPContextActivation::parse(reader).map([](L3RequestPDPContextActivation v){ return SM(std::move(v)); });
        case L3RequestPDPContextActivationReject::MTI: return L3RequestPDPContextActivationReject::parse(reader).map([](L3RequestPDPContextActivationReject v){ return SM(std::move(v)); });
        // Deactivate PDP Context (24.008 9.5.4-9.5.5)
        case L3DeactivatePDPContextRequest::MTI: return L3DeactivatePDPContextRequest::parse(reader).map([](L3DeactivatePDPContextRequest v){ return SM(std::move(v)); });
        case L3DeactivatePDPContextAccept::MTI:  return L3DeactivatePDPContextAccept::parse(reader).map([](L3DeactivatePDPContextAccept v){ return SM(std::move(v)); });
        // Modify PDP Context (24.008 9.5.6-9.5.8)
        case L3ModifyPDPContextRequest::MTI:    return L3ModifyPDPContextRequest::parse(reader).map([](L3ModifyPDPContextRequest v){ return SM(std::move(v)); });
        case L3ModifyPDPContextAccept::MTI:     return L3ModifyPDPContextAccept::parse(reader).map([](L3ModifyPDPContextAccept v){ return SM(std::move(v)); });
        case L3ModifyPDPContextRequestMS::MTI:  return L3ModifyPDPContextRequestMS::parse(reader).map([](L3ModifyPDPContextRequestMS v){ return SM(std::move(v)); });
        case L3ModifyPDPContextAcceptNet::MTI:  return L3ModifyPDPContextAcceptNet::parse(reader).map([](L3ModifyPDPContextAcceptNet v){ return SM(std::move(v)); });
        case L3ModifyPDPContextReject::MTI:     return L3ModifyPDPContextReject::parse(reader).map([](L3ModifyPDPContextReject v){ return SM(std::move(v)); });
        // Activate Secondary PDP Context (24.008 9.5.11-9.5.13)
        case L3ActivateSecondaryPDPContextRequest::MTI: return L3ActivateSecondaryPDPContextRequest::parse(reader).map([](L3ActivateSecondaryPDPContextRequest v){ return SM(std::move(v)); });
        case L3ActivateSecondaryPDPContextAccept::MTI: return L3ActivateSecondaryPDPContextAccept::parse(reader).map([](L3ActivateSecondaryPDPContextAccept v){ return SM(std::move(v)); });
        case L3ActivateSecondaryPDPContextReject::MTI: return L3ActivateSecondaryPDPContextReject::parse(reader).map([](L3ActivateSecondaryPDPContextReject v){ return SM(std::move(v)); });
        // Activate AA PDP Context (24.008 9.5.14-9.5.16)
        case L3ActivateAAPDPContextRequest::MTI: return L3ActivateAAPDPContextRequest::parse(reader).map([](L3ActivateAAPDPContextRequest v){ return SM(std::move(v)); });
        case L3ActivateAAPDPContextAccept::MTI: return L3ActivateAAPDPContextAccept::parse(reader).map([](L3ActivateAAPDPContextAccept v){ return SM(std::move(v)); });
        case L3ActivateAAPDPContextReject::MTI: return L3ActivateAAPDPContextReject::parse(reader).map([](L3ActivateAAPDPContextReject v){ return SM(std::move(v)); });
        // Deactivate AA PDP Context (24.008 9.5.17)
        case L3DeactivateAAPDPContextRequest::MTI: return L3DeactivateAAPDPContextRequest::parse(reader).map([](L3DeactivateAAPDPContextRequest v){ return SM(std::move(v)); });
        case L3DeactivateAAPDPContextAccept::MTI: return L3DeactivateAAPDPContextAccept::parse(reader).map([](L3DeactivateAAPDPContextAccept v){ return SM(std::move(v)); });
        // SM Status (24.008 9.5.9)
        case L3SMStatus::MTI:                   return L3SMStatus::parse(reader).map([](L3SMStatus v){ return SM(std::move(v)); });
        // Activate MBMS Context (24.008 9.5.18-9.5.20)
        case L3ActivateMBMSContextRequest::MTI: return L3ActivateMBMSContextRequest::parse(reader).map([](L3ActivateMBMSContextRequest v){ return SM(std::move(v)); });
        case L3ActivateMBMSContextAccept::MTI:  return L3ActivateMBMSContextAccept::parse(reader).map([](L3ActivateMBMSContextAccept v){ return SM(std::move(v)); });
        case L3ActivateMBMSContextReject::MTI:  return L3ActivateMBMSContextReject::parse(reader).map([](L3ActivateMBMSContextReject v){ return SM(std::move(v)); });
        // Request MBMS Context Activation (24.008 9.5.21-9.5.22)
        case L3RequestMBMSContextActivation::MTI: return L3RequestMBMSContextActivation::parse(reader).map([](L3RequestMBMSContextActivation v){ return SM(std::move(v)); });
        case L3RequestMBMSContextActivationReject::MTI: return L3RequestMBMSContextActivationReject::parse(reader).map([](L3RequestMBMSContextActivationReject v){ return SM(std::move(v)); });
        // Request Secondary PDP Context Activation (24.008 9.5.23-9.5.24)
        case L3RequestSecondaryPDPContextActivation::MTI: return L3RequestSecondaryPDPContextActivation::parse(reader).map([](L3RequestSecondaryPDPContextActivation v){ return SM(std::move(v)); });
        case L3RequestSecondaryPDPContextActivationReject::MTI: return L3RequestSecondaryPDPContextActivationReject::parse(reader).map([](L3RequestSecondaryPDPContextActivationReject v){ return SM(std::move(v)); });
        // SM Notification (24.008 9.5.25)
        case L3SMNotification::MTI:             return L3SMNotification::parse(reader).map([](L3SMNotification v){ return SM(std::move(v)); });
        default:
            return Expected<SM>::error(ParseError{ParseError::Code::InvalidMTI, "Unknown SM MTI", static_cast<size_t>(mti)});
    }
}

// SMS messages (24.008 Table 10.6a, 24.011 sections 7-8)
// Note: MTI 0x12 and 0x13 overlap between CP-layer and L3-layer messages.
// CP-STATUS(0x12) vs SMSProvidedReplyExpected(0x12), CP-SMT(0x13) vs SMSSubmitRep(0x13).
// For backward compatibility, overlapping MTIs dispatch to CP messages.
Expected<SMS> parseL3SMS(BitReader& reader, int mti) {
    switch (mti) {
        // CP-DATA (24.011 8.1.2)
        case L3CPData::MTI:       return L3CPData::parse(reader).map([](L3CPData v){ return SMS(std::move(v)); });
        // CP-ACK (24.011 8.1.3)
        case L3CPAck::MTI:        return L3CPAck::parse(reader).map([](L3CPAck v){ return SMS(std::move(v)); });
        // CP-ERROR (24.011 8.1.4)
        case L3CPErr::MTI:        return L3CPErr::parse(reader).map([](L3CPErr v){ return SMS(std::move(v)); });
        // CP-STATUS (24.011 8.1.5) — MTI 0x12 overlaps with SMSProvidedReplyExpected; CP takes precedence
        case L3CPStatus::MTI:     return L3CPStatus::parse(reader).map([](L3CPStatus v){ return SMS(std::move(v)); });
        // CP-SMT (24.011 8.1.6) — MTI 0x13 overlaps with SMSSubmitRep; CP takes precedence
        case L3CPSMT::MTI:        return L3CPSMT::parse(reader).map([](L3CPSMT v){ return SMS(std::move(v)); });
        // SMS Status Report (24.008 9.6.1)
        case L3SMSStatusReport::MTI: return L3SMSStatusReport::parse(reader).map([](L3SMSStatusReport v){ return SMS(std::move(v)); });
        // SMS Deliver (24.008 9.6.4)
        case L3SMSDeliver::MTI:   return L3SMSDeliver::parse(reader).map([](L3SMSDeliver v){ return SMS(std::move(v)); });
        // SMS Deliver Reply (24.008 9.6.5)
        case L3SMSDeliverRep::MTI: return L3SMSDeliverRep::parse(reader).map([](L3SMSDeliverRep v){ return SMS(std::move(v)); });
        // SMS Status Report Ack (24.008 9.6.6)
        case L3SMSStatusReportAck::MTI: return L3SMSStatusReportAck::parse(reader).map([](L3SMSStatusReportAck v){ return SMS(std::move(v)); });
        // SMS Status Report Reject (24.008 9.6.7)
        case L3SMSStatusReportReject::MTI: return L3SMSStatusReportReject::parse(reader).map([](L3SMSStatusReportReject v){ return SMS(std::move(v)); });
        // SMS TS Reject (24.008 9.6.8)
        case L3SMSTSReject::MTI:  return L3SMSTSReject::parse(reader).map([](L3SMSTSReject v){ return SMS(std::move(v)); });
        // SMS Submit Deferred (24.008 9.6.9)
        case L3SMSSubmitDeferred::MTI: return L3SMSSubmitDeferred::parse(reader).map([](L3SMSSubmitDeferred v){ return SMS(std::move(v)); });
        // SMS Submit Reject (24.008 9.6.10)
        case L3SMSSubmitReject::MTI: return L3SMSSubmitReject::parse(reader).map([](L3SMSSubmitReject v){ return SMS(std::move(v)); });
        // SMS SSF Provided Reply (24.008 9.6.11)
        case L3SMSSFProvidedRep::MTI: return L3SMSSFProvidedRep::parse(reader).map([](L3SMSSFProvidedRep v){ return SMS(std::move(v)); });
        // SMS SSF Provided Reply Ack (24.008 9.6.12)
        case L3SMSSFProvidedRepAck::MTI: return L3SMSSFProvidedRepAck::parse(reader).map([](L3SMSSFProvidedRepAck v){ return SMS(std::move(v)); });
        // SMS Notification (24.008 9.6.13)
        case L3SMSNotification::MTI: return L3SMSNotification::parse(reader).map([](L3SMSNotification v){ return SMS(std::move(v)); });
        // SMS Short Code Info (24.008 9.6.14)
        case L3SMSShortCodeInfo::MTI: return L3SMSShortCodeInfo::parse(reader).map([](L3SMSShortCodeInfo v){ return SMS(std::move(v)); });
        default:
            return Expected<SMS>::error(ParseError{ParseError::Code::InvalidMTI, "Unknown SMS MTI", static_cast<size_t>(mti)});
    }
}

// BCC messages (44.018 Table 10.4.3)
Expected<BCCM> parseL3BCC(BitReader& reader, int mti, unsigned ti) {
    switch (mti) {
        // Broadcast Call Setup (44.018 9.6.2.2)
        case L3BCCSetup::MTI:                  return L3BCCSetup::parse(reader).map([ti](L3BCCSetup v){ v.ti(ti); return BCCM(std::move(v)); });
        // Broadcast Call Proceeding (44.018 9.6.2.3)
        case L3BCCProceeding::MTI:             return L3BCCProceeding::parse(reader).map([ti](L3BCCProceeding v){ v.ti(ti); return BCCM(std::move(v)); });
        // Broadcast Call Connect (44.018 9.6.2.6)
        case L3BCCConnect::MTI:                return L3BCCConnect::parse(reader).map([ti](L3BCCConnect v){ v.ti(ti); return BCCM(std::move(v)); });
        // Broadcast Call Disconnect (44.018 9.6.2.7)
        case L3BCCDisconnect::MTI:             return L3BCCDisconnect::parse(reader).map([ti](L3BCCDisconnect v){ v.ti(ti); return BCCM(std::move(v)); });
        // Broadcast Call Release (44.018 9.6.2.8)
        case L3BCCRelease::MTI:                return L3BCCRelease::parse(reader).map([ti](L3BCCRelease v){ v.ti(ti); return BCCM(std::move(v)); });
        // Broadcast Call Release Complete (44.018 9.6.2.9)
        case L3BCCReleaseComplete::MTI:        return L3BCCReleaseComplete::parse(reader).map([ti](L3BCCReleaseComplete v){ v.ti(ti); return BCCM(std::move(v)); });
        // Broadcast Call Confirmed (44.018 9.6.2.5)
        case L3BCCCallConfirmed::MTI:          return L3BCCCallConfirmed::parse(reader).map([ti](L3BCCCallConfirmed v){ v.ti(ti); return BCCM(std::move(v)); });
        // Broadcast Connect Acknowledge (44.018 9.6.2.10)
        case L3BCCConnectAcknowledge::MTI:     return L3BCCConnectAcknowledge::parse(reader).map([ti](L3BCCConnectAcknowledge v){ v.ti(ti); return BCCM(std::move(v)); });
        default:
            return Expected<BCCM>::error(ParseError{ParseError::Code::InvalidMTI, "Unknown BCC MTI", static_cast<size_t>(mti)});
    }
}

// GCC messages (44.018 Table 10.4.4)
Expected<GCCM> parseL3GCC(BitReader& reader, int mti, unsigned ti) {
    switch (mti) {
        // Group Call Setup (44.018 9.7.2.2)
        case L3GCCSetup::MTI:                  return L3GCCSetup::parse(reader).map([ti](L3GCCSetup v){ v.ti(ti); return GCCM(std::move(v)); });
        // Group Call Acknowledge (44.018 9.7.2.3)
        case L3GCCAcknowledge::MTI:            return L3GCCAcknowledge::parse(reader).map([ti](L3GCCAcknowledge v){ v.ti(ti); return GCCM(std::move(v)); });
        // Group Call Proceeding (44.018 9.7.2.4)
        case L3GCCProceeding::MTI:             return L3GCCProceeding::parse(reader).map([ti](L3GCCProceeding v){ v.ti(ti); return GCCM(std::move(v)); });
        // Group Call Connect (44.018 9.7.2.6)
        case L3GCCConnect::MTI:                return L3GCCConnect::parse(reader).map([ti](L3GCCConnect v){ v.ti(ti); return GCCM(std::move(v)); });
        // Group Call Disconnect (44.018 9.7.2.7)
        case L3GCCDisconnect::MTI:             return L3GCCDisconnect::parse(reader).map([ti](L3GCCDisconnect v){ v.ti(ti); return GCCM(std::move(v)); });
        // Group Call Release (44.018 9.7.2.8)
        case L3GCCRelease::MTI:                return L3GCCRelease::parse(reader).map([ti](L3GCCRelease v){ v.ti(ti); return GCCM(std::move(v)); });
        // Group Call Release Complete (44.018 9.7.2.9)
        case L3GCCReleaseComplete::MTI:        return L3GCCReleaseComplete::parse(reader).map([ti](L3GCCReleaseComplete v){ v.ti(ti); return GCCM(std::move(v)); });
        // Group Call Confirmed (44.018 9.7.2.5)
        case L3GCCCallConfirmed::MTI:          return L3GCCCallConfirmed::parse(reader).map([ti](L3GCCCallConfirmed v){ v.ti(ti); return GCCM(std::move(v)); });
        default:
            return Expected<GCCM>::error(ParseError{ParseError::Code::InvalidMTI, "Unknown GCC MTI", static_cast<size_t>(mti)});
    }
}

// LS messages (TS 44.031)
Expected<LSM> parseL3LS(BitReader& reader, int mti) {
    switch (mti) {
        // Location Service Request (TS 44.031 9.1.2)
        case L3LocationServiceRequest::MTI:            return L3LocationServiceRequest::parse(reader).map([](L3LocationServiceRequest v){ return LSM(std::move(v)); });
        // Location Service Provider Message (TS 44.031 9.1.3)
        case L3LocationServiceProviderMessage::MTI:    return L3LocationServiceProviderMessage::parse(reader).map([](L3LocationServiceProviderMessage v){ return LSM(std::move(v)); });
        default:
            return Expected<LSM>::error(ParseError{ParseError::Code::InvalidMTI, "Unknown LS MTI", static_cast<size_t>(mti)});
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

    // For 4-byte and 7-byte data: handle short messages (no standard L3 header).
    // HandoverAccess is 4 bytes, SynchronizationChannelInformation is 7 bytes.
    // These are RR short messages but their first byte's high nibble may match
    // any PD value (including BCC=0x01, GCC=0x00), since they have no standard header.
    if (data.size() == 4 || data.size() == 7) {
        uint8_t pdNibble = (data[0] >> 4) & 0x0F;

        // RR PD: try standard RR parsing first, then fall back to short message.
        if (pdNibble == static_cast<uint8_t>(L3PD::RadioResource)) {
            auto hdrResult = parseL3Header(data);
            if (hdrResult) {
                size_t bodyBits = (data.size() - 2) * 8;
                BitReader reader(data.data() + 2, bodyBits);
                auto rrRes = detail::parseL3RR(reader, hdrResult.value().mti);
                if (rrRes) return rrRes.map([](RRM v){ return ParsedMessage(std::move(v)); });
            }
        }

        // BCC/GCC PD: short messages may have first byte that looks like BCC/GCC header.
        // Try short message handler first, then fall through to standard BCC/GCC parsing.
        if (pdNibble == static_cast<uint8_t>(L3PD::BroadcastCallControl) ||
            pdNibble == static_cast<uint8_t>(L3PD::GroupCallControl)) {
            if (data.size() == 4) {
                BitReader reader(data.data(), 32);
                auto res = L3HandoverAccess::parse(reader);
                if (res) return res.map([](L3HandoverAccess v){ return ParsedMessage(RRM(std::move(v))); });
            }
            if (data.size() == 7) {
                BitReader reader(data.data(), 56);
                auto res = L3SynchronizationChannelInformation::parse(reader);
                if (res) return res.map([](L3SynchronizationChannelInformation v){ return ParsedMessage(RRM(std::move(v))); });
            }
        }

        // RR standard parse failed; fall back to short message handler.
        if (pdNibble == static_cast<uint8_t>(L3PD::RadioResource)) {
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

        case L3PD::GPRSMobilityManagement:
            return detail::parseL3GMM(reader, hdr.mti)
                .map([](GMM v){ return ParsedMessage(std::move(v)); });

        case L3PD::GPRSSessionManagement:
            return detail::parseL3SM(reader, hdr.mti)
                .map([](SM v){ return ParsedMessage(std::move(v)); });

        case L3PD::SMS:
            return detail::parseL3SMS(reader, hdr.mti)
                .map([](SMS v){ return ParsedMessage(std::move(v)); });

        case L3PD::BroadcastCallControl:
            return detail::parseL3BCC(reader, hdr.mti, hdr.ti)
                .map([](BCCM v){ return ParsedMessage(std::move(v)); });

        case L3PD::GroupCallControl:
            return detail::parseL3GCC(reader, hdr.mti, hdr.ti)
                .map([](GCCM v){ return ParsedMessage(std::move(v)); });

        case L3PD::Location:
            return detail::parseL3LS(reader, hdr.mti)
                .map([](LSM v){ return ParsedMessage(std::move(v)); });

        default: {
            auto* handler = cfg.getPDHandler(hdr.pd);
            if (handler) {
                return Expected<ParsedMessage>::error(
                    ParseError{ParseError::Code::UnsupportedFeature, "Custom PD handler not yet wired"});
            }
            // For 7-byte data with unsupported PD, try SynchronizationChannelInformation.
            if (data.size() == 7) {
                BitReader rawReader(data.data(), 56);
                auto res = L3SynchronizationChannelInformation::parse(rawReader);
                if (res) return res.map([](L3SynchronizationChannelInformation v){ return ParsedMessage(RRM(std::move(v))); });
            }
            // For 4-byte data with unsupported PD, try HandoverAccess.
            if (data.size() == 4) {
                BitReader rawReader(data.data(), 32);
                auto res = L3HandoverAccess::parse(rawReader);
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
constexpr bool is_short_message_v = MessageTraits<ConcreteMsg>::mti >= 0x100;

template<typename ConcreteMsg>
Expected<size_t> writeL3Body(const ConcreteMsg& msg, uint8_t* out, size_t maxlen) {
    L3PD pd = message_pd<ConcreteMsg>();
    int mtiVal = message_mti<ConcreteMsg>();

    // Short messages: no standard L3 header, body only.
    if constexpr (is_short_message_v<ConcreteMsg>) {
        size_t bodyLen = msg.bodyLength();
        if (bodyLen > maxlen) return Expected<size_t>::error(
            ParseError{ParseError::Code::InvalidValue, "Buffer too small"});
        BitWriter writer(out, bodyLen * 8);
        msg.write(writer);
        return Expected<size_t>::hold(bodyLen);
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
