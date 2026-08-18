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

#include "gsml3parser/stack/subscriber_registry.h"
#include <mutex>

namespace gsml3parser {

// ── SubscriberRegistry ──────────────────────────────────────────────

SubscriberSession* SubscriberRegistry::createByTMSI(uint32_t tmsi) {
    auto [it, inserted] = mByTMSI.emplace(tmsi, SessionEntry{});
    if (!inserted) return nullptr;

    it->second.session.context.setTMSI(tmsi);
    it->second.session.assignedTmsi = tmsi;
    return &it->second.session;
}

SubscriberSession* SubscriberRegistry::createByIMSI(std::string_view imsi) {
    std::string key(imsi);
    if (mByIMSI.count(key) != 0) return nullptr;

    // Allocate a unique TMSI: advance the high-water mark past any in-use
    // value (user-assigned TMSIs may occupy arbitrary slots) and skip the
    // reserved all-zero TMSI.
    uint32_t tmsi = mNextAutoTmsi++;
    while (tmsi == 0 || mByTMSI.count(tmsi) != 0) {
        tmsi = mNextAutoTmsi++;
    }

    auto [tmsiIt, inserted] = mByTMSI.emplace(tmsi, SessionEntry{});
    if (!inserted) return nullptr;

    tmsiIt->second.session.context.setIMSI(imsi);
    tmsiIt->second.session.assignedTmsi = tmsi;
    mByIMSI.emplace(std::move(key), tmsi);
    return &tmsiIt->second.session;
}

SubscriberSession* SubscriberRegistry::findByTMSI(uint32_t tmsi) noexcept {
    auto it = mByTMSI.find(tmsi);
    if (it != mByTMSI.end() && it->second.active) return &it->second.session;
    return nullptr;
}

const SubscriberSession* SubscriberRegistry::findByTMSI(uint32_t tmsi) const noexcept {
    auto it = mByTMSI.find(tmsi);
    if (it != mByTMSI.end() && it->second.active) return &it->second.session;
    return nullptr;
}

SubscriberSession* SubscriberRegistry::findByIMSI(std::string_view imsi) noexcept {
    auto it = mByIMSI.find(std::string(imsi));
    if (it != mByIMSI.end()) {
        uint32_t tmsi = it->second;
        return findByTMSI(tmsi);
    }
    return nullptr;
}

const SubscriberSession* SubscriberRegistry::findByIMSI(std::string_view imsi) const noexcept {
    auto it = mByIMSI.find(std::string(imsi));
    if (it != mByIMSI.end()) {
        uint32_t tmsi = it->second;
        return findByTMSI(tmsi);
    }
    return nullptr;
}

namespace {
/// Encode LAPDm link key from trx, timeslot, and link ID.
constexpr uint32_t encodeLinkKey(uint8_t trx, uint8_t ts, uint8_t lapdmLink) noexcept {
    return (static_cast<uint32_t>(trx) << 16) |
           (static_cast<uint32_t>(ts) << 8) |
           static_cast<uint32_t>(lapdmLink);
}
} // anonymous namespace

SubscriberSession* SubscriberRegistry::findByLink(uint8_t trx, uint8_t ts, uint8_t lapdmLink) noexcept {
    uint32_t key = encodeLinkKey(trx, ts, lapdmLink);
    auto it = mByLink.find(key);
    if (it != mByLink.end()) return it->second;
    return nullptr;
}

void SubscriberRegistry::assignChannel(SubscriberSession* session, ChannelDescriptor desc,
                                        uint8_t lapdmLink) noexcept {
    // Remove old link entry if channel was previously assigned.
    if (session->channel.has_value()) {
        uint32_t oldKey = encodeLinkKey(session->context.trxNumber(),
                                        session->context.timeslot(),
                                        session->lapdmLink);
        mByLink.erase(oldKey);
    }

    session->channel = desc;
    session->lapdmLink = lapdmLink;
    session->context.assignChannel(desc.type, desc.trxNumber, desc.timeslot, desc.arfcn);

    uint32_t key = encodeLinkKey(desc.trxNumber, desc.timeslot, lapdmLink);
    mByLink[key] = session;
}

void SubscriberRegistry::releaseChannel(SubscriberSession* session) noexcept {
    if (session->channel.has_value()) {
        uint32_t key = encodeLinkKey(session->context.trxNumber(),
                                     session->context.timeslot(),
                                     session->lapdmLink);
        mByLink.erase(key);
    }
    session->channel.reset();
    session->context.releaseChannel();
}

bool SubscriberRegistry::remove(SubscriberSession* session) noexcept {
    // O(1): derive the TMSI key from the session and look it up directly.
    if (!session) return false;
    uint32_t tmsi = session->assignedTmsi;
    auto it = mByTMSI.find(tmsi);
    if (it == mByTMSI.end() || &it->second.session != session || !it->second.active) {
        return false;
    }
    releaseChannel(session);
    if (session->context.identity().isIMSI()) {
        mByIMSI.erase(std::string(session->context.identity().digits()));
    }
    // Erase the entry so memory is reclaimed (previously the entry
    // stayed in the map with active=false, leaking on every removal).
    // The session pointer is invalidated by this call.
    mByTMSI.erase(it);
    return true;
}

void SubscriberRegistry::clear() noexcept {
    // Erase all entries to release memory (previously only flagged inactive).
    mByTMSI.clear();
    mByIMSI.clear();
    mByLink.clear();
}

size_t SubscriberRegistry::count() const noexcept {
    size_t c = 0;
    for (const auto& [tmsi, entry] : mByTMSI) {
        if (entry.active) ++c;
    }
    return c;
}

size_t SubscriberRegistry::tickAllTimers(std::chrono::milliseconds delta,
                                          std::span<L3TimerId> expiredOut) {
    size_t written = 0;
    for (auto& [tmsi, entry] : mByTMSI) {
        if (!entry.active) continue;
        std::array<L3TimerId, 32> localBuf{};
        size_t n = entry.session.timers.tick(delta, std::span<L3TimerId>{localBuf});
        for (size_t j = 0; j < n && written < expiredOut.size(); ++j, ++written) {
            expiredOut[written] = localBuf[j];
        }
    }
    return written;
}

// ── ShardedSubscriberRegistry: template method definitions ───────────

template<int N>
SubscriberSession* ShardedSubscriberRegistry<N>::createByTMSI(uint32_t tmsi) {
    int idx = shardIndex(hashTMSI(tmsi));
    std::unique_lock lock(mShards[idx].mutex);
    return mShards[idx].registry.createByTMSI(tmsi);
}

template<int N>
SubscriberSession* ShardedSubscriberRegistry<N>::findByTMSI(uint32_t tmsi) noexcept {
    int idx = shardIndex(hashTMSI(tmsi));
    std::shared_lock lock(mShards[idx].mutex);
    return mShards[idx].registry.findByTMSI(tmsi);
}

template<int N>
typename ShardedSubscriberRegistry<N>::LockedSession
ShardedSubscriberRegistry<N>::findLocked(uint32_t tmsi) {
    int idx = shardIndex(hashTMSI(tmsi));
    LockedSession ls;
    ls.guard = SharedGuard(mShards[idx]);
    ls.session = mShards[idx].registry.findByTMSI(tmsi);
    return ls;
}

template<int N>
typename ShardedSubscriberRegistry<N>::LockedShard
ShardedSubscriberRegistry<N>::lockForTMSI(uint32_t tmsi) {
    int idx = shardIndex(hashTMSI(tmsi));
    LockedShard ls{mShards[idx].registry, UniqueGuard(mShards[idx])};
    return ls;
}

template<int N>
SubscriberSession* ShardedSubscriberRegistry<N>::findByIMSI(std::string_view imsi) noexcept {
    for (auto& shard : mShards) {
        std::shared_lock lock(shard.mutex);
        if (auto* s = shard.registry.findByIMSI(imsi)) return s;
    }
    return nullptr;
}

template<int N>
SubscriberSession* ShardedSubscriberRegistry<N>::findByLink(uint8_t trx, uint8_t ts,
                                                              uint8_t lapdmLink) noexcept {
    for (auto& shard : mShards) {
        std::shared_lock lock(shard.mutex);
        if (auto* s = shard.registry.findByLink(trx, ts, lapdmLink)) return s;
    }
    return nullptr;
}

template<int N>
bool ShardedSubscriberRegistry<N>::remove(SubscriberSession* session) noexcept {
    for (auto& shard : mShards) {
        std::unique_lock lock(shard.mutex);
        if (shard.registry.remove(session)) return true;
    }
    return false;
}

template<int N>
size_t ShardedSubscriberRegistry<N>::tickAllTimers(std::chrono::milliseconds delta,
                                                     std::span<L3TimerId> expiredOut) {
    size_t total = 0;
    for (auto& shard : mShards) {
        std::unique_lock lock(shard.mutex);
        size_t remaining = expiredOut.size() - total;
        if (remaining == 0) break;
        total += shard.registry.tickAllTimers(delta, expiredOut.subspan(total, remaining));
    }
    return total;
}

// Explicit template instantiations
template class ShardedSubscriberRegistry<4>;
template class ShardedSubscriberRegistry<8>;
template class ShardedSubscriberRegistry<16>;
template class ShardedSubscriberRegistry<32>;

} // namespace gsml3parser
