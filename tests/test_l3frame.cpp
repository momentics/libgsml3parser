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
    EXPECT_EQ(frame.sapi(), SAPI::SAPI0);
    EXPECT_EQ(frame.length(), 0u);
}

TEST(L3FrameTest, PrimitiveConstructor) {
    L3Frame frame(Primitive::L3_DATA);
    EXPECT_EQ(frame.primitive(), Primitive::L3_DATA);
    EXPECT_EQ(frame.sapi(), SAPI::SAPI0);
}

TEST(L3FrameTest, SAPI_PrimitiveConstructor) {
    L3Frame frame(SAPI::SAPI3, Primitive::L3_DATA);
    EXPECT_EQ(frame.sapi(), SAPI::SAPI3);
    EXPECT_EQ(frame.primitive(), Primitive::L3_DATA);
}

TEST(L3FrameTest, SizedConstructor) {
    L3Frame frame(Primitive::L3_DATA, 64, SAPI::SAPI0);
    EXPECT_EQ(frame.length(), 8u); // 64 bits = 8 bytes
    EXPECT_EQ(frame.primitive(), Primitive::L3_DATA);
}

// GSM 04.08 10.2: PD=0x06(RR) in high nibble, skip=0, MTI=0x19(SystemInformationType1)
// Reference: GSM_RR_Types.ttcn SYSTEM_INFORMATION_TYPE_1 = '00011001'B = 0x19
TEST(L3FrameTest, HexConstructor) {
    L3Frame frame(SAPI::SAPI0, "601900");
    EXPECT_EQ(frame.pd(), L3PD::RadioResource);
    EXPECT_EQ(frame.mti(), 0x19);
    EXPECT_EQ(frame.length(), 3u);
}

// GSM 04.08 10.2: PD=0x06(RR) high nibble, skip=0, MTI=0x19(SystemInformationType1)
// Reference: GSM_RR_Types.ttcn SYSTEM_INFORMATION_TYPE_1 = 0x19
TEST(L3FrameTest, HexConstructor_Spaces) {
    L3Frame frame(SAPI::SAPI0, "60 19 00");
    EXPECT_EQ(frame.pd(), L3PD::RadioResource);
    EXPECT_EQ(frame.mti(), 0x19);
}

// GSM 04.08 10.2: PD(4 bits) at high nibble, skip(4 bits) at low nibble, MTI(8 bits), body
// Reference: GSM_RR_Types.ttcn RrHeader (skip_indicator + rr_protocol_discriminator + message_type)
TEST(L3FrameTest, BitVectorSourceConstructor) {
    BitVector bv(24);
    size_t wp = 0;
    bv.writeField(wp, 0x06, 4);  // PD in high nibble
    bv.writeField(wp, 0x00, 4);  // skip in low nibble
    bv.writeField(wp, 0x19, 8);  // MTI
    bv.writeField(wp, 0x00, 8);  // body

    L3Frame frame(SAPI::SAPI0, bv);
    EXPECT_EQ(frame.pd(), L3PD::RadioResource);
    EXPECT_EQ(frame.mti(), 0x19);
}

// ── PD/MTI/TI Extraction ─────────────────────────────────────────────

// GSM 04.08 10.2: PD=0x06(RadioResource) in high nibble, skip=0 in low nibble → byte 0 = 0x60
// Reference: GSMCommon.h L3RadioResourcePD=0x06
TEST(L3FrameTest, PD_RR) {
    L3Frame frame(SAPI::SAPI0, "601900");
    EXPECT_EQ(frame.pd(), L3PD::RadioResource);
}

// GSM 04.08 10.2: PD=0x05(MobilityManagement) in high nibble, skip=0 → byte 0 = 0x50
// Byte 1: messageType(6)<<2|NSD(2) = 0x21<<2 = 0x84 (CMServiceAccept)
// Reference: GSMCommon.h L3MobilityManagementPD=0x05
TEST(L3FrameTest, PD_MM) {
    L3Frame frame(SAPI::SAPI0, "5084");
    EXPECT_EQ(frame.pd(), L3PD::MobilityManagement);
}

// GSM 04.08 10.2: PD=0x03(CallControl) in high nibble, TIO=0,TIF=0 in low nibble → byte 0 = 0x30
// Reference: GSMCommon.h L3CallControlPD=0x03
TEST(L3FrameTest, PD_CC) {
    L3Frame frame(SAPI::SAPI0, "3000");
    EXPECT_EQ(frame.pd(), L3PD::CallControl);
}

// GSM 04.08 10.2: PD=0x0B(NonCallSS) in high nibble, TIO=0,TIF=0 in low nibble → byte 0 = 0xB0
// Reference: GSMCommon.h L3NonCallSSPD=0x0B
TEST(L3FrameTest, PD_SS) {
    L3Frame frame(SAPI::SAPI0, "B000");
    EXPECT_EQ(frame.pd(), L3PD::NonCallSS);
}

// GSM 04.08 10.2: PD=0x06(RR) high nibble, skip=0, MTI=0x19(SystemInformationType1)
// Reference: GSM_RR_Types.ttcn SYSTEM_INFORMATION_TYPE_1 = '00011001'B
TEST(L3FrameTest, MTI_Extraction) {
    L3Frame frame(SAPI::SAPI0, "601900");
    EXPECT_EQ(frame.mti(), 0x19);
}

// GSM 04.08 10.2: CC L3 header Byte 0 = PD(4,high) | TIO(3)+TIF(1,low)
// For PD=0x03(CallControl), TIO=0, TIF=0: byte 0 = 0x30
// Reference: GSMCommon.h L3CallControlPD=0x03, L3_Templates.ttcn c_TIF_ORIG
TEST(L3FrameTest, TI_Extraction_CC) {
    L3Frame frame(SAPI::SAPI0, "3000");
    EXPECT_EQ(frame.pd(), L3PD::CallControl);
    EXPECT_EQ(frame.ti(), 0u);
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
    frame.l2Length(10);
    EXPECT_EQ(frame.l2Length(), 10u);
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
    // Reference format: PD=0x06(RR) high nibble, skip=0, MTI=0x19(SI1)
    L3Frame orig(SAPI::SAPI3, "601900");
    orig.l2Length(20);
    orig.setTimestamp(100.0);

    L3Frame copy(orig);
    EXPECT_EQ(copy.pd(), orig.pd());
    EXPECT_EQ(copy.mti(), orig.mti());
    EXPECT_EQ(copy.sapi(), orig.sapi());
    EXPECT_EQ(copy.l2Length(), orig.l2Length());
    EXPECT_EQ(copy.timestamp(), orig.timestamp());
}

TEST(L3FrameTest, AssignmentOperator) {
    // Reference format: PD=0x05(MM) high nibble, skip=0, MTI=0x21(CMServiceAccept)
    L3Frame orig(SAPI::SAPI0, "5021");
    orig.l2Length(15);

    L3Frame assigned;
    assigned = orig;
    EXPECT_EQ(assigned.pd(), orig.pd());
    EXPECT_EQ(assigned.mti(), orig.mti());
    EXPECT_EQ(assigned.l2Length(), orig.l2Length());
}

// ── SAPI Setting ──────────────────────────────────────────────────────

TEST(L3FrameTest, SetSAPI) {
    L3Frame frame;
    frame.sapi(SAPI::SAPI3);
    EXPECT_EQ(frame.sapi(), SAPI::SAPI3);

    frame.sapi(SAPI::SAPI0_Sacch);
    EXPECT_EQ(frame.sapi(), SAPI::SAPI0_Sacch);
}

// ── text() output ─────────────────────────────────────────────────────

TEST(L3FrameTest, TextOutput) {
    // Reference format: PD=0x06(RR) high nibble, skip=0, MTI=0x19(SI1)
    L3Frame frame(SAPI::SAPI0, "601900");
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
