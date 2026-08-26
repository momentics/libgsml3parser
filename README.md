# libgsml3parser

**GSM Layer 3 Protocol Stack — Parse, Build, and Run a Software BTS in C++20**

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Build](https://github.com/momentics/libgsml3parser/actions/workflows/build-release.yml/badge.svg)](https://github.com/momentics/libgsml3parser/actions/workflows/build-release.yml)
[![Version](https://img.shields.io/badge/Version-0.16.0-blue.svg)](https://github.com/momentics/libgsml3parser/releases)

## Why This Library?

Building a software GSM Base Transceiver Station (BTS) requires implementing the complete Layer 3 signalling stack: parsing binary messages, managing protocol state machines, tracking timers, correlating request-response transactions, and generating correct responses. Existing solutions make this hard:

- **osmo-bts** (C/libosmocore) — hand-coded parsers per message type, no builder API, implicit FSMs scattered across handler code
- **OpenBTS / srsRAN** — custom C++ structs with manual byte construction, limited round-trip testing
- **TTCN-3 test suites** — excellent for validation but not suitable as a production protocol stack

libgsml3parser fills the gap: a **type-safe, zero-allocation C++20 library** that provides everything from raw L3 parse/serialize up to per-subscriber state machines, timers, and transaction correlation — ready to connect to any SDR backend.

## What You Save

| Without libgsml3parser | With libgsml3parser |
|------------------------|---------------------|
| Hand-roll binary parsers for 200+ message types | `parseL3Hex("060D00")` — one call, typed result |
| Manual byte construction for responses | Fluent builder: `.pageMode(TMSI).tmsi(…).build()` |
| Scatter/gather FSM logic across handlers | Pre-built `ProcedureOrchestrator` auto-chains Location Update, Auth, Call Setup |
| Track timers with raw `std::map` + cron jobs | `TimerManager` — O(1) tick, zero allocation, all GSM timers built-in |
| Correlate request/response with custom TI tables | `TransactionManager` — O(1) lookup, 0.004 µs per match |
| Debug hex dumps by eye | `std::format` support for every enum, `Expected<T>` with bit-position errors |

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
| **MSContext** | Per-MS identity, channel, flags | 92 bytes |
| **TimerManager** | Protocol timers T3101–T3395, zero-alloc tick | ~1.2 KB |
| **TransactionManager** | Request-response correlation, O(1) TI lookup | ~768 bytes |
| **RR/MM/CC StateMachine** | Protocol FSM skeletons with O(1) dispatch | ~16 bytes each |
| **ChannelPool** | Logical channel allocation/release, VEA support | global |
| **SubscriberRegistry** | Per-MS session management, TMSI/IMSI/link indexes | < 4 KB/session |

Total per-MS footprint: **~2 KB** (`sizeof(SubscriberSession)` = 2056 B; 10K concurrent sessions ≈ 20 MB). See [BTS Architecture Guide](doc/bts_architecture.md) for scaling to millions.

## Performance

Numbers that matter for a real-time radio stack:

| Metric | Result |
|--------|--------|
| **L3 parse throughput** | 9.6 – 39.3 M msg/s (per message type, single core) |
| **Mixed-domain stream** | 8.6 M msg/s (all 12 PD domains) |
| **Full BTS stack dispatch** (1K MS, pre-parsed messages, timers, FSM) | **~74–80 M msg/s** (single core; end-to-end parse+stream: 8.7 M msg/s) |
| **TimerManager tick** | 48.1 M ticks/sec |
| **Transaction lookup** | 0.004 µs per match |
| **State machine dispatch** | 0.007 µs per message |
| **ChannelPool alloc+release** | 0.069 µs per cycle |
| **1M sessions** (create / lookup / tick 10K active) | 1.4 s / 0.17 s / 1.4 ms |

Numbers measured with `example_benchmark` / `example_benchmark_stack` (Release, single core, MSVC 2026).
Every benchmark test and example prints a dynamically detected **hardware ID**
(CPU brand, base clock, sockets/cores/logical, L1/L2/L3 cache, RAM, memory
slots + clock, OS) so results are attributed to the machine that produced
them — see [`benchmark_results.txt`](benchmark_results.txt) for the full run.

| Optimization | Impact |
|--------------|--------|
| Handler dispatch `std::array[16][136]` | O(1) index, ~35 KB L2-cache resident |
| `FlatHandler` callbacks (16 bytes) | 2.5x smaller than `std::function`, no type erasure |
| RingBuffer `& mask` wrap | 1 CPU cycle vs 20-80 for modulo |
| Zero-copy parsing | span -> parse directly, no memcpy |
| Per-MS stack modules | ~2 KB per session (10K sessions = ~20 MB) |

```bash
./build/Release/examples/example_benchmark.exe       # all 12 PD domains (parse + stream)
./build/Release/examples/example_benchmark_stack.exe # full BTS stack (1K MS) component benchmarks
./build/Release/examples/example_multithread.exe     # concurrent parsing
./build/Release/examples/example_zero_copy.exe       # InlineFramer + ZeroCopyStreamProcessor
```

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

One include, one link:

```cmake
find_package(gsml3parser REQUIRED)
target_link_libraries(myapp PRIVATE gsml3parser::parser)
```

```cpp
#include <gsml3parser/gsml3parser.hpp>  // single header — full API
```

### Parsing a Message

```cpp
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

### Error Handling with Context

Every parse failure includes the error code and exact bit position:

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

### BTS Examples

The `examples/` directory contains complete BTS workflow demonstrations:

| Example | Description |
|---------|-------------|
| `example_bts_paging.cpp` | Full paging cycle: Builder -> L3 bytes -> LAPDm -> unwrap -> parse |
| `example_bts_channel_assignment.cpp` | Channel Request handling and Immediate Assignment response |
| `example_bts_sysinfo.cpp` | System Information (SI1–SI4) construction for BCCH broadcast |
| `example_bts_dispatcher.cpp` | ProtocolDispatcher with multiple message handlers |
| `example_lapdm_entity.cpp` | Full LAPDmEntity lifecycle: SABME/UA, UI data, DISC release |

### BTS Procedure Framework — High-Level Protocol Procedures

The highest level of abstraction: pre-built protocol procedures that encapsulate FSM, timers, transactions, and response generation. Instead of manually assembling message sequences, feed L3 messages into a `ProcedureOrchestrator` (for compound chains) or `ProcedureRunner` (for individual procedures). The framework returns a `ResponseToken` indicating which message to build, and the caller uses `ResponseBuilder::buildResponseFromToken()` to generate bytes in a pre-allocated Arena buffer (zero heap allocation):

```cpp
#include <gsml3parser/gsml3parser.hpp>
#include <gsml3parser/stack/procedure_orchestrator.h>

using namespace gsml3parser;

// Create subscriber session; the BTS application keeps one orchestrator
// per session for compound procedure chains.
SubscriberRegistry registry;
auto* session = registry.createByTMSI(0x12345678);
ProcedureOrchestrator orchestrator;   // app-owned, one per session

// Feed incoming L3 messages — orchestrator auto-chains sub-procedures.
auto result = orchestrator.feed(incomingMessage, session);

if (result.action == ProcedureStepResult::Action::SendResponseWithToken) {
    uint8_t buf[512];
    int n = ResponseBuilder::buildResponseFromToken(
        result.responseToken, {buf, sizeof(buf)}, session);
    if (n > 0) sendToMS(buf, n);
}

// Feed typed external decisions (e.g., VLR accept/reject, AuC RAND+SRES).
// The orchestrator forwards the session, so the procedure records the
// response parameters (RAND, new TMSI, ...) into session->response.
VLRDecision vlr{true, 0x87654321u, MMRejectCause::Zero};
orchestrator.feedExternalTyped(vlr);

AuthChallenge chal{};
std::memcpy(chal.rand.data(), aucRand, 16);
std::memcpy(chal.expectedSres.data(), aucSres, 4);
orchestrator.feedExternalTyped(chal);
```

**Available procedures:**

| Procedure | Spec | Description |
|-----------|------|-------------|
| `LocationUpdateProcedure` | TS 24.008 4.4.1 | Full location updating with auth + VLR decision |
| `AuthenticationProcedure` | TS 24.008 4.4.2 | RAND/SRES exchange with external AuC integration |
| `CallSetupMOPercedure` | TS 24.008 6.1 | Mobile Originated Call (RACH -> Active) |
| `CallSetupMTPercedure` | TS 24.008 6.1 | Mobile Terminated Call (Paging -> Active) |
| `ChannelAssignmentProcedure` | TS 04.08 9.1.2 | RACH -> Immediate Assignment -> Channel seizure |
| `CipheringModeProcedure` | TS 24.008 4.4.3 | A5 ciphering activation |
| `PagingProcedure` | TS 04.08 9.1.25 | Paging request (Type1/2/3) with T3109 retransmission |
| `HandoverProcedure` | TS 04.08 9.1.40 | Handover command/response flow |
| `CallReleaseProcedure` | TS 24.008 6.1 | Call release (disconnect -> release complete) |
| `IMSIDetachProcedure` | TS 24.008 4.4.6 | IMSI detach procedure |

**Abis/RSL Interface:**

Parse and construct A-bis RSL messages for BSC integration:

```cpp
auto rslMsg = RSLParser::parse(rawRSLBytes);
auto l3Payload = RSLParser::extractL3(rslMsg.value());
// ... process L3 message through ProcedureRunner ...
auto response = RSLBuilder::buildDataInd(chanNr, linkId, responseL3Bytes);
```

See `examples/` directory, [doc/bts_integration.md](doc/bts_integration.md) (step-by-step integration guide), and [doc/bts_architecture.md](doc/bts_architecture.md) for full examples.

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
3. **Variant dispatch** — `ParsedMessage` holds 12 domains on the stack (`sizeof(ParsedMessage) = 416 bytes`, static_assert < 8 KB)
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
- **ProcedureOrchestrator** — Auto-chains compound procedures (Location Update, Call Setup) with zero-alloc ResponseToken pattern
- **TypedExternalData** — Strongly-typed structures (`AuthChallenge`, `VLRDecision`) replace raw byte arrays for external data
- **Arena allocator** — Bump allocator for high-throughput batch parsing
- **Zero external dependencies** — C++20 standard library only
- **Fuzzing-ready** — Clean parse/generate API suitable for libFuzzer
- **Spec-compliant** — GSM 04.08 / 3GPP TS 24.008, GSM 04.06, GSM 04.07, 3GPP TS 24.080, TS 44.018, TS 44.031

## Thread Safety

- **ParserConfig** — Immutable, safe for concurrent read access. Builder methods return new instances.
- **parseL3()** — Stateless function, thread-safe with shared read-only config.
- **BitReader/BitWriter** — Plain value types, no shared state.
- **Arena** — NOT thread-safe. Each thread uses its own instance.

## Documentation

| Document | Topic |
|----------|-------|
| [doc/API.md](doc/API.md) | Full API reference (62 sections) |
| [doc/bts_architecture.md](doc/bts_architecture.md) | BTS architecture, threading model, scaling to millions of MS |
| [doc/bts_integration.md](doc/bts_integration.md) | **Primary guide for BTS developers**: ProcedureOrchestrator, ResponseToken pattern, typed external data |
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
