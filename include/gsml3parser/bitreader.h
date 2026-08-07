// Copyright (c) 2026 gsml3parser contributors
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

#include "expected.h"
#include <cstddef>
#include <cstdint>
#include <span>
#include <cstring>

namespace gsml3parser {

class BitReader {
public:
    constexpr BitReader(const uint8_t* buf, size_t nbits) noexcept
        : mBuf(buf), mTotalBits(nbits), mPos(0), mByteIndex(0), mBitOffset(0) {}

    [[nodiscard]] Expected<uint32_t> readField(unsigned nbits) & {
        if (nbits == 0) return Expected<uint32_t>::hold(0u);
        if (nbits > 32) return Expected<uint32_t>::error(ParseError{ParseError::Code::InvalidValue, "field too large"});
        if (mPos + nbits > mTotalBits) {
            return Expected<uint32_t>::error(
                ParseError{ParseError::Code::TruncatedInput, "read past end", mPos});
        }

        uint32_t val = 0;
        size_t pos = mPos;
        for (unsigned i = 0; i < nbits; ++i) {
            size_t bi = pos + i;
            size_t byteIdx = bi / 8;
            unsigned bitOff = 7u - static_cast<unsigned>(bi % 8);
            val = (val << 1) | ((mBuf[byteIdx] >> bitOff) & 1u);
        }
        mPos += nbits;
        mByteIndex = mPos / 8;
        mBitOffset = static_cast<unsigned>(mPos % 8);
        return Expected<uint32_t>::hold(val);
    }

    [[nodiscard]] uint32_t peekField(unsigned nbits) const {
        if (nbits == 0) return 0u;
        if (nbits > 32) nbits = 32;

        uint32_t val = 0;
        size_t limit = mPos + nbits;
        if (limit > mTotalBits) limit = mTotalBits;

        for (size_t bi = mPos; bi < limit; ++bi) {
            size_t byteIdx = bi / 8;
            unsigned bitOff = 7u - static_cast<unsigned>(bi % 8);
            val = (val << 1) | ((mBuf[byteIdx] >> bitOff) & 1u);
        }
        while (limit < mPos + nbits) {
            val <<= 1;
            ++limit;
        }
        return val;
    }

    [[nodiscard]] bool hasMore() const noexcept {
        return mPos < mTotalBits;
    }

    [[nodiscard]] size_t remainingBits() const noexcept {
        return mTotalBits - mPos;
    }

    [[nodiscard]] size_t position() const noexcept {
        return mPos;
    }

    void alignToOctet() noexcept {
        if (mPos % 8 != 0) {
            mPos += 8 - (mPos % 8);
            if (mPos > mTotalBits) mPos = mTotalBits;
        }
        mByteIndex = mPos / 8;
        mBitOffset = static_cast<unsigned>(mPos % 8);
    }

    [[nodiscard]] Expected<void> readBytes(uint8_t* out, size_t count) & {
        if (count == 0) return Expected<void>::hold();
        if (mPos / 8 + count > (mTotalBits + 7) / 8) {
            return Expected<void>::error(
                ParseError{ParseError::Code::TruncatedInput, "read past end", mPos});
        }

        if (mBitOffset == 0) {
            std::memcpy(out, mBuf + mByteIndex, count);
            mPos += count * 8;
            mByteIndex += count;
        } else {
            for (size_t i = 0; i < count; ++i) {
                auto res = readField(8);
                if (!res) return Expected<void>::error(res.error());
                out[i] = static_cast<uint8_t>(res.value());
            }
        }
        return Expected<void>::hold();
    }

private:
    const uint8_t* mBuf;
    size_t mTotalBits;
    size_t mPos;
    size_t mByteIndex;
    unsigned mBitOffset;
};

} // namespace gsml3parser
