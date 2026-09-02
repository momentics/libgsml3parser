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

// Tests for RSLBuilder: validates build->parse round-trip for all message types,
// span overload correctness, and proper encoding of TLV information elements.
// 3GPP coverage: TS 48.058 (A-bis RSL), GSM 04.08 (L3 encapsulation in RSL).

#include <array>
#include <gtest/gtest.h>
#include <vector>
#include "gsml3parser/abis/rsl_builder.h"
#include "gsml3parser/abis/rsl_parser.h"

using namespace gsml3parser;

// Test: Build DATA_REQ with L3 payload and parse it back.
// Importance: Round-trip validates that built messages are parseable by RSLParser.
// 3GPP: TS 48.058 RLL DATA_REQ encoding.
TEST(RSLB_buildDataReq_L3Payload, ParsesBack) {
    std::vector<uint8_t> l3 = {0x09, 0x68, 0x02}; // CM Service Request
    auto result = RSLBuilder::buildDataReq(0x7c, 1, l3);
    ASSERT_TRUE(result.has_value());
    auto parsed = RSLParser::parse(*result);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ((*parsed).discriminator, RSLDiscriminator::RLL);
    EXPECT_EQ((*parsed).msgType, static_cast<uint8_t>(RSLL3MessageType::DataReq));
    EXPECT_EQ((*parsed).chanNr, 0x7c);
    EXPECT_EQ((*parsed).linkId, 1);
    auto l3Out = RSLParser::extractL3(*parsed);
    ASSERT_TRUE(l3Out.has_value());
    EXPECT_EQ(l3Out->size(), l3.size());
    EXPECT_EQ(std::memcmp(l3Out->data(), l3.data(), l3.size()), 0);
}

// Test: Build DATA_IND and verify round-trip.
TEST(RSLB_buildDataInd_EncodeDecode, RoundTrip) {
    std::vector<uint8_t> l3 = {0x0d, 0x04, 0x01}; // Channel Release
    auto result = RSLBuilder::buildDataInd(0x7e, 5, l3);
    ASSERT_TRUE(result.has_value());
    auto parsed = RSLParser::parse(*result);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ((*parsed).msgType, static_cast<uint8_t>(RSLL3MessageType::DataInd));
    EXPECT_EQ((*parsed).chanNr, 0x7e);
    EXPECT_EQ((*parsed).linkId, 5);
}

// Test: Build CHAN_ACTIV_ACK with frame number and parse back.
TEST(RSLB_buildChanActivAck_FrameNumber, ParsesBack) {
    auto result = RSLBuilder::buildChanActivAck(0x78, 0x1234);
    ASSERT_TRUE(result.has_value());
    auto parsed = RSLParser::parse(*result);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ((*parsed).discriminator, RSLDiscriminator::DedicatedChannel);
    EXPECT_EQ((*parsed).msgType, static_cast<uint8_t>(RSLDChanMessageType::ChanActivAck));
    EXPECT_EQ((*parsed).chanNr, 0x78);

    auto* fnIE = RSLParser::findIE(*parsed, RSL_IE::FrameNumber);
    ASSERT_NE(fnIE, nullptr);
    EXPECT_EQ(fnIE->len, 2u);
    // Frame number is big-endian: 0x12, 0x34
    EXPECT_EQ(fnIE->val[0], 0x12);
    EXPECT_EQ(fnIE->val[1], 0x34);
}

// Test: Build CHAN_ACTIV_NACK with cause and parse back.
TEST(RSLB_buildChanActivNack_Cause, ParsesBack) {
    auto result = RSLBuilder::buildChanActivNack(0x78, RSLErrorCause::ResourceUnavailable);
    ASSERT_TRUE(result.has_value());
    auto parsed = RSLParser::parse(*result);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ((*parsed).msgType, static_cast<uint8_t>(RSLDChanMessageType::ChanActivNack));

    auto* causeIE = RSLParser::findIE(*parsed, RSL_IE::Cause);
    ASSERT_NE(causeIE, nullptr);
    EXPECT_EQ(causeIE->len, 1u);
    EXPECT_EQ(causeIE->val[0], static_cast<uint8_t>(RSLErrorCause::ResourceUnavailable));
}

// Test: Build MEAS_RES with RXLEV/RXQUAL and parse back.
TEST(RSLB_buildMeasRes_RXLEV_RXQUAL, ParsesBack) {
    std::vector<uint8_t> l1Info = {0x01, 0x02};
    auto result = RSLBuilder::buildMeasRes(0x7c, 5, -45, 3, l1Info);
    ASSERT_TRUE(result.has_value());
    auto parsed = RSLParser::parse(*result);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ((*parsed).msgType, static_cast<uint8_t>(RSLDChanMessageType::MeasRes));

    auto* measNrIE = RSLParser::findIE(*parsed, RSL_IE::MeasResNr);
    ASSERT_NE(measNrIE, nullptr);
    EXPECT_EQ(measNrIE->len, 1u);
    EXPECT_EQ(measNrIE->val[0], 5u);

    auto* uplinkIE = RSLParser::findIE(*parsed, RSL_IE::UplinkMeas);
    ASSERT_NE(uplinkIE, nullptr);
    EXPECT_EQ(uplinkIE->len, 3u);
}

// Test: Build CCCH_LOAD_IND and parse back.
TEST(RSLB_buildCCCHLoadInd_Loads, ParsesBack) {
    auto result = RSLBuilder::buildCCCHLoadInd(0x00, 50, 100, 30, 80);
    ASSERT_TRUE(result.has_value());
    auto parsed = RSLParser::parse(*result);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ((*parsed).discriminator, RSLDiscriminator::CommonChannel);
    EXPECT_EQ((*parsed).msgType, static_cast<uint8_t>(RSLCChanMessageType::CCCHLoadInd));
    EXPECT_EQ((*parsed).chanNr, 0x00);
}

// Test: Build CHAN_RQD with request reference and parse back.
TEST(RSLB_buildChanRqd_RefRef, ParsesBack) {
    L3RequestReference ref(5, 1, 2, 3);
    auto result = RSLBuilder::buildChanRqd(0x40, ref, 10);
    ASSERT_TRUE(result.has_value());
    auto parsed = RSLParser::parse(*result);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ((*parsed).msgType, static_cast<uint8_t>(RSLCChanMessageType::ChanRqd));

    auto* reqRefIE = RSLParser::findIE(*parsed, RSL_IE::ReqReference);
    ASSERT_NE(reqRefIE, nullptr);
    EXPECT_EQ(reqRefIE->len, 3u);
    EXPECT_EQ(reqRefIE->val[0], ref.ra());
    EXPECT_EQ(reqRefIE->val[1], ref.t1p());
    EXPECT_EQ(reqRefIE->val[2], ref.t2());

    auto* delayIE = RSLParser::findIE(*parsed, RSL_IE::AccessDelay);
    ASSERT_NE(delayIE, nullptr);
    EXPECT_EQ(delayIE->len, 1u);
    EXPECT_EQ(delayIE->val[0], 10u);
}

// Test: Build HANDO_DET and parse back.
TEST(RSLB_buildHandoDet_Delay, ParsesBack) {
    auto result = RSLBuilder::buildHandoDet(0x7c, 25);
    ASSERT_TRUE(result.has_value());
    auto parsed = RSLParser::parse(*result);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ((*parsed).msgType, static_cast<uint8_t>(RSLDChanMessageType::HandoDet));

    auto* delayIE = RSLParser::findIE(*parsed, RSL_IE::AccessDelay);
    ASSERT_NE(delayIE, nullptr);
    EXPECT_EQ(delayIE->val[0], 25u);
}

// Test: Span overload for DATA_IND writes correct byte count.
TEST(RSLB_buildDataInd_SpanOverload, CorrectBytes) {
    std::vector<uint8_t> l3 = {0x09, 0x68, 0x02};
    std::vector<uint8_t> buf(256, 0);
    int n = RSLBuilder::buildDataInd(buf, 0x7c, 2, l3);
    EXPECT_GT(n, 0);
    // Header(4) + L3(3) = 7 bytes.
    EXPECT_EQ(n, 7);

    auto parsed = RSLParser::parse(std::span<const uint8_t>(buf.data(), n));
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ((*parsed).msgType, static_cast<uint8_t>(RSLL3MessageType::DataInd));
}

// Test: Span overload returns -1 when buffer too small.
TEST(RSLB_buildDataInd_SpanOverload_BufferTooSmall, ReturnsMinusOne) {
    std::vector<uint8_t> l3(100, 0xaa); // 100 bytes L3
    std::vector<uint8_t> buf(10, 0);   // Too small for header + payload
    int n = RSLBuilder::buildDataInd(buf, 0x7c, 1, l3);
    EXPECT_EQ(n, -1);
}

// Test: Build RF_CHAN_REL_ACK parses back.
TEST(RSLB_buildRFChanRelAck, ParsesBack) {
    auto result = RSLBuilder::buildRFChanRelAck(0x7c);
    ASSERT_TRUE(result.has_value());
    auto parsed = RSLParser::parse(*result);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ((*parsed).msgType, static_cast<uint8_t>(RSLDChanMessageType::RFChanRelAck));
    EXPECT_EQ((*parsed).chanNr, 0x7c);
}

// Test: Build CONN_FAIL with cause parses back.
TEST(RSLB_buildConnFail_Cause, ParsesBack) {
    auto result = RSLBuilder::buildConnFail(0x7c, RSLErrorCause::EquipmentFailure);
    ASSERT_TRUE(result.has_value());
    auto parsed = RSLParser::parse(*result);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ((*parsed).msgType, static_cast<uint8_t>(RSLDChanMessageType::ConnFail));

    auto* causeIE = RSLParser::findIE(*parsed, RSL_IE::Cause);
    ASSERT_NE(causeIE, nullptr);
    EXPECT_EQ(causeIE->val[0], static_cast<uint8_t>(RSLErrorCause::EquipmentFailure));
}

// Test: Build UNIT_DATA_REQ round-trip.
TEST(RSLB_buildUnitDataReq, RoundTrip) {
    std::vector<uint8_t> l3 = {0x04, 0x10, 0x01};
    auto result = RSLBuilder::buildUnitDataReq(0x60, 0, l3);
    ASSERT_TRUE(result.has_value());
    auto parsed = RSLParser::parse(*result);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ((*parsed).msgType, static_cast<uint8_t>(RSLL3MessageType::UnitDataReq));
}

// Test: Build UNIT_DATA_IND round-trip.
TEST(RSLB_buildUnitDataInd, RoundTrip) {
    std::vector<uint8_t> l3 = {0x04, 0x20, 0x01};
    auto result = RSLBuilder::buildUnitDataInd(0x60, 0, l3);
    ASSERT_TRUE(result.has_value());
    auto parsed = RSLParser::parse(*result);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ((*parsed).msgType, static_cast<uint8_t>(RSLL3MessageType::UnitDataInd));
}

// Test: Build DELETE_IND round-trip.
TEST(RSLB_buildDeleteInd, RoundTrip) {
    std::vector<uint8_t> immAssInfo = {0x01, 0x02, 0x03};
    auto result = RSLBuilder::buildDeleteInd(0x60, immAssInfo);
    ASSERT_TRUE(result.has_value());
    auto parsed = RSLParser::parse(*result);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ((*parsed).msgType, static_cast<uint8_t>(RSLCChanMessageType::DeleteInd));

    auto* ie = RSLParser::findIE(*parsed, RSL_IE::FullImmAssInfo);
    ASSERT_NE(ie, nullptr);
    EXPECT_EQ(ie->len, 3u);
}

// Test: builders set the TS 48.058 direction bit correctly (audit C6):
// BTS->BSC messages carry bit 0 set; BSC->BTS (testing/loopback) clear.
TEST(RSLB_build_DirectionBit, SetPerMessageDirection) {
    std::array<uint8_t, 3> l3{0x09, 0x68, 0x02};
    auto l3Span = std::span<const uint8_t>(l3.data(), l3.size());

    auto ind = RSLBuilder::buildDataInd(0x7e, 3, l3Span);
    ASSERT_TRUE(ind.has_value());
    EXPECT_EQ((*ind)[0], 0x01u); // RLL | BTS->BSC

    auto req = RSLBuilder::buildDataReq(0x7c, 1, l3Span);
    ASSERT_TRUE(req.has_value());
    EXPECT_EQ((*req)[0], 0x00u); // RLL | BSC->BTS

    auto ack = RSLBuilder::buildChanActivAck(0x78, 100);
    ASSERT_TRUE(ack.has_value());
    EXPECT_EQ((*ack)[0], 0x61u); // DCHAN | BTS->BSC

    auto load = RSLBuilder::buildCCCHLoadInd(0x00, 50, 100, 30, 80);
    ASSERT_TRUE(load.has_value());
    EXPECT_EQ((*load)[0], 0x41u); // CCHAN | BTS->BSC
}
