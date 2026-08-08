#pragma once

#include <variant>
#include "rr/l3rrmessages.h"
#include "mm/l3mmmessages.h"
#include "cc/l3ccmessages.h"
#include "ss/l3ssmessages.h"

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
    L3SystemInformationType17
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
    L3AuthenticationReject
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
    L3Progress
>;

using SSM = std::variant<
    L3SupServFacilityMessage,
    L3SupServRegisterMessage,
    L3SupServReleaseCompleteMessage
>;

using ParsedMessage = std::variant<RRM, MMM, CCM, SSM>;

static_assert(sizeof(ParsedMessage) < 4096, "ParsedMessage variant too large");

} // namespace gsml3parser
