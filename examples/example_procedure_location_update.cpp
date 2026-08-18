// Copyright 2026 momentics <momentics@gmail.com>
// Copyright libgsml3parser contributors
// MIT License - see header for full text.

// Demonstrates a full Location Update procedure via the ProcedureRunner API.
// Shows: session creation, message feeding, external data (auth/VLR), and completion.

#include <gsml3parser/gsml3parser.hpp>
#include <gsml3parser/stack/typed_external_data.h>

#include <iostream>

using namespace gsml3parser;

int main() {
    std::cout << "=== Location Update Procedure Demo ===\n\n";

    // 1. Create subscriber session with TMSI.
    SubscriberRegistry registry;
    auto* session = registry.createByTMSI(0x12345678);
    if (!session) return 1;
    std::cout << "Session created for TMSI=0x12345678\n";

    ProcedureRunner& runner = session->procedures;
    int sinkCalls = 0;

    ResponseSink responseSink = makeResponseSink(
        [&sinkCalls](SMAction action, const ParsedMessage&, const SubscriberSession*) {
            (void)action;
            ++sinkCalls;
            std::cout << "  [ResponseSink called #" << sinkCalls << "]\n";
        });

    // 2. Feed CMServiceRequest(LocationUpdateRequest) -> auto-creates LU procedure.
    std::cout << "\n--- Step 1: CMServiceRequest(LocationUpdating) ---\n";
    auto cmReq = L3CMServiceRequest::builder()
        .mobileIdentity(L3MobileIdentity(0x12345678))
        .serviceType(L3CMServiceType(L3CMServiceType::LocationUpdateRequest))
        .build();
    ParsedMessage cmReqMsg{std::move(cmReq)};

    auto r1 = runner.feed(cmReqMsg, session, responseSink);
    std::cout << "Action: " << static_cast<int>(r1.action)
              << " (Continue=" << static_cast<int>(ProcedureStepResult::Action::Continue) << ")\n";
    std::cout << "Active procedures: " << runner.activeCount() << "\n";

    // 3. Feed another MM message to advance IDENTITY_CHECK -> AUTH_CHECK (TMSI known).
    std::cout << "\n--- Step 2: Advance to AUTH_CHECK ---\n";
    ParsedMessage mmMsg{MMM{L3MMStatus{}}};
    auto r2 = runner.feed(mmMsg, session, responseSink);
    std::cout << "Action: " << static_cast<int>(r2.action) << "\n";

    // 4. Feed external auth data (16-byte RAND + 4-byte expected SRES).
    std::cout << "\n--- Step 3: FeedExternal auth data (RAND + SRES) ---\n";

    auto r3 = runner.feedExternalTyped(procedure::ProcedureType::LocationUpdate, AuthChallenge{}, responseSink);
    std::cout << "Action: " << static_cast<int>(r3.action)
              << " (SendResponseWithToken=" << static_cast<int>(ProcedureStepResult::Action::SendResponseWithToken) << ")\n";

    // 5. Feed to advance AUTH_CHECK -> SEND_AUTH -> WAIT_AUTH.
    std::cout << "\n--- Step 4: Advance past auth check ---\n";
    auto r4 = runner.feed(mmMsg, session, responseSink);
    std::cout << "Action: " << static_cast<int>(r4.action) << "\n";

    // 6. Feed authentication response with matching SRES.
    std::cout << "\n--- Step 5: AuthenticationResponse (matching SRES) ---\n";
    auto authResp = L3AuthenticationResponse::builder().sres(0xEFBEADDEu).build();
    ParsedMessage authRespMsg{MMM{std::move(authResp)}};
    auto r5 = runner.feed(authRespMsg, session, responseSink);
    std::cout << "Action: " << static_cast<int>(r5.action) << "\n";

    // 7. Feed to advance LU_REQUEST -> WAITING_EXTERNAL.
    std::cout << "\n--- Step 6: Advance to WAITING_EXTERNAL ---\n";
    auto r6 = runner.feed(mmMsg, session, responseSink);
    std::cout << "Action: " << static_cast<int>(r6.action) << "\n";

    auto* luProc = runner.getActive(procedure::ProcedureType::LocationUpdate);
    if (luProc) {
        std::cout << "Procedure state: " << procedureStateName(luProc->state()) << "\n";
    }

    // 8. Feed VLR accept decision -> COMPLETED.
    std::cout << "\n--- Step 7: FeedExternal VLR accept ---\n";
    auto r7 = runner.feedExternalTyped(procedure::ProcedureType::LocationUpdate, VLRDecision{true, 0x12345678u, MMRejectCause::Zero}, responseSink);
    std::cout << "Action: " << static_cast<int>(r7.action)
              << " (Completed=" << static_cast<int>(ProcedureStepResult::Action::Completed) << ")\n";

    // 9. Verify completion and auto-cleanup.
    std::cout << "\n--- Result ---\n";
    if (r7.action == ProcedureStepResult::Action::Completed) {
        std::cout << "Location update completed successfully!\n";
        std::cout << "  Reason: " << r7.finalResult.reason << "\n";
    }
    std::cout << "Active procedures after completion: " << runner.activeCount() << "\n";
    std::cout << "ResponseSink was called " << sinkCalls << " times.\n";

    return 0;
}
