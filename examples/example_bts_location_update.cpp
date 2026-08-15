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

// Full BTS Location Update procedure demo.
// Demonstrates the complete location updating flow: channel request with LUR cause,
// SDCCH assignment, CM Service Request, identity verification, and TMSI reallocation.
// Uses: MSContext, ChannelPool, TimerManager, TransactionManager, MMStateMachine,
//       RRStateMachine, ProtocolDispatcher.
// Reference: GSM 04.08 4.4.1 (Location Updating), 9.2.15 (Location Updating Request)

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

void printStep(const std::string& title) {
    std::cout << "\n--- " << title << " ---\n";
}

// Step 1: MS sends Channel Request with Location Updating cause.
void step1_channelRequestForLUR() {
    printStep("Step 1: Channel Request (Location Updating)");

    uint8_t ra = 0xC0;
    std::cout << "  Received RA: 0x" << std::hex << static_cast<int>(ra) << "\n";

    bool isLUR = isLocationUpdatingRequest(ra);
    std::cout << "  Is Location Updating Request: " << (isLUR ? "yes" : "no") << "\n";

    ChannelType needed = decodeChannelNeeded(ra, false, false);
    std::cout << "  Channel type needed: " << static_cast<int>(needed)
              << " (SDCCH for location update)\n";

    auto chanReq = L3ChannelRequest(ra);
    ParsedMessage crMsg{RRM{std::move(chanReq)}};

    std::cout << "  PD=" << messagePD(crMsg)
              << " MTI=0x" << std::hex << messageMTI(crMsg) << "\n";
}

// Step 2: BTS allocates SDCCH and sends ImmediateAssignment.
void step2_assignSDCCH() {
    printStep("Step 2: Allocate SDCCH and Send ImmediateAssignment");

    ChannelPool pool;
    pool.addChannel({ChannelType::SDCCHType, 0, 0, 100});
    pool.addChannel({ChannelType::SDCCHType, 0, 1, 101});
    pool.addChannel({ChannelType::TCHFType, 1, 2, 102});

    std::cout << "  Pool: " << pool.totalCount() << " channels ("
              << pool.freeCount(ChannelType::SDCCHType) << " SDCCH free)\n";

    auto ch = pool.allocate(ChannelType::SDCCHType);
    if (!ch) {
        std::cerr << "  ERROR: No SDCCH available!\n";
        std::exit(1);
    }

    std::cout << "  Allocated: trx=" << static_cast<int>(ch->trxNumber)
              << " ts=" << static_cast<int>(ch->timeslot)
              << " arfcn=" << ch->arfcn << "\n";

    auto ia = L3ImmediateAssignment::builder()
        .pageMode(L3PageMode(0))
        .dedicatedModeOrTBF(L3DedicatedModeOrTBF(false, false))
        .requestReference(L3RequestReference(1, 2, 3, 4))
        .channelDescription(L3ChannelDescription(TDMA_SDCCH, 0, ch->timeslot, ch->arfcn))
        .timingAdvance(L3TimingAdvance(24))
        .build();

    ParsedMessage pm{RRM{std::move(ia)}};
    auto l3Bytes = writeL3Bytes(pm);
    if (!l3Bytes) {
        std::cerr << "  ERROR: serialization failed\n";
        std::exit(1);
    }

    auto uiFrame = makeUIFrame(SAPI::SAPI0, false, *l3Bytes);
    auto lapdmFrame = encodeFrame(uiFrame);
    std::cout << "  ImmediateAssignment sent (" << (*l3Bytes).size()
              << " L3 bytes, " << lapdmFrame.size() << " LAPDm bytes)\n";

    auto decoded = LAPDmFrame::decode(lapdmFrame);
    auto reparsed = decoded ? parseL3((*decoded).info) : Expected<ParsedMessage>::error(
        ParseError(ParseError::Code::TruncatedInput, "decode failed"));
    if (reparsed && tryGet<L3ImmediateAssignment>(*reparsed)) {
        std::cout << "  Round-trip verified\n";
    }

    pool.release(*ch);
}

// Step 3: MS sends CMServiceRequest with location updating service type.
void step3_cmServiceRequest() {
    printStep("Step 3: CM Service Request (Location Updating)");

    RRStateMachine rrSM;
    rrSM.setState(RRStateMachine::State::CHANNEL_ASSIGNED);

    auto pagingResp = L3PagingResponse::builder()
        .mobileId(L3MobileIdentity(0x12345678))
        .build();
    ParsedMessage prMsg{RRM{std::move(pagingResp)}};
    SMResult rrResult = rrSM.processMessage(prMsg);

    std::cout << "  RR FSM: CHANNEL_ASSIGNED -> ";
    if (rrResult.nextState) {
        std::cout << "state " << *rrResult.nextState;
    } else {
        std::cout << "no transition";
    }
    std::cout << "\n";

    MMStateMachine mmSM;
    mmSM.setState(MMStateMachine::State::DEREGISTERED);

    auto cmReq = L3CMServiceRequest::builder()
        .classmark(L3MobileStationClassmark2{})
        .build();
    ParsedMessage cmMsg{MMM{std::move(cmReq)}};
    SMResult mmResult = mmSM.processMessage(cmMsg);

    std::cout << "  MM FSM: DEREGISTERED -> ";
    if (mmResult.nextState) {
        std::cout << "state " << *mmResult.nextState;
    } else {
        std::cout << "no transition";
    }
    std::cout << "\n";

    TimerManager tm;
    tm.start(L3TimerId::T3103);
    std::cout << "  Started T3103 (Location Updating timer): "
              << tm.remaining(L3TimerId::T3103).count() << " ms\n";
}

// Step 4: Identity verification if needed.
void step4_identityVerification() {
    printStep("Step 4: Identity Verification");

    auto ctx = MSContext::createWithTMSI(0x12345678);
    std::cout << "  Current identity: TMSI=0x"
              << std::hex << ctx.identity().tmsi() << "\n";

    auto idReq = L3IdentityRequest::builder()
        .type(MobileIDType::IMSI)
        .build();
    ParsedMessage idReqMsg{MMM{std::move(idReq)}};
    auto l3Bytes = writeL3Bytes(idReqMsg);
    if (l3Bytes) {
        std::cout << "  IdentityRequest (IMSI) sent (" << (*l3Bytes).size()
                  << " bytes)\n";
    }

    auto idResp = L3IdentityResponse::builder()
        .mobileId(L3MobileIdentity("244051234567890"))
        .build();
    ParsedMessage idRespMsg{MMM{std::move(idResp)}};

    ctx.setIMSI("244051234567890");
    std::cout << "  Identity verified: IMSI=" << ctx.identity().digits() << "\n";

    MMStateMachine mmSM;
    mmSM.setState(MMStateMachine::State::SERVICE_REQUEST);
    SMResult result = mmSM.processMessage(idRespMsg);
    std::cout << "  MM FSM: ";
    if (result.nextState) {
        std::cout << "-> state " << *result.nextState;
    } else {
        std::cout << "no transition";
    }
    std::cout << "\n";
}

// Step 5: Authentication.
void step5_authentication() {
    printStep("Step 5: Authentication");

    MMStateMachine mmSM;
    mmSM.setState(MMStateMachine::State::IDENTITY_VERIFIED);

    auto authReq = L3AuthenticationRequest::builder()
        .rand(std::vector<uint8_t>(32, 0x55))
        .build();
    ParsedMessage authReqMsg{MMM{std::move(authReq)}};
    auto l3Bytes = writeL3Bytes(authReqMsg);
    if (l3Bytes) {
        std::cout << "  AuthenticationRequest sent (" << (*l3Bytes).size()
                  << " bytes)\n";
    }

    TimerManager tm;
    tm.start(L3TimerId::T3106);
    std::cout << "  Started T3106: " << tm.remaining(L3TimerId::T3106).count() << " ms\n";

    auto authResp = L3AuthenticationResponse::builder()
        .sres(0xDEADBEEF)
        .build();
    ParsedMessage authRespMsg{MMM{std::move(authResp)}};
    SMResult result = mmSM.processMessage(authRespMsg);

    std::cout << "  MM FSM: ";
    if (result.nextState) {
        std::cout << "-> state " << *result.nextState;
    } else {
        std::cout << "no transition";
    }
    std::cout << "\n";

    auto ctx = MSContext::createWithTMSI(0x12345678);
    ctx.setAuthenticated(true);
    std::cout << "  MSContext: authenticated=" << ctx.isAuthenticated() << "\n";

    tm.stop(L3TimerId::T3106);
}

// Step 6: Location Updating Request from MS.
void step6_locationUpdatingRequest() {
    printStep("Step 6: Location Updating Request");

    L3LocationAreaIdentity lai("244", "05", 0x1234);

    auto lur = L3LocationUpdatingRequest::builder()
        .updateType(0)
        .cksn(3)
        .classmark(L3MobileStationClassmark1{})
        .mobileIdentity(L3MobileIdentity(0x12345678))
        .lai(lai)
        .build();

    ParsedMessage lurMsg{MMM{std::move(lur)}};
    auto l3Bytes = writeL3Bytes(lurMsg);
    if (l3Bytes) {
        std::cout << "  LocationUpdatingRequest received (" << (*l3Bytes).size()
                  << " bytes)\n";
    }

    auto uiFrame = makeUIFrame(SAPI::SAPI0, false, *l3Bytes);
    auto lapdmFrame = encodeFrame(uiFrame);
    auto decoded = LAPDmFrame::decode(lapdmFrame);
    auto reparsed = decoded ? parseL3((*decoded).info) : Expected<ParsedMessage>::error(
        ParseError(ParseError::Code::TruncatedInput, "decode failed"));

    if (reparsed) {
        auto* lur2 = tryGet<L3LocationUpdatingRequest>(*reparsed);
        if (lur2) {
            std::cout << "  Parsed LocationUpdatingRequest:\n";
            std::cout << "    Update type: " << static_cast<int>(lur2->getLocationUpdatingType()) << "\n";
            std::cout << "    Mobile ID type: " << static_cast<int>(lur2->mobileId().type()) << "\n";
        }
    }

    MMStateMachine mmSM;
    mmSM.setState(MMStateMachine::State::AUTHENTICATED);
    SMResult result = mmSM.processMessage(*reparsed);
    std::cout << "  MM FSM: ";
    if (result.nextState) {
        std::cout << "-> state " << *result.nextState;
    } else {
        std::cout << "no transition";
    }
    std::cout << "\n";

    TransactionManager txnMgr;
    auto txnId = txnMgr.create(L3PD::MobilityManagement, L3LocationUpdatingRequest::MTI, 0,
                               L3TimerId::T3103);
    if (txnId) {
        std::cout << "  Location update transaction: id=" << *txnId << "\n";
    }
}

// Step 7: BTS sends LocationUpdatingAccept with new TMSI.
void step7_locationUpdatingAccept() {
    printStep("Step 7: Location Updating Accept");

    L3LocationAreaIdentity lai("244", "05", 0x1234);
    auto accept = L3LocationUpdatingAccept::builder()
        .lai(lai)
        .build();

    ParsedMessage acceptMsg{MMM{std::move(accept)}};
    auto l3Bytes = writeL3Bytes(acceptMsg);
    if (l3Bytes) {
        std::cout << "  LocationUpdatingAccept sent (" << (*l3Bytes).size()
                  << " bytes)\n";
    }

    MMStateMachine mmSM;
    mmSM.setState(MMStateMachine::State::LOCATION_UPDATE);
    SMResult result = mmSM.processMessage(acceptMsg);
    std::cout << "  MM FSM: ";
    if (result.nextState) {
        std::cout << "-> state " << *result.nextState;
    } else {
        std::cout << "no transition";
    }
    std::cout << "\n";

    auto ctx = MSContext::createWithTMSI(0x12345678);
    ctx.setRegistered(true);
    ctx.setLAI(lai);
    std::cout << "  MSContext: registered=" << ctx.isRegistered() << "\n";
    if (ctx.lai()) {
        std::cout << "  MSContext: LAI stored\n";
    }
}

// Step 8: TMSI Reallocation.
void step8_tmsiReallocation() {
    printStep("Step 8: TMSI Reallocation");

    auto tmsiCmd = L3TMSIReallocationCommand::builder()
        .tmsi(L3MobileIdentity(0xDEADBEEF))
        .build();

    ParsedMessage cmdMsg{MMM{std::move(tmsiCmd)}};
    auto l3Bytes = writeL3Bytes(cmdMsg);
    if (l3Bytes) {
        std::cout << "  TMSIReallocationCommand sent (" << (*l3Bytes).size()
                  << " bytes)\n";
    }

    auto tmsiComp = L3TMSIReallocationComplete::builder().build();
    ParsedMessage compMsg{MMM{std::move(tmsiComp)}};
    auto l3Bytes2 = writeL3Bytes(compMsg);
    if (l3Bytes2) {
        std::cout << "  TMSIReallocationComplete received (" << (*l3Bytes2).size()
                  << " bytes)\n";
    }

    TimerManager tm;
    tm.start(L3TimerId::T3108);
    std::cout << "  Started T3108: " << tm.remaining(L3TimerId::T3108).count() << " ms\n";

    auto ctx = MSContext::createWithTMSI(0x12345678);
    std::cout << "  Old TMSI: 0x" << std::hex << ctx.identity().tmsi() << "\n";
    ctx.setTMSI(0xDEADBEEF);
    std::cout << "  New TMSI: 0x" << std::hex << ctx.identity().tmsi() << "\n";

    TransactionManager txnMgr;
    auto txnId = txnMgr.create(L3PD::MobilityManagement, L3TMSIReallocationCommand::MTI, 0,
                               L3TimerId::T3108);
    if (txnId) {
        Transaction* txn = txnMgr.get(*txnId);
        if (txn) {
            txn->complete();
            std::cout << "  TMSI reallocation transaction completed\n";
        }
    }
    txnMgr.cleanup();
}

// Step 9: Dispatcher integration.
void step9_dispatcherIntegration() {
    printStep("Step 9: ProtocolDispatcher for Location Update");

    ProtocolDispatcher dispatcher;
    int handledCount = 0;

    dispatcher.registerHandler(L3PD::MobilityManagement, L3CMServiceRequest::MTI,
        makeSharedHandler([&handledCount](const ParsedMessage&, void*) { ++handledCount; }));

    dispatcher.registerHandler(L3PD::MobilityManagement, L3IdentityResponse::MTI,
        makeSharedHandler([&handledCount](const ParsedMessage&, void*) { ++handledCount; }));

    dispatcher.registerHandler(L3PD::MobilityManagement, L3AuthenticationResponse::MTI,
        makeSharedHandler([&handledCount](const ParsedMessage&, void*) { ++handledCount; }));

    dispatcher.registerHandler(L3PD::MobilityManagement, L3LocationUpdatingRequest::MTI,
        makeSharedHandler([&handledCount](const ParsedMessage&, void*) { ++handledCount; }));

    auto cmr = L3CMServiceRequest::builder().classmark(L3MobileStationClassmark2{}).build();
    dispatcher.dispatch(ParsedMessage{MMM{std::move(cmr)}}, nullptr);

    auto idr = L3IdentityResponse::builder()
        .mobileId(L3MobileIdentity("244051234567890")).build();
    dispatcher.dispatch(ParsedMessage{MMM{std::move(idr)}}, nullptr);

    auto ar = L3AuthenticationResponse::builder().sres(0x12345678).build();
    dispatcher.dispatch(ParsedMessage{MMM{std::move(ar)}}, nullptr);

    L3LocationAreaIdentity lai("244", "05", 0x1234);
    auto lur = L3LocationUpdatingRequest::builder()
        .updateType(0).cksn(3).classmark(L3MobileStationClassmark1{})
        .mobileIdentity(L3MobileIdentity(0x12345678)).lai(lai).build();
    dispatcher.dispatch(ParsedMessage{MMM{std::move(lur)}}, nullptr);

    std::cout << "  Messages handled: " << handledCount << "/4\n";
}

} // anonymous namespace

int main() {
    std::cout << "BTS Location Update Full Procedure Demo\n";
    std::cout << "Demonstrates complete location updating flow using stack components.\n";

    step1_channelRequestForLUR();
    step2_assignSDCCH();
    step3_cmServiceRequest();
    step4_identityVerification();
    step5_authentication();
    step6_locationUpdatingRequest();
    step7_locationUpdatingAccept();
    step8_tmsiReallocation();
    step9_dispatcherIntegration();

    std::cout << "\nAll location update procedure steps completed successfully.\n";
    return 0;
}
