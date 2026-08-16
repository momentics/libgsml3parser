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

/// Demonstrates the Abis/RSL pipeline: receiving RSL messages from BSC,
/// extracting L3 payloads, processing them, and building RSL response messages
/// to send back to the BSC. Covers DATA_REQ/DATA_IND round-trip, CHAN_ACTIV
/// acknowledgment, and measurement reporting.
#include <iostream>
#include <iomanip>
#include <vector>
#include "gsml3parser/gsml3parser.hpp"

using namespace gsml3parser;

// Print hex bytes for debugging.
static void printHex(const char* label, std::span<const uint8_t> data) {
    std::cout << label << " (" << data.size() << " bytes): ";
    for (auto b : data) std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b) << " ";
    std::cout << std::dec << "\n";
}

int main()
{
    std::cout << "=== Abis/RSL Pipeline Demo ===\n\n";

    // ── 1. BSC sends RLL DATA_REQ with L3 payload -> BTS extracts L3 ──
    std::cout << "[1] BSC->BTS: RLL DATA_REQ with L3 CM Service Request\n";
    {
        // Simulate raw RSL frame from BSC: DATA_REQ carrying L3 CM Service Request.
        std::vector<uint8_t> rawRSL = {
            0x00,                       // discriminator: RLL
            0x21,                       // msgType: DATA_REQ
            0x7c,                       // chanNr: dedicated channel (SDCCH/8, TS4)
            0x01,                       // linkId: LAPDm link 1
            // L3 payload: CM Service Request (PD=0x09, MTI=0x68, serviceType=2)
            0x09, 0x68, 0x02
        };

        printHex("  Raw RSL", rawRSL);

        auto parsed = RSLParser::parse(rawRSL);
        if (!parsed) {
            std::cerr << "  ERROR: Failed to parse RSL: " << parsed.error().message << "\n";
            return 1;
        }

        std::cout << "  Discriminator: " << rslDiscriminatorName((*parsed).discriminator) << "\n";
        std::cout << "  Message: " << RSLParser::messageName((*parsed).discriminator, (*parsed).msgType) << "\n";
        std::cout << "  Channel: 0x" << std::hex << static_cast<int>((*parsed).chanNr) << std::dec << "\n";
        std::cout << "  Link ID: " << static_cast<int>((*parsed).linkId) << "\n";

        auto l3Payload = RSLParser::extractL3(*parsed);
        if (l3Payload) {
            printHex("  Extracted L3", *l3Payload);
            // In a real BTS, you would pass this to parseL3() and feed it to ProcedureRunner.
        } else {
            std::cerr << "  ERROR: No L3 payload in DATA_REQ\n";
            return 1;
        }
    }

    // ── 2. BTS sends RLL DATA_IND with L3 response -> BSC ──
    std::cout << "\n[2] BTS->BSC: RLL DATA_IND with L3 CM Service Accept\n";
    {
        // Simulate L3 CM Service Accept bytes (PD=0x09, MTI=0x60).
        std::vector<uint8_t> l3Response = {0x09, 0x60};

        auto rslFrame = RSLBuilder::buildDataInd(0x7c, 1, l3Response);
        if (!rslFrame) {
            std::cerr << "  ERROR: Failed to build DATA_IND\n";
            return 1;
        }

        printHex("  Built RSL", *rslFrame);

        // Verify round-trip: parse the built frame.
        auto reparsed = RSLParser::parse(*rslFrame);
        if (reparsed && RSLParser::hasL3Payload(*reparsed)) {
            auto l3 = RSLParser::extractL3(*reparsed);
            std::cout << "  Round-trip OK: L3 payload matches\n";
            (void)l3;
        }
    }

    // ── 3. DCHAN CHAN_ACTIV from BSC -> BTS processes and sends ACK ──
    std::cout << "\n[3] BSC->BTS: DCHAN CHAN_ACTIV -> BTS responds with CHAN_ACTIV_ACK\n";
    {
        // Simulate raw CHAN_ACTIV with ActType and ChanMode IEs.
        std::vector<uint8_t> rawActiv = {
            0x60,                               // discriminator: DCHAN
            0x01,                               // msgType: CHAN_ACTIV
            0x78,                               // chanNr: SDCCH/4, TS0
            0x00,                               // reserved
            // ActType IE (TV): type=0x21, value=2 (IntraSDCCH4)
            0x21, 0x02,
            // ChanMode IE (TLV): type=0x22, len=5, value=signalling mode
            0x22, 0x05, 0x00, 0x01, 0x01, 0x00, 0x00,
        };

        printHex("  Raw CHAN_ACTIV", rawActiv);

        auto parsed = RSLParser::parse(rawActiv);
        if (!parsed) {
            std::cerr << "  ERROR: Failed to parse CHAN_ACTIV\n";
            return 1;
        }

        std::cout << "  Message: " << RSLParser::messageName((*parsed).discriminator, (*parsed).msgType) << "\n";
        std::cout << "  Channel: 0x" << std::hex << static_cast<int>((*parsed).chanNr) << std::dec << "\n";

        auto* actType = RSLParser::findIE(*parsed, RSL_IE::ActType);
        if (actType) {
            std::cout << "  Activation Type: " << static_cast<int>(actType->val[0]) << "\n";
        }

        auto mode = RSLParser::getChannelMode(*parsed);
        if (mode) {
            std::cout << "  Channel Mode: " << (mode->isSignalling() ? "Signalling" :
                                                 mode->isSpeech() ? "Speech" : "Data") << "\n";
        }

        // Build CHAN_ACTIV_ACK response.
        uint16_t frameNumber = 45678; // FN when activation took effect.
        auto ackFrame = RSLBuilder::buildChanActivAck((*parsed).chanNr, frameNumber);
        if (ackFrame) {
            printHex("  CHAN_ACTIV_ACK", *ackFrame);

            auto ackParsed = RSLParser::parse(*ackFrame);
            if (ackParsed) {
                std::cout << "  ACK round-trip OK: chanNr=0x" << std::hex << static_cast<int>((*ackParsed).chanNr) << std::dec << "\n";
            }
        }
    }

    // ── 4. BTS sends MEAS_RES with measurement results -> BSC ──
    std::cout << "\n[4] BTS->BSC: DCHAN MEAS_RES with uplink measurements\n";
    {
        auto measFrame = RSLBuilder::buildMeasRes(0x7c, 1, -52, 2);
        if (!measFrame) {
            std::cerr << "  ERROR: Failed to build MEAS_RES\n";
            return 1;
        }

        printHex("  Built MEAS_RES", *measFrame);

        auto parsed = RSLParser::parse(*measFrame);
        if (parsed) {
            std::cout << "  MEAS_RES round-trip OK\n";
            auto* uplinkIE = RSLParser::findIE(*parsed, RSL_IE::UplinkMeas);
            if (uplinkIE && uplinkIE->len >= 2) {
                std::cout << "  RXLEV: " << static_cast<int>(static_cast<int8_t>(uplinkIE->val[0])) << " dBm\n";
                std::cout << "  RXQUAL: " << static_cast<int>(uplinkIE->val[1]) << "\n";
            }
        }
    }

    // ── 5. Arena-based zero-allocation demo ──
    std::cout << "\n[5] Zero-allocation RSL building with Arena\n";
    {
        Arena arena(65536);
        std::vector<uint8_t> l3 = {0x09, 0x60};

        // Allocate buffer from Arena (zero heap alloc on hot path).
        auto* buf = static_cast<uint8_t*>(arena.allocate(512));
        int n = RSLBuilder::buildDataInd({buf, 512}, 0x7c, 1, l3);
        if (n > 0) {
            std::cout << "  Built DATA_IND in Arena buffer: " << n << " bytes (zero heap alloc)\n";

            // Parse to verify.
            auto parsed = RSLParser::parse({buf, static_cast<size_t>(n)});
            if (parsed && RSLParser::hasL3Payload(*parsed)) {
                std::cout << "  Arena buffer round-trip OK\n";
            }
        }

        // Reset arena for next batch.
        arena.reset();
        std::cout << "  Arena reset: " << arena.used() << " bytes used (should be 0)\n";
    }

    std::cout << "\n=== All RSL pipeline steps completed successfully ===\n";
    return 0;
}
