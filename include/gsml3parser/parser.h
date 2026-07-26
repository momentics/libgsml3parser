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

#ifndef GSML3PARSER_PARSER_H
#define GSML3PARSER_PARSER_H

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "l3message.h"
#include "l3frame.h"

namespace gsml3parser {

// Forward declarations
class L3RRMessage;
class L3MMMessage;
class L3CCMessage;
class L3SupServMessage;

/**
 * Callback type for parsing messages of a specific Protocol Discriminator
 * that the library does not handle by default (e.g. SMS, GPRS).
 */
using PDHandler = std::function<std::unique_ptr<L3Message>(const L3Frame&)>;

/**
 * Parse a complete L3 message from an L3Frame.
 *
 * This is the primary entry point for the library.
 *
 * @param frame  The L3 frame to parse.
 * @return       A unique_ptr to the parsed message, or nullptr on failure.
 *
 * The caller owns the returned object.
 */
std::unique_ptr<L3Message> parseL3(const L3Frame& frame);

/**
 * Parse a complete L3 message from raw bytes.
 *
 * @param data   Pointer to raw L3 message bytes.
 * @param len    Number of bytes.
 * @return       A unique_ptr to the parsed message, or nullptr on failure.
 */
std::unique_ptr<L3Message> parseL3(const uint8_t* data, size_t len);

/**
 * Parse a complete L3 message from a hex string.
 *
 * @param hex    Hex-encoded L3 message (e.g. "061900...").
 * @return       A unique_ptr to the parsed message, or nullptr on failure.
 */
std::unique_ptr<L3Message> parseL3Hex(const std::string& hex);

/**
 * Register a custom handler for an unsupported Protocol Discriminator.
 *
 * By default, the library handles PD values for RR, MM, CC, and SS.
 * SMS (PD=0x09) and GPRS (PD=0x08, 0x0a) require external handlers.
 *
 * @param pd       The Protocol Discriminator to handle.
 * @param handler  Callback that returns a parsed message, or nullptr.
 */
void registerPDHandler(L3PD pd, PDHandler handler);

/**
 * Remove a previously registered handler for a PD.
 */
void unregisterPDHandler(L3PD pd);

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

// ── RR parser ───────────────────────────────────────────────────────────

/** Parse a complete L3 radio resource message. */
std::unique_ptr<L3RRMessage> parseL3RR(const L3Frame& source);

/** Factory: create an RR message by MTI. Returns nullptr if unsupported. */
L3RRMessage* L3RRFactory(int mti);

// ── MM parser ───────────────────────────────────────────────────────────

/** Parse a complete L3 mobility management message. */
std::unique_ptr<L3MMMessage> parseL3MM(const L3Frame& source);

/** Factory: create an MM message by MTI. Returns nullptr if unsupported. */
L3MMMessage* L3MMFactory(int mti);

// ── CC parser ───────────────────────────────────────────────────────────

/** Parse a complete L3 call control message. */
std::unique_ptr<L3CCMessage> parseL3CC(const L3Frame& source);

/** Factory: create a CC message by MTI. Returns nullptr if unsupported. */
L3CCMessage* L3CCFactory(int mti);

// ── SS parser ───────────────────────────────────────────────────────────

/** Parse a complete L3 supplementary service message. */
std::unique_ptr<L3SupServMessage> parseL3SupServ(const L3Frame& source);

/** Factory: create an SS message by MTI. Returns nullptr if unsupported. */
L3SupServMessage* L3SupServFactory(int mti);

} // namespace gsml3parser

#endif // GSML3PARSER_PARSER_H
