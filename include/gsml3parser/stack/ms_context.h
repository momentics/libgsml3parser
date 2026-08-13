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

/// Per-subscriber state context for GSM L3 protocol handling.
///
/// MSContext aggregates all state associated with a single mobile station:
/// identity (TMSI/IMSI), channel assignment, classmark, location area, and
/// protocol-layer flags (ciphering, registration, authentication).
///
/// This is the primary object through which a BTS tracks each subscriber.
/// Designed for high-load scenarios with millions of concurrent MS contexts.
///
/// Thread safety: NOT thread-safe. One instance per MS, accessed from a single thread.
/// Memory: sizeof(MSContext) <= 256 bytes, zero heap allocations.
/// Fields are ordered by access frequency for optimal cache behavior.
///
/// Example:
/// @code
///   auto ctx = MSContext::createWithTMSI(0x12345678);
///   ctx.assignChannel(ChannelType::SDCCHType, 0, 0, 125);
///   ctx.setCiphered(true);
/// @endcode
#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

#include "gsml3parser/types.h"
#include "gsml3parser/common/l3common.h"

namespace gsml3parser {

/// Per-subscriber state context. Holds identity, channel assignment,
/// classmark, and protocol-layer state for one mobile station.
///
/// 3GPP TS 24.008 — Mobility Management sublayer data.
///
/// Thread safety: NOT thread-safe. One instance per MS, accessed from a single thread.
/// Memory: sizeof(MSContext) <= 256 bytes, zero heap allocations.
/// Fields are ordered by access frequency for optimal cache behavior.
class MSContext {
public:
    /// Create a new context with an assigned TMSI.
    /// @param tmsi The 32-bit TMSI value.
    /// @return A new MSContext initialized with the given TMSI.
    ///
    /// Example:
    /// @code
    ///   auto ctx = MSContext::createWithTMSI(0x12345678);
    ///   assert(ctx.identity().isTMSI());
    ///   assert(ctx.identity().tmsi() == 0x12345678);
    /// @endcode
    [[nodiscard]] static MSContext createWithTMSI(uint32_t tmsi);

    /// Create a new context with an IMSI (BCD digit string).
    /// @param imsiDigits The IMSI digits as a character string (e.g. "244051234567890").
    /// @return A new MSContext initialized with the given IMSI.
    ///
    /// Example:
    /// @code
    ///   auto ctx = MSContext::createWithIMSI("244051234567890");
    ///   assert(ctx.identity().isIMSI());
    /// @endcode
    [[nodiscard]] static MSContext createWithIMSI(std::string_view imsiDigits);

    /// Get the primary identity of this MS.
    /// @return Reference to the stored L3MobileIdentity (TMSI or IMSI).
    const L3MobileIdentity& identity() const noexcept;

    /// Set or update the TMSI.
    /// @param tmsi The new 32-bit TMSI value.
    /// Replaces the current identity with a TMSI-based one.
    void setTMSI(uint32_t tmsi);

    /// Set or update the IMSI.
    /// @param digits The IMSI digits as a character string.
    /// Replaces the current identity with an IMSI-based one.
    void setIMSI(std::string_view digits);

    /// Current channel type assigned to this MS.
    /// @return The ChannelType, or ChannelType::UndefinedCHType if no channel assigned.
    ChannelType channelType() const noexcept;

    /// Assign a logical channel to this MS.
    /// @param type  The channel type (SDCCH, TCHF, TCHH, etc.).
    /// @param trx   Transceiver index (0-based).
    /// @param ts    TDMA timeslot number (0-15).
    /// @param arfcn Absolute Radio Frequency Channel Number.
    void assignChannel(ChannelType type, uint8_t trx, uint8_t ts, uint16_t arfcn);

    /// Release the current channel assignment.
    /// Resets channel type to UndefinedCHType and clears physical parameters.
    void releaseChannel() noexcept;

    /// Get transceiver number from current channel assignment.
    /// @return The TRX number, or 0 if no channel assigned.
    uint8_t trxNumber() const noexcept;

    /// Get timeslot from current channel assignment.
    /// @return The timeslot number, or 0 if no channel assigned.
    uint8_t timeslot() const noexcept;

    /// Get ARFCN from current channel assignment.
    /// @return The ARFCN value, or 0 if no channel assigned.
    uint16_t arfcn() const noexcept;

    /// Store MS classmark.
    /// @param cm The L3MobileStationClassmark1 to store.
    /// 3GPP TS 24.008 9.1.22 — Mobile Station Classmark 1.
    void setClassmark(const L3MobileStationClassmark1& cm);

    /// Get stored classmark.
    /// @return The classmark if previously set, std::nullopt otherwise.
    std::optional<L3MobileStationClassmark1> classmark() const noexcept;

    /// Location Area Identity known for this MS.
    /// @return The LAI if previously set, std::nullopt otherwise.
    /// 3GPP TS 24.008 10.5.1.3 — Location Area Identity.
    std::optional<L3LocationAreaIdentity> lai() const noexcept;

    /// Set LAI.
    /// @param lai The Location Area Identity to store.
    void setLAI(const L3LocationAreaIdentity& lai);

    /// Check ciphering state.
    /// @return True if ciphering is active for this MS.
    bool isCiphered() const noexcept;

    /// Set ciphering state.
    /// @param v True to enable ciphering, false to disable.
    void setCiphered(bool v) noexcept;

    /// Get timing advance value.
    /// @return The timing advance (0-63) if set, std::nullopt otherwise.
    /// 3GPP TS 24.008 10.5.2.40 — Timing Advance value.
    std::optional<uint8_t> timingAdvance() const noexcept;

    /// Set timing advance value.
    /// @param ta The timing advance value (0-63).
    void setTimingAdvance(uint8_t ta) noexcept;

    /// Check if context has been registered (location update completed).
    /// @return True if the MS has completed a location updating procedure.
    bool isRegistered() const noexcept;

    /// Set registration state.
    /// @param v True to mark as registered, false otherwise.
    void setRegistered(bool v) noexcept;

    /// Check if authentication has been performed.
    /// @return True if the MS has passed authentication.
    bool isAuthenticated() const noexcept;

    /// Set authentication state.
    /// @param v True to mark as authenticated, false otherwise.
    void setAuthenticated(bool v) noexcept;

private:
    MSContext() = default;

    // ── Hot fields (accessed on every message) ──────────────────────────
    L3MobileIdentity mIdentity{};
    ChannelType mChannelType{ChannelType::UndefinedCHType};
    uint8_t mTrxNumber{};
    uint8_t mTimeslot{};
    uint16_t mArfcn{};

    // ── Flags (packed for cache efficiency) ────────────────────────────
    bool mCiphered{false};
    bool mRegistered{false};
    bool mAuthenticated{false};
    bool mHasTimingAdvance{false};
    uint8_t mTimingAdvance{};

    // ── Warm fields (accessed during setup) ────────────────────────────
    L3MobileStationClassmark1 mClassmark{};
    bool mHasClassmark{false};

    // ── Cold fields (accessed during registration/handover) ────────────
    L3LocationAreaIdentity mLAI{};
    bool mHasLAI{false};
};

static_assert(sizeof(MSContext) <= 256, "MSContext too large for cache efficiency");

} // namespace gsml3parser
