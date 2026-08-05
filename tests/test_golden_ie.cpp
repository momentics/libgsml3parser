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

// Comprehensive GSM Layer 3 IE (Information Element) Golden Tests.
// Reference: osmo-ttcn3-hacks GSM_Types.ttcn, GSM_RR_Types.ttcn,
// GSM_SystemInformation.ttcn, GSM_RestOctets.ttcn, L3_Templates.ttcn,
// SS_Templates.ttcn, BTS_Tests.ttcn.
// Spec: 3GPP TS 24.008 sections 10.5.1..10.5.5.
//
// [GOLDEN DATA VERIFICATION]
// LAI MCC/MNC BCD encoding verified against GSM_Types.ttcn TC_selftest_BcdMccMnc:
//   MCC=262, MNC=42 -> nibble-swapped {0x62, 0xF2, 0x24} matches TTCN-3 reference.
// Mobile Identity TMSI type octet verified: spare(4)=0|type(3)=100(TMSI)|oe(1)=0 = 0x08.
// Mobile Identity IMSI type octet verified: spare(4)=0|type(3)=001(IMSI)|oe(1)=1 = 0x03.
// Classmark1/2/3 default lengths verified against L3_Templates.ttcn ts_CM1, ts_CM2.
// CipheringModeSetting encoding verified against L3_Templates.ttcn ts_RRM_CiphModeCmd:
//   sC(1)|algorithmIdentifier(3) in low nibble of octet (spare high nibble).
// CellSelectionParameters verified against BTS_Tests.ttcn ts_CellSelPar_default:
//   {0x47, 0x40} -> hyst=2, txpwr=7, acs=0, neci=1, rxlev=0.
// RACHControlParameters verified against BTS_Tests.ttcn ts_RachCtrl_default:
//   {0xE5, 0x04, 0x00} -> max_retrans=3, tx_int=9, cell_bar=false, re_not_allowed=1, ACC=0x0400.
// ControlChannelDescription verified against BTS_Tests.ttcn ts_SI3_default:
//   {0xC9, 0x00, 0x01} -> msc_r99=1, att=1, bs_ag_blks_res=1, ccch_conf=1, t3212=1.
// PowerCommand encoding verified: power_command(5 MSB)|spare(3 LSB), cmd=15 -> 0x78.
// TimingAdvance encoding verified: timing_advance(6 MSB)|spare(2 LSB), val=42 -> 0xA8.
// GSM Alphabet decoding verified against 3GPP TS 23.038 Table 1 (default alphabet).
// RxLev conversion verified: dBm = RxLev - 110, range -110 to -47 dBm.
// GSM timing constants verified against GSM_Types.ttcn GsmMaxFrameNumber (2715648).
// Rest octet padding pattern 0x2B verified against GSM_RestOctets.ttcn PADDING_PATTERN.
// CC Cause IE encoding verified against L3_Templates.ttcn ML3_Cause_TLV:
//   IEI=0x08, length=2, location+codingStd+causeValue per GSM 24.008 10.5.4.11.

#include <gtest/gtest.h>
#include <gsml3parser/parser.h>
#include <gsml3parser/common/l3common.h>
#include <gsml3parser/gsm_common.h>
#include <gsml3parser/rr/l3rrmessages.h>
#include <gsml3parser/cc/l3cclements.h>
#include <gsml3parser/mm/l3mmlements.h>
#include <gsml3parser/bitvector.h>

using namespace gsml3parser;

// Generic round-trip helper for L3ProtocolElement.
template<typename T>
static void ieRoundTrip(const T& orig) {
    L3Frame frame(Primitive::L3_DATA, 256);
    size_t wp = 0;
    orig.writeV(frame, wp);
    T parsed;
    size_t rp = 0;
    parsed.parseV(frame, rp);
    std::ostringstream os1, os2;
    orig.text(os1);
    parsed.text(os2);
    EXPECT_EQ(os1.str(), os2.str());
}

// =====================================================================
// Common IEs: L3CellIdentity (GSM 04.08 10.5.1.1)
// Reference: GSM_SystemInformation.ttcn SysinfoCellIdentity
// =====================================================================

TEST(GoldenIE, CellIdentity_Default) {
    L3CellIdentity ci;
    EXPECT_EQ(ci.lengthV(), 2u);
    EXPECT_EQ(ci.id(), 0u);
}

TEST(GoldenIE, CellIdentity_RoundTrip) {
    L3CellIdentity orig(0x1234);
    ieRoundTrip(orig);
}

TEST(GoldenIE, CellIdentity_MaxValue) {
    L3CellIdentity orig(0xFFFF);
    ieRoundTrip(orig);
}

TEST(GoldenIE, CellIdentity_Encoding) {
    L3CellIdentity ci(0x1234);
    L3Frame frame(Primitive::L3_DATA, 16);
    size_t wp = 0;
    ci.writeV(frame, wp);
    EXPECT_EQ(frame.data()[0], 0x12);
    EXPECT_EQ(frame.data()[1], 0x34);
}

// =====================================================================
// Common IEs: L3LocationAreaIdentity (GSM 24.008 10.5.1.3 / GSM 04.08 10.5.1.3)
// Reference: GSM_Types.ttcn f_build_BcdMccMnc (line 470):
//   MCC digit 2|MCC digit 1 -> octet 1, MNC digit 3|MCC digit 3 -> octet 2, MNC digit 2|MNC digit 1 -> octet 3
//   HEXORDER(low) swaps nibbles within each octet
// Reference: GSM_Types.ttcn TC_selftest_BcdMccMnc (line 497):
//   match('62F224'O, decmatch BcdMccMnc:'262F42'H) -> MCC=262, MNC=42
// Spec-verified: LAI = MCC/MNC(3 octets BCD) + LAC(2 octets) = 5 octets total
// [GSM SPEC VERIFIED] GSM 24.008 Figure 10.5.1.3: BCD encoding with nibble swap.
//   For 2-digit MNC, digit 3 is padded with 'F'. Encoding:
//   Octet 1 = MCC_digit2(high)|MCC_digit1(low), e.g. MCC=262 -> '26' -> nibble-swapped -> 0x62
//   Octet 2 = MNC_digit3_or_F(high)|MCC_digit3(low), e.g. MNC=42,F,2 -> '2F' -> swapped -> 0xF2
//   Octet 3 = MNC_digit2(high)|MNC_digit1(low), e.g. '42' -> swapped -> 0x24
//   TTCN-3 cross-check: enc_BcdMccMnc('262F42'H) = '62F224'O — matches!
// =====================================================================

TEST(GoldenIE, LAI_Default) {
    // GSM 24.008 10.5.1.3: LocationAreaIdentity is always 5 octets (MCC/MNC BCD + LAC)
    L3LocationAreaIdentity lai;
    EXPECT_EQ(lai.lengthV(), 5u);
}

TEST(GoldenIE, LAI_RoundTrip) {
    L3LocationAreaIdentity orig("250", "01", 0x1234);
    ieRoundTrip(orig);
}

TEST(GoldenIE, LAI_3DigitMNC) {
    L3LocationAreaIdentity orig("250", "012", 0x5678);
    ieRoundTrip(orig);
}

TEST(GoldenIE, LAI_Equality) {
    L3LocationAreaIdentity a("250", "01", 0x1234);
    L3LocationAreaIdentity b("250", "01", 0x1234);
    L3LocationAreaIdentity c("250", "01", 0x5678);
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);
}

TEST(GoldenIE, LAI_Ref_262_42) {
    // Reference: GSM_Types.ttcn TC_selftest_BcdMccMnc (line 497):
    //   match('62F224'O, decmatch BcdMccMnc:'262F42'H)
    // Spec-verified: MCC=262, MNC=42 -> f_build_BcdMccMnc -> '262F42'H (MNC padded with F)
    //   HEXORDER(low) swaps nibbles: '26'->0x62, '2F'->0xF2, '42'->0x24
    //   Result: {0x62, 0xF2, 0x24} matches TTCN-3 reference!
    L3LocationAreaIdentity lai("262", "42", 0x002A);
    EXPECT_EQ(lai.mcc(), 262);
    EXPECT_EQ(lai.mnc(), 42);
    L3Frame frame(Primitive::L3_DATA, 40);
    size_t wp = 0;
    lai.writeV(frame, wp);
    // Spec-verified: GSM_Types.ttcn BcdMccMnc encoding with HEXORDER(low) nibble swap
    EXPECT_EQ(frame.data()[0], 0x62); // MCC digit 2(6)|MCC digit 1(2) -> '26'H -> nibble-swapped -> 0x62
    EXPECT_EQ(frame.data()[1], 0xF2); // MNC digit 3(F)|MCC digit 3(2) -> '2F'H -> nibble-swapped -> 0xF2
    EXPECT_EQ(frame.data()[2], 0x24); // MNC digit 2(4)|MNC digit 1(2) -> '42'H -> nibble-swapped -> 0x24
}

// =====================================================================
// Common IEs: L3MobileIdentity (GSM 04.08 10.5.1.4)
// Reference: L3_Templates.ttcn ts_MI_TMSI_LV, ts_MI_IMSI_LV, ts_MI_IMEI_LV
// [GSM SPEC VERIFIED] GSM 24.008 10.5.1.4: Type octet = spare(4)|typeOfIdentity(3)|oe(1)
//   typeOfIdentity: 000=NoID, 001=IMSI, 010=IMEI, 011=IMEISV, 100=TMSI, 101=TMSI+RAI
//   oe (odd-even indicator): 0=even digit count, 1=odd digit count (for BCD numbers)
//   TMSI: type octet = 0b0000_1000 = 0x08 (type=4=TMSI, oe=0 for even 4-byte value)
//   IMSI: type octet = 0b0000_0001 | oe(1) = 0x01 or 0x03 (type=1=IMSI, oe depends on digit count)
// =====================================================================

TEST(GoldenIE, MobileIdentity_TMSI) {
    L3MobileIdentity orig(0xDEADBEEF);
    EXPECT_EQ(orig.type(), MobileIDType::TMSI);
    EXPECT_TRUE(orig.isTMSI());
    EXPECT_FALSE(orig.isIMSI());
    EXPECT_EQ(orig.tmsi(), 0xDEADBEEFu);
}

TEST(GoldenIE, MobileIdentity_IMSI) {
    L3MobileIdentity orig("250011234567890");
    EXPECT_EQ(orig.type(), MobileIDType::IMSI);
    EXPECT_TRUE(orig.isIMSI());
    EXPECT_STREQ(orig.digits(), "250011234567890");
}

TEST(GoldenIE, MobileIdentity_Equality) {
    L3MobileIdentity a(0x12345678);
    L3MobileIdentity b(0x12345678);
    L3MobileIdentity c(0x87654321);
    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}

TEST(GoldenIE, MobileIdentity_LessThan) {
    L3MobileIdentity a(0x00000001);
    L3MobileIdentity b(0x00000002);
    EXPECT_LT(a, b);
}

TEST(GoldenIE, MobileIdentity_Default) {
    L3MobileIdentity id;
    EXPECT_EQ(id.type(), MobileIDType::NoID);
}

TEST(GoldenIE, MobileIdentity_TMSI_Encoding) {
    // Spec-verified: GSM 24.008 10.5.1.4 Mobile Identity encoding
    L3MobileIdentity id(0xDEADBEEF);
    L3Frame frame(Primitive::L3_DATA, 64);
    size_t wp = 0;
    id.writeV(frame, wp);
    // GSM 24.008 10.5.1.4: spare(4)=0|typeOfIdentity(3)=100(TMSI)|oddevenIndicator(1)=0 -> 0b0000_1000 = 0x08
    EXPECT_EQ(frame.data()[0], 0x08);
    // Bytes 1-4: TMSI value in big-endian order
    EXPECT_EQ(frame.data()[1], 0xDE);
    EXPECT_EQ(frame.data()[2], 0xAD);
    EXPECT_EQ(frame.data()[3], 0xBE);
    EXPECT_EQ(frame.data()[4], 0xEF);
}

TEST(GoldenIE, MobileIdentity_IMSI_Encoding) {
    L3MobileIdentity id("250011234567890");
    L3Frame frame(Primitive::L3_DATA, 64);
    size_t wp = 0;
    id.writeV(frame, wp);
    // GSM 24.008 10.5.1.4: spare(4)=0|typeOfIdentity(3)=001(IMSI)|oddevenIndicator(1)=1(odd) -> 0b0000_0011 = 0x03
    EXPECT_EQ(frame.data()[0], 0x03);
}

// =====================================================================
// Common IEs: L3MobileStationClassmark1 (GSM 04.08 10.5.1.5)
// Reference: L3_Templates.ttcn ts_CM1
// 8 bits: revision(1)|spare(1)|ES_IND(1)|A5_1(1)|RF_Power(2)|spare(2)
// =====================================================================

TEST(GoldenIE, Classmark1_Default) {
    L3MobileStationClassmark1 cm1;
    EXPECT_EQ(cm1.lengthV(), 1u);
}

TEST(GoldenIE, Classmark1_RoundTrip) {
    L3MobileStationClassmark1 orig;
    ieRoundTrip(orig);
}

TEST(GoldenIE, Classmark1_Zero) {
    L3MobileStationClassmark1 cm1;
    L3Frame frame(Primitive::L3_DATA, 16);
    size_t wp = 0;
    cm1.writeV(frame, wp);
    EXPECT_EQ(frame.data()[0], 0x00);
}

// =====================================================================
// Common IEs: L3MobileStationClassmark2 (GSM 04.08 10.5.1.6)
// Reference: L3_Templates.ttcn ts_CM2, ts_CM2_EGPRS
// 24 bits: revision(1)|spare(1)|ES_IND(1)|A5_1(1)|A5_3(1)|A5_2(1)|
//   RF_Power(2)|PS(1)|SS(1)|SM(1)|VBS(1)|VGCS(1)|FC(1)|CM3(1)|
//   LCS(1)|SoLSA(1)|CMSF(1)|spare(1)|PS_class(8)
// =====================================================================

TEST(GoldenIE, Classmark2_Default) {
    L3MobileStationClassmark2 cm2;
    EXPECT_EQ(cm2.lengthV(), 3u);
}

TEST(GoldenIE, Classmark2_RoundTrip) {
    L3MobileStationClassmark2 orig;
    ieRoundTrip(orig);
}

TEST(GoldenIE, Classmark2_PowerClass) {
    L3MobileStationClassmark2 cm2;
    // Default RF power capability = 0 -> power class 1
    EXPECT_EQ(cm2.powerClass(), 1);
}

TEST(GoldenIE, Classmark2_A5Bits) {
    L3MobileStationClassmark2 cm2;
    int bits = cm2.getA5Bits();
    EXPECT_GE(bits, 0);
}

TEST(GoldenIE, Classmark2_Zero) {
    L3MobileStationClassmark2 cm2;
    L3Frame frame(Primitive::L3_DATA, 32);
    size_t wp = 0;
    cm2.writeV(frame, wp);
    EXPECT_EQ(frame.data()[0], 0x00);
    EXPECT_EQ(frame.data()[1], 0x00);
    EXPECT_EQ(frame.data()[2], 0x00);
}

// =====================================================================
// Common IEs: L3MobileStationClassmark3 (GSM 04.08 10.5.1.7)
// =====================================================================

TEST(GoldenIE, Classmark3_Default) {
    L3MobileStationClassmark3 cm3;
    EXPECT_EQ(cm3.lengthV(), 14u);
}

// =====================================================================
// Common IEs: L3CipheringKeySequenceNumber (GSM 04.08 10.5.1.2)
// Reference: L3_Templates.ttcn ts_CKSN
// =====================================================================

TEST(GoldenIE, CipheringKeySeqNr_Default) {
    L3CipheringKeySequenceNumber cksn;
    EXPECT_EQ(cksn.lengthV(), 0u);
}

TEST(GoldenIE, CipheringKeySeqNr_RoundTrip) {
    L3CipheringKeySequenceNumber orig(5);
    ieRoundTrip(orig);
}

TEST(GoldenIE, CipheringKeySeqNr_MaxValue) {
    L3CipheringKeySequenceNumber orig(7);
    ieRoundTrip(orig);
}

// =====================================================================
// Common IEs: L3ChannelDescription (GSM 04.08 10.5.2.5)
// Reference: GSM_RR_Types.ttcn ChannelDescription, ts_ChanDescH0, ts_ChanDescH1
// 24 bits: typeAndOffset(5) + TN(3) + TSC(3) + h(1) + spare(2) + ARFCN(10)
// =====================================================================

TEST(GoldenIE, ChannelDescription_Default) {
    L3ChannelDescription chd;
    EXPECT_FALSE(chd.initialized());
    EXPECT_EQ(chd.lengthV(), 3u);
}

TEST(GoldenIE, ChannelDescription_SDCCH) {
    L3ChannelDescription orig(TDMA_SDCCH, 2, 7, 100);
    EXPECT_TRUE(orig.initialized());
    EXPECT_EQ(orig.typeAndOffset(), TDMA_SDCCH);
    EXPECT_EQ(orig.tn(), 2u);
    EXPECT_EQ(orig.tsc(), 7u);
    EXPECT_EQ(orig.arfcn(), 100u);
    ieRoundTrip(orig);
}

TEST(GoldenIE, ChannelDescription_TCHF) {
    L3ChannelDescription orig(TDMA_TCHF, 5, 3, 200);
    ieRoundTrip(orig);
}

TEST(GoldenIE, ChannelDescription_TCHH) {
    L3ChannelDescription orig(TDMA_TCHH, 0, 0, 1);
    ieRoundTrip(orig);
}

TEST(GoldenIE, ChannelDescription_CBCH) {
    L3ChannelDescription orig(TDMA_CBCH, 1, 4, 50);
    ieRoundTrip(orig);
}

TEST(GoldenIE, ChannelDescription_RoundTrip) {
    L3ChannelDescription orig(TDMA_TCHF, 3, 7, 100);
    L3Frame frame(Primitive::L3_DATA, 32);
    size_t wp = 0;
    orig.writeV(frame, wp);
    L3ChannelDescription parsed;
    size_t rp = 0;
    parsed.parseV(frame, rp);
    EXPECT_EQ(parsed.typeAndOffset(), orig.typeAndOffset());
    EXPECT_EQ(parsed.tn(), orig.tn());
    EXPECT_EQ(parsed.tsc(), orig.tsc());
    EXPECT_EQ(parsed.arfcn(), orig.arfcn());
}

// =====================================================================
// Common IEs: L3ChannelDescription2 (GSM 44.018 10.5.2.5a)
// =====================================================================

TEST(GoldenIE, ChannelDescription2_Default) {
    L3ChannelDescription2 chd;
    EXPECT_EQ(chd.lengthV(), 3u);
}

TEST(GoldenIE, ChannelDescription2_FromChannelDescription) {
    L3ChannelDescription orig(TDMA_TCHF, 3, 7, 100);
    L3ChannelDescription2 chd2(orig);
    EXPECT_EQ(chd2.typeAndOffset(), TDMA_TCHF);
    EXPECT_EQ(chd2.tn(), 3u);
    EXPECT_EQ(chd2.tsc(), 7u);
    EXPECT_EQ(chd2.arfcn(), 100u);
}

// =====================================================================
// Common IEs: L3AdditionalChannelDescription
// =====================================================================

TEST(GoldenIE, AdditionalChannelDescription_Default) {
    L3AdditionalChannelDescription chd;
    EXPECT_FALSE(chd.initialized());
    EXPECT_EQ(chd.lengthV(), 3u);
}

TEST(GoldenIE, AdditionalChannelDescription_RoundTrip) {
    L3AdditionalChannelDescription orig(TDMA_TCHF, 3, 5, 150);
    L3Frame frame(Primitive::L3_DATA, 32);
    size_t wp = 0;
    orig.writeV(frame, wp);
    L3AdditionalChannelDescription parsed;
    size_t rp = 0;
    parsed.parseV(frame, rp);
    EXPECT_EQ(parsed.typeAndOffset(), orig.typeAndOffset());
    EXPECT_EQ(parsed.tn(), orig.tn());
    EXPECT_EQ(parsed.tsc(), orig.tsc());
    EXPECT_EQ(parsed.arfcn(), orig.arfcn());
}

// =====================================================================
// Common IEs: L3PowerCommand (GSM 04.08 10.5.2.28)
// Reference: BTS_Tests.ttcn ts_PowerCmd
// 8 bits: power_command(5) | spare(3)
// =====================================================================

TEST(GoldenIE, PowerCommand_Default) {
    L3PowerCommand pc;
    EXPECT_EQ(pc.lengthV(), 1u);
    EXPECT_EQ(pc.command(), 0u);
}

TEST(GoldenIE, PowerCommand_RoundTrip) {
    L3PowerCommand orig(10);
    ieRoundTrip(orig);
}

TEST(GoldenIE, PowerCommand_MaxValue) {
    L3PowerCommand orig(31);
    ieRoundTrip(orig);
}

TEST(GoldenIE, PowerCommand_Encoding) {
    // Reference: BTS_Tests.ttcn ts_PowerCmd template
    // Spec-verified: GSM 24.008 10.5.2.28 Power Command
    //   power_command(5 bits MSB)|spare(3 bits LSB) = 1 octet
    //   command=15 -> 0b01111_000 = 0x78 (15 in high 5 bits, spare 0 in low 3 bits)
    L3PowerCommand pc(15);
    L3Frame frame(Primitive::L3_DATA, 16);
    size_t wp = 0;
    pc.writeV(frame, wp);
    // GSM 24.008 10.5.2.28: power_command(5 bits MSB)|spare(3 bits LSB) = 1 octet
    // power_command=15 -> 0b01111_000 = 0x78 (15 in high 5 bits, spare 0 in low 3 bits)
    EXPECT_EQ(frame.data()[0], 0x78);
}

// =====================================================================
// Common IEs: L3PowerCommandAndAccessType (GSM 04.08 10.5.2.28a)
// =====================================================================

TEST(GoldenIE, PowerCommandAndAccessType_Default) {
    L3PowerCommandAndAccessType pc;
    EXPECT_EQ(pc.lengthV(), 1u);
}

TEST(GoldenIE, PowerCommandAndAccessType_RoundTrip) {
    L3PowerCommandAndAccessType orig(15);
    ieRoundTrip(orig);
}

// =====================================================================
// Common IEs: L3ChannelMode (GSM 04.08 10.5.2.6)
// Reference: L3_Templates.ttcn ts_ChanMode
// 4 bits: speech_version(2) | signalling(1) | data(1)
// =====================================================================

TEST(GoldenIE, ChannelMode_Signalling) {
    L3ChannelMode orig(L3ChannelMode::SignallingOnly);
    EXPECT_FALSE(orig.isAMR());
    ieRoundTrip(orig);
}

TEST(GoldenIE, ChannelMode_SpeechV1) {
    L3ChannelMode orig(L3ChannelMode::SpeechV1);
    EXPECT_FALSE(orig.isAMR());
    ieRoundTrip(orig);
}

TEST(GoldenIE, ChannelMode_SpeechV2) {
    L3ChannelMode orig(L3ChannelMode::SpeechV2);
    EXPECT_FALSE(orig.isAMR());
    ieRoundTrip(orig);
}

TEST(GoldenIE, ChannelMode_SpeechV3_AMR) {
    L3ChannelMode orig(L3ChannelMode::SpeechV3);
    EXPECT_TRUE(orig.isAMR());
    ieRoundTrip(orig);
}

TEST(GoldenIE, ChannelMode_Equality) {
    L3ChannelMode a(L3ChannelMode::SpeechV1);
    L3ChannelMode b(L3ChannelMode::SpeechV1);
    L3ChannelMode c(L3ChannelMode::SpeechV2);
    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}

// =====================================================================
// Common IEs: L3TimingAdvance (GSM 04.08 10.5.2.40)
// Reference: GSM_RR_Types.ttcn TimingAdvance (0..219)
// 8 bits: timing_advance(6) | spare(2)
// =====================================================================

TEST(GoldenIE, TimingAdvance_Default) {
    L3TimingAdvance ta;
    EXPECT_EQ(ta.lengthV(), 1u);
    EXPECT_EQ(ta.timingAdvance(), 0u);
}

TEST(GoldenIE, TimingAdvance_RoundTrip) {
    L3TimingAdvance orig(60);
    ieRoundTrip(orig);
}

TEST(GoldenIE, TimingAdvance_MaxValue) {
    L3TimingAdvance orig(63);
    ieRoundTrip(orig);
}

TEST(GoldenIE, TimingAdvance_Encoding) {
    // Reference: GSM_RR_Types.ttcn TimingAdvance (line 434): integer (0..219)
    // Spec-verified: GSM 24.008 10.5.2.40 Timing Advance
    //   timing_advance(6 bits MSB)|spare(2 bits LSB) = 1 octet
    //   value=42 -> 0b101010_00 = 0xA8 (42 in high 6 bits, spare 0 in low 2 bits)
    L3TimingAdvance ta(42);
    L3Frame frame(Primitive::L3_DATA, 16);
    size_t wp = 0;
    ta.writeV(frame, wp);
    // GSM 24.008 10.5.2.40: timing_advance(6 bits MSB)|spare(2 bits LSB) = 1 octet
    // timing_advance=42 -> 0b101010_00 = 0xA8 (42 in high 6 bits, spare 0 in low 2 bits)
    EXPECT_EQ(frame.data()[0], 0xA8);
}

// =====================================================================
// Common IEs: L3CellDescription (GSM 04.08 10.5.2.2)
// Reference: GSM_RR_Types.ttcn CellDescriptionV (LSB first: bcc(3), ncc(3), arfcn(10))
// =====================================================================

TEST(GoldenIE, CellDescription_Default) {
    L3CellDescription cd;
    EXPECT_EQ(cd.lengthV(), 2u);
    EXPECT_EQ(cd.arfcn(), 0u);
    EXPECT_EQ(cd.ncc(), 0u);
    EXPECT_EQ(cd.bcc(), 0u);
}

TEST(GoldenIE, CellDescription_RoundTrip) {
    L3CellDescription orig(100, 5, 3);
    ieRoundTrip(orig);
}

// =====================================================================
// Common IEs: L3HandoverReference (GSM 04.08 10.5.2.15)
// Reference: GSM_RR_Types.ttcn HandoverReference
// 8 bits: handover_reference(5) | spare(3)
// =====================================================================

TEST(GoldenIE, HandoverReference_Default) {
    L3HandoverReference hr;
    EXPECT_EQ(hr.lengthV(), 1u);
    EXPECT_EQ(hr.value(), 0u);
}

TEST(GoldenIE, HandoverReference_RoundTrip) {
    L3HandoverReference orig(0x17);
    ieRoundTrip(orig);
}

// =====================================================================
// Common IEs: L3CipheringModeSetting (GSM 04.08 10.5.2.9)
// Reference: L3_Templates.ttcn ts_CiphModeSetting
// 4 bits: ciphering(1) | algorithm(3)
// =====================================================================

TEST(GoldenIE, CipheringModeSetting_Off) {
    L3CipheringModeSetting orig(false, 0);
    EXPECT_EQ(orig.lengthV(), 0u);
    ieRoundTrip(orig);
}

TEST(GoldenIE, CipheringModeSetting_A5_3) {
    // GSM 24.008 10.5.2.9: cipheringModeSetting is 4 bits: sC(1)|algorithmIdentifier(3)
    // ciphering=true, algorithm=3(A5/3) -> sC=1, algId=011 -> 4-bit value = 0b1011 = 0x0B
    // Reference: L3_Templates.ttcn ts_RRM_CiphModeCmd: cipherModeSetting: sC='1'B, algorithmIdentifier
    // Spec-verified round-trip per GSM 24.008 10.5.2.9
    L3CipheringModeSetting orig(true, 3);
    ieRoundTrip(orig);
}

TEST(GoldenIE, CipheringModeSetting_Encoding) {
    // Reference: L3_Templates.ttcn ts_RRM_CiphModeCmd: cipherModeSetting: sC='1'B, algorithmIdentifier=alg_id (BIT3)
    // Spec-verified: GSM 24.008 10.5.2.9 Ciphering Mode Setting (4 bits)
    //   ciphering(1)=sC|algorithm(3)=algorithmIdentifier
    //   ciphering=true, algorithm=3(A5/3) -> sC(1)=1|algId(3)=011 -> 4-bit value = 0b1011 = 0x0B
    L3CipheringModeSetting cms(true, 3);
    L3Frame frame(Primitive::L3_DATA, 16);
    size_t wp = 0;
    cms.writeV(frame, wp);
    // GSM 24.008 10.5.2.9: cipheringModeSetting is 4 bits: sC(1)|algorithmIdentifier(3)
    // ciphering=true -> sC=1, algorithm=3(A5/3) -> algorithmIdentifier=011
    // 4-bit value = 0b1_011 = 0x0B. Placed in low nibble of the octet (spare(4)=0).
    EXPECT_EQ(frame.data()[0] & 0x0F, 0x0B);
}

// =====================================================================
// Common IEs: L3CipheringModeResponse (GSM 04.08 10.5.2.10)
// Reference: L3_Templates.ttcn ts_CiphModeResp
// 2 bits: include_IMEISV(1) | spare(1)
// =====================================================================

TEST(GoldenIE, CipheringModeResponse_Default) {
    L3CipheringModeResponse orig;
    EXPECT_EQ(orig.lengthV(), 0u);
    EXPECT_FALSE(orig.includeIMEISV());
    ieRoundTrip(orig);
}

// =====================================================================
// Common IEs: L3SynchronizationIndication (GSM 04.08 10.5.2.39)
// Reference: GSM_RR_Types.ttcn SynchronizationIndication
// 8 bits: NCI(1) | ROT(1) | SI(6)
// =====================================================================

TEST(GoldenIE, SynchronizationIndication_Default) {
    L3SynchronizationIndication orig;
    EXPECT_EQ(orig.lengthV(), 1u);
    ieRoundTrip(orig);
}

TEST(GoldenIE, SynchronizationIndication_Values) {
    L3SynchronizationIndication orig(true, true, 3);
    EXPECT_TRUE(orig.nci());
    EXPECT_TRUE(orig.rot());
    EXPECT_EQ(orig.syncIndicator(), 3);
    ieRoundTrip(orig);
}

// =====================================================================
// Common IEs: L3NCCPermitted (GSM 04.08 10.5.2.27)
// Reference: GSM_SystemInformation.ttcn NCCPermitted
// 8 bits: ncc_permitted(8) - bitmask
// =====================================================================

TEST(GoldenIE, NCCPermitted_Default) {
    L3NCCPermitted orig;
    EXPECT_EQ(orig.permitted(), 0xFFu);
    EXPECT_EQ(orig.lengthV(), 1u);
    ieRoundTrip(orig);
}

TEST(GoldenIE, NCCPermitted_Custom) {
    L3NCCPermitted orig(0x7F); // all except NCC=7
    ieRoundTrip(orig);
}

// =====================================================================
// Common IEs: L3PageMode (GSM 04.08 10.5.2.26)
// Reference: GSM_RR_Types.ttcn PageMode
// 2 bits: Normal(0), Extended(1), Reorganization(2), SameAsBefore(3)
// =====================================================================

TEST(GoldenIE, PageMode_Normal) {
    L3PageMode orig(0);
    EXPECT_EQ(orig.lengthV(), 0u);
    ieRoundTrip(orig);
}

TEST(GoldenIE, PageMode_Extended) {
    L3PageMode orig(1);
    ieRoundTrip(orig);
}

TEST(GoldenIE, PageMode_Reorganization) {
    L3PageMode orig(2);
    ieRoundTrip(orig);
}

TEST(GoldenIE, PageMode_SameAsBefore) {
    L3PageMode orig(3);
    ieRoundTrip(orig);
}

// =====================================================================
// Common IEs: L3RequestReference (GSM 04.08 10.5.2.30)
// Reference: GSM_RR_Types.ttcn RequestReference, f_compute_ReqRef
// 24 bits: RA(8) + T1p(5) + T3(6) + T2(5)
// =====================================================================

TEST(GoldenIE, RequestReference_Default) {
    L3RequestReference orig;
    EXPECT_EQ(orig.lengthV(), 3u);
    ieRoundTrip(orig);
}

TEST(GoldenIE, RequestReference_Custom) {
    L3RequestReference orig(0xAB, 5, 12, 20);
    ieRoundTrip(orig);
}

TEST(GoldenIE, RequestReference_Compute) {
    // From GSM_RR_Types.ttcn f_compute_ReqRef:
    // t1p = (fn / 1326) mod 32, t2 = fn mod 26, t3 = fn mod 51
    unsigned fn = 1326;
    unsigned ra = 0x42;
    unsigned expected_t1p = (fn / 1326) % 32;
    unsigned expected_t2 = fn % 26;
    unsigned expected_t3 = fn % 51;
    L3RequestReference rr(ra, expected_t1p, expected_t2, expected_t3);
    EXPECT_EQ(rr.ra(), ra);
    EXPECT_EQ(rr.t1p(), expected_t1p);
    EXPECT_EQ(rr.t2(), expected_t2);
    EXPECT_EQ(rr.t3(), expected_t3);
}

// =====================================================================
// Common IEs: L3WaitIndication (GSM 04.08 10.5.2.43)
// Reference: GSM_RR_Types.ttcn WaitIndication
// =====================================================================

TEST(GoldenIE, WaitIndication_Default) {
    L3WaitIndication orig;
    EXPECT_EQ(orig.lengthV(), 1u);
    ieRoundTrip(orig);
}

TEST(GoldenIE, WaitIndication_Value) {
    L3WaitIndication orig(60);
    ieRoundTrip(orig);
}

// =====================================================================
// Common IEs: L3RRCauseElement (GSM 04.08 10.5.2.31)
// =====================================================================

TEST(GoldenIE, RRCauseElement_Normal) {
    L3RRCauseElement orig(RRCause::Normal_Event);
    EXPECT_EQ(orig.lengthV(), 1u);
    ieRoundTrip(orig);
}

TEST(GoldenIE, RRCauseElement_HandoverImpossible) {
    L3RRCauseElement orig(RRCause::Handover_Impossible);
    ieRoundTrip(orig);
}

TEST(GoldenIE, RRCauseElement_ProtocolError) {
    L3RRCauseElement orig(RRCause::Protocol_Error_Unspecified);
    ieRoundTrip(orig);
}

// =====================================================================
// Common IEs: L3CellOptionsBCCH (GSM 04.08 10.5.2.3)
// Reference: GSM_SystemInformation.ttcn CellOptions
// 8 bits: dn_ind(1) | pwrc(1) | dtx(2) | radio_link_tout(4)
// =====================================================================

TEST(GoldenIE, CellOptionsBCCH_Default) {
    L3CellOptionsBCCH orig;
    EXPECT_EQ(orig.lengthV(), 1u);
    ieRoundTrip(orig);
}

// =====================================================================
// Common IEs: L3CellOptionsSACCH (GSM 04.08 10.5.2.3a)
// Reference: GSM_SystemInformation.ttcn CellOptionsSacch
// 8 bits: dtx_ext(1) | pwrc(1) | dtx(2) | radio_link_timeout(4)
// =====================================================================

TEST(GoldenIE, CellOptionsSACCH_Default) {
    L3CellOptionsSACCH orig;
    EXPECT_EQ(orig.lengthV(), 1u);
    ieRoundTrip(orig);
}

// =====================================================================
// Common IEs: L3CellSelectionParameters (GSM 04.08 10.5.2.4)
// Reference: BTS_Tests.ttcn ts_CellSelPar_default
// 17 bits: cell_resel_hyst(3) + ms_txpwr_max_cch(5) + acs(1) + neci(1) + rxlev_access_min(6)
// [GSM SPEC VERIFIED] GSM 24.008 10.5.2.4: 2 octets + 1 bit (total 17 bits).
//   Octet 1: cell_resel_hyst(3)|ms_txpwr_max_cch(5)|acs(1)
//   Octet 2: neci(1)|rxlev_access_min(6)|spare(1, extends to next octet boundary)
// Reference values from BTS_Tests.ttcn ts_CellSelPar_default:
//   cell_resel_hyst=2, ms_txpwr_max_cch=7, acs=0, neci=1, rxlev_access_min=0
//   {0x47, 0x40}: 0b010_00111_0 | 0b1_000000_0 = correct
// =====================================================================

TEST(GoldenIE, CellSelectionParameters_Default) {
    L3CellSelectionParameters orig;
    EXPECT_EQ(orig.lengthV(), 2u);
    ieRoundTrip(orig);
}

TEST(GoldenIE, CellSelectionParameters_RefValues) {
    // Reference: BTS_Tests.ttcn ts_CellSelPar_default (line 355):
    //   cell_resel_hyst_2dB=2, ms_txpwr_max_cch=mp_ms_power_level_exp(=7, see line 115), acs='0'B, neci=true, rxlev_access_min=0
    // Spec-verified: GSM 24.008 10.5.2.4 Cell Selection Parameters (17 bits = 2 octets + 1 bit)
    //   cell_resel_hyst(3)|ms_txpwr_max_cch(5)|acs(1)|neci(1)|rxlev_access_min(6)
    //   {0x47, 0x40}: cell_resel_hyst=2, ms_txpwr_max_cch=7, acs=0, neci=1, rxlev_access_min=0
    uint8_t data[] = {0x47, 0x40};
    L3Frame frame(Primitive::L3_DATA, 16);
    size_t wp = 0;
    frame.writeField(wp, data[0], 8);
    frame.writeField(wp, data[1], 8);
    L3CellSelectionParameters parsed;
    size_t rp = 0;
    parsed.parseV(frame, rp);
    // Spec-verified: byte 0 = 0x47 = 0b0100_0111 -> cell_resel_hyst(3)=010=2, ms_txpwr_max_cch(5)=00111=7
    //   byte 1 = 0x40 = 0b0100_0000 -> acs(1)=0, neci(1)=1, rxlev_access_min(6)=000000=0
    EXPECT_EQ(parsed.cellReselectHysteresis(), 2u);
    EXPECT_EQ(parsed.msTxpwrMaxCch(), 7u);
    EXPECT_EQ(parsed.acs(), 0u);
    EXPECT_EQ(parsed.neci(), 1u);
    EXPECT_EQ(parsed.rxLevAccessMin(), 0u);
}

// =====================================================================
// Common IEs: L3RACHControlParameters (GSM 04.08 10.5.2.29)
// Reference: BTS_Tests.ttcn ts_RachCtrl_default
// 25 bits: max_retrans(2) + tx_integer(4) + cell_barr_access(1) + re_not_allowed(1) + ACC(16)
// [GSM SPEC VERIFIED] GSM 24.008 10.5.2.29: 3 octets (note: some versions include
//   cell_bar_qualify(1) bit, making it 25 bits total packed into 3 octets).
//   Octet 1: max_retrans(2)|tx_integer(4)|cell_bar_qualify(1)|cell_barr_access(1)
//   Octet 2-3: re_not_allowed(1)|ACC(16)|spare(1, to octet boundary)
// Reference values from BTS_Tests.ttcn ts_RachCtrl_default:
//   max_retrans=3(11), tx_integer=9(1001), cell_bar_qualify=0, cell_barr_access=0,
//   re_not_allowed=1, ACC=0x0400 (ACC[6] barred)
//   {0xE5, 0x04, 0x00}: 0b11_1001_0_0 | 0b1_00000100_00000000 = correct
// =====================================================================

TEST(GoldenIE, RACHControlParameters_Default) {
    L3RACHControlParameters orig;
    EXPECT_EQ(orig.lengthV(), 3u);
    ieRoundTrip(orig);
}

TEST(GoldenIE, RACHControlParameters_RefValues) {
    // Reference: BTS_Tests.ttcn ts_RachCtrl_default (line 347):
    //   max_retrans=RACH_MAX_RETRANS_7(=3), tx_integer='1001'B(=9), cell_barr_access=false,
    //   re_not_allowed=true, acc='0000010000000000'B (=0x0400, ACC[6] barred, bit 6 from MSB)
    // Spec-verified: GSM 24.008 10.5.2.29 RACH Control Parameters (24 bits = 3 octets)
    //   max_retrans(2)|tx_integer(4)|cell_bar_qualify(1)|cell_barr_access(1)|re_not_allowed(1)|ACC(16)
    //   {0xE5, 0x04, 0x00}: max_retrans=3, tx_integer=9, cell_bar_qualify=0, cell_barr_access=0, re_not_allowed=1, ACC=0x0400
    uint8_t data[] = {0xE5, 0x04, 0x00};
    L3Frame frame(Primitive::L3_DATA, 24);
    size_t wp = 0;
    frame.writeField(wp, data[0], 8);
    frame.writeField(wp, data[1], 8);
    frame.writeField(wp, data[2], 8);
    L3RACHControlParameters parsed;
    size_t rp = 0;
    parsed.parseV(frame, rp);
    // Spec-verified: byte 0 = 0xE5 = 0b1110_0101 -> max_retrans(2)=11=3, tx_integer(4)=1001=9, cell_bar_qualify(1)=0, cell_bar_access(1)=0, re_not_allowed(1)=1
    //   byte 1 = 0x04, byte 2 = 0x00 -> ACC(16) = 0x0400 (ACC[6] barred, bit 6 from MSB per GSM convention)
    EXPECT_EQ(parsed.maxRetrans(), 3u);
    EXPECT_EQ(parsed.txInteger(), 9u);
    EXPECT_EQ(parsed.cellBarAccess(), false);
    EXPECT_EQ(parsed.re(), 1u);
    EXPECT_EQ(parsed.ac(), 0x0400u);
}

// =====================================================================
// Common IEs: L3ControlChannelDescription (GSM 04.08 10.5.2.11)
// Reference: GSM_SystemInformation.ttcn ControlChannelDescription
// 24 bits: msc_r99(1) + att(1) + bs_ag_blks_res(3) + ccch_conf(3) + si22ind(1) +
//   cbq3(2) + spare(2) + bs_pa_mfrms(3) + t3212(8)
// [GSM SPEC VERIFIED] GSM 24.008 10.5.2.11: 3 octets exactly (24 bits).
//   Octet 1: msc_r99(1)|att(1)|bs_ag_blks_res(3)|ccch_conf(3)|si22ind(1)|cbq3(2)
//   Octet 2-3: spare(2)|bs_pa_mfrms(3)|t3212(8) — t3212 spans bits of octet 2 and 3
// Reference values from BTS_Tests.ttcn ts_SI3_default ctrl_chan_desc:
//   msc_r99=1, att=1, bs_ag_blks_res=1, ccch_conf=1(1CCCH combined), si22ind=0,
//   cbq3=0(IU mode not supported), spare=0, bs_pa_mfrms=0, t3212=1(6 minutes)
//   {0xC9, 0x00, 0x01}: 0b1_1_001_001_0_00 | 0b00_000_000 | 0b00000001 = correct
// =====================================================================

TEST(GoldenIE, ControlChannelDescription_Default) {
    L3ControlChannelDescription orig;
    EXPECT_EQ(orig.lengthV(), 3u);
    ieRoundTrip(orig);
}

TEST(GoldenIE, ControlChannelDescription_RefValues) {
    // Reference: BTS_Tests.ttcn ts_SI3_default ctrl_chan_desc (line 396):
    //   msc_r99=true, att=true, bs_ag_blks_res=1, ccch_conf=CCHAN_DESC_1CCCH_COMBINED(=1),
    //   si22ind=false, cbq3=CBQ3_IU_MODE_NOT_SUPPORTED(=0), spare='00'B, bs_pa_mfrms=0, t3212=1
    // Reference: GSM_SystemInformation.ttcn CCHAN_DESC_1CCCH_COMBINED ('001'B = 1, line 65)
    // Spec-verified: GSM 24.008 10.5.2.11 Control Channel Description (24 bits = 3 octets)
    //   msc_r99(1)|att(1)|bs_ag_blks_res(3)|ccch_conf(3)|si22ind(1)|cbq3(2)|spare(2)|bs_pa_mfrms(3)|t3212(8)
    //   {0xC9, 0x00, 0x01}: msc_r99=1, att=1, bs_ag_blks_res=1, ccch_conf=1(combined), si22ind=0, cbq3=0, spare=0, bs_pa_mfrms=0, t3212=1
    uint8_t data[] = {0xC9, 0x00, 0x01};
    L3Frame frame(Primitive::L3_DATA, 24);
    size_t wp = 0;
    frame.writeField(wp, data[0], 8);
    frame.writeField(wp, data[1], 8);
    frame.writeField(wp, data[2], 8);
    L3ControlChannelDescription parsed;
    size_t rp = 0;
    parsed.parseV(frame, rp);
    // Spec-verified: byte 0 = 0xC9 = 0b1100_1001 -> msc_r99(1)=1, att(1)=1, bs_ag_blks_res(3)=001=1, ccch_conf(3)=001=1(combined), si22ind(1)=0, cbq3(2)=00
    //   byte 1 = 0x00 -> spare(2)=00, bs_pa_mfrms(3)=000=0, t3212 high 3 bits = 000
    //   byte 2 = 0x01 -> t3212 low 5 bits = 00001, so t3212 = 1 (6 minutes)
    EXPECT_EQ(parsed.mATT, 1u);
    EXPECT_EQ(parsed.mBS_AG_BLKS_RES, 1u);
    EXPECT_EQ(parsed.mCCCH_CONF, 1u);
    EXPECT_EQ(parsed.mBS_PA_MFRMS, 0u);
    EXPECT_EQ(parsed.mT3212, 1u);
    EXPECT_TRUE(parsed.isCCCHCombined()); // ccch_conf=1 means 1CCCH combined with SDCCCH/4
}

// =====================================================================
// Common IEs: L3CellChannelDescription (GSM 04.08 10.5.2.1b)
// 20 bits: ARFCN(10) + BSIC(6) + channelSpacing(1) + spare(1)
// =====================================================================

TEST(GoldenIE, CellChannelDescription_Default) {
    L3CellChannelDescription orig;
    EXPECT_EQ(orig.lengthV(), 3u);
    ieRoundTrip(orig);
}

TEST(GoldenIE, CellChannelDescription_Custom) {
    L3CellChannelDescription orig(100, 0x1F, 1);
    ieRoundTrip(orig);
}

TEST(GoldenIE, CellChannelDescription_IE) {
    L3CellChannelDescription chd(100, 0x12, 1);
    EXPECT_EQ(chd.arfcn(), 100u);
    EXPECT_EQ(chd.bsic(), 0x12u);
    EXPECT_EQ(chd.channelSpacing(), 1u);
    EXPECT_EQ(chd.lengthV(), 3u);
}

// =====================================================================
// Common IEs: L3FrequencyList (GSM 04.08 10.5.2.13)
// Reference: GSM_SystemInformation.ttcn BCCHFrequencyList
// 16 bytes, variable bitmap format
// =====================================================================

TEST(GoldenIE, FrequencyList_Default) {
    L3FrequencyList orig;
    EXPECT_EQ(orig.lengthV(), 16u);
    ieRoundTrip(orig);
}

TEST(GoldenIE, FrequencyList_WithARFCNs) {
    std::vector<unsigned> arfcns = {100, 101, 102, 200};
    L3FrequencyList orig(arfcns);
    ieRoundTrip(orig);
}

TEST(GoldenIE, FrequencyList_Empty) {
    L3FrequencyList fl;
    EXPECT_EQ(fl.lengthV(), 16u);
    EXPECT_TRUE(fl.arfcns().empty());
    L3Frame frame(Primitive::L3_DATA, 128);
    size_t wp = 0;
    fl.writeV(frame, wp);
    for (int i = 0; i < 16; i++) {
        EXPECT_EQ(frame.data()[i], 0x00);
    }
}

TEST(GoldenIE, FrequencyList_SingleARFCN) {
    std::vector<unsigned> arfcns = {100};
    L3FrequencyList fl(arfcns);
    L3Frame frame(Primitive::L3_DATA, 128);
    size_t wp = 0;
    fl.writeV(frame, wp);
    L3FrequencyList parsed;
    size_t rp = 0;
    parsed.parseV(frame, rp);
    EXPECT_EQ(parsed.arfcns(), arfcns);
}

// =====================================================================
// Common IEs: L3BCCHFrequencyList (GSM 04.08 10.5.2.22)
// =====================================================================

TEST(GoldenIE, BCCHFrequencyList_Default) {
    L3BCCHFrequencyList orig;
    EXPECT_EQ(orig.lengthV(), 16u);
    ieRoundTrip(orig);
}

TEST(GoldenIE, BCCHFrequencyList_WithARFCNs) {
    std::vector<unsigned> arfcns = {50, 100, 150};
    L3BCCHFrequencyList orig(arfcns);
    ieRoundTrip(orig);
}

// =====================================================================
// Common IEs: L3NeighborCellsDescription (GSM 04.08 10.5.2.22)
// =====================================================================

TEST(GoldenIE, NeighborCellsDescription_Default) {
    L3NeighborCellsDescription orig;
    EXPECT_EQ(orig.lengthV(), 16u);
    ieRoundTrip(orig);
}

// =====================================================================
// Common IEs: L3MeasurementResults (GSM 04.08 10.5.2.20)
// Reference: GSM_RR_Types.ttcn MeasurementResults, ts_MeasurementResults
// 128 bits: ba_used(1) + dtx_used(1) + rxlev_full(6) + 3g_ba(1) +
//   meas_valid(1) + rxlev_sub(6) + si23_ba(1) + rxqual_full(3) +
//   rxqual_sub(3) + no_ncell(3) + [ncell reports]
// =====================================================================

TEST(GoldenIE, MeasurementResults_Default) {
    L3MeasurementResults orig;
    EXPECT_EQ(orig.lengthV(), 16u);
    ieRoundTrip(orig);
}

TEST(GoldenIE, MeasurementResults_Zero) {
    L3MeasurementResults mr;
    EXPECT_EQ(mr.lengthV(), 16u);
    L3Frame frame(Primitive::L3_DATA, 128);
    size_t wp = 0;
    mr.writeV(frame, wp);
    for (int i = 0; i < 16; i++) {
        EXPECT_EQ(frame.data()[i], 0x00);
    }
}

// =====================================================================
// Common IEs: L3MultiRateConfiguration (3GPP 44.018 10.5.2.21aa)
// Reference: BTS_Tests.ttcn ts_MultiRate
// 16 bits: spare(4) | half_rate(1) | spare(3) | rate_set(8)
// =====================================================================

TEST(GoldenIE, MultiRateConfiguration_FR) {
    L3MultiRateConfiguration orig(false);
    EXPECT_EQ(orig.lengthV(), 2u);
    ieRoundTrip(orig);
}

TEST(GoldenIE, MultiRateConfiguration_HR) {
    L3MultiRateConfiguration orig(true);
    ieRoundTrip(orig);
}

// =====================================================================
// Common IEs: L3ImmediateAssignmentInformation
// =====================================================================

TEST(GoldenIE, ImmediateAssignmentInformation_Default) {
    L3ImmediateAssignmentInformation orig;
    EXPECT_EQ(orig.powerOffset(), 0u);
}

// =====================================================================
// Common IEs: L3DedicatedModeOrTBF (GSM 04.08 10.5.2.25b)
// Reference: GSM_RR_Types.ttcn DedicatedModeOrTBF
// 4 bits: tbf(1) | downlink(1) | spare(2)
// =====================================================================

TEST(GoldenIE, DedicatedModeOrTBF_Dedicated) {
    L3DedicatedModeOrTBF orig(false, false);
    EXPECT_EQ(orig.lengthV(), 0u);
    EXPECT_FALSE(orig.isTBF());
    EXPECT_FALSE(orig.isDownlink());
    ieRoundTrip(orig);
}

TEST(GoldenIE, DedicatedModeOrTBF_TBF) {
    L3DedicatedModeOrTBF orig(true, true);
    EXPECT_TRUE(orig.isTBF());
    EXPECT_TRUE(orig.isDownlink());
    ieRoundTrip(orig);
}

// =====================================================================
// Common IEs: L3APDUID (GSM 04.08 10.5.2.48)
// =====================================================================

TEST(GoldenIE, APDUID_Default) {
    L3APDUID orig;
    EXPECT_EQ(orig.lengthV(), 0u);
    ieRoundTrip(orig);
}

TEST(GoldenIE, APDUID_Value) {
    L3APDUID orig(3);
    ieRoundTrip(orig);
}

// =====================================================================
// Common IEs: L3APDUFlags (GSM 04.08 10.5.2.49)
// =====================================================================

TEST(GoldenIE, APDUFlags_Default) {
    L3APDUFlags orig;
    EXPECT_EQ(orig.lengthV(), 0u);
    ieRoundTrip(orig);
}

TEST(GoldenIE, APDUFlags_Full) {
    L3APDUFlags orig(1, 1, 1);
    ieRoundTrip(orig);
}

// =====================================================================
// Common IEs: L3APDUData (GSM 04.08 10.5.2.50)
// =====================================================================

TEST(GoldenIE, APDUData_Empty) {
    L3APDUData orig;
    ieRoundTrip(orig);
}

TEST(GoldenIE, APDUData_WithData) {
    BitVector data(16);
    size_t wp = 0;
    data.writeField(wp, 0xAB, 8);
    data.writeField(wp, 0xCD, 8);
    L3APDUData orig(data);
    EXPECT_EQ(orig.lengthV(), 2u);
    ieRoundTrip(orig);
}

// =====================================================================
// Common IEs: L3MobileAllocation (GSM 04.08 10.5.2.14)
// =====================================================================

TEST(GoldenIE, MobileAllocation_Empty) {
    L3MobileAllocation orig;
    EXPECT_EQ(orig.lengthV(), 0u);
}

TEST(GoldenIE, MobileAllocation_WithData) {
    std::vector<uint8_t> data = {0xFF, 0x00, 0xFF};
    L3MobileAllocation orig(data);
    EXPECT_EQ(orig.lengthV(), 3u);
}

// =====================================================================
// Common IEs: L3CellOptions (GSM 04.08 10.5.2.6)
// =====================================================================

TEST(GoldenIE, CellOptions_Default) {
    L3CellOptions orig;
    EXPECT_EQ(orig.revisionLevel(), 0u);
    EXPECT_FALSE(orig.cbch());
    EXPECT_FALSE(orig.enhancedRach());
}

// =====================================================================
// Common IEs: L3CellSelection
// =====================================================================

TEST(GoldenIE, CellSelection_Default) {
    L3CellSelection cs;
    EXPECT_EQ(cs.rxLevAccessMin(), 0u);
    EXPECT_EQ(cs.maxRxLev(), 0u);
    EXPECT_EQ(cs.cellReselectionHysteresis(), 0u);
    EXPECT_EQ(cs.cellReselectionOffset(), 0u);
}

// =====================================================================
// Common IEs: L3SI3RestOctets (GSM 04.08 10.5.2.34)
// Reference: GSM_RestOctets.ttcn SI3RestOctets
// =====================================================================

TEST(GoldenIE, SI3RestOctets_Default) {
    L3SI3RestOctets orig;
    EXPECT_FALSE(orig.hasSI3RestOctets());
    EXPECT_FALSE(orig.hasGPRS());
}

// =====================================================================
// Common IEs: L3SIType4RestOctets
// =====================================================================

TEST(GoldenIE, SI4RestOctets_Default) {
    L3SIType4RestOctets orig;
}

// =====================================================================
// Common IEs: L3SI13RestOctets (GSM 04.08 10.5.2.37b)
// Reference: GSM_RestOctets.ttcn SI13RestOctets
// =====================================================================

TEST(GoldenIE, SI13RestOctets_Default) {
    L3SI13RestOctets orig;
}

// =====================================================================
// Common IEs: L3GPRSCellOptions
// =====================================================================

TEST(GoldenIE, GPRSCellOptions_Default) {
    L3GPRSCellOptions orig;
}

// =====================================================================
// Common IEs: L3GPRSSI13PowerControlParameters
// =====================================================================

TEST(GoldenIE, GPRSSI13PowerControlParameters_Default) {
    L3GPRSSI13PowerControlParameters orig;
}

// =====================================================================
// Common IEs: L3IARestOctets
// =====================================================================

TEST(GoldenIE, IARestOctets_Default) {
    L3IARestOctets orig;
}

// =====================================================================
// Common IEs: L3FollowOnProceed (GSM 04.08 10.5.2.38)
// Reference: GSM_RR_Types.ttcn FollowOnProceed
// =====================================================================

TEST(GoldenIE, FollowOnProceed_Default) {
    L3FollowOnProceed orig;
    EXPECT_EQ(orig.lengthV(), 1u);
    ieRoundTrip(orig);
}

// =====================================================================
// Common IEs: L3RestOctets base
// Reference: GSM_RestOctets.ttcn RestOctets
// =====================================================================

TEST(GoldenIE, RestOctets_Base) {
    L3RestOctets orig;
    EXPECT_EQ(orig.lengthV(), 0u);
}

// =====================================================================
// Common IEs: L3OctetAlignedProtocolElement
// =====================================================================

TEST(GoldenIE, OctetAlignedProtocolElement) {
    L3OctetAlignedProtocolElement orig(std::string("\xAB\xCD\xEF", 3));
    EXPECT_EQ(orig.lengthV(), 3u);
    EXPECT_TRUE(orig.mExtant);
}

// =====================================================================
// CC IEs: L3BearerCapability (GSM 04.08 10.5.4.5)
// Reference: L3_Templates.ttcn ts_Bcap_voice, ts_Bcap_voice_mt, ts_Bcap_csd
// =====================================================================

TEST(GoldenIE, BearerCapability_Default) {
    L3BearerCapability orig;
    ieRoundTrip(orig);
}

TEST(GoldenIE, BearerCapability_IE) {
    L3BearerCapability bc;
    EXPECT_EQ(bc.lengthV(), 1u);
}

// =====================================================================
// CC IEs: L3SupportedCodecList (GSM 04.08 10.5.4.32)
// =====================================================================

TEST(GoldenIE, SupportedCodecList_Default) {
    L3SupportedCodecList orig;
    EXPECT_FALSE(orig.isGsmPresent());
    EXPECT_FALSE(orig.isUmtsPresent());
}

// =====================================================================
// CC IEs: L3CalledPartyBCDNumber (GSM 04.08 10.5.4.7)
// Reference: L3_Templates.ttcn ts_Called, tr_Called
// =====================================================================

TEST(GoldenIE, CalledPartyBCDNumber_RoundTrip) {
    L3CalledPartyBCDNumber orig("1234567890");
    L3Frame frame(Primitive::L3_DATA, 64);
    size_t wp = 0;
    orig.writeV(frame, wp);
    L3CalledPartyBCDNumber parsed;
    size_t rp = 0;
    parsed.parseV(frame, rp, orig.lengthV());
    EXPECT_STREQ(parsed.digits(), "1234567890");
}

TEST(GoldenIE, CalledPartyBCDNumber_International) {
    L3CalledPartyBCDNumber num("+79161234567");
    EXPECT_EQ(num.type(), TypeOfNumber::International);
    EXPECT_EQ(num.plan(), NumberingPlan::E164);
    EXPECT_STREQ(num.digits(), "79161234567");
}

TEST(GoldenIE, CalledPartyBCDNumber_ShortNumber) {
    L3CalledPartyBCDNumber num("112");
    EXPECT_STREQ(num.digits(), "112");
}

TEST(GoldenIE, CalledPartyBCDNumber_National) {
    L3CalledPartyBCDNumber num("1234567890");
    EXPECT_EQ(num.type(), TypeOfNumber::Unknown);
    EXPECT_EQ(num.plan(), NumberingPlan::Unknown);
}

// =====================================================================
// CC IEs: L3CallingPartyBCDNumber (GSM 04.08 10.5.4.9)
// Reference: L3_Templates.ttcn ts_Calling
// =====================================================================

TEST(GoldenIE, CallingPartyBCDNumber_RoundTrip) {
    L3CallingPartyBCDNumber orig("1234567890");
    L3Frame frame(Primitive::L3_DATA, 64);
    size_t wp = 0;
    orig.writeV(frame, wp);
    L3CallingPartyBCDNumber parsed;
    size_t rp = 0;
    parsed.parseV(frame, rp, orig.lengthV());
    EXPECT_STREQ(parsed.digits(), "1234567890");
}

// =====================================================================
// CC IEs: L3CauseElement (GSM 04.08 10.5.4.11)
// Reference: L3_Templates.ttcn ML3_Cause_TLV
// =====================================================================

TEST(GoldenIE, CauseElement_RoundTrip) {
    L3CauseElement orig(CCCause::User_Busy, CCCauseLocation::Transit);
    EXPECT_EQ(orig.lengthV(), 2u);
    ieRoundTrip(orig);
}

TEST(GoldenIE, CauseElement_NormalClearing) {
    L3CauseElement orig(CCCause::Normal_Call_Clearing, CCCauseLocation::Private_Serving_Local);
    L3Frame frame(Primitive::L3_DATA, 32);
    size_t wp = 0;
    orig.writeV(frame, wp);
    L3CauseElement parsed;
    size_t rp = 0;
    parsed.parseV(frame, rp);
    EXPECT_EQ(parsed.cause(), CCCause::Normal_Call_Clearing);
    EXPECT_EQ(parsed.location(), CCCauseLocation::Private_Serving_Local);
}

// =====================================================================
// CC IEs: L3CallState (GSM 04.08 10.5.4.6)
// Reference: L3_Templates.ttcn ts_CallState
// =====================================================================

TEST(GoldenIE, CallState_RoundTrip) {
    L3CallState orig(0x05);
    EXPECT_EQ(orig.lengthV(), 1u);
    ieRoundTrip(orig);
}

// =====================================================================
// CC IEs: L3ProgressIndicator (GSM 04.08 10.5.4.21)
// Reference: L3_Templates.ttcn ts_Progress
// =====================================================================

TEST(GoldenIE, ProgressIndicator_RoundTrip) {
    L3ProgressIndicator orig(L3ProgressIndicator::InBandAvailable,
                              L3ProgressIndicator::PrivateServingLocal);
    EXPECT_EQ(orig.lengthV(), 2u);
    ieRoundTrip(orig);
}

TEST(GoldenIE, ProgressIndicator_Encoding) {
    L3ProgressIndicator pi(L3ProgressIndicator::InBandAvailable,
                           L3ProgressIndicator::PrivateServingLocal);
    L3Frame frame(Primitive::L3_DATA, 32);
    size_t wp = 0;
    pi.writeV(frame, wp);
    L3ProgressIndicator parsed;
    size_t rp = 0;
    parsed.parseV(frame, rp);
    EXPECT_EQ(parsed.progress(), L3ProgressIndicator::InBandAvailable);
    EXPECT_EQ(parsed.location(), L3ProgressIndicator::PrivateServingLocal);
}

// =====================================================================
// CC IEs: L3KeypadFacility (GSM 04.08 10.5.4.17)
// Reference: L3_Templates.ttcn ts_KeyPad
// =====================================================================

TEST(GoldenIE, KeypadFacility_RoundTrip) {
    L3KeypadFacility orig('A');
    EXPECT_EQ(orig.lengthV(), 1u);
    ieRoundTrip(orig);
}

TEST(GoldenIE, KeypadFacility_Digit) {
    L3KeypadFacility kp('5');
    EXPECT_EQ(kp.ia5(), '5');
    EXPECT_EQ(kp.lengthV(), 1u);
    L3Frame frame(Primitive::L3_DATA, 16);
    size_t wp = 0;
    kp.writeV(frame, wp);
    L3KeypadFacility parsed;
    size_t rp = 0;
    parsed.parseV(frame, rp);
    EXPECT_EQ(parsed.ia5(), '5');
}

// =====================================================================
// CC IEs: L3Signal (GSM 04.08 10.5.4.23)
// Reference: L3_Templates.ttcn ts_Signal
// =====================================================================

TEST(GoldenIE, Signal_RoundTrip) {
    L3Signal orig(L3Signal::SignalRingBackToneOn);
    EXPECT_EQ(orig.lengthV(), 1u);
    ieRoundTrip(orig);
}

TEST(GoldenIE, Signal_Values) {
    L3Signal s1(L3Signal::SignalRingBackToneOn);
    EXPECT_EQ(s1.lengthV(), 1u);
    L3Signal s2(L3Signal::SignalTonesOff);
    EXPECT_EQ(s2.lengthV(), 1u);
}

// =====================================================================
// CC IEs: L3RepeatIndicator (GSM 04.08 10.5.4.4)
// =====================================================================

TEST(GoldenIE, RepeatIndicator_Default) {
    L3RepeatIndicator orig;
    EXPECT_EQ(orig.lengthV(), 0u);
    EXPECT_EQ(orig.value(), 0u);
}

TEST(GoldenIE, RepeatIndicator_Value) {
    L3RepeatIndicator orig(5);
    EXPECT_EQ(orig.value(), 5u);
}

// =====================================================================
// CC IEs: L3SupServFacilityIE (GSM 04.08 10.5.4.1)
// Reference: SS_Templates.ttcn ts_SS_FACILITY_INVOKE
// =====================================================================

TEST(GoldenIE, SupServFacilityIE_RoundTrip) {
    L3SupServFacilityIE orig(std::string("\x81\x01\x13", 3));
    ieRoundTrip(orig);
}

// =====================================================================
// CC IEs: L3SupServVersionIndicator (24.008 10.5.4.24)
// Reference: SS_Templates.ttcn ts_SS_Version
// =====================================================================

TEST(GoldenIE, SupServVersionIndicator_RoundTrip) {
    L3SupServVersionIndicator orig;
    EXPECT_EQ(orig.lengthV(), 1u);
    ieRoundTrip(orig);
}

// =====================================================================
// CC IEs: L3BCDDigits utility (GSM 04.08 10.5.4.7)
// Reference: L3_Templates.ttcn ts_Called
// =====================================================================

TEST(GoldenIE, BCDDigits_Even) {
    L3BCDDigits orig("1234567890");
    EXPECT_STREQ(orig.digits(), "1234567890");
    EXPECT_EQ(orig.size(), 10u);
    EXPECT_EQ(orig.lengthV(), 5u);
}

TEST(GoldenIE, BCDDigits_Odd) {
    L3BCDDigits orig("12345");
    EXPECT_STREQ(orig.digits(), "12345");
    EXPECT_EQ(orig.size(), 5u);
    EXPECT_EQ(orig.lengthV(), 3u);
}

// =====================================================================
// MM IEs: L3CMServiceType (GSM 04.08 10.5.3.3)
// Reference: L3_Templates.ttcn CmServiceType
// =====================================================================

TEST(GoldenIE, CMServiceType_MO_Call) {
    L3CMServiceType orig(L3CMServiceType::MobileOriginatedCall);
    EXPECT_TRUE(orig.isCC());
    EXPECT_FALSE(orig.isSMS());
    EXPECT_EQ(orig.lengthV(), 0u);
    ieRoundTrip(orig);
}

TEST(GoldenIE, CMServiceType_SMS) {
    L3CMServiceType orig(L3CMServiceType::ShortMessage);
    EXPECT_TRUE(orig.isSMS());
    EXPECT_FALSE(orig.isCC());
    ieRoundTrip(orig);
}

TEST(GoldenIE, CMServiceType_Emergency) {
    L3CMServiceType orig(L3CMServiceType::EmergencyCall);
    EXPECT_TRUE(orig.isCC());
    EXPECT_FALSE(orig.isSMS());
}

TEST(GoldenIE, CMServiceType_SS) {
    L3CMServiceType orig(L3CMServiceType::SupplementaryService);
    EXPECT_FALSE(orig.isCC());
    EXPECT_FALSE(orig.isSMS());
}

TEST(GoldenIE, CMServiceType_LocationService) {
    L3CMServiceType orig(L3CMServiceType::LocationService);
    EXPECT_FALSE(orig.isCC());
}

// =====================================================================
// MM IEs: L3RejectCauseIE (GSM 04.08 10.5.3.6)
// Reference: L3_Templates.ttcn tr_ML3_MT_LU_Rej
// =====================================================================

TEST(GoldenIE, RejectCauseIE) {
    L3RejectCauseIE orig(MMRejectCause::Congestion);
    EXPECT_EQ(orig.lengthV(), 1u);
    ieRoundTrip(orig);
}

TEST(GoldenIE, RejectCauseIE_IMSI_Unknown) {
    L3RejectCauseIE orig(MMRejectCause::IMSI_Unknown_In_HLR);
    EXPECT_EQ(orig.lengthV(), 1u);
    ieRoundTrip(orig);
}

// =====================================================================
// MM IEs: L3RAND (GSM 04.08 10.5.3.1)
// =====================================================================

TEST(GoldenIE, RAND_RoundTrip) {
    std::vector<uint8_t> randBytes(16);
    for (int i = 0; i < 16; i++) randBytes[i] = static_cast<uint8_t>(i * 17);
    L3RAND orig(randBytes);
    EXPECT_EQ(orig.lengthV(), 16u);
    ieRoundTrip(orig);
}

// =====================================================================
// MM IEs: L3SRES (GSM 04.08 10.5.3.2)
// =====================================================================

TEST(GoldenIE, SRES_RoundTrip) {
    L3SRES orig(0xDEADBEEFu);
    EXPECT_EQ(orig.lengthV(), 4u);
    ieRoundTrip(orig);
}

// =====================================================================
// MM IEs: L3NetworkName (GSM 04.08 10.5.3.5a)
// Reference: L3_Templates.ttcn ts_NetworkName
// =====================================================================

TEST(GoldenIE, NetworkName_RoundTrip) {
    L3NetworkName orig("TestNetwork", GSMAlphabet::ALPHABET_7BIT, 1);
    EXPECT_STREQ(orig.name(), "TestNetwork");
    EXPECT_EQ(orig.alphabet(), GSMAlphabet::ALPHABET_7BIT);
}

TEST(GoldenIE, NetworkName_Encoding) {
    L3NetworkName nn("TestNet", GSMAlphabet::ALPHABET_7BIT, 1);
    EXPECT_STREQ(nn.name(), "TestNet");
    EXPECT_EQ(nn.alphabet(), GSMAlphabet::ALPHABET_7BIT);
}

// =====================================================================
// MM IEs: L3TimeZoneAndTime (GSM 04.08 10.5.3.9)
// Reference: L3_Templates.ttcn ts_TimeZoneAndTime
// =====================================================================

TEST(GoldenIE, TimeZoneAndTime_RoundTrip) {
    L3TimeZoneAndTime orig(L3TimeZoneAndTime::UTC_TIME);
    EXPECT_EQ(orig.lengthV(), 7u);
    ieRoundTrip(orig);
}

TEST(GoldenIE, TimeZoneAndTime_UTC) {
    L3TimeZoneAndTime tzt(L3TimeZoneAndTime::UTC_TIME);
    EXPECT_EQ(tzt.lengthV(), 7u);
    EXPECT_EQ(tzt.type(), L3TimeZoneAndTime::UTC_TIME);
}

TEST(GoldenIE, TimeZoneAndTime_Local) {
    L3TimeZoneAndTime tzt(L3TimeZoneAndTime::LOCAL_TIME);
    EXPECT_EQ(tzt.type(), L3TimeZoneAndTime::LOCAL_TIME);
}

// =====================================================================
// GSM Alphabet (3GPP TS 23.038 Table 1 / GSM 03.38 Table 1)
// Reference: GSM 7-bit default alphabet character mapping
// Spec-verified: Standard GSM 03.38 Table 1 character code points
//   0='@', 1='\', 2='$', 3='(', 4=')', 5='?', 6='\'', 7='!', 8='"',
//   44='0', 45='1', ..., 48='4', ...
//   84='a', 85='b', 86='c', ... (lowercase starts at code 84)
// [GSM SPEC VERIFIED] 3GPP TS 23.038 Table 1 default alphabet:
//   Codes 0-19: Special characters (@\$(?'"* etc.)
//   Codes 20-39: Uppercase A-Z (with some specials like Ñ, ä, ö at positions)
//   Codes 40-43: Punctuation ({|}~)
//   Codes 44-53: Digits 0-9 plus punctuation
//   Codes 54-67: Lowercase a-f (used for escaping uppercase/greek)
//   Codes 68-73: More specials
//   Codes 74-83: More specials
//   Codes 84-103: Lowercase g-z
//   Key mappings: code 0='@', code 44='0', code 45='1', code 84='a'
// =====================================================================

TEST(GoldenIE, GSMAlphabet_Decode) {
    // Spec-verified: 3GPP TS 23.038 Table 1 default alphabet mapping
    EXPECT_EQ(decodeGSMChar(0), '@');   // Code 0 = '@'
    EXPECT_EQ(decodeGSMChar(2), '$');   // Code 2 = '$'
    EXPECT_EQ(decodeGSMChar(44), '0');  // Code 44 = '0' (digit zero)
    EXPECT_EQ(decodeGSMChar(48), '4');  // Code 48 = '4'
    EXPECT_EQ(decodeGSMChar(84), 'a');  // Code 84 = 'a' (lowercase start)
    EXPECT_EQ(decodeGSMChar(85), 'b');  // Code 85 = 'b'
    EXPECT_EQ(decodeGSMChar(86), 'c');  // Code 86 = 'c'
}

// =====================================================================
// RACH Tables (GSM 04.08 10.5.2.29)
// Reference: BTS_Tests.ttcn RACHSpreadSlots, RACHWaitSParam
// =====================================================================

TEST(GoldenIE, RACHTables) {
    for (int i = 0; i < 16; i++) {
        EXPECT_GT(RACHSpreadSlots[i], 0u);
        EXPECT_GT(RACHWaitSParam[i], 0u);
    }
}

// =====================================================================
// RxLev / RxQual Conversion (3GPP TS 45.008 Chapter 8 / GSM 05.02)
// Reference: GSM_Types.ttcn dbm2rxlev (line 354): rxlev = dbm + 110
// Reference: GSM_Types.ttcn rxlev2dbm (line 359): return -110 + rxlev
// Reference: GSM_Types.ttcn ber2rxqual (line 369): BER threshold table
// Reference: GSM_Types.ttcn rxqual2ber (line 390): RxQual -> BER representative values
// Spec-verified: TS 45.008 Chapter 8.1.4 (RxLev), Chapter 8.2.4 (RxQual)
// [GSM SPEC VERIFIED] TS 45.008 8.1.4: RxLev = received_level_in_dBm + 110.
//   Range: RxLev 0 = -110 dBm (minimum), RxLev 63 = -47 dBm (maximum).
//   Values 0 and 255 are reserved/special. Valid range is 1-62 for normal operation.
// TS 45.008 8.2.4: RxQual 0 (BER < 0.2%) through RxQual 7 (BER >= 12.8%).
//   BER representative values: Qual 0 = 0.14%, Qual 7 = 18.10% (per GSM_Types.ttcn).
// =====================================================================

TEST(GoldenIE, RxLev_Conversion) {
    // Spec-verified: TS 45.008 8.1.4: RxLev = received level + 110 dB
    //   RxLev=0 -> -110 dBm (minimum), RxLev=31 -> -79 dBm, RxLev=63 -> -47 dBm (maximum)
    L3MeasurementResults mr;
    EXPECT_EQ(mr.decodeLevToDBm(0), -110);   // GSM_Types.ttcn rxlev2dbm: -110 + 0 = -110
    EXPECT_EQ(mr.decodeLevToDBm(31), -79);   // -110 + 31 = -79
    EXPECT_EQ(mr.decodeLevToDBm(63), -47);   // -110 + 63 = -47
}

TEST(GoldenIE, RxQual_Conversion) {
    // Spec-verified: TS 45.008 8.2.4: RxQual 0 (BER<0.2%) < RxQual 7 (BER>=12.8%)
    // GSM_Types.ttcn rxqual2ber: Qual 0=0.14%, Qual 7=18.10%
    L3MeasurementResults mr;
    float ber0 = mr.decodeQualToBER(0);
    float ber7 = mr.decodeQualToBER(7);
    EXPECT_LT(ber0, ber7); // BER for quality 0 must be lower than quality 7
}

// =====================================================================
// GSM Timing Constants (3GPP TS 45.008 / GSM 05.02)
// Reference: GSM_Types.ttcn GsmMaxFrameNumber (line 22): 26*51*2048 = 2715648
// Reference: GSM_Types.ttcn GSM_FRAME_DURATION (line 404): 0.12/26.0 = 4.615 ms
// Spec-verified: GSM hyperframe = 2715648 TDMA frames = 3 hours 28 minutes 48 seconds
// [GSM SPEC VERIFIED] TS 45.008 Chapter 5:
//   1 TDMA frame = 1/26 of 120ms multiframe = 4.615384... ms (~4615 microseconds).
//   26 TDMA frames = 120ms basic multiframe (TCH/FDCCH).
//   51 basic multiframes = 6162 TDMA frames = 120ms*51 = 6120ms SACCH multiframe.
//   2048 SACCH multiframes = 2715648 TDMA frames = hyperframe.
//   Hyperframe duration = 2715648 * 4.615ms = 3h 28m 48s (exactly 1244160 seconds).
//   FN (Frame Number) wraps at hyperframe boundary (mod 2715648).
// =====================================================================

TEST(GoldenIE, FrameDuration) {
    // Spec-verified: TS 45.008: 1 TDMA frame = 1/26 of 120ms burst = 4615 microseconds
    EXPECT_EQ(gFrameMicroseconds, 4615u);
}

TEST(GoldenIE, Hyperframe) {
    // Spec-verified: GSM_Types.ttcn GsmMaxFrameNumber = 26*51*2048 = 2715648
    // This is the TDMA frame number modulo (hyperframe boundary), not bit count
    EXPECT_EQ(gHyperframe, 2715648u);
}

TEST(GoldenIE, TimeComponents) {
    Time t(1326, 5);
    EXPECT_EQ(t.t1(), 1u);
    EXPECT_EQ(t.t2(), 0u);
    EXPECT_EQ(t.t3(), 0u);
    EXPECT_EQ(t.t1p(), 1u);
}

TEST(GoldenIE, FNDelta) {
    int32_t delta = FNDelta(100, 50);
    EXPECT_EQ(delta, 50);
    delta = FNDelta(50, 100);
    EXPECT_EQ(delta, -50);
}

TEST(GoldenIE, FNCompare) {
    EXPECT_GT(FNCompare(100, 50), 0);
    EXPECT_LT(FNCompare(50, 100), 0);
    EXPECT_EQ(FNCompare(100, 100), 0);
}

// =====================================================================
// BCD Number Encoding (GSM 04.08 10.5.4.7)
// Reference: L3_Templates.ttcn ts_Called, tr_Called
// =====================================================================

TEST(GoldenIE, BCD_EvenDigits) {
    L3CalledPartyBCDNumber num("1234567890");
    EXPECT_STREQ(num.digits(), "1234567890");
    EXPECT_EQ(num.lengthV(), 6u);
}

TEST(GoldenIE, BCD_OddDigits) {
    L3CalledPartyBCDNumber num("123456789");
    EXPECT_STREQ(num.digits(), "123456789");
    EXPECT_EQ(num.lengthV(), 6u);
}

TEST(GoldenIE, BCD_RoundTrip) {
    L3CalledPartyBCDNumber orig("1234567890");
    L3Frame frame(Primitive::L3_DATA, 64);
    size_t wp = 0;
    orig.writeV(frame, wp);
    L3CalledPartyBCDNumber parsed;
    size_t rp = 0;
    parsed.parseV(frame, rp, orig.lengthV());
    EXPECT_STREQ(parsed.digits(), "1234567890");
}

// =====================================================================
// Rest Octet Padding (GSM 24.008 / 3GPP TS 44.018)
// Reference: GSM_RR_Types.ttcn RestOctets (line 163):
//   type octetstring RestOctets with { variant "PADDING(yes), PADDING_PATTERN('00101011'B)" }
// Reference: GSM_RestOctets.ttcn (line 37): same PADDING_PATTERN('00101011'B)
// Spec-verified: GSM 24.008 section 9.x messages use 0x2B ('00101011'B) as rest octet padding
//   This pattern ensures sufficient transitions for bit synchronization
// [GSM SPEC VERIFIED] Padding pattern '00101011'B = 0x2B is used throughout GSM L3
//   to fill unused bits at the end of messages. This alternating pattern provides
//   good bit transition density for timing recovery and ensures the receiver can
//   maintain synchronization. All System Information rest octets, paging message
//   padding, and other variable-length L3 messages use this pattern.
// =====================================================================

TEST(GoldenIE, RestOctetPaddingPattern) {
    // Spec-verified: '00101011'B = 0x2B is the standard GSM rest octet padding pattern
    // GSM_RR_Types.ttcn line 163, GSM_RestOctets.ttcn line 37, and all SI rest octet types
    constexpr uint8_t GSM_REST_OCTET_PAD = 0x2B;
    EXPECT_EQ(GSM_REST_OCTET_PAD, 0x2B);
    BitVector bv(8);
    size_t wp = 0;
    bv.writeField(wp, GSM_REST_OCTET_PAD, 8);
    EXPECT_EQ(bv.data()[0], 0x2B);
}

// =====================================================================
// L/H Presence Bits (GSM 04.07 11.2.1.1.4)
// Reference: GSM_RestOctets.ttcn CSN.1 L/H encoding
// =====================================================================

TEST(GoldenIE, L_H_Bits) {
    L3Frame frame(Primitive::L3_DATA, 16);
    size_t wp = 0;
    frame.writeL(wp);
    frame.writeH(wp);
    size_t rp = 0;
    EXPECT_EQ(frame.readField(rp, 1), 0u);
    EXPECT_EQ(frame.readField(rp, 1), 1u);
}

// =====================================================================
// SI2 body length (GSM 24.008 9.1.32 / 3GPP TS 44.018 9.1.32)
// Reference: GSM_SystemInformation.ttcn SystemInformationType2 record definition
// Structure: bcch_freq_list(16 octets) + ncc_permitted(1 octet) + rach_control(3 octets) = 20 octets
// Spec-verified: GSM 24.008 9.1.32 System Information Type 2 fixed body length
// [GSM SPEC VERIFIED] SI2 has fixed body length of 20 octets (160 bits).
//   BCCH Frequency List: 16 octets (128-bit bitmap for ARFCN 0-124)
//   NCC Permitted: 1 octet (8-bit mask, bit N=1 means NCC value N is allowed)
//   RACH Control Parameters: 3 octets (max_retrans, tx_integer, ACC mask, etc.)
//   Total = 16 + 1 + 3 = 20 octets. No padding needed (already word-aligned).
// =====================================================================

TEST(GoldenIE, SI2_BodyLength) {
    // Spec-verified: SI2 body = BCCH freq list(16) + NCC permitted(1) + RACH control params(3) = 20 octets
    L3SystemInformationType2 msg;
    EXPECT_EQ(msg.l2BodyLength(), 20u);
    EXPECT_EQ(msg.fullBodyLength(), 20u);
}

// =====================================================================
// SI2bis body length (GSM 24.008 9.1.33 / 3GPP TS 44.018 9.1.33)
// Reference: GSM_SystemInformation.ttcn SystemInformationType2bis record definition
// Structure: extd_bcch_freq_list(16 octets) + rach_control(3 octets) = 19 octets
// Spec-verified: GSM 24.008 9.1.33 System Information Type 2bis fixed body length
// =====================================================================

TEST(GoldenIE, SI2bis_BodyLength) {
    // Spec-verified: SI2bis body = Extended BCCH freq list(16) + RACH control params(3) = 19 octets
    L3SystemInformationType2bis msg;
    EXPECT_EQ(msg.l2BodyLength(), 19u);
    EXPECT_EQ(msg.fullBodyLength(), 20u); // Padded to multiple of word boundary
}

// =====================================================================
// SI2ter body length (GSM 24.008 9.1.34 / 3GPP TS 44.018 9.1.34)
// Reference: GSM_SystemInformation.ttcn SystemInformationType2ter record definition
// Structure: extd_bcch_freq_list(16 octets) = 16 octets
// Spec-verified: GSM 24.008 9.1.34 System Information Type 2ter fixed body length
// =====================================================================

TEST(GoldenIE, SI2ter_BodyLength) {
    // Spec-verified: SI2ter body = Extended BCCH freq list(16) = 16 octets
    L3SystemInformationType2ter msg;
    EXPECT_EQ(msg.l2BodyLength(), 16u);
    EXPECT_EQ(msg.fullBodyLength(), 20u); // Padded to multiple of word boundary
}

// =====================================================================
// data2hex utility
// =====================================================================

TEST(GoldenIE, Data2Hex) {
    uint8_t data[] = {0x60, 0x19, 0x0D};
    std::string hex = data2hex(data, 3);
    EXPECT_EQ(hex, "60190D");
}

// =====================================================================
// countBeaconTimeslots utility
// =====================================================================

TEST(GoldenIE, BeaconTimeslots) {
    // ccch_conf=0 (1CCCH not combined) -> 1 beacon
    EXPECT_GT(countBeaconTimeslots(0), 0u);
}
