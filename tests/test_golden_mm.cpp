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
// MM MESSAGE TYPE VALUES (GSM 04.08 Table 10.5.3)
// Reference: L3_Templates.ttcn MM message type constants
// =====================================================================

TEST(GoldenMM, MessageTypeValues) {
    EXPECT_EQ(L3MMMessage::IMSIDetachIndication, 0x01);
    EXPECT_EQ(L3MMMessage::LocationUpdatingAccept, 0x02);
    EXPECT_EQ(L3MMMessage::LocationUpdatingReject, 0x04);
    EXPECT_EQ(L3MMMessage::LocationUpdatingRequest, 0x08);
    EXPECT_EQ(L3MMMessage::AuthenticationRequest, 0x12);
    EXPECT_EQ(L3MMMessage::AuthenticationResponse, 0x14);
    EXPECT_EQ(L3MMMessage::AuthenticationReject, 0x11);
    EXPECT_EQ(L3MMMessage::IdentityRequest, 0x18);
    EXPECT_EQ(L3MMMessage::IdentityResponse, 0x19);
    EXPECT_EQ(L3MMMessage::TMSIReallocationCommand, 0x1a);
    EXPECT_EQ(L3MMMessage::TMSIReallocationComplete, 0x1b);
    EXPECT_EQ(L3MMMessage::CMServiceAccept, 0x21);
    EXPECT_EQ(L3MMMessage::CMServiceReject, 0x22);
    EXPECT_EQ(L3MMMessage::CMServiceAbort, 0x23);
    EXPECT_EQ(L3MMMessage::CMServiceRequest, 0x24);
    EXPECT_EQ(L3MMMessage::CMReestablishmentRequest, 0x28);
    EXPECT_EQ(L3MMMessage::MMInformation, 0x32);
    EXPECT_EQ(L3MMMessage::MMStatus, 0x31);
}

// =====================================================================
// MM PARSE FROM HEX: Location Updating Request (GSM 04.08 9.2.15)
// Reference: L3_Templates.ttcn ts_LU_REQ, ts_ML3_MO_LU_Req
// Structure: LU_Type(2)|spare(2)|CKSN(4), CM1 LV, MI LV, [LAI LV]
// =====================================================================

TEST(GoldenMM, LocationUpdatingRequest_Parse) {
    // Byte 0: PD(4)=5|skip(4)=0 = 0x50
    // Byte 1: MTI = 0x08 (LocationUpdatingRequest)
    // Byte 2: LU_Type(2)=0(Normal), spare(2)=0, CKSN(4)=0 = 0x00
    // Byte 3: CM1 length = 1
    // Byte 4: CM1 = 0x00
    // Byte 5: MI length = 5
    // Byte 6: spare(4)=0, type(3)=100(TMSI), oe(1)=0 = 0x0C
    // Bytes 7-10: TMSI = 0x12345678
    uint8_t data[] = {
        0x50, 0x08, 0x00,
        0x01, 0x00,
        0x05, 0x0C, 0x12, 0x34, 0x56, 0x78
    };
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->MTI(), L3MMMessage::LocationUpdatingRequest);
}

// =====================================================================
// MM PARSE FROM HEX: Location Updating Accept (GSM 04.08 9.2.13)
// Reference: L3_Templates.ttcn ts_LU_ACCEPT
// Structure: LAI(5), [MI TLV], [FOP TV]
// =====================================================================

TEST(GoldenMM, LocationUpdatingAccept_Parse) {
    // Byte 0: PD(4)|skip(4) = 0x50
    // Byte 1: MTI = 0x02 (LocationUpdatingAccept)
    // Bytes 2-6: LAI: MCC=250, MNC=01, LAC=0x1234
    uint8_t data[] = {0x50, 0x02, 0x52, 0xF0, 0x10, 0x12, 0x34};
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->MTI(), L3MMMessage::LocationUpdatingAccept);
}

// =====================================================================
// MM PARSE FROM HEX: TMSI Reallocation Command (GSM 04.08 9.2.17)
// Reference: L3_Templates.ttcn ts_TMSI_REALLOC_CM
// Structure: LAI(5), MI LV, FollowOnProceed(4)|spare(4)
// =====================================================================

TEST(GoldenMM, TMSIReallocationCommand_Parse) {
    // Byte 0: PD(4)|skip(4) = 0x50
    // Byte 1: MTI = 0x1a (TMSIReallocationCommand)
    // Bytes 2-6: LAI: MCC=250, MNC=01, LAC=0x1234
    // Byte 7: MI length = 5
    // Byte 8: spare(4)=0, type(3)=100(TMSI), oe(1)=0 = 0x0C
    // Bytes 9-12: TMSI = 0x87654321
    // Byte 13: FollowOnProceed(4)=0, spare(4)=0 = 0x00
    uint8_t data[] = {
        0x50, 0x1a,
        0x52, 0xF0, 0x10, 0x12, 0x34,
        0x05, 0x0C, 0x87, 0x65, 0x43, 0x21,
        0x00
    };
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->MTI(), L3MMMessage::TMSIReallocationCommand);
}

// =====================================================================
// MM PARSE FROM HEX: CM Service Request (GSM 04.08 9.2.9)
// Reference: L3_Templates.ttcn ts_CM_SERV_REQ
// Structure: CM_ServiceType(4)|CKSN(4), CM2 LV, MI LV
// =====================================================================

TEST(GoldenMM, CMServiceRequest_Parse) {
    // Byte 0: PD(4)|skip(4) = 0x50
    // Byte 1: MTI = 0x24 (CMServiceRequest)
    // Byte 2: CM_ServiceType(4)=1(MO_Call), CKSN(4)=0 = 0x01
    // Byte 3: CM2 length = 3
    // Bytes 4-6: CM2 = 0x20, 0x00, 0x80
    // Byte 7: MI length = 5
    // Byte 8: spare(4)=0, type(3)=100(TMSI), oe(1)=0 = 0x0C
    // Bytes 9-12: TMSI = 0x12345678
    uint8_t data[] = {
        0x50, 0x24, 0x01,
        0x03, 0x20, 0x00, 0x80,
        0x05, 0x0C, 0x12, 0x34, 0x56, 0x78
    };
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->MTI(), L3MMMessage::CMServiceRequest);
}

// =====================================================================
// MM PARSE FROM HEX: CM Service Reject (GSM 04.08 9.2.6)
// Reference: L3_Templates.ttcn ts_CM_SERV_REJ
// Structure: reject_cause(8)
// =====================================================================

TEST(GoldenMM, CMServiceReject_Parse) {
    // Byte 0: PD(4)|skip(4) = 0x50
    // Byte 1: messageType(6)=0x22|NSD(2)=0 = 0x22<<2 = 0x88
    //   Actually MTI=0x22, so byte 1 = 0x22<<2 = 0x88
    // Byte 2: reject_cause = 0x16 (Congestion)
    uint8_t data[] = {0x50, 0x88, 0x16};
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->MTI(), L3MMMessage::CMServiceReject);
}

// =====================================================================
// MM PARSE FROM HEX: IMSI Detach Indication (GSM 04.08 9.2.15)
// Reference: L3_Templates.ttcn ts_ML3_MO_MM_IMSI_DET_Ind
// Structure: CM1 LV, MI LV
// =====================================================================

TEST(GoldenMM, IMSIDetachIndication_Parse) {
    // Byte 0: PD(4)|skip(4) = 0x50
    // Byte 1: MTI = 0x01 (IMSIDetachIndication)
    // Byte 2: CM1 length = 1
    // Byte 3: CM1 = 0x00
    // Byte 4: MI length = 5
    // Byte 5: spare(4)=0, type(3)=100(TMSI), oe(1)=0 = 0x0C
    // Bytes 6-9: TMSI = 0x12345678
    uint8_t data[] = {
        0x50, 0x01,
        0x01, 0x00,
        0x05, 0x0C, 0x12, 0x34, 0x56, 0x78
    };
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->MTI(), L3MMMessage::IMSIDetachIndication);
}

// =====================================================================
// MM PARSE FROM HEX: MM Status (GSM 04.08 9.2.15)
// Reference: L3_Templates.ttcn tr_ML3_MT_MM_STATUS
// Structure: cause(8), spare(8), spare(8)
// =====================================================================

TEST(GoldenMM, MMStatus_Parse) {
    // Byte 0: PD(4)|skip(4) = 0x50
    // Byte 1: MTI = 0x31 (MMStatus)
    // Byte 2: cause = 0x60 (Invalid_Mandatory_Information)
    // Byte 3: spare = 0x00
    // Byte 4: spare = 0x00
    uint8_t data[] = {0x50, 0x62, 0x60, 0x00, 0x00};
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->MTI(), L3MMMessage::MMStatus);
}

// =====================================================================
// MM PARSE FROM HEX: Identity Response (GSM 04.08 9.2.11)
// Reference: L3_Templates.ttcn ts_ML3_MO_MM_ID_Rsp
// Structure: MI LV
// =====================================================================

TEST(GoldenMM, IdentityResponse_Parse) {
    // Byte 0: PD(4)|skip(4) = 0x50
    // Byte 1: MTI = 0x19 (IdentityResponse)
    // Byte 2: MI length = 5
    // Byte 3: spare(4)=0, type(3)=100(TMSI), oe(1)=0 = 0x0C
    // Bytes 4-7: TMSI = 0x12345678
    uint8_t data[] = {0x50, 0x32, 0x05, 0x0C, 0x12, 0x34, 0x56, 0x78};
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->MTI(), L3MMMessage::IdentityResponse);
}

// =====================================================================
// MM PARSE FROM HEX: CM Reestablishment Request (GSM 04.08 9.2.4)
// Reference: L3_Templates.ttcn ts_CM_REESTABL_REQ
// Structure: CM2 LV, MI LV, [LAI LV]
// =====================================================================

TEST(GoldenMM, CMReestablishmentRequest_Parse) {
    // Byte 0: PD(4)|skip(4) = 0x50
    // Byte 1: MTI = 0x28 (CMReestablishmentRequest)
    // Byte 2: CM2 length = 3
    // Bytes 3-5: CM2 = 0x20, 0x00, 0x80
    // Byte 6: MI length = 5
    // Byte 7: spare(4)=0, type(3)=100(TMSI), oe(1)=0 = 0x0C
    // Bytes 8-11: TMSI = 0x12345678
    uint8_t data[] = {
        0x50, 0x50,
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
// MMRejectCause values (GSM 04.08 10.5.3.6)
// Reference: L3_Templates.ttcn c_MM_CAUSE_IMSI_UNKNOWN_IN_HLR
// =====================================================================

TEST(GoldenMM, RejectCauseValues) {
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
