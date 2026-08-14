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

/// GSM Layer 3 timer framework for protocol timers (T3101, T3102, etc.).
///
/// Provides timer identifiers from 3GPP TS 24.008 / TS 44.018, a single-timer
/// class with start/stop/tick semantics, and a TimerManager that tracks up to
/// 32 concurrent timers per MS using fixed-size arrays (zero heap allocation).
///
/// Thread safety: NOT thread-safe. One instance per MS, accessed from a single thread.
/// Performance: tick() avoids heap allocation via callback or pre-allocated span.
/// Internal storage is std::array — no dynamic allocation.
///
/// Example:
/// @code
///   TimerManager tm;
///   tm.start(L3TimerId::T3101);
///   // ... later, in event loop:
///   std::array<L3TimerId, 32> expired;
///   size_t n = tm.tick(std::chrono::milliseconds(500), expired);
///   for (size_t i = 0; i < n; ++i) {
///       handleExpired(expired[i]);
///   }
/// @endcode
#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace gsml3parser {

/// GSM Layer 3 timer identifiers (3GPP TS 24.008 / TS 44.018).
/// Each timer corresponds to a protocol retransmission or timeout period.
enum class L3TimerId : uint8_t {
    T3101 = 0,   /// CM service request retransmission (3s) — 3GPP TS 24.008 10.5.4
    T3102,       /// Identity response retransmission (3s) — 3GPP TS 24.008 10.5.6
    T3103,       /// Location updating request retransmission (5s) — 3GPP TS 24.008 10.5.12
    T3106,       /// Authentication response retransmission (3s) — 3GPP TS 24.008 10.5.18
    T3108,       /// TMSI reallocation complete retransmission (3s) — 3GPP TS 24.008 10.5.23
    T3109,       /// Paging response retransmission (etom * 5s) — 3GPP TS 24.008 10.5.26
    T3111,       /// CM reestablishment request retransmission (3s) — 3GPP TS 24.008 10.5.34
    T3112,       /// IMSI detach indication retransmission (3s) — 3GPP TS 24.008 10.5.36
    T3113,       /// MM status retransmission (3s) — 3GPP TS 24.008 10.5.38
    T3310,       /// GPRS attach request retransmission (5s) — 3GPP TS 24.008 10.5.76
    T3311,       /// Routing area update retransmission (etor * 5s) — 3GPP TS 24.008 10.5.78
    T3312,       /// P-TMSI reallocation complete retransmission (3s) — 3GPP TS 24.008 10.5.80
    T3314,       /// GPRS service request retransmission (3s) — 3GPP TS 24.008 10.5.84
    T3315,       /// Authentication and ciphering resp retransmission (3s) — 3GPP TS 24.008 10.5.86
    T3320,       /// Activate PDP context request retransmission (3s) — 3GPP TS 24.008 10.5.96
    T3321,       /// Deactivate PDP context request retransmission (3s) — 3GPP TS 24.008 10.5.98
    T3322,       /// Modify PDP context request retransmission (3s) — 3GPP TS 24.008 10.5.100
    T3334,       /// GMM status retransmission (3s) — 3GPP TS 24.008 10.5.112
    T3395,       /// Packet reservation request retransmission (3s) — 3GPP TS 24.008 10.5.130
    Unknown = 0xFF
};

/// Returns the default duration for a given L3 timer ID.
/// Values from 3GPP TS 24.008 / TS 44.018 specifications.
/// @param id The timer identifier.
/// @return Default duration in milliseconds, or 3000ms for Unknown.
/// Performance: O(1) constexpr array lookup.
[[nodiscard]] std::chrono::milliseconds l3TimerDefault(L3TimerId id);

/// Returns a human-readable name for a timer ID.
/// @param id The timer identifier.
/// @return String view with the timer name (e.g. "T3101"), or "Unknown".
/// Performance: O(1) constexpr array lookup.
[[nodiscard]] std::string_view l3TimerName(L3TimerId id);

/// Single timer instance with start/stop/expired semantics.
/// Uses std::chrono::steady_clock for monotonic time measurement.
///
/// 3GPP TS 24.008 — Protocol timer definitions.
///
/// Memory: No heap allocations. All state stored inline (~24 bytes).
class L3Timer {
public:
    /// Default constructor. Creates a timer with Unknown ID and zero expiry.
    /// Used internally by TimerManager for array storage. Not intended for direct use.
    L3Timer() = default;

    /// Create a timer with the given ID and default expiry duration.
    /// @param id The timer identifier (e.g. T3101).
    /// The timer starts in the stopped state; call start() to activate.
    explicit L3Timer(L3TimerId id);

    /// Create a timer with a custom expiry duration.
    /// @param id The timer identifier.
    /// @param expiry Custom expiry duration in milliseconds.
    /// The timer starts in the stopped state; call start() to activate.
    L3Timer(L3TimerId id, std::chrono::milliseconds expiry);

    /// Start (or restart) the timer.
    /// @return True if this is the first start (timer was not running).
    ///         False if the timer was already running (restart).
    /// Captures the current steady_clock time and schedules expiry.
    bool start();

    /// Stop the timer without firing.
    /// Clears the running state; the timer will not expire on subsequent ticks.
    void stop() noexcept;

    /// Advance the timer by `delta`. Call this periodically from your event loop.
    /// @param delta Time elapsed since the last tick.
    /// @return True if the timer expired during this advance.
    ///         False if the timer is still running or was not started.
    bool tick(std::chrono::milliseconds delta);

    /// Returns true if the timer is currently running.
    /// @return True if start() was called and the timer has not yet expired or been stopped.
    [[nodiscard]] bool isRunning() const noexcept;

    /// Returns remaining time before expiry.
    /// @return Remaining milliseconds, or zero if not running or already expired.
    [[nodiscard]] std::chrono::milliseconds remaining() const noexcept;

    /// Returns the timer's ID.
    /// @return The L3TimerId this timer represents.
    [[nodiscard]] L3TimerId id() const noexcept;

    /// Returns the configured expiry duration.
    /// @return The total expiry period in milliseconds.
    [[nodiscard]] std::chrono::milliseconds expiry() const noexcept;

    /// Reconfigure this timer with a new ID and expiry.
    /// Used by TimerManager to initialize default-constructed timer slots.
    /// Stops the timer if it was running.
    /// @param id New timer identifier.
    /// @param expiry New expiry duration in milliseconds.
    void reconfigure(L3TimerId id, std::chrono::milliseconds expiry) noexcept;

private:
    L3TimerId mId{L3TimerId::Unknown};
    std::chrono::milliseconds mExpiry{0};
    std::chrono::milliseconds mRemaining{0};
    bool mRunning{false};
};

/// Manages a set of named timers for one MS context.
///
/// Uses fixed-size arrays indexed by timer enum value — no heap allocation.
/// Maximum 32 concurrent timers per MS (sufficient for all defined L3TimerId values).
///
/// 3GPP TS 24.008 — Multiple protocol timers may run concurrently per MS.
///
/// Thread safety: NOT thread-safe. One instance per MS, accessed from a single thread.
/// Performance: tick() uses callback or span to avoid heap allocation on the hot path.
/// Internal storage is std::array (no dynamic allocation).
class TimerManager {
public:
    TimerManager() = default;

    /// Start a timer with its default expiry duration.
    /// @param id The timer identifier to start.
    /// @return True if this is the first start (not a restart of an already running timer).
    ///         False if the timer was already running and has been restarted.
    bool start(L3TimerId id);

    /// Start a timer with a custom expiry duration.
    /// @param id The timer identifier to start.
    /// @param expiry Custom expiry duration in milliseconds.
    /// @return True if this is the first start (not a restart).
    bool start(L3TimerId id, std::chrono::milliseconds expiry);

    /// Stop a specific timer.
    /// @param id The timer identifier to stop.
    /// No-op if the timer is not running.
    void stop(L3TimerId id) noexcept;

    /// Stop all running timers.
    /// Clears all timer state; equivalent to calling stop() for every timer ID.
    void stopAll() noexcept;

    /// Advance all timers by `delta`. For each expired timer, invokes the callback.
    /// @param delta Time elapsed since the last tick.
    /// @param onExpired Callback invoked for each expired timer ID.
    ///                  The callback receives the L3TimerId of the expired timer.
    /// This overload avoids heap allocation on the hot path (called per-message
    /// or per-event-loop-tick). The callback is responsible for handling expiry.
    template<typename Callback>
    void tick(std::chrono::milliseconds delta, Callback&& onExpired) {
        for (size_t i = 0; i < MAX_TIMERS; ++i) {
            if (mInitialized[i] && mTimers[i].isRunning()) {
                L3TimerId tid = mTimers[i].id();
                if (mTimers[i].tick(delta)) {
                    std::forward<Callback>(onExpired)(tid);
                }
            }
        }
    }

    /// Advance all timers by `delta`. Fills the pre-allocated output buffer with expired IDs.
    /// @param delta Time elapsed since the last tick.
    /// @param out Pre-allocated span to receive expired timer IDs.
    ///            The caller must ensure sufficient capacity (max ~32 entries).
    /// @return The number of expired timer IDs written to `out`.
    /// This overload avoids heap allocation by using a caller-provided buffer.
    size_t tick(std::chrono::milliseconds delta, std::span<L3TimerId> out);

    /// Check if a specific timer is currently running.
    /// @param id The timer identifier to check.
    /// @return True if the timer was started and has not yet expired or been stopped.
    [[nodiscard]] bool isRunning(L3TimerId id) const noexcept;

    /// Get remaining time for a specific timer.
    /// @param id The timer identifier.
    /// @return Remaining milliseconds, or zero if not running.
    [[nodiscard]] std::chrono::milliseconds remaining(L3TimerId id) const noexcept;

    /// Get the timer object for direct access.
    /// @param id The timer identifier.
    /// @return Pointer to the L3Timer if it has been configured, nullptr otherwise.
    [[nodiscard]] const L3Timer* get(L3TimerId id) const noexcept;

    /// Number of currently running timers.
    /// @return Count of active (running) timers.
    [[nodiscard]] size_t runningCount() const noexcept;

private:
    static constexpr size_t MAX_TIMERS = 32;

    // Fixed-size array — no heap allocation. Index by enum value.
    std::array<L3Timer, MAX_TIMERS> mTimers{};

    // Tracks which timer slots have been initialized (configured with a real ID).
    std::array<bool, MAX_TIMERS> mInitialized{};

    [[nodiscard]] size_t index(L3TimerId id) const noexcept {
        return static_cast<size_t>(static_cast<uint8_t>(id));
    }
};

} // namespace gsml3parser
