// Copyright 2026 momentics <momentics@gmail.com>
// Copyright libgsml3parser contributors
// MIT License - see header for full text.

/// Ciphering Mode procedure (TS 24.008 4.4.3 / TS 04.08 9.1.37).
#pragma once

#include <chrono>
#include <cstdint>

#include "gsml3parser/stack/procedure.h"
#include "gsml3parser/stack/procedure_state_mixin.h"

namespace gsml3parser {

class SubscriberSession;

enum class CipheringModeState : uint8_t {
    INIT,
    SEND_COMMAND,
    WAIT_COMPLETE,
    COMPLETED,
    FAILED
};

/// Ciphering Mode procedure per TS 24.008 4.4.3.
class CipheringModeProcedure : public Procedure,
                                public ProcedureStateMixin<CipheringModeProcedure, CipheringModeState> {
public:
    using State = CipheringModeState;

    explicit CipheringModeProcedure(uint8_t algo);

    [[nodiscard]] procedure::ProcedureType type() const override;
    [[nodiscard]] procedure::ProcedureState state() const override;
    [[nodiscard]] ProcedureStepResult feed(const ParsedMessage& msg,
                                              SubscriberSession* session,
                                              ResponseSink sink) override;
    [[nodiscard]] ProcedureStepResult feedExternalTyped(
        const ExternalData& data, SubscriberSession* session, ResponseSink sink = {}) override;
    [[nodiscard]] ProcedureStepResult tick(std::chrono::milliseconds delta) override;
    void cancel() noexcept override;

    [[nodiscard]] uint8_t cipherAlgo() const noexcept { return mCipherAlgo; }

private:
    uint8_t mCipherAlgo{0};

public:
    /// CRTP hooks called by ProcedureStateMixin base.
    void doTransitionTo(State s);
    void doFail(std::string_view reason);
    void doComplete();
};

} // namespace gsml3parser
