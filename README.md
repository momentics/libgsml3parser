# libgsml3parser

**GSM Layer 3 Protocol Stack — Parse, Build, and Run a Software BTS in C++20**

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Build](https://github.com/momentics/libgsml3parser/actions/workflows/build-release.yml/badge.svg)](https://github.com/momentics/libgsml3parser/actions/workflows/build-release.yml)
[![Version](https://img.shields.io/badge/Version-0.18.0-blue.svg)](https://github.com/momentics/libgsml3parser/releases)

## Why This Library?

Building a software GSM Base Transceiver Station (BTS) requires implementing the complete Layer 3 signalling stack: parsing binary messages, managing protocol state machines, tracking timers, correlating request-response transactions, and generating correct responses. Existing solutions make this hard:

- **osmo-bts** (C/libosmocore) — hand-coded parsers per message type, no builder API, implicit FSMs scattered across handler code
- **OpenBTS / srsRAN** — custom C++ structs with manual byte construction, limited round-trip testing
- **TTCN-3 test suites** — excellent for validation but not suitable as a production protocol stack

libgsml3parser fills the gap: a **type-safe, zero-allocation C++20 library** that provides everything from raw L3 parse/serialize up to per-subscriber state machines, timers, transaction correlation, and pre-built protocol procedures — ready to connect to any SDR backend or BSC over A-bis RSL.

## What You Save

| Without libgsml3parser | With libgsml3parser |
|------------------------|---------------------|
| Hand-roll binary parsers for 200+ message types | `parseL3Hex("600D00")` — one call, typed result |
| Manual byte construction for responses | Fluent builder: `.addTMSI(0x12345678, SDCCHType).build()` |
| Scatter/gather FSM logic across handlers | Pre-built `ProcedureOrchestrator` auto-chains Location Update, Auth, Call Setup |
| Track timers with raw `std::map` + cron jobs | `TimerManager` — fixed 32-slot arrays, zero allocation, GSM L3 timers T3101–T3395 built in |
| Correlate request/response with custom TI tables | `TransactionManager` — O(1) TI lookup, 0.004 µs per match |
| Debug hex dumps by eye | `std::format` specializations for protocol enums, `Expected<T>` with bit-position errors |

## Who Is This For?

| Audience | What You Get |
|----------|-------------|
| **Software BTS developers** | Drop-in replacement for the osmo-bts L3 layer: parse, build, FSMs, timers, procedures, LAPDm — link `gsml3parser` and go |
| **Protocol testers & fuzzers** | Bidirectional API (binary to typed objects and back), golden test vectors cross-validated against Osmocom TTCN-3, libFuzzer targets for every entry point |
| **SDR / radio hobbyists** | Complete L2 (LAPDm) + L3 stack for the Um interface plus A-bis RSL parsing/construction — no networking or SIP dependencies |

## What You Get

Four layers of capability, from low-level parsing to high-level protocol state management:

### 1. L3 Parser & Serializer

Parse any GSM L3 message from raw bytes to a typed C++ object. Serialize back to bytes for transmission. All **12 PD domains**, **236 message types**, zero heap allocation on the hot path.

```cpp
auto msg = gsml3parser::parseL3Hex("600D00");  // RR Channel Release
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
    .addTMSI(0x12345678, ChannelType::SDCCHType)
    .build();

ParsedMessage pm{RRM{std::move(msg)}};
auto bytes = writeL3Bytes(pm);  // raw bytes ready for radio
```

### 3. LAPDm Protocol Entity

Full LAPDm state machine (GSM 04.06 / TS 45.006) with SABME/UA/DISC, I-frame segmentation and reassembly (k=1), T200 retransmission, and contention resolution:

```cpp
LAPDmEntity entity(LAPDmChannelProfile::SDCCH(), onL3, onL1, nullptr);
entity.open(SAPI::SAPI0, true);   // BTS side (C/R=1)
entity.sendUI(SAPI::SAPI0, l3Data);  // unacknowledged UI frame
entity.sendData(l3Data);           // acknowledged I-frames (segmented)
```

Channel profiles carry per-channel N201/N200/T200 values (SDCCH, SACCH, FACCH). Reassembly is bounded at 4 KB to keep untrusted radio input from growing the buffer unboundedly. The TX encode buffer is allocated on the first send and reused afterwards, so the steady-state send path performs no heap allocation (`sizeof(LAPDmEntity) = 216 bytes` on x64, static-asserted < 512).

### 4. BTS Stack Modules — Protocol State Machines

What sets this library apart: ready-to-use per-subscriber state management primitives for building a complete BTS:

| Module | Purpose | Size (64-bit) |
|--------|---------|---------------|
| **MSContext** | Per-MS identity, channel, classmark, flags | 92 bytes |
| **TimerManager** | GSM L3 protocol timers T3101–T3395 (TS 24.008 10.5), ≤32 concurrent per MS | 1080 bytes |
| **TransactionManager** | Request/response correlation, O(1) TI index for CC/SS | 536 bytes |
| **RR/MM/CC StateMachine** | Protocol FSM skeletons with switch(PD)+switch(MTI) dispatch | 16 bytes each |
| **ChannelPool / ShardedChannelPool** | Logical channel allocation/release, VEA support (fixed arrays; sharded variant is thread-safe) | 2816 B / 45192 B per instance (default shard count 16) |
| **SubscriberRegistry** | Per-MS sessions with TMSI/IMSI/link indexes (open-addressing FlatMap), O(active) timer tick | 2056 bytes per session |
| **ShardedSubscriberRegistry** | Thread-safe registry: N power-of-two shards, per-shard `shared_mutex` | default 16 shards |

Total per-MS footprint: `sizeof(SubscriberSession)` = 2056 B (context + 3 FSMs + timers + transactions + ProcedureRunner + ResponseContext); 10K concurrent sessions ≈ 20 MB. See [BTS Architecture Guide](doc/bts_architecture.md) for scaling to millions.

## Performance

Numbers that matter for a real-time radio stack:

| Metric | Result |
|--------|--------|
| **L3 parse throughput** | 7.7 – 26.5 M msg/s (per message type, single core) |
| **Mixed-domain stream** (`L3StreamProcessor`, all 12 PDs) | 11.0 M msg/s |
| **Zero-copy stream** (`ZeroCopyStreamProcessor`) | 12.9 M msg/s (1.12x vs L3StreamProcessor on small frames) |
| **Full BTS stack dispatch** (1K MS, pre-parsed messages, timers, FSMs) | ~66 – 86 M msg/s (single core, 7 runs; byte-level parsing excluded — see parse throughput above) |
| **TimerManager tick** | 42.6 M ticks/sec |
| **Transaction lookup** | 0.004 µs per match |
| **State machine dispatch** | 0.006 µs per message |
| **ChannelPool alloc+release** | 0.070 µs per cycle |
| **1M sessions** (create / TMSI lookup / tick 10K active) | 1195 ms / 127 ms / 1.4 ms |
| **2M sessions** (create / TMSI lookup / tick 10K active) | 1096 ms / 258 ms / 1.4 ms |
| **1M-session real-time event loop** (`example_rt_scale`) | 6.2 M msg/s aggregate, tick avg 0.081 ms, 0 overruns / 1000, peak working set 2.0 GB |

Numbers measured with `example_benchmark` / `example_benchmark_stack` (Release, single core) and the stress/RT-scale tests.
Every benchmark test and example prints a dynamically detected **hardware ID**
(CPU brand, base clock, sockets/cores/logical processors, L1/L2/L3 cache, RAM,
memory slots + clock, OS — via CPUID/registry/SMBIOS on Windows and /proc//sys on Linux)
so results are attributed to the machine that produced them — see
[`benchmark_results.txt`](benchmark_results.txt) for a full run with machine details.

| Optimization | Impact |
|--------------|--------|
| Handler dispatch `std::array[16][136]` | O(1) index, 35 KB compact table (L2-cache resident) |
| `FlatHandler` callbacks (16 bytes = 2 pointers) | no type erasure, no heap on the hot path |
| RingBuffer power-of-two wrap (`& mask`) | one-cycle wrap instead of modulo |
| Zero-copy parsing | span → typed object directly; RSL/LAPDm decode in place |
| Per-MS stack modules | ~2 KB per session (10K sessions = ~20 MB) |

```bash
./build/Release/examples/example_benchmark.exe       # all 12 PD domains (parse + stream)
./build/Release/examples/example_benchmark_stack.exe # full BTS stack (1K MS) component benchmarks
./build/Release/examples/example_multithread.exe     # concurrent parsing
./build/Release/examples/example_zero_copy.exe       # InlineFramer + ZeroCopyStreamProcessor
./build/Release/examples/example_rt_scale.exe        # 1M sessions, real-time event-loop check
```

## How It Compares

| Aspect | osmo-bts (C) | OpenBTS / srsRAN | libgsml3parser |
|--------|-------------|-------------------|----------------|
| **Language** | C (libosmocore) | Legacy C++ | C++20 |
| **Type safety** | enum + manual cast | custom structs | `std::variant` + `tryGet<T>()` |
| **Message types** | hand-coded per message | partial coverage | 236 typed messages, all 12 PD domains |
| **Builder API** | none (manual struct) | partial | fluent builder for every type |
| **FSM + timers** | implicit in handlers | custom | built-in stack modules + procedure framework |
| **Memory model** | heap-allocated structs | varies | stack variants, zero-alloc hot path |
| **LAPDm** | libosmocore (separate) | custom | full state machine, included |
| **Dependencies** | libosmocore + osmo-* | multiple | **zero** (C++20 stdlib only) |

## Quick Start

### Building

```bash
mkdir build && cd build
cmake .. -DBUILD_TESTS=ON -DBUILD_EXAMPLES=ON
cmake --build . --config Release --parallel
ctest --output-on-failure          # run the test suite
```

| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_SHARED_LIBS` | OFF | Shared library instead of static |
| `BUILD_TESTS` | OFF | Unit tests (Google Test 1.14.0 via FetchContent) + C89 check of the C ABI header |
| `BUILD_EXAMPLES` | OFF | Example programs (20) |
| `ENABLE_FUZZING` | OFF | libFuzzer targets in `fuzz/` (requires Clang/LLVM; no-op with a status message on MSVC) |
| `ENABLE_ASAN` | OFF | AddressSanitizer for Debug builds (MSVC 17.8+) |

### Using in Your Project

One include, one link:

```cmake
find_package(gsml3parser REQUIRED)
target_link_libraries(myapp PRIVATE gsml3parser::gsml3parser)
```

```cpp
#include <gsml3parser/gsml3parser.hpp>  // single umbrella header (L3, LAPDm, stack)
```

### Parsing a Message

```cpp
auto msg = gsml3parser::parseL3Hex("600D00");  // RR Channel Release
if (msg) {
    std::cout << gsml3parser::messageName(*msg) << "\n";  // "ChannelRelease"
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
auto result = gsml3parser::parseL3Hex("600D");  // truncated Channel Release
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
| `SourceExhausted` | ByteSource reached EOF without a complete frame |

### BTS Examples

The `examples/` directory contains complete, runnable demonstrations:

| Example | Description |
|---------|-------------|
| `example_parse_file` | Parse L3 messages from hex strings/files, typed access via `tryGet<>`, round-trip serialization |
| `example_streaming` | `L3Framer` + stream processing over `SpanByteSource` and a `RingBuffer` producer/consumer |
| `example_multithread` | Concurrent parsing across all 12 PD domains with immutable configs (no mutex) |
| `example_zero_copy` | `InlineFramer` + `ZeroCopyStreamProcessor` on a contiguous buffer (e.g. DMA/PCAP) |
| `example_benchmark` / `example_benchmark_stack` | Component benchmarks with hardware-ID reporting |
| `example_lapdm_entity` | Full LAPDm lifecycle: SABME/UA, UI + segmented I-frame data, T200 retransmission, DISC |
| `example_bts_paging` | Paging cycle: Builder → L3 bytes → LAPDm UI frame → unwrap → parse → verify |
| `example_bts_channel_assignment` | Channel Request on RACH → ImmediateAssignment response, full round-trip |
| `example_bts_rach_assignment` | RACH → channel allocation → assignment → MS response using stack modules |
| `example_bts_sysinfo` | System Information (SI1–SI4) construction for BCCH broadcast |
| `example_bts_dispatcher` | `ProtocolDispatcher` routing: specific, domain fallback, TI-based handlers |
| `example_bts_location_update` | Location updating flow: CM Service Request, identity verification, TMSI reallocation |
| `example_bts_paging_call` | Paging → channel assignment → MM auth → CC call setup |
| `example_subscriber_registry` | Registry workflow: create/assign/tick/remove subscriber sessions |
| `example_rt_scale` | 1M sessions, 10 ms event-loop ticks, real-time overrun verification |
| `example_procedure_location_update` | Full Location Update procedure via `ProcedureRunner` |
| `example_procedure_call_setup` | Mobile-Originated Call Setup via `ProcedureRunner` |
| `example_rsl_pipeline` | A-bis RSL in → L3 out (incl. TL16V `L3Info` wrapping) and RSL response out |
| `example_reference_bts` | Location Update + MO Call Setup chains through `ProcedureOrchestrator` with `ShardedSubscriberRegistry` |

### BTS Procedure Framework — High-Level Protocol Procedures

The highest level of abstraction: pre-built protocol procedures that encapsulate FSM, timers, transactions, and response generation. Feed L3 messages into a `ProcedureOrchestrator` (for compound chains) or use `ProcedureRunner` (individual procedures). The framework returns a `ResponseToken` indicating which message to build, and the caller uses `ResponseBuilder::buildResponseFromToken()` to generate bytes in a pre-allocated Arena buffer (zero heap allocation):

```cpp
#include <gsml3parser/gsml3parser.hpp>
#include <gsml3parser/stack/procedure_orchestrator.h>

using namespace gsml3parser;

// Create subscriber session; the BTS application keeps one orchestrator
// per session for compound procedure chains.
SubscriberRegistry registry;
auto* session = registry.createByTMSI(0x12345678);
ProcedureOrchestrator orchestrator;   // app-owned, one per session

// Feed incoming L3 messages — the orchestrator auto-chains sub-procedures.
auto result = orchestrator.feed(incomingMessage, session);

if (result.action == ProcedureStepResult::Action::SendResponseWithToken) {
    uint8_t buf[512];
    int n = ResponseBuilder::buildResponseFromToken(
        result.responseToken, {buf, sizeof(buf)}, session);
    if (n > 0) sendToMS(buf, n);
}

// Feed typed external decisions (e.g. VLR accept/reject, AuC RAND+SRES).
// The orchestrator forwards the session, so the procedure records the
// response parameters (RAND, new TMSI, ...) into session->response.
VLRDecision vlr{true, 0x87654321u, MMRejectCause::Zero};
orchestrator.feedExternalTyped(vlr);

AuthChallenge chal{};
std::memcpy(chal.rand.data(), aucRand, 16);
std::memcpy(chal.expectedSres.data(), aucSres, 4);  // SRES is big-endian
orchestrator.feedExternalTyped(chal);

// Event loop: tick chain timers; retransmission tokens surface here.
orchestrator.tickAll(std::chrono::milliseconds(10));
if (auto rt = orchestrator.takeRetransmissionToken(); rt != ResponseToken::None) {
    // build and send via buildResponseFromToken(rt, ...)
}
```

**Available procedures:**

| Procedure | Spec | Description |
|-----------|------|-------------|
| `LocationUpdateProcedure` | TS 24.008 4.4.1 | Full location updating with auth + VLR decision |
| `AuthenticationProcedure` | TS 24.008 4.4.2 | RAND/SRES exchange with external AuC integration |
| `CallSetupMOPercedure` | TS 24.008 6.1 | Mobile Originated Call (RACH → Active) |
| `CallSetupMTProcedure` | TS 24.008 6.1 | Mobile Terminated Call (Paging → Active) |
| `ChannelAssignmentProcedure` | TS 04.08 9.1.2 / 9.1.35 | RACH → Immediate Assignment → channel seizure |
| `CipheringModeProcedure` | TS 24.008 4.4.3 | A5 ciphering activation |
| `PagingProcedure` | TS 04.08 9.1.25 | Paging request (Type1/2/3) with T3109 retransmission |
| `HandoverProcedure` | TS 04.08 9.1.40 | Handover command/response flow |
| `CallReleaseProcedure` | TS 24.008 6.1 | Call release (disconnect → release complete) |
| `IMSIDetachProcedure` | TS 24.008 4.4.6 | IMSI detach procedure |

The orchestrator also runs inline phases between procedures — CM service request handling, identity verification (T3102 with retransmission), ciphering mode, periodic location updating, and call release — keeping the session FSMs in sync as it goes.

**Abis/RSL Interface (TS 48.058):**

Parse and construct A-bis RSL messages for BSC integration:

```cpp
auto rslMsg = RSLParser::parse(rawRSLBytes);       // Expected<RSLParsedMessage>
if (rslMsg) {
    if (RSLParser::hasL3Payload(rslMsg.value())) {
        auto l3Payload = RSLParser::extractL3(rslMsg.value());
        // ... process the L3 message through ProcedureRunner / orchestrator ...
    }
}
auto response = RSLBuilder::buildDataInd(chanNr, linkId, responseL3Bytes);
```

`RSLBuilder` covers DATA_IND/REQ, UNIT_DATA_*, CHAN_ACTIV_ACK/NACK, RF_CHAN_REL_ACK, CONN_FAIL, MEAS_RES, HANDO_DET, CCCH_LOAD_IND, CHAN_RQD and DELETE_IND — 13 frame builders, each with a `std::vector` and a zero-alloc `span` overload (11 are BTS→BSC; `buildDataReq()` and `buildUnitDataReq()` are BSC→BTS, provided for loopback testing).

See `examples/`, [doc/bts_integration.md](doc/bts_integration.md) (step-by-step integration guide), and [doc/bts_architecture.md](doc/bts_architecture.md) for full examples.

## Architecture

```
ByteSource (Span/File/RingBuffer)
    -> L3Framer (L2-length framing by default; header-scan mode available)
    -> parseL3() -> Expected<ParsedMessage>
    -> std::visit / tryGet<T>() for typed access
```

Layered design, bottom to top:

1. **Bit-level I/O** — `BitReader`/`BitWriter`, bounds-checked, MSB-first, no heap
2. **Message types** — plain C++ structs with `parse()` and `write()`, no inheritance; dispatch via constexpr function-pointer tables per domain (O(1) MTI index)
3. **Variant dispatch** — `ParsedMessage` holds the 12 domains on the stack (`sizeof(ParsedMessage)` = 416 bytes on x64, static-asserted < 8 KB)
4. **Streaming** — `ByteSource` → `L3Framer` (deterministic L2-length octet framing by default) → `L3StreamProcessor` / `InlineFramer` + `ZeroCopyStreamProcessor` (views into the input, zero allocation)
5. **Stack modules** — MSContext, TimerManager, TransactionManager, FSMs, ChannelPool, SubscriberRegistry
6. **Procedure framework** — `ProcedureRunner` + `ProcedureOrchestrator`, `ResponseToken` → `ResponseBuilder` (pre-allocated buffer, zero heap)
7. **A-bis RSL** — zero-copy RSL parse/extract/build for BSC integration
8. **C ABI** — stable C89 interface over the full stack

## Supported Messages Summary

| Domain | PD | Messages |
|--------|----|----------|
| Radio Resource (RR) | `0x06` | 98 |
| Call Control (CC) | `0x03` | 24 |
| Mobility Management (MM) | `0x05` | 20 |
| GPRS Session Management (SM) | `0x0a` | 29 |
| GPRS Mobility Management (GMM) | `0x08` | 23 |
| SMS | `0x09` | 19 |
| Broadcast Call Control (BCC) | `0x01` | 8 |
| Group Call Control (GCC) | `0x00` | 8 |
| Supplementary Services (SS) | `0x0b` | 3 |
| Location Services (LS) | `0x0c` | 2 |
| Extended PD | `0x0e` | 1 |
| Test Procedure PD | `0x0f` | 1 |
| **Total** | 12 domains | **236** |

Information elements are defined alongside their domain (e.g. `common/l3common.h`, `cc/l3ccelements.h`, `gmm/l3gmmelements.h`, `sm/l3smelements.h`) and parsed/serialized as part of each message.

**Full message catalog:** [doc/messages.md](doc/messages.md)

## Key Features

- **Full L3 message parsing** — binary to typed C++ objects with compile-time dispatch via `std::variant` (236 message types, all 12 PD domains)
- **Fluent Builder API** — construct any L3 message from scratch; every type exposes `builder()`
- **Message generation** — typed objects to binary data (`writeL3` / `writeL3Bytes` / `writeL3Hex`) for test harnesses, fuzzing, replay
- **Full LAPDm protocol** — SABME/UA/DISC state machine, I-frame segmentation with k=1 retransmission queueing, T200 timer, contention resolution, 4 KB-bounded reassembly
- **ProtocolDispatcher** — O(1) PD+MTI callback routing over a compact 16×136 handler table, plus TI-based dispatch for CC/SS and domain/global fallbacks
- **`std::format` support** — `enum_formatters.h` provides `std::formatter` specializations for protocol enums (delegating to `std::ostream <<`)
- **`Expected<T>` result type** — zero-allocation errors with bit-position tracking (`ParseError` carries an inline message buffer)
- **Immutable ParserConfig** — thread-safe by design; builder-style `withLogLevel()`/`withStrictFraming()` return new instances
- **Zero heap allocation on hot path** — `ParsedMessage` variant and stack modules live on the stack / in fixed arrays
- **Compile-time message dispatch** — `std::variant` + `std::visit`, constexpr parse tables, no RTTI
- **Bitstream I/O** — `ByteSource` hierarchy (`SpanByteSource`, `FileByteSource`, power-of-two-masked `RingBuffer`) for streaming
- **Zero-copy stream processing** — `InlineFramer` and `ZeroCopyStreamProcessor` return views into the input buffer
- **Procedure framework** — `ProcedureOrchestrator` auto-chains compound procedures (Location Update, Call Setup) with a zero-alloc `ResponseToken` → `ResponseBuilder` pattern
- **TypedExternalData** — strongly typed external data (`AuthChallenge`, `VLRDecision`, `PagingTrigger`, `CipheringParameters`, `HandoverTarget`) instead of raw byte spans
- **Sharded, thread-safe scaling primitives** — `ShardedChannelPool` and `ShardedSubscriberRegistry<N>` for concurrent access; O(active) timer/procedure ticks skip idle sessions
- **Arena allocator** — segmented bump allocator for high-throughput response building
- **Zero external dependencies** — C++20 standard library only (Google Test is a test-time FetchContent, not a library dependency)
- **Fuzzing targets** — eight libFuzzer entry points (`fuzz/`, `ENABLE_FUZZING=ON`): `parseL3`, RSL parse, LAPDm frame decode, LAPDm entity, `L3Framer`, `ProcedureOrchestrator`, subscriber registry, C API
- **C ABI** — stable C89 header (`gsml3parser_c.h`) for FFI (C, Python ctypes/cffi, Rust, Go): L3 parse/reparse/serialize with typed accessors and builders for 40+ message types, RSL parsing + 13 frame builders, LAPDm zero-copy decoder + full entity, subscriber registry/sessions/timers, and the orchestrator; the header compiles clean under strict C89
- **Spec-compliant** — TS 24.008 / GSM 04.08 (MM/CC), TS 44.018 / GSM 04.07 (RR), GSM 04.06 + TS 48.008 (LAPDm), TS 48.058 (A-bis RSL), GSM 03.38 + TS 24.011 + 23.040 (SMS), GSM 04.80 / 02.90 / 23.038 (broadcast call), GSM 05.02 / 05.05 (channels & frequency planning)

## Thread Safety

- **ParserConfig** — immutable; builder methods return new instances.
- **parseL3()** — stateless function, thread-safe with shared read-only config.
- **BitReader/BitWriter, `RSLParser`** — plain value types / pure functions, no shared state.
- **Arena** — not thread-safe; each thread uses its own instance.
- **Stack modules (MSContext, TimerManager, TransactionManager, FSMs, ProcedureRunner/Orchestrator, LAPDmEntity)** — one instance per subscriber, accessed from a single thread; no internal locks on the hot path.
- **SubscriberRegistry** — not thread-safe (single event-loop access); **ShardedSubscriberRegistry** and **ShardedChannelPool** are thread-safe via per-shard locks (`findLocked()` keeps the shard shared lock held through a returned guard).
- **LAPDmEntity TX path** — the encode buffer is reused after the first send; transmit or copy the frame synchronously inside the L1 callback.

## Testing & Fuzzing

- GoogleTest suite: parse/golden-vector (cross-validated against Osmocom TTCN-3 vectors), round-trip, builder, LAPDm FSM, dispatcher, all procedures and orchestrator chains, RSL, C ABI + C89 header check, high-load stress (1M/2M sessions) and concurrency tests — 2100+ tests.
- `ENABLE_FUZZING=ON` (Clang/LLVM only) builds the eight targets with ASan/UBSan and registers 2000-run smoke tests in ctest. The build-release workflow additionally runs a ThreadSanitizer pass over the concurrency tests on Linux and attaches a prebuilt static+shared package to each GitHub Release.

## Documentation

| Document | Topic |
|----------|-------|
| [doc/API.md](doc/API.md) | Full API reference (63 numbered sections) |
| [doc/bts_architecture.md](doc/bts_architecture.md) | BTS architecture, threading model, scaling to millions of MS |
| [doc/bts_integration.md](doc/bts_integration.md) | **Primary guide for BTS developers**: ProcedureOrchestrator, ResponseToken pattern, typed external data |
| [doc/messages.md](doc/messages.md) | Complete catalog of all 236 message types |
| [doc/boundaries.md](doc/boundaries.md) | What the library does and does not include, with integration points for each excluded domain |

## Build Requirements

| Requirement | Minimum Version |
|-------------|-----------------|
| C++ compiler | MSVC 2022 (17.x), GCC 13+, or Clang with a C++20 `std::format`-capable standard library |
| CMake | 3.20 |
| Standard Library | C++20 (`std::variant`, `std::span`, concepts, `std::format`) |

## Roadmap

- [ ] Python bindings (pybind11)

## License

MIT License. See [COPYING](COPYING) for details.

## Acknowledgments

- Copyright 2026 momentics &lt;momentics@gmail.com&gt;
- Copyright libgsml3parser contributors
- Golden test vectors validated against the [Osmocom](https://osmocom.org/) TTCN-3 testing infrastructure
