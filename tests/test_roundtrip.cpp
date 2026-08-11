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
//
// [GOLDEN VERIFICATION]
// All round-trip hex parse test data verified against osmo-ttcn3-hacks reference:
//   - RR ChannelRelease {0x60, 0x0D, 0x00}: PD=6(RR), MTI=0x0D(ChannelRelease), cause=0x00(Normal_Event)
//     Verified against GSM_RR_Types.ttcn CHANNEL_RELEASE='00001101'B(0x0D), RR_Cause NORMAL='00'O
//   - RR AssignmentComplete {0x60, 0x29, 0x00}: PD=6(RR), MTI=0x29(AssignmentComplete), cause=0x00(Normal_Event)
//     Verified against GSM_RR_Types.ttcn ASSIGNMENT_COMPLETE='00101001'B(0x29)
//   - RR AssignmentFailure {0x60, 0x2F, 0x09}: PD=6(RR), MTI=0x2F(AssignmentFailure), cause=0x09(Channel_Mode_Unacceptable)
//     Verified against GSM_RR_Types.ttcn ASSIGNMENT_FAILURE='00101111'B(0x2F), RR_Cause CH_MODE_UNACC='09'O
//   - RR HandoverComplete {0x60, 0x2C, 0x00}: PD=6(RR), MTI=0x2C(HandoverComplete), cause=0x00(Normal_Event)
//     Verified against GSM_RR_Types.ttcn HANDOVER_COMPLETE='00101100'B(0x2C)
//   - RR HandoverFailure {0x60, 0x28, 0x08}: PD=6(RR), MTI=0x28(HandoverFailure), cause=0x08(Handover_Impossible)
//     Verified against GSM_RR_Types.ttcn HANDOVER_FAILURE='00101000'B(0x28), RR_Cause HNDOVER_IMP='08'O
//   - RR ClassmarkChange {0x60, 0x16, 0x03, 0x20, 0x00, 0x80}: PD=6(RR), MTI=0x16(ClassmarkChange), CM2 LV
//     Verified against GSM_RR_Types.ttcn CLASSMARK_CHANGE='00010110'B(0x16)
//   - RR ChannelModeModifyAcknowledge {0x60, 0x17, ChanDesc, ChanMode}: PD=6(RR), MTI=0x17(CMMAck)
//     Verified against GSM_RR_Types.ttcn CHANNEL_MODE_MODIFY_ACKNOWLEDGE='00010111'B(0x17)
//   - All SI message types (SI1-SI17) verified against GSM_RR_Types.ttcn RrMessageType enum values

#include <gtest/gtest.h>
#include <gsml3parser/parser.h>
#include <gsml3parser/rr/l3rrmessages.h>
#include <gsml3parser/common/l3common.h>
#include <gsml3parser/visitor.h>

using namespace gsml3parser;

static Expected<ParsedMessage> roundtrip(const ParsedMessage& msg) {
    auto hex = writeL3Hex(msg);
    if (!hex) return Expected<ParsedMessage>::error(hex.error());
    return parseL3Hex(hex.value());
}

static void checkHeader(const ParsedMessage& parsed, L3PD expectPD, int expectMTI) {
    EXPECT_EQ(messagePD(parsed), expectPD);
    EXPECT_EQ(messageMTI(parsed), expectMTI);
}

// Paging Request Type 1 (GSM 04.08 9.1.22)
// Reference: L3_Templates.ttcn ts_PAG_RESP builds on ts_MI_TMSI_LV / ts_MI_IMSI_LV
// Paging Request Type 1 structure:
//   PD(4)=0x06, MTI(8)=0x21, ChanNeeded(4), PageMode(4), MI1 LV..., [MI2 TLV...]

TEST(RoundTripTest, PagingRequestType1_TMSI) {
    L3MobileIdentity id(0x12345678);
    L3PagingRequestType1 concrete = L3PagingRequestType1::builder()
        .addMobileId(id, ChannelType::SDCCHType).build();
    ParsedMessage msg{RRM{std::move(concrete)}};
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    checkHeader(*parsed, L3PD::RadioResource, L3PagingRequestType1::MTI);
}

// GSM 04.08 9.1.22: PagingRequestType1 with IMSI MobileIdentity
// Reference: L3_Templates.ttcn ts_MI_IMSI_LV (IMSI BCD encoding with HEXORDER low nibble swap)
TEST(RoundTripTest, PagingRequestType1_IMSI) {
    L3MobileIdentity id("250011234567890");
    L3PagingRequestType1 concrete = L3PagingRequestType1::builder()
        .addMobileId(id, ChannelType::TCHFType).build();
    ParsedMessage msg{RRM{std::move(concrete)}};
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    checkHeader(*parsed, L3PD::RadioResource, L3PagingRequestType1::MTI);
}

// Paging Request Type 2 (GSM 04.08 9.1.23)

TEST(RoundTripTest, PagingRequestType2) {
    L3PagingRequestType2 concrete = L3PagingRequestType2::builder()
        .addTMSI(0xDEADBEEF, ChannelType::SDCCHType).build();
    ParsedMessage msg{RRM{std::move(concrete)}};
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    checkHeader(*parsed, L3PD::RadioResource, L3PagingRequestType2::MTI);
}

// Paging Request Type 3 (GSM 04.08 9.1.24)

TEST(RoundTripTest, PagingRequestType3) {
    L3PagingRequestType3 concrete = L3PagingRequestType3::builder()
        .addTMSI(0xABCDEF01, ChannelType::TCHHType).build();
    ParsedMessage msg{RRM{std::move(concrete)}};
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    checkHeader(*parsed, L3PD::RadioResource, L3PagingRequestType3::MTI);
}

// Paging Response (GSM 04.08 9.1.25)
// Reference: L3_Templates.ttcn ts_PAG_RESP
// Structure: spare_half(4), CKSN(4), CM2 LV, MI LV, [addl_upd_par TV]

TEST(RoundTripTest, PagingResponse) {
    ParsedMessage msg{RRM{L3PagingResponse{}}};
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    checkHeader(*parsed, L3PD::RadioResource, L3PagingResponse::MTI);
}

// System Information messages (GSM 04.08 9.1.31..9.1.43c)
// Reference: GSM_SystemInformation.ttcn SystemInformationType1..Type17,
// BTS_Tests.ttcn ts_SI*_default, GSM_RestOctets.ttcn

TEST(RoundTripTest, SystemInformationType1) {
    ParsedMessage msg{RRM{L3SystemInformationType1{}}};
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    checkHeader(*parsed, L3PD::RadioResource, L3SystemInformationType1::MTI);
}

// GSM 04.08 9.1.32: BCCHFrequencyList(16) + NCCPermitted(1) + RACHControlParameters(3) = 20 bytes
// Reference: GSM_SystemInformation.ttcn SystemInformationType2 (no rest_octets)
TEST(RoundTripTest, SystemInformationType2) {
    ParsedMessage msg{RRM{L3SystemInformationType2{}}};
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    checkHeader(*parsed, L3PD::RadioResource, L3SystemInformationType2::MTI);
}

// GSM 04.08 9.1.33: ExtdBCCHFrequencyList(16) + RACHControlParameters(3) + rest_octets(0..1)
// Reference: GSM_SystemInformation.ttcn SystemInformationType2bis
TEST(RoundTripTest, SystemInformationType2bis) {
    ParsedMessage msg{RRM{L3SystemInformationType2bis{}}};
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    checkHeader(*parsed, L3PD::RadioResource, L3SystemInformationType2bis::MTI);
}

// GSM 04.08 9.1.34: ExtdBCCHFrequencyList(16) + rest_octets(0..4)
// Reference: GSM_SystemInformation.ttcn SystemInformationType2ter
TEST(RoundTripTest, SystemInformationType2ter) {
    ParsedMessage msg{RRM{L3SystemInformationType2ter{}}};
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    checkHeader(*parsed, L3PD::RadioResource, L3SystemInformationType2ter::MTI);
}

// GSM 04.08 9.1.35: CellIdentity(2) + LAI(5) + ControlChannelDesc(3) + CellOptions(1) +
//   CellSelectionParameters(2) + RACHControlParameters(3) + SI3RestOctets
// Reference: GSM_SystemInformation.ttcn SystemInformationType3
TEST(RoundTripTest, SystemInformationType3) {
    ParsedMessage msg{RRM{L3SystemInformationType3{}}};
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    checkHeader(*parsed, L3PD::RadioResource, L3SystemInformationType3::MTI);
}

// GSM 04.08 9.1.36: LAI(5) + CellSelectionParameters(2) + RACHControlParameters(3) +
//   [CBCH ChannelDesc TLV] + [CBCH MobileAlloc TLV] + SI4RestOctets
// Reference: GSM_SystemInformation.ttcn SystemInformationType4
TEST(RoundTripTest, SystemInformationType4) {
    ParsedMessage msg{RRM{L3SystemInformationType4{}}};
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    checkHeader(*parsed, L3PD::RadioResource, L3SystemInformationType4::MTI);
}

// GSM 04.08 9.1.37: BCCHFrequencyList(16)
// Reference: GSM_SystemInformation.ttcn SystemInformationType5
TEST(RoundTripTest, SystemInformationType5) {
    ParsedMessage msg{RRM{L3SystemInformationType5{}}};
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    checkHeader(*parsed, L3PD::RadioResource, L3SystemInformationType5::MTI);
}

// GSM 04.08 9.1.38: ExtdBCCHFrequencyList(16)
// Reference: GSM_SystemInformation.ttcn SystemInformationType5bis
TEST(RoundTripTest, SystemInformationType5bis) {
    ParsedMessage msg{RRM{L3SystemInformationType5bis{}}};
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    checkHeader(*parsed, L3PD::RadioResource, L3SystemInformationType5bis::MTI);
}

// GSM 04.08 9.1.39: ExtdBCCHFrequencyList(16)
// Reference: GSM_SystemInformation.ttcn SystemInformationType5ter
TEST(RoundTripTest, SystemInformationType5ter) {
    ParsedMessage msg{RRM{L3SystemInformationType5ter{}}};
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    checkHeader(*parsed, L3PD::RadioResource, L3SystemInformationType5ter::MTI);
}

// GSM 04.08 9.1.40: CellIdentity(2) + LAI(5) + CellOptionsSacch(1) + NCCPermitted(1) +
//   SI6RestOctets
// Reference: GSM_SystemInformation.ttcn SystemInformationType6
TEST(RoundTripTest, SystemInformationType6) {
    ParsedMessage msg{RRM{L3SystemInformationType6{}}};
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    checkHeader(*parsed, L3PD::RadioResource, L3SystemInformationType6::MTI);
}

// GSM 04.08 9.1.41: CellIdentity(2) + LAI(5) + CellOptionsSacch(1) + NCCPermitted(1) +
//   NeighborCellDescription(16) + SI7RestOctets
// Reference: GSM_SystemInformation.ttcn SystemInformationType7
TEST(RoundTripTest, SystemInformationType7) {
    ParsedMessage msg{RRM{L3SystemInformationType7{}}};
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    checkHeader(*parsed, L3PD::RadioResource, L3SystemInformationType7::MTI);
}

// GSM 04.08 9.1.42: CellChannelDescription(16) + CellOptionsSacch(1) + NCCPermitted(1) +
//   SI8RestOctets
// Reference: GSM_SystemInformation.ttcn SystemInformationType8
TEST(RoundTripTest, SystemInformationType8) {
    ParsedMessage msg{RRM{L3SystemInformationType8{}}};
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    checkHeader(*parsed, L3PD::RadioResource, L3SystemInformationType8::MTI);
}

// GSM 04.08 9.1.43: CellIdentity(2) + LAI(5) + CellOptionsSacch(1) + NCCPermitted(1) +
//   NeighborCellDescription(16) + SI9RestOctets
// Reference: GSM_SystemInformation.ttcn SystemInformationType9
TEST(RoundTripTest, SystemInformationType9) {
    ParsedMessage msg{RRM{L3SystemInformationType9{}}};
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    checkHeader(*parsed, L3PD::RadioResource, L3SystemInformationType9::MTI);
}

// GSM 04.08 9.1.43a: SI13RestOctets (GPRSCellOptions, etc.)
// Reference: GSM_SystemInformation.ttcn SystemInformationType13
TEST(RoundTripTest, SystemInformationType13) {
    ParsedMessage msg{RRM{L3SystemInformationType13{}}};
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    checkHeader(*parsed, L3PD::RadioResource, L3SystemInformationType13::MTI);
}

// GSM 04.08 9.1.43b: TDDCellDescription + TDDCellOptions + TDDCellSelectionParameters +
//   TDDRACHControlParameters + SI16RestOctets
// Reference: GSM_SystemInformation.ttcn SystemInformationType16
TEST(RoundTripTest, SystemInformationType16) {
    ParsedMessage msg{RRM{L3SystemInformationType16{}}};
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    checkHeader(*parsed, L3PD::RadioResource, L3SystemInformationType16::MTI);
}

// GSM 04.08 9.1.43c: TDDCellIdentity + TDDLocationAreaIdentification + TDDCellOptionsSacch +
//   TDDNCCPermitted + TDDNeighborCellDescription + SI17RestOctets
// Reference: GSM_SystemInformation.ttcn SystemInformationType17
TEST(RoundTripTest, SystemInformationType17) {
    ParsedMessage msg{RRM{L3SystemInformationType17{}}};
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    checkHeader(*parsed, L3PD::RadioResource, L3SystemInformationType17::MTI);
}

// Channel Release (GSM 04.08 9.1.7)
// Reference: L3_Templates.ttcn tr_RRM_RR_RELEASE

TEST(RoundTripTest, ChannelRelease_Normal) {
    ParsedMessage msg{RRM{L3ChannelRelease{RRCause::Normal_Event}}};
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    auto* cr = tryGet<L3ChannelRelease>(*parsed);
    ASSERT_TRUE(cr);
    EXPECT_EQ(cr->cause(), RRCause::Normal_Event);
}

TEST(RoundTripTest, ChannelRelease_Preemptive) {
    ParsedMessage msg{RRM{L3ChannelRelease{RRCause::Preemptive_Release}}};
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    auto* cr = tryGet<L3ChannelRelease>(*parsed);
    ASSERT_TRUE(cr);
    EXPECT_EQ(cr->cause(), RRCause::Preemptive_Release);
}

// RR Status (GSM 04.08 9.1.29)
// Reference: L3_Templates.ttcn tr_RRM_RR_STATUS, GSM_RR_Types.ttcn RR_STATUS='00010010'B
// GSM 04.08 10.2: PD=0x06(RR) high nibble, skip=0, MTI=0x12(RRStatus), cause=0x60
// Byte 0: PD(4)|skip(4) = 0110 0000 = 0x60
// Byte 1: MTI = 0x12
// Byte 2: cause = 0x60 (Invalid_Mandatory_Information)
TEST(RoundTripTest, RRStatus) {
    uint8_t data[] = {0x60, 0x12, 0x60};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messagePD(*msg), L3PD::RadioResource);
    EXPECT_EQ(messageMTI(*msg), L3RRStatus::MTI);
    auto* rs = tryGet<L3RRStatus>(*msg);
    ASSERT_TRUE(rs);
    EXPECT_EQ(rs->cause(), RRCause::Invalid_Mandatory_Information);
}

// Assignment Command (GSM 04.08 9.1.2)
// Reference: L3_Templates.ttcn tr_RR_AssignmentCommand
// GSM_RR_Types.ttcn AssignmentCommand: ChanDesc(24 bits) + PowerCmd(8 bits) + [optional IEs]

TEST(RoundTripTest, AssignmentCommand) {
    ParsedMessage msg{RRM{L3AssignmentCommand{}}};
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    checkHeader(*parsed, L3PD::RadioResource, L3AssignmentCommand::MTI);
}

// Assignment Complete (GSM 04.08 9.1.3)
// Reference: GSM_RR_Types.ttcn ASSIGNMENT_COMPLETE='00101001'B = 0x29
// GSM 04.08 10.2: PD=0x06(RR) high nibble, skip=0, MTI=0x29(AssignmentComplete)
// Byte 0: PD(4)|skip(4) = 0110 0000 = 0x60
// Byte 1: MTI = 0x29
// Byte 2: cause = 0x00 (Normal_Event)
TEST(RoundTripTest, AssignmentComplete) {
    uint8_t data[] = {0x60, 0x29, 0x00};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    auto* ac = tryGet<L3AssignmentComplete>(*msg);
    ASSERT_TRUE(ac);
    EXPECT_EQ(ac->cause(), RRCause::Normal_Event);

    auto parsed = roundtrip(*msg);
    ASSERT_TRUE(parsed);
    checkHeader(*parsed, L3PD::RadioResource, L3AssignmentComplete::MTI);
}

// Assignment Failure (GSM 04.08 9.1.3)
// Reference: GSM_RR_Types.ttcn ASSIGNMENT_FAILURE='00101111'B = 0x2F
// GSM 04.08 10.2: PD=0x06(RR) high nibble, skip=0, MTI=0x2F(AssignmentFailure)
// Byte 0: 0x60, Byte 1: 0x2F, Byte 2: cause=0x09(Channel_Mode_Unacceptable)
TEST(RoundTripTest, AssignmentFailure) {
    uint8_t data[] = {0x60, 0x2F, 0x09};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    auto* af = tryGet<L3AssignmentFailure>(*msg);
    ASSERT_TRUE(af);
    EXPECT_EQ(af->cause(), RRCause::Channel_Mode_Unacceptable);

    auto parsed = roundtrip(*msg);
    ASSERT_TRUE(parsed);
    checkHeader(*parsed, L3PD::RadioResource, L3AssignmentFailure::MTI);
}

// Classmark Enquiry (GSM 04.08 9.1.14)
// Reference: L3_Templates.ttcn tr_RRM_CM_ENQUIRY

TEST(RoundTripTest, ClassmarkEnquiry) {
    ParsedMessage msg{RRM{L3ClassmarkEnquiry{}}};
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    checkHeader(*parsed, L3PD::RadioResource, L3ClassmarkEnquiry::MTI);
}

// Measurement Report (GSM 04.08 9.1.21)
// Reference: L3_Templates.ttcn ts_MEAS_REP, ts_MeasurementResults
// GSM_RR_Types.ttcn MeasurementResults: 16 bytes fixed

TEST(RoundTripTest, MeasurementReport) {
    ParsedMessage msg{RRM{L3MeasurementReport{}}};
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    checkHeader(*parsed, L3PD::RadioResource, L3MeasurementReport::MTI);
}

// Ciphering Mode Command (GSM 04.08 9.1.9)
// Reference: L3_Templates.ttcn ts_RRM_CiphModeCmd

TEST(RoundTripTest, CipheringModeCommand_A5_0) {
    ParsedMessage msg{RRM{L3CipheringModeCommand{false, 0}}};
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    checkHeader(*parsed, L3PD::RadioResource, L3CipheringModeCommand::MTI);
}

TEST(RoundTripTest, CipheringModeCommand_A5_3) {
    ParsedMessage msg{RRM{L3CipheringModeCommand{true, 3}}};
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    checkHeader(*parsed, L3PD::RadioResource, L3CipheringModeCommand::MTI);
}

// Ciphering Mode Complete (GSM 04.08 9.1.10)

TEST(RoundTripTest, CipheringModeComplete) {
    ParsedMessage msg{RRM{L3CipheringModeComplete{}}};
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    checkHeader(*parsed, L3PD::RadioResource, L3CipheringModeComplete::MTI);
}

// Handover Command (GSM 04.08 9.1.15)
// Reference: GSM_RR_Types.ttcn HandoverCommand
// L3_Templates.ttcn ts_RR_HandoverCommand
// Structure: CellDesc(16) + ChanDesc(24) + HORef(8) + PowerCmdAccType(8) + SyncInd(8) = 70 bits

TEST(RoundTripTest, HandoverCommand) {
    ParsedMessage msg{RRM{L3HandoverCommand{}}};
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    checkHeader(*parsed, L3PD::RadioResource, L3HandoverCommand::MTI);
}

// Handover Complete (GSM 04.08 9.1.16)
// Reference: GSM_RR_Types.ttcn HANDOVER_COMPLETE='00101100'B = 0x2C
// GSM 04.08 10.2: PD=0x06(RR) high nibble, skip=0, MTI=0x2C(HandoverComplete), cause=Normal
TEST(RoundTripTest, HandoverComplete) {
    uint8_t data[] = {0x60, 0x2C, 0x00};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    auto* hc = tryGet<L3HandoverComplete>(*msg);
    ASSERT_TRUE(hc);
    EXPECT_EQ(hc->cause(), RRCause::Normal_Event);

    auto parsed = roundtrip(*msg);
    ASSERT_TRUE(parsed);
    checkHeader(*parsed, L3PD::RadioResource, L3HandoverComplete::MTI);
}

// Handover Failure (GSM 04.08 9.1.17)
// Reference: GSM_RR_Types.ttcn HANDOVER_FAILURE='00101000'B = 0x28
// GSM 04.08 10.2: PD=0x06(RR) high nibble, skip=0, MTI=0x28(HandoverFailure), cause=Handover_Impossible
TEST(RoundTripTest, HandoverFailure) {
    uint8_t data[] = {0x60, 0x28, 0x08};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    auto* hf = tryGet<L3HandoverFailure>(*msg);
    ASSERT_TRUE(hf);
    EXPECT_EQ(hf->cause(), RRCause::Handover_Impossible);

    auto parsed = roundtrip(*msg);
    ASSERT_TRUE(parsed);
    checkHeader(*parsed, L3PD::RadioResource, L3HandoverFailure::MTI);
}

// Physical Information (GSM 04.08 9.1.12)

TEST(RoundTripTest, PhysicalInformation) {
    ParsedMessage msg{RRM{L3PhysicalInformation{}}};
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    checkHeader(*parsed, L3PD::RadioResource, L3PhysicalInformation::MTI);
}

// Immediate Assignment (GSM 04.08 9.1.19)
// Reference: GSM_RR_Types.ttcn ImmediateAssignment
// Structure: DedOrTBF(4) + PageMode(4) + ChanDesc(24) + ReqRef(24) + TA(8) + MobileAlloc LV + RestOctets

TEST(RoundTripTest, ImmediateAssignment) {
    ParsedMessage msg{RRM{L3ImmediateAssignment{}}};
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    checkHeader(*parsed, L3PD::RadioResource, L3ImmediateAssignment::MTI);
}

// Immediate Assignment Extended (GSM 04.08 9.1.18)

TEST(RoundTripTest, ImmediateAssignmentExtended) {
    ParsedMessage msg{RRM{L3ImmediateAssignmentExtended{}}};
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    checkHeader(*parsed, L3PD::RadioResource, L3ImmediateAssignmentExtended::MTI);
}

// Immediate Assignment Reject (GSM 04.08 9.1.20)
// Reference: GSM_RR_Types.ttcn IMMEDIATE_ASSIGNMENT_REJECT='00111010'B = 0x3A
// GSM_RestOctets.ttcn IARRestOctets
TEST(RoundTripTest, ImmediateAssignmentReject) {
    ParsedMessage msg{RRM{L3ImmediateAssignmentReject{30}}};
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    checkHeader(*parsed, L3PD::RadioResource, L3ImmediateAssignmentReject::MTI);
}

// Additional Assignment (GSM 04.08 9.1.1)

TEST(RoundTripTest, AdditionalAssignment) {
    ParsedMessage msg{RRM{L3AdditionalAssignment{}}};
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    checkHeader(*parsed, L3PD::RadioResource, L3AdditionalAssignment::MTI);
}

// Channel Mode Modify (GSM 04.08 9.1.5)
// Reference: L3_Templates.ttcn tr_RRM_ModeModify

TEST(RoundTripTest, ChannelModeModify) {
    L3ChannelDescription chd(TDMA_TCHF, 1, 7, 100);
    L3ChannelMode mode(L3ChannelMode::SpeechV1);
    ParsedMessage msg{RRM{L3ChannelModeModify{chd, mode}}};
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    checkHeader(*parsed, L3PD::RadioResource, L3ChannelModeModify::MTI);
}

// Channel Mode Modify Acknowledge (GSM 04.08 9.1.6)
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
        // Byte 2: 00011001 00 -> 01100100 = 0x64
        0x11, 0xE0, 0x64,
        // ChanMode: SpeechV1 = 1
        0x01};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    auto* cma = tryGet<L3ChannelModeModifyAcknowledge>(*msg);
    ASSERT_TRUE(cma);
    EXPECT_EQ(cma->description().typeAndOffset(), TDMA_TCHF);
    EXPECT_EQ(cma->mode().mode(), L3ChannelMode::SpeechV1);

    auto parsed = roundtrip(*msg);
    ASSERT_TRUE(parsed);
    checkHeader(*parsed, L3PD::RadioResource, L3ChannelModeModifyAcknowledge::MTI);
}

// GPRS Suspension Request (GSM 04.08 9.1.13b)
// Reference: GSM_RR_Types.ttcn GPRS_SUSPENSION_REQUEST='00110100'B = 0x34
// 3GPP 44.018 3.4.25: GPRS Suspension procedure, TLLI + RA_ID + SuspensionCause
TEST(RoundTripTest, GPRSSuspensionRequest) {
    ParsedMessage msg{RRM{L3GPRSSuspensionRequest{}}};
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    checkHeader(*parsed, L3PD::RadioResource, L3GPRSSuspensionRequest::MTI);
}

// Application Information (GSM 04.08 9.1.53)
// Reference: L3_Templates.ttcn tr_RR_APP_INFO

TEST(RoundTripTest, ApplicationInformation) {
    ParsedMessage msg{RRM{L3ApplicationInformation{{0xAB}}}};
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    checkHeader(*parsed, L3PD::RadioResource, L3ApplicationInformation::MTI);
}

// Synchronization Channel Information (GSM 04.08 9.1.30)
// SynchronizationChannelInformation uses MTI=0x100 (internal RrShortDisc code),
// not a standard 8-bit RR messageType. Reference: GSM_RR_Types.ttcn RrShortDisc.
// These are sent on SCH and use a different encoding path.
TEST(RoundTripTest, SynchronizationChannelInformation) {
    ParsedMessage msg{RRM{L3SynchronizationChannelInformation{}}};
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    checkHeader(*parsed, L3PD::RadioResource, L3SynchronizationChannelInformation::MTI);
}

// Channel Request (GSM 04.08 9.1.13)
// ChannelRequest uses MTI=0x101 (internal RrShortDisc code).
// Reference: GSM_RR_Types.ttcn RrShortDisc. Sent on RACH, encoded differently.
TEST(RoundTripTest, ChannelRequest) {
    ParsedMessage msg{RRM{L3ChannelRequest{0x42}}};
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    checkHeader(*parsed, L3PD::RadioResource, L3ChannelRequest::MTI);
}

// Handover Access (GSM 04.08 9.1.14a)
// HandoverAccess uses MTI=0x102 (internal RrShortDisc code).
// Reference: GSM_RR_Types.ttcn RrShortDisc. Sent on HO access timeslot.
TEST(RoundTripTest, HandoverAccess) {
    ParsedMessage msg{RRM{L3HandoverAccess{0x17}}};
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    checkHeader(*parsed, L3PD::RadioResource, L3HandoverAccess::MTI);
}

// Classmark Change (GSM 04.08 9.1.11)
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
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3ClassmarkChange::MTI);
}

// System Information Type 2quater (GSM 04.08 §9.1.34a, MTI=0x4e)
TEST(RoundTripTest, SI2quater_RoundTrip) {
    ParsedMessage msg{RRM{L3SystemInformationType2quater{}}};
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    checkHeader(*parsed, L3PD::RadioResource, L3SystemInformationType2quater::MTI);
}

// System Information Type 11 (GSM 04.08 §9.1.43o, MTI=0x4b)
TEST(RoundTripTest, SI11_RoundTrip) {
    ParsedMessage msg{RRM{L3SystemInformationType11{}}};
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    checkHeader(*parsed, L3PD::RadioResource, L3SystemInformationType11::MTI);
}

// Measurement Order (GSM 04.08 §9.1.21a, MTI=0x3c)
TEST(RoundTripTest, MeasurementOrder_RoundTrip) {
    ParsedMessage msg{RRM{L3MeasurementOrder{}}};
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    checkHeader(*parsed, L3PD::RadioResource, L3MeasurementOrder::MTI);
}
