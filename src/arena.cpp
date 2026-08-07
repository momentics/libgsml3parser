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

#include "gsml3parser/arena.h"

namespace gsml3parser {

Arena::Arena(size_t initialCapacity)
    : mBuffer(initialCapacity, 0)
{
}

void* Arena::allocate(size_t bytes, size_t alignment)
{
    if (bytes == 0) return nullptr;

    size_t alignedOffset = (mOffset + alignment - 1) & ~(alignment - 1);

    if (alignedOffset + bytes > mBuffer.size()) {
        size_t newCapacity = mBuffer.size() * 2;
        if (newCapacity < alignedOffset + bytes) {
            newCapacity = alignedOffset + bytes;
        }
        try {
            mBuffer.resize(newCapacity, 0);
        } catch (...) {
            return nullptr;
        }
    }

    void* ptr = mBuffer.data() + alignedOffset;
    mOffset = alignedOffset + bytes;
    return ptr;
}

void Arena::reset()
{
    mOffset = 0;
}

} // namespace gsml3parser
