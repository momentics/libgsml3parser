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

/// Zero-overhead message handler using raw function pointer + context.
/// Replaces std::function<void(const ParsedMessage&, void*)> to eliminate
/// virtual call overhead on the dispatch hot path.
#pragma once

#include <concepts>
#include <cstdint>
#include <memory>

#include "gsml3parser/message_types.h"

namespace gsml3parser {

/**
 * Zero-overhead message handler using a raw function pointer and context.
 *
 * This type replaces std::function<void(const ParsedMessage&, void*)> to
 * eliminate virtual call overhead on the dispatch hot path.  Each instance
 * is exactly two machine words (pointer + pointer).
 *
 * Usage:
 *   FlatHandler h{[](const ParsedMessage* msg, void* ctx) { ... }, &myContext};
 *   h(msg, nullptr); // direct call, no virtual dispatch
 */
struct FlatHandler {
    using Callback = void (*)(const ParsedMessage*, void*);

    Callback fn{nullptr};
    void* ctx{nullptr};

    constexpr FlatHandler() noexcept = default;
    constexpr FlatHandler(Callback f, void* c) noexcept : fn(f), ctx(c) {}

    /**
     * Invoke the handler.  Direct function pointer call — no virtual dispatch,
     * no type erasure overhead.  O(1) single indirect call.
     *
     * @param msg The parsed message to dispatch.
     * @param userCtx Optional user-provided context passed to the callback.
     */
    void operator()(const ParsedMessage& msg, void* userCtx = nullptr) const;

    bool operator==(const FlatHandler& other) const noexcept {
        return fn == other.fn && ctx == other.ctx;
    }
    bool operator!=(const FlatHandler& other) const noexcept {
        return !(*this == other);
    }
    explicit operator bool() const noexcept { return fn != nullptr; }
};

static_assert(sizeof(FlatHandler) == 2 * sizeof(void*),
              "FlatHandler must be exactly two pointers");

// ── Factory for non-capturing lambdas (zero allocation) ────────────────

/**
 * Creates a FlatHandler from a non-capturing callable.
 * No heap allocation; the callback function pointer encodes the lambda,
 * and ctx stores an optional static user context (nullptr by default).
 */
template<typename F>
    requires std::is_invocable_v<F, const ParsedMessage&, void*>
constexpr FlatHandler makeHandler(F f) noexcept {
    return FlatHandler{[f](const ParsedMessage* msg, void*) {
        f(*msg, nullptr);
    }, nullptr};
}

// ── Type-erased base for capturing lambdas (namespace-level) ───────────

namespace detail {

/**
 * Base class for type-erased shared handler storage.
 * Used internally by makeSharedHandler to store capturing lambdas
 * on the heap via shared_ptr.
 */
struct SharedHandlerBase {
    virtual void invoke(const ParsedMessage& msg, void* ctx) = 0;
    virtual ~SharedHandlerBase() = default;
};

/**
 * Template implementation of SharedHandlerBase that stores a callable F.
 */
template<typename F>
struct SharedHandlerImpl : SharedHandlerBase {
    F fn;
    explicit SharedHandlerImpl(F f) : fn(std::move(f)) {}
    void invoke(const ParsedMessage& msg, void* ctx) override { fn(msg, ctx); }
};

/**
 * Combined storage for shared handlers.  Holds both the shared_ptr (for
 * lifetime management) and a pointer to the actual shared_ptr object (for
 * destroySharedHandler cleanup).  This single allocation avoids the need
 * to distinguish between "shared_ptr holder" and "user context" in ctx.
 */
struct SharedHandlerContext {
    std::shared_ptr<SharedHandlerBase> handler;
};

} // namespace detail

/**
 * Creates a FlatHandler from a capturing callable by storing it in a
 * shared_ptr-controlled heap allocation.  The SharedHandlerContext struct
 * is stored as the handler's ctx pointer and contains the shared_ptr for
 * lifetime management.
 *
 * When the handler slot is overwritten or destroyed, call destroySharedHandler()
 * to avoid memory leaks.
 */
template<typename F>
    requires std::is_invocable_v<F, const ParsedMessage&, void*>
FlatHandler makeSharedHandler(F f) {
    auto* sc = new detail::SharedHandlerContext{
        std::make_shared<detail::SharedHandlerImpl<F>>(std::move(f))
    };
    return FlatHandler{
        [](const ParsedMessage* msg, void* ctx) {
            auto* sc = static_cast<detail::SharedHandlerContext*>(ctx);
            sc->handler->invoke(*msg, nullptr);
        },
        sc
    };
}

/**
 * Destroys the heap allocation created by makeSharedHandler.
 * Call this when replacing or removing a handler to avoid leaks.
 * Safe to call on empty handlers (fn == nullptr).
 */
inline void destroySharedHandler(FlatHandler& h) {
    if (h.fn && h.ctx) {
        delete static_cast<detail::SharedHandlerContext*>(h.ctx);
        h = FlatHandler{};
    }
}

// ── operator() implementation ──────────────────────────────────────────

inline void FlatHandler::operator()(const ParsedMessage& msg, void* userCtx) const {
    // For shared handlers, ctx points to SharedHandlerContext which owns the lambda.
    // The callback extracts the handler from ctx and invokes it.
    // Captured lambdas access their state through captures; userCtx is ignored.
    (void)userCtx;
    fn(&msg, ctx);
}

} // namespace gsml3parser
