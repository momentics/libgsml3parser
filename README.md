# libgsml3parser

**GSM Layer 3 Protocol Stack — Parse, Build, and Run a Software BTS in C++20**

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Build](https://github.com/momentics/libgsml3parser/actions/workflows/build-release.yml/badge.svg)](https://github.com/momentics/libgsml3parser/actions/workflows/build-release.yml)
[![Version](https://img.shields.io/badge/Version-0.14.0-blue.svg)](https://github.com/momentics/libgsml3parser/releases)

## Why This Library?

Building a software GSM Base Transceiver Station (BTS) requires implementing the complete Layer 3 signalling stack: parsing binary messages, managing protocol state machines, tracking timers, correlating request-response transactions, and generating correct responses. Existing solutions make this hard:

- **osmo-bts** (C/libosmocore) — hand-coded parsers per message type, no builder API, implicit FSMs scattered across handler code
- **OpenBTS / srsRAN** — custom C++ structs with manual byte construction, limited round-trip testing
- **TTCN-3 test suites** — excellent for validation but not suitable as a production protocol stack

libgsml3parser fills the gap: a **type-safe, zero-allocation C++20 library** that provides everything from raw L3 parse/serialize up to per-subscriber state machines, timers, and transaction correlation — ready to connect to any SDR backend.

## Who Is This For?

| Audience | What You Get |
|----------|-------------|
| **Software BTS developers** | Drop-in replacement for osmo-bts L3 layer: parse, build, FSM, timers, LAPDm — link `libgsml3parser.a` and go |
| **Protocol testers & fuzzers** | Bidirectional API (binary to typed objects and back), golden test vectors cross-validated against Osmocom TTCN-3 |
| **SDR / radio hobbyists** | Complete L2 (LAPDm) + L3 stack for the Um interface, no networking or SIP dependencies |

## What You Get

Four layers of capability, from low-level parsing to high-level protocol state management:

### 1. L3 Parser & Serializer

Parse any GSM L3 message from raw bytes to a typed C++ object. Serialize back to bytes for transmission. All **12 protocol domains**, **200+ message types**, zero heap allocation on the hot path.

```cpp
auto msg = gsml3parser::parseL3Hex("060D00");  // hex string -> typed object
if (msg) {
    if (auto* cr = gsml3parser::tryGet<gsml3parser::L3ChannelRelease>(*msg)) {
        // Compile-time typed access — no dynamic_cast, no RTTI
    }
}
```

### 2. Fluent Builder API

Construct any L3 message from scratch with chainable setters. Every message type has a `builder()`:

```cpp
auto msg = L3PagingRequestType2::builder()
    .pageMode(L3PageMode::TMSI)
    .tmsi(0x12345678)
    .build();

ParsedMessage pm{RRM{std::move(msg)}};
auto bytes = writeL3Bytes(pm);  // raw bytes ready for radio
```

### 3. LAPDm Protocol Entity

Full LAPDm state machine (GSM 04.06) with SABME/UA/DISC, I-frame segmentation/reassembly, T200 retransmission, and contention resolution:

```cpp
LAPDmEntity entity(LAPDmChannelProfile::SDCCH(), onL3, onL1, nullptr);
entity.open(SAPI::SAPI0, true);   // BTS side
entity.sendUI(SAPI::SAPI0, l3Data);  // unacknowledged UI frame
entity.sendData(l3Data);           // acknowledged I-frames (segmented)
```

### 4. BTS Stack Modules — Protocol State Machines

What sets this library apart: ready-to-use per-subscriber state management primitives for building a complete BTS:

| Module | Purpose | Size |
|--------|---------|------|
| **MSContext** | Per-MS identity, channel, flags | ≤ 256 bytes |
| **TimerManager** | Protocol timers T3101–T3395, zero-alloc tick | ~1.2 KB |
| **TransactionManager** | Request-response correlation, O(1) TI lookup | ~768 bytes |
| **RR/MM/CC StateMachine** | Protocol FSM skeletons with O(1) dispatch | ~16 bytes each |
| **ChannelPool** | Logical channel allocation/release, VEA support | global |
| **SubscriberRegistry** | Per-MS session management, TMSI/IMSI/link indexes | < 4 KB/session |

Total per-MS footprint: **~2.7 KB** (fits L3 cache at 10K concurrent sessions). See [BTS Architecture Guide](doc/bts_architecture.md) for scaling to millions.

## How It Compares

| Aspect | osmo-bts (C) | OpenBTS / srsRAN | libgsml3parser |
|--------|-------------|-------------------|----------------|
| **Language** | C (libosmocore) | Legacy C++ | C++20 |
| **Type safety** | enum + manual cast | custom structs | `std::variant` + `tryGet<T>()` |
| **Message types** | hand-coded per message | partial coverage | 200+ typed messages, all 12 PD domains |
| **Builder API** | none (manual struct) | partial | fluent builder for every type |
| **FSM + timers** | implicit in handlers | custom | built-in stack modules |
| **Memory model** | heap-allocated structs | varies | stack variants, zero-alloc hot path |
| **LAPDm** | libosmocore (separate) | custom | full state machine, included |
| **Dependencies** | libosmocore + osmo-* | multiple | **zero** (C++20 stdlib only) |

## Quick Start

### Building

```bash
mkdir build && cd build
cmake .. -DBUILD_TESTS=ON -DBUILD_EXAMPLES=ON
cmake --build . --config Release --parallel
```

| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_SHARED_LIBS` | OFF | Shared library instead of static |
| `BUILD_TESTS` | OFF | Unit tests (Google Test 1.14.0) |
| `BUILD_EXAMPLES` | OFF | Example programs |
| `ENABLE_FUZZING` | OFF | Fuzzing target |

### Using in Your Project

```cmake
find_package(gsml3parser REQUIRED)
target_link_libraries(myapp PRIVATE gsml3parser::parser)
```

### Parsing a Message

```cpp
#include <gsml3parser/gsml3parser.hpp>

auto msg = gsml3parser::parseL3Hex("060D00");
if (msg) {
    std::cout << gsml3parser::messageName(*msg) << "\n";
}
```

### Building Messages

```cpp
auto msg = L3ImmediateAssignment::builder()
    .channelDescription(L3ChannelDescription(TDMA_SDCCH, 0, 1, 100))
    .timingAdvance(L3TimingAdvance(32))
    .build();

ParsedMessage pm{RRM{std::move(msg)}};
auto bytes = writeL3Bytes(pm);  // ready for LAPDmEntity.sendUI()
```

### BTS Examples

The `examples/` directory contains complete BTS workflow demonstrations:

| Example | Description |
|---------|-------------|
| `example_bts_paging.cpp` | Full paging cycle: Builder -> L3 bytes -> LAPDm -> unwrap -> parse |
| `example_bts_channel_assignment.cpp` | Channel Request handling and Immediate Assignment response |
| `example_bts_sysinfo.cpp` | System Information (SI1–SI4) construction for BCCH broadcast |
| `example_bts_dispatcher.cpp` | ProtocolDispatcher with multiple message handlers |
| `example_lapdm_entity.cpp` | Full LAPDmEntity lifecycle: SABME/UA, UI data, DISC release |

## Architecture

```
ByteSource (Span/File/RingBuffer)
    -> L3Framer (frame boundary detection)
    -> parseL3() -> Expected<ParsedMessage>
    -> std::visit / tryGet<T>() for typed access
```

Layered design, bottom to top:

1. **Bit-level I/O** — `BitReader`/`BitWriter`, bounds-checked, MSB-first, no heap
2. **Message types** — plain C++ structs with `parse()` and `write()`, no inheritance
3. **Variant dispatch** — `ParsedMessage` holds 12 domains on the stack (`sizeof < 8 KB`)
4. **Streaming** — `ByteSource` -> `L3Framer` -> `L3StreamProcessor` pipeline
5. **Stack modules** — MSContext, TimerManager, FSMs for BTS state management

## Supported Messages Summary

| Domain | PD | Messages | IEs |
|--------|----|----------|-----|
| Group Call Control (GCC) | `0x00` | 7 | — |
| Broadcast Call Control (BCC) | `0x01` | 6 | — |
| Call Control (CC) | `0x03` | 20 | 26 |
| Mobility Management (MM) | `0x05` | 18 | — |
| Radio Resource (RR) | `0x06` | 95 | 15+ |
| GPRS Mobility Mgmt (GMM) | `0x08` | 19 | 12 |
| SMS | `0x09` | 23 | 2 |
| GPRS Session Mgmt (SM) | `0x0a` | 29 | 8 |
| Supplementary Services (SS) | `0x0b` | 3 | 2 |
| Location Services (LS) | `0x0c` | 2 | — |
| Extended PD | `0x0e` | 1 | — |
| Test Procedure PD | `0x0f` | 1 | — |

**Full message catalog:** [doc/messages.md](doc/messages.md)

## Performance

Optimized for high-throughput, low-latency L3 parsing at scale:

| Optimization | Impact |
|--------------|--------|
| Handler dispatch `std::array[16][256]` | O(1) index, ~64 KB L2-cache resident |
| `FlatHandler` callbacks (16 bytes) | 2.5x smaller than `std::function`, no type erasure |
| RingBuffer `& mask` wrap | 1 CPU cycle vs 20-80 for modulo |
| Zero-copy parsing | span -> parse directly, no memcpy |
| Per-MS stack modules | ~2.7 KB per session (10K sessions = ~27 MB) |

```bash
./build/Release/examples/example_benchmark.exe      # all 12 PD domains
./build/Release/examples/example_multithread.exe    # concurrent parsing
./build/Release/examples/example_zero_copy.exe      # InlineFramer + ZeroCopyStreamProcessor
```

## Key Features

- **Full L3 message parsing** — Binary to typed C++ objects with compile-time dispatch via `std::variant`
- **Fluent Builder API** — Construct any L3 message from scratch (all 12 domains)
- **Message generation** — Typed objects to binary data (test harnesses, fuzzing, replay)
- **Full LAPDm protocol** — State machine with SABME/UA/DISC, I-frame segmentation, T200 retransmission
- **ProtocolDispatcher** — O(1) PD+MTI callback routing, TI-based dispatch for CC/SS
- **std::format support** — `enum_formatters.h` for all protocol enums
- **Expected<T> result type** — Zero-allocation errors with bit-position tracking
- **Immutable ParserConfig** — Thread-safe by design, no mutex on parse path
- **Zero heap allocation on hot path** — `ParsedMessage` variant on stack
- **Compile-time message dispatch** — `std::variant` + `std::visit`, no RTTI
- **Bitstream I/O** — `ByteSource` hierarchy (Span, File, RingBuffer) for streaming
- **Zero-copy stream processing** — `InlineFramer` and `ZeroCopyStreamProcessor`
- **ShardedChannelPool** — Thread-safe channel pool for million-concurrent-MS scaling
- **Arena allocator** — Bump allocator for high-throughput batch parsing
- **Zero external dependencies** — C++20 standard library only
- **Fuzzing-ready** — Clean parse/generate API suitable for libFuzzer
- **Spec-compliant** — GSM 04.08 / 3GPP TS 24.008, GSM 04.06, GSM 04.07, 3GPP TS 24.080, TS 44.018, TS 44.031

## Error Handling

```cpp
auto result = gsml3parser::parseL3Hex("060D");

if (result) {
    std::cout << gsml3parser::messageName(*result) << "\n";
} else {
    const auto& err = result.error();
    std::cerr << "Error " << static_cast<int>(err.code)
              << " at bit " << err.bitPosition
              << ": " << err.message << "\n";
}
```

| Code | Meaning |
|------|---------|
| `Ok` | Parse succeeded |
| `TruncatedInput` | Input data too short |
| `InvalidPD` | Unknown Protocol Discriminator |
| `InvalidMTI` | Message Type Indicator not recognized |
| `LengthMismatch` | Declared length does not match actual data |
| `InvalidIE` | Malformed Information Element |
| `InvalidValue` | Field value outside valid range |
| `UnsupportedFeature` | Feature not yet implemented |

## Thread Safety

- **ParserConfig** — Immutable, safe for concurrent read access. Builder methods return new instances.
- **parseL3()** — Stateless function, thread-safe with shared read-only config.
- **BitReader/BitWriter** — Plain value types, no shared state.
- **Arena** — NOT thread-safe. Each thread uses its own instance.

## Documentation

| Document | Topic |
|----------|-------|
| [doc/API.md](doc/API.md) | Full API reference (41 sections) |
| [doc/bts_architecture.md](doc/bts_architecture.md) | BTS architecture, threading model, scaling to millions of MS |
| [doc/bts_integration.md](doc/bts_integration.md) | Step-by-step integration guide with code examples |
| [doc/messages.md](doc/messages.md) | Complete catalog of 200+ message types |

## Build Requirements

| Requirement | Minimum Version |
|-------------|----------------|
| C++ compiler | GCC 11+, Clang 10+, MSVC 2022 17.3+ |
| CMake | 3.20 |
| Standard Library | C++20 (libstdc++ or libc++) |

## Roadmap

- [ ] Fuzzing target (libFuzzer integration)
- [ ] C API wrapper for FFI
- [ ] Python bindings (pybind11)

## License

MIT License. See [COPYING](COPYING) for details.

## Acknowledgments

- Copyright 2026 momentics &lt;momentics@gmail.com&gt;
- Copyright libgsml3parser contributors
- Golden test vectors validated against the [Osmocom](https://osmocom.org/) TTCN-3 testing infrastructure
