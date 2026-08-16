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

/// Channel Assignment procedure (TS 04.08 9.1.2 / 9.1.35).
///
/// Manages channel assignment: receives ChannelRequest or PagingResponse, allocates
/// a channel from the pool, sends ImmediateAssignment, and waits for the MS to
/// seize the assigned channel (first L3 message on new channel).
///
/// 3GPP TS 04.08 9.1.2 - Assignment procedure, TS 04.08 9.1.35 - Immediate Assignment.
/// Thread safety: NOT thread-safe.
/// Memory: Minimal state, zero heap allocations.
#pragma once

#include <chrono>
#include <cstdint>

#include "gsml3parser/stack/procedure.h"
#include "gsml3parser/types.h"

namespace gsml3parser {

class SubscriberSession;

/// Channel Assignment procedure per TS 04.08 9.1.2 / 9.1.35.
///
/// State machine:
///   INIT -> [recv ChannelRequest/PagingResponse] -> ALLOCATE_CHANNEL
///   ALLOCATE_CHANNEL -> [pool.allocate(type)] -> SEND_IMMEDIATE_ASSIGNMENT
///   SEND_IMMEDIATE_ASSIGNMENT -> [send ImmediateAssignment, start T3101] -> WAIT_SEIZURE
///   WAIT_SEIZURE -> [MS seizes channel (first L3 msg)] -> COMPLETE
///   WAIT_SEIZURE -> [T3101 expired] -> FAILED + release channel
class ChannelAssignmentProcedure : public Procedure {
public:
    explicit ChannelAssignmentProcedure(ChannelType target);

    [[nodiscard]] procedure::ProcedureType type() const override;
    [[nodiscard]] procedure::ProcedureState state() const override;
    [[nodiscard]] ProcedureStepResult feed(const ParsedMessage& msg,
                                             SubscriberSession* session,
                                             ResponseSink&& sink) override;
    [[nodiscard]] ProcedureStepResult tick(std::chrono::milliseconds delta) override;
    void cancel() noexcept override;

    /// Get the target channel type.
    [[nodiscard]] ChannelType targetChannelType() const noexcept { return mTargetType; }

private:
    enum class State : uint8_t {
        INIT,
        ALLOCATE_CHANNEL,
        SEND_IMMEDIATE_ASSIGNMENT,
        WAIT_SEIZURE,
        COMPLETED,
        FAILED
    };

    State mCurrentState{State::INIT};
    procedure::ProcedureState mProcState{procedure::ProcedureState::Initiated};
    ChannelType mTargetType{ChannelType::SDCCHType};

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
