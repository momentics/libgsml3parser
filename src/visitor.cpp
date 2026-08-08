#include "gsml3parser/visitor.h"
#include <sstream>

namespace gsml3parser {

namespace {

struct NameVisitor {
    std::string_view operator()(const L3PagingRequestType1&) const { return "PagingRequestType1"; }
    std::string_view operator()(const L3PagingRequestType2&) const { return "PagingRequestType2"; }
    std::string_view operator()(const L3PagingRequestType3&) const { return "PagingRequestType3"; }
    std::string_view operator()(const L3PagingResponse&) const { return "PagingResponse"; }
    std::string_view operator()(const L3ClassmarkChange&) const { return "ClassmarkChange"; }
    std::string_view operator()(const L3ClassmarkEnquiry&) const { return "ClassmarkEnquiry"; }
    std::string_view operator()(const L3MeasurementReport&) const { return "MeasurementReport"; }
    std::string_view operator()(const L3ChannelRelease&) const { return "ChannelRelease"; }
    std::string_view operator()(const L3RRStatus&) const { return "RRStatus"; }
    std::string_view operator()(const L3AssignmentCommand&) const { return "AssignmentCommand"; }
    std::string_view operator()(const L3AssignmentComplete&) const { return "AssignmentComplete"; }
    std::string_view operator()(const L3AssignmentFailure&) const { return "AssignmentFailure"; }
    std::string_view operator()(const L3ImmediateAssignment&) const { return "ImmediateAssignment"; }
    std::string_view operator()(const L3ImmediateAssignmentExtended&) const { return "ImmediateAssignmentExtended"; }
    std::string_view operator()(const L3ImmediateAssignmentReject&) const { return "ImmediateAssignmentReject"; }
    std::string_view operator()(const L3AdditionalAssignment&) const { return "AdditionalAssignment"; }
    std::string_view operator()(const L3HandoverCommand&) const { return "HandoverCommand"; }
    std::string_view operator()(const L3HandoverComplete&) const { return "HandoverComplete"; }
    std::string_view operator()(const L3HandoverFailure&) const { return "HandoverFailure"; }
    std::string_view operator()(const L3PhysicalInformation&) const { return "PhysicalInformation"; }
    std::string_view operator()(const L3CipheringModeCommand&) const { return "CipheringModeCommand"; }
    std::string_view operator()(const L3CipheringModeComplete&) const { return "CipheringModeComplete"; }
    std::string_view operator()(const L3ChannelModeModify&) const { return "ChannelModeModify"; }
    std::string_view operator()(const L3ChannelModeModifyAcknowledge&) const { return "ChannelModeModifyAcknowledge"; }
    std::string_view operator()(const L3GPRSSuspensionRequest&) const { return "GPRSSuspensionRequest"; }
    std::string_view operator()(const L3ApplicationInformation&) const { return "ApplicationInformation"; }
    std::string_view operator()(const L3SynchronizationChannelInformation&) const { return "SynchronizationChannelInformation"; }
    std::string_view operator()(const L3ChannelRequest&) const { return "ChannelRequest"; }
    std::string_view operator()(const L3HandoverAccess&) const { return "HandoverAccess"; }
    std::string_view operator()(const L3SystemInformationType1&) const { return "SystemInformationType1"; }
    std::string_view operator()(const L3SystemInformationType2&) const { return "SystemInformationType2"; }
    std::string_view operator()(const L3SystemInformationType2bis&) const { return "SystemInformationType2bis"; }
    std::string_view operator()(const L3SystemInformationType2ter&) const { return "SystemInformationType2ter"; }
    std::string_view operator()(const L3SystemInformationType3&) const { return "SystemInformationType3"; }
    std::string_view operator()(const L3SystemInformationType4&) const { return "SystemInformationType4"; }
    std::string_view operator()(const L3SystemInformationType5&) const { return "SystemInformationType5"; }
    std::string_view operator()(const L3SystemInformationType5bis&) const { return "SystemInformationType5bis"; }
    std::string_view operator()(const L3SystemInformationType5ter&) const { return "SystemInformationType5ter"; }
    std::string_view operator()(const L3SystemInformationType6&) const { return "SystemInformationType6"; }
    std::string_view operator()(const L3SystemInformationType7&) const { return "SystemInformationType7"; }
    std::string_view operator()(const L3SystemInformationType8&) const { return "SystemInformationType8"; }
    std::string_view operator()(const L3SystemInformationType9&) const { return "SystemInformationType9"; }
    std::string_view operator()(const L3SystemInformationType13&) const { return "SystemInformationType13"; }
    std::string_view operator()(const L3SystemInformationType16&) const { return "SystemInformationType16"; }
    std::string_view operator()(const L3SystemInformationType17&) const { return "SystemInformationType17"; }

    // MM names
    std::string_view operator()(const L3IMSIDetachIndication&) const { return "IMSIDetachIndication"; }
    std::string_view operator()(const L3CMServiceAccept&) const { return "CMServiceAccept"; }
    std::string_view operator()(const L3CMServiceReject&) const { return "CMServiceReject"; }
    std::string_view operator()(const L3CMServiceAbort&) const { return "CMServiceAbort"; }
    std::string_view operator()(const L3CMServiceRequest&) const { return "CMServiceRequest"; }
    std::string_view operator()(const L3CMReestablishmentRequest&) const { return "CMReestablishmentRequest"; }
    std::string_view operator()(const L3IdentityResponse&) const { return "IdentityResponse"; }
    std::string_view operator()(const L3IdentityRequest&) const { return "IdentityRequest"; }
    std::string_view operator()(const L3MMInformation&) const { return "MMInformation"; }
    std::string_view operator()(const L3LocationUpdatingAccept&) const { return "LocationUpdatingAccept"; }
    std::string_view operator()(const L3LocationUpdatingReject&) const { return "LocationUpdatingReject"; }
    std::string_view operator()(const L3LocationUpdatingRequest&) const { return "LocationUpdatingRequest"; }
    std::string_view operator()(const L3TMSIReallocationCommand&) const { return "TMSIReallocationCommand"; }
    std::string_view operator()(const L3TMSIReallocationComplete&) const { return "TMSIReallocationComplete"; }
    std::string_view operator()(const L3MMStatus&) const { return "MMStatus"; }
    std::string_view operator()(const L3AuthenticationRequest&) const { return "AuthenticationRequest"; }
    std::string_view operator()(const L3AuthenticationResponse&) const { return "AuthenticationResponse"; }
    std::string_view operator()(const L3AuthenticationReject&) const { return "AuthenticationReject"; }

    // CC names
    std::string_view operator()(const L3Setup&) const { return "Setup"; }
    std::string_view operator()(const L3EmergencySetup&) const { return "EmergencySetup"; }
    std::string_view operator()(const L3CallProceeding&) const { return "CallProceeding"; }
    std::string_view operator()(const L3Alerting&) const { return "Alerting"; }
    std::string_view operator()(const L3Connect&) const { return "Connect"; }
    std::string_view operator()(const L3ConnectAcknowledge&) const { return "ConnectAcknowledge"; }
    std::string_view operator()(const L3CallConfirmed&) const { return "CallConfirmed"; }
    std::string_view operator()(const L3Disconnect&) const { return "Disconnect"; }
    std::string_view operator()(const L3Release&) const { return "Release"; }
    std::string_view operator()(const L3ReleaseComplete&) const { return "ReleaseComplete"; }
    std::string_view operator()(const L3StartDTMF&) const { return "StartDTMF"; }
    std::string_view operator()(const L3StopDTMF&) const { return "StopDTMF"; }
    std::string_view operator()(const L3StopDTMFAcknowledge&) const { return "StopDTMFAcknowledge"; }
    std::string_view operator()(const L3StartDTMFAcknowledge&) const { return "StartDTMFAcknowledge"; }
    std::string_view operator()(const L3StartDTMFReject&) const { return "StartDTMFReject"; }
    std::string_view operator()(const L3Hold&) const { return "Hold"; }
    std::string_view operator()(const L3HoldReject&) const { return "HoldReject"; }
    std::string_view operator()(const L3CCStatus&) const { return "CCStatus"; }
    std::string_view operator()(const L3Progress&) const { return "Progress"; }

    // SS names
    std::string_view operator()(const L3SupServFacilityMessage&) const { return "SupServFacilityMessage"; }
    std::string_view operator()(const L3SupServRegisterMessage&) const { return "SupServRegisterMessage"; }
    std::string_view operator()(const L3SupServReleaseCompleteMessage&) const { return "SupServReleaseCompleteMessage"; }
};

struct PDVisitor {
    L3PD operator()(const RRM&) const { return L3PD::RadioResource; }
    L3PD operator()(const MMM&) const { return L3PD::MobilityManagement; }
    L3PD operator()(const CCM&) const { return L3PD::CallControl; }
    L3PD operator()(const SSM&) const { return L3PD::NonCallSS; }
};

struct MTIVisitor {
    int operator()(const RRM& v) const { return std::visit(*this, v); }
    int operator()(const MMM& v) const { return std::visit(*this, v); }
    int operator()(const CCM& v) const { return std::visit(*this, v); }
    int operator()(const SSM& v) const { return std::visit(*this, v); }

    int operator()(const L3PagingRequestType1&) const { return L3PagingRequestType1::MTI; }
    int operator()(const L3PagingRequestType2&) const { return L3PagingRequestType2::MTI; }
    int operator()(const L3PagingRequestType3&) const { return L3PagingRequestType3::MTI; }
    int operator()(const L3PagingResponse&) const { return L3PagingResponse::MTI; }
    int operator()(const L3ClassmarkChange&) const { return L3ClassmarkChange::MTI; }
    int operator()(const L3ClassmarkEnquiry&) const { return L3ClassmarkEnquiry::MTI; }
    int operator()(const L3MeasurementReport&) const { return L3MeasurementReport::MTI; }
    int operator()(const L3ChannelRelease&) const { return L3ChannelRelease::MTI; }
    int operator()(const L3RRStatus&) const { return L3RRStatus::MTI; }
    int operator()(const L3AssignmentCommand&) const { return L3AssignmentCommand::MTI; }
    int operator()(const L3AssignmentComplete&) const { return L3AssignmentComplete::MTI; }
    int operator()(const L3AssignmentFailure&) const { return L3AssignmentFailure::MTI; }
    int operator()(const L3ImmediateAssignment&) const { return L3ImmediateAssignment::MTI; }
    int operator()(const L3ImmediateAssignmentExtended&) const { return L3ImmediateAssignmentExtended::MTI; }
    int operator()(const L3ImmediateAssignmentReject&) const { return L3ImmediateAssignmentReject::MTI; }
    int operator()(const L3AdditionalAssignment&) const { return L3AdditionalAssignment::MTI; }
    int operator()(const L3HandoverCommand&) const { return L3HandoverCommand::MTI; }
    int operator()(const L3HandoverComplete&) const { return L3HandoverComplete::MTI; }
    int operator()(const L3HandoverFailure&) const { return L3HandoverFailure::MTI; }
    int operator()(const L3PhysicalInformation&) const { return L3PhysicalInformation::MTI; }
    int operator()(const L3CipheringModeCommand&) const { return L3CipheringModeCommand::MTI; }
    int operator()(const L3CipheringModeComplete&) const { return L3CipheringModeComplete::MTI; }
    int operator()(const L3ChannelModeModify&) const { return L3ChannelModeModify::MTI; }
    int operator()(const L3ChannelModeModifyAcknowledge&) const { return L3ChannelModeModifyAcknowledge::MTI; }
    int operator()(const L3GPRSSuspensionRequest&) const { return L3GPRSSuspensionRequest::MTI; }
    int operator()(const L3ApplicationInformation&) const { return L3ApplicationInformation::MTI; }
    int operator()(const L3SynchronizationChannelInformation&) const { return L3SynchronizationChannelInformation::MTI; }
    int operator()(const L3ChannelRequest&) const { return L3ChannelRequest::MTI; }
    int operator()(const L3HandoverAccess&) const { return L3HandoverAccess::MTI; }
    int operator()(const L3SystemInformationType1&) const { return L3SystemInformationType1::MTI; }
    int operator()(const L3SystemInformationType2&) const { return L3SystemInformationType2::MTI; }
    int operator()(const L3SystemInformationType2bis&) const { return L3SystemInformationType2bis::MTI; }
    int operator()(const L3SystemInformationType2ter&) const { return L3SystemInformationType2ter::MTI; }
    int operator()(const L3SystemInformationType3&) const { return L3SystemInformationType3::MTI; }
    int operator()(const L3SystemInformationType4&) const { return L3SystemInformationType4::MTI; }
    int operator()(const L3SystemInformationType5&) const { return L3SystemInformationType5::MTI; }
    int operator()(const L3SystemInformationType5bis&) const { return L3SystemInformationType5bis::MTI; }
    int operator()(const L3SystemInformationType5ter&) const { return L3SystemInformationType5ter::MTI; }
    int operator()(const L3SystemInformationType6&) const { return L3SystemInformationType6::MTI; }
    int operator()(const L3SystemInformationType7&) const { return L3SystemInformationType7::MTI; }
    int operator()(const L3SystemInformationType8&) const { return L3SystemInformationType8::MTI; }
    int operator()(const L3SystemInformationType9&) const { return L3SystemInformationType9::MTI; }
    int operator()(const L3SystemInformationType13&) const { return L3SystemInformationType13::MTI; }
    int operator()(const L3SystemInformationType16&) const { return L3SystemInformationType16::MTI; }
    int operator()(const L3SystemInformationType17&) const { return L3SystemInformationType17::MTI; }

    int operator()(const L3IMSIDetachIndication&) const { return L3IMSIDetachIndication::MTI; }
    int operator()(const L3CMServiceAccept&) const { return L3CMServiceAccept::MTI; }
    int operator()(const L3CMServiceReject&) const { return L3CMServiceReject::MTI; }
    int operator()(const L3CMServiceAbort&) const { return L3CMServiceAbort::MTI; }
    int operator()(const L3CMServiceRequest&) const { return L3CMServiceRequest::MTI; }
    int operator()(const L3CMReestablishmentRequest&) const { return L3CMReestablishmentRequest::MTI; }
    int operator()(const L3IdentityResponse&) const { return L3IdentityResponse::MTI; }
    int operator()(const L3IdentityRequest&) const { return L3IdentityRequest::MTI; }
    int operator()(const L3MMInformation&) const { return L3MMInformation::MTI; }
    int operator()(const L3LocationUpdatingAccept&) const { return L3LocationUpdatingAccept::MTI; }
    int operator()(const L3LocationUpdatingReject&) const { return L3LocationUpdatingReject::MTI; }
    int operator()(const L3LocationUpdatingRequest&) const { return L3LocationUpdatingRequest::MTI; }
    int operator()(const L3TMSIReallocationCommand&) const { return L3TMSIReallocationCommand::MTI; }
    int operator()(const L3TMSIReallocationComplete&) const { return L3TMSIReallocationComplete::MTI; }
    int operator()(const L3MMStatus&) const { return L3MMStatus::MTI; }
    int operator()(const L3AuthenticationRequest&) const { return L3AuthenticationRequest::MTI; }
    int operator()(const L3AuthenticationResponse&) const { return L3AuthenticationResponse::MTI; }
    int operator()(const L3AuthenticationReject&) const { return L3AuthenticationReject::MTI; }

    int operator()(const L3Setup&) const { return L3Setup::MTI; }
    int operator()(const L3EmergencySetup&) const { return L3EmergencySetup::MTI; }
    int operator()(const L3CallProceeding&) const { return L3CallProceeding::MTI; }
    int operator()(const L3Alerting&) const { return L3Alerting::MTI; }
    int operator()(const L3Connect&) const { return L3Connect::MTI; }
    int operator()(const L3ConnectAcknowledge&) const { return L3ConnectAcknowledge::MTI; }
    int operator()(const L3CallConfirmed&) const { return L3CallConfirmed::MTI; }
    int operator()(const L3Disconnect&) const { return L3Disconnect::MTI; }
    int operator()(const L3Release&) const { return L3Release::MTI; }
    int operator()(const L3ReleaseComplete&) const { return L3ReleaseComplete::MTI; }
    int operator()(const L3StartDTMF&) const { return L3StartDTMF::MTI; }
    int operator()(const L3StopDTMF&) const { return L3StopDTMF::MTI; }
    int operator()(const L3StopDTMFAcknowledge&) const { return L3StopDTMFAcknowledge::MTI; }
    int operator()(const L3StartDTMFAcknowledge&) const { return L3StartDTMFAcknowledge::MTI; }
    int operator()(const L3StartDTMFReject&) const { return L3StartDTMFReject::MTI; }
    int operator()(const L3Hold&) const { return L3Hold::MTI; }
    int operator()(const L3HoldReject&) const { return L3HoldReject::MTI; }
    int operator()(const L3CCStatus&) const { return L3CCStatus::MTI; }
    int operator()(const L3Progress&) const { return L3Progress::MTI; }

    int operator()(const L3SupServFacilityMessage&) const { return L3SupServFacilityMessage::MTI; }
    int operator()(const L3SupServRegisterMessage&) const { return L3SupServRegisterMessage::MTI; }
    int operator()(const L3SupServReleaseCompleteMessage&) const { return L3SupServReleaseCompleteMessage::MTI; }
};

} // anonymous namespace

std::string_view messageName(const ParsedMessage& msg) {
    return std::visit([](const auto& domain) -> std::string_view {
        return std::visit(NameVisitor{}, domain);
    }, msg);
}

L3PD messagePD(const ParsedMessage& msg) {
    return std::visit(PDVisitor{}, msg);
}

int messageMTI(const ParsedMessage& msg) {
    return std::visit(MTIVisitor{}, msg);
}

} // namespace gsml3parser
