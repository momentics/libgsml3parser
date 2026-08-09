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
    L3Setup setup(7);
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
    ParsedMessage orig{CCM{L3Setup(7)}};
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
