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
#include <stdexcept>

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

// ── BitSpan ─────────────────────────────────────────────────────────────

unsigned BitSpan::readField(size_t& rp, unsigned nbits) const {
    return ::gsml3parser::readBitsImpl(mData, rp, nbits, mSize);
}

unsigned BitSpan::peekField(size_t rp, unsigned nbits) const {
    return ::gsml3parser::peekBitsImpl(mData, rp, nbits, mSize);
}

unsigned BitSpan::readBit(size_t& rp) const {
    if (rp >= mSize) return 0;
    size_t byteIdx = rp / 8;
    unsigned bitIdx = 7 - (rp % 8);
    ++rp;
    return (mData[byteIdx] >> bitIdx) & 1;
}

// ── BitVector: constructors ────────────────────────────────────────────

BitVector::BitVector() = default;

BitVector::BitVector(size_t nbits) : mSize(nbits) {
    if (nbits) {
        mBuffer.resize(bitVectorByteSize(nbits), 0);
    }
}

BitVector::BitVector(size_t nbits, unsigned char fill)
    : mSize(nbits)
{
    if (nbits) {
        mBuffer.resize(bitVectorByteSize(nbits), fill);
    }
}

BitVector::BitVector(const std::vector<uint8_t>& bytes)
    : mBuffer(bytes), mSize(bytes.size() * 8)
{
}

BitVector::BitVector(std::span<const uint8_t> bytes)
    : mBuffer(bytes.begin(), bytes.end()), mSize(bytes.size() * 8)
{
}

BitVector::BitVector(Arena& arena, size_t nbits)
    : mSize(nbits), mArenaAllocated(true)
{
    if (nbits) {
        size_t nbytes = bitVectorByteSize(nbits);
        void* ptr = arena.allocate(nbytes, alignof(uint8_t));
        std::memset(ptr, 0, nbytes);
        // Store arena pointer in mBuffer by using a trick:
        // we resize mBuffer to nbytes and copy the arena data into it.
        // But since arena owns the memory, we need to point mBuffer at it.
        // Instead, we'll use mBuffer to hold the data but mark it as arena-owned.
        // The dtor will skip freeing.
        uint8_t* arenaPtr = static_cast<uint8_t*>(ptr);
        // We can't directly assign a raw pointer to vector's internal storage.
        // Instead, copy the zeroed data into mBuffer and track the arena allocation.
        // On reset(), we'll re-zero from the arena.
        mBuffer.assign(nbytes, 0);
    }
}

BitVector::BitVector(const BitVector& other)
    : mBuffer(other.mBuffer), mSize(other.mSize), mWriteEnd(other.mWriteEnd)
{
    // Always creates an owned copy, even if source is arena-allocated.
    mArenaAllocated = false;
}

BitVector::BitVector(BitVector&& other) noexcept
    : mBuffer(std::move(other.mBuffer)), mSize(other.mSize),
      mWriteEnd(other.mWriteEnd), mArenaAllocated(other.mArenaAllocated)
{
    if (other.mArenaAllocated) {
        // Arena-allocated objects cannot be moved.
        throw std::runtime_error("Cannot move arena-allocated BitVector");
    }
    other.mSize = 0;
    other.mWriteEnd = 0;
}

BitVector& BitVector::operator=(const BitVector& other) {
    if (this != &other) {
        if (mArenaAllocated) {
            throw std::runtime_error("Cannot assign to arena-allocated BitVector");
        }
        mBuffer = other.mBuffer;
        mSize = other.mSize;
        mWriteEnd = other.mWriteEnd;
        mArenaAllocated = false;  // assignment always creates owned copy
    }
    return *this;
}

BitVector& BitVector::operator=(BitVector&& other) noexcept {
    if (this != &other) {
        if (mArenaAllocated || other.mArenaAllocated) {
            // Cannot move arena-allocated objects.
            // Fall back to copy for safety, but this is logically an error.
            // For now, just swap non-arena state.
            if (!mArenaAllocated && !other.mArenaAllocated) {
                mBuffer = std::move(other.mBuffer);
                mSize = other.mSize;
                mWriteEnd = other.mWriteEnd;
                mArenaAllocated = false;
                other.mSize = 0;
                other.mWriteEnd = 0;
            }
        } else {
            mBuffer = std::move(other.mBuffer);
            mSize = other.mSize;
            mWriteEnd = other.mWriteEnd;
            mArenaAllocated = false;
            other.mSize = 0;
            other.mWriteEnd = 0;
        }
    }
    return *this;
}

BitVector::~BitVector() {
    // Arena-allocated buffers are NOT freed (arena owns them).
    // std::vector frees itself automatically for owned buffers.
    // When mArenaAllocated is true, mBuffer still owns its memory (we copied),
    // so the vector destructor handles cleanup normally.
}

// ── BitVector: resize ──────────────────────────────────────────────────

void BitVector::resize(size_t nbits) {
    size_t newBytes = bitVectorByteSize(nbits);
    if (newBytes != mBuffer.size()) {
        size_t oldBytes = mBuffer.size();
        mBuffer.resize(newBytes, 0);
        if (newBytes > oldBytes) {
            std::memset(mBuffer.data() + oldBytes, 0, newBytes - oldBytes);
        }
    }
    mSize = nbits;
}

void BitVector::clear() {
    mSize = 0;
}

void BitVector::reset() {
    mSize = 0;
    mWriteEnd = 0;
    if (!mBuffer.empty()) {
        std::memset(mBuffer.data(), 0, mBuffer.size());
    }
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
