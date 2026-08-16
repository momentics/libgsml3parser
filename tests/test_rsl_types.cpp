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

// Tests for RSL types: validates channel number encoding/decoding, channel mode
// flags, and that all name helper functions return non-empty strings for every enum value.
// 3GPP coverage: TS 48.058 (A-bis RSL), GSM 04.08 10.5.2.6 (Channel Mode IE).

#include <gtest/gtest.h>
#include "gsml3parser/abis/rsl_types.h"

using namespace gsml3parser;

// Test: Channel number encode/decode round-trips correctly for dedicated channels.
// Importance: BTS must correctly extract timeslot and type from RSL channel numbers.
// 3GPP: TS 48.058 section 7.2 - Channel Number encoding.
TEST(RSLT_ChannelNumber_EncodeDecode, RoundTrip) {
    // Channel type codes: 6=SDCCH/8, 7=SDCCH/4, 14=TCH/H, 30=SDCCH+TCH/F, 31=TCH/F
    for (uint8_t cbits : {6u, 7u, 14u, 30u, 31u}) {
        for (uint8_t ts = 0; ts < 8; ++ts) {
            uint8_t encoded = RSLChannelNumber::encode(cbits, ts);
            EXPECT_EQ(RSLChannelNumber::getCBits(encoded), cbits)
                << "cbits=" << static_cast<int>(cbits) << " ts=" << static_cast<int>(ts);
            EXPECT_EQ(RSLChannelNumber::getTimeslot(encoded), ts)
                << "cbits=" << static_cast<int>(cbits) << " ts=" << static_cast<int>(ts);
        }
    }
}

// Test: Common channel numbers are correctly identified as non-dedicated.
// Importance: BCCH, RACH, PCH/AGCH must not be treated as dedicated channels.
TEST(RSLT_ChannelNumber_IsDedicated, Correct) {
    EXPECT_FALSE(RSLChannelNumber::isDedicated(RSLChannelNumber::BCCH));
    EXPECT_FALSE(RSLChannelNumber::isDedicated(RSLChannelNumber::RACH));
    EXPECT_FALSE(RSLChannelNumber::isDedicated(RSLChannelNumber::PCH_AGCH));
    // Dedicated channels: type code != common channel values.
    EXPECT_TRUE(RSLChannelNumber::isDedicated(RSLChannelNumber::encode(6, 0)));  // SDCCH/8
    EXPECT_TRUE(RSLChannelNumber::isDedicated(RSLChannelNumber::encode(31, 7))); // TCH/F
}

// Test: ChannelMode isSignalling/isSpeech/isData return correct values.
// Importance: BTS must know channel type to select appropriate processing path.
TEST(RSLT_ChannelMode_SpeechData, Correct) {
    RSLChannelMode mode;

    mode.spdInd = static_cast<uint8_t>(RSLChannelMode::SpeedIndicator::Signalling);
    EXPECT_TRUE(mode.isSignalling());
    EXPECT_FALSE(mode.isSpeech());
    EXPECT_FALSE(mode.isData());

    mode.spdInd = static_cast<uint8_t>(RSLChannelMode::SpeedIndicator::Speech);
    EXPECT_FALSE(mode.isSignalling());
    EXPECT_TRUE(mode.isSpeech());
    EXPECT_FALSE(mode.isData());

    mode.spdInd = static_cast<uint8_t>(RSLChannelMode::SpeedIndicator::Data);
    EXPECT_FALSE(mode.isSignalling());
    EXPECT_FALSE(mode.isSpeech());
    EXPECT_TRUE(mode.isData());
}

// Test: All name helper functions return non-empty strings for every defined enum value.
// Importance: Logging and diagnostics depend on readable names for all RSL types.
TEST(RSLT_NameFunctions, AllNonEmpty) {
    // Discriminator names
    ASSERT_NE(rslDiscriminatorName(RSLDiscriminator::RLL), "");
    ASSERT_NE(rslDiscriminatorName(RSLDiscriminator::CommonChannel), "");
    ASSERT_NE(rslDiscriminatorName(RSLDiscriminator::DedicatedChannel), "");
    ASSERT_NE(rslDiscriminatorName(RSLDiscriminator::TRX), "");
    ASSERT_NE(rslDiscriminatorName(RSLDiscriminator::IPAccess), "");

    // IE names
    ASSERT_NE(rslIEName(RSL_IE::ChanNr), "");
    ASSERT_NE(rslIEName(RSL_IE::LinkIdent), "");
    ASSERT_NE(rslIEName(RSL_IE::ActType), "");
    ASSERT_NE(rslIEName(RSL_IE::ChanMode), "");
    ASSERT_NE(rslIEName(RSL_IE::EncrInfo), "");
    ASSERT_NE(rslIEName(RSL_IE::Cause), "");
    ASSERT_NE(rslIEName(RSL_IE::ReqReference), "");
    ASSERT_NE(rslIEName(RSL_IE::FrameNumber), "");
    ASSERT_NE(rslIEName(RSL_IE::L3Info), "");
    ASSERT_NE(rslIEName(RSL_IE::MeasResNr), "");
    ASSERT_NE(rslIEName(RSL_IE::UplinkMeas), "");
    ASSERT_NE(rslIEName(RSL_IE::AccessDelay), "");
    ASSERT_NE(rslIEName(RSL_IE::FullImmAssInfo), "");

    // Error cause names
    ASSERT_NE(rslErrorCauseName(RSLErrorCause::NormalUnspecified), "");
    ASSERT_NE(rslErrorCauseName(RSLErrorCause::EquipmentFailure), "");
    ASSERT_NE(rslErrorCauseName(RSLErrorCause::ResourceUnavailable), "");
    ASSERT_NE(rslErrorCauseName(RSLErrorCause::IEContentError), "");
    ASSERT_NE(rslErrorCauseName(RSLErrorCause::ProtocolError), "");
}

// Test: RSLChannelMode size is exactly 5 bytes as required by spec.
TEST(RSLT_ChannelMode_Size, ExactlyFiveBytes) {
    EXPECT_EQ(sizeof(RSLChannelMode), 5u);
}
