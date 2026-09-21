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

"""Minimal LAPDm (GSM 04.06) MS/peer-side frame builder for simulation.

Purpose: simulation and tests need to BUILD Mobile-station-side frames to feed
into a BTS-side entity (the C API decodes peer frames and TRANSMITS only the
entity's own frames). Production transmission goes through the C entity — do
not use these builders on that path.

Byte layout rules mirror src/lapdm_frame.cpp 1:1 (anchor: the U-frame control
byte switch): address = (sapi << 4) | (C/R) | 0x01 (EA=1); U-control bytes by
(type, pf=0/1): UI 0x03/0x07, SABME 0x2B/0x2F, UA 0x5F/0x63, DM 0x0B/0x0F,
DISC 0x0C/0x08; I-frame control = (NR << 5) | (PF << 4) | (NS << 1), preceded
by a length octet (M << 7 | len & 0x3F) before the info; S-frame control =
(NR << 5) | (PF << 4) | type (RR=0x01 / REJ=0x0D). SABME/UA carry a length
octet only when they carry info (m=0); UI carries raw info with NO length
octet. Every builder validates its inputs first: sapi 0..15, NR/NS 0..7,
info <= 63 bytes — violations raise ValueError before any byte is produced.
Each generated frame in the test-suite is cross-checked against the C
decoder gsml3_lapdm_frame_decode (closed loop).
"""


def _check_sapi(sapi):
    if not isinstance(sapi, int) or isinstance(sapi, bool) or not 0 <= sapi <= 15:
        raise ValueError(f"sapi out of range (0..15): {sapi!r}")
    return sapi


def _check_seq(value, what):
    if not isinstance(value, int) or isinstance(value, bool) or not 0 <= value <= 7:
        raise ValueError(f"{what} out of range (0..7): {value!r}")
    return value & 0x07


def _check_info(info, what="info"):
    if info is None or isinstance(info, (bytes, bytearray, memoryview)):
        data = bytes(info) if info is not None else b""
    else:
        raise ValueError(f"{what} must be bytes-like: {type(info).__name__}")
    if len(data) > 63:
        raise ValueError(f"{what} exceeds the LAPDm 63-octet info field: {len(data)}")
    return data


_U_CTRL = {  # control byte by (type, pf): rules from src/lapdm_frame.cpp
    ("ui", 0): 0x03, ("ui", 1): 0x07,
    ("sabme", 0): 0x2B, ("sabme", 1): 0x2F,
    ("ua", 0): 0x5F, ("ua", 1): 0x63,
    ("dm", 0): 0x0B, ("dm", 1): 0x0F,
    ("disc", 0): 0x0C, ("disc", 1): 0x08,
}


def _addr(sapi: int, command: bool) -> int:
    """Address octet: (sapi << 4) | (C/R ? 0x08 : 0) | 0x01 (EA=1)."""
    return (_check_sapi(sapi) << 4) | (0x08 if command else 0) | 0x01


def ui(sapi: int, command: bool, info: bytes, pf: bool = False) -> bytes:
    """UI frame: [address][control] + raw info — NO length octet."""
    ctrl = _U_CTRL[("ui", 1 if pf else 0)]
    return bytes((_addr(sapi, command), ctrl)) + _check_info(info)


def ua(sapi: int = 0, pf: bool = True) -> bytes:
    """UA (unnumbered acknowledgement): [address][control] only."""
    return bytes((_addr(sapi, False), _U_CTRL[("ua", 1 if pf else 0)]))


def sabme(sapi: int, command: bool, pf: bool = True, info: bytes = b"") -> bytes:
    """SABME; carries a length octet (m=0) only when it carries info."""
    ctrl = _U_CTRL[("sabme", 1 if pf else 0)]
    out = bytes((_addr(sapi, command), ctrl))
    info = _check_info(info or b"")
    if info:
        out += bytes((len(info) & 0x3F,)) + info  # m = 0
    return out


def dm(sapi: int = 0, pf: bool = False) -> bytes:
    """DM (discouraged mode): [address][control] only."""
    return bytes((_addr(sapi, False), _U_CTRL[("dm", 1 if pf else 0)]))


def disc(sapi: int, command: bool, pf: bool = True) -> bytes:
    """DISC (disconnect request/ack): [address][control] only."""
    return bytes((_addr(sapi, command), _U_CTRL[("disc", 1 if pf else 0)]))


def rr(nr: int, sapi: int = 0, pf: bool = False) -> bytes:
    """S-frame RR (receive ready): [address][NR<<5 | PF<<4 | 0x01]."""
    nr = _check_seq(nr, "nr")
    return bytes((_addr(sapi, False), (nr << 5) | (0x10 if pf else 0) | 0x01))


def rej(nr: int, sapi: int = 0, pf: bool = False) -> bytes:
    """S-frame REJ: [address][NR<<5 | PF<<4 | 0x0D]."""
    nr = _check_seq(nr, "nr")
    return bytes((_addr(sapi, False), (nr << 5) | (0x10 if pf else 0) | 0x0D))


def i_frame(sapi: int, command: bool, nr: int, ns: int,
            pf: bool, m: bool, info: bytes) -> bytes:
    """I-frame: [address][NR<<5|PF<<4|NS<<1][length (M<<7|len)] + info."""
    nr = _check_seq(nr, "nr")
    ns = _check_seq(ns, "ns")
    info = _check_info(info)
    ctrl = (nr << 5) | (0x10 if pf else 0) | (ns << 1)
    return bytes((_addr(sapi, command), ctrl, (0x80 if m else 0) | len(info))) + info


__all__ = ["ui", "ua", "sabme", "dm", "disc", "rr", "rej", "i_frame"]
