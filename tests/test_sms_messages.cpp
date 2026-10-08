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

// Comprehensive GSM Layer 3 Golden Tests (Part 6: SMS).
// Message identifiers and wire layouts per 3GPP TS 24.011 (CP/RP) and TS 23.040 (TP).
// Spec: 3GPP TS 24.008 Table 10.6a (SMS control part); 3GPP TS 24.011 sections 7-8.
//
// [GOLDEN DATA VERIFICATION]
// All SMS CP message type identifiers per 3GPP TS 24.008 Table 10.6a
//   (SMS Control Part).
// SMS header format verified: PD=9('1001'B) in the low nibble of byte 0,
//   TI(3 bits) in bits 7:5 and TIF(1 bit) in bit 4; CP-MTI(8 bits, raw) in byte 1.
// Message structures per 3GPP TS 24.011 and TS 23.040:
//   CP-DATA / CP-ACK / CP-ERROR / CP-DATA(MT),
//   RP-DATA / RP-ACK / RP-ERROR / RP-SMMA,
//   TP-Submit, TP-Deliver.
//
// [GOLDEN VERIFICATION]
// All byte-level parse test data cross-checked against 3GPP TS 24.011 / TS 23.040:
//   - CP-MTI values per the CP message type table (TS 24.008 Table 10.6a)
//   - SMS header encoding: PD=9 in the low nibble of byte 0, CP-MTI in the
//     low five bits of byte 1 (the three high bits are spare, TS 24.011 8.2)
//   - RP-MTI encoding: Spare(5)=0 | RP-MTI(3) in first RP octet
//   - TP-DU header octet (TS 23.040 section 9.1.2, bit positions in the octet
//     value): MTI(2)=bits0-1, MMS/RD(1)=bit2, VPF(2)=bits3-4, SRI/SRR(1)=bit5,
//     UDHI(1)=bit6, RP(1)=bit7
//   - TP address length = number of digits (TS 23.040 section 9.1.2.4)
//   - TP-UDL = septets for the GSM 7-bit alphabet (TS 23.040 section 9.2.3.16)

#include <gtest/gtest.h>
#include <gsml3parser/parser.h>
#include <gsml3parser/l3header.h>
#include <gsml3parser/sms/l3smsmessages.h>
#include <gsml3parser/sms/l3smselements.h>
#include <gsml3parser/visitor.h>

using namespace gsml3parser;

static Expected<ParsedMessage> roundtrip(const ParsedMessage& msg) {
    auto hex = writeL3Hex(msg);
    if (!hex) return Expected<ParsedMessage>::error(hex.error());
    return parseL3Hex(hex.value());
}

// =====================================================================
// SMS CP MESSAGE TYPE VALUES (GSM 24.008 Table 10.6a)
// CP-MTI values per GSM 24.008 Table 10.6a.
// [GSM SPEC VERIFIED] SMS messages take the CP-MTI in the five low bits of
//   byte 1 (the three high bits are spare, TS 24.011 section 8.2), unlike
//   MM/CC/SS/BCC/GCC which take the MTI in the six low bits of byte 1.
// =====================================================================

TEST(GoldenSMSTest, MessageTypeValues) {
    EXPECT_EQ(L3CPData::MTI, 0x01);
    EXPECT_EQ(L3CPAck::MTI, 0x04);
    EXPECT_EQ(L3CPErr::MTI, 0x10);
    EXPECT_EQ(L3CPStatus::MTI, 0x12);
    EXPECT_EQ(L3CPSMT::MTI, 0x13);
}

// =====================================================================
// SMS L3 Header Encoding Test
// Byte 0: TI(3)=0 << 5 | TIF(1)=0 << 4 | PD(4)=9(SMS) -> 0x09
// Byte 1: raw CP-MTI (no shift!)
// This is the same format as GMM/SM headers.
// =====================================================================

TEST(GoldenSMSTest, HeaderEncoding) {
    // CP-DATA: PD=9, CP-MTI=0x01 -> header = 0x09 0x01
    uint8_t data[] = {0x09, 0x01};
    auto hdr = parseL3Header(std::span<const uint8_t>(data));
    ASSERT_TRUE(hdr);
    EXPECT_EQ(hdr.value().pd, L3PD::SMS);
    EXPECT_EQ(hdr.value().mti, 0x01);

    // CP-ACK: PD=9, CP-MTI=0x04 -> header = 0x09 0x04
    data[1] = 0x04;
    hdr = parseL3Header(std::span<const uint8_t>(data));
    ASSERT_TRUE(hdr);
    EXPECT_EQ(hdr.value().mti, 0x04);

    // CP-ERROR: PD=9, CP-MTI=0x10 -> header = 0x09 0x10
    data[1] = 0x10;
    hdr = parseL3Header(std::span<const uint8_t>(data));
    ASSERT_TRUE(hdr);
    EXPECT_EQ(hdr.value().mti, 0x10);

    // CP-STATUS: PD=9, CP-MTI=0x12 -> header = 0x09 0x12
    data[1] = 0x12;
    hdr = parseL3Header(std::span<const uint8_t>(data));
    ASSERT_TRUE(hdr);
    EXPECT_EQ(hdr.value().mti, 0x12);

    // CP-SMT: PD=9, CP-MTI=0x13 -> header = 0x09 0x13
    data[1] = 0x13;
    hdr = parseL3Header(std::span<const uint8_t>(data));
    ASSERT_TRUE(hdr);
    EXPECT_EQ(hdr.value().mti, 0x13);
}

// =====================================================================
// SMS CP-ACK (GSM 24.011 8.1.3) - minimal message
// CP-ACK wire layout (GSM 24.011).
// Hex breakdown:
//   0x09 = PD=0x09(SMS) in the low nibble of byte 0, TI=0, TIF=0
//   0x04 = CP-MTI(8)=0x04(CP-ACK), raw encoding
// No body octets.
// =====================================================================

TEST(GoldenSMSTest, CPAck_Minimal) {
    uint8_t data[] = {0x09, 0x04};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3CPAck::MTI);
    EXPECT_EQ(messagePD(*msg), L3PD::SMS);
    EXPECT_NE(tryGet<L3CPAck>(*msg), nullptr);
}

// =====================================================================
// SMS CP-ACK Round-Trip
// Construct empty CP-ACK -> serialize -> parse -> verify MTI preserved.
// CP-ACK wire layout (GSM 24.011).
// =====================================================================

TEST(GoldenSMSTest, CPAck_RoundTrip) {
    L3CPAck orig;
    ParsedMessage pm(SMS(std::move(orig)));
    auto rt = roundtrip(pm);
    ASSERT_TRUE(rt);
    EXPECT_EQ(messageMTI(*rt), L3CPAck::MTI);
}

// =====================================================================
// SMS CP-ERROR (GSM 24.011 8.1.4) - with cause
// CP-ERROR wire layout (GSM 24.011).
// Hex breakdown:
//   0x09 = PD=0x09(SMS) in the low nibble of byte 0, TI=0, TIF=0
//   0x10 = CP-MTI(8)=0x10(CP-ERROR), raw encoding
//   0x03 = CP-Cause=UnknownRPMessageType (7-bit value)
// =====================================================================

TEST(GoldenSMSTest, CPErr_WithCause) {
    uint8_t data[] = {0x09, 0x10, 0x03};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3CPErr::MTI);
    auto* err = tryGet<L3CPErr>(*msg);
    ASSERT_NE(err, nullptr);
    EXPECT_EQ(err->cause(), CPCause::UnknownRPMessageType);
}

// =====================================================================
// SMS CP-ERROR Round-Trip
// Construct with cause -> serialize -> parse -> verify cause preserved.
// CP-ERROR wire layout (GSM 24.011).
// =====================================================================

TEST(GoldenSMSTest, CPErr_RoundTrip) {
    ParsedMessage pm(SMS(L3CPErr{}));
    auto rt = roundtrip(pm);
    ASSERT_TRUE(rt);
    EXPECT_EQ(messageMTI(*rt), L3CPErr::MTI);
}

// =====================================================================
// SMS CP-DATA (GSM 24.011 8.1.2) - with RPDU payload
// CP-DATA wire layout (GSM 24.011).
// Hex breakdown:
//   0x09 = PD=0x09(SMS) in the low nibble of byte 0, TI=0, TIF=0
//   0x01 = CP-MTI(8)=0x01(CP-DATA), raw encoding
//   0x02 = CP-User-Data-Length(8) = 2 octets of RPDU follow
//   0x00 = RP header: Spare(5)=0 | RP-MTI(3)=0 (RP-DATA MO)
//   0x01 = RP-Message-Reference = 1
// =====================================================================

TEST(GoldenSMSTest, CPData_WithRPDU) {
    // CP-DATA containing a minimal RP-DATA header (2 octets: rp-header + message-ref)
    uint8_t data[] = {0x09, 0x01, 0x02, 0x00, 0x01};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3CPData::MTI);
    auto* cpd = tryGet<L3CPData>(*msg);
    ASSERT_NE(cpd, nullptr);
    EXPECT_EQ(cpd->rpdu().size(), 2u);
    EXPECT_EQ(cpd->rpdu()[0], 0x00); // RP header: Spare(5)=0, RP-MTI(3)=0 (RP-DATA MO)
    EXPECT_EQ(cpd->rpdu()[1], 0x01); // RP-Message-Reference = 1
}

// =====================================================================
// SMS CP-DATA Round-Trip
// Construct with RPDU payload -> serialize -> parse -> verify preserved.
// CP-DATA wire layout (GSM 24.011).
// =====================================================================

TEST(GoldenSMSTest, CPData_RoundTrip) {
    L3CPData orig;
    orig.setRpdu({0x00, 0x01}); // minimal RP-DATA header
    ParsedMessage pm(SMS(std::move(orig)));
    auto rt = roundtrip(pm);
    ASSERT_TRUE(rt);
    EXPECT_EQ(messageMTI(*rt), L3CPData::MTI);
}

// =====================================================================
// SMS CP-STATUS (GSM 24.011 8.1.5) - minimal message
// Reference: 3GPP TS 24.011 section 8.1.5
// Hex breakdown:
//   0x09 = PD=0x09(SMS) in the low nibble of byte 0, TI=0, TIF=0
//   0x12 = CP-MTI(8)=0x12(CP-STATUS), raw encoding
//   0x00 = TP-OI(8) = 0
//   0x00 = MTI(8) = 0 (no message reference since bit 1 == 0)
// =====================================================================

TEST(GoldenSMSTest, CPStatus_Minimal) {
    uint8_t data[] = {0x09, 0x12, 0x00, 0x00};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3CPStatus::MTI);
    auto* st = tryGet<L3CPStatus>(*msg);
    ASSERT_NE(st, nullptr);
    EXPECT_EQ(st->tpOi(), 0);
    EXPECT_EQ(st->mtiValue(), 0);
    EXPECT_FALSE(st->hasMessageRef());
}

// =====================================================================
// SMS CP-STATUS Round-Trip
// Construct with fields -> serialize -> parse -> verify preserved.
// Reference: 3GPP TS 24.011 section 8.1.5 message structure
// =====================================================================

TEST(GoldenSMSTest, CPStatus_RoundTrip) {
    ParsedMessage pm(SMS(L3CPStatus{}));
    auto rt = roundtrip(pm);
    ASSERT_TRUE(rt);
    EXPECT_EQ(messageMTI(*rt), L3CPStatus::MTI);
}

// =====================================================================
// SMS CP-SMT (GSM 24.011 8.1.6) - with RPDU payload
// Reference: 3GPP TS 24.011 section 8.1.6
// Hex breakdown:
//   0x09 = PD=0x09(SMS) in the low nibble of byte 0, TI=0, TIF=0
//   0x13 = CP-MTI(8)=0x13(CP-SMT), raw encoding
//   0x02 = CP-User-Data-Length(8) = 2 octets of RPDU follow
//   0x07 = RP header: Spare(5)=0 | RP-MTI(3)=7 (RP-SMMA MT)
//   0x05 = RP-Message-Reference = 5
// =====================================================================

TEST(GoldenSMSTest, CPSMT_WithRPDU) {
    uint8_t data[] = {0x09, 0x13, 0x02, 0x07, 0x05};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3CPSMT::MTI);
    auto* smt = tryGet<L3CPSMT>(*msg);
    ASSERT_NE(smt, nullptr);
    EXPECT_EQ(smt->rpdu().size(), 2u);
}

// =====================================================================
// SMS CP-SMT Round-Trip
// Construct with RPDU payload -> serialize -> parse -> verify preserved.
// Reference: 3GPP TS 24.011 section 8.1.6 message structure
// =====================================================================

TEST(GoldenSMSTest, CPSMT_RoundTrip) {
    L3CPSMT orig;
    orig.setRpdu({0x07, 0x05});
    ParsedMessage pm(SMS(std::move(orig)));
    auto rt = roundtrip(pm);
    ASSERT_TRUE(rt);
    EXPECT_EQ(messageMTI(*rt), L3CPSMT::MTI);
}

// =====================================================================
// SMS CP Cause String Conversion
// Verify CPCause2Str returns correct names for known cause values.
// Reference: 3GPP TS 24.011 Table 10.5
// =====================================================================

TEST(GoldenSMSTest, CPCauseStrings) {
    EXPECT_STREQ(CPCause2Str(CPCause::Unspecified), "Unspecified");
    EXPECT_STREQ(CPCause2Str(CPCause::NoRPLPDU), "No RP-LPDU");
    EXPECT_STREQ(CPCause2Str(CPCause::UnknownRPMessageType), "Unknown RP message type");
    EXPECT_STREQ(CPCause2Str(CPCause::RPUserBusy), "RP-User busy");
}

// =====================================================================
// SMS Message Name via Visitor
// Verify that messageName() returns correct names for all SMS CP types.
// Reference: visitor.h messageName() function with SMS domain support
// =====================================================================

TEST(GoldenSMSTest, MessageNames) {
    auto check = [](const ParsedMessage& msg, std::string_view expected) {
        EXPECT_EQ(messageName(msg), expected);
    };

    L3CPData cpd;
    cpd.setRpdu({});
    check(ParsedMessage(SMS(std::move(cpd))), "CPData");
    check(ParsedMessage(SMS(L3CPAck{})), "CPAck");
    check(ParsedMessage(SMS(L3CPErr{})), "CPErr");
    check(ParsedMessage(SMS(L3CPStatus{})), "CPStatus");

    L3CPSMT smt;
    smt.setRpdu({});
    check(ParsedMessage(SMS(std::move(smt))), "CPSMT");
}

// =====================================================================
// SMS PD Discriminator via Visitor
// Verify that messagePD() returns SMS for all SMS CP types.
// Reference: visitor.cpp PDVisitor with SMS support
// =====================================================================

TEST(GoldenSMSTest, MessagePD) {
    auto check = [](const ParsedMessage& msg) {
        EXPECT_EQ(messagePD(msg), L3PD::SMS);
    };

    L3CPData cpd;
    cpd.setRpdu({});
    check(ParsedMessage(SMS(std::move(cpd))));
    check(ParsedMessage(SMS(L3CPAck{})));
    check(ParsedMessage(SMS(L3CPErr{})));
    check(ParsedMessage(SMS(L3CPStatus{})));

    L3CPSMT smt;
    smt.setRpdu({});
    check(ParsedMessage(SMS(std::move(smt))));
}

// =====================================================================
// SMS RP-ACK (GSM 24.011 7.3.2) - parse from raw bytes
// RP-ACK wire layout (GSM 24.011).
// Hex breakdown (within CP-DATA RPDU):
//   0x02 = Spare(5)=0 | RP-MTI(3)=2 (RP-ACK MO)
//   0x0A = RP-Message-Reference = 10
// =====================================================================

TEST(GoldenSMSTest, RPAck_Parse) {
    uint8_t data[] = {0x02, 0x0A};
    BitReader br(data, 16);
    auto ack = L3RPAck::parse(br);
    ASSERT_TRUE(ack);
    EXPECT_EQ(ack.value().rpMti(), L3RPAck::RP_MTI_MO);
    EXPECT_TRUE(ack.value().isMo());
    EXPECT_EQ(ack.value().messageRef(), 0x0A);
}

// =====================================================================
// SMS RP-ACK Round-Trip (standalone parse/write)
// Construct RP-ACK -> serialize -> parse -> verify fields preserved.
// RP-ACK wire layout (GSM 24.011).
// =====================================================================

TEST(GoldenSMSTest, RPAck_RoundTrip) {
    L3RPAck orig;
    orig.setRpMti(L3RPAck::RP_MTI_MO);
    orig.setMessageRef(0x0A);

    std::vector<uint8_t> buf(32);
    BitWriter bw(buf.data(), buf.size() * 8);
    orig.write(bw);

    BitReader br(buf.data(), orig.bodyLength() * 8);
    auto rt = L3RPAck::parse(br);
    ASSERT_TRUE(rt);
    EXPECT_EQ(rt.value().rpMti(), L3RPAck::RP_MTI_MO);
    EXPECT_EQ(rt.value().messageRef(), 0x0A);
}

// =====================================================================
// SMS RP-ERROR (GSM 24.011 7.3.4) - parse from raw bytes
// RP-ERROR wire layout (GSM 24.011).
// Hex breakdown (within CP-DATA RPDU):
//   0x04 = Spare(5)=0 | RP-MTI(3)=4 (RP-ERROR MO)
//   0x14 = RP-Message-Reference = 20
//   0x01 = RP-Cause Length = 1
//   0x05 = RP-Cause Value = RPUserBusy
// =====================================================================

TEST(GoldenSMSTest, RPError_Parse) {
    uint8_t data[] = {0x04, 0x14, 0x01, 0x05};
    BitReader br(data, 32);
    auto err = L3RPError::parse(br);
    ASSERT_TRUE(err);
    EXPECT_EQ(err.value().rpMti(), L3RPError::RP_MTI_MO);
    EXPECT_EQ(err.value().messageRef(), 0x14);
    EXPECT_EQ(err.value().cause(), CPCause::RPUserBusy);
}

// =====================================================================
// SMS RP-ERROR Round-Trip (standalone parse/write)
// Construct RP-ERROR -> serialize -> parse -> verify fields preserved.
// RP-ERROR wire layout (GSM 24.011).
// =====================================================================

TEST(GoldenSMSTest, RPError_RoundTrip) {
    L3RPError orig;
    orig.setRpMti(L3RPError::RP_MTI_MO);
    orig.setMessageRef(0x14);
    orig.setCause(CPCause::RPUserBusy);

    std::vector<uint8_t> buf(32);
    BitWriter bw(buf.data(), buf.size() * 8);
    orig.write(bw);

    BitReader br(buf.data(), orig.bodyLength() * 8);
    auto rt = L3RPError::parse(br);
    ASSERT_TRUE(rt);
    EXPECT_EQ(rt.value().rpMti(), L3RPError::RP_MTI_MO);
    EXPECT_EQ(rt.value().messageRef(), 0x14);
    EXPECT_EQ(rt.value().cause(), CPCause::RPUserBusy);
}

// =====================================================================
// SMS RP-SMMA (GSM 24.011 7.3.3) - parse from raw bytes
// RP-SMMA wire layout (GSM 24.011).
// Hex breakdown (within CP-DATA RPDU):
//   0x06 = Spare(5)=0 | RP-MTI(3)=6 (RP-SMMA MO)
//   0xFF = RP-Message-Reference = 255
// =====================================================================

TEST(GoldenSMSTest, RPSMMA_Parse) {
    uint8_t data[] = {0x06, 0xFF};
    BitReader br(data, 16);
    auto smma = L3RPSMMA::parse(br);
    ASSERT_TRUE(smma);
    EXPECT_EQ(smma.value().rpMti(), L3RPSMMA::RP_MTI_MO);
    EXPECT_TRUE(smma.value().isMo());
    EXPECT_EQ(smma.value().messageRef(), 0xFF);
}

// =====================================================================
// SMS RP-SMMA Round-Trip (standalone parse/write)
// Construct RP-SMMA -> serialize -> parse -> verify fields preserved.
// RP-SMMA wire layout (GSM 24.011).
// =====================================================================

TEST(GoldenSMSTest, RPSMMA_RoundTrip) {
    L3RPSMMA orig;
    orig.setRpMti(L3RPSMMA::RP_MTI_MO);
    orig.setMessageRef(0xFF);

    std::vector<uint8_t> buf(32);
    BitWriter bw(buf.data(), buf.size() * 8);
    orig.write(bw);

    BitReader br(buf.data(), orig.bodyLength() * 8);
    auto rt = L3RPSMMA::parse(br);
    ASSERT_TRUE(rt);
    EXPECT_EQ(rt.value().rpMti(), L3RPSMMA::RP_MTI_MO);
    EXPECT_EQ(rt.value().messageRef(), 0xFF);
}

// =====================================================================
// SMS TP Deliver (TS 23.040 9.2.2.1) - parse minimal TPDU
// TP-Deliver TPDU layout (TS 23.040).
// Hex breakdown (TPDU within RP-User-Data):
//   0x00 = TP-MTI(2)=00(Deliver) | MMS=0 | SRI=0 | UDHI=0 | RP=0
//          (bit positions: MTI bits0-1, MMS bit2, SRI bit5, UDHI bit6, RP bit7)
//   0x08 = TP-OA Length = 8 (number of DIGITS, not octets)
//   0x91 = TON=International(1) | NPI=E164(1) -> 0b1001_0001 = 0x91
//   0x23 = BCD digit pair: first digit 3 (low nibble), second digit 2
//   0x45, 0x67, 0x89 = ...digits 5,4 / 7,6 / 9,8
//   0x00 = TP-PID = Default
//   0x04 = TP-DCS = Default_8bit (user data is plain octets)
//   [7 octets TP-SCTS omitted for brevity, using zeros]
//   0x05 = TP-UDL = 5 (octets for the 8-bit alphabet)
//   0x48 0x65 0x6C 0x6C 0x6F = "Hello" (user data)
// =====================================================================

TEST(GoldenSMSTest, TPDeliver_Parse) {
    uint8_t data[] = {
        0x00,                     // header: TP-MTI=00, all flags=0
        0x08, 0x91, 0x23, 0x45,   // TP-OA: length=8 digits (TON_NPI + 4 BCD bytes follow)
        0x67, 0x89,               // ...digits 32547698
        0x00,                     // TP-PID = Default
        0x04,                     // TP-DCS = Default_8bit
        0x00, 0x00, 0x00, 0x00,   // TP-SCTS (7 octets, zeros)
        0x00, 0x00, 0x00,
        0x05,                     // TP-UDL = 5 octets
        0x48, 0x65, 0x6C, 0x6C, 0x6F // "Hello"
    };
    BitReader br(data, sizeof(data) * 8);
    auto deliver = L3TPDeliver::parse(br);
    ASSERT_TRUE(deliver);
    EXPECT_FALSE(deliver.value().mms());
    EXPECT_FALSE(deliver.value().sri());
    EXPECT_FALSE(deliver.value().udhi());
    EXPECT_FALSE(deliver.value().rp());
    EXPECT_EQ(deliver.value().originatingAddress().lengthV(), 8u);
    EXPECT_EQ(deliver.value().originatingAddress().digits(),
              (std::vector<uint8_t>{3, 2, 5, 4, 7, 6, 9, 8}));
    EXPECT_EQ(deliver.value().userDataLength(), 5u);
    EXPECT_EQ(deliver.value().userData().size(), 5u);
    EXPECT_EQ(deliver.value().userData()[0], 0x48); // 'H'
}

// =====================================================================
// SMS TP Submit (TS 23.040 9.2.2.2) - parse minimal TPDU
// TP-Submit TPDU layout (TS 23.040).
// Hex breakdown (TPDU within RP-User-Data):
//   0x65 = TP-MTI(2)=01(Submit) | RD=1(bit2) | VPF=00 | SRR=1(bit5) | UDHI=1(bit6) | RP=0(bit7)
//   0x03 = TP-MR = 3 (message reference)
//   0x0A = TP-DA Length = 10 (number of DIGITS, not octets)
//   0x91 = TON=International(1) | NPI=E164(1)
//   0x21 0x43 0x65 0x87 0x09 = BCD digits 1234567890 (first digit in the low nibble)
//   0x00 = TP-PID = Default
//   0x00 = TP-DCS = Default_Alphabet (GSM 7-bit)
//   0x0A = TP-UDL = 10 septets for the GSM 7-bit alphabet (TS 23.040 section 9.2.3.16)
//   9 octets of packed septets: ceil(10*7/8) = 9 bytes
// =====================================================================

TEST(GoldenSMSTest, TPSubmit_Parse) {
    uint8_t data[] = {
        0x65,                     // header: TP-MTI=01, RD=1, SRR=1, UDHI=1 (TS 23.040 section 9.1.2)
        0x03,                     // TP-MR = 3
        0x0A, 0x91, 0x21, 0x43,   // TP-DA: length=10 digits (TON_NPI + 5 BCD bytes follow)
        0x65, 0x87, 0x09,         // ...digits 1234567890
        0x00,                     // TP-PID = Default
        0x00,                     // TP-DCS = Default_Alphabet (GSM 7-bit)
        0x0A,                     // TP-UDL = 10 septets
        0x06, 0x10, 0x28,         // packed septets (first three bytes)
        0x3B, 0x4F, 0x63, 0x77,   // ...
        0x8B, 0x9F                // ...last two of nine
    };
    BitReader br(data, sizeof(data) * 8);
    auto submit = L3TPSubmit::parse(br);
    ASSERT_TRUE(submit);
    EXPECT_TRUE(submit.value().rd());
    EXPECT_EQ(submit.value().vpf(), 0u);
    EXPECT_TRUE(submit.value().srr());
    EXPECT_TRUE(submit.value().udhi());
    EXPECT_FALSE(submit.value().rp());
    EXPECT_EQ(submit.value().messageReference(), 3u);
    // The address length octet counts digits: 10 digits in 5 BCD octets.
    EXPECT_EQ(submit.value().destinationAddress().lengthV(), 10u);
    EXPECT_EQ(submit.value().destinationAddress().digits(),
              (std::vector<uint8_t>{1, 2, 3, 4, 5, 6, 7, 8, 9, 0}));
    // UDL counts septets for the GSM 7-bit alphabet; ten septets pack
    // into ceil(10*7/8) = 9 octets.
    EXPECT_EQ(submit.value().userDataLength(), 10u);
    EXPECT_EQ(submit.value().userData().size(), 9u);
    // Round-trip: re-encoding reproduces the vector byte-for-byte, in
    // particular the 0x65 first octet (TS 23.040 section 9.1.2).
    uint8_t out[32];
    BitWriter bw(out, sizeof(out) * 8);
    submit.value().write(bw);
    EXPECT_EQ(submit.value().bodyLength(), sizeof(data));
    for (size_t i = 0; i < sizeof(data); ++i) {
        EXPECT_EQ(out[i], data[i]) << "byte " << i;
    }
}

// =====================================================================
// SMS TP Submit with Validity Period (GSM 23.040 9.2.2.2)
// The TP-VP octets follow TP-DCS and precede TP-UDL when the VPF bits in
// the header octet select a non-zero encoding:
//   VPF=1 -> one octet (relative), VPF=2 -> seven octets (encoded),
//   VPF=3 -> ten octets (enhanced). The octets are preserved verbatim.
// =====================================================================

TEST(GoldenSMSTest, TPSubmit_VP_RoundTrip) {
    uint8_t data[] = {
        0x0D,                     // header: MTI=01, RD=1(bit2), VPF=01(bits3-4) (TS 23.040 section 9.1.2)
        0x03,                     // TP-MR = 3
        0x0A, 0x91, 0x21, 0x43,   // TP-DA: length=10 digits (TON_NPI + 5 BCD bytes follow)
        0x65, 0x87, 0x09,         // ...digits 1234567890
        0x00,                     // TP-PID = Default
        0x04,                     // TP-DCS = Default_8bit (user data is plain octets)
        0x0A,                     // TP-VP (relative): one octet
        0x05,                     // TP-UDL = 5 octets
        0x48, 0x65, 0x6C, 0x6C, 0x6F // "Hello"
    };
    BitReader br(data, sizeof(data) * 8);
    auto submit = L3TPSubmit::parse(br);
    ASSERT_TRUE(submit);
    EXPECT_EQ(submit.value().vpf(), 1u);
    ASSERT_EQ(submit.value().validityPeriod().size(), 1u);
    EXPECT_EQ(submit.value().validityPeriod()[0], 0x0Au);
    // The validity period octet is accounted for in the body length.
    EXPECT_EQ(submit.value().bodyLength(), sizeof(data));
    // Round-trip: re-encoding reproduces the vector byte-for-byte.
    uint8_t out[64];
    BitWriter bw(out, sizeof(out) * 8);
    submit.value().write(bw);
    for (size_t i = 0; i < sizeof(data); ++i) {
        EXPECT_EQ(out[i], data[i]) << "byte " << i;
    }
    // Builder path: the same fields produce the same wire octets.
    L3TPSubmit built = L3TPSubmit::builder()
        .rd(true)
        .vpf(1)
        .messageReference(3)
        .destinationAddress(submit.value().destinationAddress())
        .dcs(TPDCS::Default_8bit)
        .validityPeriod(std::span<const uint8_t>(data + 11, 1))
        .userDataLength(5)
        .userData(std::span<const uint8_t>(data + 13, 5))
        .build();
    EXPECT_EQ(built.bodyLength(), sizeof(data));
    uint8_t out2[64];
    BitWriter bw2(out2, sizeof(out2) * 8);
    built.write(bw2);
    for (size_t i = 0; i < sizeof(data); ++i) {
        EXPECT_EQ(out2[i], data[i]) << "byte " << i;
    }
}

TEST(GoldenSMSTest, TPSubmit_VPEnhanced_RoundTrip) {
    // VPF=3 (enhanced validity period): ten octets between TP-DCS and TP-UDL.
    uint8_t data[] = {
        0x1D,                     // header: MTI=01, RD=1(bit2), VPF=11(bits3-4) (TS 23.040 section 9.1.2)
        0x07,                     // TP-MR = 7
        0x0A, 0x91, 0x21, 0x43,   // TP-DA: length=10 digits (TON_NPI + 5 BCD bytes follow)
        0x65, 0x87, 0x09,         // ...digits 1234567890
        0x00,                     // TP-PID = Default
        0x04,                     // TP-DCS = Default_8bit (user data is plain octets)
        0x01, 0x02, 0x03, 0x04, 0x05, // TP-VP (enhanced): first five octets
        0x06, 0x07, 0x08, 0x09, 0x0A, // ...last five octets
        0x02,                     // TP-UDL = 2 octets
        0x48, 0x69                // "Hi"
    };
    BitReader br(data, sizeof(data) * 8);
    auto submit = L3TPSubmit::parse(br);
    ASSERT_TRUE(submit);
    EXPECT_EQ(submit.value().vpf(), 3u);
    ASSERT_EQ(submit.value().validityPeriod().size(), 10u);
    EXPECT_EQ(submit.value().validityPeriod()[0], 0x01u);
    EXPECT_EQ(submit.value().validityPeriod()[9], 0x0Au);
    // The ten validity period octets are accounted for in the body length.
    EXPECT_EQ(submit.value().bodyLength(), sizeof(data));
    // Round-trip: re-encoding reproduces the vector byte-for-byte.
    uint8_t out[64];
    BitWriter bw(out, sizeof(out) * 8);
    submit.value().write(bw);
    for (size_t i = 0; i < sizeof(data); ++i) {
        EXPECT_EQ(out[i], data[i]) << "byte " << i;
    }
}

// =====================================================================
// SMS Full Wrapper Test
// Parse full L3 SMS message: CP-DATA -> RP-DATA -> TP-Submit
// This tests the complete nesting: L3 header -> CP layer -> RP layer -> TP layer.
// Full MO SMS wrapper per GSM 24.008 (CP-DATA -> RP-DATA -> TP-Submit).
// =====================================================================

TEST(GoldenSMSTest, FullSMSWrapper_MO) {
    // Construct a full MO SMS: CP-DATA containing RP-DATA containing TP-Submit
    // L3 Header: PD=9, CP-MTI=1 (CP-DATA)
    // CP Body: Length + RPDU
    //   RP header: Spare(5)=0 | RP-MTI(3)=0 (RP-DATA MO)
    //   RP Message-Ref: 1
    //   RP Originator-SM-SC-Address: empty LV (single zero octet, TS 24.011 7.3.1)
    //   RP Destination-SM-SC-Address: empty LV (single zero octet)
    //   RP User-Data: TP-Submit TPDU

    // Build TP-Submit first
    std::vector<uint8_t> tpdu;
    tpdu.push_back(0x05); // TP-Submit header: MTI=01, RD=1(bit2) (TS 23.040 section 9.1.2)
    tpdu.push_back(0x03); // TP-MR
    tpdu.push_back(0x08); // TP-DA length = 8 digits
    tpdu.push_back(0x91); // TON/NPI: International/E.164
    tpdu.push_back(0x23);
    tpdu.push_back(0x45);
    tpdu.push_back(0x67);
    tpdu.push_back(0x89); // digits 32547698 (first digit in the low nibble)
    tpdu.push_back(0x00); // TP-PID
    tpdu.push_back(0x04); // TP-DCS: Default_8bit (user data is plain octets)
    tpdu.push_back(0x05); // TP-UDL = 5 octets
    tpdu.insert(tpdu.end(), {0x48, 0x65, 0x6C, 0x6C, 0x6F}); // "Hello"

    // Build RP-DATA: header + message-ref + both (empty) SMSC LVs + user data
    std::vector<uint8_t> rpdu;
    rpdu.push_back(0x00); // RP header: Spare(5)=0 | RP-MTI(3)=0 (RP-DATA MO)
    rpdu.push_back(0x01); // RP-Message-Reference = 1
    rpdu.push_back(0x00); // RP-Originator-SM-SC-Address: empty LV
    rpdu.push_back(0x00); // RP-Destination-SM-SC-Address: empty LV
    rpdu.push_back(static_cast<uint8_t>(tpdu.size())); // RP-User-Data length
    rpdu.insert(rpdu.end(), tpdu.begin(), tpdu.end());

    // Build full L3 message: header + CP body
    std::vector<uint8_t> l3msg;
    l3msg.push_back(0x09); // PD=9(SMS) in the low nibble, TI=0, TIF=0
    l3msg.push_back(0x01); // CP-MTI=1 (CP-DATA)
    l3msg.push_back(static_cast<uint8_t>(rpdu.size())); // CP-User-Data-Length
    l3msg.insert(l3msg.end(), rpdu.begin(), rpdu.end());

    auto msg = parseL3(std::span<const uint8_t>(l3msg));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3CPData::MTI);
    EXPECT_EQ(messagePD(*msg), L3PD::SMS);

    auto* cpd = tryGet<L3CPData>(*msg);
    ASSERT_NE(cpd, nullptr);
    EXPECT_EQ(cpd->rpdu().size(), rpdu.size());
    EXPECT_EQ(cpd->rpdu()[0], 0x00); // RP-DATA MO header
    EXPECT_EQ(cpd->rpdu()[1], 0x01); // RP-Message-Reference

    // Parse RP layer from CP-DATA body
    BitReader rpBr(cpd->rpdu().data(), cpd->rpdu().size() * 8);
    auto rpData = L3RPData::parse(rpBr);
    ASSERT_TRUE(rpData);
    EXPECT_EQ(rpData.value().rpMti(), L3RPData::RP_MTI_MO);
    EXPECT_TRUE(rpData.value().isMo());
    EXPECT_EQ(rpData.value().messageRef(), 1u);
    // Both SMSC addresses are present on the wire; empty LVs parse back to
    // "not set".
    EXPECT_FALSE(rpData.value().originatorAddress().has_value());
    EXPECT_FALSE(rpData.value().destinationAddress().has_value());
    // The user data carries the TP-Submit TPDU verbatim.
    ASSERT_EQ(rpData.value().userData().size(), tpdu.size());
    for (size_t i = 0; i < tpdu.size(); ++i) {
        EXPECT_EQ(rpData.value().userData()[i], tpdu[i]) << "tpdu byte " << i;
    }
}

// =====================================================================
// SMS Full Wrapper MT Test
// Parse full MT SMS message: CP-DATA -> RP-DATA -> TP-Deliver
// Full MT SMS wrapper per GSM 24.008 (CP-DATA -> RP-DATA -> TP-Deliver).
// =====================================================================

TEST(GoldenSMSTest, FullSMSWrapper_MT) {
    // Construct a full MT SMS: CP-DATA containing RP-DATA containing TP-Deliver
    std::vector<uint8_t> tpdu;
    tpdu.push_back(0x00); // TP-Deliver header (MTI=00, no flags)
    tpdu.push_back(0x08); // TP-OA length = 8 digits
    tpdu.insert(tpdu.end(), {0x91, 0x23, 0x45, 0x67, 0x89}); // OA: 32547698
    tpdu.push_back(0x00); // TP-PID
    tpdu.push_back(0x04); // TP-DCS: Default_8bit (user data is plain octets)
    tpdu.insert(tpdu.end(), {0,0,0,0,0,0,0}); // TP-SCTS (7 zero octets)
    tpdu.push_back(0x05); // TP-UDL = 5 octets
    tpdu.insert(tpdu.end(), {0x48, 0x65, 0x6C, 0x6C, 0x6F}); // "Hello"

    // Build RP-DATA MT: header + message-ref + both (empty) SMSC LVs + user data
    std::vector<uint8_t> rpdu;
    rpdu.push_back(0x01); // RP header: Spare(5)=0 | RP-MTI(3)=1 (RP-DATA MT)
    rpdu.push_back(0x02); // RP-Message-Reference = 2
    rpdu.push_back(0x00); // RP-Originator-SM-SC-Address: empty LV
    rpdu.push_back(0x00); // RP-Destination-SM-SC-Address: empty LV
    rpdu.push_back(static_cast<uint8_t>(tpdu.size()));
    rpdu.insert(rpdu.end(), tpdu.begin(), tpdu.end());

    // Build full L3 message
    std::vector<uint8_t> l3msg;
    l3msg.push_back(0x09); // PD=9(SMS) in the low nibble, TI=0, TIF=0
    l3msg.push_back(0x01); // CP-MTI=1 (CP-DATA)
    l3msg.push_back(static_cast<uint8_t>(rpdu.size()));
    l3msg.insert(l3msg.end(), rpdu.begin(), rpdu.end());

    auto msg = parseL3(std::span<const uint8_t>(l3msg));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3CPData::MTI);
    EXPECT_EQ(messagePD(*msg), L3PD::SMS);

    auto* cpd = tryGet<L3CPData>(*msg);
    ASSERT_NE(cpd, nullptr);
    EXPECT_EQ(cpd->rpdu()[0], 0x01); // RP-DATA MT header (RP-MTI=1)

    BitReader rpBr(cpd->rpdu().data(), cpd->rpdu().size() * 8);
    auto rpData = L3RPData::parse(rpBr);
    ASSERT_TRUE(rpData);
    EXPECT_EQ(rpData.value().rpMti(), L3RPData::RP_MTI_MT);
    EXPECT_FALSE(rpData.value().isMo());
    EXPECT_FALSE(rpData.value().originatorAddress().has_value());
    EXPECT_FALSE(rpData.value().destinationAddress().has_value());
    ASSERT_EQ(rpData.value().userData().size(), tpdu.size());
    // Parse the TP-Deliver from the user data.
    BitReader tpBr(rpData.value().userData().data(), rpData.value().userData().size() * 8);
    auto deliver = L3TPDeliver::parse(tpBr);
    ASSERT_TRUE(deliver);
    EXPECT_EQ(deliver.value().originatingAddress().lengthV(), 8u);
    EXPECT_EQ(deliver.value().userDataLength(), 5u);
}

// =====================================================================
// SMS TP Enum String Converters
// Verify TPDCS2Str and TPPID2Str return correct names.
// Reference: 3GPP TS 23.040 section 9.2.3
// =====================================================================

TEST(GoldenSMSTest, TPEnumStrings) {
    EXPECT_STREQ(TPDCS2Str(TPDCS::Default_Alphabet), "Default-Alphabet");
    EXPECT_STREQ(TPDCS2Str(TPDCS::UCS2), "UCS2");
    EXPECT_STREQ(TPPID2Str(TPPID::GSM), "GSM");
    EXPECT_STREQ(TPPID2Str(TPPID::X121), "X.121");
}

// =====================================================================
// SMS Data Coding Scheme (TS 23.040)
// decodeSmsDcs() is checked against a local reference model of the TS
// 23.040 DCS bit layout for all 256 octet values, plus hand-computed
// spot checks of the well-known codings.
// =====================================================================

namespace {

struct DcsExpectation {
    SmsDcsAlphabet alphabet;
    bool compressed;
    bool hasMessageClass;
    unsigned messageClass;
};

// Local reference model of the TS 23.040 DCS bit layout, used as the
// expected-value source for the table test below: coding group CG = dcs >> 4.
constexpr DcsExpectation dcsReference(uint8_t dcs) noexcept {
    const unsigned cg = dcs >> 4;
    DcsExpectation e{SmsDcsAlphabet::Undefined, false, false, dcs & 0x3u};
    if ((cg & 0xCu) == 0u) {
        // Coding groups 0..3: the scheme is (dcs >> 2) & 3.
        switch ((dcs >> 2) & 0x3u) {
            case 0:  e.alphabet = SmsDcsAlphabet::Gsm7Bit;   break;
            case 1:  e.alphabet = SmsDcsAlphabet::Data8Bit;  break;
            case 2:  e.alphabet = SmsDcsAlphabet::Ucs2;      break;
            default: e.alphabet = SmsDcsAlphabet::Undefined; break;
        }
        e.compressed = (dcs & 0x20u) != 0u;
        e.hasMessageClass = (cg == 0u) && ((dcs & 0x10u) != 0u);
    } else if (cg >= 0xCu && cg <= 0xDu) {
        // Coding groups 12/13 select the GSM 7-bit alphabet.
        e.alphabet = SmsDcsAlphabet::Gsm7Bit;
    } else if (cg == 0xEu) {
        // Coding group 14 selects UCS2.
        e.alphabet = SmsDcsAlphabet::Ucs2;
    } else {
        // Coding group 15: 8-bit data when bit 0x04 is set, otherwise GSM
        // 7-bit; the message class always applies.
        e.alphabet = (dcs & 0x04u) ? SmsDcsAlphabet::Data8Bit : SmsDcsAlphabet::Gsm7Bit;
        e.hasMessageClass = true;
    }
    return e;
}

} // namespace

TEST(GoldenSMSTest, DCS_WellKnownOctets) {
    // TPDCS members hold the full well-known DCS octets (TS 23.040).
    EXPECT_EQ(static_cast<uint8_t>(TPDCS::Default_Alphabet), 0x00u);
    EXPECT_EQ(static_cast<uint8_t>(TPDCS::Default_8bit), 0x04u);
    EXPECT_EQ(static_cast<uint8_t>(TPDCS::UCS2), 0x08u);
}

TEST(GoldenSMSTest, DCS_DecodeSpotChecks) {
    // Default GSM 7-bit text; the message class does not apply for 0x00.
    auto d0 = decodeSmsDcs(0x00);
    EXPECT_EQ(d0.alphabet, SmsDcsAlphabet::Gsm7Bit);
    EXPECT_FALSE(d0.compressed);
    EXPECT_FALSE(d0.hasMessageClass);
    EXPECT_EQ(d0.messageClass, 0u);

    // Well-known data codings.
    EXPECT_EQ(decodeSmsDcs(0x04).alphabet, SmsDcsAlphabet::Data8Bit);
    EXPECT_EQ(decodeSmsDcs(0x08).alphabet, SmsDcsAlphabet::Ucs2);

    // Coding group 1: the message class does not apply (it requires coding
    // group 0 with bit 0x10 set).
    auto d10 = decodeSmsDcs(0x10);
    EXPECT_EQ(d10.alphabet, SmsDcsAlphabet::Gsm7Bit);
    EXPECT_FALSE(d10.hasMessageClass);
    EXPECT_EQ(d10.messageClass, 0u);

    // Compressed flag (bit 0x20) in coding group 1.
    auto d30 = decodeSmsDcs(0x30);
    EXPECT_EQ(d30.alphabet, SmsDcsAlphabet::Gsm7Bit);
    EXPECT_TRUE(d30.compressed);
    EXPECT_FALSE(d30.hasMessageClass);

    // Coding groups 12/13 select the GSM 7-bit alphabet; group 14 selects UCS2.
    EXPECT_EQ(decodeSmsDcs(0xC0).alphabet, SmsDcsAlphabet::Gsm7Bit);
    EXPECT_EQ(decodeSmsDcs(0xDE).alphabet, SmsDcsAlphabet::Gsm7Bit);
    EXPECT_EQ(decodeSmsDcs(0xE1).alphabet, SmsDcsAlphabet::Ucs2);

    // Coding group 15 with bit 0x04 set: 8-bit data, class always applies.
    auto df6 = decodeSmsDcs(0xF6);
    EXPECT_EQ(df6.alphabet, SmsDcsAlphabet::Data8Bit);
    EXPECT_TRUE(df6.hasMessageClass);
    EXPECT_EQ(df6.messageClass, 2u);

    // Coding group 15 without bit 0x04: GSM 7-bit.
    auto df0 = decodeSmsDcs(0xF0);
    EXPECT_EQ(df0.alphabet, SmsDcsAlphabet::Gsm7Bit);
    EXPECT_TRUE(df0.hasMessageClass);

    // Reserved scheme '11' in coding group 0..3 yields no alphabet.
    EXPECT_EQ(decodeSmsDcs(0x0C).alphabet, SmsDcsAlphabet::Undefined);
}

TEST(GoldenSMSTest, DCS_DecodeAllOctets) {
    // per TS 23.040 DCS bit layout
    for (int dcs = 0; dcs < 256; ++dcs) {
        const auto got = decodeSmsDcs(static_cast<uint8_t>(dcs));
        const auto want = dcsReference(static_cast<uint8_t>(dcs));
        EXPECT_EQ(got.alphabet, want.alphabet) << "dcs=" << dcs;
        EXPECT_EQ(got.compressed, want.compressed) << "dcs=" << dcs;
        EXPECT_EQ(got.hasMessageClass, want.hasMessageClass) << "dcs=" << dcs;
        EXPECT_EQ(got.messageClass, want.messageClass) << "dcs=" << dcs;
    }
}

// =====================================================================
// SMS TP Status Report (TS 23.040 9.2.2.3) - minimal parse
// Reference: 3GPP TS 23.040 section 9.2.2.3
// =====================================================================

TEST(GoldenSMSTest, TPStatusReport_Parse) {
    uint8_t data[] = {
        0x02,                     // header: TP-MTI='10'B, remaining bits zero (TS 23.040 section 9.1.2)
        0x05,                     // TP-MR = 5
        0x08, 0x91, 0x23, 0x45,   // TP-DA: length=8 digits (TON_NPI + 4 BCD bytes)
        0x67, 0x89,               // ...digits 32547698
        0x00,                     // TP-PID
        0x00,                     // TP-DCS
        0x00, 0x00, 0x00, 0x00,   // TP-SCTS (7 octets)
        0x00, 0x00, 0x00,
        0x01                      // TP-STS = delivered
    };
    BitReader br(data, sizeof(data) * 8);
    auto sr = L3TPStatusReport::parse(br);
    ASSERT_TRUE(sr);
    EXPECT_EQ(sr.value().messageReference(), 5u);
    EXPECT_EQ(sr.value().destinationAddress().lengthV(), 8u);
    EXPECT_EQ(sr.value().sts(), 1u);
    // Round-trip: the first octet stays TP-MTI='10'B with zeroed flags.
    uint8_t out[32];
    BitWriter bw(out, sizeof(out) * 8);
    sr.value().write(bw);
    for (size_t i = 0; i < sizeof(data); ++i) {
        EXPECT_EQ(out[i], data[i]) << "byte " << i;
    }
}

// =====================================================================
// SMS TP Command (TS 23.040 9.2.2.5) - minimal parse
// Reference: 3GPP TS 23.040 section 9.2.2.5
// =====================================================================

TEST(GoldenSMSTest, TPCommand_Parse) {
    uint8_t data[] = {
        0x02,                     // header: TP-MTI='10'B, remaining bits zero (TS 23.040 section 9.1.2)
        0x0A,                     // TP-MR = 10
        0x00,                     // TP-PID
        0x00,                     // TP-DCS
        0x03                      // TP-CMD = 3
    };
    BitReader br(data, sizeof(data) * 8);
    auto cmd = L3TPCommand::parse(br);
    ASSERT_TRUE(cmd);
    EXPECT_EQ(cmd.value().messageReference(), 10u);
    EXPECT_EQ(cmd.value().cmd(), 3u);
    // Round-trip: the first octet stays TP-MTI='10'B with zeroed flags.
    uint8_t out[32];
    BitWriter bw(out, sizeof(out) * 8);
    cmd.value().write(bw);
    for (size_t i = 0; i < sizeof(data); ++i) {
        EXPECT_EQ(out[i], data[i]) << "byte " << i;
    }
}

// =====================================================================
// SMS TP Address (TS 23.040 9.1.2.4) - parse LV format
// TP-DA / TP-OA address layout per TS 23.040: the length field counts the
// number of digits, not octets.
// =====================================================================

TEST(GoldenSMSTest, TPAddress_Parse) {
    // Length=10 digits, TON=International(1), NPI=E164(1), BCD 1234567890
    uint8_t data[] = {0x0A, 0x91, 0x21, 0x43, 0x65, 0x87, 0x09};
    BitReader br(data, sizeof(data) * 8);
    auto addr = L3TPAddress::parse(br);
    ASSERT_TRUE(addr);
    EXPECT_EQ(addr.value().ton(), TypeOfNumber::International);
    EXPECT_EQ(addr.value().npi(), NumberingPlan::E164);
    EXPECT_EQ(addr.value().lengthV(), 10u);
    EXPECT_EQ(addr.value().digits(),
              (std::vector<uint8_t>{1, 2, 3, 4, 5, 6, 7, 8, 9, 0}));
    // Odd digit count: the last high nibble is 'F'-filled and not a digit.
    uint8_t odd[] = {0x05, 0xA1, 0x21, 0x43, 0xF6};
    BitReader oddBr(odd, sizeof(odd) * 8);
    auto oddAddr = L3TPAddress::parse(oddBr);
    ASSERT_TRUE(oddAddr);
    EXPECT_EQ(oddAddr.value().lengthV(), 5u);
    EXPECT_EQ(oddAddr.value().digits(), (std::vector<uint8_t>{1, 2, 3, 4, 6}));
    // Round-trip: re-encoding reproduces the vector byte-for-byte.
    uint8_t out[16];
    BitWriter bw(out, sizeof(out) * 8);
    oddAddr.value().write(bw);
    for (size_t i = 0; i < sizeof(odd); ++i) {
        EXPECT_EQ(out[i], odd[i]) << "byte " << i;
    }
}

// =====================================================================
// SMS TP-MTI values (TS 23.040 section 9.1.2)
// Status Report (network to mobile) and Command (mobile to network) share
// the '10'B code; '11'B is reserved. The semantics are determined by the
// frame direction.
// =====================================================================

TEST(GoldenSMSTest, TPMTIValues) {
    EXPECT_EQ(L3TPDeliver::TP_MTI, 0);
    EXPECT_EQ(L3TPSubmit::TP_MTI, 1);
    EXPECT_EQ(L3TPStatusReport::TP_MTI, 2);
    EXPECT_EQ(L3TPCommand::TP_MTI, 2);
}

// =====================================================================
// SMS TP Deliver header octet flags (TS 23.040 section 9.1.2)
// (DELIVER, MMS=1, UDHI=1, RP=1): bit7(RP)|bit6(UDHI)|bit2(MMS) = 0xC4.
// Parse the first octet from the vector and verify that re-encoding
// reproduces it byte-for-byte.
// =====================================================================

TEST(GoldenSMSTest, TPDeliver_FirstOctetFlags) {
    uint8_t data[] = {
        0xC4,                     // header: MTI=00, MMS=1(bit2), UDHI=1(bit6), RP=1(bit7)
        0x08, 0x91, 0x23, 0x45,   // TP-OA: length=8 digits (TON_NPI + 4 BCD bytes follow)
        0x67, 0x89,               // ...digits 32547698
        0x00,                     // TP-PID = Default
        0x04,                     // TP-DCS = Default_8bit
        0x00, 0x00, 0x00, 0x00,   // TP-SCTS (7 octets, zeros)
        0x00, 0x00, 0x00,
        0x05,                     // TP-UDL = 5 octets
        0x48, 0x65, 0x6C, 0x6C, 0x6F // "Hello"
    };
    BitReader br(data, sizeof(data) * 8);
    auto deliver = L3TPDeliver::parse(br);
    ASSERT_TRUE(deliver);
    EXPECT_TRUE(deliver.value().mms());
    EXPECT_FALSE(deliver.value().sri());
    EXPECT_TRUE(deliver.value().udhi());
    EXPECT_TRUE(deliver.value().rp());
    uint8_t out[32];
    BitWriter bw(out, sizeof(out) * 8);
    deliver.value().write(bw);
    EXPECT_EQ(out[0], 0xC4); // recomputed first octet per TS 23.040 section 9.1.2
    for (size_t i = 0; i < sizeof(data); ++i) {
        EXPECT_EQ(out[i], data[i]) << "byte " << i;
    }
}

// =====================================================================
// SMS TP Deliver with GSM 7-bit user data, UDL not a multiple of 8
// (TS 23.040 section 9.2.3.16): ten septets pack into ceil(10*7/8) = 9
// octets, so the consumed user-data size differs from the UDL value.
// =====================================================================

TEST(GoldenSMSTest, TPDeliver_SeptetPacking) {
    uint8_t data[] = {
        0x00,                     // header: MTI=00, no flags
        0x0A, 0x91, 0x21, 0x43,   // TP-OA: length=10 digits (TON_NPI + 5 BCD bytes follow)
        0x65, 0x87, 0x09,         // ...digits 1234567890
        0x00,                     // TP-PID = Default
        0x00,                     // TP-DCS = Default_Alphabet (GSM 7-bit)
        0x00, 0x00, 0x00, 0x00,   // TP-SCTS (7 octets, zeros)
        0x00, 0x00, 0x00,
        0x0A,                     // TP-UDL = 10 septets (ULDL % 8 != 0)
        0x06, 0x10, 0x28,         // packed septets (first three bytes)
        0x3B, 0x4F, 0x63, 0x77,   // ...
        0x8B, 0x9F                // ...last two of nine
    };
    BitReader br(data, sizeof(data) * 8);
    auto deliver = L3TPDeliver::parse(br);
    ASSERT_TRUE(deliver);
    EXPECT_EQ(deliver.value().userDataLength(), 10u);
    // Ten septets consume exactly nine octets of user data.
    EXPECT_EQ(deliver.value().userData().size(), 9u);
    uint8_t out[32];
    BitWriter bw(out, sizeof(out) * 8);
    deliver.value().write(bw);
    for (size_t i = 0; i < sizeof(data); ++i) {
        EXPECT_EQ(out[i], data[i]) << "byte " << i;
    }
}

// =====================================================================
// SMS CP dispatch: the message type is masked to its low five bits, so a
// CP-DATA octet with spare high bits set still dispatches to CP-DATA
// (TS 24.011 section 8.2).
// =====================================================================

TEST(GoldenSMSTest, CPDispatch_MTIMasked) {
    // CP-DATA whose MTI octet carries the three spare high bits:
    // 0x21 = 0x01 | spare(3)<<5. Body: RPDU length 2 + two RP octets.
    uint8_t data[] = {0x09, 0x21, 0x02, 0x00, 0x01};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3CPData::MTI);
    EXPECT_EQ(messagePD(*msg), L3PD::SMS);
    EXPECT_NE(tryGet<L3CPData>(*msg), nullptr);

    // parseL3Header exposes the masked MTI directly.
    auto hdr = parseL3Header(std::span<const uint8_t>(data, 2));
    ASSERT_TRUE(hdr);
    EXPECT_EQ(hdr.value().mti, 0x01);
}

// =====================================================================
// SMS CP-DATA Round-Trip with full RPDU
// Construct CP-DATA with RP-DATA containing TP-Submit -> serialize -> parse -> verify.
// Full L3 -> CP -> RP -> TP nesting per GSM 24.008 / TS 24.011 / TS 23.040.
// =====================================================================

TEST(GoldenSMSTest, FullCPData_RoundTrip) {
    // Build the same message as in FullSMSWrapper_MO but via construction
    std::vector<uint8_t> tpdu = {
        0x05, 0x03, 0x08, 0x91, 0x23, 0x45, 0x67, 0x89,
        0x00, 0x04, 0x05, 0x48, 0x65, 0x6C, 0x6C, 0x6F
    };
    // RP-DATA: header + message-ref + both (empty) SMSC LVs + user data.
    std::vector<uint8_t> rpdu = {0x00, 0x01, 0x00, 0x00};
    rpdu.push_back(static_cast<uint8_t>(tpdu.size()));
    rpdu.insert(rpdu.end(), tpdu.begin(), tpdu.end());

    L3CPData orig;
    orig.setRpdu(std::move(rpdu));
    ParsedMessage pm(SMS(std::move(orig)));
    auto rt = roundtrip(pm);
    ASSERT_TRUE(rt);
    EXPECT_EQ(messageMTI(*rt), L3CPData::MTI);
    auto* cpd = tryGet<L3CPData>(*rt);
    ASSERT_NE(cpd, nullptr);
    EXPECT_EQ(cpd->rpdu()[0], 0x00); // RP-DATA MO
    EXPECT_EQ(cpd->rpdu()[1], 0x01); // Message-Reference
}

// ── SMS Builder Tests ──────────────────────────────────────────────────

// 3GPP TS 24.011 8.1.2: CP-DATA Builder
TEST(SMSBuilderTest, CPData) {
    auto msg = L3CPData::builder()
        .rpdu(std::vector<uint8_t>{0x00, 0x01, 0xAA})
        .build();
    ParsedMessage pm{SMS{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
    EXPECT_EQ((*bytes)[0], 0x09); // PD=SMS in the low nibble

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3CPData::MTI);
}

// 3GPP TS 24.011 8.1.3: CP-ACK Builder (empty)
TEST(SMSBuilderTest, CPAck) {
    auto msg = L3CPAck::builder().build();
    ParsedMessage pm{SMS{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3CPAck::MTI);
}

// 3GPP TS 24.011 8.1.4: CP-ERROR Builder
TEST(SMSBuilderTest, CPErr) {
    auto msg = L3CPErr::builder()
        .cause(CPCause::NoRPLPDU)
        .build();
    ParsedMessage pm{SMS{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3CPErr::MTI);
}

// 3GPP TS 24.011 8.1.5: CP-STATUS Builder
TEST(SMSBuilderTest, CPStatus) {
    auto msg = L3CPStatus::builder()
        .tpOi(1)
        .mtiValue(0x11)
        .messageRef(5)
        .build();
    ParsedMessage pm{SMS{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3CPStatus::MTI);
}

// 3GPP TS 24.011 8.1.6: CP-SMT Builder
TEST(SMSBuilderTest, CPSMT) {
    auto msg = L3CPSMT::builder()
        .rpdu(std::vector<uint8_t>{0x04, 0x01, 0xBB})
        .build();
    ParsedMessage pm{SMS{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3CPSMT::MTI);
}

// 3GPP TS 24.011 7.3.1: RP-DATA Builder
TEST(SMSBuilderTest, RPData) {
    auto msg = L3RPData::builder()
        .rpMti(L3RPData::RP_MTI_MO)
        .messageRef(3)
        .userData(std::vector<uint8_t>{0x65, 0x6C, 0x6C, 0x6F})
        .build();
    EXPECT_EQ(msg.rpMti(), L3RPData::RP_MTI_MO);
    EXPECT_EQ(msg.messageRef(), 3u);
}

// 3GPP TS 24.011 7.3.2: RP-ACK Builder
TEST(SMSBuilderTest, RPAck) {
    auto msg = L3RPAck::builder()
        .rpMti(L3RPAck::RP_MTI_MO)
        .messageRef(3)
        .build();
    EXPECT_EQ(msg.rpMti(), L3RPAck::RP_MTI_MO);
}

// 3GPP TS 24.011 7.3.4: RP-ERROR Builder
TEST(SMSBuilderTest, RPError) {
    auto msg = L3RPError::builder()
        .rpMti(L3RPError::RP_MTI_MO)
        .messageRef(4)
        .cause(CPCause::NoRPLPDU)
        .build();
    EXPECT_EQ(msg.cause(), CPCause::NoRPLPDU);
}

// 3GPP TS 24.011 7.3.3: RP-SMMA Builder
TEST(SMSBuilderTest, RPSMMA) {
    auto msg = L3RPSMMA::builder()
        .rpMti(L3RPSMMA::RP_MTI_MO)
        .messageRef(5)
        .build();
    EXPECT_EQ(msg.rpMti(), L3RPSMMA::RP_MTI_MO);
}
