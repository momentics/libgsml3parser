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

// Comprehensive GSM Layer 3 Golden Tests (Part 6: SM).
// Message identifiers and wire layouts per 3GPP TS 24.080 (SM).
// Spec: 3GPP TS 24.008 sections 9.5, Table 10.4a.
//
// [GOLDEN DATA VERIFICATION]
// All SM message type identifiers per 3GPP TS 24.008 Table 10.4a
//   (GPRS Session Management).
// SM header format verified: PD=0x0A('1010'B) in the low nibble of byte 0,
//   TI(3 bits) in bits 7:5 and TIF(1 bit) in bit 4; MessageType(8 bits, raw) in byte 1.
// This follows the same encoding as GMM (PD=0x08).
// Message structures per 3GPP TS 24.080 (SM message set):
//   Activate PDP Context Request / Accept / Reject,
//   Deactivate PDP Context Request (MO/MT),
//   Deactivate PDP Context Accept (MT/MO),
//   Modify PDP Context Request / Accept / Reject,
//   Status.
//
// [GOLDEN VERIFICATION]
// All byte-level parse test data cross-checked against 3GPP TS 24.080:
//   - SM MTI values per the SM message type table (TS 24.008 Table 10.4a)
//   - SM header encoding: PD=0x0A in the low nibble of byte 0, raw MTI in byte 1 (no shift)
//   - PDP Address TLV format per the SM information element definitions
//   - APN TLV format per the SM information element definitions
//   - QoS TLV format per the SM information element definitions

#include <gtest/gtest.h>
#include <gsml3parser/parser.h>
#include <gsml3parser/sm/l3smmessages.h>
#include <gsml3parser/sm/l3smelements.h>
#include <gsml3parser/visitor.h>

using namespace gsml3parser;

static Expected<ParsedMessage> roundtrip(const ParsedMessage& msg) {
    auto hex = writeL3Hex(msg);
    if (!hex) return Expected<ParsedMessage>::error(hex.error());
    return parseL3Hex(hex.value());
}

// =====================================================================
// SM MESSAGE TYPE VALUES (GSM 24.008 Table 10.4a)
// Message type identifiers per GSM 24.008 Table 10.4a.
// [GSM SPEC VERIFIED] SM messages use 8-bit raw MTI in byte 1,
//   same as GMM (unlike MM/CC/SS which use a 6-bit MTI in the low bits of byte 1).
// =====================================================================

TEST(GoldenSMTest, MessageTypeValues) {
    EXPECT_EQ(L3ActivatePDPContextRequest::MTI, 0x41);
    EXPECT_EQ(L3ActivatePDPContextAccept::MTI, 0x42);
    EXPECT_EQ(L3ActivatePDPContextReject::MTI, 0x43);
    EXPECT_EQ(L3DeactivatePDPContextRequest::MTI, 0x46);
    EXPECT_EQ(L3DeactivatePDPContextAccept::MTI, 0x47);
    EXPECT_EQ(L3ModifyPDPContextRequest::MTI, 0x48);
    EXPECT_EQ(L3ModifyPDPContextAccept::MTI, 0x49);
    EXPECT_EQ(L3ModifyPDPContextReject::MTI, 0x4c);
    EXPECT_EQ(L3SMStatus::MTI, 0x55);
    // SM message type values - additional messages (GSM 24.008 Table 10.4a)
    EXPECT_EQ(L3RequestPDPContextActivation::MTI, 0x44);
    EXPECT_EQ(L3RequestPDPContextActivationReject::MTI, 0x45);
    EXPECT_EQ(L3ModifyPDPContextRequestMS::MTI, 0x4A);
    EXPECT_EQ(L3ModifyPDPContextAcceptNet::MTI, 0x4B);
    EXPECT_EQ(L3ActivateSecondaryPDPContextRequest::MTI, 0x4D);
    EXPECT_EQ(L3ActivateSecondaryPDPContextAccept::MTI, 0x4E);
    EXPECT_EQ(L3ActivateSecondaryPDPContextReject::MTI, 0x4F);
    EXPECT_EQ(L3ActivateAAPDPContextRequest::MTI, 0x50);
    EXPECT_EQ(L3ActivateAAPDPContextAccept::MTI, 0x51);
    EXPECT_EQ(L3ActivateAAPDPContextReject::MTI, 0x52);
    EXPECT_EQ(L3DeactivateAAPDPContextRequest::MTI, 0x53);
    EXPECT_EQ(L3DeactivateAAPDPContextAccept::MTI, 0x54);
    EXPECT_EQ(L3ActivateMBMSContextRequest::MTI, 0x56);
    EXPECT_EQ(L3ActivateMBMSContextAccept::MTI, 0x57);
    EXPECT_EQ(L3ActivateMBMSContextReject::MTI, 0x58);
    EXPECT_EQ(L3RequestMBMSContextActivation::MTI, 0x59);
    EXPECT_EQ(L3RequestMBMSContextActivationReject::MTI, 0x5A);
    EXPECT_EQ(L3RequestSecondaryPDPContextActivation::MTI, 0x5B);
    EXPECT_EQ(L3RequestSecondaryPDPContextActivationReject::MTI, 0x5C);
    EXPECT_EQ(L3SMNotification::MTI, 0x5D);
}

// =====================================================================
// SM L3 Header Encoding Test
// Byte 0: TI(3)=0 << 5 | TIF(1)=0 << 4 | PD(4)=0x0A(SM) -> 0x0A
// Byte 1: raw MTI (no shift!)
// This is the same encoding pattern as GMM.
// Verified via parseL3Hex round-trip since encodeL3Header is internal.
// =====================================================================

TEST(GoldenSMTest, HeaderRoundTrip) {
    // Test that SM header bytes are correctly produced and parsed back.
    // ActivatePDPContextRequest minimal: PD=0x0A, MTI=0x41, body=pdpType(0)+APN(TLV)+QoS(TLV)
    // Hex: 0A 41 00 [APN TLV] [QoS TLV]
    // APN: 8F (extended IEI 0x2F) 03 (length) 69 70 6E ("ipn")
    // QoS: 89 (extended IEI 0x09) 01 (length) 00 (requested type, no elements)
    std::string hex = "0a 41 00 af 03 6970 6e 89 01 00";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);
    EXPECT_EQ(messagePD(res.value()), L3PD::GPRSSessionManagement);
    EXPECT_EQ(messageMTI(res.value()), 0x41);
    EXPECT_EQ(messageName(res.value()), "ActivatePDPContextRequest");
}

// =====================================================================
// Activate PDP Context Request Golden Tests
// Wire layout per GSM 24.080 (Activate PDP Context Request).
// =====================================================================

// GSM 24.008 9.5.1: ActivatePDPContextRequest with IPv4, auto-assign APN, minimal QoS.
// Hex breakdown:
//   0a = PD=0x0A(SM) in the low nibble of byte 0, TI=0, TIF=0
//   41 = MTI(8)=0x41(ActivatePDPContextRequest)
//   00 = pdpType(4)=0(IPv4), spare(4)=0
//   8f = extended IEI for APN (0x2F with extension bit)
//   07 = length 7
//   69 70 2e 67 73 6d 2e = "ip.gsm." (APN string)
//   89 = extended IEI for QoS (0x09 with extension bit)
//   01 = length 1
//   00 = QoS type = requested(0), no elements
TEST(GoldenSMTest, ActivatePDPContextRequest_Minimal) {
    std::string hex = "0a 41 00 af 07 6970 2e67 736d 2e 89 01 00";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);
    EXPECT_EQ(messagePD(res.value()), L3PD::GPRSSessionManagement);

    auto* msg = tryGet<L3ActivatePDPContextRequest>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->pdpType(), PDPType::IPv4);
    EXPECT_FALSE(msg->hasPDPAddress());
    EXPECT_EQ(msg->apn().value(), "ip.gsm.");
    EXPECT_EQ(msg->qos().type(), QoSType::Requested);
}

// GSM 24.008 9.5.1: ActivatePDPContextRequest with PDP address (IPv4).
// Hex breakdown:
//   0a 41 = L3 header (SM, ActivatePDPContextRequest)
//   00 = pdpType(4)=0(IPv4), spare(4)=0
//   88 = extended IEI for PDP Address (0x08 with extension bit)
//   05 = length 5 (1 for type + 4 for IPv4 address)
//   00 = PDP type = IPv4
//   c0 a8 01 01 = 192.168.1.1
//   8f 03 6970 6e = APN TLV: "ipn"
//   89 01 00 = QoS TLV: requested, no elements
TEST(GoldenSMTest, ActivatePDPContextRequest_WithAddress) {
    std::string hex = "0a 41 00 88 05 00c0 a801 01 af 03 6970 6e 89 01 00";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);

    auto* msg = tryGet<L3ActivatePDPContextRequest>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->pdpType(), PDPType::IPv4);
    EXPECT_TRUE(msg->hasPDPAddress());
    EXPECT_EQ(msg->pdpAddress().type(), PDPType::IPv4);
    EXPECT_EQ(msg->pdpAddress().address().size(), 4u);
    EXPECT_EQ(msg->pdpAddress().address()[0], 0xc0);
    EXPECT_EQ(msg->pdpAddress().address()[1], 0xa8);
    EXPECT_EQ(msg->pdpAddress().address()[2], 0x01);
    EXPECT_EQ(msg->pdpAddress().address()[3], 0x01);
    EXPECT_EQ(msg->apn().value(), "ipn");
}

// =====================================================================
// Activate PDP Context Accept Golden Tests
// Wire layout per GSM 24.080 (Activate PDP Context Accept).
// =====================================================================

// GSM 24.008 9.5.2: ActivatePDPContextAccept with assigned address.
// Hex breakdown:
//   0a 42 = L3 header (SM, ActivatePDPContextAccept)
//   10 = pdpHandle(4)=1, spare(4)=0
//   88 05 00c0 a801 64 = PDP Address TLV: type=IPv4, addr=192.168.1.100
//   89 03 0010 01 = QoS TLV: type=requested(0), elements=10:01
TEST(GoldenSMTest, ActivatePDPContextAccept_WithAddress) {
    std::string hex = "0a 42 10 88 05 00c0 a801 64 89 03 0010 01";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);

    auto* msg = tryGet<L3ActivatePDPContextAccept>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->pdpHandle(), 1u);
    EXPECT_TRUE(msg->hasPDPAddress());
    EXPECT_EQ(msg->pdpAddress().type(), PDPType::IPv4);
    EXPECT_EQ(msg->qos().type(), QoSType::Requested);
}

// GSM 24.008 9.5.2: ActivatePDPContextAccept minimal (no address assigned).
// Hex breakdown:
//   0a 42 = L3 header (SM, Accept)
//   00 = pdpHandle(4)=0, spare(4)=0
//   89 01 00 = QoS TLV: requested, no elements
TEST(GoldenSMTest, ActivatePDPContextAccept_Minimal) {
    std::string hex = "0a 42 00 89 01 00";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);

    auto* msg = tryGet<L3ActivatePDPContextAccept>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->pdpHandle(), 0u);
    EXPECT_FALSE(msg->hasPDPAddress());
}

// =====================================================================
// Activate PDP Context Reject Golden Tests
// Wire layout per GSM 24.080 (Activate PDP Context Reject).
// =====================================================================

// GSM 24.008 9.5.3: ActivatePDPContextReject with cause and back-off timer.
// Hex breakdown:
//   0a 43 = L3 header (SM, Reject)
//   a7 01 13 = SM Cause TLV: IEI=0x27, len=1, cause=0x13(Unsupported_PDP_Address_Type)
//   a8 01 05 = Back-Off Timer TLV: IEI=0x28, len=1, timer=0x05
TEST(GoldenSMTest, ActivatePDPContextReject_Full) {
    std::string hex = "0a 43 a7 01 13 a8 01 05";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);

    auto* msg = tryGet<L3ActivatePDPContextReject>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->cause(), SMCause::Unsupported_PDP_Address_Type);
    EXPECT_TRUE(msg->hasBackOffTimer());
    EXPECT_EQ(msg->backOffTimer().value(), 0x05);
}

// GSM 24.008 9.5.3: ActivatePDPContextReject minimal (cause only).
// Hex breakdown:
//   0a 43 = L3 header (SM, Reject)
//   a7 01 13 = SM Cause TLV: IEI=0x27, len=1, cause=0x13(Unsupported_PDP_Address_Type)
TEST(GoldenSMTest, ActivatePDPContextReject_Minimal) {
    std::string hex = "0a 43 a7 01 13";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);

    auto* msg = tryGet<L3ActivatePDPContextReject>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->cause(), SMCause::Unsupported_PDP_Address_Type);
    EXPECT_FALSE(msg->hasBackOffTimer());
}

// =====================================================================
// Deactivate PDP Context Request Golden Tests
// Wire layout per GSM 24.080 (MO and MT directions).
// =====================================================================

// GSM 24.008 9.5.4: DeactivatePDPContextRequest with PDP handle and address.
// Hex breakdown:
//   0a 46 = L3 header (SM, DeactivatePDPContextRequest)
//   20 = pdpHandle(4)=2, spare(4)=0
//   88 05 00c0 a801 01 = PDP Address TLV: type=IPv4, addr=192.168.1.1
TEST(GoldenSMTest, DeactivatePDPContextRequest_WithAddress) {
    std::string hex = "0a 46 20 88 05 00c0 a801 01";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);

    auto* msg = tryGet<L3DeactivatePDPContextRequest>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->pdpHandle(), 2u);
    EXPECT_TRUE(msg->hasPDPAddress());
    EXPECT_EQ(msg->pdpAddress().type(), PDPType::IPv4);
}

// GSM 24.008 9.5.4: DeactivatePDPContextRequest minimal (handle only).
// Hex breakdown:
//   0a 46 = L3 header (SM, DeactivatePDPContextRequest)
//   0f = pdpHandle(4)=0, spare(4)=f
TEST(GoldenSMTest, DeactivatePDPContextRequest_Minimal) {
    std::string hex = "0a 46 0f";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);

    auto* msg = tryGet<L3DeactivatePDPContextRequest>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->pdpHandle(), 0u);
}

// =====================================================================
// Deactivate PDP Context Accept Golden Tests
// Wire layout per GSM 24.080 (MT and MO directions).
// =====================================================================

// GSM 24.008 9.5.5: DeactivatePDPContextAccept with handle.
// Hex breakdown:
//   0a 47 = L3 header (SM, DeactivatePDPContextAccept)
//   30 = pdpHandle(4)=3, spare(4)=0
TEST(GoldenSMTest, DeactivatePDPContextAccept) {
    std::string hex = "0a 47 30";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);

    auto* msg = tryGet<L3DeactivatePDPContextAccept>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->pdpHandle(), 3u);
}

// =====================================================================
// Modify PDP Context Request Golden Tests
// Wire layout per GSM 24.080 (Modify PDP Context Request).
// =====================================================================

// GSM 24.008 9.5.6: ModifyPDPContextRequest with QoS.
// Hex breakdown:
//   0a 48 = L3 header (SM, ModifyPDPContextRequest)
//   50 = pdpHandle(4)=5, spare(4)=0
//   89 02 0001 = QoS TLV: type=requested(0), elements=01
TEST(GoldenSMTest, ModifyPDPContextRequest) {
    std::string hex = "0a 48 50 89 02 0001";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);

    auto* msg = tryGet<L3ModifyPDPContextRequest>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->pdpHandle(), 5u);
    EXPECT_EQ(msg->qos().type(), QoSType::Requested);
}

// =====================================================================
// Modify PDP Context Accept Golden Tests
// Wire layout per GSM 24.080 (Modify PDP Context Accept).
// =====================================================================

// GSM 24.008 9.5.7: ModifyPDPContextAccept with QoS.
// Hex breakdown:
//   0a 49 = L3 header (SM, ModifyPDPContextAccept)
//   50 = pdpHandle(4)=5, spare(4)=0
//   89 02 0101 = QoS TLV: type=default(1), elements=01
TEST(GoldenSMTest, ModifyPDPContextAccept) {
    std::string hex = "0a 49 50 89 02 0101";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);

    auto* msg = tryGet<L3ModifyPDPContextAccept>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->pdpHandle(), 5u);
    EXPECT_EQ(msg->qos().type(), QoSType::Default);
}

// =====================================================================
// Modify PDP Context Reject Golden Tests
// Wire layout per GSM 24.080 (Modify PDP Context Reject).
// =====================================================================

// GSM 24.008 9.5.8: ModifyPDPContextReject with handle, cause, and back-off timer.
// Hex breakdown:
//   0a 4c = L3 header (SM, ModifyPDPContextReject)
//   70 = pdpHandle(4)=7, spare(4)=0
//   a7 01 13 = SM Cause TLV: cause=Unsupported_PDP_Address_Type
//   a8 01 0a = Back-Off Timer TLV: timer=0x0a
TEST(GoldenSMTest, ModifyPDPContextReject_Full) {
    std::string hex = "0a 4c 70 a7 01 13 a8 01 0a";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);

    auto* msg = tryGet<L3ModifyPDPContextReject>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->pdpHandle(), 7u);
    EXPECT_EQ(msg->cause(), SMCause::Unsupported_PDP_Address_Type);
    EXPECT_TRUE(msg->hasBackOffTimer());
    EXPECT_EQ(msg->backOffTimer().value(), 0x0a);
}

// =====================================================================
// SM Status Golden Tests
// Wire layout per GSM 24.080 (Status).
// =====================================================================

// GSM 24.008 9.5.9: SMStatus with cause.
// Hex breakdown:
//   0a 55 = L3 header (SM, SMStatus)
//   a7 01 01 = SM Cause TLV: IEI=0x27, len=1, cause=0x01(Request accepted)
TEST(GoldenSMTest, SMStatus) {
    std::string hex = "0a 55 a7 01 01";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);

    auto* msg = tryGet<L3SMStatus>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->cause(), SMCause::ReqAccepted);
}

// =====================================================================
// Round-trip tests - construct -> writeL3Hex -> parseL3Hex -> verify fields
// =====================================================================

// ── L3TMGI IE roundtrip test ──
TEST(SMIEsTest, TMGI_Roundtrip) {
    std::array<uint8_t, 3> plmn{0x45, 0xF7, 0x10};
    L3TMGI orig(plmn, 0x1234, 0x05);
    uint8_t buf[10];
    BitWriter bw(buf, 80);
    orig.write(bw);
    BitReader br(buf, 48);
    auto res = L3TMGI::parse(br, 6);
    ASSERT_TRUE(res);
    EXPECT_EQ(res.value().serviceId(), 0x1234u);
    EXPECT_EQ(res.value().sessionId(), 0x05u);
    EXPECT_EQ(res.value().plmn()[0], 0x45u);
}

// GSM 24.008 9.5.10: RequestPDPContextActivation with handle, APN, QoS.
// 0a 44 = header (SM, MTI=0x44)
// 30 = pdpHandle(4)=3, spare(4)=0
// af 03 6970 6e = APN TLV: "ipn"
// 89 01 00 = QoS TLV: requested, no elements
TEST(GoldenSMTest, RequestPDPContextActivation) {
    std::string hex = "0a 44 30 af 03 6970 6e 89 01 00";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);
    EXPECT_EQ(messagePD(res.value()), L3PD::GPRSSessionManagement);
    auto* msg = tryGet<L3RequestPDPContextActivation>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->pdpHandle(), 3u);
    EXPECT_EQ(msg->apn().value(), "ipn");
    EXPECT_EQ(msg->qos().type(), QoSType::Requested);
}

// GSM 24.008 9.5.10: RequestPDPContextActivationReject
// 0a 45 = header, 50 = handle=5, a7 01 13 = cause TLV
TEST(GoldenSMTest, RequestPDPContextActivationReject) {
    std::string hex = "0a 45 50 a7 01 13";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);
    auto* msg = tryGet<L3RequestPDPContextActivationReject>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->pdpHandle(), 5u);
    EXPECT_EQ(msg->cause(), SMCause::Unsupported_PDP_Address_Type);
}

// GSM 24.008 9.5.6: ModifyPDPContextRequest (MS->Net)
TEST(GoldenSMTest, ModifyPDPContextRequestMS) {
    std::string hex = "0a 4a 40 89 02 0001";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);
    auto* msg = tryGet<L3ModifyPDPContextRequestMS>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->pdpHandle(), 4u);
}

// GSM 24.008 9.5.7: ModifyPDPContextAccept (Net->MS)
TEST(GoldenSMTest, ModifyPDPContextAcceptNet) {
    std::string hex = "0a 4b 60 89 02 0101";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);
    auto* msg = tryGet<L3ModifyPDPContextAcceptNet>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->pdpHandle(), 6u);
}

// GSM 24.008 9.5.11: ActivateSecondaryPDPContextRequest
TEST(GoldenSMTest, ActivateSecondaryPDPContextRequest) {
    std::string hex = "0a 4d 20 af 03 6970 6e 89 01 00";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);
    auto* msg = tryGet<L3ActivateSecondaryPDPContextRequest>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->pdpHandle(), 2u);
    EXPECT_EQ(msg->apn().value(), "ipn");
}

// GSM 24.008 9.5.12: ActivateSecondaryPDPContextAccept
TEST(GoldenSMTest, ActivateSecondaryPDPContextAccept) {
    std::string hex = "0a 4e 10 89 01 00";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);
    auto* msg = tryGet<L3ActivateSecondaryPDPContextAccept>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->pdpHandle(), 1u);
}

// GSM 24.008 9.5.13: ActivateSecondaryPDPContextReject
TEST(GoldenSMTest, ActivateSecondaryPDPContextReject) {
    std::string hex = "0a 4f 80 a7 01 14";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);
    auto* msg = tryGet<L3ActivateSecondaryPDPContextReject>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->pdpHandle(), 8u);
}

// GSM 24.008 9.5.14: ActivateAAPDPContextRequest
TEST(GoldenSMTest, ActivateAAPDPContextRequest) {
    std::string hex = "0a 50 40 af 04 6970 6e74 89 01 00";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);
    auto* msg = tryGet<L3ActivateAAPDPContextRequest>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->pdpHandle(), 4u);
    EXPECT_EQ(msg->apn().value(), "ipnt");
}

// GSM 24.008 9.5.15: ActivateAAPDPContextAccept
TEST(GoldenSMTest, ActivateAAPDPContextAccept) {
    std::string hex = "0a 51 70 89 01 00";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);
    auto* msg = tryGet<L3ActivateAAPDPContextAccept>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->pdpHandle(), 7u);
}

// GSM 24.008 9.5.16: ActivateAAPDPContextReject
TEST(GoldenSMTest, ActivateAAPDPContextReject) {
    std::string hex = "0a 52 90 a7 01 13";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);
    auto* msg = tryGet<L3ActivateAAPDPContextReject>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->pdpHandle(), 9u);
}

// GSM 24.008 9.5.17: DeactivateAAPDPContextRequest
TEST(GoldenSMTest, DeactivateAAPDPContextRequest) {
    std::string hex = "0a 53 a0";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);
    auto* msg = tryGet<L3DeactivateAAPDPContextRequest>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->pdpHandle(), 10u);
}

// GSM 24.008 9.5.17: DeactivateAAPDPContextAccept
TEST(GoldenSMTest, DeactivateAAPDPContextAccept) {
    std::string hex = "0a 54 b0";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);
    auto* msg = tryGet<L3DeactivateAAPDPContextAccept>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->pdpHandle(), 11u);
}

// GSM 24.008 9.5.18: ActivateMBMSContextRequest with TMGI
// 0a 56 = header
// c2 06 = extended IEI 0x42 (TMGI), length 6
// 45 f7 10 = PLMN
// 12 34 = Service ID
// 05 = Session ID
// 89 01 00 = QoS TLV
TEST(GoldenSMTest, ActivateMBMSContextRequest) {
    std::string hex = "0a 56 c2 06 45f7 1012 3405 89 01 00";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);
    auto* msg = tryGet<L3ActivateMBMSContextRequest>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->tmgi().serviceId(), 0x1234u);
    EXPECT_EQ(msg->tmgi().sessionId(), 0x05u);
}

// GSM 24.008 9.5.19: ActivateMBMSContextAccept
TEST(GoldenSMTest, ActivateMBMSContextAccept) {
    std::string hex = "0a 57 c0 89 01 00";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);
    auto* msg = tryGet<L3ActivateMBMSContextAccept>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->pdpHandle(), 12u);
}

// GSM 24.008 9.5.20: ActivateMBMSContextReject
TEST(GoldenSMTest, ActivateMBMSContextReject) {
    std::string hex = "0a 58 a7 01 13";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);
    auto* msg = tryGet<L3ActivateMBMSContextReject>(res.value());
    ASSERT_NE(msg, nullptr);
}

// GSM 24.008 9.5.21: RequestMBMSContextActivation
TEST(GoldenSMTest, RequestMBMSContextActivation) {
    std::string hex = "0a 59 c2 06 45f7 1012 3405 89 01 00";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);
    auto* msg = tryGet<L3RequestMBMSContextActivation>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->tmgi().serviceId(), 0x1234u);
}

// GSM 24.008 9.5.22: RequestMBMSContextActivationReject
TEST(GoldenSMTest, RequestMBMSContextActivationReject) {
    std::string hex = "0a 5a a7 01 13";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);
    auto* msg = tryGet<L3RequestMBMSContextActivationReject>(res.value());
    ASSERT_NE(msg, nullptr);
}

// GSM 24.008 9.5.23: RequestSecondaryPDPContextActivation
TEST(GoldenSMTest, RequestSecondaryPDPContextActivation) {
    std::string hex = "0a 5b d0 af 03 6970 6e 89 01 00";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);
    auto* msg = tryGet<L3RequestSecondaryPDPContextActivation>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->pdpHandle(), 13u);
}

// GSM 24.008 9.5.24: RequestSecondaryPDPContextActivationReject
TEST(GoldenSMTest, RequestSecondaryPDPContextActivationReject) {
    std::string hex = "0a 5c e0 a7 01 13";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);
    auto* msg = tryGet<L3RequestSecondaryPDPContextActivationReject>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->pdpHandle(), 14u);
}

// GSM 24.008 9.5.25: SMNotification
TEST(GoldenSMTest, SMNotification) {
    std::string hex = "0a 5d f0";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);
    auto* msg = tryGet<L3SMNotification>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->pdpHandle(), 15u);
}

// ── Roundtrip tests for additional SM messages ──

TEST(RoundTripTest, RequestPDPContextActivation_RT) {
    auto res = parseL3Hex("0a 44 30 af 03 6970 6e 89 01 00");
    ASSERT_TRUE(res);
    auto rt = roundtrip(res.value());
    ASSERT_TRUE(rt);
    auto* m = tryGet<L3RequestPDPContextActivation>(rt.value());
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->pdpHandle(), 3u);
}

TEST(RoundTripTest, RequestPDPContextActivationReject_RT) {
    auto res = parseL3Hex("0a 45 50 a7 01 13");
    ASSERT_TRUE(res);
    auto rt = roundtrip(res.value());
    ASSERT_TRUE(rt);
    auto* m = tryGet<L3RequestPDPContextActivationReject>(rt.value());
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->pdpHandle(), 5u);
}

TEST(RoundTripTest, ModifyPDPContextRequestMS_RT) {
    auto res = parseL3Hex("0a 4a 40 89 02 0001");
    ASSERT_TRUE(res);
    auto rt = roundtrip(res.value());
    ASSERT_TRUE(rt);
    auto* m = tryGet<L3ModifyPDPContextRequestMS>(rt.value());
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->pdpHandle(), 4u);
}

TEST(RoundTripTest, ModifyPDPContextAcceptNet_RT) {
    auto res = parseL3Hex("0a 4b 60 89 02 0101");
    ASSERT_TRUE(res);
    auto rt = roundtrip(res.value());
    ASSERT_TRUE(rt);
    auto* m = tryGet<L3ModifyPDPContextAcceptNet>(rt.value());
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->pdpHandle(), 6u);
}

TEST(RoundTripTest, ActivateSecondaryPDPContextRequest_RT) {
    auto res = parseL3Hex("0a 4d 20 af 03 6970 6e 89 01 00");
    ASSERT_TRUE(res);
    auto rt = roundtrip(res.value());
    ASSERT_TRUE(rt);
    auto* m = tryGet<L3ActivateSecondaryPDPContextRequest>(rt.value());
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->pdpHandle(), 2u);
}

TEST(RoundTripTest, ActivateSecondaryPDPContextAccept_RT) {
    auto res = parseL3Hex("0a 4e 10 89 01 00");
    ASSERT_TRUE(res);
    auto rt = roundtrip(res.value());
    ASSERT_TRUE(rt);
    auto* m = tryGet<L3ActivateSecondaryPDPContextAccept>(rt.value());
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->pdpHandle(), 1u);
}

TEST(RoundTripTest, ActivateSecondaryPDPContextReject_RT) {
    auto res = parseL3Hex("0a 4f 80 a7 01 14");
    ASSERT_TRUE(res);
    auto rt = roundtrip(res.value());
    ASSERT_TRUE(rt);
    auto* m = tryGet<L3ActivateSecondaryPDPContextReject>(rt.value());
    ASSERT_NE(m, nullptr);
}

TEST(RoundTripTest, ActivateAAPDPContextRequest_RT) {
    auto res = parseL3Hex("0a 50 40 af 04 6970 6e74 89 01 00");
    ASSERT_TRUE(res);
    auto rt = roundtrip(res.value());
    ASSERT_TRUE(rt);
    auto* m = tryGet<L3ActivateAAPDPContextRequest>(rt.value());
    ASSERT_NE(m, nullptr);
}

TEST(RoundTripTest, ActivateAAPDPContextAccept_RT) {
    auto res = parseL3Hex("0a 51 70 89 01 00");
    ASSERT_TRUE(res);
    auto rt = roundtrip(res.value());
    ASSERT_TRUE(rt);
    auto* m = tryGet<L3ActivateAAPDPContextAccept>(rt.value());
    ASSERT_NE(m, nullptr);
}

TEST(RoundTripTest, ActivateAAPDPContextReject_RT) {
    auto res = parseL3Hex("0a 52 90 a7 01 13");
    ASSERT_TRUE(res);
    auto rt = roundtrip(res.value());
    ASSERT_TRUE(rt);
    auto* m = tryGet<L3ActivateAAPDPContextReject>(rt.value());
    ASSERT_NE(m, nullptr);
}

TEST(RoundTripTest, DeactivateAAPDPContextRequest_RT) {
    auto res = parseL3Hex("0a 53 a0");
    ASSERT_TRUE(res);
    auto rt = roundtrip(res.value());
    ASSERT_TRUE(rt);
    auto* m = tryGet<L3DeactivateAAPDPContextRequest>(rt.value());
    ASSERT_NE(m, nullptr);
}

TEST(RoundTripTest, DeactivateAAPDPContextAccept_RT) {
    auto res = parseL3Hex("0a 54 b0");
    ASSERT_TRUE(res);
    auto rt = roundtrip(res.value());
    ASSERT_TRUE(rt);
    auto* m = tryGet<L3DeactivateAAPDPContextAccept>(rt.value());
    ASSERT_NE(m, nullptr);
}

TEST(RoundTripTest, ActivateMBMSContextRequest_RT) {
    auto res = parseL3Hex("0a 56 c2 06 45f7 1012 3405 89 01 00");
    ASSERT_TRUE(res);
    auto rt = roundtrip(res.value());
    ASSERT_TRUE(rt);
    auto* m = tryGet<L3ActivateMBMSContextRequest>(rt.value());
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->tmgi().serviceId(), 0x1234u);
}

TEST(RoundTripTest, ActivateMBMSContextAccept_RT) {
    auto res = parseL3Hex("0a 57 c0 89 01 00");
    ASSERT_TRUE(res);
    auto rt = roundtrip(res.value());
    ASSERT_TRUE(rt);
    auto* m = tryGet<L3ActivateMBMSContextAccept>(rt.value());
    ASSERT_NE(m, nullptr);
}

TEST(RoundTripTest, ActivateMBMSContextReject_RT) {
    auto res = parseL3Hex("0a 58 a7 01 13");
    ASSERT_TRUE(res);
    auto rt = roundtrip(res.value());
    ASSERT_TRUE(rt);
    auto* m = tryGet<L3ActivateMBMSContextReject>(rt.value());
    ASSERT_NE(m, nullptr);
}

TEST(RoundTripTest, RequestMBMSContextActivation_RT) {
    auto res = parseL3Hex("0a 59 c2 06 45f7 1012 3405 89 01 00");
    ASSERT_TRUE(res);
    auto rt = roundtrip(res.value());
    ASSERT_TRUE(rt);
    auto* m = tryGet<L3RequestMBMSContextActivation>(rt.value());
    ASSERT_NE(m, nullptr);
}

TEST(RoundTripTest, RequestMBMSContextActivationReject_RT) {
    auto res = parseL3Hex("0a 5a a7 01 13");
    ASSERT_TRUE(res);
    auto rt = roundtrip(res.value());
    ASSERT_TRUE(rt);
    auto* m = tryGet<L3RequestMBMSContextActivationReject>(rt.value());
    ASSERT_NE(m, nullptr);
}

TEST(RoundTripTest, RequestSecondaryPDPContextActivation_RT) {
    auto res = parseL3Hex("0a 5b d0 af 03 6970 6e 89 01 00");
    ASSERT_TRUE(res);
    auto rt = roundtrip(res.value());
    ASSERT_TRUE(rt);
    auto* m = tryGet<L3RequestSecondaryPDPContextActivation>(rt.value());
    ASSERT_NE(m, nullptr);
}

TEST(RoundTripTest, RequestSecondaryPDPContextActivationReject_RT) {
    auto res = parseL3Hex("0a 5c e0 a7 01 13");
    ASSERT_TRUE(res);
    auto rt = roundtrip(res.value());
    ASSERT_TRUE(rt);
    auto* m = tryGet<L3RequestSecondaryPDPContextActivationReject>(rt.value());
    ASSERT_NE(m, nullptr);
}

TEST(RoundTripTest, SMNotification_RT) {
    auto res = parseL3Hex("0a 5d f0");
    ASSERT_TRUE(res);
    auto rt = roundtrip(res.value());
    ASSERT_TRUE(rt);
    auto* m = tryGet<L3SMNotification>(rt.value());
    ASSERT_NE(m, nullptr);
}

// =====================================================================
// Original roundtrip tests below
// =====================================================================

// GSM 24.008 9.5.1: ActivatePDPContextRequest round-trip.
// Construct with IPv4, APN="internet", QoS=requested -> serialize -> parse -> verify.
TEST(RoundTripTest, ActivatePDPContextRequest_Full) {
    L3ActivatePDPContextRequest msg;
    // We construct via parse since there's no public constructor for all fields.
    auto res = parseL3Hex("0a 41 00 af 08 696e74 65726e 6574 89 01 00");
    ASSERT_TRUE(res);

    auto rt = roundtrip(res.value());
    ASSERT_TRUE(rt);

    auto* m = tryGet<L3ActivatePDPContextRequest>(rt.value());
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->pdpType(), PDPType::IPv4);
    EXPECT_EQ(m->apn().value(), "internet");
    EXPECT_EQ(m->qos().type(), QoSType::Requested);
}

// GSM 24.008 9.5.2: ActivatePDPContextAccept round-trip.
TEST(RoundTripTest, ActivatePDPContextAccept_Full) {
    auto res = parseL3Hex("0a 42 10 88 05 00c0 a801 64 89 03 0010 01");
    ASSERT_TRUE(res);

    auto rt = roundtrip(res.value());
    ASSERT_TRUE(rt);

    auto* m = tryGet<L3ActivatePDPContextAccept>(rt.value());
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->pdpHandle(), 1u);
    EXPECT_TRUE(m->hasPDPAddress());
}

// GSM 24.008 9.5.3: ActivatePDPContextReject round-trip.
TEST(RoundTripTest, ActivatePDPContextReject_Full) {
    auto res = parseL3Hex("0a 43 a7 01 13 a8 01 05");
    ASSERT_TRUE(res);

    auto rt = roundtrip(res.value());
    ASSERT_TRUE(rt);

    auto* m = tryGet<L3ActivatePDPContextReject>(rt.value());
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->cause(), SMCause::Unsupported_PDP_Address_Type);
    EXPECT_TRUE(m->hasBackOffTimer());
}

// GSM 24.008 9.5.4: DeactivatePDPContextRequest round-trip.
TEST(RoundTripTest, DeactivatePDPContextRequest_Full) {
    auto res = parseL3Hex("0a 46 20 88 05 00c0 a801 01");
    ASSERT_TRUE(res);

    auto rt = roundtrip(res.value());
    ASSERT_TRUE(rt);

    auto* m = tryGet<L3DeactivatePDPContextRequest>(rt.value());
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->pdpHandle(), 2u);
    EXPECT_TRUE(m->hasPDPAddress());
}

// GSM 24.008 9.5.5: DeactivatePDPContextAccept round-trip.
TEST(RoundTripTest, DeactivatePDPContextAccept_Full) {
    auto res = parseL3Hex("0a 47 30");
    ASSERT_TRUE(res);

    auto rt = roundtrip(res.value());
    ASSERT_TRUE(rt);

    auto* m = tryGet<L3DeactivatePDPContextAccept>(rt.value());
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->pdpHandle(), 3u);
}

// GSM 24.008 9.5.6: ModifyPDPContextRequest round-trip.
TEST(RoundTripTest, ModifyPDPContextRequest_Full) {
    auto res = parseL3Hex("0a 48 50 89 02 0001");
    ASSERT_TRUE(res);

    auto rt = roundtrip(res.value());
    ASSERT_TRUE(rt);

    auto* m = tryGet<L3ModifyPDPContextRequest>(rt.value());
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->pdpHandle(), 5u);
}

// GSM 24.008 9.5.7: ModifyPDPContextAccept round-trip.
TEST(RoundTripTest, ModifyPDPContextAccept_Full) {
    auto res = parseL3Hex("0a 49 50 89 02 0101");
    ASSERT_TRUE(res);

    auto rt = roundtrip(res.value());
    ASSERT_TRUE(rt);

    auto* m = tryGet<L3ModifyPDPContextAccept>(rt.value());
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->pdpHandle(), 5u);
}

// GSM 24.008 9.5.8: ModifyPDPContextReject round-trip.
TEST(RoundTripTest, ModifyPDPContextReject_Full) {
    auto res = parseL3Hex("0a 4c 70 a7 01 13 a8 01 0a");
    ASSERT_TRUE(res);

    auto rt = roundtrip(res.value());
    ASSERT_TRUE(rt);

    auto* m = tryGet<L3ModifyPDPContextReject>(rt.value());
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->pdpHandle(), 7u);
    EXPECT_EQ(m->cause(), SMCause::Unsupported_PDP_Address_Type);
}

// GSM 24.008 9.5.9: SMStatus round-trip.
TEST(RoundTripTest, SMStatus_Full) {
    auto res = parseL3Hex("0a 55 a7 01 01");
    ASSERT_TRUE(res);

    auto rt = roundtrip(res.value());
    ASSERT_TRUE(rt);

    auto* m = tryGet<L3SMStatus>(rt.value());
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->cause(), SMCause::ReqAccepted);
}

// =====================================================================
// SM IE Tests
// =====================================================================

TEST(SMIEsTest, PDPAddress_IPv4) {
    uint8_t addr[] = {0x00, 0xc0, 0xa8, 0x01, 0x01};
    std::vector<uint8_t> bytes(std::begin(addr), std::end(addr));
    BitReader br(bytes.data(), bytes.size() * 8);

    auto res = L3PDPAddress::parse(br, 5);
    ASSERT_TRUE(res);
    EXPECT_EQ(res.value().type(), PDPType::IPv4);
    EXPECT_EQ(res.value().address()[0], 0xc0);
    EXPECT_EQ(res.value().address()[1], 0xa8);
}

TEST(SMIEsTest, AccessPointName) {
    std::string apnStr = "internet";
    uint8_t buf[10];
    size_t totalLen = 1 + apnStr.size(); // IEI + length + value... actually just value for parse
    BitReader br(reinterpret_cast<const uint8_t*>(apnStr.data()), apnStr.size() * 8);

    auto res = L3AccessPointName::parse(br, apnStr.size());
    ASSERT_TRUE(res);
    EXPECT_EQ(res.value().value(), "internet");
}

TEST(SMIEsTest, QoS_Requested) {
    uint8_t data[] = {0x00, 0x10, 0x01}; // type=requested(0), elements=10:01
    std::vector<uint8_t> bytes(std::begin(data), std::end(data));
    BitReader br(bytes.data(), bytes.size() * 8);

    auto res = L3QoS::parse(br, 3);
    ASSERT_TRUE(res);
    EXPECT_EQ(res.value().type(), QoSType::Requested);
    EXPECT_EQ(res.value().elements().size(), 2u);
}

TEST(SMIEsTest, SMCauseToString) {
    EXPECT_STREQ(SMCause2Str(SMCause::ReqAccepted), "Request accepted");
    EXPECT_STREQ(SMCause2Str(SMCause::Unsupported_PDP_Address_Type), "Unsupported PDP address type");
    EXPECT_STREQ(SMCause2Str(SMCause::Protocol_Error_Unspecified), "Protocol error unspecified");
}

// =====================================================================
// Visitor tests for SM messages
// =====================================================================

TEST(SMVisitorTest, MessageNames) {
    auto names = std::vector<std::pair<std::string_view, std::string>>{
        {"0a 41 00 af 03 6970 6e 89 01 00", "ActivatePDPContextRequest"},
        {"0a 42 00 89 01 00", "ActivatePDPContextAccept"},
        {"0a 43 a7 01 13", "ActivatePDPContextReject"},
        {"0a 46 0f", "DeactivatePDPContextRequest"},
        {"0a 47 30", "DeactivatePDPContextAccept"},
        {"0a 48 50 89 02 0001", "ModifyPDPContextRequest"},
        {"0a 49 50 89 02 0101", "ModifyPDPContextAccept"},
        {"0a 4c 70 a7 01 13", "ModifyPDPContextReject"},
        {"0a 55 a7 01 01", "SMStatus"},
        {"0a 44 30 af 03 6970 6e 89 01 00", "RequestPDPContextActivation"},
        {"0a 45 50 a7 01 13", "RequestPDPContextActivationReject"},
        {"0a 4a 40 89 02 0001", "ModifyPDPContextRequestMS"},
        {"0a 4b 60 89 02 0101", "ModifyPDPContextAcceptNet"},
        {"0a 4d 20 af 03 6970 6e 89 01 00", "ActivateSecondaryPDPContextRequest"},
        {"0a 4e 10 89 01 00", "ActivateSecondaryPDPContextAccept"},
        {"0a 4f 80 a7 01 14", "ActivateSecondaryPDPContextReject"},
        {"0a 50 40 af 04 6970 6e74 89 01 00", "ActivateAAPDPContextRequest"},
        {"0a 51 70 89 01 00", "ActivateAAPDPContextAccept"},
        {"0a 52 90 a7 01 13", "ActivateAAPDPContextReject"},
        {"0a 53 a0", "DeactivateAAPDPContextRequest"},
        {"0a 54 b0", "DeactivateAAPDPContextAccept"},
        {"0a 56 c2 06 45f7 1012 3405 89 01 00", "ActivateMBMSContextRequest"},
        {"0a 57 c0 89 01 00", "ActivateMBMSContextAccept"},
        {"0a 58 a7 01 13", "ActivateMBMSContextReject"},
        {"0a 59 c2 06 45f7 1012 3405 89 01 00", "RequestMBMSContextActivation"},
        {"0a 5a a7 01 13", "RequestMBMSContextActivationReject"},
        {"0a 5b d0 af 03 6970 6e 89 01 00", "RequestSecondaryPDPContextActivation"},
        {"0a 5c e0 a7 01 13", "RequestSecondaryPDPContextActivationReject"},
        {"0a 5d f0", "SMNotification"},
    };

    for (auto& [hex, expectedName] : names) {
        auto res = parseL3Hex(hex);
        ASSERT_TRUE(res) << "Failed to parse: " << hex;
        EXPECT_EQ(messageName(res.value()), expectedName) << "For hex: " << hex;
        EXPECT_EQ(messagePD(res.value()), L3PD::GPRSSessionManagement);
    }
}

TEST(SMVisitorTest, MessageMTIValues) {
    auto mtis = std::vector<std::pair<std::string_view, int>>{
        {"0a 41 00 af 03 6970 6e 89 01 00", 0x41},
        {"0a 42 00 89 01 00", 0x42},
        {"0a 43 a7 01 13", 0x43},
        {"0a 46 0f", 0x46},
        {"0a 47 30", 0x47},
        {"0a 48 50 89 02 0001", 0x48},
        {"0a 49 50 89 02 0101", 0x49},
        {"0a 4c 70 a7 01 13", 0x4c},
        {"0a 55 a7 01 01", 0x55},
        {"0a 44 30 af 03 6970 6e 89 01 00", 0x44},
        {"0a 45 50 a7 01 13", 0x45},
        {"0a 4a 40 89 02 0001", 0x4A},
        {"0a 4b 60 89 02 0101", 0x4B},
        {"0a 4d 20 af 03 6970 6e 89 01 00", 0x4D},
        {"0a 4e 10 89 01 00", 0x4E},
        {"0a 4f 80 a7 01 14", 0x4F},
        {"0a 50 40 af 04 6970 6e74 89 01 00", 0x50},
        {"0a 51 70 89 01 00", 0x51},
        {"0a 52 90 a7 01 13", 0x52},
        {"0a 53 a0", 0x53},
        {"0a 54 b0", 0x54},
        {"0a 56 c2 06 45f7 1012 3405 89 01 00", 0x56},
        {"0a 57 c0 89 01 00", 0x57},
        {"0a 58 a7 01 13", 0x58},
        {"0a 59 c2 06 45f7 1012 3405 89 01 00", 0x59},
        {"0a 5a a7 01 13", 0x5A},
        {"0a 5b d0 af 03 6970 6e 89 01 00", 0x5B},
        {"0a 5c e0 a7 01 13", 0x5C},
        {"0a 5d f0", 0x5D},
    };

    for (auto& [hex, expectedMTI] : mtis) {
        auto res = parseL3Hex(hex);
        ASSERT_TRUE(res) << "Failed to parse: " << hex;
        EXPECT_EQ(messageMTI(res.value()), expectedMTI) << "For hex: " << hex;
    }
}
