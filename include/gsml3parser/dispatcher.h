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

#include <array>
#include <functional>
#include <optional>
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

    /// @brief Register a catch-all handler for all messages in a PD domain.
    /// @param pd Protocol Discriminator domain.
    /// @param handler Callback invoked for any unregistered MTI within this PD.
    void registerDomainHandler(L3PD pd, MessageHandler handler);

    /// @brief Register a global fallback handler for unregistered message types.
    /// @param handler Callback invoked when no specific or domain handler matches.
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

    /// Register a handler that matches on TI (Transaction Identifier).
    /// Used for CC/SS messages where the same message type can belong to
    /// different concurrent transactions.
    /// @param ti Transaction Identifier (0-7).
    /// @param handler Callback invoked when a CC/SS message with matching TI is dispatched via dispatchWithTI().
    void registerTIHandler(uint8_t ti, MessageHandler handler);

    /// Dispatch with TI awareness: for CC/SS messages, tries TI-specific handler first.
    /// Falls back to specific handler (PD+MTI), then domain handler, then global fallback.
    /// Performance: TI lookup is O(1) via std::array<MessageHandler, 8>.
    /// @param msg The parsed L3 message.
    /// @param context Optional user context passed to handlers.
    void dispatchWithTI(const ParsedMessage& msg, void* context = nullptr);

private:
    // Per-PD, per-MTI handler table. L3PD has 16 values (0x00..0x0f), MTI fits in 0..255.
    // O(1) direct array index — no hash, no heap allocation for nodes.
    std::array<std::array<std::optional<MessageHandler>, 256>, 16> mHandlers{};

    // Domain-level fallback handlers, indexed by L3PD (16 entries).
    // O(1) direct array index.
    std::array<std::optional<MessageHandler>, 16> mDomainHandlers{};

    MessageHandler mFallback;

    // TI-indexed handlers for CC/SS messages - O(1) lookup.
    std::array<std::optional<MessageHandler>, 8> mTIHandlers{};
};

} // namespace gsml3parser
