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

// GMM Builder tests: round-trip serialization for every GMM message type.
// Reference: 3GPP TS 24.008 Chapter 9.4 (GMM messages)

#include <gtest/gtest.h>
#include <gsml3parser/parser.h>
#include <gsml3parser/gmm/l3gmmmessages.h>
#include <gsml3parser/visitor.h>

using namespace gsml3parser;

static Expected<ParsedMessage> roundtrip(const ParsedMessage& msg) {
    auto bytes = writeL3Bytes(msg);
    if (!bytes) return Expected<ParsedMessage>::error(bytes.error());
    return parseL3(*bytes);
}

// 3GPP TS 24.008 9.4.3: Attach Complete (empty body)
TEST(GMMBuilders, AttachComplete) {
    auto msg = L3AttachComplete::builder().build();
    ParsedMessage pm{GMM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
    EXPECT_EQ((*bytes)[0], 0x08); // PD=GMM in the low nibble

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3AttachComplete::MTI);
}

// 3GPP TS 24.008 9.4.1: Attach Request
TEST(GMMBuilders, AttachRequest) {
    auto msg = L3AttachRequest::builder()
        .attachType(GMMAttachType::GPRSAttach)
        .cksn(5)
        .forL3(true)
        .drxParam(L3DRXParameter{})
        .mobileIdentity(L3MobileIdentity(0x12345678))
        .oldRAI(L3RoutingAreaIdentification{})
        .build();
    ParsedMessage pm{GMM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
    EXPECT_EQ((*bytes)[0], 0x08); // PD=GMM in the low nibble
}

// 3GPP TS 24.008 9.4.2: Attach Accept
TEST(GMMBuilders, AttachAccept) {
    auto msg = L3AttachAccept::builder()
        .attachResult(GMMAttachType::GPRSAttach)
        .forceToStandby(false)
        .updateTimer(10)
        .radioPriority(0)
        .leadingExtra(0x00, 0x2B)
        .rai(L3RoutingAreaIdentification{})
        .ptmsiSignature({0x11, 0x22, 0x33})
        .build();
    ParsedMessage pm{GMM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    // The two further leading octets and the P-TMSI signature (TV, IEI
    // 0x19) are part of the body between the first octet and the routing
    // area identification.
    EXPECT_EQ((*bytes)[3], 0x00);
    EXPECT_EQ((*bytes)[4], 0x2B);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3AttachAccept::MTI);
    auto* acc = tryGet<L3AttachAccept>(*reparsed);
    ASSERT_NE(acc, nullptr);
    const auto extra = acc->leadingExtra();
    EXPECT_EQ(extra[0], 0x00);
    EXPECT_EQ(extra[1], 0x2B);
    EXPECT_EQ(acc->hasPtmsiSignature(), true);
    const auto& sig = acc->ptmsiSignature();
    EXPECT_EQ(sig[0], 0x11);
    EXPECT_EQ(sig[1], 0x22);
    EXPECT_EQ(sig[2], 0x33);
}

// 3GPP TS 24.008 9.4.4: Attach Reject
TEST(GMMBuilders, AttachReject) {
    auto msg = L3AttachReject::builder()
        .cause(GMMCause::GprsNotAllowed)
        .build();
    ParsedMessage pm{GMM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3AttachReject::MTI);
}

// 3GPP TS 24.008 9.4.5: Detach Request
TEST(GMMBuilders, DetachRequest) {
    auto msg = L3DetachRequest::builder()
        .detachType(1)
        .powerOff(false)
        .forceToStandby(false)
        .cause(GMMCause::ReqAccepted)
        .build();
    ParsedMessage pm{GMM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3DetachRequest::MTI);
}

// 3GPP TS 24.008 9.4.6: Detach Accept
TEST(GMMBuilders, DetachAccept) {
    auto msg = L3DetachAccept::builder()
        .forceToStandby(false)
        .build();
    ParsedMessage pm{GMM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3DetachAccept::MTI);
}

// 3GPP TS 24.008 9.4.12: Routing Area Update Request
TEST(GMMBuilders, RoutingAreaUpdateRequest) {
    auto msg = L3RoutingAreaUpdateRequest::builder()
        .updateType(GMMUpdateType::RAUpdated)
        .cksn(3)
        .forL3(false)
        .oldRAI(L3RoutingAreaIdentification{})
        .build();
    ParsedMessage pm{GMM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3RoutingAreaUpdateRequest::MTI);
}

// 3GPP TS 24.008 9.4.15: Routing Area Update Accept
TEST(GMMBuilders, RoutingAreaUpdateAccept) {
    auto msg = L3RoutingAreaUpdateAccept::builder()
        .forceToStandby(false)
        .updateResult(GMMUpdateType::RAUpdated)
        .raUpdateTimer(15)
        .leadingExtra(0x00)
        .rai(L3RoutingAreaIdentification{})
        .build();
    ParsedMessage pm{GMM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3RoutingAreaUpdateAccept::MTI);
}

// 3GPP TS 24.008 9.4.16: Routing Area Update Complete (empty body)
TEST(GMMBuilders, RoutingAreaUpdateComplete) {
    auto msg = L3RoutingAreaUpdateComplete::builder().build();
    ParsedMessage pm{GMM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3RoutingAreaUpdateComplete::MTI);
}

// 3GPP TS 24.008 9.4.17: Routing Area Update Reject
TEST(GMMBuilders, RoutingAreaUpdateReject) {
    auto msg = L3RoutingAreaUpdateReject::builder()
        .cause(GMMCause::GprsNotAllowed)
        .build();
    ParsedMessage pm{GMM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3RoutingAreaUpdateReject::MTI);
}

// 3GPP TS 24.008 9.4.20: Service Request
TEST(GMMBuilders, ServiceRequest) {
    auto msg = L3ServiceRequest::builder()
        .cksn(7)
        .serviceType(0)
        .ptmsi(L3MobileIdentity(0x12345678))
        .build();
    ParsedMessage pm{GMM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    // First-octet layout (TS 44.068 section 9.5): the CKSN occupies the
    // low three bits of the octet value and the service type the high
    // half-octet, so cksn=7/serviceType=0 emit octet 0x07.
    const uint8_t expected[] = {
        0x08, 0x0c,                 // header: PD=GMM, MTI=ServiceRequest
        0x07,                       // CKSN(3)=7|spare(1)=0 (low)|serviceType(3)=0|spare(1)=0 (high)
        0x05,                       // PTMSI LV length = 5 bytes
        0xF4,                       // first octet: spare 'F'(4)|0(1)|type(3)=TMSI('100'B)
        0x12, 0x34, 0x56, 0x78      // TMSI value = 0x12345678
    };
    ASSERT_EQ(bytes.value().size(), sizeof(expected));
    for (size_t i = 0; i < sizeof(expected); ++i) {
        EXPECT_EQ((*bytes)[i], expected[i]);
    }

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3ServiceRequest::MTI);
}

// 3GPP TS 24.008 9.4.21: Service Accept (empty body)
TEST(GMMBuilders, ServiceAccept) {
    auto msg = L3ServiceAccept::builder().build();
    ParsedMessage pm{GMM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3ServiceAccept::MTI);
}

// 3GPP TS 24.008 9.4.22: Service Reject
TEST(GMMBuilders, ServiceReject) {
    auto msg = L3ServiceReject::builder()
        .cause(GMMCause::GprsNotAllowed)
        .build();
    ParsedMessage pm{GMM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3ServiceReject::MTI);
}

// 3GPP TS 24.008 9.4.8: P-TMSI Reallocation Command
TEST(GMMBuilders, P_TMSIReallocationCommand) {
    auto msg = L3P_TMSIReallocationCommand::builder()
        .ptmsiType(GMMPTMSIType::Native)
        .forceToStandby(false)
        .rai(L3RoutingAreaIdentification{})
        .build();
    ParsedMessage pm{GMM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3P_TMSIReallocationCommand::MTI);
}

// 3GPP TS 24.008 9.4.8: P-TMSI Reallocation Complete (empty body)
TEST(GMMBuilders, P_TMSIReallocationComplete) {
    auto msg = L3P_TMSIReallocationComplete::builder().build();
    ParsedMessage pm{GMM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3P_TMSIReallocationComplete::MTI);
}

// 3GPP TS 24.008 9.4.9: Authentication And Ciphering Request
TEST(GMMBuilders, AuthenticationAndCipheringRequest) {
    auto msg = L3AuthenticationAndCipheringRequest::builder()
        .cipheringAlgorithm(1)
        .imeisvRequest(false)
        .forceToStandby(false)
        .acReferenceNumber(5)
        .rand(L3AuthRAND{})
        .build();
    ParsedMessage pm{GMM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3AuthenticationAndCipheringRequest::MTI);
}

// 3GPP TS 24.008 9.4.9: Authentication And Ciphering Response
TEST(GMMBuilders, AuthenticationAndCipheringResponse) {
    auto msg = L3AuthenticationAndCipheringResponse::builder()
        .acReferenceNumber(5)
        .res(L3AuthRES{})
        .build();
    ParsedMessage pm{GMM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3AuthenticationAndCipheringResponse::MTI);
}

// 3GPP TS 24.008 9.4.9: Authentication And Ciphering Reject (empty body)
TEST(GMMBuilders, AuthenticationAndCipheringReject) {
    auto msg = L3AuthenticationAndCipheringReject::builder().build();
    ParsedMessage pm{GMM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3AuthenticationAndCipheringReject::MTI);
}

// 3GPP TS 24.008 9.4.7: GMM Identity Request
TEST(GMMBuilders, GMMIdentityRequest) {
    auto msg = L3GMMIdentityRequest::builder()
        .identityType(MobileIDType::TMSI)
        .build();
    ParsedMessage pm{GMM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    // One-octet body (TS 44.068 section 9.5): the identity type in the low
    // three bits and spare(5), so TMSI ('100'B) emits octet 0x04.
    const uint8_t expected[] = {0x08, 0x15, 0x04};
    ASSERT_EQ(bytes.value().size(), sizeof(expected));
    for (size_t i = 0; i < sizeof(expected); ++i) {
        EXPECT_EQ((*bytes)[i], expected[i]);
    }

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3GMMIdentityRequest::MTI);
}

// 3GPP TS 24.008 9.4.10: GMM Identity Response
TEST(GMMBuilders, GMMIdentityResponse) {
    auto msg = L3GMMIdentityResponse::builder()
        .mobileIdentity(L3MobileIdentity(0x98765432))
        .build();
    ParsedMessage pm{GMM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3GMMIdentityResponse::MTI);
}

// 3GPP TS 24.008 9.4.23: Authentication And Ciphering Failure
TEST(GMMBuilders, AuthenticationAndCipheringFailure) {
    auto msg = L3AuthenticationAndCipheringFailure::builder()
        .cause(GMMCause::Synch_Failure)
        .authFailureParam(L3AuthFailureParam{})
        .build();
    ParsedMessage pm{GMM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3AuthenticationAndCipheringFailure::MTI);
}

// 3GPP TS 24.008 9.4.24: GMM Status
TEST(GMMBuilders, GMMStatus) {
    auto msg = L3GMMStatus::builder()
        .cause(GMMCause::ReqAccepted)
        .build();
    ParsedMessage pm{GMM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3GMMStatus::MTI);
}

// 3GPP TS 24.008: GMM Information (empty body)
TEST(GMMBuilders, GMMInformation) {
    auto msg = L3GMMInformation::builder().build();
    ParsedMessage pm{GMM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3GMMInformation::MTI);
}
