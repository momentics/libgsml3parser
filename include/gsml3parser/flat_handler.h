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

#include <concepts>
#include <cstdint>
#include <memory>
#include <type_traits>
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
 * Per-owner storage for a shared handler.
 *
 * Every FlatHandler copy that owns a shared callable carries its own holder
 * (one small heap allocation) holding a copy of the shared_ptr to the
 * callable. The shared_ptr's atomic reference count is the single source of
 * truth for how many owners (possibly in different objects, threads, or
 * dispatcher slots) remain, so the callable is freed exactly when the last
 * holder is deleted — no double-free, no dangling.
 */
struct SharedHandlerHolder {
    std::shared_ptr<SharedHandlerBase> handler;
};

/// Named trampoline for shared handlers. Its address doubles as the marker
/// distinguishing shared-handler ctx pointers from plain user contexts.
inline void sharedTrampoline(const ParsedMessage* msg, void* ctx) {
    auto* holder = static_cast<SharedHandlerHolder*>(ctx);
    holder->handler->invoke(*msg, nullptr);
}

/// Acquire one ownership reference on a shared handler by copying the
/// shared_ptr into a fresh per-owner holder.
/// @param ctx Existing holder (must be non-null).
/// @return The new holder; the caller becomes its sole owner.
inline SharedHandlerHolder* acquireSharedHandler(void* ctx) {
    auto* holder = static_cast<SharedHandlerHolder*>(ctx);
    return new SharedHandlerHolder{holder->handler};
}

/// Release one ownership reference by deleting the per-owner holder; the
/// shared_ptr copy inside drops the callable's reference count and frees
/// the callable when the last holder goes away.
inline void releaseSharedHandler(void* ctx) noexcept {
    if (!ctx) return;
    delete static_cast<SharedHandlerHolder*>(ctx);
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
 * co-own the callable (refcounted via the shared_ptr held by each copy's
 * per-owner holder); moving transfers ownership without touching the
 * reference count. Handlers created with makeHandler() are trivially
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

    // Copying a shared handler allocates a per-owner holder (a shared_ptr
    // copy), so the copy operations may throw std::bad_alloc and are not
    // marked noexcept. Move and destruction never allocate.
    FlatHandler(const FlatHandler& other)
        : fn(other.fn), ctx(other.ctx) {
        if (isShared()) ctx = detail::acquireSharedHandler(ctx);
    }

    FlatHandler(FlatHandler&& other) noexcept
        : fn(other.fn), ctx(other.ctx) {
        other.fn = nullptr;
        other.ctx = nullptr;
    }

    FlatHandler& operator=(const FlatHandler& other) {
        if (this != &other) {
            // Acquire the new reference before releasing the old one so a
            // failed allocation leaves this handler untouched.
            void* newCtx = other.ctx;
            if (isShared(other.fn, other.ctx)) {
                newCtx = detail::acquireSharedHandler(other.ctx);
            }
            releaseShared();
            fn = other.fn;
            ctx = newCtx;
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
 * No heap allocation.
 *
 * Function pointers and function types are REJECTED (C10): a
 * default-constructed function pointer is null, so the static instance
 * stored by this factory would crash on the first invocation. For plain
 * function pointers with a user context, use the FlatHandler constructor
 * directly: FlatHandler h{myFunction, myCtx};
 *
 * @tparam F Stateless callable type. Must be invocable with
 *          (const ParsedMessage&, void*), default-constructible, and must
 *          not be a pointer or function type.
 */
template<typename F>
    requires (std::is_invocable_v<F, const ParsedMessage&, void*> &&
              std::is_default_constructible_v<F> &&
              !std::is_pointer_v<F> &&
              !std::is_function_v<std::decay_t<F>>)
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
 * The callable is stored in a shared_ptr-controlled allocation referenced
 * by a per-owner holder (two small heap allocations: holder + callable
 * storage). Every copy of the returned FlatHandler co-owns the callable;
 * it is freed when the last owner is destroyed or releases it. This is
 * safe to copy into containers, dispatcher slots, or across threads (the
 * shared_ptr reference count is atomic).
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
 * Safe to call on empty or non-shared handlers (there is no shared storage
 * to free; per the release() contract the handler is still reset to empty).
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
