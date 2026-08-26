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

// Regression: misaligned field ending exactly at the last bit of the buffer
// must return the correct value (previously triggered a negative right-shift,
// i.e. undefined behavior, and returned 0x00 instead of 0xA5).
TEST(BitReaderTest, MisalignedFieldEndingAtBufferEnd) {
    // 12 bits total. After reading 4 bits, 8 bits remain starting at bit 4.
    uint8_t buf[] = {0xAA, 0x5C};
    BitReader br(buf, 12);
    auto r1 = br.readField(4);
    ASSERT_TRUE(r1.has_value());
    EXPECT_EQ(r1.value(), 0xAu);
    auto r2 = br.readField(8);
    ASSERT_TRUE(r2.has_value());
    EXPECT_EQ(r2.value(), 0xA5u);
    EXPECT_EQ(br.position(), 12u);
}

// Regression: peekField of a misaligned field ending at the buffer end.
TEST(BitReaderTest, MisalignedPeekEndingAtBufferEnd) {
    uint8_t buf[] = {0xAA, 0x5C};
    BitReader br(buf, 12);
    (void)br.readField(4);
    uint32_t p = br.peekField(8);
    EXPECT_EQ(p, 0xA5u);
    EXPECT_EQ(br.position(), 4u);
}

// Regression: 16-bit misaligned field ending exactly at the buffer end.
TEST(BitReaderTest, Misaligned16BitEndingAtBufferEnd) {
    // 24 bits total. After 4 bits, 20 bits remain; read 16 of them.
    uint8_t buf[] = {0x12, 0x34, 0x56};
    BitReader br(buf, 24);
    auto r1 = br.readField(4);
    ASSERT_TRUE(r1.has_value());
    EXPECT_EQ(r1.value(), 0x1u);
    auto r2 = br.readField(16);
    ASSERT_TRUE(r2.has_value());
    EXPECT_EQ(r2.value(), 0x2345u);
    EXPECT_EQ(br.position(), 20u);
}

// Regression: 32-bit misaligned field ending exactly at the buffer end.
TEST(BitReaderTest, Misaligned32BitEndingAtBufferEnd) {
    // 40 bits total. After 4 bits, 36 bits remain; read 32 of them.
    uint8_t buf[] = {0xAB, 0xCD, 0xEF, 0x01, 0x23};
    BitReader br(buf, 40);
    auto r1 = br.readField(4);
    ASSERT_TRUE(r1.has_value());
    EXPECT_EQ(r1.value(), 0xAu);
    auto r2 = br.readField(32);
    ASSERT_TRUE(r2.has_value());
    EXPECT_EQ(r2.value(), 0xBCDEF012u);
    EXPECT_EQ(br.position(), 36u);
}

// Regression: misaligned readBytes ending exactly at the buffer end.
TEST(BitReaderTest, MisalignedReadBytesEndingAtBufferEnd) {
    uint8_t buf[] = {0xAA, 0x5C};
    BitReader br(buf, 12);
    (void)br.readField(4);
    uint8_t out = 0;
    auto res = br.readBytes(&out, 1);
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ(out, 0xA5u);
    EXPECT_EQ(br.position(), 12u);
}

// peekField past the end must return 0 without undefined behavior.
TEST(BitReaderTest, PeekPastEndReturnsZero) {
    uint8_t buf[] = {0xFF};
    BitReader br(buf, 8);
    (void)br.readField(8);
    EXPECT_EQ(br.peekField(8), 0u);
    EXPECT_EQ(br.peekField(1), 0u);
}

// Regression: peekField with nbits > 32 clamps to 32 (returns top 32 bits), no UB.
TEST(BitReaderTest, PeekFieldOver32Bits_ClampsTo32) {
    uint8_t buf[] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0};
    BitReader br(buf, 64);
    EXPECT_EQ(br.peekField(40), 0x12345678u) << "peekField(40) clamps to 32 -> top 32 bits";
    EXPECT_EQ(br.position(), 0u) << "peek must not advance";
    BitReader br2(buf, 64);
    (void)br2.readField(4);
    EXPECT_EQ(br2.peekField(40), 0x23456789u) << "misaligned peekField(40) clamps to 32";
    EXPECT_EQ(br2.position(), 4u);
}

// Regression: peekField on an empty/null buffer must return 0 WITHOUT undefined
// behavior (previously `actual == 0` produced a negative right-shift).
TEST(BitReaderTest, PeekFieldEmptyBuffer_ReturnsZeroNoUB) {
    uint8_t buf[] = {0x00};
    BitReader brEmpty(buf, 0);            // zero total bits
    EXPECT_EQ(brEmpty.peekField(8), 0u);
    EXPECT_EQ(brEmpty.peekField(40), 0u);
    BitReader brNull(nullptr, 0);         // null buffer, zero bits
    EXPECT_EQ(brNull.peekField(8), 0u);
    EXPECT_EQ(brNull.peekField(32), 0u);
}

// Regression: a null buffer with a non-zero declared bit count must still return 0
// WITHOUT undefined behavior. This exercises the `actual == 0` guard in the
// aligned branch (loadN yields zero bytes, so the right-shift count would
// otherwise be negative).
TEST(BitReaderTest, PeekFieldNullBufferWithBits_ReturnsZeroNoUB) {
    BitReader br(nullptr, 16);            // null buffer, 16 declared bits
    EXPECT_EQ(br.peekField(8), 0u);
    EXPECT_EQ(br.peekField(16), 0u);
    EXPECT_EQ(br.peekField(40), 0u);
}

// Test: a BitReader over a null buffer with a non-zero bit count must return
// errors (not undefined behavior) from readField()/readBytes(), and 0 from
// peekField(). Symmetric with the existing peekField null-buffer guard.
TEST(BitReaderTest, NullBuffer_ReadField_ReturnsError) {
    BitReader r(nullptr, 64);
    auto res = r.readField(8);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code, ParseError::Code::TruncatedInput);
}

TEST(BitReaderTest, NullBuffer_ReadBytes_ReturnsError) {
    BitReader r(nullptr, 64);
    uint8_t out[4];
    auto res = r.readBytes(out, 4);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code, ParseError::Code::TruncatedInput);
}

TEST(BitReaderTest, NullBuffer_PeekField_ReturnsZero) {
    BitReader r(nullptr, 64);
    EXPECT_EQ(r.peekField(8), 0u);
}
