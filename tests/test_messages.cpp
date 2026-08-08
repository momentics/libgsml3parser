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
#include <gsml3parser/rr/l3rrmessages.h>
#include <gsml3parser/mm/l3mmmessages.h>
#include <gsml3parser/cc/l3ccmessages.h>
#include <gsml3parser/common/l3common.h>

using namespace gsml3parser;

// ── IE Tests ─────────────────────────────────────────────────────────────

TEST(IE_Tests, CellChannelDescription) {
    L3CellChannelDescription chd(100, 0x12, 1);
    EXPECT_EQ(chd.arfcn(), 100u);
    EXPECT_EQ(chd.bsic(), 0x12u);
    EXPECT_EQ(chd.channelSpacing(), 1u);
    EXPECT_EQ(chd.lengthV(), 3u);
}

TEST(IE_Tests, ControlChannelDescription) {
    L3ControlChannelDescription chd;
    chd.mATT = 1;
    chd.mBS_AG_BLKS_RES = 2;
    chd.mCCCH_CONF = 1;
    chd.mBS_PA_MFRMS = 4;
    chd.mT3212 = 10;
    EXPECT_TRUE(chd.isCCCHCombined());
    EXPECT_EQ(chd.lengthV(), 3u);
}

TEST(IE_Tests, ChannelDescription) {
    L3ChannelDescription chd(TDMA_SDCCH, 0, 1, 100);
    EXPECT_EQ(chd.typeAndOffset(), TDMA_SDCCH);
    EXPECT_EQ(chd.tn(), 0u);
    EXPECT_EQ(chd.tsc(), 1u);
    EXPECT_EQ(chd.arfcn(), 100u);
    EXPECT_EQ(chd.lengthV(), 3u);
}

TEST(IE_Tests, PowerCommand) {
    L3PowerCommand pc(5);
    EXPECT_EQ(pc.command(), 5u);
    EXPECT_EQ(pc.lengthV(), 1u);
}

TEST(IE_Tests, CellSelection) {
    L3CellSelection cs;
    EXPECT_EQ(cs.rxLevAccessMin(), 0u);
    EXPECT_EQ(cs.maxRxLev(), 0u);
    EXPECT_EQ(cs.cellReselectionHysteresis(), 0u);
    EXPECT_EQ(cs.cellReselectionOffset(), 0u);
}

TEST(IE_Tests, RACHControlParameters) {
    L3RACHControlParameters rcp;
    EXPECT_EQ(rcp.maxRetrans(), 0u);
    EXPECT_EQ(rcp.txInteger(), 0u);
    EXPECT_EQ(rcp.re(), 0u);
    EXPECT_EQ(rcp.ac(), 0u);
    EXPECT_EQ(rcp.lengthV(), 3u);
}

TEST(IE_Tests, ImmediateAssignmentInformation) {
    L3ImmediateAssignmentInformation iai;
    EXPECT_EQ(iai.powerOffset(), 0u);
}

TEST(IE_Tests, AdditionalChannelDescription) {
    L3AdditionalChannelDescription chd(TDMA_TCHF, 5, 3, 200);
    EXPECT_EQ(chd.typeAndOffset(), TDMA_TCHF);
    EXPECT_EQ(chd.tn(), 5u);
    EXPECT_EQ(chd.tsc(), 3u);
    EXPECT_EQ(chd.arfcn(), 200u);
    EXPECT_EQ(chd.lengthV(), 3u);
}

// ── RR Message Tests ───────────────────────────────────────────────────

TEST(MessagesTest, RR_ChannelRelease) {
    L3ChannelRelease msg(RRCause::Normal_Event);
    EXPECT_EQ(msg.mti(), L3ChannelRelease::MTI);
    EXPECT_EQ(msg.pd(), L3PD::RadioResource);
    EXPECT_EQ(msg.l2BodyLength(), 1u);
}

TEST(MessagesTest, RR_ClassmarkEnquiry) {
    L3ClassmarkEnquiry msg;
    EXPECT_EQ(msg.mti(), L3ClassmarkEnquiry::MTI);
    EXPECT_EQ(msg.l2BodyLength(), 0u);
}

TEST(MessagesTest, RR_CipheringModeCommand) {
    L3CipheringModeCommand msg(true, 1);
    EXPECT_EQ(msg.mti(), L3CipheringModeCommand::MTI);
    EXPECT_EQ(msg.l2BodyLength(), 1u);
}

TEST(MessagesTest, RR_CipheringModeComplete) {
    L3CipheringModeComplete msg;
    EXPECT_EQ(msg.mti(), L3CipheringModeComplete::MTI);
    EXPECT_EQ(msg.l2BodyLength(), 0u);
}

TEST(MessagesTest, RR_PagingRequestType1) {
    L3MobileIdentity id(0x12345678);
    L3PagingRequestType1 msg = L3PagingRequestType1::builder().addMobileId(id, ChannelType::SDCCHType).build();
    EXPECT_EQ(msg.mti(), L3PagingRequestType1::MTI);
    EXPECT_GT(msg.l2BodyLength(), 0u);
}

TEST(MessagesTest, RR_PagingRequestType2) {
    L3PagingRequestType2 msg = L3PagingRequestType2::builder().addTMSI(0x12345678, ChannelType::SDCCHType).build();
    EXPECT_EQ(msg.mti(), L3PagingRequestType2::MTI);
    EXPECT_GT(msg.l2BodyLength(), 0u);
}

TEST(MessagesTest, RR_PagingRequestType3) {
    L3PagingRequestType3 msg = L3PagingRequestType3::builder().addTMSI(0x12345678, ChannelType::SDCCHType).build();
    EXPECT_EQ(msg.mti(), L3PagingRequestType3::MTI);
    EXPECT_GT(msg.l2BodyLength(), 0u);
}

TEST(MessagesTest, RR_ImmediateAssignment) {
    L3ImmediateAssignment msg;
    EXPECT_EQ(msg.mti(), L3ImmediateAssignment::MTI);
    EXPECT_GT(msg.l2BodyLength(), 0u);
}

TEST(MessagesTest, RR_ImmediateAssignmentExtended) {
    L3ImmediateAssignmentExtended msg;
    EXPECT_EQ(msg.mti(), L3ImmediateAssignmentExtended::MTI);
    EXPECT_GT(msg.l2BodyLength(), 0u);
    EXPECT_FALSE(msg.hasAdditionalChannel());
}

TEST(MessagesTest, RR_ImmediateAssignmentReject) {
    // GSM 04.08 9.1.20: ImmediateAssignmentReject body = FeatureIndicator(4 bits) + PageMode(2 bits) + WaitIndication(4 bits) + [optional RequestReferences]
    // Reference: GSM_RR_Types.ttcn ImmediateAssignmentReject (line 555): FeatureIndicator feature_ind, PageMode page_mode, ReqRefWaitInd4 payload
    // Minimum body is 1 byte: FeatureIndicator(4)|PageMode(2)|WaitIndication(4) = 10 bits -> padded to 2 bytes on wire
    L3ImmediateAssignmentReject msg(30);
    EXPECT_EQ(msg.mti(), L3ImmediateAssignmentReject::MTI);
    EXPECT_EQ(msg.waitTime(), 30u);
    EXPECT_GE(msg.l2BodyLength(), 1u);
}

TEST(MessagesTest, RR_PhysicalInformation) {
    L3PhysicalInformation msg;
    EXPECT_EQ(msg.mti(), L3PhysicalInformation::MTI);
    EXPECT_EQ(msg.l2BodyLength(), 1u);
}

TEST(MessagesTest, RR_HandoverCommand) {
    L3HandoverCommand msg;
    EXPECT_EQ(msg.mti(), L3HandoverCommand::MTI);
    EXPECT_GT(msg.l2BodyLength(), 0u);
}

TEST(MessagesTest, RR_AdditionalAssignment) {
    L3AdditionalAssignment msg;
    EXPECT_EQ(msg.mti(), L3AdditionalAssignment::MTI);
    EXPECT_GT(msg.l2BodyLength(), 0u);
}

TEST(MessagesTest, RR_SystemInformationType2) {
    L3SystemInformationType2 msg;
    EXPECT_EQ(msg.mti(), L3SystemInformationType2::MTI);
    EXPECT_EQ(msg.l2BodyLength(), 20u);
}

TEST(MessagesTest, RR_SystemInformationType2bis) {
    L3SystemInformationType2bis msg;
    EXPECT_EQ(msg.mti(), L3SystemInformationType2bis::MTI);
    // Reference: GSM_SystemInformation.ttcn SystemInformationType2bis:
    //   extd_bcch_freq_list(16) + rach_control(3) = 19 bytes (no ncc_permitted)
    EXPECT_EQ(msg.l2BodyLength(), 19u);
}

TEST(MessagesTest, RR_SystemInformationType2ter) {
    // Per GSM_SystemInformation.ttcn, SI2ter = extd_bcch_freq_list(16) + rest_octets(0..4)
    // Body is 16 bytes (no RachControlParameters, no NCCPermitted unlike SI2/SI2bis)
    L3SystemInformationType2ter msg;
    EXPECT_EQ(msg.mti(), L3SystemInformationType2ter::MTI);
    EXPECT_EQ(msg.l2BodyLength(), 16u);
}

TEST(MessagesTest, RR_SystemInformationType4) {
    L3SystemInformationType4 msg;
    EXPECT_EQ(msg.mti(), L3SystemInformationType4::MTI);
    // Reference GSM_SystemInformation.ttcn: LAI(5) + CellSelPar(2) + RachCtrl(3) = 10 bytes
    EXPECT_EQ(msg.l2BodyLength(), 10u);
}

TEST(MessagesTest, RR_SystemInformationType5) {
    L3SystemInformationType5 msg;
    EXPECT_EQ(msg.mti(), L3SystemInformationType5::MTI);
}

TEST(MessagesTest, RR_SystemInformationType5bis) {
    L3SystemInformationType5bis msg;
    EXPECT_EQ(msg.mti(), L3SystemInformationType5bis::MTI);
}

TEST(MessagesTest, RR_SystemInformationType5ter) {
    L3SystemInformationType5ter msg;
    EXPECT_EQ(msg.mti(), L3SystemInformationType5ter::MTI);
}

TEST(MessagesTest, RR_SystemInformationType6) {
    L3SystemInformationType6 msg;
    EXPECT_EQ(msg.mti(), L3SystemInformationType6::MTI);
}

TEST(MessagesTest, RR_SystemInformationType7) {
    L3SystemInformationType7 msg;
    EXPECT_EQ(msg.mti(), L3SystemInformationType7::MTI);
}

TEST(MessagesTest, RR_SystemInformationType8) {
    L3SystemInformationType8 msg;
    EXPECT_EQ(msg.mti(), L3SystemInformationType8::MTI);
}

TEST(MessagesTest, RR_SystemInformationType9) {
    L3SystemInformationType9 msg;
    EXPECT_EQ(msg.mti(), L3SystemInformationType9::MTI);
}

TEST(MessagesTest, RR_SystemInformationType16) {
    L3SystemInformationType16 msg;
    EXPECT_EQ(msg.mti(), L3SystemInformationType16::MTI);
}

TEST(MessagesTest, RR_SystemInformationType17) {
    L3SystemInformationType17 msg;
    EXPECT_EQ(msg.mti(), L3SystemInformationType17::MTI);
}

// ── CC Message Tests ───────────────────────────────────────────────────

TEST(MessagesTest, CC_Setup) {
    L3Setup msg(7);
    EXPECT_EQ(msg.mti(), L3Setup::MTI);
    EXPECT_EQ(msg.ti(), 7u);
    EXPECT_FALSE(msg.haveCalledParty());
}

TEST(MessagesTest, CC_SetupWithDigits) {
    L3CalledPartyBCDNumber called("1234567890");
    L3Setup msg = L3Setup::builder(7).calledParty(called).build();
    EXPECT_EQ(msg.mti(), L3Setup::MTI);
    EXPECT_TRUE(msg.haveCalledParty());
    EXPECT_STREQ(msg.digits(), "1234567890");
}

TEST(MessagesTest, CC_Disconnect) {
    L3Disconnect msg(7, CCCause::Normal_Call_Clearing);
    EXPECT_EQ(msg.mti(), L3Disconnect::MTI);
    EXPECT_EQ(msg.cause(), CCCause::Normal_Call_Clearing);
    EXPECT_EQ(msg.l2BodyLength(), 4u);
}

TEST(MessagesTest, CC_Release) {
    L3Release msg(7);
    EXPECT_EQ(msg.mti(), L3Release::MTI);
    EXPECT_FALSE(msg.haveCause());

    L3Release msg2 = L3Release::builder(7).cause(CCCause::User_Busy).build();
    EXPECT_TRUE(msg2.haveCause());
    EXPECT_EQ(msg2.cause(), CCCause::User_Busy);
}

TEST(MessagesTest, CC_ReleaseComplete) {
    L3ReleaseComplete msg(7);
    EXPECT_EQ(msg.mti(), L3ReleaseComplete::MTI);
}

TEST(MessagesTest, CC_Alerting) {
    L3Alerting msg(7);
    EXPECT_EQ(msg.mti(), L3Alerting::MTI);
    EXPECT_EQ(msg.l2BodyLength(), 0u);
}

TEST(MessagesTest, CC_CallProceeding) {
    L3CallProceeding msg(7);
    EXPECT_EQ(msg.mti(), L3CallProceeding::MTI);
    EXPECT_EQ(msg.l2BodyLength(), 0u);
}

TEST(MessagesTest, CC_Connect) {
    L3Connect msg(7);
    EXPECT_EQ(msg.mti(), L3Connect::MTI);
    EXPECT_EQ(msg.l2BodyLength(), 0u);
}

// ── MM Message Tests ───────────────────────────────────────────────────

TEST(MessagesTest, MM_LocationUpdatingAccept) {
    L3LocationAreaIdentity lai("250", "01", 0x0001);
    L3LocationUpdatingAccept msg = L3LocationUpdatingAccept::builder().lai(lai).build();
    EXPECT_EQ(msg.mti(), L3LocationUpdatingAccept::MTI);
}

TEST(MessagesTest, MM_LocationUpdatingReject) {
    L3LocationUpdatingReject msg(MMRejectCause::Congestion);
    EXPECT_EQ(msg.mti(), L3LocationUpdatingReject::MTI);
    EXPECT_EQ(msg.l2BodyLength(), 1u);
}

TEST(MessagesTest, MM_CMServiceAccept) {
    L3CMServiceAccept msg;
    EXPECT_EQ(msg.mti(), L3CMServiceAccept::MTI);
    EXPECT_EQ(msg.l2BodyLength(), 0u);
}

TEST(MessagesTest, MM_CMServiceAbort) {
    L3CMServiceAbort msg;
    EXPECT_EQ(msg.mti(), L3CMServiceAbort::MTI);
    EXPECT_EQ(msg.l2BodyLength(), 0u);
}

TEST(MessagesTest, MM_AuthenticationRequest) {
    std::vector<uint8_t> rand(16, 0x01);
    L3AuthenticationRequest msg(0, rand);
    EXPECT_EQ(msg.mti(), L3AuthenticationRequest::MTI);
    EXPECT_EQ(msg.l2BodyLength(), 17u);
}

TEST(MessagesTest, MM_AuthenticationReject) {
    L3AuthenticationReject msg;
    EXPECT_EQ(msg.mti(), L3AuthenticationReject::MTI);
    EXPECT_EQ(msg.l2BodyLength(), 0u);
}

TEST(MessagesTest, MM_TMSIReallocationComplete) {
    L3TMSIReallocationComplete msg;
    EXPECT_EQ(msg.mti(), L3TMSIReallocationComplete::MTI);
    EXPECT_EQ(msg.l2BodyLength(), 0u);
}

TEST(MessagesTest, MM_IdentityRequest) {
    L3IdentityRequest msg(MobileIDType::IMSI);
    EXPECT_EQ(msg.mti(), L3IdentityRequest::MTI);
    EXPECT_EQ(msg.l2BodyLength(), 1u);
}

// ── Common IE Tests ────────────────────────────────────────────────────

TEST(MessagesTest, Common_L3CellIdentity) {
    L3CellIdentity ci(0x1234);
    EXPECT_EQ(ci.id(), 0x1234u);
    EXPECT_EQ(ci.lengthV(), 2u);
}

TEST(MessagesTest, Common_L3LocationAreaIdentity) {
    L3LocationAreaIdentity lai("250", "01", 0x0001);
    EXPECT_EQ(lai.mcc(), 250);
    EXPECT_EQ(lai.mnc(), 1);
    EXPECT_EQ(lai.lac(), 1);
    EXPECT_EQ(lai.lengthV(), 5u);
}

TEST(MessagesTest, Common_L3MobileIdentityTMSI) {
    L3MobileIdentity id(0xDEADBEEF);
    EXPECT_EQ(id.type(), MobileIDType::TMSI);
    EXPECT_EQ(id.tmsi(), 0xDEADBEEFu);
    EXPECT_TRUE(id.isTMSI());
    EXPECT_FALSE(id.isIMSI());
}

TEST(MessagesTest, Common_L3MobileIdentityIMSI) {
    L3MobileIdentity id("250011234567890");
    EXPECT_EQ(id.type(), MobileIDType::IMSI);
    EXPECT_TRUE(id.isIMSI());
    EXPECT_STREQ(id.digits(), "250011234567890");
}

TEST(MessagesTest, Common_L3MobileIdentityEquality) {
    L3MobileIdentity a(0x12345678);
    L3MobileIdentity b(0x12345678);
    EXPECT_EQ(a, b);

    L3MobileIdentity c(0x87654321);
    EXPECT_NE(a, c);
}

// ── Utility Tests ──────────────────────────────────────────────────────

// mti2string and L3RRMessage::name() not yet implemented; tests removed.
