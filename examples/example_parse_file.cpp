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

// Demonstrates parsing L3 messages from hex strings or files, typed access
// via tryGet<>, round-trip serialization, and batch parsing across all 12 PD
// domains (RR, MM, CC, SS, GMM, SM, SMS, BCC, GCC, LS, EXT, TST).

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
    std::cout << "  PD: " << messagePD(msg) << "\n";
    std::cout << "  MTI: 0x" << std::hex << messageMTI(msg) << std::dec << "\n";
    std::cout << "  Name: " << messageName(msg) << "\n";
}

// Demo: parse multiple hex frames from all 9 PD domains and display their types
void demoBatchParse(const std::vector<std::pair<std::string, std::string>>& hexFrames) {
    for (const auto& [label, hex] : hexFrames) {
        auto result = parseL3Hex(hex);
        if (result) {
            const auto& msg = *result;
            std::cout << "[OK] " << label << ": " << messageName(msg)
                       << " (PD=" << messagePD(msg)
                      << ", MTI=0x" << std::hex << messageMTI(msg) << std::dec << ")\n";

            // Round-trip: serialize back to hex and compare
            auto reHex = writeL3Hex(msg);
            if (reHex) {
                std::cout << "    Round-trip: " << *reHex << "\n";
            }
        } else {
            std::cout << "[FAIL] " << label << ": " << result.error().message << "\n";
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

    // Parse using the Expected<ParsedMessage> API
    auto result = parseL3Hex(content);

    if (result) {
        const auto& msg = *result;

        // Typed access via tryGet - covers all 9 PD domains
        if (auto* cr = tryGet<L3ChannelRelease>(msg)) {
            std::cout << "RR: Channel Release detected\n";
        } else if (auto* paging = tryGet<L3PagingRequestType1>(msg)) {
            std::cout << "RR: Paging Request Type 1 detected\n";
        } else if (auto* setup = tryGet<L3Setup>(msg)) {
            std::cout << "CC: Setup detected\n";
        } else if (auto* fac = tryGet<L3SupServFacilityMessage>(msg)) {
            std::cout << "SS: Facility detected\n";
        } else if (auto* attach = tryGet<L3AttachRequest>(msg)) {
            std::cout << "GMM: Attach Request detected\n";
        } else if (auto* pdu = tryGet<L3ActivatePDPContextRequest>(msg)) {
            std::cout << "SM: Activate PDP Context Request detected\n";
        } else if (auto* cpd = tryGet<L3CPData>(msg)) {
            std::cout << "SMS: CP Data detected\n";
        } else if (auto* bccSetup = tryGet<L3BCCSetup>(msg)) {
            std::cout << "BCC: Setup detected\n";
        } else if (auto* gccSetup = tryGet<L3GCCSetup>(msg)) {
            std::cout << "GCC: Setup detected\n";
        } else if (auto* lsReq = tryGet<L3LocationServiceRequest>(msg)) {
            std::cout << "LS: LocationServiceRequest detected\n";
        } else if (auto* lsProv = tryGet<L3LocationServiceProviderMessage>(msg)) {
            std::cout << "LS: LocationServiceProviderMessage detected\n";
        } else if (auto* ext = tryGet<L3ExtendedMessage>(msg)) {
            std::cout << "EXT: ExtendedMessage detected\n";
        } else if (auto* tst = tryGet<L3TestProcedureMessage>(msg)) {
            std::cout << "TST: TestProcedureMessage detected\n";
        } else {
            std::cout << "Parsed message type: " << messageName(msg) << "\n";
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

    // Demo batch parsing with representative messages from all 9 PD domains
    std::cout << "\n--- Batch Parse Demo (All 12 PD Domains) ---\n";
    std::vector<std::pair<std::string, std::string>> batch{
        {"RR",       "60 0D 00"},                          // Channel Release
        {"MM",       "50 84"},                              // CM Service Accept
        {"CC",       "3E 94 08 02 16 21"},                  // Disconnect (TI=7)
        {"SS",       "B0 E8"},                              // Facility
        {"GMM",      "80 20 05"},                            // GMM Status (cause=5)
        {"SM",       "A0 55 32 01 05"},                      // SM Status (cause=5)
        {"SMS",      "90 04 01 02"},                         // CP Ack (ref=2)
        {"BCC",      "10 01"},                               // BCC Setup
        {"GCC",      "00 01 02"},                            // GCC Setup
        {"LS",       "C0 01"},                               // LocationServiceRequest
        {"EXT",      "E0 01"},                               // ExtendedMessage
        {"TST",      "F0 01"},                               // TestProcedureMessage
    };
    demoBatchParse(batch);

    return 0;
}
