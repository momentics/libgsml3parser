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

#ifndef GSML3PARSER_BITVECTOR_H
#define GSML3PARSER_BITVECTOR_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iosfwd>
#include <initializer_list>
#include <vector>

namespace gsml3parser {

/**
 * BitVector — a resizable bit vector with MSB-first bit ordering within
 * each octet.  Used as the transport format for all GSM L3 messages.
 *
 * Part of libgsml3parser.
 */
class BitVector {
public:
    BitVector();
    explicit BitVector(size_t nbits);
    BitVector(size_t nbits, unsigned char fill);
    BitVector(const BitVector& other);
    BitVector(BitVector&& other) noexcept;
    BitVector(const std::vector<uint8_t>& bytes);
    ~BitVector();

    BitVector& operator=(const BitVector& other);
    BitVector& operator=(BitVector&& other) noexcept;

    // ── Capacity ────────────────────────────────────────────────────

    size_t size() const { return mSize; }
    bool empty() const { return mSize == 0; }
    void resize(size_t nbits);
    void clear();

    // ── Bit access ──────────────────────────────────────────────────

    unsigned readField(size_t& rp, unsigned nbits) const;
    void writeField(size_t& wp, unsigned value, unsigned nbits) const;
    unsigned peekField(size_t rp, unsigned nbits) const;

    bool readBit(size_t& rp) const;
    void writeBit(size_t& wp, bool bit) const;

    // ── Byte access ─────────────────────────────────────────────────

    const uint8_t* data() const { return mStart; }
    uint8_t*       data()       { return mStart; }

    // ── Segment / clone ─────────────────────────────────────────────

    BitVector segment(size_t offset, size_t nbits) const;
    BitVector clone() const;

    // ── Comparison ──────────────────────────────────────────────────

    bool operator==(const BitVector& other) const;
    bool operator!=(const BitVector& other) const { return !(*this == other); }

private:
    uint8_t* mStart;
    size_t mSize;    // number of bits
    size_t mAlloc;   // allocated bits

    void alloc(size_t nbits);
    void dealloc();
};

inline size_t bitVectorByteSize(size_t nbits) {
    return (nbits + 7) / 8;
}

std::ostream& operator<<(std::ostream& os, const BitVector& bv);

} // namespace gsml3parser

#endif // GSML3PARSER_BITVECTOR_H
