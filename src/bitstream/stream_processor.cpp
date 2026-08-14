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

#include "gsml3parser/bitstream/stream_processor.h"
#include "gsml3parser/parser.h"
#include "gsml3parser/visitor.h"
#include <cstdio>

namespace gsml3parser {

// ── L3StreamProcessor ──────────────────────────────────────────────────

L3StreamProcessor::L3StreamProcessor(ByteSource& source, ParserConfig cfg, FrameConfig fcfg)
    : mSource(&source), mFramer(source, std::move(fcfg)), mConfig(std::move(cfg)) {}

L3StreamProcessor::L3StreamProcessor(std::unique_ptr<ByteSource> source, ParserConfig cfg, FrameConfig fcfg)
    : mOwnedSource(std::move(source)), mSource(mOwnedSource.get()),
      mFramer(*mOwnedSource, std::move(fcfg)), mConfig(std::move(cfg)) {}

bool L3StreamProcessor::processOne(std::function<void(const ParsedMessage&)> handler) {
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

    // Categorize by protocol discriminator.
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

    if (handler) handler(*msgResult);
    return true;
}

void L3StreamProcessor::processUntilEOF(FrameHandler& handler) {
    while (true) {
        auto frameResult = mFramer.nextFrame();
        if (!frameResult) {
            const auto& err = frameResult.error();
            if (err.code == ParseError::Code::TruncatedInput) {
                mStats.truncatedInputs++;
                break; // No more data available.
            }
            // Other errors (corrupt frames) - continue processing.
            mStats.parseErrors++;
            handler.onError(err, {});
            continue;
        }

        const auto& frame = frameResult.value();
        mStats.totalBytes += frame.data.size();
        mStats.totalFrames++;

        auto msgResult = parseL3(frame.data, mConfig);
        if (!msgResult) {
            mStats.parseErrors++;
            handler.onError(msgResult.error(), frame.data);
            continue;
        }

        mStats.parsedOk++;

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

        handler.onFrame(*msgResult, frame);
        handler.onStats(mStats);
    }
}

void L3StreamProcessor::processN(size_t count, FrameHandler& handler) {
    size_t processed = 0;
    while (processed < count) {
        auto frameResult = mFramer.nextFrame();
        if (!frameResult) {
            const auto& err = frameResult.error();
            if (err.code == ParseError::Code::TruncatedInput) {
                mStats.truncatedInputs++;
                break;
            }
            mStats.parseErrors++;
            handler.onError(err, {});
            continue;
        }

        const auto& frame = frameResult.value();
        mStats.totalBytes += frame.data.size();
        mStats.totalFrames++;
        processed++;

        auto msgResult = parseL3(frame.data, mConfig);
        if (!msgResult) {
            mStats.parseErrors++;
            handler.onError(msgResult.error(), frame.data);
            continue;
        }

        mStats.parsedOk++;

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

        handler.onFrame(*msgResult, frame);
    }
}

void L3StreamProcessor::resetStats() {
    mStats = StreamStats{};
}

// ── L3StreamBuilder ────────────────────────────────────────────────────

L3StreamBuilder& L3StreamBuilder::source(ByteSource& src) {
    mSource = &src;
    mOwnedSource.reset();
    return *this;
}

L3StreamBuilder& L3StreamBuilder::source(std::span<const uint8_t> data) {
    mOwnedSource = std::make_unique<SpanByteSource>(data);
    mSource = mOwnedSource.get();
    return *this;
}

L3StreamBuilder& L3StreamBuilder::sourceFile(const char* path) {
    std::FILE* f = std::fopen(path, "rb");
    if (f) {
        mOwnedSource = std::make_unique<FileByteSource>(f);
        mSource = mOwnedSource.get();
    }
    return *this;
}

L3StreamBuilder& L3StreamBuilder::logLevel(LogLevel lvl) {
    mConfig = mConfig.withLogLevel(lvl);
    return *this;
}

L3StreamBuilder& L3StreamBuilder::pdHandler(L3PD pd, PDHandler handler) {
    mConfig = mConfig.withPDHandler(pd, std::move(handler));
    return *this;
}

L3StreamBuilder& L3StreamBuilder::useL2Length(bool v) {
    mFrameConfig.useL2Length = v;
    return *this;
}

L3StreamBuilder& L3StreamBuilder::maxMessageLength(size_t v) {
    mFrameConfig.maxMessageLength = v;
    return *this;
}

L3StreamBuilder& L3StreamBuilder::ringBufferSize(size_t v) {
    mRingBufferSize = v;
    return *this;
}

std::unique_ptr<L3StreamProcessor> L3StreamBuilder::build() {
    if (!mSource) {
        // Default: create an empty RingBuffer.
        mOwnedSource = std::make_unique<RingBuffer>(mRingBufferSize);
        mSource = mOwnedSource.get();
    }
    // Transfer ownership of the ByteSource to the processor when we own it.
    if (mOwnedSource) {
        return std::make_unique<L3StreamProcessor>(std::move(mOwnedSource), mConfig, mFrameConfig);
    }
    // External source: use reference-based constructor.
    return std::make_unique<L3StreamProcessor>(*mSource, mConfig, mFrameConfig);
}

} // namespace gsml3parser
