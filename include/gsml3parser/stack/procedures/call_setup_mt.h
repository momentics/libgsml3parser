/// Copyright 2026 momentics <momentics@gmail.com>
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

// Mobile-Terminated Call Setup procedure (TS 24.008 6.1).
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
    [[nodiscard]] bool matches(const ParsedMessage& msg) const override;
    [[nodiscard]] ProcedureStepResult feedExternalTyped(
        const ExternalData& data, SubscriberSession* session, ResponseSink sink = {}) override;
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
