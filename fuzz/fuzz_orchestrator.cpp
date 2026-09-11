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

// Fuzz target: ProcedureOrchestrator fed with random L3 message chunks,
// random typed external data (AuthChallenge / VLRDecision), random tick
// deltas and response builds (audit D15). Exercises chain detection,
// phase transitions, the T3102/T3103 phase timers (audit D3) and the
// retransmission channel.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

#include "gsml3parser/parser.h"
#include "gsml3parser/stack/procedure_orchestrator.h"
#include "gsml3parser/stack/subscriber_registry.h"

using namespace gsml3parser;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size == 0) return 0;

    SubscriberSession session;
    session.context.setTMSI(0x12345678);
    ProcedureOrchestrator orch;
    uint8_t buf[512];

    size_t pos = 0;
    while (pos < size) {
        const uint8_t op = data[pos] % 5;
        ++pos;
        switch (op) {
            case 0: {
                // Feed a random L3 message (1..64 byte chunk).
                size_t len = size - pos;
                if (len == 0) break;
                len = 1 + (data[pos] % 64);
                if (len > size - pos) len = size - pos;
                auto res = parseL3(std::span<const uint8_t>(data + pos, len));
                if (res) orch.feed(*res, &session);
                pos += len;
                break;
            }
            case 1: {
                // Feed an AuthChallenge (20 input bytes).
                if (pos + 20 > size) break;
                AuthChallenge chal{};
                std::memcpy(chal.rand.data(), data + pos, 16);
                std::memcpy(chal.expectedSres.data(), data + pos + 16, 4);
                pos += 20;
                orch.feedExternalTyped(chal);
                break;
            }
            case 2: {
                // Feed a VLRDecision (2 input bytes).
                if (pos + 2 > size) break;
                VLRDecision vlr{(data[pos] & 1) != 0, std::nullopt, MMRejectCause::Zero};
                if (data[pos + 1] & 1) vlr.newTmsi = 0x80000000u;
                pos += 2;
                orch.feedExternalTyped(vlr);
                break;
            }
            case 3: {
                // Random tick (0..6000 ms) + drain the retransmission channel.
                const size_t delta = pos < size ? data[pos] % 6000 : 100;
                ++pos;
                orch.tickAll(std::chrono::milliseconds(delta));
                orch.takeRetransmissionToken();
                break;
            }
            default: {
                // Build the pending response into a local buffer.
                orch.buildPendingResponse({buf, sizeof(buf)}, &session);
                break;
            }
        }
    }
    return 0;
}
