# Library Boundaries — What libgsml3parser Does and Does Not Do

This document defines the explicit scope of libgsml3parser. It is intentional what is excluded, and the library provides well-defined integration points for each excluded domain so that a BTS developer can connect external systems without modifying the library.

## Included in the Library

The following capabilities are part of libgsml3parser and maintained by this project:

| Domain | Components | Specification |
|--------|-----------|---------------|
| **L3 Parsing / Serialization** | `parseL3()`, `writeL3Bytes()`, Builder API (200+ message types) | 3GPP TS 24.008, TS 24.008 Annex B, GSM 04.08 |
| **LAPDm Framing** | `LAPDmEntity`, `lapdm::wrapL3()`, `lapdm::unwrapL3()` | GSM 04.06 / 3GPP TS 24.022 |
| **Protocol Procedures (FSM)** | `ProcedureOrchestrator`, `ProcedureRunner`, 10 concrete procedures (LocationUpdate, Authentication, CallSetupMO/MT, ChannelAssignment, CipheringMode, Paging, Handover, CallRelease, IMSIDetach) | TS 24.008 §§ 4.4, 6.1 |
| **Protocol State Machines** | `RRStateMachine`, `MMStateMachine`, `CCStateMachine` | TS 24.008 protocol state model |
| **L3 Timers** | `TimerManager`, `L3TimerId` (T3101–T3395) | TS 24.008 timer definitions |
| **Transaction Management** | `TransactionManager` (request-response correlation by TI/transaction ID) | TS 24.008 transaction model |
| **Subscriber Management** | `SubscriberSession`, `MSContext`, `SubscriberRegistry`, `ShardedSubscriberRegistry<N>` | Per-MS inline state (< 4 KB/session) |
| **Response Building** | `ResponseBuilder` (static methods, zero-allocation span overloads), `ResponseToken` enum | TS 24.008 message construction |
| **Typed External Data** | `ExternalData` variant (`AuthChallenge`, `VLRDecision`, `PagingTrigger`, `CipheringParameters`, `HandoverTarget`) | Type-safe integration API |
| **Channel Pool** | `ChannelPool`, `ShardedChannelPool<N>` | Logical channel allocation/release |
| **A-bis RSL Parsing / Building** | `RSLParser`, `RSLBuilder` (DCHAN, RLL messages, MEAS_RES, ACK/NACK) | 3GPP TS 48.058 |
| **Bit-Level I/O** | `BitReader`, `BitWriter` | Bit-exact GSM encoding |
| **Streaming Parse** | `L3Framer`, `L3StreamProcessor`, `ByteSource` | Incremental parse from byte streams |
| **Protocol Dispatcher** | `ProtocolDispatcher`, `FlatHandler` | O(1) PD+MTI callback routing |

## Excluded from the Library (Intentionally)

The following domains are deliberately not included. Each has a well-defined integration point so that BTS developers can connect their own implementations.

### PHY / SDR Backend

**Not included:** Radio transmission/reception, timeslot management, frequency hopping, power control, frame synchronization, baseband processing.

**Reason:** PHY backends vary widely (GNU Radio, srsRAN, Limesuite, hardware-specific drivers). The library cannot abstract over all of them and would become a maintenance burden.

**Integration point:** The BTS application provides two callbacks:

```cpp
// Transmit: send L3 bytes to radio
void sendToRadio(std::span<const uint8_t> lapdmFrame, ChannelType type, uint8_t trx, uint8_t ts);

// Receive: incoming frame from radio
void onRadioFrameReceived(std::span<const uint8_t> lapdmFrame, ChannelType type, uint8_t trx, uint8_t ts);
```

The library produces LAPDm-framed bytes via `ResponseBuilder` + `lapdm::wrapL3()`. The BTS application passes these to its PHY transmit path. Incoming frames from the PHY are unwrapped with `lapdm::unwrapL3()` and parsed with `parseL3()`, then fed to `ProcedureOrchestrator::feed()`.

### Speech Codecs (AMR, FR, HR)

**Not included:** Audio encoding/decoding for TCH traffic channels.

**Reason:** Codec implementations are large, patent-encumbered, and have their own ecosystems (e.g., Opus, AMR-NB libraries).

**Integration point:** Once a call reaches the connected state (`ResponseToken::Connect`), the BTS application switches to its codec pipeline. The library is no longer involved in TCH data flow; it only manages call control signaling (disconnect, release).

### Ciphering Algorithms (A5/1, A5/2, A5/3)

**Not included:** Stream cipher implementations for user data encryption.

**Reason:** A5/1 and A5/2 have export restrictions in some jurisdictions. A5/3 requires external cryptographic libraries. The library needs only to signal when ciphering starts.

**Integration point:** After the procedure returns `ResponseToken::CipheringModeCommand` and the MS responds with `CipheringModeComplete`, the BTS application enables its A5/XOR engine at the L2 level on the affected logical channels. The library does not participate in the actual encryption/decryption of subsequent frames.

```cpp
// After CipheringModeComplete is received:
// BTS application enables ciphering at L2:
phyBackend.enableCiphering(channel, algorithmSelector, kcKey);
```

### OML / Network Management

**Not included:** Operation & Maintenance Link (OML) messages, performance monitoring, fault management, configuration management over A-bis.

**Reason:** OML is BSC-specific and not standardized in a way that a single library can cover all deployments.

**Integration point:** The BTS application handles OML independently. The library's `RSLParser`/`RSLBuilder` only covers RLL (Radio Link Layer) and DCHAN (Dedicated Channel Assignment) messages, not OML PDUs.

### SIP / Media Gateway Integration

**Not included:** SIP signaling, RTP/RTCP media streaming, media gateway control (H.248/Megaco).

**Reason:** These are core network interfaces that vary by MSC/VLR implementation and are outside the Um interface scope.

**Integration point:** The BTS application bridges call control state from the library to its SIP stack:

```cpp
// Library signals call connected via ResponseToken::Connect
// BTS application creates SIP INVITE to MSC:
sipStack.sendInvite(bearerCapability, calledPartyNumber, ti);
```

### GPRS / Packet Switched Full Stack

**Not included:** PCU logic, SGSN interface (Gb protocol), LLC/SNDCP, PDP context management.

**Reason:** GPRS introduces a parallel packet-switched control plane that is architecturally separate from the circuit-switched L3 stack covered by this library.

**Integration point:** The BTS application handles GPRS on separate timeslots and channels. The library's `ChannelPool` can allocate PACCH resources, but all GPRS-specific signaling is handled externally.

### Configuration Management

**Not included:** Cell configuration (BCCH frequency, BSIC, power levels), neighbor cell lists, system information templates.

**Reason:** Configuration formats and sources vary (static files, HLR download, OML push).

**Integration point:** The BTS application loads configuration at startup and provides it to the library through typed parameters:

```cpp
// System Information construction uses caller-provided parameters:
auto sysInfo = RRMessageBuilder::buildSystemInformationType1(
    cellIdentity, rachControlParams, cellDescription, ...);
```

### Logging Infrastructure

**Not included:** Structured logging, log rotation, log levels, output destinations.

**Reason:** Logging frameworks are application-level concerns with many mature options (spdlog, glog, slog).

**Integration point:** The library is log-free. Procedure state transitions and timer events are returned as structured `ProcedureStepResult` objects. The BTS application can log these results using its preferred framework:

```cpp
auto result = orchestrator.feed(msg, session);
logger.info("procedure {} -> action {}, token {}",
    procedureName(result), actionName(result.action),
    tokenName(result.responseToken));
```

## Summary

| Domain | In Library? | Integration |
|--------|------------|-------------|
| L3 parse / serialize | Yes | Core API |
| LAPDm framing | Yes | `LAPDmEntity` |
| Signal procedures (FSM) | Yes | `ProcedureOrchestrator` |
| Subscriber state | Yes | `SubscriberSession` + Registry |
| Response building | Yes | `ResponseBuilder` + Arena |
| A-bis RSL (RLL/DCHAN) | Yes | `RSLParser` / `RSLBuilder` |
| PHY / SDR | No | `sendToRadio()` / `onRadioFrameReceived()` callbacks |
| Speech codecs | No | After `Connect`, app switches to codec pipeline |
| Ciphering (A5) | No | After `CipheringModeComplete`, app enables A5 at L2 |
| OML / Network Mgmt | No | App handles independently |
| SIP / Media Gateway | No | App bridges library call state to SIP stack |
| GPRS / PS stack | No | Separate timeslots, app handles LLC/SNDCP |
| Configuration | No | App provides typed parameters at startup |
| Logging | No | App logs `ProcedureStepResult` with its framework |
