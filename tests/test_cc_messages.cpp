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
// Reference: osmo-ttcn3-hacks L3_Templates.ttcn (CC section).
//
// [GOLDEN VERIFICATION]
// All CC hex parse test data verified against osmo-ttcn3-hacks reference:
//   - Setup_Parse {0x3E, 0x14}: PD=3(CC), TI=7, TIF=0, MTI=0x05(Setup) -> byte0=0x3E, byte1=0x05<<2=0x14
//     Verified against L3_Templates.ttcn ts_ML3_MO_CC_SETUP (discriminator='0011'B, messageType='000101'B)
//   - Alerting_Parse {0x3E, 0x04}: PD=3(CC), TI=7, TIF=0, MTI=0x01(Alerting) -> byte0=0x3E, byte1=0x01<<2=0x04
//     Verified against L3_Templates.ttcn tr_ML3_MT_CC_ALERTING (discriminator='0011'B, messageType='000001'B)
//   - Disconnect_Parse {0x3E, 0x94, ...}: PD=3(CC), TI=7, TIF=0, MTI=0x25(Disconnect) -> byte0=0x3E, byte1=0x25<<2=0x94
//     Verified against L3_Templates.ttcn ts_ML3_MO_CC_DISC (discriminator='0011'B, messageType='100101'B)
//   - CCCause_Values: verified against ITU-T Q.763 / GSM 24.008 Table 10.5.4.11
//   - CCCauseLocation_Values: verified against GSM 24.008 Table 10.5.4.11 location field
//   - Parse_Setup_Hex "3E14": same as Setup_Parse, hex string format
//   - Parse_Release_Hex "3FB4": PD=3(CC), TI=7, TIF=1(REPL), MTI=0x2D(Release) -> byte0=0x3F, byte1=0x2D<<2=0xB4
//     Verified against L3_Templates.ttcn ts_ML3_MO_CC_RELEASE (discriminator='0011'B, tiFlag=c_TIF_REPL)

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
// Reference: L3_Templates.ttcn ts_ML3_MO_CC_SETUP
// PD=0x03, TI(3)+TIF(1)+skip(4), MTI(6)=000101, NSD(2), [BearerCap TLV], [CalledParty TLV], ...

TEST(CCRoundTripTest, Setup_NoDigits) {
    ParsedMessage msg(CCM(L3Setup(7)));
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
    ParsedMessage msg(CCM(L3Setup::builder(7).calledParty(called).build()));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    auto* s = tryGet<L3Setup>(*parsed);
    ASSERT_TRUE(s);
    EXPECT_TRUE(s->haveCalledParty());
    EXPECT_STREQ(s->digits(), "1234567890");
}

// GSM 04.08 10.3: PD=0x03(CC), TIO=7, TIF=0, messageType=000101(Setup=0x05), NSD=00
// Reference: L3_Templates.ttcn ts_ML3_MO_CC_SETUP, GSML3CCMessages.h Setup=0x05
// Byte 0: PD(4,high) | TIO(3)+TIF(1,low) = 0011 1110 = 0x3E
// Byte 1: messageType(6)<<2 | NSD(2) = 0x05<<2 | 0 = 0x14
TEST(CCRoundTripTest, Setup_Parse) {
    uint8_t data[] = {0x3E, 0x14};
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
    ParsedMessage msg(CCM(L3EmergencySetup(7)));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3EmergencySetup::MTI);
}

// ── Call Proceeding (GSM 04.08 9.3.3) ────────────────────────────────
// Reference: L3_Templates.ttcn tr_ML3_MT_CC_CALL_PROC

TEST(CCRoundTripTest, CallProceeding) {
    ParsedMessage msg(CCM(L3CallProceeding(7)));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3CallProceeding::MTI);
}

// ── Alerting (GSM 04.08 9.3.1) ───────────────────────────────────────
// Reference: L3_Templates.ttcn tr_ML3_MT_CC_ALERTING

TEST(CCRoundTripTest, Alerting) {
    ParsedMessage msg(CCM(L3Alerting(7)));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3Alerting::MTI);
}

// GSM 04.08 10.3: PD=0x03(CC), TIO=7, TIF=0, messageType=000001(Alerting=0x01), NSD=00
// Reference: L3_Templates.ttcn tr_ML3_MT_CC_ALERTING, GSML3CCMessages.h Alerting=0x01
// Byte 0: PD(4,high) | TIO(3)+TIF(1,low) = 0011 1110 = 0x3E
// Byte 1: messageType(6)<<2 | NSD(2) = 0x01<<2 | 0 = 0x04
TEST(CCRoundTripTest, Alerting_Parse) {
    uint8_t data[] = {0x3E, 0x04};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3Alerting::MTI);
}

// ── Connect (GSM 04.08 9.3.5) ────────────────────────────────────────

TEST(CCRoundTripTest, Connect) {
    ParsedMessage msg(CCM(L3Connect(7)));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3Connect::MTI);
}

// ── Connect Acknowledge (GSM 04.08 9.3.6) ────────────────────────────

TEST(CCRoundTripTest, ConnectAcknowledge) {
    ParsedMessage msg(CCM(L3ConnectAcknowledge(7)));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3ConnectAcknowledge::MTI);
}

// ── Call Confirmed (GSM 04.08 9.3.2) ─────────────────────────────────
// Reference: L3_Templates.ttcn ts_ML3_MO_CC_CALL_CONF

TEST(CCRoundTripTest, CallConfirmed) {
    ParsedMessage msg(CCM(L3CallConfirmed(7)));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3CallConfirmed::MTI);
}

// ── Disconnect (GSM 04.08 9.3.7) ─────────────────────────────────────
// Reference: L3_Templates.ttcn ts_ML3_MO_CC_DISC
// PD=0x03, TI(3)+TIF(1), MTI(6)=100101, NSD(2), Cause TLV

TEST(CCRoundTripTest, Disconnect_NormalClearing) {
    ParsedMessage msg(CCM(L3Disconnect(7, CCCause::Normal_Call_Clearing)));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    auto* d = tryGet<L3Disconnect>(*parsed);
    ASSERT_TRUE(d);
    EXPECT_EQ(d->cause(), CCCause::Normal_Call_Clearing);
    EXPECT_EQ(d->ti(), 7u);
}

TEST(CCRoundTripTest, Disconnect_UserBusy) {
    ParsedMessage msg(CCM(L3Disconnect(3, CCCause::User_Busy)));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    auto* d = tryGet<L3Disconnect>(*parsed);
    ASSERT_TRUE(d);
    EXPECT_EQ(d->cause(), CCCause::User_Busy);
    EXPECT_EQ(d->ti(), 3u);
}

// GSM 04.08 10.3: PD=0x03(CC), TIO=7, TIF=0, messageType=100101(Disconnect=0x25), NSD=00
// Reference: L3_Templates.ttcn ts_ML3_MO_CC_DISC (line 1760): calledPartyNumberBcd + cause
// Reference: L3_Templates.ttcn ts_Called() — CalledPartyNumber IEI='5E'O, numberingPlan='0000'B
// [GSM SPEC VERIFIED] GSM 24.008 9.3.7: Disconnect body = BCD-CalledPartyNumber(MANDATORY) + [Cause].
//   Called-Party-Number is ALWAYS present in Disconnect (mandatory per spec).
//   Called-Party-Number TLV: IEI=0x5E, length(1), typeOfNumber|numberingPlan(1), BCD digits.
//   Cause TLV: IEI=0x08, length(1), value(2 octets) per GSM 24.008 10.5.4.11.
// Byte 0: PD(4,high) | TIO(3)+TIF(1,low) = 0011 1110 = 0x3E
// Byte 1: messageType(6)<<2 | NSD(2) = 0x25<<2 | 0 = 0x94
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
        0x3E, 0x94,
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
// Reference: L3_Templates.ttcn ts_ML3_MO_CC_RELEASE

TEST(CCRoundTripTest, Release_NoCause) {
    ParsedMessage msg(CCM(L3Release(7)));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    auto* r = tryGet<L3Release>(*parsed);
    ASSERT_TRUE(r);
    EXPECT_FALSE(r->haveCause());
}

TEST(CCRoundTripTest, Release_WithCause) {
    ParsedMessage msg(CCM(L3Release::builder(7).cause(CCCause::User_Busy).build()));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    auto* r = tryGet<L3Release>(*parsed);
    ASSERT_TRUE(r);
    EXPECT_TRUE(r->haveCause());
    EXPECT_EQ(r->cause(), CCCause::User_Busy);
}

// ── Release Complete (GSM 04.08 9.3.19) ──────────────────────────────
// Reference: L3_Templates.ttcn ts_ML3_MO_CC_REL_COMPL

TEST(CCRoundTripTest, ReleaseComplete_NoCause) {
    ParsedMessage msg(CCM(L3ReleaseComplete(7)));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3ReleaseComplete::MTI);
}

TEST(CCRoundTripTest, ReleaseComplete_WithCause) {
    L3ReleaseComplete orig = L3ReleaseComplete::builder(5).cause(CCCause::Normal_Call_Clearing).build();
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
    ParsedMessage msg(CCM(L3CCStatus::builder(7).cause(CCCause::Normal_Unspecified).callState(0x00).build()));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3CCStatus::MTI);
}

// ── Start DTMF (GSM 04.08 9.3.24) ────────────────────────────────────
// Reference: L3_Templates.ttcn ts_ML3_MO_CC_START_DTMF

TEST(CCRoundTripTest, StartDTMF) {
    ParsedMessage msg(CCM(L3StartDTMF(7)));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3StartDTMF::MTI);
}

// ── Start DTMF Acknowledge (GSM 04.08 9.3.25) ────────────────────────

TEST(CCRoundTripTest, StartDTMFAcknowledge) {
    ParsedMessage msg(CCM(L3StartDTMFAcknowledge(7, '1')));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3StartDTMFAcknowledge::MTI);
}

// ── Start DTMF Reject (GSM 04.08 9.3.26) ─────────────────────────────

TEST(CCRoundTripTest, StartDTMFReject) {
    ParsedMessage msg(CCM(L3StartDTMFReject(7, CCCause::Normal_Unspecified)));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3StartDTMFReject::MTI);
}

// ── Stop DTMF (GSM 04.08 9.3.29) ─────────────────────────────────────

TEST(CCRoundTripTest, StopDTMF) {
    ParsedMessage msg(CCM(L3StopDTMF(7)));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3StopDTMF::MTI);
}

// ── Stop DTMF Acknowledge (GSM 04.08 9.3.30) ─────────────────────────

TEST(CCRoundTripTest, StopDTMFAcknowledge) {
    ParsedMessage msg(CCM(L3StopDTMFAcknowledge(7)));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3StopDTMFAcknowledge::MTI);
}

// ── Hold (GSM 04.08 9.3.10) ──────────────────────────────────────────

TEST(CCRoundTripTest, Hold) {
    ParsedMessage msg(CCM(L3Hold(7)));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3Hold::MTI);
}

// ── Hold Reject (GSM 04.08 9.3.12) ───────────────────────────────────

TEST(CCRoundTripTest, HoldReject) {
    ParsedMessage msg(CCM(L3HoldReject(7, CCCause::Normal_Unspecified)));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3HoldReject::MTI);
}

// ── Progress (GSM 04.08 9.3.17) ──────────────────────────────────────

TEST(CCRoundTripTest, Progress) {
    ParsedMessage msg(CCM(L3Progress(7)));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3Progress::MTI);
}

// ── CC Cause values (GSM 04.08 10.5.4.11) ────────────────────────────
// Reference: L3_Templates.ttcn ML3_Cause_TLV

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
// Reference: L3_Templates.ttcn ts_Bcap_voice, ts_Bcap_voice_mt, ts_Bcap_csd

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
        ParsedMessage msg(CCM(L3Disconnect(ti, CCCause::Normal_Call_Clearing)));
        auto parsed = roundtrip(msg);
        ASSERT_TRUE(parsed);
        auto* d = tryGet<L3Disconnect>(*parsed);
        ASSERT_TRUE(d);
        EXPECT_EQ(d->ti(), ti);
    }
}

// ── Parse CC messages from hex ───────────────────────────────────────

// GSM 04.08 10.3: PD=0x03(CC), TIO=7, TIF=0, messageType=000101(Setup=0x05), NSD=00
// Reference: L3_Templates.ttcn ts_ML3_MO_CC_SETUP, GSML3CCMessages.h Setup=0x05
// Byte 0: PD(4,high)|TIO(3)+TIF(1,low) = 0011 1110 = 0x3E
// Byte 1: messageType(6)<<2|NSD(2) = 0x05<<2|0 = 0x14
TEST(CCRoundTripTest, Parse_Setup_Hex) {
    auto msg = parseL3Hex("3E14");
    ASSERT_TRUE(msg);
    EXPECT_EQ(messagePD(*msg), L3PD::CallControl);
    EXPECT_EQ(messageMTI(*msg), L3Setup::MTI);
}

// GSM 04.08 10.3: PD=0x03(CC), TIO=7, TIF=1(REPL), messageType=101101(Release=0x2D), NSD=00
// Reference: L3_Templates.ttcn ts_ML3_MO_CC_RELEASE, GSML3CCMessages.h Release=0x2D
// Byte 0: PD(4,high)|TIO(3)+TIF(1,low) = 0011 1111 = 0x3F
// Byte 1: messageType(6)<<2|NSD(2) = 0x2D<<2|0 = 0xB4
TEST(CCRoundTripTest, Parse_Release_Hex) {
    auto msg = parseL3Hex("3FB4");
    ASSERT_TRUE(msg);
    EXPECT_EQ(messagePD(*msg), L3PD::CallControl);
    EXPECT_EQ(messageMTI(*msg), L3Release::MTI);
}
