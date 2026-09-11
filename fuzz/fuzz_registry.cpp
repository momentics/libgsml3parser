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

// Fuzz target: SubscriberRegistry random operation mix — create/find/
// remove by TMSI, timer start/tick, channel assign/release (audit D15).
// The live-session vector tracks created sessions so remove() only ever
// sees pointers owned by the registry (no use-after-free by construction).
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "gsml3parser/stack/subscriber_registry.h"

using namespace gsml3parser;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    SubscriberRegistry reg;
    std::vector<SubscriberSession*> live;
    std::array<TimerExpiry, 64> expired{};

    size_t pos = 0;
    while (pos < size) {
        const uint8_t op = data[pos] % 6;
        uint32_t arg = 0;
        if (pos + 4 < size) {
            arg = static_cast<uint32_t>(data[pos + 1])
                | (static_cast<uint32_t>(data[pos + 2]) << 8)
                | (static_cast<uint32_t>(data[pos + 3]) << 16)
                | (static_cast<uint32_t>(data[pos + 4]) << 24);
            pos += 5;
        } else {
            ++pos;
            continue;
        }
        switch (op) {
            case 0: {
                SubscriberSession* s = reg.createByTMSI(arg);
                if (s) live.push_back(s);
                break;
            }
            case 1:
                (void)reg.findByTMSI(arg);
                break;
            case 2: {
                if (live.empty()) break;
                size_t i = arg % live.size();
                reg.remove(live[i]);
                live[i] = live.back();
                live.pop_back();
                break;
            }
            case 3: {
                if (live.empty()) break;
                live[arg % live.size()]->timers.start(L3TimerId::T3101);
                break;
            }
            case 4:
                reg.tickAllTimers(std::chrono::milliseconds(arg % 10000),
                                  std::span<TimerExpiry>(expired));
                break;
            case 5: {
                if (live.empty()) break;
                SubscriberSession* s = live[arg % live.size()];
                ChannelDescriptor desc{};
                desc.type = ChannelType::SDCCHType;
                desc.trxNumber = static_cast<uint8_t>(arg & 0x07);
                desc.timeslot = static_cast<uint8_t>((arg >> 3) & 0x07);
                desc.arfcn = static_cast<uint16_t>(arg & 0x03FF);
                reg.assignChannel(s, desc, static_cast<uint8_t>(arg & 0x07));
                reg.releaseChannel(s);
                break;
            }
        }
    }
    return 0;
}
