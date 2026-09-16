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

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* GSML3PARSER_C_H */
