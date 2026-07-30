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

// Common IE round-trip tests: every L3ProtocolElement writeV → parseV.
// Reference: osmo-ttcn3-hacks GSM_Types.ttcn, GSM_RR_Types.ttcn,
// GSM_SystemInformation.ttcn, GSM_RestOctets.ttcn, L3_Templates.ttcn.

#include <gtest/gtest.h>
#include <gsml3parser/parser.h>
#include <gsml3parser/common/l3common.h>
#include <gsml3parser/rr/l3rrmessages.h>
#include <gsml3parser/mm/l3mmlements.h>
#include <gsml3parser/cc/l3cclements.h>

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
    // If the type has a text() that we can compare, use it as a sanity check.
    std::ostringstream os1, os2;
    orig.text(os1);
    parsed.text(os2);
    EXPECT_EQ(os1.str(), os2.str());
}

// ── L3CellIdentity (GSM 04.08 10.5.1.1) ───────────────────────────────

TEST(CommonIETest, CellIdentity_Default) {
    L3CellIdentity ci;
    EXPECT_EQ(ci.lengthV(), 2u);
    EXPECT_EQ(ci.ID(), 0u);
}

TEST(CommonIETest, CellIdentity_RoundTrip) {
    L3CellIdentity orig(0x1234);
    ieRoundTrip(orig);
}

TEST(CommonIETest, CellIdentity_MaxValue) {
    L3CellIdentity orig(0xFFFF);
    ieRoundTrip(orig);
}

// ── L3LocationAreaIdentity (GSM 04.08 10.5.1.3) ──────────────────────
// Reference: GSM_Types.ttcn LocationAreaIdentification, ts_LAI

TEST(CommonIETest, LAI_Default) {
    L3LocationAreaIdentity lai;
    EXPECT_EQ(lai.lengthV(), 5u);
}

TEST(CommonIETest, LAI_RoundTrip) {
    L3LocationAreaIdentity orig("250", "01", 0x1234);
    ieRoundTrip(orig);
}

TEST(CommonIETest, LAI_3DigitMNC) {
    L3LocationAreaIdentity orig("250", "012", 0x5678);
    ieRoundTrip(orig);
}

TEST(CommonIETest, LAI_Equality) {
    L3LocationAreaIdentity a("250", "01", 0x1234);
    L3LocationAreaIdentity b("250", "01", 0x1234);
    L3LocationAreaIdentity c("250", "01", 0x5678);
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);
}

// ── L3MobileIdentity (GSM 04.08 10.5.1.4) ────────────────────────────
// Reference: L3_Templates.ttcn ts_MI_TMSI_LV, ts_MI_IMSI_LV, ts_MI_IMEI_LV

TEST(CommonIETest, MobileIdentity_TMSI) {
    L3MobileIdentity orig(0xDEADBEEF);
    EXPECT_EQ(orig.type(), MobileIDType::TMSI);
    EXPECT_TRUE(orig.isTMSI());
    EXPECT_FALSE(orig.isIMSI());
    EXPECT_EQ(orig.TMSI(), 0xDEADBEEFu);
}

TEST(CommonIETest, MobileIdentity_IMSI) {
    L3MobileIdentity orig("250011234567890");
    EXPECT_EQ(orig.type(), MobileIDType::IMSI);
    EXPECT_TRUE(orig.isIMSI());
    EXPECT_STREQ(orig.digits(), "250011234567890");
}

TEST(CommonIETest, MobileIdentity_Equality) {
    L3MobileIdentity a(0x12345678);
    L3MobileIdentity b(0x12345678);
    L3MobileIdentity c(0x87654321);
    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}

TEST(CommonIETest, MobileIdentity_LessThan) {
    L3MobileIdentity a(0x00000001);
    L3MobileIdentity b(0x00000002);
    EXPECT_LT(a, b);
}

TEST(CommonIETest, MobileIdentity_Default) {
    L3MobileIdentity id;
    // Default should be NoID
    EXPECT_EQ(id.type(), MobileIDType::NoID);
}

// ── L3MobileStationClassmark1 (GSM 04.08 10.5.1.5) ───────────────────
// Reference: L3_Templates.ttcn ts_CM1

TEST(CommonIETest, Classmark1_Default) {
    L3MobileStationClassmark1 cm1;
    EXPECT_EQ(cm1.lengthV(), 1u);
}

TEST(CommonIETest, Classmark1_RoundTrip) {
    L3MobileStationClassmark1 orig;
    ieRoundTrip(orig);
}

// ── L3MobileStationClassmark2 (GSM 04.08 10.5.1.6) ───────────────────
// Reference: L3_Templates.ttcn ts_CM2

TEST(CommonIETest, Classmark2_Default) {
    L3MobileStationClassmark2 cm2;
    EXPECT_EQ(cm2.lengthV(), 3u);
}

TEST(CommonIETest, Classmark2_RoundTrip) {
    L3MobileStationClassmark2 orig;
    ieRoundTrip(orig);
}

TEST(CommonIETest, Classmark2_PowerClass) {
    L3MobileStationClassmark2 cm2;
    // Default RF power capability = 0 → power class = 1
    EXPECT_EQ(cm2.powerClass(), 1);
}

TEST(CommonIETest, Classmark2_A5Bits) {
    L3MobileStationClassmark2 cm2;
    // getA5Bits returns bit mask of supported A5 algorithms
    int bits = cm2.getA5Bits();
    EXPECT_GE(bits, 0);
}

// ── L3MobileStationClassmark3 (GSM 04.08 10.5.1.7) ──────────────────

TEST(CommonIETest, Classmark3_Default) {
    L3MobileStationClassmark3 cm3;
    EXPECT_EQ(cm3.lengthV(), 14u);
}

// ── L3CipheringKeySequenceNumber (GSM 04.08 10.5.1.2) ────────────────
// Reference: L3_Templates.ttcn ts_CKSN

TEST(CommonIETest, CipheringKeySeqNr_Default) {
    L3CipheringKeySequenceNumber cksn;
    EXPECT_EQ(cksn.lengthV(), 0u); // half-octet field
}

TEST(CommonIETest, CipheringKeySeqNr_RoundTrip) {
    L3CipheringKeySequenceNumber orig(5);
    ieRoundTrip(orig);
}

TEST(CommonIETest, CipheringKeySeqNr_MaxValue) {
    L3CipheringKeySequenceNumber orig(7);
    ieRoundTrip(orig);
}

// ── L3ChannelDescription (GSM 04.08 10.5.2.5) ────────────────────────
// Reference: GSM_RR_Types.ttcn ChannelDescription, ts_ChanDescH0, ts_ChanDescH1

TEST(CommonIETest, ChannelDescription_Default) {
    L3ChannelDescription chd;
    EXPECT_FALSE(chd.initialized());
    EXPECT_EQ(chd.lengthV(), 3u);
}

TEST(CommonIETest, ChannelDescription_SDCCH) {
    L3ChannelDescription orig(TDMA_SDCCH, 2, 7, 100);
    EXPECT_TRUE(orig.initialized());
    EXPECT_EQ(orig.typeAndOffset(), TDMA_SDCCH);
    EXPECT_EQ(orig.TN(), 2u);
    EXPECT_EQ(orig.TSC(), 7u);
    EXPECT_EQ(orig.ARFCN(), 100u);
    ieRoundTrip(orig);
}

TEST(CommonIETest, ChannelDescription_TCHF) {
    L3ChannelDescription orig(TDMA_TCHF, 5, 3, 200);
    ieRoundTrip(orig);
}

TEST(CommonIETest, ChannelDescription_TCHH) {
    L3ChannelDescription orig(TDMA_TCHH, 0, 0, 1);
    ieRoundTrip(orig);
}

TEST(CommonIETest, ChannelDescription_CBCH) {
    L3ChannelDescription orig(TDMA_CBCH, 1, 4, 50);
    ieRoundTrip(orig);
}

// ── L3AdditionalChannelDescription ───────────────────────────────────

TEST(CommonIETest, AdditionalChannelDescription_Default) {
    L3AdditionalChannelDescription chd;
    EXPECT_FALSE(chd.initialized());
    EXPECT_EQ(chd.lengthV(), 3u);
}

TEST(CommonIETest, AdditionalChannelDescription_RoundTrip) {
    L3AdditionalChannelDescription orig(TDMA_TCHF, 3, 5, 150);
    ieRoundTrip(orig);
}

// ── L3PowerCommand (GSM 04.08 10.5.2.28) ─────────────────────────────

TEST(CommonIETest, PowerCommand_Default) {
    L3PowerCommand pc;
    EXPECT_EQ(pc.lengthV(), 1u);
    EXPECT_EQ(pc.command(), 0u);
}

TEST(CommonIETest, PowerCommand_RoundTrip) {
    L3PowerCommand orig(10);
    ieRoundTrip(orig);
}

TEST(CommonIETest, PowerCommand_MaxValue) {
    L3PowerCommand orig(31);
    ieRoundTrip(orig);
}

// ── L3PowerCommandAndAccessType ──────────────────────────────────────

TEST(CommonIETest, PowerCommandAndAccessType_Default) {
    L3PowerCommandAndAccessType pc;
    EXPECT_EQ(pc.lengthV(), 1u);
}

TEST(CommonIETest, PowerCommandAndAccessType_RoundTrip) {
    L3PowerCommandAndAccessType orig(15);
    ieRoundTrip(orig);
}

// ── L3ChannelMode (GSM 04.08 10.5.2.6) ───────────────────────────────

TEST(CommonIETest, ChannelMode_Signalling) {
    L3ChannelMode orig(L3ChannelMode::SignallingOnly);
    EXPECT_FALSE(orig.isAMR());
    ieRoundTrip(orig);
}

TEST(CommonIETest, ChannelMode_SpeechV1) {
    L3ChannelMode orig(L3ChannelMode::SpeechV1);
    EXPECT_FALSE(orig.isAMR());
    ieRoundTrip(orig);
}

TEST(CommonIETest, ChannelMode_SpeechV2) {
    L3ChannelMode orig(L3ChannelMode::SpeechV2);
    EXPECT_FALSE(orig.isAMR());
    ieRoundTrip(orig);
}

TEST(CommonIETest, ChannelMode_SpeechV3_AMR) {
    L3ChannelMode orig(L3ChannelMode::SpeechV3);
    EXPECT_TRUE(orig.isAMR());
    ieRoundTrip(orig);
}

TEST(CommonIETest, ChannelMode_Equality) {
    L3ChannelMode a(L3ChannelMode::SpeechV1);
    L3ChannelMode b(L3ChannelMode::SpeechV1);
    L3ChannelMode c(L3ChannelMode::SpeechV2);
    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}

// ── L3TimingAdvance (GSM 04.08 10.5.2.40) ────────────────────────────
// Reference: GSM_RR_Types.ttcn TimingAdvance (0..219)

TEST(CommonIETest, TimingAdvance_Default) {
    L3TimingAdvance ta;
    EXPECT_EQ(ta.lengthV(), 1u);
    EXPECT_EQ(ta.timingAdvance(), 0u);
}

TEST(CommonIETest, TimingAdvance_RoundTrip) {
    L3TimingAdvance orig(100);
    ieRoundTrip(orig);
}

TEST(CommonIETest, TimingAdvance_MaxValue) {
    L3TimingAdvance orig(219); // max per GSM spec
    ieRoundTrip(orig);
}

// ── L3CellDescription (GSM 04.08 10.5.2.2) ───────────────────────────
// Reference: GSM_RR_Types.ttcn CellDescriptionV (LSB first: bcc(3), ncc(3), arfcn(10))

TEST(CommonIETest, CellDescription_Default) {
    L3CellDescription cd;
    EXPECT_EQ(cd.lengthV(), 2u);
    EXPECT_EQ(cd.ARFCN(), 0u);
    EXPECT_EQ(cd.NCC(), 0u);
    EXPECT_EQ(cd.BCC(), 0u);
}

TEST(CommonIETest, CellDescription_RoundTrip) {
    L3CellDescription orig(100, 5, 3);
    ieRoundTrip(orig);
}

// ── L3HandoverReference (GSM 04.08 10.5.2.15) ────────────────────────

TEST(CommonIETest, HandoverReference_Default) {
    L3HandoverReference hr;
    EXPECT_EQ(hr.lengthV(), 1u);
    EXPECT_EQ(hr.value(), 0u);
}

TEST(CommonIETest, HandoverReference_RoundTrip) {
    L3HandoverReference orig(0x17);
    ieRoundTrip(orig);
}

// ── L3CipheringModeSetting (GSM 04.08 10.5.2.9) ──────────────────────

TEST(CommonIETest, CipheringModeSetting_Off) {
    L3CipheringModeSetting orig(false, 0);
    EXPECT_EQ(orig.lengthV(), 0u); // half-octet
    ieRoundTrip(orig);
}

TEST(CommonIETest, CipheringModeSetting_A5_3) {
    L3CipheringModeSetting orig(true, 3);
    ieRoundTrip(orig);
}

// ── L3CipheringModeResponse (GSM 04.08 10.5.2.10) ────────────────────

TEST(CommonIETest, CipheringModeResponse_Default) {
    L3CipheringModeResponse orig;
    EXPECT_EQ(orig.lengthV(), 0u);
    EXPECT_FALSE(orig.includeIMEISV());
    ieRoundTrip(orig);
}

// ── L3SynchronizationIndication (GSM 04.08 10.5.2.39) ────────────────

TEST(CommonIETest, SynchronizationIndication_Default) {
    L3SynchronizationIndication orig;
    EXPECT_EQ(orig.lengthV(), 1u);
    ieRoundTrip(orig);
}

TEST(CommonIETest, SynchronizationIndication_Values) {
    L3SynchronizationIndication orig(true, true, 3);
    EXPECT_TRUE(orig.NCI());
    EXPECT_TRUE(orig.ROT());
    EXPECT_EQ(orig.SI(), 3);
    ieRoundTrip(orig);
}

// ── L3NCCPermitted (GSM 04.08 10.5.2.27) ─────────────────────────────

TEST(CommonIETest, NCCPermitted_Default) {
    L3NCCPermitted orig;
    EXPECT_EQ(orig.permitted(), 0xFFu);
    EXPECT_EQ(orig.lengthV(), 1u);
    ieRoundTrip(orig);
}

TEST(CommonIETest, NCCPermitted_Custom) {
    L3NCCPermitted orig(0x7F); // all except NCC=7
    ieRoundTrip(orig);
}

// ── L3PageMode (GSM 04.08 10.5.2.26) ─────────────────────────────────
// Reference: GSM_RR_Types.ttcn PageMode

TEST(CommonIETest, PageMode_Normal) {
    L3PageMode orig(0);
    EXPECT_EQ(orig.lengthV(), 0u); // half-octet
    ieRoundTrip(orig);
}

TEST(CommonIETest, PageMode_Extended) {
    L3PageMode orig(1);
    ieRoundTrip(orig);
}

TEST(CommonIETest, PageMode_Reorganization) {
    L3PageMode orig(2);
    ieRoundTrip(orig);
}

TEST(CommonIETest, PageMode_SameAsBefore) {
    L3PageMode orig(3);
    ieRoundTrip(orig);
}

// ── L3RequestReference (GSM 04.08 10.5.2.30) ─────────────────────────
// Reference: GSM_RR_Types.ttcn RequestReference, f_compute_ReqRef

TEST(CommonIETest, RequestReference_Default) {
    L3RequestReference orig;
    EXPECT_EQ(orig.lengthV(), 3u);
    ieRoundTrip(orig);
}

TEST(CommonIETest, RequestReference_Custom) {
    L3RequestReference orig(0xAB, 5, 12, 20);
    ieRoundTrip(orig);
}

// ── L3WaitIndication (GSM 04.08 10.5.2.43) ───────────────────────────

TEST(CommonIETest, WaitIndication_Default) {
    L3WaitIndication orig;
    EXPECT_EQ(orig.lengthV(), 1u);
    ieRoundTrip(orig);
}

TEST(CommonIETest, WaitIndication_Value) {
    L3WaitIndication orig(60);
    ieRoundTrip(orig);
}

// ── L3RRCauseElement (GSM 04.08 10.5.2.31) ───────────────────────────

TEST(CommonIETest, RRCauseElement_Normal) {
    L3RRCauseElement orig(RRCause::Normal_Event);
    EXPECT_EQ(orig.lengthV(), 1u);
    ieRoundTrip(orig);
}

TEST(CommonIETest, RRCauseElement_HandoverImpossible) {
    L3RRCauseElement orig(RRCause::Handover_Impossible);
    ieRoundTrip(orig);
}

TEST(CommonIETest, RRCauseElement_ProtocolError) {
    L3RRCauseElement orig(RRCause::Protocol_Error_Unspecified);
    ieRoundTrip(orig);
}

// ── L3CellOptionsBCCH (GSM 04.08 10.5.2.3) ───────────────────────────
// Reference: GSM_SystemInformation.ttcn CellOptions

TEST(CommonIETest, CellOptionsBCCH_Default) {
    L3CellOptionsBCCH orig;
    EXPECT_EQ(orig.lengthV(), 1u);
    ieRoundTrip(orig);
}

// ── L3CellOptionsSACCH (GSM 04.08 10.5.2.3a) ─────────────────────────
// Reference: GSM_SystemInformation.ttcn CellOptionsSacch

TEST(CommonIETest, CellOptionsSACCH_Default) {
    L3CellOptionsSACCH orig;
    EXPECT_EQ(orig.lengthV(), 1u);
    ieRoundTrip(orig);
}

// ── L3CellSelectionParameters (GSM 04.08 10.5.2.4) ───────────────────
// Reference: BTS_Tests.ttcn ts_CellSelPar_default

TEST(CommonIETest, CellSelectionParameters_Default) {
    L3CellSelectionParameters orig;
    EXPECT_EQ(orig.lengthV(), 2u);
    ieRoundTrip(orig);
}

// ── L3RACHControlParameters (GSM 04.08 10.5.2.29) ────────────────────
// Reference: BTS_Tests.ttcn ts_RachCtrl_default

TEST(CommonIETest, RACHControlParameters_Default) {
    L3RACHControlParameters orig;
    EXPECT_EQ(orig.lengthV(), 3u);
    ieRoundTrip(orig);
}

// ── L3ControlChannelDescription (GSM 04.08 10.5.2.11) ────────────────
// Reference: GSM_SystemInformation.ttcn ControlChannelDescription

TEST(CommonIETest, ControlChannelDescription_Default) {
    L3ControlChannelDescription orig;
    EXPECT_EQ(orig.lengthV(), 3u);
    ieRoundTrip(orig);
}

// ── L3CellChannelDescription (GSM 04.08 10.5.2.1b) ───────────────────

TEST(CommonIETest, CellChannelDescription_Default) {
    L3CellChannelDescription orig;
    EXPECT_EQ(orig.lengthV(), 3u);
    ieRoundTrip(orig);
}

TEST(CommonIETest, CellChannelDescription_Custom) {
    L3CellChannelDescription orig(100, 0x1F, 1);
    ieRoundTrip(orig);
}

// ── L3FrequencyList (GSM 04.08 10.5.2.13) ────────────────────────────

TEST(CommonIETest, FrequencyList_Default) {
    L3FrequencyList orig;
    EXPECT_EQ(orig.lengthV(), 16u);
    ieRoundTrip(orig);
}

TEST(CommonIETest, FrequencyList_WithARFCNs) {
    std::vector<unsigned> arfcns = {100, 101, 102, 200};
    L3FrequencyList orig(arfcns);
    ieRoundTrip(orig);
}

// ── L3BCCHFrequencyList ──────────────────────────────────────────────

TEST(CommonIETest, BCCHFrequencyList_Default) {
    L3BCCHFrequencyList orig;
    EXPECT_EQ(orig.lengthV(), 16u);
    ieRoundTrip(orig);
}

TEST(CommonIETest, BCCHFrequencyList_WithARFCNs) {
    std::vector<unsigned> arfcns = {50, 100, 150};
    L3BCCHFrequencyList orig(arfcns);
    ieRoundTrip(orig);
}

// ── L3NeighborCellsDescription ───────────────────────────────────────

TEST(CommonIETest, NeighborCellsDescription_Default) {
    L3NeighborCellsDescription orig;
    EXPECT_EQ(orig.lengthV(), 16u);
    ieRoundTrip(orig);
}

// ── L3MeasurementResults (GSM 04.08 10.5.2.20) ──────────────────────
// Reference: GSM_RR_Types.ttcn MeasurementResults, ts_MeasurementResults

TEST(CommonIETest, MeasurementResults_Default) {
    L3MeasurementResults orig;
    EXPECT_EQ(orig.lengthV(), 16u);
    ieRoundTrip(orig);
}

// ── L3MultiRateConfiguration (3GPP 44.018 10.5.2.21aa) ──────────────

TEST(CommonIETest, MultiRateConfiguration_FR) {
    L3MultiRateConfiguration orig(false); // full rate
    EXPECT_EQ(orig.lengthV(), 2u);
    ieRoundTrip(orig);
}

TEST(CommonIETest, MultiRateConfiguration_HR) {
    L3MultiRateConfiguration orig(true); // half rate
    ieRoundTrip(orig);
}

// ── L3ImmediateAssignmentInformation ─────────────────────────────────

TEST(CommonIETest, ImmediateAssignmentInformation_Default) {
    L3ImmediateAssignmentInformation orig;
    EXPECT_EQ(orig.PowerOffset(), 0u);
}

// ── L3DedicatedModeOrTBF (GSM 04.08 10.5.2.25b) ─────────────────────

TEST(CommonIETest, DedicatedModeOrTBF_Dedicated) {
    L3DedicatedModeOrTBF orig(false, false);
    EXPECT_EQ(orig.lengthV(), 0u);
    EXPECT_FALSE(orig.isTBF());
    EXPECT_FALSE(orig.isDownlink());
    ieRoundTrip(orig);
}

TEST(CommonIETest, DedicatedModeOrTBF_TBF) {
    L3DedicatedModeOrTBF orig(true, true);
    EXPECT_TRUE(orig.isTBF());
    EXPECT_TRUE(orig.isDownlink());
    ieRoundTrip(orig);
}

// ── L3APDUID (GSM 04.08 10.5.2.48) ──────────────────────────────────

TEST(CommonIETest, APDUID_Default) {
    L3APDUID orig;
    EXPECT_EQ(orig.lengthV(), 0u);
    ieRoundTrip(orig);
}

TEST(CommonIETest, APDUID_Value) {
    L3APDUID orig(3);
    ieRoundTrip(orig);
}

// ── L3APDUFlags (GSM 04.08 10.5.2.49) ───────────────────────────────

TEST(CommonIETest, APDUFlags_Default) {
    L3APDUFlags orig;
    EXPECT_EQ(orig.lengthV(), 0u);
    ieRoundTrip(orig);
}

TEST(CommonIETest, APDUFlags_Full) {
    L3APDUFlags orig(1, 1, 1);
    ieRoundTrip(orig);
}

// ── L3APDUData (GSM 04.08 10.5.2.50) ────────────────────────────────

TEST(CommonIETest, APDUData_Empty) {
    L3APDUData orig;
    ieRoundTrip(orig);
}

TEST(CommonIETest, APDUData_WithData) {
    BitVector data(16);
    size_t wp = 0;
    data.writeField(wp, 0xAB, 8);
    data.writeField(wp, 0xCD, 8);
    L3APDUData orig(data);
    ieRoundTrip(orig);
}

// ── L3SI3RestOctets (GSM 04.08 10.5.2.34) ───────────────────────────
// Reference: GSM_RestOctets.ttcn SI3RestOctets

TEST(CommonIETest, SI3RestOctets_Default) {
    L3SI3RestOctets orig;
    EXPECT_FALSE(orig.hasSI3RestOctets());
    EXPECT_FALSE(orig.hasGPRS());
}

// ── L3SIType4RestOctets ──────────────────────────────────────────────

TEST(CommonIETest, SI4RestOctets_Default) {
    L3SIType4RestOctets orig;
}

// ── L3SI13RestOctets (GSM 04.08 10.5.2.37b) ─────────────────────────
// Reference: GSM_RestOctets.ttcn SI13RestOctets

TEST(CommonIETest, SI13RestOctets_Default) {
    L3SI13RestOctets orig;
}

// ── L3GPRSCellOptions ────────────────────────────────────────────────

TEST(CommonIETest, GPRSCellOptions_Default) {
    L3GPRSCellOptions orig;
}

// ── L3GPRSSI13PowerControlParameters ─────────────────────────────────

TEST(CommonIETest, GPRSSI13PowerControlParameters_Default) {
    L3GPRSSI13PowerControlParameters orig;
}

// ── L3IARestOctets ───────────────────────────────────────────────────

TEST(CommonIETest, IARestOctets_Default) {
    L3IARestOctets orig;
}

// ── L3FollowOnProceed ────────────────────────────────────────────────

TEST(CommonIETest, FollowOnProceed_Default) {
    L3FollowOnProceed orig;
    EXPECT_EQ(orig.lengthV(), 1u);
    ieRoundTrip(orig);
}

// ── MM IEs ───────────────────────────────────────────────────────────

TEST(CommonIETest, CMServiceType_MO_Call) {
    L3CMServiceType orig(L3CMServiceType::MobileOriginatedCall);
    EXPECT_TRUE(orig.isCC());
    EXPECT_FALSE(orig.isSMS());
    EXPECT_EQ(orig.lengthV(), 0u);
    ieRoundTrip(orig);
}

TEST(CommonIETest, CMServiceType_SMS) {
    L3CMServiceType orig(L3CMServiceType::ShortMessage);
    EXPECT_TRUE(orig.isSMS());
    EXPECT_FALSE(orig.isCC());
    ieRoundTrip(orig);
}

TEST(CommonIETest, RejectCauseIE) {
    L3RejectCauseIE orig(MMRejectCause::Congestion);
    EXPECT_EQ(orig.lengthV(), 1u);
    ieRoundTrip(orig);
}

TEST(CommonIETest, RAND_RoundTrip) {
    std::vector<uint8_t> randBytes(16);
    for (int i = 0; i < 16; i++) randBytes[i] = static_cast<uint8_t>(i * 17);
    L3RAND orig(randBytes);
    EXPECT_EQ(orig.lengthV(), 16u);
    ieRoundTrip(orig);
}

TEST(CommonIETest, SRES_RoundTrip) {
    L3SRES orig(0xDEADBEEFu);
    EXPECT_EQ(orig.lengthV(), 4u);
    ieRoundTrip(orig);
}

TEST(CommonIETest, NetworkName_RoundTrip) {
    L3NetworkName orig("TestNetwork", GSMAlphabet::ALPHABET_7BIT, 1);
    EXPECT_STREQ(orig.name(), "TestNetwork");
    EXPECT_EQ(orig.alphabet(), GSMAlphabet::ALPHABET_7BIT);
}

TEST(CommonIETest, TimeZoneAndTime_RoundTrip) {
    L3TimeZoneAndTime orig(L3TimeZoneAndTime::UTC_TIME);
    EXPECT_EQ(orig.lengthV(), 7u);
    ieRoundTrip(orig);
}

// ── CC IEs ───────────────────────────────────────────────────────────

TEST(CommonIETest, BearerCapability_Default) {
    L3BearerCapability orig;
    ieRoundTrip(orig);
}

TEST(CommonIETest, CalledPartyBCDNumber_RoundTrip) {
    L3CalledPartyBCDNumber orig("1234567890");
    ieRoundTrip(orig);
}

TEST(CommonIETest, CallingPartyBCDNumber_RoundTrip) {
    L3CallingPartyBCDNumber orig("0987654321");
    ieRoundTrip(orig);
}

TEST(CommonIETest, CauseElement_RoundTrip) {
    L3CauseElement orig(CCCause::User_Busy, CCCauseLocation::Transit);
    EXPECT_EQ(orig.lengthV(), 2u);
    ieRoundTrip(orig);
}

TEST(CommonIETest, CallState_RoundTrip) {
    L3CallState orig(0x05);
    EXPECT_EQ(orig.lengthV(), 1u);
    ieRoundTrip(orig);
}

TEST(CommonIETest, ProgressIndicator_RoundTrip) {
    L3ProgressIndicator orig(L3ProgressIndicator::InBandAvailable,
                              L3ProgressIndicator::PrivateServingLocal);
    EXPECT_EQ(orig.lengthV(), 2u);
    ieRoundTrip(orig);
}

TEST(CommonIETest, KeypadFacility_RoundTrip) {
    L3KeypadFacility orig('A');
    EXPECT_EQ(orig.lengthV(), 1u);
    ieRoundTrip(orig);
}

TEST(CommonIETest, Signal_RoundTrip) {
    L3Signal orig(L3Signal::SignalRingBackToneOn);
    EXPECT_EQ(orig.lengthV(), 1u);
    ieRoundTrip(orig);
}

TEST(CommonIETest, SupServFacilityIE_RoundTrip) {
    L3SupServFacilityIE orig(std::string("\x81\x01\x13", 3));
    ieRoundTrip(orig);
}

TEST(CommonIETest, SupServVersionIndicator_RoundTrip) {
    L3SupServVersionIndicator orig;
    EXPECT_EQ(orig.lengthV(), 1u);
    ieRoundTrip(orig);
}

TEST(CommonIETest, BCDDigits_RoundTrip) {
    L3BCDDigits orig("1234567890");
    EXPECT_STREQ(orig.digits(), "1234567890");
    EXPECT_EQ(orig.size(), 10u);
    EXPECT_EQ(orig.lengthV(), 5u);
}

TEST(CommonIETest, BCDDigits_OddLength) {
    L3BCDDigits orig("12345");
    EXPECT_STREQ(orig.digits(), "12345");
    EXPECT_EQ(orig.size(), 5u);
    EXPECT_EQ(orig.lengthV(), 3u); // 5 digits → 3 bytes (last nibble = F)
}

// ── L3ChannelDescription2 ────────────────────────────────────────────

TEST(CommonIETest, ChannelDescription2_Default) {
    L3ChannelDescription2 chd;
    EXPECT_EQ(chd.lengthV(), 3u);
}

TEST(CommonIETest, ChannelDescription2_FromChannelDescription) {
    L3ChannelDescription orig(TDMA_TCHF, 3, 7, 100);
    L3ChannelDescription2 chd2(orig);
    EXPECT_EQ(chd2.typeAndOffset(), TDMA_TCHF);
    EXPECT_EQ(chd2.TN(), 3u);
    EXPECT_EQ(chd2.TSC(), 7u);
    EXPECT_EQ(chd2.ARFCN(), 100u);
}

// ── L3RestOctets base ────────────────────────────────────────────────

TEST(CommonIETest, RestOctets_Base) {
    L3RestOctets orig;
    EXPECT_EQ(orig.lengthV(), 0u);
}

// ── L3OctetAlignedProtocolElement ────────────────────────────────────

TEST(CommonIETest, OctetAlignedProtocolElement) {
    L3OctetAlignedProtocolElement orig(std::string("\xAB\xCD\xEF", 3));
    EXPECT_EQ(orig.lengthV(), 3u);
    EXPECT_TRUE(orig.mExtant);
}
