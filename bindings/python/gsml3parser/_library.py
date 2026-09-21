# Copyright 2026 momentics <momentics@gmail.com>
# Copyright libgsml3parser contributors
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

"""Shared-library loader, ctypes declarations and the FFI test seam.

This module owns every contact point between Python and the C ABI
(``include/gsml3parser/gsml3parser_c.h``):

* **Loading** — finds and loads the prebuilt shared core once (cached,
  thread-safe). Search order: ``GSML3PARSER_LIBRARY`` (explicit file) ->
  ``GSML3PARSER_LIB_DIR`` (explicit directory, set by the unified gate to
  ``build_bindings/bin``) -> ``<repo>/build_bindings/bin`` (flat artifacts
  dir, decision #2) -> the regular core CMake build dirs (multi-config MSVC
  + single-config Unix) -> the platform library search path. After loading,
  the library must report ``GSML3_ABI_VERSION == EXPECTED_ABI`` or the
  import fails fast: a prebuilt binary may lag the header this module was
  written against, and continuing would be undefined behavior.

* **Declarations** — ctypes mirrors of every public C struct and callback
  type, plus ``PROTOTYPES``: ``(restype, argtypes)`` for ALL functions of
  the C ABI (238 at GSML3_ABI_VERSION 1, ABI
  inventory). At import time every entry is registered on the loaded
  library; a missing symbol or an unknown token raises immediately — this
  module never exposes a partially typed binding.

* **The test seam** — ``lib`` is a lazy proxy over ``_FUNCS``: each name
  resolves to one stable wrapper, and every invocation increments
  ``CALL_COUNTS[name]`` BEFORE delegating to ctypes. ``CALL_COUNTS`` is the
  fixed interface the closed-object tests use to prove that a code path
  performs NO FFI call at all (raw C handles have no "closed" state, so no
  C-side mirror test can exist; the seam is the proof). The hot C->Python
  callback path does NOT go through this proxy: the bridges read spans and
  append copies to queues directly (queue model, decision #3).

Token legend used in ``PROTOTYPES``:
    V     None            restype marker for C `void`
    I     c_int           C int (and all enum-mirroring ints)
    U8/I8 c_uint8/c_int8  byte fields
    U16   c_uint16        16-bit fields
    U32   c_uint32        uint32_t arguments and `unsigned` results alike:
                          the header has no other unsigned return, so a 32-bit
                          type matches the ABI on every supported platform
    SZ    c_size_t        size_t
    B     c_char_p        byte buffers, READ-ONLY positions (`const uint8_t*`
                          input). Pass bytes (wrappers normalize — this CPython
                          refuses bytearray/memview at a c_char_p arg); C borrows
                          the address only for the duration of the call
    PB    POINTER(c_ubyte) byte buffer at a WRITABLE position (`uint8_t* out`)
                          or wherever pointer arithmetic over the buffer is
                          needed: pass a (ctypes.c_ubyte * n) array — C writes
                          into it only during this single call
    CS    c_char_p        strings: WRAPPERS MUST PASS BYTES (CPython does not
                          convert str into c_char_p); with this restype, results
                          arrive as bytes copies of static/thread-local storage —
                          never free them
    H     c_void_p        opaque handle ARG type; None == NULL, and every
                          *_free / gsml3_free is documented NULL-safe
    P     c_void_p        raw pointer RESULT type: opaque handle constructors,
                          byte views (gsml3_rsl_l3) and library-allocated strings
                          freed with gsml3_free (hex/dump). Deliberately NOT
                          c_char_p: it copies through NUL — losing the pointer
                          that gsml3_free must release and truncating any data
                          containing a 0x00 byte. See the "copy then free" idiom.
    PC    c_void_p        void* user token (callback context)
    PS / P16 / P8V / PP8   out-parameter pointer types (size_t* / uint16_t* /
                          uint8_t* / const uint8_t**)
    PLAPDM / PEXP / PCH / PID / PLAI  POINTER(...) of the mirror structs below

L3CB / L1CB are ctypes mirrors of `gsml3_lapdm_l3_cb` / `gsml3_lapdm_l1_cb`:
the l3/frame spans they deliver are valid ONLY during the callback — copy
synchronously, never retain (gsml3parser_c.h "Entity callbacks").
"""

import ctypes
import ctypes.util
import os
import sys
import threading
from ctypes import POINTER
from pathlib import Path

from ._errors import INTERNAL, InternalError

#: Must equal GSML3_ABI_VERSION in gsml3parser_c.h; bump both together.
EXPECTED_ABI = 1

_LIB_NAME = {
    "win32": ("gsml3parser.dll",),
    "darwin": ("libgsml3parser.dylib",),
}
_DEFAULT_LIB_NAME = "libgsml3parser.so"


def _platform_lib_names():
    return list(_LIB_NAME.get(sys.platform, (_DEFAULT_LIB_NAME,)))


def _repo_root() -> Path:
    # This file lives in <root>/bindings/python/gsml3parser/.
    return Path(__file__).resolve().parents[3]


def _candidate_paths():
    """Ordered library search list; returns (path-like candidates, raw system entries)."""
    explicit = os.environ.get("GSML3PARSER_LIBRARY")
    if explicit:
        p = Path(explicit)
        if not p.is_file():
            raise InternalError(INTERNAL,
                f"GSML3PARSER_LIBRARY is set to {str(p)!r} but the file does not exist")
        return [p], []

    cands, raws = [], []
    names = _platform_lib_names()
    lib_dir = os.environ.get("GSML3PARSER_LIB_DIR")
    if lib_dir:
        # Explicit directory (the unified gate sets it to build_bindings/bin).
        cands += [Path(lib_dir) / name for name in names]
    root = _repo_root()
    cands.append(root / "build_bindings" / "bin" / names[0])
    if sys.platform == "win32":
        # Multi-config MSVC layout: Release first, then Debug.
        cands += [root / "build" / conf / names[0] for conf in ("Release", "Debug")]
    else:
        cands.append(root / "build" / _DEFAULT_LIB_NAME)
    found = ctypes.util.find_library("gsml3parser")  # last chance, may be None
    if found:
        raws.append(found)
    return cands, raws


_LIB = None
_lock = threading.Lock()


def _load():
    """Load the shared core once (thread-safe), ABI-check it, and return it."""
    global _LIB
    with _lock:
        if _LIB is not None:
            return _LIB

        tried = []
        cands, raws = _candidate_paths()
        lib = None
        for path in cands:
            tried.append(str(path))
            if path.is_file():
                try:
                    lib = ctypes.CDLL(str(path))
                    break
                except OSError as exc:
                    raise InternalError(INTERNAL, f"found but failed to load {path}: {exc}") from None
        if lib is None:
            for entry in raws:
                tried.append(f"<system> {entry!r}")
                try:
                    lib = ctypes.CDLL(entry)
                    break
                except OSError:
                    continue
        if lib is None:
            raise InternalError(INTERNAL,
                "could not find the libgsml3parser shared library. Searched:\n  "
                + "\n  ".join(tried)
                + "\nBuild it first (see scripts/verify_bindings.ps1): "
                "cmake -S <root> -B build_bindings -DBUILD_SHARED_LIBS=ON "
                "-DBUILD_TESTS=OFF -DBUILD_EXAMPLES=OFF && "
                "cmake --build build_bindings, then flatten the artifacts into "
                "build_bindings/bin — or point GSML3PARSER_LIBRARY / "
                "GSML3PARSER_LIB_DIR at them.")
        _LIB = lib
    return _LIB


# ── Mirror structs (verbatim layouts of gsml3parser_c.h) ───────────────

class LapdmFrameInfo(ctypes.Structure):
    """Mirrors ``gsml3_lapdm_frame_info``.

    ZERO-COPY: ``info`` points into the buffer that was passed to decode();
    treat it as a view of that buffer — never retain it beyond the call that
    filled the struct, and never free it.
    """

    _fields_ = [
        ("format", ctypes.c_int),     # GSML3_LAPDM_FMT_I=0 / _S=1 / _U=2
        ("u_type", ctypes.c_int),     # GSML3_LAPDM_U_* (format == U), else -1
        ("s_type", ctypes.c_int),     # GSML3_LAPDM_S_* (format == S), else -1
        ("nr", ctypes.c_uint8),       # receive sequence number (I/S frames)
        ("ns", ctypes.c_uint8),       # send sequence number (I frames)
        ("pf", ctypes.c_int),         # Poll/Final bit
        ("m_bit", ctypes.c_int),      # message-complete bit (I frames)
        ("sapi", ctypes.c_int),       # 0..15
        ("command", ctypes.c_int),    # C/R: 1 = command, 0 = response
        ("info", POINTER(ctypes.c_ubyte)),  # ZERO-COPY view into the input buffer
        ("info_len", ctypes.c_size_t),
    ]


class TimerExpiry(ctypes.Structure):
    """Mirrors ``gsml3_timer_expiry``: one session-timer expiry event."""

    _fields_ = [
        ("session", ctypes.c_void_p),  # BORROWED session handle; do not free it
        ("timer_id", ctypes.c_int),    # GSML3_TIMER_*
    ]


class StepResult(ctypes.Structure):
    """Mirrors ``gsml3_step_result`` (returned BY VALUE from feed*).

    ``reason`` points into thread-local library storage: it is valid only
    until the next gsml3_* call — copy it immediately after each feed.
    """

    _fields_ = [
        ("action", ctypes.c_int),         # GSML3_ACTION_*
        ("response_token", ctypes.c_int),  # GSML3_TOKEN_* (NONE=0 when none)
        ("final_state", ctypes.c_int),     # GSML3_STATE_*
        ("final_type", ctypes.c_int),      # GSML3_PROC_* (UNKNOWN=0xFF when idle)
        ("reason", ctypes.c_char_p),       # thread-local; NULL when empty
        ("error", ctypes.c_int),           # gsml3_error code; OK on a real step
    ]


class Channel(ctypes.Structure):
    """Mirrors ``gsml3_channel`` (GSM 04.08 channel description)."""

    _fields_ = [
        ("type_and_offset", ctypes.c_int),
        ("tn", ctypes.c_uint8),
        ("tsc", ctypes.c_uint8),
        ("arfcn", ctypes.c_uint16),
    ]


class MobileIdentity(ctypes.Structure):
    """Mirrors ``gsml3_mobile_identity``.

    ``imsi`` points into the owning gsml3_message handle (valid while that
    handle is alive); NULL for TMSI identities.
    """

    _fields_ = [
        ("type", ctypes.c_int),     # GSML3_ID_*
        ("tmsi", ctypes.c_uint32),  # valid when type == GSML3_ID_TMSI
        ("imsi", ctypes.c_char_p),  # view into the message handle; never free
    ]


class Lai(ctypes.Structure):
    """Mirrors ``gsml3_lai`` (Location Area Identity, numeric form)."""

    _fields_ = [
        ("mcc", ctypes.c_int),   # e.g. 244
        ("mnc", ctypes.c_int),   # e.g. 5
        ("lac", ctypes.c_uint16),
    ]


# ── Callback types and argument tokens ─────────────────────────────────

L3CB = ctypes.CFUNCTYPE(None, ctypes.c_int, ctypes.c_int,
                        POINTER(ctypes.c_ubyte), ctypes.c_size_t, ctypes.c_void_p)
# void (int sapi, int primitive, const uint8_t* l3, size_t l3_len, void* user)

L1CB = ctypes.CFUNCTYPE(None, POINTER(ctypes.c_ubyte),
                        ctypes.c_size_t, ctypes.c_void_p)
# void (const uint8_t* frame, size_t frame_len, void* user)

I = ctypes.c_int
U8 = ctypes.c_uint8
I8 = ctypes.c_int8
U16 = ctypes.c_uint16
U32 = ctypes.c_uint32
SZ = ctypes.c_size_t
B = ctypes.c_char_p      # read-only byte buffers (see module doc, token legend)
PB = POINTER(ctypes.c_ubyte)  # writable byte buffer / pointer-arithmetic positions
CS = ctypes.c_char_p     # strings; wrappers must pass bytes
H = ctypes.c_void_p      # opaque handle argument (None == NULL)
P = ctypes.c_void_p      # raw pointer result (handle ctors, views, alloc-ed strings)
PC = ctypes.c_void_p     # void* user token (callback context)
V = None                 # restype marker for C `void`

PS = POINTER(ctypes.c_size_t)    # size_t* out (rsl_l3 len, rsl_ie_get len)
P16 = POINTER(U16)               # uint16_t* out
P8V = POINTER(U8)                # uint8_t* out / in-out byte
PP8 = POINTER(POINTER(ctypes.c_ubyte))  # const uint8_t** out (rsl_ie_get value view)

PLAPDM = POINTER(LapdmFrameInfo)
PEXP = POINTER(TimerExpiry)
PCH = POINTER(Channel)
PID = POINTER(MobileIdentity)
PLAI = POINTER(Lai)


# ── PROTOTYPES: one entry per C ABI function — the full S1..S9 surface ──
# Transcribed 1:1 from include/gsml3parser/gsml3parser_c.h (GSML3_ABI_VERSION
# 1). test_api_surface.py enforces set equality with the header.
PROTOTYPES = {
    # ── S1 core (5) ──────────────────────────────────────────────────────
    "gsml3_version":               ("CS", ()),   # static storage; do not free
    "gsml3_abi_version":           ("U32", ()),  # `unsigned` in C -> c_uint32, exact ABI
    "gsml3_last_error":            ("CS", ()),   # thread-local, "" when none; never NULL
    "gsml3_last_error_code":       ("I",  ()),   # GSML3_OK when no pending error
    "gsml3_free":                  ("V",  ("H",)),  # frees char* results; NULL-safe
    # ── S2 config (4) ────────────────────────────────────────────────────
    "gsml3_config_new":            ("P",  ()),   # NULL on allocation failure
    "gsml3_config_set_log_level":  ("V",  ("H", "I")),  # GSML3_LOG_*; out-of-range ignored
    "gsml3_config_set_strict_framing": ("V", ("H", "I")),
    "gsml3_config_free":           ("V",  ("H",)),  # NULL-safe
    # ── S3 message (12) ──────────────────────────────────────────────────
    "gsml3_parse_l3":              ("P",  ("B", "SZ", "H")),   # cfg may be None/NULL
    "gsml3_parse_l3_hex":          ("P",  ("CS", "H")),        # spaces allowed, e.g. "60 0D 00"
    "gsml3_parse_l3_into":         ("I",  ("H", "B", "SZ", "H")),  # reparse in place; previous content kept on error
    "gsml3_message_free":          ("V",  ("H",)),
    "gsml3_message_name":          ("CS", ("H",)),  # static storage, "" for NULL
    "gsml3_message_pd":            ("I",  ("H",)),  # GSML3_PD_*, -1 for NULL
    "gsml3_message_mti":           ("I",  ("H",)),  # 0..255 (RR short 256..511), -1 for NULL
    "gsml3_message_ti":            ("I",  ("H",)),  # CC/SS 0..7 else 0; 0 for NULL
    "gsml3_message_size":          ("SZ", ("H",)),  # exact wire size, zero alloc; 0 for NULL
    "gsml3_message_write":         ("SZ", ("H", "PB", "SZ")),  # 0 on error / too small
    "gsml3_message_hex":           ("P",  ("H",)),  # allocated -> gsml3_free (copy then free)
    "gsml3_message_dump":          ("P",  ("H",)),  # allocated -> gsml3_free; NULL for NULL msg
    # ── S4 RSL / A-bis (25) ──────────────────────────────────────────────
    "gsml3_rsl_parse":             ("P",  ("B", "SZ")),   # handle owns a COPY of the input
    "gsml3_rsl_free":              ("V",  ("H",)),
    "gsml3_rsl_name":              ("CS", ("H",)),        # static storage
    "gsml3_rsl_discriminator":     ("I",  ("H",)),
    "gsml3_rsl_msg_type":          ("I",  ("H",)),
    "gsml3_rsl_chan_nr":           ("I",  ("H",)),
    "gsml3_rsl_link_id":           ("I",  ("H",)),
    "gsml3_rsl_bts_to_bsc":        ("I",  ("H",)),
    "gsml3_rsl_has_l3":            ("I",  ("H",)),
    # Raw byte-pointer VIEW into the handle's copy; None == no L3 (len still set).
    "gsml3_rsl_l3":                ("P",  ("H", "PS")),
    "gsml3_rsl_ie_count":          ("SZ", ("H",)),
    "gsml3_rsl_ie_get":            ("I",  ("H", "SZ", "P8V", "PS", "PP8")),
    "gsml3_rsl_build_data_req":        ("SZ", ("PB", "SZ", "U8", "U8", "B", "SZ")),
    "gsml3_rsl_build_data_ind":        ("SZ", ("PB", "SZ", "U8", "U8", "B", "SZ")),
    "gsml3_rsl_build_unit_data_req":   ("SZ", ("PB", "SZ", "U8", "U8", "B", "SZ")),
    "gsml3_rsl_build_unit_data_ind":   ("SZ", ("PB", "SZ", "U8", "U8", "B", "SZ")),
    "gsml3_rsl_build_chan_activ_ack":  ("SZ", ("PB", "SZ", "U8", "U16")),
    "gsml3_rsl_build_chan_activ_nack": ("SZ", ("PB", "SZ", "U8", "I")),  # cause range-checked
    "gsml3_rsl_build_rf_chan_rel_ack": ("SZ", ("PB", "SZ", "U8")),
    "gsml3_rsl_build_conn_fail":       ("SZ", ("PB", "SZ", "U8", "I")),  # cause range-checked
    "gsml3_rsl_build_meas_res":        ("SZ", ("PB", "SZ", "U8", "U8", "I8", "I8", "B", "SZ")),
    "gsml3_rsl_build_hando_det":       ("SZ", ("PB", "SZ", "U8", "U8")),
    "gsml3_rsl_build_ccch_load_ind":   ("SZ", ("PB", "SZ", "U8", "U16", "U16", "U16", "U16")),
    "gsml3_rsl_build_chan_rqd":        ("SZ", ("PB", "SZ", "U8", "U8", "U8", "U8", "U8", "U8")),
    "gsml3_rsl_build_delete_ind":      ("SZ", ("PB", "SZ", "U8", "B", "SZ")),
    # ── S5 LAPDm (16) ────────────────────────────────────────────────────
    "gsml3_lapdm_frame_decode":  ("I", ("PB", "SZ", "PLAPDM")),  # zero-copy: info points into data
    # Callbacks may be None (C-documented NULL); user = the context token.
    "gsml3_lapdm_entity_new":    ("P",  ("I", L3CB, L1CB, "PC")),  # invalid profile -> NULL
    "gsml3_lapdm_entity_free":   ("V",  ("H",)),                   # NULL-safe
    "gsml3_lapdm_entity_open":   ("V",  ("H", "I", "I")),          # sapi 0..15; 1 = BTS side (C/R=1)
    "gsml3_lapdm_entity_receive": ("V", ("H", "B", "SZ")),         # callbacks fire synchronously HERE
    "gsml3_lapdm_entity_send_ui": ("I", ("H", "I", "B", "SZ")),    # works in ANY state; address SAPI
    "gsml3_lapdm_entity_send_data": ("I", ("H", "B", "SZ")),       # I-frames; established link required
    "gsml3_lapdm_entity_send_sabme": ("I", ("H",)),                # LinkReleased required
    "gsml3_lapdm_entity_send_disc":  ("I", ("H",)),                # established required
    "gsml3_lapdm_entity_hard_release": ("V", ("H",)),
    "gsml3_lapdm_entity_tick_t200": ("I", ("H", "U32")),           # 1 / 0 / -1 (internal error)
    "gsml3_lapdm_entity_state":         ("I",   ("H",)),           # NULL-safe; GSML3_LAPDM_STATE_*
    "gsml3_lapdm_entity_is_established": ("I",  ("H",)),           # NULL-safe
    "gsml3_lapdm_entity_frames_sent":      ("U32", ("H",)),        # NULL-safe
    "gsml3_lapdm_entity_frames_received":  ("U32", ("H",)),        # NULL-safe
    "gsml3_lapdm_entity_retransmissions":  ("U32", ("H",)),        # NULL-safe
    # ── S6 registry + session (29) ───────────────────────────────────────
    # shard_count: 0 plain single-thread; 4/8/16/32 sharded thread-safe; else NULL.
    "gsml3_registry_new":      ("P",  ("I",)),
    "gsml3_registry_free":     ("V",  ("H",)),   # NULL-safe; frees every session (they are borrowed)
    "gsml3_registry_reserve":  ("V",  ("H", "SZ")),  # cold path pre-size
    "gsml3_registry_count":    ("SZ", ("H",)),
    "gsml3_registry_create_by_tmsi":  ("P",  ("H", "U32")),  # NULL: all-zero TMSI -> INVALID_ARG; duplicate -> DUPLICATE
    "gsml3_registry_create_by_imsi":  ("P",  ("H", "CS")),   # sharded -> NULL + UNSUPPORTED
    "gsml3_registry_find_by_tmsi":    ("P",  ("H", "U32")),  # NULL = not found (NOT an error)
    "gsml3_registry_find_by_imsi":    ("P",  ("H", "CS")),
    "gsml3_registry_find_by_link":    ("P",  ("H", "U8", "U8", "U8")),
    "gsml3_registry_remove":          ("I",  ("H", "H")),   # 1 = removed, 0 = not found / unowned
    "gsml3_registry_clear":           ("V",  ("H",)),       # plain only; sharded -> no-op + UNSUPPORTED
    "gsml3_registry_assign_channel":  ("V",  ("H", "H", "I", "U8", "U8", "U16", "U8")),  # foreign session -> no-op + INVALID_ARG
    "gsml3_registry_release_channel": ("V",  ("H", "H")),
    # expired_out: caller buffer of cap events, need NOT be pre-zeroed;
    # events that do not fit are re-armed (1 ms) — never dropped.
    "gsml3_registry_tick_timers":     ("SZ", ("H", "U32", "PEXP", "SZ")),
    "gsml3_registry_tick_procedures": ("SZ", ("H", "U32")),
    # Session access: BORROWED handle — there is NO free function on purpose.
    # All getters NULL-safe (sentinels 0/""), setters no-ops on NULL.
    "gsml3_session_tmsi":            ("U32", ("H",)),
    "gsml3_session_assigned_tmsi":   ("U32", ("H",)),   # 0 when not from a registry
    "gsml3_session_set_tmsi":        ("V",  ("H", "U32")),  # changes identity only — NOT re-indexed
    "gsml3_session_set_imsi":        ("V",  ("H", "CS")),
    "gsml3_session_is_registered":   ("I",  ("H",)),
    "gsml3_session_set_registered":  ("V",  ("H", "I")),
    "gsml3_session_is_authenticated": ("I", ("H",)),
    "gsml3_session_set_authenticated": ("V", ("H", "I")),
    "gsml3_session_is_ciphered":     ("I",  ("H",)),
    "gsml3_session_set_ciphered":    ("V",  ("H", "I")),
    # timer_id: GSML3_TIMER_*; start: 1 fresh, 0 restart or out-of-range (the
    # latter also sets the thread-local error).
    "gsml3_session_timer_start":     ("I",  ("H", "I")),
    "gsml3_session_timer_stop":      ("V",  ("H", "I")),
    "gsml3_session_timer_running":   ("I",  ("H", "I")),
    "gsml3_session_transaction_pending": ("SZ", ("H",)),
    # ── S7 orchestrator + responses (35) ─────────────────────────────────
    "gsml3_orchestrator_new":        ("P",  ()),
    "gsml3_orchestrator_free":       ("V",  ("H",)),   # NULL-safe
    # All feed* return the step BY VALUE; a NULL session is legal per the ABI.
    "gsml3_orchestrator_feed":              (StepResult, ("H", "H", "H")),
    # rand: 16 octets wire order; sres: 4 octets big-endian (octet 0 = MSB).
    "gsml3_orchestrator_feed_auth_challenge": (StepResult, ("H", "B", "B")),
    "gsml3_orchestrator_feed_vlr_decision":   (StepResult, ("H", "I", "I", "U32", "I")),
    "gsml3_orchestrator_feed_ciphering":      (StepResult, ("H", "U8", "I")),
    "gsml3_orchestrator_feed_paging_trigger": (StepResult, ("H", "I", "U32", "CS", "I")),
    "gsml3_orchestrator_tick":            ("SZ", ("H", "U32")),  # number of procedure failures
    "gsml3_orchestrator_build_response":  ("SZ", ("H", "H", "PB", "SZ")),  # 0: too-small OR nothing pending
    "gsml3_orchestrator_required_size":   ("SZ", ("H", "H")),     # exact size, zero alloc; 0 -> nothing buildable
    "gsml3_orchestrator_take_retransmit": ("I",  ("H",)),         # consume-on-read token; NONE=0 when none
    "gsml3_orchestrator_cancel_all":      ("V",  ("H",)),
    "gsml3_orchestrator_chain_phase":     ("I",  ("H",)),         # GSML3_PROC_*; UNKNOWN=0xFF when idle
    "gsml3_response_build_from_token":    ("SZ", ("I", "H", "PB", "SZ")),
    "gsml3_response_required_size":       ("SZ", ("I", "H")),
    # 20 stateless builders: cause params are range-checked in C (INVALID_ARG).
    # NOTE the S7 LAI string form: mcc/mnc are `const char*` here ("244"/"05").
    "gsml3_response_build_cm_service_accept":         ("SZ", ("PB", "SZ")),
    "gsml3_response_build_cm_service_reject":         ("SZ", ("PB", "SZ", "I")),
    "gsml3_response_build_identity_request":          ("SZ", ("PB", "SZ", "I")),
    "gsml3_response_build_authentication_request":    ("SZ", ("PB", "SZ", "B")),  # rand[16] wire order
    "gsml3_response_build_location_updating_accept":  ("SZ", ("PB", "SZ", "CS", "CS", "U16", "I", "U32")),
    "gsml3_response_build_location_updating_reject":  ("SZ", ("PB", "SZ", "I")),
    "gsml3_response_build_tmsi_reallocation_command": ("SZ", ("PB", "SZ", "CS", "CS", "U16", "U32")),
    "gsml3_response_build_channel_release":           ("SZ", ("PB", "SZ", "I")),
    "gsml3_response_build_ciphering_mode_command":    ("SZ", ("PB", "SZ", "U8")),
    "gsml3_response_build_physical_information":      ("SZ", ("PB", "SZ", "U8")),
    "gsml3_response_build_immediate_assignment":      ("SZ", ("PB", "SZ", "I", "U8", "U8", "U16", "U8")),
    "gsml3_response_build_assignment_command":        ("SZ", ("PB", "SZ", "I", "U8", "U8", "U16")),
    "gsml3_response_build_call_proceeding":           ("SZ", ("PB", "SZ", "U8")),
    "gsml3_response_build_alerting":                  ("SZ", ("PB", "SZ", "U8")),
    "gsml3_response_build_connect":                   ("SZ", ("PB", "SZ", "U8")),
    "gsml3_response_build_connect_acknowledge":       ("SZ", ("PB", "SZ", "U8")),
    "gsml3_response_build_disconnect":                ("SZ", ("PB", "SZ", "U8", "I")),
    "gsml3_response_build_release":                   ("SZ", ("PB", "SZ", "U8", "I")),
    "gsml3_response_build_release_complete":          ("SZ", ("PB", "SZ", "U8")),
    "gsml3_response_build_setup":                     ("SZ", ("PB", "SZ", "CS", "U8")),  # (called_digits, ti) order
    # ── S8 typed getters (69): (const gsml3_message*, ...) — Python v1 only ─
    # Sentinels: -1/0/None on wrong type. Out-struct variants fill a ctypes
    # mirror; the wrapper layer converts them to plain values.
    "gsml3_msg_channel_release_cause":                ("I",   ("H",)),
    "gsml3_msg_channel_release_gprs_resumption":      ("I",   ("H",)),  # 1/0 present, -1 absent
    "gsml3_msg_channel_request_ra":                   ("I",   ("H",)),
    "gsml3_msg_immediate_assignment_channel":         ("I",   ("H", "PCH")),
    "gsml3_msg_immediate_assignment_ta":              ("I",   ("H",)),
    "gsml3_msg_immediate_assignment_reject_wait_time": ("I",  ("H",)),
    "gsml3_msg_assignment_command_channel":           ("I",   ("H", "PCH")),
    "gsml3_msg_assignment_complete_cause":            ("I",   ("H",)),
    "gsml3_msg_assignment_failure_cause":             ("I",   ("H",)),
    "gsml3_msg_paging_request_type1_count":           ("I",   ("H",)),  # 1..2
    "gsml3_msg_paging_request_type1_identity":        ("I",   ("H", "I", "PID")),
    "gsml3_msg_paging_request_type2_tmsi":            ("U32", ("H", "I")),  # 0 out-of-range (2 slots)
    "gsml3_msg_paging_request_type3_tmsi":            ("U32", ("H", "I")),  # 4 slots
    "gsml3_msg_paging_response_cks":                  ("I",   ("H",)),
    "gsml3_msg_paging_response_identity":             ("I",   ("H", "PID")),
    "gsml3_msg_ciphering_mode_command_ciphering":     ("I",   ("H",)),
    "gsml3_msg_ciphering_mode_command_algorithm":     ("I",   ("H",)),
    "gsml3_msg_ciphering_mode_complete_response":     ("I",   ("H",)),
    "gsml3_msg_ciphering_mode_complete_has_imeisv":   ("I",   ("H",)),
    "gsml3_msg_handover_complete_cause":              ("I",   ("H",)),
    "gsml3_msg_handover_command_cell":                ("I",   ("H", "P16", "P8V", "P8V")),
    "gsml3_msg_physical_information_ta":              ("I",   ("H",)),
    # MM getters
    "gsml3_msg_cm_service_request_service_type":      ("I",   ("H",)),  # L3CMServiceType::TypeCode
    "gsml3_msg_cm_service_request_identity":          ("I",   ("H", "PID")),
    "gsml3_msg_cm_service_reject_cause":              ("I",   ("H",)),
    "gsml3_msg_cm_service_abort_cause":               ("I",   ("H",)),
    "gsml3_msg_identity_request_type":                ("I",   ("H",)),  # MobileIDType
    "gsml3_msg_identity_response_identity":           ("I",   ("H", "PID")),
    "gsml3_msg_location_updating_request_update_type": ("I",  ("H",)),   # 0 Normal / 1 Periodic / 2 IMSI Attach
    "gsml3_msg_location_updating_request_identity":   ("I",   ("H", "PID")),
    "gsml3_msg_location_updating_request_lai":        ("I",   ("H", "PLAI")),
    "gsml3_msg_location_updating_accept_lai":         ("I",   ("H", "PLAI")),
    "gsml3_msg_location_updating_accept_identity":    ("I",   ("H", "PID")),
    "gsml3_msg_location_updating_reject_cause":       ("I",   ("H",)),
    "gsml3_msg_authentication_request_cks":           ("I",   ("H",)),
    # Copies the 16-octet RAND (wire order) into rand[16]; 0 on wrong type.
    "gsml3_msg_authentication_request_rand":          ("I", ("H", "PB")),
    "gsml3_msg_authentication_response_sres":         ("U32", ("H",)),
    "gsml3_msg_tmsi_reallocation_command_lai":        ("I",   ("H", "PLAI")),
    "gsml3_msg_tmsi_reallocation_command_tmsi":       ("U32", ("H",)),
    "gsml3_msg_imsi_detach_indication_identity":      ("I",   ("H", "PID")),
    # CC getters
    "gsml3_msg_setup_ti":                             ("I",  ("H",)),
    "gsml3_msg_setup_have_called_party":              ("I",  ("H",)),
    # BCD digit string into the handle (None when absent) — a VIEW, not alloc-ed.
    "gsml3_msg_setup_called_number":                  ("CS", ("H",)),
    "gsml3_msg_call_proceeding_ti":                   ("I",  ("H",)),
    "gsml3_msg_alerting_ti":                          ("I",  ("H",)),
    "gsml3_msg_connect_ti":                           ("I",  ("H",)),
    "gsml3_msg_connect_acknowledge_ti":               ("I",  ("H",)),
    "gsml3_msg_disconnect_ti":                        ("I",  ("H",)),
    "gsml3_msg_disconnect_cause":                     ("I",  ("H",)),   # CCCause
    "gsml3_msg_release_ti":                           ("I",  ("H",)),
    "gsml3_msg_release_have_cause":                   ("I",  ("H",)),
    "gsml3_msg_release_cause":                        ("I",  ("H",)),
    "gsml3_msg_release_complete_ti":                  ("I",  ("H",)),
    "gsml3_msg_release_complete_have_cause":          ("I",  ("H",)),
    "gsml3_msg_release_complete_cause":               ("I",  ("H",)),
    "gsml3_msg_facility_ti":                          ("I",  ("H",)),
    "gsml3_msg_facility_body":                        ("SZ", ("H", "PB", "SZ")),
    # SMS getters
    "gsml3_msg_cp_data_rpdu":                         ("SZ", ("H", "PB", "SZ")),
    "gsml3_msg_cp_status_tp_oi":                      ("I",  ("H",)),
    "gsml3_msg_cp_status_mti_value":                  ("I",  ("H",)),
    "gsml3_msg_cp_status_has_message_ref":            ("I",  ("H",)),
    "gsml3_msg_cp_status_message_ref":                ("I",  ("H",)),
    "gsml3_msg_cp_smt_rpdu":                          ("SZ", ("H", "PB", "SZ")),
    "gsml3_msg_sms_deliver_tp_mti":                   ("I",  ("H",)),
    "gsml3_msg_sms_deliver_tp_mr":                    ("I",  ("H",)),
    "gsml3_msg_sms_deliver_has_tp_ud":                ("I",  ("H",)),
    "gsml3_msg_sms_deliver_tp_ud":                    ("SZ", ("H", "PB", "SZ")),
    # SS getters
    "gsml3_msg_sup_serv_facility_ti":                 ("I",  ("H",)),
    "gsml3_msg_sup_serv_facility_data":               ("SZ", ("H", "PB", "SZ")),
    # ── S9 typed builders (43): zero-alloc into the caller's buffer ──────
    # cause/type params are range-checked in C (INVALID_ARG, no frame built);
    # NOTE the S9 LAI numeric form: mcc/mnc are plain `int` here (244 / 5),
    # unlike the string forms of the S7 response builders.
    "gsml3_build_channel_release":                 ("SZ", ("PB", "SZ", "I")),
    "gsml3_build_channel_request":                 ("SZ", ("PB", "SZ", "U8")),
    "gsml3_build_immediate_assignment":            ("SZ", ("PB", "SZ", "I", "U8", "U8", "U16", "U8", "U8")),
    "gsml3_build_immediate_assignment_reject":     ("SZ", ("PB", "SZ", "U8")),
    "gsml3_build_assignment_command":              ("SZ", ("PB", "SZ", "I", "U8", "U8", "U16")),
    "gsml3_build_assignment_complete":             ("SZ", ("PB", "SZ", "I")),
    "gsml3_build_assignment_failure":              ("SZ", ("PB", "SZ", "I")),
    "gsml3_build_paging_request_type1":            ("SZ", ("PB", "SZ", "U32")),  # tmsi != all-zero
    "gsml3_build_paging_request_type2":            ("SZ", ("PB", "SZ", "U32", "U32")),
    "gsml3_build_paging_request_type3":            ("SZ", ("PB", "SZ", "U32", "U32", "U32", "U32")),
    "gsml3_build_paging_response":                 ("SZ", ("PB", "SZ", "I", "U32", "CS")),
    "gsml3_build_ciphering_mode_command":          ("SZ", ("PB", "SZ", "U8")),
    "gsml3_build_ciphering_mode_complete":         ("SZ", ("PB", "SZ", "I")),
    "gsml3_build_handover_complete":               ("SZ", ("PB", "SZ", "I")),
    "gsml3_build_physical_information":            ("SZ", ("PB", "SZ", "U8")),
    "gsml3_build_cm_service_request":              ("SZ", ("PB", "SZ", "I", "I", "U32", "CS")),  # service_type, id_type, tmsi, imsi|None
    "gsml3_build_cm_service_accept":               ("SZ", ("PB", "SZ")),
    "gsml3_build_cm_service_reject":               ("SZ", ("PB", "SZ", "I")),
    "gsml3_build_cm_service_abort":                ("SZ", ("PB", "SZ", "I")),
    "gsml3_build_identity_request":                ("SZ", ("PB", "SZ", "I")),
    "gsml3_build_identity_response":               ("SZ", ("PB", "SZ", "I", "U32", "CS")),
    # LAI numeric form: mcc/mnc are int (244 / 5), lac uint16.
    "gsml3_build_location_updating_request":       ("SZ", ("PB", "SZ", "I", "I", "U32", "CS", "I", "I", "U16")),
    "gsml3_build_location_updating_accept":        ("SZ", ("PB", "SZ", "I", "I", "U16", "I", "U32")),
    "gsml3_build_location_updating_reject":        ("SZ", ("PB", "SZ", "I")),
    "gsml3_build_authentication_request":          ("SZ", ("PB", "SZ", "U8", "B")),  # cksn, rand[16] wire order
    "gsml3_build_authentication_response":         ("SZ", ("PB", "SZ", "U32")),  # sres big-endian
    "gsml3_build_tmsi_reallocation_command":       ("SZ", ("PB", "SZ", "I", "I", "U16", "U32")),
    "gsml3_build_tmsi_reallocation_complete":      ("SZ", ("PB", "SZ")),
    "gsml3_build_imsi_detach_indication":          ("SZ", ("PB", "SZ", "I", "U32", "CS")),
    "gsml3_build_setup":                           ("SZ", ("PB", "SZ", "U8", "CS")),  # (ti, called_digits) order
    "gsml3_build_call_proceeding":                 ("SZ", ("PB", "SZ", "U8")),
    "gsml3_build_alerting":                        ("SZ", ("PB", "SZ", "U8")),
    "gsml3_build_connect":                         ("SZ", ("PB", "SZ", "U8")),
    "gsml3_build_connect_acknowledge":             ("SZ", ("PB", "SZ", "U8")),
    "gsml3_build_disconnect":                      ("SZ", ("PB", "SZ", "U8", "I")),
    "gsml3_build_release":                         ("SZ", ("PB", "SZ", "U8", "I")),
    "gsml3_build_release_complete":                ("SZ", ("PB", "SZ", "U8")),
    "gsml3_build_facility":                        ("SZ", ("PB", "SZ", "U8", "B", "SZ")),
    "gsml3_build_cp_data":                         ("SZ", ("PB", "SZ", "B", "SZ")),
    "gsml3_build_cp_status":                       ("SZ", ("PB", "SZ", "U8", "U8", "I", "U8")),
    "gsml3_build_cp_smt":                          ("SZ", ("PB", "SZ", "B", "SZ")),
    "gsml3_build_sms_deliver":                     ("SZ", ("PB", "SZ", "U8", "U8", "B", "SZ")),
    "gsml3_build_sup_serv_facility":               ("SZ", ("PB", "SZ", "U8", "B", "SZ")),
}


def _resolve_token(token):
    """Map a PROTOTYPES token to the ctypes object (strings -> _TOKENS, classes pass through)."""
    if isinstance(token, str):
        try:
            return _TOKENS[token]
        except KeyError:
            raise InternalError(
                f"unknown PROTOTYPES token {token!r} (a binding bug — "
                f"known tokens: {sorted(_TOKENS)})") from None
    return token


_TOKENS = {
    "V": V, "I": I, "U8": U8, "I8": I8, "U16": U16, "U32": U32, "SZ": SZ,
    "B": B, "CS": CS, "H": H, "P": P, "PC": PC,
    "PB": PB,
    "PS": PS, "P16": P16, "P8V": P8V, "PP8": PP8,
    "PLAPDM": PLAPDM, "PEXP": PEXP, "PCH": PCH, "PID": PID, "PLAI": PLAI,
    "StepResult": StepResult,
}


class _FuncProxy:
    """One stable, call-counting wrapper around a registered C function."""

    __slots__ = ("name", "_fn")

    def __init__(self, name, fn):
        self.name = name
        self._fn = fn

    def __call__(self, *args, **kwargs):
        # Fixed test seam: every REAL FFI call is counted exactly once here,
        # before the delegate (one dict update — cheap at this boundary).
        CALL_COUNTS[self.name] = CALL_COUNTS.get(self.name, 0) + 1
        return self._fn(*args, **kwargs)

    def __repr__(self):  # pragma: no cover - debug aid
        return f"<gsml3 C function {self.name!r}>"


class _LibraryProxy:
    """Lazy accessor over ``_FUNCS`` — the only supported call path.

    Attribute access never fails silently: names that are not part of the
    registered C ABI raise AttributeError, so typos and ABI drift surface
    immediately instead of as ctypes errors deep inside a callback.
    """

    def __getattr__(self, name):
        try:
            return _FUNCS[name]
        except KeyError:
            raise AttributeError(
                f"'{name}' is not a registered gsml3parser C function") from None


#: Cache of the registered (counting) wrappers — one per C function.
_FUNCS = {}
#: Counters of REAL FFI calls per function name — the closed-path test seam.
CALL_COUNTS = {}


def _register_all():
    lib = _load()
    for name, (rest_tok, arg_toks) in PROTOTYPES.items():
        try:
            fn = getattr(lib, name)
        except (OSError, AttributeError) as exc:
            raise InternalError(
                f"symbol {name!r} not found in the shared library "
                f"(ABI drift — the prebuilt binary does not match this header): {exc}") from None
        try:
            fn.restype = _resolve_token(rest_tok)
            fn.argtypes = tuple(_resolve_token(t) for t in arg_toks)
        except Exception as exc:
            raise InternalError(
                f"failed to register prototype for {name!r} (a binding bug): {exc!r}") from None
        _FUNCS[name] = _FuncProxy(name, fn)
    # ABI guard, after registration (needs the typed restype to be in place).
    reported = int(_FUNCS["gsml3_abi_version"]())
    if reported != EXPECTED_ABI:
        raise InternalError(INTERNAL,
            f"ABI mismatch: header expects GSML3_ABI_VERSION {EXPECTED_ABI}, "
            f"library reports {reported} (a prebuilt binary may lag the header — rebuild)")


_register_all()

#: The public call path — every C function of the ABI, counting and typed.
lib = _LibraryProxy()
