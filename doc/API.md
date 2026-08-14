# libgsml3parser — Full API Reference

> Version 0.10.0 | C++20 | Thread-safe | Zero heap allocation on hot path | No external dependencies

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
11. [LAPDm Framing](#11-lapdm-framing)
12. [Protocol Dispatcher](#12-protocol-dispatcher)
13. [Builder Pattern](#13-builder-pattern)
14. [Enum Formatters](#14-enum-formatters)
15. [Data Types](#15-data-types)
16. [Enumerations](#16-enumerations)
17. [Protocol Types](#17-protocol-types)
18. [Common Information Elements](#18-common-information-elements)
19. [Radio Resource Messages](#19-radio-resource-messages)
20. [Mobility Management Messages](#20-mobility-management-messages)
21. [Call Control Messages](#21-call-control-messages)
22. [Supplementary Services Messages](#22-supplementary-services-messages)
23. [GPRS Mobility Management Messages](#23-gprs-mobility-management-messages)
24. [GPRS Session Management Messages](#24-gprs-session-management-messages)
25. [SMS Messages](#25-sms-messages)
26. [Broadcast Call Control Messages](#26-broadcast-call-control-messages)
27. [Group Call Control Messages](#27-group-call-control-messages)
28. [Location Services Messages](#28-location-services-messages)
29. [SMS L3 Messages](#29-sms-l3-messages)
30. [Extended PD Messages](#30-extended-pd-messages)
31. [Test Procedure PD Messages](#31-test-procedure-pd-messages)
32. [MSContext — Per-Subscriber State](#32-mscontext-per-subscriber-state)
33. [Timer Framework](#33-timer-framework)
34. [Transaction Framework](#34-transaction-framework)

---

## 1. Getting Started

### 1.1 Build Requirements

| Requirement | Minimum Version |
|-------------|-----------------|
| C++ Compiler | GCC 11+, Clang 10+, MSVC 2022 17.3+ |
| CMake | 3.20+ |
| Standard Library | C++20 (libstdc++ or libc++) |

### 1.2 Build

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

### 1.3 CMake Integration

```cmake
find_package(gsml3parser REQUIRED)
target_link_libraries(myapp PRIVATE gsml3parser::parser)
```

Or as a subdirectory:

```cmake
add_subdirectory(libgsml3parser)
target_link_libraries(myapp PRIVATE gsml3parser::parser)
```

### 1.4 Umbrella Header

Include everything with a single header:

```cpp
#include <gsml3parser/gsml3parser.hpp>
```

---

## 2. Core Types

### 2.1 ParseError

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

### 2.2 Expected<T>

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

### 3.1 BitReader

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
| `peekField(nbits)` | Read without advancing. Returns 0 for out-of-bounds bits. |
| `hasMore()` | True if more bits available |
| `remainingBits()` | Number of unread bits remaining |
| `position()` | Current bit position |
| `alignToOctet()` | Round up to next byte boundary |
| `readBytes(out, count)` | Read whole bytes into buffer. Optimized for aligned reads. |

**Bit ordering:** MSB-first within each octet (bit 7 is read first).

### 3.2 BitWriter

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

### 4.1 ParserConfig

**File:** `gsml3parser/parser_config.h`

Immutable, thread-safe parser configuration. No mutex, no atomic operations. Pure data struct.

```cpp
struct ParserConfig {
    LogLevel logLevel{LogLevel::WARNING};
    std::array<PDHandler, 16> pdHandlers{};

    [[nodiscard]] constexpr LogLevel getLogLevel() const;
    [[nodiscard]] const PDHandler* getPDHandler(L3PD pd) const;
    [[nodiscard]] ParserConfig withLogLevel(LogLevel lvl) const;
    [[nodiscard]] ParserConfig withPDHandler(L3PD pd, PDHandler handler) const;
};
```

| Method | Description |
|--------|-------------|
| `getLogLevel()` | Current log level |
| `getPDHandler(pd)` | Get custom handler for a PD (returns nullptr if none) |
| `withLogLevel(lvl)` | Return new config with changed log level |
| `withPDHandler(pd, h)` | Return new config with handler registered |

**Thread safety:** Safe for concurrent read access from any number of threads. Builder methods return new instances — the original is never modified.

**Usage example:**

```cpp
ParserConfig cfg;
cfg = cfg.withLogLevel(LogLevel::ERR)
         .withPDHandler(L3PD::SMS, mySmsHandler);

// Parse with config — no mutex acquired
auto result = parseL3Hex("060D", cfg);
```

---

## 5. L3 Header Parsing

### 5.1 L3Header

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

### 5.2 parseL3Header()

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

### 6.1 Domain Variants

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

### 6.2 ParsedMessage

The top-level variant that wraps all domains:

```cpp
using ParsedMessage = std::variant<RRM, MMM, CCM, SSM, GMM, SM, SMS, BCCM, GCCM, LSM, EXTENDED, TESTPROC>;
```

Stored on the stack — no heap allocation. `sizeof(ParsedMessage)` is guaranteed < 8 KB via `static_assert`. The variant spans 12 protocol domains.

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

### 7.1 parseL3()

Parse a complete L3 message from raw bytes.

```cpp
Expected<ParsedMessage> parseL3(std::span<const uint8_t> data, const ParserConfig& cfg = {});
```

| Parameter | Description |
|-----------|-------------|
| `data` | Raw L3 message bytes (header + body) |
| `cfg` | Parser configuration (optional, defaults to empty) |

Returns `Expected<ParsedMessage>` — the parsed variant on success, or `ParseError` on failure.

Handles short messages automatically: 1-byte Channel Request, 4-byte Handover Access, 7-byte Synchronization Channel Information.

### 7.2 parseL3Hex()

Parse a complete L3 message from a hex-encoded string.

```cpp
Expected<ParsedMessage> parseL3Hex(std::string_view hex, const ParserConfig& cfg = {});
```

Whitespace in the hex string is ignored.

### 7.3 writeL3()

Serialize a ParsedMessage to raw bytes.

```cpp
Expected<size_t> writeL3(const ParsedMessage& msg, uint8_t* out, size_t maxlen);
```

Returns number of bytes written on success. Returns `InvalidValue` error if buffer too small.

### 7.4 writeL3Hex()

Serialize a ParsedMessage to a hex-encoded string.

```cpp
Expected<std::string> writeL3Hex(const ParsedMessage& msg);
```

Uses inline encoding with lookup table — no `std::ostringstream` or `std::iomanip`.

### 7.5 writeL3Bytes()

Serialize a ParsedMessage to a `std::vector<uint8_t>` of raw bytes (header + body). Returns bytes ready for LAPDm framing and over-the-air transmission.

```cpp
Expected<std::vector<uint8_t>> writeL3Bytes(const ParsedMessage& msg);
```

| Return | Description |
|--------|-------------|
| `std::vector<uint8_t>` | Raw L3 bytes, ready for LAPDm wrapping or direct transmission |

**Usage:**

```cpp
auto msg = parseL3Hex("060D00");
if (msg) {
    auto bytes = writeL3Bytes(*msg);
    if (bytes) {
        // bytes->data() is ready for LAPDm framing
    }
}
```

### 7.6 Round-trip Pattern

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

### 8.1 tryGet<T>()

Compile-time typed access — no `dynamic_cast` needed.

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

### 8.2 messageName()

Returns a human-readable name for the message type.

```cpp
std::string_view messageName(const ParsedMessage& msg);
```

### 8.3 messagePD()

Returns the Protocol Discriminator of the message.

```cpp
L3PD messagePD(const ParsedMessage& msg);
```

### 8.4 messageMTI()

Returns the Message Type Indicator value.

```cpp
int messageMTI(const ParsedMessage& msg);
```

---

## 9. Bitstream I/O

### 9.1 ByteSource

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

### 9.2 SpanByteSource

Reads from a contiguous memory span. Non-blocking.

```cpp
class SpanByteSource : public ByteSource {
public:
    explicit SpanByteSource(std::span<const uint8_t> data);
    [[nodiscard]] size_t read(uint8_t* buf, size_t maxSize) override;
    [[nodiscard]] size_t remaining() const noexcept;
};
```

### 9.3 FileByteSource

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

### 9.4 RingBuffer

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

**Thread safety:** Single-producer, single-consumer is fully safe without locks. Uses `std::atomic` with acquire/release ordering for correct behaviour on weakly-ordered architectures (ARM, PowerPC). On x86-64 (TSO) the compiler emits plain loads/stores — zero overhead. Multi-producer or multi-consumer requires external synchronization.

### 9.5 L3Framer

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

### 9.6 L3StreamProcessor

**File:** `gsml3parser/bitstream/stream_processor.h`

High-throughput streaming parser with statistics tracking.

```cpp
class L3StreamProcessor {
public:
    L3StreamProcessor(ByteSource& source, ParserConfig cfg = {}, FrameConfig fcfg = {});
    bool processOne(std::function<void(const ParsedMessage&)> handler);
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

### 9.7 FrameHandler

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

### 9.8 StreamStats

Running statistics for stream processing.

```cpp
struct StreamStats {
    uint64_t totalBytes{}, totalFrames{}, parsedOk{}, parseErrors{};
    uint64_t truncatedInputs{}, unsupportedPD{};
    uint64_t rrMessages{}, mmMessages{}, ccMessages{}, ssMessages{};
};
```

### 9.9 L3StreamBuilder

Fluent builder for `L3StreamProcessor`.

```cpp
class L3StreamBuilder {
public:
    L3StreamBuilder& source(ByteSource& src);
    L3StreamBuilder& source(std::span<const uint8_t> data);
    L3StreamBuilder& sourceFile(const char* path);
    L3StreamBuilder& logLevel(LogLevel lvl);
    L3StreamBuilder& pdHandler(L3PD pd, PDHandler handler);
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
    [[nodiscard]] size_t used() const;
    [[nodiscard]] size_t capacity() const;
};
```

| Method | Description |
|--------|-------------|
| `allocate(bytes, alignment)` | Bump-allocate. Returns `nullptr` on failure. |
| `reset()` | Reclaim all memory. All previously returned pointers become invalid. |
| `used()` | Bytes consumed since last reset |
| `capacity()` | Total buffer size |

**Thread safety:** NOT thread-safe. Each thread must use its own Arena instance.

---

## 11. LAPDm Framing

**File:** `gsml3parser/lapdm.h`
**Namespace:** `gsml3parser::lapdm`
**Spec:** GSM 04.06 / 3GPP TS 24.022

Wraps L3 messages in LAPDm (Link Access Protocol for DM channel) frames for transmission over the Um interface, and unwraps incoming LAPDm frames to extract L3 payloads.

### makeAddress()

Encode a LAPDm address field byte from SAPI, C/R, and EA bits.

```cpp
uint8_t makeAddress(SAPI sapi, bool cr, bool ea = true);
```

| Parameter | Description |
|-----------|-------------|
| `sapi` | Service Access Point (SAPI0=signalling, SAPI3=data) |
| `cr` | Command (0) or Response (1) bit |
| `ea` | Extended Address bit (normally 1 for L3 messages) |

Bit layout: `[SAPI(7:4)][C/R(3)][reserved(2:1)][EA(0)]`.

### ControlField

```cpp
enum class ControlField : uint8_t {
    UI = 0x03,       // Unnumbered Information (most L3 messages)
    SABME = 0x2F,    // Set Asynchronous Balanced Mode Extended
    UA = 0x63,       // Unnumbered Acknowledgement
    DM = 0x0F,       // Disconnected Mode
};
```

NOTE: DISC shares the same bit pattern as UI (0x03). They are distinguished by protocol context.

### wrapL3()

Wrap an L3 message body in a LAPDm UI frame header. Output is `[address_field][control_field][l3_body...]`.

```cpp
std::vector<uint8_t> wrapL3(std::span<const uint8_t> l3Body, SAPI sapi = SAPI::SAPI0, bool cr = false);
```

### unwrapL3()

Unwrap a LAPDm frame and extract the L3 payload. Validates that the frame is at least 2 bytes.

```cpp
Expected<std::vector<uint8_t>> unwrapL3(std::span<const uint8_t> lapdmFrame);
```

Returns `TruncatedInput` error if fewer than 2 bytes provided.

### extractSAPI() / extractCR()

Decode individual fields from a LAPDm address byte.

```cpp
SAPI extractSAPI(uint8_t addrByte);
bool extractCR(uint8_t addrByte);
```

### isUIFrame()

Check if a LAPDm frame carries a UI control field.

```cpp
bool isUIFrame(std::span<const uint8_t> lapdmFrame);
```

**Usage:**

```cpp
auto msg = parseL3Hex("600d00");
auto l3Bytes = writeL3Bytes(*msg);

// Wrap in LAPDm UI frame (SAPI0, command)
auto frame = gsml3parser::lapdm::wrapL3(*l3Bytes, SAPI::SAPI0, false);

// Unwrap on the receiving side
auto payload = gsml3parser::lapdm::unwrapL3(frame);
auto reparsed = parseL3(*payload);
```

---

## 12. Protocol Dispatcher

**File:** `gsml3parser/dispatcher.h`

Callback-based message routing for BTS-style protocol handling. Dispatches incoming L3 messages to registered type-specific handlers using O(1) hash-map lookup on PD+MTI keys.

### MessageHandler

```cpp
using MessageHandler = std::function<void(const ParsedMessage& msg, void* context)>;
```

| Parameter | Description |
|-----------|-------------|
| `msg` | The parsed L3 message |
| `context` | User-provided context pointer (e.g., channel state, MS context) |

### ProtocolDispatcher

```cpp
class ProtocolDispatcher {
public:
    void registerHandler(L3PD pd, int mti, MessageHandler handler);
    void registerDomainHandler(L3PD pd, MessageHandler handler);
    void setFallbackHandler(MessageHandler handler);
    void dispatch(const ParsedMessage& msg, void* context = nullptr);
    bool dispatchRaw(std::span<const uint8_t> data, void* context = nullptr);
};
```

| Method | Description |
|--------|-------------|
| `registerHandler(pd, mti, h)` | Register handler for a specific PD + MTI combination |
| `registerDomainHandler(pd, h)` | Catch-all handler for all messages in a PD domain |
| `setFallbackHandler(h)` | Global fallback for unregistered message types |
| `dispatch(msg, ctx)` | Route a parsed message to the matching handler |
| `dispatchRaw(data, ctx)` | Parse raw bytes and dispatch in one call. Returns `true` if a handler was invoked. |

**Dispatch priority:** Specific handler (PD+MTI) → Domain handler (PD) → Fallback handler.

**Thread safety:** NOT thread-safe. Each thread should create its own `ProtocolDispatcher` instance.

**Usage:**

```cpp
gsml3parser::ProtocolDispatcher dispatcher;

// Specific handler for Channel Release (RR, MTI=0x0D)
dispatcher.registerHandler(gsml3parser::L3PD::RadioResource, 0x0D,
    [](const gsml3parser::ParsedMessage& msg, void*) {
        // Handle channel release...
    });

// Domain-wide fallback for all RR messages
dispatcher.registerDomainHandler(gsml3parser::L3PD::RadioResource,
    [](const gsml3parser::ParsedMessage& msg, void*) {
        std::cout << "RR message: " << gsml3parser::messageName(msg) << "\n";
    });

// Dispatch parsed message
auto msg = gsml3parser::parseL3Hex("600d00");
if (msg) dispatcher.dispatch(*msg);

// Or dispatch raw bytes directly
uint8_t data[] = {0x60, 0x0D, 0x00};
dispatcher.dispatchRaw(std::span<const uint8_t>(data));
```

---

## 13. Builder Pattern

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

### Example — Building an Immediate Assignment

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

### Example — Building a Paging Request Type 2

```cpp
auto msg = L3PagingRequestType2::builder()
    .pageMode(L3PageMode::TMSI)
    .tmsi(0x12345678)
    .build();

ParsedMessage pm{RRM{std::move(msg)}};
auto hex = writeL3Hex(pm);
```

### Example — Building a Setup (CC) Message

```cpp
auto msg = L3Setup::builder()
    .calledPartyNumber(L3CalledPartyBCDNumber("1234567890"))
    .bearerCapability(L3BearerCapability{})
    .build();

ParsedMessage pm{CCM{std::move(msg)}};
```

---

## 14. Enum Formatters

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

## 15. Data Types

**File:** `gsml3parser/types.h`

### L3PD — Protocol Discriminator

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

### Primitive — Interlayer Primitives

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

### SAPI — Service Access Point Indicator

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

## 16. Enumerations

**File:** `gsml3parser/enums.h`

### RRCause — RR Cause Codes

3GPP TS 24.008 10.5.2.31. Values: `Normal_Event`, `Unspecified`, `Channel_Unacceptable`, `Timer_Expired`, `No_Activity_On_The_Radio`, `Preemptive_Release`, `Handover_Impossible`, `Channel_Mode_Unacceptable`, `Frequency_Not_Implemented`, `Leaving_Group_Call_Area`, `Lower_Layer_Failure`, and protocol error codes.

```cpp
const char* RRCause2Str(RRCause cause);
```

### MMRejectCause — MM Reject Cause Codes

3GPP TS 24.008 10.5.3.6. Values: `IMSI_Unknown_In_HLR`, `Illegal_MS`, `IMSI_Unknown_In_VLR`, `IMEI_Not_Accepted`, `Illegal_ME`, `PLMN_Not_Allowed`, `Location_Area_Not_Allowed`, `Roaming_Not_Allowed_In_LA`, `Network_Failure`, `MAC_Failure`, `Congestion`, and protocol error codes.

```cpp
const char* MMRejectCause2Str(MMRejectCause cause);
```

### CCCause — CC Cause Codes

3GPP TS 24.008 10.5.4.11 / ISDN Q.931. Values: `Unassigned_Number`, `No_Route_To_Destination`, `Normal_Call_Clearing`, `User_Busy`, `Call_Rejected`, `No_Channel_Available`, `Network_Out_Of_Order`, `Requested_Facility_Not_Subscribed`, and 40+ more.

```cpp
const char* CCCause2Str(CCCause cause);
```

### CCCauseLocation

Indicates where the cause originated: `User`, `Private_Serving_Local`, `Public_Serving_Local`, `Transit`, `Public_Serving_Remote`, `Private_Serving_Remote`, `International`, `Beyond_Inter_Networking`.

### BSSCause — BSS Cause Codes

3GPP TS 48.008 3.2.2.5. Values: `Radio_Interface_Failure`, `Uplink_Quality`, `Downlink_Quality`, `Handover_Successful`, `Better_Cell`, `No_Radio_Resource_Available`, `CCCH_Overload`, and more.

```cpp
const char* BSSCause2Str(BSSCause cause);
```

---

## 17. Protocol Types

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

## 18. Common Information Elements

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

## 19. Radio Resource Messages

**File:** `gsml3parser/rr/l3rrmessages.h` — 95 message types in the `RRM` variant.

Each message is a plain struct with:
- `parse(BitReader&)` → `Expected<Self>` (static method)
- `write(BitWriter&) const` → void
- `mti() const` → returns MTI value
- `text(std::ostream&) const` → human-readable output

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
| `L3ImmediateAssignmentExtended` | — | DL | Extended immediate assignment |
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
| `L3NotificationFACCH` | — | FACCH notification |
| `L3UplinkFree` | — | FACCH uplink free |
| `L3EnhancedMeasurementRepUL` | variable | FACCH measurement report UL |
| `L3MeasurementInfoDL` | variable | FACCH measurement info DL |
| `L3VBSVGCSRecon` | — | VBS/VGCS reconfiguration |
| `L3VBSVGCSRecon2` | — | VBS/VGCS reconfiguration 2 |
| `L3VGCSAddInfo` | — | VGCS additional info |
| `L3VGCSMSInfo` | — | VGCS SMS info |
| `L3VGCSSNeighCellInfo` | — | VGCS neighbor cell info |
| `L3NotifyAppData` | — | Notify application data |

---

## 20. Mobility Management Messages

**File:** `gsml3parser/mm/l3mmmessages.h` — 18 message types in the `MMM` variant.

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

## 21. Call Control Messages

**File:** `gsml3parser/cc/l3ccmessages.h` — 20 message types in the `CCM` variant.

### CC Information Elements

**File:** `gsml3parser/cc/l3ccelements.h`

| IE | IEI | Format | Description |
|----|-----|--------|-------------|
| `L3BearerCapability` | 0x04 | TLV | Bearer capability (coding, mode, rate) |
| `L3BackupBearerCapability` | 0x7c | TLV | Backup bearer capability |
| `L3SupportedCodecList` | 0x40 | TLV | AMR codec set and mode preferences |
| `L3BCDDigits` | — | V | BCD-encoded digit string utility |
| `L3CalledPartyBCDNumber` | 0x5e | TLV | Called party number |
| `L3CallingPartyBCDNumber` | 0x5c | TLV | Calling party number |
| `L3ConnectedNumber` | 0x9c | TLV | Connected party number (GSM 04.08 10.5.4.7) |
| `L3RedirectingNumber` | 0x97 | TLV | Redirecting number (GSM 04.08 10.5.4.13) |
| `L3SubAddress` | 0x9a/0x9b | TLV | Calling/Called party sub-address (GSM 04.08 10.5.4.3) |
| `L3CauseElement` | 0x08 | TLV | CC cause code + location + diagnostic |
| `L3CallState` | — | V | Call state flags (speech, DTMF, hold, etc.) |
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
| `L3SupServVersionIndicator` | — | V | SS version indicator (GSM 04.08 10.5.4.24) |

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

## 22. Supplementary Services Messages

**File:** `gsml3parser/ss/l3ssmessages.h` — 3 message types in the `SSM` variant.

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

### L3FacilityOpCode — TCAP Component Parser

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

### L3USSDData — USSD Message IE

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

## 23. GPRS Mobility Management Messages

**File:** `gsml3parser/gmm/l3gmmmessages.h` — 19 message types in the `GMM` variant.
**Spec:** 3GPP TS 24.008 sections 9.4, Table 10.4.
**PD:** `0x08` (GPRSMobilityManagement).

### GMM Information Elements

**File:** `gsml3parser/gmm/l3gmmelements.h`

| IE | IEI | Format | Description |
|----|-----|--------|-------------|
| `L3PDPContextStatus` | 0x32 | TLV | PDP context activation bitmap (16 contexts) |
| `L3T3302Timer` | 0x1b | TLV | T3302 timer value (GPRS Timer 2 encoding) |
| `L3MSNetworkCapability` | — | V | MS network capability bit string |
| `L3RoutingAreaIdentification` | — | V | MCC/MNC BCD(3) + LAC(2) + RAC(1) = 6 octets |
| `L3DRXParameter` | 0x1a | TV | DRX cycle code + timer settings (2 octets) |
| `L3GMMCKSN` | — | bit-field | Ciphering key sequence number (3 bits) |
| `L3GMMCauseIE` | 0x25 | TLV | GMM cause value |
| `L3AuthRAND` | 0x15 | TLV | 128-bit authentication challenge |
| `L3AuthRES` | 0x16 | TLV | 32-bit authentication response |
| `L3AuthFailureParam` | 0x30 | TLV | AUTS failure parameter (variable) |
| `L3PTMSISignature` | 0x13 | TV | P-TMSI signature (3 octets) |
| `L3GMMStatusCause` | — | V | GMM status cause octet |

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

## 24. GPRS Session Management Messages

**File:** `gsml3parser/sm/l3smmessages.h` — 29 message types in the `SM` variant.
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
| `L3PDPHandle` | — | bit-field | PDP context identifier (4 bits, 0–15) |
| `L3TMGI` | 0x42 | TLV | Temporary Mobile Group Identity: PLMN(3) + ServiceID(2) + SessionID(1) |

### SM Enums

| Enum | Values | Description |
|------|--------|-------------|
| `PDPType` | 6 values | IPv4, IPv6, IPsecAH, PPP, Private, Unknown |
| `QoSType` | 3 values | Requested, Default, Teardown |
| `QoSElementType` | 18 values | QoSClass, MaxBitRate UL/DL, Delay, DeliveryOrder, SopClass, ResidualErrorRate, PeakThroughput, MeanThroughput, TrafficClass, GuaranteedBitRate, SRB rate, GPRS/External Priority |
| `SMCause` | 17 codes | SM cause values (ReqAccepted, Unsupported_PDP_Address_Type, PDP_Auth_Failed, IE_Invalid, etc.) |

### SM Messages — Primary PDP Context

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

### SM Messages — Request PDP Context Activation (Net-initiated)

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3RequestPDPContextActivation` | 0x44 | DL | PDP handle, [PDP address], APN, QoS, [PCO] |
| `L3RequestPDPContextActivationReject` | 0x45 | UL | PDP handle, SM cause |

### SM Messages — Bidirectional Modify (MS-initiated variants)

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3ModifyPDPContextRequestMS` | 0x4A | UL | PDP handle, QoS, [PCO] |
| `L3ModifyPDPContextAcceptNet` | 0x4B | DL | PDP handle, QoS, [PCO] |

### SM Messages — Secondary PDP Context

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3ActivateSecondaryPDPContextRequest` | 0x4D | DL | PDP handle, [PDP address], APN, QoS, [PCO] |
| `L3ActivateSecondaryPDPContextAccept` | 0x4E | UL | PDP handle, [PDP address], QoS, [PCO] |
| `L3ActivateSecondaryPDPContextReject` | 0x4F | UL | PDP handle, SM cause |

### SM Messages — Always Active (AA) PDP Context

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3ActivateAAPDPContextRequest` | 0x50 | DL | PDP handle, [PDP address], APN, QoS, [PCO] |
| `L3ActivateAAPDPContextAccept` | 0x51 | UL | PDP handle, [PDP address], QoS, [PCO] |
| `L3ActivateAAPDPContextReject` | 0x52 | UL | PDP handle, SM cause |
| `L3DeactivateAAPDPContextRequest` | 0x53 | DL | PDP handle |
| `L3DeactivateAAPDPContextAccept` | 0x54 | UL | PDP handle |

### SM Messages — MBMS Context

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3ActivateMBMSContextRequest` | 0x56 | UL | TMGI, QoS, [PCO] |
| `L3ActivateMBMSContextAccept` | 0x57 | DL | PDP handle, QoS, [PCO] |
| `L3ActivateMBMSContextReject` | 0x58 | DL | SM cause |
| `L3RequestMBMSContextActivation` | 0x59 | DL | TMGI, QoS, [PCO] |
| `L3RequestMBMSContextActivationReject` | 0x5A | UL | SM cause |

### SM Messages — Network-Initiated Secondary & Notification

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3RequestSecondaryPDPContextActivation` | 0x5B | DL | PDP handle, [PDP address], APN, QoS, [PCO] |
| `L3RequestSecondaryPDPContextActivationReject` | 0x5C | UL | PDP handle, SM cause |
| `L3SMNotification` | 0x5D | DL | PDP handle |

---

## 25. SMS Messages

**File:** `gsml3parser/sms/l3smsmessages.h` — 5 CP messages; `gsml3parser/sms/l3smsl3messages.h` — 14 L3 messages; total 19 in the `SMS` variant.
**Spec:** 3GPP TS 24.011 sections 7-8, 3GPP TS 23.040 (CP/RP/TP layers); 3GPP TS 24.008 sections 9.6, Table 10.6a (SMS L3 primitives).
**PD:** `0x09` (SMS).

The SMS layer uses a three-level encapsulation: L3 header → CP message → RP message → TP PDU. Additionally, the SMS L3 messages (MTI=0x11–0x1E) provide TE-to-MS SMS primitives for status reporting, deliver/reply, and notification flows.

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

## 26. Broadcast Call Control Messages

**File:** `gsml3parser/bcc/l3bccmessages.h` — 6 message types in the `BCCM` variant.
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

## 27. Group Call Control Messages

**File:** `gsml3parser/gcc/l3gccmessages.h` — 7 message types in the `GCCM` variant.
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

## 28. Location Services Messages

**File:** `gsml3parser/ls/l3lsmessages.h` — 2 message types in the `LSM` variant.
**Spec:** 3GPP TS 44.031 / TS 24.027 / TS 24.028.
**PD:** `0x0c` (Location).

Location Services messages carry mobile location service parameters between the MS and the network. Both message types store their body as a raw octet sequence, with parse/write handling the L3 header dispatch.

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3LocationServiceRequest` | 0x01 | Bidir | Location service request parameters (raw body) |
| `L3LocationServiceProviderMessage` | 0x02 | Bidir | Location service provider data (raw body) |

---

## 29. SMS L3 Messages

**File:** `gsml3parser/sms/l3smsl3messages.h` — 14 message types, part of the `SMS` variant.
**Spec:** 3GPP TS 24.008 sections 9.6.1–9.6.14, Table 10.6a.
**PD:** `0x09` (SMS).

These are L3-level SMS primitives used for SMS-on-CS fallback, status reporting, and network-initiated SMS delivery. They share the PD with CP-layer messages but operate in a different context. MTI 0x12 and 0x13 overlap with CP-STATUS and CP-SMT; the parser resolves overlaps by preferring CP messages for backward compatibility.

### SMS L3 Enums

| Enum | Values | Description |
|------|--------|-------------|
| `TPStatus` | 5 values | Delivered, DeliveryAttempted, ErasedAtMS, DeliveryNotPossible, Decrypted |
| `RPDisposalType` | 4 values | NoFurtherAction, DisplayToUser, StoreInSIM, DeleteFromMS |
| `SMSCause` | 8 codes | SMS-specific cause values (NoCause, SMSSystemFailure, etc.) |

### SMS L3 Messages — Status Report Flow

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3SMSStatusReport` | 0x11 | Bidir | TP-MR, RP-Disp, [TP-DA], [TP-OA], [SCTS], [MT-StartTime], TP-ST |
| `L3SMSProvidedReplyExpected` | 0x12 | DL | [TP-PID], TP-DCS, [TP-Ud] |
| `L3SMSSubmitRep` | 0x13 | DL | [TP-PID], TP-DCS, [TP-Ud] |

### SMS L3 Messages — Deliver Flow

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3SMSDeliver` | 0x14 | DL | TP-MTI, TP-MR, [TP-OA], TP-PID, TP-DCS, SCTS, [TP-Ud] |
| `L3SMSDeliverRep` | 0x15 | UL | TP-MTI, TP-MR, [TP-DA], TP-PID, TP-DCS, [TP-Ud] |

### SMS L3 Messages — Status Ack/Reject

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3SMSStatusReportAck` | 0x16 | UL | TP-MR |
| `L3SMSStatusReportReject` | 0x17 | DL | TP-MR, SM-Cause |
| `L3SMSTSReject` | 0x18 | DL | SM-Cause |

### SMS L3 Messages — Submit Control

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3SMSSubmitDeferred` | 0x19 | DL | [TP-PID], TP-DCS, [TP-Ud] |
| `L3SMSSubmitReject` | 0x1A | DL | SM-Cause |

### SMS L3 Messages — Service Centre & Notification

| Message | MTI | Direction | Description |
|---------|-----|-----------|-------------|
| `L3SMSSFProvidedRep` | 0x1B | UL | [TP-PID], TP-DCS, [TP-Ud] |
| `L3SMSSFProvidedRepAck` | 0x1C | DL | Empty body |
| `L3SMSNotification` | 0x1D | Bidir | [TP-PID], TP-DCS, [TP-Ud] |
| `L3SMSShortCodeInfo` | 0x1E | Bidir | ShortCodeType, [ShortCode] |

---

## 30. Extended PD Messages

**File:** `gsml3parser/extended/l3extendedmessages.h` — 1 placeholder type in the `EXTENDED` variant.
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

## 31. Test Procedure PD Messages

**File:** `gsml3parser/testproc/l3testproceduremessages.h` — 1 placeholder type in the `TESTPROC` variant.
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

## 32. MSContext — Per-Subscriber State

MSContext aggregates all state associated with a single mobile station: identity (TMSI/IMSI), channel assignment, classmark, location area, and protocol-layer flags (ciphering, registration, authentication). This is the primary object through which a BTS tracks each subscriber.

**Header:** `#include <gsml3parser/stack/ms_context.h>`

### 32.1 Performance Characteristics

| Property | Value |
|----------|-------|
| `sizeof(MSContext)` | ≤ 256 bytes (enforced by `static_assert`) |
| Heap allocations | **Zero** — all fields stored inline |
| Hot path methods | O(1), no virtual dispatch, no heap |
| Thread safety | NOT thread-safe. One instance per MS, single-thread access |

Fields are ordered by access frequency: hot fields (identity, channel, flags) first, cold fields (LAI, classmark) last. This layout minimizes cache line misses for the typical message processing path.

### 32.2 Factory Methods

| Method | Description |
|--------|-------------|
| `MSContext::createWithTMSI(uint32_t tmsi)` | Create context with TMSI identity |
| `MSContext::createWithIMSI(std::string_view imsiDigits)` | Create context with IMSI identity |

### 32.3 Identity API

| Method | Description |
|--------|-------------|
| `identity()` | Returns `const L3MobileIdentity&` — current primary identity |
| `setTMSI(uint32_t tmsi)` | Update or set TMSI |
| `setIMSI(std::string_view digits)` | Update or set IMSI |

### 32.4 Channel Assignment API

| Method | Description |
|--------|-------------|
| `channelType()` | Returns current `ChannelType` (or `UndefinedCHType`) |
| `assignChannel(type, trx, ts, arfcn)` | Assign a logical channel with physical parameters |
| `releaseChannel()` | Release channel, resets to `UndefinedCHType` |
| `trxNumber()` | Returns transceiver index |
| `timeslot()` | Returns TDMA timeslot number |
| `arfcn()` | Returns ARFCN value |

### 32.5 State Flags API

| Method | Description |
|--------|-------------|
| `isCiphered()` / `setCiphered(bool)` | Ciphering active state |
| `isRegistered()` / `setRegistered(bool)` | Location update completed |
| `isAuthenticated()` / `setAuthenticated(bool)` | Authentication performed |
| `timingAdvance()` / `setTimingAdvance(uint8_t)` | Timing advance value (0-63) |
| `classmark()` / `setClassmark(cm)` | MS Classmark 1 storage |
| `lai()` / `setLAI(lai)` | Location Area Identity storage |

### 32.6 Example

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

## 33. Timer Framework

The timer framework provides GSM Layer 3 protocol timers as defined in 3GPP TS 24.008 and TS 44.018. It includes timer identifiers, a single-timer class, and a manager that tracks up to 32 concurrent timers per MS using fixed-size arrays (zero heap allocation).

**Header:** `#include <gsml3parser/stack/l3_timer.h>`

### 33.1 L3TimerId Enumerations

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

### 33.2 Free Functions

| Function | Description |
|----------|-------------|
| `l3TimerDefault(L3TimerId)` | Returns the default duration for a timer ID (O(1) lookup) |
| `l3TimerName(L3TimerId)` | Returns human-readable timer name (e.g. "T3101") |

### 33.3 L3Timer Class

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

### 33.4 TimerManager Class

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

### 33.5 Performance Characteristics

| Metric | Value |
|--------|-------|
| Memory footprint | `sizeof(TimerManager)` ≈ 1.2 KB (32 timers × ~36 bytes + 32 bytes init flags) |
| Heap allocations | **Zero** — all storage is `std::array` |
| `tick()` complexity | O(32) = constant, iterates fixed array |
| `start()` / `stop()` | O(1) — direct index into array |
| Thread safety | NOT thread-safe. One instance per MS, single-thread access |

### 33.6 Example

```cpp
#include <gsml3parser/stack/l3_timer.h>

using namespace gsml3parser;

// --- Callback-based tick (no heap allocation) ---
TimerManager tm;
tm.start(L3TimerId::T3101);  // CM service request, default 3s
tm.start(L3TimerId::T3106, 5000ms); // custom expiry

// In event loop, advance time:
tm.tick(std::chrono::milliseconds(100), [](L3TimerId expiredId) {
    // Handle timer expiry — retransmit or abort procedure
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

## 34. Transaction Framework

The transaction framework provides request-response correlation for L3 messaging. It tracks outgoing requests and matches incoming responses using either TI (Transaction Identifier) for CC/SS protocols or PD+MTI for other protocol discriminators.

### 34.1 Overview

| Component | Description |
|-----------|-------------|
| `Transaction` | Single transaction metadata: PD, MTI, TI, timer ID, state |
| `TransactionManager` | Manages up to 16 concurrent transactions per MS |
| `TransactionState` | Pending, Completed, Expired, Cancelled |

### 34.2 Matching Semantics

| Protocol Discriminator | Matching Strategy | Complexity |
|------------------------|-------------------|------------|
| `CallControl` (PD=0x03) | TI-based lookup via `mTiIndex[8]` | O(1) |
| `NonCallSS` (PD=0x0b) | TI-based lookup via `mTiIndex[8]` | O(1) |
| All other PDs | PD + MTI comparison | O(K), K < 16 |

### 34.3 API Reference

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

### 34.4 Usage Example

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

### 34.5 Performance Characteristics

| Metric | Value |
|--------|-------|
| `sizeof(Transaction)` | <= 48 bytes |
| Max concurrent transactions | 16 per MS |
| CC/SS match() complexity | O(1) via TI index |
| Non-CC/SS match() complexity | O(K), K < 16 |
| Heap allocations | None (std::array storage) |
| Thread safety | NOT thread-safe; one instance per MS |

---

## Conformance Notes

The library implements encodings defined by:

| Standard | Scope | Coverage |
|----------|-------|----------|
| **GSM 04.06 / 3GPP TS 24.022** | LAPDm framing for Um interface | `wrapL3()`/`unwrapL3()` UI frame construction, address field encoding (SAPI/C/R/EA), control field extraction |
| **GSM 04.08 / 3GPP TS 24.008** | Mobile radio interface L3 protocol | Full RR, MM, CC, GMM, SM (29 types), SMS L3 (14 types) message parsing and generation |
| **GSM 04.07 / 3GPP TS 24.007** | Information element encoding rules | V, TV, TLV, LV formats; H/L rest octet padding (0x2B); bit ordering |
| **GSM 04.80 / 3GPP TS 24.080** | Supplementary services on mobile | Facility, Register, Release Complete messages; SSOpCode/SSErrorCode enums; L3FacilityOpCode TCAP parser; L3USSDData IE |
| **GSM 02.90 / 3GPP TS 23.038** | USSD alphabet and encoding | GSM 7-bit default/extended alphabet, UCS2, DCS handling in L3USSDData |
| **3GPP TS 44.018** | Group call and broadcast call control | GCC (PD=0x00): 7 messages; BCC (PD=0x01): 6 messages |
| **3GPP TS 24.011 / GSM 04.11** | SMS over mobile radio interface | CP layer (5 messages), RP layer (4 messages), TP PDUs (Deliver, Submit, StatusReport, Command) |
| **3GPP TS 23.040 / GSM 03.40** | SMS TPDU formatting and encoding | TP address, TP-DCS, TP-PID, TP-SCTS, GSM 7-bit alphabet |
| **3GPP TS 44.031 / TS 24.027/24.028** | Location services on mobile radio | LSM: L3LocationServiceRequest, L3LocationServiceProviderMessage |
| **GSM 04.08 §10.2** | Extended and Test Procedure PDs | EXTENDED (PD=0x0e), TESTPROC (PD=0x0f) raw-body placeholder parsing |
