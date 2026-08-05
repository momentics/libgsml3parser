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
#include <cstring>
#include <iosfwd>
#include <span>
#include <vector>

namespace gsml3parser {

// ── Arena allocator (forward) ───────────────────────────────────────────

class Arena;

// ── BitView: non-owning read-only view over external bytes ──────────────

/**
 * BitView — a zero-copy, read-only view over an external byte buffer.
 * The caller must ensure the underlying buffer outlives the BitView.
 */
class BitView {
public:
    BitView() : mData(nullptr), mSize(0) {}
    BitView(const uint8_t* data, size_t nbits) : mData(data), mSize(nbits) {}
    explicit BitView(std::span<const uint8_t> bytes)
        : mData(bytes.data()), mSize(bytes.size() * 8) {}

    size_t size() const { return mSize; }
    bool empty() const { return mSize == 0; }

    unsigned readField(size_t& rp, unsigned nbits) const;
    unsigned peekField(size_t rp, unsigned nbits) const;
    unsigned readBit(size_t& rp) const;

    const uint8_t* data() const { return mData; }

private:
    const uint8_t* mData;
    size_t mSize;
};

// ── Shared bit-access helpers ───────────────────────────────────────────

unsigned readBitsImpl(const uint8_t* buf, size_t& rp, unsigned nbits, size_t sz);
unsigned peekBitsImpl(const uint8_t* buf, size_t rp, unsigned nbits, size_t sz);

// ── BitVector: owning, resizable bit vector ─────────────────────────────

/**
 * BitVector — a resizable bit vector with MSB-first bit ordering within
 * each octet.  Used as the transport format for all GSM L3 messages.
 *
 * Memory modes:
 *   - Default: std::vector<uint8_t> owns the buffer (HPL-compliant).
 *   - Arena:   arena-allocated raw buffer; BitVector does NOT free it.
 *   - View:    use BitView for zero-copy read-only access.
 *
 * Part of libgsml3parser.
 */
class BitVector {
public:
    // ── Constructors ────────────────────────────────────────────────

    BitVector();
    explicit BitVector(size_t nbits);
    BitVector(size_t nbits, unsigned char fill);

    // Copy from std::vector<uint8_t> (owning)
    BitVector(const std::vector<uint8_t>& bytes);

    // Copy from std::span<const uint8_t> (owning — makes a copy)
    explicit BitVector(std::span<const uint8_t> bytes);

    // Arena-allocated (non-owning — Arena manages lifetime).
    // The arena block is sized to hold `nbits` bits.
    BitVector(Arena& arena, size_t nbits);

    // Default copy/move — vector handles them automatically for owned buffers.
    // Arena-allocated BitVectors are NOT copyable/movable (would break ownership).
    BitVector(const BitVector& other);
    BitVector(BitVector&& other) noexcept;
    BitVector& operator=(const BitVector& other);
    BitVector& operator=(BitVector&& other) noexcept;

    ~BitVector();

    // ── Capacity ────────────────────────────────────────────────────

    size_t size() const { return mSize; }
    bool empty() const { return mSize == 0; }
    void resize(size_t nbits);
    void clear();
    size_t writeEnd() const { return mWriteEnd; }

    // ── Bit access ──────────────────────────────────────────────────

    unsigned readField(size_t& rp, unsigned nbits) const;
    void writeField(size_t& wp, unsigned value, unsigned nbits);
    unsigned peekField(size_t rp, unsigned nbits) const;

    unsigned readBit(size_t& rp) const;
    void writeBit(size_t& wp, bool bit);

    // ── Byte access ─────────────────────────────────────────────────

    const uint8_t* data() const { return mOwned ? mBuffer.data() : mRawPtr; }
    uint8_t*       data()       { return mOwned ? mBuffer.data() : mRawPtr; }

    // ── Segment / clone ─────────────────────────────────────────────

    BitVector segment(size_t offset, size_t nbits) const;
    BitVector clone() const;

    // ── Comparison ──────────────────────────────────────────────────

    bool operator==(const BitVector& other) const;
    bool operator!=(const BitVector& other) const { return !(*this == other); }

    // ── Create a non-owning read-only view ──────────────────────────

    BitView view() const { return BitView(data(), mSize); }

    // ── Reuse: reset size to 0 but keep capacity (arena-like reuse) ──

    void reset() {
        mSize = 0;
        mWriteEnd = 0;
        if (mOwned && !mBuffer.empty()) {
            std::memset(mBuffer.data(), 0, mBuffer.size());
        } else if (!mOwned && mRawPtr) {
            // Can't clear without knowing allocation size; caller's responsibility.
        }
    }

private:
    std::vector<uint8_t> mBuffer;  // used when mOwned == true
    uint8_t* mRawPtr;              // used when mOwned == false (arena)
    size_t mSize;                  // number of bits
    mutable size_t mWriteEnd;      // tracks highest write position
    bool mOwned;                   // true = vector owns, false = arena/raw

    friend class L3Frame;
    friend std::ostream& operator<<(std::ostream& os, const BitVector& bv);
};

[[nodiscard]] constexpr size_t bitVectorByteSize(size_t nbits) {
    return (nbits + 7) / 8;
}

std::ostream& operator<<(std::ostream& os, const BitVector& bv);

} // namespace gsml3parser


