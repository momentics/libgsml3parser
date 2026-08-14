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

#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <memory>

#include "types.h"
#include "l3header.h"

namespace gsml3parser {

/** Base class for all L3 messages. */
class L3Message {};

/** Raw L3 frame: header + payload bytes. */
struct L3Frame {
    L3Header header;
    std::span<const uint8_t> data;
};

using PDHandler = std::function<std::unique_ptr<L3Message>(const L3Frame&)>;

/**
 * ParserConfig - an immutable, thread-safe parser configuration.
 *
 * Pure data struct with no internal synchronization.  Safe for concurrent
 * read access from multiple threads.  Modifications return a new config
 * instance (immutable builder pattern), eliminating the need for mutexes.
 */
struct ParserConfig {
    LogLevel logLevel{LogLevel::WARNING};
    std::array<PDHandler, 16> pdHandlers{};

    [[nodiscard]] LogLevel getLogLevel() const noexcept
    {
        return logLevel;
    }

    [[nodiscard]] const PDHandler* getPDHandler(L3PD pd) const noexcept
    {
        int idx = static_cast<int>(pd);
        if (idx < 0 || idx > 15) return nullptr;
        const auto& h = pdHandlers[idx];
        return h ? &h : nullptr;
    }

    [[nodiscard]] ParserConfig withLogLevel(LogLevel lvl) const noexcept
    {
        ParserConfig cfg = *this;
        cfg.logLevel = lvl;
        return cfg;
    }

    [[nodiscard]] ParserConfig withPDHandler(L3PD pd, PDHandler handler) const
    {
        int idx = static_cast<int>(pd);
        if (idx < 0 || idx > 15) return *this;
        ParserConfig cfg = *this;
        cfg.pdHandlers[idx] = std::move(handler);
        return cfg;
    }
};

} // namespace gsml3parser
