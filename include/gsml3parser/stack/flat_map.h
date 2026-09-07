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

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace gsml3parser {

/// Open-addressing flat hash map with linear probing (registry scale,
/// audit: tens of millions of concurrent streams).
///
/// Stores key/value pairs in a slot index plus individually allocated
/// entry blocks: no pointer chasing on lookup, no per-node allocator
/// fragmentation.
///
/// ADDRESS STABILITY (audit P0-1): every entry lives in its own heap
/// block (std::unique_ptr<Entry>), so the address of an entry is stable
/// for the entry's entire lifetime — insertions, erasures of OTHER
/// entries, rehashes and growth never move it. This is a hard
/// requirement: SessionEntry values carry self-referencing owner
/// pointers (TimerManager/ProcedureRunner), and the registry indexes
/// (mByLink, mActiveTimerSessions, mActiveProcedureSessions) plus
/// application code hold raw SubscriberSession* pointers into the map.
/// The previous deque-based storage relocated the last entry into the
/// erased slot on every erase, silently invalidating all of those
/// pointers (ghost timer ticks, hung procedures, cross-subscriber
/// state corruption).
///
/// Erase is in-place: the entry's Value is destroyed (std::optional
/// reset) and the entry index is recycled through a free list. A dead
/// entry KEEPS its heap block (the ~sizeof(Entry) allocation) until the
/// index is recycled by a later emplace, so a churn burst of K
/// simultaneous removals holds at most K extra blocks; the 8-byte
/// unique_ptr slot in mEntries is retained in any case. For steady
/// 1-remove/1-create churn the free list stays bounded by the burst
/// size, so no re-allocation happens on the recycle path.
///
/// The slot table capacity is always a power of two; probing wraps with
/// a bitmask. Deletion leaves a tombstone (probe chains stay intact);
/// the table rehashes (same capacity, clearing tombstones) when
/// tombstones exceed 50% of capacity, and grows (x2) when live
/// occupancy exceeds 70%. (The tombstone-density condition is the one
/// that bounds probe length: live occupancy alone can never exceed the
/// growth threshold, so a cleanup keyed on live+tombstone usage would
/// be unreachable.)
///
/// Thread safety: NOT thread-safe. One instance per owner/thread.
/// Memory: ~sizeof(Key) + sizeof(Value) + 16 bytes per entry (entry
/// block) plus an 8-byte unique_ptr slot and a 4-byte slot-table entry
/// at ~1/0.7 entries per slot.
template <typename Key, typename Value>
class FlatMap {
public:
    static constexpr uint32_t kEmpty = 0xFFFFFFFEu;  // slot: empty
    static constexpr uint32_t kTomb = 0xFFFFFFFFu;  // slot: tombstone
    static constexpr size_t npos = static_cast<size_t>(-1);

    FlatMap() = default;
    explicit FlatMap(size_t expectedEntries) { reserve(expectedEntries); }

    FlatMap(const FlatMap&) = delete;
    FlatMap& operator=(const FlatMap&) = delete;
    FlatMap(FlatMap&&) noexcept = default;
    FlatMap& operator=(FlatMap&&) noexcept = default;

    [[nodiscard]] size_t size() const noexcept { return mSize; }
    [[nodiscard]] bool empty() const noexcept { return mSize == 0; }
    [[nodiscard]] size_t capacity() const noexcept { return mSlots.size(); }

    /// Reserve space for at least `expectedEntries` entries under the 70%
    /// load threshold (cold path: startup sizing for known scale).
    void reserve(size_t expectedEntries);

    /// Insert or locate (std::unordered_map::emplace semantics).
    /// @return {entryIndex, inserted}. When the key already exists the
    ///         passed value is discarded and the existing entry is
    ///         returned.
    std::pair<size_t, bool> emplace(Key key, Value value);

    /// Find. @return entry index, or npos when absent.
    [[nodiscard]] size_t find(const Key& key) const noexcept;

    /// Access by entry index (from emplace/find). The entry must be live
    /// (not erased). The index and the entry address stay valid until
    /// THIS entry is erased — erasing other entries does not invalidate
    /// them (audit P0-1).
    [[nodiscard]] Value& at(size_t idx) noexcept { return *mEntries[idx]->value; }
    [[nodiscard]] const Value& at(size_t idx) const noexcept { return *mEntries[idx]->value; }
    [[nodiscard]] const Key& keyAt(size_t idx) const noexcept { return mEntries[idx]->key; }

    /// Erase by entry index. In-place: no other entry is moved, so all
    /// other entry addresses (and raw pointers derived from them) remain
    /// valid (audit P0-1). @return true if an entry was removed.
    bool erase(size_t idx) noexcept;

    /// Remove all entries and release every entry block (slot table is
    /// kept and reinitialized).
    void clear() noexcept;

    /// Visit every live entry: f(key, value). Order is unspecified.
    template <typename F>
    void forEach(F&& f) const {
        for (const auto& e : mEntries)
            if (e && e->value) f(e->key, *e->value);
    }

private:
    struct Entry {
        Key key{};
        std::optional<Value> value;  // engaged == live entry
        uint32_t slot{kEmpty};       // slot table index of this entry
    };

    static uint64_t hashKey(const Key& key) noexcept {
        // splitmix64 finalizer over std::hash: good avalanche for
        // sequential / patterned keys (TMSI counters, link keys).
        uint64_t h = static_cast<uint64_t>(std::hash<Key>{}(key));
        h ^= h >> 30; h *= 0xbf58476d1ce4e5b9ull;
        h ^= h >> 27; h *= 0x94d049bb133111ebull;
        h ^= h >> 31;
        return h;
    }

    static size_t nextPow2(size_t v) noexcept {
        size_t cap = 8;
        while (cap < v) cap <<= 1;
        return cap;
    }

    /// (Re)build the slot table into `newCap` slots, re-inserting all
    /// live entries. Entry blocks are NOT moved — only entry.slot is
    /// updated, so entry addresses stay stable across rehash.
    void rehash(size_t newCap);

    /// Obtain an entry index for a new entry: recycle a dead index from
    /// the free list (in-place reinitialization, address preserved) or
    /// append a fresh entry block.
    size_t acquireEntry(Key key, Value value);

    std::vector<std::unique_ptr<Entry>> mEntries;  // index = entry index; block addresses are stable
    std::vector<uint32_t> mSlots;  // slot -> entry index | kEmpty | kTomb
    std::vector<size_t> mFree;     // dead entry indexes, recycled by emplace
    size_t mSize{0};               // live entries
    size_t mTomb{0};               // tombstone slots
};

// ── Implementation ───────────────────────────────────────────────────

template <typename Key, typename Value>
void FlatMap<Key, Value>::reserve(size_t expectedEntries) {
    // Headroom so the 70% threshold is not crossed immediately.
    size_t need = expectedEntries + expectedEntries / 3 + 1;
    size_t cap = nextPow2(need);
    if (cap <= mSlots.size()) return;
    rehash(cap);
}

template <typename Key, typename Value>
void FlatMap<Key, Value>::rehash(size_t newCap) {
    std::vector<uint32_t> slots(newCap, kEmpty);
    const size_t mask = newCap - 1;
    for (size_t idx = 0; idx < mEntries.size(); ++idx) {
        if (!mEntries[idx] || !mEntries[idx]->value) continue;
        Entry& e = *mEntries[idx];
        size_t i = static_cast<size_t>(hashKey(e.key)) & mask;
        while (slots[i] != kEmpty) i = (i + 1) & mask;
        slots[i] = static_cast<uint32_t>(idx);
        e.slot = static_cast<uint32_t>(i);
    }
    mSlots = std::move(slots);
    mTomb = 0;
}

template <typename Key, typename Value>
size_t FlatMap<Key, Value>::acquireEntry(Key key, Value value) {
    if (!mFree.empty()) {
        // Recycle a dead entry block: the address is preserved, so the
        // new occupant sits at the same address the old one had.
        size_t idx = mFree.back();
        mFree.pop_back();
        Entry& e = *mEntries[idx];
        e.key = std::move(key);
        e.value.emplace(std::move(value));
        return idx;
    }
    mEntries.push_back(nullptr);
    size_t idx = mEntries.size() - 1;
    mEntries[idx] = std::make_unique<Entry>();
    Entry& e = *mEntries[idx];
    e.key = std::move(key);
    e.value.emplace(std::move(value));
    return idx;
}

template <typename Key, typename Value>
std::pair<size_t, bool> FlatMap<Key, Value>::emplace(Key key, Value value) {
    if (mSlots.empty()) rehash(8);
    const size_t mask = mSlots.size() - 1;
    size_t i = static_cast<size_t>(hashKey(key)) & mask;
    size_t firstTomb = mSlots.size(); // remember the first tombstone
    while (true) {
        uint32_t s = mSlots[i];
        if (s == kEmpty) {
            // Insert at the first tombstone (if any) to keep probes short.
            size_t at = (firstTomb != mSlots.size()) ? firstTomb : i;
            size_t idx = acquireEntry(std::move(key), std::move(value));
            mSlots[at] = static_cast<uint32_t>(idx);
            mEntries[idx]->slot = static_cast<uint32_t>(at);
            if (at == firstTomb) --mTomb;
            ++mSize;
            // Growth check (70% occupancy).
            if (mSize * 10 > mSlots.size() * 7) {
                rehash(mSlots.size() * 2);
            }
            return {idx, true};
        }
        if (s == kTomb) {
            if (firstTomb == mSlots.size()) firstTomb = i;
        } else if (mEntries[s]->key == key) {
            return {s, false}; // existing entry; passed value discarded
        }
        i = (i + 1) & mask;
    }
}

template <typename Key, typename Value>
size_t FlatMap<Key, Value>::find(const Key& key) const noexcept {
    if (mSlots.empty()) return npos;
    const size_t mask = mSlots.size() - 1;
    size_t i = static_cast<size_t>(hashKey(key)) & mask;
    while (true) {
        uint32_t s = mSlots[i];
        if (s == kEmpty) return npos;
        if (s != kTomb && mEntries[s]->value && mEntries[s]->key == key) return s;
        i = (i + 1) & mask;
    }
}

template <typename Key, typename Value>
bool FlatMap<Key, Value>::erase(size_t idx) noexcept {
    if (idx >= mEntries.size() || !mEntries[idx] || !mEntries[idx]->value) return false;
    // In-place erase: the entry block stays at its address; only its
    // Value is destroyed and the slot becomes a tombstone. No other
    // entry moves, so every other entry address (and every raw pointer
    // derived from one) remains valid (audit P0-1).
    mSlots[mEntries[idx]->slot] = kTomb;
    ++mTomb;
    mEntries[idx]->value.reset();
    mEntries[idx]->key = Key{};
    mFree.push_back(idx);
    --mSize;
    // Tombstone cleanup: more than half the slots are tombstones.
    // (mSize + mTomb is invariant under erase, so a cleanup keyed on
    // total used slots would be unreachable.)
    if (mTomb * 2 > mSlots.size()) {
        rehash(mSlots.size());
    }
    return true;
}

template <typename Key, typename Value>
void FlatMap<Key, Value>::clear() noexcept {
    // Release every entry block (matches the previous semantics: clear()
    // frees all entry storage, not just the values).
    mEntries.clear();
    mFree.clear();
    mSize = 0;
    mTomb = 0;
    std::fill(mSlots.begin(), mSlots.end(), kEmpty);
}

} // namespace gsml3parser
