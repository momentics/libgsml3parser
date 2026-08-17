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

/// IMSI Detach procedure (TS 24.008 4.4.6).
///
/// Manages the IMSI detach flow: CMServiceAccept -> detach complete.
/// Uses timer T3110 for detach acknowledgement timeout.
///
/// 3GPP TS 24.008 4.4.6 - IMSI detach procedure.
/// Thread safety: NOT thread-safe. One instance per IMSI detach.
/// Memory: Pre-allocated state, zero heap allocations during feed().
#pragma once

#include <cstdint>

#include "gsml3parser/stack/procedure.h"
#include "gsml3parser/stack/procedure_state_mixin.h"

namespace gsml3parser {

class SubscriberSession;

enum class IMSIDetachState : uint8_t {
    INIT,
    SEND_CM_SERVICE_ACCEPT,
    WAIT_DETACH_COMPLETE,
    COMPLETED,
    FAILED
};

/// IMSI Detach procedure per TS 24.008 4.4.6.
class IMSIDetachProcedure : public Procedure,
                             public ProcedureStateMixin<IMSIDetachProcedure, IMSIDetachState> {
public:
    using State = IMSIDetachState;

    IMSIDetachProcedure() = default;

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
    uint8_t mAttempt{0};

public:
    void doTransitionTo(State s);
    void doFail(std::string_view reason);
    void doComplete();
};

} // namespace gsml3parser
