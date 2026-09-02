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

#include "gsml3parser/stack/channel_pool.h"

#include <algorithm>
#include <iterator>

namespace gsml3parser {

// ── RA Decoding (GSM 04.08 Table 9.9) ──────────────────────────────────

ChannelType decodeChannelNeeded(uint8_t ra, bool neci, bool vea) {
    (void)neci; // NECI-specific variants are covered by the explicit patterns below.

    // 8-bit RA pattern decoding — TS 44.018 Table 9.1.8.1 / 9.1.8.2
    // (audit C2: the previous 2-bit (ra >> 5) & 0x03 mapping misclassified
    // most patterns, e.g. originating call 111xxxxx became "location
    // updating" -> SDCCH instead of TCH).
    if (ra < 0x20) return ChannelType::SDCCHType;   // 0000xxxx LU / 0001xxxx other SDCCH procedures
    if (ra < 0x30) return ChannelType::TCHFType;    // 0010xxxx answer to paging, TCH/F
    if (ra < 0x40) return ChannelType::TCHHType;    // 0011xxxx answer to paging, TCH/H or TCH/F
    if (ra < 0x60) return ChannelType::TCHHType;    // 0100xxxx/0101xxxx MO speech/data TCH/H (NECI)
    if (ra < 0x68) return ChannelType::SDCCHType;   // 01100xxx MBMS/reserved + 01100111 LMU
    if (ra < 0x70) return ChannelType::TCHHType;    // 011010xx/011011xx re-establishment TCH/H (NECI)
    if (ra < 0x80) return ChannelType::UndefinedCHType; // 0111xxxx GPRS packet access / reserved (no PCU)
    if (ra < 0xA0) return ChannelType::TCHFType;    // 100xxxxx answer to paging (any channel)
    if (ra < 0xC0) return ChannelType::TCHFType;    // 101xxxxx emergency / 110xxxxx re-establishment TCH/F
    return vea ? ChannelType::TCHFType : ChannelType::SDCCHType; // 111xxxxx MO call
}

bool isLocationUpdatingRequest(uint8_t ra, bool neci) {
    (void)neci; // NECI variants are covered by the explicit pattern below.
    // 0000xxxx: location updating (NECI=1). The 0001xxxx form ("other
    // SDCCH procedures", NECI=1) is ambiguous with paging SDCCH-only
    // accesses, so only 0000xxxx is reported as LU (audit C2: the previous
    // (ra >> 5) & 0x03 == 0x03 test matched 0110xxxx = re-establishment).
    return ra < 0x10;
}

// ── ChannelPool Implementation ─────────────────────────────────────────

void ChannelPool::addChannel(ChannelDescriptor desc) {
    size_t idx = static_cast<size_t>(desc.type);
    mFreeByType[idx].push_back(std::move(desc));
}

bool ChannelPool::removeChannel(const ChannelDescriptor& desc) {
    if (!knowsChannel(desc)) {
        return false;
    }

    // Try to remove from free list first
    if (removeFromFreeList(desc.type, desc)) {
        return true;
    }

    // Remove from allocated set - O(1)
    size_t idx = static_cast<size_t>(desc.type);
    auto& allocs = mAllocatedByType[idx];
    if (allocs.erase(desc) > 0) {
        return true;
    }

    return false;
}

std::optional<ChannelDescriptor> ChannelPool::allocate(ChannelType type) {
    size_t idx = static_cast<size_t>(type);
    auto& freeVec = mFreeByType[idx];
    if (freeVec.empty()) {
        return std::nullopt;
    }

    // Pop from back of vector - O(1), no reallocation
    ChannelDescriptor ch = std::move(freeVec.back());
    freeVec.pop_back();

    // Track as allocated - O(1) set insert
    mAllocatedByType[static_cast<size_t>(ch.type)].insert(ch);
    return ch;
}

bool ChannelPool::release(const ChannelDescriptor& desc) {
    size_t idx = static_cast<size_t>(desc.type);
    auto& allocs = mAllocatedByType[idx];
    auto it = allocs.find(desc);
    if (it == allocs.end()) {
        return false;
    }

    // Remove from allocated - O(1) set erase
    allocs.erase(it);

    // Return to free list
    mFreeByType[idx].push_back(desc);
    return true;
}

bool ChannelPool::isFree(const ChannelDescriptor& desc) const {
    size_t idx = static_cast<size_t>(desc.type);
    const auto& freeVec = mFreeByType[idx];
    return std::any_of(freeVec.begin(), freeVec.end(),
                       [&desc](const ChannelDescriptor& ch) { return ch == desc; });
}

size_t ChannelPool::freeCount(ChannelType type) const {
    size_t idx = static_cast<size_t>(type);
    return mFreeByType[idx].size();
}

size_t ChannelPool::totalCount() const {
    size_t total = 0;
    for (const auto& channels : mFreeByType) {
        total += channels.size();
    }
    for (const auto& channels : mAllocatedByType) {
        total += channels.size();
    }
    return total;
}

std::optional<ChannelDescriptor> ChannelPool::allocateVEA(uint8_t ra) {
    // VEA (Very Early Assignment) applies to originating calls
    // (RA 111xxxxx): assign a TCH directly, falling back to SDCCH when no
    // TCH is free (TS 44.018 5.2.4, audit C2: the previous
    // (ra >> 5) & 0x03 == 0 test applied VEA to location updating 000xxxxx,
    // which must never be assigned a TCH).
    if (ra >= 0xC0) {
        if (auto tch = allocate(ChannelType::TCHFType)) return tch;
        // Try TCHH as fallback within the TCH family.
        if (auto tchh = allocate(ChannelType::TCHHType)) return tchh;
        // Fall back to SDCCH.
        return allocate(ChannelType::SDCCHType);
    }

    // Other causes: standard decode + allocate.
    ChannelType needed = decodeChannelNeeded(ra, false, false);
    if (needed == ChannelType::UndefinedCHType) {
        return std::nullopt;
    }
    return allocate(needed);
}

std::vector<ChannelDescriptor> ChannelPool::freeChannels(ChannelType type) const {
    size_t idx = static_cast<size_t>(type);
    return mFreeByType[idx];
}

size_t ChannelPool::allocatedCount(ChannelType type) const {
    size_t idx = static_cast<size_t>(type);
    return mAllocatedByType[idx].size();
}

bool ChannelPool::knowsChannel(const ChannelDescriptor& desc) const {
    size_t idx = static_cast<size_t>(desc.type);
    for (const auto& ch : mFreeByType[idx]) {
        if (ch == desc) return true;
    }
    // O(1) set lookup for the allocated part
    return mAllocatedByType[idx].contains(desc);
}

bool ChannelPool::removeFromFreeList(ChannelType type, const ChannelDescriptor& desc) {
    size_t idx = static_cast<size_t>(type);
    auto& vec = mFreeByType[idx];
    auto it = std::find(vec.begin(), vec.end(), desc);
    if (it != vec.end()) {
        vec.erase(it);
        return true;
    }
    return false;
}

} // namespace gsml3parser
