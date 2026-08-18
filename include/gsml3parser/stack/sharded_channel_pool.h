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

/// Thread-safe sharded channel pool for high-concurrency scenarios.
///
/// Partitions channels across N shards using a hash of the channel descriptor.
/// Each shard has its own std::shared_mutex, so threads operating on different
/// shards never contend.  This allows near-linear scaling with thread count
/// when the hash distributes work evenly.
///
/// Thread safety: all public methods are thread-safe.  Multiple threads can
/// concurrently call allocate(), release(), addChannel() etc.  Read-only
/// operations (freeCount, totalCount) use shared locks for better concurrency.
///
/// Performance: O(1) hash + per-shard lock.  Contention only occurs when two
/// threads hash to the same shard.  With N=16 or N=32 and millions of channels,
/// expected contention is negligible.
#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <shared_mutex>

#include "gsml3parser/types.h"
#include "gsml3parser/stack/channel_pool.h"

namespace gsml3parser {

template<int N = 16>
class ShardedChannelPool {
    static_assert((N & (N - 1)) == 0, "N must be a power of two");
    static_assert(N >= 2, "N must be at least 2");

    struct Shard {
        ChannelPool pool;
        mutable std::shared_mutex mutex;
    };

    std::array<Shard, N> mShards{};

    // Round-robin starting shard for allocate(): spreads lock contention across
    // shards instead of always probing shard 0 first.
    std::atomic<uint32_t> mRrCounter{0};

public:
    ShardedChannelPool() = default;

    /// Add a channel to the pool. Thread-safe.
    /// @param desc The channel descriptor to add.
    void addChannel(ChannelDescriptor desc);

    /// Allocate a channel of the requested type from any shard that has one.
    /// Starts probing from a round-robin shard to spread lock contention across
    /// shards, then falls back to the remaining shards (channels are
    /// hash-distributed at addChannel time, so a free one may be in any shard).
    /// Thread-safe. Performance: O(N) worst case (a free channel may reside in
    /// any shard); typically terminates on the first or second shard.
    /// @param type The channel type to allocate.
    /// @return A ChannelDescriptor if found, std::nullopt otherwise.
    [[nodiscard]] std::optional<ChannelDescriptor> allocate(ChannelType type);

    /// Release a channel back to the pool. Thread-safe.
    /// @param desc The channel descriptor to release.
    /// @return True if the channel was known and released.
    bool release(const ChannelDescriptor& desc);

    /// Total free channels of a given type across all shards.
    /// Thread-safe. Uses shared locks. O(N).
    [[nodiscard]] size_t freeCount(ChannelType type) const;

    /// Total number of channels (free + allocated) across all shards.
    /// Thread-safe. Uses shared locks. O(N).
    [[nodiscard]] size_t totalCount() const;

private:
    /// Hash a channel descriptor to a shard index. O(1), bitmask.
    static constexpr size_t hashDescriptor(const ChannelDescriptor& desc) noexcept {
        size_t h = static_cast<size_t>(desc.trxNumber) << 16;
        h |= static_cast<size_t>(desc.timeslot) << 8;
        h |= desc.arfcn & 0xFF;
        h ^= h >> 12;
        h ^= h >> 25;
        return h;
    }

    static constexpr int shardIndex(size_t hash) noexcept {
        return static_cast<int>(hash & static_cast<size_t>(N - 1));
    }
};

// ── Inline method definitions ──────────────────────────────────────────

template<int N>
inline void ShardedChannelPool<N>::addChannel(ChannelDescriptor desc) {
    size_t h = hashDescriptor(desc);
    int idx = shardIndex(h);
    std::unique_lock lock(mShards[idx].mutex);
    mShards[idx].pool.addChannel(std::move(desc));
}

template<int N>
inline std::optional<ChannelDescriptor> ShardedChannelPool<N>::allocate(ChannelType type) {
    // Start from a round-robin shard to spread lock contention, then probe the
    // remaining shards (channels are hash-distributed, so a free one may be in
    // any shard). N is a power of two, so the mask replaces modulo.
    const uint32_t start = mRrCounter.fetch_add(1, std::memory_order_relaxed) & static_cast<uint32_t>(N - 1);
    for (uint32_t offset = 0; offset < N; ++offset) {
        const int idx = static_cast<int>((start + offset) & static_cast<uint32_t>(N - 1));
        std::unique_lock lock(mShards[idx].mutex);
        auto ch = mShards[idx].pool.allocate(type);
        if (ch) return ch;
    }
    return std::nullopt;
}

template<int N>
inline bool ShardedChannelPool<N>::release(const ChannelDescriptor& desc) {
    size_t h = hashDescriptor(desc);
    int idx = shardIndex(h);
    std::unique_lock lock(mShards[idx].mutex);
    return mShards[idx].pool.release(desc);
}

template<int N>
inline size_t ShardedChannelPool<N>::freeCount(ChannelType type) const {
    size_t total = 0;
    for (int i = 0; i < N; ++i) {
        std::shared_lock lock(mShards[i].mutex);
        total += mShards[i].pool.freeCount(type);
    }
    return total;
}

template<int N>
inline size_t ShardedChannelPool<N>::totalCount() const {
    size_t total = 0;
    for (int i = 0; i < N; ++i) {
        std::shared_lock lock(mShards[i].mutex);
        total += mShards[i].pool.totalCount();
    }
    return total;
}

// ── Explicit template instantiations for common shard counts ───────────
template class ShardedChannelPool<4>;
template class ShardedChannelPool<8>;
template class ShardedChannelPool<16>;
template class ShardedChannelPool<32>;

} // namespace gsml3parser
