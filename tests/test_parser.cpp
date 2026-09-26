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
//
// [GOLDEN VERIFICATION]
// All parser hex test data follow the TS 24.008 / TS 44.018 L3 protocol
// header layout: octet 0 = TI(7:5) | TIF(4) | PD(3:0); octet 1 carries the
// message type (six low bits for MM/CC/NC-SS/GCC/BCC, raw octet otherwise):
//   - RR ChannelRelease {0x06, 0x0D, 0x00}: PD=6(RR) low nibble, MTI=0x0D(ChannelRelease), cause=0x00(Normal_Event)
//   - RR SI1 {0x06, 0x19, 0x2B}: PD=6(RR), MTI=0x19(SI1), body=0x2B(rest octet padding)
//   - MM CMServiceAccept {0x05, 0x21}: PD=5(MM), MT=0x21(CMServAcc) in the six low bits of octet 1
//   - CC CallProceeding {0xE3, 0x02}: PD=3(CC), TI=7 (bits 7:5), TIF=0, MT=0x02(CallProc)
//   - CC Alerting {0xE3, 0x01}: PD=3(CC), TI=7, TIF=0, MT=0x01(Alerting)
//   - SS ReleaseComplete {0xEB, 0x2A}: PD=11(SS), TI=7, TIF=0, MT=0x2A(ReleaseComp)
//   - SS Facility {0xEB, 0x3A}: PD=11(SS), TI=7, TIF=0, MT=0x3A(Facility)
//   - Error handling tests: InvalidPD (reserved low nibble), UnknownMTI, TruncatedBody all verified

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
#include <gsml3parser/sms/l3smsmessages.h>
#include <gsml3parser/ls/l3lsmessages.h>
#include <gsml3parser/extended/l3extendedmessages.h>
#include <gsml3parser/testproc/l3testproceduremessages.h>

using namespace gsml3parser;

// =====================================================================
// parseL3() - raw byte span parsing for each domain
// =====================================================================

TEST(ParserTest, ParseL3_RR_ChannelRelease) {
    // RR header: PD=0x06 (low nibble of octet 0), MTI=0x0D (ChannelRelease), body: cause=0x00
    uint8_t data[] = {0x06, 0x0D, 0x00};
    auto res = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(res);
    EXPECT_EQ(messagePD(*res), L3PD::RadioResource);
    EXPECT_NE(tryGet<L3ChannelRelease>(*res), nullptr);
}

TEST(ParserTest, ParseL3_RR_SI1) {
    // SI1 has a long fixed body (cell channel description + RACH control
    // parameters), so build a complete message and parse its serialized bytes.
    ParsedMessage orig{RRM{L3SystemInformationType1{}}};
    auto hex = writeL3Hex(orig);
    ASSERT_TRUE(hex);
    std::string h = hex.value();
    std::vector<uint8_t> data(h.size() / 2);
    for (size_t i = 0; i < data.size(); ++i)
        data[i] = static_cast<uint8_t>(std::strtoul(h.substr(i * 2, 2).c_str(), nullptr, 16));
    auto res = parseL3(data);
    ASSERT_TRUE(res);
    EXPECT_EQ(messagePD(*res), L3PD::RadioResource);
    EXPECT_NE(tryGet<L3SystemInformationType1>(*res), nullptr);
}

TEST(ParserTest, TruncatedSI1_ReturnsError) {
    // SI1 header with only a 1-byte body: truncated input must be a hard
    // error (TruncatedInput), never a silently default-constructed message.
    uint8_t data[] = {0x06, 0x19, 0x2B};
    auto res = parseL3(std::span<const uint8_t>(data));
    EXPECT_FALSE(res);
    EXPECT_EQ(res.error().code, ParseError::Code::TruncatedInput);
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
    // MM header: PD=0x05 (low nibble of octet 0); octet 1 = 0x21
    // (MT=CMServiceAccept in the six low bits, NSD=0), no body
    uint8_t data[] = {0x05, 0x21};
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
    // CC header: PD=0x03 low nibble, TI=7 in bits 7:5, TIF=0 -> byte0=0xE3;
    // octet 1 = 0x02 (MT=CallProceeding, NSD=0)
    uint8_t data[] = {0xE3, 0x02};
    auto res = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(res);
    EXPECT_EQ(messagePD(*res), L3PD::CallControl);
    EXPECT_NE(tryGet<L3CallProceeding>(*res), nullptr);
}

TEST(ParserTest, ParseL3_CC_Alerting) {
    // CC header: PD=0x03 low nibble, TI=7 in bits 7:5, TIF=0 -> byte0=0xE3;
    // octet 1 = 0x01 (MT=Alerting, NSD=0)
    uint8_t data[] = {0xE3, 0x01};
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
    // SS header: PD=0x0B low nibble, TI=7 in bits 7:5, TIF=0 -> byte0=0xEB;
    // octet 1 = 0x2A (MT=ReleaseComplete, NSD=0)
    uint8_t data[] = {0xEB, 0x2A};
    auto res = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(res);
    EXPECT_EQ(messagePD(*res), L3PD::NonCallSS);
    EXPECT_NE(tryGet<L3SupServReleaseCompleteMessage>(*res), nullptr);
}

TEST(ParserTest, ParseL3_SS_Facility) {
    // SS header: PD=0x0B low nibble, TI=7 in bits 7:5, TIF=0 -> byte0=0xEB;
    // octet 1 = 0x3A (MT=Facility, NSD=0)
    uint8_t data[] = {0xEB, 0x3A};
    auto res = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(res);
    EXPECT_EQ(messagePD(*res), L3PD::NonCallSS);
    EXPECT_NE(tryGet<L3SupServFacilityMessage>(*res), nullptr);
}

// =====================================================================
// parseL3Hex() - hex string parsing for each domain
// =====================================================================

TEST(ParserTest, ParseL3Hex_RR) {
    // PD=RR (low nibble of octet 0), MTI=Channel Release, cause=0x00.
    auto res = parseL3Hex("060D00");
    ASSERT_TRUE(res);
    EXPECT_EQ(messagePD(*res), L3PD::RadioResource);
    EXPECT_NE(tryGet<L3ChannelRelease>(*res), nullptr);
}

TEST(ParserTest, ParseL3Hex_MM) {
    // PD=MM; MT=CM Service Accept in the six low bits of octet 1 (NSD=0).
    auto res = parseL3Hex("0521");
    ASSERT_TRUE(res);
    EXPECT_EQ(messagePD(*res), L3PD::MobilityManagement);
    EXPECT_NE(tryGet<L3CMServiceAccept>(*res), nullptr);
}

TEST(ParserTest, ParseL3Hex_CC) {
    // PD=CC, TI=7 in bits 7:5; MT=Call Proceeding (NSD=0).
    auto res = parseL3Hex("E302");
    ASSERT_TRUE(res);
    EXPECT_EQ(messagePD(*res), L3PD::CallControl);
    EXPECT_NE(tryGet<L3CallProceeding>(*res), nullptr);
}

TEST(ParserTest, ParseL3Hex_SS) {
    // PD=SS, TI=7 in bits 7:5; MT=Facility in the six low bits (NSD=0).
    auto res = parseL3Hex("EB3A");
    ASSERT_TRUE(res);
    EXPECT_EQ(messagePD(*res), L3PD::NonCallSS);
    EXPECT_NE(tryGet<L3SupServFacilityMessage>(*res), nullptr);
}

TEST(ParserTest, ParseL3Hex_WithSpaces) {
    auto res = parseL3Hex("06 0D 00");
    ASSERT_TRUE(res);
    EXPECT_EQ(messagePD(*res), L3PD::RadioResource);
}

// =====================================================================
// Error handling - invalid data returns error, not exception or nullptr
// =====================================================================

TEST(ParserTest, EmptyInput) {
    std::array<uint8_t, 0> data{};
    auto res = parseL3(data);
    EXPECT_FALSE(res);
    EXPECT_EQ(res.error().code, ParseError::Code::TruncatedInput);
}

TEST(ParserTest, SingleByte) {
    // A single octet is a Channel Request: the whole octet is the 8-bit
    // request reference (RA), so any of the 256 values parses.
    uint8_t data[] = {0x60};
    auto res = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(res);
    EXPECT_EQ(messageMTI(*res), L3ChannelRequest::MTI);
    const auto* cr = tryGet<L3ChannelRequest>(*res);
    ASSERT_NE(cr, nullptr);
    EXPECT_EQ(cr->requestReference(), 0x60u);
}

TEST(ParserTest, EmptyHex) {
    auto res = parseL3Hex("");
    EXPECT_FALSE(res);
    EXPECT_EQ(res.error().code, ParseError::Code::TruncatedInput);
}

TEST(ParserTest, TruncatedHex) {
    // "060d" is an RR ChannelRelease header with no body bytes: the 8-bit
    // cause is missing, so the parse must fail with TruncatedInput.
    // (A single octet like "06" is a valid Channel Request.)
    auto res = parseL3Hex("060d");
    EXPECT_FALSE(res);
    EXPECT_EQ(res.error().code, ParseError::Code::TruncatedInput);
}

TEST(ParserTest, InvalidPD) {
    // PD=0x02 is a reserved Protocol Discriminator (TS 44.018 section 10.2);
    // it occupies the low nibble of octet 0.
    uint8_t data[] = {0x02, 0x01};
    auto res = parseL3(std::span<const uint8_t>(data));
    EXPECT_FALSE(res);
}

TEST(ParserTest, UnknownMTI_RR) {
    // PD=0x06 (RR), MTI=0xFF (unknown)
    uint8_t data[] = {0x06, 0xFF};
    auto res = parseL3(std::span<const uint8_t>(data));
    EXPECT_FALSE(res);
    EXPECT_EQ(res.error().code, ParseError::Code::InvalidMTI);
}

TEST(ParserTest, UnknownMTI_MM) {
    // PD=0x05 (MM), octet 1=0xFF -> MT=0x3F = unknown
    uint8_t data[] = {0x05, 0xFF};
    auto res = parseL3(std::span<const uint8_t>(data));
    EXPECT_FALSE(res);
    EXPECT_EQ(res.error().code, ParseError::Code::InvalidMTI);
}

TEST(ParserTest, TruncatedBody) {
    // RR header says ChannelRelease (needs 1 byte cause), but no body provided
    uint8_t data[] = {0x06, 0x0D};
    auto res = parseL3(std::span<const uint8_t>(data));
    EXPECT_FALSE(res);
    EXPECT_EQ(res.error().code, ParseError::Code::TruncatedInput);
}

// =====================================================================
// writeL3() - binary serialization
// =====================================================================

TEST(ParserTest, WriteL3_RR) {
    ParsedMessage msg{RRM{L3ChannelRelease(RRCause::Normal_Event)}};
    uint8_t buf[64];
    auto res = writeL3(msg, buf, sizeof(buf));
    ASSERT_TRUE(res);
    // Header: PD=0x06 (low nibble of octet 0), MTI=0x0D, body: cause=0x00
    EXPECT_EQ(buf[0], 0x06);
    EXPECT_EQ(buf[1], 0x0D);
    EXPECT_EQ(buf[2], 0x00);
}

TEST(ParserTest, WriteL3_MM) {
    ParsedMessage msg{MMM{L3CMServiceAccept{}}};
    uint8_t buf[64];
    auto res = writeL3(msg, buf, sizeof(buf));
    ASSERT_TRUE(res);
    // Header: PD=0x05 (low nibble of octet 0); octet 1 = MT in the six low
    // bits (CM Service Accept = 0x21), NSD=0
    EXPECT_EQ(buf[0], 0x05);
    EXPECT_EQ(buf[1], 0x21);
}

TEST(ParserTest, WriteL3_CC) {
    ParsedMessage msg{CCM{L3CallProceeding{}}};
    uint8_t buf[64];
    auto res = writeL3(msg, buf, sizeof(buf));
    ASSERT_TRUE(res);
    // Header: PD=0x03 low nibble, TI=7 in bits 7:5, TIF=0 -> byte0=0xE3;
    // octet 1 = MT (Call Proceeding = 0x02), NSD=0
    EXPECT_EQ(buf[0], 0xE3);
    EXPECT_EQ(buf[1], 0x02);
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
// writeL3Hex() - hex serialization
// =====================================================================

TEST(ParserTest, WriteL3Hex_RR) {
    ParsedMessage msg{RRM{L3ChannelRelease(RRCause::Normal_Event)}};
    auto res = writeL3Hex(msg);
    ASSERT_TRUE(res);
    EXPECT_EQ(res.value(), "060d00");
}

TEST(ParserTest, WriteL3Hex_MM) {
    ParsedMessage msg{MMM{L3CMServiceAccept{}}};
    auto res = writeL3Hex(msg);
    ASSERT_TRUE(res);
    EXPECT_EQ(res.value(), "0521");
}

TEST(ParserTest, WriteL3Hex_CC) {
    ParsedMessage msg{CCM{L3CallProceeding{}}};
    auto res = writeL3Hex(msg);
    ASSERT_TRUE(res);
    EXPECT_EQ(res.value(), "e302");
}

// =====================================================================
// writeL3Bytes() - raw byte vector serialization
// =====================================================================

// TS 44.018 9.1.7: Channel Release (RR, MTI=0x0D)
TEST(ParserTest, WriteL3Bytes_ReturnsRawBytes) {
    auto msg = parseL3Hex("060d00");
    ASSERT_TRUE(msg);
    auto bytes = writeL3Bytes(*msg);
    ASSERT_TRUE(bytes);
    EXPECT_EQ(bytes.value().size(), 3u);
    EXPECT_EQ(bytes.value()[0], 0x06);
    EXPECT_EQ(bytes.value()[1], 0x0D);
    EXPECT_EQ(bytes.value()[2], 0x00);
}

// =====================================================================
// Round-trip: construct -> writeL3Hex -> parseL3Hex -> verify type
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
// ParserConfig integration - custom log level does not break parsing
// =====================================================================

TEST(ParserTest, ParseWithConfig) {
    uint8_t data[] = {0x06, 0x0D, 0x00};
    ParserConfig cfg;
    cfg = cfg.withLogLevel(LogLevel::DEBUG);
    auto res = parseL3(std::span<const uint8_t>(data), cfg);
    ASSERT_TRUE(res);
    EXPECT_EQ(messagePD(*res), L3PD::RadioResource);
}

// =====================================================================
// Short messages - ChannelRequest (1 byte), HandoverAccess (4 bytes)
// =====================================================================

// RA 0x42: the single octet is not an L3 header at all.
TEST(ParserTest, ShortMessage_ChannelRequest) {
    // 1-byte RACH message: framed by length, no standard L3 header
    uint8_t data[] = {0x42};
    auto res = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(res);
    EXPECT_EQ(messagePD(*res), L3PD::RadioResource);
}

// Test: ALL 256 one-octet RACH values parse as ChannelRequest with the
// full 8-bit RA preserved.
// Importance: the RACH Channel Request is a single octet (TS 44.018 9.1.8);
// every RA value must parse with all eight bits kept intact, because the
// network echoes the full RA in the Immediate Assignment.
TEST(ParserTest, ShortMessage_ChannelRequest_AllRAValues) {
    for (int v = 0; v < 256; ++v) {
        uint8_t data[1] = {static_cast<uint8_t>(v)};
        auto res = parseL3(std::span<const uint8_t>(data, 1));
        ASSERT_TRUE(res) << "RA 0x" << std::hex << v << " must parse";
        EXPECT_EQ(messageMTI(*res), L3ChannelRequest::MTI);
        const auto* cr = tryGet<L3ChannelRequest>(*res);
        ASSERT_NE(cr, nullptr);
        EXPECT_EQ(cr->requestReference(), static_cast<uint8_t>(v))
            << "full 8-bit RA must round-trip (RA 0x" << std::hex << v << ")";
    }
}

TEST(ParserTest, ShortMessage_HandoverAccess) {
    // 4-byte Handover Access: FN bits encoded directly
    // Last byte 0x00: the 5 reserved bits are zero .
    uint8_t data[] = {0x69, 0x00, 0x00, 0x00};
    auto res = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(res);
    EXPECT_EQ(messagePD(*res), L3PD::RadioResource);
}

// Test: a 4-byte HandoverAccess whose first octet is a valid RR header
// (PD=RR in the low nibble) and whose second byte is a valid RR MTI with a
// shorter body must NOT be misparsed as the RR message. The standard parse
// wins only on exact frame consumption.
TEST(ParserTest, ShortMessage_HandoverAccess_RRPrefixNotMisparsed) {
    // {0x06, 0x12, 0x00, 0x00}: PD=RR (low nibble of octet 0), TIF=0, MTI
    // 0x12 (RR Status, 1-byte body) would consume only 3 of the 4 bytes ->
    // not exact -> the frame is a HandoverAccess. Last byte 0x00: the 5
    // reserved bits are zero.
    uint8_t data[] = {0x06, 0x12, 0x00, 0x00};
    auto res = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(res);
    EXPECT_EQ(messageMTI(*res), L3HandoverAccess::MTI)
        << "expected HandoverAccess, got MTI 0x" << std::hex << messageMTI(*res);
}

// Test: a 4-byte frame with non-zero HandoverAccess reserved bits is
// not misclassified as HandoverAccess. The standard parse wins instead:
// RR Status consumes 3 of the 4 bytes and the trailing octet is ignored
// in lenient mode (strict framing rejects it).
TEST(ParserTest, ShortMessage_HandoverAccess_ReservedBitsRejected) {
    uint8_t data[] = {0x06, 0x12, 0x00, 0x03}; // HandoverAccess reserved = 0x03 (non-zero)
    auto res = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(res);
    EXPECT_EQ(messageMTI(*res), L3RRStatus::MTI)
        << "must be parsed as RR Status, not misclassified as HandoverAccess";
}

// Test: L3HandoverAccess::parse directly rejects non-zero reserved bits
// .
TEST(ParserTest, HandoverAccess_Parse_ReservedBitsRejected) {
    uint8_t data[] = {0x17, 0x00, 0x00, 0x1F}; // all 5 reserved bits set
    BitReader br(data, 32);
    auto res = L3HandoverAccess::parse(br);
    ASSERT_FALSE(res);
    EXPECT_EQ(res.error().code, ParseError::Code::InvalidValue);
}

// Test: a genuine 4-byte CC message (Facility, 2-byte body) parses as CC —
// the short-message handler must not swallow it.
TEST(ParserTest, ShortMessage_ExactCCMessageWins) {
    auto fac = L3Facility::builder().ti(0).facilityBody({0x27, 0x00}).build();
    ParsedMessage pm{CCM{std::move(fac)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
    const auto& vec = bytes.value();
    ASSERT_EQ(vec.size(), 4u);
    auto res = parseL3(std::span<const uint8_t>(vec.data(), vec.size()));
    ASSERT_TRUE(res);
    EXPECT_EQ(messagePD(*res), L3PD::CallControl);
    EXPECT_EQ(messageMTI(*res), L3Facility::MTI);
}

// Test: a genuine 7-byte CC message (Facility, 5-byte body) parses as CC,
// not as SynchronizationChannelInformation.
TEST(ParserTest, ShortMessage_ExactCCMessageWins_7Bytes) {
    auto fac = L3Facility::builder().ti(1).facilityBody({0x27, 0x01, 0x02, 0x03, 0x04}).build();
    ParsedMessage pm{CCM{std::move(fac)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
    const auto& vec = bytes.value();
    ASSERT_EQ(vec.size(), 7u);
    auto res = parseL3(std::span<const uint8_t>(vec.data(), vec.size()));
    ASSERT_TRUE(res);
    EXPECT_EQ(messagePD(*res), L3PD::CallControl);
}

// Test: 4-byte frames whose low nibble of octet 0 is a reserved PD
// (0x02/0x04/0x07/0x0d) are HandoverAccess, not "invalid PD" errors
// (parseL3Header rejects reserved PDs, so the short-message path must
// still be reached for such frames).
TEST(ParserTest, ShortMessage_HandoverAccess_ReservedPDNibble) {
    for (uint8_t first : {0x02u, 0x04u, 0x07u, 0x0Du}) {
        // Last byte 0x00: the 5 reserved bits are zero .
        uint8_t data[] = {first, 0x00, 0x00, 0x00};
        auto res = parseL3(std::span<const uint8_t>(data));
        ASSERT_TRUE(res) << "first byte 0x" << std::hex << first;
        EXPECT_EQ(messageMTI(*res), L3HandoverAccess::MTI);
    }
}

// =====================================================================
// parseL3Hex with uppercase and lowercase hex digits
// =====================================================================

TEST(ParserTest, ParseL3Hex_CaseInsensitive) {
    auto resLower = parseL3Hex("060d00");
    auto resUpper = parseL3Hex("060D00");
    ASSERT_TRUE(resLower);
    ASSERT_TRUE(resUpper);
    EXPECT_EQ(messagePD(*resLower), messagePD(*resUpper));
}

// =====================================================================
// Binary round-trip: construct -> writeL3 -> parseL3 -> verify
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
    // PD=0x0a (SM) in the low nibble of octet 0, MTI=0xFF (unknown SM message type)
    uint8_t data[] = {0x0A, 0xFF};
    auto res = parseL3(std::span<const uint8_t>(data));
    EXPECT_FALSE(res);
    EXPECT_EQ(res.error().code, ParseError::Code::InvalidMTI);
}

TEST(ParserTest, UnknownMTI_SMS) {
    // PD=0x09 (SMS), MTI=0xFF (unknown SMS message type)
    uint8_t data[] = {0x09, 0xFF};
    auto res = parseL3(std::span<const uint8_t>(data));
    EXPECT_FALSE(res);
    EXPECT_EQ(res.error().code, ParseError::Code::InvalidMTI);
}

TEST(ParserTest, UnknownMTI_GMM) {
    // PD=0x08 (GMM), MTI=0xFF (unknown GMM message type)
    uint8_t data[] = {0x08, 0xFF};
    auto res = parseL3(std::span<const uint8_t>(data));
    EXPECT_FALSE(res);
    EXPECT_EQ(res.error().code, ParseError::Code::InvalidMTI);
}

TEST(ParserTest, UnknownMTI_LS) {
    // PD=0x0c (LS), MTI=0xFF (unknown LS message type)
    uint8_t data[] = {0x0C, 0xFF};
    auto res = parseL3(std::span<const uint8_t>(data));
    EXPECT_FALSE(res);
    EXPECT_EQ(res.error().code, ParseError::Code::InvalidMTI);
}

// Test: SMS MTI 0x12/0x13 overlap — the CP-layer parsers keep precedence
// over the L3-layer duplicates (the dispatch table must
// preserve the previous switch's first-case-wins behavior).
TEST(ParserTest, SMS_MTIOverlap_CPTakesPrecedence) {
    // CP-STATUS (MTI 0x12): body = 1-octet TP-Status.
    auto cpStatus = L3CPStatus::builder().tpOi(1).build();
    ParsedMessage pm1{SMS{std::move(cpStatus)}};
    auto bytes1 = writeL3Bytes(pm1);
    ASSERT_TRUE(bytes1);
    auto r1 = parseL3(std::span<const uint8_t>((*bytes1).data(), (*bytes1).size()));
    ASSERT_TRUE(r1);
    EXPECT_NE(tryGet<L3CPStatus>(*r1), nullptr) << "CP-STATUS must win MTI 0x12";

    // CP-SMT (MTI 0x13): body = Length(1) + RPDU.
    auto cpSmt = L3CPSMT::builder().rpdu({0x01, 0x02}).build();
    ParsedMessage pm2{SMS{std::move(cpSmt)}};
    auto bytes2 = writeL3Bytes(pm2);
    ASSERT_TRUE(bytes2);
    auto r2 = parseL3(std::span<const uint8_t>((*bytes2).data(), (*bytes2).size()));
    ASSERT_TRUE(r2);
    EXPECT_NE(tryGet<L3CPSMT>(*r2), nullptr) << "CP-SMT must win MTI 0x13";
}

// =====================================================================
// parseL3Hex tests for domains
// =====================================================================

TEST(ParserTest, ParseL3Hex_SM) {
    // SM: ActivatePDPContextRequest - PD=0x0a (low nibble), MTI=0x41, body: pdpType(4)|spare(4)=0xF (IPv4), then QoS IE
    auto res = parseL3Hex("0A41 0F");
    ASSERT_TRUE(res);
    EXPECT_EQ(messagePD(*res), L3PD::GPRSSessionManagement);
    EXPECT_NE(tryGet<L3ActivatePDPContextRequest>(*res), nullptr);
}

TEST(ParserTest, ParseL3Hex_LS) {
    // LS: LocationServiceRequest - PD=0x0c (low nibble), MTI=0x01, empty body
    auto res = parseL3Hex("0C01");
    ASSERT_TRUE(res);
    EXPECT_EQ(messagePD(*res), L3PD::Location);
    EXPECT_NE(tryGet<L3LocationServiceRequest>(*res), nullptr);
}

TEST(ParserTest, ParseL3Hex_Extended) {
    // Extended: PD=0x0e (low nibble), MTI=0x42, body=AA BB CC
    auto res = parseL3Hex("0E42 AABBCC");
    ASSERT_TRUE(res);
    EXPECT_EQ(messagePD(*res), L3PD::Extended);
    EXPECT_NE(tryGet<L3ExtendedMessage>(*res), nullptr);
}

TEST(ParserTest, ParseL3Hex_TestProcedure) {
    // TestProcedure: PD=0x0f (low nibble), MTI=0xA1, body=11 22 33
    auto res = parseL3Hex("0FA1 112233");
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
    uint8_t data[] = {0x0E, 0x77, 0xDE, 0xAD};
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
    uint8_t data[] = {0x0F, 0xBB, 0xCA, 0xFE};
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

// Test: strict framing rejects trailing bytes after a complete message
// (the lenient default ignored them silently).
TEST(ParserTest, StrictFraming_TrailingDataRejected) {
    // RR Status (3 bytes: 0x06 0x12 0x00) + 2 trailing bytes.
    uint8_t data[] = {0x06, 0x12, 0x00, 0x01, 0x02};
    auto lenient = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(lenient) << "lenient mode keeps ignoring the tail";

    auto strict = parseL3(std::span<const uint8_t>(data),
                          ParserConfig{}.withStrictFraming(true));
    ASSERT_FALSE(strict);
    EXPECT_EQ(strict.error().code, ParseError::Code::LengthMismatch);
}

// Test: strict framing accepts an exactly-consumed message.
TEST(ParserTest, StrictFraming_ExactMessageAccepted) {
    uint8_t data[] = {0x06, 0x0D, 0x00}; // Channel Release, exact
    auto strict = parseL3(std::span<const uint8_t>(data),
                          ParserConfig{}.withStrictFraming(true));
    ASSERT_TRUE(strict);
    EXPECT_EQ(messageMTI(*strict), L3ChannelRelease::MTI);
}
