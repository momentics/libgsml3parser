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
// All L3 header byte values follow the TS 24.008 / TS 44.018 L3 protocol
// header layout: octet 0 = TI(7:5) | TIF(4) | PD(3:0), octet 1 carries the
// message type per domain:
//   - RRHeader {0x06, 0x0D}: PD=6(RR) in the low nibble of octet 0; MTI=0x0D(ChannelRelease)
//   - MMHeader {0x05, 0x21}: PD=5(MM); messageType=0x21(CMServAcc) in the six low bits of octet 1
//   - CCHeader {0xE3, 0x25}: PD=3(CC), TI=7 (bits 7:5), TIF=0; MTI=0x25(Disconnect) in the six low bits
//   - SSHeader {0x0B, 0x3A}: PD=11(SS); MTI=0x3A(Facility) in the six low bits

#include <gtest/gtest.h>
#include "gsml3parser/l3header.h"
#include <array>

using namespace gsml3parser;

TEST(L3HeaderTest, RRHeader) {
    std::array<uint8_t, 2> data{0x06, 0x0D};
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
    std::array<uint8_t, 2> data{0x05, 0x21};
    auto res = parseL3Header(data);
    EXPECT_TRUE(res.has_value());
    L3Header hdr = res.value();
    EXPECT_EQ(hdr.pd, L3PD::MobilityManagement);
    EXPECT_EQ(hdr.mti, 0x21);
    EXPECT_EQ(hdr.ti, 0u);
    EXPECT_FALSE(hdr.tif);
}

TEST(L3HeaderTest, CCHeader) {
    // PD=3(CC) low nibble, TI=7 in bits 7:5 -> octet 0 = 0xE3;
    // MTI=0x25(Disconnect) in the six low bits of octet 1 (NSD=0).
    std::array<uint8_t, 2> data{0xE3, 0x25};
    auto res = parseL3Header(data);
    EXPECT_TRUE(res.has_value());
    L3Header hdr = res.value();
    EXPECT_EQ(hdr.pd, L3PD::CallControl);
    EXPECT_EQ(hdr.mti, 0x25);
    EXPECT_EQ(hdr.ti, 7u);
    EXPECT_FALSE(hdr.tif);
}

TEST(L3HeaderTest, SSHeader) {
    // PD=11(SS) low nibble; MTI=0x3A(Facility) in the six low bits of octet 1.
    std::array<uint8_t, 2> data{0x0B, 0x3A};
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
    std::array<uint8_t, 1> data{0x06};
    auto res = parseL3Header(data);
    EXPECT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code, ParseError::Code::TruncatedInput);
}

// ── Extended PD (0x0e) header ──────────────────────────────────────────

TEST(L3HeaderTest, ExtendedHeader) {
    // PD=0x0e(Extended) in the low nibble of octet 0, MTI=0x55, raw byte extraction
    std::array<uint8_t, 2> data{0x0E, 0x55};
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
    std::array<uint8_t, 2> data{0x0E, 0xFF};
    auto res = parseL3Header(data);
    EXPECT_TRUE(res.has_value());
    L3Header hdr = res.value();
    EXPECT_EQ(hdr.pd, L3PD::Extended);
    EXPECT_EQ(hdr.mti, 0xFF);
}

// ── TestProcedure PD (0x0f) header ─────────────────────────────────────

TEST(L3HeaderTest, TestProcedureHeader) {
    // PD=0x0f(TestProcedure) in the low nibble of octet 0, MTI=0xAA, raw byte extraction
    std::array<uint8_t, 2> data{0x0F, 0xAA};
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
    std::array<uint8_t, 2> data{0x0F, 0x00};
    auto res = parseL3Header(data);
    EXPECT_TRUE(res.has_value());
    L3Header hdr = res.value();
    EXPECT_EQ(hdr.pd, L3PD::TestProcedure);
    EXPECT_EQ(hdr.mti, 0x00);
}

// ── Location Services PD (0x0c) header ─────────────────────────────────

TEST(L3HeaderTest, LocationServicesHeader) {
    // PD=0x0c(Location) in the low nibble of octet 0, MTI=0x01(LocationServiceRequest)
    std::array<uint8_t, 2> data{0x0C, 0x01};
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
    std::array<uint8_t, 2> data{0x0C, 0x02};
    auto res = parseL3Header(data);
    EXPECT_TRUE(res.has_value());
    L3Header hdr = res.value();
    EXPECT_EQ(hdr.pd, L3PD::Location);
    EXPECT_EQ(hdr.mti, 0x02);
}

// ── GMM header (PD=0x08) ───────────────────────────────────────────────

TEST(L3HeaderTest, GMMHeader) {
    // PD=0x08(GMM) in the low nibble of octet 0, MTI=0x01(AttachRequest), raw byte extraction
    std::array<uint8_t, 2> data{0x08, 0x01};
    auto res = parseL3Header(data);
    EXPECT_TRUE(res.has_value());
    L3Header hdr = res.value();
    EXPECT_EQ(hdr.pd, L3PD::GPRSMobilityManagement);
    EXPECT_EQ(hdr.mti, 0x01);
}

// ── SM header (PD=0x0a) ────────────────────────────────────────────────

TEST(L3HeaderTest, SMHeader) {
    // PD=0x0a(SM) in the low nibble of octet 0, MTI=0x41(ActivatePDPContextRequest), raw byte extraction
    std::array<uint8_t, 2> data{0x0A, 0x41};
    auto res = parseL3Header(data);
    EXPECT_TRUE(res.has_value());
    L3Header hdr = res.value();
    EXPECT_EQ(hdr.pd, L3PD::GPRSSessionManagement);
    EXPECT_EQ(hdr.mti, 0x41);
}

// ── SMS header (PD=0x09) ───────────────────────────────────────────────

TEST(L3HeaderTest, SMSHeader) {
    // PD=0x09(SMS) in the low nibble of octet 0, MTI=0x01(CPData), raw byte extraction
    std::array<uint8_t, 2> data{0x09, 0x01};
    auto res = parseL3Header(data);
    ASSERT_TRUE(res);
    L3Header hdr = res.value();
    EXPECT_EQ(hdr.pd, L3PD::SMS);
    EXPECT_EQ(hdr.mti, 0x01);
}

// Test: reserved PD values (0x02, 0x04, 0x07, 0x0d) are rejected with
// InvalidPD instead of producing an L3Header with a non-enumerator PD.
// The PD occupies the low nibble of octet 0 (TS 24.008 L3 header).
TEST(L3HeaderTest, ReservedPD_Invalid) {
    for (uint8_t pd : {0x02u, 0x04u, 0x07u, 0x0Du}) {
        uint8_t data[] = {static_cast<uint8_t>(pd), 0x00};
        auto res = parseL3Header(std::span<const uint8_t>(data, 2));
        ASSERT_FALSE(res) << "PD 0x" << std::hex << pd << " must be rejected";
        EXPECT_EQ(res.error().code, ParseError::Code::InvalidPD);
    }
}

// Octet layout: PD in the low nibble of octet 0; TI in the three high bits,
// TIF in bit 4. A CC frame with TI=3 and TIF set must decode those fields
// while still selecting the CC domain.
TEST(L3HeaderTest, TiTifPositions) {
    uint8_t data[] = {static_cast<uint8_t>((3 << 5) | 0x10 | 0x03), 0x05};
    auto res = parseL3Header(std::span<const uint8_t>(data, 2));
    ASSERT_TRUE(res);
    L3Header hdr = res.value();
    EXPECT_EQ(hdr.pd, L3PD::CallControl);
    EXPECT_EQ(hdr.ti, 3u);
    EXPECT_TRUE(hdr.tif);
    EXPECT_EQ(hdr.mti, 0x05);
}

// Six-bit-domain MTI extraction: only the low six bits of octet 1 are the
// message type (the two high bits carry the NSD and must not leak into the
// decoded MTI).
TEST(L3HeaderTest, MtMaskSixBit) {
    uint8_t data[] = {0x05, static_cast<uint8_t>(0xC0 | 0x21)}; // NSD=3, MT=CM Service Accept
    auto res = parseL3Header(std::span<const uint8_t>(data, 2));
    ASSERT_TRUE(res);
    EXPECT_EQ(res.value().mti, 0x21);
}

// RR short-message header (TIF=1): the message code occupies the low five
// bits of octet 1 (the high three bits are reserved and must be ignored);
// the internal MTI is remapped above kRRTifShortBase. Code 5 here decodes
// to Measurement Info DL (TS 44.018 short-message table).
TEST(L3HeaderTest, RrTifShortCode) {
    uint8_t data[] = {0x16, 0x25}; // TIF|PD=RR, code bits '00101', reserved '000'
    auto res = parseL3Header(std::span<const uint8_t>(data, 2));
    ASSERT_TRUE(res);
    L3Header hdr = res.value();
    EXPECT_EQ(hdr.pd, L3PD::RadioResource);
    EXPECT_TRUE(hdr.tif);
    EXPECT_EQ(hdr.mti, kRRTifShortBase + 0x05);
}
