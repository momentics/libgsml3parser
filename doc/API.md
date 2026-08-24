# libgsml3parser - Full API Reference

> Version 0.15.0 | C++20 | Thread-safe | Zero heap allocation on hot path | No external dependencies

## Table of Contents

1. [Getting Started](#1-getting-started)
2. [Core Types](#2-core-types)
3. [Bit-Level I/O](#3-bit-level-io)
4. [Parser Configuration](#4-parser-configuration)
5. [L3 Header Parsing](#5-l3-header-parsing)
6. [Message Variants](#6-message-variants)
7. [Parser and Serializer API](#7-parser-and-serializer-api)
8. [Visitor Helpers](#8-visitor-helpers)
9. [Bitstream I/O](#9-bitstream-io)
10. [Arena Allocator](#10-arena-allocator)
11. [Procedure Types](#11-procedure-types)
12. [Response Builder](#12-response-builder)
13. [LAPDm Framing](#13-lapdm-framing)
14. [Protocol Dispatcher](#14-protocol-dispatcher)
15. [Builder Pattern](#15-builder-pattern)
16. [Enum Formatters](#16-enum-formatters)
17. [Data Types](#17-data-types)
18. [Enumerations](#18-enumerations)
19. [Protocol Types](#19-protocol-types)
20. [Common Information Elements](#20-common-information-elements)
21. [Radio Resource Messages](#21-radio-resource-messages)
22. [Mobility Management Messages](#22-mobility-management-messages)
23. [Call Control Messages](#23-call-control-messages)
24. [Supplementary Services Messages](#24-supplementary-services-messages)
25. [GPRS Mobility Management Messages](#25-gprs-mobility-management-messages)
26. [GPRS Session Management Messages](#26-gprs-session-management-messages)
27. [SMS Messages](#27-sms-messages)
28. [Broadcast Call Control Messages](#28-broadcast-call-control-messages)
29. [Group Call Control Messages](#29-group-call-control-messages)
30. [Location Services Messages](#30-location-services-messages)
31. [SMS L3 Messages](#31-sms-l3-messages)
32. [Extended PD Messages](#32-extended-pd-messages)
33. [Test Procedure PD Messages](#33-test-procedure-pd-messages)
34. [MSContext - Per-Subscriber State](#34-mscontext-per-subscriber-state)
35. [Timer Framework](#35-timer-framework)
36. [Transaction Framework](#36-transaction-framework)
37. [Protocol State Machines](#37-protocol-state-machines)
38. [Channel Pool - Logical Channel Management](#38-channel-pool-logical-channel-management)
39. [FlatHandler - Zero-Overhead Callbacks](#39-flathandler-zero-overhead-callbacks)
40. [ShardedChannelPool - Thread-Safe Channel Pool](#40-shardedchannelpool-thread-safe-channel-pool)
41. [InlineFramer - Zero-Copy Frame Extraction](#41-inlineframer-zero-copy-frame-extraction)
42. [ZeroCopyStreamProcessor - Zero-Copy Stream Parsing](#42-zerocopystreamparser-zero-copy-stream-parsing)
43. [Subscriber Registry - Subscriber Session Management](#43-subscriber-registry-subscriber-session-management)
44. [RSL Types - A-bis RSL Type Definitions](#44-rsl-types-abis-rsl-type-definitions)
45. [RSL Parser - A-bis RSL Message Parsing](#45-rsl-parser-abis-rsl-message-parsing)
46. [RSL Builder - A-bis RSL Message Construction](#46-rsl-builder-abis-rsl-message-construction)
47. [Procedure Framework - Protocol Procedure Base Class](#47-procedure-framework-protocol-procedure-base-class)
48. [Procedure Runner - Concurrent Procedure Manager](#48-procedure-runner-concurrent-procedure-manager)
49. [Typed External Data - Strongly-Typed External Data Structures](#49-typed-external-data-strongly-typed-external-data-structures)
50. [ProcedureStateMixin - CRTP Mixin for Common Procedure Code](#50-procedurestatemixin-crtp-mixin-for-common-procedure-code)
51. [Procedure Orchestrator - Chained Protocol Procedures](#51-procedure-orchestrator-chained-protocol-procedures)
52. [Location Update Procedure](#52-location-update-procedure)
53. [Authentication Procedure](#53-authentication-procedure)
54. [Call Setup MO Procedure](#54-call-setup-mo-procedure)
55. [Call Setup MT Procedure](#55-call-setup-mt-procedure)
56. [Channel Assignment Procedure](#56-channel-assignment-procedure)
57. [Ciphering Mode Procedure](#57-ciphering-mode-procedure)
58. [Paging Procedure](#58-paging-procedure)
59. [Handover Procedure](#59-handover-procedure)
60. [Call Release Procedure](#60-call-release-procedure)
61. [IMSI Detach Procedure](#61-imsi-detach-procedure)
62. [Performance Optimizations Summary](#62-performance-optimizations-summary)

---

## 1. Getting Started

### Build Requirements

| Requirement | Minimum Version |
|-------------|-----------------|
| C++ Compiler | GCC 11+, Clang 10+, MSVC 2022 17.3+ |
| CMake | 3.20+ |
| Standard Library | C++20 (libstdc++ or libc++) |

### Build

```bash
mkdir build && cd build
cmake .. -DBUILD_TESTS=ON -DBUILD_EXAMPLES=ON
cmake --build . --config Release --parallel
```

CMake options:

| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_SHARED_LIBS` | OFF | Build shared library instead of static |
| `BUILD_TESTS` | OFF | Build unit tests (Google Test 1.14+) |
| `BUILD_EXAMPLES` | OFF | Build example programs |
| `ENABLE_FUZZING` | OFF | Build fuzzing target |

### CMake Integration

```cmake
find_package(gsml3parser REQUIRED)
target_link_libraries(myapp PRIVATE gsml3parser::parser)
```

Or as a subdirectory:

```cmake
add_subdirectory(libgsml3parser)
target_link_libraries(myapp PRIVATE gsml3parser::parser)
```

### Umbrella Header

Include everything with a single header:

```cpp
#include <gsml3parser/gsml3parser.hpp>
```

### Stack Modules for BTS Development

Version 0.11.0 introduces the **stack module** namespace (`gsml3parser::stack/`), providing high-level primitives for building a software Base Transceiver Station (BTS) on top of the L3 parser:

| Module | Header | Purpose |
|--------|--------|---------|
| **MSContext** | `stack/ms_context.h` | Per-subscriber state: identity, channel, flags (≤ 256 bytes, zero allocations) |
| **Timer Framework** | `stack/l3_timer.h` | Protocol timers (T3101–T3395), TimerManager with callback/span-based tick (zero heap) |
| **Transaction Framework** | `stack/transaction.h` | Request-response correlation: O(1) TI lookup for CC/SS, PD+MTI scan for others |
| **Protocol State Machines** | `stack/state_machine.h` | RR/MM/CC FSM skeletons with switch-based O(1) dispatch |
| **Channel Pool** | `stack/channel_pool.h` | Logical channel allocation/release, RA decoding, VEA support |

All stack modules follow the same design principles: zero heap allocations on hot paths, fixed-size internal storage, and one-instance-per-MS thread model. See [`doc/bts_architecture.md`](bts_architecture.md) for the complete architecture guide and scaling recommendations.

```cpp
// Minimal BTS setup using stack modules:
#include <gsml3parser/gsml3parser.hpp>

using namespace gsml3parser;

auto ctx     = MSContext::createWithTMSI(0x12345678u);
TimerManager timers;
TransactionManager transactions;
RRStateMachine rrFsm;
ChannelPool channels;

// Register channels, start timers, process messages...
channels.addChannel({ChannelType::SDCCHType, 0, 0, 100});
timers.start(L3TimerId::T3101);
rrFsm.setState(RRStateMachine::State::IDLE);
```

---

## 2. Core Types

### ParseError

**File:** `gsml3parser/expected.h`

Structured error type with zero heap allocation for short messages (SSO up to 47 characters).

```cpp
struct ParseError {
    enum class Code : uint8_t {
        Ok, TruncatedInput, InvalidPD, InvalidMTI,
        LengthMismatch, InvalidIE, InvalidValue, UnsupportedFeature
    };
    Code code{Code::Ok};
    std::string_view message{};
    size_t bitPosition{};
    [[nodiscard]] constexpr bool failed() const;
};
```

| Member | Description |
|--------|-------------|
| `code` | Error classification enum |
| `message` | Human-readable error description (SSO-backed) |
| `bitPosition` | Bit offset in input where the error occurred |
| `failed()` | Returns `true` if `code != Ok` |

### Expected<T>

**File:** `gsml3parser/expected.h`

Result type that holds either a success value of type `T` or a `ParseError`. Wraps `std::variant<T, ParseError>`.

```cpp
template<typename T>
class Expected {
public:
    [[nodiscard]] static Expected hold(T value);
    [[nodiscard]] static Expected error(ParseError e);
    [[nodiscard]] constexpr bool has_value() const;
    [[nodiscard]] T& value() &;
    [[nodiscard]] const T& value() const&;
    [[nodiscard]] T&& value() &&;
    [[nodiscard]] const ParseError& error() const;
    [[nodiscard]] explicit operator bool() const;
    [[nodiscard]] T& operator*();
    [[nodiscard]] const T& operator*() const;

    template<typename F> [[nodiscard]] auto map(F&&) && -> Expected<...>;
    template<typename F> [[nodiscard]] auto and_then(F&&) && -> ...;
};
```

| Method | Description |
|--------|-------------|
| `hold(value)` | Create from success value |
| `error(err)` | Create from error |
| `has_value()` | Check for success |
| `value()` | Get the success value (throws on error) |
| `error()` | Get the error (throws on success) |
| `operator bool()` | Implicit conversion: `true` = has value |
| `operator*()` | Dereference to get value |
| `map(f)` | Transform success value, propagate error |
| `and_then(f)` | Chain Expected-producing calls |

**Expected<void>** specialization for operations that only succeed or fail:

```cpp
template<>
class Expected<void> {
public:
    [[nodiscard]] static Expected hold();
    [[nodiscard]] static Expected error(ParseError e);
    [[nodiscard]] constexpr bool has_value() const;
    [[nodiscard]] explicit operator bool() const;
    // map and and_then also available
};
```

**Usage example:**

```cpp
auto msg = parseL3Hex("060D");

if (msg) {
    std::cout << messageName(*msg) << "\n";
} else {
    std::cerr << msg.error().message << "\n";
}

// Composition with map
auto name = msg.map([](const ParsedMessage& m) {
    return messageName(m);
});

// Chaining with and_then
auto roundTripped = msg.and_then(writeL3Hex)
    .and_then(parseL3Hex);
```

---

## 3. Bit-Level I/O

### BitReader

**File:** `gsml3parser/bitreader.h`

Bounds-checked, MSB-first bit reader over a byte buffer. No heap allocation.

```cpp
class BitReader {
public:
    constexpr BitReader(const uint8_t* buf, size_t nbits);
    [[nodiscard]] Expected<uint32_t> readField(unsigned nbits) &;
    [[nodiscard]] uint32_t peekField(unsigned nbits) const &;
    [[nodiscard]] bool hasMore() const noexcept;
    [[nodiscard]] size_t remainingBits() const noexcept;
    [[nodiscard]] size_t position() const noexcept;
    void alignToOctet();
    [[nodiscard]] Expected<void> readBytes(uint8_t* out, size_t count) &;
};
```

| Method | Description |
|--------|-------------|
| `readField(nbits)` | Read `nbits` bits, advance position. Returns error if past end. |
| `peekField(nbits)` | Read without advancing. `nbits` is clamped to 32 (return type is `uint32_t`), so requesting more than 32 bits is safe (no undefined behavior). Returns 0 when no bits are available (empty/null buffer) or for out-of-bounds bits. |
| `hasMore()` | True if more bits available |
| `remainingBits()` | Number of unread bits remaining |
| `position()` | Current bit position |
| `alignToOctet()` | Round up to next byte boundary |
| `readBytes(out, count)` | Read whole bytes into buffer. Optimized for aligned reads. |

**Bit ordering:** MSB-first within each octet (bit 7 is read first).

> **Safety notes:** `peekField(nbits)` clamps `nbits` to 32 (its return type is
> `uint32_t`), so requesting more than 32 bits is safe and never triggers undefined
> behavior. When no bits are available (empty or null buffer), `peekField` returns 0
> instead of performing a negative shift. `readField(nbits)` rejects `nbits > 32` with an
> error and reads at most the bits that remain.

### BitWriter

**File:** `gsml3parser/bitwriter.h`

MSB-first bit writer over a byte buffer. No heap allocation.

```cpp
class BitWriter {
public:
    constexpr BitWriter(uint8_t* buf, size_t nbits);
    void writeField(uint32_t value, unsigned nbits);
    void writeOctet(uint8_t v);
    void writeBytes(const uint8_t* data, size_t count);
    void alignToOctet();
    [[nodiscard]] size_t position() const noexcept;
};
```

| Method | Description |
|--------|-------------|
| `writeField(value, nbits)` | Write top `nbits` of `value`, MSB-first |
| `writeOctet(v)` | Write a single byte |
| `writeBytes(data, count)` | Write a sequence of bytes |
| `alignToOctet()` | Pad with zeros to next byte boundary |
| `position()` | Current bit position written |

**Usage example:**

```cpp
uint8_t buf[64] = {};
BitWriter writer(buf, sizeof(buf) * 8);

writer.writeField(0x06, 4);  // PD
writer.writeField(0x0D, 8);  // MTI
// ... write message body ...
size_t bitsWritten = writer.position();
```

---

## 4. Parser Configuration

### ParserConfig

**File:** `gsml3parser/parser_config.h`

Immutable, thread-safe parser configuration. No mutex, no atomic operations. Pure data struct.

```cpp
struct ParserConfig {
    LogLevel logLevel{LogLevel::WARNING};

    [[nodiscard]] constexpr LogLevel getLogLevel() const;
    [[nodiscard]] ParserConfig withLogLevel(LogLevel lvl) const;
};
```

| Method | Description |
|--------|-------------|
| `getLogLevel()` | Current log level |
| `withLogLevel(lvl)` | Return new config with changed log level |

The struct is reserved for future parser options (log level and similar). It intentionally carries no per-PD handler table: all 12 protocol domains are parsed by the built-in domain parsers in `parseL3()`.

**Thread safety:** Safe for concurrent read access from any number of threads. Builder methods return new instances - the original is never modified.

**Usage example:**

```cpp
ParserConfig cfg;
cfg = cfg.withLogLevel(LogLevel::ERR);

// Parse with config - no mutex acquired
auto result = parseL3Hex("060D", cfg);
```

---

## 5. L3 Header Parsing

### L3Header

**File:** `gsml3parser/l3header.h`

Decoded L3 protocol header fields.

```cpp
struct L3Header {
    L3PD pd;       // Protocol Discriminator
    int mti{};     // Message Type Indicator
    unsigned ti{}; // Transaction Identifier (CC/SS)
    bool tif{};    // TI Flag (short RR messages)
    [[nodiscard]] bool isValid() const;
};
```

### parseL3Header()

Parse a 2-byte L3 header from raw data.

```cpp
Expected<L3Header> parseL3Header(std::span<const uint8_t> data);
```

Returns `TruncatedInput` error if fewer than 2 bytes provided. Returns `InvalidPD` for unrecognized PD values.

**Decoding rules:**
- Byte 0 high nibble (bits 4-7): PD value
- For CC/SS: Byte 0 bits 1-3 = TI, bit 0 = TIF
- Byte 1: raw MTI (8 bits)
- For MM/CC/SS: actual mti = `(raw & 0xFC) >> 2` (6-bit messageType, NSD discarded)
- For RR short messages with TIF=1: mti = `0x100 + raw_byte1`

---

## 6. Message Variants

### Domain Variants

**File:** `gsml3parser/message_types.h`

Each protocol domain has a `std::variant` type that holds all message types for that domain:

```cpp
using RRM      = std::variant< /* 95 RR types */ >;
using MMM      = std::variant< /* 20 MM types */ >;
using CCM      = std::variant< /* 23 CC types */ >;
using SSM      = std::variant< /* 3 SS types */ >;
using GMM      = std::variant< /* 23 GMM types */ >;
using SM       = std::variant< /* 29 SM types */ >;
using SMS      = std::variant< /* 19 SMS types (5 CP + 14 L3) */ >;
using BCCM     = std::variant< /* 8 BCC types */ >;
using GCCM     = std::variant< /* 8 GCC types */ >;
using LSM      = std::variant< /* 2 LS types */ >;
using EXTENDED = std::variant<L3ExtendedMessage>;
using TESTPROC = std::variant<L3TestProcedureMessage>;
```

### ParsedMessage

The top-level variant that wraps all domains:

```cpp
using ParsedMessage = std::variant<RRM, MMM, CCM, SSM, GMM, SM, SMS, BCCM, GCCM, LSM, EXTENDED, TESTPROC>;
```

Stored on the stack - no heap allocation. `sizeof(ParsedMessage) = 416` bytes (guaranteed < 8 KB via `static_assert`). The variant spans 12 protocol domains.

**Usage:**

```cpp
auto msg = parseL3Hex("060D");
if (msg) {
    const ParsedMessage& parsed = *msg;

    // Visit all alternatives
    std::visit([](const auto& domain) {
        std::visit([](const auto& concrete) {
            using T = std::decay_t<decltype(concrete)>;
            // Process concrete message type T
        }, domain);
    }, parsed);
}
```

---

## 7. Parser and Serializer API

**File:** `gsml3parser/parser.h`

### parseL3()

Parse a complete L3 message from raw bytes.

```cpp
Expected<ParsedMessage> parseL3(std::span<const uint8_t> data, const ParserConfig& cfg = {});
```

| Parameter | Description |
|-----------|-------------|
| `data` | Raw L3 message bytes (header + body) |
| `cfg` | Parser configuration (optional, defaults to empty) |

Returns `Expected<ParsedMessage>` - the parsed variant on success, or `ParseError` on failure.

Handles short messages automatically: 1-byte Channel Request, 4-byte Handover Access, 7-byte Synchronization Channel Information.

### parseL3Hex()

Parse a complete L3 message from a hex-encoded string.

```cpp
Expected<ParsedMessage> parseL3Hex(std::string_view hex, const ParserConfig& cfg = {});
```

Whitespace in the hex string is ignored.

### writeL3()

Serialize a ParsedMessage to raw bytes.

```cpp
Expected<size_t> writeL3(const ParsedMessage& msg, uint8_t* out, size_t maxlen);
```

Returns number of bytes written on success. Returns `InvalidValue` error if buffer too small.

### writeL3Hex()

Serialize a ParsedMessage to a hex-encoded string.

```cpp
Expected<std::string> writeL3Hex(const ParsedMessage& msg);
```

Uses inline encoding with lookup table - no `std::ostringstream` or `std::iomanip`.

### writeL3Bytes()

Serialize a ParsedMessage to a `std::vector<uint8_t>` of raw bytes (header + body). Returns bytes ready for `LAPDmEntity.sendUI()`/`sendData()` and over-the-air transmission.

```cpp
Expected<std::vector<uint8_t>> writeL3Bytes(const ParsedMessage& msg);
```

| Return | Description |
|--------|-------------|
| `std::vector<uint8_t>` | Raw L3 bytes, ready for LAPDmEntity.sendUI()/sendData() or direct transmission |

**Usage:**

```cpp
auto msg = parseL3Hex("060D00");
if (msg) {
    auto bytes = writeL3Bytes(*msg);
    if (bytes) {
        // bytes->data() is ready for LAPDmEntity.sendUI()/sendData()
    }
}
```

### Round-trip Pattern

```cpp
auto original = parseL3Hex("06270460001");
if (original) {
    auto hex = writeL3Hex(*original);
    if (hex) {
        auto reparsed = parseL3Hex(*hex);
        // reparsed should hold the same message type with the same fields
    }
}
```

---

## 8. Visitor Helpers

**File:** `gsml3parser/visitor.h`

### tryGet<T>()

Compile-time typed access - no `dynamic_cast` needed.

```cpp
template<typename T> const T* tryGet(const ParsedMessage& msg);
template<typename T> T* tryGet(ParsedMessage& msg);
```

Returns pointer to the concrete type if the variant holds it, or `nullptr` otherwise. If `T` is not in the correct domain variant, `if constexpr` prevents instantiation of invalid `std::get_if`.

**Usage:**

```cpp
auto msg = parseL3Hex("060D");
if (msg) {
    if (auto* cr = tryGet<L3ChannelRelease>(*msg)) {
        std::cout << "Cause: " << static_cast<int>(cr->cause()) << "\n";
    }
}
```

### messageName()

Returns a human-readable name for the message type.

```cpp
std::string_view messageName(const ParsedMessage& msg);
```

### messagePD()

Returns the Protocol Discriminator of the message.

```cpp
L3PD messagePD(const ParsedMessage& msg);
```

### messageMTI()

Returns the Message Type Indicator value.

```cpp
int messageMTI(const ParsedMessage& msg);
```

---

## 9. Bitstream I/O

### ByteSource

**File:** `gsml3parser/bitstream/byte_source.h`

Abstract interface for byte sources.

```cpp
class ByteSource {
public:
    virtual ~ByteSource() = default;
    [[nodiscard]] virtual size_t read(uint8_t* buf, size_t maxSize) = 0;
};
```

Returns bytes actually read. Zero indicates EOF. May block.

### SpanByteSource

Reads from a contiguous memory span. Non-blocking.

```cpp
class SpanByteSource : public ByteSource {
public:
    explicit SpanByteSource(std::span<const uint8_t> data);
    [[nodiscard]] size_t read(uint8_t* buf, size_t maxSize) override;
    [[nodiscard]] size_t remaining() const noexcept;
};
```

### FileByteSource

Reads from a C `FILE*`. Takes ownership and closes on destruction.

```cpp
class FileByteSource : public ByteSource {
public:
    explicit FileByteSource(FILE* f);
    ~FileByteSource();
    // Non-copyable, non-movable
    [[nodiscard]] size_t read(uint8_t* buf, size_t maxSize) override;
};
```

### RingBuffer

Lock-free-ish ring buffer for producer/consumer streaming.

```cpp
class RingBuffer : public ByteSource {
public:
    explicit RingBuffer(size_t capacity = 262144);
    size_t write(const uint8_t* data, size_t len);
    [[nodiscard]] size_t available() const noexcept;
    [[nodiscard]] size_t freeSpace() const noexcept;
    [[nodiscard]] size_t read(uint8_t* buf, size_t maxSize) override;
};
```

| Method | Description |
|--------|-------------|
| `write(data, len)` | Non-blocking write. Returns bytes accepted. Returns 0 if full. |
| `available()` | Bytes available for reading |
| `freeSpace()` | Free slots in the buffer |
| `read(buf, maxSize)` | Read up to `maxSize` bytes |

**Thread safety:** Single-producer, single-consumer is fully safe without locks. Uses `std::atomic` with acquire/release ordering for correct behaviour on weakly-ordered architectures (ARM, PowerPC). On x86-64 (TSO) the compiler emits plain loads/stores - zero overhead. Multi-producer or multi-consumer requires external synchronization.

### L3Framer

**File:** `gsml3parser/bitstream/framer.h`

Extracts L3 frames from a raw byte stream.

```cpp
struct FrameConfig {
    bool useL2Length{false};       // Use L2 length octet for framing
    size_t maxMessageLength{4096}; // Safety limit
    size_t minHeaderLength{2};     // Minimum bytes for L3 header
};

class L3Framer {
public:
    explicit L3Framer(ByteSource& source, FrameConfig cfg = {});
    [[nodiscard]] Expected<ExtractedFrame> nextFrame();
    void skip(size_t nbytes);
    [[nodiscard]] size_t buffered() const noexcept;
};
```

| Method | Description |
|--------|-------------|
| `nextFrame()` | Extract next frame. Returns `TruncatedInput` when more data needed. |
| `skip(nbytes)` | Skip N bytes for error recovery |
| `buffered()` | Bytes buffered but not yet consumed |

**Framing modes:**

- **L2 length mode** (`useL2Length = true`): each frame is preceded by a length octet. Deterministic; recommended for production streams.
- **Header-based mode** (default): the frame length is derived from PD + MTI. All 12 protocol domains are supported, including BCC (PD=0x01), GCC (PD=0x00) and LS (PD=0x0c): BCC/GCC use the CC-style 6-bit MTI (byte 1 = MTI<<2 | NSD), LS uses a raw 8-bit MTI. Messages with a known fixed body length (e.g. BCC Setup/CallConfirmed/ConnectAcknowledge, GCC Setup/CallConfirmed, LS LocationServiceRequest, RR ChannelRelease, MM CMServiceAccept) are framed exactly. Variable-length messages (SI, SMS, Setup with IEs, ...) rely on a boundary heuristic that scans for the next plausible L3 header; for BCC/GCC/LS messages the scan accepts any of the 12 valid PDs, for other PDs it uses a conservative list to avoid false boundaries inside message bodies. Use L2 length mode when deterministic framing of variable-length messages is required.

### L3StreamProcessor

**File:** `gsml3parser/bitstream/stream_processor.h`

High-throughput streaming parser with statistics tracking.

```cpp
class L3StreamProcessor {
public:
    L3StreamProcessor(ByteSource& source, ParserConfig cfg = {}, FrameConfig fcfg = {});
    template<typename F>
        requires std::is_invocable_v<F, const ParsedMessage&>
    bool processOne(F&& handler);
    void processUntilEOF(FrameHandler& handler);
    void processN(size_t count, FrameHandler& handler);
    [[nodiscard]] const StreamStats& stats() const;
    void resetStats();
};
```

| Method | Description |
|--------|-------------|
| `processOne(handler)` | Non-blocking: process one frame if available. Returns true if processed. |
| `processUntilEOF(handler)` | Blocking: process all frames until source exhausted |
| `processN(count, handler)` | Process exactly N frames (or until EOF) |
| `stats()` | Running statistics |
| `resetStats()` | Reset all counters |

### FrameHandler

Callback interface for processed frames.

```cpp
class FrameHandler {
public:
    virtual ~FrameHandler() = default;
    virtual void onFrame(const ParsedMessage& msg, const ExtractedFrame& raw) = 0;
    virtual void onError(const ParseError& err, std::span<const uint8_t> rawData) {}
    virtual void onStats(const StreamStats& stats) {}
};
```

### StreamStats

Running statistics for stream processing.

```cpp
struct StreamStats {
    uint64_t totalBytes{}, totalFrames{}, parsedOk{}, parseErrors{};
    uint64_t truncatedInputs{}, unsupportedPD{};
    uint64_t rrMessages{}, mmMessages{}, ccMessages{}, ssMessages{};
    uint64_t gmmMessages{}, smMessages{}, smsMessages{};
    uint64_t bccMessages{}, gccMessages{}, lsMessages{};
    uint64_t extendedMessages{}, testprocMessages{};
};
```

### L3StreamBuilder

Fluent builder for `L3StreamProcessor`.

```cpp
class L3StreamBuilder {
public:
    L3StreamBuilder& source(ByteSource& src);
    L3StreamBuilder& source(std::span<const uint8_t> data);
    L3StreamBuilder& sourceFile(const char* path);
    L3StreamBuilder& logLevel(LogLevel lvl);
    L3StreamBuilder& useL2Length(bool v);
    L3StreamBuilder& maxMessageLength(size_t v);
    L3StreamBuilder& ringBufferSize(size_t v);
    [[nodiscard]] std::unique_ptr<L3StreamProcessor> build();
};
```

**Usage:**

```cpp
auto processor = L3StreamBuilder()
    .sourceFile("capture.bin")
    .useL2Length(true)
    .logLevel(LogLevel::WARNING)
    .build();

MyHandler handler;
processor->processUntilEOF(handler);
```

---

## 10. Arena Allocator

**File:** `gsml3parser/arena.h`

Simple bump allocator for high-throughput batch parsing.

```cpp
class Arena {
public:
    explicit Arena(size_t initialCapacity = 4096);
    void* allocate(size_t bytes, size_t alignment = alignof(std::max_align_t));
    void reset();
    [[nodiscard]] size_t remaining() const;
    [[nodiscard]] size_t used() const;
    [[nodiscard]] size_t capacity() const;
};
```

| Method | Description |
|--------|-------------|
| `allocate(bytes, alignment)` | Bump-allocate with specified alignment. Returns `nullptr` on failure. |
| `reset()` | Reclaim all memory. All previously returned pointers become invalid. |
| `remaining()` | Remaining capacity without allocation. Use to decide when to reset. |
| `used()` | Bytes consumed since last reset |
| `capacity()` | Total buffer size |

**Thread safety:** NOT thread-safe. Each thread must use its own Arena instance.

---

## 11. Procedure Types

**File:** `gsml3parser/stack/procedure_types.h`
**Namespace:** `gsml3parser::procedure`
**Spec:** 3GPP TS 24.008 (MM/CC procedures), TS 04.08 (RR procedures)

Foundation types for the Procedure Framework. Defines procedure type identifiers, lifecycle states, and result structures used by `ProcedureRunner` to manage protocol-level message exchanges.

### ProcedureType

Protocol procedure types mapped to 3GPP specification chapters:

| Value | Name | Spec Reference | Description |
|-------|------|----------------|-------------|
| `0x01` | `LocationUpdate` | TS 24.008 4.4.1 | Normal/IMSI-attached location updating |
| `0x02` | `Authentication` | TS 24.008 4.4.2 | Authentication and ciphering key setup |
| `0x03` | `CipheringMode` | TS 24.008 4.4.3 / TS 04.08 9.1.37 | Ciphering activation |
| `0x04` | `CallSetup_MO` | TS 24.008 6.1 | Mobile Originated Call setup |
| `0x05` | `CallSetup_MT` | TS 24.008 6.1 | Mobile Terminated Call setup |
| `0x06` | `ChannelAssignment` | TS 04.08 9.1.2 / 9.1.35 | Channel assignment procedure |
| `0x07` | `Handover` | TS 04.08 9.1.40 | Handover command and completion |
| `0x08` | `Paging` | TS 04.08 9.1.25 | Network-initiated paging of MS |
| `0x09` | `CMServiceRequest` | TS 24.008 4.7 | CM service request procedure |
| `0x0A` | `IMSIDetach` | TS 24.008 4.4.6 | IMSI detach procedure |
| `0x0B` | `CallRelease` | TS 24.008 6.1 | Call release (disconnect -> release complete) |
| `0x0C` | `PeriodicLocationUpdate` | TS 24.008 4.4.1 | Periodic location updating |
| `0xFF` | `Unknown` | — | Unrecognized or unsupported procedure |

### ProcedureState

Lifecycle states for a managed procedure instance:

| State | Description |
|-------|-------------|
| `Initiated` | Procedure started; awaiting first protocol message |
| `InProgress` | Active message exchange in progress |
| `WaitingExternal` | Blocked on external data (RAND from AuC, BSC decision, etc.) |
| `Completed` | Procedure finished successfully |
| `Failed` | Procedure terminated with an error condition |
| `TimedOut` | Procedure exceeded its allowed duration without completion |

### ProcedureResult

Terminal result structure for completed or failed procedures:

```cpp
struct ProcedureResult {
    ProcedureType type{ProcedureType::Unknown};
    ProcedureState state{ProcedureState::Initiated};
    std::string_view reason{};  // "ok", "timeout", "reject", etc.
};

static_assert(sizeof(ProcedureResult) <= 64);
```

### Helper Functions

| Function | Description |
|----------|-------------|
| `procedureTypeName(ProcedureType)` | Human-readable name for logging (returns "?" for Unknown) |
| `procedureStateName(ProcedureState)` | Human-readable state name for logging |

**Thread safety:** All types are trivially copyable and safe for concurrent read.
**Memory:** `sizeof(ProcedureResult) <= 64` bytes (cache-friendly).

---

## 12. Response Builder

**File:** `gsml3parser/stack/response_builder.h`
**Namespace:** `gsml3parser`
**Spec:** 3GPP TS 04.08 (message formats), TS 24.008 (procedure context)

Factory for building L3 response messages. Used after the FSM returns a `SendResponse` action to generate the corresponding protocol message. Each method provides two overloads: a vector-returning version for convenience and a span-writing version for zero-heap-allocation high-throughput paths.

### Design Pattern

```
FSM.processMessage(msg) -> SMResult{action: SendResponse, nextState: X}
  |
  v
ResponseBuilder::build<Method>(params) -> bytes
  |
  v
sendToMS(bytes)
```

### RR Responses

| Method | Response Message | Spec |
|--------|-----------------|------|
| `buildImmediateAssignment(channel, ta, requestRef?)` | ImmediateAssignment | TS 04.08 9.1.19 |
| `buildAssignmentCommand(channel, mode?)` | AssignmentCommand | TS 04.08 9.1.2 |
| `buildChannelRelease(cause?)` | ChannelRelease | TS 04.08 9.1.7 |
| `buildCipheringModeCommand(algo)` | CipheringModeCommand | TS 04.08 9.1.9 |
| `buildPhysicalInformation(ta)` | PhysicalInformation | TS 04.08 9.1.12 |

### MM Responses

| Method | Response Message | Spec |
|--------|-----------------|------|
| `buildCMServiceAccept()` | CM Service Accept | TS 04.08 9.2.5 |
| `buildCMServiceReject(cause?)` | CM Service Reject | TS 04.08 9.2.6 |
| `buildIdentityRequest(type)` | IdentityRequest | TS 04.08 9.2.10 |
| `buildAuthenticationRequest(rand)` | AuthenticationRequest | TS 04.08 9.2.2 |
| `buildLocationUpdatingAccept(lai, newTmsi?)` | LocationUpdatingAccept | TS 04.08 9.2.13 |
| `buildLocationUpdatingReject(cause)` | LocationUpdatingReject | TS 04.08 9.2.14 |
| `buildTMSIReallocationCommand(lai, tmsi)` | TMSIReallocationCommand | TS 04.08 9.2.17 |

### CC Responses

| Method | Response Message | Spec |
|--------|-----------------|------|
| `buildCallProceeding(ti)` | CallProceeding | TS 04.08 9.3.3 |
| `buildAlerting(ti)` | Alerting | TS 04.08 9.3.1 |
| `buildConnect(ti)` | Connect | TS 04.08 9.3.5 |
| `buildConnectAcknowledge(ti)` | ConnectAcknowledge | TS 04.08 9.3.6 |
| `buildDisconnect(ti, cause)` | Disconnect | TS 04.08 9.3.7 |
| `buildRelease(ti, cause)` | Release | TS 04.08 9.3.19 |
| `buildReleaseComplete(ti)` | ReleaseComplete | TS 04.08 9.3.19 |

### RR Paging & Handover Responses

| Method | Response Message | Spec |
|--------|-----------------|------|
| `buildPagingRequestType1(identity)` | PagingRequestType1 | TS 04.08 9.1.25 |
| `buildPagingRequestType2(identity)` | PagingRequestType2 | TS 04.08 9.1.25 |
| `buildPagingRequestType3(identity)` | PagingRequestType3 | TS 04.08 9.1.25 |
| `buildHandoverCommand(channel)` | HandoverCommand | TS 04.08 9.1.40 |

### CC Setup Response

| Method | Response Message | Spec |
|--------|-----------------|------|
| `buildSetup(calledNumber, ti)` | Setup | TS 24.008 9.3.2 |

### Token-Based Response Building

| Method | Description |
|--------|-------------|
| `buildResponseFromToken(token, out, session)` | Build response bytes from `ResponseToken` + session context into pre-allocated buffer (zero heap allocation). Dispatches to the appropriate `buildXxx()` method based on token value. Returns byte count or -1 on error. `session` is required: every response parameter is read from the session's `ResponseContext` (see below). The method returns -1 when a required parameter is missing instead of fabricating a value. |
| `buildSetupZeroAlloc(out, digits, len, ti)` | Build a CC Setup directly from a fixed-size BCD digit buffer (zero heap allocation). Used by the hot path; the `std::string`-based `buildSetup` overload remains for cold-path callers. |

### ResponseContext — Single Source of Response Parameters

**File:** `gsml3parser/stack/response_context.h`

Per-session parameters required to construct protocol responses. The active procedure populates it as it progresses (via `feed()` / `feedExternalTyped()`); `ResponseBuilder::buildResponseFromToken()` consumes it to build the exact bytes to transmit. This makes the session the single source of truth for response parameters and eliminates fabricated/hardcoded values on the response path.

```cpp
struct ResponseContext {
    // Authentication (TS 24.008 10.5.1.21)
    std::array<uint8_t, 16> rand{};   // 128-bit RAND in wire order
    bool hasRand{false};
    // Call Control (TS 24.008 9.3)
    uint8_t ti{0};
    CCCause ccCause{CCCause::Normal_Call_Clearing};
    std::array<char, 20> calledNumber{};
    uint8_t calledNumberLen{0};
    bool hasCalledNumber{false};
    // Mobility Management (TS 24.008 9.2)
    MMRejectCause mmCause{MMRejectCause::Zero};
    std::optional<uint32_t> newTmsi;
    // Radio Resource (TS 04.08 9.1)
    L3ChannelDescription channel{};
    bool hasChannel{false};
    uint8_t cipherAlgo{0};
    bool hasCipherAlgo{false};
    // Paging (TS 04.08 9.1.25)
    L3MobileIdentity identity{};
    bool hasIdentity{false};
    // Handover (TS 04.08 9.1.40)
    L3ChannelDescription hoChannel{};
    bool hasHoChannel{false};

    void reset() noexcept;   // clear all pending parameters
};
```

- **Ownership:** one instance per `SubscriberSession`, stored inline as `session->response` (`sizeof(ResponseContext) <= 160` bytes, fixed arrays, zero heap).
- **Population rule:** each procedure writes the parameters it learns (RAND from `AuthChallenge`, new TMSI / reject cause from `VLRDecision`, channel from a channel request, TI + called number from a `Setup`, etc.) before returning a `SendResponseWithToken` result.
- **Reset rule:** the context is reset when a new procedure chain starts (`ProcedureRunner::feed()` auto-creation) and on `ProcedureOrchestrator::cancelAll()`. It is deliberately NOT reset in `onProcedureCompleted`/`onProcedureFailed`, because the caller may still build the response from the returned token after the procedure has terminated.
- **Missing-parameter rule:** `buildResponseFromToken()` returns -1 if the token requires a parameter that is not present (e.g. `hasRand == false` for `AuthenticationRequest`), never a fabricated value.

**Thread safety:** NOT thread-safe. One instance per session, single-threaded.

### Zero-Allocation Usage with Arena

```cpp
Arena arena(65536);
auto* buf = static_cast<uint8_t*>(arena.allocate(512));
int n = ResponseBuilder::buildCMServiceAccept({buf, 512});
if (n > 0) sendToMS(buf, n);
```

**Thread safety:** All methods are stateless and safe for concurrent use.
**Memory:** Vector overloads allocate one `std::vector` per call; span overloads write directly into caller-provided buffers with zero heap cost.

---

## 13. LAPDm Protocol

**Files:** `gsml3parser/lapdm_frame.h`, `gsml3parser/lapdm_entity.h`
**Namespace:** `gsml3parser::lapdm` (frame types), `gsml3parser` (entity)
**Spec:** GSM 04.06 / 3GPP TS 45.006

Full LAPDm protocol implementation with frame encoding/decoding, state machine, I-frame segmentation/reassembly, T200 timer with retransmission, and contention resolution. Zero-allocation callbacks follow the FlatHandler pattern.

### LAPDm Frame Types

**File:** `gsml3parser/lapdm_frame.h`

#### LAPDmControlFormat

Frame format discriminator (GSM 04.06 4.4):

| Value | Description |
|-------|-------------|
| `I_Format` | Information frame — carries user data with sequence numbers |
| `S_Format` | Supervisory frame — RR, REJ for flow control |
| `U_Format` | Unnumbered frame — UI, SABME, UA, DM, DISC |

#### LAPDmUFrameType

Unnumbered frame types (GSM 04.06 4.4.2.2):

| Value | Control Byte | Description |
|-------|-------------|-------------|
| `UI` | `0x03` | Unnumbered Information — unacknowledged data |
| `SABME` | `0x2F` | Set Asynchronous Balanced Mode Extended — link establishment |
| `UA` | `0x63` | Unnumbered Acknowledgement — response to SABME/DISC |
| `DM` | `0x0F` | Disconnected Mode — reject when no link available |
| `DISC` | `0x08` | Disconnect — normal link release |

#### LAPDmSFrameType

Supervisory frame types (GSM 04.06 4.4.2.1):

| Value | Description |
|-------|-------------|
| `RR` | Receive Ready — acknowledges I-frames up to NR-1 |
| `REJ` | Reject — requests retransmission from NR |

#### Field Structures

All field encode/decode methods are `constexpr` for compile-time evaluation:

| Struct | Purpose | Spec |
|--------|---------|------|
| `LAPDmAddressField` | `[SAPI(7:4)][C/R(3)][Reserved(2:1)=00][EA(0)]` | GSM 04.06 4.2.1 |
| `LAPDmIControlField` | `[NR(7:5)][P/F(4)][NS(2:0)][Fixed(3)=0]` | GSM 04.06 4.4.1 |
| `LAPDmSControlField` | `[NR(7:5)][P/F(4)][Function(1:0)][Fixed(3)=1]` | GSM 04.06 4.4.2.1 |
| `LAPDmLengthField` | `[M(7)][Reserved(6)=0][Length(5:0)]` | GSM 04.06 5.5.2 |

#### LAPDmFrame

Non-owning, zero-copy view over the input buffer. The `info` span points into the original buffer passed to `decode()`. Never store as a class member.

```cpp
struct LAPDmFrame {
    LAPDmAddressField address;
    LAPDmControlFormat format;
    LAPDmUFrameType uType;     // U-frames
    uint8_t nr, ns;            // sequence numbers (I/S frames)
    bool pf, m;                // Poll/Final, Message complete
    LAPDmSFrameType sType;     // S-frames
    std::span<const uint8_t> info; // zero-copy payload

    SAPI sapi() const noexcept;
    bool isCommand() const noexcept;
    bool hasInfo() const noexcept;
    size_t infoSize() const noexcept;
    static Expected<LAPDmFrame> decode(std::span<const uint8_t> data);
};
```

#### Frame Factory Functions

All factory functions are `constexpr`:

| Function | Description | Spec |
|----------|-------------|------|
| `makeUIFrame(sapi, command, info)` | Unnumbered Information frame | GSM 04.06 5.2.1 |
| `makeSABMEFrame(sapi, command, info)` | Link establishment (with optional payload) | GSM 04.06 5.4.1 |
| `makeUAFrame(sapi, pf, info)` | Unnumbered Acknowledgement | GSM 04.06 5.4.1.2 |
| `makeDMFrame(sapi, pf)` | Disconnected Mode response | GSM 04.06 5.4.6 |
| `makeDISCFrame(sapi, command)` | Normal link release | GSM 04.06 5.4.4 |
| `makeIFrame(sapi, command, nr, ns, pf, m, info)` | Information frame with segmentation | GSM 04.06 5.5.2 |
| `makeRRFrame(sapi, nr, pf)` | Receive Ready supervisory | GSM 04.06 5.3.2 |
| `makeREJFrame(sapi, nr, pf)` | Reject supervisory | GSM 04.06 5.3.3 |

#### Encoding Functions

| Function | Description |
|----------|-------------|
| `encodeFrame(frame)` | Serialize to `std::vector<uint8_t>` (heap allocation) |
| `encodeFrameToBuffer(frame, out, outSize)` | Zero-allocation encode into pre-allocated buffer |

**Usage:**

```cpp
using namespace gsml3parser::lapdm;

// Encode a UI frame for SAPI0
uint8_t l3Data[] = {0x60, 0x0D, 0x00}; // Channel Release
auto uiFrame = makeUIFrame(SAPI::SAPI0, true, std::span(l3Data));
auto encoded = encodeFrame(uiFrame);

// Decode received frame (zero-copy)
auto decoded = LAPDmFrame::decode(std::span(encoded));
if (decoded && decoded->format == LAPDmControlFormat::U_Format &&
    decoded->uType == LAPDmUFrameType::UI) {
    // Process payload from decoded->info
}
```

### LAPDmEntity -- Full Protocol State Machine

**File:** `gsml3parser/lapdm_entity.h`

#### LAPDmState

Protocol states (GSM 04.06 3.5.2):

| State | Description |
|-------|-------------|
| `Unused` | Initial state before `open()` |
| `LinkReleased` | No link, idle |
| `AwaitingEstablish` | Waiting for UA response to SABME |
| `AwaitingRelease` | Waiting for UA response to DISC |
| `LinkEstablished` | Normal data transfer (ABM mode) |
| `ContentionResolution` | Contention resolution phase (SAPI0 only) |

#### LAPDmChannelProfile

Channel-specific parameters:

```cpp
struct LAPDmChannelProfile {
    size_t n201;        // Max I-frame payload in octets
    unsigned n200;      // Max retransmissions
    uint32_t t200Ms;    // T200 timer in milliseconds

    static LAPDmChannelProfile SDCCH(); // n201=20, n200=23, t200=900ms
    static LAPDmChannelProfile SACCH(); // n201=18, n200=5,  t200=3600ms
    static LAPDmChannelProfile FACCH(); // n201=20, n200=34, t200=900ms
};
```

| Channel | N201 (octets) | N200 (retries) | T200 (ms) | Max timeout |
|---------|---------------|----------------|-----------|-------------|
| SDCCH   | 20            | 23             | 900       | 20.7s       |
| SACCH   | 18            | 5              | 3600      | 18.0s       |
| FACCH   | 20            | 34             | 900       | 30.6s       |

#### Callback Types

Zero-allocation callbacks (FlatHandler-style):

```cpp
using L3ReceiveFn = void (*)(SAPI sapi, Primitive prim, std::span<const uint8_t> l3Data, void* ctx);
using L1TransmitFn = void (*)(std::span<const uint8_t> frameBytes, void* ctx);
```

#### LAPDmEntity Public API

| Method | Description | Spec |
|--------|-------------|------|
| `LAPDmEntity(profile, l3Cb, l1Cb, ctx)` | Construct with callbacks | — |
| `open(sapi, commandBit)` | Transition to LinkReleased | GSM 04.06 3.5.2 |
| `receiveFrame(frameBytes)` | Process incoming frame through FSM | GSM 04.06 5.x |
| `sendUI(sapi, l3Data)` | Send unacknowledged UI frame | GSM 04.06 5.2.1 |
| `sendData(l3Data)` | Send acknowledged I-frames (segmented) | GSM 04.06 5.5.2 |
| `sendSABME()` | Initiate link establishment | GSM 04.06 5.4.1 |
| `sendDISC()` | Initiate normal link release | GSM 04.06 5.4.4 |
| `hardRelease()` | Immediate transition to LinkReleased | — |
| `tickT200(elapsed)` | Advance T200 timer, triggers retransmission | GSM 04.06 4.3.2 |
| `state()` | Current protocol state | — |
| `sapi()` | Configured SAPI | — |
| `isEstablished()` | True if LinkEstablished or ContentionResolution | — |
| `framesSent()` / `framesReceived()` / `retransmissions()` | Protocol counters | — |
| `hasOutstandingFrame()` | True if I-frame awaiting ACK (k=1) | GSM 04.06 4.3.1 |
| `resetStats()` | Reset all statistics | — |

#### State Transition Diagram

```
Unused ──open()──> LinkReleased
                 │  ^
          SABME  │  │ DISC/UA
           from   │  │
            MS    │  │
                  ▼  │
         AwaitingEstablish ──UA──> LinkEstablished
                  │                      │
             T200 expiry                  │ sendDISC()
                  │                      ▼
                  ▼             AwaitingRelease ──UA──> LinkReleased
             LinkReleased                          │
                                                 T200 expiry
                                                      ▼
                                                 LinkReleased
```

#### Performance Characteristics

| Metric | Value |
|--------|-------|
| `sizeof(LAPDmEntity)` | < 512 bytes (enforced by `static_assert`) |
| Heap allocations | Zero on `receiveFrame()` hot path |
| Callbacks | Raw function pointer + void* ctx — zero heap per instance |
| Dynamic buffers | `mPendingFrame`, `mReassemblyBuffer` lazy-allocated |
| Thread safety | NOT thread-safe; one instance per SAPI per logical channel |

**Usage:**

```cpp
#include <gsml3parser/lapdm_entity.h>

using namespace gsml3parser;

// Callbacks (static functions for zero-allocation)
static void onL3(SAPI sapi, Primitive prim, std::span<const uint8_t> data, void* ctx) {
    // Handle L3 message delivery
}
static void onL1(std::span<const uint8_t> frame, void* ctx) {
    // Send encoded frame to PHY/radio layer
}

// Create entity for SDCCH channel
auto profile = LAPDmChannelProfile::SDCCH();
LAPDmEntity entity(profile, onL3, onL1, nullptr);

entity.open(SAPI::SAPI0, true); // BTS side (command bit = true)

// Send unacknowledged UI data
uint8_t l3Data[] = {0x60, 0x0D, 0x00};
entity.sendUI(SAPI::SAPI0, std::span(l3Data));

// In event loop: tick T200 timer periodically
entity.tickT200(std::chrono::milliseconds(100));
```

---

## 14. Protocol Dispatcher

**File:** `gsml3parser/dispatcher.h`

Callback-based message routing for BTS-style protocol handling. Dispatches incoming L3 messages to registered type-specific handlers using O(1) fixed-array lookup on PD+MTI keys (no hash map, no heap allocation for handler storage).

### MessageHandler

The `MessageHandler` alias now uses `FlatHandler` — a zero-overhead callback type that eliminates `std::function` virtual dispatch overhead. Each `FlatHandler` is exactly two machine words (16 bytes on 64-bit).

```cpp
using MessageHandler = FlatHandler;
```

To create handlers from lambdas, use the factory functions:

| Factory | Use Case | Allocation |
|---------|----------|------------|
| `makeHandler(lambda)` | Non-capturing lambdas (stateless callables only; function pointers are rejected at compile time) | Zero (compile-time resolved) |
| `makeSharedHandler(lambda)` | Capturing lambdas | Heap allocation (shared_ptr managed) |
| `FlatHandler{fn, ctx}` constructor | Plain function pointers with a user context | Zero |

### ProtocolDispatcher

```cpp
class ProtocolDispatcher {
public:
    ProtocolDispatcher() = default;
    ~ProtocolDispatcher();

    void registerHandler(L3PD pd, int mti, MessageHandler handler);
    void registerDomainHandler(L3PD pd, MessageHandler handler);
    void setFallbackHandler(MessageHandler handler);
    void dispatch(const ParsedMessage& msg, void* context = nullptr);
    bool dispatchRaw(std::span<const uint8_t> data, void* context = nullptr);
    void registerTIHandler(uint8_t ti, MessageHandler handler);
    void dispatchWithTI(const ParsedMessage& msg, void* context = nullptr);
};
```

| Method | Description | Complexity |
|--------|-------------|------------|
| `registerHandler(pd, mti, h)` | Register handler for a specific PD + MTI combination | O(1) array index |
| `registerDomainHandler(pd, h)` | Catch-all handler for all messages in a PD domain | O(1) array index |
| `setFallbackHandler(h)` | Global fallback for unregistered message types | O(1) |
| `dispatch(msg, ctx)` | Route a parsed message to the matching handler | O(1) array lookup |
| `dispatchRaw(data, ctx)` | Parse raw bytes and dispatch in one call | O(1) + parse cost |
| `registerTIHandler(ti, h)` | Register handler by Transaction Identifier (CC/SS) | O(1) array index |
| `dispatchWithTI(msg, ctx)` | Dispatch with TI awareness for CC/SS messages | O(1) TI lookup |

**Internal storage:** `std::array<std::array<MessageHandler, 256>, 16>` — fixed 64 KB handler table, no heap allocations.

**Dispatch priority:** Specific handler (PD+MTI) -> Domain handler (PD) -> Fallback handler.

**Thread safety:** NOT thread-safe. Each thread should create its own `ProtocolDispatcher` instance.

**Usage:**

```cpp
gsml3parser::ProtocolDispatcher dispatcher;

// Non-capturing lambda: use makeHandler (zero allocation)
dispatcher.registerHandler(gsml3parser::L3PD::RadioResource, 0x0D,
    gsml3parser::makeHandler([](const gsml3parser::ParsedMessage& msg, void*) {
        // Handle channel release...
    }));

// Capturing lambda: use makeSharedHandler (heap allocation)
int counter = 0;
dispatcher.registerDomainHandler(gsml3parser::L3PD::RadioResource,
    gsml3parser::makeSharedHandler([&counter](const gsml3parser::ParsedMessage& msg, void*) {
        std::cout << "RR message: " << gsml3parser::messageName(msg) << "\n";
        ++counter;
    }));

// Dispatch parsed message
auto msg = gsml3parser::parseL3Hex("600d00");
if (msg) dispatcher.dispatch(*msg);

// Or dispatch raw bytes directly
uint8_t data[] = {0x60, 0x0D, 0x00};
dispatcher.dispatchRaw(std::span<const uint8_t>(data));
```

---

## 15. Builder Pattern

Every L3 message type provides a fluent `Builder` interface for constructing messages from scratch. Each message class exposes a static `builder()` factory method that returns a builder with chainable setters matching the message's Information Elements, and a `build()` method that returns the constructed message object.

### General API

```cpp
// Each message type follows this pattern:
struct L3SomeMessage {
    static Builder builder();
    // ...
};

// Builder has chainable setters and build():
auto msg = L3SomeMessage::builder()
    .field1(value1)
    .field2(value2)
    .optionalField(value3)
    .build();
```

### Builder Coverage

Builder patterns are implemented for all message types across all 12 protocol domains:

| Domain | Messages with Builder |
|--------|----------------------|
| **RR** | All 95 types (Paging, System Information SI1–SI23, Handover, Assignment, Ciphering, etc.) |
| **MM** | All 20 types (Location Updating, Authentication, Identity, CM Service, TMSI Reallocation) |
| **CC** | All 25 types (Setup, Connect, Disconnect, Release, DTMF, Hold, Progress, etc.) |
| **GMM** | All 19 types (Attach, Detach, RA Update, Service Request, P-TMSI Reallocation, etc.) |
| **SM** | All 29 types (Activate/Deactivate/Modify PDP Context, MBMS, AA PDP, etc.) |
| **SMS** | CP messages + L3 SMS messages |
| **BCC** | All 6 types |
| **GCC** | All 8 types |
| **LS** | Both types |
| **SS** | Facility, Register, Release Complete |
| **Extended** | `L3ExtendedMessage` |
| **TestProcedure** | `L3TestProcedureMessage` |

### Example - Building an Immediate Assignment

```cpp
using namespace gsml3parser;

auto msg = L3ImmediateAssignment::builder()
    .channelDescription(L3ChannelDescription(TDMA_SDCCH, 0, 1, 100))
    .timingAdvance(L3TimingAdvance(32))
    .build();

// Wrap in ParsedMessage and serialize
ParsedMessage pm{RRM{std::move(msg)}};
auto bytes = writeL3Bytes(pm);
```

### Example - Building a Paging Request Type 2

```cpp
auto msg = L3PagingRequestType2::builder()
    .pageMode(L3PageMode::TMSI)
    .tmsi(0x12345678)
    .build();

ParsedMessage pm{RRM{std::move(msg)}};
auto hex = writeL3Hex(pm);
```

### Example - Building a Setup (CC) Message

```cpp
auto msg = L3Setup::builder()
    .calledPartyNumber(L3CalledPartyBCDNumber("1234567890"))
    .bearerCapability(L3BearerCapability{})
    .build();

ParsedMessage pm{CCM{std::move(msg)}};
```

---

## 16. Enum Formatters

**File:** `gsml3parser/enum_formatters.h`

`std::formatter` specializations for all gsml3parser enums, enabling use with `std::format` and `std::println`. Each specialization delegates to the existing `operator<<(ostream&, T)`.

### Supported Enums

| Enum | Domain |
|------|--------|
| `LogLevel` | Parser config |
| `L3PD` | Protocol Discriminator |
| `Primitive` | Interlayer Primitives |
| `SAPI` | Service Access Point |
| `MobileIDType` | Identity types |
| `TypeOfNumber` | Numbering |
| `NumberingPlan` | Numbering |
| `ChannelType` | RR channels |
| `GSMAlphabet` | Text encoding |
| `RRCause` | RR cause codes |
| `MMRejectCause` | MM reject causes |
| `CCCause` | CC cause codes (Q.931) |
| `CCCauseLocation` | CC cause location |
| `BSSCause` | BSSMAP cause codes |
| `CCMessageType` | CC message type IDs |
| `GMMPTMSIType` | GMM P-TMSI type |
| `L3ProgressIndicator::Location` | CC Progress nested enum |
| `L3ProgressIndicator::Progress` | CC Progress nested enum |

### Usage

```cpp
#include <gsml3parser/enum_formatters.h>

auto msg = gsml3parser::parseL3Hex("060D00");
if (msg) {
    std::println("PD: {}", gsml3parser::messagePD(*msg));
    // Output: PD: RadioResource (0x06)

    if (auto* cr = gsml3parser::tryGet<gsml3parser::L3ChannelRelease>(*msg)) {
        std::println("Cause: {}", cr->cause());
        // Output: cause value formatted via operator<<
    }
}
```

---

## 17. Data Types

**File:** `gsml3parser/types.h`

### L3PD - Protocol Discriminator

```cpp
enum class L3PD : int8_t {
    GroupCallControl = 0x00, BroadcastCallControl = 0x01,
    CallControl = 0x03, MobilityManagement = 0x05,
    RadioResource = 0x06, GPRSMobilityManagement = 0x08,
    SMS = 0x09, GPRSSessionManagement = 0x0a,
    NonCallSS = 0x0b, Location = 0x0c,
    Extended = 0x0e, TestProcedure = 0x0f,
    Undefined = -1
};
```

### Primitive - Interlayer Primitives

```cpp
enum class Primitive : uint8_t {
    L2_DATA, L3_DATA, L3_DATA_CONFIRM, L3_UNIT_DATA,
    L3_ESTABLISH_REQUEST, L3_ESTABLISH_INDICATION,
    L3_ESTABLISH_CONFIRM, L3_RELEASE_REQUEST,
    L3_RELEASE_CONFIRM, L3_HARDRELEASE_REQUEST,
    MDL_ERROR_INDICATION, L3_RELEASE_INDICATION,
    PH_CONNECT, HANDOVER_ACCESS
};
```

### SAPI - Service Access Point Indicator

```cpp
enum class SAPI : uint8_t {
    SAPI0 = 0, SAPI3 = 3, SAPI0_Sacch = 4,
    SAPI3_Sacch = 7, Undefined = 16
};
```

### MobileIDType

```cpp
enum class MobileIDType : uint8_t {
    NoID = 0, IMSI = 1, IMEI = 2, IMEISV = 3, TMSI = 4
};
```

### TypeOfNumber

```cpp
enum class TypeOfNumber : uint8_t {
    Unknown = 0, International = 1, National = 2,
    NetworkSpecific = 3, ShortCode = 4,
    Alphanumeric = 5, Abbreviated = 6
};
```

### NumberingPlan

```cpp
enum class NumberingPlan : uint8_t {
    Unknown = 0, E164 = 1, X121 = 3, F69 = 4,
    National = 8, Private = 9, ERMES = 10
};
```

### ChannelType

```cpp
enum class ChannelType : uint8_t {
    SCHType, FCCHType, BCCHType, CCCHType, RACHType,
    SACCHType, CBCHType, SDCCHType, FACCHType, TCHFType,
    TCHHType, AnyTCHType, PDTCHCS1Type, PDTCHCS2Type,
    PDTCHCS3Type, PDTCHCS4Type, LoopbackFullType,
    LoopbackHalfType, AnyDCCHType, UndefinedCHType
};
```

### GSMAlphabet

```cpp
enum class GSMAlphabet : uint8_t {
    ALPHABET_7BIT, ALPHABET_8BIT, ALPHABET_UCS2
};
```

---

## 18. Enumerations

**File:** `gsml3parser/enums.h`

### RRCause - RR Cause Codes

3GPP TS 24.008 10.5.2.31. Values: `Normal_Event`, `Unspecified`, `Channel_Unacceptable`, `Timer_Expired`, `No_Activity_On_The_Radio`, `Preemptive_Release`, `Handover_Impossible`, `Channel_Mode_Unacceptable`, `Frequency_Not_Implemented`, `Leaving_Group_Call_Area`, `Lower_Layer_Failure`, and protocol error codes.

```cpp
const char* RRCause2Str(RRCause cause);
```

### MMRejectCause - MM Reject Cause Codes

3GPP TS 24.008 10.5.3.6. Values: `IMSI_Unknown_In_HLR`, `Illegal_MS`, `IMSI_Unknown_In_VLR`, `IMEI_Not_Accepted`, `Illegal_ME`, `PLMN_Not_Allowed`, `Location_Area_Not_Allowed`, `Roaming_Not_Allowed_In_LA`, `Network_Failure`, `MAC_Failure`, `Congestion`, and protocol error codes.

```cpp
const char* MMRejectCause2Str(MMRejectCause cause);
```

### CCCause - CC Cause Codes

3GPP TS 24.008 10.5.4.11 / ISDN Q.931. Values: `Unassigned_Number`, `No_Route_To_Destination`, `Normal_Call_Clearing`, `User_Busy`, `Call_Rejected`, `No_Channel_Available`, `Network_Out_Of_Order`, `Requested_Facility_Not_Subscribed`, and 40+ more.

```cpp
const char* CCCause2Str(CCCause cause);
```

### CCCauseLocation

Indicates where the cause originated: `User`, `Private_Serving_Local`, `Public_Serving_Local`, `Transit`, `Public_Serving_Remote`, `Private_Serving_Remote`, `International`, `Beyond_Inter_Networking`.

### BSSCause - BSS Cause Codes

3GPP TS 48.008 3.2.2.5. Values: `Radio_Interface_Failure`, `Uplink_Quality`, `Downlink_Quality`, `Handover_Successful`, `Better_Cell`, `No_Radio_Resource_Available`, `CCCH_Overload`, and more.

```cpp
const char* BSSCause2Str(BSSCause cause);
```

---

## 19. Protocol Types

**File:** `gsml3parser/protocol_types.h`

Bounded protocol field types that enforce spec-defined ranges at compile time.

### Bounded<T, Min, Max>

Template for range-checked scalar values:

```cpp
template<typename T, T Min, T Max>
class Bounded {
public:
    constexpr Bounded(T value);
    [[nodiscard]] constexpr T get() const;
    [[nodiscard]] constexpr operator T() const;
};
```

### Common Bounded Types

| Type | Range | Description |
|------|-------|-------------|
| `Arfcn` | 0–1023 | Absolute Radio Frequency Channel Number |
| `Bsic` | 0–63 | Base Station Identity Code |
| `TimingAdvanceValue` | 0–63 | Timing Advance |
| `Ncc` | 0–7 | Network Color Code |
| `Bcc` | 0–7 | Base Station Color Code |
| `Tsc` | 0–7 | Training Sequence Code |
| `Hsn` | 0–7 | Hopping Sequence Number |
| `Maio` | 0–63 | MAIO (Multiframe Offset) |
| `TimeslotNumber` | 0–15 | TDMA timeslot |
| `CellIdentity` | 0–65535 | Cell Identity |
| `Lac` | 0–65535 | Location Area Code |

---

## 20. Common Information Elements

**File:** `gsml3parser/common/l3common.h`

All IEs are plain value types with `parse(BitReader&, size_t)`, `write(BitWriter&)`, and `text(std::ostream&)` methods. No virtual base class.

### L3MobileIdentity

Mobile station identifier (TMSI, IMSI, IMEI, IMEISV).

| Method | Description |
|--------|-------------|
| `L3MobileIdentity(uint32_t tmsi)` | Construct from TMSI |
| `L3MobileIdentity(std::string_view digits)` | Construct from digit string (IMSI/IMEI) |
| `type()` | Returns `MobileIDType` enum |
| `digits()` | Returns digit string view |
| `tmsi()` | Returns TMSI value |

### L3LocationAreaIdentity

MCC + MNC (BCD encoded) + LAC.

| Method | Description |
|--------|-------------|
| `L3LocationAreaIdentity(mcc, mnc, lac)` | Construct from components |
| `MCC()`, `MNC()`, `LAC()` | Accessor methods |

### L3CellIdentity

2-byte cell identifier.

### L3MobileStationClassmark1/2/3

MS classmark with power class, voice support, frequency bands, and optional GPRS extensions.

### L3ChannelDescription / L3ChannelDescription2

Logical channel type + TDMA frame number + timeslot + ARFCN.

### L3BCCHFrequencyList

List of BCCH carrier frequencies with NCC information.

### L3CellSelectionParameters

RxLevAccessMin, CellBarred, CAC, ACs for GPRS.

### L3RACHControlParameters

MaxRepetition, RACH-Timeout, Initial-Duration, Maximum-Duration, Periodic-Window.

### L3MeasurementResults

Serving cell RxLev/RxQual + up to 7 neighbor cell measurements.

### L3PowerCommand / L3PowerCommandAndAccessType

Absolute or relative power control command with optional access type.

### L3TimingAdvance

6-bit timing advance value (0–63).

### L3CipheringModeSetting / L3CipheringModeResponse

4-bit ciphering algorithm + mode flag / IMEISV inclusion flag.

### L3MultiRateConfiguration

AMR codec set and mode set negotiation.

### L3NeighborCellsDescription, L3ControlChannelDescription

Lists of neighbor cells and control channel descriptions.

### L3CellDescription, L3HandoverReference

Cell parameters and handover reference for handover procedures.

---

## 21. Radio Resource Messages

**File:** `gsml3parser/rr/l3rrmessages.h` - 95 message types in the `RRM` variant.

Each message is a plain struct with:
- `parse(BitReader&)` -> `Expected<Self>` (static method)
- `write(BitWriter&) const` -> void
- `mti() const` -> returns MTI value
- `text(std::ostream&) const` -> human-readable output

### Short Messages (no standard L3 header)

| Message | Size | Description |
|---------|------|-------------|
| `L3ChannelRequest` | 1 byte | RACH access with cause + TSC |
| `L3HandoverAccess` | 4 bytes | Handover confirmation with HO reference |
| `L3SynchronizationChannelInformation` | 7 bytes | SCH info with FN, TOA, BSIC |

### Paging Messages

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3PagingRequestType1` | 0x21 | DL | PageMode + MobileIdentity [+ second ID] |
| `L3PagingRequestType2` | 0x22 | DL | PageMode + TMSI (4 bytes) |
| `L3PagingRequestType3` | 0x24 | DL | PageMode + IMSI/IMEI digits |
| `L3PagingResponse` | 0x27 | UL | MobileIdentity [+ Classmark2/3] |

### System Information Messages

| Message | MTI | Description |
|---------|-----|-------------|
| `L3SystemInformationType1` | 0x19 | Cell access parameters, CBCH flag |
| `L3SystemInformationType2` | 0x1a | BCCH freq list, NCC permitted, RACH control |
| `L3SystemInformationType2bis` | 0x1f | Extended BCCH freq list (GPRS) |
| `L3SystemInformationType2ter` | 0x14 | BCCH freq list with GPRS cell options |
| `L3SystemInformationType3` | 0x1b | Cell desc, BA list type 1, rest octets |
| `L3SystemInformationType4` | 0x1c | LAI, CI, cell selection, RACH control, rest octets |
| `L3SystemInformationType5` | 0x1d | BA list type 2 |
| `L3SystemInformationType5bis` | 0x20 | Extended BA list (GPRS) |
| `L3SystemInformationType5ter` | 0x23 | BA list with GPRS cell options |
| `L3SystemInformationType6` | 0x1e | CI, LAI, SACCH cell options, NCC permitted |
| `L3SystemInformationType7` | 0x15 | BA list type 3 |
| `L3SystemInformationType8` | 0x16 | NCC permitted (SACCH) |
| `L3SystemInformationType9` | 0x17 | CI, cell selection, BCCH cell options |
| `L3SystemInformationType13` | 0x00 | Cell desc, BA list type 1, rest octets |
| `L3SystemInformationType16` | 0x01 | CI, cell selection, BCCH cell options (SACCH) |
| `L3SystemInformationType17` | 0x04 | NCC permitted (SACCH extended) |

### Dedicated Channel Messages

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3ChannelRelease` | 0x0D | DL | Cause [+ GPRS resumption] |
| `L3ImmediateAssignment` | 0x3F | DL | PageMode, channel desc, TA, mobile alloc |
| `L3ImmediateAssignmentExtended` | - | DL | Extended immediate assignment |
| `L3ImmediateAssignmentReject` | 0x3A | DL | Wait indication entries |
| `L3AdditionalAssignment` | 0x01 | DL | Additional channel assignment |
| `L3PhysicalInformation` | 0x26 | DL | Timing advance command |
| `L3AssignmentCommand` | 0x2E | DL | Channel desc, mode, power command |
| `L3AssignmentComplete` | 0x29 | UL | Cause |
| `L3AssignmentFailure` | 0x2F | UL | Cause |
| `L3HandoverCommand` | 0x2B | DL | Cell desc, channel desc2, HO ref, power, sync |
| `L3HandoverComplete` | 0x2C | UL | Cause |
| `L3HandoverFailure` | 0x28 | UL | Cause |
| `L3RRStatus` | 0x12 | UL | Cause |
| `L3ClassmarkChange` | 0x16 | UL | Classmark2/3 |
| `L3ClassmarkEnquiry` | 0x13 | DL | Empty body |
| `L3MeasurementReport` | 0x15 | UL | RxLev/RxQual + neighbors |
| `L3CipheringModeCommand` | 0x35 | DL | Ciphering setting + key seq |
| `L3CipheringModeComplete` | 0x32 | UL | Empty body |
| `L3ChannelModeModify` | 0x10 | DL | Channel desc + mode [+ multi-rate] |
| `L3ChannelModeModifyAcknowledge` | 0x11 | UL | Channel desc + mode |
| `L3GPRSSuspensionRequest` | 0x34 | UL | TLLI, RA ID, suspension cause |
| `L3ApplicationInformation` | 0x38 | DL/UL | RRLP encapsulation data |

### Configuration Change Messages

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3ConfigurationChangeCommand` | 0x30 | DL | [ChanDesc] [PowerCmd] |
| `L3ConfigurationChangeAcknowledge` | 0x31 | UL | Empty body |
| `L3ConfigurationChangeReject` | 0x33 | UL | Cause |

### Partial Release Messages

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3PartialRelease` | 0x0a | DL | ChannelDescription |
| `L3PartialReleaseComplete` | 0x0f | UL | Empty body |

### Extended Measurement Messages

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3ExtendedMeasurementReport` | 0x36 | UL | MeasurementResults (16 octets) |
| `L3ExtendedMeasurementOrder` | 0x37 | DL | Variable-length data |

### Frequency Redefinition

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3FrequencyRedefinition` | 0x14 | DL | CellChannelDescription + RACHControlParameters |

### Notification Messages

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3NotificationNCH` | 0x20 | DL | Variable-length data (CBCH) |
| `L3NotificationResponse` | 0x26 | UL | Variable-length data |

### VGCS/VBS Messages

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3VGCSUplinkGrant` | 0x09 | DL | Empty body |
| `L3UplinkRelease` | 0x0e | DL | Empty body |
| `L3UplinkBusy` | 0x2a | DL | Empty body |

### Data Indication Messages

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3TalkerIndication` | 0x11 | DL | Empty body |
| `L3PriorityUplinkRequest` | 0x66 | UL | TMSI (4 octets) |
| `L3DataIndication` | 0x67 | DL | Variable-length data |
| `L3DataIndication2` | 0x68 | DL | Variable-length data |

### DTM and Packet Messages

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3DTMAssignmentFailure` | 0x80 | UL | Cause |
| `L3DTMReject` | 0x81 | DL | Empty body |
| `L3DTMRequest` | 0x82 | UL | Empty body |
| `L3PacketAssignment` | 0x83 | DL | ChannelDescription + TimingAdvance |
| `L3DTMAssignmentCommand` | 0x84 | DL | Empty body |
| `L3DTMInformation` | 0x85 | UL | Empty body |
| `L3PacketInformation` | 0x86 | DL | Empty body |

### Inter-RAT Classmark Change Messages

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3UTRANClassmarkChange` | 0x60 | UL | Variable-length classmark |
| `L3CDMA2000ClassmarkChange` | 0x62 | UL | Variable-length classmark |
| `L3IntersysToUTRANHOCommand` | 0x63 | DL | Variable-length HO data |
| `L3IntersysToCDMA2000HOCommand` | 0x64 | DL | Variable-length HO data |
| `L3GERANIUClassmarkChange` | 0x65 | UL | Variable-length classmark |

### Additional System Information Messages

| Message | MTI | Description |
|---------|-----|-------------|
| `L3SystemInformationType14` | 0x01 | CellIdentity + CellSelectionParameters |
| `L3SystemInformationType15` | 0x43 | Empty body |
| `L3SystemInformationType18` | 0x40 | RACHControl + CellChannelDescriptions |
| `L3SystemInformationType19` | 0x41 | RACHControl + CellChannelDescriptions |
| `L3SystemInformationType20` | 0x42 | RACHControl + CellChannelDescriptions |
| `L3SystemInformationType13alt` | 0x44 | Empty body |
| `L3SystemInformationType2n` | 0x45 | Empty body |
| `L3SystemInformationType21` | 0x46 | Empty body |
| `L3SystemInformationType22` | 0x47 | Empty body |
| `L3SystemInformationType23` | 0x4f | Empty body |

### Short Messages (FACCH, BCCH)

| Message | Size | Description |
|---------|------|-------------|
| `L3SystemInformationType10` | 10 bytes | CI + LAI + CellOptions + CellSelectionParameters |
| `L3SystemInformationType10bis` | 10 bytes | CI + LAI + CellOptions + CellSelectionParameters |
| `L3SystemInformationType10ter` | 10 bytes | CI + LAI + CellOptions + CellSelectionParameters |
| `L3NotificationFACCH` | - | FACCH notification |
| `L3UplinkFree` | - | FACCH uplink free |
| `L3EnhancedMeasurementRepUL` | variable | FACCH measurement report UL |
| `L3MeasurementInfoDL` | variable | FACCH measurement info DL |
| `L3VBSVGCSRecon` | - | VBS/VGCS reconfiguration |
| `L3VBSVGCSRecon2` | - | VBS/VGCS reconfiguration 2 |
| `L3VGCSAddInfo` | - | VGCS additional info |
| `L3VGCSMSInfo` | - | VGCS SMS info |
| `L3VGCSSNeighCellInfo` | - | VGCS neighbor cell info |
| `L3NotifyAppData` | - | Notify application data |

---

## 22. Mobility Management Messages

**File:** `gsml3parser/mm/l3mmmessages.h` - 18 message types in the `MMM` variant.

### MM Information Elements

**File:** `gsml3parser/mm/l3mmelements.h`

| IE | Description |
|----|-------------|
| `L3CMServiceType` | CM service type code (MO call, emergency, SMS, etc.) |
| `L3RejectCauseIE` | MM reject cause value |
| `L3NetworkName` | PLMN network name (GSM alphabet encoded) |
| `L3TimeZoneAndTime` | Local/UTC time with timezone offset |
| `L3RAND` | 128-bit authentication challenge |
| `L3SRES` | 32-bit authentication response |

### MM Messages

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3IMSIDetachIndication` | 0x01 | UL | MobileIdentity (IMSI detach) |
| `L3CMServiceAccept` | 0x21 | DL | Empty body |
| `L3CMServiceReject` | 0x22 | DL | Reject cause |
| `L3CMServiceAbort` | 0x23 | DL | CM service type [+ cause] |
| `L3CMServiceRequest` | 0x24 | UL | MobileIdentity + CM service type |
| `L3CMReestablishmentRequest` | 0x28 | UL | MobileIdentity (TMSI) |
| `L3IdentityResponse` | 0x19 | UL | MobileIdentity |
| `L3IdentityRequest` | 0x18 | DL | Identity type (IMSI/IMEI) |
| `L3MMInformation` | 0x32 | DL | Network name, time zone, ciphering mode |
| `L3LocationUpdatingAccept` | 0x02 | DL | LAI [+ MobileIdentity] |
| `L3LocationUpdatingReject` | 0x04 | DL | Reject cause |
| `L3LocationUpdatingRequest` | 0x08 | UL | MobileIdentity + LAI + update type |
| `L3TMSIReallocationCommand` | 0x1A | DL | New TMSI + old TMSI |
| `L3TMSIReallocationComplete` | 0x1B | UL | Empty body |
| `L3MMStatus` | 0x31 | UL/DL | Cause + spare |
| `L3AuthenticationRequest` | 0x12 | DL | CKSN + RAND (128-bit) |
| `L3AuthenticationResponse` | 0x14 | UL | SRES (32-bit) |
| `L3AuthenticationReject` | 0x11 | DL | Empty body |

---

## 23. Call Control Messages

**File:** `gsml3parser/cc/l3ccmessages.h` - 20 message types in the `CCM` variant.

### CC Information Elements

**File:** `gsml3parser/cc/l3ccelements.h`

| IE | IEI | Format | Description |
|----|-----|--------|-------------|
| `L3BearerCapability` | 0x04 | TLV | Bearer capability (coding, mode, rate) |
| `L3BackupBearerCapability` | 0x7c | TLV | Backup bearer capability |
| `L3SupportedCodecList` | 0x40 | TLV | AMR codec set and mode preferences |
| `L3BCDDigits` | - | V | BCD-encoded digit string utility |
| `L3CalledPartyBCDNumber` | 0x5e | TLV | Called party number |
| `L3CallingPartyBCDNumber` | 0x5c | TLV | Calling party number |
| `L3ConnectedNumber` | 0x9c | TLV | Connected party number (GSM 04.08 10.5.4.7) |
| `L3RedirectingNumber` | 0x97 | TLV | Redirecting number (GSM 04.08 10.5.4.13) |
| `L3SubAddress` | 0x9a/0x9b | TLV | Calling/Called party sub-address (GSM 04.08 10.5.4.3) |
| `L3CauseElement` | 0x08 | TLV | CC cause code + location + diagnostic |
| `L3CallState` | - | V | Call state flags (speech, DTMF, hold, etc.) |
| `L3ProgressIndicator` | 0x1e | TLV | Progress cause and location |
| `L3KeypadFacility` | 0x2c | TV | DTMF digit indicator |
| `L3Signal` | 0x34 | TV | Signal type indicator (GSM 04.08 10.5.4.23) |
| `L3RepeatIndicator` | 0x0d | TV | Repeat count for keypad DTMF |
| `L3CLIRSuppression` | 0xc1 | TV | CLIR suppression (GSM 04.08 10.5.4.16) |
| `L3CLIRInvocation` | 0xc2 | TV | CLIR invocation (GSM 04.08 10.5.4.17) |
| `L3NetworkCCCapabilities` | 0x7a | TLV | Network CC capabilities (GSM 04.08 10.5.4.15) |
| `L3LowLayerCompatibility` | 0x86 | TLV | Low layer compatibility (GSM 04.08 10.5.4.14) |
| `L3HighLayerCompatibility` | 0x87 | TLV | High layer compatibility (GSM 04.08 10.5.4.14) |
| `L3UserUser` | 0x75 | TLV | User-User information element (GSM 04.08 10.5.4.27) |
| `L3Priority` | 0x88 | TV | Priority level and request flag (GSM 04.08 10.5.4.19) |
| `L3StreamIdentifier` | 0x8e | TV | VBS/VGCS stream identifier (GSM 04.08 10.5.4.29) |
| `L3AllowedActions` | 0x92 | TLV | Allowed actions bitmask (GSM 04.08 10.5.4.2) |
| `L3CCCapabilities` | 0x51 | TLV | CC capabilities (GSM 04.08 10.5.4.4) |
| `L3SupServFacilityIE` | 0x1c | TLV | Supplementary service facility data |
| `L3SupServVersionIndicator` | - | V | SS version indicator (GSM 04.08 10.5.4.24) |

#### IE Encoding Formats

- **V (Value-only):** No IEI octet; length is fixed by spec
- **TV (Type-Value):** IEI octet + fixed-size value (typically 1 octet)
- **TLV (Type-Length-Value):** IEI octet + length octet + variable-length value
- **LV (Length-Value):** Length octet + value (IEI known from context)

### CC Messages

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3Setup` | 0x05 | UL | CalledParty [+ CallingParty, BearerCapability, SupportedCodecs, Signal, SubAddress, LowLayerCompat, HighLayerCompat, UserUser, CLIRSuppression, CLIRInvocation, CCCapabilities, StreamIdentifier, Facility, SSVersion] |
| `L3EmergencySetup` | 0x0e | UL | Emergency call setup (no called party) |
| `L3CallProceeding` | 0x02 | DL | [+ BearerCapability×2, ProgressIndicator, Priority, NetworkCCCapabilities] |
| `L3Alerting` | 0x01 | DL | [+ Facility, ProgressIndicator, UserUser, SSVersion] |
| `L3Connect` | 0x07 | UL | [+ ProgressIndicator, ConnectedNumber, ConnectedSubAddress, UserUser, StreamIdentifier] |
| `L3ConnectAcknowledge` | 0x0f | DL | Empty body |
| `L3CallConfirmed` | 0x08 | DL | [+ BearerCapability, SupportedCodecs, Cause, UserUser] |
| `L3Disconnect` | 0x25 | UL | Cause (CCCause + CCCauseLocation) |
| `L3Release` | 0x2d | DL/UL | [+ Cause, Facility, SSVersion] |
| `L3ReleaseComplete` | 0x2a | DL/UL | [+ Cause, Facility, SSVersion] |
| `L3StartDTMF` | 0x35 | UL | KeypadFacility digit |
| `L3StopDTMF` | 0x31 | UL | Empty body |
| `L3StopDTMFAcknowledge` | 0x32 | DL | Empty body |
| `L3StartDTMFAcknowledge` | 0x36 | DL | KeypadFacility digit |
| `L3StartDTMFReject` | 0x37 | DL | Cause |
| `L3Hold` | 0x18 | UL | Empty body |
| `L3HoldReject` | 0x1a | DL | Cause |
| `L3CCStatus` | 0x3d | DL/UL | Cause + CallState |
| `L3Progress` | 0x03 | DL | ProgressIndicator |

#### CC Message Type Identifiers

MTI values are the 6-bit messageType field (GSM 04.08 Table 10.3). In the L3 header byte 1, the actual encoding is `(mti << 2) | nsd` where `nsd` (Not Significant Data) is 2 bits.

| MTI | Message | Spec Section |
|-----|---------|-------------|
| 0x01 | Alerting | GSM 04.08 9.3.1 |
| 0x02 | Call Proceeding | GSM 04.08 9.3.3 |
| 0x03 | Progress | GSM 04.08 9.3.17 |
| 0x05 | Setup | GSM 04.08 9.3.19 |
| 0x07 | Connect | GSM 04.08 9.3.5 |
| 0x08 | Call Confirmed | GSM 04.08 9.3.2 |
| 0x0e | Emergency Setup | GSM 04.08 9.3.8 |
| 0x0f | Connect Acknowledge | GSM 04.08 9.3.6 |
| 0x18 | Hold | GSM 04.08 9.3.23 |
| 0x1a | Hold Reject | GSM 04.08 9.3.24 |
| 0x25 | Disconnect | GSM 04.08 9.3.7 |
| 0x2a | Release Complete | GSM 04.08 9.3.19 |
| 0x2d | Release | GSM 04.08 9.3.19 |
| 0x31 | Stop DTMF | GSM 04.08 9.3.25 |
| 0x32 | Stop DTMF Acknowledge | GSM 04.08 9.3.25 |
| 0x35 | Start DTMF | GSM 04.08 9.3.25 |
| 0x36 | Start DTMF Acknowledge | GSM 04.08 9.3.25 |
| 0x37 | Start DTMF Reject | GSM 04.08 9.3.25 |
| 0x3d | CC Status | GSM 04.08 9.3.19 |

---

## 24. Supplementary Services Messages

**File:** `gsml3parser/ss/l3ssmessages.h` - 3 message types in the `SSM` variant.

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3SupServFacilityMessage` | varies | DL/UL | SS facility data (TLV) |
| `L3SupServRegisterMessage` | varies | DL/UL | Registration request/response |
| `L3SupServReleaseCompleteMessage` | varies | DL | SS release complete |

### SS Operation Codes

**File:** `gsml3parser/cc/l3ccelements.h`

TCAP operation codes defined in GSM TS 04.80 section 4.5. Reference: `ref/osmo-ttcn3-hacks/library/SS_Templates.ttcn` `SS_Op_Code` enum.

```cpp
enum class SSOpCode : uint8_t {
    RegisterSS = 0x0A,       EraseSS = 0x0B,
    ActivateSS = 0x0C,       DeactivateSS = 0x0D,
    InterrogateSS = 0x0E,    NotifySS = 0x10,
    RegisterPassword = 0x11, GetPassword = 0x12,
    ProcessUSSData = 0x13,   ForwardCheckSSInd = 0x26,
    ProcessUSSReq = 0x3B,    USSRequest = 0x3C,
    USSNotify = 0x3D,        ForwardCUGInfo = 0x78,
    SplitMPTY = 0x79,        RetrieveMPTY = 0x7A,
    HoldMPTY = 0x7B,         BuildMPTY = 0x7C,
    ForwardChargeAdvice = 0x7D
};

std::string_view ssOpCodeName(SSOpCode code);
```

### SS Error Codes

Error codes defined in GSM TS 04.80 section 4.5. Reference: `SS_Templates.ttcn` `SS_Err_Code` enum.

```cpp
enum class SSErrorCode : uint8_t {
    UnknownSubscriber = 0x01,       IllegalSubscriber = 0x09,
    BearerServiceNotProvisioned = 0x0A, TeleserviceNotProvisioned = 0x0B,
    IllegalEquipment = 0x0C,        CallBarred = 0x0D,
    IllegalSSOperation = 0x10,      SSErrorStatus = 0x11,
    SSNotAvailable = 0x12,          SSSubscriptionViolation = 0x13,
    SSIncompatibility = 0x14,       FacilityNotSupported = 0x15,
    AbsentSubscriber = 0x1B,        SystemFailure = 0x22,
    DataMissing = 0x23,             UnexpectedDataValue = 0x24,
    PWRegistrationFailure = 0x25,   NegativePWCheck = 0x26,
    NumPWAttemptsViolation = 0x2B,  UnknownAlphabet = 0x47,
    USSDBusy = 0x48,                MaxMPTYParticipants = 0x7E,
    ResourcesNotAvailable = 0x7F
};

std::string_view ssErrorCodeName(SSErrorCode code);
```

### L3FacilityOpCode - TCAP Component Parser

Parses the TCAP-level component from raw SS Facility data. Supports all four TCAP component types:

```cpp
class L3FacilityOpCode {
public:
    enum ComponentType : uint8_t {
        Invoke = 0x81,    ReturnResult = 0x82,
        ReturnError = 0x83, Reject = 0x84
    };

    static Expected<L3FacilityOpCode> parse(const std::string& facilityData);

    ComponentType component() const;
    int8_t invokeId() const;
    SSOpCode opCode() const;           // Invoke only
    bool hasErrorCode() const;
    SSErrorCode errorCode() const;     // ReturnError only
    const std::vector<uint8_t>& parameters() const;
    void write(BitWriter& bw) const;
    size_t lengthV() const;
    void text(std::ostream& os) const;
};
```

| Method | Description |
|--------|-------------|
| `parse(facilityData)` | Parse TCAP component from raw Facility IE data |
| `component()` | Returns Invoke/ReturnResult/ReturnError/Reject |
| `invokeId()` | TCAP invoke identifier (-128..127) |
| `opCode()` | SS operation code (Invoke components) |
| `errorCode()` | SS error code (ReturnError components) |
| `parameters()` | Remaining parameter bytes |

**Usage:**

```cpp
// Parse Facility message to extract TCAP op_code
auto msg = parseL3Hex("be e8 03 81 01 3c");
if (msg) {
    if (auto* fac = tryGet<L3SupServFacilityMessage>(*msg)) {
        auto fc = L3FacilityOpCode::parse(fac->getMapComponents());
        if (fc) {
            // fc->component() == Invoke, fc->opCode() == USSRequest
        }
    }
}
```

### L3USSDData - USSD Message IE

Parses USSD-specific content from SS Facility data. Handles GSM 7-bit alphabet encoding and UCS2.

```cpp
class L3USSDData {
public:
    enum Alphabet : uint8_t {
        DefaultAlphabet = 0,  ExtendedCIDAlphabet = 1,
        ShiftGSMtoUCS2 = 4,   UCS2 = 6
    };

    static Expected<L3USSDData> parse(const std::string& facilityData, SSOpCode opCode);

    int8_t invokeId() const;
    SSOpCode opCode() const;
    uint8_t dcs() const;
    Alphabet alphabet() const;
    unsigned language() const;
    const std::vector<uint8_t>& rawUssdString() const;
    bool isResult() const;
    void write(BitWriter& bw) const;
    size_t lengthV() const;
    void text(std::ostream& os) const;

    std::string decodeUssdString() const;
    static std::vector<uint8_t> encodeUssdString(const std::string& text);
};
```

| Method | Description |
|--------|-------------|
| `parse(facilityData, opCode)` | Parse USSD from TCAP parameter bytes |
| `invokeId()` | TCAP invoke ID |
| `opCode()` | USSD op code (USSRequest, USSNotify, ProcessUSSData, etc.) |
| `dcs()` | Data Coding Scheme (GSM 02.90 §4.1.1) |
| `alphabet()` | Alphabet type from DCS low nibble |
| `language()` | Language indicator from DCS high nibble |
| `rawUssdString()` | Raw GSM 7-bit or UCS2 encoded bytes |
| `isResult()` | True for USS-Notify (network response) |
| `decodeUssdString()` | Decode to human-readable string |
| `encodeUssdString(text)` | Encode text to GSM 7-bit packed bytes |

**Usage:**

```cpp
// Encode a USSD string
auto encoded = L3USSDData::encodeUssdString("*#100#");

// Parse USSD from Facility data
auto ussd = L3USSDData::parse(facilityBytes, SSOpCode::USSRequest);
if (ussd) {
    std::string text = ussd->decodeUssdString();  // e.g. "*#100#"
}
```

**GSM 7-bit encoding:** The encoder packs characters into 7-bit frames per GSM 03.38, producing compact byte sequences suitable for USSD transport over the SS Facility IE.

---

## 25. GPRS Mobility Management Messages

**File:** `gsml3parser/gmm/l3gmmmessages.h` - 19 message types in the `GMM` variant.
**Spec:** 3GPP TS 24.008 sections 9.4, Table 10.4.
**PD:** `0x08` (GPRSMobilityManagement).

### GMM Information Elements

**File:** `gsml3parser/gmm/l3gmmelements.h`

| IE | IEI | Format | Description |
|----|-----|--------|-------------|
| `L3PDPContextStatus` | 0x32 | TLV | PDP context activation bitmap (16 contexts) |
| `L3T3302Timer` | 0x1b | TLV | T3302 timer value (GPRS Timer 2 encoding) |
| `L3MSNetworkCapability` | - | V | MS network capability bit string |
| `L3RoutingAreaIdentification` | - | V | MCC/MNC BCD(3) + LAC(2) + RAC(1) = 6 octets |
| `L3DRXParameter` | 0x1a | TV | DRX cycle code + timer settings (2 octets) |
| `L3GMMCKSN` | - | bit-field | Ciphering key sequence number (3 bits) |
| `L3GMMCauseIE` | 0x25 | TLV | GMM cause value |
| `L3AuthRAND` | 0x15 | TLV | 128-bit authentication challenge |
| `L3AuthRES` | 0x16 | TLV | 32-bit authentication response |
| `L3AuthFailureParam` | 0x30 | TLV | AUTS failure parameter (variable) |
| `L3PTMSISignature` | 0x13 | TV | P-TMSI signature (3 octets) |
| `L3GMMStatusCause` | - | V | GMM status cause octet |

### GMM Enums

| Enum | Values | Description |
|------|--------|-------------|
| `GMMCause` | 27 codes | GMM cause values (ReqAccepted, GprsNotAllowed, PLMN_Not_Allowed, MAC_Failure, etc.) |
| `GMMAttachType` | 2 values | GPRSAttach, CombinedGPRSAndIMSIAttach |
| `GMMUpdateType` | 4 values | RAUpdated, CombinedRALAUpdated, CombinedRALAWithImsiAttach, PeriodicUpdating |
| `GMMDetachTypeMO` | 3 values | GPRS, IMSI, CombinedGPRSIMSI (MS-originated) |
| `GMMDetachTypeMT` | 3 values | ReattachRequired, ReattachNotRequired, IMSIDetach (MT-originated) |
| `GMMPTMSIType` | 2 values | Native, Mapped |

### GMM Messages

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3AttachRequest` | 0x01 | UL | MS Network Capability, attach type, CKSN, DRX, MobileIdentity, old RAI |
| `L3AttachAccept` | 0x02 | DL | Attach result, force-to-standby, update timer, RAI, [PTMSI] |
| `L3AttachComplete` | 0x03 | UL | Empty body |
| `L3AttachReject` | 0x04 | DL | GMM cause, [T3302 timer] |
| `L3DetachRequest` | 0x05 | Bidir | Detach type, power-off flag, [PTMSI], [cause] |
| `L3DetachAccept` | 0x06 | Bidir | Force-to-standby flag |
| `L3RoutingAreaUpdateRequest` | 0x08 | UL | Update type, CKSN, old RAI, [MS RA cap] |
| `L3RoutingAreaUpdateAccept` | 0x09 | DL | Force-to-standby, update result, timer, radio priority, RAI, [PTMSI] |
| `L3RoutingAreaUpdateComplete` | 0x0a | UL | Empty body |
| `L3RoutingAreaUpdateReject` | 0x0b | DL | Force-to-standby, GMM cause, [T3302 timer] |
| `L3ServiceRequest` | 0x0c | UL | CKSN, service type, PTMSI, [PDP context status] |
| `L3ServiceAccept` | 0x0d | DL | [PDP context status] |
| `L3ServiceReject` | 0x0e | DL | GMM cause, [T3346 timer] |
| `L3P_TMSIReallocationCommand` | 0x10 | DL | P-TMSI type, force-to-standby, RAI, [allocated PTMSI] |
| `L3P_TMSIReallocationComplete` | 0x11 | UL | Empty body |
| `L3AuthenticationAndCipheringRequest` | 0x12 | DL | Ciphering algorithm, IMEISV request, AC ref number, RAND |
| `L3AuthenticationAndCipheringResponse` | 0x13 | UL | AC ref number, RES |
| `L3AuthenticationAndCipheringReject` | 0x14 | DL | Empty body |
| `L3GMMIdentityRequest` | 0x15 | DL | Identity type (IMSI/IMEI), force-to-standby |
| `L3GMMIdentityResponse` | 0x16 | UL | Mobile identity |
| `L3AuthenticationAndCipheringFailure` | 0x1c | UL | GMM cause, AUTS failure parameter |
| `L3GMMStatus` | 0x20 | Bidir | GMM cause |
| `L3GMMInformation` | 0x21 | DL | Network information |

---

## 26. GPRS Session Management Messages

**File:** `gsml3parser/sm/l3smmessages.h` - 29 message types in the `SM` variant.
**Spec:** 3GPP TS 24.008 sections 9.5, Table 10.4a.
**PD:** `0x0a` (GPRSSessionManagement).

### SM Information Elements

**File:** `gsml3parser/sm/l3smelements.h`

| IE | IEI | Format | Description |
|----|-----|--------|-------------|
| `L3PDPAddress` | 0x08 | TLV | PDP type (IPv4/IPv6/PPP/IPsec) + address bytes |
| `L3QoS` | 0x09 | TLV | QoS profile: type + up to 18 element types |
| `L3AccessPointName` | 0x2F | TLV | APN string (UTF-8 encoded) |
| `L3ProtocolConfigOptions` | 0x3C | TLV | Protocol config options (e.g. IPCP=0xC029 for IPv4) |
| `L3SMCauseIE` | 0x27 | TV | SM cause value |
| `L3BackOffTimer` | 0x28 | TV | Back-off timer value (GPRS Timer 2 encoding) |
| `L3PDPHandle` | - | bit-field | PDP context identifier (4 bits, 0–15) |
| `L3TMGI` | 0x42 | TLV | Temporary Mobile Group Identity: PLMN(3) + ServiceID(2) + SessionID(1) |

### SM Enums

| Enum | Values | Description |
|------|--------|-------------|
| `PDPType` | 6 values | IPv4, IPv6, IPsecAH, PPP, Private, Unknown |
| `QoSType` | 3 values | Requested, Default, Teardown |
| `QoSElementType` | 18 values | QoSClass, MaxBitRate UL/DL, Delay, DeliveryOrder, SopClass, ResidualErrorRate, PeakThroughput, MeanThroughput, TrafficClass, GuaranteedBitRate, SRB rate, GPRS/External Priority |
| `SMCause` | 17 codes | SM cause values (ReqAccepted, Unsupported_PDP_Address_Type, PDP_Auth_Failed, IE_Invalid, etc.) |

### SM Messages - Primary PDP Context

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3ActivatePDPContextRequest` | 0x41 | UL | PDP type, [PDP address], APN, QoS, [PCO] |
| `L3ActivatePDPContextAccept` | 0x42 | DL | PDP handle, [PDP address], QoS, [PCO] |
| `L3ActivatePDPContextReject` | 0x43 | DL | SM cause, [Back-off timer] |
| `L3DeactivatePDPContextRequest` | 0x46 | Bidir | PDP handle, [PDP type], [PDP address] |
| `L3DeactivatePDPContextAccept` | 0x47 | Bidir | PDP handle |
| `L3ModifyPDPContextRequest` | 0x48 | DL | PDP handle, QoS, [PCO] |
| `L3ModifyPDPContextAccept` | 0x49 | UL | PDP handle, QoS, [PCO] |
| `L3ModifyPDPContextReject` | 0x4c | Bidir | PDP handle, SM cause, [Back-off timer] |
| `L3SMStatus` | 0x55 | Bidir | SM cause |

### SM Messages - Request PDP Context Activation (Net-initiated)

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3RequestPDPContextActivation` | 0x44 | DL | PDP handle, [PDP address], APN, QoS, [PCO] |
| `L3RequestPDPContextActivationReject` | 0x45 | UL | PDP handle, SM cause |

### SM Messages - Bidirectional Modify (MS-initiated variants)

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3ModifyPDPContextRequestMS` | 0x4A | UL | PDP handle, QoS, [PCO] |
| `L3ModifyPDPContextAcceptNet` | 0x4B | DL | PDP handle, QoS, [PCO] |

### SM Messages - Secondary PDP Context

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3ActivateSecondaryPDPContextRequest` | 0x4D | DL | PDP handle, [PDP address], APN, QoS, [PCO] |
| `L3ActivateSecondaryPDPContextAccept` | 0x4E | UL | PDP handle, [PDP address], QoS, [PCO] |
| `L3ActivateSecondaryPDPContextReject` | 0x4F | UL | PDP handle, SM cause |

### SM Messages - Always Active (AA) PDP Context

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3ActivateAAPDPContextRequest` | 0x50 | DL | PDP handle, [PDP address], APN, QoS, [PCO] |
| `L3ActivateAAPDPContextAccept` | 0x51 | UL | PDP handle, [PDP address], QoS, [PCO] |
| `L3ActivateAAPDPContextReject` | 0x52 | UL | PDP handle, SM cause |
| `L3DeactivateAAPDPContextRequest` | 0x53 | DL | PDP handle |
| `L3DeactivateAAPDPContextAccept` | 0x54 | UL | PDP handle |

### SM Messages - MBMS Context

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3ActivateMBMSContextRequest` | 0x56 | UL | TMGI, QoS, [PCO] |
| `L3ActivateMBMSContextAccept` | 0x57 | DL | PDP handle, QoS, [PCO] |
| `L3ActivateMBMSContextReject` | 0x58 | DL | SM cause |
| `L3RequestMBMSContextActivation` | 0x59 | DL | TMGI, QoS, [PCO] |
| `L3RequestMBMSContextActivationReject` | 0x5A | UL | SM cause |

### SM Messages - Network-Initiated Secondary & Notification

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3RequestSecondaryPDPContextActivation` | 0x5B | DL | PDP handle, [PDP address], APN, QoS, [PCO] |
| `L3RequestSecondaryPDPContextActivationReject` | 0x5C | UL | PDP handle, SM cause |
| `L3SMNotification` | 0x5D | DL | PDP handle |

---

## 27. SMS Messages

**File:** `gsml3parser/sms/l3smsmessages.h` - 5 CP messages; `gsml3parser/sms/l3smsl3messages.h` - 14 L3 messages; total 19 in the `SMS` variant.
**Spec:** 3GPP TS 24.011 sections 7-8, 3GPP TS 23.040 (CP/RP/TP layers); 3GPP TS 24.008 sections 9.6, Table 10.6a (SMS L3 primitives).
**PD:** `0x09` (SMS).

The SMS layer uses a three-level encapsulation: L3 header -> CP message -> RP message -> TP PDU. Additionally, the SMS L3 messages (MTI=0x11–0x1E) provide TE-to-MS SMS primitives for status reporting, deliver/reply, and notification flows.

### CP Cause Codes

| Code | Meaning |
|------|---------|
| `Unspecified` | Unspecified error |
| `CpusNotSupported` | CUPS not supported |
| `NoRPLPDU` | No RPLPDU present |
| `UnknownRPMessageType` | Unknown RP message type |
| `InvalidRPMessageReference` | Invalid message reference |
| `RPUserBusy` | RP user busy |
| `UnknownRPOriginatorAddress` | Unknown originator |
| `UnknownRPDestinationAddress` | Unknown destination |
| `RPLinkNotAvailable` | RP link not available |
| `NoRPResponse` | No RP response |

### Control Part (CP) Messages

| Message | CP-MTI | Direction | Description |
|---------|--------|-----------|-------------|
| `L3CPData` | 0x01 | Bidir | Length + RPDU (wraps relay or TP layer) |
| `L3CPAck` | 0x04 | Bidir | Acknowledgement, no body |
| `L3CPErr` | 0x10 | Bidir | CP cause value (7-bit + extension bit) |
| `L3CPStatus` | 0x12 | MT | TP-OI, MTI type, [message reference] |
| `L3CPSMT` | 0x13 | MT | Length + RPDU (Short Message to Telephony) |

### Relay Part (RP) Messages

| Message | RP-MTI (MO/MT) | Description |
|---------|----------------|-------------|
| `L3RPData` | 0 / 1 | Relay data: [originator addr], [destination addr], user data |
| `L3RPAck` | 2 / 3 | Relay acknowledgement: message reference |
| `L3RPError` | 4 / 5 | Relay error: message reference + cause |
| `L3RPSMMA` | 6 / 7 | Short message memory available |

### Transport Part (TP) Elements

**File:** `gsml3parser/sms/l3smselements.h`

| Type | TP-MTI | Description |
|------|--------|-------------|
| `L3TPDeliver` | 0x00 | MT delivery: MMS, SRI, UDHI, RP flags, OA, PID, DCS, [SCTS], UDL, user data |
| `L3TPSubmit` | 0x01 | MO submission: RD, VPF, SRR, UDHI, RP flags, MR, DA, PID, DCS, [VP], UDL, user data |
| `L3TPStatusReport` | 0x02 | Status report: MR, DA, PID, DCS, SCTS, STS |
| `L3TPCommand` | 0x03 | Command: MR, PID, DCS, CMD, [address] |

### TP Enums

| Enum | Values | Description |
|------|--------|-------------|
| `TPDCS` | 3 values | Default_Alphabet (7-bit), Default_8bit, UCS2 |
| `TPPID` | 6 values | Default, GSM, X121, Telex, LandLine, SS7_DestinationAccess |

### TP Information Elements

| IE | Description |
|----|-------------|
| `L3TPAddress` | TP-DA/TP-OA: LV format with TON/NPI + BCD digits |
| `TPSCTimeStamp` | Service centre time stamp: year, month, day, hour, minute, second, timezone (7 octets) |

---

## 28. Broadcast Call Control Messages

**File:** `gsml3parser/bcc/l3bccmessages.h` - 6 message types in the `BCCM` variant.
**Spec:** 3GPP TS 44.018 sections 9.6, Table 10.4.3.
**PD:** `0x01` (BroadcastCallControl).

L3 header encoding matches CC: Byte 0 high nibble = PD, bits 1-3 = TI, bit 0 = TIF. Byte 1 encodes 6-bit messageType shifted left by 2, plus 2-bit NSD.

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3BCCSetup` | 0x00 | MO | Broadcast call setup with TI + opaque body |
| `L3BCCProceeding` | 0x01 | MT | Network proceeding indication |
| `L3BCCConnect` | 0x05 | MT | Broadcast call connected |
| `L3BCCDisconnect` | 0x06 | MO | Broadcast call disconnect |
| `L3BCCRelease` | 0x07 | MT | Broadcast call release |
| `L3BCCReleaseComplete` | 0x0a | Bidir | Release complete |

Each message stores the body as an opaque octet sequence for basic infrastructure parsing. The `ti()` accessor returns the Transaction Identifier.

---

## 29. Group Call Control Messages

**File:** `gsml3parser/gcc/l3gccmessages.h` - 7 message types in the `GCCM` variant.
**Spec:** 3GPP TS 44.018 sections 9.7, Table 10.4.4.
**PD:** `0x00` (GroupCallControl).

L3 header encoding matches CC: Byte 0 high nibble = PD, bits 1-3 = TI, bit 0 = TIF. Byte 1 encodes 6-bit messageType shifted left by 2, plus 2-bit NSD.

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3GCCSetup` | 0x00 | MO | Group call setup with TI + opaque body |
| `L3GCCProceeding` | 0x01 | MT | Network proceeding indication |
| `L3GCCAcknowledge` | 0x02 | MT | Group call acknowledgement |
| `L3GCCConnect` | 0x05 | MT | Group call connected |
| `L3GCCDisconnect` | 0x06 | MO | Group call disconnect |
| `L3GCCRelease` | 0x07 | MT | Group call release |
| `L3GCCReleaseComplete` | 0x0a | Bidir | Release complete |

Each message stores the body as an opaque octet sequence for basic infrastructure parsing. The `ti()` accessor returns the Transaction Identifier.

---

## 30. Location Services Messages

**File:** `gsml3parser/ls/l3lsmessages.h` - 2 message types in the `LSM` variant.
**Spec:** 3GPP TS 44.031 / TS 24.027 / TS 24.028.
**PD:** `0x0c` (Location).

Location Services messages carry mobile location service parameters between the MS and the network. Both message types store their body as a raw octet sequence, with parse/write handling the L3 header dispatch.

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3LocationServiceRequest` | 0x01 | Bidir | Location service request parameters (raw body) |
| `L3LocationServiceProviderMessage` | 0x02 | Bidir | Location service provider data (raw body) |

---

## 31. SMS L3 Messages

**File:** `gsml3parser/sms/l3smsl3messages.h` - 14 message types, part of the `SMS` variant.
**Spec:** 3GPP TS 24.008 sections 9.6.1–9.6.14, Table 10.6a.
**PD:** `0x09` (SMS).

These are L3-level SMS primitives used for SMS-on-CS fallback, status reporting, and network-initiated SMS delivery. They share the PD with CP-layer messages but operate in a different context. MTI 0x12 and 0x13 overlap with CP-STATUS and CP-SMT; the parser resolves overlaps by preferring CP messages for backward compatibility.

### SMS L3 Enums

| Enum | Values | Description |
|------|--------|-------------|
| `TPStatus` | 5 values | Delivered, DeliveryAttempted, ErasedAtMS, DeliveryNotPossible, Decrypted |
| `RPDisposalType` | 4 values | NoFurtherAction, DisplayToUser, StoreInSIM, DeleteFromMS |
| `SMSCause` | 8 codes | SMS-specific cause values (NoCause, SMSSystemFailure, etc.) |

### SMS L3 Messages - Status Report Flow

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3SMSStatusReport` | 0x11 | Bidir | TP-MR, RP-Disp, [TP-DA], [TP-OA], [SCTS], [MT-StartTime], TP-ST |
| `L3SMSProvidedReplyExpected` | 0x12 | DL | [TP-PID], TP-DCS, [TP-Ud] |
| `L3SMSSubmitRep` | 0x13 | DL | [TP-PID], TP-DCS, [TP-Ud] |

### SMS L3 Messages - Deliver Flow

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3SMSDeliver` | 0x14 | DL | TP-MTI, TP-MR, [TP-OA], TP-PID, TP-DCS, SCTS, [TP-Ud] |
| `L3SMSDeliverRep` | 0x15 | UL | TP-MTI, TP-MR, [TP-DA], TP-PID, TP-DCS, [TP-Ud] |

### SMS L3 Messages - Status Ack/Reject

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3SMSStatusReportAck` | 0x16 | UL | TP-MR |
| `L3SMSStatusReportReject` | 0x17 | DL | TP-MR, SM-Cause |
| `L3SMSTSReject` | 0x18 | DL | SM-Cause |

### SMS L3 Messages - Submit Control

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3SMSSubmitDeferred` | 0x19 | DL | [TP-PID], TP-DCS, [TP-Ud] |
| `L3SMSSubmitReject` | 0x1A | DL | SM-Cause |

### SMS L3 Messages - Service Centre & Notification

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3SMSSFProvidedRep` | 0x1B | UL | [TP-PID], TP-DCS, [TP-Ud] |
| `L3SMSSFProvidedRepAck` | 0x1C | DL | Empty body |
| `L3SMSNotification` | 0x1D | Bidir | [TP-PID], TP-DCS, [TP-Ud] |
| `L3SMSShortCodeInfo` | 0x1E | Bidir | ShortCodeType, [ShortCode] |

---

## 32. Extended PD Messages

**File:** `gsml3parser/extended/l3extendedmessages.h` - 1 placeholder type in the `EXTENDED` variant.
**Spec:** GSM 04.08 §10.2.
**PD:** `0x0e` (Extended).

The Extended PD provides infrastructure for future extended protocol discriminators. The `L3ExtendedMessage` class captures the raw MTI from the L3 header and stores the remaining body as an opaque octet sequence, allowing forward-compatible parsing of unknown extended messages.

| Method | Description |
|--------|-------------|
| `mti()` | Returns the parsed MTI value |
| `pd()` | Returns `L3PD::Extended` |
| `body()` | Raw body bytes |
| `parse(br, parsedMti)` | Static factory; takes pre-parsed MTI |
| `write(bw)` | Serializes body octets |
| `text(os)` | Human-readable dump of MTI and body |

---

## 33. Test Procedure PD Messages

**File:** `gsml3parser/testproc/l3testproceduremessages.h` - 1 placeholder type in the `TESTPROC` variant.
**Spec:** GSM 04.08 §10.2.
**PD:** `0x0f` (TestProcedure).

The Test Procedure PD provides infrastructure for test procedure messages used in network testing scenarios. The `L3TestProcedureMessage` class captures the raw MTI from the L3 header and stores the remaining body as an opaque octet sequence, allowing forward-compatible parsing of test procedure messages.

| Method | Description |
|--------|-------------|
| `mti()` | Returns the parsed MTI value |
| `pd()` | Returns `L3PD::TestProcedure` |
| `body()` | Raw body bytes |
| `parse(br, parsedMti)` | Static factory; takes pre-parsed MTI |
| `write(bw)` | Serializes body octets |
| `text(os)` | Human-readable dump of MTI and body |

---

## 34. MSContext - Per-Subscriber State

MSContext aggregates all state associated with a single mobile station: identity (TMSI/IMSI), channel assignment, classmark, location area, and protocol-layer flags (ciphering, registration, authentication). This is the primary object through which a BTS tracks each subscriber.

**Header:** `#include <gsml3parser/stack/ms_context.h>`

### Performance Characteristics

| Property | Value |
|----------|-------|
| `sizeof(MSContext)` | ≤ 256 bytes (enforced by `static_assert`) |
| Heap allocations | **Zero** - all fields stored inline |
| Hot path methods | O(1), no virtual dispatch, no heap |
| Thread safety | NOT thread-safe. One instance per MS, single-thread access |

Fields are ordered by access frequency: hot fields (identity, channel, flags) first, cold fields (LAI, classmark) last. This layout minimizes cache line misses for the typical message processing path.

### Factory Methods

| Method | Description |
|--------|-------------|
| `MSContext::createWithTMSI(uint32_t tmsi)` | Create context with TMSI identity |
| `MSContext::createWithIMSI(std::string_view imsiDigits)` | Create context with IMSI identity |

### Identity API

| Method | Description |
|--------|-------------|
| `identity()` | Returns `const L3MobileIdentity&` - current primary identity |
| `setTMSI(uint32_t tmsi)` | Update or set TMSI |
| `setIMSI(std::string_view digits)` | Update or set IMSI |

### Channel Assignment API

| Method | Description |
|--------|-------------|
| `channelType()` | Returns current `ChannelType` (or `UndefinedCHType`) |
| `assignChannel(type, trx, ts, arfcn)` | Assign a logical channel with physical parameters |
| `releaseChannel()` | Release channel, resets to `UndefinedCHType` |
| `trxNumber()` | Returns transceiver index |
| `timeslot()` | Returns TDMA timeslot number |
| `arfcn()` | Returns ARFCN value |

### State Flags API

| Method | Description |
|--------|-------------|
| `isCiphered()` / `setCiphered(bool)` | Ciphering active state |
| `isRegistered()` / `setRegistered(bool)` | Location update completed |
| `isAuthenticated()` / `setAuthenticated(bool)` | Authentication performed |
| `timingAdvance()` / `setTimingAdvance(uint8_t)` | Timing advance value (0-63) |
| `classmark()` / `setClassmark(cm)` | MS Classmark 1 storage |
| `lai()` / `setLAI(lai)` | Location Area Identity storage |

### Example

```cpp
#include <gsml3parser/stack/ms_context.h>

using namespace gsml3parser;

// Create context for a known MS
auto ctx = MSContext::createWithTMSI(0x12345678u);

// Assign SDCCH channel after RACH handling
ctx.assignChannel(ChannelType::SDCCHType, /*trx=*/0, /*ts=*/0, /*arfcn=*/125);

// After authentication procedure completes
ctx.setAuthenticated(true);
ctx.setCiphered(true);

// Store classmark received in CM Service Request
L3MobileStationClassmark1 cm;
ctx.setClassmark(cm);

// After location update
L3LocationAreaIdentity lai("262", "42", 1234);
ctx.setLAI(lai);
ctx.setRegistered(true);
```

---

## 35. Timer Framework

The timer framework provides GSM Layer 3 protocol timers as defined in 3GPP TS 24.008 and TS 44.018. It includes timer identifiers, a single-timer class, and a manager that tracks up to 32 concurrent timers per MS using fixed-size arrays (zero heap allocation).

**Header:** `#include <gsml3parser/stack/l3_timer.h>`

### L3TimerId Enumerations

| Timer ID | Default Duration | Description | Spec Reference |
|----------|-----------------|-------------|----------------|
| `T3101` | 3000ms | CM service request retransmission | 3GPP TS 24.008 10.5.4 |
| `T3102` | 3000ms | Identity response retransmission | 3GPP TS 24.008 10.5.6 |
| `T3103` | 5000ms | Location updating request retransmission | 3GPP TS 24.008 10.5.12 |
| `T3106` | 3000ms | Authentication response retransmission | 3GPP TS 24.008 10.5.18 |
| `T3108` | 3000ms | TMSI reallocation complete retransmission | 3GPP TS 24.008 10.5.23 |
| `T3109` | 30000ms | Paging response retransmission (etom × 5s) | 3GPP TS 24.008 10.5.26 |
| `T3111` | 3000ms | CM reestablishment request retransmission | 3GPP TS 24.008 10.5.34 |
| `T3112` | 3000ms | IMSI detach indication retransmission | 3GPP TS 24.008 10.5.36 |
| `T3113` | 3000ms | MM status retransmission | 3GPP TS 24.008 10.5.38 |
| `T3310` | 5000ms | GPRS attach request retransmission | 3GPP TS 24.008 10.5.76 |
| `T3311` | 30000ms | Routing area update retransmission (etor × 5s) | 3GPP TS 24.008 10.5.78 |
| `T3312` | 3000ms | P-TMSI reallocation complete retransmission | 3GPP TS 24.008 10.5.80 |
| `T3314` | 3000ms | GPRS service request retransmission | 3GPP TS 24.008 10.5.84 |
| `T3315` | 3000ms | Authentication and ciphering resp retransmission | 3GPP TS 24.008 10.5.86 |
| `T3320` | 3000ms | Activate PDP context request retransmission | 3GPP TS 24.008 10.5.96 |
| `T3321` | 3000ms | Deactivate PDP context request retransmission | 3GPP TS 24.008 10.5.98 |
| `T3322` | 3000ms | Modify PDP context request retransmission | 3GPP TS 24.008 10.5.100 |
| `T3334` | 3000ms | GMM status retransmission | 3GPP TS 24.008 10.5.112 |
| `T3395` | 3000ms | Packet reservation request retransmission | 3GPP TS 24.008 10.5.130 |

### Free Functions

| Function | Description |
|----------|-------------|
| `l3TimerDefault(L3TimerId)` | Returns the default duration for a timer ID (O(1) lookup) |
| `l3TimerName(L3TimerId)` | Returns human-readable timer name (e.g. "T3101") |

### L3Timer Class

Single timer instance with start/stop/expired semantics.

| Method | Description |
|--------|-------------|
| `L3Timer(id)` | Construct with default expiry from spec |
| `L3Timer(id, expiry)` | Construct with custom expiry duration |
| `start()` | Start or restart the timer; returns true if first start |
| `stop()` | Stop the timer without firing |
| `tick(delta)` | Advance by delta; returns true if expired |
| `isRunning()` | Returns true if timer is currently running |
| `remaining()` | Returns remaining time (zero if not running) |
| `id()` | Returns the timer's L3TimerId |
| `expiry()` | Returns the configured expiry duration |

### TimerManager Class

Manages up to 32 named timers for one MS context using fixed-size arrays.

| Method | Description |
|--------|-------------|
| `start(id)` | Start timer with default expiry; returns true if first start |
| `start(id, expiry)` | Start timer with custom expiry |
| `stop(id)` | Stop a specific timer |
| `stopAll()` | Stop all running timers |
| `tick(delta, callback)` | Advance all timers; invoke callback for each expired (zero allocation) |
| `tick(delta, span)` | Advance all timers; fill pre-allocated span with expired IDs |
| `isRunning(id)` | Check if a specific timer is running |
| `remaining(id)` | Get remaining time for a specific timer |
| `get(id)` | Get the L3Timer object for direct access |
| `runningCount()` | Number of currently running timers |
| `setOwner(void*)` | Set the owning object reported to the active-change observer |
| `setOnActiveChange(fn, ctx)` | Register a zero-alloc observer fired on 0->>0 / >0->0 running-timer transitions |

### Performance Characteristics

| Metric | Value |
|--------|-------|
| Memory footprint | `sizeof(TimerManager)` ≈ 1.2 KB (32 timers × ~36 bytes + 32 bytes init flags + 3 observer words) |
| Heap allocations | **Zero** - all storage is `std::array`; observer is a plain fn pointer + context |
| `tick()` complexity | O(32) = constant, iterates fixed array |
| `start()` / `stop()` | O(1) - direct index into array (plus O(32) running-count check for active-change detection) |
| Thread safety | NOT thread-safe. One instance per MS, single-thread access |

### Example

```cpp
#include <gsml3parser/stack/l3_timer.h>

using namespace gsml3parser;

// --- Callback-based tick (no heap allocation) ---
TimerManager tm;
tm.start(L3TimerId::T3101);  // CM service request, default 3s
tm.start(L3TimerId::T3106, 5000ms); // custom expiry

// In event loop, advance time:
tm.tick(std::chrono::milliseconds(100), [](L3TimerId expiredId) {
    // Handle timer expiry - retransmit or abort procedure
    handleExpired(expiredId);
});

// --- Span-based tick (caller provides buffer) ---
std::array<L3TimerId, 32> expired;
size_t n = tm.tick(std::chrono::milliseconds(500), expired);
for (size_t i = 0; i < n; ++i) {
    handleExpired(expired[i]);
}

// Check individual timer state
if (tm.isRunning(L3TimerId::T3101)) {
    auto remaining = tm.remaining(L3TimerId::T3101);
    // ...
}
```

---

## 36. Transaction Framework

The transaction framework provides request-response correlation for L3 messaging. It tracks outgoing requests and matches incoming responses using either TI (Transaction Identifier) for CC/SS protocols or PD+MTI for other protocol discriminators.

### Overview

| Component | Description |
|-----------|-------------|
| `Transaction` | Single transaction metadata: PD, MTI, TI, timer ID, state |
| `TransactionManager` | Manages up to 16 concurrent transactions per MS |
| `TransactionState` | Pending, Completed, Expired, Cancelled |

### Matching Semantics

| Protocol Discriminator | Matching Strategy | Complexity |
|------------------------|-------------------|------------|
| `CallControl` (PD=0x03) | TI-based lookup via `mTiIndex[8]` | O(1) |
| `NonCallSS` (PD=0x0b) | TI-based lookup via `mTiIndex[8]` | O(1) |
| All other PDs | PD + MTI comparison | O(K), K < 16 |

### API Reference

#### TransactionState

```cpp
enum class TransactionState : uint8_t {
    Pending,      // Waiting for response
    Completed,    // Response received successfully
    Expired,      // Timer expired, no response
    Cancelled     // Manually cancelled
};
```

#### Transaction

```cpp
class Transaction {
public:
    Transaction(L3PD pd, int mti, uint8_t ti, L3TimerId timerId);

    L3PD requestPD() const noexcept;
    int requestMTI() const noexcept;
    uint8_t ti() const noexcept;
    L3TimerId timerId() const noexcept;
    TransactionState state() const noexcept;

    void complete() noexcept;
    void expire() noexcept;
    void cancel() noexcept;

    // Match with TI (for CC/SS protocols)
    bool matches(const ParsedMessage& msg, uint8_t ti) const;

    // Match without TI (for non-CC/SS protocols)
    bool matches(const ParsedMessage& msg) const;

    std::chrono::steady_clock::time_point createdAt() const noexcept;
};
```

#### TransactionManager

```cpp
class TransactionManager {
public:
    TransactionManager() = default;

    // Create a new pending transaction. Returns unique ID or nullopt if full.
    std::optional<uint32_t> create(L3PD pd, int mti, uint8_t ti, L3TimerId timerId);

    // Get transaction by ID. Returns nullptr if not found or not pending.
    Transaction* get(uint32_t id) noexcept;

    // Match with L3 header (full TI support for CC/SS).
    Transaction* match(const L3Header& header, const ParsedMessage& msg);

    // Match without header (PD+MTI scan only).
    Transaction* match(const ParsedMessage& msg);

    // Expire all pending transactions with the given timer ID.
    void onTimerExpired(L3TimerId timerId) noexcept;

    // Remove finished transactions. Returns count removed.
    size_t cleanup() noexcept;

    size_t pendingCount() const noexcept;
    size_t totalCount() const noexcept;
};
```

### Usage Example

```cpp
#include <gsml3parser/stack/transaction.h>

TransactionManager tm;

// BTS sends Setup, expects Connect
auto txId = tm.create(L3PD::CallControl, L3Setup::MTI, /*ti=*/1, L3TimerId::T3101);
// Start timer T3101...

// Later: response arrives
auto header = parseL3Header(incomingBytes).value();
auto msg = parseL3(incomingBytes.subspan(2)).value();

Transaction* tx = tm.match(header, msg);
if (tx) {
    // Correlated with our Setup request!
    tx->complete();
    // Stop timer T3101...
}

// Timer expiry handler:
tm.onTimerExpired(L3TimerId::T3101);
tm.cleanup();
```

### Performance Characteristics

| Metric | Value |
|--------|-------|
| `sizeof(Transaction)` | <= 48 bytes |
| Max concurrent transactions | 16 per MS |
| CC/SS match() complexity | O(1) via TI index |
| Non-CC/SS match() complexity | O(K), K < 16 |
| Heap allocations | None (std::array storage) |
| Thread safety | NOT thread-safe; one instance per MS |

---

## 37. Protocol State Machines

**File:** `gsml3parser/stack/state_machine.h` - FSM base class and RR/MM/CC skeleton implementations.
**Spec:** 3GPP TS 24.008 Chapters 4-6 (RR, MM, CC procedures).

The state machine framework provides a base class for protocol state machines with message and timer event processing, plus concrete skeleton implementations for the three main GSM Layer 3 sublayers: Radio Resource (RR), Mobility Management (MM), and Call Control (CC).

### SMAction - State Machine Actions

| Action | Description |
|--------|-------------|
| `None` | Stay in current state, no side effect |
| `Transition` | Move to the next state specified by `nextState` |
| `SendResponse` | Transition and signal that a response message should be built externally |
| `Reject` | Signal that a reject/status message should be sent; stay in current state |
| `ReleaseChannel` | Signal that the logical channel should be released |
| `PushSubstate` | Push a sub-state machine onto the stack (for nested procedures) |
| `PopSubstate` | Pop back to the parent state machine |

### SMResult - Processing Result

The `SMResult` struct is returned from every `processMessage()` and `processTimer()` call:

```cpp
struct SMResult {
    SMAction action{SMAction::None};
    std::optional<int> nextState;
    bool causesTransition() const noexcept;
};
```

**Important:** `SMResult` does NOT contain `ParsedMessage`. The FSM returns action + next state; the caller builds response messages externally via Builder API. `sizeof(SMResult) <= 16 bytes`.

### ProtocolStateMachine - Base Class

```cpp
class ProtocolStateMachine {
public:
    virtual ~ProtocolStateMachine() = default;
    void setState(int state);
    int state() const;
    SMResult processMessage(const ParsedMessage& msg);
    SMResult processTimer(L3TimerId timerId);
    virtual std::string_view debugName() const = 0;

protected:
    virtual SMResult handle_message_impl(int state, const ParsedMessage& msg) = 0;
    virtual SMResult handle_timer_impl(int state, L3TimerId timerId) = 0;
};
```

### RRStateMachine States

| State | Description |
|-------|-------------|
| `IDLE` | No dedicated channel assigned |
| `CHANNEL_REQUESTED` | RACH received, waiting for AGCH |
| `CHANNEL_ASSIGNED` | Immediate Assignment sent, waiting for SABM |
| `LINK_ESTABLISHED` | LAPDm link established (UA received) |
| `WAITING_MM` | Waiting for MM procedure (CM Service Request) |
| `ACTIVE` | Full communication on DCCH |
| `CIPHER_MODE` | Ciphering mode procedure in progress |
| `HANDOVER` | Handover procedure in progress |
| `CHANNEL_RELEASE` | Channel release in progress |

**Default RR transitions:**
```
IDLE + ChannelRequest           -> CHANNEL_REQUESTED
CHANNEL_ASSIGNED + PagingResp   -> WAITING_MM
LINK_ESTABLISHED + (any MM msg) -> WAITING_MM
WAITING_MM + CMServiceAccept    -> ACTIVE
ACTIVE + ChannelRelease         -> CHANNEL_RELEASE
ACTIVE + HandoverCommand        -> HANDOVER
CIPHER_MODE + CipherModeComplete-> ACTIVE
T3109 expiry (CHANNEL_ASSIGNED) -> CHANNEL_RELEASE
```

### MMStateMachine States

| State | Description |
|-------|-------------|
| `DEREGISTERED`    | MS not registered in VLR |
| `SERVICE_REQUEST` | CM Service Request received |
| `WAITING_IDENTITY`| Identity Request sent, awaiting response |
| `IDENTITY_VERIFIED`| IMSI/TMSI known and verified |
| `AUTHENTICATION`  | Authentication Request sent, awaiting response |
| `AUTHENTICATED`   | Authentication complete |
| `LOCATION_UPDATE` | Location Updating in progress |
| `REGISTERED`      | Fully registered, ready for calls |

**Default MM transitions:**
```
DEREGISTERED + CMServiceRequest       -> SERVICE_REQUEST
SERVICE_REQUEST + IdentityResponse    -> IDENTITY_VERIFIED
IDENTITY_VERIFIED + AuthResponse      -> AUTHENTICATED
AUTHENTICATION + AuthResponse         -> AUTHENTICATED
AUTHENTICATED + LocationUpdatingReq   -> LOCATION_UPDATE
LOCATION_UPDATE + CMServiceAccept     -> REGISTERED
T3101/T3102 expiry (SERVICE_REQUEST)  -> DEREGISTERED
T3106 expiry (AUTHENTICATION)         -> DEREGISTERED
```

### CCStateMachine States

| State | Description |
|-------|-------------|
| `IDLE`                | No call in progress |
| `SETUP_RECEIVED`      | Setup message received |
| `PROCEEDING`          | Call Proceeding sent |
| `ALERTING`            | Alerting sent |
| `CONNECT`             | Connect received |
| `ACTIVE`              | Bidirectional speech path established |
| `DISCONNECT_RECEIVED` | Disconnect received |
| `RELEASE`             | Release in progress |

**Default CC transitions:**
```
IDLE + Setup              -> SETUP_RECEIVED
SETUP_RECEIVED            -> PROCEEDING (auto-transition)
PROCEEDING + Alerting     -> ALERTING
ALERTING + Connect        -> CONNECT
CONNECT + CallConfirmed   -> ACTIVE
ACTIVE + Disconnect       -> DISCONNECT_RECEIVED
DISCONNECT_RECEIVED       -> RELEASE (auto-transition)
T3101 expiry (SETUP_RECEIVED/PROCEEDING/ALERTING) -> IDLE
```

### Usage Example

```cpp
#include <gsml3parser/stack/state_machine.h>

// Use the default RR state machine
RRStateMachine rrFsm;
rrFsm.setState(RRStateMachine::State::IDLE);

// Process incoming messages
auto msg = parseL3(buffer).value();
SMResult result = rrFsm.processMessage(msg);

if (result.action == SMAction::Transition) {
    int newState = result.nextState.value();
    // Handle state change, build response messages as needed
}

// Custom FSM: inherit and override specific transitions
class MyRRFSM : public RRStateMachine {
protected:
    SMResult handle_message_impl(int state, const ParsedMessage& msg) override {
        if (state == State::ACTIVE && messagePD(msg) == L3PD::MobilityManagement) {
            return {SMAction::SendResponse, static_cast<int>(State::CIPHER_MODE)};
        }
        return RRStateMachine::handle_message_impl(state, msg);
    }
};
```

### Performance Characteristics

| Metric | Value |
|--------|-------|
| `sizeof(SMResult)` | <= 16 bytes |
| Message dispatch | O(1) via switch(PD) + switch(MTI), compile-time resolved |
| Heap allocations | None on hot path |
| Virtual dispatch | Only at base class level; derived impl uses switch statements |
| Thread safety | NOT thread-safe; one instance per MS |

---

## 38. Channel Pool - Logical Channel Management

**File:** `gsml3parser/stack/channel_pool.h`

Manages a pool of logical channels for BTS operation. Provides O(1) channel allocation via per-type free-lists, Request Reference (RA) decoding from RACH bursts, and Very Early Assignment (VEA) fallback logic.

### ChannelDescriptor

Describes a logical channel with type, transceiver index, timeslot, and ARFCN.

```cpp
struct ChannelDescriptor {
    ChannelType type{ChannelType::UndefinedCHType};
    uint8_t trxNumber{};
    uint8_t timeslot{};
    uint16_t arfcn{};
    bool operator==(const ChannelDescriptor&) const = default;
};
```

### decodeChannelNeeded()

Decodes the Request Reference (RA) byte from a RACH burst into the required channel type, following GSM 04.08 Table 9.9.

```cpp
ChannelType decodeChannelNeeded(uint8_t ra, bool neci = false, bool vea = false);
```

| Parameter | Description |
|-----------|-------------|
| `ra` | 8-bit Request Reference from Channel Request message |
| `neci` | Non-Extended Channel Indicator (reserved for future extended decoding) |
| `vea` | Very Early Assignment enabled; when true, MO calls get TCH directly |

**Establishment cause mapping (bits 6-5 of RA):**

| Bits 6-5 | Cause | Channel (VEA=off) | Channel (VEA=on) |
|----------|-------|-------------------|-------------------|
| `00` | MO Call | SDCCH | TCHF |
| `01` | Emergency Call | TCHF | TCHF |
| `10` | Answer to Paging | TCHF | TCHF |
| `11` | Location Updating | SDCCH | SDCCH |

### isLocationUpdatingRequest()

```cpp
bool isLocationUpdatingRequest(uint8_t ra, bool neci = false);
```

Returns `true` when establishment cause bits (6-5) equal `11`.

### ChannelPool

```cpp
class ChannelPool {
public:
    void addChannel(ChannelDescriptor desc);
    std::optional<ChannelDescriptor> allocate(ChannelType type);
    bool release(const ChannelDescriptor& desc);
    bool removeChannel(const ChannelDescriptor& desc);
    bool isFree(const ChannelDescriptor& desc) const;
    size_t freeCount(ChannelType type) const;
    size_t totalCount() const;
    size_t allocatedCount(ChannelType type) const;
    std::optional<ChannelDescriptor> allocateVEA(uint8_t ra);
    std::vector<ChannelDescriptor> freeChannels(ChannelType type) const;
};
```

| Method | Description | Complexity |
|--------|-------------|------------|
| `addChannel(desc)` | Register a channel (cold path, init time) | O(1) amortized |
| `allocate(type)` | Pop from per-type free-list | O(1) |
| `release(desc)` | Return channel to free-list | O(1) find/erase (per-type unordered_set) + O(1) push |
| `removeChannel(desc)` | Permanently remove from pool | O(1) if allocated; O(N) free-list scan if free (cold path) |
| `isFree(desc)` | Check if channel is available | O(N) |
| `freeCount(type)` | Number of free channels of type | O(1) |
| `totalCount()` | Total channels (free + allocated) | O(T) |
| `allocatedCount(type)` | In-use channels of type | O(1) |
| `allocateVEA(ra)` | VEA: try TCH, fall back SDCCH | O(1) |
| `freeChannels(type)` | List all free channels (diagnostic) | O(1) copy |

### VEA Procedure

Very Early Assignment (GSM 05.08) skips the intermediate SDCCH assignment for MO calls:

```cpp
ChannelPool pool;
pool.addChannel({ChannelType::TCHFType, 1, 0, 200});
pool.addChannel({ChannelType::SDCCHType, 0, 0, 100});

// RA=0x00 (MO call): VEA allocates TCHF directly
auto ch = pool.allocateVEA(0x00);
// ch->type == ChannelType::TCHFType

// After TCH exhausted, falls back to SDCCH
pool.allocate(ChannelType::TCHFType); // consume the TCH
auto fallback = pool.allocateVEA(0x00);
// fallback->type == ChannelType::SDCCHType
```

### Usage Example

```cpp
#include <gsml3parser/stack/channel_pool.h>

using namespace gsml3parser;

// Initialize channel pool at BTS startup
ChannelPool pool;
pool.addChannel({ChannelType::SDCCHType, 0, 0, 100});
pool.addChannel({ChannelType::TCHFType, 1, 0, 200});
pool.addChannel({ChannelType::TCHFType, 1, 1, 201});

// Handle incoming Channel Request on RACH
uint8_t ra = 0x03; // establishment cause from RACH burst
ChannelType needed = decodeChannelNeeded(ra, false, true);

auto ch = pool.allocate(needed);
if (ch) {
    // Build ImmediateAssignment with allocated channel
    // Send to MS...
}

// After call completed, release the channel
pool.release(*ch);
```

### Performance Characteristics

| Metric | Value |
|--------|-------|
| `sizeof(ChannelDescriptor)` | 8 bytes (with padding) |
| allocate() complexity | O(1) - vector pop_back on free-list |
| release() complexity | O(1) - unordered_set find/erase on allocated set + free-list push |
| Heap allocations | Amortized O(1) on hot path (allocate/release). addChannel() grows internal vectors; the allocated set may rehash as channels are allocated. |
| Thread safety | NOT thread-safe; caller must provide synchronization for multi-threaded access |

---

## 39. FlatHandler - Zero-Overhead Callbacks

**File:** `gsml3parser/flat_handler.h`

`FlatHandler` replaces `std::function<void(const ParsedMessage&, void*)>` with a zero-overhead callback type. Each instance is exactly two machine words (16 bytes on 64-bit), compared to 40+ bytes for `std::function`. Invocations use direct function pointer calls — no virtual dispatch, no type erasure overhead.

### FlatHandler Struct

```cpp
struct FlatHandler {
    using Callback = void (*)(const ParsedMessage*, void*);

    Callback fn{nullptr};
    void* ctx{nullptr};

    constexpr FlatHandler() noexcept = default;
    constexpr FlatHandler(Callback f, void* c) noexcept;

    void operator()(const ParsedMessage& msg, void* userCtx = nullptr) const;
    bool operator==(const FlatHandler& other) const noexcept;
    explicit operator bool() const noexcept;
};

static_assert(sizeof(FlatHandler) == 2 * sizeof(void*));
```

| Method | Description | Complexity |
|--------|-------------|------------|
| `operator()(msg, ctx)` | Invoke handler. Direct function pointer call. | O(1) single indirect call |
| `operator==` | Compare two handlers (fn + ctx equality) | O(1) |
| `operator bool` | True if fn is not nullptr | O(1) |

### Factory Functions

#### makeHandler (Zero Allocation)

For non-capturing lambdas (stateless callables):

```cpp
template<typename F>
    requires (std::is_invocable_v<F, const ParsedMessage&, void*> &&
              std::is_default_constructible_v<F> &&
              !std::is_pointer_v<F> &&
              !std::is_function_v<std::decay_t<F>>)
FlatHandler makeHandler(F) noexcept;
```

No heap allocation. The lambda is converted to a function pointer at compile time.
Function pointers and function types are rejected (C10): a default-constructed
function pointer is null and would crash on the first invocation. For plain
function pointers with a user context, use the constructor directly:
`FlatHandler h{myFunction, myCtx};`

#### makeSharedHandler (Heap Allocation)

For capturing lambdas:

```cpp
template<typename F>
    requires std::is_invocable_v<F, const ParsedMessage&, void*>
FlatHandler makeSharedHandler(F f);
```

Stores the capturing lambda in a `shared_ptr`-controlled heap allocation. The `shared_ptr` is accessible through the handler's `ctx` pointer.

#### destroySharedHandler

```cpp
void destroySharedHandler(FlatHandler& h);
```

Destroys the heap allocation created by `makeSharedHandler`. Call when replacing or removing a handler to avoid memory leaks. Safe to call on empty handlers. The `ProtocolDispatcher` destructor calls this automatically for all registered handlers.

### Usage Example

```cpp
#include <gsml3parser/flat_handler.h>

// Non-capturing: zero allocation
FlatHandler h1 = makeHandler([](const ParsedMessage& msg, void*) {
    std::cout << messageName(msg) << "\n";
});
h1(parsedMsg);

// Capturing: heap allocation via shared_ptr
int count = 0;
FlatHandler h2 = makeSharedHandler([&count](const ParsedMessage& msg, void*) {
    ++count;
});
h2(parsedMsg);
```

### Performance Characteristics

| Metric | Value |
|--------|-------|
| `sizeof(FlatHandler)` | 16 bytes (two pointers) vs 40+ bytes for std::function |
| Invocation overhead | Single indirect call vs virtual dispatch + type erasure |
| Default construction | Zero-cost (nullptr, nullptr) |

---

## 40. ShardedChannelPool - Thread-Safe Channel Pool

**File:** `gsml3parser/stack/sharded_channel_pool.h`

Thread-safe sharded channel pool for high-concurrency BTS scenarios. Partitions channels across N shards using a hash of the channel descriptor. Each shard has its own `std::shared_mutex`, so threads operating on different shards never contend.

### Template Parameter

```cpp
template<int N = 16>
class ShardedChannelPool;
```

| Parameter | Constraint | Description |
|-----------|------------|-------------|
| `N` | Power of two, ≥ 2 | Number of shards (commonly 4, 8, 16, or 32) |

Explicit template instantiations are provided for N = 4, 8, 16, and 32.

### API

```cpp
void addChannel(ChannelDescriptor desc);
std::optional<ChannelDescriptor> allocate(ChannelType type);
bool release(const ChannelDescriptor& desc);
size_t freeCount(ChannelType type) const;
size_t totalCount() const;
```

| Method | Description | Complexity | Lock Type |
|--------|-------------|------------|-----------|
| `addChannel(desc)` | Add channel to the hashed shard | O(1) hash + lock | Exclusive |
| `allocate(type)` | Round-robin shard start, then remaining shards (fallback) | O(N) worst case | Exclusive per shard |
| `release(desc)` | Return channel to its shard | O(1) hash + lock | Exclusive |
| `freeCount(type)` | Total free across all shards | O(N) | Shared |
| `totalCount()` | Total channels across all shards | O(N) | Shared |

### Hash Function

The shard index is computed from `(trxNumber, timeslot, arfcn)` using a simple XOR-shift hash followed by a bitmask (`hash & (N-1)`). This ensures:
- O(1) computation (no modulo division)
- Deterministic: same channel always maps to same shard
- Even distribution for typical BTS channel layouts

### Allocation Strategy

`allocate(type)` does not know which channel will be taken, only its type, so the descriptor hash cannot be used for routing. Instead it starts probing from a **round-robin shard** (an atomic counter masked by `N-1`), which spreads lock contention evenly across shards instead of concentrating it on shard 0, then falls back to the remaining shards. The fallback is required because channels are hash-distributed across shards at `addChannel()` time, so a free channel of the requested type may reside in any shard. Complexity is O(N) in the worst case (all shards probed), typically terminating on the first or second shard.

### Thread Safety

All public methods are thread-safe. Multiple threads can concurrently call `allocate()`, `release()`, `addChannel()` etc. Read-only operations use shared locks (`std::shared_lock`) for better concurrency.

**Performance:** With N=16 or N=32 and millions of channels, expected contention is negligible when the hash distributes work evenly.

### Usage Example

```cpp
#include <gsml3parser/stack/sharded_channel_pool.h>
#include <thread>

using namespace gsml3parser;

ShardedChannelPool<16> pool;

// Pre-populate channels
for (int trx = 0; trx < 4; ++trx) {
    for (int ts = 0; ts < 16; ++ts) {
        pool.addChannel({ChannelType::SDCCHType, static_cast<uint8_t>(trx),
                         static_cast<uint8_t>(ts), 100 + trx * 16 + ts});
    }
}

// Concurrent allocation from multiple threads
std::vector<std::thread> workers;
for (int i = 0; i < 8; ++i) {
    workers.emplace_back([&pool]() {
        auto ch = pool.allocate(ChannelType::SDCCHType);
        if (ch) {
            // Use channel...
            pool.release(*ch);
        }
    });
}
for (auto& t : workers) t.join();
```

### Performance Characteristics

| Metric | Value |
|--------|-------|
| `sizeof(ShardedChannelPool<16>)` | 16 × sizeof(ChannelPool + shared_mutex) |
| Hash computation | O(1) bitmask, no division |
| Contention | Only when two threads hash to the same shard |

---

## 41. InlineFramer - Zero-Copy Frame Extraction

**File:** `gsml3parser/bitstream/inline_framer.h` (header-only)

Zero-copy L3 frame extractor for contiguous memory buffers. Unlike `L3Framer` which reads from a `ByteSource` and copies data into an internal buffer, `InlineFramer` operates directly on the caller's `std::span`, yielding non-owning views into the original data with zero allocations.

### API

```cpp
class InlineFramer {
public:
    explicit InlineFramer(std::span<const uint8_t> data, bool useL2Length = false);

    [[nodiscard]] std::optional<std::span<const uint8_t>> nextFrame() noexcept;
    [[nodiscard]] constexpr size_t remaining() const noexcept;
    void reset() noexcept;
    void setMaxFrameLength(size_t len) noexcept;
};
```

| Method | Description | Complexity |
|--------|-------------|------------|
| `nextFrame()` | Extract next frame as a span into original data | O(1) for L2 length, O(L) for header-based scanning |
| `remaining()` | Unconsumed bytes | O(1) |
| `reset()` | Reset to beginning of buffer | O(1) |

### Framing Modes

| Mode | Description |
|------|-------------|
| **L2 length** (`useL2Length=true`) | Each frame preceded by a single length octet. Most common for test data and buffered streams. |
| **Header-based** (`useL2Length=false`) | Frame length derived from PD+MTI fixed-length table, or next-header scanning for variable-length messages. |

### Usage Example

```cpp
#include <gsml3parser/bitstream/inline_framer.h>

// Buffer with L2-framed data: [len][msg...][len][msg...]
std::vector<uint8_t> buffer = {3, 0x60, 0x0D, 0x00, 2, 0x50, 0x84};

InlineFramer framer(std::span{buffer}, true /* L2 length mode */);

while (auto frame = framer.nextFrame()) {
    // 'frame' is a span into the original buffer — zero copies!
    auto msg = parseL3(*frame);
    if (msg) {
        std::cout << messageName(*msg) << "\n";
    }
}
```

### Thread Safety

NOT thread-safe. One instance per buffer, single-threaded use. Safe for concurrent use when each thread has its own `InlineFramer` instance operating on separate memory regions.

---

## 42. ZeroCopyStreamProcessor - Zero-Copy Stream Parsing

**File:** `gsml3parser/bitstream/zero_copy_processor.h` (header-only)

Zero-copy L3 stream processor for contiguous buffers. Parses L3 messages directly from a memory span without any intermediate copying. Ideal for high-throughput scenarios where data arrives in large contiguous chunks (e.g., memory-mapped files, DMA buffers).

### API

```cpp
class ZeroCopyStreamProcessor {
public:
    explicit ZeroCopyStreamProcessor(std::span<const uint8_t> data, bool useL2Length = false);

    [[nodiscard]] std::optional<ParsedMessage> nextMessage();

    template<typename F>
        requires std::is_invocable_v<F, const ParsedMessage&>
    void forEach(F&& handler);

    [[nodiscard]] const StreamStats& stats() const noexcept;
    void resetStats() noexcept;
    [[nodiscard]] size_t remaining() const noexcept;
    void reset() noexcept;
};
```

| Method | Description |
|--------|-------------|
| `nextMessage()` | Parse next L3 message. Returns `std::nullopt` when buffer exhausted or parse error. |
| `forEach(handler)` | Bulk process all remaining messages, invoking handler for each. |
| `stats()` | Access accumulated stream statistics (frame count, per-PD counts, errors). |
| `remaining()` | Unconsumed bytes in the buffer. |
| `reset()` | Reset to beginning of buffer. |

### Usage Example

```cpp
#include <gsml3parser/bitstream/zero_copy_processor.h>

std::vector<uint8_t> data = { /* L2-framed L3 messages */ };

ZeroCopyStreamProcessor proc(std::span{data}, true);

// Iterate one by one
while (auto msg = proc.nextMessage()) {
    std::cout << messageName(*msg) << "\n";
}

// Or bulk process with forEach
proc.reset();
size_t count = 0;
proc.forEach([&count](const ParsedMessage& msg) {
    (void)msg;
    ++count;
});
std::cout << "Parsed " << count << " messages\n";
```

### Performance

Zero-copy parsing avoids the `memcpy` overhead of `L3StreamProcessor`'s internal buffering. For large contiguous buffers, expect 2-5x speedup depending on frame sizes and CPU cache characteristics.

### Thread Safety

NOT thread-safe. Safe for concurrent use when each thread has its own instance operating on separate memory regions.

---

## 43. Subscriber Registry - Subscriber Session Management

**File:** `gsml3parser/stack/subscriber_registry.h`

Manages per-subscriber sessions for software BTS implementations. Each session aggregates MSContext, RR/MM/CC state machines, timers, transactions, and a ProcedureRunner into a single object indexed by TMSI, IMSI, and LAPDm link.

### SubscriberSession

Aggregates all per-MS state:

| Member | Type | Purpose |
|--------|------|---------|
| `context` | `MSContext` | Identity, channel, flags (≤256B) |
| `rrSM` | `RRStateMachine` | Radio Resource FSM |
| `mmSM` | `MMStateMachine` | Mobility Management FSM |
| `ccSM` | `CCStateMachine` | Call Control FSM |
| `timers` | `TimerManager` | Protocol timers (≤32 concurrent) |
| `transactions` | `TransactionManager` | Request-response correlation |
| `procedures` | `ProcedureRunner` | Active protocol procedures |
| `response` | `ResponseContext` | Parameters for building protocol responses (populated by the active procedure; consumed by `ResponseBuilder::buildResponseFromToken`) |
| `channel` | `optional<ChannelDescriptor>` | Current channel assignment |
| `lapdmLink` | `uint8_t` | LAPDm link ID for routing |
| `assignedTmsi` | `uint32_t` | TMSI key in the owning registry (set by `createByTMSI`/`createByIMSI`; 0 = not owned) |

Size: `sizeof(SubscriberSession) < 4096 bytes`, all components inline.

### SubscriberRegistry API

```cpp
// Create sessions by identity.
SubscriberSession* createByTMSI(uint32_t tmsi);
SubscriberSession* createByIMSI(std::string_view imsi);

// Lookup by any index.
SubscriberSession* findByTMSI(uint32_t tmsi) noexcept;
SubscriberSession* findByIMSI(std::string_view imsi) noexcept;
SubscriberSession* findByLink(uint8_t trx, uint8_t ts, uint8_t lapdmLink) noexcept;

// Channel management (maintains link index).
void assignChannel(SubscriberSession* session, ChannelDescriptor desc, uint8_t lapdmLink);
void releaseChannel(SubscriberSession* session);

// Lifecycle.
// remove() is O(1): it derives the TMSI key from session->assignedTmsi and
// looks it up directly (no linear scan over all sessions).
bool remove(SubscriberSession* session) noexcept;
void clear() noexcept;
size_t count() const noexcept;

// Timer management.
// tickAllTimers() is O(active): it ticks only sessions with at least one running
// timer (active-timer index), not all sessions. Sessions register/unregister
// themselves automatically when their first timer starts / last timer stops.
size_t tickAllTimers(std::chrono::milliseconds delta, std::span<L3TimerId> expiredOut);

// Iterate all sessions (guaranteed single visit).
template<typename F> void forEach(F&& callback);
```

### ShardedSubscriberRegistry

Thread-safe variant that partitions sessions across N shards:

```cpp
ShardedSubscriberRegistry<16> registry;  // 16 shards

// Thread-safe operations.
auto* s1 = registry.createByTMSI(0x12345678);
auto* s2 = registry.findByTMSI(0x12345678);  // shared_lock
registry.remove(s1);  // unique_lock
```

Explicit instantiations provided for N=4, 8, 16, 32.

### Example

```cpp
SubscriberRegistry registry;
auto* session = registry.createByTMSI(0x12345678);

// Assign channel when MS seizes SDCCH.
ChannelDescriptor ch{ChannelType::SDCCHType, 0, 5, 100};
registry.assignChannel(session, ch, /*lapdmLink=*/3);

// Route incoming message to session via link.
auto* target = registry.findByLink(0, 5, 3);

// Tick all timers.
std::array<L3TimerId, 256> expired;
size_t n = registry.tickAllTimers(100ms, expired);

// Clean up when procedure completes.
registry.remove(session);
```

---

## 44. RSL Types - A-bis RSL Type Definitions

**Header:** `include/gsml3parser/abis/rsl_types.h`
**Source:** `src/abis/rsl_types.cpp`
**3GPP:** TS 48.058 (A-bis interface)

Defines all RSL enumerations and structures for A-bis message parsing and construction.

### Discriminators

| Enum | Value | Description |
|------|-------|-------------|
| `RSLDiscriminator::RLL` | `0x00` | Radio Link Layer (L3 data transport) |
| `RSLDiscriminator::CommonChannel` | `0x40` | CCHAN - common channel control |
| `RSLDiscriminator::DedicatedChannel` | `0x60` | DCHAN - dedicated channel control |
| `RSLDiscriminator::TRX` | `0xa0` | Transceiver-level management |
| `RSLDiscriminator::IPAccess` | `0xc0` | ip.access vendor-specific |

### RLL Message Types

| Enum | Value | Direction | Description |
|------|-------|-----------|-------------|
| `RSLL3MessageType::DataReq` | `0x21` | BSC->BTS | Numbered L3 data |
| `RSLL3MessageType::DataInd` | `0x22` | BTS->BSC | Numbered L3 data |
| `RSLL3MessageType::UnitDataReq` | `0x41` | BSC->BTS | Unnumbered L3 data |
| `RSLL3MessageType::UnitDataInd` | `0x42` | BTS->BSC | Unnumbered L3 data |

### DCHAN Message Types

| Enum | Value | Direction | Description |
|------|-------|-----------|-------------|
| `RSLDChanMessageType::ChanActiv` | `0x01` | BSC->BTS | Channel activation |
| `RSLDChanMessageType::ChanActivAck` | `0x11` | BTS->BSC | Activation ACK |
| `RSLDChanMessageType::ChanActivNack` | `0x12` | BTS->BSC | Activation NACK |
| `RSLDChanMessageType::RFChanRelAck` | `0x15` | BTS->BSC | Release ACK |
| `RSLDChanMessageType::MeasRes` | `0x24` | BTS->BSC | Measurement result |

### CCHAN Message Types

| Enum | Value | Direction | Description |
|------|-------|-----------|-------------|
| `RSLCChanMessageType::BCCHInfo` | `0x01` | BSC->BTS | System information |
| `RSLCChanMessageType::PagingCmd` | `0x03` | BSC->BTS | Paging command |
| `RSLCChanMessageType::CCCHLoadInd` | `0x13` | BTS->BSC | CCCH load report |
| `RSLCChanMessageType::ChanRqd` | `0x16` | BTS->BSC | Channel required |

### Information Elements

The `RSL_IE` enum defines 24 IE types (ChanNr, LinkIdent, ActType, ChanMode, EncrInfo, L3Info, etc.).
L3Info uses TL16V encoding (16-bit length) for large payloads.

### Structures

- **`RSLChannelNumber`** - Encode/decode dedicated channel numbers. Static methods: `encode(cbits, ts)`, `getCBits()`, `getTimeslot()`, `isDedicated()`. Constants: `BCCH=0x00`, `RACH=0x40`, `PCH_AGCH=0x60`.
- **`RSLChannelMode`** - 5-byte channel mode (spdInd, chanRT, dtxDTU, chanRate). Methods: `isSignalling()`, `isSpeech()`, `isData()`.
- **`RSLEncryptionInfo`** - algorithmId + key span for A5 ciphering.

### Helper Functions

- `rslDiscriminatorName(disc)` -> string_view
- `rslIEName(ie)` -> string_view
- `rslErrorCauseName(cause)` -> string_view

---

## 45. RSL Parser - A-bis RSL Message Parsing

**Header:** `include/gsml3parser/abis/rsl_parser.h`
**Source:** `src/abis/rsl_parser.cpp`
**3GPP:** TS 48.058 (A-bis interface)

Zero-heap-allocation parser for A-bis RSL messages. Extracts L3 payloads from RLL and control messages.

### `RSLParsedMessage`

Fixed-size result struct (~544 bytes on 64-bit). Contains:
- `discriminator`, `msgType`, `chanNr`, `linkId` - header fields
- `l3Payload` - extracted L3 bytes (span into original buffer)
- `informationElements[MAX_IE=32]` - parsed TLV IEs (pointers into original buffer)
- `rawData` - full message span for debugging

### `RSLParser::parse(data)`

Parse raw RSL bytes into structured message. Returns `Expected<RSLParsedMessage>`.
Validates discriminator, header size, and TLV boundaries.

### `RSLParser::extractL3(parsed)`

Return L3 payload from parsed message. Returns `optional<span<const uint8_t>>`.

### `RSLParser::findIE(parsed, ieType)`

Find IE by type code. Returns pointer or nullptr.

### `RSLParser::getChannelMode(parsed)`

Extract ChannelMode from CHAN_ACTIV. Returns `optional<RSLChannelMode>`.

### `RSLParser::getEncryptionInfo(parsed)`

Extract encryption parameters from ENCR_CMD. Returns `optional<RSLEncryptionInfo>`.

---

## 46. RSL Builder - A-bis RSL Message Construction

**Header:** `include/gsml3parser/abis/rsl_builder.h`
**Source:** `src/abis/rsl_builder.cpp`
**3GPP:** TS 48.058 (A-bis interface)

Constructs serialized RSL messages for BTS->BSC communication. Every method has both vector and span overloads for zero-allocation building.

### RLL Messages

- `buildDataReq(chanNr, linkId, l3Payload)` - Encapsulate L3 in DATA_REQ
- `buildDataInd(chanNr, linkId, l3Payload)` - Encapsulate L3 in DATA_IND
- `buildUnitDataReq(chanNr, linkId, l3Payload)` - Connectionless DATA_REQ
- `buildUnitDataInd(chanNr, linkId, l3Payload)` - Connectionless DATA_IND

### DCHAN Messages

- `buildChanActivAck(chanNr, frameNumber)` - Channel activation acknowledgment
- `buildChanActivNack(chanNr, cause)` - Channel activation rejection
- `buildRFChanRelAck(chanNr)` - RF channel release acknowledgment
- `buildConnFail(chanNr, cause)` - Connection failure report
- `buildMeasRes(chanNr, measNr, rxlev, rxqual, l1Info)` - Measurement results
- `buildHandoDet(chanNr, accessDelay)` - Handover detection

### CCHAN Messages

- `buildCCCHLoadInd(chanNr, pagingLoad, rachTotal, rachBusy, rachAccess)` - CCCH load statistics
- `buildChanRqd(chanNr, reqRef, accessDelay)` - Channel required from RACH
- `buildDeleteInd(chanNr, fullImmAssInfo)` - Immediate assignment deletion

### Span Overloads

Every method has a `static int buildXxx(std::span<uint8_t> out, ...)` overload that writes directly into a pre-allocated Arena buffer, returning byte count or -1 on error.

---

## 47. Procedure Framework - Protocol Procedure Base Class

**File:** `gsml3parser/stack/procedure.h`
**Namespace:** `gsml3parser`
**Spec:** 3GPP TS 24.008 (procedure lifecycle), TS 04.08

Abstract base class for protocol procedures and the ResponseSink callback mechanism. Each procedure encapsulates a complete GSM Layer 3 protocol flow with its own internal state machine, timers, and message sequence logic. BTS applications feed incoming L3 messages via `feed()`, receive external data via `feedExternalTyped()`, and manage timeouts via `tick()`.

### ResponseSink

Zero-overhead callback type for building protocol responses without heap allocation on the hot path (`gsml3parser/stack/response_sink.h`):

```cpp
struct ResponseSink {
    using Callback = void (*)(SMAction, const ParsedMessage&, const SubscriberSession*, void*);
    Callback fn{nullptr};
    void* ctx{nullptr};
    // RAII copy/move with atomic refcounting for capturing lambdas
    void operator()(SMAction action, const ParsedMessage& msg, const SubscriberSession* session) const;
    explicit operator bool() const noexcept;
};

static_assert(sizeof(ResponseSink) == 2 * sizeof(void*)); // 16 bytes on 64-bit

template<typename F>
ResponseSink makeResponseSink(F f); // wraps a capturing callable (one heap allocation at creation)
```

The BTS application provides this callback when calling `Procedure::feed()`. Inside the callback, invoke `ResponseBuilder` and write bytes into a pre-allocated buffer (Arena).

The sink is an observability hook, not the response-building mechanism itself: it is invoked only from `feed()` with the real incoming message. `feedExternalTyped()` never invokes the sink — on that path the response to send is signaled by the `ResponseToken` in the returned `ProcedureStepResult`.

**Performance:** `ResponseSink` is exactly two machine words (16 bytes) with direct function-pointer invocation — no virtual dispatch, no type erasure, no per-call heap allocation (replaces `std::function`, which was 40+ bytes and could heap-allocate). Capturing lambdas are wrapped with `makeResponseSink()` (one heap allocation at creation, shared by all copies via an atomic refcount); stateless callbacks use the zero-allocation two-argument constructor `ResponseSink{fn, ctx}`.

### ResponseToken

Enum that indicates which L3 message type the caller should build when `ProcedureStepResult.action == SendResponseWithToken`. Used with `ResponseBuilder::buildResponseFromToken()` to generate response bytes in a pre-allocated Arena buffer (zero heap allocation).

```cpp
enum class ResponseToken : uint8_t {
    None = 0,
    // RR responses:
    ImmediateAssignment, AssignmentCommand, ChannelRelease,
    CipheringModeCommand, PhysicalInformation, HandoverCommand,
    PagingRequestType1, PagingRequestType2, PagingRequestType3,
    // MM responses:
    CMServiceAccept, CMServiceReject, IdentityRequest,
    AuthenticationRequest, LocationUpdatingAccept, LocationUpdatingReject,
    TMSIReallocationCommand,
    // CC responses:
    CallProceeding, Alerting, Connect, ConnectAcknowledge,
    Disconnect, Release, ReleaseComplete, Setup,
};
```

| Token | Message | Spec |
|-------|---------|------|
| `ImmediateAssignment` | ImmediateAssignment | TS 04.08 9.1.19 |
| `AssignmentCommand` | AssignmentCommand | TS 04.08 9.1.2 |
| `ChannelRelease` | ChannelRelease | TS 04.08 9.1.7 |
| `CipheringModeCommand` | CipheringModeCommand | TS 04.08 9.1.9 |
| `PhysicalInformation` | PhysicalInformation | TS 04.08 9.1.12 |
| `HandoverCommand` | HandoverCommand | TS 04.08 9.1.40 |
| `PagingRequestType1/2/3` | PagingRequestType1/2/3 | TS 04.08 9.1.25 |
| `CMServiceAccept` | CM Service Accept | TS 04.08 9.2.5 |
| `CMServiceReject` | CM Service Reject | TS 04.08 9.2.6 |
| `IdentityRequest` | IdentityRequest | TS 04.08 9.2.10 |
| `AuthenticationRequest` | AuthenticationRequest | TS 04.08 9.2.2 |
| `LocationUpdatingAccept` | LocationUpdatingAccept | TS 04.08 9.2.13 |
| `LocationUpdatingReject` | LocationUpdatingReject | TS 04.08 9.2.14 |
| `TMSIReallocationCommand` | TMSIReallocationCommand | TS 04.08 9.2.17 |
| `CallProceeding` | CallProceeding | TS 04.08 9.3.3 |
| `Alerting` | Alerting | TS 04.08 9.3.1 |
| `Connect` | Connect | TS 04.08 9.3.5 |
| `ConnectAcknowledge` | ConnectAcknowledge | TS 04.08 9.3.6 |
| `Disconnect` | Disconnect | TS 04.08 9.3.7 |
| `Release` | Release | TS 04.08 9.3.19 |
| `ReleaseComplete` | ReleaseComplete | TS 04.08 9.3.19 |
| `Setup` | Setup | TS 24.008 9.3.2 |

**Memory:** `sizeof(ResponseToken) == 1` byte (uint8_t). Fits inline in `ProcedureStepResult` without exceeding the 32-byte budget.

### ProcedureStepResult

Terminal result from each procedure step:

```cpp
struct ProcedureStepResult {
    enum class Action : uint8_t {
        Continue,               ///< Procedure continues; awaiting next message
        SendResponseWithToken,  ///< Build response using responseToken + ResponseBuilder
                                 ///< (kept even when the procedure terminates in this step)
        WaitingExternal,        ///< Needs external data (RAND from AuC, BSC decision)
        Completed,              ///< Procedure finished successfully (no response pending)
        Failed                  ///< Procedure terminated with an error (no response pending)
    };

    Action action{Action::Continue};
    ResponseToken responseToken{ResponseToken::None};  ///< Which message to build when action == SendResponseWithToken
    procedure::ProcedureResult finalResult{};  ///< Sole terminal indicator
};

static_assert(sizeof(ProcedureStepResult) <= 32);
```

| Field | Type | Description |
|-------|------|-------------|
| `action` | `Action` (uint8_t) | What the caller should do next |
| `responseToken` | `ResponseToken` (uint8_t) | Which L3 message to build; valid when `action == SendResponseWithToken` |
| `finalResult` | `ProcedureResult` | Sole terminal indicator; `state` is Completed/Failed/TimedOut when the procedure is done |

**Response/terminal rule:** if a procedure must send a response, `action == SendResponseWithToken` is ALWAYS the case — even when the procedure reaches a terminal state in the same step. The terminal state is reported exclusively through `finalResult` (state == Completed/Failed). Caller contract:

1. if `action == SendResponseWithToken` → build the response from `responseToken` (`ResponseBuilder::buildResponseFromToken`);
2. if `finalResult.state` is terminal (Completed/Failed/TimedOut) → release the procedure.

A result may therefore carry both a token to build and a terminal `finalResult` (e.g. LocationUpdate VLR-accept: `action == SendResponseWithToken`, `responseToken == LocationUpdatingAccept`, `finalResult.state == Completed`).

**Memory:** `sizeof(ProcedureStepResult) <= 32` bytes. The `responseToken` field (1 byte) replaces the old `SendResponse` action, keeping the struct within the cache-line budget for millions of concurrent calls.

### Procedure Base Class

Abstract interface that all concrete procedures implement:

| Method | Description |
|--------|-------------|
| `type()` | Returns the `procedure::ProcedureType` identifier |
| `state()` | Returns the current `procedure::ProcedureState` |
| `feed(msg, session, sink)` | Process an incoming L3 message; returns step result |
| `matches(msg)` | Report whether this procedure accepts the given message (PD + MTI). Used by `ProcedureRunner` for precise routing when several procedures are active; base implementation returns `false` |
| `feedExternalTyped(data, session, sink)` | Provide typed external data (`AuthChallenge`, `VLRDecision`, etc.); resume procedure. `session` (nullable) lets the procedure populate the session's `ResponseContext` with the parameters the resulting response needs |
| `tick(delta)` | Advance procedure timers; may return `Failed` on timeout |
| `cancel()` | Explicitly abort the procedure |

**Thread safety:** NOT thread-safe. One instance per logical procedure, single-thread access.
**Memory:** Zero heap allocations for state. Pre-allocated buffers for RAND/SRES data.

### Usage Example

```cpp
#include <gsml3parser/stack/procedure.h>

using namespace gsml3parser;

auto proc = ProcedureFactory::createLocationUpdate();
auto result = proc->feed(incomingMsg, session,
    makeResponseSink([](SMAction action, const ParsedMessage& msg, const SubscriberSession* sess) {
        // Build response via ResponseBuilder into Arena buffer
    }));

if (result.action == ProcedureStepResult::Action::Completed) {
    std::cout << "Procedure finished: " << result.finalResult.reason << "\n";
}
```

---

## 48. Procedure Runner - Concurrent Procedure Manager

**File:** `gsml3parser/stack/procedure_runner.h`
**Namespace:** `gsml3parser`
**Spec:** 3GPP TS 24.008 (procedure orchestration)

Manages concurrent protocol procedures for a single subscriber. Routes incoming L3 messages to the correct active procedure based on Protocol Discriminator (PD), auto-creates new procedures when no matching procedure is active, and automatically cleans up completed/failed procedure slots for reuse.

### ProcedureRunner API

```cpp
class ProcedureRunner {
public:
    ProcedureRunner() = default;

    /// Feed incoming L3 message; routes to active procedure or starts new one.
    /// Routing: the first active procedure whose matches(msg) returns true
    /// receives the message (disambiguates procedures sharing a PD, e.g.
    /// CallRelease vs CallSetup_MO); if none matches, the message is routed by
    /// PD and a new procedure is auto-created when no slot for that PD exists.
    ProcedureStepResult feed(const ParsedMessage& msg, SubscriberSession* session,
                                ResponseSink sink);

    /// Feed typed external data to an active procedure by type.
    /// @p session is passed through to the procedure so it can populate the
    /// session's ResponseContext with the response parameters it learns.
    ProcedureStepResult feedExternalTyped(procedure::ProcedureType type,
                                              SubscriberSession* session,
                                              const ExternalData& data,
                                              ResponseSink sink = {});

    /// Tick all active procedures; auto-cleans terminal slots.
    size_t tickAll(std::chrono::milliseconds delta);

    /// Get active procedure by type, or nullptr.
    [[nodiscard]] Procedure* getActive(procedure::ProcedureType type) noexcept;

    /// Number of currently active procedures.
    [[nodiscard]] size_t activeCount() const noexcept;

    /// Cancel all active procedures and free their slots.
    void cancelAll() noexcept;
};
```

| Method | Description | Slot Cleanup |
|--------|-------------|--------------|
| `feed(msg, session, sink)` | Route message to active procedure (via `matches()`, then PD fallback) or auto-create. Starting a new procedure resets the session's `ResponseContext` | Auto-frees on Completed/Failed |
| `feedExternalTyped(type, session, data, sink)` | Send typed external data (`ExternalData` variant) by ProcedureType; `session` (nullable) is forwarded to the procedure for `ResponseContext` population | Auto-frees on Completed/Failed |
| `tickAll(delta)` | Advance all timers; returns count of Failed procedures | Auto-frees terminal slots |
| `getActive(type)` | Lookup running procedure | None |
| `activeCount()` | Count active slots | None |
| `cancelAll()` | Cancel and free all slots | Frees all |

**Internal storage:** Fixed `std::array<ProcedureSlot, 8>` — no heap allocation for the runner itself. Procedures are heap-allocated via `unique_ptr`.

### ProcedureFactory

Static factory for creating specific procedure instances:

| Factory Method | Returns | Spec |
|----------------|---------|------|
| `createLocationUpdate()` | `LocationUpdateProcedure` | TS 24.008 4.4.1 |
| `createAuthentication()` | `AuthenticationProcedure` | TS 24.008 4.4.2 |
| `createCipheringMode(algo)` | `CipheringModeProcedure` | TS 24.008 4.4.3 |
| `createCallSetupMO()` | `CallSetupMOPercedure` | TS 24.008 6.1 |
| `createCallSetupMT(number)` | `CallSetupMTPercedure` | TS 24.008 6.1 |
| `createChannelAssignment(type)` | `ChannelAssignmentProcedure` | TS 04.08 9.1.2 |
| `createPaging(identity)` | `PagingProcedure` | TS 04.08 9.1.25 |
| `createHandover(target)` | `HandoverProcedure` | TS 04.08 9.1.40 |
| `createCallRelease(ti, cause)` | `CallReleaseProcedure` | TS 24.008 6.1 |
| `createIMSIDetach()` | `IMSIDetachProcedure` | TS 24.008 4.4.6 |

### Usage Example

```cpp
#include <gsml3parser/stack/procedure_runner.h>

using namespace gsml3parser;

SubscriberRegistry registry;
auto* session = registry.createByTMSI(0x12345678);
ProcedureRunner runner;

// Feed incoming message — procedure auto-created and started.
Arena arena(65536);
auto result = runner.feed(incomingMsg, session,
    [&](SMAction action, const ParsedMessage& msg, const SubscriberSession* sess) {
        uint8_t buf[512];
        int n = ResponseBuilder::buildCMServiceAccept({buf, sizeof(buf)});
        if (n > 0) sendToMS(buf, n);
    });

// Feed typed external decision from VLR (session lets the procedure record
// newTmsi/rejectCause into session->response for the Accept/Reject build).
VLRDecision vlr{true, 0x87654321u, MMRejectCause::Zero};
runner.feedExternalTyped(procedure::ProcedureType::LocationUpdate, session, vlr);

// Tick timers every 100ms.
size_t failed = runner.tickAll(std::chrono::milliseconds(100));
```

---

## 49. Typed External Data - Strongly-Typed External Data Structures

**File:** `gsml3parser/stack/typed_external_data.h`
**Namespace:** `gsml3parser`
**Spec:** 3GPP TS 24.008 (authentication, location update), TS 04.08 (paging, ciphering)

Replaces the opaque `std::span<const uint8_t>` parameter with named, self-documenting structures. Each structure corresponds to a specific type of external data that a BTS application feeds into a procedure via `feedExternalTyped()`. All structures are small (≤ 64 bytes) and passed by const reference to avoid copies.

### AuthChallenge

Authentication challenge data from AuC. Contains RAND (128-bit per TS 24.008) and expected SRES for verification.

```cpp
struct AuthChallenge {
    std::array<uint8_t, 16> rand{};       // 128-bit RAND per GSM spec
    std::array<uint8_t, 4> expectedSres{}; // Expected signed response
};
static_assert(sizeof(AuthChallenge) <= 32);
```

### VLRDecision

VLR decision for location update accept/reject. When accept is true, optional newTmsi may be assigned to the MS. When accept is false, rejectCause indicates the reason.

```cpp
struct VLRDecision {
    bool accept{false};
    std::optional<uint32_t> newTmsi;
    MMRejectCause rejectCause{MMRejectCause::Zero};
};
```

### PagingTrigger

Trigger data for network-initiated paging. Contains the mobile identity to page and the target channel type.

```cpp
struct PagingTrigger {
    L3MobileIdentity identity;
    ChannelType targetChannel{ChannelType::SDCCHType};
};
```

### CipheringParameters

Ciphering activation parameters. `algorithmSelector` determines the ciphering algorithm (0=A5/0, 1=A5/1, etc.).

```cpp
struct CipheringParameters {
    uint8_t algorithmSelector{0};
    bool enableCiphering{true};
};
static_assert(sizeof(CipheringParameters) <= 16);
```

### HandoverTarget

Handover target cell and channel description. Used by the network to instruct MS to switch to a new channel.

```cpp
struct HandoverTarget {
    L3ChannelDescription channel;
    L3CellDescription cell;
};
```

### ExternalData Variant

Unified variant for all external data types. Always pass by `const&` to avoid copying the variant (~64 bytes).

```cpp
using ExternalData = std::variant<
    AuthChallenge,
    VLRDecision,
    PagingTrigger,
    CipheringParameters,
    HandoverTarget
>;
```

### Usage Example

```cpp
#include <gsml3parser/stack/typed_external_data.h>

// Feed authentication challenge from AuC:
AuthChallenge chal{};
std::memcpy(chal.rand.data(), aucRand, 16);
std::memcpy(chal.expectedSres.data(), aucSres, 4);
auto result = orchestrator.feedExternalTyped(chal);

// Feed VLR accept decision:
VLRDecision vlr{true, 0x12345678u, MMRejectCause::Zero};
auto result2 = orchestrator.feedExternalTyped(vlr);
```

### Performance Characteristics

| Metric | Value |
|--------|-------|
| `sizeof(AuthChallenge)` | ≤ 32 bytes |
| `sizeof(CipheringParameters)` | ≤ 16 bytes |
| `sizeof(ExternalData)` | ~64 bytes (dominated by largest alternative) |
| Heap allocations | Zero when passed by const reference |

---

## 50. ProcedureStateMixin - CRTP Mixin for Common Procedure Code

**File:** `gsml3parser/stack/procedure_state_mixin.h` (header-only template)
**Namespace:** `gsml3parser`
**Spec:** 3GPP TS 24.008 - Procedure lifecycle and timer management

CRTP mixin that eliminates ~50 lines of duplicated code per procedure (`transitionTo`, `fail`, `complete`, `startTimer`, `stopTimer`, `tick`, `cancel`). Each derived procedure inherits from both `Procedure` and `ProcedureStateMixin<XxxProcedure, State>` via multiple inheritance.

### Template Signature

```cpp
template<typename Derived, typename StateEnum>
class ProcedureStateMixin {
protected:
    StateEnum mCurrentState{};
    procedure::ProcedureState mProcState{procedure::ProcedureState::Initiated};
    std::chrono::milliseconds mTimerRemaining{0};
    L3TimerId mCurrentTimer{L3TimerId::Unknown};
    bool mTimerRunning{false};

    void transitionTo(StateEnum s);
    void fail(std::string_view reason);
    void complete();
    void startTimer(L3TimerId id, std::chrono::milliseconds duration);
    void stopTimer() noexcept;

public:
    [[nodiscard]] ProcedureStepResult doTick(std::chrono::milliseconds delta);
    void doCancel() noexcept;
};
```

### Derived Class Hooks

Each procedure that uses the mixin must implement three private hooks:

| Hook | Called By | Purpose |
|------|-----------|---------|
| `doTransitionTo(State s)` | `transitionTo()` | Map internal state to `ProcedureState` |
| `doFail(std::string_view reason)` | `fail()`, `doTick()` (timeout) | Transition to FAILED state |
| `doComplete()` | `complete()` | Transition to COMPLETED state |

### Usage Pattern

```cpp
class LocationUpdateProcedure : public Procedure,
                                 public ProcedureStateMixin<LocationUpdateProcedure, State> {
public:
    [[nodiscard]] ProcedureStepResult tick(std::chrono::milliseconds delta) override {
        return static_cast<ProcedureStateMixin<LocationUpdateProcedure, State>&>(*this).doTick(delta);
    }
    void cancel() noexcept override {
        static_cast<ProcedureStateMixin<LocationUpdateProcedure, State>&>(*this).doCancel();
    }
private:
    void doTransitionTo(State s) { /* map State -> ProcedureState */ }
    void doFail(std::string_view reason) { mCurrentState = State::FAILED; }
    void doComplete() { mCurrentState = State::COMPLETED; }
};
```

### Performance Characteristics

| Metric | Value |
|--------|-------|
| Code size reduction | ~50 lines per procedure eliminated |
| Memory overhead | ~20 bytes per procedure instance (state + timer fields) |
| Runtime overhead | Zero — all methods resolved at compile time via CRTP |
| Header-only | Yes — no .cpp file needed |

---

## 51. Procedure Orchestrator - Chained Protocol Procedures

**File:** `gsml3parser/stack/procedure_orchestrator.h` / `gsml3parser/stack/procedure_orchestrator.cpp`
**Namespace:** `gsml3parser`
**Spec:** 3GPP TS 24.008 - Compound procedure chains (Location Update, Call Setup)

Manages compound procedure chains such as Location Update (CMServiceRequest -> Identity -> Authentication -> CipheringMode -> LocationUpdate) and Call Setup MO (CMServiceRequest -> CallSetupMO). The orchestrator owns a single active `Procedure` at any time, transitions between phases based on procedure outcomes, and updates the `SubscriberSession` FSM states to stay in sync.

**Does NOT store `ParsedMessage` (416-byte variant) internally.** Instead stores the last `ResponseToken` and provides `buildPendingResponse()` for zero-allocation response building. This is critical for high-load BTS: avoids heap allocation per response.

### API

```cpp
class ProcedureOrchestrator {
public:
    ProcedureOrchestrator() = default;

    [[nodiscard]] ProcedureStepResult feed(const ParsedMessage& msg, SubscriberSession* session);
    [[nodiscard]] ProcedureStepResult feedExternalTyped(const ExternalData& data, ResponseSink sink = {});
    size_t tickAll(std::chrono::milliseconds delta);
    void cancelAll() noexcept;

    [[nodiscard]] Procedure* activeProcedure() noexcept;
    [[nodiscard]] const Procedure* activeProcedure() const noexcept;
    [[nodiscard]] ResponseToken lastResponseToken() const noexcept;
    [[nodiscard]] int buildPendingResponse(std::span<uint8_t> out,
                                             const SubscriberSession* session) const;
    [[nodiscard]] procedure::ProcedureType chainPhase() const noexcept;
};
```

| Method | Description |
|--------|-------------|
| `feed(msg, session)` | Feed incoming L3 message; orchestrator auto-chains sub-procedures. The orchestrator keeps the session and forwards it to the active procedure (including `feedExternalTyped`), so procedures can populate `session->response` |
| `feedExternalTyped(data, sink)` | Feed typed external data to the active chain (AuthChallenge, VLRDecision, etc.); the stored session is passed to the procedure for `ResponseContext` population |
| `tickAll(delta)` | Tick the active procedure's timers plus the inline-phase timer (T3103, 5 s, started when the LocationUpdate phase begins); returns count of failed procedures. A phase-timer expiry fails the chain with `finalResult.state == TimedOut` |
| `cancelAll()` | Cancel all active procedures in the chain, stop the phase timer, and reset the session's `ResponseContext` |
| `activeProcedure()` | Get the current active procedure, or nullptr if in an inline phase |
| `lastResponseToken()` | Get the last response token generated by the chain |
| `buildPendingResponse(out, session)` | Build the pending response into a pre-allocated buffer (zero heap allocation) |
| `chainPhase()` | Get the current chain phase (ProcedureType) |

**Identity phase semantics:** while the chain awaits an `IdentityResponse`, `feed()` returns `SendResponseWithToken(IdentityRequest)`. Once the `IdentityResponse` arrives, the chain transitions to Authentication and returns `Continue` (no response) — an `IdentityRequest` is never sent after the identity has already been received.

### Supported Chains

| Chain Type | Phases |
|------------|--------|
| Location Update | CMServiceRequest -> [IdentityVerification] -> Authentication -> CipheringMode -> LocationUpdate |
| Call Setup MO | CMServiceRequest -> CallSetupMO |
| Call Setup MT | Paging -> ChannelAssignment -> CallSetupMT |
| IMSI Detach | CMServiceRequest -> IMSIDetach |
| Call Release | Disconnect -> Release -> ReleaseComplete |

### Usage Example

```cpp
#include <gsml3parser/stack/procedure_orchestrator.h>

using namespace gsml3parser;

ProcedureOrchestrator orchestrator;
auto result = orchestrator.feed(incomingMsg, session);

if (result.action == ProcedureStepResult::Action::SendResponseWithToken) {
    uint8_t buf[512];
    int n = orchestrator.buildPendingResponse({buf, sizeof(buf)}, session);
    if (n > 0) sendToRadio(buf, n);
}

// Feed external data when procedure enters WaitingExternal:
if (result.action == ProcedureStepResult::Action::WaitingExternal) {
    AuthChallenge chal{};
    // ... fill chal from AuC ...
    auto r2 = orchestrator.feedExternalTyped(chal);
}
```

### Performance Characteristics

| Metric | Value |
|--------|-------|
| Heap allocations during feed() | Zero (uses unique_ptr for current procedure only) |
| ParsedMessage storage | None — stores only ResponseToken (1 byte) |
| Thread safety | NOT thread-safe; one instance per subscriber chain |

---

## 52. Location Update Procedure

**File:** `gsml3parser/stack/procedures/location_update.h`
**Spec:** 3GPP TS 24.008 4.4.1

Full location updating flow with identity check, optional authentication, VLR/BSC decision via `feedExternalTyped(VLRDecision, session)`, and TMSI reallocation. On the VLR decision the procedure records `newTmsi`/`rejectCause` into `session->response` so the Accept/Reject can be built from real parameters.

### State Machine

| State | Trigger | Next State | Response |
|-------|---------|------------|----------|
| `INIT` | CMServiceRequest/PagingResponse | `IDENTITY_CHECK` | — |
| `IDENTITY_CHECK` | TMSI known | `AUTH_CHECK` | — |
| `IDENTITY_CHECK` | TMSI unknown | `REQUEST_IDENTITY` | `buildIdentityRequest(IMSI)` |
| `REQUEST_IDENTITY` | IdentityResponse | `AUTH_CHECK` | — |
| `AUTH_CHECK` | Auth needed | `SEND_AUTH` | — |
| `AUTH_CHECK` | Auth skipped | `LU_REQUEST` | — |
| `SEND_AUTH` | — | `WAIT_AUTH` (T3106 started) | `buildAuthenticationRequest(rand)` |
| `WAIT_AUTH` | AuthenticationResponse | `VERIFY_AUTH` | — |
| `VERIFY_AUTH` | SRES matches | `LU_REQUEST` | — |
| `VERIFY_AUTH` | SRES mismatch | `REJECT` | — |
| `LU_REQUEST` | — | `WAITING_EXTERNAL` | — (forward to VLR) |
| `WAITING_EXTERNAL` | feedExternalTyped: accept | `SEND_ACCEPT` | `buildLocationUpdatingAccept(lai, newTmsi)` |
| `WAITING_EXTERNAL` | feedExternalTyped: reject | `SEND_REJECT` | `buildLocationUpdatingReject(cause)` |
| Any | Timer expired | `FAILED` | — |

### Internal State

| Member | Type | Purpose |
|--------|------|---------|
| `mRandBuffer` | `array<uint8_t, 16>` | Pre-allocated RAND from AuC (128-bit per TS 24.008) |
| `mExpectedSRES` | `array<uint8_t, 4>` | Expected SRES for verification |
| `mLAI` | `L3LocationAreaIdentity` | LAI from MSContext |
| `mNewTmsi` | `optional<uint32_t>` | New TMSI assignment from VLR |

### Timers

| Timer | Default | Used During |
|-------|---------|-------------|
| `T3106` | 3000ms | Authentication phase |
| `T3103` | 5000ms | Location update request |
| `T3108` | 3000ms | TMSI reallocation complete |

---

## 53. Authentication Procedure

**File:** `gsml3parser/stack/procedures/authentication.h`
**Spec:** 3GPP TS 24.008 4.4.2

Standalone authentication exchange: receives RAND+SRES from AuC via `feedExternalTyped(AuthChallenge, session)`, sends AuthenticationRequest, verifies MS response. The received RAND is copied into `session->response.rand` (`hasRand = true`) so the AuthenticationRequest is built from the real AuC RAND, never a fabricated one.

### State Machine

| State | Trigger | Next State | Response |
|-------|---------|------------|----------|
| `INIT` | feedExternalTyped (AuthChallenge) | `SEND_AUTH_REQ` | — |
| `SEND_AUTH_REQ` | — | `WAIT_RESPONSE` (T3106 started) | `buildAuthenticationRequest(rand)` |
| `WAIT_RESPONSE` | AuthenticationResponse | `VERIFY_SRES` | — |
| `VERIFY_SRES` | SRES matches | `COMPLETED` | — |
| `VERIFY_SRES` | SRES mismatch | `FAILED` | — |

### Internal State

| Member | Type | Purpose |
|--------|------|---------|
| `mRandBuffer` | `array<uint8_t, 16>` | RAND from AuC (128-bit per TS 24.008) |
| `mExpectedSRES` | `array<uint8_t, 4>` | Expected SRES for comparison |

### Timer

| Timer | Default | Used During |
|-------|---------|-------------|
| `T3106` | 3000ms | Authentication response wait |

---

## 54. Call Setup MO Procedure

**File:** `gsml3parser/stack/procedures/call_setup_mo.h`
**Spec:** 3GPP TS 24.008 6.1

Mobile-Originated Call establishment: CMServiceAccept through Setup, Proceeding, TCH assignment, Alerting, Connect, to Active speech path.

### State Machine

| State | Trigger | Next State | Response |
|-------|---------|------------|----------|
| `INIT` | CMServiceRequest(Call) | `SERVICE_ACCEPT` | — |
| `SERVICE_ACCEPT` | — | `WAIT_SETUP` | `buildCMServiceAccept()` |
| `WAIT_SETUP` | Setup (T3101 started) | `PROCEEDING` | — |
| `PROCEEDING` | — | `ASSIGN_TCH` | `buildCallProceeding(ti)` |
| `ASSIGN_TCH` | — | `WAIT_ASSIGN_COMPLETE` | `buildAssignmentCommand(channel)` |
| `WAIT_ASSIGN_COMPLETE` | AssignmentComplete | `ALERTING` | — |
| `ALERTING` | — | `CONNECT` | `buildAlerting(ti)` |
| `CONNECT` | — | `ACTIVE` | `buildConnect(ti)` |
| `ACTIVE` | ConnectAcknowledge | `COMPLETED` | — |

### Internal State

| Member | Type | Purpose |
|--------|------|---------|
| `mTI` | `uint8_t` | Transaction Identifier from Setup |

### Timers

| Timer | Default | Used During |
|-------|---------|-------------|
| `T3101` | 3000ms | Call setup phases (Setup, Assignment) |

---

## 55. Call Setup MT Procedure

**File:** `gsml3parser/stack/procedures/call_setup_mt.h`
**Spec:** 3GPP TS 24.008 6.1

Mobile-Terminated Call establishment: Paging (up to 3 attempts with T3109), SDCCH assignment, Setup delivery, CallConfirmed, TCH assignment, to Active speech path.

### State Machine

| State | Trigger | Next State | Response |
|-------|---------|------------|----------|
| `INIT` | feedExternalTyped (PagingTrigger) | `PAGE` | — |
| `PAGE` | — | `WAIT_PAGE_RESPONSE` (T3109 started) | `buildPagingRequestType1/2/3()` |
| `WAIT_PAGE_RESPONSE` | PagingResponse | `ASSIGN_SDCCH` | — |
| `ASSIGN_SDCCH` | — | `SEND_SETUP` | `buildImmediateAssignment(channel)` |
| `SEND_SETUP` | — | `WAIT_CONFIRMED` (T3101 started) | `buildSetup(calledNumber)` |
| `WAIT_CONFIRMED` | CallConfirmed | `ASSIGN_TCH` | — |
| Remaining states | Same as MO | `COMPLETED` | Same responses as MO |

### Internal State

| Member | Type | Purpose |
|--------|------|---------|
| `mCalledNumber` | `std::string` | Dialed number for Setup message |
| `mTI` | `uint8_t` | Transaction Identifier |
| `mPageAttempt` | `uint8_t` | Current paging attempt (max 3) |

### Timers

| Timer | Default | Used During |
|-------|---------|-------------|
| `T3109` | 30000ms | Paging response wait |
| `T3101` | 3000ms | Call setup phases |

---

## 56. Channel Assignment Procedure

**File:** `gsml3parser/stack/procedures/channel_assignment.h`
**Spec:** 3GPP TS 04.08 9.1.2 / 9.1.35

Channel assignment: receives ChannelRequest or PagingResponse, sends ImmediateAssignment, waits for MS to seize the channel.

### State Machine

| State | Trigger | Next State | Response |
|-------|---------|------------|----------|
| `INIT` | ChannelRequest/PagingResponse | `ALLOCATE_CHANNEL` | — |
| `ALLOCATE_CHANNEL` | pool.allocate(type) | `SEND_IMMEDIATE_ASSIGNMENT` | — |
| `SEND_IMMEDIATE_ASSIGNMENT` | — | `WAIT_SEIZURE` (T3101 started) | `buildImmediateAssignment(channel, ta)` |
| `WAIT_SEIZURE` | First L3 msg on new channel | `COMPLETED` | — |
| `WAIT_SEIZURE` | T3101 expired | `FAILED` (release channel) | — |

### Internal State

| Member | Type | Purpose |
|--------|------|---------|
| `mTargetType` | `ChannelType` | Target channel type to assign |

### Timer

| Timer | Default | Used During |
|-------|---------|-------------|
| `T3101` | 3000ms | Channel seizure wait |

---

## 57. Ciphering Mode Procedure

**File:** `gsml3parser/stack/procedures/ciphering_mode.h`
**Spec:** 3GPP TS 24.008 4.4.3 / TS 04.08 9.1.37

Short procedure to activate ciphering: receives algorithm and key via `feedExternalTyped(CipheringParameters, session)`, sends CipheringModeCommand, waits for CipheringModeComplete from MS. The algorithm selector is recorded into `session->response.cipherAlgo`.

### State Machine

| State | Trigger | Next State | Response |
|-------|---------|------------|----------|
| `INIT` | feedExternalTyped (CipheringParameters) | `SEND_COMMAND` | — |
| `SEND_COMMAND` | — | `WAIT_COMPLETE` | `buildCipheringModeCommand(algo)` |
| `WAIT_COMPLETE` | CipheringModeComplete | `COMPLETED` | — |

### Internal State

| Member | Type | Purpose |
|--------|------|---------|
| `mCipherAlgo` | `uint8_t` | Algorithm ID (0=A5/0, 1=A5/1, etc.) |

---

## 58. Paging Procedure

**File:** `gsml3parser/stack/procedures/paging.h`
**Spec:** 3GPP TS 04.08 9.1.25

Network-initiated paging of MS with up to 3 attempts (Type1, Type2, Type3) using T3109 timer between each attempt. Created via `ProcedureFactory::createPaging()`, not through auto-start.

### State Machine

| State | Trigger | Next State | Response |
|-------|---------|------------|----------|
| `INIT` | feedExternalTyped (PagingTrigger) | `SEND_PAGE1` | — |
| `SEND_PAGE1` | — | `WAIT_PAGE1` (T3109 started) | PagingRequestType1 |
| `WAIT_PAGE1` | T3109 expired | `SEND_PAGE2` | — |
| `SEND_PAGE2` | — | `WAIT_PAGE2` (T3109 restarted) | PagingRequestType2 |
| `WAIT_PAGE2` | T3109 expired | `SEND_PAGE3` | — |
| `SEND_PAGE3` | — | `WAIT_PAGE3` (T3109 restarted) | PagingRequestType3 |
| `WAIT_PAGE3` | PagingResponse | `COMPLETED` | — |
| `WAIT_PAGE3` | T3109 expired | `FAILED` | — |

### Internal State

| Member | Type | Purpose |
|--------|------|---------|
| `mIdentity` | `L3MobileIdentity` | Identity to page |
| `mPageAttempt` | `uint8_t` | Current attempt (max 3) |

---

## 59. Handover Procedure

**File:** `gsml3parser/stack/procedures/handover.h`
**Spec:** 3GPP TS 04.08 9.1.40

Handover: receives target channel via `feedExternalTyped(HandoverTarget, session)`, sends HandoverCommand, waits for HandoverComplete or HandoverFailure from MS. The target channel is recorded into `session->response.hoChannel`. After completion, updates MSContext with new channel assignment.

### State Machine

| State | Trigger | Next State | Response |
|-------|---------|------------|----------|
| `INIT` | feedExternalTyped (HandoverTarget) | `SEND_HO_CMD` | — |
| `SEND_HO_CMD` | — | `WAIT_HO_COMPLETE` (T3101 started) | `buildHandoverCommand(target)` |
| `WAIT_HO_COMPLETE` | HandoverComplete | `COMPLETED` | — |
| `WAIT_HO_COMPLETE` | HandoverFailure | `FAILED` | — |
| `WAIT_HO_COMPLETE` | T3101 expired | `FAILED` | — |

### Internal State

| Member | Type | Purpose |
|--------|------|---------|
| `mTargetChannel` | `L3ChannelDescription` | Target channel for handover |

---

## 60. Call Release Procedure

**File:** `gsml3parser/stack/procedures/call_release.h` / `call_release.cpp`
**Spec:** 3GPP TS 24.008 6.1

Call release procedure for terminating an active call. Manages the Disconnect -> Release -> ReleaseComplete message exchange.

### State Machine

| State | Trigger | Next State | Response |
|-------|---------|------------|----------|
| `INIT` | Disconnect message from MS | `SEND_RELEASE` | — |
| `SEND_RELEASE` | — | `WAIT_RELEASE_COMPLETE` (T3101 started) | `buildRelease(ti, cause)` via `ResponseToken::Release` |
| `WAIT_RELEASE_COMPLETE` | ReleaseComplete from MS | `COMPLETED` | — |
| `WAIT_RELEASE_COMPLETE` | T3101 expired | `FAILED` | — |

### Internal State

| Member | Type | Purpose |
|--------|------|---------|
| `mTI` | `uint8_t` | Transaction Identifier from Disconnect message |
| `mCause` | `CCCause` | CC cause code for Release message |

### Timer

| Timer | Default | Used During |
|-------|---------|-------------|
| `T3101` | 3000ms | Release Complete wait |

---

## 61. IMSI Detach Procedure

**File:** `gsml3parser/stack/procedures/imsi_detach.h` / `imsi_detach.cpp`
**Spec:** 3GPP TS 24.008 4.4.6

IMSI detach procedure: MS signals intent to detach from the network. BTS sends CMServiceAccept and waits for confirmation or timeout.

### State Machine

| State | Trigger | Next State | Response |
|-------|---------|------------|----------|
| `INIT` | IMSIDetachIndication from MS | `SEND_CM_SERVICE_ACCEPT` | — |
| `SEND_CM_SERVICE_ACCEPT` | — | `WAIT_DETACH_COMPLETE` (T3112 started) | `buildCMServiceAccept()` via `ResponseToken::CMServiceAccept` |
| `WAIT_DETACH_COMPLETE` | Confirmation or any L3 message | `COMPLETED` | — |
| `WAIT_DETACH_COMPLETE` | T3112 expired | `COMPLETED` | — |

### Timer

| Timer | Default | Used During |
|-------|---------|-------------|
| `T3112` | 3000ms | Detach confirmation wait |

---

## 62. Performance Optimizations Summary

The following optimizations have been applied to achieve high-throughput, low-latency L3 parsing at scale:

### Fixed-Array Handler Tables (ProtocolDispatcher)

`std::unordered_map<HandlerKey, MessageHandler>` replaced with `std::array<std::array<MessageHandler, 256>, 16>`. Eliminates hash computation, node allocation, and pointer chasing on the dispatch hot path. The handler table is ~64 KB and fits in L2 cache.

### FlatHandler (Zero-Overhead Callbacks)

`std::function<void(const ParsedMessage&, void*)>` replaced with `FlatHandler` — a two-word struct with direct function pointer calls. Factory functions `makeHandler()` and `makeSharedHandler()` provide ergonomic lambda wrapping.

### ResponseSink (Zero-Overhead Procedure Callbacks)

`std::function` replaced with `ResponseSink` — a two-word struct (16 bytes) with direct function pointer calls, used by `Procedure::feed()`/`feedExternalTyped()` and the runner/orchestrator. `makeResponseSink()` wraps capturing lambdas with one heap allocation at creation (refcounted, shared by copies); invocation is allocation-free.

### Fixed-Array Channel Pool

`ChannelPool` uses `std::array<std::vector<ChannelDescriptor>, 32>` (free-lists) + `std::array<std::unordered_set<ChannelDescriptor>, 32>` (allocated tracking) instead of `std::unordered_map<ChannelType, vector>`. O(1) direct index lookup, O(1) `allocate()` (vector pop_back) and O(1) `release()` (unordered_set find/erase).

### ShardedChannelPool (Thread-Safe Concurrency)

`ShardedChannelPool<N>` partitions channels across N shards with per-shard `std::shared_mutex`. Enables near-linear scaling with thread count when the hash distributes work evenly.

### RingBuffer Bitmask Optimization

Ring buffer index wrap-around uses `idx & mMask` (1 CPU cycle) instead of `idx % mCapacity` (20-80 CPU cycles). Capacity is rounded up to the next power of two; one slot is sacrificed for full/empty distinction (standard lock-free ring buffer pattern).

### Zero-Copy Stream Processing

`InlineFramer` and `ZeroCopyStreamProcessor` operate directly on caller-owned memory spans, eliminating intermediate buffer copies. `L3StreamProcessor::processOne()` is now a template method, allowing compiler inlining of the handler lambda.

### Template-Based processOne

`L3StreamProcessor::processOne(F&& handler)` is a template method with C++20 concepts constraint. The compiler can inline and optimize the handler call, eliminating `std::function` type-erasure overhead for this hot path.

---

## Conformance Notes

The library implements encodings defined by:

| Standard | Scope | Coverage |
|----------|-------|----------|
| **GSM 04.06 / 3GPP TS 45.006** | LAPDm protocol for Um interface | `LAPDmFrame` zero-copy decode, `LAPDmEntity` full state machine (SABME/UA/DISC), I-frame segmentation/reassembly, T200 retransmission, contention resolution |
| **GSM 04.08 / 3GPP TS 24.008** | Mobile radio interface L3 protocol | Full RR, MM, CC, GMM, SM (29 types), SMS L3 (14 types) message parsing and generation |
| **GSM 04.07 / 3GPP TS 24.007** | Information element encoding rules | V, TV, TLV, LV formats; H/L rest octet padding (0x2B); bit ordering |
| **GSM 04.80 / 3GPP TS 24.080** | Supplementary services on mobile | Facility, Register, Release Complete messages; SSOpCode/SSErrorCode enums; L3FacilityOpCode TCAP parser; L3USSDData IE |
| **GSM 02.90 / 3GPP TS 23.038** | USSD alphabet and encoding | GSM 7-bit default/extended alphabet, UCS2, DCS handling in L3USSDData |
| **3GPP TS 44.018** | Group call and broadcast call control | GCC (PD=0x00): 7 messages; BCC (PD=0x01): 6 messages |
| **3GPP TS 24.011 / GSM 04.11** | SMS over mobile radio interface | CP layer (5 messages), RP layer (4 messages), TP PDUs (Deliver, Submit, StatusReport, Command) |
| **3GPP TS 23.040 / GSM 03.40** | SMS TPDU formatting and encoding | TP address, TP-DCS, TP-PID, TP-SCTS, GSM 7-bit alphabet |
| **3GPP TS 44.031 / TS 24.027/24.028** | Location services on mobile radio | LSM: L3LocationServiceRequest, L3LocationServiceProviderMessage |
| **GSM 04.08 §10.2** | Extended and Test Procedure PDs | EXTENDED (PD=0x0e), TESTPROC (PD=0x0f) raw-body placeholder parsing |
