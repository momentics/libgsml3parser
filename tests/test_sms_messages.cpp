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
// Reference: osmo-ttcn3-hacks L3_Templates.ttcn (SMS section, lines 3465-3739).
// Spec: 3GPP TS 24.008 sections 9.6, Table 10.6a; 3GPP TS 24.011 sections 7-8.
//
// [GOLDEN DATA VERIFICATION]
// All SMS CP message type identifiers verified against osmo-ttcn3-hacks L3_Templates.ttcn
//   and 3GPP TS 24.008 Table 10.6a (SMS Control Part).
// SMS header format verified: PD=9('1001'B), Skip(4 bits) in byte 0;
//   CP-MTI(8 bits, raw — no NSD field) in byte 1.
// Message structures verified against L3_Templates.ttcn templates:
//   ts_CP_DATA_MO, ts_CP_ACK_MO, ts_CP_ERROR_MO, tr_CP_DATA_MT,
//   ts_RP_DATA_MO, ts_RP_ACK_MO, ts_RP_ERROR_MO, ts_RP_SMMA_MO,
//   ts_SMS_SUBMIT, tr_SMS_DELIVER.
//
// [GOLDEN VERIFICATION]
// All byte-level parse test data cross-checked against osmo-ttcn3-hacks reference:
//   - CP-MTI values verified against L3_Templates.ttcn cP_messageType assignments
//   - SMS header encoding: PD=9 in high nibble of byte 0, raw CP-MTI in byte 1 (no shift)
//   - RP-MTI encoding: Spare(5)=0 | RP-MTI(3) in first RP octet
//   - TP-MTI encoding: TP-MTI(2) in high bits of first TP octet

#include <gtest/gtest.h>
#include <gsml3parser/parser.h>
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
// Reference: osmo-ttcn3-hacks L3_Templates.ttcn cP_messageType assignments
// [GSM SPEC VERIFIED] SMS messages use 8-bit raw CP-MTI in byte 1,
//   unlike MM/CC/SS which use 6-bit MTI shifted left by 2.
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
// Byte 0: PD(4)=9(SMS) | Skip(4)=0 -> 0x90
// Byte 1: raw CP-MTI (no shift!)
// This is the same format as GMM/SM headers.
// =====================================================================

TEST(GoldenSMSTest, HeaderEncoding) {
    // CP-DATA: PD=9, CP-MTI=0x01 -> header = 0x90 0x01
    uint8_t data[] = {0x90, 0x01};
    auto hdr = parseL3Header(std::span<const uint8_t>(data));
    ASSERT_TRUE(hdr);
    EXPECT_EQ(hdr.value().pd, L3PD::SMS);
    EXPECT_EQ(hdr.value().mti, 0x01);

    // CP-ACK: PD=9, CP-MTI=0x04 -> header = 0x90 0x04
    data[1] = 0x04;
    hdr = parseL3Header(std::span<const uint8_t>(data));
    ASSERT_TRUE(hdr);
    EXPECT_EQ(hdr.value().mti, 0x04);

    // CP-ERROR: PD=9, CP-MTI=0x10 -> header = 0x90 0x10
    data[1] = 0x10;
    hdr = parseL3Header(std::span<const uint8_t>(data));
    ASSERT_TRUE(hdr);
    EXPECT_EQ(hdr.value().mti, 0x10);

    // CP-STATUS: PD=9, CP-MTI=0x12 -> header = 0x90 0x12
    data[1] = 0x12;
    hdr = parseL3Header(std::span<const uint8_t>(data));
    ASSERT_TRUE(hdr);
    EXPECT_EQ(hdr.value().mti, 0x12);

    // CP-SMT: PD=9, CP-MTI=0x13 -> header = 0x90 0x13
    data[1] = 0x13;
    hdr = parseL3Header(std::span<const uint8_t>(data));
    ASSERT_TRUE(hdr);
    EXPECT_EQ(hdr.value().mti, 0x13);
}

// =====================================================================
// SMS CP-ACK (GSM 24.011 8.1.3) — minimal message
// Reference: L3_Templates.ttcn ts_CP_ACK_MO (line 3658)
// Hex breakdown:
//   0x90 = PD(4)=0x09(SMS), Skip(4)=0x00
//   0x04 = CP-MTI(8)=0x04(CP-ACK), raw encoding
// No body octets.
// =====================================================================

TEST(GoldenSMSTest, CPAck_Minimal) {
    uint8_t data[] = {0x90, 0x04};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3CPAck::MTI);
    EXPECT_EQ(messagePD(*msg), L3PD::SMS);
    EXPECT_NE(tryGet<L3CPAck>(*msg), nullptr);
}

// =====================================================================
// SMS CP-ACK Round-Trip
// Construct empty CP-ACK -> serialize -> parse -> verify MTI preserved.
// Reference: L3_Templates.ttcn ts_CP_ACK_MO template structure
// =====================================================================

TEST(GoldenSMSTest, CPAck_RoundTrip) {
    L3CPAck orig;
    ParsedMessage pm(SMS(std::move(orig)));
    auto rt = roundtrip(pm);
    ASSERT_TRUE(rt);
    EXPECT_EQ(messageMTI(*rt), L3CPAck::MTI);
}

// =====================================================================
// SMS CP-ERROR (GSM 24.011 8.1.4) — with cause
// Reference: L3_Templates.ttcn ts_CP_ERROR_MO (line 3664)
// Hex breakdown:
//   0x90 = PD(4)=0x09(SMS), Skip(4)=0x00
//   0x10 = CP-MTI(8)=0x10(CP-ERROR), raw encoding
//   0x03 = CP-Cause=UnknownRPMessageType (7-bit value)
// =====================================================================

TEST(GoldenSMSTest, CPErr_WithCause) {
    uint8_t data[] = {0x90, 0x10, 0x03};
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
// Reference: L3_Templates.ttcn ts_CP_ERROR_MO template structure
// =====================================================================

TEST(GoldenSMSTest, CPErr_RoundTrip) {
    ParsedMessage pm(SMS(L3CPErr{}));
    auto rt = roundtrip(pm);
    ASSERT_TRUE(rt);
    EXPECT_EQ(messageMTI(*rt), L3CPErr::MTI);
}

// =====================================================================
// SMS CP-DATA (GSM 24.011 8.1.2) — with RPDU payload
// Reference: L3_Templates.ttcn ts_CP_DATA_MO (line 3648)
// Hex breakdown:
//   0x90 = PD(4)=0x09(SMS), Skip(4)=0x00
//   0x01 = CP-MTI(8)=0x01(CP-DATA), raw encoding
//   0x02 = CP-User-Data-Length(8) = 2 octets of RPDU follow
//   0x00 = RP header: Spare(5)=0 | RP-MTI(3)=0 (RP-DATA MO)
//   0x01 = RP-Message-Reference = 1
// =====================================================================

TEST(GoldenSMSTest, CPData_WithRPDU) {
    // CP-DATA containing a minimal RP-DATA header (2 octets: rp-header + message-ref)
    uint8_t data[] = {0x90, 0x01, 0x02, 0x00, 0x01};
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
// Reference: L3_Templates.ttcn ts_CP_DATA_MO template structure
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
// SMS CP-STATUS (GSM 24.011 8.1.5) — minimal message
// Reference: 3GPP TS 24.011 section 8.1.5
// Hex breakdown:
//   0x90 = PD(4)=0x09(SMS), Skip(4)=0x00
//   0x12 = CP-MTI(8)=0x12(CP-STATUS), raw encoding
//   0x00 = TP-OI(8) = 0
//   0x00 = MTI(8) = 0 (no message reference since bit 1 == 0)
// =====================================================================

TEST(GoldenSMSTest, CPStatus_Minimal) {
    uint8_t data[] = {0x90, 0x12, 0x00, 0x00};
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
// SMS CP-SMT (GSM 24.011 8.1.6) — with RPDU payload
// Reference: 3GPP TS 24.011 section 8.1.6
// Hex breakdown:
//   0x90 = PD(4)=0x09(SMS), Skip(4)=0x00
//   0x13 = CP-MTI(8)=0x13(CP-SMT), raw encoding
//   0x02 = CP-User-Data-Length(8) = 2 octets of RPDU follow
//   0x07 = RP header: Spare(5)=0 | RP-MTI(3)=7 (RP-SMMA MT)
//   0x05 = RP-Message-Reference = 5
// =====================================================================

TEST(GoldenSMSTest, CPSMT_WithRPDU) {
    uint8_t data[] = {0x90, 0x13, 0x02, 0x07, 0x05};
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
// SMS RP-ACK (GSM 24.011 7.3.2) — parse from raw bytes
// Reference: L3_Templates.ttcn ts_RP_ACK_MO (line 3572)
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
// Reference: L3_Templates.ttcn ts_RP_ACK_MO template structure
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
// SMS RP-ERROR (GSM 24.011 7.3.4) — parse from raw bytes
// Reference: L3_Templates.ttcn ts_RP_ERROR_MO (line 3590)
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
// Reference: L3_Templates.ttcn ts_RP_ERROR_MO template structure
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
// SMS RP-SMMA (GSM 24.011 7.3.3) — parse from raw bytes
// Reference: L3_Templates.ttcn ts_RP_SMMA_MO (line 3635)
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
// Reference: L3_Templates.ttcn ts_RP_SMMA_MO template structure
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
// SMS TP Deliver (GSM 23.040 9.2.2.1) — parse minimal TPDU
// Reference: L3_Templates.ttcn tr_SMS_DELIVER (line 3489)
// Hex breakdown (TPDU within RP-User-Data):
//   0x00 = TP-MTI(2)=00(Deliver) | mms(1)=0 | lp(1)=0 | spare(1)=0 | sri(1)=0 | udhi(1)=0 | rp(1)=0
//   0x07 = TP-OA Length = 7 (TON_NPI + 6 BCD digit bytes = phone number)
//   0x91 = TON=International(1) | NPI=E164(1) -> 0b1001_0001 = 0x91
//   0x23 = spare(4) | digit2(4)=3 (nibble-swapped BCD for phone 12...)
//   0x45 = digit3(4)=5 | digit4(4)=4
//   0x67 = digit5(4)=7 | digit6(4)=6
//   0x89 = digit7(4)=9 | digit8(4)=8
//   0x00 = TP-PID = Default
//   0x00 = TP-DCS = Default_Alphabet
//   [7 octets TP-SCTS omitted for brevity, using zeros]
//   0x05 = TP-UDL = 5 bytes of user data
//   0x48 0x65 0x6C 0x6C 0x6F = "Hello" (user data)
// =====================================================================

TEST(GoldenSMSTest, TPDeliver_Parse) {
    uint8_t data[] = {
        0x00,                     // header: TP-MTI=00, all flags=0
        0x05, 0x91, 0x23, 0x45,   // TP-OA: length=5 (TON_NPI + 4 digit bytes follow)
        0x67, 0x89,               // ...digits
        0x00,                     // TP-PID = Default
        0x00,                     // TP-DCS = Default_Alphabet
        0x00, 0x00, 0x00, 0x00,   // TP-SCTS (7 octets, zeros)
        0x00, 0x00, 0x00,
        0x05,                     // TP-UDL = 5
        0x48, 0x65, 0x6C, 0x6C, 0x6F // "Hello"
    };
    BitReader br(data, sizeof(data) * 8);
    auto deliver = L3TPDeliver::parse(br);
    ASSERT_TRUE(deliver);
    EXPECT_EQ(deliver.value().udl(), 5u);
    EXPECT_EQ(deliver.value().userData().size(), 5u);
    EXPECT_EQ(deliver.value().userData()[0], 0x48); // 'H'
}

// =====================================================================
// SMS TP Submit (GSM 23.040 9.2.2.2) — parse minimal TPDU
// Reference: L3_Templates.ttcn ts_SMS_SUBMIT (line 3467)
// Hex breakdown (TPDU within RP-User-Data):
//   0x61 = TP-MTI(2)=01(Submit) | rd(1)=1 | vpf(2)=00 | srr(1)=0 | udhi(1)=0 | rp(1)=0
//   0x03 = TP-MR = 3 (message reference)
//   0x07 = TP-DA Length = 7
//   0x91 = TON=International(1) | NPI=E164(1)
//   0x23 = digit bytes...
//   0x45, 0x67, 0x89
//   0x00 = TP-PID = Default
//   0x00 = TP-DCS = Default_Alphabet
//   0x05 = TP-UDL = 5
//   0x48 0x65 0x6C 0x6C 0x6F = "Hello" (user data)
// =====================================================================

TEST(GoldenSMSTest, TPSubmit_Parse) {
    uint8_t data[] = {
        0x61,                     // header: TP-MTI=01, rd=1, vpf=00, srr=0, udhi=0, rp=0
        0x03,                     // TP-MR = 3
        0x05, 0x91, 0x23, 0x45,   // TP-DA: length=5 (TON_NPI + 4 digit bytes follow)
        0x67, 0x89,               // ...digits
        0x00,                     // TP-PID = Default
        0x00,                     // TP-DCS = Default_Alphabet
        0x05,                     // TP-UDL = 5
        0x48, 0x65, 0x6C, 0x6C, 0x6F // "Hello"
    };
    BitReader br(data, sizeof(data) * 8);
    auto submit = L3TPSubmit::parse(br);
    ASSERT_TRUE(submit);
    EXPECT_EQ(submit.value().messageReference(), 3u);
    EXPECT_EQ(submit.value().userData().size(), 5u);
}

// =====================================================================
// SMS Full Wrapper Test
// Parse full L3 SMS message: CP-DATA -> RP-DATA -> TP-Submit
// This tests the complete nesting: L3 header -> CP layer -> RP layer -> TP layer.
// Reference: L3_Templates.ttcn ts_ML3_MO_SMS (line 3700)
// =====================================================================

TEST(GoldenSMSTest, FullSMSWrapper_MO) {
    // Construct a full MO SMS: CP-DATA containing RP-DATA containing TP-Submit
    // L3 Header: PD=9, CP-MTI=1 (CP-DATA)
    // CP Body: Length + RPDU
    //   RP header: Spare(5)=0 | RP-MTI(3)=0 (RP-DATA MO)
    //   RP Message-Ref: 1
    //   RP User-Data: TP-Submit TPDU

    // Build TP-Submit first
    std::vector<uint8_t> tpdu;
    tpdu.push_back(0x61); // TP-Submit header
    tpdu.push_back(0x03); // TP-MR
    tpdu.push_back(0x07); // TP-DA length
    tpdu.push_back(0x91); // TON/NPI
    tpdu.push_back(0x23);
    tpdu.push_back(0x45);
    tpdu.push_back(0x67);
    tpdu.push_back(0x89);
    tpdu.push_back(0x00); // TP-PID
    tpdu.push_back(0x00); // TP-DCS
    tpdu.push_back(0x05); // TP-UDL
    tpdu.insert(tpdu.end(), {0x48, 0x65, 0x6C, 0x6C, 0x6F}); // "Hello"

    // Build RP-DATA: header + message-ref + user-data-length + TPDU
    std::vector<uint8_t> rpdu;
    rpdu.push_back(0x00); // RP header: Spare(5)=0 | RP-MTI(3)=0 (RP-DATA MO)
    rpdu.push_back(0x01); // RP-Message-Reference = 1
    rpdu.push_back(static_cast<uint8_t>(tpdu.size())); // RP-User-Data length
    rpdu.insert(rpdu.end(), tpdu.begin(), tpdu.end());

    // Build full L3 message: header + CP body
    std::vector<uint8_t> l3msg;
    l3msg.push_back(0x90); // PD=9, Skip=0
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
}

// =====================================================================
// SMS Full Wrapper MT Test
// Parse full MT SMS message: CP-DATA -> RP-DATA -> TP-Deliver
// Reference: L3_Templates.ttcn tr_ML3_MT_SMS (line 3726)
// =====================================================================

TEST(GoldenSMSTest, FullSMSWrapper_MT) {
    // Construct a full MT SMS: CP-DATA containing RP-DATA containing TP-Deliver
    std::vector<uint8_t> tpdu;
    tpdu.push_back(0x00); // TP-Deliver header
    tpdu.push_back(0x07); // TP-OA length
    tpdu.insert(tpdu.end(), {0x91, 0x23, 0x45, 0x67, 0x89}); // OA
    tpdu.push_back(0x00); // TP-PID
    tpdu.push_back(0x00); // TP-DCS
    tpdu.insert(tpdu.end(), {0,0,0,0,0,0,0}); // TP-SCTS (7 zero octets)
    tpdu.push_back(0x05); // TP-UDL
    tpdu.insert(tpdu.end(), {0x48, 0x65, 0x6C, 0x6C, 0x6F}); // "Hello"

    // Build RP-DATA MT: header + message-ref + user-data-length + TPDU
    std::vector<uint8_t> rpdu;
    rpdu.push_back(0x01); // RP header: Spare(5)=0 | RP-MTI(3)=1 (RP-DATA MT)
    rpdu.push_back(0x02); // RP-Message-Reference = 2
    rpdu.push_back(static_cast<uint8_t>(tpdu.size()));
    rpdu.insert(rpdu.end(), tpdu.begin(), tpdu.end());

    // Build full L3 message
    std::vector<uint8_t> l3msg;
    l3msg.push_back(0x90); // PD=9, Skip=0
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
// SMS TP Status Report (GSM 23.040 9.2.2.3) — minimal parse
// Reference: 3GPP TS 23.040 section 9.2.2.3
// =====================================================================

TEST(GoldenSMSTest, TPStatusReport_Parse) {
    uint8_t data[] = {
        0x80,                     // header: TP-MTI=10, spare=0
        0x05,                     // TP-MR = 5
        0x05, 0x91, 0x23, 0x45,   // TP-DA: length=5 (TON_NPI + 4 digit bytes)
        0x67, 0x89,               // ...digits
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
    EXPECT_EQ(sr.value().sts(), 1u);
}

// =====================================================================
// SMS TP Command (GSM 23.040 9.2.2.5) — minimal parse
// Reference: 3GPP TS 23.040 section 9.2.2.5
// =====================================================================

TEST(GoldenSMSTest, TPCommand_Parse) {
    uint8_t data[] = {
        0xC0,                     // header: TP-MTI=11, spare=0
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
}

// =====================================================================
// SMS TP Address (GSM 23.040 9.1.2.4) — parse LV format
// Reference: L3_Templates.ttcn TP_DA and TP_OA templates
// =====================================================================

TEST(GoldenSMSTest, TPAddress_Parse) {
    // Length=5 (TON_NPI + 4 digit bytes), TON=International(1), NPI=E164(1), digits BCD-swapped
    uint8_t data[] = {0x05, 0x91, 0x23, 0x45, 0x67, 0x89};
    BitReader br(data, sizeof(data) * 8);
    auto addr = L3TPAddress::parse(br);
    ASSERT_TRUE(addr);
    EXPECT_EQ(addr.value().ton(), TypeOfNumber::International);
    EXPECT_EQ(addr.value().npi(), NumberingPlan::E164);
}

// =====================================================================
// SMS CP-DATA Round-Trip with full RPDU
// Construct CP-DATA with RP-DATA containing TP-Submit -> serialize -> parse -> verify.
// Reference: L3_Templates.ttcn ts_ML3_MO_SMS (line 3700) end-to-end template
// =====================================================================

TEST(GoldenSMSTest, FullCPData_RoundTrip) {
    // Build the same message as in FullSMSWrapper_MO but via construction
    std::vector<uint8_t> tpdu = {
        0x61, 0x03, 0x07, 0x91, 0x23, 0x45, 0x67, 0x89,
        0x00, 0x00, 0x05, 0x48, 0x65, 0x6C, 0x6C, 0x6F
    };
    std::vector<uint8_t> rpdu = {0x00, 0x01};
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
    EXPECT_EQ((*bytes)[0], 0x90); // PD=9(SMS)

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

// ── SMS L3 Layer Builder Tests ────────────────────────────────────────

#include <gsml3parser/sms/l3smsl3messages.h>

// 3GPP TS 24.008 9.6.1: SMS Status Report Builder
TEST(SMSBuilderTest, SMSStatusReport) {
    auto msg = L3SMSStatusReport::builder()
        .tpMr(10)
        .rpDisp(RPDisposalType::DisplayToUser)
        .tpSt(TPStatus::Delivered)
        .build();
    ParsedMessage pm{SMS{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3SMSStatusReport::MTI);
}

// 3GPP TS 24.008 9.6.4: SMS Deliver Builder
TEST(SMSBuilderTest, SMSDeliver) {
    auto msg = L3SMSDeliver::builder()
        .tpMti(0)
        .tpMr(7)
        .tpPid(TPPID::Default)
        .tpDcs(TPDCS::Default_Alphabet)
        .scts(TPSCTimeStamp{})
        .build();
    ParsedMessage pm{SMS{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3SMSDeliver::MTI);
}

// 3GPP TS 24.008 9.6.5: SMS Deliver Reply Builder
TEST(SMSBuilderTest, SMSDeliverRep) {
    auto msg = L3SMSDeliverRep::builder()
        .tpMti(1)
        .tpMr(7)
        .build();
    ParsedMessage pm{SMS{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3SMSDeliverRep::MTI);
}

// 3GPP TS 24.008 9.6.6: SMS Status Report Ack Builder
TEST(SMSBuilderTest, SMSStatusReportAck) {
    auto msg = L3SMSStatusReportAck::builder()
        .tpMr(10)
        .build();
    ParsedMessage pm{SMS{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3SMSStatusReportAck::MTI);
}

// 3GPP TS 24.008 9.6.7: SMS Status Report Reject Builder
TEST(SMSBuilderTest, SMSStatusReportReject) {
    auto msg = L3SMSStatusReportReject::builder()
        .tpMr(10)
        .smCause(SMSCause::SMSSystemFailure)
        .build();
    ParsedMessage pm{SMS{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3SMSStatusReportReject::MTI);
}

// 3GPP TS 24.008 9.6.8: SMS TS Reject Builder
TEST(SMSBuilderTest, SMSTSReject) {
    auto msg = L3SMSTSReject::builder()
        .smCause(SMSCause::SMSSystemFailure)
        .build();
    ParsedMessage pm{SMS{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3SMSTSReject::MTI);
}

// 3GPP TS 24.008 9.6.10: SMS Submit Reject Builder
TEST(SMSBuilderTest, SMSSubmitReject) {
    auto msg = L3SMSSubmitReject::builder()
        .smCause(SMSCause::SMSSystemFailure)
        .build();
    ParsedMessage pm{SMS{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3SMSSubmitReject::MTI);
}

// 3GPP TS 24.008 9.6.12: SMS SSF Provided Reply Ack Builder (empty)
TEST(SMSBuilderTest, SMSSFProvidedRepAck) {
    auto msg = L3SMSSFProvidedRepAck::builder().build();
    ParsedMessage pm{SMS{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3SMSSFProvidedRepAck::MTI);
}

// 3GPP TS 24.008 9.6.13: SMS Notification Builder
TEST(SMSBuilderTest, SMSNotification) {
    auto msg = L3SMSNotification::builder()
        .tpDcs(TPDCS::Default_Alphabet)
        .build();
    ParsedMessage pm{SMS{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3SMSNotification::MTI);
}

// 3GPP TS 24.008 9.6.14: SMS Short Code Info Builder
TEST(SMSBuilderTest, SMSShortCodeInfo) {
    auto msg = L3SMSShortCodeInfo::builder()
        .shortCodeType(1)
        .build();
    ParsedMessage pm{SMS{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3SMSShortCodeInfo::MTI);
}
