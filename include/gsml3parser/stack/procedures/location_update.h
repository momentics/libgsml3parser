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
/// VLR/BSC decision via feedExternalTyped(VLRDecision), and TMSI reallocation. Uses internal timers
/// T3106 (authentication), T3103 (location update), and T3108 (TMSI assignment).
///
/// 3GPP TS 24.008 4.4.1 - Normal/IMSI-attached location updating procedure.
/// Thread safety: NOT thread-safe. One instance per location update procedure.
/// Memory: Pre-allocated RAND buffer (16 bytes), zero heap allocations for state.
#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>

#include "gsml3parser/stack/procedure.h"
#include "gsml3parser/stack/procedure_state_mixin.h"
#include "gsml3parser/common/l3common.h"

namespace gsml3parser {

class SubscriberSession;

enum class LocationUpdateState : uint8_t {
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

/// Full Location Update procedure per TS 24.008 4.4.1.
class LocationUpdateProcedure : public Procedure,
                                 public ProcedureStateMixin<LocationUpdateProcedure, LocationUpdateState> {
public:
    using State = LocationUpdateState;

    LocationUpdateProcedure() = default;

    [[nodiscard]] procedure::ProcedureType type() const override;
    [[nodiscard]] procedure::ProcedureState state() const override;
    [[nodiscard]] ProcedureStepResult feed(const ParsedMessage& msg,
                                              SubscriberSession* session,
                                              ResponseSink sink) override;
    [[nodiscard]] ProcedureStepResult feedExternalTyped(
        const ExternalData& data, ResponseSink sink = {}) override;
    [[nodiscard]] ProcedureStepResult tick(std::chrono::milliseconds delta) override;
    void cancel() noexcept override;

private:
    std::array<uint8_t, 16> mRandBuffer{};
    bool mHasRand{false};

    std::array<uint8_t, 4> mExpectedSRES{};
    bool mHasExpectedSRES{false};

    L3LocationAreaIdentity mLAI{};
    std::optional<uint32_t> mNewTmsi;
    MMRejectCause mRejectCause{MMRejectCause::Zero};

public:
    /// CRTP hooks called by ProcedureStateMixin base.
    void doTransitionTo(State s);
    void doFail(std::string_view reason);
    void doComplete();
};

} // namespace gsml3parser
