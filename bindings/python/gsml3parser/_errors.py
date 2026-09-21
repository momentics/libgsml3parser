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

"""Error model of the libgsml3parser C ABI (Python binding).

The C ABI reports failures in two paired ways:

* a return code — `enum gsml3_error`, mirrored 1:1 by the module constants
  below;
* thread-local state: ``gsml3_last_error_code()`` and ``gsml3_last_error()``.

Every function that performs an operation clears any pending error on entry,
so a failed call leaves exactly one fresh (code, message) pair in
thread-local storage and the NEXT successful call erases it. Consequently the
message must be copied SYNCHRONOUSLY at the failing call site — never later,
never from another thread, never after any other FFI call.
:func:`raise_last_error` is the single place where that copy happens; it maps
the code onto the typed exception hierarchy below (errno/strerror idiom), so
callers can branch on the failure class programmatically.
"""

# Error codes, mirror `enum gsml3_error` in
# include/gsml3parser/gsml3parser_c.h 1:1 — do not renumber or rename.
OK = 0
INVALID_ARG = 1
TRUNCATED = 2
INVALID_PD = 3
INVALID_MTI = 4
LENGTH_MISMATCH = 5
INVALID_IE = 6
INVALID_VALUE = 7
UNSUPPORTED = 8
SOURCE_EXHAUSTED = 9
NO_MEMORY = 10
BUFFER_TOO_SMALL = 11
INTERNAL = 12
DUPLICATE = 13

#: Synthetic code for binding-level errors with no C origin (e.g. access to a
#: closed object). Never produced by the library.
CODE_CLOSED = -1


class GsmL3Error(Exception):
    """Base class for all libgsml3parser failures raised by this binding."""

    def __init__(self, code: int, message: str) -> None:
        super().__init__(message)
        #: C error code (``enum gsml3_error``), or CODE_CLOSED for the pure
        #: Python-side closed-object error.
        self.code = code
        #: Human-readable detail; ``"<op>: <C message>"`` when the C side
        #: supplied one, ``"<op>"`` otherwise.
        self.message = message

    @classmethod
    def _for_code(cls, code: int, message: str) -> "GsmL3Error":
        """Factory: map a C code onto its subclass (unknown codes -> base)."""
        klass = _CLASS_BY_CODE.get(code, GsmL3Error)
        return klass(code, message)


class InvalidArgument(GsmL3Error):
    """GSML3_ERR_INVALID_ARG (1): out-of-domain value, reserved TMSI, bad digit string."""


class TruncatedInput(GsmL3Error):
    """GSML3_ERR_TRUNCATED (2): input ended before the frame was complete."""


class ProtocolDomainError(GsmL3Error):
    """INVALID_PD / INVALID_MTI / INVALID_IE / INVALID_VALUE (3/4/6/7)."""


class UnsupportedOperation(GsmL3Error):
    """GSML3_ERR_UNSUPPORTED (8), e.g. create_by_imsi on a sharded registry."""


# Named without shadowing builtins: the C code is NO_MEMORY=10.
class GsmL3NoMemory(GsmL3Error):
    """GSML3_ERR_NO_MEMORY (10)."""


class BufferTooSmall(GsmL3Error):
    """GSML3_ERR_BUFFER_TOO_SMALL (11): the C side names the exact required size in its message."""


class InternalError(GsmL3Error):
    """GSML3_ERR_INTERNAL (12): unexpected condition inside the library."""


class ClosedGsmL3ObjectError(GsmL3Error):
    """Access to a closed registry / session view / orchestrator / entity / stack.

    Raised purely on the Python side: touching the raw C handle after its free
    would be undefined behavior, so wrappers refuse before the FFI boundary
    (NULL policy). Carries ``code == CODE_CLOSED``.
    """

    def __init__(self, message: str) -> None:
        super().__init__(CODE_CLOSED, message)


_CLASS_BY_CODE = {
    INVALID_ARG: InvalidArgument,
    TRUNCATED: TruncatedInput,
    INVALID_PD: ProtocolDomainError,
    INVALID_MTI: ProtocolDomainError,
    INVALID_IE: ProtocolDomainError,
    INVALID_VALUE: ProtocolDomainError,
    UNSUPPORTED: UnsupportedOperation,
    NO_MEMORY: GsmL3NoMemory,
    BUFFER_TOO_SMALL: BufferTooSmall,
    INTERNAL: InternalError,
}


def _to_text(value) -> str:
    """Normalize a C-string ctypes result (bytes/str/pointer-object) to text."""
    if value is None:
        return ""
    # Defensive: tolerate pointer-object result shapes (they expose .value).
    if not isinstance(value, (bytes, str)) and hasattr(value, "value"):
        value = value.value
    if isinstance(value, bytes):
        return value.decode("utf-8", "replace")
    if isinstance(value, str):
        return value
    return str(value)


def raise_last_error(lib, code: int | None = None, op: str = "gsml3 call") -> None:
    """Copy the thread-local C error state NOW and raise a typed exception.

    ``lib`` is the registered library proxy (any object exposing the raw C
    functions). When ``code`` is omitted it is read from
    ``gsml3_last_error_code()``. The code AND the message are both copied
    here, synchronously: any later successful FFI call clears the pending
    error, so deferring the copy would silently lose it (see module doc).

    :raises GsmL3Error: a subclass selected by ``_for_code``; the message is
        ``f"{op}: {c_message}"`` when the C side set one, else ``op``.
    """
    if code is None:
        code = int(lib.gsml3_last_error_code())
    msg = _to_text(lib.gsml3_last_error()).strip()
    raise GsmL3Error._for_code(int(code), f"{op}: {msg}" if msg else op)
