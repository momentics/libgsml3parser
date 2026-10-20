# Supported Messages — Full Reference

Complete catalog of all L3 message types, Information Elements, and enums implemented in libgsml3parser.

**Total: 216 message types across 13 protocol domains.**

For a summary table see [README.md](../README.md#supported-messages-summary).

Dispatch notes:
- Octet 0 of the L3 header is `(TI << 5) | (TIF << 4) | PD` — the PD is the low nibble (TS 24.007 Table 11.3).
- MM/CC/SS/BCC/GCC headers use the six low bits of octet 1 as the messageType: `mti = byte1 & 0x3F` (the two high bits carry the NSD and are informational).
- GMM/SMS/SM/LS use the raw 8-bit second octet.
- RR normally uses the raw second octet; with TIF=1 (short messages) the five-bit code in the low bits of octet 1 maps to `mti = kRRTifShortBase | (byte1 & 0x1F)` — internal MTIs ≥ 0x100 are listed with their dispatch value. Unallocated short codes are rejected as InvalidMTI.
- SMS: only the CP-layer messages are defined for the SMS protocol discriminator (TS 24.011); MTI 0x12 dispatches to `L3CPStatus` and MTI 0x13 to `L3CPSMT`.
- The synthetic no-header RR messages are routed by frame length, not by an L3 header; their internal MTIs sit above the TIF short range so a wire short frame can never be misclassified as one of them: ChannelRequest (0x181, 1 byte), HandoverAccess (0x182, 4 bytes).

---

## Group Call Control (PD=0x00) — 8 message types

No normative reference templates exist for this block: bodies are kept as opaque octet sequences
(best-effort parsing), and bit-level conformance is not claimed.

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3GCCSetup` | 0x00 | MO | Group call setup request (TI + opaque body) |
| `L3GCCProceeding` | 0x01 | MT | Network proceeding indication |
| `L3GCCAcknowledge` | 0x02 | MT | Group call acknowledgement |
| `L3GCCCallConfirmed` | 0x03 | MT | Call confirmed |
| `L3GCCConnect` | 0x05 | MT | Group call connected |
| `L3GCCDisconnect` | 0x06 | MO | Group call disconnect request |
| `L3GCCRelease` | 0x07 | MT | Group call release |
| `L3GCCReleaseComplete` | 0x0a | Bidir | Group call release complete |

## Broadcast Call Control (PD=0x01) — 8 message types

No normative reference templates exist for this block: bodies are kept as opaque octet sequences
(best-effort parsing), and bit-level conformance is not claimed.

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3BCCSetup` | 0x00 | MO | Broadcast call setup request (TI + opaque body) |
| `L3BCCProceeding` | 0x01 | MT | Network proceeding indication |
| `L3BCCConnect` | 0x05 | MT | Broadcast call connected |
| `L3BCCDisconnect` | 0x06 | MO | Broadcast call disconnect request |
| `L3BCCRelease` | 0x07 | MT | Broadcast call release |
| `L3BCCCallConfirmed` | 0x04 | MT | Call confirmed |
| `L3BCCReleaseComplete` | 0x0a | Bidir | Broadcast call release complete |
| `L3BCCConnectAcknowledge` | 0x09 | Bidir | Connect acknowledged |

## Call Control (PD=0x03) — 21 message types, 27 IE types

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3Alerting` | 0x01 | DL | Alerting notification |
| `L3CallProceeding` | 0x02 | DL | Call proceeding indication |
| `L3Progress` | 0x03 | DL | Progress indication |
| `L3Setup` | 0x05 | UL | Call setup request |
| `L3Connect` | 0x07 | UL | Call connected |
| `L3CallConfirmed` | 0x08 | DL | Call confirmed |
| `L3EmergencySetup` | 0x0e | UL | Emergency call setup |
| `L3ConnectAcknowledge` | 0x0f | DL | Connect acknowledged |
| `L3Hold` | 0x18 | UL | Hold request |
| `L3HoldReject` | 0x1a | DL | Hold rejected |
| `L3Disconnect` | 0x25 | UL | Disconnect request |
| `L3ReleaseComplete` | 0x2a | Bidir | Release complete |
| `L3Release` | 0x2d | Bidir | Release request |
| `L3StopDTMF` | 0x31 | UL | Stop DTMF tones |
| `L3StopDTMFAcknowledge` | 0x32 | DL | Stop DTMF acknowledged |
| `L3StartDTMF` | 0x35 | UL | Start DTMF tones |
| `L3StartDTMFAcknowledge` | 0x36 | DL | Start DTMF acknowledged |
| `L3StartDTMFReject` | 0x37 | DL | Start DTMF rejected |
| `L3Facility` | 0x3a | DL/UL | CC Facility — SS data container (TS 24.008 9.3.21) |
| `L3CCStatus` | 0x3d | Bidir | CC status: cause as a length-value element without an identifier (first body octet = value length) + call state (one octet); four-octet body |
| `L3CCNotify` | 0x3e | MT | CC notify (single cause octet, TS 24.008) |

### CC Information Elements (27 types)

| IE | Format | Description |
|----|--------|-------------|
| `L3BearerCapability` | TLV | Bearer capability (coding, mode, rate) |
| `L3BackupBearerCapability` | TLV | Backup bearer capability |
| `L3SupportedCodecList` | TLV | AMR codec set and mode preferences |
| `L3BCDDigits` | V | BCD-encoded digit string utility |
| `L3CalledPartyBCDNumber` | TLV | Called party number |
| `L3CallingPartyBCDNumber` | TLV | Calling party number |
| `L3ConnectedNumber` | TLV | Connected party number |
| `L3RedirectingNumber` | TLV | Redirecting number |
| `L3SubAddress` | TLV | Calling/Called party sub-address |
| `L3CauseElement` | TLV | CC cause (IEI 0x08, length 2; TS 24.008 §10.5.1.2): value octets `[0x60\|location(4)][0x80\|cause(7)]` |
| `L3CallState` | V | Call state flags (speech, DTMF, hold) |
| `L3ProgressIndicator` | TV | Progress indicator: in SETUP/CONNECT/ALERTING/Call Proceeding a one-octet value `[location(4)\|progress(4)]` after IEI 0x1E; in the CC PROGRESS body an LV of two octets `[0xE0\|location][0x80\|progress]` |
| `L3KeypadFacility` | TV | DTMF digit indicator |
| `L3Signal` | TV | Signal type indicator |
| `L3RepeatIndicator` | TV | Repeat count for keypad DTMF |
| `L3CLIRSuppression` | T | CLIR suppression (type T: the identifier octet is the whole encoding, no value part) |
| `L3CLIRInvocation` | T | CLIR invocation (type T: the identifier octet is the whole encoding, no value part) |
| `L3NetworkCCCapabilities` | TLV | Network CC capabilities |
| `L3LowLayerCompatibility` | TLV | Low layer compatibility |
| `L3HighLayerCompatibility` | TLV | High layer compatibility |
| `L3UserUser` | TLV | User-User information element |
| `L3Priority` | TV | Priority level and request flag |
| `L3StreamIdentifier` | TLV | VBS/VGCS stream identifier (IEI 0x2D, one-octet value part) |
| `L3AllowedActions` | TLV | Allowed actions bitmask |
| `L3CCCapabilities` | TLV | CC capabilities |
| `L3SupServFacilityIE` | TLV | Supplementary service facility data |
| `L3SupServVersionIndicator` | V | SS version indicator |

## Mobility Management (PD=0x05) — 21 message types

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3IMSIDetachIndication` | 0x01 | UL | IMSI detach: classmark1 (one value octet, no length) + mobile identity (LV) |
| `L3LocationUpdatingAccept` | 0x02 | DL | Location updating accepted |
| `L3LocationUpdatingReject` | 0x04 | DL | Location updating rejected |
| `L3LocationUpdatingRequest` | 0x08 | UL | Update type + follow-on + CKSN (one octet), LAI (V, five octets), classmark1 (V, one octet, no length), mobile identity (LV) |
| `L3AuthenticationReject` | 0x11 | DL | Authentication rejected |
| `L3AuthenticationRequest` | 0x12 | DL | CKSN (high half-octet) + RAND (16 octets); optional parameters kept opaque |
| `L3AuthenticationResponse` | 0x14 | UL | Authentication response (SRES) |
| `L3AuthenticationFailure` | 0x1C | UL | Reject cause + opt. AUTS TLV 0x22 (TS 24.008 9.2.3a) |
| `L3CMServiceAccept` | 0x21 | DL | CM service accepted |
| `L3CMServiceReject` | 0x22 | DL | CM service rejected |
| `L3CMServiceAbort` | 0x23 | DL | CM service aborted |
| `L3CMServiceRequest` | 0x24 | UL | CM service type + CKSN (one octet), classmark2 (LV, three to seven value octets), mobile identity |
| `L3CMServicePrompt` | 0x25 | DL | PD/SAPI octet of the requested CM protocol (TS 24.008 9.2.5a) |
| `L3CMReestablishmentRequest` | 0x28 | UL | CKSN (one octet), classmark2 (LV, three to seven value octets), mobile identity [+ LAI (TLV 0x13)] |
| `L3MMAbort` | 0x29 | Bidir | MM abort (no value part, TS 24.008) |
| `L3MMStatus` | 0x31 | Bidir | MM status report |
| `L3MMInformation` | 0x32 | DL | Network information broadcast |
| `L3IdentityRequest` | 0x18 | DL | Identity request (IMSI/IMEI) |
| `L3IdentityResponse` | 0x19 | UL | Identity response |
| `L3TMSIReallocationCommand` | 0x1A | DL | LAI (V, five octets, not length-prefixed) + new mobile identity (LV); no further value part (TS 24.008 §9.2.17) |
| `L3TMSIReallocationComplete` | 0x1B | UL | TMSI reallocation complete |

## Radio Resource (PD=0x06) — 98 message types

### Paging

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3PagingRequestType1` | 0x21 | DL | First body octet [channelNeeded(4)\|pageMode(4)] (channel needed packed second\|first), mobile identity (LV) [+ optional second identity (TLV 0x17)]; trailing rest octets kept as an opaque tail |
| `L3PagingRequestType2` | 0x22 | DL | First body octet [channelNeeded(4)\|pageMode(4)], two raw TMSIs (8 octets) [+ optional third identity (TLV 0x17)]; trailing rest octets kept as an opaque tail |
| `L3PagingRequestType3` | 0x24 | DL | First body octet [channelNeeded(4)\|pageMode(4)], four raw TMSIs (16 octets); trailing rest octets kept as an opaque tail |
| `L3PagingResponse` | 0x27 | UL | First body octet [spare(4)\|CKSN(4)], classmark2 (LV, three to seven value octets), mobile identity (LV), [additional update parameter (one raw octet)] |

### System Information (BCCH/SACCH)

| Message | MTI | Description |
|---------|-----|-------------|
| `L3SystemInformationType1` | 0x19 | Cell channel description in the sixteen-octet variable-length BCCH frequency list form (seven-bit header, ten-bit base ARFCN, 111-bit presence map; TS 44.018 §10.5.2.1b), RACH control parameters (three octets), [rest octet]; body 19–20 octets |
| `L3SystemInformationType2` | 0x1a | BCCH frequency list (sixteen-octet bitmap form), NCC permitted, RACH control |
| `L3SystemInformationType2bis` | 0x02 | BCCH frequency list (sixteen-octet bitmap form) + RACH control, [rest octets 0–1] |
| `L3SystemInformationType2ter` | 0x03 | BCCH frequency list (sixteen-octet bitmap form), [rest octets 0–4] |
| `L3SystemInformationType3` | 0x1b | Cell desc, BA list type 1, rest octets — a rest region of pure 0x2B padding means no rest octets are present; sub-octet alignment is padded with the 0x2B pattern (TS 44.018 §10.5.2.35) |
| `L3SystemInformationType4` | 0x1c | LAI, CI, cell selection, RACH control, rest octets — same 0x2B padding rule (§10.5.2.37) |
| `L3SystemInformationType5` | 0x1d | BA list type 2 (sixteen-octet frequency list form) |
| `L3SystemInformationType5bis` | 0x05 | Extended BA list (GPRS, sixteen-octet frequency list form) |
| `L3SystemInformationType5ter` | 0x06 | BA list with GPRS cell options (sixteen-octet frequency list form) |
| `L3SystemInformationType6` | 0x1e | CI, LAI, SACCH cell options, NCC permitted |
| `L3SystemInformationType7` | 0x1f | RACH control parameters (V-form, no identifier octet) + neighbour cell description records (TV 0x21) — TS 44.018 §9.1.41 |
| `L3SystemInformationType8` | 0x18 | NCC permitted + RACH control parameters (V-form, no identifier octets) + neighbour cell description records (TV 0x21) — TS 44.018 §9.1.42 |
| `L3SystemInformationType9` | 0x04 | CI, cell selection, BCCH cell options |
| `L3SystemInformationType13` | 0x00 | Rest-octets record only: typed GPRS mobile allocation / cell options / power-control parameters (variable length, padded to the octet boundary) |
| `L3SystemInformationType2quater` | 0x07 | Extended BCCH freq list, RACH ctrl params, CBCH description (raw body) |
| `L3SystemInformationType14` | 0x01 | CellIdentity + CellSelectionParameters |
| `L3SystemInformationType15` | 0x43 | Empty body |
| `L3SystemInformationType16` | 0x3d | CI, cell selection (SACCH) |
| `L3SystemInformationType17` | 0x3e | RACH control parameters (V-form, no identifier octet) + neighbour cell description records (TV 0x21) — TS 44.018 §9.1.46 |
| `L3SystemInformationType18` | 0x40 | RACH control parameters / NCC permitted (V-form, no identifier octets) + neighbour cell description records (TV 0x21) — TS 44.018 §9.1.47 |
| `L3SystemInformationType19` | 0x41 | RACH control parameters / NCC permitted (V-form, no identifier octets) + neighbour cell description records (TV 0x21) — TS 44.018 §9.1.48 |
| `L3SystemInformationType20` | 0x42 | RACH control parameters / NCC permitted (V-form, no identifier octets) + neighbour cell description records (TV 0x21) — TS 44.018 §9.1.49 |
| `L3SystemInformationType13alt` | 0x44 | SACCH alternative format |
| `L3SystemInformationType2n` | 0x45 | Empty body |
| `L3SystemInformationType21` | 0x46 | Empty body |
| `L3SystemInformationType22` | 0x47 | Empty body |
| `L3SystemInformationType23` | 0x4f | Empty body |

*The Neighbour Cell Description lists of SI5bis/SI5ter are handled as the 16-octet BCCH frequency list form; the bit placement of BA-ind/EXT-ind is not fixed by the reference templates and no structured NCD interpretation is provided.*

### SACCH Short-Form System Information (TIF=1)

| Message | MTI | Description |
|---------|-----|-------------|
| `L3SystemInformationType10` | 0x100 | CI + LAI + CellOptions + CellSelParams (code '00000'B) |
| `L3SystemInformationType10bis` | 0x10A | CI + LAI + CellOptions + CellSelParams (code '01010'B) |
| `L3SystemInformationType10ter` | 0x10B | CI + LAI + CellOptions + CellSelParams (code '01011'B) |

### Dedicated Channel (DCCH/FACCH)

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3ChannelRelease` | 0x0D | DL | Cause [+ GPRS resumption]; trailing optional IEs (e.g. cell-selection indicator TLV 0x77) kept as an opaque tail |
| `L3ImmediateAssignment` | 0x3F | DL | dedicated mode/TBF + page mode, channel description, request reference, timing advance (raw octet, 0..219), mobile allocation (LV, mandatory on parse), [start time (TLV 0x7C: two value octets packing T1(5)/T3(6)/T2(5))]; trailing rest octets kept as an opaque tail |
| `L3ImmediateAssignmentExtended` | 0x39 | DL | Extended immediate assignment |
| `L3ImmediateAssignmentReject` | 0x3A | DL | First octet [spare(4)\|pageMode(4)] + exactly four (request reference, wait indication) pairs — fewer supplied pairs are padded by repeating the last one; body 17 octets plus any preserved trailing rest octets (opaque tail, never added on emit) |
| `L3AdditionalAssignment` | 0x3B | DL | Additional channel assignment |
| `L3PhysicalInformation` | 0x2D | DL | Timing advance command (raw octet, valid range 0..219) |
| `L3AssignmentCommand` | 0x2E | DL | Channel desc, mode, power command ([reserved(3)=0\|msPowerLevel(5)]); trailing optional IEs kept as an opaque tail |
| `L3AssignmentComplete` | 0x29 | UL | Cause |
| `L3AssignmentFailure` | 0x2F | UL | Cause |
| `L3HandoverCommand` | 0x2B | DL | Cell desc, channel desc2, HO ref, power ([reserved(3)=0\|msPowerLevel(5)]); trailing optional IEs kept as an opaque tail |
| `L3HandoverComplete` | 0x2C | UL | Cause; optional IEs after the cause kept as an opaque tail |
| `L3HandoverFailure` | 0x28 | UL | Cause; optional IEs after the cause kept as an opaque tail |
| `L3RRStatus` | 0x12 | UL | Cause |
| `L3ClassmarkChange` | 0x16 | UL | classmark2 (LV, three to seven value octets) [+ classmark3 (TLV 0x20: one-octet length + raw value octets, fourteen for a conforming value)] |
| `L3ClassmarkEnquiry` | 0x13 | DL | Empty body |
| `L3MeasurementReport` | 0x15 | UL | RxLev/RxQual + neighbors; bytes after the measurement reports kept as an opaque tail |
| `L3ExtendedMeasurementReport` | 0x36 | UL | Extended measurement results |
| `L3ExtendedMeasurementOrder` | 0x37 | DL | Measurement order |
| `L3CipheringModeCommand` | 0x35 | DL | One octet: high nibble [spare(3)\|cR(1)], low nibble [algorithm(3)\|sC(1)] — start-ciphering in the low bit, algorithm identifier bits 1–3 (TS 44.018 §9.2.24) |
| `L3CipheringModeComplete` | 0x32 | UL | No fixed fields; optional mobile equipment identity kept opaque |
| `L3ChannelModeModify` | 0x10 | DL | Channel desc + mode [+ multi-rate] |
| `L3ChannelModeModifyAcknowledge` | 0x17 | UL | Channel desc + mode |
| `L3GPRSSuspensionRequest` | 0x34 | UL | TLLI (V, four octets), RAI (V, six octets), suspension cause (one octet) [+ service support (TV, IEI 0x01, one value octet)]; body 11 or 13 octets |
| `L3ApplicationInformation` | 0x38 | DL/UL | First octet [flags(4)\|protocolId(4)] (flags: spare\|CR\|first segment\|last segment), data length octet, then the RRLP data bytes |
| `L3ConfigurationChangeCommand` | 0x30 | DL | ChanDesc + PowerCmd |
| `L3ConfigurationChangeAcknowledge` | 0x31 | UL | Empty body |
| `L3ConfigurationChangeReject` | 0x33 | UL | Cause |
| `L3PartialRelease` | 0x0a | DL | ChannelDescription |
| `L3PartialReleaseComplete` | 0x0f | UL | Empty body |
| `L3FrequencyRedefinition` | 0x14 | DL | CellChannelDescription (sixteen-octet variable BCCH frequency list form) + RACHControlParams |

### Short Messages (no standard L3 header)

| Message | MTI | Size | Description |
|---------|-----|------|-------------|
| `L3ChannelRequest` | 0x181 | 1 byte | RACH access: single-octet request reference (RA) |
| `L3HandoverAccess` | 0x182 | 4 bytes | 27-bit payload + five reserved bits (TS 44.018); the payload is kept as an opaque value |

### VGCS/VBS and Notification

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3NotificationNCH` | 0x20 | DL | Variable-length data (CBCH) |
| `L3NotificationResponse` | 0x26 | UL | Variable-length data |
| `L3VGCSUplinkGrant` | 0x09 | DL | Request reference (three octets) + timing advance (one octet); body 4 octets |
| `L3UplinkRelease` | 0x0e | DL | RR cause (one octet) |
| `L3UplinkBusy` | 0x2a | DL | All information elements optional — a header-only frame is valid; present IEs kept as an opaque tail for lossless re-emit |
| `L3TalkerIndication` | 0x11 | DL | Classmark2 (LV, three to seven value octets) + mobile identity (LV) [+ CKSN (TV, IEI 0x0B, one value octet)] |
| `L3PriorityUplinkRequest` | 0x66 | UL | Estimated cause (one octet) + token (four octets) + reduced GCR (four octets) + mobile identity (LV) |
| `L3DataIndication` | 0x67 | DL | TMSI (four octets) + application data (nine octets) + data identifier (one octet); body 14 octets |
| `L3DataIndication2` | 0x68 | DL | TMSI (four octets) + reduced GCR (four octets) + application data (nine octets) + data identifier (one octet); body 18 octets |

### DTM and Packet

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3DTMAssignmentFailure` | 0x48 | UL | Cause |
| `L3DTMReject` | 0x49 | DL | Empty body |
| `L3DTMRequest` | 0x4A | UL | Empty body |
| `L3PacketAssignment` | 0x4B | DL | ChannelDescription + TimingAdvance |
| `L3ImmediatePacketAssignment` | 0x69 | DL | Opaque variable value part (DTM packet channel) |
| `L3DTMAssignmentCommand` | 0x4C | DL | Empty body |
| `L3DTMInformation` | 0x4D | UL | Empty body |
| `L3PacketInformation` | 0x4E | DL | Empty body |

### Inter-RAT

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3UTRANClassmarkChange` | 0x60 | UL | Variable-length classmark |
| `L3CDMA2000ClassmarkChange` | 0x62 | UL | Variable-length classmark |
| `L3IntersysToUTRANHOCommand` | 0x63 | DL | Variable-length HO data |
| `L3IntersysToCDMA2000HOCommand` | 0x64 | DL | Variable-length HO data |
| `L3GERANIUClassmarkChange` | 0x65 | UL | Variable-length classmark |

### SACCH FACCH/VBS-VGCS (TIF=1)

RR short messages carry the standard L3 header with TIF set (octet 0 = 0x16 for TI=0);
the five-bit code in octet 1 maps to the internal MTI `kRRTifShortBase + code`. Codes not assigned by
TS 44.018 Table 10.4.2 (code 3 and codes 14–31) map to empty dispatcher slots and are rejected as
InvalidMTI.

| Message | MTI | Description |
|---------|-----|-------------|
| `L3NotificationFACCH` | 0x101 | FACCH notification (code '00001'B) |
| `L3UplinkFree` | 0x102 | FACCH uplink free (code '00010'B) |
| `L3EnhancedMeasurementRepUL` | 0x104 | FACCH measurement report UL (code '00100'B) |
| `L3MeasurementInfoDL` | 0x105 | FACCH measurement info DL (code '00101'B) |
| `L3VBSVGCSRecon` | 0x106 | VBS/VGCS reconfiguration (code '00110'B) |
| `L3VBSVGCSRecon2` | 0x107 | VBS/VGCS reconfiguration 2 (code '00111'B) |
| `L3VGCSAddInfo` | 0x108 | VGCS additional info (code '01000'B) |
| `L3VGCSMSInfo` | 0x109 | VGCS MS info (code '01001'B) |
| `L3VGCSSNeighCellInfo` | 0x10C | VGCS neighbor cell info (code '01100'B) |
| `L3NotifyAppData` | 0x10D | Notify application data (code '01101'B) |

## GPRS Mobility Management (PD=0x08) — 23 message types

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3AttachRequest` | 0x01 | UL | MS network capability (LV), attach type + forL3 + GPRS CKSN (one octet), DRX parameter (V, two octets), mobile identity (LV), old RAI (V, six octets), MS radio access capability (LV) |
| `L3AttachAccept` | 0x02 | DL | [forceToStandby(4)\|attachResult(4)] (bit positions in the octet value), periodic RA update timer octet ([units(3)\|value(5)]), radio priority octet ([SMS(4)\|TOM8(4)]), RAI (V, six octets); optional IEs after the RAI in any order: P-TMSI signature (TV, IEI 0x19, three octets), allocated P-TMSI (TLV 0x18, MI form), ready timer (TV 0x17, two value octets); remaining IEs kept opaque |
| `L3AttachComplete` | 0x03 | UL | Attach complete |
| `L3AttachReject` | 0x04 | DL | GMM cause (value octet); optional IEs kept opaque |
| `L3DetachRequest` | 0x05 | Bidir | First octet: detach type in the three low bits + power-off flag (MO, bit 3) or force-to-standby (MT, bit 4); [P-TMSI (TLV 0x18)], [cause (TV, IEI 0x25)] |
| `L3DetachAccept` | 0x06 | Bidir | Exactly one octet, always present: force-to-standby in the low bit, remaining bits zero |
| `L3RoutingAreaUpdateRequest` | 0x08 | UL | Update type + forL3 + GPRS CKSN (one octet), old RAI (V, six octets), MS radio access capability (LV) |
| `L3RoutingAreaUpdateAccept` | 0x09 | DL | [spare(4)\|updateResult(3)\|forceToStandby(1)] (bit positions in the octet value), periodic RA update timer octet ([units(3)\|value(5)]), RAI (V, six octets); optional IEs after the RAI in any order: P-TMSI signature (TV 0x19, three octets), allocated P-TMSI (TLV 0x18), MS identity / old P-TMSI (TLV 0x23), ready timer (TV 0x17, two value octets), PDP context status (TLV 0x32, two value octets) when included by the network; remaining IEs kept opaque |
| `L3RoutingAreaUpdateComplete` | 0x0a | UL | RA update complete |
| `L3RoutingAreaUpdateReject` | 0x0b | DL | GMM cause (value octet) + one force-to-standby octet (low bit, remaining bits zero) |
| `L3ServiceRequest` | 0x0c | UL | First octet: GPRS CKSN(3)\|spare(1) low half-octet, service type(3)\|spare(1) high half-octet; then P-TMSI (LV); further optional IEs after the P-TMSI LV kept opaque (a conforming MS appends the PDP context status TLV 0x32 here) |
| `L3ServiceAccept` | 0x0d | DL | [PDP context status (TLV 0x32)]; additional IEs kept opaque |
| `L3ServiceReject` | 0x0e | DL | Service rejected |
| `L3P_TMSIReallocationCommand` | 0x10 | DL | Positional: allocated P-TMSI LV (length 5, MI form `0xF4` + TMSI), RAI (V, six octets), one force-to-standby octet (low bit); [P-TMSI signature (TV, IEI 0x19, three octets)]; additional IEs kept opaque |
| `L3P_TMSIReallocationComplete` | 0x11 | UL | P-TMSI reallocation complete |
| `L3AuthenticationAndCipheringRequest` | 0x12 | DL | First octet [ciphering algorithm(3)\|spare(1)\|IMEISV request(1)\|force to standby(1)\|spare(2)], second octet [AC reference(4)\|spare(4)], RAND (TV, IEI 0x21, sixteen value octets) |
| `L3AuthenticationAndCipheringResponse` | 0x13 | UL | First octet [AC reference(4)\|spare(4)], RES (TV, IEI 0x22, four value octets) |
| `L3AuthenticationAndCipheringReject` | 0x14 | DL | Auth rejected |
| `L3GMMIdentityRequest` | 0x15 | DL | One octet [spare(3)\|forceToStandby(1)\|identityType(3)] (bit positions in the octet value) |
| `L3GMMIdentityResponse` | 0x16 | UL | Identity response |
| `L3AuthenticationAndCipheringFailure` | 0x1c | UL | Auth failure with AUTS |
| `L3GMMStatus` | 0x20 | Bidir | GMM status report |
| `L3GMMInformation` | 0x21 | DL | Network information |

### GMM Information Elements

| IE | Description |
|----|-------------|
| `L3PDPContextStatus` | PDP context activation bitmap (16 contexts) |
| `L3T3302Timer` | T3302 timer value |
| `L3MSNetworkCapability` | MS network capability bit string |
| `L3RoutingAreaIdentification` | MCC/MNC + LAC + RAC (6 octets) |
| `L3DRXParameter` | DRX parameter (two value octets, no identifier) |
| `L3GMMCauseIE` | GMM cause value octet (no identifier; first body octet of reject/failure/status messages) |
| `L3AuthRAND` | 128-bit authentication challenge |
| `L3AuthRES` | 32-bit authentication response |
| `L3AuthFailureParam` | AUTS failure parameter |
| `L3PTMSISignature` | P-TMSI signature (3 octets) |
| `L3GMMStatusCause` | GMM status cause octet |

Enums: `GMMCause`, `GMMAttachType`, `GMMUpdateType`, `GMMDetachTypeMO`, `GMMDetachTypeMT`.

## SMS (PD=0x09) — 5 L3 messages in the `SMS` variant, plus 4 TP types and 4 RP types

The variant holds 5 CP-layer messages. The CP-layer messages follow TS 24.011; no other L3 message types are defined for the SMS protocol discriminator. The RP/TP classes below parse payloads inside CP-DATA/CP-SMT frames (not standalone PD=0x09 dispatch).

### Control Part (CP) Messages

| Message | CP-MTI | Direction | Description |
|---------|--------|-----------|-------------|
| `L3CPData` | 0x01 | Bidir | SMS data container (wraps RPDU/TPDU) |
| `L3CPAck` | 0x04 | Bidir | CP acknowledgement |
| `L3CPErr` | 0x10 | Bidir | CP error with cause |
| `L3CPStatus` | 0x12 | MT | CP status report (SC to MS) — wins parse slot 0x12 |
| `L3CPSMT` | 0x13 | MT | Short message to telephony — wins parse slot 0x13 |

### Relay Part (RP) Messages

The RP header octet is `reserved(5)=0 | RP-MTI(3)`; the MTI is direction dependent — a downlink
(network to MS) message carries the uplink value plus one (TS 24.079 §7.2.1, Table 8.3). The reserved
high bits must be zero on parse.

| Message | RP-MTI (uplink / downlink) | Description |
|---------|----------------|-------------|
| `L3RPData` | 0 / 1 | Relay data (wraps TPDU) |
| `L3RPAck` | 2 / 3 | Relay acknowledgement |
| `L3RPError` | 4 / 5 | Relay error with cause |
| `L3RPSMMA` | 6 / 7 | Short message memory available |

### Transport Part (TP) Types

| Type | TP-MTI | Description |
|------|--------|-------------|
| `L3TPDeliver` | 0x00 | MT SMS delivery |
| `L3TPSubmit` | 0x01 | MO SMS submission |
| `L3TPStatusReport` | 0x02 | Delivery status report (wire MTI '10'B, network→mobile direction) |
| `L3TPCommand` | 0x02 | SMS command (e.g. delete; same wire MTI '10'B, mobile→network direction) |

### TP Information Elements

| IE | Description |
|----|-------------|
| `L3TPAddress` | TP-DA/TP-OA: length octet = number of digits, then TON/NPI octet and packed BCD digits ('F'-filled when odd) |
| `TPSCTimeStamp` | Service centre time stamp (7 octets) |
| `TPDCS` | Data coding scheme (Default, 8-bit, UCS2) |
| `TPPID` | Protocol identifier (GSM, X121, Telex, etc.) |

## GPRS Session Management (PD=0x0a) — 24 message types

### Primary PDP Context

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3ActivatePDPContextRequest` | 0x41 | UL | NSAPI and LLC SAPI (two octets, value in the low four bits with the high nibble zero), requested QoS (LV), requested PDP address (LV), [APN (TLV 0x28)], [PCO (TLV 0x27)], [request type (0xAx)] |
| `L3ActivatePDPContextAccept` | 0x42 | DL | PDP context activated |
| `L3ActivatePDPContextReject` | 0x43 | DL | SM cause (value octet); optional IEs kept opaque |
| `L3DeactivatePDPContextRequest` | 0x46 | Bidir | SM cause (value octet), [tear-down indicator (one octet: high nibble 0x9, flag in bit 0)], [PCO (TLV 0x27)] |
| `L3DeactivatePDPContextAccept` | 0x47 | Bidir | PDP context deactivated |
| `L3ModifyPDPContextRequest` | 0x48 | DL | Modify PDP context (QoS change) |
| `L3ModifyPDPContextAccept` | 0x49 | UL | PDP context modified |
| `L3ModifyPDPContextReject` | 0x4c | Bidir | Modification rejected |
| `L3SMStatus` | 0x55 | Bidir | SM status report |

### Request PDP Context Activation (Net-initiated)

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3RequestPDPContextActivation` | 0x44 | DL | Network requests PDP context activation |
| `L3RequestPDPContextActivationReject` | 0x45 | UL | MS rejects network request |

### Bidirectional Modify (MS-initiated variants)

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3ModifyPDPContextRequestMS` | 0x4A | UL | MS requests PDP context modification |
| `L3ModifyPDPContextAcceptNet` | 0x4B | DL | Network accepts MS modification |

### Secondary PDP Context

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3ActivateSecondaryPDPContextRequest` | 0x4D | DL | Network requests secondary PDP activation |
| `L3ActivateSecondaryPDPContextAccept` | 0x4E | UL | MS accepts secondary PDP activation |
| `L3ActivateSecondaryPDPContextReject` | 0x4F | UL | MS rejects secondary PDP activation |

### MBMS Context

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3ActivateMBMSContextRequest` | 0x56 | UL | MS requests MBMS context activation |
| `L3ActivateMBMSContextAccept` | 0x57 | DL | Network accepts MBMS activation |
| `L3ActivateMBMSContextReject` | 0x58 | DL | Network rejects MBMS activation |
| `L3RequestMBMSContextActivation` | 0x59 | DL | Network requests MBMS activation |
| `L3RequestMBMSContextActivationReject` | 0x5A | UL | MS rejects network MBMS request |

### Network-Initiated Secondary & Notification

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3RequestSecondaryPDPContextActivation` | 0x5B | DL | Network requests secondary PDP activation |
| `L3RequestSecondaryPDPContextActivationReject` | 0x5C | UL | MS rejects network secondary request |
| `L3SMNotification` | 0x5D | DL | Network notification to MS |

### SM Information Elements

| IE | Description |
|----|-------------|
| `L3PDPAddress` | PDP type-and-address (LV): value = [type organization(4) = 0x01 IETF \| spare(4)][type number(8)] + address octets only for a static address — type numbers 0x21 (IPv4, four octets) and 0x53 (IPv6, sixteen octets); LV length 2 (dynamic), 6 (static IPv4) or 18 (static IPv6) |
| `L3QoS` | QoS profile (LV, no identifier): three mandatory octets — reserved(2)\|delay class(3)\|reliability class(3), peak throughput(4)\|reserved(1)\|precedence class(3), reserved(3)\|mean throughput(5) — plus any optional characteristics, preserved verbatim |
| `L3AccessPointName` | APN string (UTF-8), TLV IEI 0x28 |
| `L3ProtocolConfigOptions` | Protocol config (e.g. IPCP for IPv4), TLV IEI 0x27 |
| `L3SMCauseIE` | SM cause value octet (no identifier; first body octet of reject/failure/status messages) |
| `L3BackOffTimer` | Back-off timer (GPRS Timer 2 encoding); kept in the message's opaque additional-IE sequence |
| `L3PDPHandle` | PDP context identifier (0–15) |
| `L3TearDownIndicator` | Tear-down indicator (one octet: high nibble 0x9, flag in bit 0 — `0x90` cleared / `0x91` set) |
| `L3TMGI` | Temporary Mobile Group Identity: PLMN(3) + ServiceID(2) + SessionID(1) |

Enums: `PdpTypeNumber`, `SMCause`.

## Supplementary Services (PD=0x0b) — 3 message types, 2 enums, 2 IEs

### Messages

| Message | MTI | Description |
|---------|-----|-------------|
| `L3SupServFacilityMessage` | 0x3a | SS-Data: TCAP component container [tag(1)][length(1)][component] — Invoke=0xA1, ReturnResult=0xA2, ReturnError=0xA3, Reject=0xA4 (TS 24.078) |
| `L3SupServRegisterMessage` | 0x3b | Registration request/response |
| `L3SupServReleaseCompleteMessage` | 0x2a | SS release complete: cause (TLV, IEI 0x08) |

### Enums

| Enum | Values | Description |
|------|--------|-------------|
| `SSOpCode` | 19 codes | TCAP operation codes (RegisterSS, EraseSS, ActivateSS, USSRequest, etc.) |
| `SSErrorCode` | 23 codes | SS error codes (UnknownSubscriber, CallBarred, SystemFailure, etc.) |

### IEs

| IE | Description |
|----|-------------|
| `L3FacilityOpCode` | TCAP component parser (Invoke/ReturnResult/ReturnError/Reject) |
| `L3USSDData` | USSD message with GSM 7-bit encode/decode, UCS2, DCS handling |

## Location Services (PD=0x0c) — 2 message types

No normative reference templates exist for this block: bodies are kept as raw octet sequences
(best-effort parsing), and bit-level conformance is not claimed.

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3LocationServiceRequest` | 0x01 | Bidir | Location service request parameters |
| `L3LocationServiceProviderMessage` | 0x02 | Bidir | Location service provider data |

## Packet Domain Supplementary Services 1 (PD=0x0d) — 1 placeholder type

| Message | Description |
|---------|-------------|
| `L3PDSS1Message` | Raw-body passthrough for PD=0x0d; MTI captured at parse time, body opaque |

## Packet Domain Supplementary Services 2 (PD=0x0e) — 1 placeholder type

| Message | Description |
|---------|-------------|
| `L3ExtendedMessage` | Raw-body passthrough for the packet-domain supplementary services 2 discriminator; MTI captured at parse time, body opaque |

## Test Procedure PD (PD=0x0f) — 1 placeholder type

| Message | Description |
|---------|-------------|
| `L3TestProcedureMessage` | Raw-body catch-all for test procedure messages; MTI captured at parse time, body opaque |
