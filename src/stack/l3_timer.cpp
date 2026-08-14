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

#include "gsml3parser/stack/l3_timer.h"

#include <chrono>
#include <utility>

namespace gsml3parser {

// ── Default timer durations (3GPP TS 24.008 / TS 44.018) ──────────────
// Indexed by sequential enum value: T3101=0, T3102=1, ..., T3395=18

namespace {
using namespace std::chrono_literals;

constexpr std::chrono::milliseconds kDefaultDurations[] = {
    3000ms,  // T3101  - CM service request retransmission
    3000ms,  // T3102  - Identity response retransmission
    5000ms,  // T3103  - Location updating request retransmission
    3000ms,  // T3106  - Authentication response retransmission
    3000ms,  // T3108  - TMSI reallocation complete retransmission
    30000ms, // T3109  - Paging response retransmission (etom * 5s, default 30s)
    3000ms,  // T3111  - CM reestablishment request retransmission
    3000ms,  // T3112  - IMSI detach indication retransmission
    3000ms,  // T3113  - MM status retransmission
    5000ms,  // T3310  - GPRS attach request retransmission
    30000ms, // T3311  - Routing area update retransmission (etor * 5s, default 30s)
    3000ms,  // T3312  - P-TMSI reallocation complete retransmission
    3000ms,  // T3314  - GPRS service request retransmission
    3000ms,  // T3315  - Authentication and ciphering resp retransmission
    3000ms,  // T3320  - Activate PDP context request retransmission
    3000ms,  // T3321  - Deactivate PDP context request retransmission
    3000ms,  // T3322  - Modify PDP context request retransmission
    3000ms,  // T3334  - GMM status retransmission
    3000ms   // T3395  - Packet reservation request retransmission
};

constexpr std::string_view kTimerNames[] = {
    "T3101", "T3102", "T3103", "T3106", "T3108",
    "T3109", "T3111", "T3112", "T3113",
    "T3310", "T3311", "T3312", "T3314", "T3315",
    "T3320", "T3321", "T3322", "T3334", "T3395"
};

constexpr size_t kNumTimers = sizeof(kTimerNames) / sizeof(kTimerNames[0]);

} // anonymous namespace

// ── Free functions ────────────────────────────────────────────────────

std::chrono::milliseconds l3TimerDefault(L3TimerId id) {
    uint8_t idx = static_cast<uint8_t>(id);
    if (idx < kNumTimers) {
        return kDefaultDurations[idx];
    }
    return std::chrono::milliseconds(3000); // default fallback for Unknown
}

std::string_view l3TimerName(L3TimerId id) {
    uint8_t idx = static_cast<uint8_t>(id);
    if (idx < kNumTimers) {
        return kTimerNames[idx];
    }
    return "Unknown";
}

// ── L3Timer implementation ────────────────────────────────────────────

L3Timer::L3Timer(L3TimerId id)
    : mId(id)
    , mExpiry(l3TimerDefault(id))
    , mRemaining(mExpiry)
{
}

L3Timer::L3Timer(L3TimerId id, std::chrono::milliseconds expiry)
    : mId(id)
    , mExpiry(std::move(expiry))
    , mRemaining(mExpiry)
{
}

bool L3Timer::start() {
    bool firstStart = !mRunning;
    mRunning = true;
    mRemaining = mExpiry;
    return firstStart;
}

void L3Timer::stop() noexcept {
    mRunning = false;
}

bool L3Timer::tick(std::chrono::milliseconds delta) {
    if (!mRunning) {
        return false;
    }
    if (mRemaining <= delta) {
        mRunning = false;
        mRemaining = std::chrono::milliseconds(0);
        return true;
    }
    mRemaining -= delta;
    return false;
}

bool L3Timer::isRunning() const noexcept {
    return mRunning;
}

std::chrono::milliseconds L3Timer::remaining() const noexcept {
    return mRunning ? mRemaining : std::chrono::milliseconds(0);
}

L3TimerId L3Timer::id() const noexcept {
    return mId;
}

std::chrono::milliseconds L3Timer::expiry() const noexcept {
    return mExpiry;
}

void L3Timer::reconfigure(L3TimerId id, std::chrono::milliseconds expiry) noexcept {
    mId = id;
    mExpiry = expiry;
    mRemaining = expiry;
    mRunning = false;
}

// ── TimerManager implementation ───────────────────────────────────────

bool TimerManager::start(L3TimerId id) {
    return start(id, l3TimerDefault(id));
}

bool TimerManager::start(L3TimerId id, std::chrono::milliseconds expiry) {
    size_t idx = index(id);
    if (idx >= MAX_TIMERS) {
        return false; // invalid timer ID (e.g. Unknown = 0xFF)
    }

    bool firstStart = !mInitialized[idx];
    if (!mInitialized[idx]) {
        mTimers[idx].reconfigure(id, expiry);
        mInitialized[idx] = true;
    } else {
        mTimers[idx].reconfigure(id, expiry);
    }
    mTimers[idx].start();
    return firstStart;
}

void TimerManager::stop(L3TimerId id) noexcept {
    size_t idx = index(id);
    if (idx < MAX_TIMERS && mInitialized[idx]) {
        mTimers[idx].stop();
    }
}

void TimerManager::stopAll() noexcept {
    for (size_t i = 0; i < MAX_TIMERS; ++i) {
        if (mInitialized[i]) {
            mTimers[i].stop();
        }
    }
}

size_t TimerManager::tick(std::chrono::milliseconds delta, std::span<L3TimerId> out) {
    size_t count = 0;
    for (size_t i = 0; i < MAX_TIMERS; ++i) {
        if (mInitialized[i] && mTimers[i].isRunning()) {
            if (mTimers[i].tick(delta)) {
                if (count < out.size()) {
                    out[count] = mTimers[i].id();
                }
                ++count;
            }
        }
    }
    return count;
}

bool TimerManager::isRunning(L3TimerId id) const noexcept {
    size_t idx = index(id);
    if (idx < MAX_TIMERS && mInitialized[idx]) {
        return mTimers[idx].isRunning();
    }
    return false;
}

std::chrono::milliseconds TimerManager::remaining(L3TimerId id) const noexcept {
    size_t idx = index(id);
    if (idx < MAX_TIMERS && mInitialized[idx]) {
        return mTimers[idx].remaining();
    }
    return std::chrono::milliseconds(0);
}

const L3Timer* TimerManager::get(L3TimerId id) const noexcept {
    size_t idx = index(id);
    if (idx < MAX_TIMERS && mInitialized[idx]) {
        return &mTimers[idx];
    }
    return nullptr;
}

size_t TimerManager::runningCount() const noexcept {
    size_t count = 0;
    for (size_t i = 0; i < MAX_TIMERS; ++i) {
        if (mInitialized[i] && mTimers[i].isRunning()) {
            ++count;
        }
    }
    return count;
}

} // namespace gsml3parser
