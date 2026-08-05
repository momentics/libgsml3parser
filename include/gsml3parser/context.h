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

#ifndef GSML3PARSER_CONTEXT_H
#define GSML3PARSER_CONTEXT_H

#include <cstddef>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <unordered_map>

#include "types.h"
#include "l3message.h"
#include "l3frame.h"
#include "logger.h"

namespace gsml3parser {

/**
 * ParserContext — a thread-safe container for per-instance parser configuration.
 *
 * Each context holds its own PD handler registry and log level, eliminating
 * the need for global mutable state.  Multiple threads may share a single
 * read-only context safely; concurrent writes are protected by a shared_mutex.
 */
class ParserContext {
public:
    ParserContext() = default;

    /** Register a custom handler for an unsupported Protocol Discriminator. */
    void registerPDHandler(L3PD pd, PDHandler handler);

    /** Remove a previously registered handler for a PD. */
    void unregisterPDHandler(L3PD pd);

    /** Retrieve the handler for a given PD (read-only, lock-free). */
    std::optional<PDHandler> getPDHandler(L3PD pd) const;

    /** Get the log level associated with this context. */
    LogLevel logLevel() const;

    /** Set the log level associated with this context. */
    void setLogLevel(LogLevel level);

private:
    std::unordered_map<int, PDHandler> mPDHandlers;
    mutable std::shared_mutex mMutex;
    LogLevel mLogLevel = LogLevel::INFO;
};

} // namespace gsml3parser

#endif // GSML3PARSER_CONTEXT_H
