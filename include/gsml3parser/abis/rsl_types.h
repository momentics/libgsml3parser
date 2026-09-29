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
/// Provides enumerations for RSL message groups, global message types,
/// information element codes, IE encoding classes, error causes, and the
/// channel number value coding. These types are used by RSLParser to decode
/// frames from the BSC and by RSLBuilder to construct frames for the BSC.
///
/// Frame layout (TS 48.058): octet 1 = message group (7 bits) + transparent
/// indication flag (bit 0); octet 2 = global message type; octets 3..n = a
/// list of information elements (TV, TLV or TL16V encoded).
///
/// 3GPP specification: TS 48.058 (A-bis interface), GSM 04.08 (L3 mapping).
/// Thread safety: all types are trivially copyable, safe for concurrent read.
/// Memory: sizeof(RSLChannelMode) == 4 bytes (packed), sizeof(RSLEncryptionInfo) == 16 bytes (span = 2 pointers).
///
/// Example:
/// @code
///   auto first = rslFirstOctet(RSLDiscriminator::Rll, true); // 0x03: RLL group, transparent
///   auto chanNr = RSLChannelNumber::encode(RSLChannelNumber::Sdcch8, 3); // SDCCH/8 sub-channel 0, TS 3 -> 0x43
/// @endcode
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace gsml3parser {

/// RSL message groups (TS 48.058 9.1). The first octet of an RSL frame
/// carries the 7-bit message group and, in its low bit, the transparent
/// indication flag.
enum class RSLDiscriminator : uint8_t {
    Rll              = 0x01,
    CommonChannel    = 0x06,   // CCHAN
    DedicatedChannel = 0x04,   // DCHAN
    TrxManagement    = 0x08,   // TRX
    Lcs              = 0x10,
    IPAccess         = 0x3F
};

/// Encode the first octet of an RSL frame: the 7-bit message group shifted
/// one bit up plus the transparent indication flag in bit 0 (TS 48.058 9.1).
[[nodiscard]] constexpr uint8_t rslFirstOctet(RSLDiscriminator d, bool transparent) noexcept {
    return static_cast<uint8_t>((static_cast<uint8_t>(d) & 0x7Fu) << 1 | (transparent ? 1u : 0u));
}

/// Decoded first octet of an RSL frame: the raw 7-bit message group and the
/// transparent indication flag.
struct RSLFirstOctet {
    uint8_t group{0};      ///< 7-bit message group code (TS 48.058 9.1)
    bool transparent{false}; ///< Transparent indication flag (bit 0 of the first octet)
};

/// Decode the first octet of an RSL frame (TS 48.058 9.1). Fails when the
/// 7-bit message group is zero (reserved) or exceeds 0x3F.
[[nodiscard]] constexpr std::optional<RSLFirstOctet> decodeRslFirstOctet(uint8_t octet) noexcept {
    const uint8_t group = static_cast<uint8_t>(octet >> 1);
    if (group == 0 || group > 0x3Fu) return std::nullopt;
    return RSLFirstOctet{group, (octet & 0x01u) != 0};
}

/// Map a decoded 7-bit message group to the discriminator enum. Returns
/// nullopt when the group is not defined by TS 48.058 9.1.
[[nodiscard]] constexpr std::optional<RSLDiscriminator> rslGroupToDiscriminator(uint8_t group) noexcept {
    switch (group) {
        case 0x01: return RSLDiscriminator::Rll;
        case 0x04: return RSLDiscriminator::DedicatedChannel;
        case 0x06: return RSLDiscriminator::CommonChannel;
        case 0x08: return RSLDiscriminator::TrxManagement;
        case 0x10: return RSLDiscriminator::Lcs;
        case 0x3F: return RSLDiscriminator::IPAccess;
    }
    return std::nullopt;
}

/// RSL message types of the RLL group (TS 48.058 8.3). The second octet of
/// every RSL frame is a single global message type shared by all groups.
enum class RSLL3MessageType : uint8_t {
    DataReq          = 0x01,  ///< DATA_REQ: L3 data for the MS (numbered)
    DataInd          = 0x02,  ///< DATA_IND: L3 data from the MS (numbered)
    ErrorInd         = 0x03,  ///< ERROR_IND: LAPDm link layer error report
    EstReq           = 0x04,  ///< EST_REQ: radio link establishment request
    EstConf          = 0x05,  ///< EST_CONF: radio link establishment confirm
    EstInd           = 0x06,  ///< EST_IND: radio link establishment indication
    RelReq           = 0x07,  ///< REL_REQ: radio link release request
    RelConf          = 0x08,  ///< REL_CONF: radio link release confirm
    RelInd           = 0x09,  ///< REL_IND: radio link release indication
    UnitDataReq      = 0x0A,  ///< UNIT_DATA_REQ: unnumbered L3 transfer
    UnitDataInd      = 0x0B,  ///< UNIT_DATA_IND: unnumbered L3 transfer
    SuspReq          = 0x0C,  ///< SUSP_REQ (vendor extension)
    SuspConf         = 0x0D,  ///< SUSP_CONF (vendor extension)
    ResReq           = 0x0E,  ///< RES_REQ (vendor extension)
    ReconReq         = 0x0F   ///< RECON_REQ (vendor extension)
};

/// RSL message types of the DCHAN group (TS 48.058 8.4): dedicated channel
/// lifecycle — activation, power control, encryption setup, measurement
/// reporting, mode modification, handover and codec management.
enum class RSLDChanMessageType : uint8_t {
    ChanActiv          = 0x21, ///< CHAN_ACTIV
    ChanActivAck       = 0x22, ///< CHAN_ACTIV_ACK
    ChanActivNack      = 0x23, ///< CHAN_ACTIV_NACK
    ConnFail           = 0x24, ///< CONN_FAIL
    DeactivateSacch    = 0x25, ///< DEACTIVATE_SACCH
    EncrCmd            = 0x26, ///< ENCR_CMD
    HandoDet           = 0x27, ///< HANDO_DET
    MeasRes            = 0x28, ///< MEAS_RES
    ModeModifyReq      = 0x29, ///< MODE_MODIFY_REQ
    ModeModifyAck      = 0x2A, ///< MODE_MODIFY_ACK
    ModeModifyNack     = 0x2B, ///< MODE_MODIFY_NACK
    PhyContextReq      = 0x2C, ///< PHY_CONTEXT_REQ
    PhyContextConf     = 0x2D, ///< PHY_CONTEXT_CONF
    RfChanRel          = 0x2E, ///< RF_CHAN_REL
    MsPowerControl     = 0x2F, ///< MS_POWER_CONTROL
    BsPowerControl     = 0x30, ///< BS_POWER_CONTROL
    PreprocConfig      = 0x31, ///< PREPROC_CONFIG
    PreprocMeasRes     = 0x32, ///< PREPROC_MEAS_RES
    RfChanRelAck       = 0x33, ///< RF_CHAN_REL_ACK
    SacchInfoModify    = 0x34, ///< SACCH_INFO_MODIFY
    TalkerDet          = 0x35, ///< TALKER_DET
    ListenerDet        = 0x36, ///< LISTENER_DET
    RemoteCodecConfRep = 0x37, ///< REMOTE_CODEC_CONF_REP
    RtdRep             = 0x38, ///< RTD_REP
    PreHandoNotif      = 0x39, ///< PRE_HANDO_NOTIF
    MrCodecModReq      = 0x3A, ///< MR_CODEC_MOD_REQ
    MrCodecModAck      = 0x3B, ///< MR_CODEC_MOD_ACK
    MrCodecModNack     = 0x3C, ///< MR_CODEC_MOD_NACK
    MrCodecModPer      = 0x3D, ///< MR_CODEC_MOD_PER
    TfoRep             = 0x3E, ///< TFO_REP
    TfoModReq          = 0x3F  ///< TFO_MOD_REQ
};

/// RSL message types of the CCHAN group (TS 48.058 8.5): common channel
/// management — system information, paging, SMS broadcast, immediate
/// assignment and load reporting.
enum class RSLCChanMessageType : uint8_t {
    BcchInfo           = 0x11, ///< BCCH_INFO
    CcchLoadInd        = 0x12, ///< CCCH_LOAD_IND
    ChanRqd            = 0x13, ///< CHAN_RQD
    DeleteInd          = 0x14, ///< DELETE_IND
    PagingCmd          = 0x15, ///< PAGING_CMD
    ImmediateAssignCmd = 0x16, ///< IMMEDIATE_ASSIGN_CMD
    SmsBcReq           = 0x17, ///< SMS_BC_REQ
    ChanConf           = 0x18, ///< CHAN_CONF (vendor extension)
    RfResInd           = 0x19, ///< RF_RES_IND
    SacchFill          = 0x1A, ///< SACCH_FILL
    Overload           = 0x1B, ///< OVERLOAD
    ErrorReport        = 0x1C, ///< ERROR_REPORT
    SmsBcCmd           = 0x1D, ///< SMS_BC_CMD
    CbchLoadInd        = 0x1E, ///< CBCH_LOAD_IND
    NotCmd             = 0x1F  ///< NOT_CMD
};

/// RSL information element type codes (TS 48.058 9.3). IEs carry channel
/// parameters, encryption keys, measurement data, and L3 payloads within
/// RSL messages; the vendor extension blocks (Osmo, ip.access) follow the
/// same coding. Type code 0x1D is not defined and has no member here.
enum class RSL_IE : uint8_t {
    ChanNr           = 0x01, ///< Channel Number
    LinkIdent        = 0x02, ///< Link Identifier
    ActType          = 0x03, ///< Activation Type
    BSPower          = 0x04, ///< BS Power
    ChanIdent        = 0x05, ///< Channel Identification
    ChanMode         = 0x06, ///< Channel Mode
    EncrInfo         = 0x07, ///< Encryption Info
    FrameNumber      = 0x08, ///< Frame Number
    HandoRef         = 0x09, ///< Handover Reference
    L1Info           = 0x0A, ///< L1 Information
    L3Info           = 0x0B, ///< L3 Information
    MSIdentity       = 0x0C, ///< MS Identity
    MSPower          = 0x0D, ///< MS Power
    PagingGroup      = 0x0E, ///< Paging Group
    PagingLoad       = 0x0F, ///< Paging Load
    PyhsContext      = 0x10, ///< PYHS Context (vendor extension)
    AccessDelay      = 0x11, ///< Access Delay
    RachLoad         = 0x12, ///< RACH Load
    ReqReference     = 0x13, ///< Request Reference
    ReleaseMode      = 0x14, ///< Release Mode
    ResourceInfo     = 0x15, ///< Resource Info
    RlmCause         = 0x16, ///< RLM Cause
    StartngTime      = 0x17, ///< Starting Time
    TimingAdvance    = 0x18, ///< Timing Advance
    UplinkMeas       = 0x19, ///< Uplink Measurements
    Cause            = 0x1A, ///< Cause
    MeasResNr        = 0x1B, ///< Measurement Result Number
    MsgId            = 0x1C, ///< Message Identifier
    SysInfoType      = 0x1E, ///< System Information Type
    MSPowerParam     = 0x1F, ///< MS Power Parameters
    BSPowerParam     = 0x20, ///< BS Power Parameters
    PreprocParam     = 0x21, ///< Preprocessing Parameters
    PreprocMeas      = 0x22, ///< Preprocessing Measurement
    ImmAssInfo       = 0x23, ///< Immediate Assignment Info
    SmscbInfo        = 0x24, ///< SMS-CB Info
    MSTimingOffset   = 0x25, ///< MS Timing Offset
    ErrMsg           = 0x26, ///< Error Message
    FullBCCHInfo     = 0x27, ///< Full BCCH Information
    ChanNeeded       = 0x28, ///< Channel Needed
    CbCmdType        = 0x29, ///< CB Command Type
    SmscbMsg         = 0x2A, ///< SMS-CB Message
    FullImmAssInfo   = 0x2B, ///< Full Immediate Assignment Info
    SacchInfo        = 0x2C, ///< SACCH Information
    CbchLoadInfo     = 0x2D, ///< CBCH Load Info
    SmscbChanIndicator = 0x2E, ///< SMS-CB Channel Indicator
    GroupCallRef     = 0x2F, ///< Group Call Reference
    GroupChanDesc    = 0x30, ///< Group Channel Description
    NchDrxInfo       = 0x31, ///< NCH DRX Information
    CmdIndicator     = 0x32, ///< Command Indicator
    EmlppPrio        = 0x33, ///< EMLPP Priority
    Uic              = 0x34, ///< UIC (UICC) Information
    MainChanRef      = 0x35, ///< Main Channel Reference
    MrConfig         = 0x36, ///< Multirate Configuration
    MrControl        = 0x37, ///< Multirate Control
    SuppCodecTypes   = 0x38, ///< Supported Codec Types
    CodecConfig      = 0x39, ///< Codec Configuration
    Rtd              = 0x3A, ///< Round Trip Delay
    TfoStatus        = 0x3B, ///< TFO Status
    LlpApdu          = 0x3C, ///< LLP APDU
    OsMoRepAcchCap   = 0x60, ///< Osmo: reported ACCH capability (vendor extension)
    OsMoTrainingSequence = 0x61, ///< Osmo: training sequence set (vendor extension)
    OsMoTopAcchCap   = 0x62, ///< Osmo: top ACCH capability (vendor extension)
    OsMoOsmuxCid     = 0x63, ///< Osmo: Osmux CID (vendor extension)
    IpacSrtpConfig   = 0xE0, ///< ip.access: SRTP configuration
    IpacProxyUdp     = 0xE1, ///< ip.access: proxy UDP
    IpacBscmplTout   = 0xE2, ///< ip.access: BSCMPL timeout
    IpacRemoteIp     = 0xF0, ///< ip.access: remote IP address
    IpacRemotePort   = 0xF1, ///< ip.access: remote port
    IpacRtpPayload   = 0xF2, ///< ip.access: RTP payload type
    IpacLocalPort    = 0xF3, ///< ip.access: local port
    IpacSpeechMode   = 0xF4, ///< ip.access: speech mode
    IpacLocalIp      = 0xF5, ///< ip.access: local IP address
    IpacConnStat     = 0xF6, ///< ip.access: connection statistics
    IpacHoCParms     = 0xF7, ///< ip.access: handover C parameters
    IpacConnId       = 0xF8, ///< ip.access: connection identifier
    IpacRtpCsdFmt    = 0xF9, ///< ip.access: RTP CSD format
    IpacRtpJitBuf    = 0xFA, ///< ip.access: RTP jitter buffer
    IpacRtpCompr     = 0xFB, ///< ip.access: RTP compression
    IpacRtpPayload2  = 0xFC, ///< ip.access: second RTP payload type
    IpacRtpMplex     = 0xFD, ///< ip.access: RTP multiplex
    IpacRtpMplexId   = 0xFE  ///< ip.access: RTP multiplex identifier
};

/// RSL information element encoding classes (TS 48.058 9.3):
/// - TV:    type + fixed value octets, no length field;
/// - LV:    type + 8-bit length + value;
/// - TL16V: type + 16-bit big-endian length + value (payloads above 255).
enum class RSLEIEncoding : uint8_t { TV, LV, TL16V };

/// Encoding class of an RSL IE type code (TS 48.058 9.3). The class is fixed
/// per IE: TV IEs have a constant value size, LV/TL16V IEs carry their value
/// length explicitly. Only the L3 Information IE (0x0B) uses the 16-bit
/// length form; the Full BCCH Information IE (0x27) is an ordinary LV IE.
/// Unknown type codes are decoded as LV (variable), which keeps malformed or
/// vendor frames parseable without desynchronizing the IE list.
[[nodiscard]] constexpr RSLEIEncoding rslIeEncoding(uint8_t iei) noexcept {
    switch (iei) {
        // TV: fixed value, no length octet.
        case 0x01: // ChanNr (1)
        case 0x02: // LinkIdent (1)
        case 0x03: // ActType (1)
        case 0x04: // BSPower (1)
        case 0x08: // FrameNumber (2)
        case 0x09: // HandoRef (1)
        case 0x0A: // L1Info (2)
        case 0x0D: // MSPower (1)
        case 0x0E: // PagingGroup (1)
        case 0x0F: // PagingLoad (2)
        case 0x11: // AccessDelay (1)
        case 0x13: // ReqReference (3)
        case 0x14: // ReleaseMode (1)
        case 0x17: // StartngTime (2)
        case 0x18: // TimingAdvance (1)
        case 0x1B: // MeasResNr (1)
        case 0x1C: // MsgId (1)
        case 0x1E: // SysInfoType (1)
        case 0x25: // MSTimingOffset (1)
        case 0x28: // ChanNeeded (1)
        case 0x29: // CbCmdType (1)
        case 0x2D: // CbchLoadInfo (1)
        case 0x2E: // SmscbChanIndicator (1)
        case 0x37: // MrControl (1)
            return RSLEIEncoding::TV;

        // TL16V: 16-bit big-endian length for large payloads.
        case 0x0B: // L3Info (the only TL16V IE)
            return RSLEIEncoding::TL16V;

        // LV (8-bit length): FullBCCHInfo, all other defined IEs and unknown codes.
        default:
            return RSLEIEncoding::LV;
    }
}

/// Value size in octets of a TV IE (0 when the IE is not TV-encoded).
[[nodiscard]] constexpr uint8_t rslIeTvValueSize(uint8_t iei) noexcept {
    switch (iei) {
        case 0x08: // FrameNumber
        case 0x0A: // L1Info
        case 0x0F: // PagingLoad
        case 0x17: // StartngTime
            return 2;
        case 0x13: // ReqReference
            return 3;
        case 0x01: // ChanNr
        case 0x02: // LinkIdent
        case 0x03: // ActType
        case 0x04: // BSPower
        case 0x09: // HandoRef
        case 0x0D: // MSPower
        case 0x0E: // PagingGroup
        case 0x11: // AccessDelay
        case 0x14: // ReleaseMode
        case 0x18: // TimingAdvance
        case 0x1B: // MeasResNr
        case 0x1C: // MsgId
        case 0x1E: // SysInfoType
        case 0x25: // MSTimingOffset
        case 0x28: // ChanNeeded
        case 0x29: // CbCmdType
        case 0x2D: // CbchLoadInfo
        case 0x2E: // SmscbChanIndicator
        case 0x37: // MrControl
            return 1;
        default:
            return 0;
    }
}

/// RSL error cause values (TS 48.058 section 9.3.26). Carried by the Cause IE
/// of NACK and failure messages; each value is a single octet on the wire.
enum class RSLErrorCause : uint8_t {
    // Normal events.
    RadioIfFail          = 0x00, ///< Radio interface failure
    RadioLinkFail        = 0x01, ///< Radio link failure
    HandoverAccFail      = 0x02, ///< Handover access failure
    TalkerAccFail        = 0x03, ///< Talker access failure
    OmIntervention       = 0x07, ///< OM intervention
    NormalUnspec         = 0x0F, ///< Normal, unspecified
    TMsrfpciExp          = 0x18, ///< TMSI/RFCI expiry
    // Resource unavailable.
    EquipmentFail        = 0x20, ///< Equipment failure
    RrUnavail            = 0x21, ///< RR layer unavailable
    TerrChFail           = 0x22, ///< Terrestrial channel failure
    CcchOverload         = 0x23, ///< CCCH overload
    AcchOverload         = 0x24, ///< ACCH overload
    ProcessorOverload    = 0x25, ///< Processor overload
    BtsNotEquipped       = 0x27, ///< BTS not equipped
    RemoteTrauFailure    = 0x28, ///< Remote TRAU failure
    NotifOverflow        = 0x29, ///< Notification overflow
    ResUnavail           = 0x2F, ///< Resource unavailable
    // Service or option not available.
    TranscUnavail        = 0x30, ///< Transcoder unavailable
    ServOptUnavail       = 0x3F, ///< Service option unavailable
    // Service or option not implemented.
    EncrUnimpl           = 0x40, ///< Encryption not implemented
    ServOptUnimpl        = 0x4F, ///< Service option not implemented
    // Invalid message.
    RchAlrActvAlloc      = 0x50, ///< Channel already active/allocated
    IpaRchNotActvAlloc   = 0x51, ///< IPA channel not active/allocated
    IpaConnInvalid       = 0x52, ///< IPA connection invalid
    IpaConnInUse         = 0x53, ///< IPA connection in use
    IpaConnAlreadyExists = 0x54, ///< IPA connection already exists
    InvalidMessage       = 0x5F, ///< Invalid message
    // Protocol error.
    MsgDiscr             = 0x60, ///< Message discriminator error
    MsgType              = 0x61, ///< Message type error
    MsgSeq               = 0x62, ///< Message sequence error
    IeError              = 0x63, ///< IE error
    MandIeError          = 0x64, ///< Mandatory IE error
    OptIeError           = 0x65, ///< Optional IE error
    IeNonexist           = 0x66, ///< IE non-existent
    IeLength             = 0x67, ///< IE wrong length
    IeContent            = 0x68, ///< IE wrong content
    Proto                = 0x6F, ///< Protocol error
    // Interworking.
    Interworking         = 0x7F  ///< Interworking, unspecified
};

/// Domain check for a raw RSL error cause octet (TS 48.058 section 9.3.26):
/// true only when the value is one of the defined causes above; the code
/// space contains reserved gaps that must not be emitted on the wire.
[[nodiscard]] constexpr bool isRslErrorCause(uint8_t value) noexcept {
    switch (value) {
        case 0x00u: case 0x01u: case 0x02u: case 0x03u: case 0x07u:
        case 0x0Fu: case 0x18u: case 0x20u: case 0x21u: case 0x22u:
        case 0x23u: case 0x24u: case 0x25u: case 0x27u: case 0x28u:
        case 0x29u: case 0x2Fu: case 0x30u: case 0x3Fu: case 0x40u:
        case 0x4Fu: case 0x50u: case 0x51u: case 0x52u: case 0x53u:
        case 0x54u: case 0x5Fu: case 0x60u: case 0x61u: case 0x62u:
        case 0x63u: case 0x64u: case 0x65u: case 0x66u: case 0x67u:
        case 0x68u: case 0x6Fu: case 0x7Fu:
            return true;
        default:
            return false;
    }
}

/// RSL Channel Number IE value coding (TS 48.058 9.3.1): a five-bit channel
/// code in the high bits and a three-bit timeslot number in the low bits,
/// i.e. value = (code << 3) | tn. Sub-channelized types use the code base
/// plus the sub-channel index: Lm codes 2..3 ('0001's'B), SDCCH/4 codes
/// 4..7 ('001'ss'B), SDCCH/8 codes 8..15 ('01sss'B).
struct RSLChannelNumber {
    // Channel codes (the five most significant bits of the IE value).
    static constexpr uint8_t Invalid = 0x00; ///< '00000'B — invalid
    static constexpr uint8_t BmAcch  = 0x01; ///< '00001'B — TCH/F or TCH/H ACCH
    static constexpr uint8_t Lm      = 0x02; ///< '0001's'B — TCH/H sub-slot, add 0/1
    static constexpr uint8_t Sdcch4  = 0x04; ///< '001'ss'B — SDCCH/4, add sub-channel 0..3
    static constexpr uint8_t Sdcch8  = 0x08; ///< '01sss'B — SDCCH/8, add sub-channel 0..7
    static constexpr uint8_t Bcch    = 0x10; ///< '10000'B — BCCH
    static constexpr uint8_t Rach    = 0x11; ///< '10001'B — RACH
    static constexpr uint8_t PchAgch = 0x12; ///< '10010'B — PCH + AGCH
    static constexpr uint8_t Pdch    = 0x18; ///< '11000'B — dynamic PDCH (vendor extension)
    static constexpr uint8_t Cbch4   = 0x19; ///< '11001'B — CBCH/4 (vendor extension)
    static constexpr uint8_t Cbch8   = 0x1A; ///< '11010'B — CBCH/8 (vendor extension)
    static constexpr uint8_t VamosBm = 0x1D; ///< '11101'B — VAMOS TCH/F ACCH (vendor extension)

    /// Encode a channel number from the five-bit code and timeslot.
    /// @param code Channel type code (0-31, see constants above)
    /// @param tn Timeslot number (0-7)
    /// @return Encoded channel number octet
    [[nodiscard]] static uint8_t encode(uint8_t code, uint8_t tn) noexcept {
        return static_cast<uint8_t>(((code & 0x1Fu) << 3) | (tn & 0x07u));
    }

    /// Extract the five-bit channel code from an encoded channel number.
    /// @param chanNr Encoded channel number octet
    /// @return High five bits (channel type identifier, 0-31)
    [[nodiscard]] static uint8_t getCBits(uint8_t chanNr) noexcept {
        return static_cast<uint8_t>(chanNr >> 3);
    }

    /// Extract the timeslot from an encoded channel number.
    /// @param chanNr Encoded channel number octet
    /// @return Low three bits (timeslot 0-7)
    [[nodiscard]] static uint8_t getTimeslot(uint8_t chanNr) noexcept {
        return chanNr & 0x07u;
    }

    /// Check whether a channel number denotes a dedicated physical channel.
    /// Common channels occupy codes 16..31 (BCCH, RACH, PCH/AGCH and the
    /// vendor common-channel extensions).
    /// @param chanNr Encoded channel number octet
    /// @return true if this is a dedicated channel
    [[nodiscard]] static bool isDedicated(uint8_t chanNr) noexcept {
        return (chanNr & 0xC0u) != 0x80u;
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

/// Channel Mode IE value part (TS 48.058 section 9.3.6): exactly four octets —
/// the DTX indicators, the speech/data/signalling indicator, the channel rate
/// and type, and a fourth octet whose meaning depends on the indicator: the
/// speech coding algorithm for speech channels, an opaque data rate code for
/// data channels, and zero for signalling channels (no resource).
struct RSLChannelMode {
    uint8_t dtx{0};            ///< [reserved(6)|DTX_d(1)|DTX_u(1)]
    uint8_t spdInd{0};         ///< Speed indicator: 1=speech, 2=data, 3=signalling
    uint8_t chanRateType{0};   ///< Channel rate and type; see ChanRateType
    uint8_t valueOctet{0};     ///< Speech algorithm / data rate code / 0x00 (signalling)

    enum SpeedIndicator : uint8_t { Speech = 0x01, Data = 0x02, Signalling = 0x03 };
    enum ChanRateType : uint8_t {
        Sdcch = 0x01, TchF = 0x08, TchH = 0x09, TchFBdMslot = 0x0A,
        TchFDlMslot = 0x1A, TchFGroup = 0x18, TchHGroup = 0x19,
        TchFBcast = 0x28, TchHBcast = 0x29, OsMoTchFVamos = 0x88, OsMoTchHVamos = 0x89 };

    [[nodiscard]] constexpr bool dtxDownlink() const noexcept { return (dtx & 0x02u) != 0; }
    [[nodiscard]] constexpr bool dtxUplink()   const noexcept { return (dtx & 0x01u) != 0; }
    [[nodiscard]] constexpr bool isSignalling() const noexcept { return spdInd == SpeedIndicator::Signalling; }
    [[nodiscard]] constexpr bool isSpeech()     const noexcept { return spdInd == SpeedIndicator::Speech; }
    [[nodiscard]] constexpr bool isData()       const noexcept { return spdInd == SpeedIndicator::Data; }
};
static_assert(sizeof(RSLChannelMode) == 4, "RSLChannelMode must be exactly 4 bytes");

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
