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
#include <gsml3parser/rr/l3rrmessages.h>
#include <gsml3parser/cc/l3ccmessages.h>
#include <fstream>
#include <iostream>
#include <sstream>

using namespace gsml3parser;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <hex_string_or_file>\n";
        return 1;
    }

    std::string input = argv[1];

    // Try to read as file first
    std::ifstream ifs(input);
    if (ifs.is_open()) {
        std::ostringstream ss;
        ss << ifs.rdbuf();
        input = ss.str();
    }

    // Parse as hex string
    ParserContext ctx;
    auto msg = parseL3Hex(input, ctx);
    if (msg) {
        std::cout << msg->text() << "\n";

        if (auto* rr = dynamic_cast<L3RRMessage*>(msg.get())) {
            std::cout << "  PD: RadioResource\n";
            std::cout << "  MTI: " << L3RRMessage::name(
                static_cast<L3RRMessage::MessageType>(rr->MTI())) << "\n";
        } else if (auto* cc = dynamic_cast<L3CCMessage*>(msg.get())) {
            std::cout << "  PD: CallControl\n";
            std::cout << "  TI: " << cc->TI() << "\n";
        }
    } else {
        std::cerr << "Failed to parse: " << input << "\n";
        return 1;
    }

    return 0;
}
