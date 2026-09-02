# BTS Architecture on libgsml3parser

This document describes the recommended architecture for building a software Base Transceiver Station (BTS) using libgsml3parser as the L3 protocol stack. It covers component relationships, data flow, threading model, performance characteristics, and scaling guidelines with the Procedure Framework.

## 1. Two Usage Modes

libgsml3parser supports two distinct usage modes, each targeting a different audience and providing a different subset of library capabilities. Choosing the correct mode determines which headers you include and which components you instantiate.

### Mode A: L3 Parser Mode

For traffic analyzers, protocol sniffers, test tools, fuzzing frameworks, and any application that needs to parse or construct GSM L3 messages without managing per-subscriber state. This mode depends only on the core parser/serializer API and has zero dependency on stack modules.

**Components used:**

| Component | Header | Purpose |
|-----------|--------|---------|
| `parseL3()` / `writeL3Bytes()` | `parser.h` | Binary to typed objects and back |
| `ProtocolDispatcher` | `dispatcher.h` | O(1) PD+MTI callback routing |
| Builder API (`MessageType::builder()`) | per-domain headers | Construct any L3 message from scratch |
| `tryGet<T>()` / `messageName()` | `visitor.h` | Compile-time typed access via std::variant |
| `L3Framer` / `L3StreamProcessor` | `bitstream/*.h` | Streaming parse from byte sources |
| `BitReader` / `BitWriter` | `bitreader.h` / `bitwriter.h` | Bit-level I/O |

**Architecture:**

```
┌─────────────────────────────────────────────────────┐
│                  L3 Parser Mode                     │
│                                                     │
│  Raw Bytes ──► parseL3() ──► ParsedMessage          │
│  ParsedMessage ──► writeL3Bytes() ──► Bytes         │
│  ParsedMessage ──► ProtocolDispatcher ──► Callbacks │
│                                                     │
│  ByteSource ──► L3Framer ──► StreamProcessor        │
└─────────────────────────────────────────────────────┘
```

**Example:**

```cpp
#include <gsml3parser/gsml3parser.hpp>

auto msg = gsml3parser::parseL3Hex("060D00");
if (msg) {
    std::cout << gsml3parser::messageName(*msg) << "\n";
    auto bytes = gsml3parser::writeL3Bytes(*msg);
}
```

### Mode B: BTS Stack Mode

For software BTS developers who need a complete Layer 3 signaling stack. This mode includes all Parser Mode capabilities plus per-subscriber state management, protocol procedures with automatic FSM transitions, typed external data integration, and zero-allocation response building.

**Additional components (on top of Parser Mode):**

| Component | Header | Purpose |
|-----------|--------|---------|
| `ProcedureOrchestrator` | `stack/procedure_orchestrator.h` | Auto-chains compound procedures (LU, Call Setup) |
| `ProcedureRunner` | `stack/procedure_runner.h` | Concurrent procedure management per subscriber |
| `SubscriberSession` + `SubscriberRegistry` | `stack/subscriber_registry.h` | Per-MS state management (< 4 KB/session) |
| `ResponseBuilder` + `ResponseToken` | `stack/response_builder.h` | Zero-allocation response generation |
| `TypedExternalData` (`AuthChallenge`, `VLRDecision`, etc.) | `stack/typed_external_data.h` | Type-safe external data integration |
| `ProcedureStateMixin<Derived, State>` | `stack/procedure_state_mixin.h` | CRTP mixin eliminating procedure code duplication |
| `ChannelPool` / `ShardedChannelPool` | `stack/channel_pool.h` | Logical channel allocation/release |
| `TimerManager` / `TransactionManager` | `stack/l3_timer.h` / `stack/transaction.h` | Protocol timers and request-response correlation |
| `LAPDmEntity` | `lapdm_entity.h` | Full LAPDm state machine (GSM 04.06) |

**Architecture:**

```
┌───────────────────────────────────────────────────────────────────┐
│                      BTS Application Logic                        │
│    AuC ──► AuthChallenge{rand, sres}        VLR ──► VLRDecision   │
└────────────────────┬──────────────────────────┬───────────────────┘
                     │ feedExternalTyped()      │ feedExternalTyped()
┌────────────────────▼──────────────────────────▼───────────────────┐
│                    BTS Stack Mode                                 │
│  ┌─────────────────────────────────────────────────────────────┐  │
│  │              ProcedureOrchestrator                          │  │
│  │  CMServiceReq->[Identity]->Auth->Ciphering->LU Accept       │  │
│  └───────────────────┬─────────────────────────────────────────┘  │
│                      │ feed() / tickAll()                         │
│  ┌───────────────────▼─────────────────────────────────────────┐  │
 │  │                  SubscriberSession                          │  │
 │  │  MSContext + RR/MM/CC FSM + TimerManager + TransactionMgr   │  │
 │  │  ResponseContext (response parameters) + ProcedureRunner    │  │
 │  └─────────────────────────────────────────────────────────────┘  │
│                      │                                            │
│  ResponseToken ──► ResponseBuilder::buildResponseFromToken()      │
│                      │ span<uint8_t> (Arena buffer, zero alloc)   │
└──────────────────────┼────────────────────────────────────────────┘
                       │ sendToRadio(span<uint8_t>)
┌──────────────────────▼────────────────────────────────────────────┐
│              Radio / PHY Layer (User's SDR backend)               │
└───────────────────────────────────────────────────────────────────┘
```

### Module Dependencies

```
BTS Stack Mode (all modules)
├── ProcedureOrchestrator
│   ├── ProcedureRunner
│   │   ├── Procedure (base class) + 10 concrete procedures
│   │   │   └── ProcedureStateMixin<Derived, State> (CRTP)
│   │   ├── ResponseBuilder + ResponseToken
│   │   └── TypedExternalData (ExternalData variant)
│   ├── SubscriberSession
│   │   ├── MSContext (identity, channel, flags)
│   │   ├── RR/MM/CC StateMachine (protocol FSM)
│   │   ├── TimerManager (T3101–T3395)
│   │   └── TransactionManager (request-response correlation)
│   └── ChannelPool / ShardedChannelPool
├── LAPDmEntity (L2 framing + state machine)
└── L3 Parser Mode (core, shared by both modes)
    ├── parseL3() / writeL3Bytes()
    ├── BitReader / BitWriter
    ├── ProtocolDispatcher + FlatHandler
    ├── Builder API (200+ message types across 12 PD domains)
    └── ByteSource / L3Framer / StreamProcessor
```

## 2. BTS Stack Architecture Diagram

```
┌──────────────────────────────────────────────────────────────────────┐
│                     BTS Application (Your Code)                      │
│  ┌─────────────┐  ┌──────────────┐  ┌────────────────────────────┐   │
│  │ AuC/HLR     │  │ VLR/BSC      │  │ SIP/Media Gateway (OpenBTS)│   │
│  │ Integration │  │ Decision API │  │ or RSL transport (osmo-bts)│   │
│  └──────┬──────┘  └──────┬───────┘  └──────────────┬─────────────┘   │
│         │                │                         │                 │
 │         └────────────────┼─────────────────────────┘                 │
 │                          │ feedExternalTyped()                       │
 ├──────────────────────────┼───────────────────────────────────────────┤
│              Procedure Framework                                     │
│  ┌───────────────────────▼──────────────────────────────────┐        │
│  │                   ProcedureRunner                        │        │
│  │  ┌──────────────┐ ┌──────────────┐ ┌──────────────────┐  │        │
│  │  │ Location     │ │ Call Setup   │ │ Channel          │  │        │
│  │  │ Update Proc  │ │ MO/MT Proc   │ │ Assignment Proc  │  │        │
│  │  │ (auto FSM)   │ │ (auto FSM)   │ │ (auto FSM)       │  │        │
│  │  └──────────────┘ └──────────────┘ └──────────────────┘  │        │
│  └───────────────────────────┬──────────────────────────────┘        │
│                              │ feed() / tick()                       │
├──────────────────────────────┼───────────────────────────────────────┤
│                      SubscriberRegistry                              │
│  ┌───────────────────────────▼──────────────────────────────────┐    │
 │  │  SubscriberSession [per-MS]                                  │    │
 │  │  ├─ MSContext (identity, channel, flags)                     │    │
 │  │  ├─ RR/MM/CC State Machines (response-aware)                 │    │
 │  │  ├─ TimerManager + TransactionManager                        │    │
 │  │  ├─ ResponseContext (response parameters, populated by       │    │
 │  │  │  the active procedure; consumed by ResponseBuilder)       │    │
 │  │  └─ ProcedureRunner (active procedures)                      │    │
│  └──────────────────────────────────────────────────────────────┘    │
├──────────────────────────────────────────────────────────────────────┤
│             Response Builder                                         │
│  Auto-generates L3 response messages from FSM transitions            │
│  Zero-heap-allocation via Arena pre-allocated buffers                │
├──────────────────────────────────────────────────────────────────────┤
│                    Core Parser / Serializer API                      │
│  parseL3() / writeL3Bytes() / Builder API / LAPDm / Dispatcher       │
├──────────────────────────────────────────────────────────────────────┤
│              Shared BTS Resources                                    │
│  ChannelPool (SDCCH, TCHF, TCHH allocation / VEA)                    │
│  ShardedChannelPool (thread-safe, million-MS scaling)                │
├──────────────────────────────────────────────────────────────────────┤
│              Abis/RSL Interface                                      │
│  RSLParser (BSC->BTS: extract L3 from RSL)                           │
│  RSLBuilder (BTS->BSC: wrap L3 in RSL, MEAS_RES, ACK/NACK)           │
├──────────────────────────────────────────────────────────────────────┤
│              Radio / PHY Layer                                       │
│  [User's SDR / hardware integration]                                 │
└──────────────────────────────────────────────────────────────────────┘
```

## 3. Data Flow Between Components

### Inbound Message Path (MS -> BTS)

Using the Procedure Framework (recommended):

```
Radio RX
  │
  ▼
LAPDm Frame (raw bytes from PHY)
  │
  ├─ LAPDmEntity.receiveFrame() ─► L3 payload via L3ReceiveFn callback
  │
  ├─ parseL3() ────────────────► ParsedMessage (stack variant)
  │
  ├─ SubscriberRegistry.findByLink(trx, ts, lapdmLink) ─► SubscriberSession*
  │
   ├─ session->procedures.feed(msg, session, responseSink)
   │       │
   │       ├─ Auto-create procedure if none active (ProcedureFactory);
   │       │    starting a new procedure resets session->response (ResponseContext)
   │       ├─ Route to active procedure: first one whose matches(msg) returns
   │       │    true (disambiguates procedures sharing a PD, e.g. CallRelease
   │       │    vs CallSetup_MO), else PD-based fallback
   │       ├─ Procedure processes message internally (FSM + timers) and records
   │       │    response parameters it learns into session->response
   │       └─ Returns ProcedureStepResult:
   │             ├─ Continue     -> await next message
   │             ├─ SendResponseWithToken -> result.responseToken set;
   │             │                  caller builds via ResponseBuilder::buildResponseFromToken()
   │             │                  (parameters read from session->response)
   │             │                  Arena buffer written, sent to MS
   │             ├─ WaitingExternal -> procedure blocks on external data
   │             ├─ Completed    -> slot freed automatically (no response pending)
   │             └─ Failed       -> slot freed automatically (no response pending)
  │
  └─ (optional) ProtocolDispatcher.dispatch() for custom handlers
```

### Outbound Message Path (BTS -> MS)

```
Application Decision or Procedure ResponseSink callback
  │
  ├─ ResponseBuilder::buildXxx(span, ...) ─► Arena buffer bytes
  │
  ├─ wrap in ParsedMessage variant
  │
  ├─ writeL3Bytes(msg) ───► raw L3 bytes (if not already serialized)
  │
  ├─ LAPDmEntity.sendUI() or sendData() ───► LAPDm frame to PHY
  │
  └─ Radio TX: send frame bytes
```

### Timer Event Path

```
Event Loop Tick (every 10-100ms)
  │
  ├─ Calculate delta since last tick
  │
  ├─ SubscriberRegistry.tickAllTimers(delta, expiredOut)
  │       │
  │       └─ For each session WITH >=1 running timer (active-timer index, O(active)):
  │             ├─ TimerManager::tick() -> expired timer IDs
  │             └─ TransactionManager::onTimerExpired() (called by the registry)
  │
  │  Returned events are TimerExpiry{session, id} pairs (order unspecified).
  │
  ├─ SubscriberRegistry.tickAllProcedures(delta)
  │       │
  │       └─ For each session WITH >=1 active procedure (active-procedure index, O(active)):
  │             └─ ProcedureRunner::tickAll(delta)
  │                   │
  │                   └─ Each procedure advances internal timers
  │                         Timer expiry -> Failed state -> slot auto-freed
  │
  └─ LAPDmEntity.tickT200() per active link
```

### External Data Flow (AuC/HLR/VLR -> BTS)

```
VLR accepts Location Update
   │
   ├─ orchestrator.feedExternalTyped(VLRDecision{accept: true, newTmsi: 0x12345678})
   │       │
   │       ├─ Procedure wakes from WaitingExternal
   │       ├─ Records newTmsi/rejectCause into session->response
   │       ├─ Returns SendResponseWithToken + ResponseToken::LocationUpdatingAccept
   │       │    (action stays SendResponseWithToken even though the procedure
   │       │    terminates in the same step; terminal state is in finalResult)
   │       ├─ Caller builds via ResponseBuilder::buildResponseFromToken()
   │       └─ Chain completed (finalResult.state == Completed), slot freed
   │
AuC provides RAND+SRES for Authentication
   │
   ├─ orchestrator.feedExternalTyped(AuthChallenge{rand: [16], expectedSres: [4]})
   │       │
   │       ├─ Copies RAND into session->response.rand (hasRand = true)
   │       ├─ Returns SendResponseWithToken + ResponseToken::AuthenticationRequest
   │       └─ Waits for MS response on next feed()
```

## 4. Integration with PHY/SDR Layer

libgsml3parser does not include a PHY layer. It interfaces with the radio through raw byte buffers at the LAPDm framing boundary.

### Required PHY Interface

The BTS application must provide:

```cpp
// Minimal PHY interface expected by BTS application
class PhyBackend {
public:
    virtual ~PhyBackend() = default;

    // Send a framed L3 message on a specific logical channel
    virtual void sendFrame(std::span<const uint8_t> lapdmFrame,
                           ChannelType type, uint8_t trx, uint8_t ts) = 0;

    // Broadcast on BCCH (system info, paging)
    virtual void broadcastBCCH(std::span<const uint8_t> lapdmFrame) = 0;

    // Register callback for incoming frames
    using FrameCallback = std::function<void(
        std::span<const uint8_t> frame, ChannelType type, uint8_t trx, uint8_t ts)>;
    virtual void setRxCallback(FrameCallback cb) = 0;

    // Timing: get current TDMA frame number for synchronization
    virtual uint32_t getCurrentFN() = 0;
};
```

### Integration Points

The library provides well-defined integration points for external systems that a BTS developer must connect. Each integration point uses strongly-typed data structures (no raw byte parsing).

| External System | Integration Point | API |
|----------------|-------------------|-----|
| **PHY / Radio (TX)** | `sendToRadio(std::span<const uint8_t>)` | After ResponseBuilder writes to Arena buffer, pass bytes to PHY transmit callback. For BCCH/PAGCH: `lapdm::wrapL3()` -> byte vector for broadcast. For AGCH/SDCCH: same pattern for dedicated channel. |
| **PHY / Radio (RX)** | `onRadioFrameReceived(std::span<const uint8_t>)` | PHY receive callback invokes LAPDmEntity -> parseL3() -> orchestrator.feed(). RACH: PHY delivers raw 1-byte Channel Request, parsed with `parseL3()`. |
| **AuC** | `orchestrator.feedExternalTyped(AuthChallenge{rand, expectedSres})` | Query AuC for RAND(16B) + SRES(4B), feed as typed struct to procedure. The library does not implement authentication algorithms (COMP128, MIL-STD-1889A). |
| **VLR / HLR** | `orchestrator.feedExternalTyped(VLRDecision{accept, newTmsi, rejectCause})` | VLR accept/reject decision with optional TMSI assignment. For IMSI detach: `feedExternalTyped(VLRDecision{accept: true})`. |
| **Ciphering (A5)** | After `CipheringModeComplete` received from MS | BTS enables A5/XOR at L2 level on affected logical channels. Library does not implement ciphering algorithms (A5/1, A5/2, A5/3). |
| **BSC (A-bis RSL, Inbound)** | `RSLParser::parse()` -> `extractL3()` -> `orchestrator.feed()` | BSC sends RLL DATA_REQ; extract L3 payload and feed to orchestrator. DCHAN CHAN_ACTIV: parse channel mode and activate via ChannelPool. CCHAN PAGING_CMD: extract identity and trigger PagingProcedure. |
| **BSC (A-bis RSL, Outbound)** | `ResponseBuilder` bytes -> `RSLBuilder::buildDataInd()` -> PHY | Wrap L3 response in RSL DATA_IND for BSC. DCHAN CHAN_ACTIV_ACK, DCHAN MEAS_RES, CCHAN CCCH_LOAD_IND built via RSLBuilder. |
| **SDR: GNU Radio** | Custom block | Call `unwrapL3()` -> `parseL3()` on RX path; `writeL3Bytes()` -> `wrapL3()` on TX path. |
| **SDR: srsRAN** | Replace L3 module | Swap `srsgsbts` L3 encode/decode with libgsml3parser equivalents. |
| **SDR: Limesuite / ADALM-Pluto** | PHY backend | Use as hardware transport; libgsml3parser handles all L2/L3 processing. |

### TCH Traffic Channel

When a call reaches the connected state (`ResponseToken::Connect`), the library is no longer involved in user data flow. The BTS application switches to its speech codec pipeline (AMR, FR, HR) for TCH timeslots. The library only manages subsequent call control signaling (Disconnect, Release).

## 5. Thread Model

### Event Loop with SubscriberRegistry (Recommended)

```
┌────────────────────────────────────────────────────────────┐
│                     Main Event Loop                        │
│                                                            │
│  ┌──────────────┐                                          │
│  │ Timer Wheel  │                                          │
│  │  (global)    │                                          │
│  └──────┬───────┘                                          │
│         │                                                  │
│         ▼                                                  │
│  ┌────────────────────────────────────────────────────┐    │
│  │           SubscriberRegistry (global)              │    │
│  │                                                    │    │
│  │  ┌────────────────┐  ┌────────────────┐            │    │
│  │  │   Session #1   │  │   Session #N   │            │    │
│  │  │ ┌────────────┐ │  │ ┌────────────┐ │            │    │
│  │  │ │   context  │ │  │ │   context  │ │            │    │
│  │  │ │   timers   │ │  │ │   timers   │ │            │    │
│  │  │ │     sm[]   │ │  │ │     sm[]   │ │            │    │
│  │  │ │ procedures │ │  │ │ procedures │ │            │    │
│  │  │ └────────────┘ │  │ └────────────┘ │            │    │
│  │  └────────────────┘  └────────────────┘            │    │
│  └────────────────────────────────────────────────────┘    │
│         ▲                                                  │
│         └──────────────────────────────────────────────────┘
│                                │
│                   tickAllTimers() / procedures.tickAll()
└─────────────────────────────────────────────────────────────┘
```

Each `SubscriberSession` is accessed from a single thread (the event loop). The `SubscriberRegistry` provides O(1) lookup by TMSI, IMSI, or LAPDm link, and `tickAllTimers()` ticks only sessions with running timers (O(active) via an active-timer index, not O(all)). For high-concurrency scenarios, use `ShardedSubscriberRegistry<N>` with per-shard mutexes.

### Thread Safety Matrix

| Component | Thread-Safe? | Access Pattern |
|-----------|-------------|----------------|
| `MSContext` | **No** | One thread per MS |
| `TimerManager` | **No** | One thread per MS |
| `TransactionManager` | **No** | One thread per MS |
| `ProtocolStateMachine` (RR/MM/CC) | **No** | One thread per MS |
| `ProcedureRunner` | **No** | One instance per session, one thread |
| `Procedure` instances | **No** | Owned by ProcedureRunner, one thread |
| `SubscriberRegistry` | **No** | Single event loop thread |
| `ShardedSubscriberRegistry<N>` | **Yes** | Per-shard shared_mutex |
| `ChannelPool` | **No** | External synchronization required |
| `ShardedChannelPool<N>` | **Yes** | Per-shard shared_mutex |
| `parseL3()` / `writeL3Bytes()` | **Yes** | Stateless functions |
| `Builder API` | **Yes** | Each builder independent |
| `ResponseBuilder` | **Yes** | Stateless static methods |
| `RSLParser` / `RSLBuilder` | **Yes** | Stateless static methods |
| `ProtocolDispatcher` | **No** | One instance per MS, one thread |

### Synchronization Strategy

```cpp
// For single-thread event loop: no synchronization needed for SubscriberRegistry
SubscriberRegistry registry;

// For multi-thread scenarios, use ShardedSubscriberRegistry
ShardedSubscriberRegistry<16> shardedRegistry;

// Thread-safe channel allocation from any MS thread:
ShardedChannelPool<16> btsChannels;
auto ch = btsChannels.allocate(ChannelType::SDCCHType);
```

## 6. Performance Considerations

### Memory Footprint Per MS

Measured sizes (MSVC 2026, Release, x64):

| Component | Size | Notes |
|-----------|------|-------|
| `MSContext` | 92 bytes | All inline storage. |
| `TimerManager` | 1,080 bytes | 32 × L3Timer + init flags + active index |
| `TransactionManager` | 536 bytes | 16 × Transaction (24B) + TI index + metadata |
| `RRStateMachine` | 16 bytes | Virtual table pointer + state int |
| `MMStateMachine` | 16 bytes | Virtual table pointer + state int |
| `CCStateMachine` | 16 bytes | Virtual table pointer + state int |
| `ProcedureRunner` | 152 bytes | 8 × ProcedureSlot (unique_ptr + bool) + active-change observer |
| `ResponseContext` | 126 bytes | Response parameters (fixed arrays, ≤ 160 budget) |
| `ProcedureOrchestrator` | ~100 bytes | Active chain state + phase timer |
| **Total per MS** | **2,056 bytes** (`sizeof(SubscriberSession)`) | Enforced `< 4096` via `static_assert`; plus `ParsedMessage` (416 bytes) on stack during processing |

At 10,000 concurrent MS sessions: ~20 MB for sessions (fits comfortably in DRAM; hot per-session data stays cache-resident under normal load).

### Cache Behavior

- **MSContext fields ordered by access frequency**: identity and channel type are accessed on every message; LAI and classmark only during setup
- **TimerManager tick()** iterates a contiguous `std::array<L3Timer, 32>` - single cache line per ~4 timers
- **TransactionManager match()** for CC/SS: direct array index into `mTiIndex[8]` - no pointer chasing
- **FSM dispatch**: `switch(PD) + switch(MTI)` compiled to jump table - O(1), no branch misprediction on hot path
- **ProcedureRunner slots**: Fixed `std::array<ProcedureSlot, 8>` - sequential scan for routing

### Allocation-Free Hot Paths

The following operations perform zero heap allocations:

| Operation | Component | Guarantee |
|-----------|-----------|-----------|
| `parseL3()` | Parser | ParsedMessage on stack, BitReader over span |
| `TimerManager::tick(callback)` | Timer | Fixed array iteration, callback invocation |
| `TimerManager::tick(span)` | Timer | Fixed array iteration, span write |
| `TransactionManager::match()` | Transaction | Array index + bounded scan |
| `ChannelPool::allocate()` | ChannelPool | Vector pop_back (no reallocation for single pop) |
| `FSM::processMessage()` | StateMachine | Switch dispatch, returns SMResult by value |
| `ProcedureRunner::feed()` | Runner | Fixed array scan, delegates to Procedure |
| `ResponseBuilder::buildXxx(span)` | ResponseBuilder | Writes to caller buffer, zero heap |
| `RSLParser::parse()` | RSLParser | Fixed IE array, span pointers into original data |
| `LAPDmEntity` send path (sendUI/sendData/…) | LAPDm | TX buffer reused after first send (audit C3); L1 callback must transmit synchronously |

### Dispatch Complexity

| Operation | Complexity | Mechanism |
|-----------|-----------|-----------|
| `ProtocolDispatcher::dispatch()` | O(1) | `std::array[16][136]` handler table |
| `TransactionManager::match()` CC/SS | O(1) | `mTiIndex[ti]` direct array access |
| `TransactionManager::match()` other | O(K), K ≤ 16 | Bounded linear scan of `mTransactions` |
| `ChannelPool::allocate()` | O(1) | Per-type free-list `pop_back()` |
| `FSM::handle_message_impl()` | O(1) | `switch(PD) + switch(MTI)` jump table |
| `TimerManager::tick()` | O(32) = O(1) | Fixed array of 32 timers |
| `ProcedureRunner::feed()` | O(8) = O(1) | Fixed array of procedure slots |
| `SubscriberRegistry::findByTMSI()` | O(1) | Hash map lookup |
| `ShardedSubscriberRegistry::findByTMSI()` | O(1) | Hash + per-shard lock |
| `SubscriberRegistry::tickAllProcedures()` | O(active) | Active-procedure index (sessions with >=1 active procedure) |
| `ShardedChannelPool::allocate()` | O(N) worst, typically O(1) | Round-robin shard start + fallback (channels hash-distributed) |

## 7. Abis/RSL Integration

The A-bis RSL interface allows libgsml3parser-based BTS to communicate with an external BSC over the A-bis interface (TS 48.058).

### RSL Message Flow

```
BSC ──[RLL DATA_REQ]──► BTS: RSLParser::parse() -> extract L3 -> ProcedureRunner::feed()
BTS ──[RLL DATA_IND]──► BSC: ResponseBuilder bytes -> RSLBuilder::buildDataInd() -> send

BSC ──[DCHAN CHAN_ACTIV]──► BTS: RSLParser::parse() -> getChannelMode() -> activate channel
BTS ──[DCHAN CHAN_ACTIV_ACK]──► BSC: RSLBuilder::buildChanActivAck() -> send

BTS ──[DCHAN MEAS_RES]──► BSC: RSLBuilder::buildMeasRes(rxlev, rxqual) -> send
BTS ──[CCHAN CCCH_LOAD_IND]──► BSC: RSLBuilder::buildCCCHLoadInd() -> send
```

### Integration Points

| Direction | RSL Message | libgsml3parser Component |
|-----------|------------|-------------------------|
| BSC->BTS | RLL DATA_REQ | `RSLParser::parse()` -> `extractL3()` -> `parseL3()` -> `ProcedureRunner::feed()` |
| BTS->BSC | RLL DATA_IND | `ResponseBuilder` bytes -> `RSLBuilder::buildDataInd()` -> PHY |
| BSC->BTS | DCHAN CHAN_ACTIV | `RSLParser::parse()` -> `getChannelMode()` -> `ChannelPool` activate |
| BTS->BSC | DCHAN CHAN_ACTIV_ACK | `RSLBuilder::buildChanActivAck()` -> PHY |
| BTS->BSC | DCHAN MEAS_RES | `RSLBuilder::buildMeasRes()` -> PHY |
| BSC->BTS | CCHAN PAGING_CMD | `RSLParser::parse()` -> `extractL3()` -> paging procedure |

## 8. Scaling Guidelines

### Managing Millions of MS Contexts

The event loop model scales to millions of sessions with `ShardedSubscriberRegistry`:

```cpp
// Sharded registry for multi-threaded access
ShardedSubscriberRegistry<16> registry;

// Size the flat indexes for the expected subscriber population
// (one-time, cold path) — avoids incremental rehashing at scale.
registry.reserve(1'000'000);

// Create session (hash-based shard selection, per-shard lock)
auto* session = registry.createByTMSI(0x12345678);

// Find session (shared lock, no contention with other shards)
auto* found = registry.findByTMSI(0x12345678);

// Tick all timers across all shards — O(active): only sessions with >=1
// running timer are ticked (active-timer index), so a large number of idle
// sessions does not slow the tick.
std::array<TimerExpiry, 4096> expired;
size_t n = registry.tickAllTimers(std::chrono::milliseconds(100), {expired.data(), expired.size()});
// Each event: {session, timerId} — route the expiry to the owner.

// Tick active procedures — O(active): only sessions with >=1 active
// procedure are visited (active-procedure index).
size_t failed = registry.tickAllProcedures(std::chrono::milliseconds(100));

// Remove session (detach) — O(1) via session->assignedTmsi reverse index,
// so high session churn at scale does not degrade to O(N) scans.
registry.remove(session);
```

### Event Loop Design

```cpp
class BtsEventLoop {
public:
    void run() {
        auto lastTick = std::chrono::steady_clock::now();
        Arena arena(65536);

        while (running) {
            auto now = std::chrono::steady_clock::now();
            auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTick);
            lastTick = now;

            // 1. Process incoming frames from radio or A-bis RSL
            processRadioFrames();
            processRslMessages();

            // 2. Tick all session timers and procedures — O(active) for both:
            //    only sessions with running timers / active procedures are visited.
            std::array<TimerExpiry, 4096> expired;
            size_t nExpired = registry.tickAllTimers(delta, {expired.data(), expired.size()});
            // (handle expired[i].session / expired[i].id protocol timeouts)
            registry.tickAllProcedures(delta);

            // 3. Periodic broadcasts (System Information)
            if (siCounter++ % SI_INTERVAL == 0) {
                broadcastSystemInfo();
            }

            // 4. Reset arena periodically to reclaim memory
            if (arena.used() > 32768) arena.reset();

            // 5. Yield to allow other threads
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
};
```

### Memory Budget Planning

Per-MS stack footprint is ~2 KB (`sizeof(SubscriberSession)` = 2056 bytes, static_assert < 4096); `sizeof(ParsedMessage) = 416` bytes.

| Scale | MS Sessions | Stack Module Memory | ParsedMessage (stack, transient) |
|-------|------------|-------------------|-------------------------------|
| Small cell | 100 | ~200 KB | ~42 KB peak |
| Macro cell | 10,000 | ~20 MB | ~4.2 MB peak |
| Large deployment | 1,000,000 | ~2 GB | ~416 MB peak (transient) |

For large deployments, ParsedMessage is only on-stack during message processing (microseconds), so peak concurrent usage is much lower than the theoretical maximum.

### Channel Pool Scaling

A typical macro cell BTS might have:
- 4 SDCCH channels (control channel)
- 24 TCHF channels (full-rate traffic, across 3 TRX × 8 timeslots)
- 3 TCHH channels (half-rate traffic)

The `ShardedChannelPool<16>` handles this with negligible memory overhead and thread-safe allocation. `allocate()` starts probing from a round-robin shard (atomic counter) to spread lock contention across shards, then falls back to the remaining shards because channels are hash-distributed at `addChannel()` time; worst case is O(N) shards, typically the first or second shard.

### Transaction Limits

Each MS can have up to 16 concurrent pending transactions (`TransactionManager::MAX_TRANSACTIONS = 16`). For typical BTS workloads, < 4 concurrent transactions per MS is expected. The `cleanup()` method should be called periodically or when `totalCount()` approaches the limit.

### Known Limitations

- **Procedure tick:** `tickAllProcedures()` is O(active) via an active-procedure index (same pattern as the active-timer index). The old documented pattern (forEach over all sessions) must not be used at scale.
- **Registry storage:** `SubscriberRegistry`/`ShardedSubscriberRegistry`
  use a flat open-addressing hash table (`stack/flat_map.h`) for the
  TMSI and LAPDm-link indexes: inline key/value entries, no per-node
  heap allocation, no pointer chasing. Call `reserve()` at startup when
  the subscriber scale is known. The IMSI index stays a
  `std::unordered_map` (owned std::string keys, cold path).
- **L3Framer header-based mode:** for variable-length messages (SI, SMS, Setup with IEs, ...) the framer uses a boundary heuristic that scans for the next plausible L3 header. Fixed-length messages (including BCC/GCC/LS header-only forms) are framed exactly. For deterministic framing of variable-length messages use the L2-length mode (`FrameConfig::useL2Length = true`), which is what production LAPDm/A-bis paths provide.

## 9. Deployment Checklist

- [ ] Build with C++20, Release mode (`-O2` or `/O2`)
- [ ] Verify `sizeof(MSContext) <= 256` via `static_assert` (measured: 92 bytes)
- [ ] Verify `sizeof(ProcedureStepResult) <= 32` via `static_assert`
- [ ] Verify `sizeof(SubscriberSession) < 4096` via `static_assert` (measured: 2056 bytes)
- [ ] Verify `sizeof(ResponseContext) <= 160` via `static_assert` (measured: 126 bytes)
- [ ] Configure `ChannelPool` with available channels at startup
- [ ] Initialize `SubscriberRegistry` (or `ShardedSubscriberRegistry<N>` for multi-threaded)
- [ ] Choose usage mode: L3 Parser Mode (parse/build only) or BTS Stack Mode (full procedures)
- [ ] For BTS Stack Mode: create `ProcedureOrchestrator` per subscriber session
- [ ] Implement `feedExternalTyped()` handlers: query AuC for `AuthChallenge`, VLR for `VLRDecision`
- [ ] Handle `ResponseToken` from `ProcedureStepResult`: call `ResponseBuilder::buildResponseFromToken()` into Arena buffer
- [ ] Integrate `orchestrator.tickAll()` or `runner.tickAll()` into event loop (10-100ms interval)
- [ ] If using A-bis: set up `RSLParser` -> `parseL3()` -> `orchestrator.feed()` pipeline
- [ ] If using A-bis: set up Arena buffer -> `RSLBuilder::buildDataInd()` -> PHY outbound pipeline
- [ ] Provide external synchronization for shared `ChannelPool` (or use `ShardedChannelPool`)
- [ ] Set up PHY backend with `sendToRadio(span)` and `onRadioFrameReceived(span)` callbacks
- [ ] Configure System Information broadcast schedule
- [ ] Add logging for procedure state transitions, ResponseToken values, and timer expirations

## 10. References

| Document | Topic |
|----------|-------|
| [doc/API.md](API.md) | Full API reference (62 sections) |
| [doc/bts_integration.md](bts_integration.md) | Step-by-step integration guide with ProcedureRunner |
| [README.md](../README.md) | Library overview and quick start |
| 3GPP TS 24.008 | Mobile radio interface L3 specification |
| 3GPP TS 44.018 | Group call and broadcast call control |
| GSM 04.06 / 3GPP TS 24.022 | LAPDm framing for Um interface |
| GSM 04.08 | Layer 3 specification (legacy reference) |
| 3GPP TS 48.058 | A-bis RSL specification |
