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

/// Authentication procedure (TS 24.008 4.4.2).
///
/// Manages the authentication exchange: receives RAND from AuC via feedExternal(),
/// sends AuthenticationRequest, verifies SRES from MS response against expected value.
/// Typically used as a sub-procedure within LocationUpdateProcedure but can also
/// be run standalone.
///
/// 3GPP TS 24.008 4.4.2 - Authentication procedure.
/// Thread safety: NOT thread-safe.
/// Memory: Pre-allocated RAND (32 bytes) and SRES (4 bytes) buffers.
#pragma once

#include <array>
#include <chrono>
#include <cstdint>

#include "gsml3parser/stack/procedure.h"

namespace gsml3parser {

class SubscriberSession;

/// Authentication procedure per TS 24.008 4.4.2.
///
/// State machine:
///   INIT -> [feedExternal: RAND+SRES] -> SEND_AUTH_REQ
///   SEND_AUTH_REQ -> [send AuthenticationRequest, start T3106] -> WAIT_RESPONSE
///   WAIT_RESPONSE -> [recv AuthenticationResponse] -> VERIFY_SRES
///   VERIFY_SRES -> [SRES matches?] -> COMPLETE / FAILED
class AuthenticationProcedure : public Procedure {
public:
    AuthenticationProcedure() = default;

    [[nodiscard]] procedure::ProcedureType type() const override;
    [[nodiscard]] procedure::ProcedureState state() const override;
    [[nodiscard]] ProcedureStepResult feed(const ParsedMessage& msg,
                                             SubscriberSession* session,
                                             ResponseSink&& sink) override;
    [[nodiscard]] ProcedureStepResult feedExternal(
        std::span<const uint8_t> data, ResponseSink&& sink = {}) override;
    [[nodiscard]] ProcedureStepResult tick(std::chrono::milliseconds delta) override;
    void cancel() noexcept override;

private:
    enum class State : uint8_t {
        INIT,
        SEND_AUTH_REQ,
        WAIT_RESPONSE,
        VERIFY_SRES,
        COMPLETED,
        FAILED
    };

    State mCurrentState{State::INIT};
    procedure::ProcedureState mProcState{procedure::ProcedureState::Initiated};

    std::array<uint8_t, 32> mRandBuffer{};
    bool mHasRand{false};

    std::array<uint8_t, 4> mExpectedSRES{};
    bool mHasExpectedSRES{false};

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
