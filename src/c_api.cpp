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

namespace {

using namespace gsml3parser;

// Thread-local last error message ("" when none). Lives until the next
// gsml3_* call on this thread; callers that need it longer must copy it.
thread_local std::string tLastError;

void setLastError(const char* msg) { tLastError = msg ? msg : ""; }

void reportParseError(const ParseError& e) { tLastError = std::string(e.message); }

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
        default:                                   return GSML3_ERR_INVALID_VALUE;
    }
}

} // namespace

// ── Opaque handle definitions (declared in gsml3parser_c.h) ────────────

struct gsml3_config  { ParserConfig cfg; };
struct gsml3_message { ParsedMessage msg; };

// ── Version / error model / free ───────────────────────────────────────

GSML3_C_API const char* gsml3_version(void) {
#ifdef GSML3PARSER_VERSION
    return GSML3PARSER_VERSION;
#else
    return "0.18.0";
#endif
}

GSML3_C_API const char* gsml3_last_error(void) {
    return tLastError.c_str();
}

GSML3_C_API void gsml3_free(void* ptr) {
    delete[] static_cast<char*>(ptr);
}

// ── Parser config ──────────────────────────────────────────────────────

GSML3_C_API gsml3_config* gsml3_config_new(void) {
    tLastError.clear();
    auto* c = new (std::nothrow) gsml3_config{};
    if (!c) setLastError("out of memory");
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
        tLastError.clear();
        if (!data || len == 0) { setLastError("NULL or empty input"); return nullptr; }
        auto r = parseL3({data, len}, cfg ? cfg->cfg : ParserConfig{});
        if (!r) { reportParseError(r.error()); return nullptr; }
        auto* m = new (std::nothrow) gsml3_message{std::move(r.value())};
        if (!m) setLastError("out of memory");
        return m;
    } catch (...) {
        setLastError("unexpected exception in gsml3_parse_l3");
        return nullptr;
    }
}

GSML3_C_API gsml3_message* gsml3_parse_l3_hex(const char* hex,
                                              const gsml3_config* cfg) {
    try {
        tLastError.clear();
        if (!hex) { setLastError("NULL hex string"); return nullptr; }
        auto r = parseL3Hex(hex, cfg ? cfg->cfg : ParserConfig{});
        if (!r) { reportParseError(r.error()); return nullptr; }
        auto* m = new (std::nothrow) gsml3_message{std::move(r.value())};
        if (!m) setLastError("out of memory");
        return m;
    } catch (...) {
        setLastError("unexpected exception in gsml3_parse_l3_hex");
        return nullptr;
    }
}

GSML3_C_API int gsml3_parse_l3_into(gsml3_message* msg, const uint8_t* data,
                                    size_t len, const gsml3_config* cfg) {
    try {
        tLastError.clear();
        if (!msg || !data || len == 0) {
            setLastError("NULL handle or empty input");
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
        setLastError("unexpected exception in gsml3_parse_l3_into");
        return GSML3_ERR_INVALID_VALUE;
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

GSML3_C_API size_t gsml3_message_write(const gsml3_message* msg,
                                       uint8_t* out, size_t maxlen) {
    try {
        tLastError.clear();
        if (!msg || !out || maxlen == 0) {
            setLastError("NULL handle or output buffer");
            return 0;
        }
        auto r = writeL3(msg->msg, out, maxlen);
        if (!r) { reportParseError(r.error()); return 0; }
        return r.value();
    } catch (...) {
        setLastError("unexpected exception in gsml3_message_write");
        return 0;
    }
}

GSML3_C_API char* gsml3_message_hex(const gsml3_message* msg) {
    try {
        tLastError.clear();
        if (!msg) { setLastError("NULL handle"); return nullptr; }
        auto r = writeL3Hex(msg->msg);
        if (!r) { reportParseError(r.error()); return nullptr; }
        const std::string& s = r.value();
        char* p = new (std::nothrow) char[s.size() + 1];
        if (!p) { setLastError("out of memory"); return nullptr; }
        std::memcpy(p, s.data(), s.size());
        p[s.size()] = '\0';
        return p;
    } catch (...) {
        setLastError("unexpected exception in gsml3_message_hex");
        return nullptr;
    }
}
