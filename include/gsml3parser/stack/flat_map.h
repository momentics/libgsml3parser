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
#include <deque>
#include <functional>
#include <utility>
#include <vector>

namespace gsml3parser {

/// Open-addressing flat hash map with linear probing (registry scale,
/// audit: tens of millions of concurrent streams).
///
/// Stores key/value pairs in a compact entry sequence plus a slot index:
/// no per-node heap allocation, no pointer chasing on lookup. The entry
/// sequence (std::deque) keeps the address of every existing entry stable
/// across insertions — required because SessionEntry values carry
/// self-referencing owner pointers (TimerManager/ProcedureRunner). The
/// slot table capacity is always a power of two; probing wraps with a
/// bitmask. Deletion leaves a tombstone (probe chains stay intact); the
/// table rehashes (same capacity, clearing tombstones) when occupied +
/// tombstone slots exceed 70% of capacity, and grows (x2) when occupancy
/// exceeds 70%.
///
/// Values are MOVED (never memcpy'd) during rehash, so value types
/// containing pointers (e.g. std::unique_ptr) are safe — this is what
/// makes SessionEntry (ProcedureRunner with unique_ptr slots) usable.
///
/// Thread safety: NOT thread-safe. One instance per owner/thread.
/// Memory: ~sizeof(Key) + sizeof(Value) + 4 bytes per entry, plus a
/// 4-byte slot table at ~1/0.7 entries per slot.
template <typename Key, typename Value>
class FlatMap {
public:
    static constexpr uint32_t kEmpty = 0xFFFFFFFEu;  // slot: empty
    static constexpr uint32_t kTomb  = 0xFFFFFFFFu;  // slot: tombstone
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

    /// Access by entry index (from emplace/find).
    [[nodiscard]] Value& at(size_t idx) noexcept { return mEntries[idx].value; }
    [[nodiscard]] const Value& at(size_t idx) const noexcept { return mEntries[idx].value; }
    [[nodiscard]] const Key& keyAt(size_t idx) const noexcept { return mEntries[idx].key; }

    /// Erase by entry index. @return true if an entry was removed.
    bool erase(size_t idx) noexcept;

    /// Remove all entries (slot table is kept).
    void clear() noexcept;

    /// Visit every occupied entry: f(key, value). Order is unspecified.
    template <typename F>
    void forEach(F&& f) const {
        for (const auto& e : mEntries) f(e.key, e.value);
    }

private:
    struct Entry {
        Key key{};
        Value value{};
        uint32_t slot{kEmpty}; // slot table index of this entry
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
    /// entries. Entry values are NOT copied — only entry.slot is updated.
    void rehash(size_t newCap);

    std::deque<Entry> mEntries;    // compact: size == mSize; stable entry addresses
    std::vector<uint32_t> mSlots;  // slot -> entry index | kEmpty | kTomb
    size_t mSize{0};               // occupied entries
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
        Entry& e = mEntries[idx];
        size_t i = static_cast<size_t>(hashKey(e.key)) & mask;
        while (slots[i] != kEmpty) i = (i + 1) & mask;
        slots[i] = static_cast<uint32_t>(idx);
        e.slot = static_cast<uint32_t>(i);
    }
    mSlots = std::move(slots);
    mTomb = 0;
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
            mEntries.push_back(Entry{std::move(key), std::move(value),
                                     static_cast<uint32_t>(at)});
            mSlots[at] = static_cast<uint32_t>(mEntries.size() - 1);
            if (at == firstTomb) --mTomb;
            ++mSize;
            // Growth check (70% occupancy).
            if (mSize * 10 > mSlots.size() * 7) {
                rehash(mSlots.size() * 2);
            }
            return {mEntries.size() - 1, true};
        }
        if (s == kTomb) {
            if (firstTomb == mSlots.size()) firstTomb = i;
        } else if (mEntries[s].key == key) {
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
        if (s != kTomb && mEntries[s].key == key) return s;
        i = (i + 1) & mask;
    }
}

template <typename Key, typename Value>
bool FlatMap<Key, Value>::erase(size_t idx) noexcept {
    if (idx >= mEntries.size() || mEntries[idx].slot == kEmpty) return false;
    const size_t mask = mSlots.size() - 1;
    uint32_t slot = mEntries[idx].slot;
    if (idx == mEntries.size() - 1) {
        // Erasing the last entry: its slot becomes a tombstone.
        mSlots[slot] = kTomb;
        ++mTomb;
        mEntries.pop_back();
    } else {
        // Swap-with-last keeps the entry sequence compact. Both the erased
        // entry's slot and the moved entry's former slot become tombstones
        // (NOT empty): probe chains that pass through them must keep going,
        // otherwise entries behind them become unreachable. The moved entry
        // is then re-seated from its hash position: linear probing requires
        // an entry to sit on its own hash's probe path, so simply pointing
        // the erased slot at it would make lookups for it miss (a probe
        // starting at its hash could hit an empty slot first).
        mSlots[slot] = kTomb;
        ++mTomb;
        mSlots[mEntries.back().slot] = kTomb;
        ++mTomb;
        mEntries[idx] = std::move(mEntries.back());
        mEntries.pop_back();
        size_t j = static_cast<size_t>(hashKey(mEntries[idx].key)) & mask;
        size_t firstTomb = mSlots.size();
        while (true) {
            uint32_t s = mSlots[j];
            if (s == kEmpty) {
                size_t at = (firstTomb != mSlots.size()) ? firstTomb : j;
                mSlots[at] = static_cast<uint32_t>(idx);
                mEntries[idx].slot = static_cast<uint32_t>(at);
                if (at == firstTomb) --mTomb;
                break;
            }
            if (s == kTomb && firstTomb == mSlots.size()) firstTomb = j;
            j = (j + 1) & mask;
        }
    }
    --mSize;
    // Tombstone cleanup (70% of slots occupied + tombstoned).
    if ((mSize + mTomb) * 10 > mSlots.size() * 7) {
        rehash(mSlots.size());
    }
    return true;
}

template <typename Key, typename Value>
void FlatMap<Key, Value>::clear() noexcept {
    mEntries.clear();
    mSize = 0;
    mTomb = 0;
    std::fill(mSlots.begin(), mSlots.end(), kEmpty);
}

} // namespace gsml3parser
