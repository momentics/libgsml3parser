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
    std::string_view operator()(const L3ConfigurationChangeCommand&) const { return "ConfigurationChangeCommand"; }
    std::string_view operator()(const L3ConfigurationChangeAcknowledge&) const { return "ConfigurationChangeAcknowledge"; }
    std::string_view operator()(const L3ConfigurationChangeReject&) const { return "ConfigurationChangeReject"; }
    std::string_view operator()(const L3PartialRelease&) const { return "PartialRelease"; }
    std::string_view operator()(const L3PartialReleaseComplete&) const { return "PartialReleaseComplete"; }
    std::string_view operator()(const L3ExtendedMeasurementReport&) const { return "ExtendedMeasurementReport"; }
    std::string_view operator()(const L3ExtendedMeasurementOrder&) const { return "ExtendedMeasurementOrder"; }
    std::string_view operator()(const L3FrequencyRedefinition&) const { return "FrequencyRedefinition"; }
    std::string_view operator()(const L3NotificationNCH&) const { return "NotificationNCH"; }
    std::string_view operator()(const L3NotificationResponse&) const { return "NotificationResponse"; }
    std::string_view operator()(const L3VGCSUplinkGrant&) const { return "VGCSUplinkGrant"; }
    std::string_view operator()(const L3UplinkRelease&) const { return "UplinkRelease"; }
    std::string_view operator()(const L3UplinkBusy&) const { return "UplinkBusy"; }
    std::string_view operator()(const L3TalkerIndication&) const { return "TalkerIndication"; }
    std::string_view operator()(const L3PriorityUplinkRequest&) const { return "PriorityUplinkRequest"; }
    std::string_view operator()(const L3DataIndication&) const { return "DataIndication"; }
    std::string_view operator()(const L3DataIndication2&) const { return "DataIndication2"; }
    std::string_view operator()(const L3DTMAssignmentFailure&) const { return "DTMAssignmentFailure"; }
    std::string_view operator()(const L3DTMReject&) const { return "DTMReject"; }
    std::string_view operator()(const L3DTMRequest&) const { return "DTMRequest"; }
    std::string_view operator()(const L3PacketAssignment&) const { return "PacketAssignment"; }
    std::string_view operator()(const L3DTMAssignmentCommand&) const { return "DTMAssignmentCommand"; }
    std::string_view operator()(const L3DTMInformation&) const { return "DTMInformation"; }
    std::string_view operator()(const L3PacketInformation&) const { return "PacketInformation"; }
    std::string_view operator()(const L3UTRANClassmarkChange&) const { return "UTRANClassmarkChange"; }
    std::string_view operator()(const L3CDMA2000ClassmarkChange&) const { return "CDMA2000ClassmarkChange"; }
    std::string_view operator()(const L3IntersysToUTRANHOCommand&) const { return "IntersysToUTRANHOCommand"; }
    std::string_view operator()(const L3IntersysToCDMA2000HOCommand&) const { return "IntersysToCDMA2000HOCommand"; }
    std::string_view operator()(const L3GERANIUClassmarkChange&) const { return "GERANIUClassmarkChange"; }
    std::string_view operator()(const L3SystemInformationType14&) const { return "SystemInformationType14"; }
    std::string_view operator()(const L3SystemInformationType15&) const { return "SystemInformationType15"; }
    std::string_view operator()(const L3SystemInformationType18&) const { return "SystemInformationType18"; }
    std::string_view operator()(const L3SystemInformationType19&) const { return "SystemInformationType19"; }
    std::string_view operator()(const L3SystemInformationType20&) const { return "SystemInformationType20"; }
    std::string_view operator()(const L3SystemInformationType13alt&) const { return "SystemInformationType13alt"; }
    std::string_view operator()(const L3SystemInformationType2n&) const { return "SystemInformationType2n"; }
    std::string_view operator()(const L3SystemInformationType21&) const { return "SystemInformationType21"; }
    std::string_view operator()(const L3SystemInformationType22&) const { return "SystemInformationType22"; }
    std::string_view operator()(const L3SystemInformationType23&) const { return "SystemInformationType23"; }
    std::string_view operator()(const L3SystemInformationType10&) const { return "SystemInformationType10"; }
    std::string_view operator()(const L3SystemInformationType10bis&) const { return "SystemInformationType10bis"; }
    std::string_view operator()(const L3SystemInformationType10ter&) const { return "SystemInformationType10ter"; }
    std::string_view operator()(const L3NotificationFACCH&) const { return "NotificationFACCH"; }
    std::string_view operator()(const L3UplinkFree&) const { return "UplinkFree"; }
    std::string_view operator()(const L3EnhancedMeasurementRepUL&) const { return "EnhancedMeasurementRepUL"; }
    std::string_view operator()(const L3MeasurementInfoDL&) const { return "MeasurementInfoDL"; }
    std::string_view operator()(const L3VBSVGCSRecon&) const { return "VBSVGCSRecon"; }
    std::string_view operator()(const L3VBSVGCSRecon2&) const { return "VBSVGCSRecon2"; }
    std::string_view operator()(const L3VGCSAddInfo&) const { return "VGCSAddInfo"; }
    std::string_view operator()(const L3VGCSMSInfo&) const { return "VGCSMSInfo"; }
    std::string_view operator()(const L3VGCSSNeighCellInfo&) const { return "VGCSSNeighCellInfo"; }
    std::string_view operator()(const L3NotifyAppData&) const { return "NotifyAppData"; }

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

    // GMM names
    std::string_view operator()(const L3AttachRequest&) const { return "AttachRequest"; }
    std::string_view operator()(const L3AttachAccept&) const { return "AttachAccept"; }
    std::string_view operator()(const L3AttachComplete&) const { return "AttachComplete"; }
    std::string_view operator()(const L3AttachReject&) const { return "AttachReject"; }
    std::string_view operator()(const L3DetachRequest&) const { return "DetachRequest"; }
    std::string_view operator()(const L3DetachAccept&) const { return "DetachAccept"; }
    std::string_view operator()(const L3RoutingAreaUpdateRequest&) const { return "RoutingAreaUpdateRequest"; }
    std::string_view operator()(const L3RoutingAreaUpdateAccept&) const { return "RoutingAreaUpdateAccept"; }
    std::string_view operator()(const L3RoutingAreaUpdateComplete&) const { return "RoutingAreaUpdateComplete"; }
    std::string_view operator()(const L3RoutingAreaUpdateReject&) const { return "RoutingAreaUpdateReject"; }
    std::string_view operator()(const L3ServiceRequest&) const { return "ServiceRequest"; }
    std::string_view operator()(const L3ServiceAccept&) const { return "ServiceAccept"; }
    std::string_view operator()(const L3ServiceReject&) const { return "ServiceReject"; }
    std::string_view operator()(const L3P_TMSIReallocationCommand&) const { return "P_TMSIReallocationCommand"; }
    std::string_view operator()(const L3P_TMSIReallocationComplete&) const { return "P_TMSIReallocationComplete"; }
    std::string_view operator()(const L3AuthenticationAndCipheringRequest&) const { return "AuthAndCipheringRequest"; }
    std::string_view operator()(const L3AuthenticationAndCipheringResponse&) const { return "AuthAndCipheringResponse"; }
    std::string_view operator()(const L3AuthenticationAndCipheringReject&) const { return "AuthAndCipheringReject"; }
    std::string_view operator()(const L3GMMIdentityRequest&) const { return "GMMIdentityRequest"; }
    std::string_view operator()(const L3GMMIdentityResponse&) const { return "GMMIdentityResponse"; }
    std::string_view operator()(const L3AuthenticationAndCipheringFailure&) const { return "AuthAndCipheringFailure"; }
    std::string_view operator()(const L3GMMStatus&) const { return "GMMStatus"; }
    std::string_view operator()(const L3GMMInformation&) const { return "GMMInformation"; }

    // SM names
    std::string_view operator()(const L3ActivatePDPContextRequest&) const { return "ActivatePDPContextRequest"; }
    std::string_view operator()(const L3ActivatePDPContextAccept&) const { return "ActivatePDPContextAccept"; }
    std::string_view operator()(const L3ActivatePDPContextReject&) const { return "ActivatePDPContextReject"; }
    std::string_view operator()(const L3DeactivatePDPContextRequest&) const { return "DeactivatePDPContextRequest"; }
    std::string_view operator()(const L3DeactivatePDPContextAccept&) const { return "DeactivatePDPContextAccept"; }
    std::string_view operator()(const L3ModifyPDPContextRequest&) const { return "ModifyPDPContextRequest"; }
    std::string_view operator()(const L3ModifyPDPContextAccept&) const { return "ModifyPDPContextAccept"; }
    std::string_view operator()(const L3ModifyPDPContextReject&) const { return "ModifyPDPContextReject"; }
    std::string_view operator()(const L3SMStatus&) const { return "SMStatus"; }
};

struct PDVisitor {
    L3PD operator()(const RRM&) const { return L3PD::RadioResource; }
    L3PD operator()(const MMM&) const { return L3PD::MobilityManagement; }
    L3PD operator()(const CCM&) const { return L3PD::CallControl; }
    L3PD operator()(const SSM&) const { return L3PD::NonCallSS; }
    L3PD operator()(const GMM&) const { return L3PD::GPRSMobilityManagement; }
    L3PD operator()(const SM&) const { return L3PD::GPRSSessionManagement; }
};

struct MTIVisitor {
    int operator()(const RRM& v) const { return std::visit(*this, v); }
    int operator()(const MMM& v) const { return std::visit(*this, v); }
    int operator()(const CCM& v) const { return std::visit(*this, v); }
    int operator()(const SSM& v) const { return std::visit(*this, v); }
    int operator()(const GMM& v) const { return std::visit(*this, v); }
    int operator()(const SM& v) const { return std::visit(*this, v); }

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
    int operator()(const L3ConfigurationChangeCommand&) const { return L3ConfigurationChangeCommand::MTI; }
    int operator()(const L3ConfigurationChangeAcknowledge&) const { return L3ConfigurationChangeAcknowledge::MTI; }
    int operator()(const L3ConfigurationChangeReject&) const { return L3ConfigurationChangeReject::MTI; }
    int operator()(const L3PartialRelease&) const { return L3PartialRelease::MTI; }
    int operator()(const L3PartialReleaseComplete&) const { return L3PartialReleaseComplete::MTI; }
    int operator()(const L3ExtendedMeasurementReport&) const { return L3ExtendedMeasurementReport::MTI; }
    int operator()(const L3ExtendedMeasurementOrder&) const { return L3ExtendedMeasurementOrder::MTI; }
    int operator()(const L3FrequencyRedefinition&) const { return L3FrequencyRedefinition::MTI; }
    int operator()(const L3NotificationNCH&) const { return L3NotificationNCH::MTI; }
    int operator()(const L3NotificationResponse&) const { return L3NotificationResponse::MTI; }
    int operator()(const L3VGCSUplinkGrant&) const { return L3VGCSUplinkGrant::MTI; }
    int operator()(const L3UplinkRelease&) const { return L3UplinkRelease::MTI; }
    int operator()(const L3UplinkBusy&) const { return L3UplinkBusy::MTI; }
    int operator()(const L3TalkerIndication&) const { return L3TalkerIndication::MTI; }
    int operator()(const L3PriorityUplinkRequest&) const { return L3PriorityUplinkRequest::MTI; }
    int operator()(const L3DataIndication&) const { return L3DataIndication::MTI; }
    int operator()(const L3DataIndication2&) const { return L3DataIndication2::MTI; }
    int operator()(const L3DTMAssignmentFailure&) const { return L3DTMAssignmentFailure::MTI; }
    int operator()(const L3DTMReject&) const { return L3DTMReject::MTI; }
    int operator()(const L3DTMRequest&) const { return L3DTMRequest::MTI; }
    int operator()(const L3PacketAssignment&) const { return L3PacketAssignment::MTI; }
    int operator()(const L3DTMAssignmentCommand&) const { return L3DTMAssignmentCommand::MTI; }
    int operator()(const L3DTMInformation&) const { return L3DTMInformation::MTI; }
    int operator()(const L3PacketInformation&) const { return L3PacketInformation::MTI; }
    int operator()(const L3UTRANClassmarkChange&) const { return L3UTRANClassmarkChange::MTI; }
    int operator()(const L3CDMA2000ClassmarkChange&) const { return L3CDMA2000ClassmarkChange::MTI; }
    int operator()(const L3IntersysToUTRANHOCommand&) const { return L3IntersysToUTRANHOCommand::MTI; }
    int operator()(const L3IntersysToCDMA2000HOCommand&) const { return L3IntersysToCDMA2000HOCommand::MTI; }
    int operator()(const L3GERANIUClassmarkChange&) const { return L3GERANIUClassmarkChange::MTI; }
    int operator()(const L3SystemInformationType14&) const { return L3SystemInformationType14::MTI; }
    int operator()(const L3SystemInformationType15&) const { return L3SystemInformationType15::MTI; }
    int operator()(const L3SystemInformationType18&) const { return L3SystemInformationType18::MTI; }
    int operator()(const L3SystemInformationType19&) const { return L3SystemInformationType19::MTI; }
    int operator()(const L3SystemInformationType20&) const { return L3SystemInformationType20::MTI; }
    int operator()(const L3SystemInformationType13alt&) const { return L3SystemInformationType13alt::MTI; }
    int operator()(const L3SystemInformationType2n&) const { return L3SystemInformationType2n::MTI; }
    int operator()(const L3SystemInformationType21&) const { return L3SystemInformationType21::MTI; }
    int operator()(const L3SystemInformationType22&) const { return L3SystemInformationType22::MTI; }
    int operator()(const L3SystemInformationType23&) const { return L3SystemInformationType23::MTI; }
    int operator()(const L3SystemInformationType10&) const { return L3SystemInformationType10::MTI; }
    int operator()(const L3SystemInformationType10bis&) const { return L3SystemInformationType10bis::MTI; }
    int operator()(const L3SystemInformationType10ter&) const { return L3SystemInformationType10ter::MTI; }
    int operator()(const L3NotificationFACCH&) const { return L3NotificationFACCH::MTI; }
    int operator()(const L3UplinkFree&) const { return L3UplinkFree::MTI; }
    int operator()(const L3EnhancedMeasurementRepUL&) const { return L3EnhancedMeasurementRepUL::MTI; }
    int operator()(const L3MeasurementInfoDL&) const { return L3MeasurementInfoDL::MTI; }
    int operator()(const L3VBSVGCSRecon&) const { return L3VBSVGCSRecon::MTI; }
    int operator()(const L3VBSVGCSRecon2&) const { return L3VBSVGCSRecon2::MTI; }
    int operator()(const L3VGCSAddInfo&) const { return L3VGCSAddInfo::MTI; }
    int operator()(const L3VGCSMSInfo&) const { return L3VGCSMSInfo::MTI; }
    int operator()(const L3VGCSSNeighCellInfo&) const { return L3VGCSSNeighCellInfo::MTI; }
    int operator()(const L3NotifyAppData&) const { return L3NotifyAppData::MTI; }

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

    // GMM MTI values
    int operator()(const L3AttachRequest&) const { return L3AttachRequest::MTI; }
    int operator()(const L3AttachAccept&) const { return L3AttachAccept::MTI; }
    int operator()(const L3AttachComplete&) const { return L3AttachComplete::MTI; }
    int operator()(const L3AttachReject&) const { return L3AttachReject::MTI; }
    int operator()(const L3DetachRequest&) const { return L3DetachRequest::MTI; }
    int operator()(const L3DetachAccept&) const { return L3DetachAccept::MTI; }
    int operator()(const L3RoutingAreaUpdateRequest&) const { return L3RoutingAreaUpdateRequest::MTI; }
    int operator()(const L3RoutingAreaUpdateAccept&) const { return L3RoutingAreaUpdateAccept::MTI; }
    int operator()(const L3RoutingAreaUpdateComplete&) const { return L3RoutingAreaUpdateComplete::MTI; }
    int operator()(const L3RoutingAreaUpdateReject&) const { return L3RoutingAreaUpdateReject::MTI; }
    int operator()(const L3ServiceRequest&) const { return L3ServiceRequest::MTI; }
    int operator()(const L3ServiceAccept&) const { return L3ServiceAccept::MTI; }
    int operator()(const L3ServiceReject&) const { return L3ServiceReject::MTI; }
    int operator()(const L3P_TMSIReallocationCommand&) const { return L3P_TMSIReallocationCommand::MTI; }
    int operator()(const L3P_TMSIReallocationComplete&) const { return L3P_TMSIReallocationComplete::MTI; }
    int operator()(const L3AuthenticationAndCipheringRequest&) const { return L3AuthenticationAndCipheringRequest::MTI; }
    int operator()(const L3AuthenticationAndCipheringResponse&) const { return L3AuthenticationAndCipheringResponse::MTI; }
    int operator()(const L3AuthenticationAndCipheringReject&) const { return L3AuthenticationAndCipheringReject::MTI; }
    int operator()(const L3GMMIdentityRequest&) const { return L3GMMIdentityRequest::MTI; }
    int operator()(const L3GMMIdentityResponse&) const { return L3GMMIdentityResponse::MTI; }
    int operator()(const L3AuthenticationAndCipheringFailure&) const { return L3AuthenticationAndCipheringFailure::MTI; }
    int operator()(const L3GMMStatus&) const { return L3GMMStatus::MTI; }
    int operator()(const L3GMMInformation&) const { return L3GMMInformation::MTI; }

    // SM MTI values
    int operator()(const L3ActivatePDPContextRequest&) const { return L3ActivatePDPContextRequest::MTI; }
    int operator()(const L3ActivatePDPContextAccept&) const { return L3ActivatePDPContextAccept::MTI; }
    int operator()(const L3ActivatePDPContextReject&) const { return L3ActivatePDPContextReject::MTI; }
    int operator()(const L3DeactivatePDPContextRequest&) const { return L3DeactivatePDPContextRequest::MTI; }
    int operator()(const L3DeactivatePDPContextAccept&) const { return L3DeactivatePDPContextAccept::MTI; }
    int operator()(const L3ModifyPDPContextRequest&) const { return L3ModifyPDPContextRequest::MTI; }
    int operator()(const L3ModifyPDPContextAccept&) const { return L3ModifyPDPContextAccept::MTI; }
    int operator()(const L3ModifyPDPContextReject&) const { return L3ModifyPDPContextReject::MTI; }
    int operator()(const L3SMStatus&) const { return L3SMStatus::MTI; }
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
