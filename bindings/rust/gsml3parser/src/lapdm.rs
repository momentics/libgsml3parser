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

//! The LAPDm layer: zero-copy frame decoding ([`decode_frame`] /
//! [`FrameInfo`]) and the safe wrapper of the C FSM entity
//! ([`LapdmEntity`]) with its trampoline context (`TrampolineState`).
//!
//! Queue model (identical in shape to Python `_c_cb_keepalive` and
//! Go `eventSink`): ONE `TrampolineState` object per entity, heap-allocated as
//! `Box<Arc<...>>` and handed to C as the `void* user` token via
//! `Box::into_raw`. The static trampolines (defined in [`crate::stack`]) run
//! INSIDE synchronous C calls: they clone the `Arc`, check the alive flag, and
//! append an OWNED copy of the span to a `Mutex`-guarded queue — memory-only
//! work. No trampoline ever calls back into any `gsml3_*` function (the ABI:
//! spans are valid only during the callback; re-entry through the same entity
//! is forbidden). The context is reclaimed exactly once with
//! [`std::box_from_raw`] in [`LapdmEntity::close_once`], strictly AFTER
//! `gsml3_lapdm_entity_free` — from that instant C can never invoke the
//! trampoline again.

use std::collections::VecDeque;
use std::mem::MaybeUninit;
use std::os::raw::c_void;
use std::ptr::NonNull;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc, Mutex, PoisonError};

use crate::error::{self, GsmL3Error};
use crate::stack::{l1_trampoline, l3_trampoline, L3Event};
use crate::sys as s;

/// Shared trampoline state for ONE entity's callback pair: the alive guard plus
/// the two queues that hold owned copies of every callback payload. The object
/// lives on the heap behind a `Box` whose raw address is C's `user` token —
/// see the module docs for the single-`from_raw` reclamation contract.
pub struct TrampolineState {
    /// Set to `false` by the owning [`LapdmEntity::close_once`] BEFORE the
    /// entity handle is freed: from then on every late callback is a no-op, so
    /// nothing can touch this state after its owner went away (belt and braces
    /// behind the free-order guarantee).
    pub alive: AtomicBool,
    /// L3 events (sapi + primitive + owned payload copy); appended by the L3
    /// trampoline, drained by the wrapper AFTER a C call returned.
    l3q: Mutex<VecDeque<L3Event>>,
    /// L1 transmit frames (owned copies); appended by the L1 trampoline.
    txq: Mutex<VecDeque<Vec<u8>>>,
}

impl TrampolineState {
    fn new() -> Arc<Self> {
        Arc::new(Self {
            alive: AtomicBool::new(true),
            l3q: Mutex::new(VecDeque::new()),
            txq: Mutex::new(VecDeque::new()),
        })
    }

    /// Take-and-empty the L3 event queue (post-FFI drain — no re-entry).
    fn drain_l3(&self) -> Vec<L3Event> {
        let mut q = self.l3q.lock().unwrap_or_else(PoisonError::into_inner);
        q.drain(..).collect()
    }

    /// Take-and-empty the transmit-frame queue.
    fn drain_tx(&self) -> Vec<Vec<u8>> {
        let mut q = self.txq.lock().unwrap_or_else(PoisonError::into_inner);
        q.drain(..).collect()
    }

    /// Drop all queued transmit frames (collector reset for one send_frame).
    fn clear_tx(&self) {
        self.txq.lock().unwrap_or_else(PoisonError::into_inner).clear();
    }

    /// Append one owned L3 event — trampoline-side push (crate-visible; the
    /// trampolines in `crate::stack` are the only producers, each holding a
    /// fresh `Arc` clone for the call's duration only).
    pub(crate) fn enqueue_l3(&self, ev: L3Event) {
        self.l3q.lock().unwrap_or_else(PoisonError::into_inner).push_back(ev);
    }

    /// Append one owned transmit-frame copy (L1 trampoline side; see above).
    pub(crate) fn enqueue_tx(&self, frame: Vec<u8>) {
        self.txq.lock().unwrap_or_else(PoisonError::into_inner).push_back(frame);
    }
}

/// A safe LAPDm entity: owns its C `gsml3_lapdm_entity` handle AND the heap
/// trampoline context (the `void* user` token). Release order inside
/// [`LapdmEntity::close_once`] is mandated by the C ABI:
/// alive-flag off → `gsml3_lapdm_entity_free` → `Box::from_raw(context)` —
/// each handle is TAKEN before freeing, so a second close or a `Drop` after an
/// explicit close is a no-op without UB.
pub struct LapdmEntity {
    /// C entity handle; taken to `None` by close_once (single free).
    p: Option<NonNull<c_void>>,
    /// The heap `Box<Arc<TrampolineState>>` token C holds as `user`;
    /// taken and reclaimed EXACTLY once, after the entity is gone. The double
    /// allocation is the fixed token shape required by the trampoline cast
    /// (`user` → `*const Box<Arc<TrampolineState>>`), not an accident —
    /// hence the targeted lint allow below.
    #[allow(clippy::redundant_allocation)]
    ctx: Option<NonNull<Box<Arc<TrampolineState>>>>,
}

// SAFETY: the C ABI is one thread per owned handle — the whole entity (its
// queue mutexes included) may MOVE to another thread but must never be SHARED:
// concurrent calls on one FSM violate the contract. `Send` is implemented
// deliberately, `Sync` intentionally not.
unsafe impl Send for LapdmEntity {}

impl LapdmEntity {
    /// Create an entity with profile 0 = SDCCH / 1 = SACCH / 2 = FACCH and
    /// register this binding's trampolines as its L3/L1 callbacks (the token is
    /// the fresh `Box::into_raw(Box::new(Arc::new(TrampolineState)))`). The
    /// profile is validated before FFI (NULL-policy). If creation fails, the
    /// context is reclaimed immediately — nothing leaks on either side.
    pub fn new(profile: i32) -> Result<Self, GsmL3Error> {
        if !(0..=2).contains(&profile) {
            return Err(error::invalid_arg(
                "LapdmEntity::new",
                "profile must be 0 (SDCCH), 1 (SACCH) or 2 (FACCH)",
            ));
        }
        // Token layout: C's `void* user` is
        // the address of a heap box whose CONTENT is `Box<Arc<TrampolineState>>`
        // — exactly what the trampolines cast it to. One extra pointer
        // indirection by construction (allocation #1: the Arc slot, allocation
        // #2: the token slot pointing at it); reclamation is the single
        // matching `Box::from_raw` in close_once below.
        let state = TrampolineState::new(); // Arc<TrampolineState>
        let inner = Box::new(state); // heap #1 holds the Arc
        let ctx = unsafe { NonNull::new_unchecked(Box::into_raw(Box::new(inner))) }; // *mut Box<Arc<T>> (heap #2)
        // SAFETY: l3/l1_trampoline satisfy the callback contract (memory-only,
        // no gsml3_* re-entry — see their SAFETY docs in crate::stack); the ctx
        // token outlives every possible callback (close_once ordering).
        let p = unsafe { s::gsml3_lapdm_entity_new(profile, Some(l3_trampoline), Some(l1_trampoline), ctx.as_ptr().cast()) };
        if p.is_null() {
            // Entity was NOT created — C can never call back: reclaim the context now.
            // SAFETY: single Box::from_raw for this token; no trampoline ran yet.
            drop(unsafe { Box::from_raw(ctx.as_ptr()) });
            return Err(error::last_error("LapdmEntity::new", 0));
        }
        Ok(Self {
            // SAFETY: p is non-null here.
            p: Some(unsafe { NonNull::new_unchecked(p as *mut c_void) }),
            ctx: Some(ctx),
        })
    }

    /// Idempotent teardown — the single code path for explicit close and
    /// `Drop`. Release order (mandated by the C ABI; stop the callbacks BEFORE
    /// freeing anything they could reference):
    /// 1) `alive = false` — any late C callback becomes a no-op;
    /// 2) `gsml3_lapdm_entity_free` and null the handle (no further callback
    ///    can occur from that instant, per the synchronous-invocation contract);
    /// 3) reclaim the trampoline context with `Box::from_raw` — exactly once.
    pub(crate) fn close_once(&mut self) {
        if let Some(ctx) = self.ctx.as_ref() {
            // SAFETY: the box is alive until this call takes it (see struct docs);
            // the explicit &* is intentional — it lives for exactly this store.
            unsafe { (&*(ctx.as_ptr())).alive.store(false, Ordering::Release) };
        }
        if let Some(p) = self.p.take() {
            // SAFETY: exactly one entity_free per created entity.
            unsafe { s::gsml3_lapdm_entity_free(p.as_ptr()) };
        }
        if let Some(ctx) = self.ctx.take() {
            #[cfg(debug_assertions)]
            {
                // The reclamation must be the SOLE Arc owner left: every trampoline
                // clone is stack-local and dropped when the callback returns. A
                // strong count > 1 here would mean a leaked (or racing) owner —
                // exactly the UB the take-ordering exists to prevent.
                // SAFETY: ctx is live until the from_raw below; &Box<Arc> derefs to &Arc.
                debug_assert_eq!(unsafe { Arc::strong_count(&*ctx.as_ptr()) }, 1, "trampoline context must be reclaimed by exactly one owner");
            }
            // SAFETY: created via Box::into_raw in new() (or kept after a failed
            // new() where it is reclaimed immediately); the entity is already
            // freed, so no callback can dereference this pointer from here on.
            drop(unsafe { Box::from_raw(ctx.as_ptr()) });
        }
    }

    /// Explicit idempotent close (same path as `Drop`; a second call and any
    /// later drop are no-ops — nothing is double-freed, take-pattern).
    pub fn close(&mut self) {
        self.close_once();
    }

    /// The live C handle, or a closed-class error — every FFI-facing method
    /// starts here (NULL-policy: no FFI through a freed pointer).
    fn ptr(&self, op: &'static str) -> Result<NonNull<c_void>, GsmL3Error> {
        match self.p {
            Some(p) => Ok(p),
            None => Err(error::closed(op)),
        }
    }

    /// Open the entity (transition to LinkReleased). `sapi` 0..15 selects the
    /// link's SAPI; `command_bit` 1 = BTS side (C/R=1), 0 = MS side. An invalid
    /// sapi sets the thread-local error in C and leaves the previous state —
    /// surfaced as this error.
    pub fn open(&self, sapi: i32, command_bit: i32) -> Result<(), GsmL3Error> {
        let p = self.ptr("LapdmEntity::open")?;
        // SAFETY: live entity; the void call reports failures via thread-local state.
        unsafe { s::gsml3_lapdm_entity_open(p.as_ptr(), sapi, command_bit) };
        error::void_result("LapdmEntity::open")
    }

    /// Feed one raw LAPDm frame from L1 into the FSM. Callbacks (if any) fire
    /// SYNCHRONOUSLY inside this call and only append to the entity's queues —
    /// process them with [`LapdmEntity::drain_l3`] / [`LapdmEntity::drain_tx`]
    /// AFTER this returns, never from within.
    pub fn receive(&self, frame: &[u8]) -> Result<(), GsmL3Error> {
        let p = self.ptr("LapdmEntity::receive")?;
        // SAFETY: live entity; valid frame slice for the duration of the call
        // (C never retains the pointer — zero-copy decode reads it in place).
        unsafe { s::gsml3_lapdm_entity_receive(p.as_ptr(), frame.as_ptr(), frame.len()) };
        Ok(())
    }

    /// Send L3 via a UI frame (works in ANY state). `sapi` is the address-octet
    /// SAPI (0..15, range-checked in C; may differ from the open sapi).
    pub fn send_ui(&self, sapi: i32, l3: &[u8]) -> Result<(), GsmL3Error> {
        let p = self.ptr("LapdmEntity::send_ui")?;
        // SAFETY: live entity; valid slices; C returns the error code directly.
        let rc = unsafe { s::gsml3_lapdm_entity_send_ui(p.as_ptr(), sapi, l3.as_ptr(), l3.len()) };
        if rc != s::GSML3_OK {
            return Err(error::last_error("LapdmEntity::send_ui", rc));
        }
        Ok(())
    }

    /// Send L3 via I-frames (segmented); requires an ESTABLISHED link — the C
    /// error (non-empty message) is passed through for earlier states.
    pub fn send_data(&self, l3: &[u8]) -> Result<(), GsmL3Error> {
        let p = self.ptr("LapdmEntity::send_data")?;
        // SAFETY: live entity; valid slice.
        let rc = unsafe { s::gsml3_lapdm_entity_send_data(p.as_ptr(), l3.as_ptr(), l3.len()) };
        if rc != s::GSML3_OK {
            return Err(error::last_error("LapdmEntity::send_data", rc));
        }
        Ok(())
    }

    /// Send SABME (link establishment); requires the LinkReleased state.
    pub fn send_sabme(&self) -> Result<(), GsmL3Error> {
        let p = self.ptr("LapdmEntity::send_sabme")?;
        // SAFETY: live entity.
        let rc = unsafe { s::gsml3_lapdm_entity_send_sabme(p.as_ptr()) };
        if rc != s::GSML3_OK {
            return Err(error::last_error("LapdmEntity::send_sabme", rc));
        }
        Ok(())
    }

    /// Send DISC (link release); requires an established link.
    pub fn send_disc(&self) -> Result<(), GsmL3Error> {
        let p = self.ptr("LapdmEntity::send_disc")?;
        // SAFETY: live entity.
        let rc = unsafe { s::gsml3_lapdm_entity_send_disc(p.as_ptr()) };
        if rc != s::GSML3_OK {
            return Err(error::last_error("LapdmEntity::send_disc", rc));
        }
        Ok(())
    }

    /// Immediate transition to LinkReleased without sending frames.
    pub fn hard_release(&self) -> Result<(), GsmL3Error> {
        let p = self.ptr("LapdmEntity::hard_release")?;
        // SAFETY: live entity; no documented failure path.
        unsafe { s::gsml3_lapdm_entity_hard_release(p.as_ptr()) };
        Ok(())
    }

    /// Advance T200 by `elapsed_ms`. Returns 1 when a retransmission or an
    /// abnormal release occurred, 0 otherwise; the C internal-error form (-1)
    /// is surfaced as an error (see the thread-local message).
    pub fn tick_t200(&self, elapsed_ms: u32) -> Result<i32, GsmL3Error> {
        let p = self.ptr("LapdmEntity::tick_t200")?;
        // SAFETY: live entity.
        let rc = unsafe { s::gsml3_lapdm_entity_tick_t200(p.as_ptr(), elapsed_ms) };
        if rc < 0 {
            return Err(error::last_error("LapdmEntity::tick_t200", 0));
        }
        Ok(rc)
    }

    /// Current FSM state (GSML3_LAPDM_STATE_*).
    pub fn state(&self) -> Result<i32, GsmL3Error> {
        let p = self.ptr("LapdmEntity::state")?;
        // SAFETY: live entity; pure read.
        Ok(unsafe { s::gsml3_lapdm_entity_state(p.as_ptr() as *const _) })
    }

    /// True when the link is established (LinkEstablished or ContentionResolution).
    pub fn is_established(&self) -> Result<bool, GsmL3Error> {
        let p = self.ptr("LapdmEntity::is_established")?;
        // SAFETY: live entity.
        Ok(unsafe { s::gsml3_lapdm_entity_is_established(p.as_ptr() as *const _) } != 0)
    }

    /// Frames transmitted by the entity so far (counter).
    pub fn frames_sent(&self) -> Result<u32, GsmL3Error> {
        let p = self.ptr("LapdmEntity::frames_sent")?;
        // SAFETY: live entity.
        Ok(unsafe { s::gsml3_lapdm_entity_frames_sent(p.as_ptr() as *const _) })
    }

    /// Frames received by the entity so far (counter).
    pub fn frames_received(&self) -> Result<u32, GsmL3Error> {
        let p = self.ptr("LapdmEntity::frames_received")?;
        // SAFETY: live entity.
        Ok(unsafe { s::gsml3_lapdm_entity_frames_received(p.as_ptr() as *const _) })
    }

    /// T200 retransmissions so far (counter).
    pub fn retransmissions(&self) -> Result<u32, GsmL3Error> {
        let p = self.ptr("LapdmEntity::retransmissions")?;
        // SAFETY: live entity.
        Ok(unsafe { s::gsml3_lapdm_entity_retransmissions(p.as_ptr() as *const _) })
    }

    /// Take-and-empty the L3 event queue. No FFI: call AFTER any C call on
    /// this entity returned (post-FFI processing is the only legal time to
    /// parse/feed/transmit — queue model). A closed entity yields an empty
    /// queue (no FFI, no error — the queues hold nothing more at that point).
    pub fn drain_l3(&self) -> Vec<L3Event> {
        self.trampoline_state().map(|s| s.drain_l3()).unwrap_or_default()
    }

    /// Take-and-empty the transmit-frame queue (owned copies; no FFI). A closed
    /// entity yields an empty queue, as [`LapdmEntity::drain_l3`].
    pub fn drain_tx(&self) -> Vec<Vec<u8>> {
        self.trampoline_state().map(|s| s.drain_tx()).unwrap_or_default()
    }

    /// Clear the transmit-frame collector (start of a fresh `send_frame`).
    /// No FFI, no error: on a closed entity this is a no-op.
    pub fn reset_tx(&self) {
        if let Some(s) = self.trampoline_state() {
            s.clear_tx();
        }
    }

    fn trampoline_state(&self) -> Option<Arc<TrampolineState>> {
        let ctx = self.ctx?;
        // SAFETY: while the entity value is not mid-close_once, the box it
        // points at is alive (reclamation needs &mut and happens last); cloning
        // the Arc keeps the queues reachable for this call's duration only.
        Some(unsafe { Arc::clone(&*ctx.as_ptr()) })
    }
}

impl Drop for LapdmEntity {
    fn drop(&mut self) {
        self.close_once(); // idempotent: an explicit close() first makes this a no-op
    }
}

impl std::fmt::Debug for LapdmEntity {
    /// Stable test/debug rendering (never the raw pointers): both take flags.
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("LapdmEntity")
            .field("entity_open", &self.p.is_some())
            .field("ctx_live", &self.ctx.is_some())
            .finish()
    }
}

/// Crate-internal test hook: the release-order tests live in sibling modules
/// and cannot read the private take-flags directly.
#[cfg(test)]
pub(crate) fn lapdm_handles_released(e: &LapdmEntity) -> bool {
    e.p.is_none() && e.ctx.is_none()
}

// ── Zero-copy frame decoding ────────────────────────────────────────────────

/// One decoded LAPDm frame — a VIEW over the input, not an allocation:
/// [`FrameInfo::payload`] is a sub-slice of the buffer that was passed to
/// [`decode_frame`], so the zero-copy path is enforced by the lifetime `'a`
/// (the decoder's C `info` pointer points inside our buffer).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct FrameInfo<'a> {
    /// GSML3_LAPDM_FMT_I / _S / _U
    pub format: i32,
    /// GSML3_LAPDM_U_* when format == U, else -1
    pub u_type: i32,
    /// GSML3_LAPDM_S_* when format == S, else -1
    pub s_type: i32,
    /// Receive sequence number (I/S frames)
    pub nr: u8,
    /// Send sequence number (I frames)
    pub ns: u8,
    /// Poll/Final bit
    pub pf: i32,
    /// Message-complete bit (I frames)
    pub m_bit: i32,
    /// SAPI 0..15
    pub sapi: i32,
    /// C/R bit: 1 = command, 0 = response
    pub command: i32,
    /// Zero-copy view of the info octets into the caller's frame (`None` when
    /// the frame carries no info); valid for exactly `'a`.
    pub payload: Option<&'a [u8]>,
}

/// Decode one raw LAPDm frame (address + control [+ length + info]) with the
/// C decoder — zero-copy: `info` points into `frame` and the result borrows it.
/// A frame shorter than 2 bytes (address+control) is rejected BEFORE FFI
/// (NULL-policy). The C decoder's view is bounds-checked against the slice; a
/// view that escaped the input would be an ABI violation and becomes an error,
/// never UB.
pub fn decode_frame<'a>(frame: &'a [u8]) -> Result<FrameInfo<'a>, GsmL3Error> {
    if frame.len() < 2 {
        return Err(error::invalid_arg(
            "lapdm::decode_frame",
            "a LAPDm frame needs at least address + control bytes (2)",
        ));
    }
    // The output struct is written fully by C before the call returns.
    let mut out: MaybeUninit<s::gsml3_lapdm_frame_info> = MaybeUninit::uninit();
    // SAFETY: frame is valid for len bytes; out provides one slot; the call is
    // synchronous and returns the error code in-band (spans stay inside frame).
    let rc = unsafe { s::gsml3_lapdm_frame_decode(frame.as_ptr(), frame.len(), out.as_mut_ptr()) };
    if rc != s::GSML3_OK {
        return Err(error::last_error("lapdm::decode_frame", rc));
    }
    // SAFETY: the call fully initialized `out` (rc == GSML3_OK).
    let fi = unsafe { out.assume_init() };
    // Zero-copy contract: fi.info is NULL or points INSIDE `frame`. The address
    // math below compares raw addresses (pointer → usize conversion performs no
    // arithmetic, so it carries no validity precondition): anything outside
    // the slice — an impossible ABI violation — becomes an error, never a
    // slice we are allowed to form.
    let payload = if fi.info.is_null() || fi.info_len == 0 {
        None
    } else {
        let start = frame.as_ptr() as usize;
        let info_addr = fi.info as usize;
        if info_addr < start || (info_addr - start) + fi.info_len > frame.len() {
            return Err(error::internal(
                "lapdm::decode_frame",
                "the C decoder returned a view outside the input buffer (ABI violation)",
            ));
        }
        let off = info_addr - start;
        Some(&frame[off..off + fi.info_len])
    };
    Ok(FrameInfo {
        format: fi.format,
        u_type: fi.u_type,
        s_type: fi.s_type,
        nr: fi.nr,
        ns: fi.ns,
        pf: fi.pf,
        m_bit: fi.m_bit,
        sapi: fi.sapi,
        command: fi.command,
        payload,
    })
}

/// Minimal LAPDm (GSM 04.06) MS/peer-side frame builders for simulation and the
/// demo — a byte-for-byte mirror of the Python `_lapdm` / Go `lapdmmini` mini
/// codecs. Purpose: a simulation must BUILD Mobile-station-
/// side frames to feed a BTS-side entity; the C core decodes peer frames and
/// transmits only its own, so PRODUCTION TRANSMISSION GOES THROUGH THE C ENTITY
/// (`send_ui` / `send_data` / `send_sabme` / `send_disc`) — never these helpers.
///
/// Byte layout per `src/lapdm_frame.cpp`: address = (sapi << 4) | (command ?
/// 0x08 : 0) | 0x01 (EA=1); U-control by (type, pf): UI 0x03/0x07, SABME
/// 0x2B/0x2F, UA 0x5F/0x63, DM 0x0B/0x0F, DISC 0x0C/0x08; S-control =
/// (nr << 5) | (pf ? 0x10 : 0) | type (RR 0x01 / REJ 0x0D); I-control =
/// (nr << 5) | (pf ? 0x10 : 0) | (ns << 1), preceded by a length octet
/// (m << 7 | len & 0x3F) before the info; UI carries RAW info, NO length octet.
/// Validation mirrors the other bindings as an `Err(INVALID_ARG)` (programmer
/// error in test/demo code, not a C-core failure): sapi 0..15, nr/ns 0..7,
/// info <= 63 bytes. Every generated frame is cross-checked against
/// [`decode_frame`] in the tests (closed loop).
pub mod mini {
    use crate::error::GsmL3Error;

    /// LAPDm info field limit: 7-bit length octet with m=0 (63 octets max).
    const MAX_INFO: usize = 63;

    /// Programmer-error factory (NOT a C-core failure): out-of-domain mini-
    /// codec input surfaces as code INVALID_ARG with a descriptive message —
    /// the Rust analogue of Python's `ValueError` / Go's miniPanic.
    fn invalid(msg: &str) -> GsmL3Error {
        GsmL3Error {
            code: crate::sys::GSML3_ERR_INVALID_ARG,
            op: "lapdm::mini",
            msg: msg.to_string(),
        }
    }

    fn check_sapi(what: &str, sapi: u8) -> Result<(), GsmL3Error> {
        if sapi > 15 {
            return Err(invalid(&format!("{what}: sapi out of range 0..=15 (got {sapi})")));
        }
        Ok(())
    }

    fn check_seq(what: &str, v: u8) -> Result<(), GsmL3Error> {
        if v > 7 {
            return Err(invalid(&format!("{what} out of range 0..=7 (got {v}) — LAPDm sequence numbers are mod 8")));
        }
        Ok(())
    }

    fn check_info(what: &str, info: &[u8]) -> Result<(), GsmL3Error> {
        if info.len() > MAX_INFO {
            return Err(invalid(&format!("{what} info exceeds the 63-octet LAPDm info field (got {} bytes)", info.len())));
        }
        Ok(())
    }

    /// The address octet of a normal (EA=1) SAPI address: (sapi << 4) |
    /// (command ? 0x08 : 0) | 0x01.
    fn addr(sapi: u8, command: bool) -> u8 {
        (sapi << 4) | u8::from(command) << 3 | 0x01
    }

    /// UI frame, MS/peer side, pf=0: [address][0x03] + RAW info (no length octet).
    /// The L3 unit-data injection used by simulation.
    pub fn ui(sapi: u8, command: bool, info: &[u8]) -> Result<Vec<u8>, GsmL3Error> {
        check_sapi("ui", sapi)?;
        check_info("UI", info)?;
        let mut f = vec![addr(sapi, command), 0x03]; // UI, pf=0
        f.extend_from_slice(info);
        Ok(f)
    }

    /// MS-side unnumbered acknowledgement: exactly [0x01, 0x63] (SAPI 0,
    /// response, pf=1) — the byte vector the link-lifecycle test expects.
    pub fn ua() -> Vec<u8> {
        vec![addr(0, false), 0x63] // UA, pf=1
    }

    /// Set-Asynchronous-Balance-Mode command/response (pf=1): [address][0x2F].
    pub fn sabme(sapi: u8, command: bool) -> Result<Vec<u8>, GsmL3Error> {
        check_sapi("sabme", sapi)?;
        Ok(vec![addr(sapi, command), 0x2F])
    }

    /// Discouraged-mode response (pf=0), e.g. a peer refusing a SABME:
    /// [address][0x0B].
    pub fn dm(sapi: u8) -> Result<Vec<u8>, GsmL3Error> {
        check_sapi("dm", sapi)?;
        Ok(vec![addr(sapi, false), 0x0B])
    }

    /// Disconnect command/response (pf=1): [address][0x08].
    pub fn disc(sapi: u8, command: bool) -> Result<Vec<u8>, GsmL3Error> {
        check_sapi("disc", sapi)?;
        Ok(vec![addr(sapi, command), 0x08])
    }

    /// Receive-Ready S-frame (response, pf=0): [address][(nr << 5) | 0x01].
    pub fn rr(nr: u8, sapi: u8) -> Result<Vec<u8>, GsmL3Error> {
        check_seq("RR nr", nr)?;
        check_sapi("rr", sapi)?;
        Ok(vec![addr(sapi, false), (nr << 5) | 0x01]) // S/RR, pf=0
    }

    /// I-frame: [address][(nr<<5)|(pf?0x10:0)|(ns<<1)] [(m<<7)|len] + info.
    /// `m` marks message-complete segmentation.
    pub fn i_frame(sapi: u8, command: bool, nr: u8, ns: u8, pf: bool, m: bool, info: &[u8]) -> Result<Vec<u8>, GsmL3Error> {
        check_seq("I nr", nr)?;
        check_seq("I ns", ns)?;
        check_sapi("i_frame", sapi)?;
        check_info("I", info)?;
        let mut ctrl = (nr << 5) | ((ns & 0x07) << 1);
        if pf {
            ctrl |= 0x10;
        }
        let mut len_octet = info.len() as u8 & 0x3F;
        if m {
            len_octet |= 0x80;
        }
        let mut out = vec![addr(sapi, command), ctrl, len_octet];
        out.extend_from_slice(info);
        Ok(out)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::error::ErrorKind;

    /// Unit half of the entity safety suite: double close / Drop after
    /// close must be a pure no-op (take-pattern — no double free, no double
    /// Box::from_raw), and every FFI method afterwards reports closed.
    #[test]
    fn test_close_idempotent() {
        let mut e = LapdmEntity::new(0).expect("SDCCH entity");
        e.open(0, 1).expect("open as BTS side");
        e.close(); // first pass: alive=false, entity_free, from_raw(context)
        assert!(e.p.is_none()); // handle taken — no second free can happen
        assert!(e.ctx.is_none()); // context taken — single reclamation
        e.close(); // second pass: pure no-op (take-pattern)
        // Every FFI-facing method is a closed-class error now (no FFI possible).
        assert!(e.state().unwrap_err().is_closed());
        assert!(e.receive(&[0x01, 0x03]).unwrap_err().is_closed());
        drop(e); // Drop after close: third pass — also a no-op, no panic, no double free
    }

    #[test]
    fn test_drop_reclaims_context_once() {
        let mut e = LapdmEntity::new(0).expect("entity");
        e.open(0, 1).expect("open");
        // Drive one event through the real trampoline path first.
        e.receive(&[0x01, 0x03, 0x60, 0x0d, 0x00]).expect("UI receive");
        assert!(!e.drain_l3().is_empty());

        // Before reclamation the box is the SOLE owner (strong count 1);
        // close_once carries the matching debug assertion right before the
        // Box::from_raw, so any leaked clone fails every debug build run.
        let strong = unsafe { Arc::strong_count(&*e.ctx.expect("ctx").as_ptr()) };
        assert_eq!(strong, 1);
        e.close(); // runs close_once: from_raw exactly once, guarded by the assertion
        assert!(e.p.is_none() && e.ctx.is_none());
        drop(e); // no-op second pass
    }

    #[test]
    fn test_entity_lifecycle_and_counters() {
        let e = LapdmEntity::new(1).expect("SACCH entity");
        e.open(3, 0).expect("open as MS side on SAPI3");
        assert_eq!(e.state().unwrap(), s::GSML3_LAPDM_STATE_LINK_RELEASED);
        assert!(!e.is_established().unwrap());

        // send_data BEFORE an established link must fail with a C-side message.
        let err = e.send_data(&[0x60, 0x0d, 0x00]).unwrap_err();
        assert_ne!(err.kind(), Some(ErrorKind::Ok));
        assert!(!err.msg.is_empty());

        e.hard_release().expect("hard release is a no-op transition");
        assert_eq!(e.frames_sent().unwrap(), 0);
        drop(e); // Drop path (no explicit close): single free, context reclaimed.
    }

    #[test]
    fn decode_rejects_short_frames_before_ffi() {
        let e = decode_frame(&[0x60]).unwrap_err();
        assert_eq!(e.kind(), Some(ErrorKind::InvalidArg));
        let e = decode_frame(&[]).unwrap_err();
        assert_eq!(e.kind(), Some(ErrorKind::InvalidArg));
    }
}
