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

//! Raw FFI declarations for the libgsml3parser C ABI (`gsml3parser_c.h`).
//!
//! This crate is a set of handwritten mirrors of the stable C interface —
//! **no code generation, no bindgen, no dependencies**: plain `extern "C"`
//! blocks plus `#[repr(C)]` structures. The C header is the single source of
//! truth; keep this file in sync when the ABI changes and bump `ABI_VERSION`.
//!
//! Surface (v1): sections S1–S7 of the header plus the two typed builders
//! needed by the demo chain — `gsml3_build_cm_service_request` and
//! `gsml3_build_setup` (128 functions total). The curated
//! S8 typed getters are intentionally not declared here.
//!
//! Completeness is enforced at compile time by `tests/surface.rs`, which
//! references every one of the 128 names with its full signature — a missing
//! declaration or a drifted type is a compile error of that test. At runtime,
//! check `ABI_VERSION` against [`gsml3_abi_version`] when linking a prebuilt
//! binary of a possibly different build (the header documents that public
//! enums/structs only grow and functions are only added).
// The opaque handles and callback typedefs keep the VERBATIM C names from
// gsml3parser_c.h on purpose: this crate mirrors the ABI, it does not
// re-style it (the safe wrapper exposes the idiomatic Rust surface instead).
#![allow(non_camel_case_types)]

use std::os::raw::{c_char, c_int, c_void, c_uchar};

/// C ABI revision. Must equal `GSML3_ABI_VERSION` in the C header; verify
/// against [`gsml3_abi_version`] when a prebuilt library is loaded.
pub const ABI_VERSION: u32 = 1; // == GSML3_ABI_VERSION

// ── Error model (enum gsml3_error, mirrored 1:1) ─────────────────────────────

/// No error. `gsml3_last_error()` returns "" and the message is absent.
pub const GSML3_OK: c_int = 0;
/// Invalid argument (out-of-domain enum/digit-string/field width).
pub const GSML3_ERR_INVALID_ARG: c_int = 1;
/// Truncated input (frame shorter than its header/body requirements).
pub const GSML3_ERR_TRUNCATED: c_int = 2;
/// Unknown protocol discriminator.
pub const GSML3_ERR_INVALID_PD: c_int = 3;
/// Unknown message type identifier.
pub const GSML3_ERR_INVALID_MTI: c_int = 4;
/// The message length does not match its declared size.
pub const GSML3_ERR_LENGTH_MISMATCH: c_int = 5;
/// Malformed information element.
pub const GSML3_ERR_INVALID_IE: c_int = 6;
/// An invalid field value for a known message.
pub const GSML3_ERR_INVALID_VALUE: c_int = 7;
/// The operation is not supported in this configuration/flavor.
pub const GSML3_ERR_UNSUPPORTED: c_int = 8;
/// A required source (e.g. an identity) has no remaining candidates.
pub const GSML3_ERR_SOURCE_EXHAUSTED: c_int = 9;
/// Allocation failure inside the library.
pub const GSML3_ERR_NO_MEMORY: c_int = 10;
/// The caller's output buffer was too small (enlarge it and retry).
pub const GSML3_ERR_BUFFER_TOO_SMALL: c_int = 11;
/// Internal failure (an unexpected condition inside the library).
pub const GSML3_ERR_INTERNAL: c_int = 12;
/// A key that must be unique is already present.
pub const GSML3_ERR_DUPLICATE: c_int = 13;

// ── Log levels (mirror enum gsml3_log_level) ──────────────────────────────────

pub const GSML3_LOG_EMERG: c_int = 0;
pub const GSML3_LOG_ALERT: c_int = 1;
pub const GSML3_LOG_CRIT: c_int = 2;
pub const GSML3_LOG_ERR: c_int = 3;
pub const GSML3_LOG_WARNING: c_int = 4;
pub const GSML3_LOG_NOTICE: c_int = 5;
pub const GSML3_LOG_INFO: c_int = 6;
pub const GSML3_LOG_DEBUG: c_int = 7;

// ── Protocol discriminators (mirror enum gsml3_pd) ────────────────────────────

pub const GSML3_PD_GCC: c_int = 0x00;
pub const GSML3_PD_BCC: c_int = 0x01;
pub const GSML3_PD_CC: c_int = 0x03;
pub const GSML3_PD_MM: c_int = 0x05;
pub const GSML3_PD_RR: c_int = 0x06;
pub const GSML3_PD_GMM: c_int = 0x08;
pub const GSML3_PD_SMS: c_int = 0x09;
pub const GSML3_PD_SM: c_int = 0x0a;
pub const GSML3_PD_SS: c_int = 0x0b;
pub const GSML3_PD_LS: c_int = 0x0c;
pub const GSML3_PD_EXT: c_int = 0x0e;
pub const GSML3_PD_TST: c_int = 0x0f;
pub const GSML3_PD_UNDEFINED: c_int = -1;

// ── Mobile identity types (mirror enum gsml3_id_type) ─────────────────────────

pub const GSML3_ID_NO_ID: c_int = 0;
pub const GSML3_ID_IMSI: c_int = 1;
pub const GSML3_ID_IMEI: c_int = 2;
pub const GSML3_ID_IMEISV: c_int = 3;
pub const GSML3_ID_TMSI: c_int = 4;

// ── LAPDm constants (mirror enums gsml3_lapdm_*) ─────────────────────────────

pub const GSML3_LAPDM_FMT_I: c_int = 0;
pub const GSML3_LAPDM_FMT_S: c_int = 1;
pub const GSML3_LAPDM_FMT_U: c_int = 2;
pub const GSML3_LAPDM_U_UI: c_int = 0x03;
pub const GSML3_LAPDM_U_SABME: c_int = 0x2F;
pub const GSML3_LAPDM_U_UA: c_int = 0x63;
pub const GSML3_LAPDM_U_DM: c_int = 0x0F;
pub const GSML3_LAPDM_U_DISC: c_int = 0x08;
pub const GSML3_LAPDM_S_RR: c_int = 0x01;
pub const GSML3_LAPDM_S_REJ: c_int = 0x0D;
pub const GSML3_LAPDM_STATE_UNUSED: c_int = 0;
pub const GSML3_LAPDM_STATE_LINK_RELEASED: c_int = 1;
pub const GSML3_LAPDM_STATE_AWAITING_ESTABLISH: c_int = 2;
pub const GSML3_LAPDM_STATE_AWAITING_RELEASE: c_int = 3;
pub const GSML3_LAPDM_STATE_LINK_ESTABLISHED: c_int = 4;
pub const GSML3_LAPDM_STATE_CONTENTION_RESOLUTION: c_int = 5;

// ── SAPI values (mirror enum gsml3_sapi) ──────────────────────────────────────

pub const GSML3_SAPI0: c_int = 0;
pub const GSML3_SAPI3: c_int = 3;
pub const GSML3_SAPI0_SACCH: c_int = 4;
pub const GSML3_SAPI3_SACCH: c_int = 7;

// ── Interlayer primitives (mirror enum gsml3_primitive) ───────────────────────

pub const GSML3_PRIM_L2_DATA: c_int = 1;
pub const GSML3_PRIM_L3_DATA: c_int = 2;
pub const GSML3_PRIM_L3_DATA_CONFIRM: c_int = 3;
pub const GSML3_PRIM_L3_UNIT_DATA: c_int = 4;
pub const GSML3_PRIM_L3_ESTABLISH_REQUEST: c_int = 5;
pub const GSML3_PRIM_L3_ESTABLISH_INDICATION: c_int = 6;
pub const GSML3_PRIM_L3_ESTABLISH_CONFIRM: c_int = 7;
pub const GSML3_PRIM_L3_RELEASE_REQUEST: c_int = 8;
pub const GSML3_PRIM_L3_RELEASE_CONFIRM: c_int = 9;
pub const GSML3_PRIM_L3_HARDRELEASE_REQUEST: c_int = 10;
pub const GSML3_PRIM_MDL_ERROR_INDICATION: c_int = 11;
pub const GSML3_PRIM_L3_RELEASE_INDICATION: c_int = 12;
pub const GSML3_PRIM_PH_CONNECT: c_int = 13;
pub const GSML3_PRIM_HANDOVER_ACCESS: c_int = 14;

// ── L3 timer IDs (mirror enum gsml3_timer) ────────────────────────────────────

pub const GSML3_TIMER_T3101: c_int = 0;
pub const GSML3_TIMER_T3102: c_int = 1;
pub const GSML3_TIMER_T3103: c_int = 2;
pub const GSML3_TIMER_T3106: c_int = 3;
pub const GSML3_TIMER_T3108: c_int = 4;
pub const GSML3_TIMER_T3109: c_int = 5;
pub const GSML3_TIMER_T3111: c_int = 6;
pub const GSML3_TIMER_T3112: c_int = 7;
pub const GSML3_TIMER_T3113: c_int = 8;
pub const GSML3_TIMER_T3310: c_int = 9;
pub const GSML3_TIMER_T3311: c_int = 10;
pub const GSML3_TIMER_T3312: c_int = 11;
pub const GSML3_TIMER_T3314: c_int = 12;
pub const GSML3_TIMER_T3315: c_int = 13;
pub const GSML3_TIMER_T3320: c_int = 14;
pub const GSML3_TIMER_T3321: c_int = 15;
pub const GSML3_TIMER_T3322: c_int = 16;
pub const GSML3_TIMER_T3334: c_int = 17;
pub const GSML3_TIMER_T3395: c_int = 18;
pub const GSML3_TIMER_UNKNOWN: c_int = 0xFF;

// ── Orchestrator constants (mirror enums gsml3_action/state/proc_type/token) ──

pub const GSML3_ACTION_CONTINUE: c_int = 0;
pub const GSML3_ACTION_SEND_RESPONSE: c_int = 1;
pub const GSML3_ACTION_WAITING_EXTERNAL: c_int = 2;
pub const GSML3_ACTION_COMPLETED: c_int = 3;
pub const GSML3_ACTION_FAILED: c_int = 4;

pub const GSML3_STATE_INITIATED: c_int = 0;
pub const GSML3_STATE_IN_PROGRESS: c_int = 1;
pub const GSML3_STATE_WAITING_EXTERNAL: c_int = 2;
pub const GSML3_STATE_COMPLETED: c_int = 3;
pub const GSML3_STATE_FAILED: c_int = 4;
pub const GSML3_STATE_TIMED_OUT: c_int = 5;

pub const GSML3_PROC_LOCATION_UPDATE: c_int = 0x01;
pub const GSML3_PROC_AUTHENTICATION: c_int = 0x02;
pub const GSML3_PROC_CIPHERING_MODE: c_int = 0x03;
pub const GSML3_PROC_CALL_SETUP_MO: c_int = 0x04;
pub const GSML3_PROC_CALL_SETUP_MT: c_int = 0x05;
pub const GSML3_PROC_CHANNEL_ASSIGNMENT: c_int = 0x06;
pub const GSML3_PROC_HANDOVER: c_int = 0x07;
pub const GSML3_PROC_PAGING: c_int = 0x08;
pub const GSML3_PROC_CM_SERVICE_REQUEST: c_int = 0x09;
pub const GSML3_PROC_IMSI_DETACH: c_int = 0x0A;
pub const GSML3_PROC_CALL_RELEASE: c_int = 0x0B;
pub const GSML3_PROC_PERIODIC_LOCATION_UPDATE: c_int = 0x0C;
pub const GSML3_PROC_UNKNOWN: c_int = 0xFF;

pub const GSML3_TOKEN_NONE: c_int = 0;
pub const GSML3_TOKEN_IMMEDIATE_ASSIGNMENT: c_int = 1;
pub const GSML3_TOKEN_ASSIGNMENT_COMMAND: c_int = 2;
pub const GSML3_TOKEN_CHANNEL_RELEASE: c_int = 3;
pub const GSML3_TOKEN_CIPHERING_MODE_COMMAND: c_int = 4;
pub const GSML3_TOKEN_PHYSICAL_INFORMATION: c_int = 5;
pub const GSML3_TOKEN_HANDOVER_COMMAND: c_int = 6;
pub const GSML3_TOKEN_PAGING_REQUEST_TYPE1: c_int = 7;
pub const GSML3_TOKEN_PAGING_REQUEST_TYPE2: c_int = 8;
pub const GSML3_TOKEN_PAGING_REQUEST_TYPE3: c_int = 9;
pub const GSML3_TOKEN_CM_SERVICE_ACCEPT: c_int = 10;
pub const GSML3_TOKEN_CM_SERVICE_REJECT: c_int = 11;
pub const GSML3_TOKEN_IDENTITY_REQUEST: c_int = 12;
pub const GSML3_TOKEN_AUTHENTICATION_REQUEST: c_int = 13;
pub const GSML3_TOKEN_LOCATION_UPDATING_ACCEPT: c_int = 14;
pub const GSML3_TOKEN_LOCATION_UPDATING_REJECT: c_int = 15;
pub const GSML3_TOKEN_TMSI_REALLOCATION_COMMAND: c_int = 16;
pub const GSML3_TOKEN_CALL_PROCEEDING: c_int = 17;
pub const GSML3_TOKEN_ALERTING: c_int = 18;
pub const GSML3_TOKEN_CONNECT: c_int = 19;
pub const GSML3_TOKEN_CONNECT_ACKNOWLEDGE: c_int = 20;
pub const GSML3_TOKEN_DISCONNECT: c_int = 21;
pub const GSML3_TOKEN_RELEASE: c_int = 22;
pub const GSML3_TOKEN_RELEASE_COMPLETE: c_int = 23;
pub const GSML3_TOKEN_SETUP: c_int = 24;

// ── Opaque handles (typedef struct gsml3_* in the header) ────────────────────

/// Parser config handle. Owned; release with [`gsml3_config_free`].
pub type gsml3_config = c_void;
/// Parsed L3 message handle. Owned; release with [`gsml3_message_free`].
pub type gsml3_message = c_void;
/// Parsed RSL (A-bis) frame handle; owns a copy of the input. Release with [`gsml3_rsl_free`].
pub type gsml3_rsl = c_void;
/// LAPDm entity handle. Owned; release with [`gsml3_lapdm_entity_free`].
pub type gsml3_lapdm_entity = c_void;
/// BTS subscriber registry handle. Owned; release with [`gsml3_registry_free`].
pub type gsml3_registry = c_void;
/// Session handle — BORROWED pointer-sized token owned by its registry:
/// NEVER free it directly; valid until the session is removed or the
/// registry is freed (`gsml3_registry_free` releases all sessions).
pub type gsml3_session = c_void;
/// Procedure chain orchestrator. Owned; release with [`gsml3_orchestrator_free`].
pub type gsml3_orchestrator = c_void;

// ── Structures (layouts verbatim from the header) ────────────────────────────

/// Mirrors `gsml3_lapdm_frame_info`. ZERO-COPY: [`gsml3_lapdm_frame_info::info`]
/// points into the input buffer that was passed to
/// [`gsml3_lapdm_frame_decode`]; treat it as a view valid while that buffer
/// is alive, never retain or copy-on-free.
#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct gsml3_lapdm_frame_info {
    /// GSML3_LAPDM_FMT_I / _S / _U
    pub format: c_int,
    /// GSML3_LAPDM_U_* when format == U, else -1
    pub u_type: c_int,
    /// GSML3_LAPDM_S_* when format == S, else -1
    pub s_type: c_int,
    /// Receive sequence number (I/S frames)
    pub nr: u8,
    /// Send sequence number (I frames)
    pub ns: u8,
    /// Poll/Final bit
    pub pf: c_int,
    /// Message-complete bit (I frames)
    pub m_bit: c_int,
    /// SAPI 0..15
    pub sapi: c_int,
    /// C/R: 1 = command, 0 = response
    pub command: c_int,
    /// Zero-copy view into the decode() input (NULL when absent)
    pub info: *const c_uchar,
    pub info_len: usize,
}

/// Mirrors `gsml3_timer_expiry`. The `session` pointer is BORROWED: do not
/// free it; it is valid until the owning registry is freed or removed it.
#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct gsml3_timer_expiry {
    /// BORROWED session handle; do not free
    pub session: *mut c_void,
    /// GSML3_TIMER_*
    pub timer_id: c_int,
}

/// Mirrors `gsml3_step_result` — returned BY VALUE from the orchestrator
/// `feed*` functions. When `error != GSML3_OK` the remaining fields carry no
/// step information. `reason` is THREAD-LOCAL library storage (valid until
/// the next gsml3_* call on this thread, NULL when empty) — copy it
/// immediately after the feed returns.
#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct gsml3_step_result {
    /// GSML3_ACTION_*
    pub action: c_int,
    /// GSML3_TOKEN_* (GSML3_TOKEN_NONE when none)
    pub response_token: c_int,
    /// GSML3_STATE_*
    pub final_state: c_int,
    /// GSML3_PROC_*
    pub final_type: c_int,
    /// Thread-local reason text — copy immediately (see struct docs)
    pub reason: *const c_char,
    /// gsml3_error code; GSML3_OK on a real step
    pub error: c_int,
}

/// Mirrors `gsml3_channel` (GSM 04.08 10.5.2.5).
#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct gsml3_channel {
    /// gsml3parser::TypeAndOffset value
    pub type_and_offset: c_int,
    /// Timeslot number 0..7
    pub tn: u8,
    /// Timeslot code 0..7
    pub tsc: u8,
    /// Absolute radio-frequency channel number 0..1023
    pub arfcn: u16,
}

/// Mirrors `gsml3_mobile_identity` (GSM 04.08 10.5.1.4). NOTE: the C field is
/// named `type` (reserved in Rust); the Rust mirror uses `type_` — a field
/// name has no ABI role, the layout is identical. `imsi` points into the
/// owning gsml3_message handle (valid while that handle is alive; NULL for
/// TMSI identities).
#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct gsml3_mobile_identity {
    /// GSML3_ID_* (C field `type`)
    pub type_: c_int,
    /// Valid when type == GSML3_ID_TMSI
    pub tmsi: u32,
    /// Points into the message handle; read while it is alive
    pub imsi: *const c_char,
}

/// Mirrors `gsml3_lai` (Location Area Identity, numeric form; e.g. 244 / 5).
#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct gsml3_lai {
    pub mcc: c_int,
    pub mnc: c_int,
    pub lac: u16,
}

// ── Callback types (mirror the C typedefs) ───────────────────────────────────

/// L3 interlayer primitive callback. SAFETY mirror of the C typedef: the
/// `l3` span is valid ONLY during the call — copy or transmit synchronously,
/// never retain it, and NEVER call back into any gsml3_* function from within
/// (the entity callbacks are invoked synchronously; re-entering through the
/// same entity is forbidden by the ABI).
pub type lapdm_l3_cb = unsafe extern "C" fn(
    sapi: c_int,
    primitive: c_int,
    l3: *const c_uchar,
    l3_len: usize,
    user: *mut c_void,
);

/// L1 transmit-frame callback. Same lifetime/contract rules as
/// [`lapdm_l3_cb`]: the `frame` span is valid only during the call.
pub type lapdm_l1_cb =
    unsafe extern "C" fn(frame: *const c_uchar, frame_len: usize, user: *mut c_void);

// ── The C ABI (128 functions: S1–S7 + 2 curated S9 builders) ────────────────
// This list is a 1:1 transcription of
// the "Inventory of C ABI functions" for the v1 surface — no omissions; the
// names are verbatim from gsml3parser_c.h (search anchor: `GSML3_C_API name(`).
// Types follow the mapping int=c_int, uint8_t=u8, uint16_t=u16, uint32_t=u32,
// size_t=usize, const uint8_t* = *const c_uchar, uint8_t* out = *mut c_uchar,
// const char* = *const c_char, char* = *mut c_char (library-allocated →
// gsml3_free), void* user = *mut c_void. LAI-carrying S9 builders take
// `int mcc, int mnc` (numeric) while the S7 response builders with LAIs take
// `const char* mcc/mnc` (BCD digit strings) — per header, not by analogy.
// Completeness: tests/surface.rs references every name below; runtime
// guard: ABI_VERSION == gsml3_abi_version(). No code generation is used.

extern "C" {
    // ── S1 Core (5) ────────────────────────────────────────────────────────
    /// Product version string. Static storage: do not free.
    pub fn gsml3_version() -> *const c_char;
    /// C ABI revision (== GSML3_ABI_VERSION == 1).
    pub fn gsml3_abi_version() -> u32;
    /// Thread-local last error message ("" when none, never NULL). Valid
    /// until a later call clears or replaces it — copy immediately.
    pub fn gsml3_last_error() -> *const c_char;
    /// Thread-local last error code (GSML3_OK when there is no pending
    /// error). Paired with [`gsml3_last_error`].
    pub fn gsml3_last_error_code() -> c_int;
    /// Release a char* string returned by this API (hex/dump). NULL-safe.
    pub fn gsml3_free(ptr: *mut c_void);

    // ── S2 Config (4) ──────────────────────────────────────────────────────
    /// Create a config (default: log WARNING, lenient framing). NULL on OOM.
    pub fn gsml3_config_new() -> *mut gsml3_config;
    /// Set the log level (GSML3_LOG_*); out-of-range values are ignored.
    pub fn gsml3_config_set_log_level(c: *mut gsml3_config, level: c_int);
    /// Enable strict framing: reject a frame not fully consumed.
    pub fn gsml3_config_set_strict_framing(c: *mut gsml3_config, on: c_int);
    /// Free the config. NULL-safe.
    pub fn gsml3_config_free(c: *mut gsml3_config);

    // ── S3 Message (12) ────────────────────────────────────────────────────
    /// Parse raw L3 bytes. Returns NULL on error; cfg may be NULL.
    pub fn gsml3_parse_l3(
        data: *const c_uchar,
        len: usize,
        cfg: *const gsml3_config,
    ) -> *mut gsml3_message;
    /// Parse a hex string (spaces allowed, e.g. "60 0D 00"). NULL on error.
    pub fn gsml3_parse_l3_hex(
        hex: *const c_char,
        cfg: *const gsml3_config,
    ) -> *mut gsml3_message;
    /// Reparse into an existing handle (zero extra allocation for typical
    /// messages); GSML3_OK or error code — on error the previous content is kept.
    pub fn gsml3_parse_l3_into(
        msg: *mut gsml3_message,
        data: *const c_uchar,
        len: usize,
        cfg: *const gsml3_config,
    ) -> c_int;
    /// Free the message. NULL-safe.
    pub fn gsml3_message_free(msg: *mut gsml3_message);
    /// Message name. Static storage (do not free), "" when msg is NULL.
    pub fn gsml3_message_name(msg: *const gsml3_message) -> *const c_char;
    /// Protocol discriminator (GSML3_PD_*), -1 when msg is NULL.
    pub fn gsml3_message_pd(msg: *const gsml3_message) -> c_int;
    /// Message type identifier (0..255; RR short 256..511), -1 when NULL.
    pub fn gsml3_message_mti(msg: *const gsml3_message) -> c_int;
    /// Transaction identifier for CC/SS messages, 0 otherwise/NULL.
    pub fn gsml3_message_ti(msg: *const gsml3_message) -> c_int;
    /// Exact wire size of the serialized message (zero allocation); a buffer
    /// of this size is guaranteed to be accepted by gsml3_message_write. 0 for NULL.
    pub fn gsml3_message_size(msg: *const gsml3_message) -> usize;
    /// Serialize into the caller's buffer; bytes written, 0 on error or
    /// buffer too small (see gsml3_last_error_code).
    pub fn gsml3_message_write(
        msg: *const gsml3_message,
        out: *mut c_uchar,
        maxlen: usize,
    ) -> usize;
    /// Hex serialization (lowercase, no spaces). Allocated by the library:
    /// free with gsml3_free. NULL on error / NULL message.
    pub fn gsml3_message_hex(msg: *const gsml3_message) -> *mut c_char;
    /// Human-readable dump. Allocated by the library: free with gsml3_free.
    /// NULL when msg is NULL.
    pub fn gsml3_message_dump(msg: *const gsml3_message) -> *mut c_char;

    // ── S4 RSL (25) ────────────────────────────────────────────────────────
    /// Parse an A-bis RSL frame; the handle owns a copy of the input.
    pub fn gsml3_rsl_parse(data: *const c_uchar, len: usize) -> *mut gsml3_rsl;
    /// Free the RSL handle. NULL-safe.
    pub fn gsml3_rsl_free(rsl: *mut gsml3_rsl);
    /// Message name ("DATA_REQ", "CHAN_ACTIV", ...). Static storage.
    pub fn gsml3_rsl_name(rsl: *const gsml3_rsl) -> *const c_char;
    /// 7-bit discriminator (direction bit stripped), -1 when NULL.
    pub fn gsml3_rsl_discriminator(rsl: *const gsml3_rsl) -> c_int;
    /// Message type byte within the discriminator, -1 when NULL.
    pub fn gsml3_rsl_msg_type(rsl: *const gsml3_rsl) -> c_int;
    /// Channel number, -1 when NULL.
    pub fn gsml3_rsl_chan_nr(rsl: *const gsml3_rsl) -> c_int;
    /// LAPDm link identifier (RLL), -1 when NULL.
    pub fn gsml3_rsl_link_id(rsl: *const gsml3_rsl) -> c_int;
    /// Direction: 1 = BTS→BSC, 0 = BSC→BTS; -1 when NULL.
    pub fn gsml3_rsl_bts_to_bsc(rsl: *const gsml3_rsl) -> c_int;
    /// 1 if the frame carries an L3 payload, 0 otherwise.
    pub fn gsml3_rsl_has_l3(rsl: *const gsml3_rsl) -> c_int;
    /// L3 payload view into the handle's copy (NULL when no L3); `*len` is
    /// set to the payload size. Valid while the handle is alive.
    pub fn gsml3_rsl_l3(rsl: *const gsml3_rsl, len: *mut usize) -> *const c_uchar;
    /// Number of parsed information elements.
    pub fn gsml3_rsl_ie_count(rsl: *const gsml3_rsl) -> usize;
    /// IE at `index`: fills type/len/val (val points into the handle's copy).
    pub fn gsml3_rsl_ie_get(
        rsl: *const gsml3_rsl,
        index: usize,
        type_: *mut c_uchar,
        len: *mut usize,
        val: *mut *const c_uchar,
    ) -> c_int;

    // 13 RSL builders — zero-alloc, caller's buffer, bytes written (0 = error /
    // too small; cause & timing fields range-checked → GSML3_ERR_INVALID_ARG).
    pub fn gsml3_rsl_build_data_req(
        out: *mut c_uchar,
        maxlen: usize,
        chan_nr: c_uchar,
        link_id: c_uchar,
        l3: *const c_uchar,
        l3_len: usize,
    ) -> usize;
    pub fn gsml3_rsl_build_data_ind(
        out: *mut c_uchar,
        maxlen: usize,
        chan_nr: c_uchar,
        link_id: c_uchar,
        l3: *const c_uchar,
        l3_len: usize,
    ) -> usize;
    pub fn gsml3_rsl_build_unit_data_req(
        out: *mut c_uchar,
        maxlen: usize,
        chan_nr: c_uchar,
        link_id: c_uchar,
        l3: *const c_uchar,
        l3_len: usize,
    ) -> usize;
    pub fn gsml3_rsl_build_unit_data_ind(
        out: *mut c_uchar,
        maxlen: usize,
        chan_nr: c_uchar,
        link_id: c_uchar,
        l3: *const c_uchar,
        l3_len: usize,
    ) -> usize;
    pub fn gsml3_rsl_build_chan_activ_ack(
        out: *mut c_uchar,
        maxlen: usize,
        chan_nr: c_uchar,
        frame_number: u16,
    ) -> usize;
    pub fn gsml3_rsl_build_chan_activ_nack(
        out: *mut c_uchar,
        maxlen: usize,
        chan_nr: c_uchar,
        cause: c_int,
    ) -> usize;
    pub fn gsml3_rsl_build_rf_chan_rel_ack(
        out: *mut c_uchar,
        maxlen: usize,
        chan_nr: c_uchar,
    ) -> usize;
    pub fn gsml3_rsl_build_conn_fail(
        out: *mut c_uchar,
        maxlen: usize,
        chan_nr: c_uchar,
        cause: c_int,
    ) -> usize;
    pub fn gsml3_rsl_build_meas_res(
        out: *mut c_uchar,
        maxlen: usize,
        chan_nr: c_uchar,
        meas_nr: c_uchar,
        rxlev: i8,
        rxqual: i8,
        l1: *const c_uchar,
        l1_len: usize,
    ) -> usize;
    pub fn gsml3_rsl_build_hando_det(
        out: *mut c_uchar,
        maxlen: usize,
        chan_nr: c_uchar,
        access_delay: c_uchar,
    ) -> usize;
    pub fn gsml3_rsl_build_ccch_load_ind(
        out: *mut c_uchar,
        maxlen: usize,
        chan_nr: c_uchar,
        paging_load: u16,
        rach_total: u16,
        rach_busy: u16,
        rach_access: u16,
    ) -> usize;
    pub fn gsml3_rsl_build_chan_rqd(
        out: *mut c_uchar,
        maxlen: usize,
        chan_nr: c_uchar,
        ra: c_uchar,
        t1p: c_uchar,
        t2: c_uchar,
        t3: c_uchar,
        access_delay: c_uchar,
    ) -> usize;
    pub fn gsml3_rsl_build_delete_ind(
        out: *mut c_uchar,
        maxlen: usize,
        chan_nr: c_uchar,
        info: *const c_uchar,
        info_len: usize,
    ) -> usize;

    // ── S5 LAPDm (16) ──────────────────────────────────────────────────────
    /// Decode one raw LAPDm frame (address + control [+ length + info]).
    /// Zero-copy: `out.info` points into `data`. GSML3_OK or error code.
    pub fn gsml3_lapdm_frame_decode(
        data: *const c_uchar,
        len: usize,
        out: *mut gsml3_lapdm_frame_info,
    ) -> c_int;
    /// Create a LAPDm entity. profile 0=SDCCH / 1=SACCH / 2=FACCH; the
    /// callbacks may be NULL (Option); `user` is returned verbatim in every
    /// callback. Invalid profile returns NULL.
    pub fn gsml3_lapdm_entity_new(
        profile: c_int,
        l3_cb: Option<lapdm_l3_cb>,
        l1_cb: Option<lapdm_l1_cb>,
        user: *mut c_void,
    ) -> *mut gsml3_lapdm_entity;
    /// Free the entity. NULL-safe.
    pub fn gsml3_lapdm_entity_free(e: *mut gsml3_lapdm_entity);
    /// Open the entity (→ LinkReleased). sapi 0..15; out-of-range sets the
    /// thread-local error and leaves the previous state. command_bit:
    /// 1 = BTS side, 0 = MS side.
    pub fn gsml3_lapdm_entity_open(e: *mut gsml3_lapdm_entity, sapi: c_int, command_bit: c_int);
    /// Feed a raw LAPDm frame from L1 into the FSM (callbacks fire here).
    pub fn gsml3_lapdm_entity_receive(
        e: *mut gsml3_lapdm_entity,
        frame: *const c_uchar,
        len: usize,
    );
    /// Send L3 via a UI frame — works in ANY state. `sapi` is the address-
    /// octet SAPI (0..15, range-checked); may differ from the open sapi.
    pub fn gsml3_lapdm_entity_send_ui(
        e: *mut gsml3_lapdm_entity,
        sapi: c_int,
        l3: *const c_uchar,
        l3_len: usize,
    ) -> c_int;
    /// Send L3 via I-frames (segmented); requires an established link.
    pub fn gsml3_lapdm_entity_send_data(
        e: *mut gsml3_lapdm_entity,
        l3: *const c_uchar,
        l3_len: usize,
    ) -> c_int;
    /// Send SABME (link establishment); requires LinkReleased.
    pub fn gsml3_lapdm_entity_send_sabme(e: *mut gsml3_lapdm_entity) -> c_int;
    /// Send DISC (link release); requires an established link.
    pub fn gsml3_lapdm_entity_send_disc(e: *mut gsml3_lapdm_entity) -> c_int;
    /// Immediate transition to LinkReleased without sending frames.
    pub fn gsml3_lapdm_entity_hard_release(e: *mut gsml3_lapdm_entity);
    /// Advance T200 by elapsed_ms: 1 = retransmission/abnormal release,
    /// 0 = none, -1 = internal error (see gsml3_last_error()).
    pub fn gsml3_lapdm_entity_tick_t200(e: *mut gsml3_lapdm_entity, elapsed_ms: u32) -> c_int;
    /// Current FSM state (GSML3_LAPDM_STATE_*). NULL-safe.
    pub fn gsml3_lapdm_entity_state(e: *const gsml3_lapdm_entity) -> c_int;
    /// 1 if LinkEstablished or ContentionResolution, else 0. NULL-safe.
    pub fn gsml3_lapdm_entity_is_established(e: *const gsml3_lapdm_entity) -> c_int;
    /// Frames transmitted so far. NULL-safe.
    pub fn gsml3_lapdm_entity_frames_sent(e: *const gsml3_lapdm_entity) -> u32;
    /// Frames received so far. NULL-safe.
    pub fn gsml3_lapdm_entity_frames_received(e: *const gsml3_lapdm_entity) -> u32;
    /// T200 retransmissions so far. NULL-safe.
    pub fn gsml3_lapdm_entity_retransmissions(e: *const gsml3_lapdm_entity) -> u32;

    // ── S6 Registry + Session (29) ─────────────────────────────────────────
    /// Create a registry: shard_count 0 = plain single-thread; 4/8/16/32 =
    /// sharded thread-safe; any other value returns NULL + ERR_INVALID_ARG.
    pub fn gsml3_registry_new(shard_count: c_int) -> *mut gsml3_registry;
    /// Free the registry and all its sessions. NULL-safe.
    pub fn gsml3_registry_free(r: *mut gsml3_registry);
    /// Pre-size the indexes for the expected population (cold path).
    pub fn gsml3_registry_reserve(r: *mut gsml3_registry, expected: usize);
    /// Number of active sessions. NULL-safe.
    pub fn gsml3_registry_count(r: *const gsml3_registry) -> usize;
    /// Create a session keyed by TMSI. NULL + code: INVALID_ARG = reserved
    /// all-zero TMSI, DUPLICATE = key taken, NO_MEMORY.
    pub fn gsml3_registry_create_by_tmsi(
        r: *mut gsml3_registry,
        tmsi: u32,
    ) -> *mut gsml3_session;
    /// Create a session keyed by IMSI (1-15 ASCII digits). The assigned TMSI
    /// is readable via gsml3_session_assigned_tmsi. Sharded registries do not
    /// support this (NULL + ERR_UNSUPPORTED).
    pub fn gsml3_registry_create_by_imsi(
        r: *mut gsml3_registry,
        imsi: *const c_char,
    ) -> *mut gsml3_session;
    /// Look up by TMSI; NULL when not found.
    pub fn gsml3_registry_find_by_tmsi(
        r: *mut gsml3_registry,
        tmsi: u32,
    ) -> *mut gsml3_session;
    /// Look up by IMSI digits; NULL when not found.
    pub fn gsml3_registry_find_by_imsi(
        r: *mut gsml3_registry,
        imsi: *const c_char,
    ) -> *mut gsml3_session;
    /// Look up by the assigned channel: key = (trx_number, timeslot,
    /// lapdm_link). NULL when not found.
    pub fn gsml3_registry_find_by_link(
        r: *mut gsml3_registry,
        trx_number: c_uchar,
        timeslot: c_uchar,
        lapdm_link: c_uchar,
    ) -> *mut gsml3_session;
    /// Remove a session. 1 = removed, 0 = not found / unowned.
    pub fn gsml3_registry_remove(r: *mut gsml3_registry, s: *mut gsml3_session) -> c_int;
    /// Remove all sessions (plain registries only; sharded → no-op + ERR_UNSUPPORTED).
    pub fn gsml3_registry_clear(r: *mut gsml3_registry);
    /// Assign a channel to `s` and update the link index. `s` must belong to
    /// `r` — foreign session = no-op + ERR_INVALID_ARG. ch_type is a
    /// range-checked gsml3parser::ChannelType value.
    pub fn gsml3_registry_assign_channel(
        r: *mut gsml3_registry,
        s: *mut gsml3_session,
        ch_type: c_int,
        trx: c_uchar,
        ts: c_uchar,
        arfcn: u16,
        lapdm_link: c_uchar,
    );
    /// Release the channel of `s` and drop it from the link index. Same
    /// ownership rules as assign_channel.
    pub fn gsml3_registry_release_channel(r: *mut gsml3_registry, s: *mut gsml3_session);
    /// Tick all session timers; returns events written into the caller's
    /// buffer of `cap` (no zero-init needed; events that do not fit are
    /// re-armed and reported on a later tick — never dropped).
    pub fn gsml3_registry_tick_timers(
        r: *mut gsml3_registry,
        delta_ms: u32,
        expired_out: *mut gsml3_timer_expiry,
        cap: usize,
    ) -> usize;
    /// Tick all active session procedures; returns the number of timeouts.
    pub fn gsml3_registry_tick_procedures(r: *mut gsml3_registry, delta_ms: u32) -> usize;

    // Session accessors (all NULL-safe; setters no-op on NULL; BORROWED —
    // there is NO session free function).
    /// TMSI of the session identity (0 when the identity is not a TMSI).
    pub fn gsml3_session_tmsi(s: *mut gsml3_session) -> u32;
    /// TMSI under which the session is keyed in its registry (auto-assigned
    /// one for IMSI sessions); 0 when the session does not belong to a registry.
    pub fn gsml3_session_assigned_tmsi(s: *mut gsml3_session) -> u32;
    /// Change the identity TMSI. NOTE: this does NOT update the registry TMSI
    /// index (remove + create re-indexes).
    pub fn gsml3_session_set_tmsi(s: *mut gsml3_session, tmsi: u32);
    /// Set the IMSI (BCD digit string).
    pub fn gsml3_session_set_imsi(s: *mut gsml3_session, digits: *const c_char);
    pub fn gsml3_session_is_registered(s: *mut gsml3_session) -> c_int;
    pub fn gsml3_session_set_registered(s: *mut gsml3_session, v: c_int);
    pub fn gsml3_session_is_authenticated(s: *mut gsml3_session) -> c_int;
    pub fn gsml3_session_set_authenticated(s: *mut gsml3_session, v: c_int);
    pub fn gsml3_session_is_ciphered(s: *mut gsml3_session) -> c_int;
    pub fn gsml3_session_set_ciphered(s: *mut gsml3_session, v: c_int);
    /// Start/restart an L3 timer (GSML3_TIMER_*). Returns 1 on a fresh start,
    /// 0 on restart or out-of-range ID (the latter sets the thread-local error).
    pub fn gsml3_session_timer_start(s: *mut gsml3_session, timer_id: c_int) -> c_int;
    pub fn gsml3_session_timer_stop(s: *mut gsml3_session, timer_id: c_int);
    /// 1 if the timer is running.
    pub fn gsml3_session_timer_running(s: *mut gsml3_session, timer_id: c_int) -> c_int;
    /// Number of pending transactions.
    pub fn gsml3_session_transaction_pending(s: *mut gsml3_session) -> usize;

    // ── S7 Orchestrator + Responses (35) ───────────────────────────────────
    /// Create an orchestrator (one per session chain; owns the active procedure).
    pub fn gsml3_orchestrator_new() -> *mut gsml3_orchestrator;
    /// Free the orchestrator. NULL-safe.
    pub fn gsml3_orchestrator_free(o: *mut gsml3_orchestrator);
    /// Feed a parsed L3 message into the chain (by-value step result; see
    /// gsml3_step_result docs for the `reason` thread-local contract). The
    /// session may be NULL — steps without one are possible.
    pub fn gsml3_orchestrator_feed(
        o: *mut gsml3_orchestrator,
        msg: *const gsml3_message,
        s: *mut gsml3_session,
    ) -> gsml3_step_result;
    /// Feed an authentication challenge (rand: 16 octets wire order; sres:
    /// 4 octets big-endian, octet 0 = MSB).
    pub fn gsml3_orchestrator_feed_auth_challenge(
        o: *mut gsml3_orchestrator,
        rand: *const c_uchar,
        sres: *const c_uchar,
    ) -> gsml3_step_result;
    /// Feed a VLR decision. reject_cause: MMRejectCause value.
    pub fn gsml3_orchestrator_feed_vlr_decision(
        o: *mut gsml3_orchestrator,
        accept: c_int,
        has_new_tmsi: c_int,
        new_tmsi: u32,
        reject_cause: c_int,
    ) -> gsml3_step_result;
    /// Feed a ciphering decision.
    pub fn gsml3_orchestrator_feed_ciphering(o: *mut gsml3_orchestrator, algo: c_uchar, enable: c_int) -> gsml3_step_result;
    /// Feed a paging trigger (id_type: GSML3_ID_TMSI/IMSI; target_channel:
    /// gsml3parser::ChannelType).
    pub fn gsml3_orchestrator_feed_paging_trigger(
        o: *mut gsml3_orchestrator,
        id_type: c_int,
        tmsi: u32,
        imsi: *const c_char,
        target_channel: c_int,
    ) -> gsml3_step_result;
    /// Tick the chain timers; returns the number of procedure failures.
    pub fn gsml3_orchestrator_tick(o: *mut gsml3_orchestrator, delta_ms: u32) -> usize;
    /// Build the pending response into the caller's buffer; 0 on error
    /// (BUFFER_TOO_SMALL → use gsml3_orchestrator_required_size, or
    /// INVALID_VALUE → no pending response / missing parameter).
    pub fn gsml3_orchestrator_build_response(
        o: *mut gsml3_orchestrator,
        s: *const gsml3_session,
        out: *mut c_uchar,
        maxlen: usize,
    ) -> usize;
    /// Exact wire size of the pending response (zero allocation); 0 = nothing
    /// can be built (NULL session / no token / missing parameter).
    pub fn gsml3_orchestrator_required_size(
        o: *const gsml3_orchestrator,
        s: *const gsml3_session,
    ) -> usize;
    /// Drain the retransmission channel (consume-on-read); GSML3_TOKEN_*.
    pub fn gsml3_orchestrator_take_retransmit(o: *mut gsml3_orchestrator) -> c_int;
    /// Cancel the active chain.
    pub fn gsml3_orchestrator_cancel_all(o: *mut gsml3_orchestrator);
    /// Current chain phase (GSML3_PROC_*; UNKNOWN when idle).
    pub fn gsml3_orchestrator_chain_phase(o: *const gsml3_orchestrator) -> c_int;

    // Response builders (zero-alloc, caller's buffer). Cause parameters are
    // int enum values, range-checked (out-of-domain → 0 + ERR_INVALID_ARG).
    /// Build the response for `token` from the session's ResponseContext.
    pub fn gsml3_response_build_from_token(
        token: c_int,
        s: *const gsml3_session,
        out: *mut c_uchar,
        maxlen: usize,
    ) -> usize;
    /// Exact wire size for gsml3_response_build_from_token (0 = cannot build).
    pub fn gsml3_response_required_size(token: c_int, s: *const gsml3_session) -> usize;
    // 20 stateless response builders.
    pub fn gsml3_response_build_cm_service_accept(
        out: *mut c_uchar,
        maxlen: usize,
    ) -> usize;
    pub fn gsml3_response_build_cm_service_reject(
        out: *mut c_uchar,
        maxlen: usize,
        mm_cause: c_int,
    ) -> usize;
    pub fn gsml3_response_build_identity_request(
        out: *mut c_uchar,
        maxlen: usize,
        id_type: c_int,
    ) -> usize;
    /// rand: 16 octets (wire order).
    pub fn gsml3_response_build_authentication_request(
        out: *mut c_uchar,
        maxlen: usize,
        rand: *const c_uchar,
    ) -> usize;
    /// LAI as BCD digit STRINGS: mcc exactly 3 digits ("244"), mnc 2-3 digits.
    pub fn gsml3_response_build_location_updating_accept(
        out: *mut c_uchar,
        maxlen: usize,
        mcc: *const c_char,
        mnc: *const c_char,
        lac: u16,
        has_new_tmsi: c_int,
        new_tmsi: u32,
    ) -> usize;
    pub fn gsml3_response_build_location_updating_reject(
        out: *mut c_uchar,
        maxlen: usize,
        mm_cause: c_int,
    ) -> usize;
    /// LAI as BCD digit STRINGS (same convention as the builder above).
    pub fn gsml3_response_build_tmsi_reallocation_command(
        out: *mut c_uchar,
        maxlen: usize,
        mcc: *const c_char,
        mnc: *const c_char,
        lac: u16,
        tmsi: u32,
    ) -> usize;
    pub fn gsml3_response_build_channel_release(
        out: *mut c_uchar,
        maxlen: usize,
        rr_cause: c_int,
    ) -> usize;
    pub fn gsml3_response_build_ciphering_mode_command(
        out: *mut c_uchar,
        maxlen: usize,
        algo: c_uchar,
    ) -> usize;
    pub fn gsml3_response_build_physical_information(
        out: *mut c_uchar,
        maxlen: usize,
        ta: c_uchar,
    ) -> usize;
    pub fn gsml3_response_build_immediate_assignment(
        out: *mut c_uchar,
        maxlen: usize,
        type_and_offset: c_int,
        tn: c_uchar,
        tsc: c_uchar,
        arfcn: u16,
        ta: c_uchar,
    ) -> usize;
    pub fn gsml3_response_build_assignment_command(
        out: *mut c_uchar,
        maxlen: usize,
        type_and_offset: c_int,
        tn: c_uchar,
        tsc: c_uchar,
        arfcn: u16,
    ) -> usize;
    pub fn gsml3_response_build_call_proceeding(out: *mut c_uchar, maxlen: usize, ti: c_uchar) -> usize;
    pub fn gsml3_response_build_alerting(out: *mut c_uchar, maxlen: usize, ti: c_uchar) -> usize;
    pub fn gsml3_response_build_connect(out: *mut c_uchar, maxlen: usize, ti: c_uchar) -> usize;
    pub fn gsml3_response_build_connect_acknowledge(
        out: *mut c_uchar,
        maxlen: usize,
        ti: c_uchar,
    ) -> usize;
    pub fn gsml3_response_build_disconnect(
        out: *mut c_uchar,
        maxlen: usize,
        ti: c_uchar,
        cc_cause: c_int,
    ) -> usize;
    pub fn gsml3_response_build_release(
        out: *mut c_uchar,
        maxlen: usize,
        ti: c_uchar,
        cc_cause: c_int,
    ) -> usize;
    pub fn gsml3_response_build_release_complete(out: *mut c_uchar, maxlen: usize, ti: c_uchar) -> usize;
    /// NOTE (S7 convention): the digit string comes FIRST, then the TI.
    pub fn gsml3_response_build_setup(
        out: *mut c_uchar,
        maxlen: usize,
        called_digits: *const c_char,
        ti: c_uchar,
    ) -> usize;

    // ── Curated S9 typed builders for the v1 surface (2) ───────────────────
    /// Build a CM Service Request (L3). service_type: L3CMServiceType::TypeCode
    /// (1 = MobileOriginatedCall, 105 = LocationUpdateRequest — note only the
    /// 4-bit-code values fit); id_type: GSML3_ID_*; imsi: BCD digits or NULL.
    pub fn gsml3_build_cm_service_request(
        out: *mut c_uchar,
        maxlen: usize,
        service_type: c_int,
        id_type: c_int,
        tmsi: u32,
        imsi: *const c_char,
    ) -> usize;
    /// Build a CC Setup. NOTE (S9 convention): the TI comes FIRST, then the
    /// BCD digit string of called_digits (e.g. "123456789").
    pub fn gsml3_build_setup(
        out: *mut c_uchar,
        maxlen: usize,
        ti: c_uchar,
        called_digits: *const c_char,
    ) -> usize;
}
