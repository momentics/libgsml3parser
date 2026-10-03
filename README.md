# libgsml3parser

**GSM Layer 3 Protocol Stack — Parse, Build, and Run a Software BTS in C++20**

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Build](https://github.com/momentics/libgsml3parser/actions/workflows/build-release.yml/badge.svg)](https://github.com/momentics/libgsml3parser/actions/workflows/build-release.yml)
[![Version](https://img.shields.io/badge/Version-0.19.0-blue.svg)](https://github.com/momentics/libgsml3parser/releases)

A type-safe, **zero-allocation C++20** library with **no external dependencies**, spanning the full GSM
signalling chain of a software Base Transceiver Station: **236 L3 message classes across 12 PD
domains** — typed fields for RR/SM/CC/GMM/MM/SS and the SMS CP/RP/TP layers, best-effort opaque-body
parsing for the SMS L3 primitives (MTI 0x11–0x1E) and BCC/GCC/LS (no normative reference templates
exist for these blocks, so bit-level conformance is not claimed), and passthrough for the Extended/Test
PDs — plus the complete **LAPDm** (L2) entity and **A-bis RSL** interface, per-subscriber state
(context, FSMs, timers, transactions), and ten spec-based **protocol procedures** — from raw radio bytes
up to working MO/MT call flows.

## Why This Library?

Building a software BTS means implementing the entire Layer-3 signalling stack: parsing hundreds of
binary messages, driving protocol state machines, tracking timers, correlating request-response
transactions, and generating correct responses at real-time speed. The existing solutions each leave a gap:

- **Hand-written C BTS stacks** — hand-coded parsers per message type, no builder API, implicit FSMs
  scattered across handler code;
- **Ad-hoc C++ stacks** — custom structs with manual byte construction and limited coverage;
- **TTCN-3 conformance suites** — excellent for validation, unusable as a production protocol stack.

libgsml3parser closes the gap: one type-safe C++20 package that provides everything from raw
parse/serialize to per-subscriber procedure state machines — ready to connect to any SDR backend or a
BSC over A-bis RSL, with zero third-party dependencies to carry.

## What You Save

| Without libgsml3parser | With libgsml3parser |
|------------------------|---------------------|
| Hand-roll binary parsers for 200+ message types | `parseL3Hex("060D41")` — one call, typed result |
| Manual byte construction for responses | Fluent builder: `.addTMSI(0x12345678, SDCCHType).build()` |
| Scatter/gather FSM logic across handlers | Pre-built `ProcedureOrchestrator` auto-chains Location Update, Auth, Call Setup |
| Track timers with raw `std::map` + cron jobs | `TimerManager` — fixed 32-slot arrays, zero allocation; built-in default durations for the 19 named GSM/GPRS timers (T3101, T3102, T3103, T3106, T3108, T3109, T3111, T3112, T3113, T3310, T3311, T3312, T3314, T3315, T3320, T3321, T3322, T3334, T3395); other timer IDs accept a custom duration |
| Correlate request/response with custom TI tables | `TransactionManager` — O(1) TI lookup, 0.004 µs per match |
| Debug hex dumps by eye | `std::format` specializations for protocol enums, `Expected<T>` with bit-position errors |

## Who Is This For?

| Audience | What You Get |
|----------|-------------|
| **Software BTS developers** | Drop-in L3 layer for a software BTS: parse, build, FSMs, timers, procedures, LAPDm — link `gsml3parser` and go |
| **Protocol testers & fuzzers** | Bidirectional binary↔typed API, golden vectors pinned to the normative TS wire layouts, libFuzzer targets for every major entry point |
| **SDR / radio hobbyists** | Complete L2 (LAPDm) + L3 stack for the Um interface plus A-bis RSL — no networking or SIP dependencies |

## What You Get

Four layers, from raw bits to protocol state:

1. **L3 Parser & Serializer** — hex/bytes ↔ typed `std::variant` objects; compile-time `tryGet<T>()`; a
   fluent `builder()` for every message type; zero heap on the hot path (`sizeof(ParsedMessage)` = 400 B).
2. **LAPDm Protocol Entity** — GSM 04.06 / TS 44.064 state machine (SABME/UA/DISC), I-frame segmentation
   with k=1 and T200 retransmission, contention resolution, 4 KB-bounded reassembly.
3. **BTS Stack Modules** — MSContext, TimerManager (fixed 32-slot arrays, zero allocation; built-in
   default durations for the 19 named GSM/GPRS timers (T3101, T3102, T3103, T3106, T3108, T3109,
   T3111, T3112, T3113, T3310, T3311, T3312, T3314, T3315, T3320, T3321, T3322, T3334, T3395); other
   timer IDs accept a custom duration; O(active) tick),
   TransactionManager (O(1) TI index), RR/MM/CC state machines, ChannelPool / ShardedChannelPool,
   SubscriberRegistry — `sizeof(SubscriberSession)` = 2056 B (10K sessions ≈ 20 MB).
4. **Procedure Framework** — `ProcedureRunner` + `ProcedureOrchestrator` auto-chain ten spec-based
   procedures (Location Update, Authentication, Call Setup MO/MT, Channel Assignment, Ciphering,
   Paging, Handover, Release, IMSI Detach) through a zero-heap `ResponseToken` → `ResponseBuilder`
   pattern; plus A-bis RSL parsing and 13 frame builders for BSC integration.

```cpp
auto msg = gsml3parser::parseL3Hex("060D41");            // RR Channel Release
if (msg) std::cout << gsml3parser::messageName(*msg);    // "ChannelRelease"

auto paging = L3PagingRequestType2::builder()
    .addTMSI(0x12345678, ChannelType::SDCCHType).build();// fluent construction
```

## Supported Messages Summary

All 12 protocol domains — RR 99 · SM 29 · CC 24 · GMM 23 · MM 19 · SMS 19 · BCC 8 · GCC 8 · SS 3 · LS 2 ·
Extended + Test PDs 2: **236 message types** in total, with Information Elements and enums defined per domain.

Full catalog (MTIs, directions, IEs, dispatch edge cases such as TIF=1 short messages and parse-slot
shadowing): [doc/messages.md](doc/messages.md).

## Wire Format Conformance

All parse/build paths for RR/SM/CC/GMM/MM and the SMS CP/RP/TP layers follow the normative 3GPP TS wire
layouts; golden vectors are pinned in the test suite. The SMS L3 primitives (MTI 0x11–0x1E) and the
BCC/GCC/LS blocks have no normative reference templates, so they are parsed best-effort as opaque bodies
and bit-level conformance is not claimed for them. Highlights of the non-obvious encodings:

**Identities (TS 24.008)** — LAI/RAI pack the PLMN as `[MCC2|MCC1][MNC3/F|MCC3][MNC2|MNC1]` BCD octets
plus a 16-bit LAC (plus one RAC octet for RAI); `mcc()`/`mnc()` return the digits in natural order.
The mobile identity value starts with `[first digit(4)|odd-count(1)|type(3)]` and encodes digit pairs
as `[next digit or F fill|current digit]`; a TMSI starts with the spare 'F' nibble, a zero bit and
type '100'B (first octet `0xF4`).

**MM (TS 24.008)** — CM Service Request body starts with one octet: CM service type in the high
half-octet, CKSN(3)|reserved(1) in the low; Location Updating Request starts with
`[luType(2)|spare(1)|FOP(1)]` + `[CKSN(3)|reserved(1)]`. Optional IEs after the mandatory part are
preserved opaquely and re-emitted verbatim.

**GMM / SM (TS 24.008)** — Attach/RAU Request pack update/attach type, forL3 and the GPRS CKSN in a
single octet; the DRX parameter is two value octets without an identifier and the MS radio access
capability is a mandatory LV. Reject/failure/status messages start with a single bare cause value
octet. SM ACTIVATE PDP CONTEXT REQUEST carries the NSAPI and the negotiated LLC SAPI as two
octets ([NSAPI(4)|spare(4)][LLC SAPI(4)|spare(4)]), requested QoS and requested PDP address as
positional LVs, the APN as TLV `0x28`, PCO as TLV `0x27` and an optional request type
`0xAx`; DEACTIVATE PDP CONTEXT REQUEST starts with the SM cause octet followed by an optional
tear-down indicator TV `0x09`.

**RR (TS 44.018)** — Ciphering Mode Command is exactly one body octet
`[sC(1)|algorithm(3)][cR(1)|spare(3)]`; SI1 carries the cell channel description (ARFCN(10)+BSIC(6),
two octets) plus RACH control parameters (three octets) and at most one rest octet; the access-class
bitmap is a 16-bit value written low byte first (AC *i* ↔ bit *i*); a set bit bars the class
(emergency = class 10); Paging Request Type 1/2/3 start with the octet `[channelNeeded(4)][pageMode(4)]`
(channel needed packed as second|first two-bit fields) followed by the identities — two raw TMSIs plus an
optional third identity (TLV `0x17`) for Type 2, four raw TMSIs for Type 3 — and Paging Response starts
with `[spare(4)][CKSN(4)]`; the Immediate Assignment start time (IEI `0x7C`) is two value octets packing
T1(5)/T3(6)/T2(5); channel numbers use the five-bit type-and-offset codes (`'00001'B` Bm ACCH …
`'10000'B` BCCH, `'10001'B` RACH, `'10010'B` PCH+AGCH, PDCH/CBCH/VAMOS extensions) with
`channelCodeLm/Sdcch4/Sdcch8` helpers.

**LAPDm (GSM 04.06 / TS 51.010-1)** — an initial SABME is accepted on SAPI 0 only when it carries
contention-resolution information; every UA we send mirrors the P/F of the received command; DM while
awaiting establishment cancels T200 and releases the link; a T200-expired I-frame is retransmitted
with P/F set and the current V(R) in N(R).

**A-bis RSL (TS 48.058)** — the Channel Mode IE (0x06) value is exactly four octets:
[reserved(6)|DTX_d(1)|DTX_u(1)], speed indicator (`Speech=0x01`, `Data=0x02`, `Signalling=0x03`),
channel rate type (`Sdcch=0x01`, `TchF=0x08`, `TchH=0x09`, …, VAMOS extensions `0x88`/`0x89`) and a
union octet (speech algorithm / opaque data rate / `0x00` for signalling). Error causes use the
canonical section 9.3.26 values (`RadioLinkFail=0x01`, `ResUnavail=0x2F`, `Proto=0x6F`,
`Interworking=0x7F`, …) and reserved gaps are rejected on emit. The Uplink Measurements IE (0x19) is
three value octets — `[RFU|DTX_d|rxlev_full(6)]`, `[res(2)|rxlev_sub(6)]`, `[res(2)|rxq_full(3)|rxq_sub(3)]`
— with any vendor supplementary bytes preserved verbatim. The Frame Number IE (0x08) packs t1p(5),
t3(6) and t2(5) derived from an absolute TDMA frame number (`t1p=(fn/1326)%32`, `t3=fn%51`,
`t2=fn%26`). The IE catalog covers the full section 9 set (0x01–0x3C; 0x1D is not allocated) plus
vendor extensions 0x60–0x63 and the IPAccess group, encoded as TV with fixed sizes, TL16V for
`L3Info` (0x0B) only, and LV for everything else — including Full BCCH Info (0x27).

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

| Aspect | Hand-written C stack | Ad-hoc C++ stack | libgsml3parser |
|--------|---------------------|------------------|----------------|
| **Language** | C | C++ (manual memory) | C++20 |
| **Type safety** | enum + manual cast | custom structs | `std::variant` + `tryGet<T>()` — compile-time, no RTTI |
| **Message types** | hand-coded per message | partial coverage | 236 typed messages, all 12 PD domains |
| **Builder API** | none (manual struct) | partial | fluent builder for every type |
| **FSM + timers + correlation** | implicit in handlers | custom | built-in stack modules + procedure framework |
| **LAPDm / A-bis RSL** | separate library | custom | full LAPDm entity + RSL parse/build included |
| **Dependencies** | external core library | multiple | **zero** (C++20 stdlib only) |

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
| [bindings/README.md](bindings/README.md) | FFI bindings (Python / Go / Rust) over the stable C ABI: unified quickstarts, ownership & threading model, callback safety rules, extension guide, test gate |
| [bindings/python/README.md](bindings/python/README.md), [bindings/go/README.md](bindings/go/README.md), [bindings/rust/README.md](bindings/rust/README.md) | Per-language binding guides with identical structure: layout, building & linking, quickstart, error model, ownership & callbacks, versioning |

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

The full stack is also reachable through a stable C89 C ABI
(`include/gsml3parser/gsml3parser_c.h`, contract in [doc/API.md §63](doc/API.md#63-c-api-gsml3parser_ch)).
Three first-party bindings share one set of semantics — RAII ownership, the borrowed-session rule,
queue-model callbacks — and carry no third-party runtime dependencies:

| Language | Mechanism | Coverage | Runnable demo |
|----------|-----------|----------|---------------|
| Python | `ctypes`, stdlib only | entire C ABI: 236 functions, completeness pinned by test | [bts_simulation.py](bindings/python/examples/bts_simulation.py) |
| Go | cgo + `cgo.Handle` callback bridges (needs a C toolchain) | v1 surface: 128 functions incl. typed L3 builders | [cmd/gsmexample](bindings/go/cmd/gsmexample/main.go) |
| Rust | handwritten `sys` crate + safe wrapper — `Send`, no bindgen/codegen | same 128-function v1 surface; extern block checked at compile time | [bts_simulation.rs](bindings/rust/gsml3parser/examples/bts_simulation.rs) |

All three run the same "MO call over SDCCH" scenario against the shared C core, and one unified gate
(`scripts/verify_bindings.ps1`, CI on Linux + Windows) builds and tests them together. Per-language
guides (quickstart, building & linking, error model, ownership & callbacks):
[bindings/README.md](bindings/README.md) plus the `README.md` in each binding directory.

## Testing & Fuzzing

Quality you can audit, not just trust:

- GoogleTest suite: 2100+ tests — parse/golden-vector (normative TS wire layouts),
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

MIT License. See [COPYING](COPYING) for details. Golden test vectors are pinned to the normative
3GPP TS wire layouts (TS 24.008, TS 44.018, TS 44.064, TS 48.058 and friends). Copyright 2026 momentics
&lt;momentics@gmail.com&gt; and libgsml3parser contributors.
