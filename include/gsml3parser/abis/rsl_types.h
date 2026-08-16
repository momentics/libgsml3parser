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

/// A-bis RSL (Radio Signal Link) type definitions and constants.
///
/// Provides enumerations for RSL message discriminators, message types,
/// information element codes, error causes, and channel number encoding.
/// These types are used by RSLParser to decode BSC->BTS messages and by
/// RSLBuilder to construct BTS->BSC messages.
///
/// 3GPP specification: TS 48.058 (A-bis interface), GSM 04.08 (L3 mapping).
/// Thread safety: all types are trivially copyable, safe for concurrent read.
/// Memory: sizeof(RSLChannelMode) == 5 bytes (packed), sizeof(RSLEncryptionInfo) == 16 bytes (span = 2 pointers).
///
/// Example:
/// @code
///   auto chanNr = RSLChannelNumber::encode(0x1c, 3); // SDCCH/8, TS 3
///   auto disc = RSLDiscriminator::DedicatedChannel;
///   auto msgType = static_cast<uint8_t>(RSLDChanMessageType::ChanActiv);
/// @endcode
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace gsml3parser {

/// RSL Message Discriminators (TS 48.058).
/// The discriminator byte determines which sub-protocol the message belongs to:
/// RLL for radio link layer data transport, DCHAN for dedicated channel control,
/// CCHAN for common channel control, TRX for transceiver-level management,
/// or IPAccess for ip.access vendor-specific extensions.
enum class RSLDiscriminator : uint8_t {
    RLL              = 0x00,  ///< Radio Link Layer messages
    CommonChannel    = 0x40,  ///< CCHAN - common channel control
    DedicatedChannel = 0x60,  ///< DCHAN - dedicated channel control
    TRX              = 0xa0,  ///< TRX-level management
    IPAccess         = 0xc0   ///< ip.access vendor-specific
};

/// RSL Message Types for RLL discriminator.
/// Used to transport L3 signalling data between BTS and BSC over established
/// LAPDm radio links. DATA_REQ/DATA_IND carry numbered L3 messages, while
/// UNIT_DATA variants handle unnumbered (connectionless) transfer.
enum class RSLL3MessageType : uint8_t {
    DataReq          = 0x21,  ///< BSC->BTS: L3 data for MS (numbered)
    DataInd          = 0x22,  ///< BTS->BSC: L3 data from MS (numbered)
    UnitDataReq      = 0x41,  ///< BSC->BTS: unnumbered L3 data
    UnitDataInd      = 0x42,  ///< BTS->BSC: unnumbered L3 data
    EstablishmentInd = 0x61,  ///< BTS->BSC: LAPDm link established
    ReleaseReq       = 0x81,  ///< BSC->BTS: release request
    ReleaseInd       = 0xa1   ///< BTS->BSC: release indication
};

/// RSL Message Types for DCHAN discriminator.
/// Controls dedicated channel lifecycle: activation, power control,
/// encryption setup, measurement reporting, and handover detection.
enum class RSLDChanMessageType : uint8_t {
    ChanActiv        = 0x01,  ///< BSC->BTS: channel activation command
    RFChanRel        = 0x02,  ///< BSC->BTS: RF channel release command
    SACCHInfoModify  = 0x03,  ///< BSC->BTS: SACCH info modify
    DeactivateSACCH  = 0x04,  ///< BSC->BTS: deactivate SACCH
    EncrCmd          = 0x06,  ///< BSC->BTS: encryption command
    ModeModifyReq    = 0x07,  ///< BSC->BTS: mode modify request
    MS_PowerControl  = 0x09,  ///< BSC->BTS: MS power control
    BS_PowerControl  = 0x0a,  ///< BSC->BTS: BS power control
    ChanActivAck     = 0x11,  ///< BTS->BSC: channel activation ACK
    ChanActivNack    = 0x12,  ///< BTS->BSC: channel activation NACK
    RFChanRelAck     = 0x15,  ///< BTS->BSC: RF channel release ACK
    ConnFail         = 0x21,  ///< BTS->BSC: connection failure report
    MeasRes          = 0x24,  ///< BTS->BSC: measurement result report
    HandoDet         = 0x26   ///< BTS->BSC: handover detection
};

/// RSL Message Types for CCHAN discriminator.
/// Manages common channels (BCCH, CCCH): system information broadcasting,
/// paging, SMS broadcast, immediate assignment, and load reporting.
enum class RSLCChanMessageType : uint8_t {
    BCCHInfo         = 0x01,  ///< BSC->BTS: BCCH system information
    ImmediateAssignCmd = 0x02, ///< BSC->BTS: immediate assignment command
    PagingCmd        = 0x03,  ///< BSC->BTS: paging command
    SMSBCCmd         = 0x04,  ///< BSC->BTS: SMS broadcast command
    CCCHLoadInd      = 0x13,  ///< BTS->BSC: CCCH load indication
    DeleteInd        = 0x14,  ///< BTS->BSC: delete indication
    ChanRqd          = 0x16   ///< BTS->BSC: channel required request
};

/// RSL Information Element type codes (TS 48.058).
/// Each IE is encoded as TLV (Type-Length-Value) or TV (Type-Value, fixed 1-byte value).
/// Used to carry channel parameters, encryption keys, measurement data, and L3 payloads
/// within RSL messages.
enum class RSL_IE : uint8_t {
    ChanNr           = 0x11,  ///< Channel Number (TV, 1 byte)
    LinkIdent        = 0x12,  ///< Link Identifier (TV, 1 byte)
    ActType          = 0x21,  ///< Activation Type (TV, 1 byte)
    ChanMode         = 0x22,  ///< Channel Mode (TLV, 6 bytes)
    EncrInfo         = 0x23,  ///< Encryption Info (TLV, 1..129 bytes)
    BSPower          = 0x24,  ///< BS Power (TV, 1 byte)
    MSPower          = 0x25,  ///< MS Power (TV, 1 byte)
    HandoRef         = 0x26,  ///< Handover Reference (TV, 1 byte)
    SACCHInfo        = 0x27,  ///< SACCH Information (TLV, variable)
    Cause            = 0x28,  ///< Cause (TV, 1 byte)
    AccessDelay      = 0x29,  ///< Access Delay (TV, 1 byte)
    ReqReference     = 0x2a,  ///< Request Reference (TLV, 3 bytes)
    FrameNumber      = 0x2b,  ///< Frame Number (TLV, 2 bytes)
    MSIdentity       = 0x2c,  ///< MS Identity (TLV, variable)
    PagingGroup      = 0x2d,  ///< Paging Group (TV, 1 byte)
    ChanNeeded       = 0x2e,  ///< Channel Needed (TV, 1 byte)
    FullImmAssInfo   = 0x2f,  ///< Full Immediate Assignment Info (TLV, variable)
    L3Info           = 0x30,  ///< L3 Information (TL16V, variable, 16-bit length)
    SysInfoType      = 0x31,  ///< System Information Type (TV, 1 byte)
    FullBCCHInfo     = 0x32,  ///< Full BCCH Info (TLV, variable)
    MeasResNr        = 0x33,  ///< Measurement Result Number (TV, 1 byte)
    UplinkMeas       = 0x34,  ///< Uplink Measurements (TLV, 3..7 bytes)
    L1Info           = 0x35,  ///< L1 Information (TLV, variable)
    TimingAdvance    = 0x36,  ///< Timing Advance (TV, 1 byte)
    MSTimingOffset   = 0x37,  ///< MS Timing Offset (TV, 1 byte)
    ReleaseMode      = 0x38   ///< Release Mode (TV, 1 byte)
};

/// RSL Error causes for NACK and failure messages.
enum class RSLErrorCause : uint8_t {
    NormalUnspecified       = 0x01,
    RRUnavailable           = 0x02,
    EquipmentFailure        = 0x03,
    ServiceOptionUnavailable = 0x04,
    ServiceOptionUnimplemented = 0x05,
    ResourceUnavailable     = 0x06,
    IEContentError          = 0x07,
    MandatoryIEMissing      = 0x08,
    OptionalIEError         = 0x09,
    MessageSeqError         = 0x0a,
    MessageTypeError        = 0x0b,
    DiscriminatorError      = 0x0c,
    ProtocolError           = 0x0d,
    EncryptionUnimplemented = 0x0e
};

/// RSL Channel Number encoding (TS 48.058).
/// Common channels use fixed values (BCCH=0x00, RACH=0x40, PCH/AGCH=0x60).
/// Dedicated channels encode type bits (upper 5) and timeslot (lower 3).
struct RSLChannelNumber {
    static constexpr uint8_t BCCH       = 0x00;
    static constexpr uint8_t RACH       = 0x40;
    static constexpr uint8_t PCH_AGCH   = 0x60;

    /// Encode dedicated channel number from type bits and timeslot.
    /// @param cbits Channel type code (0-31, shifted to upper 5 bits)
    /// @param ts Timeslot number (lower 3 bits, range 0-7)
    /// @return Encoded channel number byte
    [[nodiscard]] static uint8_t encode(uint8_t cbits, uint8_t ts) noexcept {
        return ((cbits << 3) & 0xf8) | (ts & 0x07);
    }

    /// Extract channel type bits from encoded channel number.
    /// @param chanNr Encoded channel number byte
    /// @return Upper 5 bits (channel type identifier, 0-31)
    [[nodiscard]] static uint8_t getCBits(uint8_t chanNr) noexcept {
        return chanNr >> 3;
    }

    /// Extract timeslot from encoded channel number.
    /// @param chanNr Encoded channel number byte
    /// @return Lower 3 bits (timeslot 0-7)
    [[nodiscard]] static uint8_t getTimeslot(uint8_t chanNr) noexcept {
        return chanNr & 0x07;
    }

    /// Check if channel number represents a dedicated channel.
    /// Common channels have fixed values: BCCH=0x00, RACH=0x40, PCH/AGCH=0x60.
    /// @param chanNr Encoded channel number byte
    /// @return true if this is a dedicated physical channel
    [[nodiscard]] static bool isDedicated(uint8_t chanNr) noexcept {
        return chanNr != BCCH && chanNr != RACH && chanNr != PCH_AGCH;
    }
};

/// Activation types for CHAN_ACTIV messages.
enum class RSLActivationType : uint8_t {
    IntraImmediateAssignment = 0x01,
    IntraSDCCH4              = 0x02,
    IntraSDCCH8              = 0x03,
    InterAsyncHandover       = 0x04,
    InterSyncHandover        = 0x05
};

/// Channel Mode structure (5 octets per GSM 04.08 10.5.2.6).
/// Describes the physical channel characteristics: signalling/speech/data indicator,
/// channel rate and type, DTX settings, and coding algorithm.
struct RSLChannelMode {
    uint8_t reserved{0};
    uint8_t spdInd{0};     ///< Speed Indicator: 1=Signalling, 2=Speech, 3=Data
    uint8_t chanRT{0};     ///< Channel Rate and Type
    uint8_t dtxDTU{0};     ///< DTX/DTU settings
    uint8_t chanRate{0};   ///< Speech coding algorithm / data rate

    enum SpeedIndicator : uint8_t { Signalling = 0x01, Speech = 0x02, Data = 0x03 };
    enum ChanRateType : uint8_t { SDCCH = 0x01, TCH_Bm = 0x02, TCH_Lm = 0x03 };

    [[nodiscard]] bool isSignalling() const noexcept { return spdInd == static_cast<uint8_t>(SpeedIndicator::Signalling); }
    [[nodiscard]] bool isSpeech() const noexcept { return spdInd == static_cast<uint8_t>(SpeedIndicator::Speech); }
    [[nodiscard]] bool isData() const noexcept { return spdInd == static_cast<uint8_t>(SpeedIndicator::Data); }
};
static_assert(sizeof(RSLChannelMode) == 5, "RSLChannelMode must be exactly 5 bytes");

/// Encryption information carried in ENCR_CMD or CHAN_ACTIV.
/// Specifies the ciphering algorithm (A5/0, A5/1, etc.) and provides a view
/// into the ciphering key Kc buffer (typically 8 bytes for A5/1).
struct RSLEncryptionInfo {
    uint8_t algorithmId{0};  ///< 0=A5/0, 1=A5/1, 2=A5/2, 3=A5/3
    std::span<const uint8_t> key;  ///< Ciphering key Kc (8 bytes for A5/1)
};

/// Return human-readable name for the RSL discriminator.
/// @param disc The discriminator value.
/// @return Non-empty string identifier for logging.
[[nodiscard]] std::string_view rslDiscriminatorName(RSLDiscriminator disc);

/// Return human-readable name for the RSL IE type.
/// @param ie The information element type code.
/// @return Non-empty string identifier for logging.
[[nodiscard]] std::string_view rslIEName(RSL_IE ie);

/// Return human-readable name for the RSL error cause.
/// @param cause The error cause value.
/// @return Non-empty string identifier for logging.
[[nodiscard]] std::string_view rslErrorCauseName(RSLErrorCause cause);

} // namespace gsml3parser
