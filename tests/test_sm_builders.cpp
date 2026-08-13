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

// SM Builder tests: round-trip serialization for every SM message type.
// Reference: 3GPP TS 24.008 Chapter 9.5 (SM messages)

#include <gtest/gtest.h>
#include <gsml3parser/parser.h>
#include <gsml3parser/sm/l3smmessages.h>
#include <gsml3parser/visitor.h>

using namespace gsml3parser;

static Expected<ParsedMessage> roundtrip(const ParsedMessage& msg) {
    auto bytes = writeL3Bytes(msg);
    if (!bytes) return Expected<ParsedMessage>::error(bytes.error());
    return parseL3(*bytes);
}

// 3GPP TS 24.008 9.5.1: Activate PDP Context Request
TEST(SMBuilders, ActivatePDPContextRequest) {
    auto msg = L3ActivatePDPContextRequest::builder()
        .pdpType(PDPType::IPv4)
        .apn(L3AccessPointName("internet"))
        .qos(L3QoS{})
        .build();
    ParsedMessage pm{SM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
    EXPECT_EQ((*bytes)[0], 0xA0); // PD=10(SM)

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3ActivatePDPContextRequest::MTI);
}

// 3GPP TS 24.008 9.5.2: Activate PDP Context Accept
TEST(SMBuilders, ActivatePDPContextAccept) {
    auto msg = L3ActivatePDPContextAccept::builder()
        .pdpHandle(1)
        .qos(L3QoS{})
        .build();
    ParsedMessage pm{SM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3ActivatePDPContextAccept::MTI);
}

// 3GPP TS 24.008 9.5.3: Activate PDP Context Reject
TEST(SMBuilders, ActivatePDPContextReject) {
    auto msg = L3ActivatePDPContextReject::builder()
        .cause(SMCause::Invalid_Mandatory_Information)
        .build();
    ParsedMessage pm{SM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3ActivatePDPContextReject::MTI);
}

// 3GPP TS 24.008 9.5.4: Deactivate PDP Context Request
TEST(SMBuilders, DeactivatePDPContextRequest) {
    auto msg = L3DeactivatePDPContextRequest::builder()
        .pdpHandle(1)
        .build();
    ParsedMessage pm{SM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3DeactivatePDPContextRequest::MTI);
}

// 3GPP TS 24.008 9.5.5: Deactivate PDP Context Accept
TEST(SMBuilders, DeactivatePDPContextAccept) {
    auto msg = L3DeactivatePDPContextAccept::builder()
        .pdpHandle(1)
        .build();
    ParsedMessage pm{SM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3DeactivatePDPContextAccept::MTI);
}

// 3GPP TS 24.008 9.5.6: Modify PDP Context Request
TEST(SMBuilders, ModifyPDPContextRequest) {
    auto msg = L3ModifyPDPContextRequest::builder()
        .pdpHandle(1)
        .qos(L3QoS{})
        .build();
    ParsedMessage pm{SM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3ModifyPDPContextRequest::MTI);
}

// 3GPP TS 24.008 9.5.7: Modify PDP Context Accept
TEST(SMBuilders, ModifyPDPContextAccept) {
    auto msg = L3ModifyPDPContextAccept::builder()
        .pdpHandle(1)
        .qos(L3QoS{})
        .build();
    ParsedMessage pm{SM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3ModifyPDPContextAccept::MTI);
}

// 3GPP TS 24.008 9.5.8: Modify PDP Context Reject
TEST(SMBuilders, ModifyPDPContextReject) {
    auto msg = L3ModifyPDPContextReject::builder()
        .pdpHandle(1)
        .cause(SMCause::Invalid_Mandatory_Information)
        .build();
    ParsedMessage pm{SM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3ModifyPDPContextReject::MTI);
}

// 3GPP TS 24.008 9.5.9: SM Status
TEST(SMBuilders, SMStatus) {
    auto msg = L3SMStatus::builder()
        .cause(SMCause::ReqAccepted)
        .build();
    ParsedMessage pm{SM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3SMStatus::MTI);
}

// 3GPP TS 24.008 9.5.10: Request PDP Context Activation
TEST(SMBuilders, RequestPDPContextActivation) {
    auto msg = L3RequestPDPContextActivation::builder()
        .pdpHandle(2)
        .apn(L3AccessPointName("default"))
        .qos(L3QoS{})
        .build();
    ParsedMessage pm{SM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3RequestPDPContextActivation::MTI);
}

// 3GPP TS 24.008 9.5.10: Request PDP Context Activation Reject
TEST(SMBuilders, RequestPDPContextActivationReject) {
    auto msg = L3RequestPDPContextActivationReject::builder()
        .pdpHandle(2)
        .cause(SMCause::Invalid_Mandatory_Information)
        .build();
    ParsedMessage pm{SM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3RequestPDPContextActivationReject::MTI);
}

// 3GPP TS 24.008 9.5.6: Modify PDP Context Request (MS->Net)
TEST(SMBuilders, ModifyPDPContextRequestMS) {
    auto msg = L3ModifyPDPContextRequestMS::builder()
        .pdpHandle(1)
        .qos(L3QoS{})
        .build();
    ParsedMessage pm{SM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3ModifyPDPContextRequestMS::MTI);
}

// 3GPP TS 24.008 9.5.7: Modify PDP Context Accept (Net->MS)
TEST(SMBuilders, ModifyPDPContextAcceptNet) {
    auto msg = L3ModifyPDPContextAcceptNet::builder()
        .pdpHandle(1)
        .qos(L3QoS{})
        .build();
    ParsedMessage pm{SM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3ModifyPDPContextAcceptNet::MTI);
}

// 3GPP TS 24.008 9.5.11: Activate Secondary PDP Context Request
TEST(SMBuilders, ActivateSecondaryPDPContextRequest) {
    auto msg = L3ActivateSecondaryPDPContextRequest::builder()
        .pdpHandle(3)
        .apn(L3AccessPointName("secondary"))
        .qos(L3QoS{})
        .build();
    ParsedMessage pm{SM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3ActivateSecondaryPDPContextRequest::MTI);
}

// 3GPP TS 24.008 9.5.12: Activate Secondary PDP Context Accept
TEST(SMBuilders, ActivateSecondaryPDPContextAccept) {
    auto msg = L3ActivateSecondaryPDPContextAccept::builder()
        .pdpHandle(3)
        .qos(L3QoS{})
        .build();
    ParsedMessage pm{SM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3ActivateSecondaryPDPContextAccept::MTI);
}

// 3GPP TS 24.008 9.5.13: Activate Secondary PDP Context Reject
TEST(SMBuilders, ActivateSecondaryPDPContextReject) {
    auto msg = L3ActivateSecondaryPDPContextReject::builder()
        .pdpHandle(3)
        .cause(SMCause::Invalid_Mandatory_Information)
        .build();
    ParsedMessage pm{SM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3ActivateSecondaryPDPContextReject::MTI);
}

// 3GPP TS 24.008 9.5.14: Activate AA PDP Context Request
TEST(SMBuilders, ActivateAAPDPContextRequest) {
    auto msg = L3ActivateAAPDPContextRequest::builder()
        .pdpHandle(4)
        .apn(L3AccessPointName("aa"))
        .qos(L3QoS{})
        .build();
    ParsedMessage pm{SM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3ActivateAAPDPContextRequest::MTI);
}

// 3GPP TS 24.008 9.5.15: Activate AA PDP Context Accept
TEST(SMBuilders, ActivateAAPDPContextAccept) {
    auto msg = L3ActivateAAPDPContextAccept::builder()
        .pdpHandle(4)
        .qos(L3QoS{})
        .build();
    ParsedMessage pm{SM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3ActivateAAPDPContextAccept::MTI);
}

// 3GPP TS 24.008 9.5.16: Activate AA PDP Context Reject
TEST(SMBuilders, ActivateAAPDPContextReject) {
    auto msg = L3ActivateAAPDPContextReject::builder()
        .pdpHandle(4)
        .cause(SMCause::Invalid_Mandatory_Information)
        .build();
    ParsedMessage pm{SM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3ActivateAAPDPContextReject::MTI);
}

// 3GPP TS 24.008 9.5.17: Deactivate AA PDP Context Request
TEST(SMBuilders, DeactivateAAPDPContextRequest) {
    auto msg = L3DeactivateAAPDPContextRequest::builder()
        .pdpHandle(4)
        .build();
    ParsedMessage pm{SM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3DeactivateAAPDPContextRequest::MTI);
}

// 3GPP TS 24.008 9.5.17: Deactivate AA PDP Context Accept
TEST(SMBuilders, DeactivateAAPDPContextAccept) {
    auto msg = L3DeactivateAAPDPContextAccept::builder()
        .pdpHandle(4)
        .build();
    ParsedMessage pm{SM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3DeactivateAAPDPContextAccept::MTI);
}

// 3GPP TS 24.008 9.5.18: Activate MBMS Context Request
TEST(SMBuilders, ActivateMBMSContextRequest) {
    auto msg = L3ActivateMBMSContextRequest::builder()
        .tmgi(L3TMGI{})
        .qos(L3QoS{})
        .build();
    ParsedMessage pm{SM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3ActivateMBMSContextRequest::MTI);
}

// 3GPP TS 24.008 9.5.19: Activate MBMS Context Accept
TEST(SMBuilders, ActivateMBMSContextAccept) {
    auto msg = L3ActivateMBMSContextAccept::builder()
        .pdpHandle(5)
        .qos(L3QoS{})
        .build();
    ParsedMessage pm{SM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3ActivateMBMSContextAccept::MTI);
}

// 3GPP TS 24.008 9.5.20: Activate MBMS Context Reject
TEST(SMBuilders, ActivateMBMSContextReject) {
    auto msg = L3ActivateMBMSContextReject::builder()
        .cause(SMCause::Invalid_Mandatory_Information)
        .build();
    ParsedMessage pm{SM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3ActivateMBMSContextReject::MTI);
}

// 3GPP TS 24.008 9.5.21: Request MBMS Context Activation
TEST(SMBuilders, RequestMBMSContextActivation) {
    auto msg = L3RequestMBMSContextActivation::builder()
        .tmgi(L3TMGI{})
        .qos(L3QoS{})
        .build();
    ParsedMessage pm{SM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3RequestMBMSContextActivation::MTI);
}

// 3GPP TS 24.008 9.5.22: Request MBMS Context Activation Reject
TEST(SMBuilders, RequestMBMSContextActivationReject) {
    auto msg = L3RequestMBMSContextActivationReject::builder()
        .cause(SMCause::Invalid_Mandatory_Information)
        .build();
    ParsedMessage pm{SM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3RequestMBMSContextActivationReject::MTI);
}

// 3GPP TS 24.008 9.5.23: Request Secondary PDP Context Activation
TEST(SMBuilders, RequestSecondaryPDPContextActivation) {
    auto msg = L3RequestSecondaryPDPContextActivation::builder()
        .pdpHandle(6)
        .apn(L3AccessPointName("req-sec"))
        .qos(L3QoS{})
        .build();
    ParsedMessage pm{SM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3RequestSecondaryPDPContextActivation::MTI);
}

// 3GPP TS 24.008 9.5.24: Request Secondary PDP Context Activation Reject
TEST(SMBuilders, RequestSecondaryPDPContextActivationReject) {
    auto msg = L3RequestSecondaryPDPContextActivationReject::builder()
        .pdpHandle(6)
        .cause(SMCause::Invalid_Mandatory_Information)
        .build();
    ParsedMessage pm{SM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3RequestSecondaryPDPContextActivationReject::MTI);
}

// 3GPP TS 24.008 9.5.25: SM Notification
TEST(SMBuilders, SMNotification) {
    auto msg = L3SMNotification::builder()
        .pdpHandle(7)
        .build();
    ParsedMessage pm{SM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3SMNotification::MTI);
}
