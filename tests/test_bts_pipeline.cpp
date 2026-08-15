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

// Full BTS Pipeline integration tests.
// Models the complete cycle: Builder -> writeL3Bytes -> LAPDm wrap -> unwrap -> parse -> verify.
// These tests validate that all BTS-critical components work together end-to-end.

#include <gtest/gtest.h>
#include <gsml3parser/parser.h>
#include <gsml3parser/visitor.h>
#include <gsml3parser/lapdm.h>
#include <gsml3parser/dispatcher.h>
#include <gsml3parser/rr/l3rrmessages.h>
#include <gsml3parser/mm/l3mmmessages.h>
#include <gsml3parser/cc/l3ccmessages.h>

using namespace gsml3parser;
using namespace gsml3parser::lapdm;

// Helper: full pipeline round-trip for any ParsedMessage.
static ParsedMessage pipelineRoundTrip(const ParsedMessage& msg) {
    auto l3Bytes = writeL3Bytes(msg);
    if (!l3Bytes) {
        ADD_FAILURE() << "writeL3Bytes failed";
        return ParsedMessage{RRM{L3ChannelRelease{RRCause::Normal_Event}}};
    }

    auto lapdmFrame = wrapL3(l3Bytes.value(), SAPI::SAPI0);
    if (lapdmFrame.size() != l3Bytes.value().size() + 2) {
        ADD_FAILURE() << "wrapL3 size mismatch";
        return ParsedMessage{RRM{L3ChannelRelease{RRCause::Normal_Event}}};
    }

    auto unwrapped = unwrapL3(lapdmFrame);
    if (!unwrapped) {
        ADD_FAILURE() << "unwrapL3 failed";
        return ParsedMessage{RRM{L3ChannelRelease{RRCause::Normal_Event}}};
    }
    if (unwrapped.value() != l3Bytes.value()) {
        ADD_FAILURE() << "unwrapL3 content mismatch";
    }

    auto reparsed = parseL3(unwrapped.value());
    if (!reparsed) {
        ADD_FAILURE() << "parseL3 failed after pipeline round-trip";
        return ParsedMessage{RRM{L3ChannelRelease{RRCause::Normal_Event}}};
    }
    return reparsed.value();
}

// Full BTS Paging cycle: Builder -> L3 bytes -> LAPDm -> unwrap -> parse -> verify
TEST(BTSPipeline, FullPagingCycle) {
    auto paging = L3PagingRequestType2::builder()
        .addTMSI(0x12345678, ChannelType::SDCCHType)
        .build();

    ParsedMessage pm{RRM{std::move(paging)}};
    auto reparsed = pipelineRoundTrip(pm);

    auto* paged = tryGet<L3PagingRequestType2>(reparsed);
    ASSERT_TRUE(paged);
    EXPECT_EQ(paged->tmsis()[0], 0x12345678u);
}

// Full BTS Channel Release cycle: Builder -> L3 bytes -> LAPDm -> unwrap -> parse -> verify
TEST(BTSPipeline, FullChannelReleaseCycle) {
    auto release = L3ChannelRelease::builder()
        .cause(RRCause::Normal_Event)
        .build();

    ParsedMessage pm{RRM{std::move(release)}};
    auto reparsed = pipelineRoundTrip(pm);

    auto* cr = tryGet<L3ChannelRelease>(reparsed);
    ASSERT_TRUE(cr);
    EXPECT_EQ(cr->cause(), RRCause::Normal_Event);
}

// Full BTS Immediate Assignment cycle: Builder -> L3 bytes -> LAPDm -> unwrap -> parse -> verify
TEST(BTSPipeline, FullImmediateAssignmentCycle) {
    auto ia = L3ImmediateAssignment::builder()
        .channelDescription(L3ChannelDescription(TDMA_SDCCH, 0, 1, 100))
        .timingAdvance(L3TimingAdvance(32))
        .build();

    ParsedMessage pm{RRM{std::move(ia)}};
    auto reparsed = pipelineRoundTrip(pm);

    auto* parsed = tryGet<L3ImmediateAssignment>(reparsed);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->timingAdvance().timingAdvance(), 32u);
}

// Full BTS System Information Type 3 cycle: Builder -> L3 bytes -> LAPDm -> unwrap -> parse -> verify
TEST(BTSPipeline, FullSystemInformationType3Cycle) {
    auto si3 = L3SystemInformationType3::builder()
        .cellIdentity(L3CellIdentity(0x1234))
        .locationAreaIdentity(L3LocationAreaIdentity("250", "01", 0x5678))
        .controlChannelDescription(L3ControlChannelDescription(0, 1, 2, 1, 0, 0, 4, 10))
        .build();

    ParsedMessage pm{RRM{std::move(si3)}};
    auto reparsed = pipelineRoundTrip(pm);

    auto* parsed = tryGet<L3SystemInformationType3>(reparsed);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->ci().id(), 0x1234u);
}

// Full BTS Assignment Command cycle: Builder -> L3 bytes -> LAPDm -> unwrap -> parse -> verify
TEST(BTSPipeline, FullAssignmentCommandCycle) {
    auto ac = L3AssignmentCommand::builder()
        .channel(L3ChannelDescription(TDMA_TCHF, 1, 0, 50))
        .build();

    ParsedMessage pm{RRM{std::move(ac)}};
    auto reparsed = pipelineRoundTrip(pm);

    auto* parsed = tryGet<L3AssignmentCommand>(reparsed);
    ASSERT_TRUE(parsed);
}

// Full BTS Handover Command cycle: Builder -> L3 bytes -> LAPDm -> unwrap -> parse -> verify
TEST(BTSPipeline, FullHandoverCommandCycle) {
    auto ho = L3HandoverCommand::builder()
        .cellDescription(L3CellDescription())
        .channelDescriptionAfter(L3ChannelDescription2(TDMA_TCHF, 1, 0, 50))
        .handoverReference(L3HandoverReference())
        .powerCommandAccessType(L3PowerCommandAndAccessType())
        .syncIndication(L3SynchronizationIndication())
        .build();

    ParsedMessage pm{RRM{std::move(ho)}};
    auto reparsed = pipelineRoundTrip(pm);

    auto* parsed = tryGet<L3HandoverCommand>(reparsed);
    ASSERT_TRUE(parsed);
}

// Full BTS Ciphering Mode Command cycle: Builder -> L3 bytes -> LAPDm -> unwrap -> parse -> verify
TEST(BTSPipeline, FullCipheringModeCycle) {
    auto cmc = L3CipheringModeCommand::builder()
        .ciphering(true)
        .algorithm(3)
        .build();

    ParsedMessage pm{RRM{std::move(cmc)}};
    auto reparsed = pipelineRoundTrip(pm);

    auto* parsed = tryGet<L3CipheringModeCommand>(reparsed);
    ASSERT_TRUE(parsed);
    EXPECT_TRUE(parsed->isCiphering());
}

// Full BTS RR Status cycle: Builder -> L3 bytes -> LAPDm -> unwrap -> parse -> verify
TEST(BTSPipeline, FullRRStatusCycle) {
    auto rs = L3RRStatus::builder()
        .cause(RRCause::Normal_Event)
        .build();

    ParsedMessage pm{RRM{std::move(rs)}};
    auto reparsed = pipelineRoundTrip(pm);

    auto* parsed = tryGet<L3RRStatus>(reparsed);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->cause(), RRCause::Normal_Event);
}

// Full BTS MM Location Updating Request cycle: Builder -> L3 bytes -> LAPDm -> unwrap -> parse -> verify
TEST(BTSPipeline, FullLocationUpdatingRequestCycle) {
    auto lur = L3LocationUpdatingRequest::builder()
        .updateType(0)
        .build();

    ParsedMessage pm{MMM{std::move(lur)}};
    auto reparsed = pipelineRoundTrip(pm);

    auto* parsed = tryGet<L3LocationUpdatingRequest>(reparsed);
    ASSERT_TRUE(parsed);
}

// Full BTS MM TMSI Reallocation Command cycle: Builder -> L3 bytes -> LAPDm -> unwrap -> parse -> verify
TEST(BTSPipeline, FullTMSIReallocationCycle) {
    auto trc = L3TMSIReallocationCommand::builder()
        .build();

    ParsedMessage pm{MMM{std::move(trc)}};
    auto reparsed = pipelineRoundTrip(pm);

    auto* parsed = tryGet<L3TMSIReallocationCommand>(reparsed);
    ASSERT_TRUE(parsed);
}

// Full BTS CC Setup cycle: Builder -> L3 bytes -> LAPDm -> unwrap -> parse -> verify
TEST(BTSPipeline, FullCCSetupCycle) {
    auto setup = L3Setup::builder()
        .ti(7)
        .build();

    ParsedMessage pm{CCM{std::move(setup)}};
    auto reparsed = pipelineRoundTrip(pm);

    auto* parsed = tryGet<L3Setup>(reparsed);
    ASSERT_TRUE(parsed);
}

// Full BTS CC Disconnect cycle: Builder -> L3 bytes -> LAPDm -> unwrap -> parse -> verify
TEST(BTSPipeline, FullCCDisconnectCycle) {
    auto disc = L3Disconnect::builder()
        .ti(5)
        .cause(CCCause::Normal_Call_Clearing)
        .build();

    ParsedMessage pm{CCM{std::move(disc)}};
    auto reparsed = pipelineRoundTrip(pm);

    auto* parsed = tryGet<L3Disconnect>(reparsed);
    ASSERT_TRUE(parsed);
}

// Full BTS GMM Attach Accept cycle: Builder -> L3 bytes -> LAPDm -> unwrap -> parse -> verify
TEST(BTSPipeline, FullGMMAttachAcceptCycle) {
    auto ar = L3AttachAccept::builder()
        .build();

    ParsedMessage pm{GMM{std::move(ar)}};
    auto reparsed = pipelineRoundTrip(pm);

    auto* parsed = tryGet<L3AttachAccept>(reparsed);
    ASSERT_TRUE(parsed);
}

// Full BTS SM Activate PDP Context Request cycle: Builder -> L3 bytes -> LAPDm -> unwrap -> parse -> verify
TEST(BTSPipeline, FullSMActivatePDPContextCycle) {
    auto apdp = L3ActivatePDPContextRequest::builder()
        .build();

    ParsedMessage pm{SM{std::move(apdp)}};
    auto reparsed = pipelineRoundTrip(pm);

    auto* parsed = tryGet<L3ActivatePDPContextRequest>(reparsed);
    ASSERT_TRUE(parsed);
}

// Dispatcher integration: register handlers, dispatch through raw bytes, verify callbacks
TEST(BTSPipeline, DispatcherWithPipeline) {
    bool releaseCalled = false;
    bool pagingCalled = false;
    bool fallbackCalled = false;

    ProtocolDispatcher disp;

    disp.registerHandler(L3PD::RadioResource, L3ChannelRelease::MTI,
        makeSharedHandler([&](const ParsedMessage& msg, void*) {
            auto* cr = tryGet<L3ChannelRelease>(msg);
            ASSERT_TRUE(cr);
            EXPECT_EQ(cr->cause(), RRCause::Normal_Event);
            releaseCalled = true;
        }));

    disp.registerHandler(L3PD::RadioResource, L3PagingRequestType2::MTI,
        makeSharedHandler([&](const ParsedMessage& msg, void*) {
            auto* pr = tryGet<L3PagingRequestType2>(msg);
            ASSERT_TRUE(pr);
            EXPECT_EQ(pr->tmsis()[0], 0x12345678u);
            pagingCalled = true;
        }));

    disp.setFallbackHandler(makeSharedHandler([&](const ParsedMessage&, void*) {
        fallbackCalled = true;
    }));

    // Build and dispatch ChannelRelease through full pipeline.
    {
        auto release = L3ChannelRelease::builder()
            .cause(RRCause::Normal_Event)
            .build();
        ParsedMessage pm{RRM{std::move(release)}};
        auto l3Bytes = writeL3Bytes(pm);
        ASSERT_TRUE(l3Bytes);

        auto lapdmFrame = wrapL3(*l3Bytes, SAPI::SAPI0);
        auto unwrapped = unwrapL3(lapdmFrame);
        ASSERT_TRUE(unwrapped);

        EXPECT_TRUE(disp.dispatchRaw(*unwrapped));
    }
    EXPECT_TRUE(releaseCalled);

    // Build and dispatch PagingRequestType2 through full pipeline.
    {
        auto paging = L3PagingRequestType2::builder()
            .addTMSI(0x12345678, ChannelType::SDCCHType)
            .build();
        ParsedMessage pm{RRM{std::move(paging)}};
        auto l3Bytes = writeL3Bytes(pm);
        ASSERT_TRUE(l3Bytes);

        auto lapdmFrame = wrapL3(*l3Bytes, SAPI::SAPI0);
        auto unwrapped = unwrapL3(lapdmFrame);
        ASSERT_TRUE(unwrapped);

        EXPECT_TRUE(disp.dispatchRaw(*unwrapped));
    }
    EXPECT_TRUE(pagingCalled);

    // Dispatch a message type with no specific handler -> fallback.
    {
        auto ci = L3CipheringModeComplete::builder().build();
        ParsedMessage pm{RRM{std::move(ci)}};
        auto l3Bytes = writeL3Bytes(pm);
        ASSERT_TRUE(l3Bytes);

        EXPECT_TRUE(disp.dispatchRaw(*l3Bytes));
    }
    EXPECT_TRUE(fallbackCalled);
}

// Multi-SAPI pipeline: messages on different SAPIs are correctly framed
TEST(BTSPipeline, MultiSAPIL3Messages) {
    auto release = L3ChannelRelease::builder()
        .cause(RRCause::Normal_Event)
        .build();
    ParsedMessage pm{RRM{std::move(release)}};
    auto l3Bytes = writeL3Bytes(pm);
    ASSERT_TRUE(l3Bytes);

    // Frame on SAPI0 (signaling).
    auto frame0 = wrapL3(*l3Bytes, SAPI::SAPI0);
    EXPECT_EQ(frame0[0], 0x01); // SAPI0, CR=0, EA=1
    EXPECT_EQ(frame0[1], 0x03); // UI

    // Frame on SAPI3 (data).
    auto frame3 = wrapL3(*l3Bytes, SAPI::SAPI3);
    EXPECT_EQ(frame3[0], 0x31); // SAPI3, CR=0, EA=1
    EXPECT_EQ(frame3[1], 0x03); // UI

    // Both unwrap to the same L3 payload.
    auto unwrapped0 = unwrapL3(frame0);
    auto unwrapped3 = unwrapL3(frame3);
    ASSERT_TRUE(unwrapped0);
    ASSERT_TRUE(unwrapped3);
    EXPECT_EQ(*unwrapped0, *unwrapped3);
}

// Domain handler integration: catch-all for all RR messages
TEST(BTSPipeline, DomainHandlerFullPipeline) {
    int rrCount = 0;
    int mmCount = 0;

    ProtocolDispatcher disp;

    disp.registerDomainHandler(L3PD::RadioResource,
        makeSharedHandler([&](const ParsedMessage& msg, void*) {
            EXPECT_EQ(messagePD(msg), L3PD::RadioResource);
            rrCount++;
        }));

    disp.registerDomainHandler(L3PD::MobilityManagement,
        makeSharedHandler([&](const ParsedMessage& msg, void*) {
            EXPECT_EQ(messagePD(msg), L3PD::MobilityManagement);
            mmCount++;
        }));

    // Send multiple RR messages through pipeline.
    for (int i = 0; i < 3; ++i) {
        auto release = L3ChannelRelease::builder()
            .cause(RRCause::Normal_Event)
            .build();
        ParsedMessage pm{RRM{std::move(release)}};
        auto l3Bytes = writeL3Bytes(pm);
        ASSERT_TRUE(l3Bytes);

        auto lapdmFrame = wrapL3(*l3Bytes, SAPI::SAPI0);
        auto unwrapped = unwrapL3(lapdmFrame);
        ASSERT_TRUE(unwrapped);

        EXPECT_TRUE(disp.dispatchRaw(*unwrapped));
    }

    // Send MM messages through pipeline.
    {
        auto cmAccept = L3CMServiceAccept::builder().build();
        ParsedMessage pm{MMM{std::move(cmAccept)}};
        auto l3Bytes = writeL3Bytes(pm);
        ASSERT_TRUE(l3Bytes);

        auto lapdmFrame = wrapL3(*l3Bytes, SAPI::SAPI0);
        auto unwrapped = unwrapL3(lapdmFrame);
        ASSERT_TRUE(unwrapped);

        EXPECT_TRUE(disp.dispatchRaw(*unwrapped));
    }

    EXPECT_EQ(rrCount, 3);
    EXPECT_EQ(mmCount, 1);
}

// LAPDm frame validation: detect malformed frames in pipeline
TEST(BTSPipeline, MalformedLAPDmFrame) {
    uint8_t shortFrame[] = {0x01};
    auto result = unwrapL3(std::span<const uint8_t>(shortFrame));
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, ParseError::Code::TruncatedInput);
}

// Invalid L3 data: dispatchRaw returns false on parse error
TEST(BTSPipeline, InvalidL3DataInPipeline) {
    ProtocolDispatcher disp;

    // Use a single byte that is too short to be a valid L3 message.
    uint8_t invalidData[] = {0x00};

    // dispatchRaw should return false since the data is too short to parse.
    EXPECT_FALSE(disp.dispatchRaw(std::span<const uint8_t>(invalidData)));
}

// Full pipeline with context pointer: user state is passed through dispatcher
TEST(BTSPipeline, PipelineWithContext) {
    struct ChannelState {
        int msgCount = 0;
        bool releaseReceived = false;
    };

    ChannelState state;

    ProtocolDispatcher disp;

    disp.registerHandler(L3PD::RadioResource, L3ChannelRelease::MTI,
        makeSharedHandler([&state](const ParsedMessage&, void*) {
            state.msgCount++;
            state.releaseReceived = true;
        }));

    auto release = L3ChannelRelease::builder()
        .cause(RRCause::Preemptive_Release)
        .build();
    ParsedMessage pm{RRM{std::move(release)}};
    auto l3Bytes = writeL3Bytes(pm);
    ASSERT_TRUE(l3Bytes);

    auto lapdmFrame = wrapL3(*l3Bytes, SAPI::SAPI0);
    auto unwrapped = unwrapL3(lapdmFrame);
    ASSERT_TRUE(unwrapped);

    EXPECT_TRUE(disp.dispatchRaw(*unwrapped, &state));
    EXPECT_EQ(state.msgCount, 1);
    EXPECT_TRUE(state.releaseReceived);
}
