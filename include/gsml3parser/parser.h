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

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include "expected.h"
#include "message_types.h"
#include "parser_config.h"

namespace gsml3parser {

/**
 * Parse a complete L3 message from a raw byte span.
 *
 * @param data  Span of raw L3 message bytes (header + body).
 * @param cfg   Parser configuration (log level).
 * @return      Expected<ParsedMessage> holding the parsed variant, or ParseError on failure.
 */
[[nodiscard]] Expected<ParsedMessage> parseL3(std::span<const uint8_t> data, const ParserConfig& cfg = {});

/**
 * Parse a complete L3 message from a hex-encoded string.
 *
 * @param hex   Hex-encoded L3 message (e.g. "060D00").
 * @param cfg   Parser configuration (log level).
 * @return      Expected<ParsedMessage> holding the parsed variant, or ParseError on failure.
 */
[[nodiscard]] Expected<ParsedMessage> parseL3Hex(std::string_view hex, const ParserConfig& cfg = {});

/**
 * Serialize a ParsedMessage to raw bytes (header + body).
 *
 * @param msg     The parsed message to serialize.
 * @param out     Output buffer.
 * @param maxlen  Maximum number of bytes to write.
 * @return        Expected<size_t> with bytes written, or ParseError if buffer too small.
 */
[[nodiscard]] Expected<size_t> writeL3(const ParsedMessage& msg, uint8_t* out, size_t maxlen);

/**
 * Serialize a ParsedMessage to a hex-encoded string.
 *
 * @param msg  The parsed message to serialize.
 * @return     Expected<std::string> with hex encoding, or ParseError on failure.
 */
[[nodiscard]] Expected<std::string> writeL3Hex(const ParsedMessage& msg);

/// Serialize a ParsedMessage to raw bytes (header + body).
/// Returns a std::vector<uint8_t> ready for transmission over LAPDm/PHY.
[[nodiscard]] Expected<std::vector<uint8_t>> writeL3Bytes(const ParsedMessage& msg);

} // namespace gsml3parser
