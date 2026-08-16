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

/// Location Update procedure implementation (TS 24.008 4.4.1).
///
/// Manages the full location updating flow: identity check, optional authentication,
/// VLR/BSC decision via feedExternal(), and TMSI reallocation. Uses internal timers
/// T3106 (authentication), T3103 (location update), and T3108 (TMSI assignment).
///
/// 3GPP TS 24.008 4.4.1 - Normal/IMSI-attached location updating procedure.
/// Thread safety: NOT thread-safe. One instance per location update procedure.
/// Memory: Pre-allocated RAND buffer (32 bytes), zero heap allocations for state.
#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>

#include "gsml3parser/stack/procedure.h"
#include "gsml3parser/common/l3common.h"

namespace gsml3parser {

class SubscriberSession;

/// Full Location Update procedure per TS 24.008 4.4.1.
///
/// State machine:
///   INIT -> [recv CMServiceRequest/PagingResponse] -> IDENTITY_CHECK
///   IDENTITY_CHECK -> [TMSI known?] -> AUTH_CHECK / [unknown?] -> REQUEST_IDENTITY
///   REQUEST_IDENTITY -> [recv IdentityResponse] -> AUTH_CHECK
///   AUTH_CHECK -> [need auth?] -> SEND_AUTH / [skip?] -> LU_REQUEST
///   SEND_AUTH -> [send AuthenticationRequest, start T3106] -> WAIT_AUTH
///   WAIT_AUTH -> [recv AuthenticationResponse] -> VERIFY_AUTH
///   VERIFY_AUTH -> [SRES matches?] -> LU_REQUEST / [no match?] -> REJECT
///   LU_REQUEST -> [forward to VLR/BSC] -> WAITING_EXTERNAL
///   WAITING_EXTERNAL -> [feedExternal: accept] -> SEND_ACCEPT
///   WAITING_EXTERNAL -> [feedExternal: reject] -> SEND_REJECT
///   SEND_ACCEPT -> [send LocationUpdatingAccept, start T3108 if TMSI] -> COMPLETE
///   SEND_REJECT -> [send LocationUpdatingReject] -> FAILED
class LocationUpdateProcedure : public Procedure {
public:
    LocationUpdateProcedure() = default;

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
        IDENTITY_CHECK,
        REQUEST_IDENTITY,
        AUTH_CHECK,
        SEND_AUTH,
        WAIT_AUTH,
        VERIFY_AUTH,
        LU_REQUEST,
        WAITING_EXTERNAL,
        SEND_ACCEPT,
        SEND_REJECT,
        COMPLETED,
        FAILED
    };

    State mCurrentState{State::INIT};
    procedure::ProcedureState mProcState{procedure::ProcedureState::Initiated};

    // Pre-allocated buffer for RAND from AuC (128 bits = 16 bytes, padded to 32).
    std::array<uint8_t, 32> mRandBuffer{};
    bool mHasRand{false};

    // Expected SRES for verification (set via feedExternal along with RAND).
    std::array<uint8_t, 4> mExpectedSRES{};
    bool mHasExpectedSRES{false};

    // VLR decision data
    L3LocationAreaIdentity mLAI{};
    std::optional<uint32_t> mNewTmsi;
    MMRejectCause mRejectCause{MMRejectCause::Zero};

    // Timer tracking
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
