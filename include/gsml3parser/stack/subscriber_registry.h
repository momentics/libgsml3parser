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

/// Subscriber session and registry management for software BTS implementations.
///
/// Provides SubscriberSession (aggregates MSContext, FSMs, timers, transactions,
/// and ProcedureRunner per mobile station) and SubscriberRegistry (manages multiple
/// sessions with TMSI, IMSI, and LAPDm link indexes). For high-concurrency scenarios,
/// ShardedSubscriberRegistry partitions sessions across independent shards.
///
/// 3GPP TS 24.008 - Mobility Management subscriber data model.
/// Thread safety: SubscriberRegistry is NOT thread-safe (single event-loop access).
/// ShardedSubscriberRegistry is thread-safe via per-shard shared_mutex.
/// Memory: sizeof(SubscriberSession) < 4096 bytes, all components stored inline.
///
/// Example:
/// @code
///   SubscriberRegistry registry;
///   auto* session = registry.createByTMSI(0x12345678);
///   session->context.setRegistered(true);
///   auto* found = registry.findByTMSI(0x12345678);
///   registry.remove(found);
/// @endcode
#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <shared_mutex>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>

#include "gsml3parser/stack/ms_context.h"
#include "gsml3parser/stack/state_machine.h"
#include "gsml3parser/stack/l3_timer.h"
#include "gsml3parser/stack/transaction.h"
#include "gsml3parser/stack/procedure_runner.h"
#include "gsml3parser/stack/channel_pool.h"

namespace gsml3parser {

/// Active session context for a single subscriber. Aggregates MSContext, FSMs,
/// timers, transactions and ProcedureRunner into one object for convenient
/// lifecycle management. All components stored inline (no unique_ptr) to
/// minimize cache misses during session traversal.
///
/// 3GPP TS 24.008 - Per-MS state aggregation.
/// Memory: sizeof(SubscriberSession) < 4096 bytes.
class SubscriberSession {
public:
    MSContext context;
    RRStateMachine rrSM;
    MMStateMachine mmSM;
    CCStateMachine ccSM;
    TimerManager timers;
    TransactionManager transactions;
    ProcedureRunner procedures;

    std::optional<ChannelDescriptor> channel;
    uint8_t lapdmLink{0};

    SubscriberSession() = default;
};

static_assert(sizeof(SubscriberSession) < 4096, "SubscriberSession too large");

/// BTS subscriber registry. Manages multiple SubscriberSession instances and provides
/// TMSI, IMSI and LAPDm link lookup indexes. Analogous to MMUserMap in OpenBTS.
///
/// For million-subscriber scenarios use ShardedSubscriberRegistry which partitions
/// sessions into shards with independent mutexes.
///
/// 3GPP TS 24.008 - Subscriber data management.
/// Thread safety: NOT thread-safe. One instance per BTS, single event-loop access.
/// Memory: std::unordered_map indexes. For >100K sessions prefer ShardedSubscriberRegistry.
class SubscriberRegistry {
public:
    SubscriberRegistry() = default;

    /// Create a new session for a subscriber identified by TMSI.
    /// @param tmsi 32-bit TMSI identifier.
    /// @return Pointer to created session, or nullptr if TMSI already exists.
    SubscriberSession* createByTMSI(uint32_t tmsi);

    /// Create a new session for a subscriber identified by IMSI.
    /// @param imsi BCD digit string of IMSI (e.g. "244051234567890").
    /// @return Pointer to created session, or nullptr if IMSI already exists.
    SubscriberSession* createByIMSI(std::string_view imsi);

    /// Find session by TMSI.
    /// @param tmsi 32-bit TMSI to look up.
    /// @return Pointer to session, or nullptr if not found.
    [[nodiscard]] SubscriberSession* findByTMSI(uint32_t tmsi) noexcept;
    [[nodiscard]] const SubscriberSession* findByTMSI(uint32_t tmsi) const noexcept;

    /// Find session by IMSI.
    /// @param imsi BCD digit string of IMSI.
    /// @return Pointer to session, or nullptr if not found.
    [[nodiscard]] SubscriberSession* findByIMSI(std::string_view imsi) noexcept;
    [[nodiscard]] const SubscriberSession* findByIMSI(std::string_view imsi) const noexcept;

    /// Find session by LAPDm link (trx, timeslot, link).
    /// Used for routing incoming radio messages to the correct subscriber.
    /// @param trx Transceiver index.
    /// @param ts TDMA timeslot number.
    /// @param lapdmLink LAPDm link identifier.
    /// @return Pointer to session, or nullptr if not found.
    [[nodiscard]] SubscriberSession* findByLink(uint8_t trx, uint8_t ts, uint8_t lapdmLink) noexcept;

    /// Assign a channel to a session and update the link index.
    /// @param session Session to assign the channel to.
    /// @param desc Channel descriptor.
    /// @param lapdmLink LAPDm link identifier for routing.
    void assignChannel(SubscriberSession* session, ChannelDescriptor desc,
                       uint8_t lapdmLink) noexcept;

    /// Release a channel from a session and remove from the link index.
    /// @param session Session to release the channel from.
    void releaseChannel(SubscriberSession* session) noexcept;

    /// Remove a subscriber session (detach, channel release).
    /// @param session Session pointer to remove.
    /// @return true if session was found and removed.
    bool remove(SubscriberSession* session) noexcept;

    /// Remove all sessions (emergency shutdown).
    void clear() noexcept;

    /// Number of active sessions.
    /// @return Count of currently tracked sessions.
    [[nodiscard]] size_t count() const noexcept;

    /// Iterate over all unique active sessions (for timer tick, periodic tasks).
    /// Guarantees each session is visited exactly once.
    template<typename F>
    void forEach(F&& callback) {
        for (auto& [key, entry] : mByTMSI) {
            if (entry.active) callback(entry.session);
        }
    }

    /// Tick timers of all sessions. Fills pre-allocated buffer with expired timer IDs.
    /// @param delta Time advance in milliseconds.
    /// @param expiredOut Pre-allocated span for expired L3TimerId entries.
    /// @return Number of expired timer entries written.
    size_t tickAllTimers(std::chrono::milliseconds delta,
                         std::span<L3TimerId> expiredOut);

private:
    struct SessionEntry {
        SessionEntry() = default;
        SubscriberSession session;
        bool active{true};
    };

    // TMSI -> session (primary index)
    std::unordered_map<uint32_t, SessionEntry> mByTMSI;

    // IMSI -> TMSI (secondary index: redirects to mByTMSI)
    std::unordered_map<std::string, uint32_t> mByIMSI;

    // LAPDm link key (trx:8 | ts:8 | lapdmLink:8) -> session pointer
    std::unordered_map<uint32_t, SubscriberSession*> mByLink;
};

/// Thread-safe, high-concurrency subscriber registry.
///
/// Partitions sessions into N shards, each with its own std::shared_mutex.
/// Operations on different shards execute in parallel without contention.
/// Uses same hash pattern as ShardedChannelPool: hash(TMSI) & (N-1).
///
/// 3GPP TS 24.008 - Scalable subscriber data management.
/// Thread safety: thread-safe via per-shard shared_mutex.
/// Memory: N independent SubscriberRegistry instances + mutex overhead.
template<int N = 16>
class ShardedSubscriberRegistry {
    static_assert((N & (N - 1)) == 0, "N must be a power of two");
    static_assert(N >= 2, "N must be at least 2");

    struct Shard {
        SubscriberRegistry registry;
        mutable std::shared_mutex mutex;
    };

    std::array<Shard, N> mShards{};

public:
    ShardedSubscriberRegistry() = default;

    /// Create session by TMSI. Thread-safe. O(1) hash + per-shard lock.
    /// @param tmsi 32-bit TMSI identifier.
    /// @return Pointer to created session, or nullptr if duplicate.
    SubscriberSession* createByTMSI(uint32_t tmsi);

    /// Find session by TMSI. Thread-safe. Uses shared lock.
    /// @param tmsi 32-bit TMSI to look up.
    /// @return Pointer to session, or nullptr if not found.
    [[nodiscard]] SubscriberSession* findByTMSI(uint32_t tmsi) noexcept;

    /// Find session by IMSI. Thread-safe. Scans all shards (cold path).
    /// @param imsi BCD digit string of IMSI.
    /// @return Pointer to session, or nullptr if not found.
    [[nodiscard]] SubscriberSession* findByIMSI(std::string_view imsi) noexcept;

    /// Find session by link. Thread-safe. Scans all shards (cold path).
    /// @param trx Transceiver index.
    /// @param ts TDMA timeslot number.
    /// @param lapdmLink LAPDm link identifier.
    /// @return Pointer to session, or nullptr if not found.
    [[nodiscard]] SubscriberSession* findByLink(uint8_t trx, uint8_t ts, uint8_t lapdmLink) noexcept;

    /// Remove session. Thread-safe. O(1) hash + per-shard lock.
    /// @param session Session pointer to remove.
    /// @return true if session was found and removed.
    bool remove(SubscriberSession* session) noexcept;

    /// Tick all timers across all shards. Thread-safe. Each shard ticked independently.
    /// @param delta Time advance in milliseconds.
    /// @param expiredOut Pre-allocated span for expired timer IDs.
    /// @return Total expired timers across all shards.
    size_t tickAllTimers(std::chrono::milliseconds delta,
                         std::span<L3TimerId> expiredOut);

    /// Iterate all sessions across all shards. Thread-safe (shared locks).
    template<typename F>
    void forEach(F&& callback) {
        for (auto& shard : mShards) {
            std::shared_lock lock(shard.mutex);
            shard.registry.forEach(std::forward<F>(callback));
        }
    }

private:
    /// Simple mix for uint32_t — sufficient for shard distribution.
    static constexpr uint32_t hashTMSI(uint32_t tmsi) noexcept {
        uint32_t h = tmsi ^ (tmsi >> 16);
        h ^= h >> 8;
        return h;
    }

    /// Select shard index from hash.
    static constexpr int shardIndex(uint32_t hash) noexcept {
        return static_cast<int>(hash & static_cast<uint32_t>(N - 1));
    }
};

// Explicit template instantiations are in subscriber_registry.cpp.

} // namespace gsml3parser
