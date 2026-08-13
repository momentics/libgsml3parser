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

#include <gtest/gtest.h>
#include <gsml3parser/dispatcher.h>
#include <gsml3parser/parser.h>
#include <gsml3parser/rr/l3rrmessages.h>

using namespace gsml3parser;

// GSM 04.08 9.1.7: Channel Release dispatched to specific handler
TEST(DispatcherTest, SpecificHandlerCalled) {
    bool called = false;
    ProtocolDispatcher disp;
    disp.registerHandler(L3PD::RadioResource, L3ChannelRelease::MTI,
        [&](const ParsedMessage& msg, void*) {
            auto* cr = tryGet<L3ChannelRelease>(msg);
            ASSERT_TRUE(cr);
            EXPECT_EQ(cr->cause(), RRCause::Normal_Event);
            called = true;
        });

    // "600d00" = RR PD(0x60), MTI=0x0D(ChannelRelease), cause=0x00
    auto msg = parseL3Hex("600d00");
    ASSERT_TRUE(msg);
    disp.dispatch(*msg);
    EXPECT_TRUE(called);
}

// Domain handler catches unregistered RR message types
TEST(DispatcherTest, DomainHandlerFallback) {
    bool domainCalled = false;
    ProtocolDispatcher disp;
    disp.registerDomainHandler(L3PD::RadioResource,
        [&](const ParsedMessage&, void*) { domainCalled = true; });

    auto msg = parseL3Hex("600d00");
    ASSERT_TRUE(msg);
    disp.dispatch(*msg);
    EXPECT_TRUE(domainCalled);
}

// Dispatch raw bytes directly to handler
TEST(DispatcherTest, DispatchRawBytes) {
    bool called = false;
    ProtocolDispatcher disp;
    disp.registerHandler(L3PD::RadioResource, L3ChannelRelease::MTI,
        [&](const ParsedMessage&, void*) { called = true; });

    uint8_t data[] = {0x60, 0x0D, 0x00};
    EXPECT_TRUE(disp.dispatchRaw(std::span<const uint8_t>(data)));
    EXPECT_TRUE(called);
}

// Specific handler takes precedence over domain handler
TEST(DispatcherTest, SpecificOverridesDomain) {
    bool specificCalled = false;
    bool domainCalled = false;
    ProtocolDispatcher disp;
    disp.registerDomainHandler(L3PD::RadioResource,
        [&](const ParsedMessage&, void*) { domainCalled = true; });
    disp.registerHandler(L3PD::RadioResource, L3ChannelRelease::MTI,
        [&](const ParsedMessage&, void*) { specificCalled = true; });

    auto msg = parseL3Hex("600d00");
    ASSERT_TRUE(msg);
    disp.dispatch(*msg);
    EXPECT_TRUE(specificCalled);
    EXPECT_FALSE(domainCalled);
}

// Fallback handler catches unregistered PD + MTI
TEST(DispatcherTest, GlobalFallback) {
    bool fallbackCalled = false;
    ProtocolDispatcher disp;
    disp.setFallbackHandler([&](const ParsedMessage&, void*) { fallbackCalled = true; });

    auto msg = parseL3Hex("600d00");
    ASSERT_TRUE(msg);
    disp.dispatch(*msg);
    EXPECT_TRUE(fallbackCalled);
}

// dispatchRaw returns false on parse error
TEST(DispatcherTest, DispatchRawInvalidData) {
    ProtocolDispatcher disp;
    uint8_t data[] = {0x00}; // Too short to be valid L3
    EXPECT_FALSE(disp.dispatchRaw(std::span<const uint8_t>(data)));
}
