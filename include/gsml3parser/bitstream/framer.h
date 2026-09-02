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

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>
#include <chrono>

#include "gsml3parser/expected.h"
#include "gsml3parser/bitstream/byte_source.h"

namespace gsml3parser {

/** Configuration for frame boundary detection. */
struct FrameConfig {
    /** Use L2 length octet before each L3 message for framing. */
    bool useL2Length{false};

    /** Safety limit: reject frames larger than this many bytes. */
    size_t maxMessageLength{4096};

    /** Minimum bytes required for a valid L3 header. */
    size_t minHeaderLength{2};
};

/** A single extracted L3 frame from the byte stream. */
struct ExtractedFrame {
    /** Non-owning view into framer's internal buffer. */
    std::span<const uint8_t> data;

    /** L2 length octet value (only meaningful when FrameConfig.useL2Length is true). */
    size_t l2Length{};

    /// Timestamp (seconds since epoch) of the buffer fill during which the
    /// frame was extracted — batched per fill, not per frame (audit N2:
    /// a steady_clock read per frame is avoidable overhead at 10M msg/s).
    double timestamp{};
};

/**
 * Extracts L3 frames from a raw byte stream.
 *
 * Reads bytes from a ByteSource, detects frame boundaries, and returns
 * ExtractedFrame objects that the caller can pass to parseL3().
 *
 * Two framing modes are supported:
 * - L2 length mode: each frame is preceded by a length octet.
 * - Header-based mode: the L3 header (PD + MTI) is parsed to determine body length.
 */
class L3Framer {
    ByteSource& mSource;
    std::vector<uint8_t> mBuf;
    size_t mPos{};
    size_t mEnd{};
    FrameConfig mConfig;
    double mBufferTimestamp{}; // set in fillBuffer(); stamped on extracted frames

public:
    explicit L3Framer(ByteSource& source, FrameConfig cfg = {});

    /**
     * Extract the next L3 frame from the byte stream.
     *
     * @return ExtractedFrame on success.
     *         TruncatedInput when more data is needed (caller should wait and retry).
     *         InvalidPD or InvalidValue on corrupt frames (frame is skipped).
     */
    [[nodiscard]] Expected<ExtractedFrame> nextFrame();

    /** Skip @p nbytes bytes (error recovery). */
    void skip(size_t nbytes);

    /** Number of bytes currently buffered but not yet consumed. */
    [[nodiscard]] size_t buffered() const noexcept { return mEnd - mPos; }

private:
    /** Read more data from source into mBuf. Returns true if data was read. */
    bool fillBuffer();

    /** Try to extract a frame from mBuf[mPos..mEnd). */
    Expected<ExtractedFrame> tryExtract();
};

} // namespace gsml3parser
