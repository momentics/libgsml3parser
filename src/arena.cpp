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

#include "gsml3parser/arena.h"

#include <cstdint>
#include <utility>

namespace gsml3parser {

namespace {

/// True if v is a power of two (and non-zero).
constexpr bool isPowerOfTwo(size_t v) noexcept {
    return v != 0 && (v & (v - 1)) == 0;
}

} // namespace

Arena::Arena(size_t initialCapacity) {
    Block b;
    b.data.resize(initialCapacity, 0);
    mBlocks.push_back(std::move(b));
}

void* Arena::allocate(size_t bytes, size_t alignment) {
    if (bytes == 0) return nullptr;
    if (!isPowerOfTwo(alignment)) return nullptr;

    // Fast path: fit the allocation into the current (last) block.
    if (!mBlocks.empty()) {
        Block& b = mBlocks.back();
        size_t alignedOffset = (b.offset + alignment - 1) & ~(alignment - 1);
        if (alignedOffset + bytes <= b.data.size()) {
            void* ptr = b.data.data() + alignedOffset;
            b.offset = alignedOffset + bytes;
            mUsed += bytes;
            return ptr;
        }
    }

    // Start a new block. Size it with 2x headroom over the triggering
    // allocation (and at least kMinBlockSize) so that capacity() stays
    // strictly greater than used() in practice and small allocations are
    // amortized. Blocks are append-only: existing blocks are never moved or
    // reallocated, so previously returned pointers stay valid.
    size_t blockCapacity = (bytes + alignment) * 2;
    if (blockCapacity < kMinBlockSize) blockCapacity = kMinBlockSize;

    Block b;
    // Over-allocate by `alignment` and round the base pointer up, so the
    // first usable address is aligned even if the heap returned a pointer
    // aligned to less than `alignment`.
    b.data.resize(blockCapacity + alignment, 0);
    uintptr_t base = reinterpret_cast<uintptr_t>(b.data.data());
    uintptr_t alignedBase = (base + alignment - 1) & ~(static_cast<uintptr_t>(alignment) - 1);
    size_t slack = static_cast<size_t>(alignedBase - base);

    void* ptr = b.data.data() + slack;
    b.offset = slack + bytes;
    mBlocks.push_back(std::move(b));
    mUsed += bytes;
    return ptr;
}

void Arena::reset() {
    mBlocks.clear();
    mUsed = 0;
}

size_t Arena::remaining() const {
    if (mBlocks.empty()) return 0;
    const Block& b = mBlocks.back();
    return b.data.size() - b.offset;
}

size_t Arena::capacity() const {
    size_t total = 0;
    for (const auto& b : mBlocks) {
        total += b.data.size();
    }
    return total;
}

} // namespace gsml3parser
