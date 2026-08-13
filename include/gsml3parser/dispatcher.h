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

/// Message dispatcher for BTS-style protocol handling.
/// Provides a callback-based routing mechanism for incoming L3 messages,
/// allowing BTS implementations to register handlers per message type.
#pragma once

#include <functional>
#include <unordered_map>
#include <cstdint>
#include <span>
#include "gsml3parser/message_types.h"
#include "gsml3parser/visitor.h"
#include "gsml3parser/parser.h"

namespace gsml3parser {

/// Signature for a message handler callback.
/// @param msg The parsed L3 message.
/// @param context User-provided context pointer (e.g., channel state).
using MessageHandler = std::function<void(const ParsedMessage& msg, void* context)>;

/// Dispatches incoming L3 messages to registered type-specific handlers.
class ProtocolDispatcher {
public:
    /// Register a handler for a specific message type.
    /// The handler is called when a message of the given PD + MTI is dispatched.
    /// @param pd Protocol Discriminator domain.
    /// @param mti Message Type Indicator.
    /// @param handler Callback function.
    void registerHandler(L3PD pd, int mti, MessageHandler handler);

    /// Register a catch-all handler for all messages in a PD domain.
    void registerDomainHandler(L3PD pd, MessageHandler handler);

    /// Register a global fallback handler for unregistered message types.
    void setFallbackHandler(MessageHandler handler);

    /// Dispatch a parsed message to the appropriate handler.
    /// @param msg The parsed L3 message.
    /// @param context Optional user context passed to handlers.
    void dispatch(const ParsedMessage& msg, void* context = nullptr);

    /// Parse raw bytes and dispatch in one call.
    /// @param data Raw L3 message bytes.
    /// @param context Optional user context.
    /// @return true if a handler was invoked, false on parse error or no handler.
    bool dispatchRaw(std::span<const uint8_t> data, void* context = nullptr);

private:
    struct HandlerKey {
        L3PD pd;
        int mti;
        bool operator==(const HandlerKey& o) const { return pd == o.pd && mti == o.mti; }
    };
    struct HandlerKeyHash {
        std::size_t operator()(const HandlerKey& k) const {
            return static_cast<std::size_t>(k.pd) * 257 + k.mti;
        }
    };

    std::unordered_map<HandlerKey, MessageHandler, HandlerKeyHash> mHandlers;
    std::unordered_map<L3PD, MessageHandler> mDomainHandlers;
    MessageHandler mFallback;
};

} // namespace gsml3parser
