# libgsml3parser

**GSM Layer 3 Protocol Stack — Parse, Build, and Run a Software BTS in C++20**

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Build](https://github.com/momentics/libgsml3parser/actions/workflows/build-release.yml/badge.svg)](https://github.com/momentics/libgsml3parser/actions/workflows/build-release.yml)
[![Version](https://img.shields.io/badge/Version-0.19.0-blue.svg)](https://github.com/momentics/libgsml3parser/releases)

A type-safe, **zero-allocation C++20** library with **no external dependencies**, spanning the full GSM
signalling chain of a software Base Transceiver Station: parse and build all **236 L3 message types
across 12 PD domains**, run the complete **LAPDm** (L2) entity and **A-bis RSL** interface, manage
per-subscriber state (context, FSMs, timers, transactions), and drive ten spec-based **protocol
procedures** — from raw radio bytes up to working MO/MT call flows.

## Why This Library?

Building a software BTS means implementing the entire Layer-3 signalling stack: parsing hundreds of
binary messages, driving protocol state machines, tracking timers, correlating request-response
transactions, and generating correct responses at real-time speed. The existing solutions each leave a gap:

- **osmo-bts** (C / libosmocore) — hand-coded parsers per message type, no builder API, implicit FSMs
  scattered across handler code;
- **OpenBTS / srsRAN** — custom C++ structs with manual byte construction and limited coverage;
- **TTCN-3 test suites** — excellent for validation, unusable as a production protocol stack.

libgsml3parser closes the gap: one type-safe C++20 package that provides everything from raw
parse/serialize to per-subscriber procedure state machines — ready to connect to any SDR backend or a
BSC over A-bis RSL, with zero third-party dependencies to carry.

## What You Save

| Without libgsml3parser | With libgsml3parser |
|------------------------|---------------------|
| Hand-roll binary parsers for 200+ message types | `parseL3Hex("600D00")` — one call, typed result |
| Manual byte construction for responses | Fluent builder: `.addTMSI(0x12345678, SDCCHType).build()` |
| Scatter/gather FSM logic across handlers | Pre-built `ProcedureOrchestrator` auto-chains Location Update, Auth, Call Setup |
| Track timers with raw `std::map` + cron jobs | `TimerManager` — fixed 32-slot arrays, zero allocation, T3101–T3395 built in |
| Correlate request/response with custom TI tables | `TransactionManager` — O(1) TI lookup, 0.004 µs per match |
| Debug hex dumps by eye | `std::format` specializations for protocol enums, `Expected<T>` with bit-position errors |

## Who Is This For?

| Audience | What You Get |
|----------|-------------|
| **Software BTS developers** | Drop-in replacement for the osmo-bts L3 layer: parse, build, FSMs, timers, procedures, LAPDm — link `gsml3parser` and go |
| **Protocol testers & fuzzers** | Bidirectional binary↔typed API, golden vectors cross-validated against Osmocom TTCN-3, libFuzzer targets for every major entry point |
| **SDR / radio hobbyists** | Complete L2 (LAPDm) + L3 stack for the Um interface plus A-bis RSL — no networking or SIP dependencies |

## What You Get

Four layers, from raw bits to protocol state:

1. **L3 Parser & Serializer** — hex/bytes ↔ typed `std::variant` objects; compile-time `tryGet<T>()`; a
   fluent `builder()` for every message type; zero heap on the hot path (`sizeof(ParsedMessage)` = 416 B).
2. **LAPDm Protocol Entity** — GSM 04.06 / TS 45.006 state machine (SABME/UA/DISC), I-frame segmentation
   with k=1 and T200 retransmission, contention resolution, 4 KB-bounded reassembly.
3. **BTS Stack Modules** — MSContext, TimerManager (T3101–T3395, zero-alloc O(active) tick),
   TransactionManager (O(1) TI index), RR/MM/CC state machines, ChannelPool / ShardedChannelPool,
   SubscriberRegistry — `sizeof(SubscriberSession)` = 2056 B (10K sessions ≈ 20 MB).
4. **Procedure Framework** — `ProcedureRunner` + `ProcedureOrchestrator` auto-chain ten spec-based
   procedures (Location Update, Authentication, Call Setup MO/MT, Channel Assignment, Ciphering,
   Paging, Handover, Release, IMSI Detach) through a zero-heap `ResponseToken` → `ResponseBuilder`
   pattern; plus A-bis RSL parsing and 13 frame builders for BSC integration.

```cpp
auto msg = gsml3parser::parseL3Hex("600D00");            // RR Channel Release
if (msg) std::cout << gsml3parser::messageName(*msg);    // "ChannelRelease"

auto paging = L3PagingRequestType2::builder()
    .addTMSI(0x12345678, ChannelType::SDCCHType).build();// fluent construction
```

## Supported Messages Summary

All 12 protocol domains — RR 98 · SM 29 · CC 24 · GMM 23 · MM 20 · SMS 19 · BCC 8 · GCC 8 · SS 3 · LS 2 ·
Extended + Test PDs 2: **236 message types** in total, with Information Elements and enums defined per domain.

Full catalog (MTIs, directions, IEs, dispatch edge cases such as TIF=1 short messages and parse-slot
shadowing): [doc/messages.md](doc/messages.md).

## Quick Start

```bash
cmake -S . -B build -DBUILD_TESTS=ON -DBUILD_EXAMPLES=ON
cmake --build build --config Release --parallel
ctest --test-dir build --output-on-failure     # 2100+ tests
```

Requirements: C++20 (MSVC 2022 17.x, GCC 13+, or Clang with a `std::format`-capable stdlib) and CMake
3.20. All build options (`BUILD_SHARED_LIBS`, `ENABLE_FUZZING`, `ENABLE_ASAN`, …), the `find_package()`
consumer pattern, the error-handling model, and the full API tour: [doc/API.md](doc/API.md).

## Performance

Numbers that matter for a real-time radio stack (Release, single core):

| Metric | Result |
|--------|--------|
| L3 parse throughput (per message type) | 7.7 – 26.5 M msg/s |
| Mixed-domain stream (all 12 PDs) / zero-copy | 11.0 / 12.9 M msg/s |
| Full BTS stack dispatch (1K MS: timers, FSMs, correlation) | ~66 – 86 M msg/s |
| TimerManager tick / Transaction match | 42.6 M ticks/s / 0.004 µs per match |
| 1M and 2M sessions (create / TMSI lookup / tick 10K active) | 1195 / 127 / 1.4 ms and 1096 / 258 / 1.4 ms |
| `example_rt_scale`: 1M sessions, real-time event loop | 6.2 M msg/s aggregate, 0 overruns / 1000 ticks, 2.0 GB peak |

Every benchmark and stress test prints the dynamically detected hardware ID (CPU, caches, RAM, OS), so
results are attributed to the machine that produced them; a full annotated run:
[`benchmark_results.txt`](benchmark_results.txt). Verify on your own machine with `example_benchmark`,
`example_benchmark_stack` and `example_rt_scale`. Optimization techniques behind the numbers:
[doc/bts_architecture.md §6](doc/bts_architecture.md#6-performance-considerations), [doc/API.md §62](doc/API.md).

## How It Compares

| Aspect | osmo-bts (C) | OpenBTS / srsRAN | libgsml3parser |
|--------|-------------|-------------------|----------------|
| **Language** | C (libosmocore) | Legacy C++ | C++20 |
| **Type safety** | enum + manual cast | custom structs | `std::variant` + `tryGet<T>()` — compile-time, no RTTI |
| **Message types** | hand-coded per message | partial coverage | 236 typed messages, all 12 PD domains |
| **Builder API** | none (manual struct) | partial | fluent builder for every type |
| **FSM + timers + correlation** | implicit in handlers | custom | built-in stack modules + procedure framework |
| **LAPDm / A-bis RSL** | libosmocore (separate) | custom | full LAPDm entity + RSL parse/build included |
| **Dependencies** | libosmocore + osmo-* | multiple | **zero** (C++20 stdlib only) |

## Documentation — Start Here

Every detail lives in a dedicated guide; this README is the pitch and the index.

| Document | What It Covers |
|----------|----------------|
| [doc/API.md](doc/API.md) | Full API reference (64 numbered sections): core types, bit I/O, streaming, parser/serializer, builders and IEs/enums of all 12 domains, LAPDm, dispatcher, arena, every stack module, RSL, all procedures, C ABI, FFI bindings + spec conformance notes |
| [doc/bts_integration.md](doc/bts_integration.md) | **Primary guide for BTS developers**: step-by-step event loop with `ProcedureOrchestrator`, full worked procedure chains (Location Update, Call Setup MO, Paging), AuC/VLR/BSC typed-data integration, LAPDm link management, L3 timer reference table, SI broadcast, production error handling |
| [doc/bts_architecture.md](doc/bts_architecture.md) | Two usage modes (L3 Parser vs BTS Stack), component & data-flow diagrams, PHY/SDR integration points, thread-safety matrix, per-MS memory footprint, allocation-free hot paths, scaling guidelines to millions of sessions |
| [doc/messages.md](doc/messages.md) | Complete message catalog: all 236 types with MTIs and directions, CC/GMM/SM IEs, SMS CP/RP/TP layers, dispatch edge cases (TIF=1 short messages, build-only types, parse-slot shadowing) |
| [doc/boundaries.md](doc/boundaries.md) | What the library intentionally excludes — PHY/SDR, speech codecs, A5 ciphering, OML, SIP/media gateways, PS full stack, configuration, logging — and the exact integration point for each |
| [examples/](examples/) | 20 runnable demos (see below), incl. full BTS flows, benchmarks, and a 1M-session real-time loop |
| [bindings/README.md](bindings/README.md) | FFI bindings (Python / Go / Rust): quickstarts, ownership & threading model, callback safety rules, unified test gate |
| [bindings/python/README.md](bindings/python/README.md) | Python `ctypes` binding over the stable C ABI: layout, prebuilt-library loading, quickstart, queue-model BTS stack, error model, ownership & threading contract |

## Examples

| Example | Description |
|---------|-------------|
| `example_parse_file` | Parse L3 from hex strings/files, typed access via `tryGet<>`, round-trip serialization |
| `example_streaming` | `L3Framer` + stream processing over `SpanByteSource` and a `RingBuffer` producer/consumer |
| `example_multithread` | Concurrent parsing across all 12 PD domains with immutable configs (no mutex) |
| `example_zero_copy` | `InlineFramer` + `ZeroCopyStreamProcessor` on a contiguous buffer (DMA/PCAP-style) |
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

## FFI and Bindings

- Stable C89 C ABI over the full stack: `include/gsml3parser/gsml3parser_c.h` (handles, error model,
  validation, threading contract documented in [doc/API.md §63](doc/API.md#63-c-api-gsml3parser_ch)).
- **FFI bindings** — first-party Python (ctypes, zero third-party deps), Go (cgo with `//export` +
  `cgo.Handle` callback bridges), and Rust (sys crate + safe wrapper, `Send`, no code generation) over the
  stable C ABI, each demonstrating the BTS stack with callbacks; unified build/test gate included
  (`bindings/`, see [bindings/README.md](bindings/README.md)).
- **Python** — `ctypes` binding in [bindings/python/](bindings/python/) (stdlib-only, full API surface,
  RAII wrappers + a queue-model BTS stack): see its [README](bindings/python/README.md).
- [x] FFI bindings for Python (ctypes), Go (cgo), and Rust (safe wrapper) over the C ABI (`bindings/`, unified gate: `scripts/verify_bindings.ps1`).

## Testing & Fuzzing

Quality you can audit, not just trust:

- GoogleTest suite: 2100+ tests — parse/golden-vector (cross-validated against Osmocom TTCN-3 vectors),
  round-trip, builders, LAPDm FSM, dispatcher, all procedures and orchestrator chains, RSL, C ABI +
  strict-C89 header check, high-load stress (1M/2M sessions) and concurrency tests.
- `ENABLE_FUZZING=ON` (Clang/LLVM) builds eight libFuzzer targets (L3 parse, RSL parse, LAPDm frame
  decode, LAPDm entity, L3Framer, orchestrator, subscriber registry, C API) with ASan/UBSan; CI adds a
  ThreadSanitizer pass on Linux and attaches a prebuilt static+shared package to each GitHub Release.

## Thread Safety

Parse/build/RSL entry points are stateless; per-session stack modules are single-thread by design (the
one-instance-per-MS event-loop model keeps the hot path lock-free); `ShardedSubscriberRegistry` and
`ShardedChannelPool` provide the thread-safe variants. Complete matrix:
[doc/bts_architecture.md §5](doc/bts_architecture.md#5-thread-model).

## License

MIT License. See [COPYING](COPYING) for details. Golden test vectors validated against the
[Osmocom](https://osmocom.org/) TTCN-3 testing infrastructure. Copyright 2026 momentics
&lt;momentics@gmail.com&gt; and libgsml3parser contributors.
