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

/// Import/export decoration for public C++ entities that must be usable
/// across a DLL boundary (Windows shared builds).
#pragma once

// dllexport when the library itself is compiled, dllimport for consumers,
// empty otherwise (static builds, non-Windows). CMake defines
// GSML3PARSER_SHARED as PUBLIC and GSML3PARSER_EXPORTS as PRIVATE on the
// library target.
#if defined(_WIN32) && defined(GSML3PARSER_SHARED)
#  if defined(GSML3PARSER_EXPORTS)
#    define GSML3PARSER_DLL __declspec(dllexport)
#  else
#    define GSML3PARSER_DLL __declspec(dllimport)
#  endif
#else
#  define GSML3PARSER_DLL
#endif
