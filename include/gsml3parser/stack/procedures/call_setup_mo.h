// Copyright 2026 momentics <momentics@gmail.com>
// Copyright libgsml3parser contributors
// MIT License - see header for full text.

/// Mobile-Originated Call Setup procedure (TS 24.008 6.1).
#pragma once

#include <chrono>
#include <cstdint>

#include "gsml3parser/stack/procedure.h"
#include "gsml3parser/stack/procedure_state_mixin.h"

namespace gsml3parser {

class SubscriberSession;

enum class CallSetupMOState : uint8_t {
    INIT,
    SERVICE_ACCEPT,
    WAIT_SETUP,
    PROCEEDING,
    ASSIGN_TCH,
    WAIT_ASSIGN_COMPLETE,
    ALERTING,
    CONNECT,
    ACTIVE,
    COMPLETED,
    FAILED
};

/// Mobile-Originated Call Setup procedure per TS 24.008 6.1.
class CallSetupMOPercedure : public Procedure,
                              public ProcedureStateMixin<CallSetupMOPercedure, CallSetupMOState> {
public:
    using State = CallSetupMOState;

    CallSetupMOPercedure() = default;

    [[nodiscard]] procedure::ProcedureType type() const override;
    [[nodiscard]] procedure::ProcedureState state() const override;
    [[nodiscard]] ProcedureStepResult feed(const ParsedMessage& msg,
                                              SubscriberSession* session,
                                              ResponseSink&& sink) override;
    [[nodiscard]] ProcedureStepResult tick(std::chrono::milliseconds delta) override;
    void cancel() noexcept override;

private:
    uint8_t mTI{0};

public:
    /// CRTP hooks called by ProcedureStateMixin base.
    void doTransitionTo(State s);
    void doFail(std::string_view reason);
    void doComplete();
};

} // namespace gsml3parser
