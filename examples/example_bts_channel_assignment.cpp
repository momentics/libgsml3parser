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

// BTS Channel Assignment demo: simulate receiving a Channel Request on RACH,
// build an ImmediateAssignment response via Builder, serialize to L3 bytes,
// wrap in LAPDm frame, then verify the full round-trip.
// Reference: GSM 04.08 9.1.13 (Channel Request) / 9.1.19 (Immediate Assignment)

#include <gsml3parser/gsml3parser.hpp>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <span>
#include <string>

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

// Simulate receiving a Channel Request from MS on RACH.
void demoChannelRequest() {
    std::cout << "=== BTS: Receive Channel Request ===\n";
    std::cout << "Simulates an MS sending a Channel Request on RACH\n\n";

    // The Channel Request is a short message (no standard L3 header).
    // In practice, it arrives as 1 byte in the LAPDm frame.
    uint8_t chanReqRaw = 0x03; // requestReference = 3

    auto chanReq = L3ChannelRequest(chanReqRaw);
    std::cout << "  Channel Request received:\n";
    std::cout << "    requestReference=" << chanReq.requestReference() << "\n";
    std::cout << "\n";
}

// Build and send ImmediateAssignment as a response to Channel Request.
void demoImmediateAssignment() {
    std::cout << "=== BTS: Send Immediate Assignment ===\n";
    std::cout << "Builds ImmediateAssignment -> L3 bytes -> LAPDm frame\n\n";

    // 1. Build the assignment with all relevant fields.
    auto ia = L3ImmediateAssignment::builder()
        .pageMode(L3PageMode(0))
        .dedicatedModeOrTBF(L3DedicatedModeOrTBF(false, false))
        .requestReference(L3RequestReference(1, 2, 3, 4))
        .channelDescription(L3ChannelDescription(TDMA_SDCCH, 0, 1, 100))
        .timingAdvance(L3TimingAdvance(32))
        .build();

    std::cout << "  Built ImmediateAssignment:\n";
    std::cout << "    typeAndOffset=" << static_cast<int>(ia.channelDescription().typeAndOffset()) << "\n";
    std::cout << "    timingAdvance=" << ia.timingAdvance().timingAdvance() << "\n";

    // 2. Serialize to L3 bytes.
    ParsedMessage pm{RRM{std::move(ia)}};
    auto l3Bytes = writeL3Bytes(pm);
    if (!l3Bytes) {
        std::cerr << "  ERROR: writeL3Bytes failed\n";
        std::exit(1);
    }

    std::cout << "  L3 bytes (" << (*l3Bytes).size() << "): "
              << bytesToHex(*l3Bytes) << "\n";
    std::cout << "    [0]=" << std::hex << static_cast<int>((*l3Bytes)[0])
              << " (PD=RR)\n";
    std::cout << "    [1]=" << std::hex << static_cast<int>((*l3Bytes)[1])
              << " (MTI=ImmediateAssignment)\n";

    // 3. Wrap in LAPDm UI frame.
    auto uiFrame = makeUIFrame(SAPI::SAPI0, false, *l3Bytes);
    auto lapdmFrame = encodeFrame(uiFrame);
    std::cout << "  LAPDm frame (" << lapdmFrame.size() << "): "
              << bytesToHex(lapdmFrame) << "\n\n";

    // 4. Round-trip verification.
    auto decoded = LAPDmFrame::decode(lapdmFrame);
    if (!decoded) {
        std::cerr << "  ERROR: LAPDmFrame::decode failed\n";
        std::exit(1);
    }

    auto reparsed = parseL3((*decoded).info);
    if (!reparsed) {
        std::cerr << "  ERROR: parseL3 failed\n";
        std::exit(1);
    }

    auto* ia2 = tryGet<L3ImmediateAssignment>(*reparsed);
    if (ia2) {
        std::cout << "  Round-trip verified:\n";
        std::cout << "    PD=" << messagePD(*reparsed)
                  << " MTI=0x" << std::hex << messageMTI(*reparsed) << "\n";
        std::cout << "    timingAdvance="
                  << ia2->timingAdvance().timingAdvance() << "\n";
    } else {
        std::cerr << "  ERROR: could not extract L3ImmediateAssignment\n";
        std::exit(1);
    }
    std::cout << "\n";
}

// Build ImmediateAssignment with start time (timed assignment).
void demoTimedAssignment() {
    std::cout << "=== BTS: Timed Immediate Assignment ===\n";
    std::cout << "Includes startTime for frame-synchronized assignment\n\n";

    auto ia = L3ImmediateAssignment::builder()
        .channelDescription(L3ChannelDescription(TDMA_TCHF, 1, 0, 50))
        .timingAdvance(L3TimingAdvance(64))
        .startTime(12345, true)
        .build();

    ParsedMessage pm{RRM{std::move(ia)}};
    auto l3Bytes = writeL3Bytes(pm);
    if (!l3Bytes) {
        std::cerr << "  ERROR: writeL3Bytes failed\n";
        std::exit(1);
    }

    auto uiFrame = makeUIFrame(SAPI::SAPI0, false, *l3Bytes);
    auto lapdmFrame = encodeFrame(uiFrame);

    // Round-trip.
    auto decoded = LAPDmFrame::decode(lapdmFrame);
    auto reparsed = decoded ? parseL3((*decoded).info) : Expected<ParsedMessage>::error(
        ParseError(ParseError::Code::TruncatedInput, "decode failed"));

    if (!reparsed) {
        std::cerr << "  ERROR: round-trip failed\n";
        std::exit(1);
    }

    auto* ia2 = tryGet<L3ImmediateAssignment>(*reparsed);
    if (ia2) {
        std::cout << "  Timed assignment verified:\n";
        std::cout << "    hasStartTime=" << ia2->hasStartTime() << "\n";
        std::cout << "    startTimeFrame=" << ia2->startTimeFrame() << "\n";
    }
    std::cout << "\n";
}

// Build ImmediateAssignmentReject (when no channel is available).
void demoAssignmentReject() {
    std::cout << "=== BTS: Immediate Assignment Reject ===\n";
    std::cout << "Sent when all channels are busy\n\n";

    auto reject = L3ImmediateAssignmentReject::builder()
        .waitTime(30)
        .build();

    ParsedMessage pm{RRM{std::move(reject)}};
    auto l3Bytes = writeL3Bytes(pm);
    if (!l3Bytes) {
        std::cerr << "  ERROR: writeL3Bytes failed\n";
        std::exit(1);
    }

    std::cout << "  Reject L3 bytes (" << (*l3Bytes).size() << "): "
              << bytesToHex(*l3Bytes) << "\n";

    auto uiFrame = makeUIFrame(SAPI::SAPI0, false, *l3Bytes);
    auto lapdmFrame = encodeFrame(uiFrame);
    auto decoded = LAPDmFrame::decode(lapdmFrame);
    auto reparsed = decoded ? parseL3((*decoded).info) : Expected<ParsedMessage>::error(
        ParseError(ParseError::Code::TruncatedInput, "decode failed"));

    if (!reparsed) {
        std::cerr << "  ERROR: round-trip failed\n";
        std::exit(1);
    }

    auto* rej = tryGet<L3ImmediateAssignmentReject>(*reparsed);
    if (rej) {
        std::cout << "  Reject round-trip verified:\n";
        std::cout << "    PD=" << messagePD(*reparsed)
                  << " MTI=0x" << std::hex << messageMTI(*reparsed) << "\n";
    }
    std::cout << "\n";
}

} // anonymous namespace

int main() {
    demoChannelRequest();
    demoImmediateAssignment();
    demoTimedAssignment();
    demoAssignmentReject();

    std::cout << "All channel assignment demos completed successfully.\n";
    return 0;
}
