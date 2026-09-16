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

// ── LAPDm (GSM 04.06) ─────────────────────────────────────────────────

GSML3_C_API int gsml3_lapdm_frame_decode(const uint8_t* data, size_t len,
                                         gsml3_lapdm_frame_info* out) {
    try {
        tLastError.clear();
        if (!data || len == 0 || !out) {
            setLastError("NULL input or out parameter");
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
        setLastError("unexpected exception in gsml3_lapdm_frame_decode");
        return GSML3_ERR_INVALID_VALUE;
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
        tLastError.clear();
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
        if (!e) { setLastError("out of memory"); return nullptr; }
        e->l3Cb = l3_cb;
        e->l1Cb = l1_cb;
        e->user = user;
        return e;
    } catch (...) {
        setLastError("unexpected exception in gsml3_lapdm_entity_new");
        return nullptr;
    }
}

GSML3_C_API void gsml3_lapdm_entity_free(gsml3_lapdm_entity* e) {
    delete e;
}

GSML3_C_API void gsml3_lapdm_entity_open(gsml3_lapdm_entity* e, int sapi,
                                         int command_bit) {
    if (e && sapi >= 0 && sapi <= 15)
        e->entity.open(static_cast<SAPI>(sapi), command_bit != 0);
}

GSML3_C_API void gsml3_lapdm_entity_receive(gsml3_lapdm_entity* e,
                                            const uint8_t* frame, size_t len) {
    if (e && frame && len) e->entity.receiveFrame({frame, len});
}

GSML3_C_API int gsml3_lapdm_entity_send_ui(gsml3_lapdm_entity* e, int sapi,
                                           const uint8_t* l3, size_t l3_len) {
    try {
        tLastError.clear();
        if (!e || (l3_len && !l3)) {
            setLastError("NULL entity or L3 payload");
            return GSML3_ERR_INVALID_ARG;
        }
        auto r = e->entity.sendUI(static_cast<SAPI>(sapi), {l3, l3_len});
        if (!r) { reportParseError(r.error()); return mapParseError(r.error()); }
        return GSML3_OK;
    } catch (...) {
        setLastError("unexpected exception in gsml3_lapdm_entity_send_ui");
        return GSML3_ERR_INVALID_VALUE;
    }
}

GSML3_C_API int gsml3_lapdm_entity_send_data(gsml3_lapdm_entity* e,
                                             const uint8_t* l3, size_t l3_len) {
    try {
        tLastError.clear();
        if (!e || (l3_len && !l3)) {
            setLastError("NULL entity or L3 payload");
            return GSML3_ERR_INVALID_ARG;
        }
        auto r = e->entity.sendData({l3, l3_len});
        if (!r) { reportParseError(r.error()); return mapParseError(r.error()); }
        return GSML3_OK;
    } catch (...) {
        setLastError("unexpected exception in gsml3_lapdm_entity_send_data");
        return GSML3_ERR_INVALID_VALUE;
    }
}

GSML3_C_API int gsml3_lapdm_entity_send_sabme(gsml3_lapdm_entity* e) {
    try {
        tLastError.clear();
        if (!e) { setLastError("NULL entity"); return GSML3_ERR_INVALID_ARG; }
        auto r = e->entity.sendSABME();
        if (!r) { reportParseError(r.error()); return mapParseError(r.error()); }
        return GSML3_OK;
    } catch (...) {
        setLastError("unexpected exception in gsml3_lapdm_entity_send_sabme");
        return GSML3_ERR_INVALID_VALUE;
    }
}

GSML3_C_API int gsml3_lapdm_entity_send_disc(gsml3_lapdm_entity* e) {
    try {
        tLastError.clear();
        if (!e) { setLastError("NULL entity"); return GSML3_ERR_INVALID_ARG; }
        auto r = e->entity.sendDISC();
        if (!r) { reportParseError(r.error()); return mapParseError(r.error()); }
        return GSML3_OK;
    } catch (...) {
        setLastError("unexpected exception in gsml3_lapdm_entity_send_disc");
        return GSML3_ERR_INVALID_VALUE;
    }
}

GSML3_C_API void gsml3_lapdm_entity_hard_release(gsml3_lapdm_entity* e) {
    if (e) e->entity.hardRelease();
}

GSML3_C_API int gsml3_lapdm_entity_tick_t200(gsml3_lapdm_entity* e,
                                             uint32_t elapsed_ms) {
    if (!e) return 0;
    return e->entity.tickT200(std::chrono::milliseconds(elapsed_ms)) ? 1 : 0;
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
    try {
        tLastError.clear();
        auto* r = new (std::nothrow) gsml3_registry{};
        if (!r) { setLastError("out of memory"); return nullptr; }
        switch (shard_count) {
            case 0:  r->reg = std::make_unique<SubscriberRegistry>(); break;
            case 4:  r->reg = std::make_unique<ShardedSubscriberRegistry<4>>(); break;
            case 8:  r->reg = std::make_unique<ShardedSubscriberRegistry<8>>(); break;
            case 16: r->reg = std::make_unique<ShardedSubscriberRegistry<16>>(); break;
            case 32: r->reg = std::make_unique<ShardedSubscriberRegistry<32>>(); break;
            default:
                setLastError("shard_count must be 0, 4, 8, 16 or 32");
                delete r;
                return nullptr;
        }
        return r;
    } catch (...) {
        setLastError("unexpected exception in gsml3_registry_new");
        return nullptr;
    }
}

GSML3_C_API void gsml3_registry_free(gsml3_registry* r) {
    delete r;
}

GSML3_C_API void gsml3_registry_reserve(gsml3_registry* r, size_t expected) {
    if (r) withRegistry(r, [expected](auto& reg) { reg.reserve(expected); });
}

GSML3_C_API size_t gsml3_registry_count(const gsml3_registry* r) {
    if (!r) return 0;
    return withRegistry(const_cast<gsml3_registry*>(r),
                        [](const auto& reg) { return reg.count(); });
}

GSML3_C_API gsml3_session* gsml3_registry_create_by_tmsi(gsml3_registry* r,
                                                          uint32_t tmsi) {
    try {
        tLastError.clear();
        if (!r) { setLastError("NULL registry"); return nullptr; }
        auto* s = withRegistry(r, [tmsi](auto& reg) { return reg.createByTMSI(tmsi); });
        if (!s) setLastError("TMSI already exists");
        return sessPtr(s);
    } catch (...) {
        setLastError("unexpected exception in gsml3_registry_create_by_tmsi");
        return nullptr;
    }
}

GSML3_C_API gsml3_session* gsml3_registry_create_by_imsi(gsml3_registry* r,
                                                          const char* imsi) {
    try {
        tLastError.clear();
        if (!r || !imsi) { setLastError("NULL registry or imsi"); return nullptr; }
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
        setLastError("unexpected exception in gsml3_registry_create_by_imsi");
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
        setLastError("unexpected exception in gsml3_registry_assign_channel");
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
        setLastError("unexpected exception in gsml3_registry_release_channel");
    }
}

// gsml3_timer_expiry is layout-compatible with TimerExpiry
// (pointer + int == pointer + L3TimerId, both 16 bytes), so the caller's
// buffer is used directly (zero allocation, O(active)).
static_assert(sizeof(gsml3_timer_expiry) == sizeof(TimerExpiry),
              "gsml3_timer_expiry must stay layout-compatible with TimerExpiry");
static_assert(alignof(gsml3_timer_expiry) >= alignof(TimerExpiry),
              "gsml3_timer_expiry must stay layout-compatible with TimerExpiry");

GSML3_C_API size_t gsml3_registry_tick_timers(gsml3_registry* r,
    uint32_t delta_ms, gsml3_timer_expiry* expired_out, size_t cap) {
    try {
        if (!r) return 0;
        auto span = std::span<TimerExpiry>(
            reinterpret_cast<TimerExpiry*>(expired_out), cap);
        size_t n = withRegistry(r, [&](auto& reg) {
            return reg.tickAllTimers(std::chrono::milliseconds(delta_ms), span);
        });
        // The session pointers are already correct (same pointer value);
        // the conversion loop documents the aliasing contract.
        for (size_t i = 0; i < n; ++i)
            expired_out[i].session = sessPtr(span[i].session);
        return n;
    } catch (...) {
        setLastError("unexpected exception in gsml3_registry_tick_timers");
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
        setLastError("unexpected exception in gsml3_registry_tick_procedures");
        return 0;
    }
}

// ── Session access ─────────────────────────────────────────────────────

GSML3_C_API uint32_t gsml3_session_tmsi(gsml3_session* s) {
    if (!s) return 0;
    const auto& id = sess(s)->context.identity();
    return id.isTMSI() ? id.tmsi() : 0;
}

GSML3_C_API void gsml3_session_set_tmsi(gsml3_session* s, uint32_t tmsi) {
    if (s) sess(s)->context.setTMSI(tmsi);
}

GSML3_C_API void gsml3_session_set_imsi(gsml3_session* s, const char* digits) {
    if (s && digits) sess(s)->context.setIMSI(digits);
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
    return sess(s)->timers.start(static_cast<L3TimerId>(timer_id)) ? 1 : 0;
}

GSML3_C_API void gsml3_session_timer_stop(gsml3_session* s, int timer_id) {
    if (s) sess(s)->timers.stop(static_cast<L3TimerId>(timer_id));
}

GSML3_C_API int gsml3_session_timer_running(gsml3_session* s, int timer_id) {
    return s && sess(s)->timers.isRunning(static_cast<L3TimerId>(timer_id)) ? 1 : 0;
}

GSML3_C_API size_t gsml3_session_transaction_pending(gsml3_session* s) {
    return s ? sess(s)->transactions.pendingCount() : 0;
}
