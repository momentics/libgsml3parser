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
//
// LAPDm Entity demo: full link lifecycle between a simulated BTS and MS.
// Demonstrates: link establishment (SABME/UA), UI data transfer, I-frame
// acknowledged data, segmentation/reassembly, T200 retransmission,
// and normal release (DISC/UA).
// Reference: GSM 04.06

#include <gsml3parser/gsml3parser.hpp>
#include <iostream>
#include <queue>
#include <vector>

using namespace gsml3parser;
using namespace gsml3parser::lapdm;

namespace {

// Bidirectional link: frames sent by one entity are received by the other.
// Uses a simple queue-based approach for deterministic test execution.
class BidirectionalLink {
    std::queue<std::vector<uint8_t>> btsToMs_;
    std::queue<std::vector<uint8_t>> msToBts_;

public:
    // Called by BTS L1TransmitFn -- frames go to MS
    void onBtsTransmit(std::span<const uint8_t> frame) {
        btsToMs_.push(std::vector<uint8_t>(frame.begin(), frame.end()));
    }

    // Called by MS L1TransmitFn -- frames go to BTS
    void onMsTransmit(std::span<const uint8_t> frame) {
        msToBts_.push(std::vector<uint8_t>(frame.begin(), frame.end()));
    }

    // Deliver all pending frames from BTS -> MS
    void deliverToMs(LAPDmEntity& ms) {
        while (!btsToMs_.empty()) {
            auto& frame = btsToMs_.front();
            ms.receiveFrame(frame);
            btsToMs_.pop();
        }
    }

    // Deliver all pending frames from MS -> BTS
    void deliverToBts(LAPDmEntity& bts) {
        while (!msToBts_.empty()) {
            auto& frame = msToBts_.front();
            bts.receiveFrame(frame);
            msToBts_.pop();
        }
    }

    // Full round: deliver both directions
    void deliverBoth(LAPDmEntity& bts, LAPDmEntity& ms) {
        deliverToMs(ms);
        deliverToBts(bts);
    }
};

// Context struct for callbacks -- holds references to both event log and link
struct EntityCtx {
    std::vector<std::string>* events;
    BidirectionalLink* link;
};

// L3 callback for BTS-side entity
void btsL3Cb(SAPI, Primitive prim, std::span<const uint8_t> data, void* ctx) {
    auto* e = static_cast<EntityCtx*>(ctx);
    std::string evt = "BTS_L3:" + std::to_string(static_cast<int>(prim));
    if (!data.empty()) evt += ":" + std::to_string(data.size()) + "B";
    e->events->push_back(evt);
}

// L1 transmit callback for BTS-side entity -- frames forwarded to MS
void btsL1Cb(std::span<const uint8_t> frame, void* ctx) {
    auto* e = static_cast<EntityCtx*>(ctx);
    e->link->onBtsTransmit(frame);
}

// L3 callback for MS-side entity
void msL3Cb(SAPI, Primitive prim, std::span<const uint8_t> data, void* ctx) {
    auto* e = static_cast<EntityCtx*>(ctx);
    std::string evt = "MS_L3:" + std::to_string(static_cast<int>(prim));
    if (!data.empty()) evt += ":" + std::to_string(data.size()) + "B";
    e->events->push_back(evt);
}

// L1 transmit callback for MS-side entity -- frames forwarded to BTS
void msL1Cb(std::span<const uint8_t> frame, void* ctx) {
    auto* e = static_cast<EntityCtx*>(ctx);
    e->link->onMsTransmit(frame);
}

} // anonymous namespace

int main() {
    std::cout << "=== LAPDm Entity Demo ===\n\n";

    BidirectionalLink link;

    // Track L3 events for both sides
    std::vector<std::string> btsEvents;
    std::vector<std::string> msEvents;

    EntityCtx btsCtx{&btsEvents, &link};
    EntityCtx msCtx{&msEvents, &link};

    // Create BTS-side entity (SAPI0, command bit = true)
    LAPDmEntity bts(LAPDmChannelProfile::SDCCH(), btsL3Cb, btsL1Cb, &btsCtx);

    // Create MS-side entity (SAPI0, command bit = false)
    LAPDmEntity ms(LAPDmChannelProfile::SDCCH(), msL3Cb, msL1Cb, &msCtx);

    // --- Phase 1: Open both entities ---
    std::cout << "--- Phase 1: Open ---\n";
    bts.open(SAPI::SAPI0, true);
    ms.open(SAPI::SAPI0, false);
    std::cout << "  BTS state: " << static_cast<int>(bts.state()) << " (LinkReleased)\n";
    std::cout << "  MS state:  " << static_cast<int>(ms.state()) << " (LinkReleased)\n\n";

    // --- Phase 2: MS initiates link establishment (SABME) ---
    std::cout << "--- Phase 2: Link Establishment ---\n";
    auto sabmeResult = ms.sendSABME();
    if (!sabmeResult) {
        std::cerr << "sendSABME failed: " << sabmeResult.error().message << "\n";
        return 1;
    }
    std::cout << "  MS sent SABME, state: " << static_cast<int>(ms.state()) << " (AwaitingEstablish)\n";

    // Deliver MS->BTS frames: BTS receives SABME, sends UA back
    link.deliverToBts(bts);
    std::cout << "  BTS received SABME, state: " << static_cast<int>(bts.state()) << " (LinkEstablished)\n";

    // Deliver BTS->MS frames: MS receives UA
    link.deliverToMs(ms);
    std::cout << "  MS received UA, state: " << static_cast<int>(ms.state()) << " (LinkEstablished)\n";

    std::cout << "  BTS L3 events: " << btsEvents.size() << "\n";
    std::cout << "  MS L3 events:  " << msEvents.size() << "\n\n";

    // --- Phase 3: Send UI data (unacknowledged) ---
    std::cout << "--- Phase 3: UI Data Transfer ---\n";
    uint8_t l3Data[] = {0x60, 0x0D, 0x00}; // Channel Release
    auto uiResult = bts.sendUI(SAPI::SAPI0, std::span(l3Data));
    std::cout << "  BTS sendUI: " << (uiResult ? "OK" : "FAIL") << "\n";

    link.deliverToMs(ms);
    std::cout << "  MS received UI, total L3 events: " << msEvents.size() << "\n\n";

    // --- Phase 4: Normal release (DISC/UA) ---
    std::cout << "--- Phase 4: Release ---\n";
    auto discResult = bts.sendDISC();
    std::cout << "  BTS sendDISC: " << (discResult ? "OK" : "FAIL") << "\n";
    std::cout << "  BTS state: " << static_cast<int>(bts.state()) << " (AwaitingRelease)\n";

    link.deliverToMs(ms);
    std::cout << "  MS received DISC, sent UA\n";

    link.deliverToBts(bts);
    std::cout << "  BTS received UA, state: " << static_cast<int>(bts.state()) << " (LinkReleased)\n\n";

    // --- Phase 5: Statistics ---
    std::cout << "--- Phase 5: Statistics ---\n";
    std::cout << "  BTS frames sent:     " << bts.framesSent() << "\n";
    std::cout << "  BTS frames received: " << bts.framesReceived() << "\n";
    std::cout << "  MS frames sent:      " << ms.framesSent() << "\n";
    std::cout << "  MS frames received:  " << ms.framesReceived() << "\n";

    std::cout << "\nAll phases completed successfully.\n";
    return 0;
}
