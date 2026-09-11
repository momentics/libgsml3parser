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

// Fuzz target: LAPDmEntity FSM driven by syntactically valid frames with
// randomized parameters (SAPI, C/R, F, NS/NR, M bit, payload) in random
// order, interleaved with T200 ticks (audit D15). Complements
// fuzz_lapdm_decode, which feeds raw bytes to the frame decoder: this
// target exercises the state machine (SABME/UA/DISC/DM, I-frame
// segmentation/reassembly, REJ retransmission, abnormal release).
#include <cstddef>
#include <cstdint>
#include <span>

#include "gsml3parser/lapdm_entity.h"
#include "gsml3parser/lapdm_frame.h"

using namespace gsml3parser;

namespace {
void l3Cb(SAPI, Primitive, std::span<const uint8_t>, void*) {}
void l1Cb(std::span<const uint8_t>, void*) {}
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 4) return 0;

    const SAPI sapi = (data[0] & 1) ? SAPI::SAPI3 : SAPI::SAPI0;
    const bool command = (data[1] & 1) != 0;

    LAPDmEntity entity(LAPDmChannelProfile::SDCCH(), &l3Cb, &l1Cb, nullptr);
    entity.open(sapi, command);

    size_t pos = 2;
    while (pos + 3 <= size) {
        const uint8_t kind = data[pos] % 8;
        const uint8_t a = data[pos + 1];
        const uint8_t b = data[pos + 2];
        pos += 3;
        const size_t infoLen = (size - pos > 24) ? 24 : (size - pos);
        std::span<const uint8_t> info(data + pos, infoLen);
        pos += infoLen;

        lapdm::LAPDmFrame frame;
        switch (kind) {
            case 0:  frame = lapdm::makeUIFrame(sapi, command, info); break;
            case 1:  frame = lapdm::makeSABMEFrame(sapi, command, info); break;
            case 2:  frame = lapdm::makeUAFrame(sapi, (a & 1) != 0, info); break;
            case 3:  frame = lapdm::makeDMFrame(sapi, (a & 1) != 0); break;
            case 4:  frame = lapdm::makeDISCFrame(sapi, command); break;
            case 5:  frame = lapdm::makeIFrame(sapi, command, a, b, (a & 1) != 0, (b & 1) != 0, info); break;
            case 6:  frame = lapdm::makeRRFrame(sapi, a, (a & 1) != 0); break;
            default: frame = lapdm::makeREJFrame(sapi, b, (b & 1) != 0); break;
        }
        auto encoded = lapdm::encodeFrame(frame);
        entity.receiveFrame(std::span<const uint8_t>(encoded.data(), encoded.size()));
        // Exercise the T200 retransmission / abnormal-release path.
        entity.tickT200(std::chrono::milliseconds(100 + (a % 4000)));
    }
    return 0;
}
