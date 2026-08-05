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

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>

#include "l3message.h"
#include "l3frame.h"
#include "context.h"

namespace gsml3parser {

// Forward declarations
class L3RRMessage;
class L3MMMessage;
class L3CCMessage;
class L3SupServMessage;

// ── Primary API: context-aware parsers ──────────────────────────────────

/**
 * Parse a complete L3 message from an L3Frame using the given context.
 *
 * @param frame  The L3 frame to parse.
 * @param ctx    Parser configuration (PD handlers, log level).
 * @return       A unique_ptr to the parsed message, or nullptr on failure.
 */
std::unique_ptr<L3Message> parseL3(const L3Frame& frame, const ParserContext& ctx);

/**
  * Parse a complete L3 message from a byte span using the given context.
  *
  * @param data   Span of raw L3 message bytes.
  * @param ctx    Parser configuration (PD handlers, log level).
  * @return       A unique_ptr to the parsed message, or nullptr on failure.
  */
std::unique_ptr<L3Message> parseL3(std::span<const uint8_t> data, const ParserContext& ctx);

/**
  * Parse a complete L3 message from a hex string using the given context.
  *
  * @param hex    Hex-encoded L3 message (e.g. "061900...").
  * @param ctx    Parser configuration (PD handlers, log level).
  * @return       A unique_ptr to the parsed message, or nullptr on failure.
  */
std::unique_ptr<L3Message> parseL3Hex(std::string_view hex, const ParserContext& ctx);

// ── Serializers (stateless, no context needed) ──────────────────────────

/**
 * Write an L3Message to raw bytes.
 *
 * @param msg     The message to serialize.
 * @param out     Output buffer (must be at least msg.fullLength() bytes).
 * @param maxlen  Maximum number of bytes to write.
 * @return        Number of bytes written, or 0 on error.
 */
size_t writeL3(const L3Message& msg, uint8_t* out, size_t maxlen);

/**
 * Write an L3Message to a hex string.
 *
 * @param msg     The message to serialize.
 * @return        Hex-encoded string.
 */
std::string writeL3Hex(const L3Message& msg);

// ── Domain parsers (internal) ───────────────────────────────────────────

/** Parse a complete L3 radio resource message. */
std::unique_ptr<L3RRMessage> parseL3RR(const L3Frame& source);

/** Factory: create an RR message by MTI. Returns nullptr if unsupported. */
std::unique_ptr<L3RRMessage> L3RRFactory(int mti);

/** Parse a complete L3 mobility management message. */
std::unique_ptr<L3MMMessage> parseL3MM(const L3Frame& source);

/** Factory: create an MM message by MTI. Returns nullptr if unsupported. */
std::unique_ptr<L3MMMessage> L3MMFactory(int mti);

/** Parse a complete L3 call control message. */
std::unique_ptr<L3CCMessage> parseL3CC(const L3Frame& source);

/** Factory: create a CC message by MTI. Returns nullptr if unsupported. */
std::unique_ptr<L3CCMessage> L3CCFactory(int mti);

/** Parse a complete L3 supplementary service message. */
std::unique_ptr<L3SupServMessage> parseL3SupServ(const L3Frame& source);

/** Factory: create an SS message by MTI. Returns nullptr if unsupported. */
std::unique_ptr<L3SupServMessage> L3SupServFactory(int mti);

} // namespace gsml3parser


