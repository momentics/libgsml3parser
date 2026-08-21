// Copyright 2026 momentics <momentics@gmail.com>
// Copyright libgsml3parser contributors
// MIT License - see header for full text.

/// Handover procedure (TS 04.08 9.1.40).
#pragma once

#include <chrono>
#include <cstdint>

#include "gsml3parser/stack/procedure.h"
#include "gsml3parser/stack/procedure_state_mixin.h"
#include "gsml3parser/common/l3common.h"

namespace gsml3parser {

class SubscriberSession;

enum class HandoverState : uint8_t {
    INIT,
    SEND_HO_CMD,
    WAIT_HO_COMPLETE,
    COMPLETED,
    FAILED
};

/// Handover procedure per TS 04.08 9.1.40.
class HandoverProcedure : public Procedure,
                           public ProcedureStateMixin<HandoverProcedure, HandoverState> {
public:
    using State = HandoverState;

    explicit HandoverProcedure(L3ChannelDescription target);

    [[nodiscard]] procedure::ProcedureType type() const override;
    [[nodiscard]] procedure::ProcedureState state() const override;
    [[nodiscard]] ProcedureStepResult feed(const ParsedMessage& msg,
                                              SubscriberSession* session,
                                              ResponseSink sink) override;
    [[nodiscard]] ProcedureStepResult feedExternalTyped(
        const ExternalData& data, SubscriberSession* session, ResponseSink sink = {}) override;
    [[nodiscard]] ProcedureStepResult tick(std::chrono::milliseconds delta) override;
    void cancel() noexcept override;

    [[nodiscard]] const L3ChannelDescription& targetChannel() const noexcept { return mTargetChannel; }

private:
    L3ChannelDescription mTargetChannel;

public:
    /// CRTP hooks called by ProcedureStateMixin base.
    void doTransitionTo(State s);
    void doFail(std::string_view reason);
    void doComplete();
};

} // namespace gsml3parser
