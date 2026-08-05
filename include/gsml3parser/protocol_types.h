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

#include <cstdint>
#include <type_traits>

namespace gsml3parser {

template <typename Base, Base MinVal, Base MaxVal>
struct Bounded {
    Base value{MinVal};

    constexpr Bounded() = default;
    constexpr explicit Bounded(Base v) : value(v) {}

    constexpr operator Base() const noexcept { return value; }
    constexpr Base get() const noexcept { return value; }
    constexpr Bounded& operator=(Base v) & noexcept { value = v; return *this; }

    static constexpr Base min() { return MinVal; }
    static constexpr Base max() { return MaxVal; }
};

// GSM protocol field types with bounded ranges
using Arfcn              = Bounded<uint16_t, 0, 1023>;   // 10 bits
using Bsic               = Bounded<uint8_t,  0, 63>;     // 6 bits
using TimingAdvanceValue = Bounded<uint8_t,  0, 63>;     // 6 bits
using Ncc                = Bounded<uint8_t,  0, 7>;      // 3 bits
using Bcc                = Bounded<uint8_t,  0, 7>;      // 3 bits
using Tsc                = Bounded<uint8_t,  0, 7>;      // 3 bits
using Hsn                = Bounded<uint8_t,  0, 7>;      // 3 bits
using Maio               = Bounded<uint8_t,  0, 63>;     // 6 bits
using TimeslotNumber     = Bounded<uint8_t,  0, 15>;     // 4 bits
using CellIdentity       = Bounded<uint16_t, 0, 65535>;  // 16 bits
using Lac                = Bounded<uint16_t, 0, 65535>;  // 16 bits
using Cksn               = Bounded<uint8_t,  0, 7>;      // 3 bits
using CiValue            = Bounded<uint8_t,  0, 15>;     // 4 bits

} // namespace gsml3parser
