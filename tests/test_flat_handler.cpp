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
#include <type_traits>
#include <utility>

#include <gsml3parser/flat_handler.h>
#include <gsml3parser/cc/l3ccmessages.h>

using namespace gsml3parser;

namespace {

// ── Compile-time checks (C10 regression guards) ─────────────────────────
//
// makeHandler() must REFUSE function pointers: with the old unconstrained
// template, F = T(*)(...) produced `static const F instance{}` — a null
// function pointer — and the first invocation crashed. It must still
// ACCEPT stateless callables (non-capturing lambdas).
//
// The constraint is probed with a detection idiom: naming a constrained
// function template with a failing constraint (e.g. &makeHandler<FP>) is a
// hard error, whereas overload resolution via declval() is SFINAE-friendly.

template<typename F, typename = void>
struct makeHandlerAccepts : std::false_type {};

template<typename F>
struct makeHandlerAccepts<F,
    std::void_t<decltype(makeHandler(std::declval<F>()))>> : std::true_type {};

using HandlerFunctionPointer = void (*)(const ParsedMessage&, void*);

static_assert(!makeHandlerAccepts<HandlerFunctionPointer>::value,
              "makeHandler must reject function pointers (C10)");

struct StatelessProbe {
    void operator()(const ParsedMessage&, void*) const noexcept {}
};

static_assert(makeHandlerAccepts<StatelessProbe>::value,
              "makeHandler must accept stateless callables");

auto nonCapturingLambda = [](const ParsedMessage&, void*) noexcept {};
using NonCapturingLambda = decltype(nonCapturingLambda);

static_assert(makeHandlerAccepts<NonCapturingLambda>::value,
              "makeHandler must accept non-capturing lambdas");

// Size invariant (also asserted in flat_handler.h).
static_assert(sizeof(FlatHandler) == 2 * sizeof(void*));

// Free function + context used by the positive constructor test.
struct FreeFunctionContext {
    int calls{0};
};

// Signature matches FlatHandler::Callback exactly (pointer, not reference).
void freeFunctionHandler(const ParsedMessage* msg, void* ctx) {
    (void)msg;
    auto* c = static_cast<FreeFunctionContext*>(ctx);
    if (c) ++c->calls;
}

ParsedMessage makeDisconnectMessage() {
    L3Disconnect disc(CCCause::Normal_Call_Clearing);
    disc.ti(2);
    return ParsedMessage{CCM{disc}};
}

} // namespace

// Positive test (step 8.1): the FlatHandler constructor with a plain
// function pointer and a user context works correctly.
TEST(FlatHandlerTest, Constructor_FreeFunctionWithContext) {
    FreeFunctionContext ctx;
    FlatHandler h{freeFunctionHandler, &ctx};
    ASSERT_TRUE(static_cast<bool>(h));

    ParsedMessage msg = makeDisconnectMessage();
    h(msg, &ctx);
    EXPECT_EQ(ctx.calls, 1);

    // Copies of a non-shared handler are trivial value copies.
    FlatHandler copy = h;
    EXPECT_EQ(copy, h);
    copy(msg, &ctx);
    EXPECT_EQ(ctx.calls, 2);
}

// makeHandler with a non-capturing lambda dispatches without allocation.
TEST(FlatHandlerTest, MakeHandler_NonCapturingLambda) {
    FlatHandler h = makeHandler([](const ParsedMessage&, void*) noexcept {});
    ASSERT_TRUE(static_cast<bool>(h));
    // Stateless handler: ctx must be null.
    EXPECT_EQ(h.ctx, nullptr);

    ParsedMessage msg = makeDisconnectMessage();
    h(msg);  // must not crash
}

// makeSharedHandler copies co-own the callable; every owner can invoke it
// and the storage is freed exactly once when the last owner goes away
// (double-free / use-after-free is caught by the ASAN build).
TEST(FlatHandlerTest, MakeSharedHandler_CopyCoOwnership) {
    int count = 0;
    FlatHandler h = makeSharedHandler([&count](const ParsedMessage&, void*) { ++count; });
    ASSERT_TRUE(static_cast<bool>(h));

    FlatHandler c1 = h;            // copy
    FlatHandler c2 = h;            // copy
    FlatHandler c3 = std::move(c1);  // move (source becomes empty)
    EXPECT_FALSE(static_cast<bool>(c1));

    ParsedMessage msg = makeDisconnectMessage();
    h(msg);
    c2(msg);
    c3(msg);
    EXPECT_EQ(count, 3);

    // Copy-assignment replaces ownership: c2 now references 'other's'
    // callable (the no-op), and the counter lambda's storage is released as
    // its owners go away (no double-free — verified by the ASAN build).
    FlatHandler other = makeSharedHandler([](const ParsedMessage&, void*) {});
    c2 = other;
    c2(msg);   // c2 invokes the no-op now, not the counter
    other(msg);
    EXPECT_EQ(count, 3);
}

// release() drops one ownership reference and resets the handler to empty;
// remaining copies keep working.
TEST(FlatHandlerTest, Release_EmptiesHandler) {
    int count = 0;
    FlatHandler h = makeSharedHandler([&count](const ParsedMessage&, void*) { ++count; });
    FlatHandler c = h;

    h.release();
    EXPECT_FALSE(static_cast<bool>(h));
    EXPECT_EQ(h.fn, nullptr);
    EXPECT_EQ(h.ctx, nullptr);

    ParsedMessage msg = makeDisconnectMessage();
    c(msg);
    EXPECT_EQ(count, 1);
}

// destroySharedHandler is safe on empty handlers; for non-shared handlers
// there is no shared storage to free, but the documented release() contract
// still resets the handler to empty.
TEST(FlatHandlerTest, DestroySharedHandler_SafeOnEmptyAndPlain) {
    FlatHandler empty;
    destroySharedHandler(empty);  // must not crash
    EXPECT_FALSE(static_cast<bool>(empty));

    FreeFunctionContext ctx;
    FlatHandler plain{freeFunctionHandler, &ctx};
    destroySharedHandler(plain);  // no shared storage; handler emptied
    EXPECT_FALSE(static_cast<bool>(plain));
    EXPECT_EQ(plain.ctx, nullptr);
}

// Move transfers ownership and leaves the source empty.
TEST(FlatHandlerTest, Move_ResetsSource) {
    int count = 0;
    FlatHandler h = makeSharedHandler([&count](const ParsedMessage&, void*) { ++count; });
    FlatHandler m = std::move(h);
    EXPECT_FALSE(static_cast<bool>(h));
    EXPECT_TRUE(static_cast<bool>(m));

    ParsedMessage msg = makeDisconnectMessage();
    m(msg);
    EXPECT_EQ(count, 1);
}
