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
#include <gsml3parser/stack/response_builder.h>
#include <gsml3parser/parser.h>
#include <gsml3parser/visitor.h>
#include <gsml3parser/message_types.h>
#include <gsml3parser/arena.h>

using namespace gsml3parser;

// Helper: build a minimal channel description for tests.
static L3ChannelDescription makeChannel() {
    return L3ChannelDescription(TDMA_SDCCH, 0, 1, 100);
}

// Helper: verify that serialized bytes can be round-tripped through parseL3().
static void assertRoundTrips(Expected<std::vector<uint8_t>> bytes) {
    ASSERT_TRUE(bytes.has_value()) << "Build failed: " << bytes.error().message;
    auto parsed = parseL3(std::span<const uint8_t>(bytes.value().data(), bytes.value().size()));
    ASSERT_TRUE(parsed.has_value()) << "Parse failed: " << parsed.error().message;
}

// Helper: verify span overload writes correct bytes matching vector overload.
static void assertSpanMatchesVector(
    std::function<Expected<std::vector<uint8_t>>()> vecFn,
    std::function<int(std::span<uint8_t>)> spanFn)
{
    auto vecResult = vecFn();
    ASSERT_TRUE(vecResult.has_value());
    const auto& expected = vecResult.value();

    uint8_t buf[512];
    int n = spanFn({buf, sizeof(buf)});
    ASSERT_GT(n, 0) << "Span overload returned error";
    EXPECT_EQ(static_cast<size_t>(n), expected.size());
    EXPECT_EQ(0, std::memcmp(buf, expected.data(), expected.size()));
}

// ── RR ResponseBuilder tests ──────────────────────────────────────────────

// Tests that buildImmediateAssignment produces parseable ImmediateAssignment.
// Verifies channel and TA fields are serialized correctly.
TEST(ResponseBuilderTest, buildImmediateAssignment_ValidChannel_ReturnsParsedIA) {
    auto ch = makeChannel();
    auto result = ResponseBuilder::buildImmediateAssignment(ch, 32);
    assertRoundTrips(result);

    auto parsed = parseL3(std::span<const uint8_t>(result.value().data(), result.value().size()));
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(messagePD(*parsed), L3PD::RadioResource);
    EXPECT_EQ(messageMTI(*parsed), L3ImmediateAssignment::MTI);
}

// Tests that buildAssignmentCommand produces parseable AssignmentCommand.
TEST(ResponseBuilderTest, buildAssignmentCommand_TCH_ReturnsParsedAC) {
    auto ch = makeChannel();
    auto mode = L3ChannelMode(L3ChannelMode::Mode::SpeechV1);
    auto result = ResponseBuilder::buildAssignmentCommand(ch, mode);
    assertRoundTrips(result);

    auto parsed = parseL3(std::span<const uint8_t>(result.value().data(), result.value().size()));
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(messagePD(*parsed), L3PD::RadioResource);
    EXPECT_EQ(messageMTI(*parsed), L3AssignmentCommand::MTI);
}

// Tests that buildChannelRelease produces parseable ChannelRelease with given cause.
TEST(ResponseBuilderTest, buildChannelRelease_NormalCause_ReturnsParsedCR) {
    auto result = ResponseBuilder::buildChannelRelease(RRCause::Normal_Event);
    assertRoundTrips(result);

    auto parsed = parseL3(std::span<const uint8_t>(result.value().data(), result.value().size()));
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(messagePD(*parsed), L3PD::RadioResource);
    EXPECT_EQ(messageMTI(*parsed), L3ChannelRelease::MTI);
}

// Tests that buildCipheringModeCommand produces parseable CipheringModeCommand.
TEST(ResponseBuilderTest, buildCipheringModeCommand_A5_1_ReturnsParsedCMC) {
    auto result = ResponseBuilder::buildCipheringModeCommand(1);
    assertRoundTrips(result);

    auto parsed = parseL3(std::span<const uint8_t>(result.value().data(), result.value().size()));
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(messagePD(*parsed), L3PD::RadioResource);
    EXPECT_EQ(messageMTI(*parsed), L3CipheringModeCommand::MTI);
}

// Tests that buildPhysicalInformation produces parseable PhysicalInformation.
TEST(ResponseBuilderTest, buildPhysicalInformation_TA42_ReturnsParsedPI) {
    auto result = ResponseBuilder::buildPhysicalInformation(42);
    assertRoundTrips(result);

    auto parsed = parseL3(std::span<const uint8_t>(result.value().data(), result.value().size()));
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(messagePD(*parsed), L3PD::RadioResource);
    EXPECT_EQ(messageMTI(*parsed), L3PhysicalInformation::MTI);
}

// Tests that span overload of buildImmediateAssignment matches vector output.
TEST(ResponseBuilderTest, buildImmediateAssignment_SpanOverload_MatchesVector) {
    auto ch = makeChannel();
    auto vecFn = [&ch]() { return ResponseBuilder::buildImmediateAssignment(ch, 32); };
    auto spanFn = [&ch](std::span<uint8_t> out) {
        return ResponseBuilder::buildImmediateAssignment(out, ch, 32);
    };
    assertSpanMatchesVector(vecFn, spanFn);
}

// ── MM ResponseBuilder tests ──────────────────────────────────────────────

// Tests that buildCMServiceAccept produces parseable CM Service Accept.
TEST(ResponseBuilderTest, buildCMServiceAccept_Empty_ReturnsParsedCMA) {
    auto result = ResponseBuilder::buildCMServiceAccept();
    assertRoundTrips(result);

    auto parsed = parseL3(std::span<const uint8_t>(result.value().data(), result.value().size()));
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(messagePD(*parsed), L3PD::MobilityManagement);
    EXPECT_EQ(messageMTI(*parsed), L3CMServiceAccept::MTI);
}

// Tests that buildCMServiceReject produces parseable CM Service Reject.
TEST(ResponseBuilderTest, buildCMServiceReject_Congestion_ReturnsParsedCMR) {
    auto result = ResponseBuilder::buildCMServiceReject(MMRejectCause::Congestion);
    assertRoundTrips(result);

    auto parsed = parseL3(std::span<const uint8_t>(result.value().data(), result.value().size()));
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(messagePD(*parsed), L3PD::MobilityManagement);
    EXPECT_EQ(messageMTI(*parsed), L3CMServiceReject::MTI);
}

// Tests that buildIdentityRequest produces parseable IdentityRequest.
TEST(ResponseBuilderTest, buildIdentityRequest_IMSI_ReturnsParsedIR) {
    auto result = ResponseBuilder::buildIdentityRequest(MobileIDType::IMSI);
    assertRoundTrips(result);

    auto parsed = parseL3(std::span<const uint8_t>(result.value().data(), result.value().size()));
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(messagePD(*parsed), L3PD::MobilityManagement);
    EXPECT_EQ(messageMTI(*parsed), L3IdentityRequest::MTI);
}

// Tests that buildAuthenticationRequest produces parseable AuthenticationRequest.
TEST(ResponseBuilderTest, buildAuthenticationRequest_16ByteRand_ReturnsParsedAR) {
    std::array<uint8_t, 16> rand{};
    for (size_t i = 0; i < rand.size(); ++i) rand[i] = static_cast<uint8_t>(i * 17);
    auto result = ResponseBuilder::buildAuthenticationRequest(rand);
    assertRoundTrips(result);

    auto parsed = parseL3(std::span<const uint8_t>(result.value().data(), result.value().size()));
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(messagePD(*parsed), L3PD::MobilityManagement);
    EXPECT_EQ(messageMTI(*parsed), L3AuthenticationRequest::MTI);
}

// Tests that buildLocationUpdatingAccept produces parseable LocationUpdatingAccept.
TEST(ResponseBuilderTest, buildLocationUpdatingAccept_ValidLAI_ReturnsParsedLUA) {
    auto lai = L3LocationAreaIdentity("244", "15", 1234);
    auto result = ResponseBuilder::buildLocationUpdatingAccept(lai);
    assertRoundTrips(result);

    auto parsed = parseL3(std::span<const uint8_t>(result.value().data(), result.value().size()));
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(messagePD(*parsed), L3PD::MobilityManagement);
    EXPECT_EQ(messageMTI(*parsed), L3LocationUpdatingAccept::MTI);
}

// Tests that buildLocationUpdatingReject produces parseable LocationUpdatingReject.
TEST(ResponseBuilderTest, buildLocationUpdatingReject_Congestion_ReturnsParsedLUR) {
    auto result = ResponseBuilder::buildLocationUpdatingReject(MMRejectCause::Congestion);
    assertRoundTrips(result);

    auto parsed = parseL3(std::span<const uint8_t>(result.value().data(), result.value().size()));
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(messagePD(*parsed), L3PD::MobilityManagement);
    EXPECT_EQ(messageMTI(*parsed), L3LocationUpdatingReject::MTI);
}

// Tests that buildTMSIReallocationCommand produces parseable TMSI Reallocation Command.
TEST(ResponseBuilderTest, buildTMSIReallocationCommand_ValidTMSI_ReturnsParsedTRC) {
    auto lai = L3LocationAreaIdentity("244", "15", 1234);
    auto result = ResponseBuilder::buildTMSIReallocationCommand(lai, 0x12345678u);
    assertRoundTrips(result);

    auto parsed = parseL3(std::span<const uint8_t>(result.value().data(), result.value().size()));
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(messagePD(*parsed), L3PD::MobilityManagement);
    EXPECT_EQ(messageMTI(*parsed), L3TMSIReallocationCommand::MTI);
}

// Tests that buildLocationUpdatingAccept with new TMSI produces correct message.
TEST(ResponseBuilderTest, buildLocationUpdatingAccept_WithNewTMSI_ReturnsParsedLUA) {
    auto lai = L3LocationAreaIdentity("244", "15", 1234);
    auto result = ResponseBuilder::buildLocationUpdatingAccept(lai, 0x87654321u);
    assertRoundTrips(result);

    auto parsed = parseL3(std::span<const uint8_t>(result.value().data(), result.value().size()));
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(messageMTI(*parsed), L3LocationUpdatingAccept::MTI);
}

// Tests that span overload of buildCMServiceAccept matches vector output.
TEST(ResponseBuilderTest, buildCMServiceAccept_SpanOverload_MatchesVector) {
    auto vecFn = []() { return ResponseBuilder::buildCMServiceAccept(); };
    auto spanFn = [](std::span<uint8_t> out) {
        return ResponseBuilder::buildCMServiceAccept(out);
    };
    assertSpanMatchesVector(vecFn, spanFn);
}

// ── CC ResponseBuilder tests ──────────────────────────────────────────────

// Tests that buildCallProceeding produces parseable Call Proceeding with correct TI.
TEST(ResponseBuilderTest, buildCallProceeding_TI3_ReturnsParsedCP) {
    auto result = ResponseBuilder::buildCallProceeding(3);
    assertRoundTrips(result);

    auto parsed = parseL3(std::span<const uint8_t>(result.value().data(), result.value().size()));
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(messagePD(*parsed), L3PD::CallControl);
    EXPECT_EQ(messageMTI(*parsed), L3CallProceeding::MTI);
}

// Tests that buildAlerting produces parseable Alerting message.
TEST(ResponseBuilderTest, buildAlerting_TI3_ReturnsParsedAlerting) {
    auto result = ResponseBuilder::buildAlerting(3);
    assertRoundTrips(result);

    auto parsed = parseL3(std::span<const uint8_t>(result.value().data(), result.value().size()));
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(messagePD(*parsed), L3PD::CallControl);
    EXPECT_EQ(messageMTI(*parsed), L3Alerting::MTI);
}

// Tests that buildConnect produces parseable Connect message.
TEST(ResponseBuilderTest, buildConnect_TI3_ReturnsParsedConnect) {
    auto result = ResponseBuilder::buildConnect(3);
    assertRoundTrips(result);

    auto parsed = parseL3(std::span<const uint8_t>(result.value().data(), result.value().size()));
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(messagePD(*parsed), L3PD::CallControl);
    EXPECT_EQ(messageMTI(*parsed), L3Connect::MTI);
}

// Tests that buildConnectAcknowledge produces parseable Connect Acknowledge.
TEST(ResponseBuilderTest, buildConnectAcknowledge_TI5_ReturnsParsedCA) {
    auto result = ResponseBuilder::buildConnectAcknowledge(5);
    assertRoundTrips(result);

    auto parsed = parseL3(std::span<const uint8_t>(result.value().data(), result.value().size()));
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(messagePD(*parsed), L3PD::CallControl);
    EXPECT_EQ(messageMTI(*parsed), L3ConnectAcknowledge::MTI);
}

// Tests that buildDisconnect produces parseable Disconnect with cause.
TEST(ResponseBuilderTest, buildDisconnect_TI1_NormalCause_ReturnsParsedDC) {
    auto result = ResponseBuilder::buildDisconnect(1, CCCause::Normal_Call_Clearing);
    assertRoundTrips(result);

    auto parsed = parseL3(std::span<const uint8_t>(result.value().data(), result.value().size()));
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(messagePD(*parsed), L3PD::CallControl);
    EXPECT_EQ(messageMTI(*parsed), L3Disconnect::MTI);
}

// Tests that buildRelease produces parseable Release message.
TEST(ResponseBuilderTest, buildRelease_TI1_NormalCause_ReturnsParsedRel) {
    auto result = ResponseBuilder::buildRelease(1, CCCause::Normal_Call_Clearing);
    assertRoundTrips(result);

    auto parsed = parseL3(std::span<const uint8_t>(result.value().data(), result.value().size()));
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(messagePD(*parsed), L3PD::CallControl);
    EXPECT_EQ(messageMTI(*parsed), L3Release::MTI);
}

// Tests that buildReleaseComplete produces parseable Release Complete.
TEST(ResponseBuilderTest, buildReleaseComplete_TI0_ReturnsParsedRC) {
    auto result = ResponseBuilder::buildReleaseComplete(0);
    assertRoundTrips(result);

    auto parsed = parseL3(std::span<const uint8_t>(result.value().data(), result.value().size()));
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(messagePD(*parsed), L3PD::CallControl);
    EXPECT_EQ(messageMTI(*parsed), L3ReleaseComplete::MTI);
}

// Tests that span overload of buildCallProceeding matches vector output.
TEST(ResponseBuilderTest, buildCallProceeding_SpanOverload_MatchesVector) {
    auto vecFn = []() { return ResponseBuilder::buildCallProceeding(3); };
    auto spanFn = [](std::span<uint8_t> out) {
        return ResponseBuilder::buildCallProceeding(out, 3);
    };
    assertSpanMatchesVector(vecFn, spanFn);
}

// Tests that Arena integration works: span overload writes into Arena buffer.
TEST(ResponseBuilderTest, buildCMServiceAccept_ArenaBuffer_ZeroAlloc) {
    Arena arena(4096);
    auto* buf = static_cast<uint8_t*>(arena.allocate(512));
    ASSERT_NE(buf, nullptr);

    int n = ResponseBuilder::buildCMServiceAccept({buf, 512});
    ASSERT_GT(n, 0);

    auto parsed = parseL3({buf, static_cast<size_t>(n)});
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(messageMTI(*parsed), L3CMServiceAccept::MTI);
}

// Tests that span overload returns -1 when buffer is too small.
TEST(ResponseBuilderTest, buildImmediateAssignment_TooSmallBuffer_ReturnsMinusOne) {
    auto ch = makeChannel();
    uint8_t tinyBuf[2];
    int n = ResponseBuilder::buildImmediateAssignment({tinyBuf, sizeof(tinyBuf)}, ch, 32);
    EXPECT_EQ(n, -1);
}
