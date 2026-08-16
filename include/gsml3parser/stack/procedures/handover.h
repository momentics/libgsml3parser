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

/// Handover procedure (TS 04.08 9.1.40).
///
/// Manages handover: receives target channel via feedExternal(), sends HandoverCommand,
/// waits for HandoverComplete or HandoverFailure from MS. After completion, updates
/// the MSContext with the new channel assignment.
///
/// 3GPP TS 04.08 9.1.40 - Handover procedure.
/// Thread safety: NOT thread-safe.
/// Memory: Minimal state, zero heap allocations.
#pragma once

#include <chrono>
#include <cstdint>

#include "gsml3parser/stack/procedure.h"
#include "gsml3parser/common/l3common.h"

namespace gsml3parser {

class SubscriberSession;

/// Handover procedure per TS 04.08 9.1.40.
///
/// State machine:
///   INIT -> [feedExternal: target ChannelDescriptor] -> SEND_HO_CMD
///   SEND_HO_CMD -> [send HandoverCommand, start T3101] -> WAIT_HO_COMPLETE
///   WAIT_HO_COMPLETE -> [recv HandoverComplete] -> COMPLETE
///   WAIT_HO_COMPLETE -> [recv HandoverFailure] -> FAILED
///   WAIT_HO_COMPLETE -> [T3101 expired] -> FAILED
class HandoverProcedure : public Procedure {
public:
    explicit HandoverProcedure(L3ChannelDescription target);

    [[nodiscard]] procedure::ProcedureType type() const override;
    [[nodiscard]] procedure::ProcedureState state() const override;
    [[nodiscard]] ProcedureStepResult feed(const ParsedMessage& msg,
                                             SubscriberSession* session,
                                             ResponseSink&& sink) override;
    [[nodiscard]] ProcedureStepResult feedExternal(
        std::span<const uint8_t> data, ResponseSink&& sink = {}) override;
    [[nodiscard]] ProcedureStepResult tick(std::chrono::milliseconds delta) override;
    void cancel() noexcept override;

    /// Get the target channel descriptor.
    [[nodiscard]] const L3ChannelDescription& targetChannel() const noexcept { return mTargetChannel; }

private:
    enum class State : uint8_t {
        INIT,
        SEND_HO_CMD,
        WAIT_HO_COMPLETE,
        COMPLETED,
        FAILED
    };

    State mCurrentState{State::INIT};
    procedure::ProcedureState mProcState{procedure::ProcedureState::Initiated};
    L3ChannelDescription mTargetChannel;

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
