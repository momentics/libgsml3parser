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

#include <gsml3parser/gsml3parser.hpp>
#include <chrono>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using namespace gsml3parser;

namespace {

// Convert a hex string to bytes.
std::vector<uint8_t> hexToBytes(std::string_view hex) {
    std::vector<uint8_t> out;
    for (size_t i = 0; i + 1 < hex.size(); i += 2) {
        uint8_t b = 0;
        for (int j = 0; j < 2; ++j) {
            char c = hex[i + j];
            b <<= 4;
            if (c >= '0' && c <= '9') b |= static_cast<uint8_t>(c - '0');
            else if (c >= 'a' && c <= 'f') b |= static_cast<uint8_t>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') b |= static_cast<uint8_t>(c - 'A' + 10);
        }
        out.push_back(b);
    }
    return out;
}

// Simple handler that prints frame info.
class PrintingFrameHandler : public FrameHandler {
public:
    void onFrame(const ParsedMessage& msg, const ExtractedFrame& raw) override {
        std::cout << "  Frame[" << raw.data.size() << "B]: "
                  << messageName(msg)
                  << " (PD=" << static_cast<int>(messagePD(msg))
                  << ", MTI=0x" << std::hex << messageMTI(msg) << std::dec << ")\n";
    }

    void onError(const ParseError& err, std::span<const uint8_t> rawData) override {
        std::cout << "  Error: " << err.message
                  << " at bit " << err.bitPosition
                  << " (" << rawData.size() << " bytes)\n";
    }

    void onStats(const StreamStats& stats) override {
        std::cout << "  Stats: " << stats.totalFrames << " frames, "
                  << stats.parsedOk << " ok, " << stats.parseErrors << " errors\n";
    }
};

// Demo 1: Parse a batch of frames from memory using SpanByteSource.
void demoSpanSource() {
    std::cout << "=== SpanByteSource Demo ===\n";

    // Simulated capture data: multiple L3 messages concatenated.
    // Each message is prefixed with its L2 length octet.
    std::vector<std::string> hexMessages{
        "600D00",             // Channel Release (RR) - PD=0x6, MTI=0x0D, Cause=Normal_Event
        "5084",               // CM Service Accept (MM) - PD=0x5, MTI=0x21, empty body
        "3E9408021621",       // CC Disconnect (CC) - PD=0x3, TI=7, MTI=0x25, Cause TLV
    };

    // Concatenate all messages into one buffer with L2 length prefixes.
    std::vector<uint8_t> buffer;
    for (const auto& hex : hexMessages) {
        auto bytes = hexToBytes(hex);
        uint8_t len = static_cast<uint8_t>(bytes.size());
        buffer.push_back(len);
        buffer.insert(buffer.end(), bytes.begin(), bytes.end());
    }

    SpanByteSource source(std::span{buffer});
    PrintingFrameHandler handler;

    L3StreamProcessor processor(source, {}, FrameConfig{.useL2Length = true});
    processor.processUntilEOF(handler);

    const auto& stats = processor.stats();
    std::cout << "Total: " << stats.totalFrames << " frames parsed\n\n";
}

// Demo 2: Producer/consumer with RingBuffer.
void demoRingBuffer() {
    std::cout << "=== RingBuffer Producer/Consumer Demo ===\n";

    RingBuffer ring(4096);

    // Producer: write frames asynchronously.
    std::vector<std::string> hexMessages{
        "600D00",             // Channel Release (RR)
        "5084",               // CM Service Accept (MM)
    };

    auto producerThread = std::thread([&ring, &hexMessages]() {
        for (const auto& hex : hexMessages) {
            auto bytes = hexToBytes(hex);
            uint8_t len = static_cast<uint8_t>(bytes.size());
            ring.write(&len, 1);
            ring.write(bytes.data(), bytes.size());
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    // Consumer: read and parse frames.
    PrintingFrameHandler handler;
    {
        L3StreamProcessor processor(ring, {}, FrameConfig{.useL2Length = true});

        // Process frames as they arrive (non-blocking loop).
        int emptyCount = 0;
        while (emptyCount < 20) {
            if (!processor.processOne([&](const ParsedMessage& msg) {
                std::cout << "  Got: " << messageName(msg) << "\n";
            })) {
                ++emptyCount;
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            } else {
                emptyCount = 0;
            }
        }
    }

    producerThread.join();
    std::cout << "RingBuffer demo complete\n\n";
}

// Demo 3: L3StreamBuilder fluent API.
void demoBuilder() {
    std::cout << "=== L3StreamBuilder Demo ===\n";

    // Build a processor from inline data.
    std::vector<uint8_t> data = hexToBytes("600D00");  // Channel Release
    uint8_t len = static_cast<uint8_t>(data.size());
    data.insert(data.begin(), len);  // prepend L2 length

    auto processor = L3StreamBuilder()
        .source(std::span{data})
        .useL2Length(true)
        .build();

    PrintingFrameHandler handler;
    processor->processUntilEOF(handler);
    std::cout << "Builder demo complete\n\n";
}

} // anonymous namespace

int main() {
    demoSpanSource();
    demoRingBuffer();
    demoBuilder();

    std::cout << "All streaming demos completed successfully.\n";
    return 0;
}
