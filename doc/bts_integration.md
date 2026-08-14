# BTS Integration Guide

This guide explains how to integrate libgsml3parser into a software Base Transceiver Station (BTS) implementation. It covers the full message lifecycle: building L3 messages, framing them in LAPDm, dispatching incoming messages, managing timers and transactions, driving protocol state machines, and handling typical BTS scenarios.

## Architecture Overview

libgsml3parser sits between the BTS application logic and the physical/radio layer. The library provides two layers of capability:

```
┌─────────────────────────────────────────────────────────────────────┐
│                      BTS Application Logic                          │
│  (roaming decisions, authentication, handover, call control policy) │
└───────────────────────┬─────────────────────────┬───────────────────┘
                        │                         │
      ┌─────────────────▼──────────┐ ┌───────────▼──────────────────┐
      │    STACK MODULES           │ │     CORE PARSER API          │
      │  (per-MS state management) │ │  (parse, serialize, build)   │
      │                           │ │                              │
      │  MSContext                │ │  Builder Pattern             │
      │  TimerManager             │ │  ProtocolDispatcher           │
      │  TransactionManager       │ │  writeL3Bytes() / parseL3()   │
      │  RR/MM/CC StateMachine    │ │  lapdm::wrapL3() / unwrapL3() │
      │  ChannelPool              │ │                              │
      └───────────┬───────────────┘ └──────────┬───────────────────┘
                  │                            │
      ┌───────────▼────────────────────────────▼───────────────────┐
      │                    Radio / Um Interface                     │
      │            (air interface, L1/PHY, SDR backend)            │
      └────────────────────────────────────────────────────────────┘
```

### Outbound Flow (BTS → MS)

1. **Decide** — State machine or application logic determines what to send
2. **Build** — Create an L3 message using the Builder API
3. **Serialize** — Convert to raw bytes with `writeL3Bytes()`
4. **Frame** — Wrap in LAPDm header with `lapdm::wrapL3()`
5. **Transmit** — Send frame bytes to the radio layer
6. **Track** — Create a Transaction, start a Timer (if expecting response)

### Inbound Flow (MS → BTS)

1. **Receive** — Get raw bytes from the radio layer
2. **Unframe** — Extract L3 payload with `lapdm::unwrapL3()`
3. **Parse** — Convert to typed C++ object with `parseL3()`
4. **Dispatch** — Route to handler via `ProtocolDispatcher`
5. **Correlate** — Match against pending Transactions via `TransactionManager`
6. **Advance** — Feed message into FSM, stop timers, update MSContext

## Building a BTS with libgsml3parser

### Step 1: Define per-MS state

Each mobile station gets its own context bundle:

```cpp
#include <gsml3parser/gsml3parser.hpp>

using namespace gsml3parser;

struct MsSession {
    MSContext ctx;              // Identity, channel, flags
    TimerManager timers;        // Protocol timers (T3101, T3106, ...)
    TransactionManager txns;    // Pending request-response pairs
    RRStateMachine rrFsm;       // Radio Resource state machine
    MMStateMachine mmFsm;       // Mobility Management state machine
    CCStateMachine ccFsm;       // Call Control state machine
    ProtocolDispatcher dispatch; // Message router
};
```

### Step 2: Initialize channel pool (global, shared)

The `ChannelPool` is shared across all MS sessions at the BTS level:

```cpp
ChannelPool btsChannels;

void initBts() {
    // Register SDCCH channels (control)
    for (int ts = 0; ts < 4; ++ts) {
        btsChannels.addChannel({ChannelType::SDCCHType, 0, static_cast<uint8_t>(ts), 100});
    }

    // Register TCHF channels (traffic, full-rate)
    for (int trx = 0; trx < 3; ++trx) {
        for (int ts = 0; ts < 8; ++ts) {
            btsChannels.addChannel({ChannelType::TCHFType,
                                    static_cast<uint8_t>(trx),
                                    static_cast<uint8_t>(ts),
                                    static_cast<uint16_t>(200 + trx * 10 + ts)});
        }
    }

    // Register TCHH channels (traffic, half-rate)
    for (int trx = 0; trx < 3; ++trx) {
        btsChannels.addChannel({ChannelType::TCHHType,
                                static_cast<uint8_t>(trx), 0,
                                static_cast<uint16_t>(300 + trx)});
    }
}
```

### Step 3: Register message handlers

Wire up the dispatcher for each MS session:

```cpp
void setupDispatcher(ProtocolDispatcher& disp, MsSession* session) {
    // RR handlers
    disp.registerHandler(L3PD::RadioResource, L3PagingResponse::MTI,
        [session](const ParsedMessage& msg, void*) {
            handlePagingResponse(session, msg);
        });

    disp.registerHandler(L3PD::RadioResource, L3MeasurementReport::MTI,
        [session](const ParsedMessage& msg, void*) {
            handleMeasurementReport(session, msg);
        });

    // MM handlers
    disp.registerHandler(L3PD::MobilityManagement, L3CMServiceRequest::MTI,
        [session](const ParsedMessage& msg, void*) {
            handleCMServiceRequest(session, msg);
        });

    disp.registerHandler(L3PD::MobilityManagement, L3AuthenticationResponse::MTI,
        [session](const ParsedMessage& msg, void*) {
            handleAuthResponse(session, msg);
        });

    // CC handlers
    disp.registerHandler(L3PD::CallControl, L3Setup::MTI,
        [session](const ParsedMessage& msg, void*) {
            handleCallSetup(session, msg);
        });

    // Fallback: log unexpected messages
    disp.setFallbackHandler([](const ParsedMessage& msg, void*) {
        logWarning("Unhandled message: {}", messageName(msg));
    });
}
```

### Step 4: Process incoming frames

The main event loop for a single MS channel:

```cpp
void processIncomingFrame(MsSession* session, std::span<const uint8_t> rawFrame) {
    // 1. Unwrap LAPDm
    auto l3Payload = gsml3parser::lapdm::unwrapL3(rawFrame);
    if (!l3Payload) return;

    // 2. Parse L3 header and message
    auto header = parseL3Header(*l3Payload);
    if (!header) return;

    auto msg = parseL3(l3Payload->subspan(2)); // skip 2-byte header
    if (!msg) return;

    // 3. Advance timers
    static auto lastTick = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTick);
    lastTick = now;

    session->timers.tick(delta, [&session](L3TimerId expiredId) {
        handleTimerExpired(session, expiredId);
    });

    // 4. Try to correlate with pending transaction
    Transaction* tx = session->txns.match(*header, *msg);
    if (tx) {
        // This is a response to our request
        stopTimerForTransaction(session, tx);
    }

    // 5. Feed into state machines
    auto rrResult = session->rrFsm.processMessage(*msg);
    if (rrResult.causesTransition()) {
        handleRrTransition(session, rrResult);
    }

    // 6. Dispatch to application handlers
    session->dispatch.dispatch(*msg, session);
}
```

## Typical BTS Procedures

### Channel Assignment (RACH → Immediate Assignment)

Handle a Channel Request and respond with an Immediate Assignment:

```cpp
void handleChannelRequest(MsSession* session, const ParsedMessage& msg) {
    auto* cr = tryGet<L3ChannelRequest>(msg);
    if (!cr) return;

    uint8_t ra = cr->requestReference();

    // Decode needed channel type from RA
    ChannelType needed = decodeChannelNeeded(ra, false, /*vea=*/true);

    // Allocate from pool
    auto ch = btsChannels.allocate(needed);
    if (!ch) {
        // Pool exhausted — send Immediate Assignment Reject
        sendImmediateAssignmentReject(session, ra);
        return;
    }

    // Update MSContext
    session->ctx.assignChannel(ch->type, ch->trxNumber, ch->timeslot, ch->arfcn);

    // Build Immediate Assignment
    auto ia = L3ImmediateAssignment::builder()
        .pageMode(L3PageMode(0))
        .requestReference(ra)
        .channelDescription(buildChannelDesc(*ch))
        .timingAdvance(L3TimingAdvance(session->ctx.timingAdvance().value_or(0)))
        .build();

    sendL3Message(session, RRM{std::move(ia)});

    // Start timer: wait for Paging Response / SABM
    session->timers.start(L3TimerId::T3109);

    // Advance RR state machine
    session->rrFsm.setState(RRStateMachine::State::CHANNEL_ASSIGNED);
}
```

### Paging a Mobile Station

Send a Paging Request Type 2 to page a mobile by TMSI:

```cpp
void pageMs(uint32_t tmsi, ChannelType wantedChannel) {
    auto paging = L3PagingRequestType2::builder()
        .addTMSI(tmsi, wantedChannel)
        .build();

    ParsedMessage pm{RRM{std::move(paging)}};
    auto bytes = writeL3Bytes(pm);
    auto frame = wrapL3(*bytes, SAPI::SAPI0, false);

    // Broadcast on BCCH/PAGCH
    bcchTransmitter.send(frame.data(), frame.size());
}
```

### Authentication Procedure

Full authentication flow with timer and transaction management:

```cpp
void startAuthentication(MsSession* session) {
    // Generate RAND (128-bit challenge) from AuC
    uint8_t rand[16] = {/* from auth center */};
    uint8_t ksn = 0;

    session->mmFsm.setState(MMStateMachine::State::AUTHENTICATION);

    auto authReq = L3AuthenticationRequest::builder()
        .cksn(ksn)
        .rand(rand)
        .build();

    // Create transaction: expect AuthenticationResponse
    auto txId = session->txns.create(
        L3PD::MobilityManagement,
        L3AuthenticationRequest::MTI,
        0,  // TI not used for MM
        L3TimerId::T3106
    );

    // Start timer T3106 (3s default)
    session->timers.start(L3TimerId::T3106);

    sendL3Message(session, MMM{std::move(authReq)});
}

void handleAuthResponse(MsSession* session, const ParsedMessage& msg) {
    auto* resp = tryGet<L3AuthenticationResponse>(msg);
    if (!resp) return;

    // Stop timer — we got a response
    session->timers.stop(L3TimerId::T3106);

    // Verify SRES against expected response from AuC
    uint32_t sres = resp->sres();
    if (sres == expectedSRES) {
        session->ctx.setAuthenticated(true);
        session->mmFsm.setState(MMStateMachine::State::AUTHENTICATED);

        // Mark transaction as completed
        Transaction* tx = session->txns.match(msg);
        if (tx) tx->complete();
    } else {
        sendAuthenticationReject(session);
        session->mmFsm.setState(MMStateMachine::State::DEREGISTERED);
    }
}
```

### Location Update Procedure

Full location update with TMSI reallocation:

```cpp
void handleLocationUpdateRequest(MsSession* session, const ParsedMessage& msg) {
    auto* lur = tryGet<L3LocationUpdatingRequest>(msg);
    if (!lur) return;

    // Verify identity, check VLR
    session->mmFsm.setState(MMStateMachine::State::LOCATION_UPDATE);

    // Assign new TMSI
    uint32_t newTmsi = allocateTmsi();
    session->ctx.setTMSI(newTmsi);
    session->ctx.setRegistered(true);

    // Send Location Updating Accept with new TMSI
    auto accept = L3LocationUpdatingAccept::builder()
        .lai(session->ctx.lai().value_or(L3LocationAreaIdentity{}))
        .mobileIdentity(L3MobileIdentity(newTmsi))
        .build();

    sendL3Message(session, MMM{std::move(accept)});

    // Start T3108 timer for TMSI reallocation complete
    session->txns.create(
        L3PD::MobilityManagement,
        L3LocationUpdatingAccept::MTI,
        0, L3TimerId::T3108
    );
    session->timers.start(L3TimerId::T3108);

    session->mmFsm.setState(MMStateMachine::State::REGISTERED);
}
```

### System Information Broadcast

Build and broadcast SI messages on BCCH:

```cpp
void broadcastSystemInfo() {
    // SI Type 3 — Full cell description (most important for MS camp-on)
    auto si3 = L3SystemInformationType3::builder()
        .cellIdentity(L3CellIdentity(0x1234))
        .locationAreaIdentity(L3LocationAreaIdentity("250", "01", 0x5678))
        .controlChannelDescription(buildControlChannelDesc())
        .cellOptions(L3CellOptionsBCCH{})
        .cellSelectionParameters(L3CellSelectionParameters{})
        .rachControlParameters(L3RACHControlParameters{})
        .build();

    ParsedMessage pm{RRM{std::move(si3)}};
    auto bytes = writeL3Bytes(pm);
    auto frame = wrapL3(*bytes, SAPI::SAPI0, false);

    // Broadcast periodically on BCCH
    bcchTransmitter.broadcast(frame);
}
```

## Message Correlation

The `TransactionManager` provides request-response correlation for L3 messaging. When the BTS sends a request message and expects a response, it creates a Transaction to track the pending exchange:

### Creating Transactions

```cpp
// CC message with TI-based correlation
auto txId = session->txns.create(
    L3PD::CallControl,        // PD
    L3Setup::MTI,             // MTI of request
    1,                        // Transaction Identifier (0-7)
    L3TimerId::T3101          // Timer to associate
);

// MM message with PD+MTI correlation (TI not used)
auto txId2 = session->txns.create(
    L3PD::MobilityManagement,
    L3IdentityRequest::MTI,
    0,                        // TI=0 for non-CC/SS
    L3TimerId::T3102
);
```

### Matching Incoming Messages

```cpp
// Parse the L3 header to get PD, MTI, and TI
auto header = parseL3Header(rawData).value();
auto msg = parseL3(rawData.subspan(2)).value();

// Match against pending transactions
Transaction* tx = session->txns.match(*header, *msg);
if (tx) {
    // Found matching transaction — this is the expected response
    tx->complete();

    // Stop the associated timer
    session->timers.stop(tx->timerId());

    // Clean up finished transactions periodically
    if (session->txns.totalCount() > 8) {
        session->txns.cleanup();
    }
}
```

### Matching Strategies

| Protocol Discriminator | Matching Strategy | Complexity |
|------------------------|-------------------|------------|
| `CallControl` (PD=0x03) | TI-based lookup via `mTiIndex[8]` | O(1) |
| `NonCallSS` (PD=0x0b) | TI-based lookup via `mTiIndex[8]` | O(1) |
| All other PDs | PD + MTI comparison | O(K), K ≤ 16 |

### Timer Expiry Handling

```cpp
void handleTimerExpired(MsSession* session, L3TimerId timerId) {
    // Expire all transactions associated with this timer
    session->txns.onTimerExpired(timerId);

    // Handle based on which timer expired
    switch (timerId) {
        case L3TimerId::T3101:
            // CM Service Request retransmission or abort
            retransmitOrAbort(session, timerId);
            break;
        case L3TimerId::T3106:
            // Authentication failed — no response
            session->ctx.setAuthenticated(false);
            sendAuthenticationReject(session);
            break;
        case L3TimerId::T3109:
            // Paging response timeout — release channel
            releaseChannelForMs(session);
            break;
        default:
            logWarning("Unhandled timer expiry: {}", l3TimerName(timerId));
            break;
    }

    // Clean up expired transactions
    session->txns.cleanup();
}
```

## Timer Management

The `TimerManager` integrates into your event loop using either callback-based or span-based tick:

### Callback-Based Tick (Recommended)

Zero heap allocation. The callback is invoked for each expired timer:

```cpp
// In your event loop, called every 10-100ms:
void eventLoopTick(MsSession* session, std::chrono::milliseconds delta) {
    session->timers.tick(delta, [session](L3TimerId expiredId) {
        handleTimerExpired(session, expiredId);
    });
}
```

### Span-Based Tick (Batch Processing)

Caller provides a pre-allocated buffer:

```cpp
std::array<L3TimerId, 32> expired;
size_t n = session->timers.tick(delta, expired);
for (size_t i = 0; i < n; ++i) {
    handleTimerExpired(session, expired[i]);
}
```

### Timer Lifecycle

```cpp
// Start with default duration from spec
session->timers.start(L3TimerId::T3101);        // 3000ms

// Start with custom duration
session->timers.start(L3TimerId::T3109, 15000ms); // 15s paging response

// Check state
if (session->timers.isRunning(L3TimerId::T3101)) {
    auto remaining = session->timers.remaining(L3TimerId::T3101);
}

// Stop when response arrives
session->timers.stop(L3TimerId::T3101);

// Stop all (e.g., on channel release)
session->timers.stopAll();
```

### Timer Reference Table

| Timer | Default | Used For |
|-------|---------|----------|
| T3101 | 3s | CM service request retransmission |
| T3102 | 3s | Identity response retransmission |
| T3103 | 5s | Location updating request retransmission |
| T3106 | 3s | Authentication response retransmission |
| T3108 | 3s | TMSI reallocation complete retransmission |
| T3109 | 30s | Paging response (etom × 5s) |
| T3111 | 3s | CM reestablishment request |
| T3112 | 3s | IMSI detach indication |
| T3113 | 3s | MM status retransmission |

## State Machine Integration

### Using Default FSM Skeletons

The library provides RR, MM, and CC state machine skeletons with standard transitions:

```cpp
// RR State Machine — handles channel setup, handover, ciphering
RRStateMachine rrFsm;
rrFsm.setState(RRStateMachine::State::IDLE);

auto result = rrFsm.processMessage(channelRequestMsg);
if (result.action == SMAction::Transition) {
    int newState = result.nextState.value();
    // Handle transition, e.g., send Immediate Assignment
}
```

### Custom FSM with Override

Inherit from a skeleton and override specific transitions:

```cpp
class MyRRFSM : public RRStateMachine {
protected:
    SMResult handle_message_impl(int state, const ParsedMessage& msg) override {
        // Custom: in ACTIVE state, start ciphering on MM messages
        if (state == State::ACTIVE && messagePD(msg) == L3PD::MobilityManagement) {
            return {SMAction::Transition, static_cast<int>(State::CIPHER_MODE)};
        }

        // Custom: handle handover decision based on measurement reports
        if (state == State::ACTIVE) {
            if (auto* report = tryGet<L3MeasurementReport>(msg)) {
                if (shouldHandover(report)) {
                    return {SMAction::Transition, static_cast<int>(State::HANDOVER)};
                }
            }
        }

        // Fall back to default transitions
        return RRStateMachine::handle_message_impl(state, msg);
    }
};
```

### Coordinating Multiple FSMs

A typical MS session runs three FSMs in parallel:

```cpp
void processMessage(MsSession* session, const ParsedMessage& msg) {
    L3PD pd = messagePD(msg);

    switch (pd) {
        case L3PD::RadioResource: {
            auto result = session->rrFsm.processMessage(msg);
            handleSmResult(session, result, "RR");
            break;
        }
        case L3PD::MobilityManagement: {
            auto result = session->mmFsm.processMessage(msg);
            handleSmResult(session, result, "MM");

            // MM acceptance may enable CC
            if (session->mmFsm.state() == MMStateMachine::State::REGISTERED) {
                session->ccFsm.setState(CCStateMachine::State::IDLE);
            }
            break;
        }
        case L3PD::CallControl: {
            auto result = session->ccFsm.processMessage(msg);
            handleSmResult(session, result, "CC");
            break;
        }
        default:
            // GMM, SM, SMS — handled by domain-specific logic
            break;
    }
}
```

### FSM State Transition Diagrams

#### RR State Machine

```
                    ChannelRequest
IDLE ──────────────────────────────► CHANNEL_REQUESTED
                                              │
                                    (send ImmediateAssignment)
                                              │
                                     PagingResponse / SABM
                                              │
                                              ▼
                                  CHANNEL_ASSIGNED ──► LINK_ESTABLISHED
                                              │                    │
                                    T3109 expiry           MM messages
                                              │                    │
                                              ▼                    ▼
                                     CHANNEL_RELEASE    WAITING_MM ──► ACTIVE
                                                                       │
                                                        ┌──────────────┤
                                                        │              │
                                               CipherModeCmd  HandoverCmd
                                                        │              │
                                                        ▼              ▼
                                                   CIPHER_MODE   HANDOVER
                                                        │              │
                                              CipherModeComplete  HO Complete
                                                        │              │
                                                        ▼              ▼
                                                      ACTIVE ◄─────────┘
```

#### MM State Machine

```
DEREGISTERED ── CMServiceRequest ──► SERVICE_REQUEST
                                           │
                                  IdentityResponse
                                           │
                                           ▼
                                      IDENTITY_VERIFIED
                                           │
                                   (send AuthRequest)
                                           │
                                  AuthenticationResponse
                                           │
                                           ▼
                                        AUTHENTICATED
                                           │
                                    LocationUpdatingReq
                                           │
                                           ▼
                                     LOCATION_UPDATE ──► REGISTERED
```

#### CC State Machine

```
IDLE ── Setup ──► SETUP_RECEIVED ──► PROCEEDING ──► ALERTING
                                                               │
                                                      Connect  │
                                                               ▼
                                                             CONNECT
                                                               │
                                                    CallConfirmed │
                                                               ▼
                                                            ACTIVE
                                                               │
                                                    Disconnect  │
                                                               ▼
                                                   DISCONNECT_RECEIVED
                                                               │
                                                              ▼
                                                           RELEASE
```

## Comparison with Existing BTS Frameworks

### osmo-bts

| Aspect | osmo-bts | libgsml3parser |
|--------|----------|----------------|
| Language | C (libosmocore) | C++20 |
| Message types | Hand-coded parsers per message | `std::variant` with 190+ typed messages |
| Type safety | Enum-based, manual casting | Compile-time `std::visit`, `tryGet<T>()` |
| Memory model | Heap-allocated structs | Stack-allocated variants, zero-alloc hot paths |
| Builder API | None (manual struct construction) | Fluent builder for every message type |
| State machines | Implicit in handler code | Explicit FSM skeletons with O(1) dispatch |
| Timer management | Custom osmo_timer | Spec-aligned L3Timer with TimerManager |

libgsml3parser can replace the TRX-layer message handling in osmo-bts while keeping the existing LAPDm and Abis layers.

### OpenBTS / srsRAN

| Aspect | OpenBTS (BTS.cpp) | srsRAN | libgsml3parser |
|--------|-------------------|--------|----------------|
| L3 parsing | Custom C++ structs | Custom C++ classes | Standardized variant hierarchy |
| Message generation | Manual byte construction | Builder-like patterns | Fluent Builder API for all 190+ types |
| Round-trip testing | Limited | Protocol-level tests | Full parse→serialize→parse round-trips |
| Performance | N/A | Optimized C++ | Zero-alloc hot paths, cache-friendly layout |

For srsRAN integration: drop in libgsml3parser as the L3 parse/generate library, link against `libgsml3parser.a`, and use `ProtocolDispatcher` + FSM skeletons as the message routing backbone.

## Full Pipeline Example

Complete round-trip with all stack modules:

```cpp
// 1. Build a Paging Request Type 2
auto paging = L3PagingRequestType2::builder()
    .addTMSI(0xDEADBEEF, ChannelType::SDCCHType)
    .build();

ParsedMessage pm{RRM{std::move(paging)}};

// 2. Serialize to raw bytes
auto l3Bytes = writeL3Bytes(pm);

// 3. Wrap in LAPDm frame
auto frame = wrapL3(*l3Bytes, SAPI::SAPI0, false);

// --- Simulate transmission over the air and MS response ---

// 4. Receive MS response (Paging Response)
std::vector<uint8_t> msFrame = {/* from radio */};

// 5. Unwrap LAPDm
auto payload = unwrapL3(msFrame);

// 6. Parse L3 header + message
auto header = parseL3Header(*payload);
auto msg = parseL3(payload->subspan(2));

// 7. Create MS session
MsSession* session = createSession();
session->ctx = MSContext::createWithTMSI(0xDEADBEEF);

// 8. Feed into RR state machine
auto result = session->rrFsm.processMessage(*msg);
assert(result.causesTransition());

// 9. Allocate channel for MS
auto ch = btsChannels.allocate(ChannelType::SDCCHType);
session->ctx.assignChannel(ch->type, ch->trxNumber, ch->timeslot, ch->arfcn);

// 10. Send Immediate Assignment
auto ia = L3ImmediateAssignment::builder()
    .channelDescription(buildChannelDesc(*ch))
    .build();
sendL3Message(session, RRM{std::move(ia)});

// 11. Start timer for next expected message
session->timers.start(L3TimerId::T3109);
```

## Error Handling in Production

Always check `Expected<T>` results:

```cpp
auto msg = parseL3(data);
if (!msg) {
    auto& err = msg.error();
    logError("Parse failed: code={} bit={} msg={}",
        static_cast<int>(err.code), err.bitPosition, err.message);
    return;
}
```

For LAPDm operations:

```cpp
auto payload = unwrapL3(frame);
if (!payload) {
    // Frame too short or malformed — drop and log
    return;
}
```

For stack module operations:

```cpp
auto ch = pool.allocate(ChannelType::SDCCHType);
if (!ch) {
    // No channels available — reject or queue
    sendImmediateAssignmentReject(session, ra);
    return;
}

auto txId = session->txns.create(pd, mti, ti, timerId);
if (!txId) {
    // Transaction pool full — cleanup first
    session->txns.cleanup();
    txId = session->txns.create(pd, mti, ti, timerId);
}
```

## Performance Considerations

- **Zero heap allocation on parse path**: `ParsedMessage` is a stack-allocated variant (< 8 KB)
- **MSContext ≤ 256 bytes**: Fits in L1 cache; millions of contexts fit in L3 cache
- **TimerManager**: Fixed-size `std::array`, no dynamic allocation, tick() uses callback or span
- **TransactionManager**: O(1) TI lookup for CC/SS, bounded scan (≤ 16) for others
- **ChannelPool**: O(1) allocate via per-type free-list, cold-path add/remove
- **FSM dispatch**: `switch(PD) + switch(MTI)` — compile-time resolved, no vtable on critical path
- **Thread safety**: Each MS session is accessed from one thread. `ChannelPool` requires external synchronization for multi-threaded access.
- **Arena allocator**: Use `Arena` for high-throughput batch parsing to reduce malloc pressure
- **Streaming**: Use `L3StreamProcessor` with `RingBuffer` for continuous frame processing

## API Reference Summary

| Module | Key Types | Purpose |
|--------|-----------|---------|
| `parser.h` | `parseL3()`, `writeL3Bytes()` | Parse and serialize L3 messages |
| `lapdm.h` | `wrapL3()`, `unwrapL3()` | LAPDm framing (GSM 04.06) |
| `dispatcher.h` | `ProtocolDispatcher` | Message routing with TI support |
| `visitor.h` | `tryGet<T>()`, `messageName()` | Type access and metadata |
| Builder API | `MessageType::builder()` | Construct L3 messages fluently |
| `stack/ms_context.h` | `MSContext` | Per-subscriber state (≤ 256 bytes) |
| `stack/l3_timer.h` | `L3Timer`, `TimerManager` | Protocol timers, zero-alloc tick |
| `stack/transaction.h` | `Transaction`, `TransactionManager` | Request-response correlation |
| `stack/state_machine.h` | `RR/MM/CCStateMachine` | Protocol FSM skeletons |
| `stack/channel_pool.h` | `ChannelPool`, `decodeChannelNeeded()` | Channel allocation, VEA |

## See Also

- [README.md](../README.md) — Library overview and quick start
- [doc/API.md](API.md) — Full API reference (all 36 sections)
- [doc/bts_architecture.md](bts_architecture.md) — Architecture overview and scaling guide
- [doc/builder_coverage.md](builder_coverage.md) — Complete Builder coverage table
- `examples/` directory — Working BTS example programs
