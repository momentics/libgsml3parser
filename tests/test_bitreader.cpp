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
#include "gsml3parser/bitreader.h"
#include "gsml3parser/expected.h"

using namespace gsml3parser;

TEST(BitReaderTest, Read1Bit) {
    uint8_t buf[] = {0x80};
    BitReader br(buf, 8);
    auto res = br.readField(1);
    EXPECT_TRUE(res.has_value());
    EXPECT_EQ(res.value(), 1u);
}

TEST(BitReaderTest, Read4Bits) {
    uint8_t buf[] = {0xAB};
    BitReader br(buf, 8);
    auto res = br.readField(4);
    EXPECT_TRUE(res.has_value());
    EXPECT_EQ(res.value(), 0xAu);
}

TEST(BitReaderTest, Read8Bits) {
    uint8_t buf[] = {0xFF};
    BitReader br(buf, 8);
    auto res = br.readField(8);
    EXPECT_TRUE(res.has_value());
    EXPECT_EQ(res.value(), 0xFFu);
}

TEST(BitReaderTest, Read16Bits) {
    uint8_t buf[] = {0x12, 0x34};
    BitReader br(buf, 16);
    auto res = br.readField(16);
    EXPECT_TRUE(res.has_value());
    EXPECT_EQ(res.value(), 0x1234u);
}

TEST(BitReaderTest, Read32Bits) {
    uint8_t buf[] = {0xDE, 0xAD, 0xBE, 0xEF};
    BitReader br(buf, 32);
    auto res = br.readField(32);
    EXPECT_TRUE(res.has_value());
    EXPECT_EQ(res.value(), 0xDEADBEEFu);
}

TEST(BitReaderTest, CrossByteBoundary) {
    uint8_t buf[] = {0xC0, 0x00};
    BitReader br(buf, 16);
    auto r1 = br.readField(3);
    EXPECT_TRUE(r1.has_value());
    EXPECT_EQ(r1.value(), 0x6u);
    auto r2 = br.readField(3);
    EXPECT_TRUE(r2.has_value());
    EXPECT_EQ(r2.value(), 0x0u);
}

TEST(BitReaderTest, OutOfBoundsError) {
    uint8_t buf[] = {0xFF};
    BitReader br(buf, 8);
    auto res = br.readField(16);
    EXPECT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code, ParseError::Code::TruncatedInput);
}

TEST(BitReaderTest, PeekDoesNotAdvance) {
    uint8_t buf[] = {0xF0};
    BitReader br(buf, 8);
    uint32_t p1 = br.peekField(4);
    EXPECT_EQ(p1, 0xFu);
    EXPECT_EQ(br.position(), 0u);
    uint32_t p2 = br.peekField(4);
    EXPECT_EQ(p2, 0xFu);
    EXPECT_EQ(br.position(), 0u);
}

TEST(BitReaderTest, AlignToOctet) {
    uint8_t buf[] = {0xFF, 0xFF};
    BitReader br(buf, 16);
    (void)br.readField(3);
    EXPECT_EQ(br.position(), 3u);
    br.alignToOctet();
    EXPECT_EQ(br.position(), 8u);
}

TEST(BitReaderTest, ReadBytesAligned) {
    uint8_t buf[] = {0x11, 0x22, 0x33, 0x44};
    BitReader br(buf, 32);
    std::vector<uint8_t> out(2);
    auto res = br.readBytes(out.data(), 2);
    EXPECT_TRUE(res.has_value());
    EXPECT_EQ(out[0], 0x11u);
    EXPECT_EQ(out[1], 0x22u);
    EXPECT_EQ(br.position(), 16u);
}

TEST(BitReaderTest, HasMore) {
    uint8_t buf[] = {0xFF, 0xFF};
    BitReader br(buf, 16);
    EXPECT_TRUE(br.hasMore());
    (void)br.readField(8);
    EXPECT_TRUE(br.hasMore());
    (void)br.readField(8);
    EXPECT_FALSE(br.hasMore());
}

TEST(BitReaderTest, RemainingBits) {
    uint8_t buf[] = {0xFF, 0xFF, 0xFF};
    BitReader br(buf, 24);
    EXPECT_EQ(br.remainingBits(), 24u);
    (void)br.readField(10);
    EXPECT_EQ(br.remainingBits(), 14u);
}

TEST(BitReaderTest, RoundTripWriteRead) {
    uint8_t buf[] = {0x12, 0x34, 0xAB, 0xCD};

    BitReader br(buf, 32);
    auto r1 = br.readField(8);
    EXPECT_TRUE(r1.has_value());
    EXPECT_EQ(r1.value(), 0x12u);
    auto r2 = br.readField(8);
    EXPECT_TRUE(r2.has_value());
    EXPECT_EQ(r2.value(), 0x34u);
    auto r3 = br.readField(8);
    EXPECT_TRUE(r3.has_value());
    EXPECT_EQ(r3.value(), 0xABu);
    auto r4 = br.readField(8);
    EXPECT_TRUE(r4.has_value());
    EXPECT_EQ(r4.value(), 0xCDu);
}

TEST(BitReaderTest, EmptyBuffer) {
    uint8_t buf[] = {0x00};
    BitReader br(buf, 0);
    EXPECT_FALSE(br.hasMore());
    EXPECT_EQ(br.remainingBits(), 0u);
    auto res = br.readField(1);
    EXPECT_FALSE(res.has_value());
}
