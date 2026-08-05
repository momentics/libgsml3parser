# libgsml3parser

**GSM Layer 3 Signalling Message Parser — Standalone C++20 Library**

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Build](https://github.com/momentics/libgsml3parser/actions/workflows/build-release.yml/badge.svg)](https://github.com/momentics/libgsml3parser/actions/workflows/build-release.yml)
[![Version](https://img.shields.io/badge/Version-0.5.0-blue.svg)](https://github.com/momentics/libgsml3parser/releases)

## Overview

libgsml3parser is a standalone C++20 library for parsing and generating GSM Layer 3 (L3) signalling messages. It implements protocol discriminator dispatch across four domains, as defined by GSM 04.08 / 3GPP TS 24.008:

| Domain | PD | Spec Section | Messages |
|--------|----|-------------|----------|
| **Radio Resource (RR)** | `0x06` | GSM 04.08 9.1 | Paging, System Information (SI1–SI17), Handover, Assignment, Ciphering, etc. |
| **Mobility Management (MM)** | `0x05` | GSM 04.08 9.2 | Location Updating, Authentication, Identity, CM Service, TMSI Reallocation |
| **Call Control (CC)** | `0x03` | GSM 04.08 9.3 / ISDN Q.931 | Setup, Connect, Disconnect, Release, DTMF, Hold, Progress |
| **Supplementary Services (SS)** | `0x0b` | GSM 04.80 / 3GPP TS 24.080 | Facility, Register, Release Complete |

The library is self-contained with zero external dependencies beyond the C++20 standard library. It provides bidirectional parsing (binary to typed C++ objects and back), human-readable output, `ParseResult`-based error handling, and callback-based extensibility for unsupported PD domains (SMS, GPRS).

## Features

- **Full L3 message parsing** — Binary data to typed C++ objects with typed accessors
- **Message generation** — Typed C++ objects to binary data (for test harnesses, fuzzing, replay)
- **Safe error handling** — `ParseResult<T>` return type with structured error codes and bit-position tracking
- **Human-readable output** — Every message has a `.text()` method for logging and debugging
- **Callback-based extension** — Register custom handlers for SMS (PD=0x09) and GPRS (PD=0x08, 0x0a)
- **Thread-safe** — `ParserContext` isolates PD handlers per caller; no global mutable state
- **HPL-compliant memory** — Arena allocator, client-owned buffers, zero-copy `BitSpan`
- **Zero external dependencies** — No networking, no SIP, no radio stack
- **Memory-safe** — `std::unique_ptr` ownership, no raw `new`/`delete` in the public API
- **Fuzzing-ready** — Clean parse/generate API suitable for libFuzzer integration
- **819 unit tests** — Comprehensive coverage including spec-verified golden test vectors, threading safety, and HPL API
- **Golden test vectors** — Cross-validated against osmo-ttcn3-hacks TTCN-3 reference suite (L3_Templates.ttcn, GSM_RR_Types.ttcn, GSM_Types.ttcn, BTS_Tests.ttcn, GSM_SystemInformation.ttcn)
- **Spec-compliant** — Follows GSM 04.08 / 3GPP TS 24.008, GSM 04.07 / 3GPP TS 24.007, GSM 04.80 / 3GPP TS 24.080, 3GPP TS 44.018
- **V/TV/TLV/LV formats** — Correct handling of all GSM 04.07 IE encoding formats
- **Bit-level parsing** — MSB-first bit ordering, half-octet field handling, H/L rest octet fill patterns (0x2B padding)
- **System Information V-format** — SI1–SI17 parsed per GSM 04.08 tables (9.1.31–9.1.43c), with proper rest octet handling
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
#include <gsml3parser/parser.h>
#include <gsml3parser/context.h>
#include <gsml3parser/result.h>
#include <iostream>

int main() {
    // Parse from raw bytes using std::span (C++20)
    std::span<const uint8_t> data{
        0x06, 0x21, 0x10, 0x05, 0x08, 0x12, 0x34, 0x56, 0x78
    };

    // Use a ParserContext for thread-safe configuration
    gsml3parser::ParserContext ctx;
    auto result = gsml3parser::parseL3(data, ctx);

    if (result) {
        std::cout << result->text() << std::endl;
    } else {
        std::cerr << "Parse error: " << result.error().message << "\n";
    }

    // Parse from hex string (std::string_view)
    auto result2 = gsml3parser::parseL3Hex("0621100508012345678", ctx);

    // Downcast to specific type
    if (result2) {
        if (auto* rrMsg = dynamic_cast<gsml3parser::L3PagingRequestType1*>(result2->get())) {
            const auto& id = rrMsg->mobileId();
            std::cout << "Paged: " << id << std::endl;
        }
    }
}
```

### Generating a Message

```cpp
#include <gsml3parser/cc/l3ccmessages.h>

// Create a Disconnect message
gsml3parser::L3Disconnect disconnect(7,
    gsml3parser::CCCause::Normal_Call_Clearing,
    gsml3parser::CCCauseLocation::Private_Serving_Local);

// Serialize to hex string
std::string hex = gsml3parser::writeL3Hex(disconnect);
std::cout << hex << std::endl;

// Serialize to bytes
std::vector<uint8_t> buf(disconnect.fullLength());
size_t n = gsml3parser::writeL3(disconnect, buf.data(), buf.size());
```

### Using the Builder Pattern

```cpp
#include <gsml3parser/cc/l3ccmessages.h>

// Build a Setup message fluently
auto setup = gsml3parser::L3Setup::builder(7)
    .calledParty(gsml3parser::L3CalledPartyBCDNumber("1234567890"))
    .callingParty(gsml3parser::L3CallingPartyBCDNumber("0987654321"))
    .build();

std::string hex = gsml3parser::writeL3Hex(setup);
```

### Registering a Custom PD Handler

```cpp
#include <gsml3parser/parser.h>
#include <gsml3parser/context.h>

// Create a dedicated context and register handlers on it
gsml3parser::ParserContext ctx;
ctx.registerPDHandler(gsml3parser::L3PD::SMS,
    [](const gsml3parser::L3Frame& frame) {
        // Your SMS parsing logic here
        return std::make_unique<MySMSMessage>();
    });

// Parse with the configured context
auto result = gsml3parser::parseL3(data, ctx);
```

### Using Arena Allocator for Batch Parsing

```cpp
#include <gsml3parser/parser.h>
#include <gsml3parser/context.h>
#include <gsml3parser/arena.h>
#include <gsml3parser/bitvector.h>

// Create a thread-local arena for high-throughput parsing
gsml3parser::Arena arena(65536);  // 64 KB initial capacity

// Parse many messages, resetting the arena between batches
for (const auto& batch : messageBatches) {
    arena.reset();  // reclaim all memory from previous batch

    for (std::span<const uint8_t> frame : batch) {
        // BitVector allocated from arena — no individual free needed
        gsml3parser::BitVector bv(arena, frame.size_bytes() * 8);
        // ... process or parse ...
    }
}
```

### Zero-Copy Parsing with BitSpan

```cpp
#include <gsml3parser/bitvector.h>

// External buffer you do not own (e.g. from a network socket)
extern uint8_t socketBuffer[256];

// Create a non-owning read-only view — no allocation
gsml3parser::BitSpan view(socketBuffer, 2048);  // 256 bytes = 2048 bits

// Read fields from the view without copying data
size_t rp = 0;
unsigned pd = view.readField(rp, 8);
unsigned mti = view.readField(rp, 7);
```

## Supported Messages

### Radio Resource (PD=0x06) — 40 message classes

| Message | Direction | Spec Section |
|---------|-----------|-------------|
| Paging Request Type 1 | DL | GSM 04.08 9.1.22 |
| Paging Request Type 2 | DL | GSM 04.08 9.1.23 |
| Paging Request Type 3 | DL | GSM 04.08 9.1.24 |
| Paging Response | UL | GSM 04.08 9.1.25 |
| System Information Type 1 | DL | GSM 04.08 9.1.31 |
| System Information Type 2 | DL | GSM 04.08 9.1.32 |
| System Information Type 2bis | DL | GSM 04.08 9.1.33 |
| System Information Type 2ter | DL | GSM 04.08 9.1.34 |
| System Information Type 3 | DL | GSM 04.08 9.1.35 |
| System Information Type 4 | DL | GSM 04.08 9.1.36 |
| System Information Type 5 | DL | GSM 04.08 9.1.37 |
| System Information Type 5bis | DL | GSM 04.08 9.1.38 |
| System Information Type 5ter | DL | GSM 04.08 9.1.39 |
| System Information Type 6 | DL | GSM 04.08 9.1.40 |
| System Information Type 7 | DL | GSM 04.08 9.1.41 |
| System Information Type 8 | DL | GSM 04.08 9.1.42 |
| System Information Type 9 | DL | GSM 04.08 9.1.43 |
| System Information Type 13 | DL | GSM 04.08 9.1.43a |
| System Information Type 16 | DL | GSM 04.08 9.1.43b |
| System Information Type 17 | DL | GSM 04.08 9.1.43c |
| Channel Release | DL | GSM 04.08 9.1.7 |
| Immediate Assignment | DL | GSM 04.08 9.1.19 |
| Immediate Assignment Extended | DL | GSM 04.08 9.1.18 |
| Immediate Assignment Reject | DL | GSM 04.08 9.1.20 |
| Additional Assignment | DL | GSM 04.08 9.1.1 |
| Physical Information | DL | GSM 04.08 9.1.12 |
| Handover Command | DL | GSM 04.08 9.1.15 |
| RR Status | UL | GSM 04.08 9.1.29 |
| Assignment Command | DL | GSM 04.08 9.1.2 |
| Assignment Complete | UL | GSM 04.08 9.1.3 |
| Assignment Failure | UL | GSM 04.08 9.1.3 |
| Classmark Enquiry | DL | GSM 04.08 9.1.14 |
| Classmark Change | UL | GSM 04.08 9.1.11 |
| Measurement Report | UL | GSM 04.08 9.1.21 |
| Ciphering Mode Command | DL | GSM 04.08 9.1.9 |
| Ciphering Mode Complete | UL | GSM 04.08 9.1.10 |
| Handover Complete | UL | GSM 04.08 9.1.16 |
| Handover Failure | UL | GSM 04.08 9.1.17 |
| Channel Mode Modify | DL | GSM 04.08 9.1.5 |
| Channel Mode Modify Acknowledge | UL | GSM 04.08 9.1.6 |
| GPRS Suspension Request | UL | GSM 04.08 9.1.13b |
| Application Information | DL/UL | GSM 04.08 9.1.53 |
| Synchronization Channel Info | DL | GSM 04.08 9.1.30 |
| Channel Request | UL | GSM 04.08 9.1.13 |
| Handover Access | UL | GSM 04.08 9.1.14a |

### Mobility Management (PD=0x05) — 18 message classes

| Message | Direction | Spec Section |
|---------|-----------|-------------|
| Location Updating Request | UL | GSM 04.08 9.2.15 |
| Location Updating Accept | DL | GSM 04.08 9.2.13 |
| Location Updating Reject | DL | GSM 04.08 9.2.14 |
| IMSI Detach Indication | UL | GSM 04.08 9.2.15 |
| CM Service Accept | DL | GSM 04.08 9.2.5 |
| CM Service Abort | DL | GSM 04.08 9.2.7 |
| CM Service Reject | DL | GSM 04.08 9.2.6 |
| CM Service Request | UL | GSM 04.08 9.2.9 |
| CM Reestablishment Request | UL | GSM 04.08 9.2.4 |
| MM Information | DL | GSM 04.08 9.2.15a |
| Identity Request | DL | GSM 04.08 9.2.10 |
| Identity Response | UL | GSM 04.08 9.2.11 |
| Authentication Request | DL | GSM 04.08 9.2.2 |
| Authentication Response | UL | GSM 04.08 9.2.3 |
| Authentication Reject | DL | GSM 04.08 9.2.1 |
| TMSI Reallocation Command | DL | GSM 04.08 9.2.17 |
| TMSI Reallocation Complete | UL | GSM 04.08 9.2.18 |
| MM Status | UL | GSM 04.08 9.2.15 |

### Call Control (PD=0x03) — 18 message classes

| Message | Direction | Spec Section |
|---------|-----------|-------------|
| Setup | UL | GSM 04.08 9.3.19 |
| Emergency Setup | UL | GSM 04.08 9.3.8 |
| Call Proceeding | DL | GSM 04.08 9.3.3 |
| Alerting | DL | GSM 04.08 9.3.1 |
| Connect | UL | GSM 04.08 9.3.5 |
| Connect Acknowledge | DL | GSM 04.08 9.3.6 |
| Call Confirmed | DL | GSM 04.08 9.3.2 |
| Disconnect | UL | GSM 04.08 9.3.7 |
| Release | DL/UL | GSM 04.08 9.3.19 |
| Release Complete | DL/UL | GSM 04.08 9.3.19 |
| CC Status | DL | GSM 04.08 9.3.19 |
| Start DTMF | UL | GSM 04.08 9.3.24 |
| Start DTMF Acknowledge | DL | GSM 04.08 9.3.25 |
| Start DTMF Reject | DL | GSM 04.08 9.3.26 |
| Stop DTMF | UL | GSM 04.08 9.3.29 |
| Stop DTMF Acknowledge | DL | GSM 04.08 9.3.30 |
| Hold | UL | GSM 04.08 9.3.10 |
| Hold Reject | DL | GSM 04.08 9.3.12 |
| Progress | DL | GSM 04.08 9.3.17 |

### Supplementary Services (PD=0x0b) — 3 message classes

| Message | Direction | Spec Section |
|---------|-----------|-------------|
| Facility | DL/UL | GSM 04.80 / 3GPP TS 24.080 2.3 |
| Register | DL/UL | GSM 04.80 / 3GPP TS 24.080 2.4 |
| Release Complete | DL | GSM 04.80 / 3GPP TS 24.080 2.5 |

## Supported Standards

The library implements encodings defined by the following specifications:

| Standard | Scope | Library Coverage |
|----------|-------|-----------------|
| **GSM 04.08 / 3GPP TS 24.008** | Mobile radio interface L3 protocol | Full RR, MM, CC message parsing and generation |
| **GSM 04.07 / 3GPP TS 24.007** | Information element encoding rules | V, TV, TLV, LV formats; H/L rest octet padding (0x2B); bit ordering |
| **GSM 04.80 / 3GPP TS 24.080** | Supplementary services on mobile | Facility, Register, Release Complete messages |
| **3GPP TS 44.018** | Multi-rate speech channels (AMR) | Channel mode, multi-rate configuration, codec set negotiation |
| **GSM 05.02 / 3GPP TS 45.002** | Physical layer constants | RxLev/RxQual conversion, TDMA frame timing, hyperframe boundaries |
| **GSM 03.38 / 3GPP TS 23.038** | Default alphabet and coding scheme | 7-bit GSM alphabet encoding/decoding, BCD digit encoding |

## Information Elements

### Common IEs (32 types) — GSM 04.08 10.5.1, 10.5.2

Cell Identity, Location Area Identity (MCC/MNC BCD + LAC), Mobile Identity (TMSI/IMSI/IMEI/IMEISV), Classmark 1/2/3, Frequency Lists, BCCH Frequency List, Cell Channel Description, Channel Description, Timing Advance, Cell Selection Parameters, RACH Control Parameters, Measurement Results, Ciphering Mode Setting/Response, Power Command, NCC Permitted, Request Reference, Handover Reference, Synchronization Indication, Multi-Rate Configuration (AMR), and more.

### CC IEs (13 types) — GSM 04.08 10.5.4

Bearer Capability, Supported Codec List, Called/Calling Party BCD Number, Cause Element, Call State, Progress Indicator, Keypad Facility, Signal, Repeat Indicator, Supplementary Service Facility, Supplementary Service Version Indicator, and more.

### MM IEs (6 types) — GSM 04.08 10.5.3

CM Service Type, Reject Cause, Network Name, Time Zone And Time, RAND (128-bit), SRES (32-bit).

## Error Handling

The parser API returns `ParseResult<T>` instead of raw pointers, providing structured error information:

```cpp
auto result = gsml3parser::parseL3(data, ctx);

if (result) {
    // Success — access the message via operator-> or .value()
    std::cout << result->text() << std::endl;
} else {
    // Failure — inspect the error details
    const auto& err = result.error();
    std::cerr << "Error " << static_cast<int>(err.code)
              << " at bit " << err.bitPosition
              << ": " << err.message << "\n";
}
```

Error codes (`ParseErrorCode`):

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

## Protocol Types

The library provides bounded protocol types (`protocol_types.h`) for common GSM fields, ensuring values stay within spec-defined ranges:

| Type | Range | Description |
|------|-------|-------------|
| `Arfcn` | 0–1023 | Absolute Radio Frequency Channel Number |
| `Bsic` | 0–63 | Base Station Identity Code |
| `TimingAdvanceValue` | 0–63 | Timing Advance |
| `Ncc` | 0–7 | Network Color Code |
| `Bcc` | 0–7 | Base Station Color Code |
| `Tsc` | 0–7 | Training Sequence Code |
| `Hsn` | 0–7 | Hopping Sequence Number |
| `Maio` | 0–63 | MAIO (Multiframe Offset) |
| `TimeslotNumber` | 0–15 | TDMA timeslot |
| `CellIdentity` | 0–65535 | Cell Identity |
| `Lac` | 0–65535 | Location Area Code |

## Project Structure

```
libgsml3parser/
├── CMakeLists.txt                    # Build configuration (CMake 3.20+)
├── COPYING                           # MIT license
├── README.md                         # This file
├── cmake/
│   └── gsml3parserConfig.cmake.in    # CMake package config
├── include/gsml3parser/
│   ├── arena.h                       # Arena bump allocator for high-throughput parsing
│   ├── bitvector.h                   # BitVector (owning) + BitSpan (zero-copy view)
│   ├── context.h                     # ParserContext — thread-safe PD handler registry
│   ├── enums.h                       # RRCause, MMRejectCause, CCCause, BSSCause
│   ├── gsm_common.h                  # GSM constants, alphabet tables, timing, RACH params
│   ├── l3frame.h                     # L3Frame — L3 message frame with metadata
│   ├── l3message.h                   # L3Message, L3ProtocolElement base classes
│   ├── logger.h                      # Configurable logging (env: GSML3PARSER_LOG_LEVEL)
│   ├── parser.h                      # Main API: parseL3(), writeL3()
│   ├── protocol_types.h              # Bounded protocol field types (Arfcn, Bsic, etc.)
│   ├── result.h                      # ParseResult<T> — structured error handling
│   ├── types.h                       # L3PD, Primitive, SAPI, MobileIDType, etc.
│   ├── common/
│   │   └── l3common.h                # Common IEs (CellID, LAI, MobileIdentity,
│   │                                 #   ChannelDesc, Classmarks, FrequencyList, etc.)
│   ├── rr/
│   │   └── l3rrmessages.h            # RR messages (Paging, SI1-SI17, Handover, etc.)
│   ├── mm/
│   │   ├── l3mmlements.h             # MM IEs (CMServiceType, RAND, SRES, NetworkName)
│   │   └── l3mmmessages.h            # MM messages (LocationUpdate, Auth, CM, etc.)
│   ├── cc/
│   │   ├── l3cclements.h             # CC IEs (BearerCapability, BCDNumbers, Cause, etc.)
│   │   └── l3ccmessages.h            # CC messages (Setup, Connect, Release, etc.)
│   └── ss/
│       └── l3ssmessages.h            # SS messages (Facility, Register, etc.)
├── src/                              # Implementation (14 .cpp files)
├── tests/                            # Google Test unit tests (819 test cases)
├── examples/
│   └── example_parse_file.cpp        # Parse L3 messages from hex string or file
└── doc/
    └── API.md                        # Full API reference
```

## Testing

The test suite includes 819 Google Test cases across 15 test files. Golden test vectors in `test_golden_*.cpp` are cross-validated against the osmo-ttcn3-hacks TTCN-3 reference testing suite (L3_Templates.ttcn, GSM_RR_Types.ttcn, GSM_Types.ttcn, BTS_Tests.ttcn, GSM_SystemInformation.ttcn).

```bash
cmake .. -DBUILD_TESTS=ON
cmake --build . --config Release --parallel
ctest --output-on-failure
```

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

## Thread Safety

The library is designed for multi-threaded use:

- **`ParserContext`** — Isolates PD handler registries per instance. Multiple threads can share a read-only `ParserContext` safely (protected by `std::shared_mutex`). Write operations (register/unregister handlers) acquire an exclusive lock.
- **Logger** — Each thread maintains its own `LogLevel` via `thread_local`. The stderr backend is mutex-protected. Custom `LogCallback` instances are also thread-local.
- **`BitSpan`** — A non-owning, read-only view that can be shared across threads as long as the underlying buffer remains valid.
- **Arena** — NOT thread-safe. Each thread must use its own `Arena` instance. Allocations from one arena must not be accessed concurrently from another thread after a `reset()`.

For maximum performance in multi-threaded parsers, create one `ParserContext` and one `Arena` per thread:

```cpp
void parseThread(std::span<const uint8_t> frames) {
    gsml3parser::ParserContext ctx;
    gsml3parser::Arena arena(65536);

    for (auto frame : frames) {
        arena.reset();
        auto result = gsml3parser::parseL3(frame, ctx);
        if (result) {
            // ... process result-> ...
        }
    }
}
```

## Memory Management (HPL)

The library follows High-Performance Library (HPL) memory conventions:

| Component | Ownership | Lifetime |
|-----------|-----------|----------|
| `std::vector<uint8_t>` (default BitVector) | Library-owned | Automatic (RAII) |
| Arena-allocated `BitVector` | Arena-owned | Until `Arena::reset()` |
| `BitSpan` | Non-owning | Caller ensures underlying buffer outlives the span |
| `parseL3()` return value | `ParseResult<std::unique_ptr<L3Message>>` | Automatic (RAII) |
| `LogCallback` | Library-held copy | Thread-local, cleared on thread exit |

### Memory Ownership Diagram

```
Client code
  ├── ParserContext (per-thread or shared read-only)
  │     └── PD handlers (unordered_map, mutex-protected)
  │
  ├── Arena (per-thread, NOT shared)
  │     └── BitVector (bump-allocated, no individual free)
  │           └── L3Frame → parseL3() → ParseResult<unique_ptr<L3Message>>
  │
  └── BitSpan (zero-copy, read-only)
        └── External buffer (caller-owned, e.g. socket, DMA ring)
```

## License

This software is distributed under the terms of the MIT License.
See the [COPYING](COPYING) file for details.

## Acknowledgments

- Copyright 2026 momentics &lt;momentics@gmail.com&gt;
- Copyright libgsml3parser contributors
- Golden test vectors validated against the [Osmocom](https://osmocom.org/) TTCN-3 testing infrastructure
