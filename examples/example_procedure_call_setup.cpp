// Copyright 2026 momentics <momentics@gmail.com>
// Copyright libgsml3parser contributors
// MIT License - see header for full text.

// Demonstrates a Mobile-Originated Call Setup via the ProcedureRunner API.
// Shows: CMServiceRequest, Setup, AssignmentComplete, ConnectAcknowledge flow.

#include <gsml3parser/gsml3parser.hpp>

#include <iostream>

using namespace gsml3parser;

int main() {
    std::cout << "=== Call Setup (MO) Procedure Demo ===\n\n";

    // 1. Create subscriber session.
    SubscriberRegistry registry;
    auto* session = registry.createByTMSI(0x87654321);
    if (!session) return 1;
    std::cout << "Session created for TMSI=0x87654321\n";

    ProcedureRunner& runner = session->procedures;
    int sinkCalls = 0;

    auto responseSink = [&sinkCalls](SMAction action, const ParsedMessage&, const SubscriberSession*) {
        (void)action;
        ++sinkCalls;
        std::cout << "  [ResponseSink called #" << sinkCalls << "]\n";
    };

    // 2. Feed CMServiceRequest(MobileOriginatedCall) -> auto-creates... but wait,
    //    ProcedureRunner auto-creates LocationUpdate for MM messages with CMServiceRequest.
    //    For a call setup, we need to create CallSetupMO directly or use CC Setup message.
    //    The autoCreateProcedure creates CallSetupMO when it sees a CC Setup message.

    std::cout << "\n--- Step 1: Setup message (auto-creates CallSetupMO) ---\n";
    auto setup = L3Setup::builder().ti(1).build();
    ParsedMessage setupMsg{CCM{std::move(setup)}};

    auto r1 = runner.feed(setupMsg, session, responseSink);
    std::cout << "Action: " << static_cast<int>(r1.action)
              << " (SendResponseWithToken=" << static_cast<int>(ProcedureStepResult::Action::SendResponseWithToken) << ")\n";
    std::cout << "Active procedures: " << runner.activeCount() << "\n";

    auto* moc = runner.getActive(procedure::ProcedureType::CallSetup_MO);
    if (moc) {
        std::cout << "Procedure state: " << procedureStateName(moc->state()) << "\n";
    }

    // 3. Advance through PROCEEDING -> ASSIGN_TCH.
    std::cout << "\n--- Step 2: Advance to PROCEEDING/ASSIGN_TCH ---\n";
    ParsedMessage dummyCC{CCM{L3CCStatus{}}};
    auto r2 = runner.feed(dummyCC, session, responseSink);
    std::cout << "Action: " << static_cast<int>(r2.action) << "\n";

    // 4. Advance to WAIT_ASSIGN_COMPLETE.
    std::cout << "\n--- Step 3: Advance to WAIT_ASSIGN_COMPLETE ---\n";
    auto r3 = runner.feed(dummyCC, session, responseSink);
    std::cout << "Action: " << static_cast<int>(r3.action) << "\n";

    if (moc) {
        std::cout << "Procedure state: " << procedureStateName(moc->state()) << "\n";
    }

    // 5. Feed AssignmentComplete -> ALERTING.
    std::cout << "\n--- Step 4: AssignmentComplete -> ALERTING ---\n";
    auto assignComplete = L3AssignmentComplete::builder().build();
    ParsedMessage acMsg{RRM{std::move(assignComplete)}};
    auto r4 = runner.feed(acMsg, session, responseSink);
    std::cout << "Action: " << static_cast<int>(r4.action) << "\n";

    // 6. Advance ALERTING -> CONNECT.
    std::cout << "\n--- Step 5: Advance to CONNECT ---\n";
    auto r5 = runner.feed(dummyCC, session, responseSink);
    std::cout << "Action: " << static_cast<int>(r5.action) << "\n";

    // 7. Advance CONNECT -> ACTIVE.
    std::cout << "\n--- Step 6: Advance to ACTIVE ---\n";
    auto r6 = runner.feed(dummyCC, session, responseSink);
    std::cout << "Action: " << static_cast<int>(r6.action) << "\n";

    if (moc) {
        std::cout << "Procedure state: " << procedureStateName(moc->state()) << "\n";
    }

    // 8. Feed ConnectAcknowledge -> COMPLETED.
    std::cout << "\n--- Step 7: ConnectAcknowledge -> COMPLETE ---\n";
    auto connAck = L3ConnectAcknowledge::builder().ti(1).build();
    ParsedMessage caMsg{CCM{std::move(connAck)}};
    auto r7 = runner.feed(caMsg, session, responseSink);
    std::cout << "Action: " << static_cast<int>(r7.action)
              << " (Completed=" << static_cast<int>(ProcedureStepResult::Action::Completed) << ")\n";

    // 9. Verify.
    std::cout << "\n--- Result ---\n";
    if (r7.action == ProcedureStepResult::Action::Completed) {
        std::cout << "Call setup completed successfully!\n";
        std::cout << "  Reason: " << r7.finalResult.reason << "\n";
    }
    std::cout << "Active procedures after completion: " << runner.activeCount() << "\n";
    std::cout << "ResponseSink was called " << sinkCalls << " times.\n";

    return 0;
}
