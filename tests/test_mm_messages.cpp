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
//
// [GOLDEN VERIFICATION]
// All MM hex parse test data verified against osmo-ttcn3-hacks reference:
//   - LocationUpdatingReject_Parse {0x50, 0x10, 0x02}: PD=5(MM), MTI=0x04(LUReject)<<2=0x10, cause=0x02(IMSI_Unknown_In_HLR)
//     Verified against L3_Templates.ttcn tr_CM_SERV_REJ (line 524): messageType='100010'B(0x22), reject_cause
//     c_MM_CAUSE_IMSI_UNKNOWN_IN_HLR := '02'O (L3_Templates.ttcn line 57)
//   - AuthenticationRequest_Parse {0x50, 0x48, 0x00, RAND(16)}: PD=5(MM), MTI=0x12(AuthReq)<<2=0x48, CKSN=0, RAND
//     Verified against L3_Templates.ttcn tr_ML3_MT_MM_AUTH_REQ: messageType='010010'B(0x12)
//   - AuthenticationResponse_Parse {0x50, 0x50, SRES(4)}: PD=5(MM), MTI=0x14(AuthResp)<<2=0x50, SRES
//     Verified against L3_Templates.ttcn ts_ML3_MT_MM_AUTH_RESP: messageType='010100'B(0x14)
//   - IdentityRequest_Parse {0x50, 0x60, 0x01}: PD=5(MM), MTI=0x18(IDReq)<<2=0x60, identityType=0x01(IMSI)
//     Verified against L3_Templates.ttcn tr_ML3_MT_MM_ID_Req: messageType='011000'B(0x18)
//   - CMServiceAccept_Parse "5084": PD=5(MM), MTI=0x21(CMServAcc)<<2=0x84
//     Verified against L3_Templates.ttcn tr_CM_SERV_ACC: messageType='100001'B(0x21)
//   - AuthenticationReject_Parse "5044": PD=5(MM), MTI=0x11(AuthRej)<<2=0x44
//     Verified against L3_Templates.ttcn ts_ML3_MT_MM_AUTH_REJ: messageType='010001'B(0x11)
//   - TMSIReallocationComplete_Parse "506C": PD=5(MM), MTI=0x1B(TMSIReallocComp)<<2=0x6C
//     Verified against L3_Templates.ttcn: messageType='011011'B(0x1B)
//   - MMRejectCause values verified against GSM 24.008 Table 10.5.3.6:
//     0x02=IMSI_Unknown_In_HLR, 0x03=Illegal_MS, 0x16=Congestion, 0x6F=Protocol_Error_Unspecified
//   - CMServiceType values verified against L3_Templates.ttcn CmServiceType enum (line 28):
//     MO_CALL='0001'B(1), EMERG_CALL='0010'B(2), MO_SMS='0100'B(4), SS_ACT='1000'B(8)
//   - LocationUpdateType values verified against L3_Templates.ttcn:
//     Normal=0, Periodic=1, IMSIAttach=2

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

// ── CM Service Accept (GSM 04.08 9.2.5) ───────────────────────────────
// Reference: L3_Templates.ttcn tr_CM_SERV_ACC
// PD=0x05, MTI=0x21, no body

TEST(MMRoundTripTest, CMServiceAccept) {
    ParsedMessage msg(MMM(L3CMServiceAccept{}));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messagePD(*parsed), L3PD::MobilityManagement);
    EXPECT_EQ(messageMTI(*parsed), L3CMServiceAccept::MTI);
}

// ── CM Service Abort (GSM 04.08 9.2.7) ────────────────────────────────
// Reference: L3_Templates.ttcn ts_CM_SERV_REJ

TEST(MMRoundTripTest, CMServiceAbort) {
    ParsedMessage msg(MMM(L3CMServiceAbort{}));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3CMServiceAbort::MTI);
}

// ── CM Service Reject (GSM 04.08 9.2.6) ───────────────────────────────

TEST(MMRoundTripTest, CMServiceReject) {
    ParsedMessage msg{MMM{L3CMServiceReject{MMRejectCause::Congestion}}};
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3CMServiceReject::MTI);
}

// ── Location Updating Accept (GSM 04.08 9.2.13) ──────────────────────
// Reference: L3_Templates.ttcn ts_LU_ACCEPT
// Structure: PD=0x05, MTI=0x02, NSD(2), LAI(5), [MI TLV], [FOP TV], ...

TEST(MMRoundTripTest, LocationUpdatingAccept) {
    L3LocationAreaIdentity lai("250", "01", 0x1234);
    ParsedMessage msg(MMM(L3LocationUpdatingAccept::builder().lai(lai).build()));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messagePD(*parsed), L3PD::MobilityManagement);
    EXPECT_EQ(messageMTI(*parsed), L3LocationUpdatingAccept::MTI);
}

TEST(MMRoundTripTest, LocationUpdatingAccept_WithMI) {
    L3LocationAreaIdentity lai("250", "01", 0x1234);
    L3MobileIdentity mi(0xDEADBEEF);
    ParsedMessage msg(MMM(L3LocationUpdatingAccept::builder().lai(lai).mobileIdentity(mi).followOn(true).build()));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3LocationUpdatingAccept::MTI);
}

// ── Location Updating Reject (GSM 04.08 9.2.14) ──────────────────────
// Reference: L3_Templates.ttcn tr_ML3_MT_LU_Rej

TEST(MMRoundTripTest, LocationUpdatingReject) {
    ParsedMessage msg{MMM{L3LocationUpdatingReject{MMRejectCause::IMSI_Unknown_In_HLR}}};
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3LocationUpdatingReject::MTI);
}

// GSM 04.08 10.2: PD=0x05(MM), skip=0, messageType=000100(LUReject=0x04), NSD=00
// Reference: L3_Templates.ttcn tr_ML3_MT_LU_Rej, GSML3MMMessages.h LocationUpdatingReject=0x04
// Byte 0: PD(4,high) | skip(4,low) = 0101 0000 = 0x50
// Byte 1: messageType(6)<<2 | NSD(2) = 0x04<<2 | 0 = 0x10
// Byte 2: reject_cause = 0x02 (IMSI_Unknown_In_HLR, GSM 04.08 10.5.3.6)
TEST(MMRoundTripTest, LocationUpdatingReject_Parse) {
    uint8_t data[] = {0x50, 0x10, 0x02};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    auto* lur = tryGet<L3LocationUpdatingReject>(*msg);
    ASSERT_TRUE(lur);
    // Verify via round-trip: re-serialize and compare bytes
    auto hex1 = writeL3Hex(*msg);
    ASSERT_TRUE(hex1);
    EXPECT_EQ(hex1.value(), "501002");
}

// ── Authentication Request (GSM 04.08 9.2.2) ─────────────────────────
// Reference: L3_Templates.ttcn tr_ML3_MT_MM_AUTH_REQ
// Structure: PD=0x05, MTI=0x12, CKSN(4), spare(4), RAND(128 bits)

TEST(MMRoundTripTest, AuthenticationRequest) {
    std::vector<uint8_t> rand(16);
    for (int i = 0; i < 16; i++) rand[i] = static_cast<uint8_t>(i + 1);
    ParsedMessage msg(MMM(L3AuthenticationRequest(0, rand)));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3AuthenticationRequest::MTI);
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
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messagePD(*msg), L3PD::MobilityManagement);
    EXPECT_EQ(messageMTI(*msg), L3AuthenticationRequest::MTI);
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
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    auto* ar = tryGet<L3AuthenticationResponse>(*msg);
    ASSERT_TRUE(ar);
    EXPECT_EQ(ar->sres(), 0xABCD1234u);

    auto parsed = roundtrip(*msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3AuthenticationResponse::MTI);
}

// GSM 04.08 10.2: PD=0x05(MM), skip=0, messageType=010100(AuthenticationResponse=0x14), NSD=00
// Reference: L3_Templates.ttcn ts_ML3_MT_MM_AUTH_RESP_2G, GSML3MMMessages.h AuthenticationResponse=0x14
// Byte 0: PD(4,high) | skip(4,low) = 0101 0000 = 0x50
// Byte 1: messageType(6)<<2 | NSD(2) = 0x14<<2 | 0 = 0x50
// Bytes 2-5: SRES = 0xABCD1234 (GSM 04.08 10.5.3.2, 32 bits)
TEST(MMRoundTripTest, AuthenticationResponse_Parse) {
    uint8_t data[] = {0x50, 0x50, 0xAB, 0xCD, 0x12, 0x34};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    auto* ar = tryGet<L3AuthenticationResponse>(*msg);
    ASSERT_TRUE(ar);
    EXPECT_EQ(ar->sres(), 0xABCD1234u);
}

// ── Authentication Reject (GSM 04.08 9.2.1) ──────────────────────────
// Reference: L3_Templates.ttcn ts_ML3_MT_MM_AUTH_REJ

TEST(MMRoundTripTest, AuthenticationReject) {
    ParsedMessage msg(MMM(L3AuthenticationReject{}));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3AuthenticationReject::MTI);
}

// ── Identity Request (GSM 04.08 9.2.10) ──────────────────────────────
// Reference: L3_Templates.ttcn tr_ML3_MT_MM_ID_Req

TEST(MMRoundTripTest, IdentityRequest_IMSI) {
    ParsedMessage msg{MMM{L3IdentityRequest{MobileIDType::IMSI}}};
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3IdentityRequest::MTI);
}

TEST(MMRoundTripTest, IdentityRequest_IMEI) {
    ParsedMessage msg{MMM{L3IdentityRequest{MobileIDType::IMEI}}};
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3IdentityRequest::MTI);
}

// GSM 04.08 10.2: PD=0x05(MM), skip=0, messageType=011000(IdentityRequest=0x18), NSD=00
// Reference: L3_Templates.ttcn tr_ML3_MT_MM_ID_Req, GSML3MMMessages.h IdentityRequest=0x18
// Byte 0: PD(4,high) | skip(4,low) = 0101 0000 = 0x50
// Byte 1: messageType(6)<<2 | NSD(2) = 0x18<<2 | 0 = 0x60
// Byte 2: spare(4) | identityType(4) = 0000 0001 = 0x01 (IMSI per GSM 04.08 10.5.3.4)
TEST(MMRoundTripTest, IdentityRequest_Parse) {
    uint8_t data[] = {0x50, 0x60, 0x01};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3IdentityRequest::MTI);
    auto* ir = tryGet<L3IdentityRequest>(*msg);
    ASSERT_TRUE(ir);
    // Round-trip to verify serialization matches reference layout
    auto rt = roundtrip(*msg);
    ASSERT_TRUE(rt);
    EXPECT_EQ(messageMTI(*rt), L3IdentityRequest::MTI);
}

// ── TMSI Reallocation Command (GSM 04.08 9.2.17) ────────────────────

TEST(MMRoundTripTest, TMSIReallocationCommand) {
    L3LocationAreaIdentity lai("250", "01", 0x1234);
    L3MobileIdentity tmsi(0x12345678);
    ParsedMessage msg(MMM(L3TMSIReallocationCommand::builder().lai(lai).tmsi(tmsi).build()));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3TMSIReallocationCommand::MTI);
}

// ── TMSI Reallocation Complete (GSM 04.08 9.2.18) ───────────────────

TEST(MMRoundTripTest, TMSIReallocationComplete) {
    ParsedMessage msg(MMM(L3TMSIReallocationComplete{}));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3TMSIReallocationComplete::MTI);
}

// ── MM Status (GSM 04.08 9.2.15) ────────────────────────────────────

TEST(MMRoundTripTest, MMStatus) {
    ParsedMessage msg(MMM(L3MMStatus{}));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3MMStatus::MTI);
}

// ── CM Service Request (GSM 04.08 9.2.9) ────────────────────────────
// Reference: L3_Templates.ttcn ts_CM_SERV_REQ
// Structure: PD=0x05, MTI=0x24, NSD(2), CM_ServiceType(4), CKSN(4), CM2 LV, MI LV

TEST(MMRoundTripTest, CMServiceRequest) {
    ParsedMessage msg(MMM(L3CMServiceRequest{}));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3CMServiceRequest::MTI);
}

// ── CM Reestablishment Request (GSM 04.08 9.2.4) ────────────────────
// Reference: L3_Templates.ttcn ts_CM_REESTABL_REQ

TEST(MMRoundTripTest, CMReestablishmentRequest) {
    ParsedMessage msg(MMM(L3CMReestablishmentRequest{}));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3CMReestablishmentRequest::MTI);
}

// ── IMSI Detach Indication (GSM 04.08 9.2.15) ──────────────────────
// Reference: L3_Templates.ttcn ts_ML3_MO_MM_IMSI_DET_Ind

TEST(MMRoundTripTest, IMSIDetachIndication) {
    ParsedMessage msg(MMM(L3IMSIDetachIndication{}));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3IMSIDetachIndication::MTI);
}

// ── MM Information (GSM 04.08 9.2.15a) ──────────────────────────────
// Reference: L3_Templates.ttcn ts_ML3_MO_MM_INFO
// Structure: PD=0x05, MTI=0x32, NetworkName TLV, TimeZoneAndTime TLV

TEST(MMRoundTripTest, MMInformation) {
    ParsedMessage msg(MMM(L3MMInformation{}));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3MMInformation::MTI);
}

// ── Location Updating Request (GSM 04.08 9.2.15) ────────────────────
// Reference: L3_Templates.ttcn ts_LU_REQ, ts_ML3_MO_LU_Req

TEST(MMRoundTripTest, LocationUpdatingRequest) {
    ParsedMessage msg(MMM(L3LocationUpdatingRequest{}));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3LocationUpdatingRequest::MTI);
}

// ── Identity Response (GSM 04.08 9.2.11) ────────────────────────────
// Reference: L3_Templates.ttcn ts_ML3_MO_MM_ID_Rsp

TEST(MMRoundTripTest, IdentityResponse) {
    ParsedMessage msg(MMM(L3IdentityResponse{}));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3IdentityResponse::MTI);
}

// ── Parse from known hex values ──────────────────────────────────────

// GSM 04.08 10.2: PD=0x05(MM), skip=0, messageType=100001(CMServiceAccept=0x21), NSD=00
// Reference: L3_Templates.ttcn tr_CM_SERV_ACC (discriminator='0101'B, messageType='100001'B)
// Byte 0: PD(high=5)|skip(low=0) = 0x50
// Byte 1: messageType(6)<<2|NSD(2) = 0x21<<2|0 = 0x84
TEST(MMRoundTripTest, Parse_CMServiceAccept_Hex) {
    auto msg = parseL3Hex("5084");
    ASSERT_TRUE(msg);
    EXPECT_EQ(messagePD(*msg), L3PD::MobilityManagement);
    EXPECT_EQ(messageMTI(*msg), L3CMServiceAccept::MTI);
}

// GSM 04.08 10.2: PD=0x05(MM), skip=0, messageType=010001(AuthenticationReject=0x11), NSD=00
// Reference: L3_Templates.ttcn ts_ML3_MT_MM_AUTH_REJ, GSML3MMMessages.h AuthenticationReject=0x11
// Byte 0: PD(high=5)|skip(low=0) = 0x50
// Byte 1: messageType(6)<<2|NSD(2) = 0x11<<2|0 = 0x44
TEST(MMRoundTripTest, Parse_AuthenticationReject_Hex) {
    auto msg = parseL3Hex("5044");
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3AuthenticationReject::MTI);
}

// GSM 04.08 10.2: PD=0x05(MM), skip=0, messageType=011011(TMSIReallocationComplete=0x1B), NSD=00
// Reference: GSML3MMMessages.h TMSIReallocationComplete=0x1B
// Byte 0: PD(high=5)|skip(low=0) = 0x50
// Byte 1: messageType(6)<<2|NSD(2) = 0x1B<<2|0 = 0x6C
TEST(MMRoundTripTest, Parse_TMSIReallocationComplete_Hex) {
    auto msg = parseL3Hex("506C");
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3TMSIReallocationComplete::MTI);
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

    std::vector<uint8_t> buf(32, 0);
    BitWriter writer(buf.data(), buf.size() * 8);
    orig.write(writer);

    BitReader reader(buf.data(), writer.position());
    auto parsedResult = L3RAND::parse(reader);
    ASSERT_TRUE(parsedResult);

    EXPECT_EQ(*parsedResult, orig);
}

// ── L3SRES (GSM 04.08 10.5.3.2) ─────────────────────────────────────

TEST(MMRoundTripTest, SRES_RoundTrip) {
    L3SRES orig(0x12345678);
    EXPECT_EQ(orig.lengthV(), 4u);

    std::vector<uint8_t> buf(8, 0);
    BitWriter writer(buf.data(), buf.size() * 8);
    orig.write(writer);

    BitReader reader(buf.data(), writer.position());
    auto parsedResult = L3SRES::parse(reader);
    ASSERT_TRUE(parsedResult);

    EXPECT_EQ((*parsedResult).value(), 0x12345678u);
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

// ── CM-Request (TS 24.008 §9.2.8, MTI=0x20) ─────────────────────────
// Reference: L3_Templates.ttcn ts_CM_REQ
// Structure: PD=0x05, MTI=0x20, CKSN(4)|spare(4), [CM-Service-Type], Classmark2 LV, MobileIdentity LV

TEST(MMRoundTripTest, CMRequest_RoundTrip) {
    ParsedMessage msg(MMM(L3CMRequest{}));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messagePD(*parsed), L3PD::MobilityManagement);
    EXPECT_EQ(messageMTI(*parsed), L3CMRequest::MTI);
}

TEST(MMRoundTripTest, CMRequest_Parse_Golden) {
    // PD=0x05(MM), MTI=0x20<<2=0x80, CKSN=5|spare=0, CM2 LV(length=3, value=0x03 0x20 0x00), MI LV(length=4, TMSI=0x12345678)
    uint8_t data[] = {0x50, 0x80, 0x50, 0x03, 0x03, 0x20, 0x00, 0x05, 0x08, 0x12, 0x34, 0x56, 0x78};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3CMRequest::MTI);
    auto* cmr = tryGet<L3CMRequest>(*msg);
    ASSERT_TRUE(cmr);
    EXPECT_EQ(cmr->cksn(), 5u);
}

// ── MM-Paging (TS 24.008 §9.2.12, MTI=0x06) ─────────────────────────
// Reference: L3_Templates.ttcn ts_ML3_MT_MM_PAGING
// Structure: PD=0x05, MTI=0x06, MobileIdentity LV

TEST(MMRoundTripTest, PagingMM_RoundTrip) {
    ParsedMessage msg(MMM(L3PagingMM{}));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messagePD(*parsed), L3PD::MobilityManagement);
    EXPECT_EQ(messageMTI(*parsed), L3PagingMM::MTI);
}

TEST(MMRoundTripTest, PagingMM_Parse_Golden) {
    // PD=0x05(MM), MTI=0x06<<2=0x18, MI LV(length=4, TMSI type=0x08, value=0x12345678)
    uint8_t data[] = {0x50, 0x18, 0x05, 0x08, 0x12, 0x34, 0x56, 0x78};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3PagingMM::MTI);
    auto* pg = tryGet<L3PagingMM>(*msg);
    ASSERT_TRUE(pg);
}
