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

// Demonstrates SubscriberRegistry workflow for a typical BTS scenario:
// 1. MS sends Channel Request -> BTS creates session
// 2. SDCCH assigned -> channel assigned to session
// 3. Timers tick -> expired timers collected
// 4. Procedure completed -> session removed

#include <gsml3parser/stack/subscriber_registry.h>
#include <gsml3parser/stack/channel_pool.h>

#include <array>
#include <chrono>
#include <iostream>

using namespace gsml3parser;
using namespace std::chrono_literals;

int main() {
    SubscriberRegistry registry;
    ChannelPool channelPool;

    // Register available channels.
    channelPool.addChannel({ChannelType::SDCCHType, 0, 0, 100});
    channelPool.addChannel({ChannelType::SDCCHType, 0, 1, 101});
    channelPool.addChannel({ChannelType::TCHFType, 1, 2, 102});

    // 1. MS sends Channel Request -> BTS creates session by TMSI.
    std::cout << "=== Step 1: Create session by TMSI ===\n";
    auto* session = registry.createByTMSI(0x12345678);
    if (session) {
        std::cout << "Session created for TMSI=0x12345678\n";
        std::cout << "Active sessions: " << registry.count() << "\n";
    }

    // 2. SDCCH assigned -> channel assigned to session.
    std::cout << "\n=== Step 2: Assign SDCCH channel ===\n";
    auto ch = channelPool.allocate(ChannelType::SDCCHType);
    if (ch) {
        registry.assignChannel(session, *ch, /*lapdmLink=*/5);
        std::cout << "Assigned SDCCH: trx=" << static_cast<int>(ch->trxNumber)
                  << " ts=" << static_cast<int>(ch->timeslot)
                  << " arfcn=" << ch->arfcn << "\n";

        // Verify link-based routing works.
        auto* found = registry.findByLink(ch->trxNumber, ch->timeslot, 5);
        std::cout << "findByLink: " << (found == session ? "OK" : "FAIL") << "\n";
    }

    // Start a protocol timer on the session.
    std::cout << "\n=== Step 3: Timer tick ===\n";
    session->timers.start(L3TimerId::T3101, 2000ms);
    std::cout << "Started T3101 (2s expiry)\n";

    // Tick 1 second — timer should NOT expire.
    {
        std::array<TimerExpiry, 32> expired{};
        size_t n = registry.tickAllTimers(1000ms, expired);
        std::cout << "After 1s tick: " << n << " timers expired\n";
    }

    // Tick another 1.5 seconds — timer SHOULD expire.
    {
        std::array<TimerExpiry, 32> expired{};
        size_t n = registry.tickAllTimers(1500ms, expired);
        std::cout << "After 1.5s tick: " << n << " timers expired\n";
        for (size_t i = 0; i < n; ++i) {
            // TimerExpiry carries the owning session; print the timer ID.
            std::cout << "  Expired: " << static_cast<int>(expired[i].id) << "\n";
        }
    }

    // 4. Procedure completed -> session removed.
    std::cout << "\n=== Step 4: Remove session ===\n";
    registry.remove(session);
    channelPool.release(*ch);
    std::cout << "After removal: " << registry.count() << " active sessions\n";
    std::cout << "findByTMSI(0x12345678): "
              << (registry.findByTMSI(0x12345678) ? "found" : "null") << "\n";

    // Demonstrate IMSI-based session creation.
    std::cout << "\n=== Bonus: IMSI-based session ===\n";
    auto* sess2 = registry.createByIMSI("244051234567890");
    if (sess2) {
        std::cout << "Session created for IMSI=244051234567890\n";
        std::cout << "Active sessions: " << registry.count() << "\n";

        auto* found2 = registry.findByIMSI("244051234567890");
        std::cout << "findByIMSI: " << (found2 == sess2 ? "OK" : "FAIL") << "\n";
    }

    std::cout << "\nAll steps completed successfully.\n";
    return 0;
}
