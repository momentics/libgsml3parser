// Copyright 2026 momentics <momentics@gmail.com>
// Copyright libgsml3parser contributors
// MIT License - see header for full text.

/// Authentication procedure (TS 24.008 4.4.2).
#pragma once

#include <array>
#include <chrono>
#include <cstdint>

#include "gsml3parser/stack/procedure.h"
#include "gsml3parser/stack/procedure_state_mixin.h"

namespace gsml3parser {

class SubscriberSession;

enum class AuthenticationState : uint8_t {
    INIT,
    SEND_AUTH_REQ,
    WAIT_RESPONSE,
    VERIFY_SRES,
    COMPLETED,
    FAILED
};

/// Authentication procedure per TS 24.008 4.4.2.
class AuthenticationProcedure : public Procedure,
                                 public ProcedureStateMixin<AuthenticationProcedure, AuthenticationState> {
public:
    using State = AuthenticationState;

    AuthenticationProcedure() = default;

    [[nodiscard]] procedure::ProcedureType type() const override;
    [[nodiscard]] procedure::ProcedureState state() const override;
    [[nodiscard]] ProcedureStepResult feed(const ParsedMessage& msg,
                                              SubscriberSession* session,
                                              ResponseSink&& sink) override;
    [[nodiscard]] ProcedureStepResult feedExternalTyped(
        const ExternalData& data, ResponseSink&& sink = {}) override;
    [[nodiscard]] ProcedureStepResult tick(std::chrono::milliseconds delta) override;
    void cancel() noexcept override;

private:
    std::array<uint8_t, 16> mRandBuffer{};
    bool mHasRand{false};

    std::array<uint8_t, 4> mExpectedSRES{};
    bool mHasExpectedSRES{false};

public:
    /// CRTP hooks called by ProcedureStateMixin base.
    void doTransitionTo(State s);
    void doFail(std::string_view reason);
    void doComplete();
};

} // namespace gsml3parser
