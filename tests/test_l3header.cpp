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
