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

#include <mutex>

#include "gsml3parser/context.h"

namespace gsml3parser {

void ParserContext::registerPDHandler(L3PD pd, PDHandler handler) {
    std::unique_lock lock(mMutex);
    mPDHandlers[static_cast<int>(pd)] = std::move(handler);
}

void ParserContext::unregisterPDHandler(L3PD pd) {
    std::unique_lock lock(mMutex);
    mPDHandlers.erase(static_cast<int>(pd));
}

std::optional<PDHandler> ParserContext::getPDHandler(L3PD pd) const {
    std::shared_lock lock(mMutex);
    auto it = mPDHandlers.find(static_cast<int>(pd));
    if (it != mPDHandlers.end()) {
        return it->second;
    }
    return std::nullopt;
}

LogLevel ParserContext::logLevel() const {
    std::shared_lock lock(mMutex);
    return mLogLevel;
}

void ParserContext::setLogLevel(LogLevel level) {
    std::unique_lock lock(mMutex);
    mLogLevel = level;
}

} // namespace gsml3parser
