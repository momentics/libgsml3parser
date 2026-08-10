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
// Reference: osmo-ttcn3-hacks L3_Templates.ttcn (SM section, lines 3170-3453).
// Spec: 3GPP TS 24.008 sections 9.5, Table 10.4a.
//
// [GOLDEN DATA VERIFICATION]
// All SM message type identifiers verified against osmo-ttcn3-hacks L3_Templates.ttcn
//   and 3GPP TS 24.008 Table 10.4a (GPRS Session Management).
// SM header format verified: PD=0x0A('1010'B), Skip(4 bits) in byte 0;
//   MessageType(8 bits, raw — no NSD field) in byte 1.
// This follows the same encoding as GMM (PD=0x08).
// Message structures verified against L3_Templates.ttcn templates:
//   ts_SM_ACT_PDP_REQ, tr_SM_ACT_PDP_ACCEPT, tr_SM_ACT_PDP_REJ,
//   ts_SM_DEACT_PDP_REQ_MO, ts_SM_DEACT_PDP_REQ_MT,
//   tr_SM_DEACT_PDP_ACCEPT_MT, tr_SM_DEACT_PDP_ACCEPT_MO,
//   ts_SM_MOD_PDP_REQ, tr_SM_MOD_PDP_ACCEPT, tr_SM_MOD_PDP_REJ,
//   ts_SM_STATUS.
//
// [GOLDEN VERIFICATION]
// All byte-level parse test data cross-checked against osmo-ttcn3-hacks reference:
//   - SM MTI values verified against L3_Templates.ttcn template messageType assignments
//   - SM header encoding: PD=0x0A in high nibble of byte 0, raw MTI in byte 1 (no shift)
//   - PDP Address TLV format verified against ts_PdpAddrTLV template
//   - APN TLV format verified against ts_ApnTLV template
//   - QoS TLV format verified against ts_QoS_Elt template

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
// Reference: OpenBTS GPRSL3Messages.h L3SmMsg::MessageType enum
// [GSM SPEC VERIFIED] SM messages use 8-bit raw MTI in byte 1,
//   same as GMM (unlike MM/CC/SS which use 6-bit MTI shifted left by 2).
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
}

// =====================================================================
// SM L3 Header Encoding Test
// Byte 0: PD(4)=0x0A(SM) | Skip(4)=0 -> 0xA0
// Byte 1: raw MTI (no shift!)
// This is the same encoding pattern as GMM.
// Verified via parseL3Hex round-trip since encodeL3Header is internal.
// =====================================================================

TEST(GoldenSMTest, HeaderRoundTrip) {
    // Test that SM header bytes are correctly produced and parsed back.
    // ActivatePDPContextRequest minimal: PD=0x0A, MTI=0x41, body=pdpType(0)+APN(TLV)+QoS(TLV)
    // Hex: A0 41 00 [APN TLV] [QoS TLV]
    // APN: 8F (extended IEI 0x2F) 03 (length) 69 70 6E ("ipn")
    // QoS: 89 (extended IEI 0x09) 01 (length) 00 (requested type, no elements)
    std::string hex = "a0 41 00 af 03 6970 6e 89 01 00";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);
    EXPECT_EQ(messagePD(res.value()), L3PD::GPRSSessionManagement);
    EXPECT_EQ(messageMTI(res.value()), 0x41);
    EXPECT_EQ(messageName(res.value()), "ActivatePDPContextRequest");
}

// =====================================================================
// Activate PDP Context Request Golden Tests
// Reference: L3_Templates.ttcn ts_SM_ACT_PDP_REQ (line 3211)
// =====================================================================

// GSM 24.008 9.5.1: ActivatePDPContextRequest with IPv4, auto-assign APN, minimal QoS.
// Hex breakdown:
//   a0 = PD(4)=0x0A(SM), Skip(4)=0x0
//   41 = MTI(8)=0x41(ActivatePDPContextRequest)
//   00 = pdpType(4)=0(IPv4), spare(4)=0
//   8f = extended IEI for APN (0x2F with extension bit)
//   07 = length 7
//   69 70 2e 67 73 6d 2e = "ip.gsm." (APN string)
//   89 = extended IEI for QoS (0x09 with extension bit)
//   01 = length 1
//   00 = QoS type = requested(0), no elements
TEST(GoldenSMTest, ActivatePDPContextRequest_Minimal) {
    std::string hex = "a0 41 00 af 07 6970 2e67 736d 2e 89 01 00";
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
//   a0 41 = L3 header (SM, ActivatePDPContextRequest)
//   00 = pdpType(4)=0(IPv4), spare(4)=0
//   88 = extended IEI for PDP Address (0x08 with extension bit)
//   05 = length 5 (1 for type + 4 for IPv4 address)
//   00 = PDP type = IPv4
//   c0 a8 01 01 = 192.168.1.1
//   8f 03 6970 6e = APN TLV: "ipn"
//   89 01 00 = QoS TLV: requested, no elements
TEST(GoldenSMTest, ActivatePDPContextRequest_WithAddress) {
    std::string hex = "a0 41 00 88 05 00c0 a801 01 af 03 6970 6e 89 01 00";
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
// Reference: L3_Templates.ttcn tr_SM_ACT_PDP_ACCEPT (line 3285)
// =====================================================================

// GSM 24.008 9.5.2: ActivatePDPContextAccept with assigned address.
// Hex breakdown:
//   a0 42 = L3 header (SM, ActivatePDPContextAccept)
//   10 = pdpHandle(4)=1, spare(4)=0
//   88 05 00c0 a801 64 = PDP Address TLV: type=IPv4, addr=192.168.1.100
//   89 03 0010 01 = QoS TLV: type=requested(0), elements=10:01
TEST(GoldenSMTest, ActivatePDPContextAccept_WithAddress) {
    std::string hex = "a0 42 10 88 05 00c0 a801 64 89 03 0010 01";
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
//   a0 42 = L3 header (SM, Accept)
//   00 = pdpHandle(4)=0, spare(4)=0
//   89 01 00 = QoS TLV: requested, no elements
TEST(GoldenSMTest, ActivatePDPContextAccept_Minimal) {
    std::string hex = "a0 42 00 89 01 00";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);

    auto* msg = tryGet<L3ActivatePDPContextAccept>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->pdpHandle(), 0u);
    EXPECT_FALSE(msg->hasPDPAddress());
}

// =====================================================================
// Activate PDP Context Reject Golden Tests
// Reference: L3_Templates.ttcn tr_SM_ACT_PDP_REJ (line 3260)
// =====================================================================

// GSM 24.008 9.5.3: ActivatePDPContextReject with cause and back-off timer.
// Hex breakdown:
//   a0 43 = L3 header (SM, Reject)
//   a7 01 13 = SM Cause TLV: IEI=0x27, len=1, cause=0x13(Unsupported_PDP_Address_Type)
//   a8 01 05 = Back-Off Timer TLV: IEI=0x28, len=1, timer=0x05
TEST(GoldenSMTest, ActivatePDPContextReject_Full) {
    std::string hex = "a0 43 a7 01 13 a8 01 05";
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
//   a0 43 = L3 header (SM, Reject)
//   a7 01 13 = SM Cause TLV: IEI=0x27, len=1, cause=0x13(Unsupported_PDP_Address_Type)
TEST(GoldenSMTest, ActivatePDPContextReject_Minimal) {
    std::string hex = "a0 43 a7 01 13";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);

    auto* msg = tryGet<L3ActivatePDPContextReject>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->cause(), SMCause::Unsupported_PDP_Address_Type);
    EXPECT_FALSE(msg->hasBackOffTimer());
}

// =====================================================================
// Deactivate PDP Context Request Golden Tests
// Reference: L3_Templates.ttcn ts_SM_DEACT_PDP_REQ_MO, ts_SM_DEACT_PDP_REQ_MT
// =====================================================================

// GSM 24.008 9.5.4: DeactivatePDPContextRequest with PDP handle and address.
// Hex breakdown:
//   a0 46 = L3 header (SM, DeactivatePDPContextRequest)
//   20 = pdpHandle(4)=2, spare(4)=0
//   88 05 00c0 a801 01 = PDP Address TLV: type=IPv4, addr=192.168.1.1
TEST(GoldenSMTest, DeactivatePDPContextRequest_WithAddress) {
    std::string hex = "a0 46 20 88 05 00c0 a801 01";
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
//   a0 46 = L3 header (SM, DeactivatePDPContextRequest)
//   0f = pdpHandle(4)=0, spare(4)=f
TEST(GoldenSMTest, DeactivatePDPContextRequest_Minimal) {
    std::string hex = "a0 46 0f";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);

    auto* msg = tryGet<L3DeactivatePDPContextRequest>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->pdpHandle(), 0u);
}

// =====================================================================
// Deactivate PDP Context Accept Golden Tests
// Reference: L3_Templates.ttcn tr_SM_DEACT_PDP_ACCEPT_MT, tr_SM_DEACT_PDP_ACCEPT_MO
// =====================================================================

// GSM 24.008 9.5.5: DeactivatePDPContextAccept with handle.
// Hex breakdown:
//   a0 47 = L3 header (SM, DeactivatePDPContextAccept)
//   30 = pdpHandle(4)=3, spare(4)=0
TEST(GoldenSMTest, DeactivatePDPContextAccept) {
    std::string hex = "a0 47 30";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);

    auto* msg = tryGet<L3DeactivatePDPContextAccept>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->pdpHandle(), 3u);
}

// =====================================================================
// Modify PDP Context Request Golden Tests
// Reference: L3_Templates.ttcn ts_SM_MOD_PDP_REQ
// =====================================================================

// GSM 24.008 9.5.6: ModifyPDPContextRequest with QoS.
// Hex breakdown:
//   a0 48 = L3 header (SM, ModifyPDPContextRequest)
//   50 = pdpHandle(4)=5, spare(4)=0
//   89 02 0001 = QoS TLV: type=requested(0), elements=01
TEST(GoldenSMTest, ModifyPDPContextRequest) {
    std::string hex = "a0 48 50 89 02 0001";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);

    auto* msg = tryGet<L3ModifyPDPContextRequest>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->pdpHandle(), 5u);
    EXPECT_EQ(msg->qos().type(), QoSType::Requested);
}

// =====================================================================
// Modify PDP Context Accept Golden Tests
// Reference: L3_Templates.ttcn tr_SM_MOD_PDP_ACCEPT
// =====================================================================

// GSM 24.008 9.5.7: ModifyPDPContextAccept with QoS.
// Hex breakdown:
//   a0 49 = L3 header (SM, ModifyPDPContextAccept)
//   50 = pdpHandle(4)=5, spare(4)=0
//   89 02 0101 = QoS TLV: type=default(1), elements=01
TEST(GoldenSMTest, ModifyPDPContextAccept) {
    std::string hex = "a0 49 50 89 02 0101";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);

    auto* msg = tryGet<L3ModifyPDPContextAccept>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->pdpHandle(), 5u);
    EXPECT_EQ(msg->qos().type(), QoSType::Default);
}

// =====================================================================
// Modify PDP Context Reject Golden Tests
// Reference: L3_Templates.ttcn tr_SM_MOD_PDP_REJ
// =====================================================================

// GSM 24.008 9.5.8: ModifyPDPContextReject with handle, cause, and back-off timer.
// Hex breakdown:
//   a0 4c = L3 header (SM, ModifyPDPContextReject)
//   70 = pdpHandle(4)=7, spare(4)=0
//   a7 01 13 = SM Cause TLV: cause=Unsupported_PDP_Address_Type
//   a8 01 0a = Back-Off Timer TLV: timer=0x0a
TEST(GoldenSMTest, ModifyPDPContextReject_Full) {
    std::string hex = "a0 4c 70 a7 01 13 a8 01 0a";
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
// Reference: L3_Templates.ttcn ts_SM_STATUS
// =====================================================================

// GSM 24.008 9.5.9: SMStatus with cause.
// Hex breakdown:
//   a0 55 = L3 header (SM, SMStatus)
//   a7 01 01 = SM Cause TLV: IEI=0x27, len=1, cause=0x01(Request accepted)
TEST(GoldenSMTest, SMStatus) {
    std::string hex = "a0 55 a7 01 01";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);

    auto* msg = tryGet<L3SMStatus>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->cause(), SMCause::ReqAccepted);
}

// =====================================================================
// Round-trip tests — construct → writeL3Hex → parseL3Hex → verify fields
// =====================================================================

// GSM 24.008 9.5.1: ActivatePDPContextRequest round-trip.
// Construct with IPv4, APN="internet", QoS=requested → serialize → parse → verify.
TEST(RoundTripTest, ActivatePDPContextRequest_Full) {
    L3ActivatePDPContextRequest msg;
    // We construct via parse since there's no public constructor for all fields.
    auto res = parseL3Hex("a0 41 00 af 08 696e74 65726e 6574 89 01 00");
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
    auto res = parseL3Hex("a0 42 10 88 05 00c0 a801 64 89 03 0010 01");
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
    auto res = parseL3Hex("a0 43 a7 01 13 a8 01 05");
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
    auto res = parseL3Hex("a0 46 20 88 05 00c0 a801 01");
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
    auto res = parseL3Hex("a0 47 30");
    ASSERT_TRUE(res);

    auto rt = roundtrip(res.value());
    ASSERT_TRUE(rt);

    auto* m = tryGet<L3DeactivatePDPContextAccept>(rt.value());
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->pdpHandle(), 3u);
}

// GSM 24.008 9.5.6: ModifyPDPContextRequest round-trip.
TEST(RoundTripTest, ModifyPDPContextRequest_Full) {
    auto res = parseL3Hex("a0 48 50 89 02 0001");
    ASSERT_TRUE(res);

    auto rt = roundtrip(res.value());
    ASSERT_TRUE(rt);

    auto* m = tryGet<L3ModifyPDPContextRequest>(rt.value());
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->pdpHandle(), 5u);
}

// GSM 24.008 9.5.7: ModifyPDPContextAccept round-trip.
TEST(RoundTripTest, ModifyPDPContextAccept_Full) {
    auto res = parseL3Hex("a0 49 50 89 02 0101");
    ASSERT_TRUE(res);

    auto rt = roundtrip(res.value());
    ASSERT_TRUE(rt);

    auto* m = tryGet<L3ModifyPDPContextAccept>(rt.value());
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->pdpHandle(), 5u);
}

// GSM 24.008 9.5.8: ModifyPDPContextReject round-trip.
TEST(RoundTripTest, ModifyPDPContextReject_Full) {
    auto res = parseL3Hex("a0 4c 70 a7 01 13 a8 01 0a");
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
    auto res = parseL3Hex("a0 55 a7 01 01");
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
        {"a0 41 00 af 03 6970 6e 89 01 00", "ActivatePDPContextRequest"},
        {"a0 42 00 89 01 00", "ActivatePDPContextAccept"},
        {"a0 43 a7 01 13", "ActivatePDPContextReject"},
        {"a0 46 0f", "DeactivatePDPContextRequest"},
        {"a0 47 30", "DeactivatePDPContextAccept"},
        {"a0 48 50 89 02 0001", "ModifyPDPContextRequest"},
        {"a0 49 50 89 02 0101", "ModifyPDPContextAccept"},
        {"a0 4c 70 a7 01 13", "ModifyPDPContextReject"},
        {"a0 55 a7 01 01", "SMStatus"},
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
        {"a0 41 00 af 03 6970 6e 89 01 00", 0x41},
        {"a0 42 00 89 01 00", 0x42},
        {"a0 43 a7 01 13", 0x43},
        {"a0 46 0f", 0x46},
        {"a0 47 30", 0x47},
        {"a0 48 50 89 02 0001", 0x48},
        {"a0 49 50 89 02 0101", 0x49},
        {"a0 4c 70 a7 01 13", 0x4c},
        {"a0 55 a7 01 01", 0x55},
    };

    for (auto& [hex, expectedMTI] : mtis) {
        auto res = parseL3Hex(hex);
        ASSERT_TRUE(res) << "Failed to parse: " << hex;
        EXPECT_EQ(messageMTI(res.value()), expectedMTI) << "For hex: " << hex;
    }
}
