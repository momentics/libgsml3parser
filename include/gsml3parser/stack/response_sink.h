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

/// Zero-overhead response callback (replaces std::function) for protocol procedures.
///
/// Two-word struct (fn pointer + context) with RAII refcounting for capturing lambdas.
/// Invocation is a direct function-pointer call — no virtual dispatch, no type erasure,
/// no per-call heap allocation. Capturing lambdas are stored in a refcounted holder
/// (one heap allocation at creation, shared by all copies); non-capturing callables use
/// a plain function pointer with user context.
///
/// Thread safety: copies may be shared across threads (refcount is atomic). The invoked
/// callback itself is the user's responsibility to make thread-safe.
/// Memory: sizeof(ResponseSink) == 2 * sizeof(void*).
#pragma once

#include <atomic>
#include <concepts>
#include <cstddef>
#include <memory>
#include <utility>

#include "gsml3parser/message_types.h"
#include "gsml3parser/stack/state_machine.h" // SMAction

namespace gsml3parser {

class SubscriberSession;

namespace detail {

/// Base class for type-erased shared response sink storage.
struct ResponseSinkSharedBase {
    virtual ~ResponseSinkSharedBase() = default;
    virtual void invoke(SMAction action, const ParsedMessage& msg, const SubscriberSession* session) = 0;
};

/// Template implementation of ResponseSinkSharedBase that stores a callable F.
template<typename F>
struct ResponseSinkSharedImpl : ResponseSinkSharedBase {
    F fn;
    explicit ResponseSinkSharedImpl(F f) : fn(std::move(f)) {}
    void invoke(SMAction a, const ParsedMessage& m, const SubscriberSession* s) override { fn(a, m, s); }
};

/// Refcounted holder for a shared response sink.
/// One heap allocation owns the callable; the atomic refcount tracks how many
/// ResponseSink values co-own it. The holder is deleted when the last owner releases.
struct ResponseSinkHolder {
    std::shared_ptr<ResponseSinkSharedBase> handler;
    std::atomic<int> refcount{1};
};

/// Named trampoline for shared sinks. Its address doubles as the marker
/// distinguishing shared-sink ctx pointers from plain user contexts.
inline void responseSinkTrampoline(SMAction a, const ParsedMessage& m, const SubscriberSession* s, void* ctx) {
    static_cast<ResponseSinkHolder*>(ctx)->handler->invoke(a, m, s);
}

/// Acquire one ownership reference on a shared sink holder.
inline void acquireResponseSink(void* ctx) noexcept {
    if (ctx) static_cast<ResponseSinkHolder*>(ctx)->refcount.fetch_add(1, std::memory_order_relaxed);
}

/// Release one ownership reference; deletes the holder at zero.
inline void releaseResponseSink(void* ctx) noexcept {
    if (!ctx) return;
    auto* h = static_cast<ResponseSinkHolder*>(ctx);
    if (h->refcount.fetch_sub(1, std::memory_order_acq_rel) == 1) delete h;
}

} // namespace detail

/// Zero-overhead response callback (fn + context). See header docs for semantics.
struct ResponseSink {
    using Callback = void (*)(SMAction, const ParsedMessage&, const SubscriberSession*, void*);

    Callback fn{nullptr};
    void* ctx{nullptr};

    constexpr ResponseSink() noexcept = default;
    constexpr ResponseSink(Callback f, void* c) noexcept : fn(f), ctx(c) {}

    /// Copy constructor: co-owns the shared callable (refcount bump) if present.
    ResponseSink(const ResponseSink& o) noexcept : fn(o.fn), ctx(o.ctx) {
        if (isShared()) detail::acquireResponseSink(ctx);
    }

    /// Move constructor: transfers ownership without touching the refcount.
    ResponseSink(ResponseSink&& o) noexcept : fn(o.fn), ctx(o.ctx) { o.fn = nullptr; o.ctx = nullptr; }

    /// Copy assignment: releases the old shared owner, then co-owns the new one.
    ResponseSink& operator=(const ResponseSink& o) noexcept {
        if (this != &o) { releaseShared(); fn = o.fn; ctx = o.ctx; if (isShared()) detail::acquireResponseSink(ctx); }
        return *this;
    }

    /// Move assignment: transfers ownership without touching the refcount.
    ResponseSink& operator=(ResponseSink&& o) noexcept {
        if (this != &o) { releaseShared(); fn = o.fn; ctx = o.ctx; o.fn = nullptr; o.ctx = nullptr; }
        return *this;
    }

    /// Destructor: releases the shared callable ownership if this is the last owner.
    ~ResponseSink() noexcept { releaseShared(); }

    /// Release one ownership reference and reset this sink to empty.
    void release() noexcept { releaseShared(); fn = nullptr; ctx = nullptr; }

    /// Invoke the callback. Direct function-pointer call, no type erasure.
    /// @param action The SMAction that triggered the response.
    /// @param msg The parsed L3 message that triggered the response.
    /// @param session The subscriber session context (may be nullptr).
    void operator()(SMAction action, const ParsedMessage& msg, const SubscriberSession* session) const {
        fn(action, msg, session, ctx);
    }

    explicit operator bool() const noexcept { return fn != nullptr; }

private:
    /// True if this sink owns a refcounted shared callable.
    static bool isShared(Callback f, void* c) noexcept {
        return f == &detail::responseSinkTrampoline && c != nullptr;
    }
    [[nodiscard]] bool isShared() const noexcept {
        return isShared(fn, ctx);
    }
    void releaseShared() noexcept {
        if (isShared()) detail::releaseResponseSink(ctx);
    }
};

static_assert(sizeof(ResponseSink) == 2 * sizeof(void*), "ResponseSink must be exactly two pointers");

/// Wrap a capturing callable (3-arg: SMAction, const ParsedMessage&, const SubscriberSession*)
/// into a refcounted ResponseSink. One heap allocation at creation; zero per invocation.
///
/// @tparam F Callable invocable with (SMAction, const ParsedMessage&, const SubscriberSession*).
/// @param f The callable to wrap (moved into the refcounted holder).
/// @return A ResponseSink co-owning the callable; copies share ownership.
template<typename F>
    requires std::is_invocable_v<F, SMAction, const ParsedMessage&, const SubscriberSession*>
ResponseSink makeResponseSink(F f) {
    auto* holder = new detail::ResponseSinkHolder{
        std::make_shared<detail::ResponseSinkSharedImpl<std::decay_t<F>>>(std::move(f))
    };
    return ResponseSink{&detail::responseSinkTrampoline, holder};
}

} // namespace gsml3parser
