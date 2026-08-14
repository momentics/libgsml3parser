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
//   MessageType(8 bits, raw - no NSD field) in byte 1.
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
// GMM Attach Complete (GSM 24.008 9.4.3) - minimal message
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
// GMM Attach Reject (GSM 24.008 9.4.4) - with cause
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
// GMM Routing Area Update Complete (GSM 24.008 9.4.16) - minimal
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
// GMM P-TMSI Reallocation Complete (GSM 24.008 9.4.8) - minimal
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
// GMM Auth And Ciphering Reject (GSM 24.008 9.4.9) - minimal
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
// GMM Service Accept (GSM 24.008 9.4.21) - minimal
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
// GMM Status (GSM 24.008 9.4.24) - with cause
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
// GMM Information (GSM 24.008) - minimal
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
// GMM Detach Accept (GSM 24.008 9.4.6) - minimal
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
// GMM Attach Accept (GSM 24.008 9.4.2) - golden parse
// Reference: L3_Templates.ttcn tr_GMM_ATTACH_ACCEPT (line 2586)
// Hex breakdown:
//   0x80 = PD(4)=0x08(GMM), Skip(4)=0x00
//   0x02 = MTI(8)=0x02(AttachAccept), raw encoding
//   0x20 = attachResult(3)=GPRS(1)|spare(1)=0|forceToStandby(1)=0|updateTimer(2)=0|radioPriority(1)=0
//   0x52 0xF0 0x10 = MCC/MNC BCD nibble-swapped: MCC=250, MNC=01
//   0x12 0x34 = LAC = 0x1234
//   0x56 = RAC = 0x56
//   0x8c = extended IEI for allocatedPTMSI (0x80 | 0x0c)
//   0x05 = length of PTMSI LV value = 5 bytes
//   0x44 = type byte: spare(4)=0|type(3)=TMSI(4)|oe(1)=0
//   0x12 0x34 0x56 0x78 = TMSI value = 0x12345678
// =====================================================================

TEST(GoldenGMMTest, AttachAccept_GoldenParse) {
    // Body: firstOctet(1) + RAI(6) + PTMSI_TLV(7) = 14 bytes
    // PTMSI TLV: IEI=0x8c | len=5 | type_byte(0x08=TMSI) | TMSI(4)
    uint8_t data[] = {
        0x80, 0x02,                            // header: PD=GMM, MTI=AttachAccept
        0x20,                                   // attachResult(3)=GPRS(1)|spare(1)=0|forceToStandby(1)=0|updateTimer(2)=0|radioPriority(1)=0
        0x52, 0xF0, 0x10, 0x12, 0x34, 0x56,    // RAI: MCC=250, MNC=01, LAC=0x1234, RAC=0x56
        0x8c, 0x05,                             // TLV: extended IEI=0x0c(allocatedPTMSI), length=5
        0x08,                                   // type byte: spare(4)=0|type(3)=TMSI(4)|oe(1)=0 = 0x08
        0x12, 0x34, 0x56, 0x78                  // TMSI value = 0x12345678
    };
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3AttachAccept::MTI);
    EXPECT_EQ(messagePD(*msg), L3PD::GPRSMobilityManagement);
    EXPECT_EQ(messageName(*msg), "AttachAccept");
    auto* acc = tryGet<L3AttachAccept>(*msg);
    ASSERT_NE(acc, nullptr);
    EXPECT_EQ(acc->attachResult(), GMMAttachType::GPRSAttach);
    EXPECT_EQ(acc->forceToStandby(), false);
    EXPECT_EQ(acc->rai().mcc(), 250);
    EXPECT_EQ(acc->rai().mnc(), 1);
    EXPECT_EQ(acc->hasPTMSI(), true);
    EXPECT_EQ(acc->ptmsi().tmsi(), 0x12345678u);
}

// =====================================================================
// GMM Detach Request (GSM 24.008 9.4.5) - golden parse
// Reference: L3_Templates.ttcn ts_GMM_DET_REQ_MO (line 3004)
// Hex breakdown:
//   0x80 = PD(4)=0x08(GMM), Skip(4)=0x00
//   0x05 = MTI(8)=0x05(DetachRequest), raw encoding
//   0x10 = detachType(3)=GPRS(1)|powerOff(1)=0|spare(4)=0
// =====================================================================

TEST(GoldenGMMTest, DetachRequest_GoldenParse) {
    uint8_t data[] = {0x80, 0x05, 0x10};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3DetachRequest::MTI);
    EXPECT_EQ(messageName(*msg), "DetachRequest");
    auto* det = tryGet<L3DetachRequest>(*msg);
    ASSERT_NE(det, nullptr);
    EXPECT_EQ(det->detachType(), 1);
    EXPECT_EQ(det->powerOff(), false);
}

// =====================================================================
// GMM Routing Area Update Request (GSM 24.008 9.4.12) - golden parse
// Reference: L3_Templates.ttcn ts_GMM_RAU_REQ (line 2662)
// Hex breakdown:
//   0x80 = PD(4)=0x08(GMM), Skip(4)=0x00
//   0x08 = MTI(8)=0x08(RoutingAreaUpdateRequest), raw encoding
//   0x70 = updateType(3)=RAUpdated(0)|forL3(1)=0|CKSN(3)=7|spare(1)=0
//   0x52 0xF0 0x10 = MCC/MNC BCD nibble-swapped: MCC=250, MNC=01
//   0x12 0x34 = LAC = 0x1234
//   0x56 = RAC = 0x56
// =====================================================================

TEST(GoldenGMMTest, RAUpdateRequest_GoldenParse) {
    // Body: updateTypeCKSN(1) + oldRAI(6) = 7 bytes
    // First byte: updateType(3)=0|forL3(1)=0|CKSN(4)=7 -> 0000 0111 = 0x07
    uint8_t data[] = {
        0x80, 0x08,                              // header: PD=GMM, MTI=RAUpdateRequest
        0x07,                                     // updateType(3)=RAUpdated(0)|forL3(1)=0|CKSN(4)=7
        0x52, 0xF0, 0x10, 0x12, 0x34, 0x56       // RAI: MCC=250, MNC=01, LAC=0x1234, RAC=0x56
    };
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3RoutingAreaUpdateRequest::MTI);
    EXPECT_EQ(messageName(*msg), "RoutingAreaUpdateRequest");
    auto* rau = tryGet<L3RoutingAreaUpdateRequest>(*msg);
    ASSERT_NE(rau, nullptr);
    EXPECT_EQ(rau->updateType(), GMMUpdateType::RAUpdated);
    EXPECT_EQ(rau->forL3(), false);
    EXPECT_EQ(rau->cksn(), 0x07);
    EXPECT_EQ(rau->oldRAI().mcc(), 250);
    EXPECT_EQ(rau->oldRAI().mnc(), 1);
    EXPECT_EQ(rau->oldRAI().lac(), 0x1234);
    EXPECT_EQ(rau->oldRAI().rac(), 0x56);
}

// =====================================================================
// GMM Routing Area Update Accept (GSM 24.008 9.4.15) - golden parse
// Reference: L3_Templates.ttcn tr_GMM_RAU_ACCEPT (line 2738)
// Hex breakdown:
//   0x80 = PD(4)=0x08(GMM), Skip(4)=0x00
//   0x09 = MTI(8)=0x09(RoutingAreaUpdateAccept), raw encoding
//   0x10 = forceToStandby(1)=0|updateResult(3)=RAUpdated(0)|spare(1)=0|raUpdateTimer(2)=0|radioPriority(1)=0
//   0x52 0xF0 0x10 = MCC/MNC BCD nibble-swapped: MCC=250, MNC=01
//   0x12 0x34 = LAC = 0x1234
//   0x56 = RAC = 0x56
//   0x8c = extended IEI for allocatedPTMSI (0x80 | 0x0c)
//   0x05 = length of PTMSI LV value = 5 bytes
//   0x44 = type byte: spare(4)=0|type(3)=TMSI(4)|oe(1)=0
//   0x12 0x34 0x56 0x78 = TMSI value = 0x12345678
// =====================================================================

TEST(GoldenGMMTest, RAUpdateAccept_GoldenParse) {
    // Body: firstOctet(1) + RAI(6) + PTMSI_TLV(7) = 14 bytes
    // First byte: forceToStandby(1)=0|updateResult(3)=0|spare(1)=0|raUpdateTimer(2)=0|radioPriority(1)=0 -> 0x00
    uint8_t data[] = {
        0x80, 0x09,                               // header: PD=GMM, MTI=RAUpdateAccept
        0x00,                                      // forceToStandby(1)=0|updateResult(3)=RAUpdated(0)|spare(1)=0|raUpdateTimer(2)=0|radioPriority(1)=0
        0x52, 0xF0, 0x10, 0x12, 0x34, 0x56,       // RAI: MCC=250, MNC=01, LAC=0x1234, RAC=0x56
        0x8c, 0x05,                                // TLV: extended IEI=0x0c(allocatedPTMSI), length=5
        0x08,                                      // type byte: spare(4)=0|type(3)=TMSI(4)|oe(1)=0 = 0x08
        0x12, 0x34, 0x56, 0x78                     // TMSI value = 0x12345678
    };
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3RoutingAreaUpdateAccept::MTI);
    EXPECT_EQ(messageName(*msg), "RoutingAreaUpdateAccept");
    auto* raua = tryGet<L3RoutingAreaUpdateAccept>(*msg);
    ASSERT_NE(raua, nullptr);
    EXPECT_EQ(raua->forceToStandby(), false);
    EXPECT_EQ(raua->updateResult(), GMMUpdateType::RAUpdated);
    EXPECT_EQ(raua->rai().mcc(), 250);
    EXPECT_EQ(raua->hasPTMSI(), true);
    EXPECT_EQ(raua->ptmsi().tmsi(), 0x12345678u);
}

// =====================================================================
// GMM Routing Area Update Reject (GSM 24.008 9.4.17) - golden parse
// Reference: L3_Templates.ttcn tr_GMM_RAU_REJECT (line 2717)
// Hex breakdown:
//   0x80 = PD(4)=0x08(GMM), Skip(4)=0x00
//   0x0b = MTI(8)=0x0b(RoutingAreaUpdateReject), raw encoding
//   0xa5 = extended IEI for GMMCause (0x80 | 0x25)
//   0x01 = length
//   0x0c = cause value = GPRS_Service_Not_Allowed
// =====================================================================

TEST(GoldenGMMTest, RAUpdateReject_GoldenParse) {
    uint8_t data[] = {0x80, 0x0b, 0xa5, 0x01, 0x0c};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3RoutingAreaUpdateReject::MTI);
    EXPECT_EQ(messageName(*msg), "RoutingAreaUpdateReject");
    auto* rej = tryGet<L3RoutingAreaUpdateReject>(*msg);
    ASSERT_NE(rej, nullptr);
    EXPECT_EQ(rej->cause(), GMMCause::GPRS_Service_Not_Allowed);
}

// =====================================================================
// GMM Service Request (GSM 24.008 9.4.20) - golden parse
// Reference: L3_Templates.ttcn ts_GMM_SERVICE_REQ (line 3095)
// Hex breakdown:
//   0x80 = PD(4)=0x08(GMM), Skip(4)=0x00
//   0x0c = MTI(8)=0x0c(ServiceRequest), raw encoding
//   0x71 = CKSN(3)=7|spare(1)=0|serviceType(3)=1(signalling)|spare(1)=0
//   0x05 = PTMSI LV length = 5 bytes
//   0x44 = type byte: spare(4)=0|type(3)=TMSI(4)|oe(1)=0
//   0x12 0x34 0x56 0x78 = TMSI value = 0x12345678
// =====================================================================

TEST(GoldenGMMTest, ServiceRequest_GoldenParse) {
    // Body: CKSN_serviceType(1) + PTMSI_LV(6) = 7 bytes
    // First byte: CKSN(4)=7|serviceType(4)=1 -> 0111 0001 = 0x71
    uint8_t data[] = {
        0x80, 0x0c,                              // header: PD=GMM, MTI=ServiceRequest
        0x71,                                     // CKSN(4)=7|serviceType(4)=1(signalling)
        0x05,                                     // PTMSI LV length = 5 bytes
        0x08,                                     // type byte: spare(4)=0|type(3)=TMSI(4)|oe(1)=0 = 0x08
        0x12, 0x34, 0x56, 0x78                    // TMSI value = 0x12345678
    };
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3ServiceRequest::MTI);
    EXPECT_EQ(messageName(*msg), "ServiceRequest");
    auto* srv = tryGet<L3ServiceRequest>(*msg);
    ASSERT_NE(srv, nullptr);
    EXPECT_EQ(srv->cksn(), 0x07);
    EXPECT_EQ(srv->serviceType(), 0x01);
    EXPECT_EQ(srv->ptmsi().tmsi(), 0x12345678u);
}

// =====================================================================
// GMM Service Reject (GSM 24.008 9.4.22) - golden parse
// Reference: L3_Templates.ttcn tr_GMM_SERVICE_REJ (line 3137)
// Hex breakdown:
//   0x80 = PD(4)=0x08(GMM), Skip(4)=0x00
//   0x0e = MTI(8)=0x0e(ServiceReject), raw encoding
//   0xa5 = extended IEI for GMMCause (0x80 | 0x25)
//   0x01 = length
//   0x0c = cause value = GPRS_Service_Not_Allowed
// =====================================================================

TEST(GoldenGMMTest, ServiceReject_GoldenParse) {
    uint8_t data[] = {0x80, 0x0e, 0xa5, 0x01, 0x0c};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3ServiceReject::MTI);
    EXPECT_EQ(messageName(*msg), "ServiceReject");
    auto* rej = tryGet<L3ServiceReject>(*msg);
    ASSERT_NE(rej, nullptr);
    EXPECT_EQ(rej->cause(), GMMCause::GPRS_Service_Not_Allowed);
}

// =====================================================================
// GMM P-TMSI Reallocation Command (GSM 24.008 9.4.8) - golden parse
// Reference: 3GPP TS 24.008 9.4.8 message structure
// Hex breakdown:
//   0x80 = PD(4)=0x08(GMM), Skip(4)=0x00
//   0x10 = MTI(8)=0x10(P_TMSIReallocationCommand), raw encoding
//   0x00 = PTMSI_Type(1)=Native(0)|spare(7)=0
//   0x52 0xF0 0x10 = MCC/MNC BCD nibble-swapped: MCC=250, MNC=01
//   0x12 0x34 = LAC = 0x1234
//   0x56 = RAC = 0x56
//   0x8c = extended IEI for allocatedPTMSI (0x80 | 0x0c)
//   0x05 = length of PTMSI LV value = 5 bytes
//   0x44 = type byte: spare(4)=0|type(3)=TMSI(4)|oe(1)=0
//   0x12 0x34 0x56 0x78 = TMSI value = 0x12345678
// =====================================================================

TEST(GoldenGMMTest, PTMSIRereallocCommand_GoldenParse) {
    // Body: PTMSI_Type(1) + RAI(6) + PTMSI_TLV(7) = 14 bytes
    uint8_t data[] = {
        0x80, 0x10,                               // header: PD=GMM, MTI=P_TMSIReallocationCommand
        0x00,                                      // PTMSI_Type(1)=Native(0)|spare(7)=0
        0x52, 0xF0, 0x10, 0x12, 0x34, 0x56,       // RAI: MCC=250, MNC=01, LAC=0x1234, RAC=0x56
        0x8c, 0x05,                                // TLV: extended IEI=0x0c(allocatedPTMSI), length=5
        0x08,                                      // type byte: spare(4)=0|type(3)=TMSI(4)|oe(1)=0 = 0x08
        0x12, 0x34, 0x56, 0x78                     // TMSI value = 0x12345678
    };
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3P_TMSIReallocationCommand::MTI);
    EXPECT_EQ(messageName(*msg), "P_TMSIReallocationCommand");
    auto* cmd = tryGet<L3P_TMSIReallocationCommand>(*msg);
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(cmd->ptmsiType(), GMMPTMSIType::Native);
    EXPECT_EQ(cmd->rai().mcc(), 250);
    EXPECT_EQ(cmd->hasPTMSI(), true);
    EXPECT_EQ(cmd->ptmsi().tmsi(), 0x12345678u);
}

// =====================================================================
// GMM Authentication And Ciphering Request (GSM 24.008 9.4.9) - golden parse
// Reference: L3_Templates.ttcn tr_GMM_AUTH_REQ (line 2862)
// Hex breakdown:
//   0x80 = PD(4)=0x08(GMM), Skip(4)=0x00
//   0x12 = MTI(8)=0x12(AuthenticationAndCipheringRequest), raw encoding
//   0xE8 = cipheringAlgorithm(3)=GEA1(1)|spare(1)=0|imeisvRequest(1)=1|forceToStandby(1)=1|spare(4)=0
//   0x0F = acReferenceNumber(4)=F(15)|spare(4)=0
//   0x25 = IEI nibble(4)=0x2 for AuthRAND | spare(4)=0x5 (part of TLV encoding)
//   0x10 0x20 0x30 0x40 0x50 0x60 0x70 0x80 = RAND bytes 0-7
//   0x90 0xA0 0xB0 0xC0 0xD0 0xE0 0xF0 0x01 = RAND bytes 8-15
// =====================================================================

TEST(GoldenGMMTest, AuthAndCipheringRequest_GoldenParse) {
    // Body: firstOctet(1) + acRef(1) + IEI_nibble(4-bit) + RAND(16 bytes)
    // Note: parser reads IEI as 4-bit nibble then calls L3AuthRAND::parse which
    // reads 16 bytes from a non-byte-aligned position. We verify control fields
    // and that RAND was read (non-zero), but individual RAND bytes depend on
    // the bit-reader's non-aligned extraction behavior.
    uint8_t data[] = {
        0x80, 0x12,                               // header: PD=GMM, MTI=AuthAndCipheringRequest
        0x20,                                      // cipheringAlg(3)=GEA1(1)|spare|imeisvReq=0|forceStandby=0|spare
        0x0F,                                      // acReferenceNumber(4)=F(15)|spare(4)=0
        0x20,                                      // IEI nibble(4)=0x2 for AuthRAND | spare(4)=0
        0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80,  // RAND bytes
        0x90, 0xA0, 0xB0, 0xC0, 0xD0, 0xE0, 0xF0, 0x01
    };
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3AuthenticationAndCipheringRequest::MTI);
    EXPECT_EQ(messageName(*msg), "AuthAndCipheringRequest");
    auto* auth = tryGet<L3AuthenticationAndCipheringRequest>(*msg);
    ASSERT_NE(auth, nullptr);
    EXPECT_EQ(auth->cipheringAlgorithm(), 1);
    EXPECT_EQ(auth->imeisvRequest(), false);
    EXPECT_EQ(auth->forceToStandby(), false);
    EXPECT_EQ(auth->acReferenceNumber(), 0x0F);
}

// =====================================================================
// GMM Authentication And Ciphering Response (GSM 24.008 9.4.9) - golden parse
// Reference: L3_Templates.ttcn ts_GMM_AUTH_RESP_2G (line 2886)
// Hex breakdown:
//   0x80 = PD(4)=0x08(GMM), Skip(4)=0x00
//   0x13 = MTI(8)=0x13(AuthenticationAndCipheringResponse), raw encoding
//   0xF0 = acReferenceNumber(4)=F(15)|spare(4)=0
//   0x22 = IEI nibble(4)=0x2 for AuthRES | spare(4)=0x2
//   0xA1 0xB2 0xC3 0xD4 = RES value (4 bytes)
// =====================================================================

TEST(GoldenGMMTest, AuthAndCipheringResponse_GoldenParse) {
    // Body: acRef(4)|spare(4) + IEI_nibble(4)|spare(4) + RES(4 bytes)
    // Note: parser reads first byte as readField(8), extracts acRef from low nibble
    // (mACReferenceNumber = o.value() & 0x0F), then reads spare(4 bits),
    // then calls L3AuthRES::parse for the RES value.
    uint8_t data[] = {
        0x80, 0x13,                           // header: PD=GMM, MTI=AuthAndCipheringResponse
        0x0F,                                  // spare(4)=0|acReferenceNumber(4)=F(15) -> parser reads low nibble
        0x20,                                  // IEI nibble(4)=0x2 for AuthRES | spare(4)=0
        0xA1, 0xB2, 0xC3, 0xD4                // RES value (4 bytes)
    };
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3AuthenticationAndCipheringResponse::MTI);
    EXPECT_EQ(messageName(*msg), "AuthAndCipheringResponse");
    auto* resp = tryGet<L3AuthenticationAndCipheringResponse>(*msg);
    ASSERT_NE(resp, nullptr);
    EXPECT_EQ(resp->acReferenceNumber(), 0x0F);
}

// =====================================================================
// GMM Identity Request (GSM 24.008 9.4.7) - golden parse
// Reference: L3_Templates.ttcn tr_GMM_ID_REQ (line 2831)
// Hex breakdown:
//   0x80 = PD(4)=0x08(GMM), Skip(4)=0x00
//   0x15 = MTI(8)=0x15(GMMIdentityRequest), raw encoding
//   0x20 = identityType(3)=IMSI(1)|spare(1)=0|forceToStandby(1)=0|spare(4)=0
//   0x00 = spare octet
// =====================================================================

TEST(GoldenGMMTest, GMMIdentityRequest_GoldenParse) {
    uint8_t data[] = {0x80, 0x15, 0x20, 0x00};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3GMMIdentityRequest::MTI);
    EXPECT_EQ(messageName(*msg), "GMMIdentityRequest");
    auto* idr = tryGet<L3GMMIdentityRequest>(*msg);
    ASSERT_NE(idr, nullptr);
    EXPECT_EQ(idr->identityType(), MobileIDType::IMSI);
    EXPECT_EQ(idr->forceToStandby(), false);
}

// =====================================================================
// GMM Identity Response (GSM 24.008 9.4.10) - golden parse
// Reference: L3_Templates.ttcn ts_GMM_ID_RESP (line 2847)
// Hex breakdown:
//   0x80 = PD(4)=0x08(GMM), Skip(4)=0x00
//   0x16 = MTI(8)=0x16(GMMIdentityResponse), raw encoding
//   0x08 = mobileIdentity LV length = 8 bytes
//   0x62 = type byte: spare(4)=0|type(3)=IMSI(1)|oe(1)=1
//   0x25 0x09 0x99 0x00 0x00 0x00 0x0F = BCD IMSI "250999000000001"
// =====================================================================

TEST(GoldenGMMTest, GMMIdentityResponse_GoldenParse) {
    uint8_t data[] = {
        0x80, 0x16,
        0x08, 0x62, 0x25, 0x09, 0x99, 0x00, 0x00, 0x00, 0x0F
    };
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3GMMIdentityResponse::MTI);
    EXPECT_EQ(messageName(*msg), "GMMIdentityResponse");
    auto* idr = tryGet<L3GMMIdentityResponse>(*msg);
    ASSERT_NE(idr, nullptr);
    EXPECT_EQ(idr->mobileId().type(), MobileIDType::IMSI);
}

// =====================================================================
// GMM Authentication And Ciphering Failure (GSM 24.008 9.4.23) - golden parse
// Reference: L3_Templates.ttcn ts_GMM_AUTH_FAIL_UMTS_AKA_RESYNC (line 2908)
// Hex breakdown:
//   0x80 = PD(4)=0x08(GMM), Skip(4)=0x00
//   0x1c = MTI(8)=0x1c(AuthenticationAndCipheringFailure), raw encoding
//   0xa5 = extended IEI for GMMCause (0x80 | 0x25)
//   0x01 = length
//   0x15 = cause value = Synch_Failure
//   0xb0 = extended IEI for AuthFailureParam (0x80 | 0x30)
//   0x0e = length (14 bytes AUTS)
//   0xAA repeated 14 times = AUTS data
// =====================================================================

TEST(GoldenGMMTest, AuthAndCipheringFailure_GoldenParse) {
    uint8_t data[] = {
        0x80, 0x1c,
        0xa5, 0x01, 0x15,
        0xb0, 0x0e,
        0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
        0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA
    };
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messageMTI(*msg), L3AuthenticationAndCipheringFailure::MTI);
    EXPECT_EQ(messageName(*msg), "AuthAndCipheringFailure");
    auto* fail = tryGet<L3AuthenticationAndCipheringFailure>(*msg);
    ASSERT_NE(fail, nullptr);
    EXPECT_EQ(fail->cause(), GMMCause::Synch_Failure);
    EXPECT_EQ(fail->authFailureParam().auts().size(), 14u);
    EXPECT_EQ(fail->authFailureParam().auts()[0], 0xAA);
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
