#pragma once

#include <variant>
#include "rr/l3rrmessages.h"
#include "mm/l3mmmessages.h"
#include "cc/l3ccmessages.h"
#include "ss/l3ssmessages.h"
#include "gmm/l3gmmmessages.h"
#include "sms/l3smsmessages.h"
#include "sm/l3smmessages.h"
#include "bcc/l3bccmessages.h"
#include "gcc/l3gccmessages.h"
#include "ls/l3lsmessages.h"

namespace gsml3parser {

using RRM = std::variant<
    L3PagingRequestType1,
    L3PagingRequestType2,
    L3PagingRequestType3,
    L3PagingResponse,
    L3ClassmarkChange,
    L3ClassmarkEnquiry,
    L3MeasurementReport,
    L3ChannelRelease,
    L3RRStatus,
    L3AssignmentCommand,
    L3AssignmentComplete,
    L3AssignmentFailure,
    L3ImmediateAssignment,
    L3ImmediateAssignmentExtended,
    L3ImmediateAssignmentReject,
    L3AdditionalAssignment,
    L3HandoverCommand,
    L3HandoverComplete,
    L3HandoverFailure,
    L3PhysicalInformation,
    L3CipheringModeCommand,
    L3CipheringModeComplete,
    L3ChannelModeModify,
    L3ChannelModeModifyAcknowledge,
    L3GPRSSuspensionRequest,
    L3ApplicationInformation,
    L3SynchronizationChannelInformation,
    L3ChannelRequest,
    L3HandoverAccess,
    L3SystemInformationType1,
    L3SystemInformationType2,
    L3SystemInformationType2bis,
    L3SystemInformationType2ter,
    L3SystemInformationType3,
    L3SystemInformationType4,
    L3SystemInformationType5,
    L3SystemInformationType5bis,
    L3SystemInformationType5ter,
    L3SystemInformationType6,
    L3SystemInformationType7,
    L3SystemInformationType8,
    L3SystemInformationType9,
    L3SystemInformationType13,
    L3SystemInformationType16,
    L3SystemInformationType17,
    L3ConfigurationChangeCommand,
    L3ConfigurationChangeAcknowledge,
    L3ConfigurationChangeReject,
    L3PartialRelease,
    L3PartialReleaseComplete,
    L3ExtendedMeasurementReport,
    L3ExtendedMeasurementOrder,
    L3FrequencyRedefinition,
    L3NotificationNCH,
    L3NotificationResponse,
    L3VGCSUplinkGrant,
    L3UplinkRelease,
    L3UplinkBusy,
    L3TalkerIndication,
    L3PriorityUplinkRequest,
    L3DataIndication,
    L3DataIndication2,
    L3DTMAssignmentFailure,
    L3DTMReject,
    L3DTMRequest,
    L3PacketAssignment,
    L3DTMAssignmentCommand,
    L3DTMInformation,
    L3PacketInformation,
    L3UTRANClassmarkChange,
    L3CDMA2000ClassmarkChange,
    L3IntersysToUTRANHOCommand,
    L3IntersysToCDMA2000HOCommand,
    L3GERANIUClassmarkChange,
    L3SystemInformationType14,
    L3SystemInformationType15,
    L3SystemInformationType18,
    L3SystemInformationType19,
    L3SystemInformationType20,
    L3SystemInformationType13alt,
    L3SystemInformationType2n,
    L3SystemInformationType21,
    L3SystemInformationType22,
    L3SystemInformationType23,
    L3SystemInformationType10,
    L3SystemInformationType10bis,
    L3SystemInformationType10ter,
    L3NotificationFACCH,
    L3UplinkFree,
    L3EnhancedMeasurementRepUL,
    L3MeasurementInfoDL,
    L3VBSVGCSRecon,
    L3VBSVGCSRecon2,
    L3VGCSAddInfo,
    L3VGCSMSInfo,
    L3VGCSSNeighCellInfo,
    L3NotifyAppData,
    L3SystemInformationType2quater
>;

using MMM = std::variant<
    L3IMSIDetachIndication,
    L3CMServiceAccept,
    L3CMServiceReject,
    L3CMServiceAbort,
    L3CMServiceRequest,
    L3CMReestablishmentRequest,
    L3IdentityResponse,
    L3IdentityRequest,
    L3MMInformation,
    L3LocationUpdatingAccept,
    L3LocationUpdatingReject,
    L3LocationUpdatingRequest,
    L3TMSIReallocationCommand,
    L3TMSIReallocationComplete,
    L3MMStatus,
    L3AuthenticationRequest,
    L3AuthenticationResponse,
    L3AuthenticationReject,
    L3CMRequest,
    L3PagingMM
>;

using CCM = std::variant<
    L3Setup,
    L3EmergencySetup,
    L3CallProceeding,
    L3Alerting,
    L3Connect,
    L3ConnectAcknowledge,
    L3CallConfirmed,
    L3Disconnect,
    L3Release,
    L3ReleaseComplete,
    L3StartDTMF,
    L3StopDTMF,
    L3StopDTMFAcknowledge,
    L3StartDTMFAcknowledge,
    L3StartDTMFReject,
    L3Hold,
    L3HoldReject,
    L3CCStatus,
    L3Progress,
    L3Facility,
    L3Modify,
    L3UnitData,
    L3UnitDataAck,
    L3ErrorIndication
>;

using SSM = std::variant<
    L3SupServFacilityMessage,
    L3SupServRegisterMessage,
    L3SupServReleaseCompleteMessage
>;

using GMM = std::variant<
    L3AttachRequest,
    L3AttachAccept,
    L3AttachComplete,
    L3AttachReject,
    L3DetachRequest,
    L3DetachAccept,
    L3RoutingAreaUpdateRequest,
    L3RoutingAreaUpdateAccept,
    L3RoutingAreaUpdateComplete,
    L3RoutingAreaUpdateReject,
    L3ServiceRequest,
    L3ServiceAccept,
    L3ServiceReject,
    L3P_TMSIReallocationCommand,
    L3P_TMSIReallocationComplete,
    L3AuthenticationAndCipheringRequest,
    L3AuthenticationAndCipheringResponse,
    L3AuthenticationAndCipheringReject,
    L3GMMIdentityRequest,
    L3GMMIdentityResponse,
    L3AuthenticationAndCipheringFailure,
    L3GMMStatus,
    L3GMMInformation
>;

using SM = std::variant<
    L3ActivatePDPContextRequest,
    L3ActivatePDPContextAccept,
    L3ActivatePDPContextReject,
    L3DeactivatePDPContextRequest,
    L3DeactivatePDPContextAccept,
    L3ModifyPDPContextRequest,
    L3ModifyPDPContextAccept,
    L3ModifyPDPContextReject,
    L3SMStatus,
    L3RequestPDPContextActivation,
    L3RequestPDPContextActivationReject,
    L3ModifyPDPContextRequestMS,
    L3ModifyPDPContextAcceptNet,
    L3ActivateSecondaryPDPContextRequest,
    L3ActivateSecondaryPDPContextAccept,
    L3ActivateSecondaryPDPContextReject,
    L3ActivateAAPDPContextRequest,
    L3ActivateAAPDPContextAccept,
    L3ActivateAAPDPContextReject,
    L3DeactivateAAPDPContextRequest,
    L3DeactivateAAPDPContextAccept,
    L3ActivateMBMSContextRequest,
    L3ActivateMBMSContextAccept,
    L3ActivateMBMSContextReject,
    L3RequestMBMSContextActivation,
    L3RequestMBMSContextActivationReject,
    L3RequestSecondaryPDPContextActivation,
    L3RequestSecondaryPDPContextActivationReject,
    L3SMNotification
>;

using SMS = std::variant<
    L3CPData,
    L3CPAck,
    L3CPErr,
    L3CPStatus,
    L3CPSMT
>;

using BCCM = std::variant<
    L3BCCSetup,
    L3BCCProceeding,
    L3BCCConnect,
    L3BCCDisconnect,
    L3BCCRelease,
    L3BCCReleaseComplete,
    L3BCCCallConfirmed,
    L3BCCConnectAcknowledge
>;

using GCCM = std::variant<
    L3GCCSetup,
    L3GCCAcknowledge,
    L3GCCProceeding,
    L3GCCConnect,
    L3GCCDisconnect,
    L3GCCRelease,
    L3GCCReleaseComplete,
    L3GCCCallConfirmed
>;

using LSM = std::variant<
    L3LocationServiceRequest,
    L3LocationServiceProviderMessage
>;

using ParsedMessage = std::variant<RRM, MMM, CCM, SSM, GMM, SM, SMS, BCCM, GCCM, LSM>;

static_assert(sizeof(ParsedMessage) < 8192, "ParsedMessage variant too large");

} // namespace gsml3parser
