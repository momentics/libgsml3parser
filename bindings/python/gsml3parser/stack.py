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

"""High-level Python API over the libgsml3parser C ABI.

Ownership (mirrors the C header "ABI rules"):
  * OWNED handles — Config / Message / RslFrame / Registry / Orchestrator /
    LapdmEntity — are created here and released EXACTLY ONCE by the matching
    gsml3_*_free. Every wrapper is RAII: idempotent ``close()`` + context
    manager + guarded ``__del__``; the raw handle is taken to None before the
    free, so double free / use-after-close is structurally impossible and
    every closed path performs NO FFI (raises ClosedGsmL3ObjectError).
  * Session is BORROWED: owned by its Registry, and there is deliberately no
    free for it (gsml3_registry_free releases all sessions). Every accessor
    re-checks the owning registry's closed flag instead of touching a dead
    pointer; never keep a Session alive past its registry.

Threading: owned handles are single-thread per the C ABI. A Registry created
with shard_count > 0 makes registry-mediated calls thread-safe; direct
Session access runs without any registry lock in either flavor (caller
synchronizes, one thread per session). GsmL3Stack serializes its
receive/drain/orchestrate path under one RLock and is not meant to be shared
concurrently.

Callback model (queue model): LAPDm entity callbacks fire
SYNCHRONOUSLY inside C calls with spans valid only for their duration, and
the ABI forbids freeing the owning entity (or mutating its FSM) from within
its own callbacks. The stack's bridges therefore do ONLY memory-safe work —
read the span (zero-copy view), append one copy under the stack lock, make NO
FFI call; orchestration, response building and transmission happen in Python
AFTER the C call returned (``GsmL3Stack.send_frame``). Both CFUNCTYPE objects
and the guard context they close over are anchored in
``GsmL3Stack._c_cb_keepalive`` — the SOLE GC anchor on the stack side: C
stores only raw function pointers, and a collected closure would mean C
jumping into dead memory.

Errors: failures raise typed GsmL3Error subclasses carrying the C code plus a
SYNCHRONOUS copy of the thread-local message (see _errors). Wrapper-level
validation rejects None / empty / too-short inputs BEFORE the FFI boundary
(NULL policy); the C-documented NULL-safe entry points (metadata sentinels,
*_free, entity state/stats) pass NULL through at the raw level.
"""

from __future__ import annotations

import ctypes
import threading
from ctypes import byref, c_size_t
from dataclasses import dataclass

from ._errors import (
    OK, CODE_CLOSED,
    GsmL3Error, InvalidArgument, UnsupportedOperation, BufferTooSmall,
    GsmL3NoMemory, InternalError, ClosedGsmL3ObjectError,
    raise_last_error,
)
from . import _lapdm as lapdm_mini
from ._library import (
    L1CB, L3CB,
    LapdmFrameInfo as _LapdmFrameInfoC,
    StepResult as _StepResultC,
    TimerExpiry as _TimerExpiryC,
    Channel as _ChannelC,
    MobileIdentity as _MobileIdentityC,
    Lai as _LaiC,
    lib,
)

# ── Enum-mirror constants for the demo surface and tests (C values 1:1) ────

PD_GCC, PD_BCC = 0x00, 0x01
PD_CC = 0x03
PD_MM, PD_RR = 0x05, 0x06
PD_GMM, PD_SMS = 0x08, 0x09
PD_SM, PD_SS = 0x0A, 0x0B
PD_LS, PD_EXT, PD_TST = 0x0C, 0x0E, 0x0F

ID_NO_ID, ID_IMSI, ID_IMEI, ID_IMEISV, ID_TMSI = 0, 1, 2, 3, 4

TOKEN_NONE = 0
TOKEN_CM_SERVICE_ACCEPT = 10
TOKEN_CALL_PROCEEDING = 17

PRIM_L3_UNIT_DATA = 4
PRIM_L3_ESTABLISH_CONFIRM = 7

LAPDM_STATE_UNUSED = 0
LAPDM_STATE_LINK_RELEASED = 1
LAPDM_STATE_AWAITING_ESTABLISH = 2
LAPDM_STATE_AWAITING_RELEASE = 3
LAPDM_STATE_LINK_ESTABLISHED = 4

PROC_CALL_SETUP_MO = 0x04
PROC_UNKNOWN = 0xFF

ACTION_CONTINUE, ACTION_SEND_RESPONSE = 0, 1

#: Fixed-size caller buffer for the stateless (S7/S9/RSL) builders —
#: decision #8: protocol-bounded frames fit comfortably; rc==0 surfaces
#: BUFFER_TOO_SMALL as an error and is NEVER retried with a grown buffer.
_BUILDER_BUF = 512


# ── Python-level value objects ───────────────────────────────────────────

@dataclass(frozen=True)
class StepResult:
    """One orchestrator step (C result returned by value).

    ``reason`` is ALREADY a synchronous str copy — the thread-local C string
    is invalidated by the next successful gsml3_* call, so it was copied
    inside feed. Use ``code`` / ``token`` explicitly for control flow.
    """

    action: int           # GSML3_ACTION_* (SEND_RESPONSE = 1, ...)
    token: int            # GSML3_TOKEN_* (TOKEN_NONE when none)
    final_state: int      # GSML3_STATE_*
    final_type: int       # GSML3_PROC_* (PROC_UNKNOWN when idle)
    reason: str
    code: int             # gsml3_error code; OK on a real step

    @property
    def ok(self) -> bool:
        return self.code == OK


@dataclass(frozen=True)
class L3Event:
    """One decoded L3 delivery captured by the bridge. ``data`` is an owned
    bytes COPY — the C span was valid only during the callback."""

    sapi: int
    primitive: int        # GSML3_PRIM_*
    data: bytes           # b"" for link-state primitives (no payload)


@dataclass(frozen=True)
class FrameInfo:
    """Result of :func:`lapdm_frame_decode`.

    ``info`` is a copy of the payload; ``info_offset`` + ``info_len`` locate
    that region INSIDE the input frame — the C decoder's info pointer was a
    zero-copy view into it (the tests verify the relationship against the raw
    bytes).
    """

    format: int           # GSML3_LAPDM_FMT_*
    u_type: int           # GSML3_LAPDM_U_* when format == U, else -1
    s_type: int           # GSML3_LAPDM_S_* when format == S, else -1
    nr: int               # receive sequence number (I/S frames)
    ns: int               # send sequence number (I frames)
    pf: int               # Poll/Final bit
    m_bit: int            # message-complete bit (I frames)
    sapi: int             # 0..15
    command: int          # C/R: 1 = command, 0 = response
    info_offset: int | None   # None when the frame has no info field
    info_len: int
    info: bytes           # payload copy (b"" when absent)


@dataclass(frozen=True)
class TimerExpiryPy:
    """One session-timer expiry from Registry.tick_timers. ``session`` is the
    BORROWED view into the same registry — valid while that registry is open
    and the session has not been removed."""

    session: "Session"
    timer_id: int         # GSML3_TIMER_*


@dataclass(frozen=True)
class Channel:
    """gsml3_channel mirror (S8 out-parameter value object)."""
    type_and_offset: int  # gsml3parser::TypeAndOffset
    tn: int               # 0..7
    tsc: int              # 0..7
    arfcn: int            # 0..1023


@dataclass(frozen=True)
class MobileIdentity:
    """gsml3_mobile_identity mirror (S8 out-parameter). ``imsi`` is a str COPY
    of the message-handle-internal view (the C pointer is never handed out)."""
    type: int             # GSML3_ID_*
    tmsi: int             # valid when type == ID_TMSI
    imsi: str | None


@dataclass(frozen=True)
class Lai:
    """gsml3_lai mirror (S8 out-parameter, numeric form)."""
    mcc: int              # e.g. 244
    mnc: int              # e.g. 5
    lac: int


# ── Internal validation / conversion helpers (NULL policy: before the FFI) ──

def _enc(s, what):
    """str | bytes | None -> the exact bytes C expects for a char* parameter.

    Modern CPython refuses to convert str into c_char_p at the boundary
    (verified on 3.14), so wrappers encode explicitly; non-ASCII fails with
    UnicodeEncodeError BEFORE any FFI call (all char* parameters of this ABI
    are ASCII digit/hex strings by protocol).
    """
    if s is None:
        return None
    if isinstance(s, bytes):
        return s
    if isinstance(s, str):
        return s.encode("ascii")
    raise TypeError(f"{what} must be a str or bytes, got {type(s).__name__}")


def _byteslike(v, what) -> bytes:
    if v is None:
        raise TypeError(f"{what} must be a bytes-like object, got None")
    if isinstance(v, (bytes, bytearray, memoryview)):
        return bytes(v)
    raise TypeError(f"{what} must be a bytes-like object, got {type(v).__name__}")


def _as_int(v, what, lo=None, hi=None) -> int:
    if isinstance(v, bool) or not isinstance(v, int):
        raise TypeError(f"{what} must be an int, got {type(v).__name__}")
    if lo is not None and v < lo:
        raise ValueError(f"{what} out of range ({lo}..{hi}): {v!r}")
    if hi is not None and v > hi:
        raise ValueError(f"{what} out of range ({lo}..{hi}): {v!r}")
    return v


def _as_u8(v, what) -> int:
    return _as_int(v, what, lo=0, hi=255)


def _as_i8(v, what) -> int:
    return _as_int(v, what, lo=-128, hi=127)


def _as_u16(v, what) -> int:
    return _as_int(v, what, lo=0, hi=65535)


def _as_user(user):
    """void* user token: None (NULL), an int address, a ctypes Structure (passed byref),
    or a c_void_p instance (e.g. a cast of a byref — stable while the target lives)."""
    if user is None:
        return None
    if isinstance(user, bool):
        raise TypeError("user must be None, an int address, a ctypes Structure "
                        f"or a c_void_p instance; got {type(user).__name__}")
    if isinstance(user, int):
        return user
    if isinstance(user, (ctypes.c_void_p, ctypes.Array)):
        return user  # byref()/cast() results carry their own storage — pass through
    if isinstance(user, ctypes.Structure):
        return byref(user)
    raise TypeError("user must be None, an int address, a ctypes Structure "
                    f"or a c_void_p instance; got {type(user).__name__}")


def _cfg_handle(cfg) -> int | None:
    if cfg is None:
        return None  # C default config (documented NULL-tolerant)
    if not isinstance(cfg, Config):
        raise TypeError(f"cfg must be a Config or None, got {type(cfg).__name__}")
    cfg._check()
    return cfg._h


def _alloc_string(p, op: str) -> str:
    """Copy-then-free idiom for the char* results (message hex/dump): the raw
    pointer restype exists precisely so this release cannot be lost to a
    NUL-truncating copy. The free happens WITHIN the same Python call as the
    copy — no FFI or allocation may run between them."""
    if p is None:
        raise_last_error(lib, None, op)
    text = ctypes.string_at(p).decode("utf-8", "replace")
    lib.gsml3_free(p)
    return text


def _lai_from(c_lai) -> Lai:
    return Lai(int(c_lai.mcc), int(c_lai.mnc), int(c_lai.lac))


def _identity_from(c_id) -> MobileIdentity:
    imsi = c_id.imsi  # view into the message handle — copy it out now
    return MobileIdentity(type=int(c_id.type), tmsi=int(c_id.tmsi),
                          imsi=None if imsi is None else imsi.decode("ascii", "replace"))


# ── LAPDm frame decode (C decoder wrapper; zero-copy view, reported as offsets)

def lapdm_frame_decode(frame) -> FrameInfo:
    """Decode one raw LAPDm frame with the C zero-copy decoder.

    The C struct's ``info`` points INSIDE the input buffer (zero-copy). The
    wrapper reports that fact as ``info_offset``/``info_len`` and hands out a
    bytes COPY — Python has no safe way to hand raw C pointers to callers,
    and the copy happens while the input is provably alive (one call frame).
    None / non-bytes input raises TypeError; len < 2 raises ValueError BEFORE
    the FFI boundary (NULL policy: a frame needs at least address + control).
    """
    if frame is None or isinstance(frame, bool):
        raise TypeError("frame must be a bytes-like object, got "
                        f"{type(frame).__name__}")
    if not isinstance(frame, (bytes, bytearray, memoryview)):
        raise TypeError(f"frame must be a bytes-like object, got {type(frame).__name__}")
    n = len(frame)
    if n < 2:
        raise ValueError("LAPDm frame too short (<2 bytes: address + control)")
    buf = (ctypes.c_ubyte * n)(*(bytes(frame)))  # stable C-side view of the input
    fi = _LapdmFrameInfoC()
    rc = int(lib.gsml3_lapdm_frame_decode(buf, c_size_t(n), byref(fi)))
    if rc != OK:
        raise_last_error(lib, rc, "lapdm_frame_decode")
    length = int(fi.info_len) if fi.info is not None else 0
    info_off, data = None, b""
    if length > 0:
        # The C decoder's info pointer is a zero-copy view INTO `buf`; recover its
        # offset as a plain integer (ctypes has no ptr-ptr subtraction): addressof()
        # of our array base, cast-to-c_void_p for the field pointer.
        base = ctypes.addressof(buf)
        off = ctypes.cast(fi.info, ctypes.c_void_p).value - base
        if 0 <= off and off + length <= n:
            data = bytes(buf[off:off + length])
            info_off = off
        else:  # defensive: C view escaped our slice (must not happen per ABI)
            data = ctypes.string_at(fi.info, length)
            info_off = None
    return FrameInfo(format=int(fi.format), u_type=int(fi.u_type), s_type=int(fi.s_type),
                     nr=int(fi.nr), ns=int(fi.ns), pf=int(fi.pf), m_bit=int(fi.m_bit),
                     sapi=int(fi.sapi), command=int(fi.command),
                     info_offset=info_off, info_len=length if fi.info is not None else 0,
                     info=data)


# ── S2 Config ──────────────────────────────────────────────────────────────

class Config:
    """OWNED wrapper over gsml3_config. C defaults: log level WARNING (4),
    lenient framing; pass values to change either. RAII like every owned type."""

    def __init__(self, log_level: int | None = None, strict_framing: bool = False) -> None:
        h = lib.gsml3_config_new()  # NULL on allocation failure (C side)
        if h is None:
            raise_last_error(lib, None, "config_new")
        self._h = h
        self._closed = False
        try:
            if log_level is not None:
                self.set_log_level(log_level)
            if strict_framing:
                self.set_strict_framing(True)
        except Exception:
            self.close()
            raise

    def _check(self) -> None:
        if self._closed or self._h is None:
            raise ClosedGsmL3ObjectError("config is closed")

    def set_log_level(self, level: int) -> None:
        """GSML3_LOG_* value. Out-of-range values are IGNORED by the C side (no
        error is set) — this wrapper intentionally does NOT pre-validate the
        domain and mirrors that documented contract 1:1."""
        self._check()
        if isinstance(level, bool) or not isinstance(level, int):
            raise TypeError(f"log_level must be an int, got {type(level).__name__}")
        lib.gsml3_config_set_log_level(self._h, level)

    def set_strict_framing(self, on: bool) -> None:
        """Strict framing rejects a frame whose message does not consume the
        entire input; lenient (default) tolerates trailing bytes."""
        self._check()
        if not isinstance(on, bool):
            raise TypeError("strict_framing must be a bool")
        lib.gsml3_config_set_strict_framing(self._h, 1 if on else 0)

    def close(self) -> None:
        """Release the config. Idempotent; no FFI once closed."""
        if self._closed:
            return
        self._closed = True
        h, self._h = self._h, None
        lib.gsml3_config_free(h)  # NULL-safe on the C side anyway

    def __del__(self):  # finalizers must never raise
        try:
            self.close()
        except Exception:
            pass

    def __enter__(self) -> "Config":
        return self

    def __exit__(self, *exc) -> None:
        self.close()


# ── S3 Message + S8 typed getters (as methods) ─────────────────────────────

class Message:
    """OWNED wrapper over gsml3_message.

    The handle owns the parsed variant. ``size()`` is the exact wire size
    (zero-alloc) and a buffer of that many bytes is guaranteed to be accepted
    by ``write()``. ``parse_into`` is the zero-extra-allocation reparse hot
    path; on failure the PREVIOUS CONTENT IS KEPT (C semantics of
    gsml3_parse_l3_into) and GsmL3Error is raised.

    S8 typed getters keep C's sentinel contract: int/uint getters pass the
    sentinel through (-1 / 0 when the message is not of the expected type);
    out-parameter variants return None in that case; the five body getters
    (facility / CP-Data / CP-SM-SS / SMS-UD) return b"" both when the IE is
    absent AND when it is genuinely empty — the C layer cannot distinguish
    those two — and raise only on real errors.
    """

    def __init__(self, h) -> None:  # internal: created by from_bytes/from_hex
        self._h = h
        self._closed = False

    # -- constructors -------------------------------------------------------

    @classmethod
    def from_bytes(cls, data, cfg: "Config | None" = None) -> "Message":
        """Parse raw L3 bytes (header + body). None / non-bytes -> TypeError
        and EMPTY input -> TypeError, all BEFORE the FFI boundary (NULL
        policy); C parse failures raise GsmL3Error with the synchronous
        last-error copy (e.g. TRUNCATED for '60', INVALID_PD/MTI variants)."""
        b = _byteslike(data, "data")
        if not b:
            raise TypeError("data must be a non-empty bytes-like object")
        h = lib.gsml3_parse_l3(b, c_size_t(len(b)), _cfg_handle(cfg))
        if h is None:
            raise_last_error(lib, None, "parse_l3")
        return cls(h)

    @classmethod
    def from_hex(cls, hexstr, cfg: "Config | None" = None) -> "Message":
        """Parse a hex string (spaces allowed, e.g. '60 0D 00'). None ->
        TypeError before FFI; '60' raises GsmL3Error(TRUNCATED=2), non-hex
        like 'zz' raises GsmL3Error(INVALID_ARG=1)."""
        if hexstr is None or not isinstance(hexstr, (str, bytes)):
            raise TypeError(f"hexstr must be a str or bytes, got {type(hexstr).__name__}")
        h = lib.gsml3_parse_l3_hex(_enc(hexstr, "hexstr"), _cfg_handle(cfg))
        if h is None:
            raise_last_error(lib, None, "parse_l3_hex")
        return cls(h)

    # -- RAII -----------------------------------------------------------------

    def _check(self) -> None:
        if self._closed or self._h is None:
            raise ClosedGsmL3ObjectError("message handle is closed")

    def close(self) -> None:
        """Release the message. Idempotent; no FFI once closed."""
        if self._closed:
            return
        self._closed = True
        h, self._h = self._h, None
        lib.gsml3_message_free(h)  # NULL-safe on the C side

    def __del__(self):
        try:
            self.close()
        except Exception:
            pass

    def __enter__(self) -> "Message":
        return self

    def __exit__(self, *exc) -> None:
        self.close()

    # -- metadata ---------------------------------------------------------------

    @property
    def name(self) -> str:
        """Message-type name (static C storage — returned as a str copy)."""
        self._check()
        v = lib.gsml3_message_name(self._h)  # never free (static)
        return v.decode("utf-8", "replace") if isinstance(v, (bytes, bytearray)) else ""

    @property
    def pd(self) -> int:
        """Protocol discriminator GSML3_PD_*; -1 for an undefined-PD message."""
        self._check()
        return int(lib.gsml3_message_pd(self._h))

    @property
    def mti(self) -> int:
        """Message-type identifier (0..255; RR short messages 256..511)."""
        self._check()
        return int(lib.gsml3_message_mti(self._h))

    @property
    def ti(self) -> int:
        """Transaction identifier for CC/SS (0..7), 0 otherwise."""
        self._check()
        return int(lib.gsml3_message_ti(self._h))

    # -- serialization -------------------------------------------------------------

    def size(self) -> int:
        """Exact wire size in bytes (zero-alloc). A buffer of this size is
        guaranteed to be accepted by write()."""
        self._check()
        return int(lib.gsml3_message_size(self._h))

    def write(self, out: bytearray | None = None) -> bytes:
        """Serialize into the caller's buffer. ``out=None`` allocates EXACTLY
        size() bytes (decision #8 — no guess-and-grow); a mutable bytearray is
        written in place via a ctypes array view over its storage (C writes
        only for the duration of this one call). A too-small ``out`` raises
        BufferTooSmall (code 11) whose C message names the required size."""
        self._check()
        n = self.size()
        if n == 0:
            raise InternalError(12, "message wire size is zero (internal invariant)")
        if out is None:
            buf = (ctypes.c_ubyte * n)()
        else:
            if not isinstance(out, bytearray):
                raise TypeError("out must be a mutable bytearray or None")
            buf = (ctypes.c_ubyte * len(out)).from_buffer(out)
        rc = int(lib.gsml3_message_write(self._h, buf, c_size_t(len(buf))))
        if rc == 0:
            raise_last_error(lib, None, "message_write")
        if out is None and rc != n:  # the exact-size guarantee must hold
            raise InternalError(12, f"message_write returned {rc} bytes of the exact size {n}")
        return bytes(buf[:rc])

    def hex(self) -> str:
        """Hex serialization (lowercase, no spaces). Library-allocated string
        — copied and released within this same call (copy-then-free)."""
        self._check()
        return _alloc_string(lib.gsml3_message_hex(self._h), "message_hex")

    def dump(self) -> str:
        """Human-readable dump: message name first, then the information-
        element text of every field (all 236 message types). Copy-then-free."""
        self._check()
        return _alloc_string(lib.gsml3_message_dump(self._h), "message_dump")

    def parse_into(self, data, cfg: "Config | None" = None) -> None:
        """Zero-extra-allocation reparse in place (hot path). On failure the
        PREVIOUS CONTENT IS KEPT (mirrors gsml3_parse_l3_into) and GsmL3Error
        is raised; the handle stays open either way."""
        self._check()
        b = _byteslike(data, "data")
        rc = int(lib.gsml3_parse_l3_into(self._h, b, c_size_t(len(b)), _cfg_handle(cfg)))
        if rc != OK:
            raise_last_error(lib, rc, "parse_into")

    # -- S8 typed getters ── RR ───────────────────────────────────────────────────

    def channel_release_cause(self) -> int:
        """RRCause; -1 sentinel when the message is not ChannelRelease."""
        self._check()
        return int(lib.gsml3_msg_channel_release_cause(self._h))

    def channel_release_gprs_resumption(self) -> int:
        """1/0 when the GPRS-resumption bit is present, -1 otherwise."""
        self._check()
        return int(lib.gsml3_msg_channel_release_gprs_resumption(self._h))

    def channel_request_ra(self) -> int:
        """8-bit request reference (RA); -1 sentinel on wrong type."""
        self._check()
        return int(lib.gsml3_msg_channel_request_ra(self._h))

    def immediate_assignment_channel(self) -> Channel | None:
        """Channel description; None when not an ImmediateAssignment."""
        self._check()
        ch = _ChannelC()
        if lib.gsml3_msg_immediate_assignment_channel(self._h, byref(ch)) != 0:
            return None
        return Channel(int(ch.type_and_offset), int(ch.tn), int(ch.tsc), int(ch.arfcn))

    def immediate_assignment_ta(self) -> int:
        """Timing advance (0..63); -1 sentinel on wrong type."""
        self._check()
        return int(lib.gsml3_msg_immediate_assignment_ta(self._h))

    def immediate_assignment_reject_wait_time(self) -> int:
        """MS retry wait in seconds; -1 sentinel when absent / wrong type."""
        self._check()
        return int(lib.gsml3_msg_immediate_assignment_reject_wait_time(self._h))

    def assignment_command_channel(self) -> Channel | None:
        self._check()
        ch = _ChannelC()
        if lib.gsml3_msg_assignment_command_channel(self._h, byref(ch)) != 0:
            return None
        return Channel(int(ch.type_and_offset), int(ch.tn), int(ch.tsc), int(ch.arfcn))

    def assignment_complete_cause(self) -> int:
        self._check()
        return int(lib.gsml3_msg_assignment_complete_cause(self._h))

    def assignment_failure_cause(self) -> int:
        self._check()
        return int(lib.gsml3_msg_assignment_failure_cause(self._h))

    def paging_request_type1_count(self) -> int:
        """Number of paged mobiles (1..2); -1 sentinel on wrong type."""
        self._check()
        return int(lib.gsml3_msg_paging_request_type1_count(self._h))

    def paging_request_type1_identity(self, index: int) -> MobileIdentity | None:
        """Paged identity at ``index`` (0/1); None on out-of-range/wrong type."""
        self._check()
        _as_int(index, "index", lo=0, hi=1)
        idc = _MobileIdentityC()
        if lib.gsml3_msg_paging_request_type1_identity(self._h, index, byref(idc)) != 0:
            return None
        return _identity_from(idc)

    def paging_request_type2_tmsi(self, index: int) -> int:
        """One of the 2 paged TMSIs; 0 on out-of-range / wrong type."""
        self._check()
        _as_int(index, "index", lo=0, hi=1)
        return int(lib.gsml3_msg_paging_request_type2_tmsi(self._h, index))

    def paging_request_type3_tmsi(self, index: int) -> int:
        """One of the 4 paged TMSIs; 0 on out-of-range / wrong type."""
        self._check()
        _as_int(index, "index", lo=0, hi=3)
        return int(lib.gsml3_msg_paging_request_type3_tmsi(self._h, index))

    def paging_response_cks(self) -> int:
        """CKSN bit; -1 sentinel on wrong type."""
        self._check()
        return int(lib.gsml3_msg_paging_response_cks(self._h))

    def paging_response_identity(self) -> MobileIdentity | None:
        self._check()
        idc = _MobileIdentityC()
        if lib.gsml3_msg_paging_response_identity(self._h, byref(idc)) != 0:
            return None
        return _identity_from(idc)

    def ciphering_mode_command_ciphering(self) -> int:
        """Ciphering on/off (1/0); -1 sentinel on wrong type."""
        self._check()
        return int(lib.gsml3_msg_ciphering_mode_command_ciphering(self._h))

    def ciphering_mode_command_algorithm(self) -> int:
        """Ciphering algorithm identifier; -1 sentinel on wrong type."""
        self._check()
        return int(lib.gsml3_msg_ciphering_mode_command_algorithm(self._h))

    def ciphering_mode_complete_response(self) -> int:
        self._check()
        return int(lib.gsml3_msg_ciphering_mode_complete_response(self._h))

    def ciphering_mode_complete_has_imeisv(self) -> int:
        self._check()
        return int(lib.gsml3_msg_ciphering_mode_complete_has_imeisv(self._h))

    def handover_complete_cause(self) -> int:
        self._check()
        return int(lib.gsml3_msg_handover_complete_cause(self._h))

    def handover_command_cell(self) -> tuple[int, int, int] | None:
        """Target cell as (arfcn, ncc, bcc); None on wrong type."""
        self._check()
        arfcn = ctypes.c_uint16(0)
        ncc = ctypes.c_uint8(0)
        bcc = ctypes.c_uint8(0)
        if lib.gsml3_msg_handover_command_cell(self._h, byref(arfcn), byref(ncc), byref(bcc)) != 0:
            return None
        return (int(arfcn.value), int(ncc.value), int(bcc.value))

    def physical_information_ta(self) -> int:
        """Timing advance (0..63); -1 sentinel on wrong type."""
        self._check()
        return int(lib.gsml3_msg_physical_information_ta(self._h))

    # -- S8 typed getters ── MM ───────────────────────────────────────────────────

    def cm_service_request_service_type(self) -> int:
        """L3CMServiceType::TypeCode value; -1 sentinel on wrong type.
        MobileOriginatedCall = 1 (fits the 4-bit wire field and starts the MO
        chain); LocationUpdateRequest = 105 does NOT fit and starts no chain."""
        self._check()
        return int(lib.gsml3_msg_cm_service_request_service_type(self._h))

    def cm_service_request_identity(self) -> MobileIdentity | None:
        self._check()
        idc = _MobileIdentityC()
        if lib.gsml3_msg_cm_service_request_identity(self._h, byref(idc)) != 0:
            return None
        return _identity_from(idc)

    def cm_service_reject_cause(self) -> int:
        """MMRejectCause; -1 sentinel on wrong type."""
        self._check()
        return int(lib.gsml3_msg_cm_service_reject_cause(self._h))

    def cm_service_abort_cause(self) -> int:
        """CMServiceAbortCause; -1 sentinel on wrong type."""
        self._check()
        return int(lib.gsml3_msg_cm_service_abort_cause(self._h))

    def identity_request_type(self) -> int:
        """MobileIDType (GSML3_ID_*); -1 sentinel on wrong type."""
        self._check()
        return int(lib.gsml3_msg_identity_request_type(self._h))

    def identity_response_identity(self) -> MobileIdentity | None:
        self._check()
        idc = _MobileIdentityC()
        if lib.gsml3_msg_identity_response_identity(self._h, byref(idc)) != 0:
            return None
        return _identity_from(idc)

    def location_updating_request_update_type(self) -> int:
        """0=Normal, 1=Periodic, 2=IMSI Attach; -1 sentinel on wrong type."""
        self._check()
        return int(lib.gsml3_msg_location_updating_request_update_type(self._h))

    def location_updating_request_identity(self) -> MobileIdentity | None:
        self._check()
        idc = _MobileIdentityC()
        if lib.gsml3_msg_location_updating_request_identity(self._h, byref(idc)) != 0:
            return None
        return _identity_from(idc)

    def location_updating_request_lai(self) -> Lai | None:
        self._check()
        lai = _LaiC()
        if lib.gsml3_msg_location_updating_request_lai(self._h, byref(lai)) != 0:
            return None
        return _lai_from(lai)

    def location_updating_accept_lai(self) -> Lai | None:
        self._check()
        lai = _LaiC()
        if lib.gsml3_msg_location_updating_accept_lai(self._h, byref(lai)) != 0:
            return None
        return _lai_from(lai)

    def location_updating_accept_identity(self) -> MobileIdentity | None:
        self._check()
        idc = _MobileIdentityC()
        if lib.gsml3_msg_location_updating_accept_identity(self._h, byref(idc)) != 0:
            return None
        return _identity_from(idc)

    def location_updating_reject_cause(self) -> int:
        self._check()
        return int(lib.gsml3_msg_location_updating_reject_cause(self._h))

    def authentication_request_cks(self) -> int:
        """CKSN bit; -1 sentinel on wrong type."""
        self._check()
        return int(lib.gsml3_msg_authentication_request_cks(self._h))

    def authentication_request_rand(self) -> bytes | None:
        """The 16-octet RAND in wire order, or None when the message is not
        AuthenticationRequest. (Verified against the C implementation: it
        returns 0 on success and -1 on wrong type — the header comment states
        the inverse; this wrapper follows the behavior.)"""
        self._check()
        buf = (ctypes.c_ubyte * 16)()
        rc = int(lib.gsml3_msg_authentication_request_rand(self._h, buf))
        if rc != 0:
            if int(lib.gsml3_last_error_code()) != OK:  # unexpected failure, not the sentinel
                raise_last_error(lib, None, "msg_authentication_request_rand")
            return None
        return bytes(buf.raw[:16])

    def authentication_response_sres(self) -> int:
        """32-bit SRES; 0 when not AuthenticationResponse."""
        self._check()
        return int(lib.gsml3_msg_authentication_response_sres(self._h))

    def tmsi_reallocation_command_lai(self) -> Lai | None:
        self._check()
        lai = _LaiC()
        if lib.gsml3_msg_tmsi_reallocation_command_lai(self._h, byref(lai)) != 0:
            return None
        return _lai_from(lai)

    def tmsi_reallocation_command_tmsi(self) -> int:
        """Reallocated TMSI; 0 sentinel on wrong type."""
        self._check()
        return int(lib.gsml3_msg_tmsi_reallocation_command_tmsi(self._h))

    def imsi_detach_indication_identity(self) -> MobileIdentity | None:
        self._check()
        idc = _MobileIdentityC()
        if lib.gsml3_msg_imsi_detach_indication_identity(self._h, byref(idc)) != 0:
            return None
        return _identity_from(idc)

    # -- S8 typed getters ── CC / SMS / SS ──────────────────────────────────────────

    def setup_ti(self) -> int:
        self._check()
        return int(lib.gsml3_msg_setup_ti(self._h))

    def setup_have_called_party(self) -> int:
        """1/0 whether the called-party number IE is present."""
        self._check()
        return int(lib.gsml3_msg_setup_have_called_party(self._h))

    def setup_called_number(self) -> str | None:
        """Called-party BCD digit string, COPIED from the handle-internal
        view (the C pointer lives in the message — never free it); None when
        the IE is absent."""
        self._check()
        v = lib.gsml3_msg_setup_called_number(self._h)  # static-ish view: NOT freed by us
        return None if v is None else v.decode("ascii", "replace")

    def call_proceeding_ti(self) -> int:
        self._check()
        return int(lib.gsml3_msg_call_proceeding_ti(self._h))

    def alerting_ti(self) -> int:
        self._check()
        return int(lib.gsml3_msg_alerting_ti(self._h))

    def connect_ti(self) -> int:
        self._check()
        return int(lib.gsml3_msg_connect_ti(self._h))

    def connect_acknowledge_ti(self) -> int:
        self._check()
        return int(lib.gsml3_msg_connect_acknowledge_ti(self._h))

    def disconnect_ti(self) -> int:
        self._check()
        return int(lib.gsml3_msg_disconnect_ti(self._h))

    def disconnect_cause(self) -> int:
        """CCCause; -1 sentinel on wrong type."""
        self._check()
        return int(lib.gsml3_msg_disconnect_cause(self._h))

    def release_ti(self) -> int:
        self._check()
        return int(lib.gsml3_msg_release_ti(self._h))

    def release_have_cause(self) -> int:
        self._check()
        return int(lib.gsml3_msg_release_have_cause(self._h))

    def release_cause(self) -> int:
        """CCCause; -1 sentinel when absent / wrong type."""
        self._check()
        return int(lib.gsml3_msg_release_cause(self._h))

    def release_complete_ti(self) -> int:
        self._check()
        return int(lib.gsml3_msg_release_complete_ti(self._h))

    def release_complete_have_cause(self) -> int:
        self._check()
        return int(lib.gsml3_msg_release_complete_have_cause(self._h))

    def release_complete_cause(self) -> int:
        self._check()
        return int(lib.gsml3_msg_release_complete_cause(self._h))

    def facility_ti(self) -> int:
        self._check()
        return int(lib.gsml3_msg_facility_ti(self._h))

    def cp_status_tp_oi(self) -> int:
        self._check()
        return int(lib.gsml3_msg_cp_status_tp_oi(self._h))

    def cp_status_mti_value(self) -> int:
        self._check()
        return int(lib.gsml3_msg_cp_status_mti_value(self._h))

    def cp_status_has_message_ref(self) -> int:
        self._check()
        return int(lib.gsml3_msg_cp_status_has_message_ref(self._h))

    def cp_status_message_ref(self) -> int:
        self._check()
        return int(lib.gsml3_msg_cp_status_message_ref(self._h))

    def sms_deliver_tp_mti(self) -> int:
        self._check()
        return int(lib.gsml3_msg_sms_deliver_tp_mti(self._h))

    def sms_deliver_tp_mr(self) -> int:
        self._check()
        return int(lib.gsml3_msg_sms_deliver_tp_mr(self._h))

    def sms_deliver_has_tp_ud(self) -> int:
        self._check()
        return int(lib.gsml3_msg_sms_deliver_has_tp_ud(self._h))

    # *S8 body getters (SZ-returning): one shared implementation. C cannot
    # distinguish "IE absent" from "IE present but empty" (both 0, no error) —
    # both surface as b""; real errors (buffer too small, unexpected) raise.*

    def facility_body(self) -> bytes:
        """Facility IE body (512-octet caller buffer, decision #8); b\"\" when
        absent or genuinely empty."""
        return _message_body_get(self, "gsml3_msg_facility_body")

    def cp_data_rpdu(self) -> bytes:
        """CP-Data RPDU body; b\"\" when absent or empty."""
        return _message_body_get(self, "gsml3_msg_cp_data_rpdu")

    def cp_smt_rpdu(self) -> bytes:
        """CP-SM-Send RPDU body; b\"\" when absent or empty."""
        return _message_body_get(self, "gsml3_msg_cp_smt_rpdu")

    def sms_deliver_tp_ud(self) -> bytes:
        """SMS-DELIVER TP-UD (user data); b\"\" when absent or empty."""
        return _message_body_get(self, "gsml3_msg_sms_deliver_tp_ud")

    def sup_serv_facility_data(self) -> bytes:
        """Supplementary-service facility body; the SS vector of the test
        table is exactly the empty-body case (b\"\" here)."""
        return _message_body_get(self, "gsml3_msg_sup_serv_facility_data")


def _message_body_get(self, op_name: str) -> bytes:
    """Shared implementation of the S8 body getters (see their docs for the
    absent/empty semantics and the decision #8 buffer rule)."""
    buf = (ctypes.c_ubyte * _BUILDER_BUF)()
    rc = int(getattr(lib, op_name)(self._h, buf, c_size_t(len(buf))))
    if rc == 0:
        if int(lib.gsml3_last_error_code()) != OK:
            raise_last_error(lib, None, op_name)
        return b""
    return bytes(buf[:rc])


# ── S4 RSL / A-bis (frame handle + the 13 builders) ─────────────────────────

class RslFrame:
    """OWNED wrapper over gsml3_rsl. The handle OWNS A COPY of the parsed
    input: IE/L3 views reference that copy, so the caller's buffer may be
    freed (discarded) immediately after parse(). This wrapper still hands out
    Python bytes COPIES and never escapes a raw pointer."""

    def __init__(self, h) -> None:
        self._h = h
        self._closed = False

    @classmethod
    def parse(cls, data) -> "RslFrame":
        """Parse an A-bis RSL frame; None/empty input -> TypeError (before FFI)."""
        b = _byteslike(data, "data")
        if not b:
            raise TypeError("data must be a non-empty bytes-like object")
        h = lib.gsml3_rsl_parse(b, c_size_t(len(b)))
        if h is None:
            raise_last_error(lib, None, "rsl_parse")
        return cls(h)

    def _check(self) -> None:
        if self._closed or self._h is None:
            raise ClosedGsmL3ObjectError("RSL frame handle is closed")

    def close(self) -> None:
        """Release the RSL handle and its input copy. Idempotent."""
        if self._closed:
            return
        self._closed = True
        h, self._h = self._h, None
        lib.gsml3_rsl_free(h)  # NULL-safe on the C side

    def __del__(self):
        try:
            self.close()
        except Exception:
            pass

    def __enter__(self) -> "RslFrame":
        return self

    def __exit__(self, *exc) -> None:
        self.close()

    @property
    def name(self) -> str:
        """'DATA_REQ', 'CHAN_ACTIV', ... (static C storage)."""
        self._check()
        v = lib.gsml3_rsl_name(self._h)
        return v.decode("utf-8", "replace") if isinstance(v, (bytes, bytearray)) else ""

    @property
    def discriminator(self) -> int:
        """7-bit discriminator (direction bit stripped)."""
        self._check()
        return int(lib.gsml3_rsl_discriminator(self._h))

    @property
    def msg_type(self) -> int:
        self._check()
        return int(lib.gsml3_rsl_msg_type(self._h))

    @property
    def chan_nr(self) -> int:
        self._check()
        return int(lib.gsml3_rsl_chan_nr(self._h))

    @property
    def link_id(self) -> int:
        """LAPDm link identifier (RLL)."""
        self._check()
        return int(lib.gsml3_rsl_link_id(self._h))

    @property
    def bts_to_bsc(self) -> int:
        """Direction: 1 = BTS->BSC, 0 = BSC->BTS."""
        self._check()
        return int(lib.gsml3_rsl_bts_to_bsc(self._h))

    @property
    def has_l3(self) -> bool:
        self._check()
        return int(lib.gsml3_rsl_has_l3(self._h)) == 1

    def l3(self) -> bytes | None:
        """L3 payload as a COPY of the handle-internal view (None when the
        frame carries no L3). Valid while this RslFrame stays open."""
        self._check()
        ln = c_size_t(0)
        p = lib.gsml3_rsl_l3(self._h, byref(ln))  # raw-pointer result (token P): int or None
        if p is None:
            return None
        n = int(ln.value)
        if n == 0:
            return b""
        return bytes(ctypes.string_at(ctypes.c_void_p(int(p)), n))  # COPY of the view — the C pointer never escapes

    @property
    def ie_count(self) -> int:
        """Number of parsed information elements."""
        self._check()
        return int(lib.gsml3_rsl_ie_count(self._h))

    def ie(self, index: int) -> tuple[int, int, bytes]:
        """IE at ``index`` as (ie_type_code, value_len, value_COPY); raises
        GsmL3Error(INVALID_ARG) for an out-of-range index (C contract)."""
        self._check()
        _as_int(index, "index", lo=0)
        t = ctypes.c_uint8(0)
        ln = c_size_t(0)
        # const uint8_t** out: a null LP_c_ubyte slot passed byref — C fills it
        val = ctypes.POINTER(ctypes.c_ubyte)()
        rc = int(lib.gsml3_rsl_ie_get(self._h, index, byref(t), byref(ln), byref(val)))
        if rc != OK:
            raise_last_error(lib, rc, "rsl_ie_get")
        n = int(ln.value)
        # COPY the handle-internal view; the raw C pointer never escapes this call.
        data = bytes(ctypes.string_at(val, n)) if n > 0 else b""
        return int(t.value), n, data


def _builder(op_name: str, args, label: str | None = None) -> bytes:
    """Shared S4/S7/S9 stateless-builder pattern (decision #8): ONE fixed
    512-octet caller buffer (a ctypes c_ubyte array — the PB argtype requires
    an array, and C writes into it only for the duration of this call),
    zero-alloc on the C side. rc == 0 -> GsmL3Error with the synchronous
    last-error copy (BUFFER_TOO_SMALL = 11 is reported, never silently retried
    with a grown buffer)."""
    buf = (ctypes.c_ubyte * _BUILDER_BUF)()
    fn = getattr(lib, op_name)
    rc = int(fn(buf, c_size_t(len(buf)), *args))
    if rc == 0:
        raise_last_error(lib, None, label or op_name.replace("gsml3_", "", 1))
    return bytes(buf[:rc])


# The 13 RSL builders — argument order/validation exactly as gsml3parser_c.h.

def rsl_build_data_req(chan_nr: int, link_id: int, l3) -> bytes:
    """RSL DATA_REQ (BTS->BSC) frame."""
    b = _byteslike(l3, "l3")
    return _builder("gsml3_rsl_build_data_req",
                    (_as_u8(chan_nr, "chan_nr"), _as_u8(link_id, "link_id"), b, len(b)))


def rsl_build_data_ind(chan_nr: int, link_id: int, l3) -> bytes:
    """RSL DATA_IND (BSC->BTS) frame."""
    b = _byteslike(l3, "l3")
    return _builder("gsml3_rsl_build_data_ind",
                    (_as_u8(chan_nr, "chan_nr"), _as_u8(link_id, "link_id"), b, len(b)))


def rsl_build_unit_data_req(chan_nr: int, link_id: int, l3) -> bytes:
    """RSL UNIT_DATA_REQ (signalling on a signalling link)."""
    b = _byteslike(l3, "l3")
    return _builder("gsml3_rsl_build_unit_data_req",
                    (_as_u8(chan_nr, "chan_nr"), _as_u8(link_id, "link_id"), b, len(b)))


def rsl_build_unit_data_ind(chan_nr: int, link_id: int, l3) -> bytes:
    """RSL UNIT_DATA_IND."""
    b = _byteslike(l3, "l3")
    return _builder("gsml3_rsl_build_unit_data_ind",
                    (_as_u8(chan_nr, "chan_nr"), _as_u8(link_id, "link_id"), b, len(b)))


def rsl_build_chan_activ_ack(chan_nr: int, frame_number: int) -> bytes:
    return _builder("gsml3_rsl_build_chan_activ_ack",
                    (_as_u8(chan_nr, "chan_nr"), _as_u16(frame_number, "frame_number")))


def rsl_build_chan_activ_nack(chan_nr: int, cause: int) -> bytes:
    """RSLErrorCause is range-checked in C (out-of-domain -> INVALID_ARG)."""
    return _builder("gsml3_rsl_build_chan_activ_nack", (_as_u8(chan_nr, "chan_nr"), cause))


def rsl_build_rf_chan_rel_ack(chan_nr: int) -> bytes:
    return _builder("gsml3_rsl_build_rf_chan_rel_ack", (_as_u8(chan_nr, "chan_nr"),))


def rsl_build_conn_fail(chan_nr: int, cause: int) -> bytes:
    """RSLErrorCause range-checked in C."""
    return _builder("gsml3_rsl_build_conn_fail", (_as_u8(chan_nr, "chan_nr"), cause))


def rsl_build_meas_res(chan_nr: int, meas_nr: int, rxlev: int, rxqual: int, l1) -> bytes:
    """rxlev/rxqual are signed 8-bit measurements."""
    b = _byteslike(l1, "l1")
    return _builder("gsml3_rsl_build_meas_res",
                    (_as_u8(chan_nr, "chan_nr"), _as_u8(meas_nr, "meas_nr"),
                     _as_i8(rxlev, "rxlev"), _as_i8(rxqual, "rxqual"), b, len(b)))


def rsl_build_hando_det(chan_nr: int, access_delay: int) -> bytes:
    return _builder("gsml3_rsl_build_hando_det",
                    (_as_u8(chan_nr, "chan_nr"), _as_u8(access_delay, "access_delay")))


def rsl_build_ccch_load_ind(chan_nr: int, paging_load: int, rach_total: int,
                            rach_busy: int, rach_access: int) -> bytes:
    return _builder("gsml3_rsl_build_ccch_load_ind",
                    (_as_u8(chan_nr, "chan_nr"), _as_u16(paging_load, "paging_load"),
                     _as_u16(rach_total, "rach_total"), _as_u16(rach_busy, "rach_busy"),
                     _as_u16(rach_access, "rach_access")))


def rsl_build_chan_rqd(chan_nr: int, ra: int, t1p: int, t2: int, t3: int,
                       access_delay: int) -> bytes:
    return _builder("gsml3_rsl_build_chan_rqd",
                    (_as_u8(chan_nr, "chan_nr"), _as_u8(ra, "ra"), _as_u8(t1p, "t1p"),
                     _as_u8(t2, "t2"), _as_u8(t3, "t3"), _as_u8(access_delay, "access_delay")))


def rsl_build_delete_ind(chan_nr: int, info) -> bytes:
    b = _byteslike(info, "info")
    return _builder("gsml3_rsl_build_delete_ind", (_as_u8(chan_nr, "chan_nr"), b, len(b)))


# ── S6 Registry + borrowed Session ─────────────────────────────────────────

class Registry:
    """OWNED wrapper over gsml3_registry. shard_count: 0 = plain
    single-threaded registry; 4/8/16/32 = sharded thread-safe registry
    (per-shard locks in C); any other value is rejected HERE (ValueError)
    before the FFI boundary and re-checked in C as backstop. ``reserve`` is a
    cold-path pre-size. Ticking buffers are allocated per call and never
    pre-zeroed — the C contract fully initializes every written event."""

    _ALLOWED_SHARDS = (0, 4, 8, 16, 32)

    def __init__(self, shard_count: int = 0) -> None:
        _as_int(shard_count, "shard_count")
        if shard_count not in self._ALLOWED_SHARDS:
            raise ValueError(
                f"shard_count must be one of {self._ALLOWED_SHARDS}, got {shard_count}")
        h = lib.gsml3_registry_new(shard_count)  # C re-validates (backstop)
        if h is None:
            raise_last_error(lib, None, "registry_new")
        self._h = h
        self.shard_count = shard_count
        self._closed = False

    @property
    def closed(self) -> bool:
        return self._closed or self._h is None

    def _check(self) -> None:
        if self.closed:
            raise ClosedGsmL3ObjectError("registry is closed")

    def close(self) -> None:
        """Release the registry AND all its sessions (they are borrowed —
        there is no per-session free). Idempotent."""
        if self.closed:
            return
        self._closed = True
        h, self._h = self._h, None
        lib.gsml3_registry_free(h)  # NULL-safe on the C side

    def __del__(self):
        try:
            self.close()
        except Exception:
            pass

    def __enter__(self) -> "Registry":
        return self

    def __exit__(self, *exc) -> None:
        self.close()

    # -- population ------------------------------------------------------------

    @property
    def count(self) -> int:
        """Number of active sessions."""
        self._check()
        return int(lib.gsml3_registry_count(self._h))

    def reserve(self, expected: int) -> None:
        """Cold-path pre-size of the internal indexes for ~`expected` sessions."""
        self._check()
        _as_int(expected, "expected", lo=0)
        lib.gsml3_registry_reserve(self._h, c_size_t(expected))

    def create_by_tmsi(self, tmsi: int) -> Session:
        """Create the session keyed by TMSI. The reserved ALL-ZERO TMSI is
        rejected BY THE C SIDE (GSML3_ERR_INVALID_ARG, TS 24.008) — passed
        through as a GsmL3Error, not pre-rejected; duplicates raise
        GsmL3Error(DUPLICATE); allocation failure raises GsmL3NoMemory."""
        self._check()
        _as_int(tmsi, "tmsi", lo=0, hi=0xFFFFFFFF)
        s = lib.gsml3_registry_create_by_tmsi(self._h, tmsi)
        if s is None:
            raise_last_error(lib, None, "registry_create_by_tmsi")
        return Session(int(s), self)

    def create_by_imsi(self, digits: str) -> Session:
        """Create the session keyed by IMSI (1-15 ASCII digits) under an
        auto-assigned TMSI (see Session.assigned_tmsi). Sharded registries do
        not support this — C raises GsmL3Error(UNSUPPORTED), passed through."""
        self._check()
        d = _enc(digits, "imsi digits")
        s = lib.gsml3_registry_create_by_imsi(self._h, d)
        if s is None:
            raise_last_error(lib, None, "registry_create_by_imsi")
        return Session(int(s), self)

    def find_by_tmsi(self, tmsi: int):
        """Lookup; returns None when NOT FOUND (that is not an error)."""
        self._check()
        _as_int(tmsi, "tmsi", lo=0, hi=0xFFFFFFFF)
        s = lib.gsml3_registry_find_by_tmsi(self._h, tmsi)
        return None if s is None else Session(int(s), self)

    def find_by_imsi(self, digits: str):
        self._check()
        d = _enc(digits, "imsi digits")
        s = lib.gsml3_registry_find_by_imsi(self._h, d)
        return None if s is None else Session(int(s), self)

    def find_by_link(self, trx: int, ts: int, lapdm_link: int):
        """Lookup by assigned channel; the link index key is (trx, ts,
        lapdm_link) — ARFCN is stored but not part of the key."""
        self._check()
        return self._wrap_find(
            lib.gsml3_registry_find_by_link(self._h,
                                            _as_u8(trx, "trx"), _as_u8(ts, "ts"),
                                            _as_u8(lapdm_link, "lapdm_link")))

    def _wrap_find(self, s):
        return None if s is None else Session(int(s), self)

    def remove(self, sess: Session) -> bool:
        """1/True when removed. A session owned by another registry is a C-side
        no-op with INVALID_ARG — surfaced here as GsmL3Error."""
        self._check()
        _check_session(sess, "sess")
        rc = int(lib.gsml3_registry_remove(self._h, sess._h))
        if rc not in (0, 1):
            raise_last_error(lib, None, "registry_remove")
        return rc == 1

    def clear(self) -> None:
        """Remove all sessions. Sharded registries: no-op + GsmL3Error(UNSUPPORTED)."""
        self._check()
        lib.gsml3_registry_clear(self._h)
        code = int(lib.gsml3_last_error_code())  # void call: read the fresh error state now
        if code != OK:
            raise_last_error(lib, code, "registry_clear")

    def assign_channel(self, sess: Session, ch_type: int, trx: int, ts: int,
                       arfcn: int, lapdm_link: int) -> None:
        """Assign a channel (gsml3parser::ChannelType value, range-checked in
        C) and update the link index. A session NOT owned by this registry is
        a C-side no-op + INVALID_ARG — surfaced as GsmL3Error."""
        self._check()
        _check_session(sess, "sess")
        lib.gsml3_registry_assign_channel(
            self._h, sess._h, ch_type, _as_u8(trx, "trx"), _as_u8(ts, "ts"),
            _as_int(arfcn, "arfcn", lo=0, hi=1023), _as_u8(lapdm_link, "lapdm_link"))
        code = int(lib.gsml3_last_error_code())
        if code != OK:
            raise_last_error(lib, code, "registry_assign_channel")

    def release_channel(self, sess: Session) -> None:
        self._check()
        _check_session(sess, "sess")
        lib.gsml3_registry_release_channel(self._h, sess._h)
        code = int(lib.gsml3_last_error_code())
        if code != OK:
            raise_last_error(lib, code, "registry_release_channel")

    # -- ticks ---------------------------------------------------------------------

    def tick_timers(self, delta_ms: int, cap: int = 16):
        """Advance all session timers by ``delta_ms`` milliseconds and return
        the expired ones as [(Session, timer_id), ...]. The output buffer is a
        fresh ctypes array of `cap` slots per call (cold path — the hot
        registry loop stays allocation-free on the C side). Events that do not
        fit in `cap` are re-armed at 1 ms by C and arrive on the NEXT tick:
        never dropped. Returned sessions are BORROWED views of THIS registry."""
        self._check()
        _as_int(delta_ms, "delta_ms", lo=0, hi=0xFFFFFFFF)
        cap = _as_int(cap, "cap", lo=1, hi=4096)
        buf = (_TimerExpiryC * cap)()  # no zero-init needed per the C contract
        n = int(lib.gsml3_registry_tick_timers(self._h, delta_ms, buf,
                                               c_size_t(cap)))
        return [(Session(int(buf[i].session), self), int(buf[i].timer_id))
                for i in range(n)]

    def tick_procedures(self, delta_ms: int) -> int:
        """Advance registry-level session procedures; returns how many timed out."""
        self._check()
        _as_int(delta_ms, "delta_ms", lo=0, hi=0xFFFFFFFF)
        return int(lib.gsml3_registry_tick_procedures(self._h, delta_ms))


class Session:
    """BORROWED wrapper over gsml3_session.

    Owned by its Registry: NO free and NO close exist here on purpose (ABI:
    'never free it; valid until removed or registry freed'). Every accessor
    checks the owning registry's closed flag first and raises
    ClosedGsmL3ObjectError instead of touching a dead pointer. Keep one thread
    per session, and never use it concurrently with its registry remove().
    """

    __slots__ = ("_h", "_registry")

    def __init__(self, h: int, registry: Registry) -> None:  # internal constructor
        self._h = h
        self._registry = registry

    def _check(self) -> None:
        if self._h is None or self._registry.closed:
            raise ClosedGsmL3ObjectError(
                "session is borrowed from a closed/invalid registry (do not keep it past the registry)")

    @property
    def tmsi(self) -> int:
        """Identity TMSI — 0 when the session identity is an IMSI."""
        self._check()
        return int(lib.gsml3_session_tmsi(self._h))

    @property
    def assigned_tmsi(self) -> int:
        """Key TMSI in the owning registry; for IMSI-created sessions this is
        the auto-assigned TMSI (0 when not registry-owned)."""
        self._check()
        return int(lib.gsml3_session_assigned_tmsi(self._h))

    def set_tmsi(self, tmsi: int) -> None:
        """Change the identity TMSI only. NOTE: this does NOT update the
        registry's TMSI index (remove + create to re-index)."""
        self._check()
        _as_int(tmsi, "tmsi", lo=0, hi=0xFFFFFFFF)
        lib.gsml3_session_set_tmsi(self._h, tmsi)
        code = int(lib.gsml3_last_error_code())
        if code != OK:
            raise_last_error(lib, code, "session_set_tmsi")

    def set_imsi(self, digits) -> None:
        """Set the IMSI (BCD digit string)."""
        self._check()
        d = _enc(digits, "imsi digits")
        lib.gsml3_session_set_imsi(self._h, d)
        code = int(lib.gsml3_last_error_code())
        if code != OK:
            raise_last_error(lib, code, "session_set_imsi")

    @property
    def registered(self) -> bool:
        self._check()
        return int(lib.gsml3_session_is_registered(self._h)) == 1

    def set_registered(self, v: bool) -> None:
        self._check()
        if not isinstance(v, bool):
            raise TypeError("registered must be a bool")
        lib.gsml3_session_set_registered(self._h, 1 if v else 0)

    @property
    def authenticated(self) -> bool:
        self._check()
        return int(lib.gsml3_session_is_authenticated(self._h)) == 1

    def set_authenticated(self, v: bool) -> None:
        self._check()
        if not isinstance(v, bool):
            raise TypeError("authenticated must be a bool")
        lib.gsml3_session_set_authenticated(self._h, 1 if v else 0)

    @property
    def ciphered(self) -> bool:
        self._check()
        return int(lib.gsml3_session_is_ciphered(self._h)) == 1

    def set_ciphered(self, v: bool) -> None:
        self._check()
        if not isinstance(v, bool):
            raise TypeError("ciphered must be a bool")
        lib.gsml3_session_set_ciphered(self._h, 1 if v else 0)

    def timer_start(self, timer_id: int) -> int:
        """GSML3_TIMER_* id. Returns 1 on a fresh start, 0 on a restart; an
        out-of-range id sets the thread-local error and is raised as GsmL3Error."""
        self._check()
        _as_int(timer_id, "timer_id", lo=0)
        rc = int(lib.gsml3_session_timer_start(self._h, timer_id))
        if rc < 0:
            raise_last_error(lib, None, "session_timer_start")
        if rc == 0 and int(lib.gsml3_last_error_code()) != OK:
            raise_last_error(lib, None, "session_timer_start")
        return rc

    def timer_stop(self, timer_id: int) -> None:
        self._check()
        _as_int(timer_id, "timer_id", lo=0)
        lib.gsml3_session_timer_stop(self._h, timer_id)

    def timer_running(self, timer_id: int) -> bool:
        self._check()
        _as_int(timer_id, "timer_id", lo=0)
        return int(lib.gsml3_session_timer_running(self._h, timer_id)) == 1

    @property
    def transaction_pending(self) -> int:
        """Number of pending transactions."""
        self._check()
        return int(lib.gsml3_session_transaction_pending(self._h))


def _check_session(s, what="session") -> None:
    if not isinstance(s, Session):
        raise TypeError(f"{what} must be a Session, got {type(s).__name__}")
    s._check()


def _session_handle(session, what="session") -> int | None:
    """Session | None -> the raw borrowed handle for C (None passes NULL, which
    the feed* ABI allows); closed sessions raise before the FFI boundary."""
    if session is None:
        return None
    _check_session(session, what)
    return session._h


# ── S7 Orchestrator (+ StepResult) and standalone response builders ────────

class Orchestrator:
    """OWNED wrapper over gsml3_orchestrator — one chain per orchestrator; it
    owns the active procedure. Every feed* returns the C step BY VALUE; its
    ``reason`` points at THREAD-LOCAL library storage that is invalidated by
    the next successful gsml3_* call, so the wrapper copies it into a str
    synchronously, inside the same Python statement flow. A step whose
    ``code != OK`` raises GsmL3Error — per the C contract its remaining
    fields carry no step information then. A None session is legal for all
    feed* (steps without a context)."""

    def __init__(self) -> None:
        h = lib.gsml3_orchestrator_new()
        if h is None:
            raise_last_error(lib, None, "orchestrator_new")
        self._h = h
        self._closed = False

    @property
    def closed(self) -> bool:
        return self._closed or self._h is None

    def _check(self) -> None:
        if self.closed:
            raise ClosedGsmL3ObjectError("orchestrator is closed")

    def close(self) -> None:
        """Release the orchestrator (and its active procedure). Idempotent;
        no FFI once closed."""
        if self.closed:
            return
        self._closed = True
        h, self._h = self._h, None
        lib.gsml3_orchestrator_free(h)  # NULL-safe on the C side

    def __del__(self):
        try:
            self.close()
        except Exception:
            pass

    def __enter__(self) -> "Orchestrator":
        return self

    def __exit__(self, *exc) -> None:
        self.close()

    def _step(self, op_name: str, args) -> StepResult:
        self._check()
        res: "_StepResultC" = getattr(lib, op_name)(self._h, *args)
        reason = res.reason  # read the thread-local pointer NOW (pre-next-call)
        step = StepResult(action=int(res.action), token=int(res.response_token),
                          final_state=int(res.final_state), final_type=int(res.final_type),
                          reason="" if reason is None else bytes(reason).decode("utf-8", "replace"),
                          code=int(res.error))
        if step.code != OK:
            raise_last_error(lib, step.code, op_name.replace("gsml3_orchestrator_", "orchestrator_"))
        return step

    def feed(self, msg, session=None) -> StepResult:
        """Feed one parsed L3 message into the chain. None msg raises TypeError
        before FFI (NULL policy); a closed message/registry raises
        ClosedGsmL3ObjectError; a C-level failure raises GsmL3Error."""
        if not isinstance(msg, Message):
            raise TypeError(f"msg must be a Message, got {type(msg).__name__}")
        msg._check()
        return self._step("gsml3_orchestrator_feed", (msg._h, _session_handle(session)))

    def feed_auth_challenge(self, rand: bytes, sres: bytes) -> StepResult:
        """Typed external data: 16-octet RAND in wire order + 4-octet SRES in
        big-endian (octet 0 = MSB). Any other length raises ValueError."""
        r = _byteslike(rand, "rand")
        s = _byteslike(sres, "sres")
        if len(r) != 16:
            raise ValueError(f"rand must be exactly 16 octets (wire order), got {len(r)}")
        if len(s) != 4:
            raise ValueError(f"sres must be exactly 4 octets (big-endian), got {len(s)}")
        return self._step("gsml3_orchestrator_feed_auth_challenge", (r, s))

    def feed_vlr_decision(self, accept: bool, has_new_tmsi: bool = False,
                          new_tmsi: int = 0, reject_cause: int = 0) -> StepResult:
        """VLR decision after authentication/identity (MMRejectCause value in
        C enums for rejects)."""
        if not isinstance(accept, bool) or not isinstance(has_new_tmsi, bool):
            raise TypeError("accept and has_new_tmsi must be bools")
        _as_int(new_tmsi, "new_tmsi", lo=0, hi=0xFFFFFFFF)
        return self._step("gsml3_orchestrator_feed_vlr_decision",
                          (1 if accept else 0, 1 if has_new_tmsi else 0, new_tmsi, reject_cause))

    def feed_ciphering(self, algo: int, enable: bool) -> StepResult:
        _as_u8(algo, "algo")
        if not isinstance(enable, bool):
            raise TypeError("enable must be a bool")
        return self._step("gsml3_orchestrator_feed_ciphering", (algo, 1 if enable else 0))

    def feed_paging_trigger(self, id_type: int, tmsi: int, imsi,
                            target_channel: int) -> StepResult:
        """Paging trigger; id_type is GSML3_ID_TMSI/GSML3_ID_IMSI (imsi digits
        when IMSI, else None); target_channel is a gsml3parser::ChannelType."""
        _as_int(id_type, "id_type", lo=0)
        _as_int(tmsi, "tmsi", lo=0, hi=0xFFFFFFFF)
        _as_int(target_channel, "target_channel", lo=0)
        return self._step("gsml3_orchestrator_feed_paging_trigger",
                          (id_type, tmsi, _enc(imsi, "imsi"), target_channel))

    def tick(self, delta_ms: int) -> int:
        """Advance the chain's own procedure timers (the T31xx timeouts of the
        ACTIVE procedure, e.g. T3101 = 3 s on the MO call-setup chain);
        returns the number of procedure FAILURES (0 inside the window)."""
        _as_int(delta_ms, "delta_ms", lo=0, hi=0xFFFFFFFF)
        self._check()
        return int(lib.gsml3_orchestrator_tick(self._h, delta_ms))

    def take_retransmit(self) -> int:
        """Drain the retransmission channel (consume-on-read). Returns a
        GSML3_TOKEN_* value (TOKEN_NONE when nothing is queued); after a
        non-none token, build and send that response."""
        self._check()
        return int(lib.gsml3_orchestrator_take_retransmit(self._h))

    def cancel_all(self) -> None:
        """Cancel the active chain (back to idle)."""
        self._check()
        lib.gsml3_orchestrator_cancel_all(self._h)

    @property
    def chain_phase(self) -> int:
        """GSML3_PROC_* of the current phase; PROC_UNKNOWN when idle."""
        self._check()
        return int(lib.gsml3_orchestrator_chain_phase(self._h))

    def required_size(self, session=None) -> int:
        """Exact wire size of the PENDING (last-token) response in bytes
        (zero-alloc); 0 when nothing can be built (the last error explains
        why — typically INVALID_VALUE)."""
        self._check()
        return int(lib.gsml3_orchestrator_required_size(self._h, _session_handle(session)))

    def build_response(self, session=None) -> bytes:
        """Build the pending response into an EXACT-SIZE buffer (decision #8):
        n = required_size() (0 raises — nothing pending / missing parameter),
        then one write into a c_ubyte array of exactly n octets. Returns the
        L3 bytes (caller owns them)."""
        n = self.required_size(session)
        if n == 0:
            raise_last_error(lib, None, "orchestrator_build_response")
        buf = (ctypes.c_ubyte * n)()
        s = _session_handle(session)
        rc = int(lib.gsml3_orchestrator_build_response(self._h, s, buf, c_size_t(n)))
        if rc != n:
            raise_last_error(lib, None, "orchestrator_build_response")  # too-small/invalid — the C message names it
        return bytes(buf)


# -- S7 standalone response builders (stateless; LAI args are STRINGS here) --

def response_build_cm_service_accept() -> bytes:
    return _builder("gsml3_response_build_cm_service_accept", ())


def response_build_cm_service_reject(mm_cause: int) -> bytes:
    """MMRejectCause range-checked in C (out-of-domain -> INVALID_ARG)."""
    return _builder("gsml3_response_build_cm_service_reject", (mm_cause,))


def response_build_identity_request(id_type: int) -> bytes:
    return _builder("gsml3_response_build_identity_request", (_as_int(id_type, "id_type", lo=0),))


def response_build_authentication_request(rand) -> bytes:
    """rand: the 16-octet challenge in wire order."""
    r = _byteslike(rand, "rand")
    if len(r) != 16:
        raise ValueError(f"rand must be exactly 16 octets, got {len(r)}")
    return _builder("gsml3_response_build_authentication_request", (r,))


def response_build_location_updating_accept(mcc: str, mnc: str, lac: int,
                                            has_new_tmsi: bool = False, new_tmsi: int = 0) -> bytes:
    """S7 STRING LAI form: mcc exactly 3 BCD digits ('244'), mnc 2-3 digits
    ('05'); both range-checked in C (out-of-domain -> INVALID_ARG)."""
    if not isinstance(has_new_tmsi, bool):
        raise TypeError("has_new_tmsi must be a bool")
    _as_int(new_tmsi, "new_tmsi", lo=0, hi=0xFFFFFFFF)
    return _builder("gsml3_response_build_location_updating_accept",
                    (_enc(mcc, "mcc"), _enc(mnc, "mnc"),
                     _as_int(lac, "lac", lo=0, hi=65535),
                     1 if has_new_tmsi else 0, new_tmsi))


def response_build_location_updating_reject(mm_cause: int) -> bytes:
    return _builder("gsml3_response_build_location_updating_reject", (mm_cause,))


def response_build_tmsi_reallocation_command(mcc: str, mnc: str, lac: int, tmsi: int) -> bytes:
    """S7 STRING LAI form (mcc '244' / mnc '05'); tmsi != all-zero is enforced in C."""
    _as_int(tmsi, "tmsi", lo=0, hi=0xFFFFFFFF)
    return _builder("gsml3_response_build_tmsi_reallocation_command",
                    (_enc(mcc, "mcc"), _enc(mnc, "mnc"), _as_int(lac, "lac", lo=0, hi=65535), tmsi))


def response_build_channel_release(rr_cause: int) -> bytes:
    return _builder("gsml3_response_build_channel_release", (rr_cause,))


def response_build_ciphering_mode_command(algo: int) -> bytes:
    return _builder("gsml3_response_build_ciphering_mode_command", (_as_u8(algo, "algo"),))


def response_build_physical_information(ta: int) -> bytes:
    """Timing advance (C checks the 0..63 on-wire field width)."""
    return _builder("gsml3_response_build_physical_information", (ta,))


def response_build_immediate_assignment(type_and_offset: int, tn: int, tsc: int,
                                        arfcn: int, ta: int) -> bytes:
    """Channel description: tn/tsc 0..7 and arfcn 0..1023 are width-checked in C."""
    return _builder("gsml3_response_build_immediate_assignment",
                    (type_and_offset, tn, tsc, arfcn, ta))


def response_build_assignment_command(type_and_offset: int, tn: int, tsc: int,
                                      arfcn: int) -> bytes:
    return _builder("gsml3_response_build_assignment_command", (type_and_offset, tn, tsc, arfcn))


def response_build_call_proceeding(ti: int) -> bytes:
    return _builder("gsml3_response_build_call_proceeding", (_as_int(ti, "ti", lo=0, hi=7),))


def response_build_alerting(ti: int) -> bytes:
    return _builder("gsml3_response_build_alerting", (_as_int(ti, "ti", lo=0, hi=7),))


def response_build_connect(ti: int) -> bytes:
    return _builder("gsml3_response_build_connect", (_as_int(ti, "ti", lo=0, hi=7),))


def response_build_connect_acknowledge(ti: int) -> bytes:
    return _builder("gsml3_response_build_connect_acknowledge", (_as_int(ti, "ti", lo=0, hi=7),))


def response_build_disconnect(ti: int, cc_cause: int) -> bytes:
    """cc_cause is a CCCause value (range-checked in C)."""
    return _builder("gsml3_response_build_disconnect", (_as_int(ti, "ti", lo=0, hi=7), cc_cause))


def response_build_release(ti: int, cc_cause: int) -> bytes:
    return _builder("gsml3_response_build_release", (_as_int(ti, "ti", lo=0, hi=7), cc_cause))


def response_build_release_complete(ti: int) -> bytes:
    return _builder("gsml3_response_build_release_complete", (_as_int(ti, "ti", lo=0, hi=7),))


def response_build_setup(called_digits: str, ti: int) -> bytes:
    """NOTE the S7 argument order (called_digits FIRST): the S9 gsml3_build_setup
    uses the inverse order (ti first). BCD digit string, e.g. '123456789'."""
    return _builder("gsml3_response_build_setup", (_enc(called_digits, "called_digits"),
                                                   _as_int(ti, "ti", lo=0, hi=7)))


# -- S7 token-based pending-response pair -----------------------------------

def response_required_size(token: int, session=None) -> int:
    """Exact wire size of the response that gsml3_response_build_from_token() would
    produce for ``token`` (GSML3_TOKEN_*): a buffer of this many bytes is guaranteed
    to be accepted. 0 when the response cannot be built (NULL session, out-of-domain
    token, missing parameter — the C last error explains). Zero allocation."""
    return int(lib.gsml3_response_required_size(_as_int(token, "token", lo=0),
                                                _session_handle(session)))


def response_build_from_token(token: int, session=None,
                              out: bytearray | None = None) -> bytes:
    """Build the response for ``token`` from the session's ResponseContext.
    ``out=None`` allocates the exact size via required_size() first (decision
    #8); a mutable bytearray is written in place (C touches its storage only
    during this one call)."""
    s = _session_handle(session)
    t = _as_int(token, "token", lo=0)
    if out is None:
        n = response_required_size(t, session)
        if n == 0:
            raise_last_error(lib, None, "response_build_from_token")
        buf = (ctypes.c_ubyte * n)()
    else:
        if not isinstance(out, bytearray):
            raise TypeError("out must be a mutable bytearray or None")
        buf = (ctypes.c_ubyte * len(out)).from_buffer(out)  # in-place via shared storage
    rc = int(lib.gsml3_response_build_from_token(t, s, buf, c_size_t(len(buf))))
    if rc == 0:
        raise_last_error(lib, None, "response_build_from_token")
    return bytes(buf[:rc])


# ── S9 typed builders (43; stateless L3 frame constructors) ─────────────────

def build_channel_release(rr_cause: int) -> bytes:
    return _builder("gsml3_build_channel_release", (rr_cause,))


def build_channel_request(ra: int) -> bytes:
    return _builder("gsml3_build_channel_request", (_as_u8(ra, "ra"),))


def build_immediate_assignment(type_and_offset: int, tn: int, tsc: int, arfcn: int,
                               ta: int, ra: int) -> bytes:
    """tn/tsc 0..7, arfcn 0..1023, ta 0..63 — width-checked in C."""
    return _builder("gsml3_build_immediate_assignment",
                    (type_and_offset, tn, tsc, arfcn, ta, ra))


def build_immediate_assignment_reject(wait_seconds: int) -> bytes:
    return _builder("gsml3_build_immediate_assignment_reject", (_as_u8(wait_seconds, "wait_seconds"),))


def build_assignment_command(type_and_offset: int, tn: int, tsc: int, arfcn: int) -> bytes:
    return _builder("gsml3_build_assignment_command", (type_and_offset, tn, tsc, arfcn))


def build_assignment_complete(rr_cause: int) -> bytes:
    return _builder("gsml3_build_assignment_complete", (rr_cause,))


def build_assignment_failure(rr_cause: int) -> bytes:
    return _builder("gsml3_build_assignment_failure", (rr_cause,))


def build_paging_request_type1(tmsi: int) -> bytes:
    """tmsi must NOT be all-zero (C rejects with INVALID_ARG)."""
    _as_int(tmsi, "tmsi", lo=0, hi=0xFFFFFFFF)
    return _builder("gsml3_build_paging_request_type1", (tmsi,))


def build_paging_request_type2(tmsi0: int, tmsi1: int) -> bytes:
    for name, v in (("tmsi0", tmsi0), ("tmsi1", tmsi1)):
        _as_int(v, name, lo=0, hi=0xFFFFFFFF)
    return _builder("gsml3_build_paging_request_type2", (tmsi0, tmsi1))


def build_paging_request_type3(tmsi0: int, tmsi1: int, tmsi2: int, tmsi3: int) -> bytes:
    for i, v in enumerate((tmsi0, tmsi1, tmsi2, tmsi3), start=0):
        _as_int(v, f"tmsi{i}", lo=0, hi=0xFFFFFFFF)
    return _builder("gsml3_build_paging_request_type3", (tmsi0, tmsi1, tmsi2, tmsi3))


def build_paging_response(id_type: int, tmsi: int, imsi=None) -> bytes:
    """id_type: GSML3_ID_*; imsi digits for IMSI identities else None."""
    return _builder("gsml3_build_paging_response",
                    (_as_int(id_type, "id_type", lo=0), _as_int(tmsi, "tmsi", lo=0, hi=0xFFFFFFFF),
                     _enc(imsi, "imsi")))


def build_ciphering_mode_command(algo: int) -> bytes:
    return _builder("gsml3_build_ciphering_mode_command", (_as_u8(algo, "algo"),))


def build_ciphering_mode_complete(response: int) -> bytes:
    return _builder("gsml3_build_ciphering_mode_complete", (_as_int(response, "response", lo=0),))


def build_handover_complete(rr_cause: int) -> bytes:
    return _builder("gsml3_build_handover_complete", (rr_cause,))


def build_physical_information(ta: int) -> bytes:
    return _builder("gsml3_build_physical_information", (_as_int(ta, "ta", lo=0),))


def build_cm_service_request(service_type: int, id_type: int, tmsi: int, imsi=None) -> bytes:
    """L3CMServiceType::TypeCode value (MobileOriginatedCall = 1; LocationUpdateRequest
    = 105 does not fit the 4-bit wire field and starts no procedure chain); id_type
    is GSML3_ID_*, imsi digits for IMSI identities else None."""
    return _builder("gsml3_build_cm_service_request",
                    (_as_int(service_type, "service_type", lo=0),
                     _as_int(id_type, "id_type", lo=0),
                     _as_int(tmsi, "tmsi", lo=0, hi=0xFFFFFFFF), _enc(imsi, "imsi")))


def build_cm_service_accept() -> bytes:
    return _builder("gsml3_build_cm_service_accept", ())


def build_cm_service_reject(mm_cause: int) -> bytes:
    return _builder("gsml3_build_cm_service_reject", (mm_cause,))


def build_cm_service_abort(abort_cause: int) -> bytes:
    """CMServiceAbortCause value (range-checked in C)."""
    return _builder("gsml3_build_cm_service_abort", (abort_cause,))


def build_identity_request(id_type: int) -> bytes:
    return _builder("gsml3_build_identity_request", (_as_int(id_type, "id_type", lo=0),))


def build_identity_response(id_type: int, tmsi: int, imsi=None) -> bytes:
    return _builder("gsml3_build_identity_response",
                    (_as_int(id_type, "id_type", lo=0),
                     _as_int(tmsi, "tmsi", lo=0, hi=0xFFFFFFFF), _enc(imsi, "imsi")))


def build_location_updating_request(update_type: int, id_type: int, tmsi: int, imsi=None,
                                    mcc: int = 244, mnc: int = 5, lac: int = 0) -> bytes:
    """S9 NUMERIC LAI form (mcc/mnc are ints: 244 / 5) — unlike the S7 response
    builders which take strings. update_type: 0=Normal, 1=Periodic, 2=IMSI Attach."""
    _as_int(update_type, "update_type", lo=0, hi=2)
    return _builder("gsml3_build_location_updating_request",
                    (update_type, _as_int(id_type, "id_type", lo=0),
                     _as_int(tmsi, "tmsi", lo=0, hi=0xFFFFFFFF), _enc(imsi, "imsi"),
                     mcc, mnc, lac))


def build_location_updating_accept(mcc: int, mnc: int, lac: int,
                                   has_new_tmsi: bool = False, new_tmsi: int = 0) -> bytes:
    """S9 NUMERIC LAI form (ints)."""
    if not isinstance(has_new_tmsi, bool):
        raise TypeError("has_new_tmsi must be a bool")
    _as_int(new_tmsi, "new_tmsi", lo=0, hi=0xFFFFFFFF)
    return _builder("gsml3_build_location_updating_accept",
                    (mcc, mnc, lac, 1 if has_new_tmsi else 0, new_tmsi))


def build_location_updating_reject(mm_cause: int) -> bytes:
    return _builder("gsml3_build_location_updating_reject", (mm_cause,))


def build_authentication_request(cksn: int, rand) -> bytes:
    """rand: the 16-octet challenge in wire order."""
    r = _byteslike(rand, "rand")
    if len(r) != 16:
        raise ValueError(f"rand must be exactly 16 octets, got {len(r)}")
    return _builder("gsml3_build_authentication_request", (_as_u8(cksn, "cksn"), r))


def build_authentication_response(sres: int) -> bytes:
    """32-bit SRES (big-endian on the wire)."""
    return _builder("gsml3_build_authentication_response",
                    (_as_int(sres, "sres", lo=0, hi=0xFFFFFFFF),))


def build_tmsi_reallocation_command(mcc: int, mnc: int, lac: int, tmsi: int) -> bytes:
    """S9 NUMERIC LAI form; all-zero tmsi rejected by C."""
    _as_int(tmsi, "tmsi", lo=0, hi=0xFFFFFFFF)
    return _builder("gsml3_build_tmsi_reallocation_command", (mcc, mnc, lac, tmsi))


def build_tmsi_reallocation_complete() -> bytes:
    return _builder("gsml3_build_tmsi_reallocation_complete", ())


def build_imsi_detach_indication(id_type: int, tmsi: int, imsi=None) -> bytes:
    return _builder("gsml3_build_imsi_detach_indication",
                    (_as_int(id_type, "id_type", lo=0),
                     _as_int(tmsi, "tmsi", lo=0, hi=0xFFFFFFFF), _enc(imsi, "imsi")))


def build_setup(ti: int, called_digits: str) -> bytes:
    """NOTE the S9 argument order (ti FIRST): the S7 gsml3_response_build_setup
    takes (called_digits, ti). BCD digit string, e.g. '123456789'."""
    return _builder("gsml3_build_setup", (_as_int(ti, "ti", lo=0, hi=7),
                                         _enc(called_digits, "called_digits")))


def build_call_proceeding(ti: int) -> bytes:
    return _builder("gsml3_build_call_proceeding", (_as_int(ti, "ti", lo=0, hi=7),))


def build_alerting(ti: int) -> bytes:
    return _builder("gsml3_build_alerting", (_as_int(ti, "ti", lo=0, hi=7),))


def build_connect(ti: int) -> bytes:
    return _builder("gsml3_build_connect", (_as_int(ti, "ti", lo=0, hi=7),))


def build_connect_acknowledge(ti: int) -> bytes:
    return _builder("gsml3_build_connect_acknowledge", (_as_int(ti, "ti", lo=0, hi=7),))


def build_disconnect(ti: int, cc_cause: int) -> bytes:
    return _builder("gsml3_build_disconnect", (_as_int(ti, "ti", lo=0, hi=7), cc_cause))


def build_release(ti: int, cc_cause: int) -> bytes:
    return _builder("gsml3_build_release", (_as_int(ti, "ti", lo=0, hi=7), cc_cause))


def build_release_complete(ti: int) -> bytes:
    return _builder("gsml3_build_release_complete", (_as_int(ti, "ti", lo=0, hi=7),))


def build_facility(ti: int, data) -> bytes:
    b = _byteslike(data, "data")
    return _builder("gsml3_build_facility", (_as_int(ti, "ti", lo=0, hi=7), b, len(b)))


def build_cp_data(rpdu) -> bytes:
    b = _byteslike(rpdu, "rpdu")
    return _builder("gsml3_build_cp_data", (b, len(b)))


def build_cp_status(tp_oi: int, mti_value: int, has_ref: bool = False, ref: int = 0) -> bytes:
    if not isinstance(has_ref, bool):
        raise TypeError("has_ref must be a bool")
    return _builder("gsml3_build_cp_status", (_as_u8(tp_oi, "tp_oi"), _as_u8(mti_value, "mti_value"),
                                              1 if has_ref else 0, _as_u8(ref, "ref")))


def build_cp_smt(rpdu) -> bytes:
    b = _byteslike(rpdu, "rpdu")
    return _builder("gsml3_build_cp_smt", (b, len(b)))


def build_sms_deliver(tp_mti: int, tp_mr: int, ud) -> bytes:
    b = _byteslike(ud, "ud")
    return _builder("gsml3_build_sms_deliver", (_as_u8(tp_mti, "tp_mti"), _as_u8(tp_mr, "tp_mr"), b, len(b)))


def build_sup_serv_facility(ti: int, data) -> bytes:
    b = _byteslike(data, "data")
    return _builder("gsml3_build_sup_serv_facility", (_as_int(ti, "ti", lo=0, hi=7), b, len(b)))


# ── S5 LAPDm entity (FSM wrapper) ──────────────────────────────────────────

class LapdmEntity:
    """OWNED wrapper over gsml3_lapdm_entity — the LAPDm link FSM.

    Two construction paths (both documented):
      * standalone (this __init__): l3_cb / l1_cb are None (C-documented NULL
        callbacks) or plain Python callables — the wrapper creates CFUNCTYPE
        objects around them and ANCHORS every object it creates in
        ``self._c_bridges``; that dict is the caller-side anchor: C retains
        raw function pointers, so the wrappers must outlive any callback the
        C core can still issue. Advanced use only.
      * internal factory ``_with_bridges(profile, l3_cb, l1_cb, user)``:
        registers prebuilt CFUNCTYPE objects with the C core WITHOUT storing
        them — in that path the GC anchor is GsmL3Stack._c_cb_keepalive (the
        keep-alive dict is the SOLE owner of stack-side callbacks; the GC test
        relies on this exact shape).

    Callback contract (gsml3parser_c.h "Entity callbacks"): invoked
    SYNCHRONOUSLY from receive() / send_*, with spans valid only DURING the
    call — transmit or copy synchronously, never retain; do NOT free this
    entity (or any gsml3_* handle it references) from within its own
    callbacks.

    profile: 0 = SDCCH (N201=20, T200=900 ms), 1 = SACCH (N201=18,
    T200=3600 ms), 2 = FACCH (N201=20, T200=900 ms); invalid values are
    rejected HERE before FFI and by C (NULL result) as backstop.
    """

    def __init__(self, profile: int = 0, l3_cb=None, l1_cb=None, user=None) -> None:
        if isinstance(profile, bool) or not isinstance(profile, int) or not 0 <= profile <= 2:
            raise ValueError(f"profile must be 0 (SDCCH), 1 (SACCH) or 2 (FACCH); got {profile!r}")
        self._c_bridges = {}  # CFUNCTYPE objects WE created — the caller-side anchor
        l3fn, l1fn = self._wrap_callbacks(l3_cb, l1_cb)
        h = lib.gsml3_lapdm_entity_new(profile, l3fn, l1fn, _as_user(user))
        if h is None:
            raise_last_error(lib, None, "lapdm_entity_new")
        self._h = h
        self._profile = profile
        self._closed = False

    @classmethod
    def _with_bridges(cls, profile: int, l3_cb, l1_cb, user) -> "LapdmEntity":
        """Internal factory used ONLY by GsmL3Stack. Registers the given
        prebuilt L3CB/L1CB objects and deliberately stores NOTHING (the anchor
        is GsmL3Stack._c_cb_keepalive). Raises TypeError if the objects are
        not exactly CFUNCTYPE instances — a plain callable here would be
        collected with no anchor (UB on the C side)."""
        if not isinstance(l3_cb, L3CB):
            raise TypeError("_with_bridges l3_cb must be a prebuilt L3CB ctypes callback")
        if not isinstance(l1_cb, L1CB):
            raise TypeError("_with_bridges l1_cb must be a prebuilt L1CB ctypes callback")
        if isinstance(profile, bool) or not isinstance(profile, int) or not 0 <= profile <= 2:
            raise ValueError(f"profile must be 0 (SDCCH), 1 (SACCH) or 2 (FACCH); got {profile!r}")
        obj = cls.__new__(cls)
        obj._c_bridges = {}  # intentionally empty: the stack anchors the callbacks
        h = lib.gsml3_lapdm_entity_new(profile, l3_cb, l1_cb, _as_user(user))
        if h is None:
            raise_last_error(lib, None, "lapdm_entity_new")
        obj._h = h
        obj._profile = profile
        obj._closed = False
        return obj

    def _wrap_callbacks(self, l3_cb, l1_cb):
        out = []
        for cb, ct, what in ((l3_cb, L3CB, "l3_cb"), (l1_cb, L1CB, "l1_cb")):
            if cb is None:
                # C-documented NULL callback. ctypes 3.14 does not convert a bare
                # None into a function-pointer arg — build a true NULL of the exact
                # CFUNCTYPE class via cast (a null c_void_p cast to ct).
                out.append(ctypes.cast(ctypes.c_void_p(0), ct))
            elif isinstance(cb, ct):
                out.append(cb)    # prebuilt CFUNCTYPE — caller's anchor responsibility
            elif callable(cb):
                fn = ct(cb)       # WE created it: anchor so the GC can never collect it
                self._c_bridges[what] = fn
                out.append(fn)
            else:
                raise TypeError(f"{what} must be None, a callable, or a prebuilt {ct.__name__}")
        return out[0], out[1]

    def _check(self) -> None:
        if self._closed or self._h is None:
            raise ClosedGsmL3ObjectError("lapdm entity is closed")

    # -- link control ----------------------------------------------------------

    def open(self, sapi: int, command_bit: int) -> None:
        """Transition the FSM to LINK_RELEASED. sapi 0..15 (ValueError before FFI;
        C re-validates and — on failure — leaves the entity in its previous
        state, which we surface as GsmL3Error). command_bit: 1 = BTS side
        (C/R=1), 0 = MS side."""
        self._check()
        _as_int(sapi, "sapi", lo=0, hi=15)
        if isinstance(command_bit, bool) or not isinstance(command_bit, int) or command_bit not in (0, 1):
            raise ValueError(f"command_bit must be 0 (MS) or 1 (BTS); got {command_bit!r}")
        lib.gsml3_lapdm_entity_open(self._h, sapi, command_bit)
        code = int(lib.gsml3_last_error_code())
        if code != OK:
            raise_last_error(lib, code, "lapdm_entity_open")

    def receive(self, frame) -> None:
        """Feed one raw LAPDm frame from L1 into the FSM. Callbacks fire here,
        synchronously. None -> TypeError and len < 2 -> ValueError BEFORE FFI."""
        self._check()
        b = _byteslike(frame, "frame")
        if len(b) < 2:
            raise ValueError("LAPDm frame too short (<2 bytes: address + control)")
        lib.gsml3_lapdm_entity_receive(self._h, b, c_size_t(len(b)))

    # -- transmissions (C return codes are mapped to typed errors) ----------------

    def send_ui(self, sapi: int, l3) -> None:
        """Send L3 data via a UI frame — no link establishment required (works in
        ANY state). ``sapi`` selects the address-octet SAPI of the emitted frame
        and may differ from the open() sapi (which alone owns the FSM);
        out-of-range raises ValueError before FFI; C failure -> GsmL3Error."""
        self._check()
        _as_int(sapi, "sapi", lo=0, hi=15)
        b = _byteslike(l3, "l3")
        rc = int(lib.gsml3_lapdm_entity_send_ui(self._h, sapi, b, c_size_t(len(b))))
        if rc != OK:
            raise_last_error(lib, rc, "lapdm_entity_send_ui")

    def send_data(self, l3) -> None:
        """Send L3 data via I-frames (segmented if needed); REQUIRES an established
        link — sending before that raises GsmL3Error with C's last error."""
        self._check()
        b = _byteslike(l3, "l3")
        rc = int(lib.gsml3_lapdm_entity_send_data(self._h, b, c_size_t(len(b))))
        if rc != OK:
            raise_last_error(lib, rc, "lapdm_entity_send_data")

    def send_sabme(self) -> None:
        """SABME (link establishment; requires LINK_RELEASED state)."""
        self._check()
        rc = int(lib.gsml3_lapdm_entity_send_sabme(self._h))
        if rc != OK:
            raise_last_error(lib, rc, "lapdm_entity_send_sabme")

    def send_disc(self) -> None:
        """DISC (link release; requires an established link)."""
        self._check()
        rc = int(lib.gsml3_lapdm_entity_send_disc(self._h))
        if rc != OK:
            raise_last_error(lib, rc, "lapdm_entity_send_disc")

    def hard_release(self) -> None:
        """Immediate transition to LINK_RELEASED without sending frames."""
        self._check()
        lib.gsml3_lapdm_entity_hard_release(self._h)

    # -- timers / state / statistics ------------------------------------------------

    def tick_t200(self, elapsed_ms: int) -> int:
        """Advance the T200 retransmission timer. Returns 1 if a retransmission or
        abnormal release happened, 0 otherwise; the C internal-error form (-1)
        raises GsmL3Error with its last error (typically INTERNAL)."""
        self._check()
        _as_int(elapsed_ms, "elapsed_ms", lo=0, hi=0xFFFFFFFF)
        rc = int(lib.gsml3_lapdm_entity_tick_t200(self._h, elapsed_ms))
        if rc < 0:
            raise_last_error(lib, None, "lapdm_entity_tick_t200")
        return rc

    def state(self) -> int:
        """Current FSM state (GSML3_LAPDM_STATE_*)."""
        self._check()
        return int(lib.gsml3_lapdm_entity_state(self._h))

    def is_established(self) -> bool:
        """True in LINK_ESTABLISHED or CONTENTION_RESOLUTION."""
        self._check()
        return int(lib.gsml3_lapdm_entity_is_established(self._h)) == 1

    def frames_sent(self) -> int:
        """L1 frames sent by the FSM (statistic counter)."""
        self._check()
        return int(lib.gsml3_lapdm_entity_frames_sent(self._h))

    def frames_received(self) -> int:
        """L1 frames received by the FSM (statistic counter)."""
        self._check()
        return int(lib.gsml3_lapdm_entity_frames_received(self._h))

    def retransmissions(self) -> int:
        """T200-driven retransmission count."""
        self._check()
        return int(lib.gsml3_lapdm_entity_retransmissions(self._h))

    # -- RAII ---------------------------------------------------------------------

    def close(self) -> None:
        """Release the entity (C stops invoking its callbacks after this). Idempotent;
        no FFI once closed."""
        if self._closed:
            return
        self._closed = True
        h, self._h = self._h, None
        lib.gsml3_lapdm_entity_free(h)  # NULL-safe on the C side

    def __del__(self):
        try:
            self.close()
        except Exception:
            pass

    def __enter__(self) -> "LapdmEntity":
        return self

    def __exit__(self, *exc) -> None:
        self.close()


# ── GsmL3Stack: the queue-model BTS stack ───────────────────────────────────

class _CallbackContext(ctypes.Structure):
    """C-visible guard token for the entity's ``void* user`` parameter.

    The CFUNCTYPE closures capture the stack's queues directly; the token lets
    a callback recognize its owner (and reject itself after close) even if the
    Python closure were somehow evicted: ``alive == 0`` means the owning
    GsmL3Stack is closed. The instance stays alive through
    GsmL3Stack._c_cb_keepalive["ctx"] — C stored the address taken from it at
    entity creation, which is stable for its lifetime; the closures are kept
    alive by the same dict (hard keep-alive: GC can never collect a callback
    C still references).
    """

    _fields_ = [("alive", ctypes.c_int)]


def _make_l3_bridge(ctx, events, lock):
    """L3 bridge closure for one GsmL3Stack (queue model, decision #3).

    Runs INSIDE a synchronous C call. Only memory-safe work is permitted here:
    read the span (zero-copy view via string_at) and append one copy under the
    stack lock. No FFI calls from this frame — re-entering gsml3_* through the
    same entity is forbidden by the ABI (gsml3parser_c.h "Entity callbacks"),
    and send_ui/feed from within the callback would mutate that very FSM mid-
    receive. Orchestration happens post-FFI in GsmL3Stack.send_frame.
    """
    def _bridge(sapi, primitive, l3p, l3_len, user):
        if ctx.alive != 1:
            return  # stack closed: late callback, drop silently
        data = ctypes.string_at(l3p, l3_len) if (l3p is not None and l3_len > 0) else b""
        with lock:
            events.append(L3Event(sapi=int(sapi), primitive=int(primitive), data=data))
    return _bridge


def _make_l1_bridge(ctx, events, lock):
    """L1 (transmit-frame) bridge: identical discipline, frame span -> bytes copy."""
    def _bridge(frame, frame_len, user):
        if ctx.alive != 1:
            return
        data = ctypes.string_at(frame, frame_len) if (frame is not None and frame_len > 0) else b""
        with lock:
            events.append(data)
    return _bridge


class GsmL3Stack:
    """BTS-side reference stack over the C core — the unified Python/Go/Rust
    surface.

    Creation order (reversed in close()): registry -> session (BORROWED) ->
    orchestrator -> callback context + CFUNCTYPE keep-alive -> entity ->
    entity.open(sapi, command_bit=1 /*BTS side*/). close() frees entity ->
    orchestrator -> registry (the session is NEVER freed — owned by the
    registry per the ABI) after marking the callback context dead, so any late
    C callback becomes a no-op. Idempotent; every method raises
    ClosedGsmL3ObjectError WITHOUT FFI after close (proven via _library's
    CALL_COUNTS seam in the tests — raw C handles have no "closed" state).

    send_frame(frame) — unified semantics, identical across all three
    bindings:
      1) reset the tx collector for this input;
      2) FFI entity.receive(frame) — C callbacks ONLY enqueue (no FFI inside);
      3) IF auto_response (default True): drain the L3 queue POST-FFI and, per
         event with a payload: parse L3 -> orchestrator.feed(session); if the
         step carries token != TOKEN_NONE: required_size() -> exact-buffer
         build_response() -> entity.send_ui(stack sapi) — new tx frames are
         captured by the L1 bridge;
      4) IF NOT auto_response: L3 events stay UNPROCESSED (retrieve with
         drain_l3_events(), orchestrate manually via feed_l3/build_response/
         send_ui);
      5) return the tx frames collected for THIS input (the C-FSM L2 reactions
         plus any auto responses; caller owns the bytes).
    Parse/feed failures in step 3 do NOT abort the frame: they are recorded and
    surfaced via last_event_error (mirror of Go LastEventError / Rust GsmL3Error
    propagation). last_step is the StepResult of the most recently processed
    event.

    Threading: one stack belongs to one thread at a time (C ABI contract — one
    thread per handle); the RLock guards the receive/drain/orchestrate path, it
    is not a sharing mechanism.
    """

    def __init__(self, *, tmsi: int | None = None, imsi: str | None = None,
                 shard_count: int = 0, sapi: int = 0, profile: int = 0,
                 auto_response: bool = True) -> None:
        if (tmsi is None) == (imsi is None):
            raise ValueError("exactly one of tmsi or imsi must be provided")
        if tmsi is not None:
            # The all-zero TMSI is intentionally NOT rejected here: that rule
            # belongs to the C side (INVALID_ARG per TS 24.008, passed through).
            _as_int(tmsi, "tmsi", lo=0, hi=0xFFFFFFFF)
        else:
            if not isinstance(imsi, str) or not imsi:
                raise ValueError("imsi must be a non-empty ASCII digit string")
        _as_int(shard_count, "shard_count")
        if shard_count not in Registry._ALLOWED_SHARDS:
            raise ValueError(f"shard_count must be one of {Registry._ALLOWED_SHARDS}; got {shard_count}")
        _as_int(sapi, "sapi", lo=0, hi=15)
        _as_int(profile, "profile", lo=0, hi=2)  # 0 SDCCH / 1 SACCH / 2 FACCH

        self._lock = threading.RLock()
        self._closed = False
        self._l3_events: list[L3Event] = []   # drained after each C call returns
        self._tx_events: list[bytes] = []     # frames captured by the L1 bridge
        self._last_event_error: GsmL3Error | None = None
        self._last_step: StepResult | None = None
        with self._lock:
            try:
                # Creation order per class doc; destruction reversed in close().
                self._registry = Registry(shard_count=shard_count)
                if tmsi is not None:
                    self._session = self._registry.create_by_tmsi(tmsi)
                else:
                    # imsi requires shard_count == 0 (C raises UNSUPPORTED otherwise).
                    self._session = self._registry.create_by_imsi(imsi)
                self._orch = Orchestrator()
                self._ctx = _CallbackContext(1)
                # `_c_cb_keepalive` is the SOLE GC anchor for the two CFUNCTYPE
                # closures (and, transitively, the queues they capture): C
                # stores only raw function pointers; a collected closure would
                # be dead memory C jumps into. Do NOT add instance attributes
                # for these on this path — the GC test asserts this exact dict.
                self._c_cb_keepalive = {
                    "l3": L3CB(_make_l3_bridge(self._ctx, self._l3_events, self._lock)),
                    "l1": L1CB(_make_l1_bridge(self._ctx, self._tx_events, self._lock)),
                    "ctx": self._ctx,
                }
                user = ctypes.cast(ctypes.byref(self._ctx), ctypes.c_void_p)  # address stable while the "ctx" object lives
                self._entity = LapdmEntity._with_bridges(
                    profile=profile,
                    l3_cb=self._c_cb_keepalive["l3"],
                    l1_cb=self._c_cb_keepalive["l1"], user=user)
                self._sapi = sapi
                self._auto_response = bool(auto_response)
                # 1 = BTS side (C/R=1): we are the network.
                self._entity.open(sapi=sapi, command_bit=1)
            except Exception:
                # Any failed step rolls back every handle created so far.
                self.close()
                raise

    def _ensure_open(self) -> None:
        if self._closed:
            raise ClosedGsmL3ObjectError("GsmL3Stack is closed")

    # -- identity / session state (all through the borrowed session) -----------

    @property
    def tmsi(self) -> int:
        """Identity TMSI of the session (0 for IMSI-created sessions)."""
        self._ensure_open()
        return self._session.tmsi

    @property
    def assigned_tmsi(self) -> int:
        """Key TMSI in the registry; auto-assigned one for IMSI-created sessions."""
        self._ensure_open()
        return self._session.assigned_tmsi

    def set_imsi(self, digits: str) -> None:
        """Set the session IMSI (BCD digit string)."""
        self._ensure_open()
        self._session.set_imsi(digits)

    @property
    def registered(self) -> bool:
        self._ensure_open()
        return self._session.registered

    @property
    def authenticated(self) -> bool:
        self._ensure_open()
        return self._session.authenticated

    @property
    def ciphered(self) -> bool:
        self._ensure_open()
        return self._session.ciphered

    # -- observations (no FFI beyond the wrapped single call each) -------------

    @property
    def chain_phase(self) -> int:
        """GSML3_PROC_* of the active chain phase; PROC_UNKNOWN when idle."""
        self._ensure_open()
        with self._lock:
            return self._orch.chain_phase

    @property
    def state(self) -> int:
        """LAPDm FSM state (GSML3_LAPDM_STATE_*)."""
        self._ensure_open()
        return self._entity.state()

    @property
    def established(self) -> bool:
        self._ensure_open()
        return self._entity.is_established()

    @property
    def last_step(self) -> StepResult | None:
        """StepResult of the most recent L3 event processed (auto mode: inside
        send_frame; manual mode: by feed_l3); None until then."""
        return self._last_step

    @property
    def last_event_error(self) -> GsmL3Error | None:
        """Last parse/feed failure recorded while processing an L3 event
        (send_frame in auto_response mode); None when clean. The frame itself
        still completed — the event was skipped, not the frame."""
        return self._last_event_error

    # -- queues -----------------------------------------------------------------

    def drain_l3_events(self) -> list[L3Event]:
        """Take-and-empty the unprocessed L3 event queue (auto_response=False
        mode; in auto mode events are consumed inside send_frame). No FFI."""
        self._ensure_open()
        with self._lock:
            out = self._l3_events[:]
            self._l3_events.clear()
            return out

    def drain_tx_frames(self) -> list[bytes]:
        """Take-and-empty pending L1 transmit frames. No FFI."""
        self._ensure_open()
        with self._lock:
            out = self._tx_events[:]
            self._tx_events.clear()
            return out

    # -- the unified send path ----------------------------------------------------

    def send_frame(self, frame) -> list[bytes]:
        """Feed one raw LAPDm frame into the C entity and drive the chain (see
        the class doc for the full 1-5 semantics). None raises TypeError; a
        frame shorter than address+control (<2 bytes) raises ValueError — both
        BEFORE the FFI boundary."""
        self._ensure_open()
        b = _byteslike(frame, "frame")
        if len(b) < 2:
            raise ValueError("LAPDm frame too short (<2 bytes: address + control)")
        with self._lock:
            self._tx_events.clear()  # (1) collector for THIS input only
            # (2) C FSM; callbacks only enqueue — no FFI inside them.
            lib.gsml3_lapdm_entity_receive(self._entity._h, b, c_size_t(len(b)))
            if self._auto_response:
                pending = self._l3_events[:]   # (3) post-FFI drain — never re-enter
                self._l3_events.clear()
                for ev in pending:
                    if not ev.data:
                        continue  # link-state primitives carry no payload
                    try:
                        with Message.from_bytes(ev.data) as msg:
                            step = self._orch.feed(msg, self._session)
                    except GsmL3Error as exc:  # parse or feed failure: recorded, frame continues
                        self._last_event_error = exc
                        continue
                    self._last_step = step
                    if step.token != TOKEN_NONE:
                        resp = self._orch.build_response(self._session)  # required_size -> exact buffer
                        self._entity.send_ui(self._sapi, resp)           # L1 bridge captures the tx frames
            out = self._tx_events[:]     # (5) everything this input produced
            self._tx_events.clear()
            return out

    def feed_l3(self, l3) -> StepResult:
        """Direct orchestration WITHOUT L2 (test/simulation hook; the manual mode
        of auto_response=False): parse + feed(session). The response is NOT sent
        automatically — use build_response() then send_ui()."""
        self._ensure_open()
        b = _byteslike(l3, "l3")
        if not b:
            raise TypeError("l3 must be a non-empty bytes-like object")
        with self._lock:
            with Message.from_bytes(b) as msg:
                step = self._orch.feed(msg, self._session)
            self._last_step = step
            return step

    def build_response(self) -> bytes:
        """required_size() -> exact-buffer build of the PENDING response; a missing
        pending response raises GsmL3Error (INVALID_VALUE), never b\"\"."""
        self._ensure_open()
        with self._lock:
            return self._orch.build_response(self._session)

    def send_ui(self, l3, sapi: int | None = None) -> list[bytes]:
        """Transmit one response manually (auto_response=False mode). Returns the
        tx frame(s) captured for this transmission."""
        self._ensure_open()
        s = self._sapi if sapi is None else _as_int(sapi, "sapi", lo=0, hi=15)
        b = _byteslike(l3, "l3")
        with self._lock:
            self._tx_events.clear()
            self._entity.send_ui(s, b)
            out = self._tx_events[:]
            self._tx_events.clear()
            return out

    # -- ticks --------------------------------------------------------------------

    def tick_timers(self, delta_ms: int, cap: int = 16) -> list[TimerExpiryPy]:
        """SESSION-level timer expiries (registry pass-through), as a list of
        TimerExpiryPy(borrowed session view, timer_id)."""
        self._ensure_open()
        with self._lock:
            pairs = self._registry.tick_timers(delta_ms, cap=cap)
            return [TimerExpiryPy(session=sess, timer_id=t) for sess, t in pairs]

    def tick_procedures(self, delta_ms: int) -> int:
        """Tick the ORCHESTRATOR CHAIN timers — the procedure timeouts owned by
        the active chain (e.g. T3101 = 3 s on MO call setup); returns the number
        of failures (0 inside the window). Distinct from Registry.tick_procedures,
        which advances registry-level session procedures."""
        self._ensure_open()
        with self._lock:
            return self._orch.tick(delta_ms)

    def take_retransmit(self) -> int:
        """Drain the retransmission channel (consume-on-read); GSML3_TOKEN_* 
(GSM04.08 retransmit slot) — returns GSML3_TOKEN_NONE when nothing is queued. After a
non-none token, build and send that response via build_response()/send_ui()."""
        self._ensure_open()
        with self._lock:
            return int(lib.gsml3_orchestrator_take_retransmit(self._orch._h))

    def tick_t200(self, elapsed_ms: int) -> int:
        """Advance the entity's LAPDm T200 (profile-dependent, e.g. 900 ms for SDCCH):
        returns 1 when a retransmission/abnormal release happened, 0 otherwise;
        the C internal-error form raises GsmL3Error."""
        self._ensure_open()
        with self._lock:
            return self._entity.tick_t200(elapsed_ms)

    # -- RAII ----------------------------------------------------------------------

    def close(self) -> None:
        """Idempotent teardown — the single code path for explicit close() and the
        __del__ finalizer. First sets the callback context alive-flag to 0 (late C
        callbacks become silent no-ops), then frees in the ABI order
        entity_free -> orchestrator_free -> registry_free (the session is NEVER
        freed directly — gsml3_registry_free releases it along with all others).
        The CFUNCTYPE keep-alive anchors are dropped only AFTER the entity is
        gone, since from that instant C can never invoke them again. After
        close: every method raises ClosedGsmL3ObjectError WITHOUT any FFI."""
        if self._closed:
            return
        with self._lock:
            if self._closed:  # re-check under the lock (idempotency under racing closes)
                return
            self._closed = True
            ctx = getattr(self, "_ctx", None)
            if ctx is not None:
                ctx.alive = 0
            entity = getattr(self, "_entity", None)
            orch = getattr(self, "_orch", None)
            reg = getattr(self, "_registry", None)
            for obj in (entity, orch, reg):
                if obj is not None:
                    try:
                        obj.close()
                    except Exception:  # teardown must never raise (also finalizer path)
                        pass
            keepalive = getattr(self, "_c_cb_keepalive", None)
            if keepalive is not None:
                keepalive.clear()

    def __del__(self):
        try:
            self.close()
        except Exception:  # finalizers must never raise; C frees are NULL-safe anyway
            pass

    def __enter__(self) -> "GsmL3Stack":
        return self

    def __exit__(self, *exc) -> None:
        self.close()
 
