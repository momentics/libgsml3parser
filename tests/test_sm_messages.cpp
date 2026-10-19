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
// All byte-level parse test data cross-checked against 3GPP TS 44.068:
//   - SM MTI values per the SM message type table (TS 24.008 Table 10.4a)
//   - SM header encoding: PD=0x0A in the low nibble of byte 0, raw MTI in byte 1 (no shift)
//   - PDP Address LV format per the SM information element definitions
//     ([spare=0|typeOrg][pdpTypeNumber: IETF IPv4=0x21, IETF IPv6=0x53]
//      [address], TS 24.008 10.5.6.4)
//   - APN TLV format (IEI=0x28, optional) per the SM information element definitions
//   - QoS LV format (no identifier; first three value octets are the
//     mandatory characteristics, further octets are optional characteristics)
//     per TS 24.008 10.5.6.5
//   - four-bit SM value fields (NSAPI, LLC SAPI, radio priority for SMS,
//     PDP handle) in the low nibble of one octet, spare high nibble
//   - tear-down indicator TV octet: '1001'B identifier + flag in bit 0
//   - SM cause as a bare value octet in reject/failure/status messages

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
    // ActivatePDPContextRequest minimal: PD=0x0A, MTI=0x41,
    // body=NSAPI(5)+LLC SAPI(5)+QoS(LV)+PDP address(LV, dynamic IPv4)+APN(TLV)
    // Hex: 0A 41 05 05 [QoS LV] [PDP address LV] [APN TLV]
    // QoS: 03 (length) 23 02 00 (default profile: delay '100'B, reliability
    //      '011'B, precedence '010'B, peak/mean throughput codes 0)
    // PDP address: 02 (length) 01 (typeOrg=IETF, spare=0) 21 (IPv4 type number, dynamic)
    // APN: 28 (IEI 0x28) 03 (length) 69 70 6e ("ipn")
    std::string hex = "0a 41 05 05 03 2302 00 02 0121 28 03 6970 6e";
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

// TS 44.068 9.5: ActivatePDPContextRequest with dynamic IPv4, APN, minimal QoS.
// Hex breakdown:
//   0a = PD=0x0A(SM) in the low nibble of byte 0, TI=0, TIF=0
//   41 = MTI(8)=0x41(ActivatePDPContextRequest)
//   05 = requestedNSAPI(4)=5 in the low nibble, spare(4)=0 high
//   05 = requestedLLCSAPI(4)=5 in the low nibble, spare(4)=0 high
//   03 = QoS LV length 3 (mandatory characteristics only)
//   23 = QoS octet 1: [spare(2)=0][delay '100'B=4][reliability '011'B=3]
//   02 = QoS octet 2: [peak throughput '0000'B=0][spare(1)=0][precedence '010'B=2]
//   00 = QoS octet 3: [spare(3)=0][mean throughput '00000'B=0]
//   02 = PDP address LV length 2 (dynamic assignment)
//   01 = typeOrg = IETF (high nibble), spare = 0 (low nibble)
//   21 = pdpTypeNumber = IETF IPv4 (no address octets: dynamic)
//   28 07 = APN TLV: IEI=0x28, length 7
//   69 70 2e 67 73 6d 2e = "ip.gsm." (APN string)
TEST(GoldenSMTest, ActivatePDPContextRequest_Minimal) {
    std::string hex = "0a 41 05 05 03 2302 00 02 0121 28 07 6970 2e67 736d 2e";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);
    EXPECT_EQ(messagePD(res.value()), L3PD::GPRSSessionManagement);

    auto* msg = tryGet<L3ActivatePDPContextRequest>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->nsapi(), 5u);
    EXPECT_EQ(msg->llcSapi(), 5u);
    EXPECT_EQ(msg->pdpAddress().typeNumber(), PdpTypeNumber::IPv4);
    EXPECT_TRUE(msg->pdpAddress().address().empty());
    EXPECT_EQ(msg->apn().value(), "ip.gsm.");
    EXPECT_EQ(msg->qos().delayClass(), 4u);
    EXPECT_EQ(msg->qos().reliabilityClass(), 3u);
    EXPECT_EQ(msg->qos().precedenceClass(), 2u);
}

// TS 44.068 9.5: ActivatePDPContextRequest with PDP address (IPv4, static).
// Hex breakdown:
//   0a 41 = L3 header (SM, ActivatePDPContextRequest)
//   05 05 = NSAPI=5, LLC SAPI=5 (low nibble of each octet)
//   03 23 02 00 = QoS LV: length 3, default profile
//   06 = PDP address LV length 6 (2 header octets + 4 IPv4 address octets)
//   01 = typeOrg = IETF (high nibble), spare = 0 (low nibble)
//   21 = pdpTypeNumber = IETF IPv4
//   c0 a8 01 01 = 192.168.1.1 (static address)
//   28 03 6970 6e = APN TLV: IEI=0x28, "ipn"
TEST(GoldenSMTest, ActivatePDPContextRequest_WithAddress) {
    std::string hex = "0a 41 05 05 03 2302 00 06 0121 c0a8 0101 28 03 6970 6e";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);

    auto* msg = tryGet<L3ActivatePDPContextRequest>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->nsapi(), 5u);
    EXPECT_EQ(msg->llcSapi(), 5u);
    EXPECT_EQ(msg->pdpAddress().typeNumber(), PdpTypeNumber::IPv4);
    EXPECT_TRUE(msg->pdpAddress().staticAddress());
    EXPECT_EQ(msg->pdpAddress().address().size(), 4u);
    EXPECT_EQ(msg->pdpAddress().address()[0], 0xc0);
    EXPECT_EQ(msg->pdpAddress().address()[1], 0xa8);
    EXPECT_EQ(msg->pdpAddress().address()[2], 0x01);
    EXPECT_EQ(msg->pdpAddress().address()[3], 0x01);
    EXPECT_EQ(msg->apn().value(), "ipn");
}

// Golden (TS 44.068 section 9.5): SM ACTIVATE PDP CONTEXT REQUEST full body —
// NSAPI=5, LLC SAPI=5 (low nibbles), QoS LV (default profile, three value
// octets), requested PDP address (LV: typeOrg IETF, IETF IPv4 type number,
// static address 192.168.1.2), APN TLV "inter"; no protocol configuration
// options and no request type present.
TEST(GoldenSMTest, ActivatePDPContextRequest_RefVector) {
    uint8_t data[] = {
        0x0A, 0x41,                    // header: PD=SM, MTI=0x41
        0x05, 0x05,                    // NSAPI=5, LLC SAPI=5 (low nibbles)
        0x03, 0x23, 0x02, 0x00,        // QoS LV (len=3): default profile
        0x06,                          // PDP address LV: len=6
        0x01,                          //   typeOrg = IETF (high nibble), spare = 0
        0x21,                          //   pdpTypeNumber = IETF IPv4
        0xC0, 0xA8, 0x01, 0x02,        //   static IPv4 address 192.168.1.2
        0x28, 0x05,                    // APN TLV: IEI=0x28, len=5
        'i', 'n', 't', 'e', 'r'        // APN string "inter"
    };
    auto res = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(res);

    const auto* msg = tryGet<L3ActivatePDPContextRequest>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->nsapi(), 5u);
    EXPECT_EQ(msg->llcSapi(), 5u);
    EXPECT_EQ(msg->qos().delayClass(), 4u);
    EXPECT_EQ(msg->qos().reliabilityClass(), 3u);
    EXPECT_EQ(msg->qos().precedenceClass(), 2u);
    EXPECT_TRUE(msg->qos().additionalCharacteristics().empty());
    EXPECT_EQ(msg->pdpAddress().typeNumber(), PdpTypeNumber::IPv4);
    EXPECT_TRUE(msg->pdpAddress().staticAddress());
    ASSERT_EQ(msg->pdpAddress().address().size(), 4u);
    EXPECT_EQ(msg->pdpAddress().address()[0], 0xC0);
    EXPECT_EQ(msg->pdpAddress().address()[1], 0xA8);
    EXPECT_TRUE(msg->hasApn());
    EXPECT_EQ(msg->apn().value(), "inter");
    EXPECT_FALSE(msg->hasPCO());
    EXPECT_FALSE(msg->hasRequestType());

    // Builder round-trip on the same object must reproduce identical bytes.
    auto rebuilt = L3ActivatePDPContextRequest::builder()
        .nsapi(5).llcSapi(5)
        .qos(L3QoS::defaultProfile())
        .pdpAddress(L3PDPAddress::ipv4Static(0xC0A80102u))
        .apn(L3AccessPointName{"inter"})
        .build();
    ParsedMessage pm{SM{rebuilt}};
    auto hex = writeL3Hex(pm);
    ASSERT_TRUE(hex);
    EXPECT_EQ(hex.value(), "0a41050503230200060121c0a801022805696e746572");
}

// Golden (TS 44.068 section 9.5): SM ACTIVATE PDP CONTEXT REQUEST with the
// optional request type TV octet '1010'B|value and protocol configuration
// options TLV (IEI=0x27); the typed accessors consume both.
TEST(GoldenSMTest, ActivatePDPContextRequest_WithRequestTypeAndPCO) {
    uint8_t data[] = {
        0x0A, 0x41,                    // header
        0x05, 0x05,                    // NSAPI=5, LLC SAPI=5 (low nibbles)
        0x03, 0x23, 0x02, 0x00,        // QoS LV (len=3): default profile
        0x02, 0x01, 0x21,              // PDP address LV: dynamic IPv4 (typeOrg IETF, type number 0x21)
        0x28, 0x03, 'i', 'p', 'n',     // APN TLV "ipn"
        0x27, 0x02, 0x01, 0x02,        // PCO TLV (IEI=0x27)
        0xA1                           // request type TV: '1010'B|1
    };
    auto res = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(res);

    const auto* msg = tryGet<L3ActivatePDPContextRequest>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_TRUE(msg->hasPCO());
    EXPECT_EQ(msg->pco().value(), (std::vector<uint8_t>{0x01, 0x02}));
    EXPECT_TRUE(msg->hasRequestType());
    EXPECT_EQ(msg->requestType(), 1u);
    EXPECT_TRUE(msg->additionalIes().empty());
}

// Golden (TS 44.068 section 9.5): SM ACTIVATE PDP CONTEXT REQUEST without an
// access point name — the APN TLV is optional, so the body ends after the
// PDP address LV: NSAPI=5, LLC SAPI=3 (low nibbles), QoS default profile,
// dynamic IPv4 address. Exact body: 05 03 03 23 02 00 02 01 21.
TEST(GoldenSMTest, ActivatePDPContextRequest_ApnAbsent) {
    uint8_t data[] = {
        0x0A, 0x41,                    // header: PD=SM, MTI=0x41
        0x05,                          // NSAPI=5 (low nibble)
        0x03,                          // LLC SAPI=3 (low nibble)
        0x03, 0x23, 0x02, 0x00,        // QoS LV: default profile
        0x02, 0x01, 0x21               // PDP address LV: dynamic IPv4
    };
    auto res = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(res);

    const auto* msg = tryGet<L3ActivatePDPContextRequest>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->nsapi(), 5u);
    EXPECT_EQ(msg->llcSapi(), 3u);
    EXPECT_FALSE(msg->hasApn());
    EXPECT_TRUE(msg->pdpAddress().address().empty());
    EXPECT_TRUE(msg->additionalIes().empty());

    // The builder emit must not append a 0x28 APN TLV.
    auto rebuilt = L3ActivatePDPContextRequest::builder()
        .nsapi(5).llcSapi(3)
        .qos(L3QoS::defaultProfile())
        .pdpAddress(L3PDPAddress::ipv4Dynamic())
        .build();
    ParsedMessage pm{SM{rebuilt}};
    auto hex = writeL3Hex(pm);
    ASSERT_TRUE(hex);
    EXPECT_EQ(hex.value(), "0a41050303230200020121");
}

// Golden (TS 44.068 section 9.5): SM ACTIVATE PDP CONTEXT REQUEST with the
// optional APN TLV present — the APN "test" follows the PDP address LV:
// ... 02 01 21 | 28 04 74 65 73 74.
TEST(GoldenSMTest, ActivatePDPContextRequest_ApnPresent) {
    uint8_t data[] = {
        0x0A, 0x41,                    // header: PD=SM, MTI=0x41
        0x05,                          // NSAPI=5 (low nibble)
        0x03,                          // LLC SAPI=3 (low nibble)
        0x03, 0x23, 0x02, 0x00,        // QoS LV: default profile
        0x02, 0x01, 0x21,              // PDP address LV: dynamic IPv4
        0x28, 0x04, 't', 'e', 's', 't' // APN TLV "test"
    };
    auto res = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(res);

    const auto* msg = tryGet<L3ActivatePDPContextRequest>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->nsapi(), 5u);
    EXPECT_EQ(msg->llcSapi(), 3u);
    EXPECT_TRUE(msg->hasApn());
    EXPECT_EQ(msg->apn().value(), "test");

    // Round-trip must be byte-identical (lossless).
    ParsedMessage pm{SM{*msg}};
    auto hex = writeL3Hex(pm);
    ASSERT_TRUE(hex);
    EXPECT_EQ(hex.value(), "0a41050303230200020121280474657374");
}

// =====================================================================
// Activate PDP Context Accept Golden Tests
// Wire layout per GSM 24.080 (Activate PDP Context Accept).
// =====================================================================

// TS 44.068 9.5: ActivatePDPContextAccept with assigned address.
// Hex breakdown:
//   0a 42 = L3 header (SM, ActivatePDPContextAccept)
//   03 = negotiatedLLCSAPI(4)=3 in the low nibble, spare(4)=0 high
//   03 23 02 00 = QoS LV: length 3, default profile
//   01 = radioPriorityForSMS(4)=1 in the low nibble, spare(4)=0 high
//   2b 06 0121 c0a8 0164 = PDP Address TLV (IEI=0x2B): typeOrg IETF,
//     IETF IPv4 type number 0x21, addr=192.168.1.100 (TS 24.008 10.5.6.4)
TEST(GoldenSMTest, ActivatePDPContextAccept_WithAddress) {
    std::string hex = "0a 42 03 03 2302 00 01 2b 06 0121 c0a8 0164";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);

    auto* msg = tryGet<L3ActivatePDPContextAccept>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->llcSapi(), 3u);
    EXPECT_EQ(msg->radioPriority(), 1u);
    EXPECT_TRUE(msg->hasPDPAddress());
    EXPECT_EQ(msg->pdpAddress().typeNumber(), PdpTypeNumber::IPv4);
    EXPECT_TRUE(msg->pdpAddress().staticAddress());
    EXPECT_EQ(msg->pdpAddress().address().size(), 4u);
    EXPECT_EQ(msg->qos().delayClass(), 4u);
    EXPECT_EQ(msg->qos().reliabilityClass(), 3u);

    // Builder emit must reproduce the exact reference bytes, including the
    // PDP Address TLV value octets (TS 24.008 section 10.5.6.4).
    auto rebuilt = L3ActivatePDPContextAccept::builder()
        .llcSapi(3)
        .qos(L3QoS::defaultProfile())
        .radioPriority(1)
        .pdpAddress(L3PDPAddress::ipv4Static(0xC0A80164u))
        .build();
    ParsedMessage pm{SM{rebuilt}};
    auto hexOut = writeL3Hex(pm);
    ASSERT_TRUE(hexOut);
    EXPECT_EQ(hexOut.value(), "0a420303230200012b060121c0a80164");
}

// TS 44.068 9.5: ActivatePDPContextAccept minimal (no address assigned).
// Hex breakdown:
//   0a 42 = L3 header (SM, Accept)
//   00 = negotiatedLLCSAPI(4)=0 in the low nibble, spare(4)=0 high
//   03 23 02 00 = QoS LV: length 3, default profile
//   00 = radioPriorityForSMS(4)=0 in the low nibble, spare(4)=0 high
TEST(GoldenSMTest, ActivatePDPContextAccept_Minimal) {
    std::string hex = "0a 42 00 03 2302 00 00";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);

    auto* msg = tryGet<L3ActivatePDPContextAccept>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->llcSapi(), 0u);
    EXPECT_FALSE(msg->hasPDPAddress());
}

// =====================================================================
// Activate PDP Context Reject Golden Tests
// Wire layout per GSM 24.080 (Activate PDP Context Reject).
// =====================================================================

// TS 44.068 9.5: ActivatePDPContextReject with cause and opaque additional IEs.
// Hex breakdown:
//   0a 43 = L3 header (SM, Reject)
//   1c = SM cause value octet (no identifier): Unknown_PDP_Address_Or_Type
//   a8 01 05 = opaque additional IE (back-off timer), preserved verbatim
TEST(GoldenSMTest, ActivatePDPContextReject_Full) {
    std::string hex = "0a 43 1c a8 01 05";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);

    auto* msg = tryGet<L3ActivatePDPContextReject>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->cause(), SMCause::Unknown_PDP_Address_Or_Type);
    EXPECT_EQ(msg->additionalIes(), (std::vector<uint8_t>{0xa8, 0x01, 0x05}));
}

// TS 44.068 9.5: ActivatePDPContextReject minimal (cause only).
// Hex breakdown:
//   0a 43 = L3 header (SM, Reject)
//   1c = SM cause value octet (no identifier): Unknown_PDP_Address_Or_Type
TEST(GoldenSMTest, ActivatePDPContextReject_Minimal) {
    std::string hex = "0a 43 1c";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);

    auto* msg = tryGet<L3ActivatePDPContextReject>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->cause(), SMCause::Unknown_PDP_Address_Or_Type);
    EXPECT_TRUE(msg->additionalIes().empty());
}

// Golden (TS 44.068 section 9.5): SM ACTIVATE PDP CONTEXT REJECT — the body
// is exactly one SM cause value octet (no identifier), no further IEs.
TEST(GoldenSMTest, ActivatePDPContextReject_RefVector) {
    uint8_t data[] = {0x0A, 0x43, 0x20};
    auto res = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(res);

    const auto* msg = tryGet<L3ActivatePDPContextReject>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(static_cast<uint8_t>(msg->cause()), 0x20u);
    EXPECT_EQ(msg->bodyLength(), 1u);
    EXPECT_TRUE(msg->additionalIes().empty());
}

// =====================================================================
// Deactivate PDP Context Request Golden Tests
// Wire layout per GSM 24.080 (MO and MT directions).
// =====================================================================

// TS 44.068 9.5: DeactivatePDPContextRequest with tear-down indicator and PCO.
// Hex breakdown:
//   0a 46 = L3 header (SM, DeactivatePDPContextRequest)
//   26 = SM cause value octet (no identifier): Network_Failure
//   91 = tear-down indicator TV: '1001'B + flag=1 in bit 0 (one octet)
//   27 02 0102 = PCO TLV: IEI=0x27, len=2, option data 01 02
TEST(GoldenSMTest, DeactivatePDPContextRequest_WithTearDown) {
    std::string hex = "0a 46 26 91 27 02 0102";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);

    auto* msg = tryGet<L3DeactivatePDPContextRequest>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(static_cast<uint8_t>(msg->cause()), 0x26u);
    EXPECT_TRUE(msg->hasTearDownIndicator());
    EXPECT_TRUE(msg->tearDownIndicator());
    EXPECT_TRUE(msg->hasPCO());
    EXPECT_EQ(msg->protocolConfigOptions().value(), (std::vector<uint8_t>{0x01, 0x02}));
}

// TS 44.068 9.5: DeactivatePDPContextRequest minimal (cause only).
// Hex breakdown:
//   0a 46 = L3 header (SM, DeactivatePDPContextRequest)
//   26 = SM cause value octet (no identifier): Network_Failure
TEST(GoldenSMTest, DeactivatePDPContextRequest_Minimal) {
    std::string hex = "0a 46 26";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);

    auto* msg = tryGet<L3DeactivatePDPContextRequest>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(static_cast<uint8_t>(msg->cause()), 0x26u);
    EXPECT_FALSE(msg->hasTearDownIndicator());
    EXPECT_FALSE(msg->tearDownIndicator());
}

// Golden (TS 44.068 section 9.5): SM DEACTIVATE PDP CONTEXT REQUEST — the
// body is the SM cause value octet followed by the single-octet tear-down
// indicator TV ('1001'B identifier + flag in bit 0 -> 0x91).
TEST(GoldenSMTest, DeactivatePDPContextRequest_RefVector) {
    uint8_t data[] = {0x0A, 0x46, 0x26, 0x91};
    auto res = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(res);

    const auto* msg = tryGet<L3DeactivatePDPContextRequest>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(static_cast<uint8_t>(msg->cause()), 0x26u);
    EXPECT_TRUE(msg->hasTearDownIndicator());
    EXPECT_TRUE(msg->tearDownIndicator());

    // Builder round-trip must reproduce the identical bytes.
    auto rebuilt = L3DeactivatePDPContextRequest::builder()
        .cause(SMCause::Network_Failure)
        .tearDownIndicator(true)
        .build();
    ParsedMessage pm{SM{rebuilt}};
    auto hex = writeL3Hex(pm);
    ASSERT_TRUE(hex);
    EXPECT_EQ(hex.value(), "0a462691");
}

// Golden (TS 44.068 section 9.5): tear-down indicator TV octet with the
// flag cleared is 0x90 ('1001'B identifier + flag=0 in bit 0).
TEST(GoldenSMTest, DeactivatePDPContextRequest_TearDownCleared) {
    uint8_t data[] = {0x0A, 0x46, 0x26, 0x90};
    auto res = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(res);

    const auto* msg = tryGet<L3DeactivatePDPContextRequest>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_TRUE(msg->hasTearDownIndicator());
    EXPECT_FALSE(msg->tearDownIndicator());
}

// =====================================================================
// Deactivate PDP Context Accept Golden Tests
// Wire layout per GSM 24.080 (MT and MO directions).
// =====================================================================

// TS 44.068 9.5: DeactivatePDPContextAccept carries no fixed body fields;
// optional IEs (here a protocol configuration options TLV) are kept opaque.
// Hex breakdown:
//   0a 47 = L3 header (SM, DeactivatePDPContextAccept)
//   27 01 01 = PCO TLV preserved in the opaque additional-IE sequence
TEST(GoldenSMTest, DeactivatePDPContextAccept) {
    std::string hex = "0a 47 27 01 01";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);

    auto* msg = tryGet<L3DeactivatePDPContextAccept>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->additionalIes(), (std::vector<uint8_t>{0x27, 0x01, 0x01}));
}

// =====================================================================
// Modify PDP Context Request Golden Tests
// Wire layout per GSM 24.080 (Modify PDP Context Request).
// =====================================================================

// TS 44.068 9.5: ModifyPDPContextRequest with QoS.
// Hex breakdown:
//   0a 48 = L3 header (SM, ModifyPDPContextRequest)
//   05 = pdpHandle(4)=5 in the low nibble, spare(4)=0 high
//   03 23 02 00 = QoS LV: length 3, default profile
TEST(GoldenSMTest, ModifyPDPContextRequest) {
    std::string hex = "0a 48 05 03 2302 00";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);

    auto* msg = tryGet<L3ModifyPDPContextRequest>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->pdpHandle(), 5u);
    EXPECT_EQ(msg->qos().delayClass(), 4u);
    EXPECT_EQ(msg->qos().reliabilityClass(), 3u);
}

// =====================================================================
// Modify PDP Context Accept Golden Tests
// Wire layout per GSM 24.080 (Modify PDP Context Accept).
// =====================================================================

// TS 44.068 9.5: ModifyPDPContextAccept with QoS.
// Hex breakdown:
//   0a 49 = L3 header (SM, ModifyPDPContextAccept)
//   05 = pdpHandle(4)=5 in the low nibble, spare(4)=0 high
//   03 23 02 00 = QoS LV: length 3, default profile
TEST(GoldenSMTest, ModifyPDPContextAccept) {
    std::string hex = "0a 49 05 03 2302 00";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);

    auto* msg = tryGet<L3ModifyPDPContextAccept>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->pdpHandle(), 5u);
    EXPECT_EQ(msg->qos().delayClass(), 4u);
    EXPECT_EQ(msg->qos().precedenceClass(), 2u);
}

// =====================================================================
// Modify PDP Context Reject Golden Tests
// Wire layout per GSM 24.080 (Modify PDP Context Reject).
// =====================================================================

// TS 44.068 9.5: ModifyPDPContextReject with cause and opaque additional IEs.
// Hex breakdown:
//   0a 4c = L3 header (SM, ModifyPDPContextReject)
//   1c = SM cause value octet (no identifier): Unknown_PDP_Address_Or_Type
//   a8 01 0a = opaque additional IE (back-off timer), preserved verbatim
TEST(GoldenSMTest, ModifyPDPContextReject_Full) {
    std::string hex = "0a 4c 1c a8 01 0a";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);

    auto* msg = tryGet<L3ModifyPDPContextReject>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->cause(), SMCause::Unknown_PDP_Address_Or_Type);
    EXPECT_EQ(msg->additionalIes(), (std::vector<uint8_t>{0xa8, 0x01, 0x0a}));
}

// =====================================================================
// SM Status Golden Tests
// Wire layout per GSM 24.080 (Status).
// =====================================================================

// TS 44.068 9.5: SMStatus with cause.
// Hex breakdown:
//   0a 55 = L3 header (SM, SMStatus)
//   24 = SM cause value octet (no identifier): Regular_Deactivation
TEST(GoldenSMTest, SMStatus) {
    std::string hex = "0a 55 24";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);

    auto* msg = tryGet<L3SMStatus>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->cause(), SMCause::Regular_Deactivation);
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

// TS 44.068 9.5: RequestPDPContextActivation with handle, QoS, APN.
// 0a 44 = header (SM, MTI=0x44)
// 03 = pdpHandle(4)=3 in the low nibble, spare(4)=0 high
// 03 23 02 00 = QoS LV: length 3, default profile
// 02 01 21 = PDP address LV: dynamic IPv4 (typeOrg IETF, type number 0x21)
// 28 03 6970 6e = APN TLV (IEI=0x28): "ipn"
TEST(GoldenSMTest, RequestPDPContextActivation) {
    std::string hex = "0a 44 03 03 2302 00 02 0121 28 03 6970 6e";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);
    EXPECT_EQ(messagePD(res.value()), L3PD::GPRSSessionManagement);
    auto* msg = tryGet<L3RequestPDPContextActivation>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->pdpHandle(), 3u);
    EXPECT_EQ(msg->apn().value(), "ipn");
    EXPECT_EQ(msg->qos().delayClass(), 4u);
}

// TS 44.068 9.5: RequestPDPContextActivationReject
// 0a 45 = header, 1c = SM cause value octet (Unknown_PDP_Address_Or_Type)
TEST(GoldenSMTest, RequestPDPContextActivationReject) {
    std::string hex = "0a 45 1c";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);
    auto* msg = tryGet<L3RequestPDPContextActivationReject>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->cause(), SMCause::Unknown_PDP_Address_Or_Type);
}

// TS 44.068 9.5: ModifyPDPContextRequest (MS->Net)
TEST(GoldenSMTest, ModifyPDPContextRequestMS) {
    std::string hex = "0a 4a 04 03 2302 00";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);
    auto* msg = tryGet<L3ModifyPDPContextRequestMS>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->pdpHandle(), 4u);
}

// TS 44.068 9.5: ModifyPDPContextAccept (Net->MS)
TEST(GoldenSMTest, ModifyPDPContextAcceptNet) {
    std::string hex = "0a 4b 06 03 2302 00";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);
    auto* msg = tryGet<L3ModifyPDPContextAcceptNet>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->pdpHandle(), 6u);
}

// TS 44.068 9.5: ActivateSecondaryPDPContextRequest
TEST(GoldenSMTest, ActivateSecondaryPDPContextRequest) {
    std::string hex = "0a 4d 02 03 2302 00 02 0121 28 03 6970 6e";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);
    auto* msg = tryGet<L3ActivateSecondaryPDPContextRequest>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->pdpHandle(), 2u);
    EXPECT_EQ(msg->apn().value(), "ipn");
}

// TS 44.068 9.5: ActivateSecondaryPDPContextAccept
TEST(GoldenSMTest, ActivateSecondaryPDPContextAccept) {
    std::string hex = "0a 4e 01 03 2302 00";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);
    auto* msg = tryGet<L3ActivateSecondaryPDPContextAccept>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->pdpHandle(), 1u);
}

// TS 44.068 9.5: ActivateSecondaryPDPContextReject
TEST(GoldenSMTest, ActivateSecondaryPDPContextReject) {
    std::string hex = "0a 4f 20";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);
    auto* msg = tryGet<L3ActivateSecondaryPDPContextReject>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(static_cast<uint8_t>(msg->cause()), 0x20u);
}

// TS 44.068 9.5: ActivateMBMSContextRequest with QoS and opaque TMGI TLV
// 0a 56 = header
// 03 23 02 00 = QoS LV: length 3, default profile
// c2 06 = TMGI TLV (length 6) preserved in the opaque additional-IE sequence
// 45 f7 10 = PLMN
// 12 34 = Service ID
// 05 = Session ID
TEST(GoldenSMTest, ActivateMBMSContextRequest) {
    std::string hex = "0a 56 03 2302 00 c2 06 45f7 1012 3405";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);
    auto* msg = tryGet<L3ActivateMBMSContextRequest>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->qos().delayClass(), 4u);
    // The TMGI TLV bytes are preserved verbatim in the opaque sequence.
    EXPECT_EQ(msg->additionalIes()[2], 0x45u);
    EXPECT_EQ(msg->additionalIes()[3], 0xF7u);
    EXPECT_EQ(msg->additionalIes()[5], 0x12u);
}

// TS 44.068 9.5: ActivateMBMSContextAccept
TEST(GoldenSMTest, ActivateMBMSContextAccept) {
    std::string hex = "0a 57 0c 03 2302 00";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);
    auto* msg = tryGet<L3ActivateMBMSContextAccept>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->pdpHandle(), 12u);
}

// TS 44.068 9.5: ActivateMBMSContextReject
TEST(GoldenSMTest, ActivateMBMSContextReject) {
    std::string hex = "0a 58 1c";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);
    auto* msg = tryGet<L3ActivateMBMSContextReject>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->cause(), SMCause::Unknown_PDP_Address_Or_Type);
}

// TS 44.068 9.5: RequestMBMSContextActivation
TEST(GoldenSMTest, RequestMBMSContextActivation) {
    std::string hex = "0a 59 03 2302 00 c2 06 45f7 1012 3405";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);
    auto* msg = tryGet<L3RequestMBMSContextActivation>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->qos().delayClass(), 4u);
    // The TMGI TLV bytes are preserved verbatim in the opaque sequence.
    EXPECT_EQ(msg->additionalIes()[5], 0x12u);
}

// TS 44.068 9.5: RequestMBMSContextActivationReject
TEST(GoldenSMTest, RequestMBMSContextActivationReject) {
    std::string hex = "0a 5a 1c";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);
    auto* msg = tryGet<L3RequestMBMSContextActivationReject>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->cause(), SMCause::Unknown_PDP_Address_Or_Type);
}

// TS 44.068 9.5: RequestSecondaryPDPContextActivation
TEST(GoldenSMTest, RequestSecondaryPDPContextActivation) {
    std::string hex = "0a 5b 0d 03 2302 00 02 0121 28 03 6970 6e";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);
    auto* msg = tryGet<L3RequestSecondaryPDPContextActivation>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->pdpHandle(), 13u);
}

// TS 44.068 9.5: RequestSecondaryPDPContextActivationReject
TEST(GoldenSMTest, RequestSecondaryPDPContextActivationReject) {
    std::string hex = "0a 5c 1c";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);
    auto* msg = tryGet<L3RequestSecondaryPDPContextActivationReject>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->cause(), SMCause::Unknown_PDP_Address_Or_Type);
}

// GSM 24.008 9.5.25: SMNotification
TEST(GoldenSMTest, SMNotification) {
    std::string hex = "0a 5d 0f";
    auto res = parseL3Hex(hex);
    ASSERT_TRUE(res);
    auto* msg = tryGet<L3SMNotification>(res.value());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->pdpHandle(), 15u);
}

// ── Roundtrip tests for additional SM messages ──

TEST(RoundTripTest, RequestPDPContextActivation_RT) {
    auto res = parseL3Hex("0a 44 03 03 2302 00 02 0121 28 03 6970 6e");
    ASSERT_TRUE(res);
    auto rt = roundtrip(res.value());
    ASSERT_TRUE(rt);
    auto* m = tryGet<L3RequestPDPContextActivation>(rt.value());
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->pdpHandle(), 3u);
}

TEST(RoundTripTest, RequestPDPContextActivationReject_RT) {
    auto res = parseL3Hex("0a 45 1c");
    ASSERT_TRUE(res);
    auto rt = roundtrip(res.value());
    ASSERT_TRUE(rt);
    auto* m = tryGet<L3RequestPDPContextActivationReject>(rt.value());
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->cause(), SMCause::Unknown_PDP_Address_Or_Type);
}

TEST(RoundTripTest, ModifyPDPContextRequestMS_RT) {
    auto res = parseL3Hex("0a 4a 04 03 2302 00");
    ASSERT_TRUE(res);
    auto rt = roundtrip(res.value());
    ASSERT_TRUE(rt);
    auto* m = tryGet<L3ModifyPDPContextRequestMS>(rt.value());
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->pdpHandle(), 4u);
}

TEST(RoundTripTest, ModifyPDPContextAcceptNet_RT) {
    auto res = parseL3Hex("0a 4b 06 03 2302 00");
    ASSERT_TRUE(res);
    auto rt = roundtrip(res.value());
    ASSERT_TRUE(rt);
    auto* m = tryGet<L3ModifyPDPContextAcceptNet>(rt.value());
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->pdpHandle(), 6u);
}

TEST(RoundTripTest, ActivateSecondaryPDPContextRequest_RT) {
    auto res = parseL3Hex("0a 4d 02 03 2302 00 02 0121 28 03 6970 6e");
    ASSERT_TRUE(res);
    auto rt = roundtrip(res.value());
    ASSERT_TRUE(rt);
    auto* m = tryGet<L3ActivateSecondaryPDPContextRequest>(rt.value());
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->pdpHandle(), 2u);
}

TEST(RoundTripTest, ActivateSecondaryPDPContextAccept_RT) {
    auto res = parseL3Hex("0a 4e 01 03 2302 00");
    ASSERT_TRUE(res);
    auto rt = roundtrip(res.value());
    ASSERT_TRUE(rt);
    auto* m = tryGet<L3ActivateSecondaryPDPContextAccept>(rt.value());
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->pdpHandle(), 1u);
}

TEST(RoundTripTest, ActivateSecondaryPDPContextReject_RT) {
    auto res = parseL3Hex("0a 4f 20");
    ASSERT_TRUE(res);
    auto rt = roundtrip(res.value());
    ASSERT_TRUE(rt);
    auto* m = tryGet<L3ActivateSecondaryPDPContextReject>(rt.value());
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(static_cast<uint8_t>(m->cause()), 0x20u);
}

TEST(RoundTripTest, ActivateMBMSContextRequest_RT) {
    auto res = parseL3Hex("0a 56 03 2302 00 c2 06 45f7 1012 3405");
    ASSERT_TRUE(res);
    auto rt = roundtrip(res.value());
    ASSERT_TRUE(rt);
    auto* m = tryGet<L3ActivateMBMSContextRequest>(rt.value());
    ASSERT_NE(m, nullptr);
    // The TMGI TLV bytes survive the round-trip in the opaque sequence.
    ASSERT_GE(m->additionalIes().size(), 7u);
    EXPECT_EQ(m->additionalIes()[5], 0x12u);
}

TEST(RoundTripTest, ActivateMBMSContextAccept_RT) {
    auto res = parseL3Hex("0a 57 0c 03 2302 00");
    ASSERT_TRUE(res);
    auto rt = roundtrip(res.value());
    ASSERT_TRUE(rt);
    auto* m = tryGet<L3ActivateMBMSContextAccept>(rt.value());
    ASSERT_NE(m, nullptr);
}

TEST(RoundTripTest, ActivateMBMSContextReject_RT) {
    auto res = parseL3Hex("0a 58 1c");
    ASSERT_TRUE(res);
    auto rt = roundtrip(res.value());
    ASSERT_TRUE(rt);
    auto* m = tryGet<L3ActivateMBMSContextReject>(rt.value());
    ASSERT_NE(m, nullptr);
}

TEST(RoundTripTest, RequestMBMSContextActivation_RT) {
    auto res = parseL3Hex("0a 59 03 2302 00 c2 06 45f7 1012 3405");
    ASSERT_TRUE(res);
    auto rt = roundtrip(res.value());
    ASSERT_TRUE(rt);
    auto* m = tryGet<L3RequestMBMSContextActivation>(rt.value());
    ASSERT_NE(m, nullptr);
}

TEST(RoundTripTest, RequestMBMSContextActivationReject_RT) {
    auto res = parseL3Hex("0a 5a 1c");
    ASSERT_TRUE(res);
    auto rt = roundtrip(res.value());
    ASSERT_TRUE(rt);
    auto* m = tryGet<L3RequestMBMSContextActivationReject>(rt.value());
    ASSERT_NE(m, nullptr);
}

TEST(RoundTripTest, RequestSecondaryPDPContextActivation_RT) {
    auto res = parseL3Hex("0a 5b 0d 03 2302 00 02 0121 28 03 6970 6e");
    ASSERT_TRUE(res);
    auto rt = roundtrip(res.value());
    ASSERT_TRUE(rt);
    auto* m = tryGet<L3RequestSecondaryPDPContextActivation>(rt.value());
    ASSERT_NE(m, nullptr);
}

TEST(RoundTripTest, RequestSecondaryPDPContextActivationReject_RT) {
    auto res = parseL3Hex("0a 5c 1c");
    ASSERT_TRUE(res);
    auto rt = roundtrip(res.value());
    ASSERT_TRUE(rt);
    auto* m = tryGet<L3RequestSecondaryPDPContextActivationReject>(rt.value());
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->cause(), SMCause::Unknown_PDP_Address_Or_Type);
}

TEST(RoundTripTest, SMNotification_RT) {
    auto res = parseL3Hex("0a 5d 0f");
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
    auto res = parseL3Hex("0a 41 05 05 03 2302 00 02 0121 28 08 696e74 65726e 6574");
    ASSERT_TRUE(res);

    auto rt = roundtrip(res.value());
    ASSERT_TRUE(rt);

    auto* m = tryGet<L3ActivatePDPContextRequest>(rt.value());
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->pdpAddress().typeNumber(), PdpTypeNumber::IPv4);
    EXPECT_TRUE(m->hasApn());
    EXPECT_EQ(m->apn().value(), "internet");
    EXPECT_EQ(m->qos().delayClass(), 4u);
}

// TS 44.068 9.5: ActivatePDPContextAccept round-trip.
TEST(RoundTripTest, ActivatePDPContextAccept_Full) {
    auto res = parseL3Hex("0a 42 01 03 2302 00 00 2b 06 0121 c0a8 0164");
    ASSERT_TRUE(res);

    auto rt = roundtrip(res.value());
    ASSERT_TRUE(rt);

    auto* m = tryGet<L3ActivatePDPContextAccept>(rt.value());
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->llcSapi(), 1u);
    EXPECT_TRUE(m->hasPDPAddress());
}

// TS 44.068 9.5: ActivatePDPContextReject round-trip.
TEST(RoundTripTest, ActivatePDPContextReject_Full) {
    auto res = parseL3Hex("0a 43 1c a8 01 05");
    ASSERT_TRUE(res);

    auto rt = roundtrip(res.value());
    ASSERT_TRUE(rt);

    auto* m = tryGet<L3ActivatePDPContextReject>(rt.value());
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->cause(), SMCause::Unknown_PDP_Address_Or_Type);
    EXPECT_EQ(m->additionalIes(), (std::vector<uint8_t>{0xa8, 0x01, 0x05}));
}

// TS 44.068 9.5: DeactivatePDPContextRequest round-trip.
TEST(RoundTripTest, DeactivatePDPContextRequest_Full) {
    auto res = parseL3Hex("0a 46 26 91");
    ASSERT_TRUE(res);

    auto rt = roundtrip(res.value());
    ASSERT_TRUE(rt);

    auto* m = tryGet<L3DeactivatePDPContextRequest>(rt.value());
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(static_cast<uint8_t>(m->cause()), 0x26u);
    EXPECT_TRUE(m->tearDownIndicator());
}

// TS 44.068 9.5: DeactivatePDPContextAccept round-trip (no fixed fields).
TEST(RoundTripTest, DeactivatePDPContextAccept_Full) {
    auto res = parseL3Hex("0a 47 27 01 01");
    ASSERT_TRUE(res);

    auto rt = roundtrip(res.value());
    ASSERT_TRUE(rt);

    auto* m = tryGet<L3DeactivatePDPContextAccept>(rt.value());
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->additionalIes(), (std::vector<uint8_t>{0x27, 0x01, 0x01}));
}

// TS 44.068 9.5: ModifyPDPContextRequest round-trip.
TEST(RoundTripTest, ModifyPDPContextRequest_Full) {
    auto res = parseL3Hex("0a 48 05 03 2302 00");
    ASSERT_TRUE(res);

    auto rt = roundtrip(res.value());
    ASSERT_TRUE(rt);

    auto* m = tryGet<L3ModifyPDPContextRequest>(rt.value());
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->pdpHandle(), 5u);
}

// TS 44.068 9.5: ModifyPDPContextAccept round-trip.
TEST(RoundTripTest, ModifyPDPContextAccept_Full) {
    auto res = parseL3Hex("0a 49 05 03 2302 00");
    ASSERT_TRUE(res);

    auto rt = roundtrip(res.value());
    ASSERT_TRUE(rt);

    auto* m = tryGet<L3ModifyPDPContextAccept>(rt.value());
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->pdpHandle(), 5u);
}

// TS 44.068 9.5: ModifyPDPContextReject round-trip.
TEST(RoundTripTest, ModifyPDPContextReject_Full) {
    auto res = parseL3Hex("0a 4c 1c a8 01 0a");
    ASSERT_TRUE(res);

    auto rt = roundtrip(res.value());
    ASSERT_TRUE(rt);

    auto* m = tryGet<L3ModifyPDPContextReject>(rt.value());
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->cause(), SMCause::Unknown_PDP_Address_Or_Type);
}

// TS 44.068 9.5: SMStatus round-trip.
TEST(RoundTripTest, SMStatus_Full) {
    auto res = parseL3Hex("0a 55 24");
    ASSERT_TRUE(res);

    auto rt = roundtrip(res.value());
    ASSERT_TRUE(rt);

    auto* m = tryGet<L3SMStatus>(rt.value());
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->cause(), SMCause::Regular_Deactivation);
}

// =====================================================================
// SM IE Tests
// =====================================================================

TEST(SMIEsTest, PDPAddress_IPv4) {
    uint8_t addr[] = {0x01, 0x21, 0xc0, 0xa8, 0x01, 0x01}; // typeOrg IETF|spare=0, IPv4 type number 0x21, 192.168.1.1
    std::vector<uint8_t> bytes(std::begin(addr), std::end(addr));
    BitReader br(bytes.data(), bytes.size() * 8);

    auto res = L3PDPAddress::parse(br, 6);
    ASSERT_TRUE(res);
    EXPECT_EQ(res.value().typeNumber(), PdpTypeNumber::IPv4);
    EXPECT_EQ(res.value().typeOrg(), 0x01u);
    EXPECT_TRUE(res.value().staticAddress());
    EXPECT_EQ(res.value().address()[0], 0xc0);
    EXPECT_EQ(res.value().address()[1], 0xa8);

    // The write path must reproduce the exact value octets (TS 24.008 section 10.5.6.4).
    uint8_t out[6] = {};
    BitWriter bw(out, 48);
    res.value().write(bw);
    EXPECT_EQ((std::vector<uint8_t>(out, out + 6)), (std::vector<uint8_t>(addr, addr + 6)));
}

// TS 24.008 10.5.6.4: dynamic PDP addresses carry no address octets — the
// value part is exactly two octets (typeOrg|spare, type number).
TEST(SMIEsTest, PDPAddress_DynamicIPv4) {
    uint8_t addr[] = {0x01, 0x21};
    std::vector<uint8_t> bytes(std::begin(addr), std::end(addr));
    BitReader br(bytes.data(), bytes.size() * 8);

    auto res = L3PDPAddress::parse(br, 2);
    ASSERT_TRUE(res);
    EXPECT_EQ(res.value().typeNumber(), PdpTypeNumber::IPv4);
    EXPECT_FALSE(res.value().staticAddress());
    EXPECT_TRUE(res.value().address().empty());
    EXPECT_EQ(res.value().lengthV(), 2u);

    uint8_t out[2] = {};
    BitWriter bw(out, 16);
    res.value().write(bw);
    EXPECT_EQ((std::vector<uint8_t>(out, out + 2)), (std::vector<uint8_t>(addr, addr + 2)));
}

// TS 24.008 10.5.6.4: static IPv6 address carries sixteen octets after the
// typeOrg and type number octets (value part length 18).
TEST(SMIEsTest, PDPAddress_StaticIPv6) {
    uint8_t addr[] = {
        0x01, 0x53,
        0x20, 0x01, 0x0d, 0xb8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x01, 0x0a, 0x0b
    };
    std::vector<uint8_t> bytes(std::begin(addr), std::end(addr));
    BitReader br(bytes.data(), bytes.size() * 8);

    auto res = L3PDPAddress::parse(br, 18);
    ASSERT_TRUE(res);
    EXPECT_EQ(res.value().typeNumber(), PdpTypeNumber::IPv6);
    EXPECT_TRUE(res.value().staticAddress());
    EXPECT_EQ(res.value().address().size(), 16u);
    EXPECT_EQ(res.value().lengthV(), 18u);

    uint8_t out[18] = {};
    BitWriter bw(out, 144);
    res.value().write(bw);
    EXPECT_EQ((std::vector<uint8_t>(out, out + 18)), (std::vector<uint8_t>(addr, addr + 18)));
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

// TS 24.008 10.5.6.5: the default profile value octets are 23 02 00
// (reliability '011'B, delay '100'B, precedence '010'B, peak/mean 0).
TEST(SMIEsTest, QoS_Requested) {
    uint8_t data[] = {0x23, 0x02, 0x00}; // default profile (TS 24.008 Table 10.5.156)
    std::vector<uint8_t> bytes(std::begin(data), std::end(data));
    BitReader br(bytes.data(), bytes.size() * 8);

    auto res = L3QoS::parse(br, 3);
    ASSERT_TRUE(res);
    EXPECT_EQ(res.value().reliabilityClass(), 3u);
    EXPECT_EQ(res.value().delayClass(), 4u);
    EXPECT_EQ(res.value().precedenceClass(), 2u);
    EXPECT_EQ(res.value().peakThroughput(), 0u);
    EXPECT_EQ(res.value().meanThroughput(), 0u);

    // The write path must reproduce the exact value octets (TS 24.008 section 10.5.6.5).
    uint8_t out[3] = {};
    BitWriter bw(out, 24);
    res.value().write(bw);
    EXPECT_EQ((std::vector<uint8_t>(out, out + 3)), (std::vector<uint8_t>(data, data + 3)));

    // defaultProfile() must produce the same three octets.
    L3QoS def = L3QoS::defaultProfile();
    uint8_t out2[3] = {};
    BitWriter bw2(out2, 24);
    def.write(bw2);
    EXPECT_EQ((std::vector<uint8_t>(out2, out2 + 3)), (std::vector<uint8_t>(data, data + 3)));
}

// TS 24.008 10.5.6.5: optional characteristics from the fourth value octet
// on are preserved verbatim — parse and re-emit must be lossless.
TEST(SMIEsTest, QoS_WithOptionalCharacteristics) {
    uint8_t data[] = {0x23, 0x02, 0x00, 0x1e, 0x05}; // default profile + traffic class etc.
    std::vector<uint8_t> bytes(std::begin(data), std::end(data));
    BitReader br(bytes.data(), bytes.size() * 8);

    auto res = L3QoS::parse(br, 5);
    ASSERT_TRUE(res);
    EXPECT_EQ(res.value().delayClass(), 4u);
    EXPECT_EQ(res.value().additionalCharacteristics(), (std::vector<uint8_t>{0x1e, 0x05}));

    uint8_t out[5] = {};
    BitWriter bw(out, 40);
    res.value().write(bw);
    EXPECT_EQ((std::vector<uint8_t>(out, out + 5)), (std::vector<uint8_t>(data, data + 5)));
}

TEST(SMIEsTest, SMCauseToString) {
    EXPECT_STREQ(SMCause2Str(SMCause::Unspecified), "Unspecified");
    EXPECT_STREQ(SMCause2Str(SMCause::Unknown_PDP_Address_Or_Type), "Unknown PDP address or PDP type");
    EXPECT_STREQ(SMCause2Str(SMCause::Protocol_Error_Unspecified), "Protocol error unspecified");
}

// TS 24.008 10.5.5.6: every defined SM cause value must round-trip through a
// message body (here the Deactivate PDP Context Request cause octet) without
// loss or re-mapping.
TEST(SMIEsTest, SMCauseValuesRoundTrip) {
    std::vector<SMCause> causes = {
        SMCause::Operator_Determined_Barring,
        SMCause::MBMS_Bearer_Caps_Insufficient,
        SMCause::LLC_Or_SNDPC_Failure,
        SMCause::Insufficient_Resources,
        SMCause::Missing_Or_Unknown_APN,
        SMCause::Unknown_PDP_Address_Or_Type,
        SMCause::User_Authentication_Failed,
        SMCause::Activation_Rejected_By_GGSN,
        SMCause::Activation_Rejected_Unspecified,
        SMCause::Service_Option_Not_Supported,
        SMCause::Requested_Service_Option_Not_Subscribed,
        SMCause::Service_Option_Temporarily_Out_Of_Order,
        SMCause::NSAPI_Already_Used,
        SMCause::Regular_Deactivation,
        SMCause::QoS_Not_Accepted,
        SMCause::Network_Failure,
        SMCause::Reactivation_Required,
        SMCause::Feature_Not_Supported,
        SMCause::Semantic_Error_In_TFT,
        SMCause::Syntactical_Error_In_TFT,
        SMCause::Unknown_PDP_Context,
        SMCause::Semantic_Errors_In_Packet_Filter,
        SMCause::Syntactical_Errors_In_Packet_Filter,
        SMCause::PDP_Context_Without_TFT_Already_Activated,
        SMCause::Multicast_Group_Membership_Timeout,
        SMCause::Activation_Rejected_BCM_Violation,
        SMCause::PDP_Type_IPv4_Only_Allowed,
        SMCause::PDP_Type_IPv6_Only_Allowed,
        SMCause::Single_Address_Bearers_Only_Allowed,
        SMCause::Collision_With_Network_Initiated_Request,
        SMCause::Invalid_Transaction_Identifier_Value,
        SMCause::Semantically_Incorrect_Message,
        SMCause::Invalid_Mandatory_Information,
        SMCause::Message_Type_Not_Existent,
        SMCause::Message_Type_Not_Compatible_With_State,
        SMCause::IE_Not_Existent,
        SMCause::Conditional_IE_Error,
        SMCause::Message_Not_Compatible,
        SMCause::Protocol_Error_Unspecified,
        SMCause::APN_Restriction_Value_Incompatible,
    };

    for (SMCause cause : causes) {
        auto msg = L3DeactivatePDPContextRequest::builder().cause(cause).build();
        ParsedMessage pm{SM{msg}};
        auto hex = writeL3Hex(pm);
        ASSERT_TRUE(hex) << "write failed for cause 0x" << std::hex
                         << static_cast<int>(static_cast<uint8_t>(cause));
        auto res = parseL3Hex(hex.value());
        ASSERT_TRUE(res) << "parse failed for cause 0x" << std::hex
                         << static_cast<int>(static_cast<uint8_t>(cause));
        const auto* m = tryGet<L3DeactivatePDPContextRequest>(res.value());
        ASSERT_NE(m, nullptr);
        EXPECT_EQ(m->cause(), cause)
            << "round-trip mismatch for cause 0x" << std::hex
            << static_cast<int>(static_cast<uint8_t>(cause));
    }
}

// =====================================================================
// Visitor tests for SM messages
// =====================================================================

TEST(SMVisitorTest, MessageNames) {
    auto names = std::vector<std::pair<std::string_view, std::string>>{
        {"0a 41 05 05 03 2302 00 02 0121 28 03 6970 6e", "ActivatePDPContextRequest"},
        {"0a 42 00 03 2302 00 00", "ActivatePDPContextAccept"},
        {"0a 43 1c", "ActivatePDPContextReject"},
        {"0a 46 26", "DeactivatePDPContextRequest"},
        {"0a 47 27 01 01", "DeactivatePDPContextAccept"},
        {"0a 48 05 03 2302 00", "ModifyPDPContextRequest"},
        {"0a 49 05 03 2302 00", "ModifyPDPContextAccept"},
        {"0a 4c 1c", "ModifyPDPContextReject"},
        {"0a 55 24", "SMStatus"},
        {"0a 44 03 03 2302 00 02 0121 28 03 6970 6e", "RequestPDPContextActivation"},
        {"0a 45 1c", "RequestPDPContextActivationReject"},
        {"0a 4a 04 03 2302 00", "ModifyPDPContextRequestMS"},
        {"0a 4b 06 03 2302 00", "ModifyPDPContextAcceptNet"},
        {"0a 4d 02 03 2302 00 02 0121 28 03 6970 6e", "ActivateSecondaryPDPContextRequest"},
        {"0a 4e 01 03 2302 00", "ActivateSecondaryPDPContextAccept"},
        {"0a 4f 20", "ActivateSecondaryPDPContextReject"},
        {"0a 56 03 2302 00 c2 06 45f7 1012 3405", "ActivateMBMSContextRequest"},
        {"0a 57 0c 03 2302 00", "ActivateMBMSContextAccept"},
        {"0a 58 1c", "ActivateMBMSContextReject"},
        {"0a 59 03 2302 00 c2 06 45f7 1012 3405", "RequestMBMSContextActivation"},
        {"0a 5a 1c", "RequestMBMSContextActivationReject"},
        {"0a 5b 0d 03 2302 00 02 0121 28 03 6970 6e", "RequestSecondaryPDPContextActivation"},
        {"0a 5c 1c", "RequestSecondaryPDPContextActivationReject"},
        {"0a 5d 0f", "SMNotification"},
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
        {"0a 41 05 05 03 2302 00 02 0121 28 03 6970 6e", 0x41},
        {"0a 42 00 03 2302 00 00", 0x42},
        {"0a 43 1c", 0x43},
        {"0a 46 26", 0x46},
        {"0a 47 27 01 01", 0x47},
        {"0a 48 05 03 2302 00", 0x48},
        {"0a 49 05 03 2302 00", 0x49},
        {"0a 4c 1c", 0x4c},
        {"0a 55 24", 0x55},
        {"0a 44 03 03 2302 00 02 0121 28 03 6970 6e", 0x44},
        {"0a 45 1c", 0x45},
        {"0a 4a 04 03 2302 00", 0x4A},
        {"0a 4b 06 03 2302 00", 0x4B},
        {"0a 4d 02 03 2302 00 02 0121 28 03 6970 6e", 0x4D},
        {"0a 4e 01 03 2302 00", 0x4E},
        {"0a 4f 20", 0x4F},
        {"0a 56 03 2302 00 c2 06 45f7 1012 3405", 0x56},
        {"0a 57 0c 03 2302 00", 0x57},
        {"0a 58 1c", 0x58},
        {"0a 59 03 2302 00 c2 06 45f7 1012 3405", 0x59},
        {"0a 5a 1c", 0x5A},
        {"0a 5b 0d 03 2302 00 02 0121 28 03 6970 6e", 0x5B},
        {"0a 5c 1c", 0x5C},
        {"0a 5d 0f", 0x5D},
    };

    for (auto& [hex, expectedMTI] : mtis) {
        auto res = parseL3Hex(hex);
        ASSERT_TRUE(res) << "Failed to parse: " << hex;
        EXPECT_EQ(messageMTI(res.value()), expectedMTI) << "For hex: " << hex;
    }
}
