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
    //   00 — Mobile Originating (normal call)
    //   01 — Emergency Call
    //   10 — Answer to Paging
    //   11 — Location Updating
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
    mFreeByType[desc.type].push_back(std::move(desc));
}

bool ChannelPool::removeChannel(const ChannelDescriptor& desc) {
    if (!knowsChannel(desc)) {
        return false;
    }

    // Try to remove from free list first
    if (removeFromFreeList(desc.type, desc)) {
        return true;
    }

    // Remove from allocated list
    auto& allocs = mAllocatedByType[desc.type];
    auto it = std::find(allocs.begin(), allocs.end(), desc);
    if (it != allocs.end()) {
        allocs.erase(it);
        return true;
    }

    return false;
}

std::optional<ChannelDescriptor> ChannelPool::allocate(ChannelType type) {
    auto fit = mFreeByType.find(type);
    if (fit == mFreeByType.end() || fit->second.empty()) {
        return std::nullopt;
    }

    // Pop from back of vector — O(1), no reallocation
    ChannelDescriptor ch = std::move(fit->second.back());
    fit->second.pop_back();

    // Track as allocated
    mAllocatedByType[ch.type].push_back(ch);
    return ch;
}

bool ChannelPool::release(const ChannelDescriptor& desc) {
    auto& allocs = mAllocatedByType[desc.type];
    auto it = std::find(allocs.begin(), allocs.end(), desc);
    if (it == allocs.end()) {
        return false;
    }

    // Remove from allocated
    allocs.erase(it);

    // Return to free list
    mFreeByType[desc.type].push_back(desc);
    return true;
}

bool ChannelPool::isFree(const ChannelDescriptor& desc) const {
    auto fit = mFreeByType.find(desc.type);
    if (fit == mFreeByType.end()) {
        return false;
    }
    return std::any_of(fit->second.begin(), fit->second.end(),
                       [&desc](const ChannelDescriptor& ch) { return ch == desc; });
}

size_t ChannelPool::freeCount(ChannelType type) const {
    auto fit = mFreeByType.find(type);
    if (fit == mFreeByType.end()) {
        return 0;
    }
    return fit->second.size();
}

size_t ChannelPool::totalCount() const {
    size_t total = 0;
    for (auto& [type, channels] : mFreeByType) {
        total += channels.size();
    }
    for (auto& [type, channels] : mAllocatedByType) {
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
    auto fit = mFreeByType.find(type);
    if (fit == mFreeByType.end()) {
        return {};
    }
    return fit->second;
}

size_t ChannelPool::allocatedCount(ChannelType type) const {
    auto ait = mAllocatedByType.find(type);
    if (ait == mAllocatedByType.end()) {
        return 0;
    }
    return ait->second.size();
}

bool ChannelPool::knowsChannel(const ChannelDescriptor& desc) const {
    auto fit = mFreeByType.find(desc.type);
    if (fit != mFreeByType.end()) {
        for (auto& ch : fit->second) {
            if (ch == desc) return true;
        }
    }
    auto ait = mAllocatedByType.find(desc.type);
    if (ait != mAllocatedByType.end()) {
        for (auto& ch : ait->second) {
            if (ch == desc) return true;
        }
    }
    return false;
}

bool ChannelPool::removeFromFreeList(ChannelType type, const ChannelDescriptor& desc) {
    auto fit = mFreeByType.find(type);
    if (fit == mFreeByType.end()) {
        return false;
    }
    auto& vec = fit->second;
    auto it = std::find(vec.begin(), vec.end(), desc);
    if (it != vec.end()) {
        vec.erase(it);
        return true;
    }
    return false;
}

} // namespace gsml3parser
