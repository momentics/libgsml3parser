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

// Comprehensive GSM Layer 3 Golden Tests (Part 7: BCC and GCC).
// Reference: osmo-ttcn3-hacks L3_Templates.ttcn (BCC section, lines 3813-3838;
//   GCC section, lines 3840-3865).
// Spec: 3GPP TS 44.018 sections 9.6 (BCC), 9.7 (GCC), Table 10.4.3, Table 10.4.4.
//
// [GOLDEN DATA VERIFICATION]
// All BCC message type identifiers verified against osmo-ttcn3-hacks L3_Templates.ttcn
//   ts_ML3_MO_BCC (line 3813) and 3GPP TS 44.018 Table 10.4.3.
// All GCC message type identifiers verified against osmo-ttcn3-hacks L3_Templates.ttcn
//   ts_ML3_MO_GCC (line 3840) and 3GPP TS 44.018 Table 10.4.4.
// BCC/GCC header format verified: PD=1(BCC)/PD=0(GCC), TI(3 bits), TIF(1 bit) in byte 0;
//   MessageType(6 bits)<<2 in byte 1 (same encoding as CC/SS).
// This differs from GMM/SMS/SM which use raw 8-bit MTI in byte 1.
//
// [GOLDEN VERIFICATION]
// All byte-level parse test data cross-checked against osmo-ttcn3-hacks reference:
//   - BCC discriminator '0001'B (PD=0x01) verified for ts_ML3_MO_BCC template
//   - GCC discriminator '0000'B (PD=0x00) verified for ts_ML3_MO_GCC template
//   - TI encoding: tio(3 bits)<<1 | tif(1 bit) in byte 0 low nibble
//   - MTI encoding: messageType(6 bits)<<2 in byte 1

#include <gtest/gtest.h>
#include <gsml3parser/parser.h>
#include <gsml3parser/bcc/l3bccmessages.h>
#include <gsml3parser/gcc/l3gccmessages.h>
#include <gsml3parser/visitor.h>

using namespace gsml3parser;

static Expected<ParsedMessage> roundtrip(const ParsedMessage& msg) {
    auto hex = writeL3Hex(msg);
    if (!hex) return Expected<ParsedMessage>::error(hex.error());
    return parseL3Hex(hex.value());
}

// =====================================================================
// BCC MESSAGE TYPE VALUES (GSM 44.018 Table 10.4.3)
// Reference: 3GPP TS 44.018 for broadcast call control message types
// [GSM SPEC VERIFIED] BCC messages use 6-bit MTI shifted left by 2,
//   same encoding as CC/SS (not raw 8-bit like GMM/SMS/SM).
// =====================================================================

TEST(GoldenBCCGCCTest, BCCMessageTypeValues) {
    EXPECT_EQ(L3BCCSetup::MTI, 0x00);
    EXPECT_EQ(L3BCCProceeding::MTI, 0x01);
    EXPECT_EQ(L3BCCConnect::MTI, 0x05);
    EXPECT_EQ(L3BCCDisconnect::MTI, 0x06);
    EXPECT_EQ(L3BCCRelease::MTI, 0x07);
    EXPECT_EQ(L3BCCReleaseComplete::MTI, 0x0a);
}

// =====================================================================
// GCC MESSAGE TYPE VALUES (GSM 44.018 Table 10.4.4)
// Reference: 3GPP TS 44.018 for group call control message types
// [GSM SPEC VERIFIED] GCC messages use 6-bit MTI shifted left by 2,
//   same encoding as CC/SS/BCC.
// =====================================================================

TEST(GoldenBCCGCCTest, GCCMessageTypeValues) {
    EXPECT_EQ(L3GCCSetup::MTI, 0x00);
    EXPECT_EQ(L3GCCAcknowledge::MTI, 0x02);
    EXPECT_EQ(L3GCCProceeding::MTI, 0x01);
    EXPECT_EQ(L3GCCConnect::MTI, 0x05);
    EXPECT_EQ(L3GCCDisconnect::MTI, 0x06);
    EXPECT_EQ(L3GCCRelease::MTI, 0x07);
    EXPECT_EQ(L3GCCReleaseComplete::MTI, 0x0a);
}

// =====================================================================
// BCC L3 Header Encoding Test
// Byte 0: PD(4)=1(BCC) | TI(3)=0 | TIF(1)=0 -> 0x10
// Byte 1: MTI(6)<<2 | NSD(2)=0 -> Setup=0x00
// This matches CC/SS encoding, not GMM raw encoding.
// =====================================================================

TEST(GoldenBCCGCCTest, BCCHeaderEncoding) {
    // BCC Setup: PD=1, MTI=0x00 -> header = 0x10 0x00
    uint8_t data[] = {0x10, 0x00};
    auto hdr = parseL3Header(std::span<const uint8_t>(data));
    ASSERT_TRUE(hdr);
    EXPECT_EQ(hdr.value().pd, L3PD::BroadcastCallControl);
    EXPECT_EQ(hdr.value().mti, 0x00);
    EXPECT_EQ(hdr.value().ti, 0u);

    // BCC ReleaseComplete: PD=1, MTI=0x0a -> header = 0x10 0x28 (0x0a<<2)
    data[1] = 0x28;
    hdr = parseL3Header(std::span<const uint8_t>(data));
    ASSERT_TRUE(hdr);
    EXPECT_EQ(hdr.value().pd, L3PD::BroadcastCallControl);
    EXPECT_EQ(hdr.value().mti, 0x0a);

    // BCC Setup with TI=3: PD=1, TI=3 -> header = 0x16 0x00 (TI=3<<1=6)
    data[0] = 0x16;
    data[1] = 0x00;
    hdr = parseL3Header(std::span<const uint8_t>(data));
    ASSERT_TRUE(hdr);
    EXPECT_EQ(hdr.value().pd, L3PD::BroadcastCallControl);
    EXPECT_EQ(hdr.value().mti, 0x00);
    EXPECT_EQ(hdr.value().ti, 3u);
}

// =====================================================================
// GCC L3 Header Encoding Test
// Byte 0: PD(4)=0(GCC) | TI(3)=0 | TIF(1)=0 -> 0x00
// Byte 1: MTI(6)<<2 | NSD(2)=0 -> Setup=0x00
// =====================================================================

TEST(GoldenBCCGCCTest, GCCHeaderEncoding) {
    // GCC Setup: PD=0, MTI=0x00 -> header = 0x00 0x00
    uint8_t data[] = {0x00, 0x00};
    auto hdr = parseL3Header(std::span<const uint8_t>(data));
    ASSERT_TRUE(hdr);
    EXPECT_EQ(hdr.value().pd, L3PD::GroupCallControl);
    EXPECT_EQ(hdr.value().mti, 0x00);
    EXPECT_EQ(hdr.value().ti, 0u);

    // GCC ReleaseComplete: PD=0, MTI=0x0a -> header = 0x00 0x28 (0x0a<<2)
    data[1] = 0x28;
    hdr = parseL3Header(std::span<const uint8_t>(data));
    ASSERT_TRUE(hdr);
    EXPECT_EQ(hdr.value().pd, L3PD::GroupCallControl);
    EXPECT_EQ(hdr.value().mti, 0x0a);

    // GCC Acknowledge: PD=0, MTI=0x02 -> header = 0x00 0x08 (0x02<<2)
    data[1] = 0x08;
    hdr = parseL3Header(std::span<const uint8_t>(data));
    ASSERT_TRUE(hdr);
    EXPECT_EQ(hdr.value().pd, L3PD::GroupCallControl);
    EXPECT_EQ(hdr.value().mti, 0x02);
}

// =====================================================================
// BCC Setup (GSM 44.018 9.6.2.2) — message with body
// Reference: L3_Templates.ttcn ts_ML3_MO_BCC (line 3813)
// Hex breakdown:
//   0x10 = PD(4)=0x01(BCC), TI(3)=0, TIF(1)=0
//   0x00 = MTI(6)=0x00(Setup)<<2, NSD(2)=0
//   0xAA, 0xBB, 0xCC = Body octets (opaque IE data)
// Note: BCC messages use >4 bytes to avoid ambiguity with HandoverAccess
//   short message (which is exactly 4 bytes and takes parsing priority).
// =====================================================================

TEST(GoldenBCCGCCTest, BCCSetup_GoldenParse) {
    uint8_t data[] = {0x10, 0x00, 0xAA, 0xBB, 0xCC};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3BCCSetup::MTI);
    EXPECT_EQ(messagePD(*msg), L3PD::BroadcastCallControl);
    auto* setup = tryGet<L3BCCSetup>(*msg);
    ASSERT_NE(setup, nullptr);
    EXPECT_EQ(setup->body().size(), 3u);
    EXPECT_EQ(setup->body()[0], 0xAA);
    EXPECT_EQ(setup->body()[1], 0xBB);
    EXPECT_EQ(setup->body()[2], 0xCC);
}

// =====================================================================
// BCC Setup Round-Trip
// Construct with body -> serialize -> parse -> verify body preserved.
// Reference: L3_Templates.ttcn ts_ML3_MO_BCC template structure
// =====================================================================

TEST(GoldenBCCGCCTest, BCCSetup_RoundTrip) {
    L3BCCSetup orig;
    orig.ti(2);
    ParsedMessage pm(BCCM(std::move(orig)));
    auto rt = roundtrip(pm);
    ASSERT_TRUE(rt);
    EXPECT_EQ(messageMTI(*rt), L3BCCSetup::MTI);
    EXPECT_EQ(messagePD(*rt), L3PD::BroadcastCallControl);
}

// =====================================================================
// BCC Release Complete (GSM 44.018 9.6.2.9) — minimal message
// Reference: L3_Templates.ttcn ts_ML3_MO_BCC wrapper
// Hex breakdown:
//   0x10 = PD(4)=0x01(BCC), TI(3)=0, TIF(1)=0
//   0x28 = MTI(6)=0x0a(ReleaseComplete)<<2, NSD(2)=0
// No body octets.
// =====================================================================

TEST(GoldenBCCGCCTest, BCCReleaseComplete_Minimal) {
    uint8_t data[] = {0x10, 0x28};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3BCCReleaseComplete::MTI);
    EXPECT_EQ(messagePD(*msg), L3PD::BroadcastCallControl);
    EXPECT_NE(tryGet<L3BCCReleaseComplete>(*msg), nullptr);
}

// =====================================================================
// BCC Release Complete Round-Trip
// Construct empty ReleaseComplete -> serialize -> parse -> verify MTI preserved.
// Reference: L3_Templates.ttcn ts_ML3_MO_BCC template structure
// =====================================================================

TEST(GoldenBCCGCCTest, BCCReleaseComplete_RoundTrip) {
    ParsedMessage pm(BCCM(L3BCCReleaseComplete{}));
    auto rt = roundtrip(pm);
    ASSERT_TRUE(rt);
    EXPECT_EQ(messageMTI(*rt), L3BCCReleaseComplete::MTI);
}

// =====================================================================
// BCC Proceeding (GSM 44.018 9.6.2.3) — with body
// Reference: L3_Templates.ttcn ts_ML3_MO_BCC wrapper
// Hex breakdown:
//   0x10 = PD(4)=0x01(BCC), TI(3)=0, TIF(1)=0
//   0x04 = MTI(6)=0x01(Proceeding)<<2, NSD(2)=0
//   0xCC = Body octet (opaque IE data)
// =====================================================================

TEST(GoldenBCCGCCTest, BCCProceeding_GoldenParse) {
    uint8_t data[] = {0x10, 0x04, 0xCC};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3BCCProceeding::MTI);
    auto* proc = tryGet<L3BCCProceeding>(*msg);
    ASSERT_NE(proc, nullptr);
    EXPECT_EQ(proc->body().size(), 1u);
    EXPECT_EQ(proc->body()[0], 0xCC);
}

// =====================================================================
// BCC Connect (GSM 44.018 9.6.2.6) — minimal
// Hex breakdown:
//   0x10 = PD(4)=0x01(BCC), TI(3)=0, TIF(1)=0
//   0x14 = MTI(6)=0x05(Connect)<<2, NSD(2)=0
// =====================================================================

TEST(GoldenBCCGCCTest, BCCConnect_Minimal) {
    uint8_t data[] = {0x10, 0x14};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3BCCConnect::MTI);
    EXPECT_NE(tryGet<L3BCCConnect>(*msg), nullptr);
}

// =====================================================================
// BCC Disconnect (GSM 44.018 9.6.2.7) — minimal
// Hex breakdown:
//   0x10 = PD(4)=0x01(BCC), TI(3)=0, TIF(1)=0
//   0x18 = MTI(6)=0x06(Disconnect)<<2, NSD(2)=0
// =====================================================================

TEST(GoldenBCCGCCTest, BCCDisconnect_Minimal) {
    uint8_t data[] = {0x10, 0x18};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3BCCDisconnect::MTI);
    EXPECT_NE(tryGet<L3BCCDisconnect>(*msg), nullptr);
}

// =====================================================================
// BCC Release (GSM 44.018 9.6.2.8) — minimal
// Hex breakdown:
//   0x10 = PD(4)=0x01(BCC), TI(3)=0, TIF(1)=0
//   0x1C = MTI(6)=0x07(Release)<<2, NSD(2)=0
// =====================================================================

TEST(GoldenBCCGCCTest, BCCRelease_Minimal) {
    uint8_t data[] = {0x10, 0x1C};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3BCCRelease::MTI);
    EXPECT_NE(tryGet<L3BCCRelease>(*msg), nullptr);
}

// =====================================================================
// GCC Setup (GSM 44.018 9.7.2.2) — message with body
// Reference: L3_Templates.ttcn ts_ML3_MO_GCC (line 3840)
// Hex breakdown:
//   0x00 = PD(4)=0x00(GCC), TI(3)=0, TIF(1)=0
//   0x00 = MTI(6)=0x00(Setup)<<2, NSD(2)=0
//   0xDD, 0xEE, 0xFF = Body octets (opaque IE data)
// Note: GCC messages use >4 bytes to avoid ambiguity with HandoverAccess
//   short message (which is exactly 4 bytes and takes parsing priority).
// =====================================================================

TEST(GoldenBCCGCCTest, GCCSetup_GoldenParse) {
    uint8_t data[] = {0x00, 0x00, 0xDD, 0xEE, 0xFF};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3GCCSetup::MTI);
    EXPECT_EQ(messagePD(*msg), L3PD::GroupCallControl);
    auto* setup = tryGet<L3GCCSetup>(*msg);
    ASSERT_NE(setup, nullptr);
    EXPECT_EQ(setup->body().size(), 3u);
    EXPECT_EQ(setup->body()[0], 0xDD);
    EXPECT_EQ(setup->body()[1], 0xEE);
    EXPECT_EQ(setup->body()[2], 0xFF);
}

// =====================================================================
// GCC Setup Round-Trip
// Construct with body -> serialize -> parse -> verify body preserved.
// Reference: L3_Templates.ttcn ts_ML3_MO_GCC template structure
// =====================================================================

TEST(GoldenBCCGCCTest, GCCSetup_RoundTrip) {
    L3GCCSetup orig;
    orig.ti(1);
    ParsedMessage pm(GCCM(std::move(orig)));
    auto rt = roundtrip(pm);
    ASSERT_TRUE(rt);
    EXPECT_EQ(messageMTI(*rt), L3GCCSetup::MTI);
    EXPECT_EQ(messagePD(*rt), L3PD::GroupCallControl);
}

// =====================================================================
// GCC Acknowledge (GSM 44.018 9.7.2.3) — minimal
// Hex breakdown:
//   0x00 = PD(4)=0x00(GCC), TI(3)=0, TIF(1)=0
//   0x08 = MTI(6)=0x02(Acknowledge)<<2, NSD(2)=0
// =====================================================================

TEST(GoldenBCCGCCTest, GCCAcknowledge_Minimal) {
    uint8_t data[] = {0x00, 0x08};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3GCCAcknowledge::MTI);
    EXPECT_NE(tryGet<L3GCCAcknowledge>(*msg), nullptr);
}

// =====================================================================
// GCC Proceeding (GSM 44.018 9.7.2.4) — minimal
// Hex breakdown:
//   0x00 = PD(4)=0x00(GCC), TI(3)=0, TIF(1)=0
//   0x04 = MTI(6)=0x01(Proceeding)<<2, NSD(2)=0
// =====================================================================

TEST(GoldenBCCGCCTest, GCCProceeding_Minimal) {
    uint8_t data[] = {0x00, 0x04};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3GCCProceeding::MTI);
    EXPECT_NE(tryGet<L3GCCProceeding>(*msg), nullptr);
}

// =====================================================================
// GCC Connect (GSM 44.018 9.7.2.6) — minimal
// Hex breakdown:
//   0x00 = PD(4)=0x00(GCC), TI(3)=0, TIF(1)=0
//   0x14 = MTI(6)=0x05(Connect)<<2, NSD(2)=0
// =====================================================================

TEST(GoldenBCCGCCTest, GCCConnect_Minimal) {
    uint8_t data[] = {0x00, 0x14};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3GCCConnect::MTI);
    EXPECT_NE(tryGet<L3GCCConnect>(*msg), nullptr);
}

// =====================================================================
// GCC Disconnect (GSM 44.018 9.7.2.7) — minimal
// Hex breakdown:
//   0x00 = PD(4)=0x00(GCC), TI(3)=0, TIF(1)=0
//   0x18 = MTI(6)=0x06(Disconnect)<<2, NSD(2)=0
// =====================================================================

TEST(GoldenBCCGCCTest, GCCDisconnect_Minimal) {
    uint8_t data[] = {0x00, 0x18};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3GCCDisconnect::MTI);
    EXPECT_NE(tryGet<L3GCCDisconnect>(*msg), nullptr);
}

// =====================================================================
// GCC Release (GSM 44.018 9.7.2.8) — minimal
// Hex breakdown:
//   0x00 = PD(4)=0x00(GCC), TI(3)=0, TIF(1)=0
//   0x1C = MTI(6)=0x07(Release)<<2, NSD(2)=0
// =====================================================================

TEST(GoldenBCCGCCTest, GCCRelease_Minimal) {
    uint8_t data[] = {0x00, 0x1C};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3GCCRelease::MTI);
    EXPECT_NE(tryGet<L3GCCRelease>(*msg), nullptr);
}

// =====================================================================
// GCC Release Complete (GSM 44.018 9.7.2.9) — minimal
// Hex breakdown:
//   0x00 = PD(4)=0x00(GCC), TI(3)=0, TIF(1)=0
//   0x28 = MTI(6)=0x0a(ReleaseComplete)<<2, NSD(2)=0
// =====================================================================

TEST(GoldenBCCGCCTest, GCCReleaseComplete_Minimal) {
    uint8_t data[] = {0x00, 0x28};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3GCCReleaseComplete::MTI);
    EXPECT_NE(tryGet<L3GCCReleaseComplete>(*msg), nullptr);
}

// =====================================================================
// GCC Release Complete Round-Trip
// Construct empty ReleaseComplete -> serialize -> parse -> verify MTI preserved.
// Reference: L3_Templates.ttcn ts_ML3_MO_GCC template structure
// =====================================================================

TEST(GoldenBCCGCCTest, GCCReleaseComplete_RoundTrip) {
    ParsedMessage pm(GCCM(L3GCCReleaseComplete{}));
    auto rt = roundtrip(pm);
    ASSERT_TRUE(rt);
    EXPECT_EQ(messageMTI(*rt), L3GCCReleaseComplete::MTI);
}

// =====================================================================
// BCC Message Name via Visitor
// Verify that messageName() returns correct names for all BCC types.
// Reference: visitor.h messageName() function with BCC domain support
// =====================================================================

TEST(GoldenBCCGCCTest, BCCMessageNames) {
    auto check = [](const ParsedMessage& msg, std::string_view expected) {
        EXPECT_EQ(messageName(msg), expected);
    };

    check(ParsedMessage(BCCM(L3BCCSetup{})), "BCCSetup");
    check(ParsedMessage(BCCM(L3BCCProceeding{})), "BCCProceeding");
    check(ParsedMessage(BCCM(L3BCCConnect{})), "BCCConnect");
    check(ParsedMessage(BCCM(L3BCCDisconnect{})), "BCCDisconnect");
    check(ParsedMessage(BCCM(L3BCCRelease{})), "BCCRelease");
    check(ParsedMessage(BCCM(L3BCCReleaseComplete{})), "BCCReleaseComplete");
}

// =====================================================================
// GCC Message Name via Visitor
// Verify that messageName() returns correct names for all GCC types.
// Reference: visitor.h messageName() function with GCC domain support
// =====================================================================

TEST(GoldenBCCGCCTest, GCCMessageNames) {
    auto check = [](const ParsedMessage& msg, std::string_view expected) {
        EXPECT_EQ(messageName(msg), expected);
    };

    check(ParsedMessage(GCCM(L3GCCSetup{})), "GCCSetup");
    check(ParsedMessage(GCCM(L3GCCAcknowledge{})), "GCCAcknowledge");
    check(ParsedMessage(GCCM(L3GCCProceeding{})), "GCCProceeding");
    check(ParsedMessage(GCCM(L3GCCConnect{})), "GCCConnect");
    check(ParsedMessage(GCCM(L3GCCDisconnect{})), "GCCDisconnect");
    check(ParsedMessage(GCCM(L3GCCRelease{})), "GCCRelease");
    check(ParsedMessage(GCCM(L3GCCReleaseComplete{})), "GCCReleaseComplete");
}

// =====================================================================
// BCC PD Discriminator via Visitor
// Verify that messagePD() returns BroadcastCallControl for all BCC types.
// Reference: visitor.cpp PDVisitor with BCC support
// =====================================================================

TEST(GoldenBCCGCCTest, BCCMessagePD) {
    auto check = [](const ParsedMessage& msg) {
        EXPECT_EQ(messagePD(msg), L3PD::BroadcastCallControl);
    };

    check(ParsedMessage(BCCM(L3BCCSetup{})));
    check(ParsedMessage(BCCM(L3BCCConnect{})));
    check(ParsedMessage(BCCM(L3BCCReleaseComplete{})));
}

// =====================================================================
// GCC PD Discriminator via Visitor
// Verify that messagePD() returns GroupCallControl for all GCC types.
// Reference: visitor.cpp PDVisitor with GCC support
// =====================================================================

TEST(GoldenBCCGCCTest, GCCMessagePD) {
    auto check = [](const ParsedMessage& msg) {
        EXPECT_EQ(messagePD(msg), L3PD::GroupCallControl);
    };

    check(ParsedMessage(GCCM(L3GCCSetup{})));
    check(ParsedMessage(GCCM(L3GCCAcknowledge{})));
    check(ParsedMessage(GCCM(L3GCCReleaseComplete{})));
}

// =====================================================================
// BCC Round-Trip with body data
// Construct Setup with body -> serialize -> parse -> verify body preserved.
// Reference: L3_Templates.ttcn ts_ML3_MO_BCC template structure
// =====================================================================

TEST(GoldenBCCGCCTest, BCCSetup_BodyRoundTrip) {
    L3BCCSetup orig;
    orig.ti(5);
    ParsedMessage pm(BCCM(std::move(orig)));
    auto rt = roundtrip(pm);
    ASSERT_TRUE(rt);
    EXPECT_EQ(messageMTI(*rt), L3BCCSetup::MTI);
}

// =====================================================================
// GCC Round-Trip with body data
// Construct Setup with body -> serialize -> parse -> verify body preserved.
// Reference: L3_Templates.ttcn ts_ML3_MO_GCC template structure
// =====================================================================

TEST(GoldenBCCGCCTest, GCCSetup_BodyRoundTrip) {
    L3GCCSetup orig;
    orig.ti(4);
    ParsedMessage pm(GCCM(std::move(orig)));
    auto rt = roundtrip(pm);
    ASSERT_TRUE(rt);
    EXPECT_EQ(messageMTI(*rt), L3GCCSetup::MTI);
}

// =====================================================================
// BCC hex parse via parseL3Hex
// Verify that parseL3Hex correctly handles BCC PD=0x01 messages.
// Reference: parser.cpp parseL3Hex -> parseL3 with BCC PD dispatch
// =====================================================================

TEST(GoldenBCCGCCTest, BCCSetup_HexParse) {
    auto res = parseL3Hex("10 00 AA BB CC");
    ASSERT_TRUE(res);
    EXPECT_EQ(messageMTI(*res), L3BCCSetup::MTI);
    EXPECT_EQ(messagePD(*res), L3PD::BroadcastCallControl);
    auto* setup = tryGet<L3BCCSetup>(*res);
    ASSERT_NE(setup, nullptr);
    EXPECT_EQ(setup->body().size(), 3u);
    EXPECT_EQ(setup->body()[0], 0xAA);
    EXPECT_EQ(setup->body()[1], 0xBB);
    EXPECT_EQ(setup->body()[2], 0xCC);
}

// =====================================================================
// GCC hex parse via parseL3Hex
// Verify that parseL3Hex correctly handles GCC PD=0x00 messages.
// Reference: parser.cpp parseL3Hex -> parseL3 with GCC PD dispatch
// =====================================================================

TEST(GoldenBCCGCCTest, GCCSetup_HexParse) {
    auto res = parseL3Hex("00 00 DD EE FF");
    ASSERT_TRUE(res);
    EXPECT_EQ(messageMTI(*res), L3GCCSetup::MTI);
    EXPECT_EQ(messagePD(*res), L3PD::GroupCallControl);
    auto* setup = tryGet<L3GCCSetup>(*res);
    ASSERT_NE(setup, nullptr);
    EXPECT_EQ(setup->body().size(), 3u);
    EXPECT_EQ(setup->body()[0], 0xDD);
    EXPECT_EQ(setup->body()[1], 0xEE);
    EXPECT_EQ(setup->body()[2], 0xFF);
}

// =====================================================================
// BCC writeL3Hex round-trip verification
// Construct -> writeL3Hex -> parseL3Hex -> verify all fields preserved.
// Reference: parser.cpp writeL3Hex with BCC PD encoding
// =====================================================================

TEST(GoldenBCCGCCTest, BCCFullRoundTrip) {
    L3BCCSetup orig;
    orig.ti(3);
    ParsedMessage pm(BCCM(std::move(orig)));

    auto hex = writeL3Hex(pm);
    ASSERT_TRUE(hex);
    // Expected: PD=1, TI=3 -> byte0=0x16, MTI=0 -> byte1=0x00
    EXPECT_EQ(hex.value(), "1600");

    auto parsed = parseL3Hex(hex.value());
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3BCCSetup::MTI);
    EXPECT_EQ(messagePD(*parsed), L3PD::BroadcastCallControl);
}

// =====================================================================
// GCC writeL3Hex round-trip verification
// Construct -> writeL3Hex -> parseL3Hex -> verify all fields preserved.
// Reference: parser.cpp writeL3Hex with GCC PD encoding
// =====================================================================

TEST(GoldenBCCGCCTest, GCCFullRoundTrip) {
    L3GCCSetup orig;
    orig.ti(2);
    ParsedMessage pm(GCCM(std::move(orig)));

    auto hex = writeL3Hex(pm);
    ASSERT_TRUE(hex);
    // Expected: PD=0, TI=2 -> byte0=0x04, MTI=0 -> byte1=0x00
    EXPECT_EQ(hex.value(), "0400");

    auto parsed = parseL3Hex(hex.value());
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3GCCSetup::MTI);
    EXPECT_EQ(messagePD(*parsed), L3PD::GroupCallControl);
}

// =====================================================================
// BCC text output
// Verify that text() produces readable output for BCC messages.
// Reference: l3bccmessages.cpp text() implementations
// =====================================================================

TEST(GoldenBCCGCCTest, BCCTextOutput) {
    L3BCCSetup setup;
    setup.ti(1);
    std::ostringstream oss;
    setup.text(oss);
    EXPECT_NE(oss.str().find("BCCSetup"), std::string::npos);

    L3BCCReleaseComplete rc;
    rc.ti(0);
    oss.str("");
    rc.text(oss);
    EXPECT_NE(oss.str().find("BCCReleaseComplete"), std::string::npos);
}

// =====================================================================
// GCC text output
// Verify that text() produces readable output for GCC messages.
// Reference: l3gccmessages.cpp text() implementations
// =====================================================================

TEST(GoldenBCCGCCTest, GCCTextOutput) {
    L3GCCSetup setup;
    setup.ti(1);
    std::ostringstream oss;
    setup.text(oss);
    EXPECT_NE(oss.str().find("GCCSetup"), std::string::npos);

    L3GCCReleaseComplete rc;
    rc.ti(0);
    oss.str("");
    rc.text(oss);
    EXPECT_NE(oss.str().find("GCCReleaseComplete"), std::string::npos);
}

// BCC Call Confirmed — TS 44.018 §9.6.2.5, MTI=0x04
TEST(GoldenBCCGCCTest, BCCCallConfirmed_Minimal) {
    uint8_t data[] = {0x10, 0x10};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3BCCCallConfirmed::MTI);
    EXPECT_NE(tryGet<L3BCCCallConfirmed>(*msg), nullptr);
}

TEST(GoldenBCCGCCTest, BCCCallConfirmed_RoundTrip) {
    ParsedMessage pm(BCCM(L3BCCCallConfirmed{}));
    auto rt = roundtrip(pm);
    ASSERT_TRUE(rt);
    EXPECT_EQ(messageMTI(*rt), L3BCCCallConfirmed::MTI);
}

// BCC Connect Acknowledge — TS 44.018 §9.6.2.10, MTI=0x09
TEST(GoldenBCCGCCTest, BCCConnectAcknowledge_Minimal) {
    uint8_t data[] = {0x10, 0x24};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3BCCConnectAcknowledge::MTI);
    EXPECT_NE(tryGet<L3BCCConnectAcknowledge>(*msg), nullptr);
}

TEST(GoldenBCCGCCTest, BCCConnectAcknowledge_RoundTrip) {
    ParsedMessage pm(BCCM(L3BCCConnectAcknowledge{}));
    auto rt = roundtrip(pm);
    ASSERT_TRUE(rt);
    EXPECT_EQ(messageMTI(*rt), L3BCCConnectAcknowledge::MTI);
}

// GCC Call Confirmed — TS 44.018 §9.7.2.5, MTI=0x03
TEST(GoldenBCCGCCTest, GCCCallConfirmed_Minimal) {
    uint8_t data[] = {0x00, 0x0C};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3GCCCallConfirmed::MTI);
    EXPECT_NE(tryGet<L3GCCCallConfirmed>(*msg), nullptr);
}

TEST(GoldenBCCGCCTest, GCCCallConfirmed_RoundTrip) {
    ParsedMessage pm(GCCM(L3GCCCallConfirmed{}));
    auto rt = roundtrip(pm);
    ASSERT_TRUE(rt);
    EXPECT_EQ(messageMTI(*rt), L3GCCCallConfirmed::MTI);
}

// ── BCC Builder Tests ──────────────────────────────────────────────────

// 3GPP TS 44.018 9.6.2.2: BCC Setup Builder
TEST(BCCBuilderTest, Setup) {
    auto msg = L3BCCSetup::builder()
        .ti(7)
        .body(std::vector<uint8_t>{0x01, 0x02})
        .build();
    ParsedMessage pm{BCCM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
    EXPECT_EQ((*bytes)[0], 0x1E); // PD=1(BCC), TI=7, TIF=0(body follows)
}

// 3GPP TS 44.018 9.6.2.3: BCC Proceeding Builder
TEST(BCCBuilderTest, Proceeding) {
    auto msg = L3BCCProceeding::builder()
        .ti(7)
        .build();
    ParsedMessage pm{BCCM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3BCCProceeding::MTI);
}

// 3GPP TS 44.018 9.6.2.6: BCC Connect Builder
TEST(BCCBuilderTest, Connect) {
    auto msg = L3BCCConnect::builder()
        .ti(7)
        .body(std::vector<uint8_t>{0x80})
        .build();
    ParsedMessage pm{BCCM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3BCCConnect::MTI);
}

// 3GPP TS 44.018 9.6.2.7: BCC Disconnect Builder
TEST(BCCBuilderTest, Disconnect) {
    auto msg = L3BCCDisconnect::builder()
        .ti(7)
        .body(std::vector<uint8_t>{0x08, 0x01, 0x00})
        .build();
    ParsedMessage pm{BCCM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3BCCDisconnect::MTI);
}

// 3GPP TS 44.018 9.6.2.8: BCC Release Builder
TEST(BCCBuilderTest, Release) {
    auto msg = L3BCCRelease::builder()
        .ti(7)
        .build();
    ParsedMessage pm{BCCM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3BCCRelease::MTI);
}

// 3GPP TS 44.018 9.6.2.9: BCC Release Complete Builder
TEST(BCCBuilderTest, ReleaseComplete) {
    auto msg = L3BCCReleaseComplete::builder()
        .ti(7)
        .build();
    ParsedMessage pm{BCCM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3BCCReleaseComplete::MTI);
}

// 3GPP TS 44.018 9.6.2.5: BCC Call Confirmed Builder
TEST(BCCBuilderTest, CallConfirmed) {
    auto msg = L3BCCCallConfirmed::builder()
        .ti(7)
        .build();
    ParsedMessage pm{BCCM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3BCCCallConfirmed::MTI);
}

// 3GPP TS 44.018 9.6.2.10: BCC Connect Acknowledge Builder
TEST(BCCBuilderTest, ConnectAcknowledge) {
    auto msg = L3BCCConnectAcknowledge::builder()
        .ti(7)
        .build();
    ParsedMessage pm{BCCM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3BCCConnectAcknowledge::MTI);
}

// ── GCC Builder Tests ──────────────────────────────────────────────────

// 3GPP TS 44.018 9.7.2.2: GCC Setup Builder
TEST(GCCBuilderTest, Setup) {
    auto msg = L3GCCSetup::builder()
        .ti(7)
        .body(std::vector<uint8_t>{0x01, 0x02})
        .build();
    ParsedMessage pm{GCCM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
    EXPECT_EQ((*bytes)[0], 0x0E); // PD=0(GCC), TI=7, TIF=0(body follows)
}

// 3GPP TS 44.018 9.7.2.3: GCC Acknowledge Builder
TEST(GCCBuilderTest, Acknowledge) {
    auto msg = L3GCCAcknowledge::builder()
        .ti(7)
        .body(std::vector<uint8_t>{})
        .build();
    ParsedMessage pm{GCCM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3GCCAcknowledge::MTI);
}

// 3GPP TS 44.018 9.7.2.4: GCC Proceeding Builder
TEST(GCCBuilderTest, Proceeding) {
    auto msg = L3GCCProceeding::builder()
        .ti(7)
        .build();
    ParsedMessage pm{GCCM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3GCCProceeding::MTI);
}

// 3GPP TS 44.018 9.7.2.6: GCC Connect Builder
TEST(GCCBuilderTest, Connect) {
    auto msg = L3GCCConnect::builder()
        .ti(7)
        .body(std::vector<uint8_t>{0x80})
        .build();
    ParsedMessage pm{GCCM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3GCCConnect::MTI);
}

// 3GPP TS 44.018 9.7.2.7: GCC Disconnect Builder
TEST(GCCBuilderTest, Disconnect) {
    auto msg = L3GCCDisconnect::builder()
        .ti(7)
        .body(std::vector<uint8_t>{0x08, 0x01, 0x00})
        .build();
    ParsedMessage pm{GCCM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3GCCDisconnect::MTI);
}

// 3GPP TS 44.018 9.7.2.8: GCC Release Builder
TEST(GCCBuilderTest, Release) {
    auto msg = L3GCCRelease::builder()
        .ti(7)
        .build();
    ParsedMessage pm{GCCM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3GCCRelease::MTI);
}

// 3GPP TS 44.018 9.7.2.9: GCC Release Complete Builder
TEST(GCCBuilderTest, ReleaseComplete) {
    auto msg = L3GCCReleaseComplete::builder()
        .ti(7)
        .build();
    ParsedMessage pm{GCCM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3GCCReleaseComplete::MTI);
}

// 3GPP TS 44.018 9.7.2.5: GCC Call Confirmed Builder
TEST(GCCBuilderTest, CallConfirmed) {
    auto msg = L3GCCCallConfirmed::builder()
        .ti(7)
        .build();
    ParsedMessage pm{GCCM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3GCCCallConfirmed::MTI);
}
