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

#include <gtest/gtest.h>
#include <gsml3parser/parser.h>
#include <gsml3parser/mm/l3mmmessages.h>
#include <gsml3parser/common/l3common.h>

using namespace gsml3parser;

static std::unique_ptr<L3Message> roundtrip(const L3Message& msg) {
    std::vector<uint8_t> buf(msg.fullLength());
    size_t n = writeL3(msg, buf.data(), buf.size());
    if (n == 0) return nullptr;
    return parseL3(buf.data(), n);
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
// =====================================================================

TEST(GoldenMM, MessageTypeValues) {
    // Spec-verified: GSM 24.008 Table 10.5.3 MM message type identifier values
    EXPECT_EQ(L3MMMessage::IMSIDetachIndication, 0x01);      // '000001'B - GSM 24.008 9.2.15
    EXPECT_EQ(L3MMMessage::LocationUpdatingAccept, 0x02);    // '000010'B - GSM 24.008 9.2.13
    EXPECT_EQ(L3MMMessage::LocationUpdatingReject, 0x04);    // '000100'B - GSM 24.008 9.2.14
    EXPECT_EQ(L3MMMessage::LocationUpdatingRequest, 0x08);   // '001000'B - GSM 24.008 9.2.15
    EXPECT_EQ(L3MMMessage::AuthenticationRequest, 0x12);     // '010010'B - GSM 24.008 9.2.1
    EXPECT_EQ(L3MMMessage::AuthenticationResponse, 0x14);    // '010100'B - GSM 24.008 9.2.1
    EXPECT_EQ(L3MMMessage::AuthenticationReject, 0x11);      // '010001'B - GSM 24.008 9.2.1
    EXPECT_EQ(L3MMMessage::IdentityRequest, 0x18);           // '011000'B - GSM 24.008 9.2.10
    EXPECT_EQ(L3MMMessage::IdentityResponse, 0x19);          // '011001'B - GSM 24.008 9.2.11
    EXPECT_EQ(L3MMMessage::TMSIReallocationCommand, 0x1a);   // '011010'B - GSM 24.008 9.2.17
    EXPECT_EQ(L3MMMessage::TMSIReallocationComplete, 0x1b);  // '011011'B - GSM 24.008 9.2.18
    EXPECT_EQ(L3MMMessage::CMServiceAccept, 0x21);           // '100001'B - GSM 24.008 9.2.5
    EXPECT_EQ(L3MMMessage::CMServiceReject, 0x22);           // '100010'B - GSM 24.008 9.2.6
    EXPECT_EQ(L3MMMessage::CMServiceAbort, 0x23);            // '100011'B - GSM 24.008 9.2.7
    EXPECT_EQ(L3MMMessage::CMServiceRequest, 0x24);          // '100100'B - GSM 24.008 9.2.9
    EXPECT_EQ(L3MMMessage::CMReestablishmentRequest, 0x28);  // '101000'B - GSM 24.008 9.2.4
    EXPECT_EQ(L3MMMessage::MMInformation, 0x32);             // '110010'B - GSM 24.008 9.2.15
    EXPECT_EQ(L3MMMessage::MMStatus, 0x31);                  // '110001'B - GSM 24.008 9.2.15
}

// =====================================================================
// MM PARSE FROM HEX: Location Updating Request (GSM 24.008 9.2.15)
// Reference: L3_Templates.ttcn ts_LU_REQ (line 356):
//   discriminator := '0101'B (PD=5=MM), messageType := overwritten
//   locationUpdatingType := lu_type, cipheringKeySequenceNumber
//   mobileStationClassmark1 := ts_CM1, mobileIdentityLV := mi_lv
// Structure: LU_Type(2)|spare(2)|CKSN(4), CM1 LV, MI LV, [LAI LV]
// Spec-verified: PD=5(MM), MTI=0x08(LocationUpdatingRequest) per GSM 24.008 Table 10.5.3
// =====================================================================

TEST(GoldenMM, LocationUpdatingRequest_Parse) {
    // Byte 0: PD(4)=5(MM)|skip(4)=0 = 0x50 [GSM 24.008 Table 11.2]
    // Byte 1: messageType(6)=0x08(LocationUpdatingRequest)|NSD(2)=0 = 0x20 [GSM 24.008 Table 10.5.3]
    // Byte 2: LU_Type(2)=00(Normal)|spare(2)=0|CKSN(4)=0 = 0x00 [L3_Templates.ttcn LU_Type_Normal line 329]
    // Byte 3: CM1 LV length = 1 (Classmark 1 is 1 octet, GSM 24.008 10.5.1.5)
    // Byte 4: CM1 value = 0x00 (default classmark)
    // Byte 5: MI LV length = 5 (1 type octet + 4 TMSI octets, GSM 24.008 10.5.1.4)
    // Byte 6: spare(4)=0|type(3)=100(TMSI)|oe(1)=0(old) = 0x0C [GSM 24.008 10.5.1.4]
    // Bytes 7-10: TMSI = 0x12345678 (4 octets, MSB first)
    uint8_t data[] = {
        0x50, 0x20, 0x00,
        0x01, 0x00,
        0x05, 0x0C, 0x12, 0x34, 0x56, 0x78
    };
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->MTI(), L3MMMessage::LocationUpdatingRequest);
}

// =====================================================================
// MM PARSE FROM HEX: Location Updating Accept (GSM 24.008 9.2.13)
// Reference: L3_Templates.ttcn ts_LU_ACCEPT (line 385):
//   discriminator := '0101'B (PD=5=MM), messageType := overwritten
//   locationAreaIdentification := {mcc_mnc, lac}
// Structure: LAI(5 octets, GSM 24.008 10.5.1.3), [MI TLV], [FOP TV]
// Spec-verified: PD=5(MM), MTI=0x02(LocationUpdatingAccept) per GSM 24.008 Table 10.5.3
// LAI encoding: GSM_Types.ttcn f_build_BcdMccMnc (line 470):
//   MCC=250, MNC=01 -> '250F01'H (MNC padded with F) -> nibble-swapped -> 0x52, 0xF0, 0x10
// =====================================================================

TEST(GoldenMM, LocationUpdatingAccept_Parse) {
    // Byte 0: PD(4)=5(MM)|skip(4)=0 = 0x50 [GSM 24.008 Table 11.2]
    // Byte 1: messageType(6)=0x02(LocationUpdatingAccept)|NSD(2)=0 = 0x08 [GSM 24.008 Table 10.5.3]
    // Bytes 2-4: LAI MCC/MNC BCD: MCC=250, MNC=01 -> '250F01'H nibble-swapped = {0x52, 0xF0, 0x10}
    //   [GSM 24.008 10.5.1.3: digit2/digit1 pairs, LSB-first nibble order]
    // Bytes 5-6: LAI LAC = 0x1234 (MSB first)
    uint8_t data[] = {0x50, 0x08, 0x52, 0xF0, 0x10, 0x12, 0x34};
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->MTI(), L3MMMessage::LocationUpdatingAccept);
}

// =====================================================================
// MM PARSE FROM HEX: TMSI Reallocation Command (GSM 24.008 9.2.17)
// Reference: L3_Templates.ttcn ts_TMSI_REALLOC_CM template
// Structure: LAI(5 octets), MI LV (length + MobileIdentity), FollowOnProceed(4)|spare(4)
// Spec-verified: PD=5(MM), MTI=0x1A(TMSIReallocationCommand) per GSM 24.008 Table 10.5.3
// =====================================================================

TEST(GoldenMM, TMSIReallocationCommand_Parse) {
    // Byte 0: PD(4)=5(MM)|skip(4)=0 = 0x50 [GSM 24.008 Table 11.2]
    // Byte 1: messageType(6)=0x1A(TMSIReallocationCommand)|NSD(2)=0 = 0x68 [GSM 24.008 Table 10.5.3]
    // Bytes 2-4: LAI MCC/MNC BCD: MCC=250, MNC=01 -> {0x52, 0xF0, 0x10} [GSM 24.008 10.5.1.3]
    // Bytes 5-6: LAI LAC = 0x1234
    // Byte 7: MI LV length = 5 (1 type octet + 4 TMSI octets) [GSM 24.008 10.5.1.4]
    // Byte 8: spare(4)=0|type(3)=100(TMSI)|oe(1)=0 = 0x0C
    // Bytes 9-12: TMSI = 0x87654321 (new TMSI assigned by network)
    // Byte 13: FollowOnProceed(4)=0|spare(4)=0 = 0x00 [GSM 24.008 10.5.2.38]
    uint8_t data[] = {
        0x50, 0x68,
        0x52, 0xF0, 0x10, 0x12, 0x34,
        0x05, 0x0C, 0x87, 0x65, 0x43, 0x21,
        0x00
    };
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->MTI(), L3MMMessage::TMSIReallocationCommand);
}

// =====================================================================
// MM PARSE FROM HEX: CM Service Request (GSM 24.008 9.2.9)
// Reference: L3_Templates.ttcn ts_CM_SERV_REQ (line 411):
//   cm_ServiceType := int2bit(enum2int(serv_type), 4)
//   cipheringKeySequenceNumber, mobileStationClassmark2, mobileIdentity
// Structure: CM_ServiceType(4)|CKSN(4), CM2 LV (3 octets), MI LV
// Spec-verified: PD=5(MM), MTI=0x24(CMServiceRequest) per GSM 24.008 Table 10.5.3
// CmServiceType: L3_Templates.ttcn line 28: CM_TYPE_MO_CALL = '0001'B (value=1)
// =====================================================================

TEST(GoldenMM, CMServiceRequest_Parse) {
    // Byte 0: PD(4)=5(MM)|skip(4)=0 = 0x50 [GSM 24.008 Table 11.2]
    // Byte 1: messageType(6)=0x24(CMServiceRequest)|NSD(2)=0 = 0x90 [GSM 24.008 Table 10.5.3]
    // Byte 2: CM_ServiceType(4)=1(MobileOriginatedCall)|CKSN(4)=0 = 0x01 [GSM 24.008 10.5.3.3]
    //   L3_Templates.ttcn CmServiceType: CM_TYPE_MO_CALL = '0001'B (line 29)
    // Byte 3: CM2 LV length = 3 (Classmark 2 is 3 octets, GSM 24.008 10.5.1.6)
    // Bytes 4-6: CM2 value (24 bits of capability flags)
    // Byte 7: MI LV length = 5 [GSM 24.008 10.5.1.4]
    // Byte 8: spare(4)=0|type(3)=100(TMSI)|oe(1)=0 = 0x0C
    // Bytes 9-12: TMSI = 0x12345678
    uint8_t data[] = {
        0x50, 0x90, 0x01,
        0x03, 0x20, 0x00, 0x80,
        0x05, 0x0C, 0x12, 0x34, 0x56, 0x78
    };
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->MTI(), L3MMMessage::CMServiceRequest);
}

// =====================================================================
// MM PARSE FROM HEX: CM Service Reject (GSM 24.008 9.2.6)
// Reference: L3_Templates.ttcn tr_CM_SERV_REJ (line 524):
//   messageType := '100010'B (MTI=0x22), rejectCause := rej_cause
// Structure: reject_cause(8 bits, GSM 24.008 10.5.3.6)
// Spec-verified: PD=5(MM), MTI=0x22(CMServiceReject) per GSM 24.008 Table 10.5.3
// reject_cause=0x16 = Congestion (GSM 24.008 10.5.3.6 Table)
// =====================================================================

TEST(GoldenMM, CMServiceReject_Parse) {
    // Byte 0: PD(4)=5(MM)|skip(4)=0 = 0x50 [GSM 24.008 Table 11.2]
    // Byte 1: messageType(6)=0x22(CMServiceReject)|NSD(2)=0 = 0x88 [GSM 24.008 Table 10.5.3]
    // Byte 2: reject_cause = 0x16 (Congestion) [GSM 24.008 10.5.3.6]
    uint8_t data[] = {0x50, 0x88, 0x16};
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->MTI(), L3MMMessage::CMServiceReject);
}

// =====================================================================
// MM PARSE FROM HEX: IMSI Detach Indication (GSM 24.008 9.2.15)
// Reference: L3_Templates.ttcn ts_ML3_MO_MM_IMSI_DET_Ind template
// Structure: CM1 LV (Classmark 1, length-prefixed), MI LV (Mobile Identity, length-prefixed)
// Spec-verified: PD=5(MM), MTI=0x01(IMSIDetachIndication) per GSM 24.008 Table 10.5.3
// =====================================================================

TEST(GoldenMM, IMSIDetachIndication_Parse) {
    // Byte 0: PD(4)=5(MM)|skip(4)=0 = 0x50 [GSM 24.008 Table 11.2]
    // Byte 1: messageType(6)=0x01(IMSIDetachIndication)|NSD(2)=0 = 0x04 [GSM 24.008 Table 10.5.3]
    // Byte 2: CM1 LV length = 1 (Classmark 1 is 1 octet, GSM 24.008 10.5.1.5)
    // Byte 3: CM1 value = 0x00 (default classmark)
    // Byte 4: MI LV length = 5 [GSM 24.008 10.5.1.4]
    // Byte 5: spare(4)=0|type(3)=100(TMSI)|oe(1)=0 = 0x0C
    // Bytes 6-9: TMSI = 0x12345678
    uint8_t data[] = {
        0x50, 0x04,
        0x01, 0x00,
        0x05, 0x0C, 0x12, 0x34, 0x56, 0x78
    };
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->MTI(), L3MMMessage::IMSIDetachIndication);
}

// =====================================================================
// MM PARSE FROM HEX: MM Status (GSM 24.008 9.2.15)
// Reference: L3_Templates.ttcn tr_ML3_MT_MM_STATUS template
// Structure: cause(8 bits, GSM 24.008 10.5.3.6), spare(8), spare(8)
// Spec-verified: PD=5(MM), MTI=0x31(MMStatus) per GSM 24.008 Table 10.5.3
// cause=0x60 = Invalid_Mandatory_Information (GSM 24.008 10.5.3.6 Table)
// =====================================================================

TEST(GoldenMM, MMStatus_Parse) {
    // Byte 0: PD(4)=5(MM)|skip(4)=0 = 0x50 [GSM 24.008 Table 11.2]
    // Byte 1: messageType(6)=0x31(MMStatus)|NSD(2)=0 = 0xC4 [GSM 24.008 Table 10.5.3]
    // Byte 2: cause = 0x60 (Invalid_Mandatory_Information) [GSM 24.008 10.5.3.6]
    // Byte 3: spare = 0x00
    // Byte 4: spare = 0x00
    uint8_t data[] = {0x50, 0xC4, 0x60, 0x00, 0x00};
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->MTI(), L3MMMessage::MMStatus);
}

// =====================================================================
// MM PARSE FROM HEX: Identity Response (GSM 24.008 9.2.11)
// Reference: L3_Templates.ttcn ts_ML3_MO_MM_ID_Rsp template
// Structure: MI LV (Mobile Identity, length-prefixed, GSM 24.008 10.5.1.4)
// Spec-verified: PD=5(MM), MTI=0x19(IdentityResponse) per GSM 24.008 Table 10.5.3
// =====================================================================

TEST(GoldenMM, IdentityResponse_Parse) {
    // Byte 0: PD(4)=5(MM)|skip(4)=0 = 0x50 [GSM 24.008 Table 11.2]
    // Byte 1: messageType(6)=0x19(IdentityResponse)|NSD(2)=0 = 0x64 [GSM 24.008 Table 10.5.3]
    // Byte 2: MI LV length = 5 (1 type octet + 4 TMSI octets) [GSM 24.008 10.5.1.4]
    // Byte 3: spare(4)=0|type(3)=100(TMSI)|oe(1)=0 = 0x0C
    // Bytes 4-7: TMSI = 0x12345678
    uint8_t data[] = {0x50, 0x64, 0x05, 0x0C, 0x12, 0x34, 0x56, 0x78};
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->MTI(), L3MMMessage::IdentityResponse);
}

// =====================================================================
// MM PARSE FROM HEX: CM Reestablishment Request (GSM 24.008 9.2.4)
// Reference: L3_Templates.ttcn ts_CM_REESTABL_REQ (line 450):
//   cipheringKeySequenceNumber, mobileStationClassmark2, mobileIdentityLV
// Structure: CKSN(4)|spare(4), CM2 LV (3 octets), MI LV, [LAI LV]
// Spec-verified: PD=5(MM), MTI=0x28(CMReestablishmentRequest) per GSM 24.008 Table 10.5.3
// =====================================================================

TEST(GoldenMM, CMReestablishmentRequest_Parse) {
    // Byte 0: PD(4)=5(MM)|skip(4)=0 = 0x50 [GSM 24.008 Table 11.2]
    // Byte 1: messageType(6)=0x28(CMReestablishmentRequest)|NSD(2)=0 = 0xA0 [GSM 24.008 Table 10.5.3]
    // Byte 2: CM2 LV length = 3 (Classmark 2 is 3 octets, GSM 24.008 10.5.1.6)
    // Bytes 3-5: CM2 value (24 bits of capability flags)
    // Byte 6: MI LV length = 5 [GSM 24.008 10.5.1.4]
    // Byte 7: spare(4)=0|type(3)=100(TMSI)|oe(1)=0 = 0x0C
    // Bytes 8-11: TMSI = 0x12345678
    uint8_t data[] = {
        0x50, 0xA0,
        0x03, 0x20, 0x00, 0x80,
        0x05, 0x0C, 0x12, 0x34, 0x56, 0x78
    };
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->MTI(), L3MMMessage::CMReestablishmentRequest);
}

// =====================================================================
// MM ROUNDTrip: All messages
// =====================================================================

TEST(GoldenMM, CMServiceAccept_RoundTrip) {
    L3CMServiceAccept msg;
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3MMMessage::CMServiceAccept);
}

TEST(GoldenMM, CMServiceAbort_RoundTrip) {
    L3CMServiceAbort msg;
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3MMMessage::CMServiceAbort);
}

TEST(GoldenMM, CMServiceReject_RoundTrip) {
    L3CMServiceReject msg(MMRejectCause::Congestion);
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3MMMessage::CMServiceReject);
}

TEST(GoldenMM, LocationUpdatingAccept_RoundTrip) {
    L3LocationAreaIdentity lai("250", "01", 0x1234);
    L3LocationUpdatingAccept msg(lai);
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3MMMessage::LocationUpdatingAccept);
}

TEST(GoldenMM, LocationUpdatingAccept_WithMI_RoundTrip) {
    L3LocationAreaIdentity lai("250", "01", 0x1234);
    L3MobileIdentity mi(0xDEADBEEF);
    L3LocationUpdatingAccept msg(lai, mi, true);
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3MMMessage::LocationUpdatingAccept);
}

TEST(GoldenMM, LocationUpdatingReject_RoundTrip) {
    L3LocationUpdatingReject msg(MMRejectCause::IMSI_Unknown_In_HLR);
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3MMMessage::LocationUpdatingReject);
}

TEST(GoldenMM, AuthenticationRequest_RoundTrip) {
    std::vector<uint8_t> rand(16);
    for (int i = 0; i < 16; i++) rand[i] = static_cast<uint8_t>(i + 1);
    L3AuthenticationRequest msg(0, rand);
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3MMMessage::AuthenticationRequest);
}

TEST(GoldenMM, AuthenticationResponse_RoundTrip) {
    uint8_t data[] = {0x50, 0x50, 0xAB, 0xCD, 0x12, 0x34};
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    auto* ar = dynamic_cast<L3AuthenticationResponse*>(msg.get());
    ASSERT_TRUE(ar);
    EXPECT_EQ(ar->SRES(), 0xABCD1234u);
    auto parsed = roundtrip(*ar);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3MMMessage::AuthenticationResponse);
}

TEST(GoldenMM, AuthenticationReject_RoundTrip) {
    L3AuthenticationReject msg;
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3MMMessage::AuthenticationReject);
}

TEST(GoldenMM, IdentityRequest_IMSI_RoundTrip) {
    L3IdentityRequest msg(MobileIDType::IMSI);
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3MMMessage::IdentityRequest);
}

TEST(GoldenMM, IdentityRequest_IMEI_RoundTrip) {
    L3IdentityRequest msg(MobileIDType::IMEI);
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3MMMessage::IdentityRequest);
}

TEST(GoldenMM, IdentityResponse_RoundTrip) {
    L3IdentityResponse msg;
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3MMMessage::IdentityResponse);
}

TEST(GoldenMM, TMSIReallocationCommand_RoundTrip) {
    L3LocationAreaIdentity lai("250", "01", 0x1234);
    L3MobileIdentity tmsi(0x12345678);
    L3TMSIReallocationCommand msg(lai, tmsi, false);
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3MMMessage::TMSIReallocationCommand);
}

TEST(GoldenMM, TMSIReallocationComplete_RoundTrip) {
    L3TMSIReallocationComplete msg;
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3MMMessage::TMSIReallocationComplete);
}

TEST(GoldenMM, MMStatus_RoundTrip) {
    L3MMStatus msg;
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3MMMessage::MMStatus);
}

TEST(GoldenMM, CMServiceRequest_RoundTrip) {
    L3CMServiceRequest msg;
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3MMMessage::CMServiceRequest);
}

TEST(GoldenMM, CMReestablishmentRequest_RoundTrip) {
    L3CMReestablishmentRequest msg;
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3MMMessage::CMReestablishmentRequest);
}

TEST(GoldenMM, IMSIDetachIndication_RoundTrip) {
    L3IMSIDetachIndication msg;
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3MMMessage::IMSIDetachIndication);
}

TEST(GoldenMM, MMInformation_RoundTrip) {
    L3MMInformation msg;
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3MMMessage::MMInformation);
}

TEST(GoldenMM, LocationUpdatingRequest_RoundTrip) {
    L3LocationUpdatingRequest msg;
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3MMMessage::LocationUpdatingRequest);
}

// =====================================================================
// MMRejectCause values (GSM 24.008 10.5.3.6 / GSM 04.08 10.5.3.6)
// Reference: L3_Templates.ttcn line 57: c_MM_CAUSE_IMSI_UNKNOWN_IN_HLR := '02'O
// Reference: 3GPP TS 24.008 Table 10.5.3.6 (MM cause values)
// Spec-verified: All MM cause values per GSM 24.008 Recommendation
//   IMSI unknown in HLR(0x02), Illegal MS(0x03), Congestion(0x16), etc.
// =====================================================================

TEST(GoldenMM, RejectCauseValues) {
    // Spec-verified: GSM 24.008 Table 10.5.3.6 MM cause values
    EXPECT_EQ(static_cast<uint8_t>(MMRejectCause::Zero), 0x00);                           // Unspecified
    EXPECT_EQ(static_cast<uint8_t>(MMRejectCause::IMSI_Unknown_In_HLR), 0x02);           // IMSI unknown in HLR
    EXPECT_EQ(static_cast<uint8_t>(MMRejectCause::Illegal_MS), 0x03);                    // Illegal MS
    EXPECT_EQ(static_cast<uint8_t>(MMRejectCause::IMSI_Unknown_In_VLR), 0x04);          // IMSI unknown in VLR
    EXPECT_EQ(static_cast<uint8_t>(MMRejectCause::IMEI_Not_Accepted), 0x05);            // IMEI not accepted
    EXPECT_EQ(static_cast<uint8_t>(MMRejectCause::Illegal_ME), 0x06);                   // Illegal ME
    EXPECT_EQ(static_cast<uint8_t>(MMRejectCause::PLMN_Not_Allowed), 0x0b);             // PLMN not allowed
    EXPECT_EQ(static_cast<uint8_t>(MMRejectCause::Location_Area_Not_Allowed), 0x0c);    // Location area not allowed
    EXPECT_EQ(static_cast<uint8_t>(MMRejectCause::Roaming_Not_Allowed_In_LA), 0x0d);    // Roaming not allowed in LA
    EXPECT_EQ(static_cast<uint8_t>(MMRejectCause::No_Suitable_Cells_In_LA), 0x0f);      // No suitable cells in LA
    EXPECT_EQ(static_cast<uint8_t>(MMRejectCause::Network_Failure), 0x11);              // Network failure
    EXPECT_EQ(static_cast<uint8_t>(MMRejectCause::MAC_Failure), 0x14);                  // MAC failure
    EXPECT_EQ(static_cast<uint8_t>(MMRejectCause::Synch_Failure), 0x15);                // Synch failure
    EXPECT_EQ(static_cast<uint8_t>(MMRejectCause::Congestion), 0x16);                   // Congestion
    EXPECT_EQ(static_cast<uint8_t>(MMRejectCause::GSM_Authentication_Unacceptable), 0x17);// GSM auth unacceptable
    EXPECT_EQ(static_cast<uint8_t>(MMRejectCause::Not_Authorized_In_CSG), 0x19);        // Not authorized in CSG
    EXPECT_EQ(static_cast<uint8_t>(MMRejectCause::Service_Option_Not_Supported), 0x20); // Service option not supported
    EXPECT_EQ(static_cast<uint8_t>(MMRejectCause::Requested_Service_Not_Subscribed), 0x21);// Requested service not subscribed
    EXPECT_EQ(static_cast<uint8_t>(MMRejectCause::Service_Option_Out_Of_Order), 0x22);  // Service option out of order
    EXPECT_EQ(static_cast<uint8_t>(MMRejectCause::Call_Cannot_Be_Identified), 0x26);    // Call cannot be identified
    EXPECT_EQ(static_cast<uint8_t>(MMRejectCause::Semantically_Incorrect_Message), 0x5f);// Semantically incorrect msg
    EXPECT_EQ(static_cast<uint8_t>(MMRejectCause::Invalid_Mandatory_Information), 0x60);// Invalid mandatory info
    EXPECT_EQ(static_cast<uint8_t>(MMRejectCause::Message_Type_Invalid), 0x61);         // Message type invalid
    EXPECT_EQ(static_cast<uint8_t>(MMRejectCause::Message_Type_Not_Compatible), 0x62);  // Message type not compatible
    EXPECT_EQ(static_cast<uint8_t>(MMRejectCause::IE_Invalid), 0x63);                   // IE invalid
    EXPECT_EQ(static_cast<uint8_t>(MMRejectCause::Conditional_IE_Error), 0x64);         // Conditional IE error
    EXPECT_EQ(static_cast<uint8_t>(MMRejectCause::Message_Not_Compatible), 0x65);       // Message not compatible
    EXPECT_EQ(static_cast<uint8_t>(MMRejectCause::Protocol_Error_Unspecified), 0x6f);   // Protocol error, unspecified
}

// =====================================================================
// CMServiceType values (GSM 04.08 10.5.3.3)
// Reference: L3_Templates.ttcn CmServiceType
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
    L3Frame frame(Primitive::L3_DATA, 128);
    size_t wp = 0;
    orig.writeV(frame, wp);
    L3RAND parsed;
    size_t rp = 0;
    parsed.parseV(frame, rp);
    EXPECT_EQ(parsed.rand(), randBytes);
}

// =====================================================================
// MM IE: L3SRES (GSM 04.08 10.5.3.2)
// =====================================================================

TEST(GoldenMM, SRES_RoundTrip) {
    L3SRES orig(0x12345678);
    EXPECT_EQ(orig.lengthV(), 4u);
    L3Frame frame(Primitive::L3_DATA, 32);
    size_t wp = 0;
    orig.writeV(frame, wp);
    L3SRES parsed;
    size_t rp = 0;
    parsed.parseV(frame, rp);
    EXPECT_EQ(parsed.value(), 0x12345678u);
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
    L3Frame frame(Primitive::L3_DATA, 16);
    size_t wp = 0;
    rc.writeV(frame, wp);
    L3RejectCauseIE parsed;
    size_t rp = 0;
    parsed.parseV(frame, rp);
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
