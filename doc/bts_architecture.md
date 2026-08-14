# BTS Architecture on libgsml3parser

This document describes the recommended architecture for building a software Base Transceiver Station (BTS) using libgsml3parser as the L3 protocol stack. It covers component relationships, data flow, threading model, performance characteristics, and scaling guidelines.

## 1. Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────────────┐
│                          BTS Application Layer                          │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────────────────┐   │
│  │ Roaming  │  │ Auth     │  │ Handover │  │ Call Control Policy  │   │
│  │ Decisions│  │ (AuC)    │  │ Decisions│  │ (routing, CLIP, etc.)│   │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘  └──────────┬───────────┘   │
│       │              │              │                   │               │
│       └──────────────┴──────────────┴───────────────────┘               │
│                                 │                                       │
├─────────────────────────────────┼───────────────────────────────────────┤
│                    libgsml3parser Stack Modules                         │
│  ┌──────────────────────────────▼──────────────────────────────┐       │
│  │                      MsSession (per-MS)                      │       │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌────────────┐  │       │
│  │  │MSContext │  │TimerMgr  │  │Txn Mgr   │  │ FSMs:      │  │       │
│  │  │(≤256B)   │  │(≤1.2KB)  │  │(≤768B)   │  │ RR/MM/CC   │  │       │
│  │  └──────────┘  └──────────┘  └──────────┘  └────────────┘  │       │
│  └───────────────────────────┬─────────────────────────────────┘       │
│                              │                                         │
│  ┌───────────────────────────▼─────────────────────────────────┐       │
│  │                   ProtocolDispatcher                        │       │
│  │              (per-MS, O(1) PD+MTI routing)                  │       │
│  └───────────────────────────┬─────────────────────────────────┘       │
│                              │                                         │
├──────────────────────────────┼─────────────────────────────────────────┤
│                    libgsml3parser Core API                             │
│  ┌───────────────────────────▼─────────────────────────────────┐       │
│  │  parseL3() ←→ ParsedMessage (stack variant, < 8 KB)        │       │
│  │  writeL3Bytes() → raw bytes                                 │       │
│  │  Builder API → construct L3 messages                        │       │
│  │  lapdm::wrapL3() / unwrapL3() → LAPDm framing              │       │
│  └───────────────────────────┬─────────────────────────────────┘       │
│                              │                                         │
├──────────────────────────────┼─────────────────────────────────────────┤
│                    Shared BTS Resources                                 │
│  ┌───────────────────────────▼─────────────────────────────────┐       │
│  │                   ChannelPool (global)                       │       │
│  │         SDCCH, TCHF, TCHH allocation / VEA                  │       │
│  └───────────────────────────┬─────────────────────────────────┘       │
│                              │                                         │
├──────────────────────────────┼─────────────────────────────────────────┤
│                      Radio / PHY Layer                                  │
│  ┌───────────────────────────▼─────────────────────────────────┐       │
│  │  Um Interface: BCCH, CCCH, DCCH, TCH timeslots              │       │
│  │  SDR Backend: GNU Radio, LMS7002, URHFD, etc.               │       │
│  └─────────────────────────────────────────────────────────────┘       │
└─────────────────────────────────────────────────────────────────────────┘
```

## 2. Data Flow Between Components

### Inbound Message Path (MS → BTS)

```
Radio RX
  │
  ▼
LAPDm Frame (raw bytes from PHY)
  │
  ├─ lapdm::unwrapL3() ───► L3 payload bytes
  │
  ├─ parseL3Header() ─────► L3Header { pd, mti, ti }
  │
  ├─ parseL3() ───────────► ParsedMessage (stack variant)
  │
  ├─ TimerManager::tick() ─► advance timers, fire expired callbacks
  │
  ├─ TransactionManager::match(header, msg) ─► correlated Transaction*
  │       │
  │       └─ if matched: tx->complete(), stop associated timer
  │
  ├─ ProtocolStateMachine::processMessage(msg) ─► SMResult { action, nextState }
  │       │
  │       └─ if transition: update MSContext, build response
  │
  └─ ProtocolDispatcher::dispatch(msg) ─► application handler
          │
          └─ handler builds response → send outbound
```

### Outbound Message Path (BTS → MS)

```
Application Decision (e.g., "send Paging Request")
  │
  ├─ Builder API: MessageType::builder().field(v).build()
  │
  ├─ Wrap in ParsedMessage variant
  │
  ├─ writeL3Bytes(msg) ───► raw L3 bytes
  │
  ├─ lapdm::wrapL3(bytes, SAPI, CR) ───► LAPDm frame
  │
  ├─ (if expecting response:)
  │     ├─ TransactionManager::create(pd, mti, ti, timerId)
  │     └─ TimerManager::start(timerId)
  │
  └─ Radio TX: send frame bytes to PHY layer
```

### Timer Event Path

```
Event Loop Tick (every 10-100ms)
  │
  ├─ Calculate delta since last tick
  │
  ├─ For each MsSession:
  │     ├─ TimerManager::tick(delta, callback)
  │     │       │
  │     │       └─ for each expired timer:
  │     │             ├─ callback(L3TimerId)
  │     │             ├─ TransactionManager::onTimerExpired(id)
  │     │             ├─ FSM::processTimer(id) → SMResult
  │     │             └─ application: retransmit or abort
  │     │
  │     └─ TransactionManager::cleanup() (periodic)
  │
  └─ ChannelPool diagnostics (freeCount, allocatedCount)
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
| BCCH/PAGCH | `lapdm::wrapL3()` → byte vector | Raw bytes for broadcast |
| AGCH/SDCCH | `lapdm::wrapL3()` → byte vector | Raw bytes for dedicated channel |
| TCH | Application-level speech/data | Encoded speech frames (AMR/G.723) |
| RACH | PHY delivers raw 1-byte Channel Request | Parse with `parseL3()` |

### SDR Frameworks

| Framework | Integration Approach |
|-----------|---------------------|
| **GNU Radio** | Custom block that calls `unwrapL3()` → `parseL3()` on RX, `writeL3Bytes()` → `wrapL3()` on TX |
| **srsRAN** | Replace L3 encode/decode in `srsgsbts` with libgsml3parser equivalents |
| **Limesuite / ADALM-Pluto** | Use as PHY backend; libgsml3parser handles all L2/L3 processing |

## 4. Thread Model

### One-Thread-Per-MS Model (Recommended)

```
┌──────────────────────────────────────────────────────────────┐
│                      Main Event Loop                         │
│                                                              │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐  │
│  │ Thread Pool │  │ Timer Wheel │  │  ChannelPool (BTS)  │  │
│  │ (MS workers)│  │ (global)    │  │  (external sync)    │  │
│  └──────┬──────┘  └──────┬──────┘  └─────────────────────┘  │
│         │                │                                    │
│   ┌─────▼─────┐    ┌────▼────┐                               │
│   │ MS #1     │    │ MS #N   │                               │
│   │ ┌───────┐ │    │ ┌───────┐ │                             │
│   │ │ctx    │ │    │ │ctx    │ │  Each MS session owns:      │
│   │ │timers │ │    │ │timers │ │  - MSContext                │
│   │ │txns   │ │    │ │txns   │ │  - TimerManager             │
│   │ │fsm[]  │ │    │ │fsm[]  │ │  - TransactionManager       │
│   │ │disp   │ │    │ │disp   │ │  - ProtocolStateMachine ×3  │
│   │ └───────┘ │    │ └───────┘ │  - ProtocolDispatcher       │
│   └───────────┘    └───────────┘                              │
└──────────────────────────────────────────────────────────────┘
```

### Thread Safety Matrix

| Component | Thread-Safe? | Access Pattern |
|-----------|-------------|----------------|
| `MSContext` | **No** | One thread per MS |
| `TimerManager` | **No** | One thread per MS |
| `TransactionManager` | **No** | One thread per MS |
| `ProtocolStateMachine` (RR/MM/CC) | **No** | One thread per MS |
| `ProtocolDispatcher` | **No** | One instance per MS, one thread |
| `ChannelPool` | **No** | External synchronization required. Single global pool accessed by all MS threads. |
| `parseL3()` / `writeL3Bytes()` | **Yes** | Stateless functions, safe for concurrent use |
| `Builder API` | **Yes** | Each builder is independent, no shared state |
| `ParsedMessage` | **Yes** (read) | Immutable after construction; safe to share read-only |

### Synchronization Strategy

```cpp
// ChannelPool requires external synchronization
std::mutex channelPoolMutex;

// Safe allocation from any MS thread:
std::optional<ChannelDescriptor> safeAllocate(ChannelType type) {
    std::lock_guard<std::mutex> lock(channelPoolMutex);
    return btsChannels.allocate(type);
}

// Safe release:
bool safeRelease(const ChannelDescriptor& desc) {
    std::lock_guard<std::mutex> lock(channelPoolMutex);
    return btsChannels.release(desc);
}
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
| `ProtocolDispatcher` | ~512 bytes | Handler map + TI handlers |
| **Total per MS** | **~2,770 bytes** | Plus ParsedMessage (~8 KB) on stack during processing |

At 10,000 concurrent MS sessions: ~27.7 MB for stack modules (fits in L3 cache range).

### Cache Behavior

- **MSContext fields ordered by access frequency**: identity and channel type are accessed on every message; LAI and classmark only during setup
- **TimerManager tick()** iterates a contiguous `std::array<L3Timer, 32>` - single cache line per ~4 timers
- **TransactionManager match()** for CC/SS: direct array index into `mTiIndex[8]` - no pointer chasing
- **FSM dispatch**: `switch(PD) + switch(MTI)` compiled to jump table - O(1), no branch misprediction on hot path

### Allocation-Free Hot Paths

The following operations perform zero heap allocations:

| Operation | Component | Guarantee |
|-----------|-----------|-----------|
| `parseL3()` | Parser | ParsedMessage on stack, BitReader over span |
| `writeL3Bytes()` | Serializer | Returns vector (one allocation), not on per-message hot path if pre-allocated |
| `TimerManager::tick(callback)` | Timer | Fixed array iteration, callback invocation |
| `TimerManager::tick(span)` | Timer | Fixed array iteration, span write |
| `TransactionManager::match()` | Transaction | Array index + bounded scan |
| `ChannelPool::allocate()` | ChannelPool | Vector pop_back (no reallocation for single pop) |
| `FSM::processMessage()` | StateMachine | Switch dispatch, returns SMResult by value |
| `MSContext` getters/setters | Context | Inline field access |

### Dispatch Complexity

| Operation | Complexity | Mechanism |
|-----------|-----------|-----------|
| `ProtocolDispatcher::dispatch()` | O(1) | `unordered_map<HandlerKey, Handler>` |
| `TransactionManager::match()` CC/SS | O(1) | `mTiIndex[ti]` direct array access |
| `TransactionManager::match()` other | O(K), K ≤ 16 | Bounded linear scan of `mTransactions` |
| `ChannelPool::allocate()` | O(1) | Per-type free-list `pop_back()` |
| `FSM::handle_message_impl()` | O(1) | `switch(PD) + switch(MTI)` jump table |
| `TimerManager::tick()` | O(32) = O(1) | Fixed array of 32 timers |

## 6. Scaling Guidelines

### Managing Millions of MS Contexts

The one-thread-per-MS model scales to millions of sessions when combined with an event-driven architecture:

```cpp
// MS session registry - thread-safe lookup by identity
class MsRegistry {
public:
    // Create or retrieve session for TMSI
    std::shared_ptr<MsSession> getOrCreate(uint32_t tmsi);

    // Remove session (channel released, timer expired)
    void remove(uint32_t tmsi);

    // Iterate all sessions for broadcast operations (e.g., paging)
    template<typename F>
    void forEach(F&& func);

private:
    std::unordered_map<uint32_t, std::shared_ptr<MsSession>> mSessions;
    std::shared_mutex mMutex; // Reader-writer lock
};
```

### Event Loop Design

```cpp
class BtsEventLoop {
public:
    void run() {
        auto lastTick = std::chrono::steady_clock::now();

        while (running) {
            auto now = std::chrono::steady_clock::now();
            auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTick);
            lastTick = now;

            // 1. Process incoming frames from radio
            processRadioFrames();

            // 2. Advance timers for all active sessions
            registry.forEach([this, delta](auto& session) {
                session->timers.tick(delta, [session](L3TimerId id) {
                    handleTimerExpired(session.get(), id);
                });
            });

            // 3. Periodic broadcasts (System Information)
            if (siCounter++ % SI_INTERVAL == 0) {
                broadcastSystemInfo();
            }

            // 4. Yield to allow other threads
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
};
```

### Memory Budget Planning

| Scale | MS Sessions | Stack Module Memory | ParsedMessage (stack, transient) |
|-------|------------|-------------------|-------------------------------|
| Small cell | 100 | ~277 KB | 800 KB peak |
| Macro cell | 10,000 | ~27.7 MB | 80 MB peak |
| Large deployment | 1,000,000 | ~2.7 GB | 8 GB peak (transient) |

For large deployments, ParsedMessage is only on-stack during message processing (microseconds), so peak concurrent usage is much lower than the theoretical maximum.

### Channel Pool Scaling

A typical macro cell BTS might have:
- 4 SDCCH channels (control channel)
- 24 TCHF channels (full-rate traffic, across 3 TRX × 8 timeslots)
- 3 TCHH channels (half-rate traffic)

The `ChannelPool` handles this with negligible memory overhead (~1 KB for the unordered_map entries).

### Transaction Limits

Each MS can have up to 16 concurrent pending transactions (`TransactionManager::MAX_TRANSACTIONS = 16`). For typical BTS workloads, < 4 concurrent transactions per MS is expected. The `cleanup()` method should be called periodically or when `totalCount()` approaches the limit.

## 7. Deployment Checklist

- [ ] Build with C++20, Release mode (`-O2` or `/O2`)
- [ ] Verify `sizeof(MSContext) <= 256` via `static_assert`
- [ ] Configure `ChannelPool` with available channels at startup
- [ ] Set up `ProtocolDispatcher` handlers for expected message types
- [ ] Integrate `TimerManager::tick()` into event loop (10-100ms interval)
- [ ] Implement `TransactionManager::cleanup()` on timer expiry or periodically
- [ ] Provide external synchronization for shared `ChannelPool`
- [ ] Set up PHY backend (SDR, GNU Radio block, etc.)
- [ ] Configure System Information broadcast schedule
- [ ] Add logging for FSM state transitions and timer expirations

## 8. References

| Document | Topic |
|----------|-------|
| [doc/API.md](API.md) | Full API reference (36 sections) |
| [doc/bts_integration.md](bts_integration.md) | Step-by-step integration guide |
| [doc/builder_coverage.md](builder_coverage.md) | Builder pattern coverage table |
| 3GPP TS 24.008 | Mobile radio interface L3 specification |
| 3GPP TS 44.018 | Group call and broadcast call control |
| GSM 04.06 / 3GPP TS 24.022 | LAPDm framing for Um interface |
| GSM 04.08 | Layer 3 specification (legacy reference) |
