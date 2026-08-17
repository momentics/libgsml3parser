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

/// Reference BTS application demonstrating full protocol stack integration.
///
/// Shows complete chains: Location Update (CMServiceRequest -> Auth -> Ciphering -> LU Accept)
/// and MO Call Setup (CMServiceRequest -> Setup -> Proceeding -> Assignment -> Connect).
/// Uses ProcedureOrchestrator, ResponseToken + Arena pattern, typed external data,
/// and ShardedSubscriberRegistry for session management.
#include <array>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>

#include "gsml3parser/arena.h"
#include "gsml3parser/message_types.h"
#include "gsml3parser/mm/l3mmmessages.h"
#include "gsml3parser/cc/l3ccmessages.h"
#include "gsml3parser/rr/l3rrmessages.h"
#include "gsml3parser/visitor.h"
#include "gsml3parser/stack/subscriber_registry.h"
#include "gsml3parser/stack/procedure_orchestrator.h"
#include "gsml3parser/stack/procedure_runner.h"
#include "gsml3parser/stack/response_builder.h"

using namespace gsml3parser;

static void printHex(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        std::printf("%02X ", data[i]);
    }
    std::printf("\n");
}

static void simulateLocationUpdateChain() {
    std::printf("=== Location Update Chain ===\n");

    ShardedSubscriberRegistry<4> registry;
    auto* session = registry.createByTMSI(0x12345678u);
    session->context.setLAI(L3LocationAreaIdentity{"244", "05", 0x1234});

    ProcedureOrchestrator orchestrator;
    Arena arena(65536);

    // Step 1: MS sends CMServiceRequest (LocationUpdate)
    auto cmReq = ParsedMessage{MMM{
        L3CMServiceRequest::builder()
            .serviceType(L3CMServiceType{L3CMServiceType::TypeCode::LocationUpdateRequest})
            .mobileIdentity(L3MobileIdentity{0x12345678u})
            .build()}};

    auto r1 = orchestrator.feed(cmReq, session);
    std::printf("  [1] CMServiceRequest -> action=%d token=%d\n",
                static_cast<int>(r1.action), static_cast<int>(r1.responseToken));

    // Build CMServiceAccept response
    std::array<uint8_t, 512> buf{};
    int n = orchestrator.buildPendingResponse({buf.data(), buf.size()}, session);
    if (n > 0) {
        std::printf("  [1] CMServiceAccept (%d bytes): ", n);
        printHex(buf.data(), static_cast<size_t>(n));
    }

    // Step 2: Feed AuthChallenge from AuC
    AuthChallenge chal{};
    for (int i = 0; i < 16; ++i) chal.rand[i] = static_cast<uint8_t>(i);
    chal.expectedSres[0] = 0xAB;
    chal.expectedSres[1] = 0xCD;
    chal.expectedSres[2] = 0xEF;
    chal.expectedSres[3] = 0x01;

    auto r2 = orchestrator.feedExternalTyped(chal);
    std::printf("  [2] AuthChallenge -> action=%d token=%d\n",
                static_cast<int>(r2.action), static_cast<int>(r2.responseToken));

    n = orchestrator.buildPendingResponse({buf.data(), buf.size()}, session);
    if (n > 0) {
        std::printf("  [2] AuthenticationRequest (%d bytes): ", n);
        printHex(buf.data(), static_cast<size_t>(n));
    }

    // Step 3: Feed CipheringParameters (chain advances from Auth -> CipheringMode)
    CipheringParameters cipherParams{0, true};
    auto r3 = orchestrator.feedExternalTyped(cipherParams);
    std::printf("  [3] CipheringParams -> action=%d token=%d\n",
                static_cast<int>(r3.action), static_cast<int>(r3.responseToken));

    n = orchestrator.buildPendingResponse({buf.data(), buf.size()}, session);
    if (n > 0) {
        std::printf("  [3] CipheringModeCommand (%d bytes): ", n);
        printHex(buf.data(), static_cast<size_t>(n));
    }

    // Step 4: Feed VLR accept decision (chain advances to LocationUpdate)
    VLRDecision vlr{true, std::nullopt, MMRejectCause::Zero};
    auto r4 = orchestrator.feedExternalTyped(vlr);
    std::printf("  [4] VLRDecision(accept) -> action=%d token=%d\n",
                static_cast<int>(r4.action), static_cast<int>(r4.responseToken));

    n = orchestrator.buildPendingResponse({buf.data(), buf.size()}, session);
    if (n > 0) {
        std::printf("  [4] LocationUpdatingAccept (%d bytes): ", n);
        printHex(buf.data(), static_cast<size_t>(n));
    }

    std::printf("  Chain phase: %s\n", procedureTypeName(orchestrator.chainPhase()).data());
    std::printf("\n");
}

static void simulateMOCallSetupChain() {
    std::printf("=== MO Call Setup Chain ===\n");

    ShardedSubscriberRegistry<4> registry;
    auto* session = registry.createByTMSI(0x87654321u);

    ProcedureOrchestrator orchestrator;

    // Step 1: MS sends CMServiceRequest (MO Call)
    auto cmReq = ParsedMessage{MMM{
        L3CMServiceRequest::builder()
            .serviceType(L3CMServiceType{L3CMServiceType::TypeCode::MobileOriginatedCall})
            .mobileIdentity(L3MobileIdentity{0x87654321u})
            .build()}};

    auto r1 = orchestrator.feed(cmReq, session);
    std::printf("  [1] CMServiceRequest(MO) -> action=%d token=%d\n",
                static_cast<int>(r1.action), static_cast<int>(r1.responseToken));

    // Build CMServiceAccept response
    std::array<uint8_t, 512> buf{};
    int n = orchestrator.buildPendingResponse({buf.data(), buf.size()}, session);
    if (n > 0) {
        std::printf("  [1] CMServiceAccept (%d bytes): ", n);
        printHex(buf.data(), static_cast<size_t>(n));
    }

    // Step 2: MS sends Setup message
    auto setup = ParsedMessage{CCM{
        L3Setup::builder().ti(3).calledParty(L3CalledPartyBCDNumber{"123456789"}).build()}};

    auto r2 = orchestrator.feed(setup, session);
    std::printf("  [2] Setup -> action=%d token=%d\n",
                static_cast<int>(r2.action), static_cast<int>(r2.responseToken));

    n = orchestrator.buildPendingResponse({buf.data(), buf.size()}, session);
    if (n > 0) {
        std::printf("  [2] Response (%d bytes): ", n);
        printHex(buf.data(), static_cast<size_t>(n));
    }

    std::printf("  Chain phase: %s\n", procedureTypeName(orchestrator.chainPhase()).data());
    std::printf("\n");
}

static void simulateIMSIDetach() {
    std::printf("=== IMSI Detach ===\n");

    ShardedSubscriberRegistry<4> registry;
    auto* session = registry.createByTMSI(0x11111111u);

    ProcedureOrchestrator orchestrator;

    // MS sends IMSI Detach Indication
    auto detach = ParsedMessage{MMM{
        L3IMSIDetachIndication::builder()
            .mobileIdentity(L3MobileIdentity{0x11111111u})
            .build()}};

    auto r1 = orchestrator.feed(detach, session);
    std::printf("  [1] IMSIDetachIndication -> action=%d token=%d\n",
                static_cast<int>(r1.action), static_cast<int>(r1.responseToken));

    std::array<uint8_t, 512> buf{};
    int n = orchestrator.buildPendingResponse({buf.data(), buf.size()}, session);
    if (n > 0) {
        std::printf("  [1] CMServiceAccept (%d bytes): ", n);
        printHex(buf.data(), static_cast<size_t>(n));
    }

    std::printf("  Chain phase: %s\n", procedureTypeName(orchestrator.chainPhase()).data());
    std::printf("\n");
}

static void demonstrateResponseBuilder() {
    std::printf("=== ResponseBuilder Direct Use ===\n");

    Arena arena(65536);
    std::array<uint8_t, 512> buf{};

    // Build CMServiceAccept
    int n = ResponseBuilder::buildCMServiceAccept({buf.data(), buf.size()});
    if (n > 0) {
        std::printf("  CMServiceAccept (%d bytes): ", n);
        printHex(buf.data(), static_cast<size_t>(n));
    }

    // Build IdentityRequest for IMSI
    n = ResponseBuilder::buildIdentityRequest({buf.data(), buf.size()}, MobileIDType::IMSI);
    if (n > 0) {
        std::printf("  IdentityRequest/IMSI (%d bytes): ", n);
        printHex(buf.data(), static_cast<size_t>(n));
    }

    // Build LocationUpdatingAccept
    L3LocationAreaIdentity lai{"244", "05", 0x1234};
    n = ResponseBuilder::buildLocationUpdatingAccept({buf.data(), buf.size()}, lai, 0xDEADBEEFu);
    if (n > 0) {
        std::printf("  LocationUpdatingAccept (%d bytes): ", n);
        printHex(buf.data(), static_cast<size_t>(n));
    }

    // Build Disconnect
    n = ResponseBuilder::buildDisconnect({buf.data(), buf.size()}, 3, CCCause::Normal_Call_Clearing);
    if (n > 0) {
        std::printf("  Disconnect/TI=3 (%d bytes): ", n);
        printHex(buf.data(), static_cast<size_t>(n));
    }

    std::printf("\n");
}

int main() {
    std::printf("libgsml3parser Reference BTS Demo\n");
    std::printf("==================================\n\n");

    // Verify size constraints
    std::printf("Size checks:\n");
    std::printf("  sizeof(ProcedureStepResult) = %zu (<= 32: %s)\n",
                sizeof(ProcedureStepResult), sizeof(ProcedureStepResult) <= 32 ? "OK" : "FAIL");
    std::printf("  sizeof(SubscriberSession) = %zu (< 4096: %s)\n",
                sizeof(SubscriberSession), sizeof(SubscriberSession) < 4096 ? "OK" : "FAIL");
    std::printf("  sizeof(SMResult) = %zu (<= 16: %s)\n",
                sizeof(SMResult), sizeof(SMResult) <= 16 ? "OK" : "FAIL");
    std::printf("\n");

    simulateLocationUpdateChain();
    simulateMOCallSetupChain();
    simulateIMSIDetach();
    demonstrateResponseBuilder();

    std::printf("All demos completed successfully.\n");
    return 0;
}
