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

// Tests for RSLParser: validates parsing of RLL DATA_REQ/DATA_IND, DCHAN CHAN_ACTIV
// with IEs, CCHAN PAGING_CMD, error handling for truncated messages, and L3 payload
// extraction from various message types.
// 3GPP coverage: TS 48.058 (A-bis RSL protocol), GSM 04.08 (L3 encapsulation).

#include <gtest/gtest.h>
#include <vector>
#include "gsml3parser/abis/rsl_parser.h"

using namespace gsml3parser;

// Helper: build a minimal RLL DATA_REQ with L3 payload.
// Frame shape (TS 48.058 8.3.1/9.1): first octet = RLL group (0x01) << 1 |
// transparent, second octet = global message type, then the IE list: Channel
// Number TV, Link Identifier TV, L3 Information TL16V.
static std::vector<uint8_t> makeRLLDataReq(uint8_t chanNr, uint8_t linkId, std::initializer_list<uint8_t> l3) {
    std::vector<uint8_t> buf;
    buf.push_back(rslFirstOctet(RSLDiscriminator::Rll, /*transparent=*/true));
    buf.push_back(static_cast<uint8_t>(RSLL3MessageType::DataReq));
    buf.push_back(static_cast<uint8_t>(RSL_IE::ChanNr));
    buf.push_back(chanNr);
    buf.push_back(static_cast<uint8_t>(RSL_IE::LinkIdent));
    buf.push_back(linkId);
    // L3Info IE (TL16V) per TS 48.058 9.3.x.
    buf.push_back(static_cast<uint8_t>(RSL_IE::L3Info));
    buf.push_back(0x00);
    buf.push_back(static_cast<uint8_t>(l3.size()));
    buf.insert(buf.end(), l3.begin(), l3.end());
    return buf;
}

// Helper: build a minimal RLL DATA_IND with L3 payload (same IE list shape).
static std::vector<uint8_t> makeRLLDataInd(uint8_t chanNr, uint8_t linkId, std::initializer_list<uint8_t> l3) {
    std::vector<uint8_t> buf;
    buf.push_back(rslFirstOctet(RSLDiscriminator::Rll, /*transparent=*/true));
    buf.push_back(static_cast<uint8_t>(RSLL3MessageType::DataInd));
    buf.push_back(static_cast<uint8_t>(RSL_IE::ChanNr));
    buf.push_back(chanNr);
    buf.push_back(static_cast<uint8_t>(RSL_IE::LinkIdent));
    buf.push_back(linkId);
    // L3Info IE (TL16V) per TS 48.058 9.3.x.
    buf.push_back(static_cast<uint8_t>(RSL_IE::L3Info));
    buf.push_back(0x00);
    buf.push_back(static_cast<uint8_t>(l3.size()));
    buf.insert(buf.end(), l3.begin(), l3.end());
    return buf;
}

// Helper: build a DCHAN CHAN_ACTIV with IEs. The Channel Number TV IE is the
// first element of the list (TS 48.058 8.4).
static std::vector<uint8_t> makeDChanActiv(uint8_t chanNr, const std::vector<uint8_t>& ies) {
    std::vector<uint8_t> buf;
    buf.push_back(rslFirstOctet(RSLDiscriminator::DedicatedChannel, /*transparent=*/false));
    buf.push_back(static_cast<uint8_t>(RSLDChanMessageType::ChanActiv));
    buf.push_back(static_cast<uint8_t>(RSL_IE::ChanNr));
    buf.push_back(chanNr);
    buf.insert(buf.end(), ies.begin(), ies.end());
    return buf;
}

// Helper: build a CCHAN PAGING_CMD with IEs.
static std::vector<uint8_t> makeCChanPaging(uint8_t chanNr, const std::vector<uint8_t>& ies) {
    std::vector<uint8_t> buf;
    buf.push_back(rslFirstOctet(RSLDiscriminator::CommonChannel, /*transparent=*/false));
    buf.push_back(static_cast<uint8_t>(RSLCChanMessageType::PagingCmd));
    buf.push_back(static_cast<uint8_t>(RSL_IE::ChanNr));
    buf.push_back(chanNr);
    buf.insert(buf.end(), ies.begin(), ies.end());
    return buf;
}

// Test: Parse RLL DATA_REQ and extract L3 payload.
// Importance: This is the primary BSC->BTS message carrying L3 data to forward to MS.
// 3GPP: TS 48.058 RLL DATA_REQ.
TEST(RSLP_parse_RLL_DataReq, ExtractsL3) {
    auto buf = makeRLLDataReq(0x7c, 1, {0x05, 0x24, 0x02}); // L3 CM Service Request (PD=MM low nibble, MT=0x24)
    auto result = RSLParser::parse(buf);
    ASSERT_TRUE(result.has_value());
    auto& msg = *result;
    EXPECT_EQ(msg.discriminator, RSLDiscriminator::Rll);
    EXPECT_EQ(msg.msgType, static_cast<uint8_t>(RSLL3MessageType::DataReq));
    EXPECT_EQ(msg.chanNr, 0x7c);
    EXPECT_EQ(msg.linkId, 1);
    EXPECT_TRUE(RSLParser::hasL3Payload(msg));
    auto l3 = RSLParser::extractL3(msg);
    ASSERT_TRUE(l3.has_value());
    EXPECT_EQ(l3->size(), 3u);
    EXPECT_EQ(l3->data()[0], 0x05);
    EXPECT_EQ(l3->data()[1], 0x24);
    EXPECT_EQ(l3->data()[2], 0x02);
}

// Test: Parse RLL DATA_IND and extract L3 payload.
// Importance: BTS->BSC direction for forwarding MS L3 messages to the BSC.
TEST(RSLP_parse_RLL_DataInd, ExtractsL3) {
    auto buf = makeRLLDataInd(0x7e, 3, {0x06, 0x0D, 0x01}); // L3 RR Channel Release (PD=RR low nibble)
    auto result = RSLParser::parse(buf);
    ASSERT_TRUE(result.has_value());
    auto& msg = *result;
    EXPECT_EQ(msg.discriminator, RSLDiscriminator::Rll);
    EXPECT_EQ(msg.msgType, static_cast<uint8_t>(RSLL3MessageType::DataInd));
    EXPECT_EQ(msg.chanNr, 0x7e);
    EXPECT_EQ(msg.linkId, 3);
    EXPECT_TRUE(RSLParser::hasL3Payload(msg));
}

// Test: Parse DCHAN CHAN_ACTIV with TLV IEs.
// Importance: Channel activation is the primary BSC->BTS control message for dedicated channels.
// 3GPP: TS 48.058 DCHAN CHAN_ACTIV.
TEST(RSLP_parse_DCHAN_ChanActiv, ParsesIEs) {
    // ChanMode IE: type=0x06, len=4, value=4 octets (TS 48.058 9.3.6)
    std::vector<uint8_t> ies = {
        0x03, 0x01, // ActType IE (TV): activation type = 1
        // ChanMode IE (LV): dtx=both, Speech, TCH/F, GSM1 speech algorithm.
        0x06, 0x04, 0x03, 0x01, 0x08, 0x01,
    };
    auto buf = makeDChanActiv(0x78, ies);
    auto result = RSLParser::parse(buf);
    ASSERT_TRUE(result.has_value());
    auto& msg = *result;
    EXPECT_EQ(msg.discriminator, RSLDiscriminator::DedicatedChannel);
    EXPECT_EQ(msg.msgType, static_cast<uint8_t>(RSLDChanMessageType::ChanActiv));
    EXPECT_EQ(msg.chanNr, 0x78);
    EXPECT_GE(msg.ieCount, 2u);

    auto* actType = RSLParser::findIE(msg, RSL_IE::ActType);
    ASSERT_NE(actType, nullptr);
    EXPECT_EQ(actType->type, static_cast<uint8_t>(RSL_IE::ActType));
    EXPECT_EQ(actType->len, 1u);

    auto* chanMode = RSLParser::findIE(msg, RSL_IE::ChanMode);
    ASSERT_NE(chanMode, nullptr);
    EXPECT_EQ(chanMode->len, 4u);
}

// Test: Parse DCHAN ENCR_CMD and extract L3 payload + encryption info.
// Importance: Encryption command carries both ciphering parameters and L3 CipheringModeCommand.
TEST(RSLP_parse_DCHAN_EncrCmd, ExtractsL3AndEncrInfo) {
    // Build ENCR_CMD with EncrInfo IE and L3Info IE (TL16V).
    std::vector<uint8_t> buf;
    buf.push_back(rslFirstOctet(RSLDiscriminator::DedicatedChannel, /*transparent=*/false));
    buf.push_back(static_cast<uint8_t>(RSLDChanMessageType::EncrCmd));
    buf.push_back(static_cast<uint8_t>(RSL_IE::ChanNr)); // Channel Number TV IE
    buf.push_back(0x7c);                                // chanNr
    // EncrInfo IE: type=0x07, len=9, algo=1 (A5/1), key=8 bytes
    buf.push_back(static_cast<uint8_t>(RSL_IE::EncrInfo)); // type
    buf.push_back(0x09);                                   // len
    buf.push_back(0x01);                                   // algo A5/1
    buf.insert(buf.end(), {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x11, 0x22}); // key
    // L3Info IE (TL16V): type=0x0B, len_hi=0, len_lo=4, value=4 bytes
    buf.push_back(static_cast<uint8_t>(RSL_IE::L3Info)); // type
    buf.push_back(0x00);                                 // len high
    buf.push_back(0x04);                                 // len low
    buf.insert(buf.end(), {0x06, 0x22, 0x01, 0x00});     // CipheringModeCommand L3 (PD=RR)

    auto result = RSLParser::parse(buf);
    ASSERT_TRUE(result.has_value());
    auto& msg = *result;
    EXPECT_TRUE(RSLParser::hasL3Payload(msg));
    auto l3 = RSLParser::extractL3(msg);
    ASSERT_TRUE(l3.has_value());
    EXPECT_EQ(l3->size(), 4u);
    EXPECT_EQ(l3->data()[0], 0x06);

    auto* encrIE = RSLParser::findIE(msg, RSL_IE::EncrInfo);
    ASSERT_NE(encrIE, nullptr);
}

// Test: Parse CCHAN PAGING_CMD and extract IEs.
// Importance: Paging is the primary mechanism for network-initiated MS contact.
TEST(RSLP_parse_CCHAN_PagingCmd, ParsesIEs) {
    // MSIdentity IE: type=0x0C, len=3, value=TMSI bytes
    std::vector<uint8_t> ies = {
        0x0C, 0x03, 0x12, 0x34, 0x56, // MSIdentity (LV)
        0x0E, 0x01,                   // PagingGroup (TV)
    };
    auto buf = makeCChanPaging(0x00, ies);
    auto result = RSLParser::parse(buf);
    ASSERT_TRUE(result.has_value());
    auto& msg = *result;
    EXPECT_EQ(msg.discriminator, RSLDiscriminator::CommonChannel);
    EXPECT_EQ(msg.msgType, static_cast<uint8_t>(RSLCChanMessageType::PagingCmd));

    auto* idIE = RSLParser::findIE(msg, RSL_IE::MSIdentity);
    ASSERT_NE(idIE, nullptr);
    EXPECT_EQ(idIE->len, 3u);
}

// Test: Parse CCHAN BCCH_INFO and extract L3 payload.
// Importance: BCCH_INFO carries system information broadcast to all MS in the cell.
TEST(RSLP_parse_CCHAN_BCCHInfo, ExtractsL3) {
    std::vector<uint8_t> buf;
    buf.push_back(rslFirstOctet(RSLDiscriminator::CommonChannel, /*transparent=*/false));
    buf.push_back(static_cast<uint8_t>(RSLCChanMessageType::BcchInfo));
    buf.push_back(static_cast<uint8_t>(RSL_IE::ChanNr)); // Channel Number TV IE
    buf.push_back(RSLChannelNumber::encode(RSLChannelNumber::Bcch, 0)); // BCCH, TN 0 -> 0x80
    // L3Info IE (TL16V): type=0x0B, len=0x0006, value=6 bytes of SI
    buf.push_back(static_cast<uint8_t>(RSL_IE::L3Info));
    buf.push_back(0x00);
    buf.push_back(0x06);
    buf.insert(buf.end(), {0x0b, 0x48, 0x01, 0xaa, 0xbb, 0xcc});

    auto result = RSLParser::parse(buf);
    ASSERT_TRUE(result.has_value());
    auto& msg = *result;
    EXPECT_TRUE(RSLParser::hasL3Payload(msg));
    auto l3 = RSLParser::extractL3(msg);
    ASSERT_TRUE(l3.has_value());
    EXPECT_EQ(l3->size(), 6u);
}

// Test: Parsing a message shorter than the two header octets returns an error.
// Importance: Defensive parsing prevents buffer overread on malformed input.
TEST(RSLP_parse_ShortMessage, ReturnsError) {
    std::vector<uint8_t> shortMsg = {0x03}; // First octet only; the global message type is missing
    auto result = RSLParser::parse(shortMsg);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ParseError::Code::TruncatedInput);
}

// Test: A frame whose first-octet group is reserved (0) is rejected.
// Importance: TS 48.058 9.1 reserves message group 0; accepting it would
// misroute frames from other A-bis protocols.
TEST(RSLP_parse_ReservedGroup, ReturnsError) {
    std::vector<uint8_t> buf = {0x00, 0x01}; // group 0 (reserved), any message type
    auto result = RSLParser::parse(buf);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ParseError::Code::InvalidValue);
}

// Test: A frame with a message group outside the defined set is rejected.
TEST(RSLP_parse_UnknownGroup, ReturnsError) {
    std::vector<uint8_t> buf = {0x05, 0x01}; // group 2: no such RSL message group (TS 48.058 9.1)
    auto result = RSLParser::parse(buf);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ParseError::Code::InvalidValue);
}

// Test: Parsing empty input returns an error.
TEST(RSLP_parse_EmptyMessage, ReturnsError) {
    std::vector<uint8_t> empty;
    auto result = RSLParser::parse(empty);
    ASSERT_FALSE(result.has_value());
}

// Test: Truncated TLV value returns partial parse (does not crash).
// Importance: Graceful degradation on malformed messages from buggy BSC implementations.
TEST(RSLP_parse_TruncatedTLV, PartialParse_NoCrash) {
    // Header + IE type + length claiming 100 bytes but only 5 available.
    std::vector<uint8_t> buf = {
        0x08, 0x21, 0x01, 0x78, // DCHAN CHAN_ACTIV: group + type + Channel Number TV IE
        0x06, 0x64, 0x00, 0x01, 0x02, 0x03, 0x04 // ChanMode (LV) claims 100 bytes, only 5 value bytes present
    };
    auto result = RSLParser::parse(buf);
    ASSERT_TRUE(result.has_value()); // Header parsed, truncated TLV stops parsing gracefully
    auto& msg = *result;
    // Parser should not crash. Truncated IE may or may not be counted depending on encoding detection.
    // The key requirement: parser handles truncated data without UB.
    EXPECT_LE(msg.ieCount, RSLParsedMessage::MAX_IE);
}

// Test: findIE returns pointer for existing IE.
TEST(RSLP_findIE_Existing, Found) {
    std::vector<uint8_t> ies = {
        0x03, 0x01, // ActType (TV): value=1
    };
    auto buf = makeDChanActiv(0x78, ies);
    auto result = RSLParser::parse(buf);
    ASSERT_TRUE(result.has_value());
    auto* ie = RSLParser::findIE(*result, RSL_IE::ActType);
    ASSERT_NE(ie, nullptr);
    EXPECT_EQ(ie->type, static_cast<uint8_t>(RSL_IE::ActType));
}

// Test: findIE returns nullptr for non-existing IE.
TEST(RSLP_findIE_NonExisting, Nullptr) {
    std::vector<uint8_t> ies = {
        0x03, 0x01, // ActType only
    };
    auto buf = makeDChanActiv(0x78, ies);
    auto result = RSLParser::parse(buf);
    ASSERT_TRUE(result.has_value());
    auto* ie = RSLParser::findIE(*result, RSL_IE::EncrInfo);
    EXPECT_EQ(ie, nullptr);
}

// Test: getChannelMode extracts valid ChannelMode from CHAN_ACTIV.
// Golden (TS 48.058 9.3.6): SDCCH signalling channel without DTX — the four
// value octets are dtx=0x00, speed indicator Signalling (0x03), channel rate
// and type SDCCH (0x01), and the no-resource union octet 0x00.
TEST(RSLP_getChannelMode_Valid, ReturnsMode) {
    std::vector<uint8_t> ies = {
        0x06, 0x04, 0x00, 0x03, 0x01, 0x00, // ChanMode: SDCCH signalling, DTX off
    };
    auto buf = makeDChanActiv(0x78, ies);
    auto result = RSLParser::parse(buf);
    ASSERT_TRUE(result.has_value());
    auto mode = RSLParser::getChannelMode(*result);
    ASSERT_TRUE(mode.has_value());
    EXPECT_TRUE(mode->isSignalling());
    EXPECT_FALSE(mode->isSpeech());
    EXPECT_EQ(mode->spdInd, static_cast<uint8_t>(RSLChannelMode::SpeedIndicator::Signalling));
    EXPECT_EQ(mode->chanRateType, static_cast<uint8_t>(RSLChannelMode::ChanRateType::Sdcch));
    EXPECT_EQ(mode->valueOctet, 0x00u);
    EXPECT_FALSE(mode->dtxDownlink());
    EXPECT_FALSE(mode->dtxUplink());
}

// Test: a Channel Mode IE value of the wrong length is rejected (the coding is
// exactly four octets, TS 48.058 9.3.6).
TEST(RSLP_getChannelMode_BadLength, Nullopt) {
    // Five-octet value: not the defined Channel Mode IE coding.
    std::vector<uint8_t> ies = {
        0x06, 0x05, 0x00, 0x03, 0x01, 0x00, 0x00,
    };
    auto buf = makeDChanActiv(0x78, ies);
    auto result = RSLParser::parse(buf);
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(RSLParser::getChannelMode(*result).has_value());

    // Truncated three-octet value: also rejected.
    std::vector<uint8_t> iesShort = {
        0x06, 0x03, 0x00, 0x03, 0x01,
    };
    auto bufShort = makeDChanActiv(0x78, iesShort);
    auto resultShort = RSLParser::parse(bufShort);
    ASSERT_TRUE(resultShort.has_value());
    EXPECT_FALSE(RSLParser::getChannelMode(*resultShort).has_value());
}

// Golden: Uplink Measurements IE value (TS 48.058 9.3.25) decodes the three
// canonical octets — RX level full=40, sub=35, RX quality full=5, sub=6, DTX
// downlink clear: [00|101000]=0x28, [00|100011]=0x23, [00|101|110]=0x2E.
TEST(RSLP_getUplinkMeas_RefVector, Decoded) {
    std::vector<uint8_t> ies = {
        0x1B, 0x07,                       // MeasResNr IE (TV): sequence number 7
        0x19, 0x03, 0x28, 0x23, 0x2E,     // UplinkMeas IE (LV): three value octets
    };
    auto buf = makeDChanActiv(0x78, ies);
    auto result = RSLParser::parse(buf);
    ASSERT_TRUE(result.has_value());

    auto meas = RSLParser::getUplinkMeas(*result);
    ASSERT_TRUE(meas.has_value());
    EXPECT_FALSE(meas->dtxDownlink);
    EXPECT_EQ(meas->rxlevFull, 40u);
    EXPECT_EQ(meas->rxlevSub, 35u);
    EXPECT_EQ(meas->rxqFull, 5u);
    EXPECT_EQ(meas->rxqSub, 6u);
}

// Test: the DTX downlink indicator bit and a vendor-extended (longer) Uplink
// Measurements value are handled by getUplinkMeas (TS 48.058 9.3.25): the
// first three octets decode, the appended supplementary bytes stay in the IE
// storage.
TEST(RSLP_getUplinkMeas_DtxAndVendorTail, Decoded) {
    std::vector<uint8_t> ies = {
        0x19, 0x0B, 0x6A, 0x23, 0x2E, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00, 0x01,
        // octet 0x6A = [0|1(DTX_d)|101010] -> DTX downlink set, RX level full 42
    };
    auto buf = makeDChanActiv(0x78, ies);
    auto result = RSLParser::parse(buf);
    ASSERT_TRUE(result.has_value());

    auto* ie = RSLParser::findIE(*result, RSL_IE::UplinkMeas);
    ASSERT_NE(ie, nullptr);
    EXPECT_EQ(ie->len, 11u); // 3 value octets + 8 vendor supplementary bytes

    auto meas = RSLParser::getUplinkMeas(*result);
    ASSERT_TRUE(meas.has_value());
    EXPECT_TRUE(meas->dtxDownlink);
    EXPECT_EQ(meas->rxlevFull, 42u);
    EXPECT_EQ(meas->rxlevSub, 35u);
    EXPECT_EQ(meas->rxqFull, 5u);
    EXPECT_EQ(meas->rxqSub, 6u);
}

// Golden: Frame Number IE value (TS 48.058 9.3.8) — the canonical absolute TDMA
// frame number 207 decomposes into t1p=0, t3=3, t2=25, which pack to the two
// octets {0x00, 0x79}: (0<<3)|(3>>3)=0x00, ((3&7)<<5)|25=0x79.
TEST(RSLP_getFrameNumber_RefVector, Decoded) {
    std::vector<uint8_t> ies = {
        0x08, 0x00, 0x79, // FrameNumber IE (TV): two value octets
    };
    auto buf = makeDChanActiv(0x78, ies);
    auto result = RSLParser::parse(buf);
    ASSERT_TRUE(result.has_value());

    auto fn = RSLParser::getFrameNumber(*result);
    ASSERT_TRUE(fn.has_value());
    EXPECT_EQ(fn->t1p, 0u);
    EXPECT_EQ(fn->t3, 3u);
    EXPECT_EQ(fn->t2, 25u);
}

// Test: getEncryptionInfo extracts algorithm ID and key from ENCR_CMD.
TEST(RSLP_getEncryptionInfo_Valid, ReturnsInfo) {
    std::vector<uint8_t> buf;
    buf.push_back(rslFirstOctet(RSLDiscriminator::DedicatedChannel, /*transparent=*/false));
    buf.push_back(static_cast<uint8_t>(RSLDChanMessageType::EncrCmd));
    buf.push_back(static_cast<uint8_t>(RSL_IE::ChanNr)); // Channel Number TV IE
    buf.push_back(0x7c);
    // EncrInfo: type=0x07, len=9, algo=1, key=8 bytes
    buf.push_back(static_cast<uint8_t>(RSL_IE::EncrInfo));
    buf.push_back(0x09);
    buf.push_back(0x01);
    buf.insert(buf.end(), {0xde, 0xad, 0xbe, 0xef, 0xca, 0xfe, 0xba, 0xbe});

    auto result = RSLParser::parse(buf);
    ASSERT_TRUE(result.has_value());
    auto info = RSLParser::getEncryptionInfo(*result);
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->algorithmId, 1u); // A5/1
    EXPECT_EQ(info->key.size(), 8u);
    EXPECT_EQ(info->key[0], 0xde);
}

// Test: messageName returns recognizable strings for known global message
// types (TS 48.058 Table 8.x — one type space per frame, not per group).
TEST(RSLP_messageName, KnownTypes) {
    EXPECT_EQ(RSLParser::messageName(RSLDiscriminator::Rll, 0x01), "DATA_REQ");
    EXPECT_EQ(RSLParser::messageName(RSLDiscriminator::Rll, 0x02), "DATA_IND");
    EXPECT_EQ(RSLParser::messageName(RSLDiscriminator::DedicatedChannel, 0x21), "CHAN_ACTIV");
    EXPECT_EQ(RSLParser::messageName(RSLDiscriminator::DedicatedChannel, 0x22), "CHAN_ACTIV_ACK");
    EXPECT_EQ(RSLParser::messageName(RSLDiscriminator::CommonChannel, 0x15), "PAGING_CMD");
    EXPECT_EQ(RSLParser::messageName(RSLDiscriminator::CommonChannel, 0x13), "CHAN_RQD");
    EXPECT_EQ(RSLParser::messageName(RSLDiscriminator::Rll, 0xff), "UNKNOWN");
}

// Test: TL16V IEs longer than 255 bytes keep their full length in the IE
// descriptor and in the extracted L3 payload.
// Importance: L3Info/FullBCCHInfo carry complete L3 messages; a uint8_t
// length field truncated payloads above 255 bytes.
// 3GPP: TS 48.058 9.2.25 (FULL_BCCH_INFO), 9.2.30 (L3_INFO).
TEST(RSLP_parse_CCHAN_L3Info, Over255Bytes_FullLengthKept) {
    // CCHAN BCCH_INFO: first octet (CCHAN group 0x06 << 1 = 0x0C) + type(0x11)
    // + Channel Number TV IE + L3Info IE: type(0x0B) + len(2, big-endian = 300)
    // + value(300 bytes).
    std::vector<uint8_t> raw;
    raw.push_back(0x0C);
    raw.push_back(static_cast<uint8_t>(RSLCChanMessageType::BcchInfo));
    raw.push_back(static_cast<uint8_t>(RSL_IE::ChanNr));
    raw.push_back(0x00);
    raw.push_back(static_cast<uint8_t>(RSL_IE::L3Info));
    raw.push_back(0x01); // 0x012C = 300
    raw.push_back(0x2C);
    for (int i = 0; i < 300; ++i) raw.push_back(static_cast<uint8_t>(i & 0xFF));

    auto result = RSLParser::parse(raw);
    ASSERT_TRUE(result.has_value());
    auto* ie = RSLParser::findIE(*result, RSL_IE::L3Info);
    ASSERT_NE(ie, nullptr);
    EXPECT_EQ(ie->len, 300u);
    auto l3 = RSLParser::extractL3(*result);
    ASSERT_TRUE(l3.has_value());
    EXPECT_EQ(l3->size(), 300u);
    EXPECT_EQ((*l3)[299], static_cast<uint8_t>(299 & 0xFF));
}

// Test: FullBCCHInfo (LV, TS 48.058 9.3.x) payloads are extracted in full via
// the l3Payload fallback path at the maximum 8-bit length of 255 bytes.
TEST(RSLP_parse_DCHAN_FullBCCHInfo, LvPayloadExtracted) {
    // DCHAN message (no L3Info IE) carrying FullBCCHInfo:
    // first octet (DCHAN group 0x04 << 1 = 0x08) + type + Channel Number TV IE
    // + type(0x27) + len(1) + value(255).
    std::vector<uint8_t> raw;
    raw.push_back(0x08);
    raw.push_back(static_cast<uint8_t>(RSLDChanMessageType::RfChanRel));
    raw.push_back(static_cast<uint8_t>(RSL_IE::ChanNr));
    raw.push_back(0x7c);
    raw.push_back(static_cast<uint8_t>(RSL_IE::FullBCCHInfo));
    raw.push_back(0xFF); // LV length = 255
    for (int i = 0; i < 255; ++i) raw.push_back(static_cast<uint8_t>(i & 0xFF));

    auto result = RSLParser::parse(raw);
    ASSERT_TRUE(result.has_value());
    auto* ie = RSLParser::findIE(*result, RSL_IE::FullBCCHInfo);
    ASSERT_NE(ie, nullptr);
    EXPECT_EQ(ie->len, 255u);
    auto l3 = RSLParser::extractL3(*result);
    ASSERT_TRUE(l3.has_value());
    EXPECT_EQ(l3->size(), 255u);
}

// Test: the transparent indication flag (bit 0 of the first octet,
// TS 48.058 9.1) is accepted for every group and reported.
TEST(RSLP_parse_TransparentFlag, AcceptedAndReported) {
    // DCHAN CHAN_ACTIV_ACK with the transparent flag set: 0x09 = (0x04 << 1) | 1.
    std::vector<uint8_t> buf = {0x09, static_cast<uint8_t>(RSLDChanMessageType::ChanActivAck),
                                static_cast<uint8_t>(RSL_IE::ChanNr), 0x78};
    auto result = RSLParser::parse(buf);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ((*result).discriminator, RSLDiscriminator::DedicatedChannel);
    EXPECT_TRUE((*result).transparent);

    // CCHAN CCCH_LOAD_IND with the transparent flag set: 0x0D = (0x06 << 1) | 1.
    std::vector<uint8_t> cbuf = {0x0D, static_cast<uint8_t>(RSLCChanMessageType::CcchLoadInd),
                                 static_cast<uint8_t>(RSL_IE::ChanNr), 0x00};
    auto cresult = RSLParser::parse(cbuf);
    ASSERT_TRUE(cresult.has_value());
    EXPECT_EQ((*cresult).discriminator, RSLDiscriminator::CommonChannel);
    EXPECT_TRUE((*cresult).transparent);

    // RLL DATA_IND (transparent): 0x03 = (0x01 << 1) | 1.
    // L3 is wrapped in an L3Info IE (type 0x0B, TL16V).
    std::vector<uint8_t> rbuf = {0x03, static_cast<uint8_t>(RSLL3MessageType::DataInd),
                                 static_cast<uint8_t>(RSL_IE::ChanNr), 0x7e,
                                 static_cast<uint8_t>(RSL_IE::LinkIdent), 0x03,
                                 0x0B, 0x00, 0x03, 0x06, 0x0D, 0x01};
    auto rresult = RSLParser::parse(rbuf);
    ASSERT_TRUE(rresult.has_value());
    EXPECT_EQ((*rresult).discriminator, RSLDiscriminator::Rll);
    EXPECT_TRUE((*rresult).transparent);
    EXPECT_TRUE(RSLParser::hasL3Payload(*rresult));

    // Flag clear (non-transparent control frame) reports false.
    std::vector<uint8_t> bbuf = {0x08, static_cast<uint8_t>(RSLDChanMessageType::ChanActiv),
                                 static_cast<uint8_t>(RSL_IE::ChanNr), 0x78};
    auto bresult = RSLParser::parse(bbuf);
    ASSERT_TRUE(bresult.has_value());
    EXPECT_FALSE((*bresult).transparent);
}

// Golden: RLL DATA_REQ frame shape (TS 48.058 8.3/9.x): first octet =
// group RLL (0x01) << 1 | transparent, global message type 0x01, then the TV
// Channel Number and Link Identifier IEs and a TL16V L3 Information IE.
TEST(RSLP_parse_RllDataReq_RefShape, ParsedPerIeList) {
    uint8_t buf[] = {0x03, 0x01,
                     static_cast<uint8_t>(RSL_IE::ChanNr), 0x08, // Bm_ACCH (code 1), TN 0
                     static_cast<uint8_t>(RSL_IE::LinkIdent), 0x00,
                     0x0B, 0x00, 0x03, 'A', 'B', 'C'};           // L3Info TL16V, len=3
    auto res = RSLParser::parse(std::span<const uint8_t>(buf));
    ASSERT_TRUE(res.has_value());
    const auto& parsed = *res;
    EXPECT_EQ(parsed.discriminator, RSLDiscriminator::Rll);
    EXPECT_TRUE(parsed.transparent);
    EXPECT_EQ(parsed.msgType, static_cast<uint8_t>(RSLL3MessageType::DataReq));
    EXPECT_EQ(parsed.chanNr, 0x08u);
    EXPECT_EQ(parsed.linkId, 0x00u);

    // L3 IE: 0x0B with 16-bit length 3.
    auto* l3IE = RSLParser::findIE(parsed, RSL_IE::L3Info);
    ASSERT_NE(l3IE, nullptr);
    EXPECT_EQ(l3IE->type, static_cast<uint8_t>(RSL_IE::L3Info));
    EXPECT_EQ(l3IE->len, 3u);
    auto l3 = RSLParser::extractL3(*res);
    ASSERT_TRUE(l3.has_value());
    EXPECT_EQ((*l3)[0], 'A');
    EXPECT_EQ((*l3)[2], 'C');
}
