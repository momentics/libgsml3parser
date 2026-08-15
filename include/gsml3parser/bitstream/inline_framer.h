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
 * Thread safety: not thread-safe. One instance per buffer, single-threaded use.
 */
class InlineFramer {
    std::span<const uint8_t> mData;
    size_t mPos{};
    bool mUseL2Length{false};
    size_t mMaxFrameLen{4096};

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
};

// ── Inline implementations (header-only) ──────────────────────────────

inline InlineFramer::InlineFramer(std::span<const uint8_t> data, bool useL2Length)
    : mData(data), mUseL2Length(useL2Length) {}

inline void InlineFramer::reset() noexcept {
    mPos = 0;
}

inline std::optional<std::span<const uint8_t>> InlineFramer::nextFrame() noexcept {
    if (mPos >= mData.size()) return std::nullopt;

    size_t frameLen;
    size_t offset = mPos;

    if (mUseL2Length) {
        // L2 length mode: first byte is the L3 message length.
        if (mPos + 1 > mData.size()) return std::nullopt;
        frameLen = static_cast<size_t>(mData[mPos]);
        if (frameLen == 0 || frameLen > mMaxFrameLen) return std::nullopt;
        mPos += 1; // skip length octet
        if (mPos + frameLen > mData.size()) return std::nullopt;
    } else {
        // Header-based mode: use fixed-length table from PD+MTI.
        // Need at least 2 bytes for L3 header.
        if (mPos + 2 > mData.size()) return std::nullopt;

        uint8_t b0 = mData[mPos];
        uint8_t b1 = mData[mPos + 1];
        int pd = (b0 >> 4) & 0x0F;
        int rawMti = b1;
        int mti = rawMti;

        // Adjust MTI for MM/CC/SS (6-bit messageType).
        if (pd == 0x05 || pd == 0x03 || pd == 0x0B) {
            mti = (rawMti & 0xFC) >> 2;
        }

        // Fixed-length lookup table — mirrors framer.cpp fixedBodyLength().
        constexpr size_t VARIABLE = static_cast<size_t>(-1);
        size_t bodyLen = VARIABLE;

        switch (pd) {
            case 0x06: // Radio Resource
                switch (mti) {
                    case 0x0D: bodyLen = 1; break; // Channel Release
                    case 0x0E: bodyLen = 5; break; // Paging Response
                    case 0x0F: bodyLen = 8; break; // Classmark Change
                    case 0x10: bodyLen = 3; break; // Classmark Enquiry
                    case 0x1A: bodyLen = 0; break; // RR Status
                    case 0x1C: bodyLen = 4; break; // Assignment Complete
                    case 0x1D: bodyLen = 2; break; // Assignment Failure
                    case 0x21: bodyLen = 3; break; // Immediate Assignment Reject
                    case 0x25: bodyLen = 0; break; // Additional Assignment
                    case 0x29: bodyLen = 4; break; // Handover Complete
                    case 0x2A: bodyLen = 2; break; // Handover Failure
                    case 0x2E: bodyLen = 1; break; // Physical Information
                    case 0x33: bodyLen = 2; break; // Ciphering Mode Complete
                    default:   break;
                }
                break;
            case 0x05: // Mobility Management
                switch (mti) {
                    case 0x1F: bodyLen = 4; break; // IMSI Detach Indication
                    case 0x21: bodyLen = 0; break; // CM Service Accept
                    case 0x22: bodyLen = 3; break; // CM Service Reject
                    case 0x23: bodyLen = 1; break; // CM Service Abort
                    case 0x27: bodyLen = 1; break; // MM Status
                    default:   break;
                }
                break;
            case 0x03: // Call Control
                switch (mti) {
                    case 0x23: bodyLen = 1; break; // Release Complete
                    case 0x25: bodyLen = 1; break; // CC Status (min body)
                    default:   break;
                }
                break;
            default:
                break;
        }

        if (bodyLen != VARIABLE) {
            frameLen = 2 + bodyLen;
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
                    candidatePd == 0x0F || candidatePd == 0x00 ||
                    candidatePd == 0x01) {
                    frameLen = i - mPos;
                    break;
                }
            }

            if (frameLen == 0 || frameLen > mMaxFrameLen) return std::nullopt;
        }
    }

    auto frame = mData.subspan(static_cast<size_t>(mPos), frameLen);
    mPos += frameLen;
    return frame;
}

} // namespace gsml3parser
