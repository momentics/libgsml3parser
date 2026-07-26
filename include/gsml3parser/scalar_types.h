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

#ifndef GSML3PARSER_SCALAR_TYPES_H
#define GSML3PARSER_SCALAR_TYPES_H

#include <cstddef>
#include <cstdint>

// Initialized scalar types that are guaranteed to be zero-initialized.
// Part of libgsml3parser.

#define _INITIALIZED_SCALAR_BASE_FUNCS(Classname, Basetype, Init) \
    Classname() : value(Init) {} \
    Classname(Basetype wvalue) { value = wvalue; } \
    operator Basetype() const { return value; } \
    Basetype operator=(Basetype wvalue) { return value = wvalue; } \
    Basetype* operator&() { return &value; }

#define _INITIALIZED_SCALAR_ARITH_FUNCS(Basetype) \
    Basetype operator++() { return ++value; } \
    Basetype operator++(int) { return value++; } \
    Basetype operator--() { return --value; } \
    Basetype operator-=(Basetype wvalue) { return value = value - wvalue; }

#define _INITIALIZED_SCALAR_FUNCS(Classname, Basetype, Init) \
    _INITIALIZED_SCALAR_BASE_FUNCS(Classname, Basetype, Init) \
    _INITIALIZED_SCALAR_ARITH_FUNCS(Basetype)

#define _DECLARE_SCALAR_TYPE(Classname_i, Classname_z, Basetype) \
    template <Basetype Init> \
    struct Classname_i { \
        Basetype value; \
        _INITIALIZED_SCALAR_FUNCS(Classname_i, Basetype, Init) \
    }; \
    typedef Classname_i<0> Classname_z;

_DECLARE_SCALAR_TYPE(Int_i, Int_z, int)
_DECLARE_SCALAR_TYPE(Char_i, Char_z, signed char)
_DECLARE_SCALAR_TYPE(Int16_i, Int16_z, int16_t)
_DECLARE_SCALAR_TYPE(Int32_i, Int32_z, int32_t)
_DECLARE_SCALAR_TYPE(UInt_i, UInt_z, unsigned)
_DECLARE_SCALAR_TYPE(UChar_i, UChar_z, unsigned char)
_DECLARE_SCALAR_TYPE(UInt16_i, UInt16_z, uint16_t)
_DECLARE_SCALAR_TYPE(UInt32_i, UInt32_z, uint32_t)
_DECLARE_SCALAR_TYPE(Size_t_i, Size_t_z, size_t)

template <bool Init>
struct Bool_i {
    bool value;
    _INITIALIZED_SCALAR_BASE_FUNCS(Bool_i, bool, Init)
};
typedef Bool_i<0> Bool_z;

struct Float_z {
    float value;
    _INITIALIZED_SCALAR_FUNCS(Float_z, float, 0)
};
struct Double_z {
    double value;
    _INITIALIZED_SCALAR_FUNCS(Double_z, double, 0)
};

#endif // GSML3PARSER_SCALAR_TYPES_H
