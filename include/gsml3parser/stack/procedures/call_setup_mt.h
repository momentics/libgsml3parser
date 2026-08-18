// Copyright 2026 momentics <momentics@gmail.com>
// Copyright libgsml3parser contributors
// MIT License - see header for full text.

/// Mobile-Terminated Call Setup procedure (TS 24.008 6.1).
#pragma once

#include <chrono>
#include <cstdint>
#include <string>

#include "gsml3parser/stack/procedure.h"
#include "gsml3parser/stack/procedure_state_mixin.h"
#include "gsml3parser/common/l3common.h"

namespace gsml3parser {

class SubscriberSession;

enum class CallSetupMTState : uint8_t {
    INIT,
    PAGE,
    WAIT_PAGE_RESPONSE,
    ASSIGN_SDCCH,
    SEND_SETUP,
    WAIT_CONFIRMED,
    ASSIGN_TCH,
    WAIT_ASSIGN_COMPLETE,
    ALERTING,
    CONNECT,
    ACTIVE,
    COMPLETED,
    FAILED
};

/// Mobile-Terminated Call Setup procedure per TS 24.008 6.1.
class CallSetupMTPercedure : public Procedure,
                              public ProcedureStateMixin<CallSetupMTPercedure, CallSetupMTState> {
public:
    using State = CallSetupMTState;

    explicit CallSetupMTPercedure(std::string calledNumber);

    [[nodiscard]] procedure::ProcedureType type() const override;
    [[nodiscard]] procedure::ProcedureState state() const override;
    [[nodiscard]] ProcedureStepResult feed(const ParsedMessage& msg,
                                              SubscriberSession* session,
                                              ResponseSink sink) override;
    [[nodiscard]] ProcedureStepResult feedExternalTyped(
        const ExternalData& data, ResponseSink sink = {}) override;
    [[nodiscard]] ProcedureStepResult tick(std::chrono::milliseconds delta) override;
    void cancel() noexcept override;

    [[nodiscard]] const std::string& calledNumber() const noexcept { return mCalledNumber; }

private:
    std::string mCalledNumber;
    uint8_t mTI{0};
    uint8_t mPageAttempt{0};
    static constexpr uint8_t MAX_PAGE_ATTEMPTS = 3;

public:
    /// CRTP hooks called by ProcedureStateMixin base.
    void doTransitionTo(State s);
    void doFail(std::string_view reason);
    void doComplete();
};

} // namespace gsml3parser
