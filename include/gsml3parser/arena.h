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

namespace gsml3parser {

/**
 * Arena — a simple bump allocator for high-throughput parsing.
 *
 * Allocated blocks must NOT be individually freed; call reset() to reclaim
 * the entire arena.  Each thread should use its own Arena instance.
 */
class Arena {
public:
    explicit Arena(size_t initialCapacity = 4096);

    /**
     * Allocate `bytes` bytes with at least `alignment` alignment (power of two).
     * Returns nullptr if the arena cannot satisfy the request.
     */
    void* allocate(size_t bytes, size_t alignment = alignof(std::max_align_t));

    /** Reset the arena — all previous allocations become invalid. */
    void reset();

    /** Bytes consumed so far. */
    size_t used() const { return mOffset; }

    /** Total capacity (bytes). */
    size_t capacity() const { return mBuffer.size(); }

private:
    std::vector<uint8_t> mBuffer;
    size_t mOffset = 0;
};

} // namespace gsml3parser


