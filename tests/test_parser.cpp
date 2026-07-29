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

TEST(ParserTest, ParseNull) {
    EXPECT_FALSE(parseL3(nullptr, 0));
    EXPECT_FALSE(parseL3Hex(""));
}

TEST(ParserTest, ParseTooShort) {
    uint8_t data[] = {0x06};
    EXPECT_FALSE(parseL3(data, 1));
}

TEST(ParserTest, ParseRR_ChannelRelease) {
    uint8_t data[] = {0x06, 0x0D, 0x00};
    auto msg = parseL3(data, 3);
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->PD(), L3PD::RadioResource);
    EXPECT_EQ(msg->MTI(), L3RRMessage::ChannelRelease);

    auto* chRelease = dynamic_cast<L3ChannelRelease*>(msg.get());
    ASSERT_TRUE(chRelease);
}

TEST(ParserTest, ParseCC_Disconnect) {
    uint8_t data[] = {0x03, 0x25, 0x10, 0x00};
    auto msg = parseL3(data, 4);
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->PD(), L3PD::CallControl);
    EXPECT_EQ(msg->MTI(), L3CCMessage::Disconnect);
}

TEST(ParserTest, ParseMM_CMServiceAccept) {
    uint8_t data[] = {0x05, 0x21};
    auto msg = parseL3(data, 2);
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->PD(), L3PD::MobilityManagement);
    EXPECT_EQ(msg->MTI(), L3MMMessage::CMServiceAccept);
}

TEST(ParserTest, ParseSS_Facility) {
    uint8_t data[] = {0x0B, 0x3A, 0x00};
    auto msg = parseL3(data, 3);
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->PD(), L3PD::NonCallSS);
    EXPECT_EQ(msg->MTI(), L3SupServMessage::Facility);
}

TEST(ParserTest, ParseHex) {
    auto msg = parseL3Hex("0521");
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->PD(), L3PD::MobilityManagement);
    EXPECT_EQ(msg->MTI(), L3MMMessage::CMServiceAccept);
}

TEST(ParserTest, ParseHexWithSpaces) {
    auto msg = parseL3Hex("05 21");
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->PD(), L3PD::MobilityManagement);
}

TEST(ParserTest, ParseUnknownPD) {
    uint8_t data[] = {0x0F, 0x01};
    auto msg = parseL3(data, 2);
    EXPECT_FALSE(msg);
}

TEST(ParserTest, RegisterPDHandler) {
    bool handlerCalled = false;
    registerPDHandler(L3PD::SMS, [&](const L3Frame&) {
        handlerCalled = true;
        return std::make_unique<L3CMServiceAccept>();
    });

    uint8_t data[] = {0x09, 0x01};
    auto msg = parseL3(data, 2);
    EXPECT_TRUE(handlerCalled);
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->PD(), L3PD::MobilityManagement);

    unregisterPDHandler(L3PD::SMS);
}

TEST(ParserTest, WriteAndParseRoundTrip) {
    L3ChannelRelease chRelease(RRCause::Normal_Event);

    std::vector<uint8_t> buf(chRelease.fullLength());
    size_t n = writeL3(chRelease, buf.data(), buf.size());
    EXPECT_GT(n, 0u);

    auto msg = parseL3(buf.data(), n);
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->PD(), L3PD::RadioResource);
    EXPECT_EQ(msg->MTI(), L3RRMessage::ChannelRelease);
}

TEST(ParserTest, WriteHexRoundTrip) {
    L3ChannelRelease chRelease(RRCause::Normal_Event);
    std::string hex = writeL3Hex(chRelease);
    EXPECT_FALSE(hex.empty());

    auto msg = parseL3Hex(hex);
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->PD(), L3PD::RadioResource);
    EXPECT_EQ(msg->MTI(), L3RRMessage::ChannelRelease);
}

TEST(ParserTest, UnknownMTI) {
    uint8_t data[] = {0x06, 0xFF};
    auto msg = parseL3(data, 2);
    EXPECT_FALSE(msg);
}
