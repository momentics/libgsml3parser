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

/// Mobile-Originated Call Setup procedure (TS 24.008 6.1).
///
/// Manages the MOC flow: CMServiceAccept, Setup -> CallProceeding, TCH assignment,
/// Alerting, Connect, ConnectAcknowledge. Uses T3101 timer for call setup phases.
///
/// 3GPP TS 24.008 6.1 - Mobile Originated Call establishment.
/// Thread safety: NOT thread-safe.
/// Memory: Minimal state, zero heap allocations.
#pragma once

#include <chrono>
#include <cstdint>

#include "gsml3parser/stack/procedure.h"

namespace gsml3parser {

class SubscriberSession;

/// Mobile-Originated Call Setup procedure per TS 24.008 6.1.
///
/// State machine:
///   INIT -> [recv CMServiceRequest(Call)] -> SERVICE_ACCEPT
///   SERVICE_ACCEPT -> [send CMServiceAccept] -> WAIT_SETUP
///   WAIT_SETUP -> [recv Setup, start T3101] -> PROCEEDING
///   PROCEEDING -> [send CallProceeding] -> ASSIGN_TCH
///   ASSIGN_TCH -> [send AssignmentCommand, start T3101] -> WAIT_ASSIGN_COMPLETE
///   WAIT_ASSIGN_COMPLETE -> [recv AssignmentComplete] -> ALERTING
///   ALERTING -> [send Alerting] -> CONNECT
///   CONNECT -> [send Connect] -> ACTIVE
///   ACTIVE -> [recv ConnectAcknowledge] -> COMPLETE
class CallSetupMOPercedure : public Procedure {
public:
    CallSetupMOPercedure() = default;

    [[nodiscard]] procedure::ProcedureType type() const override;
    [[nodiscard]] procedure::ProcedureState state() const override;
    [[nodiscard]] ProcedureStepResult feed(const ParsedMessage& msg,
                                             SubscriberSession* session,
                                             ResponseSink&& sink) override;
    [[nodiscard]] ProcedureStepResult tick(std::chrono::milliseconds delta) override;
    void cancel() noexcept override;

private:
    enum class State : uint8_t {
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

    State mCurrentState{State::INIT};
    procedure::ProcedureState mProcState{procedure::ProcedureState::Initiated};
    uint8_t mTI{0};

    std::chrono::milliseconds mTimerRemaining{0};
    L3TimerId mCurrentTimer{L3TimerId::Unknown};
    bool mTimerRunning{false};

    void transitionTo(State s);
    void fail(const std::string_view& reason);
    void complete();
    void startTimer(L3TimerId id, std::chrono::milliseconds duration);
    void stopTimer() noexcept;
};

} // namespace gsml3parser
