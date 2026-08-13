// MM Builder tests: round-trip serialization for every MM message type.
// Reference: GSM 04.08 Chapter 9 (MM messages)

#include <gtest/gtest.h>
#include <gsml3parser/parser.h>
#include <gsml3parser/mm/l3mmmessages.h>
#include <gsml3parser/visitor.h>

using namespace gsml3parser;

static Expected<ParsedMessage> roundtrip(const ParsedMessage& msg) {
    auto bytes = writeL3Bytes(msg);
    if (!bytes) return Expected<ParsedMessage>::error(bytes.error());
    return parseL3(*bytes);
}

// GSM 04.08 9.2.5: CM Service Accept (empty body)
TEST(MMBuilders, CMServiceAccept) {
    auto msg = L3CMServiceAccept::builder().build();
    ParsedMessage pm{MMM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
    EXPECT_EQ((*bytes)[0], 0x50); // PD=5(MM)
    EXPECT_EQ((*bytes)[1], 0x84); // MTI=0x21<<2

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3CMServiceAccept::MTI);
}

// GSM 04.08 9.2.7: CM Service Abort (empty body)
TEST(MMBuilders, CMServiceAbort) {
    auto msg = L3CMServiceAbort::builder().build();
    ParsedMessage pm{MMM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
    EXPECT_EQ((*bytes)[0], 0x50); // PD=5(MM)

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3CMServiceAbort::MTI);
}

// GSM 04.08 9.2.1: Authentication Reject (empty body)
TEST(MMBuilders, AuthenticationReject) {
    auto msg = L3AuthenticationReject::builder().build();
    ParsedMessage pm{MMM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3AuthenticationReject::MTI);
}

// GSM 04.08 9.2.18: TMSI Reallocation Complete (empty body)
TEST(MMBuilders, TMSIReallocationComplete) {
    auto msg = L3TMSIReallocationComplete::builder().build();
    ParsedMessage pm{MMM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3TMSIReallocationComplete::MTI);
}

// GSM 04.08 9.2.15: IMSI Detach Indication
TEST(MMBuilders, IMSIDetachIndication) {
    auto msg = L3IMSIDetachIndication::builder()
        .classmark(L3MobileStationClassmark1{})
        .mobileIdentity(L3MobileIdentity(0x12345678))
        .build();
    ParsedMessage pm{MMM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3IMSIDetachIndication::MTI);
}

// GSM 04.08 9.2.14: Location Updating Reject
TEST(MMBuilders, LocationUpdatingReject) {
    auto msg = L3LocationUpdatingReject::builder()
        .cause(MMRejectCause::Congestion)
        .build();
    ParsedMessage pm{MMM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    auto* lur = tryGet<L3LocationUpdatingReject>(*reparsed);
    ASSERT_TRUE(lur);
}

// GSM 04.08 9.2.2: Authentication Request
TEST(MMBuilders, AuthenticationRequest) {
    std::vector<uint8_t> rand(16);
    for (int i = 0; i < 16; i++) rand[i] = static_cast<uint8_t>(i + 1);
    auto msg = L3AuthenticationRequest::builder()
        .cksn(5)
        .rand(std::move(rand))
        .build();
    ParsedMessage pm{MMM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
    EXPECT_EQ((*bytes)[0], 0x50); // PD=5(MM)
    EXPECT_EQ((*bytes)[1], 0x48); // MTI=0x12<<2

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3AuthenticationRequest::MTI);
}

// GSM 04.08 9.2.3: Authentication Response
TEST(MMBuilders, AuthenticationResponse) {
    auto msg = L3AuthenticationResponse::builder()
        .sres(0xABCD1234u)
        .build();
    ParsedMessage pm{MMM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    auto* ar = tryGet<L3AuthenticationResponse>(*reparsed);
    ASSERT_TRUE(ar);
    EXPECT_EQ(ar->sres(), 0xABCD1234u);
}

// GSM 04.08 9.2.6: CM Service Reject
TEST(MMBuilders, CMServiceReject_WithCause) {
    auto msg = L3CMServiceReject::builder()
        .cause(MMRejectCause::Network_Failure)
        .build();
    ParsedMessage pm{MMM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    auto* rej = tryGet<L3CMServiceReject>(*reparsed);
    ASSERT_TRUE(rej);
}

// GSM 04.08 9.2.15: MM Status
TEST(MMBuilders, MMStatus) {
    auto msg = L3MMStatus::builder()
        .cause(MMRejectCause::Illegal_MS)
        .build();
    ParsedMessage pm{MMM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    auto* ms = tryGet<L3MMStatus>(*reparsed);
    ASSERT_TRUE(ms);
    EXPECT_EQ(ms->cause(), MMRejectCause::Illegal_MS);
}

// GSM 04.08 9.2.10: Identity Request
TEST(MMBuilders, IdentityRequest_IMSI) {
    auto msg = L3IdentityRequest::builder()
        .type(MobileIDType::IMSI)
        .build();
    ParsedMessage pm{MMM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3IdentityRequest::MTI);
}

// GSM 04.08 9.2.11: Identity Response
TEST(MMBuilders, IdentityResponse) {
    auto msg = L3IdentityResponse::builder()
        .mobileId(L3MobileIdentity(0xDEADBEEF))
        .build();
    ParsedMessage pm{MMM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    auto* ir = tryGet<L3IdentityResponse>(*reparsed);
    ASSERT_TRUE(ir);
}

// GSM 04.08 9.2.15: Location Updating Request (most important MM for BTS)
TEST(MMBuilders, LocationUpdatingRequest_Full) {
    auto msg = L3LocationUpdatingRequest::builder()
        .updateType(0) // Normal
        .cksn(7)
        .classmark(L3MobileStationClassmark1{})
        .mobileIdentity(L3MobileIdentity(0x12345678))
        .lai(L3LocationAreaIdentity("250", "01", 0x5678))
        .build();
    ParsedMessage pm{MMM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
    EXPECT_EQ((*bytes)[0], 0x50); // PD=5(MM)
    EXPECT_EQ((*bytes)[1], 0x20); // MTI=0x08<<2

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    auto* lur = tryGet<L3LocationUpdatingRequest>(*reparsed);
    ASSERT_TRUE(lur);
    EXPECT_EQ(lur->getLocationUpdatingType(), LocationUpdateType::Normal);
}

// GSM 04.08 9.2.15a: MM Information
TEST(MMBuilders, MMInformation) {
    auto msg = L3MMInformation::builder()
        .time(L3TimeZoneAndTime{L3TimeZoneAndTime::UTC_TIME})
        .build();
    ParsedMessage pm{MMM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3MMInformation::MTI);
}

// GSM 04.08 9.2.9: CM Service Request
TEST(MMBuilders, CMServiceRequest) {
    auto msg = L3CMServiceRequest::builder()
        .classmark(L3MobileStationClassmark2{})
        .mobileIdentity(L3MobileIdentity(0x12345678))
        .serviceType(L3CMServiceType{L3CMServiceType::MobileOriginatedCall})
        .build();
    ParsedMessage pm{MMM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3CMServiceRequest::MTI);
}

// TS 24.008 9.2.8: CM Request
TEST(MMBuilders, CMRequest) {
    auto msg = L3CMRequest::builder()
        .cksn(5)
        .classmark(L3MobileStationClassmark2{})
        .mobileIdentity(L3MobileIdentity(0x12345678))
        .build();
    ParsedMessage pm{MMM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3CMRequest::MTI);
}

// TS 24.008 9.2.12: MM Paging
TEST(MMBuilders, PagingMM) {
    auto msg = L3PagingMM::builder()
        .mobileIdentity(L3MobileIdentity(0xABCDEF01))
        .build();
    ParsedMessage pm{MMM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    auto* pg = tryGet<L3PagingMM>(*reparsed);
    ASSERT_TRUE(pg);
}

// GSM 04.08 9.2.4: CM Reestablishment Request
TEST(MMBuilders, CMReestablishmentRequest) {
    auto msg = L3CMReestablishmentRequest::builder()
        .cksn(3)
        .classmark(L3MobileStationClassmark2{})
        .mobileId(L3MobileIdentity(0x98765432))
        .lai(L3LocationAreaIdentity("250", "01", 0xABCD))
        .build();
    ParsedMessage pm{MMM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    auto* cmr = tryGet<L3CMReestablishmentRequest>(*reparsed);
    ASSERT_TRUE(cmr);
    EXPECT_EQ(cmr->cksn(), 3u);
}

// Existing Builder: L3LocationUpdatingAccept (already had Builder)
TEST(MMBuilders, LocationUpdatingAccept_ExistingBuilder) {
    L3LocationAreaIdentity lai("250", "01", 0x1234);
    auto msg = L3LocationUpdatingAccept::builder().lai(lai).build();
    ParsedMessage pm{MMM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3LocationUpdatingAccept::MTI);
}

// Existing Builder: L3TMSIReallocationCommand (already had Builder)
TEST(MMBuilders, TMSIReallocationCommand_ExistingBuilder) {
    L3LocationAreaIdentity lai("250", "01", 0x1234);
    auto msg = L3TMSIReallocationCommand::builder()
        .lai(lai)
        .tmsi(L3MobileIdentity(0x12345678))
        .build();
    ParsedMessage pm{MMM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    auto* trc = tryGet<L3TMSIReallocationCommand>(*reparsed);
    ASSERT_TRUE(trc);
}
