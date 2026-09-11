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
#include "gsml3parser/bitreader.h"
#include "gsml3parser/bitwriter.h"
#include "gsml3parser/l3header.h"
#include "gsml3parser/rr/l3rrmessages.h"
#include "gsml3parser/mm/l3mmmessages.h"
#include "gsml3parser/cc/l3ccmessages.h"
#include "gsml3parser/ss/l3ssmessages.h"
#include "gsml3parser/gmm/l3gmmmessages.h"
#include "gsml3parser/sms/l3smsmessages.h"
#include "gsml3parser/sms/l3smsl3messages.h"
#include "gsml3parser/sm/l3smmessages.h"
#include "gsml3parser/bcc/l3bccmessages.h"
#include "gsml3parser/gcc/l3gccmessages.h"
#include "gsml3parser/ls/l3lsmessages.h"
#include "gsml3parser/extended/l3extendedmessages.h"
#include "gsml3parser/testproc/l3testproceduremessages.h"

#include <algorithm>
#include <cstring>

namespace gsml3parser {

// ── Parse dispatch tables ──────────────────────────────
// Replaces the previous per-MTI switch arms (~360 lines) and the
// macro-generated MessageTraits specializations (~285 lines) with
// constexpr function-pointer tables indexed by MTI: O(1) dispatch
// (one .rodata load, same class as a jump table), one generic wrapper
// per parse signature, no macros.
//
// Table size: 256 slots per domain (20 KB total for the 10 fixed-MTI
// domains). Short messages (MTI >= 0x100) get no slot — they are framed
// by length in parseL3() and have no standard-header form (an RR
// TIF=1 header maps to MTI >= 0x100 and yields InvalidMTI, as before).

namespace detail {

template<typename Variant>
using ParseFn = Expected<Variant>(*)(BitReader&, int, unsigned);

/// Parse one concrete message type and wrap it into the domain variant.
/// Handles the three parse signatures present in the catalog:
///  - T::parse(BitReader&, int mti)   runtime-MTI types (Extended, TestProc)
///  - T::parse(BitReader&) + T::ti()  TI-carrying dialogs (CC/SS/BCC/GCC):
///    the TI is set from the L3 header after parsing
///  - T::parse(BitReader&)            everything else
template<typename Variant, typename T>
Expected<Variant> parseInto(BitReader& reader, int mti, unsigned ti) {
    if constexpr (requires { T::parse(reader, mti); }) {
        return T::parse(reader, mti).map([](T v){ return Variant(std::move(v)); });
    } else if constexpr (requires(T v) { v.ti(ti); }) {
        return T::parse(reader).map([ti](T v){ v.ti(ti); return Variant(std::move(v)); });
    } else {
        return T::parse(reader).map([](T v){ return Variant(std::move(v)); });
    }
}

template<typename Variant, typename T>
constexpr void fillParseTableEntry(std::array<ParseFn<Variant>, 256>& table) {
    if constexpr (T::MTI >= 0 && T::MTI < 256) {
        // First alternative wins on MTI overlap: the SMS variant carries
        // both CP-layer (CP-STATUS 0x12, CP-SMT 0x13) and L3-layer
        // (SMSProvidedReplyExpected 0x12, SMSSubmitRep 0x13) messages;
        // the CP alternatives precede the L3 ones in the variant, so the
        // CP parsers must keep their slots — matching the previous
        // switch, where the first case won.
        if (table[static_cast<size_t>(T::MTI)] == nullptr) {
            table[static_cast<size_t>(T::MTI)] = &parseInto<Variant, T>;
        }
    }
    // MTI >= 0x100 (short messages): no slot — see the section comment.
}

template<typename Variant, size_t... I>
constexpr auto makeParseTableImpl(std::index_sequence<I...>) {
    std::array<ParseFn<Variant>, 256> table{};
    (fillParseTableEntry<Variant, std::variant_alternative_t<I, Variant>>(table), ...);
    return table;
}

template<typename Variant>
constexpr auto makeParseTable() {
    return makeParseTableImpl<Variant>(
        std::make_index_sequence<std::variant_size_v<Variant>>{});
}

// One table per fixed-MTI domain (Extended/TestProc are runtime-MTI
// passthrough domains and keep direct one-line parsers below).
constexpr auto kRRParseTable = makeParseTable<RRM>();
constexpr auto kMMParseTable = makeParseTable<MMM>();
constexpr auto kCCParseTable = makeParseTable<CCM>();
constexpr auto kSSParseTable = makeParseTable<SSM>();
constexpr auto kGMMParseTable = makeParseTable<GMM>();
constexpr auto kSMParseTable = makeParseTable<SM>();
constexpr auto kSMSParseTable = makeParseTable<SMS>();
constexpr auto kBCCParseTable = makeParseTable<BCCM>();
constexpr auto kGCCParseTable = makeParseTable<GCCM>();
constexpr auto kLSParseTable = makeParseTable<LSM>();

template<typename Variant>
Expected<Variant> parseFromTable(const std::array<ParseFn<Variant>, 256>& table,
                                 BitReader& reader, int mti, unsigned ti,
                                 std::string_view domainName) {
    if (mti < 0 || mti >= 256 || table[static_cast<size_t>(mti)] == nullptr) {
        // Cold error path: compose "Unknown <domain> MTI" (fits the
        // ParseError 47-char inline buffer; matches the previous
        // switch-default messages).
        std::string msg = "Unknown " + std::string(domainName) + " MTI";
        return Expected<Variant>::error(
            ParseError{ParseError::Code::InvalidMTI, msg, static_cast<size_t>(mti)});
    }
    return table[static_cast<size_t>(mti)](reader, mti, ti);
}

} // namespace detail

// ── Hex decoding helper ────────────────────────────────────────────────

static bool decodeHexPair(const char* p, uint8_t& out) {
    auto val = [](char c) -> unsigned {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return 255;
    };
    unsigned hi = val(p[0]);
    unsigned lo = val(p[1]);
    if (hi > 15 || lo > 15) return false;
    out = static_cast<uint8_t>((hi << 4) | lo);
    return true;
}

// ── L3 header encoding helper ──────────────────────────────────────────

static void encodeL3Header(uint8_t* buf, L3PD pd, int mti, unsigned ti = 0, bool tif = false) {
    switch (pd) {
        case L3PD::RadioResource: {
            buf[0] = static_cast<uint8_t>((static_cast<uint8_t>(pd) & 0x0F) << 4);
            buf[1] = static_cast<uint8_t>(mti & 0xFF);
            break;
        }
        case L3PD::MobilityManagement: {
            buf[0] = static_cast<uint8_t>((static_cast<uint8_t>(pd) & 0x0F) << 4);
            buf[1] = static_cast<uint8_t>(((mti & 0x3F) << 2) | 0);
            break;
        }
        case L3PD::CallControl: {
            buf[0] = static_cast<uint8_t>((static_cast<uint8_t>(pd) & 0x0F) << 4 | (ti & 0x07) << 1);
            if (tif) buf[0] |= 0x01;
            buf[1] = static_cast<uint8_t>(((mti & 0x3F) << 2) | 0);
            break;
        }
        case L3PD::NonCallSS: {
            buf[0] = static_cast<uint8_t>((static_cast<uint8_t>(pd) & 0x0F) << 4 | (ti & 0x07) << 1);
            if (tif) buf[0] |= 0x01;
            buf[1] = static_cast<uint8_t>(((mti & 0x3F) << 2) | 0);
            break;
        }
        case L3PD::GPRSMobilityManagement: {
            buf[0] = static_cast<uint8_t>((static_cast<uint8_t>(pd) & 0x0F) << 4);
            buf[1] = static_cast<uint8_t>(mti & 0xFF);
            break;
        }
        case L3PD::GPRSSessionManagement: {
            buf[0] = static_cast<uint8_t>((static_cast<uint8_t>(pd) & 0x0F) << 4);
            buf[1] = static_cast<uint8_t>(mti & 0xFF);
            break;
        }
        case L3PD::SMS: {
            buf[0] = static_cast<uint8_t>((static_cast<uint8_t>(pd) & 0x0F) << 4);
            buf[1] = static_cast<uint8_t>(mti & 0xFF);
            break;
        }
        case L3PD::BroadcastCallControl: {
            buf[0] = static_cast<uint8_t>((static_cast<uint8_t>(pd) & 0x0F) << 4 | (ti & 0x07) << 1);
            if (tif) buf[0] |= 0x01;
            buf[1] = static_cast<uint8_t>(((mti & 0x3F) << 2) | 0);
            break;
        }
        case L3PD::GroupCallControl: {
            buf[0] = static_cast<uint8_t>((static_cast<uint8_t>(pd) & 0x0F) << 4 | (ti & 0x07) << 1);
            if (tif) buf[0] |= 0x01;
            buf[1] = static_cast<uint8_t>(((mti & 0x3F) << 2) | 0);
            break;
        }
        case L3PD::Location: {
            buf[0] = static_cast<uint8_t>((static_cast<uint8_t>(pd) & 0x0F) << 4);
            buf[1] = static_cast<uint8_t>(mti & 0xFF);
            break;
        }
        case L3PD::Extended: {
            buf[0] = static_cast<uint8_t>((static_cast<uint8_t>(pd) & 0x0F) << 4);
            buf[1] = static_cast<uint8_t>(mti & 0xFF);
            break;
        }
        case L3PD::TestProcedure: {
            buf[0] = static_cast<uint8_t>((static_cast<uint8_t>(pd) & 0x0F) << 4);
            buf[1] = static_cast<uint8_t>(mti & 0xFF);
            break;
        }
        default:
            break;
    }
}

namespace detail {

Expected<RRM> parseL3RR(BitReader& reader, int mti) {
    return parseFromTable(kRRParseTable, reader, mti, 0, "RR");
}

Expected<MMM> parseL3MM(BitReader& reader, int mti) {
    return parseFromTable(kMMParseTable, reader, mti, 0, "MM");
}

Expected<CCM> parseL3CC(BitReader& reader, int mti, unsigned ti) {
    return parseFromTable(kCCParseTable, reader, mti, ti, "CC");
}

Expected<SSM> parseL3SS(BitReader& reader, int mti, unsigned ti) {
    return parseFromTable(kSSParseTable, reader, mti, ti, "SS");
}

// GMM messages (24.008 Table 10.4)
Expected<GMM> parseL3GMM(BitReader& reader, int mti) {
    return parseFromTable(kGMMParseTable, reader, mti, 0, "GMM");
}

// SM messages (24.008 Table 10.4a)
Expected<SM> parseL3SM(BitReader& reader, int mti) {
    return parseFromTable(kSMParseTable, reader, mti, 0, "SM");
}

// SMS messages (24.008 Table 10.6a, 24.011 sections 7-8)
// Note: MTI 0x12 and 0x13 overlap between CP-layer and L3-layer messages.
// CP-STATUS(0x12) vs SMSProvidedReplyExpected(0x12), CP-SMT(0x13) vs SMSSubmitRep(0x13).
// For backward compatibility, overlapping MTIs dispatch to CP messages.
Expected<SMS> parseL3SMS(BitReader& reader, int mti) {
    return parseFromTable(kSMSParseTable, reader, mti, 0, "SMS");
}

// BCC messages (44.018 Table 10.4.3)
Expected<BCCM> parseL3BCC(BitReader& reader, int mti, unsigned ti) {
    return parseFromTable(kBCCParseTable, reader, mti, ti, "BCC");
}

// GCC messages (44.018 Table 10.4.4)
Expected<GCCM> parseL3GCC(BitReader& reader, int mti, unsigned ti) {
    return parseFromTable(kGCCParseTable, reader, mti, ti, "GCC");
}

// LS messages (TS 44.031)
Expected<LSM> parseL3LS(BitReader& reader, int mti) {
    return parseFromTable(kLSParseTable, reader, mti, 0, "LS");
}

// Extended PD messages (GSM 04.08 §10.2, PD=0x0e)
Expected<EXTENDED> parseL3Extended(BitReader& reader, uint8_t mti) {
    return L3ExtendedMessage::parse(reader, mti).map([](L3ExtendedMessage v){ return EXTENDED(std::move(v)); });
}

// TestProcedure PD messages (GSM 04.08 §10.2, PD=0x0f)
Expected<TESTPROC> parseL3TestProc(BitReader& reader, uint8_t mti) {
    return L3TestProcedureMessage::parse(reader, mti).map([](L3TestProcedureMessage v){ return TESTPROC(std::move(v)); });
}

/// Parse the body of a standard-header message: the 12-domain switch.
/// Shared by the main parseL3 path and by the 4/7-byte short-message
/// disambiguation (which additionally requires exact frame consumption
/// .
[[nodiscard]] Expected<ParsedMessage> parseStandardBody(const L3Header& hdr, BitReader& reader) {
    switch (hdr.pd) {
        case L3PD::RadioResource: {
            auto rrRes = parseL3RR(reader, hdr.mti);
            if (rrRes) return rrRes.map([](RRM v){ return ParsedMessage(std::move(v)); });
            // Truncated body (e.g. incomplete SI message) is a hard error:
            // never fabricate a default-constructed message (C11).
            return Expected<ParsedMessage>::error(rrRes.error());
        }
        case L3PD::MobilityManagement:
            return parseL3MM(reader, hdr.mti)
                .map([](MMM v){ return ParsedMessage(std::move(v)); });
        case L3PD::CallControl:
            return parseL3CC(reader, hdr.mti, hdr.ti)
                .map([](CCM v){ return ParsedMessage(std::move(v)); });
        case L3PD::NonCallSS:
            return parseL3SS(reader, hdr.mti, hdr.ti)
                .map([](SSM v){ return ParsedMessage(std::move(v)); });
        case L3PD::GPRSMobilityManagement:
            return parseL3GMM(reader, hdr.mti)
                .map([](GMM v){ return ParsedMessage(std::move(v)); });
        case L3PD::GPRSSessionManagement:
            return parseL3SM(reader, hdr.mti)
                .map([](SM v){ return ParsedMessage(std::move(v)); });
        case L3PD::SMS:
            return parseL3SMS(reader, hdr.mti)
                .map([](SMS v){ return ParsedMessage(std::move(v)); });
        case L3PD::BroadcastCallControl:
            return parseL3BCC(reader, hdr.mti, hdr.ti)
                .map([](BCCM v){ return ParsedMessage(std::move(v)); });
        case L3PD::GroupCallControl:
            return parseL3GCC(reader, hdr.mti, hdr.ti)
                .map([](GCCM v){ return ParsedMessage(std::move(v)); });
        case L3PD::Location:
            return parseL3LS(reader, hdr.mti)
                .map([](LSM v){ return ParsedMessage(std::move(v)); });
        case L3PD::Extended:
            return parseL3Extended(reader, static_cast<uint8_t>(hdr.mti))
                .map([](EXTENDED v){ return ParsedMessage(std::move(v)); });
        case L3PD::TestProcedure:
            return parseL3TestProc(reader, static_cast<uint8_t>(hdr.mti))
                .map([](TESTPROC v){ return ParsedMessage(std::move(v)); });
        case L3PD::Undefined:
            // Spelled out explicitly so the switch covers the whole L3PD
            // domain (-Wswitch); parseL3Header rejects it upstream anyway.
            break;
    }
    // Unreachable: parseL3Header only accepts the 12 defined PD values
    // (reserved PDs are rejected).
    return Expected<ParsedMessage>::error(
        {ParseError::Code::InvalidPD, "Unsupported Protocol Discriminator"});
}

} // namespace detail

Expected<ParsedMessage> parseL3(std::span<const uint8_t> data, const ParserConfig& cfg) {
    (void)cfg;   // Reserved for future parser options (e.g. log level).
    if (data.empty()) {
        return Expected<ParsedMessage>::error(
            ParseError{ParseError::Code::TruncatedInput, "Empty input"});
    }

    // ── Short messages (no standard 2-octet L3 header) ─────────────────
    /// GSM 04.08 defines RR short messages that carry no standard L3 header,
    /// so they are disambiguated by total frame length:
    ///   - 1 byte  -> ChannelRequest (TS 44.018 9.1.8)
    ///   - 4 bytes -> HandoverAccess (GSM 04.08 9.1.38)
    ///   - 7 bytes -> SynchronizationChannelInformation (GSM 04.08 9.1.39)
    ///
    /// Disambiguation for 4/7-byte frames: the standard-header
    /// parse wins only when it consumes the frame EXACTLY
    /// (remainingBits() == 0). A standard parse that leaves trailing bytes
    /// means the frame is a short message whose first octet merely looks
    /// like a plausible header. Short-message handlers are tried for any
    /// first-octet value, because a short message's first octet is not an
    /// L3 header at all.
    if (data.size() == 1) {
        // A one-octet frame can only be a Channel Request: every
        // standard-header L3 message is at least two octets long, and the
        // RACH carries exactly this one-octet message. The full octet is
        // the 8-bit request reference (RA); any of the 256 values is
        // valid, so no nibble filtering is applied (the previous
        // heuristic rejected 192 of 256 legitimate RA values).
        BitReader reader(data.data(), 8);
        auto res = L3ChannelRequest::parse(reader);
        return std::move(res).map([](L3ChannelRequest v){ return ParsedMessage(RRM(std::move(v))); });
    }

    // For 4-byte and 7-byte data: handle short messages (no standard L3 header).
    // HandoverAccess is 4 bytes, SynchronizationChannelInformation is 7 bytes.
    // These are RR short messages but their first byte's high nibble may match
    // any PD value (including BCC=0x01, GCC=0x00), since they have no standard header.
    if (data.size() == 4 || data.size() == 7) {
        uint8_t pdNibble = (data[0] >> 4) & 0x0F;

        // BCC/GCC PD: short messages win. BCC Setup (MTI 0x00) and
        // BCC Proceeding (MTI 0x01) have opaque bodies that consume the
        // whole frame, so a standard parse would always "succeed exactly"
        // and swallow genuine HandoverAccess/SynchronizationChannelInformation
        // frames (golden vectors {0x17, 0x00, ...} and {0x12, 0x34, ...}
        // pin this behavior).
        if (pdNibble == static_cast<uint8_t>(L3PD::BroadcastCallControl) ||
            pdNibble == static_cast<uint8_t>(L3PD::GroupCallControl)) {
            if (data.size() == 4) {
                BitReader reader(data.data(), 32);
                auto res = L3HandoverAccess::parse(reader);
                if (res) return res.map([](L3HandoverAccess v){ return ParsedMessage(RRM(std::move(v))); });
            }
            if (data.size() == 7) {
                BitReader reader(data.data(), 56);
                auto res = L3SynchronizationChannelInformation::parse(reader);
                if (res) return res.map([](L3SynchronizationChannelInformation v){ return ParsedMessage(RRM(std::move(v))); });
            }
        } else {
            // Other PDs: the standard parse wins only on EXACT
            // consumption. A standard parse that leaves trailing bytes
            // means the frame is a short message whose first octet merely
            // looks like a plausible header (e.g. HandoverAccess
            // {0x60, 0x12, ..} used to be misparsed as RR Status with the
            // trailing byte silently dropped).
            auto hdrResult = parseL3Header(data);
            if (hdrResult) {
                size_t bodyBits = (data.size() - 2) * 8;
                BitReader reader(data.data() + 2, bodyBits);
                auto stdRes = detail::parseStandardBody(hdrResult.value(), reader);
                if (stdRes && reader.remainingBits() == 0) {
                    return stdRes;
                }
            }

            // Short messages: HandoverAccess (4 bytes) / Synchronization
            // Channel Information (7 bytes). Their first octet is not an
            // L3 header, so they are tried for ANY first-octet value,
            // including reserved PD nibbles (rejected by parseL3Header).
            // Both parsers consume the whole frame, so success is
            // unambiguous.
            if (data.size() == 4) {
                BitReader reader(data.data(), 32);
                auto res = L3HandoverAccess::parse(reader);
                if (res) return res.map([](L3HandoverAccess v){ return ParsedMessage(RRM(std::move(v))); });
            }
            if (data.size() == 7) {
                BitReader reader(data.data(), 56);
                auto res = L3SynchronizationChannelInformation::parse(reader);
                if (res) return res.map([](L3SynchronizationChannelInformation v){ return ParsedMessage(RRM(std::move(v))); });
            }
        }
        // Defensive fall-through: a 4/7-byte frame whose short parse
        // unexpectedly failed (impossible today: both short parsers always
        // succeed on full-length input) reaches the standard parse below,
        // which returns a proper error.
        // Note: L3HandoverAccess::parse can now fail on
        // non-zero reserved bits — such a 4-byte frame falls through to
        // the standard parse below and produces a proper error instead of
        // a fake HandoverAccess.
    }

    // Standard L3 header parsing.
    auto hdrResult = parseL3Header(data);
    if (!hdrResult) {
        return Expected<ParsedMessage>::error(hdrResult.error());
    }
    size_t bodyBits = (data.size() - 2) * 8;
    BitReader reader(data.data() + 2, bodyBits);
    auto res = detail::parseStandardBody(hdrResult.value(), reader);
    if (res && cfg.requireFullConsumption && reader.remainingBits() != 0) {
        // Strict framing: trailing bytes after a complete message mean the
        // frame boundary was wrong.
        return Expected<ParsedMessage>::error(
            {ParseError::Code::LengthMismatch, "trailing data after L3 message"});
    }
    return res;
}

Expected<ParsedMessage> parseL3Hex(std::string_view hex, const ParserConfig& cfg) {
    std::string cleaned;
    cleaned.reserve(hex.size());
    for (char c : hex) {
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n') {
            cleaned.push_back(c);
        }
    }

    if (cleaned.empty()) {
        return Expected<ParsedMessage>::error(
            ParseError{ParseError::Code::TruncatedInput, "Empty input"});
    }
    if (cleaned.size() % 2 != 0) {
        return Expected<ParsedMessage>::error(
            ParseError{ParseError::Code::InvalidValue, "Odd-length hex string"});
    }

    size_t byteCount = cleaned.size() / 2;
    std::vector<uint8_t> bytes(byteCount);
    for (size_t i = 0; i < byteCount; ++i) {
        if (!decodeHexPair(cleaned.data() + i * 2, bytes[i])) {
            return Expected<ParsedMessage>::error(
                ParseError{ParseError::Code::InvalidValue, "Invalid hex character"});
        }
    }

    return parseL3(std::span<const uint8_t>(bytes), cfg);
}

namespace detail {

// ── Write path ───────────────────────────────────────────
// domainPd<Variant>() replaces the previous MessageTraits specializations
// (which served both parse and write dispatch): 12 one-line mappings.
template<typename V> constexpr L3PD domainPd();
template<> constexpr L3PD domainPd<RRM>() { return L3PD::RadioResource; }
template<> constexpr L3PD domainPd<MMM>() { return L3PD::MobilityManagement; }
template<> constexpr L3PD domainPd<CCM>() { return L3PD::CallControl; }
template<> constexpr L3PD domainPd<SSM>() { return L3PD::NonCallSS; }
template<> constexpr L3PD domainPd<GMM>() { return L3PD::GPRSMobilityManagement; }
template<> constexpr L3PD domainPd<SM>() { return L3PD::GPRSSessionManagement; }
template<> constexpr L3PD domainPd<SMS>() { return L3PD::SMS; }
template<> constexpr L3PD domainPd<BCCM>() { return L3PD::BroadcastCallControl; }
template<> constexpr L3PD domainPd<GCCM>() { return L3PD::GroupCallControl; }
template<> constexpr L3PD domainPd<LSM>() { return L3PD::Location; }
template<> constexpr L3PD domainPd<EXTENDED>() { return L3PD::Extended; }
template<> constexpr L3PD domainPd<TESTPROC>() { return L3PD::TestProcedure; }

template<L3PD pd, typename ConcreteMsg>
Expected<size_t> writeL3Body(const ConcreteMsg& msg, uint8_t* out, size_t maxlen) {
    int mtiVal = ConcreteMsg::MTI;

    // For messages with runtime-determined MTI (Extended, TestProcedure), override.
    if constexpr (requires { msg.mti(); }) {
        mtiVal = static_cast<int>(msg.mti());
    }

    // Short messages: no standard L3 header, body only.
    if constexpr (ConcreteMsg::MTI >= 0x100) {
        size_t bodyLen = msg.bodyLength();
        if (bodyLen > maxlen) return Expected<size_t>::error(
            ParseError{ParseError::Code::InvalidValue, "Buffer too small"});
        BitWriter writer(out, bodyLen * 8);
        msg.write(writer);
        return Expected<size_t>::hold(bodyLen);
    } else {
        // Standard messages: L3 header + body.
        unsigned ti = 0;
        bool tif = false;

        if constexpr (requires { msg.ti(); }) {
            ti = msg.ti();
        }

        size_t bodyLen = msg.bodyLength();
        size_t totalLen = 2 + bodyLen;
        if (totalLen > maxlen) return Expected<size_t>::error(
            ParseError{ParseError::Code::InvalidValue, "Buffer too small"});

        encodeL3Header(out, pd, mtiVal, ti, tif);

        BitWriter writer(out + 2, bodyLen * 8);
        msg.write(writer);

        return Expected<size_t>::hold(totalLen);
    }
}

/// Exact wire length of a message: 2-byte header + body for standard
/// messages, body only for short messages (MTI >= 0x100). Used to size
/// the output container exactly (the previous 4 KB stack
/// buffer is gone).
template<typename ConcreteMsg>
constexpr size_t wireLength(const ConcreteMsg& msg) noexcept {
    if constexpr (ConcreteMsg::MTI >= 0x100) return msg.bodyLength();
    else return 2 + msg.bodyLength();
}

template<typename Msg>
size_t messageWireLength(const Msg& msg) noexcept {
    return std::visit([](const auto& domainVariant) -> size_t {
        return std::visit([](const auto& m) -> size_t {
            return wireLength<std::decay_t<decltype(m)>>(m);
        }, domainVariant);
    }, msg);
}

} // namespace detail

Expected<size_t> writeL3(const ParsedMessage& msg, uint8_t* out, size_t maxlen) {
    return std::visit([out, maxlen](const auto& domainVariant) -> Expected<size_t> {
        using DomainV = std::decay_t<decltype(domainVariant)>;
        constexpr L3PD pd = detail::domainPd<DomainV>();
        return std::visit([out, maxlen, pd](const auto& concreteMsg) -> Expected<size_t> {
            using MsgType = std::decay_t<decltype(concreteMsg)>;
            return detail::writeL3Body<pd, MsgType>(concreteMsg, out, maxlen);
        }, domainVariant);
    }, msg);
}

Expected<std::string> writeL3Hex(const ParsedMessage& msg) {
    // Exact-size output: the wire length is known up front
    // via bodyLength(), so no oversized stack buffer is needed.
    size_t n = detail::messageWireLength(msg);
    std::vector<uint8_t> buf(n);
    auto szResult = writeL3(msg, buf.data(), buf.size());
    if (!szResult) return Expected<std::string>::error(szResult.error());
    n = szResult.value();

    static constexpr char hexDigits[] = "0123456789abcdef";
    std::string result;
    result.resize(n * 2);
    for (size_t i = 0; i < n; ++i) {
        result[i * 2]     = hexDigits[buf[i] >> 4];
        result[i * 2 + 1] = hexDigits[buf[i] & 0x0F];
    }
    return Expected<std::string>::hold(std::move(result));
}

Expected<std::vector<uint8_t>> writeL3Bytes(const ParsedMessage& msg) {
    // Exact-size output: sized by bodyLength() up front,
    // written once — no 4 KB stack buffer, no second copy.
    std::vector<uint8_t> buf(detail::messageWireLength(msg));
    auto szResult = writeL3(msg, buf.data(), buf.size());
    if (!szResult) return Expected<std::vector<uint8_t>>::error(szResult.error());
    buf.resize(szResult.value());
    return Expected<std::vector<uint8_t>>::hold(std::move(buf));
}

} // namespace gsml3parser
