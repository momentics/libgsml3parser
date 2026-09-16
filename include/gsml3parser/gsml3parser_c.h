/* Copyright 2026 momentics <momentics@gmail.com>
 * Copyright libgsml3parser contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
 * libgsml3parser C ABI (FFI).
 *
 * A stable C interface over the C++20 library for FFI consumers (C, Python
 * ctypes/cffi, Rust, Go). The header is C89-clean (compiles with
 * `gcc -std=c89 -pedantic-errors` and MSVC /TC) and exposes no C++ types.
 *
 * ABI rules:
 *  - Handles are opaque. Owned handles (gsml3_config, gsml3_message,
 *    gsml3_rsl, gsml3_registry, gsml3_orchestrator, gsml3_lapdm_entity)
 *    are created by gsml3_*_new()/gsml3_parse_*() and released by the
 *    matching gsml3_*_free() (all NULL-safe). gsml3_session is a
 *    BORROWED pointer-sized handle owned by the registry: never free it;
 *    it is valid until the session is removed or the registry is freed.
 *  - Errors: functions returning a handle return NULL on error; action
 *    functions return an enum gsml3_error code (GSML3_OK on success);
 *    serializers return the number of bytes written, 0 on error or
 *    buffer-too-small. Details: gsml3_last_error() (thread-local, "" when
 *    none; valid until the next gsml3_* call on the same thread — copy it
 *    if you need it longer).
 *  - Strings: const char* results (names, gsml3_last_error) point to
 *    static or thread-local storage — do not free. char* results (hex)
 *    are allocated by the library — free with gsml3_free().
 *  - Buffers: all serializers take (uint8_t* out, size_t maxlen) and
 *    write into the caller's buffer (zero heap allocation on the hot
 *    path).
 *  - Threading: stateless functions (parse/serialize/builders) are
 *    thread-safe. Owned handles are single-thread (one thread per handle,
 *    or external synchronization). A gsml3_registry created with
 *    shard_count > 0 is thread-safe (per-shard locks inside).
 *  - No C++ exception ever crosses this boundary; every function is
 *    exception-safe.
 *  - Allocation note: a minority of message types carry std::vector /
 *    std::string fields (most RR, SMS, BCC/GCC messages); parsing them
 *    allocates (a property of the C++ API). Zero allocation is
 *    guaranteed for the variant as a whole and for the typical short
 *    messages.
 *
 * 3GPP: TS 24.008 / TS 44.018 (L3), TS 48.058 (A-bis RSL), TS 45.006
 * (LAPDm).
 */

#ifndef GSML3PARSER_C_H
#define GSML3PARSER_C_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Export macro: dllexport/dllimport on Windows shared builds, default
 * visibility on ELF shared builds, empty otherwise. */
#if defined(_WIN32) && defined(GSML3PARSER_SHARED)
#  if defined(GSML3PARSER_EXPORTS)
#    define GSML3_C_API __declspec(dllexport)
#  else
#    define GSML3_C_API __declspec(dllimport)
#  endif
#else
#  if defined(GSML3PARSER_SHARED) && defined(GSML3PARSER_EXPORTS)
#    define GSML3_C_API __attribute__((visibility("default")))
#  else
#    define GSML3_C_API
#  endif
#endif

/* ── Error model ─────────────────────────────────────────────────────── */

enum gsml3_error {
    GSML3_OK = 0,
    GSML3_ERR_INVALID_ARG = 1,
    GSML3_ERR_TRUNCATED = 2,
    GSML3_ERR_INVALID_PD = 3,
    GSML3_ERR_INVALID_MTI = 4,
    GSML3_ERR_LENGTH_MISMATCH = 5,
    GSML3_ERR_INVALID_IE = 6,
    GSML3_ERR_INVALID_VALUE = 7,
    GSML3_ERR_UNSUPPORTED = 8,
    GSML3_ERR_SOURCE_EXHAUSTED = 9,
    GSML3_ERR_NO_MEMORY = 10
};

/* Library version (e.g. "0.18.0"). Static storage; do not free. */
GSML3_C_API const char* gsml3_version(void);

/* Thread-local last error message ("" when none). Never NULL. Valid
 * until the next gsml3_* call on this thread; copy if needed longer. */
GSML3_C_API const char* gsml3_last_error(void);

/* Release any char* string returned by this API. NULL-safe. */
GSML3_C_API void gsml3_free(void* ptr);

/* ── Log levels (mirror gsml3parser::LogLevel 1:1) ───────────────────── */
enum gsml3_log_level {
    GSML3_LOG_EMERG = 0,
    GSML3_LOG_ALERT = 1,
    GSML3_LOG_CRIT = 2,
    GSML3_LOG_ERR = 3,
    GSML3_LOG_WARNING = 4,
    GSML3_LOG_NOTICE = 5,
    GSML3_LOG_INFO = 6,
    GSML3_LOG_DEBUG = 7
};

/* ── Protocol discriminators (mirror gsml3parser::L3PD 1:1) ──────────── */
enum gsml3_pd {
    GSML3_PD_GCC = 0x00,
    GSML3_PD_BCC = 0x01,
    GSML3_PD_CC = 0x03,
    GSML3_PD_MM = 0x05,
    GSML3_PD_RR = 0x06,
    GSML3_PD_GMM = 0x08,
    GSML3_PD_SMS = 0x09,
    GSML3_PD_SM = 0x0a,
    GSML3_PD_SS = 0x0b,
    GSML3_PD_LS = 0x0c,
    GSML3_PD_EXT = 0x0e,
    GSML3_PD_TST = 0x0f,
    GSML3_PD_UNDEFINED = -1
};

/* ── Mobile identity types (mirror gsml3parser::MobileIDType 1:1) ────── */
enum gsml3_id_type {
    GSML3_ID_NO_ID = 0,
    GSML3_ID_IMSI = 1,
    GSML3_ID_IMEI = 2,
    GSML3_ID_IMEISV = 3,
    GSML3_ID_TMSI = 4
};

/* ── Parser config ───────────────────────────────────────────────────── */
typedef struct gsml3_config gsml3_config;

/* Create a config with default settings (log level WARNING, lenient
 * framing). NULL on allocation failure. Free with gsml3_config_free. */
GSML3_C_API gsml3_config* gsml3_config_new(void);
/* Set the log level (GSML3_LOG_*); out-of-range values are ignored. */
GSML3_C_API void gsml3_config_set_log_level(gsml3_config* c, int level);
/* Enable strict framing: reject a frame whose message does not consume
 * the entire input. */
GSML3_C_API void gsml3_config_set_strict_framing(gsml3_config* c, int on);
/* Free the config. NULL-safe. */
GSML3_C_API void gsml3_config_free(gsml3_config* c);

/* ── L3 message handle ───────────────────────────────────────────────── */
typedef struct gsml3_message gsml3_message;

/* Parse raw L3 bytes (header + body). Returns NULL on error (see
 * gsml3_last_error). cfg may be NULL (default config). */
GSML3_C_API gsml3_message* gsml3_parse_l3(const uint8_t* data, size_t len,
                                          const gsml3_config* cfg);
/* Parse a hex string (spaces allowed, e.g. "60 0D 00"). */
GSML3_C_API gsml3_message* gsml3_parse_l3_hex(const char* hex,
                                              const gsml3_config* cfg);
/* High-throughput reparse into an existing handle (zero extra allocation
 * for typical messages). Returns GSML3_OK or an error code; on error the
 * handle keeps its previous content. */
GSML3_C_API int gsml3_parse_l3_into(gsml3_message* msg, const uint8_t* data,
                                    size_t len, const gsml3_config* cfg);
/* Free the message. NULL-safe. */
GSML3_C_API void gsml3_message_free(gsml3_message* msg);

/* Message metadata. name: static storage, valid while the handle is
 * alive. All accessors are NULL-safe ("" / -1 / 0). */
GSML3_C_API const char* gsml3_message_name(const gsml3_message* msg);
/* Protocol discriminator (GSML3_PD_*), -1 if msg is NULL. */
GSML3_C_API int gsml3_message_pd(const gsml3_message* msg);
/* Message type identifier (0..255; RR short messages 256..511), -1 if NULL. */
GSML3_C_API int gsml3_message_mti(const gsml3_message* msg);
/* Transaction identifier for CC/SS messages (0..7), 0 otherwise. */
GSML3_C_API int gsml3_message_ti(const gsml3_message* msg);

/* Serialize into the caller's buffer. Returns bytes written; 0 on error
 * or buffer too small. */
GSML3_C_API size_t gsml3_message_write(const gsml3_message* msg,
                                       uint8_t* out, size_t maxlen);
/* Hex serialization (lowercase, no spaces). Allocated by the library;
 * free with gsml3_free(). NULL on error. */
GSML3_C_API char* gsml3_message_hex(const gsml3_message* msg);

/* ── RSL (A-bis, TS 48.058) ──────────────────────────────────────────── */
typedef struct gsml3_rsl gsml3_rsl;

/* Parse an RSL frame. The handle owns a copy of the input: the IE and
 * L3 views reference that copy (the caller's buffer may be freed
 * immediately). Returns NULL on error. */
GSML3_C_API gsml3_rsl* gsml3_rsl_parse(const uint8_t* data, size_t len);
/* Free the RSL handle. NULL-safe. */
GSML3_C_API void gsml3_rsl_free(gsml3_rsl* rsl);

/* Message name ("DATA_REQ", "CHAN_ACTIV", ...). Static storage. */
GSML3_C_API const char* gsml3_rsl_name(const gsml3_rsl* rsl);
/* 7-bit discriminator (direction bit stripped), -1 if NULL. */
GSML3_C_API int gsml3_rsl_discriminator(const gsml3_rsl* rsl);
/* Message type byte within the discriminator, -1 if NULL. */
GSML3_C_API int gsml3_rsl_msg_type(const gsml3_rsl* rsl);
/* Channel number, -1 if NULL. */
GSML3_C_API int gsml3_rsl_chan_nr(const gsml3_rsl* rsl);
/* LAPDm link identifier (RLL), -1 if NULL. */
GSML3_C_API int gsml3_rsl_link_id(const gsml3_rsl* rsl);
/* Direction: 1 = BTS->BSC, 0 = BSC->BTS; -1 if NULL. */
GSML3_C_API int gsml3_rsl_bts_to_bsc(const gsml3_rsl* rsl);
/* 1 if the frame carries an L3 payload, 0 otherwise. */
GSML3_C_API int gsml3_rsl_has_l3(const gsml3_rsl* rsl);
/* L3 payload view into the handle's copy (valid while the handle is
 * alive); NULL when there is no L3. *len is set to the payload size. */
GSML3_C_API const uint8_t* gsml3_rsl_l3(const gsml3_rsl* rsl, size_t* len);
/* Number of parsed information elements. */
GSML3_C_API size_t gsml3_rsl_ie_count(const gsml3_rsl* rsl);
/* IE at `index`: *type = IE type code, *len = value length, *val points
 * into the handle's copy. GSML3_OK or GSML3_ERR_INVALID_ARG. */
GSML3_C_API int gsml3_rsl_ie_get(const gsml3_rsl* rsl, size_t index,
                                 uint8_t* type, size_t* len,
                                 const uint8_t** val);

/* RSL builders (zero-alloc; write into the caller's buffer). Return
 * bytes written, 0 on error or buffer too small. cause: RSLErrorCause
 * value (see include/gsml3parser/abis/rsl_types.h). */
GSML3_C_API size_t gsml3_rsl_build_data_req(uint8_t* out, size_t maxlen,
    uint8_t chan_nr, uint8_t link_id, const uint8_t* l3, size_t l3_len);
GSML3_C_API size_t gsml3_rsl_build_data_ind(uint8_t* out, size_t maxlen,
    uint8_t chan_nr, uint8_t link_id, const uint8_t* l3, size_t l3_len);
GSML3_C_API size_t gsml3_rsl_build_unit_data_req(uint8_t* out, size_t maxlen,
    uint8_t chan_nr, uint8_t link_id, const uint8_t* l3, size_t l3_len);
GSML3_C_API size_t gsml3_rsl_build_unit_data_ind(uint8_t* out, size_t maxlen,
    uint8_t chan_nr, uint8_t link_id, const uint8_t* l3, size_t l3_len);
GSML3_C_API size_t gsml3_rsl_build_chan_activ_ack(uint8_t* out, size_t maxlen,
    uint8_t chan_nr, uint16_t frame_number);
GSML3_C_API size_t gsml3_rsl_build_chan_activ_nack(uint8_t* out, size_t maxlen,
    uint8_t chan_nr, int cause);
GSML3_C_API size_t gsml3_rsl_build_rf_chan_rel_ack(uint8_t* out, size_t maxlen,
    uint8_t chan_nr);
GSML3_C_API size_t gsml3_rsl_build_conn_fail(uint8_t* out, size_t maxlen,
    uint8_t chan_nr, int cause);
GSML3_C_API size_t gsml3_rsl_build_meas_res(uint8_t* out, size_t maxlen,
    uint8_t chan_nr, uint8_t meas_nr, int8_t rxlev, int8_t rxqual,
    const uint8_t* l1, size_t l1_len);
GSML3_C_API size_t gsml3_rsl_build_hando_det(uint8_t* out, size_t maxlen,
    uint8_t chan_nr, uint8_t access_delay);
GSML3_C_API size_t gsml3_rsl_build_ccch_load_ind(uint8_t* out, size_t maxlen,
    uint8_t chan_nr, uint16_t paging_load, uint16_t rach_total,
    uint16_t rach_busy, uint16_t rach_access);
GSML3_C_API size_t gsml3_rsl_build_chan_rqd(uint8_t* out, size_t maxlen,
    uint8_t chan_nr, uint8_t ra, uint8_t t1p, uint8_t t2, uint8_t t3,
    uint8_t access_delay);
GSML3_C_API size_t gsml3_rsl_build_delete_ind(uint8_t* out, size_t maxlen,
    uint8_t chan_nr, const uint8_t* info, size_t info_len);

/* ── LAPDm (GSM 04.06) ───────────────────────────────────────────────── */

/* Frame formats (mirror gsml3parser::lapdm::LAPDmControlFormat 1:1). */
enum gsml3_lapdm_format {
    GSML3_LAPDM_FMT_I = 0,
    GSML3_LAPDM_FMT_S = 1,
    GSML3_LAPDM_FMT_U = 2
};
/* U-frame types (mirror gsml3parser::lapdm::LAPDmUFrameType 1:1). */
enum gsml3_lapdm_u_type {
    GSML3_LAPDM_U_UI = 0x03,
    GSML3_LAPDM_U_SABME = 0x2F,
    GSML3_LAPDM_U_UA = 0x63,
    GSML3_LAPDM_U_DM = 0x0F,
    GSML3_LAPDM_U_DISC = 0x08
};
/* S-frame types (mirror gsml3parser::lapdm::LAPDmSFrameType 1:1). */
enum gsml3_lapdm_s_type {
    GSML3_LAPDM_S_RR = 0x01,
    GSML3_LAPDM_S_REJ = 0x0D
};
/* LAPDm FSM states (mirror gsml3parser::LAPDmState 1:1). */
enum gsml3_lapdm_state {
    GSML3_LAPDM_STATE_UNUSED = 0,
    GSML3_LAPDM_STATE_LINK_RELEASED = 1,
    GSML3_LAPDM_STATE_AWAITING_ESTABLISH = 2,
    GSML3_LAPDM_STATE_AWAITING_RELEASE = 3,
    GSML3_LAPDM_STATE_LINK_ESTABLISHED = 4,
    GSML3_LAPDM_STATE_CONTENTION_RESOLUTION = 5
};
/* SAPI values (mirror gsml3parser::SAPI 1:1). */
enum gsml3_sapi {
    GSML3_SAPI0 = 0,
    GSML3_SAPI3 = 3,
    GSML3_SAPI0_SACCH = 4,
    GSML3_SAPI3_SACCH = 7
};
/* Interlayer primitives (mirror gsml3parser::Primitive 1:1). */
enum gsml3_primitive {
    GSML3_PRIM_L2_DATA = 1,
    GSML3_PRIM_L3_DATA = 2,
    GSML3_PRIM_L3_DATA_CONFIRM = 3,
    GSML3_PRIM_L3_UNIT_DATA = 4,
    GSML3_PRIM_L3_ESTABLISH_REQUEST = 5,
    GSML3_PRIM_L3_ESTABLISH_INDICATION = 6,
    GSML3_PRIM_L3_ESTABLISH_CONFIRM = 7,
    GSML3_PRIM_L3_RELEASE_REQUEST = 8,
    GSML3_PRIM_L3_RELEASE_CONFIRM = 9,
    GSML3_PRIM_L3_HARDRELEASE_REQUEST = 10,
    GSML3_PRIM_MDL_ERROR_INDICATION = 11,
    GSML3_PRIM_L3_RELEASE_INDICATION = 12,
    GSML3_PRIM_PH_CONNECT = 13,
    GSML3_PRIM_HANDOVER_ACCESS = 14
};

/* Decoded LAPDm frame. ZERO-COPY: info points into the input buffer and
 * is valid while that buffer is alive. */
typedef struct gsml3_lapdm_frame_info {
    int format;       /* GSML3_LAPDM_FMT_* */
    int u_type;       /* GSML3_LAPDM_U_* when format == U, else -1 */
    int s_type;       /* GSML3_LAPDM_S_* when format == S, else -1 */
    uint8_t nr;       /* receive sequence number (I/S frames) */
    uint8_t ns;       /* send sequence number (I frames) */
    int pf;           /* Poll/Final bit */
    int m_bit;        /* message-complete bit (I frames) */
    int sapi;         /* GSML3_SAPI* value */
    int command;      /* C/R bit: 1 = command, 0 = response */
    const uint8_t* info;  /* info field (NULL when absent) */
    size_t info_len;
} gsml3_lapdm_frame_info;

/* Decode a raw LAPDm frame (address + control [+ length + info]).
 * GSML3_OK or an error code. */
GSML3_C_API int gsml3_lapdm_frame_decode(const uint8_t* data, size_t len,
                                         gsml3_lapdm_frame_info* out);

/* Entity callbacks: fn + user, invoked synchronously. The l3/frame
 * spans are valid only DURING the callback: transmit or copy
 * synchronously, never retain them. */
typedef void (*gsml3_lapdm_l3_cb)(int sapi, int primitive,
                                  const uint8_t* l3, size_t l3_len, void* user);
typedef void (*gsml3_lapdm_l1_cb)(const uint8_t* frame, size_t frame_len,
                                  void* user);

typedef struct gsml3_lapdm_entity gsml3_lapdm_entity;

/* profile: 0 = SDCCH (N201=20, N200=23, T200=900ms), 1 = SACCH
 * (N201=18, N200=5, T200=3600ms), 2 = FACCH (N201=20, N200=34,
 * T200=900ms). Callbacks may be NULL. Invalid profile returns NULL. */
GSML3_C_API gsml3_lapdm_entity* gsml3_lapdm_entity_new(int profile,
    gsml3_lapdm_l3_cb l3_cb, gsml3_lapdm_l1_cb l1_cb, void* user);
/* Free the entity. NULL-safe. */
GSML3_C_API void gsml3_lapdm_entity_free(gsml3_lapdm_entity* e);
/* Open the entity (transition to LinkReleased). command_bit: 1 = BTS
 * side (C/R=1), 0 = MS side (C/R=0). */
GSML3_C_API void gsml3_lapdm_entity_open(gsml3_lapdm_entity* e, int sapi,
                                         int command_bit);
/* Feed a raw LAPDm frame from L1 into the FSM. */
GSML3_C_API void gsml3_lapdm_entity_receive(gsml3_lapdm_entity* e,
                                            const uint8_t* frame, size_t len);
/* Send L3 data via a UI frame (works in any state). GSML3_OK or error. */
GSML3_C_API int gsml3_lapdm_entity_send_ui(gsml3_lapdm_entity* e, int sapi,
                                           const uint8_t* l3, size_t l3_len);
/* Send L3 data via I-frames (segmented if needed; requires an
 * established link). GSML3_OK or error. */
GSML3_C_API int gsml3_lapdm_entity_send_data(gsml3_lapdm_entity* e,
                                             const uint8_t* l3, size_t l3_len);
/* Send SABME (link establishment; requires LinkReleased). */
GSML3_C_API int gsml3_lapdm_entity_send_sabme(gsml3_lapdm_entity* e);
/* Send DISC (link release; requires an established link). */
GSML3_C_API int gsml3_lapdm_entity_send_disc(gsml3_lapdm_entity* e);
/* Immediate transition to LinkReleased without sending frames. */
GSML3_C_API void gsml3_lapdm_entity_hard_release(gsml3_lapdm_entity* e);
/* Advance T200 by elapsed_ms; returns 1 if a retransmission or abnormal
 * release occurred, 0 otherwise. */
GSML3_C_API int gsml3_lapdm_entity_tick_t200(gsml3_lapdm_entity* e,
                                             uint32_t elapsed_ms);
/* Current FSM state (GSML3_LAPDM_STATE_*). */
GSML3_C_API int gsml3_lapdm_entity_state(const gsml3_lapdm_entity* e);
/* 1 if the link is established (LinkEstablished or
 * ContentionResolution), 0 otherwise. */
GSML3_C_API int gsml3_lapdm_entity_is_established(const gsml3_lapdm_entity* e);
/* Statistics counters. */
GSML3_C_API unsigned gsml3_lapdm_entity_frames_sent(const gsml3_lapdm_entity* e);
GSML3_C_API unsigned gsml3_lapdm_entity_frames_received(const gsml3_lapdm_entity* e);
GSML3_C_API unsigned gsml3_lapdm_entity_retransmissions(const gsml3_lapdm_entity* e);

/* ── BTS stack: registry / session ───────────────────────────────────── */

/* Registry. shard_count: 0 = plain single-threaded registry (event-loop
 * model); 4/8/16/32 = sharded thread-safe registry (per-shard locks).
 * Any other value returns NULL + GSML3_ERR_INVALID_ARG. */
typedef struct gsml3_registry gsml3_registry;

/* Session: a BORROWED pointer-sized handle (zero allocations on
 * create/find). Owned by the registry: do NOT free it; it is valid until
 * the session is removed or the registry is freed. Direct gsml3_session_*
 * calls operate on the session WITHOUT a registry lock: the caller owns
 * per-session synchronization (one thread per session; never use a
 * session concurrently with gsml3_registry_remove of it). Note:
 * gsml3_session_set_tmsi changes the session identity only; it does not
 * update the registry TMSI index (remove + create to re-index). */
typedef struct gsml3_session gsml3_session;

/* Create the registry. NULL on invalid shard_count or allocation
 * failure. */
GSML3_C_API gsml3_registry* gsml3_registry_new(int shard_count);
/* Free the registry and all its sessions. NULL-safe. */
GSML3_C_API void gsml3_registry_free(gsml3_registry* r);
/* Pre-size the indexes for the expected population (cold path). */
GSML3_C_API void gsml3_registry_reserve(gsml3_registry* r, size_t expected);
/* Number of active sessions. */
GSML3_C_API size_t gsml3_registry_count(const gsml3_registry* r);

/* Create a session; NULL on duplicate key (or allocation failure).
 * create_by_imsi is not supported by sharded registries (NULL +
 * gsml3_last_error explains). */
GSML3_C_API gsml3_session* gsml3_registry_create_by_tmsi(gsml3_registry* r,
                                                          uint32_t tmsi);
/* imsi: BCD digit string (e.g. "244051234567890"). */
GSML3_C_API gsml3_session* gsml3_registry_create_by_imsi(gsml3_registry* r,
                                                          const char* imsi);
/* Lookups; NULL when not found. */
GSML3_C_API gsml3_session* gsml3_registry_find_by_tmsi(gsml3_registry* r,
                                                        uint32_t tmsi);
GSML3_C_API gsml3_session* gsml3_registry_find_by_imsi(gsml3_registry* r,
                                                        const char* imsi);
GSML3_C_API gsml3_session* gsml3_registry_find_by_link(gsml3_registry* r,
    uint8_t trx, uint8_t ts, uint8_t lapdm_link);
/* Remove a session. 1 = removed, 0 = not found / unowned. */
GSML3_C_API int gsml3_registry_remove(gsml3_registry* r, gsml3_session* s);
/* Remove all sessions. Not supported by sharded registries (no-op +
 * gsml3_last_error explains). */
GSML3_C_API void gsml3_registry_clear(gsml3_registry* r);
/* Assign a channel and update the link index. ch_type:
 * gsml3parser::ChannelType value (see include/gsml3parser/types.h). */
GSML3_C_API void gsml3_registry_assign_channel(gsml3_registry* r,
    gsml3_session* s, int ch_type, uint8_t trx, uint8_t ts, uint16_t arfcn,
    uint8_t lapdm_link);
GSML3_C_API void gsml3_registry_release_channel(gsml3_registry* r,
                                                gsml3_session* s);

/* L3 timer IDs (mirror L3TimerId 1:1). */
enum gsml3_timer {
    GSML3_TIMER_T3101 = 0,
    GSML3_TIMER_T3102 = 1,
    GSML3_TIMER_T3103 = 2,
    GSML3_TIMER_T3106 = 3,
    GSML3_TIMER_T3108 = 4,
    GSML3_TIMER_T3109 = 5,
    GSML3_TIMER_T3111 = 6,
    GSML3_TIMER_T3112 = 7,
    GSML3_TIMER_T3113 = 8,
    GSML3_TIMER_T3310 = 9,
    GSML3_TIMER_T3311 = 10,
    GSML3_TIMER_T3312 = 11,
    GSML3_TIMER_T3314 = 12,
    GSML3_TIMER_T3315 = 13,
    GSML3_TIMER_T3320 = 14,
    GSML3_TIMER_T3321 = 15,
    GSML3_TIMER_T3322 = 16,
    GSML3_TIMER_T3334 = 17,
    GSML3_TIMER_T3395 = 18,
    GSML3_TIMER_UNKNOWN = 0xFF
};

/* Timer expiry event (session + timer ID). */
typedef struct gsml3_timer_expiry {
    gsml3_session* session;
    int timer_id;  /* GSML3_TIMER_* */
} gsml3_timer_expiry;

/* Tick all session timers (O(active)). expired_out: caller buffer of
 * `cap` events; returns the number written. Events that do not fit are
 * re-armed (1 ms) and reported on a later tick — never dropped. */
GSML3_C_API size_t gsml3_registry_tick_timers(gsml3_registry* r,
    uint32_t delta_ms, gsml3_timer_expiry* expired_out, size_t cap);
/* Tick all active procedures (O(active)); returns the number of
 * procedures that timed out. */
GSML3_C_API size_t gsml3_registry_tick_procedures(gsml3_registry* r,
                                                  uint32_t delta_ms);

/* Session access (MSContext subset + timers + transactions). NULL-safe:
 * setters are no-ops, getters return 0. */
/* TMSI of the session identity (0 when the identity is not a TMSI). */
GSML3_C_API uint32_t gsml3_session_tmsi(gsml3_session* s);
GSML3_C_API void gsml3_session_set_tmsi(gsml3_session* s, uint32_t tmsi);
/* digits: BCD digit string. */
GSML3_C_API void gsml3_session_set_imsi(gsml3_session* s, const char* digits);
GSML3_C_API int gsml3_session_is_registered(gsml3_session* s);
GSML3_C_API void gsml3_session_set_registered(gsml3_session* s, int v);
GSML3_C_API int gsml3_session_is_authenticated(gsml3_session* s);
GSML3_C_API void gsml3_session_set_authenticated(gsml3_session* s, int v);
GSML3_C_API int gsml3_session_is_ciphered(gsml3_session* s);
GSML3_C_API void gsml3_session_set_ciphered(gsml3_session* s, int v);
/* timer_id: GSML3_TIMER_*. start returns 1 on a fresh start, 0 on a
 * restart or invalid ID. */
GSML3_C_API int gsml3_session_timer_start(gsml3_session* s, int timer_id);
GSML3_C_API void gsml3_session_timer_stop(gsml3_session* s, int timer_id);
GSML3_C_API int gsml3_session_timer_running(gsml3_session* s, int timer_id);
/* Number of pending transactions. */
GSML3_C_API size_t gsml3_session_transaction_pending(gsml3_session* s);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* GSML3PARSER_C_H */
