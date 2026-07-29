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
#include <gsml3parser/bitvector.h>

using namespace gsml3parser;

TEST(BitVectorTest, DefaultConstructor) {
    BitVector bv;
    EXPECT_EQ(bv.size(), 0u);
    EXPECT_TRUE(bv.empty());
}

TEST(BitVectorTest, SizeConstructor) {
    BitVector bv(32);
    EXPECT_EQ(bv.size(), 32u);
    EXPECT_FALSE(bv.empty());
}

TEST(BitVectorTest, FillConstructor) {
    BitVector bv(16, 0xFF);
    EXPECT_EQ(bv.size(), 16u);
    EXPECT_EQ(bv.data()[0], 0xFF);
    EXPECT_EQ(bv.data()[1], 0xFF);
}

TEST(BitVectorTest, VectorConstructor) {
    std::vector<uint8_t> bytes = {0x12, 0x34, 0x56};
    BitVector bv(bytes);
    EXPECT_EQ(bv.size(), 24u);
    EXPECT_EQ(bv.data()[0], 0x12);
    EXPECT_EQ(bv.data()[1], 0x34);
    EXPECT_EQ(bv.data()[2], 0x56);
}

TEST(BitVectorTest, ReadWriteField) {
    BitVector bv(32);
    size_t wp = 0;
    bv.writeField(wp, 0x06, 4);
    bv.writeField(wp, 0x19, 8);
    bv.writeField(wp, 0xAB, 8);
    bv.writeField(wp, 0xCD, 8);

    size_t rp = 0;
    EXPECT_EQ(bv.readField(rp, 4), 0x06);
    EXPECT_EQ(bv.readField(rp, 8), 0x19);
    EXPECT_EQ(bv.readField(rp, 8), 0xAB);
    EXPECT_EQ(bv.readField(rp, 8), 0xCD);
}

TEST(BitVectorTest, PeekField) {
    BitVector bv(16, 0);
    size_t wp = 0;
    bv.writeField(wp, 0x55, 8);

    size_t rp = 0;
    EXPECT_EQ(bv.peekField(rp, 8), 0x55);
    EXPECT_EQ(rp, 0u);
}

TEST(BitVectorTest, ReadWriteBit) {
    BitVector bv(8);
    size_t wp = 0;
    bv.writeBit(wp, true);
    bv.writeBit(wp, false);
    bv.writeBit(wp, true);
    bv.writeBit(wp, true);
    bv.writeBit(wp, false);
    bv.writeBit(wp, true);
    bv.writeBit(wp, false);
    bv.writeBit(wp, true);

    size_t rp = 0;
    EXPECT_EQ(bv.readBit(rp), 1u);
    EXPECT_EQ(bv.readBit(rp), 0u);
    EXPECT_EQ(bv.readBit(rp), 1u);
    EXPECT_EQ(bv.readBit(rp), 1u);
    EXPECT_EQ(bv.readBit(rp), 0u);
    EXPECT_EQ(bv.readBit(rp), 1u);
    EXPECT_EQ(bv.readBit(rp), 0u);
    EXPECT_EQ(bv.readBit(rp), 1u);
}

TEST(BitVectorTest, Segment) {
    BitVector bv(16);
    size_t wp = 0;
    bv.writeField(wp, 0xAB, 8);
    bv.writeField(wp, 0xCD, 8);

    BitVector seg = bv.segment(0, 8);
    EXPECT_EQ(seg.size(), 8u);
    EXPECT_EQ(seg.data()[0], 0xAB);

    seg = bv.segment(8, 8);
    EXPECT_EQ(seg.data()[0], 0xCD);
}

TEST(BitVectorTest, Clone) {
    BitVector orig(16);
    size_t wp = 0;
    orig.writeField(wp, 0xFF, 8);
    orig.writeField(wp, 0x00, 8);

    BitVector cloned = orig.clone();
    EXPECT_EQ(cloned.size(), orig.size());
    EXPECT_EQ(cloned.data()[0], 0xFF);
    EXPECT_EQ(cloned.data()[1], 0x00);
}

TEST(BitVectorTest, Resize) {
    BitVector bv(8);
    size_t wp = 0;
    bv.writeField(wp, 0xAB, 8);

    bv.resize(32);
    EXPECT_EQ(bv.size(), 32u);
    EXPECT_EQ(bv.data()[0], 0xAB);
    EXPECT_EQ(bv.data()[1], 0x00);
    EXPECT_EQ(bv.data()[2], 0x00);
    EXPECT_EQ(bv.data()[3], 0x00);
}

TEST(BitVectorTest, Equality) {
    BitVector a(16);
    size_t wp = 0;
    a.writeField(wp, 0x12, 8);
    a.writeField(wp, 0x34, 8);

    BitVector b(16);
    wp = 0;
    b.writeField(wp, 0x12, 8);
    b.writeField(wp, 0x34, 8);

    EXPECT_EQ(a, b);
}

TEST(BitVectorTest, Inequality) {
    BitVector a(16);
    size_t wp = 0;
    a.writeField(wp, 0x12, 8);
    a.writeField(wp, 0x34, 8);

    BitVector c(16);
    wp = 0;
    c.writeField(wp, 0x12, 8);
    c.writeField(wp, 0x56, 8);

    EXPECT_NE(a, c);
}

TEST(BitVectorTest, CopyConstructor) {
    BitVector orig(16);
    size_t wp = 0;
    orig.writeField(wp, 0xAB, 8);
    orig.writeField(wp, 0xCD, 8);

    BitVector copy(orig);
    EXPECT_EQ(copy.size(), orig.size());
    EXPECT_EQ(copy.data()[0], 0xAB);
    EXPECT_EQ(copy.data()[1], 0xCD);
}

TEST(BitVectorTest, MoveConstructor) {
    BitVector orig(16);
    size_t wp = 0;
    orig.writeField(wp, 0xAB, 8);
    orig.writeField(wp, 0xCD, 8);

    BitVector moved(std::move(orig));
    EXPECT_EQ(moved.size(), 16u);
    EXPECT_EQ(moved.data()[0], 0xAB);
    EXPECT_EQ(moved.data()[1], 0xCD);
    EXPECT_EQ(orig.size(), 0u);
}
