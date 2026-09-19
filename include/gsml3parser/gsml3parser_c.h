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
 *    buffer-too-small. Details: gsml3_last_error() (thread-local message,
 *    "" when none) and gsml3_last_error_code() (the gsml3_error code of the
 *    last failure). Every function that performs an operation clears any
 *    pending error on entry, so a call that succeeds always leaves
 *    gsml3_last_error_code() == GSML3_OK and a call that fails reports a
 *    fresh error; read them right after a failed call and copy the string
 *    if you need it longer. The state observers (gsml3_last_error,
 *    gsml3_last_error_code) and the release functions (gsml3_*_free) never
 *    modify the pending error, so they may be used while inspecting one.
 *  - Validation: integer parameters that mirror C++ enum values (causes,
 *    SAPIs, channel types, tokens, timer IDs, ...) and digit strings (IMSI,
 *    called-party numbers, LAI components) are range-checked against their
 *    protocol domain; fixed-width frame fields (timeslot number, time slot
 *    code, ARFCN, timing advance, request-reference timings) are
 *    range-checked against the on-wire field width. Invalid input fails
 *    with GSML3_ERR_INVALID_ARG (or NULL / 0 where that is the error form):
 *    no frame is ever built or sent with an out-of-domain field and digit
 *    strings are never truncated silently. The reserved all-zero TMSI is
 *    rejected by every session-keying and TMSI-carrying function.
 *  - ABI versioning: GSML3_ABI_VERSION / gsml3_abi_version() identify this
 *    header's C ABI. Public enums only grow values, public structs only grow
 *    trailing fields, functions are only added; check the value at startup
 *    when linking against a prebuilt binary of an older or newer build.
 *  - Strings: const char* results (names, gsml3_last_error) point to
 *    static or thread-local storage — do not free. char* results (hex)
 *    are allocated by the library — free with gsml3_free().
 *  - Buffers: all serializers take (uint8_t* out, size_t maxlen) and
 *    write into the caller's buffer (zero heap allocation on the hot
 *    path).
 *  - Threading: stateless functions (parse/serialize/builders) are
 *    thread-safe. Owned handles are single-thread (one thread per handle,
 *    or external synchronization). A gsml3_registry created with
 *    shard_count > 0 makes its registry-mediated calls thread-safe (per-
 *    shard locks); direct gsml3_session_* access runs WITHOUT the registry
 *    lock in either flavor, so it is the caller's to synchronize: keep one
 *    thread per session and never use a session concurrently with
 *    gsml3_registry_remove() of it.
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

/* C ABI revision. Bumped only when the layout or calling contract of a
 * public enum or struct changes in an incompatible way. */
#define GSML3_ABI_VERSION 1

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
    GSML3_ERR_NO_MEMORY = 10,
    /* The caller's output buffer was too small; enlarge it and retry. */
    GSML3_ERR_BUFFER_TOO_SMALL = 11,
    /* Internal failure (an unexpected condition inside the library). */
    GSML3_ERR_INTERNAL = 12,
    /* A key that must be unique is already present (e.g. creating a session
     * for a TMSI or IMSI that already has one). */
    GSML3_ERR_DUPLICATE = 13
};

/* Library version (e.g. "0.18.0"). Static storage; do not free. */
GSML3_C_API const char* gsml3_version(void);

/* C ABI revision of this header (== GSML3_ABI_VERSION). */
GSML3_C_API unsigned gsml3_abi_version(void);

/* Thread-local last error message ("" when none). Never NULL. Valid until
 * a later call clears or replaces it; copy if needed longer. */
GSML3_C_API const char* gsml3_last_error(void);

/* Thread-local last error code (GSML3_OK when there is no pending error).
 * Paired with gsml3_last_error(); lets callers branch on the failure class
 * programmatically instead of comparing message text. */
GSML3_C_API int gsml3_last_error_code(void);

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

/* Exact wire size of the serialized message in bytes (zero allocation).
 * A buffer of this size is guaranteed to be accepted by
 * gsml3_message_write(). Returns 0 when msg is NULL. */
GSML3_C_API size_t gsml3_message_size(const gsml3_message* msg);
/* Serialize into the caller's buffer. Returns bytes written; 0 on error
 * or buffer too small (see gsml3_last_error_code():
 * GSML3_ERR_BUFFER_TOO_SMALL means "enlarge and retry"). */
GSML3_C_API size_t gsml3_message_write(const gsml3_message* msg,
                                       uint8_t* out, size_t maxlen);
/* Hex serialization (lowercase, no spaces). Allocated by the library;
 * free with gsml3_free(). NULL on error. */
GSML3_C_API char* gsml3_message_hex(const gsml3_message* msg);
/* Human-readable dump: message name on the first line followed by the
 * information-element text of every field (all 236 message types).
 * Allocated by the library; free with gsml3_free(). NULL when msg is NULL. */
GSML3_C_API char* gsml3_message_dump(const gsml3_message* msg);

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
  * value (see include/gsml3parser/abis/rsl_types.h); the request-reference
  * timing fields of gsml3_rsl_build_chan_rqd; both range-checked:
  * out-of-domain values fail with GSML3_ERR_INVALID_ARG before a frame is
  * produced. */
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

/* Entity callbacks: fn + user, invoked synchronously from
 * gsml3_lapdm_entity_receive() / gsml3_lapdm_entity_send_*. The l3/frame
 * spans are valid only DURING the callback: transmit or copy
 * synchronously, never retain them. Do not free the owning entity (or any
 * gsml3_* handle it may reference) from within its own callbacks; re-enter
 * gsml3_* calls that only observe unrelated state. */
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
/* Open the entity (transition to LinkReleased). sapi: 0..15 (gsml3_sapi
 * names the common ones; values outside 0..15 are rejected — an invalid
 * value sets the thread-local error and leaves the entity in its previous
 * state). command_bit: 1 = BTS side (C/R=1), 0 = MS side (C/R=0). */
GSML3_C_API void gsml3_lapdm_entity_open(gsml3_lapdm_entity* e, int sapi,
                                         int command_bit);
/* Feed a raw LAPDm frame from L1 into the FSM. */
GSML3_C_API void gsml3_lapdm_entity_receive(gsml3_lapdm_entity* e,
                                            const uint8_t* frame, size_t len);
 /* Send L3 data via a UI frame (no link establishment required, so this
  * works in any state). sapi selects the address-octet SAPI of the emitted
  * UI frame and may differ from the SAPI given to gsml3_lapdm_entity_open()
  * (which alone owns the link FSM); out-of-range values (not 0..15) are
  * rejected before encoding. GSML3_OK or error code. */
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
 * release occurred, 0 otherwise, -1 on an internal error (see
 * gsml3_last_error()). */
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

 /* Create a session. Returns NULL on error; gsml3_last_error_code()
  * distinguishes: reserved TMSI (GSML3_ERR_INVALID_ARG, the all-zero TMSI
  * is rejected in both registry flavors per TS 24.008), a key that already
  * has a session (GSML3_ERR_DUPLICATE), or allocation failure
  * (GSML3_ERR_NO_MEMORY). */
 GSML3_C_API gsml3_session* gsml3_registry_create_by_tmsi(gsml3_registry* r,
                                                           uint32_t tmsi);
 /* imsi: non-empty ASCII digit string of at most 15 digits (e.g.
  * "244051234567890"). The session is keyed by an auto-assigned TMSI,
  * readable via gsml3_session_assigned_tmsi(). Not supported by sharded
  * registries (NULL + GSML3_ERR_UNSUPPORTED); on a plain registry the error
  * codes follow the same rules as create_by_tmsi, plus
  * GSML3_ERR_NO_MEMORY when no free TMSI remains for auto-assignment. */
 GSML3_C_API gsml3_session* gsml3_registry_create_by_imsi(gsml3_registry* r,
                                                           const char* imsi);
/* Lookups; NULL when not found. */
GSML3_C_API gsml3_session* gsml3_registry_find_by_tmsi(gsml3_registry* r,
                                                        uint32_t tmsi);
GSML3_C_API gsml3_session* gsml3_registry_find_by_imsi(gsml3_registry* r,
                                                        const char* imsi);
/* Look up a session by the channel it was assigned to: the link index is
 * keyed on (trx number, timeslot, LAPDm link id) — the ARFCN of the
 * assigned channel is stored with the session but not part of the key. */
GSML3_C_API gsml3_session* gsml3_registry_find_by_link(gsml3_registry* r,
    uint8_t trx_number, uint8_t timeslot, uint8_t lapdm_link);
 /* Remove a session. 1 = removed, 0 = not found / unowned. */
 GSML3_C_API int gsml3_registry_remove(gsml3_registry* r, gsml3_session* s);
 /* Remove all sessions. Not supported by sharded registries (no-op +
  * GSML3_ERR_UNSUPPORTED). */
 GSML3_C_API void gsml3_registry_clear(gsml3_registry* r);
 /* Assign a channel and update the link index. s must belong to r: for a
  * session owned by another registry (or not owned at all) the call is a
  * no-op that sets GSML3_ERR_INVALID_ARG, so one registry's link index can
  * never reference a session it does not control. ch_type:
  * gsml3parser::ChannelType value (see include/gsml3parser/types.h),
  * range-checked like every other mirrored enum parameter. */
 GSML3_C_API void gsml3_registry_assign_channel(gsml3_registry* r,
     gsml3_session* s, int ch_type, uint8_t trx, uint8_t ts, uint16_t arfcn,
     uint8_t lapdm_link);
 /* Release the channel and remove it from the link index. Same ownership
  * rules as gsml3_registry_assign_channel. */
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
 * `cap` events; returns the number written. The buffer need not be
 * pre-zeroed: every written event is fully initialized. Events that do
 * not fit are re-armed (1 ms) and reported on a later tick — never
 * dropped. */
GSML3_C_API size_t gsml3_registry_tick_timers(gsml3_registry* r,
    uint32_t delta_ms, gsml3_timer_expiry* expired_out, size_t cap);
/* Tick all active procedures (O(active)); returns the number of
 * procedures that timed out. */
GSML3_C_API size_t gsml3_registry_tick_procedures(gsml3_registry* r,
                                                  uint32_t delta_ms);

/* Session access (MSContext subset + timers + transactions). NULL-safe:
 * setters are no-ops, getters return 0. */
/* TMSI of the session identity (0 when the identity is not a TMSI — in
 * particular for sessions created by IMSI). */
GSML3_C_API uint32_t gsml3_session_tmsi(gsml3_session* s);
/* TMSI under which the session is keyed in its owning registry; for
 * sessions created with gsml3_registry_create_by_imsi this is the
 * auto-assigned TMSI (use it with gsml3_registry_find_by_tmsi). 0 when
 * the session does not belong to a registry. */
GSML3_C_API uint32_t gsml3_session_assigned_tmsi(gsml3_session* s);
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
 * restart or an out-of-range ID (which also sets the thread-local error). */
GSML3_C_API int gsml3_session_timer_start(gsml3_session* s, int timer_id);
GSML3_C_API void gsml3_session_timer_stop(gsml3_session* s, int timer_id);
GSML3_C_API int gsml3_session_timer_running(gsml3_session* s, int timer_id);
/* Number of pending transactions. */
GSML3_C_API size_t gsml3_session_transaction_pending(gsml3_session* s);

/* ── BTS stack: orchestrator / responses ─────────────────────────────── */

/* Step actions (mirror ProcedureStepResult::Action 1:1). */
enum gsml3_action {
    GSML3_ACTION_CONTINUE = 0,
    GSML3_ACTION_SEND_RESPONSE = 1,
    GSML3_ACTION_WAITING_EXTERNAL = 2,
    GSML3_ACTION_COMPLETED = 3,
    GSML3_ACTION_FAILED = 4
};
/* Procedure lifecycle states (mirror procedure::ProcedureState 1:1). */
enum gsml3_state {
    GSML3_STATE_INITIATED = 0,
    GSML3_STATE_IN_PROGRESS = 1,
    GSML3_STATE_WAITING_EXTERNAL = 2,
    GSML3_STATE_COMPLETED = 3,
    GSML3_STATE_FAILED = 4,
    GSML3_STATE_TIMED_OUT = 5
};
/* Procedure types (mirror procedure::ProcedureType 1:1). */
enum gsml3_proc_type {
    GSML3_PROC_LOCATION_UPDATE = 0x01,
    GSML3_PROC_AUTHENTICATION = 0x02,
    GSML3_PROC_CIPHERING_MODE = 0x03,
    GSML3_PROC_CALL_SETUP_MO = 0x04,
    GSML3_PROC_CALL_SETUP_MT = 0x05,
    GSML3_PROC_CHANNEL_ASSIGNMENT = 0x06,
    GSML3_PROC_HANDOVER = 0x07,
    GSML3_PROC_PAGING = 0x08,
    GSML3_PROC_CM_SERVICE_REQUEST = 0x09,
    GSML3_PROC_IMSI_DETACH = 0x0A,
    GSML3_PROC_CALL_RELEASE = 0x0B,
    GSML3_PROC_PERIODIC_LOCATION_UPDATE = 0x0C,
    GSML3_PROC_UNKNOWN = 0xFF
};
/* Response tokens (mirror ResponseToken 1:1). */
enum gsml3_token {
    GSML3_TOKEN_NONE = 0,
    GSML3_TOKEN_IMMEDIATE_ASSIGNMENT = 1,
    GSML3_TOKEN_ASSIGNMENT_COMMAND = 2,
    GSML3_TOKEN_CHANNEL_RELEASE = 3,
    GSML3_TOKEN_CIPHERING_MODE_COMMAND = 4,
    GSML3_TOKEN_PHYSICAL_INFORMATION = 5,
    GSML3_TOKEN_HANDOVER_COMMAND = 6,
    GSML3_TOKEN_PAGING_REQUEST_TYPE1 = 7,
    GSML3_TOKEN_PAGING_REQUEST_TYPE2 = 8,
    GSML3_TOKEN_PAGING_REQUEST_TYPE3 = 9,
    GSML3_TOKEN_CM_SERVICE_ACCEPT = 10,
    GSML3_TOKEN_CM_SERVICE_REJECT = 11,
    GSML3_TOKEN_IDENTITY_REQUEST = 12,
    GSML3_TOKEN_AUTHENTICATION_REQUEST = 13,
    GSML3_TOKEN_LOCATION_UPDATING_ACCEPT = 14,
    GSML3_TOKEN_LOCATION_UPDATING_REJECT = 15,
    GSML3_TOKEN_TMSI_REALLOCATION_COMMAND = 16,
    GSML3_TOKEN_CALL_PROCEEDING = 17,
    GSML3_TOKEN_ALERTING = 18,
    GSML3_TOKEN_CONNECT = 19,
    GSML3_TOKEN_CONNECT_ACKNOWLEDGE = 20,
    GSML3_TOKEN_DISCONNECT = 21,
    GSML3_TOKEN_RELEASE = 22,
    GSML3_TOKEN_RELEASE_COMPLETE = 23,
    GSML3_TOKEN_SETUP = 24
};

/* Result of one orchestrator step. error: GSML3_OK when the step was
 * executed; GSML3_ERR_INVALID_ARG / GSML3_ERR_INTERNAL otherwise — when
 * error != GSML3_OK the remaining fields carry no step information.
 * reason: a thread-local copy of the C++ string_view (whose lifetime is
 * not guaranteed beyond the call); valid until the next gsml3_* call on
 * this thread, NULL when empty. */
typedef struct gsml3_step_result {
    int action;          /* GSML3_ACTION_* */
    int response_token;  /* GSML3_TOKEN_* (GSML3_TOKEN_NONE when none) */
    int final_state;     /* GSML3_STATE_* */
    int final_type;      /* GSML3_PROC_* */
    const char* reason;
    int error;           /* gsml3_error code; GSML3_OK on a real step */
} gsml3_step_result;

/* One orchestrator per session chain. Owns the active procedure. */
typedef struct gsml3_orchestrator gsml3_orchestrator;

GSML3_C_API gsml3_orchestrator* gsml3_orchestrator_new(void);
/* Free the orchestrator. NULL-safe. */
GSML3_C_API void gsml3_orchestrator_free(gsml3_orchestrator* o);

/* Feed a parsed L3 message into the chain. */
GSML3_C_API gsml3_step_result gsml3_orchestrator_feed(
    gsml3_orchestrator* o, const gsml3_message* msg, gsml3_session* s);
/* Typed external data (1:1 with the C++ ExternalData alternatives).
 * rand: 16 octets (wire order); sres: 4 octets (big-endian, octet 0 =
 * MSB). */
GSML3_C_API gsml3_step_result gsml3_orchestrator_feed_auth_challenge(
    gsml3_orchestrator* o, const uint8_t rand[16], const uint8_t sres[4]);
/* reject_cause: MMRejectCause value (see include/gsml3parser/enums.h). */
GSML3_C_API gsml3_step_result gsml3_orchestrator_feed_vlr_decision(
    gsml3_orchestrator* o, int accept, int has_new_tmsi, uint32_t new_tmsi,
    int reject_cause);
GSML3_C_API gsml3_step_result gsml3_orchestrator_feed_ciphering(
    gsml3_orchestrator* o, uint8_t algo, int enable);
/* id_type: GSML3_ID_TMSI or GSML3_ID_IMSI (imsi digits when IMSI);
 * target_channel: gsml3parser::ChannelType value. */
GSML3_C_API gsml3_step_result gsml3_orchestrator_feed_paging_trigger(
    gsml3_orchestrator* o, int id_type, uint32_t tmsi, const char* imsi,
    int target_channel);

/* Tick the chain timers; returns the number of failures. */
GSML3_C_API size_t gsml3_orchestrator_tick(gsml3_orchestrator* o,
                                           uint32_t delta_ms);
 /* Build the pending response (last token) into the caller's buffer;
  * 0 on error. On 0, gsml3_last_error_code() distinguishes the two causes:
  * GSML3_ERR_BUFFER_TOO_SMALL (enlarge the buffer and retry — see
  * gsml3_orchestrator_required_size) or GSML3_ERR_INVALID_VALUE (no
  * pending response or a required parameter missing from the session's
  * ResponseContext). */
 GSML3_C_API size_t gsml3_orchestrator_build_response(gsml3_orchestrator* o,
     const gsml3_session* s, uint8_t* out, size_t maxlen);
 /* Exact wire size of the pending response (last token): a buffer of this
  * many bytes is guaranteed to be accepted by
  * gsml3_orchestrator_build_response(). Returns 0 when nothing can be built
  * (NULL session, no pending response, or missing parameter; see
  * gsml3_last_error_code()). Zero allocation. */
 GSML3_C_API size_t gsml3_orchestrator_required_size(const gsml3_orchestrator* o,
                                                     const gsml3_session* s);
/* Drain the retransmission channel (consume-on-read); GSML3_TOKEN_*.
 * After a non-None token, build and send the response via
 * gsml3_orchestrator_build_response. */
GSML3_C_API int gsml3_orchestrator_take_retransmit(gsml3_orchestrator* o);
/* Cancel the active chain. */
GSML3_C_API void gsml3_orchestrator_cancel_all(gsml3_orchestrator* o);
/* Current chain phase (GSML3_PROC_*; GSML3_PROC_UNKNOWN when idle). */
GSML3_C_API int gsml3_orchestrator_chain_phase(const gsml3_orchestrator* o);

 /* Standalone response builders (stateless; zero-alloc). Return bytes
  * written, 0 on error or buffer too small. Cause parameters are int
  * values mirroring the corresponding C++ enums (RRCause /
  * MMRejectCause / CCCause — see include/gsml3parser/enums.h),
  * range-checked: out-of-domain values fail with GSML3_ERR_INVALID_ARG. */
 /* Build the response for `token` from the session's ResponseContext.
  * 0 on error: gsml3_last_error_code() is GSML3_ERR_BUFFER_TOO_SMALL when
  * the caller buffer must grow (see gsml3_response_required_size), or
  * GSML3_ERR_INVALID_VALUE / GSML3_ERR_INVALID_ARG otherwise (a required
  * parameter missing from the session's ResponseContext, or a NULL
  * argument). */
 GSML3_C_API size_t gsml3_response_build_from_token(int token,
     const gsml3_session* s, uint8_t* out, size_t maxlen);
 /* Exact wire size of the response that gsml3_response_build_from_token()
  * would produce for `token`: a buffer of this many bytes is guaranteed to
  * be accepted. Returns 0 when the response cannot be built (NULL session,
  * out-of-domain token, or a required parameter missing; see
  * gsml3_last_error_code()). Zero allocation. */
 GSML3_C_API size_t gsml3_response_required_size(int token,
                                                 const gsml3_session* s);
GSML3_C_API size_t gsml3_response_build_cm_service_accept(uint8_t* out,
    size_t maxlen);
GSML3_C_API size_t gsml3_response_build_cm_service_reject(uint8_t* out,
    size_t maxlen, int mm_cause);
GSML3_C_API size_t gsml3_response_build_identity_request(uint8_t* out,
    size_t maxlen, int id_type);
GSML3_C_API size_t gsml3_response_build_authentication_request(uint8_t* out,
    size_t maxlen, const uint8_t rand[16]);
/* mcc: exactly 3 BCD digits ("244", not "24"); mnc: 2 or 3 digits ("05").
 * Both are validated; out-of-domain values fail with
 * GSML3_ERR_INVALID_ARG. */
GSML3_C_API size_t gsml3_response_build_location_updating_accept(
    uint8_t* out, size_t maxlen, const char* mcc, const char* mnc,
    uint16_t lac, int has_new_tmsi, uint32_t new_tmsi);
GSML3_C_API size_t gsml3_response_build_location_updating_reject(
    uint8_t* out, size_t maxlen, int mm_cause);
GSML3_C_API size_t gsml3_response_build_tmsi_reallocation_command(
    uint8_t* out, size_t maxlen, const char* mcc, const char* mnc,
    uint16_t lac, uint32_t tmsi);
GSML3_C_API size_t gsml3_response_build_channel_release(uint8_t* out,
    size_t maxlen, int rr_cause);
GSML3_C_API size_t gsml3_response_build_ciphering_mode_command(uint8_t* out,
    size_t maxlen, uint8_t algo);
GSML3_C_API size_t gsml3_response_build_physical_information(uint8_t* out,
    size_t maxlen, uint8_t ta);
/* type_and_offset: gsml3parser::TypeAndOffset value (types.h). */
GSML3_C_API size_t gsml3_response_build_immediate_assignment(uint8_t* out,
    size_t maxlen, int type_and_offset, uint8_t tn, uint8_t tsc,
    uint16_t arfcn, uint8_t ta);
GSML3_C_API size_t gsml3_response_build_assignment_command(uint8_t* out,
    size_t maxlen, int type_and_offset, uint8_t tn, uint8_t tsc,
    uint16_t arfcn);
GSML3_C_API size_t gsml3_response_build_call_proceeding(uint8_t* out,
    size_t maxlen, uint8_t ti);
GSML3_C_API size_t gsml3_response_build_alerting(uint8_t* out, size_t maxlen,
    uint8_t ti);
GSML3_C_API size_t gsml3_response_build_connect(uint8_t* out, size_t maxlen,
    uint8_t ti);
GSML3_C_API size_t gsml3_response_build_connect_acknowledge(uint8_t* out,
    size_t maxlen, uint8_t ti);
GSML3_C_API size_t gsml3_response_build_disconnect(uint8_t* out,
    size_t maxlen, uint8_t ti, int cc_cause);
GSML3_C_API size_t gsml3_response_build_release(uint8_t* out, size_t maxlen,
    uint8_t ti, int cc_cause);
GSML3_C_API size_t gsml3_response_build_release_complete(uint8_t* out,
    size_t maxlen, uint8_t ti);
/* called_digits: BCD digit string (e.g. "123456789"). */
GSML3_C_API size_t gsml3_response_build_setup(uint8_t* out, size_t maxlen,
    const char* called_digits, uint8_t ti);

/* ── Typed access: curated message fields / builders ─────────────────── */
/*
 * The general layer (gsml3_parse_l3* / gsml3_message_*) covers all 236
 * message types; this section adds typed field access and builders for
 * the ~44 key messages used by BTS procedure chains, the examples and
 * the quickstart. Getters return sentinel values (-1 / 0 / NULL) when
 * the message is not the expected type.
 */

/* Channel description (GSM 04.08 10.5.2.5). type_and_offset:
 * gsml3parser::TypeAndOffset value (types.h). */
typedef struct gsml3_channel {
    int type_and_offset;
    uint8_t tn;
    uint8_t tsc;
    uint16_t arfcn;
} gsml3_channel;

/* Mobile identity (GSM 04.08 10.5.1.4). imsi points into the message
 * handle (valid while the handle is alive); NULL for TMSI identities. */
typedef struct gsml3_mobile_identity {
    int type;        /* GSML3_ID_* */
    uint32_t tmsi;   /* valid when type == GSML3_ID_TMSI */
    const char* imsi;
} gsml3_mobile_identity;

/* Location Area Identity (GSM 04.08 10.5.1.3), numeric form. */
typedef struct gsml3_lai {
    int mcc;   /* e.g. 244 */
    int mnc;   /* e.g. 5 */
    uint16_t lac;
} gsml3_lai;

/* ── RR getters ──────────────────────────────────────────────────────── */
GSML3_C_API int gsml3_msg_channel_release_cause(const gsml3_message* msg);
/* 1/0 when the GPRS resumption bit is present, -1 otherwise/absent. */
GSML3_C_API int gsml3_msg_channel_release_gprs_resumption(const gsml3_message* msg);
/* 8-bit request reference (RA) from the RACH burst. */
GSML3_C_API int gsml3_msg_channel_request_ra(const gsml3_message* msg);
GSML3_C_API int gsml3_msg_immediate_assignment_channel(const gsml3_message* msg,
                                                        gsml3_channel* ch);
GSML3_C_API int gsml3_msg_immediate_assignment_ta(const gsml3_message* msg);
GSML3_C_API int gsml3_msg_immediate_assignment_reject_wait_time(const gsml3_message* msg);
GSML3_C_API int gsml3_msg_assignment_command_channel(const gsml3_message* msg,
                                                      gsml3_channel* ch);
GSML3_C_API int gsml3_msg_assignment_complete_cause(const gsml3_message* msg);
GSML3_C_API int gsml3_msg_assignment_failure_cause(const gsml3_message* msg);
/* Number of paged mobiles (1..2). */
GSML3_C_API int gsml3_msg_paging_request_type1_count(const gsml3_message* msg);
GSML3_C_API int gsml3_msg_paging_request_type1_identity(const gsml3_message* msg,
                                                         int index,
                                                         gsml3_mobile_identity* id);
/* Paged TMSIs (2 / 4 respectively); 0 on out-of-range. */
GSML3_C_API uint32_t gsml3_msg_paging_request_type2_tmsi(const gsml3_message* msg,
                                                          int index);
GSML3_C_API uint32_t gsml3_msg_paging_request_type3_tmsi(const gsml3_message* msg,
                                                          int index);
GSML3_C_API int gsml3_msg_paging_response_cks(const gsml3_message* msg);
GSML3_C_API int gsml3_msg_paging_response_identity(const gsml3_message* msg,
                                                    gsml3_mobile_identity* id);
GSML3_C_API int gsml3_msg_ciphering_mode_command_ciphering(const gsml3_message* msg);
GSML3_C_API int gsml3_msg_ciphering_mode_command_algorithm(const gsml3_message* msg);
GSML3_C_API int gsml3_msg_ciphering_mode_complete_response(const gsml3_message* msg);
GSML3_C_API int gsml3_msg_ciphering_mode_complete_has_imeisv(const gsml3_message* msg);
GSML3_C_API int gsml3_msg_handover_complete_cause(const gsml3_message* msg);
/* Target cell of the handover command. */
GSML3_C_API int gsml3_msg_handover_command_cell(const gsml3_message* msg,
                                                uint16_t* arfcn, uint8_t* ncc,
                                                uint8_t* bcc);
GSML3_C_API int gsml3_msg_physical_information_ta(const gsml3_message* msg);

/* ── MM getters ──────────────────────────────────────────────────────── */
/* L3CMServiceType::TypeCode value. */
GSML3_C_API int gsml3_msg_cm_service_request_service_type(const gsml3_message* msg);
GSML3_C_API int gsml3_msg_cm_service_request_identity(const gsml3_message* msg,
                                                       gsml3_mobile_identity* id);
GSML3_C_API int gsml3_msg_cm_service_reject_cause(const gsml3_message* msg);
/* CMServiceAbortCause value (enums.h). */
GSML3_C_API int gsml3_msg_cm_service_abort_cause(const gsml3_message* msg);
/* MobileIDType value. */
GSML3_C_API int gsml3_msg_identity_request_type(const gsml3_message* msg);
GSML3_C_API int gsml3_msg_identity_response_identity(const gsml3_message* msg,
                                                      gsml3_mobile_identity* id);
/* 0=Normal, 1=Periodic, 2=IMSI Attach. */
GSML3_C_API int gsml3_msg_location_updating_request_update_type(const gsml3_message* msg);
GSML3_C_API int gsml3_msg_location_updating_request_identity(const gsml3_message* msg,
                                                              gsml3_mobile_identity* id);
GSML3_C_API int gsml3_msg_location_updating_request_lai(const gsml3_message* msg,
                                                         gsml3_lai* lai);
GSML3_C_API int gsml3_msg_location_updating_accept_lai(const gsml3_message* msg,
                                                        gsml3_lai* lai);
GSML3_C_API int gsml3_msg_location_updating_accept_identity(const gsml3_message* msg,
                                                             gsml3_mobile_identity* id);
GSML3_C_API int gsml3_msg_location_updating_reject_cause(const gsml3_message* msg);
GSML3_C_API int gsml3_msg_authentication_request_cks(const gsml3_message* msg);
/* Copies the 16-octet RAND (wire order) into rand[16]; 0 on wrong type. */
GSML3_C_API int gsml3_msg_authentication_request_rand(const gsml3_message* msg,
                                                       uint8_t rand[16]);
/* 32-bit SRES; 0 when the message is not AuthenticationResponse. */
GSML3_C_API uint32_t gsml3_msg_authentication_response_sres(const gsml3_message* msg);
GSML3_C_API int gsml3_msg_tmsi_reallocation_command_lai(const gsml3_message* msg,
                                                         gsml3_lai* lai);
GSML3_C_API uint32_t gsml3_msg_tmsi_reallocation_command_tmsi(const gsml3_message* msg);
GSML3_C_API int gsml3_msg_imsi_detach_indication_identity(const gsml3_message* msg,
                                                           gsml3_mobile_identity* id);

/* ── CC getters ──────────────────────────────────────────────────────── */
GSML3_C_API int gsml3_msg_setup_ti(const gsml3_message* msg);
GSML3_C_API int gsml3_msg_setup_have_called_party(const gsml3_message* msg);
/* BCD digit string into the handle (NULL when absent). */
GSML3_C_API const char* gsml3_msg_setup_called_number(const gsml3_message* msg);
GSML3_C_API int gsml3_msg_call_proceeding_ti(const gsml3_message* msg);
GSML3_C_API int gsml3_msg_alerting_ti(const gsml3_message* msg);
GSML3_C_API int gsml3_msg_connect_ti(const gsml3_message* msg);
GSML3_C_API int gsml3_msg_connect_acknowledge_ti(const gsml3_message* msg);
GSML3_C_API int gsml3_msg_disconnect_ti(const gsml3_message* msg);
/* CCCause value (enums.h). */
GSML3_C_API int gsml3_msg_disconnect_cause(const gsml3_message* msg);
GSML3_C_API int gsml3_msg_release_ti(const gsml3_message* msg);
GSML3_C_API int gsml3_msg_release_have_cause(const gsml3_message* msg);
GSML3_C_API int gsml3_msg_release_cause(const gsml3_message* msg);
GSML3_C_API int gsml3_msg_release_complete_ti(const gsml3_message* msg);
GSML3_C_API int gsml3_msg_release_complete_have_cause(const gsml3_message* msg);
GSML3_C_API int gsml3_msg_release_complete_cause(const gsml3_message* msg);
GSML3_C_API int gsml3_msg_facility_ti(const gsml3_message* msg);
GSML3_C_API size_t gsml3_msg_facility_body(const gsml3_message* msg,
                                            uint8_t* out, size_t maxlen);

/* ── SMS getters ─────────────────────────────────────────────────────── */
GSML3_C_API size_t gsml3_msg_cp_data_rpdu(const gsml3_message* msg,
                                           uint8_t* out, size_t maxlen);
GSML3_C_API int gsml3_msg_cp_status_tp_oi(const gsml3_message* msg);
GSML3_C_API int gsml3_msg_cp_status_mti_value(const gsml3_message* msg);
GSML3_C_API int gsml3_msg_cp_status_has_message_ref(const gsml3_message* msg);
GSML3_C_API int gsml3_msg_cp_status_message_ref(const gsml3_message* msg);
GSML3_C_API size_t gsml3_msg_cp_smt_rpdu(const gsml3_message* msg,
                                          uint8_t* out, size_t maxlen);
GSML3_C_API int gsml3_msg_sms_deliver_tp_mti(const gsml3_message* msg);
GSML3_C_API int gsml3_msg_sms_deliver_tp_mr(const gsml3_message* msg);
GSML3_C_API int gsml3_msg_sms_deliver_has_tp_ud(const gsml3_message* msg);
GSML3_C_API size_t gsml3_msg_sms_deliver_tp_ud(const gsml3_message* msg,
                                                uint8_t* out, size_t maxlen);

/* ── SS getters ──────────────────────────────────────────────────────── */
GSML3_C_API int gsml3_msg_sup_serv_facility_ti(const gsml3_message* msg);
GSML3_C_API size_t gsml3_msg_sup_serv_facility_data(const gsml3_message* msg,
                                                     uint8_t* out, size_t maxlen);

 /* ── Typed builders ──────────────────────────────────────────────────── */
 /* cause parameters: RRCause / MMRejectCause / CMServiceAbortCause /
  * CCCause values (include/gsml3parser/enums.h); id_type: GSML3_ID_*;
  * service_type: L3CMServiceType::TypeCode value. All parameters are
  * range-checked against the protocol domain, including the fixed-width
  * frame fields of channel descriptions (timeslot and time slot code 0..7,
  * ARFCN 0..1023) and timing advance (0..63), and the reserved all-zero
  * TMSI: an out-of-domain value fails with GSML3_ERR_INVALID_ARG (return
  * 0) and is never truncated into the frame. */
GSML3_C_API size_t gsml3_build_channel_release(uint8_t* out, size_t maxlen,
                                                int rr_cause);
GSML3_C_API size_t gsml3_build_channel_request(uint8_t* out, size_t maxlen,
                                                uint8_t ra);
GSML3_C_API size_t gsml3_build_immediate_assignment(uint8_t* out, size_t maxlen,
    int type_and_offset, uint8_t tn, uint8_t tsc, uint16_t arfcn, uint8_t ta,
    uint8_t ra);
GSML3_C_API size_t gsml3_build_immediate_assignment_reject(uint8_t* out,
    size_t maxlen, uint8_t wait_seconds);
GSML3_C_API size_t gsml3_build_assignment_command(uint8_t* out, size_t maxlen,
    int type_and_offset, uint8_t tn, uint8_t tsc, uint16_t arfcn);
GSML3_C_API size_t gsml3_build_assignment_complete(uint8_t* out, size_t maxlen,
                                                    int rr_cause);
GSML3_C_API size_t gsml3_build_assignment_failure(uint8_t* out, size_t maxlen,
                                                   int rr_cause);
GSML3_C_API size_t gsml3_build_paging_request_type1(uint8_t* out, size_t maxlen,
                                                     uint32_t tmsi);
GSML3_C_API size_t gsml3_build_paging_request_type2(uint8_t* out, size_t maxlen,
                                                     uint32_t tmsi0, uint32_t tmsi1);
GSML3_C_API size_t gsml3_build_paging_request_type3(uint8_t* out, size_t maxlen,
    uint32_t tmsi0, uint32_t tmsi1, uint32_t tmsi2, uint32_t tmsi3);
GSML3_C_API size_t gsml3_build_paging_response(uint8_t* out, size_t maxlen,
    int id_type, uint32_t tmsi, const char* imsi);
GSML3_C_API size_t gsml3_build_ciphering_mode_command(uint8_t* out, size_t maxlen,
                                                       uint8_t algo);
GSML3_C_API size_t gsml3_build_ciphering_mode_complete(uint8_t* out,
                                                        size_t maxlen, int response);
GSML3_C_API size_t gsml3_build_handover_complete(uint8_t* out, size_t maxlen,
                                                  int rr_cause);
GSML3_C_API size_t gsml3_build_physical_information(uint8_t* out, size_t maxlen,
                                                     uint8_t ta);
GSML3_C_API size_t gsml3_build_cm_service_request(uint8_t* out, size_t maxlen,
    int service_type, int id_type, uint32_t tmsi, const char* imsi);
GSML3_C_API size_t gsml3_build_cm_service_accept(uint8_t* out, size_t maxlen);
GSML3_C_API size_t gsml3_build_cm_service_reject(uint8_t* out, size_t maxlen,
                                                  int mm_cause);
GSML3_C_API size_t gsml3_build_cm_service_abort(uint8_t* out, size_t maxlen,
                                                 int abort_cause);
GSML3_C_API size_t gsml3_build_identity_request(uint8_t* out, size_t maxlen,
                                                 int id_type);
GSML3_C_API size_t gsml3_build_identity_response(uint8_t* out, size_t maxlen,
    int id_type, uint32_t tmsi, const char* imsi);
GSML3_C_API size_t gsml3_build_location_updating_request(uint8_t* out,
    size_t maxlen, int update_type, int id_type, uint32_t tmsi, const char* imsi,
    int mcc, int mnc, uint16_t lac);
GSML3_C_API size_t gsml3_build_location_updating_accept(uint8_t* out,
    size_t maxlen, int mcc, int mnc, uint16_t lac, int has_new_tmsi,
    uint32_t new_tmsi);
GSML3_C_API size_t gsml3_build_location_updating_reject(uint8_t* out,
                                                         size_t maxlen, int mm_cause);
GSML3_C_API size_t gsml3_build_authentication_request(uint8_t* out, size_t maxlen,
                                                       uint8_t cksn,
                                                       const uint8_t rand[16]);
GSML3_C_API size_t gsml3_build_authentication_response(uint8_t* out, size_t maxlen,
                                                        uint32_t sres);
GSML3_C_API size_t gsml3_build_tmsi_reallocation_command(uint8_t* out,
    size_t maxlen, int mcc, int mnc, uint16_t lac, uint32_t tmsi);
GSML3_C_API size_t gsml3_build_tmsi_reallocation_complete(uint8_t* out,
                                                           size_t maxlen);
GSML3_C_API size_t gsml3_build_imsi_detach_indication(uint8_t* out, size_t maxlen,
    int id_type, uint32_t tmsi, const char* imsi);
GSML3_C_API size_t gsml3_build_setup(uint8_t* out, size_t maxlen, uint8_t ti,
                                      const char* called_digits);
GSML3_C_API size_t gsml3_build_call_proceeding(uint8_t* out, size_t maxlen,
                                                uint8_t ti);
GSML3_C_API size_t gsml3_build_alerting(uint8_t* out, size_t maxlen, uint8_t ti);
GSML3_C_API size_t gsml3_build_connect(uint8_t* out, size_t maxlen, uint8_t ti);
GSML3_C_API size_t gsml3_build_connect_acknowledge(uint8_t* out, size_t maxlen,
                                                    uint8_t ti);
GSML3_C_API size_t gsml3_build_disconnect(uint8_t* out, size_t maxlen,
                                           uint8_t ti, int cc_cause);
GSML3_C_API size_t gsml3_build_release(uint8_t* out, size_t maxlen,
                                        uint8_t ti, int cc_cause);
GSML3_C_API size_t gsml3_build_release_complete(uint8_t* out, size_t maxlen,
                                                 uint8_t ti);
GSML3_C_API size_t gsml3_build_facility(uint8_t* out, size_t maxlen, uint8_t ti,
                                         const uint8_t* data, size_t len);
GSML3_C_API size_t gsml3_build_cp_data(uint8_t* out, size_t maxlen,
                                        const uint8_t* rpdu, size_t rpdu_len);
GSML3_C_API size_t gsml3_build_cp_status(uint8_t* out, size_t maxlen,
    uint8_t tp_oi, uint8_t mti_value, int has_ref, uint8_t ref);
GSML3_C_API size_t gsml3_build_cp_smt(uint8_t* out, size_t maxlen,
                                       const uint8_t* rpdu, size_t rpdu_len);
GSML3_C_API size_t gsml3_build_sms_deliver(uint8_t* out, size_t maxlen,
    uint8_t tp_mti, uint8_t tp_mr, const uint8_t* ud, size_t ud_len);
GSML3_C_API size_t gsml3_build_sup_serv_facility(uint8_t* out, size_t maxlen,
    uint8_t ti, const uint8_t* data, size_t len);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* GSML3PARSER_C_H */
