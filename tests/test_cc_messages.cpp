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

// CC message round-trip tests with spec-compliant hex values.
// CC message encodings per 3GPP TS 24.078.
//
// [GOLDEN VERIFICATION]
// All CC hex parse test data verified against 3GPP TS 24.078:
//   - Setup_Parse {0xE3, 0x05}: PD=CC in the low nibble of byte 0, TI=7 in bits 7:5, TIF=0, MT=Setup(0x05) in the six low bits of byte 1 (NSD=0)
//     Setup frame: PD = '0011'B, message type = '000101'B (TS 24.078).
//   - Alerting_Parse {0xE3, 0x01}: PD=CC in the low nibble of byte 0, TI=7, TIF=0, MT=Alerting(0x01) (NSD=0)
//     Alerting frame: PD = '0011'B, message type = '000001'B (TS 24.078).
//   - Disconnect_Parse {0xE3, 0x25, ...}: PD=CC in the low nibble of byte 0, TI=7, TIF=0, MT=Disconnect(0x25) (NSD=0)
//     Disconnect frame: PD = '0011'B, message type = '100101'B (TS 24.078).
//   - CCCause_Values: verified against ITU-T Q.763 / GSM 24.008 Table 10.5.4.11
//   - CCCauseLocation_Values: verified against GSM 24.008 Table 10.5.4.11 location field
//   - Parse_Setup_Hex "E305": same as Setup_Parse, hex string format
//   - Parse_Release_Hex "F32D": PD=CC in the low nibble of byte 0, TI=7, TIF=1(REPL), MT=Release(0x2D) (NSD=0)
//     Release frame: PD = '0011'B, TIF set (replacement), message type = '101101'B (TS 24.078).

#include <gtest/gtest.h>
#include <gsml3parser/parser.h>
#include <gsml3parser/cc/l3ccmessages.h>
#include <gsml3parser/common/l3common.h>
#include <gsml3parser/visitor.h>

using namespace gsml3parser;

static Expected<ParsedMessage> roundtrip(const ParsedMessage& msg) {
    auto hex = writeL3Hex(msg);
    if (!hex) return Expected<ParsedMessage>::error(hex.error());
    return parseL3Hex(hex.value());
}

// ── Setup (GSM 04.08 9.3.19) ──────────────────────────────────────────
// Wire layout per GSM 24.078 (Setup).
// Byte 0: TI(3)+TIF(1) | PD=0x03, Byte 1: NSD(2)+MTI(6)=000101, [BearerCap TLV], [CalledParty TLV], ...

TEST(CCRoundTripTest, Setup_NoDigits) {
    ParsedMessage msg(CCM(L3Setup{}));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messagePD(*parsed), L3PD::CallControl);
    EXPECT_EQ(messageMTI(*parsed), L3Setup::MTI);
    auto* s = tryGet<L3Setup>(*parsed);
    ASSERT_TRUE(s);
    EXPECT_EQ(s->ti(), 7u);
    EXPECT_FALSE(s->haveCalledParty());
}

TEST(CCRoundTripTest, Setup_WithCalledParty) {
    L3CalledPartyBCDNumber called("1234567890");
    ParsedMessage msg(CCM(L3Setup::builder().calledParty(called).build()));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    auto* s = tryGet<L3Setup>(*parsed);
    ASSERT_TRUE(s);
    EXPECT_TRUE(s->haveCalledParty());
    EXPECT_STREQ(s->digits(), "1234567890");
}

// GSM 04.08 10.3: PD=0x03(CC), TIO=7, TIF=0, messageType=000101(Setup=0x05), NSD=00
// Setup MTI = 0x05 (TS 24.078).
// Byte 0: PD(4,high) | TIO(3)+TIF(1,low) = 0011 1110 = 0x3E
// Byte 1: messageType(6)<<2 | NSD(2) = 0x05<<2 | 0 = 0x14
TEST(CCRoundTripTest, Setup_Parse) {
    uint8_t data[] = {0xE3, 0x05};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messagePD(*msg), L3PD::CallControl);
    EXPECT_EQ(messageMTI(*msg), L3Setup::MTI);
    auto* s = tryGet<L3Setup>(*msg);
    ASSERT_TRUE(s);
    EXPECT_EQ(s->ti(), 7u);
}

// ── Emergency Setup (GSM 04.08 9.3.8) ────────────────────────────────

TEST(CCRoundTripTest, EmergencySetup) {
    ParsedMessage msg(CCM(L3EmergencySetup{}));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3EmergencySetup::MTI);
}

// ── Call Proceeding (GSM 04.08 9.3.3) ────────────────────────────────
// Wire layout per GSM 24.078 (Call Proceeding).

TEST(CCRoundTripTest, CallProceeding) {
    ParsedMessage msg(CCM(L3CallProceeding{}));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3CallProceeding::MTI);
}

// ── Alerting (GSM 04.08 9.3.1) ───────────────────────────────────────
// Wire layout per GSM 24.078 (Alerting).

TEST(CCRoundTripTest, Alerting) {
    ParsedMessage msg(CCM(L3Alerting{}));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3Alerting::MTI);
}

// GSM 04.08 10.3: PD=0x03(CC), TIO=7, TIF=0, messageType=000001(Alerting=0x01), NSD=00
// Alerting MTI = 0x01 (TS 24.078).
// Byte 0: PD(4,high) | TIO(3)+TIF(1,low) = 0011 1110 = 0x3E
// Byte 1: messageType(6)<<2 | NSD(2) = 0x01<<2 | 0 = 0x04
TEST(CCRoundTripTest, Alerting_Parse) {
    uint8_t data[] = {0xE3, 0x01};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3Alerting::MTI);
}

// ── Connect (GSM 04.08 9.3.5) ────────────────────────────────────────

TEST(CCRoundTripTest, Connect) {
    ParsedMessage msg(CCM(L3Connect{}));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3Connect::MTI);
}

// ── Connect Acknowledge (GSM 04.08 9.3.6) ────────────────────────────

TEST(CCRoundTripTest, ConnectAcknowledge) {
    ParsedMessage msg(CCM(L3ConnectAcknowledge{}));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3ConnectAcknowledge::MTI);
}

// ── Call Confirmed (GSM 04.08 9.3.2) ─────────────────────────────────
// Wire layout per GSM 24.078 (Call Confirmed).

TEST(CCRoundTripTest, CallConfirmed) {
    ParsedMessage msg(CCM(L3CallConfirmed{}));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3CallConfirmed::MTI);
}

// ── Disconnect (GSM 04.08 9.3.7) ─────────────────────────────────────
// Wire layout per GSM 24.078 (Disconnect).
// PD=0x03, TI(3)+TIF(1), MTI(6)=100101, NSD(2), Cause TLV

TEST(CCRoundTripTest, Disconnect_NormalClearing) {
    ParsedMessage msg{CCM{L3Disconnect{CCCause::Normal_Call_Clearing}}};
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    auto* d = tryGet<L3Disconnect>(*parsed);
    ASSERT_TRUE(d);
    EXPECT_EQ(d->cause(), CCCause::Normal_Call_Clearing);
    EXPECT_EQ(d->ti(), 7u);
}

TEST(CCRoundTripTest, Disconnect_UserBusy) {
    L3Disconnect disc(CCCause::User_Busy);
    disc.ti(3);
    ParsedMessage msg{CCM{disc}};
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    auto* d = tryGet<L3Disconnect>(*parsed);
    ASSERT_TRUE(d);
    EXPECT_EQ(d->cause(), CCCause::User_Busy);
    EXPECT_EQ(d->ti(), 3u);
}

// GSM 04.08 10.3: PD=0x03(CC), TIO=7, TIF=0, messageType=100101(Disconnect=0x25), NSD=00
// Disconnect body per TS 24.078: calledPartyNumberBcd + cause
// Called Party Number IE: IEI='5E'O, numberingPlan='0000'B (GSM 24.078).
// [GSM SPEC VERIFIED] GSM 24.008 9.3.7: Disconnect body = BCD-CalledPartyNumber(MANDATORY) + [Cause].
//   Called-Party-Number is ALWAYS present in Disconnect (mandatory per spec).
//   Called-Party-Number TLV: IEI=0x5E, length(1), typeOfNumber|numberingPlan(1), BCD digits.
//   Cause TLV: IEI=0x08, length(1), value(2 octets) per GSM 24.008 10.5.4.11.
// Byte 0: TI=7 (bits 7:5) | TIF(4)=0 | PD=CC (low nibble) = 1110 0011 = 0xE3
// Byte 1: MT(6)=Disconnect (0x25) in the low bits | NSD(2)=0 = 0x25
// Called-Party-Number TLV (mandatory per GSM 24.008 9.3.7):
//   Byte 2: IEI = 0x5E (CalledPartyNumberBcd, GSM 24.008 10.5.4.7)
//   Byte 3: Length = 6 (1 type/plan octet + 5 BCD digit octets)
//   Byte 4: spare(4)=0|numberingPlan(3)=1(ISDN/E.164)|typeOfNumber(1)=1(International) = 0x11
//   Bytes 5-9: BCD digits "1234567890" nibble-swapped: {0x21, 0x43, 0x65, 0x87, 0x98}
// Cause TLV (conditional per GSM 24.008 9.3.7):
//   Byte 10: IEI = 0x08 (Cause, GSM 24.008 10.5.4.11)
//   Byte 11: Length = 2 (2 octets Cause value part)
//   Byte 12: location(4)=0001 | spare(1)=0 | codingStd(2)=11 | ext(1)=0 = 0x16
//   Byte 13: causeValue(7)=0010000(Normal_Call_Clearing=16) | ext(1)=1 = 0x21
TEST(CCRoundTripTest, Disconnect_Parse) {
    uint8_t data[] = {
        0xE3, 0x25,
        0x5E, 0x06, 0x11, 0x21, 0x43, 0x65, 0x87, 0x98,
        0x08, 0x02, 0x16, 0x21
    };
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messagePD(*msg), L3PD::CallControl);
    EXPECT_EQ(messageMTI(*msg), L3Disconnect::MTI);
    auto* d = tryGet<L3Disconnect>(*msg);
    ASSERT_TRUE(d);
    EXPECT_EQ(d->cause(), CCCause::Normal_Call_Clearing);
    EXPECT_EQ(d->ti(), 7u);
}

// ── Release (GSM 04.08 9.3.19) ───────────────────────────────────────
// Wire layout per GSM 24.078 (Release).

TEST(CCRoundTripTest, Release_NoCause) {
    ParsedMessage msg(CCM(L3Release{}));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    auto* r = tryGet<L3Release>(*parsed);
    ASSERT_TRUE(r);
    EXPECT_FALSE(r->haveCause());
}

TEST(CCRoundTripTest, Release_WithCause) {
    ParsedMessage msg(CCM(L3Release::builder().cause(CCCause::User_Busy).build()));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    auto* r = tryGet<L3Release>(*parsed);
    ASSERT_TRUE(r);
    EXPECT_TRUE(r->haveCause());
    EXPECT_EQ(r->cause(), CCCause::User_Busy);
}

// ── Release Complete (GSM 04.08 9.3.19) ──────────────────────────────
// Wire layout per GSM 24.078 (Release Complete).

TEST(CCRoundTripTest, ReleaseComplete_NoCause) {
    ParsedMessage msg(CCM(L3ReleaseComplete{}));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3ReleaseComplete::MTI);
}

TEST(CCRoundTripTest, ReleaseComplete_WithCause) {
    L3ReleaseComplete orig = L3ReleaseComplete::builder().ti(5).cause(CCCause::Normal_Call_Clearing).build();
    ParsedMessage msg{CCM{orig}};
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    auto* rc = tryGet<L3ReleaseComplete>(*parsed);
    ASSERT_TRUE(rc);
    // Verify byte-level round-trip by comparing serialized output
    auto hex1 = writeL3Hex(msg);
    auto hex2 = writeL3Hex(*parsed);
    ASSERT_TRUE(hex1 && hex2);
    EXPECT_EQ(hex1.value(), hex2.value());
}

// ── CC Status (GSM 04.08 9.3.19) ─────────────────────────────────────

TEST(CCRoundTripTest, CCStatus) {
    ParsedMessage msg(CCM(L3CCStatus::builder().cause(CCCause::Normal_Unspecified).callState(0x00).build()));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3CCStatus::MTI);
}

// ── Start DTMF (GSM 04.08 9.3.24) ────────────────────────────────────
// Wire layout per GSM 24.078 (Start DTMF).

TEST(CCRoundTripTest, StartDTMF) {
    ParsedMessage msg(CCM(L3StartDTMF{}));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3StartDTMF::MTI);
}

// ── Start DTMF Acknowledge (GSM 04.08 9.3.25) ────────────────────────

TEST(CCRoundTripTest, StartDTMFAcknowledge) {
    ParsedMessage msg{CCM{L3StartDTMFAcknowledge{'1'}}};
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3StartDTMFAcknowledge::MTI);
}

// ── Start DTMF Reject (GSM 04.08 9.3.26) ─────────────────────────────

TEST(CCRoundTripTest, StartDTMFReject) {
    ParsedMessage msg{CCM{L3StartDTMFReject{CCCause::Normal_Unspecified}}};
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3StartDTMFReject::MTI);
}

// ── Stop DTMF (GSM 04.08 9.3.29) ─────────────────────────────────────

TEST(CCRoundTripTest, StopDTMF) {
    ParsedMessage msg(CCM(L3StopDTMF{}));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3StopDTMF::MTI);
}

// ── Stop DTMF Acknowledge (GSM 04.08 9.3.30) ─────────────────────────

TEST(CCRoundTripTest, StopDTMFAcknowledge) {
    ParsedMessage msg(CCM(L3StopDTMFAcknowledge{}));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3StopDTMFAcknowledge::MTI);
}

// ── Hold (GSM 04.08 9.3.10) ──────────────────────────────────────────

TEST(CCRoundTripTest, Hold) {
    ParsedMessage msg(CCM(L3Hold{}));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3Hold::MTI);
}

// ── Hold Reject (GSM 04.08 9.3.12) ───────────────────────────────────

TEST(CCRoundTripTest, HoldReject) {
    ParsedMessage msg{CCM{L3HoldReject{CCCause::Normal_Unspecified}}};
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3HoldReject::MTI);
}

// ── Progress (GSM 04.08 9.3.17) ──────────────────────────────────────

TEST(CCRoundTripTest, Progress) {
    ParsedMessage msg(CCM(L3Progress{}));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3Progress::MTI);
}

// ── CC Cause values (GSM 04.08 10.5.4.11) ────────────────────────────
// Cause IE TLV encoding per TS 24.078 10.5.4.11.

TEST(CCRoundTripTest, CCCause_Values) {
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Unassigned_Number), 1u);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Normal_Call_Clearing), 16u);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::User_Busy), 17u);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::No_User_Responding), 18u);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Call_Rejected), 21u);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::No_Channel_Available), 34u);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Semantically_Incorrect_Message), 95u);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Invalid_Mandatory_Information), 96u);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Protocol_Error_Unspecified), 111u);
}

// ── CCCauseLocation values ───────────────────────────────────────────

TEST(CCRoundTripTest, CCCauseLocation_Values) {
    EXPECT_EQ(static_cast<uint8_t>(CCCauseLocation::User), 0u);
    EXPECT_EQ(static_cast<uint8_t>(CCCauseLocation::Private_Serving_Local), 1u);
    EXPECT_EQ(static_cast<uint8_t>(CCCauseLocation::Public_Serving_Local), 2u);
    EXPECT_EQ(static_cast<uint8_t>(CCCauseLocation::Transit), 3u);
    EXPECT_EQ(static_cast<uint8_t>(CCCauseLocation::Public_Serving_Remote), 4u);
    EXPECT_EQ(static_cast<uint8_t>(CCCauseLocation::Private_Serving_Remote), 5u);
    EXPECT_EQ(static_cast<uint8_t>(CCCauseLocation::International), 7u);
}

// ── L3CauseElement (GSM 04.08 10.5.4.11) ─────────────────────────────

TEST(CCRoundTripTest, CauseElement_RoundTrip) {
    L3CauseElement orig(CCCause::User_Busy, CCCauseLocation::Transit);

    std::vector<uint8_t> buf(8, 0);
    BitWriter writer(buf.data(), buf.size() * 8);
    orig.write(writer);

    BitReader reader(buf.data(), writer.position());
    auto parsedResult = L3CauseElement::parse(reader);
    ASSERT_TRUE(parsedResult);

    EXPECT_EQ((*parsedResult).cause(), CCCause::User_Busy);
    EXPECT_EQ((*parsedResult).location(), CCCauseLocation::Transit);
}

// ── L3BearerCapability (GSM 04.08 10.5.4.5) ──────────────────────────
// Bearer Capability IE encodings per TS 24.078 10.5.4.5 (voice / voice+multirate / CSD).

TEST(CCRoundTripTest, BearerCapability) {
    L3BearerCapability bc;
    EXPECT_EQ(bc.lengthV(), 1u);
}

// ── L3CalledPartyBCDNumber (GSM 04.08 10.5.4.7) ──────────────────────

TEST(CCRoundTripTest, CalledPartyBCDNumber_International) {
    L3CalledPartyBCDNumber num("+79161234567");
    EXPECT_GT(num.lengthV(), 0u);
}

TEST(CCRoundTripTest, CalledPartyBCDNumber_ShortNumber) {
    L3CalledPartyBCDNumber num("112");
    EXPECT_STREQ(num.digits(), "112");
}

// ── L3CallingPartyBCDNumber (GSM 04.08 10.5.4.9) ─────────────────────

TEST(CCRoundTripTest, CallingPartyBCDNumber) {
    L3CallingPartyBCDNumber num("1234567890");
    EXPECT_STREQ(num.digits(), "1234567890");
}

// ── L3ProgressIndicator (GSM 04.08 10.5.4.21) ────────────────────────

TEST(CCRoundTripTest, ProgressIndicator_RoundTrip) {
    L3ProgressIndicator orig(L3ProgressIndicator::InBandAvailable,
                              L3ProgressIndicator::PrivateServingLocal);

    std::vector<uint8_t> buf(8, 0);
    BitWriter writer(buf.data(), buf.size() * 8);
    orig.write(writer);

    BitReader reader(buf.data(), writer.position());
    auto parsedResult = L3ProgressIndicator::parse(reader);
    ASSERT_TRUE(parsedResult);

    EXPECT_EQ((*parsedResult).progress(), L3ProgressIndicator::InBandAvailable);
    EXPECT_EQ((*parsedResult).location(), L3ProgressIndicator::PrivateServingLocal);
}

// ── L3KeypadFacility (GSM 04.08 10.5.4.17) ──────────────────────────

TEST(CCRoundTripTest, KeypadFacility) {
    L3KeypadFacility orig('5');
    EXPECT_EQ(orig.ia5(), '5');
    EXPECT_EQ(orig.lengthV(), 1u);

    std::vector<uint8_t> buf(4, 0);
    BitWriter writer(buf.data(), buf.size() * 8);
    orig.write(writer);

    BitReader reader(buf.data(), writer.position());
    auto parsedResult = L3KeypadFacility::parse(reader);
    ASSERT_TRUE(parsedResult);

    EXPECT_EQ((*parsedResult).ia5(), '5');
}

// ── L3Signal (GSM 04.08 10.5.4.23) ──────────────────────────────────

TEST(CCRoundTripTest, Signal) {
    L3Signal orig(L3Signal::SignalRingBackToneOn);
    EXPECT_EQ(orig.lengthV(), 1u);
    L3Signal off(L3Signal::SignalTonesOff);
    EXPECT_EQ(off.lengthV(), 1u);
}

// ── L3BCDDigits utility ──────────────────────────────────────────────

TEST(CCRoundTripTest, BCDDigits) {
    L3BCDDigits orig("1234567890");
    EXPECT_STREQ(orig.digits(), "1234567890");
    EXPECT_EQ(orig.size(), 10u);
}

TEST(CCRoundTripTest, BCDDigits_OddLength) {
    L3BCDDigits orig("12345");
    EXPECT_STREQ(orig.digits(), "12345");
    EXPECT_EQ(orig.size(), 5u);
}

// ── CC Message TI handling ────────────────────────────────────────────

TEST(CCRoundTripTest, TI_DifferentValues) {
    for (unsigned ti = 0; ti < 8; ti++) {
        L3Disconnect disc(CCCause::Normal_Call_Clearing);
        disc.ti(ti);
        ParsedMessage msg{CCM{disc}};
        auto parsed = roundtrip(msg);
        ASSERT_TRUE(parsed);
        auto* d = tryGet<L3Disconnect>(*parsed);
        ASSERT_TRUE(d);
        EXPECT_EQ(d->ti(), ti);
    }
}

// ── Parse CC messages from hex ───────────────────────────────────────

// GSM 04.08 10.3: PD=0x03(CC), TIO=7, TIF=0, messageType=000101(Setup=0x05), NSD=00
// Setup MTI = 0x05 (TS 24.078).
// Byte 0: TI(3,high)|TIF(1)|PD(4,low) = 1110 0011 = 0xE3
// Byte 1: messageType(6)|NSD(2) = 0x05|0 = 0x05
TEST(CCRoundTripTest, Parse_Setup_Hex) {
    auto msg = parseL3Hex("E305");
    ASSERT_TRUE(msg);
    EXPECT_EQ(messagePD(*msg), L3PD::CallControl);
    EXPECT_EQ(messageMTI(*msg), L3Setup::MTI);
}

// GSM 04.08 10.3: PD=0x03(CC), TIO=7, TIF=1(REPL), messageType=101101(Release=0x2D), NSD=00
// Release MTI = 0x2D (TS 24.078).
// Byte 0: TI(3,high)|TIF(1)|PD(4,low) = 1111 0011 = 0xF3
// Byte 1: messageType(6)|NSD(2) = 0x2D|0 = 0x2D
TEST(CCRoundTripTest, Parse_Release_Hex) {
    auto msg = parseL3Hex("F32D");
    ASSERT_TRUE(msg);
    EXPECT_EQ(messagePD(*msg), L3PD::CallControl);
    EXPECT_EQ(messageMTI(*msg), L3Release::MTI);
}

// ── CC Facility (TS 24.008 §9.3.21, MTI=0x3a) ────────────────────────
// Wire layout per GSM 24.008 (CC Facility).
// Structure: PD=0x03(CC), TI=7, TIF=0, messageType=111010(Facility=0x3a)

TEST(CCRoundTripTest, Facility_RoundTrip) {
    ParsedMessage msg(CCM(L3Facility{}));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messagePD(*parsed), L3PD::CallControl);
    EXPECT_EQ(messageMTI(*parsed), L3Facility::MTI);
}

TEST(CCRoundTripTest, Facility_Parse_Golden) {
    // PD=0x03(CC), TI=7, TIF=0 -> byte0=0x3E, messageType=0x3a<<2=0xEA
    uint8_t data[] = {0xE3, 0x3A, 0x01, 0x02, 0x03};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3Facility::MTI);
    auto* fac = tryGet<L3Facility>(*msg);
    ASSERT_TRUE(fac);
    EXPECT_EQ(fac->facilityBody().size(), 3u);
}

// ── CC Notify (TS 24.078, MTI=0x3E) ───────────────────────────────────
// Structure: PD=0x03(CC), MT=0x3E in the six low bits of octet 1, body is
// the single cause octet.

TEST(CCRoundTripTest, Notify_RoundTrip) {
    ParsedMessage msg(CCM(L3CCNotify::builder().cause(CCCause::Normal_Call_Clearing).build()));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messagePD(*parsed), L3PD::CallControl);
    EXPECT_EQ(messageMTI(*parsed), L3CCNotify::MTI);
    auto* notify = tryGet<L3CCNotify>(*parsed);
    ASSERT_TRUE(notify);
    EXPECT_EQ(notify->cause(), CCCause::Normal_Call_Clearing);
}

// ── CC UnitData (TS 24.008 §9.3.16, MTI=0x27) ────────────────────────

TEST(CCRoundTripTest, UnitData_RoundTrip) {
    ParsedMessage msg(CCM(L3UnitData{}));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messagePD(*parsed), L3PD::CallControl);
    EXPECT_EQ(messageMTI(*parsed), L3UnitData::MTI);
}

// ── CC UnitDataAck (TS 24.008 §9.3.16a, MTI=0x28) ────────────────────

TEST(CCRoundTripTest, UnitDataAck_RoundTrip) {
    ParsedMessage msg(CCM(L3UnitDataAck{}));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messagePD(*parsed), L3PD::CallControl);
    EXPECT_EQ(messageMTI(*parsed), L3UnitDataAck::MTI);
}

// ── CC ErrorIndication (TS 24.008 §9.3.16b, MTI=0x2b) ────────────────

TEST(CCRoundTripTest, ErrorIndication_RoundTrip) {
    ParsedMessage msg(CCM(L3ErrorIndication{}));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messagePD(*parsed), L3PD::CallControl);
    EXPECT_EQ(messageMTI(*parsed), L3ErrorIndication::MTI);
}

TEST(CCRoundTripTest, ErrorIndication_Parse_Golden) {
    // Build via round-trip to get correct wire encoding, then parse
    L3ErrorIndication orig;
    orig.ti(7);
    ParsedMessage pm(CCM(std::move(orig)));
    auto hex = writeL3Hex(pm);
    ASSERT_TRUE(hex);
    auto msg = parseL3Hex(hex.value());
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3ErrorIndication::MTI);
    auto* ei = tryGet<L3ErrorIndication>(*msg);
    ASSERT_TRUE(ei);
}
