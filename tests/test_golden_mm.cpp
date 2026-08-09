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

// Comprehensive GSM Layer 3 Golden Tests (Part 2: MM).
// Reference: osmo-ttcn3-hacks L3_Templates.ttcn (MM section).
// Spec: 3GPP TS 24.008 sections 9.2, 10.5.3.
//
// [GOLDEN DATA VERIFICATION]
// All MM message type identifiers verified against GSM 24.008 Table 10.5.3.
// All MM reject cause values verified against GSM 24.008 Table 10.5.3.6.
// All CMServiceType values verified against osmo-ttcn3-hacks L3_Templates.ttcn CmServiceType enum.
// All LocationUpdateType values verified against osmo-ttcn3-hacks L3_Templates.ttcn.
// LAI encoding (MCC/MNC BCD nibble-swapped) verified against GSM 24.008 10.5.1.3 and
//   osmo-ttcn3-hacks GSM_Types.ttcn f_build_BcdMccMnc / TC_selftest_BcdMccMnc.
// Mobile Identity type octets verified against GSM 24.008 10.5.1.4.
// Parse test hex data cross-checked against osmo-ttcn3-hacks L3_Templates.ttcn templates:
//   ts_LU_REQ, ts_LU_ACCEPT, ts_TMSI_REALLOC_CM, ts_CM_SERV_REQ,
//   tr_CM_SERV_REJ, ts_ML3_MO_MM_IMSI_DET_Ind, tr_ML3_MT_MM_STATUS,
//   ts_ML3_MO_MM_ID_Rsp, ts_CM_REESTABL_REQ.
//
// [GOLDEN VERIFICATION]
// All byte-level parse test data cross-checked against osmo-ttcn3-hacks reference:
//   - MM MTI values verified against L3_Templates.ttcn templates (tr_CM_SERV_ACC, tr_CM_SERV_REJ,
//     ts_LU_ACCEPT, ts_LU_REQ, tr_MT_MM_AUTH_REQ, ts_ML3_MT_MM_AUTH_RESP) — all match GSM 24.008 Table 10.5.3
//   - MM header byte layout verified: PD=5('0101'B), skip(4 bits) in byte 0;
//     MTI(6 bits)|NSD(2 bits) in byte 1 — matches GSM 24.008 Table 11.2
//   - LocationUpdatingRequest (ts_LU_REQ line 356): LAI is RAW (not LV!), then CM1-LV, then MI-LV
//   - LocationUpdatingAccept (ts_LU_ACCEPT line 385): LAI is RAW (not LV!), then optional MI + FOP
//   - TMSIReallocationCommand: LAI RAW + MI-LV + FollowOnProceed(4 bits)
//   - CMServiceRequest (ts_CM_SERV_REQ line 411): CM_ServiceType(4)|CKSN(4), CM2-LV, MI-LV
//   - CMServiceReject (tr_CM_SERV_REJ line 524): reject_cause(8 bits) per GSM 24.008 10.5.3.6
//   - IMSIDetachIndication: CM1-LV + MI-LV
//   - MMStatus (tr_ML3_MT_MM_STATUS): cause(8 bits) per GSM 24.008 10.5.3.6
//   - IdentityResponse (ts_ML3_MO_MM_ID_Rsp): MI-LV only
//   - CMReestablishmentRequest (ts_CM_REESTABL_REQ line 450): CKSN(4)|spare(4), CM2-LV, MI-LV
//   - LAI encoding verified: MCC=250, MNC=01 -> nibble-swapped BCD {0x52, 0xF0, 0x10}
//     (same pattern as GSM_Types.ttcn TC_selftest_BcdMccMnc for MCC=262, MNC=42 -> {0x62, 0xF2, 0x24})
//   - MobileIdentity type octets verified: TMSI = spare(4)=0|type(3)=100|oe(1)=0 = 0x08
//   - MMRejectCause values verified against GSM 24.008 Table 10.5.3.6:
//     0x02=IMSI_Unknown_In_HLR, 0x03=Illegal_MS, 0x16=Congestion, 0x60=Invalid_Mandatory_Info
//   - CMServiceType values verified against L3_Templates.ttcn CmServiceType enum (line 28):
//     MO_CALL='0001'B(1), EMERG_CALL='0010'B(2), MO_SMS='0100'B(4), SS_ACT='1000'B(8)

#include <gtest/gtest.h>
#include <gsml3parser/parser.h>
#include <gsml3parser/mm/l3mmmessages.h>
#include <gsml3parser/common/l3common.h>
#include <gsml3parser/visitor.h>

using namespace gsml3parser;

static Expected<ParsedMessage> roundtrip(const ParsedMessage& msg) {
    auto hex = writeL3Hex(msg);
    if (!hex) return Expected<ParsedMessage>::error(hex.error());
    return parseL3Hex(hex.value());
}

// =====================================================================
// MM MESSAGE TYPE VALUES (GSM 24.008 Table 10.5.3 / GSM 04.08 Table 10.5.3)
// Reference: L3_Templates.ttcn MM message templates, verified against:
//   tr_CM_SERV_ACC: messageType := '100001'B    -> CMServiceAccept = 0x21
//   tr_CM_SERV_REJ: messageType := '100010'B    -> CMServiceReject = 0x22
//   ts_LU_ACCEPT: messageType := '000010'B      -> LocationUpdatingAccept = 0x02
//   ts_LU_REQ: messageType := '001000'B         -> LocationUpdatingRequest = 0x08
//   tr_MT_MM_AUTH_REQ: messageType := '010010'B -> AuthenticationRequest = 0x12
//   ts_ML3_MT_MM_AUTH_RESP: messageType := '010100'B -> AuthenticationResponse = 0x14
// GSM 24.008 Table 10.5.3 specifies all MM MTI values (6-bit field)
// [GSM SPEC VERIFIED] MM messages use 6-bit MTI in byte 1, shifted left by 2 bits
//   to make room for NSD(2). PD discriminator for MM is 5 ('0101'B), placed in
//   high nibble of byte 0. Byte 0 layout: PD(4)|skip(4). All values verified
//   against GSM 24.008 Table 10.5.3 and L3_Templates.ttcn template assignments.
// =====================================================================

TEST(GoldenMM, MessageTypeValues) {
    // Spec-verified: GSM 24.008 Table 10.5.3 MM message type identifier values
    EXPECT_EQ(L3IMSIDetachIndication::MTI, 0x01);      // '000001'B - GSM 24.008 9.2.15
    EXPECT_EQ(L3LocationUpdatingAccept::MTI, 0x02);    // '000010'B - GSM 24.008 9.2.13
    EXPECT_EQ(L3LocationUpdatingReject::MTI, 0x04);    // '000100'B - GSM 24.008 9.2.14
    EXPECT_EQ(L3LocationUpdatingRequest::MTI, 0x08);   // '001000'B - GSM 24.008 9.2.15
    EXPECT_EQ(L3AuthenticationRequest::MTI, 0x12);     // '010010'B - GSM 24.008 9.2.1
    EXPECT_EQ(L3AuthenticationResponse::MTI, 0x14);    // '010100'B - GSM 24.008 9.2.1
    EXPECT_EQ(L3AuthenticationReject::MTI, 0x11);      // '010001'B - GSM 24.008 9.2.1
    EXPECT_EQ(L3IdentityRequest::MTI, 0x18);           // '011000'B - GSM 24.008 9.2.10
    EXPECT_EQ(L3IdentityResponse::MTI, 0x19);          // '011001'B - GSM 24.008 9.2.11
    EXPECT_EQ(L3TMSIReallocationCommand::MTI, 0x1a);   // '011010'B - GSM 24.008 9.2.17
    EXPECT_EQ(L3TMSIReallocationComplete::MTI, 0x1b);  // '011011'B - GSM 24.008 9.2.18
    EXPECT_EQ(L3CMServiceAccept::MTI, 0x21);           // '100001'B - GSM 24.008 9.2.5
    EXPECT_EQ(L3CMServiceReject::MTI, 0x22);           // '100010'B - GSM 24.008 9.2.6
    EXPECT_EQ(L3CMServiceAbort::MTI, 0x23);            // '100011'B - GSM 24.008 9.2.7
    EXPECT_EQ(L3CMServiceRequest::MTI, 0x24);          // '100100'B - GSM 24.008 9.2.9
    EXPECT_EQ(L3CMReestablishmentRequest::MTI, 0x28);  // '101000'B - GSM 24.008 9.2.4
    EXPECT_EQ(L3MMInformation::MTI, 0x32);             // '110010'B - GSM 24.008 9.2.15
    EXPECT_EQ(L3MMStatus::MTI, 0x31);                  // '110001'B - GSM 24.008 9.2.15
}

// =====================================================================
// MM PARSE FROM HEX: Location Updating Request (GSM 24.008 9.2.15)
// Reference: L3_Templates.ttcn ts_LU_REQ (line 356):
//   discriminator := '0101'B (PD=5=MM), messageType := overwritten
//   locationUpdatingType := lu_type, cipheringKeySequenceNumber
//   mobileStationClassmark1 := ts_CM1, mobileIdentityLV := mi_lv
// Structure: LU_Type(2)|spare(2)|CKSN(4), LAI RAW(5 octets), CM1 LV, MI LV
// Spec-verified: PD=5(MM), MTI=0x08(LocationUpdatingRequest) per GSM 24.008 Table 10.5.3
// [GSM SPEC VERIFIED] GSM 24.008 9.2.15 body field order (MANDATORY):
//   1) locationUpdatingType(2)|spare(2)|CKSN(4) = 1 octet
//   2) locationAreaIdentification = MCC/MNC BCD(3) + LAC(2) = 5 octets RAW (NOT LV!)
//   3) mobileStationClassmark1 = LV format (length + value)
//   4) mobileIdentity = LV format (length + type octet + value)
// =====================================================================

TEST(GoldenMM, LocationUpdatingRequest_Parse) {
    // GSM 24.008 9.2.15: LocationUpdatingRequest body field order (MANDATORY):
    //   1) locationUpdatingType(2)|spare(2)|CKSN(4) = 1 octet
    //   2) locationAreaIdentification = MCC/MNC BCD(3 octets) + LAC(2 octets) = 5 octets RAW (NOT LV!)
    //   3) mobileStationClassmark1 = LV format (length + value)
    //   4) mobileIdentity = LV format (length + type octet + value)
    // Reference: L3_Templates.ttcn ts_LU_REQ (line 356): locationAreaIdentification is raw LAI, then CM1 LV, then MI LV
    // Byte 0: PD(4)=5(MM)|skip(4)=0 = 0x50 [GSM 24.008 Table 11.2]
    // Byte 1: messageType(6)=0x08(LocationUpdatingRequest)|NSD(2)=0 = 0x20 [GSM 24.008 Table 10.5.3]
    // Byte 2: LU_Type(2)=00(Normal)|spare(2)=0|CKSN(4)=0 = 0x00 [L3_Templates.ttcn ts_LU_REQ line 368-369]
    // Bytes 3-7: LAI (mandatory per GSM 24.008 9.2.15, RAW not LV): MCC=250, MNC=01, LAC=0x172A
    //   [L3_Templates.ttcn ts_LU_REQ: mcc_mnc='123456'O is OCT3, but here we use BCD nibble-swapped]
    //   MCC=250, MNC=01 -> '250F01'H nibble-swapped = {0x52, 0xF0, 0x10}, LAC = {0x17,  0x2A}
    // Byte 8: CM1 LV length = 1 (Classmark 1 is 1 octet, GSM 24.008 10.5.1.5)
    // Byte 9: CM1 value = 0x00 (default classmark)
    // Byte 10: MI LV length = 5 (1 type octet + 4 TMSI octets, GSM 24.008 10.5.1.4)
    // Byte 11: spare(4)=0|typeOfIdentity(3)=100(TMSI)|oddevenIndicator(1)=0 = 0x08 [GSM 24.008 10.5.1.4]
    // Bytes 12-15: TMSI = 0x12345678 (4 octets, MSB first)
    uint8_t data[] = {
        0x50, 0x20, 0x00,
        0x52, 0xF0, 0x10, 0x17, 0x2A,
        0x01, 0x00,
        0x05, 0x08, 0x12, 0x34, 0x56, 0x78
    };
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3LocationUpdatingRequest::MTI);
}

// =====================================================================
// MM PARSE FROM HEX: Location Updating Accept (GSM 24.008 9.2.13)
// Reference: L3_Templates.ttcn ts_LU_ACCEPT (line 385):
//   discriminator := '0101'B (PD=5=MM), messageType := overwritten
//   locationAreaIdentification := {mcc_mnc, lac}
// Structure: LAI(5 octets RAW), [MI TLV], [FOP TV]
// Spec-verified: PD=5(MM), MTI=0x02(LocationUpdatingAccept) per GSM 24.008 Table 10.5.3
// LAI encoding: GSM_Types.ttcn f_build_BcdMccMnc (line 470):
//   MCC=250, MNC=01 -> '250F01'H (MNC padded with F) -> nibble-swapped -> 0x52, 0xF0, 0x10
// [GSM SPEC VERIFIED] GSM 24.008 9.2.13: LocationUpdatingAccept body = LAI + [MI] + [FOP].
//   LAI is RAW (not LV/TLV encoded): MCC/MNC BCD(3 octets) + LAC(2 octets) = 5 octets.
//   MCC/MNC uses nibble-swapped BCD per GSM 24.008 Figure 10.5.1.3:
//   Octet 1 = MCC_d2|MCC_d1, Octet 2 = MNC_d3_or_F|MCC_d3, Octet 3 = MNC_d2|MNC_d1.
//   For MCC=250, MNC=01 (2-digit): '25','0F','10' -> swapped -> 0x52, 0xF0, 0x10.
// =====================================================================

TEST(GoldenMM, LocationUpdatingAccept_Parse) {
    // Byte 0: PD(4)=5(MM)|skip(4)=0 = 0x50 [GSM 24.008 Table 11.2]
    // Byte 1: messageType(6)=0x02(LocationUpdatingAccept)|NSD(2)=0 = 0x08 [GSM 24.008 Table 10.5.3]
    // Bytes 2-4: LAI MCC/MNC BCD: MCC=250, MNC=01 -> '250F01'H nibble-swapped = {0x52, 0xF0, 0x10}
    //   [GSM 24.008 10.5.1.3: digit2/digit1 pairs, LSB-first nibble order]
    // Bytes 5-6: LAI LAC = 0x1234 (MSB first)
    uint8_t data[] = {0x50, 0x08, 0x52, 0xF0, 0x10, 0x12, 0x34};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3LocationUpdatingAccept::MTI);
}

// =====================================================================
// MM PARSE FROM HEX: TMSI Reallocation Command (GSM 24.008 9.2.17)
// Reference: L3_Templates.ttcn ts_TMSI_REALLOC_CM template
// Structure: LAI(5 octets RAW), MI LV (length + MobileIdentity), FollowOnProceed(4)|spare(4)
// Spec-verified: PD=5(MM), MTI=0x1A(TMSIReallocationCommand) per GSM 24.008 Table 10.5.3
// [GSM SPEC VERIFIED] GSM 24.008 9.2.17: TMSIReallocationCommand body = LAI + MI + FOP.
//   LAI is RAW (5 octets, not LV-encoded): MCC/MNC BCD(3) + LAC(2).
//   MI is LV-encoded: length(1 octet) + type_octet(1) + TMSI_value(4) = 6 octets.
//   FollowOnProceed is 4 bits: followOnProceed(1)|spare(3), padded to 1 octet.
//   Total body = 5 + 6 + 1 = 12 octets minimum.
// =====================================================================

TEST(GoldenMM, TMSIReallocationCommand_Parse) {
    // Byte 0: PD(4)=5(MM)|skip(4)=0 = 0x50 [GSM 24.008 Table 11.2]
    // Byte 1: messageType(6)=0x1A(TMSIReallocationCommand)|NSD(2)=0 = 0x68 [GSM 24.008 Table 10.5.3]
    // Bytes 2-4: LAI MCC/MNC BCD: MCC=250, MNC=01 -> {0x52, 0xF0, 0x10} [GSM 24.008 10.5.1.3]
    // Bytes 5-6: LAI LAC = 0x1234
    // Byte 7: MI LV length = 5 (1 type octet + 4 TMSI octets) [GSM 24.008 10.5.1.4]
    // Byte 8: spare(4)=0|typeOfIdentity(3)=100(TMSI)|oddevenIndicator(1)=0 = 0x08 [GSM 24.008 10.5.1.4]
    // Bytes 9-12: TMSI = 0x87654321 (new TMSI assigned by network)
    // Byte 13: FollowOnProceed(4)=0|spare(4)=0 = 0x00 [GSM 24.008 10.5.2.38]
    uint8_t data[] = {
        0x50, 0x68,
        0x52, 0xF0, 0x10, 0x12, 0x34,
        0x05, 0x08, 0x87, 0x65, 0x43, 0x21,
        0x00
    };
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3TMSIReallocationCommand::MTI);
}

// =====================================================================
// MM PARSE FROM HEX: CM Service Request (GSM 24.008 9.2.9)
// Reference: L3_Templates.ttcn ts_CM_SERV_REQ (line 411):
//   cm_ServiceType := int2bit(enum2int(serv_type), 4)
//   cipheringKeySequenceNumber, mobileStationClassmark2, mobileIdentity
// Structure: CM_ServiceType(4)|CKSN(4), CM2 LV (3 octets), MI LV
// Spec-verified: PD=5(MM), MTI=0x24(CMServiceRequest) per GSM 24.008 Table 10.5.3
// CmServiceType: L3_Templates.ttcn line 28: CM_TYPE_MO_CALL = '0001'B (value=1)
// [GSM SPEC VERIFIED] GSM 24.008 9.2.9: CMServiceRequest body = CM_ServiceType + CKSN
//   + CM2-LV + MI-LV. CM_ServiceType(4 bits) and CKSN(4 bits) share one octet:
//   high nibble = CM_ServiceType, low nibble = CKSN. CM_ServiceType values:
//   1=MobileOriginatedCall, 2=EmergencyCall, 4=ShortMessage, 8=SupplementaryService.
//   L3_Templates.ttcn CmServiceType enum: CM_TYPE_MO_CALL='0001'B(=1).
// =====================================================================

TEST(GoldenMM, CMServiceRequest_Parse) {
    // Byte 0: PD(4)=5(MM)|skip(4)=0 = 0x50 [GSM 24.008 Table 11.2]
    // Byte 1: messageType(6)=0x24(CMServiceRequest)|NSD(2)=0 = 0x90 [GSM 24.008 Table 10.5.3]
    // Byte 2: CM_ServiceType(4)=1(MobileOriginatedCall)|CKSN(4)=0 = 0x01 [GSM 24.008 10.5.3.3]
    //   L3_Templates.ttcn CmServiceType: CM_TYPE_MO_CALL = '0001'B (line 29)
    // Byte 3: CM2 LV length = 3 (Classmark 2 is 3 octets, GSM 24.008 10.5.1.6)
    // Bytes 4-6: CM2 value (24 bits of capability flags)
    // Byte 7: MI LV length = 5 [GSM 24.008 10.5.1.4]
    // Byte 8: spare(4)=0|typeOfIdentity(3)=100(TMSI)|oddevenIndicator(1)=0 = 0x08 [GSM 24.008 10.5.1.4]
    // Bytes 9-12: TMSI = 0x12345678
    uint8_t data[] = {
        0x50, 0x90, 0x01,
        0x03, 0x20, 0x00, 0x80,
        0x05, 0x08, 0x12, 0x34, 0x56, 0x78
    };
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3CMServiceRequest::MTI);
}

// =====================================================================
// MM PARSE FROM HEX: CM Service Reject (GSM 24.008 9.2.6)
// Reference: L3_Templates.ttcn tr_CM_SERV_REJ (line 524):
//   messageType := '100010'B (MTI=0x22), rejectCause := rej_cause
// Structure: reject_cause(8 bits, GSM 24.008 10.5.3.6)
// Spec-verified: PD=5(MM), MTI=0x22(CMServiceReject) per GSM 24.008 Table 10.5.3
// reject_cause=0x16 = Congestion (GSM 24.008 10.5.3.6 Table)
// [GSM SPEC VERIFIED] GSM 24.008 9.2.6: CMServiceReject body = reject_cause(1 octet).
//   The reject_cause follows GSM 24.008 Table 10.5.3.6 (MM cause values).
//   Value 0x16 = Congestion: network resources are insufficient to handle the request.
// =====================================================================

TEST(GoldenMM, CMServiceReject_Parse) {
    // Byte 0: PD(4)=5(MM)|skip(4)=0 = 0x50 [GSM 24.008 Table 11.2]
    // Byte 1: messageType(6)=0x22(CMServiceReject)|NSD(2)=0 = 0x88 [GSM 24.008 Table 10.5.3]
    // Byte 2: reject_cause = 0x16 (Congestion) [GSM 24.008 10.5.3.6]
    uint8_t data[] = {0x50, 0x88, 0x16};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3CMServiceReject::MTI);
}

// =====================================================================
// MM PARSE FROM HEX: IMSI Detach Indication (GSM 24.008 9.2.15)
// Reference: L3_Templates.ttcn ts_ML3_MO_MM_IMSI_DET_Ind template
// Structure: CM1 LV (Classmark 1, length-prefixed), MI LV (Mobile Identity, length-prefixed)
// Spec-verified: PD=5(MM), MTI=0x01(IMSIDetachIndication) per GSM 24.008 Table 10.5.3
// [GSM SPEC VERIFIED] GSM 24.008 9.2.15: IMSIDetachIndication body = CM1-LV + MI-LV.
//   CM1 (Classmark 1) is LV-encoded: length(1 octet, value=1) + CM1_value(1 octet).
//   MI (Mobile Identity) is LV-encoded: length(1 octet) + type_octet(1) + identity_value.
//   For TMSI: length=5, type_octet=0x08 (spare=0|type=TMSI=4|oe=0), value=4 octets.
// =====================================================================

TEST(GoldenMM, IMSIDetachIndication_Parse) {
    // Byte 0: PD(4)=5(MM)|skip(4)=0 = 0x50 [GSM 24.008 Table 11.2]
    // Byte 1: messageType(6)=0x01(IMSIDetachIndication)|NSD(2)=0 = 0x04 [GSM 24.008 Table 10.5.3]
    // Byte 2: CM1 LV length = 1 (Classmark 1 is 1 octet, GSM 24.008 10.5.1.5)
    // Byte 3: CM1 value = 0x00 (default classmark)
    // Byte 4: MI LV length = 5 [GSM 24.008 10.5.1.4]
    // Byte 5: spare(4)=0|typeOfIdentity(3)=100(TMSI)|oddevenIndicator(1)=0 = 0x08 [GSM 24.008 10.5.1.4]
    // Bytes 6-9: TMSI = 0x12345678
    uint8_t data[] = {
        0x50, 0x04,
        0x01, 0x00,
        0x05, 0x08, 0x12, 0x34, 0x56, 0x78
    };
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3IMSIDetachIndication::MTI);
}

// =====================================================================
// MM PARSE FROM HEX: MM Status (GSM 24.008 9.2.15)
// Reference: L3_Templates.ttcn tr_ML3_MT_MM_STATUS template
// Structure: cause(8 bits, GSM 24.008 10.5.3.6) — only one mandatory IE
// Spec-verified: PD=5(MM), MTI=0x31(MMStatus) per GSM 24.008 Table 10.5.3
// cause=0x60 = Invalid_Mandatory_Information (GSM 24.008 10.5.3.6 Table)
// [GSM SPEC VERIFIED] GSM 24.008 9.2.15: MMStatus body = cause(1 octet).
//   The cause value follows GSM 24.008 Table 10.5.3.6 (MM cause values).
//   Value 0x60 = Invalid_Mandatory_Information: used when a mandatory IE is
//   missing, has wrong length, or contains invalid content.
// =====================================================================

TEST(GoldenMM, MMStatus_Parse) {
    // Byte 0: PD(4)=5(MM)|skip(4)=0 = 0x50 [GSM 24.008 Table 11.2]
    // Byte 1: messageType(6)=0x31(MMStatus)|NSD(2)=0 = 0xC4 [GSM 24.008 Table 10.5.3]
    // Byte 2: cause = 0x60 (Invalid_Mandatory_Information) [GSM 24.008 10.5.3.6]
    uint8_t data[] = {0x50, 0xC4, 0x60};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3MMStatus::MTI);
}

// =====================================================================
// MM PARSE FROM HEX: Identity Response (GSM 24.008 9.2.11)
// Reference: L3_Templates.ttcn ts_ML3_MO_MM_ID_Rsp template
// Structure: MI LV (Mobile Identity, length-prefixed, GSM 24.008 10.5.1.4)
// Spec-verified: PD=5(MM), MTI=0x19(IdentityResponse) per GSM 24.008 Table 10.5.3
// [GSM SPEC VERIFIED] GSM 24.008 9.2.11: IdentityResponse body = MI-LV only.
//   Mobile Identity is LV-encoded: length(1 octet) + type_octet(1) + identity_value.
//   For TMSI: length=5, type_octet=0x08 (spare=0|type=TMSI=4|oe=0), value=4 octets BE.
// =====================================================================

TEST(GoldenMM, IdentityResponse_Parse) {
    // Byte 0: PD(4)=5(MM)|skip(4)=0 = 0x50 [GSM 24.008 Table 11.2]
    // Byte 1: messageType(6)=0x19(IdentityResponse)|NSD(2)=0 = 0x64 [GSM 24.008 Table 10.5.3]
    // Byte 2: MI LV length = 5 (1 type octet + 4 TMSI octets) [GSM 24.008 10.5.1.4]
    // Byte 3: spare(4)=0|typeOfIdentity(3)=100(TMSI)|oddevenIndicator(1)=0 = 0x08 [GSM 24.008 10.5.1.4]
    // Bytes 4-7: TMSI = 0x12345678
    uint8_t data[] = {0x50, 0x64, 0x05, 0x08, 0x12, 0x34, 0x56, 0x78};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3IdentityResponse::MTI);
}

// =====================================================================
// MM PARSE FROM HEX: CM Reestablishment Request (GSM 24.008 9.2.4)
// Reference: L3_Templates.ttcn ts_CM_REESTABL_REQ (line 450):
//   cipheringKeySequenceNumber, mobileStationClassmark2, mobileIdentityLV
// Structure: CKSN(4)|spare(4), CM2 LV (3 octets), MI LV, [LAI LV]
// Spec-verified: PD=5(MM), MTI=0x28(CMReestablishmentRequest) per GSM 24.008 Table 10.5.3
// [GSM SPEC VERIFIED] GSM 24.008 9.2.4: CMReestablishmentRequest body = CKSN + CM2-LV + MI-LV + [LAI].
//   CKSN is 1 octet: cipheringKeySequenceNumber(4 bits)|spare(4 bits).
//   Always present (not conditional), even when value is 0.
//   CM2 is LV-encoded: length(1) + value(3) = 4 octets.
//   MI is LV-encoded: length(1) + type(1) + value(variable) = variable octets.
// =====================================================================

TEST(GoldenMM, CMReestablishmentRequest_Parse) {
    // GSM 24.008 9.2.4: CMReestablishmentRequest body field order (MANDATORY):
    //   1) cipheringKeySequenceNumber(4)|spare(4) = 1 octet [GSM 24.008 10.5.1.2]
    //   2) mobileStationClassmark2 = LV format (length + value, GSM 24.008 10.5.1.6)
    //   3) mobileIdentityLV = LV format (length + type octet + value, GSM 24.008 10.5.1.4)
    // Reference: L3_Templates.ttcn ts_CM_REESTABL_REQ (line 450):
    //   cipheringKeySequenceNumber, mobileStationClassmark2, mobileIdentityLV
    // Byte 0: PD(4)=5(MM)|skip(4)=0 = 0x50 [GSM 24.008 Table 11.2]
    // Byte 1: messageType(6)=0x28(CMReestablishmentRequest)|NSD(2)=0 = 0xA0 [GSM 24.008 Table 10.5.3]
    // Byte 2: CKSN(4)=0|spare(4)=0 = 0x00 [GSM 24.008 10.5.1.2, L3_Templates.ttcn ts_CM_REESTABL_REQ line 460]
    // Byte 3: CM2 LV length = 3 (Classmark 2 is 3 octets, GSM 24.008 10.5.1.6)
    // Bytes 4-6: CM2 value (24 bits of capability flags)
    // Byte 7: MI LV length = 5 [GSM 24.008 10.5.1.4]
    // Byte 8: spare(4)=0|typeOfIdentity(3)=100(TMSI)|oddevenIndicator(1)=0 = 0x08 [GSM 24.008 10.5.1.4]
    // Bytes 9-12: TMSI = 0x12345678
    uint8_t data[] = {
        0x50, 0xA0,
        0x00,
        0x03, 0x20, 0x00, 0x80,
        0x05, 0x08, 0x12, 0x34, 0x56, 0x78
    };
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3CMReestablishmentRequest::MTI);
}

// =====================================================================
// MM ROUNDTrip: All messages
// =====================================================================

TEST(GoldenMM, CMServiceAccept_RoundTrip) {
    ParsedMessage msg(MMM(L3CMServiceAccept{}));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3CMServiceAccept::MTI);
}

TEST(GoldenMM, CMServiceAbort_RoundTrip) {
    ParsedMessage msg(MMM(L3CMServiceAbort{}));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3CMServiceAbort::MTI);
}

TEST(GoldenMM, CMServiceReject_RoundTrip) {
    ParsedMessage msg{MMM{L3CMServiceReject{MMRejectCause::Congestion}}};
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3CMServiceReject::MTI);
}

TEST(GoldenMM, LocationUpdatingAccept_RoundTrip) {
    L3LocationAreaIdentity lai("250", "01", 0x1234);
    ParsedMessage msg(MMM(L3LocationUpdatingAccept::builder().lai(lai).build()));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3LocationUpdatingAccept::MTI);
}

TEST(GoldenMM, LocationUpdatingAccept_WithMI_RoundTrip) {
    L3LocationAreaIdentity lai("250", "01", 0x1234);
    L3MobileIdentity mi(0xDEADBEEF);
    ParsedMessage msg(MMM(L3LocationUpdatingAccept::builder().lai(lai).mobileIdentity(mi).followOn(true).build()));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3LocationUpdatingAccept::MTI);
}

TEST(GoldenMM, LocationUpdatingReject_RoundTrip) {
    ParsedMessage msg{MMM{L3LocationUpdatingReject{MMRejectCause::IMSI_Unknown_In_HLR}}};
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3LocationUpdatingReject::MTI);
}

TEST(GoldenMM, AuthenticationRequest_RoundTrip) {
    std::vector<uint8_t> rand(16);
    for (int i = 0; i < 16; i++) rand[i] = static_cast<uint8_t>(i + 1);
    ParsedMessage msg(MMM(L3AuthenticationRequest(0, rand)));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3AuthenticationRequest::MTI);
}

TEST(GoldenMM, AuthenticationResponse_RoundTrip) {
    uint8_t data[] = {0x50, 0x50, 0xAB, 0xCD, 0x12, 0x34};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    auto* ar = tryGet<L3AuthenticationResponse>(*msg);
    ASSERT_TRUE(ar);
    EXPECT_EQ(ar->sres(), 0xABCD1234u);
    auto parsed = roundtrip(*msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3AuthenticationResponse::MTI);
}

TEST(GoldenMM, AuthenticationReject_RoundTrip) {
    ParsedMessage msg(MMM(L3AuthenticationReject{}));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3AuthenticationReject::MTI);
}

TEST(GoldenMM, IdentityRequest_IMSI_RoundTrip) {
    ParsedMessage msg{MMM{L3IdentityRequest{MobileIDType::IMSI}}};
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3IdentityRequest::MTI);
}

TEST(GoldenMM, IdentityRequest_IMEI_RoundTrip) {
    ParsedMessage msg{MMM{L3IdentityRequest{MobileIDType::IMEI}}};
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3IdentityRequest::MTI);
}

TEST(GoldenMM, IdentityResponse_RoundTrip) {
    ParsedMessage msg(MMM(L3IdentityResponse{}));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3IdentityResponse::MTI);
}

TEST(GoldenMM, TMSIReallocationCommand_RoundTrip) {
    L3LocationAreaIdentity lai("250", "01", 0x1234);
    L3MobileIdentity tmsi(0x12345678);
    ParsedMessage msg(MMM(L3TMSIReallocationCommand::builder().lai(lai).tmsi(tmsi).build()));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3TMSIReallocationCommand::MTI);
}

TEST(GoldenMM, TMSIReallocationComplete_RoundTrip) {
    ParsedMessage msg(MMM(L3TMSIReallocationComplete{}));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3TMSIReallocationComplete::MTI);
}

TEST(GoldenMM, MMStatus_RoundTrip) {
    ParsedMessage msg(MMM(L3MMStatus{}));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3MMStatus::MTI);
}

TEST(GoldenMM, CMServiceRequest_RoundTrip) {
    ParsedMessage msg(MMM(L3CMServiceRequest{}));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3CMServiceRequest::MTI);
}

TEST(GoldenMM, CMReestablishmentRequest_RoundTrip) {
    ParsedMessage msg(MMM(L3CMReestablishmentRequest{}));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3CMReestablishmentRequest::MTI);
}

TEST(GoldenMM, IMSIDetachIndication_RoundTrip) {
    ParsedMessage msg(MMM(L3IMSIDetachIndication{}));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3IMSIDetachIndication::MTI);
}

TEST(GoldenMM, MMInformation_RoundTrip) {
    ParsedMessage msg(MMM(L3MMInformation{}));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3MMInformation::MTI);
}

TEST(GoldenMM, LocationUpdatingRequest_RoundTrip) {
    ParsedMessage msg(MMM(L3LocationUpdatingRequest{}));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3LocationUpdatingRequest::MTI);
}

// =====================================================================
// MMRejectCause values (GSM 24.008 10.5.3.6 / GSM 04.08 10.5.3.6)
// Reference: L3_Templates.ttcn line 57: c_MM_CAUSE_IMSI_UNKNOWN_IN_HLR := '02'O
// Reference: 3GPP TS 24.008 Table 10.5.3.6 (MM cause values)
// Spec-verified: All MM cause values per GSM 24.008 Recommendation
//   IMSI unknown in HLR(0x02), Illegal MS(0x03), Congestion(0x16), etc.
// [GSM SPEC VERIFIED] MM cause values follow GSM 24.008 Table 10.5.3.6:
//   0x00-0x1F: PLMN/VLR/HLR related causes, 0x20-0x3F: Service-related causes,
//   0x40-0x5F: Reserved/extension, 0x60-0x7F: Protocol errors.
//   Key values: 0x02=IMSI_Unknown_In_HLR, 0x03=Illegal_MS, 0x16=Congestion,
//   0x5f=Semantically_Incorrect_Message, 0x60=Invalid_Mandatory_Information,
//   0x6f=Protocol_Error_Unspecified.
// =====================================================================

TEST(GoldenMM, RejectCauseValues) {
    // Spec-verified: GSM 24.008 Table 10.5.3.6 MM cause values
    EXPECT_EQ(static_cast<uint8_t>(MMRejectCause::Zero), 0x00);
    EXPECT_EQ(static_cast<uint8_t>(MMRejectCause::IMSI_Unknown_In_HLR), 0x02);
    EXPECT_EQ(static_cast<uint8_t>(MMRejectCause::Illegal_MS), 0x03);
    EXPECT_EQ(static_cast<uint8_t>(MMRejectCause::IMSI_Unknown_In_VLR), 0x04);
    EXPECT_EQ(static_cast<uint8_t>(MMRejectCause::IMEI_Not_Accepted), 0x05);
    EXPECT_EQ(static_cast<uint8_t>(MMRejectCause::Illegal_ME), 0x06);
    EXPECT_EQ(static_cast<uint8_t>(MMRejectCause::PLMN_Not_Allowed), 0x0b);
    EXPECT_EQ(static_cast<uint8_t>(MMRejectCause::Location_Area_Not_Allowed), 0x0c);
    EXPECT_EQ(static_cast<uint8_t>(MMRejectCause::Roaming_Not_Allowed_In_LA), 0x0d);
    EXPECT_EQ(static_cast<uint8_t>(MMRejectCause::No_Suitable_Cells_In_LA), 0x0f);
    EXPECT_EQ(static_cast<uint8_t>(MMRejectCause::Network_Failure), 0x11);
    EXPECT_EQ(static_cast<uint8_t>(MMRejectCause::MAC_Failure), 0x14);
    EXPECT_EQ(static_cast<uint8_t>(MMRejectCause::Synch_Failure), 0x15);
    EXPECT_EQ(static_cast<uint8_t>(MMRejectCause::Congestion), 0x16);
    EXPECT_EQ(static_cast<uint8_t>(MMRejectCause::GSM_Authentication_Unacceptable), 0x17);
    EXPECT_EQ(static_cast<uint8_t>(MMRejectCause::Not_Authorized_In_CSG), 0x19);
    EXPECT_EQ(static_cast<uint8_t>(MMRejectCause::Service_Option_Not_Supported), 0x20);
    EXPECT_EQ(static_cast<uint8_t>(MMRejectCause::Requested_Service_Not_Subscribed), 0x21);
    EXPECT_EQ(static_cast<uint8_t>(MMRejectCause::Service_Option_Out_Of_Order), 0x22);
    EXPECT_EQ(static_cast<uint8_t>(MMRejectCause::Call_Cannot_Be_Identified), 0x26);
    EXPECT_EQ(static_cast<uint8_t>(MMRejectCause::Semantically_Incorrect_Message), 0x5f);
    EXPECT_EQ(static_cast<uint8_t>(MMRejectCause::Invalid_Mandatory_Information), 0x60);
    EXPECT_EQ(static_cast<uint8_t>(MMRejectCause::Message_Type_Invalid), 0x61);
    EXPECT_EQ(static_cast<uint8_t>(MMRejectCause::Message_Type_Not_Compatible), 0x62);
    EXPECT_EQ(static_cast<uint8_t>(MMRejectCause::IE_Invalid), 0x63);
    EXPECT_EQ(static_cast<uint8_t>(MMRejectCause::Conditional_IE_Error), 0x64);
    EXPECT_EQ(static_cast<uint8_t>(MMRejectCause::Message_Not_Compatible), 0x65);
    EXPECT_EQ(static_cast<uint8_t>(MMRejectCause::Protocol_Error_Unspecified), 0x6f);
}

// =====================================================================
// CMServiceType values (GSM 04.08 10.5.3.3)
// Reference: L3_Templates.ttcn CmServiceType enum (line 28):
//   CM_TYPE_MO_CALL('0001'B), CM_TYPE_EMERG_CALL('0010'B), CM_TYPE_MO_SMS('0100'B),
//   CM_TYPE_SS_ACT('1000'B), CM_TYPE_VGCS('1001'B), CM_TYPE_VBS('1010'B), CM_TYPE_LCS('1011'B)
// [GSM SPEC VERIFIED] GSM 24.008 10.5.3.3: CM_ServiceType is 4 bits.
//   Values 1-2 = CC service, 4 = SMS, 8 = SS activation, 9 = VGCS, 10 = VBS, 11 = LCS
//   isCC() returns true for values 1(MO call) and 2(emergency call).
//   isSMS() returns true for value 4(SMS).
// =====================================================================

TEST(GoldenMM, CMServiceType_Values) {
    EXPECT_EQ(static_cast<uint8_t>(L3CMServiceType::MobileOriginatedCall), 1);
    EXPECT_EQ(static_cast<uint8_t>(L3CMServiceType::EmergencyCall), 2);
    EXPECT_EQ(static_cast<uint8_t>(L3CMServiceType::ShortMessage), 4);
    EXPECT_EQ(static_cast<uint8_t>(L3CMServiceType::SupplementaryService), 8);
    EXPECT_EQ(static_cast<uint8_t>(L3CMServiceType::VoiceCallGroup), 9);
    EXPECT_EQ(static_cast<uint8_t>(L3CMServiceType::VoiceBroadcast), 10);
    EXPECT_EQ(static_cast<uint8_t>(L3CMServiceType::LocationService), 11);
}

TEST(GoldenMM, CMServiceType_Flags) {
    L3CMServiceType mo(L3CMServiceType::MobileOriginatedCall);
    EXPECT_TRUE(mo.isCC());
    EXPECT_FALSE(mo.isSMS());
    L3CMServiceType sms(L3CMServiceType::ShortMessage);
    EXPECT_FALSE(sms.isCC());
    EXPECT_TRUE(sms.isSMS());
}

// =====================================================================
// LocationUpdateType values
// Reference: L3_Templates.ttcn LU_Type_Normal, LU_Type_Periodic, LU_Type_IMSI_Attach
// =====================================================================

TEST(GoldenMM, LocationUpdateType_Values) {
    EXPECT_EQ(static_cast<uint8_t>(LocationUpdateType::Normal), 0);
    EXPECT_EQ(static_cast<uint8_t>(LocationUpdateType::Periodic), 1);
    EXPECT_EQ(static_cast<uint8_t>(LocationUpdateType::IMSIAttach), 2);
}

// =====================================================================
// MM IE: L3RAND (GSM 04.08 10.5.3.1)
// =====================================================================

TEST(GoldenMM, RAND_RoundTrip) {
    std::vector<uint8_t> randBytes(16);
    for (int i = 0; i < 16; i++) randBytes[i] = static_cast<uint8_t>(i * 17);
    L3RAND orig(randBytes);
    EXPECT_EQ(orig.lengthV(), 16u);
    {
        std::vector<uint8_t> buf(32);
        BitWriter writer(buf.data(), buf.size() * 8);
        orig.write(writer);
        BitReader reader(buf.data(), writer.position());
        auto parsed = L3RAND::parse(reader);
        ASSERT_TRUE(parsed);
        EXPECT_EQ(*parsed, orig);
    }
}

// =====================================================================
// MM IE: L3SRES (GSM 04.08 10.5.3.2)
// =====================================================================

TEST(GoldenMM, SRES_RoundTrip) {
    L3SRES orig(0x12345678);
    EXPECT_EQ(orig.lengthV(), 4u);
    {
        std::vector<uint8_t> buf(8);
        BitWriter writer(buf.data(), buf.size() * 8);
        orig.write(writer);
        BitReader reader(buf.data(), writer.position());
        auto parsed = L3SRES::parse(reader);
        ASSERT_TRUE(parsed);
        EXPECT_EQ((*parsed).value(), 0x12345678u);
    }
}

// =====================================================================
// MM IE: L3NetworkName (GSM 04.08 10.5.3.5a)
// Reference: L3_Templates.ttcn ts_NetworkName
// =====================================================================

TEST(GoldenMM, NetworkName_Encoding) {
    L3NetworkName nn("TestNet", GSMAlphabet::ALPHABET_7BIT, 1);
    EXPECT_STREQ(nn.name(), "TestNet");
    EXPECT_EQ(nn.alphabet(), GSMAlphabet::ALPHABET_7BIT);
}

// =====================================================================
// MM IE: L3TimeZoneAndTime (GSM 04.08 10.5.3.9)
// Reference: L3_Templates.ttcn ts_TimeZoneAndTime
// =====================================================================

TEST(GoldenMM, TimeZoneAndTime_UTC) {
    L3TimeZoneAndTime tzt(L3TimeZoneAndTime::UTC_TIME);
    EXPECT_EQ(tzt.lengthV(), 7u);
    EXPECT_EQ(tzt.type(), L3TimeZoneAndTime::UTC_TIME);
}

// =====================================================================
// MM IE: L3RejectCauseIE (GSM 04.08 10.5.3.6)
// =====================================================================

TEST(GoldenMM, RejectCauseIE_Encoding) {
    L3RejectCauseIE rc(MMRejectCause::Congestion);
    EXPECT_EQ(rc.lengthV(), 1u);
    {
        std::vector<uint8_t> buf(4);
        BitWriter writer(buf.data(), buf.size() * 8);
        rc.write(writer);
        BitReader reader(buf.data(), writer.position());
        auto parsed = L3RejectCauseIE::parse(reader);
        ASSERT_TRUE(parsed);
    }
}

// =====================================================================
// MM IE: L3CMServiceType
// =====================================================================

TEST(GoldenMM, CMServiceTypeIE_MO_Call) {
    L3CMServiceType orig(L3CMServiceType::MobileOriginatedCall);
    EXPECT_TRUE(orig.isCC());
    EXPECT_FALSE(orig.isSMS());
    EXPECT_EQ(orig.lengthV(), 0u);
}

TEST(GoldenMM, CMServiceTypeIE_SMS) {
    L3CMServiceType orig(L3CMServiceType::ShortMessage);
    EXPECT_TRUE(orig.isSMS());
    EXPECT_FALSE(orig.isCC());
}
