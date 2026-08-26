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

    /// Load up to \p n bytes from \p start (capped by \p totalBytes) into a
    /// big-endian uint32_t.  Sets \p *actual to the number of bytes loaded.
    /// Bytes are packed at the MSB of the returned value.
    [[nodiscard]] static constexpr uint32_t loadN(const uint8_t* buf,
                                                   size_t start, size_t totalBytes,
                                                   unsigned n, unsigned& actual) noexcept {
        actual = 0;
        uint32_t val = 0;
        unsigned s = static_cast<unsigned>(start);
        unsigned e = s + n;
        unsigned m = static_cast<unsigned>(totalBytes);
        if (e > m) e = m;
        for (unsigned i = s; i < e; ++i) {
            val = (val << 8) | buf[i];
            ++actual;
        }
        return val;
    }

    /// Number of bytes spanning a field of \p nbits starting at bit offset
    /// \p bitOff within the first byte (0 = MSB).
    [[nodiscard]] static constexpr unsigned fieldSpanBytes(unsigned bitOff,
                                                            unsigned nbits) noexcept {
        if (nbits == 0) return 0;
        return (bitOff + nbits - 1) / 8 + 1;
    }

    [[nodiscard]] Expected<uint32_t> readField(unsigned nbits) & {
        if (nbits == 0) return Expected<uint32_t>::hold(0u);
        if (!mBuf) return Expected<uint32_t>::error(
            ParseError{ParseError::Code::TruncatedInput, "null buffer", mPos});
        if (nbits > 32) return Expected<uint32_t>::error(ParseError{ParseError::Code::InvalidValue, "field too large"});
        if (mPos + nbits > mTotalBits) {
            return Expected<uint32_t>::error(
                ParseError{ParseError::Code::TruncatedInput, "read past end", mPos});
        }

        uint32_t val;
        size_t totalBytes = mBuf ? (mTotalBits + 7) / 8 : 0;
        uint32_t mask = ~0u >> (32 - nbits);

        if (mBitOffset == 0) {
            // ── Byte-aligned: load ceil(nbits/8) bytes, extract top nbits -> LSB ──
            unsigned toLoad = fieldSpanBytes(0, nbits);
            if (toLoad > 4) toLoad = 4;
            unsigned actual;
            uint32_t loaded = loadN(mBuf, mByteIndex, totalBytes, toLoad, actual);
            // 'actual' bytes at MSB of loaded. Shift right so top nbits land at LSB.
            val = (loaded >> (actual * 8 - nbits)) & mask;
        } else {
            // ── Misaligned: load bytes spanning the field ──
            unsigned needBytes = fieldSpanBytes(mBitOffset, nbits);
            unsigned availBytes = remainingBytes();

            if (availBytes >= needBytes && needBytes <= 4) {
                unsigned actual;
                uint32_t loaded = loadN(mBuf, mByteIndex, totalBytes, needBytes, actual);
                // 'actual' bytes packed at MSB. Field starts at bit mBitOffset of first byte.
                // Shift right so the field lands at LSB.
                val = (loaded >> (actual * 8 - mBitOffset - nbits)) & mask;
            } else if (mBitOffset + nbits <= 32) {
                // Load exactly the bytes the field spans (at most 4). This is
                // always in-bounds: mPos + nbits <= mTotalBits guarantees the
                // spanned bytes exist. Using the full span (not the remaining
                // byte count) keeps the right-shift amount non-negative even
                // when the field ends exactly at the last bit of the buffer.
                unsigned toLoad = fieldSpanBytes(mBitOffset, nbits);
                if (toLoad > 4) toLoad = 4;
                unsigned actual;
                uint32_t loaded = loadN(mBuf, mByteIndex, totalBytes, toLoad, actual);
                val = (loaded >> (actual * 8 - mBitOffset - nbits)) & mask;
            } else {
                // Fallback: bit-by-bit.
                size_t pos = mPos;
                val = 0;
                for (unsigned i = 0; i < nbits; ++i) {
                    size_t bi = pos + i;
                    val = (val << 1) | ((mBuf[bi / 8] >> (7u - static_cast<unsigned>(bi % 8))) & 1u);
                }
            }
        }

        mPos += nbits;
        mByteIndex = mPos / 8;
        mBitOffset = static_cast<unsigned>(mPos % 8);
        return Expected<uint32_t>::hold(val);
    }

    /// Peek at the next \p nbits bits without advancing the position.
    /// Returns the value right-aligned, zero-extended if fewer than \p nbits
    /// bits remain. \p nbits is clamped to 32 (the return type is uint32_t),
    /// so requesting more than 32 bits is safe (no undefined behavior).
    /// Returns 0 when no bits are available (empty/null buffer).
    [[nodiscard]] uint32_t peekField(unsigned nbits) const {
        if (nbits == 0) return 0u;
        // peekField returns uint32_t, so it can never expose more than 32 bits.
        // Clamp nbits to 32 up-front so every right/left shift below has a
        // non-negative count (a shift count >= the type width is undefined behavior).
        if (nbits > 32) nbits = 32;

        size_t limit = mPos + nbits;
        if (limit > mTotalBits) limit = mTotalBits;
        unsigned actualNbits = static_cast<unsigned>(limit - mPos);
        if (actualNbits == 0) return 0u;

        size_t totalBytes = mBuf ? (mTotalBits + 7) / 8 : 0;
        uint32_t mask = ~0u >> (32 - actualNbits);

        if (mBitOffset == 0) {
            unsigned toLoad = fieldSpanBytes(0, actualNbits);
            if (toLoad > 4) toLoad = 4;
            unsigned actual;
            uint32_t loaded = loadN(mBuf, mByteIndex, totalBytes, toLoad, actual);
            if (actual == 0) return 0u;   // no bytes available (null/empty buffer) -> avoid negative shift
            uint32_t val = (loaded >> (actual * 8 - actualNbits)) & mask;
            return val << (nbits - actualNbits);
        }

        unsigned needBytes = fieldSpanBytes(mBitOffset, actualNbits);
        unsigned availBytes = remainingBytes();

        if (availBytes >= needBytes && needBytes <= 4) {
            unsigned actual;
            uint32_t loaded = loadN(mBuf, mByteIndex, totalBytes, needBytes, actual);
            if (actual == 0) return 0u;   // no bytes available (null/empty buffer) -> avoid negative shift
            uint32_t val = (loaded >> (actual * 8 - mBitOffset - actualNbits)) & mask;
            return val << (nbits - actualNbits);
        } else if (mBitOffset + actualNbits <= 32) {
            // Load the full spanned bytes (see readField for the in-bounds proof).
            unsigned toLoad = fieldSpanBytes(mBitOffset, actualNbits);
            if (toLoad > 4) toLoad = 4;
            unsigned actual;
            uint32_t loaded = loadN(mBuf, mByteIndex, totalBytes, toLoad, actual);
            if (actual == 0) return 0u;   // no bytes available (null/empty buffer) -> avoid negative shift
            uint32_t val = (loaded >> (actual * 8 - mBitOffset - actualNbits)) & mask;
            return val << (nbits - actualNbits);
        }

        // Fallback: bit-by-bit.
        uint32_t val = 0;
        for (size_t bi = mPos; bi < limit; ++bi) {
            val = (val << 1) | ((mBuf[bi / 8] >> (7u - static_cast<unsigned>(bi % 8))) & 1u);
        }
        return val << (nbits - actualNbits);
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
        if (!mBuf) return Expected<void>::error(
            ParseError{ParseError::Code::TruncatedInput, "null buffer", mPos});
        if (mPos / 8 + count > (mTotalBits + 7) / 8) {
            return Expected<void>::error(
                ParseError{ParseError::Code::TruncatedInput, "read past end", mPos});
        }

        if (mBitOffset == 0) {
            // ── Byte-aligned: bulk memcpy ──
            std::memcpy(out, mBuf + mByteIndex, count);
            mPos += count * 8;
            mByteIndex += count;
        } else {
            size_t totalBytes = mBuf ? (mTotalBits + 7) / 8 : 0;
            size_t written = 0;

            while (written < count) {
                unsigned bitOff = static_cast<unsigned>(mPos % 8);
                size_t left = count - written;
                unsigned takeBytes = static_cast<unsigned>(std::min<size_t>(left, 3));
                unsigned needBytes = fieldSpanBytes(bitOff, takeBytes * 8);
                unsigned availBytes = remainingBytes();

                if (availBytes >= needBytes && needBytes <= 4) {
                    unsigned actual;
                    uint32_t loaded = loadN(mBuf, mByteIndex, totalBytes, needBytes, actual);
                    // Extract each full byte from the loaded value.
                    for (unsigned i = 0; i < takeBytes; ++i) {
                        out[written + i] = static_cast<uint8_t>(
                            loaded >> (actual * 8 - bitOff - (i + 1) * 8));
                    }
                    mPos += takeBytes * 8;
                    mByteIndex = mPos / 8;
                    written += takeBytes;
                } else {
                    auto r = readField(8);
                    if (!r) return Expected<void>::error(r.error());
                    out[written++] = static_cast<uint8_t>(r.value());
                }
            }

            mBitOffset = static_cast<unsigned>(mPos % 8);
        }

        return Expected<void>::hold();
    }

private:
    [[nodiscard]] constexpr unsigned remainingBytes() const noexcept {
        return static_cast<unsigned>((mTotalBits - mPos + 7) / 8);
    }

    const uint8_t* mBuf;
    size_t mTotalBits;
    size_t mPos;
    size_t mByteIndex;
    unsigned mBitOffset;
};

} // namespace gsml3parser
