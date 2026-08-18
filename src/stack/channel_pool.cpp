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
    (void)neci; // NECI extension reserved for future use; legacy decoding applied

    // Establishment cause: bits 6-5 of RA byte (GSM 04.08 5.1.3)
    //   00 - Mobile Originating (normal call)
    //   01 - Emergency Call
    //   10 - Answer to Paging
    //   11 - Location Updating
    uint8_t establishmentCause = (ra >> 5) & 0x03;

    switch (establishmentCause) {
        case 0x00: {
            // MO call: with VEA assign TCH directly, otherwise SDCCH
            if (vea) {
                return ChannelType::TCHFType;
            }
            return ChannelType::SDCCHType;
        }
        case 0x01:
            // Emergency call always needs TCH
            return ChannelType::TCHFType;
        case 0x02:
            // Answer to Paging needs TCH
            return ChannelType::TCHFType;
        case 0x03:
            // Location Updating needs SDCCH
            return ChannelType::SDCCHType;
        default:
            return ChannelType::UndefinedCHType;
    }
}

bool isLocationUpdatingRequest(uint8_t ra, bool neci) {
    (void)neci; // NECI extension reserved for future use
    // Establishment cause 11 (bits 6-5) = Location Updating
    return ((ra >> 5) & 0x03) == 0x03;
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
    // For MO calls (establishment cause 00), try TCH first, then SDCCH
    uint8_t establishmentCause = (ra >> 5) & 0x03;

    if (establishmentCause == 0x00) {
        // MO call: try TCHF first
        auto tch = allocate(ChannelType::TCHFType);
        if (tch) return tch;
        // Try TCHH as fallback within TCH family
        auto tchh = allocate(ChannelType::TCHHType);
        if (tchh) return tchh;
        // Fall back to SDCCH
        return allocate(ChannelType::SDCCHType);
    }

    // For other causes, use standard decode + allocate
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
