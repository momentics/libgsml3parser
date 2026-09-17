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

/// Out-of-line definitions for the FlatHandler shared-handler machinery
/// (flat_handler.h).

#include "gsml3parser/flat_handler.h"

namespace gsml3parser::detail {

// The shared-handler marker (see flat_handler.h for why this must be a
// single library-wide copy rather than an inline function): isShared()
// identifies shared handlers by comparing their fn pointer against this
// function's address.
void sharedTrampoline(const ParsedMessage* msg, void* ctx) {
    auto* holder = static_cast<SharedHandlerHolder*>(ctx);
    holder->handler->invoke(*msg, nullptr);
}

} // namespace gsml3parser::detail
