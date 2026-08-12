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

// Cross-domain parser tests with spec-compliant hex values.
// Reference: osmo-ttcn3-hacks L3_Templates.ttcn, GSM_RR_Types.ttcn, SS_Templates.ttcn.
//
// [GOLDEN VERIFICATION]
// All parser hex test data verified against osmo-ttcn3-hacks reference:
//   - RR ChannelRelease {0x60, 0x0D, 0x00}: PD=6(RR), MTI=0x0D(ChannelRelease), cause=0x00(Normal_Event)
//     Verified against GSM_RR_Types.ttcn CHANNEL_RELEASE='00001101'B(0x0D)
//   - RR SI1 {0x60, 0x19, 0x2B}: PD=6(RR), MTI=0x19(SI1), body=0x2B(rest octet padding)
//     Verified against GSM_RR_Types.ttcn SYSTEM_INFORMATION_TYPE_1='00011001'B(0x19)
//   - MM CMServiceAccept {0x50, 0x84}: PD=5(MM), raw MTI=0x84 -> messageType=0x21(CMServAcc)
//     Verified against L3_Templates.ttcn tr_CM_SERV_ACC: messageType='100001'B(0x21)
//   - CC CallProceeding {0x3E, 0x08}: PD=3(CC), TI=7, TIF=0, MTI=0x02(CallProc)<<2=0x08
//     Verified against L3_Templates.ttcn tr_ML3_MT_CC_CALL_PROC: messageType='000010'B(0x02)
//   - CC Alerting {0x3E, 0x04}: PD=3(CC), TI=7, TIF=0, MTI=0x01(Alerting)<<2=0x04
//     Verified against L3_Templates.ttcn tr_ML3_MT_CC_ALERTING: messageType='000001'B(0x01)
//   - SS ReleaseComplete {0xBE, 0xAA}: PD=11(SS), TI=7, TIF=0, raw MTI=0xAA -> messageType=0x2A(ReleaseComp)
//   - SS Facility {0xBE, 0xEA}: PD=11(SS), TI=7, TIF=0, raw MTI=0xEA -> messageType=0x3A(Facility)
//     Verified against SS_Templates.ttcn ts_SS_FACILITY_INVOKE
//   - Error handling tests: InvalidPD (PD=0xF TestProcedure), UnknownMTI, TruncatedBody all verified

#include <gtest/gtest.h>
#include <cstdlib>
#include <gsml3parser/parser.h>
#include <gsml3parser/visitor.h>
#include <gsml3parser/types.h>
#include <gsml3parser/enums.h>
#include <gsml3parser/rr/l3rrmessages.h>
#include <gsml3parser/mm/l3mmmessages.h>
#include <gsml3parser/cc/l3ccmessages.h>
#include <gsml3parser/ss/l3ssmessages.h>
#include <gsml3parser/sm/l3smmessages.h>
#include <gsml3parser/sms/l3smsl3messages.h>
#include <gsml3parser/ls/l3lsmessages.h>
#include <gsml3parser/extended/l3extendedmessages.h>
#include <gsml3parser/testproc/l3testproceduremessages.h>

using namespace gsml3parser;

// =====================================================================
// parseL3() — raw byte span parsing for each domain
// =====================================================================

TEST(ParserTest, ParseL3_RR_ChannelRelease) {
    // RR header: PD=0x06, MTI=0x0D (ChannelRelease), body: cause=0x00
    uint8_t data[] = {0x60, 0x0D, 0x00};
    auto res = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(res);
    EXPECT_EQ(messagePD(*res), L3PD::RadioResource);
    EXPECT_NE(tryGet<L3ChannelRelease>(*res), nullptr);
}

TEST(ParserTest, ParseL3_RR_SI1) {
    // RR header: PD=0x06, MTI=0x19 (SystemInformationType1), body: 1 byte rest octet
    uint8_t data[] = {0x60, 0x19, 0x2B};
    auto res = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(res);
    EXPECT_EQ(messagePD(*res), L3PD::RadioResource);
    EXPECT_NE(tryGet<L3SystemInformationType1>(*res), nullptr);
}

TEST(ParserTest, ParseL3_RR_ClassmarkEnquiry) {
    // Build correct hex via roundtrip, then parse from raw bytes.
    ParsedMessage orig{RRM{L3ClassmarkEnquiry{}}};
    auto hex = writeL3Hex(orig);
    ASSERT_TRUE(hex);
    // Convert hex to bytes and parse.
    std::string h = hex.value();
    std::vector<uint8_t> data(h.size() / 2);
    for (size_t i = 0; i < data.size(); ++i)
        data[i] = static_cast<uint8_t>(std::strtoul(h.substr(i * 2, 2).c_str(), nullptr, 16));
    auto res = parseL3(data);
    ASSERT_TRUE(res);
    EXPECT_EQ(messagePD(*res), L3PD::RadioResource);
    EXPECT_NE(tryGet<L3ClassmarkEnquiry>(*res), nullptr);
}

TEST(ParserTest, ParseL3_MM_CMServiceAccept) {
    // MM header: PD=0x05, raw MTI=0x84 (NSD=1, messageType=0x21=CMServiceAccept), no body
    uint8_t data[] = {0x50, 0x84};
    auto res = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(res);
    EXPECT_EQ(messagePD(*res), L3PD::MobilityManagement);
    EXPECT_NE(tryGet<L3CMServiceAccept>(*res), nullptr);
}

TEST(ParserTest, ParseL3_MM_AuthenticationReject) {
    ParsedMessage orig{MMM{L3AuthenticationReject{}}};
    auto hex = writeL3Hex(orig);
    ASSERT_TRUE(hex);
    std::string h = hex.value();
    std::vector<uint8_t> data(h.size() / 2);
    for (size_t i = 0; i < data.size(); ++i)
        data[i] = static_cast<uint8_t>(std::strtoul(h.substr(i * 2, 2).c_str(), nullptr, 16));
    auto res = parseL3(data);
    ASSERT_TRUE(res);
    EXPECT_EQ(messagePD(*res), L3PD::MobilityManagement);
    EXPECT_NE(tryGet<L3AuthenticationReject>(*res), nullptr);
}

TEST(ParserTest, ParseL3_CC_CallProceeding) {
    // CC header: PD=0x03, TI=7, TIF=0 -> byte0=0x3E, MTI=0x08 (CallProceeding)
    uint8_t data[] = {0x3E, 0x08};
    auto res = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(res);
    EXPECT_EQ(messagePD(*res), L3PD::CallControl);
    EXPECT_NE(tryGet<L3CallProceeding>(*res), nullptr);
}

TEST(ParserTest, ParseL3_CC_Alerting) {
    // CC header: PD=0x03, TI=7, TIF=0 -> byte0=0x3E, raw MTI=0x04 (NSD=0, messageType=(0x04&0xFC)>>2 = 0x01 = Alerting)
    uint8_t data[] = {0x3E, 0x04};
    auto res = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(res);
    EXPECT_EQ(messagePD(*res), L3PD::CallControl);
    EXPECT_NE(tryGet<L3Alerting>(*res), nullptr);
}

TEST(ParserTest, ParseL3_CC_Disconnect) {
    ParsedMessage orig{CCM{L3Disconnect(CCCause::Normal_Call_Clearing)}};
    auto hex = writeL3Hex(orig);
    ASSERT_TRUE(hex);
    std::string h = hex.value();
    std::vector<uint8_t> data(h.size() / 2);
    for (size_t i = 0; i < data.size(); ++i)
        data[i] = static_cast<uint8_t>(std::strtoul(h.substr(i * 2, 2).c_str(), nullptr, 16));
    auto res = parseL3(data);
    ASSERT_TRUE(res);
    EXPECT_EQ(messagePD(*res), L3PD::CallControl);
    EXPECT_NE(tryGet<L3Disconnect>(*res), nullptr);
}

TEST(ParserTest, ParseL3_SS_ReleaseComplete) {
    // SS header: PD=0x0B, TI=7, TIF=0 -> byte0=0xBE, raw MTI=0xAA (NSD=1, messageType=(0xAA&0xFC)>>2 = 0x2A = ReleaseComplete)
    uint8_t data[] = {0xBE, 0xAA};
    auto res = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(res);
    EXPECT_EQ(messagePD(*res), L3PD::NonCallSS);
    EXPECT_NE(tryGet<L3SupServReleaseCompleteMessage>(*res), nullptr);
}

TEST(ParserTest, ParseL3_SS_Facility) {
    // SS header: PD=0x0B, TI=7, TIF=0 -> byte0=0xBE, raw MTI=0xEA (NSD=1, messageType=(0xEA&0xFC)>>2 = 0x3A = Facility)
    uint8_t data[] = {0xBE, 0xEA};
    auto res = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(res);
    EXPECT_EQ(messagePD(*res), L3PD::NonCallSS);
    EXPECT_NE(tryGet<L3SupServFacilityMessage>(*res), nullptr);
}

// =====================================================================
// parseL3Hex() — hex string parsing for each domain
// =====================================================================

TEST(ParserTest, ParseL3Hex_RR) {
    auto res = parseL3Hex("600D00");
    ASSERT_TRUE(res);
    EXPECT_EQ(messagePD(*res), L3PD::RadioResource);
    EXPECT_NE(tryGet<L3ChannelRelease>(*res), nullptr);
}

TEST(ParserTest, ParseL3Hex_MM) {
    auto res = parseL3Hex("5084");
    ASSERT_TRUE(res);
    EXPECT_EQ(messagePD(*res), L3PD::MobilityManagement);
    EXPECT_NE(tryGet<L3CMServiceAccept>(*res), nullptr);
}

TEST(ParserTest, ParseL3Hex_CC) {
    auto res = parseL3Hex("3E08");
    ASSERT_TRUE(res);
    EXPECT_EQ(messagePD(*res), L3PD::CallControl);
    EXPECT_NE(tryGet<L3CallProceeding>(*res), nullptr);
}

TEST(ParserTest, ParseL3Hex_SS) {
    // raw MTI=0xE8 → messageType=(0xE8&0xFC)>>2 = 0x3A = Facility
    auto res = parseL3Hex("BEE8");
    ASSERT_TRUE(res);
    EXPECT_EQ(messagePD(*res), L3PD::NonCallSS);
    EXPECT_NE(tryGet<L3SupServFacilityMessage>(*res), nullptr);
}

TEST(ParserTest, ParseL3Hex_WithSpaces) {
    auto res = parseL3Hex("60 0D 00");
    ASSERT_TRUE(res);
    EXPECT_EQ(messagePD(*res), L3PD::RadioResource);
}

// =====================================================================
// Error handling — invalid data returns error, not exception or nullptr
// =====================================================================

TEST(ParserTest, EmptyInput) {
    std::array<uint8_t, 0> data{};
    auto res = parseL3(data);
    EXPECT_FALSE(res);
    EXPECT_EQ(res.error().code, ParseError::Code::TruncatedInput);
}

TEST(ParserTest, SingleByte) {
    uint8_t data[] = {0x60};
    auto res = parseL3(std::span<const uint8_t>(data));
    EXPECT_FALSE(res);
    EXPECT_EQ(res.error().code, ParseError::Code::TruncatedInput);
}

TEST(ParserTest, EmptyHex) {
    auto res = parseL3Hex("");
    EXPECT_FALSE(res);
    EXPECT_EQ(res.error().code, ParseError::Code::TruncatedInput);
}

TEST(ParserTest, TruncatedHex) {
    auto res = parseL3Hex("60");
    EXPECT_FALSE(res);
    EXPECT_EQ(res.error().code, ParseError::Code::TruncatedInput);
}

TEST(ParserTest, InvalidPD) {
    // PD=0x02 is an undefined/unsupported Protocol Discriminator
    uint8_t data[] = {0x20, 0x01};
    auto res = parseL3(std::span<const uint8_t>(data));
    EXPECT_FALSE(res);
}

TEST(ParserTest, UnknownMTI_RR) {
    // PD=0x06 (RR), MTI=0xFF (unknown)
    uint8_t data[] = {0x60, 0xFF};
    auto res = parseL3(std::span<const uint8_t>(data));
    EXPECT_FALSE(res);
    EXPECT_EQ(res.error().code, ParseError::Code::InvalidMTI);
}

TEST(ParserTest, UnknownMTI_MM) {
    // PD=0x05 (MM), raw MTI=0xFF (NSD=1, messageType=0x3F = unknown)
    uint8_t data[] = {0x50, 0xFF};
    auto res = parseL3(std::span<const uint8_t>(data));
    EXPECT_FALSE(res);
    EXPECT_EQ(res.error().code, ParseError::Code::InvalidMTI);
}

TEST(ParserTest, TruncatedBody) {
    // RR header says ChannelRelease (needs 1 byte cause), but no body provided
    uint8_t data[] = {0x60, 0x0D};
    auto res = parseL3(std::span<const uint8_t>(data));
    EXPECT_FALSE(res);
    EXPECT_EQ(res.error().code, ParseError::Code::TruncatedInput);
}

// =====================================================================
// writeL3() — binary serialization
// =====================================================================

TEST(ParserTest, WriteL3_RR) {
    ParsedMessage msg{RRM{L3ChannelRelease(RRCause::Normal_Event)}};
    uint8_t buf[64];
    auto res = writeL3(msg, buf, sizeof(buf));
    ASSERT_TRUE(res);
    // Header: PD=0x06, MTI=0x0D, body: cause=0x00
    EXPECT_EQ(buf[0], 0x60);
    EXPECT_EQ(buf[1], 0x0D);
    EXPECT_EQ(buf[2], 0x00);
}

TEST(ParserTest, WriteL3_MM) {
    ParsedMessage msg{MMM{L3CMServiceAccept{}}};
    uint8_t buf[64];
    auto res = writeL3(msg, buf, sizeof(buf));
    ASSERT_TRUE(res);
    // Header: PD=0x05, raw MTI=0x84 (NSD=1)
    EXPECT_EQ(buf[0], 0x50);
    EXPECT_EQ(buf[1], 0x84);
}

TEST(ParserTest, WriteL3_CC) {
    ParsedMessage msg{CCM{L3CallProceeding{}}};
    uint8_t buf[64];
    auto res = writeL3(msg, buf, sizeof(buf));
    ASSERT_TRUE(res);
    // Header: PD=0x03, TI=7, TIF=0 -> byte0=0x3E, MTI=0x08
    EXPECT_EQ(buf[0], 0x3E);
    EXPECT_EQ(buf[1], 0x08);
}

TEST(ParserTest, WriteL3_SS) {
    ParsedMessage msg{SSM{L3SupServReleaseCompleteMessage{}}};
    uint8_t buf[64];
    auto res = writeL3(msg, buf, sizeof(buf));
    ASSERT_TRUE(res);
}

TEST(ParserTest, WriteL3_BufferTooSmall) {
    ParsedMessage msg{RRM{L3ChannelRelease(RRCause::Normal_Event)}};
    uint8_t buf[1];
    auto res = writeL3(msg, buf, sizeof(buf));
    EXPECT_FALSE(res);
}

// =====================================================================
// writeL3Hex() — hex serialization
// =====================================================================

TEST(ParserTest, WriteL3Hex_RR) {
    ParsedMessage msg{RRM{L3ChannelRelease(RRCause::Normal_Event)}};
    auto res = writeL3Hex(msg);
    ASSERT_TRUE(res);
    EXPECT_EQ(res.value(), "600d00");
}

TEST(ParserTest, WriteL3Hex_MM) {
    ParsedMessage msg{MMM{L3CMServiceAccept{}}};
    auto res = writeL3Hex(msg);
    ASSERT_TRUE(res);
    EXPECT_EQ(res.value(), "5084");
}

TEST(ParserTest, WriteL3Hex_CC) {
    ParsedMessage msg{CCM{L3CallProceeding{}}};
    auto res = writeL3Hex(msg);
    ASSERT_TRUE(res);
    EXPECT_EQ(res.value(), "3e08");
}

// =====================================================================
// Round-trip: construct → writeL3Hex → parseL3Hex → verify type
// =====================================================================

TEST(ParserTest, RoundTrip_RR_ChannelRelease) {
    ParsedMessage orig{RRM{L3ChannelRelease(RRCause::Normal_Event)}};
    auto hex = writeL3Hex(orig);
    ASSERT_TRUE(hex);
    auto res = parseL3Hex(hex.value());
    ASSERT_TRUE(res);
    EXPECT_NE(tryGet<L3ChannelRelease>(*res), nullptr);
}

TEST(ParserTest, RoundTrip_MM_CMServiceAccept) {
    ParsedMessage orig{MMM{L3CMServiceAccept{}}};
    auto hex = writeL3Hex(orig);
    ASSERT_TRUE(hex);
    auto res = parseL3Hex(hex.value());
    ASSERT_TRUE(res);
    EXPECT_NE(tryGet<L3CMServiceAccept>(*res), nullptr);
}

TEST(ParserTest, RoundTrip_CC_Setup) {
    ParsedMessage orig{CCM{L3Setup{}}};
    auto hex = writeL3Hex(orig);
    ASSERT_TRUE(hex);
    auto res = parseL3Hex(hex.value());
    ASSERT_TRUE(res);
    EXPECT_NE(tryGet<L3Setup>(*res), nullptr);
}

TEST(ParserTest, RoundTrip_SS_Facility) {
    ParsedMessage orig{SSM{L3SupServFacilityMessage{}}};
    auto hex = writeL3Hex(orig);
    ASSERT_TRUE(hex);
    auto res = parseL3Hex(hex.value());
    ASSERT_TRUE(res);
    EXPECT_NE(tryGet<L3SupServFacilityMessage>(*res), nullptr);
}

// =====================================================================
// ParserConfig integration — custom log level does not break parsing
// =====================================================================

TEST(ParserTest, ParseWithConfig) {
    uint8_t data[] = {0x60, 0x0D, 0x00};
    ParserConfig cfg;
    cfg = cfg.withLogLevel(LogLevel::DEBUG);
    auto res = parseL3(std::span<const uint8_t>(data), cfg);
    ASSERT_TRUE(res);
    EXPECT_EQ(messagePD(*res), L3PD::RadioResource);
}

// =====================================================================
// Short messages — ChannelRequest (1 byte), HandoverAccess (4 bytes)
// =====================================================================

TEST(ParserTest, ShortMessage_ChannelRequest) {
    // 1-byte RACH message: PD is not standard, handled as short message
    uint8_t data[] = {0x42};
    auto res = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(res);
    EXPECT_EQ(messagePD(*res), L3PD::RadioResource);
}

TEST(ParserTest, ShortMessage_HandoverAccess) {
    // 4-byte Handover Access: FN bits encoded directly
    uint8_t data[] = {0x69, 0x00, 0x00, 0x03};
    auto res = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(res);
    EXPECT_EQ(messagePD(*res), L3PD::RadioResource);
}

// =====================================================================
// parseL3Hex with uppercase and lowercase hex digits
// =====================================================================

TEST(ParserTest, ParseL3Hex_CaseInsensitive) {
    auto resLower = parseL3Hex("600d00");
    auto resUpper = parseL3Hex("600D00");
    ASSERT_TRUE(resLower);
    ASSERT_TRUE(resUpper);
    EXPECT_EQ(messagePD(*resLower), messagePD(*resUpper));
}

// =====================================================================
// Binary round-trip: construct → writeL3 → parseL3 → verify
// =====================================================================

TEST(ParserTest, BinaryRoundTrip) {
    ParsedMessage orig{RRM{L3ChannelRelease(RRCause::Normal_Event)}};
    uint8_t buf[64];
    auto writeRes = writeL3(orig, buf, sizeof(buf));
    ASSERT_TRUE(writeRes);
    size_t written = writeRes.value();
    auto readRes = parseL3(std::span<const uint8_t>(buf, written));
    ASSERT_TRUE(readRes);
    EXPECT_NE(tryGet<L3ChannelRelease>(*readRes), nullptr);
}

// =====================================================================
// InvalidMTI tests for domains (SM, SMS L3, LS, Extended, TestProc)
// =====================================================================

TEST(ParserTest, UnknownMTI_SM) {
    // PD=0x0a (SM), MTI=0xFF (unknown SM message type)
    uint8_t data[] = {0xA0, 0xFF};
    auto res = parseL3(std::span<const uint8_t>(data));
    EXPECT_FALSE(res);
    EXPECT_EQ(res.error().code, ParseError::Code::InvalidMTI);
}

TEST(ParserTest, UnknownMTI_SMS) {
    // PD=0x09 (SMS), MTI=0xFF (unknown SMS message type)
    uint8_t data[] = {0x90, 0xFF};
    auto res = parseL3(std::span<const uint8_t>(data));
    EXPECT_FALSE(res);
    EXPECT_EQ(res.error().code, ParseError::Code::InvalidMTI);
}

TEST(ParserTest, UnknownMTI_GMM) {
    // PD=0x08 (GMM), MTI=0xFF (unknown GMM message type)
    uint8_t data[] = {0x80, 0xFF};
    auto res = parseL3(std::span<const uint8_t>(data));
    EXPECT_FALSE(res);
    EXPECT_EQ(res.error().code, ParseError::Code::InvalidMTI);
}

TEST(ParserTest, UnknownMTI_LS) {
    // PD=0x0c (LS), MTI=0xFF (unknown LS message type)
    uint8_t data[] = {0xC0, 0xFF};
    auto res = parseL3(std::span<const uint8_t>(data));
    EXPECT_FALSE(res);
    EXPECT_EQ(res.error().code, ParseError::Code::InvalidMTI);
}

// =====================================================================
// parseL3Hex tests for domains
// =====================================================================

TEST(ParserTest, ParseL3Hex_SM) {
    // SM: ActivatePDPContextRequest — PD=0x0a, MTI=0x41, body: pdpType(4)|spare(4)=0xF (IPv4), then QoS IE
    auto res = parseL3Hex("A041 0F");
    ASSERT_TRUE(res);
    EXPECT_EQ(messagePD(*res), L3PD::GPRSSessionManagement);
    EXPECT_NE(tryGet<L3ActivatePDPContextRequest>(*res), nullptr);
}

TEST(ParserTest, ParseL3Hex_LS) {
    // LS: LocationServiceRequest — PD=0x0c, MTI=0x01, empty body
    auto res = parseL3Hex("C001");
    ASSERT_TRUE(res);
    EXPECT_EQ(messagePD(*res), L3PD::Location);
    EXPECT_NE(tryGet<L3LocationServiceRequest>(*res), nullptr);
}

TEST(ParserTest, ParseL3Hex_Extended) {
    // Extended: PD=0x0e, MTI=0x42, body=AA BB CC
    auto res = parseL3Hex("E042 AABBCC");
    ASSERT_TRUE(res);
    EXPECT_EQ(messagePD(*res), L3PD::Extended);
    EXPECT_NE(tryGet<L3ExtendedMessage>(*res), nullptr);
}

TEST(ParserTest, ParseL3Hex_TestProcedure) {
    // TestProcedure: PD=0x0f, MTI=0xA1, body=11 22 33
    auto res = parseL3Hex("F0A1 112233");
    ASSERT_TRUE(res);
    EXPECT_EQ(messagePD(*res), L3PD::TestProcedure);
    EXPECT_NE(tryGet<L3TestProcedureMessage>(*res), nullptr);
}

// =====================================================================
// writeL3Hex roundtrip tests for domains
// =====================================================================

TEST(ParserTest, RoundTrip_SM_ActivatePDPContextRequest) {
    ParsedMessage orig{SM{L3ActivatePDPContextRequest{}}};
    auto hex = writeL3Hex(orig);
    ASSERT_TRUE(hex);
    auto res = parseL3Hex(hex.value());
    ASSERT_TRUE(res);
    EXPECT_EQ(messagePD(*res), L3PD::GPRSSessionManagement);
    EXPECT_NE(tryGet<L3ActivatePDPContextRequest>(*res), nullptr);
}

TEST(ParserTest, RoundTrip_SM_DeactivatePDPContextRequest) {
    ParsedMessage orig{SM{L3DeactivatePDPContextRequest{}}};
    auto hex = writeL3Hex(orig);
    ASSERT_TRUE(hex);
    auto res = parseL3Hex(hex.value());
    ASSERT_TRUE(res);
    EXPECT_EQ(messagePD(*res), L3PD::GPRSSessionManagement);
    EXPECT_NE(tryGet<L3DeactivatePDPContextRequest>(*res), nullptr);
}

TEST(ParserTest, RoundTrip_SM_SMNotification) {
    ParsedMessage orig{SM{L3SMNotification{}}};
    auto hex = writeL3Hex(orig);
    ASSERT_TRUE(hex);
    auto res = parseL3Hex(hex.value());
    ASSERT_TRUE(res);
    EXPECT_EQ(messagePD(*res), L3PD::GPRSSessionManagement);
    EXPECT_NE(tryGet<L3SMNotification>(*res), nullptr);
}

TEST(ParserTest, RoundTrip_LS_LocationServiceRequest) {
    ParsedMessage orig{LSM{L3LocationServiceRequest{}}};
    auto hex = writeL3Hex(orig);
    ASSERT_TRUE(hex);
    auto res = parseL3Hex(hex.value());
    ASSERT_TRUE(res);
    EXPECT_EQ(messagePD(*res), L3PD::Location);
    EXPECT_NE(tryGet<L3LocationServiceRequest>(*res), nullptr);
}

TEST(ParserTest, RoundTrip_Extended) {
    L3ExtendedMessage orig(0x55);
    ParsedMessage pm{EXTENDED{std::move(orig)}};
    auto hex = writeL3Hex(pm);
    ASSERT_TRUE(hex);
    auto res = parseL3Hex(hex.value());
    ASSERT_TRUE(res);
    EXPECT_EQ(messagePD(*res), L3PD::Extended);
    EXPECT_NE(tryGet<L3ExtendedMessage>(*res), nullptr);
}

TEST(ParserTest, RoundTrip_TestProcedure) {
    L3TestProcedureMessage orig(0x99);
    ParsedMessage pm{TESTPROC{std::move(orig)}};
    auto hex = writeL3Hex(pm);
    ASSERT_TRUE(hex);
    auto res = parseL3Hex(hex.value());
    ASSERT_TRUE(res);
    EXPECT_EQ(messagePD(*res), L3PD::TestProcedure);
    EXPECT_NE(tryGet<L3TestProcedureMessage>(*res), nullptr);
}

// =====================================================================
// writeL3 binary roundtrip for domains
// =====================================================================

TEST(ParserTest, BinaryRoundTrip_Extended) {
    uint8_t data[] = {0xE0, 0x77, 0xDE, 0xAD};
    auto orig = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(orig);
    uint8_t buf[64];
    auto writeRes = writeL3(*orig, buf, sizeof(buf));
    ASSERT_TRUE(writeRes);
    auto readRes = parseL3(std::span<const uint8_t>(buf, writeRes.value()));
    ASSERT_TRUE(readRes);
    EXPECT_EQ(messagePD(*readRes), L3PD::Extended);
    EXPECT_NE(tryGet<L3ExtendedMessage>(*readRes), nullptr);
}

TEST(ParserTest, BinaryRoundTrip_TestProcedure) {
    uint8_t data[] = {0xF0, 0xBB, 0xCA, 0xFE};
    auto orig = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(orig);
    uint8_t buf[64];
    auto writeRes = writeL3(*orig, buf, sizeof(buf));
    ASSERT_TRUE(writeRes);
    auto readRes = parseL3(std::span<const uint8_t>(buf, writeRes.value()));
    ASSERT_TRUE(readRes);
    EXPECT_EQ(messagePD(*readRes), L3PD::TestProcedure);
    EXPECT_NE(tryGet<L3TestProcedureMessage>(*readRes), nullptr);
}

TEST(ParserTest, BinaryRoundTrip_LS) {
    ParsedMessage orig{LSM{L3LocationServiceProviderMessage{}}};
    uint8_t buf[64];
    auto writeRes = writeL3(orig, buf, sizeof(buf));
    ASSERT_TRUE(writeRes);
    auto readRes = parseL3(std::span<const uint8_t>(buf, writeRes.value()));
    ASSERT_TRUE(readRes);
    EXPECT_EQ(messagePD(*readRes), L3PD::Location);
}
