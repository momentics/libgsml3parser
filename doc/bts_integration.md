# BTS Integration Guide

This guide explains how to integrate libgsml3parser into a software Base Transceiver Station (BTS) implementation using the **BTS Stack Mode** API. It covers the full message lifecycle: managing subscriber sessions, feeding L3 messages through `ProcedureOrchestrator`, building responses with `ResponseToken` + `ResponseBuilder`, handling typed external data from AuC/VLR, and integrating with A-bis RSL.

For L3-only parsing without state management, see [L3 Parser Mode](bts_architecture.md#mode-a-l3-parser-mode) in the architecture guide.

## Architecture Overview

libgsml3parser sits between the BTS application logic and the physical/radio layer. In BTS Stack Mode, `ProcedureOrchestrator` manages compound procedure chains automatically, returning `ResponseToken` values that the caller uses with `ResponseBuilder` to construct response bytes in a pre-allocated Arena buffer (zero heap allocation).

```
┌─────────────────────────────────────────────────────────────────────┐
│                      BTS Application Logic                          │
│  AuC: AuthChallenge{rand, expectedSres}     VLR: VLRDecision{...}   │
└───────────────────────┬──────────────────────┬──────────────────────┘
                        │ feedExternalTyped()  │ feedExternalTyped()
    ┌───────────────────▼──────────────────────▼──────────────────────┐
    │              ProcedureOrchestrator                              │
    │                                                                 │
    │  feed(msg, session) ──► auto-chain sub-procedures               │
    │  Returns: ProcedureStepResult{action,responseToken,finalResult} │
    │                                                                 │
    │  ResponseToken ──► ResponseBuilder::buildResponseFromToken()    │
    │                        writes to Arena buffer (zero heap alloc) │
    └───────────┬─────────────────────────────────────────────────────┘
                │ Arena bytes
    ┌───────────▼─────────────────────────────────────────────────────┐
    │              LAPDmEntity -> PHY / Radio / A-bis RSL             │
    └─────────────────────────────────────────────────────────────────┘
```

### Inbound Flow (MS -> BTS)

1. **Receive** — Get raw LAPDm frame bytes from the radio layer or A-bis RSL
2. **Process LAPDm** — Pass to `LAPDmEntity.receiveFrame()`. The entity decodes the frame and invokes the `L3ReceiveFn` callback with complete L3 messages.
3. **Parse** — In the callback, convert L3 bytes to typed C++ object with `parseL3()`
4. **Find Session** — Look up `SubscriberSession` via `SubscriberRegistry.findByLink()` or `findByTMSI()`
5. **Feed Orchestrator** — Route message to `ProcedureOrchestrator::feed()` — orchestrator auto-chains sub-procedures
6. **Handle Result** — Check `ProcedureStepResult`:
   - `Continue` — procedure awaits next message
   - `SendResponseWithToken` — build response using `result.responseToken` + `ResponseBuilder::buildResponseFromToken()`
   - `WaitingExternal` — query AuC/VLR, then call `feedExternalTyped(typedData)`
   - `Completed` — chain finished successfully (no response pending)
   - `Failed` — chain aborted (timeout, error; no response pending)

   **Response/terminal rule:** if a procedure must send a response, `action == SendResponseWithToken` is always the case — even when the procedure terminates in the same step. The terminal state is reported exclusively through `finalResult` (state == Completed/Failed/TimedOut). So after building a response, always check `finalResult.state`: if it is terminal, release the procedure/chain.

### Outbound Flow (BTS -> MS)

1. **Orchestrator returns token** — `ProcedureStepResult.action == SendResponseWithToken`, `result.responseToken` indicates message type
2. **Build response** — Call `ResponseBuilder::buildResponseFromToken(token, arenaBuffer, session)` — zero heap allocation
3. **Frame & Transmit** — Pass to `LAPDmEntity.sendUI()` (unacknowledged) or `.sendData()` (acknowledged, segmented)
4. **A-bis encapsulation** (optional) — Wrap L3 bytes in RSL via `RSLBuilder::buildDataInd()`

## Building a BTS with libgsml3parser

### Step 1: Initialize subscriber registry and channel pool

```cpp
#include <gsml3parser/gsml3parser.hpp>

using namespace gsml3parser;

// Global channel pool (shared across all MS sessions)
ChannelPool btsChannels;

// Subscriber registry (sessions are created on demand)
ShardedSubscriberRegistry<16> registry;

// App-owned per-session orchestrators: one ProcedureOrchestrator (48 bytes)
// per subscriber. SubscriberSession does not embed the orchestrator.
std::unordered_map<uint32_t, ProcedureOrchestrator> orchestrators; // keyed by TMSI

ProcedureOrchestrator& orchestratorFor(SubscriberSession* session) {
    return orchestrators[session->assignedTmsi];
}

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

### Step 2: Process incoming frames with ProcedureOrchestrator

The main event loop processes incoming L3 messages by feeding them into the subscriber's `ProcedureOrchestrator`. The orchestrator manages compound procedure chains (e.g., CMServiceRequest -> Authentication -> CipheringMode -> LocationUpdate) automatically.

Note: `SubscriberSession` does not embed the orchestrator — the BTS application owns one `ProcedureOrchestrator` (48 bytes) per session. The examples below use `orchestratorFor(session)` as the app-side lookup (e.g. a map keyed by TMSI, or a parallel structure alongside the registry).

```cpp
// Arena for zero-heap-allocation response building
Arena arena(65536);

// App-side per-session orchestrator storage (illustrative).
ProcedureOrchestrator& orchestratorFor(SubscriberSession* session);

static void onL3(SAPI sapi, Primitive prim, std::span<const uint8_t> l3Data, void* ctx) {
    auto* btsCtx = static_cast<BtsContext*>(ctx);

    // Parse L3 message
    auto msg = parseL3(l3Data);
    if (!msg) return;

    // Find the subscriber session for this link
    SubscriberSession* session = btsCtx->registry.findByLink(0, 5, 3);
    if (!session) return;

    // Feed into the session's ProcedureOrchestrator — auto-chains sub-procedures.
    auto& orchestrator = orchestratorFor(session);
    auto result = orchestrator.feed(*msg, session);

    switch (result.action) {
        case ProcedureStepResult::Action::Continue:
            // Procedure continues, awaiting next message
            break;

        case ProcedureStepResult::Action::SendResponseWithToken:
            // Build response from token into Arena buffer (zero heap allocation).
            // Response parameters come from session->response (ResponseContext),
            // which the active procedure populated as it progressed.
            uint8_t respBuf[512];
            int n = ResponseBuilder::buildResponseFromToken(
                result.responseToken, {respBuf, sizeof(respBuf)}, session);
            if (n > 0) {
                sendToMS(session, respBuf, n);
            }
            // The procedure may have terminated in the same step: the terminal
            // state is reported via finalResult, not via action.
            if (result.finalResult.state == procedure::ProcedureState::Completed ||
                result.finalResult.state == procedure::ProcedureState::Failed) {
                logInfo("Chain terminated after response: {}", result.finalResult.reason);
            }
            break;

        case ProcedureStepResult::Action::WaitingExternal:
            // Procedure needs external data (RAND from AuC, VLR decision)
            handleWaitingExternal(session, orchestrator, result);
            break;

        case ProcedureStepResult::Action::Completed:
            logInfo("Procedure completed: {}", result.finalResult.reason);
            break;

        case ProcedureStepResult::Action::Failed:
            logWarning("Procedure failed: {}", result.finalResult.reason);
            break;
    }
}
```

### Step 3: Handle external data with typed structures

When a procedure enters `WaitingExternal` state, query the appropriate external system (AuC, VLR) and feed the result using strongly-typed structures.

```cpp
void handleWaitingExternal(SubscriberSession* session,
                           ProcedureOrchestrator& orchestrator,
                           const ProcedureStepResult& result) {
    auto* proc = orchestrator.activeProcedure();
    if (!proc) return;

    switch (proc->type()) {
        case procedure::ProcedureType::Authentication: {
            // Query AuC for RAND + expected SRES triplet
            auto triplet = aucQuery(session->context.identity().digits());

            AuthChallenge chal{};
            std::memcpy(chal.rand.data(), triplet.rand.data(), 16);
            std::memcpy(chal.expectedSres.data(), triplet.sres.data(), 4);

            auto feedResult = orchestrator.feedExternalTyped(chal);
            if (feedResult.action == ProcedureStepResult::Action::SendResponseWithToken) {
                uint8_t buf[512];
                int n = ResponseBuilder::buildResponseFromToken(
                    feedResult.responseToken, {buf, sizeof(buf)}, session);
                if (n > 0) sendToMS(session, buf, n);
            }
            break;
        }

        case procedure::ProcedureType::LocationUpdate: {
            // Query VLR for accept/reject decision
            auto vlrResult = vlrQuery(session->context.identity().digits());

            if (vlrResult.accept) {
                VLRDecision decision{true, vlrResult.newTmsi, MMRejectCause::Zero};
                auto feedResult = orchestrator.feedExternalTyped(decision);
                if (feedResult.action == ProcedureStepResult::Action::SendResponseWithToken) {
                    uint8_t buf[512];
                    int n = ResponseBuilder::buildResponseFromToken(
                        feedResult.responseToken, {buf, sizeof(buf)}, session);
                    if (n > 0) sendToMS(session, buf, n);
                }
            } else {
                VLRDecision decision{false, std::nullopt, vlrResult.cause};
                auto feedResult = orchestrator.feedExternalTyped(decision);
                if (feedResult.action == ProcedureStepResult::Action::SendResponseWithToken) {
                    uint8_t buf[512];
                    int n = ResponseBuilder::buildResponseFromToken(
                        feedResult.responseToken, {buf, sizeof(buf)}, session);
                    if (n > 0) sendToMS(session, buf, n);
                }
            }
            break;
        }

        default:
            logWarning("Unhandled WaitingExternal for procedure type 0x{:02X}",
                       static_cast<uint8_t>(proc->type()));
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

        // 2. Tick all session orchestrators and LAPDm timers.
        //    (LAPDm entities are per-link and app-owned; the orchestrator is
        //     app-owned per session — see Step 2.)
        registry.forEach([delta](SubscriberSession* sess) {
            size_t failed = orchestratorFor(sess).tickAll(delta);
            if (failed > 0) {
                logWarning("{} procedures timed out for session", failed);
            }
        });
        for (auto& link : activeLapdmLinks) {
            link.entity.tickT200(delta);
        }

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

## Typical BTS Procedures with ProcedureOrchestrator

### Location Update (Full Chain)

The orchestrator automatically chains: CMServiceRequest -> [Identity] -> Authentication -> CipheringMode -> LocationUpdate.

```cpp
// App-owned orchestrator for this session (see Step 2).
auto& orchestrator = orchestratorFor(session);

// 1. MS sends CMServiceRequest (Location Updating)
auto cmReq = parseL3(incomingData).value();
auto result = orchestrator.feed(cmReq, session);
// Orchestrator starts chain: CMServiceRequest phase
// Returns SendResponseWithToken + ResponseToken::CMServiceAccept

// Build and send CM Service Accept
uint8_t buf[512];
int n = ResponseBuilder::buildResponseFromToken(result.responseToken, {buf, sizeof(buf)}, session);
sendToMS(session, buf, n);

// 2. Orchestrator transitions to Authentication phase automatically
//    Returns WaitingExternal — need AuC data
AuthChallenge chal{};
std::memcpy(chal.rand.data(), aucRandBytes, 16);
std::memcpy(chal.expectedSres.data(), aucSresBytes, 4);
result = orchestrator.feedExternalTyped(chal);
// Returns SendResponseWithToken + ResponseToken::AuthenticationRequest
// (the procedure recorded RAND into session->response)

// 3. MS responds with AuthenticationResponse
auto authResp = parseL3(authResponseData).value();
result = orchestrator.feed(authResp, session);
// Orchestrator verifies SRES internally, transitions to CipheringMode

// 4. Ciphering phase
CipheringParameters cipher{1, true}; // A5/1 enabled
result = orchestrator.feedExternalTyped(cipher);
// Returns SendResponseWithToken + ResponseToken::CipheringModeCommand

// 5. MS sends CipheringModeComplete
auto cipherComplete = parseL3(cipherCompleteData).value();
result = orchestrator.feed(cipherComplete, session);
// Transitions to LocationUpdate phase, returns WaitingExternal — need VLR decision

// 6. VLR accepts
VLRDecision vlr{true, 0x87654321u, MMRejectCause::Zero};
result = orchestrator.feedExternalTyped(vlr);
// Returns SendResponseWithToken + ResponseToken::LocationUpdatingAccept
// (newTmsi recorded into session->response; action stays SendResponseWithToken
//  even though the chain terminates in this step — see finalResult.state)

// 7. Build and send Location Updating Accept
n = ResponseBuilder::buildResponseFromToken(result.responseToken, {buf, sizeof(buf)}, session);
sendToMS(session, buf, n);
// Chain completed successfully
```

### Call Setup MO (Full Chain)

The orchestrator chains: CMServiceRequest(MO_Call) -> CallSetupMO.

```cpp
auto& orchestrator = orchestratorFor(session);

// 1. MS sends CMServiceRequest (MO Call)
auto cmReq = parseL3(incomingData).value();
auto result = orchestrator.feed(cmReq, session);
// Returns SendResponseWithToken + ResponseToken::CMServiceAccept

// 2. MS sends Setup
auto setup = parseL3(setupData).value();
result = orchestrator.feed(setup, session);
// Returns SendResponseWithToken + ResponseToken::CallProceeding
// (TI from the Setup header is recorded into session->response.ti)

// 3. MS sends AssignmentComplete (after TCH assignment)
auto assignComplete = parseL3(assignCompleteData).value();
result = orchestrator.feed(assignComplete, session);
// Returns SendResponseWithToken + ResponseToken::Alerting

// 4. MS sends Connect
auto connect = parseL3(connectData).value();
result = orchestrator.feed(connect, session);
// Returns SendResponseWithToken + ResponseToken::Connect

// 5. MS sends ConnectAcknowledge
auto connAck = parseL3(connAckData).value();
result = orchestrator.feed(connAck, session);
// finalResult.state == Completed — call is active, speech path established
```

### Paging Procedure

For network-initiated paging:

```cpp
auto& orchestrator = orchestratorFor(session);

PagingTrigger trigger;
trigger.identity = L3MobileIdentity(0x12345678u); // TMSI
trigger.targetChannel = ChannelType::SDCCHType;

auto result = orchestrator.feedExternalTyped(trigger);
// Returns SendResponseWithToken + ResponseToken::PagingRequestType2
// (identity recorded into session->response — the page is built from it)

uint8_t buf[512];
int n = ResponseBuilder::buildResponseFromToken(result.responseToken, {buf, sizeof(buf)}, session);
broadcastPaging(buf, n); // Send on PAGCH
```

## External System Integration (feedExternalTyped)

Procedures receive data from external systems via `feedExternalTyped()`. The `ExternalData` variant holds strongly-typed structures.

**Session and ResponseContext:** the procedure-level signature is `feedExternalTyped(const ExternalData& data, SubscriberSession* session, ResponseSink sink = {})` — the session (nullable) lets the procedure record the parameters it learns into `session->response` (`ResponseContext`): the RAND from an `AuthChallenge`, the new TMSI / reject cause from a `VLRDecision`, the ciphering algorithm selector, the paging identity, the handover target channel, and so on. `ResponseBuilder::buildResponseFromToken()` then reads exactly those values, so responses are built from real parameters (and it returns -1 rather than fabricating a value when one is missing). `ProcedureOrchestrator` stores the session from `feed()` and forwards it automatically, so application code just calls `orchestrator.feedExternalTyped(data)` on the session's app-owned orchestrator; `ProcedureRunner::feedExternalTyped(type, session, data, sink)` takes the session explicitly.

**ResponseSink note:** the sink is an observability hook invoked only from `feed()` with the real incoming message. `feedExternalTyped()` never invokes it — the response on that path is signaled by the token in the result.

### AuC Integration (Authentication)

```cpp
void onAuthNeeded(SubscriberSession* session) {
    auto triplet = aucQuery(session->context.identity().digits());

    AuthChallenge chal{};
    std::memcpy(chal.rand.data(), triplet.rand.data(), 16);   // 128-bit RAND
    std::memcpy(chal.expectedSres.data(), triplet.sres.data(), 4); // 32-bit SRES

    auto& orchestrator = orchestratorFor(session);
    auto result = orchestrator.feedExternalTyped(chal);
    // Procedure sends AuthenticationRequest to MS via ResponseToken
    // (RAND is recorded into session->response.rand)
}
```

### VLR Integration (Location Update)

```cpp
void onLocationUpdateDecision(SubscriberSession* session, bool accept,
                               std::optional<uint32_t> newTmsi, MMRejectCause cause) {
    VLRDecision decision{accept, newTmsi, accept ? MMRejectCause::Zero : cause};

    auto& orchestrator = orchestratorFor(session);
    auto result = orchestrator.feedExternalTyped(decision);

    if (result.action == ProcedureStepResult::Action::SendResponseWithToken) {
        uint8_t buf[512];
        int n = ResponseBuilder::buildResponseFromToken(
            result.responseToken, {buf, sizeof(buf)}, session);
        if (n > 0) sendToMS(session, buf, n);
    }
}
```

### BSC Integration (Handover)

```cpp
void onHandoverDecision(SubscriberSession* session,
                         const L3ChannelDescription& target, const L3CellDescription& cell) {
    HandoverTarget ho{target, cell};

    auto& orchestrator = orchestratorFor(session);
    auto result = orchestrator.feedExternalTyped(ho);
    // Procedure sends HandoverCommand via ResponseToken::HandoverCommand
    // (target channel recorded into session->response.hoChannel)

    if (result.action == ProcedureStepResult::Action::SendResponseWithToken) {
        uint8_t buf[512];
        int n = ResponseBuilder::buildResponseFromToken(
            result.responseToken, {buf, sizeof(buf)}, session);
        if (n > 0) sendToMS(session, buf, n);
    }
}
```

## Abis/RSL Integration

When the BTS communicates with a BSC over the A-bis interface (TS 48.058), RSL messages wrap L3 payloads. Use `RSLParser` to extract L3 from inbound RSL, and `RSLBuilder` to encapsulate outbound L3.

### Inbound RSL (BSC -> BTS)

```cpp
void onRslMessage(std::span<const uint8_t> rslBytes) {
    auto parsed = RSLParser::parse(rslBytes);
    if (!parsed) return;

    auto l3Payload = RSLParser::extractL3(*parsed);
    if (!l3Payload) return; // Control message without L3 data

    auto msg = parseL3(*l3Payload);
    if (!msg) return;

    uint8_t chanNr = parsed->chanNr;
    uint8_t linkId = parsed->linkId;
    auto* session = registry.findByLink(/*trx from chanNr*/, /*ts from chanNr*/, linkId);
    if (!session) return;

    auto& orchestrator = orchestratorFor(session);
    auto result = orchestrator.feed(*msg, session);

    if (result.action == ProcedureStepResult::Action::SendResponseWithToken) {
        uint8_t l3Buf[512];
        int l3Len = ResponseBuilder::buildResponseFromToken(
            result.responseToken, {l3Buf, sizeof(l3Buf)}, session);
        if (l3Len > 0) {
            uint8_t rslBuf[1024];
            int rslLen = RSLBuilder::buildDataInd({rslBuf, sizeof(rslBuf)},
                session->channel.value().arfcn, session->lapdmLink,
                {l3Buf, static_cast<size_t>(l3Len)});
            if (rslLen > 0) sendToBsc(rslBuf, rslLen);
        }
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
            auto mode = RSLParser::getChannelMode(parsed);
            if (mode) {
                activateChannel(parsed.chanNr, *mode);
                uint8_t ackBuf[64];
                int n = RSLBuilder::buildChanActivAck({ackBuf, sizeof(ackBuf)},
                    parsed.chanNr, getCurrentFrameNumber());
                sendToBsc(ackBuf, n);
            }
            break;
        }

        case static_cast<uint8_t>(RSLDChanMessageType::RFChanRel): {
            releaseChannel(parsed.chanNr);
            uint8_t ackBuf[64];
            int n = RSLBuilder::buildRFChanRelAck({ackBuf, sizeof(ackBuf)}, parsed.chanNr);
            sendToBsc(ackBuf, n);
            break;
        }

        case static_cast<uint8_t>(RSLCChanMessageType::PagingCmd): {
            auto l3 = RSLParser::extractL3(parsed);
            if (l3) broadcastPaging(*l3);
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

Timers are managed at two levels: LAPDm T200 timer (per link) and L3 protocol timers (managed by ProcedureOrchestrator).

### Event Loop Integration

```cpp
void eventLoopTick(SubscriberSession* session, std::chrono::milliseconds delta) {
    // Advance LAPDm T200 timer (per-link entity, app-owned)
    bool retransmitted = lapdmEntityFor(session).tickT200(delta);

    // Advance procedure timers (managed by the app-owned orchestrator)
    size_t failed = orchestratorFor(session).tickAll(delta);
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

For orchestrator results:

```cpp
auto result = orchestratorFor(session).feed(msg, session);
if (result.action == ProcedureStepResult::Action::Failed) {
    logWarning("Procedure {} failed: {}",
        procedureTypeName(result.finalResult.type),
        result.finalResult.reason);
}
```

## Performance Considerations

- **Zero heap allocation on parse path**: `ParsedMessage` is a stack-allocated variant (416 bytes, static_assert < 8 KB)
- **MSContext 92 bytes**: Fits in L1 cache; millions of contexts fit in L3 cache
- **ProcedureStepResult ≤ 32 bytes**: Compact result with `ResponseToken` (uint8_t), no heap allocation
- **ResponseContext ≤ 160 bytes**: Fixed arrays, zero heap; single source of response parameters on the session
- **SubscriberSession < 4096 bytes**: All components stored inline
- **ResponseBuilder span overload**: Writes directly into caller's Arena buffer, zero heap cost
- **ResponseToken pattern**: Procedure returns token (1 byte); caller builds response in pre-allocated buffer
- **TypedExternalData**: Small structures (≤ 64 bytes) passed by const reference — no copy overhead
- **ProcedureOrchestrator**: No `std::vector<ParsedMessage>` storage; stores only last ResponseToken
- **RSLParser**: Fixed-size IE array (32 max), all pointers into original buffer — zero heap
- **TimerManager**: Fixed-size `std::array`, no dynamic allocation; optional zero-alloc active-change observer (`setOnActiveChange`) lets `SubscriberRegistry` track which sessions have running timers so `tickAllTimers()` is O(active)
- **Thread safety**: Each MS session is accessed from one thread. `ShardedChannelPool` and `ShardedSubscriberRegistry` provide thread-safe variants for multi-threaded scenarios.

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
| `stack/state_machine.h` | `RR/MM/CCStateMachine` | Protocol FSM skeletons |
| `stack/channel_pool.h` | `ChannelPool`, `decodeChannelNeeded()` | Channel allocation, VEA |
| `stack/response_builder.h` | `ResponseBuilder`, `buildResponseFromToken()`, `buildSetupZeroAlloc()` | Factory for L3 response messages (parameters from `ResponseContext`) |
| `stack/response_context.h` | `ResponseContext` | Per-session response parameters (RAND, TI, channel, identity, ...), populated by the active procedure |
| `stack/response_sink.h` | `ResponseSink`, `makeResponseSink()` | Zero-overhead response callback (fn+ctx, 16 bytes, refcounted captures) |
| `stack/procedure.h` | `Procedure`, `ProcedureStepResult`, `ResponseToken` | Base class for protocol procedures |
| `stack/typed_external_data.h` | `ExternalData`, `AuthChallenge`, `VLRDecision`, `PagingTrigger`, etc. | Type-safe external data structures |
| `stack/procedure_runner.h` | `ProcedureRunner`, `ProcedureFactory` | Concurrent procedure management |
| `stack/procedure_orchestrator.h` | `ProcedureOrchestrator` | Auto-chained compound procedures |
| `stack/procedure_state_mixin.h` | `ProcedureStateMixin<Derived, State>` | CRTP mixin for common procedure code |
| `stack/subscriber_registry.h` | `SubscriberSession`, `SubscriberRegistry` | Per-MS session management |
| `abis/rsl_types.h` | `RSLDiscriminator`, `RSL_IE`, `RSLChannelNumber` | A-bis RSL type definitions |
| `abis/rsl_parser.h` | `RSLParser`, `RSLParsedMessage` | Parse RSL messages, extract L3 |
| `abis/rsl_builder.h` | `RSLBuilder` | Construct RSL messages for BSC |

## See Also

- [README.md](../README.md) - Library overview and quick start
- [doc/API.md](API.md) - Full API reference
- [doc/bts_architecture.md](bts_architecture.md) - Architecture overview, two usage modes, and scaling guide
- `examples/` directory - Working BTS example programs
