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
#include <cstring>
#include <cstdint>

namespace gsml3parser {

/// MSB-first bit writer. Bit 7 is the first bit written into each byte.
class BitWriter {
public:
    constexpr BitWriter(uint8_t* buf, size_t nbits)
        : m_buf(buf), m_bits(nbits), m_pos(0) {}

    /// Write \p nbits bits of \p value (MSB-first). Only the top \p nbits
    /// are taken from \p value. \p nbits is clamped to 32 (the value width):
    /// a shift count >= 32 would be undefined behavior (audit Q2; mirrors
    /// BitReader::peekField's documented clamp).
    void writeField(uint32_t value, unsigned nbits)
    {
        if (nbits > 32) nbits = 32;
        for (int i = static_cast<int>(nbits) - 1; i >= 0; --i) {
            if (m_pos >= m_bits) break;
            size_t byteIdx = m_pos / 8;
            unsigned bitIdx = 7u - static_cast<unsigned>(m_pos % 8);
            if ((value >> i) & 1u)
                m_buf[byteIdx] |= static_cast<uint8_t>(1u << bitIdx);
            else
                m_buf[byteIdx] &= static_cast<uint8_t>( ~(1u << bitIdx) );
            ++m_pos;
        }
    }

    /// Write a single octet.
    void writeOctet(uint8_t v)
    {
        writeField(v, 8);
    }

    /// Write \p count bytes verbatim. Uses a bulk memcpy when byte-aligned
    /// and the bytes fit in the buffer; otherwise falls back to per-octet
    /// bit writes (audit Q2: the previous per-bit loop cost 8 operations
    /// per byte on the aligned path).
    void writeBytes(const uint8_t* data, size_t count)
    {
        if (count == 0) return;
        if (m_pos % 8 == 0 && m_pos + count * 8 <= m_bits) {
            std::memcpy(m_buf + m_pos / 8, data, count);
            m_pos += count * 8;
            return;
        }
        for (size_t i = 0; i < count; ++i) {
            writeOctet(data[i]);
        }
    }

    /// Pad with zeros up to the next octet boundary.
    void alignToOctet()
    {
        unsigned spare = (8u - static_cast<unsigned>(m_pos % 8)) % 8u;
        if (spare) {
            writeField(0, spare);
        }
    }

    /// Current bit position.
    size_t position() const
    {
        return m_pos;
    }

private:
    uint8_t* m_buf;
    size_t m_bits;
    size_t m_pos;
};

} // namespace gsml3parser
