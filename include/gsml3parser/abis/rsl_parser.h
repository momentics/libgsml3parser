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

/// A-bis RSL message parser with L3 payload extraction.
///
/// Parses raw RSL messages from the BSC, extracts TLV information elements,
/// and retrieves L3 signalling payloads for further processing by the parser.
/// Uses zero heap allocation: all IE pointers reference the original input buffer,
/// and parsed results are returned as value types with fixed-size arrays.
///
/// 3GPP specification: TS 48.058 (A-bis interface RSL protocol).
/// Thread safety: parse() is thread-safe (pure function on input span).
/// Memory: sizeof(RSLParsedMessage) is fixed (~544 bytes on 64-bit), zero heap allocation.
///
/// Example:
/// @code
///   auto result = RSLParser::parse(rawBytes);
///   if (result) {
///       auto& msg = *result;
///       if (RSLParser::hasL3Payload(msg)) {
///           auto l3 = RSLParser::extractL3(msg);
///           // Parse L3 message from l3Payload...
///       }
///   }
/// @endcode
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include "gsml3parser/expected.h"
#include "gsml3parser/abis/rsl_types.h"

namespace gsml3parser {

/// Result of parsing an RSL message. Contains header fields, parsed TLV
/// information elements, and extracted L3 payload (if present).
///
/// All pointers reference the original input buffer — no data is copied.
/// The fixed-size IE array eliminates heap allocation for high-throughput scenarios.
struct RSLParsedMessage {
    RSLDiscriminator discriminator{RSLDiscriminator::RLL};
    uint8_t msgType{0};
    uint8_t chanNr{0};
    uint8_t linkId{0};  ///< LAPDm link identifier (RLL messages only)

    /// Extracted L3 payload bytes (GSM 04.08 message).
    /// Only populated for messages that carry L3 data (DATA_REQ, DATA_IND, BCCH_INFO, ENCR_CMD, etc.).
    std::span<const uint8_t> l3Payload{};

    /// TLV Information Element descriptor. Each IE points into the original buffer.
    struct IE {
        uint8_t type{0};
        uint8_t len{0};
        const uint8_t* val{nullptr};
    };

    static constexpr size_t MAX_IE = 32;
    std::array<IE, MAX_IE> informationElements{};
    size_t ieCount{0};  ///< Number of parsed IEs (<= MAX_IE)

    /// Raw RSL message bytes for debugging and re-serialization.
    std::span<const uint8_t> rawData{};

    /// Return a span over the parsed IEs for range-based iteration.
    [[nodiscard]] std::span<IE> ies() noexcept {
        return {informationElements.data(), ieCount};
    }
    [[nodiscard]] std::span<const IE> ies() const noexcept {
        return {informationElements.data(), ieCount};
    }
};

static_assert(sizeof(RSLParsedMessage::IE) <= 16, "IE must be cache-friendly (<= 16 bytes)");

/// RSL message parser. Decodes raw A-bis RSL frames into structured messages
/// with extracted information elements and L3 payloads.
class RSLParser {
public:
    /// Parse an RSL message from raw bytes.
    /// @param data Raw RSL message bytes (discriminator + msg_type + header + TLV IEs).
    /// Minimum size depends on discriminator: RLL/DCHAN/CCHAN require at least 4 header bytes.
    /// @return Parsed message with discriminator, message type, channel number,
    ///         link ID, extracted L3 payload, and parsed information elements.
    ///         Returns ParseError if the message is truncated or malformed.
    /// 3GPP TS 48.058 - RSL message structure.
    [[nodiscard]] static Expected<RSLParsedMessage> parse(std::span<const uint8_t> data);

    /// Extract L3 payload from a parsed RSL message.
    /// @param parsed Previously parsed RSL message.
    /// @return L3 payload bytes (GSM 04.08 format), or std::nullopt if this message type
    ///         does not carry L3 data.
    [[nodiscard]] static std::optional<std::span<const uint8_t>> extractL3(const RSLParsedMessage& parsed);

    /// Check whether the parsed message carries an L3 payload.
    /// @param parsed Previously parsed RSL message.
    /// @return true if l3Payload is non-empty.
    [[nodiscard]] static bool hasL3Payload(const RSLParsedMessage& parsed) noexcept {
        return !parsed.l3Payload.empty();
    }

    /// Find an information element by type code.
    /// @param parsed Previously parsed RSL message.
    /// @param ieType The IE type code to search for.
    /// @return Pointer to the matching IE, or nullptr if not found.
    [[nodiscard]] static const RSLParsedMessage::IE* findIE(
        const RSLParsedMessage& parsed, RSL_IE ieType) noexcept;

    /// Get Channel Mode from a CHAN_ACTIV message.
    /// @param parsed Parsed CHAN_ACTIV message.
    /// @return Channel mode structure, or std::nullopt if ChanMode IE is missing or too short.
    [[nodiscard]] static std::optional<RSLChannelMode> getChannelMode(
        const RSLParsedMessage& parsed) noexcept;

    /// Get Encryption Info from an ENCR_CMD or CHAN_ACTIV message.
    /// @param parsed Parsed message containing encryption IE.
    /// @return Encryption info (algorithm ID + key span), or std::nullopt if EncrInfo IE missing.
    [[nodiscard]] static std::optional<RSLEncryptionInfo> getEncryptionInfo(
        const RSLParsedMessage& parsed) noexcept;

    /// Return human-readable message name for logging.
    /// @param disc RSL discriminator.
    /// @param msgType Message type byte within the discriminator.
    /// @return Descriptive name string (e.g. "DATA_REQ", "CHAN_ACTIV").
    [[nodiscard]] static std::string_view messageName(RSLDiscriminator disc, uint8_t msgType);
};

} // namespace gsml3parser
