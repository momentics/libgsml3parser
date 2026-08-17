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

/// CRTP mixin for common procedure state management and timer handling.
///
/// Eliminates ~50 lines of duplicated code per procedure (transitionTo, fail, complete,
/// startTimer, stopTimer, tick, cancel). Uses CRTP pattern so each derived procedure
/// provides its own State enum and implements three hooks: doTransitionTo, doFail, doComplete.
///
/// Does NOT inherit from Procedure; instead uses multiple inheritance:
///   class XxxProcedure : public Procedure, public ProcedureStateMixin<XxxProcedure, State>
///
/// All code is inline (header-only template). No .cpp file needed.
///
/// 3GPP: TS 24.008 - Procedure lifecycle and timer management.
/// Thread safety: NOT thread-safe. Same constraints as base Procedure class.
/// Memory: Adds ~20 bytes per procedure instance (state + timer fields).
#pragma once

#include <chrono>
#include <cstdint>
#include <string_view>

#include "gsml3parser/stack/l3_timer.h"
#include "gsml3parser/stack/procedure.h"
#include "gsml3parser/stack/procedure_types.h"

namespace gsml3parser {

template<typename Derived, typename StateEnum>
class ProcedureStateMixin {
protected:
    /// Current internal FSM state of the procedure.
    StateEnum mCurrentState{};

    /// High-level lifecycle state reported to ProcedureRunner.
    procedure::ProcedureState mProcState{procedure::ProcedureState::Initiated};

    /// Remaining time on the active timer (milliseconds).
    std::chrono::milliseconds mTimerRemaining{0};

    /// Identifier of the currently active L3 protocol timer.
    L3TimerId mCurrentTimer{L3TimerId::Unknown};

    /// Whether a timer is currently running.
    bool mTimerRunning{false};

    // State transitions

    void transitionTo(StateEnum s) {
        static_cast<Derived&>(*this).doTransitionTo(s);
    }

    void fail(std::string_view reason) {
        stopTimer();
        static_cast<Derived&>(*this).doFail(reason);
    }

    void complete() {
        stopTimer();
        static_cast<Derived&>(*this).doComplete();
    }

    // Timer management

    void startTimer(L3TimerId id, std::chrono::milliseconds duration) {
        mCurrentTimer = id;
        mTimerRemaining = duration;
        mTimerRunning = true;
    }

    void stopTimer() noexcept {
        mTimerRunning = false;
        mCurrentTimer = L3TimerId::Unknown;
        mTimerRemaining = std::chrono::milliseconds(0);
    }

public:
    // Called by derived Procedure::tick() override

    [[nodiscard]] ProcedureStepResult doTick(std::chrono::milliseconds delta) {
        if (!mTimerRunning) {
            return {ProcedureStepResult::Action::Continue};
        }
        mTimerRemaining -= delta;
        if (mTimerRemaining <= std::chrono::milliseconds(0)) {
            stopTimer();
            static_cast<Derived&>(*this).doFail("timer_expired");
            ProcedureStepResult result;
            result.action = ProcedureStepResult::Action::Failed;
            auto procType = static_cast<Derived&>(*this).type();
            result.finalResult = {procType, mProcState, "timer_expired"};
            return result;
        }
        return {ProcedureStepResult::Action::Continue};
    }

    void doCancel() noexcept {
        stopTimer();
        static_cast<Derived&>(*this).doFail("cancelled");
    }
};

} // namespace gsml3parser
