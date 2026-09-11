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
/// Storage (audit D1): entries live in contiguous slabs of kSlabEntries
/// (64) entries — one slab allocation per 64 entries instead of the
/// previous one heap block per entry (10M sessions = 10M ~2KB mallocs,
/// ~21 GB RSS, allocator churn and poor traversal locality). Slabs are
/// never moved or reallocated, so every entry address is stable for the
/// entry's whole lifetime: insertions, erasures of OTHER entries,
/// rehashes and growth never move an entry (audit P0-1 — a hard
/// requirement, because SessionEntry values carry self-referencing
/// owner pointers and the registry indexes plus application code hold
/// raw SubscriberSession* pointers into the map).
///
/// Erase is in-place: the entry's Value is destroyed (std::optional
/// reset) and the entry index is recycled through a free list at the
/// SAME address. A dead entry keeps its slab slot until recycled by a
/// later emplace, so a churn burst of K simultaneous removals holds at
/// most K dead entries; steady 1-remove/1-create churn recycles without
/// any allocation.
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
/// forEach() visits every ALLOCATED entry (live + dead): O(allocated),
/// not O(live). At 10M scale with high churn this degrades relative to
/// the live count — use the active-set indexes (registry) for hot
/// iteration, not forEach.
///
/// Thread safety: NOT thread-safe. One instance per owner/thread.
/// Memory: sizeof(Key) + sizeof(Value) + ~16 bytes per entry inside a
/// shared slab, plus a 4-byte slot-table entry at ~1/0.7 entries per
/// slot and one slab header per 64 entries.
template <typename Key, typename Value>
class FlatMap {
public:
    static constexpr uint32_t kEmpty = 0xFFFFFFFEu;  // slot: empty
    static constexpr uint32_t kTomb = 0xFFFFFFFFu;  // slot: tombstone
    static constexpr size_t npos = static_cast<size_t>(-1);

    // Slab layout (audit D1): entries live in contiguous slabs of
    // kSlabEntries entries each, replacing the previous one-heap-block-
    // per-entry storage (10M sessions = 10M ~2KB allocations). Slabs are
    // never moved or reallocated, so an entry's address stays stable for
    // its whole lifetime (audit P0-1: insertions, erasures of OTHER
    // entries, rehashes and growth never move it) — a hard requirement:
    // SessionEntry values carry self-referencing owner pointers
    // (TimerManager/ProcedureRunner), and the registry indexes
    // (mByLink, mActiveTimerSessions, mActiveProcedureSessions) plus
    // application code hold raw SubscriberSession* pointers into the map.
    static constexpr unsigned kLog2SlabEntries = 6;
    static constexpr size_t kSlabEntries = 1u << kLog2SlabEntries;

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
    /// Pre-allocates the slot table AND the slabs, so steady-state
    /// emplace never allocates (audit D1).
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
    [[nodiscard]] Value& at(size_t idx) noexcept { return *mEntryAt(idx).value; }
    [[nodiscard]] const Value& at(size_t idx) const noexcept { return *mEntryAt(idx).value; }
    [[nodiscard]] const Key& keyAt(size_t idx) const noexcept { return mEntryAt(idx).key; }

    /// Erase by entry index. In-place: no other entry is moved, so all
    /// other entry addresses (and raw pointers derived from them) remain
    /// valid (audit P0-1). @return true if an entry was removed.
    bool erase(size_t idx) noexcept;

    /// Remove all entries and release every slab (slot table is kept and
    /// reinitialized).
    void clear() noexcept;

    /// Visit every live entry: f(key, value). Order is unspecified.
    /// O(allocated entries), not O(live) — with high churn the visited
    /// set includes dead (recyclable) entries (see class docs).
    template <typename F>
    void forEach(F&& f) const {
        for (const auto& slab : mSlabs)
            for (size_t j = 0; j < kSlabEntries; ++j)
                if (slab[j].value) f(slab[j].key, *slab[j].value);
    }

private:
    struct Entry {
        Key key{};
        std::optional<Value> value;  // engaged == live entry
        uint32_t slot{kEmpty};       // slot table index of this entry
    };

    // Entry index -> slab storage. Slabs are fixed-size blocks that are
    // never moved: the address of slab[j] is stable for the slab's
    // lifetime (audit D1 / P0-1).
    [[nodiscard]] Entry& mEntryAt(size_t idx) noexcept {
        return mSlabs[idx >> kLog2SlabEntries][idx & (kSlabEntries - 1)];
    }
    [[nodiscard]] const Entry& mEntryAt(size_t idx) const noexcept {
        return mSlabs[idx >> kLog2SlabEntries][idx & (kSlabEntries - 1)];
    }

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

    /// Allocate one slab (kSlabEntries default-constructed entries).
    void allocateSlab() {
        mSlabs.push_back(std::unique_ptr<Entry[]>(new Entry[kSlabEntries]));
    }

    /// Obtain an entry index for a new entry: recycle a dead index from
    /// the free list (in-place reinitialization, address preserved) or
    /// take the next index, growing the slab list when needed.
    size_t acquireEntry(Key key, Value value);

    std::vector<std::unique_ptr<Entry[]>> mSlabs;  // slab i holds entries [i*K, (i+1)*K)
    std::vector<uint32_t> mSlots;  // slot -> entry index | kEmpty | kTomb
    std::vector<size_t> mFree;     // dead entry indexes, recycled by emplace
    size_t mEntryCount{0};         // entries allocated (slabs * kSlabEntries)
    size_t mSize{0};               // live entries
    size_t mTomb{0};               // tombstone slots
};

// ── Implementation ───────────────────────────────────────────────────

template <typename Key, typename Value>
void FlatMap<Key, Value>::reserve(size_t expectedEntries) {
    // Headroom so the 70% threshold is not crossed immediately.
    size_t need = expectedEntries + expectedEntries / 3 + 1;
    size_t cap = nextPow2(need);
    if (cap > mSlots.size()) rehash(cap);
    // Pre-allocate slabs so steady-state emplace never allocates
    // (audit D1: startup sizing for known scale).
    while (mSlabs.size() * kSlabEntries < expectedEntries) allocateSlab();
}

template <typename Key, typename Value>
void FlatMap<Key, Value>::rehash(size_t newCap) {
    std::vector<uint32_t> slots(newCap, kEmpty);
    const size_t mask = newCap - 1;
    for (size_t idx = 0; idx < mEntryCount; ++idx) {
        Entry& e = mEntryAt(idx);
        if (!e.value) continue;
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
    size_t idx;
    if (!mFree.empty()) {
        // Recycle a dead entry: the address is preserved, so the new
        // occupant sits at the same address the old one had.
        idx = mFree.back();
        mFree.pop_back();
    } else {
        idx = mEntryCount++;
        if (idx >= mSlabs.size() * kSlabEntries) allocateSlab();
    }
    Entry& e = mEntryAt(idx);
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
            mEntryAt(idx).slot = static_cast<uint32_t>(at);
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
        } else if (mEntryAt(s).key == key) {
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
        if (s != kTomb) {
            const Entry& e = mEntryAt(s);
            if (e.value && e.key == key) return s;
        }
        i = (i + 1) & mask;
    }
}

template <typename Key, typename Value>
bool FlatMap<Key, Value>::erase(size_t idx) noexcept {
    if (idx >= mEntryCount || !mEntryAt(idx).value) return false;
    // In-place erase: the entry keeps its slab address; only its Value
    // is destroyed and the slot becomes a tombstone. No other entry
    // moves, so every other entry address (and every raw pointer
    // derived from one) remains valid (audit P0-1).
    Entry& e = mEntryAt(idx);
    mSlots[e.slot] = kTomb;
    ++mTomb;
    e.value.reset();
    e.key = Key{};
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
    // Release every slab (matches the previous semantics: clear() frees
    // all entry storage, not just the values).
    mSlabs.clear();
    mFree.clear();
    mEntryCount = 0;
    mSize = 0;
    mTomb = 0;
    std::fill(mSlots.begin(), mSlots.end(), kEmpty);
}

} // namespace gsml3parser
