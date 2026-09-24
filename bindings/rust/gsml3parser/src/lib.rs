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

//! Safe Rust bindings over the libgsml3parser C ABI (`gsml3parser_c.h`):
//! L3 parsing/serialization (all 12 protocol domains), A-bis RSL, the LAPDm
//! entity with safe callback delivery, and the BTS stack layer (registry /
//! borrowed session / orchestrator). The v1 surface mirrors planK decision #6:
//! sections S1–S7 of the C header plus the two typed builders
//! `gsml3_build_cm_service_request` / `gsml3_build_setup`. Zero runtime
//! dependencies — `std` only.
//!
//! ## Ownership
//!
//! Every constructor returns a value that owns its C handle and releases it
//! exactly once, idempotently: [`Message`], [`Config`], [`RslFrame`],
//! [`Registry`], [`LapdmEntity`] and [`GsmL3Stack`] all implement `Drop` with a
//! take-pattern (`Option`/nulled fields), so an explicit [`GsmL3Stack::close`]
//! plus the later `Drop` is a no-op second pass. Release order inside a stack
//! teardown is mandated by the C ABI and enforced by `close()`:
//! **entity → orchestrator → registry** — and a session pointer is BORROWED:
//! it is never freed directly (its registry frees all sessions), see [`Session`].
//! After any close, every method returns a closed-class error WITHOUT touching
//! the (already freed) handle.
//!
//! ## Callback rules (queue model)
//!
//! LAPDm entity callbacks are invoked SYNCHRONOUSLY from `gsml3_lapdm_entity_*`
//! calls and their spans (`l3`, `frame`) are valid ONLY for the duration of the
//! callback. This binding therefore follows one rule everywhere: the C-side
//! trampoline performs memory-only work — it clones its context
//! (`Arc<TrampolineState>` behind a `Box::into_raw` token in `void* user`) and
//! appends an OWNED copy to a `Mutex`-guarded queue; it NEVER calls back into
//! any `gsml3_*` function. [`GsmL3Stack::send_frame`] drains those queues only
//! AFTER the C call returns, then parses, feeds the orchestrator and transmits
//! auto-responses through the same entity — re-entering a live FSM mid-receive
//! would violate the ABI. The trampoline context is reclaimed exactly once,
//! with `Box::from_raw`, strictly AFTER `gsml3_lapdm_entity_free`
//! (`LapdmEntity::close_once`).
//!
//! ## Threading
//!
//! The C ABI is one thread per owned handle (a sharded registry makes its own
//! registry-mediated calls thread-safe). Rust expresses this directly:
//! [`GsmL3Stack`] is `Send` (the whole stack may move to another thread) but
//! intentionally NOT `Sync` — sharing a `&GsmL3Stack` would allow unsynchronized
//! concurrent calls on one entity, which the ABI forbids. Owned wrapper types
//! are `!Send`/`!Sync` by default (they hold raw pointers) and are used on a
//! single thread.
//!
//! ## Errors and NULL policy
//!
//! A failing C call produces a [`GsmL3Error`] carrying the C code plus a
//! synchronously copied thread-local message (`gsml3_last_error()` must be read
//! at the failing call site: every later successful FFI call clears it).
//! Inputs where the C side is documented to fail are rejected BEFORE the FFI
//! boundary with code `INVALID_ARG` (empty parse input, frame shorter than 2
//! bytes, zero TMSI, unknown shard count / SAPI / profile); the C-documented
//! NULL-safe entry points pass nulls through and report their sentinels.

pub mod error;
pub mod lapdm;
pub mod message;
pub mod registry;
pub mod stack;

/// The raw FFI layer: `#[repr(C)]` structures, callback types and the 128
/// `extern "C"` declarations of the v1 surface (S1–S7 + two curated S9
/// builders). Exposed for advanced/test use — prefer the safe API.
pub use gsml3parser_sys as sys;

pub use error::{ErrorKind, GsmL3Error};
pub use lapdm::{decode_frame, FrameInfo, LapdmEntity, TrampolineState};
pub use message::{build_cm_service_request, build_setup, Config, Message, RslFrame};
pub use registry::{Registry, Session};
pub use stack::{GsmL3Stack, L3Event, StepResult};

/// Product version of the C core, from `gsml3_version()` (stamped by CMake from
/// the repo-root `VERSION` file — the single source of truth, decision #13).
pub fn version() -> String {
    // SAFETY: gsml3_version returns a pointer to static storage that is never NULL.
    let p = unsafe { sys::gsml3_version() };
    // SAFETY: p points to a NUL-terminated string (header contract: never free).
    unsafe { std::ffi::CStr::from_ptr(p) }.to_string_lossy().into_owned()
}

/// C ABI revision reported by the loaded library (`== GSML3_ABI_VERSION`).
/// Compare against [`sys::ABI_VERSION`] at startup when linking prebuilt
/// binaries of possibly different builds.
pub fn abi_version() -> u32 {
    // SAFETY: stateless no-argument observer of the loaded C core.
    unsafe { sys::gsml3_abi_version() }
}
