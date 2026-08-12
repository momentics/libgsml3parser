# Builder Coverage Inventory

Generated: 2026-08-12

Scanned all `class L3*` message types across 13 domain headers. For each class, whether it has an inner `Builder` struct/class and an `explicit` constructor.

**Summary:** 240 total classes | 142 OK (have Builder) | 98 NEEDS_BUILDER

## Domains with full Builder coverage

| Domain | Classes | Have Builder | Need Builder |
|--------|---------|-------------|--------------|
| RR | 93 | 93 | 0 |
| MM | 20 | 20 | 0 |
| CC | 24 | 24 | 0 |

## Domains needing Builder

| Domain | Classes | Have Builder | Need Builder |
|--------|---------|-------------|--------------|
| GMM | 23 | 0 | 23 |
| SM | 29 | 0 | 29 |
| SMSL3 | 14 | 0 | 14 |
| SMS | 9 | 0 | 9 |
| BCC | 8 | 0 | 8 |
| GCC | 8 | 0 | 8 |
| SS | 3 | 0 | 3 |
| LS | 2 | 0 | 2 |
| Extended | 1 | 0 | 1 |
| TestProc | 1 | 0 | 1 |

## Detailed Table

| Domain | Message Class | Has Builder | Has Param Constructor | Action Needed |
|--------|--------------|-------------|----------------------|---------------|
| RR | L3AdditionalAssignment | Yes | No | OK |
| RR | L3ApplicationInformation | Yes | No | OK |
| RR | L3AssignmentCommand | Yes | No | OK |
| RR | L3AssignmentComplete | Yes | No | OK |
| RR | L3AssignmentFailure | Yes | No | OK |
| RR | L3CDMA2000ClassmarkChange | Yes | No | OK |
| RR | L3ChannelModeModify | Yes | No | OK |
| RR | L3ChannelModeModifyAcknowledge | Yes | No | OK |
| RR | L3ChannelRelease | Yes | Yes | OK |
| RR | L3ChannelRequest | Yes | Yes | OK |
| RR | L3CipheringModeCommand | Yes | No | OK |
| RR | L3CipheringModeComplete | Yes | No | OK |
| RR | L3ClassmarkChange | Yes | No | OK |
| RR | L3ClassmarkEnquiry | Yes | No | OK |
| RR | L3ConfigurationChangeAcknowledge | Yes | No | OK |
| RR | L3ConfigurationChangeCommand | Yes | No | OK |
| RR | L3ConfigurationChangeReject | Yes | Yes | OK |
| RR | L3DataIndication | Yes | No | OK |
| RR | L3DataIndication2 | Yes | No | OK |
| RR | L3DTMAssignmentCommand | Yes | No | OK |
| RR | L3DTMAssignmentFailure | Yes | Yes | OK |
| RR | L3DTMInformation | Yes | No | OK |
| RR | L3DTMReject | Yes | No | OK |
| RR | L3DTMRequest | Yes | No | OK |
| RR | L3EnhancedMeasurementRepUL | Yes | No | OK |
| RR | L3ExtendedMeasurementOrder | Yes | No | OK |
| RR | L3ExtendedMeasurementReport | Yes | No | OK |
| RR | L3FrequencyRedefinition | Yes | No | OK |
| RR | L3GERANIUClassmarkChange | Yes | No | OK |
| RR | L3GPRSSuspensionRequest | Yes | No | OK |
| RR | L3HandoverAccess | Yes | Yes | OK |
| RR | L3HandoverCommand | Yes | No | OK |
| RR | L3HandoverComplete | Yes | No | OK |
| RR | L3HandoverFailure | Yes | No | OK |
| RR | L3ImmediateAssignment | Yes | No | OK |
| RR | L3ImmediateAssignmentExtended | Yes | No | OK |
| RR | L3ImmediateAssignmentReject | Yes | Yes | OK |
| RR | L3IntersysToCDMA2000HOCommand | Yes | No | OK |
| RR | L3IntersysToUTRANHOCommand | Yes | No | OK |
| RR | L3MeasurementInfoDL | Yes | No | OK |
| RR | L3MeasurementReport | Yes | No | OK |
| RR | L3NotificationFACCH | Yes | No | OK |
| RR | L3NotificationNCH | Yes | No | OK |
| RR | L3NotificationResponse | Yes | No | OK |
| RR | L3NotifyAppData | Yes | No | OK |
| RR | L3PacketAssignment | Yes | No | OK |
| RR | L3PacketInformation | Yes | No | OK |
| RR | L3PagingRequestType1 | Yes | No | OK |
| RR | L3PagingRequestType2 | Yes | No | OK |
| RR | L3PagingRequestType3 | Yes | No | OK |
| RR | L3PagingResponse | Yes | No | OK |
| RR | L3PartialRelease | Yes | No | OK |
| RR | L3PartialReleaseComplete | Yes | No | OK |
| RR | L3PhysicalInformation | Yes | No | OK |
| RR | L3PriorityUplinkRequest | Yes | No | OK |
| RR | L3RRStatus | Yes | No | OK |
| RR | L3SynchronizationChannelInformation | Yes | No | OK |
| RR | L3SystemInformationType1 | Yes | No | OK |
| RR | L3SystemInformationType10 | Yes | No | OK |
| RR | L3SystemInformationType10bis | Yes | No | OK |
| RR | L3SystemInformationType10ter | Yes | No | OK |
| RR | L3SystemInformationType13 | Yes | No | OK |
| RR | L3SystemInformationType13alt | Yes | No | OK |
| RR | L3SystemInformationType14 | Yes | No | OK |
| RR | L3SystemInformationType15 | Yes | No | OK |
| RR | L3SystemInformationType16 | Yes | No | OK |
| RR | L3SystemInformationType17 | Yes | No | OK |
| RR | L3SystemInformationType18 | Yes | No | OK |
| RR | L3SystemInformationType19 | Yes | No | OK |
| RR | L3SystemInformationType2 | Yes | No | OK |
| RR | L3SystemInformationType20 | Yes | No | OK |
| RR | L3SystemInformationType21 | Yes | No | OK |
| RR | L3SystemInformationType22 | Yes | No | OK |
| RR | L3SystemInformationType23 | Yes | No | OK |
| RR | L3SystemInformationType2bis | Yes | No | OK |
| RR | L3SystemInformationType2n | Yes | No | OK |
| RR | L3SystemInformationType2quater | Yes | No | OK |
| RR | L3SystemInformationType2ter | Yes | No | OK |
| RR | L3SystemInformationType3 | Yes | No | OK |
| RR | L3SystemInformationType4 | Yes | No | OK |
| RR | L3SystemInformationType5 | Yes | No | OK |
| RR | L3SystemInformationType5bis | Yes | No | OK |
| RR | L3SystemInformationType5ter | Yes | No | OK |
| RR | L3SystemInformationType6 | Yes | No | OK |
| RR | L3SystemInformationType7 | Yes | No | OK |
| RR | L3SystemInformationType8 | Yes | No | OK |
| RR | L3SystemInformationType9 | Yes | No | OK |
| RR | L3TalkerIndication | Yes | No | OK |
| RR | L3UplinkBusy | Yes | No | OK |
| RR | L3UplinkFree | Yes | No | OK |
| RR | L3UplinkRelease | Yes | No | OK |
| RR | L3UTRANClassmarkChange | Yes | No | OK |
| RR | L3VBSVGCSRecon | Yes | No | OK |
| RR | L3VBSVGCSRecon2 | Yes | No | OK |
| RR | L3VGCSAddInfo | Yes | No | OK |
| RR | L3VGCSMSInfo | Yes | No | OK |
| RR | L3VGCSSNeighCellInfo | Yes | No | OK |
| RR | L3VGCSUplinkGrant | Yes | No | OK |
| MM | L3AuthenticationReject | Yes | No | OK |
| MM | L3AuthenticationRequest | Yes | No | OK |
| MM | L3AuthenticationResponse | Yes | Yes | OK |
| MM | L3CMReestablishmentRequest | Yes | No | OK |
| MM | L3CMRequest | Yes | No | OK |
| MM | L3CMServiceAbort | Yes | No | OK |
| MM | L3CMServiceAccept | Yes | No | OK |
| MM | L3CMServiceReject | Yes | Yes | OK |
| MM | L3CMServiceRequest | Yes | No | OK |
| MM | L3IdentityRequest | Yes | Yes | OK |
| MM | L3IdentityResponse | Yes | No | OK |
| MM | L3IMSIDetachIndication | Yes | No | OK |
| MM | L3LocationUpdatingAccept | Yes | No | OK |
| MM | L3LocationUpdatingReject | Yes | Yes | OK |
| MM | L3LocationUpdatingRequest | Yes | No | OK |
| MM | L3MMInformation | Yes | No | OK |
| MM | L3MMStatus | Yes | No | OK |
| MM | L3PagingMM | Yes | No | OK |
| MM | L3TMSIReallocationCommand | Yes | No | OK |
| MM | L3TMSIReallocationComplete | Yes | No | OK |
| CC | L3Alerting | Yes | No | OK |
| CC | L3CallConfirmed | Yes | No | OK |
| CC | L3CallProceeding | Yes | No | OK |
| CC | L3CCStatus | Yes | No | OK |
| CC | L3Connect | Yes | No | OK |
| CC | L3ConnectAcknowledge | Yes | No | OK |
| CC | L3Disconnect | Yes | Yes | OK |
| CC | L3EmergencySetup | Yes | No | OK |
| CC | L3ErrorIndication | Yes | No | OK |
| CC | L3Facility | Yes | No | OK |
| CC | L3Hold | Yes | No | OK |
| CC | L3HoldReject | Yes | Yes | OK |
| CC | L3Modify | Yes | No | OK |
| CC | L3Progress | Yes | No | OK |
| CC | L3Release | Yes | No | OK |
| CC | L3ReleaseComplete | Yes | No | OK |
| CC | L3Setup | Yes | No | OK |
| CC | L3StartDTMF | Yes | No | OK |
| CC | L3StartDTMFAcknowledge | Yes | Yes | OK |
| CC | L3StartDTMFReject | Yes | Yes | OK |
| CC | L3StopDTMF | Yes | No | OK |
| CC | L3StopDTMFAcknowledge | Yes | No | OK |
| CC | L3UnitData | Yes | No | OK |
| CC | L3UnitDataAck | Yes | No | OK |
| GMM | L3AttachAccept | No | No | NEEDS_BUILDER |
| GMM | L3AttachComplete | No | No | NEEDS_BUILDER |
| GMM | L3AttachReject | No | No | NEEDS_BUILDER |
| GMM | L3AttachRequest | No | No | NEEDS_BUILDER |
| GMM | L3AuthenticationAndCipheringFailure | No | No | NEEDS_BUILDER |
| GMM | L3AuthenticationAndCipheringReject | No | No | NEEDS_BUILDER |
| GMM | L3AuthenticationAndCipheringRequest | No | No | NEEDS_BUILDER |
| GMM | L3AuthenticationAndCipheringResponse | No | No | NEEDS_BUILDER |
| GMM | L3DetachAccept | No | No | NEEDS_BUILDER |
| GMM | L3DetachRequest | No | No | NEEDS_BUILDER |
| GMM | L3GMMIdentityRequest | No | No | NEEDS_BUILDER |
| GMM | L3GMMIdentityResponse | No | No | NEEDS_BUILDER |
| GMM | L3GMMInformation | No | No | NEEDS_BUILDER |
| GMM | L3GMMStatus | No | Yes | NEEDS_BUILDER |
| GMM | L3P_TMSIReallocationCommand | No | No | NEEDS_BUILDER |
| GMM | L3P_TMSIReallocationComplete | No | No | NEEDS_BUILDER |
| GMM | L3RoutingAreaUpdateAccept | No | No | NEEDS_BUILDER |
| GMM | L3RoutingAreaUpdateComplete | No | No | NEEDS_BUILDER |
| GMM | L3RoutingAreaUpdateReject | No | No | NEEDS_BUILDER |
| GMM | L3RoutingAreaUpdateRequest | No | No | NEEDS_BUILDER |
| GMM | L3ServiceAccept | No | No | NEEDS_BUILDER |
| GMM | L3ServiceReject | No | No | NEEDS_BUILDER |
| GMM | L3ServiceRequest | No | No | NEEDS_BUILDER |
| SM | L3ActivateAAPDPContextAccept | No | No | NEEDS_BUILDER |
| SM | L3ActivateAAPDPContextReject | No | No | NEEDS_BUILDER |
| SM | L3ActivateAAPDPContextRequest | No | No | NEEDS_BUILDER |
| SM | L3ActivateMBMSContextAccept | No | No | NEEDS_BUILDER |
| SM | L3ActivateMBMSContextReject | No | No | NEEDS_BUILDER |
| SM | L3ActivateMBMSContextRequest | No | No | NEEDS_BUILDER |
| SM | L3ActivatePDPContextAccept | No | No | NEEDS_BUILDER |
| SM | L3ActivatePDPContextReject | No | No | NEEDS_BUILDER |
| SM | L3ActivatePDPContextRequest | No | No | NEEDS_BUILDER |
| SM | L3ActivateSecondaryPDPContextAccept | No | No | NEEDS_BUILDER |
| SM | L3ActivateSecondaryPDPContextReject | No | No | NEEDS_BUILDER |
| SM | L3ActivateSecondaryPDPContextRequest | No | No | NEEDS_BUILDER |
| SM | L3DeactivateAAPDPContextAccept | No | No | NEEDS_BUILDER |
| SM | L3DeactivateAAPDPContextRequest | No | No | NEEDS_BUILDER |
| SM | L3DeactivatePDPContextAccept | No | No | NEEDS_BUILDER |
| SM | L3DeactivatePDPContextRequest | No | No | NEEDS_BUILDER |
| SM | L3ModifyPDPContextAccept | No | No | NEEDS_BUILDER |
| SM | L3ModifyPDPContextAcceptNet | No | No | NEEDS_BUILDER |
| SM | L3ModifyPDPContextReject | No | No | NEEDS_BUILDER |
| SM | L3ModifyPDPContextRequest | No | No | NEEDS_BUILDER |
| SM | L3ModifyPDPContextRequestMS | No | No | NEEDS_BUILDER |
| SM | L3RequestMBMSContextActivation | No | No | NEEDS_BUILDER |
| SM | L3RequestMBMSContextActivationReject | No | No | NEEDS_BUILDER |
| SM | L3RequestPDPContextActivation | No | No | NEEDS_BUILDER |
| SM | L3RequestPDPContextActivationReject | No | No | NEEDS_BUILDER |
| SM | L3RequestSecondaryPDPContextActivation | No | No | NEEDS_BUILDER |
| SM | L3RequestSecondaryPDPContextActivationReject | No | No | NEEDS_BUILDER |
| SM | L3SMNotification | No | No | NEEDS_BUILDER |
| SM | L3SMStatus | No | Yes | NEEDS_BUILDER |
| SMS | L3CPAck | No | No | NEEDS_BUILDER |
| SMS | L3CPData | No | No | NEEDS_BUILDER |
| SMS | L3CPErr | No | No | NEEDS_BUILDER |
| SMS | L3CPSMT | No | No | NEEDS_BUILDER |
| SMS | L3CPStatus | No | No | NEEDS_BUILDER |
| SMS | L3RPAck | No | No | NEEDS_BUILDER |
| SMS | L3RPData | No | No | NEEDS_BUILDER |
| SMS | L3RPError | No | No | NEEDS_BUILDER |
| SMS | L3RPSMMA | No | No | NEEDS_BUILDER |
| SMSL3 | L3SMSDeliver | No | No | NEEDS_BUILDER |
| SMSL3 | L3SMSDeliverRep | No | No | NEEDS_BUILDER |
| SMSL3 | L3SMSNotification | No | No | NEEDS_BUILDER |
| SMSL3 | L3SMSProvidedReplyExpected | No | No | NEEDS_BUILDER |
| SMSL3 | L3SMSSFProvidedRep | No | No | NEEDS_BUILDER |
| SMSL3 | L3SMSSFProvidedRepAck | No | No | NEEDS_BUILDER |
| SMSL3 | L3SMSShortCodeInfo | No | No | NEEDS_BUILDER |
| SMSL3 | L3SMSStatusReport | No | No | NEEDS_BUILDER |
| SMSL3 | L3SMSStatusReportAck | No | No | NEEDS_BUILDER |
| SMSL3 | L3SMSStatusReportReject | No | No | NEEDS_BUILDER |
| SMSL3 | L3SMSSubmitDeferred | No | No | NEEDS_BUILDER |
| SMSL3 | L3SMSSubmitReject | No | No | NEEDS_BUILDER |
| SMSL3 | L3SMSSubmitRep | No | No | NEEDS_BUILDER |
| SMSL3 | L3SMSTSReject | No | No | NEEDS_BUILDER |
| BCC | L3BCCCallConfirmed | No | No | NEEDS_BUILDER |
| BCC | L3BCCConnect | No | No | NEEDS_BUILDER |
| BCC | L3BCCConnectAcknowledge | No | No | NEEDS_BUILDER |
| BCC | L3BCCDisconnect | No | No | NEEDS_BUILDER |
| BCC | L3BCCProceeding | No | No | NEEDS_BUILDER |
| BCC | L3BCCRelease | No | No | NEEDS_BUILDER |
| BCC | L3BCCReleaseComplete | No | No | NEEDS_BUILDER |
| BCC | L3BCCSetup | No | No | NEEDS_BUILDER |
| GCC | L3GCCAcknowledge | No | No | NEEDS_BUILDER |
| GCC | L3GCCCallConfirmed | No | No | NEEDS_BUILDER |
| GCC | L3GCCConnect | No | No | NEEDS_BUILDER |
| GCC | L3GCCDisconnect | No | No | NEEDS_BUILDER |
| GCC | L3GCCProceeding | No | No | NEEDS_BUILDER |
| GCC | L3GCCRelease | No | No | NEEDS_BUILDER |
| GCC | L3GCCReleaseComplete | No | No | NEEDS_BUILDER |
| GCC | L3GCCSetup | No | No | NEEDS_BUILDER |
| LS | L3LocationServiceProviderMessage | No | No | NEEDS_BUILDER |
| LS | L3LocationServiceRequest | No | No | NEEDS_BUILDER |
| SS | L3SupServFacilityMessage | No | Yes | NEEDS_BUILDER |
| SS | L3SupServRegisterMessage | No | Yes | NEEDS_BUILDER |
| SS | L3SupServReleaseCompleteMessage | No | Yes | NEEDS_BUILDER |
| Extended | L3ExtendedMessage | No | Yes | NEEDS_BUILDER |
| TestProc | L3TestProcedureMessage | No | Yes | NEEDS_BUILDER |
