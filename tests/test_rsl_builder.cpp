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
    std::vector<uint8_t> l3 = {0x05, 0x24, 0x02}; // CM Service Request (PD=MM low nibble, MT=0x24)
    auto result = RSLBuilder::buildDataReq(0x7c, 1, l3);
    ASSERT_TRUE(result.has_value());
    auto parsed = RSLParser::parse(*result);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ((*parsed).discriminator, RSLDiscriminator::Rll);
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
    std::vector<uint8_t> l3 = {0x06, 0x0D, 0x01}; // RR Channel Release (PD=RR low nibble)
    auto result = RSLBuilder::buildDataInd(0x7e, 5, l3);
    ASSERT_TRUE(result.has_value());
    auto parsed = RSLParser::parse(*result);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ((*parsed).msgType, static_cast<uint8_t>(RSLL3MessageType::DataInd));
    EXPECT_EQ((*parsed).chanNr, 0x7e);
    EXPECT_EQ((*parsed).linkId, 5);
}

// Test: Build CHAN_ACTIV_ACK with frame number and parse back. The Frame
// Number IE value is the starting-time coding (TS 48.058 9.3.8) of the
// absolute TDMA frame number: for FN=0x1234 (4660), t1p=(4660/1326)%32=3,
// t3=4660%51=19, t2=4660%26=6, which pack to {0x1A, 0x66}.
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
    // Starting-time octets: (t1p<<3)|(t3>>3) and ((t3&7)<<5)|t2.
    EXPECT_EQ(fnIE->val[0], 0x1A);
    EXPECT_EQ(fnIE->val[1], 0x66);

    auto fn = RSLParser::getFrameNumber(*parsed);
    ASSERT_TRUE(fn.has_value());
    EXPECT_EQ(fn->t1p, 3u);
    EXPECT_EQ(fn->t3, 19u);
    EXPECT_EQ(fn->t2, 6u);
}

// Golden: CHAN_ACTIV_ACK for the canonical absolute TDMA frame number FN=207
// (TS 48.058 9.3.8): t1p=(207/1326)%32=0, t3=207%51=3, t2=207%26=25 pack to
// the two value octets {0x00, 0x79}.
TEST(RSLB_buildChanActivAck_FrameNumber207, RefVector) {
    auto result = RSLBuilder::buildChanActivAck(0x78, 207);
    ASSERT_TRUE(result.has_value());
    auto parsed = RSLParser::parse(*result);
    ASSERT_TRUE(parsed.has_value());

    auto* fnIE = RSLParser::findIE(*parsed, RSL_IE::FrameNumber);
    ASSERT_NE(fnIE, nullptr);
    EXPECT_EQ(fnIE->val[0], 0x00);
    EXPECT_EQ(fnIE->val[1], 0x79);

    auto fn = RSLParser::getFrameNumber(*parsed);
    ASSERT_TRUE(fn.has_value());
    EXPECT_EQ(fn->t1p, 0u);
    EXPECT_EQ(fn->t3, 3u);
    EXPECT_EQ(fn->t2, 25u);
}

// Test: Build CHAN_ACTIV_NACK with cause and parse back.
TEST(RSLB_buildChanActivNack_Cause, ParsesBack) {
    auto result = RSLBuilder::buildChanActivNack(0x78, RSLErrorCause::ResUnavail);
    ASSERT_TRUE(result.has_value());
    auto parsed = RSLParser::parse(*result);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ((*parsed).msgType, static_cast<uint8_t>(RSLDChanMessageType::ChanActivNack));

    auto* causeIE = RSLParser::findIE(*parsed, RSL_IE::Cause);
    ASSERT_NE(causeIE, nullptr);
    EXPECT_EQ(causeIE->len, 1u);
    EXPECT_EQ(causeIE->val[0], static_cast<uint8_t>(RSLErrorCause::ResUnavail));
}

// Golden: MEAS_RES uplink measurements (TS 48.058 9.3.25) — RX level
// full=40, sub=35; RX quality full=5, sub=6; DTX downlink clear encode to the
// three value octets {0x28, 0x23, 0x2E}. The IE sequence MeasResNr + UplinkMeas
// is the canonical seven-octet vector.
TEST(RSLB_buildMeasRes_UplinkMeas, RefVector) {
    auto result = RSLBuilder::buildMeasRes(0x7c, 5, 40, 35, 5, 6, false, 0);
    ASSERT_TRUE(result.has_value());
    auto parsed = RSLParser::parse(*result);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ((*parsed).msgType, static_cast<uint8_t>(RSLDChanMessageType::MeasRes));

    // Seven-octet IE vector: MeasResNr TV (2) + UplinkMeas LV (5).
    const uint8_t expectedIes[7] = {0x1B, 0x05, 0x19, 0x03, 0x28, 0x23, 0x2E};
    ASSERT_GE((*result).size(), 4u + sizeof(expectedIes));
    EXPECT_EQ(0, std::memcmp((*result).data() + 4, expectedIes, sizeof(expectedIes)));

    auto* measNrIE = RSLParser::findIE(*parsed, RSL_IE::MeasResNr);
    ASSERT_NE(measNrIE, nullptr);
    EXPECT_EQ(measNrIE->len, 1u);
    EXPECT_EQ(measNrIE->val[0], 5u);

    auto* uplinkIE = RSLParser::findIE(*parsed, RSL_IE::UplinkMeas);
    ASSERT_NE(uplinkIE, nullptr);
    EXPECT_EQ(uplinkIE->len, 3u);
    EXPECT_EQ(uplinkIE->val[0], 0x28u);
    EXPECT_EQ(uplinkIE->val[1], 0x23u);
    EXPECT_EQ(uplinkIE->val[2], 0x2Eu);

    auto meas = RSLParser::getUplinkMeas(*parsed);
    ASSERT_TRUE(meas.has_value());
    EXPECT_FALSE(meas->dtxDownlink);
    EXPECT_EQ(meas->rxlevFull, 40u);
    EXPECT_EQ(meas->rxlevSub, 35u);
    EXPECT_EQ(meas->rxqFull, 5u);
    EXPECT_EQ(meas->rxqSub, 6u);

    // l1_info == 0: the L1 Information IE is omitted.
    EXPECT_EQ(RSLParser::findIE(*parsed, RSL_IE::L1Info), nullptr);
}

// Test: Build MEAS_RES with the DTX downlink indicator set and a non-zero L1
// information octet (TS 48.058 9.3.25/9.3.10) and parse back.
TEST(RSLB_buildMeasRes_DtxAndL1Info, ParsesBack) {
    auto result = RSLBuilder::buildMeasRes(0x7c, 5, 40, 35, 5, 6, true, 0x1A);
    ASSERT_TRUE(result.has_value());
    auto parsed = RSLParser::parse(*result);
    ASSERT_TRUE(parsed.has_value());

    // DTX_d set in the first value octet: [0|1|101000] = 0x68.
    auto* uplinkIE = RSLParser::findIE(*parsed, RSL_IE::UplinkMeas);
    ASSERT_NE(uplinkIE, nullptr);
    EXPECT_EQ(uplinkIE->val[0], 0x68u);

    auto meas = RSLParser::getUplinkMeas(*parsed);
    ASSERT_TRUE(meas.has_value());
    EXPECT_TRUE(meas->dtxDownlink);
    EXPECT_EQ(meas->rxlevFull, 40u);

    auto* l1IE = RSLParser::findIE(*parsed, RSL_IE::L1Info);
    ASSERT_NE(l1IE, nullptr);
    EXPECT_EQ(l1IE->len, 2u);
    EXPECT_EQ(l1IE->val[0], 0x1Au);
    EXPECT_EQ(l1IE->val[1], 0x00u);
}

// Test: excess high bits of the measurement fields are discarded on encode
// (RX level is six-bit, RX quality three-bit, TS 48.058 9.3.25).
TEST(RSLB_buildMeasRes_FieldMasking, TruncatedToWidth) {
    auto result = RSLBuilder::buildMeasRes(0x7c, 1, 0xFF, 0xFF, 0xFF, 0xFF, false, 0);
    ASSERT_TRUE(result.has_value());
    auto parsed = RSLParser::parse(*result);
    ASSERT_TRUE(parsed.has_value());

    auto* uplinkIE = RSLParser::findIE(*parsed, RSL_IE::UplinkMeas);
    ASSERT_NE(uplinkIE, nullptr);
    EXPECT_EQ(uplinkIE->val[0], 0x3Fu); // [0|0|111111]
    EXPECT_EQ(uplinkIE->val[1], 0x3Fu); // [00|111111]
    EXPECT_EQ(uplinkIE->val[2], 0x3Fu); // [00|111|111]

    auto meas = RSLParser::getUplinkMeas(*parsed);
    ASSERT_TRUE(meas.has_value());
    EXPECT_EQ(meas->rxlevFull, 63u);
    EXPECT_EQ(meas->rxlevSub, 63u);
    EXPECT_EQ(meas->rxqFull, 7u);
    EXPECT_EQ(meas->rxqSub, 7u);
}

// Test: Build CCCH_LOAD_IND and parse back.
TEST(RSLB_buildCCCHLoadInd_Loads, ParsesBack) {
    auto result = RSLBuilder::buildCCCHLoadInd(0x00, 50, 100, 30, 80);
    ASSERT_TRUE(result.has_value());
    auto parsed = RSLParser::parse(*result);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ((*parsed).discriminator, RSLDiscriminator::CommonChannel);
    EXPECT_EQ((*parsed).msgType, static_cast<uint8_t>(RSLCChanMessageType::CcchLoadInd));
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
    // TV with the fixed three-octet value: RA plus the two-octet frame number
    // T1'(5)|T3(6)|T2(5) (TS 48.058 9.3.19).
    EXPECT_EQ(reqRefIE->len, 3u);
    EXPECT_EQ(reqRefIE->val[0], ref.ra());
    EXPECT_EQ(reqRefIE->val[1], static_cast<uint8_t>((ref.t1p() << 3) | (ref.t3() >> 3)));
    EXPECT_EQ(reqRefIE->val[2], static_cast<uint8_t>((ref.t3() << 5) | ref.t2()));

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
    std::vector<uint8_t> l3 = {0x05, 0x24, 0x02};
    std::vector<uint8_t> buf(256, 0);
    int n = RSLBuilder::buildDataInd(buf, 0x7c, 2, l3);
    EXPECT_GT(n, 0);
    // first(1) + type(1) + ChanNr IE(2) + LinkIdent IE(2) + L3Info TL16V(3) + L3(3) = 12.
    EXPECT_EQ(n, 12);
    EXPECT_EQ(buf[0], rslFirstOctet(RSLDiscriminator::Rll, /*transparent=*/true)); // 0x03
    EXPECT_EQ(buf[1], static_cast<uint8_t>(RSLL3MessageType::DataInd));            // 0x02
    // L3Info IE header (type 0x0B, TL16V length 3).
    EXPECT_EQ(buf[6], static_cast<uint8_t>(RSL_IE::L3Info));
    EXPECT_EQ(buf[7], 0x00);
    EXPECT_EQ(buf[8], 0x03);

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
    EXPECT_EQ((*parsed).msgType, static_cast<uint8_t>(RSLDChanMessageType::RfChanRelAck));
    EXPECT_EQ((*parsed).chanNr, 0x7c);
}

// Test: Build CONN_FAIL with cause parses back.
TEST(RSLB_buildConnFail_Cause, ParsesBack) {
    auto result = RSLBuilder::buildConnFail(0x7c, RSLErrorCause::EquipmentFail);
    ASSERT_TRUE(result.has_value());
    auto parsed = RSLParser::parse(*result);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ((*parsed).msgType, static_cast<uint8_t>(RSLDChanMessageType::ConnFail));

    auto* causeIE = RSLParser::findIE(*parsed, RSL_IE::Cause);
    ASSERT_NE(causeIE, nullptr);
    EXPECT_EQ(causeIE->val[0], static_cast<uint8_t>(RSLErrorCause::EquipmentFail));
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

// Test: builders set the TS 48.058 9.1 transparent flag correctly:
// RLL data frames carry it set (L3 transported transparently); DCHAN/CCHAN
// control frames clear it.
TEST(RSLB_build_TransparentFlag, SetPerMessageGroup) {
    std::array<uint8_t, 3> l3{0x05, 0x24, 0x02};
    auto l3Span = std::span<const uint8_t>(l3.data(), l3.size());

    auto ind = RSLBuilder::buildDataInd(0x7e, 3, l3Span);
    ASSERT_TRUE(ind.has_value());
    EXPECT_EQ((*ind)[0], rslFirstOctet(RSLDiscriminator::Rll, true)); // 0x03

    auto req = RSLBuilder::buildDataReq(0x7c, 1, l3Span);
    ASSERT_TRUE(req.has_value());
    EXPECT_EQ((*req)[0], rslFirstOctet(RSLDiscriminator::Rll, true)); // 0x03

    auto ack = RSLBuilder::buildChanActivAck(0x78, 100);
    ASSERT_TRUE(ack.has_value());
    EXPECT_EQ((*ack)[0], rslFirstOctet(RSLDiscriminator::DedicatedChannel, false)); // 0x08

    auto load = RSLBuilder::buildCCCHLoadInd(0x00, 50, 100, 30, 80);
    ASSERT_TRUE(load.has_value());
    EXPECT_EQ((*load)[0], rslFirstOctet(RSLDiscriminator::CommonChannel, false)); // 0x0C
}
