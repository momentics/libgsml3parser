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

// Full BTS Paging -> Call Setup procedure demo.
// Demonstrates the complete inbound call flow: paging MS, receiving response,
// channel assignment, MM authentication flow, and CC call setup.
// Uses: MSContext, TimerManager, TransactionManager, MMStateMachine,
//       CCStateMachine, ChannelPool, ProtocolDispatcher.
// Reference: GSM 04.08 Chapters 4 (MM), 6 (CC)

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

void printPhase(const std::string& title) {
    std::cout << "\n--- " << title << " ---\n";
}

// Phase 1: BTS sends PagingRequestType2 to page the MS.
void phase1_sendPaging() {
    printPhase("Phase 1: BTS sends PagingRequestType2");

    auto paging = L3PagingRequestType2::builder()
        .addTMSI(0x12345678, ChannelType::SDCCHType)
        .build();

    ParsedMessage pm{RRM{std::move(paging)}};
    auto l3Bytes = writeL3Bytes(pm);
    if (!l3Bytes) {
        std::cerr << "  ERROR: writeL3Bytes failed\n";
        std::exit(1);
    }

    auto lapdmFrame = wrapL3(*l3Bytes, SAPI::SAPI0, false);
    std::cout << "  PagingRequestType2 sent for TMSI=0x12345678\n";
    std::cout << "  L3 bytes (" << (*l3Bytes).size() << "): "
              << bytesToHex(*l3Bytes) << "\n";
    std::cout << "  LAPDm frame (" << lapdmFrame.size() << " bytes)\n";

    TimerManager tm;
    tm.start(L3TimerId::T3109);
    std::cout << "  Started T3109 (Paging Response timer): "
              << tm.remaining(L3TimerId::T3109).count() << " ms\n";
}

// Phase 2: MS responds with PagingResponse.
void phase2_receivePagingResponse() {
    printPhase("Phase 2: MS sends PagingResponse");

    auto pagingResp = L3PagingResponse::builder()
        .mobileId(L3MobileIdentity(0x12345678))
        .cksn(3)
        .build();

    ParsedMessage pm{RRM{std::move(pagingResp)}};
    auto l3Bytes = writeL3Bytes(pm);
    if (!l3Bytes) {
        std::cerr << "  ERROR: writeL3Bytes failed\n";
        std::exit(1);
    }

    auto lapdmFrame = wrapL3(*l3Bytes, SAPI::SAPI0);
    auto unwrapped = unwrapL3(lapdmFrame);
    auto reparsed = parseL3(*unwrapped);

    if (!reparsed) {
        std::cerr << "  ERROR: round-trip failed\n";
        std::exit(1);
    }

    auto* pr = tryGet<L3PagingResponse>(*reparsed);
    if (pr) {
        std::cout << "  PagingResponse received:\n";
        std::cout << "    Mobile ID type: " << static_cast<int>(pr->mobileId().type()) << "\n";
        std::cout << "    CKSN: " << pr->cksn() << "\n";
    }

    RRStateMachine rrSM;
    rrSM.setState(RRStateMachine::State::CHANNEL_ASSIGNED);
    SMResult result = rrSM.processMessage(*reparsed);
    std::cout << "  RR FSM: transition=" << result.causesTransition();
    if (result.nextState) {
        std::cout << " -> state " << *result.nextState;
    }
    std::cout << "\n";

    TimerManager tm;
    tm.start(L3TimerId::T3109);
    tm.stop(L3TimerId::T3109);
    std::cout << "  T3109 stopped (response received)\n";
}

// Phase 3: BTS allocates SDCCH and sends ImmediateAssignment.
void phase3_channelAssignment() {
    printPhase("Phase 3: Channel Assignment");

    ChannelPool pool;
    pool.addChannel({ChannelType::SDCCHType, 0, 0, 100});
    pool.addChannel({ChannelType::SDCCHType, 0, 1, 101});
    pool.addChannel({ChannelType::TCHFType, 1, 2, 102});

    auto ch = pool.allocate(ChannelType::SDCCHType);
    if (!ch) {
        std::cerr << "  ERROR: No SDCCH available\n";
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

    ParsedMessage pm{RRM{std::move(ia)}};
    auto l3Bytes = writeL3Bytes(pm);
    if (!l3Bytes) {
        std::cerr << "  ERROR: serialization failed\n";
        std::exit(1);
    }

    std::cout << "  ImmediateAssignment sent (" << (*l3Bytes).size() << " L3 bytes)\n";

    auto ctx = MSContext::createWithTMSI(0x12345678);
    ctx.assignChannel(ch->type, ch->trxNumber, ch->timeslot, ch->arfcn);
    ctx.setTimingAdvance(32);
    std::cout << "  MSContext updated: channel="
              << static_cast<int>(ctx.channelType())
              << " ta=" << *ctx.timingAdvance() << "\n";

    pool.release(*ch);
}

// Phase 4: MS sends CMServiceRequest; MM state machine processes.
void phase4_cmServiceRequest() {
    printPhase("Phase 4: CM Service Request and MM Flow");

    MMStateMachine mmSM;
    mmSM.setState(MMStateMachine::State::DEREGISTERED);
    std::cout << "  MM state: DEREGISTERED\n";

    auto cmReq = L3CMServiceRequest::builder()
        .classmark(L3MobileStationClassmark2{})
        .build();

    ParsedMessage cmMsg{MMM{std::move(cmReq)}};
    SMResult result = mmSM.processMessage(cmMsg);

    std::cout << "  Processed CMServiceRequest:\n";
    std::cout << "    action=" << static_cast<int>(result.action) << "\n";
    if (result.nextState) {
        std::cout << "    nextState=" << *result.nextState
                  << " (SERVICE_REQUEST)\n";
    }
    std::cout << "  MM state after: " << mmSM.state() << "\n";

    TimerManager tm;
    tm.start(L3TimerId::T3101);
    std::cout << "  Started T3101: " << tm.remaining(L3TimerId::T3101).count() << " ms\n";
}

// Phase 5: Identity Request / Response.
void phase5_identityExchange() {
    printPhase("Phase 5: Identity Request / Response");

    MMStateMachine mmSM;
    mmSM.setState(MMStateMachine::State::SERVICE_REQUEST);
    std::cout << "  MM state: SERVICE_REQUEST\n";

    auto idReq = L3IdentityRequest::builder()
        .type(MobileIDType::IMSI)
        .build();

    ParsedMessage idReqMsg{MMM{std::move(idReq)}};
    auto l3Bytes = writeL3Bytes(idReqMsg);
    if (l3Bytes) {
        std::cout << "  IdentityRequest sent (" << (*l3Bytes).size() << " bytes)\n";
    }

    auto idResp = L3IdentityResponse::builder()
        .mobileId(L3MobileIdentity("244051234567890"))
        .build();

    ParsedMessage idRespMsg{MMM{std::move(idResp)}};
    SMResult result = mmSM.processMessage(idRespMsg);

    std::cout << "  Processed IdentityResponse:\n";
    if (result.nextState) {
        std::cout << "    nextState=" << *result.nextState
                  << " (IDENTITY_VERIFIED)\n";
    }
    std::cout << "  MM state after: " << mmSM.state() << "\n";

    auto ctx = MSContext::createWithTMSI(0x12345678);
    ctx.setIMSI("244051234567890");
    std::cout << "  MSContext identity updated to IMSI\n";
}

// Phase 6: Authentication flow.
void phase6_authentication() {
    printPhase("Phase 6: Authentication Request / Response");

    MMStateMachine mmSM;
    mmSM.setState(MMStateMachine::State::IDENTITY_VERIFIED);
    std::cout << "  MM state: IDENTITY_VERIFIED\n";

    auto authReq = L3AuthenticationRequest::builder()
        .rand(std::vector<uint8_t>{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                                    0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
                                    0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
                                    0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20})
        .build();

    ParsedMessage authReqMsg{MMM{std::move(authReq)}};
    auto l3Bytes = writeL3Bytes(authReqMsg);
    if (l3Bytes) {
        std::cout << "  AuthenticationRequest sent (" << (*l3Bytes).size() << " bytes)\n";
    }

    TimerManager tm;
    tm.start(L3TimerId::T3106);
    std::cout << "  Started T3106: " << tm.remaining(L3TimerId::T3106).count() << " ms\n";

    auto authResp = L3AuthenticationResponse::builder()
        .sres(0xDEADBEEF)
        .build();

    ParsedMessage authRespMsg{MMM{std::move(authResp)}};
    SMResult result = mmSM.processMessage(authRespMsg);

    std::cout << "  Processed AuthenticationResponse:\n";
    if (result.nextState) {
        std::cout << "    nextState=" << *result.nextState
                  << " (AUTHENTICATED)\n";
    }
    std::cout << "  MM state after: " << mmSM.state() << "\n";

    auto ctx = MSContext::createWithTMSI(0x12345678);
    ctx.setAuthenticated(true);
    std::cout << "  MSContext: isAuthenticated=" << ctx.isAuthenticated() << "\n";

    tm.stop(L3TimerId::T3106);
}

// Phase 7: CC call setup.
void phase7_callSetup() {
    printPhase("Phase 7: Call Control Setup");

    MMStateMachine mmSM;
    mmSM.setState(MMStateMachine::State::REGISTERED);
    std::cout << "  MM state: REGISTERED\n";

    CCStateMachine ccSM;
    ccSM.setState(CCStateMachine::State::IDLE);
    std::cout << "  CC state: IDLE\n";

    L3CalledPartyBCDNumber calledNum("1234567890");
    auto setup = L3Setup::builder()
        .calledParty(calledNum)
        .ti(1)
        .build();

    ParsedMessage setupMsg{CCM{std::move(setup)}};
    SMResult result = ccSM.processMessage(setupMsg);

    std::cout << "  Processed Setup:\n";
    if (result.nextState) {
        std::cout << "    nextState=" << *result.nextState
                  << " (SETUP_RECEIVED)\n";
    }
    std::cout << "  CC state after: " << ccSM.state() << "\n";

    TransactionManager txnMgr;
    auto txnId = txnMgr.create(L3PD::CallControl, L3Setup::MTI, 1, L3TimerId::T3101);
    if (txnId) {
        std::cout << "  Transaction created: id=" << *txnId << "\n";
    }

    std::cout << "  Pending transactions: " << txnMgr.pendingCount() << "\n";

    auto proceeding = L3CallProceeding::builder()
        .ti(1)
        .build();

    ParsedMessage procMsg{CCM{std::move(proceeding)}};
    auto l3Bytes = writeL3Bytes(procMsg);
    if (l3Bytes) {
        std::cout << "  CallProceeding sent (" << (*l3Bytes).size() << " bytes)\n";
    }

    if (txnId) {
        Transaction* txn = txnMgr.get(*txnId);
        if (txn) {
            txn->complete();
            std::cout << "  Transaction completed\n";
        }
    }

    size_t cleaned = txnMgr.cleanup();
    std::cout << "  Cleaned up " << cleaned << " finished transaction(s)\n";
}

// Phase 8: Dispatcher integration for full flow.
void phase8_dispatcherFlow() {
    printPhase("Phase 8: ProtocolDispatcher Full Flow");

    ProtocolDispatcher dispatcher;

    struct Stats {
        int pagingResponses = 0;
        int cmServiceRequests = 0;
        int authResponses = 0;
        int setups = 0;
    } stats;

    dispatcher.registerHandler(L3PD::RadioResource, L3PagingResponse::MTI,
        [&stats](const ParsedMessage&, void*) { ++stats.pagingResponses; });

    dispatcher.registerHandler(L3PD::MobilityManagement, L3CMServiceRequest::MTI,
        [&stats](const ParsedMessage&, void*) { ++stats.cmServiceRequests; });

    dispatcher.registerHandler(L3PD::MobilityManagement, L3AuthenticationResponse::MTI,
        [&stats](const ParsedMessage&, void*) { ++stats.authResponses; });

    dispatcher.registerHandler(L3PD::CallControl, L3Setup::MTI,
        [&stats](const ParsedMessage&, void*) { ++stats.setups; });

    auto pr = L3PagingResponse::builder().mobileId(L3MobileIdentity(0x12345678)).build();
    dispatcher.dispatch(ParsedMessage{RRM{std::move(pr)}}, nullptr);

    auto cmr = L3CMServiceRequest::builder().classmark(L3MobileStationClassmark2{}).build();
    dispatcher.dispatch(ParsedMessage{MMM{std::move(cmr)}}, nullptr);

    auto ar = L3AuthenticationResponse::builder().sres(0x12345678).build();
    dispatcher.dispatch(ParsedMessage{MMM{std::move(ar)}}, nullptr);

    L3CalledPartyBCDNumber calledNum("9876543210");
    auto setup = L3Setup::builder().calledParty(calledNum).ti(2).build();
    dispatcher.dispatch(ParsedMessage{CCM{std::move(setup)}}, nullptr);

    std::cout << "  Dispatcher stats:\n";
    std::cout << "    PagingResponse: " << stats.pagingResponses << "\n";
    std::cout << "    CMServiceRequest: " << stats.cmServiceRequests << "\n";
    std::cout << "    AuthenticationResponse: " << stats.authResponses << "\n";
    std::cout << "    Setup: " << stats.setups << "\n";
}

} // anonymous namespace

int main() {
    std::cout << "BTS Paging -> Call Setup Full Procedure Demo\n";
    std::cout << "Demonstrates complete inbound call flow using stack components.\n";

    phase1_sendPaging();
    phase2_receivePagingResponse();
    phase3_channelAssignment();
    phase4_cmServiceRequest();
    phase5_identityExchange();
    phase6_authentication();
    phase7_callSetup();
    phase8_dispatcherFlow();

    std::cout << "\nAll paging -> call setup phases completed successfully.\n";
    return 0;
}
