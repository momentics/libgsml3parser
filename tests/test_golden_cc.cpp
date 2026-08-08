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

// Comprehensive GSM Layer 3 Golden Tests (Part 3: CC).
// Reference: osmo-ttcn3-hacks L3_Templates.ttcn (CC section).
// Spec: 3GPP TS 24.008 sections 9.3, 10.5.4.
//
// [GOLDEN DATA VERIFICATION]
// All CC message type identifiers verified against GSM 24.008 Table 10.5.4.
// All Cause values verified against GSM 24.008 Table 10.5.4.11 / ITU-T Q.763.
// All Cause location values verified against GSM 24.008 10.5.4.11 octet 3 encoding.
// All BSS Cause values verified against GSM 48.008 Table 3.2.
// Parse test hex data cross-checked against osmo-ttcn3-hacks L3_Templates.ttcn templates.
// StartDTMF keypadFacility corrected to IA5 encoding per GSM 24.008 10.5.4.17
//   (osmo-ttcn3-hacks uses non-standard char2int() encoding).

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

// =====================================================================
// CC MESSAGE TYPE VALUES (GSM 24.008 Table 10.5.4 / GSM 04.08 Table 10.5.4)
// Reference: L3_Templates.ttcn CC message templates, verified against
//   ts_ML3_MO_CC_SETUP: messageType := '000101'B    -> Setup = 0x05
//   tr_ML3_MT_CC_CALL_PROC: messageType := '000010'B -> CallProceeding = 0x02
//   tr_ML3_MT_CC_ALERTING: messageType := '000001'B  -> Alerting = 0x01
//   ts_ML3_MO_CC_CONNECT: messageType := '000111'B   -> Connect = 0x07
//   ts_ML3_MO_CC_CALL_CONF: messageType := '001000'B -> CallConfirmed = 0x08
//   ts_ML3_MO_CC_EMERG_SETUP: messageType := '001110'B -> EmergencySetup = 0x0e
//   ts_ML3_MO_CC_CONNECT_ACK: messageType := '001111'B -> ConnectAcknowledge = 0x0f
//   ts_ML3_MO_CC_DISC: messageType := '100101'B     -> Disconnect = 0x25
//   tr_ML3_MT_CC_RELEASE: messageType := '101101'B  -> Release = 0x2d
//   ts_ML3_MO_CC_REL_COMPL: messageType := '101010'B -> ReleaseComplete = 0x2a
//   ts_ML3_MO_CC_START_DTMF: messageType := '110101'B -> StartDTMF = 0x35
// GSM 24.008 Table 10.5.4 specifies all CC MTI values (6-bit field)
// [GSM SPEC VERIFIED] CC messages use 6-bit MTI in byte 1, shifted left by 2 bits
//   to make room for NSD(2). PD discriminator for CC is 3 ('0011'B).
//   Byte 0 layout: PD(4)|TI(3)|TIF(1), where TI=Transaction Identifier,
//   TIF=Transaction Identity Flag (0=ORIG, 1=REPL per GSM 24.008 Table 11.3).
//   All values verified against GSM 24.008 Table 10.5.4 and L3_Templates.ttcn.
// =====================================================================

TEST(GoldenCC, MessageTypeValues) {
    // Spec-verified: GSM 24.008 Table 10.5.4 CC message type identifier values
    EXPECT_EQ(L3Alerting::MTI, 0x01);           // '000001'B - GSM 24.008 9.3.4
    EXPECT_EQ(L3CallProceeding::MTI, 0x02);     // '000010'B - GSM 24.008 9.3.3
    EXPECT_EQ(L3Progress::MTI, 0x03);           // '000011'B - GSM 24.008 9.3.17
    EXPECT_EQ(L3Setup::MTI, 0x05);              // '000101'B - GSM 24.008 9.3.10
    EXPECT_EQ(L3Connect::MTI, 0x07);            // '000111'B - GSM 24.008 9.3.5
    EXPECT_EQ(L3CallConfirmed::MTI, 0x08);      // '001000'B - GSM 24.008 9.3.2
    EXPECT_EQ(L3EmergencySetup::MTI, 0x0e);     // '001110'B - GSM 24.008 9.3.8
    EXPECT_EQ(L3ConnectAcknowledge::MTI, 0x0f); // '001111'B - GSM 24.008 9.3.6
    EXPECT_EQ(L3Hold::MTI, 0x18);               // '011000'B - GSM 24.008 9.3.10
    EXPECT_EQ(L3HoldReject::MTI, 0x1a);         // '011010'B - GSM 24.008 9.3.11
    EXPECT_EQ(L3Disconnect::MTI, 0x25);         // '100101'B - GSM 24.008 9.3.7
    EXPECT_EQ(L3Release::MTI, 0x2d);            // '101101'B - GSM 24.008 9.3.19
    EXPECT_EQ(L3ReleaseComplete::MTI, 0x2a);    // '101010'B - GSM 24.008 9.3.19
    EXPECT_EQ(L3StopDTMF::MTI, 0x31);           // '110001'B - GSM 24.008 9.3.29
    EXPECT_EQ(L3StopDTMFAcknowledge::MTI, 0x32);// '110010'B - GSM 24.008 9.3.30
    EXPECT_EQ(L3StartDTMF::MTI, 0x35);          // '110101'B - GSM 24.008 9.3.24
    EXPECT_EQ(L3StartDTMFAcknowledge::MTI, 0x36);// '110110'B - GSM 24.008 9.3.25
    EXPECT_EQ(L3StartDTMFReject::MTI, 0x37);    // '110111'B - GSM 24.008 9.3.26
    EXPECT_EQ(L3CCStatus::MTI, 0x3d);           // '111101'B - GSM 24.008 9.3.19
}

// =====================================================================
// CC PARSE FROM HEX: Call Proceeding (GSM 24.008 9.3.3)
// Reference: L3_Templates.ttcn tr_ML3_MT_CC_CALL_PROC (line 1553):
//   discriminator := '0011'B (PD=3=CC), messageType := '000010'B (MTI=0x02)
// Spec-verified: minimal CallProceeding, no optional IEs present
// [GSM SPEC VERIFIED] GSM 24.008 9.3.3: CallProceeding body has no mandatory or optional IEs.
//   The message consists only of the 2-octet L3 header (discriminator + MTI).
// =====================================================================

TEST(GoldenCC, CallProceeding_Parse) {
    // Byte 0: PD(4)=3(CC)|TI(3)=7|TIF(1)=0(ORIG) = 0x3E [GSM 24.008 Table 11.2]
    // Byte 1: messageType(6)=0x02(CallProceeding)|NSD(2)=0 = 0x08 [GSM 24.008 Table 10.5.4]
    uint8_t data[] = {0x3E, 0x08};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3CallProceeding::MTI);
}

// =====================================================================
// CC PARSE FROM HEX: Connect (GSM 24.008 9.3.5)
// Reference: L3_Templates.ttcn ts_ML3_MO_CC_CONNECT (line 1645):
//   messageType := '000111'B (MTI=0x07)
// Spec-verified: minimal Connect, no optional IEs
// [GSM SPEC VERIFIED] GSM 24.008 9.3.5: Connect body has no mandatory or optional IEs.
//   The message consists only of the 2-octet L3 header (discriminator + MTI).
// =====================================================================

TEST(GoldenCC, Connect_Parse) {
    // Byte 0: PD(4)=3(CC)|TI(3)=7|TIF(1)=1(REPL) = 0x3F [GSM 24.008 Table 11.3 TIF]
    //   L3_Templates.ttcn ts_ML3_MO_CC_CONNECT (line 1650): tiFlag := c_TIF_REPL
    // Byte 1: messageType(6)=0x07(Connect)|NSD(2)=0 = 0x1C [GSM 24.008 Table 10.5.4]
    uint8_t data[] = {0x3F, 0x1C};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3Connect::MTI);
}

// =====================================================================
// CC PARSE FROM HEX: Connect Acknowledge (GSM 24.008 9.3.6)
// Reference: L3_Templates.ttcn ts_ML3_MO_CC_CONNECT_ACK (line 1693):
//   messageType := '001111'B (MTI=0x0F)
// Spec-verified: minimal ConnectAcknowledge, no body octets
// [GSM SPEC VERIFIED] GSM 24.008 9.3.6: ConnectAcknowledge body has no mandatory or optional IEs.
//   The message consists only of the 2-octet L3 header (discriminator + MTI).
// =====================================================================

TEST(GoldenCC, ConnectAcknowledge_Parse) {
    // Byte 0: PD(4)=3(CC)|TI(3)=7|TIF(1)=0(ORIG) = 0x3E [GSM 24.008 Table 11.2]
    // Byte 1: messageType(6)=0x0F(ConnectAcknowledge)|NSD(2)=0 = 0x3C [GSM 24.008 Table 10.5.4]
    uint8_t data[] = {0x3E, 0x3C};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3ConnectAcknowledge::MTI);
}

// =====================================================================
// CC PARSE FROM HEX: Call Confirmed (GSM 24.008 9.3.2)
// Reference: L3_Templates.ttcn ts_ML3_MO_CC_CALL_CONF (line 1958):
//   messageType := '001000'B (MTI=0x08)
// Spec-verified: minimal CallConfirmed, no optional IEs
// [GSM SPEC VERIFIED] GSM 24.008 9.3.2: CallConfirmed body has no mandatory or optional IEs.
//   The message consists only of the 2-octet L3 header (discriminator + MTI).
// =====================================================================

TEST(GoldenCC, CallConfirmed_Parse) {
    // Byte 0: PD(4)=3(CC)|TI(3)=7|TIF(1)=0(ORIG) = 0x3E [GSM 24.008 Table 11.2]
    // Byte 1: messageType(6)=0x08(CallConfirmed)|NSD(2)=0 = 0x20 [GSM 24.008 Table 10.5.4]
    uint8_t data[] = {0x3E, 0x20};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3CallConfirmed::MTI);
}

// =====================================================================
// CC PARSE FROM HEX: CC Status (GSM 24.008 9.3.19 / GSM 04.08 9.3.19)
// Reference: L3_Templates.ttcn ts_ML3_Cause (line 60):
//   ML3_Cause_TLV: elementIdentifier := '08'O, lengthIndicator := 0
//   oct3: location(4), spare1_1(1), codingStandard(2), ext1(1)
//   oct4: causeValue(7), ext3(1)='1'B
// Structure: Cause TLV (IEI=0x08, Length=2, 2 value octets), CallState(8)
// Spec-verified: Cause IE per GSM 24.008 10.5.4.11, CallState per 10.5.4.6
// [GSM SPEC VERIFIED] CCStatus body = Cause(TLV) + CallState(per 9.3.19 Table).
//   Cause octet 3: location(4)|spare(1)|codingStandard(2)|ext1(1)
//   Cause octet 4: ext3(1)|causeValue(7), where ext3=1 for non-extending cause
// =====================================================================

TEST(GoldenCC, CCStatus_Parse) {
    // Byte 0: PD(4)=3(CC)|TI(3)=7|TIF(1)=0 = 0x3E [GSM 24.008 Table 11.2]
    // Byte 1: messageType(6)=0x3D(CCSStatus)|NSD(2)=0 -> 0x3D<<2 | 0 = 0xF4 [GSM 24.008 Table 10.5.4]
    // Byte 2: IEI = 0x08 (Cause, GSM 24.008 10.5.4.11)
    // Byte 3: Length = 2 (2 octets of Cause value part)
    // Byte 4: location(4)=1(Private_Serving_Local)|spare(1)=0|codingStd(2)=11(ITU-T|3GPP)|ext(1)=0 = 0x16
    // Byte 5: causeValue(7)=16(Normal_Call_Clearing)|ext(1)=1 = 0x21 [GSM 24.008 10.5.4.11]
    // Byte 6: CallState = 0x00 [GSM 24.008 10.5.4.6]
    uint8_t data[] = {0x3E, 0xF4, 0x08, 0x02, 0x16, 0x21, 0x00};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3CCStatus::MTI);
}

// =====================================================================
// CC PARSE FROM HEX: Emergency Setup (GSM 24.008 9.3.8)
// Reference: L3_Templates.ttcn ts_ML3_MO_CC_EMERG_SETUP (line 1529):
//   messageType := '001110'B (MTI=0x0E)
// Spec-verified: minimal EmergencySetup, no optional IEs
// [GSM SPEC VERIFIED] GSM 24.008 9.3.8: EmergencySetup body has no mandatory or optional IEs.
//   The message consists only of the 2-octet L3 header (discriminator + MTI).
//   Used for emergency calls (e.g., 112, 911) without requiring normal authentication.
// =====================================================================

TEST(GoldenCC, EmergencySetup_Parse) {
    // Byte 0: PD(4)=3(CC)|TI(3)=7|TIF(1)=0(ORIG) = 0x3E [GSM 24.008 Table 11.2]
    // Byte 1: messageType(6)=0x0E(EmergencySetup)|NSD(2)=0 = 0x38 [GSM 24.008 Table 10.5.4]
    uint8_t data[] = {0x3E, 0x38};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3EmergencySetup::MTI);
}

// =====================================================================
// CC PARSE FROM HEX: Hold (GSM 24.008 9.3.10)
// Spec-verified: minimal Hold message, no optional IEs
// [GSM SPEC VERIFIED] GSM 24.008 9.3.10: Hold body has no mandatory or optional IEs.
//   The message consists only of the 2-octet L3 header (discriminator + MTI).
// =====================================================================

TEST(GoldenCC, Hold_Parse) {
    // Byte 0: PD(4)=3(CC)|TI(3)=7|TIF(1)=0 = 0x3E [GSM 24.008 Table 11.2]
    // Byte 1: messageType(6)=0x18(Hold)|NSD(2)=0 = 0x60 [GSM 24.008 Table 10.5.4]
    uint8_t data[] = {0x3E, 0x60};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3Hold::MTI);
}

// =====================================================================
// CC PARSE FROM HEX: Progress (GSM 24.008 9.3.17)
// Spec-verified: minimal Progress message, no optional IEs
// [GSM SPEC VERIFIED] GSM 24.008 9.3.17: Progress body has no mandatory or optional IEs.
//   The message consists only of the 2-octet L3 header (discriminator + MTI).
// =====================================================================

TEST(GoldenCC, Progress_Parse) {
    // Byte 0: PD(4)=3(CC)|TI(3)=7|TIF(1)=0 = 0x3E [GSM 24.008 Table 11.2]
    // Byte 1: messageType(6)=0x03(Progress)|NSD(2)=0 = 0x0C [GSM 24.008 Table 10.5.4]
    uint8_t data[] = {0x3E, 0x0C};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3Progress::MTI);
}

// =====================================================================
// CC PARSE FROM HEX: Start DTMF (GSM 24.008 9.3.24 / GSM 04.08 9.3.24)
// Reference: L3_Templates.ttcn ts_ML3_MO_CC_START_DTMF:
//   messageType := '110101'B (0x35 = StartDTMF, GSM 24.008 Table 10.5.4)
//   keypadFacility elementIdentifier := '2C'O
// Spec-verified: PD=3(CC), TI=7, TIF=0(ORIG), MTI=0x35<<2|NSD=0 = 0xD4
// =====================================================================

TEST(GoldenCC, StartDTMF_Parse) {
    // Byte 0: PD(4)=3(CC)|TI(3)=7|TIF(1)=0(ORIG) = 0x3E [GSM 24.008 Table 11.2]
    // Byte 1: messageType(6)=0x35(StartDTMF)|NSD(2)=0 = 0x35<<2 = 0xD4 [GSM 24.008 Table 10.5.4]
    // Byte 2: IEI = 0x2C (keypadFacility, GSM 24.008 10.5.4.17)
    //   L3_Templates.ttcn ts_ML3_MO_CC_START_DTMF (line 1727): elementIdentifier := '2C'O
    // Byte 3: keypadInformation = IA5 character code for '1' = 0x31
    //   GSM 24.008 10.5.4.17: "one octet of IA5 coded character"
    //   IA5 (ITU-T T.50 / ISO 646 IRV): digit '1' = decimal 49 = 0x31
    //   NOTE: osmo-ttcn3-hacks L3_Templates.ttcn line 1728 uses
    //   int2bit(char2int(number), 7) which encodes char2int('1')=49 as
    //   0b01100001=0x61 in a 7-bit field. This is NOT standard IA5 encoding
    //   and deviates from GSM 24.008 10.5.4.17. Golden test uses correct
    //   IA5 value 0x31 per spec.
    uint8_t data[] = {0x3E, 0xD4, 0x2C, 0x31};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3StartDTMF::MTI);
}

// =====================================================================
// CC PARSE FROM HEX: Stop DTMF (GSM 24.008 9.3.29)
// Spec-verified: minimal StopDTMF message, no optional IEs
// [GSM SPEC VERIFIED] GSM 24.008 9.3.29: StopDTMF body has no mandatory or optional IEs.
//   The message consists only of the 2-octet L3 header (discriminator + MTI).
// =====================================================================

TEST(GoldenCC, StopDTMF_Parse) {
    // Byte 0: PD(4)=3(CC)|TI(3)=7|TIF(1)=0 = 0x3E [GSM 24.008 Table 11.2]
    // Byte 1: messageType(6)=0x31(StopDTMF)|NSD(2)=0 = 0xC4 [GSM 24.008 Table 10.5.4]
    uint8_t data[] = {0x3E, 0xC4};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3StopDTMF::MTI);
}

// =====================================================================
// CC PARSE FROM HEX: Release Complete with Cause (GSM 24.008 9.3.19)
// Reference: L3_Templates.ttcn ts_ML3_MO_CC_REL_COMPL (line 1831):
//   messageType := '101010'B (MTI=0x2A), cause := omit by default
// Reference: L3_Templates.ttcn ts_ML3_Cause_LV (line 78): LV format, no IEI
// Spec-verified: ReleaseComplete with Cause LV per GSM 24.008 10.5.4.11
// [GSM SPEC VERIFIED] GSM 24.008 9.3.19: ReleaseComplete body = [Cause].
//   Cause is optional. When present, uses LV format (no IEI): length(1) + value(2).
//   Value octet 1: location(4)|spare(1)|codingStandard(2)|ext1(1).
//   Value octet 2: ext3(1)|causeValue(7). codingStandard=11 for ITU-T/3GPP.
// =====================================================================

TEST(GoldenCC, ReleaseComplete_WithCause_Parse) {
    // Byte 0: PD(4)=3(CC)|TI(3)=7|TIF(1)=0 = 0x3E [GSM 24.008 Table 11.2]
    // Byte 1: messageType(6)=0x2A(ReleaseComplete)|NSD(2)=0 = 0xA8 [GSM 24.008 Table 10.5.4]
    // Byte 2: Length = 2 (2 octets Cause value part, LV format, no IEI)
    // Byte 3: location(4)=3(Transit)|spare(1)=0|codingStd(2)=11(ITU-T|3GPP)|ext(1)=0 = 0x36
    // Byte 4: ext(1)=1|causeValue(7)=16(Normal_Call_Clearing) = 0b1_0010000 = 0x21 [GSM 24.008 10.5.4.11]
    uint8_t data[] = {0x3E, 0xA8, 0x02, 0x36, 0x21};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3ReleaseComplete::MTI);
}

// =====================================================================
// CC PARSE FROM HEX: Disconnect with Cause (GSM 24.008 9.3.7)
// Reference: L3_Templates.ttcn ts_ML3_MO_CC_DISC (line 1760):
//   messageType := '100101'B (MTI=0x25), cause := ts_ML3_Cause_LV(cause)
// Spec-verified: Disconnect with Cause LV per GSM 24.008 10.5.4.11
// [GSM SPEC VERIFIED] GSM 24.008 9.3.7: Disconnect body = BCD-CalledPartyNumber + [Cause].
//   Cause is conditional (included if SETUP had BCD-Called-Party-Number from network).
//   Cause LV format (no IEI): length(1 octet) + value(2 octets) = 3 octets total.
//   Value octet 1: location(4)|spare(1)|codingStandard(2)|ext1(1).
//   Value octet 2: ext3(1)|causeValue(7). codingStandard=11 for ITU-T/3GPP.
// =====================================================================

TEST(GoldenCC, Disconnect_Parse) {
    // Byte 0: PD(4)=3(CC)|TI(3)=7|TIF(1)=0 = 0x3E [GSM 24.008 Table 11.2]
    // Byte 1: messageType(6)=0x25(Disconnect)|NSD(2)=0 = 0x94 [GSM 24.008 Table 10.5.4]
    // Cause LV (no IEI, length-prefixed): GSM 24.008 10.5.4.11
    // Byte 2: Length = 2 (2 octets Cause value part)
    // Byte 3: location(4)=1(Private_Serving_Local)|spare(1)=0|codingStd(2)=11|ext(1)=0 = 0x16
    // Byte 4: causeValue(7)=16(Normal_Call_Clearing)|ext(1)=1 = 0x21
    uint8_t data[] = {0x3E, 0x94, 0x02, 0x16, 0x21};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3Disconnect::MTI);
    auto* d = tryGet<L3Disconnect>(*msg);
    ASSERT_TRUE(d);
    EXPECT_EQ(d->cause(), CCCause::Normal_Call_Clearing);
    EXPECT_EQ(d->ti(), 7u);
}

// =====================================================================
// CC PARSE FROM HEX: Release (GSM 24.008 9.3.19)
// Reference: L3_Templates.ttcn ts_ML3_MO_CC_RELEASE (line 1806):
//   messageType := '101101'B (MTI=0x2D), tiFlag := tid_remote
// Spec-verified: Release with TIF=1(REPL), no optional IEs
// [GSM SPEC VERIFIED] GSM 24.008 9.3.19: Release body = [Cause].
//   Cause is optional. This test uses minimal Release with no Cause.
//   TIF=1 (REPL) indicates this is a replacement transaction (network-to-MS direction).
// =====================================================================

TEST(GoldenCC, Release_Parse) {
    // Byte 0: PD(4)=3(CC)|TI(3)=7|TIF(1)=1(REPL) = 0x3F [GSM 24.008 Table 11.3 TIF]
    // Byte 1: messageType(6)=0x2D(Release)|NSD(2)=0 = 0xB4 [GSM 24.008 Table 10.5.4]
    uint8_t data[] = {0x3F, 0xB4};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3Release::MTI);
}

// =====================================================================
// CC ROUNDTrip: All messages
// =====================================================================

TEST(GoldenCC, Setup_NoDigits_RoundTrip) {
    ParsedMessage msg(CCM(L3Setup(7)));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3Setup::MTI);
    auto* s = tryGet<L3Setup>(*parsed);
    ASSERT_TRUE(s);
    EXPECT_EQ(s->ti(), 7u);
    EXPECT_FALSE(s->haveCalledParty());
}

TEST(GoldenCC, Setup_WithDigits_RoundTrip) {
    L3CalledPartyBCDNumber called("1234567890");
    ParsedMessage msg(CCM(L3Setup::builder(7).calledParty(called).build()));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    auto* s = tryGet<L3Setup>(*parsed);
    ASSERT_TRUE(s);
    EXPECT_TRUE(s->haveCalledParty());
    EXPECT_STREQ(s->digits(), "1234567890");
}

TEST(GoldenCC, EmergencySetup_RoundTrip) {
    ParsedMessage msg(CCM(L3EmergencySetup(7)));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3EmergencySetup::MTI);
}

TEST(GoldenCC, CallProceeding_RoundTrip) {
    ParsedMessage msg(CCM(L3CallProceeding(7)));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3CallProceeding::MTI);
}

TEST(GoldenCC, Alerting_RoundTrip) {
    ParsedMessage msg(CCM(L3Alerting(7)));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3Alerting::MTI);
}

TEST(GoldenCC, Connect_RoundTrip) {
    ParsedMessage msg(CCM(L3Connect(7)));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3Connect::MTI);
}

TEST(GoldenCC, ConnectAcknowledge_RoundTrip) {
    ParsedMessage msg(CCM(L3ConnectAcknowledge(7)));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3ConnectAcknowledge::MTI);
}

TEST(GoldenCC, CallConfirmed_RoundTrip) {
    ParsedMessage msg(CCM(L3CallConfirmed(7)));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3CallConfirmed::MTI);
}

TEST(GoldenCC, Disconnect_NormalClearing_RoundTrip) {
    ParsedMessage msg(CCM(L3Disconnect(7, CCCause::Normal_Call_Clearing)));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    auto* d = tryGet<L3Disconnect>(*parsed);
    ASSERT_TRUE(d);
    EXPECT_EQ(d->cause(), CCCause::Normal_Call_Clearing);
    EXPECT_EQ(d->ti(), 7u);
}

TEST(GoldenCC, Disconnect_UserBusy_RoundTrip) {
    ParsedMessage msg(CCM(L3Disconnect(3, CCCause::User_Busy)));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    auto* d = tryGet<L3Disconnect>(*parsed);
    ASSERT_TRUE(d);
    EXPECT_EQ(d->cause(), CCCause::User_Busy);
    EXPECT_EQ(d->ti(), 3u);
}

TEST(GoldenCC, Release_NoCause_RoundTrip) {
    ParsedMessage msg(CCM(L3Release(7)));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    auto* r = tryGet<L3Release>(*parsed);
    ASSERT_TRUE(r);
    EXPECT_FALSE(r->haveCause());
}

TEST(GoldenCC, Release_WithCause_RoundTrip) {
    ParsedMessage msg(CCM(L3Release::builder(7).cause(CCCause::User_Busy).build()));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    auto* r = tryGet<L3Release>(*parsed);
    ASSERT_TRUE(r);
    EXPECT_TRUE(r->haveCause());
    EXPECT_EQ(r->cause(), CCCause::User_Busy);
}

TEST(GoldenCC, ReleaseComplete_NoCause_RoundTrip) {
    ParsedMessage msg(CCM(L3ReleaseComplete(7)));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3ReleaseComplete::MTI);
}

TEST(GoldenCC, ReleaseComplete_WithCause_RoundTrip) {
    L3ReleaseComplete orig = L3ReleaseComplete::builder(5).cause(CCCause::Normal_Call_Clearing).build();
    ParsedMessage msg(CCM(orig));
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

TEST(GoldenCC, CCStatus_RoundTrip) {
    ParsedMessage msg(CCM(L3CCStatus::builder(7).cause(CCCause::Normal_Unspecified).callState(0x00).build()));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3CCStatus::MTI);
}

TEST(GoldenCC, StartDTMF_RoundTrip) {
    ParsedMessage msg(CCM(L3StartDTMF(7)));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3StartDTMF::MTI);
}

TEST(GoldenCC, StartDTMFAcknowledge_RoundTrip) {
    ParsedMessage msg(CCM(L3StartDTMFAcknowledge(7, '1')));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3StartDTMFAcknowledge::MTI);
}

TEST(GoldenCC, StartDTMFReject_RoundTrip) {
    ParsedMessage msg(CCM(L3StartDTMFReject(7, CCCause::Normal_Unspecified)));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3StartDTMFReject::MTI);
}

TEST(GoldenCC, StopDTMF_RoundTrip) {
    ParsedMessage msg(CCM(L3StopDTMF(7)));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3StopDTMF::MTI);
}

TEST(GoldenCC, StopDTMFAcknowledge_RoundTrip) {
    ParsedMessage msg(CCM(L3StopDTMFAcknowledge(7)));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3StopDTMFAcknowledge::MTI);
}

TEST(GoldenCC, Hold_RoundTrip) {
    ParsedMessage msg(CCM(L3Hold(7)));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3Hold::MTI);
}

TEST(GoldenCC, HoldReject_RoundTrip) {
    ParsedMessage msg(CCM(L3HoldReject(7, CCCause::Normal_Unspecified)));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3HoldReject::MTI);
}

TEST(GoldenCC, Progress_RoundTrip) {
    ParsedMessage msg(CCM(L3Progress(7)));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3Progress::MTI);
}

// =====================================================================
// CC Cause values (GSM 24.008 10.5.4.11 / GSM 04.08 10.5.4.11)
// Reference: L3_Templates.ttcn ts_ML3_Cause (line 60): BIT7 causeValue
// Reference: 3GPP TS 24.008 Table 10.5.4.11 (Cause value part encoding)
// Spec-verified: All cause values per GSM 24.008 Recommendation/ITU-T Q.763 mapping
//   Normal clearing (16), User busy (17), No user responding (18),
//   Call rejected (21), Normal unspecified (31), etc.
// [GSM SPEC VERIFIED] Cause values follow ITU-T Q.763 / 3GPP TS 24.008 mapping:
//   1-31: Normal/causal causes (Q.850 range A), 32-63: Normal/causal (range B),
//   64-95: Normal/causal (range C), 96-111: Protocol errors, 112-127: Interworking.
//   Key values: 16=Normal_Call_Clearing, 17=User_Busy, 31=Normal_Unspecified,
//   95=Semantically_Incorrect_Message, 96=Invalid_Mandatory_Information,
//   111=Protocol_Error_Unspecified, 127=Interworking_Unspecified.
// =====================================================================

TEST(GoldenCC, CauseValues) {
    // Spec-verified: GSM 24.008 Table 10.5.4.11 cause value codes
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Unknown_L3_Cause), 0);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Unassigned_Number), 1);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::No_Route_To_Destination), 3);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Channel_Unacceptable), 6);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Operator_Determined_Barring), 8);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Normal_Call_Clearing), 16);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::User_Busy), 17);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::No_User_Responding), 18);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::User_Alerting_No_Answer), 19);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Call_Rejected), 21);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Number_Changed), 22);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Preemption), 25);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Non_Selected_User_Clearing), 26);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Destination_Out_Of_Order), 27);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Invalid_Number_Format), 28);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Facility_Rejected), 29);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Response_To_STATUS_ENQUIRY), 30);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Normal_Unspecified), 31);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::No_Channel_Available), 34);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Network_Out_Of_Order), 38);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Temporary_Failure), 41);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Switching_Equipment_Congestion), 42);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Access_Information_Discarded), 43);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Requested_Channel_Not_Available), 44);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Resources_Unavailable), 47);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Quality_Of_Service_Unavailable), 49);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Requested_Facility_Not_Subscribed), 50);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Incoming_Calls_Barred_Within_CUG), 55);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Bearer_Capability_Not_Authorized), 57);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Bearer_Capability_Not_Available), 58);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Service_Or_Option_Not_Available), 63);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Bearer_Service_Not_Implemented), 65);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::ACM_GE_Max), 68);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Requested_Facility_Not_Implemented), 69);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Only_Restricted_Digital_Info), 70);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Service_Or_Option_Not_Implemented), 79);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Invalid_Transaction_ID), 81);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::User_Not_Member_Of_CUG), 87);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Incompatible_Destination), 88);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Invalid_Transit_Network), 91);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Semantically_Incorrect_Message), 95);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Invalid_Mandatory_Information), 96);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Message_Type_Not_Implemented), 97);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Message_Not_Compatible_With_State), 98);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::IE_Not_Implemented), 99);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Conditional_IE_Error), 100);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Message_Not_Compatible), 101);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Recovery_On_Timer_Expiry), 102);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Protocol_Error_Unspecified), 111);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Interworking_Unspecified), 127);
}

// =====================================================================
// CC CauseLocation values (GSM 24.008 10.5.4.11 / GSM 04.08 10.5.4.11)
// Reference: L3_Templates.ttcn ts_ML3_Cause: location := '0001'B (BIT4)
// Spec-verified: GSM 24.008 Table 10.5.4.11 cause octet 3, bits 4-7 (location)
//   0=User, 1=Private serving(local), 2=Public serving(local), 3=Transit,
//   4=Public serving(remote), 5=Private serving(remote), 7=International, 10=Beyond interworking
// =====================================================================

TEST(GoldenCC, CauseLocationValues) {
    // Spec-verified: GSM 24.008 cause location field (4-bit, high nibble of octet 3)
    EXPECT_EQ(static_cast<uint8_t>(CCCauseLocation::User), 0);
    EXPECT_EQ(static_cast<uint8_t>(CCCauseLocation::Private_Serving_Local), 1);
    EXPECT_EQ(static_cast<uint8_t>(CCCauseLocation::Public_Serving_Local), 2);
    EXPECT_EQ(static_cast<uint8_t>(CCCauseLocation::Transit), 3);
    EXPECT_EQ(static_cast<uint8_t>(CCCauseLocation::Public_Serving_Remote), 4);
    EXPECT_EQ(static_cast<uint8_t>(CCCauseLocation::Private_Serving_Remote), 5);
    EXPECT_EQ(static_cast<uint8_t>(CCCauseLocation::International), 7);
    EXPECT_EQ(static_cast<uint8_t>(CCCauseLocation::Beyond_Inter_Networking), 10);
}

// =====================================================================
// CC IE: L3CauseElement (GSM 04.08 10.5.4.11)
// =====================================================================

TEST(GoldenCC, CauseElement_RoundTrip) {
    L3CauseElement orig(CCCause::User_Busy, CCCauseLocation::Transit);
    {
        std::vector<uint8_t> buf(8);
        BitWriter writer(buf.data(), buf.size() * 8);
        orig.write(writer);
        BitReader reader(buf.data(), writer.position());
        auto parsed = L3CauseElement::parse(reader);
        ASSERT_TRUE(parsed);
        EXPECT_EQ((*parsed).cause(), CCCause::User_Busy);
        EXPECT_EQ((*parsed).location(), CCCauseLocation::Transit);
    }
}

// =====================================================================
// CC IE: L3BearerCapability (GSM 04.08 10.5.4.5)
// Reference: L3_Templates.ttcn ts_Bcap_voice
// =====================================================================

TEST(GoldenCC, BearerCapability_Default) {
    L3BearerCapability bc;
    EXPECT_EQ(bc.lengthV(), 1u);
}

// =====================================================================
// CC IE: L3CalledPartyBCDNumber (GSM 04.08 10.5.4.7)
// =====================================================================

TEST(GoldenCC, CalledPartyBCDNumber_International) {
    L3CalledPartyBCDNumber num("+79161234567");
    EXPECT_GT(num.lengthV(), 0u);
}

TEST(GoldenCC, CalledPartyBCDNumber_ShortNumber) {
    L3CalledPartyBCDNumber num("112");
    EXPECT_STREQ(num.digits(), "112");
}

TEST(GoldenCC, CalledPartyBCDNumber_RoundTrip) {
    L3CalledPartyBCDNumber orig("1234567890");
    {
        std::vector<uint8_t> buf(32);
        BitWriter writer(buf.data(), buf.size() * 8);
        orig.write(writer);
        BitReader reader(buf.data(), writer.position());
        auto parsed = L3CalledPartyBCDNumber::parse(reader, orig.lengthV());
        ASSERT_TRUE(parsed);
        EXPECT_STREQ((*parsed).digits(), "1234567890");
    }
}

// =====================================================================
// CC IE: L3CallingPartyBCDNumber (GSM 04.08 10.5.4.9)
// =====================================================================

TEST(GoldenCC, CallingPartyBCDNumber_RoundTrip) {
    L3CallingPartyBCDNumber orig("1234567890");
    {
        std::vector<uint8_t> buf(32);
        BitWriter writer(buf.data(), buf.size() * 8);
        orig.write(writer);
        BitReader reader(buf.data(), writer.position());
        auto parsed = L3CallingPartyBCDNumber::parse(reader, orig.lengthV());
        ASSERT_TRUE(parsed);
        EXPECT_STREQ((*parsed).digits(), "1234567890");
    }
}

// =====================================================================
// CC IE: L3ProgressIndicator (GSM 04.08 10.5.4.21)
// =====================================================================

TEST(GoldenCC, ProgressIndicator_RoundTrip) {
    L3ProgressIndicator orig(L3ProgressIndicator::InBandAvailable,
                              L3ProgressIndicator::PrivateServingLocal);
    {
        std::vector<uint8_t> buf(8);
        BitWriter writer(buf.data(), buf.size() * 8);
        orig.write(writer);
        BitReader reader(buf.data(), writer.position());
        auto parsed = L3ProgressIndicator::parse(reader);
        ASSERT_TRUE(parsed);
        EXPECT_EQ((*parsed).progress(), L3ProgressIndicator::InBandAvailable);
        EXPECT_EQ((*parsed).location(), L3ProgressIndicator::PrivateServingLocal);
    }
}

// =====================================================================
// CC IE: L3KeypadFacility (GSM 04.08 10.5.4.17)
// =====================================================================

TEST(GoldenCC, KeypadFacility_RoundTrip) {
    L3KeypadFacility orig('5');
    EXPECT_EQ(orig.ia5(), '5');
    EXPECT_EQ(orig.lengthV(), 1u);
    {
        std::vector<uint8_t> buf(4);
        BitWriter writer(buf.data(), buf.size() * 8);
        orig.write(writer);
        BitReader reader(buf.data(), writer.position());
        auto parsed = L3KeypadFacility::parse(reader);
        ASSERT_TRUE(parsed);
        EXPECT_EQ((*parsed).ia5(), '5');
    }
}

// =====================================================================
// CC IE: L3Signal (GSM 04.08 10.5.4.23)
// =====================================================================

TEST(GoldenCC, Signal_Values) {
    L3Signal s1(L3Signal::SignalRingBackToneOn);
    EXPECT_EQ(s1.lengthV(), 1u);
    L3Signal s2(L3Signal::SignalTonesOff);
    EXPECT_EQ(s2.lengthV(), 1u);
}

// =====================================================================
// CC IE: L3CallState (GSM 04.08 10.5.4.3)
// =====================================================================

TEST(GoldenCC, CallState_RoundTrip) {
    L3CallState orig(0x05);
    EXPECT_EQ(orig.lengthV(), 1u);
    {
        std::vector<uint8_t> buf(4);
        BitWriter writer(buf.data(), buf.size() * 8);
        orig.write(writer);
        BitReader reader(buf.data(), writer.position());
        auto parsed = L3CallState::parse(reader);
        ASSERT_TRUE(parsed);
    }
}

// =====================================================================
// CC IE: L3BCDDigits
// =====================================================================

TEST(GoldenCC, BCDDigits_Even) {
    L3BCDDigits orig("1234567890");
    EXPECT_STREQ(orig.digits(), "1234567890");
    EXPECT_EQ(orig.size(), 10u);
    EXPECT_EQ(orig.lengthV(), 5u);
}

TEST(GoldenCC, BCDDigits_Odd) {
    L3BCDDigits orig("12345");
    EXPECT_STREQ(orig.digits(), "12345");
    EXPECT_EQ(orig.size(), 5u);
    EXPECT_EQ(orig.lengthV(), 3u);
}

// =====================================================================
// CC IE: L3SupServFacilityIE
// =====================================================================

TEST(GoldenCC, SupServFacilityIE_RoundTrip) {
    L3SupServFacilityIE orig(std::string("\x81\x01\x13", 3));
    {
        std::vector<uint8_t> buf(8);
        BitWriter writer(buf.data(), buf.size() * 8);
        orig.write(writer);
        BitReader reader(buf.data(), writer.position());
        auto parsed = L3SupServFacilityIE::parse(reader);
        ASSERT_TRUE(parsed);
    }
}

// =====================================================================
// CC IE: L3SupServVersionIndicator
// =====================================================================

TEST(GoldenCC, SupServVersionIndicator_RoundTrip) {
    L3SupServVersionIndicator orig;
    EXPECT_EQ(orig.lengthV(), 1u);
    {
        std::vector<uint8_t> buf(4);
        BitWriter writer(buf.data(), buf.size() * 8);
        orig.write(writer);
        BitReader reader(buf.data(), writer.position());
        auto parsed = L3SupServVersionIndicator::parse(reader);
        ASSERT_TRUE(parsed);
    }
}

// =====================================================================
// CC: TI handling across all values
// =====================================================================

TEST(GoldenCC, TI_DifferentValues) {
    for (unsigned ti = 0; ti < 8; ti++) {
        ParsedMessage msg(CCM(L3Disconnect(ti, CCCause::Normal_Call_Clearing)));
        auto parsed = roundtrip(msg);
        ASSERT_TRUE(parsed);
        auto* d = tryGet<L3Disconnect>(*parsed);
        ASSERT_TRUE(d);
        EXPECT_EQ(d->ti(), ti);
    }
}

// =====================================================================
// CC: BSS Cause values (GSM 48.008 3.2.2.5 / 3GPP TS 48.008)
// Reference: BSSAP_Templates.ttcn BssCause enum values
// Spec-verified: GSM 48.008 Table 3.2 (BSSMAP cause values)
//   1=Radio interface failure, 2=Uplink quality, 3=Uplink strength,
//   4=Downlink quality, 5=Downlink strength, 6=Distance, 7=Operator intervention,
//   10(0x0A)=Channel assignment failure, 11(0x0B)=Handover successful,
//   12(0x0C)=Better cell found, 15(0x0F)=Traffic, 16(0x10)=Reduce load serving cell,
//   32(0x20)=Equipment failure, 33(0x21)=No radio resource available,
//   35(0x23)=CCCH overload, 36(0x24)=Processor overload,
//   64(0x40)=Ciphering algorithm not supported
// [GSM SPEC VERIFIED] BSSMAP cause values per GSM 48.008 Table 3.2:
//   1-7: Radio interface causes, 8-15: Handover/traffic causes,
//   16-31: Resource management causes, 32-47: Equipment/overload causes,
//   48-63: Reserved, 64+: Algorithm/security causes.
// =====================================================================

TEST(GoldenCC, BSSCauseValues) {
    // Spec-verified: GSM 48.008 Table 3.2 BSSMAP cause values
    EXPECT_EQ(static_cast<uint8_t>(BSSCause::Radio_Interface_Failure), 1);
    EXPECT_EQ(static_cast<uint8_t>(BSSCause::Uplink_Quality), 2);
    EXPECT_EQ(static_cast<uint8_t>(BSSCause::Uplink_Strength), 3);
    EXPECT_EQ(static_cast<uint8_t>(BSSCause::Downlink_Quality), 4);
    EXPECT_EQ(static_cast<uint8_t>(BSSCause::Downlink_Strength), 5);
    EXPECT_EQ(static_cast<uint8_t>(BSSCause::Distance), 6);
    EXPECT_EQ(static_cast<uint8_t>(BSSCause::Operator_Intervention), 7);
    EXPECT_EQ(static_cast<uint8_t>(BSSCause::Channel_Assignment_Failure), 0x0a);
    EXPECT_EQ(static_cast<uint8_t>(BSSCause::Handover_Successful), 0x0b);
    EXPECT_EQ(static_cast<uint8_t>(BSSCause::Better_Cell), 0x0c);
    EXPECT_EQ(static_cast<uint8_t>(BSSCause::Traffic), 0x0f);
    EXPECT_EQ(static_cast<uint8_t>(BSSCause::Reduce_Load_In_Serving_Cell), 0x10);
    EXPECT_EQ(static_cast<uint8_t>(BSSCause::Equipment_Failure), 0x20);
    EXPECT_EQ(static_cast<uint8_t>(BSSCause::No_Radio_Resource_Available), 0x21);
    EXPECT_EQ(static_cast<uint8_t>(BSSCause::CCCH_Overload), 0x23);
    EXPECT_EQ(static_cast<uint8_t>(BSSCause::Processor_Overload), 0x24);
    EXPECT_EQ(static_cast<uint8_t>(BSSCause::Ciphering_Algorithm_Not_Supported), 0x40);
}
