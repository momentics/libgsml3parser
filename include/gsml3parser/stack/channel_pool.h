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

/// Logical channel pool management and Very Early Assignment (VEA) support.
///
/// Provides channel allocation/release with O(1) per-type free-list lookup,
/// Request Reference (RA) decoding from RACH bursts, and VEA fallback logic.
/// Used by BTS to manage SDCCH, TCHF, TCHH and other logical channels.
///
/// Thread safety: NOT thread-safe. For multi-threaded access, the caller must
/// provide external synchronization (e.g., one ChannelPool per BTS instance,
/// protected by the event loop).
/// Performance: allocate() is O(1) via per-type free-list. Internal storage
/// uses std::unordered_map + std::vector but only grows during channel
/// registration (not on hot path). addChannel() and removeChannel() are cold-path.
///
/// Example:
/// @code
///   ChannelPool pool;
///   pool.addChannel({ChannelType::SDCCHType, 0, 0, 100});
///   pool.addChannel({ChannelType::TCHFType, 1, 1, 101});
///   auto ch = pool.allocate(ChannelType::SDCCHType);
///   // ... use channel ...
///   pool.release(*ch);
/// @endcode
#pragma once

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

#include "gsml3parser/types.h"

namespace gsml3parser {

/// Describes an allocated logical channel.
/// Contains channel type, transceiver index, TDMA timeslot, and ARFCN.
///
/// 3GPP TS 04.08 10.5.2.5 — Channel Description element.
struct ChannelDescriptor {
    ChannelType type{ChannelType::UndefinedCHType};
    uint8_t trxNumber{};
    uint8_t timeslot{};
    uint16_t arfcn{};

    bool operator==(const ChannelDescriptor&) const = default;
};

/// Decodes Request Reference (RA) from a RACH burst into the needed
/// channel type, following GSM 04.08 Table 9.9.
///
/// The RA byte encodes the establishment cause and access category.
/// Establishment cause bits (6-5) determine the channel type:
///   00 — Mobile Originating call → TCH (with VEA) or SDCCH (without VEA)
///   01 — Emergency call → TCH always
///   10 — Answer to Paging → TCH
///   11 — Location Updating → SDCCH
///
/// @param ra The 8-bit RA value from the Channel Request message.
/// @param neci Non-Extended Channel Indicator (0 = legacy, 1 = NECI extended).
///             When NECI is set, additional channel type information may be encoded.
/// @param vea Very Early Assignment enabled. When true, MO calls can be assigned
///            directly to TCH without an intermediate SDCCH assignment.
/// @return Required channel type, or UndefinedCHType if RA value is unrecognized.
/// Performance: O(1) bit operations, no heap allocation.
[[nodiscard]] ChannelType decodeChannelNeeded(uint8_t ra, bool neci = false, bool vea = false);

/// Returns true if the RA indicates a Location Updating Request.
///
/// Checks establishment cause bits (6-5) for value 11 (Location Updating).
///
/// @param ra The 8-bit RA value from the Channel Request message.
/// @param neci Non-Extended Channel Indicator (reserved for future extended decoding).
/// @return True if the establishment cause indicates a location updating request.
/// Performance: O(1) bit operations, no heap allocation.
[[nodiscard]] bool isLocationUpdatingRequest(uint8_t ra, bool neci = false);

/// Manages a pool of logical channels and handles allocation/release.
///
/// Channels are grouped by type in per-type free-lists for O(1) allocation.
/// During BTS initialization, addChannel() registers available channels.
/// At runtime, allocate() pops from the front of the appropriate free-list,
/// and release() pushes channels back to the list.
///
/// 3GPP TS 04.08 — Radio Resource Management channel assignment procedures.
///
/// Thread safety: NOT thread-safe. For multi-threaded access, the caller must
/// provide external synchronization (e.g., one ChannelPool per BTS instance,
/// protected by the event loop).
/// Performance: allocate() is O(1) via per-type free-list pop_front.
/// Internal storage uses std::unordered_map + std::vector but only grows
/// during channel registration (not on hot path).
class ChannelPool {
public:
    ChannelPool() = default;

    /// Add a channel to the available pool. Called during BTS initialization.
    /// @param desc The channel descriptor describing the new channel.
    /// Channels added this way are immediately available for allocation.
    /// If a duplicate channel is added, it will be tracked separately.
    void addChannel(ChannelDescriptor desc);

    /// Remove a channel from the pool (permanently unavailable).
    /// @param desc The channel descriptor to remove.
    /// @return True if the channel was found and removed, false otherwise.
    /// If the channel was allocated, it is simply forgotten; if free, it is removed from the free-list.
    bool removeChannel(const ChannelDescriptor& desc);

    /// Allocate a channel of the requested type. Returns nullopt if none available.
    /// @param type The channel type to allocate (SDCCH, TCHF, TCHH, etc.).
    /// @return A ChannelDescriptor if a free channel was found, std::nullopt otherwise.
    /// O(1) via per-type free-list pop from back of vector.
    /// No heap allocation on the hot path (vector shrink does not reallocate for single pop).
    [[nodiscard]] std::optional<ChannelDescriptor> allocate(ChannelType type);

    /// Release a previously allocated channel back to the pool.
    /// @param desc The channel descriptor to release.
    /// @return True if the channel was known to this pool and released, false otherwise.
    /// Restores the channel to the free-list for its type.
    bool release(const ChannelDescriptor& desc);

    /// Check if a specific channel is currently free (available for allocation).
    /// @param desc The channel descriptor to check.
    /// @return True if the channel is in the free-list for its type.
    /// Performance: O(N) where N is the number of free channels of that type.
    /// This is a cold-path diagnostic operation, not used on hot paths.
    [[nodiscard]] bool isFree(const ChannelDescriptor& desc) const;

    /// Number of free channels of a given type.
    /// @param type The channel type to query.
    /// @return Count of available (unallocated) channels of the specified type.
    /// O(1) via vector::size() on the free-list.
    [[nodiscard]] size_t freeCount(ChannelType type) const;

    /// Total number of channels registered in the pool (free + allocated).
    /// @return Sum of all free and allocated channels across all types.
    /// Performance: O(T) where T is the number of distinct channel types tracked.
    /// This is a cold-path diagnostic operation.
    [[nodiscard]] size_t totalCount() const;

    /// Perform Very Early Assignment: try TCH first, fall back to SDCCH.
    /// @param ra The Request Reference value from the Channel Request message.
    /// @return A ChannelDescriptor if allocation succeeded, std::nullopt otherwise.
    /// VEA attempts to allocate a full-rate TCH (TCHFType) directly for MO calls,
    /// skipping the intermediate SDCCH assignment. If no TCH is available, falls
    /// back to SDCCH. The channel type needed is decoded from the RA value.
    /// 3GPP TS 05.08 — Very Early Assignment procedure.
    [[nodiscard]] std::optional<ChannelDescriptor> allocateVEA(uint8_t ra);

    /// Get all free channels of a given type. Returns by value (called rarely).
    /// @param type The channel type to query.
    /// @return Vector of all currently free ChannelDescriptors of the specified type.
    /// This is a cold-path diagnostic operation that performs a heap allocation.
    /// Not intended for use on hot paths.
    [[nodiscard]] std::vector<ChannelDescriptor> freeChannels(ChannelType type) const;

    /// Number of currently allocated (in-use) channels of a given type.
    /// @param type The channel type to query.
    /// @return Count of allocated channels that have not been released.
    /// O(1) via vector::size() on the allocated list.
    [[nodiscard]] size_t allocatedCount(ChannelType type) const;

private:
    /// Check if a channel descriptor is known to this pool (free or allocated).
    bool knowsChannel(const ChannelDescriptor& desc) const;

    /// Find and remove a channel from a free-list by type.
    bool removeFromFreeList(ChannelType type, const ChannelDescriptor& desc);

    // Per-type bucket of free channel descriptors. Indexed by ChannelType enum value.
    // allocate() pops from the back of the vector — O(1).
    std::unordered_map<ChannelType, std::vector<ChannelDescriptor>> mFreeByType;

    // Per-type bucket of allocated (in-use) channel descriptors.
    // Used for tracking total count and release validation.
    std::unordered_map<ChannelType, std::vector<ChannelDescriptor>> mAllocatedByType;
};

} // namespace gsml3parser
