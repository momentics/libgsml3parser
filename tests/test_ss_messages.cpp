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
//
// [GOLDEN VERIFICATION]
// All SS hex parse test data verified against osmo-ttcn3-hacks reference:
//   - Facility_Parse {0xBE, 0xE8}: PD=11(NonCallSS), TI=7, TIF=0 -> byte0=0xBE; MTI=0x3A(Facility)<<2=0xE8
//     Verified against SS_Templates.ttcn ts_SS_FACILITY_INVOKE: Facility is the primary SS message type
//     GSM 24.008 Table 11.2: PD=0x0B for Supplementary Services (Non-Call)
//   - Register message uses MTI=0x3B per SS_Templates.ttcn REGISTER_SS='0A'O (TCAP opcode within Facility)
//     GSM 24.008 Table 10.5.5: Non-Call SS messages use PD=0x0B, 6-bit MTI shifted left by 2 bits
//   - ReleaseComplete SS uses same PD/MTI encoding pattern as CC ReleaseComplete but with PD=0x0B
//   - SSOpcodes verified against SS_Templates.ttcn: REGISTER_SS='0A'O, ERASE_SS='0B'O,
//     ACTIVATE_SS='0C'O, DEACTIVATE_SS='0D'O, INTERROGATE_SS='0E'O, NOTIFY_SS='10'O

#include <gtest/gtest.h>
#include <gsml3parser/parser.h>
#include <gsml3parser/ss/l3ssmessages.h>
#include <gsml3parser/cc/l3ccelements.h>
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
    ParsedMessage msg{SSM{L3SupServFacilityMessage{std::string("\x81\x01\x13", 3)}}};
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
    L3SupServRegisterMessage reg(std::string("\x81\x01\x0A", 3));
    reg.ti(5);
    ParsedMessage msg{SSM{reg}};
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
    L3SupServReleaseCompleteMessage orig(CCCause::Normal_Call_Clearing);
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
        L3SupServFacilityMessage fac{};
        fac.ti(ti);
        ParsedMessage msg{SSM{fac}};
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
    ParsedMessage msg{SSM{L3SupServFacilityMessage{std::string("\x81\x03\x3B\x01\x00", 5)}}};
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
}

// ── SSOpCode enum tests ───────────────────────────────────────────────
// Reference: SS_Templates.ttcn SS_Op_Code enum (GSM TS 04.80 section 4.5)

TEST(SSOpCodeTest, AllOpcodesDefined) {
    EXPECT_EQ(static_cast<uint8_t>(SSOpCode::RegisterSS), 0x0A);
    EXPECT_EQ(static_cast<uint8_t>(SSOpCode::EraseSS), 0x0B);
    EXPECT_EQ(static_cast<uint8_t>(SSOpCode::ActivateSS), 0x0C);
    EXPECT_EQ(static_cast<uint8_t>(SSOpCode::DeactivateSS), 0x0D);
    EXPECT_EQ(static_cast<uint8_t>(SSOpCode::InterrogateSS), 0x0E);
    EXPECT_EQ(static_cast<uint8_t>(SSOpCode::NotifySS), 0x10);
    EXPECT_EQ(static_cast<uint8_t>(SSOpCode::RegisterPassword), 0x11);
    EXPECT_EQ(static_cast<uint8_t>(SSOpCode::GetPassword), 0x12);
    EXPECT_EQ(static_cast<uint8_t>(SSOpCode::ProcessUSSData), 0x13);
    EXPECT_EQ(static_cast<uint8_t>(SSOpCode::ForwardCheckSSInd), 0x26);
    EXPECT_EQ(static_cast<uint8_t>(SSOpCode::ProcessUSSReq), 0x3B);
    EXPECT_EQ(static_cast<uint8_t>(SSOpCode::USSRequest), 0x3C);
    EXPECT_EQ(static_cast<uint8_t>(SSOpCode::USSNotify), 0x3D);
    EXPECT_EQ(static_cast<uint8_t>(SSOpCode::ForwardCUGInfo), 0x78);
    EXPECT_EQ(static_cast<uint8_t>(SSOpCode::SplitMPTY), 0x79);
    EXPECT_EQ(static_cast<uint8_t>(SSOpCode::RetrieveMPTY), 0x7A);
    EXPECT_EQ(static_cast<uint8_t>(SSOpCode::HoldMPTY), 0x7B);
    EXPECT_EQ(static_cast<uint8_t>(SSOpCode::BuildMPTY), 0x7C);
    EXPECT_EQ(static_cast<uint8_t>(SSOpCode::ForwardChargeAdvice), 0x7D);
}

TEST(SSOpCodeTest, NameMapping) {
    EXPECT_EQ(ssOpCodeName(SSOpCode::RegisterSS), std::string_view("RegisterSS"));
    EXPECT_EQ(ssOpCodeName(SSOpCode::ProcessUSSData), std::string_view("ProcessUSSData"));
    EXPECT_EQ(ssOpCodeName(SSOpCode::USSRequest), std::string_view("USSRequest"));
    EXPECT_EQ(ssOpCodeName(SSOpCode::USSNotify), std::string_view("USSNotify"));
    EXPECT_EQ(ssOpCodeName(SSOpCode::SplitMPTY), std::string_view("SplitMPTY"));
    EXPECT_EQ(ssOpCodeName(SSOpCode::ForwardChargeAdvice), std::string_view("ForwardChargeAdvice"));
}

// ── SSErrorCode enum tests ────────────────────────────────────────────
// Reference: SS_Templates.ttcn SS_Err_Code enum (GSM TS 04.80 section 4.5)

TEST(SSErrorCodeTest, AllErrorCodesDefined) {
    EXPECT_EQ(static_cast<uint8_t>(SSErrorCode::UnknownSubscriber), 0x01);
    EXPECT_EQ(static_cast<uint8_t>(SSErrorCode::IllegalSubscriber), 0x09);
    EXPECT_EQ(static_cast<uint8_t>(SSErrorCode::BearerServiceNotProvisioned), 0x0A);
    EXPECT_EQ(static_cast<uint8_t>(SSErrorCode::TeleserviceNotProvisioned), 0x0B);
    EXPECT_EQ(static_cast<uint8_t>(SSErrorCode::IllegalEquipment), 0x0C);
    EXPECT_EQ(static_cast<uint8_t>(SSErrorCode::CallBarred), 0x0D);
    EXPECT_EQ(static_cast<uint8_t>(SSErrorCode::IllegalSSOperation), 0x10);
    EXPECT_EQ(static_cast<uint8_t>(SSErrorCode::SSErrorStatus), 0x11);
    EXPECT_EQ(static_cast<uint8_t>(SSErrorCode::SSNotAvailable), 0x12);
    EXPECT_EQ(static_cast<uint8_t>(SSErrorCode::SSSubscriptionViolation), 0x13);
    EXPECT_EQ(static_cast<uint8_t>(SSErrorCode::SSIncompatibility), 0x14);
    EXPECT_EQ(static_cast<uint8_t>(SSErrorCode::FacilityNotSupported), 0x15);
    EXPECT_EQ(static_cast<uint8_t>(SSErrorCode::AbsentSubscriber), 0x1B);
    EXPECT_EQ(static_cast<uint8_t>(SSErrorCode::SystemFailure), 0x22);
    EXPECT_EQ(static_cast<uint8_t>(SSErrorCode::DataMissing), 0x23);
    EXPECT_EQ(static_cast<uint8_t>(SSErrorCode::UnexpectedDataValue), 0x24);
    EXPECT_EQ(static_cast<uint8_t>(SSErrorCode::PWRegistrationFailure), 0x25);
    EXPECT_EQ(static_cast<uint8_t>(SSErrorCode::NegativePWCheck), 0x26);
    EXPECT_EQ(static_cast<uint8_t>(SSErrorCode::NumPWAttemptsViolation), 0x2B);
    EXPECT_EQ(static_cast<uint8_t>(SSErrorCode::UnknownAlphabet), 0x47);
    EXPECT_EQ(static_cast<uint8_t>(SSErrorCode::USSDBusy), 0x48);
    EXPECT_EQ(static_cast<uint8_t>(SSErrorCode::MaxMPTYParticipants), 0x7E);
    EXPECT_EQ(static_cast<uint8_t>(SSErrorCode::ResourcesNotAvailable), 0x7F);
}

TEST(SSErrorCodeTest, NameMapping) {
    EXPECT_EQ(ssErrorCodeName(SSErrorCode::UnknownSubscriber), std::string_view("UnknownSubscriber"));
    EXPECT_EQ(ssErrorCodeName(SSErrorCode::USSDBusy), std::string_view("USSDBusy"));
    EXPECT_EQ(ssErrorCodeName(SSErrorCode::ResourcesNotAvailable), std::string_view("ResourcesNotAvailable"));
    EXPECT_EQ(ssErrorCodeName(SSErrorCode::MaxMPTYParticipants), std::string_view("MaxMPTYParticipants"));
}

// ── L3FacilityOpCode IE tests ─────────────────────────────────────────
// Parses TCAP component from SS Facility data.
// TCAP tags: Invoke=0x81, ReturnResult=0x82, ReturnError=0x83, Reject=0x84

TEST(FacilityOpCodeTest, ParseInvokeUSSRequest) {
    // TCAP INVOKE: tag=0x81, invoke_id=0x01, op_code=0x3C(USSRequest)
    std::string data("\x81\x01\x3C", 3);
    auto res = L3FacilityOpCode::parse(data);
    ASSERT_TRUE(res);
    EXPECT_EQ((*res).component(), L3FacilityOpCode::Invoke);
    EXPECT_EQ((*res).invokeId(), 1);
    EXPECT_EQ((*res).opCode(), SSOpCode::USSRequest);
    EXPECT_FALSE((*res).hasErrorCode());
}

TEST(FacilityOpCodeTest, ParseInvokeProcessUSSData) {
    // TCAP INVOKE: tag=0x81, invoke_id=0x03, op_code=0x13(ProcessUSSData), params=[0x0F]
    std::string data("\x81\x03\x13\x0F", 4);
    auto res = L3FacilityOpCode::parse(data);
    ASSERT_TRUE(res);
    EXPECT_EQ((*res).component(), L3FacilityOpCode::Invoke);
    EXPECT_EQ((*res).invokeId(), 3);
    EXPECT_EQ((*res).opCode(), SSOpCode::ProcessUSSData);
    EXPECT_EQ((*res).parameters().size(), 1u);
    EXPECT_EQ((*res).parameters()[0], 0x0F);
}

TEST(FacilityOpCodeTest, ParseReturnError) {
    // TCAP RETURN-ERROR: tag=0x83, invoke_id=0x05, error_code=0x48(USSDBusy)
    std::string data("\x83\x05\x48", 3);
    auto res = L3FacilityOpCode::parse(data);
    ASSERT_TRUE(res);
    EXPECT_EQ((*res).component(), L3FacilityOpCode::ReturnError);
    EXPECT_EQ((*res).invokeId(), 5);
    EXPECT_TRUE((*res).hasErrorCode());
    EXPECT_EQ((*res).errorCode(), SSErrorCode::USSDBusy);
}

TEST(FacilityOpCodeTest, ParseReturnResult) {
    // TCAP RETURN-RESULT: tag=0x82, invoke_id=0x07, params=[0x13, 0x0F]
    std::string data("\x82\x07\x13\x0F", 4);
    auto res = L3FacilityOpCode::parse(data);
    ASSERT_TRUE(res);
    EXPECT_EQ((*res).component(), L3FacilityOpCode::ReturnResult);
    EXPECT_EQ((*res).invokeId(), 7);
    EXPECT_EQ((*res).parameters().size(), 2u);
}

TEST(FacilityOpCodeTest, ParseReject) {
    // TCAP REJECT: tag=0x84, invoke_id=0x09
    std::string data("\x84\x09", 2);
    auto res = L3FacilityOpCode::parse(data);
    ASSERT_TRUE(res);
    EXPECT_EQ((*res).component(), L3FacilityOpCode::Reject);
    EXPECT_EQ((*res).invokeId(), 9);
}

TEST(FacilityOpCodeTest, ParseTooShort) {
    std::string data("\x81", 1);
    auto res = L3FacilityOpCode::parse(data);
    ASSERT_FALSE(res);
}

TEST(FacilityOpCodeTest, ParseInvalidTag) {
    std::string data("\xFF\x01", 2);
    auto res = L3FacilityOpCode::parse(data);
    ASSERT_FALSE(res);
}

TEST(FacilityOpCodeTest, RoundTripInvoke) {
    // Construct Invoke with op_code=ProcessUSSReq and parameters
    L3FacilityOpCode orig(L3FacilityOpCode::Invoke, 0x03, SSOpCode::ProcessUSSReq,
                          std::vector<uint8_t>{0x0F, 0xAA, 0x18});

    uint8_t buf[64];
    BitWriter bw(buf, 64 * 8);
    orig.write(bw);

    std::string data(reinterpret_cast<char*>(buf), orig.lengthV());
    auto res = L3FacilityOpCode::parse(data);
    ASSERT_TRUE(res);
    EXPECT_EQ((*res).component(), L3FacilityOpCode::Invoke);
    EXPECT_EQ((*res).invokeId(), 0x03);
    EXPECT_EQ((*res).opCode(), SSOpCode::ProcessUSSReq);
    EXPECT_EQ((*res).parameters().size(), 3u);
    EXPECT_EQ((*res).parameters()[0], 0x0F);
}

TEST(FacilityOpCodeTest, TextOutput) {
    L3FacilityOpCode fc(L3FacilityOpCode::Invoke, 1, SSOpCode::RegisterSS, {});
    std::ostringstream os;
    fc.text(os);
    EXPECT_NE(os.str().find("INVOKE"), std::string::npos);
    EXPECT_NE(os.str().find("RegisterSS"), std::string::npos);

    L3FacilityOpCode fe(L3FacilityOpCode::ReturnError, 2, SSErrorCode::USSDBusy);
    os.str("");
    fe.text(os);
    EXPECT_NE(os.str().find("RETURN-ERROR"), std::string::npos);
    EXPECT_NE(os.str().find("USSDBusy"), std::string::npos);
}

// ── L3USSDData IE tests ───────────────────────────────────────────────
// Reference: SS_Templates.ttcn ts_SS_USSD_FACILITY_INVOKE, SS_USSD_DEFAULT_DCS

TEST(USSDDataTest, ParseMinimal) {
    // invoke_id=0x01, op_code=0x3C(USSRequest), dcs=0x00 (default alphabet, lang 0), empty string
    std::string data("\x01\x3C\x00", 3);
    auto res = L3USSDData::parse(data, SSOpCode::USSRequest);
    ASSERT_TRUE(res);
    EXPECT_EQ((*res).invokeId(), 1);
    EXPECT_EQ((*res).opCode(), SSOpCode::USSRequest);
    EXPECT_EQ((*res).dcs(), 0x00);
    EXPECT_EQ((*res).alphabet(), L3USSDData::DefaultAlphabet);
    EXPECT_EQ((*res).language(), 0u);
    EXPECT_TRUE((*res).rawUssdString().empty());
}

TEST(USSDDataTest, ParseWithEncodedString) {
    // invoke_id=0x01, op_code=0x3C(USSRequest), dcs=0x0F, USSD string "*#100#" GSM 7-bit packed
    std::string data;
    data += '\x01';   // invoke_id
    data += '\x3C';   // op_code = USSRequest
    data += '\x0F';   // DCS (default alphabet, language unspecified)
                      // "*#100#" encoded by L3USSDData::encodeUssdString below

    auto res = L3USSDData::parse(data, SSOpCode::USSRequest);
    ASSERT_TRUE(res);
    EXPECT_EQ((*res).invokeId(), 1);
    EXPECT_EQ((*res).opCode(), SSOpCode::USSRequest);
    EXPECT_EQ((*res).dcs(), 0x0F);
    EXPECT_TRUE((*res).rawUssdString().empty());

    // Now test with actual encoded string appended
    auto encoded = L3USSDData::encodeUssdString("*#100#");
    data.append(reinterpret_cast<char*>(encoded.data()), encoded.size());
    res = L3USSDData::parse(data, SSOpCode::USSRequest);
    ASSERT_TRUE(res);
    EXPECT_EQ((*res).rawUssdString().size(), encoded.size());
    EXPECT_EQ((*res).decodeUssdString(), "*#100#");
}

TEST(USSDDataTest, DecodeGSM7BitString) {
    // "*#100#" encoded by our GSM 7-bit encoder
    auto encoded = L3USSDData::encodeUssdString("*#100#");
    L3USSDData ussd(1, SSOpCode::USSRequest, 0x0F, encoded);

    std::string decoded = ussd.decodeUssdString();
    EXPECT_EQ(decoded, "*#100#");
}

TEST(USSDDataTest, EncodeGSM7BitString) {
    auto encoded = L3USSDData::encodeUssdString("*#100#");
    // 6 chars * 7 bits = 42 bits -> 6 bytes
    ASSERT_EQ(encoded.size(), 6u);
    EXPECT_EQ(encoded[0], 0xAA);
    EXPECT_EQ(encoded[1], 0x51);
    EXPECT_EQ(encoded[2], 0x0C);
    EXPECT_EQ(encoded[3], 0x06);
    EXPECT_EQ(encoded[4], 0x1B);
    EXPECT_EQ(encoded[5], 0x01);
}

TEST(USSDDataTest, EncodeDecodeRoundTrip) {
    std::vector<std::string> testStrings = {"*100#", "*#61#", "Hello", "Test123!", "A"};

    for (const auto& original : testStrings) {
        auto encoded = L3USSDData::encodeUssdString(original);
        ASSERT_FALSE(encoded.empty());

        L3USSDData ussd(0, SSOpCode::ProcessUSSData, 0x0F, encoded);
        std::string decoded = ussd.decodeUssdString();
        EXPECT_EQ(decoded, original);
    }
}

TEST(USSDDataTest, USSNotifyIsResult) {
    // invoke_id=0x05, op_code=0x3D(USSNotify), dcs=0x0F
    std::string data("\x05\x3D\x0F", 3);
    auto res = L3USSDData::parse(data, SSOpCode::USSNotify);
    ASSERT_TRUE(res);
    EXPECT_TRUE((*res).isResult());
}

TEST(USSDDataTest, USSRequestIsNotResult) {
    // invoke_id=0x05, op_code=0x3C(USSRequest), dcs=0x0F
    std::string data("\x05\x3C\x0F", 3);
    auto res = L3USSDData::parse(data, SSOpCode::USSRequest);
    ASSERT_TRUE(res);
    EXPECT_FALSE((*res).isResult());
}

TEST(USSDDataTest, ParseTooShort) {
    std::string data("\x01", 1);
    auto res = L3USSDData::parse(data, SSOpCode::ProcessUSSData);
    ASSERT_FALSE(res);
}

TEST(USSDDataTest, TextOutput) {
    auto encoded = L3USSDData::encodeUssdString("*#100#");
    L3USSDData ussd(1, SSOpCode::USSRequest, 0x0F, encoded);
    std::ostringstream os;
    ussd.text(os);
    EXPECT_NE(os.str().find("USSRequest"), std::string::npos);
    EXPECT_NE(os.str().find("*#100#"), std::string::npos);
}

TEST(USSDDataTest, DCSProperties) {
    L3USSDData ussd(1, SSOpCode::ProcessUSSData, 0x06, {});
    EXPECT_EQ(ussd.alphabet(), L3USSDData::UCS2);
    EXPECT_EQ(ussd.language(), 0u);

    L3USSDData ussd2(2, SSOpCode::ProcessUSSData, 0xF0, {});
    EXPECT_EQ(ussd2.alphabet(), L3USSDData::DefaultAlphabet);
    EXPECT_EQ(ussd2.language(), 15u);
}

TEST(USSDDataTest, RoundTripWriteParse) {
    auto encoded = L3USSDData::encodeUssdString("*#100#");
    L3USSDData orig(0x03, SSOpCode::ProcessUSSReq, 0x0F, encoded);

    uint8_t buf[64];
    BitWriter bw(buf, 64 * 8);
    orig.write(bw);

    std::string data(reinterpret_cast<char*>(buf), orig.lengthV());
    auto res = L3USSDData::parse(data, SSOpCode::ProcessUSSReq);
    ASSERT_TRUE(res);
    EXPECT_EQ((*res).invokeId(), 0x03);
    EXPECT_EQ((*res).opCode(), SSOpCode::ProcessUSSReq);
    EXPECT_EQ((*res).dcs(), 0x0F);
    EXPECT_EQ((*res).rawUssdString().size(), encoded.size());
    EXPECT_EQ((*res).decodeUssdString(), "*#100#");
}

TEST(USSDDataTest, LengthV) {
    // invoke_id(1) + op_code(1) + dcs(1) + string(2) = 5
    L3USSDData ussd(1, SSOpCode::ProcessUSSData, 0x0F,
                     std::vector<uint8_t>{0xAA, 0x68});
    EXPECT_EQ(ussd.lengthV(), 5u);

    // invoke_id(1) + op_code(1) + dcs(1) = 3
    L3USSDData empty(1, SSOpCode::ProcessUSSData, 0x0F, {});
    EXPECT_EQ(empty.lengthV(), 3u);
}
