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

#include <cstddef>
#include <cstdint>
#include <vector>
#include <memory>

namespace gsml3parser {

/// Segmented bump (arena) allocator.
///
/// Allocations are carved out of a sequence of fixed-size blocks. Because
/// blocks are never moved or reallocated, every pointer returned by
/// allocate() stays valid until the next reset() or the arena's
/// destruction. This is the key difference from a single growing buffer,
/// whose reallocation would silently invalidate all outstanding pointers.
///
/// Thread safety: NOT thread-safe. One Arena per thread/owner.
/// Memory: blocks are heap-allocated once and reused until reset(). Blocks
/// are default-initialized (not zeroed).
class Arena {
public:
    /// Create an arena whose first block has `initialCapacity` bytes.
    explicit Arena(size_t initialCapacity = 4096);

    Arena(const Arena&) = delete;
    Arena& operator=(const Arena&) = delete;
    Arena(Arena&&) noexcept = default;
    Arena& operator=(Arena&&) noexcept = default;

    /// Carve `bytes` out of the arena, aligned to `alignment`.
    /// @param bytes Number of bytes to allocate (must be > 0).
    /// @param alignment Required alignment; must be a power of two.
    /// @return Pointer to the allocated region, or nullptr if bytes == 0
    ///         or alignment is not a power of two.
    /// @note The pointer remains valid until the next reset() or the
    ///       arena's destruction, even if further allocations follow.
    void* allocate(size_t bytes, size_t alignment = alignof(std::max_align_t));

    /// Release all blocks and reset the usage counter.
    /// All previously returned pointers become invalid.
    void reset();

    /// Bytes remaining in the current (last) block.
    /// Useful for deciding whether to reset the arena before the next
    /// batch of message processing to avoid unbounded growth.
    [[nodiscard]] size_t remaining() const;

    /// Total bytes handed out by allocate() since the last reset().
    [[nodiscard]] size_t used() const noexcept { return mUsed; }

    /// Total capacity across all blocks.
    [[nodiscard]] size_t capacity() const;

private:
    struct Block {
        // Default-initialized storage (NOT zero-filled — audit Q3: the
        // previous std::vector<uint8_t> value-initialized every new block,
        // a wasted 64 KB+ memset per block on the allocation path).
        // NOTE: std::make_unique<uint8_t[]>(n) value-initializes the array
        // (zero-fills it), so the allocation below uses plain new[].
        std::unique_ptr<uint8_t[]> data;
        size_t size{0};
        size_t offset{0};
    };

    /// Minimum block size for newly created blocks (amortizes small allocs).
    static constexpr size_t kMinBlockSize = 65536;

    std::vector<Block> mBlocks;
    size_t mUsed{0};
};

} // namespace gsml3parser
