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

namespace gsml3parser {

/// MSB-first bit writer. Bit 7 is the first bit written into each byte.
class BitWriter {
public:
    constexpr BitWriter(uint8_t* buf, size_t nbits)
        : m_buf(buf), m_bits(nbits), m_pos(0) {}

    /// Write \p nbits bits of \p value (MSB-first). Only the top \p nbits
    /// are taken from \p value.
    void writeField(uint32_t value, unsigned nbits)
    {
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

    /// Write \p count bytes verbatim.
    void writeBytes(const uint8_t* data, size_t count)
    {
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
