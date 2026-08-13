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
#include <gsml3parser/lapdm.h>
#include <gsml3parser/parser.h>

using namespace gsml3parser;
using namespace gsml3parser::lapdm;

// GSM 04.06 4.2.1: Address field encoding
TEST(LAPDMTest, MakeAddress_SAPI0_CR0) {
    uint8_t addr = makeAddress(SAPI::SAPI0, false);
    // SAPI0 = 0000, C/R=0, EA=1 -> 0000 0001 = 0x01
    EXPECT_EQ(addr, 0x01);
}

// GSM 04.06 4.2.1: Address field encoding
TEST(LAPDMTest, MakeAddress_SAPI3_CR1) {
    uint8_t addr = makeAddress(SAPI::SAPI3, true);
    // SAPI3 = 0011, C/R=1, EA=1 -> 0011 1001 = 0x39
    EXPECT_EQ(addr, 0x39);
}

// GSM 04.06: Wrap/unwrap L3 message in LAPDm frame
TEST(LAPDMTest, WrapAndUnwrapL3) {
    auto msg = parseL3Hex("600d00"); // Channel Release (RR, MTI=0x0D)
    ASSERT_TRUE(msg);
    auto bytes = writeL3Bytes(*msg);
    ASSERT_TRUE(bytes);

    auto frame = wrapL3(*bytes, SAPI::SAPI0, false);
    EXPECT_EQ(frame.size(), (*bytes).size() + 2);
    EXPECT_EQ(frame[0], 0x01); // SAPI0, CR=0, EA=1
    EXPECT_EQ(frame[1], 0x03); // UI

    auto payload = unwrapL3(frame);
    ASSERT_TRUE(payload);
    EXPECT_EQ(*payload, *bytes);
}

// Full BTS pipeline: build L3 -> serialize -> LAPDm wrap -> unwrap -> parse
TEST(LAPDMTest, FullPipeline_L3ToLAPDm) {
    ParsedMessage pm{RRM{L3ChannelRelease{RRCause::Normal_Event}}};
    auto l3Bytes = writeL3Bytes(pm);
    ASSERT_TRUE(l3Bytes);

    auto lapdmFrame = wrapL3(*l3Bytes, SAPI::SAPI0);
    EXPECT_EQ(lapdmFrame[0], 0x01); // address: SAPI0, CR=0, EA=1
    EXPECT_EQ(lapdmFrame[1], 0x03); // control: UI

    auto unwrapped = unwrapL3(lapdmFrame);
    ASSERT_TRUE(unwrapped);
    EXPECT_EQ(*unwrapped, *l3Bytes);
}

// LAPDm extract helpers
TEST(LAPDMTest, ExtractSAPIAndCR) {
    uint8_t addr = 0x39; // SAPI3, CR=1, EA=1
    EXPECT_EQ(extractSAPI(addr), SAPI::SAPI3);
    EXPECT_TRUE(extractCR(addr));

    addr = 0x01; // SAPI0, CR=0, EA=1
    EXPECT_EQ(extractSAPI(addr), SAPI::SAPI0);
    EXPECT_FALSE(extractCR(addr));
}

// Unwrap with too-short frame
TEST(LAPDMTest, UnwrapL3_TooShort) {
    uint8_t shortFrame[] = {0x01};
    auto result = unwrapL3(std::span<const uint8_t>(shortFrame));
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, ParseError::Code::TruncatedInput);
}

// isUIFrame checks
TEST(LAPDMTest, IsUIFrame) {
    uint8_t uiFrame[] = {0x01, 0x03, 0x60, 0x0D};
    EXPECT_TRUE(isUIFrame(std::span<const uint8_t>(uiFrame)));

    uint8_t nonUiFrame[] = {0x01, 0x2F, 0x60, 0x0D}; // SABME
    EXPECT_FALSE(isUIFrame(std::span<const uint8_t>(nonUiFrame)));
}
