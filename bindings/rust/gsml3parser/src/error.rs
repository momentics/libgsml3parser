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

//! Error model: one typed error carrying the C code plus a
//! SYNCHRONOUSLY COPIED message from the thread-local `gsml3_last_error()`.
//! The copy must happen at the failing call site: every later successful FFI
//! call clears the pending error, so reading it afterwards loses the report.
//! The state observers themselves (`gsml3_last_error*`) never clear the error
//! and may be used while inspecting one.

use std::ffi::CStr;
use std::fmt;

/// 1:1 mirror of `enum gsml3_error` from `include/gsml3parser/gsml3parser_c.h`.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[repr(i32)]
pub enum ErrorKind {
    /// GSML3_OK
    Ok = 0,
    /// GSML3_ERR_INVALID_ARG — out-of-domain argument (enum range, digit strings, field widths).
    InvalidArg = 1,
    /// GSML3_ERR_TRUNCATED — input shorter than its header/body requirements.
    Truncated = 2,
    /// GSML3_ERR_INVALID_PD — unknown protocol discriminator.
    InvalidPd = 3,
    /// GSML3_ERR_INVALID_MTI — unknown message type identifier.
    InvalidMti = 4,
    /// GSML3_ERR_LENGTH_MISMATCH — declared size disagrees with the actual one.
    LengthMismatch = 5,
    /// GSML3_ERR_INVALID_IE — malformed information element.
    InvalidIe = 6,
    /// GSML3_ERR_INVALID_VALUE — invalid field value for a known message/operation.
    InvalidValue = 7,
    /// GSML3_ERR_UNSUPPORTED — not supported in this flavor/configuration.
    Unsupported = 8,
    /// GSML3_ERR_SOURCE_EXHAUSTED — no remaining candidates for an allocation.
    SourceExhausted = 9,
    /// GSML3_ERR_NO_MEMORY — allocation failure inside the library.
    NoMemory = 10,
    /// GSML3_ERR_BUFFER_TOO_SMALL — enlarge the caller buffer (see *_required_size).
    BufferTooSmall = 11,
    /// GSML3_ERR_INTERNAL — unexpected condition inside the library.
    Internal = 12,
    /// GSML3_ERR_DUPLICATE — a unique key is already present.
    Duplicate = 13,
}

impl ErrorKind {
    /// The numeric C code of this variant.
    pub const fn code(self) -> i32 {
        self as i32
    }

    /// Map a raw C code onto the enum; `None` for codes outside the mirror
    /// (defensive: unknown values stay visible through `GsmL3Error::code`).
    pub const fn from_code(code: i32) -> Option<ErrorKind> {
        match code {
            0 => Some(Self::Ok),
            1 => Some(Self::InvalidArg),
            2 => Some(Self::Truncated),
            3 => Some(Self::InvalidPd),
            4 => Some(Self::InvalidMti),
            5 => Some(Self::LengthMismatch),
            6 => Some(Self::InvalidIe),
            7 => Some(Self::InvalidValue),
            8 => Some(Self::Unsupported),
            9 => Some(Self::SourceExhausted),
            10 => Some(Self::NoMemory),
            11 => Some(Self::BufferTooSmall),
            12 => Some(Self::Internal),
            13 => Some(Self::Duplicate),
            _ => None,
        }
    }
}

impl fmt::Display for ErrorKind {
    /// Renders the code under the name the C header uses for it.
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(match self {
            Self::Ok => "OK",
            Self::InvalidArg => "INVALID_ARG",
            Self::Truncated => "TRUNCATED",
            Self::InvalidPd => "INVALID_PD",
            Self::InvalidMti => "INVALID_MTI",
            Self::LengthMismatch => "LENGTH_MISMATCH",
            Self::InvalidIe => "INVALID_IE",
            Self::InvalidValue => "INVALID_VALUE",
            Self::Unsupported => "UNSUPPORTED",
            Self::SourceExhausted => "SOURCE_EXHAUSTED",
            Self::NoMemory => "NO_MEMORY",
            Self::BufferTooSmall => "BUFFER_TOO_SMALL",
            Self::Internal => "INTERNAL",
            Self::Duplicate => "DUPLICATE",
        })
    }
}

/// A typed failure of one operation: the C error code,
/// the Rust call site that produced it, and a synchronous copy of the
/// thread-local `gsml3_last_error()` message. The message MUST be captured at
/// the failing call site — every later successful FFI call on this thread
/// clears the pending error, so reading it afterwards loses the report.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct GsmL3Error {
    /// The raw `gsml3_error` code (always preserved verbatim; see
    /// [`GsmL3Error::kind`] for the mapped class).
    pub code: i32,
    /// The Rust call site producing the error, e.g. `"Message::parse_hex"`.
    pub op: &'static str,
    /// Synchronous copy of the C message; empty when the C side set none.
    pub msg: String,
}

impl GsmL3Error {
    /// The failure class, when the raw code is a known `enum gsml3_error` value.
    pub const fn kind(&self) -> Option<ErrorKind> {
        ErrorKind::from_code(self.code)
    }

    /// True for the closed-class error produced by any access to an already
    /// closed object (NULL-policy: rejected at language level, no FFI made —
    /// a freed raw handle has no "closed" state in C to mirror).
    pub fn is_closed(&self) -> bool {
        self.code == crate::sys::GSML3_ERR_INVALID_ARG && self.msg.contains("closed")
    }
}

impl fmt::Display for GsmL3Error {
    /// `"gsml3: <op>: <msg> [<CODE>]"` (message omitted when empty), mirroring
    /// the errno/strerror idiom of the C layer and the Go/Python bindings.
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match ErrorKind::from_code(self.code) {
            Some(kind) if self.msg.is_empty() => write!(f, "gsml3: {} ({kind})", self.op),
            Some(kind) => write!(f, "gsml3: {}: {} ({kind})", self.op, self.msg),
            None if self.msg.is_empty() => write!(f, "gsml3: {} [code {}]", self.op, self.code),
            None => write!(f, "gsml3: {}: {} [code {}]", self.op, self.msg, self.code),
        }
    }
}

impl std::error::Error for GsmL3Error {}

impl From<GsmL3Error> for String {
    /// Convenience for callers whose error type is `String` (e.g. the demo's
    /// `run() -> Result<(), String>`): renders through [`GsmL3Error`]'s Display
    /// (`gsml3: <op>: <msg> [<CODE>]`).
    fn from(e: GsmL3Error) -> Self {
        e.to_string()
    }
}

/// Read the thread-local C error state into a typed error for `op`. When
/// `explicit_code` is not `GSML3_OK` it wins (the failing function already
/// reported its `gsml3_error` code in the return value); otherwise the code is
/// taken from `gsml3_last_error_code()`. MUST be called synchronously at the
/// failing call site — see the module docs.
pub fn last_error(op: &'static str, explicit_code: i32) -> GsmL3Error {
    let code = if explicit_code == crate::sys::GSML3_OK {
        // SAFETY: pure thread-local observer; takes no arguments, returns an int.
        unsafe { crate::sys::gsml3_last_error_code() }
    } else {
        explicit_code
    };
    let msg = unsafe {
        // SAFETY: gsml3_last_error never returns NULL ("", per header) and the
        // string is NUL-terminated; copying it does not move any FFI state.
        let p = crate::sys::gsml3_last_error();
        if p.is_null() {
            String::new()
        } else {
            CStr::from_ptr(p).to_string_lossy().into_owned()
        }
    };
    GsmL3Error { code, op, msg }
}

/// Pre-FFI validation error (NULL-policy: input the C side is documented to
/// reject is refused at the binding level, with a non-empty message and no FFI).
pub fn invalid_arg(op: &'static str, msg: &str) -> GsmL3Error {
    GsmL3Error {
        code: crate::sys::GSML3_ERR_INVALID_ARG,
        op,
        msg: msg.to_string(),
    }
}

/// Internal-failure error for wrapper-side invariants that must not happen.
pub(crate) fn internal(op: &'static str, msg: &str) -> GsmL3Error {
    GsmL3Error {
        code: crate::sys::GSML3_ERR_INTERNAL,
        op,
        msg: msg.to_string(),
    }
}

/// Closed-class error: the owning object was already released (take-pattern),
/// so every handle is null and no FFI may be attempted. Constructed WITHOUT any
/// C call (message wording matches the Python `ClosedGsmL3ObjectError` / Go
/// "…is closed" convention across bindings).
pub(crate) fn closed(op: &'static str) -> GsmL3Error {
    GsmL3Error {
        code: crate::sys::GSML3_ERR_INVALID_ARG,
        op,
        msg: format!("{op}: the object is already closed"),
    }
}

/// Guard for constructor-style results: a NULL handle means failure — read the
/// thread-local error state synchronously.
pub fn check_ptr<T>(op: &'static str, ptr: *mut T) -> Result<(), GsmL3Error> {
    if ptr.is_null() {
        Err(last_error(op, crate::sys::GSML3_OK))
    } else {
        Ok(())
    }
}

/// Error read for C operations that report failures ONLY through thread-local
/// state (void-returning calls such as `entity_open` with an invalid sapi,
/// `registry_clear` on a sharded registry, channel-ownership violations; and
/// `timer_start` with an out-of-range ID). Call IMMEDIATELY after the matching
/// FFI call and before any other gsml3_* operation on this thread.
pub(crate) fn void_result(op: &'static str) -> Result<(), GsmL3Error> {
    let code = unsafe { crate::sys::gsml3_last_error_code() }; // SAFETY: observer, no state change.
    if code == crate::sys::GSML3_OK {
        Ok(())
    } else {
        Err(last_error(op, code))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn error_kind_is_one_to_one_with_the_c_enum() {
        // Mirror check: all 14 values with the stable numeric mapping from `enum gsml3_error`.
        assert_eq!(ErrorKind::Ok.code(), crate::sys::GSML3_OK);
        assert_eq!(
            ErrorKind::InvalidArg.code(),
            crate::sys::GSML3_ERR_INVALID_ARG
        );
        assert_eq!(
            ErrorKind::Duplicate.code(),
            crate::sys::GSML3_ERR_DUPLICATE
        );
        assert_eq!(ErrorKind::from_code(11), Some(ErrorKind::BufferTooSmall));
        assert_eq!(ErrorKind::from_code(-1), None);
        assert_eq!(ErrorKind::from_code(99), None);
    }

    #[test]
    fn error_display_and_closed_class() {
        let e = GsmL3Error {
            code: crate::sys::GSML3_ERR_INVALID_ARG,
            op: "GsmL3Stack::send_frame",
            msg: "GsmL3Stack::send_frame: the object is already closed".to_string(),
        };
        assert!(e.is_closed());
        assert_eq!(
            e.to_string(),
            "gsml3: GsmL3Stack::send_frame: GsmL3Stack::send_frame: the object is already closed (INVALID_ARG)"
        );
        let ok_msg = GsmL3Error {
            code: crate::sys::GSML3_OK,
            op: "Message::parse",
            msg: String::new(),
        };
        assert_eq!(ok_msg.to_string(), "gsml3: Message::parse (OK)");
    }

    #[test]
    fn check_ptr_distinguishes_null_and_non_null() {
        use std::os::raw::c_void;
        let p = std::ptr::NonNull::dangling().as_ptr();
        assert!(check_ptr::<c_void>("t", p).is_ok());
        assert!(check_ptr::<c_void>("t", std::ptr::null_mut()).is_err());
    }
}
