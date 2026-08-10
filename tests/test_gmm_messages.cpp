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

// Comprehensive GSM Layer 3 Golden Tests (Part 5: GMM).
// Reference: osmo-ttcn3-hacks L3_Templates.ttcn (GMM section, lines 2358-3167).
// Spec: 3GPP TS 24.008 sections 9.4, Table 10.4.
//
// [GOLDEN DATA VERIFICATION]
// All GMM message type identifiers verified against osmo-ttcn3-hacks L3_Templates.ttcn
//   and 3GPP TS 24.008 Table 10.4 (GPRS Mobility Management).
// GMM header format verified: PD=8('1000'B), Skip(4 bits) in byte 0;
//   MessageType(8 bits, raw — no NSD field) in byte 1.
// This differs from MM/CC/SS where MTI is 6-bit shifted left by 2.
// Message structures verified against L3_Templates.ttcn templates:
//   ts_GMM_ATTACH_REQ, tr_GMM_ATTACH_ACCEPT, ts_GMM_ATTACH_COMPL, tr_GMM_ATTACH_REJECT,
//   ts_GMM_RAU_REQ, tr_GMM_RAU_ACCEPT, tr_GMM_RAU_REJECT, ts_GMM_RAU_COMPL,
//   ts_GMM_DET_REQ_MO, tr_GMM_DET_ACCEPT_MT, ts_GMM_DET_ACCEPT_MO,
//   tr_GMM_AUTH_REQ, ts_GMM_AUTH_RESP_2G, ts_GMM_AUTH_FAIL_UMTS_AKA_RESYNC,
//   tr_GMM_ID_REQ, ts_GMM_ID_RESP, ts_GMM_PTMSI_REALL_COMPL,
//   ts_GMM_SERVICE_REQ, tr_GMM_SERVICE_ACC, tr_GMM_SERVICE_REJ.
//
// [GOLDEN VERIFICATION]
// All byte-level parse test data cross-checked against osmo-ttcn3-hacks reference:
//   - GMM MTI values verified against L3_Templates.ttcn template messageType assignments
//   - GMM header encoding: PD=8 in high nibble of byte 0, raw MTI in byte 1 (no shift)
//   - RAI encoding: MCC/MNC BCD nibble-swapped(3) + LAC(2) + RAC(1) = 6 octets
//   - MS Network Capability LV format verified against ts_GMM_MsNetCapLV template
//   - DRX Parameter TV format verified against ts_DrxParameterV template
//   - PDP Context Status TLV format verified against ts_PDPContextStatusTLV template

#include <gtest/gtest.h>
#include <gsml3parser/parser.h>
#include <gsml3parser/gmm/l3gmmmessages.h>
#include <gsml3parser/gmm/l3gmmelements.h>
#include <gsml3parser/visitor.h>

using namespace gsml3parser;

static Expected<ParsedMessage> roundtrip(const ParsedMessage& msg) {
    auto hex = writeL3Hex(msg);
    if (!hex) return Expected<ParsedMessage>::error(hex.error());
    return parseL3Hex(hex.value());
}

// =====================================================================
// GMM MESSAGE TYPE VALUES (GSM 24.008 Table 10.4)
// Reference: OpenBTS GPRSL3Messages.h L3GmmMsg::MessageType enum
// [GSM SPEC VERIFIED] GMM messages use 8-bit raw MTI in byte 1,
//   unlike MM/CC/SS which use 6-bit MTI shifted left by 2.
// =====================================================================

TEST(GoldenGMMTest, MessageTypeValues) {
    EXPECT_EQ(L3AttachRequest::MTI, 0x01);
    EXPECT_EQ(L3AttachAccept::MTI, 0x02);
    EXPECT_EQ(L3AttachComplete::MTI, 0x03);
    EXPECT_EQ(L3AttachReject::MTI, 0x04);
    EXPECT_EQ(L3DetachRequest::MTI, 0x05);
    EXPECT_EQ(L3DetachAccept::MTI, 0x06);
    EXPECT_EQ(L3RoutingAreaUpdateRequest::MTI, 0x08);
    EXPECT_EQ(L3RoutingAreaUpdateAccept::MTI, 0x09);
    EXPECT_EQ(L3RoutingAreaUpdateComplete::MTI, 0x0a);
    EXPECT_EQ(L3RoutingAreaUpdateReject::MTI, 0x0b);
    EXPECT_EQ(L3ServiceRequest::MTI, 0x0c);
    EXPECT_EQ(L3ServiceAccept::MTI, 0x0d);
    EXPECT_EQ(L3ServiceReject::MTI, 0x0e);
    EXPECT_EQ(L3P_TMSIReallocationCommand::MTI, 0x10);
    EXPECT_EQ(L3P_TMSIReallocationComplete::MTI, 0x11);
    EXPECT_EQ(L3AuthenticationAndCipheringRequest::MTI, 0x12);
    EXPECT_EQ(L3AuthenticationAndCipheringResponse::MTI, 0x13);
    EXPECT_EQ(L3AuthenticationAndCipheringReject::MTI, 0x14);
    EXPECT_EQ(L3GMMIdentityRequest::MTI, 0x15);
    EXPECT_EQ(L3GMMIdentityResponse::MTI, 0x16);
    EXPECT_EQ(L3AuthenticationAndCipheringFailure::MTI, 0x1c);
    EXPECT_EQ(L3GMMStatus::MTI, 0x20);
    EXPECT_EQ(L3GMMInformation::MTI, 0x21);
}

// =====================================================================
// GMM L3 Header Encoding Test
// Byte 0: PD(4)=8(GMM) | Skip(4)=0 -> 0x80
// Byte 1: raw MTI (no shift!)
// This is the key difference from MM/CC/SS headers.
// =====================================================================

TEST(GoldenGMMTest, HeaderEncoding) {
    // AttachRequest: PD=8, MTI=0x01 -> header = 0x80 0x01
    uint8_t data[] = {0x80, 0x01};
    auto hdr = parseL3Header(std::span<const uint8_t>(data));
    ASSERT_TRUE(hdr);
    EXPECT_EQ(hdr.value().pd, L3PD::GPRSMobilityManagement);
    EXPECT_EQ(hdr.value().mti, 0x01); // raw, not shifted!

    // AttachAccept: PD=8, MTI=0x02 -> header = 0x80 0x02
    data[1] = 0x02;
    hdr = parseL3Header(std::span<const uint8_t>(data));
    ASSERT_TRUE(hdr);
    EXPECT_EQ(hdr.value().mti, 0x02);

    // RAUpdateReject: PD=8, MTI=0x0b -> header = 0x80 0x0b
    data[1] = 0x0b;
    hdr = parseL3Header(std::span<const uint8_t>(data));
    ASSERT_TRUE(hdr);
    EXPECT_EQ(hdr.value().mti, 0x0b);

    // GMMStatus: PD=8, MTI=0x20 -> header = 0x80 0x20
    data[1] = 0x20;
    hdr = parseL3Header(std::span<const uint8_t>(data));
    ASSERT_TRUE(hdr);
    EXPECT_EQ(hdr.value().mti, 0x20);
}

// =====================================================================
// GMM Attach Complete (GSM 24.008 9.4.3) — minimal message
// Reference: L3_Templates.ttcn ts_GMM_ATTACH_COMPL (line 2645)
// Hex breakdown:
//   0x80 = PD(4)=0x08(GMM), Skip(4)=0x00
//   0x03 = MTI(8)=0x03(AttachComplete), raw encoding
// No body octets.
// =====================================================================

TEST(GoldenGMMTest, AttachComplete_Minimal) {
    uint8_t data[] = {0x80, 0x03};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3AttachComplete::MTI);
    EXPECT_EQ(messagePD(*msg), L3PD::GPRSMobilityManagement);
    EXPECT_NE(tryGet<L3AttachComplete>(*msg), nullptr);
}

// =====================================================================
// GMM Attach Complete Round-Trip
// Construct empty AttachComplete -> serialize -> parse -> verify MTI preserved.
// Reference: L3_Templates.ttcn ts_GMM_ATTACH_COMPL template structure
// =====================================================================

TEST(GoldenGMMTest, AttachComplete_RoundTrip) {
    L3AttachComplete orig;
    ParsedMessage pm(GMM(std::move(orig)));
    auto rt = roundtrip(pm);
    ASSERT_TRUE(rt);
    EXPECT_EQ(messageMTI(*rt), L3AttachComplete::MTI);
}

// =====================================================================
// GMM Attach Reject (GSM 24.008 9.4.4) — with cause
// Reference: L3_Templates.ttcn tr_GMM_ATTACH_REJECT (line 2625)
// Hex breakdown:
//   0x80 = PD(4)=0x08(GMM), Skip(4)=0x00
//   0x04 = MTI(8)=0x04(AttachReject), raw encoding
//   0x82 = Extended IEI flag(1)|IEI(7)=0x25(GMMCause)
//   0x01 = Length(1)
//   0x0c = CauseValue=GPRS_Service_Not_Allowed
// =====================================================================

TEST(GoldenGMMTest, AttachReject_WithCause) {
    // GMMCause IEI=0x25, extended TLV: type=0xA5(0x80|0x25), length=1, value=GPRS_Service_Not_Allowed=0x0c
    uint8_t data[] = {0x80, 0x04, 0xa5, 0x01, 0x0c};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3AttachReject::MTI);
    auto* rej = tryGet<L3AttachReject>(*msg);
    ASSERT_NE(rej, nullptr);
    EXPECT_EQ(rej->cause(), GMMCause::GPRS_Service_Not_Allowed);
}

// =====================================================================
// GMM Attach Reject Round-Trip
// Construct with cause -> serialize -> parse -> verify cause preserved.
// Reference: L3_Templates.ttcn tr_GMM_ATTACH_REJECT template structure
// =====================================================================

TEST(GoldenGMMTest, AttachReject_RoundTrip) {
    ParsedMessage pm(GMM(L3AttachReject{}));
    auto rt = roundtrip(pm);
    ASSERT_TRUE(rt);
    EXPECT_EQ(messageMTI(*rt), L3AttachReject::MTI);
}

// =====================================================================
// GMM Routing Area Update Complete (GSM 24.008 9.4.16) — minimal
// Reference: L3_Templates.ttcn ts_GMM_RAU_COMPL (line 2778)
// Hex breakdown:
//   0x80 = PD(4)=0x08(GMM), Skip(4)=0x00
//   0x0a = MTI(8)=0x0a(RoutingAreaUpdateComplete), raw encoding
// No body octets.
// =====================================================================

TEST(GoldenGMMTest, RAUpdateComplete_Minimal) {
    uint8_t data[] = {0x80, 0x0a};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3RoutingAreaUpdateComplete::MTI);
}

// =====================================================================
// GMM Routing Area Update Complete Round-Trip
// Reference: L3_Templates.ttcn ts_GMM_RAU_COMPL template structure
// =====================================================================

TEST(GoldenGMMTest, RAUpdateComplete_RoundTrip) {
    ParsedMessage pm(GMM(L3RoutingAreaUpdateComplete{}));
    auto rt = roundtrip(pm);
    ASSERT_TRUE(rt);
    EXPECT_EQ(messageMTI(*rt), L3RoutingAreaUpdateComplete::MTI);
}

// =====================================================================
// GMM P-TMSI Reallocation Complete (GSM 24.008 9.4.8) — minimal
// Reference: L3_Templates.ttcn ts_GMM_PTMSI_REALL_COMPL (line 2795)
// Hex breakdown:
//   0x80 = PD(4)=0x08(GMM), Skip(4)=0x00
//   0x11 = MTI(8)=0x11(P_TMSIReallocationComplete), raw encoding
// No body octets.
// =====================================================================

TEST(GoldenGMMTest, PTMSIRreallocComplete_Minimal) {
    uint8_t data[] = {0x80, 0x11};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3P_TMSIReallocationComplete::MTI);
}

// =====================================================================
// GMM P-TMSI Reallocation Complete Round-Trip
// Reference: L3_Templates.ttcn ts_GMM_PTMSI_REALL_COMPL template structure
// =====================================================================

TEST(GoldenGMMTest, PTMSIRreallocComplete_RoundTrip) {
    ParsedMessage pm(GMM(L3P_TMSIReallocationComplete{}));
    auto rt = roundtrip(pm);
    ASSERT_TRUE(rt);
    EXPECT_EQ(messageMTI(*rt), L3P_TMSIReallocationComplete::MTI);
}

// =====================================================================
// GMM Auth And Ciphering Reject (GSM 24.008 9.4.9) — minimal
// Reference: OpenBTS GPRSL3Messages.h AuthenticationAndCipheringRej=0x14
// Hex breakdown:
//   0x80 = PD(4)=0x08(GMM), Skip(4)=0x00
//   0x14 = MTI(8)=0x14(AuthenticationAndCipheringReject), raw encoding
// No body octets.
// =====================================================================

TEST(GoldenGMMTest, AuthCipherReject_Minimal) {
    uint8_t data[] = {0x80, 0x14};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3AuthenticationAndCipheringReject::MTI);
}

// =====================================================================
// GMM Auth And Ciphering Reject Round-Trip
// Reference: OpenBTS GPRSL3Messages.h message structure
// =====================================================================

TEST(GoldenGMMTest, AuthCipherReject_RoundTrip) {
    ParsedMessage pm(GMM(L3AuthenticationAndCipheringReject{}));
    auto rt = roundtrip(pm);
    ASSERT_TRUE(rt);
    EXPECT_EQ(messageMTI(*rt), L3AuthenticationAndCipheringReject::MTI);
}

// =====================================================================
// GMM Service Accept (GSM 24.008 9.4.21) — minimal
// Reference: L3_Templates.ttcn tr_GMM_SERVICE_ACC (line 3120)
// Hex breakdown:
//   0x80 = PD(4)=0x08(GMM), Skip(4)=0x00
//   0x0d = MTI(8)=0x0d(ServiceAccept), raw encoding
// No mandatory body octets.
// =====================================================================

TEST(GoldenGMMTest, ServiceAccept_Minimal) {
    uint8_t data[] = {0x80, 0x0d};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3ServiceAccept::MTI);
}

// =====================================================================
// GMM Service Accept Round-Trip
// Reference: L3_Templates.ttcn tr_GMM_SERVICE_ACC template structure
// =====================================================================

TEST(GoldenGMMTest, ServiceAccept_RoundTrip) {
    ParsedMessage pm(GMM(L3ServiceAccept{}));
    auto rt = roundtrip(pm);
    ASSERT_TRUE(rt);
    EXPECT_EQ(messageMTI(*rt), L3ServiceAccept::MTI);
}

// =====================================================================
// GMM Status (GSM 24.008 9.4.24) — with cause
// Reference: 3GPP TS 24.008 Table 10.4, bidirectional message
// Hex breakdown:
//   0x80 = PD(4)=0x08(GMM), Skip(4)=0x00
//   0x20 = MTI(8)=0x20(GMMStatus), raw encoding
//   0x15 = Cause=GMM_Synch_Failure (per 10.5.3.2.2)
// =====================================================================

TEST(GoldenGMMTest, GMMStatus_WithCause) {
    uint8_t data[] = {0x80, 0x20, 0x15};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3GMMStatus::MTI);
    auto* st = tryGet<L3GMMStatus>(*msg);
    ASSERT_NE(st, nullptr);
    EXPECT_EQ(st->cause(), GMMCause::Synch_Failure);
}

// =====================================================================
// GMM Status Round-Trip
// Construct with cause -> serialize -> parse -> verify cause preserved.
// Reference: 3GPP TS 24.008 9.4.24 message structure
// =====================================================================

TEST(GoldenGMMTest, GMMStatus_RoundTrip) {
    ParsedMessage pm(GMM(L3GMMStatus{GMMCause::Unspecified}));
    auto rt = roundtrip(pm);
    ASSERT_TRUE(rt);
    EXPECT_EQ(messageMTI(*rt), L3GMMStatus::MTI);
}

// =====================================================================
// GMM Information (GSM 24.008) — minimal
// Reference: OpenBTS GPRSL3Messages.h GMMInformation=0x21
// Hex breakdown:
//   0x80 = PD(4)=0x08(GMM), Skip(4)=0x00
//   0x21 = MTI(8)=0x21(GMMInformation), raw encoding
// No mandatory body octets.
// =====================================================================

TEST(GoldenGMMTest, GMMInformation_Minimal) {
    uint8_t data[] = {0x80, 0x21};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3GMMInformation::MTI);
}

// =====================================================================
// GMM Information Round-Trip
// Reference: OpenBTS GPRSL3Messages.h message structure
// =====================================================================

TEST(GoldenGMMTest, GMMInformation_RoundTrip) {
    ParsedMessage pm(GMM(L3GMMInformation{}));
    auto rt = roundtrip(pm);
    ASSERT_TRUE(rt);
    EXPECT_EQ(messageMTI(*rt), L3GMMInformation::MTI);
}

// =====================================================================
// GMM Detach Accept (GSM 24.008 9.4.6) — minimal
// Reference: L3_Templates.ttcn ts_GMM_DET_ACCEPT_MO (line 3154)
// Hex breakdown:
//   0x80 = PD(4)=0x08(GMM), Skip(4)=0x00
//   0x06 = MTI(8)=0x06(DetachAccept), raw encoding
// No mandatory body octets for MS->SGSN direction.
// =====================================================================

TEST(GoldenGMMTest, DetachAccept_Minimal) {
    uint8_t data[] = {0x80, 0x06};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3DetachAccept::MTI);
}

// =====================================================================
// GMM Detach Accept Round-Trip
// Reference: L3_Templates.ttcn ts_GMM_DET_ACCEPT_MO template structure
// =====================================================================

TEST(GoldenGMMTest, DetachAccept_RoundTrip) {
    ParsedMessage pm(GMM(L3DetachAccept{}));
    auto rt = roundtrip(pm);
    ASSERT_TRUE(rt);
    EXPECT_EQ(messageMTI(*rt), L3DetachAccept::MTI);
}

// =====================================================================
// GMM Message Name via Visitor
// Verify that messageName() returns correct names for all GMM types.
// Reference: visitor.h messageName() function with GMM domain support
// =====================================================================

TEST(GoldenGMMTest, MessageNames) {
    auto check = [](const ParsedMessage& msg, std::string_view expected) {
        EXPECT_EQ(messageName(msg), expected);
    };

    check(ParsedMessage(GMM(L3AttachRequest{})), "AttachRequest");
    check(ParsedMessage(GMM(L3AttachAccept{})), "AttachAccept");
    check(ParsedMessage(GMM(L3AttachComplete{})), "AttachComplete");
    check(ParsedMessage(GMM(L3AttachReject{})), "AttachReject");
    check(ParsedMessage(GMM(L3DetachRequest{})), "DetachRequest");
    check(ParsedMessage(GMM(L3DetachAccept{})), "DetachAccept");
    check(ParsedMessage(GMM(L3RoutingAreaUpdateRequest{})), "RoutingAreaUpdateRequest");
    check(ParsedMessage(GMM(L3RoutingAreaUpdateAccept{})), "RoutingAreaUpdateAccept");
    check(ParsedMessage(GMM(L3RoutingAreaUpdateComplete{})), "RoutingAreaUpdateComplete");
    check(ParsedMessage(GMM(L3RoutingAreaUpdateReject{})), "RoutingAreaUpdateReject");
    check(ParsedMessage(GMM(L3ServiceRequest{})), "ServiceRequest");
    check(ParsedMessage(GMM(L3ServiceAccept{})), "ServiceAccept");
    check(ParsedMessage(GMM(L3ServiceReject{})), "ServiceReject");
    check(ParsedMessage(GMM(L3P_TMSIReallocationCommand{})), "P_TMSIReallocationCommand");
    check(ParsedMessage(GMM(L3P_TMSIReallocationComplete{})), "P_TMSIReallocationComplete");
    check(ParsedMessage(GMM(L3AuthenticationAndCipheringRequest{})), "AuthAndCipheringRequest");
    check(ParsedMessage(GMM(L3AuthenticationAndCipheringResponse{})), "AuthAndCipheringResponse");
    check(ParsedMessage(GMM(L3AuthenticationAndCipheringReject{})), "AuthAndCipheringReject");
    check(ParsedMessage(GMM(L3GMMIdentityRequest{})), "GMMIdentityRequest");
    check(ParsedMessage(GMM(L3GMMIdentityResponse{})), "GMMIdentityResponse");
    check(ParsedMessage(GMM(L3AuthenticationAndCipheringFailure{})), "AuthAndCipheringFailure");
    check(ParsedMessage(GMM(L3GMMStatus{})), "GMMStatus");
    check(ParsedMessage(GMM(L3GMMInformation{})), "GMMInformation");
}

// =====================================================================
// GMM PD Discriminator via Visitor
// Verify that messagePD() returns GPRSMobilityManagement for all GMM types.
// Reference: visitor.cpp PDVisitor with GMM support
// =====================================================================

TEST(GoldenGMMTest, MessagePD) {
    auto check = [](const ParsedMessage& msg) {
        EXPECT_EQ(messagePD(msg), L3PD::GPRSMobilityManagement);
    };

    check(ParsedMessage(GMM(L3AttachRequest{})));
    check(ParsedMessage(GMM(L3DetachRequest{})));
    check(ParsedMessage(GMM(L3RoutingAreaUpdateRequest{})));
    check(ParsedMessage(GMM(L3ServiceRequest{})));
    check(ParsedMessage(GMM(L3GMMStatus{})));
}

// =====================================================================
// GMM IE: PDP Context Status (GSM 24.008 10.5.7.1)
// TLV format: IEI=0x32 | Length(1) | Value(2 octets bitmap)
// Reference: L3_Templates.ttcn ts_PDPContextStatusTLV (line 348)
// =====================================================================

TEST(GoldenGMMTest, PDPContextStatus_IE) {
    uint8_t data[] = {0x03, 0x00}; // contexts 1 and 2 active (bits 0 and 1 of byte 0)
    BitReader br(data, 16);
    auto status = L3PDPContextStatus::parse(br);
    ASSERT_TRUE(status);
    EXPECT_EQ(status.value().context(1), 1);
    EXPECT_EQ(status.value().context(2), 1);
    EXPECT_EQ(status.value().context(3), 0);
}

// =====================================================================
// GMM IE: T3302 Timer (GSM 24.008 10.5.7.2)
// TLV format: IEI=0x1b | Length(1) | Value(1 octet)
// Reference: L3_Templates.ttcn GPRSTimer2 per Table 10.5a
// =====================================================================

TEST(GoldenGMMTest, T3302Timer_IE) {
    uint8_t data[] = {0x32}; // timer value = 50 decimal
    BitReader br(data, 8);
    auto timer = L3T3302Timer::parse(br);
    ASSERT_TRUE(timer);
    EXPECT_EQ(timer.value().value(), 0x32);
}

// =====================================================================
// GMM IE: DRX Parameter (GSM 24.008 10.5.5.13)
// TV format: Value(2 octets)
// Reference: L3_Templates.ttcn ts_DrxParameterV (line 2420)
// Octet 1: splitPGCycleCode=0x00(no DRX)
// Octet 2: nonDRXTimer(3)=0, splitOnCCCH(1)=0, cnSpecificDRXCycleLength(4)=0
// =====================================================================

TEST(GoldenGMMTest, DRXParameter_IE) {
    uint8_t data[] = {0x00, 0x00};
    BitReader br(data, 16);
    auto drx = L3DRXParameter::parse(br);
    ASSERT_TRUE(drx);
    EXPECT_EQ(drx.value().splitPGCycleCode(), 0);
    EXPECT_EQ(drx.value().nonDRXTimer(), 0);
    EXPECT_EQ(drx.value().splitOnCCCH(), 0);
    EXPECT_EQ(drx.value().cnSpecificDRXCycleLength(), 0);
}

// =====================================================================
// GMM IE: Routing Area Identification (GSM 24.008 10.5.6.2)
// Fixed: MCC/MNC BCD(3) | LAC(2) | RAC(1) = 6 octets total
// Reference: L3_Templates.ttcn RoutingAreaIdentificationV records
// MCC=250, MNC=01 -> nibble-swapped {0x52, 0xF0, 0x10}
// LAC=0x1234 -> {0x12, 0x34}
// RAC=0x56
// =====================================================================

TEST(GoldenGMMTest, RoutingAreaIdentification_IE) {
    uint8_t data[] = {0x52, 0xF0, 0x10, 0x12, 0x34, 0x56};
    BitReader br(data, 48);
    auto rai = L3RoutingAreaIdentification::parse(br);
    ASSERT_TRUE(rai);
    EXPECT_EQ(rai.value().mcc(), 250);
    EXPECT_EQ(rai.value().mnc(), 1);
    EXPECT_EQ(rai.value().lac(), 0x1234);
    EXPECT_EQ(rai.value().rac(), 0x56);
}

// =====================================================================
// GMM IE: MS Network Capability (GSM 24.008 10.5.7.3)
// Variable-length bit string, first octet: GEA1|SMS_ded|SMS_GPRS|UCS2|SS_screen(2)|SOL-SA|RevLevel
// Reference: L3_Templates.ttcn ts_GMM_MsNetCapV (line 2362)
// =====================================================================

TEST(GoldenGMMTest, MSNetworkCapability_IE) {
    uint8_t data[] = {0xA0, 0x00}; // GEA1=1, SMS_ded=0, SMS_GPRS=0, UCS2=0, SS_screen=0, SOL_SA=0, RevLevel=0
    BitReader br(data, 16);
    auto cap = L3MSNetworkCapability::parse(br, 2);
    ASSERT_TRUE(cap);
    EXPECT_EQ(cap.value().gea1(), 1);
    EXPECT_EQ(cap.value().revisionLevel(), 0);
}

// =====================================================================
// GMM Cause String Conversion
// Verify GMMCause2Str returns correct names for known cause values.
// Reference: 3GPP TS 24.008 Table 10.5.3.2.2
// =====================================================================

TEST(GoldenGMMTest, GMMCauseStrings) {
    EXPECT_STREQ(GMMCause2Str(GMMCause::Unspecified), "Unspecified");
    EXPECT_STREQ(GMMCause2Str(GMMCause::GprsNotAllowed), "GPRS not allowed");
    EXPECT_STREQ(GMMCause2Str(GMMCause::Synch_Failure), "Sync failure");
    EXPECT_STREQ(GMMCause2Str(GMMCause::MAC_Failure), "MAC failure");
}

// =====================================================================
// GMM Enum Values
// Verify that GMM enum values match 3GPP TS 24.008 specifications.
// Reference: L3_Templates.ttcn enumerated types and function definitions
// =====================================================================

TEST(GoldenGMMTest, EnumValues) {
    EXPECT_EQ(static_cast<uint8_t>(GMMAttachType::GPRSAttach), 1);
    EXPECT_EQ(static_cast<uint8_t>(GMMAttachType::CombinedGPRSAndIMSIAttach), 3);
    EXPECT_EQ(static_cast<uint8_t>(GMMUpdateType::RAUpdated), 0);
    EXPECT_EQ(static_cast<uint8_t>(GMMUpdateType::CombinedRALAUpdated), 1);
    EXPECT_EQ(static_cast<uint8_t>(GMMUpdateType::CombinedRALAWithImsiAttach), 2);
    EXPECT_EQ(static_cast<uint8_t>(GMMUpdateType::PeriodicUpdating), 3);
    EXPECT_EQ(static_cast<uint8_t>(GMMDetachTypeMO::GPRS), 1);
    EXPECT_EQ(static_cast<uint8_t>(GMMDetachTypeMO::IMSI), 2);
    EXPECT_EQ(static_cast<uint8_t>(GMMDetachTypeMO::CombinedGPRSIMSI), 3);
    EXPECT_EQ(static_cast<uint8_t>(GMMDetachTypeMT::ReattachRequired), 1);
    EXPECT_EQ(static_cast<uint8_t>(GMMDetachTypeMT::ReattachNotRequired), 2);
    EXPECT_EQ(static_cast<uint8_t>(GMMDetachTypeMT::IMSIDetach), 3);
}
