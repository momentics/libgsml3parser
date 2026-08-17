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

#include "gsml3parser/stack/procedure_runner.h"

#include "gsml3parser/stack/subscriber_registry.h"
#include "gsml3parser/stack/procedures/location_update.h"
#include "gsml3parser/stack/procedures/authentication.h"
#include "gsml3parser/stack/procedures/call_setup_mo.h"
#include "gsml3parser/stack/procedures/call_setup_mt.h"
#include "gsml3parser/stack/procedures/channel_assignment.h"
#include "gsml3parser/stack/procedures/ciphering_mode.h"
#include "gsml3parser/stack/procedures/paging.h"
#include "gsml3parser/stack/procedures/handover.h"

namespace gsml3parser {

// ── ProcedureRunner implementation ────────────────────────────────────

procedure::ProcedureType ProcedureRunner::pdToProcedureType(L3PD pd) noexcept {
    switch (pd) {
        case L3PD::RadioResource:
            return procedure::ProcedureType::ChannelAssignment;
        case L3PD::MobilityManagement:
            return procedure::ProcedureType::LocationUpdate;
        case L3PD::CallControl:
            return procedure::ProcedureType::CallSetup_MO;
        default:
            return procedure::ProcedureType::Unknown;
    }
}

std::optional<size_t> ProcedureRunner::findActiveSlotByPD(L3PD pd) noexcept {
    procedure::ProcedureType target = pdToProcedureType(pd);
    for (size_t i = 0; i < MAX_PROCEDURES; ++i) {
        if (mSlots[i].active && mSlots[i].proc &&
            mSlots[i].proc->type() == target) {
            return i;
        }
    }
    return std::nullopt;
}

std::optional<size_t> ProcedureRunner::insertSlot(std::unique_ptr<Procedure> proc) {
    if (!proc) return std::nullopt;
    for (size_t i = 0; i < MAX_PROCEDURES; ++i) {
        if (!mSlots[i].active) {
            mSlots[i].proc = std::move(proc);
            mSlots[i].active = true;
            return i;
        }
    }
    return std::nullopt;
}

void ProcedureRunner::cleanupSlotIfTerminal(size_t idx) noexcept {
    if (idx < MAX_PROCEDURES && mSlots[idx].active) {
        auto st = mSlots[idx].proc->state();
        if (st == procedure::ProcedureState::Completed ||
            st == procedure::ProcedureState::Failed ||
            st == procedure::ProcedureState::TimedOut) {
            mSlots[idx].proc.reset();
            mSlots[idx].active = false;
        }
    }
}

ProcedureStepResult ProcedureRunner::feed(const ParsedMessage& msg,
    SubscriberSession* session, ResponseSink&& sink) {
    auto pd = messagePD(msg);

    // Try to route to an active procedure matching the PD
    auto slotIdx = findActiveSlotByPD(pd);
    if (slotIdx.has_value()) {
        ProcedureStepResult result = mSlots[slotIdx.value()].proc->feed(msg, session, std::move(sink));
        cleanupSlotIfTerminal(slotIdx.value());
        return result;
    }

    // No active procedure — try to auto-create one
    auto newProc = autoCreateProcedure(msg, session);
    slotIdx = insertSlot(std::move(newProc));
    if (slotIdx.has_value()) {
        ProcedureStepResult result = mSlots[slotIdx.value()].proc->feed(msg, session, std::move(sink));
        cleanupSlotIfTerminal(slotIdx.value());
        return result;
    }

    return {ProcedureStepResult::Action::Continue};
}

ProcedureStepResult ProcedureRunner::feedExternalTyped(procedure::ProcedureType type,
    const ExternalData& data, ResponseSink&& sink) {
    for (size_t i = 0; i < MAX_PROCEDURES; ++i) {
        if (mSlots[i].active && mSlots[i].proc && mSlots[i].proc->type() == type) {
            ProcedureStepResult result = mSlots[i].proc->feedExternalTyped(data, std::move(sink));
            cleanupSlotIfTerminal(i);
            return result;
        }
    }
    return {ProcedureStepResult::Action::Continue};
}

size_t ProcedureRunner::tickAll(std::chrono::milliseconds delta) {
    size_t failedCount = 0;
    for (size_t i = 0; i < MAX_PROCEDURES; ++i) {
        if (mSlots[i].active && mSlots[i].proc) {
            ProcedureStepResult result = mSlots[i].proc->tick(delta);
            if (result.action == ProcedureStepResult::Action::Failed) {
                ++failedCount;
            }
            cleanupSlotIfTerminal(i);
        }
    }
    return failedCount;
}

Procedure* ProcedureRunner::getActive(procedure::ProcedureType type) noexcept {
    for (size_t i = 0; i < MAX_PROCEDURES; ++i) {
        if (mSlots[i].active && mSlots[i].proc && mSlots[i].proc->type() == type) {
            return mSlots[i].proc.get();
        }
    }
    return nullptr;
}

size_t ProcedureRunner::activeCount() const noexcept {
    size_t count = 0;
    for (size_t i = 0; i < MAX_PROCEDURES; ++i) {
        if (mSlots[i].active) ++count;
    }
    return count;
}

void ProcedureRunner::cancelAll() noexcept {
    for (size_t i = 0; i < MAX_PROCEDURES; ++i) {
        if (mSlots[i].active) {
            mSlots[i].proc->cancel();
            mSlots[i].proc.reset();
            mSlots[i].active = false;
        }
    }
}

// ── Auto-create procedure from message ────────────────────────────────

std::unique_ptr<Procedure> ProcedureRunner::autoCreateProcedure(
    const ParsedMessage& msg, SubscriberSession* session) {
    (void)session;
    auto pd = messagePD(msg);
    auto mti = messageMTI(msg);

    if (pd == L3PD::RadioResource) {
        if (mti == L3ChannelRequest::MTI) {
            return ProcedureFactory::createChannelAssignment(ChannelType::SDCCHType);
        }
        if (mti == L3PagingResponse::MTI) {
            return ProcedureFactory::createChannelAssignment(ChannelType::SDCCHType);
        }
    } else if (pd == L3PD::MobilityManagement) {
        if (mti == L3CMServiceRequest::MTI) {
            const auto* cmReq = tryGet<L3CMServiceRequest>(msg);
            if (cmReq && cmReq->serviceType() == L3CMServiceType::LocationUpdateRequest) {
                return ProcedureFactory::createLocationUpdate();
            }
        }
    } else if (pd == L3PD::CallControl) {
        if (mti == L3Setup::MTI) {
            return ProcedureFactory::createCallSetupMO();
        }
    }
    return nullptr;
}

// ── ProcedureFactory implementation ───────────────────────────────────

std::unique_ptr<Procedure> ProcedureFactory::createLocationUpdate() {
    return std::make_unique<LocationUpdateProcedure>();
}

std::unique_ptr<Procedure> ProcedureFactory::createAuthentication() {
    return std::make_unique<AuthenticationProcedure>();
}

std::unique_ptr<Procedure> ProcedureFactory::createCipheringMode(uint8_t algo) {
    return std::make_unique<CipheringModeProcedure>(algo);
}

std::unique_ptr<Procedure> ProcedureFactory::createCallSetupMO() {
    return std::make_unique<CallSetupMOPercedure>();
}

std::unique_ptr<Procedure> ProcedureFactory::createCallSetupMT(const std::string& calledNumber) {
    return std::make_unique<CallSetupMTPercedure>(calledNumber);
}

std::unique_ptr<Procedure> ProcedureFactory::createChannelAssignment(ChannelType target) {
    return std::make_unique<ChannelAssignmentProcedure>(target);
}

std::unique_ptr<Procedure> ProcedureFactory::createPaging(const L3MobileIdentity& identity) {
    return std::make_unique<PagingProcedure>(identity);
}

std::unique_ptr<Procedure> ProcedureFactory::createHandover(const L3ChannelDescription& target) {
    return std::make_unique<HandoverProcedure>(target);
}

} // namespace gsml3parser
