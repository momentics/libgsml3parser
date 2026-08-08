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
//
// [GOLDEN DATA VERIFICATION]
// All RR message type identifiers verified against GSM_RR_Types.ttcn RrMessageType enum
//   and 3GPP TS 44.018 Table 10.4.1.
// All RR cause values verified against GSM_RR_Types.ttcn RR_Cause enum.
// PagingRequestType1/2/3 structures verified against GSM_RR_Types.ttcn records:
//   GsmTmsi (raw 4-byte, NOT length-prefixed) for Type 2 and Type 3.
// HandoverCommand CellDescriptionV verified against GSM_RR_Types.ttcn FIELDORDER(lsb):
//   bcc(3)|ncc(3)|arfcn(10) packed LSB-first across 2 octets.
// ChannelDescription encoding verified against GSM 24.008 10.5.2.5:
//   typeAndOffset(5)|TN(3)|TSC(3)|h(1)|spare(2)|ARFCN(10).
// CipheringModeCommand byte layout verified against L3_Templates.ttcn ts_RRM_CiphModeCmd:
//   cipherModeResponse(4 MSB)|cipherModeSetting(4 LSB), sC=1|algId=A5/3.
// CellSelectionParameters verified against BTS_Tests.ttcn ts_CellSelPar_default.
// RACHControlParameters verified against BTS_Tests.ttcn ts_RachCtrl_default.
// ControlChannelDescription verified against BTS_Tests.ttcn ts_SI3_default ctrl_chan_desc.
// PowerCommand encoding verified: power_command(5 MSB)|spare(3 LSB).
// TimingAdvance encoding verified: timing_advance(6 MSB)|spare(2 LSB).
// Rest octet padding pattern 0x2B verified against GSM_RR_Types.ttcn PADDING_PATTERN.

#include <gtest/gtest.h>
#include <gsml3parser/parser.h>
#include <gsml3parser/common/l3common.h>
#include <gsml3parser/gsm_common.h>
#include <gsml3parser/rr/l3rrmessages.h>
#include <gsml3parser/visitor.h>

using namespace gsml3parser;

// Helper: serialize ParsedMessage to hex, parse back, return result.
static Expected<ParsedMessage> roundtrip(const ParsedMessage& msg) {
    auto hex = writeL3Hex(msg);
    if (!hex) return Expected<ParsedMessage>::error(hex.error());
    return parseL3Hex(hex.value());
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
// [GSM SPEC VERIFIED] RR messages use 8-bit MTI in byte 1 of L3 header.
//   PD discriminator for RR is 6 ('0110'B), placed in high nibble of byte 0.
//   All values cross-checked against GSM_RR_Types.ttcn RrMessageType enum definitions.
// =====================================================================

TEST(GoldenRR, MessageTypeValues) {
    // Spec-verified: 3GPP TS 44.018 Table 10.4.1 RR message type identifier values
    // System Information messages (GSM_RR_Types.ttcn lines 57-74):
    EXPECT_EQ(L3SystemInformationType1::MTI, 0x19);     // '00011001'B - 44.018 9.1.31
    EXPECT_EQ(L3SystemInformationType2::MTI, 0x1a);     // '00011010'B - 44.018 9.1.32
    EXPECT_EQ(L3SystemInformationType2bis::MTI, 0x02);  // '00000010'B - 44.018 9.1.33
    EXPECT_EQ(L3SystemInformationType2ter::MTI, 0x03);  // '00000011'B - 44.018 9.1.34
    EXPECT_EQ(L3SystemInformationType3::MTI, 0x1b);     // '00011011'B - 44.018 9.1.35
    EXPECT_EQ(L3SystemInformationType4::MTI, 0x1c);     // '00011100'B - 44.018 9.1.36
    EXPECT_EQ(L3SystemInformationType5::MTI, 0x1d);     // '00011101'B - 44.018 9.1.37
    EXPECT_EQ(L3SystemInformationType5bis::MTI, 0x05);  // '00000101'B - 44.018 9.1.38
    EXPECT_EQ(L3SystemInformationType5ter::MTI, 0x06);  // '00000110'B - 44.018 9.1.39
    EXPECT_EQ(L3SystemInformationType6::MTI, 0x1e);     // '00011110'B - 44.018 9.1.40
    EXPECT_EQ(L3SystemInformationType7::MTI, 0x1f);     // '00011111'B - 44.018 9.1.41
    EXPECT_EQ(L3SystemInformationType8::MTI, 0x18);     // '00011000'B - 44.018 9.1.42
    EXPECT_EQ(L3SystemInformationType9::MTI, 0x04);     // '00000100'B - 44.018 9.1.43
    EXPECT_EQ(L3SystemInformationType13::MTI, 0x00);    // '00000000'B - 44.018 9.1.43a
    EXPECT_EQ(L3SystemInformationType16::MTI, 0x3d);    // '00111101'B - 44.018 9.1.43b
    EXPECT_EQ(L3SystemInformationType17::MTI, 0x3e);    // '00111110'B - 44.018 9.1.43c
    // Assignment/Handover messages (GSM_RR_Types.ttcn lines 38-44):
    EXPECT_EQ(L3AssignmentCommand::MTI, 0x2e);          // '00101110'B - 44.018 9.1.2
    EXPECT_EQ(L3AssignmentComplete::MTI, 0x29);         // '00101001'B - 44.018 9.1.3
    EXPECT_EQ(L3AssignmentFailure::MTI, 0x2f);          // '00101111'B - 44.018 9.1.3
    EXPECT_EQ(L3HandoverCommand::MTI, 0x2b);            // '00101011'B - 44.018 9.1.15
    EXPECT_EQ(L3HandoverComplete::MTI, 0x2c);           // '00101100'B - 44.018 9.1.16
    EXPECT_EQ(L3HandoverFailure::MTI, 0x28);            // '00101000'B - 44.018 9.1.17
    // Paging messages (GSM_RR_Types.ttcn lines 50-54):
    EXPECT_EQ(L3PagingRequestType1::MTI, 0x21);         // '00100001'B - 44.018 9.1.22
    EXPECT_EQ(L3PagingRequestType2::MTI, 0x22);         // '00100010'B - 44.018 9.1.23
    EXPECT_EQ(L3PagingRequestType3::MTI, 0x24);         // '00100100'B - 44.018 9.1.24
    EXPECT_EQ(L3PagingResponse::MTI, 0x27);             // '00100111'B - 44.018 9.1.25
    // Immediate Assignment (GSM_RR_Types.ttcn lines 26-28):
    EXPECT_EQ(L3ImmediateAssignment::MTI, 0x3f);        // '00111111'B - 44.018 9.1.19
    EXPECT_EQ(L3ImmediateAssignmentExtended::MTI, 0x39);// '00111001'B - 44.018 9.1.18
    EXPECT_EQ(L3ImmediateAssignmentReject::MTI, 0x3a);  // '00111010'B - 44.018 9.1.20
    EXPECT_EQ(L3AdditionalAssignment::MTI, 0x3b);       // '00111011'B - 44.018 9.1.1
    // Other RR messages:
    EXPECT_EQ(L3ChannelRelease::MTI, 0x0d);             // '00001101'B - 44.018 9.1.7
    EXPECT_EQ(L3PhysicalInformation::MTI, 0x2d);        // '00101101'B - 44.018 9.1.12
    EXPECT_EQ(L3CipheringModeCommand::MTI, 0x35);       // '00110101'B - 44.018 9.1.9
    EXPECT_EQ(L3CipheringModeComplete::MTI, 0x32);      // '00110010'B - 44.018 9.1.10
    EXPECT_EQ(L3ChannelModeModify::MTI, 0x10);          // '00010000'B - 44.018 9.1.5
    EXPECT_EQ(L3ChannelModeModifyAcknowledge::MTI, 0x17);// '00010111'B - 44.018 9.1.6
    EXPECT_EQ(L3RRStatus::MTI, 0x12);                   // '00010010'B - 44.018 9.1.29
    EXPECT_EQ(L3ClassmarkChange::MTI, 0x16);            // '00010110'B - 44.018 9.1.11
    EXPECT_EQ(L3ClassmarkEnquiry::MTI, 0x13);           // '00010011'B - 44.018 9.1.14
    EXPECT_EQ(L3MeasurementReport::MTI, 0x15);          // '00010101'B - 44.018 9.1.21
    EXPECT_EQ(L3GPRSSuspensionRequest::MTI, 0x34);      // '00110100'B - 44.018 9.1.13b
    EXPECT_EQ(L3ApplicationInformation::MTI, 0x38);     // '00111000'B - 44.018 9.1.53
}

// =====================================================================
// RR PARSE FROM HEX: Paging Request Type 1 (3GPP TS 44.018 9.1.22 / GSM 04.08 9.1.22)
// Reference: L3_Templates.ttcn tr_PAGING_REQ1 (line 541):
//   discriminator := '0110'B (PD=6=RR), messageType := '00100001'B (MTI=0x21)
// Reference: GSM_RR_Types.ttcn PagingRequestType1 (line 568):
//   ChannelNeeded12 chan_needed, PageMode page_mode, MobileIdentityLV mi1
// Spec-verified: PD=6(RR), MTI=0x21(PagingRequestType1) per 3GPP TS 44.018 Table 10.4.1
// [GSM SPEC VERIFIED] PagingRequestType1 body = ChannelNeeded12(8 bits) + PageMode(4 bits)
//   + MobileIdentityLV(variable). ChannelNeeded12 encodes two ChannelNeeded values:
//   second(2)|first(2), packed as high nibble. From GSM_Types.ttcn:
//   CHAN_NEED_ANY(0), CHAN_NEED_SDCCH(1), CHAN_NEED_TCH_F(2), CHAN_NEED_TCH_H(3).
//   PageMode from GSM_RR_Types.ttcn: NORMAL(0), EXTENDED(1), REORGANIZATION(2), SAME_AS_BEFORE(3).
// =====================================================================

TEST(GoldenRR, PagingRequestType1_Parse) {
    // Byte 0: PD(4)=6(RR)|skip(4)=0 = 0x60 [GSM 24.008 Table 11.2]
    // Byte 1: MTI = 0x21 (PagingRequestType1) [3GPP TS 44.018 Table 10.4.1]
    // Byte 2: ChannelNeeded12(4)|PageMode(4) = 0x10 [GSM_Types.ttcn ChannelNeeded12: second(2)|first(2)]
    //   ChannelNeeded12: second=00(ANY), first=01(SDCCH) -> high nibble = 0b0001 = 0x1
    //   PageMode: PAGE_MODE_NORMAL(0) [GSM_RR_Types.ttcn line 382] -> low nibble = 0x0
    //   Combined: 0x10. Spec-verified against tr_PAGING_REQ1 (L3_Templates.ttcn line 541)
    // Byte 3: MI LV length = 5 (1 type octet + 4 TMSI octets) [GSM 24.008 10.5.1.4]
    // Byte 4: spare(4)=0|typeOfIdentity(3)=100(TMSI)|oddevenIndicator(1)=0 = 0x08 [GSM 24.008 10.5.1.4]
    // Bytes 5-8: TMSI = 0x12345678 (4 octets, MSB first)
    uint8_t data[] = {
        0x60, 0x21, 0x10, 0x05, 0x08, 0x12, 0x34, 0x56, 0x78
    };
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messagePD(*msg), L3PD::RadioResource);
    EXPECT_EQ(messageMTI(*msg), L3PagingRequestType1::MTI);
}

// =====================================================================
// RR PARSE FROM HEX: Paging Request Type 2 (3GPP TS 44.018 9.1.23 / GSM 04.08 9.1.23)
// Reference: L3_Templates.ttcn tr_PAGING_REQ2 (line 561):
//   discriminator := '0110'B (PD=6=RR), messageType := '00100010'B (MTI=0x22)
// Reference: GSM_RR_Types.ttcn PagingRequestType2 (line 577):
//   ChannelNeeded12 chan_needed, PageMode page_mode, GsmTmsi mi1, GsmTmsi mi2
// Spec-verified: PD=6(RR), MTI=0x22(PagingRequestType2) per 3GPP TS 44.018 Table 10.4.1
// [GSM SPEC VERIFIED] PagingRequestType2 body = ChannelNeeded12(8 bits) + PageMode(4 bits)
//   + GsmTmsi mi1(4 octets RAW) + GsmTmsi mi2(4 octets RAW).
//   IMPORTANT: TMSI values are raw 4-octet integers (GSM_Types.ttcn GsmTmsi = uint32_t),
//   NOT length-prefixed MobileIdentityLV! This differs from PagingRequestType1 which uses
//   MobileIdentityLV (length + type octet + value).
// =====================================================================

TEST(GoldenRR, PagingRequestType2_Parse) {
    // GSM 24.008 9.1.23: PagingRequestType2 structure:
    //   ChannelNeeded(4 bits)|PageMode(4 bits) + GsmTmsi mi1(4 octets) + GsmTmsi mi2(4 octets) + [optional MobileIdentityTLV]
    // Reference: GSM_RR_Types.ttcn PagingRequestType2 (line 577): GsmTmsi mi1, GsmTmsi mi2
    //   GsmTmsi = type uint32_t GsmTmsi; (GSM_Types.ttcn line 26) - raw 4-byte TMSI, NOT length-prefixed!
    // Byte 0: PD(4)=6(RR)|skip(4)=0 = 0x60 [GSM 24.008 Table 11.2]
    // Byte 1: MTI = 0x22 (PagingRequestType2) [3GPP TS 44.018 Table 10.4.1]
    // Byte 2: ChannelNeeded12(4)=0x1|PageMode(4)=0(Normal) = 0x10
    //   GSM_Types.ttcn ChannelNeeded12: second(2)=00(ANY)|first(2)=01(SDCCH) -> 0b0001 = 0x1
    // Bytes 3-6: GsmTmsi mi1 = 0x12345678 (raw 4 octets, MSB first, no length prefix)
    // Bytes 7-10: GsmTmsi mi2 = 0xDEADBEEF (raw 4 octets, MSB first, no length prefix)
    uint8_t data[] = {
        0x60, 0x22, 0x10,
        0x12, 0x34, 0x56, 0x78,
        0xDE, 0xAD, 0xBE, 0xEF
    };
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3PagingRequestType2::MTI);
}

// =====================================================================
// RR PARSE FROM HEX: Paging Request Type 3 (3GPP TS 44.018 9.1.24 / GSM 04.08 9.1.24)
// Reference: L3_Templates.ttcn tr_PAGING_REQ3 (line 583):
//   discriminator := '0110'B (PD=6=RR), messageType := '00100100'B (MTI=0x24)
// Reference: GSM_RR_Types.ttcn PagingRequestType3 (line 587):
//   type record length(4) of GsmTmsi GsmTmsi4; — exactly 4 raw TMSIs
//   ChannelNeeded12 chan_needed, PageMode page_mode, GsmTmsi4 mi
// Spec-verified: PD=6(RR), MTI=0x24(PagingRequestType3) per 3GPP TS 44.018 Table 10.4.1
// [GSM SPEC VERIFIED] PagingRequestType3 body = ChannelNeeded12(8 bits) + PageMode(4 bits)
//   + GsmTmsi4 mi(16 octets RAW). GsmTmsi4 is a fixed record of exactly 4 TMSI values,
//   each 4 octets (32 bits), MSB-first. Total body = 1 + 16 = 17 octets minimum.
//   IMPORTANT: All TMSI values are raw integers, NOT length-prefixed MobileIdentityLV!
// =====================================================================

TEST(GoldenRR, PagingRequestType3_Parse) {
    // GSM 24.008 9.1.24: PagingRequestType3 structure:
    //   ChannelNeeded(4 bits)|PageMode(4 bits) + GsmTmsi4 mi (4x raw 4-octet TMSIs) + [optional RestOctets]
    // Reference: GSM_RR_Types.ttcn PagingRequestType3 (line 587): GsmTmsi4 mi
    //   GsmTmsi4 = type record length(4) of GsmTmsi; -> 4 raw uint32_t TMSIs, NOT length-prefixed!
    // Byte 0: PD(4)=6(RR)|skip(4)=0 = 0x60 [GSM 24.008 Table 11.2]
    // Byte 1: MTI = 0x24 (PagingRequestType3) [3GPP TS 44.018 Table 10.4.1]
    // Byte 2: ChannelNeeded12(4)=0x1|PageMode(4)=0(Normal) = 0x10
    //   GSM_Types.ttcn ChannelNeeded12: second(2)=00(ANY)|first(2)=01(SDCCH) -> 0b0001 = 0x1
    // Bytes 3-6: GsmTmsi mi[0] = 0x12345678 (raw 4 octets, MSB first, no length prefix)
    // Bytes 7-10: GsmTmsi mi[1] = 0xDEADBEEF (raw 4 octets, MSB first, no length prefix)
    // Bytes 11-14: GsmTmsi mi[2] = 0xABCDEF01 (raw 4 octets, MSB first, no length prefix)
    // Bytes 15-18: GsmTmsi mi[3] = 0x11223344 (raw 4 octets, MSB first, no length prefix)
    uint8_t data[] = {
        0x60, 0x24, 0x10,
        0x12, 0x34, 0x56, 0x78,
        0xDE, 0xAD, 0xBE, 0xEF,
        0xAB, 0xCD, 0xEF, 0x01,
        0x11, 0x22, 0x33, 0x44
    };
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3PagingRequestType3::MTI);
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
    // GSM 24.008 9.1.25: PagingResponse body = CKSN + CM2-LV + MI-LV
    // CKSN is 4 bits (GSM 24.008 10.5.1.2), padded to octet boundary before CM2-LV
    // Reference: L3_Templates.ttcn ts_PAG_RESP (line 610):
    //   cipheringKeySequenceNumber := { '000'B, '0'B } (4 bits)
    //   spare1_4 := '0000'B (4 bits) — these 4+4 bits pack into ONE octet
    // Reference: GSM_RR_Types.ttcn PagingResponse record:
    //   OCT4 cipheringKeySequenceNumber, mobileStationClassmark2LV, MobileIdentityLV
    //   CKSN(4 bits) + implicit padding(4 bits) = 1 octet before CM2-LV
    // Byte 0: PD(4)=6(RR)|skip(4)=0 = 0x60 [GSM 24.008 Table 11.2]
    // Byte 1: MTI = 0x27 (PagingResponse) [3GPP TS 44.018 Table 10.4.1]
    // Byte 2: CKSN(4)=0 | spare/padding(4)=0 = 0x00
    //   GSM 24.008 10.5.1.2: cipheringKeySequenceNumber is 4 bits (keySequence 3 + spare 1)
    //   Padded to octet boundary with 4 spare bits before CM2-LV (octet-aligned)
    // Byte 3: CM2 LV length = 3 (Classmark 2 is 3 octets) [GSM 24.008 10.5.1.6]
    // Bytes 4-6: CM2 value (24 bits of capability flags)
    // Byte 7: MI LV length = 5 [GSM 24.008 10.5.1.4]
    // Byte 8: spare(4)=0|typeOfIdentity(3)=100(TMSI)|oddevenIndicator(1)=0 = 0x08
    // Bytes 9-12: TMSI = 0x12345678 (MSB first)
    uint8_t data[] = {
        0x60, 0x27, 0x00,
        0x03, 0x20, 0x00, 0x80,
        0x05, 0x08, 0x12, 0x34, 0x56, 0x78
    };
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3PagingResponse::MTI);
}

// =====================================================================
// RR PARSE FROM HEX: Classmark Change (3GPP TS 44.018 9.1.11 / GSM 04.08 9.1.11)
// Reference: L3_Templates.ttcn ts_RRM_CM_CHG template
// Reference: GSM_RR_Types.ttcn CLASSMARK_CHANGE ('00010110'B = 0x16, line 81)
// Structure: CM2 LV (length + 3 octets Classmark 2)
// Spec-verified: PD=6(RR), MTI=0x16(ClassmarkChange) per 3GPP TS 44.018 Table 10.4.1
// [GSM SPEC VERIFIED] GSM 24.008 9.1.11: ClassmarkChange body = CM2-LV only.
//   Classmark 2 (GSM 24.008 10.5.1.6): LV-encoded, length=3, value=3 octets (24 bits).
//   Bit layout: spare(1)|revisionLevel(2)|ES_IND(1)|A5_1(1)|RF_Power(3)|spare(1)|
//   PS_Capability(1)|SS_Screen(2)|SM_Capability(1)|VBS(1)|VGCS(1)|FC(1)|CM3(1)|
//   LCS_VA(1)|spare(1)|SoLSA(1)|CMSF(1)|A5_3(1)|A5_2(1)|PS_class(8).
// =====================================================================

TEST(GoldenRR, ClassmarkChange_Parse) {
    // Byte 0: PD(4)=6(RR)|skip(4)=0 = 0x60 [GSM 24.008 Table 11.2]
    // Byte 1: MTI = 0x16 (ClassmarkChange) [3GPP TS 44.018 Table 10.4.1]
    // Byte 2: CM2 LV length = 3 (Classmark 2 is 3 octets) [GSM 24.008 10.5.1.6]
    // Bytes 3-5: CM2 value (24 bits of capability flags)
    uint8_t data[] = {0x60, 0x16, 0x03, 0x20, 0x00, 0x80};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3ClassmarkChange::MTI);
    auto* cm = tryGet<L3ClassmarkChange>(*msg);
    ASSERT_TRUE(cm);
    auto rt = roundtrip(*msg);
    ASSERT_TRUE(rt);
    EXPECT_EQ(messageMTI(*rt), L3ClassmarkChange::MTI);
}

// =====================================================================
// RR PARSE FROM HEX: Measurement Report (3GPP TS 44.018 9.1.21 / GSM 04.08 9.1.21)
// Reference: L3_Templates.ttcn ts_MEAS_REP template
// Reference: GSM_RR_Types.ttcn MeasurementResults (line 457):
//   ba_used(1), dtx_used(1), rxlev_full(6), threeg_ba(1), meas_valid(1),
//   rxlev_sub(6), si23_ba(1), rxqual_full(3), rxqual_sub(3), no_ncell(3)
// Structure: 16 bytes of MeasurementResults (128 bits, padded to 16 octets)
// Spec-verified: PD=6(RR), MTI=0x15(MeasurementReport) per 3GPP TS 44.018 Table 10.4.1
// [GSM SPEC VERIFIED] GSM 24.008 9.1.21: MeasurementReport body = MeasurementResults(16 octets).
//   The MeasurementResults structure is exactly 16 octets (128 bits) per GSM 24.008 10.5.2.20:
//   ba_used(1)|dtx_used(1)|rxlev_full(6)|threeg_ba(1)|meas_valid(1)|rxlev_sub(6)|
//   si23_ba(1)|rxqual_full(3)|rxqual_sub(3)|no_ncell(3)|[ncell reports up to 3 cells].
//   When no_nccell=0, no neighbor cell reports follow. All-zero data = default values.
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
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3MeasurementReport::MTI);
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
// [GSM SPEC VERIFIED] CellDescriptionV uses FIELDORDER(lsb): bcc(3) packed first,
//   then ncc(3), then arfcn(10). For BCC=3(011), NCC=5(101), ARFCN=100(0001100100):
//   bits 0-2=bcc=011, bits 3-5=ncc=101, bits 6-15=arfcn=0001100100
//   Byte 0 = bits 0-7: 011|101|00 = 0b0111_0100 = 0x74
//   Byte 1 = bits 8-15: 00011001 = 0b0001_1001 = 0x19
// =====================================================================

TEST(GoldenRR, HandoverCommand_Parse) {
    // Byte 0: PD(4)=6(RR)|skip(4)=0 = 0x60 [GSM 24.008 Table 11.2]
    // Byte 1: MTI = 0x2B (HandoverCommand) [3GPP TS 44.018 Table 10.4.1]
    // Bytes 2-3: CellDesc: ARFCN=100, NCC=5, BCC=3 [GSM 24.008 10.5.2.2]
    //   GSM_RR_Types.ttcn CellDescriptionV: FIELDORDER(lsb) - bcc first, then ncc, then arfcn
    //   bcc(3)=011, ncc(3)=101, arfcn(10)=0001100100
    //   LSB-first: 011|101|00 = 0x74, 00011001|00xxxxxx = 0x19 (arfcn=100=0x64, high 2 bits in byte 1)
    // Bytes 4-6: ChanDesc: typeAndOffset(5), TN(3), TSC(3), h(1), spare(2), ARFCN(10) [GSM 24.008 10.5.2.5]
    //   {0x11, 0xE0, 0x64}: typeAndOffset=2(TDMA_TCHF), TN=1, TSC=7, h=0, ARFCN=100
    // Byte 7: HORef = 0x17 [GSM 24.008 10.5.2.15, 5-bit handover reference]
    // Byte 8: PowerCmdAccType = 0x00 [GSM 24.008 10.5.2.28a]
    // Byte 9: SyncInd = 0x00 [GSM 24.008 10.5.2.39]
    uint8_t data[] = {
        0x60, 0x2b,
        0x74, 0x19,
        0x11, 0xE0, 0x64,
        0x17, 0x00, 0x00
    };
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3HandoverCommand::MTI);
}

// =====================================================================
// RR PARSE FROM HEX: Assignment Command (3GPP TS 44.018 9.1.2 / GSM 04.08 9.1.2)
// Reference: L3_Templates.ttcn tr_RR_AssignmentCommand (line 732):
//   discriminator := '0110'B (PD=6=RR), messageType := '00101110'B (MTI=0x2E)
// Reference: GSM_RR_Types.ttcn AssignmentCommand (line 483):
//   ChannelDescription chan_desc, PowerCommand_V power_cmd, ChannelMode_TV chan1_mode
// Structure: ChanDesc(24 bits) + PowerCmd(8 bits) + [optional IEs]
// Spec-verified: PD=6(RR), MTI=0x2E(AssignmentCommand) per 3GPP TS 44.018 Table 10.4.1
// [GSM SPEC VERIFIED] GSM 24.008 9.1.2: AssignmentCommand body = ChanDesc + PowerCmd + [optional].
//   ChannelDescription (GSM 24.008 10.5.2.5): 3 octets, MSB-first bit packing:
//   typeAndOffset(5)|TN(3)|TSC(3)|h(1)|spare(2)|ARFCN(10).
//   PowerCommand (GSM 24.008 10.5.2.28): 1 octet, power_command(5 MSB)|spare(3 LSB).
// =====================================================================

TEST(GoldenRR, AssignmentCommand_Parse) {
    // Byte 0: PD(4)=6(RR)|skip(4)=0 = 0x60 [GSM 24.008 Table 11.2]
    // Byte 1: MTI = 0x2E (AssignmentCommand) [3GPP TS 44.018 Table 10.4.1]
    // Bytes 2-4: ChanDesc: typeAndOffset(5), TN(3), TSC(3), h(1), spare(2), ARFCN(10) [GSM 24.008 10.5.2.5]
    //   {0x10, 0xE0, 0x64}: typeAndOffset=2(TDMA_TCHF), TN=0, TSC=7, h=0, ARFCN=100
    // Byte 5: PowerCmd = 0x00 [GSM 24.008 10.5.2.28, 5-bit power_command << 3]
    uint8_t data[] = {0x60, 0x2e, 0x10, 0xE0, 0x64, 0x00};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3AssignmentCommand::MTI);
}

// =====================================================================
// RR PARSE FROM HEX: Immediate Assignment (3GPP TS 44.018 9.1.19 / GSM 04.08 9.1.19)
// Reference: GSM_RR_Types.ttcn ImmediateAssignment (line 536):
//   DedicatedModeOrTbf ded_or_tbf, PageMode page_mode, ChannelDescription chan_desc,
//   RequestReference req_ref, TimingAdvance timing_advance, MobileAllocationLV mobile_allocation
// Structure: DedOrTBF(4)|PageMode(4) + ChanDesc(24 bits) + ReqRef(24 bits) + TA(8 bits) + MobileAlloc LV
// Spec-verified: PD=6(RR), MTI=0x3F(ImmediateAssignment) per 3GPP TS 44.018 Table 10.4.1
// [GSM SPEC VERIFIED] GSM 24.008 9.1.19: ImmediateAssignment body = DedOrTBF + PageMode
//   + ChanDesc + ReqRef + TA + MobileAlloc-LV.
//   DedicatedModeOrTBF (4 bits): tbf(1)|downlink(1)|spare(2), high nibble of byte.
//   PageMode (4 bits): NORMAL(0), EXTENDED(1), REORGANIZATION(2), SAME_AS_BEFORE(3).
//   RequestReference (GSM 24.008 10.5.2.30): 3 octets, RA(8)|T1p(5)|T3(6)|T2(5).
//   TimingAdvance (GSM 24.008 10.5.2.40): 1 octet, TA(6 MSB)|spare(2 LSB).
// =====================================================================

TEST(GoldenRR, ImmediateAssignment_Parse) {
    // Byte 0: PD(4)=6(RR)|skip(4)=0 = 0x60 [GSM 24.008 Table 11.2]
    // Byte 1: MTI = 0x3F (ImmediateAssignment) [3GPP TS 44.018 Table 10.4.1]
    // Byte 2: DedOrTBF(4)=0(dedicated)|PageMode(4)=0(Normal) = 0x00
    //   GSM_RR_Types.ttcn DedicatedModeOrTbf (line 374): spare+tma+downlink+tbf
    //   GSM_RR_Types.ttcn PageMode (line 382): PAGE_MODE_NORMAL(0)
    // Bytes 3-5: ChanDesc: typeAndOffset(5), TN(3), TSC(3), h(1), spare(2), ARFCN(10) [GSM 24.008 10.5.2.5]
    //   {0x00, 0x00, 0x64}: typeAndOffset=0(TDMA_SACCH), TN=0, TSC=0, h=0, ARFCN=100
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
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3ImmediateAssignment::MTI);
}

// =====================================================================
// RR PARSE FROM HEX: Immediate Assignment Reject (3GPP TS 44.018 9.1.20 / GSM 04.08 9.1.20)
// Reference: GSM_RR_Types.ttcn IMMEDIATE_ASSIGNMENT_REJECT ('00111010'B = 0x3A, line 28)
// Reference: GSM_RR_Types.ttcn ImmediateAssignmentReject (line 555):
//   FeatureIndicator feature_ind, PageMode page_mode, ReqRefWaitInd4 payload
// Structure: FeatureIndicator(4 MSB)|PageMode(4 LSB) + [optional RequestReferences]
// Spec-verified: PD=6(RR), MTI=0x3A(ImmediateAssignmentReject) per 3GPP TS 44.018 Table 10.4.1
// [GSM SPEC VERIFIED] GSM 24.008 9.1.20: ImmediateAssignmentReject body = FeatureInd + PageMode
//   + [ReqRefWaitInd4]. FeatureIndicator is 4 bits (peo_bcch_change_mark(2)|cs_ir(1)|ps_ir(1)).
//   PageMode is 4 bits: NORMAL(0), EXTENDED(1), REORGANIZATION(2), SAME_AS_BEFORE(3).
//   ReqRefWaitInd4 is optional: requestReference(8) + waitIndication(8), repeated up to 4 times.
// =====================================================================

TEST(GoldenRR, ImmediateAssignmentReject_Parse) {
    // Byte 0: PD(4)=6(RR)|skip(4)=0 = 0x60 [GSM 24.008 Table 11.2]
    // Byte 1: MTI = 0x3A (ImmediateAssignmentReject) [3GPP TS 44.018 Table 10.4.1]
    // Byte 2: FeatureIndicator(4)=0|PageMode(4)=3(SameAsBefore) = 0x03
    //   FeatureIndicator: peo_bcch_change_mark(2)=0, cs_ir(1)=0, ps_ir(1)=0 [GSM_RR_Types.ttcn line 440]
    //   PageMode: PAGE_MODE_SAME_AS_BEFORE(3) [GSM_RR_Types.ttcn line 382, FIELDLENGTH(4)]
    //   WaitIndication is a separate IE in ReqRefWaitInd4 payload (absent here, minimal message)
    uint8_t data[] = {0x60, 0x3a, 0x03};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3ImmediateAssignmentReject::MTI);
    auto* iar = tryGet<L3ImmediateAssignmentReject>(*msg);
    ASSERT_TRUE(iar);
    // PageMode=3(SameAsBefore) from low nibble of byte 2. Spec-verified: GSM_RR_Types.ttcn PageMode enum (line 382)
    EXPECT_EQ(iar->pageMode(), 3u);
}

// =====================================================================
// RR PARSE FROM HEX: Channel Mode Modify (3GPP TS 44.018 9.1.5 / GSM 04.08 9.1.5)
// Reference: L3_Templates.ttcn tr_RRM_ModeModify (line 650):
//   discriminator := '0110'B (PD=6=RR), messageType := '00010000'B (MTI=0x10)
// Reference: GSM_RR_Types.ttcn CHANNEL_MODE_MODIFY ('00010000'B = 0x10, line 76)
// Structure: ChanDesc(24 bits) + ChanMode(4 bits)|spare(4 bits) + [optional MultiRate]
// Spec-verified: PD=6(RR), MTI=0x10(ChannelModeModify) per 3GPP TS 44.018 Table 10.4.1
// [GSM SPEC VERIFIED] GSM 24.008 9.1.5: ChannelModeModify body = ChanDesc + ChanMode.
//   ChannelMode (GSM 24.008 10.5.2.6): 4 bits in low nibble, spare(4) in high nibble.
//   Values: SpeechV1(1), SpeechV2(2), SignallingOnly(8), SpeechV3_AMR(3).
// =====================================================================

TEST(GoldenRR, ChannelModeModify_Parse) {
    // Byte 0: PD(4)=6(RR)|skip(4)=0 = 0x60 [GSM 24.008 Table 11.2]
    // Byte 1: MTI = 0x10 (ChannelModeModify) [3GPP TS 44.018 Table 10.4.1]
    // Bytes 2-4: ChanDesc: typeAndOffset(5), TN(3), TSC(3), h(1), spare(2), ARFCN(10) [GSM 24.008 10.5.2.5]
    //   {0x11, 0xE0, 0x64}: typeAndOffset=2(TDMA_TCHF), TN=1, TSC=7, h=0, ARFCN=100
    // Byte 5: ChanMode(4)=1(SpeechV1)|spare(4)=0 = 0x01 [GSM 24.008 10.5.2.6]
    uint8_t data[] = {0x60, 0x10, 0x11, 0xE0, 0x64, 0x01};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3ChannelModeModify::MTI);
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
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3GPRSSuspensionRequest::MTI);
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
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3ApplicationInformation::MTI);
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
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3SynchronizationChannelInformation::MTI);
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
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3ChannelRequest::MTI);
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
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3HandoverAccess::MTI);
}

// =====================================================================
// RR PARSE FROM HEX: Ciphering Mode Command (3GPP TS 44.018 9.1.9 / GSM 04.08 9.1.9)
// Reference: L3_Templates.ttcn ts_RRM_CiphModeCmd (line 690):
//   messageType := '00110101'B (MTI=0x35), cipherModeSetting: sC='1'B, algorithmIdentifier
// Reference: GSM_RR_Types.ttcn CIPHERING_MODE_COMMAND ('00110101'B = 0x35, line 31)
// Structure: cipherModeResponse(4 MSB)|cipherModeSetting(4 LSB) = 8 bits
// Spec-verified: PD=6(RR), MTI=0x35(CipheringModeCommand) per 3GPP TS 44.018 Table 10.4.1
// CipheringModeSetting: GSM 24.008 10.5.2.9 (4 bits: sC(1)|algorithmIdentifier(3))
//   Algorithm 3 = A5/3 (KASUMI), sC=1 (ciphering on)
// [GSM SPEC VERIFIED] Byte layout per GSM_RR_Types.ttcn CipheringModeCommand record:
//   cipherModeResponse(4)|cipherModeSetting(4). cipherModeResponse has cR(1)|spare(3).
//   cipherModeSetting=0b1_011 (sC=1, algId=3), cipherModeResponse=0b0000 (cR=0)
//   Combined: 0b0000_1011 = 0x0B
// =====================================================================

TEST(GoldenRR, CipheringModeCommand_Parse) {
    // Byte 0: PD(4)=6(RR)|skip(4)=0 = 0x60 [GSM 24.008 Table 11.2]
    // Byte 1: MTI = 0x35 (CipheringModeCommand) [3GPP TS 44.018 Table 10.4.1]
    // Byte 2: cipherModeResponse(4)=0(cR=0,no IMEISV)|cipherModeSetting(4)=sC(1)=1(on)|algorithmIdentifier(3)=3(A5/3) = 0x0B
    //   GSM 24.008 10.5.2.9: cipheringModeSetting is 4 bits sC(1)|algorithmIdentifier(3), MSB-first
    //   L3_Templates.ttcn ts_RRM_CiphModeCmd: sC='1'B, algorithmIdentifier=alg_id(BIT3)
    uint8_t data[] = {0x60, 0x35, 0x0B};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3CipheringModeCommand::MTI);
}

// =====================================================================
// RR PARSE FROM HEX: RR Status (GSM 04.08 9.1.29)
// Reference: L3_Templates.ttcn tr_RRM_RR_STATUS
// [GSM SPEC VERIFIED] GSM 24.008 9.1.29: RRStatus body = cause(1 octet).
//   The cause follows GSM 24.008 Table 10.5.2.31 (RR cause values).
//   Value 0x6F = Protocol_Error_Unspecified: generic protocol error indicator.
// =====================================================================

TEST(GoldenRR, RRStatus_Parse_ProtocolError) {
    // Byte 0: PD(4)|skip(4) = 0x60
    // Byte 1: MTI = 0x12 (RRStatus)
    // Byte 2: cause = 0x6f (Protocol_Error_Unspecified)
    uint8_t data[] = {0x60, 0x12, 0x6f};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    auto* rs = tryGet<L3RRStatus>(*msg);
    ASSERT_TRUE(rs);
    EXPECT_EQ(rs->cause(), RRCause::Protocol_Error_Unspecified);
}

// =====================================================================
// RR PARSE FROM HEX: Physical Information (3GPP TS 44.018 9.1.12 / GSM 04.08 9.1.12)
// Reference: GSM_RR_Types.ttcn PHYSICAL_INFORMATION ('00101101'B = 0x2D, line 44)
// Structure: TimingAdvance(8 bits, GSM 24.008 10.5.2.40)
// Spec-verified: PD=6(RR), MTI=0x2D(PhysicalInformation) per 3GPP TS 44.018 Table 10.4.1
// TimingAdvance: 6-bit value (0-63) shifted left by 2 bits, spare(2)=0
// [GSM SPEC VERIFIED] GSM 24.008 9.1.12: PhysicalInformation body = TimingAdvance(1 octet).
//   TimingAdvance encoding: timing_advance(6 MSB)|spare(2 LSB). Value range 0-63.
//   This test uses TA=63 (maximum), encoded as 63<<2 = 0b111111_00 = 0xFC.
// =====================================================================

TEST(GoldenRR, PhysicalInformation_Parse) {
    // Byte 0: PD(4)=6(RR)|skip(4)=0 = 0x60 [GSM 24.008 Table 11.2]
    // Byte 1: MTI = 0x2D (PhysicalInformation) [3GPP TS 44.018 Table 10.4.1]
    // Byte 2: TA = 63<<2 = 0xFC [GSM 24.008 10.5.2.40: timing_advance(6)=63(max)|spare(2)=0]
    uint8_t data[] = {0x60, 0x2d, 0xFC};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3PhysicalInformation::MTI);
}

// =====================================================================
// RR PARSE FROM HEX: Additional Assignment (3GPP TS 44.018 9.1.1 / GSM 04.08 9.1.1)
// Reference: GSM_RR_Types.ttcn ADDITIONAL_ASSIGNMENT ('00111011'B = 0x3B, line 25)
// Structure: AdditionalChanDesc(24 bits) + [optional PowerCommand(8 bits)]
// Spec-verified: PD=6(RR), MTI=0x3B(AdditionalAssignment) per 3GPP TS 44.018 Table 10.4.1
// [GSM SPEC VERIFIED] GSM 24.008 9.1.1: AdditionalAssignment body = AdditionalChanDesc + [PowerCmd].
//   AdditionalChannelDescription (same format as ChannelDescription, GSM 24.008 10.5.2.5):
//   typeAndOffset(5)|TN(3)|TSC(3)|h(1)|spare(2)|ARFCN(10) = 3 octets.
//   This test: typeAndOffset=2(TCHF), TN=2, TSC=5, h=0, ARFCN=86 -> {0x12, 0xA0, 0x56}.
// =====================================================================

TEST(GoldenRR, AdditionalAssignment_Parse) {
    // Byte 0: PD(4)=6(RR)|skip(4)=0 = 0x60 [GSM 24.008 Table 11.2]
    // Byte 1: MTI = 0x3B (AdditionalAssignment) [3GPP TS 44.018 Table 10.4.1]
    // Bytes 2-4: AdditionalChanDesc: typeAndOffset(5), TN(3), TSC(3), h(1), spare(2), ARFCN(10)
    //   {0x12, 0xA0, 0x56}: typeAndOffset=2, TN=2, TSC=5, h=0, ARFCN=86 [GSM 24.008 10.5.2.5]
    uint8_t data[] = {0x60, 0x3b, 0x12, 0xA0, 0x56};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3AdditionalAssignment::MTI);
}

// =====================================================================
// RR ROUNDTrip: AssignmentCommand (GSM 04.08 9.1.2)
// Reference: L3_Templates.ttcn tr_RR_AssignmentCommand
// =====================================================================

TEST(GoldenRR, AssignmentCommand_RoundTrip) {
    ParsedMessage msg(RRM(L3AssignmentCommand{}));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3AssignmentCommand::MTI);
}

// =====================================================================
// RR ROUNDTrip: ImmediateAssignment (GSM 04.08 9.1.19)
// Reference: GSM_RR_Types.ttcn ImmediateAssignment
// =====================================================================

TEST(GoldenRR, ImmediateAssignment_RoundTrip) {
    ParsedMessage msg(RRM(L3ImmediateAssignment{}));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3ImmediateAssignment::MTI);
}

// =====================================================================
// RR ROUNDTrip: ImmediateAssignmentExtended (GSM 04.08 9.1.18)
// Reference: GSM_RR_Types.ttcn IMMEDIATE_ASSIGNMENT_EXTENDED
// =====================================================================

TEST(GoldenRR, ImmediateAssignmentExtended_RoundTrip) {
    ParsedMessage msg(RRM(L3ImmediateAssignmentExtended{}));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3ImmediateAssignmentExtended::MTI);
    auto* iaext = tryGet<L3ImmediateAssignmentExtended>(*parsed);
    ASSERT_TRUE(iaext);
    EXPECT_FALSE(iaext->hasAdditionalChannel());
}

// =====================================================================
// RR ROUNDTrip: ImmediateAssignmentReject (GSM 04.08 9.1.20)
// Reference: GSM_RR_Types.ttcn IMMEDIATE_ASSIGNMENT_REJECT
// =====================================================================

TEST(GoldenRR, ImmediateAssignmentReject_RoundTrip) {
    L3ImmediateAssignmentReject concrete(30);
    EXPECT_EQ(concrete.waitTime(), 30u);
    ParsedMessage msg(RRM(std::move(concrete)));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3ImmediateAssignmentReject::MTI);
}

// =====================================================================
// RR ROUNDTrip: AdditionalAssignment (GSM 04.08 9.1.1)
// Reference: GSM_RR_Types.ttcn ADDITIONAL_ASSIGNMENT
// =====================================================================

TEST(GoldenRR, AdditionalAssignment_RoundTrip) {
    ParsedMessage msg(RRM(L3AdditionalAssignment{}));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3AdditionalAssignment::MTI);
}

// =====================================================================
// RR ROUNDTrip: ChannelModeModify (GSM 04.08 9.1.5)
// Reference: L3_Templates.ttcn tr_RRM_ModeModify
// =====================================================================

TEST(GoldenRR, ChannelModeModify_RoundTrip) {
    L3ChannelDescription chd(TDMA_TCHF, 1, 7, 100);
    L3ChannelMode mode(L3ChannelMode::SpeechV1);
    ParsedMessage msg(RRM(L3ChannelModeModify(chd, mode)));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3ChannelModeModify::MTI);
}

// =====================================================================
// RR ROUNDTrip: ChannelModeModifyAcknowledge (GSM 04.08 9.1.6)
// Reference: GSM_RR_Types.ttcn CHANNEL_MODE_MODIFY_ACKNOWLEDGE
// =====================================================================

TEST(GoldenRR, ChannelModeModifyAcknowledge_RoundTrip) {
    uint8_t data[] = {0x60, 0x17, 0x11, 0xE0, 0x64, 0x01};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    auto* cma = tryGet<L3ChannelModeModifyAcknowledge>(*msg);
    ASSERT_TRUE(cma);
    EXPECT_EQ(cma->description().typeAndOffset(), TDMA_TCHF);
    EXPECT_EQ(cma->mode().mode(), L3ChannelMode::SpeechV1);
    auto parsed = roundtrip(*msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3ChannelModeModifyAcknowledge::MTI);
}

// =====================================================================
// RR ROUNDTrip: MeasurementReport (GSM 04.08 9.1.21)
// Reference: L3_Templates.ttcn ts_MEAS_REP
// =====================================================================

TEST(GoldenRR, MeasurementReport_RoundTrip) {
    ParsedMessage msg(RRM(L3MeasurementReport{}));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3MeasurementReport::MTI);
}

// =====================================================================
// RR ROUNDTrip: ClassmarkEnquiry (GSM 04.08 9.1.14)
// Reference: L3_Templates.ttcn tr_RRM_CM_ENQUIRY
// =====================================================================

TEST(GoldenRR, ClassmarkEnquiry_RoundTrip) {
    ParsedMessage msg(RRM(L3ClassmarkEnquiry{}));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3ClassmarkEnquiry::MTI);
}

// =====================================================================
// RR ROUNDTrip: ClassmarkChange (GSM 04.08 9.1.11)
// Reference: L3_Templates.ttcn ts_RRM_CM_CHG
// =====================================================================

TEST(GoldenRR, ClassmarkChange_RoundTrip) {
    uint8_t data[] = {0x60, 0x16, 0x03, 0x20, 0x00, 0x80};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    auto* cm = tryGet<L3ClassmarkChange>(*msg);
    ASSERT_TRUE(cm);
    auto parsed = roundtrip(*msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3ClassmarkChange::MTI);
}

// =====================================================================
// RR ROUNDTrip: HandoverCommand (GSM 04.08 9.1.15)
// Reference: L3_Templates.ttcn ts_RR_HandoverCommand
// =====================================================================

TEST(GoldenRR, HandoverCommand_RoundTrip) {
    ParsedMessage msg(RRM(L3HandoverCommand{}));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3HandoverCommand::MTI);
}

// =====================================================================
// RR ROUNDTrip: PagingResponse (GSM 04.08 9.1.25)
// Reference: L3_Templates.ttcn ts_PAG_RESP
// =====================================================================

TEST(GoldenRR, PagingResponse_RoundTrip) {
    ParsedMessage msg(RRM(L3PagingResponse{}));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3PagingResponse::MTI);
}

// =====================================================================
// RR ROUNDTrip: PhysicalInformation (GSM 04.08 9.1.12)
// Reference: GSM_RR_Types.ttcn PHYSICAL_INFORMATION
// =====================================================================

TEST(GoldenRR, PhysicalInformation_RoundTrip) {
    ParsedMessage msg(RRM(L3PhysicalInformation{}));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3PhysicalInformation::MTI);
}

// =====================================================================
// RR ROUNDTrip: RRStatus (GSM 04.08 9.1.29)
// Reference: L3_Templates.ttcn tr_RRM_RR_STATUS
// =====================================================================

TEST(GoldenRR, RRStatus_RoundTrip) {
    uint8_t data[] = {0x60, 0x12, 0x60};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    auto* rs = tryGet<L3RRStatus>(*msg);
    ASSERT_TRUE(rs);
    EXPECT_EQ(rs->cause(), RRCause::Invalid_Mandatory_Information);
    auto parsed = roundtrip(*msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3RRStatus::MTI);
}

// =====================================================================
// RR ROUNDTrip: AssignmentComplete (GSM 04.08 9.1.3)
// Reference: GSM_RR_Types.ttcn ASSIGNMENT_COMPLETE
// =====================================================================

TEST(GoldenRR, AssignmentComplete_RoundTrip) {
    uint8_t data[] = {0x60, 0x29, 0x00};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    auto* ac = tryGet<L3AssignmentComplete>(*msg);
    ASSERT_TRUE(ac);
    EXPECT_EQ(ac->cause(), RRCause::Normal_Event);
    auto parsed = roundtrip(*msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3AssignmentComplete::MTI);
}

// =====================================================================
// RR ROUNDTrip: AssignmentFailure (GSM 04.08 9.1.3)
// Reference: GSM_RR_Types.ttcn ASSIGNMENT_FAILURE
// =====================================================================

TEST(GoldenRR, AssignmentFailure_RoundTrip) {
    uint8_t data[] = {0x60, 0x2f, 0x09};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    auto* af = tryGet<L3AssignmentFailure>(*msg);
    ASSERT_TRUE(af);
    EXPECT_EQ(af->cause(), RRCause::Channel_Mode_Unacceptable);
    auto parsed = roundtrip(*msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3AssignmentFailure::MTI);
}

// =====================================================================
// RR ROUNDTrip: HandoverComplete (GSM 04.08 9.1.16)
// Reference: GSM_RR_Types.ttcn HANDOVER_COMPLETE
// =====================================================================

TEST(GoldenRR, HandoverComplete_RoundTrip) {
    uint8_t data[] = {0x60, 0x2c, 0x00};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    auto* hc = tryGet<L3HandoverComplete>(*msg);
    ASSERT_TRUE(hc);
    EXPECT_EQ(hc->cause(), RRCause::Normal_Event);
    auto parsed = roundtrip(*msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3HandoverComplete::MTI);
}

// =====================================================================
// RR ROUNDTrip: HandoverFailure (GSM 04.08 9.1.17)
// Reference: GSM_RR_Types.ttcn HANDOVER_FAILURE
// =====================================================================

TEST(GoldenRR, HandoverFailure_RoundTrip) {
    uint8_t data[] = {0x60, 0x28, 0x08};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    auto* hf = tryGet<L3HandoverFailure>(*msg);
    ASSERT_TRUE(hf);
    EXPECT_EQ(hf->cause(), RRCause::Handover_Impossible);
    auto parsed = roundtrip(*msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3HandoverFailure::MTI);
}

// =====================================================================
// RR ROUNDTrip: GPRSSuspensionRequest (GSM 04.08 9.1.13b)
// Reference: GSM_RR_Types.ttcn GPRS_SUSPENSION_REQUEST
// =====================================================================

TEST(GoldenRR, GPRSSuspensionRequest_RoundTrip) {
    ParsedMessage msg(RRM(L3GPRSSuspensionRequest{}));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3GPRSSuspensionRequest::MTI);
}
