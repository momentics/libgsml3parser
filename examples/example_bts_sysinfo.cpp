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

// BTS System Information broadcast demo: build SI1, SI2, SI3, SI4 via
// Builder, serialize each to L3 bytes, wrap in LAPDm frames for BCCH
// transmission, and verify round-trip parsing of each message type.
// Reference: GSM 04.08 9.1.20 (SI1) / 9.1.20a (SI2) / 9.1.20b (SI3) / 9.1.20c (SI4)

#include <gsml3parser/gsml3parser.hpp>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <span>
#include <string>
#include <vector>

using namespace gsml3parser;
using namespace gsml3parser::lapdm;

namespace {

// Print a byte vector as space-separated hex.
std::string bytesToHex(std::span<const uint8_t> data) {
    std::ostringstream oss;
    oss << std::hex << std::uppercase << std::setfill('0');
    for (size_t i = 0; i < data.size(); ++i) {
        if (i > 0) oss << ' ';
        oss << std::setw(2) << static_cast<int>(data[i]);
    }
    return oss.str();
}

// Generic round-trip: build SI -> serialize -> LAPDm -> unwrap -> parse -> verify.
bool roundTrip(const char* name, ParsedMessage&& pm) {
    auto l3Bytes = writeL3Bytes(pm);
    if (!l3Bytes) {
        std::cerr << "  ERROR: writeL3Bytes failed for " << name << "\n";
        return false;
    }

    auto lapdmFrame = wrapL3(*l3Bytes, SAPI::SAPI0, false);
    auto unwrapped = unwrapL3(lapdmFrame);
    if (!unwrapped) {
        std::cerr << "  ERROR: unwrapL3 failed for " << name << "\n";
        return false;
    }

    auto reparsed = parseL3(*unwrapped);
    if (!reparsed) {
        std::cerr << "  ERROR: parseL3 failed for " << name << "\n";
        return false;
    }

    std::cout << "  " << name << ": L3=" << (*l3Bytes).size() << "B, "
              << "LAPDm=" << lapdmFrame.size() << "B, "
              << "PD=" << messagePD(*reparsed)
              << ", MTI=0x" << std::hex << messageMTI(*reparsed)
              << " -- OK\n";
    return true;
}

// Build SI1: Cell Channel Description (frequency list) + RACH parameters.
void demoSI1() {
    std::cout << "=== System Information Type 1 ===\n";
    std::cout << "Contains frequency list and RACH control parameters\n\n";

    auto si1 = L3SystemInformationType1::builder()
        .cellChannelDescription(L3FrequencyList())
        .rachControlParameters(L3RACHControlParameters())
        .build();

    if (!roundTrip("SI1", ParsedMessage{RRM{std::move(si1)}})) {
        std::exit(1);
    }
    std::cout << "\n";
}

// Build SI2: Neighbour cell list + RACH parameters.
void demoSI2() {
    std::cout << "=== System Information Type 2 ===\n";
    std::cout << "Contains neighbour BCCH frequency list\n\n";

    auto si2 = L3SystemInformationType2::builder()
        .bcchFrequencyList(L3BCCHFrequencyList())
        .nccPermitted(L3NCCPermitted())
        .rachControlParameters(L3RACHControlParameters())
        .build();

    if (!roundTrip("SI2", ParsedMessage{RRM{std::move(si2)}})) {
        std::exit(1);
    }
    std::cout << "\n";
}

// Build SI3: Cell identity, LAI, control channel description, cell selection.
void demoSI3() {
    std::cout << "=== System Information Type 3 ===\n";
    std::cout << "Most critical SI: cell identity, LAI, control channels\n\n";

    auto si3 = L3SystemInformationType3::builder()
        .cellIdentity(L3CellIdentity(0x1234))
        .locationAreaIdentity(L3LocationAreaIdentity("250", "01", 0x5678))
        .controlChannelDescription(L3ControlChannelDescription(0, 1, 2, 1, 0, 0, 4, 10))
        .cellOptions(L3CellOptionsBCCH{})
        .cellSelectionParameters(L3CellSelectionParameters{})
        .rachControlParameters(L3RACHControlParameters{})
        .build();

    if (!roundTrip("SI3", ParsedMessage{RRM{std::move(si3)}})) {
        std::exit(1);
    }

    // Verify cell identity survived the round-trip.
    auto si3b = L3SystemInformationType3::builder()
        .cellIdentity(L3CellIdentity(0x5678))
        .locationAreaIdentity(L3LocationAreaIdentity("250", "01", 0x1234))
        .controlChannelDescription(L3ControlChannelDescription(0, 1, 2, 1, 0, 0, 4, 10))
        .cellOptions(L3CellOptionsBCCH{})
        .cellSelectionParameters(L3CellSelectionParameters{})
        .rachControlParameters(L3RACHControlParameters{})
        .build();

    ParsedMessage pm{RRM{std::move(si3b)}};
    auto l3Bytes = writeL3Bytes(pm);
    auto lapdmFrame = wrapL3(*l3Bytes, SAPI::SAPI0);
    auto unwrapped = unwrapL3(lapdmFrame);
    auto reparsed = parseL3(*unwrapped);

    if (reparsed) {
        auto* parsed = tryGet<L3SystemInformationType3>(*reparsed);
        if (parsed) {
            std::cout << "  SI3 verification: CI=0x"
                      << std::hex << std::setw(4) << std::setfill('0')
                      << parsed->ci().id() << "\n";
        }
    }
    std::cout << "\n";
}

// Build SI4: LAI, cell selection parameters, RACH parameters.
void demoSI4() {
    std::cout << "=== System Information Type 4 ===\n";
    std::cout << "Contains LAI and cell selection parameters\n\n";

    auto si4 = L3SystemInformationType4::builder()
        .locationAreaIdentity(L3LocationAreaIdentity("250", "01", 0xABCD))
        .cellSelectionParameters(L3CellSelectionParameters{})
        .rachControlParameters(L3RACHControlParameters{})
        .build();

    if (!roundTrip("SI4", ParsedMessage{RRM{std::move(si4)}})) {
        std::exit(1);
    }
    std::cout << "\n";
}

// Broadcast all SI messages in sequence (typical BCCH multiplex cycle).
void demoBroadcastCycle() {
    std::cout << "=== BCCH Multiplex Cycle ===\n";
    std::cout << "Simulates broadcasting SI1, SI2, SI3, SI4 in sequence\n\n";

    std::vector<std::pair<std::string, ParsedMessage>> messages;

    // SI1
    auto si1 = L3SystemInformationType1::builder()
        .cellChannelDescription(L3FrequencyList())
        .rachControlParameters(L3RACHControlParameters())
        .build();
    messages.emplace_back("SI1", ParsedMessage{RRM{std::move(si1)}});

    // SI2
    auto si2 = L3SystemInformationType2::builder()
        .bcchFrequencyList(L3BCCHFrequencyList())
        .nccPermitted(L3NCCPermitted())
        .rachControlParameters(L3RACHControlParameters())
        .build();
    messages.emplace_back("SI2", ParsedMessage{RRM{std::move(si2)}});

    // SI3
    auto si3 = L3SystemInformationType3::builder()
        .cellIdentity(L3CellIdentity(0x1234))
        .locationAreaIdentity(L3LocationAreaIdentity("250", "01", 0x5678))
        .controlChannelDescription(L3ControlChannelDescription(0, 1, 2, 1, 0, 0, 4, 10))
        .cellOptions(L3CellOptionsBCCH{})
        .cellSelectionParameters(L3CellSelectionParameters{})
        .rachControlParameters(L3RACHControlParameters{})
        .build();
    messages.emplace_back("SI3", ParsedMessage{RRM{std::move(si3)}});

    // SI4
    auto si4 = L3SystemInformationType4::builder()
        .locationAreaIdentity(L3LocationAreaIdentity("250", "01", 0x5678))
        .cellSelectionParameters(L3CellSelectionParameters{})
        .rachControlParameters(L3RACHControlParameters{})
        .build();
    messages.emplace_back("SI4", ParsedMessage{RRM{std::move(si4)}});

    // Serialize and wrap each SI in LAPDm frame.
    std::vector<std::vector<uint8_t>> frames;
    for (auto& [name, msg] : messages) {
        auto bytes = writeL3Bytes(msg);
        if (!bytes) {
            std::cerr << "  ERROR: failed to serialize " << name << "\n";
            std::exit(1);
        }
        auto frame = wrapL3(*bytes, SAPI::SAPI0);
        frames.push_back(std::move(frame));

        std::cout << "  " << name << ": LAPDm frame[" << frames.back().size()
                  << "] = " << bytesToHex(frames.back()) << "\n";
    }

    // Verify each frame can be unwrapped and parsed.
    std::cout << "\n  Verifying all frames:\n";
    for (size_t i = 0; i < frames.size(); ++i) {
        auto payload = unwrapL3(frames[i]);
        auto parsed = parseL3(*payload);
        if (!parsed) {
            std::cerr << "  ERROR: frame " << i << " failed to parse\n";
            std::exit(1);
        }
        std::cout << "    Frame[" << i << "]: " << messageName(*parsed)
                  << " OK\n";
    }
    std::cout << "\n";
}

} // anonymous namespace

int main() {
    demoSI1();
    demoSI2();
    demoSI3();
    demoSI4();
    demoBroadcastCycle();

    std::cout << "All system information demos completed successfully.\n";
    return 0;
}
