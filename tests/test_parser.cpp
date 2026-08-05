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

#include <gtest/gtest.h>
#include <gsml3parser/parser.h>
#include <gsml3parser/rr/l3rrmessages.h>
#include <gsml3parser/mm/l3mmmessages.h>
#include <gsml3parser/cc/l3ccmessages.h>
#include <gsml3parser/ss/l3ssmessages.h>

using namespace gsml3parser;

static ParserContext ctx;

TEST(ParserTest, ParseNull) {
    EXPECT_FALSE(parseL3(std::span<const uint8_t>(), ctx));
    EXPECT_FALSE(parseL3Hex("", ctx));
}

TEST(ParserTest, ParseTooShort) {
    // Reference format: PD=0x06(RR) in high nibble, skip=0 → byte 0 = 0x60
    uint8_t data[] = {0x60};
    EXPECT_FALSE(parseL3(std::span<const uint8_t>(data), ctx));
}

// GSM 04.08 10.2: PD=0x06(RR) in high nibble, skip=0, MTI=0x0D(ChannelRelease), cause=0x00
// Reference: GSM_RR_Types.ttcn CHANNEL_RELEASE = '00001101'B = 0x0D
// Byte 0: PD(high=6) | skip(low=0) = 0x60
// Byte 1: MTI = 0x0D (RR uses full 8-bit messageType)
// Byte 2: cause = 0x00 (Normal_Event)
TEST(ParserTest, ParseRR_ChannelRelease) {
    uint8_t data[] = {0x60, 0x0D, 0x00};
    auto msg = parseL3(std::span<const uint8_t>(data), ctx);
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->PD(), L3PD::RadioResource);
    EXPECT_EQ(msg->MTI(), L3RRMessage::ChannelRelease);

    auto* chRelease = dynamic_cast<L3ChannelRelease*>(msg.get());
    ASSERT_TRUE(chRelease);
}

// GSM 04.08 10.3: PD=0x03(CC), TIO=0, TIF=0, messageType=100101(Disconnect=0x25), NSD=00
// Reference: L3_Templates.ttcn ts_ML3_MO_CC_DISC, GSM_RR_Types.ttcn RrHeader
// Byte 0: PD(high=3) | TIO+TIF(low=0) = 0x30
// Byte 1: messageType(6)<<2 | NSD(2) = 0x25<<2 | 0 = 0x94
// Cause TLV: GSM 04.08 10.5.4.11, IEI=0x08, length=0x02, octet3=0x16, octet4=0x21 (Normal_Call_Clearing)
TEST(ParserTest, ParseCC_Disconnect) {
    uint8_t data[] = {0x30, 0x94, 0x08, 0x02, 0x16, 0x21};
    auto msg = parseL3(std::span<const uint8_t>(data), ctx);
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->PD(), L3PD::CallControl);
    EXPECT_EQ(msg->MTI(), L3CCMessage::Disconnect);
}

// GSM 04.08 10.2: PD=0x05(MM), skip=0, messageType=100001(CMServiceAccept=0x21), NSD=00
// Reference: L3_Templates.ttcn tr_CM_SERV_ACC (discriminator='0101'B, messageType='100001'B)
// Byte 0: PD(high=5) | skip(low=0) = 0x50
// Byte 1: messageType(6)<<2 | NSD(2) = 0x21<<2 | 0 = 0x84
TEST(ParserTest, ParseMM_CMServiceAccept) {
    uint8_t data[] = {0x50, 0x84};
    auto msg = parseL3(std::span<const uint8_t>(data), ctx);
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->PD(), L3PD::MobilityManagement);
    EXPECT_EQ(msg->MTI(), L3MMMessage::CMServiceAccept);
}

// GSM 04.08 10.2: PD=0x0B(NonCallSS), TIO=0, TIF=0, messageType=111010(Facility=0x3A), NSD=00
// Reference: GSML3SSMessages.h Facility=0x3A, SS_Templates.ttcn
// Byte 0: PD(high=0xB) | TIO+TIF(low=0) = 0xB0
// Byte 1: messageType(6)<<2 | NSD(2) = 0x3A<<2 | 0 = 0xE8
TEST(ParserTest, ParseSS_Facility) {
    uint8_t data[] = {0xB0, 0xE8};
    auto msg = parseL3(std::span<const uint8_t>(data), ctx);
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->PD(), L3PD::NonCallSS);
    EXPECT_EQ(msg->MTI(), L3SupServMessage::Facility);
}

// GSM 04.08 10.2: PD=0x05(MM), skip=0, messageType=100001(CMServiceAccept=0x21)<<2|NSD=00
// Reference: L3_Templates.ttcn tr_CM_SERV_ACC
// "50" = PD(high=5)|skip(low=0), "84" = messageType(6)<<2|NSD(2)
TEST(ParserTest, ParseHex) {
    auto msg = parseL3Hex("5084", ctx);
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->PD(), L3PD::MobilityManagement);
    EXPECT_EQ(msg->MTI(), L3MMMessage::CMServiceAccept);
}

// GSM 04.08 10.2: PD=0x05(MM), skip=0, messageType=CMServiceAccept
// Reference: L3_Templates.ttcn tr_CM_SERV_ACC
TEST(ParserTest, ParseHexWithSpaces) {
    auto msg = parseL3Hex("50 84", ctx);
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->PD(), L3PD::MobilityManagement);
}

TEST(ParserTest, ParseUnknownPD) {
    // Reference: PD=0x0F(TestProcedure) in high nibble, skip=0
    uint8_t data[] = {0xF0, 0x01};
    auto msg = parseL3(std::span<const uint8_t>(data), ctx);
    EXPECT_FALSE(msg);
}

// GSM 04.08 10.2: PD=0x09(SMS) in high nibble, skip=0
// Reference: GSMCommon.h L3SMSPD=0x09
TEST(ParserTest, RegisterPDHandler) {
    ParserContext localCtx;
    bool handlerCalled = false;
    localCtx.registerPDHandler(L3PD::SMS, [&](const L3Frame&) {
        handlerCalled = true;
        return std::make_unique<L3CMServiceAccept>();
    });

    // Byte 0: PD(high=9) | skip(low=0) = 0x90
    uint8_t data[] = {0x90, 0x01};
    auto msg = parseL3(std::span<const uint8_t>(data), localCtx);
    EXPECT_TRUE(handlerCalled);
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->PD(), L3PD::MobilityManagement);

    localCtx.unregisterPDHandler(L3PD::SMS);
}

TEST(ParserTest, WriteAndParseRoundTrip) {
    L3ChannelRelease chRelease(RRCause::Normal_Event);

    std::vector<uint8_t> buf(chRelease.fullLength());
    size_t n = writeL3(chRelease, buf.data(), buf.size());
    EXPECT_GT(n, 0u);

    auto msg = parseL3(std::span<const uint8_t>(buf), ctx);
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->PD(), L3PD::RadioResource);
    EXPECT_EQ(msg->MTI(), L3RRMessage::ChannelRelease);
}

TEST(ParserTest, WriteHexRoundTrip) {
    L3ChannelRelease chRelease(RRCause::Normal_Event);
    std::string hex = writeL3Hex(chRelease);
    EXPECT_FALSE(hex.empty());

    auto msg = parseL3Hex(hex, ctx);
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->PD(), L3PD::RadioResource);
    EXPECT_EQ(msg->MTI(), L3RRMessage::ChannelRelease);
}

// GSM 04.08 10.2: PD=0x06(RR) in high nibble, skip=0, MTI=0xFF (unknown)
// Reference: GSM_RR_Types.ttcn RrMessageType -- 0xFF not defined
TEST(ParserTest, UnknownMTI) {
    uint8_t data[] = {0x60, 0xFF};
    auto msg = parseL3(std::span<const uint8_t>(data), ctx);
    EXPECT_FALSE(msg);
}
