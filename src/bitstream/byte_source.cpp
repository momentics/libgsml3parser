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

#include "gsml3parser/bitstream/byte_source.h"
#include <atomic>
#include <cstring>

namespace gsml3parser {

// ── SpanByteSource ─────────────────────────────────────────────────────

SpanByteSource::SpanByteSource(std::span<const uint8_t> data)
    : mData(data), mPos(0) {}

size_t SpanByteSource::read(uint8_t* buf, size_t maxSize) {
    if (mPos >= mData.size()) return 0;
    size_t avail = mData.size() - mPos;
    size_t n = (maxSize < avail) ? maxSize : avail;
    std::memcpy(buf, mData.data() + mPos, n);
    mPos += n;
    return n;
}

// ── FileByteSource ─────────────────────────────────────────────────────

FileByteSource::FileByteSource(std::FILE* f)
    : mFile(f) {}

FileByteSource::~FileByteSource() {
    if (mFile) std::fclose(mFile);
}

size_t FileByteSource::read(uint8_t* buf, size_t maxSize) {
    if (!mFile) return 0;
    return std::fread(buf, 1, maxSize, mFile);
}

// ── RingBuffer ─────────────────────────────────────────────────────────

RingBuffer::RingBuffer(size_t capacity) {
    // Round up to next power of two, minimum 2.
    size_t cap = capacity;
    if (cap < 2) cap = 2;
    cap--;
    cap |= cap >> 1;
    cap |= cap >> 2;
    cap |= cap >> 4;
    cap |= cap >> 8;
    cap |= cap >> 16;
#if defined(_WIN64) || defined(__x86_64__) || defined(__aarch64__)
    cap |= cap >> 32;
#endif
    cap++;

    mMask = cap - 1;           // bitmask for wrap-around
    mCapacity = cap - 1;       // effective capacity (sacrifice one slot)
    mBuf.resize(cap, 0);       // physical size = cap
}

size_t RingBuffer::write(const uint8_t* data, size_t len) {
    size_t free = freeSpace();
    if (free == 0) return 0;
    if (len > free) len = free;

    // Write in at most two segments (linear region, then wrap).
    // Bitmask wrap: O(1) bitwise AND vs 20-80 cycle modulo.
    size_t head = mHead.load(std::memory_order_relaxed);
    size_t first = (mMask + 1) - head;  // bytes until end of buffer
    if (first > len) first = len;
    std::memcpy(mBuf.data() + head, data, first);
    head = (head + first) & mMask;       // bitmask wrap

    size_t second = len - first;
    if (second > 0) {
        std::memcpy(mBuf.data() + head, data + first, second);
        head = (head + second) & mMask;  // bitmask wrap
    }
    // Release: ensures all memcpy stores to mBuf are visible before mHead update.
    mHead.store(head, std::memory_order_release);
    return len;
}

size_t RingBuffer::available() const noexcept {
    // Acquire on both loads: the reader (consumer) needs to see the latest
    // mHead published by the producer, and the writer (producer) needs to see
    // the latest mTail published by the consumer.
    size_t head = mHead.load(std::memory_order_acquire);
    size_t tail = mTail.load(std::memory_order_acquire);
    return head >= tail ? (head - tail) : ((mMask + 1) - tail + head);
}

size_t RingBuffer::freeSpace() const noexcept {
    return mCapacity - available();  // mCapacity = effective capacity
}

size_t RingBuffer::read(uint8_t* buf, size_t maxSize) {
    size_t avail = available();
    if (avail == 0) return 0;
    if (maxSize > avail) maxSize = avail;

    // Read in at most two segments.
    // Bitmask wrap: O(1) bitwise AND vs 20-80 cycle modulo.
    size_t tail = mTail.load(std::memory_order_relaxed);
    size_t first = (mMask + 1) - tail;  // bytes until end of buffer
    if (first > maxSize) first = maxSize;
    std::memcpy(buf, mBuf.data() + tail, first);
    tail = (tail + first) & mMask;       // bitmask wrap

    size_t second = maxSize - first;
    if (second > 0) {
        std::memcpy(buf + first, mBuf.data() + tail, second);
        tail = (tail + second) & mMask;  // bitmask wrap
    }
    // Release: ensures all memcpy loads from mBuf complete before mTail update.
    mTail.store(tail, std::memory_order_release);
    return maxSize;
}

} // namespace gsml3parser
