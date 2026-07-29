# libgsml3parser

**GSM L3 Signalling Message Parser — Standalone C++17 Library**

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![Build](https://img.shields.io/badge/Build-CMake-brightgreen.svg)](https://cmake.org)

## Overview

libgsml3parser is a standalone C++17 library for parsing and generating GSM Layer 3 (L3) signalling messages. It implements the complete GSM 04.08 protocol discriminator dispatch for:

- **Radio Resource (RR)** — GSM 04.08 9.1 — Paging, System Information, Handover, Ciphering, Assignment, etc.
- **Mobility Management (MM)** — GSM 04.08 9.2 — Location Updating, Authentication, Identity, CM Service, etc.
- **Call Control (CC)** — GSM 04.08 9.3 / ISDN Q.931 — Setup, Connect, Disconnect, Release, DTMF, Hold, etc.
- **Supplementary Services (SS)** — GSM 04.80 — Facility, Register, Release Complete

The library is a standalone, self-contained project with no external dependencies beyond the C++17 standard library.

## Features

- **Full L3 message parsing** — Binary data → typed C++ objects with typed accessors
- **Message generation** — Typed C++ objects → binary data (for test harnesses, fuzzing)
- **Human-readable output** — Every message has a `.text()` method
- **Callback-based extension** — Register custom handlers for SMS (PD=0x09) and GPRS (PD=0x08, 0x0a)
- **Zero external dependencies** — No threading, no networking, no SIP, no radio
- **Memory-safe** — `std::unique_ptr` ownership, no raw `new`/`delete` in the public API
- **Fuzzing-ready** — Clean API suitable for libFuzzer integration
- **90 unit tests** — Comprehensive test coverage for all protocols
- **Spec-compliant** — Follows GSM 04.08 / 3GPP TS 24.008, GSM 04.07 / 3GPP TS 24.007

## Quick Start

### Building

```bash
mkdir build && cd build
cmake .. -DBUILD_TESTS=ON -DBUILD_EXAMPLES=ON
make -j$(nproc)
```

### Installing

```bash
sudo make install
```

### Using in Your Project

```cmake
find_package(gsml3parser REQUIRED)
target_link_libraries(myapp PRIVATE gsml3parser::parser)
```

### Parsing a Message

```cpp
#include <gsml3parser/parser.h>
#include <iostream>

int main() {
    // Parse from raw bytes
    uint8_t data[] = { 0x06, 0x19, 0x00, /* ... */ };
    auto msg = gsml3parser::parseL3(data, sizeof(data));

    if (msg) {
        std::cout << msg->text() << std::endl;
    }

    // Parse from hex string
    auto msg2 = gsml3parser::parseL3Hex("061900...");

    // Downcast to specific type
    if (auto rrMsg = dynamic_cast<gsml3parser::L3PagingResponse*>(msg.get())) {
        const auto& id = rrMsg->mobileID();
        std::cout << "Paged: " << id << std::endl;
    }
}
```

### Generating a Message

```cpp
#include <gsml3parser/cc/l3ccmessages.h>

// Create a Disconnect message
gsml3parser::L3Disconnect disconnect(7, gsml3parser::CCCause::Normal_Call_Clearing);

// Serialize to hex
std::string hex = gsml3parser::writeL3Hex(disconnect);
std::cout << hex << std::endl;  // "0703251000"

// Serialize to bytes
std::vector<uint8_t> buf(disconnect.fullLength());
size_t n = gsml3parser::writeL3(disconnect, buf.data(), buf.size());
```

### Registering a Custom PD Handler

```cpp
#include <gsml3parser/parser.h>

gsml3parser::registerPDHandler(gsml3parser::L3PD::SMS,
    [](const gsml3parser::L3Frame& frame) {
        // Your SMS parsing logic here
        return std::make_unique<MySMSMessage>();
    });
```

## API Documentation

Full API reference is available in [doc/API.md](doc/API.md).

## Project Structure

```
libgsml3parser/
├── CMakeLists.txt              # Build configuration
├── COPYING                     # MIT license
├── cmake/
│   └── gsml3parserConfig.cmake.in  # CMake package config
├── include/
│   └── gsml3parser/
│       ├── bitvector.h         # BitVector — bit-level container
│       ├── l3frame.h           # L3Frame — L3 message container
│       ├── l3message.h         # L3Message, L3ProtocolElement
│       ├── types.h             # L3PD, Primitive, SAPI, etc.
│       ├── enums.h             # RRCause, CCCause, MMRejectCause, BSSCause
│       ├── scalar_types.h      # Bool_z — initialized scalar types
│       ├── gsm_common.h        # GSM constants, alphabet tables, timing
│       ├── logger.h            # Simple logging
│       ├── parser.h            # Main API entry point
│       ├── common/
│       │   └── l3common.h      # Common IEs (CellID, LAI, MobileIdentity,
│       │                       #   ChannelDesc, Classmarks, FrequencyList,
│       │                       #   MeasurementResults, CellSelection, etc.)
│       ├── rr/
│       │   └── l3rrmessages.h  # RR messages (Paging, SI1-17, Handover, etc.)
│       ├── mm/
│       │   ├── l3mmlements.h   # MM IEs (CMServiceType, RAND, SRES,
│       │   │                   #   NetworkName, TimeZoneAndTime)
│       │   └── l3mmmessages.h  # MM messages (LocationUpdate, Auth, CM, etc.)
│       ├── cc/
│       │   ├── l3cclements.h   # CC IEs (BearerCapability, BCDNumbers,
│       │   │                   #   Cause, Progress, CCCommonIEs, etc.)
│       │   └── l3ccmessages.h  # CC messages (Setup, Connect, Release, etc.)
│       └── ss/
│           └── l3ssmessages.h  # SS messages (Facility, Register, etc.)
├── src/                        # Implementation
├── tests/                      # Google Test unit tests (90 tests)
├── examples/
│   └── example_parse_file.cpp  # Parse L3 messages from file
└── doc/
    └── API.md                  # Full API reference
```

## Supported Messages

### Radio Resource (PD=0x06)

| Message | Direction | Spec |
|---------|-----------|------|
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
| Channel Mode Modify Ack | UL | GSM 04.08 9.1.6 |
| GPRS Suspension Request | UL | GSM 04.08 9.1.13b |
| Application Information | DL/UL | GSM 04.08 9.1.53 |

### Mobility Management (PD=0x05)

| Message | Direction | Spec |
|---------|-----------|------|
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

### Call Control (PD=0x03)

| Message | Direction | Spec |
|---------|-----------|------|
| Setup | UL | GSM 04.08 9.3.19 |
| Emergency Setup | UL | GSM 04.08 9.3.8 |
| Call Proceeding | DL | GSM 04.08 9.3.3 |
| Alerting | DL | GSM 04.08 9.3.1 |
| Connect | UL | GSM 04.08 9.3.5 |
| Connect Acknowledge | DL | GSM 04.08 9.3.6 |
| Call Confirmed | DL | GSM 04.08 9.3.2 |
| Disconnect | DL | GSM 04.08 9.3.7 |
| Release | DL | GSM 04.08 9.3.19 |
| Release Complete | DL | GSM 04.08 9.3.19 |
| Start DTMF | UL | GSM 04.08 9.3.24 |
| Start DTMF Acknowledge | DL | GSM 04.08 9.3.25 |
| Start DTMF Reject | DL | GSM 04.08 9.3.26 |
| Stop DTMF | UL | GSM 04.08 9.3.29 |
| Stop DTMF Acknowledge | DL | GSM 04.08 9.3.30 |
| Hold | UL | GSM 04.08 9.3.10 |
| Hold Reject | DL | GSM 04.08 9.3.12 |
| Progress | DL | GSM 04.08 9.3.17 |
| CC Status | DL | GSM 04.08 9.3.19 |

### Supplementary Services (PD=0x0b)

| Message | Direction | Spec |
|---------|-----------|------|
| Facility | DL/UL | GSM 04.80 2.3 |
| Register | DL/UL | GSM 04.80 2.4 |
| Release Complete | DL | GSM 04.80 2.5 |

## Roadmap

- [ ] SMS parser (PD=0x09) — GSM 04.11
- [ ] GPRS L3 parser (PD=0x08, 0x0a) — GSM 04.08 9.4
- [ ] Fuzzing target (libFuzzer)
- [ ] C API wrapper for FFI
- [ ] Python bindings (pybind11)

## License

This software is distributed under the terms of the MIT License.
See the COPYING file for details.

## Acknowledgments

- Copyright 2026 momentics <momentics@gmail.com>
- Copyright libgsml3parser contributors
