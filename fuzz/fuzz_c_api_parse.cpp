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

// Fuzz target: the C ABI parse surface. Random bytes go through
// gsml3_parse_l3 (fresh handle), gsml3_parse_l3_into (reused handle, the
// zero-allocation hot path) and gsml3_rsl_parse (the copy-owning RSL
// handle). The C API must never crash, leak or let an exception escape.

#include <gsml3parser/gsml3parser_c.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // Fresh-handle parse + query + serialize + free.
    gsml3_message* msg = gsml3_parse_l3(data, size, nullptr);
    if (msg) {
        (void)gsml3_message_name(msg);
        (void)gsml3_message_pd(msg);
        (void)gsml3_message_mti(msg);
        (void)gsml3_message_ti(msg);
        uint8_t out[1024];
        (void)gsml3_message_write(msg, out, sizeof(out));
        char* hex = gsml3_message_hex(msg);
        if (hex) gsml3_free(hex);

        // Reused-handle reparse (hot path): same input, then garbage.
        (void)gsml3_parse_l3_into(msg, data, size, nullptr);
        if (size > 0) (void)gsml3_parse_l3_into(msg, data, 1, nullptr);
        gsml3_message_free(msg);
    }

    // RSL parse (the handle copies the input) + L3 view re-parsed as L3.
    gsml3_rsl* rsl = gsml3_rsl_parse(data, size);
    if (rsl) {
        (void)gsml3_rsl_name(rsl);
        size_t l3len = 0;
        const uint8_t* l3 = gsml3_rsl_l3(rsl, &l3len);
        if (l3 && l3len) {
            gsml3_message* m2 = gsml3_parse_l3(l3, l3len, nullptr);
            if (m2) gsml3_message_free(m2);
        }
        gsml3_rsl_free(rsl);
    }
    return 0;
}
