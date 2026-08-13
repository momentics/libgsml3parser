# Builder Coverage Inventory

Generated: 2026-08-13 (Phase 11 final check)

Scanned all `class L3*` message types across 13 domain headers. For each class, whether it has an inner `Builder` with `static Builder builder()` factory.

**Summary:** 240 total classes | 167 OK (have Builder) | 73 NEEDS_BUILDER | Coverage: 69.6%

## Domains with full Builder coverage

| Domain | Classes | Have Builder | Need Builder |
|--------|---------|-------------|--------------|
| MM | 20 | 20 | 0 |
| CC | 24 | 24 | 0 |
| GMM | 23 | 23 | 0 |
| SM | 29 | 29 | 0 |
| SMS | 9 | 9 | 0 |
| SMSL3 | 14 | 14 | 0 |
| BCC | 8 | 8 | 0 |
| GCC | 8 | 8 | 0 |
| LS | 2 | 2 | 0 |

## Domains needing Builder

| Domain | Classes | Have Builder | Need Builder |
|--------|---------|-------------|--------------|
| RR | 98 | 50 | 48 |
| SS | 3 | 0 | 3 |
| Extended | 1 | 0 | 1 |
| TestProc | 1 | 0 | 1 |

## Detailed Table

### RR — Classes WITH Builder (50)

| Domain | Message Class | Has Builder | Has Param Constructor | Action |
|--------|--------------|-------------|----------------------|--------|
| RR | L3AdditionalAssignment | Yes | No | OK |
| RR | L3ApplicationInformation | No | No | NEEDS_BUILDER |
| RR | L3AssignmentCommand | Yes | No | OK |
| RR | L3AssignmentComplete | Yes | No | OK |
| RR | L3AssignmentFailure | Yes | No | OK |
| RR | L3CDMA2000ClassmarkChange | No | No | NEEDS_BUILDER |
| RR | L3ChannelModeModify | Yes | No | OK |
| RR | L3ChannelModeModifyAcknowledge | Yes | No | OK |
| RR | L3ChannelRelease | Yes | Yes | OK |
| RR | L3ChannelRequest | Yes | Yes | OK |
| RR | L3CipheringModeCommand | Yes | No | OK |
| RR | L3CipheringModeComplete | Yes | No | OK |
| RR | L3ClassmarkChange | No | No | NEEDS_BUILDER |
| RR | L3ClassmarkEnquiry | No | No | NEEDS_BUILDER |
| RR | L3ConfigurationChangeAcknowledge | Yes | No | OK |
| RR | L3ConfigurationChangeCommand | Yes | No | OK |
| RR | L3ConfigurationChangeReject | Yes | Yes | OK |
| RR | L3DataIndication | No | No | NEEDS_BUILDER |
| RR | L3DataIndication2 | No | No | NEEDS_BUILDER |
| RR | L3DTMAssignmentCommand | No | No | NEEDS_BUILDER |
| RR | L3DTMAssignmentFailure | No | Yes | NEEDS_BUILDER |
| RR | L3DTMInformation | No | No | NEEDS_BUILDER |
| RR | L3DTMReject | No | No | NEEDS_BUILDER |
| RR | L3DTMRequest | No | No | NEEDS_BUILDER |
| RR | L3EnhancedMeasurementRepUL | No | No | NEEDS_BUILDER |
| RR | L3ExtendedMeasurementOrder | No | No | NEEDS_BUILDER |
| RR | L3ExtendedMeasurementReport | No | No | NEEDS_BUILDER |
| RR | L3FrequencyRedefinition | No | No | NEEDS_BUILDER |
| RR | L3GERANIUClassmarkChange | No | No | NEEDS_BUILDER |
| RR | L3GPRSSuspensionRequest | No | No | NEEDS_BUILDER |
| RR | L3HandoverAccess | Yes | Yes | OK |
| RR | L3HandoverCommand | Yes | No | OK |
| RR | L3HandoverComplete | Yes | No | OK |
| RR | L3HandoverFailure | Yes | No | OK |
| RR | L3ImmediateAssignment | Yes | No | OK |
| RR | L3ImmediateAssignmentExtended | Yes | No | OK |
| RR | L3ImmediateAssignmentReject | Yes | Yes | OK |
| RR | L3IntersysToCDMA2000HOCommand | No | No | NEEDS_BUILDER |
| RR | L3IntersysToUTRANHOCommand | No | No | NEEDS_BUILDER |
| RR | L3MeasurementInfoDL | No | No | NEEDS_BUILDER |
| RR | L3MeasurementReport | No | No | NEEDS_BUILDER |
| RR | L3NotificationFACCH | No | No | NEEDS_BUILDER |
| RR | L3NotificationNCH | No | No | NEEDS_BUILDER |
| RR | L3NotificationResponse | No | No | NEEDS_BUILDER |
| RR | L3NotifyAppData | No | No | NEEDS_BUILDER |
| RR | L3PacketAssignment | No | No | NEEDS_BUILDER |
| RR | L3PacketInformation | No | No | NEEDS_BUILDER |
| RR | L3PagingRequestType1 | Yes | No | OK |
| RR | L3PagingRequestType2 | Yes | No | OK |
| RR | L3PagingRequestType3 | Yes | No | OK |
| RR | L3PagingResponse | No | No | NEEDS_BUILDER |
| RR | L3PartialRelease | Yes | No | OK |
| RR | L3PartialReleaseComplete | Yes | No | OK |
| RR | L3PhysicalInformation | Yes | No | OK |
| RR | L3PriorityUplinkRequest | No | No | NEEDS_BUILDER |
| RR | L3RRStatus | Yes | No | OK |
| RR | L3SynchronizationChannelInformation | No | No | NEEDS_BUILDER |
| RR | L3SystemInformationType1 | Yes | No | OK |
| RR | L3SystemInformationType10 | No | No | NEEDS_BUILDER |
| RR | L3SystemInformationType10bis | No | No | NEEDS_BUILDER |
| RR | L3SystemInformationType10ter | No | No | NEEDS_BUILDER |
| RR | L3SystemInformationType13 | Yes | No | OK |
| RR | L3SystemInformationType13alt | No | No | NEEDS_BUILDER |
| RR | L3SystemInformationType14 | No | No | NEEDS_BUILDER |
| RR | L3SystemInformationType15 | No | No | NEEDS_BUILDER |
| RR | L3SystemInformationType16 | Yes | No | OK |
| RR | L3SystemInformationType17 | Yes | No | OK |
| RR | L3SystemInformationType18 | No | No | NEEDS_BUILDER |
| RR | L3SystemInformationType19 | No | No | NEEDS_BUILDER |
| RR | L3SystemInformationType2 | Yes | No | OK |
| RR | L3SystemInformationType20 | No | No | NEEDS_BUILDER |
| RR | L3SystemInformationType21 | No | No | NEEDS_BUILDER |
| RR | L3SystemInformationType22 | No | No | NEEDS_BUILDER |
| RR | L3SystemInformationType23 | No | No | NEEDS_BUILDER |
| RR | L3SystemInformationType2bis | Yes | No | OK |
| RR | L3SystemInformationType2n | No | No | NEEDS_BUILDER |
| RR | L3SystemInformationType2quater | No | No | NEEDS_BUILDER |
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
| RR | L3TalkerIndication | No | No | NEEDS_BUILDER |
| RR | L3UplinkBusy | No | No | NEEDS_BUILDER |
| RR | L3UplinkFree | No | No | NEEDS_BUILDER |
| RR | L3UplinkRelease | No | No | NEEDS_BUILDER |
| RR | L3UTRANClassmarkChange | No | No | NEEDS_BUILDER |
| RR | L3VBSVGCSRecon | No | No | NEEDS_BUILDER |
| RR | L3VBSVGCSRecon2 | No | No | NEEDS_BUILDER |
| RR | L3VGCSAddInfo | No | No | NEEDS_BUILDER |
| RR | L3VGCSMSInfo | No | No | NEEDS_BUILDER |
| RR | L3VGCSSNeighCellInfo | No | No | NEEDS_BUILDER |
| RR | L3VGCSUplinkGrant | No | No | NEEDS_BUILDER |

### MM — All 20 classes have Builder (OK)

| Domain | Message Class | Has Builder | Has Param Constructor | Action |
|--------|--------------|-------------|----------------------|--------|
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

### CC — All 24 classes have Builder (OK)

| Domain | Message Class | Has Builder | Has Param Constructor | Action |
|--------|--------------|-------------|----------------------|--------|
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

### GMM — All 23 classes have Builder (OK)

| Domain | Message Class | Has Builder | Has Param Constructor | Action |
|--------|--------------|-------------|----------------------|--------|
| GMM | L3AttachAccept | Yes | No | OK |
| GMM | L3AttachComplete | Yes | No | OK |
| GMM | L3AttachReject | Yes | No | OK |
| GMM | L3AttachRequest | Yes | No | OK |
| GMM | L3AuthenticationAndCipheringFailure | Yes | No | OK |
| GMM | L3AuthenticationAndCipheringReject | Yes | No | OK |
| GMM | L3AuthenticationAndCipheringRequest | Yes | No | OK |
| GMM | L3AuthenticationAndCipheringResponse | Yes | No | OK |
| GMM | L3DetachAccept | Yes | No | OK |
| GMM | L3DetachRequest | Yes | No | OK |
| GMM | L3GMMIdentityRequest | Yes | No | OK |
| GMM | L3GMMIdentityResponse | Yes | No | OK |
| GMM | L3GMMInformation | Yes | No | OK |
| GMM | L3GMMStatus | Yes | Yes | OK |
| GMM | L3P_TMSIReallocationCommand | Yes | No | OK |
| GMM | L3P_TMSIReallocationComplete | Yes | No | OK |
| GMM | L3RoutingAreaUpdateAccept | Yes | No | OK |
| GMM | L3RoutingAreaUpdateComplete | Yes | No | OK |
| GMM | L3RoutingAreaUpdateReject | Yes | No | OK |
| GMM | L3RoutingAreaUpdateRequest | Yes | No | OK |
| GMM | L3ServiceAccept | Yes | No | OK |
| GMM | L3ServiceReject | Yes | No | OK |
| GMM | L3ServiceRequest | Yes | No | OK |

### SM — All 29 classes have Builder (OK)

| Domain | Message Class | Has Builder | Has Param Constructor | Action |
|--------|--------------|-------------|----------------------|--------|
| SM | L3ActivateAAPDPContextAccept | Yes | No | OK |
| SM | L3ActivateAAPDPContextReject | Yes | No | OK |
| SM | L3ActivateAAPDPContextRequest | Yes | No | OK |
| SM | L3ActivateMBMSContextAccept | Yes | No | OK |
| SM | L3ActivateMBMSContextReject | Yes | No | OK |
| SM | L3ActivateMBMSContextRequest | Yes | No | OK |
| SM | L3ActivatePDPContextAccept | Yes | No | OK |
| SM | L3ActivatePDPContextReject | Yes | No | OK |
| SM | L3ActivatePDPContextRequest | Yes | No | OK |
| SM | L3ActivateSecondaryPDPContextAccept | Yes | No | OK |
| SM | L3ActivateSecondaryPDPContextReject | Yes | No | OK |
| SM | L3ActivateSecondaryPDPContextRequest | Yes | No | OK |
| SM | L3DeactivateAAPDPContextAccept | Yes | No | OK |
| SM | L3DeactivateAAPDPContextRequest | Yes | No | OK |
| SM | L3DeactivatePDPContextAccept | Yes | No | OK |
| SM | L3DeactivatePDPContextRequest | Yes | No | OK |
| SM | L3ModifyPDPContextAccept | Yes | No | OK |
| SM | L3ModifyPDPContextAcceptNet | Yes | No | OK |
| SM | L3ModifyPDPContextReject | Yes | No | OK |
| SM | L3ModifyPDPContextRequest | Yes | No | OK |
| SM | L3ModifyPDPContextRequestMS | Yes | No | OK |
| SM | L3RequestMBMSContextActivation | Yes | No | OK |
| SM | L3RequestMBMSContextActivationReject | Yes | No | OK |
| SM | L3RequestPDPContextActivation | Yes | No | OK |
| SM | L3RequestPDPContextActivationReject | Yes | No | OK |
| SM | L3RequestSecondaryPDPContextActivation | Yes | No | OK |
| SM | L3RequestSecondaryPDPContextActivationReject | Yes | No | OK |
| SM | L3SMNotification | Yes | No | OK |
| SM | L3SMStatus | Yes | Yes | OK |

### SMS — All 9 classes have Builder (OK)

| Domain | Message Class | Has Builder | Has Param Constructor | Action |
|--------|--------------|-------------|----------------------|--------|
| SMS | L3CPAck | Yes | No | OK |
| SMS | L3CPData | Yes | No | OK |
| SMS | L3CPErr | Yes | No | OK |
| SMS | L3CPSMT | Yes | No | OK |
| SMS | L3CPStatus | Yes | No | OK |
| SMS | L3RPAck | Yes | No | OK |
| SMS | L3RPData | Yes | No | OK |
| SMS | L3RPError | Yes | No | OK |
| SMS | L3RPSMMA | Yes | No | OK |

### SMSL3 — All 14 classes have Builder (OK)

| Domain | Message Class | Has Builder | Has Param Constructor | Action |
|--------|--------------|-------------|----------------------|--------|
| SMSL3 | L3SMSDeliver | Yes | No | OK |
| SMSL3 | L3SMSDeliverRep | Yes | No | OK |
| SMSL3 | L3SMSNotification | Yes | No | OK |
| SMSL3 | L3SMSProvidedReplyExpected | Yes | No | OK |
| SMSL3 | L3SMSSFProvidedRep | Yes | No | OK |
| SMSL3 | L3SMSSFProvidedRepAck | Yes | No | OK |
| SMSL3 | L3SMSShortCodeInfo | Yes | No | OK |
| SMSL3 | L3SMSStatusReport | Yes | No | OK |
| SMSL3 | L3SMSStatusReportAck | Yes | No | OK |
| SMSL3 | L3SMSStatusReportReject | Yes | No | OK |
| SMSL3 | L3SMSSubmitDeferred | Yes | No | OK |
| SMSL3 | L3SMSSubmitReject | Yes | No | OK |
| SMSL3 | L3SMSSubmitRep | Yes | No | OK |
| SMSL3 | L3SMSTSReject | Yes | No | OK |

### BCC — All 8 classes have Builder (OK)

| Domain | Message Class | Has Builder | Has Param Constructor | Action |
|--------|--------------|-------------|----------------------|--------|
| BCC | L3BCCCallConfirmed | Yes | No | OK |
| BCC | L3BCCConnect | Yes | No | OK |
| BCC | L3BCCConnectAcknowledge | Yes | No | OK |
| BCC | L3BCCDisconnect | Yes | No | OK |
| BCC | L3BCCProceeding | Yes | No | OK |
| BCC | L3BCCRelease | Yes | No | OK |
| BCC | L3BCCReleaseComplete | Yes | No | OK |
| BCC | L3BCCSetup | Yes | No | OK |

### GCC — All 8 classes have Builder (OK)

| Domain | Message Class | Has Builder | Has Param Constructor | Action |
|--------|--------------|-------------|----------------------|--------|
| GCC | L3GCCAcknowledge | Yes | No | OK |
| GCC | L3GCCCallConfirmed | Yes | No | OK |
| GCC | L3GCCConnect | Yes | No | OK |
| GCC | L3GCCDisconnect | Yes | No | OK |
| GCC | L3GCCProceeding | Yes | No | OK |
| GCC | L3GCCRelease | Yes | No | OK |
| GCC | L3GCCReleaseComplete | Yes | No | OK |
| GCC | L3GCCSetup | Yes | No | OK |

### LS — All 2 classes have Builder (OK)

| Domain | Message Class | Has Builder | Has Param Constructor | Action |
|--------|--------------|-------------|----------------------|--------|
| LS | L3LocationServiceProviderMessage | Yes | No | OK |
| LS | L3LocationServiceRequest | Yes | No | OK |

### SS — 0 of 3 classes have Builder

| Domain | Message Class | Has Builder | Has Param Constructor | Action |
|--------|--------------|-------------|----------------------|--------|
| SS | L3SupServFacilityMessage | No | Yes | NEEDS_BUILDER |
| SS | L3SupServRegisterMessage | No | Yes | NEEDS_BUILDER |
| SS | L3SupServReleaseCompleteMessage | No | Yes | NEEDS_BUILDER |

### Extended — 0 of 1 class has Builder

| Domain | Message Class | Has Builder | Has Param Constructor | Action |
|--------|--------------|-------------|----------------------|--------|
| Extended | L3ExtendedMessage | No | Yes | NEEDS_BUILDER |

### TestProc — 0 of 1 class has Builder

| Domain | Message Class | Has Builder | Has Param Constructor | Action |
|--------|--------------|-------------|----------------------|--------|
| TestProc | L3TestProcedureMessage | No | Yes | NEEDS_BUILDER |
