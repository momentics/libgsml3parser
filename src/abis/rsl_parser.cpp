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

#include "gsml3parser/abis/rsl_parser.h"
#include <cstring>

namespace gsml3parser {

namespace {

// Parse the information element list that follows the two RSL header octets.
// Encoding classes per IE type code (TS 48.058 9.3, rslIeEncoding): TV IEs
// carry a fixed-size value with no length field, LV IEs an 8-bit length, and
// TL16V IEs a 16-bit big-endian length. A truncated IE stops the scan; the
// IEs parsed so far are kept (graceful degradation on malformed frames).
size_t parseIEs(const uint8_t* data, size_t len, RSLParsedMessage::IE ies[], size_t maxIes) {
    size_t count = 0;
    size_t pos = 0;

    while (pos < len && count < maxIes) {
        const uint8_t type = data[pos];
        ++pos;

        switch (rslIeEncoding(type)) {
            case RSLEIEncoding::TV: {
                // TV: type + fixed value octets, no length field.
                const size_t vsize = rslIeTvValueSize(type);
                if (pos + vsize > len) return count; // truncated value
                ies[count].type = type;
                ies[count].len = static_cast<uint16_t>(vsize);
                ies[count].val = data + pos;
                ++count;
                pos += vsize;
                break;
            }
            case RSLEIEncoding::TL16V: {
                // TL16V: type + length(2, big-endian) + value(length).
                if (pos + 2 > len) return count; // truncated length field
                const size_t vlen = static_cast<size_t>(static_cast<uint16_t>((data[pos] << 8) | data[pos + 1]));
                pos += 2;
                if (pos + vlen > len) return count; // truncated value
                ies[count].type = type;
                ies[count].len = static_cast<uint16_t>(vlen);
                ies[count].val = data + pos;
                ++count;
                pos += vlen;
                break;
            }
            case RSLEIEncoding::LV: {
                // LV: type + length(1) + value(length).
                if (pos >= len) return count; // truncated length
                const size_t vlen = data[pos];
                ++pos;
                if (pos + vlen > len) return count; // truncated value
                ies[count].type = type;
                ies[count].len = static_cast<uint16_t>(vlen);
                ies[count].val = data + pos;
                ++count;
                pos += vlen;
                break;
            }
        }
    }
    return count;
}

} // anonymous namespace

Expected<RSLParsedMessage> RSLParser::parse(std::span<const uint8_t> data)
{
    RSLParsedMessage msg;
    msg.rawData = data;

    if (data.size() < 2) {
        return Expected<RSLParsedMessage>::error(
            ParseError{ParseError::Code::TruncatedInput, "RSL message too short for header"});
    }

    // First octet: 7-bit message group plus the transparent indication
    // flag in bit 0 (TS 48.058 9.1).
    const auto firstOctet = decodeRslFirstOctet(data[0]);
    if (!firstOctet) {
        return Expected<RSLParsedMessage>::error(
            ParseError{ParseError::Code::InvalidValue, "Reserved RSL message group"});
    }
    const auto group = rslGroupToDiscriminator(firstOctet->group);
    if (!group) {
        return Expected<RSLParsedMessage>::error(
            ParseError{ParseError::Code::InvalidValue, "Unknown RSL message group"});
    }
    msg.discriminator = *group;
    msg.transparent = firstOctet->transparent;
    msg.msgType = data[1];

    // The remainder of the frame is the information element list; the
    // Channel Number and Link Identifier are ordinary TV IEs of that list
    // (TS 48.058 9.3.1/9.3.2).
    const uint8_t* payloadStart = data.data() + 2;
    const size_t payloadLen = data.size() - 2;
    msg.ieCount = parseIEs(payloadStart, payloadLen, msg.informationElements.data(), RSLParsedMessage::MAX_IE);

    if (const auto* chanNrIE = findIE(msg, RSL_IE::ChanNr)) {
        msg.chanNr = *chanNrIE->val;
    }
    if (const auto* linkIdIE = findIE(msg, RSL_IE::LinkIdent)) {
        msg.linkId = *linkIdIE->val;
    }

    // Extract the L3 payload for messages that carry one. RLL data messages
    // (DATA_REQ/DATA_IND/UNIT_DATA_*) and CCHAN/DCHAN messages such as
    // BCCH_INFO, ENCR_CMD or PAGING_CMD wrap the L3 PDU in the L3
    // Information IE (TL16V, TS 48.058 9.3.x); when several are present the
    // first one is used (one L3 PDU per RSL frame in this library's pipeline).
    if (msg.discriminator == RSLDiscriminator::Rll) {
        const uint8_t mtype = msg.msgType;
        if (mtype == static_cast<uint8_t>(RSLL3MessageType::DataReq) ||
            mtype == static_cast<uint8_t>(RSLL3MessageType::DataInd) ||
            mtype == static_cast<uint8_t>(RSLL3MessageType::UnitDataReq) ||
            mtype == static_cast<uint8_t>(RSLL3MessageType::UnitDataInd)) {
            if (auto* l3IE = findIE(msg, RSL_IE::L3Info)) {
                msg.l3Payload = std::span<const uint8_t>(l3IE->val, l3IE->len);
            }
        }
    } else if (msg.discriminator == RSLDiscriminator::CommonChannel ||
               msg.discriminator == RSLDiscriminator::DedicatedChannel) {
        if (auto* l3IE = findIE(msg, RSL_IE::L3Info)) {
            msg.l3Payload = std::span<const uint8_t>(l3IE->val, l3IE->len);
        }
    }

    // The Full BCCH Information IE also carries system information.
    if (msg.l3Payload.empty()) {
        auto* bcchIE = findIE(msg, RSL_IE::FullBCCHInfo);
        if (bcchIE && bcchIE->val) {
            msg.l3Payload = std::span<const uint8_t>(bcchIE->val, bcchIE->len);
        }
    }

    return Expected<RSLParsedMessage>::hold(std::move(msg));
}

std::optional<std::span<const uint8_t>> RSLParser::extractL3(const RSLParsedMessage& parsed)
{
    if (parsed.l3Payload.empty()) return std::nullopt;
    return parsed.l3Payload;
}

const RSLParsedMessage::IE* RSLParser::findIE(const RSLParsedMessage& parsed, RSL_IE ieType) noexcept
{
    uint8_t target = static_cast<uint8_t>(ieType);
    for (size_t i = 0; i < parsed.ieCount; ++i) {
        if (parsed.informationElements[i].type == target) {
            return &parsed.informationElements[i];
        }
    }
    return nullptr;
}

std::optional<RSLChannelMode> RSLParser::getChannelMode(const RSLParsedMessage& parsed) noexcept
{
    auto* ie = findIE(parsed, RSL_IE::ChanMode);
    if (!ie || !ie->val || ie->len < sizeof(RSLChannelMode)) return std::nullopt;

    RSLChannelMode mode{};
    std::memcpy(&mode, ie->val, sizeof(RSLChannelMode));
    return mode;
}

std::optional<RSLEncryptionInfo> RSLParser::getEncryptionInfo(const RSLParsedMessage& parsed) noexcept
{
    auto* ie = findIE(parsed, RSL_IE::EncrInfo);
    if (!ie || !ie->val || ie->len < 2) return std::nullopt;

    RSLEncryptionInfo info;
    info.algorithmId = ie->val[0];
    // Remaining bytes are the key.
    info.key = std::span<const uint8_t>(ie->val + 1, ie->len - 1);
    return info;
}

std::string_view RSLParser::messageName(RSLDiscriminator disc, uint8_t msgType)
{
    switch (disc) {
        case RSLDiscriminator::Rll:
            switch (static_cast<RSLL3MessageType>(msgType)) {
                case RSLL3MessageType::DataReq:     return "DATA_REQ";
                case RSLL3MessageType::DataInd:     return "DATA_IND";
                case RSLL3MessageType::ErrorInd:    return "ERROR_IND";
                case RSLL3MessageType::EstReq:      return "EST_REQ";
                case RSLL3MessageType::EstConf:     return "EST_CONF";
                case RSLL3MessageType::EstInd:      return "EST_IND";
                case RSLL3MessageType::RelReq:      return "REL_REQ";
                case RSLL3MessageType::RelConf:     return "REL_CONF";
                case RSLL3MessageType::RelInd:      return "REL_IND";
                case RSLL3MessageType::UnitDataReq: return "UNIT_DATA_REQ";
                case RSLL3MessageType::UnitDataInd: return "UNIT_DATA_IND";
                case RSLL3MessageType::SuspReq:     return "SUSP_REQ";
                case RSLL3MessageType::SuspConf:    return "SUSP_CONF";
                case RSLL3MessageType::ResReq:      return "RES_REQ";
                case RSLL3MessageType::ReconReq:    return "RECON_REQ";
                default: break;
            }
            break;

        case RSLDiscriminator::DedicatedChannel:
            switch (static_cast<RSLDChanMessageType>(msgType)) {
                case RSLDChanMessageType::ChanActiv:          return "CHAN_ACTIV";
                case RSLDChanMessageType::ChanActivAck:       return "CHAN_ACTIV_ACK";
                case RSLDChanMessageType::ChanActivNack:      return "CHAN_ACTIV_NACK";
                case RSLDChanMessageType::ConnFail:           return "CONN_FAIL";
                case RSLDChanMessageType::DeactivateSacch:    return "DEACTIVATE_SACCH";
                case RSLDChanMessageType::EncrCmd:            return "ENCR_CMD";
                case RSLDChanMessageType::HandoDet:           return "HANDO_DET";
                case RSLDChanMessageType::MeasRes:            return "MEAS_RES";
                case RSLDChanMessageType::ModeModifyReq:      return "MODE_MODIFY_REQ";
                case RSLDChanMessageType::ModeModifyAck:      return "MODE_MODIFY_ACK";
                case RSLDChanMessageType::ModeModifyNack:     return "MODE_MODIFY_NACK";
                case RSLDChanMessageType::PhyContextReq:      return "PHY_CONTEXT_REQ";
                case RSLDChanMessageType::PhyContextConf:     return "PHY_CONTEXT_CONF";
                case RSLDChanMessageType::RfChanRel:          return "RF_CHAN_REL";
                case RSLDChanMessageType::MsPowerControl:     return "MS_POWER_CONTROL";
                case RSLDChanMessageType::BsPowerControl:     return "BS_POWER_CONTROL";
                case RSLDChanMessageType::PreprocConfig:      return "PREPROC_CONFIG";
                case RSLDChanMessageType::PreprocMeasRes:     return "PREPROC_MEAS_RES";
                case RSLDChanMessageType::RfChanRelAck:       return "RF_CHAN_REL_ACK";
                case RSLDChanMessageType::SacchInfoModify:    return "SACCH_INFO_MODIFY";
                case RSLDChanMessageType::TalkerDet:          return "TALKER_DET";
                case RSLDChanMessageType::ListenerDet:        return "LISTENER_DET";
                case RSLDChanMessageType::RemoteCodecConfRep: return "REMOTE_CODEC_CONF_REP";
                case RSLDChanMessageType::RtdRep:             return "RTD_REP";
                case RSLDChanMessageType::PreHandoNotif:      return "PRE_HANDO_NOTIF";
                case RSLDChanMessageType::MrCodecModReq:      return "MR_CODEC_MOD_REQ";
                case RSLDChanMessageType::MrCodecModAck:      return "MR_CODEC_MOD_ACK";
                case RSLDChanMessageType::MrCodecModNack:     return "MR_CODEC_MOD_NACK";
                case RSLDChanMessageType::MrCodecModPer:      return "MR_CODEC_MOD_PER";
                case RSLDChanMessageType::TfoRep:             return "TFO_REP";
                case RSLDChanMessageType::TfoModReq:          return "TFO_MOD_REQ";
                default: break;
            }
            break;

        case RSLDiscriminator::CommonChannel:
            switch (static_cast<RSLCChanMessageType>(msgType)) {
                case RSLCChanMessageType::BcchInfo:           return "BCCH_INFO";
                case RSLCChanMessageType::CcchLoadInd:        return "CCCH_LOAD_IND";
                case RSLCChanMessageType::ChanRqd:            return "CHAN_RQD";
                case RSLCChanMessageType::DeleteInd:          return "DELETE_IND";
                case RSLCChanMessageType::PagingCmd:          return "PAGING_CMD";
                case RSLCChanMessageType::ImmediateAssignCmd: return "IMMEDIATE_ASSIGN_CMD";
                case RSLCChanMessageType::SmsBcReq:           return "SMS_BC_REQ";
                case RSLCChanMessageType::ChanConf:           return "CHAN_CONF";
                case RSLCChanMessageType::RfResInd:           return "RF_RES_IND";
                case RSLCChanMessageType::SacchFill:          return "SACCH_FILL";
                case RSLCChanMessageType::Overload:           return "OVERLOAD";
                case RSLCChanMessageType::ErrorReport:        return "ERROR_REPORT";
                case RSLCChanMessageType::SmsBcCmd:           return "SMS_BC_CMD";
                case RSLCChanMessageType::CbchLoadInd:        return "CBCH_LOAD_IND";
                case RSLCChanMessageType::NotCmd:             return "NOT_CMD";
                default: break;
            }
            break;

        case RSLDiscriminator::TrxManagement:
            if (msgType == 0x41) return "LOCATION_INFO";
            break;

        default:
            break;
    }
    return "UNKNOWN";
}

} // namespace gsml3parser
