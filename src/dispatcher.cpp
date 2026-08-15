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
    int pidx = static_cast<int>(pd);
    if (pidx >= 0 && pidx < 16 && mti >= 0 && mti < 256) {
        mHandlers[static_cast<size_t>(pidx)][static_cast<size_t>(mti)] = std::move(handler);
    }
}

void ProtocolDispatcher::registerDomainHandler(L3PD pd, MessageHandler handler) {
    int pidx = static_cast<int>(pd);
    if (pidx >= 0 && pidx < 16) {
        mDomainHandlers[static_cast<size_t>(pidx)] = std::move(handler);
    }
}

void ProtocolDispatcher::setFallbackHandler(MessageHandler handler) {
    mFallback = std::move(handler);
}

void ProtocolDispatcher::dispatch(const ParsedMessage& msg, void* context) {
    L3PD pd = messagePD(msg);
    int mti = messageMTI(msg);
    int pidx = static_cast<int>(pd);

    // O(1) direct array lookup for specific handler.
    if (pidx >= 0 && pidx < 16 && mti >= 0 && mti < 256) {
        auto& h = mHandlers[static_cast<size_t>(pidx)][static_cast<size_t>(mti)];
        if (h.has_value()) {
            h.value()(msg, context);
            return;
        }
    }

    // O(1) direct array lookup for domain handler.
    if (pidx >= 0 && pidx < 16) {
        auto& dh = mDomainHandlers[static_cast<size_t>(pidx)];
        if (dh.has_value()) {
            dh.value()(msg, context);
            return;
        }
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

void ProtocolDispatcher::registerTIHandler(uint8_t ti, MessageHandler handler) {
    if (ti < 8) {
        mTIHandlers[ti] = std::move(handler);
    }
}

void ProtocolDispatcher::dispatchWithTI(const ParsedMessage& msg, void* context) {
    L3PD pd = messagePD(msg);

    if (pd == L3PD::CallControl || pd == L3PD::NonCallSS) {
        uint8_t ti = messageTI(msg);
        if (ti < 8 && mTIHandlers[ti].has_value()) {
            (*mTIHandlers[ti])(msg, context);
            return;
        }
    }

    dispatch(msg, context);
}

} // namespace gsml3parser
