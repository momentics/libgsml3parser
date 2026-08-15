// Copyright 2026 momentics <momentics@gmail.com>
// Copyright libgsml3parser contributors
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include <cstdint>
#include <iosfwd>

namespace gsml3parser {

// Log severity levels
enum class LogLevel : uint8_t {
    EMERG   = 0,
    ALERT   = 1,
    CRIT    = 2,
    ERR     = 3,
    WARNING = 4,
    NOTICE  = 5,
    INFO    = 6,
    DEBUG   = 7
};

std::ostream& operator<<(std::ostream& os, LogLevel level);

// L3 Protocol Discriminator - GSM 04.08 10.2, GSM 04.07 11.2.3.1.1
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

std::ostream& operator<<(std::ostream& os, L3PD pd);

// Interlayer primitives - GSM 04.04, GSM 04.06, GSM 04.07
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

std::ostream& operator<<(std::ostream& os, Primitive prim);

// SAPI - Service Access Point Indicator, GSM 04.06
enum class SAPI : uint8_t {
    SAPI0 = 0,
    SAPI3 = 3,
    SAPI0_Sacch = 4,
    SAPI3_Sacch = 7,
    Undefined = 16
};

std::ostream& operator<<(std::ostream& os, SAPI sapi);

// Mobile identity types - GSM 04.08 10.5.1.4
enum class MobileIDType : uint8_t {
    NoID   = 0,
    IMSI   = 1,
    IMEI   = 2,
    IMEISV = 3,
    TMSI   = 4
};

std::ostream& operator<<(std::ostream& os, MobileIDType type);

// Type of Number - GSM 04.08 Table 10.5.118
enum class TypeOfNumber : uint8_t {
    Unknown         = 0,
    International   = 1,
    National        = 2,
    NetworkSpecific = 3,
    ShortCode       = 4,
    Alphanumeric    = 5,
    Abbreviated     = 6
};

std::ostream& operator<<(std::ostream& os, TypeOfNumber ton);

// Numbering Plan - GSM 04.08 Table 10.5.118
enum class NumberingPlan : uint8_t {
    Unknown  = 0,
    E164     = 1,
    X121     = 3,
    F69      = 4,
    National = 8,
    Private  = 9,
    ERMES    = 10
};

std::ostream& operator<<(std::ostream& os, NumberingPlan np);

// GSM band types - GSM 05.05 2
enum class GSMBand : uint16_t {
    GSM850  = 850,
    EGSM900 = 900,
    DCS1800 = 1800,
    PCS1900 = 1900
};

// GSM logical channel types
enum class ChannelType : uint8_t {
    SCHType,
    FCCHType,
    BCCHType,
    CCCHType,
    RACHType,
    SACCHType,
    CBCHType,
    SDCCHType,
    FACCHType,
    TCHFType,
    TCHHType,
    AnyTCHType,
    PDTCHCS1Type,
    PDTCHCS2Type,
    PDTCHCS3Type,
    PDTCHCS4Type,
    LoopbackFullType,
    LoopbackHalfType,
    AnyDCCHType,
    UndefinedCHType
};

// Ensure ChannelType values fit in a fixed-size array for O(1) indexing.
static_assert(static_cast<uint8_t>(ChannelType::UndefinedCHType) < 32,
              "ChannelType values must fit in 32-element array");
constexpr int kMaxChannelTypes = 32;

std::ostream& operator<<(std::ostream& os, ChannelType ch);

// GSM 7-bit alphabet - GSM 03.38 6.2.1
enum class GSMAlphabet : uint8_t {
    ALPHABET_7BIT,
    ALPHABET_8BIT,
    ALPHABET_UCS2
};

std::ostream& operator<<(std::ostream& os, GSMAlphabet alphabet);

// Type And Offset for L3ChannelDescription - GSM 04.08 10.5.2.5
// Encodes channel type (3 bits) and TDMA offset (2 bits).
enum TypeAndOffset : uint8_t {
    TDMA_SACCH  = 0,
    TDMA_SDCCH  = 1,
    TDMA_TCHF   = 2,
    TDMA_TCHH   = 3,
    TDMA_CBCH   = 4,
    TDMA_PDTCH  = 5,
    TDMA_PACCCH = 6,
    TDMA_PAGCH  = 7,
    TDMA_PCCCH  = 8,
    TDMA_PNCH   = 9,
    TDMA_PRACH  = 10,
    TDMA_PTCCH  = 11,
    TDMA_PDCH   = 12,
    TDMA_PACCH  = 13,
    TDMA_MISC   = 15
};

} // namespace gsml3parser
