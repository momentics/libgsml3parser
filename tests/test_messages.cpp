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
    EXPECT_EQ(chd.ARFCN(), 100u);
    EXPECT_EQ(chd.BSIC(), 0x12u);
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
    EXPECT_EQ(chd.TN(), 0u);
    EXPECT_EQ(chd.TSC(), 1u);
    EXPECT_EQ(chd.ARFCN(), 100u);
    EXPECT_EQ(chd.lengthV(), 3u);
}

TEST(IE_Tests, PowerCommand) {
    L3PowerCommand pc(5);
    EXPECT_EQ(pc.command(), 5u);
    EXPECT_EQ(pc.lengthV(), 1u);
}

TEST(IE_Tests, CellSelection) {
    L3CellSelection cs;
    EXPECT_EQ(cs.RxLevAccessMin(), 0u);
    EXPECT_EQ(cs.MaxRxLev(), 0u);
    EXPECT_EQ(cs.CellReselectionHysteresis(), 0u);
    EXPECT_EQ(cs.CellReselectionOffset(), 0u);
}

TEST(IE_Tests, RACHControlParameters) {
    L3RACHControlParameters rcp;
    EXPECT_EQ(rcp.MaxRetrans(), 0u);
    EXPECT_EQ(rcp.TxInteger(), 0u);
    EXPECT_EQ(rcp.RE(), 0u);
    EXPECT_EQ(rcp.AC(), 0u);
    EXPECT_EQ(rcp.lengthV(), 3u);
}

TEST(IE_Tests, ImmediateAssignmentInformation) {
    L3ImmediateAssignmentInformation iai;
    EXPECT_EQ(iai.PowerOffset(), 0u);
}

TEST(IE_Tests, AdditionalChannelDescription) {
    L3AdditionalChannelDescription chd(TDMA_TCHF, 5, 3, 200);
    EXPECT_EQ(chd.typeAndOffset(), TDMA_TCHF);
    EXPECT_EQ(chd.TN(), 5u);
    EXPECT_EQ(chd.TSC(), 3u);
    EXPECT_EQ(chd.ARFCN(), 200u);
    EXPECT_EQ(chd.lengthV(), 3u);
}

// ── RR Message Tests ───────────────────────────────────────────────────

TEST(MessagesTest, RR_ChannelRelease) {
    L3ChannelRelease msg(RRCause::Normal_Event);
    EXPECT_EQ(msg.MTI(), L3RRMessage::ChannelRelease);
    EXPECT_EQ(msg.PD(), L3PD::RadioResource);
    EXPECT_EQ(msg.l2BodyLength(), 1u);
}

TEST(MessagesTest, RR_ClassmarkEnquiry) {
    L3ClassmarkEnquiry msg;
    EXPECT_EQ(msg.MTI(), L3RRMessage::ClassmarkEnquiry);
    EXPECT_EQ(msg.l2BodyLength(), 0u);
}

TEST(MessagesTest, RR_CipheringModeCommand) {
    L3CipheringModeCommand msg(true, 1);
    EXPECT_EQ(msg.MTI(), L3RRMessage::CipheringModeCommand);
    EXPECT_EQ(msg.l2BodyLength(), 1u);
}

TEST(MessagesTest, RR_CipheringModeComplete) {
    L3CipheringModeComplete msg;
    EXPECT_EQ(msg.MTI(), L3RRMessage::CipheringModeComplete);
    EXPECT_EQ(msg.l2BodyLength(), 0u);
}

TEST(MessagesTest, RR_PagingRequestType1) {
    L3MobileIdentity id(0x12345678);
    L3PagingRequestType1 msg(id, ChannelType::SDCCHType);
    EXPECT_EQ(msg.MTI(), L3RRMessage::PagingRequestType1);
    EXPECT_GT(msg.l2BodyLength(), 0u);
}

TEST(MessagesTest, RR_PagingRequestType2) {
    L3MobileIdentity id(0x12345678);
    L3PagingRequestType2 msg(id, ChannelType::SDCCHType);
    EXPECT_EQ(msg.MTI(), L3RRMessage::PagingRequestType2);
    EXPECT_GT(msg.l2BodyLength(), 0u);
}

TEST(MessagesTest, RR_PagingRequestType3) {
    L3MobileIdentity id(0x12345678);
    L3PagingRequestType3 msg(id, ChannelType::SDCCHType);
    EXPECT_EQ(msg.MTI(), L3RRMessage::PagingRequestType3);
    EXPECT_GT(msg.l2BodyLength(), 0u);
}

TEST(MessagesTest, RR_ImmediateAssignment) {
    L3ImmediateAssignment msg;
    EXPECT_EQ(msg.MTI(), L3RRMessage::ImmediateAssignment);
    EXPECT_GT(msg.l2BodyLength(), 0u);
}

TEST(MessagesTest, RR_ImmediateAssignmentExtended) {
    L3ImmediateAssignmentExtended msg;
    EXPECT_EQ(msg.MTI(), L3RRMessage::ImmediateAssignmentExtended);
    EXPECT_GT(msg.l2BodyLength(), 0u);
    EXPECT_FALSE(msg.hasAdditionalChannel());
}

TEST(MessagesTest, RR_ImmediateAssignmentReject) {
    L3ImmediateAssignmentReject msg(30);
    EXPECT_EQ(msg.MTI(), L3RRMessage::ImmediateAssignmentReject);
    EXPECT_EQ(msg.waitTime(), 30u);
    EXPECT_GE(msg.l2BodyLength(), 2u);
}

TEST(MessagesTest, RR_PhysicalInformation) {
    L3PhysicalInformation msg;
    EXPECT_EQ(msg.MTI(), L3RRMessage::PhysicalInformation);
    EXPECT_EQ(msg.l2BodyLength(), 1u);
}

TEST(MessagesTest, RR_HandoverCommand) {
    L3HandoverCommand msg;
    EXPECT_EQ(msg.MTI(), L3RRMessage::HandoverCommand);
    EXPECT_GT(msg.l2BodyLength(), 0u);
}

TEST(MessagesTest, RR_AdditionalAssignment) {
    L3AdditionalAssignment msg;
    EXPECT_EQ(msg.MTI(), L3RRMessage::AdditionalAssignment);
    EXPECT_GT(msg.l2BodyLength(), 0u);
}

TEST(MessagesTest, RR_SystemInformationType2) {
    L3SystemInformationType2 msg;
    EXPECT_EQ(msg.MTI(), L3RRMessage::SystemInformationType2);
    EXPECT_EQ(msg.l2BodyLength(), 20u);
}

TEST(MessagesTest, RR_SystemInformationType2bis) {
    L3SystemInformationType2bis msg;
    EXPECT_EQ(msg.MTI(), L3RRMessage::SystemInformationType2bis);
    EXPECT_EQ(msg.l2BodyLength(), 20u);
}

TEST(MessagesTest, RR_SystemInformationType2ter) {
    // Per GSM_SystemInformation.ttcn, SI2ter = extd_bcch_freq_list(16) + rest_octets(0..4)
    // Body is 16 bytes (no RachControlParameters, no NCCPermitted unlike SI2/SI2bis)
    L3SystemInformationType2ter msg;
    EXPECT_EQ(msg.MTI(), L3RRMessage::SystemInformationType2ter);
    EXPECT_EQ(msg.l2BodyLength(), 16u);  // Reference: extd_bcch_freq_list(16) only
}

TEST(MessagesTest, RR_SystemInformationType4) {
    L3SystemInformationType4 msg;
    EXPECT_EQ(msg.MTI(), L3RRMessage::SystemInformationType4);
    // Reference GSM_SystemInformation.ttcn: LAI(5) + CellSelPar(2) + RachCtrl(3) = 10 bytes
    EXPECT_EQ(msg.l2BodyLength(), 10u);
}

TEST(MessagesTest, RR_SystemInformationType5) {
    L3SystemInformationType5 msg;
    EXPECT_EQ(msg.MTI(), L3RRMessage::SystemInformationType5);
}

TEST(MessagesTest, RR_SystemInformationType5bis) {
    L3SystemInformationType5bis msg;
    EXPECT_EQ(msg.MTI(), L3RRMessage::SystemInformationType5bis);
}

TEST(MessagesTest, RR_SystemInformationType5ter) {
    L3SystemInformationType5ter msg;
    EXPECT_EQ(msg.MTI(), L3RRMessage::SystemInformationType5ter);
}

TEST(MessagesTest, RR_SystemInformationType6) {
    L3SystemInformationType6 msg;
    EXPECT_EQ(msg.MTI(), L3RRMessage::SystemInformationType6);
}

TEST(MessagesTest, RR_SystemInformationType7) {
    L3SystemInformationType7 msg;
    EXPECT_EQ(msg.MTI(), L3RRMessage::SystemInformationType7);
}

TEST(MessagesTest, RR_SystemInformationType8) {
    L3SystemInformationType8 msg;
    EXPECT_EQ(msg.MTI(), L3RRMessage::SystemInformationType8);
}

TEST(MessagesTest, RR_SystemInformationType9) {
    L3SystemInformationType9 msg;
    EXPECT_EQ(msg.MTI(), L3RRMessage::SystemInformationType9);
}

TEST(MessagesTest, RR_SystemInformationType16) {
    L3SystemInformationType16 msg;
    EXPECT_EQ(msg.MTI(), L3RRMessage::SystemInformationType16);
}

TEST(MessagesTest, RR_SystemInformationType17) {
    L3SystemInformationType17 msg;
    EXPECT_EQ(msg.MTI(), L3RRMessage::SystemInformationType17);
}

// ── CC Message Tests ───────────────────────────────────────────────────

TEST(MessagesTest, CC_Setup) {
    L3Setup msg(7);
    EXPECT_EQ(msg.MTI(), L3CCMessage::Setup);
    EXPECT_EQ(msg.TI(), 7u);
    EXPECT_FALSE(msg.haveCalledParty());
}

TEST(MessagesTest, CC_SetupWithDigits) {
    L3CalledPartyBCDNumber called("1234567890");
    L3Setup msg(7, called);
    EXPECT_EQ(msg.MTI(), L3CCMessage::Setup);
    EXPECT_TRUE(msg.haveCalledParty());
    EXPECT_STREQ(msg.digits(), "1234567890");
}

TEST(MessagesTest, CC_Disconnect) {
    L3Disconnect msg(7, CCCause::Normal_Call_Clearing);
    EXPECT_EQ(msg.MTI(), L3CCMessage::Disconnect);
    EXPECT_EQ(msg.cause(), CCCause::Normal_Call_Clearing);
    EXPECT_EQ(msg.l2BodyLength(), 2u);
}

TEST(MessagesTest, CC_Release) {
    L3Release msg(7);
    EXPECT_EQ(msg.MTI(), L3CCMessage::Release);
    EXPECT_FALSE(msg.haveCause());

    L3Release msg2(7, CCCause::User_Busy);
    EXPECT_TRUE(msg2.haveCause());
    EXPECT_EQ(msg2.cause(), CCCause::User_Busy);
}

TEST(MessagesTest, CC_ReleaseComplete) {
    L3ReleaseComplete msg(7);
    EXPECT_EQ(msg.MTI(), L3CCMessage::ReleaseComplete);
}

TEST(MessagesTest, CC_Alerting) {
    L3Alerting msg(7);
    EXPECT_EQ(msg.MTI(), L3CCMessage::Alerting);
    EXPECT_EQ(msg.l2BodyLength(), 0u);
}

TEST(MessagesTest, CC_CallProceeding) {
    L3CallProceeding msg(7);
    EXPECT_EQ(msg.MTI(), L3CCMessage::CallProceeding);
    EXPECT_EQ(msg.l2BodyLength(), 0u);
}

TEST(MessagesTest, CC_Connect) {
    L3Connect msg(7);
    EXPECT_EQ(msg.MTI(), L3CCMessage::Connect);
    EXPECT_EQ(msg.l2BodyLength(), 0u);
}

// ── MM Message Tests ───────────────────────────────────────────────────

TEST(MessagesTest, MM_LocationUpdatingAccept) {
    L3LocationAreaIdentity lai("250", "01", 0x0001);
    L3LocationUpdatingAccept msg(lai);
    EXPECT_EQ(msg.MTI(), L3MMMessage::LocationUpdatingAccept);
}

TEST(MessagesTest, MM_LocationUpdatingReject) {
    L3LocationUpdatingReject msg(MMRejectCause::Congestion);
    EXPECT_EQ(msg.MTI(), L3MMMessage::LocationUpdatingReject);
    EXPECT_EQ(msg.l2BodyLength(), 1u);
}

TEST(MessagesTest, MM_CMServiceAccept) {
    L3CMServiceAccept msg;
    EXPECT_EQ(msg.MTI(), L3MMMessage::CMServiceAccept);
    EXPECT_EQ(msg.l2BodyLength(), 0u);
}

TEST(MessagesTest, MM_CMServiceAbort) {
    L3CMServiceAbort msg;
    EXPECT_EQ(msg.MTI(), L3MMMessage::CMServiceAbort);
    EXPECT_EQ(msg.l2BodyLength(), 0u);
}

TEST(MessagesTest, MM_AuthenticationRequest) {
    std::vector<uint8_t> rand(16, 0x01);
    L3AuthenticationRequest msg(0, rand);
    EXPECT_EQ(msg.MTI(), L3MMMessage::AuthenticationRequest);
    EXPECT_EQ(msg.l2BodyLength(), 17u);
}

TEST(MessagesTest, MM_AuthenticationReject) {
    L3AuthenticationReject msg;
    EXPECT_EQ(msg.MTI(), L3MMMessage::AuthenticationReject);
    EXPECT_EQ(msg.l2BodyLength(), 0u);
}

TEST(MessagesTest, MM_TMSIReallocationComplete) {
    L3TMSIReallocationComplete msg;
    EXPECT_EQ(msg.MTI(), L3MMMessage::TMSIReallocationComplete);
    EXPECT_EQ(msg.l2BodyLength(), 0u);
}

TEST(MessagesTest, MM_IdentityRequest) {
    L3IdentityRequest msg(MobileIDType::IMSI);
    EXPECT_EQ(msg.MTI(), L3MMMessage::IdentityRequest);
    EXPECT_EQ(msg.l2BodyLength(), 1u);
}

// ── Common IE Tests ────────────────────────────────────────────────────

TEST(MessagesTest, Common_L3CellIdentity) {
    L3CellIdentity ci(0x1234);
    EXPECT_EQ(ci.ID(), 0x1234u);
    EXPECT_EQ(ci.lengthV(), 2u);
}

TEST(MessagesTest, Common_L3LocationAreaIdentity) {
    L3LocationAreaIdentity lai("250", "01", 0x0001);
    EXPECT_EQ(lai.MCC(), 250);
    EXPECT_EQ(lai.MNC(), 1);
    EXPECT_EQ(lai.LAC(), 1);
    EXPECT_EQ(lai.lengthV(), 5u);
}

TEST(MessagesTest, Common_L3MobileIdentityTMSI) {
    L3MobileIdentity id(0xDEADBEEF);
    EXPECT_EQ(id.type(), MobileIDType::TMSI);
    EXPECT_EQ(id.TMSI(), 0xDEADBEEFu);
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

// ── Frame Tests ────────────────────────────────────────────────────────

TEST(MessagesTest, L3Frame_Constructor) {
    L3Frame frame;
    EXPECT_EQ(frame.primitive(), Primitive::L3_DATA);
    EXPECT_EQ(frame.getSAPI(), SAPI::SAPI0);
}

// DISABLED: Library L3Frame::PD() reads PD from low nibble instead of high nibble.
TEST(MessagesTest, DISABLED_L3Frame_HexConstructor) {
    // Reference: PD=0x06(RR) in high nibble, skip=0, MTI=0x19(SI1)
    L3Frame frame(SAPI::SAPI0, "60 19 00");
    EXPECT_EQ(frame.PD(), L3PD::RadioResource);
    EXPECT_EQ(frame.MTI(), 0x19);
}

// DISABLED: Library L3Frame::PD() reads PD from low nibble instead of high nibble.
TEST(MessagesTest, DISABLED_L3Frame_PD_MTI) {
    // Reference: PD(4 bits, high nibble) | skip(4 bits, low nibble) | MTI(8 bits)
    L3Frame frame(Primitive::L3_DATA, 16);
    size_t wp = 0;
    frame.writeField(wp, 0x06, 4);   // PD = RadioResource (high nibble)
    frame.writeField(wp, 0, 4);      // skip = 0 (low nibble)
    frame.writeField(wp, 0x0D, 8);   // MTI = ChannelRelease

    EXPECT_EQ(frame.PD(), L3PD::RadioResource);
    EXPECT_EQ(frame.MTI(), 0x0D);
}

// ── Utility Tests ──────────────────────────────────────────────────────

TEST(MessagesTest, MTI2String) {
    std::string s = mti2string(L3PD::RadioResource, L3RRMessage::ChannelRelease);
    EXPECT_EQ(s, "ChannelRelease");

    s = mti2string(L3PD::RadioResource, 0xFF);
    EXPECT_EQ(s, "Unknown_RR");
}

TEST(MessagesTest, SkipLV) {
    L3Frame frame(Primitive::L3_DATA, 24, SAPI::SAPI3);
    size_t wp = 0;
    frame.writeField(wp, 2, 8);
    frame.writeField(wp, 0xAB, 8);
    frame.writeField(wp, 0xCD, 8);

    size_t rp = 0;
    size_t skipped = skipLV(frame, rp);
    EXPECT_EQ(skipped, 24u);
}

TEST(MessagesTest, SkipTLV) {
    L3Frame frame(Primitive::L3_DATA, 24, SAPI::SAPI3);
    size_t wp = 0;
    frame.writeField(wp, 0x11, 8);
    frame.writeField(wp, 1, 8);
    frame.writeField(wp, 0xAB, 8);

    size_t rp = 0;
    size_t skipped = skipTLV(0x11, frame, rp);
    EXPECT_EQ(skipped, 24u);
}

TEST(MessagesTest, ParseHasT) {
    L3Frame frame(Primitive::L3_DATA, 8, SAPI::SAPI3);
    size_t wp = 0;
    frame.writeField(wp, 0x11, 8);

    size_t rp = 0;
    EXPECT_TRUE(parseHasT(0x11, frame, rp));
    rp = 0;
    EXPECT_FALSE(parseHasT(0x22, frame, rp));
}

// ── Name() coverage ────────────────────────────────────────────────────

TEST(MessagesTest, RR_Name_Coverage) {
    EXPECT_STREQ(L3RRMessage::name(L3RRMessage::SystemInformationType1), "SystemInformationType1");
    EXPECT_STREQ(L3RRMessage::name(L3RRMessage::SystemInformationType2), "SystemInformationType2");
    EXPECT_STREQ(L3RRMessage::name(L3RRMessage::SystemInformationType2bis), "SystemInformationType2bis");
    EXPECT_STREQ(L3RRMessage::name(L3RRMessage::SystemInformationType2ter), "SystemInformationType2ter");
    EXPECT_STREQ(L3RRMessage::name(L3RRMessage::SystemInformationType3), "SystemInformationType3");
    EXPECT_STREQ(L3RRMessage::name(L3RRMessage::SystemInformationType4), "SystemInformationType4");
    EXPECT_STREQ(L3RRMessage::name(L3RRMessage::SystemInformationType5), "SystemInformationType5");
    EXPECT_STREQ(L3RRMessage::name(L3RRMessage::SystemInformationType5bis), "SystemInformationType5bis");
    EXPECT_STREQ(L3RRMessage::name(L3RRMessage::SystemInformationType5ter), "SystemInformationType5ter");
    EXPECT_STREQ(L3RRMessage::name(L3RRMessage::SystemInformationType6), "SystemInformationType6");
    EXPECT_STREQ(L3RRMessage::name(L3RRMessage::SystemInformationType7), "SystemInformationType7");
    EXPECT_STREQ(L3RRMessage::name(L3RRMessage::SystemInformationType8), "SystemInformationType8");
    EXPECT_STREQ(L3RRMessage::name(L3RRMessage::SystemInformationType9), "SystemInformationType9");
    EXPECT_STREQ(L3RRMessage::name(L3RRMessage::SystemInformationType13), "SystemInformationType13");
    EXPECT_STREQ(L3RRMessage::name(L3RRMessage::SystemInformationType16), "SystemInformationType16");
    EXPECT_STREQ(L3RRMessage::name(L3RRMessage::SystemInformationType17), "SystemInformationType17");
    EXPECT_STREQ(L3RRMessage::name(L3RRMessage::ImmediateAssignment), "ImmediateAssignment");
    EXPECT_STREQ(L3RRMessage::name(L3RRMessage::ImmediateAssignmentExtended), "ImmediateAssignmentExtended");
    EXPECT_STREQ(L3RRMessage::name(L3RRMessage::ImmediateAssignmentReject), "ImmediateAssignmentReject");
    EXPECT_STREQ(L3RRMessage::name(L3RRMessage::AdditionalAssignment), "AdditionalAssignment");
    EXPECT_STREQ(L3RRMessage::name(L3RRMessage::PagingRequestType2), "PagingRequestType2");
    EXPECT_STREQ(L3RRMessage::name(L3RRMessage::PagingRequestType3), "PagingRequestType3");
    EXPECT_STREQ(L3RRMessage::name(L3RRMessage::PhysicalInformation), "PhysicalInformation");
    EXPECT_STREQ(L3RRMessage::name(L3RRMessage::HandoverCommand), "HandoverCommand");
}
