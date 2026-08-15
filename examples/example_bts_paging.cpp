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

// BTS Paging cycle demo: build a PagingRequestType2 via Builder, serialize to
// raw L3 bytes, wrap in LAPDm UI frame for air transmission, then simulate
// the reverse path (unwrap LAPDm, parse L3, verify fields).
// Reference: GSM 04.08 9.1.25 / 04.06 4.2.1

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

// Build a PagingRequestType2 message and serialize the full pipeline.
void demoPagingOutbound() {
    std::cout << "=== BTS Paging Outbound ===\n";
    std::cout << "Builds PagingRequestType2 -> L3 bytes -> LAPDm frame\n\n";

    // 1. Build PagingRequestType2 via Builder.
    auto paging = L3PagingRequestType2::builder()
        .addTMSI(0x12345678, ChannelType::SDCCHType)
        .build();

    std::cout << "  Built PagingRequestType2 with TMSI=0x12345678\n";

    // 2. Wrap in ParsedMessage variant and serialize to L3 bytes.
    ParsedMessage pm{RRM{std::move(paging)}};
    auto l3Bytes = writeL3Bytes(pm);
    if (!l3Bytes) {
        std::cerr << "  ERROR: writeL3Bytes failed\n";
        std::exit(1);
    }

    std::cout << "  L3 bytes (" << (*l3Bytes).size() << "): "
              << bytesToHex(*l3Bytes) << "\n";

    // 3. Wrap in LAPDm UI frame for air transmission.
    auto uiFrame = makeUIFrame(SAPI::SAPI0, false, *l3Bytes);
    auto lapdmFrame = encodeFrame(uiFrame);
    std::cout << "  LAPDm frame (" << lapdmFrame.size() << "): "
              << bytesToHex(lapdmFrame) << "\n";
    std::cout << "    [0]=" << std::hex << static_cast<int>(lapdmFrame[0])
              << " (addr: SAPI0, CR=0, EA=1)\n";
    std::cout << "    [1]=" << std::hex << static_cast<int>(lapdmFrame[1])
              << " (control: UI)\n";
    std::cout << "\n";

    // 4. Simulate reverse path: receive LAPDm frame, decode, parse.
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

    // 5. Verify the round-trip preserved TMSI.
    auto* paged = tryGet<L3PagingRequestType2>(*reparsed);
    if (!paged) {
        std::cerr << "  ERROR: could not extract L3PagingRequestType2\n";
        std::exit(1);
    }

    std::cout << "  Round-trip verified:\n";
    std::cout << "    PD=" << messagePD(*reparsed)
              << " MTI=0x" << std::hex << messageMTI(*reparsed) << "\n";
    if (!paged->tmsis().empty()) {
        std::cout << "    TMSI[0]=0x" << std::setw(8) << std::setfill('0')
                  << paged->tmsis()[0] << "\n";
    }
    std::cout << "\n";
}

// Demonstrate building a PagingRequestType2 with multiple TMSIs.
void demoMultiPaging() {
    std::cout << "=== BTS Multi-TMSI Paging ===\n";
    std::cout << "Builds PagingRequestType2 with 2 TMSIs\n\n";

    auto paging = L3PagingRequestType2::builder()
        .addTMSI(0x11111111, ChannelType::SDCCHType)
        .addTMSI(0x22222222, ChannelType::TCHFType)
        .build();

    ParsedMessage pm{RRM{std::move(paging)}};
    auto l3Bytes = writeL3Bytes(pm);
    if (!l3Bytes) {
        std::cerr << "  ERROR: writeL3Bytes failed\n";
        std::exit(1);
    }

    auto uiFrame = makeUIFrame(SAPI::SAPI0, false, *l3Bytes);
    auto lapdmFrame = encodeFrame(uiFrame);
    auto decoded = LAPDmFrame::decode(lapdmFrame);
    auto reparsed = decoded ? parseL3((*decoded).info) : Expected<ParsedMessage>::error(
        ParseError(ParseError::Code::TruncatedInput, "decode failed"));

    if (!reparsed) {
        std::cerr << "  ERROR: round-trip failed\n";
        std::exit(1);
    }

    auto* paged = tryGet<L3PagingRequestType2>(*reparsed);
    if (paged) {
        const auto& tmsis = paged->tmsis();
        for (size_t i = 0; i < tmsis.size(); ++i) {
            std::cout << "  TMSI[" << i << "]=0x"
                      << std::hex << std::setw(8) << std::setfill('0')
                      << tmsis[i] << "\n";
        }
    }
    std::cout << "\n";
}

} // anonymous namespace

int main() {
    demoPagingOutbound();
    demoMultiPaging();

    std::cout << "All paging demos completed successfully.\n";
    return 0;
}
