// Copyright 2026 momentics <momentics@gmail.com>
// Copyright libgsml3parser contributors
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject with the following conditions:
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

// BTS Protocol Dispatcher demo: register type-specific handlers for incoming
// L3 messages, simulate a stream of messages from the air interface, and
// verify that each message is routed to the correct handler.
// Demonstrates specific handlers, domain fallback, and global fallback.
// Reference: GSM 04.08 (message types across PD domains)

#include <gsml3parser/gsml3parser.hpp>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <span>
#include <string>

using namespace gsml3parser;

namespace {

// Convert hex string to bytes.
std::vector<uint8_t> hexToBytes(std::string_view hex) {
    std::vector<uint8_t> out;
    for (size_t i = 0; i + 1 < hex.size(); i += 2) {
        uint8_t b = 0;
        for (int j = 0; j < 2; ++j) {
            char c = hex[i + j];
            b <<= 4;
            if (c >= '0' && c <= '9') b |= static_cast<uint8_t>(c - '0');
            else if (c >= 'a' && c <= 'f') b |= static_cast<uint8_t>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') b |= static_cast<uint8_t>(c - 'A' + 10);
        }
        out.push_back(b);
    }
    return out;
}

// Handler statistics tracker.
struct DispatchStats {
    std::atomic<int> specific{0};
    std::atomic<int> domain{0};
    std::atomic<int> fallback{0};
};

// Demo 1: Register specific handlers for known RR message types.
void demoSpecificHandlers() {
    std::cout << "=== Specific Message Handlers ===\n";
    std::cout << "Registers handlers for ChannelRelease, PagingRequestType2\n\n";

    ProtocolDispatcher disp;
    DispatchStats stats;

    // Handler for Channel Release (RR MTI=0x0D).
    disp.registerHandler(L3PD::RadioResource, L3ChannelRelease::MTI,
        [&](const ParsedMessage& msg, void*) {
            ++stats.specific;
            auto* cr = tryGet<L3ChannelRelease>(msg);
            std::cout << "  [SPECIFIC] ChannelRelease received\n";
            if (cr) {
                std::cout << "    cause=" << static_cast<int>(cr->cause()) << "\n";
            }
        });

    // Handler for Paging Request Type 2 (RR MTI=0x22).
    disp.registerHandler(L3PD::RadioResource, L3PagingRequestType2::MTI,
        [&](const ParsedMessage& msg, void*) {
            ++stats.specific;
            auto* pag = tryGet<L3PagingRequestType2>(msg);
            std::cout << "  [SPECIFIC] PagingRequestType2 received\n";
            if (pag) {
                const auto& tmsis = pag->tmsis();
                if (!tmsis.empty()) {
                    std::cout << "    TMSI[0]=0x" << std::hex << std::setw(8)
                              << std::setfill('0') << tmsis[0] << "\n";
                }
            }
        });

    // Simulate incoming messages.
    std::cout << "  Dispatching ChannelRelease (hex: 600d00):\n";
    {
        auto msg = parseL3Hex("600d00");
        if (msg) disp.dispatch(*msg);
    }

    std::cout << "  Dispatching PagingRequestType2 (hex: 6022...):\n";
    {
        // Build a PagingRequestType2 and dispatch via raw bytes.
        auto paging = L3PagingRequestType2::builder()
            .addTMSI(0xDEADBEEF, ChannelType::SDCCHType)
            .build();
        ParsedMessage pm{RRM{std::move(paging)}};
        auto bytes = writeL3Bytes(pm);
        if (bytes) {
            disp.dispatchRaw(*bytes);
        }
    }

    std::cout << "\n  Specific handlers called: " << stats.specific.load() << "\n\n";
}

// Demo 2: Domain-level fallback handler.
void demoDomainFallback() {
    std::cout << "=== Domain Fallback Handler ===\n";
    std::cout << "RR domain handler catches unregistered RR message types\n\n";

    ProtocolDispatcher disp;
    DispatchStats stats;

    // Register only ChannelRelease as specific handler.
    disp.registerHandler(L3PD::RadioResource, L3ChannelRelease::MTI,
        [&](const ParsedMessage& msg, void*) {
            ++stats.specific;
            std::cout << "  [SPECIFIC] ChannelRelease\n";
        });

    // Register domain handler for all RR messages.
    disp.registerDomainHandler(L3PD::RadioResource,
        [&](const ParsedMessage& msg, void*) {
            ++stats.domain;
            std::cout << "  [DOMAIN] Unregistered RR message: "
                      << messageName(msg)
                      << " (MTI=0x" << std::hex << messageMTI(msg) << std::dec << ")\n";
        });

    // Dispatch ChannelRelease -> goes to specific handler.
    std::cout << "  Sending ChannelRelease (600d00):\n";
    {
        auto msg = parseL3Hex("600d00");
        if (msg) disp.dispatch(*msg);
    }

    // Dispatch a PhysicalInformation -> goes to domain handler.
    std::cout << "  Sending PhysicalInformation (602d...):\n";
    {
        auto pi = L3PhysicalInformation::builder()
            .timingAdvance(L3TimingAdvance(42))
            .build();
        ParsedMessage pm{RRM{std::move(pi)}};
        auto bytes = writeL3Bytes(pm);
        if (bytes) disp.dispatchRaw(*bytes);
    }

    std::cout << "\n  Specific: " << stats.specific.load()
              << ", Domain fallback: " << stats.domain.load() << "\n\n";
}

// Demo 3: Global fallback for all unhandled messages.
void demoGlobalFallback() {
    std::cout << "=== Global Fallback Handler ===\n";
    std::cout << "Catches any message type not handled by specific or domain handlers\n\n";

    ProtocolDispatcher disp;
    DispatchStats stats;

    // Register a specific RR handler.
    disp.registerHandler(L3PD::RadioResource, L3ChannelRelease::MTI,
        [&](const ParsedMessage&, void*) {
            ++stats.specific;
            std::cout << "  [SPECIFIC] ChannelRelease\n";
        });

    // Global fallback for anything else.
    disp.setFallbackHandler([&](const ParsedMessage& msg, void*) {
        ++stats.fallback;
        std::cout << "  [FALLBACK] Unhandled: " << messageName(msg)
                  << " (PD=" << messagePD(msg)
                  << ", MTI=0x" << std::hex << messageMTI(msg) << std::dec << ")\n";
    });

    // ChannelRelease -> specific handler.
    std::cout << "  Sending RR ChannelRelease:\n";
    {
        auto msg = parseL3Hex("600d00");
        if (msg) disp.dispatch(*msg);
    }

    // MM message -> fallback.
    std::cout << "  Sending MM CMServiceAccept:\n";
    {
        auto msg = parseL3Hex("5084");
        if (msg) disp.dispatch(*msg);
    }

    // CC message -> fallback.
    std::cout << "  Sending CC Disconnect:\n";
    {
        auto msg = parseL3Hex("3E9408021621");
        if (msg) disp.dispatch(*msg);
    }

    std::cout << "\n  Specific: " << stats.specific.load()
              << ", Fallback: " << stats.fallback.load() << "\n\n";
}

// Demo 4: Full BTS dispatch pipeline with LAPDm unwrapping.
void demoFullPipeline() {
    std::cout << "=== Full BTS Dispatch Pipeline ===\n";
    std::cout << "LAPDm frame -> unwrap -> parse -> dispatch\n\n";

    ProtocolDispatcher disp;
    int handled = 0;

    // Register handlers for several RR types.
    disp.registerHandler(L3PD::RadioResource, L3ChannelRelease::MTI,
        [&](const ParsedMessage&, void*) {
            std::cout << "  [HANDLER] ChannelRelease\n";
            ++handled;
        });

    disp.registerHandler(L3PD::RadioResource, L3PagingRequestType2::MTI,
        [&](const ParsedMessage& msg, void*) {
            std::cout << "  [HANDLER] PagingRequestType2\n";
            ++handled;
        });

    // Global fallback.
    disp.setFallbackHandler([&](const ParsedMessage& msg, void*) {
        std::cout << "  [FALLBACK] " << messageName(msg) << "\n";
        ++handled;
    });

    // Simulate receiving LAPDm frames from the air interface.
    struct IncomingFrame {
        std::string name;
        std::string hexPayload; // L3 payload (without LAPDm header)
    };

    std::vector<IncomingFrame> frames = {
        {"ChannelRelease", "600d00"},
        {"PagingType2", "6022807856341203"}, // TMSI=0x12345678
    };

    for (const auto& frame : frames) {
        auto l3Bytes = hexToBytes(frame.hexPayload);

        // Wrap in LAPDm (simulates receiving from PHY).
        auto lapdmFrame = gsml3parser::lapdm::wrapL3(l3Bytes, SAPI::SAPI0, false);

        // Unwrap and dispatch.
        auto payload = gsml3parser::lapdm::unwrapL3(lapdmFrame);
        if (!payload) continue;

        auto msg = parseL3(*payload);
        if (!msg) continue;

        std::cout << "  Frame \"" << frame.name << "\" -> ";
        disp.dispatch(*msg);
    }

    // Also dispatch a message built from scratch.
    std::cout << "\n  Building and dispatching ImmediateAssignment:\n";
    {
        auto ia = L3ImmediateAssignment::builder()
            .channelDescription(L3ChannelDescription(TDMA_SDCCH, 0, 1, 100))
            .timingAdvance(L3TimingAdvance(32))
            .build();
        ParsedMessage pm{RRM{std::move(ia)}};
        auto bytes = writeL3Bytes(pm);
        if (bytes) {
            // Wrap and unwrap through LAPDm.
            auto lapdmFrame = gsml3parser::lapdm::wrapL3(*bytes, SAPI::SAPI0);
            auto unwrapped = gsml3parser::lapdm::unwrapL3(lapdmFrame);
            if (unwrapped) {
                auto reparsed = parseL3(*unwrapped);
                if (reparsed) disp.dispatch(*reparsed);
            }
        }
    }

    std::cout << "\n  Total messages handled: " << handled << "\n\n";
}

// Demo 5: Priority dispatch (specific > domain > fallback).
void demoPriority() {
    std::cout << "=== Handler Priority ===\n";
    std::cout << "Specific handler takes precedence over domain and fallback\n\n";

    ProtocolDispatcher disp;
    int specific = 0, domain = 0, fallback = 0;

    // Register all three levels.
    disp.setFallbackHandler([&](const ParsedMessage&, void*) {
        ++fallback;
        std::cout << "  [FALLBACK]\n";
    });

    disp.registerDomainHandler(L3PD::RadioResource,
        [&](const ParsedMessage&, void*) {
            ++domain;
            std::cout << "  [DOMAIN]\n";
        });

    disp.registerHandler(L3PD::RadioResource, L3ChannelRelease::MTI,
        [&](const ParsedMessage&, void*) {
            ++specific;
            std::cout << "  [SPECIFIC]\n";
        });

    // ChannelRelease should hit specific handler.
    std::cout << "  ChannelRelease -> ";
    {
        auto msg = parseL3Hex("600d00");
        if (msg) disp.dispatch(*msg);
    }

    // PhysicalInformation should hit domain handler (no specific registered).
    std::cout << "  PhysicalInformation -> ";
    {
        auto pi = L3PhysicalInformation::builder()
            .timingAdvance(L3TimingAdvance(10))
            .build();
        ParsedMessage pm{RRM{std::move(pi)}};
        auto bytes = writeL3Bytes(pm);
        if (bytes) disp.dispatchRaw(*bytes);
    }

    // MM message should hit global fallback.
    std::cout << "  MM CMServiceAccept -> ";
    {
        auto msg = parseL3Hex("5084");
        if (msg) disp.dispatch(*msg);
    }

    std::cout << "\n  specific=" << specific
              << ", domain=" << domain
              << ", fallback=" << fallback << "\n\n";
}

} // anonymous namespace

int main() {
    demoSpecificHandlers();
    demoDomainFallback();
    demoGlobalFallback();
    demoFullPipeline();
    demoPriority();

    std::cout << "All dispatcher demos completed successfully.\n";
    return 0;
}
