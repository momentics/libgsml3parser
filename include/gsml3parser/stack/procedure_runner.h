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

/// ProcedureRunner manages concurrent protocol procedures for a single subscriber.
///
/// Routes incoming L3 messages to the correct active procedure based on Protocol
/// Discriminator (PD), auto-creates new procedures when no matching procedure is
/// active, and automatically cleans up completed/failed procedure slots for reuse.
/// ProcedureFactory provides static factory methods to create specific procedure types.
///
/// 3GPP specifications: TS 24.008 (procedure orchestration), TS 04.08.
/// Thread safety: NOT thread-safe. One instance per SubscriberSession.
/// Memory: Fixed-size array of up to MAX_PROCEDURES slots (~8); no heap allocation
/// for the runner itself (procedures are heap-allocated via unique_ptr).
///
/// Example:
/// @code
///   ProcedureRunner runner;
///   auto result = runner.feed(incomingMsg, session, responseSink);
///   if (result.action == ProcedureStepResult::Action::Completed) {
///       // Procedure finished; slot automatically freed
///   }
///   // Feed external data:
///   runner.feedExternal(ProcedureType::LocationUpdate, randBytes);
///   // Tick all procedures:
///   size_t failed = runner.tickAll(std::chrono::milliseconds(100));
/// @endcode
#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "gsml3parser/message_types.h"
#include "gsml3parser/visitor.h"
#include "gsml3parser/types.h"
#include "gsml3parser/common/l3common.h"
#include "gsml3parser/mm/l3mmelements.h"
#include "gsml3parser/stack/procedure.h"
#include "gsml3parser/stack/procedure_types.h"

namespace gsml3parser {

class SubscriberSession;

/// Manager for concurrent protocol procedures belonging to one subscriber.
///
/// Maintains a fixed-size array of procedure slots. Each slot holds a unique_ptr<Procedure>
/// and an active flag. Incoming messages are routed to the matching active procedure
/// based on the message's Protocol Discriminator (PD). If no active procedure matches,
/// a new procedure is auto-created using ProcedureFactory.
///
/// After each feed() call, if the result action is Completed or Failed, the slot is
/// automatically freed (proc.reset(), active=false) for reuse by subsequent procedures.
class ProcedureRunner {
public:
    ProcedureRunner() = default;

    /// Feed an incoming L3 message. Routes to the active procedure or starts a new one.
    /// @param msg The parsed L3 message from the MS.
    /// @param session Pointer to the subscriber session for context.
    /// @param sink Response callback invoked when the procedure needs to send a response.
    /// @return ProcedureStepResult from the target procedure. Slot is auto-freed on Completed/Failed.
    ProcedureStepResult feed(const ParsedMessage& msg, SubscriberSession* session,
                              ResponseSink sink);

    /// Feed typed external data to an active procedure by type.
    /// @param type The ProcedureType of the target procedure.
    /// @param data Strongly-typed external data variant (AuthChallenge, VLRDecision, etc.).
    /// @param sink Optional response callback.
    /// @return ProcedureStepResult from the target procedure. Slot is auto-freed on Completed/Failed.
    ProcedureStepResult feedExternalTyped(procedure::ProcedureType type, const ExternalData& data,
                                            ResponseSink sink = {});

    /// Tick all active procedures' timers. Auto-cleans slots for Completed/Failed procedures.
    /// @param delta Elapsed time in milliseconds.
    /// @return Number of procedures that transitioned to Failed due to timeout.
    size_t tickAll(std::chrono::milliseconds delta);

    /// Get the active procedure by type, or nullptr if no procedure of that type is running.
    /// @param type The ProcedureType to search for.
    /// @return Raw pointer to the active Procedure, or nullptr.
    [[nodiscard]] Procedure* getActive(procedure::ProcedureType type) noexcept;

    /// Number of currently active procedures.
    /// @return Count of slots with active=true.
    [[nodiscard]] size_t activeCount() const noexcept;

    /// Cancel all active procedures and free their slots.
    void cancelAll() noexcept;

private:
    static constexpr size_t MAX_PROCEDURES = 8;

    struct ProcedureSlot {
        std::unique_ptr<Procedure> proc;
        bool active{false};
    };

    std::array<ProcedureSlot, MAX_PROCEDURES> mSlots{};

    /// Find an active slot whose procedure matches the PD of the incoming message.
    /// @param pd The Protocol Discriminator to match against.
    /// @return Slot index, or std::nullopt if no match found.
    [[nodiscard]] std::optional<size_t> findActiveSlotByPD(L3PD pd) noexcept;

    /// Insert a new procedure into the first free slot.
    /// @param proc The procedure to insert.
    /// @return Slot index, or std::nullopt if all slots are occupied.
    std::optional<size_t> insertSlot(std::unique_ptr<Procedure> proc);

    /// Auto-detect procedure type from the first message and create it.
    /// @param msg The incoming L3 message.
    /// @param session The subscriber session (for context).
    /// @return A new Procedure, or nullptr if the message type doesn't match any known procedure.
    [[nodiscard]] static std::unique_ptr<Procedure> autoCreateProcedure(
        const ParsedMessage& msg, SubscriberSession* session);

    /// Map PD to procedure type for routing.
    [[nodiscard]] static procedure::ProcedureType pdToProcedureType(L3PD pd) noexcept;

    /// Clean up a slot if the procedure has reached a terminal state.
    void cleanupSlotIfTerminal(size_t idx) noexcept;
};

/// Factory for creating specific protocol procedure instances.
///
/// Each static method creates a fully initialized procedure ready to be fed into
/// a ProcedureRunner or managed directly by the BTS application.
class ProcedureFactory {
public:
    /// Create a Location Update procedure (TS 24.008 4.4.1).
    [[nodiscard]] static std::unique_ptr<Procedure> createLocationUpdate();

    /// Create an Authentication procedure (TS 24.008 4.4.2).
    [[nodiscard]] static std::unique_ptr<Procedure> createAuthentication();

    /// Create a Ciphering Mode procedure (TS 24.008 4.4.3).
    /// @param algo Ciphering algorithm identifier (0=A5/0, 1=A5/1, etc.).
    [[nodiscard]] static std::unique_ptr<Procedure> createCipheringMode(uint8_t algo);

    /// Create a Mobile-Originated Call Setup procedure (TS 24.008 6.1).
    [[nodiscard]] static std::unique_ptr<Procedure> createCallSetupMO();

    /// Create a Mobile-Terminated Call Setup procedure (TS 24.008 6.1).
    /// @param calledNumber The dialed number string.
    [[nodiscard]] static std::unique_ptr<Procedure> createCallSetupMT(const std::string& calledNumber);

    /// Create a Channel Assignment procedure (TS 04.08 9.1.2 / 9.1.35).
    /// @param target The target channel type to assign.
    [[nodiscard]] static std::unique_ptr<Procedure> createChannelAssignment(ChannelType target);

    /// Create a Paging procedure (TS 04.08 9.1.25).
    /// @param identity The mobile identity to page.
    [[nodiscard]] static std::unique_ptr<Procedure> createPaging(const L3MobileIdentity& identity);

    /// Create a Handover procedure (TS 04.08 9.1.40).
    /// @param target The target channel descriptor for handover.
    [[nodiscard]] static std::unique_ptr<Procedure> createHandover(const L3ChannelDescription& target);

    /// Create a Call Release procedure (TS 24.008 6.1).
    /// @param ti Transaction Identifier (0-7).
    /// @param cause CC cause for the disconnect message.
    [[nodiscard]] static std::unique_ptr<Procedure> createCallRelease(uint8_t ti, CCCause cause);

    /// Create an IMSI Detach procedure (TS 24.008 4.4.6).
    [[nodiscard]] static std::unique_ptr<Procedure> createIMSIDetach();
};

} // namespace gsml3parser
