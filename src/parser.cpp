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

#include "gsml3parser/parser.h"
#include "gsml3parser/rr/l3rrmessages.h"
#include "gsml3parser/mm/l3mmmessages.h"
#include "gsml3parser/cc/l3ccmessages.h"
#include "gsml3parser/ss/l3ssmessages.h"
#include "gsml3parser/logger.h"
#include <iomanip>
#include <sstream>

namespace gsml3parser {

// ── Internal: dispatch to domain parsers + custom handlers ──────────────

static ParseResult<std::unique_ptr<L3Message>> parseL3WithPDHandler(
    const L3Frame& frame, const ParserContext& ctx)
{
    if (frame.size() < 16) {
        return ParseResult<std::unique_ptr<L3Message>>(
            ParseErrorCode::TruncatedInput, "Frame too short for L3 header");
    }

    L3PD pd = frame.pd();

    switch (pd) {
        case L3PD::RadioResource: {
            auto res = detail::parseL3RR(frame);
            if (!res.has_value()) return ParseResult<std::unique_ptr<L3Message>>(res.error());
            auto ptr = std::move(res.value());
            return ParseResult<std::unique_ptr<L3Message>>(
                static_cast<std::unique_ptr<L3Message>>(ptr.release()));
        }
        case L3PD::MobilityManagement: {
            auto res = detail::parseL3MM(frame);
            if (!res.has_value()) return ParseResult<std::unique_ptr<L3Message>>(res.error());
            auto ptr = std::move(res.value());
            return ParseResult<std::unique_ptr<L3Message>>(
                static_cast<std::unique_ptr<L3Message>>(ptr.release()));
        }
        case L3PD::CallControl: {
            auto res = detail::parseL3CC(frame);
            if (!res.has_value()) return ParseResult<std::unique_ptr<L3Message>>(res.error());
            auto ptr = std::move(res.value());
            return ParseResult<std::unique_ptr<L3Message>>(
                static_cast<std::unique_ptr<L3Message>>(ptr.release()));
        }
        case L3PD::NonCallSS: {
            auto res = detail::parseL3SupServ(frame);
            if (!res.has_value()) return ParseResult<std::unique_ptr<L3Message>>(res.error());
            auto ptr = std::move(res.value());
            return ParseResult<std::unique_ptr<L3Message>>(
                static_cast<std::unique_ptr<L3Message>>(ptr.release()));
        }
        default:
            break;
    }

    std::optional<PDHandler> handlerOpt = ctx.getPDHandler(pd);
    if (handlerOpt && *handlerOpt) {
        return ParseResult<std::unique_ptr<L3Message>>((*handlerOpt)(frame));
    }

    GSML3PARSER_LOG_WARN("Unsupported PD: 0x{:02x}", static_cast<int>(pd));
    return ParseResult<std::unique_ptr<L3Message>>(
        ParseErrorCode::InvalidPD, "Unsupported protocol discriminator");
}

// ── Context-aware parsers ───────────────────────────────────────────────

ParseResult<std::unique_ptr<L3Message>> parseL3(const L3Frame& frame, const ParserContext& ctx) {
    return parseL3WithPDHandler(frame, ctx);
}

ParseResult<std::unique_ptr<L3Message>> parseL3(std::span<const uint8_t> data, const ParserContext& ctx) {
    if (data.empty()) {
        return ParseResult<std::unique_ptr<L3Message>>(
            ParseErrorCode::TruncatedInput, "Empty input");
    }

    unsigned firstNibble = (data[0] >> 4) & 0x0F;

    if (data.size() == 1 && firstNibble != 6 && firstNibble != 5 && firstNibble != 3 && firstNibble != 0x0B) {
        auto msg = std::make_unique<L3ChannelRequest>(data[0]);
        return ParseResult<std::unique_ptr<L3Message>>(
            std::unique_ptr<L3Message>(static_cast<L3Message*>(msg.release())));
    }
    if (data.size() == 4 && firstNibble != 6 && firstNibble != 5 && firstNibble != 3 && firstNibble != 0x0B) {
        auto msg = std::make_unique<L3HandoverAccess>(data[0]);
        return ParseResult<std::unique_ptr<L3Message>>(
            std::unique_ptr<L3Message>(static_cast<L3Message*>(msg.release())));
    }
    if (data.size() == 7 && firstNibble != 6 && firstNibble != 5 && firstNibble != 3 && firstNibble != 0x0B) {
        L3Frame frame(Primitive::L3_DATA, static_cast<size_t>(data.size()) * 8);
        std::memcpy(frame.data(), data.data(), data.size());
        auto sch = std::make_unique<L3SynchronizationChannelInformation>();
        auto parseRes = sch->parse(frame);
        if (!parseRes.has_value()) {
            return ParseResult<std::unique_ptr<L3Message>>(parseRes.error());
        }
        return ParseResult<std::unique_ptr<L3Message>>(
            std::unique_ptr<L3Message>(static_cast<L3Message*>(sch.release())));
    }

    L3Frame frame(Primitive::L3_DATA, static_cast<size_t>(data.size()) * 8);
    std::memcpy(frame.data(), data.data(), data.size());
    return parseL3WithPDHandler(frame, ctx);
}

ParseResult<std::unique_ptr<L3Message>> parseL3Hex(std::string_view hex, const ParserContext& ctx) {
    if (hex.empty()) {
        return ParseResult<std::unique_ptr<L3Message>>(
            ParseErrorCode::TruncatedInput, "Empty hex string");
    }

    std::string clean;
    clean.reserve(hex.size());
    for (char c : hex) {
        if (std::isxdigit(static_cast<unsigned char>(c))) {
            clean += c;
        }
    }

    if (clean.size() % 2 != 0) {
        return ParseResult<std::unique_ptr<L3Message>>(
            ParseErrorCode::TruncatedInput, "Odd-length hex string");
    }

    std::vector<uint8_t> bytes;
    bytes.reserve(clean.size() / 2);
    for (size_t i = 0; i < clean.size(); i += 2) {
        std::string byteStr = clean.substr(i, 2);
        try {
            bytes.push_back(static_cast<uint8_t>(std::stoi(byteStr, nullptr, 16)));
        } catch (...) {
            return ParseResult<std::unique_ptr<L3Message>>(
                ParseErrorCode::InvalidValue, "Invalid hex character");
        }
    }

    return parseL3(std::span<const uint8_t>(bytes), ctx);
}

// ── Serializers (stateless) ─────────────────────────────────────────────

ParseResult<size_t> writeL3(const L3Message& msg, uint8_t* out, size_t maxlen) {
    if (!out || maxlen < msg.fullLength()) {
        return ParseResult<size_t>(ParseErrorCode::InvalidValue, "Output buffer too small", 0);
    }

    L3Frame frame(Primitive::L3_DATA, msg.bitsNeeded(), SAPI::SAPI3);
    auto res = msg.write(frame);
    if (!res.has_value()) return ParseResult<size_t>(res.error());

    std::memcpy(out, frame.data(), msg.fullLength());
    return msg.fullLength();
}

std::string writeL3Hex(const L3Message& msg) {
    std::vector<uint8_t> buf(msg.fullLength());
    auto res = writeL3(msg, buf.data(), buf.size());
    if (!res.has_value()) return "";

    std::ostringstream os;
    for (const auto& b : buf) {
        os << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
    }
    return os.str();
}

// ── mti2string ──────────────────────────────────────────────────────────

std::string mti2string(L3PD pd, unsigned mti) {
    switch (pd) {
        case L3PD::RadioResource:
            if (mti < 256) return L3RRMessage::name(static_cast<L3RRMessage::MessageType>(mti));
            break;
        case L3PD::MobilityManagement: {
            std::ostringstream os;
            os << static_cast<L3MMMessage::MessageType>(mti);
            return os.str();
        }
        case L3PD::CallControl: {
            std::ostringstream os;
            os << static_cast<L3CCMessage::MessageType>(mti);
            return os.str();
        }
        case L3PD::NonCallSS: {
            std::ostringstream os;
            os << static_cast<L3SupServMessage::MessageType>(mti);
            return os.str();
        }
        default:
            break;
    }
    return "unknown";
}

} // namespace gsml3parser
