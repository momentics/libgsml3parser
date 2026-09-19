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

/// C ABI wrapper (gsml3parser_c.h) implementation.
///
/// Every public function is wrapped so no C++ exception can cross the C
/// boundary; errors are reported via the return value and the thread-local
/// gsml3_last_error() message. Handles own their state (the opaque struct
/// definitions live here, the header only declares them). All char*
/// pointers returned to the caller are allocated with `new char[]` and
/// released with gsml3_free().

#include <gsml3parser/gsml3parser_c.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <new>
#include <optional>
#include <span>
#include <string>
#include <variant>

#include "gsml3parser/message_types.h"
#include "gsml3parser/parser.h"
#include "gsml3parser/parser_config.h"
#include "gsml3parser/visitor.h"
#include "gsml3parser/abis/rsl_parser.h"
#include "gsml3parser/abis/rsl_builder.h"
#include "gsml3parser/lapdm_frame.h"
#include "gsml3parser/lapdm_entity.h"
#include "gsml3parser/stack/subscriber_registry.h"
#include "gsml3parser/stack/ms_context.h"
#include "gsml3parser/stack/transaction.h"
#include "gsml3parser/stack/procedure_orchestrator.h"
#include "gsml3parser/stack/response_builder.h"
#include "gsml3parser/stack/typed_external_data.h"

#include <cstdio>
#include "gsml3parser/common/l3common.h"
#include "gsml3parser/rr/l3rrmessages.h"
#include "gsml3parser/mm/l3mmmessages.h"
#include "gsml3parser/cc/l3ccmessages.h"
#include "gsml3parser/ss/l3ssmessages.h"
#include "gsml3parser/sms/l3smsmessages.h"
#include "gsml3parser/sms/l3smsl3messages.h"

namespace {

using namespace gsml3parser;

// Thread-local last error: message + machine-readable code. Both are valid
// until a later call clears or replaces them; callers that need the string
// longer must copy it.
thread_local std::string tLastError;
thread_local int tLastErrorCode{GSML3_OK};

void setError(int code, const char* msg) {
    tLastErrorCode = code;
    tLastError = msg ? msg : "";
}

inline void clearLastError() {
    tLastErrorCode = GSML3_OK;
    tLastError.clear();
}

// Default-class error (argument/value problems); the specific helpers below
// override the class for OOM, buffer-too-small and internal failures.
inline void setLastError(const char* msg) { setError(GSML3_ERR_INVALID_VALUE, msg); }

inline void setOomError() { setError(GSML3_ERR_NO_MEMORY, "out of memory"); }

inline void setBufferTooSmallError() {
    setError(GSML3_ERR_BUFFER_TOO_SMALL, "output buffer too small");
}

// Used by catch (...) guards: an exception escaped from a noexcept-expected
// C++ path; the library remains functional but the caller should treat the
// affected handle as suspect.
inline void setLastErrorUnexpected(const char* msg) {
    setError(GSML3_ERR_INTERNAL, msg);
}

int mapParseError(const ParseError& e);

void reportParseError(const ParseError& e) {
    setError(mapParseError(e), std::string(e.message).c_str());
}

int mapParseError(const ParseError& e) {
    switch (e.code) {
        case ParseError::Code::TruncatedInput:     return GSML3_ERR_TRUNCATED;
        case ParseError::Code::InvalidPD:          return GSML3_ERR_INVALID_PD;
        case ParseError::Code::InvalidMTI:         return GSML3_ERR_INVALID_MTI;
        case ParseError::Code::LengthMismatch:     return GSML3_ERR_LENGTH_MISMATCH;
        case ParseError::Code::InvalidIE:          return GSML3_ERR_INVALID_IE;
        case ParseError::Code::InvalidValue:       return GSML3_ERR_INVALID_VALUE;
        case ParseError::Code::UnsupportedFeature: return GSML3_ERR_UNSUPPORTED;
        case ParseError::Code::SourceExhausted:    return GSML3_ERR_SOURCE_EXHAUSTED;
        case ParseError::Code::BufferTooSmall:     return GSML3_ERR_BUFFER_TOO_SMALL;
        default:                                   return GSML3_ERR_INVALID_VALUE;
    }
}

// ── Input validation (boundary of the C API) ────────────────────────────

// Range check for a parameter that mirrors a C++ enum value. The bounds are
// derived from the enum's own enumerators so a future core change is caught
// by this function's call sites, not by runtime surprises on the wire.
inline bool checkEnumValue(int v, int lo, int hi, const char* what) {
    if (v < lo || v > hi) {
        char msg[96];
        std::snprintf(msg, sizeof(msg), "invalid %s value %d (expected %d..%d)",
                      what, v, lo, hi);
        setError(GSML3_ERR_INVALID_ARG, msg);
        return false;
    }
    return true;
}

// Validation of BCD digit strings accepted from the caller: ASCII digits
// only (plus one optional leading '+'), length in [minLen, maxLen]. The
// C++ storage truncates silently; failing fast here keeps the wire format
// honest.
inline bool checkDigitString(const char* s, size_t minLen, size_t maxLen,
                             bool plusAllowed, const char* what) {
    if (!s) {
        setError(GSML3_ERR_INVALID_ARG, (std::string("NULL ") + what).c_str());
        return false;
    }
    const char* p = s;
    if (*p == '+') {
        if (!plusAllowed) {
            setError(GSML3_ERR_INVALID_ARG,
                     (std::string(what) + " must not start with '+'").c_str());
            return false;
        }
        ++p;
    }
    size_t n = 0;
    while (*p != '\0') {
        if (*p < '0' || *p > '9') {
            setError(GSML3_ERR_INVALID_ARG,
                     (std::string(what) + " must contain ASCII digits only").c_str());
            return false;
        }
        ++p;
        ++n;
    }
    if (n < minLen || n > maxLen) {
        char msg[96];
        std::snprintf(msg, sizeof(msg), "invalid %s length %zu (expected %d..%d digits)",
                      what, n, static_cast<int>(minLen), static_cast<int>(maxLen));
        setError(GSML3_ERR_INVALID_ARG, msg);
        return false;
    }
    return true;
}

// ── Valid value domains (derived from the mirrored C++ enums) ──────────
// Keep in sync with include/gsml3parser/{enums,types,rsl_types}.h. The low
// and high ends reference the enums themselves where that is meaningful so
// the ranges cannot drift from the core definitions.

namespace ranges {
inline constexpr int rrCauseLo    = 0;  // RRCause::Normal_Event
inline constexpr int rrCauseHi    = static_cast<int>(RRCause::Protocol_Error_Unspecified);
inline constexpr int mmCauseLo    = 0;  // MMRejectCause::Zero
inline constexpr int mmCauseHi    = static_cast<int>(MMRejectCause::Protocol_Error_Unspecified);
inline constexpr int cmAbortLo    = static_cast<int>(CMServiceAbortCause::Unspecified);
inline constexpr int cmAbortHi    = static_cast<int>(CMServiceAbortCause::SemanticInconsistency);
inline constexpr int ccCauseLo    = 0;  // CCCause::Unknown_L3_Cause
inline constexpr int ccCauseHi    = static_cast<int>(CCCause::Interworking_Unspecified);
inline constexpr int idTypeLo     = static_cast<int>(MobileIDType::NoID);
inline constexpr int idTypeHi     = static_cast<int>(MobileIDType::TMSI);
inline constexpr int typeOffsetLo = 0;  // TypeAndOffset::TDMA_SACCH
inline constexpr int typeOffsetHi = static_cast<int>(TDMA_MISC);
inline constexpr int chanTypeLo   = static_cast<int>(ChannelType::SCHType);
inline constexpr int chanTypeHi   = static_cast<int>(ChannelType::UndefinedCHType);
inline constexpr int tokenLo      = 0;  // ResponseToken::None
inline constexpr int tokenHi      = static_cast<int>(ResponseToken::Setup);
inline constexpr int rslCauseLo   = static_cast<int>(RSLErrorCause::NormalUnspecified);
inline constexpr int rslCauseHi   = static_cast<int>(RSLErrorCause::EncryptionUnimplemented);
inline constexpr int sapiLo       = 0;
inline constexpr int sapiHi       = 15;
inline constexpr int serviceTypeLo = 0;  // L3CMServiceType::TypeCode::UndefinedType
inline constexpr int serviceTypeHi = static_cast<int>(L3CMServiceType::TypeCode::LocationUpdateRequest);
inline constexpr int updateTypeLo = 0;   // Location updating type: Normal / Periodic / IMSI Attach
inline constexpr int updateTypeHi = 2;
inline constexpr int timerLo      = static_cast<int>(L3TimerId::T3101);
inline constexpr int timerHi      = static_cast<int>(L3TimerId::T3395);
} // namespace ranges

} // namespace

// ── Opaque handle definitions (declared in gsml3parser_c.h) ────────────

struct gsml3_config  { ParserConfig cfg; };
struct gsml3_message { ParsedMessage msg; };

// ── Version / error model / free ───────────────────────────────────────

#ifdef GSML3PARSER_VERSION
#define GSML3PARSER_VERSION_STRING GSML3PARSER_VERSION
#else
#  error GSML3PARSER_VERSION must be defined when building libgsml3parser (CMakeLists.txt sets it)
#endif

GSML3_C_API const char* gsml3_version(void) {
    return GSML3PARSER_VERSION_STRING;
}

GSML3_C_API unsigned gsml3_abi_version(void) {
    return GSML3_ABI_VERSION;
}

GSML3_C_API const char* gsml3_last_error(void) {
    return tLastError.c_str();
}

GSML3_C_API int gsml3_last_error_code(void) {
    return tLastErrorCode;
}

GSML3_C_API void gsml3_free(void* ptr) {
    delete[] static_cast<char*>(ptr);
}

// ── Parser config ──────────────────────────────────────────────────────

GSML3_C_API gsml3_config* gsml3_config_new(void) {
    clearLastError();
    auto* c = new (std::nothrow) gsml3_config{};
    if (!c) setOomError();
    return c;
}

GSML3_C_API void gsml3_config_set_log_level(gsml3_config* c, int level) {
    if (c && level >= GSML3_LOG_EMERG && level <= GSML3_LOG_DEBUG)
        c->cfg.logLevel = static_cast<LogLevel>(level);
}

GSML3_C_API void gsml3_config_set_strict_framing(gsml3_config* c, int on) {
    if (c) c->cfg.requireFullConsumption = (on != 0);
}

GSML3_C_API void gsml3_config_free(gsml3_config* c) {
    delete c;
}

// ── L3 message handle ──────────────────────────────────────────────────

GSML3_C_API gsml3_message* gsml3_parse_l3(const uint8_t* data, size_t len,
                                          const gsml3_config* cfg) {
    try {
        clearLastError();
        if (!data || len == 0) { setLastError("NULL or empty input"); return nullptr; }
        auto r = parseL3({data, len}, cfg ? cfg->cfg : ParserConfig{});
        if (!r) { reportParseError(r.error()); return nullptr; }
        auto* m = new (std::nothrow) gsml3_message{std::move(r.value())};
        if (!m) setOomError();
        return m;
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_parse_l3");
        return nullptr;
    }
}

GSML3_C_API gsml3_message* gsml3_parse_l3_hex(const char* hex,
                                              const gsml3_config* cfg) {
    try {
        clearLastError();
        if (!hex) { setLastError("NULL hex string"); return nullptr; }
        auto r = parseL3Hex(hex, cfg ? cfg->cfg : ParserConfig{});
        if (!r) { reportParseError(r.error()); return nullptr; }
        auto* m = new (std::nothrow) gsml3_message{std::move(r.value())};
        if (!m) setOomError();
        return m;
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_parse_l3_hex");
        return nullptr;
    }
}

GSML3_C_API int gsml3_parse_l3_into(gsml3_message* msg, const uint8_t* data,
                                    size_t len, const gsml3_config* cfg) {
    try {
        clearLastError();
        if (!msg || !data || len == 0) {
            setError(GSML3_ERR_INVALID_ARG, "NULL handle or empty input");
            return GSML3_ERR_INVALID_ARG;
        }
        auto r = parseL3({data, len}, cfg ? cfg->cfg : ParserConfig{});
        if (!r) {
            // On error the handle keeps its previous content.
            reportParseError(r.error());
            return mapParseError(r.error());
        }
        msg->msg = std::move(r.value());
        return GSML3_OK;
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_parse_l3_into");
        return GSML3_ERR_INTERNAL;
    }
}

GSML3_C_API void gsml3_message_free(gsml3_message* msg) {
    delete msg;
}

GSML3_C_API const char* gsml3_message_name(const gsml3_message* msg) {
    if (!msg) return "";
    return messageName(msg->msg).data();
}

GSML3_C_API int gsml3_message_pd(const gsml3_message* msg) {
    if (!msg) return -1;
    return static_cast<int>(messagePD(msg->msg));
}

GSML3_C_API int gsml3_message_mti(const gsml3_message* msg) {
    if (!msg) return -1;
    return messageMTI(msg->msg);
}

GSML3_C_API int gsml3_message_ti(const gsml3_message* msg) {
    if (!msg) return 0;
    return messageTI(msg->msg);
}

GSML3_C_API size_t gsml3_message_size(const gsml3_message* msg) {
    if (!msg) return 0;
    try {
        return messageWireLength(msg->msg);
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_message_size");
        return 0;
    }
}

GSML3_C_API size_t gsml3_message_write(const gsml3_message* msg,
                                       uint8_t* out, size_t maxlen) {
    try {
        clearLastError();
        if (!msg || !out || maxlen == 0) {
            setLastError("NULL handle or output buffer");
            return 0;
        }
        auto r = writeL3(msg->msg, out, maxlen);
        if (!r) { reportParseError(r.error()); return 0; }
        return r.value();
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_message_write");
        return 0;
    }
}

GSML3_C_API char* gsml3_message_hex(const gsml3_message* msg) {
    try {
        clearLastError();
        if (!msg) { setLastError("NULL handle"); return nullptr; }
        auto r = writeL3Hex(msg->msg);
        if (!r) { reportParseError(r.error()); return nullptr; }
        const std::string& s = r.value();
        char* p = new (std::nothrow) char[s.size() + 1];
        if (!p) { setOomError(); return nullptr; }
        std::memcpy(p, s.data(), s.size());
        p[s.size()] = '\0';
        return p;
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_message_hex");
        return nullptr;
    }
}

GSML3_C_API char* gsml3_message_dump(const gsml3_message* msg) {
    try {
        clearLastError();
        if (!msg) { setLastError("NULL handle"); return nullptr; }
        const std::string s = messageText(msg->msg);
        char* p = new (std::nothrow) char[s.size() + 1];
        if (!p) { setOomError(); return nullptr; }
        std::memcpy(p, s.data(), s.size());
        p[s.size()] = '\0';
        return p;
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_message_dump");
        return nullptr;
    }
}

// ── RSL (A-bis, TS 48.058) ─────────────────────────────────────────────

// The handle owns a copy of the input: RSLParsedMessage holds spans into
// that copy (IE values, L3 payload), so the caller's buffer may be freed
// right after gsml3_rsl_parse returns.
struct gsml3_rsl {
    std::vector<uint8_t> buffer;
    RSLParsedMessage parsed;
};

GSML3_C_API gsml3_rsl* gsml3_rsl_parse(const uint8_t* data, size_t len) {
    if (!data || len == 0) { setLastError("NULL or empty input"); return nullptr; }
    clearLastError();
    std::unique_ptr<gsml3_rsl> h(new (std::nothrow) gsml3_rsl{});
    if (!h) { setOomError(); return nullptr; }
    try {
        auto r = RSLParser::parse({data, len});
        if (!r) { reportParseError(r.error()); return nullptr; }
        h->buffer.assign(data, data + len);
        h->parsed = std::move(r.value());
        // Point the spans at the owned copy (parse() filled them with
        // views into the caller's buffer).
        if (h->parsed.l3Payload.data())
            h->parsed.l3Payload = {h->buffer.data() + (h->parsed.l3Payload.data() - data),
                                   h->parsed.l3Payload.size()};
        for (auto& ie : h->parsed.informationElements) {
            if (ie.val) ie.val = h->buffer.data() + (ie.val - data);
        }
        if (h->parsed.rawData.data())
            h->parsed.rawData = {h->buffer.data() + (h->parsed.rawData.data() - data),
                                  h->parsed.rawData.size()};
        return h.release();
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_rsl_parse");
        return nullptr;
    }
}

GSML3_C_API void gsml3_rsl_free(gsml3_rsl* rsl) {
    delete rsl;
}

GSML3_C_API const char* gsml3_rsl_name(const gsml3_rsl* rsl) {
    if (!rsl) return "";
    return RSLParser::messageName(rsl->parsed.discriminator, rsl->parsed.msgType).data();
}

GSML3_C_API int gsml3_rsl_discriminator(const gsml3_rsl* rsl) {
    if (!rsl) return -1;
    return static_cast<int>(rsl->parsed.discriminator);
}

GSML3_C_API int gsml3_rsl_msg_type(const gsml3_rsl* rsl) {
    if (!rsl) return -1;
    return rsl->parsed.msgType;
}

GSML3_C_API int gsml3_rsl_chan_nr(const gsml3_rsl* rsl) {
    if (!rsl) return -1;
    return rsl->parsed.chanNr;
}

GSML3_C_API int gsml3_rsl_link_id(const gsml3_rsl* rsl) {
    if (!rsl) return -1;
    return rsl->parsed.linkId;
}

GSML3_C_API int gsml3_rsl_bts_to_bsc(const gsml3_rsl* rsl) {
    if (!rsl) return -1;
    return rsl->parsed.btsToBsc ? 1 : 0;
}

GSML3_C_API int gsml3_rsl_has_l3(const gsml3_rsl* rsl) {
    if (!rsl) return 0;
    return RSLParser::hasL3Payload(rsl->parsed) ? 1 : 0;
}

GSML3_C_API const uint8_t* gsml3_rsl_l3(const gsml3_rsl* rsl, size_t* len) {
    if (!rsl) { if (len) *len = 0; return nullptr; }
    auto l3 = RSLParser::extractL3(rsl->parsed);
    if (len) *len = l3 ? l3->size() : 0;
    return l3 ? l3->data() : nullptr;
}

GSML3_C_API size_t gsml3_rsl_ie_count(const gsml3_rsl* rsl) {
    if (!rsl) return 0;
    return rsl->parsed.ieCount;
}

GSML3_C_API int gsml3_rsl_ie_get(const gsml3_rsl* rsl, size_t index,
                                 uint8_t* type, size_t* len,
                                 const uint8_t** val) {
    clearLastError();
    if (!rsl || index >= rsl->parsed.ieCount || !type || !len || !val) {
        setError(GSML3_ERR_INVALID_ARG, "invalid RSL IE index or NULL out parameter");
        return GSML3_ERR_INVALID_ARG;
    }
    const auto& ie = rsl->parsed.informationElements[index];
    *type = ie.type;
    *len = ie.len;
    *val = ie.val;
    return GSML3_OK;
}

namespace {

// The C++ span overloads return int (bytes written, -1 when the buffer
// is too small); the C API returns size_t (0 = error/too small).
size_t rslSpanResult(int n) {
    return n > 0 ? static_cast<size_t>(n) : 0;
}

} // namespace

GSML3_C_API size_t gsml3_rsl_build_data_req(uint8_t* out, size_t maxlen,
    uint8_t chan_nr, uint8_t link_id, const uint8_t* l3, size_t l3_len) {
    try {
        clearLastError();
        if (!out || maxlen == 0 || (l3_len && !l3)) {
            setLastError("NULL output buffer or L3 payload");
            return 0;
        }
        int n = RSLBuilder::buildDataReq({out, maxlen}, chan_nr, link_id, {l3, l3_len});
        if (n < 0) setBufferTooSmallError();
        return rslSpanResult(n);
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_rsl_build_data_req");
        return 0;
    }
}

GSML3_C_API size_t gsml3_rsl_build_data_ind(uint8_t* out, size_t maxlen,
    uint8_t chan_nr, uint8_t link_id, const uint8_t* l3, size_t l3_len) {
    try {
        clearLastError();
        if (!out || maxlen == 0 || (l3_len && !l3)) {
            setLastError("NULL output buffer or L3 payload");
            return 0;
        }
        int n = RSLBuilder::buildDataInd({out, maxlen}, chan_nr, link_id, {l3, l3_len});
        if (n < 0) setBufferTooSmallError();
        return rslSpanResult(n);
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_rsl_build_data_ind");
        return 0;
    }
}

GSML3_C_API size_t gsml3_rsl_build_unit_data_req(uint8_t* out, size_t maxlen,
    uint8_t chan_nr, uint8_t link_id, const uint8_t* l3, size_t l3_len) {
    try {
        clearLastError();
        if (!out || maxlen == 0 || (l3_len && !l3)) {
            setLastError("NULL output buffer or L3 payload");
            return 0;
        }
        int n = RSLBuilder::buildUnitDataReq({out, maxlen}, chan_nr, link_id, {l3, l3_len});
        if (n < 0) setBufferTooSmallError();
        return rslSpanResult(n);
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_rsl_build_unit_data_req");
        return 0;
    }
}

GSML3_C_API size_t gsml3_rsl_build_unit_data_ind(uint8_t* out, size_t maxlen,
    uint8_t chan_nr, uint8_t link_id, const uint8_t* l3, size_t l3_len) {
    try {
        clearLastError();
        if (!out || maxlen == 0 || (l3_len && !l3)) {
            setLastError("NULL output buffer or L3 payload");
            return 0;
        }
        int n = RSLBuilder::buildUnitDataInd({out, maxlen}, chan_nr, link_id, {l3, l3_len});
        if (n < 0) setBufferTooSmallError();
        return rslSpanResult(n);
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_rsl_build_unit_data_ind");
        return 0;
    }
}

GSML3_C_API size_t gsml3_rsl_build_chan_activ_ack(uint8_t* out, size_t maxlen,
    uint8_t chan_nr, uint16_t frame_number) {
    try {
        clearLastError();
        if (!out || maxlen == 0) { setLastError("NULL output buffer"); return 0; }
        int n = RSLBuilder::buildChanActivAck({out, maxlen}, chan_nr, frame_number);
        if (n < 0) setBufferTooSmallError();
        return rslSpanResult(n);
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_rsl_build_chan_activ_ack");
        return 0;
    }
}

GSML3_C_API size_t gsml3_rsl_build_chan_activ_nack(uint8_t* out, size_t maxlen,
    uint8_t chan_nr, int cause) {
    try {
        clearLastError();
        if (!out || maxlen == 0) { setLastError("NULL output buffer"); return 0; }
        if (!checkEnumValue(cause, ranges::rslCauseLo, ranges::rslCauseHi, "cause"))
            return 0;
        int n = RSLBuilder::buildChanActivNack({out, maxlen}, chan_nr,
                                                static_cast<RSLErrorCause>(cause));
        if (n < 0) setBufferTooSmallError();
        return rslSpanResult(n);
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_rsl_build_chan_activ_nack");
        return 0;
    }
}

GSML3_C_API size_t gsml3_rsl_build_rf_chan_rel_ack(uint8_t* out, size_t maxlen,
    uint8_t chan_nr) {
    try {
        clearLastError();
        if (!out || maxlen == 0) { setLastError("NULL output buffer"); return 0; }
        int n = RSLBuilder::buildRFChanRelAck({out, maxlen}, chan_nr);
        if (n < 0) setBufferTooSmallError();
        return rslSpanResult(n);
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_rsl_build_rf_chan_rel_ack");
        return 0;
    }
}

GSML3_C_API size_t gsml3_rsl_build_conn_fail(uint8_t* out, size_t maxlen,
    uint8_t chan_nr, int cause) {
    try {
        clearLastError();
        if (!out || maxlen == 0) { setLastError("NULL output buffer"); return 0; }
        if (!checkEnumValue(cause, ranges::rslCauseLo, ranges::rslCauseHi, "cause"))
            return 0;
        int n = RSLBuilder::buildConnFail({out, maxlen}, chan_nr,
                                          static_cast<RSLErrorCause>(cause));
        if (n < 0) setBufferTooSmallError();
        return rslSpanResult(n);
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_rsl_build_conn_fail");
        return 0;
    }
}

GSML3_C_API size_t gsml3_rsl_build_meas_res(uint8_t* out, size_t maxlen,
    uint8_t chan_nr, uint8_t meas_nr, int8_t rxlev, int8_t rxqual,
    const uint8_t* l1, size_t l1_len) {
    try {
        clearLastError();
        if (!out || maxlen == 0 || (l1_len && !l1)) {
            setLastError("NULL output buffer or L1 info");
            return 0;
        }
        int n = RSLBuilder::buildMeasRes({out, maxlen}, chan_nr, meas_nr,
                                         rxlev, rxqual, {l1, l1_len});
        if (n < 0) setBufferTooSmallError();
        return rslSpanResult(n);
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_rsl_build_meas_res");
        return 0;
    }
}

GSML3_C_API size_t gsml3_rsl_build_hando_det(uint8_t* out, size_t maxlen,
    uint8_t chan_nr, uint8_t access_delay) {
    try {
        clearLastError();
        if (!out || maxlen == 0) { setLastError("NULL output buffer"); return 0; }
        int n = RSLBuilder::buildHandoDet({out, maxlen}, chan_nr, access_delay);
        if (n < 0) setBufferTooSmallError();
        return rslSpanResult(n);
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_rsl_build_hando_det");
        return 0;
    }
}

GSML3_C_API size_t gsml3_rsl_build_ccch_load_ind(uint8_t* out, size_t maxlen,
    uint8_t chan_nr, uint16_t paging_load, uint16_t rach_total,
    uint16_t rach_busy, uint16_t rach_access) {
    try {
        clearLastError();
        if (!out || maxlen == 0) { setLastError("NULL output buffer"); return 0; }
        int n = RSLBuilder::buildCCCHLoadInd({out, maxlen}, chan_nr, paging_load,
                                             rach_total, rach_busy, rach_access);
        if (n < 0) setBufferTooSmallError();
        return rslSpanResult(n);
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_rsl_build_ccch_load_ind");
        return 0;
    }
}

GSML3_C_API size_t gsml3_rsl_build_chan_rqd(uint8_t* out, size_t maxlen,
    uint8_t chan_nr, uint8_t ra, uint8_t t1p, uint8_t t2, uint8_t t3,
    uint8_t access_delay) {
    try {
        clearLastError();
        if (!out || maxlen == 0) { setLastError("NULL output buffer"); return 0; }
        L3RequestReference ref(ra, t1p, t2, t3);
        int n = RSLBuilder::buildChanRqd({out, maxlen}, chan_nr, ref, access_delay);
        if (n < 0) setBufferTooSmallError();
        return rslSpanResult(n);
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_rsl_build_chan_rqd");
        return 0;
    }
}

GSML3_C_API size_t gsml3_rsl_build_delete_ind(uint8_t* out, size_t maxlen,
    uint8_t chan_nr, const uint8_t* info, size_t info_len) {
    try {
        clearLastError();
        if (!out || maxlen == 0 || (info_len && !info)) {
            setLastError("NULL output buffer or info");
            return 0;
        }
        int n = RSLBuilder::buildDeleteInd({out, maxlen}, chan_nr, {info, info_len});
        if (n < 0) setBufferTooSmallError();
        return rslSpanResult(n);
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_rsl_build_delete_ind");
        return 0;
    }
}

// ── LAPDm (GSM 04.06) ─────────────────────────────────────────────────

GSML3_C_API int gsml3_lapdm_frame_decode(const uint8_t* data, size_t len,
                                         gsml3_lapdm_frame_info* out) {
    try {
        clearLastError();
        if (!data || len == 0 || !out) {
            setError(GSML3_ERR_INVALID_ARG, "NULL input or out parameter");
            return GSML3_ERR_INVALID_ARG;
        }
        auto r = lapdm::LAPDmFrame::decode({data, len});
        if (!r) { reportParseError(r.error()); return mapParseError(r.error()); }
        const auto& f = r.value();
        out->format = static_cast<int>(f.format);
        out->u_type = (f.format == lapdm::LAPDmControlFormat::U_Format)
            ? static_cast<int>(f.uType) : -1;
        out->s_type = (f.format == lapdm::LAPDmControlFormat::S_Format)
            ? static_cast<int>(f.sType) : -1;
        out->nr = f.nr;
        out->ns = f.ns;
        out->pf = f.pf ? 1 : 0;
        out->m_bit = f.m ? 1 : 0;
        out->sapi = static_cast<int>(f.address.sapi);
        out->command = f.address.command ? 1 : 0;
        out->info = f.info.data();
        out->info_len = f.info.size();
        return GSML3_OK;
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_lapdm_frame_decode");
        return GSML3_ERR_INTERNAL;
    }
}

namespace {

// C trampolines for LAPDmEntity's fn+ctx callbacks. Declared before the
// handle definition (the constructor takes their addresses) and defined
// after it; the two blocks are the same unnamed namespace.
void lapdmL3Trampoline(SAPI sapi, Primitive prim,
                       std::span<const uint8_t> l3, void* ctx);
void lapdmL1Trampoline(std::span<const uint8_t> frame, void* ctx);

} // namespace

// The handle owns the entity plus the C callback state. LAPDmEntity has no
// default constructor, so its callbacks are wired in the member
// initializer-list: `this` already is the final handle address there, and
// the trampolines use it as the callback ctx to reach the C callbacks and
// the user pointer.
struct gsml3_lapdm_entity {
    LAPDmEntity entity;
    gsml3_lapdm_l3_cb l3Cb{nullptr};
    gsml3_lapdm_l1_cb l1Cb{nullptr};
    void* user{nullptr};

    explicit gsml3_lapdm_entity(LAPDmChannelProfile profile)
        : entity(profile, &lapdmL3Trampoline, &lapdmL1Trampoline,
                 static_cast<void*>(this)) {}
};

namespace {

void lapdmL3Trampoline(SAPI sapi, Primitive prim,
                       std::span<const uint8_t> l3, void* ctx) {
    auto* self = static_cast<gsml3_lapdm_entity*>(ctx);
    if (self->l3Cb)
        self->l3Cb(static_cast<int>(sapi), static_cast<int>(prim),
                   l3.data(), l3.size(), self->user);
}

void lapdmL1Trampoline(std::span<const uint8_t> frame, void* ctx) {
    auto* self = static_cast<gsml3_lapdm_entity*>(ctx);
    if (self->l1Cb)
        self->l1Cb(frame.data(), frame.size(), self->user);
}

} // namespace

GSML3_C_API gsml3_lapdm_entity* gsml3_lapdm_entity_new(int profile,
    gsml3_lapdm_l3_cb l3_cb, gsml3_lapdm_l1_cb l1_cb, void* user) {
    try {
        clearLastError();
        LAPDmChannelProfile p;
        switch (profile) {
            case 0:  p = LAPDmChannelProfile::SDCCH(); break;
            case 1:  p = LAPDmChannelProfile::SACCH(); break;
            case 2:  p = LAPDmChannelProfile::FACCH(); break;
            default:
                setLastError("invalid LAPDm profile (0=SDCCH, 1=SACCH, 2=FACCH)");
                return nullptr;
        }
        auto* e = new (std::nothrow) gsml3_lapdm_entity(p);
        if (!e) { setOomError(); return nullptr; }
        e->l3Cb = l3_cb;
        e->l1Cb = l1_cb;
        e->user = user;
        return e;
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_lapdm_entity_new");
        return nullptr;
    }
}

GSML3_C_API void gsml3_lapdm_entity_free(gsml3_lapdm_entity* e) {
    delete e;
}

GSML3_C_API void gsml3_lapdm_entity_open(gsml3_lapdm_entity* e, int sapi,
                                         int command_bit) {
    if (!e) return;
    if (sapi < 0 || sapi > 15) { setError(GSML3_ERR_INVALID_ARG, "invalid SAPI value (expected 0..15)"); return; }
    e->entity.open(static_cast<SAPI>(sapi), command_bit != 0);
}

GSML3_C_API void gsml3_lapdm_entity_receive(gsml3_lapdm_entity* e,
                                            const uint8_t* frame, size_t len) {
    if (!e || !frame || !len) return;
    try {
        clearLastError();
        e->entity.receiveFrame({frame, len});
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_lapdm_entity_receive; "
                               "the entity state may be inconsistent - free it and "
                               "create a fresh one");
    }
}

GSML3_C_API int gsml3_lapdm_entity_send_ui(gsml3_lapdm_entity* e, int sapi,
                                           const uint8_t* l3, size_t l3_len) {
    try {
        clearLastError();
        if (!e || (l3_len && !l3)) {
            setError(GSML3_ERR_INVALID_ARG, "NULL entity or L3 payload");
            return GSML3_ERR_INVALID_ARG;
        }
        // SAPI is 4 bits in the LAPDm address octet: an out-of-range value
        // would wrap and silently corrupt the SAPI and C/R fields, so it is
        // rejected before it can reach the encoder.
        if (sapi < ranges::sapiLo || sapi > ranges::sapiHi) {
            setError(GSML3_ERR_INVALID_ARG, "invalid SAPI value (expected 0..15)");
            return GSML3_ERR_INVALID_ARG;
        }
        auto r = e->entity.sendUI(static_cast<SAPI>(sapi), {l3, l3_len});
        if (!r) { reportParseError(r.error()); return mapParseError(r.error()); }
        return GSML3_OK;
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_lapdm_entity_send_ui");
        return GSML3_ERR_INTERNAL;
    }
}

GSML3_C_API int gsml3_lapdm_entity_send_data(gsml3_lapdm_entity* e,
                                             const uint8_t* l3, size_t l3_len) {
    try {
        clearLastError();
        if (!e || (l3_len && !l3)) {
            setError(GSML3_ERR_INVALID_ARG, "NULL entity or L3 payload");
            return GSML3_ERR_INVALID_ARG;
        }
        auto r = e->entity.sendData({l3, l3_len});
        if (!r) { reportParseError(r.error()); return mapParseError(r.error()); }
        return GSML3_OK;
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_lapdm_entity_send_data");
        return GSML3_ERR_INTERNAL;
    }
}

GSML3_C_API int gsml3_lapdm_entity_send_sabme(gsml3_lapdm_entity* e) {
    try {
        clearLastError();
        if (!e) { setError(GSML3_ERR_INVALID_ARG, "NULL entity"); return GSML3_ERR_INVALID_ARG; }
        auto r = e->entity.sendSABME();
        if (!r) { reportParseError(r.error()); return mapParseError(r.error()); }
        return GSML3_OK;
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_lapdm_entity_send_sabme");
        return GSML3_ERR_INTERNAL;
    }
}

GSML3_C_API int gsml3_lapdm_entity_send_disc(gsml3_lapdm_entity* e) {
    try {
        clearLastError();
        if (!e) { setError(GSML3_ERR_INVALID_ARG, "NULL entity"); return GSML3_ERR_INVALID_ARG; }
        auto r = e->entity.sendDISC();
        if (!r) { reportParseError(r.error()); return mapParseError(r.error()); }
        return GSML3_OK;
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_lapdm_entity_send_disc");
        return GSML3_ERR_INTERNAL;
    }
}

GSML3_C_API void gsml3_lapdm_entity_hard_release(gsml3_lapdm_entity* e) {
    if (e) e->entity.hardRelease();
}

GSML3_C_API int gsml3_lapdm_entity_tick_t200(gsml3_lapdm_entity* e,
                                             uint32_t elapsed_ms) {
    if (!e) return 0;
    try {
        clearLastError();
        return e->entity.tickT200(std::chrono::milliseconds(elapsed_ms)) ? 1 : 0;
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_lapdm_entity_tick_t200");
        return -1;
    }
}

GSML3_C_API int gsml3_lapdm_entity_state(const gsml3_lapdm_entity* e) {
    if (!e) return GSML3_LAPDM_STATE_UNUSED;
    return static_cast<int>(e->entity.state());
}

GSML3_C_API int gsml3_lapdm_entity_is_established(const gsml3_lapdm_entity* e) {
    if (!e) return 0;
    return e->entity.isEstablished() ? 1 : 0;
}

GSML3_C_API unsigned gsml3_lapdm_entity_frames_sent(const gsml3_lapdm_entity* e) {
    return e ? e->entity.framesSent() : 0;
}

GSML3_C_API unsigned gsml3_lapdm_entity_frames_received(const gsml3_lapdm_entity* e) {
    return e ? e->entity.framesReceived() : 0;
}

GSML3_C_API unsigned gsml3_lapdm_entity_retransmissions(const gsml3_lapdm_entity* e) {
    return e ? e->entity.retransmissions() : 0;
}

// ── BTS stack: registry / session ─────────────────────────────────────

// gsml3_session is a pointer-sized alias of SubscriberSession*: the
// slab-backed FlatMap keeps every entry address stable for the entry's
// whole lifetime, so the alias costs zero allocations on create/find.
struct gsml3_session { int _opaque; };  // never instantiated

// The registry handle owns exactly one of the five registry types
// (plain + the four explicitly instantiated sharded sizes). Defined at
// global scope like every other opaque handle in this file (it completes
// the tag declared in gsml3parser_c.h).
struct gsml3_registry {
    std::variant<
        std::unique_ptr<SubscriberRegistry>,
        std::unique_ptr<ShardedSubscriberRegistry<4>>,
        std::unique_ptr<ShardedSubscriberRegistry<8>>,
        std::unique_ptr<ShardedSubscriberRegistry<16>>,
        std::unique_ptr<ShardedSubscriberRegistry<32>>> reg;
};

namespace {

inline SubscriberSession* sess(gsml3_session* s) noexcept {
    return reinterpret_cast<SubscriberSession*>(s);
}

inline gsml3_session* sessPtr(SubscriberSession* s) noexcept {
    return reinterpret_cast<gsml3_session*>(s);
}

template <typename F>
auto withRegistry(gsml3_registry* r, F&& f) {
    return std::visit([&f](auto& up) { return f(*up); }, r->reg);
}

inline bool isSharded(const gsml3_registry* r) noexcept {
    return !std::holds_alternative<std::unique_ptr<SubscriberRegistry>>(r->reg);
}

} // namespace

GSML3_C_API gsml3_registry* gsml3_registry_new(int shard_count) {
    clearLastError();
    auto r = std::unique_ptr<gsml3_registry>(new (std::nothrow) gsml3_registry{});
    if (!r) { setOomError(); return nullptr; }
    try {
        switch (shard_count) {
            case 0:  r->reg = std::make_unique<SubscriberRegistry>(); break;
            case 4:  r->reg = std::make_unique<ShardedSubscriberRegistry<4>>(); break;
            case 8:  r->reg = std::make_unique<ShardedSubscriberRegistry<8>>(); break;
            case 16: r->reg = std::make_unique<ShardedSubscriberRegistry<16>>(); break;
            case 32: r->reg = std::make_unique<ShardedSubscriberRegistry<32>>(); break;
            default:
                setError(GSML3_ERR_INVALID_ARG, "shard_count must be 0, 4, 8, 16 or 32");
                return nullptr;
        }
        return r.release();
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_registry_new");
        return nullptr;
    }
}

GSML3_C_API void gsml3_registry_free(gsml3_registry* r) {
    delete r;
}

GSML3_C_API void gsml3_registry_reserve(gsml3_registry* r, size_t expected) {
    if (!r) return;
    try {
        clearLastError();
        withRegistry(r, [expected](auto& reg) { reg.reserve(expected); });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_registry_reserve");
    }
}

GSML3_C_API size_t gsml3_registry_count(const gsml3_registry* r) {
    if (!r) return 0;
    return withRegistry(const_cast<gsml3_registry*>(r),
                        [](const auto& reg) { return reg.count(); });
}

GSML3_C_API gsml3_session* gsml3_registry_create_by_tmsi(gsml3_registry* r,
                                                          uint32_t tmsi) {
    try {
        clearLastError();
        if (!r) { setLastError("NULL registry"); return nullptr; }
        if (tmsi == 0) {
            setError(GSML3_ERR_INVALID_ARG, "TMSI 0 is reserved and cannot be used");
            return nullptr;
        }
        auto* s = withRegistry(r, [tmsi](auto& reg) { return reg.createByTMSI(tmsi); });
        if (!s) setLastError("TMSI already exists");
        return sessPtr(s);
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_registry_create_by_tmsi");
        return nullptr;
    }
}

GSML3_C_API gsml3_session* gsml3_registry_create_by_imsi(gsml3_registry* r,
                                                          const char* imsi) {
    try {
        clearLastError();
        if (!r) { setLastError("NULL registry"); return nullptr; }
        if (!checkDigitString(imsi, 1, 15, false, "IMSI")) return nullptr;
        if (isSharded(r)) {
            setLastError("create_by_imsi is not supported by sharded registries");
            return nullptr;
        }
        // std::visit compiles the lambda for every alternative, so the
        // plain-registry-only member is selected with if constexpr (the
        // sharded branch is rejected by the guard above and returns null).
        auto* s = withRegistry(r, [imsi](auto& reg) {
            if constexpr (std::is_same_v<std::decay_t<decltype(reg)>, SubscriberRegistry>)
                return reg.createByIMSI(imsi);
            else
                return static_cast<SubscriberSession*>(nullptr);
        });
        if (!s) setLastError("IMSI already exists");
        return sessPtr(s);
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_registry_create_by_imsi");
        return nullptr;
    }
}

GSML3_C_API gsml3_session* gsml3_registry_find_by_tmsi(gsml3_registry* r,
                                                        uint32_t tmsi) {
    if (!r) return nullptr;
    return sessPtr(withRegistry(r, [tmsi](auto& reg) { return reg.findByTMSI(tmsi); }));
}

GSML3_C_API gsml3_session* gsml3_registry_find_by_imsi(gsml3_registry* r,
                                                        const char* imsi) {
    if (!r || !imsi) return nullptr;
    return sessPtr(withRegistry(r, [imsi](auto& reg) { return reg.findByIMSI(imsi); }));
}

GSML3_C_API gsml3_session* gsml3_registry_find_by_link(gsml3_registry* r,
    uint8_t trx, uint8_t ts, uint8_t lapdm_link) {
    if (!r) return nullptr;
    return sessPtr(withRegistry(r, [trx, ts, lapdm_link](auto& reg) {
        return reg.findByLink(trx, ts, lapdm_link);
    }));
}

GSML3_C_API int gsml3_registry_remove(gsml3_registry* r, gsml3_session* s) {
    if (!r || !s) return 0;
    return withRegistry(r, [s](auto& reg) { return reg.remove(sess(s)); }) ? 1 : 0;
}

GSML3_C_API void gsml3_registry_clear(gsml3_registry* r) {
    if (!r) return;
    if (isSharded(r)) {
        setLastError("clear is not supported by sharded registries");
        return;
    }
    // clear() exists only on the plain registry; std::visit compiles the
    // lambda for every alternative, hence if constexpr.
    withRegistry(r, [](auto& reg) {
        if constexpr (std::is_same_v<std::decay_t<decltype(reg)>, SubscriberRegistry>)
            reg.clear();
    });
}

GSML3_C_API void gsml3_registry_assign_channel(gsml3_registry* r,
    gsml3_session* s, int ch_type, uint8_t trx, uint8_t ts, uint16_t arfcn,
    uint8_t lapdm_link) {
    try {
        if (!r || !s) return;
        auto* session = sess(s);
        ChannelDescriptor desc{static_cast<ChannelType>(ch_type), trx, ts, arfcn};
        withRegistry(r, [&](auto& reg) {
            if constexpr (std::is_same_v<std::decay_t<decltype(reg)>, SubscriberRegistry>) {
                reg.assignChannel(session, desc, lapdm_link);
            } else {
                // Sharded: lock the session's shard exclusively.
                auto locked = reg.lockForTMSI(session->assignedTmsi);
                locked.registry.assignChannel(session, desc, lapdm_link);
            }
        });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_registry_assign_channel");
    }
}

GSML3_C_API void gsml3_registry_release_channel(gsml3_registry* r,
                                                gsml3_session* s) {
    try {
        if (!r || !s) return;
        auto* session = sess(s);
        withRegistry(r, [&](auto& reg) {
            if constexpr (std::is_same_v<std::decay_t<decltype(reg)>, SubscriberRegistry>) {
                reg.releaseChannel(session);
            } else {
                auto locked = reg.lockForTMSI(session->assignedTmsi);
                locked.registry.releaseChannel(session);
            }
        });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_registry_release_channel");
    }
}

// The caller's gsml3_timer_expiry array is reused as the tick buffer: the
// C struct (pointer + int) and TimerExpiry (pointer + one-byte L3TimerId)
// are the same size, and the C++ side writes into it in place. The
// conversion below then re-stores each event field by field, which is what
// makes every byte of the caller-visible struct defined (the 3 padding
// bytes TimerExpiry carries after its enum are never read back as part of
// the C int).
static_assert(sizeof(gsml3_timer_expiry) == sizeof(TimerExpiry),
              "gsml3_timer_expiry must stay layout-compatible with TimerExpiry");
static_assert(alignof(gsml3_timer_expiry) >= alignof(TimerExpiry),
              "gsml3_timer_expiry must stay layout-compatible with TimerExpiry");
static_assert(sizeof(((const gsml3_timer_expiry*)nullptr)->session) ==
                  sizeof(((const TimerExpiry*)nullptr)->session),
              "session must be the first member of both structs");

GSML3_C_API size_t gsml3_registry_tick_timers(gsml3_registry* r,
    uint32_t delta_ms, gsml3_timer_expiry* expired_out, size_t cap) {
    try {
        clearLastError();
        if (!r || (cap != 0 && !expired_out)) {
            setError(GSML3_ERR_INVALID_ARG, "NULL registry or timer-expiry buffer");
            return 0;
        }
        auto span = std::span<TimerExpiry>(
            reinterpret_cast<TimerExpiry*>(expired_out), cap);
        size_t n = withRegistry(r, [&](auto& reg) {
            return reg.tickAllTimers(std::chrono::milliseconds(delta_ms), span);
        });
        for (size_t i = 0; i < n; ++i) {
            expired_out[i].session  = sessPtr(span[i].session);
            expired_out[i].timer_id = static_cast<int>(span[i].id);
        }
        return n;
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_registry_tick_timers");
        return 0;
    }
}

GSML3_C_API size_t gsml3_registry_tick_procedures(gsml3_registry* r,
                                                  uint32_t delta_ms) {
    try {
        if (!r) return 0;
        return withRegistry(r, [&](auto& reg) {
            return reg.tickAllProcedures(std::chrono::milliseconds(delta_ms));
        });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_registry_tick_procedures");
        return 0;
    }
}

// ── Session access ─────────────────────────────────────────────────────

GSML3_C_API uint32_t gsml3_session_tmsi(gsml3_session* s) {
    if (!s) return 0;
    const auto& id = sess(s)->context.identity();
    return id.isTMSI() ? id.tmsi() : 0;
}

GSML3_C_API uint32_t gsml3_session_assigned_tmsi(gsml3_session* s) {
    return s ? sess(s)->assignedTmsi : 0;
}

GSML3_C_API void gsml3_session_set_tmsi(gsml3_session* s, uint32_t tmsi) {
    if (s) sess(s)->context.setTMSI(tmsi);
}

GSML3_C_API void gsml3_session_set_imsi(gsml3_session* s, const char* digits) {
    if (!s || !digits) return;
    clearLastError();
    if (checkDigitString(digits, 1, 15, false, "IMSI")) sess(s)->context.setIMSI(digits);
}

GSML3_C_API int gsml3_session_is_registered(gsml3_session* s) {
    return s && sess(s)->context.isRegistered() ? 1 : 0;
}

GSML3_C_API void gsml3_session_set_registered(gsml3_session* s, int v) {
    if (s) sess(s)->context.setRegistered(v != 0);
}

GSML3_C_API int gsml3_session_is_authenticated(gsml3_session* s) {
    return s && sess(s)->context.isAuthenticated() ? 1 : 0;
}

GSML3_C_API void gsml3_session_set_authenticated(gsml3_session* s, int v) {
    if (s) sess(s)->context.setAuthenticated(v != 0);
}

GSML3_C_API int gsml3_session_is_ciphered(gsml3_session* s) {
    return s && sess(s)->context.isCiphered() ? 1 : 0;
}

GSML3_C_API void gsml3_session_set_ciphered(gsml3_session* s, int v) {
    if (s) sess(s)->context.setCiphered(v != 0);
}

GSML3_C_API int gsml3_session_timer_start(gsml3_session* s, int timer_id) {
    if (!s) return 0;
    clearLastError();
    if (!checkEnumValue(timer_id, ranges::timerLo, ranges::timerHi, "timer_id"))
        return 0;
    return sess(s)->timers.start(static_cast<L3TimerId>(timer_id)) ? 1 : 0;
}

GSML3_C_API void gsml3_session_timer_stop(gsml3_session* s, int timer_id) {
    if (!s) return;
    clearLastError();
    if (checkEnumValue(timer_id, ranges::timerLo, ranges::timerHi, "timer_id"))
        sess(s)->timers.stop(static_cast<L3TimerId>(timer_id));
}

GSML3_C_API int gsml3_session_timer_running(gsml3_session* s, int timer_id) {
    if (!s) return 0;
    clearLastError();
    if (!checkEnumValue(timer_id, ranges::timerLo, ranges::timerHi, "timer_id"))
        return 0;
    return sess(s)->timers.isRunning(static_cast<L3TimerId>(timer_id)) ? 1 : 0;
}

GSML3_C_API size_t gsml3_session_transaction_pending(gsml3_session* s) {
    return s ? sess(s)->transactions.pendingCount() : 0;
}

// ── BTS stack: orchestrator / responses ───────────────────────────────

// Thread-local copy of the last ProcedureStepResult::finalResult.reason
// (the C++ string_view lifetime is not guaranteed beyond the call).
thread_local std::string tLastReason;

struct gsml3_orchestrator {
    ProcedureOrchestrator orch;
};

namespace {

// Result returned when the step could not be executed (invalid arguments,
// internal error): error carries the gsml3_error code, the other fields are
// neutral and must not be interpreted as step information.
gsml3_step_result failedStepResult(int error) {
    gsml3_step_result c;
    c.action = GSML3_ACTION_CONTINUE;
    c.response_token = GSML3_TOKEN_NONE;
    c.final_state = GSML3_STATE_INITIATED;
    c.final_type = GSML3_PROC_UNKNOWN;
    c.reason = nullptr;
    c.error = error;
    return c;
}

gsml3_step_result toCResult(const ProcedureStepResult& r) {
    gsml3_step_result c;
    c.action = static_cast<int>(r.action);
    c.response_token = static_cast<int>(r.responseToken);
    c.final_state = static_cast<int>(r.finalResult.state);
    c.final_type = static_cast<int>(r.finalResult.type);
    c.reason = nullptr;
    c.error = GSML3_OK;
    if (!r.finalResult.reason.empty()) {
        tLastReason = std::string(r.finalResult.reason);
        c.reason = tLastReason.c_str();
    }
    return c;
}

} // namespace

GSML3_C_API gsml3_orchestrator* gsml3_orchestrator_new(void) {
    clearLastError();
    auto* o = new (std::nothrow) gsml3_orchestrator{};
    if (!o) setOomError();
    return o;
}

GSML3_C_API void gsml3_orchestrator_free(gsml3_orchestrator* o) {
    delete o;
}

GSML3_C_API gsml3_step_result gsml3_orchestrator_feed(
    gsml3_orchestrator* o, const gsml3_message* msg, gsml3_session* s) {
    try {
        clearLastError();
        if (!o || !msg) {
            setError(GSML3_ERR_INVALID_ARG, "NULL orchestrator or message handle");
            return failedStepResult(GSML3_ERR_INVALID_ARG);
        }
        // The session is optional (procedures that need one degrade
        // gracefully), so it is passed through unchecked.
        return toCResult(o->orch.feed(msg->msg, sess(s)));
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_orchestrator_feed");
        return failedStepResult(GSML3_ERR_INTERNAL);
    }
}

GSML3_C_API gsml3_step_result gsml3_orchestrator_feed_auth_challenge(
    gsml3_orchestrator* o, const uint8_t rand[16], const uint8_t sres[4]) {
    try {
        clearLastError();
        if (!o || !rand || !sres) {
            setError(GSML3_ERR_INVALID_ARG, "NULL orchestrator or challenge data");
            return failedStepResult(GSML3_ERR_INVALID_ARG);
        }
        AuthChallenge c{};
        std::memcpy(c.rand.data(), rand, 16);
        std::memcpy(c.expectedSres.data(), sres, 4);
        return toCResult(o->orch.feedExternalTyped(c));
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_orchestrator_feed_auth_challenge");
        return failedStepResult(GSML3_ERR_INTERNAL);
    }
}

GSML3_C_API gsml3_step_result gsml3_orchestrator_feed_vlr_decision(
    gsml3_orchestrator* o, int accept, int has_new_tmsi, uint32_t new_tmsi,
    int reject_cause) {
    try {
        clearLastError();
        if (!o) {
            setError(GSML3_ERR_INVALID_ARG, "NULL orchestrator");
            return failedStepResult(GSML3_ERR_INVALID_ARG);
        }
        // The reject cause is only meaningful when accept == 0, but an
        // out-of-domain value must never be written into a frame later.
        if (accept == 0 &&
            !checkEnumValue(reject_cause, ranges::mmCauseLo, ranges::mmCauseHi,
                            "reject_cause")) {
            return failedStepResult(GSML3_ERR_INVALID_ARG);
        }
        VLRDecision v;
        v.accept = (accept != 0);
        v.newTmsi = has_new_tmsi ? std::optional<uint32_t>(new_tmsi) : std::nullopt;
        v.rejectCause = static_cast<MMRejectCause>(reject_cause);
        return toCResult(o->orch.feedExternalTyped(v));
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_orchestrator_feed_vlr_decision");
        return failedStepResult(GSML3_ERR_INTERNAL);
    }
}

GSML3_C_API gsml3_step_result gsml3_orchestrator_feed_ciphering(
    gsml3_orchestrator* o, uint8_t algo, int enable) {
    try {
        clearLastError();
        if (!o) {
            setError(GSML3_ERR_INVALID_ARG, "NULL orchestrator");
            return failedStepResult(GSML3_ERR_INVALID_ARG);
        }
        CipheringParameters p{algo, enable != 0};
        return toCResult(o->orch.feedExternalTyped(p));
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_orchestrator_feed_ciphering");
        return failedStepResult(GSML3_ERR_INTERNAL);
    }
}

GSML3_C_API gsml3_step_result gsml3_orchestrator_feed_paging_trigger(
    gsml3_orchestrator* o, int id_type, uint32_t tmsi, const char* imsi,
    int target_channel) {
    try {
        clearLastError();
        if (!o) {
            setError(GSML3_ERR_INVALID_ARG, "NULL orchestrator");
            return failedStepResult(GSML3_ERR_INVALID_ARG);
        }
        if (!checkEnumValue(id_type, ranges::idTypeLo, ranges::idTypeHi, "id_type") ||
            !checkEnumValue(target_channel, ranges::chanTypeLo, ranges::chanTypeHi,
                            "target_channel")) {
            return failedStepResult(GSML3_ERR_INVALID_ARG);
        }
        PagingTrigger p;
        if (id_type == GSML3_ID_TMSI) {
            p.identity = L3MobileIdentity{tmsi};
        } else if (!checkDigitString(imsi, 1, 15, false, "IMSI")) {
            return failedStepResult(GSML3_ERR_INVALID_ARG);
        } else {
            p.identity = L3MobileIdentity{std::string_view(imsi)};
        }
        p.targetChannel = static_cast<ChannelType>(target_channel);
        return toCResult(o->orch.feedExternalTyped(p));
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_orchestrator_feed_paging_trigger");
        return failedStepResult(GSML3_ERR_INTERNAL);
    }
}

GSML3_C_API size_t gsml3_orchestrator_tick(gsml3_orchestrator* o,
                                           uint32_t delta_ms) {
    try {
        if (!o) return 0;
        return o->orch.tickAll(std::chrono::milliseconds(delta_ms));
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_orchestrator_tick");
        return 0;
    }
}

GSML3_C_API size_t gsml3_orchestrator_build_response(gsml3_orchestrator* o,
    gsml3_session* s, uint8_t* out, size_t maxlen) {
    try {
        clearLastError();
        if (!o || !out || maxlen == 0) {
            setLastError("NULL orchestrator or output buffer");
            return 0;
        }
        int n = o->orch.buildPendingResponse({out, maxlen}, sess(s));
        if (n < 0) setLastError("missing response parameter or buffer too small");
        return n > 0 ? static_cast<size_t>(n) : 0;
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_orchestrator_build_response");
        return 0;
    }
}

GSML3_C_API int gsml3_orchestrator_take_retransmit(gsml3_orchestrator* o) {
    if (!o) return GSML3_TOKEN_NONE;
    return static_cast<int>(o->orch.takeRetransmissionToken());
}

GSML3_C_API void gsml3_orchestrator_cancel_all(gsml3_orchestrator* o) {
    if (o) o->orch.cancelAll();
}

GSML3_C_API int gsml3_orchestrator_chain_phase(const gsml3_orchestrator* o) {
    if (!o) return GSML3_PROC_UNKNOWN;
    return static_cast<int>(o->orch.chainPhase());
}

// ── Standalone response builders ───────────────────────────────────────

GSML3_C_API size_t gsml3_response_build_from_token(int token,
    gsml3_session* s, uint8_t* out, size_t maxlen) {
    try {
        clearLastError();
        if (!s || !out || maxlen == 0) {
            setLastError("NULL session or output buffer");
            return 0;
        }
        // A token outside the enum range would be cast into a garbage
        // ResponseToken and reach the builder's switch default silently.
        if (!checkEnumValue(token, ranges::tokenLo, ranges::tokenHi, "token"))
            return 0;
        int n = ResponseBuilder::buildResponseFromToken(
            static_cast<ResponseToken>(token), {out, maxlen}, sess(s));
        // -1 here means either a missing response parameter on the session's
        // context or an undersized buffer; both are reported as INVALID_VALUE
        // because the token alone does not disambiguate them.
        if (n < 0) setLastError("missing response parameter or buffer too small");
        return n > 0 ? static_cast<size_t>(n) : 0;
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_response_build_from_token");
        return 0;
    }
}

GSML3_C_API size_t gsml3_response_build_cm_service_accept(uint8_t* out,
    size_t maxlen) {
    try {
        clearLastError();
        if (!out || maxlen == 0) { setLastError("NULL output buffer"); return 0; }
        int n = ResponseBuilder::buildCMServiceAccept({out, maxlen});
        if (n < 0) setBufferTooSmallError();
        return n > 0 ? static_cast<size_t>(n) : 0;
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_response_build_cm_service_accept");
        return 0;
    }
}

GSML3_C_API size_t gsml3_response_build_cm_service_reject(uint8_t* out,
    size_t maxlen, int mm_cause) {
    try {
        clearLastError();
        if (!out || maxlen == 0) { setLastError("NULL output buffer"); return 0; }
        if (!checkEnumValue(mm_cause, ranges::mmCauseLo, ranges::mmCauseHi, "mm_cause"))
            return 0;
        int n = ResponseBuilder::buildCMServiceReject(
            {out, maxlen}, static_cast<MMRejectCause>(mm_cause));
        if (n < 0) setBufferTooSmallError();
        return n > 0 ? static_cast<size_t>(n) : 0;
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_response_build_cm_service_reject");
        return 0;
    }
}

GSML3_C_API size_t gsml3_response_build_identity_request(uint8_t* out,
    size_t maxlen, int id_type) {
    try {
        clearLastError();
        if (!out || maxlen == 0) { setLastError("NULL output buffer"); return 0; }
        // MobileIDType::NoID is not a valid identity-request type on the wire.
        if (!checkEnumValue(id_type, ranges::idTypeLo + 1, ranges::idTypeHi, "id_type"))
            return 0;
        int n = ResponseBuilder::buildIdentityRequest(
            {out, maxlen}, static_cast<MobileIDType>(id_type));
        if (n < 0) setBufferTooSmallError();
        return n > 0 ? static_cast<size_t>(n) : 0;
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_response_build_identity_request");
        return 0;
    }
}

GSML3_C_API size_t gsml3_response_build_authentication_request(uint8_t* out,
    size_t maxlen, const uint8_t rand[16]) {
    try {
        clearLastError();
        if (!out || maxlen == 0 || !rand) {
            setLastError("NULL output buffer or rand");
            return 0;
        }
        int n = ResponseBuilder::buildAuthenticationRequest(
            {out, maxlen}, {rand, 16});
        if (n < 0) setBufferTooSmallError();
        return n > 0 ? static_cast<size_t>(n) : 0;
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_response_build_authentication_request");
        return 0;
    }
}

GSML3_C_API size_t gsml3_response_build_location_updating_accept(
    uint8_t* out, size_t maxlen, const char* mcc, const char* mnc,
    uint16_t lac, int has_new_tmsi, uint32_t new_tmsi) {
    try {
        clearLastError();
        if (!out || maxlen == 0) { setLastError("NULL output buffer"); return 0; }
        // This LAI encoding carries exactly three MCC digits and two or
        // three MNC digits as raw BCD nibbles; anything else is rejected.
        if (!checkDigitString(mcc, 3, 3, false, "MCC")) return 0;
        if (!checkDigitString(mnc, 2, 3, false, "MNC")) return 0;
        L3LocationAreaIdentity lai{mcc, mnc, lac};
        int n = ResponseBuilder::buildLocationUpdatingAccept(
            {out, maxlen}, lai,
            has_new_tmsi ? std::optional<uint32_t>(new_tmsi) : std::nullopt);
        if (n < 0) setBufferTooSmallError();
        return n > 0 ? static_cast<size_t>(n) : 0;
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_response_build_location_updating_accept");
        return 0;
    }
}

GSML3_C_API size_t gsml3_response_build_location_updating_reject(
    uint8_t* out, size_t maxlen, int mm_cause) {
    try {
        clearLastError();
        if (!out || maxlen == 0) { setLastError("NULL output buffer"); return 0; }
        if (!checkEnumValue(mm_cause, ranges::mmCauseLo, ranges::mmCauseHi, "mm_cause"))
            return 0;
        int n = ResponseBuilder::buildLocationUpdatingReject(
            {out, maxlen}, static_cast<MMRejectCause>(mm_cause));
        if (n < 0) setBufferTooSmallError();
        return n > 0 ? static_cast<size_t>(n) : 0;
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_response_build_location_updating_reject");
        return 0;
    }
}

GSML3_C_API size_t gsml3_response_build_tmsi_reallocation_command(
    uint8_t* out, size_t maxlen, const char* mcc, const char* mnc,
    uint16_t lac, uint32_t tmsi) {
    try {
        clearLastError();
        if (!out || maxlen == 0) { setLastError("NULL output buffer"); return 0; }
        if (!checkDigitString(mcc, 3, 3, false, "MCC")) return 0;
        if (!checkDigitString(mnc, 2, 3, false, "MNC")) return 0;
        L3LocationAreaIdentity lai{mcc, mnc, lac};
        int n = ResponseBuilder::buildTMSIReallocationCommand(
            {out, maxlen}, lai, tmsi);
        if (n < 0) setBufferTooSmallError();
        return n > 0 ? static_cast<size_t>(n) : 0;
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_response_build_tmsi_reallocation_command");
        return 0;
    }
}

GSML3_C_API size_t gsml3_response_build_channel_release(uint8_t* out,
    size_t maxlen, int rr_cause) {
    try {
        clearLastError();
        if (!out || maxlen == 0) { setLastError("NULL output buffer"); return 0; }
        if (!checkEnumValue(rr_cause, ranges::rrCauseLo, ranges::rrCauseHi, "rr_cause"))
            return 0;
        int n = ResponseBuilder::buildChannelRelease(
            {out, maxlen}, static_cast<RRCause>(rr_cause));
        if (n < 0) setBufferTooSmallError();
        return n > 0 ? static_cast<size_t>(n) : 0;
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_response_build_channel_release");
        return 0;
    }
}

GSML3_C_API size_t gsml3_response_build_ciphering_mode_command(uint8_t* out,
    size_t maxlen, uint8_t algo) {
    try {
        clearLastError();
        if (!out || maxlen == 0) { setLastError("NULL output buffer"); return 0; }
        int n = ResponseBuilder::buildCipheringModeCommand({out, maxlen}, algo);
        if (n < 0) setBufferTooSmallError();
        return n > 0 ? static_cast<size_t>(n) : 0;
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_response_build_ciphering_mode_command");
        return 0;
    }
}

GSML3_C_API size_t gsml3_response_build_physical_information(uint8_t* out,
    size_t maxlen, uint8_t ta) {
    try {
        clearLastError();
        if (!out || maxlen == 0) { setLastError("NULL output buffer"); return 0; }
        int n = ResponseBuilder::buildPhysicalInformation({out, maxlen}, ta);
        if (n < 0) setBufferTooSmallError();
        return n > 0 ? static_cast<size_t>(n) : 0;
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_response_build_physical_information");
        return 0;
    }
}

GSML3_C_API size_t gsml3_response_build_immediate_assignment(uint8_t* out,
    size_t maxlen, int type_and_offset, uint8_t tn, uint8_t tsc,
    uint16_t arfcn, uint8_t ta) {
    try {
        clearLastError();
        if (!out || maxlen == 0) { setLastError("NULL output buffer"); return 0; }
        if (!checkEnumValue(type_and_offset, ranges::typeOffsetLo,
                            ranges::typeOffsetHi, "type_and_offset"))
            return 0;
        L3ChannelDescription channel{static_cast<TypeAndOffset>(type_and_offset),
                                     tn, tsc, arfcn};
        int n = ResponseBuilder::buildImmediateAssignment(
            {out, maxlen}, channel, ta);
        if (n < 0) setBufferTooSmallError();
        return n > 0 ? static_cast<size_t>(n) : 0;
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_response_build_immediate_assignment");
        return 0;
    }
}

GSML3_C_API size_t gsml3_response_build_assignment_command(uint8_t* out,
    size_t maxlen, int type_and_offset, uint8_t tn, uint8_t tsc,
    uint16_t arfcn) {
    try {
        clearLastError();
        if (!out || maxlen == 0) { setLastError("NULL output buffer"); return 0; }
        if (!checkEnumValue(type_and_offset, ranges::typeOffsetLo,
                            ranges::typeOffsetHi, "type_and_offset"))
            return 0;
        L3ChannelDescription channel{static_cast<TypeAndOffset>(type_and_offset),
                                     tn, tsc, arfcn};
        int n = ResponseBuilder::buildAssignmentCommand({out, maxlen}, channel);
        if (n < 0) setBufferTooSmallError();
        return n > 0 ? static_cast<size_t>(n) : 0;
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_response_build_assignment_command");
        return 0;
    }
}

GSML3_C_API size_t gsml3_response_build_call_proceeding(uint8_t* out,
    size_t maxlen, uint8_t ti) {
    try {
        clearLastError();
        if (!out || maxlen == 0) { setLastError("NULL output buffer"); return 0; }
        int n = ResponseBuilder::buildCallProceeding({out, maxlen}, ti);
        if (n < 0) setBufferTooSmallError();
        return n > 0 ? static_cast<size_t>(n) : 0;
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_response_build_call_proceeding");
        return 0;
    }
}

GSML3_C_API size_t gsml3_response_build_alerting(uint8_t* out, size_t maxlen,
    uint8_t ti) {
    try {
        clearLastError();
        if (!out || maxlen == 0) { setLastError("NULL output buffer"); return 0; }
        int n = ResponseBuilder::buildAlerting({out, maxlen}, ti);
        if (n < 0) setBufferTooSmallError();
        return n > 0 ? static_cast<size_t>(n) : 0;
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_response_build_alerting");
        return 0;
    }
}

GSML3_C_API size_t gsml3_response_build_connect(uint8_t* out, size_t maxlen,
    uint8_t ti) {
    try {
        clearLastError();
        if (!out || maxlen == 0) { setLastError("NULL output buffer"); return 0; }
        int n = ResponseBuilder::buildConnect({out, maxlen}, ti);
        if (n < 0) setBufferTooSmallError();
        return n > 0 ? static_cast<size_t>(n) : 0;
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_response_build_connect");
        return 0;
    }
}

GSML3_C_API size_t gsml3_response_build_connect_acknowledge(uint8_t* out,
    size_t maxlen, uint8_t ti) {
    try {
        clearLastError();
        if (!out || maxlen == 0) { setLastError("NULL output buffer"); return 0; }
        int n = ResponseBuilder::buildConnectAcknowledge({out, maxlen}, ti);
        if (n < 0) setBufferTooSmallError();
        return n > 0 ? static_cast<size_t>(n) : 0;
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_response_build_connect_acknowledge");
        return 0;
    }
}

GSML3_C_API size_t gsml3_response_build_disconnect(uint8_t* out,
    size_t maxlen, uint8_t ti, int cc_cause) {
    try {
        clearLastError();
        if (!out || maxlen == 0) { setLastError("NULL output buffer"); return 0; }
        if (!checkEnumValue(cc_cause, ranges::ccCauseLo, ranges::ccCauseHi, "cc_cause"))
            return 0;
        int n = ResponseBuilder::buildDisconnect(
            {out, maxlen}, ti, static_cast<CCCause>(cc_cause));
        if (n < 0) setBufferTooSmallError();
        return n > 0 ? static_cast<size_t>(n) : 0;
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_response_build_disconnect");
        return 0;
    }
}

GSML3_C_API size_t gsml3_response_build_release(uint8_t* out, size_t maxlen,
    uint8_t ti, int cc_cause) {
    try {
        clearLastError();
        if (!out || maxlen == 0) { setLastError("NULL output buffer"); return 0; }
        if (!checkEnumValue(cc_cause, ranges::ccCauseLo, ranges::ccCauseHi, "cc_cause"))
            return 0;
        int n = ResponseBuilder::buildRelease(
            {out, maxlen}, ti, static_cast<CCCause>(cc_cause));
        if (n < 0) setBufferTooSmallError();
        return n > 0 ? static_cast<size_t>(n) : 0;
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_response_build_release");
        return 0;
    }
}

GSML3_C_API size_t gsml3_response_build_release_complete(uint8_t* out,
    size_t maxlen, uint8_t ti) {
    try {
        clearLastError();
        if (!out || maxlen == 0) { setLastError("NULL output buffer"); return 0; }
        int n = ResponseBuilder::buildReleaseComplete({out, maxlen}, ti);
        if (n < 0) setBufferTooSmallError();
        return n > 0 ? static_cast<size_t>(n) : 0;
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_response_build_release_complete");
        return 0;
    }
}

GSML3_C_API size_t gsml3_response_build_setup(uint8_t* out, size_t maxlen,
    const char* called_digits, uint8_t ti) {
    try {
        clearLastError();
        if (!out || maxlen == 0) { setLastError("NULL output buffer"); return 0; }
        // A leading '+' marks an international (E.164) number.
        if (!checkDigitString(called_digits, 1, L3BCDDigits::maxDigits, true,
                              "called_digits"))
            return 0;
        int n = ResponseBuilder::buildSetupZeroAlloc(
            {out, maxlen}, called_digits, std::strlen(called_digits), ti);
        if (n < 0) setBufferTooSmallError();
        return n > 0 ? static_cast<size_t>(n) : 0;
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_response_build_setup");
        return 0;
    }
}

// ── Typed access: curated message fields / builders ───────────────────

namespace {

// int getter: the value, or -1 when the message is not the expected type.
template <typename T, typename F>
int typedGetInt(const gsml3_message* msg, F&& f) {
    if (!msg) return -1;
    if (auto* m = tryGet<T>(msg->msg)) return f(*m);
    return -1;
}

// Builder: write one message into the caller's buffer; 0 on error.
// (Clears the thread-local error so a success never reports a stale one.)
template <typename Variant, typename F>
size_t typedBuild(uint8_t* out, size_t maxlen, F&& f) {
    clearLastError();
    if (!out || maxlen == 0) { setLastError("NULL or empty output buffer"); return 0; }
    ParsedMessage pm(f());
    auto r = writeL3(pm, out, maxlen);
    if (!r) { reportParseError(r.error()); return 0; }
    return r.value();
}

L3MobileIdentity cIdentity(int id_type, uint32_t tmsi, const char* imsi,
                             bool* ok) {
    *ok = false;
    if (id_type == GSML3_ID_TMSI) { *ok = true; return L3MobileIdentity{tmsi}; }
    if (id_type == GSML3_ID_IMSI) {
        // The IMSI digit string is validated before it reaches the BCD
        // encoder, so the builder never emits an out-of-domain sequence.
        if (checkDigitString(imsi, 1, 15, false, "IMSI")) {
            *ok = true;
            return L3MobileIdentity{std::string_view(imsi)};
        }
        return L3MobileIdentity{};
    }
    setError(GSML3_ERR_INVALID_ARG,
             "id_type must be TMSI (GSML3_ID_TMSI) or IMSI (GSML3_ID_IMSI)");
    return L3MobileIdentity{};
}

// Validated LAI: MCC is 2..999 (rendered as three digits) and MNC 1..999.
bool checkLaiDigits(int mcc, int mnc, uint16_t /*lac*/) {
    if (mcc < 1 || mcc > 999) {
        setError(GSML3_ERR_INVALID_ARG, "invalid MCC value (expected 1..999)");
        return false;
    }
    if (mnc < 1 || mnc > 999) {
        setError(GSML3_ERR_INVALID_ARG, "invalid MNC value (expected 1..999)");
        return false;
    }
    return true;
}

L3LocationAreaIdentity cLai(int mcc, int mnc, uint16_t lac) {
    // MCC: 3 digits; MNC: at least 2 digits (the BCD nibble layout needs a
    // zero-padded 2-digit MNC, so 5 is formatted as "05").
    char m[8], n[8];
    std::snprintf(m, sizeof(m), "%03d", mcc);
    std::snprintf(n, sizeof(n), "%02d", mnc);
    return L3LocationAreaIdentity{m, n, lac};
}

// Fill the C mobile-identity struct; digits points into the message handle
// and is valid while the handle is alive.
void fillCIdentity(const L3MobileIdentity& mi, gsml3_mobile_identity* id) {
    id->type = static_cast<int>(mi.type());
    id->tmsi = mi.isTMSI() ? mi.tmsi() : 0;
    id->imsi = mi.isIMSI() ? mi.digits() : nullptr;
}

// Fill the C channel struct from a channel description.
void fillCChannel(const L3ChannelDescription& c, gsml3_channel* ch) {
    ch->type_and_offset = static_cast<int>(c.typeAndOffset());
    ch->tn = c.tn();
    ch->tsc = c.tsc();
    ch->arfcn = c.arfcn();
}

} // namespace

// ── RR getters ─────────────────────────────────────────────────────────

GSML3_C_API int gsml3_msg_channel_release_cause(const gsml3_message* msg) {
    try {
        return typedGetInt<L3ChannelRelease>(msg, [](auto& m){ return (int)m.cause(); });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_channel_release_cause");
        return -1;
    }
}

GSML3_C_API int gsml3_msg_channel_release_gprs_resumption(const gsml3_message* msg) {
    try {
        return typedGetInt<L3ChannelRelease>(msg,
            [](auto& m){ return m.hasGprsResumption() ? (m.gprsResumption() ? 1 : 0) : -1; });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_channel_release_gprs_resumption");
        return -1;
    }
}

GSML3_C_API int gsml3_msg_channel_request_ra(const gsml3_message* msg) {
    try {
        return typedGetInt<L3ChannelRequest>(msg, [](auto& m){ return (int)m.requestReference(); });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_channel_request_ra");
        return -1;
    }
}

GSML3_C_API int gsml3_msg_immediate_assignment_channel(const gsml3_message* msg,
                                                        gsml3_channel* ch) {
    try {
        if (!msg || !ch) return -1;
        if (auto* m = tryGet<L3ImmediateAssignment>(msg->msg)) {
            fillCChannel(m->channelDescription(), ch);
            return 0;
        }
        return -1;
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_immediate_assignment_channel");
        return -1;
    }
}

GSML3_C_API int gsml3_msg_immediate_assignment_ta(const gsml3_message* msg) {
    try {
        return typedGetInt<L3ImmediateAssignment>(msg,
            [](auto& m){ return (int)m.timingAdvance().timingAdvance(); });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_immediate_assignment_ta");
        return -1;
    }
}

GSML3_C_API int gsml3_msg_immediate_assignment_reject_wait_time(const gsml3_message* msg) {
    try {
        return typedGetInt<L3ImmediateAssignmentReject>(msg, [](auto& m){ return (int)m.waitTime(); });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_immediate_assignment_reject_wait_time");
        return -1;
    }
}

GSML3_C_API int gsml3_msg_assignment_command_channel(const gsml3_message* msg,
                                                      gsml3_channel* ch) {
    try {
        if (!msg || !ch) return -1;
        if (auto* m = tryGet<L3AssignmentCommand>(msg->msg)) {
            fillCChannel(m->channel(), ch);
            return 0;
        }
        return -1;
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_assignment_command_channel");
        return -1;
    }
}

GSML3_C_API int gsml3_msg_assignment_complete_cause(const gsml3_message* msg) {
    try {
        return typedGetInt<L3AssignmentComplete>(msg, [](auto& m){ return (int)m.cause(); });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_assignment_complete_cause");
        return -1;
    }
}

GSML3_C_API int gsml3_msg_assignment_failure_cause(const gsml3_message* msg) {
    try {
        return typedGetInt<L3AssignmentFailure>(msg, [](auto& m){ return (int)m.cause(); });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_assignment_failure_cause");
        return -1;
    }
}

GSML3_C_API int gsml3_msg_paging_request_type1_count(const gsml3_message* msg) {
    try {
        return typedGetInt<L3PagingRequestType1>(msg, [](auto& m){ return (int)m.mobileIds().size(); });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_paging_request_type1_count");
        return -1;
    }
}

GSML3_C_API int gsml3_msg_paging_request_type1_identity(const gsml3_message* msg,
                                                         int index,
                                                         gsml3_mobile_identity* id) {
    try {
        if (!msg || !id) return -1;
        auto* m = tryGet<L3PagingRequestType1>(msg->msg);
        if (!m) return -1;
        auto ids = m->mobileIds();
        if (index < 0 || static_cast<size_t>(index) >= ids.size()) return -1;
        fillCIdentity(ids[index], id);
        return 0;
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_paging_request_type1_identity");
        return -1;
    }
}

GSML3_C_API uint32_t gsml3_msg_paging_request_type2_tmsi(const gsml3_message* msg,
                                                          int index) {
    try {
        if (!msg) return 0;
        auto* m = tryGet<L3PagingRequestType2>(msg->msg);
        if (!m) return 0;
        auto t = m->tmsis();
        if (index < 0 || static_cast<size_t>(index) >= t.size()) return 0;
        return t[static_cast<size_t>(index)];
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_paging_request_type2_tmsi");
        return 0;
    }
}

GSML3_C_API uint32_t gsml3_msg_paging_request_type3_tmsi(const gsml3_message* msg,
                                                          int index) {
    try {
        if (!msg) return 0;
        auto* m = tryGet<L3PagingRequestType3>(msg->msg);
        if (!m) return 0;
        auto t = m->tmsis();
        if (index < 0 || static_cast<size_t>(index) >= t.size()) return 0;
        return t[static_cast<size_t>(index)];
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_paging_request_type3_tmsi");
        return 0;
    }
}

GSML3_C_API int gsml3_msg_paging_response_cks(const gsml3_message* msg) {
    try {
        return typedGetInt<L3PagingResponse>(msg, [](auto& m){ return (int)m.cksn(); });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_paging_response_cks");
        return -1;
    }
}

GSML3_C_API int gsml3_msg_paging_response_identity(const gsml3_message* msg,
                                                    gsml3_mobile_identity* id) {
    try {
        if (!msg || !id) return -1;
        auto* m = tryGet<L3PagingResponse>(msg->msg);
        if (!m) return -1;
        fillCIdentity(m->mobileId(), id);
        return 0;
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_paging_response_identity");
        return -1;
    }
}

GSML3_C_API int gsml3_msg_ciphering_mode_command_ciphering(const gsml3_message* msg) {
    try {
        return typedGetInt<L3CipheringModeCommand>(msg, [](auto& m){ return m.isCiphering() ? 1 : 0; });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_ciphering_mode_command_ciphering");
        return -1;
    }
}

GSML3_C_API int gsml3_msg_ciphering_mode_command_algorithm(const gsml3_message* msg) {
    try {
        return typedGetInt<L3CipheringModeCommand>(msg, [](auto& m){ return m.algorithm(); });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_ciphering_mode_command_algorithm");
        return -1;
    }
}

GSML3_C_API int gsml3_msg_ciphering_mode_complete_response(const gsml3_message* msg) {
    try {
        return typedGetInt<L3CipheringModeComplete>(msg, [](auto& m){ return (int)m.cipheringModeResponse(); });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_ciphering_mode_complete_response");
        return -1;
    }
}

GSML3_C_API int gsml3_msg_ciphering_mode_complete_has_imeisv(const gsml3_message* msg) {
    try {
        return typedGetInt<L3CipheringModeComplete>(msg, [](auto& m){ return m.hasImeisv() ? 1 : 0; });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_ciphering_mode_complete_has_imeisv");
        return -1;
    }
}

GSML3_C_API int gsml3_msg_handover_complete_cause(const gsml3_message* msg) {
    try {
        return typedGetInt<L3HandoverComplete>(msg, [](auto& m){ return (int)m.cause(); });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_handover_complete_cause");
        return -1;
    }
}

GSML3_C_API int gsml3_msg_handover_command_cell(const gsml3_message* msg,
                                                uint16_t* arfcn, uint8_t* ncc,
                                                uint8_t* bcc) {
    try {
        if (!msg || !arfcn || !ncc || !bcc) return -1;
        if (auto* m = tryGet<L3HandoverCommand>(msg->msg)) {
            const auto& c = m->cellDescription();
            *arfcn = c.arfcn();
            *ncc = c.ncc();
            *bcc = c.bcc();
            return 0;
        }
        return -1;
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_handover_command_cell");
        return -1;
    }
}

GSML3_C_API int gsml3_msg_physical_information_ta(const gsml3_message* msg) {
    try {
        return typedGetInt<L3PhysicalInformation>(msg, [](auto& m){ return (int)m.timingAdvance().timingAdvance(); });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_physical_information_ta");
        return -1;
    }
}

// ── MM getters ─────────────────────────────────────────────────────────

GSML3_C_API int gsml3_msg_cm_service_request_service_type(const gsml3_message* msg) {
    try {
        return typedGetInt<L3CMServiceRequest>(msg, [](auto& m){ return (int)m.serviceType(); });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_cm_service_request_service_type");
        return -1;
    }
}

GSML3_C_API int gsml3_msg_cm_service_request_identity(const gsml3_message* msg,
                                                       gsml3_mobile_identity* id) {
    try {
        if (!msg || !id) return -1;
        auto* m = tryGet<L3CMServiceRequest>(msg->msg);
        if (!m) return -1;
        fillCIdentity(m->mobileId(), id);
        return 0;
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_cm_service_request_identity");
        return -1;
    }
}

GSML3_C_API int gsml3_msg_cm_service_reject_cause(const gsml3_message* msg) {
    try {
        return typedGetInt<L3CMServiceReject>(msg, [](auto& m){ return (int)m.cause(); });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_cm_service_reject_cause");
        return -1;
    }
}

GSML3_C_API int gsml3_msg_cm_service_abort_cause(const gsml3_message* msg) {
    try {
        return typedGetInt<L3CMServiceAbort>(msg, [](auto& m){ return (int)m.cause(); });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_cm_service_abort_cause");
        return -1;
    }
}

GSML3_C_API int gsml3_msg_identity_request_type(const gsml3_message* msg) {
    try {
        return typedGetInt<L3IdentityRequest>(msg, [](auto& m){ return (int)m.type(); });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_identity_request_type");
        return -1;
    }
}

GSML3_C_API int gsml3_msg_identity_response_identity(const gsml3_message* msg,
                                                      gsml3_mobile_identity* id) {
    try {
        if (!msg || !id) return -1;
        auto* m = tryGet<L3IdentityResponse>(msg->msg);
        if (!m) return -1;
        fillCIdentity(m->mobileId(), id);
        return 0;
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_identity_response_identity");
        return -1;
    }
}

GSML3_C_API int gsml3_msg_location_updating_request_update_type(const gsml3_message* msg) {
    try {
        return typedGetInt<L3LocationUpdatingRequest>(msg, [](auto& m){ return (int)m.getLocationUpdatingType(); });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_location_updating_request_update_type");
        return -1;
    }
}

GSML3_C_API int gsml3_msg_location_updating_request_identity(const gsml3_message* msg,
                                                              gsml3_mobile_identity* id) {
    try {
        if (!msg || !id) return -1;
        auto* m = tryGet<L3LocationUpdatingRequest>(msg->msg);
        if (!m) return -1;
        fillCIdentity(m->mobileId(), id);
        return 0;
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_location_updating_request_identity");
        return -1;
    }
}

GSML3_C_API int gsml3_msg_location_updating_request_lai(const gsml3_message* msg,
                                                         gsml3_lai* lai) {
    try {
        if (!msg || !lai) return -1;
        if (auto* m = tryGet<L3LocationUpdatingRequest>(msg->msg)) {
            const auto& l = m->lai();
            lai->mcc = l.mcc();
            lai->mnc = l.mnc();
            lai->lac = static_cast<uint16_t>(l.lac());
            return 0;
        }
        return -1;
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_location_updating_request_lai");
        return -1;
    }
}

GSML3_C_API int gsml3_msg_location_updating_accept_lai(const gsml3_message* msg,
                                                        gsml3_lai* lai) {
    try {
        if (!msg || !lai) return -1;
        if (auto* m = tryGet<L3LocationUpdatingAccept>(msg->msg)) {
            const auto& l = m->lai();
            lai->mcc = l.mcc();
            lai->mnc = l.mnc();
            lai->lac = static_cast<uint16_t>(l.lac());
            return 0;
        }
        return -1;
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_location_updating_accept_lai");
        return -1;
    }
}

GSML3_C_API int gsml3_msg_location_updating_accept_identity(const gsml3_message* msg,
                                                             gsml3_mobile_identity* id) {
    try {
        if (!msg || !id) return -1;
        auto* m = tryGet<L3LocationUpdatingAccept>(msg->msg);
        if (!m) return -1;
        if (!m->hasMobileIdentity()) return -1;
        fillCIdentity(m->mobileIdentity(), id);
        return 0;
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_location_updating_accept_identity");
        return -1;
    }
}

GSML3_C_API int gsml3_msg_location_updating_reject_cause(const gsml3_message* msg) {
    try {
        return typedGetInt<L3LocationUpdatingReject>(msg, [](auto& m){ return (int)m.cause(); });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_location_updating_reject_cause");
        return -1;
    }
}

GSML3_C_API int gsml3_msg_authentication_request_cks(const gsml3_message* msg) {
    try {
        return typedGetInt<L3AuthenticationRequest>(msg, [](auto& m){ return (int)m.cksn(); });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_authentication_request_cks");
        return -1;
    }
}

GSML3_C_API int gsml3_msg_authentication_request_rand(const gsml3_message* msg,
                                                       uint8_t rand[16]) {
    try {
        if (!msg || !rand) return -1;
        auto* m = tryGet<L3AuthenticationRequest>(msg->msg);
        if (!m) return -1;
        const auto r = m->rand();  // span over a fixed std::array<uint8_t,16>
        std::memcpy(rand, r.data(), 16);
        return 0;
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_authentication_request_rand");
        return -1;
    }
}

GSML3_C_API uint32_t gsml3_msg_authentication_response_sres(const gsml3_message* msg) {
    try {
        if (!msg) return 0;
        if (auto* m = tryGet<L3AuthenticationResponse>(msg->msg)) return m->sres();
        return 0;
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_authentication_response_sres");
        return 0;
    }
}

GSML3_C_API int gsml3_msg_tmsi_reallocation_command_lai(const gsml3_message* msg,
                                                         gsml3_lai* lai) {
    try {
        if (!msg || !lai) return -1;
        if (auto* m = tryGet<L3TMSIReallocationCommand>(msg->msg)) {
            const auto& l = m->lai();
            lai->mcc = l.mcc();
            lai->mnc = l.mnc();
            lai->lac = static_cast<uint16_t>(l.lac());
            return 0;
        }
        return -1;
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_tmsi_reallocation_command_lai");
        return -1;
    }
}

GSML3_C_API uint32_t gsml3_msg_tmsi_reallocation_command_tmsi(const gsml3_message* msg) {
    try {
        if (!msg) return 0;
        if (auto* m = tryGet<L3TMSIReallocationCommand>(msg->msg))
            return m->tmsi().isTMSI() ? m->tmsi().tmsi() : 0;
        return 0;
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_tmsi_reallocation_command_tmsi");
        return 0;
    }
}

GSML3_C_API int gsml3_msg_imsi_detach_indication_identity(const gsml3_message* msg,
                                                           gsml3_mobile_identity* id) {
    try {
        if (!msg || !id) return -1;
        auto* m = tryGet<L3IMSIDetachIndication>(msg->msg);
        if (!m) return -1;
        fillCIdentity(m->mobileId(), id);
        return 0;
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_imsi_detach_indication_identity");
        return -1;
    }
}

// ── CC getters ─────────────────────────────────────────────────────────

GSML3_C_API int gsml3_msg_setup_ti(const gsml3_message* msg) {
    try {
        return typedGetInt<L3Setup>(msg, [](auto& m){ return (int)m.ti(); });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_setup_ti");
        return -1;
    }
}

GSML3_C_API int gsml3_msg_setup_have_called_party(const gsml3_message* msg) {
    try {
        return typedGetInt<L3Setup>(msg, [](auto& m){ return m.haveCalledParty() ? 1 : 0; });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_setup_have_called_party");
        return -1;
    }
}

GSML3_C_API const char* gsml3_msg_setup_called_number(const gsml3_message* msg) {
    try {
        if (!msg) return nullptr;
        if (auto* m = tryGet<L3Setup>(msg->msg))
            return m->haveCalledParty() ? m->digits() : nullptr;
        return nullptr;
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_setup_called_number");
        return nullptr;
    }
}

GSML3_C_API int gsml3_msg_call_proceeding_ti(const gsml3_message* msg) {
    try {
        return typedGetInt<L3CallProceeding>(msg, [](auto& m){ return (int)m.ti(); });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_call_proceeding_ti");
        return -1;
    }
}

GSML3_C_API int gsml3_msg_alerting_ti(const gsml3_message* msg) {
    try {
        return typedGetInt<L3Alerting>(msg, [](auto& m){ return (int)m.ti(); });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_alerting_ti");
        return -1;
    }
}

GSML3_C_API int gsml3_msg_connect_ti(const gsml3_message* msg) {
    try {
        return typedGetInt<L3Connect>(msg, [](auto& m){ return (int)m.ti(); });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_connect_ti");
        return -1;
    }
}

GSML3_C_API int gsml3_msg_connect_acknowledge_ti(const gsml3_message* msg) {
    try {
        return typedGetInt<L3ConnectAcknowledge>(msg, [](auto& m){ return (int)m.ti(); });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_connect_acknowledge_ti");
        return -1;
    }
}

GSML3_C_API int gsml3_msg_disconnect_ti(const gsml3_message* msg) {
    try {
        return typedGetInt<L3Disconnect>(msg, [](auto& m){ return (int)m.ti(); });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_disconnect_ti");
        return -1;
    }
}

GSML3_C_API int gsml3_msg_disconnect_cause(const gsml3_message* msg) {
    try {
        return typedGetInt<L3Disconnect>(msg, [](auto& m){ return (int)m.cause(); });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_disconnect_cause");
        return -1;
    }
}

GSML3_C_API int gsml3_msg_release_ti(const gsml3_message* msg) {
    try {
        return typedGetInt<L3Release>(msg, [](auto& m){ return (int)m.ti(); });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_release_ti");
        return -1;
    }
}

GSML3_C_API int gsml3_msg_release_have_cause(const gsml3_message* msg) {
    try {
        return typedGetInt<L3Release>(msg, [](auto& m){ return m.haveCause() ? 1 : 0; });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_release_have_cause");
        return -1;
    }
}

GSML3_C_API int gsml3_msg_release_cause(const gsml3_message* msg) {
    try {
        return typedGetInt<L3Release>(msg, [](auto& m){ return (int)m.cause(); });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_release_cause");
        return -1;
    }
}

GSML3_C_API int gsml3_msg_release_complete_ti(const gsml3_message* msg) {
    try {
        return typedGetInt<L3ReleaseComplete>(msg, [](auto& m){ return (int)m.ti(); });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_release_complete_ti");
        return -1;
    }
}

GSML3_C_API int gsml3_msg_release_complete_have_cause(const gsml3_message* msg) {
    try {
        return typedGetInt<L3ReleaseComplete>(msg, [](auto& m){ return m.haveCause() ? 1 : 0; });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_release_complete_have_cause");
        return -1;
    }
}

GSML3_C_API int gsml3_msg_release_complete_cause(const gsml3_message* msg) {
    try {
        return typedGetInt<L3ReleaseComplete>(msg, [](auto& m){ return (int)m.cause(); });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_release_complete_cause");
        return -1;
    }
}

GSML3_C_API int gsml3_msg_facility_ti(const gsml3_message* msg) {
    try {
        return typedGetInt<L3Facility>(msg, [](auto& m){ return (int)m.ti(); });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_facility_ti");
        return -1;
    }
}

GSML3_C_API size_t gsml3_msg_facility_body(const gsml3_message* msg,
                                            uint8_t* out, size_t maxlen) {
    try {
        if (!msg || !out) return 0;
        auto* m = tryGet<L3Facility>(msg->msg);
        if (!m) return 0;
        const auto& b = m->facilityBody();
        if (b.size() > maxlen) { setLastError("output buffer too small"); return 0; }
        if (!b.empty()) std::memcpy(out, b.data(), b.size());
        return b.size();
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_facility_body");
        return 0;
    }
}

// ── SMS getters ────────────────────────────────────────────────────────

GSML3_C_API size_t gsml3_msg_cp_data_rpdu(const gsml3_message* msg,
                                           uint8_t* out, size_t maxlen) {
    try {
        if (!msg || !out) return 0;
        auto* m = tryGet<L3CPData>(msg->msg);
        if (!m) return 0;
        const auto& b = m->rpdu();
        if (b.size() > maxlen) { setLastError("output buffer too small"); return 0; }
        if (!b.empty()) std::memcpy(out, b.data(), b.size());
        return b.size();
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_cp_data_rpdu");
        return 0;
    }
}

GSML3_C_API int gsml3_msg_cp_status_tp_oi(const gsml3_message* msg) {
    try {
        return typedGetInt<L3CPStatus>(msg, [](auto& m){ return (int)m.tpOi(); });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_cp_status_tp_oi");
        return -1;
    }
}

GSML3_C_API int gsml3_msg_cp_status_mti_value(const gsml3_message* msg) {
    try {
        return typedGetInt<L3CPStatus>(msg, [](auto& m){ return (int)m.mtiValue(); });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_cp_status_mti_value");
        return -1;
    }
}

GSML3_C_API int gsml3_msg_cp_status_has_message_ref(const gsml3_message* msg) {
    try {
        return typedGetInt<L3CPStatus>(msg, [](auto& m){ return m.hasMessageRef() ? 1 : 0; });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_cp_status_has_message_ref");
        return -1;
    }
}

GSML3_C_API int gsml3_msg_cp_status_message_ref(const gsml3_message* msg) {
    try {
        return typedGetInt<L3CPStatus>(msg, [](auto& m){ return (int)m.messageRef(); });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_cp_status_message_ref");
        return -1;
    }
}

GSML3_C_API size_t gsml3_msg_cp_smt_rpdu(const gsml3_message* msg,
                                          uint8_t* out, size_t maxlen) {
    try {
        if (!msg || !out) return 0;
        auto* m = tryGet<L3CPSMT>(msg->msg);
        if (!m) return 0;
        const auto& b = m->rpdu();
        if (b.size() > maxlen) { setLastError("output buffer too small"); return 0; }
        if (!b.empty()) std::memcpy(out, b.data(), b.size());
        return b.size();
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_cp_smt_rpdu");
        return 0;
    }
}

GSML3_C_API int gsml3_msg_sms_deliver_tp_mti(const gsml3_message* msg) {
    try {
        return typedGetInt<L3SMSDeliver>(msg, [](auto& m){ return (int)m.tpMti(); });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_sms_deliver_tp_mti");
        return -1;
    }
}

GSML3_C_API int gsml3_msg_sms_deliver_tp_mr(const gsml3_message* msg) {
    try {
        return typedGetInt<L3SMSDeliver>(msg, [](auto& m){ return (int)m.tpMr(); });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_sms_deliver_tp_mr");
        return -1;
    }
}

GSML3_C_API int gsml3_msg_sms_deliver_has_tp_ud(const gsml3_message* msg) {
    try {
        return typedGetInt<L3SMSDeliver>(msg, [](auto& m){ return m.hasTpUd() ? 1 : 0; });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_sms_deliver_has_tp_ud");
        return -1;
    }
}

GSML3_C_API size_t gsml3_msg_sms_deliver_tp_ud(const gsml3_message* msg,
                                                uint8_t* out, size_t maxlen) {
    try {
        if (!msg || !out) return 0;
        auto* m = tryGet<L3SMSDeliver>(msg->msg);
        if (!m) return 0;
        const auto& b = m->tpUd();
        if (b.size() > maxlen) { setLastError("output buffer too small"); return 0; }
        if (!b.empty()) std::memcpy(out, b.data(), b.size());
        return b.size();
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_sms_deliver_tp_ud");
        return 0;
    }
}

// ── SS getters ─────────────────────────────────────────────────────────

GSML3_C_API int gsml3_msg_sup_serv_facility_ti(const gsml3_message* msg) {
    try {
        return typedGetInt<L3SupServFacilityMessage>(msg, [](auto& m){ return (int)m.ti(); });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_sup_serv_facility_ti");
        return -1;
    }
}

GSML3_C_API size_t gsml3_msg_sup_serv_facility_data(const gsml3_message* msg,
                                                     uint8_t* out, size_t maxlen) {
    try {
        if (!msg || !out) return 0;
        auto* m = tryGet<L3SupServFacilityMessage>(msg->msg);
        if (!m) return 0;
        const auto& d = m->getMapComponents();
        if (d.size() > maxlen) { setLastError("output buffer too small"); return 0; }
        if (!d.empty()) std::memcpy(out, d.data(), d.size());
        return d.size();
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_msg_sup_serv_facility_data");
        return 0;
    }
}

// ── Typed builders ─────────────────────────────────────────────────────

GSML3_C_API size_t gsml3_build_channel_release(uint8_t* out, size_t maxlen,
                                                 int rr_cause) {
    try {
        if (!checkEnumValue(rr_cause, ranges::rrCauseLo, ranges::rrCauseHi, "rr_cause"))
            return 0;
        return typedBuild<RRM>(out, maxlen, [&]{
            return RRM{L3ChannelRelease::builder()
                           .cause(static_cast<RRCause>(rr_cause))
                           .build()};
        });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_build_channel_release");
        return 0;
    }
}

GSML3_C_API size_t gsml3_build_channel_request(uint8_t* out, size_t maxlen,
                                                uint8_t ra) {
    try {
        return typedBuild<RRM>(out, maxlen, [&]{
            return RRM{L3ChannelRequest::builder().requestReference(ra).build()};
        });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_build_channel_request");
        return 0;
    }
}

GSML3_C_API size_t gsml3_build_immediate_assignment(uint8_t* out, size_t maxlen,
    int type_and_offset, uint8_t tn, uint8_t tsc, uint16_t arfcn, uint8_t ta,
    uint8_t ra) {
    try {
        if (!checkEnumValue(type_and_offset, ranges::typeOffsetLo,
                            ranges::typeOffsetHi, "type_and_offset"))
            return 0;
        return typedBuild<RRM>(out, maxlen, [&]{
            return RRM{L3ImmediateAssignment::builder()
                           .channelDescription(L3ChannelDescription{
                               static_cast<TypeAndOffset>(type_and_offset), tn, tsc, arfcn})
                           .timingAdvance(L3TimingAdvance(ta))
                           .requestReference(L3RequestReference{ra, 0, 0, 0})
                           .build()};
        });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_build_immediate_assignment");
        return 0;
    }
}

GSML3_C_API size_t gsml3_build_immediate_assignment_reject(uint8_t* out,
    size_t maxlen, uint8_t wait_seconds) {
    try {
        return typedBuild<RRM>(out, maxlen, [&]{
            return RRM{L3ImmediateAssignmentReject::builder()
                           .waitTime(wait_seconds).build()};
        });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_build_immediate_assignment_reject");
        return 0;
    }
}

GSML3_C_API size_t gsml3_build_assignment_command(uint8_t* out, size_t maxlen,
    int type_and_offset, uint8_t tn, uint8_t tsc, uint16_t arfcn) {
    try {
        if (!checkEnumValue(type_and_offset, ranges::typeOffsetLo,
                            ranges::typeOffsetHi, "type_and_offset"))
            return 0;
        return typedBuild<RRM>(out, maxlen, [&]{
            return RRM{L3AssignmentCommand::builder()
                           .channel(L3ChannelDescription{
                               static_cast<TypeAndOffset>(type_and_offset), tn, tsc, arfcn})
                           .build()};
        });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_build_assignment_command");
        return 0;
    }
}

GSML3_C_API size_t gsml3_build_assignment_complete(uint8_t* out, size_t maxlen,
                                                    int rr_cause) {
    try {
        if (!checkEnumValue(rr_cause, ranges::rrCauseLo, ranges::rrCauseHi, "rr_cause"))
            return 0;
        return typedBuild<RRM>(out, maxlen, [&]{
            return RRM{L3AssignmentComplete::builder()
                           .cause(static_cast<RRCause>(rr_cause))
                           .build()};
        });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_build_assignment_complete");
        return 0;
    }
}

GSML3_C_API size_t gsml3_build_assignment_failure(uint8_t* out, size_t maxlen,
                                                   int rr_cause) {
    try {
        if (!checkEnumValue(rr_cause, ranges::rrCauseLo, ranges::rrCauseHi, "rr_cause"))
            return 0;
        return typedBuild<RRM>(out, maxlen, [&]{
            return RRM{L3AssignmentFailure::builder()
                           .cause(static_cast<RRCause>(rr_cause))
                           .build()};
        });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_build_assignment_failure");
        return 0;
    }
}

GSML3_C_API size_t gsml3_build_paging_request_type1(uint8_t* out, size_t maxlen,
                                                     uint32_t tmsi) {
    try {
        return typedBuild<RRM>(out, maxlen, [&]{
            return RRM{L3PagingRequestType1::builder()
                           .addMobileId(L3MobileIdentity{tmsi}, ChannelType::AnyDCCHType)
                           .build()};
        });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_build_paging_request_type1");
        return 0;
    }
}

GSML3_C_API size_t gsml3_build_paging_request_type2(uint8_t* out, size_t maxlen,
                                                     uint32_t tmsi0, uint32_t tmsi1) {
    try {
        return typedBuild<RRM>(out, maxlen, [&]{
            return RRM{L3PagingRequestType2::builder()
                           .addTMSI(tmsi0, ChannelType::AnyDCCHType)
                           .addTMSI(tmsi1, ChannelType::AnyDCCHType)
                           .build()};
        });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_build_paging_request_type2");
        return 0;
    }
}

GSML3_C_API size_t gsml3_build_paging_request_type3(uint8_t* out, size_t maxlen,
    uint32_t tmsi0, uint32_t tmsi1, uint32_t tmsi2, uint32_t tmsi3) {
    try {
        return typedBuild<RRM>(out, maxlen, [&]{
            return RRM{L3PagingRequestType3::builder()
                           .addTMSI(tmsi0, ChannelType::AnyDCCHType)
                           .addTMSI(tmsi1, ChannelType::AnyDCCHType)
                           .addTMSI(tmsi2, ChannelType::AnyDCCHType)
                           .addTMSI(tmsi3, ChannelType::AnyDCCHType)
                           .build()};
        });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_build_paging_request_type3");
        return 0;
    }
}

GSML3_C_API size_t gsml3_build_paging_response(uint8_t* out, size_t maxlen,
    int id_type, uint32_t tmsi, const char* imsi) {
    try {
        bool ok = false;
        L3MobileIdentity mi = cIdentity(id_type, tmsi, imsi, &ok);
        if (!ok) return 0; // cIdentity() reports the exact reason above
        return typedBuild<RRM>(out, maxlen, [&]{
            return RRM{L3PagingResponse::builder().mobileId(mi).build()};
        });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_build_paging_response");
        return 0;
    }
}

GSML3_C_API size_t gsml3_build_ciphering_mode_command(uint8_t* out, size_t maxlen,
                                                       uint8_t algo) {
    try {
        return typedBuild<RRM>(out, maxlen, [&]{
            // Mirror ResponseBuilder::buildCipheringModeCommand: the
            // algorithm only survives the wire when ciphering is on.
            return RRM{L3CipheringModeCommand::builder()
                           .ciphering(algo != 0)
                           .algorithm(static_cast<int>(algo))
                           .build()};
        });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_build_ciphering_mode_command");
        return 0;
    }
}

GSML3_C_API size_t gsml3_build_ciphering_mode_complete(uint8_t* out,
                                                        size_t maxlen, int response) {
    try {
        // 2-bit ciphering-mode response: 0 or 1 only.
        if (!checkEnumValue(response, 0, 1, "response")) return 0;
        return typedBuild<RRM>(out, maxlen, [&]{
            return RRM{L3CipheringModeComplete::builder()
                           .response(static_cast<unsigned>(response)).build()};
        });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_build_ciphering_mode_complete");
        return 0;
    }
}

GSML3_C_API size_t gsml3_build_handover_complete(uint8_t* out, size_t maxlen,
                                                  int rr_cause) {
    try {
        if (!checkEnumValue(rr_cause, ranges::rrCauseLo, ranges::rrCauseHi, "rr_cause"))
            return 0;
        return typedBuild<RRM>(out, maxlen, [&]{
            return RRM{L3HandoverComplete::builder()
                           .cause(static_cast<RRCause>(rr_cause))
                           .build()};
        });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_build_handover_complete");
        return 0;
    }
}

GSML3_C_API size_t gsml3_build_physical_information(uint8_t* out, size_t maxlen,
                                                     uint8_t ta) {
    try {
        return typedBuild<RRM>(out, maxlen, [&]{
            return RRM{L3PhysicalInformation::builder()
                           .timingAdvance(L3TimingAdvance(ta)).build()};
        });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_build_physical_information");
        return 0;
    }
}

// ── MM builders ────────────────────────────────────────────────────────

GSML3_C_API size_t gsml3_build_cm_service_request(uint8_t* out, size_t maxlen,
    int service_type, int id_type, uint32_t tmsi, const char* imsi) {
    try {
        if (!checkEnumValue(service_type, ranges::serviceTypeLo, ranges::serviceTypeHi,
                            "service_type"))
            return 0;
        bool ok = false;
        L3MobileIdentity mi = cIdentity(id_type, tmsi, imsi, &ok);
        if (!ok) return 0; // cIdentity() already reported the reason
        return typedBuild<MMM>(out, maxlen, [&]{
            return MMM{L3CMServiceRequest::builder()
                           .serviceType(L3CMServiceType{static_cast<L3CMServiceType::TypeCode>(service_type)})
                           .mobileIdentity(mi)
                           .build()};
        });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_build_cm_service_request");
        return 0;
    }
}

GSML3_C_API size_t gsml3_build_cm_service_accept(uint8_t* out, size_t maxlen) {
    try {
        return typedBuild<MMM>(out, maxlen, []{
            return MMM{L3CMServiceAccept::builder().build()};
        });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_build_cm_service_accept");
        return 0;
    }
}

GSML3_C_API size_t gsml3_build_cm_service_reject(uint8_t* out, size_t maxlen,
                                                  int mm_cause) {
    try {
        if (!checkEnumValue(mm_cause, ranges::mmCauseLo, ranges::mmCauseHi, "mm_cause"))
            return 0;
        return typedBuild<MMM>(out, maxlen, [&]{
            return MMM{L3CMServiceReject::builder()
                           .cause(static_cast<MMRejectCause>(mm_cause))
                           .build()};
        });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_build_cm_service_reject");
        return 0;
    }
}

GSML3_C_API size_t gsml3_build_cm_service_abort(uint8_t* out, size_t maxlen,
                                                 int abort_cause) {
    try {
        if (!checkEnumValue(abort_cause, ranges::cmAbortLo, ranges::cmAbortHi,
                            "abort_cause"))
            return 0;
        return typedBuild<MMM>(out, maxlen, [&]{
            return MMM{L3CMServiceAbort::builder()
                           .cause(static_cast<CMServiceAbortCause>(abort_cause))
                           .build()};
        });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_build_cm_service_abort");
        return 0;
    }
}

GSML3_C_API size_t gsml3_build_identity_request(uint8_t* out, size_t maxlen,
                                                  int id_type) {
    try {
        // NoID is not a requestable identity on the wire.
        if (!checkEnumValue(id_type, ranges::idTypeLo + 1, ranges::idTypeHi, "id_type"))
            return 0;
        return typedBuild<MMM>(out, maxlen, [&]{
            return MMM{L3IdentityRequest::builder()
                           .type(static_cast<MobileIDType>(id_type))
                           .build()};
        });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_build_identity_request");
        return 0;
    }
}

GSML3_C_API size_t gsml3_build_identity_response(uint8_t* out, size_t maxlen,
    int id_type, uint32_t tmsi, const char* imsi) {
    try {
        bool ok = false;
        L3MobileIdentity mi = cIdentity(id_type, tmsi, imsi, &ok);
        if (!ok) return 0; // cIdentity() reports the exact reason above
        return typedBuild<MMM>(out, maxlen, [&]{
            return MMM{L3IdentityResponse::builder().mobileId(mi).build()};
        });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_build_identity_response");
        return 0;
    }
}

GSML3_C_API size_t gsml3_build_location_updating_request(uint8_t* out,
    size_t maxlen, int update_type, int id_type, uint32_t tmsi, const char* imsi,
    int mcc, int mnc, uint16_t lac) {
    try {
        if (!checkEnumValue(update_type, ranges::updateTypeLo, ranges::updateTypeHi,
                            "update_type"))
            return 0;
        if (!checkLaiDigits(mcc, mnc, lac)) return 0;
        bool ok = false;
        L3MobileIdentity mi = cIdentity(id_type, tmsi, imsi, &ok);
        if (!ok) return 0; // cIdentity() already reported the reason
        return typedBuild<MMM>(out, maxlen, [&]{
            return MMM{L3LocationUpdatingRequest::builder()
                           .updateType(static_cast<unsigned>(update_type))
                           .mobileIdentity(mi)
                           .lai(cLai(mcc, mnc, lac))
                           .build()};
        });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_build_location_updating_request");
        return 0;
    }
}

GSML3_C_API size_t gsml3_build_location_updating_accept(uint8_t* out,
    size_t maxlen, int mcc, int mnc,     uint16_t lac, int has_new_tmsi,
    uint32_t new_tmsi) {
    try {
        if (!checkLaiDigits(mcc, mnc, lac)) return 0;
        return typedBuild<MMM>(out, maxlen, [&]{
            auto b = L3LocationUpdatingAccept::builder().lai(cLai(mcc, mnc, lac));
            if (has_new_tmsi != 0) b.mobileIdentity(L3MobileIdentity{new_tmsi});
            return MMM{b.build()};
        });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_build_location_updating_accept");
        return 0;
    }
}

GSML3_C_API size_t gsml3_build_location_updating_reject(uint8_t* out,
                                                         size_t maxlen, int mm_cause) {
    try {
        if (!checkEnumValue(mm_cause, ranges::mmCauseLo, ranges::mmCauseHi, "mm_cause"))
            return 0;
        return typedBuild<MMM>(out, maxlen, [&]{
            return MMM{L3LocationUpdatingReject::builder()
                           .cause(static_cast<MMRejectCause>(mm_cause))
                           .build()};
        });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_build_location_updating_reject");
        return 0;
    }
}

GSML3_C_API size_t gsml3_build_authentication_request(uint8_t* out, size_t maxlen,
                                                       uint8_t cksn,
                                                       const uint8_t rand[16]) {
    try {
        if (!rand) { setLastError("NULL rand"); return 0; }
        return typedBuild<MMM>(out, maxlen, [&]{
            return MMM{L3AuthenticationRequest::builder()
                           .cksn(cksn)
                           .rand(std::span<const uint8_t>(rand, 16))
                           .build()};
        });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_build_authentication_request");
        return 0;
    }
}

GSML3_C_API size_t gsml3_build_authentication_response(uint8_t* out, size_t maxlen,
                                                        uint32_t sres) {
    try {
        return typedBuild<MMM>(out, maxlen, [&]{
            return MMM{L3AuthenticationResponse::builder().sres(sres).build()};
        });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_build_authentication_response");
        return 0;
    }
}

GSML3_C_API size_t gsml3_build_tmsi_reallocation_command(uint8_t* out,
    size_t maxlen, int mcc, int mnc, uint16_t lac, uint32_t tmsi) {
    try {
        if (!checkLaiDigits(mcc, mnc, lac)) return 0;
        return typedBuild<MMM>(out, maxlen, [&]{
            return MMM{L3TMSIReallocationCommand::builder()
                           .lai(cLai(mcc, mnc, lac))
                           .tmsi(L3MobileIdentity{tmsi})
                           .build()};
        });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_build_tmsi_reallocation_command");
        return 0;
    }
}

GSML3_C_API size_t gsml3_build_tmsi_reallocation_complete(uint8_t* out,
                                                           size_t maxlen) {
    try {
        return typedBuild<MMM>(out, maxlen, []{
            return MMM{L3TMSIReallocationComplete::builder().build()};
        });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_build_tmsi_reallocation_complete");
        return 0;
    }
}

GSML3_C_API size_t gsml3_build_imsi_detach_indication(uint8_t* out, size_t maxlen,
    int id_type, uint32_t tmsi, const char* imsi) {
    try {
        bool ok = false;
        L3MobileIdentity mi = cIdentity(id_type, tmsi, imsi, &ok);
        if (!ok) return 0; // cIdentity() reports the exact reason above
        return typedBuild<MMM>(out, maxlen, [&]{
            return MMM{L3IMSIDetachIndication::builder().mobileIdentity(mi).build()};
        });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_build_imsi_detach_indication");
        return 0;
    }
}

// ── CC builders ────────────────────────────────────────────────────────

GSML3_C_API size_t gsml3_build_setup(uint8_t* out, size_t maxlen, uint8_t ti,
                                      const char* called_digits) {
    try {
        // A leading '+' marks an international (E.164) number.
        if (!checkDigitString(called_digits, 1, L3BCDDigits::maxDigits, true,
                              "called_digits"))
            return 0;
        return typedBuild<CCM>(out, maxlen, [&]{
            return CCM{L3Setup::builder()
                           .ti(ti)
                           .calledParty(L3CalledPartyBCDNumber{called_digits})
                           .build()};
        });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_build_setup");
        return 0;
    }
}

GSML3_C_API size_t gsml3_build_call_proceeding(uint8_t* out, size_t maxlen,
                                                uint8_t ti) {
    try {
        return typedBuild<CCM>(out, maxlen, [&]{
            return CCM{L3CallProceeding::builder().ti(ti).build()};
        });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_build_call_proceeding");
        return 0;
    }
}

GSML3_C_API size_t gsml3_build_alerting(uint8_t* out, size_t maxlen, uint8_t ti) {
    try {
        return typedBuild<CCM>(out, maxlen, [&]{
            return CCM{L3Alerting::builder().ti(ti).build()};
        });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_build_alerting");
        return 0;
    }
}

GSML3_C_API size_t gsml3_build_connect(uint8_t* out, size_t maxlen, uint8_t ti) {
    try {
        return typedBuild<CCM>(out, maxlen, [&]{
            return CCM{L3Connect::builder().ti(ti).build()};
        });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_build_connect");
        return 0;
    }
}

GSML3_C_API size_t gsml3_build_connect_acknowledge(uint8_t* out, size_t maxlen,
                                                    uint8_t ti) {
    try {
        return typedBuild<CCM>(out, maxlen, [&]{
            return CCM{L3ConnectAcknowledge::builder().ti(ti).build()};
        });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_build_connect_acknowledge");
        return 0;
    }
}

GSML3_C_API size_t gsml3_build_disconnect(uint8_t* out, size_t maxlen,
                                           uint8_t ti, int cc_cause) {
    try {
        if (!checkEnumValue(cc_cause, ranges::ccCauseLo, ranges::ccCauseHi, "cc_cause"))
            return 0;
        return typedBuild<CCM>(out, maxlen, [&]{
            return CCM{L3Disconnect::builder()
                           .ti(ti)
                           .cause(static_cast<CCCause>(cc_cause))
                           .build()};
        });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_build_disconnect");
        return 0;
    }
}

GSML3_C_API size_t gsml3_build_release(uint8_t* out, size_t maxlen,
                                        uint8_t ti, int cc_cause) {
    try {
        if (!checkEnumValue(cc_cause, ranges::ccCauseLo, ranges::ccCauseHi, "cc_cause"))
            return 0;
        return typedBuild<CCM>(out, maxlen, [&]{
            return CCM{L3Release::builder()
                           .ti(ti)
                           .cause(static_cast<CCCause>(cc_cause))
                           .build()};
        });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_build_release");
        return 0;
    }
}

GSML3_C_API size_t gsml3_build_release_complete(uint8_t* out, size_t maxlen,
                                                 uint8_t ti) {
    try {
        return typedBuild<CCM>(out, maxlen, [&]{
            return CCM{L3ReleaseComplete::builder().ti(ti).build()};
        });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_build_release_complete");
        return 0;
    }
}

GSML3_C_API size_t gsml3_build_facility(uint8_t* out, size_t maxlen, uint8_t ti,
                                         const uint8_t* data, size_t len) {
    try {
        if (len != 0 && !data) {
            clearLastError();
            setLastError("NULL payload with non-zero length");
            return 0;
        }
        std::vector<uint8_t> body;
        if (len != 0) body.assign(data, data + len);
        return typedBuild<CCM>(out, maxlen, [&]{
            return CCM{L3Facility::builder()
                           .ti(ti)
                           .facilityBody(std::move(body))
                           .build()};
        });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_build_facility");
        return 0;
    }
}

// ── SMS builders ───────────────────────────────────────────────────────

GSML3_C_API size_t gsml3_build_cp_data(uint8_t* out, size_t maxlen,
                                        const uint8_t* rpdu, size_t rpdu_len) {
    try {
        if (rpdu_len != 0 && !rpdu) {
            clearLastError();
            setLastError("NULL payload with non-zero length");
            return 0;
        }
        std::vector<uint8_t> payload;
        if (rpdu_len != 0) payload.assign(rpdu, rpdu + rpdu_len);
        return typedBuild<SMS>(out, maxlen, [&]{
            return SMS{L3CPData::builder().rpdu(std::move(payload)).build()};
        });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_build_cp_data");
        return 0;
    }
}

GSML3_C_API size_t gsml3_build_cp_status(uint8_t* out, size_t maxlen,
    uint8_t tp_oi, uint8_t mti_value, int has_ref, uint8_t ref) {
    try {
        return typedBuild<SMS>(out, maxlen, [&]{
            return SMS{L3CPStatus::builder()
                           .tpOi(tp_oi)
                           .mtiValue(mti_value)
                           .haveMessageRef(has_ref != 0)
                           .messageRef(ref)
                           .build()};
        });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_build_cp_status");
        return 0;
    }
}

GSML3_C_API size_t gsml3_build_cp_smt(uint8_t* out, size_t maxlen,
                                       const uint8_t* rpdu, size_t rpdu_len) {
    try {
        if (rpdu_len != 0 && !rpdu) {
            clearLastError();
            setLastError("NULL payload with non-zero length");
            return 0;
        }
        std::vector<uint8_t> payload;
        if (rpdu_len != 0) payload.assign(rpdu, rpdu + rpdu_len);
        return typedBuild<SMS>(out, maxlen, [&]{
            return SMS{L3CPSMT::builder().rpdu(std::move(payload)).build()};
        });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_build_cp_smt");
        return 0;
    }
}

GSML3_C_API size_t gsml3_build_sms_deliver(uint8_t* out, size_t maxlen,
    uint8_t tp_mti, uint8_t tp_mr, const uint8_t* ud, size_t ud_len) {
    try {
        if (ud_len != 0 && !ud) {
            clearLastError();
            setLastError("NULL payload with non-zero length");
            return 0;
        }
        std::vector<uint8_t> user_data;
        if (ud_len != 0) user_data.assign(ud, ud + ud_len);
        return typedBuild<SMS>(out, maxlen, [&]{
            return SMS{L3SMSDeliver::builder()
                           .tpMti(tp_mti)
                           .tpMr(tp_mr)
                           .haveTpUd(ud_len != 0)
                           .tpUd(std::move(user_data))
                           .build()};
        });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_build_sms_deliver");
        return 0;
    }
}

// ── SS builders ────────────────────────────────────────────────────────

GSML3_C_API size_t gsml3_build_sup_serv_facility(uint8_t* out, size_t maxlen,
    uint8_t ti, const uint8_t* data, size_t len) {
    try {
        if (len != 0 && !data) {
            clearLastError();
            setLastError("NULL payload with non-zero length");
            return 0;
        }
        // The SS facility payload is a sequence of opaque octets carried
        // in a string by the C++ type; the cast keeps every byte intact.
        std::string body;
        if (len != 0) body.assign(reinterpret_cast<const char*>(data), len);
        return typedBuild<SSM>(out, maxlen, [&]{
            return SSM{L3SupServFacilityMessage::builder()
                           .ti(ti)
                           .facility(body)
                           .build()};
        });
    } catch (...) {
        setLastErrorUnexpected("unexpected exception in gsml3_build_sup_serv_facility");
        return 0;
    }
}
