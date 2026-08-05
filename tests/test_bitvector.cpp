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
#include <gsml3parser/arena.h>

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

// ── HPL API Tests (Phase 7, Step 7.2) ──────────────────────────────────

TEST(BitVectorTest, SpanConstructor) {
    std::vector<uint8_t> bytes = {0x12, 0x34, 0x56, 0x78};
    BitVector bv(std::span<const uint8_t>(bytes.data(), bytes.size()));
    EXPECT_EQ(bv.size(), 32u);
    EXPECT_EQ(bv.data()[0], 0x12);
    EXPECT_EQ(bv.data()[1], 0x34);
    EXPECT_EQ(bv.data()[2], 0x56);
    EXPECT_EQ(bv.data()[3], 0x78);

    // Modifying original vector does not affect BitVector (owning copy)
    bytes[0] = 0xFF;
    EXPECT_EQ(bv.data()[0], 0x12);
}

TEST(BitVectorTest, ArenaConstructor) {
    Arena arena(4096);
    size_t usedBefore = arena.used();

    BitVector bv(arena, 64);
    EXPECT_EQ(bv.size(), 64u);

    // Arena memory was consumed
    EXPECT_GT(arena.used(), usedBefore);

    // Write and read back
    size_t wp = 0;
    bv.writeField(wp, 0xDE, 8);
    bv.writeField(wp, 0xAD, 8);

    size_t rp = 0;
    EXPECT_EQ(bv.readField(rp, 8), 0xDE);
    EXPECT_EQ(bv.readField(rp, 8), 0xAD);
}

TEST(BitVectorTest, ArenaResetReclaimsMemory) {
    Arena arena(4096);

    BitVector bv1(arena, 128);
    size_t wp = 0;
    bv1.writeField(wp, 0xAB, 8);

    size_t usedAfterAlloc = arena.used();
    EXPECT_GT(usedAfterAlloc, 0u);

    arena.reset();
    EXPECT_EQ(arena.used(), 0u);

    // After reset, can allocate again
    BitVector bv2(arena, 64);
    wp = 0;
    bv2.writeField(wp, 0xCD, 8);

    size_t rp = 0;
    EXPECT_EQ(bv2.readField(rp, 8), 0xCD);
}

TEST(BitVectorTest, ArenaMemoryReuse) {
    Arena arena(4096);

    // First allocation cycle
    BitVector bv1(arena, 256);
    size_t used1 = arena.used();

    arena.reset();

    // Second cycle — memory should be reused
    BitVector bv2(arena, 256);
    size_t used2 = arena.used();

    EXPECT_EQ(used1, used2);
}

TEST(BitVectorTest, BitViewZeroCopy) {
    std::vector<uint8_t> externalBuffer = {0xAB, 0xCD, 0xEF, 0x01};
    BitView view(std::span<const uint8_t>(externalBuffer.data(), externalBuffer.size()));

    EXPECT_EQ(view.size(), 32u);
    EXPECT_FALSE(view.empty());

    size_t rp = 0;
    EXPECT_EQ(view.readField(rp, 8), 0xAB);
    EXPECT_EQ(view.readField(rp, 8), 0xCD);
    EXPECT_EQ(view.readField(rp, 8), 0xEF);
    EXPECT_EQ(view.readField(rp, 8), 0x01);
}

TEST(BitVectorTest, BitViewReadBit) {
    std::vector<uint8_t> externalBuffer = {0b10110011};
    BitView view(externalBuffer.data(), 8);

    size_t rp = 0;
    EXPECT_EQ(view.readBit(rp), 1u);
    EXPECT_EQ(view.readBit(rp), 0u);
    EXPECT_EQ(view.readBit(rp), 1u);
    EXPECT_EQ(view.readBit(rp), 1u);
    EXPECT_EQ(view.readBit(rp), 0u);
    EXPECT_EQ(view.readBit(rp), 0u);
    EXPECT_EQ(view.readBit(rp), 1u);
    EXPECT_EQ(view.readBit(rp), 1u);
}

TEST(BitVectorTest, BitViewPeekField) {
    std::vector<uint8_t> externalBuffer = {0x55, 0xAA};
    BitView view(externalBuffer.data(), 16);

    size_t rp = 0;
    EXPECT_EQ(view.peekField(rp, 8), 0x55);
    // peek does not advance position
    EXPECT_EQ(rp, 0u);
    EXPECT_EQ(view.peekField(rp, 8), 0x55);
    EXPECT_EQ(rp, 0u);

    // read advances position
    EXPECT_EQ(view.readField(rp, 8), 0x55);
    EXPECT_EQ(rp, 8u);
}

TEST(BitVectorTest, BitViewEmpty) {
    BitView view;
    EXPECT_EQ(view.size(), 0u);
    EXPECT_TRUE(view.empty());
}

TEST(BitVectorTest, BitVectorcreateView) {
    BitVector bv(32);
    size_t wp = 0;
    bv.writeField(wp, 0x12, 8);
    bv.writeField(wp, 0x34, 8);
    bv.writeField(wp, 0x56, 8);
    bv.writeField(wp, 0x78, 8);

    BitView view = bv.view();
    EXPECT_EQ(view.size(), 32u);

    size_t rp = 0;
    EXPECT_EQ(view.readField(rp, 8), 0x12);
    EXPECT_EQ(view.readField(rp, 8), 0x34);
    EXPECT_EQ(view.readField(rp, 8), 0x56);
    EXPECT_EQ(view.readField(rp, 8), 0x78);
}

TEST(BitVectorTest, BitViewDataPointer) {
    std::vector<uint8_t> externalBuffer = {0xDE, 0xAD};
    BitView view(externalBuffer.data(), 16);

    // data() returns the same pointer as the underlying buffer (zero-copy)
    EXPECT_EQ(view.data(), externalBuffer.data());
}

TEST(BitVectorTest, ArenaBitVectorSegment) {
    Arena arena(4096);
    BitVector bv(arena, 64);
    size_t wp = 0;
    bv.writeField(wp, 0xAB, 8);
    bv.writeField(wp, 0xCD, 8);
    bv.writeField(wp, 0xEF, 8);
    bv.writeField(wp, 0x01, 8);

    BitVector seg = bv.segment(8, 16);
    EXPECT_EQ(seg.size(), 16u);
    size_t rp = 0;
    EXPECT_EQ(seg.readField(rp, 8), 0xCD);
    EXPECT_EQ(seg.readField(rp, 8), 0xEF);
}

TEST(BitVectorTest, ArenaBitVectorClone) {
    Arena arena(4096);
    BitVector bv(arena, 32);
    size_t wp = 0;
    bv.writeField(wp, 0x12, 8);
    bv.writeField(wp, 0x34, 8);

    BitVector cloned = bv.clone();
    EXPECT_EQ(cloned.size(), 32u);
    size_t rp = 0;
    EXPECT_EQ(cloned.readField(rp, 8), 0x12);
    EXPECT_EQ(cloned.readField(rp, 8), 0x34);
}

TEST(BitVectorTest, ArenaBitVectorReset) {
    Arena arena(4096);
    BitVector bv(arena, 64);
    size_t wp = 0;
    bv.writeField(wp, 0xFF, 8);

    EXPECT_EQ(bv.size(), 64u);
    bv.reset();
    EXPECT_EQ(bv.size(), 0u);
}

TEST(BitVectorTest, ArenaCapacityGrowth) {
    Arena arena(128); // small initial capacity

    // Allocate more than initial capacity to test growth
    BitVector bv(arena, 1024);
    size_t wp = 0;
    bv.writeField(wp, 0xCA, 8);
    bv.writeField(wp, 0xFE, 8);

    size_t rp = 0;
    EXPECT_EQ(bv.readField(rp, 8), 0xCA);
    EXPECT_EQ(bv.readField(rp, 8), 0xFE);
}

TEST(BitVectorTest, MultipleArenaAllocations) {
    Arena arena(4096);

    std::vector<BitVector> vectors;
    for (int i = 0; i < 100; ++i) {
        BitVector bv(arena, 32);
        size_t wp = 0;
        bv.writeField(wp, static_cast<unsigned>(i & 0xFF), 8);
        vectors.push_back(std::move(bv));
    }

    // Verify all allocations are readable
    for (int i = 0; i < 100; ++i) {
        size_t rp = 0;
        EXPECT_EQ(vectors[i].readField(rp, 8), static_cast<unsigned>(i & 0xFF));
    }

    // Reset and verify reuse
    arena.reset();
    EXPECT_EQ(arena.used(), 0u);

    BitVector bv2(arena, 32);
    size_t wp = 0;
    bv2.writeField(wp, 0xBE, 8);
    size_t rp = 0;
    EXPECT_EQ(bv2.readField(rp, 8), 0xBE);
}

TEST(BitVectorTest, BitViewFromExternalArray) {
    uint8_t arr[] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};
    BitView view(arr, 64);

    size_t rp = 0;
    for (int i = 0; i < 8; ++i) {
        EXPECT_EQ(view.readField(rp, 8), arr[i]);
    }
}
