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

#include "gsml3parser/dispatcher.h"

namespace gsml3parser {

void ProtocolDispatcher::registerHandler(L3PD pd, int mti, MessageHandler handler) {
    mHandlers[{pd, mti}] = std::move(handler);
}

void ProtocolDispatcher::registerDomainHandler(L3PD pd, MessageHandler handler) {
    mDomainHandlers[pd] = std::move(handler);
}

void ProtocolDispatcher::setFallbackHandler(MessageHandler handler) {
    mFallback = std::move(handler);
}

void ProtocolDispatcher::dispatch(const ParsedMessage& msg, void* context) {
    L3PD pd = messagePD(msg);
    int mti = messageMTI(msg);
    HandlerKey key{pd, mti};

    auto it = mHandlers.find(key);
    if (it != mHandlers.end()) {
        it->second(msg, context);
        return;
    }

    auto dit = mDomainHandlers.find(pd);
    if (dit != mDomainHandlers.end()) {
        dit->second(msg, context);
        return;
    }

    if (mFallback) {
        mFallback(msg, context);
    }
}

bool ProtocolDispatcher::dispatchRaw(std::span<const uint8_t> data, void* context) {
    auto msg = parseL3(data);
    if (!msg) return false;
    dispatch(*msg, context);
    return true;
}

} // namespace gsml3parser
