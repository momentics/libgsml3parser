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

// Location Services (LS) message tests.
// Reference: 3GPP TS 44.031 / TS 24.027 / TS 24.028
// PD=0x0c (Location Services)

#include <gtest/gtest.h>
#include <gsml3parser/parser.h>
#include <gsml3parser/ls/l3lsmessages.h>
#include <gsml3parser/visitor.h>

using namespace gsml3parser;

static Expected<ParsedMessage> roundtrip(const ParsedMessage& msg) {
    auto hex = writeL3Hex(msg);
    if (!hex) return Expected<ParsedMessage>::error(hex.error());
    return parseL3Hex(hex.value());
}

// Location Service Request (TS 44.031 §9.1.2, MTI=0x01)
TEST(LSMessageTest, LocationServiceRequest_RoundTrip) {
    ParsedMessage msg(LSM(L3LocationServiceRequest{}));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messagePD(*parsed), L3PD::Location);
    EXPECT_EQ(messageMTI(*parsed), L3LocationServiceRequest::MTI);
    EXPECT_EQ(messageName(*parsed), "LocationServiceRequest");
}

TEST(LSMessageTest, LocationServiceRequest_Parse_Golden) {
    // PD=0x0c(Location), byte1=MTI=0x01, body=0xAA 0xBB
    // L3 header: PD(4)|TI(3)|TIF(1) = 1100 0000 = 0xC0, MTI byte = 0x01
    uint8_t data[] = {0xC0, 0x01, 0xAA, 0xBB};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(static_cast<int>(messagePD(*msg)), static_cast<int>(L3PD::Location));
    EXPECT_EQ(messageMTI(*msg), L3LocationServiceRequest::MTI);
    EXPECT_EQ(messageName(*msg), "LocationServiceRequest");
    auto* ls = tryGet<L3LocationServiceRequest>(*msg);
    ASSERT_TRUE(ls);
    EXPECT_EQ(ls->body().size(), 2u);
}

// Location Service Provider Message (TS 44.031 §9.1.3, MTI=0x02)
TEST(LSMessageTest, LocationServiceProviderMessage_RoundTrip) {
    ParsedMessage msg(LSM(L3LocationServiceProviderMessage{}));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messagePD(*parsed), L3PD::Location);
    EXPECT_EQ(messageMTI(*parsed), L3LocationServiceProviderMessage::MTI);
    EXPECT_EQ(messageName(*parsed), "LocationServiceProviderMessage");
}

TEST(LSMessageTest, LocationServiceProviderMessage_Parse_Golden) {
    // PD=0x0c(Location), byte1=MTI=0x02, body=0xCC 0xDD 0xEE
    uint8_t data[] = {0xC0, 0x02, 0xCC, 0xDD, 0xEE};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3LocationServiceProviderMessage::MTI);
    auto* lsp = tryGet<L3LocationServiceProviderMessage>(*msg);
    ASSERT_TRUE(lsp);
    EXPECT_EQ(lsp->body().size(), 3u);
}
