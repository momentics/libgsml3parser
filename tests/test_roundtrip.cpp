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

// GSM 04.08 9.1.22: PagingRequestType1 with IMSI MobileIdentity
// Reference: L3_Templates.ttcn ts_MI_IMSI_LV (IMSI BCD encoding with HEXORDER low nibble swap)
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
    L3PagingResponse msg;
    auto parsed = roundtrip(msg);
    checkHeader(parsed, L3PD::RadioResource, L3RRMessage::PagingResponse);
}

// ── System Information messages (GSM 04.08 9.1.31..9.1.43c) ──────────
// Reference: GSM_SystemInformation.ttcn SystemInformationType1..Type17,
// BTS_Tests.ttcn ts_SI*_default, GSM_RestOctets.ttcn

TEST(RoundTripTest, SystemInformationType1) {
    L3SystemInformationType1 msg;
    auto parsed = roundtrip(msg);
    checkHeader(parsed, L3PD::RadioResource, L3RRMessage::SystemInformationType1);
}

// GSM 04.08 9.1.32: BCCHFrequencyList(16) + NCCPermitted(1) + RACHControlParameters(3) = 20 bytes
// Reference: GSM_SystemInformation.ttcn SystemInformationType2 (no rest_octets)
TEST(RoundTripTest, SystemInformationType2) {
    L3SystemInformationType2 msg;
    auto parsed = roundtrip(msg);
    checkHeader(parsed, L3PD::RadioResource, L3RRMessage::SystemInformationType2);
}

// GSM 04.08 9.1.33: ExtdBCCHFrequencyList(16) + RACHControlParameters(3) + rest_octets(0..1)
// Reference: GSM_SystemInformation.ttcn SystemInformationType2bis
TEST(RoundTripTest, SystemInformationType2bis) {
    L3SystemInformationType2bis msg;
    auto parsed = roundtrip(msg);
    checkHeader(parsed, L3PD::RadioResource, L3RRMessage::SystemInformationType2bis);
}

// GSM 04.08 9.1.34: ExtdBCCHFrequencyList(16) + rest_octets(0..4)
// Reference: GSM_SystemInformation.ttcn SystemInformationType2ter
TEST(RoundTripTest, SystemInformationType2ter) {
    L3SystemInformationType2ter msg;
    auto parsed = roundtrip(msg);
    checkHeader(parsed, L3PD::RadioResource, L3RRMessage::SystemInformationType2ter);
}

// GSM 04.08 9.1.35: CellIdentity(2) + LAI(5) + ControlChannelDesc(3) + CellOptions(1) +
//   CellSelectionParameters(2) + RACHControlParameters(3) + SI3RestOctets
// Reference: GSM_SystemInformation.ttcn SystemInformationType3
TEST(RoundTripTest, SystemInformationType3) {
    L3SystemInformationType3 msg;
    auto parsed = roundtrip(msg);
    checkHeader(parsed, L3PD::RadioResource, L3RRMessage::SystemInformationType3);
}

// GSM 04.08 9.1.36: LAI(5) + CellSelectionParameters(2) + RACHControlParameters(3) +
//   [CBCH ChannelDesc TLV] + [CBCH MobileAlloc TLV] + SI4RestOctets
// Reference: GSM_SystemInformation.ttcn SystemInformationType4
TEST(RoundTripTest, SystemInformationType4) {
    L3SystemInformationType4 msg;
    auto parsed = roundtrip(msg);
    checkHeader(parsed, L3PD::RadioResource, L3RRMessage::SystemInformationType4);
}

// GSM 04.08 9.1.37: BCCHFrequencyList(16)
// Reference: GSM_SystemInformation.ttcn SystemInformationType5
TEST(RoundTripTest, SystemInformationType5) {
    L3SystemInformationType5 msg;
    auto parsed = roundtrip(msg);
    checkHeader(parsed, L3PD::RadioResource, L3RRMessage::SystemInformationType5);
}

// GSM 04.08 9.1.38: ExtdBCCHFrequencyList(16)
// Reference: GSM_SystemInformation.ttcn SystemInformationType5bis
TEST(RoundTripTest, SystemInformationType5bis) {
    L3SystemInformationType5bis msg;
    auto parsed = roundtrip(msg);
    checkHeader(parsed, L3PD::RadioResource, L3RRMessage::SystemInformationType5bis);
}

// GSM 04.08 9.1.39: ExtdBCCHFrequencyList(16)
// Reference: GSM_SystemInformation.ttcn SystemInformationType5ter
TEST(RoundTripTest, SystemInformationType5ter) {
    L3SystemInformationType5ter msg;
    auto parsed = roundtrip(msg);
    checkHeader(parsed, L3PD::RadioResource, L3RRMessage::SystemInformationType5ter);
}

// GSM 04.08 9.1.40: CellIdentity(2) + LAI(5) + CellOptionsSacch(1) + NCCPermitted(1) +
//   SI6RestOctets
// Reference: GSM_SystemInformation.ttcn SystemInformationType6
TEST(RoundTripTest, SystemInformationType6) {
    L3SystemInformationType6 msg;
    auto parsed = roundtrip(msg);
    checkHeader(parsed, L3PD::RadioResource, L3RRMessage::SystemInformationType6);
}

// GSM 04.08 9.1.41: CellIdentity(2) + LAI(5) + CellOptionsSacch(1) + NCCPermitted(1) +
//   NeighborCellDescription(16) + SI7RestOctets
// Reference: GSM_SystemInformation.ttcn SystemInformationType7
TEST(RoundTripTest, SystemInformationType7) {
    L3SystemInformationType7 msg;
    auto parsed = roundtrip(msg);
    checkHeader(parsed, L3PD::RadioResource, L3RRMessage::SystemInformationType7);
}

// GSM 04.08 9.1.42: CellChannelDescription(16) + CellOptionsSacch(1) + NCCPermitted(1) +
//   SI8RestOctets
// Reference: GSM_SystemInformation.ttcn SystemInformationType8
TEST(RoundTripTest, SystemInformationType8) {
    L3SystemInformationType8 msg;
    auto parsed = roundtrip(msg);
    checkHeader(parsed, L3PD::RadioResource, L3RRMessage::SystemInformationType8);
}

// GSM 04.08 9.1.43: CellIdentity(2) + LAI(5) + CellOptionsSacch(1) + NCCPermitted(1) +
//   NeighborCellDescription(16) + SI9RestOctets
// Reference: GSM_SystemInformation.ttcn SystemInformationType9
TEST(RoundTripTest, SystemInformationType9) {
    L3SystemInformationType9 msg;
    auto parsed = roundtrip(msg);
    checkHeader(parsed, L3PD::RadioResource, L3RRMessage::SystemInformationType9);
}

// GSM 04.08 9.1.43a: SI13RestOctets (GPRSCellOptions, etc.)
// Reference: GSM_SystemInformation.ttcn SystemInformationType13
TEST(RoundTripTest, SystemInformationType13) {
    L3SystemInformationType13 msg;
    auto parsed = roundtrip(msg);
    checkHeader(parsed, L3PD::RadioResource, L3RRMessage::SystemInformationType13);
}

// GSM 04.08 9.1.43b: TDDCellDescription + TDDCellOptions + TDDCellSelectionParameters +
//   TDDRACHControlParameters + SI16RestOctets
// Reference: GSM_SystemInformation.ttcn SystemInformationType16
TEST(RoundTripTest, SystemInformationType16) {
    L3SystemInformationType16 msg;
    auto parsed = roundtrip(msg);
    checkHeader(parsed, L3PD::RadioResource, L3RRMessage::SystemInformationType16);
}

// GSM 04.08 9.1.43c: TDDCellIdentity + TD德拉LocationAreaIdentification + TDDCellOptionsSacch +
//   TDDNCCPermitted + TDDNeighborCellDescription + SI17RestOctets
// Reference: GSM_SystemInformation.ttcn SystemInformationType17
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
// Reference: L3_Templates.ttcn tr_RRM_RR_STATUS, GSM_RR_Types.ttcn RR_STATUS='00010010'B
// GSM 04.08 10.2: PD=0x06(RR) high nibble, skip=0, MTI=0x12(RRStatus), cause=0x60
// Byte 0: PD(4)|skip(4) = 0110 0000 = 0x60
// Byte 1: MTI = 0x12
// Byte 2: cause = 0x60 (Invalid_Mandatory_Information)
TEST(RoundTripTest, RRStatus) {
    uint8_t data[] = {0x60, 0x12, 0x60};
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
// Reference: GSM_RR_Types.ttcn ASSIGNMENT_COMPLETE='00101001'B = 0x29
// GSM 04.08 10.2: PD=0x06(RR) high nibble, skip=0, MTI=0x29(AssignmentComplete)
// Byte 0: PD(4)|skip(4) = 0110 0000 = 0x60
// Byte 1: MTI = 0x29
// Byte 2: cause = 0x00 (Normal_Event)
TEST(RoundTripTest, AssignmentComplete) {
    uint8_t data[] = {0x60, 0x29, 0x00};
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    auto* ac = dynamic_cast<L3AssignmentComplete*>(msg.get());
    ASSERT_TRUE(ac);
    EXPECT_EQ(ac->cause(), RRCause::Normal_Event);

    auto parsed = roundtrip(*ac);
    checkHeader(parsed, L3PD::RadioResource, L3RRMessage::AssignmentComplete);
}

// ── Assignment Failure (GSM 04.08 9.1.3) ──────────────────────────────
// Reference: GSM_RR_Types.ttcn ASSIGNMENT_FAILURE='00101111'B = 0x2F
// GSM 04.08 10.2: PD=0x06(RR) high nibble, skip=0, MTI=0x2F(AssignmentFailure)
// Byte 0: 0x60, Byte 1: 0x2F, Byte 2: cause=0x09(Channel_Mode_Unacceptable)
TEST(RoundTripTest, AssignmentFailure) {
    uint8_t data[] = {0x60, 0x2F, 0x09};
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
// Reference: GSM_RR_Types.ttcn HANDOVER_COMPLETE='00101100'B = 0x2C
// GSM 04.08 10.2: PD=0x06(RR) high nibble, skip=0, MTI=0x2C(HandoverComplete), cause=Normal
TEST(RoundTripTest, HandoverComplete) {
    uint8_t data[] = {0x60, 0x2C, 0x00};
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    auto* hc = dynamic_cast<L3HandoverComplete*>(msg.get());
    ASSERT_TRUE(hc);
    EXPECT_EQ(hc->cause(), RRCause::Normal_Event);

    auto parsed = roundtrip(*hc);
    checkHeader(parsed, L3PD::RadioResource, L3RRMessage::HandoverComplete);
}

// ── Handover Failure (GSM 04.08 9.1.17) ──────────────────────────────
// Reference: GSM_RR_Types.ttcn HANDOVER_FAILURE='00101000'B = 0x28
// GSM 04.08 10.2: PD=0x06(RR) high nibble, skip=0, MTI=0x28(HandoverFailure), cause=Handover_Impossible
TEST(RoundTripTest, HandoverFailure) {
    uint8_t data[] = {0x60, 0x28, 0x08};
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
// Reference: GSM_RR_Types.ttcn IMMEDIATE_ASSIGNMENT_REJECT='00111010'B = 0x3A
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
// Reference: GSM_RR_Types.ttcn CHANNEL_MODE_MODIFY_ACKNOWLEDGE='00010111'B = 0x17
// GSM 04.08 10.2: PD=0x06(RR) high nibble, skip=0, MTI=0x17(ChannelModeModifyAcknowledge)
// Byte 0: PD(4)|skip(4) = 0110 0000 = 0x60
// Byte 1: MTI = 0x17
TEST(RoundTripTest, ChannelModeModifyAcknowledge) {
    uint8_t data[] = {0x60, 0x17,
        // ChanDesc: typeAndOffset(5)=TDMA_TCHF(2), TN(3)=1, TSC(3)=7, h(1)=0, spare(2)=0, ARFCN(10)=100
        // Bits: 00010 001 111 0 00 0001100100
        // Byte 0: 00010001 = 0x11
        // Byte 1: 11100000 = 0xE0
        // Byte 2: 00011001 00 → 01100100 = 0x64
        0x11, 0xE0, 0x64,
        // ChanMode: SpeechV1 = 1
        0x01};
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
// Reference: GSM_RR_Types.ttcn GPRS_SUSPENSION_REQUEST='00110100'B = 0x34
// 3GPP 44.018 3.4.25: GPRS Suspension procedure, TLLI + RA_ID + SuspensionCause
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
// SynchronizationChannelInformation uses MTI=0x100 (internal RrShortDisc code),
// not a standard 8-bit RR messageType. Reference: GSM_RR_Types.ttcn RrShortDisc.
// These are sent on SCH and use a different encoding path.
TEST(RoundTripTest, SynchronizationChannelInformation) {
    L3SynchronizationChannelInformation msg;
    auto parsed = roundtrip(msg);
    checkHeader(parsed, L3PD::RadioResource, L3RRMessage::SynchronizationChannelInformation);
}

// ── Channel Request (GSM 04.08 9.1.13) ───────────────────────────────
// ChannelRequest uses MTI=0x101 (internal RrShortDisc code).
// Reference: GSM_RR_Types.ttcn RrShortDisc. Sent on RACH, encoded differently.
TEST(RoundTripTest, ChannelRequest) {
    L3ChannelRequest msg(0x42);
    auto parsed = roundtrip(msg);
    checkHeader(parsed, L3PD::RadioResource, L3RRMessage::ChannelRequest);
}

// ── Handover Access (GSM 04.08 9.1.14a) ──────────────────────────────
// HandoverAccess uses MTI=0x102 (internal RrShortDisc code).
// Reference: GSM_RR_Types.ttcn RrShortDisc. Sent on HO access timeslot.
TEST(RoundTripTest, HandoverAccess) {
    L3HandoverAccess msg(0x17);
    auto parsed = roundtrip(msg);
    checkHeader(parsed, L3PD::RadioResource, L3RRMessage::HandoverAccess);
}

// ── Classmark Change (GSM 04.08 9.1.11) ──────────────────────────────
// Reference: L3_Templates.ttcn ts_RRM_CM_CHG, GSM_RR_Types.ttcn CLASSMARK_CHANGE='00010110'B
// GSM 04.08 10.2: PD=0x06(RR) high nibble, skip=0, MTI=0x16(ClassmarkChange)
// Byte 0: PD(4)|skip(4) = 0110 0000 = 0x60
// Byte 1: MTI = 0x16
// Byte 2: CM2 length = 3 (L3MobileStationClassmark2 is 24 bits = 3 bytes)
// Bytes 3-5: CM2 value
TEST(RoundTripTest, ClassmarkChange) {
    uint8_t data[] = {
        0x60, 0x16, // PD + MTI
        0x03,       // CM2 length = 3
        0x20, 0x00, 0x80 // CM2 value (24 bits)
    };
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->MTI(), L3RRMessage::ClassmarkChange);
}
