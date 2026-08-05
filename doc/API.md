# libgsml3parser — Full API Reference

> Version 0.4.0 | C++20 | Thread-safe | HPL-compliant memory | No external dependencies

## Table of Contents

1. [Getting Started](#1-getting-started)
2. [Core API](#2-core-api)
3. [ParserContext — Thread-Safe Configuration](#3-parsercontext-thread-safe-configuration)
4. [Memory Management — Arena and BitView](#4-memory-management-arena-and-bitview)
5. [Data Types](#5-data-types)
6. [Information Elements](#6-information-elements)
7. [Radio Resource Messages (PD=0x06)](#7-radio-resource-messages-pd0x06)
8. [Mobility Management Messages (PD=0x05)](#8-mobility-management-messages-pd0x05)
9. [Call Control Messages (PD=0x03)](#9-call-control-messages-pd0x03)
10. [Supplementary Services (PD=0x0b)](#10-supplementary-services-pd0x0b)
11. [Error Handling](#11-error-handling)
12. [Logging](#12-logging)
13. [Conformance Notes](#13-conformance-notes)
14. [Appendix: GSM Specifications](#14-appendix-gsm-specifications)

---

## 1. Getting Started

### 1.1 Build Requirements

| Requirement | Minimum Version |
|-------------|-----------------|
| C++ Compiler | GCC 11+, Clang 10+, MSVC 2022 17.3+ |
| CMake | 3.20+ |
| Standard Library | C++20 (libstdc++ or libc++) |
| Make / Ninja | Any |
| Google Test (optional) | 1.14+ |

### 1.2 Build

```bash
mkdir build && cd build
cmake .. -DBUILD_TESTS=ON -DBUILD_EXAMPLES=ON
make -j$(nproc)
```

CMake options:

| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_SHARED_LIBS` | OFF | Build shared library instead of static |
| `BUILD_TESTS` | OFF | Build unit tests (requires Google Test) |
| `BUILD_EXAMPLES` | OFF | Build example programs |
| `ENABLE_FUZZING` | OFF | Build fuzzing target |

### 1.3 Install

```bash
sudo make install
```

### 1.4 CMake Integration

```cmake
find_package(gsml3parser REQUIRED)
target_link_libraries(myapp PRIVATE gsml3parser::parser)
```

Or as a subdirectory:

```cmake
add_subdirectory(libgsml3parser)
target_link_libraries(myapp PRIVATE gsml3parser::parser)
```

---

## 2. Core API

### 2.1 Primary Entry Point: `parseL3()`

**File:** `gsml3parser/parser.h`

Parse a complete L3 message from binary data. The library dispatches to the correct
sub-parser based on the Protocol Discriminator (PD) field.

```cpp
std::unique_ptr<L3Message> parseL3(const L3Frame& frame, const ParserContext& ctx);
std::unique_ptr<L3Message> parseL3(std::span<const uint8_t> data, const ParserContext& ctx);
std::unique_ptr<L3Message> parseL3Hex(std::string_view hex, const ParserContext& ctx);
```

| Parameter | Description |
|-----------|-------------|
| `frame` | An L3 frame with PD + MTI + body. |
| `data` | `std::span` of raw L3 message bytes. |
| `hex` | Hex-encoded string (`std::string_view`). Whitespace is ignored. |
| `ctx` | Parser configuration (PD handlers, log level). See [§3](#3-parsercontext-thread-safe-configuration). |

**Returns:** `std::unique_ptr<L3Message>` — the parsed message, or `nullptr` on failure.

**Example:**

```cpp
#include <gsml3parser/parser.h>
#include <gsml3parser/context.h>

gsml3parser::ParserContext ctx;

std::span<const uint8_t> data{0x06, 0x27, /* PagingResponse body */};
auto msg = gsml3parser::parseL3(data, ctx);

if (msg) {
    std::cout << msg->text() << std::endl;
}
```

### 2.2 Serialize Message to Bytes

```cpp
size_t writeL3(const L3Message& msg, uint8_t* out, size_t maxlen);
```

**Returns:** Number of bytes written, or `0` if `out` is too small.

### 2.3 Serialize Message to Hex

```cpp
std::string writeL3Hex(const L3Message& msg);
```

### 2.4 Domain Parsers and Factories

Each PD domain has a dedicated parser and factory. All factories return
`std::unique_ptr` (no raw `new`/`delete`):

```cpp
// RR
std::unique_ptr<L3RRMessage> parseL3RR(const L3Frame& source);
std::unique_ptr<L3RRMessage> L3RRFactory(int mti);

// MM
std::unique_ptr<L3MMMessage> parseL3MM(const L3Frame& source);
std::unique_ptr<L3MMMessage> L3MMFactory(int mti);

// CC
std::unique_ptr<L3CCMessage> parseL3CC(const L3Frame& source);
std::unique_ptr<L3CCMessage> L3CCFactory(int mti);

// SS
std::unique_ptr<L3SupServMessage> parseL3SupServ(const L3Frame& source);
std::unique_ptr<L3SupServMessage> L3SupServFactory(int mti);
```

### 2.5 MTI to String

```cpp
std::string mti2string(L3PD pd, unsigned mti);
```

Converts a PD + MTI pair to a human-readable name.

---

## 3. ParserContext — Thread-Safe Configuration

**File:** `gsml3parser/context.h`

`ParserContext` replaces the legacy global PD handler registry. Each context
maintains its own handler map and log level, making it safe to share across
threads for read-only access.

```cpp
class ParserContext {
public:
    ParserContext() = default;

    void registerPDHandler(L3PD pd, PDHandler handler);
    void unregisterPDHandler(L3PD pd);
    std::optional<PDHandler> getPDHandler(L3PD pd) const;

    LogLevel logLevel() const;
    void setLogLevel(LogLevel level);
};
```

### Thread-Safety Guarantees

| Operation | Lock type | Safe to call concurrently? |
|-----------|-----------|---------------------------|
| `parseL3(..., ctx)` | Shared read lock | Yes — multiple threads can parse with the same context |
| `getPDHandler(pd)` | Shared read lock | Yes |
| `logLevel()` | Lock-free | Yes |
| `registerPDHandler(pd, handler)` | Exclusive write lock | Yes — but blocks concurrent reads briefly |
| `unregisterPDHandler(pd)` | Exclusive write lock | Yes — but blocks concurrent reads briefly |
| `setLogLevel(level)` | Lock-free | Yes |

### Usage Pattern

```cpp
// Create once at program startup, share read-only across threads
gsml3parser::ParserContext globalCtx;
globalCtx.registerPDHandler(gsml3parser::L3PD::SMS, mySmsHandler);

// Each thread parses independently
void workerThread(std::span<const uint8_t> frames) {
    for (auto frame : frames) {
        auto msg = gsml3parser::parseL3(frame, globalCtx);
        // ... process ...
    }
}
```

For isolated per-thread contexts (no shared state at all):

```cpp
void isolatedThread() {
    gsml3parser::ParserContext ctx;  // completely independent
    ctx.registerPDHandler(gsml3parser::L3PD::SMS, mySmsHandler);
    // ... parse with ctx ...
}
```

---

## 4. Memory Management — Arena and BitView

### 4.1 Arena Allocator

**File:** `gsml3parser/arena.h`

A simple bump allocator for high-throughput parsing scenarios. Avoids per-object
allocation overhead by allocating from a single growable buffer.

```cpp
class Arena {
public:
    explicit Arena(size_t initialCapacity = 4096);

    void* allocate(size_t bytes, size_t alignment = alignof(std::max_align_t));
    void reset();
    size_t used() const;
    size_t capacity() const;
};
```

| Method | Description |
|--------|-------------|
| `allocate(bytes, alignment)` | Bump-allocate `bytes` with the given alignment. Returns `nullptr` on failure. |
| `reset()` | Reclaim all memory. All previously returned pointers become invalid. |
| `used()` | Bytes consumed since last `reset()`. |
| `capacity()` | Total allocated buffer size. |

**Thread-safety:** NOT thread-safe. Each thread must use its own Arena instance.

**Example — batch parsing:**

```cpp
gsml3parser::Arena arena(65536);  // 64 KB

for (const auto& batch : messageBatches) {
    arena.reset();  // free all allocations from previous batch

    for (auto frame : batch) {
        gsml3parser::BitVector bv(arena, frame.size_bytes() * 8);
        // ... parse using bv ...
        // No individual free needed — arena.reset() handles it
    }
}
```

### 4.2 BitView — Zero-Copy Read-Only View

**File:** `gsml3parser/bitvector.h`

A non-owning, read-only view over an external byte buffer. Useful for parsing
data you do not own (network buffers, DMA rings, shared memory).

```cpp
class BitView {
public:
    BitView();
    BitView(const uint8_t* data, size_t nbits);
    explicit BitView(std::span<const uint8_t> bytes);

    size_t size() const;
    bool empty() const;

    unsigned readField(size_t& rp, unsigned nbits) const;
    unsigned peekField(size_t rp, unsigned nbits) const;
    unsigned readBit(size_t& rp) const;
    const uint8_t* data() const;
};
```

| Method | Description |
|--------|-------------|
| `readField(rp, nbits)` | Read `nbits` from position `rp`, advance `rp`. |
| `peekField(rp, nbits)` | Read without advancing. |
| `readBit(rp)` | Read single bit, advance `rp`. |

**Ownership:** The caller guarantees the underlying buffer outlives the BitView.

**Example — parsing socket data without copying:**

```cpp
extern uint8_t socketBuffer[256];  // filled by network layer

gsml3parser::BitView view(std::span<const uint8_t>{socketBuffer, 256});
size_t rp = 0;
unsigned pd = view.readField(rp, 8);
unsigned mti = view.readField(rp, 7);
```

### 4.3 BitVector Memory Modes

The `BitVector` class supports three memory modes:

| Mode | Constructor | Ownership | Destructor frees? |
|------|-----------|-----------|-------------------|
| Default (vector) | `BitVector(nbits)` | Library-owned (`std::vector`) | Yes |
| Arena | `BitVector(arena, nbits)` | Arena-owned | **No** — call `Arena::reset()` |
| Copy from span | `BitVector(span<const uint8_t>)` | Library-owned (copy) | Yes |

### 4.4 Memory Ownership Diagram

```
Client code
  ├── ParserContext (per-thread or shared read-only)
  │     └── PD handlers (unordered_map, mutex-protected)
  │
  ├── Arena (per-thread, NOT shared)
  │     └── BitVector (bump-allocated, no individual free)
  │           └── L3Frame → parseL3() → unique_ptr<L3Message>
  │
  └── BitView (zero-copy, read-only)
        └── External buffer (caller-owned, e.g. socket, DMA ring)
```

---

## 5. Data Types

### 5.1 BitVector

**File:** `gsml3parser/bitvector.h`

A resizable bit vector with MSB-first bit ordering within each octet.  Supports
three memory modes: default (owned `std::vector`), arena-allocated, and copy from
`std::span`.

```cpp
class BitVector {
public:
    // Default (vector-owned) constructors
    BitVector();
    explicit BitVector(size_t nbits);
    BitVector(size_t nbits, unsigned char fill);
    BitVector(const std::vector<uint8_t>& bytes);
    explicit BitVector(std::span<const uint8_t> bytes);  // makes a copy

    // Arena-allocated (non-owning)
    BitVector(Arena& arena, size_t nbits);

    BitVector(const BitVector& other);
    BitVector(BitVector&& other) noexcept;
    BitVector& operator=(const BitVector& other);
    BitVector& operator=(BitVector&& other) noexcept;
    ~BitVector();

    size_t size() const;
    bool empty() const;
    void resize(size_t nbits);
    void clear();
    void reset();  // size → 0, keeps capacity (arena-like reuse)

    unsigned readField(size_t& rp, unsigned nbits) const;
    void writeField(size_t& wp, unsigned value, unsigned nbits);
    unsigned peekField(size_t rp, unsigned nbits) const;
    unsigned readBit(size_t& rp) const;
    void writeBit(size_t& wp, bool bit);

    const uint8_t* data() const;
    uint8_t*       data();

    BitVector segment(size_t offset, size_t nbits) const;
    BitVector clone() const;
    BitView view() const;  // non-owning read-only view

    bool operator==(const BitVector& other) const;
    bool operator!=(const BitVector& other) const;
};
```

**Key methods:**

| Method | Description |
|--------|-------------|
| `readField(rp, nbits)` | Read `nbits` bits from position `rp`, advance `rp` |
| `writeField(wp, value, nbits)` | Write `value` as `nbits` bits at position `wp`, advance `wp` |
| `peekField(rp, nbits)` | Read without advancing `rp` |
| `segment(offset, nbits)` | Extract a sub-segment as a new BitVector |
| `clone()` | Create a deep copy |
| `reset()` | Reset size to 0 but keep allocated capacity for reuse |
| `view()` | Create a non-owning `BitView` over the current buffer |

**Memory modes:** See [§4.3](#43-bitvector-memory-modes).

**Bit ordering:** Within each octet, bit 7 (MSB) is read first.

### 5.2 BitView

A non-owning, zero-copy, read-only view over external bytes.  See [§4.2](#42-bitview-zero-copy-read-only-view).

### 5.3 L3Frame

**File:** `gsml3parser/l3frame.h`

An L3 message container with metadata.

```cpp
class L3Frame : public BitVector {
public:
    L3Frame();
    explicit L3Frame(Primitive prim);
    L3Frame(SAPI sapi, Primitive prim);
    L3Frame(Primitive prim, size_t nbits, SAPI sapi = SAPI::SAPI0);
    L3Frame(SAPI sapi, const BitVector& source, Primitive prim = Primitive::L3_DATA);
    L3Frame(SAPI sapi, const char* hexString);

    L3PD PD() const;
    unsigned MTI() const;
    unsigned TI() const;

    Primitive primitive() const;
    bool isData() const;
    size_t length() const;
    size_t L2Length() const;
    SAPI getSAPI() const;
    void setSAPI(SAPI sapi);
    double timestamp() const;
    void setTimestamp(double ts);

    void writeH(size_t& wp) const;
    void writeL(size_t& wp) const;
    void text(std::ostream& os) const;
};
```

**Protocol fields:**

| Method | Description | Spec |
|--------|-------------|------|
| `PD()` | Protocol Discriminator | GSM 04.08 10.2 |
| `MTI()` | Message Type Indicator | GSM 04.08 10.4 |
| `TI()` | Transaction Identifier | GSM 04.07 11.2.3.1.3 |

### 5.4 L3Message

**File:** `gsml3parser/l3message.h`

Abstract base class for all L3 messages.

```cpp
class L3Message {
public:
    virtual ~L3Message() = default;
    virtual size_t l2BodyLength() const = 0;
    virtual size_t fullBodyLength() const = 0;
    size_t l2Length() const;
    size_t fullLength() const;
    size_t bitsNeeded() const;
    virtual void parse(const L3Frame& source);
    virtual void write(L3Frame& dest) const;
    std::unique_ptr<L3Frame> frame(Primitive prim) const;
    virtual L3PD PD() const = 0;
    virtual int MTI() const = 0;
    virtual unsigned TI() const { return 0; }
    virtual void text(std::ostream& os) const;
    std::string text() const;
};
```

**Length semantics:**

| Method | Description |
|--------|-------------|
| `l2BodyLength()` | Body length in bytes, excluding L3 header and rest octets |
| `fullBodyLength()` | Body length including rest octets |
| `l2Length()` | Total length with L3 header, excluding rest octets |
| `fullLength()` | Total length including L3 header and rest octets |
| `bitsNeeded()` | Total bits needed for serialization |

### 5.5 L3ProtocolElement

**File:** `gsml3parser/l3message.h`

Abstract base class for Information Elements (IEs). Supports V, TV, TLV, LV formats per GSM 04.07 11.2.1.1.4.

```cpp
class L3ProtocolElement {
public:
    virtual ~L3ProtocolElement() = default;
    virtual size_t lengthV() const = 0;
    size_t lengthTV()  const;
    size_t lengthLV()  const;
    size_t lengthTLV() const;

    virtual void parseV(const L3Frame& src, size_t& rp) = 0;
    virtual void parseV(const L3Frame& src, size_t& rp, size_t expectedLength) = 0;
    void parseLV(const L3Frame& src, size_t& rp);
    bool parseTV(unsigned IEI, const L3Frame& src, size_t& rp);
    bool parseTLV(unsigned IEI, const L3Frame& src, size_t& rp);

    virtual void writeV(L3Frame& dest, size_t& wp) const = 0;
    void writeLV(L3Frame& dest, size_t& wp) const;
    void writeTV(unsigned IEI, L3Frame& dest, size_t& wp) const;
    void writeTLV(unsigned IEI, L3Frame& dest, size_t& wp) const;

    virtual void text(std::ostream& os) const { os << "(no text())";
};
```

**IE format types:**

| Format | Encoding | Example |
|--------|----------|---------|
| V | Value only (fixed length) | RRCause (1 byte) |
| TV | Type + Value (fixed length) | IEI + value |
| TLV | Type + Length + Value | BCD Numbers |
| LV | Length + Value (variable) | MobileIdentity |

### 5.6 L3OctetAlignedProtocolElement

Convenience class for raw TLV/LV elements:

```cpp
class L3OctetAlignedProtocolElement : public L3ProtocolElement {
public:
    std::string mData;
    Bool_z mExtant;

    const unsigned char* peData() const;
    size_t lengthV() const override;
    void writeV(L3Frame& dest, size_t& wp) const override;
    void parseV(const L3Frame& src, size_t& rp, size_t expectedLength) override;
    void text(std::ostream& os) const override;
};
```

### 5.7 GenericMessageElement

For non-TLV bit-level elements (used in rest octets):

```cpp
class GenericMessageElement {
public:
    virtual ~GenericMessageElement() = default;
    virtual size_t lengthBits() const = 0;
    virtual void writeBits(L3Frame& dest, size_t& wp) const = 0;
    virtual void text(std::ostream& os) const = 0;
};
```

### 5.8 Utility Functions

**File:** `gsml3parser/l3message.h`

```cpp
// Skip an LV element (returns bits skipped)
size_t skipLV(const L3Frame& source, size_t& rp);

// Skip a TLV element (returns bits skipped, 0 if IEI doesn't match)
size_t skipTLV(unsigned IEI, const L3Frame& source, size_t& rp);

// Skip a TV element
size_t skipTV(unsigned IEI, size_t numBits, const L3Frame& source, size_t& rp);

// Check if an IE is present at current position
bool parseHasT(unsigned IEI, const L3Frame& source, size_t& rp);
```

### 5.9 Scalar Types

**File:** `gsml3parser/scalar_types.h`

Initialized scalar types that guarantee zero-initialization:

| Type | Base | Description |
|------|------|-------------|
| `Int_z` | `int` | Zero-initialized int |
| `Char_z` | `signed char` | Zero-initialized char |
| `Int16_z` | `int16_t` | Zero-initialized int16 |
| `Int32_z` | `int32_t` | Zero-initialized int32 |
| `UInt_z` | `unsigned` | Zero-initialized unsigned |
| `UChar_z` | `unsigned char` | Zero-initialized uchar |
| `UInt16_z` | `uint16_t` | Zero-initialized uint16 |
| `UInt32_z` | `uint32_t` | Zero-initialized uint32 |
| `Size_t_z` | `size_t` | Zero-initialized size_t |
| `Bool_z` | `bool` | Zero-initialized bool |
| `Float_z` | `float` | Zero-initialized float |
| `Double_z` | `double` | Zero-initialized double |

### 5.10 Protocol Types

**File:** `gsml3parser/types.h`

#### L3PD — Protocol Discriminator

```cpp
enum class L3PD : int8_t {
    GroupCallControl     = 0x00,
    BroadcastCallControl = 0x01,
    CallControl          = 0x03,
    MobilityManagement   = 0x05,
    RadioResource        = 0x06,
    GPRSMobilityManagement = 0x08,
    SMS                  = 0x09,
    GPRSSessionManagement = 0x0a,
    NonCallSS            = 0x0b,
    Location             = 0x0c,
    Extended             = 0x0e,
    TestProcedure        = 0x0f,
    Undefined            = -1
};
```

#### Primitive — Interlayer Primitives

```cpp
enum class Primitive : uint8_t {
    L2_DATA = 1,
    L3_DATA,
    L3_DATA_CONFIRM,
    L3_UNIT_DATA,
    L3_ESTABLISH_REQUEST,
    L3_ESTABLISH_INDICATION,
    L3_ESTABLISH_CONFIRM,
    L3_RELEASE_REQUEST,
    L3_RELEASE_CONFIRM,
    L3_HARDRELEASE_REQUEST,
    MDL_ERROR_INDICATION,
    L3_RELEASE_INDICATION,
    PH_CONNECT,
    HANDOVER_ACCESS
};
```

#### SAPI — Service Access Point Indicator

```cpp
enum class SAPI : uint8_t {
    SAPI0 = 0,
    SAPI3 = 3,
    SAPI0_Sacch = 4,
    SAPI3_Sacch = 7,
    Undefined = 16
};
```

#### MobileIDType

```cpp
enum class MobileIDType : uint8_t {
    NoID  = 0,
    IMSI  = 1,
    IMEI  = 2,
    IMEISV = 3,
    TMSI  = 4
};
```

#### TypeOfNumber

```cpp
enum class TypeOfNumber : uint8_t {
    Unknown         = 0,
    International   = 1,
    National        = 2,
    NetworkSpecific = 3,
    ShortCode       = 4,
    Alphanumeric    = 5,
    Abbreviated     = 6
};
```

#### NumberingPlan

```cpp
enum class NumberingPlan : uint8_t {
    Unknown = 0,
    E164    = 1,
    X121    = 3,
    F69     = 4,
    National = 8,
    Private = 9,
    ERMES   = 10
};
```

#### ChannelType

```cpp
enum class ChannelType : uint8_t {
    SCHType, FCCHType, BCCHType, CCCHType, RACHType,
    SACCHType, CBCHType, SDCCHType, FACCHType, TCHFType,
    TCHHType, AnyTCHType, PDTCHCS1Type, PDTCHCS2Type,
    PDTCHCS3Type, PDTCHCS4Type, LoopbackFullType,
    LoopbackHalfType, AnyDCCHType, UndefinedCHType
};
```

---

## 6. Information Elements

### 6.1 L3CellIdentity

**File:** `gsml3parser/common/l3common.h` | **Spec:** GSM 04.08 10.5.1.1

```cpp
class L3CellIdentity : public L3ProtocolElement {
public:
    explicit L3CellIdentity(unsigned wID = 0);
    unsigned ID() const;
    size_t lengthV() const override { return 2; }
};
```

### 6.2 L3LocationAreaIdentity

**File:** `gsml3parser/common/l3common.h` | **Spec:** GSM 04.08 10.5.1.3

```cpp
class L3LocationAreaIdentity : public L3ProtocolElement {
public:
    L3LocationAreaIdentity(const char* wMCC = "250", const char* wMNC = "01", unsigned wLAC = 1);
    bool operator==(const L3LocationAreaIdentity&) const;
    int MCC() const;
    int MNC() const;
    int LAC() const;
    size_t lengthV() const override { return 5; }
};
```

### 6.3 L3MobileIdentity

**File:** `gsml3parser/common/l3common.h` | **Spec:** GSM 04.08 10.5.1.4

```cpp
class L3MobileIdentity : public L3ProtocolElement {
public:
    L3MobileIdentity();
    explicit L3MobileIdentity(uint32_t wTMSI);
    explicit L3MobileIdentity(const char* wDigits);

    MobileIDType type() const;
    const char* digits() const;
    uint32_t TMSI() const;
    bool isIMSI() const;
    bool isTMSI() const;

    bool operator==(const L3MobileIdentity&) const;
    bool operator!=(const L3MobileIdentity& other) const;
    bool operator<(const L3MobileIdentity&) const;
    size_t lengthV() const override;
};
```

### 6.4 L3MobileStationClassmark1

**File:** `gsml3parser/common/l3common.h` | **Spec:** GSM 04.08 10.5.1.5

```cpp
class L3MobileStationClassmark1 : public L3ProtocolElement {
public:
    size_t lengthV() const override { return 1; }
};
```

### 6.5 L3MobileStationClassmark2

**File:** `gsml3parser/common/l3common.h` | **Spec:** GSM 04.08 10.5.1.5

```cpp
class L3MobileStationClassmark2 : public L3ProtocolElement {
public:
    size_t lengthV() const override { return 3; }
    int getA5Bits() const;
    int powerClass() const;
};
```

### 6.6 L3MobileStationClassmark3

**File:** `gsml3parser/common/l3common.h` | **Spec:** GSM 04.08 10.5.1.7

```cpp
class L3MobileStationClassmark3 : public L3ProtocolElement {
public:
    L3MobileStationClassmark3();
    size_t lengthV() const override { return 14; }
};
```

### 6.7 L3CipheringKeySequenceNumber

**File:** `gsml3parser/common/l3common.h`

```cpp
class L3CipheringKeySequenceNumber : public L3ProtocolElement {
public:
    explicit L3CipheringKeySequenceNumber(unsigned wCIValue = 0);
    size_t lengthV() const override { return 0; }
};
```

### 6.8 L3CipheringModeSetting

**File:** `gsml3parser/common/l3common.h` | **Spec:** GSM 04.08 10.5.2.11

4-bit field: 3-bit ciphering algorithm + 1-bit ciphering mode flag.

```cpp
class L3CipheringModeSetting : public GenericMessageElement {
public:
    L3CipheringModeSetting();
    explicit L3CipheringModeSetting(bool wCiphering, int wAlgorithm);
    bool ciphering() const;
    int algorithm() const;
    size_t lengthBits() const override { return 4; }
};
```

### 6.9 L3CipheringModeResponse

**File:** `gsml3parser/common/l3common.h` | **Spec:** GSM 04.08 10.5.2.12

4-bit field: 3 spare bits + 1-bit includeIMEISV flag.

```cpp
class L3CipheringModeResponse : public GenericMessageElement {
public:
    L3CipheringModeResponse();
    explicit L3CipheringModeResponse(bool wIncludeIMEISV);
    bool includeIMEISV() const;
    size_t lengthBits() const override { return 4; }
};
```

### 6.10 L3SI3RestOctets

**File:** `gsml3parser/common/l3common.h` | **Spec:** GSM 04.08 10.5.2.24

Rest octets for System Information Type 3, containing optional CellSelectionInfo and NeighbourCellDescription.

```cpp
class L3SI3RestOctets : public L3ProtocolElement {
public:
    L3SI3RestOctets();
    bool hasCellSelection() const;
    const L3CellSelectionInfo& cellSelection() const;
    bool hasNeighbourCell() const;
    const L3NeighbourCellDescription& neighbourCell() const;
    size_t lengthV() const override;
};
```

---

## 7. Radio Resource Messages (PD=0x06)

**File:** `gsml3parser/rr/l3rrmessages.h` | **Spec:** GSM 04.08 9.1

### 7.1 L3RRMessage

Base class for all RR messages.

```cpp
class L3RRMessage : public L3Message {
public:
    enum MessageType : int {
        SystemInformationType1  = 0x19,
        SystemInformationType2  = 0x1a,
        SystemInformationType2bis = 0x1f,
        SystemInformationType2ter = 0x14,
        SystemInformationType3  = 0x1b,
        SystemInformationType4  = 0x1c,
        SystemInformationType5  = 0x1d,
        SystemInformationType5bis = 0x20,
        SystemInformationType5ter = 0x23,
        SystemInformationType6  = 0x1e,
        SystemInformationType7  = 0x15,
        SystemInformationType8  = 0x16,
        SystemInformationType9  = 0x17,
        SystemInformationType13 = 0x00,
        SystemInformationType16 = 0x01,
        SystemInformationType17 = 0x04,
        AssignmentCommand       = 0x2e,
        AssignmentComplete      = 0x29,
        AssignmentFailure       = 0x2f,
        ChannelRelease          = 0x0d,
        ImmediateAssignment     = 0x3f,
        ImmediateAssignmentReject = 0x3a,
        PagingRequestType1      = 0x21,
        PagingRequestType2      = 0x22,
        PagingRequestType3      = 0x24,
        PagingResponse          = 0x27,
        PhysicalInformation     = 0x26,
        HandoverCommand         = 0x2b,
        HandoverComplete        = 0x2c,
        HandoverFailure         = 0x28,
        CipheringModeCommand    = 0x35,
        CipheringModeComplete   = 0x32,
        ChannelModeModify       = 0x10,
        RRStatus                = 0x12,
        ClassmarkChange         = 0x16,
        ClassmarkEnquiry        = 0x13,
        MeasurementReport       = 0x15,
        GPRSSuspensionRequest   = 0x34,
        ApplicationInformation  = 0x38,
        // ...
    };

    static const char* name(MessageType mt);
    L3PD PD() const override { return L3PD::RadioResource; }
};
```

### 7.2 L3RRMessageNRO / L3RRMessageRO

```cpp
class L3RRMessageNRO : public L3RRMessage {
    // No rest octets
    size_t fullBodyLength() const override { return l2BodyLength(); }
};

class L3RRMessageRO : public L3RRMessage {
    // With rest octets
    virtual size_t restOctetsLength() const = 0;
    size_t fullBodyLength() const override { return l2BodyLength() + restOctetsLength(); }
};
```

### 7.3 L3PagingRequestType1

**Spec:** GSM 04.08 9.1.22 — V-format: PageMode(4 bits) + ChannelNeeded(2 bits) + MobileIdentity(LV) [+ second MobileIdentity(TLV)]

```cpp
class L3PagingRequestType1 : public L3RRMessageNRO {
public:
    L3PagingRequestType1();
    L3PagingRequestType1(const L3MobileIdentity& wId, ChannelType wType);
    L3PagingRequestType1(const L3MobileIdentity& wId1, ChannelType wType1,
                         const L3MobileIdentity& wId2, ChannelType wType2);
    const L3MobileIdentity& mobileID() const;
    ChannelType channelType() const;
    int MTI() const override { return PagingRequestType1; }
};
```

PageMode is a 4-bit (half-octet) field. Two-ID constructor supports dual paging.
```

### 7.4 L3PagingResponse

**Spec:** GSM 04.08 9.1.25 — V-format: MobileIdentity [+ Classmark2/3]

```cpp
class L3PagingResponse : public L3RRMessageNRO {
public:
    const L3MobileIdentity& mobileID() const;
    const L3MobileStationClassmark2& classmark() const;
    int MTI() const override { return PagingResponse; }
};
```

### 7.5 L3ChannelRelease

**Spec:** GSM 04.08 9.1.7 — V-format: Cause [+ GPRSResumption]

```cpp
class L3ChannelRelease : public L3RRMessageNRO {
public:
    explicit L3ChannelRelease(RRCause cause = RRCause::Normal_Event);
    RRCause cause() const;
    bool hasGprsResumption() const;
    bool gprsResumption() const;
    int MTI() const override { return ChannelRelease; }
    size_t l2BodyLength() const override;
};
```

l2BodyLength is dynamic: 1 + (GPRS resumption ? 1 : 0).
```

### 7.6 L3RRStatus

**Spec:** GSM 04.08 9.1.29

```cpp
class L3RRStatus : public L3RRMessageNRO {
public:
    RRCause cause() const;
    int MTI() const override { return RRStatus; }
};
```

### 7.7 L3AssignmentCommand

**Spec:** GSM 04.08 9.1.2 — V-format: ChannelDescription + ChannelMode + PowerCommand [+ Mode1] + [+ MultiRate]

```cpp
class L3AssignmentCommand : public L3RRMessageNRO {
public:
    L3AssignmentCommand();
    const L3ChannelDescription& description() const;
    const L3ChannelMode& mode() const;
    const L3PowerCommand& powerCommand() const;
    bool hasMode1() const;
    const L3ChannelMode& mode1() const;
    bool isAMR() const;
    const L3MultiRateConfiguration& multiRate() const;
    int MTI() const override { return AssignmentCommand; }
};
```

Power command is mandatory. Optional Mode1 and MultiRate fields supported for AMR.

### 7.8 L3AssignmentComplete

**Spec:** GSM 04.08 9.1.3

```cpp
class L3AssignmentComplete : public L3RRMessageNRO {
public:
    RRCause cause() const;
    int MTI() const override { return AssignmentComplete; }
};
```

### 7.9 L3AssignmentFailure

**Spec:** GSM 04.08 9.1.3

```cpp
class L3AssignmentFailure : public L3RRMessageNRO {
public:
    RRCause cause() const;
    int MTI() const override { return AssignmentFailure; }
};
```

### 7.10 L3ClassmarkEnquiry

**Spec:** GSM 04.08 9.1.14

```cpp
class L3ClassmarkEnquiry : public L3RRMessageNRO {
public:
    int MTI() const override { return ClassmarkEnquiry; }
    size_t l2BodyLength() const override { return 0; }
};
```

### 7.11 L3ClassmarkChange

**Spec:** GSM 04.08 9.1.11

```cpp
class L3ClassmarkChange : public L3RRMessageNRO {
public:
    const L3MobileStationClassmark2& classmark() const;
    int MTI() const override { return ClassmarkChange; }
};
```

### 7.12 L3MeasurementReport

**Spec:** GSM 04.08 9.1.21

```cpp
class L3MeasurementReport : public L3RRMessageNRO {
public:
    int MTI() const override { return MeasurementReport; }
    size_t l2BodyLength() const override { return 16; }
};
```

### 7.13 L3CipheringModeCommand

**Spec:** GSM 04.08 9.1.9 — V-format: L3CipheringModeSetting (4 bits: 3 algorithm + 1 ciphering flag) + L3CipheringModeResponse (4 bits: 3 spare + 1 includeIMEISV) + CipheringKeySequenceNumber

```cpp
class L3CipheringModeCommand : public L3RRMessageNRO {
public:
    L3CipheringModeCommand(bool ciphering, int algorithm);
    const L3CipheringModeSetting& modeSetting() const;
    const L3CipheringModeResponse& modeResponse() const;
    int MTI() const override;
};
```

### 7.14 L3CipheringModeComplete

**Spec:** GSM 04.08 9.1.10

```cpp
class L3CipheringModeComplete : public L3RRMessageNRO {
public:
    int MTI() const override;
    size_t l2BodyLength() const override { return 0; }
};
```

### 7.15 L3HandoverCommand

**Spec:** GSM 04.08 9.1.15 — V-format: CellDescription + ChannelDesc2 + HandoverRef + PowerCmd + SyncInd

```cpp
class L3HandoverCommand : public L3RRMessageNRO {
public:
    L3HandoverCommand();
    const L3CellDescription& cellDescription() const;
    const L3ChannelDescription2& channelDescriptionAfter() const;
    const L3HandoverReference& handoverReference() const;
    const L3PowerCommandAndAccessType& powerCommandAccessType() const;
    const L3SynchronizationIndication& syncIndication() const;
    int MTI() const override { return HandoverCommand; }
};
```

### 7.16 L3HandoverComplete

**Spec:** GSM 04.08 9.1.16

```cpp
class L3HandoverComplete : public L3RRMessageNRO {
public:
    RRCause cause() const;
    int MTI() const override { return HandoverComplete; }
};
```

### 5.16 L3HandoverFailure

**Spec:** GSM 04.08 9.1.17

```cpp
class L3HandoverFailure : public L3RRMessageNRO {
public:
    RRCause cause() const;
    int MTI() const override { return HandoverFailure; }
};
```

### 5.17 L3ChannelModeModify

**Spec:** GSM 04.08 9.1.5 — V-format: ChannelDescription + ChannelMode [+ MultiRate for AMR]

```cpp
class L3ChannelModeModify : public L3RRMessageNRO {
public:
    L3ChannelModeModify();
    L3ChannelModeModify(const L3ChannelDescription& wDesc, const L3ChannelMode& wMode);
    bool isAMR() const;
    const L3ChannelDescription& description() const;
    const L3ChannelMode& mode() const;
    int MTI() const override { return ChannelModeModify; }
};
```

### 5.18 L3ChannelModeModifyAcknowledge

**Spec:** GSM 04.08 9.1.6 — V-format: ChannelDescription + ChannelMode

```cpp
class L3ChannelModeModifyAcknowledge : public L3RRMessageNRO {
public:
    const L3ChannelDescription& description() const;
    const L3ChannelMode& mode() const;
    int MTI() const override { return ChannelModeModifyAcknowledge; }
};
```

### 5.19 L3GPRSSuspensionRequest

**Spec:** GSM 04.08 9.1.13b

```cpp
class L3GPRSSuspensionRequest : public L3RRMessageNRO {
public:
    uint32_t mTLLI;
    std::vector<uint8_t> mRaId;
    uint8_t mSuspensionCause;
    uint8_t mServiceSupport;
    int MTI() const override { return GPRSSuspensionRequest; }
};
```

### 5.20 L3ApplicationInformation

**Spec:** GSM 04.08 9.1.53 (RRLP encapsulation)

```cpp
class L3ApplicationInformation : public L3RRMessageNRO {
public:
    L3ApplicationInformation();
    L3ApplicationInformation(BitVector data, unsigned protocolId = 0,
                              unsigned cr = 0, unsigned first = 0, unsigned last = 0);

    unsigned protocolIdentifier() const;
    unsigned CR() const;
    unsigned firstSegment() const;
    unsigned lastSegment() const;
    const BitVector& data() const;
    int MTI() const override { return ApplicationInformation; }
};
```

### 5.21 L3ImmediateAssignment

**Spec:** GSM 04.08 9.1.19 — V-format: PageMode + DedicatedModeOrTBF + RequestReference + ChannelDescription + TimingAdvance [+ optional StartTime] + MobileAllocation (LV format)

```cpp
class L3ImmediateAssignment : public L3RRMessageNRO {
public:
    L3ImmediateAssignment();
    const L3ChannelDescription& channelDescription() const;
    const L3RequestReference& requestReference() const;
    const L3TimingAdvance& timingAdvance() const;
    const L3MobileAllocation& mobileAllocation() const;
    bool hasStartTime() const;
    uint32_t startTimeFrame() const;
    int MTI() const override { return ImmediateAssignment; }
};
```

### 5.22 L3ImmediateAssignmentReject

**Spec:** GSM 04.08 9.1.20 — Fixed 17-byte body: PageMode(1 bit) + 4 paired (RequestReference + WaitIndication) entries

```cpp
class L3ImmediateAssignmentReject : public L3RRMessageNRO {
public:
    L3ImmediateAssignmentReject();
    explicit L3ImmediateAssignmentReject(unsigned waitSeconds);
    unsigned waitTime() const;
    const std::vector<L3RequestReference>& requestReferences() const;
    int MTI() const override { return ImmediateAssignmentReject; }
};
```

### 5.21a L3SystemInformationType1

**Spec:** GSM 04.08 9.1.31

```cpp
class L3SystemInformationType1 : public L3RRMessageNRO {
public:
    int MTI() const override { return SystemInformationType1; }
    size_t l2BodyLength() const override { return 19; }
};
```

### 5.21b L3SystemInformationType2

**Spec:** GSM 04.08 9.1.32 — V-format: BCCHFrequencyList + NCCPermitted + RACHControlParameters

```cpp
class L3SystemInformationType2 : public L3RRMessageNRO {
public:
    L3SystemInformationType2();
    const L3BCCHFrequencyList& bcchFrequencyList() const;
    const L3NCCPermitted& nccPermitted() const;
    const L3RACHControlParameters& rachaControl() const;
    int MTI() const override { return SystemInformationType2; }
    size_t l2BodyLength() const override { return 20; }
};
```

### 5.21c L3SystemInformationType2bis

**Spec:** GSM 04.08 9.1.32a — V-format: BCCHFrequencyList + NCCPermitted + RACHControlParameters

```cpp
class L3SystemInformationType2bis : public L3RRMessageNRO {
public:
    L3SystemInformationType2bis();
    const L3BCCHFrequencyList& bcchFrequencyList() const;
    const L3NCCPermitted& nccPermitted() const;
    const L3RACHControlParameters& rachaControl() const;
    int MTI() const override { return SystemInformationType2bis; }
    size_t l2BodyLength() const override { return 20; }
};
```

### 5.21d L3SystemInformationType2ter

**Spec:** GSM 04.08 9.1.32b — V-format: BCCHFrequencyList + NCCPermitted + RACHControlParameters

```cpp
class L3SystemInformationType2ter : public L3RRMessageNRO {
public:
    L3SystemInformationType2ter();
    const L3BCCHFrequencyList& bcchFrequencyList() const;
    const L3NCCPermitted& nccPermitted() const;
    const L3RACHControlParameters& rachaControl() const;
    int MTI() const override { return SystemInformationType2ter; }
    size_t l2BodyLength() const override { return 20; }
};
```

### 5.22 L3SystemInformationType3

**Spec:** GSM 04.08 9.1.35 — V-format + RestOctets (CellSelectionInfo, NeighbourCellDescription)

```cpp
class L3SystemInformationType3 : public L3RRMessageRO {
public:
    int MTI() const override { return SystemInformationType3; }
    size_t l2BodyLength() const override { return 16; }
    size_t restOctetsLength() const override;
    const L3SI3RestOctets& restOctets() const;
};
```

### 5.22a L3SystemInformationType4

**Spec:** GSM 04.08 9.1.36 — V-format: LAI + CI + CellSelectionParameters + CellOptionsBCCH + RACHControlParameters [+ CBCH + RestOctets]

```cpp
class L3SystemInformationType4 : public L3RRMessageRO {
public:
    L3SystemInformationType4();
    const L3LocationAreaIdentity& LAI() const;
    const L3CellIdentity& CI() const;
    const L3CellSelectionParameters& cellSelectionParameters() const;
    const L3CellOptionsBCCH& cellOptions() const;
    const L3RACHControlParameters& rachaControl() const;
    bool hasCBCH() const;
    const L3CBCHChannelDescription& cbchChannelDescription() const;
    const L3SIType4RestOctets& restOctets() const;
    int MTI() const override { return SystemInformationType4; }
    size_t l2BodyLength() const override { return 13; }
    size_t restOctetsLength() const override;
};
```

### 5.23 L3SystemInformationType5

**Spec:** GSM 04.08 9.1.37 — V-format: BCCHFrequencyList

```cpp
class L3SystemInformationType5 : public L3RRMessageNRO {
public:
    L3SystemInformationType5();
    const L3BCCHFrequencyList& bcchFrequencyList() const;
    int MTI() const override { return SystemInformationType5; }
    size_t l2BodyLength() const override { return 16; }
};
```

### 5.23a L3SystemInformationType5bis

**Spec:** GSM 04.08 9.1.37a — V-format: BCCHFrequencyList

```cpp
class L3SystemInformationType5bis : public L3RRMessageNRO {
public:
    L3SystemInformationType5bis();
    const L3BCCHFrequencyList& bcchFrequencyList() const;
    int MTI() const override { return SystemInformationType5bis; }
    size_t l2BodyLength() const override { return 16; }
};
```

### 5.23b L3SystemInformationType5ter

**Spec:** GSM 04.08 9.1.37b — V-format: BCCHFrequencyList

```cpp
class L3SystemInformationType5ter : public L3RRMessageNRO {
public:
    L3SystemInformationType5ter();
    const L3BCCHFrequencyList& bcchFrequencyList() const;
    int MTI() const override { return SystemInformationType5ter; }
    size_t l2BodyLength() const override { return 16; }
};
```

### 5.23c L3SystemInformationType6

**Spec:** GSM 04.08 9.1.38 — V-format: CI + LAI + CellOptionsSACCH + NCCPermitted

```cpp
class L3SystemInformationType6 : public L3RRMessageNRO {
public:
    L3SystemInformationType6();
    const L3CellIdentity& CI() const;
    const L3LocationAreaIdentity& LAI() const;
    const L3CellOptionsSACCH& cellOptions() const;
    const L3NCCPermitted& nccPermitted() const;
    int MTI() const override { return SystemInformationType6; }
    size_t l2BodyLength() const override { return 9; }
};
```

### 5.23d L3SystemInformationType7

**Spec:** GSM 04.08 9.1.39 — TV-format

```cpp
class L3SystemInformationType7 : public L3RRMessageNRO {
public:
    L3SystemInformationType7();
    int MTI() const override { return SystemInformationType7; }
};
```

### 5.23e L3SystemInformationType8

**Spec:** GSM 04.08 9.1.40 — TV-format: NCCPermitted

```cpp
class L3SystemInformationType8 : public L3RRMessageNRO {
public:
    L3SystemInformationType8();
    const L3NCCPermitted& nccPermitted() const;
    int MTI() const override { return SystemInformationType8; }
};
```

### 5.23f L3SystemInformationType9

**Spec:** GSM 04.08 9.1.41 — V-format: CI + CellSelectionParameters + CellOptionsBCCH

```cpp
class L3SystemInformationType9 : public L3RRMessageNRO {
public:
    L3SystemInformationType9();
    const L3CellIdentity& CI() const;
    const L3CellSelectionParameters& cellSelectionParameters() const;
    const L3CellOptionsBCCH& cellOptions() const;
    int MTI() const override { return SystemInformationType9; }
    size_t l2BodyLength() const override { return 5; }
};
```

### 5.23g L3SystemInformationType13

**Spec:** GSM 04.08 9.1.43a

```cpp
class L3SystemInformationType13 : public L3RRMessageRO {
public:
    int MTI() const override { return SystemInformationType13; }
    size_t l2BodyLength() const override { return 0; }
    size_t restOctetsLength() const override;
};
```

### 5.23h L3SystemInformationType16

**Spec:** GSM 04.08 9.1.43d — V-format: CI + CellSelectionParameters + CellOptionsBCCH

```cpp
class L3SystemInformationType16 : public L3RRMessageNRO {
public:
    L3SystemInformationType16();
    const L3CellIdentity& CI() const;
    const L3CellSelectionParameters& cellSelectionParameters() const;
    const L3CellOptionsBCCH& cellOptions() const;
    int MTI() const override { return SystemInformationType16; }
    size_t l2BodyLength() const override { return 5; }
};
```

### 5.23i L3SystemInformationType17

**Spec:** GSM 04.08 9.1.43e — TV-format

```cpp
class L3SystemInformationType17 : public L3RRMessageNRO {
public:
    L3SystemInformationType17();
    int MTI() const override { return SystemInformationType17; }
};
```

### 5.24 L3PhysicalInformation

**Spec:** GSM 04.08 9.1.26 — V-format: TimingAdvance

```cpp
class L3PhysicalInformation : public L3RRMessageNRO {
public:
    L3PhysicalInformation();
    const L3TimingAdvance& timingAdvance() const;
    int MTI() const override { return PhysicalInformation; }
};
```

---

## 8. Mobility Management Messages (PD=0x05)

**File:** `gsml3parser/mm/l3mmmessages.h` | **Spec:** GSM 04.08 9.2

### 6.0 MM Information Elements

**File:** `gsml3parser/mm/l3mmlements.h`

#### L3CMServiceType

**Spec:** GSM 04.08 10.5.3.3

```cpp
class L3CMServiceType : public L3ProtocolElement {
public:
    enum TypeCode : uint8_t {
        UndefinedType = 0,
        MobileOriginatedCall = 1,
        EmergencyCall = 2,
        ShortMessage = 4,
        SupplementaryService = 8,
        VoiceCallGroup = 9,
        VoiceBroadcast = 10,
        LocationService = 11
    };
    explicit L3CMServiceType(TypeCode wType = UndefinedType);
    TypeCode type() const;
    bool isCC() const;
    bool isSMS() const;
    bool isMM() const;
    size_t lengthV() const override { return 0; }
};
```

#### L3RejectCauseIE

**Spec:** GSM 04.08 10.5.3.6

```cpp
class L3RejectCauseIE : public L3ProtocolElement {
public:
    explicit L3RejectCauseIE(MMRejectCause wCause = MMRejectCause::Zero);
    MMRejectCause rejectCause() const;
    size_t lengthV() const override { return 1; }
};
```

#### L3NetworkName

**Spec:** GSM 04.08 10.5.3.5a

```cpp
class L3NetworkName : public L3ProtocolElement {
public:
    L3NetworkName(const char* wName = "", GSMAlphabet alphabet = GSMAlphabet::ALPHABET_7BIT, int wCI = 0);
    const char* name() const;
    GSMAlphabet alphabet() const;
    size_t lengthV() const override;
};
```

#### L3TimeZoneAndTime

**Spec:** GSM 04.08 10.5.3.9

```cpp
class L3TimeZoneAndTime : public L3ProtocolElement {
public:
    enum TimeType : uint8_t { LOCAL_TIME, UTC_TIME };
    L3TimeZoneAndTime(TimeType type = UTC_TIME);
    uint8_t year() const;
    uint8_t month() const;
    uint8_t day() const;
    uint8_t hour() const;
    uint8_t minute() const;
    uint8_t timezone() const;
    TimeType type() const;
    size_t lengthV() const override { return 7; }
};
```

#### L3RAND

**Spec:** GSM 04.08 10.5.3.1

```cpp
class L3RAND : public L3ProtocolElement {
public:
    L3RAND();
    explicit L3RAND(const std::vector<uint8_t>& rand);
    const std::vector<uint8_t>& rand() const;
    size_t lengthV() const override { return 16; }
};
```

#### L3SRES

**Spec:** GSM 04.08 10.5.3.2

```cpp
class L3SRES : public L3ProtocolElement {
public:
    explicit L3SRES(uint32_t wValue = 0);
    uint32_t value() const;
    size_t lengthV() const override { return 4; }
};
```

### 6.1 L3MMMessage

Base class for all MM messages.

```cpp
class L3MMMessage : public L3Message {
public:
    enum MessageType : int {
        IMSIDetachIndication      = 0x01,
        CMServiceAccept           = 0x21,
        CMServiceReject           = 0x22,
        CMServiceAbort            = 0x23,
        CMServiceRequest          = 0x24,
        CMReestablishmentRequest  = 0x28,
        IdentityResponse          = 0x19,
        IdentityRequest           = 0x18,
        MMInformation             = 0x32,
        LocationUpdatingAccept    = 0x02,
        LocationUpdatingReject    = 0x04,
        LocationUpdatingRequest   = 0x08,
        TMSIReallocationCommand   = 0x1a,
        TMSIReallocationComplete  = 0x1b,
        MMStatus                  = 0x31,
        AuthenticationRequest     = 0x12,
        AuthenticationResponse    = 0x14,
        AuthenticationReject      = 0x11,
        Undefined                 = -1
    };

    size_t fullBodyLength() const override { return l2BodyLength(); }
    L3PD PD() const override { return L3PD::MobilityManagement; }
};
```

### 6.2 LocationUpdateType

```cpp
enum class LocationUpdateType : uint8_t {
    Normal = 0,
    Periodic = 1,
    IMSIAttach = 2
};
```

### 6.3 L3LocationUpdatingRequest

**Spec:** GSM 04.08 9.2.15 — V-format: MobileIdentity(LV) + LAI + FollowOnRequest(1 bit) + LocationUpdatingType(1 bit) + EMLPP(1 bit) + Rest

```cpp
class L3LocationUpdatingRequest : public L3MMMessage {
public:
    const L3MobileIdentity& mobileID() const;
    const L3LocationAreaIdentity& LAI() const;
    LocationUpdateType getLocationUpdatingType() const;
    bool getFollowOnRequest() const;
    int MTI() const override { return LocationUpdatingRequest; }
};
```

### 6.4 L3LocationUpdatingAccept

**Spec:** GSM 04.08 9.2.13

```cpp
class L3LocationUpdatingAccept : public L3MMMessage {
public:
    L3LocationUpdatingAccept(const L3LocationAreaIdentity& wLAI, bool wFollowOn = false);
    L3LocationUpdatingAccept(const L3LocationAreaIdentity& wLAI,
                             const L3MobileIdentity& wID, bool wFollowOn = false);
    int MTI() const override { return LocationUpdatingAccept; }
};
```

### 6.5 L3LocationUpdatingReject

**Spec:** GSM 04.08 9.2.14

```cpp
class L3LocationUpdatingReject : public L3MMMessage {
public:
    explicit L3LocationUpdatingReject(MMRejectCause cause);
    int MTI() const override { return LocationUpdatingReject; }
};
```

### 6.6 L3IMSIDetachIndication

**Spec:** GSM 04.08 9.2.15

```cpp
class L3IMSIDetachIndication : public L3MMMessage {
public:
    const L3MobileIdentity& mobileID() const;
    int MTI() const override { return IMSIDetachIndication; }
};
```

### 6.7 L3CMServiceAccept

**Spec:** GSM 04.08 9.2.5

```cpp
class L3CMServiceAccept : public L3MMMessage {
public:
    int MTI() const override { return CMServiceAccept; }
    size_t l2BodyLength() const override { return 0; }
};
```

### 6.8 L3CMServiceAbort

**Spec:** GSM 04.08 9.2.7 — V-format: CMServiceType [+ Cause(optional)]

```cpp
class L3CMServiceAbort : public L3MMMessage {
public:
    L3CMServiceAbort();
    L3CMServiceAbort(MMRejectCause cause);
    L3CMServiceType::TypeCode serviceType() const;
    bool hasCause() const;
    MMRejectCause cause() const;
    int MTI() const override { return CMServiceAbort; }
    size_t l2BodyLength() const override;
};
```

### 6.9 L3CMServiceReject

**Spec:** GSM 04.08 9.2.6

```cpp
class L3CMServiceReject : public L3MMMessage {
public:
    explicit L3CMServiceReject(MMRejectCause cause);
    int MTI() const override { return CMServiceReject; }
};
```

### 6.10 L3CMServiceRequest

**Spec:** GSM 04.08 9.2.9

```cpp
class L3CMServiceRequest : public L3MMMessage {
public:
    const L3MobileIdentity& mobileID() const;
    L3CMServiceType::TypeCode serviceType() const;
    int MTI() const override { return CMServiceRequest; }
};
```

### 6.11 L3CMReestablishmentRequest

**Spec:** GSM 04.08 9.2.4

```cpp
class L3CMReestablishmentRequest : public L3MMMessage {
public:
    const L3MobileIdentity& mobileID() const;
    int MTI() const override { return CMReestablishmentRequest; }
};
```

### 6.12 L3MMInformation

**Spec:** GSM 04.08 9.2.15a

```cpp
class L3MMInformation : public L3MMMessage {
public:
    int MTI() const override { return MMInformation; }
};
```

### 6.13 L3IdentityRequest

**Spec:** GSM 04.08 9.2.10

```cpp
class L3IdentityRequest : public L3MMMessage {
public:
    explicit L3IdentityRequest(MobileIDType type);
    int MTI() const override { return IdentityRequest; }
};
```

### 6.14 L3IdentityResponse

**Spec:** GSM 04.08 9.2.11

```cpp
class L3IdentityResponse : public L3MMMessage {
public:
    const L3MobileIdentity& mobileID() const;
    int MTI() const override { return IdentityResponse; }
};
```

### 6.15 L3AuthenticationRequest

**Spec:** GSM 04.08 9.2.2

```cpp
class L3AuthenticationRequest : public L3MMMessage {
public:
    L3AuthenticationRequest(unsigned ckSN, const std::vector<uint8_t>& rand);
    int MTI() const override { return AuthenticationRequest; }
    size_t l2BodyLength() const override { return 17; }
};
```

### 6.16 L3AuthenticationResponse

**Spec:** GSM 04.08 9.2.3

```cpp
class L3AuthenticationResponse : public L3MMMessage {
public:
    uint32_t SRES() const;
    int MTI() const override { return AuthenticationResponse; }
    size_t l2BodyLength() const override { return 4; }
};
```

### 6.17 L3AuthenticationReject

**Spec:** GSM 04.08 9.2.1

```cpp
class L3AuthenticationReject : public L3MMMessage {
public:
    int MTI() const override { return AuthenticationReject; }
    size_t l2BodyLength() const override { return 0; }
};
```

### 6.18 L3TMSIReallocationComplete

**Spec:** GSM 04.08 9.2.18

```cpp
class L3TMSIReallocationComplete : public L3MMMessage {
public:
    int MTI() const override { return TMSIReallocationComplete; }
    size_t l2BodyLength() const override { return 0; }
};
```

### 6.19 L3MMStatus

**Spec:** GSM 04.08 9.2.15

```cpp
class L3MMStatus : public L3MMMessage {
public:
    MMRejectCause cause() const;
    int MTI() const override { return MMStatus; }
    size_t l2BodyLength() const override { return 3; }
};
```

---

## 9. Call Control Messages (PD=0x03)

**File:** `gsml3parser/cc/l3ccmessages.h` | **Spec:** GSM 04.08 9.3 / ISDN Q.931

### 7.0 CC Mixin Classes

CC messages use mixin classes for shared functionality.

**File:** `gsml3parser/cc/l3cclements.h`

#### L3CCCapabilities

Provides bearer capability and codec list IEs:

```cpp
class L3CCCapabilities {
public:
    L3BearerCapability mBearerCapability;
    L3SupportedCodecList mSupportedCodecs;
    std::string getCodecSet() const;
};
```

#### L3CCCommonIEs

Provides supplementary service facility and version indicator IEs:

```cpp
class L3CCCommonIEs {
public:
    bool mHaveFacility;
    L3SupServFacilityIE mFacility;
    bool mHaveSSVersion;
    L3SupServVersionIndicator mSSVersion;
    void ccCommonText(std::ostream&) const;
    void ccCommonParse(const L3Frame& src, size_t& rp);
    void ccCommonWrite(L3Frame& dest, size_t& wp) const;
    size_t ccCommonLength() const;
};
```

#### L3BearerCapability

**Spec:** GSM 04.08 10.5.4.5

```cpp
class L3BearerCapability : public L3ProtocolElement {
public:
    L3BearerCapability();
    bool isPresent() const;
    uint8_t octet3() const;
    const std::vector<uint8_t>& octet3a() const;
    bool getHalfRateSupport() const;
    size_t lengthV() const override;
};
```

#### L3SupportedCodecList

**Spec:** GSM 04.08 10.5.4.32

```cpp
class L3SupportedCodecList : public L3ProtocolElement {
public:
    L3SupportedCodecList();
    bool isGsmPresent() const;
    bool isUmtsPresent() const;
    const std::vector<uint8_t>& gsmCodecs() const;
    const std::vector<uint8_t>& umtsCodecs() const;
    size_t lengthV() const override;
};
```

#### L3CalledPartyBCDNumber

**Spec:** GSM 04.08 10.5.4.7

```cpp
class L3CalledPartyBCDNumber : public L3ProtocolElement {
public:
    L3CalledPartyBCDNumber();
    explicit L3CalledPartyBCDNumber(const char* wDigits);
    TypeOfNumber type() const;
    NumberingPlan plan() const;
    const char* digits() const;
    size_t lengthV() const override;
};
```

#### L3CallingPartyBCDNumber

**Spec:** GSM 04.08 10.5.4.9

```cpp
class L3CallingPartyBCDNumber : public L3ProtocolElement {
public:
    L3CallingPartyBCDNumber();
    explicit L3CallingPartyBCDNumber(const char* wDigits);
    TypeOfNumber type() const;
    NumberingPlan plan() const;
    const char* digits() const;
    size_t lengthV() const override;
};
```

#### L3CauseElement

**Spec:** GSM 04.08 10.5.4.11

```cpp
class L3CauseElement : public L3ProtocolElement {
public:
    using Location = CCCauseLocation;
    using Cause = CCCause;
    L3CauseElement(Cause wCause = Cause::Normal_Call_Clearing,
                   Location wLocation = Location::Private_Serving_Local);
    Location location() const;
    Cause cause() const;
    size_t lengthV() const override { return 2; }
};
```

#### L3ProgressIndicator

**Spec:** GSM 04.08 10.5.4.21

```cpp
class L3ProgressIndicator : public L3ProtocolElement {
public:
    enum Location : uint8_t { User, PrivateServingLocal, PublicServingLocal, ... };
    enum Progress : uint8_t { Unspecified, NotISDN, DestinationNotISDN, ... };
    L3ProgressIndicator(Progress wProgress = Unspecified,
                        Location wLocation = PrivateServingLocal);
    Location location() const;
    Progress progress() const;
    size_t lengthV() const override { return 2; }
};
```

### 7.1 L3CCMessage

Base class for all CC messages. Includes TI (Transaction Identifier) in header.

```cpp
class L3CCMessage : public L3Message {
public:
    enum MessageType : int {
        Alerting           = 0x01,
        CallConfirmed      = 0x08,
        CallProceeding     = 0x02,
        Connect            = 0x07,
        Setup              = 0x05,
        EmergencySetup     = 0x0e,
        ConnectAcknowledge = 0x0f,
        Progress           = 0x03,
        Disconnect         = 0x25,
        Release            = 0x2d,
        ReleaseComplete    = 0x2a,
        StartDTMF          = 0x35,
        StopDTMF           = 0x31,
        StopDTMFAcknowledge = 0x32,
        StartDTMFAcknowledge = 0x36,
        StartDTMFReject    = 0x37,
        Hold               = 0x18,
        HoldReject         = 0x1a,
        CCStatus           = 0x3d
    };

    explicit L3CCMessage(unsigned wTI = 7);
    L3PD PD() const override { return L3PD::CallControl; }
    unsigned TI() const override;
    void TI(unsigned wTI);
};
```

### 7.2 L3Setup

**Spec:** GSM 04.08 9.3.19

```cpp
class L3Setup : public L3CCMessage, public L3CCCapabilities, public L3CCCommonIEs {
public:
    explicit L3Setup(unsigned wTI = 7);
    L3Setup(unsigned wTI, const L3CalledPartyBCDNumber& wCalled);

    bool haveCalledParty() const;
    TypeOfNumber typeOfNumber() const;
    NumberingPlan numberingPlan() const;
    const char* digits() const;
    int MTI() const override { return Setup; }
};
```

### 7.3 L3EmergencySetup

**Spec:** GSM 04.08 9.3.8

```cpp
class L3EmergencySetup : public L3Setup {
public:
    explicit L3EmergencySetup(unsigned wTI = 7);
    int MTI() const override { return EmergencySetup; }
    size_t l2BodyLength() const override { return 0; }
};
```

### 7.4 L3CallProceeding

**Spec:** GSM 04.08 9.3.3

```cpp
class L3CallProceeding : public L3CCMessage {
public:
    explicit L3CallProceeding(unsigned wTI = 7);
    bool hasProgress() const;
    const L3ProgressIndicator& progress() const;
    int MTI() const override { return CallProceeding; }
    size_t l2BodyLength() const override;
};
```

### 7.5 L3Alerting

**Spec:** GSM 04.08 9.3.1

```cpp
class L3Alerting : public L3CCMessage, public L3CCCommonIEs {
public:
    explicit L3Alerting(unsigned wTI = 7);
    bool hasProgress() const;
    const L3ProgressIndicator& progress() const;
    int MTI() const override { return Alerting; }
    size_t l2BodyLength() const override;
};
```

### 7.6 L3Connect

**Spec:** GSM 04.08 9.3.5

```cpp
class L3Connect : public L3CCMessage {
public:
    explicit L3Connect(unsigned wTI = 7);
    int MTI() const override { return Connect; }
    size_t l2BodyLength() const override;
};
```

### 7.7 L3ConnectAcknowledge

**Spec:** GSM 04.08 9.3.6

```cpp
class L3ConnectAcknowledge : public L3CCMessage {
public:
    explicit L3ConnectAcknowledge(unsigned wTI = 7);
    int MTI() const override { return ConnectAcknowledge; }
    size_t l2BodyLength() const override { return 0; }
};
```

### 7.8 L3CallConfirmed

**Spec:** GSM 04.08 9.3.2 — TLV format: Cause (IEI=0x08), SupportedCodecList (IEI=0x40)

```cpp
class L3CallConfirmed : public L3CCMessage, public L3CCCapabilities {
public:
    explicit L3CallConfirmed(unsigned wTI = 7);
    bool hasCause() const;
    const L3CauseElement& cause() const;
    bool hasSupportedCodecs() const;
    const L3SupportedCodecList& supportedCodecs() const;
    int MTI() const override { return CallConfirmed; }
    size_t l2BodyLength() const override;
};
```

### 7.9 L3Disconnect

**Spec:** GSM 04.08 9.3.7

```cpp
class L3Disconnect : public L3CCMessage {
public:
    L3Disconnect(unsigned wTI = 7, CCCause cause = CCCause::Normal_Call_Clearing,
                  CCCauseLocation loc = CCCauseLocation::Private_Serving_Local);

    CCCause cause() const;
    CCCauseLocation location() const;
    int MTI() const override { return Disconnect; }
    size_t l2BodyLength() const override { return 2; }
};
```

### 7.10 L3Release

**Spec:** GSM 04.08 9.3.19

```cpp
class L3Release : public L3CCMessage, public L3CCCommonIEs {
public:
    explicit L3Release(unsigned wTI = 7);
    L3Release(unsigned wTI, CCCause cause);

    bool haveCause() const;
    CCCause cause() const;
    int MTI() const override { return Release; }
    size_t l2BodyLength() const override;
};
```

### 7.11 L3ReleaseComplete

**Spec:** GSM 04.08 9.3.19

```cpp
class L3ReleaseComplete : public L3CCMessage, public L3CCCommonIEs {
public:
    explicit L3ReleaseComplete(unsigned wTI = 7);
    L3ReleaseComplete(unsigned wTI, CCCause cause);
    bool haveCause() const;
    CCCause cause() const;
    int MTI() const override { return ReleaseComplete; }
    size_t l2BodyLength() const override;
};
```

### 7.12 L3CCStatus

**Spec:** GSM 04.08 9.3.19

```cpp
class L3CCStatus : public L3CCMessage {
public:
    explicit L3CCStatus(unsigned wTI = 7);
    L3CCStatus(unsigned wTI, CCCause cause, unsigned callState);

    CCCause cause() const;
    unsigned callState() const;
    int MTI() const override { return CCStatus; }
    size_t l2BodyLength() const override { return 4; }
};
```

### 7.13 L3StartDTMF

**Spec:** GSM 04.08 9.3.24 — TV format: KeypadFacility (IEI=0x2c)

```cpp
class L3StartDTMF : public L3CCMessage {
public:
    explicit L3StartDTMF(unsigned wTI = 7);
    char key() const;
    int MTI() const override { return StartDTMF; }
    size_t l2BodyLength() const override { return 1; }
};
```

### 7.14 L3StartDTMFAcknowledge

**Spec:** GSM 04.08 9.3.25 — TV format: KeypadFacility (IEI=0x2c)

```cpp
class L3StartDTMFAcknowledge : public L3CCMessage {
public:
    L3StartDTMFAcknowledge(unsigned wTI, char key);
    int MTI() const override { return StartDTMFAcknowledge; }
    size_t l2BodyLength() const override { return 1; }
};
```

### 7.15 L3StartDTMFReject

**Spec:** GSM 04.08 9.3.26 — LV format: CauseElement

```cpp
class L3StartDTMFReject : public L3CCMessage {
public:
    L3StartDTMFReject(unsigned wTI, CCCause cause);
    CCCause cause() const;
    int MTI() const override { return StartDTMFReject; }
    size_t l2BodyLength() const override { return 3; }
};
```

### 7.16 L3StopDTMF

**Spec:** GSM 04.08 9.3.29

```cpp
class L3StopDTMF : public L3CCMessage {
public:
    explicit L3StopDTMF(unsigned wTI = 7);
    int MTI() const override { return StopDTMF; }
    size_t l2BodyLength() const override { return 0; }
};
```

### 7.17 L3StopDTMFAcknowledge

**Spec:** GSM 04.08 9.3.30

```cpp
class L3StopDTMFAcknowledge : public L3CCMessage {
public:
    explicit L3StopDTMFAcknowledge(unsigned wTI);
    int MTI() const override { return StopDTMFAcknowledge; }
    size_t l2BodyLength() const override { return 0; }
};
```

### 7.18 L3Hold

**Spec:** GSM 04.08 9.3.10

```cpp
class L3Hold : public L3CCMessage {
public:
    explicit L3Hold(unsigned wTI = 7);
    int MTI() const override { return Hold; }
    size_t l2BodyLength() const override { return 0; }
};
```

### 7.19 L3HoldReject

**Spec:** GSM 04.08 9.3.12 — LV format: CauseElement

```cpp
class L3HoldReject : public L3CCMessage {
public:
    L3HoldReject(unsigned wTI, CCCause cause);
    CCCause cause() const;
    int MTI() const override { return HoldReject; }
    size_t l2BodyLength() const override { return 3; }
};
```

### 7.20 L3Progress

**Spec:** GSM 04.08 9.3.17 — LV format: ProgressIndicator (standalone). Note: when embedded inside Alerting/Connect/CallProceeding, the same IE uses TLV (IEI=0x1e).

```cpp
class L3Progress : public L3CCMessage {
public:
    explicit L3Progress(unsigned wTI);
    int MTI() const override { return Progress; }
    size_t l2BodyLength() const override { return 3; }
};
```

---

## 10. Supplementary Services (PD=0x0b)

**File:** `gsml3parser/ss/l3ssmessages.h` | **Spec:** GSM 04.80

### 8.1 L3SupServMessage

Base class for all SS messages.

```cpp
class L3SupServMessage : public L3Message {
public:
    enum MessageType : int {
        ReleaseComplete = 0x2a,
        Facility        = 0x3a,
        Register        = 0x3b
    };

    explicit L3SupServMessage(unsigned wTI = 7);
    L3PD PD() const override { return L3PD::NonCallSS; }
    unsigned TI() const override;
    void setTI(unsigned wTI);
};
```

### 8.2 L3SupServFacilityMessage

**Spec:** GSM 04.80 2.3

```cpp
class L3SupServFacilityMessage : public L3SupServMessage {
public:
    L3SupServFacilityMessage();
    L3SupServFacilityMessage(unsigned wTI, const std::string& facility);

    std::string getMapComponents() const;
    int MTI() const override { return Facility; }
};
```

### 8.3 L3SupServRegisterMessage

**Spec:** GSM 04.80 2.4 — TLV format: Facility (IEI=0x1c), SSVersion (IEI=0x7f, TLV 3 bytes)

```cpp
class L3SupServRegisterMessage : public L3SupServMessage {
public:
    L3SupServRegisterMessage();
    L3SupServRegisterMessage(unsigned wTI, const std::string& facility);

    bool haveVersionIndicator() const;
    uint8_t versionIndicator() const;
    std::string getMapComponents() const;
    int MTI() const override { return Register; }
};
```

Version indicator uses TLV format (3 bytes: IEI + Length + Value).


### 8.4 L3SupServReleaseCompleteMessage

**Spec:** GSM 04.80 2.5 — TLV format: Facility (IEI=0x1c), Cause (IEI=0x08)

```cpp
class L3SupServReleaseCompleteMessage : public L3SupServMessage {
public:
    L3SupServReleaseCompleteMessage();
    explicit L3SupServReleaseCompleteMessage(unsigned wTI);
    L3SupServReleaseCompleteMessage(unsigned wTI, CCCause cause);

    bool haveFacility() const;
    CCCause cause() const;
    CCCauseLocation causeLocation() const;
    int MTI() const override { return ReleaseComplete; }
};
```

---

## 11. Error Handling

### 9.1 Exception Classes

**File:** `gsml3parser/l3message.h`

```cpp
class ParseError : public std::runtime_error {
public:
    explicit ParseError(const std::string& what);
};

class WriteError : public std::runtime_error {
public:
    explicit WriteError(const std::string& what);
};
```

### 9.2 Error Conventions

| Situation | Behavior |
|-----------|----------|
| Message too short for PD+MTI | Returns `nullptr` |
| Unknown MTI for known PD | Logs warning, returns `nullptr` |
| Unknown PD | Returns `nullptr` (or invokes custom handler) |
| Malformed IE during parse | Throws `ParseError` |
| Unimplemented writeV | Throws `WriteError` |
| Buffer too small for `writeL3()` | Returns `0` |

### 9.3 No-Throw Parsers

The top-level `parseL3()` functions never throw. They catch all `ParseError`
exceptions and return `nullptr` instead. Internal `parseBody()` methods may throw.

---

## 12. Logging

**File:** `gsml3parser/logger.h`

### 10.1 Log Levels

```cpp
enum class LogLevel : int {
    EMERG  = 0,
    ALERT  = 1,
    CRIT   = 2,
    ERR    = 3,
    WARNING = 4,
    NOTICE = 5,
    INFO   = 6,
    DEBUG  = 7
};
```

### 10.2 API

```cpp
LogLevel getLogLevel();
void setLogLevel(LogLevel level);
void logMessage(LogLevel level, const char* file, int line, const char* fmt, ...);
```

### 10.3 Macros

```cpp
GSML3PARSER_LOG_EMERG(...)
GSML3PARSER_LOG_ALERT(...)
GSML3PARSER_LOG_CRIT(...)
GSML3PARSER_LOG_ERR(...)
GSML3PARSER_LOG_WARN(...)
GSML3PARSER_LOG_NOTICE(...)
GSML3PARSER_LOG_INFO(...)
GSML3PARSER_LOG_DEBUG(...)
```

### 10.4 Environment Variable

Set `GSML3PARSER_LOG_LEVEL` to control the threshold (0–7). Default: 6 (INFO).

---

## 13. Conformance Notes

### 13.1 H/L Bit Fill Pattern

Per GSM 04.07 7.2, rest octets use an alternating H/L bit pattern based on
position within the octet:

```
Bit position (mod 8):  0  1  2  3  4  5  6  7
Pattern:                0  0  1  0  1  0  1  1
H bit (inverted):       1  1  0  1  0  1  0  0
L bit (pattern):        0  0  1  0  1  0  1  1
```

The library implements this pattern in `L3Frame::writeH()` and `L3Frame::writeL()`.

### 13.2 MTI Bit 6 Masking

Per GSM 04.08 Table 10.2/3/4/5, bit 6 (0x40) of the MTI is "don't care" for
MM, CC, and SS protocols. The library masks this bit when parsing these protocols
and in `L3Frame::MTI()`.

### 13.3 BCD Encoding

BCD digits follow GSM 04.07 11.2.1.1 encoding rules:

- Nibbles are swapped within each byte (even nibble = higher digit)
- Last nibble is `0xF` filler for odd-length numbers
- TMSI uses special 0xF4 header byte

### 13.4 IE Format Compliance

The library strictly follows GSM 04.07 11.2.1.1.4 for IE formats:

| Protocol | Format Used | Notes |
|----------|-------------|-------|
| RR System Information Type 1 | V | Fixed 19-byte body |
| RR System Information Type 2/2bis/2ter | V | Fixed 20-byte body: BCCHFrequencyList + NCCPermitted + RACHControlParameters |
| RR System Information Type 3 | V + RestOctets | Fixed 16-byte body + variable rest octets |
| RR System Information Type 4 | V + RestOctets | Fixed 13-byte body: LAI + CI + CellSelectionParameters + CellOptionsBCCH + RACHControlParameters |
| RR System Information Type 5/5bis/5ter | V | Fixed 16-byte body: BCCHFrequencyList |
| RR System Information Type 6 | V | Fixed 9-byte body: CI + LAI + CellOptionsSACCH + NCCPermitted |
| RR System Information Type 7 | TV | No rest octets |
| RR System Information Type 8 | TV | NCCPermitted field |
| RR System Information Type 9/16 | V | Fixed 5-byte body: CI + CellSelectionParameters + CellOptionsBCCH |
| RR System Information Type 13 | V + RestOctets | Variable rest octets |
| RR System Information Type 17 | TV | No rest octets |
| RR Paging | LV for MobileIdentity | Second identity uses TLV (IEI=0x17) |
| RR Classmark | LV + optional TLV | Classmark2 as LV, Classmark3 as TLV (IEI=0x20) |
| RR Assignment Command | V | Mandatory PowerCommand + optional Mode1/MultiRate |
| RR Channel Release | V | Dynamic: Cause + optional GPRSResumption |
| RR Physical Information | V | L3TimingAdvance object |
| CC messages | TLV/LV switch dispatch | IEI-based parsing, arbitrary order |
| CC DTMF | TV (IEI=0x2c) | KeypadFacility with type prefix |
| CC DTMF Reject | LV | CauseElement, 3-byte body |
| CC Progress | TLV (IEI=0x1e) | ProgressIndicator with type + length |
| CC Cause | LV | CauseElement with length prefix |
| CC Hold Reject | LV | CauseElement, 3-byte body |
| CC CM Service Abort | V | CMServiceType + optional Cause byte |
| SS Register | TLV | Facility (IEI=0x1c), SSVersion (IEI=0x7f, 3-byte TLV) |
| SS ReleaseComplete | TLV | Cause (IEI=0x08), Facility (IEI=0x1c) |

### 13.5 Paging Channel Needed Encoding

Paging Request messages (Type 1/2/3) encode Channel Needed as 2-bit codes
(0=AnyDCCH, 1=SDCCH, 2=TCHF, 3=AnyTCH) in reversed half-octet order,
followed by a 4-bit Page Mode field. L3PageMode handles 4 bits (half-octet), not 8 bits.

---

## 14. Appendix: GSM Specifications

### 14.1 Protocol Discriminators

| PD | Value | Protocol | Status |
|----|-------|----------|--------|
| GroupCallControl | 0x00 | GSM 04.08 | Not implemented |
| BroadcastCallControl | 0x01 | GSM 04.08 | Not implemented |
| **CallControl** | **0x03** | GSM 04.08 9.3 | Implemented |
| **MobilityManagement** | **0x05** | GSM 04.08 9.2 | Implemented |
| **RadioResource** | **0x06** | GSM 04.08 9.1 | Implemented |
| GPRSMobilityManagement | 0x08 | GSM 04.08 9.4 | Not implemented |
| **SMS** | **0x09** | GSM 04.11 | Custom handler only |
| GPRSSessionManagement | 0x0a | GSM 04.08 9.5 | Not implemented |
| **NonCallSS** | **0x0b** | GSM 04.80 | Implemented |
| Location | 0x0c | 3GPP TS 44.071 | Not implemented |
| Extended | 0x0e | Reserved | Not implemented |
| TestProcedure | 0x0f | Reserved | Not implemented |

### 14.2 Cause Code References

| Enum | Spec Section | Description |
|------|-------------|-------------|
| `RRCause` | GSM 04.08 10.5.2.31 | RR cause codes |
| `MMRejectCause` | GSM 04.08 10.5.3.6 | MM reject cause codes |
| `CCCause` | GSM 04.08 10.5.4.11 | CC cause codes |
| `CCCauseLocation` | GSM 04.08 10.5.4.11 | CC cause location |
| `BSSCause` | GSM 48.008 3.2.2.5 | BSS cause codes |

### 14.3 Key GSM Specifications

| Spec | Title |
|------|-------|
| GSM 04.08 | Mobile radio interface signalling |
| GSM 04.07 | Mobile radio interface application part |
| GSM 04.06 | LAPDm — Link Access on the Dm channel |
| GSM 04.11 | SMS over the Um interface |
| GSM 05.02 | Multiplexing and multiple access |
| GSM 05.03 | Coding on the radio path |
| GSM 03.38 | Alphabets and language-specific information |
| GSM 03.03 | Numbering, addressing and identification |
| GSM 04.80 | Supplementary services — call independent |
| ISDN Q.931 | Digital signalling system No. 1 — User-network interface |
