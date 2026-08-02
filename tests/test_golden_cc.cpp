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

// Comprehensive GSM Layer 3 Golden Tests (Part 3: CC).
// Reference: osmo-ttcn3-hacks L3_Templates.ttcn (CC section).
// Spec: 3GPP TS 24.008 sections 9.3, 10.5.4.

#include <gtest/gtest.h>
#include <gsml3parser/parser.h>
#include <gsml3parser/cc/l3ccmessages.h>
#include <gsml3parser/common/l3common.h>

using namespace gsml3parser;

static std::unique_ptr<L3Message> roundtrip(const L3Message& msg) {
    std::vector<uint8_t> buf(msg.fullLength());
    size_t n = writeL3(msg, buf.data(), buf.size());
    if (n == 0) return nullptr;
    return parseL3(buf.data(), n);
}

// =====================================================================
// CC MESSAGE TYPE VALUES (GSM 04.08 Table 10.5.4)
// Reference: L3_Templates.ttcn CC message type constants
// =====================================================================

TEST(GoldenCC, MessageTypeValues) {
    EXPECT_EQ(L3CCMessage::Alerting, 0x01);
    EXPECT_EQ(L3CCMessage::CallProceeding, 0x02);
    EXPECT_EQ(L3CCMessage::Progress, 0x03);
    EXPECT_EQ(L3CCMessage::Setup, 0x05);
    EXPECT_EQ(L3CCMessage::Connect, 0x07);
    EXPECT_EQ(L3CCMessage::CallConfirmed, 0x08);
    EXPECT_EQ(L3CCMessage::EmergencySetup, 0x0e);
    EXPECT_EQ(L3CCMessage::ConnectAcknowledge, 0x0f);
    EXPECT_EQ(L3CCMessage::Hold, 0x18);
    EXPECT_EQ(L3CCMessage::HoldReject, 0x1a);
    EXPECT_EQ(L3CCMessage::Disconnect, 0x25);
    EXPECT_EQ(L3CCMessage::Release, 0x2d);
    EXPECT_EQ(L3CCMessage::ReleaseComplete, 0x2a);
    EXPECT_EQ(L3CCMessage::StopDTMF, 0x31);
    EXPECT_EQ(L3CCMessage::StopDTMFAcknowledge, 0x32);
    EXPECT_EQ(L3CCMessage::StartDTMF, 0x35);
    EXPECT_EQ(L3CCMessage::StartDTMFAcknowledge, 0x36);
    EXPECT_EQ(L3CCMessage::StartDTMFReject, 0x37);
    EXPECT_EQ(L3CCMessage::CCStatus, 0x3d);
}

// =====================================================================
// CC PARSE FROM HEX: Call Proceeding (GSM 04.08 9.3.3)
// Reference: L3_Templates.ttcn tr_ML3_MT_CC_CALL_PROC
// =====================================================================

TEST(GoldenCC, CallProceeding_Parse) {
    // Byte 0: PD(4)=3|TI(3)=7+TIF(1)=0 = 0x3E
    // Byte 1: messageType(6)=0x02|NSD(2)=0 = 0x08
    uint8_t data[] = {0x3E, 0x08};
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->MTI(), L3CCMessage::CallProceeding);
}

// =====================================================================
// CC PARSE FROM HEX: Connect (GSM 04.08 9.3.5)
// =====================================================================

TEST(GoldenCC, Connect_Parse) {
    // Byte 0: PD(4)=3|TI(3)=7+TIF(1)=0 = 0x3E
    // Byte 1: messageType(6)=0x07|NSD(2)=0 = 0x1C
    uint8_t data[] = {0x3E, 0x1C};
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->MTI(), L3CCMessage::Connect);
}

// =====================================================================
// CC PARSE FROM HEX: Connect Acknowledge (GSM 04.08 9.3.6)
// =====================================================================

TEST(GoldenCC, ConnectAcknowledge_Parse) {
    // Byte 0: PD(4)=3|TI(3)=7+TIF(1)=0 = 0x3E
    // Byte 1: messageType(6)=0x0f|NSD(2)=0 = 0x3C
    uint8_t data[] = {0x3E, 0x3C};
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->MTI(), L3CCMessage::ConnectAcknowledge);
}

// =====================================================================
// CC PARSE FROM HEX: Call Confirmed (GSM 04.08 9.3.2)
// Reference: L3_Templates.ttcn ts_ML3_MO_CC_CALL_CONF
// =====================================================================

TEST(GoldenCC, CallConfirmed_Parse) {
    // Byte 0: PD(4)=3|TI(3)=7+TIF(1)=0 = 0x3E
    // Byte 1: messageType(6)=0x08|NSD(2)=0 = 0x20
    uint8_t data[] = {0x3E, 0x20};
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->MTI(), L3CCMessage::CallConfirmed);
}

// =====================================================================
// CC PARSE FROM HEX: CC Status (GSM 04.08 9.3.19)
// Structure: Cause TLV, CallState(8)
// =====================================================================

TEST(GoldenCC, CCStatus_Parse) {
    // Byte 0: PD(4)=3|TI(3)=7+TIF(1)=0 = 0x3E
    // Byte 1: messageType(6)=0x3d|NSD(2)=0 = 0x3d<<2 = 0xEC
    // Byte 2: IEI = 0x08 (Cause)
    // Byte 3: Length = 2
    // Byte 4: location(4)=1, spare(1)=0, codingStd(2)=11, ext(1)=0 = 0x16
    // Byte 5: causeValue(7)=16(Normal_Call_Clearing), ext(1)=1 = 0x21
    // Byte 6: CallState = 0x00
    uint8_t data[] = {0x3E, 0xEC, 0x08, 0x02, 0x16, 0x21, 0x00};
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->MTI(), L3CCMessage::CCStatus);
}

// =====================================================================
// CC PARSE FROM HEX: Emergency Setup (GSM 04.08 9.3.8)
// =====================================================================

TEST(GoldenCC, EmergencySetup_Parse) {
    // Byte 0: PD(4)=3|TI(3)=7+TIF(1)=0 = 0x3E
    // Byte 1: messageType(6)=0x0e|NSD(2)=0 = 0x38
    uint8_t data[] = {0x3E, 0x38};
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->MTI(), L3CCMessage::EmergencySetup);
}

// =====================================================================
// CC PARSE FROM HEX: Hold (GSM 04.08 9.3.10)
// =====================================================================

TEST(GoldenCC, Hold_Parse) {
    // Byte 0: PD(4)=3|TI(3)=7+TIF(1)=0 = 0x3E
    // Byte 1: messageType(6)=0x18|NSD(2)=0 = 0x60
    uint8_t data[] = {0x3E, 0x60};
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->MTI(), L3CCMessage::Hold);
}

// =====================================================================
// CC PARSE FROM HEX: Progress (GSM 04.08 9.3.17)
// =====================================================================

TEST(GoldenCC, Progress_Parse) {
    // Byte 0: PD(4)=3|TI(3)=7+TIF(1)=0 = 0x3E
    // Byte 1: messageType(6)=0x03|NSD(2)=0 = 0x0C
    uint8_t data[] = {0x3E, 0x0C};
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->MTI(), L3CCMessage::Progress);
}

// =====================================================================
// CC PARSE FROM HEX: Start DTMF (GSM 04.08 9.3.24)
// Reference: L3_Templates.ttcn ts_ML3_MO_CC_START_DTMF
// =====================================================================

TEST(GoldenCC, StartDTMF_Parse) {
    // Byte 0: PD(4)=3|TI(3)=7+TIF(1)=0 = 0x3E
    // Byte 1: messageType(6)=0x35|NSD(2)=0 = 0xDC
    // Byte 2: KeypadFacility = '1' = 0x31
    uint8_t data[] = {0x3E, 0xDC, 0x31};
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->MTI(), L3CCMessage::StartDTMF);
}

// =====================================================================
// CC PARSE FROM HEX: Stop DTMF (GSM 04.08 9.3.29)
// =====================================================================

TEST(GoldenCC, StopDTMF_Parse) {
    // Byte 0: PD(4)=3|TI(3)=7+TIF(1)=0 = 0x3E
    // Byte 1: messageType(6)=0x31|NSD(2)=0 = 0xC4
    uint8_t data[] = {0x3E, 0xC4};
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->MTI(), L3CCMessage::StopDTMF);
}

// =====================================================================
// CC PARSE FROM HEX: Release Complete with Cause (GSM 04.08 9.3.19)
// Reference: L3_Templates.ttcn ts_ML3_MO_CC_REL_COMPL
// =====================================================================

TEST(GoldenCC, ReleaseComplete_WithCause_Parse) {
    // Byte 0: PD(4)=3|TI(3)=7+TIF(1)=0 = 0x3E
    // Byte 1: messageType(6)=0x2a|NSD(2)=0 = 0xA8
    // Byte 2: IEI = 0x08 (Cause)
    // Byte 3: Length = 2
    // Byte 4: location(4)=3(Transit), spare(1)=0, codingStd(2)=11, ext(1)=0 = 0x36
    // Byte 5: causeValue(7)=17(User_Busy), ext(1)=1 = 0x22
    uint8_t data[] = {0x3E, 0xA8, 0x08, 0x02, 0x36, 0x22};
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->MTI(), L3CCMessage::ReleaseComplete);
}

// =====================================================================
// CC PARSE FROM HEX: Disconnect with Cause (GSM 04.08 9.3.7)
// Reference: L3_Templates.ttcn ts_ML3_MO_CC_DISC
// =====================================================================

TEST(GoldenCC, Disconnect_Parse) {
    // Byte 0: PD(4)=3|TI(3)=7+TIF(1)=0 = 0x3E
    // Byte 1: messageType(6)=0x25|NSD(2)=0 = 0x94
    // Cause TLV: IEI=0x08, length=2, octet3=0x16, octet4=0x21
    uint8_t data[] = {0x3E, 0x94, 0x08, 0x02, 0x16, 0x21};
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->MTI(), L3CCMessage::Disconnect);
    auto* d = dynamic_cast<L3Disconnect*>(msg.get());
    ASSERT_TRUE(d);
    EXPECT_EQ(d->cause(), CCCause::Normal_Call_Clearing);
    EXPECT_EQ(d->TI(), 7u);
}

// =====================================================================
// CC PARSE FROM HEX: Release (GSM 04.08 9.3.19)
// Reference: L3_Templates.ttcn ts_ML3_MO_CC_RELEASE
// =====================================================================

TEST(GoldenCC, Release_Parse) {
    // Byte 0: PD(4)=3|TI(3)=7+TIF(1)=1(REPL) = 0x3F
    // Byte 1: messageType(6)=0x2d|NSD(2)=0 = 0xB4
    uint8_t data[] = {0x3F, 0xB4};
    auto msg = parseL3(data, sizeof(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->MTI(), L3CCMessage::Release);
}

// =====================================================================
// CC ROUNDTrip: All messages
// =====================================================================

TEST(GoldenCC, Setup_NoDigits_RoundTrip) {
    L3Setup msg(7);
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3CCMessage::Setup);
    auto* s = dynamic_cast<L3Setup*>(parsed.get());
    ASSERT_TRUE(s);
    EXPECT_EQ(s->TI(), 7u);
    EXPECT_FALSE(s->haveCalledParty());
}

TEST(GoldenCC, Setup_WithDigits_RoundTrip) {
    L3CalledPartyBCDNumber called("1234567890");
    L3Setup msg(7, called);
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    auto* s = dynamic_cast<L3Setup*>(parsed.get());
    ASSERT_TRUE(s);
    EXPECT_TRUE(s->haveCalledParty());
    EXPECT_STREQ(s->digits(), "1234567890");
}

TEST(GoldenCC, EmergencySetup_RoundTrip) {
    L3EmergencySetup msg(7);
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3CCMessage::EmergencySetup);
}

TEST(GoldenCC, CallProceeding_RoundTrip) {
    L3CallProceeding msg(7);
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3CCMessage::CallProceeding);
}

TEST(GoldenCC, Alerting_RoundTrip) {
    L3Alerting msg(7);
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3CCMessage::Alerting);
}

TEST(GoldenCC, Connect_RoundTrip) {
    L3Connect msg(7);
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3CCMessage::Connect);
}

TEST(GoldenCC, ConnectAcknowledge_RoundTrip) {
    L3ConnectAcknowledge msg(7);
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3CCMessage::ConnectAcknowledge);
}

TEST(GoldenCC, CallConfirmed_RoundTrip) {
    L3CallConfirmed msg(7);
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3CCMessage::CallConfirmed);
}

TEST(GoldenCC, Disconnect_NormalClearing_RoundTrip) {
    L3Disconnect msg(7, CCCause::Normal_Call_Clearing);
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    auto* d = dynamic_cast<L3Disconnect*>(parsed.get());
    ASSERT_TRUE(d);
    EXPECT_EQ(d->cause(), CCCause::Normal_Call_Clearing);
    EXPECT_EQ(d->TI(), 7u);
}

TEST(GoldenCC, Disconnect_UserBusy_RoundTrip) {
    L3Disconnect msg(3, CCCause::User_Busy);
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    auto* d = dynamic_cast<L3Disconnect*>(parsed.get());
    ASSERT_TRUE(d);
    EXPECT_EQ(d->cause(), CCCause::User_Busy);
    EXPECT_EQ(d->TI(), 3u);
}

TEST(GoldenCC, Release_NoCause_RoundTrip) {
    L3Release msg(7);
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    auto* r = dynamic_cast<L3Release*>(parsed.get());
    ASSERT_TRUE(r);
    EXPECT_FALSE(r->haveCause());
}

TEST(GoldenCC, Release_WithCause_RoundTrip) {
    L3Release msg(7, CCCause::User_Busy);
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    auto* r = dynamic_cast<L3Release*>(parsed.get());
    ASSERT_TRUE(r);
    EXPECT_TRUE(r->haveCause());
    EXPECT_EQ(r->cause(), CCCause::User_Busy);
}

TEST(GoldenCC, ReleaseComplete_NoCause_RoundTrip) {
    L3ReleaseComplete msg(7);
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3CCMessage::ReleaseComplete);
}

TEST(GoldenCC, ReleaseComplete_WithCause_RoundTrip) {
    L3ReleaseComplete msg(5, CCCause::Normal_Call_Clearing);
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    auto* rc = dynamic_cast<L3ReleaseComplete*>(parsed.get());
    ASSERT_TRUE(rc);
    std::vector<uint8_t> buf1(msg.fullLength());
    std::vector<uint8_t> buf2(rc->fullLength());
    size_t n1 = writeL3(msg, buf1.data(), buf1.size());
    size_t n2 = writeL3(*rc, buf2.data(), buf2.size());
    EXPECT_EQ(n1, n2);
    for (size_t i = 0; i < n1; i++) {
        EXPECT_EQ(buf1[i], buf2[i]);
    }
}

TEST(GoldenCC, CCStatus_RoundTrip) {
    L3CCStatus msg(7, CCCause::Normal_Unspecified, 0x00);
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3CCMessage::CCStatus);
}

TEST(GoldenCC, StartDTMF_RoundTrip) {
    L3StartDTMF msg(7);
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3CCMessage::StartDTMF);
}

TEST(GoldenCC, StartDTMFAcknowledge_RoundTrip) {
    L3StartDTMFAcknowledge msg(7, '1');
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3CCMessage::StartDTMFAcknowledge);
}

TEST(GoldenCC, StartDTMFReject_RoundTrip) {
    L3StartDTMFReject msg(7, CCCause::Normal_Unspecified);
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3CCMessage::StartDTMFReject);
}

TEST(GoldenCC, StopDTMF_RoundTrip) {
    L3StopDTMF msg(7);
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3CCMessage::StopDTMF);
}

TEST(GoldenCC, StopDTMFAcknowledge_RoundTrip) {
    L3StopDTMFAcknowledge msg(7);
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3CCMessage::StopDTMFAcknowledge);
}

TEST(GoldenCC, Hold_RoundTrip) {
    L3Hold msg(7);
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3CCMessage::Hold);
}

TEST(GoldenCC, HoldReject_RoundTrip) {
    L3HoldReject msg(7, CCCause::Normal_Unspecified);
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3CCMessage::HoldReject);
}

TEST(GoldenCC, Progress_RoundTrip) {
    L3Progress msg(7);
    auto parsed = roundtrip(msg);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->MTI(), L3CCMessage::Progress);
}

// =====================================================================
// CC Cause values (GSM 04.08 10.5.4.11)
// Reference: L3_Templates.ttcn ML3_Cause_TLV
// =====================================================================

TEST(GoldenCC, CauseValues) {
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Unknown_L3_Cause), 0);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Unassigned_Number), 1);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::No_Route_To_Destination), 3);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Channel_Unacceptable), 6);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Operator_Determined_Barring), 8);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Normal_Call_Clearing), 16);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::User_Busy), 17);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::No_User_Responding), 18);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::User_Alerting_No_Answer), 19);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Call_Rejected), 21);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Number_Changed), 22);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Preemption), 25);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Non_Selected_User_Clearing), 26);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Destination_Out_Of_Order), 27);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Invalid_Number_Format), 28);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Facility_Rejected), 29);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Response_To_STATUS_ENQUIRY), 30);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Normal_Unspecified), 31);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::No_Channel_Available), 34);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Network_Out_Of_Order), 38);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Temporary_Failure), 41);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Switching_Equipment_Congestion), 42);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Access_Information_Discarded), 43);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Requested_Channel_Not_Available), 44);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Resources_Unavailable), 47);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Quality_Of_Service_Unavailable), 49);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Requested_Facility_Not_Subscribed), 50);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Bearer_Capability_Not_Authorized), 57);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Bearer_Capability_Not_Available), 58);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Incoming_Calls_Barred_Within_CUG), 55);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Service_Or_Option_Not_Available), 63);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Bearer_Service_Not_Implemented), 65);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::ACM_GE_Max), 68);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Requested_Facility_Not_Implemented), 69);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Only_Restricted_Digital_Info), 70);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Service_Or_Option_Not_Implemented), 79);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Invalid_Transaction_ID), 81);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::User_Not_Member_Of_CUG), 87);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Incompatible_Destination), 88);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Invalid_Transit_Network), 91);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Semantically_Incorrect_Message), 95);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Invalid_Mandatory_Information), 96);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Message_Type_Not_Implemented), 97);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Message_Not_Compatible_With_State), 98);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::IE_Not_Implemented), 99);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Conditional_IE_Error), 100);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Message_Not_Compatible), 101);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Recovery_On_Timer_Expiry), 102);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Protocol_Error_Unspecified), 111);
    EXPECT_EQ(static_cast<uint8_t>(CCCause::Interworking_Unspecified), 127);
}

// =====================================================================
// CC CauseLocation values (GSM 04.08 10.5.4.11)
// =====================================================================

TEST(GoldenCC, CauseLocationValues) {
    EXPECT_EQ(static_cast<uint8_t>(CCCauseLocation::User), 0);
    EXPECT_EQ(static_cast<uint8_t>(CCCauseLocation::Private_Serving_Local), 1);
    EXPECT_EQ(static_cast<uint8_t>(CCCauseLocation::Public_Serving_Local), 2);
    EXPECT_EQ(static_cast<uint8_t>(CCCauseLocation::Transit), 3);
    EXPECT_EQ(static_cast<uint8_t>(CCCauseLocation::Public_Serving_Remote), 4);
    EXPECT_EQ(static_cast<uint8_t>(CCCauseLocation::Private_Serving_Remote), 5);
    EXPECT_EQ(static_cast<uint8_t>(CCCauseLocation::International), 7);
    EXPECT_EQ(static_cast<uint8_t>(CCCauseLocation::Beyond_Inter_Networking), 10);
}

// =====================================================================
// CC IE: L3CauseElement (GSM 04.08 10.5.4.11)
// =====================================================================

TEST(GoldenCC, CauseElement_RoundTrip) {
    L3CauseElement orig(CCCause::User_Busy, CCCauseLocation::Transit);
    L3Frame frame(Primitive::L3_DATA, 32);
    size_t wp = 0;
    orig.writeV(frame, wp);
    L3CauseElement parsed;
    size_t rp = 0;
    parsed.parseV(frame, rp);
    EXPECT_EQ(parsed.cause(), CCCause::User_Busy);
    EXPECT_EQ(parsed.location(), CCCauseLocation::Transit);
}

// =====================================================================
// CC IE: L3BearerCapability (GSM 04.08 10.5.4.5)
// Reference: L3_Templates.ttcn ts_Bcap_voice
// =====================================================================

TEST(GoldenCC, BearerCapability_Default) {
    L3BearerCapability bc;
    EXPECT_EQ(bc.lengthV(), 1u);
}

// =====================================================================
// CC IE: L3CalledPartyBCDNumber (GSM 04.08 10.5.4.7)
// =====================================================================

TEST(GoldenCC, CalledPartyBCDNumber_International) {
    L3CalledPartyBCDNumber num("+79161234567");
    EXPECT_GT(num.lengthV(), 0u);
}

TEST(GoldenCC, CalledPartyBCDNumber_ShortNumber) {
    L3CalledPartyBCDNumber num("112");
    EXPECT_STREQ(num.digits(), "112");
}

TEST(GoldenCC, CalledPartyBCDNumber_RoundTrip) {
    L3CalledPartyBCDNumber orig("1234567890");
    L3Frame frame(Primitive::L3_DATA, 64);
    size_t wp = 0;
    orig.writeV(frame, wp);
    L3CalledPartyBCDNumber parsed;
    size_t rp = 0;
    parsed.parseV(frame, rp, orig.lengthV());
    EXPECT_STREQ(parsed.digits(), "1234567890");
}

// =====================================================================
// CC IE: L3CallingPartyBCDNumber (GSM 04.08 10.5.4.9)
// =====================================================================

TEST(GoldenCC, CallingPartyBCDNumber_RoundTrip) {
    L3CallingPartyBCDNumber orig("1234567890");
    L3Frame frame(Primitive::L3_DATA, 64);
    size_t wp = 0;
    orig.writeV(frame, wp);
    L3CallingPartyBCDNumber parsed;
    size_t rp = 0;
    parsed.parseV(frame, rp, orig.lengthV());
    EXPECT_STREQ(parsed.digits(), "1234567890");
}

// =====================================================================
// CC IE: L3ProgressIndicator (GSM 04.08 10.5.4.21)
// =====================================================================

TEST(GoldenCC, ProgressIndicator_RoundTrip) {
    L3ProgressIndicator orig(L3ProgressIndicator::InBandAvailable,
                             L3ProgressIndicator::PrivateServingLocal);
    L3Frame frame(Primitive::L3_DATA, 32);
    size_t wp = 0;
    orig.writeV(frame, wp);
    L3ProgressIndicator parsed;
    size_t rp = 0;
    parsed.parseV(frame, rp);
    EXPECT_EQ(parsed.progress(), L3ProgressIndicator::InBandAvailable);
    EXPECT_EQ(parsed.location(), L3ProgressIndicator::PrivateServingLocal);
}

// =====================================================================
// CC IE: L3KeypadFacility (GSM 04.08 10.5.4.17)
// =====================================================================

TEST(GoldenCC, KeypadFacility_RoundTrip) {
    L3KeypadFacility orig('5');
    EXPECT_EQ(orig.IA5(), '5');
    EXPECT_EQ(orig.lengthV(), 1u);
    L3Frame frame(Primitive::L3_DATA, 16);
    size_t wp = 0;
    orig.writeV(frame, wp);
    L3KeypadFacility parsed;
    size_t rp = 0;
    parsed.parseV(frame, rp);
    EXPECT_EQ(parsed.IA5(), '5');
}

// =====================================================================
// CC IE: L3Signal (GSM 04.08 10.5.4.23)
// =====================================================================

TEST(GoldenCC, Signal_Values) {
    L3Signal s1(L3Signal::SignalRingBackToneOn);
    EXPECT_EQ(s1.lengthV(), 1u);
    L3Signal s2(L3Signal::SignalTonesOff);
    EXPECT_EQ(s2.lengthV(), 1u);
}

// =====================================================================
// CC IE: L3CallState (GSM 04.08 10.5.4.3)
// =====================================================================

TEST(GoldenCC, CallState_RoundTrip) {
    L3CallState orig(0x05);
    EXPECT_EQ(orig.lengthV(), 1u);
    L3Frame frame(Primitive::L3_DATA, 16);
    size_t wp = 0;
    orig.writeV(frame, wp);
    L3CallState parsed;
    size_t rp = 0;
    parsed.parseV(frame, rp);
}

// =====================================================================
// CC IE: L3BCDDigits
// =====================================================================

TEST(GoldenCC, BCDDigits_Even) {
    L3BCDDigits orig("1234567890");
    EXPECT_STREQ(orig.digits(), "1234567890");
    EXPECT_EQ(orig.size(), 10u);
    EXPECT_EQ(orig.lengthV(), 5u);
}

TEST(GoldenCC, BCDDigits_Odd) {
    L3BCDDigits orig("12345");
    EXPECT_STREQ(orig.digits(), "12345");
    EXPECT_EQ(orig.size(), 5u);
    EXPECT_EQ(orig.lengthV(), 3u);
}

// =====================================================================
// CC IE: L3SupServFacilityIE
// =====================================================================

TEST(GoldenCC, SupServFacilityIE_RoundTrip) {
    L3SupServFacilityIE orig(std::string("\x81\x01\x13", 3));
    L3Frame frame(Primitive::L3_DATA, 32);
    size_t wp = 0;
    orig.writeV(frame, wp);
    L3SupServFacilityIE parsed;
    size_t rp = 0;
    parsed.parseV(frame, rp);
}

// =====================================================================
// CC IE: L3SupServVersionIndicator
// =====================================================================

TEST(GoldenCC, SupServVersionIndicator_RoundTrip) {
    L3SupServVersionIndicator orig;
    EXPECT_EQ(orig.lengthV(), 1u);
    L3Frame frame(Primitive::L3_DATA, 16);
    size_t wp = 0;
    orig.writeV(frame, wp);
    L3SupServVersionIndicator parsed;
    size_t rp = 0;
    parsed.parseV(frame, rp);
}

// =====================================================================
// CC: TI handling across all values
// =====================================================================

TEST(GoldenCC, TI_DifferentValues) {
    for (unsigned ti = 0; ti < 8; ti++) {
        L3Disconnect msg(ti, CCCause::Normal_Call_Clearing);
        auto parsed = roundtrip(msg);
        ASSERT_TRUE(parsed);
        auto* d = dynamic_cast<L3Disconnect*>(parsed.get());
        ASSERT_TRUE(d);
        EXPECT_EQ(d->TI(), ti);
    }
}

// =====================================================================
// CC: BSS Cause values (GSM 48.008 3.2.2.5)
// Reference: BSSAP_Templates.ttcn BssCause
// =====================================================================

TEST(GoldenCC, BSSCauseValues) {
    EXPECT_EQ(static_cast<uint8_t>(BSSCause::Radio_Interface_Failure), 1);
    EXPECT_EQ(static_cast<uint8_t>(BSSCause::Uplink_Quality), 2);
    EXPECT_EQ(static_cast<uint8_t>(BSSCause::Uplink_Strength), 3);
    EXPECT_EQ(static_cast<uint8_t>(BSSCause::Downlink_Quality), 4);
    EXPECT_EQ(static_cast<uint8_t>(BSSCause::Downlink_Strength), 5);
    EXPECT_EQ(static_cast<uint8_t>(BSSCause::Distance), 6);
    EXPECT_EQ(static_cast<uint8_t>(BSSCause::Operator_Intervention), 7);
    EXPECT_EQ(static_cast<uint8_t>(BSSCause::Channel_Assignment_Failure), 0x0a);
    EXPECT_EQ(static_cast<uint8_t>(BSSCause::Handover_Successful), 0x0b);
    EXPECT_EQ(static_cast<uint8_t>(BSSCause::Better_Cell), 0x0c);
    EXPECT_EQ(static_cast<uint8_t>(BSSCause::Traffic), 0x0f);
    EXPECT_EQ(static_cast<uint8_t>(BSSCause::Reduce_Load_In_Serving_Cell), 0x10);
    EXPECT_EQ(static_cast<uint8_t>(BSSCause::Equipment_Failure), 0x20);
    EXPECT_EQ(static_cast<uint8_t>(BSSCause::No_Radio_Resource_Available), 0x21);
    EXPECT_EQ(static_cast<uint8_t>(BSSCause::CCCH_Overload), 0x23);
    EXPECT_EQ(static_cast<uint8_t>(BSSCause::Processor_Overload), 0x24);
    EXPECT_EQ(static_cast<uint8_t>(BSSCause::Ciphering_Algorithm_Not_Supported), 0x40);
}
