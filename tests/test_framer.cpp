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
#include "gsml3parser/parser.h"
#include "gsml3parser/visitor.h"
#include "gsml3parser/message_types.h"
#include "gsml3parser/rr/l3rrmessages.h"
#include "gsml3parser/common/l3common.h"
#include <vector>

using namespace gsml3parser;

// ── Single frame extraction (header-based mode) ────────────────────────

TEST(L3Framer, SingleFixedLengthFrame) {
    // Channel Release: 06 0D 00 (PD=RR in the low nibble of octet 0, MTI=0x0D, 1 body byte)
    uint8_t data[] = {0x06, 0x0D, 0x00};
    SpanByteSource src(std::span<const uint8_t>(data, std::size(data)));
    FrameConfig cfg;
    cfg.useL2Length = false;  // header-based mode
    L3Framer framer(src, cfg);

    auto result = framer.nextFrame();
    ASSERT_TRUE(result.has_value());
    const auto& frame = result.value();
    ASSERT_EQ(frame.data.size(), 3u);
    ASSERT_EQ(frame.data[0], 0x06);
    ASSERT_EQ(frame.data[1], 0x0D);
    ASSERT_EQ(frame.data[2], 0x00);
}

TEST(L3Framer, SingleCMServiceAccept) {
    // CM Service Accept: 05 21 (PD=MM in the low nibble; MT=0x21 in the six
    // low bits of octet 1)
    uint8_t data[] = {0x05, 0x21};
    SpanByteSource src(std::span<const uint8_t>(data, std::size(data)));
    FrameConfig cfg;
    cfg.useL2Length = false;  // header-based mode
    L3Framer framer(src, cfg);

    auto result = framer.nextFrame();
    ASSERT_TRUE(result.has_value());
    const auto& frame = result.value();
    ASSERT_EQ(frame.data.size(), 2u);
}

// ── Multiple frames back-to-back ───────────────────────────────────────

TEST(L3Framer, MultipleFixedLengthFrames) {
    // RR Status frames: constant 1-byte cause body -> fixed 3 bytes each.
    uint8_t data[] = {
        0x06, 0x12, 0x41,  // RR Status #1
        0x06, 0x12, 0x42   // RR Status #2
    };
    SpanByteSource src(std::span<const uint8_t>(data, std::size(data)));
    FrameConfig cfg;
    cfg.useL2Length = false;  // header-based mode
    L3Framer framer(src, cfg);

    auto r1 = framer.nextFrame();
    ASSERT_TRUE(r1.has_value());
    ASSERT_EQ(r1.value().data.size(), 3u);

    auto r2 = framer.nextFrame();
    ASSERT_TRUE(r2.has_value());
    const auto& f2 = r2.value();
    ASSERT_EQ(f2.data.size(), 3u);
    ASSERT_EQ(f2.data[0], 0x06);
    ASSERT_EQ(f2.data[1], 0x12);
    ASSERT_EQ(f2.data[2], 0x42);

    auto r3 = framer.nextFrame();
    ASSERT_FALSE(r3.has_value());
    // SourceExhausted: the span source is at EOF.
    ASSERT_EQ(static_cast<int>(r3.error().code), static_cast<int>(ParseError::Code::SourceExhausted));
}

// ── Truncated frame at end of buffer ───────────────────────────────────

TEST(L3Framer, TruncatedFrame) {
    // Only the header of a Channel Release (need 3 bytes, only have 2).
    // Channel Release is variable-length (optional GPRS resumption
    // octet), so at end of stream the framer emits the 2-byte tail and
    // the parser rejects it downstream (fixed-length
    // framing applies only to constant-body messages).
    uint8_t data[] = {0x06, 0x0D};
    SpanByteSource src(std::span<const uint8_t>(data, std::size(data)));
    FrameConfig cfg;
    cfg.useL2Length = false;  // header-based mode
    L3Framer framer(src, cfg);

    auto result = framer.nextFrame();
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value().data.size(), 2u);
}

TEST(L3Framer, EmptySource) {
    uint8_t dummy;
    SpanByteSource src(std::span<const uint8_t>(&dummy, 0));
    FrameConfig cfg;
    cfg.useL2Length = false;  // header-based mode
    L3Framer framer(src, cfg);

    auto result = framer.nextFrame();
    ASSERT_FALSE(result.has_value());
    // SourceExhausted: the span source is at EOF.
    ASSERT_EQ(static_cast<int>(result.error().code), static_cast<int>(ParseError::Code::SourceExhausted));
}

// ── L2 length mode ─────────────────────────────────────────────────────

TEST(L3Framer, L2LengthMode) {
    // L2 length octet (0x03) + 3-byte L3 message.
    uint8_t data[] = {0x03, 0x06, 0x0D, 0x00};
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
        0x03, 0x06, 0x0D, 0x00,  // Frame 1: length=3, Channel Release
        0x02, 0x05, 0x21          // Frame 2: length=2, CM Service Accept
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
    uint8_t data[] = {0x05, 0x06, 0x0D};
    SpanByteSource src(std::span<const uint8_t>(data, std::size(data)));
    FrameConfig cfg;
    cfg.useL2Length = true;
    L3Framer framer(src, cfg);

    auto result = framer.nextFrame();
    ASSERT_FALSE(result.has_value());
    // SourceExhausted: the span source is at EOF.
    ASSERT_EQ(static_cast<int>(result.error().code), static_cast<int>(ParseError::Code::SourceExhausted));
}

// ── buffered() tracking ────────────────────────────────────────────────

TEST(L3Framer, BufferedCount) {
    uint8_t data[] = {0x06, 0x12, 0x41, 0x06, 0x12, 0x42};
    SpanByteSource src(std::span<const uint8_t>(data, std::size(data)));
    FrameConfig cfg;
    cfg.useL2Length = false;  // header-based mode
    L3Framer framer(src, cfg);

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
    // System Information Type 13 (variable-length RR, MTI=0x01) followed by
    // Channel Release. Body bytes 0x22/0x47 are chosen so their low nibbles
    // (0x02/0x04, reserved PDs) are not valid PDs, keeping the scanned
    // boundary at the real next header.
    uint8_t data[] = {
        0x06, 0x01, 0x22, 0x47,  // SI13 frame (4 bytes, scanned boundary)
        0x06, 0x0D, 0x00          // Channel Release (3 bytes)
    };
    SpanByteSource src(std::span<const uint8_t>(data, std::size(data)));
    FrameConfig cfg;
    cfg.useL2Length = false;  // header-based mode
    L3Framer framer(src, cfg);

    auto r1 = framer.nextFrame();
    ASSERT_TRUE(r1.has_value());
    ASSERT_EQ(r1.value().data.size(), 4u);

    auto r2 = framer.nextFrame();
    ASSERT_TRUE(r2.has_value());
    ASSERT_EQ(r2.value().data.size(), 3u);
}

// ── BCC/GCC/LS framing (header-based mode) ─────────────────────────────

TEST(L3Framer, BCCSetupStreamThreeFrames) {
    // Three BCC Setup frames back-to-back: 01 00 (PD=BCC in the low nibble,
    // MT=0x00, no body). Each frame is 2 bytes, so all three are extracted,
    // including the last one at end of stream.
    uint8_t data[] = {
        0x01, 0x00,  // BCC Setup #1
        0x01, 0x00,  // BCC Setup #2
        0x01, 0x00   // BCC Setup #3
    };
    SpanByteSource src(std::span<const uint8_t>(data, std::size(data)));
    FrameConfig cfg;
    cfg.useL2Length = false;  // header-based mode
    L3Framer framer(src, cfg);

    for (int i = 0; i < 3; ++i) {
        auto r = framer.nextFrame();
        ASSERT_TRUE(r.has_value()) << "frame " << i << " not extracted";
        ASSERT_EQ(r.value().data.size(), 2u);
        ASSERT_EQ(r.value().data[0], 0x01);
        ASSERT_EQ(r.value().data[1], 0x00);
    }

    auto r4 = framer.nextFrame();
    ASSERT_FALSE(r4.has_value());
}

TEST(L3Framer, GCCSetupStreamThreeFrames) {
    // Three GCC Setup frames: 00 00 22 (PD=GCC in the low nibble, MT=0x00,
    // 1-byte opaque body). GCC Setup has a variable body, so it is framed by
    // the boundary heuristic, not the fixed-length table. The body octet 0x22
    // is chosen so its low nibble (0x02, a reserved PD) is not a plausible
    // L3 header start.
    uint8_t data[] = {
        0x00, 0x00, 0x22,  // GCC Setup #1
        0x00, 0x00, 0x22,  // GCC Setup #2
        0x00, 0x00, 0x22   // GCC Setup #3
    };
    SpanByteSource src(std::span<const uint8_t>(data, std::size(data)));
    FrameConfig cfg;
    cfg.useL2Length = false;  // header-based mode
    L3Framer framer(src, cfg);

    for (int i = 0; i < 3; ++i) {
        auto r = framer.nextFrame();
        ASSERT_TRUE(r.has_value()) << "frame " << i << " not extracted";
        ASSERT_EQ(r.value().data.size(), 3u);
        ASSERT_EQ(r.value().data[0], 0x00);
        ASSERT_EQ(r.value().data[1], 0x00);
        ASSERT_EQ(r.value().data[2], 0x22);
    }

    auto r4 = framer.nextFrame();
    ASSERT_FALSE(r4.has_value());
}

TEST(L3Framer, LSRequestStreamThreeFrames) {
    // Three LS Location Service Request frames: 0C 01 (PD=LS in the low
    // nibble, MTI=0x01, no body).
    uint8_t data[] = {
        0x0C, 0x01,  // LS Request #1
        0x0C, 0x01,  // LS Request #2
        0x0C, 0x01   // LS Request #3
    };
    SpanByteSource src(std::span<const uint8_t>(data, std::size(data)));
    FrameConfig cfg;
    cfg.useL2Length = false;  // header-based mode
    L3Framer framer(src, cfg);

    for (int i = 0; i < 3; ++i) {
        auto r = framer.nextFrame();
        ASSERT_TRUE(r.has_value()) << "frame " << i << " not extracted";
        ASSERT_EQ(r.value().data.size(), 2u);
        ASSERT_EQ(r.value().data[0], 0x0C);
        ASSERT_EQ(r.value().data[1], 0x01);
    }

    auto r4 = framer.nextFrame();
    ASSERT_FALSE(r4.has_value());
}

// ── Paging Response (fixed-length RR) ──────────────────────────────────

// [GOLDEN] A real Paging Response (variable body: CKSN + classmark +
// mobile identity) framed in L2-length mode — the deterministic framing
// path for variable-length messages (a pinned 5-byte "fixed" body would
// match neither the message definition (body 7–15 bytes) nor any spec MTI).
TEST(L3Framer, PagingResponse_L2LengthMode) {
    auto pr = L3PagingResponse::builder()
        .cksn(1)
        .classmark(L3MobileStationClassmark2{})
        .mobileId(L3MobileIdentity{0x12345678u})
        .build();
    ParsedMessage pm{RRM{std::move(pr)}};
    auto wire = writeL3Bytes(pm);
    ASSERT_TRUE(wire);

    std::vector<uint8_t> data;
    data.push_back(static_cast<uint8_t>(wire.value().size()));
    data.insert(data.end(), wire.value().begin(), wire.value().end());
    data.push_back(3);
    data.push_back(0x06); data.push_back(0x0D); data.push_back(0x00);

    SpanByteSource src(std::span<const uint8_t>(data.data(), data.size()));
    FrameConfig cfg;
    cfg.useL2Length = true;
    L3Framer framer(src, cfg);

    auto r1 = framer.nextFrame();
    ASSERT_TRUE(r1.has_value());
    ASSERT_EQ(r1.value().data.size(), wire.value().size());
    auto parsed1 = parseL3(r1.value().data);
    ASSERT_TRUE(parsed1) << "the framed paging response must parse";
    EXPECT_EQ(messageMTI(*parsed1), L3PagingResponse::MTI);

    auto r2 = framer.nextFrame();
    ASSERT_TRUE(r2.has_value());
    ASSERT_EQ(r2.value().data.size(), 3u);
}

// Header-based mode: a variable-length RR frame (System Information Type
// 2quater, opaque body) followed by a Channel Release. The boundary scan
// accepts octets whose low nibble is a TS-valid PD; the body bytes
// 0x22/0x47 carry reserved low nibbles (0x02/0x04), so the heuristic
// boundary lands on the real next header.
TEST(L3Framer, VariableLengthRR_HeaderBasedHeuristic) {
    auto si = L3SystemInformationType2quater::builder().body({0x22, 0x47}).build();
    ParsedMessage pm{RRM{std::move(si)}};
    auto wire = writeL3Bytes(pm);
    ASSERT_TRUE(wire);

    std::vector<uint8_t> data = *wire;
    data.push_back(0x06); data.push_back(0x0D); data.push_back(0x00);

    SpanByteSource src(std::span<const uint8_t>(data.data(), data.size()));
    FrameConfig cfg;
    cfg.useL2Length = false;  // header-based mode
    L3Framer framer(src, cfg);

    auto r1 = framer.nextFrame();
    ASSERT_TRUE(r1.has_value());
    ASSERT_EQ(r1.value().data.size(), wire.value().size());
    auto parsed1 = parseL3(r1.value().data);
    ASSERT_TRUE(parsed1);
    EXPECT_EQ(messageMTI(*parsed1), L3SystemInformationType2quater::MTI);

    auto r2 = framer.nextFrame();
    ASSERT_TRUE(r2.has_value());
    ASSERT_EQ(r2.value().data.size(), 3u);
}

// Test: extracted frames carry a non-zero, non-decreasing batched
// timestamp.
// Suite name follows the existing convention of test_framer.cpp ("L3Framer").
TEST(L3Framer, Timestamp_BatchedAndSet) {
    std::vector<uint8_t> stream;
    // RR Status frames: fixed 3 bytes each per the fixed-length table.
    for (int i = 0; i < 10; ++i) stream.insert(stream.end(), {0x06, 0x12, 0x41});
    SpanByteSource src(std::span<const uint8_t>(stream.data(), stream.size()));
    FrameConfig cfg;
    cfg.useL2Length = false;  // header-based mode
    L3Framer framer(src, cfg);
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
