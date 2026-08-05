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

#include <gsml3parser/parser.h>
#include <gsml3parser/context.h>
#include <gsml3parser/arena.h>
#include <gsml3parser/bitvector.h>
#include <gsml3parser/rr/l3rrmessages.h>
#include <gsml3parser/mm/l3mmmessages.h>
#include <gsml3parser/cc/l3ccmessages.h>
#include <gsml3parser/ss/l3ssmessages.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <span>
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

void printMsgDetails(const L3Message& msg) {
    if (auto* rr = dynamic_cast<const L3RRMessage*>(&msg)) {
        std::cout << "  PD: RadioResource\n";
        std::cout << "  MTI: " << L3RRMessage::name(
            static_cast<L3RRMessage::MessageType>(rr->mti())) << "\n";
    } else if (auto* mm = dynamic_cast<const L3MMMessage*>(&msg)) {
        std::cout << "  PD: MobilityManagement\n";
    } else if (auto* cc = dynamic_cast<const L3CCMessage*>(&msg)) {
        std::cout << "  PD: CallControl\n";
        std::cout << "  TI: " << cc->ti() << "\n";
    } else if (auto* ss = dynamic_cast<const L3SupServMessage*>(&msg)) {
        std::cout << "  PD: SupplementaryServices\n";
        std::cout << "  TI: " << ss->ti() << "\n";
    }
}

// Demo: use Arena for batch BitVector allocation
void demoArenaBatch(const std::vector<std::string>& hexFrames) {
    Arena arena(16384);
    size_t totalParsed = 0;

    for (const auto& hex : hexFrames) {
        arena.reset();
        BitVector bv(arena, hex.size() * 4);
        ++totalParsed;
    }

    std::cout << "[Arena] Processed " << totalParsed
              << " frames, arena used: " << arena.used() << " bytes\n";
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

    // Parse with explicit ParserContext
    ParserContext ctx;
    auto msg = parseL3Hex(content, ctx);

    if (msg) {
        std::cout << msg->text() << "\n";
        printMsgDetails(*msg);
    } else {
        std::cerr << "Failed to parse: " << content << "\n";
        return 1;
    }

    // Demo Arena batch parsing with multiple frames
    std::vector<std::string> batch{
        "06270460001",
        "05080460001",
        content
    };
    demoArenaBatch(batch);

    return 0;
}
