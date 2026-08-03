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

// Comprehensive GSM Layer 3 Golden Tests (Part 1: RR).
// Reference: osmo-ttcn3-hacks L3_Templates.ttcn, GSM_Types.ttcn,
// GSM_RR_Types.ttcn, GSM_SystemInformation.ttcn, GSM_RestOctets.ttcn.
// Spec: 3GPP TS 24.008, 3GPP TS 44.018 (GSM 04.08).

#include <gtest/gtest.h>
#include <gsml3parser/parser.h>
#include <gsml3parser/common/l3common.h>
#include <gsml3parser/gsm_common.h>
#include <gsml3parser/rr/l3rrmessages.h>
#include <gsml3parser/bitvector.h>

using namespace gsml3parser;

// ── Helper: serialize → parse → return ─────────────────────────────────
static std::unique_ptr<L3Message> roundtrip(const L3Message& msg) {
    std::vector<uint8_t> buf(msg.fullLength());
    size_t n = writeL3(msg, buf.data(), buf.size());
    if (n == 0) return nullptr;
    return parseL3(buf.data(), n);
}

// =====================================================================
// RR MESSAGE TYPE VALUES (3GPP TS 44.018 Table 10.4.1 / GSM 04.08 Table 10.4.1)
// Reference: GSM_RR_Types.ttcn RrMessageType enum (line 24):
//   SYSTEM_INFORMATION_TYPE_1 ('00011001'B = 0x19)
//   PAGING_REQUEST_TYPE_1 ('00100001'B = 0x21)
//   ASSIGNMENT_COMMAND ('00101110'B = 0x2E)
//   HANDOVER_COMMAND ('00101011'B = 0x2B)
//   IMMEDIATE_ASSIGNMENT ('00111111'B = 0x3F)
//   CIPHERING_MODE_COMMAND ('00110101'B = 0x35)
//   etc.
// Spec-verified: All RR MTI values per 3GPP TS 44.018 Table 10.4.1 (8-bit field)
// =====================================================================

TEST(GoldenRR, MessageTypeValues) {
    // Spec-verified: 3GPP TS 44.018 Table 10.4.1 RR message type identifier values
    // System Information messages (GSM_RR_Types.ttcn lines 57-74):
    EXPECT_EQ(L3RRMessage::SystemInformationType1, 0x19);     // '00011001'B - 44.018 9.1.31
    EXPECT_EQ(L3RRMessage::SystemInformationType2, 0x1a);     // '00011010'B - 44.018 9.1.32
    EXPECT_EQ(L3RRMessage::SystemInformationType2bis, 0x02);  // '00000010'B - 44.018 9.1.33
    EXPECT_EQ(L3RRMessage::SystemInformationType2ter, 0x03);  // '00000011'B - 44.018 9.1.34
    EXPECT_EQ(L3RRMessage::SystemInformationType3, 0x1b);     // '00011011'B - 44.018 9.1.35
    EXPECT_EQ(L3RRMessage::SystemInformationType4, 0x1c);     // '00011100'B - 44.018 9.1.36
    EXPECT_EQ(L3RRMessage::SystemInformationType5, 0x1d);     // '00011101'B - 44.018 9.1.37
    EXPECT_EQ(L3RRMessage::SystemInformationType5bis, 0x05);  // '00000101'B - 44.018 9.1.38
    EXPECT_EQ(L3RRMessage::SystemInformationType5ter, 0x06);  // '00000110'B - 44.018 9.1.39
    EXPECT_EQ(L3RRMessage::SystemInformationType6, 0x1e);     // '00011110'B - 44.018 9.1.40
    EXPECT_EQ(L3RRMessage::SystemInformationType7, 0x1f);     // '00011111'B - 44.018 9.1.41
    EXPECT_EQ(L3RRMessage::SystemInformationType8, 0x18);     // '00011000'B - 44.018 9.1.42
    EXPECT_EQ(L3RRMessage::SystemInformationType9, 0x04);     // '00000100'B - 44.018 9.1.43
    EXPECT_EQ(L3RRMessage::SystemInformationType13, 0x00);    // '00000000'B - 44.018 9.1.43a
    EXPECT_EQ(L3RRMessage::SystemInformationType16, 0x3d);    // '00111101'B - 44.018 9.1.43b
    EXPECT_EQ(L3RRMessage::SystemInformationType17, 0x3e);    // '00111110'B - 44.018 9.1.43c
    // Assignment/Handover messages (GSM_RR_Types.ttcn lines 38-44):
    EXPECT_EQ(L3RRMessage::AssignmentCommand, 0x2e);          // '00101110'B - 44.018 9.1.2
    EXPECT_EQ(L3RRMessage::AssignmentComplete, 0x29);         // '00101001'B - 44.018 9.1.3
    EXPECT_EQ(L3RRMessage::AssignmentFailure, 0x2f);          // '00101111'B - 44.018 9.1.3
    EXPECT_EQ(L3RRMessage::HandoverCommand, 0x2b);            // '00101011'B - 44.018 9.1.15
    EXPECT_EQ(L3RRMessage::HandoverComplete, 0x2c);           // '00101100'B - 44.018 9.1.16
    EXPECT_EQ(L3RRMessage::HandoverFailure, 0x28);            // '00101000'B - 44.018 9.1.17
    // Paging messages (GSM_RR_Types.ttcn lines 50-54):
    EXPECT_EQ(L3RRMessage::PagingRequestType1, 0x21);         // '00100001'B - 44.018 9.1.22
    EXPECT_EQ(L3RRMessage::PagingRequestType2, 0x22);         // '00100010'B - 44.018 9.1.23
    EXPECT_EQ(L3RRMessage::PagingRequestType3, 0x24);         // '00100100'B - 44.018 9.1.24
    EXPECT_EQ(L3RRMessage::PagingResponse, 0x27);             // '00100111'B - 44.018 9.1.25
    // Immediate Assignment (GSM_RR_Types.ttcn lines 26-28):
    EXPECT_EQ(L3RRMessage::ImmediateAssignment, 0x3f);        // '00111111'B - 44.018 9.1.19
    EXPECT_EQ(L3RRMessage::ImmediateAssignmentExtended, 0x39);// '00111001'B - 44.018 9.1.18
    EXPECT_EQ(L3RRMessage::ImmediateAssignmentReject, 0x3a);  // '00111010'B - 44.018 9.1.20
    EXPECT_EQ(L3RRMessage::AdditionalAssignment, 0x3b);       // '00111011'B - 44.018 9.1.1
    // Other RR messages:
    EXPECT_EQ(L3RRMessage::ChannelRelease, 0x0d);             // '00001101'B - 44.018 9.1.7
    EXPECT_EQ(L3RRMessage::PhysicalInformation, 0x2d);        // '00101101'B - 44.018 9.1.12
    EXPECT_EQ(L3RRMessage::CipheringModeCommand, 0x35);       // '00110101'B - 44.018 9.1.9
    EXPECT_EQ(L3RRMessage::CipheringModeComplete, 0x32);      // '00110010'B - 44.018 9.1.10
    EXPECT_EQ(L3RRMessage::ChannelModeModify, 0x10);          // '00010000'B - 44.018 9.1.5
    EXPECT_EQ(L3RRMessage::ChannelModeModifyAcknowledge, 0x17);// '00010111'B - 44.018 9.1.6
    EXPECT_EQ(L3RRMessage::RRStatus, 0x12);                   // '00010010'B - 44.018 9.1.29
    EXPECT_EQ(L3RRMessage::ClassmarkChange, 0x16);            // '00010110'B - 44.018 9.1.11
    EXPECT_EQ(L3RRMessage::ClassmarkEnquiry, 0x13);           // '00010011'B - 44.018 9.1.14
    EXPECT_EQ(L3RRMessage::MeasurementReport, 0x15);          // '00010101'B - 44.018 9.1.21
    EXPECT_EQ(L3RRMessage::GPRSSuspensionRequest, 0x34);      // '00110100'B - 44.018 9.1.13b
    EXPECT_EQ(L3RRMessage::ApplicationInformation, 0x38);     // '00111000'B - 44.018 9.1.53
}

// =====================================================================
// RR PARSE FROM HEX: Paging Request Type 1 (3GPP TS 44.018 9.1.22 / GSM 04.08 9.1.22)
// Reference: L3_Templates.ttcn tr_PAGING_REQ1 (line 541):
//   discriminator := '0110'B (PD=6=RR), messageType := '00100001'B (MTI=0x21)
// Reference: GSM_RR_Types.ttcn PagingRequestType1 (line 568):
//   ChannelNeeded12 chan_needed, PageMode page_mode, MobileIdentityLV mi1
// Spec-verified: PD=6(RR), MTI=0x21(PagingRequestType1) per 3GPP TS 44.018 Table 10.4.1
// =====================================================================

TEST(GoldenRR, PagingRequestType1_Parse) {
    // Byte 0: PD(4)=6(RR)|skip(4)=0 = 0x60 [GSM 24.008 Table 11.2]
    // Byte 1: MTI = 0x21 (PagingRequestType1) [3GPP TS 44.018 Table 10.4.1]
    // Byte 2: ChanNeeded(4)=1(SDCCH)|PageMode(4)=0(Normal) = 0x10 [GSM 24.008 10.5.2.26]
    //   GSM_Types.ttcn ChannelNeeded: CHAN_NEED_SDCCH(1), PageMode: PAGE_MODE_NORMAL(0)
    // Byte 3: MI LV length = 5 (1 type octet + 4 TMSI octets) [GSM 24.008 10.5.1.4]
    // Byte 4: spare(4)=0|type(3)=100(TMSI)|oe(1)=0 = 0x0C [GSM_RR_Types.ttcn MobileIdentityType]
    // Bytes 5-8: TMSI = 0x12345678 (4 octets, MSB first)
    uint8_t data[] = {
        0x60, 0x21, 0x10, 0x05, 0x0C, 0x12, 0x34, 0x56, 0x78
    };
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->PD(), L3PD::RadioResource);
    EXPECT_EQ(msg->MTI(), L3RRMessage::PagingRequestType1);
}

// =====================================================================
// RR PARSE FROM HEX: Paging Request Type 2 (3GPP TS 44.018 9.1.23 / GSM 04.08 9.1.23)
// Reference: L3_Templates.ttcn tr_PAGING_REQ2 (line 561):
//   discriminator := '0110'B (PD=6=RR), messageType := '00100010'B (MTI=0x22)
// Reference: GSM_RR_Types.ttcn PagingRequestType2 (line 577):
//   ChannelNeeded12 chan_needed, PageMode page_mode, GsmTmsi mi1, GsmTmsi mi2
// Spec-verified: PD=6(RR), MTI=0x22(PagingRequestType2) per 3GPP TS 44.018 Table 10.4.1
// =====================================================================

TEST(GoldenRR, PagingRequestType2_Parse) {
    // Byte 0: PD(4)=6(RR)|skip(4)=0 = 0x60 [GSM 24.008 Table 11.2]
    // Byte 1: MTI = 0x22 (PagingRequestType2) [3GPP TS 44.018 Table 10.4.1]
    // Byte 2: ChanNeeded(4)=1(SDCCH)|PageMode(4)=0(Normal) = 0x10
    // Byte 3: MI length = 5 (TMSI type + 4 octets)
    // Byte 4: spare(4)=0|type(3)=100(TMSI)|oe(1)=0 = 0x0C
    // Bytes 5-8: TMSI = 0xDEADBEEF (MSB first)
    uint8_t data[] = {
        0x60, 0x22, 0x10, 0x05, 0x0C, 0xDE, 0xAD, 0xBE, 0xEF
    };
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->MTI(), L3RRMessage::PagingRequestType2);
}

// =====================================================================
// RR PARSE FROM HEX: Paging Request Type 3 (3GPP TS 44.018 9.1.24 / GSM 04.08 9.1.24)
// Reference: L3_Templates.ttcn tr_PAGING_REQ3 (line 583):
//   discriminator := '0110'B (PD=6=RR), messageType := '00100100'B (MTI=0x24)
// Reference: GSM_RR_Types.ttcn PagingRequestType3 (line 588):
//   ChannelNeeded12 chan_needed, PageMode page_mode, GsmTmsi4 mi
// Spec-verified: PD=6(RR), MTI=0x24(PagingRequestType3) per 3GPP TS 44.018 Table 10.4.1
// =====================================================================

TEST(GoldenRR, PagingRequestType3_Parse) {
    // Byte 0: PD(4)=6(RR)|skip(4)=0 = 0x60 [GSM 24.008 Table 11.2]
    // Byte 1: MTI = 0x24 (PagingRequestType3) [3GPP TS 44.018 Table 10.4.1]
    // Byte 2: ChanNeeded(4)=1(SDCCH)|PageMode(4)=0(Normal) = 0x10
    // Byte 3: MI length = 5 (TMSI type + 4 octets)
    // Byte 4: spare(4)=0|type(3)=100(TMSI)|oe(1)=0 = 0x0C
    // Bytes 5-8: TMSI = 0xABCDEF01 (MSB first)
    uint8_t data[] = {
        0x60, 0x24, 0x10, 0x05, 0x0C, 0xAB, 0xCD, 0xEF, 0x01
    };
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->MTI(), L3RRMessage::PagingRequestType3);
}

// =====================================================================
// RR PARSE FROM HEX: Paging Response (3GPP TS 44.018 9.1.25 / GSM 04.08 9.1.25)
// Reference: L3_Templates.ttcn ts_PAG_RESP (line 610):
//   discriminator := overwritten, messageType := '00100111'B (MTI=0x27)
//   cipheringKeySequenceNumber, mobileStationClassmark, mobileIdentity
// Structure: spare(4)|CKSN(4), CM2 LV (length + 3 octets), MI LV
// Spec-verified: PD=6(RR), MTI=0x27(PagingResponse) per 3GPP TS 44.018 Table 10.4.1
// CKSN: GSM 24.008 10.5.1.2 (3-bit key sequence number, range 0-7)
// CM2: GSM 24.008 10.5.1.6 (24-bit Classmark 2 capability flags)
// =====================================================================

TEST(GoldenRR, PagingResponse_Parse) {
    // Byte 0: PD(4)=6(RR)|skip(4)=0 = 0x60 [GSM 24.008 Table 11.2]
    // Byte 1: MTI = 0x27 (PagingResponse) [3GPP TS 44.018 Table 10.4.1]
    // Byte 2: spare(4)=0|CKSN(4)=0 = 0x00 [GSM 24.008 10.5.1.2]
    // Byte 3: CM2 LV length = 3 (Classmark 2 is 3 octets) [GSM 24.008 10.5.1.6]
    // Bytes 4-6: CM2 value (24 bits of capability flags)
    // Byte 7: MI LV length = 5 [GSM 24.008 10.5.1.4]
    // Byte 8: spare(4)=0|type(3)=100(TMSI)|oe(1)=0 = 0x0C
    // Bytes 9-12: TMSI = 0x12345678 (MSB first)
    uint8_t data[] = {
        0x60, 0x27, 0x00,
        0x03, 0x20, 0x00, 0x80,
        0x05, 0x0C, 0x12, 0x34, 0x56, 0x78
    };
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->MTI(), L3RRMessage::PagingResponse);
}

// =====================================================================
// RR PARSE FROM HEX: Classmark Change (3GPP TS 44.018 9.1.11 / GSM 04.08 9.1.11)
// Reference: L3_Templates.ttcn ts_RRM_CM_CHG template
// Reference: GSM_RR_Types.ttcn CLASSMARK_CHANGE ('00010110'B = 0x16, line 81)
// Structure: CM2 LV (length + 3 octets Classmark 2)
// Spec-verified: PD=6(RR), MTI=0x16(ClassmarkChange) per 3GPP TS 44.018 Table 10.4.1
// =====================================================================

TEST(GoldenRR, ClassmarkChange_Parse) {
    // Byte 0: PD(4)=6(RR)|skip(4)=0 = 0x60 [GSM 24.008 Table 11.2]
    // Byte 1: MTI = 0x16 (ClassmarkChange) [3GPP TS 44.018 Table 10.4.1]
    // Byte 2: CM2 LV length = 3 (Classmark 2 is 3 octets) [GSM 24.008 10.5.1.6]
    // Bytes 3-5: CM2 value (24 bits of capability flags)
    uint8_t data[] = {0x60, 0x16, 0x03, 0x20, 0x00, 0x80};
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->MTI(), L3RRMessage::ClassmarkChange);
    auto* cm = dynamic_cast<L3ClassmarkChange*>(msg.get());
    ASSERT_TRUE(cm);
    auto rt = roundtrip(*cm);
    ASSERT_TRUE(rt);
    EXPECT_EQ(rt->MTI(), L3RRMessage::ClassmarkChange);
}

// =====================================================================
// RR PARSE FROM HEX: Measurement Report (3GPP TS 44.018 9.1.21 / GSM 04.08 9.1.21)
// Reference: L3_Templates.ttcn ts_MEAS_REP template
// Reference: GSM_RR_Types.ttcn MeasurementResults (line 457):
//   ba_used(1), dtx_used(1), rxlev_full(6), threeg_ba(1), meas_valid(1),
//   rxlev_sub(6), si23_ba(1), rxqual_full(3), rxqual_sub(3), no_ncell(3)
// Structure: 16 bytes of MeasurementResults (128 bits, padded to 16 octets)
// Spec-verified: PD=6(RR), MTI=0x15(MeasurementReport) per 3GPP TS 44.018 Table 10.4.1
// =====================================================================

TEST(GoldenRR, MeasurementReport_Parse) {
    // Byte 0: PD(4)=6(RR)|skip(4)=0 = 0x60 [GSM 24.008 Table 11.2]
    // Byte 1: MTI = 0x15 (MeasurementReport) [3GPP TS 44.018 Table 10.4.1]
    // Bytes 2-17: MeasurementResults (16 bytes, all zero = default values)
    //   GSM_RR_Types.ttcn MeasurementResults: 128-bit structure padded to 16 octets
    uint8_t data[] = {
        0x60, 0x15,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->MTI(), L3RRMessage::MeasurementReport);
}

// =====================================================================
// RR PARSE FROM HEX: Handover Command (3GPP TS 44.018 9.1.15 / GSM 04.08 9.1.15)
// Reference: L3_Templates.ttcn ts_RR_HandoverCommand (line 871):
//   discriminator := '0110'B (PD=6=RR), messageType := '00101011'B (MTI=0x2B)
// Reference: GSM_RR_Types.ttcn HandoverCommand (line 505):
//   CellDescriptionV cell_desc, ChannelDescription chan_desc,
//   OCT1 ho_ref, PowerCommandAndAccesstype_V power_cmd_acc_type
// Reference: GSM_RR_Types.ttcn CellDescriptionV (line 528):
//   uint3_t bcc, uint3_t ncc, uint10_t bcch_arfcn [FIELDORDER(lsb)]
// Structure: CellDesc(16 bits LSB: bcc+ncc+arfcn) + ChanDesc(24 bits) + HORef(8) + PowerCmdAccType(8) + SyncInd(8)
// Spec-verified: PD=6(RR), MTI=0x2B(HandoverCommand) per 3GPP TS 44.018 Table 10.4.1
// =====================================================================

TEST(GoldenRR, HandoverCommand_Parse) {
    // Byte 0: PD(4)=6(RR)|skip(4)=0 = 0x60 [GSM 24.008 Table 11.2]
    // Byte 1: MTI = 0x2B (HandoverCommand) [3GPP TS 44.018 Table 10.4.1]
    // Bytes 2-3: CellDesc: ARFCN=100, NCC=5, BCC=3 [GSM 24.008 10.5.2.2]
    //   GSM_RR_Types.ttcn CellDescriptionV: FIELDORDER(lsb) - bcc first, then ncc, then arfcn
    //   bcc(3)=011, ncc(3)=101, arfcn(10)=0001100100
    //   LSB-first: 011|101|00 = 0x74, 00011001|00xxxxxx = 0x19 (arfcn=100=0x64, high 2 bits in byte 1)
    // Bytes 4-6: ChanDesc: typeAndOffset(5), TN(3), TSC(3), h(1), spare(2), ARFCN(10) [GSM 24.008 10.5.2.5]
    //   GSM_RR_Types.ttcn ChannelDescription (line 313): chan_nr(5), tsc(3), h(1), arfcn(12)
    // Byte 7: HORef = 0x17 [GSM 24.008 10.5.2.15, 5-bit handover reference]
    // Byte 8: PowerCmdAccType = 0x00 [GSM 24.008 10.5.2.28a]
    // Byte 9: SyncInd = 0x00 [GSM 24.008 10.5.2.39]
    uint8_t data[] = {
        0x60, 0x2b,
        0x74, 0x19,
        0x11, 0xE0, 0x64,
        0x17, 0x00, 0x00
    };
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->MTI(), L3RRMessage::HandoverCommand);
}

// =====================================================================
// RR PARSE FROM HEX: Assignment Command (3GPP TS 44.018 9.1.2 / GSM 04.08 9.1.2)
// Reference: L3_Templates.ttcn tr_RR_AssignmentCommand (line 732):
//   discriminator := '0110'B (PD=6=RR), messageType := '00101110'B (MTI=0x2E)
// Reference: GSM_RR_Types.ttcn AssignmentCommand (line 483):
//   ChannelDescription chan_desc, PowerCommand_V power_cmd, ChannelMode_TV chan1_mode
// Structure: ChanDesc(24 bits) + PowerCmd(8 bits) + [optional IEs]
// Spec-verified: PD=6(RR), MTI=0x2E(AssignmentCommand) per 3GPP TS 44.018 Table 10.4.1
// =====================================================================

TEST(GoldenRR, AssignmentCommand_Parse) {
    // Byte 0: PD(4)=6(RR)|skip(4)=0 = 0x60 [GSM 24.008 Table 11.2]
    // Byte 1: MTI = 0x2E (AssignmentCommand) [3GPP TS 44.018 Table 10.4.1]
    // Bytes 2-4: ChanDesc: typeAndOffset(5), TN(3), TSC(3), h(1), spare(2), ARFCN(10) [GSM 24.008 10.5.2.5]
    //   GSM_RR_Types.ttcn ChannelDescription (line 313): chan_nr + tsc + h + arfcn
    // Byte 5: PowerCmd = 0x00 [GSM 24.008 10.5.2.28, 5-bit power_command << 3]
    uint8_t data[] = {0x60, 0x2e, 0x10, 0xE0, 0x64, 0x00};
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->MTI(), L3RRMessage::AssignmentCommand);
}

// =====================================================================
// RR PARSE FROM HEX: Immediate Assignment (3GPP TS 44.018 9.1.19 / GSM 04.08 9.1.19)
// Reference: GSM_RR_Types.ttcn ImmediateAssignment (line 536):
//   DedicatedModeOrTbf ded_or_tbf, PageMode page_mode, ChannelDescription chan_desc,
//   RequestReference req_ref, TimingAdvance timing_advance, MobileAllocationLV mobile_allocation
// Structure: DedOrTBF(4)|PageMode(4) + ChanDesc(24 bits) + ReqRef(24 bits) + TA(8 bits) + MobileAlloc LV
// Spec-verified: PD=6(RR), MTI=0x3F(ImmediateAssignment) per 3GPP TS 44.018 Table 10.4.1
// =====================================================================

TEST(GoldenRR, ImmediateAssignment_Parse) {
    // Byte 0: PD(4)=6(RR)|skip(4)=0 = 0x60 [GSM 24.008 Table 11.2]
    // Byte 1: MTI = 0x3F (ImmediateAssignment) [3GPP TS 44.018 Table 10.4.1]
    // Byte 2: DedOrTBF(4)=0(dedicated)|PageMode(4)=0(Normal) = 0x00
    //   GSM_RR_Types.ttcn DedicatedModeOrTbf (line 374): spare+tma+downlink+tbf
    //   GSM_RR_Types.ttcn PageMode (line 382): PAGE_MODE_NORMAL(0)
    // Bytes 3-5: ChanDesc: typeAndOffset(5), TN(3), TSC(3), h(1), spare(2), ARFCN(10) [GSM 24.008 10.5.2.5]
    //   SDCCH, TN=0, TSC=0, h=0, ARFCN=100
    // Bytes 6-8: ReqRef: RA(8)=0x42, T1p(5)=0, T3(6)=0, T2(5)=0 [GSM 24.008 10.5.2.30]
    //   GSM_RR_Types.ttcn RequestReference (line 390): ra(8), t1p(5), t3(6), t2(5)
    // Byte 9: TA = 0x00 [GSM 24.008 10.5.2.40, 6-bit timing_advance << 2]
    // Byte 10: MobileAlloc LV length = 0 (no mobile allocation)
    uint8_t data[] = {
        0x60, 0x3f, 0x00,
        0x00, 0x00, 0x64,
        0x42, 0x00, 0x00,
        0x00, 0x00
    };
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->MTI(), L3RRMessage::ImmediateAssignment);
}

// =====================================================================
// RR PARSE FROM HEX: Immediate Assignment Reject (3GPP TS 44.018 9.1.20 / GSM 04.08 9.1.20)
// Reference: GSM_RR_Types.ttcn IMMEDIATE_ASSIGNMENT_REJECT ('00111010'B = 0x3A, line 28)
// Reference: GSM_RR_Types.ttcn ImmediateAssignmentReject (line 555):
//   FeatureIndicator feature_ind, PageMode page_mode, ReqRefWaitInd4 payload
// Structure: FeatureIndicator/PageMode(8 bits) + [optional RequestReferences]
// Spec-verified: PD=6(RR), MTI=0x3A(ImmediateAssignmentReject) per 3GPP TS 44.018 Table 10.4.1
// =====================================================================

TEST(GoldenRR, ImmediateAssignmentReject_Parse) {
    // Byte 0: PD(4)=6(RR)|skip(4)=0 = 0x60 [GSM 24.008 Table 11.2]
    // Byte 1: MTI = 0x3A (ImmediateAssignmentReject) [3GPP TS 44.018 Table 10.4.1]
    // Byte 2: PageMode(4)=0(Normal)|WaitIndication(4)=3 = 0x03 [GSM 24.008 10.5.2.43]
    uint8_t data[] = {0x60, 0x3a, 0x03};
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->MTI(), L3RRMessage::ImmediateAssignmentReject);
    auto* iar = dynamic_cast<L3ImmediateAssignmentReject*>(msg.get());
    ASSERT_TRUE(iar);
    EXPECT_EQ(iar->waitTime(), 3u);
}

// =====================================================================
// RR PARSE FROM HEX: Channel Mode Modify (3GPP TS 44.018 9.1.5 / GSM 04.08 9.1.5)
// Reference: L3_Templates.ttcn tr_RRM_ModeModify (line 650):
//   discriminator := '0110'B (PD=6=RR), messageType := '00010000'B (MTI=0x10)
// Reference: GSM_RR_Types.ttcn CHANNEL_MODE_MODIFY ('00010000'B = 0x10, line 76)
// Structure: ChanDesc(24 bits) + ChanMode(4 bits) + [optional MultiRate]
// Spec-verified: PD=6(RR), MTI=0x10(ChannelModeModify) per 3GPP TS 44.018 Table 10.4.1
// =====================================================================

TEST(GoldenRR, ChannelModeModify_Parse) {
    // Byte 0: PD(4)=6(RR)|skip(4)=0 = 0x60 [GSM 24.008 Table 11.2]
    // Byte 1: MTI = 0x10 (ChannelModeModify) [3GPP TS 44.018 Table 10.4.1]
    // Bytes 2-4: ChanDesc: typeAndOffset(5), TN(3), TSC(3), h(1), spare(2), ARFCN(10) [GSM 24.008 10.5.2.5]
    //   TDMA_TCHF, TN=1, TSC=7, h=0, ARFCN=100
    // Byte 5: ChanMode(4)=1(SpeechV1)|spare(4)=0 = 0x01 [GSM 24.008 10.5.2.6]
    uint8_t data[] = {0x60, 0x10, 0x11, 0xE0, 0x64, 0x01};
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->MTI(), L3RRMessage::ChannelModeModify);
}

// =====================================================================
// RR PARSE FROM HEX: GPRS Suspension Request (GSM 04.08 9.1.13b)
// Reference: GSM_RR_Types.ttcn GPRS_SUSPENSION_REQUEST
// Structure: TLLI(32) + RA_ID(48) + SuspensionCause(8) + ServiceSupport(8)
// =====================================================================

TEST(GoldenRR, GPRSSuspensionRequest_Parse) {
    // Byte 0: PD(4)|skip(4) = 0x60
    // Byte 1: MTI = 0x34 (GPRSSuspensionRequest)
    // Bytes 2-5: TLLI = 0x12345678
    // Bytes 6-11: RA_ID (6 bytes) = 0
    // Byte 12: SuspensionCause = 0x00
    // Byte 13: ServiceSupport = 0x00
    uint8_t data[] = {
        0x60, 0x34,
        0x12, 0x34, 0x56, 0x78,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00
    };
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->MTI(), L3RRMessage::GPRSSuspensionRequest);
}

// =====================================================================
// RR PARSE FROM HEX: Application Information (GSM 04.08 9.1.53)
// Reference: L3_Templates.ttcn tr_RR_APP_INFO
// Structure: ProtocolIdentifier(4) + CR(4) + FirstSegment(1) + LastSegment(1) + Data
// =====================================================================

TEST(GoldenRR, ApplicationInformation_Parse) {
    // Byte 0: PD(4)|skip(4) = 0x60
    // Byte 1: MTI = 0x38 (ApplicationInformation)
    // Byte 2: ProtocolId(4)=0, CR(4)=0 = 0x00
    // Byte 3: FirstSeg(1)=0, LastSeg(1)=0, spare(2)=0, data(4) = 0xAB
    // Byte 4: data continued = 0xCD
    uint8_t data[] = {0x60, 0x38, 0x00, 0xAB, 0xCD};
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->MTI(), L3RRMessage::ApplicationInformation);
}

// =====================================================================
// RR PARSE FROM HEX: Synchronization Channel Information (GSM 04.08 9.1.30)
// Reference: GSM_RR_Types.ttcn RrShortDisc
// Structure: CI(16) + LAI(40) = 7 bytes, no PD/MTI header
// =====================================================================

TEST(GoldenRR, SynchronizationChannelInformation_Parse) {
    // SCH uses internal MTI=0x100, no standard L3 header
    // Byte 0-1: CI = 0x1234
    // Byte 2-4: MCC/MNC (BCD, nibble-swapped) for MCC=250, MNC=01
    // Byte 5-6: LAC = 0x0001
    uint8_t data[] = {0x12, 0x34, 0x52, 0xF0, 0x10, 0x00, 0x01};
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->MTI(), L3RRMessage::SynchronizationChannelInformation);
}

// =====================================================================
// RR PARSE FROM HEX: Channel Request (GSM 04.08 9.1.13)
// Reference: GSM_RR_Types.ttcn RrShortDisc
// Structure: RequestReference(8), no PD/MTI header
// =====================================================================

TEST(GoldenRR, ChannelRequest_Parse) {
    // Channel Request uses internal MTI=0x101, no standard L3 header
    // Byte 0: RequestReference = 0x42
    uint8_t data[] = {0x42};
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->MTI(), L3RRMessage::ChannelRequest);
}

// =====================================================================
// RR PARSE FROM HEX: Handover Access (GSM 04.08 9.1.14a)
// Reference: GSM_RR_Types.ttcn RrShortDisc
// Structure: HandoverNumber(8) + HandoverReference(8) + TA(8) + Spare(8)
// =====================================================================

TEST(GoldenRR, HandoverAccess_Parse) {
    // Handover Access uses internal MTI=0x102, no standard L3 header
    // Bytes 0-3: HO number(8), HO ref(8), TA(8), spare(8)
    uint8_t data[] = {0x17, 0x00, 0x00, 0x00};
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->MTI(), L3RRMessage::HandoverAccess);
}

// =====================================================================
// RR PARSE FROM HEX: Ciphering Mode Command (3GPP TS 44.018 9.1.9 / GSM 04.08 9.1.9)
// Reference: L3_Templates.ttcn ts_RRM_CiphModeCmd (line 690):
//   messageType := '00110101'B (MTI=0x35), cipherModeSetting: sC='1'B, algorithmIdentifier
// Reference: GSM_RR_Types.ttcn CIPHERING_MODE_COMMAND ('00110101'B = 0x35, line 31)
// Structure: Ciphering(1)|Algorithm(3)|CipheringModeResponse(4) = 8 bits
// Spec-verified: PD=6(RR), MTI=0x35(CipheringModeCommand) per 3GPP TS 44.018 Table 10.4.1
// CipheringModeSetting: GSM 24.008 10.5.2.9 (4 bits: ciphering(1), algorithm(3))
//   Algorithm 3 = A5/3 (KASUMI)
// =====================================================================

TEST(GoldenRR, CipheringModeCommand_Parse) {
    // Byte 0: PD(4)=6(RR)|skip(4)=0 = 0x60 [GSM 24.008 Table 11.2]
    // Byte 1: MTI = 0x35 (CipheringModeCommand) [3GPP TS 44.018 Table 10.4.1]
    // Byte 2: ciphering(1)=1(on)|algorithm(3)=3(A5/3)|cipherModeResponse(4)=0 = 0x13
    //   GSM 24.008 10.5.2.9: sC(1)+algorithmIdentifier(3), L3_Templates.ttcn line 699-701
    uint8_t data[] = {0x60, 0x35, 0x13};
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->MTI(), L3RRMessage::CipheringModeCommand);
}

// =====================================================================
// RR PARSE FROM HEX: RR Status (GSM 04.08 9.1.29)
// Reference: L3_Templates.ttcn tr_RRM_RR_STATUS
// =====================================================================

TEST(GoldenRR, RRStatus_Parse_ProtocolError) {
    // Byte 0: PD(4)|skip(4) = 0x60
    // Byte 1: MTI = 0x12 (RRStatus)
    // Byte 2: cause = 0x6f (Protocol_Error_Unspecified)
    uint8_t data[] = {0x60, 0x12, 0x6f};
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    auto* rs = dynamic_cast<L3RRStatus*>(msg.get());
    ASSERT_TRUE(rs);
    EXPECT_EQ(rs->cause(), RRCause::Protocol_Error_Unspecified);
}

// =====================================================================
// RR PARSE FROM HEX: Physical Information (3GPP TS 44.018 9.1.12 / GSM 04.08 9.1.12)
// Reference: GSM_RR_Types.ttcn PHYSICAL_INFORMATION ('00101101'B = 0x2D, line 44)
// Structure: TimingAdvance(8 bits, GSM 24.008 10.5.2.40)
// Spec-verified: PD=6(RR), MTI=0x2D(PhysicalInformation) per 3GPP TS 44.018 Table 10.4.1
// TimingAdvance: 6-bit value (0-63) shifted left by 2 bits, spare(2)=0
// =====================================================================

TEST(GoldenRR, PhysicalInformation_Parse) {
    // Byte 0: PD(4)=6(RR)|skip(4)=0 = 0x60 [GSM 24.008 Table 11.2]
    // Byte 1: MTI = 0x2D (PhysicalInformation) [3GPP TS 44.018 Table 10.4.1]
    // Byte 2: TA = 60<<2 = 0x3C [GSM 24.008 10.5.2.40: timing_advance(6)|spare(2)]
    uint8_t data[] = {0x60, 0x2d, 0x3C};
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->MTI(), L3RRMessage::PhysicalInformation);
}

// =====================================================================
// RR PARSE FROM HEX: Additional Assignment (3GPP TS 44.018 9.1.1 / GSM 04.08 9.1.1)
// Reference: GSM_RR_Types.ttcn ADDITIONAL_ASSIGNMENT ('00111011'B = 0x3B, line 25)
// Structure: AdditionalChanDesc(24 bits) + [optional PowerCommand(8 bits)]
// Spec-verified: PD=6(RR), MTI=0x3B(AdditionalAssignment) per 3GPP TS 44.018 Table 10.4.1
// =====================================================================

TEST(GoldenRR, AdditionalAssignment_Parse) {
    // Byte 0: PD(4)=6(RR)|skip(4)=0 = 0x60 [GSM 24.008 Table 11.2]
    // Byte 1: MTI = 0x3B (AdditionalAssignment) [3GPP TS 44.018 Table 10.4.1]
    // Bytes 2-4: AdditionalChanDesc: typeAndOffset(5), TN(3), TSC(3), h(1), spare(2), ARFCN(10)
    //   TDMA_TCHF, TN=2, TSC=5, h=0, ARFCN=150 [GSM 24.008 10.5.2.5]
    uint8_t data[] = {0x60, 0x3b, 0x12, 0xA0, 0x56};
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->MTI(), L3RRMessage::AdditionalAssignment);
}

// =====================================================================
// RR ROUNDTrip: AssignmentCommand (GSM 04.08 9.1.2)
// Reference: L3_Templates.ttcn tr_RR_AssignmentCommand
// =====================================================================

TEST(GoldenRR, AssignmentCommand_RoundTrip) {
    L3AssignmentCommand msg;
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3RRMessage::AssignmentCommand);
}

// =====================================================================
// RR ROUNDTrip: ImmediateAssignment (GSM 04.08 9.1.19)
// Reference: GSM_RR_Types.ttcn ImmediateAssignment
// =====================================================================

TEST(GoldenRR, ImmediateAssignment_RoundTrip) {
    L3ImmediateAssignment msg;
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3RRMessage::ImmediateAssignment);
}

// =====================================================================
// RR ROUNDTrip: ImmediateAssignmentExtended (GSM 04.08 9.1.18)
// Reference: GSM_RR_Types.ttcn IMMEDIATE_ASSIGNMENT_EXTENDED
// =====================================================================

TEST(GoldenRR, ImmediateAssignmentExtended_RoundTrip) {
    L3ImmediateAssignmentExtended msg;
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3RRMessage::ImmediateAssignmentExtended);
    EXPECT_FALSE(msg.hasAdditionalChannel());
}

// =====================================================================
// RR ROUNDTrip: ImmediateAssignmentReject (GSM 04.08 9.1.20)
// Reference: GSM_RR_Types.ttcn IMMEDIATE_ASSIGNMENT_REJECT
// =====================================================================

TEST(GoldenRR, ImmediateAssignmentReject_RoundTrip) {
    L3ImmediateAssignmentReject msg(30);
    EXPECT_EQ(msg.waitTime(), 30u);
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3RRMessage::ImmediateAssignmentReject);
}

// =====================================================================
// RR ROUNDTrip: AdditionalAssignment (GSM 04.08 9.1.1)
// Reference: GSM_RR_Types.ttcn ADDITIONAL_ASSIGNMENT
// =====================================================================

TEST(GoldenRR, AdditionalAssignment_RoundTrip) {
    L3AdditionalAssignment msg;
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3RRMessage::AdditionalAssignment);
}

// =====================================================================
// RR ROUNDTrip: ChannelModeModify (GSM 04.08 9.1.5)
// Reference: L3_Templates.ttcn tr_RRM_ModeModify
// =====================================================================

TEST(GoldenRR, ChannelModeModify_RoundTrip) {
    L3ChannelDescription chd(TDMA_TCHF, 1, 7, 100);
    L3ChannelMode mode(L3ChannelMode::SpeechV1);
    L3ChannelModeModify msg(chd, mode);
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3RRMessage::ChannelModeModify);
}

// =====================================================================
// RR ROUNDTrip: ChannelModeModifyAcknowledge (GSM 04.08 9.1.6)
// Reference: GSM_RR_Types.ttcn CHANNEL_MODE_MODIFY_ACKNOWLEDGE
// =====================================================================

TEST(GoldenRR, ChannelModeModifyAcknowledge_RoundTrip) {
    uint8_t data[] = {0x60, 0x17, 0x11, 0xE0, 0x64, 0x01};
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    auto* cma = dynamic_cast<L3ChannelModeModifyAcknowledge*>(msg.get());
    ASSERT_TRUE(cma);
    EXPECT_EQ(cma->description().typeAndOffset(), TDMA_TCHF);
    EXPECT_EQ(cma->mode().mode(), L3ChannelMode::SpeechV1);
    auto parsed = roundtrip(*cma);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3RRMessage::ChannelModeModifyAcknowledge);
}

// =====================================================================
// RR ROUNDTrip: MeasurementReport (GSM 04.08 9.1.21)
// Reference: L3_Templates.ttcn ts_MEAS_REP
// =====================================================================

TEST(GoldenRR, MeasurementReport_RoundTrip) {
    L3MeasurementReport msg;
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3RRMessage::MeasurementReport);
}

// =====================================================================
// RR ROUNDTrip: ClassmarkEnquiry (GSM 04.08 9.1.14)
// Reference: L3_Templates.ttcn tr_RRM_CM_ENQUIRY
// =====================================================================

TEST(GoldenRR, ClassmarkEnquiry_RoundTrip) {
    L3ClassmarkEnquiry msg;
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3RRMessage::ClassmarkEnquiry);
}

// =====================================================================
// RR ROUNDTrip: ClassmarkChange (GSM 04.08 9.1.11)
// Reference: L3_Templates.ttcn ts_RRM_CM_CHG
// =====================================================================

TEST(GoldenRR, ClassmarkChange_RoundTrip) {
    uint8_t data[] = {0x60, 0x16, 0x03, 0x20, 0x00, 0x80};
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    auto* cm = dynamic_cast<L3ClassmarkChange*>(msg.get());
    ASSERT_TRUE(cm);
    auto parsed = roundtrip(*cm);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3RRMessage::ClassmarkChange);
}

// =====================================================================
// RR ROUNDTrip: HandoverCommand (GSM 04.08 9.1.15)
// Reference: L3_Templates.ttcn ts_RR_HandoverCommand
// =====================================================================

TEST(GoldenRR, HandoverCommand_RoundTrip) {
    L3HandoverCommand msg;
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3RRMessage::HandoverCommand);
}

// =====================================================================
// RR ROUNDTrip: PagingResponse (GSM 04.08 9.1.25)
// Reference: L3_Templates.ttcn ts_PAG_RESP
// =====================================================================

TEST(GoldenRR, PagingResponse_RoundTrip) {
    L3PagingResponse msg;
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3RRMessage::PagingResponse);
}

// =====================================================================
// RR ROUNDTrip: PhysicalInformation (GSM 04.08 9.1.12)
// Reference: GSM_RR_Types.ttcn PHYSICAL_INFORMATION
// =====================================================================

TEST(GoldenRR, PhysicalInformation_RoundTrip) {
    L3PhysicalInformation msg;
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3RRMessage::PhysicalInformation);
}

// =====================================================================
// RR ROUNDTrip: RRStatus (GSM 04.08 9.1.29)
// Reference: L3_Templates.ttcn tr_RRM_RR_STATUS
// =====================================================================

TEST(GoldenRR, RRStatus_RoundTrip) {
    uint8_t data[] = {0x60, 0x12, 0x60};
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    auto* rs = dynamic_cast<L3RRStatus*>(msg.get());
    ASSERT_TRUE(rs);
    EXPECT_EQ(rs->cause(), RRCause::Invalid_Mandatory_Information);
    auto parsed = roundtrip(*rs);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3RRMessage::RRStatus);
}

// =====================================================================
// RR ROUNDTrip: AssignmentComplete (GSM 04.08 9.1.3)
// Reference: GSM_RR_Types.ttcn ASSIGNMENT_COMPLETE
// =====================================================================

TEST(GoldenRR, AssignmentComplete_RoundTrip) {
    uint8_t data[] = {0x60, 0x29, 0x00};
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    auto* ac = dynamic_cast<L3AssignmentComplete*>(msg.get());
    ASSERT_TRUE(ac);
    EXPECT_EQ(ac->cause(), RRCause::Normal_Event);
    auto parsed = roundtrip(*ac);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3RRMessage::AssignmentComplete);
}

// =====================================================================
// RR ROUNDTrip: AssignmentFailure (GSM 04.08 9.1.3)
// Reference: GSM_RR_Types.ttcn ASSIGNMENT_FAILURE
// =====================================================================

TEST(GoldenRR, AssignmentFailure_RoundTrip) {
    uint8_t data[] = {0x60, 0x2f, 0x09};
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    auto* af = dynamic_cast<L3AssignmentFailure*>(msg.get());
    ASSERT_TRUE(af);
    EXPECT_EQ(af->cause(), RRCause::Channel_Mode_Unacceptable);
    auto parsed = roundtrip(*af);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3RRMessage::AssignmentFailure);
}

// =====================================================================
// RR ROUNDTrip: HandoverComplete (GSM 04.08 9.1.16)
// Reference: GSM_RR_Types.ttcn HANDOVER_COMPLETE
// =====================================================================

TEST(GoldenRR, HandoverComplete_RoundTrip) {
    uint8_t data[] = {0x60, 0x2c, 0x00};
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    auto* hc = dynamic_cast<L3HandoverComplete*>(msg.get());
    ASSERT_TRUE(hc);
    EXPECT_EQ(hc->cause(), RRCause::Normal_Event);
    auto parsed = roundtrip(*hc);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3RRMessage::HandoverComplete);
}

// =====================================================================
// RR ROUNDTrip: HandoverFailure (GSM 04.08 9.1.17)
// Reference: GSM_RR_Types.ttcn HANDOVER_FAILURE
// =====================================================================

TEST(GoldenRR, HandoverFailure_RoundTrip) {
    uint8_t data[] = {0x60, 0x28, 0x08};
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    auto* hf = dynamic_cast<L3HandoverFailure*>(msg.get());
    ASSERT_TRUE(hf);
    EXPECT_EQ(hf->cause(), RRCause::Handover_Impossible);
    auto parsed = roundtrip(*hf);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3RRMessage::HandoverFailure);
}

// =====================================================================
// RR ROUNDTrip: GPRSSuspensionRequest (GSM 04.08 9.1.13b)
// Reference: GSM_RR_Types.ttcn GPRS_SUSPENSION_REQUEST
// =====================================================================

TEST(GoldenRR, GPRSSuspensionRequest_RoundTrip) {
    L3GPRSSuspensionRequest msg;
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3RRMessage::GPRSSuspensionRequest);
}

// =====================================================================
// RR ROUNDTrip: ApplicationInformation (GSM 04.08 9.1.53)
// Reference: L3_Templates.ttcn tr_RR_APP_INFO
// =====================================================================

TEST(GoldenRR, ApplicationInformation_RoundTrip) {
    BitVector data(16);
    size_t wp = 0;
    data.writeField(wp, 0xAB, 8);
    data.writeField(wp, 0xCD, 8);
    L3ApplicationInformation msg(data);
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3RRMessage::ApplicationInformation);
}

// =====================================================================
// RR ROUNDTrip: ChannelRequest (GSM 04.08 9.1.13)
// Reference: GSM_RR_Types.ttcn RrShortDisc
// =====================================================================

TEST(GoldenRR, ChannelRequest_RoundTrip) {
    L3ChannelRequest msg(0x42);
    EXPECT_EQ(msg.requestReference(), 0x42u);
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3RRMessage::ChannelRequest);
}

// =====================================================================
// RR ROUNDTrip: HandoverAccess (GSM 04.08 9.1.14a)
// Reference: GSM_RR_Types.ttcn RrShortDisc
// =====================================================================

TEST(GoldenRR, HandoverAccess_RoundTrip) {
    L3HandoverAccess msg(0x17);
    EXPECT_EQ(msg.handoverNumber(), 0x17u);
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3RRMessage::HandoverAccess);
}

// =====================================================================
// RR ROUNDTrip: SynchronizationChannelInformation (GSM 04.08 9.1.30)
// Reference: GSM_RR_Types.ttcn RrShortDisc
// =====================================================================

TEST(GoldenRR, SynchronizationChannelInformation_RoundTrip) {
    L3SynchronizationChannelInformation msg;
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3RRMessage::SynchronizationChannelInformation);
}

// =====================================================================
// RR: All SI message round-trips (GSM 04.08 9.1.31..9.1.43c)
// Reference: GSM_SystemInformation.ttcn
// =====================================================================

TEST(GoldenRR, SI1_RoundTrip) {
    L3SystemInformationType1 msg;
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3RRMessage::SystemInformationType1);
}

TEST(GoldenRR, SI2_RoundTrip) {
    L3SystemInformationType2 msg;
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3RRMessage::SystemInformationType2);
}

TEST(GoldenRR, SI2bis_RoundTrip) {
    L3SystemInformationType2bis msg;
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3RRMessage::SystemInformationType2bis);
}

TEST(GoldenRR, SI2ter_RoundTrip) {
    L3SystemInformationType2ter msg;
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3RRMessage::SystemInformationType2ter);
}

TEST(GoldenRR, SI3_RoundTrip) {
    L3SystemInformationType3 msg;
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3RRMessage::SystemInformationType3);
}

TEST(GoldenRR, SI4_RoundTrip) {
    L3SystemInformationType4 msg;
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3RRMessage::SystemInformationType4);
}

TEST(GoldenRR, SI5_RoundTrip) {
    L3SystemInformationType5 msg;
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3RRMessage::SystemInformationType5);
}

TEST(GoldenRR, SI5bis_RoundTrip) {
    L3SystemInformationType5bis msg;
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3RRMessage::SystemInformationType5bis);
}

TEST(GoldenRR, SI5ter_RoundTrip) {
    L3SystemInformationType5ter msg;
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3RRMessage::SystemInformationType5ter);
}

TEST(GoldenRR, SI6_RoundTrip) {
    L3SystemInformationType6 msg;
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3RRMessage::SystemInformationType6);
}

TEST(GoldenRR, SI7_RoundTrip) {
    L3SystemInformationType7 msg;
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3RRMessage::SystemInformationType7);
}

TEST(GoldenRR, SI8_RoundTrip) {
    L3SystemInformationType8 msg;
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3RRMessage::SystemInformationType8);
}

TEST(GoldenRR, SI9_RoundTrip) {
    L3SystemInformationType9 msg;
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3RRMessage::SystemInformationType9);
    EXPECT_EQ(msg.l2BodyLength(), 5u);
}

TEST(GoldenRR, SI13_RoundTrip) {
    L3SystemInformationType13 msg;
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3RRMessage::SystemInformationType13);
}

TEST(GoldenRR, SI16_RoundTrip) {
    L3SystemInformationType16 msg;
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3RRMessage::SystemInformationType16);
}

TEST(GoldenRR, SI17_RoundTrip) {
    L3SystemInformationType17 msg;
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3RRMessage::SystemInformationType17);
}

// =====================================================================
// RR Cause values (GSM 24.008 10.5.2.31 / GSM 04.08 10.5.2.31)
// Reference: GSM_RR_Types.ttcn RR_Cause enum (line 143):
//   GSM48_RR_CAUSE_NORMAL ('00'O = 0x00)
//   GSM48_RR_CAUSE_HNDOVER_IMP ('08'O = 0x08)
//   GSM48_RR_CAUSE_CHAN_MODE_UNACCT ('09'O = 0x09)
//   GSM48_RR_CAUSE_CALL_CLEARED ('41'O = 0x41)
//   GSM48_RR_CAUSE_PROT_ERROR_UNSPC ('6f'O = 0x6F)
// Spec-verified: All RR cause values per GSM 24.008 Table 10.5.2.31
// =====================================================================

TEST(GoldenRR, CauseValues) {
    // Spec-verified: GSM 24.008 Table 10.5.2.31 RR cause values
    EXPECT_EQ(static_cast<uint8_t>(RRCause::Normal_Event), 0x00);                      // Normal event
    EXPECT_EQ(static_cast<uint8_t>(RRCause::Unspecified), 0x01);                       // Abnormal, unspecified
    EXPECT_EQ(static_cast<uint8_t>(RRCause::Channel_Unacceptable), 0x02);              // Abnormal, unacceptable
    EXPECT_EQ(static_cast<uint8_t>(RRCause::Timer_Expired), 0x03);                     // Abnormal, timer expired
    EXPECT_EQ(static_cast<uint8_t>(RRCause::No_Activity_On_The_Radio), 0x04);          // Abnormal, no activity
    EXPECT_EQ(static_cast<uint8_t>(RRCause::Preemptive_Release), 0x05);                // Preemptive release
    EXPECT_EQ(static_cast<uint8_t>(RRCause::Handover_Impossible), 0x08);               // Handover impossible
    EXPECT_EQ(static_cast<uint8_t>(RRCause::Channel_Mode_Unacceptable), 0x09);         // Channel mode unacceptable
    EXPECT_EQ(static_cast<uint8_t>(RRCause::Frequency_Not_Implemented), 0x0a);         // Frequency not implemented
    EXPECT_EQ(static_cast<uint8_t>(RRCause::Call_Already_Cleared), 0x41);              // Call already cleared
    EXPECT_EQ(static_cast<uint8_t>(RRCause::Semantically_Incorrect_Message), 0x5f);    // Semantically incorrect message
    EXPECT_EQ(static_cast<uint8_t>(RRCause::Invalid_Mandatory_Information), 0x60);     // Invalid mandatory information
    EXPECT_EQ(static_cast<uint8_t>(RRCause::Message_Type_Invalid), 0x61);              // Message type invalid
    EXPECT_EQ(static_cast<uint8_t>(RRCause::Message_Type_Not_Compatible), 0x62);       // Message type not compatible
    EXPECT_EQ(static_cast<uint8_t>(RRCause::Conditional_IE_Error), 0x64);              // Conditional IE error
    EXPECT_EQ(static_cast<uint8_t>(RRCause::No_Cell_Available), 0x65);                 // No cell available
    EXPECT_EQ(static_cast<uint8_t>(RRCause::Protocol_Error_Unspecified), 0x6f);        // Protocol error, unspecified
}

// =====================================================================
// RR IE: PowerCommand encoding (GSM 04.08 10.5.2.28)
// Reference: BTS_Tests.ttcn ts_PowerCmd
// 8 bits: power_command(5) | spare(3)
// =====================================================================

TEST(GoldenRR, PowerCommand_Encoding) {
    L3PowerCommand pc(15);
    EXPECT_EQ(pc.command(), 15u);
    EXPECT_EQ(pc.lengthV(), 1u);
    L3Frame frame(Primitive::L3_DATA, 16);
    size_t wp = 0;
    pc.writeV(frame, wp);
    EXPECT_EQ(frame.data()[0], 0xF0); // 15 << 3 (MSB first, 5 bits)
}

// =====================================================================
// RR IE: TimingAdvance encoding (GSM 04.08 10.5.2.40)
// Reference: GSM_RR_Types.ttcn TimingAdvance
// 8 bits: timing_advance(6) | spare(2)
// =====================================================================

TEST(GoldenRR, TimingAdvance_Encoding) {
    L3TimingAdvance ta(42);
    EXPECT_EQ(ta.timingAdvance(), 42u);
    EXPECT_EQ(ta.lengthV(), 1u);
    L3Frame frame(Primitive::L3_DATA, 16);
    size_t wp = 0;
    ta.writeV(frame, wp);
    EXPECT_EQ(frame.data()[0], 0xA0); // 42 << 2 (6 bits)
}

// =====================================================================
// RR IE: ChannelMode encoding (GSM 04.08 10.5.2.6)
// Reference: L3_Templates.ttcn ts_ChanMode
// 4 bits: speech_version(2) | signalling(1) | data(1)
// =====================================================================

TEST(GoldenRR, ChannelMode_Encoding) {
    L3ChannelMode mode1(L3ChannelMode::SpeechV1);
    L3Frame frame(Primitive::L3_DATA, 16);
    size_t wp = 0;
    mode1.writeV(frame, wp);
    EXPECT_EQ(frame.data()[0] & 0x0F, 0x01);

    L3ChannelMode mode2(L3ChannelMode::SpeechV2);
    frame = L3Frame(Primitive::L3_DATA, 16);
    wp = 0;
    mode2.writeV(frame, wp);
    EXPECT_EQ(frame.data()[0] & 0x0F, 0x02);

    L3ChannelMode mode3(L3ChannelMode::SpeechV3);
    EXPECT_TRUE(mode3.isAMR());
}

// =====================================================================
// RR IE: FrequencyList encoding (GSM 04.08 10.5.2.13)
// Reference: GSM_SystemInformation.ttcn BCCHFrequencyList
// 16 bytes, variable bitmap format
// =====================================================================

TEST(GoldenRR, FrequencyList_Empty) {
    L3FrequencyList fl;
    EXPECT_EQ(fl.lengthV(), 16u);
    EXPECT_TRUE(fl.ARFCNs().empty());
    L3Frame frame(Primitive::L3_DATA, 128);
    size_t wp = 0;
    fl.writeV(frame, wp);
    for (int i = 0; i < 16; i++) {
        EXPECT_EQ(frame.data()[i], 0x00);
    }
}

TEST(GoldenRR, FrequencyList_SingleARFCN) {
    std::vector<unsigned> arfcns = {100};
    L3FrequencyList fl(arfcns);
    L3Frame frame(Primitive::L3_DATA, 128);
    size_t wp = 0;
    fl.writeV(frame, wp);
    L3FrequencyList parsed;
    size_t rp = 0;
    parsed.parseV(frame, rp);
    EXPECT_EQ(parsed.ARFCNs(), arfcns);
}

// =====================================================================
// RR IE: MeasurementResults bit layout (GSM 04.08 10.5.2.20)
// Reference: GSM_RR_Types.ttcn MeasurementResults
// 128 bits: ba_used(1) + dtx_used(1) + rxlev_full(6) + ...
// =====================================================================

TEST(GoldenRR, MeasurementResults_Zero) {
    L3MeasurementResults mr;
    EXPECT_EQ(mr.lengthV(), 16u);
    L3Frame frame(Primitive::L3_DATA, 128);
    size_t wp = 0;
    mr.writeV(frame, wp);
    for (int i = 0; i < 16; i++) {
        EXPECT_EQ(frame.data()[i], 0x00);
    }
}

// =====================================================================
// RR IE: Classmark1 bit layout (GSM 04.08 10.5.1.5)
// Reference: L3_Templates.ttcn ts_CM1
// 8 bits: revision(1)|spare(1)|ES_IND(1)|A5_1(1)|RF_Power(2)|spare(2)
// =====================================================================

TEST(GoldenRR, Classmark1_Zero) {
    L3MobileStationClassmark1 cm1;
    L3Frame frame(Primitive::L3_DATA, 16);
    size_t wp = 0;
    cm1.writeV(frame, wp);
    EXPECT_EQ(frame.data()[0], 0x00);
}

// =====================================================================
// RR IE: Classmark2 bit layout (GSM 04.08 10.5.1.6)
// Reference: L3_Templates.ttcn ts_CM2
// 24 bits: revision(1)|spare(1)|ES_IND(1)|A5_1(1)|A5_3(1)|A5_2(1)|
//   RF_Power(2)|PS(1)|SS(1)|SM(1)|VBS(1)|VGCS(1)|FC(1)|CM3(1)|
//   LCS(1)|SoLSA(1)|CMSF(1)|spare(1)|PS_class(8)
// =====================================================================

TEST(GoldenRR, Classmark2_Zero) {
    L3MobileStationClassmark2 cm2;
    L3Frame frame(Primitive::L3_DATA, 32);
    size_t wp = 0;
    cm2.writeV(frame, wp);
    EXPECT_EQ(frame.data()[0], 0x00);
    EXPECT_EQ(frame.data()[1], 0x00);
    EXPECT_EQ(frame.data()[2], 0x00);
    EXPECT_EQ(cm2.powerClass(), 1);
    EXPECT_EQ(cm2.getA5Bits(), 0);
}

// =====================================================================
// RR IE: NCCPermitted encoding (GSM 04.08 10.5.2.27)
// Reference: GSM_SystemInformation.ttcn NCCPermitted
// 8 bits: ncc_permitted(8) - bitmask
// =====================================================================

TEST(GoldenRR, NCCPermitted_Encoding) {
    L3NCCPermitted ncc;
    EXPECT_EQ(ncc.permitted(), 0xFFu);
    L3NCCPermitted ncc2(0x7F);
    EXPECT_EQ(ncc2.permitted(), 0x7Fu);
}

// =====================================================================
// RR IE: PageMode encoding (GSM 04.08 10.5.2.26)
// Reference: GSM_RR_Types.ttcn PageMode
// 2 bits: Normal(0), Extended(1), Reorganization(2), SameAsBefore(3)
// =====================================================================

TEST(GoldenRR, PageMode_Values) {
    EXPECT_EQ(L3PageMode(0).lengthV(), 0u);
    EXPECT_EQ(L3PageMode(1).lengthV(), 0u);
    EXPECT_EQ(L3PageMode(2).lengthV(), 0u);
    EXPECT_EQ(L3PageMode(3).lengthV(), 0u);
}

// =====================================================================
// RR IE: HandoverReference encoding (GSM 04.08 10.5.2.15)
// Reference: GSM_RR_Types.ttcn HandoverReference
// 8 bits: handover_reference(5) | spare(3)
// =====================================================================

TEST(GoldenRR, HandoverReference_Encoding) {
    L3HandoverReference hr(0x17);
    EXPECT_EQ(hr.value(), 0x17u);
    EXPECT_EQ(hr.lengthV(), 1u);
}

// =====================================================================
// RR IE: CipheringModeSetting (GSM 04.08 10.5.2.9)
// Reference: L3_Templates.ttcn ts_CiphModeSetting
// 4 bits: ciphering(1) | algorithm(3)
// =====================================================================

TEST(GoldenRR, CipheringModeSetting_A5_3) {
    L3CipheringModeSetting cms(true, 3);
    L3Frame frame(Primitive::L3_DATA, 16);
    size_t wp = 0;
    cms.writeV(frame, wp);
    EXPECT_EQ(frame.data()[0] & 0x0F, 0x07);
}

// =====================================================================
// RR IE: SynchronizationIndication (GSM 04.08 10.5.2.39)
// Reference: GSM_RR_Types.ttcn SynchronizationIndication
// 8 bits: NCI(1) | ROT(1) | SI(6)
// =====================================================================

TEST(GoldenRR, SynchronizationIndication_Encoding) {
    L3SynchronizationIndication si(true, true, 3);
    EXPECT_TRUE(si.NCI());
    EXPECT_TRUE(si.ROT());
    EXPECT_EQ(si.SI(), 3);
    EXPECT_EQ(si.lengthV(), 1u);
}

// =====================================================================
// RR IE: CellOptionsBCCH (GSM 04.08 10.5.2.3)
// Reference: GSM_SystemInformation.ttcn CellOptions
// =====================================================================

TEST(GoldenRR, CellOptionsBCCH_Default) {
    L3CellOptionsBCCH co;
    EXPECT_EQ(co.lengthV(), 1u);
}

// =====================================================================
// RR IE: CellOptionsSACCH (GSM 04.08 10.5.2.3a)
// Reference: GSM_SystemInformation.ttcn CellOptionsSacch
// =====================================================================

TEST(GoldenRR, CellOptionsSACCH_Default) {
    L3CellOptionsSACCH co;
    EXPECT_EQ(co.lengthV(), 1u);
}

// =====================================================================
// RR IE: FollowOnProceed (GSM 04.08 10.5.2.38)
// Reference: GSM_RR_Types.ttcn FollowOnProceed
// =====================================================================

TEST(GoldenRR, FollowOnProceed_Encoding) {
    L3FollowOnProceed fop;
    EXPECT_EQ(fop.lengthV(), 1u);
    L3Frame frame(Primitive::L3_DATA, 16);
    size_t wp = 0;
    fop.writeV(frame, wp);
}

// =====================================================================
// RR IE: WaitIndication (GSM 04.08 10.5.2.43)
// Reference: GSM_RR_Types.ttcn WaitIndication
// =====================================================================

TEST(GoldenRR, WaitIndication_Encoding) {
    L3WaitIndication wi(60);
    EXPECT_EQ(wi.lengthV(), 1u);
}

// =====================================================================
// RR IE: MultiRateConfiguration (3GPP 44.018 10.5.2.21aa)
// Reference: BTS_Tests.ttcn ts_MultiRate
// =====================================================================

TEST(GoldenRR, MultiRateConfiguration_FR) {
    L3MultiRateConfiguration mrc(false);
    EXPECT_EQ(mrc.lengthV(), 2u);
}

TEST(GoldenRR, MultiRateConfiguration_HR) {
    L3MultiRateConfiguration mrc(true);
    EXPECT_EQ(mrc.lengthV(), 2u);
}

// =====================================================================
// RR IE: DedicatedModeOrTBF (GSM 04.08 10.5.2.25b)
// Reference: GSM_RR_Types.ttcn DedicatedModeOrTBF
// =====================================================================

TEST(GoldenRR, DedicatedModeOrTBF_Dedicated) {
    L3DedicatedModeOrTBF dmt(false, false);
    EXPECT_EQ(dmt.lengthV(), 0u);
    EXPECT_FALSE(dmt.isTBF());
    EXPECT_FALSE(dmt.isDownlink());
}

TEST(GoldenRR, DedicatedModeOrTBF_TBF) {
    L3DedicatedModeOrTBF dmt(true, true);
    EXPECT_TRUE(dmt.isTBF());
    EXPECT_TRUE(dmt.isDownlink());
}

// =====================================================================
// RR IE: APDUID (GSM 04.08 10.5.2.48)
// =====================================================================

TEST(GoldenRR, APDUID_Values) {
    L3APDUID apdu;
    EXPECT_EQ(apdu.lengthV(), 0u);
    L3APDUID apdu2(3);
    EXPECT_EQ(apdu2.lengthV(), 0u);
}

// =====================================================================
// RR IE: APDUFlags (GSM 04.08 10.5.2.49)
// =====================================================================

TEST(GoldenRR, APDUFlags_Values) {
    L3APDUFlags flags;
    EXPECT_EQ(flags.lengthV(), 0u);
    L3APDUFlags flags2(1, 1, 1);
    EXPECT_EQ(flags2.lengthV(), 0u);
}

// =====================================================================
// RR IE: APDUData (GSM 04.08 10.5.2.50)
// =====================================================================

TEST(GoldenRR, APDUData_WithData) {
    BitVector data(16);
    size_t wp = 0;
    data.writeField(wp, 0xAB, 8);
    data.writeField(wp, 0xCD, 8);
    L3APDUData apdu(data);
    EXPECT_EQ(apdu.lengthV(), 2u);
}

// =====================================================================
// RR IE: CellDescription (GSM 04.08 10.5.2.2)
// Reference: GSM_RR_Types.ttcn CellDescriptionV
// LSB first: bcc(3), ncc(3), arfcn(10)
// =====================================================================

TEST(GoldenRR, CellDescription_RoundTrip) {
    L3CellDescription orig(100, 5, 3);
    EXPECT_EQ(orig.ARFCN(), 100u);
    EXPECT_EQ(orig.NCC(), 5u);
    EXPECT_EQ(orig.BCC(), 3u);
    L3Frame frame(Primitive::L3_DATA, 32);
    size_t wp = 0;
    orig.writeV(frame, wp);
    L3CellDescription parsed;
    size_t rp = 0;
    parsed.parseV(frame, rp);
    EXPECT_EQ(parsed.ARFCN(), orig.ARFCN());
    EXPECT_EQ(parsed.NCC(), orig.NCC());
    EXPECT_EQ(parsed.BCC(), orig.BCC());
}

// =====================================================================
// RR IE: CellIdentity (GSM 04.08 10.5.1.1)
// Reference: GSM_SystemInformation.ttcn SysinfoCellIdentity
// =====================================================================

TEST(GoldenRR, CellIdentity_Encoding) {
    L3CellIdentity ci(0x1234);
    EXPECT_EQ(ci.ID(), 0x1234u);
    EXPECT_EQ(ci.lengthV(), 2u);
    L3Frame frame(Primitive::L3_DATA, 16);
    size_t wp = 0;
    ci.writeV(frame, wp);
    EXPECT_EQ(frame.data()[0], 0x12);
    EXPECT_EQ(frame.data()[1], 0x34);
}

// =====================================================================
// RR IE: RequestReference (GSM 04.08 10.5.2.30)
// Reference: GSM_RR_Types.ttcn RequestReference, f_compute_ReqRef
// =====================================================================

TEST(GoldenRR, RequestReference_RoundTrip) {
    L3RequestReference orig(0xAB, 5, 12, 20);
    L3Frame frame(Primitive::L3_DATA, 32);
    size_t wp = 0;
    orig.writeV(frame, wp);
    L3RequestReference parsed;
    size_t rp = 0;
    parsed.parseV(frame, rp);
    EXPECT_EQ(parsed.RA(), orig.RA());
    EXPECT_EQ(parsed.T1p(), orig.T1p());
    EXPECT_EQ(parsed.T2(), orig.T2());
    EXPECT_EQ(parsed.T3(), orig.T3());
}

// =====================================================================
// RR IE: ChannelDescription (GSM 04.08 10.5.2.5)
// Reference: GSM_RR_Types.ttcn ChannelDescription, ts_ChanDescH0
// =====================================================================

TEST(GoldenRR, ChannelDescription_SDCCH) {
    L3ChannelDescription orig(TDMA_SDCCH, 2, 7, 100);
    EXPECT_TRUE(orig.initialized());
    EXPECT_EQ(orig.typeAndOffset(), TDMA_SDCCH);
    EXPECT_EQ(orig.TN(), 2u);
    EXPECT_EQ(orig.TSC(), 7u);
    EXPECT_EQ(orig.ARFCN(), 100u);
    L3Frame frame(Primitive::L3_DATA, 32);
    size_t wp = 0;
    orig.writeV(frame, wp);
    L3ChannelDescription parsed;
    size_t rp = 0;
    parsed.parseV(frame, rp);
    EXPECT_EQ(parsed.typeAndOffset(), orig.typeAndOffset());
    EXPECT_EQ(parsed.TN(), orig.TN());
    EXPECT_EQ(parsed.TSC(), orig.TSC());
    EXPECT_EQ(parsed.ARFCN(), orig.ARFCN());
}

// =====================================================================
// RR IE: AdditionalChannelDescription
// =====================================================================

TEST(GoldenRR, AdditionalChannelDescription_RoundTrip) {
    L3AdditionalChannelDescription orig(TDMA_TCHF, 3, 5, 150);
    L3Frame frame(Primitive::L3_DATA, 32);
    size_t wp = 0;
    orig.writeV(frame, wp);
    L3AdditionalChannelDescription parsed;
    size_t rp = 0;
    parsed.parseV(frame, rp);
    EXPECT_EQ(parsed.typeAndOffset(), orig.typeAndOffset());
    EXPECT_EQ(parsed.TN(), orig.TN());
    EXPECT_EQ(parsed.TSC(), orig.TSC());
    EXPECT_EQ(parsed.ARFCN(), orig.ARFCN());
}

// =====================================================================
// RR IE: PowerCommandAndAccessType
// =====================================================================

TEST(GoldenRR, PowerCommandAndAccessType_RoundTrip) {
    L3PowerCommandAndAccessType orig(15);
    L3Frame frame(Primitive::L3_DATA, 16);
    size_t wp = 0;
    orig.writeV(frame, wp);
    L3PowerCommandAndAccessType parsed;
    size_t rp = 0;
    parsed.parseV(frame, rp);
    EXPECT_EQ(parsed.command(), orig.command());
}

// =====================================================================
// RR IE: CellChannelDescription (GSM 04.08 10.5.2.1b)
// =====================================================================

TEST(GoldenRR, CellChannelDescription_RoundTrip) {
    L3CellChannelDescription orig(100, 0x1F, 1);
    L3Frame frame(Primitive::L3_DATA, 32);
    size_t wp = 0;
    orig.writeV(frame, wp);
    L3CellChannelDescription parsed;
    size_t rp = 0;
    parsed.parseV(frame, rp);
    EXPECT_EQ(parsed.ARFCN(), orig.ARFCN());
    EXPECT_EQ(parsed.BSIC(), orig.BSIC());
    EXPECT_EQ(parsed.channelSpacing(), orig.channelSpacing());
}

// =====================================================================
// RR IE: NeighborCellsDescription
// =====================================================================

TEST(GoldenRR, NeighborCellsDescription_RoundTrip) {
    L3NeighborCellsDescription orig;
    EXPECT_EQ(orig.lengthV(), 16u);
    L3Frame frame(Primitive::L3_DATA, 128);
    size_t wp = 0;
    orig.writeV(frame, wp);
    L3NeighborCellsDescription parsed;
    size_t rp = 0;
    parsed.parseV(frame, rp);
}

// =====================================================================
// RR IE: BCCHFrequencyList
// =====================================================================

TEST(GoldenRR, BCCHFrequencyList_RoundTrip) {
    std::vector<unsigned> arfcns = {50, 100, 150};
    L3BCCHFrequencyList orig(arfcns);
    L3Frame frame(Primitive::L3_DATA, 128);
    size_t wp = 0;
    orig.writeV(frame, wp);
    L3BCCHFrequencyList parsed;
    size_t rp = 0;
    parsed.parseV(frame, rp);
}

// =====================================================================
// RR IE: RACHControlParameters (GSM 04.08 10.5.2.29)
// Reference: BTS_Tests.ttcn ts_RachCtrl_default
// =====================================================================

TEST(GoldenRR, RACHControlParameters_RoundTrip) {
    L3RACHControlParameters orig;
    EXPECT_EQ(orig.lengthV(), 3u);
    L3Frame frame(Primitive::L3_DATA, 24);
    size_t wp = 0;
    orig.writeV(frame, wp);
    L3RACHControlParameters parsed;
    size_t rp = 0;
    parsed.parseV(frame, rp);
    EXPECT_EQ(parsed.MaxRetrans(), orig.MaxRetrans());
    EXPECT_EQ(parsed.TxInteger(), orig.TxInteger());
}

// =====================================================================
// RR IE: CellSelectionParameters (GSM 04.08 10.5.2.4)
// Reference: BTS_Tests.ttcn ts_CellSelPar_default
// =====================================================================

TEST(GoldenRR, CellSelectionParameters_RoundTrip) {
    L3CellSelectionParameters orig;
    EXPECT_EQ(orig.lengthV(), 2u);
    L3Frame frame(Primitive::L3_DATA, 16);
    size_t wp = 0;
    orig.writeV(frame, wp);
    L3CellSelectionParameters parsed;
    size_t rp = 0;
    parsed.parseV(frame, rp);
}

// =====================================================================
// RR IE: ControlChannelDescription (GSM 04.08 10.5.2.11)
// Reference: GSM_SystemInformation.ttcn ControlChannelDescription
// =====================================================================

TEST(GoldenRR, ControlChannelDescription_RoundTrip) {
    L3ControlChannelDescription orig;
    EXPECT_EQ(orig.lengthV(), 3u);
    L3Frame frame(Primitive::L3_DATA, 24);
    size_t wp = 0;
    orig.writeV(frame, wp);
    L3ControlChannelDescription parsed;
    size_t rp = 0;
    parsed.parseV(frame, rp);
}

// =====================================================================
// RR IE: SI3RestOctets (GSM 04.08 10.5.2.34)
// Reference: GSM_RestOctets.ttcn SI3RestOctets
// =====================================================================

TEST(GoldenRR, SI3RestOctets_Default) {
    L3SI3RestOctets ro;
    EXPECT_FALSE(ro.hasSI3RestOctets());
    EXPECT_FALSE(ro.hasGPRS());
}

// =====================================================================
// RR IE: SI13RestOctets (GSM 04.08 10.5.2.37b)
// Reference: GSM_RestOctets.ttcn SI13RestOctets
// =====================================================================

TEST(GoldenRR, SI13RestOctets_Default) {
    L3SI13RestOctets ro;
}

// =====================================================================
// RR IE: GPRSCellOptions
// =====================================================================

TEST(GoldenRR, GPRSCellOptions_Default) {
    L3GPRSCellOptions co;
}

// =====================================================================
// RR IE: GPRSSI13PowerControlParameters
// =====================================================================

TEST(GoldenRR, GPRSSI13PowerControlParameters_Default) {
    L3GPRSSI13PowerControlParameters pc;
}

// =====================================================================
// RR IE: IARestOctets
// =====================================================================

TEST(GoldenRR, IARestOctets_Default) {
    L3IARestOctets ro;
}

// =====================================================================
// RR IE: SIType4RestOctets
// =====================================================================

TEST(GoldenRR, SIType4RestOctets_Default) {
    L3SIType4RestOctets ro;
}

// =====================================================================
// RR IE: RRCauseElement (GSM 04.08 10.5.2.31)
// =====================================================================

TEST(GoldenRR, RRCauseElement_Normal) {
    L3RRCauseElement orig(RRCause::Normal_Event);
    EXPECT_EQ(orig.lengthV(), 1u);
    L3Frame frame(Primitive::L3_DATA, 16);
    size_t wp = 0;
    orig.writeV(frame, wp);
    L3RRCauseElement parsed;
    size_t rp = 0;
    parsed.parseV(frame, rp);
    EXPECT_EQ(parsed.causeValue(), RRCause::Normal_Event);
}

TEST(GoldenRR, RRCauseElement_HandoverImpossible) {
    L3RRCauseElement orig(RRCause::Handover_Impossible);
    L3Frame frame(Primitive::L3_DATA, 16);
    size_t wp = 0;
    orig.writeV(frame, wp);
    L3RRCauseElement parsed;
    size_t rp = 0;
    parsed.parseV(frame, rp);
    EXPECT_EQ(parsed.causeValue(), RRCause::Handover_Impossible);
}

TEST(GoldenRR, RRCauseElement_ProtocolError) {
    L3RRCauseElement orig(RRCause::Protocol_Error_Unspecified);
    L3Frame frame(Primitive::L3_DATA, 16);
    size_t wp = 0;
    orig.writeV(frame, wp);
    L3RRCauseElement parsed;
    size_t rp = 0;
    parsed.parseV(frame, rp);
    EXPECT_EQ(parsed.causeValue(), RRCause::Protocol_Error_Unspecified);
}

// =====================================================================
// RR IE: CipheringModeResponse (GSM 04.08 10.5.2.10)
// Reference: L3_Templates.ttcn ts_CiphModeResp
// =====================================================================

TEST(GoldenRR, CipheringModeResponse_Default) {
    L3CipheringModeResponse cmr;
    EXPECT_EQ(cmr.lengthV(), 0u);
    EXPECT_FALSE(cmr.includeIMEISV());
}

// =====================================================================
// RR IE: ChannelDescription2
// =====================================================================

TEST(GoldenRR, ChannelDescription2_FromChannelDescription) {
    L3ChannelDescription orig(TDMA_TCHF, 3, 7, 100);
    L3ChannelDescription2 chd2(orig);
    EXPECT_EQ(chd2.typeAndOffset(), TDMA_TCHF);
    EXPECT_EQ(chd2.TN(), 3u);
    EXPECT_EQ(chd2.TSC(), 7u);
    EXPECT_EQ(chd2.ARFCN(), 100u);
}

// =====================================================================
// RR IE: ImmediateAssignmentInformation
// =====================================================================

TEST(GoldenRR, ImmediateAssignmentInformation_Default) {
    L3ImmediateAssignmentInformation iai;
    EXPECT_EQ(iai.PowerOffset(), 0u);
}

// =====================================================================
// RR IE: RestOctets base
// =====================================================================

TEST(GoldenRR, RestOctets_Base) {
    L3RestOctets ro;
    EXPECT_EQ(ro.lengthV(), 0u);
}

// =====================================================================
// RR IE: OctetAlignedProtocolElement
// =====================================================================

TEST(GoldenRR, OctetAlignedProtocolElement_Encoding) {
    L3OctetAlignedProtocolElement oe(std::string("\xAB\xCD\xEF", 3));
    EXPECT_EQ(oe.lengthV(), 3u);
    EXPECT_TRUE(oe.mExtant);
}

// =====================================================================
// RR IE: CipheringKeySeqNr (GSM 04.08 10.5.1.2)
// Reference: L3_Templates.ttcn ts_CKSN
// =====================================================================

TEST(GoldenRR, CipheringKeySeqNr_RoundTrip) {
    L3CipheringKeySequenceNumber orig(5);
    L3Frame frame(Primitive::L3_DATA, 16);
    size_t wp = 0;
    orig.writeV(frame, wp);
    L3CipheringKeySequenceNumber parsed;
    size_t rp = 0;
    parsed.parseV(frame, rp);
}

TEST(GoldenRR, CipheringKeySeqNr_MaxValue) {
    L3CipheringKeySequenceNumber orig(7);
    L3Frame frame(Primitive::L3_DATA, 16);
    size_t wp = 0;
    orig.writeV(frame, wp);
    L3CipheringKeySequenceNumber parsed;
    size_t rp = 0;
    parsed.parseV(frame, rp);
}
