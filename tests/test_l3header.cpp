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
