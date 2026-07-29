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
#include <cstring>
#include <algorithm>
#include <iomanip>

namespace gsml3parser {

BitVector::BitVector() : mStart(nullptr), mSize(0), mAlloc(0) {}

BitVector::BitVector(size_t nbits) : mStart(nullptr), mSize(0), mAlloc(0) {
    alloc(nbits);
    std::memset(mStart, 0, bitVectorByteSize(nbits));
}

BitVector::BitVector(size_t nbits, unsigned char fill)
    : mStart(nullptr), mSize(0), mAlloc(0)
{
    alloc(nbits);
    std::memset(mStart, fill, bitVectorByteSize(nbits));
}

BitVector::BitVector(const BitVector& other)
    : mStart(nullptr), mSize(0), mAlloc(0)
{
    alloc(other.mSize);
    std::memcpy(mStart, other.mStart, bitVectorByteSize(other.mSize));
}

BitVector::BitVector(BitVector&& other) noexcept
    : mStart(other.mStart), mSize(other.mSize), mAlloc(other.mAlloc)
{
    other.mStart = nullptr;
    other.mSize = 0;
    other.mAlloc = 0;
}

BitVector::BitVector(const std::vector<uint8_t>& bytes)
    : mStart(nullptr), mSize(bytes.size() * 8), mAlloc(0)
{
    alloc(bytes.size() * 8);
    std::memcpy(mStart, bytes.data(), bytes.size());
}

BitVector::~BitVector() {
    dealloc();
}

BitVector& BitVector::operator=(const BitVector& other) {
    if (this != &other) {
        dealloc();
        alloc(other.mSize);
        std::memcpy(mStart, other.mStart, bitVectorByteSize(other.mSize));
    }
    return *this;
}

BitVector& BitVector::operator=(BitVector&& other) noexcept {
    if (this != &other) {
        dealloc();
        mStart = other.mStart;
        mSize = other.mSize;
        mAlloc = other.mAlloc;
        other.mStart = nullptr;
        other.mSize = 0;
        other.mAlloc = 0;
    }
    return *this;
}

void BitVector::alloc(size_t nbits) {
    mAlloc = nbits;
    mStart = static_cast<uint8_t*>(std::malloc(bitVectorByteSize(nbits)));
    if (!mStart) throw std::bad_alloc();
}

void BitVector::dealloc() {
    if (mStart) {
        std::free(mStart);
        mStart = nullptr;
    }
    mSize = 0;
    mAlloc = 0;
}

void BitVector::resize(size_t nbits) {
    BitVector tmp(nbits);
    size_t copyBits = std::min(nbits, mSize);
    if (copyBits > 0 && mStart) {
        std::memcpy(tmp.mStart, mStart, bitVectorByteSize(copyBits));
    }
    dealloc();
    mStart = tmp.mStart;
    mSize = nbits;
    mAlloc = nbits;
}

void BitVector::clear() {
    mSize = 0;
}

unsigned BitVector::readField(size_t& rp, unsigned nbits) const {
    unsigned result = 0;
    for (unsigned i = 0; i < nbits; ++i) {
        result <<= 1;
        if (rp < mSize) {
            size_t byteIdx = rp / 8;
            unsigned bitIdx = 7 - (rp % 8);
            result |= (mStart[byteIdx] >> bitIdx) & 1;
        }
        ++rp;
    }
    return result;
}

void BitVector::writeField(size_t& wp, unsigned value, unsigned nbits) const {
    for (int i = nbits - 1; i >= 0; --i) {
        if (wp < mSize) {
            size_t byteIdx = wp / 8;
            unsigned bitIdx = 7 - (wp % 8);
            if ((value >> i) & 1) {
                mStart[byteIdx] |= (1u << bitIdx);
            } else {
                mStart[byteIdx] &= ~(1u << bitIdx);
            }
        }
        ++wp;
    }
}

unsigned BitVector::peekField(size_t rp, unsigned nbits) const {
    unsigned result = 0;
    for (unsigned i = 0; i < nbits; ++i) {
        result <<= 1;
        if (rp < mSize) {
            size_t byteIdx = rp / 8;
            unsigned bitIdx = 7 - (rp % 8);
            result |= (mStart[byteIdx] >> bitIdx) & 1;
        }
        ++rp;
    }
    return result;
}

unsigned BitVector::readBit(size_t& rp) const {
    if (rp >= mSize) return 0;
    size_t byteIdx = rp / 8;
    unsigned bitIdx = 7 - (rp % 8);
    ++rp;
    return (mStart[byteIdx] >> bitIdx) & 1;
}

void BitVector::writeBit(size_t& wp, bool bit) const {
    if (wp < mSize) {
        size_t byteIdx = wp / 8;
        unsigned bitIdx = 7 - (wp % 8);
        if (bit) {
            mStart[byteIdx] |= (1u << bitIdx);
        } else {
            mStart[byteIdx] &= ~(1u << bitIdx);
        }
    }
    ++wp;
}

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
    return BitVector(*this);
}

bool BitVector::operator==(const BitVector& other) const {
    if (mSize != other.mSize) return false;
    return std::memcmp(mStart, other.mStart, bitVectorByteSize(mSize)) == 0;
}

std::ostream& operator<<(std::ostream& os, const BitVector& bv) {
    for (size_t i = 0; i < bitVectorByteSize(bv.mSize); ++i) {
        os << std::hex << std::setw(2) << std::setfill('0')
           << static_cast<int>(bv.mStart[i]);
    }
    return os;
}

} // namespace gsml3parser
