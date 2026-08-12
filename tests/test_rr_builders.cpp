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
