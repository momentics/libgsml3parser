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

//! L3 messages (parse / metadata / serialize / hex / dump), parser config and
//! A-bis RSL frames — the S2/S3/S4 surface of the C ABI plus the two curated
//! S9 typed builders used by the demo chain.
//!
//! Ownership: every type here owns exactly one C
//! handle created through this wrapper and releases it with its paired
//! `gsml3_*_free` in `Drop`. The take-pattern (`Option<NonNull>`) makes an
//! explicit drop plus any later `Drop` a no-op second pass. All byte outputs
//! are COPIES owned by the caller; the only C pointers ever observed here are
//! (a) static storage for names (copied, never freed), and (b)
//! library-allocated hex/dump strings handled with the copy-then-free idiom
//! INSIDE one method call — a raw `char*` result never escapes a method.

use std::ffi::{CStr, CString};
use std::marker::PhantomData;
use std::os::raw::{c_char, c_void};
use std::ptr::{self, NonNull};

use crate::error;
use crate::error::GsmL3Error;
use crate::sys as s;

/// Parser configuration (log level, strict framing). Owns its `gsml3_config`
/// handle; released in `Drop`. Out-of-range log levels are ignored by the C
/// side per the ABI, so the setters only fail when the config is closed.
pub struct Config {
    p: Option<NonNull<c_void>>,
}

impl Config {
    /// Create a config with the C defaults (log level WARNING, lenient
    /// framing). Fails only on allocation error (NO_MEMORY from the thread-
    /// local state).
    pub fn new() -> Result<Config, GsmL3Error> {
        // SAFETY: stateless allocator call; returns a valid handle or NULL.
        let p = unsafe { s::gsml3_config_new() };
        if p.is_null() {
            return Err(error::last_error("Config::new", 0));
        }
        Ok(Config {
            // SAFETY: p was just checked to be non-null.
            p: Some(unsafe { NonNull::new_unchecked(p as *mut c_void) }),
        })
    }

    /// The live handle, or a closed-class error (take-pattern: once the field
    /// is `None` — after close/drop — no FFI may touch the freed config).
    fn ptr(&self, op: &'static str) -> Result<NonNull<c_void>, GsmL3Error> {
        match self.p {
            Some(p) => Ok(p),
            None => Err(error::closed(op)),
        }
    }

    /// Set the log level (`GSML3_LOG_*` constants from `gsml3parser_sys`).
    /// Out-of-range values are ignored by the C core (documented: no error).
    pub fn set_log_level(&self, level: i32) -> Result<(), GsmL3Error> {
        let p = self.ptr("Config::set_log_level")?;
        // SAFETY: p is a live gsml3_config*; the call cannot fail per the ABI.
        unsafe { s::gsml3_config_set_log_level(p.as_ptr(), level) };
        Ok(())
    }

    /// Enable strict framing (reject a frame whose message does not consume
    /// the entire input), or disable it.
    pub fn set_strict_framing(&self, on: bool) -> Result<(), GsmL3Error> {
        let p = self.ptr("Config::set_strict_framing")?;
        // SAFETY: p is a live gsml3_config*.
        unsafe { s::gsml3_config_set_strict_framing(p.as_ptr(), if on { 1 } else { 0 }) };
        Ok(())
    }
}

impl Drop for Config {
    fn drop(&mut self) {
        if let Some(p) = self.p.take() {
            // SAFETY: exactly one gsml3_config_free per created handle; NULL-safe in C.
            unsafe { s::gsml3_config_free(p.as_ptr()) };
        }
    }
}

/// A parsed L3 message. The handle owns the parsed variant for its whole
/// lifetime; [`Message::parse_into`] is the zero-extra-allocation reparse hot
/// path, and on failure the PREVIOUS CONTENT IS KEPT (mirrors
/// `gsml3_parse_l3_into` — C contract). Released in `Drop`.
pub struct Message {
    p: Option<NonNull<c_void>>,
    /// Ownership marker: the wrapper frees the C handle exactly once (the
    /// type itself carries no data beyond the pointer).
    _free: PhantomData<fn() -> ()>,
}

impl Message {
    /// Crate-internal handle accessor for FFI call sites outside this module
    /// (e.g. `GsmL3Stack` feeding this message into the orchestrator): the
    /// closed-class error keeps a freed handle from ever crossing the boundary.
    pub(crate) fn c_handle(&self, op: &'static str) -> Result<NonNull<c_void>, GsmL3Error> {
        self.ptr(op)
    }
}

/// Resolve an optional config reference into a `const gsml3_config*` for the
/// next FFI call; a closed config is a wrapper error, never a stale pointer.
fn cfg_arg(cfg: Option<&Config>, op: &'static str) -> Result<*const s::gsml3_config, GsmL3Error> {
    match cfg {
        None => Ok(ptr::null()),
        Some(c) => c.ptr(op).map(|p| p.as_ptr() as *const _),
    }
}

impl Message {
    /// The live C handle, or a closed-class error (take-pattern: once `None`
    /// no FFI may touch the freed message).
    pub(crate) fn ptr(&self, op: &'static str) -> Result<NonNull<c_void>, GsmL3Error> {
        match self.p {
            Some(p) => Ok(p),
            None => Err(error::closed(op)),
        }
    }

    /// Parse raw L3 bytes. Empty input is rejected BEFORE the FFI boundary
    /// (NULL-policy); a C-level parse failure returns the typed error with the
    /// synchronously copied thread-local message. `cfg` may be `None`.
    pub fn parse(data: &[u8], cfg: Option<&Config>) -> Result<Message, GsmL3Error> {
        if data.is_empty() {
            return Err(error::invalid_arg(
                "Message::parse",
                "empty input: an L3 frame needs at least discriminator + length bytes",
            ));
        }
        let cfg = cfg_arg(cfg, "Message::parse (config)")?;
        // SAFETY: data is a valid [u8]; cfg is null or live; C returns NULL on error.
        let p = unsafe { s::gsml3_parse_l3(data.as_ptr(), data.len(), cfg) };
        if p.is_null() {
            return Err(error::last_error("Message::parse", 0));
        }
        Ok(Message {
            // SAFETY: p is non-null here.
            p: Some(unsafe { NonNull::new_unchecked(p as *mut c_void) }),
            _free: PhantomData,
        })
    }

    /// Parse a hex string (spaces allowed, e.g. "60 0D 00"). C-level failures
    /// (truncated frame / invalid characters) return the typed error; `cfg`
    /// may be `None`. A Rust `&str` never holds an interior NUL, so the C
    /// string contract holds by construction.
    pub fn parse_hex(hex: &str, cfg: Option<&Config>) -> Result<Message, GsmL3Error> {
        let cfg = cfg_arg(cfg, "Message::parse_hex (config)")?;
        // A &str never contains interior NULs; CString supplies the trailing NUL
        // and stays alive across this single call.
        let c_hex = CString::new(hex).expect("a Rust str cannot hold interior NULs");
        // SAFETY: c_hex is valid for the duration of this call.
        let p = unsafe { s::gsml3_parse_l3_hex(c_hex.as_ptr(), cfg) };
        if p.is_null() {
            return Err(error::last_error("Message::parse_hex", 0));
        }
        Ok(Message {
            // SAFETY: p is non-null here.
            p: Some(unsafe { NonNull::new_unchecked(p as *mut c_void) }),
            _free: PhantomData,
        })
    }

    /// High-throughput reparse INTO this handle (zero extra allocation for
    /// typical messages). On failure the previous content is KEPT (C contract)
    /// and the typed error is returned — the handle stays valid either way.
    pub fn parse_into(&mut self, data: &[u8]) -> Result<(), GsmL3Error> {
        if data.is_empty() {
            return Err(error::invalid_arg("Message::parse_into", "empty input"));
        }
        let p = match self.p {
            Some(p) => p,
            None => return Err(error::closed("Message::parse_into")),
        };
        // SAFETY: p live; data valid; rc is an error code — on error the C
        // side keeps the previous content of this same handle.
        let rc = unsafe { s::gsml3_parse_l3_into(p.as_ptr(), data.as_ptr(), data.len(), ptr::null()) };
        if rc != s::GSML3_OK {
            return Err(error::last_error("Message::parse_into", rc));
        }
        Ok(())
    }

    /// Message name (e.g. "CMServiceRequest"). Static storage: copied out and
    /// never freed by the caller (ABI).
    pub fn name(&self) -> Result<String, GsmL3Error> {
        let p = match self.p {
            Some(p) => p,
            None => return Err(error::closed("Message::name")),
        };
        // SAFETY: p live; name is static storage (never NULL for a live handle).
        let raw = unsafe { s::gsml3_message_name(p.as_ptr() as *const _) };
        Ok(unsafe { CStr::from_ptr(raw) }.to_string_lossy().into_owned())
    }

    /// Protocol discriminator (`GSML3_PD_*`).
    pub fn pd(&self) -> Result<i32, GsmL3Error> {
        let p = match self.p {
            Some(p) => p,
            None => return Err(error::closed("Message::pd")),
        };
        // SAFETY: p live; pure metadata read.
        Ok(unsafe { s::gsml3_message_pd(p.as_ptr() as *const _) })
    }

    /// Message type identifier (0..255; RR short messages 256..511).
    pub fn mti(&self) -> Result<i32, GsmL3Error> {
        let p = match self.p {
            Some(p) => p,
            None => return Err(error::closed("Message::mti")),
        };
        // SAFETY: p live.
        Ok(unsafe { s::gsml3_message_mti(p.as_ptr() as *const _) })
    }

    /// Transaction identifier for CC/SS messages (0..7), 0 otherwise.
    pub fn ti(&self) -> Result<i32, GsmL3Error> {
        let p = match self.p {
            Some(p) => p,
            None => return Err(error::closed("Message::ti")),
        };
        // SAFETY: p live.
        Ok(unsafe { s::gsml3_message_ti(p.as_ptr() as *const _) })
    }

    /// Exact wire size of the serialized message (zero allocation); a buffer
    /// of this size is guaranteed to be accepted by [`Message::write_to`].
    pub fn size(&self) -> Result<usize, GsmL3Error> {
        let p = match self.p {
            Some(p) => p,
            None => return Err(error::closed("Message::size")),
        };
        // SAFETY: p live.
        Ok(unsafe { s::gsml3_message_size(p.as_ptr() as *const _) })
    }

    /// Serialize into the CALLER's buffer (zero-alloc hot path). Returns bytes
    /// written; 0 means error/buffer-too-small and yields the typed error —
    /// code GSML3_ERR_BUFFER_TOO_SMALL (11) carries the hint: size the buffer
    /// via [`Message::size`]. An empty `out` is rejected before FFI.
    pub fn write_to(&self, out: &mut [u8]) -> Result<usize, GsmL3Error> {
        if out.is_empty() {
            return Err(error::invalid_arg("Message::write_to", "output buffer is empty"));
        }
        let p = match self.p {
            Some(p) => p,
            None => return Err(error::closed("Message::write_to")),
        };
        // SAFETY: p live; out valid for its len bytes during the call.
        let n = unsafe { s::gsml3_message_write(p.as_ptr() as *const _, out.as_mut_ptr(), out.len()) };
        if n == 0 {
            return Err(error::last_error("Message::write_to", 0));
        }
        Ok(n)
    }

    /// Serialize into a freshly allocated `Vec` sized EXACTLY by
    /// [`Message::size`] — the "required size → build in exact buffer"
    /// pattern: no guess-and-grow, no second size probe.
    pub fn to_vec(&self) -> Result<Vec<u8>, GsmL3Error> {
        let n = self.size()?;
        let mut buf = vec![0u8; n];
        let written = self.write_to(&mut buf)?;
        if written != n {
            return Err(error::internal(
                "Message::to_vec",
                "wire size changed between the size probe and the write (must not happen)",
            ));
        }
        Ok(buf)
    }

    /// Hex serialization (lowercase, no spaces). Copy-then-free idiom: the
    /// library-allocated string is read in full and released SYNCHRONOUSLY in
    /// this call — a raw pointer never escapes the wrapper.
    pub fn hex_str(&self) -> Result<String, GsmL3Error> {
        let p = match self.p {
            Some(p) => p,
            None => return Err(error::closed("Message::hex_str")),
        };
        // SAFETY: p live; result is a library-allocated string or NULL on error.
        let raw = unsafe { s::gsml3_message_hex(p.as_ptr() as *const _) };
        if raw.is_null() {
            return Err(error::last_error("Message::hex_str", 0));
        }
        let out = unsafe { CStr::from_ptr(raw) }.to_string_lossy().into_owned();
        // SAFETY: paired release for gsml3_message_hex; NULL-safe. Same call,
        // before any later FFI could clear/replace the error state we copied.
        unsafe { s::gsml3_free(raw as *mut _) };
        Ok(out)
    }

    /// Human-readable dump (message name line + every field). Copy-then-free,
    /// as [`Message::hex_str`].
    pub fn dump(&self) -> Result<String, GsmL3Error> {
        let p = match self.p {
            Some(p) => p,
            None => return Err(error::closed("Message::dump")),
        };
        // SAFETY: p live.
        let raw = unsafe { s::gsml3_message_dump(p.as_ptr() as *const _) };
        if raw.is_null() {
            return Err(error::last_error("Message::dump", 0));
        }
        let out = unsafe { CStr::from_ptr(raw) }.to_string_lossy().into_owned();
        // SAFETY: paired release, same call.
        unsafe { s::gsml3_free(raw as *mut _) };
        Ok(out)
    }
}

impl Drop for Message {
    fn drop(&mut self) {
        if let Some(p) = self.p.take() {
            // SAFETY: exactly one gsml3_message_free per parsed handle.
            unsafe { s::gsml3_message_free(p.as_ptr()) };
        }
    }
}

/// A parsed A-bis RSL frame (TS 48.058). The handle OWNS A COPY OF THE INPUT:
/// the L3/IE views returned below point into that internal copy and stay
/// valid while this value lives — the caller's input buffer can be dropped
/// right after parsing. Released in `Drop`.
pub struct RslFrame {
    p: Option<NonNull<c_void>>,
    _free: PhantomData<fn() -> ()>,
}

impl RslFrame {
    /// Parse an RSL frame from raw bytes (the handle copies the input).
    pub fn parse(data: &[u8]) -> Result<RslFrame, GsmL3Error> {
        if data.is_empty() {
            return Err(error::invalid_arg("RslFrame::parse", "empty input"));
        }
        // SAFETY: valid slice; C returns NULL on error — checked synchronously.
        let p = unsafe { s::gsml3_rsl_parse(data.as_ptr(), data.len()) };
        if p.is_null() {
            return Err(error::last_error("RslFrame::parse", 0));
        }
        Ok(RslFrame {
            // SAFETY: p non-null here.
            p: Some(unsafe { NonNull::new_unchecked(p as *mut c_void) }),
            _free: PhantomData,
        })
    }

    fn ptr(&self, op: &'static str) -> Result<NonNull<c_void>, GsmL3Error> {
        match self.p {
            Some(p) => Ok(p),
            None => Err(error::closed(op)),
        }
    }

    /// Frame name ("DATA_REQ", "CHAN_ACTIV", ...). Static storage: copied.
    pub fn name(&self) -> Result<String, GsmL3Error> {
        let p = self.ptr("RslFrame::name")?;
        // SAFETY: live handle; static storage, never freed by the caller.
        let raw = unsafe { s::gsml3_rsl_name(p.as_ptr() as *const _) };
        Ok(unsafe { CStr::from_ptr(raw) }.to_string_lossy().into_owned())
    }

    /// 7-bit discriminator (direction bit stripped).
    pub fn discriminator(&self) -> Result<i32, GsmL3Error> {
        let p = self.ptr("RslFrame::discriminator")?;
        // SAFETY: live handle.
        Ok(unsafe { s::gsml3_rsl_discriminator(p.as_ptr() as *const _) })
    }

    /// Message type byte within the discriminator.
    pub fn msg_type(&self) -> Result<i32, GsmL3Error> {
        let p = self.ptr("RslFrame::msg_type")?;
        // SAFETY: live handle.
        Ok(unsafe { s::gsml3_rsl_msg_type(p.as_ptr() as *const _) })
    }

    /// Channel number.
    pub fn chan_nr(&self) -> Result<i32, GsmL3Error> {
        let p = self.ptr("RslFrame::chan_nr")?;
        // SAFETY: live handle.
        Ok(unsafe { s::gsml3_rsl_chan_nr(p.as_ptr() as *const _) })
    }

    /// LAPDm link identifier (RLL).
    pub fn link_id(&self) -> Result<i32, GsmL3Error> {
        let p = self.ptr("RslFrame::link_id")?;
        // SAFETY: live handle.
        Ok(unsafe { s::gsml3_rsl_link_id(p.as_ptr() as *const _) })
    }

    /// Direction: `true` = BTS→BSC, `false` = BSC→BTS.
    pub fn bts_to_bsc(&self) -> Result<bool, GsmL3Error> {
        let p = self.ptr("RslFrame::bts_to_bsc")?;
        // SAFETY: live handle.
        Ok(unsafe { s::gsml3_rsl_bts_to_bsc(p.as_ptr() as *const _) } != 0)
    }

    /// Whether the frame carries an L3 payload.
    pub fn has_l3(&self) -> Result<bool, GsmL3Error> {
        let p = self.ptr("RslFrame::has_l3")?;
        // SAFETY: live handle.
        Ok(unsafe { s::gsml3_rsl_has_l3(p.as_ptr() as *const _) } != 0)
    }

    /// The L3 payload as an OWNED copy of the view into the handle's internal
    /// copy; `None` when there is no L3. Copying detaches the result from the
    /// frame (the view itself is valid only while this value is alive).
    pub fn l3(&self) -> Result<Option<Vec<u8>>, GsmL3Error> {
        let p = self.ptr("RslFrame::l3")?;
        // SAFETY: live handle; C sets *len unconditionally and returns NULL when
        // the frame has no L3 payload.
        let mut len: usize = 0;
        let v = unsafe { s::gsml3_rsl_l3(p.as_ptr() as *const _, &mut len) };
        if v.is_null() {
            return Ok(None);
        }
        // SAFETY: view into the handle's own copy — valid for this call.
        let bytes = unsafe { std::slice::from_raw_parts(v, len) }.to_vec();
        Ok(Some(bytes))
    }

    /// Number of parsed information elements.
    pub fn ie_count(&self) -> Result<usize, GsmL3Error> {
        let p = self.ptr("RslFrame::ie_count")?;
        // SAFETY: live handle.
        Ok(unsafe { s::gsml3_rsl_ie_count(p.as_ptr() as *const _) })
    }

    /// The IE at `index` as `(type code, owned value copy)`; `None` when the
    /// index is out of range (C: GSML3_ERR_INVALID_ARG — not a hard failure).
    pub fn ie(&self, index: usize) -> Result<Option<(u8, Vec<u8>)>, GsmL3Error> {
        let p = self.ptr("RslFrame::ie")?;
        // SAFETY: live handle; outputs are locals initialized for the call.
        let (mut ty, mut len, mut val): (u8, usize, *const u8) = (0, 0, ptr::null());
        let rc = unsafe { s::gsml3_rsl_ie_get(p.as_ptr() as *const _, index, &mut ty, &mut len, &mut val) };
        if rc != s::GSML3_OK || val.is_null() {
            return Ok(None);
        }
        let bytes = unsafe { std::slice::from_raw_parts(val, len) }.to_vec();
        Ok(Some((ty, bytes)))
    }
}

impl Drop for RslFrame {
    fn drop(&mut self) {
        if let Some(p) = self.p.take() {
            // SAFETY: exactly one gsml3_rsl_free per parsed frame.
            unsafe { s::gsml3_rsl_free(p.as_ptr()) };
        }
    }
}

// Debug for the owned types (used by test `.unwrap_err()`/`.expect()`): a stable,
// allocation-free "open" flag — never the raw pointer (no leak of ASLR-ish data,
// no misleading address output in test logs).

impl std::fmt::Debug for Config {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("Config").field("open", &self.p.is_some()).finish()
    }
}

impl std::fmt::Debug for Message {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("Message").field("open", &self.p.is_some()).finish()
    }
}

impl std::fmt::Debug for RslFrame {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("RslFrame").field("open", &self.p.is_some()).finish()
    }
}

// ── RSL builders (S4, 13) ───────────────────────────────────────────────────
// All write into the caller's buffer and return bytes written; 0 means error
// or buffer-too-small — surfaced through last_error() with its code
// (BUFFER_TOO_SMALL = 11 included), never silently retried with a bigger
// buffer.

fn build_result(op: &'static str, n: usize) -> Result<usize, GsmL3Error> {
    if n == 0 {
        Err(error::last_error(op, 0))
    } else {
        Ok(n)
    }
}

/// RSL DATA REQUEST (BTS→BSC carrying an L3 payload).
pub fn rsl_build_data_req(out: &mut [u8], chan_nr: u8, link_id: u8, l3: &[u8]) -> Result<usize, GsmL3Error> {
    // SAFETY: out/l3 are valid slices for the duration of this single call.
    let n = unsafe {
        s::gsml3_rsl_build_data_req(out.as_mut_ptr(), out.len(), chan_nr, link_id, l3.as_ptr(), l3.len())
    };
    build_result("rsl_build_data_req", n)
}

/// RSL DATA INDICATION (BSC→BTS carrying an L3 payload).
pub fn rsl_build_data_ind(out: &mut [u8], chan_nr: u8, link_id: u8, l3: &[u8]) -> Result<usize, GsmL3Error> {
    // SAFETY: valid out/l3 slices for this call.
    let n = unsafe {
        s::gsml3_rsl_build_data_ind(out.as_mut_ptr(), out.len(), chan_nr, link_id, l3.as_ptr(), l3.len())
    };
    build_result("rsl_build_data_ind", n)
}

/// RSL UNIT DATA REQUEST (SDCCH unit data, BTS→BSC).
pub fn rsl_build_unit_data_req(out: &mut [u8], chan_nr: u8, link_id: u8, l3: &[u8]) -> Result<usize, GsmL3Error> {
    // SAFETY: valid out/l3 slices for this call.
    let n = unsafe {
        s::gsml3_rsl_build_unit_data_req(out.as_mut_ptr(), out.len(), chan_nr, link_id, l3.as_ptr(), l3.len())
    };
    build_result("rsl_build_unit_data_req", n)
}

/// RSL UNIT DATA INDICATION (SDCCH unit data, BSC→BTS).
pub fn rsl_build_unit_data_ind(out: &mut [u8], chan_nr: u8, link_id: u8, l3: &[u8]) -> Result<usize, GsmL3Error> {
    // SAFETY: valid out/l3 slices for this call.
    let n = unsafe {
        s::gsml3_rsl_build_unit_data_ind(out.as_mut_ptr(), out.len(), chan_nr, link_id, l3.as_ptr(), l3.len())
    };
    build_result("rsl_build_unit_data_ind", n)
}

/// RSL CHANNEL ACTIVATE ACKNOWLEDGE.
pub fn rsl_build_chan_activ_ack(out: &mut [u8], chan_nr: u8, frame_number: u16) -> Result<usize, GsmL3Error> {
    // SAFETY: valid out slice for this call.
    let n = unsafe { s::gsml3_rsl_build_chan_activ_ack(out.as_mut_ptr(), out.len(), chan_nr, frame_number) };
    build_result("rsl_build_chan_activ_ack", n)
}

/// RSL CHANNEL ACTIVATE REJECT (`cause`: RSL error cause, range-checked in C).
pub fn rsl_build_chan_activ_nack(out: &mut [u8], chan_nr: u8, cause: i32) -> Result<usize, GsmL3Error> {
    // SAFETY: valid out slice; an out-of-domain cause yields 0 + INVALID_ARG.
    let n = unsafe { s::gsml3_rsl_build_chan_activ_nack(out.as_mut_ptr(), out.len(), chan_nr, cause) };
    build_result("rsl_build_chan_activ_nack", n)
}

/// RSL RADIO CHANNEL RELEASE ACKNOWLEDGE.
pub fn rsl_build_rf_chan_rel_ack(out: &mut [u8], chan_nr: u8) -> Result<usize, GsmL3Error> {
    // SAFETY: valid out slice for this call.
    let n = unsafe { s::gsml3_rsl_build_rf_chan_rel_ack(out.as_mut_ptr(), out.len(), chan_nr) };
    build_result("rsl_build_rf_chan_rel_ack", n)
}

/// RSL CONNECTION FAILURE (`cause`: RSL error cause, range-checked in C).
pub fn rsl_build_conn_fail(out: &mut [u8], chan_nr: u8, cause: i32) -> Result<usize, GsmL3Error> {
    // SAFETY: valid out slice.
    let n = unsafe { s::gsml3_rsl_build_conn_fail(out.as_mut_ptr(), out.len(), chan_nr, cause) };
    build_result("rsl_build_conn_fail", n)
}

/// RSL MEASUREMENT RESULT (RXLEV/RXQUAL 8-bit signed; optional L1 info).
pub fn rsl_build_meas_res(
    out: &mut [u8],
    chan_nr: u8,
    meas_nr: u8,
    rxlev: i8,
    rxqual: i8,
    l1: &[u8],
) -> Result<usize, GsmL3Error> {
    // SAFETY: valid out/l1 slices for this call.
    let n = unsafe {
        s::gsml3_rsl_build_meas_res(
            out.as_mut_ptr(),
            out.len(),
            chan_nr,
            meas_nr,
            rxlev,
            rxqual,
            l1.as_ptr(),
            l1.len(),
        )
    };
    build_result("rsl_build_meas_res", n)
}

/// RSL HANDOVER DETECTED (access delay, range-checked in C).
pub fn rsl_build_hando_det(out: &mut [u8], chan_nr: u8, access_delay: u8) -> Result<usize, GsmL3Error> {
    // SAFETY: valid out slice for this call.
    let n = unsafe { s::gsml3_rsl_build_hando_det(out.as_mut_ptr(), out.len(), chan_nr, access_delay) };
    build_result("rsl_build_hando_det", n)
}

/// RSL CCCH LOAD INDICATION (16-bit counters).
pub fn rsl_build_ccch_load_ind(
    out: &mut [u8],
    chan_nr: u8,
    paging_load: u16,
    rach_total: u16,
    rach_busy: u16,
    rach_access: u16,
) -> Result<usize, GsmL3Error> {
    // SAFETY: valid out slice for this call.
    let n = unsafe {
        s::gsml3_rsl_build_ccch_load_ind(out.as_mut_ptr(), out.len(), chan_nr, paging_load, rach_total, rach_busy, rach_access)
    };
    build_result("rsl_build_ccch_load_ind", n)
}

/// RSL CHANNEL REQUEST (RACH: request reference + timings u8×4, all
/// range-checked in C against the on-wire field widths).
pub fn rsl_build_chan_rqd(
    out: &mut [u8],
    chan_nr: u8,
    ra: u8,
    t1p: u8,
    t2: u8,
    t3: u8,
    access_delay: u8,
) -> Result<usize, GsmL3Error> {
    // SAFETY: valid out slice for this call.
    let n = unsafe { s::gsml3_rsl_build_chan_rqd(out.as_mut_ptr(), out.len(), chan_nr, ra, t1p, t2, t3, access_delay) };
    build_result("rsl_build_chan_rqd", n)
}

/// RSL DELETE INDICATION (info octets).
pub fn rsl_build_delete_ind(out: &mut [u8], chan_nr: u8, info: &[u8]) -> Result<usize, GsmL3Error> {
    // SAFETY: valid out/info slices for this call.
    let n = unsafe { s::gsml3_rsl_build_delete_ind(out.as_mut_ptr(), out.len(), chan_nr, info.as_ptr(), info.len()) };
    build_result("rsl_build_delete_ind", n)
}

// ── Curated S9 typed builders for the v1 surface (2) ────────────────────────

/// Fixed caller-side buffer for standalone builders: protocol-bounded L3/RSL
/// frames are far below 512 octets, and when one is not, the C side reports
/// BUFFER_TOO_SMALL (0 + code) — surfaced as an error, never guess-and-grown.
const BUILDER_BUF_LEN: usize = 512;

/// Build a CM SERVICE REQUEST L3 message with the C typed builder.
/// `service_type`: `L3CMServiceType::TypeCode` — 1 = MobileOriginatedCall fits
/// the 4-bit wire field (105 = LocationUpdateRequest does NOT fit and will not
/// start a procedure chain); `id_type`: a GSML3_ID_*; `tmsi`: used for TMSI
/// identities; `imsi`: BCD digit string or `None` when absent. Out-of-domain
/// values (including the reserved all-zero TMSI) fail in C with INVALID_ARG.
pub fn build_cm_service_request(
    service_type: i32,
    id_type: i32,
    tmsi: u32,
    imsi: Option<&str>,
) -> Result<Vec<u8>, GsmL3Error> {
    // Rust &str cannot contain interior NULs; CString adds the trailing NUL and
    // must stay alive across the FFI call (it does: local to this scope).
    let mut buf = [0u8; BUILDER_BUF_LEN];
    let c_imsi = imsi.map(|d| CString::new(d).expect("a Rust str cannot hold interior NULs"));
    let imsi_ptr: *const c_char = c_imsi.as_ref().map_or(ptr::null(), |c| c.as_ptr());
    // SAFETY: buf valid for the call; imsi_ptr null or a valid C string.
    let n = unsafe {
        s::gsml3_build_cm_service_request(buf.as_mut_ptr(), buf.len(), service_type, id_type, tmsi, imsi_ptr)
    };
    if n == 0 {
        return Err(error::last_error("build_cm_service_request", 0));
    }
    Ok(buf[..n].to_vec())
}

/// Build a CC SETUP message with the C typed builder (S9): `ti` is the
/// transaction identifier (0..7) and `called_digits` the BCD digit string of
/// the called number (e.g. "123456789"); empty digits are rejected before FFI.
pub fn build_setup(ti: u8, called_digits: &str) -> Result<Vec<u8>, GsmL3Error> {
    if called_digits.is_empty() {
        return Err(error::invalid_arg("build_setup", "called_digits must be a non-empty BCD digit string"));
    }
    let c_digits = CString::new(called_digits).expect("a Rust str cannot hold interior NULs");
    let mut buf = [0u8; BUILDER_BUF_LEN];
    // SAFETY: valid buffer and C string for this single call.
    let n = unsafe { s::gsml3_build_setup(buf.as_mut_ptr(), buf.len(), ti, c_digits.as_ptr()) };
    if n == 0 {
        return Err(error::last_error("build_setup", 0));
    }
    Ok(buf[..n].to_vec())
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::error::ErrorKind;

    /// The stable Channel Release test vector, shared with the C API tests:
    /// 0x60 0x0D 0x00 — PD=RR(0x06), MTI=ChannelRelease(0x0D).
    #[test]
    fn parse_write_roundtrip_channel_release() {
        let mut m = Message::parse(&[0x60, 0x0d, 0x00], None).expect("channel release parses");
        assert_eq!(m.pd().unwrap(), s::GSML3_PD_RR);
        assert_eq!(m.mti().unwrap(), 0x0d);
        assert_eq!(m.size().unwrap(), 3);
        assert_eq!(m.to_vec().unwrap(), vec![0x60, 0x0d, 0x00]);
        assert_eq!(m.hex_str().unwrap(), "600d00"); // lowercase, no spaces
        assert!(!m.dump().unwrap().is_empty());

        // Zero-extra-alloc reparse path; same content after.
        m.parse_into(&[0x60, 0x0d, 0x00]).expect("parse_into keeps the handle");
        assert_eq!(m.to_vec().unwrap(), vec![0x60, 0x0d, 0x00]);

        // Hex input accepts spaces ("60 0D 00") and round-trips byte-for-byte.
        let h = Message::parse_hex("60 0D 00", None).expect("hex parses");
        assert_eq!(h.to_vec().unwrap(), m.to_vec().unwrap());
        drop(m); // Drop path: gsml3_message_free runs exactly once.
    }

    #[test]
    fn error_paths_rejected_before_ffi_or_reported_by_c() {
        // NULL-policy: empty byte input never reaches C.
        let e = Message::parse(&[], None).unwrap_err();
        assert_eq!(e.kind(), Some(ErrorKind::InvalidArg));

        // Truncated frame: "60 0D" is the canonical truncated RR ChannelRelease —
        // code TRUNCATED (2) with the C message copied synchronously.
        let e = Message::parse_hex("60 0D", None).unwrap_err();
        assert_eq!(e.kind(), Some(ErrorKind::Truncated));
        assert!(!e.msg.is_empty());

        // Guard against over-strict truncation (mirrors the C vector in Go):
        // a LONE octet "60" is a COMPLETE 1-octet RR short-form message.
        let m = Message::parse_hex("60", None).expect("lone RR octet must parse");
        assert_eq!(m.name().unwrap(), "ChannelRequest");
        assert_eq!(m.pd().unwrap(), s::GSML3_PD_RR);
        assert_eq!(m.size().unwrap(), 1);

        // Non-hex characters: the C core reports INVALID_VALUE (7) for these.
        let e = Message::parse_hex("zz", None).unwrap_err();
        assert_eq!(e.kind(), Some(ErrorKind::InvalidValue));
        assert!(!e.msg.is_empty());

        // An empty caller buffer is a wrapper-level rejection.
        let m = Message::parse(&[0x60, 0x0d, 0x00], None).unwrap();
        let mut empty: [u8; 0] = [];
        assert_eq!(m.write_to(&mut empty).unwrap_err().kind(), Some(ErrorKind::InvalidArg));

        // Too-small caller buffer: C reports BUFFER_TOO_SMALL (11).
        let mut tiny = [0u8; 2];
        assert_eq!(m.write_to(&mut tiny).unwrap_err().kind(), Some(ErrorKind::BufferTooSmall));

        // parse_setup with empty digits is rejected before FFI.
        assert!(build_setup(3, "").is_err());
    }

    #[test]
    fn config_strict_framing_rejects_trailing_byte() {
        // Mirror of the C `Config` test / Go TestErrorPaths: lenient mode
        // parses "50 84"...
        let cfg = Config::new().expect("config allocates");
        assert!(Message::parse_hex("50 84", Some(&cfg)).is_ok());

        // Strict framing: a message that consumes EXACTLY the input still
        // parses; one with a trailing byte is rejected.
        cfg.set_strict_framing(true).expect("strict on");
        assert!(Message::parse_hex("50 84", Some(&cfg)).is_ok());
        let e = Message::parse(&[0x50, 0x84, 0x00], Some(&cfg)).unwrap_err();
        assert!(!matches!(e.kind(), Some(ErrorKind::Ok)), "strict framing must reject the trailing byte");

        // Out-of-range log levels are ignored per the ABI — no error either way.
        cfg.set_log_level(99).expect("out-of-range level ignored");
        cfg.set_log_level(-5).expect("negative level ignored");
    }
}
