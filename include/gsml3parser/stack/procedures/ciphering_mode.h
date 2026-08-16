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

/// Ciphering Mode procedure (TS 24.008 4.4.3 / TS 04.08 9.1.37).
///
/// Short procedure to activate ciphering: receives algorithm and key via feedExternal(),
/// sends CipheringModeCommand, waits for CipheringModeComplete from MS.
///
/// 3GPP TS 24.008 4.4.3 - Ciphering mode procedure.
/// Thread safety: NOT thread-safe.
/// Memory: Minimal state, zero heap allocations.
#pragma once

#include <chrono>
#include <cstdint>

#include "gsml3parser/stack/procedure.h"

namespace gsml3parser {

class SubscriberSession;

/// Ciphering Mode procedure per TS 24.008 4.4.3.
///
/// State machine:
///   INIT -> [feedExternal: cipher algo + key] -> SEND_COMMAND
///   SEND_COMMAND -> [send CipheringModeCommand] -> WAIT_COMPLETE
///   WAIT_COMPLETE -> [recv CipheringModeComplete] -> COMPLETE
class CipheringModeProcedure : public Procedure {
public:
    explicit CipheringModeProcedure(uint8_t algo);

    [[nodiscard]] procedure::ProcedureType type() const override;
    [[nodiscard]] procedure::ProcedureState state() const override;
    [[nodiscard]] ProcedureStepResult feed(const ParsedMessage& msg,
                                             SubscriberSession* session,
                                             ResponseSink&& sink) override;
    [[nodiscard]] ProcedureStepResult feedExternal(
        std::span<const uint8_t> data, ResponseSink&& sink = {}) override;
    [[nodiscard]] ProcedureStepResult tick(std::chrono::milliseconds delta) override;
    void cancel() noexcept override;

    /// Get the ciphering algorithm identifier.
    [[nodiscard]] uint8_t cipherAlgo() const noexcept { return mCipherAlgo; }

private:
    enum class State : uint8_t {
        INIT,
        SEND_COMMAND,
        WAIT_COMPLETE,
        COMPLETED,
        FAILED
    };

    State mCurrentState{State::INIT};
    procedure::ProcedureState mProcState{procedure::ProcedureState::Initiated};
    uint8_t mCipherAlgo{0};

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
