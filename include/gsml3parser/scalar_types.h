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

// Initialized scalar types that are guaranteed to be zero-initialized.
// Replaces the old macro-based _DECLARE_SCALAR_TYPE with C++20 templates.
// Part of libgsml3parser.

namespace gsml3parser {

// ── Generic integer scalar wrapper ───────────────────────────────────────

template <typename T, T Init = T{0}>
struct Scalar_i {
    T value = Init;

    constexpr Scalar_i() = default;
    constexpr explicit Scalar_i(T v) : value(v) {}

    constexpr operator T() const { return value; }
    constexpr Scalar_i& operator=(T v) & { value = v; return *this; }
};

// ── Boolean scalar wrapper ───────────────────────────────────────────────

template <bool Init = false>
struct Bool_i {
    bool value = Init;

    constexpr Bool_i() = default;
    constexpr explicit Bool_i(bool v) : value(v) {}

    constexpr operator bool() const noexcept { return value; }
    constexpr Bool_i& operator=(bool v) & { value = v; return *this; }
};

// ── Zero-initialized typedefs ────────────────────────────────────────────

using Int_z     = Scalar_i<int, 0>;
using Char_z    = Scalar_i<signed char, 0>;
using Int16_z   = Scalar_i<int16_t, 0>;
using Int32_z   = Scalar_i<int32_t, 0>;
using UInt_z    = Scalar_i<unsigned, 0>;
using UChar_z   = Scalar_i<unsigned char, 0>;
using UInt16_z  = Scalar_i<uint16_t, 0>;
using UInt32_z  = Scalar_i<uint32_t, 0>;
using Size_t_z  = Scalar_i<size_t, 0>;
using Bool_z    = Bool_i<false>;
using Float_z   = Scalar_i<float, 0.f>;
using Double_z  = Scalar_i<double, 0.0>;

} // namespace gsml3parser


