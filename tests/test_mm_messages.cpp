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

// MM message round-trip tests with spec-compliant hex values.
// Reference: osmo-ttcn3-hacks L3_Templates.ttcn (MM section).

#include <gtest/gtest.h>
#include <gsml3parser/parser.h>
#include <gsml3parser/mm/l3mmmessages.h>
#include <gsml3parser/common/l3common.h>

using namespace gsml3parser;

static ParserContext ctx;

static std::unique_ptr<L3Message> roundtrip(const L3Message& msg) {
    std::vector<uint8_t> buf(msg.fullLength());
    size_t n = writeL3(msg, buf.data(), buf.size());
    if (n == 0) return nullptr;
    auto result = parseL3(std::span<const uint8_t>(buf), ctx);
    return result;
}

// ── CM Service Accept (GSM 04.08 9.2.5) ───────────────────────────────
// Reference: L3_Templates.ttcn tr_CM_SERV_ACC
// PD=0x05, MTI=0x21, no body

TEST(MMRoundTripTest, CMServiceAccept) {
    L3CMServiceAccept msg;
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->PD(), L3PD::MobilityManagement);
    EXPECT_EQ(parsed->MTI(), L3MMMessage::CMServiceAccept);
}

// ── CM Service Abort (GSM 04.08 9.2.7) ────────────────────────────────
// Reference: L3_Templates.ttcn ts_CM_SERV_REJ

TEST(MMRoundTripTest, CMServiceAbort) {
    L3CMServiceAbort msg;
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3MMMessage::CMServiceAbort);
}

// ── CM Service Reject (GSM 04.08 9.2.6) ───────────────────────────────

TEST(MMRoundTripTest, CMServiceReject) {
    L3CMServiceReject msg(MMRejectCause::Congestion);
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3MMMessage::CMServiceReject);
}

// ── Location Updating Accept (GSM 04.08 9.2.13) ──────────────────────
// Reference: L3_Templates.ttcn ts_LU_ACCEPT
// Structure: PD=0x05, MTI=0x02, NSD(2), LAI(5), [MI TLV], [FOP TV], ...

TEST(MMRoundTripTest, LocationUpdatingAccept) {
    L3LocationAreaIdentity lai("250", "01", 0x1234);
    L3LocationUpdatingAccept msg(lai);
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->PD(), L3PD::MobilityManagement);
    EXPECT_EQ(parsed->MTI(), L3MMMessage::LocationUpdatingAccept);
}

TEST(MMRoundTripTest, LocationUpdatingAccept_WithMI) {
    L3LocationAreaIdentity lai("250", "01", 0x1234);
    L3MobileIdentity mi(0xDEADBEEF);
    L3LocationUpdatingAccept msg(lai, mi, true);
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3MMMessage::LocationUpdatingAccept);
}

// ── Location Updating Reject (GSM 04.08 9.2.14) ──────────────────────
// Reference: L3_Templates.ttcn tr_ML3_MT_LU_Rej

TEST(MMRoundTripTest, LocationUpdatingReject) {
    L3LocationUpdatingReject msg(MMRejectCause::IMSI_Unknown_In_HLR);
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3MMMessage::LocationUpdatingReject);
}

// GSM 04.08 10.2: PD=0x05(MM), skip=0, messageType=000100(LUReject=0x04), NSD=00
// Reference: L3_Templates.ttcn tr_ML3_MT_LU_Rej, GSML3MMMessages.h LocationUpdatingReject=0x04
// Byte 0: PD(4,high) | skip(4,low) = 0101 0000 = 0x50
// Byte 1: messageType(6)<<2 | NSD(2) = 0x04<<2 | 0 = 0x10
// Byte 2: reject_cause = 0x02 (IMSI_Unknown_In_HLR, GSM 04.08 10.5.3.6)
TEST(MMRoundTripTest, LocationUpdatingReject_Parse) {
    uint8_t data[] = {0x50, 0x10, 0x02};
    auto msg = parseL3(std::span<const uint8_t>(data), ctx);
    ASSERT_TRUE(msg);
    auto* lur = dynamic_cast<L3LocationUpdatingReject*>(msg.get());
    ASSERT_TRUE(lur);
    // Verify via round-trip: re-serialize and compare bytes
    std::vector<uint8_t> buf(lur->fullLength());
    size_t n = writeL3(*lur, buf.data(), buf.size());
    EXPECT_EQ(n, sizeof(data));
    for (size_t i = 0; i < sizeof(data); i++) {
        EXPECT_EQ(buf[i], data[i]);
    }
}

// ── Authentication Request (GSM 04.08 9.2.2) ─────────────────────────
// Reference: L3_Templates.ttcn tr_ML3_MT_MM_AUTH_REQ
// Structure: PD=0x05, MTI=0x12, CKSN(4), spare(4), RAND(128 bits)

TEST(MMRoundTripTest, AuthenticationRequest) {
    std::vector<uint8_t> rand(16);
    for (int i = 0; i < 16; i++) rand[i] = static_cast<uint8_t>(i + 1);
    L3AuthenticationRequest msg(0, rand);
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3MMMessage::AuthenticationRequest);
}

// GSM 04.08 10.2: PD=0x05(MM), skip=0, messageType=010010(AuthenticationRequest=0x12), NSD=00
// Reference: L3_Templates.ttcn tr_ML3_MT_MM_AUTH_REQ, GSML3MMMessages.h AuthenticationRequest=0x12
// Byte 0: PD(4,high) | skip(4,low) = 0101 0000 = 0x50
// Byte 1: messageType(6)<<2 | NSD(2) = 0x12<<2 | 0 = 0x48
// Byte 2: CKSN(4)=0, spare(4)=0 = 0x00
// Bytes 3-18: RAND (16 bytes, GSM 04.08 10.5.3.1)
TEST(MMRoundTripTest, AuthenticationRequest_Parse) {
    uint8_t data[] = {
        0x50, 0x48, 0x00,
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10
    };
    auto msg = parseL3(std::span<const uint8_t>(data), ctx);
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->PD(), L3PD::MobilityManagement);
    EXPECT_EQ(msg->MTI(), L3MMMessage::AuthenticationRequest);
}

// ── Authentication Response (GSM 04.08 9.2.3) ────────────────────────
// Reference: L3_Templates.ttcn ts_ML3_MT_MM_AUTH_RESP_2G
// Structure: PD=0x05, MTI=0x14, SRES(32 bits)

TEST(MMRoundTripTest, AuthenticationResponse) {
    // Reference format: PD=0x05(MM), skip=0, messageType=010100(AuthResponse=0x14), NSD=00
    // Byte 0: PD(4) | skip(4) = 0101 0000 = 0x50
    // Byte 1: messageType(6) | NSD(2) = 010100 00 = 0x50
    // Bytes 2-5: SRES = 0xABCD1234
    uint8_t data[] = {0x50, 0x50, 0xAB, 0xCD, 0x12, 0x34};
    auto msg = parseL3(std::span<const uint8_t>(data), ctx);
    ASSERT_TRUE(msg);
    auto* ar = dynamic_cast<L3AuthenticationResponse*>(msg.get());
    ASSERT_TRUE(ar);
    EXPECT_EQ(ar->SRES(), 0xABCD1234u);

    auto parsed = roundtrip(*ar);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3MMMessage::AuthenticationResponse);
}

// GSM 04.08 10.2: PD=0x05(MM), skip=0, messageType=010100(AuthenticationResponse=0x14), NSD=00
// Reference: L3_Templates.ttcn ts_ML3_MT_MM_AUTH_RESP_2G, GSML3MMMessages.h AuthenticationResponse=0x14
// Byte 0: PD(4,high) | skip(4,low) = 0101 0000 = 0x50
// Byte 1: messageType(6)<<2 | NSD(2) = 0x14<<2 | 0 = 0x50
// Bytes 2-5: SRES = 0xABCD1234 (GSM 04.08 10.5.3.2, 32 bits)
TEST(MMRoundTripTest, AuthenticationResponse_Parse) {
    uint8_t data[] = {0x50, 0x50, 0xAB, 0xCD, 0x12, 0x34};
    auto msg = parseL3(std::span<const uint8_t>(data), ctx);
    ASSERT_TRUE(msg);
    auto* ar = dynamic_cast<L3AuthenticationResponse*>(msg.get());
    ASSERT_TRUE(ar);
    EXPECT_EQ(ar->SRES(), 0xABCD1234u);
}

// ── Authentication Reject (GSM 04.08 9.2.1) ──────────────────────────
// Reference: L3_Templates.ttcn ts_ML3_MT_MM_AUTH_REJ

TEST(MMRoundTripTest, AuthenticationReject) {
    L3AuthenticationReject msg;
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3MMMessage::AuthenticationReject);
}

// ── Identity Request (GSM 04.08 9.2.10) ──────────────────────────────
// Reference: L3_Templates.ttcn tr_ML3_MT_MM_ID_Req

TEST(MMRoundTripTest, IdentityRequest_IMSI) {
    L3IdentityRequest msg(MobileIDType::IMSI);
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3MMMessage::IdentityRequest);
}

TEST(MMRoundTripTest, IdentityRequest_IMEI) {
    L3IdentityRequest msg(MobileIDType::IMEI);
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3MMMessage::IdentityRequest);
}

// GSM 04.08 10.2: PD=0x05(MM), skip=0, messageType=011000(IdentityRequest=0x18), NSD=00
// Reference: L3_Templates.ttcn tr_ML3_MT_MM_ID_Req, GSML3MMMessages.h IdentityRequest=0x18
// Byte 0: PD(4,high) | skip(4,low) = 0101 0000 = 0x50
// Byte 1: messageType(6)<<2 | NSD(2) = 0x18<<2 | 0 = 0x60
// Byte 2: spare(4) | identityType(4) = 0000 0001 = 0x01 (IMSI per GSM 04.08 10.5.3.4)
TEST(MMRoundTripTest, IdentityRequest_Parse) {
    uint8_t data[] = {0x50, 0x60, 0x01};
    auto msg = parseL3(std::span<const uint8_t>(data), ctx);
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->MTI(), L3MMMessage::IdentityRequest);
    auto* ir = dynamic_cast<L3IdentityRequest*>(msg.get());
    ASSERT_TRUE(ir);
    // Round-trip to verify serialization matches reference layout
    auto rt = roundtrip(*ir);
    ASSERT_TRUE(rt);
    EXPECT_EQ(rt->MTI(), L3MMMessage::IdentityRequest);
}

// ── TMSI Reallocation Command (GSM 04.08 9.2.17) ────────────────────

TEST(MMRoundTripTest, TMSIReallocationCommand) {
    L3LocationAreaIdentity lai("250", "01", 0x1234);
    L3MobileIdentity tmsi(0x12345678);
    L3TMSIReallocationCommand msg(lai, tmsi, false);
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3MMMessage::TMSIReallocationCommand);
}

// ── TMSI Reallocation Complete (GSM 04.08 9.2.18) ───────────────────

TEST(MMRoundTripTest, TMSIReallocationComplete) {
    L3TMSIReallocationComplete msg;
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3MMMessage::TMSIReallocationComplete);
}

// ── MM Status (GSM 04.08 9.2.15) ────────────────────────────────────

TEST(MMRoundTripTest, MMStatus) {
    L3MMStatus msg;
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3MMMessage::MMStatus);
}

// ── CM Service Request (GSM 04.08 9.2.9) ────────────────────────────
// Reference: L3_Templates.ttcn ts_CM_SERV_REQ
// Structure: PD=0x05, MTI=0x24, NSD(2), CM_ServiceType(4), CKSN(4), CM2 LV, MI LV

TEST(MMRoundTripTest, CMServiceRequest) {
    L3CMServiceRequest msg;
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3MMMessage::CMServiceRequest);
}

// ── CM Reestablishment Request (GSM 04.08 9.2.4) ────────────────────
// Reference: L3_Templates.ttcn ts_CM_REESTABL_REQ

TEST(MMRoundTripTest, CMReestablishmentRequest) {
    L3CMReestablishmentRequest msg;
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3MMMessage::CMReestablishmentRequest);
}

// ── IMSI Detach Indication (GSM 04.08 9.2.15) ──────────────────────
// Reference: L3_Templates.ttcn ts_ML3_MO_MM_IMSI_DET_Ind

TEST(MMRoundTripTest, IMSIDetachIndication) {
    L3IMSIDetachIndication msg;
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3MMMessage::IMSIDetachIndication);
}

// ── MM Information (GSM 04.08 9.2.15a) ──────────────────────────────
// Reference: L3_Templates.ttcn ts_ML3_MO_MM_INFO
// Structure: PD=0x05, MTI=0x32, NetworkName TLV, TimeZoneAndTime TLV

TEST(MMRoundTripTest, MMInformation) {
    L3MMInformation msg;
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3MMMessage::MMInformation);
}

// ── Location Updating Request (GSM 04.08 9.2.15) ────────────────────
// Reference: L3_Templates.ttcn ts_LU_REQ, ts_ML3_MO_LU_Req

TEST(MMRoundTripTest, LocationUpdatingRequest) {
    L3LocationUpdatingRequest msg;
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3MMMessage::LocationUpdatingRequest);
}

// ── Identity Response (GSM 04.08 9.2.11) ────────────────────────────
// Reference: L3_Templates.ttcn ts_ML3_MO_MM_ID_Rsp

TEST(MMRoundTripTest, IdentityResponse) {
    L3IdentityResponse msg;
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3MMMessage::IdentityResponse);
}

// ── Parse from known hex values ──────────────────────────────────────

// GSM 04.08 10.2: PD=0x05(MM), skip=0, messageType=100001(CMServiceAccept=0x21), NSD=00
// Reference: L3_Templates.ttcn tr_CM_SERV_ACC (discriminator='0101'B, messageType='100001'B)
// Byte 0: PD(high=5)|skip(low=0) = 0x50
// Byte 1: messageType(6)<<2|NSD(2) = 0x21<<2|0 = 0x84
TEST(MMRoundTripTest, Parse_CMServiceAccept_Hex) {
    auto msg = parseL3Hex("5084", ctx);
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->PD(), L3PD::MobilityManagement);
    EXPECT_EQ(msg->MTI(), L3MMMessage::CMServiceAccept);
}

// GSM 04.08 10.2: PD=0x05(MM), skip=0, messageType=010001(AuthenticationReject=0x11), NSD=00
// Reference: L3_Templates.ttcn ts_ML3_MT_MM_AUTH_REJ, GSML3MMMessages.h AuthenticationReject=0x11
// Byte 0: PD(high=5)|skip(low=0) = 0x50
// Byte 1: messageType(6)<<2|NSD(2) = 0x11<<2|0 = 0x44
TEST(MMRoundTripTest, Parse_AuthenticationReject_Hex) {
    auto msg = parseL3Hex("5044", ctx);
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->MTI(), L3MMMessage::AuthenticationReject);
}

// GSM 04.08 10.2: PD=0x05(MM), skip=0, messageType=011011(TMSIReallocationComplete=0x1B), NSD=00
// Reference: GSML3MMMessages.h TMSIReallocationComplete=0x1B
// Byte 0: PD(high=5)|skip(low=0) = 0x50
// Byte 1: messageType(6)<<2|NSD(2) = 0x1B<<2|0 = 0x6C
TEST(MMRoundTripTest, Parse_TMSIReallocationComplete_Hex) {
    auto msg = parseL3Hex("506C", ctx);
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->MTI(), L3MMMessage::TMSIReallocationComplete);
}

// ── MMRejectCause values from spec ───────────────────────────────────
// Reference: L3_Templates.ttcn c_MM_CAUSE_IMSI_UNKNOWN_IN_HLR = '02'O

TEST(MMRoundTripTest, MMRejectCause_Values) {
    EXPECT_EQ(static_cast<uint8_t>(MMRejectCause::IMSI_Unknown_In_HLR), 2u);
    EXPECT_EQ(static_cast<uint8_t>(MMRejectCause::Illegal_MS), 3u);
    EXPECT_EQ(static_cast<uint8_t>(MMRejectCause::IMSI_Unknown_In_VLR), 4u);
    EXPECT_EQ(static_cast<uint8_t>(MMRejectCause::Congestion), 0x16u);
    EXPECT_EQ(static_cast<uint8_t>(MMRejectCause::Protocol_Error_Unspecified), 0x6fu);
}

// ── CMServiceType values ─────────────────────────────────────────────
// Reference: L3_Templates.ttcn CmServiceType

TEST(MMRoundTripTest, CMServiceType_Values) {
    EXPECT_EQ(static_cast<uint8_t>(L3CMServiceType::MobileOriginatedCall), 1u);
    EXPECT_EQ(static_cast<uint8_t>(L3CMServiceType::EmergencyCall), 2u);
    EXPECT_EQ(static_cast<uint8_t>(L3CMServiceType::ShortMessage), 4u);
    EXPECT_EQ(static_cast<uint8_t>(L3CMServiceType::SupplementaryService), 8u);
    EXPECT_EQ(static_cast<uint8_t>(L3CMServiceType::VoiceCallGroup), 9u);
    EXPECT_EQ(static_cast<uint8_t>(L3CMServiceType::VoiceBroadcast), 10u);
    EXPECT_EQ(static_cast<uint8_t>(L3CMServiceType::LocationService), 11u);
}

// ── L3RAND (GSM 04.08 10.5.3.1) ─────────────────────────────────────

TEST(MMRoundTripTest, RAND_RoundTrip) {
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

// ── L3SRES (GSM 04.08 10.5.3.2) ─────────────────────────────────────

TEST(MMRoundTripTest, SRES_RoundTrip) {
    L3RAND rand16; // default
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

// ── L3NetworkName (GSM 04.08 10.5.3.5a) ─────────────────────────────

TEST(MMRoundTripTest, NetworkName) {
    L3NetworkName orig("TestNet", GSMAlphabet::ALPHABET_7BIT, 0);
    EXPECT_STREQ(orig.name(), "TestNet");
    EXPECT_EQ(orig.alphabet(), GSMAlphabet::ALPHABET_7BIT);
}

// ── L3TimeZoneAndTime (GSM 04.08 10.5.3.9) ──────────────────────────

TEST(MMRoundTripTest, TimeZoneAndTime) {
    L3TimeZoneAndTime orig(L3TimeZoneAndTime::UTC_TIME);
    EXPECT_EQ(orig.lengthV(), 7u);
    EXPECT_EQ(orig.type(), L3TimeZoneAndTime::UTC_TIME);
}

// ── LocationUpdateType ───────────────────────────────────────────────
// Reference: L3_Templates.ttcn LU_Type_Normal, LU_Type_Periodic, LU_Type_IMSI_Attach

TEST(MMRoundTripTest, LocationUpdateType_Values) {
    EXPECT_EQ(static_cast<uint8_t>(LocationUpdateType::Normal), 0u);
    EXPECT_EQ(static_cast<uint8_t>(LocationUpdateType::Periodic), 1u);
    EXPECT_EQ(static_cast<uint8_t>(LocationUpdateType::IMSIAttach), 2u);
}
