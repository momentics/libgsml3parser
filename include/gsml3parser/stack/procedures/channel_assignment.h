// Copyright 2026 momentics <momentics@gmail.com>
// Copyright libgsml3parser contributors
// MIT License - see header for full text.

/// Channel Assignment procedure (TS 04.08 9.1.2 / 9.1.35).
#pragma once

#include <chrono>
#include <cstdint>

#include "gsml3parser/stack/procedure.h"
#include "gsml3parser/stack/procedure_state_mixin.h"
#include "gsml3parser/types.h"

namespace gsml3parser {

class SubscriberSession;

enum class ChannelAssignmentState : uint8_t {
    INIT,
    ALLOCATE_CHANNEL,
    SEND_IMMEDIATE_ASSIGNMENT,
    WAIT_SEIZURE,
    COMPLETED,
    FAILED
};

/// Channel Assignment procedure per TS 04.08 9.1.2 / 9.1.35.
class ChannelAssignmentProcedure : public Procedure,
                                    public ProcedureStateMixin<ChannelAssignmentProcedure, ChannelAssignmentState> {
public:
    using State = ChannelAssignmentState;

    explicit ChannelAssignmentProcedure(ChannelType target);

    [[nodiscard]] procedure::ProcedureType type() const override;
    [[nodiscard]] procedure::ProcedureState state() const override;
    [[nodiscard]] ProcedureStepResult feed(const ParsedMessage& msg,
                                              SubscriberSession* session,
                                              ResponseSink sink) override;
    [[nodiscard]] bool matches(const ParsedMessage& msg) const override;
    [[nodiscard]] ProcedureStepResult tick(std::chrono::milliseconds delta) override;
    void cancel() noexcept override;

    [[nodiscard]] ChannelType targetChannelType() const noexcept { return mTargetType; }

private:
    ChannelType mTargetType{ChannelType::SDCCHType};

public:
    /// CRTP hooks called by ProcedureStateMixin base.
    void doTransitionTo(State s);
    void doFail(std::string_view reason);
    void doComplete();
};

} // namespace gsml3parser
