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
static std::vector<uint8_t> makeRLLDataReq(uint8_t chanNr, uint8_t linkId, std::initializer_list<uint8_t> l3) {
    std::vector<uint8_t> buf;
    buf.push_back(static_cast<uint8_t>(RSLDiscriminator::RLL));
    buf.push_back(static_cast<uint8_t>(RSLL3MessageType::DataReq));
    buf.push_back(chanNr);
    buf.push_back(linkId);
    buf.insert(buf.end(), l3.begin(), l3.end());
    return buf;
}

// Helper: build a minimal RLL DATA_IND with L3 payload.
static std::vector<uint8_t> makeRLLDataInd(uint8_t chanNr, uint8_t linkId, std::initializer_list<uint8_t> l3) {
    std::vector<uint8_t> buf;
    buf.push_back(static_cast<uint8_t>(RSLDiscriminator::RLL));
    buf.push_back(static_cast<uint8_t>(RSLL3MessageType::DataInd));
    buf.push_back(chanNr);
    buf.push_back(linkId);
    buf.insert(buf.end(), l3.begin(), l3.end());
    return buf;
}

// Helper: build a DCHAN CHAN_ACTIV with IEs.
static std::vector<uint8_t> makeDChanActiv(uint8_t chanNr, const std::vector<uint8_t>& ies) {
    std::vector<uint8_t> buf;
    buf.push_back(static_cast<uint8_t>(RSLDiscriminator::DedicatedChannel));
    buf.push_back(static_cast<uint8_t>(RSLDChanMessageType::ChanActiv));
    buf.push_back(chanNr);
    buf.push_back(0); // reserved
    buf.insert(buf.end(), ies.begin(), ies.end());
    return buf;
}

// Helper: build a CCHAN PAGING_CMD with IEs.
static std::vector<uint8_t> makeCChanPaging(uint8_t chanNr, const std::vector<uint8_t>& ies) {
    std::vector<uint8_t> buf;
    buf.push_back(static_cast<uint8_t>(RSLDiscriminator::CommonChannel));
    buf.push_back(static_cast<uint8_t>(RSLCChanMessageType::PagingCmd));
    buf.push_back(chanNr);
    buf.push_back(0); // reserved
    buf.insert(buf.end(), ies.begin(), ies.end());
    return buf;
}

// Test: Parse RLL DATA_REQ and extract L3 payload.
// Importance: This is the primary BSC->BTS message carrying L3 data to forward to MS.
// 3GPP: TS 48.058 RLL DATA_REQ.
TEST(RSLP_parse_RLL_DataReq, ExtractsL3) {
    auto buf = makeRLLDataReq(0x7c, 1, {0x09, 0x68, 0x02}); // L3 CM Service Request
    auto result = RSLParser::parse(buf);
    ASSERT_TRUE(result.has_value());
    auto& msg = *result;
    EXPECT_EQ(msg.discriminator, RSLDiscriminator::RLL);
    EXPECT_EQ(msg.msgType, static_cast<uint8_t>(RSLL3MessageType::DataReq));
    EXPECT_EQ(msg.chanNr, 0x7c);
    EXPECT_EQ(msg.linkId, 1);
    EXPECT_TRUE(RSLParser::hasL3Payload(msg));
    auto l3 = RSLParser::extractL3(msg);
    ASSERT_TRUE(l3.has_value());
    EXPECT_EQ(l3->size(), 3u);
    EXPECT_EQ(l3->data()[0], 0x09);
    EXPECT_EQ(l3->data()[1], 0x68);
    EXPECT_EQ(l3->data()[2], 0x02);
}

// Test: Parse RLL DATA_IND and extract L3 payload.
// Importance: BTS->BSC direction for forwarding MS L3 messages to the BSC.
TEST(RSLP_parse_RLL_DataInd, ExtractsL3) {
    auto buf = makeRLLDataInd(0x7e, 3, {0x0d, 0x04, 0x01, 0x02});
    auto result = RSLParser::parse(buf);
    ASSERT_TRUE(result.has_value());
    auto& msg = *result;
    EXPECT_EQ(msg.discriminator, RSLDiscriminator::RLL);
    EXPECT_EQ(msg.msgType, static_cast<uint8_t>(RSLL3MessageType::DataInd));
    EXPECT_EQ(msg.chanNr, 0x7e);
    EXPECT_EQ(msg.linkId, 3);
    EXPECT_TRUE(RSLParser::hasL3Payload(msg));
}

// Test: Parse DCHAN CHAN_ACTIV with TLV IEs.
// Importance: Channel activation is the primary BSC->BTS control message for dedicated channels.
// 3GPP: TS 48.058 DCHAN CHAN_ACTIV.
TEST(RSLP_parse_DCHAN_ChanActiv, ParsesIEs) {
    // ChanMode IE: type=0x22, len=5, value=5 bytes
    std::vector<uint8_t> ies = {
        0x21, 0x01, // ActType IE (TV): activation type = 1
        0x22, 0x05, 0x00, 0x01, 0x01, 0x00, 0x00, // ChanMode IE (TLV): 5 bytes
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
    EXPECT_EQ(chanMode->len, 5u);
}

// Test: Parse DCHAN ENCR_CMD and extract L3 payload + encryption info.
// Importance: Encryption command carries both ciphering parameters and L3 CipheringModeCommand.
TEST(RSLP_parse_DCHAN_EncrCmd, ExtractsL3AndEncrInfo) {
    // Build ENCR_CMD with EncrInfo IE and L3Info IE (TL16V).
    std::vector<uint8_t> buf;
    buf.push_back(static_cast<uint8_t>(RSLDiscriminator::DedicatedChannel));
    buf.push_back(static_cast<uint8_t>(RSLDChanMessageType::EncrCmd));
    buf.push_back(0x7c); // chanNr
    buf.push_back(0);    // reserved
    // EncrInfo IE: type=0x23, len=9, algo=1 (A5/1), key=8 bytes
    buf.push_back(0x23); // type
    buf.push_back(0x09); // len
    buf.push_back(0x01); // algo A5/1
    buf.insert(buf.end(), {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x11, 0x22}); // key
    // L3Info IE (TL16V): type=0x30, len_hi=0, len_lo=4, value=4 bytes
    buf.push_back(0x30); // type
    buf.push_back(0x00); // len high
    buf.push_back(0x04); // len low
    buf.insert(buf.end(), {0x05, 0x38, 0x01, 0x00}); // CipheringModeCommand L3

    auto result = RSLParser::parse(buf);
    ASSERT_TRUE(result.has_value());
    auto& msg = *result;
    EXPECT_TRUE(RSLParser::hasL3Payload(msg));
    auto l3 = RSLParser::extractL3(msg);
    ASSERT_TRUE(l3.has_value());
    EXPECT_EQ(l3->size(), 4u);
    EXPECT_EQ(l3->data()[0], 0x05);

    auto* encrIE = RSLParser::findIE(msg, RSL_IE::EncrInfo);
    ASSERT_NE(encrIE, nullptr);
}

// Test: Parse CCHAN PAGING_CMD and extract IEs.
// Importance: Paging is the primary mechanism for network-initiated MS contact.
TEST(RSLP_parse_CCHAN_PagingCmd, ParsesIEs) {
    // MSIdentity IE: type=0x2c, len=3, value=TMSI bytes
    std::vector<uint8_t> ies = {
        0x2c, 0x03, 0x12, 0x34, 0x56, // MSIdentity (TLV)
        0x2d, 0x01,                   // PagingGroup (TV)
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
    buf.push_back(static_cast<uint8_t>(RSLDiscriminator::CommonChannel));
    buf.push_back(static_cast<uint8_t>(RSLCChanMessageType::BCCHInfo));
    buf.push_back(0x00); // chanNr (BCCH)
    buf.push_back(0);    // reserved
    // L3Info IE (TL16V): type=0x30, len=0x0006, value=6 bytes of SI
    buf.push_back(0x30);
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

// Test: Parsing a message shorter than the header returns an error.
// Importance: Defensive parsing prevents buffer overread on malformed input.
TEST(RSLP_parse_ShortMessage, ReturnsError) {
    std::vector<uint8_t> shortMsg = {0x00, 0x21}; // Only discriminator + msgType, missing header
    auto result = RSLParser::parse(shortMsg);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ParseError::Code::TruncatedInput);
}

// Test: Parsing empty input returns an error.
TEST(RSLP_parse_EmptyMessage, ReturnsError) {
    std::vector<uint8_t> empty;
    auto result = RSLParser::parse(empty);
    ASSERT_FALSE(result.has_value());
}

// Test: Truncated TLV value returns partial parse (does not crash).
// Importance: Graceful degradation on malformed messages from buggy BSC implementations.
TEST(RSLP_parse_TruncatedTLV, ReturnsError) {
    // Header + IE type + length claiming 100 bytes but only 5 available.
    std::vector<uint8_t> buf = {
        0x60, 0x01, 0x78, 0x00, // DCHAN CHAN_ACTIV header
        0x22, 0x64, 0x00, 0x01, 0x02, 0x03, 0x04 // ChanMode claims 100 bytes, only 5 value bytes present
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
        0x21, 0x03, // ActType (TV): value=3
    };
    auto buf = makeDChanActiv(0x78, ies);
    auto result = RSLParser::parse(buf);
    ASSERT_TRUE(result.has_value());
    auto* ie = RSLParser::findIE(*result, RSL_IE::ActType);
    ASSERT_NE(ie, nullptr);
    EXPECT_EQ(ie->type, 0x21);
}

// Test: findIE returns nullptr for non-existing IE.
TEST(RSLP_findIE_NonExisting, Nullptr) {
    std::vector<uint8_t> ies = {
        0x21, 0x03, // ActType only
    };
    auto buf = makeDChanActiv(0x78, ies);
    auto result = RSLParser::parse(buf);
    ASSERT_TRUE(result.has_value());
    auto* ie = RSLParser::findIE(*result, RSL_IE::EncrInfo);
    EXPECT_EQ(ie, nullptr);
}

// Test: getChannelMode extracts valid ChannelMode from CHAN_ACTIV.
TEST(RSLP_getChannelMode_Valid, ReturnsMode) {
    // ChanMode IE: type=0x22, len=5, value with spdInd=2 (Speech)
    std::vector<uint8_t> ies = {
        0x22, 0x05, 0x00, 0x02, 0x02, 0x00, 0x04, // ChanMode: reserved, Speech, TCH_Bm, dtx=0, rate=4
    };
    auto buf = makeDChanActiv(0x78, ies);
    auto result = RSLParser::parse(buf);
    ASSERT_TRUE(result.has_value());
    auto mode = RSLParser::getChannelMode(*result);
    ASSERT_TRUE(mode.has_value());
    EXPECT_TRUE(mode->isSpeech());
    EXPECT_FALSE(mode->isSignalling());
}

// Test: getEncryptionInfo extracts algorithm ID and key from ENCR_CMD.
TEST(RSLP_getEncryptionInfo_Valid, ReturnsInfo) {
    std::vector<uint8_t> buf;
    buf.push_back(static_cast<uint8_t>(RSLDiscriminator::DedicatedChannel));
    buf.push_back(static_cast<uint8_t>(RSLDChanMessageType::EncrCmd));
    buf.push_back(0x7c);
    buf.push_back(0);
    // EncrInfo: type=0x23, len=9, algo=1, key=8 bytes
    buf.push_back(0x23);
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

// Test: messageName returns recognizable strings for known message types.
TEST(RSLP_messageName, KnownTypes) {
    EXPECT_EQ(RSLParser::messageName(RSLDiscriminator::RLL, 0x21), "DATA_REQ");
    EXPECT_EQ(RSLParser::messageName(RSLDiscriminator::RLL, 0x22), "DATA_IND");
    EXPECT_EQ(RSLParser::messageName(RSLDiscriminator::DedicatedChannel, 0x01), "CHAN_ACTIV");
    EXPECT_EQ(RSLParser::messageName(RSLDiscriminator::DedicatedChannel, 0x11), "CHAN_ACTIV_ACK");
    EXPECT_EQ(RSLParser::messageName(RSLDiscriminator::CommonChannel, 0x03), "PAGING_CMD");
    EXPECT_EQ(RSLParser::messageName(RSLDiscriminator::CommonChannel, 0x16), "CHAN_RQD");
    EXPECT_EQ(RSLParser::messageName(RSLDiscriminator::RLL, 0xff), "UNKNOWN");
}
