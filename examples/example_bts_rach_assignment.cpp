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

// Full BTS RACH -> Channel Assignment procedure demo.
// Demonstrates the complete flow from receiving a Channel Request on RACH
// through allocating a channel, building ImmediateAssignment, serializing,
// and processing the MS response with stack components.
// Uses: MSContext, ChannelPool, ProtocolDispatcher, RRStateMachine, TimerManager.
// Reference: GSM 04.08 9.1.13 (Channel Request), 9.1.19 (Immediate Assignment)

#include <gsml3parser/gsml3parser.hpp>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <span>
#include <string>

using namespace gsml3parser;
using namespace gsml3parser::lapdm;

namespace {

std::string bytesToHex(std::span<const uint8_t> data) {
    std::ostringstream oss;
    oss << std::hex << std::uppercase << std::setfill('0');
    for (size_t i = 0; i < data.size(); ++i) {
        if (i > 0) oss << ' ';
        oss << std::setw(2) << static_cast<int>(data[i]);
    }
    return oss.str();
}

void printSeparator(const std::string& title) {
    std::cout << "\n=== " << title << " ===\n";
}

// Step 1: Simulate receiving Channel Request on RACH and decode RA.
void step1_receiveChannelRequest() {
    printSeparator("Step 1: Receive Channel Request on RACH");

    uint8_t ra = 0x03;
    std::cout << "  Raw RA byte: 0x" << std::hex << static_cast<int>(ra) << "\n";

    ChannelType neededWithoutVEA = decodeChannelNeeded(ra, false, false);
    ChannelType neededWithVEA = decodeChannelNeeded(ra, false, true);

    std::cout << "  Without VEA: need " << static_cast<int>(neededWithoutVEA)
              << " (SDCCH)\n";
    std::cout << "  With VEA:    need " << static_cast<int>(neededWithVEA)
              << " (TCHF)\n";

    bool isLUR = isLocationUpdatingRequest(ra);
    std::cout << "  Is Location Updating: " << (isLUR ? "yes" : "no") << "\n";
}

// Step 2: Allocate channel from pool and build ImmediateAssignment.
void step2_allocateChannelAndBuildIA() {
    printSeparator("Step 2: Allocate Channel and Build ImmediateAssignment");

    ChannelPool pool;
    pool.addChannel({ChannelType::SDCCHType, 0, 0, 100});
    pool.addChannel({ChannelType::SDCCHType, 0, 1, 101});
    pool.addChannel({ChannelType::TCHFType, 1, 2, 102});
    pool.addChannel({ChannelType::TCHHType, 1, 3, 103});

    std::cout << "  Pool initialized: " << pool.totalCount() << " channels\n";
    std::cout << "  Free SDCCH: " << pool.freeCount(ChannelType::SDCCHType) << "\n";
    std::cout << "  Free TCHF:  " << pool.freeCount(ChannelType::TCHFType) << "\n";

    auto ch = pool.allocate(ChannelType::SDCCHType);
    if (!ch) {
        std::cerr << "  ERROR: No SDCCH available!\n";
        std::exit(1);
    }
    std::cout << "  Allocated SDCCH: trx=" << static_cast<int>(ch->trxNumber)
              << " ts=" << static_cast<int>(ch->timeslot)
              << " arfcn=" << ch->arfcn << "\n";

    auto ia = L3ImmediateAssignment::builder()
        .pageMode(L3PageMode(0))
        .dedicatedModeOrTBF(L3DedicatedModeOrTBF(false, false))
        .requestReference(L3RequestReference(1, 2, 3, 4))
        .channelDescription(L3ChannelDescription(TDMA_SDCCH, 0, ch->timeslot, ch->arfcn))
        .timingAdvance(L3TimingAdvance(32))
        .build();

    std::cout << "  Built ImmediateAssignment:\n";
    std::cout << "    typeAndOffset=" << static_cast<int>(ia.channelDescription().typeAndOffset()) << "\n";
    std::cout << "    timingAdvance=" << ia.timingAdvance().timingAdvance() << "\n";

    ParsedMessage pm{RRM{std::move(ia)}};
    auto l3Bytes = writeL3Bytes(pm);
    if (!l3Bytes) {
        std::cerr << "  ERROR: writeL3Bytes failed\n";
        std::exit(1);
    }

    std::cout << "  L3 bytes (" << (*l3Bytes).size() << "): "
              << bytesToHex(*l3Bytes) << "\n";

    auto lapdmFrame = wrapL3(*l3Bytes, SAPI::SAPI0, false);
    std::cout << "  LAPDm frame (" << lapdmFrame.size() << "): "
              << bytesToHex(lapdmFrame) << "\n";

    pool.release(*ch);
    std::cout << "  Channel released. Free SDCCH: "
              << pool.freeCount(ChannelType::SDCCHType) << "\n";
}

// Step 3: Create MSContext and assign channel.
void step3_createMSContext() {
    printSeparator("Step 3: Create MSContext and Assign Channel");

    auto ctx = MSContext::createWithTMSI(0x12345678);

    std::cout << "  Created MSContext with TMSI=0x"
              << std::hex << ctx.identity().tmsi() << "\n";
    std::cout << "  sizeof(MSContext) = " << sizeof(MSContext) << " bytes\n";

    ctx.assignChannel(ChannelType::SDCCHType, 0, 1, 100);
    std::cout << "  Assigned channel: type=" << static_cast<int>(ctx.channelType())
              << " trx=" << static_cast<int>(ctx.trxNumber())
              << " ts=" << static_cast<int>(ctx.timeslot())
              << " arfcn=" << ctx.arfcn() << "\n";

    ctx.setTimingAdvance(48);
    std::cout << "  Timing advance: " << *ctx.timingAdvance() << "\n";

    ctx.setTMSI(0xDEADBEEF);
    std::cout << "  Updated TMSI to 0x" << std::hex << ctx.identity().tmsi() << "\n";

    ctx.releaseChannel();
    std::cout << "  Channel released. Type now: "
              << static_cast<int>(ctx.channelType()) << "\n";
}

// Step 4: Process MS response through RR state machine.
void step4_processMSResponseWithFSM() {
    printSeparator("Step 4: Process MS Response via RRStateMachine");

    RRStateMachine rrSM;
    rrSM.setState(RRStateMachine::State::CHANNEL_ASSIGNED);
    std::cout << "  RR state: CHANNEL_ASSIGNED\n";

    auto pagingResp = L3PagingResponse::builder()
        .mobileId(L3MobileIdentity(0x12345678))
        .build();

    ParsedMessage respMsg{RRM{std::move(pagingResp)}};
    SMResult result = rrSM.processMessage(respMsg);

    std::cout << "  Processed PagingResponse:\n";
    std::cout << "    action=" << static_cast<int>(result.action) << "\n";
    std::cout << "    causesTransition=" << result.causesTransition() << "\n";
    if (result.nextState) {
        std::cout << "    nextState=" << *result.nextState << " (WAITING_MM)\n";
    }
    std::cout << "  RR state after: " << rrSM.state() << "\n";

    rrSM.setState(RRStateMachine::State::ACTIVE);
    auto cipherComplete = L3CipheringModeComplete::builder().build();
    ParsedMessage cipherMsg{RRM{std::move(cipherComplete)}};

    SMResult cipherResult = rrSM.processMessage(cipherMsg);
    std::cout << "  Processed CipheringModeComplete in ACTIVE:\n";
    std::cout << "    action=" << static_cast<int>(cipherResult.action)
              << " (stay in ACTIVE)\n";
}

// Step 5: Use TimerManager for protocol timers.
void step5_timerManagement() {
    printSeparator("Step 5: Timer Management with TimerManager");

    TimerManager tm;

    tm.start(L3TimerId::T3109);
    std::cout << "  Started T3109, remaining: "
              << tm.remaining(L3TimerId::T3109).count() << " ms\n";

    tm.start(L3TimerId::T3101);
    std::cout << "  Started T3101, remaining: "
              << tm.remaining(L3TimerId::T3101).count() << " ms\n";

    std::cout << "  Running timers: " << tm.runningCount() << "\n";

    std::cout << "  Ticking 2000ms with callback:\n";
    tm.tick(std::chrono::milliseconds(2000), [](L3TimerId id) {
        std::cout << "    Timer expired: " << l3TimerName(id) << "\n";
    });

    std::cout << "  Remaining T3109: " << tm.remaining(L3TimerId::T3109).count() << " ms\n";
    std::cout << "  Remaining T3101: " << tm.remaining(L3TimerId::T3101).count() << " ms\n";

    std::array<L3TimerId, 32> expired;
    size_t n = tm.tick(std::chrono::milliseconds(5000), std::span<L3TimerId>(expired));
    std::cout << "  Ticked 5000ms with span: " << n << " expired\n";
    for (size_t i = 0; i < n; ++i) {
        std::cout << "    Expired: " << l3TimerName(expired[i]) << "\n";
    }

    std::cout << "  Running timers after expiry: " << tm.runningCount() << "\n";
}

// Step 6: Use ProtocolDispatcher with stack components.
void step6_dispatcherIntegration() {
    printSeparator("Step 6: ProtocolDispatcher Integration");

    ProtocolDispatcher dispatcher;
    bool channelRequestReceived = false;
    bool pagingResponseReceived = false;

    dispatcher.registerHandler(L3PD::RadioResource, L3ChannelRequest::MTI,
        [&channelRequestReceived](const ParsedMessage& msg, void*) {
            if (tryGet<L3ChannelRequest>(msg)) {
                channelRequestReceived = true;
            }
        });

    dispatcher.registerHandler(L3PD::RadioResource, L3PagingResponse::MTI,
        [&pagingResponseReceived](const ParsedMessage& msg, void*) {
            if (tryGet<L3PagingResponse>(msg)) {
                pagingResponseReceived = true;
            }
        });

    auto chanReq = L3ChannelRequest(0x05);
    ParsedMessage crMsg{RRM{std::move(chanReq)}};
    dispatcher.dispatch(crMsg, nullptr);

    std::cout << "  Channel Request dispatched: "
              << (channelRequestReceived ? "handled" : "missed") << "\n";

    auto pagingResp = L3PagingResponse::builder()
        .mobileId(L3MobileIdentity(0x98765432))
        .build();
    ParsedMessage prMsg{RRM{std::move(pagingResp)}};
    dispatcher.dispatch(prMsg, nullptr);

    std::cout << "  Paging Response dispatched: "
              << (pagingResponseReceived ? "handled" : "missed") << "\n";
}

// Step 7: Full round-trip verification.
void step7_roundtripVerification() {
    printSeparator("Step 7: Full Round-Trip Verification");

    auto ia = L3ImmediateAssignment::builder()
        .channelDescription(L3ChannelDescription(TDMA_SDCCH, 0, 2, 150))
        .timingAdvance(L3TimingAdvance(64))
        .build();

    ParsedMessage pm{RRM{std::move(ia)}};
    auto l3Bytes = writeL3Bytes(pm);
    if (!l3Bytes) {
        std::cerr << "  ERROR: writeL3Bytes failed\n";
        std::exit(1);
    }

    auto lapdmFrame = wrapL3(*l3Bytes, SAPI::SAPI0);
    auto unwrapped = unwrapL3(lapdmFrame);
    if (!unwrapped) {
        std::cerr << "  ERROR: unwrapL3 failed\n";
        std::exit(1);
    }

    auto reparsed = parseL3(*unwrapped);
    if (!reparsed) {
        std::cerr << "  ERROR: parseL3 failed\n";
        std::exit(1);
    }

    auto* ia2 = tryGet<L3ImmediateAssignment>(*reparsed);
    if (ia2) {
        std::cout << "  Round-trip verified:\n";
        std::cout << "    PD=" << messagePD(*reparsed)
                  << " MTI=0x" << std::hex << messageMTI(*reparsed) << "\n";
        std::cout << "    timingAdvance="
                  << ia2->timingAdvance().timingAdvance() << "\n";
    } else {
        std::cerr << "  ERROR: could not extract L3ImmediateAssignment\n";
        std::exit(1);
    }

    std::cout << "  LAPDm address byte: 0x" << std::hex
              << static_cast<int>(lapdmFrame[0]) << "\n";
    std::cout << "  LAPDm control byte: 0x" << std::hex
              << static_cast<int>(lapdmFrame[1]) << "\n";
}

} // anonymous namespace

int main() {
    std::cout << "BTS RACH -> Channel Assignment Full Procedure Demo\n";
    std::cout << "Demonstrates complete procedure using stack components.\n";

    step1_receiveChannelRequest();
    step2_allocateChannelAndBuildIA();
    step3_createMSContext();
    step4_processMSResponseWithFSM();
    step5_timerManagement();
    step6_dispatcherIntegration();
    step7_roundtripVerification();

    std::cout << "\nAll RACH assignment procedure steps completed successfully.\n";
    return 0;
}
