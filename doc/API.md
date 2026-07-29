# libgsml3parser — Full API Reference

> Version 0.1.0 | C++17 | Thread-safe (read-only) | No external dependencies

---

## Table of Contents

1. [Getting Started](#1-getting-started)
2. [Core API](#2-core-api)
3. [Data Types](#3-data-types)
4. [Information Elements](#4-information-elements)
5. [Radio Resource Messages (PD=0x06)](#5-radio-resource-messages-pd0x06)
6. [Mobility Management Messages (PD=0x05)](#6-mobility-management-messages-pd0x05)
7. [Call Control Messages (PD=0x03)](#7-call-control-messages-pd0x03)
8. [Supplementary Services (PD=0x0b)](#8-supplementary-services-pd0x0b)
9. [Error Handling](#9-error-handling)
10. [Logging](#10-logging)
11. [Appendix: GSM Specifications](#11-appendix-gsm-specifications)

---

## 1. Getting Started

### 1.1 Build Requirements

| Requirement | Minimum Version |
|-------------|-----------------|
| C++ Compiler | GCC 7+, Clang 5+, MSVC 2017+ |
| CMake | 3.16+ |
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
std::unique_ptr<L3Message> parseL3(const uint8_t* data, size_t len);
```

| Parameter | Description |
|-----------|-------------|
| `data` | Pointer to raw L3 message bytes (PD + MTI + body). |
| `len` | Number of bytes in `data`. |

**Returns:** `std::unique_ptr<L3Message>` — the parsed message, or `nullptr` on failure.

**Example:**

```cpp
uint8_t data[] = { 0x06, 0x27, /* PagingResponse body */ };
auto msg = gsml3parser::parseL3(data, sizeof(data));

if (msg) {
    std::cout << msg->text() << std::endl;
}
```

### 2.2 Parse from L3Frame

```cpp
std::unique_ptr<L3Message> parseL3(const L3Frame& frame);
```

### 2.3 Parse from Hex String

```cpp
std::unique_ptr<L3Message> parseL3Hex(const std::string& hex);
```

Accepts a hex-encoded string (e.g. `"0627..."`). Whitespace is ignored.

### 2.4 Serialize Message to Bytes

```cpp
size_t writeL3(const L3Message& msg, uint8_t* out, size_t maxlen);
```

**Returns:** Number of bytes written, or `0` if `out` is too small.

### 2.5 Serialize Message to Hex

```cpp
std::string writeL3Hex(const L3Message& msg);
```

### 2.6 Register Custom PD Handler

For Protocol Discriminators not handled by the library (SMS, GPRS), register a callback:

```cpp
using PDHandler = std::function<std::unique_ptr<L3Message>(const L3Frame&)>;
void registerPDHandler(L3PD pd, PDHandler handler);
void unregisterPDHandler(L3PD pd);
```

**Example:**

```cpp
gsml3parser::registerPDHandler(gsml3parser::L3PD::SMS,
    [](const gsml3parser::L3Frame& frame) {
        return parseMySMS(frame);
    });
```

### 2.7 Downcast Helpers

Each PD has its own parser and factory:

```cpp
// RR
std::unique_ptr<L3RRMessage> parseL3RR(const L3Frame& source);
L3RRMessage* L3RRFactory(int mti);

// MM
std::unique_ptr<L3MMMessage> parseL3MM(const L3Frame& source);
L3MMMessage* L3MMFactory(int mti);

// CC
std::unique_ptr<L3CCMessage> parseL3CC(const L3Frame& source);
L3CCMessage* L3CCFactory(int mti);

// SS
std::unique_ptr<L3SupServMessage> parseL3SupServ(const L3Frame& source);
L3SupServMessage* L3SupServFactory(int mti);
```

### 2.8 MTI to String

```cpp
std::string mti2string(L3PD pd, unsigned mti);
```

Converts a PD + MTI pair to a human-readable name.

---

## 3. Data Types

### 3.1 BitVector

**File:** `gsml3parser/bitvector.h`

A resizable bit vector with MSB-first bit ordering within each octet.

```cpp
class BitVector {
public:
    BitVector();
    explicit BitVector(size_t nbits);
    BitVector(size_t nbits, unsigned char fill);
    BitVector(const BitVector& other);
    BitVector(BitVector&& other) noexcept;
    BitVector(const std::vector<uint8_t>& bytes);
    ~BitVector();

    BitVector& operator=(const BitVector& other);
    BitVector& operator=(BitVector&& other) noexcept;

    size_t size() const;
    bool empty() const;
    void resize(size_t nbits);
    void clear();

    unsigned readField(size_t& rp, unsigned nbits) const;
    void writeField(size_t& wp, unsigned value, unsigned nbits) const;
    unsigned peekField(size_t rp, unsigned nbits) const;

    bool readBit(size_t& rp) const;
    void writeBit(size_t& wp, bool bit) const;

    const uint8_t* data() const;
    uint8_t*       data();

    BitVector segment(size_t offset, size_t nbits) const;
    BitVector clone() const;

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

**Bit ordering:** Within each octet, bit 7 (MSB) is read first.

### 3.2 L3Frame

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

### 3.3 L3Message

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

### 3.4 L3ProtocolElement

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

### 3.5 L3OctetAlignedProtocolElement

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

### 3.6 GenericMessageElement

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

### 3.7 Utility Functions

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

### 3.8 Scalar Types

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

### 3.9 Protocol Types

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

## 4. Information Elements

### 4.1 L3CellIdentity

**File:** `gsml3parser/common/l3common.h` | **Spec:** GSM 04.08 10.5.1.1

```cpp
class L3CellIdentity : public L3ProtocolElement {
public:
    explicit L3CellIdentity(unsigned wID = 0);
    unsigned ID() const;
    size_t lengthV() const override { return 2; }
};
```

### 4.2 L3LocationAreaIdentity

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

### 4.3 L3MobileIdentity

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

### 4.4 L3MobileStationClassmark1

**File:** `gsml3parser/common/l3common.h` | **Spec:** GSM 04.08 10.5.1.5

```cpp
class L3MobileStationClassmark1 : public L3ProtocolElement {
public:
    size_t lengthV() const override { return 1; }
};
```

### 4.5 L3MobileStationClassmark2

**File:** `gsml3parser/common/l3common.h` | **Spec:** GSM 04.08 10.5.1.5

```cpp
class L3MobileStationClassmark2 : public L3ProtocolElement {
public:
    size_t lengthV() const override { return 3; }
    int getA5Bits() const;
    int powerClass() const;
};
```

### 4.6 L3MobileStationClassmark3

**File:** `gsml3parser/common/l3common.h` | **Spec:** GSM 04.08 10.5.1.7

```cpp
class L3MobileStationClassmark3 : public L3ProtocolElement {
public:
    L3MobileStationClassmark3();
    size_t lengthV() const override { return 14; }
};
```

### 4.7 L3CipheringKeySequenceNumber

**File:** `gsml3parser/common/l3common.h`

```cpp
class L3CipheringKeySequenceNumber : public L3ProtocolElement {
public:
    explicit L3CipheringKeySequenceNumber(unsigned wCIValue = 0);
    size_t lengthV() const override { return 0; }
};
```

---

## 5. Radio Resource Messages (PD=0x06)

**File:** `gsml3parser/rr/l3rrmessages.h` | **Spec:** GSM 04.08 9.1

### 5.1 L3RRMessage

Base class for all RR messages.

```cpp
class L3RRMessage : public L3Message {
public:
    enum MessageType : int {
        SystemInformationType1  = 0x19,
        SystemInformationType2  = 0x1a,
        SystemInformationType3  = 0x1b,
        SystemInformationType4  = 0x1c,
        SystemInformationType5  = 0x1d,
        SystemInformationType6  = 0x1e,
        SystemInformationType13 = 0x00,
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

### 5.2 L3RRMessageNRO / L3RRMessageRO

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

### 5.3 L3PagingRequestType1

**Spec:** GSM 04.08 9.1.22

```cpp
class L3PagingRequestType1 : public L3RRMessageNRO {
public:
    L3PagingRequestType1();
    L3PagingRequestType1(const L3MobileIdentity& wId, ChannelType wType);
    int MTI() const override { return PagingRequestType1; }
};
```

### 5.4 L3PagingResponse

**Spec:** GSM 04.08 9.1.25

```cpp
class L3PagingResponse : public L3RRMessageNRO {
public:
    const L3MobileIdentity& mobileID() const;
    int MTI() const override { return PagingResponse; }
};
```

### 5.5 L3ChannelRelease

**Spec:** GSM 04.08 9.1.7

```cpp
class L3ChannelRelease : public L3RRMessageNRO {
public:
    explicit L3ChannelRelease(RRCause cause = RRCause::Normal_Event);
    int MTI() const override { return ChannelRelease; }
};
```

### 5.6 L3RRStatus

**Spec:** GSM 04.08 9.1.29

```cpp
class L3RRStatus : public L3RRMessageNRO {
public:
    RRCause cause() const;
    int MTI() const override { return RRStatus; }
};
```

### 5.7 L3AssignmentCommand

**Spec:** GSM 04.08 9.1.2

```cpp
class L3AssignmentCommand : public L3RRMessageNRO {
public:
    int MTI() const override { return AssignmentCommand; }
};
```

### 5.8 L3AssignmentComplete

**Spec:** GSM 04.08 9.1.3

```cpp
class L3AssignmentComplete : public L3RRMessageNRO {
public:
    RRCause cause() const;
    int MTI() const override { return AssignmentComplete; }
};
```

### 5.9 L3AssignmentFailure

**Spec:** GSM 04.08 9.1.3

```cpp
class L3AssignmentFailure : public L3RRMessageNRO {
public:
    RRCause cause() const;
    int MTI() const override { return AssignmentFailure; }
};
```

### 5.10 L3ClassmarkEnquiry

**Spec:** GSM 04.08 9.1.14

```cpp
class L3ClassmarkEnquiry : public L3RRMessageNRO {
public:
    int MTI() const override { return ClassmarkEnquiry; }
    size_t l2BodyLength() const override { return 0; }
};
```

### 5.11 L3ClassmarkChange

**Spec:** GSM 04.08 9.1.11

```cpp
class L3ClassmarkChange : public L3RRMessageNRO {
public:
    const L3MobileStationClassmark2& classmark() const;
    int MTI() const override { return ClassmarkChange; }
};
```

### 5.12 L3MeasurementReport

**Spec:** GSM 04.08 9.1.21

```cpp
class L3MeasurementReport : public L3RRMessageNRO {
public:
    int MTI() const override { return MeasurementReport; }
    size_t l2BodyLength() const override { return 16; }
};
```

### 5.13 L3CipheringModeCommand

**Spec:** GSM 04.08 9.1.9

```cpp
class L3CipheringModeCommand : public L3RRMessageNRO {
public:
    L3CipheringModeCommand(bool ciphering, int algorithm);
    int MTI() const override;
};
```

### 5.14 L3CipheringModeComplete

**Spec:** GSM 04.08 9.1.10

```cpp
class L3CipheringModeComplete : public L3RRMessageNRO {
public:
    int MTI() const override;
    size_t l2BodyLength() const override { return 0; }
};
```

### 5.15 L3HandoverCommand

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

### 5.16 L3HandoverComplete

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

**Spec:** GSM 04.08 9.1.19 — V-format: PageMode + DedicatedModeOrTBF + RequestReference + ChannelDescription + TimingAdvance + MobileAllocation

```cpp
class L3ImmediateAssignment : public L3RRMessageNRO {
public:
    L3ImmediateAssignment();
    const L3ChannelDescription& channelDescription() const;
    const L3RequestReference& requestReference() const;
    const L3TimingAdvance& timingAdvance() const;
    bool hasStartTime() const;
    uint32_t startTimeFrame() const;
    int MTI() const override { return ImmediateAssignment; }
};
```

### 5.22 L3ImmediateAssignmentReject

**Spec:** GSM 04.08 9.1.20 — V-format: PageMode + RequestReference(s) + WaitIndication

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

### 5.23 L3SystemInformationType1

**Spec:** GSM 04.08 9.1.31

```cpp
class L3SystemInformationType1 : public L3RRMessageNRO {
public:
    int MTI() const override { return SystemInformationType1; }
    size_t l2BodyLength() const override { return 19; }
};
```

### 5.22 L3SystemInformationType3

**Spec:** GSM 04.08 9.1.35

```cpp
class L3SystemInformationType3 : public L3RRMessageRO {
public:
    int MTI() const override { return SystemInformationType3; }
    size_t l2BodyLength() const override { return 16; }
    size_t restOctetsLength() const override;
};
```

### 5.23 L3SystemInformationType13

**Spec:** GSM 04.08 9.1.43a

```cpp
class L3SystemInformationType13 : public L3RRMessageRO {
public:
    int MTI() const override { return SystemInformationType13; }
    size_t l2BodyLength() const override { return 0; }
    size_t restOctetsLength() const override;
};
```

---

## 6. Mobility Management Messages (PD=0x05)

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

**Spec:** GSM 04.08 9.2.15

```cpp
class L3LocationUpdatingRequest : public L3MMMessage {
public:
    const L3MobileIdentity& mobileID() const;
    const L3LocationAreaIdentity& LAI() const;
    LocationUpdateType getLocationUpdatingType() const;
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

**Spec:** GSM 04.08 9.2.7

```cpp
class L3CMServiceAbort : public L3MMMessage {
public:
    int MTI() const override { return CMServiceAbort; }
    size_t l2BodyLength() const override { return 0; }
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

## 7. Call Control Messages (PD=0x03)

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

**Spec:** GSM 04.08 9.3.2

```cpp
class L3CallConfirmed : public L3CCMessage, public L3CCCapabilities {
public:
    explicit L3CallConfirmed(unsigned wTI = 7);
    bool hasCause() const;
    const L3CauseElement& cause() const;
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
    int MTI() const override { return StartDTMFReject; }
    size_t l2BodyLength() const override { return 2; }
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
    int MTI() const override { return HoldReject; }
    size_t l2BodyLength() const override { return 2; }
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

## 8. Supplementary Services (PD=0x0b)

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

**Spec:** GSM 04.80 2.4 — TLV format: Facility (IEI=0x1c), SSVersion (IEI=0x7f)

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

### 8.4 L3SupServReleaseCompleteMessage

**Spec:** GSM 04.80 2.5 — TLV format: Cause (IEI=0x08), Facility (IEI=0x1c)

```cpp
class L3SupServReleaseCompleteMessage : public L3SupServMessage {
public:
    L3SupServReleaseCompleteMessage();
    explicit L3SupServReleaseCompleteMessage(unsigned wTI);
    L3SupServReleaseCompleteMessage(unsigned wTI, CCCause cause);

    bool haveFacility() const;
    int MTI() const override { return ReleaseComplete; }
};
```

---

## 9. Error Handling

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

## 10. Logging

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

## 11. Conformance Notes

### 12.1 H/L Bit Fill Pattern

Per GSM 04.07 7.2, rest octets use an alternating H/L bit pattern based on
position within the octet:

```
Bit position (mod 8):  0  1  2  3  4  5  6  7
Pattern:                0  0  1  0  1  0  1  1
H bit (inverted):       1  1  0  1  0  1  0  0
L bit (pattern):        0  0  1  0  1  0  1  1
```

The library implements this pattern in `L3Frame::writeH()` and `L3Frame::writeL()`.

### 12.2 MTI Bit 6 Masking

Per GSM 04.08 Table 10.2/3/4/5, bit 6 (0x40) of the MTI is "don't care" for
MM, CC, and SS protocols. The library masks this bit when parsing these protocols
and in `L3Frame::MTI()`.

### 12.3 BCD Encoding

BCD digits follow GSM 04.07 11.2.1.1 encoding rules:
- Nibbles are swapped within each byte (even nibble = higher digit)
- Last nibble is `0xF` filler for odd-length numbers
- TMSI uses special 0xF4 header byte

### 11.4 IE Format Compliance

The library strictly follows GSM 04.07 11.2.1.1.4 for IE formats:

| Protocol | Format Used | Notes |
|----------|-------------|-------|
| RR messages | V (sequential) | Fields parsed in fixed order, no IEI prefixes |
| RR Paging | LV for MobileIdentity | Second identity uses TLV (IEI=0x17) |
| RR Classmark | LV + optional TLV | Classmark2 as LV, Classmark3 as TLV (IEI=0x20) |
| CC messages | TLV/LV switch dispatch | IEI-based parsing, arbitrary order |
| CC DTMF | TV (IEI=0x2c) | KeypadFacility with type prefix |
| CC Progress | TLV (IEI=0x1e) | ProgressIndicator with type + length |
| CC Cause | LV | CauseElement with length prefix |
| SS Register | TLV | Facility (IEI=0x1c), SSVersion (IEI=0x7f) |
| SS ReleaseComplete | TLV | Cause (IEI=0x08), Facility (IEI=0x1c) |

### 11.5 Paging Channel Needed Encoding

Paging Request messages (Type 1/2/3) encode Channel Needed as 2-bit codes
(0=AnyDCCH, 1=SDCCH, 2=TCHF, 3=AnyTCH) in reversed half-octet order,
followed by a 4-bit Page Mode field.

---

## 11. Appendix: GSM Specifications

### 11.1 Protocol Discriminators

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

### 11.2 Cause Code References

| Enum | Spec Section | Description |
|------|-------------|-------------|
| `RRCause` | GSM 04.08 10.5.2.31 | RR cause codes |
| `MMRejectCause` | GSM 04.08 10.5.3.6 | MM reject cause codes |
| `CCCause` | GSM 04.08 10.5.4.11 | CC cause codes |
| `CCCauseLocation` | GSM 04.08 10.5.4.11 | CC cause location |
| `BSSCause` | GSM 48.008 3.2.2.5 | BSS cause codes |

### 11.3 Key GSM Specifications

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
