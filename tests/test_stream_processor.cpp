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
#include "gsml3parser/bitstream/stream_processor.h"
#include "gsml3parser/bitstream/byte_source.h"
#include "gsml3parser/parser.h"
#include "gsml3parser/visitor.h"
#include <vector>
#include <functional>

using namespace gsml3parser;

// Minimal FrameHandler for testing.
class TestHandler : public FrameHandler {
public:
    std::vector<const ParsedMessage*> messages;
    std::vector<ParseError> errors;
    size_t statsCalls{0};

    void onFrame(const ParsedMessage& msg, const ExtractedFrame&) override {
        // Store address of msg (valid for duration of test).
        messages.push_back(&msg);
    }

    void onError(const ParseError& err, std::span<const uint8_t>) override {
        errors.push_back(err);
    }

    void onStats(const StreamStats&) override {
        statsCalls++;
    }
};

// ── SpanByteSource with known frames ───────────────────────────────────

TEST(L3StreamProcessor, ParseAllFrames) {
    // Three Channel Release messages.
    uint8_t data[] = {
        0x60, 0x0D, 0x00,  // Channel Release #1
        0x60, 0x0D, 0x01,  // Channel Release #2
        0x60, 0x0D, 0x02   // Channel Release #3
    };
    SpanByteSource src(std::span<const uint8_t>(data, std::size(data)));
    L3StreamProcessor proc(src);

    std::vector<int> mtis;
    while (proc.processOne([&mtis](const ParsedMessage& msg) {
        if (tryGet<L3ChannelRelease>(msg)) {
            mtis.push_back(L3ChannelRelease::MTI);
        }
    })) {}

    ASSERT_EQ(mtis.size(), 3u);
    ASSERT_EQ(mtis[0], 0x0D);
    ASSERT_EQ(mtis[1], 0x0D);
    ASSERT_EQ(mtis[2], 0x0D);

    const auto& stats = proc.stats();
    ASSERT_EQ(stats.parsedOk, 3u);
    ASSERT_EQ(stats.parseErrors, 0u);
    ASSERT_EQ(stats.rrMessages, 3u);
    ASSERT_EQ(stats.totalFrames, 3u);
}

// ── Stats tracking ─────────────────────────────────────────────────────

TEST(L3StreamProcessor, StatsTracking) {
    uint8_t data[] = {
        0x60, 0x0D, 0x00,  // RR: Channel Release
        0x50, 0x84,          // MM: CM Service Accept
    };
    SpanByteSource src(std::span<const uint8_t>(data, std::size(data)));
    L3StreamProcessor proc(src);

    TestHandler h;
    proc.processUntilEOF(h);

    const auto& stats = proc.stats();
    ASSERT_EQ(stats.parsedOk, 2u);
    ASSERT_EQ(stats.rrMessages, 1u);
    ASSERT_EQ(stats.mmMessages, 1u);
}

// ── Error handling: corrupt frames continue processing ─────────────────

TEST(L3StreamProcessor, CorruptFrameContinues) {
    // Invalid frame followed by valid one.
    uint8_t data[] = {
        0x60, 0x0D, 0x00,  // Valid Channel Release
        0xFF, 0xFF,          // Invalid (PD=0xF TestProcedure, treated as variable)
        0x60, 0x0D, 0x01    // Valid Channel Release
    };
    SpanByteSource src(std::span<const uint8_t>(data, std::size(data)));
    L3StreamProcessor proc(src);

    TestHandler handler;
    proc.processUntilEOF(handler);

    const auto& stats = proc.stats();
    // At least the first valid frame should be parsed.
    ASSERT_GT(stats.parsedOk, 0u);
}

// ── Empty source: processUntilEOF returns immediately ──────────────────

TEST(L3StreamProcessor, EmptySource) {
    uint8_t dummy;
    SpanByteSource src(std::span<const uint8_t>(&dummy, 0));
    L3StreamProcessor proc(src);

    TestHandler handler;
    proc.processUntilEOF(handler);

    const auto& stats = proc.stats();
    ASSERT_EQ(stats.totalFrames, 0u);
    ASSERT_EQ(stats.truncatedInputs, 1u);
}

// ── processN ───────────────────────────────────────────────────────────

TEST(L3StreamProcessor, ProcessN) {
    uint8_t data[] = {
        0x60, 0x0D, 0x00,  // #1
        0x60, 0x0D, 0x01,  // #2
        0x60, 0x0D, 0x02,  // #3
        0x60, 0x0D, 0x03   // #4
    };
    SpanByteSource src(std::span<const uint8_t>(data, std::size(data)));
    L3StreamProcessor proc(src);

    TestHandler handler;
    proc.processN(2, handler);

    ASSERT_EQ(handler.messages.size(), 2u);
    const auto& stats = proc.stats();
    ASSERT_EQ(stats.totalFrames, 2u);
}

// ── resetStats ─────────────────────────────────────────────────────────

TEST(L3StreamProcessor, ResetStats) {
    uint8_t data[] = {0x60, 0x0D, 0x00};
    SpanByteSource src(std::span<const uint8_t>(data, std::size(data)));
    L3StreamProcessor proc(src);

    TestHandler h;
    proc.processUntilEOF(h);
    ASSERT_GT(proc.stats().parsedOk, 0u);

    proc.resetStats();
    const auto& stats = proc.stats();
    ASSERT_EQ(stats.parsedOk, 0u);
    ASSERT_EQ(stats.totalFrames, 0u);
}

// ── L3StreamBuilder fluent API ─────────────────────────────────────────

TEST(L3StreamBuilder, BuildFromSpan) {
    uint8_t data[] = {0x60, 0x0D, 0x00};
    auto proc = L3StreamBuilder()
        .source(std::span<const uint8_t>(data, std::size(data)))
        .build();

    ASSERT_TRUE(proc != nullptr);

    TestHandler handler;
    proc->processUntilEOF(handler);
    ASSERT_EQ(handler.messages.size(), 1u);
}

TEST(L3StreamBuilder, BuildWithL2Length) {
    uint8_t data[] = {0x03, 0x60, 0x0D, 0x00}; // L2 len=3 + Channel Release
    auto proc = L3StreamBuilder()
        .source(std::span<const uint8_t>(data, std::size(data)))
        .useL2Length(true)
        .build();

    ASSERT_TRUE(proc != nullptr);

    TestHandler handler;
    proc->processUntilEOF(handler);
    ASSERT_EQ(handler.messages.size(), 1u);
}

// ── Mixed message types ────────────────────────────────────────────────

TEST(L3StreamProcessor, MixedMessageTypes) {
    uint8_t data[] = {
        0x60, 0x0D, 0x00,  // RR: Channel Release
        0x50, 0x84,          // MM: CM Service Accept
        0x60, 0x0E, 0x01, 0x02, 0x03, 0x04, 0x05,  // RR: Paging Response (7 bytes)
    };
    SpanByteSource src(std::span<const uint8_t>(data, std::size(data)));
    L3StreamProcessor proc(src);

    TestHandler handler;
    proc.processUntilEOF(handler);

    const auto& stats = proc.stats();
    ASSERT_EQ(stats.parsedOk, 3u);
    ASSERT_EQ(stats.rrMessages, 2u);
    ASSERT_EQ(stats.mmMessages, 1u);
}
