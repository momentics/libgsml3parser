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
#include <span>
#include <vector>
#include <cstdio>

namespace gsml3parser {

/**
 * Abstract byte source for streaming I/O.
 *
 * Subclasses provide bytes from different origins: memory spans, files,
 * ring buffers, network sockets, etc.  The L3Framer reads from this
 * interface to extract frame boundaries.
 */
class ByteSource {
public:
    virtual ~ByteSource() = default;

    /**
     * Read up to @p maxSize bytes into @p buf.
     * @return Number of bytes actually read.  Zero indicates EOF.
     *         May block if the underlying source has no data yet.
     */
    [[nodiscard]] virtual size_t read(uint8_t* buf, size_t maxSize) = 0;
};

/**
 * Reads from a contiguous memory span (in-memory buffer).
 * Non-blocking; returns remaining bytes or zero at EOF.
 */
class SpanByteSource : public ByteSource {
    std::span<const uint8_t> mData;
    size_t mPos{};

public:
    explicit SpanByteSource(std::span<const uint8_t> data);

    [[nodiscard]] size_t read(uint8_t* buf, size_t maxSize) override;

    /** Remaining unread bytes in the span. */
    [[nodiscard]] size_t remaining() const noexcept { return mData.size() - mPos; }
};

/**
 * Reads from a C FILE*.  Wraps fread().
 * Takes ownership of the FILE* and closes it on destruction.
 */
class FileByteSource : public ByteSource {
    std::FILE* mFile;

public:
    explicit FileByteSource(std::FILE* f);
    ~FileByteSource() override;

    // Non-copyable, non-movable (owns the FILE*).
    FileByteSource(const FileByteSource&) = delete;
    FileByteSource& operator=(const FileByteSource&) = delete;

    [[nodiscard]] size_t read(uint8_t* buf, size_t maxSize) override;
};

/**
 * Lock-free-ish ring buffer for producer/consumer streaming.
 *
 * Intended use: an SDR or network callback writes bytes via write(),
 * and the L3Framer / L3StreamProcessor reads them via read().
 *
 * Single-producer, single-consumer pattern is fully safe without locks.
 * Multi-producer or multi-consumer requires external synchronization.
 */
class RingBuffer : public ByteSource {
    std::vector<uint8_t> mBuf;
    size_t mHead{};   // write position
    size_t mTail{};   // read position
    size_t mCapacity{};

public:
    explicit RingBuffer(size_t capacity = 262144);

    /**
     * Write up to @p len bytes from @p data into the buffer.
     * Non-blocking.  Returns number of bytes actually accepted.
     * If the buffer is full, returns 0 (producer must wait).
     */
    size_t write(const uint8_t* data, size_t len);

    /** Number of bytes available for reading. */
    [[nodiscard]] size_t available() const noexcept;

    /** Number of free slots in the buffer. */
    [[nodiscard]] size_t freeSpace() const noexcept;

    [[nodiscard]] size_t read(uint8_t* buf, size_t maxSize) override;
};

} // namespace gsml3parser
