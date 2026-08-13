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

#include <gtest/gtest.h>
#include <gsml3parser/parser.h>
#include <gsml3parser/visitor.h>
#include <gsml3parser/types.h>
#include <gsml3parser/enums.h>
#include <gsml3parser/rr/l3rrmessages.h>

using namespace gsml3parser;

// GSM 04.08 9.1.19: Immediate Assignment
TEST(RRBuilders, ImmediateAssignment_FullFields) {
    auto msg = L3ImmediateAssignment::builder()
        .pageMode(L3PageMode(0))
        .dedicatedModeOrTBF(L3DedicatedModeOrTBF(false, false))
        .requestReference(L3RequestReference(1, 2, 3, 4))
        .channelDescription(L3ChannelDescription(TDMA_SDCCH, 0, 1, 100))
        .timingAdvance(L3TimingAdvance(32))
        .build();
    ParsedMessage pm{RRM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
    EXPECT_EQ((*bytes)[0], 0x60);
    EXPECT_EQ((*bytes)[1], 0x3f);

    auto reparsed = parseL3(*bytes);
    ASSERT_TRUE(reparsed);
    auto* ia = tryGet<L3ImmediateAssignment>(*reparsed);
    ASSERT_TRUE(ia);
    EXPECT_EQ(ia->timingAdvance().timingAdvance(), 32u);
}

// GSM 04.08 9.1.19: Immediate Assignment (minimal)
TEST(RRBuilders, ImmediateAssignment_Minimal) {
    auto msg = L3ImmediateAssignment::builder()
        .channelDescription(L3ChannelDescription(TDMA_TCHF, 1, 0, 50))
        .build();
    ParsedMessage pm{RRM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
}

// GSM 04.08 9.1.20: Immediate Assignment Reject
TEST(RRBuilders, ImmediateAssignmentReject_Wait30s) {
    auto msg = L3ImmediateAssignmentReject::builder()
        .waitTime(30)
        .build();
    ParsedMessage pm{RRM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
    EXPECT_EQ((*bytes)[0], 0x60);
    EXPECT_EQ((*bytes)[1], 0x3a);
}

// GSM 04.08 9.1.12: Physical Information
TEST(RRBuilders, PhysicalInformation_TA42) {
    auto msg = L3PhysicalInformation::builder()
        .timingAdvance(L3TimingAdvance(42))
        .build();
    ParsedMessage pm{RRM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
    auto reparsed = parseL3(*bytes);
    ASSERT_TRUE(reparsed);
    auto* pi = tryGet<L3PhysicalInformation>(*reparsed);
    ASSERT_TRUE(pi);
    EXPECT_EQ(pi->timingAdvance().timingAdvance(), 42u);
}

// GSM 04.08 9.1.18: Immediate Assignment Extended
TEST(RRBuilders, ImmediateAssignmentExtended_WithAdditionalChannel) {
    auto msg = L3ImmediateAssignmentExtended::builder()
        .channelDescription(L3ChannelDescription(TDMA_SDCCH, 0, 1, 100))
        .timingAdvance(L3TimingAdvance(64))
        .build();
    ParsedMessage pm{RRM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
    EXPECT_EQ((*bytes)[0], 0x60);
    EXPECT_EQ((*bytes)[1], 0x39);
}

// GSM 04.08 9.1.2: Assignment Command
TEST(RRBuilders, AssignmentCommand_Full) {
    auto msg = L3AssignmentCommand::builder()
        .channel(L3ChannelDescription(TDMA_TCHF, 1, 0, 50))
        .powerCommand(L3PowerCommand())
        .build();
    ParsedMessage pm{RRM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
    EXPECT_EQ((*bytes)[0], 0x60);
    EXPECT_EQ((*bytes)[1], 0x2e);
}

// GSM 04.08 9.1.15: Handover Command
TEST(RRBuilders, HandoverCommand_Full) {
    auto msg = L3HandoverCommand::builder()
        .cellDescription(L3CellDescription())
        .channelDescriptionAfter(L3ChannelDescription2(TDMA_TCHF, 1, 0, 50))
        .handoverReference(L3HandoverReference())
        .powerCommandAccessType(L3PowerCommandAndAccessType())
        .syncIndication(L3SynchronizationIndication())
        .build();
    ParsedMessage pm{RRM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
    EXPECT_EQ((*bytes)[0], 0x60);
    EXPECT_EQ((*bytes)[1], 0x2b);
}

// GSM 04.08 9.1.3: Assignment Complete
TEST(RRBuilders, AssignmentComplete_DefaultCause) {
    auto msg = L3AssignmentComplete::builder().build();
    ParsedMessage pm{RRM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
    EXPECT_EQ((*bytes)[0], 0x60);
    EXPECT_EQ((*bytes)[1], 0x29);

    auto reparsed = parseL3(*bytes);
    ASSERT_TRUE(reparsed);
    auto* ac = tryGet<L3AssignmentComplete>(*reparsed);
    ASSERT_TRUE(ac);
    EXPECT_EQ(ac->cause(), RRCause::Normal_Event);
}

// GSM 04.08 9.1.3: Assignment Complete with custom cause
TEST(RRBuilders, AssignmentComplete_CustomCause) {
    auto msg = L3AssignmentComplete::builder()
        .cause(RRCause::Unspecified)
        .build();
    ParsedMessage pm{RRM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = parseL3(*bytes);
    ASSERT_TRUE(reparsed);
    auto* ac = tryGet<L3AssignmentComplete>(*reparsed);
    ASSERT_TRUE(ac);
    EXPECT_EQ(ac->cause(), RRCause::Unspecified);
}

// GSM 04.08 9.1.3: Assignment Failure
TEST(RRBuilders, AssignmentFailure_Cause) {
    auto msg = L3AssignmentFailure::builder()
        .cause(RRCause::Unspecified)
        .build();
    ParsedMessage pm{RRM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
    EXPECT_EQ((*bytes)[0], 0x60);
    EXPECT_EQ((*bytes)[1], 0x2f);

    auto reparsed = parseL3(*bytes);
    ASSERT_TRUE(reparsed);
    auto* af = tryGet<L3AssignmentFailure>(*reparsed);
    ASSERT_TRUE(af);
    EXPECT_EQ(af->cause(), RRCause::Unspecified);
}

// GSM 04.08 9.1.16: Handover Complete
TEST(RRBuilders, HandoverComplete_DefaultCause) {
    auto msg = L3HandoverComplete::builder().build();
    ParsedMessage pm{RRM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
    EXPECT_EQ((*bytes)[0], 0x60);
    EXPECT_EQ((*bytes)[1], 0x2c);

    auto reparsed = parseL3(*bytes);
    ASSERT_TRUE(reparsed);
    auto* hoc = tryGet<L3HandoverComplete>(*reparsed);
    ASSERT_TRUE(hoc);
    EXPECT_EQ(hoc->cause(), RRCause::Normal_Event);
}

// GSM 04.08 9.1.17: Handover Failure
TEST(RRBuilders, HandoverFailure_Cause) {
    auto msg = L3HandoverFailure::builder()
        .cause(RRCause::Unspecified)
        .build();
    ParsedMessage pm{RRM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
    EXPECT_EQ((*bytes)[0], 0x60);
    EXPECT_EQ((*bytes)[1], 0x28);

    auto reparsed = parseL3(*bytes);
    ASSERT_TRUE(reparsed);
    auto* hof = tryGet<L3HandoverFailure>(*reparsed);
    ASSERT_TRUE(hof);
    EXPECT_EQ(hof->cause(), RRCause::Unspecified);
}

// GSM 04.08 9.1.7: Channel Release
TEST(RRBuilders, ChannelRelease_DefaultCause) {
    auto msg = L3ChannelRelease::builder().build();
    ParsedMessage pm{RRM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
    EXPECT_EQ((*bytes)[0], 0x60);
    EXPECT_EQ((*bytes)[1], 0x0d);

    auto reparsed = parseL3(*bytes);
    ASSERT_TRUE(reparsed);
    auto* cr = tryGet<L3ChannelRelease>(*reparsed);
    ASSERT_TRUE(cr);
    EXPECT_EQ(cr->cause(), RRCause::Normal_Event);
}

// GSM 04.08 9.1.7: Channel Release with custom cause
TEST(RRBuilders, ChannelRelease_CustomCause) {
    auto msg = L3ChannelRelease::builder()
        .cause(RRCause::Unspecified)
        .build();
    ParsedMessage pm{RRM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);

    auto reparsed = parseL3(*bytes);
    ASSERT_TRUE(reparsed);
    auto* cr = tryGet<L3ChannelRelease>(*reparsed);
    ASSERT_TRUE(cr);
    EXPECT_EQ(cr->cause(), RRCause::Unspecified);
}

// GSM 04.08 9.1.29: RR Status
TEST(RRBuilders, RRStatus_Cause) {
    auto msg = L3RRStatus::builder()
        .cause(RRCause::Protocol_Error_Unspecified)
        .build();
    ParsedMessage pm{RRM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
    EXPECT_EQ((*bytes)[0], 0x60);
    EXPECT_EQ((*bytes)[1], 0x12);

    auto reparsed = parseL3(*bytes);
    ASSERT_TRUE(reparsed);
    auto* rs = tryGet<L3RRStatus>(*reparsed);
    ASSERT_TRUE(rs);
    EXPECT_EQ(rs->cause(), RRCause::Protocol_Error_Unspecified);
}

// GSM 04.08 9.1.9: Ciphering Mode Command
TEST(RRBuilders, CipheringModeCommand_Full) {
    auto msg = L3CipheringModeCommand::builder()
        .ciphering(true)
        .algorithm(1)
        .build();
    ParsedMessage pm{RRM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
    EXPECT_EQ((*bytes)[0], 0x60);
    EXPECT_EQ((*bytes)[1], 0x35);

    auto reparsed = parseL3(*bytes);
    ASSERT_TRUE(reparsed);
    auto* cmc = tryGet<L3CipheringModeCommand>(*reparsed);
    ASSERT_TRUE(cmc);
    EXPECT_TRUE(cmc->isCiphering());
    EXPECT_EQ(cmc->algorithm(), 1);
}

// GSM 04.08 9.1.10: Ciphering Mode Complete (empty)
TEST(RRBuilders, CipheringModeComplete_Empty) {
    auto msg = L3CipheringModeComplete::builder().build();
    ParsedMessage pm{RRM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
    EXPECT_EQ((*bytes)[0], 0x60);
    EXPECT_EQ((*bytes)[1], 0x32);
}

// GSM 04.08 9.1.5: Channel Mode Modify
TEST(RRBuilders, ChannelModeModify_Full) {
    auto msg = L3ChannelModeModify::builder()
        .description(L3ChannelDescription(TDMA_TCHF, 1, 0, 50))
        .mode(L3ChannelMode(L3ChannelMode::SpeechV1))
        .build();
    ParsedMessage pm{RRM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
    EXPECT_EQ((*bytes)[0], 0x60);
    EXPECT_EQ((*bytes)[1], 0x10);
}

// GSM 04.08 9.1.6: Channel Mode Modify Acknowledge
TEST(RRBuilders, ChannelModeModifyAcknowledge_Full) {
    auto msg = L3ChannelModeModifyAcknowledge::builder()
        .description(L3ChannelDescription(TDMA_TCHF, 1, 0, 50))
        .mode(L3ChannelMode(L3ChannelMode::SpeechV1))
        .build();
    ParsedMessage pm{RRM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
    EXPECT_EQ((*bytes)[0], 0x60);
    EXPECT_EQ((*bytes)[1], 0x17);
}

// GSM 04.08 9.1.1: Additional Assignment
TEST(RRBuilders, AdditionalAssignment_WithPowerCommand) {
    auto msg = L3AdditionalAssignment::builder()
        .additionalChannel(L3AdditionalChannelDescription(TDMA_SDCCH, 0, 1, 100))
        .powerCommand(L3PowerCommand())
        .build();
    ParsedMessage pm{RRM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
    EXPECT_EQ((*bytes)[0], 0x60);
    EXPECT_EQ((*bytes)[1], 0x3b);

    auto reparsed = parseL3(*bytes);
    ASSERT_TRUE(reparsed);
    auto* aa = tryGet<L3AdditionalAssignment>(*reparsed);
    ASSERT_TRUE(aa);
    EXPECT_TRUE(aa->hasPowerCommand());
}

// GSM 04.08 9.1.4: Configuration Change Command
TEST(RRBuilders, ConfigurationChangeCommand_WithChanDesc) {
    auto msg = L3ConfigurationChangeCommand::builder()
        .channelDescription(L3ChannelDescription(TDMA_TCHF, 1, 0, 50))
        .build();
    ParsedMessage pm{RRM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
    EXPECT_EQ((*bytes)[0], 0x60);
    EXPECT_EQ((*bytes)[1], 0x30);
}

// GSM 04.08 9.1.4: Configuration Change Acknowledge (empty)
TEST(RRBuilders, ConfigurationChangeAcknowledge_Empty) {
    auto msg = L3ConfigurationChangeAcknowledge::builder().build();
    ParsedMessage pm{RRM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
    EXPECT_EQ((*bytes)[0], 0x60);
    EXPECT_EQ((*bytes)[1], 0x31);
}

// GSM 04.08 9.1.4: Configuration Change Reject
TEST(RRBuilders, ConfigurationChangeReject_Cause) {
    auto msg = L3ConfigurationChangeReject::builder()
        .cause(RRCause::Protocol_Error_Unspecified)
        .build();
    ParsedMessage pm{RRM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
    EXPECT_EQ((*bytes)[0], 0x60);
    EXPECT_EQ((*bytes)[1], 0x33);

    auto reparsed = parseL3(*bytes);
    ASSERT_TRUE(reparsed);
    auto* ccr = tryGet<L3ConfigurationChangeReject>(*reparsed);
    ASSERT_TRUE(ccr);
    EXPECT_EQ(ccr->cause(), RRCause::Protocol_Error_Unspecified);
}

// GSM 04.08 9.1.8: Partial Release
TEST(RRBuilders, PartialRelease_WithChanDesc) {
    auto msg = L3PartialRelease::builder()
        .channelDescription(L3ChannelDescription(TDMA_TCHF, 1, 0, 50))
        .build();
    ParsedMessage pm{RRM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
    EXPECT_EQ((*bytes)[0], 0x60);
    EXPECT_EQ((*bytes)[1], 0x0a);
}

// GSM 04.08 9.1.8: Partial Release Complete (empty)
TEST(RRBuilders, PartialReleaseComplete_Empty) {
    auto msg = L3PartialReleaseComplete::builder().build();
    ParsedMessage pm{RRM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
    EXPECT_EQ((*bytes)[0], 0x60);
    EXPECT_EQ((*bytes)[1], 0x0f);
}

// GSM 04.08 9.1.31: System Information Type 1
TEST(RRBuilders, SystemInformationType1_Full) {
    auto si1 = L3SystemInformationType1::builder()
        .cellChannelDescription(L3FrequencyList())
        .rachControlParameters(L3RACHControlParameters())
        .build();
    ParsedMessage pm{RRM{std::move(si1)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
    EXPECT_EQ((*bytes)[0], 0x60);
    EXPECT_EQ((*bytes)[1], 0x19);

    auto reparsed = parseL3(*bytes);
    ASSERT_TRUE(reparsed);
    auto* parsed = tryGet<L3SystemInformationType1>(*reparsed);
    ASSERT_TRUE(parsed);
}

// GSM 04.08 9.1.32: System Information Type 2
TEST(RRBuilders, SystemInformationType2_Full) {
    auto si2 = L3SystemInformationType2::builder()
        .bcchFrequencyList(L3BCCHFrequencyList())
        .nccPermitted(L3NCCPermitted())
        .rachControlParameters(L3RACHControlParameters())
        .build();
    ParsedMessage pm{RRM{std::move(si2)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
    EXPECT_EQ((*bytes)[0], 0x60);
    EXPECT_EQ((*bytes)[1], 0x1a);

    auto reparsed = parseL3(*bytes);
    ASSERT_TRUE(reparsed);
    auto* parsed = tryGet<L3SystemInformationType2>(*reparsed);
    ASSERT_TRUE(parsed);
}

// GSM 04.08 9.1.33: System Information Type 2bis
TEST(RRBuilders, SystemInformationType2bis_Full) {
    auto si2bis = L3SystemInformationType2bis::builder()
        .bcchFrequencyList(L3BCCHFrequencyList())
        .rachControlParameters(L3RACHControlParameters())
        .build();
    ParsedMessage pm{RRM{std::move(si2bis)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
    EXPECT_EQ((*bytes)[0], 0x60);
    EXPECT_EQ((*bytes)[1], 0x02);

    auto reparsed = parseL3(*bytes);
    ASSERT_TRUE(reparsed);
    auto* parsed = tryGet<L3SystemInformationType2bis>(*reparsed);
    ASSERT_TRUE(parsed);
}

// GSM 04.08 9.1.34: System Information Type 2ter
TEST(RRBuilders, SystemInformationType2ter_Full) {
    auto si2ter = L3SystemInformationType2ter::builder()
        .bcchFrequencyList(L3BCCHFrequencyList())
        .build();
    ParsedMessage pm{RRM{std::move(si2ter)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
    EXPECT_EQ((*bytes)[0], 0x60);
    EXPECT_EQ((*bytes)[1], 0x03);

    auto reparsed = parseL3(*bytes);
    ASSERT_TRUE(reparsed);
    auto* parsed = tryGet<L3SystemInformationType2ter>(*reparsed);
    ASSERT_TRUE(parsed);
}

// GSM 04.08 9.1.35: System Information Type 3
TEST(RRBuilders, SystemInformationType3_FullCell) {
    auto si3 = L3SystemInformationType3::builder()
        .cellIdentity(L3CellIdentity(0x1234))
        .locationAreaIdentity(L3LocationAreaIdentity("250", "01", 0x5678))
        .controlChannelDescription(L3ControlChannelDescription())
        .cellOptions(L3CellOptionsBCCH{})
        .cellSelectionParameters(L3CellSelectionParameters{})
        .rachControlParameters(L3RACHControlParameters{})
        .build();
    ParsedMessage pm{RRM{std::move(si3)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
    EXPECT_EQ((*bytes)[0], 0x60);
    EXPECT_EQ((*bytes)[1], 0x1b);

    auto reparsed = parseL3(*bytes);
    ASSERT_TRUE(reparsed);
    auto* parsed = tryGet<L3SystemInformationType3>(*reparsed);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->ci().id(), 0x1234u);
}

// GSM 04.08 9.1.36: System Information Type 4
TEST(RRBuilders, SystemInformationType4_Full) {
    auto si4 = L3SystemInformationType4::builder()
        .locationAreaIdentity(L3LocationAreaIdentity("250", "01", 0x5678))
        .cellSelectionParameters(L3CellSelectionParameters{})
        .rachControlParameters(L3RACHControlParameters{})
        .build();
    ParsedMessage pm{RRM{std::move(si4)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
    EXPECT_EQ((*bytes)[0], 0x60);
    EXPECT_EQ((*bytes)[1], 0x1c);

    auto reparsed = parseL3(*bytes);
    ASSERT_TRUE(reparsed);
    auto* parsed = tryGet<L3SystemInformationType4>(*reparsed);
    ASSERT_TRUE(parsed);
}

// GSM 04.08 9.1.37: System Information Type 5
TEST(RRBuilders, SystemInformationType5_Full) {
    auto si5 = L3SystemInformationType5::builder()
        .bcchFrequencyList(L3BCCHFrequencyList())
        .build();
    ParsedMessage pm{RRM{std::move(si5)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
    EXPECT_EQ((*bytes)[0], 0x60);
    EXPECT_EQ((*bytes)[1], 0x1d);

    auto reparsed = parseL3(*bytes);
    ASSERT_TRUE(reparsed);
    auto* parsed = tryGet<L3SystemInformationType5>(*reparsed);
    ASSERT_TRUE(parsed);
}

// GSM 04.08 9.1.38: System Information Type 5bis
TEST(RRBuilders, SystemInformationType5bis_Full) {
    auto si5bis = L3SystemInformationType5bis::builder()
        .bcchFrequencyList(L3BCCHFrequencyList())
        .build();
    ParsedMessage pm{RRM{std::move(si5bis)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
    EXPECT_EQ((*bytes)[0], 0x60);
    EXPECT_EQ((*bytes)[1], 0x05);

    auto reparsed = parseL3(*bytes);
    ASSERT_TRUE(reparsed);
    auto* parsed = tryGet<L3SystemInformationType5bis>(*reparsed);
    ASSERT_TRUE(parsed);
}

// GSM 04.08 9.1.39: System Information Type 5ter
TEST(RRBuilders, SystemInformationType5ter_Full) {
    auto si5ter = L3SystemInformationType5ter::builder()
        .bcchFrequencyList(L3BCCHFrequencyList())
        .build();
    ParsedMessage pm{RRM{std::move(si5ter)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
    EXPECT_EQ((*bytes)[0], 0x60);
    EXPECT_EQ((*bytes)[1], 0x06);

    auto reparsed = parseL3(*bytes);
    ASSERT_TRUE(reparsed);
    auto* parsed = tryGet<L3SystemInformationType5ter>(*reparsed);
    ASSERT_TRUE(parsed);
}

// GSM 04.08 9.1.40: System Information Type 6
TEST(RRBuilders, SystemInformationType6_Full) {
    auto si6 = L3SystemInformationType6::builder()
        .cellIdentity(L3CellIdentity(0x1234))
        .locationAreaIdentity(L3LocationAreaIdentity("250", "01", 0x5678))
        .cellOptions(L3CellOptionsSACCH{})
        .nccPermitted(L3NCCPermitted())
        .build();
    ParsedMessage pm{RRM{std::move(si6)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
    EXPECT_EQ((*bytes)[0], 0x60);
    EXPECT_EQ((*bytes)[1], 0x1e);

    auto reparsed = parseL3(*bytes);
    ASSERT_TRUE(reparsed);
    auto* parsed = tryGet<L3SystemInformationType6>(*reparsed);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->ci().id(), 0x1234u);
}

// GSM 04.08 9.1.41: System Information Type 7
TEST(RRBuilders, SystemInformationType7_Full) {
    auto si7 = L3SystemInformationType7::builder()
        .rachControl(L3RACHControlParameters())
        .build();
    ParsedMessage pm{RRM{std::move(si7)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
    EXPECT_EQ((*bytes)[0], 0x60);
    EXPECT_EQ((*bytes)[1], 0x1f);

    auto reparsed = parseL3(*bytes);
    ASSERT_TRUE(reparsed);
    auto* parsed = tryGet<L3SystemInformationType7>(*reparsed);
    ASSERT_TRUE(parsed);
}

// GSM 04.08 9.1.42: System Information Type 8
TEST(RRBuilders, SystemInformationType8_Full) {
    auto si8 = L3SystemInformationType8::builder()
        .nccPermitted(L3NCCPermitted())
        .rachControl(L3RACHControlParameters())
        .build();
    ParsedMessage pm{RRM{std::move(si8)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
    EXPECT_EQ((*bytes)[0], 0x60);
    EXPECT_EQ((*bytes)[1], 0x18);

    auto reparsed = parseL3(*bytes);
    ASSERT_TRUE(reparsed);
    auto* parsed = tryGet<L3SystemInformationType8>(*reparsed);
    ASSERT_TRUE(parsed);
}

// GSM 04.08 9.1.43: System Information Type 9
TEST(RRBuilders, SystemInformationType9_Full) {
    auto si9 = L3SystemInformationType9::builder()
        .cellIdentity(L3CellIdentity(0x5678))
        .cellSelectionParameters(L3CellSelectionParameters{})
        .cellOptions(L3CellOptionsBCCH{})
        .build();
    ParsedMessage pm{RRM{std::move(si9)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
    EXPECT_EQ((*bytes)[0], 0x60);
    EXPECT_EQ((*bytes)[1], 0x04);

    auto reparsed = parseL3(*bytes);
    ASSERT_TRUE(reparsed);
    auto* parsed = tryGet<L3SystemInformationType9>(*reparsed);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->ci().id(), 0x5678u);
}

// GSM 04.08 9.1.43a: System Information Type 13
TEST(RRBuilders, SystemInformationType13_Full) {
    auto si13 = L3SystemInformationType13::builder()
        .build();
    ParsedMessage pm{RRM{std::move(si13)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
    EXPECT_EQ((*bytes)[0], 0x60);
    EXPECT_EQ((*bytes)[1], 0x00);

    auto reparsed = parseL3(*bytes);
    ASSERT_TRUE(reparsed);
    auto* parsed = tryGet<L3SystemInformationType13>(*reparsed);
    ASSERT_TRUE(parsed);
}

// GSM 04.08 9.1.43b: System Information Type 16
TEST(RRBuilders, SystemInformationType16_Full) {
    auto si16 = L3SystemInformationType16::builder()
        .cellIdentity(L3CellIdentity(0x9ABC))
        .cellSelectionParameters(L3CellSelectionParameters{})
        .cellOptions(L3CellOptionsBCCH{})
        .build();
    ParsedMessage pm{RRM{std::move(si16)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
    EXPECT_EQ((*bytes)[0], 0x60);
    EXPECT_EQ((*bytes)[1], 0x3d);

    auto reparsed = parseL3(*bytes);
    ASSERT_TRUE(reparsed);
    auto* parsed = tryGet<L3SystemInformationType16>(*reparsed);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->ci().id(), 0x9ABCu);
}

// GSM 04.08 9.1.43c: System Information Type 17
TEST(RRBuilders, SystemInformationType17_Full) {
    auto si17 = L3SystemInformationType17::builder()
        .rachControl(L3RACHControlParameters())
        .build();
    ParsedMessage pm{RRM{std::move(si17)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
    EXPECT_EQ((*bytes)[0], 0x60);
    EXPECT_EQ((*bytes)[1], 0x3e);

    auto reparsed = parseL3(*bytes);
    ASSERT_TRUE(reparsed);
    auto* parsed = tryGet<L3SystemInformationType17>(*reparsed);
    ASSERT_TRUE(parsed);
}
