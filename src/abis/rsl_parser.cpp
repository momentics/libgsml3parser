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

// RSL header sizes by discriminator:
// RLL: discriminator(1) + msg_type(1) + chan_nr(1) + link_id(1) = 4 bytes
// DCHAN: discriminator(1) + msg_type(1) + chan_nr(1) + reserved(1) = 4 bytes
// CCHAN: discriminator(1) + msg_type(1) + chan_nr(1) + reserved(1) = 4 bytes
// TRX: discriminator(1) + msg_type(1) + trx_nr(1) + reserved(1) = 4 bytes
constexpr size_t RSL_HEADER_SIZE = 4;

// Determine IE encoding format from type code.
// TV = type + 1-byte value (no length field)
// TLV = type + length(1) + value(length)
// TL16V = type + length(2, big-endian) + value(length)
enum class IEEncoding : uint8_t { TV, TLV, TL16V };

IEEncoding ieEncoding(uint8_t type) noexcept {
    switch (type) {
        // TV IEs: fixed 1-byte value, no length field.
        case 0x11: // ChanNr
        case 0x12: // LinkIdent
        case 0x21: // ActType
        case 0x24: // BSPower
        case 0x25: // MSPower
        case 0x26: // HandoRef
        case 0x28: // Cause
        case 0x29: // AccessDelay
        case 0x2d: // PagingGroup
        case 0x2e: // ChanNeeded
        case 0x31: // SysInfoType
        case 0x33: // MeasResNr
        case 0x36: // TimingAdvance
        case 0x37: // MSTimingOffset
        case 0x38: // ReleaseMode
            return IEEncoding::TV;

        // TL16V IEs: 16-bit length for large payloads.
        case 0x30: // L3Info
        case 0x32: // FullBCCHInfo
            return IEEncoding::TL16V;

        // All other IEs use standard TLV encoding.
        default:
            return IEEncoding::TLV;
    }
}

// Parse IEs (TV, TLV, TL16V) from the payload after the RSL header.
// Returns the number of IEs parsed (capped at MAX_IE).
size_t parseIEs(const uint8_t* data, size_t len, RSLParsedMessage::IE ies[], size_t maxIes) {
    size_t count = 0;
    size_t pos = 0;

    while (pos < len && count < maxIes) {
        uint8_t type = data[pos];
        ++pos;

        switch (ieEncoding(type)) {
            case IEEncoding::TV: {
                // TV: type(1) + value(1), no length field.
                if (pos >= len) break; // truncated
                if (count < maxIes) {
                    ies[count].type = type;
                    ies[count].len = 1;
                    ies[count].val = data + pos;
                    ++count;
                }
                ++pos;
                break;
            }
            case IEEncoding::TL16V: {
                // TL16V: type(1) + length(2, big-endian) + value(length).
                if (pos + 2 > len) break; // truncated length field
                uint16_t vlen = static_cast<uint16_t>(data[pos]) << 8 | data[pos + 1];
                pos += 2;
                if (pos + vlen > len) break; // truncated value
                if (count < maxIes) {
                    ies[count].type = type;
                    ies[count].len = static_cast<uint8_t>(vlen > 255 ? 255 : vlen);
                    ies[count].val = data + pos;
                    ++count;
                }
                pos += vlen;
                break;
            }
            case IEEncoding::TLV: {
                // TLV: type(1) + length(1) + value(length).
                if (pos >= len) break; // truncated length
                uint8_t vlen = data[pos];
                ++pos;
                if (pos + vlen > len) break; // truncated value
                if (count < maxIes) {
                    ies[count].type = type;
                    ies[count].len = vlen;
                    ies[count].val = data + pos;
                    ++count;
                }
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

    // Parse discriminator and message type.
    msg.discriminator = static_cast<RSLDiscriminator>(data[0]);
    msg.msgType = data[1];

    // Validate discriminator is known.
    if (data[0] != static_cast<uint8_t>(RSLDiscriminator::RLL) &&
        data[0] != static_cast<uint8_t>(RSLDiscriminator::CommonChannel) &&
        data[0] != static_cast<uint8_t>(RSLDiscriminator::DedicatedChannel) &&
        data[0] != static_cast<uint8_t>(RSLDiscriminator::TRX) &&
        data[0] != static_cast<uint8_t>(RSLDiscriminator::IPAccess)) {
        return Expected<RSLParsedMessage>::error(
            ParseError{ParseError::Code::InvalidValue, "Unknown RSL discriminator"});
    }

    // All discriminators have a 4-byte common header.
    if (data.size() < RSL_HEADER_SIZE) {
        return Expected<RSLParsedMessage>::error(
            ParseError{ParseError::Code::TruncatedInput, "RSL message truncated before header complete"});
    }

    msg.chanNr = data[2];

    // RLL messages include link ID in byte 3.
    if (msg.discriminator == RSLDiscriminator::RLL) {
        msg.linkId = data[3];
    } else {
        msg.linkId = 0;
    }

    // Parse TLV IEs from payload after header.
    const uint8_t* payloadStart = data.data() + RSL_HEADER_SIZE;
    size_t payloadLen = data.size() - RSL_HEADER_SIZE;
    msg.ieCount = parseIEs(payloadStart, payloadLen, msg.informationElements.data(), RSLParsedMessage::MAX_IE);

    // Extract L3 payload for messages that carry it.
    // DATA_REQ, DATA_IND, UNIT_DATA_REQ, UNIT_DATA_IND: L3 is the entire payload after header.
    if (msg.discriminator == RSLDiscriminator::RLL) {
        uint8_t mtype = msg.msgType;
        if (mtype == static_cast<uint8_t>(RSLL3MessageType::DataReq) ||
            mtype == static_cast<uint8_t>(RSLL3MessageType::DataInd) ||
            mtype == static_cast<uint8_t>(RSLL3MessageType::UnitDataReq) ||
            mtype == static_cast<uint8_t>(RSLL3MessageType::UnitDataInd)) {
            if (payloadLen > 0) {
                msg.l3Payload = std::span<const uint8_t>(payloadStart, payloadLen);
            }
        }
    }

    // For BCCH_INFO, ENCR_CMD, PAGING_CMD: L3 is in the L3Info IE.
    if (msg.discriminator == RSLDiscriminator::CommonChannel ||
        msg.discriminator == RSLDiscriminator::DedicatedChannel) {
        auto* l3IE = findIE(msg, RSL_IE::L3Info);
        if (l3IE && l3IE->val) {
            // For TL16V IEs, recalculate actual length from the raw data.
            for (size_t i = 0; i < msg.ieCount; ++i) {
                if (msg.informationElements[i].val == l3IE->val) {
                    // The val pointer already points to the value data.
                    // For TL16V, the length is 2 bytes before val.
                    const uint8_t* lenPos = l3IE->val - 2;
                    if (lenPos >= payloadStart) {
                        uint16_t actualLen = static_cast<uint16_t>(lenPos[0]) << 8 | lenPos[1];
                        if (actualLen > 0 && l3IE->val + actualLen <= data.data() + data.size()) {
                            msg.l3Payload = std::span<const uint8_t>(l3IE->val, actualLen);
                        } else {
                            msg.l3Payload = std::span<const uint8_t>(l3IE->val, l3IE->len);
                        }
                    } else {
                        msg.l3Payload = std::span<const uint8_t>(l3IE->val, l3IE->len);
                    }
                    break;
                }
            }
        }
    }

    // FullBCCHInfo IE also carries L3-like system information.
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
        case RSLDiscriminator::RLL:
            switch (static_cast<RSLL3MessageType>(msgType)) {
                case RSLL3MessageType::DataReq:          return "DATA_REQ";
                case RSLL3MessageType::DataInd:          return "DATA_IND";
                case RSLL3MessageType::UnitDataReq:      return "UNIT_DATA_REQ";
                case RSLL3MessageType::UnitDataInd:      return "UNIT_DATA_IND";
                case RSLL3MessageType::EstablishmentInd: return "ESTABLISHMENT_IND";
                case RSLL3MessageType::ReleaseReq:       return "RELEASE_REQ";
                case RSLL3MessageType::ReleaseInd:       return "RELEASE_IND";
                default: break;
            }
            break;

        case RSLDiscriminator::DedicatedChannel:
            switch (static_cast<RSLDChanMessageType>(msgType)) {
                case RSLDChanMessageType::ChanActiv:      return "CHAN_ACTIV";
                case RSLDChanMessageType::RFChanRel:      return "RF_CHAN_REL";
                case RSLDChanMessageType::SACCHInfoModify: return "SACCH_INFO_MODIFY";
                case RSLDChanMessageType::DeactivateSACCH: return "DEACTIVATE_SACCH";
                case RSLDChanMessageType::EncrCmd:        return "ENCR_CMD";
                case RSLDChanMessageType::ModeModifyReq:  return "MODE_MODIFY_REQ";
                case RSLDChanMessageType::MS_PowerControl: return "MS_POWER_CONTROL";
                case RSLDChanMessageType::BS_PowerControl: return "BS_POWER_CONTROL";
                case RSLDChanMessageType::ChanActivAck:   return "CHAN_ACTIV_ACK";
                case RSLDChanMessageType::ChanActivNack:  return "CHAN_ACTIV_NACK";
                case RSLDChanMessageType::RFChanRelAck:   return "RF_CHAN_REL_ACK";
                case RSLDChanMessageType::ConnFail:       return "CONN_FAIL";
                case RSLDChanMessageType::MeasRes:        return "MEAS_RES";
                case RSLDChanMessageType::HandoDet:       return "HANDO_DET";
                default: break;
            }
            break;

        case RSLDiscriminator::CommonChannel:
            switch (static_cast<RSLCChanMessageType>(msgType)) {
                case RSLCChanMessageType::BCCHInfo:          return "BCCH_INFO";
                case RSLCChanMessageType::ImmediateAssignCmd: return "IMMEDIATE_ASSIGN_CMD";
                case RSLCChanMessageType::PagingCmd:         return "PAGING_CMD";
                case RSLCChanMessageType::SMSBCCmd:          return "SMS_BC_CMD";
                case RSLCChanMessageType::CCCHLoadInd:       return "CCCH_LOAD_IND";
                case RSLCChanMessageType::DeleteInd:         return "DELETE_IND";
                case RSLCChanMessageType::ChanRqd:           return "CHAN_RQD";
                default: break;
            }
            break;

        default:
            break;
    }
    return "UNKNOWN";
}

} // namespace gsml3parser
