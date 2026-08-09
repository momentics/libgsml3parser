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
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

using namespace gsml3parser;

namespace {

std::string readFileAsString(std::string_view path) {
    std::ifstream ifs(path.data());
    if (!ifs.is_open()) return {};
    std::ostringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
}

void printMsgDetails(const ParsedMessage& msg) {
    std::cout << "  PD: " << static_cast<int>(messagePD(msg)) << "\n";
    std::cout << "  MTI: 0x" << std::hex << messageMTI(msg) << std::dec << "\n";
    std::cout << "  Name: " << messageName(msg) << "\n";
}

// Demo: parse multiple hex frames and display their types
void demoBatchParse(const std::vector<std::string>& hexFrames) {
    for (const auto& hex : hexFrames) {
        auto result = parseL3Hex(hex);
        if (result) {
            const auto& msg = *result;
            std::cout << "[OK] " << messageName(msg)
                      << " (PD=" << static_cast<int>(messagePD(msg))
                      << ", MTI=0x" << std::hex << messageMTI(msg) << std::dec << ")\n";

            // Round-trip: serialize back to hex and compare
            auto reHex = writeL3Hex(msg);
            if (reHex) {
                std::cout << "    Round-trip: " << *reHex << "\n";
            }
        } else {
            std::cout << "[FAIL] " << result.error().message << "\n";
        }
    }
}

} // anonymous namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <hex_string_or_file>\n";
        return 1;
    }

    std::string_view input{argv[1]};
    std::string content;

    // Try to read as file first
    content = readFileAsString(input);
    if (content.empty()) {
        content = std::string{input};
    }

    // Parse using the new Expected<ParsedMessage> API
    auto result = parseL3Hex(content);

    if (result) {
        const auto& msg = *result;

        // Typed access via tryGet — no dynamic_cast needed
        if (auto* cr = tryGet<L3ChannelRelease>(msg)) {
            std::cout << "Channel Release detected\n";
        } else if (auto* paging = tryGet<L3PagingRequestType1>(msg)) {
            std::cout << "Paging Request Type 1 detected\n";
        } else if (auto* setup = tryGet<L3Setup>(msg)) {
            std::cout << "CC Setup detected\n";
        }

        printMsgDetails(msg);

        // Serialize back to hex for round-trip verification
        auto hexOut = writeL3Hex(msg);
        if (hexOut) {
            std::cout << "Serialized: " << *hexOut << "\n";
        }
    } else {
        std::cerr << "Failed to parse: " << result.error().message << "\n";
        return 1;
    }

    // Demo batch parsing with multiple example frames
    std::cout << "\n--- Batch Parse Demo ---\n";
    std::vector<std::string> batch{
        "600D00",            // Channel Release (RR)
        "5084",              // CM Service Accept (MM)
        "3E9408021621",      // CC Disconnect (CC, TI=7)
        content
    };
    demoBatchParse(batch);

    return 0;
}
