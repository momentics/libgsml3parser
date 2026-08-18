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
///
/// Ownership model:
///   - makeHandler(): non-owning. fn is a plain function pointer, ctx is a
///     user-managed pointer. Copies are trivial.
///   - makeSharedHandler(): owning. The capturing callable is stored in a
///     refcounted holder; every FlatHandler copy co-owns it. Destruction of
///     the last owner frees the callable. This makes copied handlers safe
///     (no double-free / use-after-free) and container-friendly.
#pragma once

#include <atomic>
#include <concepts>
#include <cstdint>
#include <memory>
#include <utility>

#include "gsml3parser/message_types.h"

namespace gsml3parser {

namespace detail {

/**
 * Base class for type-erased shared handler storage.
 * Used internally by makeSharedHandler to store capturing lambdas.
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
 * Refcounted holder for a shared handler.
 *
 * A single heap allocation owns the callable; the atomic refcount tracks
 * how many FlatHandler values (possibly in different objects, threads, or
 * dispatcher slots) co-own it. The holder is deleted when the last owner
 * releases it, so copied FlatHandlers can never double-free or dangle.
 */
struct SharedHandlerHolder {
    std::shared_ptr<SharedHandlerBase> handler;
    std::atomic<int> refcount{1};
};

/// Named trampoline for shared handlers. Its address doubles as the marker
/// distinguishing shared-handler ctx pointers from plain user contexts.
inline void sharedTrampoline(const ParsedMessage* msg, void* ctx) {
    auto* holder = static_cast<SharedHandlerHolder*>(ctx);
    holder->handler->invoke(*msg, nullptr);
}

/// Acquire one ownership reference on a shared handler holder.
inline void acquireSharedHandler(void* ctx) noexcept {
    if (ctx) {
        static_cast<SharedHandlerHolder*>(ctx)->refcount.fetch_add(1, std::memory_order_relaxed);
    }
}

/// Release one ownership reference; deletes the holder at zero.
inline void releaseSharedHandler(void* ctx) noexcept {
    if (!ctx) return;
    auto* holder = static_cast<SharedHandlerHolder*>(ctx);
    int prev = holder->refcount.fetch_sub(1, std::memory_order_acq_rel);
    if (prev == 1) {
        delete holder;
    }
}

} // namespace detail

/**
 * Zero-overhead message handler using a raw function pointer and context.
 *
 * This type replaces std::function<void(const ParsedMessage*, void*)> to
 * eliminate virtual call overhead on the dispatch hot path.  Each instance
 * is exactly two machine words (pointer + pointer).
 *
 * Copy/move semantics: copies of a handler created with makeSharedHandler()
 * co-own the callable (refcounted); moving transfers ownership without
 * touching the refcount. Handlers created with makeHandler() are trivially
 * copyable value types.
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

    // ── RAII ownership for shared handlers ──────────────────────────────

    FlatHandler(const FlatHandler& other) noexcept
        : fn(other.fn), ctx(other.ctx) {
        if (isShared()) detail::acquireSharedHandler(ctx);
    }

    FlatHandler(FlatHandler&& other) noexcept
        : fn(other.fn), ctx(other.ctx) {
        other.fn = nullptr;
        other.ctx = nullptr;
    }

    FlatHandler& operator=(const FlatHandler& other) noexcept {
        if (this != &other) {
            releaseShared();
            fn = other.fn;
            ctx = other.ctx;
            if (isShared()) detail::acquireSharedHandler(ctx);
        }
        return *this;
    }

    FlatHandler& operator=(FlatHandler&& other) noexcept {
        if (this != &other) {
            releaseShared();
            fn = other.fn;
            ctx = other.ctx;
            other.fn = nullptr;
            other.ctx = nullptr;
        }
        return *this;
    }

    ~FlatHandler() noexcept {
        releaseShared();
    }

    /**
     * Release one ownership reference to the shared handler storage
     * (no-op for non-shared handlers) and reset this handler to empty.
     * Equivalent to the old destroySharedHandler() for a single owner.
     */
    void release() noexcept {
        releaseShared();
        fn = nullptr;
        ctx = nullptr;
    }

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

private:
    /// True if this handler owns a refcounted shared callable.
    static bool isShared(Callback f, void* c) noexcept {
        return f == &detail::sharedTrampoline && c != nullptr;
    }
    [[nodiscard]] bool isShared() const noexcept {
        return isShared(fn, ctx);
    }
    void releaseShared() noexcept {
        if (isShared()) detail::releaseSharedHandler(ctx);
    }
};

static_assert(sizeof(FlatHandler) == 2 * sizeof(void*),
              "FlatHandler must be exactly two pointers");

// ── Factory for non-capturing lambdas (zero allocation) ────────────────

/**
 * Creates a FlatHandler from a stateless callable (non-capturing lambda).
 * No heap allocation. For plain function pointers with a user context,
 * use the FlatHandler constructor directly: FlatHandler h{myFunction, myCtx};
 */
template<typename F>
    requires (std::is_invocable_v<F, const ParsedMessage&, void*> &&
              std::is_default_constructible_v<F>)
FlatHandler makeHandler(F) noexcept {
    // Stateless instance per callable type; the trampoline lambda below is
    // non-capturing, so it converts to a plain function pointer.
    static const F instance{};
    return FlatHandler{[](const ParsedMessage* msg, void*) {
        instance(*msg, nullptr);
    }, nullptr};
}

/**
 * Creates a FlatHandler owning a capturing callable.
 *
 * The callable is stored in a refcounted holder (one heap allocation).
 * Every copy of the returned FlatHandler co-owns the callable; it is freed
 * when the last owner is destroyed or releases it. This is safe to copy
 * into containers, dispatcher slots, or across threads (the refcount is
 * atomic).
 */
template<typename F>
    requires std::is_invocable_v<F, const ParsedMessage&, void*>
FlatHandler makeSharedHandler(F f) {
    auto* holder = new detail::SharedHandlerHolder{
        std::make_shared<detail::SharedHandlerImpl<F>>(std::move(f))
    };
    return FlatHandler{&detail::sharedTrampoline, holder};
}

/**
 * Releases the shared handler allocation owned by \p h and resets it to
 * empty. Kept for API compatibility; FlatHandler's RAII already releases
 * ownership on destruction/assignment, so this is only needed to release
 * one of several co-owning copies explicitly.
 * Safe to call on empty or non-shared handlers (no-op).
 */
inline void destroySharedHandler(FlatHandler& h) {
    h.release();
}

// ── operator() implementation ──────────────────────────────────────────

inline void FlatHandler::operator()(const ParsedMessage& msg, void* userCtx) const {
    // For shared handlers, ctx points to the refcounted holder which owns
    // the callable; the trampoline extracts and invokes it.
    // Captured lambdas access their state through captures; userCtx is ignored.
    (void)userCtx;
    fn(&msg, ctx);
}

} // namespace gsml3parser
