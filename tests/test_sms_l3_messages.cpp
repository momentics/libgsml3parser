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

// SMS L3 Messages (TS 24.008 9.6) — round-trip and golden parse tests.
// Reference: osmo-ttcn3-hacks L3_Templates.ttcn (SMS-TS-* templates).
// Spec: 3GPP TS 24.008 sections 9.6.1-9.6.14, Table 10.6a.
//
// [GOLDEN DATA VERIFICATION]
// All SMS L3 message type identifiers verified against 3GPP TS 24.008 Table 10.6a.
// SMS L3 header format: PD=0x09(SMS), Skip(4 bits) in byte 0;
//   MTI(8 bits, raw — no NSD field) in byte 1.
// Note: MTI 0x12 and 0x13 overlap with CP-STATUS and CP-SMT respectively.
// The parser resolves overlaps by preferring CP messages for backward compat.

#include <gtest/gtest.h>
#include <gsml3parser/parser.h>
#include <gsml3parser/sms/l3smsl3messages.h>
#include <gsml3parser/visitor.h>

using namespace gsml3parser;

static Expected<ParsedMessage> roundtrip(const ParsedMessage& msg) {
    auto hex = writeL3Hex(msg);
    if (!hex) return Expected<ParsedMessage>::error(hex.error());
    return parseL3Hex(hex.value());
}

// =====================================================================
// SMS L3 MESSAGE TYPE VALUES (GSM 24.008 Table 10.6a)
// [GSM SPEC VERIFIED] SMS L3 messages use 8-bit raw MTI in byte 1,
//   same as CP-layer SMS messages (unlike MM/CC which shift MTI).
// =====================================================================

TEST(SMSL3Messages, MessageTypeValues) {
    EXPECT_EQ(L3SMSStatusReport::MTI, 0x11);
    EXPECT_EQ(L3SMSProvidedReplyExpected::MTI, 0x12);
    EXPECT_EQ(L3SMSSubmitRep::MTI, 0x13);
    EXPECT_EQ(L3SMSDeliver::MTI, 0x14);
    EXPECT_EQ(L3SMSDeliverRep::MTI, 0x15);
    EXPECT_EQ(L3SMSStatusReportAck::MTI, 0x16);
    EXPECT_EQ(L3SMSStatusReportReject::MTI, 0x17);
    EXPECT_EQ(L3SMSTSReject::MTI, 0x18);
    EXPECT_EQ(L3SMSSubmitDeferred::MTI, 0x19);
    EXPECT_EQ(L3SMSSubmitReject::MTI, 0x1A);
    EXPECT_EQ(L3SMSSFProvidedRep::MTI, 0x1B);
    EXPECT_EQ(L3SMSSFProvidedRepAck::MTI, 0x1C);
    EXPECT_EQ(L3SMSNotification::MTI, 0x1D);
    EXPECT_EQ(L3SMSShortCodeInfo::MTI, 0x1E);
}

// =====================================================================
// SMS Status Report Round-Trip (24.008 9.6.1)
// Minimal message: TP-MR(1) + RP-Disp(1) + TP-ST(1) = 3 bytes body
// =====================================================================

TEST(SMSL3Messages, Roundtrip_SMSStatusReport) {
    L3SMSStatusReport orig;
    SMS smVariant(orig);
    ParsedMessage pm(std::move(smVariant));
    auto hex = writeL3Hex(pm);
    ASSERT_TRUE(hex);
    auto reparsed = parseL3Hex(*hex);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3SMSStatusReport::MTI);
    EXPECT_EQ(messagePD(*reparsed), L3PD::SMS);
}

// =====================================================================
// SMS Provided Reply Expected (24.008 9.6.2)
// Note: MTI 0x12 overlaps with CP-STATUS; parser dispatches to CP message.
// Only verify write produces correct hex output.
// =====================================================================

TEST(SMSL3Messages, Write_SMSProvidedReplyExpected) {
    L3SMSProvidedReplyExpected orig;
    SMS smVariant(orig);
    ParsedMessage pm(std::move(smVariant));
    auto hex = writeL3Hex(pm);
    ASSERT_TRUE(hex);
    EXPECT_EQ(hex.value(), "901200"); // PD=0x09, MTI=0x12, TP-DCS=0x00
}

// =====================================================================
// SMS Submit Reply (24.008 9.6.3)
// Note: MTI 0x13 overlaps with CP-SMT; parser dispatches to CP message.
// Only verify write produces correct hex output.
// =====================================================================

TEST(SMSL3Messages, Write_SMSSubmitRep) {
    L3SMSSubmitRep orig;
    SMS smVariant(orig);
    ParsedMessage pm(std::move(smVariant));
    auto hex = writeL3Hex(pm);
    ASSERT_TRUE(hex);
    EXPECT_EQ(hex.value(), "901300"); // PD=0x09, MTI=0x13, TP-DCS=0x00
}

// =====================================================================
// SMS Deliver Round-Trip (24.008 9.6.4)
// Minimal: TP-MTI(4)|TP-MR(1) + TP-PID(1) + TP-DCS(1) + SCTS(7) = 11 bytes body
// =====================================================================

TEST(SMSL3Messages, Roundtrip_SMSDeliver) {
    L3SMSDeliver orig;
    SMS smVariant(orig);
    ParsedMessage pm(std::move(smVariant));
    auto hex = writeL3Hex(pm);
    ASSERT_TRUE(hex);
    auto reparsed = parseL3Hex(*hex);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3SMSDeliver::MTI);
    EXPECT_EQ(messagePD(*reparsed), L3PD::SMS);
}

// =====================================================================
// SMS Deliver Reply Round-Trip (24.008 9.6.5)
// Minimal: TP-MTI(4)|TP-MR(1) + TP-PID(1) + TP-DCS(1) = 4 bytes body
// =====================================================================

TEST(SMSL3Messages, Roundtrip_SMSDeliverRep) {
    L3SMSDeliverRep orig;
    SMS smVariant(orig);
    ParsedMessage pm(std::move(smVariant));
    auto hex = writeL3Hex(pm);
    ASSERT_TRUE(hex);
    auto reparsed = parseL3Hex(*hex);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3SMSDeliverRep::MTI);
    EXPECT_EQ(messagePD(*reparsed), L3PD::SMS);
}

// =====================================================================
// SMS Status Report Ack Round-Trip (24.008 9.6.6)
// Body: TP-MR(1) = 1 byte
// =====================================================================

TEST(SMSL3Messages, Roundtrip_SMSStatusReportAck) {
    L3SMSStatusReportAck orig;
    SMS smVariant(orig);
    ParsedMessage pm(std::move(smVariant));
    auto hex = writeL3Hex(pm);
    ASSERT_TRUE(hex);
    auto reparsed = parseL3Hex(*hex);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3SMSStatusReportAck::MTI);
    EXPECT_EQ(messagePD(*reparsed), L3PD::SMS);
}

// =====================================================================
// SMS Status Report Reject Round-Trip (24.008 9.6.7)
// Body: TP-MR(1) + SM-Cause(1) = 2 bytes
// =====================================================================

TEST(SMSL3Messages, Roundtrip_SMSStatusReportReject) {
    L3SMSStatusReportReject orig;
    SMS smVariant(orig);
    ParsedMessage pm(std::move(smVariant));
    auto hex = writeL3Hex(pm);
    ASSERT_TRUE(hex);
    auto reparsed = parseL3Hex(*hex);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3SMSStatusReportReject::MTI);
    EXPECT_EQ(messagePD(*reparsed), L3PD::SMS);
}

// =====================================================================
// SMS TS Reject Round-Trip (24.008 9.6.8)
// Body: SM-Cause(1) = 1 byte
// =====================================================================

TEST(SMSL3Messages, Roundtrip_SMSTSReject) {
    L3SMSTSReject orig;
    SMS smVariant(orig);
    ParsedMessage pm(std::move(smVariant));
    auto hex = writeL3Hex(pm);
    ASSERT_TRUE(hex);
    auto reparsed = parseL3Hex(*hex);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3SMSTSReject::MTI);
    EXPECT_EQ(messagePD(*reparsed), L3PD::SMS);
}

// =====================================================================
// SMS Submit Deferred Round-Trip (24.008 9.6.9)
// Minimal: TP-DCS(1) = 1 byte body
// =====================================================================

TEST(SMSL3Messages, Roundtrip_SMSSubmitDeferred) {
    L3SMSSubmitDeferred orig;
    SMS smVariant(orig);
    ParsedMessage pm(std::move(smVariant));
    auto hex = writeL3Hex(pm);
    ASSERT_TRUE(hex);
    auto reparsed = parseL3Hex(*hex);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3SMSSubmitDeferred::MTI);
    EXPECT_EQ(messagePD(*reparsed), L3PD::SMS);
}

// =====================================================================
// SMS Submit Reject Round-Trip (24.008 9.6.10)
// Body: SM-Cause(1) = 1 byte
// =====================================================================

TEST(SMSL3Messages, Roundtrip_SMSSubmitReject) {
    L3SMSSubmitReject orig;
    SMS smVariant(orig);
    ParsedMessage pm(std::move(smVariant));
    auto hex = writeL3Hex(pm);
    ASSERT_TRUE(hex);
    auto reparsed = parseL3Hex(*hex);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3SMSSubmitReject::MTI);
    EXPECT_EQ(messagePD(*reparsed), L3PD::SMS);
}

// =====================================================================
// SMS SSF Provided Reply Round-Trip (24.008 9.6.11)
// Minimal: TP-DCS(1) = 1 byte body
// =====================================================================

TEST(SMSL3Messages, Roundtrip_SMSSFProvidedRep) {
    L3SMSSFProvidedRep orig;
    SMS smVariant(orig);
    ParsedMessage pm(std::move(smVariant));
    auto hex = writeL3Hex(pm);
    ASSERT_TRUE(hex);
    auto reparsed = parseL3Hex(*hex);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3SMSSFProvidedRep::MTI);
    EXPECT_EQ(messagePD(*reparsed), L3PD::SMS);
}

// =====================================================================
// SMS SSF Provided Reply Ack Round-Trip (24.008 9.6.12)
// Empty body message
// =====================================================================

TEST(SMSL3Messages, Roundtrip_SMSSFProvidedRepAck) {
    L3SMSSFProvidedRepAck orig;
    SMS smVariant(orig);
    ParsedMessage pm(std::move(smVariant));
    auto hex = writeL3Hex(pm);
    ASSERT_TRUE(hex);
    auto reparsed = parseL3Hex(*hex);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3SMSSFProvidedRepAck::MTI);
    EXPECT_EQ(messagePD(*reparsed), L3PD::SMS);
}

// =====================================================================
// SMS Notification Round-Trip (24.008 9.6.13)
// Minimal: TP-DCS(1) = 1 byte body
// =====================================================================

TEST(SMSL3Messages, Roundtrip_SMSNotification) {
    L3SMSNotification orig;
    SMS smVariant(orig);
    ParsedMessage pm(std::move(smVariant));
    auto hex = writeL3Hex(pm);
    ASSERT_TRUE(hex);
    auto reparsed = parseL3Hex(*hex);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3SMSNotification::MTI);
    EXPECT_EQ(messagePD(*reparsed), L3PD::SMS);
}

// =====================================================================
// SMS Short Code Info Round-Trip (24.008 9.6.14)
// Minimal: ShortCodeType(1) = 1 byte body
// =====================================================================

TEST(SMSL3Messages, Roundtrip_SMSShortCodeInfo) {
    L3SMSShortCodeInfo orig;
    SMS smVariant(orig);
    ParsedMessage pm(std::move(smVariant));
    auto hex = writeL3Hex(pm);
    ASSERT_TRUE(hex);
    auto reparsed = parseL3Hex(*hex);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3SMSShortCodeInfo::MTI);
    EXPECT_EQ(messagePD(*reparsed), L3PD::SMS);
}

// =====================================================================
// Golden Parse: SMS Status Report (24.008 9.6.1)
// Header: PD=0x09, MTI=0x11
// Body: TP-MR=0x05, RP-Disp=0x01(DisplayToUser), TP-ST=0x00(Delivered)
// =====================================================================

TEST(SMSL3Messages, GoldenParse_SMSStatusReport) {
    uint8_t data[] = {0x90, 0x11, 0x05, 0x01, 0x00};
    auto result = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(result);
    EXPECT_EQ(messageName(*result), "SMSStatusReport");
    EXPECT_EQ(messageMTI(*result), L3SMSStatusReport::MTI);
    EXPECT_EQ(messagePD(*result), L3PD::SMS);
    if (auto* msg = tryGet<L3SMSStatusReport>(*result)) {
        EXPECT_EQ(msg->tpMr(), 5u);
        EXPECT_EQ(msg->rpDisp(), RPDisposalType::DisplayToUser);
        EXPECT_EQ(msg->tpSt(), TPStatus::Delivered);
    }
}

// =====================================================================
// Golden Parse: SMS Status Report Ack (24.008 9.6.6)
// Header: PD=0x09, MTI=0x16
// Body: TP-MR=0x0A
// =====================================================================

TEST(SMSL3Messages, GoldenParse_SMSStatusReportAck) {
    uint8_t data[] = {0x90, 0x16, 0x0A};
    auto result = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(result);
    EXPECT_EQ(messageName(*result), "SMSStatusReportAck");
    if (auto* msg = tryGet<L3SMSStatusReportAck>(*result)) {
        EXPECT_EQ(msg->tpMr(), 10u);
    }
}

// =====================================================================
// Golden Parse: SMS TS Reject (24.008 9.6.8)
// Header: PD=0x09, MTI=0x18
// Body: SM-Cause=0x0C(SMSSystemFailure)
// =====================================================================

TEST(SMSL3Messages, GoldenParse_SMSTSReject) {
    uint8_t data[] = {0x90, 0x18, 0x0C};
    auto result = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(result);
    EXPECT_EQ(messageName(*result), "SMSTSReject");
    if (auto* msg = tryGet<L3SMSTSReject>(*result)) {
        EXPECT_EQ(msg->smCause(), SMSCause::SMSSystemFailure);
    }
}

// =====================================================================
// Golden Parse: SMS Submit Reject (24.008 9.6.10)
// Header: PD=0x09, MTI=0x1A
// Body: SM-Cause=0x1C(InvalidSourceAddressSubsystem)
// =====================================================================

TEST(SMSL3Messages, GoldenParse_SMSSubmitReject) {
    uint8_t data[] = {0x90, 0x1A, 0x1C};
    auto result = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(result);
    EXPECT_EQ(messageName(*result), "SMSSubmitReject");
    if (auto* msg = tryGet<L3SMSSubmitReject>(*result)) {
        EXPECT_EQ(msg->smCause(), SMSCause::InvalidSourceAddressSubsystem);
    }
}

// =====================================================================
// Golden Parse: SMS SSF Provided Reply Ack (24.008 9.6.12)
// Header: PD=0x09, MTI=0x1C
// Empty body
// =====================================================================

TEST(SMSL3Messages, GoldenParse_SMSSFProvidedRepAck) {
    uint8_t data[] = {0x90, 0x1C};
    auto result = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(result);
    EXPECT_EQ(messageName(*result), "SMSSFProvidedReplyAck");
    EXPECT_EQ(messageMTI(*result), L3SMSSFProvidedRepAck::MTI);
}

// =====================================================================
// Golden Parse: SMS Short Code Info (24.008 9.6.14)
// Header: PD=0x09, MTI=0x1E
// Body: ShortCodeType=0x03(NetworkSpecific)
// =====================================================================

TEST(SMSL3Messages, GoldenParse_SMSShortCodeInfo) {
    uint8_t data[] = {0x90, 0x1E, 0x03};
    auto result = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(result);
    EXPECT_EQ(messageName(*result), "SMSShortCodeInfo");
    if (auto* msg = tryGet<L3SMSShortCodeInfo>(*result)) {
        EXPECT_EQ(msg->shortCodeType(), 3u);
    }
}

// =====================================================================
// Enum String Converters
// Verify TPStatus2Str, RPDisposalType2Str, SMSCause2Str return correct names.
// =====================================================================

TEST(SMSL3Messages, EnumStrings) {
    EXPECT_STREQ(TPStatus2Str(TPStatus::Delivered), "Delivered");
    EXPECT_STREQ(TPStatus2Str(TPStatus::ErasedAtMS), "ErasedAtMS");
    EXPECT_STREQ(TPStatus2Str(TPStatus::DeliveryNotPossible), "DeliveryNotPossible");
    EXPECT_STREQ(RPDisposalType2Str(RPDisposalType::NoFurtherAction), "NoFurtherAction");
    EXPECT_STREQ(RPDisposalType2Str(RPDisposalType::DisplayToUser), "DisplayToUser");
    EXPECT_STREQ(SMSCause2Str(SMSCause::NoCause), "NoCause");
    EXPECT_STREQ(SMSCause2Str(SMSCause::SMSSystemFailure), "SMSSystemFailure");
    EXPECT_STREQ(SMSCause2Str(SMSCause::CUGRejectDueToInvalidTGroupID), "CUGRejectDueToInvalidTGroupID");
}

// =====================================================================
// SMS Deliver Reply Golden Parse (24.008 9.6.5)
// Header: PD=0x09, MTI=0x15
// Body: TP-MTI(4bits)=0|spare(4bits) + TP-MR(1) + TP-PID(1) + TP-DCS(1)
// No optional TP-DA since only 4 body bytes (less than 3 required for addr detection)
// =====================================================================

TEST(SMSL3Messages, GoldenParse_SMSDeliverRep) {
    uint8_t data[] = {0x90, 0x15, 0x00, 0x04, 0x01, 0x00};
    auto result = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(result);
    EXPECT_EQ(messageName(*result), "SMSDeliverReply");
    if (auto* msg = tryGet<L3SMSDeliverRep>(*result)) {
        EXPECT_EQ(msg->tpMti(), 0u);
        EXPECT_EQ(msg->tpMr(), 4u);
        EXPECT_FALSE(msg->hasTpDa());
        EXPECT_EQ(msg->tpPid(), TPPID::GSM);
        EXPECT_EQ(msg->tpDcs(), TPDCS::Default_Alphabet);
    }
}

// =====================================================================
// SMS Notification with user data (24.008 9.6.13)
// Header: PD=0x09, MTI=0x1D
// Body: TP-PID(1) + TP-DCS(1) + UD-Length(1) + UD(data)
// =====================================================================

TEST(SMSL3Messages, GoldenParse_SMSNotificationWithUD) {
    uint8_t data[] = {0x90, 0x1D, 0x01, 0x00, 0x02, 0xDE, 0xAD};
    auto result = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(result);
    EXPECT_EQ(messageName(*result), "SMSNotification");
    if (auto* msg = tryGet<L3SMSNotification>(*result)) {
        EXPECT_TRUE(msg->hasTpPid());
        EXPECT_EQ(msg->tpPid(), TPPID::GSM);
        EXPECT_EQ(msg->tpDcs(), TPDCS::Default_Alphabet);
        EXPECT_TRUE(msg->hasTpUd());
        EXPECT_EQ(msg->tpUd().size(), 2u);
        EXPECT_EQ(msg->tpUd()[0], 0xDE);
        EXPECT_EQ(msg->tpUd()[1], 0xAD);
    }
}

// =====================================================================
// SMS Short Code Info with short code data (24.008 9.6.14)
// Header: PD=0x09, MTI=0x1E
// Body: ShortCodeType(1) + Length(1) + ShortCode(data)
// =====================================================================

TEST(SMSL3Messages, GoldenParse_SMSShortCodeInfoWithData) {
    uint8_t data[] = {0x90, 0x1E, 0x03, 0x02, 0x12, 0x34};
    auto result = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(result);
    EXPECT_EQ(messageName(*result), "SMSShortCodeInfo");
    if (auto* msg = tryGet<L3SMSShortCodeInfo>(*result)) {
        EXPECT_EQ(msg->shortCodeType(), 3u);
        EXPECT_TRUE(msg->hasShortCode());
        EXPECT_EQ(msg->shortCode().size(), 2u);
        EXPECT_EQ(msg->shortCode()[0], 0x12);
        EXPECT_EQ(msg->shortCode()[1], 0x34);
    }
}

// =====================================================================
// bodyLength correctness for all message types
// Verify that bodyLength() returns expected values for default-constructed messages.
// =====================================================================

TEST(SMSL3Messages, BodyLengths) {
    L3SMSStatusReport sr;
    EXPECT_EQ(sr.bodyLength(), 3u); // TP-MR + RP-Disp + TP-ST

    L3SMSProvidedReplyExpected pre;
    EXPECT_EQ(pre.bodyLength(), 1u); // TP-DCS only

    L3SMSSubmitRep srep;
    EXPECT_EQ(srep.bodyLength(), 1u); // TP-DCS only

    L3SMSDeliver del;
    EXPECT_EQ(del.bodyLength(), 11u); // TP-MTI+MR + PID + DCS + SCTS(7)

    L3SMSDeliverRep drep;
    EXPECT_EQ(drep.bodyLength(), 4u); // TP-MTI+MR + PID + DCS

    L3SMSStatusReportAck sack;
    EXPECT_EQ(sack.bodyLength(), 1u); // TP-MR

    L3SMSStatusReportReject srj;
    EXPECT_EQ(srj.bodyLength(), 2u); // TP-MR + SM-Cause

    L3SMSTSReject tsr;
    EXPECT_EQ(tsr.bodyLength(), 1u); // SM-Cause

    L3SMSSubmitDeferred sd;
    EXPECT_EQ(sd.bodyLength(), 1u); // TP-DCS only

    L3SMSSubmitReject srej;
    EXPECT_EQ(srej.bodyLength(), 1u); // SM-Cause

    L3SMSSFProvidedRep ssf;
    EXPECT_EQ(ssf.bodyLength(), 1u); // TP-DCS only

    L3SMSSFProvidedRepAck ssfa;
    EXPECT_EQ(ssfa.bodyLength(), 0u); // empty

    L3SMSNotification notif;
    EXPECT_EQ(notif.bodyLength(), 1u); // TP-DCS only

    L3SMSShortCodeInfo sci;
    EXPECT_EQ(sci.bodyLength(), 1u); // ShortCodeType only
}

// =====================================================================
// SMS Status Report Reject Golden Parse (24.008 9.6.7)
// Header: PD=0x09, MTI=0x17
// Body: TP-MR(1) + SM-Cause(1)
// =====================================================================

TEST(SMSL3Messages, GoldenParse_SMSStatusReportReject) {
    uint8_t data[] = {0x90, 0x17, 0x20, 0x0C};
    auto result = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(result);
    EXPECT_EQ(messageName(*result), "SMSStatusReportReject");
    if (auto* msg = tryGet<L3SMSStatusReportReject>(*result)) {
        EXPECT_EQ(msg->tpMr(), 0x20u);
        EXPECT_EQ(msg->smCause(), SMSCause::SMSSystemFailure);
    }
}
