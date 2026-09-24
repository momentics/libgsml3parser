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

//! BTS stack layer: the subscriber [`Registry`] and the BORROWED
//! [`Session<'r>`] view. The registry owns every session on the C side —
//! `gsml3_registry_free` releases them all, so this wrapper has NO free/close
//! call for a session: it stores only the raw pointer and
//! a lifetime bound to its owner's borrow, which the type system turns into
//! "you cannot hold a session across a registry close".

use std::ffi::CString;
use std::marker::PhantomData;
use std::mem::MaybeUninit;
use std::os::raw::c_void;
use std::ptr::NonNull;

use crate::error::{self, GsmL3Error};
use crate::sys as s;

/// Shared tick_timers plumbing (used by both [`Registry::tick_timers`] and the
/// raw-handle path inside `GsmL3Stack`): the caller's buffer needs no
/// zero-init per the C contract — every WRITTEN event is fully initialized and
/// we read exactly `n <= cap` of them.
pub(crate) fn tick_timers_into(reg: NonNull<c_void>, delta_ms: u32, cap: usize) -> Result<Vec<(usize, i32)>, GsmL3Error> {
    if cap == 0 {
        return Ok(Vec::new());
    }
    // The output buffer may be uninitialized (see docs above).
    let mut buf: Vec<MaybeUninit<s::gsml3_timer_expiry>> = vec![MaybeUninit::uninit(); cap];
    // SAFETY: reg is a live gsml3_registry*; buf provides `cap` slots; C writes
    // at most n fully-initialized events and never retains the pointer.
    let n = unsafe { s::gsml3_registry_tick_timers(reg.as_ptr(), delta_ms, buf.as_mut_ptr().cast(), cap) };
    if n > cap {
        // Cannot happen per the ABI; refuse to over-read the buffer anyway.
        return Err(error::internal("Registry::tick_timers", "C wrote more timer expiries than the buffer capacity"));
    }
    let mut out = Vec::with_capacity(n as usize);
    for slot in buf.iter().take(n as usize) {
        // SAFETY: slots 0..n were fully written by the C call just made.
        let ev = unsafe { slot.assume_init_read() };
        out.push((ev.session as usize, ev.timer_id));
    }
    Ok(out)
}

/// A registry of subscriber sessions. `shards`: 0 = plain single-threaded
/// registry (event-loop model), 4/8/16/32 = sharded thread-safe registry
/// (per-shard locks make its registry-mediated calls thread-safe per the ABI).
/// Owns its `gsml3_registry` handle; released in [`Registry::close`] / `Drop`
/// (take-pattern, exactly once), which also frees every session on the C side.
pub struct Registry {
    p: Option<NonNull<c_void>>,
    shards: u8,
}

impl Registry {
    /// Create a registry. The shard count is validated BEFORE the FFI boundary
    /// (NULL-policy); the C side re-checks as a backstop. Any other value is an
    /// `INVALID_ARG` error without any C call.
    pub fn new(shard_count: i32) -> Result<Registry, GsmL3Error> {
        if !matches!(shard_count, 0 | 4 | 8 | 16 | 32) {
            return Err(error::invalid_arg(
                "Registry::new",
                "shard_count must be 0 (plain) or one of 4, 8, 16, 32 (sharded)",
            ));
        }
        // SAFETY: stateless constructor; NULL on invalid shard count / OOM — checked.
        let p = unsafe { s::gsml3_registry_new(shard_count) };
        if p.is_null() {
            return Err(error::last_error("Registry::new", 0));
        }
        Ok(Registry {
            // SAFETY: p is non-null here.
            p: Some(unsafe { NonNull::new_unchecked(p as *mut c_void) }),
            shards: shard_count as u8,
        })
    }

    /// The configured shard count (0 for a plain registry).
    pub fn shards(&self) -> u32 {
        self.shards as u32
    }

    /// Idempotent explicit teardown (same single path as `Drop`). After it the
    /// C registry and ALL its sessions are gone — any session borrow obtained
    /// from it cannot be live here, by the type system.
    pub fn close(&mut self) {
        if let Some(p) = self.p.take() {
            // SAFETY: exactly one gsml3_registry_free per created registry.
            unsafe { s::gsml3_registry_free(p.as_ptr()) };
        }
    }

    /// The live handle, or a closed-class error (no FFI after release).
    fn ptr(&self, op: &'static str) -> Result<NonNull<c_void>, GsmL3Error> {
        match self.p {
            Some(p) => Ok(p),
            None => Err(error::closed(op)),
        }
    }

    /// Pre-size the indexes for the expected population (cold path).
    pub fn reserve(&self, expected: usize) -> Result<(), GsmL3Error> {
        let p = self.ptr("Registry::reserve")?;
        // SAFETY: p live; cold-path pre-allocation hint.
        unsafe { s::gsml3_registry_reserve(p.as_ptr(), expected) };
        Ok(())
    }

    /// Number of active sessions.
    pub fn count(&self) -> Result<usize, GsmL3Error> {
        let p = self.ptr("Registry::count")?;
        // SAFETY: p live; pure read.
        Ok(unsafe { s::gsml3_registry_count(p.as_ptr() as *const _) })
    }

    /// Create a session keyed by TMSI (borrowed view tied to this registry's
    /// borrow). C validation passes through: the reserved all-zero TMSI is an
    /// `INVALID_ARG` with a non-empty message; a taken key is `DUPLICATE`; an
    /// allocation failure is `NO_MEMORY`.
    pub fn create_by_tmsi(&self, tmsi: u32) -> Result<Session<'_>, GsmL3Error> {
        let p = self.ptr("Registry::create_by_tmsi")?;
        // SAFETY: p live; NULL means a documented validation failure.
        let sp = unsafe { s::gsml3_registry_create_by_tmsi(p.as_ptr(), tmsi) };
        if sp.is_null() {
            return Err(error::last_error("Registry::create_by_tmsi", 0));
        }
        Ok(Session {
            // SAFETY: sp non-null here.
            ptr: unsafe { NonNull::new_unchecked(sp as *mut c_void) },
            _reg: PhantomData,
        })
    }

    /// Create a session keyed by IMSI (1-15 ASCII digits); the auto-assigned
    /// TMSI is readable via [`Session::assigned_tmsi`]. Plain registries only —
    /// sharded ones reject this with `UNSUPPORTED` per the C API. Empty digits
    /// are rejected before FFI (NULL-policy).
    pub fn create_by_imsi(&self, imsi: &str) -> Result<Session<'_>, GsmL3Error> {
        if imsi.is_empty() || imsi.len() > 15 {
            return Err(error::invalid_arg(
                "Registry::create_by_imsi",
                "imsi must be 1..=15 ASCII digits",
            ));
        }
        let p = self.ptr("Registry::create_by_imsi")?;
        // A &str never contains interior NULs; CString supplies the trailing NUL
        // C requires and stays alive across this single call (the C side copies).
        let c_imsi = CString::new(imsi).expect("a Rust str cannot hold interior NULs");
        // SAFETY: valid C string for this call.
        let sp = unsafe { s::gsml3_registry_create_by_imsi(p.as_ptr(), c_imsi.as_ptr()) };
        if sp.is_null() {
            return Err(error::last_error("Registry::create_by_imsi", 0));
        }
        Ok(Session {
            // SAFETY: sp non-null here.
            ptr: unsafe { NonNull::new_unchecked(sp as *mut c_void) },
            _reg: PhantomData,
        })
    }

    /// Look up a session by TMSI; `None` when not found (a miss, not an error).
    pub fn find_by_tmsi(&self, tmsi: u32) -> Result<Option<Session<'_>>, GsmL3Error> {
        let p = self.ptr("Registry::find_by_tmsi")?;
        // SAFETY: p live.
        let sp = unsafe { s::gsml3_registry_find_by_tmsi(p.as_ptr(), tmsi) };
        if sp.is_null() {
            return Ok(None);
        }
        Ok(Some(Session {
            // SAFETY: sp non-null here.
            ptr: unsafe { NonNull::new_unchecked(sp as *mut c_void) },
            _reg: PhantomData,
        }))
    }

    /// Look up a session by IMSI digits; `None` when not found.
    pub fn find_by_imsi(&self, imsi: &str) -> Result<Option<Session<'_>>, GsmL3Error> {
        let p = self.ptr("Registry::find_by_imsi")?;
        let c_imsi = CString::new(imsi).expect("a Rust str cannot hold interior NULs");
        // SAFETY: valid C string for this call.
        let sp = unsafe { s::gsml3_registry_find_by_imsi(p.as_ptr(), c_imsi.as_ptr()) };
        if sp.is_null() {
            return Ok(None);
        }
        Ok(Some(Session {
            // SAFETY: sp non-null here.
            ptr: unsafe { NonNull::new_unchecked(sp as *mut c_void) },
            _reg: PhantomData,
        }))
    }

    /// Look up the session assigned to the channel `(trx, ts, lapdm_link)`;
    /// `None` when not found (the ARFCN is stored with the session but is not
    /// part of this key).
    pub fn find_by_link(
        &self,
        trx_number: u8,
        timeslot: u8,
        lapdm_link: u8,
    ) -> Result<Option<Session<'_>>, GsmL3Error> {
        let p = self.ptr("Registry::find_by_link")?;
        // SAFETY: p live.
        let sp = unsafe { s::gsml3_registry_find_by_link(p.as_ptr(), trx_number, timeslot, lapdm_link) };
        if sp.is_null() {
            return Ok(None);
        }
        Ok(Some(Session {
            // SAFETY: sp non-null here.
            ptr: unsafe { NonNull::new_unchecked(sp as *mut c_void) },
            _reg: PhantomData,
        }))
    }

    /// Remove a session. `true` = removed, `false` = not found / unowned
    /// (C: no separate error form).
    pub fn remove(&self, s: &Session<'_>) -> Result<bool, GsmL3Error> {
        let p = self.ptr("Registry::remove")?;
        // SAFETY: p live and the session borrow proves this registry owns it.
        let r = unsafe { s::gsml3_registry_remove(p.as_ptr(), s.ptr.as_ptr()) };
        Ok(r != 0)
    }

    /// Remove all sessions. Plain registries only; a sharded registry makes
    /// this a no-op and the C-side `UNSUPPORTED` code is surfaced as an error.
    pub fn clear(&self) -> Result<(), GsmL3Error> {
        let p = self.ptr("Registry::clear")?;
        // SAFETY: p live; errors (if any) arrive via thread-local state — read now.
        unsafe { s::gsml3_registry_clear(p.as_ptr()) };
        error::void_result("Registry::clear")
    }

    /// Assign a channel to `s` and update the link index. Ownership is
    /// enforced in C: for a session that belongs to another registry (or none)
    /// the call is a no-op and surfaces `INVALID_ARG` through this error.
    /// `ch_type`: gsml3parser::ChannelType value (range-checked in C).
    pub fn assign_channel(
        &self,
        s: &Session<'_>,
        ch_type: i32,
        trx: u8,
        ts: u8,
        arfcn: u16,
        lapdm_link: u8,
    ) -> Result<(), GsmL3Error> {
        let p = self.ptr("Registry::assign_channel")?;
        // SAFETY: p live; ownership rules enforced in C and read below.
        unsafe { s::gsml3_registry_assign_channel(p.as_ptr(), s.ptr.as_ptr(), ch_type, trx, ts, arfcn, lapdm_link) };
        error::void_result("Registry::assign_channel")
    }

    /// Release the channel of `s` and remove it from the link index. Same
    /// ownership rules (and error path) as [`Registry::assign_channel`].
    pub fn release_channel(&self, s: &Session<'_>) -> Result<(), GsmL3Error> {
        let p = self.ptr("Registry::release_channel")?;
        // SAFETY: p live.
        unsafe { s::gsml3_registry_release_channel(p.as_ptr(), s.ptr.as_ptr()) };
        error::void_result("Registry::release_channel")
    }

    /// Tick all session timers by `delta_ms`; returns the expiry events that
    /// fit into a buffer of `cap` as `(raw session pointer, timer id)` — the
    /// pointer is BORROWED (never free it; validate through the registry). The
    /// C side does not need zero-initialization (every written event is fully
    /// initialized) and events that do not fit are re-armed on 1 ms and arrive
    /// on a later tick — never dropped.
    pub fn tick_timers(&self, delta_ms: u32, cap: usize) -> Result<Vec<(usize, i32)>, GsmL3Error> {
        let p = self.ptr("Registry::tick_timers")?;
        tick_timers_into(p, delta_ms, cap)
    }

    /// Tick all active session procedures by `delta_ms`; returns the number of
    /// timeouts (registry level — distinct from the orchestrator's chain tick).
    pub fn tick_procedures(&self, delta_ms: u32) -> Result<usize, GsmL3Error> {
        let p = self.ptr("Registry::tick_procedures")?;
        // SAFETY: p live.
        Ok(unsafe { s::gsml3_registry_tick_procedures(p.as_ptr(), delta_ms) })
    }
}

impl Drop for Registry {
    fn drop(&mut self) {
        self.close(); // take-pattern: no double free, no-op if close() ran
    }
}

impl std::fmt::Debug for Registry {
    /// Stable test/debug rendering: shard count + open flag (never the raw pointer).
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("Registry")
            .field("open", &self.p.is_some())
            .field("shards", &self.shards)
            .finish()
    }
}

/// A BORROWED view of one subscriber session. It is NEVER freed by this type —
/// its owning registry frees all sessions (C ABI); the lifetime `'r` binds the
/// view to a live `&Registry`, and closing the registry takes `&mut`, so a
/// session borrow cannot straddle a close (the wrapper has no free function at
/// all, mirroring the C surface). Accessors check nothing here: validity is a
/// property of the borrow. Equality is pointer equality (same session == same
/// raw pointer) — enough for test assertions and `remove()` checks.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Session<'r> {
    ptr: NonNull<c_void>,
    _reg: PhantomData<&'r ()>,
}

impl Session<'_> {
    /// The TMSI of the session identity (0 when the identity is an IMSI).
    pub fn tmsi(&self) -> u32 {
        // SAFETY: the borrow proves the owning registry — and hence this
        // borrowed pointer — is still alive.
        unsafe { s::gsml3_session_tmsi(self.ptr.as_ptr()) }
    }

    /// The TMSI under which this session is keyed in its registry (the
    /// auto-assigned one for IMSI sessions); 0 when the session does not
    /// belong to a registry.
    pub fn assigned_tmsi(&self) -> u32 {
        // SAFETY: as [`Session::tmsi`].
        unsafe { s::gsml3_session_assigned_tmsi(self.ptr.as_ptr()) }
    }

    /// Change the session's identity TMSI. NOTE (C contract): this does NOT
    /// re-index the registry — remove + create re-key the session.
    pub fn set_tmsi(&self, tmsi: u32) {
        // SAFETY: as [`Session::tmsi`]; setters are no-ops on NULL in C anyway.
        unsafe { s::gsml3_session_set_tmsi(self.ptr.as_ptr(), tmsi) }
    }

    /// Set the IMSI (BCD digit string).
    pub fn set_imsi(&self, digits: &str) {
        let c_digits = CString::new(digits).expect("a Rust str cannot hold interior NULs");
        // SAFETY: valid C string for this call.
        unsafe { s::gsml3_session_set_imsi(self.ptr.as_ptr(), c_digits.as_ptr()) }
    }

    /// Whether the subscriber is registered in the home location register.
    pub fn is_registered(&self) -> bool {
        // SAFETY: as [`Session::tmsi`].
        let v = unsafe { s::gsml3_session_is_registered(self.ptr.as_ptr()) };
        v != 0
    }

    pub fn set_registered(&self, v: bool) {
        // SAFETY: as above.
        unsafe { s::gsml3_session_set_registered(self.ptr.as_ptr(), if v { 1 } else { 0 }) }
    }

    /// Whether the subscriber has passed authentication.
    pub fn is_authenticated(&self) -> bool {
        // SAFETY: as above.
        let v = unsafe { s::gsml3_session_is_authenticated(self.ptr.as_ptr()) };
        v != 0
    }

    pub fn set_authenticated(&self, v: bool) {
        // SAFETY: as above.
        unsafe { s::gsml3_session_set_authenticated(self.ptr.as_ptr(), if v { 1 } else { 0 }) }
    }

    /// Whether ciphering is enabled on the channel.
    pub fn is_ciphered(&self) -> bool {
        // SAFETY: as above.
        let v = unsafe { s::gsml3_session_is_ciphered(self.ptr.as_ptr()) };
        v != 0
    }

    pub fn set_ciphered(&self, v: bool) {
        // SAFETY: as above.
        unsafe { s::gsml3_session_set_ciphered(self.ptr.as_ptr(), if v { 1 } else { 0 }) }
    }

    /// Start or restart an L3 timer (GSML3_TIMER_*). Returns `true` on a fresh
    /// start, `false` on a restart. An out-of-range ID fails with the C-side
    /// code (`INVALID_VALUE` family) and a message — surfaced here, not as 0.
    pub fn timer_start(&self, timer_id: i32) -> Result<bool, GsmL3Error> {
        // SAFETY: as [`Session::tmsi`]; rc + thread-local state read synchronously.
        let r = unsafe { s::gsml3_session_timer_start(self.ptr.as_ptr(), timer_id) };
        error::void_result("Session::timer_start")?;
        Ok(r != 0)
    }

    pub fn timer_stop(&self, timer_id: i32) {
        // SAFETY: as above; no documented failure path.
        unsafe { s::gsml3_session_timer_stop(self.ptr.as_ptr(), timer_id) }
    }

    /// Whether the given timer is currently running.
    pub fn timer_running(&self, timer_id: i32) -> bool {
        // SAFETY: as above.
        let v = unsafe { s::gsml3_session_timer_running(self.ptr.as_ptr(), timer_id) };
        v != 0
    }

    /// Number of pending transactions.
    pub fn transaction_pending(&self) -> usize {
        // SAFETY: as above.
        unsafe { s::gsml3_session_transaction_pending(self.ptr.as_ptr()) }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn registry_validates_shard_count_before_ffi() {
        // NULL-policy: 3 is not a valid shard count — rejected without any C call.
        let e = Registry::new(3).unwrap_err();
        assert_eq!(e.kind(), Some(crate::error::ErrorKind::InvalidArg));
        // The raw C backstop agrees: gsml3_registry_new(3) == NULL.
        // SAFETY: null-result observation only, no dereference.
        assert!(unsafe { s::gsml3_registry_new(3) }.is_null());
        // Every documented flavor constructs and reports its count.
        for shards in [0u8, 4, 8, 16, 32] {
            let mut r = Registry::new(shards as i32).unwrap();
            assert_eq!(r.shards() as u8, shards);
            r.close(); // idempotent teardown; the later Drop is a no-op.
        }
    }

    #[test]
    fn session_lifecycle_plain_registry() {
        let mut r = Registry::new(0).unwrap();
        assert_eq!(r.count().unwrap(), 0);

        // All-zero TMSI: C validation passes through with a message (NULL-policy test vector).
        let e = r.create_by_tmsi(0).unwrap_err();
        assert_eq!(e.kind(), Some(crate::error::ErrorKind::InvalidArg));
        assert!(!e.msg.is_empty());

        let s = r.create_by_tmsi(0x8765_4321).unwrap();
        assert_eq!(s.tmsi(), 0x8765_4321);
        assert_eq!(s.assigned_tmsi(), 0x8765_4321); // TMSI-keyed: assigned == identity

        // Duplicate key → DUPLICATE (13).
        let e = r.create_by_tmsi(0x8765_4321).unwrap_err();
        assert_eq!(e.kind(), Some(crate::error::ErrorKind::Duplicate));

        assert_eq!(r.count().unwrap(), 1);
        assert!(r.find_by_tmsi(0x8765_4321).unwrap().is_some());
        assert!(r.find_by_tmsi(0xDEAD_BEEF).unwrap().is_none());

        // Identity setters (set_tmsi does NOT re-index — documented).
        s.set_registered(true);
        s.set_authenticated(true);
        s.set_ciphered(false);
        s.set_imsi("244051234567890");
        assert!(s.is_registered());
        assert!(s.is_authenticated());
        assert!(!s.is_ciphered());

        // Timers: fresh start / running / restart semantics.
        assert!(s.timer_start(s::GSML3_TIMER_T3101).unwrap(), "first start is fresh");
        assert!(s.timer_running(s::GSML3_TIMER_T3101));
        assert!(!s.timer_start(s::GSML3_TIMER_T3101).unwrap(), "a restart is reported as not-fresh, not an error");
        s.timer_stop(s::GSML3_TIMER_T3101);
        assert!(!s.timer_running(s::GSML3_TIMER_T3101));
        // Out-of-range timer id → C error, not a silent false.
        let e = s.timer_start(999).unwrap_err();
        assert_ne!(e.kind(), Some(crate::error::ErrorKind::Ok));

        assert_eq!(s.transaction_pending(), 0);
        assert!(r.remove(&s).unwrap());
        assert!(!r.remove(&s).unwrap()); // already gone
        assert_eq!(r.count().unwrap(), 0);
        r.close();
    }

    #[test]
    fn imsi_sessions_plain_only_and_clear_rules() {
        // Plain: create by IMSI works and auto-assigns a non-zero TMSI.
        let mut r = Registry::new(0).unwrap();
        let s = r.create_by_imsi("244051234567890").unwrap();
        assert!(s.assigned_tmsi() != 0);
        assert_eq!(r.find_by_imsi("244051234567890").unwrap(), Some(s));
        // find_by_tmsi with the ASSIGNED tmsi finds the same session.
        assert!(r.find_by_tmsi(s.assigned_tmsi()).unwrap().is_some());
        assert_eq!(r.tick_timers(10, 4).unwrap().len(), 0); // no running timers → no expiries
        assert!(r.clear().is_ok()); // plain: allowed
        assert_eq!(r.count().unwrap(), 0);
        r.close();

        // Sharded: IMSI creation is UNSUPPORTED per the C API...
        let mut sh = Registry::new(8).unwrap();
        let e = sh.create_by_imsi("244051234567890").unwrap_err();
        assert_eq!(e.kind(), Some(crate::error::ErrorKind::Unsupported));
        // ...and clear() is a no-op + UNSUPPORTED as well.
        let e = sh.clear().unwrap_err();
        assert_eq!(e.kind(), Some(crate::error::ErrorKind::Unsupported));
        // TMSI sessions still work on sharded registries (smoke).
        for i in 0u32..10 {
            sh.create_by_tmsi(0x7000_0000 | i).unwrap();
        }
        assert_eq!(sh.count().unwrap(), 10);
        assert!(sh.find_by_tmsi(0x7000_0005).unwrap().is_some());
        sh.close();

        // Empty digits are rejected before FFI (NULL-policy).
        let mut r = Registry::new(0).unwrap();
        assert!(r.create_by_imsi("").is_err());
        assert!(r.create_by_imsi(&"1".repeat(16)).is_err());
        r.close();
    }
}
