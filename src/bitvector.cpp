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

#include "gsml3parser/bitvector.h"
#include "gsml3parser/arena.h"
#include <algorithm>
#include <iomanip>

namespace gsml3parser {

// ── Shared bit-access helpers ───────────────────────────────────────────

unsigned readBitsImpl(const uint8_t* buf, size_t& rp, unsigned nbits, size_t sz) {
    unsigned result = 0;
    for (unsigned i = 0; i < nbits; ++i) {
        result <<= 1;
        if (rp < sz) {
            size_t byteIdx = rp / 8;
            unsigned bitIdx = 7 - (rp % 8);
            result |= (buf[byteIdx] >> bitIdx) & 1;
        }
        ++rp;
    }
    return result;
}

unsigned peekBitsImpl(const uint8_t* buf, size_t rp, unsigned nbits, size_t sz) {
    unsigned result = 0;
    for (unsigned i = 0; i < nbits; ++i) {
        result <<= 1;
        if (rp < sz) {
            size_t byteIdx = rp / 8;
            unsigned bitIdx = 7 - (rp % 8);
            result |= (buf[byteIdx] >> bitIdx) & 1;
        }
        ++rp;
    }
    return result;
}

unsigned BitView::readField(size_t& rp, unsigned nbits) const {
    return ::gsml3parser::readBitsImpl(mData, rp, nbits, mSize);
}

unsigned BitView::peekField(size_t rp, unsigned nbits) const {
    return ::gsml3parser::peekBitsImpl(mData, rp, nbits, mSize);
}

unsigned BitView::readBit(size_t& rp) const {
    if (rp >= mSize) return 0;
    size_t byteIdx = rp / 8;
    unsigned bitIdx = 7 - (rp % 8);
    ++rp;
    return (mData[byteIdx] >> bitIdx) & 1;
}

// ── BitVector: constructors ────────────────────────────────────────────

BitVector::BitVector() : mRawPtr(nullptr), mSize(0), mWriteEnd(0), mOwned(true) {}

BitVector::BitVector(size_t nbits) : mRawPtr(nullptr), mSize(nbits), mWriteEnd(0), mOwned(true) {
    if (nbits) {
        mBuffer.resize(bitVectorByteSize(nbits), 0);
    }
}

BitVector::BitVector(size_t nbits, unsigned char fill)
    : mRawPtr(nullptr), mSize(nbits), mWriteEnd(0), mOwned(true)
{
    if (nbits) {
        mBuffer.resize(bitVectorByteSize(nbits), fill);
    }
}

BitVector::BitVector(const std::vector<uint8_t>& bytes)
    : mBuffer(bytes), mRawPtr(nullptr), mSize(bytes.size() * 8), mWriteEnd(0), mOwned(true)
{
}

BitVector::BitVector(std::span<const uint8_t> bytes)
    : mBuffer(bytes.begin(), bytes.end()), mRawPtr(nullptr), mSize(bytes.size() * 8), mWriteEnd(0), mOwned(true)
{
}

BitVector::BitVector(Arena& arena, size_t nbits)
    : mSize(nbits), mWriteEnd(0), mOwned(false)
{
    if (nbits) {
        size_t nbytes = bitVectorByteSize(nbits);
        mRawPtr = static_cast<uint8_t*>(arena.allocate(nbytes, alignof(uint8_t)));
        std::memset(mRawPtr, 0, nbytes);
    } else {
        mRawPtr = nullptr;
    }
}

BitVector::BitVector(const BitVector& other)
    : mSize(other.mSize), mWriteEnd(other.mWriteEnd), mOwned(true)
{
    if (other.mSize) {
        size_t nbytes = bitVectorByteSize(other.mSize);
        mBuffer.resize(nbytes);
        std::memcpy(mBuffer.data(), other.data(), nbytes);
    }
}

BitVector::BitVector(BitVector&& other) noexcept
    : mBuffer(std::move(other.mBuffer)), mRawPtr(other.mRawPtr), mSize(other.mSize),
      mWriteEnd(other.mWriteEnd), mOwned(other.mOwned)
{
    other.mRawPtr = nullptr;
    other.mSize = 0;
    other.mWriteEnd = 0;
    other.mOwned = true;
    other.mBuffer.clear();
}

BitVector& BitVector::operator=(const BitVector& other) {
    if (this != &other) {
        // Free owned resources
        mBuffer.clear();
        mRawPtr = nullptr;
        mOwned = true;
        mSize = other.mSize;
        mWriteEnd = other.mWriteEnd;
        if (other.mSize) {
            size_t nbytes = bitVectorByteSize(other.mSize);
            mBuffer.resize(nbytes);
            std::memcpy(mBuffer.data(), other.data(), nbytes);
        }
    }
    return *this;
}

BitVector& BitVector::operator=(BitVector&& other) noexcept {
    if (this != &other) {
        mBuffer = std::move(other.mBuffer);
        mRawPtr = other.mRawPtr;
        mSize = other.mSize;
        mWriteEnd = other.mWriteEnd;
        mOwned = other.mOwned;
        other.mRawPtr = nullptr;
        other.mSize = 0;
        other.mWriteEnd = 0;
        other.mOwned = true;
        other.mBuffer.clear();
    }
    return *this;
}

BitVector::~BitVector() {
    // Arena-allocated buffers are NOT freed (arena owns them).
    // std::vector frees itself automatically.
}

// ── BitVector: resize ──────────────────────────────────────────────────

void BitVector::resize(size_t nbits) {
    if (mOwned) {
        size_t oldBytes = mBuffer.size();
        size_t newBytes = bitVectorByteSize(nbits);
        if (newBytes != oldBytes) {
            mBuffer.resize(newBytes, 0);
        }
        if (newBytes > oldBytes) {
            std::memset(mBuffer.data() + oldBytes, 0, newBytes - oldBytes);
        }
    } else {
        // Arena-allocated: cannot resize (fixed block). Truncate or pad.
        // For safety, just clamp the size to what we allocated.
        // In practice, writeField handles expansion for owned vectors only.
    }
    mSize = nbits;
}

void BitVector::clear() {
    mSize = 0;
}

// ── BitVector: bit access ──────────────────────────────────────────────

unsigned BitVector::readField(size_t& rp, unsigned nbits) const {
    return readBitsImpl(data(), rp, nbits, mSize);
}

void BitVector::writeField(size_t& wp, unsigned value, unsigned nbits) {
    size_t endBit = wp + nbits;
    if (endBit > mSize) {
        resize(endBit);
    }
    if (endBit > mWriteEnd) {
        mWriteEnd = endBit;
    }
    uint8_t* buf = data();
    for (int i = nbits - 1; i >= 0; --i) {
        size_t byteIdx = wp / 8;
        unsigned bitIdx = 7 - (wp % 8);
        if ((value >> i) & 1) {
            buf[byteIdx] |= (1u << bitIdx);
        } else {
            buf[byteIdx] &= ~(1u << bitIdx);
        }
        ++wp;
    }
}

unsigned BitVector::peekField(size_t rp, unsigned nbits) const {
    return peekBitsImpl(data(), rp, nbits, mSize);
}

unsigned BitVector::readBit(size_t& rp) const {
    if (rp >= mSize) return 0;
    const uint8_t* buf = data();
    size_t byteIdx = rp / 8;
    unsigned bitIdx = 7 - (rp % 8);
    ++rp;
    return (buf[byteIdx] >> bitIdx) & 1;
}

void BitVector::writeBit(size_t& wp, bool bit) {
    if (wp + 1 > mSize) {
        resize(wp + 1);
    }
    if (wp + 1 > mWriteEnd) {
        mWriteEnd = wp + 1;
    }
    uint8_t* buf = data();
    size_t byteIdx = wp / 8;
    unsigned bitIdx = 7 - (wp % 8);
    if (bit) {
        buf[byteIdx] |= (1u << bitIdx);
    } else {
        buf[byteIdx] &= ~(1u << bitIdx);
    }
    ++wp;
}

// ── BitVector: segment / clone ─────────────────────────────────────────

BitVector BitVector::segment(size_t offset, size_t nbits) const {
    BitVector result(nbits);
    if (offset + nbits > mSize) nbits = mSize - offset;
    size_t rp = offset;
    size_t wp = 0;
    for (size_t i = 0; i < nbits; ++i) {
        unsigned bit = readField(rp, 1);
        result.writeField(wp, bit, 1);
    }
    return result;
}

BitVector BitVector::clone() const {
    return *this;
}

// ── BitVector: comparison ──────────────────────────────────────────────

bool BitVector::operator==(const BitVector& other) const {
    if (mSize != other.mSize) return false;
    size_t nbytes = bitVectorByteSize(mSize);
    return std::memcmp(data(), other.data(), nbytes) == 0;
}

// ── Stream output ──────────────────────────────────────────────────────

std::ostream& operator<<(std::ostream& os, const BitVector& bv) {
    const uint8_t* buf = bv.data();
    for (size_t i = 0; i < bitVectorByteSize(bv.mSize); ++i) {
        os << std::hex << std::setw(2) << std::setfill('0')
           << static_cast<int>(buf[i]);
    }
    return os;
}

// ── Arena ──────────────────────────────────────────────────────────────

Arena::Arena(size_t initialCapacity) : mBuffer(initialCapacity), mOffset(0) {}

void* Arena::allocate(size_t bytes, size_t alignment) {
    // Align current offset up to `alignment`
    size_t aligned = (mOffset + alignment - 1) & ~(alignment - 1);
    if (aligned + bytes > mBuffer.size()) {
        // Grow the buffer
        size_t newCap = std::max(mBuffer.size() * 2, aligned + bytes);
        mBuffer.resize(newCap);
    }
    void* ptr = mBuffer.data() + aligned;
    mOffset = aligned + bytes;
    return ptr;
}

void Arena::reset() {
    mOffset = 0;
}

} // namespace gsml3parser
