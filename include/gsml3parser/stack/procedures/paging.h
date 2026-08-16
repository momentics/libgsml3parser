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

/// Paging procedure (TS 04.08 9.1.25).
///
/// Network-initiated paging of MS with up to 3 attempts using T3109 timer between
/// each attempt. Sends PagingRequestType1, Type2, and Type3 messages sequentially.
///
/// 3GPP TS 04.08 9.1.25 - Paging procedure.
/// Thread safety: NOT thread-safe.
/// Memory: Minimal state, zero heap allocations.
#pragma once

#include <chrono>
#include <cstdint>

#include "gsml3parser/stack/procedure.h"
#include "gsml3parser/common/l3common.h"

namespace gsml3parser {

class SubscriberSession;

/// Paging procedure per TS 04.08 9.1.25.
///
/// State machine:
///   INIT -> [feedExternal: trigger] -> SEND_PAGE1
///   SEND_PAGE1 -> [send PagingRequestType1, start T3109] -> WAIT_PAGE1
///   WAIT_PAGE1 -> [T3109 expired] -> SEND_PAGE2
///   SEND_PAGE2 -> [send PagingRequestType2, start T3109] -> WAIT_PAGE2
///   WAIT_PAGE2 -> [T3109 expired] -> SEND_PAGE3
///   WAIT_PAGE3 -> [recv PagingResponse] -> COMPLETE
///   WAIT_PAGE3 -> [T3109 expired] -> FAILED (no response after 3 attempts)
class PagingProcedure : public Procedure {
public:
    explicit PagingProcedure(L3MobileIdentity identity);

    [[nodiscard]] procedure::ProcedureType type() const override;
    [[nodiscard]] procedure::ProcedureState state() const override;
    [[nodiscard]] ProcedureStepResult feed(const ParsedMessage& msg,
                                             SubscriberSession* session,
                                             ResponseSink&& sink) override;
    [[nodiscard]] ProcedureStepResult feedExternal(
        std::span<const uint8_t> data, ResponseSink&& sink = {}) override;
    [[nodiscard]] ProcedureStepResult tick(std::chrono::milliseconds delta) override;
    void cancel() noexcept override;

    /// Get the mobile identity being paged.
    [[nodiscard]] const L3MobileIdentity& identity() const noexcept { return mIdentity; }

private:
    enum class State : uint8_t {
        INIT,
        SEND_PAGE1,
        WAIT_PAGE1,
        SEND_PAGE2,
        WAIT_PAGE2,
        SEND_PAGE3,
        WAIT_PAGE3,
        COMPLETED,
        FAILED
    };

    State mCurrentState{State::INIT};
    procedure::ProcedureState mProcState{procedure::ProcedureState::Initiated};
    L3MobileIdentity mIdentity;
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
