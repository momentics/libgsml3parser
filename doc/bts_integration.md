# BTS Integration Guide

This guide explains how to integrate libgsml3parser into a software Base Transceiver Station (BTS) implementation. It covers the full message lifecycle: building L3 messages, framing them in LAPDm, dispatching incoming messages, and handling typical BTS scenarios.

## Architecture Overview

libgsml3parser sits between the BTS application logic and the physical/radio layer. The library provides three core capabilities:

```
┌──────────────────────────────────────────────────────┐
│                  BTS Application                      │
│  (paging logic, channel management, call control)    │
└────────────────┬─────────────────────┬───────────────┘
                 │                     │
     ┌───────────▼──────────┐ ┌───────▼──────────────┐
     │   Builder API        │ │  ProtocolDispatcher   │
     │   (construct L3)     │ │  (route incoming)     │
     └───────────┬──────────┘ └───────┬──────────────┘
                 │                     │
     ┌───────────▼──────────┐ ┌───────▼──────────────┐
     │   writeL3Bytes()     │ │  parseL3()           │
     │   (C++ → raw bytes)  │ │  (raw bytes → C++)   │
     └───────────┬──────────┘ └───────┬──────────────┘
                 │                     │
     ┌───────────▼──────────┐ ┌───────▼──────────────┐
     │   lapdm::wrapL3()    │ │  lapdm::unwrapL3()   │
     │   (add L2 headers)   │ │  (strip L2 headers)  │
     └───────────┬──────────┘ └───────┬──────────────┘
                 │                     │
                 ▼                     ▼
           ┌─────────────────────────────────┐
           │      Radio / Um Interface        │
           │   (air interface, L1/PHY)        │
           └─────────────────────────────────┘
```

### Outbound Flow (BTS → MS)

1. **Build** — Create an L3 message using the Builder API
2. **Serialize** — Convert to raw bytes with `writeL3Bytes()`
3. **Frame** — Wrap in LAPDm header with `lapdm::wrapL3()`
4. **Transmit** — Send frame bytes to the radio layer

### Inbound Flow (MS → BTS)

1. **Receive** — Get raw bytes from the radio layer
2. **Unframe** — Extract L3 payload with `lapdm::unwrapL3()`
3. **Parse** — Convert to typed C++ object with `parseL3()`
4. **Dispatch** — Route to handler via `ProtocolDispatcher`

## Building and Sending an L3 Message

### Includes

```cpp
#include <gsml3parser/gsml3parser.hpp>
#include <gsml3parser/lapdm.h>

using namespace gsml3parser;
using namespace gsml3parser::lapdm;
```

### Step-by-step: Channel Release Example

```cpp
// 1. Build the L3 message using the Builder pattern
auto channelRelease = L3ChannelRelease::builder()
    .cause(RRCause::Normal_Event)
    .build();

// 2. Wrap in ParsedMessage (variant type for serialization)
ParsedMessage pm{RRM{std::move(channelRelease)}};

// 3. Serialize to raw L3 bytes
auto l3Bytes = writeL3Bytes(pm);
if (!l3Bytes) {
    // Handle serialization error
    return;
}

// 4. Wrap in LAPDm frame (SAPI0 for signaling, command=0 for downlink)
std::vector<uint8_t> lapdmFrame = wrapL3(*l3Bytes, SAPI::SAPI0, false);

// 5. Send to radio layer
radioLayer.send(lapdmFrame.data(), lapdmFrame.size());
```

The `lapdmFrame` contains: `[address_byte][control_byte][l3_bytes...]`. For the Channel Release above this would be `[0x01][0x03][0x60][0x0D][0x00]`.

### Builder API Pattern

Every L3 message type that has fields provides a Builder:

```cpp
auto msg = MessageType::builder()
    .field1(value1)
    .field2(value2)
    // ... chainable setters return Builder&
    .build();  // returns MessageType by value
```

Empty messages (no fields) still have a Builder for API consistency:

```cpp
auto msg = L3CipheringModeComplete::builder().build();
```

### Serialization Options

| Function | Returns | Use Case |
|----------|---------|----------|
| `writeL3Bytes(msg)` | `Expected<std::vector<uint8_t>>` | Raw bytes for LAPDm framing (recommended for BTS) |
| `writeL3Hex(msg)` | `Expected<std::string>` | Hex string for logging and debugging |

## Receiving and Processing Incoming Messages

### Basic Parse Flow

```cpp
// Raw bytes received from radio layer
std::vector<uint8_t> incomingFrame = {/* from radio */};

// 1. Unwrap LAPDm header
auto l3Payload = unwrapL3(incomingFrame);
if (!l3Payload) {
    // Handle framing error
    return;
}

// 2. Parse L3 message
auto msg = parseL3(*l3Payload);
if (!msg) {
    // Handle parse error
    return;
}

// 3. Access typed message
if (auto* paging = tryGet<L3PagingResponse>(*msg)) {
    // Handle Paging Response
}
```

### Using ProtocolDispatcher

For production BTS code, use `ProtocolDispatcher` to route messages:

```cpp
class BtsChannelContext {
public:
    void onChannelRelease(const ParsedMessage& msg);
    void onPagingResponse(const ParsedMessage& msg);
    void onMeasurementReport(const ParsedMessage& msg);
    void onUnknown(const ParsedMessage& msg);
};

void setupDispatcher(ProtocolDispatcher& disp, BtsChannelContext* ctx) {
    // Specific handlers (checked first)
    disp.registerHandler(L3PD::RadioResource, L3ChannelRelease::MTI,
        [ctx](const ParsedMessage& msg, void*) {
            ctx->onChannelRelease(msg);
        });

    disp.registerHandler(L3PD::RadioResource, L3PagingResponse::MTI,
        [ctx](const ParsedMessage& msg, void*) {
            ctx->onPagingResponse(msg);
        });

    // Domain fallback (checked second)
    disp.registerDomainHandler(L3PD::RadioResource,
        [ctx](const ParsedMessage& msg, void*) {
            ctx->onUnknown(msg);
        });

    // Global fallback (checked last)
    disp.setFallbackHandler([](const ParsedMessage& msg, void*) {
        // Log unexpected PD domain
    });
}

// Dispatch incoming frame
void handleIncomingFrame(std::span<const uint8_t> frame, BtsChannelContext* ctx) {
    auto payload = unwrapL3(frame);
    if (!payload) return;

    ProtocolDispatcher dispatcher;
    setupDispatcher(dispatcher, ctx);
    dispatcher.dispatchRaw(*payload);
}
```

### Dispatch Priority

The dispatcher checks handlers in order:

1. **Specific handler** — Exact PD + MTI match (highest priority)
2. **Domain handler** — PD match only
3. **Fallback handler** — Global catch-all

## Typical BTS Patterns

### Paging a Mobile Station

Send a Paging Request Type 2 to page a mobile by TMSI:

```cpp
auto paging = L3PagingRequestType2::builder()
    .addTMSI(0x12345678, ChannelType::SDCCHType)
    .build();

ParsedMessage pm{RRM{std::move(paging)}};
auto bytes = writeL3Bytes(pm);
auto frame = wrapL3(*bytes, SAPI::SAPI0, false);
// Send frame to radio layer for BCCH/PAGCH broadcast
```

### Channel Assignment (RACH → Immediate Assignment)

Handle a Channel Request and respond with an Immediate Assignment:

```cpp
// Parse incoming Channel Request from RACH
auto msg = parseL3(channelRequestBytes);
auto* cr = tryGet<L3ChannelRequest>(*msg);

// Build Immediate Assignment response
auto assignment = L3ImmediateAssignment::builder()
    .pageMode(L3PageMode(0))
    .requestReference(cr->requestReference())
    .channelDescription(L3ChannelDescription(TDMA_SDCCH, 0, assignedTimeslot, 100))
    .timingAdvance(L3TimingAdvance(calculatedTA))
    .build();

ParsedMessage pm{RRM{std::move(assignment)}};
auto bytes = writeL3Bytes(pm);
auto frame = wrapL3(*bytes, SAPI::SAPI0, false);
// Send frame on AGCH
```

### System Information Broadcast

Build and broadcast System Information messages on BCCH:

```cpp
// SI Type 1 — Cell access parameters
auto si1 = L3SystemInformationType1::builder()
    .cellChannelDescription(L3FrequencyList(/* freq list */))
    .rachControlParameters(L3RACHControlParameters())
    .build();

// SI Type 3 — Full cell description (most important for MS camp-on)
auto si3 = L3SystemInformationType3::builder()
    .cellIdentity(L3CellIdentity(0x1234))
    .locationAreaIdentity(L3LocationAreaIdentity("250", "01", 0x5678))
    .controlChannelDescription(L3ControlChannelDescription(/* params */))
    .cellOptions(L3CellOptionsBCCH{})
    .cellSelectionParameters(L3CellSelectionParameters{})
    .rachControlParameters(L3RACHControlParameters{})
    .build();

// Serialize each and broadcast on BCCH
void broadcastSI(const auto& siMsg) {
    ParsedMessage pm{RRM{siMsg}};
    auto bytes = writeL3Bytes(pm);
    auto frame = wrapL3(*bytes, SAPI::SAPI0, false);
    bcchTransmitter.send(frame);
}

broadcastSI(si1);
broadcastSI(si3);
```

### Measurement Report Processing

Handle uplink measurement reports for handover decisions:

```cpp
disp.registerHandler(L3PD::RadioResource, L3MeasurementReport::MTI,
    [](const ParsedMessage& msg, void* ctx) {
        auto* report = tryGet<L3MeasurementReport>(msg);
        if (report) {
            // Evaluate serving cell signal level
            auto rxLev = report->currentRxlev();

            // Evaluate neighbor cells
            for (const auto& neighbor : report->measuredResults()) {
                // Handover decision logic...
            }
        }
    });
```

## Full Pipeline Example

Complete round-trip: build → serialize → frame → unframe → parse → verify:

```cpp
// 1. Build
auto paging = L3PagingRequestType2::builder()
    .addTMSI(0xDEADBEEF, ChannelType::SDCCHType)
    .build();

// 2. Serialize
ParsedMessage pm{RRM{std::move(paging)}};
auto l3Bytes = writeL3Bytes(pm);

// 3. Frame
auto frame = wrapL3(*l3Bytes, SAPI::SAPI0, false);

// --- Simulate transmission over the air ---

// 4. Unframe
auto payload = unwrapL3(frame);

// 5. Parse
auto reparsed = parseL3(*payload);

// 6. Verify
auto* paged = tryGet<L3PagingRequestType2>(*reparsed);
assert(paged && paged->tmsis()[0] == 0xDEADBEEFu);
```

## Integration with Existing Stacks

### osmo-bts

libgsml3parser can replace or complement the TRX layer message handling in osmo-bts:

- Use `parseL3()` to decode incoming Um interface messages instead of the built-in decoder
- Use Builder API + `writeL3Bytes()` + `wrapL3()` to generate outbound messages
- The LAPDm framing layer is compatible with osmo-bts' LAPDm implementation

### OpenBTS / srsRAN

For projects that build their own L3 stack:

- Drop in libgsml3parser as the L3 parse/generate library
- Link against the static library (`libgsml3parser.a`)
- Use `ProtocolDispatcher` as the message routing backbone

### Custom SDR-based BTS

If building from scratch with GNU Radio or similar:

1. Receive IQ samples → decode GSM burst → extract LAPDm PDU
2. Call `unwrapL3()` to get L3 bytes → `parseL3()` to get typed message
3. Use `ProtocolDispatcher` to route to your application handlers
4. Build response messages with Builder API → `writeL3Bytes()` → `wrapL3()` → encode burst

## Error Handling in Production

Always check `Expected<T>` results:

```cpp
auto msg = parseL3(data);
if (!msg) {
    auto& err = msg.error();
    logError("Parse failed: code=%d bit=%d msg=%s",
        static_cast<int>(err.code), err.bitPosition, err.message.c_str());
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

## Performance Considerations

- **Zero heap allocation on parse path**: `ParsedMessage` is a stack-allocated variant (< 8 KB)
- **Thread safety**: Each thread should use its own `ParserConfig` and `ProtocolDispatcher`
- **Arena allocator**: Use `Arena` for high-throughput batch parsing to reduce malloc pressure
- **Streaming**: Use `L3StreamProcessor` with `RingBuffer` for continuous frame processing

## API Reference Summary

| Module | Key Functions | Purpose |
|--------|--------------|---------|
| `parser.h` | `parseL3()`, `parseL3Hex()`, `writeL3Bytes()`, `writeL3Hex()` | Parse and serialize L3 messages |
| `lapdm.h` | `wrapL3()`, `unwrapL3()`, `makeAddress()`, `extractSAPI()` | LAPDm framing (GSM 04.06) |
| `dispatcher.h` | `ProtocolDispatcher::registerHandler()`, `dispatch()`, `dispatchRaw()` | Message routing |
| `visitor.h` | `tryGet<T>()`, `messageName()`, `messagePD()`, `messageMTI()` | Type access and metadata |
| Builder API | `MessageType::builder().field(v).build()` | Construct L3 messages |

## See Also

- [README.md](../README.md) — Library overview and quick start
- [doc/builder_coverage.md](builder_coverage.md) — Complete Builder coverage table
- `examples/` directory — Working BTS example programs
