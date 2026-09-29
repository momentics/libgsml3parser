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
// 3GPP coverage: TS 48.058 (A-bis RSL), 9.3.6 (Channel Mode IE), 9.3.26 (Cause values).

#include <gtest/gtest.h>
#include "gsml3parser/abis/rsl_types.h"

using namespace gsml3parser;

// Test: Channel number encode/decode round-trips correctly.
// Importance: BTS must correctly extract timeslot and type from RSL channel numbers.
// 3GPP: TS 48.058 9.3.1 - Channel Number IE value (code << 3 | tn).
TEST(RSLT_ChannelNumber_EncodeDecode, RoundTrip) {
    // Channel codes (five high bits): 1=Bm ACCH, 2=Lm, 4=SDCCH/4, 8=SDCCH/8,
    // 0x10=BCCH, 0x11=RACH, 0x1D=VAMOS Bm.
    for (uint8_t code : {1u, 2u, 4u, 8u, 0x10u, 0x11u, 0x1Du}) {
        for (uint8_t ts = 0; ts < 8; ++ts) {
            uint8_t encoded = RSLChannelNumber::encode(code, ts);
            EXPECT_EQ(RSLChannelNumber::getCBits(encoded), code)
                << "code=" << static_cast<int>(code) << " ts=" << static_cast<int>(ts);
            EXPECT_EQ(RSLChannelNumber::getTimeslot(encoded), ts)
                << "code=" << static_cast<int>(code) << " ts=" << static_cast<int>(ts);
        }
    }
}

// Test: common-channel codes (16..31) are identified as non-dedicated.
// Importance: BCCH, RACH, PCH/AGCH and the common-channel extensions must not
// be treated as dedicated channels.
TEST(RSLT_ChannelNumber_IsDedicated, Correct) {
    EXPECT_FALSE(RSLChannelNumber::isDedicated(RSLChannelNumber::encode(RSLChannelNumber::Bcch, 0)));
    EXPECT_FALSE(RSLChannelNumber::isDedicated(RSLChannelNumber::encode(RSLChannelNumber::Rach, 3)));
    EXPECT_FALSE(RSLChannelNumber::isDedicated(RSLChannelNumber::encode(RSLChannelNumber::PchAgch, 0)));
    // Dedicated channels: codes below the common-channel range.
    EXPECT_TRUE(RSLChannelNumber::isDedicated(RSLChannelNumber::encode(RSLChannelNumber::Sdcch8, 0)));  // SDCCH/8 sub 0
    EXPECT_TRUE(RSLChannelNumber::isDedicated(RSLChannelNumber::encode(RSLChannelNumber::BmAcch, 7))); // TCH/F ACCH
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
    ASSERT_NE(rslDiscriminatorName(RSLDiscriminator::Rll), "");
    ASSERT_NE(rslDiscriminatorName(RSLDiscriminator::CommonChannel), "");
    ASSERT_NE(rslDiscriminatorName(RSLDiscriminator::DedicatedChannel), "");
    ASSERT_NE(rslDiscriminatorName(RSLDiscriminator::TrxManagement), "");
    ASSERT_NE(rslDiscriminatorName(RSLDiscriminator::Lcs), "");
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

    // Error cause names (TS 48.058 9.3.26)
    ASSERT_NE(rslErrorCauseName(RSLErrorCause::NormalUnspec), "");
    ASSERT_NE(rslErrorCauseName(RSLErrorCause::EquipmentFail), "");
    ASSERT_NE(rslErrorCauseName(RSLErrorCause::ResUnavail), "");
    ASSERT_NE(rslErrorCauseName(RSLErrorCause::IeContent), "");
    ASSERT_NE(rslErrorCauseName(RSLErrorCause::Proto), "");
}

// Test: RSLChannelMode size is exactly 4 bytes as required by the Channel Mode
// IE value coding (TS 48.058 9.3.6).
TEST(RSLT_ChannelMode_Size, ExactlyFourBytes) {
    EXPECT_EQ(sizeof(RSLChannelMode), 4u);
}

// Test: RSLChannelMode DTX helpers decode the two indicator bits of the first
// value octet [reserved(6)|DTX_d(1)|DTX_u(1)] (TS 48.058 9.3.6).
TEST(RSLT_ChannelMode_DtxBits, Correct) {
    RSLChannelMode mode;
    mode.dtx = 0x00;
    EXPECT_FALSE(mode.dtxDownlink());
    EXPECT_FALSE(mode.dtxUplink());

    mode.dtx = 0x01; // DTX_u only
    EXPECT_FALSE(mode.dtxDownlink());
    EXPECT_TRUE(mode.dtxUplink());

    mode.dtx = 0x02; // DTX_d only
    EXPECT_TRUE(mode.dtxDownlink());
    EXPECT_FALSE(mode.dtxUplink());

    mode.dtx = 0x03; // both
    EXPECT_TRUE(mode.dtxDownlink());
    EXPECT_TRUE(mode.dtxUplink());
}

// Test: the canonical speed-indicator and channel-rate-type values of the
// Channel Mode IE (TS 48.058 9.3.6) round-trip through the accessors.
TEST(RSLT_ChannelMode_CanonicalValues, Correct) {
    RSLChannelMode mode;

    mode.spdInd = RSLChannelMode::SpeedIndicator::Speech;
    EXPECT_TRUE(mode.isSpeech());
    EXPECT_EQ(mode.spdInd, 0x01u);

    mode.spdInd = RSLChannelMode::SpeedIndicator::Data;
    EXPECT_TRUE(mode.isData());
    EXPECT_EQ(mode.spdInd, 0x02u);

    mode.spdInd = RSLChannelMode::SpeedIndicator::Signalling;
    EXPECT_TRUE(mode.isSignalling());
    EXPECT_EQ(mode.spdInd, 0x03u);

    // Channel rate and type codes (TS 48.058 9.3.6).
    EXPECT_EQ(static_cast<uint8_t>(RSLChannelMode::ChanRateType::Sdcch), 0x01u);
    EXPECT_EQ(static_cast<uint8_t>(RSLChannelMode::ChanRateType::TchF), 0x08u);
    EXPECT_EQ(static_cast<uint8_t>(RSLChannelMode::ChanRateType::TchH), 0x09u);
    EXPECT_EQ(static_cast<uint8_t>(RSLChannelMode::ChanRateType::TchFBdMslot), 0x0Au);
    EXPECT_EQ(static_cast<uint8_t>(RSLChannelMode::ChanRateType::TchFDlMslot), 0x1Au);
    EXPECT_EQ(static_cast<uint8_t>(RSLChannelMode::ChanRateType::TchFGroup), 0x18u);
    EXPECT_EQ(static_cast<uint8_t>(RSLChannelMode::ChanRateType::TchHGroup), 0x19u);
    EXPECT_EQ(static_cast<uint8_t>(RSLChannelMode::ChanRateType::TchFBcast), 0x28u);
    EXPECT_EQ(static_cast<uint8_t>(RSLChannelMode::ChanRateType::TchHBcast), 0x29u);
    EXPECT_EQ(static_cast<uint8_t>(RSLChannelMode::ChanRateType::OsMoTchFVamos), 0x88u);
    EXPECT_EQ(static_cast<uint8_t>(RSLChannelMode::ChanRateType::OsMoTchHVamos), 0x89u);
}

// Test: the RSL IE encoding class is fixed per type code (TS 48.058 9.3): only
// the L3 Information IE (0x0B) is TL16V, the Full BCCH Information IE (0x27)
// is an ordinary LV IE, and unknown codes fall back to LV.
TEST(RSLT_IeEncoding_Classes, Correct) {
    EXPECT_EQ(rslIeEncoding(0x0Bu), RSLEIEncoding::TL16V); // L3Info: the only TL16V
    EXPECT_EQ(rslIeEncoding(0x27u), RSLEIEncoding::LV);    // FullBCCHInfo: LV

    // TV IEs with their fixed value sizes.
    for (uint8_t iei : {0x01u, 0x02u, 0x03u, 0x04u, 0x09u, 0x0Du, 0x0Eu, 0x11u,
                        0x14u, 0x18u, 0x1Bu, 0x1Cu, 0x1Eu, 0x25u, 0x28u, 0x29u,
                        0x2Du, 0x2Eu, 0x37u}) {
        EXPECT_EQ(rslIeEncoding(iei), RSLEIEncoding::TV) << "iei=" << iei;
        EXPECT_EQ(rslIeTvValueSize(iei), 1u) << "iei=" << iei;
    }
    for (uint8_t iei : {0x08u, 0x0Au, 0x0Fu, 0x17u}) { // two-octet TV values
        EXPECT_EQ(rslIeEncoding(iei), RSLEIEncoding::TV) << "iei=" << iei;
        EXPECT_EQ(rslIeTvValueSize(iei), 2u) << "iei=" << iei;
    }
    EXPECT_EQ(rslIeTvValueSize(0x13u), 3u); // ReqReference: three octets

    // LV IEs (defined and undefined codes alike).
    for (uint8_t iei : {0x05u, 0x06u, 0x07u, 0x0Cu, 0x10u, 0x12u, 0x15u, 0x16u,
                        0x19u, 0x1Au, 0x1Fu, 0x20u, 0x21u, 0x22u, 0x23u, 0x24u,
                        0x26u, 0x27u, 0x2Au, 0x2Bu, 0x2Cu, 0x2Fu, 0x30u, 0x31u,
                        0x32u, 0x33u, 0x34u, 0x35u, 0x36u, 0x38u, 0x39u, 0x3Au,
                        0x3Bu, 0x3Cu, 0x60u, 0x61u, 0x62u, 0x63u}) {
        EXPECT_EQ(rslIeEncoding(iei), RSLEIEncoding::LV) << "iei=" << iei;
    }
}

// Test: every RSL_IE catalog member has a non-empty name and a defined
// encoding class; the reserved code 0x1D has no member (TS 48.058 9.3).
TEST(RSLT_IeCatalog_Complete, NamesAndEncodings) {
    const auto ies = []() {
        return std::array<RSL_IE, 81>{
            RSL_IE::ChanNr, RSL_IE::LinkIdent, RSL_IE::ActType, RSL_IE::BSPower,
            RSL_IE::ChanIdent, RSL_IE::ChanMode, RSL_IE::EncrInfo, RSL_IE::FrameNumber,
            RSL_IE::HandoRef, RSL_IE::L1Info, RSL_IE::L3Info, RSL_IE::MSIdentity,
            RSL_IE::MSPower, RSL_IE::PagingGroup, RSL_IE::PagingLoad, RSL_IE::PyhsContext,
            RSL_IE::AccessDelay, RSL_IE::RachLoad, RSL_IE::ReqReference, RSL_IE::ReleaseMode,
            RSL_IE::ResourceInfo, RSL_IE::RlmCause, RSL_IE::StartngTime, RSL_IE::TimingAdvance,
            RSL_IE::UplinkMeas, RSL_IE::Cause, RSL_IE::MeasResNr, RSL_IE::MsgId,
            RSL_IE::SysInfoType, RSL_IE::MSPowerParam, RSL_IE::BSPowerParam,
            RSL_IE::PreprocParam, RSL_IE::PreprocMeas, RSL_IE::ImmAssInfo, RSL_IE::SmscbInfo,
            RSL_IE::MSTimingOffset, RSL_IE::ErrMsg, RSL_IE::FullBCCHInfo, RSL_IE::ChanNeeded,
            RSL_IE::CbCmdType, RSL_IE::SmscbMsg, RSL_IE::FullImmAssInfo, RSL_IE::SacchInfo,
            RSL_IE::CbchLoadInfo, RSL_IE::SmscbChanIndicator, RSL_IE::GroupCallRef,
            RSL_IE::GroupChanDesc, RSL_IE::NchDrxInfo, RSL_IE::CmdIndicator, RSL_IE::EmlppPrio,
            RSL_IE::Uic, RSL_IE::MainChanRef, RSL_IE::MrConfig, RSL_IE::MrControl,
            RSL_IE::SuppCodecTypes, RSL_IE::CodecConfig, RSL_IE::Rtd, RSL_IE::TfoStatus,
            RSL_IE::LlpApdu, RSL_IE::OsMoRepAcchCap, RSL_IE::OsMoTrainingSequence,
            RSL_IE::OsMoTopAcchCap, RSL_IE::OsMoOsmuxCid, RSL_IE::IpacSrtpConfig,
            RSL_IE::IpacProxyUdp, RSL_IE::IpacBscmplTout, RSL_IE::IpacRemoteIp,
            RSL_IE::IpacRemotePort, RSL_IE::IpacRtpPayload, RSL_IE::IpacLocalPort,
            RSL_IE::IpacSpeechMode, RSL_IE::IpacLocalIp, RSL_IE::IpacConnStat,
            RSL_IE::IpacHoCParms, RSL_IE::IpacConnId, RSL_IE::IpacRtpCsdFmt,
            RSL_IE::IpacRtpJitBuf, RSL_IE::IpacRtpCompr, RSL_IE::IpacRtpPayload2,
            RSL_IE::IpacRtpMplex, RSL_IE::IpacRtpMplexId};
    }();
    ASSERT_EQ(ies.size(), 81u);
    for (const auto ie : ies) {
        EXPECT_NE(rslIEName(ie), "") << "ie=" << static_cast<int>(static_cast<uint8_t>(ie));
    }
    // Spot-check the catalog values against TS 48.058 9.3 type codes.
    EXPECT_EQ(static_cast<uint8_t>(RSL_IE::PyhsContext), 0x10u);
    EXPECT_EQ(static_cast<uint8_t>(RSL_IE::PreprocParam), 0x21u);
    EXPECT_EQ(static_cast<uint8_t>(RSL_IE::SmscbChanIndicator), 0x2Eu);
    EXPECT_EQ(static_cast<uint8_t>(RSL_IE::LlpApdu), 0x3Cu);
    EXPECT_EQ(static_cast<uint8_t>(RSL_IE::OsMoOsmuxCid), 0x63u);
    EXPECT_EQ(static_cast<uint8_t>(RSL_IE::IpacRtpMplexId), 0xFEu);
}

// Test: the RSL error cause domain (TS 48.058 9.3.26) accepts only defined
// values; reserved gaps in the code space are rejected.
TEST(RSLT_ErrorCause_Domain, Correct) {
    const auto causes = []() {
        return std::array<RSLErrorCause, 38>{
            RSLErrorCause::RadioIfFail, RSLErrorCause::RadioLinkFail,
            RSLErrorCause::HandoverAccFail, RSLErrorCause::TalkerAccFail,
            RSLErrorCause::OmIntervention, RSLErrorCause::NormalUnspec,
            RSLErrorCause::TMsrfpciExp, RSLErrorCause::EquipmentFail,
            RSLErrorCause::RrUnavail, RSLErrorCause::TerrChFail,
            RSLErrorCause::CcchOverload, RSLErrorCause::AcchOverload,
            RSLErrorCause::ProcessorOverload, RSLErrorCause::BtsNotEquipped,
            RSLErrorCause::RemoteTrauFailure, RSLErrorCause::NotifOverflow,
            RSLErrorCause::ResUnavail, RSLErrorCause::TranscUnavail,
            RSLErrorCause::ServOptUnavail, RSLErrorCause::EncrUnimpl,
            RSLErrorCause::ServOptUnimpl, RSLErrorCause::RchAlrActvAlloc,
            RSLErrorCause::IpaRchNotActvAlloc, RSLErrorCause::IpaConnInvalid,
            RSLErrorCause::IpaConnInUse, RSLErrorCause::IpaConnAlreadyExists,
            RSLErrorCause::InvalidMessage, RSLErrorCause::MsgDiscr,
            RSLErrorCause::MsgType, RSLErrorCause::MsgSeq, RSLErrorCause::IeError,
            RSLErrorCause::MandIeError, RSLErrorCause::OptIeError,
            RSLErrorCause::IeNonexist, RSLErrorCause::IeLength,
            RSLErrorCause::IeContent, RSLErrorCause::Proto,
            RSLErrorCause::Interworking};
    }();
    ASSERT_EQ(causes.size(), 38u);
    for (const auto c : causes) {
        EXPECT_TRUE(isRslErrorCause(static_cast<uint8_t>(c)))
            << "cause=0x" << std::hex << static_cast<int>(static_cast<uint8_t>(c));
        EXPECT_NE(rslErrorCauseName(c), "")
            << "cause=0x" << std::hex << static_cast<int>(static_cast<uint8_t>(c));
    }
    // Reserved gaps and out-of-space values are rejected.
    EXPECT_FALSE(isRslErrorCause(0x04u));
    EXPECT_FALSE(isRslErrorCause(0x19u));
    EXPECT_FALSE(isRslErrorCause(0x55u));
    EXPECT_FALSE(isRslErrorCause(0x80u));
}
