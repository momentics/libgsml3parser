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

// SS message round-trip tests.
// Reference: osmo-ttcn3-hacks SS_Templates.ttcn.

#include <gtest/gtest.h>
#include <gsml3parser/parser.h>
#include <gsml3parser/ss/l3ssmessages.h>
#include <gsml3parser/visitor.h>

using namespace gsml3parser;

static Expected<ParsedMessage> roundtrip(const ParsedMessage& msg) {
    auto hex = writeL3Hex(msg);
    if (!hex) return Expected<ParsedMessage>::error(hex.error());
    return parseL3Hex(hex.value());
}

// ── Facility Message (GSM 04.80 2.3) ─────────────────────────────────
// Reference: SS_Templates.ttcn ts_SS_FACILITY_INVOKE, ts_SS_USSD_FACILITY_INVOKE

TEST(SSRoundTripTest, Facility_Empty) {
    ParsedMessage msg{SSM{L3SupServFacilityMessage{}}};
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messagePD(*parsed), L3PD::NonCallSS);
    EXPECT_EQ(messageMTI(*parsed), L3SupServFacilityMessage::MTI);
}

TEST(SSRoundTripTest, Facility_WithData) {
    ParsedMessage msg(SSM(L3SupServFacilityMessage(7, std::string("\x81\x01\x13", 3))));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3SupServFacilityMessage::MTI);
}

// GSM 04.08 10.2: PD=0x0B(NonCallSS), TIO=7, TIF=0, messageType=111010(Facility=0x3A), NSD=00
// Reference: GSML3SSMessages.h Facility=0x3A, SS_Templates.ttcn ts_SS_FACILITY_INVOKE
// Byte 0: PD(4,high) | TIO(3)+TIF(1,low) = 1011 1110 = 0xBE
// Byte 1: messageType(6)<<2 | NSD(2) = 0x3A<<2 | 0 = 0xE8
TEST(SSRoundTripTest, Facility_Parse) {
    uint8_t data[] = {0xBE, 0xE8};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messagePD(*msg), L3PD::NonCallSS);
    EXPECT_EQ(messageMTI(*msg), L3SupServFacilityMessage::MTI);
}

// ── Register Message (GSM 04.80 2.4 / 3GPP TS 24.080) ────────────────
// Reference: SS_Templates.ttcn ts_SS_REGISTER, REGISTER=0x3B
// GSM 24.008 Table 11.2: PD=0x0B(NonCallSS), discriminator = PD(4)|TI(3)|TIF(1)
// Message type octet: messageType(6)=0x3B(Register)|NSD(2)=0 -> 0xEC
// Spec-verified: Register message carries Facility TLV (IEI=0x1C) + optional VersionIndicator TV

TEST(SSRoundTripTest, Register_Empty) {
    // Construct default Register (TI=7), serialize -> parse -> verify MTI survives round-trip
    ParsedMessage msg{SSM{L3SupServRegisterMessage{}}};
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3SupServRegisterMessage::MTI);
}

TEST(SSRoundTripTest, Register_WithData) {
    // Register with TI=5 and 3-byte Facility data (TCAP INVOKE: opcode 0x81, length 1, value 0x0A)
    ParsedMessage msg(SSM(L3SupServRegisterMessage(5, std::string("\x81\x01\x0A", 3))));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3SupServRegisterMessage::MTI);
}

// ── Release Complete (GSM 04.80 2.5) ─────────────────────────────────

TEST(SSRoundTripTest, ReleaseComplete_Empty) {
    ParsedMessage msg{SSM{L3SupServReleaseCompleteMessage{}}};
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(messageMTI(*parsed), L3SupServReleaseCompleteMessage::MTI);
}

TEST(SSRoundTripTest, ReleaseComplete_WithCause) {
    L3SupServReleaseCompleteMessage orig(7, CCCause::Normal_Call_Clearing);
    ParsedMessage msg{SSM{orig}};
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    auto* rc = tryGet<L3SupServReleaseCompleteMessage>(*parsed);
    ASSERT_TRUE(rc);
    // Verify round-trip preserves cause by comparing serialized bytes
    auto hex1 = writeL3Hex(msg);
    auto hex2 = writeL3Hex(*parsed);
    ASSERT_TRUE(hex1 && hex2);
    EXPECT_EQ(hex1.value(), hex2.value());
}

// ── SS Message TI handling ───────────────────────────────────────────

TEST(SSRoundTripTest, TI_DifferentValues) {
    for (unsigned ti = 0; ti < 8; ti++) {
        ParsedMessage msg(SSM(L3SupServFacilityMessage(ti, std::string())));
        auto parsed = roundtrip(msg);
        ASSERT_TRUE(parsed);
        auto* s = tryGet<L3SupServFacilityMessage>(*parsed);
        ASSERT_TRUE(s);
        EXPECT_EQ(s->ti(), ti);
    }
}

// ── SS Op Codes from reference ───────────────────────────────────────
// Reference: SS_Templates.ttcn SS_Op_Code

TEST(SSRoundTripTest, SSOpcodes_Exist) {
    // These are TCAP opcodes defined in SS_Templates.ttcn
    // Our library doesn't parse TCAP internally, but the Facility IE carries raw data
    // Verify that the facility data can carry arbitrary TCAP content
    ParsedMessage msg(SSM(L3SupServFacilityMessage(7, std::string("\x81\x03\x3B\x01\x00", 5))));
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
}
