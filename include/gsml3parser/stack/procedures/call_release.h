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

/// Call Release procedure (TS 24.008 6.1).
///
/// Manages the call release flow: Disconnect -> Release -> ReleaseComplete.
/// Uses timer T3101 for Disconnect acknowledgement timeout.
///
/// 3GPP TS 24.008 6.1 - Call Control clear command procedure.
/// Thread safety: NOT thread-safe. One instance per call release.
/// Memory: Pre-allocated state, zero heap allocations during feed().
#pragma once

#include <cstdint>

#include "gsml3parser/stack/procedure.h"
#include "gsml3parser/stack/procedure_state_mixin.h"
#include "gsml3parser/enums.h"

namespace gsml3parser {

class SubscriberSession;

enum class CallReleaseState : uint8_t {
    INIT,
    SEND_DISCONNECT,
    WAIT_RELEASE,
    SEND_RELEASE_COMPLETE,
    COMPLETED,
    FAILED
};

/// Call Release procedure per TS 24.008 6.1.
class CallReleaseProcedure : public Procedure,
                              public ProcedureStateMixin<CallReleaseProcedure, CallReleaseState> {
public:
    using State = CallReleaseState;

    /// Create a call release procedure with the given TI and cause.
    /// @param ti Transaction Identifier (0-7).
    /// @param cause CC cause for the disconnect message.
    explicit CallReleaseProcedure(uint8_t ti = 0, CCCause cause = CCCause::Normal_Call_Clearing);

    [[nodiscard]] procedure::ProcedureType type() const override;
    [[nodiscard]] procedure::ProcedureState state() const override;
    [[nodiscard]] ProcedureStepResult feed(const ParsedMessage& msg,
                                              SubscriberSession* session,
                                              ResponseSink sink) override;
    [[nodiscard]] bool matches(const ParsedMessage& msg) const override;
    [[nodiscard]] ProcedureStepResult feedExternalTyped(
        const ExternalData& data, SubscriberSession* session, ResponseSink sink = {}) override;
    [[nodiscard]] ProcedureStepResult tick(std::chrono::milliseconds delta) override;
    void cancel() noexcept override;

    /// Get the transaction identifier.
    [[nodiscard]] uint8_t ti() const noexcept;

    /// Get the CC cause.
    [[nodiscard]] CCCause cause() const noexcept;

private:
    uint8_t mTI{};
    CCCause mCause{CCCause::Normal_Call_Clearing};

public:
    void doTransitionTo(State s);
    void doFail(std::string_view reason);
    void doComplete();
};

} // namespace gsml3parser
