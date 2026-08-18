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

// Tests for ResponseSink: zero-overhead fn+ctx callback replacing std::function.
// Validates the two-word size invariant, empty-sink semantics, capturing-lambda
// invocation via makeResponseSink, and refcounted copy/move ownership (no
// double-free).
// 3GPP coverage: TS 24.008 / TS 04.08 procedure response callback mechanism.

#include <gtest/gtest.h>
#include <gsml3parser/stack/response_sink.h>
#include <gsml3parser/rr/l3rrmessages.h>

#include <utility>

using namespace gsml3parser;

// sizeof invariant: two pointers, no hidden state.
TEST(ResponseSink, SizeIsTwoPointers) {
    EXPECT_EQ(sizeof(ResponseSink), 2 * sizeof(void*));
}

// Empty sink is falsy and safe to call-check.
TEST(ResponseSink, Empty_IsFalsy) {
    ResponseSink empty;
    EXPECT_FALSE(static_cast<bool>(empty));
}

// Capturing lambda via makeResponseSink is invoked with the captured state.
TEST(ResponseSink, MakeResponseSink_CapturesAndInvokes) {
    int calls = 0;
    ResponseSink sink = makeResponseSink(
        [&calls](SMAction, const ParsedMessage&, const SubscriberSession*) { ++calls; });
    EXPECT_TRUE(static_cast<bool>(sink));
    ParsedMessage msg{RRM{L3ChannelRelease{}}};
    sink(SMAction::SendResponse, msg, nullptr);
    EXPECT_EQ(calls, 1);
}

// Copies co-own the captured state (refcount), no double-free.
TEST(ResponseSink, Copies_CoOwn_NoDoubleFree) {
    int calls = 0;
    ResponseSink a = makeResponseSink([&calls](SMAction, const ParsedMessage&, const SubscriberSession*) { ++calls; });
    ResponseSink b = a;   // copy -> refcount 2
    ResponseSink c = std::move(a); // move -> a empty, c owns
    a = ResponseSink{};    // destroy a
    ParsedMessage msg{RRM{L3ChannelRelease{}}};
    b(SMAction::None, msg, nullptr);
    c(SMAction::None, msg, nullptr);
    EXPECT_EQ(calls, 2);
}
