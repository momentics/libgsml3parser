// Copyright 2026 momentics <momentics@gmail.com>
// Copyright libgsml3parser contributors
// MIT License - see header for full text.

/// Paging procedure (TS 04.08 9.1.25).
#pragma once

#include <chrono>
#include <cstdint>

#include "gsml3parser/stack/procedure.h"
#include "gsml3parser/stack/procedure_state_mixin.h"
#include "gsml3parser/common/l3common.h"

namespace gsml3parser {

class SubscriberSession;

enum class PagingState : uint8_t {
    INIT,
    SEND_PAGE1,
    WAIT_PAGE1,
    SEND_PAGE2,
    WAIT_PAGE2,
    SEND_PAGE3,
    WAIT_PAGE3,
    COMPLETED,
    FAILED
};

/// Paging procedure per TS 04.08 9.1.25.
class PagingProcedure : public Procedure,
                         public ProcedureStateMixin<PagingProcedure, PagingState> {
public:
    using State = PagingState;

    explicit PagingProcedure(L3MobileIdentity identity);

    [[nodiscard]] procedure::ProcedureType type() const override;
    [[nodiscard]] procedure::ProcedureState state() const override;
    [[nodiscard]] ProcedureStepResult feed(const ParsedMessage& msg,
                                              SubscriberSession* session,
                                              ResponseSink sink) override;
    [[nodiscard]] ProcedureStepResult feedExternalTyped(
        const ExternalData& data, ResponseSink sink = {}) override;
    [[nodiscard]] ProcedureStepResult tick(std::chrono::milliseconds delta) override;
    void cancel() noexcept override;

    [[nodiscard]] const L3MobileIdentity& identity() const noexcept { return mIdentity; }

private:
    L3MobileIdentity mIdentity;
    uint8_t mPageAttempt{0};
    static constexpr uint8_t MAX_PAGE_ATTEMPTS = 3;

public:
    /// CRTP hooks called by ProcedureStateMixin base.
    void doTransitionTo(State s);
    void doFail(std::string_view reason);
    void doComplete();
};

} // namespace gsml3parser
