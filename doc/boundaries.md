# Library Boundaries — What libgsml3parser Does and Does Not Do

This document defines the explicit scope of libgsml3parser. Exclusions are deliberate, and the library provides well-defined integration points for each excluded domain so that a BTS developer can connect external systems without modifying the library.

## Included in the Library

The following capabilities are part of libgsml3parser and maintained by this project:

| Domain | Components | Specification |
|--------|-----------|---------------|
| **L3 Parsing / Serialization** | `parseL3()`/`parseL3Hex()`, `writeL3()`/`writeL3Bytes()`/`writeL3Hex()`, fluent Builder API (236 message types, all 12 PD domains) | TS 24.008 / GSM 04.08 |
| **LAPDm Protocol** | `lapdm::LAPDmFrame` decode/encode + factories (`makeUIFrame`, `makeSABMEFrame`, ...), `LAPDmEntity` full state machine (SABME/UA/DISC, UI, I-frame segmentation k=1 with T200 retransmission, 4 KB-bounded reassembly, contention resolution) | GSM 04.06 / TS 45.006 |
| **Protocol Procedures (FSM)** | `ProcedureOrchestrator` (auto-chains), `ProcedureRunner`, 10 concrete procedures (LocationUpdate, Authentication, CallSetupMO/MT, ChannelAssignment, CipheringMode, Paging, Handover, CallRelease, IMSIDetach) | TS 24.008 §§ 4.4, 6.1; TS 04.08 § 9.1 |
| **Protocol State Machines** | `RRStateMachine`, `MMStateMachine`, `CCStateMachine` skeletons with `SMResult` transitions | TS 24.008 protocol state model |
| **L3 Timers** | `TimerManager`/`L3Timer` (T3101–T3395, ≤32 concurrent per MS, zero-alloc tick) | TS 24.008 10.5 |
| **Transaction Management** | `TransactionManager` (request-response correlation; O(1) TI index for CC/SS, PD+MTI match for the rest, 16 pending slots) | TS 24.008 transaction model |
| **Subscriber Management** | `SubscriberSession` (2,056 B inline state), `MSContext`, `SubscriberRegistry`, `ShardedSubscriberRegistry<N>` (flat open-addressing TMSI/link indexes, O(active) ticks) | Per-MS state, < 4 KB/session bound |
| **Response Building** | `ResponseBuilder` static factories (vector + zero-alloc span overloads), `ResponseToken` → `buildResponseFromToken(token, buf, session)`, per-session `ResponseContext` | TS 24.008 / GSM 04.08 message formats |
| **Typed External Data** | `ExternalData` variant (`AuthChallenge`, `VLRDecision`, `PagingTrigger`, `CipheringParameters`, `HandoverTarget`) via `feedExternalTyped()` | Type-safe AuC/VLR/BSC integration API |
| **Channel Pool** | `ChannelPool`, `ShardedChannelPool<N>` (logical channel allocation/release, RA decoding, VEA) | GSM 04.08 9.1 / 05.08 |
| **A-bis RSL Parsing / Building** | `RSLParser` (zero-copy IE/L3 extraction; RLL, CCHAN, DCHAN, TRX and IPAccess discriminators), `RSLBuilder` (13 frame builders with span overloads: 11 BTS→BSC plus DATA_REQ/UNIT_DATA_REQ for BSC→BTS loopback) | TS 48.058 |
| **Bit-Level I/O & Streaming** | `BitReader`/`BitWriter`; `ByteSource` (Span/File/RingBuffer), `L3Framer`, `L3StreamProcessor`, zero-copy `InlineFramer`/`ZeroCopyStreamProcessor` | Bit-exact GSM encoding |
| **Protocol Dispatcher** | `ProtocolDispatcher` + `FlatHandler` (16×136 O(1) handler table, domain/fallback/TI handlers) | Callback routing |
| **C ABI** | Stable C89 header `gsml3parser_c.h` over the full stack (parse/serialize, typed accessors/builders, RSL, LAPDm, registry/orchestrator) for FFI | — |

## Excluded from the Library (Intentionally)

The following domains are deliberately not included. Each has a well-defined integration point so that BTS developers can connect their own implementations.

### PHY / SDR Backend

**Not included:** Radio transmission/reception, timeslot management, frequency hopping, power control, frame synchronization, baseband processing.

**Reason:** PHY backends vary widely (GNU Radio, srsRAN, Limesuite, hardware-specific drivers). The library cannot abstract over all of them and would become a maintenance burden.

**Integration point:** The BTS application owns its radio transport and connects it at the LAPDm frame boundary:

```cpp
// App-owned radio interface (illustrative signatures)
void sendToRadio(std::span<const uint8_t> lapdmFrame, ChannelType type, uint8_t trx, uint8_t ts);
void onRadioFrameReceived(std::span<const uint8_t> lapdmFrame, ChannelType type, uint8_t trx, uint8_t ts);
```

TX: `ResponseBuilder` writes complete L3 bytes into a caller buffer; the app frames them — `LAPDmEntity.sendUI()`/`sendData()` on dedicated channels (the entity's TX span is reused per send: transmit or copy synchronously inside the `L1TransmitFn` callback), `lapdm::makeUIFrame()` + `encodeFrame()` for BCCH/PAGCH broadcasts — and passes the result to `sendToRadio()`. RX: incoming frames are handed to `LAPDmEntity.receiveFrame()`, whose `L3ReceiveFn` callback delivers complete L3 messages that go through `parseL3()` and then `ProcedureOrchestrator::feed()`. Headerless RACH bursts (1-byte Channel Request) parse directly with `parseL3()`.

### Speech Codecs (AMR, FR, HR)

**Not included:** Audio encoding/decoding for TCH traffic channels.

**Reason:** Codec implementations are large, patent-encumbered, and have their own ecosystems.

**Integration point:** Once the call reaches the active state (`CallSetupMOPercedure`/`CallSetupMTProcedure` complete with `"call_active"`, i.e. after the ConnectAcknowledge exchange), the BTS application switches the TCH timeslots to its codec pipeline. The library no longer participates in TCH user-data flow; it only manages subsequent call-control signaling (Disconnect/Release).

### Ciphering Algorithms (A5/1, A5/2, A5/3)

**Not included:** Stream cipher implementations for user data encryption.

**Reason:** A5/1 and A5/2 carry export restrictions in some jurisdictions; A5/3 requires external cryptographic libraries. The library only signals when ciphering starts.

**Integration point:** `CipheringModeProcedure` (and the orchestrator's CipheringMode phase) issues `ResponseToken::CipheringModeCommand` from `feedExternalTyped(CipheringParameters{algorithmSelector, enableCiphering})` and completes on the MS's `CipheringModeComplete`. After that event the BTS application enables its A5 engine at the L2 level on the affected logical channels with the same algorithm selector (key material is the app's concern). The library does not encrypt or decrypt subsequent frames.

### OML / Network Management

**Not included:** Operation & Maintenance Link (OML) messages, performance monitoring, fault management, and configuration management over A-bis.

**Reason:** OML is BSC-specific and varies by deployment; a single library cannot cover it all.

**Integration point:** The BTS application handles OML independently. The RSL support covers the RLL, CCHAN, DCHAN, TRX and IPAccess discriminators (parse + 13 frame builders) — OML PDUs are out of scope for `RSLParser`/`RSLBuilder`.

### SIP / Media Gateway Integration

**Not included:** SIP signaling, RTP/RTCP media streaming, media gateway control (H.248/Megaco).

**Reason:** These are core network interfaces that vary by MSC/VLR implementation and are outside the Um interface scope.

**Integration point:** The BTS application bridges library call state to its SIP stack when a call becomes active:

```cpp
// When CallSetupMO/MT reports finalResult "call_active":
// build the INVITE from session->response (calledNumber, TI) / MSContext and hand
// over media on the TCH timeslot.
sipStack.sendInvite(calledNumberDigits, bearerCapability);
```

### GPRS / Packet-Switched Full Stack

**Not included:** PCU logic, SGSN interface (Gb protocol), LLC/SNDCP, PDP context management beyond L3 message parsing.

**Reason:** GPRS is a parallel packet-switched control plane, architecturally separate from the circuit-switched BTS stack this library implements end to end.

**Integration point:** GMM/SM messages parse and build like any other L3 message (29 SM + 23 GMM types). The `ChannelPool` can hold PDTCH channels (including `allocateVEA()`), but the application owns all GPRS-specific processing above the radio interface.

### Configuration Management

**Not included:** Cell configuration sources (BCCH frequency, BSIC, power levels), neighbor cell lists, system information template storage.

**Reason:** Configuration formats and sources vary (static files, HLR download, OML push).

**Integration point:** The BTS application loads configuration at startup and fills the builder parameters for broadcast messages:

```cpp
auto si1 = L3SystemInformationType1::builder()
    .cellSelectionParameters(params)
    // ... per-field setters for the cell's actual configuration ...
    .build();
ParsedMessage pm{RRM{std::move(si1)}};
auto bytes = writeL3Bytes(pm);   // broadcast via LAPDm UI frames on BCCH
```

### Logging Infrastructure

**Not included:** Structured logging, log rotation, log level routing, output destinations. The only hook is the `ParserConfig` log level (default WARNING).

**Reason:** Logging frameworks are an application-level concern with many mature options (spdlog, glog, ...).

**Integration point:** Procedures and the orchestrator return structured results — log them in the application:

```cpp
auto result = orchestrator.feed(msg, session);
std::string line = std::format("chain {} action={} token={} final={}",
    gsml3parser::procedure::procedureTypeName(result.finalResult.type),
    static_cast<unsigned>(result.action),
    static_cast<unsigned>(result.responseToken),
    result.finalResult.reason);
// ship `line` to your logger
```

## Summary

| Domain | In Library? | Integration |
|--------|------------|-------------|
| L3 parse / serialize | Yes | Core API (236 message types, 12 PD domains) |
| LAPDm framing | Yes | `LAPDmEntity` + `lapdm::` frame API |
| Signal procedures (FSM) | Yes | `ProcedureOrchestrator` / `ProcedureRunner` |
| Subscriber state | Yes | `SubscriberSession` (+ sharded) registry |
| Response building | Yes | `ResponseBuilder` + caller buffers (Arena optional) |
| A-bis RSL | Yes | `RSLParser` / `RSLBuilder` (RLL, CCHAN, DCHAN, TRX, IPAccess) |
| PHY / SDR | No | App callbacks at the LAPDm frame boundary (`L1TransmitFn`/`L3ReceiveFn`) |
| Speech codecs | No | After `"call_active"`, app owns TCH media path |
| Ciphering (A5) | No | After `CipheringModeComplete`, app enables A5 with the same selector |
| OML / Network Mgmt | No | App handles independently; RSL support excludes OML PDUs |
| SIP / Media Gateway | No | App bridges call-active state to its SIP stack |
| GPRS / PS stack | No | L3 messages only; app owns LLC/SNDCP and beyond |
| Configuration | No | App fills builder parameters from its config source |
| Logging | No | App logs the structured `ProcedureStepResult` values |
