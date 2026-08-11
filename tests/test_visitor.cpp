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

#include <gtest/gtest.h>
#include <gsml3parser/parser.h>
#include <gsml3parser/visitor.h>
#include <gsml3parser/types.h>
#include <gsml3parser/enums.h>
#include <gsml3parser/rr/l3rrmessages.h>
#include <gsml3parser/mm/l3mmmessages.h>
#include <gsml3parser/cc/l3ccmessages.h>
#include <gsml3parser/ss/l3ssmessages.h>
#include <gsml3parser/gmm/l3gmmmessages.h>
#include <gsml3parser/sm/l3smmessages.h>
#include <gsml3parser/sms/l3smsmessages.h>
#include <gsml3parser/bcc/l3bccmessages.h>
#include <gsml3parser/gcc/l3gccmessages.h>

using namespace gsml3parser;

// =====================================================================
// tryGet on domain variants (RRM)
// =====================================================================

TEST(Visitor, tryGet_RRM_ChannelRelease) {
    RRM rrm{L3ChannelRelease{}};
    EXPECT_NE(tryGet<L3ChannelRelease>(rrm), nullptr);
    EXPECT_EQ(tryGet<L3PagingResponse>(rrm), nullptr);
}

TEST(Visitor, tryGet_RRM_PagingResponse) {
    RRM rrm{L3PagingResponse{}};
    EXPECT_NE(tryGet<L3PagingResponse>(rrm), nullptr);
    EXPECT_EQ(tryGet<L3ChannelRelease>(rrm), nullptr);
}

TEST(Visitor, tryGet_RRM_SystemInfoType1) {
    RRM rrm{L3SystemInformationType1{}};
    EXPECT_NE(tryGet<L3SystemInformationType1>(rrm), nullptr);
    EXPECT_EQ(tryGet<L3HandoverCommand>(rrm), nullptr);
}

TEST(Visitor, tryGet_RRM_Mutable) {
    L3ChannelRelease cr(RRCause::Normal_Event);
    RRM rrm{cr};
    auto* p = tryGet<L3ChannelRelease>(rrm);
    EXPECT_NE(p, nullptr);
}

// =====================================================================
// tryGet on domain variants (MMM)
// =====================================================================

TEST(Visitor, tryGet_MMM_CMServiceAccept) {
    MMM mmm{L3CMServiceAccept{}};
    EXPECT_NE(tryGet<L3CMServiceAccept>(mmm), nullptr);
    EXPECT_EQ(tryGet<L3AuthenticationRequest>(mmm), nullptr);
}

TEST(Visitor, tryGet_MMM_AuthenticationRequest) {
    MMM mmm{L3AuthenticationRequest{}};
    EXPECT_NE(tryGet<L3AuthenticationRequest>(mmm), nullptr);
    EXPECT_EQ(tryGet<L3CMServiceReject>(mmm), nullptr);
}

TEST(Visitor, tryGet_MMM_LocationUpdatingRequest) {
    MMM mmm{L3LocationUpdatingRequest{}};
    EXPECT_NE(tryGet<L3LocationUpdatingRequest>(mmm), nullptr);
    EXPECT_EQ(tryGet<L3IdentityResponse>(mmm), nullptr);
}

// =====================================================================
// tryGet on domain variants (CCM)
// =====================================================================

TEST(Visitor, tryGet_CCM_Setup) {
    CCM ccm{L3Setup{}};
    EXPECT_NE(tryGet<L3Setup>(ccm), nullptr);
    EXPECT_EQ(tryGet<L3Disconnect>(ccm), nullptr);
}

TEST(Visitor, tryGet_CCM_Disconnect) {
    CCM ccm{L3Disconnect{}};
    EXPECT_NE(tryGet<L3Disconnect>(ccm), nullptr);
    EXPECT_EQ(tryGet<L3Alerting>(ccm), nullptr);
}

TEST(Visitor, tryGet_CCM_Release) {
    CCM ccm{L3Release{}};
    EXPECT_NE(tryGet<L3Release>(ccm), nullptr);
    EXPECT_EQ(tryGet<L3CallProceeding>(ccm), nullptr);
}

// =====================================================================
// tryGet on domain variants (SSM)
// =====================================================================

TEST(Visitor, tryGet_SSM_FacilityMessage) {
    SSM ssm{L3SupServFacilityMessage{}};
    EXPECT_NE(tryGet<L3SupServFacilityMessage>(ssm), nullptr);
    EXPECT_EQ(tryGet<L3SupServRegisterMessage>(ssm), nullptr);
}

TEST(Visitor, tryGet_SSM_RegisterMessage) {
    SSM ssm{L3SupServRegisterMessage{}};
    EXPECT_NE(tryGet<L3SupServRegisterMessage>(ssm), nullptr);
    EXPECT_EQ(tryGet<L3SupServReleaseCompleteMessage>(ssm), nullptr);
}

TEST(Visitor, tryGet_SSM_ReleaseComplete) {
    SSM ssm{L3SupServReleaseCompleteMessage{}};
    EXPECT_NE(tryGet<L3SupServReleaseCompleteMessage>(ssm), nullptr);
    EXPECT_EQ(tryGet<L3SupServFacilityMessage>(ssm), nullptr);
}

// =====================================================================
// tryGet on top-level ParsedMessage — correct domain returns pointer
// =====================================================================

TEST(Visitor, tryGet_ParsedMessage_RR) {
    ParsedMessage msg{RRM{L3ChannelRelease{}}};
    EXPECT_NE(tryGet<L3ChannelRelease>(msg), nullptr);
}

TEST(Visitor, tryGet_ParsedMessage_MM) {
    ParsedMessage msg{MMM{L3CMServiceAccept{}}};
    EXPECT_NE(tryGet<L3CMServiceAccept>(msg), nullptr);
}

TEST(Visitor, tryGet_ParsedMessage_CC) {
    ParsedMessage msg{CCM{L3Setup{}}};
    EXPECT_NE(tryGet<L3Setup>(msg), nullptr);
}

TEST(Visitor, tryGet_ParsedMessage_SS) {
    ParsedMessage msg{SSM{L3SupServFacilityMessage{}}};
    EXPECT_NE(tryGet<L3SupServFacilityMessage>(msg), nullptr);
}

// =====================================================================
// tryGet on top-level ParsedMessage — wrong domain returns nullptr
// =====================================================================

TEST(Visitor, tryGet_ParsedMessage_WrongDomain_RR_ask_MM) {
    ParsedMessage msg{RRM{L3ChannelRelease{}}};
    EXPECT_EQ(tryGet<L3CMServiceAccept>(msg), nullptr);
}

TEST(Visitor, tryGet_ParsedMessage_WrongDomain_MM_ask_CC) {
    ParsedMessage msg{MMM{L3AuthenticationRequest{}}};
    EXPECT_EQ(tryGet<L3Setup>(msg), nullptr);
}

TEST(Visitor, tryGet_ParsedMessage_WrongDomain_CC_ask_RR) {
    ParsedMessage msg{CCM{L3Release{}}};
    EXPECT_EQ(tryGet<L3PagingResponse>(msg), nullptr);
}

TEST(Visitor, tryGet_ParsedMessage_WrongDomain_SS_ask_MM) {
    ParsedMessage msg{SSM{L3SupServRegisterMessage{}}};
    EXPECT_EQ(tryGet<L3LocationUpdatingRequest>(msg), nullptr);
}

// =====================================================================
// tryGet mutable on top-level ParsedMessage
// =====================================================================

TEST(Visitor, tryGet_ParsedMessage_Mutable_RR) {
    L3ChannelRelease cr(RRCause::Normal_Event);
    ParsedMessage msg{RRM{cr}};
    auto* p = tryGet<L3ChannelRelease>(msg);
    EXPECT_NE(p, nullptr);
}

TEST(Visitor, tryGet_ParsedMessage_Mutable_CC) {
    L3Setup setup{};
    ParsedMessage msg{CCM{setup}};
    auto* p = tryGet<L3Setup>(msg);
    EXPECT_NE(p, nullptr);
}

// =====================================================================
// messageName returns correct string for each domain
// =====================================================================

TEST(Visitor, messageName_RR_ChannelRelease) {
    ParsedMessage msg{RRM{L3ChannelRelease{}}};
    EXPECT_EQ(messageName(msg), "ChannelRelease");
}

TEST(Visitor, messageName_RR_PagingRequestType1) {
    ParsedMessage msg{RRM{L3PagingRequestType1{}}};
    EXPECT_EQ(messageName(msg), "PagingRequestType1");
}

TEST(Visitor, messageName_RR_SystemInfoType3) {
    ParsedMessage msg{RRM{L3SystemInformationType3{}}};
    EXPECT_EQ(messageName(msg), "SystemInformationType3");
}

TEST(Visitor, messageName_MM_CMServiceAccept) {
    ParsedMessage msg{MMM{L3CMServiceAccept{}}};
    EXPECT_EQ(messageName(msg), "CMServiceAccept");
}

TEST(Visitor, messageName_MM_AuthenticationReject) {
    ParsedMessage msg{MMM{L3AuthenticationReject{}}};
    EXPECT_EQ(messageName(msg), "AuthenticationReject");
}

TEST(Visitor, messageName_MM_LocationUpdatingAccept) {
    ParsedMessage msg{MMM{L3LocationUpdatingAccept{}}};
    EXPECT_EQ(messageName(msg), "LocationUpdatingAccept");
}

TEST(Visitor, messageName_CC_Setup) {
    ParsedMessage msg{CCM{L3Setup{}}};
    EXPECT_EQ(messageName(msg), "Setup");
}

TEST(Visitor, messageName_CC_ReleaseComplete) {
    ParsedMessage msg{CCM{L3ReleaseComplete{}}};
    EXPECT_EQ(messageName(msg), "ReleaseComplete");
}

TEST(Visitor, messageName_CC_StartDTMF) {
    ParsedMessage msg{CCM{L3StartDTMF{}}};
    EXPECT_EQ(messageName(msg), "StartDTMF");
}

TEST(Visitor, messageName_SS_Facility) {
    ParsedMessage msg{SSM{L3SupServFacilityMessage{}}};
    EXPECT_EQ(messageName(msg), "SupServFacilityMessage");
}

TEST(Visitor, messageName_SS_Register) {
    ParsedMessage msg{SSM{L3SupServRegisterMessage{}}};
    EXPECT_EQ(messageName(msg), "SupServRegisterMessage");
}

TEST(Visitor, messageName_SS_ReleaseComplete) {
    ParsedMessage msg{SSM{L3SupServReleaseCompleteMessage{}}};
    EXPECT_EQ(messageName(msg), "SupServReleaseCompleteMessage");
}

// =====================================================================
// messagePD returns correct L3PD for each domain
// =====================================================================

TEST(Visitor, messagePD_RR) {
    ParsedMessage msg{RRM{L3ChannelRelease{}}};
    EXPECT_EQ(messagePD(msg), L3PD::RadioResource);
}

TEST(Visitor, messagePD_MM) {
    ParsedMessage msg{MMM{L3CMServiceAccept{}}};
    EXPECT_EQ(messagePD(msg), L3PD::MobilityManagement);
}

TEST(Visitor, messagePD_CC) {
    ParsedMessage msg{CCM{L3Setup{}}};
    EXPECT_EQ(messagePD(msg), L3PD::CallControl);
}

TEST(Visitor, messagePD_SS) {
    ParsedMessage msg{SSM{L3SupServFacilityMessage{}}};
    EXPECT_EQ(messagePD(msg), L3PD::NonCallSS);
}

// =====================================================================
// messageMTI returns correct MTI for each domain
// =====================================================================

TEST(Visitor, messageMTI_RR) {
    ParsedMessage msg{RRM{L3ChannelRelease{}}};
    EXPECT_EQ(messageMTI(msg), L3ChannelRelease::MTI);
}

TEST(Visitor, messageMTI_MM) {
    ParsedMessage msg{MMM{L3CMServiceAccept{}}};
    EXPECT_EQ(messageMTI(msg), L3CMServiceAccept::MTI);
}

TEST(Visitor, messageMTI_CC) {
    ParsedMessage msg{CCM{L3Setup{}}};
    EXPECT_EQ(messageMTI(msg), L3Setup::MTI);
}

TEST(Visitor, messageMTI_SS) {
    ParsedMessage msg{SSM{L3SupServFacilityMessage{}}};
    EXPECT_EQ(messageMTI(msg), L3SupServFacilityMessage::MTI);
}

// =====================================================================
// Integration: parse from hex, then use visitor helpers
// =====================================================================

TEST(Visitor, ParseAndTryGet_RR) {
    ParsedMessage orig{RRM{L3ChannelRelease(RRCause::Normal_Event)}};
    auto hex = writeL3Hex(orig);
    ASSERT_TRUE(hex);
    auto res = parseL3Hex(hex.value());
    ASSERT_TRUE(res);
    EXPECT_NE(tryGet<L3ChannelRelease>(*res), nullptr);
    EXPECT_EQ(messagePD(*res), L3PD::RadioResource);
    EXPECT_EQ(messageMTI(*res), L3ChannelRelease::MTI);
}

TEST(Visitor, ParseAndTryGet_MM) {
    ParsedMessage orig{MMM{L3CMServiceAccept{}}};
    auto hex = writeL3Hex(orig);
    ASSERT_TRUE(hex);
    auto res = parseL3Hex(hex.value());
    ASSERT_TRUE(res);
    EXPECT_NE(tryGet<L3CMServiceAccept>(*res), nullptr);
    EXPECT_EQ(messagePD(*res), L3PD::MobilityManagement);
}

TEST(Visitor, ParseAndTryGet_CC) {
    ParsedMessage orig{CCM{L3Setup{}}};
    auto hex = writeL3Hex(orig);
    ASSERT_TRUE(hex);
    auto res = parseL3Hex(hex.value());
    ASSERT_TRUE(res);
    EXPECT_NE(tryGet<L3Setup>(*res), nullptr);
    EXPECT_EQ(messagePD(*res), L3PD::CallControl);
}

TEST(Visitor, ParseAndTryGet_SS) {
    ParsedMessage orig{SSM{L3SupServReleaseCompleteMessage{}}};
    auto hex = writeL3Hex(orig);
    ASSERT_TRUE(hex);
    auto res = parseL3Hex(hex.value());
    ASSERT_TRUE(res);
    EXPECT_NE(tryGet<L3SupServReleaseCompleteMessage>(*res), nullptr);
    EXPECT_EQ(messagePD(*res), L3PD::NonCallSS);
}

TEST(Visitor, ParseRoundTrip_Visitor) {
    ParsedMessage orig{RRM{L3ChannelRelease(RRCause::Normal_Event)}};
    auto hex = writeL3Hex(orig);
    ASSERT_TRUE(hex);
    auto reparsed = parseL3Hex(hex.value());
    ASSERT_TRUE(reparsed);
    EXPECT_NE(tryGet<L3ChannelRelease>(*reparsed), nullptr);
    EXPECT_EQ(messageName(*reparsed), messageName(orig));
    EXPECT_EQ(messagePD(*reparsed), messagePD(orig));
    EXPECT_EQ(messageMTI(*reparsed), messageMTI(orig));
}

// =====================================================================
// tryGet on domain variants (GMM)
// =====================================================================

TEST(Visitor, tryGet_GMM_AttachRequest) {
    GMM gmm{L3AttachRequest{}};
    EXPECT_NE(tryGet<L3AttachRequest>(gmm), nullptr);
    EXPECT_EQ(tryGet<L3DetachRequest>(gmm), nullptr);
}

TEST(Visitor, tryGet_GMM_RoutingAreaUpdateAccept) {
    GMM gmm{L3RoutingAreaUpdateAccept{}};
    EXPECT_NE(tryGet<L3RoutingAreaUpdateAccept>(gmm), nullptr);
    EXPECT_EQ(tryGet<L3ServiceRequest>(gmm), nullptr);
}

TEST(Visitor, tryGet_GMM_AuthAndCipheringRequest) {
    GMM gmm{L3AuthenticationAndCipheringRequest{}};
    EXPECT_NE(tryGet<L3AuthenticationAndCipheringRequest>(gmm), nullptr);
    EXPECT_EQ(tryGet<L3GMMIdentityRequest>(gmm), nullptr);
}

// =====================================================================
// tryGet on domain variants (SM)
// =====================================================================

TEST(Visitor, tryGet_SM_ActivatePDPRequest) {
    SM sm{L3ActivatePDPContextRequest{}};
    EXPECT_NE(tryGet<L3ActivatePDPContextRequest>(sm), nullptr);
    EXPECT_EQ(tryGet<L3DeactivatePDPContextRequest>(sm), nullptr);
}

TEST(Visitor, tryGet_SM_ModifyPDPAccept) {
    SM sm{L3ModifyPDPContextAccept{}};
    EXPECT_NE(tryGet<L3ModifyPDPContextAccept>(sm), nullptr);
    EXPECT_EQ(tryGet<L3SMStatus>(sm), nullptr);
}

TEST(Visitor, tryGet_SM_RequestPDPActivation) {
    SM sm{L3RequestPDPContextActivation{}};
    EXPECT_NE(tryGet<L3RequestPDPContextActivation>(sm), nullptr);
    EXPECT_EQ(tryGet<L3ActivatePDPContextRequest>(sm), nullptr);
}

TEST(Visitor, tryGet_SM_ActivateSecondaryPDP) {
    SM sm{L3ActivateSecondaryPDPContextRequest{}};
    EXPECT_NE(tryGet<L3ActivateSecondaryPDPContextRequest>(sm), nullptr);
    EXPECT_EQ(tryGet<L3ActivateAAPDPContextRequest>(sm), nullptr);
}

TEST(Visitor, tryGet_SM_ActivateAAPDP) {
    SM sm{L3ActivateAAPDPContextAccept{}};
    EXPECT_NE(tryGet<L3ActivateAAPDPContextAccept>(sm), nullptr);
    EXPECT_EQ(tryGet<L3DeactivateAAPDPContextAccept>(sm), nullptr);
}

TEST(Visitor, tryGet_SM_ActivateMBMS) {
    SM sm{L3ActivateMBMSContextRequest{}};
    EXPECT_NE(tryGet<L3ActivateMBMSContextRequest>(sm), nullptr);
    EXPECT_EQ(tryGet<L3RequestMBMSContextActivation>(sm), nullptr);
}

TEST(Visitor, tryGet_SM_SMNotification) {
    SM sm{L3SMNotification{}};
    EXPECT_NE(tryGet<L3SMNotification>(sm), nullptr);
    EXPECT_EQ(tryGet<L3SMStatus>(sm), nullptr);
}

// =====================================================================
// tryGet on domain variants (SMS)
// =====================================================================

TEST(Visitor, tryGet_SMS_CPData) {
    SMS sms{L3CPData{}};
    EXPECT_NE(tryGet<L3CPData>(sms), nullptr);
    EXPECT_EQ(tryGet<L3CPAck>(sms), nullptr);
}

TEST(Visitor, tryGet_SMS_CPSMT) {
    SMS sms{L3CPSMT{}};
    EXPECT_NE(tryGet<L3CPSMT>(sms), nullptr);
    EXPECT_EQ(tryGet<L3CPStatus>(sms), nullptr);
}

// =====================================================================
// tryGet on domain variants (BCCM)
// =====================================================================

TEST(Visitor, tryGet_BCC_Setup) {
    BCCM bcc{L3BCCSetup{}};
    EXPECT_NE(tryGet<L3BCCSetup>(bcc), nullptr);
    EXPECT_EQ(tryGet<L3BCCRelease>(bcc), nullptr);
}

TEST(Visitor, tryGet_BCC_Connect) {
    BCCM bcc{L3BCCConnect{}};
    EXPECT_NE(tryGet<L3BCCConnect>(bcc), nullptr);
    EXPECT_EQ(tryGet<L3BCCDisconnect>(bcc), nullptr);
}

// =====================================================================
// tryGet on domain variants (GCCM)
// =====================================================================

TEST(Visitor, tryGet_GCC_Setup) {
    GCCM gcc{L3GCCSetup{}};
    EXPECT_NE(tryGet<L3GCCSetup>(gcc), nullptr);
    EXPECT_EQ(tryGet<L3GCCRelease>(gcc), nullptr);
}

TEST(Visitor, tryGet_GCC_ReleaseComplete) {
    GCCM gcc{L3GCCReleaseComplete{}};
    EXPECT_NE(tryGet<L3GCCReleaseComplete>(gcc), nullptr);
    EXPECT_EQ(tryGet<L3GCCAcknowledge>(gcc), nullptr);
}

// =====================================================================
// tryGet on top-level ParsedMessage — GMM/SM/SMS/BCC/GCC domains
// =====================================================================

TEST(Visitor, tryGet_ParsedMessage_GMM) {
    ParsedMessage msg{GMM{L3AttachRequest{}}};
    EXPECT_NE(tryGet<L3AttachRequest>(msg), nullptr);
}

TEST(Visitor, tryGet_ParsedMessage_SM) {
    ParsedMessage msg{SM{L3ActivatePDPContextRequest{}}};
    EXPECT_NE(tryGet<L3ActivatePDPContextRequest>(msg), nullptr);
}

TEST(Visitor, tryGet_ParsedMessage_SMS) {
    ParsedMessage msg{SMS{L3CPData{}}};
    EXPECT_NE(tryGet<L3CPData>(msg), nullptr);
}

TEST(Visitor, tryGet_ParsedMessage_BCC) {
    ParsedMessage msg{BCCM{L3BCCSetup{}}};
    EXPECT_NE(tryGet<L3BCCSetup>(msg), nullptr);
}

TEST(Visitor, tryGet_ParsedMessage_GCC) {
    ParsedMessage msg{GCCM{L3GCCSetup{}}};
    EXPECT_NE(tryGet<L3GCCSetup>(msg), nullptr);
}

// =====================================================================
// tryGet on top-level ParsedMessage — wrong domain returns nullptr (new)
// =====================================================================

TEST(Visitor, tryGet_ParsedMessage_WrongDomain_GMM_ask_RR) {
    ParsedMessage msg{GMM{L3AttachRequest{}}};
    EXPECT_EQ(tryGet<L3ChannelRelease>(msg), nullptr);
}

TEST(Visitor, tryGet_ParsedMessage_WrongDomain_SM_ask_CC) {
    ParsedMessage msg{SM{L3ActivatePDPContextAccept{}}};
    EXPECT_EQ(tryGet<L3Setup>(msg), nullptr);
}

TEST(Visitor, tryGet_ParsedMessage_WrongDomain_SMS_ask_MM) {
    ParsedMessage msg{SMS{L3CPAck{}}};
    EXPECT_EQ(tryGet<L3CMServiceAccept>(msg), nullptr);
}

TEST(Visitor, tryGet_ParsedMessage_WrongDomain_BCC_ask_GMM) {
    ParsedMessage msg{BCCM{L3BCCConnect{}}};
    EXPECT_EQ(tryGet<L3AttachAccept>(msg), nullptr);
}

TEST(Visitor, tryGet_ParsedMessage_WrongDomain_GCC_ask_SM) {
    ParsedMessage msg{GCCM{L3GCCProceeding{}}};
    EXPECT_EQ(tryGet<L3SMStatus>(msg), nullptr);
}

// =====================================================================
// messageName returns correct string for GMM/SM/SMS/BCC/GCC
// =====================================================================

TEST(Visitor, messageName_GMM_AttachRequest) {
    ParsedMessage msg{GMM{L3AttachRequest{}}};
    EXPECT_EQ(messageName(msg), "AttachRequest");
}

TEST(Visitor, messageName_GMM_RoutingAreaUpdateReject) {
    ParsedMessage msg{GMM{L3RoutingAreaUpdateReject{}}};
    EXPECT_EQ(messageName(msg), "RoutingAreaUpdateReject");
}

TEST(Visitor, messageName_GMM_AuthAndCipheringFailure) {
    ParsedMessage msg{GMM{L3AuthenticationAndCipheringFailure{}}};
    EXPECT_EQ(messageName(msg), "AuthAndCipheringFailure");
}

TEST(Visitor, messageName_GMM_GMMStatus) {
    ParsedMessage msg{GMM{L3GMMStatus{}}};
    EXPECT_EQ(messageName(msg), "GMMStatus");
}

TEST(Visitor, messageName_SM_ActivatePDPRequest) {
    ParsedMessage msg{SM{L3ActivatePDPContextRequest{}}};
    EXPECT_EQ(messageName(msg), "ActivatePDPContextRequest");
}

TEST(Visitor, messageName_SM_DeactivatePDPAccept) {
    ParsedMessage msg{SM{L3DeactivatePDPContextAccept{}}};
    EXPECT_EQ(messageName(msg), "DeactivatePDPContextAccept");
}

TEST(Visitor, messageName_SM_ModifyPDPReject) {
    ParsedMessage msg{SM{L3ModifyPDPContextReject{}}};
    EXPECT_EQ(messageName(msg), "ModifyPDPContextReject");
}

TEST(Visitor, messageName_SM_SMStatus) {
    ParsedMessage msg{SM{L3SMStatus{}}};
    EXPECT_EQ(messageName(msg), "SMStatus");
}

TEST(Visitor, messageName_SM_RequestPDPActivation) {
    ParsedMessage msg{SM{L3RequestPDPContextActivation{}}};
    EXPECT_EQ(messageName(msg), "RequestPDPContextActivation");
}

TEST(Visitor, messageName_SM_ActivateSecondaryPDP) {
    ParsedMessage msg{SM{L3ActivateSecondaryPDPContextRequest{}}};
    EXPECT_EQ(messageName(msg), "ActivateSecondaryPDPContextRequest");
}

TEST(Visitor, messageName_SM_ActivateAAPDP) {
    ParsedMessage msg{SM{L3ActivateAAPDPContextRequest{}}};
    EXPECT_EQ(messageName(msg), "ActivateAAPDPContextRequest");
}

TEST(Visitor, messageName_SM_DeactivateAAPDP) {
    ParsedMessage msg{SM{L3DeactivateAAPDPContextAccept{}}};
    EXPECT_EQ(messageName(msg), "DeactivateAAPDPContextAccept");
}

TEST(Visitor, messageName_SM_ActivateMBMS) {
    ParsedMessage msg{SM{L3ActivateMBMSContextRequest{}}};
    EXPECT_EQ(messageName(msg), "ActivateMBMSContextRequest");
}

TEST(Visitor, messageName_SM_RequestMBMS) {
    ParsedMessage msg{SM{L3RequestMBMSContextActivation{}}};
    EXPECT_EQ(messageName(msg), "RequestMBMSContextActivation");
}

TEST(Visitor, messageName_SM_RequestSecondaryPDP) {
    ParsedMessage msg{SM{L3RequestSecondaryPDPContextActivation{}}};
    EXPECT_EQ(messageName(msg), "RequestSecondaryPDPContextActivation");
}

TEST(Visitor, messageName_SM_Notification) {
    ParsedMessage msg{SM{L3SMNotification{}}};
    EXPECT_EQ(messageName(msg), "SMNotification");
}

TEST(Visitor, messageName_SMS_CPData) {
    ParsedMessage msg{SMS{L3CPData{}}};
    EXPECT_EQ(messageName(msg), "CPData");
}

TEST(Visitor, messageName_SMS_CPAck) {
    ParsedMessage msg{SMS{L3CPAck{}}};
    EXPECT_EQ(messageName(msg), "CPAck");
}

TEST(Visitor, messageName_SMS_CPErr) {
    ParsedMessage msg{SMS{L3CPErr{}}};
    EXPECT_EQ(messageName(msg), "CPErr");
}

TEST(Visitor, messageName_SMS_CPStatus) {
    ParsedMessage msg{SMS{L3CPStatus{}}};
    EXPECT_EQ(messageName(msg), "CPStatus");
}

TEST(Visitor, messageName_SMS_CPSMT) {
    ParsedMessage msg{SMS{L3CPSMT{}}};
    EXPECT_EQ(messageName(msg), "CPSMT");
}

TEST(Visitor, messageName_BCC_Setup) {
    ParsedMessage msg{BCCM{L3BCCSetup{}}};
    EXPECT_EQ(messageName(msg), "BCCSetup");
}

TEST(Visitor, messageName_BCC_Connect) {
    ParsedMessage msg{BCCM{L3BCCConnect{}}};
    EXPECT_EQ(messageName(msg), "BCCConnect");
}

TEST(Visitor, messageName_BCC_ReleaseComplete) {
    ParsedMessage msg{BCCM{L3BCCReleaseComplete{}}};
    EXPECT_EQ(messageName(msg), "BCCReleaseComplete");
}

TEST(Visitor, messageName_GCC_Setup) {
    ParsedMessage msg{GCCM{L3GCCSetup{}}};
    EXPECT_EQ(messageName(msg), "GCCSetup");
}

TEST(Visitor, messageName_GCC_Acknowledge) {
    ParsedMessage msg{GCCM{L3GCCAcknowledge{}}};
    EXPECT_EQ(messageName(msg), "GCCAcknowledge");
}

TEST(Visitor, messageName_GCC_ReleaseComplete) {
    ParsedMessage msg{GCCM{L3GCCReleaseComplete{}}};
    EXPECT_EQ(messageName(msg), "GCCReleaseComplete");
}

// =====================================================================
// messagePD returns correct L3PD for GMM/SM/SMS/BCC/GCC
// =====================================================================

TEST(Visitor, messagePD_GMM) {
    ParsedMessage msg{GMM{L3AttachAccept{}}};
    EXPECT_EQ(messagePD(msg), L3PD::GPRSMobilityManagement);
}

TEST(Visitor, messagePD_SM) {
    ParsedMessage msg{SM{L3ActivatePDPContextAccept{}}};
    EXPECT_EQ(messagePD(msg), L3PD::GPRSSessionManagement);
}

TEST(Visitor, messagePD_SMS) {
    ParsedMessage msg{SMS{L3CPData{}}};
    EXPECT_EQ(messagePD(msg), L3PD::SMS);
}

TEST(Visitor, messagePD_BCC) {
    ParsedMessage msg{BCCM{L3BCCProceeding{}}};
    EXPECT_EQ(messagePD(msg), L3PD::BroadcastCallControl);
}

TEST(Visitor, messagePD_GCC) {
    ParsedMessage msg{GCCM{L3GCCConnect{}}};
    EXPECT_EQ(messagePD(msg), L3PD::GroupCallControl);
}

// =====================================================================
// messageMTI returns correct MTI for GMM/SM/SMS/BCC/GCC
// =====================================================================

TEST(Visitor, messageMTI_GMM) {
    ParsedMessage msg{GMM{L3DetachRequest{}}};
    EXPECT_EQ(messageMTI(msg), L3DetachRequest::MTI);
}

TEST(Visitor, messageMTI_SM) {
    ParsedMessage msg{SM{L3ModifyPDPContextRequest{}}};
    EXPECT_EQ(messageMTI(msg), L3ModifyPDPContextRequest::MTI);
}

TEST(Visitor, messageMTI_SMS) {
    ParsedMessage msg{SMS{L3CPErr{}}};
    EXPECT_EQ(messageMTI(msg), L3CPErr::MTI);
}

TEST(Visitor, messageMTI_BCC) {
    ParsedMessage msg{BCCM{L3BCCDisconnect{}}};
    EXPECT_EQ(messageMTI(msg), L3BCCDisconnect::MTI);
}

TEST(Visitor, messageMTI_GCC) {
    ParsedMessage msg{GCCM{L3GCCDisconnect{}}};
    EXPECT_EQ(messageMTI(msg), L3GCCDisconnect::MTI);
}

// =====================================================================
// Integration: parse from hex, then use visitor helpers (GMM/SM/SMS)
// =====================================================================

TEST(Visitor, ParseAndTryGet_GMM) {
    ParsedMessage orig{GMM{L3GMMStatus{GMMCause::ReqAccepted}}};
    auto hex = writeL3Hex(orig);
    ASSERT_TRUE(hex);
    auto res = parseL3Hex(hex.value());
    ASSERT_TRUE(res);
    EXPECT_NE(tryGet<L3GMMStatus>(*res), nullptr);
    EXPECT_EQ(messagePD(*res), L3PD::GPRSMobilityManagement);
    EXPECT_EQ(messageMTI(*res), L3GMMStatus::MTI);
}

TEST(Visitor, ParseAndTryGet_SM) {
    ParsedMessage orig{SM{L3ActivatePDPContextRequest{}}};
    auto hex = writeL3Hex(orig);
    ASSERT_TRUE(hex);
    auto res = parseL3Hex(hex.value());
    ASSERT_TRUE(res);
    EXPECT_NE(tryGet<L3ActivatePDPContextRequest>(*res), nullptr);
    EXPECT_EQ(messagePD(*res), L3PD::GPRSSessionManagement);
}

TEST(Visitor, ParseAndTryGet_SMS) {
    ParsedMessage orig{SMS{L3CPData{}}};
    auto hex = writeL3Hex(orig);
    ASSERT_TRUE(hex);
    auto res = parseL3Hex(hex.value());
    ASSERT_TRUE(res);
    EXPECT_NE(tryGet<L3CPData>(*res), nullptr);
    EXPECT_EQ(messagePD(*res), L3PD::SMS);
}

TEST(Visitor, ParseAndTryGet_BCC) {
    ParsedMessage orig{BCCM{L3BCCSetup{}}};
    auto hex = writeL3Hex(orig);
    ASSERT_TRUE(hex);
    auto res = parseL3Hex(hex.value());
    ASSERT_TRUE(res);
    EXPECT_NE(tryGet<L3BCCSetup>(*res), nullptr);
    EXPECT_EQ(messagePD(*res), L3PD::BroadcastCallControl);
}

TEST(Visitor, ParseAndTryGet_GCC) {
    ParsedMessage orig{GCCM{L3GCCSetup{}}};
    auto hex = writeL3Hex(orig);
    ASSERT_TRUE(hex);
    auto res = parseL3Hex(hex.value());
    ASSERT_TRUE(res);
    EXPECT_NE(tryGet<L3GCCSetup>(*res), nullptr);
    EXPECT_EQ(messagePD(*res), L3PD::GroupCallControl);
}

// =====================================================================
// Domain MessageName functions
// =====================================================================

TEST(Visitor, rrMessageName_CoversAll) {
    EXPECT_STREQ(rrMessageName(L3ChannelRelease::MTI), "ChannelRelease");
    EXPECT_STREQ(rrMessageName(L3ConfigurationChangeCommand::MTI), "ConfigurationChangeCommand");
    EXPECT_STREQ(rrMessageName(L3SystemInformationType14::MTI), "SystemInformationType14");
    EXPECT_STREQ(rrMessageName(L3DTMRequest::MTI), "DTMRequest");
    EXPECT_STREQ(rrMessageName(L3NotificationFACCH::MTI), "NotificationFACCH");
    EXPECT_STREQ(rrMessageName(0xFF), "Unknown_RR");
}

TEST(Visitor, ccMessageName_CoversAll) {
    EXPECT_STREQ(ccMessageName(L3Setup::MTI), "Setup");
    EXPECT_STREQ(ccMessageName(L3Hold::MTI), "Hold");
    EXPECT_STREQ(ccMessageName(L3Progress::MTI), "Progress");
    EXPECT_STREQ(ccMessageName(L3ReleaseComplete::MTI), "ReleaseComplete");
    EXPECT_STREQ(ccMessageName(0xFF), "Unknown_CC");
}

TEST(Visitor, gmmMessageName_CoversAll) {
    EXPECT_STREQ(gmmMessageName(L3AttachRequest::MTI), "AttachRequest");
    EXPECT_STREQ(gmmMessageName(L3RoutingAreaUpdateAccept::MTI), "RoutingAreaUpdateAccept");
    EXPECT_STREQ(gmmMessageName(L3GMMStatus::MTI), "GMMStatus");
    EXPECT_STREQ(gmmMessageName(0xFF), "Unknown_GMM");
}

TEST(Visitor, smMessageName_CoversAll) {
    EXPECT_STREQ(smMessageName(L3ActivatePDPContextRequest::MTI), "ActivatePDPContextRequest");
    EXPECT_STREQ(smMessageName(L3DeactivatePDPContextAccept::MTI), "DeactivatePDPContextAccept");
    EXPECT_STREQ(smMessageName(L3SMStatus::MTI), "SMStatus");
    EXPECT_STREQ(smMessageName(L3RequestPDPContextActivation::MTI), "RequestPDPContextActivation");
    EXPECT_STREQ(smMessageName(L3ActivateSecondaryPDPContextRequest::MTI), "ActivateSecondaryPDPContextRequest");
    EXPECT_STREQ(smMessageName(L3ActivateAAPDPContextRequest::MTI), "ActivateAAPDPContextRequest");
    EXPECT_STREQ(smMessageName(L3DeactivateAAPDPContextAccept::MTI), "DeactivateAAPDPContextAccept");
    EXPECT_STREQ(smMessageName(L3ActivateMBMSContextRequest::MTI), "ActivateMBMSContextRequest");
    EXPECT_STREQ(smMessageName(L3RequestMBMSContextActivation::MTI), "RequestMBMSContextActivation");
    EXPECT_STREQ(smMessageName(L3RequestSecondaryPDPContextActivation::MTI), "RequestSecondaryPDPContextActivation");
    EXPECT_STREQ(smMessageName(L3SMNotification::MTI), "SMNotification");
    EXPECT_STREQ(smMessageName(0xFF), "Unknown_SM");
}

TEST(Visitor, smsMessageName_CoversAll) {
    EXPECT_STREQ(smsMessageName(L3CPData::MTI), "CPData");
    EXPECT_STREQ(smsMessageName(L3CPAck::MTI), "CPAck");
    EXPECT_STREQ(smsMessageName(L3CPSMT::MTI), "CPSMT");
    EXPECT_STREQ(smsMessageName(0xFF), "Unknown_SMS");
}
