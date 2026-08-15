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

/// Zero-copy L3 stream processor for contiguous buffers.
/// Parses L3 messages directly from a memory span without any intermediate
/// copying.  Ideal for high-throughput scenarios where data arrives in
/// large contiguous chunks (e.g., memory-mapped files, DMA buffers).
#pragma once

#include <cstddef>
#include <cstdint>
#include <concepts>
#include <optional>
#include <span>
#include <utility>

#include "gsml3parser/bitstream/stream_processor.h"
#include "gsml3parser/bitstream/inline_framer.h"
#include "gsml3parser/parser.h"
#include "gsml3parser/visitor.h"
#include "gsml3parser/types.h"

namespace gsml3parser {

/**
 * Zero-copy L3 stream processor for contiguous buffers.
 *
 * Parses L3 messages directly from a memory span without any intermediate
 * copying.  Ideal for high-throughput scenarios where data arrives in
 * large contiguous chunks (e.g., memory-mapped files, DMA buffers).
 *
 * Uses InlineFramer internally to extract frame boundaries as non-owning
 * spans into the original buffer, then calls parseL3() on each span.
 *
 * Thread safety: not thread-safe. Safe for concurrent use when each thread
 * has its own instance operating on separate memory regions.
 */
class ZeroCopyStreamProcessor {
    InlineFramer mFramer;
    StreamStats mStats{};

public:
    /**
     * Construct a zero-copy processor over a contiguous data span.
     *
     * @param data        The raw byte buffer containing L3 frames.
     * @param useL2Length If true, each frame is preceded by a single length octet.
     */
    explicit ZeroCopyStreamProcessor(std::span<const uint8_t> data,
                                      bool useL2Length = false);

    /**
     * Parse the next L3 message from the buffer.
     *
     * Returns a ParsedMessage on success.  No data is copied from the
     * original buffer — all intermediate spans are non-owning views.
     * The returned ParsedMessage owns its internal structures (variant of
     * parsed message objects), but the parsing path avoids heap-based
     * frame buffering.
     *
     * @return ParsedMessage on success, std::nullopt if no more data or parse error.
     */
    [[nodiscard]] std::optional<ParsedMessage> nextMessage();

    /**
     * Bulk process: call handler for each frame until exhausted.
     * Template parameter F must be invocable as F(const ParsedMessage&).
     */
    template<typename F>
        requires std::is_invocable_v<F, const ParsedMessage&>
    void forEach(F&& handler) {
        while (auto msg = nextMessage()) {
            handler(*msg);
        }
    }

    /** Access accumulated stream statistics. */
    [[nodiscard]] const StreamStats& stats() const noexcept { return mStats; }

    /** Reset all counters to zero. */
    void resetStats() noexcept { mStats = StreamStats{}; }

    /** Remaining unconsumed bytes in the buffer. */
    [[nodiscard]] size_t remaining() const noexcept { return mFramer.remaining(); }

    /** Reset framer to beginning of buffer. */
    void reset() noexcept { mFramer.reset(); }
};

// ── Inline implementation (header-only) ────────────────────────────────

inline ZeroCopyStreamProcessor::ZeroCopyStreamProcessor(
        std::span<const uint8_t> data, bool useL2Length)
    : mFramer(data, useL2Length) {}

inline std::optional<ParsedMessage> ZeroCopyStreamProcessor::nextMessage() {
    auto frame = mFramer.nextFrame();
    if (!frame) return std::nullopt;

    const auto& data = *frame;
    mStats.totalBytes += data.size();
    mStats.totalFrames++;

    // parseL3 returns Expected<ParsedMessage>; convert to optional.
    auto result = parseL3(data);
    if (!result.has_value()) {
        mStats.parseErrors++;
        return std::nullopt;
    }

    ParsedMessage msg = std::move(result.value());
    mStats.parsedOk++;

    // Categorize by protocol discriminator. O(1) switch dispatch.
    switch (messagePD(msg)) {
        case L3PD::RadioResource:          mStats.rrMessages++; break;
        case L3PD::MobilityManagement:     mStats.mmMessages++; break;
        case L3PD::CallControl:            mStats.ccMessages++; break;
        case L3PD::NonCallSS:              mStats.ssMessages++; break;
        case L3PD::GPRSMobilityManagement: mStats.gmmMessages++; break;
        case L3PD::GPRSSessionManagement:  mStats.smMessages++; break;
        case L3PD::SMS:                    mStats.smsMessages++; break;
        case L3PD::BroadcastCallControl:   mStats.bccMessages++; break;
        case L3PD::GroupCallControl:       mStats.gccMessages++; break;
        case L3PD::Location:               mStats.lsMessages++; break;
        case L3PD::Extended:               mStats.extendedMessages++; break;
        case L3PD::TestProcedure:          mStats.testprocMessages++; break;
        default:                           mStats.unsupportedPD++; break;
    }

    return msg;
}

} // namespace gsml3parser
