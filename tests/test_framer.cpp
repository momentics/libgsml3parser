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

#include <gtest/gtest.h>
#include "gsml3parser/bitstream/framer.h"
#include "gsml3parser/bitstream/byte_source.h"
#include <vector>

using namespace gsml3parser;

// ── Single frame extraction (header-based mode) ────────────────────────

TEST(L3Framer, SingleFixedLengthFrame) {
    // Channel Release: 60 0D 00 (PD=6 in high nibble, MTI=0x0D, 1 body byte)
    uint8_t data[] = {0x60, 0x0D, 0x00};
    SpanByteSource src(std::span<const uint8_t>(data, std::size(data)));
    L3Framer framer(src);

    auto result = framer.nextFrame();
    ASSERT_TRUE(result.has_value());
    const auto& frame = result.value();
    ASSERT_EQ(frame.data.size(), 3u);
    ASSERT_EQ(frame.data[0], 0x60);
    ASSERT_EQ(frame.data[1], 0x0D);
    ASSERT_EQ(frame.data[2], 0x00);
}

TEST(L3Framer, SingleCMServiceAccept) {
    // CM Service Accept: 50 84 (PD=5 in high nibble, MTI encoded in byte 1)
    uint8_t data[] = {0x50, 0x84};
    SpanByteSource src(std::span<const uint8_t>(data, std::size(data)));
    L3Framer framer(src);

    auto result = framer.nextFrame();
    ASSERT_TRUE(result.has_value());
    const auto& frame = result.value();
    ASSERT_EQ(frame.data.size(), 2u);
}

// ── Multiple frames back-to-back ───────────────────────────────────────

TEST(L3Framer, MultipleFixedLengthFrames) {
    uint8_t data[] = {
        0x60, 0x0D, 0x00,  // Channel Release #1
        0x60, 0x0D, 0x01   // Channel Release #2
    };
    SpanByteSource src(std::span<const uint8_t>(data, std::size(data)));
    L3Framer framer(src);

    auto r1 = framer.nextFrame();
    ASSERT_TRUE(r1.has_value());
    ASSERT_EQ(r1.value().data.size(), 3u);

    auto r2 = framer.nextFrame();
    ASSERT_TRUE(r2.has_value());
    const auto& f2 = r2.value();
    ASSERT_EQ(f2.data.size(), 3u);
    ASSERT_EQ(f2.data[0], 0x60);
    ASSERT_EQ(f2.data[1], 0x0D);
    ASSERT_EQ(f2.data[2], 0x01);

    auto r3 = framer.nextFrame();
    ASSERT_FALSE(r3.has_value());
    ASSERT_EQ(static_cast<int>(r3.error().code), static_cast<int>(ParseError::Code::TruncatedInput));
}

// ── Truncated frame at end of buffer ───────────────────────────────────

TEST(L3Framer, TruncatedFrame) {
    // Only the header of a Channel Release (need 3 bytes, only have 2).
    uint8_t data[] = {0x60, 0x0D};
    SpanByteSource src(std::span<const uint8_t>(data, std::size(data)));
    L3Framer framer(src);

    auto result = framer.nextFrame();
    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(static_cast<int>(result.error().code), static_cast<int>(ParseError::Code::TruncatedInput));
}

TEST(L3Framer, EmptySource) {
    uint8_t dummy;
    SpanByteSource src(std::span<const uint8_t>(&dummy, 0));
    L3Framer framer(src);

    auto result = framer.nextFrame();
    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(static_cast<int>(result.error().code), static_cast<int>(ParseError::Code::TruncatedInput));
}

// ── L2 length mode ─────────────────────────────────────────────────────

TEST(L3Framer, L2LengthMode) {
    // L2 length octet (0x03) + 3-byte L3 message.
    uint8_t data[] = {0x03, 0x60, 0x0D, 0x00};
    SpanByteSource src(std::span<const uint8_t>(data, std::size(data)));
    FrameConfig cfg;
    cfg.useL2Length = true;
    L3Framer framer(src, cfg);

    auto result = framer.nextFrame();
    ASSERT_TRUE(result.has_value());
    const auto& frame = result.value();
    ASSERT_EQ(frame.data.size(), 3u);
    ASSERT_EQ(frame.l2Length, 3u);
}

TEST(L3Framer, L2LengthMultipleFrames) {
    uint8_t data[] = {
        0x03, 0x60, 0x0D, 0x00,  // Frame 1: length=3, Channel Release
        0x02, 0x50, 0x84          // Frame 2: length=2, CM Service Accept
    };
    SpanByteSource src(std::span<const uint8_t>(data, std::size(data)));
    FrameConfig cfg;
    cfg.useL2Length = true;
    L3Framer framer(src, cfg);

    auto r1 = framer.nextFrame();
    ASSERT_TRUE(r1.has_value());
    ASSERT_EQ(r1.value().data.size(), 3u);

    auto r2 = framer.nextFrame();
    ASSERT_TRUE(r2.has_value());
    ASSERT_EQ(r2.value().data.size(), 2u);
}

TEST(L3Framer, L2LengthTruncated) {
    // L2 length says 5 bytes but only 3 available.
    uint8_t data[] = {0x05, 0x60, 0x0D};
    SpanByteSource src(std::span<const uint8_t>(data, std::size(data)));
    FrameConfig cfg;
    cfg.useL2Length = true;
    L3Framer framer(src, cfg);

    auto result = framer.nextFrame();
    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(static_cast<int>(result.error().code), static_cast<int>(ParseError::Code::TruncatedInput));
}

// ── buffered() tracking ────────────────────────────────────────────────

TEST(L3Framer, BufferedCount) {
    uint8_t data[] = {0x60, 0x0D, 0x00, 0x60, 0x0D, 0x01};
    SpanByteSource src(std::span<const uint8_t>(data, std::size(data)));
    L3Framer framer(src);

    auto r1 = framer.nextFrame();
    ASSERT_TRUE(r1.has_value());
    ASSERT_EQ(framer.buffered(), 3u);

    auto r2 = framer.nextFrame();
    ASSERT_TRUE(r2.has_value());
    ASSERT_EQ(framer.buffered(), 0u);

    auto r3 = framer.nextFrame();
    ASSERT_FALSE(r3.has_value());
}

// ── Variable-length message framing (header scan) ──────────────────────

TEST(L3Framer, VariableLengthWithNextHeader) {
    // Channel Request (variable-length RR, MTI=0x01) followed by Channel Release.
    // Body bytes 0x21/0x40 are chosen so their high nibbles (0x02/0x04) are not
    // valid PDs, keeping the scanned boundary at the real next header.
    uint8_t data[] = {
        0x60, 0x01, 0x21, 0x40,  // Channel Request (4 bytes, scanned boundary)
        0x60, 0x0D, 0x00          // Channel Release (3 bytes, fixed)
    };
    SpanByteSource src(std::span<const uint8_t>(data, std::size(data)));
    L3Framer framer(src);

    auto r1 = framer.nextFrame();
    ASSERT_TRUE(r1.has_value());
    ASSERT_EQ(r1.value().data.size(), 4u);

    auto r2 = framer.nextFrame();
    ASSERT_TRUE(r2.has_value());
    ASSERT_EQ(r2.value().data.size(), 3u);
}

// ── BCC/GCC/LS framing (header-based mode, C17) ────────────────────────

TEST(L3Framer, BCCSetupStreamThreeFrames) {
    // Three BCC Setup frames back-to-back: 10 01 (PD=0x01, MTI=0x00, no body).
    // Each frame is fixed-length (2 bytes) per fixedBodyLength(), so all three
    // are extracted, including the last one at end of stream.
    uint8_t data[] = {
        0x10, 0x01,  // BCC Setup #1
        0x10, 0x01,  // BCC Setup #2
        0x10, 0x01   // BCC Setup #3
    };
    SpanByteSource src(std::span<const uint8_t>(data, std::size(data)));
    L3Framer framer(src);

    for (int i = 0; i < 3; ++i) {
        auto r = framer.nextFrame();
        ASSERT_TRUE(r.has_value()) << "frame " << i << " not extracted";
        ASSERT_EQ(r.value().data.size(), 2u);
        ASSERT_EQ(r.value().data[0], 0x10);
        ASSERT_EQ(r.value().data[1], 0x01);
    }

    auto r4 = framer.nextFrame();
    ASSERT_FALSE(r4.has_value());
}

TEST(L3Framer, GCCSetupStreamThreeFrames) {
    // Three GCC Setup frames: 00 01 02 (PD=0x00, MTI=0x00, 1-byte opaque body).
    uint8_t data[] = {
        0x00, 0x01, 0x02,  // GCC Setup #1
        0x00, 0x01, 0x02,  // GCC Setup #2
        0x00, 0x01, 0x02   // GCC Setup #3
    };
    SpanByteSource src(std::span<const uint8_t>(data, std::size(data)));
    L3Framer framer(src);

    for (int i = 0; i < 3; ++i) {
        auto r = framer.nextFrame();
        ASSERT_TRUE(r.has_value()) << "frame " << i << " not extracted";
        ASSERT_EQ(r.value().data.size(), 3u);
        ASSERT_EQ(r.value().data[0], 0x00);
        ASSERT_EQ(r.value().data[1], 0x01);
        ASSERT_EQ(r.value().data[2], 0x02);
    }

    auto r4 = framer.nextFrame();
    ASSERT_FALSE(r4.has_value());
}

TEST(L3Framer, LSRequestStreamThreeFrames) {
    // Three LS Location Service Request frames: C0 01 (PD=0x0c, MTI=0x01, no body).
    uint8_t data[] = {
        0xC0, 0x01,  // LS Request #1
        0xC0, 0x01,  // LS Request #2
        0xC0, 0x01   // LS Request #3
    };
    SpanByteSource src(std::span<const uint8_t>(data, std::size(data)));
    L3Framer framer(src);

    for (int i = 0; i < 3; ++i) {
        auto r = framer.nextFrame();
        ASSERT_TRUE(r.has_value()) << "frame " << i << " not extracted";
        ASSERT_EQ(r.value().data.size(), 2u);
        ASSERT_EQ(r.value().data[0], 0xC0);
        ASSERT_EQ(r.value().data[1], 0x01);
    }

    auto r4 = framer.nextFrame();
    ASSERT_FALSE(r4.has_value());
}

// ── Paging Response (fixed-length RR) ──────────────────────────────────

TEST(L3Framer, PagingResponseFixedLength) {
    // Paging Response: 60 0E + 5 body bytes = 7 bytes total
    uint8_t data[] = {0x60, 0x0E, 0x01, 0x02, 0x03, 0x04, 0x05,
                      0x60, 0x0D, 0x00}; // followed by Channel Release
    SpanByteSource src(std::span<const uint8_t>(data, std::size(data)));
    L3Framer framer(src);

    auto r1 = framer.nextFrame();
    ASSERT_TRUE(r1.has_value());
    ASSERT_EQ(r1.value().data.size(), 7u);

    auto r2 = framer.nextFrame();
    ASSERT_TRUE(r2.has_value());
    ASSERT_EQ(r2.value().data.size(), 3u);
}

// Test: extracted frames carry a non-zero, non-decreasing batched
// timestamp (audit N2).
// Suite name follows the existing convention of test_framer.cpp ("L3Framer").
TEST(L3Framer, Timestamp_BatchedAndSet) {
    std::vector<uint8_t> stream;
    for (int i = 0; i < 10; ++i) stream.insert(stream.end(), {0x60, 0x0D, 0x00});
    SpanByteSource src(std::span<const uint8_t>(stream.data(), stream.size()));
    L3Framer framer(src);
    double last = 0.0;
    int frames = 0;
    while (true) {
        auto res = framer.nextFrame();
        if (!res) break;
        EXPECT_GT(res.value().timestamp, 0.0);
        EXPECT_GE(res.value().timestamp, last);
        last = res.value().timestamp;
        ++frames;
    }
    EXPECT_EQ(frames, 10);
}
