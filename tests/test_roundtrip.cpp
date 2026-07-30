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

// Round-trip tests: construct message → serialize → parse → verify fields.
// Derived from osmo-ttcn3-hacks reference: L3_Templates.ttcn, GSM_RR_Types.ttcn,
// GSM_SystemInformation.ttcn, GSM_RestOctets.ttcn.

#include <gtest/gtest.h>
#include <gsml3parser/parser.h>
#include <gsml3parser/rr/l3rrmessages.h>
#include <gsml3parser/common/l3common.h>

using namespace gsml3parser;

// Helper: serialize msg → parse → return result.
static std::unique_ptr<L3Message> roundtrip(const L3Message& msg) {
    std::vector<uint8_t> buf(msg.fullLength());
    size_t n = writeL3(msg, buf.data(), buf.size());
    if (n == 0) return nullptr;
    auto result = parseL3(buf.data(), n);
    return result;
}

// Helper: verify PD + MTI survived round-trip.
static void checkHeader(std::unique_ptr<L3Message>& parsed, L3PD expectPD, int expectMTI) {
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->PD(), expectPD);
    EXPECT_EQ(parsed->MTI(), expectMTI);
}

// ── Paging Request Type 1 (GSM 04.08 9.1.22) ───────────────────────────
// Reference: L3_Templates.ttcn ts_PAG_RESP builds on ts_MI_TMSI_LV / ts_MI_IMSI_LV
// Paging Request Type 1 structure:
//   PD(4)=0x06, MTI(8)=0x21, ChanNeeded(4), PageMode(4), MI1 LV..., [MI2 TLV...]

TEST(RoundTripTest, PagingRequestType1_TMSI) {
    L3MobileIdentity id(0x12345678);
    L3PagingRequestType1 msg(id, ChannelType::SDCCHType);
    auto parsed = roundtrip(msg);
    checkHeader(parsed, L3PD::RadioResource, L3RRMessage::PagingRequestType1);
}

TEST(RoundTripTest, PagingRequestType1_IMSI) {
    L3MobileIdentity id("250011234567890");
    L3PagingRequestType1 msg(id, ChannelType::TCHFType);
    auto parsed = roundtrip(msg);
    checkHeader(parsed, L3PD::RadioResource, L3RRMessage::PagingRequestType1);
}

// ── Paging Request Type 2 (GSM 04.08 9.1.23) ───────────────────────────

TEST(RoundTripTest, PagingRequestType2) {
    L3MobileIdentity id(0xDEADBEEF);
    L3PagingRequestType2 msg(id, ChannelType::SDCCHType);
    auto parsed = roundtrip(msg);
    checkHeader(parsed, L3PD::RadioResource, L3RRMessage::PagingRequestType2);
}

// ── Paging Request Type 3 (GSM 04.08 9.1.24) ───────────────────────────

TEST(RoundTripTest, PagingRequestType3) {
    L3MobileIdentity id(0xABCDEF01);
    L3PagingRequestType3 msg(id, ChannelType::TCHHType);
    auto parsed = roundtrip(msg);
    checkHeader(parsed, L3PD::RadioResource, L3RRMessage::PagingRequestType3);
}

// ── Paging Response (GSM 04.08 9.1.25) ─────────────────────────────────
// Reference: L3_Templates.ttcn ts_PAG_RESP
// Structure: spare_half(4), CKSN(4), CM2 LV, MI LV, [addl_upd_par TV]

TEST(RoundTripTest, PagingResponse) {
    L3MobileIdentity id(0x12345678);
    L3PagingRequestType1 orig(id, ChannelType::SDCCHType);
    auto parsedReq = roundtrip(orig);
    ASSERT_TRUE(parsedReq);
}

// ── System Information Type 1 (GSM 04.08 9.1.31) ──────────────────────
// Reference: GSM_SystemInformation.ttcn SystemInformationType1
// Structure: CellChannelDescription(16 bits) + RachControlParameters(24 bits) + RestOctets(0..8 bits)
// Total fixed: 19 bytes + rest octets padded to 23

TEST(RoundTripTest, SystemInformationType1) {
    L3SystemInformationType1 msg;
    auto parsed = roundtrip(msg);
    checkHeader(parsed, L3PD::RadioResource, L3RRMessage::SystemInformationType1);
}

// ── System Information Type 2 (GSM 04.08 9.1.32) ──────────────────────
// Reference: GSM_SystemInformation.ttcn SystemInformationType2
// Structure: BCCH Freq List(128 bits) + NCC Permitted(8 bits) + RACH Ctrl Params(24 bits)
// Total: 20 bytes + rest octets padded to 23

TEST(RoundTripTest, SystemInformationType2) {
    L3SystemInformationType2 msg;
    auto parsed = roundtrip(msg);
    checkHeader(parsed, L3PD::RadioResource, L3RRMessage::SystemInformationType2);
}

// ── System Information Type 2bis (GSM 04.08 9.1.33) ──────────────────

TEST(RoundTripTest, SystemInformationType2bis) {
    L3SystemInformationType2bis msg;
    auto parsed = roundtrip(msg);
    checkHeader(parsed, L3PD::RadioResource, L3RRMessage::SystemInformationType2bis);
}

// ── System Information Type 2ter (GSM 04.08 9.1.34) ──────────────────

TEST(RoundTripTest, SystemInformationType2ter) {
    L3SystemInformationType2ter msg;
    auto parsed = roundtrip(msg);
    checkHeader(parsed, L3PD::RadioResource, L3RRMessage::SystemInformationType2ter);
}

// ── System Information Type 3 (GSM 04.08 9.1.35) ──────────────────────
// Reference: BTS_Tests.ttcn ts_SI3_default
// Structure: CI(2) + LAI(5) + CtrlChanDesc(3) + CellOptions(1) + CellSelPar(2) + RACHCtrl(3) = 16 bytes
// Rest octets per GSM_RestOctets.ttcn SI3RestOctets

TEST(RoundTripTest, SystemInformationType3) {
    L3SystemInformationType3 msg;
    auto parsed = roundtrip(msg);
    checkHeader(parsed, L3PD::RadioResource, L3RRMessage::SystemInformationType3);
}

// ── System Information Type 4 (GSM 04.08 9.1.36) ─────────────────────
// Reference: BTS_Tests.ttcn ts_SI4_default
// Structure: LAI(5) + CI(2) + CellSelPar(2) + CellOptions(1) + RACHCtrl(3) = 13 bytes

TEST(RoundTripTest, SystemInformationType4) {
    L3SystemInformationType4 msg;
    auto parsed = roundtrip(msg);
    checkHeader(parsed, L3PD::RadioResource, L3RRMessage::SystemInformationType4);
}

// ── System Information Type 5 (GSM 04.08 9.1.37) ─────────────────────
// Reference: GSM_SystemInformation.ttcn SystemInformationType5
// Structure: BCCH Freq List(16 bytes)

TEST(RoundTripTest, SystemInformationType5) {
    L3SystemInformationType5 msg;
    auto parsed = roundtrip(msg);
    checkHeader(parsed, L3PD::RadioResource, L3RRMessage::SystemInformationType5);
}

// ── System Information Type 5bis (GSM 04.08 9.1.38) ──────────────────

TEST(RoundTripTest, SystemInformationType5bis) {
    L3SystemInformationType5bis msg;
    auto parsed = roundtrip(msg);
    checkHeader(parsed, L3PD::RadioResource, L3RRMessage::SystemInformationType5bis);
}

// ── System Information Type 5ter (GSM 04.08 9.1.39) ──────────────────

TEST(RoundTripTest, SystemInformationType5ter) {
    L3SystemInformationType5ter msg;
    auto parsed = roundtrip(msg);
    checkHeader(parsed, L3PD::RadioResource, L3RRMessage::SystemInformationType5ter);
}

// ── System Information Type 6 (GSM 04.08 9.1.40) ─────────────────────
// Reference: GSM_SystemInformation.ttcn SystemInformationType6
// Structure: CI(2) + LAI(5) + CellOptionsSACCH(1) + NCCPermitted(1) = 9 bytes

TEST(RoundTripTest, SystemInformationType6) {
    L3SystemInformationType6 msg;
    auto parsed = roundtrip(msg);
    checkHeader(parsed, L3PD::RadioResource, L3RRMessage::SystemInformationType6);
}

// ── System Information Type 7 (GSM 04.08 9.1.41) ─────────────────────
// Reference: GSM_SystemInformation.ttcn SystemInformationType7
// TV-format: RACH Control Parameters(TV,0x28) + Cell Channel Description(0..10, TV,0x21)

TEST(RoundTripTest, SystemInformationType7) {
    L3SystemInformationType7 msg;
    auto parsed = roundtrip(msg);
    checkHeader(parsed, L3PD::RadioResource, L3RRMessage::SystemInformationType7);
}

// ── System Information Type 8 (GSM 04.08 9.1.42) ─────────────────────
// Reference: GSM_SystemInformation.ttcn SystemInformationType8
// TV-format: NCC Permitted(TV,0x27) + RACH Control Parameters(TV,0x28) + Cell Channel Desc(0..10, TV,0x21)

TEST(RoundTripTest, SystemInformationType8) {
    L3SystemInformationType8 msg;
    auto parsed = roundtrip(msg);
    checkHeader(parsed, L3PD::RadioResource, L3RRMessage::SystemInformationType8);
}

// ── System Information Type 9 (GSM 04.08 9.1.43) ─────────────────────
// Reference: GSM_SystemInformation.ttcn SystemInformationType9
// V-format: CI(2) + CellSelPar(2) + CellOptions(1) = 5 bytes

TEST(RoundTripTest, SystemInformationType9) {
    L3SystemInformationType9 msg;
    auto parsed = roundtrip(msg);
    checkHeader(parsed, L3PD::RadioResource, L3RRMessage::SystemInformationType9);
}

// ── System Information Type 13 (GSM 04.08 9.1.43a) ───────────────────
// Reference: GSM_SystemInformation.ttcn SystemInformationType13
// Rest octets per GSM_RestOctets.ttcn SI13RestOctets

TEST(RoundTripTest, SystemInformationType13) {
    L3SystemInformationType13 msg;
    auto parsed = roundtrip(msg);
    checkHeader(parsed, L3PD::RadioResource, L3RRMessage::SystemInformationType13);
}

// ── System Information Type 16 (GSM 04.08 9.1.43b) ───────────────────

TEST(RoundTripTest, SystemInformationType16) {
    L3SystemInformationType16 msg;
    auto parsed = roundtrip(msg);
    checkHeader(parsed, L3PD::RadioResource, L3RRMessage::SystemInformationType16);
}

// ── System Information Type 17 (GSM 04.08 9.1.43c) ───────────────────

TEST(RoundTripTest, SystemInformationType17) {
    L3SystemInformationType17 msg;
    auto parsed = roundtrip(msg);
    checkHeader(parsed, L3PD::RadioResource, L3RRMessage::SystemInformationType17);
}

// ── Channel Release (GSM 04.08 9.1.7) ────────────────────────────────
// Reference: L3_Templates.ttcn tr_RRM_RR_RELEASE

TEST(RoundTripTest, ChannelRelease_Normal) {
    L3ChannelRelease msg(RRCause::Normal_Event);
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    auto* cr = dynamic_cast<L3ChannelRelease*>(parsed.get());
    ASSERT_TRUE(cr);
    EXPECT_EQ(cr->cause(), RRCause::Normal_Event);
}

TEST(RoundTripTest, ChannelRelease_Preemptive) {
    L3ChannelRelease msg(RRCause::Preemptive_Release);
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    auto* cr = dynamic_cast<L3ChannelRelease*>(parsed.get());
    ASSERT_TRUE(cr);
    EXPECT_EQ(cr->cause(), RRCause::Preemptive_Release);
}

// ── RR Status (GSM 04.08 9.1.29) ─────────────────────────────────────
// Reference: L3_Templates.ttcn tr_RRM_RR_STATUS

TEST(RoundTripTest, RRStatus) {
    // Build RR Status from hex: PD=0x06, MTI=0x12, cause=0x60 (Invalid Mandatory Info)
    uint8_t data[] = {0x06, 0x12, 0x60};
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->PD(), L3PD::RadioResource);
    EXPECT_EQ(msg->MTI(), L3RRMessage::RRStatus);
    auto* rs = dynamic_cast<L3RRStatus*>(msg.get());
    ASSERT_TRUE(rs);
    EXPECT_EQ(rs->cause(), RRCause::Invalid_Mandatory_Information);
}

// ── Assignment Command (GSM 04.08 9.1.2) ──────────────────────────────
// Reference: L3_Templates.ttcn tr_RR_AssignmentCommand
// GSM_RR_Types.ttcn AssignmentCommand: ChanDesc(24 bits) + PowerCmd(8 bits) + [optional IEs]

TEST(RoundTripTest, AssignmentCommand) {
    L3AssignmentCommand msg;
    auto parsed = roundtrip(msg);
    checkHeader(parsed, L3PD::RadioResource, L3RRMessage::AssignmentCommand);
}

// ── Assignment Complete (GSM 04.08 9.1.3) ─────────────────────────────

TEST(RoundTripTest, AssignmentComplete) {
    uint8_t data[] = {0x06, 0x29, 0x00}; // PD=RR, MTI=AssignmentComplete, cause=Normal
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    auto* ac = dynamic_cast<L3AssignmentComplete*>(msg.get());
    ASSERT_TRUE(ac);
    EXPECT_EQ(ac->cause(), RRCause::Normal_Event);

    auto parsed = roundtrip(*ac);
    checkHeader(parsed, L3PD::RadioResource, L3RRMessage::AssignmentComplete);
}

// ── Assignment Failure (GSM 04.08 9.1.3) ──────────────────────────────

TEST(RoundTripTest, AssignmentFailure) {
    uint8_t data[] = {0x06, 0x2f, 0x09}; // cause=Channel Mode Unacceptable
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    auto* af = dynamic_cast<L3AssignmentFailure*>(msg.get());
    ASSERT_TRUE(af);
    EXPECT_EQ(af->cause(), RRCause::Channel_Mode_Unacceptable);

    auto parsed = roundtrip(*af);
    checkHeader(parsed, L3PD::RadioResource, L3RRMessage::AssignmentFailure);
}

// ── Classmark Enquiry (GSM 04.08 9.1.14) ─────────────────────────────
// Reference: L3_Templates.ttcn tr_RRM_CM_ENQUIRY

TEST(RoundTripTest, ClassmarkEnquiry) {
    L3ClassmarkEnquiry msg;
    auto parsed = roundtrip(msg);
    checkHeader(parsed, L3PD::RadioResource, L3RRMessage::ClassmarkEnquiry);
}

// ── Measurement Report (GSM 04.08 9.1.21) ────────────────────────────
// Reference: L3_Templates.ttcn ts_MEAS_REP, ts_MeasurementResults
// GSM_RR_Types.ttcn MeasurementResults: 16 bytes fixed

TEST(RoundTripTest, MeasurementReport) {
    L3MeasurementReport msg;
    auto parsed = roundtrip(msg);
    checkHeader(parsed, L3PD::RadioResource, L3RRMessage::MeasurementReport);
}

// ── Ciphering Mode Command (GSM 04.08 9.1.9) ─────────────────────────
// Reference: L3_Templates.ttcn ts_RRM_CiphModeCmd

TEST(RoundTripTest, CipheringModeCommand_A5_0) {
    L3CipheringModeCommand msg(false, 0);
    auto parsed = roundtrip(msg);
    checkHeader(parsed, L3PD::RadioResource, L3RRMessage::CipheringModeCommand);
}

TEST(RoundTripTest, CipheringModeCommand_A5_3) {
    L3CipheringModeCommand msg(true, 3);
    auto parsed = roundtrip(msg);
    checkHeader(parsed, L3PD::RadioResource, L3RRMessage::CipheringModeCommand);
}

// ── Ciphering Mode Complete (GSM 04.08 9.1.10) ───────────────────────

TEST(RoundTripTest, CipheringModeComplete) {
    L3CipheringModeComplete msg;
    auto parsed = roundtrip(msg);
    checkHeader(parsed, L3PD::RadioResource, L3RRMessage::CipheringModeComplete);
}

// ── Handover Command (GSM 04.08 9.1.15) ──────────────────────────────
// Reference: GSM_RR_Types.ttcn HandoverCommand
// L3_Templates.ttcn ts_RR_HandoverCommand
// Structure: CellDesc(16) + ChanDesc(24) + HORef(8) + PowerCmdAccType(8) + SyncInd(8) = 70 bits

TEST(RoundTripTest, HandoverCommand) {
    L3HandoverCommand msg;
    auto parsed = roundtrip(msg);
    checkHeader(parsed, L3PD::RadioResource, L3RRMessage::HandoverCommand);
}

// ── Handover Complete (GSM 04.08 9.1.16) ─────────────────────────────

TEST(RoundTripTest, HandoverComplete) {
    uint8_t data[] = {0x06, 0x2c, 0x00}; // cause=Normal
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    auto* hc = dynamic_cast<L3HandoverComplete*>(msg.get());
    ASSERT_TRUE(hc);
    EXPECT_EQ(hc->cause(), RRCause::Normal_Event);

    auto parsed = roundtrip(*hc);
    checkHeader(parsed, L3PD::RadioResource, L3RRMessage::HandoverComplete);
}

// ── Handover Failure (GSM 04.08 9.1.17) ──────────────────────────────

TEST(RoundTripTest, HandoverFailure) {
    uint8_t data[] = {0x06, 0x28, 0x08}; // cause=Handover Impossible
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    auto* hf = dynamic_cast<L3HandoverFailure*>(msg.get());
    ASSERT_TRUE(hf);
    EXPECT_EQ(hf->cause(), RRCause::Handover_Impossible);

    auto parsed = roundtrip(*hf);
    checkHeader(parsed, L3PD::RadioResource, L3RRMessage::HandoverFailure);
}

// ── Physical Information (GSM 04.08 9.1.12) ──────────────────────────

TEST(RoundTripTest, PhysicalInformation) {
    L3PhysicalInformation msg;
    auto parsed = roundtrip(msg);
    checkHeader(parsed, L3PD::RadioResource, L3RRMessage::PhysicalInformation);
}

// ── Immediate Assignment (GSM 04.08 9.1.19) ──────────────────────────
// Reference: GSM_RR_Types.ttcn ImmediateAssignment
// Structure: DedOrTBF(4) + PageMode(4) + ChanDesc(24) + ReqRef(24) + TA(8) + MobileAlloc LV + RestOctets

TEST(RoundTripTest, ImmediateAssignment) {
    L3ImmediateAssignment msg;
    auto parsed = roundtrip(msg);
    checkHeader(parsed, L3PD::RadioResource, L3RRMessage::ImmediateAssignment);
}

// ── Immediate Assignment Extended (GSM 04.08 9.1.18) ─────────────────

TEST(RoundTripTest, ImmediateAssignmentExtended) {
    L3ImmediateAssignmentExtended msg;
    auto parsed = roundtrip(msg);
    checkHeader(parsed, L3PD::RadioResource, L3RRMessage::ImmediateAssignmentExtended);
}

// ── Immediate Assignment Reject (GSM 04.08 9.1.20) ───────────────────
// Reference: GSM_RR_Types.ttcn ImmediateAssignmentReject
// GSM_RestOctets.ttcn IARRestOctets

TEST(RoundTripTest, ImmediateAssignmentReject) {
    L3ImmediateAssignmentReject msg(30);
    auto parsed = roundtrip(msg);
    checkHeader(parsed, L3PD::RadioResource, L3RRMessage::ImmediateAssignmentReject);
}

// ── Additional Assignment (GSM 04.08 9.1.1) ──────────────────────────

TEST(RoundTripTest, AdditionalAssignment) {
    L3AdditionalAssignment msg;
    auto parsed = roundtrip(msg);
    checkHeader(parsed, L3PD::RadioResource, L3RRMessage::AdditionalAssignment);
}

// ── Channel Mode Modify (GSM 04.08 9.1.5) ────────────────────────────
// Reference: L3_Templates.ttcn tr_RRM_ModeModify

TEST(RoundTripTest, ChannelModeModify) {
    L3ChannelDescription chd(TDMA_TCHF, 1, 7, 100);
    L3ChannelMode mode(L3ChannelMode::SpeechV1);
    L3ChannelModeModify msg(chd, mode);
    auto parsed = roundtrip(msg);
    checkHeader(parsed, L3PD::RadioResource, L3RRMessage::ChannelModeModify);
}

// ── Channel Mode Modify Acknowledge (GSM 04.08 9.1.6) ────────────────

TEST(RoundTripTest, ChannelModeModifyAcknowledge) {
    uint8_t data[] = {0x06, 0x17,
        // ChanDesc: type&offset(5)=TDMA_TCHF(2), TN(3)=1, tsc(3)=7, h(1)=0, arfcn(10)=100
        0x24, 0x64, 0x14,
        // ChanMode: 0x82 (SpeechV1)
        0x82};
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    auto* cma = dynamic_cast<L3ChannelModeModifyAcknowledge*>(msg.get());
    ASSERT_TRUE(cma);
    EXPECT_EQ(cma->description().typeAndOffset(), TDMA_TCHF);
    EXPECT_EQ(cma->mode().mode(), L3ChannelMode::SpeechV1);

    auto parsed = roundtrip(*cma);
    checkHeader(parsed, L3PD::RadioResource, L3RRMessage::ChannelModeModifyAcknowledge);
}

// ── GPRS Suspension Request (GSM 04.08 9.1.13b) ──────────────────────

TEST(RoundTripTest, GPRSSuspensionRequest) {
    L3GPRSSuspensionRequest msg;
    auto parsed = roundtrip(msg);
    checkHeader(parsed, L3PD::RadioResource, L3RRMessage::GPRSSuspensionRequest);
}

// ── Application Information (GSM 04.08 9.1.53) ───────────────────────
// Reference: L3_Templates.ttcn tr_RR_APP_INFO

TEST(RoundTripTest, ApplicationInformation) {
    BitVector data(8);
    size_t wp = 0;
    data.writeField(wp, 0xAB, 8);
    L3ApplicationInformation msg(data);
    auto parsed = roundtrip(msg);
    checkHeader(parsed, L3PD::RadioResource, L3RRMessage::ApplicationInformation);
}

// ── Synchronization Channel Information (GSM 04.08 9.1.30) ───────────

TEST(RoundTripTest, SynchronizationChannelInformation) {
    L3SynchronizationChannelInformation msg;
    auto parsed = roundtrip(msg);
    checkHeader(parsed, L3PD::RadioResource, L3RRMessage::SynchronizationChannelInformation);
}

// ── Channel Request (GSM 04.08 9.1.13) ───────────────────────────────

TEST(RoundTripTest, ChannelRequest) {
    L3ChannelRequest msg(0x42);
    auto parsed = roundtrip(msg);
    checkHeader(parsed, L3PD::RadioResource, L3RRMessage::ChannelRequest);
}

// ── Handover Access (GSM 04.08 9.1.14a) ──────────────────────────────

TEST(RoundTripTest, HandoverAccess) {
    L3HandoverAccess msg(0x17);
    auto parsed = roundtrip(msg);
    checkHeader(parsed, L3PD::RadioResource, L3RRMessage::HandoverAccess);
}

// ── Classmark Change (GSM 04.08 9.1.11) ──────────────────────────────
// Reference: L3_Templates.ttcn ts_RRM_CM_CHG

TEST(RoundTripTest, ClassmarkChange) {
    // Parse a Classmark Change message from hex
    // PD=0x06, MTI=0x16, CM2 LV (length=5, then 5 bytes of CM2)
    uint8_t data[] = {
        0x06, 0x16, // PD + MTI
        0x05,       // CM2 length = 5
        0x20, 0x00, 0x80, 0x40, 0x00 // CM2 value
    };
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->MTI(), L3RRMessage::ClassmarkChange);
}
