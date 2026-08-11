# libgsml3parser

**GSM Layer 3 Signalling Message Parser — Standalone C++20 Library**

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Build](https://github.com/momentics/libgsml3parser/actions/workflows/build-release.yml/badge.svg)](https://github.com/momentics/libgsml3parser/actions/workflows/build-release.yml)
[![Version](https://img.shields.io/badge/Version-0.8.0-blue.svg)](https://github.com/momentics/libgsml3parser/releases)

## Overview

libgsml3parser is a standalone C++20 library for parsing and generating GSM Layer 3 (L3) signalling messages. It implements protocol discriminator dispatch across nine domains, as defined by GSM 04.08 / 3GPP TS 24.008:

| Domain | PD | Spec Section | Messages |
|--------|----|-------------|----------|
| **Group Call Control (GCC)** | `0x00` | 3GPP TS 44.018 9.7 | Setup, Acknowledge, Proceeding, Connect, Disconnect, Release, Release Complete |
| **Broadcast Call Control (BCC)** | `0x01` | 3GPP TS 44.018 9.6 | Setup, Proceeding, Connect, Disconnect, Release, Release Complete |
| **Call Control (CC)** | `0x03` | GSM 04.08 9.3 / ISDN Q.931 | Setup, Connect, Disconnect, Release, DTMF, Hold, Progress (+ 26 CC IEs) |
| **Mobility Management (MM)** | `0x05` | GSM 04.08 9.2 | Location Updating, Authentication, Identity, CM Service, TMSI Reallocation |
| **Radio Resource (RR)** | `0x06` | GSM 04.08 9.1 | Paging, System Information (SI1–SI23), Handover, Assignment, Ciphering, etc. |
| **GPRS Mobility Mgmt (GMM)** | `0x08` | GSM 04.08 9.4 / Table 10.4 | Attach, Detach, RA Update, Service Request, P-TMSI Reallocation, Auth/Ciphering, Identity, Status, Information |
| **SMS** | `0x09` | GSM 24.011 / 3GPP TS 23.040 | CP-DATA/ACK/ERROR/STATUS/SMT, RP-DATA/ACK/ERROR/SMMA, TP-Deliver/Submit/StatusReport/Command |
| **GPRS Session Mgmt (SM)** | `0x0a` | GSM 24.008 9.5 / Table 10.4a | Activate/Deactivate/Modify PDP Context (Request/Accept/Reject), SM Status |
| **Supplementary Services (SS)** | `0x0b` | GSM 04.80 / 3GPP TS 24.080 | Facility, Register, Release Complete (+ SSOpCode/SSErrorCode enums, L3FacilityOpCode IE, L3USSDDate IE with GSM 7-bit encode/decode) |

The library is self-contained with zero external dependencies beyond the C++20 standard library. It provides bidirectional parsing (binary to typed C++ objects and back), human-readable output, `Expected<T>` result types, immutable configuration, compile-time message dispatch via `std::variant` + `std::visit`, and a streaming bitstream I/O layer. `ParsedMessage` spans 9 protocol domains on the stack (`sizeof < 8 KB`).

## Features

- **Full L3 message parsing** — Binary data to typed C++ objects with compile-time dispatch via `std::variant`
- **Message generation** — Typed C++ objects to binary data (for test harnesses, fuzzing, replay)
- **Expected<T> result type** — Zero-allocation errors with structured error codes and bit-position tracking
- **Immutable ParserConfig** — No mutex on parse path, thread-safe by design
- **Zero heap allocation on hot path** — `ParsedMessage` variant on stack, no `std::unique_ptr` in parse/serialise
- **Compile-time message dispatch** — `std::variant` + `std::visit`, no `dynamic_cast`, no RTTI
- **Bit-level I/O** — Bounds-checked `BitReader`/`BitWriter`, MSB-first bit ordering
- **Bitstream I/O** — `ByteSource` hierarchy (Span, File, RingBuffer) for streaming from any source
- **L3Framer** — Automatic frame boundary detection in raw byte streams
- **L3StreamProcessor** — High-throughput streaming parser with `FrameHandler` callback interface
- **Human-readable output** — Every message type has a `.text()` method for logging and debugging
- **PD handler registry** — Custom handlers for unsupported PD domains via immutable config builder
- **Arena allocator** — Bump allocator for high-throughput batch parsing
- **Zero external dependencies** — No networking, no SIP, no radio stack
- **Fuzzing-ready** — Clean parse/generate API suitable for libFuzzer integration
- **Comprehensive test suite** — Golden test vectors cross-validated against osmo-ttcn3-hacks TTCN-3 reference
- **Spec-compliant** — Follows GSM 04.08 / 3GPP TS 24.008, GSM 04.07 / 3GPP TS 24.007, GSM 04.80 / 3GPP TS 24.080, 3GPP TS 44.018
- **V/TV/TLV/LV formats** — Correct handling of all GSM 04.07 IE encoding formats
- **Short messages** — Synchronization Channel Information, Channel Request, Handover Access

## Quick Start

### Building

```bash
mkdir build && cd build
cmake .. -DBUILD_TESTS=ON -DBUILD_EXAMPLES=ON
cmake --build . --config Release --parallel
```

CMake options:

| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_SHARED_LIBS` | OFF | Build shared library instead of static |
| `BUILD_TESTS` | OFF | Build unit tests (Google Test 1.14.0) |
| `BUILD_EXAMPLES` | OFF | Build example programs |
| `ENABLE_FUZZING` | OFF | Build fuzzing target |

### Installing

```bash
cmake --install .
```

### Using in Your Project

```cmake
find_package(gsml3parser REQUIRED)
target_link_libraries(myapp PRIVATE gsml3parser::parser)
```

### Parsing a Message

```cpp
#include <gsml3parser/gsml3parser.hpp>
#include <iostream>

int main() {
    // Parse from hex string (std::string_view)
    auto msg = gsml3parser::parseL3Hex("060D00");

    if (msg) {
        // Compile-time typed access — no dynamic_cast needed
        if (auto* cr = gsml3parser::tryGet<gsml3parser::L3ChannelRelease>(*msg)) {
            std::cout << "Channel Release, cause: "
                      << static_cast<int>(cr->cause()) << "\n";
        }

        // Message metadata helpers
        std::cout << "Name: " << gsml3parser::messageName(*msg) << "\n";
        std::cout << "PD: " << static_cast<int>(gsml3parser::messagePD(*msg)) << "\n";
        std::cout << "MTI: 0x" << std::hex << gsml3parser::messageMTI(*msg) << "\n";
    } else {
        std::cerr << "Parse error: " << msg.error().message << "\n";
    }
}
```

### Parsing from Raw Bytes

```cpp
#include <gsml3parser/gsml3parser.hpp>
#include <span>

int main() {
    std::span<const uint8_t> data{
        0x50, 0x84  // CM Service Accept (MM) — PD=0x05 high nibble, NSD=1
    };

    auto msg = gsml3parser::parseL3(data);

    if (msg) {
        std::cout << gsml3parser::messageName(*msg) << "\n";
    }
}
```

### Generating a Message

```cpp
#include <gsml3parser/gsml3parser.hpp>
#include <iostream>

int main() {
    // Parse a message, then serialize back to hex
    auto msg = gsml3parser::parseL3Hex("060D00");

    if (msg) {
        auto hex = gsml3parser::writeL3Hex(*msg);
        if (hex) {
            std::cout << "Serialized: " << *hex << "\n";
        }
    }
}
```

### Using the Builder Pattern for CC Messages

```cpp
#include <gsml3parser/gsml3parser.hpp>

    // Build a Disconnect message
    gsml3parser::L3Disconnect disconnect(gsml3parser::CCCause::Normal_Call_Clearing);

// Serialize to hex string
auto hex = gsml3parser::writeL3Hex(disconnect);
```

### Registering a Custom PD Handler

Custom handlers for unsupported Protocol Discriminators can be registered via an immutable `ParserConfig` builder. The handler receives the raw L3 header and body bytes, allowing you to parse with `BitReader`:

```cpp
#include <gsml3parser/gsml3parser.hpp>

// Create immutable config with custom handler for SMS (PD=0x09)
gsml3parser::ParserConfig cfg;
cfg = cfg.withPDHandler(gsml3parser::L3PD::SMS,
    /* PDHandler callback — receives L3Header + body span */);

// Parse with the configured parser
auto result = gsml3parser::parseL3(data, cfg);
```

### Streaming with RingBuffer

```cpp
#include <gsml3parser/gsml3parser.hpp>

gsml3parser::RingBuffer ring(262144);  // 256 KB ring buffer

// Producer: feed data from SDR or network callback
ring.write(incomingBytes.data(), incomingBytes.size());

// Consumer: process frames
gsml3parser::L3StreamProcessor processor(ring);

processor.processUntilEOF(gsml3parser::FrameHandler{
    /* implement onFrame / onError */
});
```

### Multi-threaded Parsing

```cpp
#include <gsml3parser/gsml3parser.hpp>
#include <thread>

// Each thread uses its own immutable ParserConfig — no mutex needed.
void parseThread(std::vector<uint8_t> frames) {
    gsml3parser::ParserConfig cfg;
    cfg = cfg.withLogLevel(gsml3parser::LogLevel::ERR);

    for (const auto& frame : frames) {
        auto result = gsml3parser::parseL3({&frame, 1}, cfg);
        if (result) {
            // ... process *result ...
        }
    }
}
```

## Architecture

```
ByteSource (Span/File/RingBuffer)
    → L3Framer (frame boundary detection)
    → parseL3() → Expected<ParsedMessage>
    → std::visit / tryGet<T>() for typed access
```

The library follows a layered design:

1. **Bit-level I/O** — `BitReader` and `BitWriter` provide bounds-checked, MSB-first bit operations over byte buffers. No heap allocation.

2. **Message types** — Each L3 message is a plain C++ struct with `parse(BitReader&)` and `write(BitWriter&)` methods. No inheritance hierarchies for IEs.

3. **Variant dispatch** — `ParsedMessage = std::variant<RRM, MMM, CCM, SSM, GMM, SM, SMS, BCCM, GCCM>` holds the parsed result on the stack (9 domains). `tryGet<T>()` provides compile-time typed access.

4. **Streaming** — `ByteSource` → `L3Framer` → `L3StreamProcessor` pipeline processes raw byte streams with automatic frame boundary detection.

## Supported Messages

### Group Call Control (PD=0x00) — 7 message types

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3GCCSetup` | 0x00 | MO | Group call setup request |
| `L3GCCProceeding` | 0x01 | MT | Network proceeding indication |
| `L3GCCAcknowledge` | 0x02 | MT | Group call acknowledgement |
| `L3GCCConnect` | 0x05 | MT | Group call connected |
| `L3GCCDisconnect` | 0x06 | MO | Group call disconnect request |
| `L3GCCRelease` | 0x07 | MT | Group call release |
| `L3GCCReleaseComplete` | 0x0a | Bidir | Group call release complete |

### Broadcast Call Control (PD=0x01) — 6 message types

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3BCCSetup` | 0x00 | MO | Broadcast call setup request |
| `L3BCCProceeding` | 0x01 | MT | Network proceeding indication |
| `L3BCCConnect` | 0x05 | MT | Broadcast call connected |
| `L3BCCDisconnect` | 0x06 | MO | Broadcast call disconnect request |
| `L3BCCRelease` | 0x07 | MT | Broadcast call release |
| `L3BCCReleaseComplete` | 0x0a | Bidir | Broadcast call release complete |

### Call Control (PD=0x03) — 20 message types, 26 IE types

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
| `L3CCStatus` | 0x3d | Bidir | CC status report |

**CC Information Elements (26 types):**

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
| `L3CauseElement` | TLV | CC cause code + location + diagnostic |
| `L3CallState` | V | Call state flags (speech, DTMF, hold) |
| `L3ProgressIndicator` | TLV | Progress cause and location |
| `L3KeypadFacility` | TV | DTMF digit indicator |
| `L3Signal` | TV | Signal type indicator |
| `L3RepeatIndicator` | TV | Repeat count for keypad DTMF |
| `L3CLIRSuppression` | TV | CLIR suppression |
| `L3CLIRInvocation` | TV | CLIR invocation |
| `L3NetworkCCCapabilities` | TLV | Network CC capabilities |
| `L3LowLayerCompatibility` | TLV | Low layer compatibility |
| `L3HighLayerCompatibility` | TLV | High layer compatibility |
| `L3UserUser` | TLV | User-User information element |
| `L3Priority` | TV | Priority level and request flag |
| `L3StreamIdentifier` | TV | VBS/VGCS stream identifier |
| `L3AllowedActions` | TLV | Allowed actions bitmask |
| `L3CCCapabilities` | TLV | CC capabilities |
| `L3SupServFacilityIE` | TLV | Supplementary service facility data |
| `L3SupServVersionIndicator` | V | SS version indicator |

### Mobility Management (PD=0x05) — 18 message types

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3IMSIDetachIndication` | 0x01 | UL | IMSI detach indication |
| `L3LocationUpdatingAccept` | 0x02 | DL | Location updating accepted |
| `L3LocationUpdatingReject` | 0x04 | DL | Location updating rejected |
| `L3LocationUpdatingRequest` | 0x08 | UL | Location update request |
| `L3CMServiceAccept` | 0x21 | DL | CM service accepted |
| `L3CMServiceReject` | 0x22 | DL | CM service rejected |
| `L3CMServiceAbort` | 0x23 | DL | CM service aborted |
| `L3CMServiceRequest` | 0x24 | UL | CM service request |
| `L3CMReestablishmentRequest` | 0x28 | UL | CM re-establishment request |
| `L3MMStatus` | 0x31 | Bidir | MM status report |
| `L3MMInformation` | 0x32 | DL | Network information broadcast |
| `L3AuthenticationRequest` | 0x12 | DL | Authentication challenge (RAND) |
| `L3AuthenticationResponse` | 0x14 | UL | Authentication response (SRES) |
| `L3AuthenticationReject` | 0x11 | DL | Authentication rejected |
| `L3IdentityRequest` | 0x18 | DL | Identity request (IMSI/IMEI) |
| `L3IdentityResponse` | 0x19 | UL | Identity response |
| `L3TMSIReallocationCommand` | 0x1A | DL | New TMSI assignment |
| `L3TMSIReallocationComplete` | 0x1B | UL | TMSI reallocation complete |

### Radio Resource (PD=0x06) — 95 message types

**Paging:**

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3PagingRequestType1` | 0x21 | DL | PageMode + MobileIdentity [+ second ID] |
| `L3PagingRequestType2` | 0x22 | DL | PageMode + TMSI (4 bytes) |
| `L3PagingRequestType3` | 0x24 | DL | PageMode + IMSI/IMEI digits |
| `L3PagingResponse` | 0x27 | UL | MobileIdentity [+ Classmark2/3] |

**System Information (BCCH):**

| Message | MTI | Description |
|---------|-----|-------------|
| `L3SystemInformationType1` | 0x19 | Cell access parameters, CBCH flag |
| `L3SystemInformationType2` | 0x1a | BCCH freq list, NCC permitted, RACH control |
| `L3SystemInformationType2bis` | 0x1f | Extended BCCH freq list (GPRS) |
| `L3SystemInformationType2ter` | 0x14 | BCCH freq list with GPRS cell options |
| `L3SystemInformationType3` | 0x1b | Cell desc, BA list type 1, rest octets |
| `L3SystemInformationType4` | 0x1c | LAI, CI, cell selection, RACH control |
| `L3SystemInformationType5` | 0x1d | BA list type 2 |
| `L3SystemInformationType5bis` | 0x20 | Extended BA list (GPRS) |
| `L3SystemInformationType5ter` | 0x23 | BA list with GPRS cell options |
| `L3SystemInformationType6` | 0x1e | CI, LAI, SACCH cell options, NCC permitted |
| `L3SystemInformationType7` | 0x15 | BA list type 3 |
| `L3SystemInformationType8` | 0x16 | NCC permitted (SACCH) |
| `L3SystemInformationType9` | 0x17 | CI, cell selection, BCCH cell options |
| `L3SystemInformationType10` | — | Short: CI + LAI + CellOptions + CellSelParams |
| `L3SystemInformationType10bis` | — | Short: CI + LAI + CellOptions + CellSelParams |
| `L3SystemInformationType10ter` | — | Short: CI + LAI + CellOptions + CellSelParams |
| `L3SystemInformationType13` | 0x00 | Cell desc, BA list type 1, rest octets |
| `L3SystemInformationType13alt` | 0x44 | SACCH alternative format |
| `L3SystemInformationType14` | 0x01 | CellIdentity + CellSelectionParameters |
| `L3SystemInformationType15` | 0x43 | Empty body |
| `L3SystemInformationType16` | 0x01 | CI, cell selection (SACCH) |
| `L3SystemInformationType17` | 0x04 | NCC permitted (SACCH extended) |
| `L3SystemInformationType18` | 0x40 | RACHControl + CellChannelDescriptions |
| `L3SystemInformationType19` | 0x41 | RACHControl + CellChannelDescriptions |
| `L3SystemInformationType20` | 0x42 | RACHControl + CellChannelDescriptions |
| `L3SystemInformationType2n` | 0x45 | Empty body |
| `L3SystemInformationType21` | 0x46 | Empty body |
| `L3SystemInformationType22` | 0x47 | Empty body |
| `L3SystemInformationType23` | 0x4f | Empty body |

**Dedicated Channel (DCCH/FACCH):**

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3ChannelRelease` | 0x0D | DL | Cause [+ GPRS resumption] |
| `L3ImmediateAssignment` | 0x3F | DL | PageMode, channel desc, TA, mobile alloc |
| `L3ImmediateAssignmentExtended` | — | DL | Extended immediate assignment |
| `L3ImmediateAssignmentReject` | 0x3A | DL | Wait indication entries |
| `L3AdditionalAssignment` | 0x01 | DL | Additional channel assignment |
| `L3PhysicalInformation` | 0x26 | DL | Timing advance command |
| `L3AssignmentCommand` | 0x2E | DL | Channel desc, mode, power command |
| `L3AssignmentComplete` | 0x29 | UL | Cause |
| `L3AssignmentFailure` | 0x2F | UL | Cause |
| `L3HandoverCommand` | 0x2B | DL | Cell desc, channel desc2, HO ref, power |
| `L3HandoverComplete` | 0x2C | UL | Cause |
| `L3HandoverFailure` | 0x28 | UL | Cause |
| `L3RRStatus` | 0x12 | UL | Cause |
| `L3ClassmarkChange` | 0x16 | UL | Classmark2/3 |
| `L3ClassmarkEnquiry` | 0x13 | DL | Empty body |
| `L3MeasurementReport` | 0x15 | UL | RxLev/RxQual + neighbors |
| `L3ExtendedMeasurementReport` | 0x36 | UL | Extended measurement results |
| `L3ExtendedMeasurementOrder` | 0x37 | DL | Measurement order |
| `L3CipheringModeCommand` | 0x35 | DL | Ciphering setting + key seq |
| `L3CipheringModeComplete` | 0x32 | UL | Empty body |
| `L3ChannelModeModify` | 0x10 | DL | Channel desc + mode [+ multi-rate] |
| `L3ChannelModeModifyAcknowledge` | 0x11 | UL | Channel desc + mode |
| `L3GPRSSuspensionRequest` | 0x34 | UL | TLLI, RA ID, suspension cause |
| `L3ApplicationInformation` | 0x38 | DL/UL | RRLP encapsulation data |
| `L3ConfigurationChangeCommand` | 0x30 | DL | ChanDesc + PowerCmd |
| `L3ConfigurationChangeAcknowledge` | 0x31 | UL | Empty body |
| `L3ConfigurationChangeReject` | 0x33 | UL | Cause |
| `L3PartialRelease` | 0x0a | DL | ChannelDescription |
| `L3PartialReleaseComplete` | 0x0f | UL | Empty body |
| `L3FrequencyRedefinition` | 0x14 | DL | CellChannelDesc + RACHControlParams |

**Short Messages (no standard L3 header):**

| Message | Size | Description |
|---------|------|-------------|
| `L3ChannelRequest` | 1 byte | RACH access with cause + TSC |
| `L3HandoverAccess` | 4 bytes | Handover confirmation with HO reference |
| `L3SynchronizationChannelInformation` | 7 bytes | SCH info with FN, TOA, BSIC |

**VGCS/VBS and Notification:**

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3NotificationNCH` | 0x20 | DL | Variable-length data (CBCH) |
| `L3NotificationResponse` | 0x26 | UL | Variable-length data |
| `L3VGCSUplinkGrant` | 0x09 | DL | Empty body |
| `L3UplinkRelease` | 0x0e | DL | Empty body |
| `L3UplinkBusy` | 0x2a | DL | Empty body |
| `L3TalkerIndication` | 0x11 | DL | Empty body |
| `L3PriorityUplinkRequest` | 0x66 | UL | TMSI (4 octets) |
| `L3DataIndication` | 0x67 | DL | Variable-length data |
| `L3DataIndication2` | 0x68 | DL | Variable-length data |

**DTM and Packet:**

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3DTMAssignmentFailure` | 0x80 | UL | Cause |
| `L3DTMReject` | 0x81 | DL | Empty body |
| `L3DTMRequest` | 0x82 | UL | Empty body |
| `L3PacketAssignment` | 0x83 | DL | ChannelDescription + TimingAdvance |
| `L3DTMAssignmentCommand` | 0x84 | DL | Empty body |
| `L3DTMInformation` | 0x85 | UL | Empty body |
| `L3PacketInformation` | 0x86 | DL | Empty body |

**Inter-RAT:**

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3UTRANClassmarkChange` | 0x60 | UL | Variable-length classmark |
| `L3CDMA2000ClassmarkChange` | 0x62 | UL | Variable-length classmark |
| `L3IntersysToUTRANHOCommand` | 0x63 | DL | Variable-length HO data |
| `L3IntersysToCDMA2000HOCommand` | 0x64 | DL | Variable-length HO data |
| `L3GERANIUClassmarkChange` | 0x65 | UL | Variable-length classmark |

**FACCH and VBS/VGCS:**

| Message | Description |
|---------|-------------|
| `L3NotificationFACCH` | FACCH notification |
| `L3UplinkFree` | FACCH uplink free |
| `L3EnhancedMeasurementRepUL` | FACCH measurement report UL |
| `L3MeasurementInfoDL` | FACCH measurement info DL |
| `L3VBSVGCSRecon` | VBS/VGCS reconfiguration |
| `L3VBSVGCSRecon2` | VBS/VGCS reconfiguration 2 |
| `L3VGCSAddInfo` | VGCS additional info |
| `L3VGCSMSInfo` | VGCS SMS info |
| `L3VGCSSNeighCellInfo` | VGCS neighbor cell info |
| `L3NotifyAppData` | Notify application data |

### GPRS Mobility Management (PD=0x08) — 19 message types

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3AttachRequest` | 0x01 | UL | GPRS attach request |
| `L3AttachAccept` | 0x02 | DL | GPRS attach accepted |
| `L3AttachComplete` | 0x03 | UL | Attach complete |
| `L3AttachReject` | 0x04 | DL | Attach rejected |
| `L3DetachRequest` | 0x05 | Bidir | Detach request |
| `L3DetachAccept` | 0x06 | Bidir | Detach accepted |
| `L3RoutingAreaUpdateRequest` | 0x08 | UL | RA update request |
| `L3RoutingAreaUpdateAccept` | 0x09 | DL | RA update accepted |
| `L3RoutingAreaUpdateComplete` | 0x0a | UL | RA update complete |
| `L3RoutingAreaUpdateReject` | 0x0b | DL | RA update rejected |
| `L3ServiceRequest` | 0x0c | UL | Packet service request |
| `L3ServiceAccept` | 0x0d | DL | Service accepted |
| `L3ServiceReject` | 0x0e | DL | Service rejected |
| `L3P_TMSIReallocationCommand` | 0x10 | DL | P-TMSI reallocation |
| `L3P_TMSIReallocationComplete` | 0x11 | UL | P-TMSI reallocation complete |
| `L3AuthenticationAndCipheringRequest` | 0x12 | DL | Auth + ciphering challenge |
| `L3AuthenticationAndCipheringResponse` | 0x13 | UL | Auth + ciphering response |
| `L3AuthenticationAndCipheringReject` | 0x14 | DL | Auth rejected |
| `L3GMMIdentityRequest` | 0x15 | DL | Identity request (IMSI/IMEI) |
| `L3GMMIdentityResponse` | 0x16 | UL | Identity response |
| `L3AuthenticationAndCipheringFailure` | 0x1c | UL | Auth failure with AUTS |
| `L3GMMStatus` | 0x20 | Bidir | GMM status report |
| `L3GMMInformation` | 0x21 | DL | Network information |

**GMM Information Elements:**

| IE | Description |
|----|-------------|
| `L3PDPContextStatus` | PDP context activation bitmap (16 contexts) |
| `L3T3302Timer` | T3302 timer value |
| `L3MSNetworkCapability` | MS network capability bit string |
| `L3RoutingAreaIdentification` | MCC/MNC + LAC + RAC (6 octets) |
| `L3DRXParameter` | DRX cycle code and timer settings |
| `L3GMMCKSN` | Ciphering key sequence number |
| `L3GMMCauseIE` | GMM cause value |
| `L3AuthRAND` | 128-bit authentication challenge |
| `L3AuthRES` | 32-bit authentication response |
| `L3AuthFailureParam` | AUTS failure parameter |
| `L3PTMSISignature` | P-TMSI signature (3 octets) |
| `L3GMMStatusCause` | GMM status cause octet |

### SMS (PD=0x09) — 5 CP messages, 4 TP types, 4 RP types

**Control Part (CP) Messages:**

| Message | CP-MTI | Direction | Description |
|---------|--------|-----------|-------------|
| `L3CPData` | 0x01 | Bidir | SMS data container (wraps RPDU/TPDU) |
| `L3CPAck` | 0x04 | Bidir | CP acknowledgement |
| `L3CPErr` | 0x10 | Bidir | CP error with cause |
| `L3CPStatus` | 0x12 | MT | CP status report (SC to MS) |
| `L3CPSMT` | 0x13 | MT | Short message to telephony |

**Transport Part (TP) Types:**

| Type | TP-MTI | Description |
|------|--------|-------------|
| `L3TPDeliver` | 0x00 | MT SMS delivery |
| `L3TPSubmit` | 0x01 | MO SMS submission |
| `L3TPStatusReport` | 0x02 | Delivery status report |
| `L3TPCommand` | 0x03 | SMS command (e.g. delete) |

**Relay Part (RP) Messages:**

| Message | RP-MTI | Description |
|---------|--------|-------------|
| `L3RPData` | MO=0, MT=1 | Relay data (wraps TPDU) |
| `L3RPAck` | MO=2, MT=3 | Relay acknowledgement |
| `L3RPError` | MO=4, MT=5 | Relay error with cause |
| `L3RPSMMA` | MO=6, MT=7 | Short message memory available |

**TP Information Elements:**

| IE | Description |
|----|-------------|
| `L3TPAddress` | TP-DA/TP-OA: TON/NPI + BCD digits |
| `TPSCTimeStamp` | Service centre time stamp (7 octets) |
| `TPDCS` | Data coding scheme (Default, 8-bit, UCS2) |
| `TPPID` | Protocol identifier (GSM, X121, Telex, etc.) |

### GPRS Session Management (PD=0x0a) — 9 message types

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3ActivatePDPContextRequest` | 0x41 | UL | Activate PDP context request |
| `L3ActivatePDPContextAccept` | 0x42 | DL | PDP context activated |
| `L3ActivatePDPContextReject` | 0x43 | DL | PDP context activation rejected |
| `L3DeactivatePDPContextRequest` | 0x46 | Bidir | Deactivate PDP context request |
| `L3DeactivatePDPContextAccept` | 0x47 | Bidir | PDP context deactivated |
| `L3ModifyPDPContextRequest` | 0x48 | DL | Modify PDP context (QoS change) |
| `L3ModifyPDPContextAccept` | 0x49 | UL | PDP context modified |
| `L3ModifyPDPContextReject` | 0x4c | Bidir | Modification rejected |
| `L3SMStatus` | 0x55 | Bidir | SM status report |

**SM Information Elements:**

| IE | Description |
|----|-------------|
| `L3PDPAddress` | PDP type (IPv4/IPv6/PPP) + address |
| `L3QoS` | QoS profile (type + 18 element types) |
| `L3AccessPointName` | APN string (UTF-8) |
| `L3ProtocolConfigOptions` | Protocol config (e.g. IPCP for IPv4) |
| `L3SMCauseIE` | SM cause value |
| `L3BackOffTimer` | Back-off timer (GPRS Timer 2 encoding) |
| `L3PDPHandle` | PDP context identifier (0–15) |

### Supplementary Services (PD=0x0b) — 3 message types, 2 enums, 2 IEs

**Messages:**

| Message | Description |
|---------|-------------|
| `L3SupServFacilityMessage` | SS facility data (TLV) |
| `L3SupServRegisterMessage` | Registration request/response |
| `L3SupServReleaseCompleteMessage` | SS release complete |

**Enums:**

| Enum | Values | Description |
|------|--------|-------------|
| `SSOpCode` | 19 codes | TCAP operation codes (RegisterSS, EraseSS, ActivateSS, USSRequest, etc.) |
| `SSErrorCode` | 23 codes | SS error codes (UnknownSubscriber, CallBarred, SystemFailure, etc.) |

**IEs:**

| IE | Description |
|----|-------------|
| `L3FacilityOpCode` | TCAP component parser (Invoke/ReturnResult/ReturnError/Reject) |
| `L3USSDData` | USSD message with GSM 7-bit encode/decode, UCS2, DCS handling |

## Error Handling

The parser API returns `Expected<T>` instead of raw pointers or exceptions, providing structured error information with zero heap allocation:

```cpp
auto result = gsml3parser::parseL3Hex("060D");

if (result) {
    // Success — access the message via operator*
    const auto& msg = *result;
    std::cout << gsml3parser::messageName(msg) << "\n";
} else {
    // Failure — inspect the error details
    const auto& err = result.error();
    std::cerr << "Error " << static_cast<int>(err.code)
              << " at bit " << err.bitPosition
              << ": " << err.message << "\n";
}
```

Error codes (`ParseError::Code`):

| Code | Meaning |
|------|---------|
| `Ok` | Parse succeeded |
| `TruncatedInput` | Input data too short for the message |
| `InvalidPD` | Unknown or unsupported Protocol Discriminator |
| `InvalidMTI` | Message Type Indicator not recognized |
| `LengthMismatch` | Declared length does not match actual data |
| `InvalidIE` | Malformed Information Element |
| `InvalidValue` | Field value outside valid range |
| `UnsupportedFeature` | Feature not yet implemented |

## Thread Safety

The library is designed for multi-threaded use:

- **`ParserConfig`** — Immutable configuration struct. No mutex, no atomic operations. Safe for concurrent read access from any number of threads. Builder methods (`withLogLevel`, `withPDHandler`) return new config instances.
- **`BitReader`/`BitWriter`** — Plain value types with no shared state. Each thread creates its own instance.
- **`parseL3()`** — Stateless function. Thread-safe when called with separate `ParserConfig` instances or a shared read-only config.
- **`Arena`** — NOT thread-safe. Each thread must use its own `Arena` instance.

For maximum performance in multi-threaded parsers, create one `ParserConfig` per thread:

```cpp
void parseThread(std::span<const uint8_t> frames) {
    gsml3parser::ParserConfig cfg;

    for (auto frame : frames) {
        auto result = gsml3parser::parseL3(frame, cfg);
        if (result) {
            // ... process result ...
        }
    }
}
```

## Memory Management

The library follows High-Performance Library (HPL) memory conventions:

| Component | Ownership | Lifetime |
|-----------|-----------|----------|
| `Expected<T>` | Stack-allocated | Automatic (RAII) |
| `ParsedMessage` variant | Stack-allocated | Automatic (RAII) |
| `BitReader`/`BitWriter` | Non-owning views | Caller ensures buffer validity |
| `Arena` | Thread-local bump allocator | Until `Arena::reset()` |

No heap allocation occurs on the parse or serialise hot path. All message objects are stored in the `ParsedMessage` variant on the stack.

## Testing

```bash
cmake .. -DBUILD_TESTS=ON
cmake --build . --config Release --parallel
ctest --output-on-failure
```

Golden test vectors are cross-validated against the osmo-ttcn3-hacks TTCN-3 reference testing suite (L3_Templates.ttcn, GSM_RR_Types.ttcn, GSM_Types.ttcn, BTS_Tests.ttcn, GSM_SystemInformation.ttcn).

## Build Requirements

| Requirement | Minimum Version |
|-------------|----------------|
| C++ compiler | GCC 11+, Clang 10+, MSVC 2022 17.3+ |
| CMake | 3.20 |
| Standard Library | C++20 (libstdc++ or libc++) |

## CI / Release

A GitHub Actions workflow builds and tests the library on every release, producing static and shared library archives for Linux x86_64. See [.github/workflows/build-release.yml](.github/workflows/build-release.yml).

## Roadmap

- [ ] Fuzzing target (libFuzzer integration)
- [ ] C API wrapper for FFI
- [ ] Python bindings (pybind11)
- [ ] Location services parser (PD=0x0c) — 3GPP TS 24.027 / 24.028

## License

This software is distributed under the terms of the MIT License.
See the [COPYING](COPYING) file for details.

## Acknowledgments

- Copyright 2026 momentics &lt;momentics@gmail.com&gt;
- Copyright libgsml3parser contributors
- Golden test vectors validated against the [Osmocom](https://osmocom.org/) TTCN-3 testing infrastructure
