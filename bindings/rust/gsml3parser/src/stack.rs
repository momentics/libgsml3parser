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

//! The BTS-side reference stack: `GsmL3Stack` composes exactly the C handles it
//! created — registry + borrowed session + orchestrator + LAPDm entity — and
//! hosts the static trampolines that implement the queue-model callback
//! delivery.
//!
//! ## Unified `send_frame` semantics (Python / Go / Rust — identical)
//! 1. reset the transmit collector for THIS input;
//! 2. FFI `entity.receive(frame)` — C callbacks ONLY append owned copies to
//!    the entity queues (no FFI inside a callback);
//! 3. IF `auto_response`: drain the L3 queue POST-FFI and, for every event
//!    carrying a payload: parse L3 → `orchestrator.feed(session)`; when the
//!    step carries a token ≠ NONE: `required_size` → exact-size `build_response`
//!    → `entity.send_ui(sapi, resp)` (new transmit frames land in the
//!    collector); one failing event is recorded in [`GsmL3Stack::last_event_error`]
//!    and NEVER aborts the batch;
//! 4. IF NOT `auto_response`: L3 events stay unprocessed in the queue — take
//!    them with [`GsmL3Stack::drain_l3_events`] and orchestrate manually via
//!    [`GsmL3Stack::feed_l3`] + [`GsmL3Stack::build_response`] +
//!    [`GsmL3Stack::send_ui`];
//! 5. return the transmit frames produced by THIS input (C-FSM L2-level
//!    reactions plus any auto-responses) as owned copies.

use std::cell::{Cell, RefCell};
use std::ffi::{CStr, CString};
use std::os::raw::{c_int, c_void};
use std::ptr::{self, NonNull};
use std::sync::atomic::Ordering;
use std::sync::Arc;

use crate::error::{self, GsmL3Error};
use crate::lapdm::{LapdmEntity, TrampolineState};
use crate::message::Message;
use crate::registry::tick_timers_into;
use crate::sys as s;

/// One L3 interlayer delivery captured by the entity's L3 trampoline.
/// `data` is an OWNED copy: the C span it was read from lived only during the
/// synchronous callback (ABI), while this value is what the queue model hands
/// to post-FFI processing — exactly the mirror of Python `L3Event` and Go
/// `L3Event`.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct L3Event {
    /// SAPI the event arrived on.
    pub sapi: i32,
    /// Interlayer primitive (GSML3_PRIM_*).
    pub primitive: i32,
    /// Owned copy of the L3 payload; empty for link-state primitives.
    pub data: Vec<u8>,
}

// ── Static C trampolines (the queue model) ────────────────────────────────────
//
// SAFETY contract of BOTH trampolines: `user` is the
// heap address of a `Box<Arc<TrampolineState>>` created in `LapdmEntity::new`
// and reclaimed — exactly once, with `Box::from_raw` — strictly AFTER
// `gsml3_lapdm_entity_free`, by `LapdmEntity::close_once` (lapdm.rs). Inside a
// trampoline we therefore only: dereference the token WITHOUT transferring
// ownership (the borrow is valid for this call's duration — no close can race,
// one thread per handle), clone the `Arc` (refcount bump released on return),
// check the alive flag (closed entity ⇒ late callbacks become no-ops), read
// the C span into an owned copy, and push under the `Mutex`. NO gsml3_*
// function is ever called here: spans are valid during the callback only, and
// re-entering this same live entity mid-receive is forbidden by the ABI.

pub(crate) unsafe extern "C" fn l3_trampoline(
    sapi: c_int,
    primitive: c_int,
    l3: *const u8,
    len: usize,
    user: *mut c_void,
) {
    if user.is_null() {
        // Defensive: per construction C always passes the live token; null
        // would mean a use-after-close — drop the event rather than trap.
        return;
    }
        // SAFETY: see the module-level contract above (the box is live for every
        // callback; close reclaims it only after the entity handle is gone).
        let state = Arc::clone(&*(user as *const Box<Arc<TrampolineState>>));
        if !state.alive.load(Ordering::Relaxed) {
            return; // stack closed: late callback — ignore (belt behind free-ordering)
        }
    let data = if l3.is_null() || len == 0 {
        Vec::new()
    } else {
        // SAFETY: the span is valid for this callback only (ABI); we make
        // exactly one owned copy that leaves the callback — zero-copy read.
        std::slice::from_raw_parts(l3, len).to_vec()
    };
    state.enqueue_l3(L3Event {
        sapi,
        primitive,
        data,
    });
}

pub(crate) unsafe extern "C" fn l1_trampoline(frame: *const u8, frame_len: usize, user: *mut c_void) {
    if user.is_null() {
        return; // defensive: drop on impossible token loss
    }
    // SAFETY: see the l3_trampoline contract.
    let state = Arc::clone(&*(user as *const Box<Arc<TrampolineState>>));
    if !state.alive.load(Ordering::Relaxed) {
        return; // late callback after close
    }
    let copy = if frame.is_null() || frame_len == 0 {
        Vec::new()
    } else {
        // SAFETY: span valid for this callback only; one owned copy leaves it.
        std::slice::from_raw_parts(frame, frame_len).to_vec()
    };
    state.enqueue_tx(copy);
}

// ── Orchestrator step (safe by-value wrapper of gsml3_step_result) ──────────

/// The result of one orchestrator step — a SAFE copy of `gsml3_step_result`:
/// `reason` is a synchronously taken String copy of the thread-local C text
/// (NULL → ""). The copy happens INSIDE the feed wrapper, at the call site:
/// every later gsml3_* call may clear or replace the thread-local storage, so
/// holding the raw pointer past the wrapper would be UB-by-use.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct StepResult {
    /// GSML3_ACTION_* (CONTINUE=0 / SEND_RESPONSE=1 / WAITING_EXTERNAL=2 / COMPLETED=3 / FAILED=4)
    pub action: i32,
    /// Pending response token, GSML3_TOKEN_* (GSML3_TOKEN_NONE=0 when none)
    pub token: i32,
    /// GSML3_STATE_* of the procedure after this step
    pub final_state: i32,
    /// GSML3_PROC_* chain type after this step
    pub final_type: i32,
    /// Reason text copied from thread-local storage (empty string when NULL)
    pub reason: String,
    /// gsml3_error code; GSML3_OK on a real step — on error the fields above
    /// carry no step information (C contract).
    pub code: i32,
}

/// Convert one by-value C step result into the safe type. When `error` is not
/// GSML3_OK the step fields are meaningless — the thread-local message is read
/// SYNCHRONOUSLY here, before any further FFI call could clear it.
fn step_from_raw(op: &'static str, raw: s::gsml3_step_result) -> Result<StepResult, GsmL3Error> {
    if raw.error != s::GSML3_OK {
        return Err(error::last_error(op, raw.error));
    }
    // SAFETY: raw.reason is a NUL-terminated thread-local copy (NULL when
    // empty) — valid now; copying it immediately is exactly the C contract.
    let reason = if raw.reason.is_null() {
        String::new()
    } else {
        unsafe { CStr::from_ptr(raw.reason).to_string_lossy().into_owned() }
    };
    Ok(StepResult {
        action: raw.action,
        token: raw.response_token,
        final_state: raw.final_state,
        final_type: raw.final_type,
        reason,
        code: raw.error,
    })
}

// ── GsmL3Stack ───────────────────────────────────────────────────────────────

/// BTS-side reference stack over the C core. Owns exactly the three handles it
/// created (registry, orchestrator, LAPDm entity) — the session pointer is
/// BORROWED and never freed here: `gsml3_registry_free` releases all sessions
/// along with the registry (C ABI). The entity's `TrampolineState` (`void* user`
/// token) lives inside `ent` and is reclaimed there, after `entity_free`.
///
/// Creation order (new): registry → session (create_by_tmsi/imsi) → orchestrator
/// → entity (+ trampoline context) → open(sapi, command_bit=1). Any failing step
/// rolls the earlier handles back in REVERSE order. Teardown order
/// ([`GsmL3Stack::close`] / `Drop`, one take-pattern path): entity (alive off →
/// free → context reclamation) → orchestrator → registry.
pub struct GsmL3Stack {
    /// C registry handle; taken to None by close() after gsml3_registry_free.
    reg: Option<NonNull<c_void>>,
    /// C orchestrator handle; taken to None by close() after gsml3_orchestrator_free.
    orch: Option<NonNull<c_void>>,
    /// BORROWED session pointer — never freed directly (the registry owns it);
    /// nulled by close(); accessors check `closed` before touching it.
    session: *mut c_void,
    /// The LAPDm entity; OWNS the trampoline context (`Box::into_raw` token).
    ent: LapdmEntity,
    /// Address-octet SAPI used for auto/manual send_ui transmissions.
    sapi: i32,
    /// Queue-model switch: true → send_frame drives parse→feed→build→send_ui
    /// after each receive (default demo behavior); false → L3 events stay in
    /// the queue for drain_l3_events/feed_l3 manual orchestration.
    auto_response: bool,
    /// Closed flag: set by close(); every public method checks it FIRST and
    /// returns a closed-class error without FFI (NULL-policy).
    closed: Cell<bool>,
    /// Last processed step (set by send_frame in auto mode / feed_l3); mirrors
    /// Python `last_step` and Go `LastStep` so the demo chain can print tokens.
    last_step: RefCell<Option<StepResult>>,
    /// Parse/feed failure of the LAST drained event (unified semantics: one
    /// failing event never aborts a frame's processing; mirrors Python/Go).
    last_event_error: RefCell<Option<GsmL3Error>>,
}

// SAFETY for `Send`: the C ABI is ONE THREAD PER OWNED HANDLE. Moving the whole
// stack to another thread is exactly "ownership transfer" — safe, because no
// other thread holds a reference (that would require `Sync`). `Sync` is
// therefore INTENTIONALLY NOT implemented: sharing `&GsmL3Stack` across
// threads would permit concurrent calls on one live FSM/entity without any
// synchronization, which the ABI forbids — the same single-thread-per-stack
// contract the Go and Python bindings document.
unsafe impl Send for GsmL3Stack {}

/// Identity key of the stack's session — internal construction detail.
enum Key<'a> {
    /// Session keyed by an explicit TMSI (must be non-zero).
    Tmsi(u32),
    /// Session keyed by IMSI digits (plain registries only).
    Imsi(&'a str),
}

impl GsmL3Stack {
    /// Build the BTS-side stack for a TMSI-keyed session.
    ///
    /// - `tmsi`: subscriber TMSI — the reserved all-zero value is rejected
    ///   BEFORE any FFI call (NULL-policy; the C side would report
    ///   INVALID_ARG anyway);
    /// - `shard_count`: 0 = plain single-thread registry, else 4/8/16/32 =
    ///   sharded (validated before FFI; the C side re-checks);
    /// - `sapi`: link SAPI 0..15; `profile`: 0=SDCCH / 1=SACCH / 2=FACCH;
    /// - `auto_response`: REQUIRED parameter (Rust has no default arguments) —
    ///   mirrors Go `StackOptions{AutoResponse}` and Python
    ///   `GsmL3Stack(auto_response=...)`; without it the manual
    ///   drain_l3_events()/feed_l3() mode would be unreachable.
    pub fn new(
        tmsi: u32,
        shard_count: u8,
        sapi: i32,
        profile: i32,
        auto_response: bool,
    ) -> Result<Self, GsmL3Error> {
        if tmsi == 0 {
            return Err(error::invalid_arg(
                "GsmL3Stack::new",
                "tmsi must be non-zero (the all-zero TMSI is reserved per TS 24.008)",
            ));
        }
        Self::create(Key::Tmsi(tmsi), shard_count, sapi, profile, auto_response)
    }

    /// Build the BTS-side stack for an IMSI-keyed session (the C core
    /// auto-assigns a TMSI; read it with [`GsmL3Stack::assigned_tmsi`]).
    /// `shard_count` MUST be 0: sharded registries reject IMSI creation with
    /// UNSUPPORTED per the C API — rejected here, before FFI.
    pub fn new_with_imsi(
        imsi: &str,
        shard_count: u8,
        sapi: i32,
        profile: i32,
        auto_response: bool,
    ) -> Result<Self, GsmL3Error> {
        if imsi.is_empty() || imsi.len() > 15 {
            return Err(error::invalid_arg(
                "GsmL3Stack::new_with_imsi",
                "imsi must be 1..=15 ASCII digits",
            ));
        }
        if shard_count != 0 {
            return Err(error::invalid_arg(
                "GsmL3Stack::new_with_imsi",
                "IMSI sessions require a plain registry (shard_count == 0): sharded creation is UNSUPPORTED by the C API",
            ));
        }
        Self::create(Key::Imsi(imsi), shard_count, sapi, profile, auto_response)
    }

    fn create(
        key: Key<'_>,
        shard_count: u8,
        sapi: i32,
        profile: i32,
        auto_response: bool,
    ) -> Result<Self, GsmL3Error> {
        // All input is validated in Rust BEFORE any FFI (NULL-policy); the C
        // side re-checks as a backstop on every path below.
        if !matches!(shard_count, 0 | 4 | 8 | 16 | 32) {
            return Err(error::invalid_arg(
                "GsmL3Stack::new",
                "shard_count must be 0 (plain) or one of 4, 8, 16, 32 (sharded)",
            ));
        }
        if !(0..=15).contains(&sapi) {
            // The temporary String lives to the end of this statement (&str borrow is valid here).
            return Err(error::invalid_arg(
                "GsmL3Stack::new",
                &format!("sapi must be in 0..=15 (got {sapi})"),
            ));
        }
        if !(0..=2).contains(&profile) {
            return Err(error::invalid_arg(
                "GsmL3Stack::new",
                "profile must be 0 (SDCCH), 1 (SACCH) or 2 (FACCH)",
            ));
        }

        // 1) registry
        // SAFETY: stateless constructor; shards pre-validated above.
        let reg = unsafe { s::gsml3_registry_new(shard_count as i32) };
        if reg.is_null() {
            return Err(error::last_error("GsmL3Stack::new", 0)); // NO_MEMORY in practice
        }
        // 2) session (borrowed from the registry) — INVALID_ARG/NO_MEMORY/DUPLICATE surface via NULL + thread-local.
        let session = match key {
            Key::Tmsi(tmsi) => {
                // SAFETY: live registry just created.
                unsafe { s::gsml3_registry_create_by_tmsi(reg, tmsi) }
            }
            Key::Imsi(imsi) => {
                let c_imsi = CString::new(imsi).expect("a Rust str cannot hold interior NULs");
                // SAFETY: live registry; c_imsi is valid for this call (the C side copies).
                unsafe { s::gsml3_registry_create_by_imsi(reg, c_imsi.as_ptr()) }
            }
        };
        if session.is_null() {
            // Roll back in reverse order (only the registry exists so far).
            // SAFETY: free what we just created.
            unsafe { s::gsml3_registry_free(reg) };
            return Err(error::last_error("GsmL3Stack::new", 0));
        }
        // 3) orchestrator
        // SAFETY: stateless constructor.
        let orch = unsafe { s::gsml3_orchestrator_new() };
        if orch.is_null() {
            // SAFETY: roll back the registry (session goes with it — never freed directly).
            unsafe { s::gsml3_registry_free(reg) };
            return Err(error::last_error("GsmL3Stack::new", 0));
        }
        // 4) entity (owns the Box::into_raw trampoline context internally)
        let ent = LapdmEntity::new(profile).map_err(|e| {
            // Roll back orchestrator → registry (reverse order); session freed by the registry.
            unsafe {
                s::gsml3_orchestrator_free(orch);
                s::gsml3_registry_free(reg);
            }
            e
        })?;
        // 5) open the link on the BTS side (command_bit = 1 → C/R=1).
        if let Err(e) = ent.open(sapi, 1) {
            // Roll back entity → orchestrator → registry in reverse.
            drop(ent); // frees entity handle AND reclaims its trampoline context once
            // SAFETY: roll back what we just created (session freed with the registry).
            unsafe {
                s::gsml3_orchestrator_free(orch);
                s::gsml3_registry_free(reg);
            }
            return Err(e);
        }

        Ok(Self {
            // SAFETY: reg/orch were just null-checked above.
            reg: Some(unsafe { NonNull::new_unchecked(reg as *mut c_void) }),
            orch: Some(unsafe { NonNull::new_unchecked(orch as *mut c_void) }),
            session,
            ent,
            sapi,
            auto_response,
            closed: Cell::new(false),
            last_step: RefCell::new(None),
            last_event_error: RefCell::new(None),
        })
    }

    /// Every public method begins here: after close() every raw handle is null
    /// and the borrowed session pointer must not be touched (NULL-policy — a
    /// freed C handle has no "closed" state, so refusal is binding-side).
    fn ensure_open(&self, op: &'static str) -> Result<(), GsmL3Error> {
        if self.closed.get() {
            return Err(error::closed(op));
        }
        Ok(())
    }

    /// The live orchestrator handle. Call AFTER `ensure_open`: with the closed
    /// flag clear the take-pattern guarantees every handle is still Some
    /// (close(&mut self) excludes any in-flight &self method, single-thread).
    fn orch_handle(&self, op: &'static str) -> Result<NonNull<c_void>, GsmL3Error> {
        self.orch.ok_or_else(|| error::closed(op))
    }

    /// Feed one parsed message into the chain and convert the by-value step
    /// result — the `reason` thread-local copy is taken INSIDE this function,
    /// before any later FFI call could clear it. The session may legally be
    /// NULL at C level; here it always points at the live borrowed session.
    fn orch_feed(&self, m: &Message) -> Result<StepResult, GsmL3Error> {
        let o = self.orch_handle("GsmL3Stack::feed")?;
        let msg = m.c_handle("GsmL3Stack::feed (message)")?;
        // SAFETY: live orchestrator and message handles; session is a valid
        // borrow while the stack is open (checked by every public caller).
        let raw = unsafe { s::gsml3_orchestrator_feed(o.as_ptr(), msg.as_ptr() as *const _, self.session) };
        step_from_raw("GsmL3Stack::feed", raw)
    }

    // ── The unified send path ────────────────────────────────────────────

    /// Feed one raw LAPDm frame into the C entity and drive the chain — the
    /// unified semantics of the crate docs (identical to Python
    /// `send_frame` / Go `SendFrame`). A frame shorter than 2 bytes
    /// (address + control) is rejected BEFORE FFI. Returned transmit frames
    /// cover exactly THIS input (C-FSM reactions + auto-responses).
    pub fn send_frame(&self, frame: &[u8]) -> Result<Vec<Vec<u8>>, GsmL3Error> {
        self.ensure_open("GsmL3Stack::send_frame")?;
        if frame.len() < 2 {
            return Err(error::invalid_arg(
                "GsmL3Stack::send_frame",
                "a LAPDm frame needs at least address + control bytes (2)",
            ));
        }
        self.ent.reset_tx(); // (1) fresh collector for this input
        self.ent.receive(frame)?; // (2) C callbacks only enqueue — no re-entry

        if self.auto_response {
            // (3) POST-FFI drain: never re-enter a live FSM mid-receive.
            for ev in self.ent.drain_l3() {
                if ev.data.is_empty() {
                    continue; // link-state primitives (ESTABLISH_CONFIRM etc.) carry no payload
                }
                let step = Message::parse(&ev.data, None).and_then(|m| self.orch_feed(&m));
                match step {
                    Ok(step) => {
                        self.last_step.borrow_mut().replace(step.clone());
                        if step.token != s::GSML3_TOKEN_NONE {
                            // required_size → exact-size build (no guess-and-grow), then send_ui.
                            let sent = self.build_response().and_then(|resp| self.ent.send_ui(self.sapi, &resp));
                            if let Err(e) = sent {
                                // One failing event never aborts the batch (unified
                                // semantics; Python records + continues, Go too).
                                *self.last_event_error.borrow_mut() = Some(e);
                            }
                        }
                    }
                    Err(e) => {
                        *self.last_event_error.borrow_mut() = Some(e);
                    }
                }
            }
        }
        // (5) what THIS input produced (owned copies). In manual mode the L3
        // events above were NOT consumed — they stay in the queue.
        Ok(self.ent.drain_tx())
    }

    /// Direct orchestration WITHOUT L2 (test/simulation hook; also the manual
    /// path of auto_response=false): parse + feed(session). The response is NOT
    /// sent automatically — build it with [`GsmL3Stack::build_response`] and
    /// transmit it with [`GsmL3Stack::send_ui`]. Empty input rejected before FFI.
    pub fn feed_l3(&self, l3: &[u8]) -> Result<StepResult, GsmL3Error> {
        self.ensure_open("GsmL3Stack::feed_l3")?;
        if l3.is_empty() {
            return Err(error::invalid_arg("GsmL3Stack::feed_l3", "l3 payload is empty"));
        }
        let m = Message::parse(l3, None)?;
        let step = self.orch_feed(&m)?;
        self.last_step.borrow_mut().replace(step.clone());
        Ok(step)
    }

    /// Build the PENDING response (last token): `required_size` → exact-size
    /// buffer → build (no guess-and-grow). Returns an error
    /// with code INVALID_VALUE when nothing is pending — never an empty vec.
    pub fn build_response(&self) -> Result<Vec<u8>, GsmL3Error> {
        self.ensure_open("GsmL3Stack::build_response")?;
        let o = self.orch_handle("GsmL3Stack::build_response")?;
        // SAFETY: live orchestrator + session (valid while open).
        let n = unsafe { s::gsml3_orchestrator_required_size(o.as_ptr(), self.session as *const _) };
        if n == 0 {
            // No pending response / NULL session / missing parameter — the code
            // is in the thread-local state; copy it NOW (INVALID_VALUE family).
            return Err(error::last_error("GsmL3Stack::build_response", 0));
        }
        let mut buf = vec![0u8; n];
        // SAFETY: exact-size buffer of the C-reported size.
        let written = unsafe { s::gsml3_orchestrator_build_response(o.as_ptr(), self.session as *const _, buf.as_mut_ptr(), buf.len()) };
        if written == 0 {
            return Err(error::last_error("GsmL3Stack::build_response", 0));
        }
        if written != n {
            return Err(error::internal(
                "GsmL3Stack::build_response",
                "C wrote fewer bytes than gsml3_orchestrator_required_size reported (must not happen)",
            ));
        }
        Ok(buf)
    }

    /// Transmit one L3 payload as a UI frame manually (auto_response=false mode):
    /// returns the transmit frames captured for this transmission. `sapi`
    /// defaults to the stack's address SAPI when None.
    pub fn send_ui(&self, l3: &[u8], sapi: Option<i32>) -> Result<Vec<Vec<u8>>, GsmL3Error> {
        self.ensure_open("GsmL3Stack::send_ui")?;
        self.ent.reset_tx();
        self.ent.send_ui(sapi.unwrap_or(self.sapi), l3)?;
        Ok(self.ent.drain_tx())
    }

    /// Take-and-empty the unprocessed L3 event queue (manual mode; in auto
    /// mode events are consumed inside `send_frame`). No FFI.
    pub fn drain_l3_events(&self) -> Result<Vec<L3Event>, GsmL3Error> {
        self.ensure_open("GsmL3Stack::drain_l3_events")?;
        Ok(self.ent.drain_l3())
    }

    /// Take-and-empty the pending transmit-frame queue. No FFI.
    pub fn drain_tx_frames(&self) -> Result<Vec<Vec<u8>>, GsmL3Error> {
        self.ensure_open("GsmL3Stack::drain_tx_frames")?;
        Ok(self.ent.drain_tx())
    }

    // ── Orchestrator pass-throughs ───────────────────────────────────────

    /// Tick the orchestrator CHAIN timers by `delta_ms`; returns the number of
    /// procedure failures (0 inside a window). Distinct from
    /// `Registry::tick_procedures` (session-procedure level) — e.g. T3101 = 3 s
    /// on MO call setup lives in the chain, so the demo's [3] uses this one.
    pub fn tick_procedures(&self, delta_ms: u32) -> Result<usize, GsmL3Error> {
        self.ensure_open("GsmL3Stack::tick_procedures")?;
        let o = self.orch_handle("GsmL3Stack::tick_procedures")?;
        // SAFETY: live orchestrator.
        Ok(unsafe { s::gsml3_orchestrator_tick(o.as_ptr(), delta_ms) })
    }

    /// Drain the retransmission channel (consume-on-read); GSML3_TOKEN_*.
    /// Returns TOKEN_NONE when nothing is queued; after a non-NONE token,
    /// build and send that response (build_response + send_ui).
    pub fn take_retransmit(&self) -> Result<i32, GsmL3Error> {
        self.ensure_open("GsmL3Stack::take_retransmit")?;
        let o = self.orch_handle("GsmL3Stack::take_retransmit")?;
        // SAFETY: live orchestrator.
        Ok(unsafe { s::gsml3_orchestrator_take_retransmit(o.as_ptr()) })
    }

    /// Current chain phase (GSML3_PROC_*; UNKNOWN=0xFF when idle).
    pub fn chain_phase(&self) -> Result<i32, GsmL3Error> {
        self.ensure_open("GsmL3Stack::chain_phase")?;
        let o = self.orch_handle("GsmL3Stack::chain_phase")?;
        // SAFETY: live orchestrator.
        Ok(unsafe { s::gsml3_orchestrator_chain_phase(o.as_ptr()) })
    }

    // ── Entity pass-throughs ─────────────────────────────────────────────

    /// Current LAPDm FSM state of the entity (GSML3_LAPDM_STATE_*).
    pub fn state(&self) -> Result<i32, GsmL3Error> {
        self.ensure_open("GsmL3Stack::state")?;
        self.ent.state()
    }

    /// True when the entity link is established.
    pub fn established(&self) -> Result<bool, GsmL3Error> {
        self.ensure_open("GsmL3Stack::established")?;
        self.ent.is_established()
    }

    /// Advance the entity's T200 by `elapsed_ms`: 1 = a retransmission or
    /// abnormal release happened, 0 = none (the C internal-error form becomes
    /// an Err with the thread-local message).
    pub fn tick_t200(&self, elapsed_ms: u32) -> Result<i32, GsmL3Error> {
        self.ensure_open("GsmL3Stack::tick_t200")?;
        self.ent.tick_t200(elapsed_ms)
    }

    // ── Session (borrowed) pass-throughs ─────────────────────────────────

    /// The session identity TMSI (0 for IMSI-keyed sessions).
    pub fn session_tmsi(&self) -> Result<u32, GsmL3Error> {
        self.ensure_open("GsmL3Stack::session_tmsi")?;
        // SAFETY: open ⇒ the borrowed session is live (its registry is ours and alive).
        Ok(unsafe { s::gsml3_session_tmsi(self.session) })
    }

    /// The TMSI the session is keyed under (the auto-assigned one for IMSI keys).
    pub fn assigned_tmsi(&self) -> Result<u32, GsmL3Error> {
        self.ensure_open("GsmL3Stack::assigned_tmsi")?;
        // SAFETY: as session_tmsi.
        Ok(unsafe { s::gsml3_session_assigned_tmsi(self.session) })
    }

    /// Set the session's IMSI (BCD digit string).
    pub fn set_imsi(&self, digits: &str) -> Result<(), GsmL3Error> {
        self.ensure_open("GsmL3Stack::set_imsi")?;
        let c_digits = CString::new(digits).expect("a Rust str cannot hold interior NULs");
        // SAFETY: live session; c_digits is valid for this single call.
        unsafe { s::gsml3_session_set_imsi(self.session, c_digits.as_ptr()) };
        Ok(())
    }

    /// Whether the subscriber is registered in the home location register.
    pub fn registered(&self) -> Result<bool, GsmL3Error> {
        self.ensure_open("GsmL3Stack::registered")?;
        // SAFETY: live session.
        Ok(unsafe { s::gsml3_session_is_registered(self.session) } != 0)
    }

    /// Whether the subscriber has passed authentication.
    pub fn authenticated(&self) -> Result<bool, GsmL3Error> {
        self.ensure_open("GsmL3Stack::authenticated")?;
        // SAFETY: live session.
        Ok(unsafe { s::gsml3_session_is_authenticated(self.session) } != 0)
    }

    /// Whether ciphering is enabled.
    pub fn ciphered(&self) -> Result<bool, GsmL3Error> {
        self.ensure_open("GsmL3Stack::ciphered")?;
        // SAFETY: live session.
        Ok(unsafe { s::gsml3_session_is_ciphered(self.session) } != 0)
    }

    // ── Registry (session timers) pass-throughs ──────────────────────────

    /// SESSION-level timer expiries (registry pass-through) as
    /// `(raw borrowed session pointer, GSML3_TIMER_*)`; the pointer is never
    /// freed by this binding. Distinct from `tick_procedures` (chain timers).
    pub fn tick_timers(&self, delta_ms: u32, cap: usize) -> Result<Vec<(usize, i32)>, GsmL3Error> {
        self.ensure_open("GsmL3Stack::tick_timers")?;
        let r = match self.reg {
            Some(r) => r,
            None => return Err(error::closed("GsmL3Stack::tick_timers")),
        };
        tick_timers_into(r, delta_ms, cap)
    }

    // ── Step/event bookkeeping ───────────────────────────────────────────

    /// The step result of the most recently PROCESSED L3 event (auto mode:
    /// inside `send_frame`; manual: from `feed_l3`). `None` when nothing has
    /// been processed yet or after close. Mirrors Python `last_step` / Go
    /// `LastStep`. No FFI.
    pub fn last_step(&self) -> Option<StepResult> {
        if self.closed.get() {
            return None;
        }
        self.last_step.borrow().clone()
    }

    /// The parse/feed failure recorded by the LAST `send_frame` in auto mode
    /// (unified "record, never abort" semantics), or `None`. No FFI.
    pub fn last_event_error(&self) -> Option<GsmL3Error> {
        if self.closed.get() {
            return None;
        }
        self.last_event_error.borrow().clone()
    }

    // ── Teardown ─────────────────────────────────────────────────────────

    /// Idempotent teardown — the SINGLE code path for explicit close and
    /// `Drop` (take-pattern: every handle is TAKEN before being freed, so a
    /// second close()/drop is a no-op without UB). Release order mandated by
    /// the C ABI (stop callbacks BEFORE freeing what they may reference):
    /// 1) entity — alive=false → gsml3_lapdm_entity_free → Box::from_raw of
    ///    its trampoline state (reclamation exactly once, per entity);
    /// 2) gsml3_orchestrator_free;
    /// 3) gsml3_registry_free — frees the borrowed session along with all
    ///    others (the session pointer is NEVER passed to a free function).
    ///
    /// After close: every method returns a closed-class error without FFI.
    pub fn close(&mut self) {
        if self.closed.get() {
            return; // idempotent — nothing left to release
        }
        self.ent.close_once(); // 1) entity + its trampoline context
        if let Some(o) = self.orch.take() {
            // 2) SAFETY: exactly one orchestrator_free per created handle.
            unsafe { s::gsml3_orchestrator_free(o.as_ptr()) };
        }
        if let Some(r) = self.reg.take() {
            // 3) SAFETY: exactly one registry_free per created registry; all
            // sessions (including our borrowed one) die with it on the C side.
            unsafe { s::gsml3_registry_free(r.as_ptr()) };
        }
        self.session = ptr::null_mut();
        *self.last_step.borrow_mut() = None;
        *self.last_event_error.borrow_mut() = None;
        self.closed.set(true);
    }
}

impl Drop for GsmL3Stack {
    fn drop(&mut self) {
        self.close(); // explicit close already ran? → pure no-op (take-pattern)
    }
}

impl std::fmt::Debug for GsmL3Stack {
    /// Stable test/debug rendering (never the raw pointers).
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("GsmL3Stack")
            .field("closed", &self.closed.get())
            .field("sapi", &self.sapi)
            .field("auto_response", &self.auto_response)
            .finish()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::error::ErrorKind;

    /// Unit check: double close via close(&mut self) + a later Drop
    /// — the second and third passes are no-ops (no UB, no double free), every
    /// method afterwards reports closed WITHOUT FFI (take-pattern: the handles
    /// were nulled before the flag went up).
    #[test]
    fn test_close_idempotent() {
        let mut st = GsmL3Stack::new(0x8765_4321, 0, 0, 0, true).expect("BTS-side stack");
        assert!(!st.closed.get());
        st.close();
        assert!(st.closed.get());
        st.close(); // second explicit close — pure no-op
        // The closed path performs NO FFI: every method refuses with the class.
        assert!(st.send_frame(&[0x01, 0x03, 0x60]).err().unwrap().is_closed());
        assert!(st.build_response().err().unwrap().is_closed());
        assert!(st.tick_procedures(10).err().unwrap().is_closed());
        assert!(st.last_step().is_none()); // bookkeeping cleared with the handles
        drop(st); // Drop after close — third pass, no panic / no double free
    }

    /// Unit check: release ORDER is entity → orchestrator →
    /// registry. After the entity's close_once its trampoline state is already
    /// reclaimed (single Box::from_raw), and the session pointer is only nulled
    /// — it is never passed to any free function.
    #[test]
    fn test_release_order() {
        use crate::lapdm::lapdm_handles_released;

        // Entity level: close reclaims context immediately after entity_free.
        let mut e = LapdmEntity::new(0).expect("entity");
        e.open(0, 1).expect("open");
        e.close_once();
        assert!(lapdm_handles_released(&e), "close must take both the handle and its context");

        // Stack level: full teardown — assert the end state of every take.
        let mut st = GsmL3Stack::new(0x2A2A_2A2A, 0, 0, 0, false).expect("stack");
        let session_before = st.session;
        assert!(!session_before.is_null()); // open stack carries its borrowed session
        st.close();
        assert!(lapdm_handles_released(&st.ent)); // (1) entity + trampoline context freed
        assert!(st.orch.is_none()); // (2) orchestrator freed
        assert!(st.reg.is_none()); // (3) registry freed last — sessions die with it in C
        assert!(st.session.is_null()); // borrowed pointer nulled, NEVER freed directly
        st.close(); // idempotent
        drop(st); // Drop after close: no-op
    }

    /// Constructors validate their input BEFORE the FFI boundary (NULL-policy):
    /// reserved TMSI / bad shard count / bad sapi / bad profile / empty or
    /// sharded IMSI keys all fail with INVALID_ARG and no C call.
    #[test]
    fn test_new_validates_before_ffi() {
        assert_eq!(
            GsmL3Stack::new(0, 0, 0, 0, true).unwrap_err().kind(),
            Some(ErrorKind::InvalidArg)
        );
        assert_eq!(
            GsmL3Stack::new(1, 3, 0, 0, true).unwrap_err().kind(),
            Some(ErrorKind::InvalidArg) // shard_count 3
        );
        assert!(GsmL3Stack::new(1, 0, 16, 0, true).is_err()); // sapi out of range
        assert!(GsmL3Stack::new(1, 0, 0, 5, true).is_err()); // profile out of range
        assert!(GsmL3Stack::new_with_imsi("", 0, 0, 0, true).is_err()); // empty IMSI
        assert!(GsmL3Stack::new_with_imsi("244051234567890", 4, 0, 0, true).is_err()); // sharded + IMSI
    }

    /// The trampoline path itself (unit half of the queue model): C callbacks
    /// deliver owned copies into the entity queues; drains take-and-empty.
    #[test]
    fn test_trampoline_queue_model() {
        let e = LapdmEntity::new(0).expect("entity");
        e.open(0, 1).expect("open as BTS side");
        e.send_sabme().expect("sabme from LinkReleased");
        // L1 trampoline captured the transmit frame: BTS SABME is [0x09, 0x2F].
        assert_eq!(e.drain_tx(), vec![vec![0x09, 0x2F]]);

        e.receive(&crate::lapdm::mini::ui(0, false, &[0x60, 0x0d, 0x00]).unwrap()).expect("UI receive");
        // L3 trampoline captured the delivery with an owned payload copy.
        let evs = e.drain_l3();
        assert_eq!(evs.len(), 1);
        assert_eq!(evs[0].primitive, s::GSML3_PRIM_L3_UNIT_DATA);
        assert_eq!(evs[0].data, vec![0x60, 0x0d, 0x00]);

        e.hard_release().expect("hard release");
        drop(e); // Drop reclaims the entity + context once (debug-asserted)
    }

    /// Manual mode: auto_response=false keeps L3 events in the queue and lets
    /// feed_l3/build_response/send_ui drive the chain explicitly.
    #[test]
    fn test_manual_mode_feed_build_send() {
        let mut st = GsmL3Stack::new(0x8765_4321, 0, 0, 0, false).expect("stack, manual mode");

        let l3 = crate::build_cm_service_request(1, s::GSML3_ID_TMSI, 0x8765_4321, None).unwrap();
        let ui = crate::lapdm::mini::ui(0, false, &l3).unwrap();
        st.send_frame(&ui).expect("receive only; no auto response in manual mode");

        // The event stays unprocessed until we drain it.
        let evs = st.drain_l3_events().expect("drain");
        assert_eq!(evs.len(), 1);
        assert_eq!(evs[0].primitive, s::GSML3_PRIM_L3_UNIT_DATA);

        let step = st.feed_l3(&evs[0].data).expect("manual orchestration");
        assert_eq!(step.token, s::GSML3_TOKEN_CM_SERVICE_ACCEPT);
        assert_eq!(step.action, s::GSML3_ACTION_SEND_RESPONSE);

        let resp = st.build_response().expect("pending CMServiceAccept response");
        assert!(!resp.is_empty());
        let txs = st.send_ui(&resp, None).expect("manual transmission");
        assert_eq!(txs.len(), 1); // BTS-side UI captured by the L1 trampoline
        st.close(); // explicit teardown (session goes with the registry)
    }
}
