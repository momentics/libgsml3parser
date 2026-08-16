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

/// Mobile-Terminated Call Setup procedure (TS 24.008 6.1).
///
/// Manages the MTC flow: Paging, PagingResponse, SDCCH assignment, Setup delivery,
/// CallConfirmed, TCH assignment, Connect, Active. Uses T3109 for paging retries,
/// T3101 for call setup phases.
///
/// 3GPP TS 24.008 6.1 - Mobile Terminated Call establishment.
/// Thread safety: NOT thread-safe.
/// Memory: Minimal state, zero heap allocations.
#pragma once

#include <chrono>
#include <cstdint>
#include <string>

#include "gsml3parser/stack/procedure.h"
#include "gsml3parser/common/l3common.h"

namespace gsml3parser {

class SubscriberSession;

/// Mobile-Terminated Call Setup procedure per TS 24.008 6.1.
///
/// State machine:
///   INIT -> [external trigger] -> PAGE
///   PAGE -> [send PagingRequestType1/2/3, start T3109] -> WAIT_PAGE_RESPONSE
///   WAIT_PAGE_RESPONSE -> [recv PagingResponse] -> ASSIGN_SDCCH
///   ASSIGN_SDCCH -> [send ImmediateAssignment] -> SEND_SETUP
///   SEND_SETUP -> [send Setup, start T3101] -> WAIT_CONFIRMED
///   WAIT_CONFIRMED -> [recv CallConfirmed] -> ASSIGN_TCH
///   ... (similar to MO from here) ...
class CallSetupMTPercedure : public Procedure {
public:
    explicit CallSetupMTPercedure(std::string calledNumber);

    [[nodiscard]] procedure::ProcedureType type() const override;
    [[nodiscard]] procedure::ProcedureState state() const override;
    [[nodiscard]] ProcedureStepResult feed(const ParsedMessage& msg,
                                             SubscriberSession* session,
                                             ResponseSink&& sink) override;
    [[nodiscard]] ProcedureStepResult feedExternal(
        std::span<const uint8_t> data, ResponseSink&& sink = {}) override;
    [[nodiscard]] ProcedureStepResult tick(std::chrono::milliseconds delta) override;
    void cancel() noexcept override;

    /// Get the called number.
    [[nodiscard]] const std::string& calledNumber() const noexcept { return mCalledNumber; }

private:
    enum class State : uint8_t {
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

    State mCurrentState{State::INIT};
    procedure::ProcedureState mProcState{procedure::ProcedureState::Initiated};
    std::string mCalledNumber;
    uint8_t mTI{0};
    uint8_t mPageAttempt{0};
    static constexpr uint8_t MAX_PAGE_ATTEMPTS = 3;

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
