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

// Fuzz target: L3Framer frame extraction + parseL3 over arbitrary byte
// streams.
#include <cstddef>
#include <cstdint>
#include <span>

#include "gsml3parser/bitstream/byte_source.h"
#include "gsml3parser/bitstream/framer.h"
#include "gsml3parser/parser.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    std::span<const uint8_t> span(data, size);
    gsml3parser::SpanByteSource src(span);
    gsml3parser::L3Framer framer(src);
    int budget = 1000; // bound the loop: fuzzer input is finite
    while (budget-- > 0) {
        auto frame = framer.nextFrame();
        if (!frame) break;
        (void)gsml3parser::parseL3(frame.value().data);
    }
    return 0;
}
