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

/// Zero-copy L3 frame extractor for contiguous memory buffers.
/// Operates directly on caller-owned std::span, yielding views into the
/// original data with no allocations or copies.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "gsml3parser/bitstream/frame_lengths.h"

namespace gsml3parser {

/**
 * Zero-copy L3 frame extractor for contiguous memory buffers.
 *
 * Unlike L3Framer which reads from a ByteSource and copies data into an
 * internal buffer, InlineFramer operates directly on the caller's span,
 * yielding views into the original data with zero allocations.
 *
 * Supports two framing modes:
 * - L2 length mode: each frame preceded by a length octet.
 * - Header-based mode: frame length derived from PD+MTI fixed-length table.
 *
 * NOTE: header-based mode is a heuristic for variable-length messages (it
 * scans for the next plausible L3 header). For deterministic framing of
 * variable-length messages use useL2Length = true, which is what production
 * LAPDm / A-bis paths provide.
 *
 * Thread safety: not thread-safe. One instance per buffer, single-threaded use.
 */
class InlineFramer {
    std::span<const uint8_t> mData;
    size_t mPos{};
    bool mUseL2Length{false};
    size_t mMaxFrameLen{4096};
    size_t mResyncSkips{0};  // corrupt length octets skipped

public:
    constexpr InlineFramer() noexcept = default;

    /**
     * Construct an InlineFramer over a contiguous data span.
     *
     * @param data        The raw byte buffer containing L3 frames.
     * @param useL2Length If true, each frame is preceded by a single length octet.
     */
    explicit InlineFramer(std::span<const uint8_t> data, bool useL2Length = false);

    /**
     * Extract the next frame from the buffer.
     *
     * Returns a non-owning span into the original data — zero-copy.
     * Caller must ensure the original buffer outlives the returned span.
     *
     * @return span into the original data, or std::nullopt if no more frames.
     */
    [[nodiscard]] std::optional<std::span<const uint8_t>> nextFrame() noexcept;

    /** Remaining unconsumed bytes. O(1). */
    [[nodiscard]] constexpr size_t remaining() const noexcept { return mData.size() - mPos; }

    /** Reset to beginning of buffer. O(1). */
    void reset() noexcept;

    /** Set maximum allowed frame length (default 4096). */
    void setMaxFrameLength(size_t len) noexcept { mMaxFrameLen = len; }

    /** Number of corrupt L2 length octets skipped during resync. */
    [[nodiscard]] constexpr size_t resyncSkips() const noexcept { return mResyncSkips; }
};

// ── Inline implementations (header-only) ──────────────────────────────

inline InlineFramer::InlineFramer(std::span<const uint8_t> data, bool useL2Length)
    : mData(data), mUseL2Length(useL2Length) {}

inline void InlineFramer::reset() noexcept {
    mPos = 0;
    mResyncSkips = 0;
}

inline std::optional<std::span<const uint8_t>> InlineFramer::nextFrame() noexcept {
    if (mPos >= mData.size()) return std::nullopt;

    size_t frameLen;
    size_t offset = mPos;

    if (mUseL2Length) {
        // L2 length mode: first byte is the L3 message length.
        for (;;) {
            if (mPos + 1 > mData.size()) return std::nullopt;
            frameLen = static_cast<size_t>(mData[mPos]);
            if (frameLen == 0 || frameLen > mMaxFrameLen) {
                // Corrupt length octet: skip it and resynchronize on the
                // next byte (the previous code returned
                // nullopt here WITHOUT advancing mPos, so the caller saw
                // "buffer exhausted" and silently abandoned every
                // remaining frame).
                ++mPos;
                ++mResyncSkips;
                continue;
            }
            break;
        }
        if (mPos + 1 + frameLen > mData.size()) return std::nullopt;
        mPos += 1; // skip length octet
    } else {
        // Header-based mode: use fixed-length table from PD+MTI.
        // Need at least 2 bytes for L3 header.
        if (mPos + 2 > mData.size()) return std::nullopt;

        uint8_t b0 = mData[mPos];
        uint8_t b1 = mData[mPos + 1];
        int pd = (b0 >> 4) & 0x0F;
        int rawMti = b1;
        int mti = rawMti;

        // Adjust MTI for MM/CC/SS/BCC/GCC (6-bit messageType + 2-bit NSD).
        // BCC (0x01) and GCC (0x00) use the same CC-style header
        // (TS 44.018 10.2) — the previous condition missed
        // them, so their fixed-length table entries never matched.
        if (pd == 0x05 || pd == 0x03 || pd == 0x0B || pd == 0x01 || pd == 0x00) {
            mti = (rawMti & 0xFC) >> 2;
        }

        // Same boundary-candidate logic as L3Framer (C17):
        // 0x00/0x01/0x0c high nibbles occur frequently inside
        // variable-length bodies (e.g. GMM/SMS cause octets), so
        // they are only accepted while framing BCC/GCC/LS
        // messages (the two framers must
        // behave identically).
        const bool callControlLike = (pd == 0x00 || pd == 0x01 || pd == 0x0c);

        // Fixed-length lookup — single source of truth in
        // bitstream/frame_lengths.h (the previous
        // duplicated switch used wrong MTI values and lengths).
        size_t fixedLen = detail::fixedFrameLength(pd, mti);

        if (fixedLen != 0) {
            frameLen = fixedLen;
            if (mPos + frameLen > mData.size()) return std::nullopt;
        } else {
            // Variable-length message: scan for next plausible L3 header.
            size_t searchEnd = mPos + 2 + mMaxFrameLen;
            if (searchEnd > mData.size()) searchEnd = mData.size();

            frameLen = 0;
            for (size_t i = mPos + 2; i + 1 < searchEnd; ++i) {
                uint8_t candidatePd = (mData[i] >> 4) & 0x0F;
                if (candidatePd == 0x03 || candidatePd == 0x05 ||
                    candidatePd == 0x06 || candidatePd == 0x0B ||
                    candidatePd == 0x08 || candidatePd == 0x09 ||
                    candidatePd == 0x0A || candidatePd == 0x0E ||
                    candidatePd == 0x0F ||
                    (callControlLike &&
                     (candidatePd == 0x00 || candidatePd == 0x01 || candidatePd == 0x0C))) {
                    frameLen = i - mPos;
                    break;
                }
            }

            if (frameLen == 0) {
                // No boundary found. This is a contiguous in-memory buffer (no more data
                // can arrive), so the remainder of the buffer is the last frame: emit it
                // if it fits the size limit. The parser still validates the content; a
                // truncated frame surfaces as a parse error rather than being silently
                // dropped.
                size_t rest = mData.size() - mPos;
                if (rest < 2 || rest > mMaxFrameLen) return std::nullopt;
                frameLen = rest;
            } else if (frameLen > mMaxFrameLen) {
                return std::nullopt;
            }
        }
    }

    auto frame = mData.subspan(static_cast<size_t>(mPos), frameLen);
    mPos += frameLen;
    return frame;
}

} // namespace gsml3parser
