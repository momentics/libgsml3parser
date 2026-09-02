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

// L3 Header parsing tests with spec-compliant PD/MTI values.
// Reference: GSM 24.008 Table 11.2 (Protocol Discriminator assignments).
//
// [GOLDEN VERIFICATION]
// All L3 header byte values verified against GSM 24.008 Table 11.2 PD assignments:
//   - RRHeader {0x60, 0x0D}: PD=6(RR) high nibble, skip=0 low nibble -> 0x60; MTI=0x0D(ChannelRelease)
//     Verified against GSM_RR_Types.ttcn CHANNEL_RELEASE='00001101'B(0x0D)
//   - MMHeader {0x50, 0x84}: PD=5(MM) high nibble, skip=0 low nibble -> 0x50; raw MTI=0x84 -> messageType=0x21(CMServAcc)
//     Verified against L3_Templates.ttcn tr_CM_SERV_ACC: messageType='100001'B(0x21)
//   - CCHeader {0x3E, 0x94}: PD=3(CC) high nibble, TI=7+TIF=0 low nibble -> 0x3E; MTI=0x25(Disconnect)<<2=0x94
//     Verified against L3_Templates.ttcn ts_ML3_MO_CC_DISC: messageType='100101'B(0x25)
//   - SSHeader {0xB0, 0xE8}: PD=11(SS) high nibble, TI=0+TIF=0 low nibble -> 0xB0; MTI=0x3A(Facility)<<2=0xE8
//     Verified against SS_Templates.ttcn ts_SS_FACILITY_INVOKE

#include <gtest/gtest.h>
#include "gsml3parser/l3header.h"
#include <array>

using namespace gsml3parser;

TEST(L3HeaderTest, RRHeader) {
    std::array<uint8_t, 2> data{0x60, 0x0D};
    auto res = parseL3Header(data);
    EXPECT_TRUE(res.has_value());
    L3Header hdr = res.value();
    EXPECT_EQ(hdr.pd, L3PD::RadioResource);
    EXPECT_EQ(hdr.mti, 0x0D);
    EXPECT_EQ(hdr.ti, 0u);
    EXPECT_FALSE(hdr.tif);
    EXPECT_TRUE(hdr.isValid());
}

TEST(L3HeaderTest, MMHeader) {
    std::array<uint8_t, 2> data{0x50, 0x84};
    auto res = parseL3Header(data);
    EXPECT_TRUE(res.has_value());
    L3Header hdr = res.value();
    EXPECT_EQ(hdr.pd, L3PD::MobilityManagement);
    EXPECT_EQ(hdr.mti, 0x21);
    EXPECT_EQ(hdr.ti, 0u);
    EXPECT_FALSE(hdr.tif);
}

TEST(L3HeaderTest, CCHeader) {
    std::array<uint8_t, 2> data{0x3E, 0x94};
    auto res = parseL3Header(data);
    EXPECT_TRUE(res.has_value());
    L3Header hdr = res.value();
    EXPECT_EQ(hdr.pd, L3PD::CallControl);
    EXPECT_EQ(hdr.mti, 0x25);
    EXPECT_EQ(hdr.ti, 7u);
    EXPECT_FALSE(hdr.tif);
}

TEST(L3HeaderTest, SSHeader) {
    std::array<uint8_t, 2> data{0xB0, 0xE8};
    auto res = parseL3Header(data);
    EXPECT_TRUE(res.has_value());
    L3Header hdr = res.value();
    EXPECT_EQ(hdr.pd, L3PD::NonCallSS);
    EXPECT_EQ(hdr.mti, 0x3A);
    EXPECT_EQ(hdr.ti, 0u);
    EXPECT_FALSE(hdr.tif);
}

TEST(L3HeaderTest, EmptySpan) {
    std::array<uint8_t, 0> data{};
    auto res = parseL3Header(data);
    EXPECT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code, ParseError::Code::TruncatedInput);
}

TEST(L3HeaderTest, SingleByte) {
    std::array<uint8_t, 1> data{0x60};
    auto res = parseL3Header(data);
    EXPECT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code, ParseError::Code::TruncatedInput);
}

// ── Extended PD (0x0e) header ──────────────────────────────────────────

TEST(L3HeaderTest, ExtendedHeader) {
    // PD=0x0e(Extended), MTI=0x55, raw byte extraction
    std::array<uint8_t, 2> data{0xE0, 0x55};
    auto res = parseL3Header(data);
    EXPECT_TRUE(res.has_value());
    L3Header hdr = res.value();
    EXPECT_EQ(hdr.pd, L3PD::Extended);
    EXPECT_EQ(hdr.mti, 0x55);
    EXPECT_EQ(hdr.ti, 0u);
    EXPECT_FALSE(hdr.tif);
}

TEST(L3HeaderTest, ExtendedHeader_HighMTI) {
    // PD=0x0e(Extended), MTI=0xFF (high raw byte value)
    std::array<uint8_t, 2> data{0xE0, 0xFF};
    auto res = parseL3Header(data);
    EXPECT_TRUE(res.has_value());
    L3Header hdr = res.value();
    EXPECT_EQ(hdr.pd, L3PD::Extended);
    EXPECT_EQ(hdr.mti, 0xFF);
}

// ── TestProcedure PD (0x0f) header ─────────────────────────────────────

TEST(L3HeaderTest, TestProcedureHeader) {
    // PD=0x0f(TestProcedure), MTI=0xAA, raw byte extraction
    std::array<uint8_t, 2> data{0xF0, 0xAA};
    auto res = parseL3Header(data);
    EXPECT_TRUE(res.has_value());
    L3Header hdr = res.value();
    EXPECT_EQ(hdr.pd, L3PD::TestProcedure);
    EXPECT_EQ(hdr.mti, 0xAA);
    EXPECT_EQ(hdr.ti, 0u);
    EXPECT_FALSE(hdr.tif);
}

TEST(L3HeaderTest, TestProcedureHeader_ZeroMTI) {
    // PD=0x0f(TestProcedure), MTI=0x00
    std::array<uint8_t, 2> data{0xF0, 0x00};
    auto res = parseL3Header(data);
    EXPECT_TRUE(res.has_value());
    L3Header hdr = res.value();
    EXPECT_EQ(hdr.pd, L3PD::TestProcedure);
    EXPECT_EQ(hdr.mti, 0x00);
}

// ── Location Services PD (0x0c) header ─────────────────────────────────

TEST(L3HeaderTest, LocationServicesHeader) {
    // PD=0x0c(Location), MTI=0x01(LocationServiceRequest)
    std::array<uint8_t, 2> data{0xC0, 0x01};
    auto res = parseL3Header(data);
    EXPECT_TRUE(res.has_value());
    L3Header hdr = res.value();
    EXPECT_EQ(hdr.pd, L3PD::Location);
    EXPECT_EQ(hdr.mti, 0x01);
    EXPECT_EQ(hdr.ti, 0u);
    EXPECT_FALSE(hdr.tif);
}

TEST(L3HeaderTest, LocationServicesHeader_ProviderMessage) {
    // PD=0x0c(Location), MTI=0x02(LocationServiceProviderMessage)
    std::array<uint8_t, 2> data{0xC0, 0x02};
    auto res = parseL3Header(data);
    EXPECT_TRUE(res.has_value());
    L3Header hdr = res.value();
    EXPECT_EQ(hdr.pd, L3PD::Location);
    EXPECT_EQ(hdr.mti, 0x02);
}

// ── GMM header (PD=0x08) ───────────────────────────────────────────────

TEST(L3HeaderTest, GMMHeader) {
    // PD=0x08(GMM), MTI=0x01(AttachRequest), raw byte extraction
    std::array<uint8_t, 2> data{0x80, 0x01};
    auto res = parseL3Header(data);
    EXPECT_TRUE(res.has_value());
    L3Header hdr = res.value();
    EXPECT_EQ(hdr.pd, L3PD::GPRSMobilityManagement);
    EXPECT_EQ(hdr.mti, 0x01);
}

// ── SM header (PD=0x0a) ────────────────────────────────────────────────

TEST(L3HeaderTest, SMHeader) {
    // PD=0x0a(SM), MTI=0x41(ActivatePDPContextRequest), raw byte extraction
    std::array<uint8_t, 2> data{0xA0, 0x41};
    auto res = parseL3Header(data);
    EXPECT_TRUE(res.has_value());
    L3Header hdr = res.value();
    EXPECT_EQ(hdr.pd, L3PD::GPRSSessionManagement);
    EXPECT_EQ(hdr.mti, 0x41);
}

// ── SMS header (PD=0x09) ───────────────────────────────────────────────

TEST(L3HeaderTest, SMSHeader) {
    // PD=0x09(SMS), MTI=0x01(CPData), raw byte extraction
    std::array<uint8_t, 2> data{0x90, 0x01};
    auto res = parseL3Header(data);
    ASSERT_TRUE(res);
    L3Header hdr = res.value();
    EXPECT_EQ(hdr.pd, L3PD::SMS);
    EXPECT_EQ(hdr.mti, 0x01);
}

// Test: reserved PD values (0x02, 0x04, 0x07, 0x0d) are rejected with
// InvalidPD instead of producing an L3Header with a non-enumerator PD
// (audit Q4).
TEST(L3HeaderTest, ReservedPD_Invalid) {
    for (uint8_t pd : {0x02u, 0x04u, 0x07u, 0x0Du}) {
        uint8_t data[] = {static_cast<uint8_t>(pd << 4), 0x00};
        auto res = parseL3Header(std::span<const uint8_t>(data, 2));
        ASSERT_FALSE(res) << "PD 0x" << std::hex << pd << " must be rejected";
        EXPECT_EQ(res.error().code, ParseError::Code::InvalidPD);
    }
}
