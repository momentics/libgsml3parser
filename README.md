# libgsml3parser

**GSM Layer 3 Signalling Message Parser — Standalone C++20 Library**

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Build](https://github.com/momentics/libgsml3parser/actions/workflows/build-release.yml/badge.svg)](https://github.com/momentics/libgsml3parser/actions/workflows/build-release.yml)
[![Version](https://img.shields.io/badge/Version-0.7.0-blue.svg)](https://github.com/momentics/libgsml3parser/releases)

## Overview

libgsml3parser is a standalone C++20 library for parsing and generating GSM Layer 3 (L3) signalling messages. It implements protocol discriminator dispatch across four domains, as defined by GSM 04.08 / 3GPP TS 24.008:

| Domain | PD | Spec Section | Messages |
|--------|----|-------------|----------|
| **Radio Resource (RR)** | `0x06` | GSM 04.08 9.1 | Paging, System Information (SI1–SI17), Handover, Assignment, Ciphering, etc. |
| **Mobility Management (MM)** | `0x05` | GSM 04.08 9.2 | Location Updating, Authentication, Identity, CM Service, TMSI Reallocation |
| **Call Control (CC)** | `0x03` | GSM 04.08 9.3 / ISDN Q.931 | Setup, Connect, Disconnect, Release, DTMF, Hold, Progress (+ 26 CC IEs: ConnectedNumber, SubAddress, RedirectingNumber, CLIR Sup/Invoke, NetworkCCCapabilities, LayerCompatibility, UserUser, Priority, StreamIdentifier, AllowedActions, CCCapabilities, BackupBearerCapability) |
| **Supplementary Services (SS)** | `0x0b` | GSM 04.80 / 3GPP TS 24.080 | Facility, Register, Release Complete (+ SSOpCode/SSErrorCode enums, L3FacilityOpCode IE, L3USSDData IE with GSM 7-bit encode/decode) |

The library is self-contained with zero external dependencies beyond the C++20 standard library. It provides bidirectional parsing (binary to typed C++ objects and back), human-readable output, `Expected<T>` result types, immutable configuration, compile-time message dispatch via `std::variant` + `std::visit`, and a streaming bitstream I/O layer.

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
gsml3parser::L3Disconnect disconnect(7,
    gsml3parser::CCCause::Normal_Call_Clearing,
    gsml3parser::CCCauseLocation::Private_Serving_Local);

// Serialize to hex string
auto hex = gsml3parser::writeL3Hex(disconnect);
```

### Registering a Custom PD Handler

Custom handlers for unsupported Protocol Discriminators (e.g. SMS, GPRS) can be registered via an immutable `ParserConfig` builder. The handler receives the raw L3 header and body bytes, allowing you to parse with `BitReader`:

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

3. **Variant dispatch** — `ParsedMessage = std::variant<RRM, MMM, CCM, SSM>` holds the parsed result on the stack. `tryGet<T>()` provides compile-time typed access.

4. **Streaming** — `ByteSource` → `L3Framer` → `L3StreamProcessor` pipeline processes raw byte streams with automatic frame boundary detection.

## Supported Messages

### Radio Resource (PD=0x06) — 95 message types

Paging Request Type 1/2/3, Paging Response, System Information Type 1/2/2bis/2ter/3/4/5/5bis/5ter/6/7/8/9/13/13alt/14/15/16/17/18/19/20/2n/21/22/23, Channel Release, Immediate Assignment/Extended/Reject, Additional Assignment, Physical Information, Handover Command/Complete/Failure, RR Status, Assignment Command/Complete/Failure, Classmark Enquiry/Change, Measurement Report, Extended Measurement Report/Order, Ciphering Mode Command/Complete, Channel Mode Modify/Acknowledge, GPRS Suspension Request, Application Information, Synchronization Channel Info, Channel Request, Handover Access, Configuration Change Command/Acknowledge/Reject, Partial Release/Complete, Frequency Redefinition, Notification NCH/Response, VGCS Uplink Grant, Uplink Release/Busy, Talker Indication, Priority Uplink Request, Data Indication/Data Indication 2, DTM Assignment Failure/Reject/Request/Assignment Command/Information, Packet Assignment/Information, UTRAN Classmark Change, CDMA2000 Classmark Change, Intersys to UTRAN HO Command, Intersys to CDMA2000 HO Command, GERAN IU Mode Classmark Change, System Information Type 10/10bis/10ter (short), Notification FACCH, Uplink Free, Enhanced Measurement Report UL, Measurement Info DL, VBS/VGCS Recon/Recon 2, VGCS Add Info, VGCS SMS Info, VGCS Neighbor Cell Info, Notify App Data.

### Mobility Management (PD=0x05) — 18 message types

IMSI Detach Indication, CM Service Accept/Reject/Abort/Request, CM Reestablishment Request, Identity Response/Request, MM Information, Location Updating Accept/Reject/Request, TMSI Reallocation Command/Complete, MM Status, Authentication Request/Response/Reject.

### Call Control (PD=0x03) — 20 message types, 26 IE types

Setup, Emergency Setup, Call Proceeding, Alerting, Connect, Connect Acknowledge, Call Confirmed, Disconnect, Release, Release Complete, Start DTMF, Stop DTMF, Stop DTMF Acknowledge, Start DTMF Acknowledge, Start DTM F Reject, Hold, Hold Reject, CC Status, Progress.

**CC Information Elements (26 types):** BearerCapability, BackupBearerCapability, SupportedCodecList, BCDDigits, CalledPartyBCDNumber, CallingPartyBCDNumber, ConnectedNumber, RedirectingNumber, SubAddress, CauseElement, CallState, ProgressIndicator, KeypadFacility, Signal, RepeatIndicator CLIRSuppression, CLIRInvocation, NetworkCCCapabilities, LowLayerCompatibility, HighLayerCompatibility, UserUser, Priority, StreamIdentifier, AllowedActions, CCCapabilities, SupServFacilityIE, SupServVersionIndicator.

**SS Enums (2 types):** SSOpCode (19 operation codes per GSM 04.80 §4.5), SSErrorCode (23 error codes).

**SS IEs (2 types):** L3FacilityOpCode (TCAP component parser: Invoke/ReturnResult/ReturnError/Reject), L3USSDData (USSD message with DCS, GSM 7-bit encode/decode, UCS2 support).

### Supplementary Services (PD=0x0b) — 3 message types, 2 enums, 2 IEs

**Messages:** Supervisory Service Facility Message, Supervisory Service Register Message, Supervisory Service Release Complete Message.

**Enums:** SSOpCode (19 TCAP operation codes: RegisterSS, EraseSS, ActivateSS, DeactivateSS, InterrogateSS, NotifySS, RegisterPassword, GetPassword, ProcessUSSData, ForwardCheckSSInd, ProcessUSSReq, USSRequest, USSNotify, ForwardCUGInfo, SplitMPTY, RetrieveMPTY, HoldMPTY, BuildMPTY, ForwardChargeAdvice), SSErrorCode (23 error codes per GSM 04.80 §4.5).

**IEs:** L3FacilityOpCode (TCAP component parser from raw Facility data — Invoke/ReturnResult/ReturnError/Reject with invoke ID, op code, error code, parameters), L3USSDData (USSD-specific IE: invoke ID, DCS with alphabet/language, GSM 7-bit packed string, UCS2 support, encode/decode helpers).

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

- [ ] SMS parser (PD=0x09) — GSM 04.11 / 3GPP TS 24.011
- [ ] GPRS L3 parser (PD=0x08, 0x0a) — GSM 04.08 9.4 / 3GPP TS 24.008
- [ ] Fuzzing target (libFuzzer integration)
- [ ] C API wrapper for FFI
- [ ] Python bindings (pybind11)

## License

This software is distributed under the terms of the MIT License.
See the [COPYING](COPYING) file for details.

## Acknowledgments

- Copyright 2026 momentics &lt;momentics@gmail.com&gt;
- Copyright libgsml3parser contributors
- Golden test vectors validated against the [Osmocom](https://osmocom.org/) TTCN-3 testing infrastructure
