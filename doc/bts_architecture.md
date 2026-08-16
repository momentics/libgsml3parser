# BTS Architecture on libgsml3parser

This document describes the recommended architecture for building a software Base Transceiver Station (BTS) using libgsml3parser as the L3 protocol stack. It covers component relationships, data flow, threading model, performance characteristics, and scaling guidelines with the Procedure Framework.

## 1. Architecture Diagram

```
┌──────────────────────────────────────────────────────────────────────┐
│                     BTS Application (Your Code)                      │
│  ┌─────────────┐  ┌──────────────┐  ┌────────────────────────────┐   │
│  │ AuC/HLR     │  │ VLR/BSC      │  │ SIP/Media Gateway (OpenBTS)│   │
│  │ Integration │  │ Decision API │  │ or RSL transport (osmo-bts)│   │
│  └──────┬──────┘  └──────┬───────┘  └──────────────┬─────────────┘   │
│         │                │                         │                 │
│         └────────────────┼─────────────────────────┘                 │
│                          │ feedExternal()                            │
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

## 2. Data Flow Between Components

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
  │       ├─ Auto-create procedure if none active (ProcedureFactory)
  │       ├─ Route to active procedure by PD
  │       ├─ Procedure processes message internally (FSM + timers)
  │       └─ Returns ProcedureStepResult:
  │             ├─ Continue     -> await next message
  │             ├─ SendResponse -> responseSink callback fires
  │             │                  ResponseBuilder builds L3 response bytes
  │             │                  Arena buffer written, sent to MS
  │             ├─ WaitingExternal -> procedure blocks on external data
  │             ├─ Completed    -> slot freed automatically
  │             └─ Failed       -> slot freed automatically
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
  │       └─ For each session:
  │             ├─ TimerManager::tick() -> expired timer IDs
  │             └─ TransactionManager::onTimerExpired()
  │
  ├─ For each session:
  │     └─ session->procedures.tickAll(delta)
  │             │
  │             └─ Each procedure advances internal timers
  │                   Timer expiry -> Failed state -> slot auto-freed
  │
  └─ LAPDmEntity.tickT200() per active link
```

### External Data Flow (AuC/HLR/VLR -> BTS)

```
VLR accepts Location Update
  │
  ├─ runner.feedExternal(ProcedureType::LocationUpdate, acceptData)
  │       │
  │       ├─ Procedure wakes from WaitingExternal
  │       ├─ Sends LocationUpdatingAccept via ResponseSink
  │       └─ Returns Completed -> slot freed
  │
AuC provides RAND+SRES for Authentication
  │
  ├─ runner.feedExternal(ProcedureType::Authentication, randSresData)
  │       │
  │       ├─ Procedure sends AuthenticationRequest via ResponseSink
  │       └─ Waits for MS response on next feed()
```

## 3. Integration with PHY/SDR Layer

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

| Layer | libgsml3parser Output | PHY Input |
|-------|----------------------|-----------|
| BCCH/PAGCH | `lapdm::wrapL3()` -> byte vector | Raw bytes for broadcast |
| AGCH/SDCCH | `lapdm::wrapL3()` -> byte vector | Raw bytes for dedicated channel |
| TCH | Application-level speech/data | Encoded speech frames (AMR/G.723) |
| RACH | PHY delivers raw 1-byte Channel Request | Parse with `parseL3()` |

### SDR Frameworks

| Framework | Integration Approach |
|-----------|---------------------|
| **GNU Radio** | Custom block that calls `unwrapL3()` -> `parseL3()` on RX, `writeL3Bytes()` -> `wrapL3()` on TX |
| **srsRAN** | Replace L3 encode/decode in `srsgsbts` with libgsml3parser equivalents |
| **Limesuite / ADALM-Pluto** | Use as PHY backend; libgsml3parser handles all L2/L3 processing |

## 4. Thread Model

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

Each `SubscriberSession` is accessed from a single thread (the event loop). The `SubscriberRegistry` provides O(1) lookup by TMSI, IMSI, or LAPDm link. For high-concurrency scenarios, use `ShardedSubscriberRegistry<N>` with per-shard mutexes.

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

## 5. Performance Considerations

### Memory Footprint Per MS

| Component | Size | Notes |
|-----------|------|-------|
| `MSContext` | ≤ 256 bytes | Enforced by `static_assert`. All inline storage. |
| `TimerManager` | ~1,248 bytes | 32 × L3Timer (~36B) + 32B init flags |
| `TransactionManager` | ~768 bytes | 16 × Transaction (≤48B) + metadata |
| `RRStateMachine` | ~16 bytes | Virtual table pointer + state int |
| `MMStateMachine` | ~16 bytes | Virtual table pointer + state int |
| `CCStateMachine` | ~16 bytes | Virtual table pointer + state int |
| `ProcedureRunner` | ~128 bytes | 8 × ProcedureSlot (unique_ptr + bool) |
| **Total per MS** | **~2,400 bytes** | Plus ParsedMessage (~8 KB) on stack during processing |

At 10,000 concurrent MS sessions: ~24 MB for stack modules (fits in L3 cache range).

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

### Dispatch Complexity

| Operation | Complexity | Mechanism |
|-----------|-----------|-----------|
| `ProtocolDispatcher::dispatch()` | O(1) | `std::array[16][256]` handler table |
| `TransactionManager::match()` CC/SS | O(1) | `mTiIndex[ti]` direct array access |
| `TransactionManager::match()` other | O(K), K ≤ 16 | Bounded linear scan of `mTransactions` |
| `ChannelPool::allocate()` | O(1) | Per-type free-list `pop_back()` |
| `FSM::handle_message_impl()` | O(1) | `switch(PD) + switch(MTI)` jump table |
| `TimerManager::tick()` | O(32) = O(1) | Fixed array of 32 timers |
| `ProcedureRunner::feed()` | O(8) = O(1) | Fixed array of procedure slots |
| `SubscriberRegistry::findByTMSI()` | O(1) | Hash map lookup |
| `ShardedSubscriberRegistry::findByTMSI()` | O(1) | Hash + per-shard lock |

## 6. Abis/RSL Integration

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

## 7. Scaling Guidelines

### Managing Millions of MS Contexts

The event loop model scales to millions of sessions with `ShardedSubscriberRegistry`:

```cpp
// Sharded registry for multi-threaded access
ShardedSubscriberRegistry<16> registry;

// Create session (hash-based shard selection, per-shard lock)
auto* session = registry.createByTMSI(0x12345678);

// Find session (shared lock, no contention with other shards)
auto* found = registry.findByTMSI(0x12345678);

// Tick all timers across all shards
std::array<L3TimerId, 4096> expired;
size_t n = registry.tickAllTimers(std::chrono::milliseconds(100), expired);
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

            // 2. Tick all session timers and procedures
            registry.forEach([&delta](SubscriberSession* sess) {
                sess->procedures.tickAll(delta);
            });

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

| Scale | MS Sessions | Stack Module Memory | ParsedMessage (stack, transient) |
|-------|------------|-------------------|-------------------------------|
| Small cell | 100 | ~240 KB | 800 KB peak |
| Macro cell | 10,000 | ~24 MB | 80 MB peak |
| Large deployment | 1,000,000 | ~2.4 GB | 8 GB peak (transient) |

For large deployments, ParsedMessage is only on-stack during message processing (microseconds), so peak concurrent usage is much lower than the theoretical maximum.

### Channel Pool Scaling

A typical macro cell BTS might have:
- 4 SDCCH channels (control channel)
- 24 TCHF channels (full-rate traffic, across 3 TRX × 8 timeslots)
- 3 TCHH channels (half-rate traffic)

The `ShardedChannelPool<16>` handles this with negligible memory overhead and thread-safe allocation.

### Transaction Limits

Each MS can have up to 16 concurrent pending transactions (`TransactionManager::MAX_TRANSACTIONS = 16`). For typical BTS workloads, < 4 concurrent transactions per MS is expected. The `cleanup()` method should be called periodically or when `totalCount()` approaches the limit.

## 8. Deployment Checklist

- [ ] Build with C++20, Release mode (`-O2` or `/O2`)
- [ ] Verify `sizeof(MSContext) <= 256` via `static_assert`
- [ ] Verify `sizeof(ProcedureStepResult) <= 32` via `static_assert`
- [ ] Configure `ChannelPool` with available channels at startup
- [ ] Initialize `SubscriberRegistry` (or `ShardedSubscriberRegistry<N>` for multi-threaded)
- [ ] Set up `ResponseSink` callback to build responses via `ResponseBuilder` into Arena buffer
- [ ] Integrate `ProcedureRunner::tickAll()` into event loop (10-100ms interval)
- [ ] Implement `feedExternal()` handlers for AuC/HLR/VLR decisions
- [ ] If using A-bis: set up `RSLParser` -> `parseL3()` -> `ProcedureRunner::feed()` pipeline
- [ ] If using A-bis: set up `ResponseBuilder` -> `RSLBuilder` -> PHY outbound pipeline
- [ ] Provide external synchronization for shared `ChannelPool` (or use `ShardedChannelPool`)
- [ ] Set up PHY backend (SDR, GNU Radio block, etc.)
- [ ] Configure System Information broadcast schedule
- [ ] Add logging for procedure state transitions and timer expirations

## 9. References

| Document | Topic |
|----------|-------|
| [doc/API.md](API.md) | Full API reference (57 sections) |
| [doc/bts_integration.md](bts_integration.md) | Step-by-step integration guide with ProcedureRunner |
| [README.md](../README.md) | Library overview and quick start |
| 3GPP TS 24.008 | Mobile radio interface L3 specification |
| 3GPP TS 44.018 | Group call and broadcast call control |
| GSM 04.06 / 3GPP TS 24.022 | LAPDm framing for Um interface |
| GSM 04.08 | Layer 3 specification (legacy reference) |
| 3GPP TS 48.058 | A-bis RSL specification |
