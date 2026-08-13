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

// Full round-trip regression test for ALL L3 message types.
// For each type: build via Builder/constructor -> writeL3Bytes() -> parseL3() -> verify PD/MTI.
// This test ensures that no message type serialization or parsing is broken.
//
// Reference: message_types.h defines all ~200 L3 message classes across 12 domains.

#include <gtest/gtest.h>
#include <gsml3parser/parser.h>
#include <gsml3parser/visitor.h>
#include <gsml3parser/message_types.h>
#include <gsml3parser/enums.h>

using namespace gsml3parser;

// Helper: serialize -> parse -> verify PD and MTI match expected values.
static void expectRoundTrip(const ParsedMessage& msg, L3PD expectPD, int expectMTI, const char* typeName) {
    auto bytes = writeL3Bytes(msg);
    ASSERT_TRUE(bytes) << "writeL3Bytes failed for " << typeName;
    auto reparsed = parseL3(*bytes);
    ASSERT_TRUE(reparsed) << "parseL3 failed for round-trip of " << typeName;
    EXPECT_EQ(messagePD(*reparsed), expectPD) << typeName << " PD mismatch";
    EXPECT_EQ(messageMTI(*reparsed), expectMTI) << typeName << " MTI mismatch";
}

// Helper: serialize only (for short-message types that don't have standard L3 parse path).
static void expectWriteOnly(const ParsedMessage& msg, const char* typeName) {
    auto bytes = writeL3Bytes(msg);
    ASSERT_TRUE(bytes) << "writeL3Bytes failed for " << typeName;
}

// GSM 04.08 9.1: All RR message types round-trip via Builder/constructor
TEST(FullRoundTrip, RR_Domain) {
    // Paging messages
    expectRoundTrip(ParsedMessage{RRM{L3PagingRequestType1::builder().addMobileId(L3MobileIdentity(0x12345678), ChannelType::SDCCHType).build()}},
        L3PD::RadioResource, L3PagingRequestType1::MTI, "L3PagingRequestType1");

    expectRoundTrip(ParsedMessage{RRM{L3PagingRequestType2::builder().addTMSI(0x12345678, ChannelType::SDCCHType).build()}},
        L3PD::RadioResource, L3PagingRequestType2::MTI, "L3PagingRequestType2");

    expectRoundTrip(ParsedMessage{RRM{L3PagingRequestType3::builder().addTMSI(0x12345678, ChannelType::SDCCHType).build()}},
        L3PD::RadioResource, L3PagingRequestType3::MTI, "L3PagingRequestType3");

    expectRoundTrip(ParsedMessage{RRM{L3PagingResponse{}}},
        L3PD::RadioResource, L3PagingResponse::MTI, "L3PagingResponse");

    // Classmark and measurement
    expectRoundTrip(ParsedMessage{RRM{L3ClassmarkChange{}}},
        L3PD::RadioResource, L3ClassmarkChange::MTI, "L3ClassmarkChange");

    expectRoundTrip(ParsedMessage{RRM{L3ClassmarkEnquiry{}}},
        L3PD::RadioResource, L3ClassmarkEnquiry::MTI, "L3ClassmarkEnquiry");

    expectRoundTrip(ParsedMessage{RRM{L3MeasurementReport{}}},
        L3PD::RadioResource, L3MeasurementReport::MTI, "L3MeasurementReport");

    // Channel management
    expectRoundTrip(ParsedMessage{RRM{L3ChannelRelease{RRCause::Normal_Event}}},
        L3PD::RadioResource, L3ChannelRelease::MTI, "L3ChannelRelease");

    expectRoundTrip(ParsedMessage{RRM{L3RRStatus::builder().cause(RRCause::Normal_Event).build()}},
        L3PD::RadioResource, L3RRStatus::MTI, "L3RRStatus");

    expectRoundTrip(ParsedMessage{RRM{L3AssignmentCommand{}}},
        L3PD::RadioResource, L3AssignmentCommand::MTI, "L3AssignmentCommand");

    expectRoundTrip(ParsedMessage{RRM{L3AssignmentComplete::builder().cause(RRCause::Normal_Event).build()}},
        L3PD::RadioResource, L3AssignmentComplete::MTI, "L3AssignmentComplete");

    expectRoundTrip(ParsedMessage{RRM{L3AssignmentFailure::builder().cause(RRCause::Normal_Event).build()}},
        L3PD::RadioResource, L3AssignmentFailure::MTI, "L3AssignmentFailure");

    expectRoundTrip(ParsedMessage{RRM{L3ImmediateAssignment{}}},
        L3PD::RadioResource, L3ImmediateAssignment::MTI, "L3ImmediateAssignment");

    expectRoundTrip(ParsedMessage{RRM{L3ImmediateAssignmentExtended{}}},
        L3PD::RadioResource, L3ImmediateAssignmentExtended::MTI, "L3ImmediateAssignmentExtended");

    expectRoundTrip(ParsedMessage{RRM{L3ImmediateAssignmentReject{30}}},
        L3PD::RadioResource, L3ImmediateAssignmentReject::MTI, "L3ImmediateAssignmentReject");

    expectRoundTrip(ParsedMessage{RRM{L3AdditionalAssignment{}}},
        L3PD::RadioResource, L3AdditionalAssignment::MTI, "L3AdditionalAssignment");

    // Handover
    expectRoundTrip(ParsedMessage{RRM{L3HandoverCommand{}}},
        L3PD::RadioResource, L3HandoverCommand::MTI, "L3HandoverCommand");

    expectRoundTrip(ParsedMessage{RRM{L3HandoverComplete::builder().cause(RRCause::Normal_Event).build()}},
        L3PD::RadioResource, L3HandoverComplete::MTI, "L3HandoverComplete");

    expectRoundTrip(ParsedMessage{RRM{L3HandoverFailure::builder().cause(RRCause::Normal_Event).build()}},
        L3PD::RadioResource, L3HandoverFailure::MTI, "L3HandoverFailure");

    // Physical and ciphering
    expectRoundTrip(ParsedMessage{RRM{L3PhysicalInformation{}}},
        L3PD::RadioResource, L3PhysicalInformation::MTI, "L3PhysicalInformation");

    expectRoundTrip(ParsedMessage{RRM{L3CipheringModeCommand{false, 0}}},
        L3PD::RadioResource, L3CipheringModeCommand::MTI, "L3CipheringModeCommand");

    expectRoundTrip(ParsedMessage{RRM{L3CipheringModeComplete{}}},
        L3PD::RadioResource, L3CipheringModeComplete::MTI, "L3CipheringModeComplete");

    // Channel mode
    expectRoundTrip(ParsedMessage{RRM{L3ChannelModeModify{{}, {}}}},
        L3PD::RadioResource, L3ChannelModeModify::MTI, "L3ChannelModeModify");

    expectRoundTrip(ParsedMessage{RRM{L3ChannelModeModifyAcknowledge{}}},
        L3PD::RadioResource, L3ChannelModeModifyAcknowledge::MTI, "L3ChannelModeModifyAcknowledge");

    // GPRS suspension
    expectRoundTrip(ParsedMessage{RRM{L3GPRSSuspensionRequest{}}},
        L3PD::RadioResource, L3GPRSSuspensionRequest::MTI, "L3GPRSSuspensionRequest");

    // Application and sync
    expectRoundTrip(ParsedMessage{RRM{L3ApplicationInformation{{0xAB}}}},
        L3PD::RadioResource, L3ApplicationInformation::MTI, "L3ApplicationInformation");

    expectRoundTrip(ParsedMessage{RRM{L3SynchronizationChannelInformation{}}},
        L3PD::RadioResource, L3SynchronizationChannelInformation::MTI, "L3SynchronizationChannelInformation");

    // Short messages (RACH/HO access)
    expectRoundTrip(ParsedMessage{RRM{L3ChannelRequest{0x42}}},
        L3PD::RadioResource, L3ChannelRequest::MTI, "L3ChannelRequest");

    expectRoundTrip(ParsedMessage{RRM{L3HandoverAccess{0x17}}},
        L3PD::RadioResource, L3HandoverAccess::MTI, "L3HandoverAccess");

    // System Information Type 1-9
    expectRoundTrip(ParsedMessage{RRM{L3SystemInformationType1{}}},
        L3PD::RadioResource, L3SystemInformationType1::MTI, "L3SystemInformationType1");

    expectRoundTrip(ParsedMessage{RRM{L3SystemInformationType2{}}},
        L3PD::RadioResource, L3SystemInformationType2::MTI, "L3SystemInformationType2");

    expectRoundTrip(ParsedMessage{RRM{L3SystemInformationType2bis{}}},
        L3PD::RadioResource, L3SystemInformationType2bis::MTI, "L3SystemInformationType2bis");

    expectRoundTrip(ParsedMessage{RRM{L3SystemInformationType2ter{}}},
        L3PD::RadioResource, L3SystemInformationType2ter::MTI, "L3SystemInformationType2ter");

    expectRoundTrip(ParsedMessage{RRM{L3SystemInformationType3{}}},
        L3PD::RadioResource, L3SystemInformationType3::MTI, "L3SystemInformationType3");

    expectRoundTrip(ParsedMessage{RRM{L3SystemInformationType4{}}},
        L3PD::RadioResource, L3SystemInformationType4::MTI, "L3SystemInformationType4");

    expectRoundTrip(ParsedMessage{RRM{L3SystemInformationType5{}}},
        L3PD::RadioResource, L3SystemInformationType5::MTI, "L3SystemInformationType5");

    expectRoundTrip(ParsedMessage{RRM{L3SystemInformationType5bis{}}},
        L3PD::RadioResource, L3SystemInformationType5bis::MTI, "L3SystemInformationType5bis");

    expectRoundTrip(ParsedMessage{RRM{L3SystemInformationType5ter{}}},
        L3PD::RadioResource, L3SystemInformationType5ter::MTI, "L3SystemInformationType5ter");

    expectRoundTrip(ParsedMessage{RRM{L3SystemInformationType6{}}},
        L3PD::RadioResource, L3SystemInformationType6::MTI, "L3SystemInformationType6");

    expectRoundTrip(ParsedMessage{RRM{L3SystemInformationType7{}}},
        L3PD::RadioResource, L3SystemInformationType7::MTI, "L3SystemInformationType7");

    expectRoundTrip(ParsedMessage{RRM{L3SystemInformationType8{}}},
        L3PD::RadioResource, L3SystemInformationType8::MTI, "L3SystemInformationType8");

    expectRoundTrip(ParsedMessage{RRM{L3SystemInformationType9{}}},
        L3PD::RadioResource, L3SystemInformationType9::MTI, "L3SystemInformationType9");

    // SI13, SI16, SI17
    expectRoundTrip(ParsedMessage{RRM{L3SystemInformationType13{}}},
        L3PD::RadioResource, L3SystemInformationType13::MTI, "L3SystemInformationType13");

    expectRoundTrip(ParsedMessage{RRM{L3SystemInformationType16{}}},
        L3PD::RadioResource, L3SystemInformationType16::MTI, "L3SystemInformationType16");

    expectRoundTrip(ParsedMessage{RRM{L3SystemInformationType17{}}},
        L3PD::RadioResource, L3SystemInformationType17::MTI, "L3SystemInformationType17");

    // Configuration change and partial release
    expectRoundTrip(ParsedMessage{RRM{L3ConfigurationChangeCommand{}}},
        L3PD::RadioResource, L3ConfigurationChangeCommand::MTI, "L3ConfigurationChangeCommand");

    expectRoundTrip(ParsedMessage{RRM{L3ConfigurationChangeAcknowledge{}}},
        L3PD::RadioResource, L3ConfigurationChangeAcknowledge::MTI, "L3ConfigurationChangeAcknowledge");

    expectRoundTrip(ParsedMessage{RRM{L3ConfigurationChangeReject{RRCause::Normal_Event}}},
        L3PD::RadioResource, L3ConfigurationChangeReject::MTI, "L3ConfigurationChangeReject");

    expectRoundTrip(ParsedMessage{RRM{L3PartialRelease{}}},
        L3PD::RadioResource, L3PartialRelease::MTI, "L3PartialRelease");

    expectRoundTrip(ParsedMessage{RRM{L3PartialReleaseComplete{}}},
        L3PD::RadioResource, L3PartialReleaseComplete::MTI, "L3PartialReleaseComplete");

    // Extended measurement and frequency
    expectRoundTrip(ParsedMessage{RRM{L3ExtendedMeasurementReport{}}},
        L3PD::RadioResource, L3ExtendedMeasurementReport::MTI, "L3ExtendedMeasurementReport");

    expectRoundTrip(ParsedMessage{RRM{L3ExtendedMeasurementOrder{}}},
        L3PD::RadioResource, L3ExtendedMeasurementOrder::MTI, "L3ExtendedMeasurementOrder");

    expectRoundTrip(ParsedMessage{RRM{L3FrequencyRedefinition{}}},
        L3PD::RadioResource, L3FrequencyRedefinition::MTI, "L3FrequencyRedefinition");

    // Notification
    expectRoundTrip(ParsedMessage{RRM{L3NotificationNCH{}}},
        L3PD::RadioResource, L3NotificationNCH::MTI, "L3NotificationNCH");

    expectRoundTrip(ParsedMessage{RRM{L3NotificationResponse{}}},
        L3PD::RadioResource, L3NotificationResponse::MTI, "L3NotificationResponse");

    // VGCS/VBS
    expectRoundTrip(ParsedMessage{RRM{L3VGCSUplinkGrant{}}},
        L3PD::RadioResource, L3VGCSUplinkGrant::MTI, "L3VGCSUplinkGrant");

    expectRoundTrip(ParsedMessage{RRM{L3UplinkRelease{}}},
        L3PD::RadioResource, L3UplinkRelease::MTI, "L3UplinkRelease");

    expectRoundTrip(ParsedMessage{RRM{L3UplinkBusy{}}},
        L3PD::RadioResource, L3UplinkBusy::MTI, "L3UplinkBusy");

    expectRoundTrip(ParsedMessage{RRM{L3TalkerIndication{}}},
        L3PD::RadioResource, L3TalkerIndication::MTI, "L3TalkerIndication");

    expectRoundTrip(ParsedMessage{RRM{L3PriorityUplinkRequest{}}},
        L3PD::RadioResource, L3PriorityUplinkRequest::MTI, "L3PriorityUplinkRequest");

    expectRoundTrip(ParsedMessage{RRM{L3DataIndication{}}},
        L3PD::RadioResource, L3DataIndication::MTI, "L3DataIndication");

    expectRoundTrip(ParsedMessage{RRM{L3DataIndication2{}}},
        L3PD::RadioResource, L3DataIndication2::MTI, "L3DataIndication2");

    // DTM and Packet
    expectRoundTrip(ParsedMessage{RRM{L3DTMAssignmentFailure{RRCause::Normal_Event}}},
        L3PD::RadioResource, L3DTMAssignmentFailure::MTI, "L3DTMAssignmentFailure");

    expectRoundTrip(ParsedMessage{RRM{L3DTMReject{}}},
        L3PD::RadioResource, L3DTMReject::MTI, "L3DTMReject");

    expectRoundTrip(ParsedMessage{RRM{L3DTMRequest{}}},
        L3PD::RadioResource, L3DTMRequest::MTI, "L3DTMRequest");

    expectRoundTrip(ParsedMessage{RRM{L3PacketAssignment{}}},
        L3PD::RadioResource, L3PacketAssignment::MTI, "L3PacketAssignment");

    expectRoundTrip(ParsedMessage{RRM{L3DTMAssignmentCommand{}}},
        L3PD::RadioResource, L3DTMAssignmentCommand::MTI, "L3DTMAssignmentCommand");

    expectRoundTrip(ParsedMessage{RRM{L3DTMInformation{}}},
        L3PD::RadioResource, L3DTMInformation::MTI, "L3DTMInformation");

    expectRoundTrip(ParsedMessage{RRM{L3PacketInformation{}}},
        L3PD::RadioResource, L3PacketInformation::MTI, "L3PacketInformation");

    // Inter-RAT classmark
    expectRoundTrip(ParsedMessage{RRM{L3UTRANClassmarkChange{}}},
        L3PD::RadioResource, L3UTRANClassmarkChange::MTI, "L3UTRANClassmarkChange");

    expectRoundTrip(ParsedMessage{RRM{L3CDMA2000ClassmarkChange{}}},
        L3PD::RadioResource, L3CDMA2000ClassmarkChange::MTI, "L3CDMA2000ClassmarkChange");

    expectRoundTrip(ParsedMessage{RRM{L3IntersysToUTRANHOCommand{}}},
        L3PD::RadioResource, L3IntersysToUTRANHOCommand::MTI, "L3IntersysToUTRANHOCommand");

    expectRoundTrip(ParsedMessage{RRM{L3IntersysToCDMA2000HOCommand{}}},
        L3PD::RadioResource, L3IntersysToCDMA2000HOCommand::MTI, "L3IntersysToCDMA2000HOCommand");

    expectRoundTrip(ParsedMessage{RRM{L3GERANIUClassmarkChange{}}},
        L3PD::RadioResource, L3GERANIUClassmarkChange::MTI, "L3GERANIUClassmarkChange");

    // Extended SI types (14-23)
    expectRoundTrip(ParsedMessage{RRM{L3SystemInformationType14{}}},
        L3PD::RadioResource, L3SystemInformationType14::MTI, "L3SystemInformationType14");

    expectRoundTrip(ParsedMessage{RRM{L3SystemInformationType15{}}},
        L3PD::RadioResource, L3SystemInformationType15::MTI, "L3SystemInformationType15");

    expectRoundTrip(ParsedMessage{RRM{L3SystemInformationType18{}}},
        L3PD::RadioResource, L3SystemInformationType18::MTI, "L3SystemInformationType18");

    expectRoundTrip(ParsedMessage{RRM{L3SystemInformationType19{}}},
        L3PD::RadioResource, L3SystemInformationType19::MTI, "L3SystemInformationType19");

    expectRoundTrip(ParsedMessage{RRM{L3SystemInformationType20{}}},
        L3PD::RadioResource, L3SystemInformationType20::MTI, "L3SystemInformationType20");

    expectRoundTrip(ParsedMessage{RRM{L3SystemInformationType13alt{}}},
        L3PD::RadioResource, L3SystemInformationType13alt::MTI, "L3SystemInformationType13alt");

    expectRoundTrip(ParsedMessage{RRM{L3SystemInformationType2n{}}},
        L3PD::RadioResource, L3SystemInformationType2n::MTI, "L3SystemInformationType2n");

    expectRoundTrip(ParsedMessage{RRM{L3SystemInformationType21{}}},
        L3PD::RadioResource, L3SystemInformationType21::MTI, "L3SystemInformationType21");

    expectRoundTrip(ParsedMessage{RRM{L3SystemInformationType22{}}},
        L3PD::RadioResource, L3SystemInformationType22::MTI, "L3SystemInformationType22");

    expectRoundTrip(ParsedMessage{RRM{L3SystemInformationType23{}}},
        L3PD::RadioResource, L3SystemInformationType23::MTI, "L3SystemInformationType23");

    // Write-only short messages (FACCH/SCH) - no standard L3 parse path
    expectWriteOnly(ParsedMessage{RRM{L3SystemInformationType10{}}}, "L3SystemInformationType10");
    expectWriteOnly(ParsedMessage{RRM{L3SystemInformationType10bis{}}}, "L3SystemInformationType10bis");
    expectWriteOnly(ParsedMessage{RRM{L3SystemInformationType10ter{}}}, "L3SystemInformationType10ter");
    expectWriteOnly(ParsedMessage{RRM{L3NotificationFACCH{}}}, "L3NotificationFACCH");
    expectWriteOnly(ParsedMessage{RRM{L3UplinkFree{}}}, "L3UplinkFree");
    expectWriteOnly(ParsedMessage{RRM{L3EnhancedMeasurementRepUL{}}}, "L3EnhancedMeasurementRepUL");
    expectWriteOnly(ParsedMessage{RRM{L3MeasurementInfoDL{}}}, "L3MeasurementInfoDL");
    expectWriteOnly(ParsedMessage{RRM{L3VBSVGCSRecon{}}}, "L3VBSVGCSRecon");
    expectWriteOnly(ParsedMessage{RRM{L3VBSVGCSRecon2{}}}, "L3VBSVGCSRecon2");
    expectWriteOnly(ParsedMessage{RRM{L3VGCSAddInfo{}}}, "L3VGCSAddInfo");
    expectWriteOnly(ParsedMessage{RRM{L3VGCSMSInfo{}}}, "L3VGCSMSInfo");
    expectWriteOnly(ParsedMessage{RRM{L3VGCSSNeighCellInfo{}}}, "L3VGCSSNeighCellInfo");
    expectWriteOnly(ParsedMessage{RRM{L3NotifyAppData{}}}, "L3NotifyAppData");

    expectRoundTrip(ParsedMessage{RRM{L3SystemInformationType2quater{}}},
        L3PD::RadioResource, L3SystemInformationType2quater::MTI, "L3SystemInformationType2quater");
}

// GSM 24.008 9.2: All MM message types round-trip
TEST(FullRoundTrip, MM_Domain) {
    expectRoundTrip(ParsedMessage{MMM{L3IMSIDetachIndication{}}},
        L3PD::MobilityManagement, L3IMSIDetachIndication::MTI, "L3IMSIDetachIndication");

    expectRoundTrip(ParsedMessage{MMM{L3CMServiceAccept{}}},
        L3PD::MobilityManagement, L3CMServiceAccept::MTI, "L3CMServiceAccept");

    expectRoundTrip(ParsedMessage{MMM{L3CMServiceReject{MMRejectCause::Zero}}},
        L3PD::MobilityManagement, L3CMServiceReject::MTI, "L3CMServiceReject");

    expectRoundTrip(ParsedMessage{MMM{L3CMServiceAbort{}}},
        L3PD::MobilityManagement, L3CMServiceAbort::MTI, "L3CMServiceAbort");

    expectRoundTrip(ParsedMessage{MMM{L3CMServiceRequest{}}},
        L3PD::MobilityManagement, L3CMServiceRequest::MTI, "L3CMServiceRequest");

    expectRoundTrip(ParsedMessage{MMM{L3CMReestablishmentRequest{}}},
        L3PD::MobilityManagement, L3CMReestablishmentRequest::MTI, "L3CMReestablishmentRequest");

    expectRoundTrip(ParsedMessage{MMM{L3IdentityResponse{}}},
        L3PD::MobilityManagement, L3IdentityResponse::MTI, "L3IdentityResponse");

    expectRoundTrip(ParsedMessage{MMM{L3IdentityRequest{MobileIDType::NoID}}},
        L3PD::MobilityManagement, L3IdentityRequest::MTI, "L3IdentityRequest");

    expectRoundTrip(ParsedMessage{MMM{L3MMInformation{}}},
        L3PD::MobilityManagement, L3MMInformation::MTI, "L3MMInformation");

    expectRoundTrip(ParsedMessage{MMM{L3LocationUpdatingAccept{}}},
        L3PD::MobilityManagement, L3LocationUpdatingAccept::MTI, "L3LocationUpdatingAccept");

    expectRoundTrip(ParsedMessage{MMM{L3LocationUpdatingReject{MMRejectCause::Zero}}},
        L3PD::MobilityManagement, L3LocationUpdatingReject::MTI, "L3LocationUpdatingReject");

    expectRoundTrip(ParsedMessage{MMM{L3LocationUpdatingRequest{}}},
        L3PD::MobilityManagement, L3LocationUpdatingRequest::MTI, "L3LocationUpdatingRequest");

    expectRoundTrip(ParsedMessage{MMM{L3TMSIReallocationCommand{}}},
        L3PD::MobilityManagement, L3TMSIReallocationCommand::MTI, "L3TMSIReallocationCommand");

    expectRoundTrip(ParsedMessage{MMM{L3TMSIReallocationComplete{}}},
        L3PD::MobilityManagement, L3TMSIReallocationComplete::MTI, "L3TMSIReallocationComplete");

    expectRoundTrip(ParsedMessage{MMM{L3MMStatus::builder().cause(MMRejectCause::Zero).build()}},
        L3PD::MobilityManagement, L3MMStatus::MTI, "L3MMStatus");

    expectRoundTrip(ParsedMessage{MMM{L3AuthenticationRequest{0, {}}}},
        L3PD::MobilityManagement, L3AuthenticationRequest::MTI, "L3AuthenticationRequest");

    expectRoundTrip(ParsedMessage{MMM{L3AuthenticationResponse{0}}},
        L3PD::MobilityManagement, L3AuthenticationResponse::MTI, "L3AuthenticationResponse");

    expectRoundTrip(ParsedMessage{MMM{L3AuthenticationReject{}}},
        L3PD::MobilityManagement, L3AuthenticationReject::MTI, "L3AuthenticationReject");

    expectRoundTrip(ParsedMessage{MMM{L3CMRequest{}}},
        L3PD::MobilityManagement, L3CMRequest::MTI, "L3CMRequest");

    expectRoundTrip(ParsedMessage{MMM{L3PagingMM{}}},
        L3PD::MobilityManagement, L3PagingMM::MTI, "L3PagingMM");
}

// GSM 24.008 9.3: All CC message types round-trip
TEST(FullRoundTrip, CC_Domain) {
    expectRoundTrip(ParsedMessage{CCM{L3Setup{}}},
        L3PD::CallControl, L3Setup::MTI, "L3Setup");

    expectRoundTrip(ParsedMessage{CCM{L3EmergencySetup{}}},
        L3PD::CallControl, L3EmergencySetup::MTI, "L3EmergencySetup");

    expectRoundTrip(ParsedMessage{CCM{L3CallProceeding{}}},
        L3PD::CallControl, L3CallProceeding::MTI, "L3CallProceeding");

    expectRoundTrip(ParsedMessage{CCM{L3Alerting{}}},
        L3PD::CallControl, L3Alerting::MTI, "L3Alerting");

    expectRoundTrip(ParsedMessage{CCM{L3Connect{}}},
        L3PD::CallControl, L3Connect::MTI, "L3Connect");

    expectRoundTrip(ParsedMessage{CCM{L3ConnectAcknowledge{}}},
        L3PD::CallControl, L3ConnectAcknowledge::MTI, "L3ConnectAcknowledge");

    expectRoundTrip(ParsedMessage{CCM{L3CallConfirmed{}}},
        L3PD::CallControl, L3CallConfirmed::MTI, "L3CallConfirmed");

    expectRoundTrip(ParsedMessage{CCM{L3Disconnect::builder().cause(CCCause::Normal_Call_Clearing).build()}},
        L3PD::CallControl, L3Disconnect::MTI, "L3Disconnect");

    expectRoundTrip(ParsedMessage{CCM{L3Release{}}},
        L3PD::CallControl, L3Release::MTI, "L3Release");

    expectRoundTrip(ParsedMessage{CCM{L3ReleaseComplete{}}},
        L3PD::CallControl, L3ReleaseComplete::MTI, "L3ReleaseComplete");

    expectRoundTrip(ParsedMessage{CCM{L3StartDTMF::builder().key('A').build()}},
        L3PD::CallControl, L3StartDTMF::MTI, "L3StartDTMF");

    expectRoundTrip(ParsedMessage{CCM{L3StopDTMF{}}},
        L3PD::CallControl, L3StopDTMF::MTI, "L3StopDTMF");

    expectRoundTrip(ParsedMessage{CCM{L3StopDTMFAcknowledge{}}},
        L3PD::CallControl, L3StopDTMFAcknowledge::MTI, "L3StopDTMFAcknowledge");

    expectRoundTrip(ParsedMessage{CCM{L3StartDTMFAcknowledge('A')}},
        L3PD::CallControl, L3StartDTMFAcknowledge::MTI, "L3StartDTMFAcknowledge");

    expectRoundTrip(ParsedMessage{CCM{L3StartDTMFReject::builder().cause(CCCause::Normal_Call_Clearing).build()}},
        L3PD::CallControl, L3StartDTMFReject::MTI, "L3StartDTMFReject");

    expectRoundTrip(ParsedMessage{CCM{L3Hold{}}},
        L3PD::CallControl, L3Hold::MTI, "L3Hold");

    expectRoundTrip(ParsedMessage{CCM{L3HoldReject::builder().cause(CCCause::Normal_Call_Clearing).build()}},
        L3PD::CallControl, L3HoldReject::MTI, "L3HoldReject");

    expectRoundTrip(ParsedMessage{CCM{L3CCStatus::builder().cause(CCCause::Normal_Call_Clearing).build()}},
        L3PD::CallControl, L3CCStatus::MTI, "L3CCStatus");

    expectRoundTrip(ParsedMessage{CCM{L3Progress{}}},
        L3PD::CallControl, L3Progress::MTI, "L3Progress");

    expectRoundTrip(ParsedMessage{CCM{L3Facility{}}},
        L3PD::CallControl, L3Facility::MTI, "L3Facility");

    expectRoundTrip(ParsedMessage{CCM{L3Modify{}}},
        L3PD::CallControl, L3Modify::MTI, "L3Modify");

    expectRoundTrip(ParsedMessage{CCM{L3UnitData{}}},
        L3PD::CallControl, L3UnitData::MTI, "L3UnitData");

    expectRoundTrip(ParsedMessage{CCM{L3UnitDataAck{}}},
        L3PD::CallControl, L3UnitDataAck::MTI, "L3UnitDataAck");

    expectRoundTrip(ParsedMessage{CCM{L3ErrorIndication::builder().cause(CCCause::Normal_Call_Clearing).build()}},
        L3PD::CallControl, L3ErrorIndication::MTI, "L3ErrorIndication");
}

// Supplementary Service domain
TEST(FullRoundTrip, SS_Domain) {
    expectRoundTrip(ParsedMessage{SSM{L3SupServFacilityMessage{}}},
        L3PD::NonCallSS, L3SupServFacilityMessage::MTI, "L3SupServFacilityMessage");

    expectRoundTrip(ParsedMessage{SSM{L3SupServRegisterMessage{}}},
        L3PD::NonCallSS, L3SupServRegisterMessage::MTI, "L3SupServRegisterMessage");

    expectRoundTrip(ParsedMessage{SSM{L3SupServReleaseCompleteMessage{}}},
        L3PD::NonCallSS, L3SupServReleaseCompleteMessage::MTI, "L3SupServReleaseCompleteMessage");
}

// GMM domain (GPRS Mobility Management)
TEST(FullRoundTrip, GMM_Domain) {
    expectWriteOnly(ParsedMessage{GMM{L3AttachRequest{}}}, "L3AttachRequest");

    expectRoundTrip(ParsedMessage{GMM{L3AttachAccept{}}},
        L3PD::GPRSMobilityManagement, L3AttachAccept::MTI, "L3AttachAccept");

    expectRoundTrip(ParsedMessage{GMM{L3AttachComplete{}}},
        L3PD::GPRSMobilityManagement, L3AttachComplete::MTI, "L3AttachComplete");

    expectRoundTrip(ParsedMessage{GMM{L3AttachReject{}}},
        L3PD::GPRSMobilityManagement, L3AttachReject::MTI, "L3AttachReject");

    expectRoundTrip(ParsedMessage{GMM{L3DetachRequest{}}},
        L3PD::GPRSMobilityManagement, L3DetachRequest::MTI, "L3DetachRequest");

    expectRoundTrip(ParsedMessage{GMM{L3DetachAccept{}}},
        L3PD::GPRSMobilityManagement, L3DetachAccept::MTI, "L3DetachAccept");

    expectRoundTrip(ParsedMessage{GMM{L3RoutingAreaUpdateRequest{}}},
        L3PD::GPRSMobilityManagement, L3RoutingAreaUpdateRequest::MTI, "L3RoutingAreaUpdateRequest");

    expectRoundTrip(ParsedMessage{GMM{L3RoutingAreaUpdateAccept{}}},
        L3PD::GPRSMobilityManagement, L3RoutingAreaUpdateAccept::MTI, "L3RoutingAreaUpdateAccept");

    expectRoundTrip(ParsedMessage{GMM{L3RoutingAreaUpdateComplete{}}},
        L3PD::GPRSMobilityManagement, L3RoutingAreaUpdateComplete::MTI, "L3RoutingAreaUpdateComplete");

    expectRoundTrip(ParsedMessage{GMM{L3RoutingAreaUpdateReject{}}},
        L3PD::GPRSMobilityManagement, L3RoutingAreaUpdateReject::MTI, "L3RoutingAreaUpdateReject");

    expectRoundTrip(ParsedMessage{GMM{L3ServiceRequest{}}},
        L3PD::GPRSMobilityManagement, L3ServiceRequest::MTI, "L3ServiceRequest");

    expectRoundTrip(ParsedMessage{GMM{L3ServiceAccept{}}},
        L3PD::GPRSMobilityManagement, L3ServiceAccept::MTI, "L3ServiceAccept");

    expectRoundTrip(ParsedMessage{GMM{L3ServiceReject{}}},
        L3PD::GPRSMobilityManagement, L3ServiceReject::MTI, "L3ServiceReject");

    expectRoundTrip(ParsedMessage{GMM{L3P_TMSIReallocationCommand{}}},
        L3PD::GPRSMobilityManagement, L3P_TMSIReallocationCommand::MTI, "L3P_TMSIReallocationCommand");

    expectRoundTrip(ParsedMessage{GMM{L3P_TMSIReallocationComplete{}}},
        L3PD::GPRSMobilityManagement, L3P_TMSIReallocationComplete::MTI, "L3P_TMSIReallocationComplete");

    expectRoundTrip(ParsedMessage{GMM{L3AuthenticationAndCipheringRequest{}}},
        L3PD::GPRSMobilityManagement, L3AuthenticationAndCipheringRequest::MTI, "L3AuthenticationAndCipheringRequest");

    expectRoundTrip(ParsedMessage{GMM{L3AuthenticationAndCipheringResponse{}}},
        L3PD::GPRSMobilityManagement, L3AuthenticationAndCipheringResponse::MTI, "L3AuthenticationAndCipheringResponse");

    expectRoundTrip(ParsedMessage{GMM{L3AuthenticationAndCipheringReject{}}},
        L3PD::GPRSMobilityManagement, L3AuthenticationAndCipheringReject::MTI, "L3AuthenticationAndCipheringReject");

    expectRoundTrip(ParsedMessage{GMM{L3GMMIdentityRequest{}}},
        L3PD::GPRSMobilityManagement, L3GMMIdentityRequest::MTI, "L3GMMIdentityRequest");

    expectRoundTrip(ParsedMessage{GMM{L3GMMIdentityResponse{}}},
        L3PD::GPRSMobilityManagement, L3GMMIdentityResponse::MTI, "L3GMMIdentityResponse");

    expectRoundTrip(ParsedMessage{GMM{L3AuthenticationAndCipheringFailure{}}},
        L3PD::GPRSMobilityManagement, L3AuthenticationAndCipheringFailure::MTI, "L3AuthenticationAndCipheringFailure");

    expectRoundTrip(ParsedMessage{GMM{L3GMMStatus{GMMCause::Unspecified}}},
        L3PD::GPRSMobilityManagement, L3GMMStatus::MTI, "L3GMMStatus");

    expectRoundTrip(ParsedMessage{GMM{L3GMMInformation{}}},
        L3PD::GPRSMobilityManagement, L3GMMInformation::MTI, "L3GMMInformation");
}

// SM domain (Session Management)
TEST(FullRoundTrip, SM_Domain) {
    expectRoundTrip(ParsedMessage{SM{L3ActivatePDPContextRequest{}}},
        L3PD::GPRSSessionManagement, L3ActivatePDPContextRequest::MTI, "L3ActivatePDPContextRequest");

    expectRoundTrip(ParsedMessage{SM{L3ActivatePDPContextAccept{}}},
        L3PD::GPRSSessionManagement, L3ActivatePDPContextAccept::MTI, "L3ActivatePDPContextAccept");

    expectRoundTrip(ParsedMessage{SM{L3ActivatePDPContextReject{}}},
        L3PD::GPRSSessionManagement, L3ActivatePDPContextReject::MTI, "L3ActivatePDPContextReject");

    expectRoundTrip(ParsedMessage{SM{L3DeactivatePDPContextRequest{}}},
        L3PD::GPRSSessionManagement, L3DeactivatePDPContextRequest::MTI, "L3DeactivatePDPContextRequest");

    expectRoundTrip(ParsedMessage{SM{L3DeactivatePDPContextAccept{}}},
        L3PD::GPRSSessionManagement, L3DeactivatePDPContextAccept::MTI, "L3DeactivatePDPContextAccept");

    expectRoundTrip(ParsedMessage{SM{L3ModifyPDPContextRequest{}}},
        L3PD::GPRSSessionManagement, L3ModifyPDPContextRequest::MTI, "L3ModifyPDPContextRequest");

    expectRoundTrip(ParsedMessage{SM{L3ModifyPDPContextAccept{}}},
        L3PD::GPRSSessionManagement, L3ModifyPDPContextAccept::MTI, "L3ModifyPDPContextAccept");

    expectRoundTrip(ParsedMessage{SM{L3ModifyPDPContextReject{}}},
        L3PD::GPRSSessionManagement, L3ModifyPDPContextReject::MTI, "L3ModifyPDPContextReject");

    expectRoundTrip(ParsedMessage{SM{L3SMStatus{SMCause::ReqAccepted}}},
        L3PD::GPRSSessionManagement, L3SMStatus::MTI, "L3SMStatus");

    expectRoundTrip(ParsedMessage{SM{L3RequestPDPContextActivation{}}},
        L3PD::GPRSSessionManagement, L3RequestPDPContextActivation::MTI, "L3RequestPDPContextActivation");

    expectRoundTrip(ParsedMessage{SM{L3RequestPDPContextActivationReject{}}},
        L3PD::GPRSSessionManagement, L3RequestPDPContextActivationReject::MTI, "L3RequestPDPContextActivationReject");

    expectRoundTrip(ParsedMessage{SM{L3ModifyPDPContextRequestMS{}}},
        L3PD::GPRSSessionManagement, L3ModifyPDPContextRequestMS::MTI, "L3ModifyPDPContextRequestMS");

    expectRoundTrip(ParsedMessage{SM{L3ModifyPDPContextAcceptNet{}}},
        L3PD::GPRSSessionManagement, L3ModifyPDPContextAcceptNet::MTI, "L3ModifyPDPContextAcceptNet");

    expectRoundTrip(ParsedMessage{SM{L3ActivateSecondaryPDPContextRequest{}}},
        L3PD::GPRSSessionManagement, L3ActivateSecondaryPDPContextRequest::MTI, "L3ActivateSecondaryPDPContextRequest");

    expectRoundTrip(ParsedMessage{SM{L3ActivateSecondaryPDPContextAccept{}}},
        L3PD::GPRSSessionManagement, L3ActivateSecondaryPDPContextAccept::MTI, "L3ActivateSecondaryPDPContextAccept");

    expectRoundTrip(ParsedMessage{SM{L3ActivateSecondaryPDPContextReject{}}},
        L3PD::GPRSSessionManagement, L3ActivateSecondaryPDPContextReject::MTI, "L3ActivateSecondaryPDPContextReject");

    expectRoundTrip(ParsedMessage{SM{L3ActivateAAPDPContextRequest{}}},
        L3PD::GPRSSessionManagement, L3ActivateAAPDPContextRequest::MTI, "L3ActivateAAPDPContextRequest");

    expectRoundTrip(ParsedMessage{SM{L3ActivateAAPDPContextAccept{}}},
        L3PD::GPRSSessionManagement, L3ActivateAAPDPContextAccept::MTI, "L3ActivateAAPDPContextAccept");

    expectRoundTrip(ParsedMessage{SM{L3ActivateAAPDPContextReject{}}},
        L3PD::GPRSSessionManagement, L3ActivateAAPDPContextReject::MTI, "L3ActivateAAPDPContextReject");

    expectRoundTrip(ParsedMessage{SM{L3DeactivateAAPDPContextRequest{}}},
        L3PD::GPRSSessionManagement, L3DeactivateAAPDPContextRequest::MTI, "L3DeactivateAAPDPContextRequest");

    expectRoundTrip(ParsedMessage{SM{L3DeactivateAAPDPContextAccept{}}},
        L3PD::GPRSSessionManagement, L3DeactivateAAPDPContextAccept::MTI, "L3DeactivateAAPDPContextAccept");

    expectRoundTrip(ParsedMessage{SM{L3ActivateMBMSContextRequest{}}},
        L3PD::GPRSSessionManagement, L3ActivateMBMSContextRequest::MTI, "L3ActivateMBMSContextRequest");

    expectRoundTrip(ParsedMessage{SM{L3ActivateMBMSContextAccept{}}},
        L3PD::GPRSSessionManagement, L3ActivateMBMSContextAccept::MTI, "L3ActivateMBMSContextAccept");

    expectRoundTrip(ParsedMessage{SM{L3ActivateMBMSContextReject{}}},
        L3PD::GPRSSessionManagement, L3ActivateMBMSContextReject::MTI, "L3ActivateMBMSContextReject");

    expectRoundTrip(ParsedMessage{SM{L3RequestMBMSContextActivation{}}},
        L3PD::GPRSSessionManagement, L3RequestMBMSContextActivation::MTI, "L3RequestMBMSContextActivation");

    expectRoundTrip(ParsedMessage{SM{L3RequestMBMSContextActivationReject{}}},
        L3PD::GPRSSessionManagement, L3RequestMBMSContextActivationReject::MTI, "L3RequestMBMSContextActivationReject");

    expectRoundTrip(ParsedMessage{SM{L3RequestSecondaryPDPContextActivation{}}},
        L3PD::GPRSSessionManagement, L3RequestSecondaryPDPContextActivation::MTI, "L3RequestSecondaryPDPContextActivation");

    expectRoundTrip(ParsedMessage{SM{L3RequestSecondaryPDPContextActivationReject{}}},
        L3PD::GPRSSessionManagement, L3RequestSecondaryPDPContextActivationReject::MTI, "L3RequestSecondaryPDPContextActivationReject");

    expectRoundTrip(ParsedMessage{SM{L3SMNotification{}}},
        L3PD::GPRSSessionManagement, L3SMNotification::MTI, "L3SMNotification");
}

// SMS domain
TEST(FullRoundTrip, SMS_Domain) {
    expectRoundTrip(ParsedMessage{SMS{L3CPData{}}},
        L3PD::SMS, L3CPData::MTI, "L3CPData");

    expectRoundTrip(ParsedMessage{SMS{L3CPAck{}}},
        L3PD::SMS, L3CPAck::MTI, "L3CPAck");

    expectRoundTrip(ParsedMessage{SMS{L3CPErr{}}},
        L3PD::SMS, L3CPErr::MTI, "L3CPErr");

    expectRoundTrip(ParsedMessage{SMS{L3CPStatus{}}},
        L3PD::SMS, L3CPStatus::MTI, "L3CPStatus");

    expectRoundTrip(ParsedMessage{SMS{L3CPSMT{}}},
        L3PD::SMS, L3CPSMT::MTI, "L3CPSMT");

    expectRoundTrip(ParsedMessage{SMS{L3SMSStatusReport{}}},
        L3PD::SMS, L3SMSStatusReport::MTI, "L3SMSStatusReport");

    expectWriteOnly(ParsedMessage{SMS{L3SMSProvidedReplyExpected{}}}, "L3SMSProvidedReplyExpected");

    expectRoundTrip(ParsedMessage{SMS{L3SMSSubmitRep{}}},
        L3PD::SMS, L3SMSSubmitRep::MTI, "L3SMSSubmitRep");

    expectRoundTrip(ParsedMessage{SMS{L3SMSDeliver{}}},
        L3PD::SMS, L3SMSDeliver::MTI, "L3SMSDeliver");

    expectRoundTrip(ParsedMessage{SMS{L3SMSDeliverRep{}}},
        L3PD::SMS, L3SMSDeliverRep::MTI, "L3SMSDeliverRep");

    expectRoundTrip(ParsedMessage{SMS{L3SMSStatusReportAck{}}},
        L3PD::SMS, L3SMSStatusReportAck::MTI, "L3SMSStatusReportAck");

    expectRoundTrip(ParsedMessage{SMS{L3SMSStatusReportReject{}}},
        L3PD::SMS, L3SMSStatusReportReject::MTI, "L3SMSStatusReportReject");

    expectRoundTrip(ParsedMessage{SMS{L3SMSTSReject{}}},
        L3PD::SMS, L3SMSTSReject::MTI, "L3SMSTSReject");

    expectRoundTrip(ParsedMessage{SMS{L3SMSSubmitDeferred{}}},
        L3PD::SMS, L3SMSSubmitDeferred::MTI, "L3SMSSubmitDeferred");

    expectRoundTrip(ParsedMessage{SMS{L3SMSSubmitReject{}}},
        L3PD::SMS, L3SMSSubmitReject::MTI, "L3SMSSubmitReject");

    expectRoundTrip(ParsedMessage{SMS{L3SMSSFProvidedRep{}}},
        L3PD::SMS, L3SMSSFProvidedRep::MTI, "L3SMSSFProvidedRep");

    expectRoundTrip(ParsedMessage{SMS{L3SMSSFProvidedRepAck{}}},
        L3PD::SMS, L3SMSSFProvidedRepAck::MTI, "L3SMSSFProvidedRepAck");

    expectRoundTrip(ParsedMessage{SMS{L3SMSNotification{}}},
        L3PD::SMS, L3SMSNotification::MTI, "L3SMSNotification");

    expectRoundTrip(ParsedMessage{SMS{L3SMSShortCodeInfo{}}},
        L3PD::SMS, L3SMSShortCodeInfo::MTI, "L3SMSShortCodeInfo");
}

// BCC domain (Bearer Independent Call Control)
TEST(FullRoundTrip, BCC_Domain) {
    expectRoundTrip(ParsedMessage{BCCM{L3BCCSetup{}}},
        L3PD::BroadcastCallControl, L3BCCSetup::MTI, "L3BCCSetup");

    expectRoundTrip(ParsedMessage{BCCM{L3BCCProceeding{}}},
        L3PD::BroadcastCallControl, L3BCCProceeding::MTI, "L3BCCProceeding");

    expectRoundTrip(ParsedMessage{BCCM{L3BCCConnect{}}},
        L3PD::BroadcastCallControl, L3BCCConnect::MTI, "L3BCCConnect");

    expectRoundTrip(ParsedMessage{BCCM{L3BCCDisconnect{}}},
        L3PD::BroadcastCallControl, L3BCCDisconnect::MTI, "L3BCCDisconnect");

    expectRoundTrip(ParsedMessage{BCCM{L3BCCRelease{}}},
        L3PD::BroadcastCallControl, L3BCCRelease::MTI, "L3BCCRelease");

    expectRoundTrip(ParsedMessage{BCCM{L3BCCReleaseComplete{}}},
        L3PD::BroadcastCallControl, L3BCCReleaseComplete::MTI, "L3BCCReleaseComplete");

    expectRoundTrip(ParsedMessage{BCCM{L3BCCCallConfirmed{}}},
        L3PD::BroadcastCallControl, L3BCCCallConfirmed::MTI, "L3BCCCallConfirmed");

    expectRoundTrip(ParsedMessage{BCCM{L3BCCConnectAcknowledge{}}},
        L3PD::BroadcastCallControl, L3BCCConnectAcknowledge::MTI, "L3BCCConnectAcknowledge");
}

// GCC domain (Group Call Control)
TEST(FullRoundTrip, GCC_Domain) {
    expectRoundTrip(ParsedMessage{GCCM{L3GCCSetup{}}},
        L3PD::GroupCallControl, L3GCCSetup::MTI, "L3GCCSetup");

    expectRoundTrip(ParsedMessage{GCCM{L3GCCAcknowledge{}}},
        L3PD::GroupCallControl, L3GCCAcknowledge::MTI, "L3GCCAcknowledge");

    expectRoundTrip(ParsedMessage{GCCM{L3GCCProceeding{}}},
        L3PD::GroupCallControl, L3GCCProceeding::MTI, "L3GCCProceeding");

    expectRoundTrip(ParsedMessage{GCCM{L3GCCConnect{}}},
        L3PD::GroupCallControl, L3GCCConnect::MTI, "L3GCCConnect");

    expectRoundTrip(ParsedMessage{GCCM{L3GCCDisconnect{}}},
        L3PD::GroupCallControl, L3GCCDisconnect::MTI, "L3GCCDisconnect");

    expectRoundTrip(ParsedMessage{GCCM{L3GCCRelease{}}},
        L3PD::GroupCallControl, L3GCCRelease::MTI, "L3GCCRelease");

    expectRoundTrip(ParsedMessage{GCCM{L3GCCReleaseComplete{}}},
        L3PD::GroupCallControl, L3GCCReleaseComplete::MTI, "L3GCCReleaseComplete");

    expectRoundTrip(ParsedMessage{GCCM{L3GCCCallConfirmed{}}},
        L3PD::GroupCallControl, L3GCCCallConfirmed::MTI, "L3GCCCallConfirmed");
}

// LS domain (Location Services)
TEST(FullRoundTrip, LS_Domain) {
    expectRoundTrip(ParsedMessage{LSM{L3LocationServiceRequest{}}},
        L3PD::Location, L3LocationServiceRequest::MTI, "L3LocationServiceRequest");

    expectRoundTrip(ParsedMessage{LSM{L3LocationServiceProviderMessage{}}},
        L3PD::Location, L3LocationServiceProviderMessage::MTI, "L3LocationServiceProviderMessage");
}

// Extended and TestProc domains
TEST(FullRoundTrip, Extended_Domain) {
    expectWriteOnly(ParsedMessage{EXTENDED{L3ExtendedMessage{}}}, "L3ExtendedMessage");
}

TEST(FullRoundTrip, TestProc_Domain) {
    expectWriteOnly(ParsedMessage{TESTPROC{L3TestProcedureMessage{}}}, "L3TestProcedureMessage");
}
