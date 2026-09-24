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

"""Core wrapper tests: parse/hex/write/size/dump, error paths, C-documented
NULL safety (raw level), config semantics, GC keep-alive of C callbacks,
RAII finalizers, closed-path no-FFI and a leak smoke run.

The NULL-safety block deliberately goes through the RAW registered functions
(``_library.lib`` with None/NULL) because the C API documents those exact
sentinels/no-op behaviors at that level — the wrappers translate them to
language errors (covered in test_stack_sim).
"""

import ctypes
import gc

import pytest

from gsml3parser import _library
import gsml3parser as g


# ── Parse / serialize round trips ──────────────────────────────────────────

def parse_hex_channel_release():
    """'60 0D 00' = RR Channel Release (stable vector, mirrors the C test suite)."""
    m = g.Message.from_hex("60 0D 00")
    try:
        assert m.name != ""
        assert m.pd == g.PD_RR == 6
        assert m.mti == 0x0D == 13
        assert m.ti == 0
        assert m.size() == 3
        assert m.write() == bytes([0x60, 0x0D, 0x00])
        assert m.hex() == "600d00"          # lowercase, no spaces (C contract)
    finally:
        m.close()


# Byte-exact copy of the C test table `kBatch` in tests/test_c_api.cpp —
# each vector is C-verified; values are NOT simplified or shortened here.
K_BATCH = [
    (g.PD_RR,   "60 0D 00",          0x0D),   # Channel Release
    (g.PD_MM,   "50 84",             0x21),   # CM Service Accept
    (g.PD_CC,   "3E 94 08 02 16 21", 0x25),   # Disconnect (TI=7)
    (g.PD_SS,   "B0 E8 00",          0x3A),   # SupServFacilityMessage (empty facility)
    (g.PD_GMM,  "80 20 05",          0x20),   # GMM Status (cause=5)
    (g.PD_SM,   "A0 55 A7 01 05",    0x55),   # SM Status (cause=5)
    (g.PD_SMS,  "90 04",             0x04),   # CP-Ack (no body)
    (g.PD_BCC,  "10 00",             0x00),   # BCC Setup
    (g.PD_GCC,  "00 00 02",          0x00),   # GCC Setup
    (g.PD_LS,   "C0 01",             0x01),   # LocationServiceRequest
    (g.PD_EXT,  "E0 01",             0x01),   # ExtendedMessage
    (g.PD_TST,  "F0 01",             0x01),   # TestProcedureMessage
]


@pytest.mark.parametrize("pd,hexstr,mti", K_BATCH, ids=[f"PD{pd:02X}_{hexstr}" for pd, hexstr, _ in K_BATCH])
def roundtrip_12_pd_vectors(pd, hexstr, mti):
    """Parse -> metadata (pd/mti) -> exact wire write -> reparse identity.
    The write is byte-exact against the original vector."""
    m = g.Message.from_hex(hexstr)
    try:
        assert m.name != ""
        assert m.pd == pd
        assert m.mti == mti
        wire = m.write()                      # exact-size buffer (size())
        expected = bytes.fromhex(hexstr.replace(" ", ""))
        assert m.size() == len(expected) == len(wire)
        assert wire == expected
        assert m.hex() == hexstr.lower().replace(" ", "")  # lowercase, no spaces
        m2 = g.Message.from_bytes(wire)       # reparse the produced bytes
        try:
            assert m2.name == m.name
            assert m2.pd == m.pd
            assert m2.mti == m.mti
            assert m2.write() == wire
        finally:
            m2.close()
    finally:
        m.close()


def parse_into_reuses_the_handle():
    """parse_into is the zero-extra-allocation reparse hot path; a failure keeps
    the PREVIOUS CONTENT (C semantics of gsml3_parse_l3_into). The failure
    vector mirrors TEST(CApi, L3ErrorPaths): '60 0D' = truncated RR."""
    m = g.Message.from_hex("60 0D 00")
    try:
        m.parse_into(bytes([0x50, 0x84]))     # -> CM Service Accept
        assert m.name != "ChannelRelease" and m.pd == g.PD_MM
        with pytest.raises(g.GsmL3Error):
            m.parse_into(bytes([0x60, 0x0D]))  # truncated RR ChannelRelease
        assert m.pd == g.PD_MM                # previous content KEPT after failure
    finally:
        m.close()


# ── Error paths (wrapper level, pre-FFI + C failures) ──────────────────────

def error_paths():
    """Wrapper-level rejections (None/empty input -> TypeError BEFORE FFI) and
    C parse failures surfaced as typed GsmL3Errors with the synchronous last-
    error copy. Vectors mirror TEST(CApi, L3ErrorPaths): '60 0D' is the
    canonical truncated RR ChannelRelease (a lone octet '60' IS a complete
    single-octet RR message for this core and parses fine — verified)."""
    with pytest.raises(TypeError):
        g.Message.from_bytes(b"")             # empty input: language-level error
    with pytest.raises(TypeError):
        g.Message.from_bytes(None)
    with pytest.raises(g.GsmL3Error) as exc:
        g.Message.from_hex("60 0D")           # truncated RR ChannelRelease (C vector)
    assert exc.value.code == g.TRUNCATED == 2
    assert exc.value.message.strip() != ""    # synchronous last-error copy present
    with pytest.raises(g.GsmL3Error) as exc2:
        g.Message.from_hex("zz")              # non-hex characters (C vector)
    assert exc2.value.code == g.INVALID_VALUE == 7   # the code C reports for this case

    # strict config mirrors TEST(CApi, L3ErrorPaths): trailing byte rejected under
    # strict framing, tolerated (consumed leniently) by default.
    with g.Config(strict_framing=True) as strict:
        ok = g.Message.from_hex("50 84", cfg=strict)
        ok.close()
        with pytest.raises(g.GsmL3Error):
            g.Message.from_bytes(b"\x50\x84\x00", cfg=strict)
    # default (lenient) config accepts the same trailing byte
    lenient = g.Message.from_bytes(b"\x50\x84\x00")
    assert lenient.name != ""
    lenient.close()


# ── NULL safety: C-documented sentinels and no-ops, RAW level ─────────────

def null_safety_c_documented():
    """Mirror of TEST(CApi, NullSafety) + section tests: the raw registered
    functions pass NULL through to C and return the documented sentinels."""
    lib = _library.lib
    assert lib.gsml3_message_name(None) == b""
    assert int(lib.gsml3_message_pd(None)) == -1
    assert int(lib.gsml3_message_mti(None)) == -1
    assert int(lib.gsml3_message_ti(None)) == 0
    out8 = (ctypes.c_ubyte * 8)()   # caller buffer, like `uint8_t buf[8]` in the C test
    assert int(lib.gsml3_message_write(None, out8, 8)) == 0
    assert lib.gsml3_message_hex(None) is None
    assert int(lib.gsml3_parse_l3_into(None, b"\x60\x0d", 2, None)) != g.OK

    # every release function is a documented no-op on NULL
    for name in ("gsml3_free", "gsml3_message_free", "gsml3_config_free",
                 "gsml3_rsl_free", "gsml3_registry_free", "gsml3_orchestrator_free",
                 "gsml3_lapdm_entity_free"):
        getattr(lib, name)(None)  # must not raise (NULL-safe free family)

    # entity state/statistics observers are NULL-safe
    assert int(lib.gsml3_lapdm_entity_state(None)) == g.LAPDM_STATE_UNUSED == 0
    assert int(lib.gsml3_lapdm_entity_is_established(None)) == 0
    for name in ("gsml3_lapdm_entity_frames_sent", "gsml3_lapdm_entity_frames_received",
                 "gsml3_lapdm_entity_retransmissions"):
        assert int(getattr(lib, name)(None)) == 0


# -- Error channel after void/count-only C calls -----------------------------

def _install_forced_error(funcs, code: int, message: bytes):
    """Replace the two thread-local error observers with fakes, as if a C call
    had just set the pending state. Returns the restore callable."""
    saved = {k: funcs.get(k) for k in ("gsml3_last_error_code", "gsml3_last_error")}

    def code_fn():
        return code

    def msg_fn():
        return message

    funcs["gsml3_last_error_code"] = code_fn
    funcs["gsml3_last_error"] = msg_fn

    def restore():
        for name, fn in saved.items():
            if fn is None:
                funcs.pop(name, None)
            else:
                funcs[name] = fn

    return restore


@pytest.mark.parametrize(
    "op",
    ["receive", "hard_release", "cancel_all", "tick", "take_retransmit", "reserve", "timer_stop"],
)
def test_void_calls_surface_the_pending_error(op):
    """Every mutating call whose C signature carries no error return value
    (void or count-only — gsml3parser_c.h reports such failures only through
    gsml3_last_error* and clears them on the next successful call) is polled
    synchronously after the FFI call. A forced GSML3_ERR_INTERNAL is raised as
    a typed GsmL3Error instead of being swallowed."""
    funcs = _library._FUNCS
    cleanups = []

    if op == "receive":
        e = g.LapdmEntity(0)
        cleanups.append(e.close)
        e.open(sapi=0, command_bit=1)      # warm up while the error state is still real
        act = lambda: e.receive(bytes([0x01, 0x63]))
    elif op == "hard_release":
        e = g.LapdmEntity(0)
        cleanups.append(e.close)
        act = lambda: e.hard_release()
    elif op in ("cancel_all", "tick", "take_retransmit"):
        o = g.Orchestrator()
        cleanups.append(o.close)
        act = {"cancel_all": o.cancel_all,
               "tick": lambda: o.tick(1),
               "take_retransmit": o.take_retransmit}[op]
    elif op == "reserve":
        r = g.Registry(0)
        cleanups.append(r.close)
        act = lambda: r.reserve(8)
    elif op == "timer_stop":
        r = g.Registry(0)
        cleanups.append(r.close)
        s = r.create_by_tmsi(0x67000001)
        act = lambda: s.timer_stop(0)      # GSML3_TIMER_T3101

    restore = _install_forced_error(funcs, g.INTERNAL, b"forced: unexpected exception in the core")
    try:
        with pytest.raises(g.InternalError) as excinfo:
            act()
        assert "forced: unexpected exception in the core" in excinfo.value.message
        assert excinfo.value.code == g.INTERNAL == 12
    finally:
        restore()
        for close in cleanups:
            close()


def config_setters():
    """Out-of-range log level is ignored by C (no error, no crash); the strict
    framing setter toggles parsing behavior (exercised end-to-end above)."""
    cfg = g.Config()
    try:
        cfg.set_log_level(99)          # out of range -> silently ignored by C
        cfg.set_log_level(3)           # GSML3_LOG_ERR
        cfg.set_strict_framing(True)   # takes effect on subsequent parses with this config
        m = g.Message.from_hex("60 0D 00", cfg=cfg)
        m.close()
    finally:
        cfg.close()


# ── GC keep-alive of C-registered callbacks (safety-critical invariant) ────

def test_callbacks_survive_gc():
    """If the CFUNCTYPE objects were collected, C would invoke dead memory:
    the process dies before this assert. Passing means _c_cb_keepalive works."""
    s = g.GsmL3Stack(tmsi=0x22222222, auto_response=False)  # callbacks reachable ONLY via keepalive
    try:
        assert {"l3", "l1", "ctx"} == set(s._c_cb_keepalive)  # exact key set — the sole anchor shape
        gc.collect(); gc.collect()
        frames = s.send_frame(g.lapdm_mini.ui(0, False, b"\x60\x0d\x00"))
        assert frames == []            # no L2 reaction expected; L3 event stays queued (auto=False)
        evs = s.drain_l3_events()
        assert evs and evs[0].primitive == g.PRIM_L3_UNIT_DATA == 4
    finally:
        s.close()


def test_finalizer_is_safe_after_explicit_close():
    s = g.GsmL3Stack(tmsi=0x22222223)
    s.close(); s.close()                                    # double close — no-op, no exception
    del s
    gc.collect()                                            # __del__ must not raise (no FFI after close)


def test_no_raw_calls_after_close():
    """_library.CALL_COUNTS counts every real FFI call by name; the
    closed path must perform NO FFI call at all."""
    s = g.GsmL3Stack(tmsi=0x22222224)
    base_receive = _library.CALL_COUNTS.get("gsml3_lapdm_entity_receive", 0)
    base_send_ui = _library.CALL_COUNTS.get("gsml3_lapdm_entity_send_ui", 0)
    s.close()
    with pytest.raises(g.ClosedGsmL3ObjectError):
        s.send_frame(g.lapdm_mini.ui(0, False, b"\x60\x0d\x00"))
    with pytest.raises(g.ClosedGsmL3ObjectError):
        _ = s.state
    assert _library.CALL_COUNTS.get("gsml3_lapdm_entity_receive", 0) == base_receive
    assert _library.CALL_COUNTS.get("gsml3_lapdm_entity_send_ui", 0) == base_send_ui
    # The closed path performs NO FFI at all — a binding-level invariant: raw C
    # handles have no "closed" state, so there is no C-side test to mirror.
    # Proven here via the _library.CALL_COUNTS test seam.


def test_leak_smoke_1k():
    """1k parse/write/hex/dump cycles — hard no-crash + deterministic behavior.
    A leaked C handle would show as address-space growth (CI OS-level gate);
    here every Message is explicitly closed, so the count of frees matches the
    count of parses exactly."""
    for _ in range(1000):
        m = g.Message.from_hex("60 0D 00")   # RAII: freed by close() below
        m.write(); m.hex(); m.dump()
        m.close()
