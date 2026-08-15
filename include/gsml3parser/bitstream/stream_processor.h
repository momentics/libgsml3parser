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
#include <concepts>
#include <cstdint>
#include <memory>
#include <utility>

#include "gsml3parser/expected.h"
#include "gsml3parser/parser_config.h"
#include "gsml3parser/message_types.h"
#include "gsml3parser/parser.h"
#include "gsml3parser/visitor.h"
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
    uint64_t gmmMessages{};
    uint64_t smMessages{};
    uint64_t smsMessages{};
    uint64_t bccMessages{};
    uint64_t gccMessages{};
    uint64_t lsMessages{};
    uint64_t extendedMessages{};
    uint64_t testprocMessages{};
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
    virtual void onError(const ParseError&, std::span<const uint8_t>) {}

    /** Called periodically with updated statistics. Default: no-op. */
    virtual void onStats(const StreamStats&) {}
};

/**
 * High-throughput streaming L3 parser.
 *
 * Reads raw bytes from a ByteSource, frames them using L3Framer,
 * and parses each frame using parseL3().  Statistics are tracked
 * in StreamStats.
 */
class L3StreamProcessor {
    std::unique_ptr<ByteSource> mOwnedSource;
    ByteSource* mSource{};
    L3Framer mFramer;
    ParserConfig mConfig;
    StreamStats mStats{};

 public:
    L3StreamProcessor(ByteSource& source, ParserConfig cfg = {}, FrameConfig fcfg = {});
    L3StreamProcessor(std::unique_ptr<ByteSource> source, ParserConfig cfg = {}, FrameConfig fcfg = {});

    /**
     * Non-blocking: process one frame if available.
     * @return true if a frame was processed (success or error).
     */
    template<typename F>
        requires std::is_invocable_v<F, const ParsedMessage&>
    bool processOne(F&& handler);

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

// ── Template method definitions (must be in header) ────────────────────

template<typename F>
    requires std::is_invocable_v<F, const ParsedMessage&>
bool L3StreamProcessor::processOne(F&& handler) {
    auto frameResult = mFramer.nextFrame();
    if (!frameResult) {
        const auto& err = frameResult.error();
        if (err.code == ParseError::Code::TruncatedInput) {
            mStats.truncatedInputs++;
        }
        return false;
    }

    const auto& frame = frameResult.value();
    mStats.totalBytes += frame.data.size();
    mStats.totalFrames++;

    auto msgResult = parseL3(frame.data, mConfig);
    if (!msgResult) {
        mStats.parseErrors++;
        return false;
    }

    mStats.parsedOk++;

    // Categorize by protocol discriminator. O(1) switch dispatch.
    switch (messagePD(*msgResult)) {
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

    handler(*msgResult);
    return true;
}

} // namespace gsml3parser
