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
#include <functional>
#include <memory>

#include "gsml3parser/expected.h"
#include "gsml3parser/parser_config.h"
#include "gsml3parser/message_types.h"
#include "gsml3parser/bitstream/byte_source.h"
#include "gsml3parser/bitstream/framer.h"

namespace gsml3parser {

// Forward declaration.
class L3Framer;

/** Running statistics for stream processing. */
struct StreamStats {
    uint64_t totalBytes{};
    uint64_t totalFrames{};
    uint64_t parsedOk{};
    uint64_t parseErrors{};
    uint64_t truncatedInputs{};
    uint64_t unsupportedPD{};
    uint64_t rrMessages{};
    uint64_t mmMessages{};
    uint64_t ccMessages{};
    uint64_t ssMessages{};
};

/**
 * Callback interface for processed frames.
 *
 * Implement onFrame() to handle each successfully parsed message.
 * onError() and onStats() have default empty implementations.
 */
class FrameHandler {
public:
    virtual ~FrameHandler() = default;

    /** Called for each successfully parsed L3 message. */
    virtual void onFrame(const ParsedMessage& msg, const ExtractedFrame& raw) = 0;

    /** Called when a frame fails to parse. Default: no-op. */
    virtual void onError(const ParseError& err, std::span<const uint8_t> rawData) {}

    /** Called periodically with updated statistics. Default: no-op. */
    virtual void onStats(const StreamStats& stats) {}
};

/**
 * High-throughput streaming L3 parser.
 *
 * Reads raw bytes from a ByteSource, frames them using L3Framer,
 * and parses each frame using parseL3().  Statistics are tracked
 * in StreamStats.
 */
class L3StreamProcessor {
    ByteSource& mSource;
    L3Framer mFramer;
    ParserConfig mConfig;
    StreamStats mStats{};

public:
    L3StreamProcessor(ByteSource& source, ParserConfig cfg = {}, FrameConfig fcfg = {});

    /**
     * Non-blocking: process one frame if available.
     * @return true if a frame was processed (success or error).
     */
    bool processOne(std::function<void(const ParsedMessage&)> handler);

    /** Blocking: process all frames until the source is exhausted. */
    void processUntilEOF(FrameHandler& handler);

    /** Process exactly @p count frames (or until EOF). */
    void processN(size_t count, FrameHandler& handler);

    [[nodiscard]] const StreamStats& stats() const noexcept { return mStats; }
    void resetStats();
};

/**
 * Builder for L3StreamProcessor with a fluent API.
 * Manages ownership of the ByteSource when using inline data or files.
 */
class L3StreamBuilder {
    std::unique_ptr<ByteSource> mOwnedSource;
    ByteSource* mSource{};
    ParserConfig mConfig;
    FrameConfig mFrameConfig;
    size_t mRingBufferSize{262144};

public:
    L3StreamBuilder& source(ByteSource& src);
    L3StreamBuilder& source(std::span<const uint8_t> data);
    L3StreamBuilder& sourceFile(const char* path);
    L3StreamBuilder& logLevel(LogLevel lvl);
    L3StreamBuilder& pdHandler(L3PD pd, PDHandler handler);
    L3StreamBuilder& useL2Length(bool v);
    L3StreamBuilder& maxMessageLength(size_t v);
    L3StreamBuilder& ringBufferSize(size_t v);
    [[nodiscard]] std::unique_ptr<L3StreamProcessor> build();
};

} // namespace gsml3parser
