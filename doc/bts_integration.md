# BTS Integration Guide

This guide explains how to integrate libgsml3parser into a software Base Transceiver Station (BTS) implementation. It covers the full message lifecycle using the Procedure Framework: managing subscriber sessions, feeding L3 messages into procedures, building responses with ResponseBuilder, handling timers and external data, and integrating with A-bis RSL.

## Architecture Overview

libgsml3parser sits between the BTS application logic and the physical/radio layer. The library provides two layers of capability:

```
┌─────────────────────────────────────────────────────────────────────┐
│                      BTS Application Logic                          │
│  (AuC/HLR integration, VLR/BSC decisions, call control policy)      │
└───────────────────────┬────────────────────────┬────────────────────┘
                        │                        │
      feedExternal()    │                        │  sendToMS()
                        │                        │
      ┌─────────────────▼──────────┐ ┌───────────▼───────────────────┐
      │    PROCEDURE FRAMEWORK     │ │     CORE PARSER API           │
      │  (per-MS procedure mgmt)   │ │  (parse, serialize, build)    │
      │                            │ │                               │
      │  SubscriberRegistry        │ │  ResponseBuilder              │
      │  ProcedureRunner           │ │  RSLParser / RSLBuilder       │
      │  + Concrete Procedures     │ │  parseL3() / writeL3Bytes()   │
      └───────────┬────────────────┘ └─────────┬─────────────────────┘
                  │                            │
      ┌───────────▼────────────────────────────▼───────────────────┐
      │              Radio / Um Interface / A-bis RSL              │
      │            (air interface, L1/PHY, SDR backend)            │
      └────────────────────────────────────────────────────────────┘
```

### Outbound Flow (BTS -> MS)

1. **Procedure decides** - ProcedureRunner feeds message, procedure determines response needed
2. **ResponseSink fires** - Callback invokes ResponseBuilder to construct L3 response bytes
3. **Arena buffer** - Bytes written into pre-allocated Arena buffer (zero heap allocation)
4. **Frame & Transmit** - Pass to `LAPDmEntity.sendUI()` (unacknowledged) or `.sendData()` (acknowledged, segmented)
5. **A-bis encapsulation** (optional) - Wrap L3 bytes in RSL via `RSLBuilder::buildDataInd()`

### Inbound Flow (MS -> BTS)

1. **Receive** - Get raw LAPDm frame bytes from the radio layer or A-bis RSL
2. **Process LAPDm** - Pass to `LAPDmEntity.receiveFrame()`. The entity decodes the frame and invokes the `L3ReceiveFn` callback with complete L3 messages.
3. **Parse** - In the callback, convert L3 bytes to typed C++ object with `parseL3()`
4. **Find Session** - Look up `SubscriberSession` via `SubscriberRegistry.findByLink()` or `findByTMSI()`
5. **Feed Procedure** - Route message to `ProcedureRunner::feed()` — procedure auto-creates or routes to active procedure
6. **Handle Result** - Check `ProcedureStepResult`: Continue, SendResponse, WaitingExternal, Completed, Failed

## Building a BTS with libgsml3parser

### Step 1: Initialize subscriber registry and channel pool

```cpp
#include <gsml3parser/gsml3parser.hpp>

using namespace gsml3parser;

// Global channel pool (shared across all MS sessions)
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
}
```

### Step 2: Define the ResponseSink callback

The `ResponseSink` callback is invoked by procedures when they need to send a response to the MS. This is where you call `ResponseBuilder` and send the bytes over the radio.

```cpp
// Arena for zero-heap-allocation response building
Arena arena(65536);

void onResponse(SMAction action, const ParsedMessage& incomingMsg,
                const SubscriberSession* session) {
    if (action != SMAction::SendResponse) return;

    uint8_t buf[512];
    int n = 0;

    // Determine which response to build based on the incoming message type.
    auto pd = messagePD(incomingMsg);
    auto mti = messageMTI(incomingMsg);

    if (pd == L3PD::MobilityManagement) {
        if (mti == L3CMServiceRequest::MTI) {
            n = ResponseBuilder::buildCMServiceAccept({buf, sizeof(buf)});
        }
    } else if (pd == L3PD::CallControl) {
        if (mti == L3Setup::MTI) {
            if (auto* setup = tryGet<L3Setup>(incomingMsg)) {
                n = ResponseBuilder::buildCallProceeding({buf, sizeof(buf)}, setup->ti());
            }
        }
    }

    if (n > 0 && session) {
        sendToMS(session, buf, n);  // Your function to transmit L3 bytes
    }
}
```

### Step 3: Process incoming frames with ProcedureRunner

The main event loop processes incoming L3 messages by feeding them into the subscriber's `ProcedureRunner`:

```cpp
// L3 callback: invoked by LAPDmEntity when a complete L3 message arrives
static void onL3(SAPI sapi, Primitive prim, std::span<const uint8_t> l3Data, void* ctx) {
    auto* btsCtx = static_cast<BtsContext*>(ctx);

    // Parse L3 message
    auto msg = parseL3(l3Data);
    if (!msg) return;

    // Find the subscriber session for this link
    SubscriberSession* session = btsCtx->registry.findByLink(
        /*trx=*/0, /*ts=*/5, /*lapdmLink=*/3);
    if (!session) return;

    // Feed into ProcedureRunner — auto-creates or routes to active procedure
    auto result = session->procedures.feed(*msg, session, onResponse);

    switch (result.action) {
        case ProcedureStepResult::Action::Continue:
            // Procedure continues, awaiting next message
            break;

        case ProcedureStepResult::Action::SendResponse:
            // Response already built and sent via onResponse callback
            break;

        case ProcedureStepResult::Action::WaitingExternal:
            // Procedure needs external data (RAND from AuC, VLR decision)
            // Call feedExternal() when the data is available
            handleWaitingExternal(session, result);
            break;

        case ProcedureStepResult::Action::Completed:
            // Procedure finished successfully; slot freed automatically
            logInfo("Procedure completed: {}", result.finalResult.reason);
            break;

        case ProcedureStepResult::Action::Failed:
            // Procedure failed (timeout, error); slot freed automatically
            logWarning("Procedure failed: {}", result.finalResult.reason);
            break;
    }
}
```

### Step 4: Event loop with timer management

```cpp
void eventLoop() {
    auto lastTick = std::chrono::steady_clock::now();

    while (running) {
        auto now = std::chrono::steady_clock::now();
        auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTick);
        lastTick = now;

        // 1. Process incoming radio frames
        processRadioFrames();

        // 2. Tick all session timers and procedure timers
        registry.forEach([delta](SubscriberSession* sess) {
            // Tick LAPDm T200 timer
            sess->lapdm.tickT200(delta);

            // Tick L3 timers
            sess->timers.tick(delta, [sess](L3TimerId expiredId) {
                handleTimerExpired(sess, expiredId);
            });

            // Tick procedure timers
            size_t failed = sess->procedures.tickAll(delta);
            if (failed > 0) {
                logWarning("{} procedures timed out", failed);
            }
        });

        // 3. Periodic broadcasts (System Information)
        if (siCounter++ % SI_INTERVAL == 0) {
            broadcastSystemInfo();
        }

        // 4. Reset arena periodically to reclaim memory
        if (arena.used() > 32768) arena.reset();

        // 5. Yield
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}
```

## Typical BTS Procedures with ProcedureRunner

### Channel Assignment (RACH -> Immediate Assignment)

The `ChannelAssignmentProcedure` handles the full channel assignment flow automatically:

```cpp
// MS sends Channel Request on RACH. The LAPDm callback invokes parseL3().
// ProcedureRunner auto-creates a ChannelAssignmentProcedure for RR messages.

auto msg = parseL3(l3Data).value();
auto* session = registry.findByLink(trx, ts, lapdmLink);

// Feed the message — procedure auto-starts and handles everything:
// 1. Allocates channel from pool (via internal logic or your callback)
// 2. Builds ImmediateAssignment via ResponseSink
// 3. Starts T3101 timer for channel seizure
// 4. Returns Continue, waiting for MS to seize the channel
auto result = session->procedures.feed(*msg, session, onResponse);

// Later, when MS seizes the channel (first L3 message on new channel):
auto seizeMsg = parseL3(seizeData).value();
result = session->procedures.feed(*seizeMsg, session, onResponse);
// result.action == Completed — procedure finished, slot freed
```

### Location Update Procedure

Full location update with authentication and VLR decision:

```cpp
// 1. MS sends CMServiceRequest (Location Updating)
auto cmReq = parseL3(incomingData).value();
auto result = session->procedures.feed(cmReq, session, onResponse);
// Auto-creates LocationUpdateProcedure, returns Continue or SendResponse

// 2. Procedure may request Identity or Authentication
//    ResponseSink builds IdentityRequest or AuthenticationRequest automatically

// 3. AuC provides RAND + expected SRES
std::array<uint8_t, 20> authData;
// First 16 bytes: RAND, next 4 bytes: expected SRES
memcpy(authData.data(), randBytes, 16);
memcpy(authData.data() + 16, expectedSres, 4);

result = session->procedures.feedExternal(
    procedure::ProcedureType::Authentication, authData);
// Procedure sends AuthenticationRequest via ResponseSink

// 4. MS responds with AuthenticationResponse
auto authResp = parseL3(authResponseData).value();
result = session->procedures.feed(authResp, session, onResponse);
// Procedure verifies SRES internally

// 5. VLR provides accept decision with new TMSI
std::array<uint8_t, 8> vlrDecision;
// Contains: accept flag + new TMSI (4 bytes)
vlrDecision[0] = 1; // accept
memcpy(vlrDecision.data() + 1, &newTmsi, 4);

result = session->procedures.feedExternal(
    procedure::ProcedureType::LocationUpdate, vlrDecision);
// Procedure sends LocationUpdatingAccept via ResponseSink
// result.action == Completed
```

### Call Setup (Mobile Originated)

Full MOC flow through ProcedureRunner:

```cpp
// 1. MS sends CMServiceRequest (MO Call)
auto cmReq = parseL3(incomingData).value();
auto result = session->procedures.feed(cmReq, session, onResponse);
// Auto-creates CallSetupMOPercedure

// 2. Procedure sends CMServiceAccept via ResponseSink
// 3. MS sends Setup
auto setup = parseL3(setupData).value();
result = session->procedures.feed(setup, session, onResponse);
// Procedure sends CallProceeding, then AssignmentCommand

// 4. MS sends AssignmentComplete
auto assignComplete = parseL3(assignCompleteData).value();
result = session->procedures.feed(assignComplete, session, onResponse);
// Procedure sends Alerting, then Connect

// 5. MS sends ConnectAcknowledge
auto connAck = parseL3(connAckData).value();
result = session->procedures.feed(connAck, session, onResponse);
// result.action == Completed — call is active, speech path established
```

## External System Integration (feedExternal)

Procedures use `feedExternal()` to receive data from external systems (AuC, HLR, VLR, BSC). This is the primary integration point for BTS business logic.

### AuC Integration (Authentication)

```cpp
// When procedure enters WaitingExternal state for Authentication:
void onAuthNeeded(SubscriberSession* session) {
    // Query AuC for RAND + SRES triplet
    auto triplet = aucQuery(session->context.identity().digits());

    // Feed RAND + expected SRES to the procedure
    std::array<uint8_t, 20> data;
    memcpy(data.data(), triplet.rand.data(), 16);
    memcpy(data.data() + 16, triplet.sres.data(), 4);

    auto result = session->procedures.feedExternal(
        procedure::ProcedureType::Authentication, data);
    // Procedure sends AuthenticationRequest to MS via ResponseSink
}
```

### VLR Integration (Location Update)

```cpp
void onLocationUpdateDecision(SubscriberSession* session, bool accept, uint32_t newTmsi) {
    std::array<uint8_t, 8> decision;
    decision[0] = accept ? 1 : 0;
    if (accept) {
        memcpy(decision.data() + 1, &newTmsi, 4);
    } else {
        decision[5] = static_cast<uint8_t>(MMRejectCause::Congestion);
    }

    auto result = session->procedures.feedExternal(
        procedure::ProcedureType::LocationUpdate, decision);

    if (result.action == ProcedureStepResult::Action::Completed) {
        logInfo("Location update completed");
    } else if (result.action == ProcedureStepResult::Action::Failed) {
        logWarning("Location update rejected");
    }
}
```

### BSC Integration (Handover)

```cpp
void onHandoverDecision(SubscriberSession* session, const L3ChannelDescription& target) {
    auto proc = session->procedures.getActive(procedure::ProcedureType::Handover);
    if (!proc) {
        // Create handover procedure explicitly
        auto hoProc = ProcedureFactory::createHandover(target);
        // Feed external trigger data to start the procedure
    }

    // Feed target channel to the handover procedure
    std::array<uint8_t, 16> targetData{};
    // Encode target channel description into bytes
    auto result = session->procedures.feedExternal(
        procedure::ProcedureType::Handover, targetData);
    // Procedure sends HandoverCommand via ResponseSink
}
```

## Abis/RSL Integration

When the BTS communicates with a BSC over the A-bis interface (TS 48.058), RSL messages wrap L3 payloads. Use `RSLParser` to extract L3 from inbound RSL, and `RSLBuilder` to encapsulate outbound L3.

### Inbound RSL (BSC -> BTS)

```cpp
void onRslMessage(std::span<const uint8_t> rslBytes) {
    // Parse RSL message
    auto parsed = RSLParser::parse(rslBytes);
    if (!parsed) return;

    // Extract L3 payload (if present)
    auto l3Payload = RSLParser::extractL3(*parsed);
    if (!l3Payload) return;  // Control message without L3 data

    // Parse L3 message
    auto msg = parseL3(*l3Payload);
    if (!msg) return;

    // Find session and feed to ProcedureRunner
    uint8_t chanNr = parsed->chanNr;
    uint8_t linkId = parsed->linkId;
    auto* session = registry.findByLink(/*trx from chanNr*/, /*ts from chanNr*/, linkId);
    if (!session) return;

    auto result = session->procedures.feed(*msg, session, onResponse);
}
```

### Outbound RSL (BTS -> BSC)

When the ResponseSink builds a response, encapsulate it in RSL for the BSC:

```cpp
void onResponseWithRsl(SMAction action, const ParsedMessage& incomingMsg,
                       const SubscriberSession* session) {
    if (action != SMAction::SendResponse) return;

    // Build L3 response into buffer
    uint8_t l3Buf[512];
    int l3Len = ResponseBuilder::buildCMServiceAccept({l3Buf, sizeof(l3Buf)});
    if (l3Len <= 0) return;

    // Encapsulate in RSL DATA_IND for BSC
    uint8_t rslBuf[1024];
    int rslLen = RSLBuilder::buildDataInd({rslBuf, sizeof(rslBuf)},
        session->channel.value().arfcn, session->lapdmLink,
        {l3Buf, static_cast<size_t>(l3Len)});

    if (rslLen > 0) {
        sendToBsc(rslBuf, rslLen);  // Your A-bis transport function
    }
}
```

### RSL Control Messages

Handle DCHAN and CCHAN control messages:

```cpp
void onRslControl(std::span<const uint8_t> rslBytes) {
    auto parsed = RSLParser::parse(rslBytes).value();

    switch (parsed.msgType) {
        case static_cast<uint8_t>(RSLDChanMessageType::ChanActiv): {
            // Channel activation from BSC
            auto mode = RSLParser::getChannelMode(parsed);
            if (mode) {
                activateChannel(parsed.chanNr, *mode);
                // Send ACK back
                uint8_t ackBuf[64];
                int n = RSLBuilder::buildChanActivAck({ackBuf, sizeof(ackBuf)},
                    parsed.chanNr, getCurrentFrameNumber());
                sendToBsc(ackBuf, n);
            }
            break;
        }

        case static_cast<uint8_t>(RSLDChanMessageType::RFChanRel): {
            // RF channel release from BSC
            releaseChannel(parsed.chanNr);
            uint8_t ackBuf[64];
            int n = RSLBuilder::buildRFChanRelAck({ackBuf, sizeof(ackBuf)}, parsed.chanNr);
            sendToBsc(ackBuf, n);
            break;
        }

        case static_cast<uint8_t>(RSLCChanMessageType::PagingCmd): {
            // Paging command from BSC — extract L3 and page the MS
            auto l3 = RSLParser::extractL3(parsed);
            if (l3) {
                broadcastPaging(*l3);
            }
            break;
        }
    }
}
```

## LAPDm Link Management

Each logical channel (SAPI) needs its own `LAPDmEntity` instance. The entity manages the full LAPDm protocol state machine per GSM 04.06.

### Opening a Channel

```cpp
auto profile = LAPDmChannelProfile::SDCCH(); // N201=20, N200=23, T200=900ms
LAPDmEntity entity(profile, onL3Callback, onL1Callback, sessionPtr);
entity.open(SAPI::SAPI0, true); // BTS side (command bit = true)
// State: LinkReleased
```

### Link Establishment

**Passive (BTS waiting for MS SABME):** The entity automatically handles incoming SABME frames. When `receiveFrame()` processes a SABME, it sends UA and transitions to `LinkEstablished`, invoking the L3 callback with `L3_ESTABLISH_INDICATION`.

**Active (BTS initiates, e.g., SAPI3 for SMS):**
```cpp
auto result = entity.sendSABME();
if (result) {
    // State: AwaitingEstablish, T200 timer started automatically
}
// When UA arrives via receiveFrame(), state -> LinkEstablished
```

### Data Transfer

```cpp
// Unacknowledged (UI frame) — works in any state
entity.sendUI(SAPI::SAPI0, l3Bytes);

// Acknowledged (I-frames with segmentation) — requires LinkEstablished
entity.sendData(l3Bytes);
```

### Link Release

```cpp
// Normal release: send DISC, wait for UA
auto result = entity.sendDISC();

// Hard release: immediate, no frames sent
entity.hardRelease();
// State: LinkReleased
```

## Timer Management

Timers are managed at two levels: LAPDm T200 timer (per link) and L3 protocol timers (per session).

### Event Loop Integration

```cpp
void eventLoopTick(SubscriberSession* session, std::chrono::milliseconds delta) {
    // Advance LAPDm T200 timer
    bool retransmitted = session->lapdm.tickT200(delta);

    // Advance L3 timers
    session->timers.tick(delta, [session](L3TimerId expiredId) {
        handleTimerExpired(session, expiredId);
    });

    // Advance procedure timers (managed by ProcedureRunner)
    size_t failed = session->procedures.tickAll(delta);
}
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

## System Information Broadcast

Build and broadcast SI messages on BCCH:

```cpp
void broadcastSystemInfo() {
    auto si3 = L3SystemInformationType3::builder()
        .cellIdentity(L3CellIdentity(0x1234))
        .locationAreaIdentity(L3LocationAreaIdentity("250", "01", 0x5678))
        .controlChannelDescription(buildControlChannelDesc())
        .cellOptions(L3CellOptionsBCCH{})
        .cellSelectionParameters(L3CellSelectionParameters{})
        .rachControlParameters(L3RACHControlParameters{})
        .build();

    ParsedMessage pm{RRM{std::move(si3)}};
    auto bytes = writeL3Bytes(pm).value();
    auto uiFrame = gsml3parser::lapdm::makeUIFrame(SAPI::SAPI0, false, std::span(bytes));
    auto frame = gsml3parser::lapdm::encodeFrame(uiFrame);

    bcchTransmitter.broadcast(frame.data(), frame.size());
}
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

For RSL operations:

```cpp
auto parsed = RSLParser::parse(rslBytes);
if (!parsed) {
    logError("RSL parse failed: {}", parsed.error().message);
    return;
}
```

For procedure results:

```cpp
auto result = session->procedures.feed(msg, session, onResponse);
if (result.action == ProcedureStepResult::Action::Failed) {
    logWarning("Procedure {} failed: {}",
        procedureTypeName(result.finalResult.type),
        result.finalResult.reason);
}
```

## Performance Considerations

- **Zero heap allocation on parse path**: `ParsedMessage` is a stack-allocated variant (< 8 KB)
- **MSContext ≤ 256 bytes**: Fits in L1 cache; millions of contexts fit in L3 cache
- **ProcedureStepResult ≤ 32 bytes**: Compact result, no heap allocation
- **ResponseBuilder span overload**: Writes directly into caller's Arena buffer, zero heap cost
- **RSLParser**: Fixed-size IE array (32 max), all pointers into original buffer — zero heap
- **TimerManager**: Fixed-size `std::array`, no dynamic allocation, tick() uses callback or span
- **ProcedureRunner**: Fixed `std::array<ProcedureSlot, 8>`, bounded scan for routing
- **SubscriberRegistry**: Hash map lookups O(1), `forEach` iterates single index
- **Thread safety**: Each MS session is accessed from one thread. `ShardedChannelPool` and `ShardedSubscriberRegistry` provide thread-safe variants for multi-threaded scenarios.
- **Arena allocator**: Use `Arena` for high-throughput response building to reduce malloc pressure

## API Reference Summary

| Module | Key Types | Purpose |
|--------|-----------|---------|
| `parser.h` | `parseL3()`, `writeL3Bytes()` | Parse and serialize L3 messages |
| `lapdm_entity.h` | `LAPDmEntity`, `LAPDmState`, `LAPDmChannelProfile` | Full LAPDm state machine (GSM 04.06) |
| `visitor.h` | `tryGet<T>()`, `messageName()` | Type access and metadata |
| Builder API | `MessageType::builder()` | Construct L3 messages fluently |
| `stack/ms_context.h` | `MSContext` | Per-subscriber state (≤ 256 bytes) |
| `stack/l3_timer.h` | `L3Timer`, `TimerManager` | Protocol timers, zero-alloc tick |
| `stack/transaction.h` | `Transaction`, `TransactionManager` | Request-response correlation |
| `stack/state_machine.h` | `RR/MM/CCStateMachine` | Protocol FSM skeletons (response-aware) |
| `stack/channel_pool.h` | `ChannelPool`, `decodeChannelNeeded()` | Channel allocation, VEA |
| `stack/response_builder.h` | `ResponseBuilder` | Factory for L3 response messages |
| `stack/procedure.h` | `Procedure`, `ProcedureStepResult`, `ResponseSink` | Base class for protocol procedures |
| `stack/procedure_runner.h` | `ProcedureRunner`, `ProcedureFactory` | Concurrent procedure management |
| `stack/subscriber_registry.h` | `SubscriberSession`, `SubscriberRegistry` | Per-MS session management |
| `abis/rsl_types.h` | `RSLDiscriminator`, `RSL_IE`, `RSLChannelNumber` | A-bis RSL type definitions |
| `abis/rsl_parser.h` | `RSLParser`, `RSLParsedMessage` | Parse RSL messages, extract L3 |
| `abis/rsl_builder.h` | `RSLBuilder` | Construct RSL messages for BSC |

## See Also

- [README.md](../README.md) - Library overview and quick start
- [doc/API.md](API.md) - Full API reference (57 sections)
- [doc/bts_architecture.md](bts_architecture.md) - Architecture overview and scaling guide
- `examples/` directory - Working BTS example programs
