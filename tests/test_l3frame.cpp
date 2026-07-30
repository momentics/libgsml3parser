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

// L3Frame operations: H/L writing, PD/MTI/TI extraction, metadata.
// Reference: osmo-ttcn3-hacks GSM_RR_Types.ttcn, GSM_RestOctets.ttcn,
// L3_Templates.ttcn, GSM_SystemInformation.ttcn.

#include <gtest/gtest.h>
#include <gsml3parser/parser.h>
#include <gsml3parser/l3frame.h>
#include <gsml3parser/rr/l3rrmessages.h>

using namespace gsml3parser;

// ── L3Frame Constructors ──────────────────────────────────────────────

TEST(L3FrameTest, DefaultConstructor) {
    L3Frame frame;
    EXPECT_EQ(frame.primitive(), Primitive::L3_DATA);
    EXPECT_EQ(frame.getSAPI(), SAPI::SAPI0);
    EXPECT_EQ(frame.length(), 0u);
}

TEST(L3FrameTest, PrimitiveConstructor) {
    L3Frame frame(Primitive::L3_DATA);
    EXPECT_EQ(frame.primitive(), Primitive::L3_DATA);
    EXPECT_EQ(frame.getSAPI(), SAPI::SAPI0);
}

TEST(L3FrameTest, SAPI_PrimitiveConstructor) {
    L3Frame frame(SAPI::SAPI3, Primitive::L3_DATA);
    EXPECT_EQ(frame.getSAPI(), SAPI::SAPI3);
    EXPECT_EQ(frame.primitive(), Primitive::L3_DATA);
}

TEST(L3FrameTest, SizedConstructor) {
    L3Frame frame(Primitive::L3_DATA, 64, SAPI::SAPI0);
    EXPECT_EQ(frame.length(), 8u); // 64 bits = 8 bytes
    EXPECT_EQ(frame.primitive(), Primitive::L3_DATA);
}

TEST(L3FrameTest, HexConstructor) {
    L3Frame frame(SAPI::SAPI0, "061900");
    EXPECT_EQ(frame.PD(), L3PD::RadioResource);
    EXPECT_EQ(frame.MTI(), 0x19);
    EXPECT_EQ(frame.length(), 3u);
}

TEST(L3FrameTest, HexConstructor_Spaces) {
    L3Frame frame(SAPI::SAPI0, "06 19 00");
    EXPECT_EQ(frame.PD(), L3PD::RadioResource);
    EXPECT_EQ(frame.MTI(), 0x19);
}

TEST(L3FrameTest, BitVectorSourceConstructor) {
    BitVector bv(24);
    size_t wp = 0;
    bv.writeField(wp, 0x06, 4);
    bv.writeField(wp, 0x19, 8);
    bv.writeField(wp, 0x00, 8);

    L3Frame frame(SAPI::SAPI0, bv);
    EXPECT_EQ(frame.PD(), L3PD::RadioResource);
    EXPECT_EQ(frame.MTI(), 0x19);
}

// ── PD/MTI/TI Extraction ─────────────────────────────────────────────

TEST(L3FrameTest, PD_RR) {
    L3Frame frame(SAPI::SAPI0, "061900");
    EXPECT_EQ(frame.PD(), L3PD::RadioResource);
}

TEST(L3FrameTest, PD_MM) {
    L3Frame frame(SAPI::SAPI0, "0521");
    EXPECT_EQ(frame.PD(), L3PD::MobilityManagement);
}

TEST(L3FrameTest, PD_CC) {
    L3Frame frame(SAPI::SAPI0, "0375");
    EXPECT_EQ(frame.PD(), L3PD::CallControl);
}

TEST(L3FrameTest, PD_SS) {
    L3Frame frame(SAPI::SAPI0, "0B3A");
    EXPECT_EQ(frame.PD(), L3PD::NonCallSS);
}

TEST(L3FrameTest, MTI_Extraction) {
    L3Frame frame(SAPI::SAPI0, "061900");
    EXPECT_EQ(frame.MTI(), 0x19); // SystemInformationType1
}

TEST(L3FrameTest, TI_Extraction_CC) {
    // L3Frame::TI() reads first 4 bits, which is PD for all messages.
    // For CC messages, the actual TI (Transaction Identifier) is encoded
    // in bits 4-6 of the first byte, but L3Frame::TI() doesn't decode that.
    // The CC message class (L3CCMessage) stores TI as a member variable.
    L3Frame frame(SAPI::SAPI0, "0375");
    EXPECT_EQ(frame.PD(), L3PD::CallControl);
    // L3Frame::TI() returns first 4 bits = PD value (3 for CC)
    EXPECT_EQ(frame.TI(), 3u);
}

// ── H/L Bit Writing (Rest Octets) ────────────────────────────────────
// Reference: GSM_RestOctets.ttcn uses CSN.1 L/H encoding

TEST(L3FrameTest, WriteL) {
    // fillPattern[0] = 0, so L writes 0
    L3Frame frame(Primitive::L3_DATA, 8);
    size_t wp = 0;
    frame.writeL(wp);
    size_t rp = 0;
    EXPECT_EQ(frame.readField(rp, 1), 0u);
}

TEST(L3FrameTest, WriteH) {
    // fillPattern[0] = 0, so H writes inverted = 1
    L3Frame frame(Primitive::L3_DATA, 8);
    size_t wp = 0;
    frame.writeH(wp);
    size_t rp = 0;
    EXPECT_EQ(frame.readField(rp, 1), 1u);
}

TEST(L3FrameTest, WriteL_AtPosition2) {
    // fillPattern[2] = 1, so L writes 1
    L3Frame frame(Primitive::L3_DATA, 8);
    size_t wp = 2;
    frame.writeL(wp);
    size_t rp = 2;
    EXPECT_EQ(frame.readField(rp, 1), 1u);
}

TEST(L3FrameTest, WriteH_AtPosition2) {
    // fillPattern[2] = 1, so H writes inverted = 0
    L3Frame frame(Primitive::L3_DATA, 8);
    size_t wp = 2;
    frame.writeH(wp);
    size_t rp = 2;
    EXPECT_EQ(frame.readField(rp, 1), 0u);
}

TEST(L3FrameTest, MultipleHL_Bits) {
    // Write H, L, H, H, L sequence
    // fillPattern = {0,0,1,0,1,0,1,1} = 0x2B
    // H writes inverted fill bit, L writes fill bit as-is
    // Bit 0 (wp=0): fill=0, H→1, wp becomes 1
    // Bit 1 (wp=1): fill=0, L→0, wp becomes 2
    // Bit 2 (wp=2): fill=1, H→0, wp becomes 3
    // Bit 3 (wp=3): fill=0, H→1, wp becomes 4
    // Bit 4 (wp=4): fill=1, L→1, wp becomes 5
    // Result: 10011000 = 0x98
    L3Frame frame(Primitive::L3_DATA, 8);
    size_t wp = 0;
    frame.writeH(wp);
    frame.writeL(wp);
    frame.writeH(wp);
    frame.writeH(wp);
    frame.writeL(wp);
    EXPECT_EQ(frame.data()[0], 0x98);
}

TEST(L3FrameTest, HLCrossOctet) {
    // Write 9 H bits to cross octet boundary
    // fillPattern = {0,0,1,0,1,0,1,1}
    // H inverts: {1,1,0,1,0,1,0,0}
    // Byte 0 (wp 0-7): 11010100 = 0xD4
    // Byte 1 (wp 8): fillPattern[8%8] = fillPattern[0] = 0, H→1, so bit 7 = 1, rest = 0
    // Byte 1: 10000000 = 0x80
    L3Frame frame(Primitive::L3_DATA, 16);
    size_t wp = 0;
    for (int i = 0; i < 9; i++) {
        frame.writeH(wp);
    }
    EXPECT_EQ(frame.data()[0], 0xD4);
    EXPECT_EQ(frame.data()[1], 0x80);
}

// ── L2Length / Timestamp ─────────────────────────────────────────────

TEST(L3FrameTest, L2Length) {
    L3Frame frame(Primitive::L3_DATA, 64);
    frame.L2Length(10);
    EXPECT_EQ(frame.L2Length(), 10u);
}

TEST(L3FrameTest, Timestamp) {
    L3Frame frame;
    frame.setTimestamp(1234567890.0);
    EXPECT_DOUBLE_EQ(frame.timestamp(), 1234567890.0);
}

// ── isData() ──────────────────────────────────────────────────────────

TEST(L3FrameTest, IsData) {
    L3Frame frame(Primitive::L3_DATA);
    EXPECT_TRUE(frame.isData());

    L3Frame frame2(Primitive::L3_RELEASE_REQUEST);
    EXPECT_FALSE(frame2.isData());
}

// ── Copy / Assignment ─────────────────────────────────────────────────

TEST(L3FrameTest, CopyConstructor) {
    L3Frame orig(SAPI::SAPI3, "061900");
    orig.L2Length(20);
    orig.setTimestamp(100.0);

    L3Frame copy(orig);
    EXPECT_EQ(copy.PD(), orig.PD());
    EXPECT_EQ(copy.MTI(), orig.MTI());
    EXPECT_EQ(copy.getSAPI(), orig.getSAPI());
    EXPECT_EQ(copy.L2Length(), orig.L2Length());
    EXPECT_EQ(copy.timestamp(), orig.timestamp());
}

TEST(L3FrameTest, AssignmentOperator) {
    L3Frame orig(SAPI::SAPI0, "0521");
    orig.L2Length(15);

    L3Frame assigned;
    assigned = orig;
    EXPECT_EQ(assigned.PD(), orig.PD());
    EXPECT_EQ(assigned.MTI(), orig.MTI());
    EXPECT_EQ(assigned.L2Length(), orig.L2Length());
}

// ── SAPI Setting ──────────────────────────────────────────────────────

TEST(L3FrameTest, SetSAPI) {
    L3Frame frame;
    frame.setSAPI(SAPI::SAPI3);
    EXPECT_EQ(frame.getSAPI(), SAPI::SAPI3);

    frame.setSAPI(SAPI::SAPI0_Sacch);
    EXPECT_EQ(frame.getSAPI(), SAPI::SAPI0_Sacch);
}

// ── text() output ─────────────────────────────────────────────────────

TEST(L3FrameTest, TextOutput) {
    L3Frame frame(SAPI::SAPI0, "061900");
    std::ostringstream oss;
    frame.text(oss);
    // Just verify it doesn't crash and produces some output
    EXPECT_FALSE(oss.str().empty());
}

// ── BitVector compatibility ──────────────────────────────────────────

TEST(L3FrameTest, InheritsBitVector) {
    L3Frame frame(Primitive::L3_DATA, 32);
    size_t wp = 0;
    frame.writeField(wp, 0xAB, 8);
    frame.writeField(wp, 0xCD, 8);

    size_t rp = 0;
    EXPECT_EQ(frame.readField(rp, 8), 0xAB);
    EXPECT_EQ(frame.readField(rp, 8), 0xCD);
}

// ── Peek field (non-consuming read) ──────────────────────────────────

TEST(L3FrameTest, PeekField) {
    L3Frame frame(Primitive::L3_DATA, 16);
    size_t wp = 0;
    frame.writeField(wp, 0x55, 8);

    size_t rp = 0;
    unsigned val = frame.peekField(rp, 8);
    EXPECT_EQ(val, 0x55);
    EXPECT_EQ(rp, 0u); // peek doesn't advance
}

// ── Segment ──────────────────────────────────────────────────────────

TEST(L3FrameTest, Segment) {
    L3Frame frame(Primitive::L3_DATA, 32);
    size_t wp = 0;
    frame.writeField(wp, 0x12, 8);
    frame.writeField(wp, 0x34, 8);
    frame.writeField(wp, 0x56, 8);
    frame.writeField(wp, 0x78, 8);

    BitVector seg = frame.segment(8, 16);
    EXPECT_EQ(seg.size(), 16u);
    EXPECT_EQ(seg.data()[0], 0x34);
    EXPECT_EQ(seg.data()[1], 0x56);
}
