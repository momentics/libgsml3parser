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

// CC Builder round-trip tests: build via Builder, serialize, parse, verify fields.
// Reference: GSM 04.08 Section 9.3 (CC message definitions).

#include <gtest/gtest.h>
#include <gsml3parser/parser.h>
#include <gsml3parser/cc/l3ccmessages.h>
#include <gsml3parser/common/l3common.h>
#include <gsml3parser/visitor.h>

using namespace gsml3parser;

static Expected<ParsedMessage> roundtrip(const ParsedMessage& msg) {
    auto bytes = writeL3Bytes(msg);
    if (!bytes) return Expected<ParsedMessage>::error(bytes.error());
    return parseL3(*bytes);
}

// GSM 04.08 9.3.19: Setup with all optional fields
TEST(CCBuilders, Setup_FullFields) {
    auto msg = L3Setup::builder()
        .calledParty(L3CalledPartyBCDNumber("1234567890"))
        .callingParty(L3CallingPartyBCDNumber("0987654321"))
        .signal(L3Signal(L3Signal::SignalRingBackToneOn))
        .build();
    ParsedMessage pm{CCM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
    EXPECT_EQ((*bytes)[0], 0x3E); // PD=CC, TI=7
    EXPECT_EQ((*bytes)[1], 0x14); // MTI=Setup(0x05)<<2

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    auto* s = tryGet<L3Setup>(*reparsed);
    ASSERT_TRUE(s);
    EXPECT_TRUE(s->haveCalledParty());
    EXPECT_STREQ(s->digits(), "1234567890");
    EXPECT_TRUE(s->haveCallingParty());
    EXPECT_TRUE(s->haveSignal());
}

// GSM 04.08 9.3.19: Setup minimal (no optional fields)
TEST(CCBuilders, Setup_Minimal) {
    auto msg = L3Setup::builder().build();
    ParsedMessage pm{CCM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
    EXPECT_EQ((*bytes)[0], 0x3E);
    EXPECT_EQ((*bytes)[1], 0x14);
}

// GSM 04.08 9.3.8: Emergency Setup
TEST(CCBuilders, EmergencySetup) {
    auto msg = L3EmergencySetup::builder().build();
    ParsedMessage pm{CCM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
    EXPECT_EQ((*bytes)[1], 0x38); // MTI=0x0e<<2

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3EmergencySetup::MTI);
}

// GSM 04.08 9.3.3: Call Proceeding with progress
TEST(CCBuilders, CallProceeding_WithProgress) {
    auto msg = L3CallProceeding::builder()
        .progress(L3ProgressIndicator(L3ProgressIndicator::InBandAvailable,
                                       L3ProgressIndicator::PrivateServingLocal))
        .build();
    ParsedMessage pm{CCM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
    EXPECT_EQ((*bytes)[1], 0x08); // MTI=0x02<<2

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    auto* cp = tryGet<L3CallProceeding>(*reparsed);
    ASSERT_TRUE(cp);
    EXPECT_TRUE(cp->hasProgress());
}

// GSM 04.08 9.3.1: Alerting with progress and user-user
TEST(CCBuilders, Alerting_WithFields) {
    auto msg = L3Alerting::builder()
        .progress(L3ProgressIndicator(L3ProgressIndicator::EndToEndISDN,
                                       L3ProgressIndicator::PrivateServingLocal))
        .build();
    ParsedMessage pm{CCM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
    EXPECT_EQ((*bytes)[1], 0x04); // MTI=0x01<<2

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    auto* al = tryGet<L3Alerting>(*reparsed);
    ASSERT_TRUE(al);
    EXPECT_TRUE(al->hasProgress());
}

// GSM 04.08 9.3.5: Connect with connected number
TEST(CCBuilders, Connect_WithConnectedNumber) {
    auto msg = L3Connect::builder()
        .connectedNumber(L3ConnectedNumber("1234567890"))
        .build();
    ParsedMessage pm{CCM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
    EXPECT_EQ((*bytes)[1], 0x1C); // MTI=0x07<<2

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    auto* c = tryGet<L3Connect>(*reparsed);
    ASSERT_TRUE(c);
    EXPECT_TRUE(c->haveConnectedNumber());
}

// GSM 04.08 9.3.6: Connect Acknowledge
TEST(CCBuilders, ConnectAcknowledge) {
    auto msg = L3ConnectAcknowledge::builder().build();
    ParsedMessage pm{CCM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
    EXPECT_EQ((*bytes)[1], 0x3C); // MTI=0x0f<<2

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3ConnectAcknowledge::MTI);
}

// GSM 04.08 9.3.2: Call Confirmed with cause
TEST(CCBuilders, CallConfirmed_WithCause) {
    auto msg = L3CallConfirmed::builder()
        .cause(L3CauseElement(CCCause::Normal_Call_Clearing, CCCauseLocation::Private_Serving_Local))
        .build();
    ParsedMessage pm{CCM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
    EXPECT_EQ((*bytes)[1], 0x20); // MTI=0x08<<2

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    auto* cc = tryGet<L3CallConfirmed>(*reparsed);
    ASSERT_TRUE(cc);
    EXPECT_TRUE(cc->hasCause());
}

// GSM 04.08 9.3.7: Disconnect with cause and location
TEST(CCBuilders, Disconnect_UserBusy) {
    auto msg = L3Disconnect::builder()
        .cause(CCCause::User_Busy)
        .location(CCCauseLocation::Transit)
        .build();
    ParsedMessage pm{CCM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
    EXPECT_EQ((*bytes)[1], 0x94); // MTI=0x25<<2

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    auto* d = tryGet<L3Disconnect>(*reparsed);
    ASSERT_TRUE(d);
    EXPECT_EQ(d->cause(), CCCause::User_Busy);
}

// GSM 04.08 9.3.19: Release with cause, facility, ssVersion
TEST(CCBuilders, Release_WithAllFields) {
    auto msg = L3Release::builder()
        .cause(CCCause::Normal_Call_Clearing)
        .build();
    ParsedMessage pm{CCM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
    EXPECT_EQ((*bytes)[1], 0xB4); // MTI=0x2D<<2

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    auto* r = tryGet<L3Release>(*reparsed);
    ASSERT_TRUE(r);
    EXPECT_TRUE(r->haveCause());
    EXPECT_EQ(r->cause(), CCCause::Normal_Call_Clearing);
}

// GSM 04.08 9.3.19: Release Complete with cause
TEST(CCBuilders, ReleaseComplete_WithCause) {
    auto msg = L3ReleaseComplete::builder()
        .ti(5)
        .cause(CCCause::Normal_Call_Clearing)
        .build();
    ParsedMessage pm{CCM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    auto* rc = tryGet<L3ReleaseComplete>(*reparsed);
    ASSERT_TRUE(rc);
    EXPECT_TRUE(rc->haveCause());
}

// GSM 04.08 9.3.19: CC Status
TEST(CCBuilders, CCStatus) {
    auto msg = L3CCStatus::builder()
        .cause(CCCause::Normal_Unspecified)
        .callState(0x00)
        .build();
    ParsedMessage pm{CCM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3CCStatus::MTI);
}

// GSM 04.08 9.3.24: Start DTMF
TEST(CCBuilders, StartDTMF_KeyA) {
    auto msg = L3StartDTMF::builder()
        .key('A')
        .build();
    ParsedMessage pm{CCM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    auto* sdtmf = tryGet<L3StartDTMF>(*reparsed);
    ASSERT_TRUE(sdtmf);
    EXPECT_EQ(sdtmf->key(), 'A');
}

// GSM 04.08 9.3.29: Stop DTMF
TEST(CCBuilders, StopDTMF) {
    auto msg = L3StopDTMF::builder().build();
    ParsedMessage pm{CCM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3StopDTMF::MTI);
}

// GSM 04.08 9.3.30: Stop DTMF Acknowledge
TEST(CCBuilders, StopDTMFAcknowledge) {
    auto msg = L3StopDTMFAcknowledge::builder().build();
    ParsedMessage pm{CCM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3StopDTMFAcknowledge::MTI);
}

// GSM 04.08 9.3.25: Start DTMF Acknowledge
TEST(CCBuilders, StartDTMFAcknowledge_Key5) {
    auto msg = L3StartDTMFAcknowledge::builder()
        .key('5')
        .build();
    ParsedMessage pm{CCM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    auto* ack = tryGet<L3StartDTMFAcknowledge>(*reparsed);
    ASSERT_TRUE(ack);
    EXPECT_EQ(ack->key(), '5');
}

// GSM 04.08 9.3.26: Start DTMF Reject
TEST(CCBuilders, StartDTMFReject) {
    auto msg = L3StartDTMFReject::builder()
        .cause(CCCause::Normal_Unspecified)
        .build();
    ParsedMessage pm{CCM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    auto* rej = tryGet<L3StartDTMFReject>(*reparsed);
    ASSERT_TRUE(rej);
    EXPECT_EQ(rej->cause(), CCCause::Normal_Unspecified);
}

// GSM 04.08 9.3.10: Hold
TEST(CCBuilders, Hold) {
    auto msg = L3Hold::builder().build();
    ParsedMessage pm{CCM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
    EXPECT_EQ((*bytes)[1], 0x60); // MTI=0x18<<2

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3Hold::MTI);
}

// GSM 04.08 9.3.12: Hold Reject
TEST(CCBuilders, HoldReject) {
    auto msg = L3HoldReject::builder()
        .cause(CCCause::Normal_Unspecified)
        .build();
    ParsedMessage pm{CCM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    auto* hr = tryGet<L3HoldReject>(*reparsed);
    ASSERT_TRUE(hr);
    EXPECT_EQ(hr->cause(), CCCause::Normal_Unspecified);
}

// GSM 04.08 9.3.17: Progress
TEST(CCBuilders, Progress) {
    auto msg = L3Progress::builder()
        .progress(L3ProgressIndicator(L3ProgressIndicator::InBandAvailable,
                                       L3ProgressIndicator::PrivateServingLocal))
        .build();
    ParsedMessage pm{CCM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3Progress::MTI);
}

// TS 24.008 9.3.21: Facility with body data
TEST(CCBuilders, Facility_WithBody) {
    auto msg = L3Facility::builder()
        .facilityBody(std::vector<uint8_t>{0x01, 0x02, 0x03})
        .build();
    ParsedMessage pm{CCM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    auto* fac = tryGet<L3Facility>(*reparsed);
    ASSERT_TRUE(fac);
    EXPECT_EQ(fac->facilityBody().size(), 3u);
}

// TS 24.008 9.3.15: Modify with bearer capability and called party
TEST(CCBuilders, Modify_WithFields) {
    auto msg = L3Modify::builder()
        .calledParty(L3CalledPartyBCDNumber("9876543210"))
        .build();
    ParsedMessage pm{CCM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    auto* mod = tryGet<L3Modify>(*reparsed);
    ASSERT_TRUE(mod);
    EXPECT_TRUE(mod->haveCalledParty());
}

// TS 24.008 9.3.16: Unit Data with user data
TEST(CCBuilders, UnitData_WithData) {
    auto msg = L3UnitData::builder()
        .userData(std::vector<uint8_t>{0xDE, 0xAD, 0xBE, 0xEF})
        .build();
    ParsedMessage pm{CCM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    auto* ud = tryGet<L3UnitData>(*reparsed);
    ASSERT_TRUE(ud);
    EXPECT_EQ(ud->userData().size(), 4u);
}

// TS 24.008 9.3.16a: Unit Data Acknowledge
TEST(CCBuilders, UnitDataAck) {
    auto msg = L3UnitDataAck::builder().build();
    ParsedMessage pm{CCM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messageMTI(*reparsed), L3UnitDataAck::MTI);
}

// TS 24.008 9.3.16b: Error Indication
TEST(CCBuilders, ErrorIndication) {
    auto msg = L3ErrorIndication::builder()
        .cause(CCCause::Invalid_Mandatory_Information)
        .build();
    ParsedMessage pm{CCM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = roundtrip(pm);
    ASSERT_TRUE(reparsed);
    auto* ei = tryGet<L3ErrorIndication>(*reparsed);
    ASSERT_TRUE(ei);
    EXPECT_EQ(ei->cause(), CCCause::Invalid_Mandatory_Information);
}

// Verify all 25 CC message types have builder() method
TEST(CCBuilders, AllTypesHaveBuilder) {
    // Each call verifies the builder compiles and returns a valid Builder.
    (void)L3Setup::builder();
    (void)L3EmergencySetup::builder();
    (void)L3CallProceeding::builder();
    (void)L3Alerting::builder();
    (void)L3Connect::builder();
    (void)L3ConnectAcknowledge::builder();
    (void)L3CallConfirmed::builder();
    (void)L3Disconnect::builder();
    (void)L3Release::builder();
    (void)L3ReleaseComplete::builder();
    (void)L3CCStatus::builder();
    (void)L3StartDTMF::builder();
    (void)L3StopDTMF::builder();
    (void)L3StopDTMFAcknowledge::builder();
    (void)L3StartDTMFAcknowledge::builder();
    (void)L3StartDTMFReject::builder();
    (void)L3Hold::builder();
    (void)L3HoldReject::builder();
    (void)L3Progress::builder();
    (void)L3Facility::builder();
    (void)L3Modify::builder();
    (void)L3UnitData::builder();
    (void)L3UnitDataAck::builder();
    (void)L3ErrorIndication::builder();
}

// TI round-trip for various CC messages
TEST(CCBuilders, TI_RoundTrip_AllTypes) {
    for (unsigned ti = 0; ti < 8; ++ti) {
        // Test with a few representative message types
        {
            auto msg = L3Disconnect::builder().ti(ti).build();
            ParsedMessage pm{CCM{std::move(msg)}};
            auto reparsed = roundtrip(pm);
            ASSERT_TRUE(reparsed);
            auto* d = tryGet<L3Disconnect>(*reparsed);
            ASSERT_TRUE(d);
            EXPECT_EQ(d->ti(), ti);
        }
        {
            auto msg = L3CallProceeding::builder().ti(ti).build();
            ParsedMessage pm{CCM{std::move(msg)}};
            auto reparsed = roundtrip(pm);
            ASSERT_TRUE(reparsed);
            auto* cp = tryGet<L3CallProceeding>(*reparsed);
            ASSERT_TRUE(cp);
            EXPECT_EQ(cp->ti(), ti);
        }
    }
}
