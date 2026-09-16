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

// ── RSL (A-bis, TS 48.058) ─────────────────────────────────────────────

// The handle owns a copy of the input: RSLParsedMessage holds spans into
// that copy (IE values, L3 payload), so the caller's buffer may be freed
// right after gsml3_rsl_parse returns.
struct gsml3_rsl {
    std::vector<uint8_t> buffer;
    RSLParsedMessage parsed;
};

GSML3_C_API gsml3_rsl* gsml3_rsl_parse(const uint8_t* data, size_t len) {
    try {
        tLastError.clear();
        if (!data || len == 0) { setLastError("NULL or empty input"); return nullptr; }
        auto r = RSLParser::parse({data, len});
        if (!r) { reportParseError(r.error()); return nullptr; }
        auto* h = new (std::nothrow) gsml3_rsl{};
        if (!h) { setLastError("out of memory"); return nullptr; }
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
        return h;
    } catch (...) {
        setLastError("unexpected exception in gsml3_rsl_parse");
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
    if (!rsl || index >= rsl->parsed.ieCount || !type || !len || !val) {
        setLastError("invalid RSL IE index or NULL out parameter");
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
        tLastError.clear();
        if (!out || maxlen == 0 || (l3_len && !l3)) {
            setLastError("NULL output buffer or L3 payload");
            return 0;
        }
        int n = RSLBuilder::buildDataReq({out, maxlen}, chan_nr, link_id, {l3, l3_len});
        if (n < 0) setLastError("buffer too small");
        return rslSpanResult(n);
    } catch (...) {
        setLastError("unexpected exception in gsml3_rsl_build_data_req");
        return 0;
    }
}

GSML3_C_API size_t gsml3_rsl_build_data_ind(uint8_t* out, size_t maxlen,
    uint8_t chan_nr, uint8_t link_id, const uint8_t* l3, size_t l3_len) {
    try {
        tLastError.clear();
        if (!out || maxlen == 0 || (l3_len && !l3)) {
            setLastError("NULL output buffer or L3 payload");
            return 0;
        }
        int n = RSLBuilder::buildDataInd({out, maxlen}, chan_nr, link_id, {l3, l3_len});
        if (n < 0) setLastError("buffer too small");
        return rslSpanResult(n);
    } catch (...) {
        setLastError("unexpected exception in gsml3_rsl_build_data_ind");
        return 0;
    }
}

GSML3_C_API size_t gsml3_rsl_build_unit_data_req(uint8_t* out, size_t maxlen,
    uint8_t chan_nr, uint8_t link_id, const uint8_t* l3, size_t l3_len) {
    try {
        tLastError.clear();
        if (!out || maxlen == 0 || (l3_len && !l3)) {
            setLastError("NULL output buffer or L3 payload");
            return 0;
        }
        int n = RSLBuilder::buildUnitDataReq({out, maxlen}, chan_nr, link_id, {l3, l3_len});
        if (n < 0) setLastError("buffer too small");
        return rslSpanResult(n);
    } catch (...) {
        setLastError("unexpected exception in gsml3_rsl_build_unit_data_req");
        return 0;
    }
}

GSML3_C_API size_t gsml3_rsl_build_unit_data_ind(uint8_t* out, size_t maxlen,
    uint8_t chan_nr, uint8_t link_id, const uint8_t* l3, size_t l3_len) {
    try {
        tLastError.clear();
        if (!out || maxlen == 0 || (l3_len && !l3)) {
            setLastError("NULL output buffer or L3 payload");
            return 0;
        }
        int n = RSLBuilder::buildUnitDataInd({out, maxlen}, chan_nr, link_id, {l3, l3_len});
        if (n < 0) setLastError("buffer too small");
        return rslSpanResult(n);
    } catch (...) {
        setLastError("unexpected exception in gsml3_rsl_build_unit_data_ind");
        return 0;
    }
}

GSML3_C_API size_t gsml3_rsl_build_chan_activ_ack(uint8_t* out, size_t maxlen,
    uint8_t chan_nr, uint16_t frame_number) {
    try {
        tLastError.clear();
        if (!out || maxlen == 0) { setLastError("NULL output buffer"); return 0; }
        int n = RSLBuilder::buildChanActivAck({out, maxlen}, chan_nr, frame_number);
        if (n < 0) setLastError("buffer too small");
        return rslSpanResult(n);
    } catch (...) {
        setLastError("unexpected exception in gsml3_rsl_build_chan_activ_ack");
        return 0;
    }
}

GSML3_C_API size_t gsml3_rsl_build_chan_activ_nack(uint8_t* out, size_t maxlen,
    uint8_t chan_nr, int cause) {
    try {
        tLastError.clear();
        if (!out || maxlen == 0) { setLastError("NULL output buffer"); return 0; }
        int n = RSLBuilder::buildChanActivNack({out, maxlen}, chan_nr,
                                               static_cast<RSLErrorCause>(cause));
        if (n < 0) setLastError("buffer too small");
        return rslSpanResult(n);
    } catch (...) {
        setLastError("unexpected exception in gsml3_rsl_build_chan_activ_nack");
        return 0;
    }
}

GSML3_C_API size_t gsml3_rsl_build_rf_chan_rel_ack(uint8_t* out, size_t maxlen,
    uint8_t chan_nr) {
    try {
        tLastError.clear();
        if (!out || maxlen == 0) { setLastError("NULL output buffer"); return 0; }
        int n = RSLBuilder::buildRFChanRelAck({out, maxlen}, chan_nr);
        if (n < 0) setLastError("buffer too small");
        return rslSpanResult(n);
    } catch (...) {
        setLastError("unexpected exception in gsml3_rsl_build_rf_chan_rel_ack");
        return 0;
    }
}

GSML3_C_API size_t gsml3_rsl_build_conn_fail(uint8_t* out, size_t maxlen,
    uint8_t chan_nr, int cause) {
    try {
        tLastError.clear();
        if (!out || maxlen == 0) { setLastError("NULL output buffer"); return 0; }
        int n = RSLBuilder::buildConnFail({out, maxlen}, chan_nr,
                                          static_cast<RSLErrorCause>(cause));
        if (n < 0) setLastError("buffer too small");
        return rslSpanResult(n);
    } catch (...) {
        setLastError("unexpected exception in gsml3_rsl_build_conn_fail");
        return 0;
    }
}

GSML3_C_API size_t gsml3_rsl_build_meas_res(uint8_t* out, size_t maxlen,
    uint8_t chan_nr, uint8_t meas_nr, int8_t rxlev, int8_t rxqual,
    const uint8_t* l1, size_t l1_len) {
    try {
        tLastError.clear();
        if (!out || maxlen == 0 || (l1_len && !l1)) {
            setLastError("NULL output buffer or L1 info");
            return 0;
        }
        int n = RSLBuilder::buildMeasRes({out, maxlen}, chan_nr, meas_nr,
                                         rxlev, rxqual, {l1, l1_len});
        if (n < 0) setLastError("buffer too small");
        return rslSpanResult(n);
    } catch (...) {
        setLastError("unexpected exception in gsml3_rsl_build_meas_res");
        return 0;
    }
}

GSML3_C_API size_t gsml3_rsl_build_hando_det(uint8_t* out, size_t maxlen,
    uint8_t chan_nr, uint8_t access_delay) {
    try {
        tLastError.clear();
        if (!out || maxlen == 0) { setLastError("NULL output buffer"); return 0; }
        int n = RSLBuilder::buildHandoDet({out, maxlen}, chan_nr, access_delay);
        if (n < 0) setLastError("buffer too small");
        return rslSpanResult(n);
    } catch (...) {
        setLastError("unexpected exception in gsml3_rsl_build_hando_det");
        return 0;
    }
}

GSML3_C_API size_t gsml3_rsl_build_ccch_load_ind(uint8_t* out, size_t maxlen,
    uint8_t chan_nr, uint16_t paging_load, uint16_t rach_total,
    uint16_t rach_busy, uint16_t rach_access) {
    try {
        tLastError.clear();
        if (!out || maxlen == 0) { setLastError("NULL output buffer"); return 0; }
        int n = RSLBuilder::buildCCCHLoadInd({out, maxlen}, chan_nr, paging_load,
                                             rach_total, rach_busy, rach_access);
        if (n < 0) setLastError("buffer too small");
        return rslSpanResult(n);
    } catch (...) {
        setLastError("unexpected exception in gsml3_rsl_build_ccch_load_ind");
        return 0;
    }
}

GSML3_C_API size_t gsml3_rsl_build_chan_rqd(uint8_t* out, size_t maxlen,
    uint8_t chan_nr, uint8_t ra, uint8_t t1p, uint8_t t2, uint8_t t3,
    uint8_t access_delay) {
    try {
        tLastError.clear();
        if (!out || maxlen == 0) { setLastError("NULL output buffer"); return 0; }
        L3RequestReference ref(ra, t1p, t2, t3);
        int n = RSLBuilder::buildChanRqd({out, maxlen}, chan_nr, ref, access_delay);
        if (n < 0) setLastError("buffer too small");
        return rslSpanResult(n);
    } catch (...) {
        setLastError("unexpected exception in gsml3_rsl_build_chan_rqd");
        return 0;
    }
}

GSML3_C_API size_t gsml3_rsl_build_delete_ind(uint8_t* out, size_t maxlen,
    uint8_t chan_nr, const uint8_t* info, size_t info_len) {
    try {
        tLastError.clear();
        if (!out || maxlen == 0 || (info_len && !info)) {
            setLastError("NULL output buffer or info");
            return 0;
        }
        int n = RSLBuilder::buildDeleteInd({out, maxlen}, chan_nr, {info, info_len});
        if (n < 0) setLastError("buffer too small");
        return rslSpanResult(n);
    } catch (...) {
        setLastError("unexpected exception in gsml3_rsl_build_delete_ind");
        return 0;
    }
}
