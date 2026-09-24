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

//! Integration tests for the Rust binding. They exercise the
//! FFI boundary with the SAME behavioral vectors as the C reference tests, the
//! Python suite and the Go suite: the unified "MO call over SDCCH" scenario
//! (`TEST(CApiOrchestrator, MOCallSetupChain)` / `CallSetupMO_T3101_Timeout`),
//! the link-lifecycle mirror of `Entity_LinkLifecycle`, T200(SDCCH) = 900 ms
//! retransmission, the mini-codec closed loop against the C decoder, the NULL policy
//! (`TEST(CApi, NullSafety)` mirror), the closed-stack no-FFI invariant and the
//! ABI version check.

use std::os::raw::c_void;
use std::path::Path;
use std::ptr;

use gsml3parser::error::ErrorKind;
use gsml3parser::lapdm::mini;
use gsml3parser::sys as s;
use gsml3parser::{
    build_cm_service_request, build_setup, decode_frame, GsmL3Stack, LapdmEntity, Message, Registry,
};

const DEMO_TMSI: u32 = 0x8765_4321; // the demo scenario constant (SAPI 0 / profile 0 / BTS side)

/// MS-side UI wrapper helper (the simulation builds peer frames; the C core
/// decodes them — production transmission goes through the C entity only).
fn ms_ui(l3: &[u8]) -> Vec<u8> {
    mini::ui(0, false, l3).expect("mini-codec ui() is valid for this input")
}

/// The full MO-call-over-SDCCH scenario with a ZERO-COPY decode verification:
/// the C decoder's payload view must alias the transmitted frame buffer (the
/// decode path never copies) — verified through pointer offset, not just bytes.
#[test]
fn mo_call_simulation() {
    let mut stack = GsmL3Stack::new(DEMO_TMSI, 0 /* plain registry */, 0 /* SAPI 0 */, 0 /* profile SDCCH */, true)
        .expect("BTS-side stack with auto-response");

    // -- [1] CMServiceRequest (MO call) → CMServiceAccept, token 10 ----------
    let l3 = build_cm_service_request(1 /* MobileOriginatedCall */, s::GSML3_ID_TMSI, DEMO_TMSI, None)
        .expect("C typed builder: cm_service_request");
    let txs = stack.send_frame(&ms_ui(&l3)).expect("send UI CMServiceRequest");
    assert_eq!(txs.len(), 1, "exactly one auto-response frame is expected");

    let step = stack.last_step().expect("auto mode records the processed step");
    assert_eq!(step.token, s::GSML3_TOKEN_CM_SERVICE_ACCEPT, "token must be CM_SERVICE_ACCEPT (10)");
    assert_eq!(step.code, s::GSML3_OK);

    let dec = decode_frame(&txs[0]).expect("decode the BTS UI response");
    assert_eq!(
        (dec.format, dec.u_type, dec.sapi, dec.command),
        (s::GSML3_LAPDM_FMT_U, s::GSML3_LAPDM_U_UI, 0, 1),
        "response frame must be a BTS-side UI (C/R=1) on SAPI 0"
    );
    let payload = dec.payload.expect("UI frames carry the L3 info octets");

    // ZERO-COPY verification: the decoded view aliases txs[0] (the decoder
    // points into our buffer — no intermediate allocation on that path).
    // Address comparison only (pointer → usize is a conversion, not arithmetic),
    // so the check itself needs no safety precondition.
    let start = txs[0].as_ptr() as usize;
    let end = start + txs[0].len();
    let info_addr = payload.as_ptr() as usize;
    assert!(info_addr >= start && info_addr <= end, "decoder payload must be a view INSIDE the transmitted frame");
    let off = info_addr - start;
    assert!(off + payload.len() <= txs[0].len(), "view ends inside the buffer");
    assert_eq!(&txs[0][off..off + payload.len()], payload, "view bytes == sub-slice bytes");

    let resp = Message::parse(payload, None).expect("parse the response L3");
    assert_eq!(resp.name().unwrap(), "CMServiceAccept");
    assert_eq!(resp.pd().unwrap(), s::GSML3_PD_MM);
    let wire = resp.to_vec().expect("exact-size serialization");
    assert_eq!(wire, payload, "the auto response IS the parsed CMServiceAccept wire form");

    // -- [2] CC Setup (TI=3, called 123456789) → CallProceeding, token 17 ----
    let l3s = build_setup(3, "123456789").expect("C typed builder: setup");
    let txs2 = stack.send_frame(&ms_ui(&l3s)).expect("send UI Setup");
    assert_eq!(txs2.len(), 1);
    let step = stack.last_step().unwrap();
    assert_ne!(step.token, s::GSML3_TOKEN_NONE);
    assert_eq!(step.token, s::GSML3_TOKEN_CALL_PROCEEDING);

    let cp_dec = decode_frame(&txs2[0]).expect("decode the CallProceeding frame");
    let cp = Message::parse(cp_dec.payload.expect("payload present"), None).expect("parse CallProceeding");
    assert_eq!(cp.name().unwrap(), "CallProceeding");
    assert_eq!(cp.ti().unwrap(), 3);

    // The chain is now in the MO call-setup phase (0x04).
    assert_eq!(stack.chain_phase().unwrap(), s::GSML3_PROC_CALL_SETUP_MO);

    // -- [3] T3101 = 3 s: inside the window nothing fails and there is no ────
    //      retransmission; cumulative > 3000 ms times the chain out (one
    //      failure) and resets it to idle.
    assert_eq!(stack.tick_procedures(1000).unwrap(), 0);
    assert_eq!(stack.take_retransmit().unwrap(), s::GSML3_TOKEN_NONE);

    assert_eq!(stack.tick_procedures(2500).unwrap(), 1, "T3101 expires after 3 s cumulative");
    assert_eq!(stack.chain_phase().unwrap(), s::GSML3_PROC_UNKNOWN, "chain must be idle after the timeout");

    stack.close(); // idempotent teardown (the Drop below is a no-op)
}

/// Link-lifecycle mirror of C `TEST(Entity, Entity_LinkLifecycle)`: SABME → UA
/// establishes the link with an empty ESTABLISH_CONFIRM, UI delivers
/// UNIT_DATA, DISC → UA releases. Byte-exact frame vectors per src/lapdm_frame.cpp.
#[test]
fn link_lifecycle() {
    let e = LapdmEntity::new(0).expect("SDCCH-profile entity");
    e.open(0, 1 /* BTS side */).expect("open → LINK_RELEASED");
    assert_eq!(e.state().unwrap(), s::GSML3_LAPDM_STATE_LINK_RELEASED);

    // SABME from the BTS side: byte-exact [addr(0, command=1)=0x09][SABME pf=1=0x2F].
    e.send_sabme().expect("sabme in LINK_RELEASED");
    assert_eq!(e.state().unwrap(), s::GSML3_LAPDM_STATE_AWAITING_ESTABLISH);
    let tx = e.drain_tx();
    assert_eq!(tx, vec![vec![0x09, 0x2F]], "SABME tx must be byte-exact");

    // MS acknowledges with UA [0x01, 0x63]: the link is established and an
    // EMPTY-payload ESTABLISH_CONFIRM reaches L3.
    e.receive(&[0x01, 0x63]).expect("receive UA");
    assert_eq!(e.state().unwrap(), s::GSML3_LAPDM_STATE_LINK_ESTABLISHED);
    assert!(e.is_established().unwrap());
    let evs = e.drain_l3();
    assert_eq!(evs.len(), 1, "exactly one L3 event on establish confirm");
    assert_eq!(evs[0].primitive, s::GSML3_PRIM_L3_ESTABLISH_CONFIRM);
    assert!(evs[0].data.is_empty(), "ESTABLISH_CONFIRM carries no payload");

    // A UI frame on the established link delivers UNIT_DATA with its payload.
    let l3: &[u8] = &[0x60, 0x0d, 0x00]; // stable Channel Release vector (mirrors the C test suite)
    e.receive(&ms_ui(l3)).expect("receive UI");
    let evs = e.drain_l3();
    assert_eq!(evs.len(), 1);
    assert_eq!(evs[0].primitive, s::GSML3_PRIM_L3_UNIT_DATA);
    assert_eq!(&evs[0].data, l3, "the L3 payload must arrive intact (owned copy)");

    // DISC (BTS side) → AWAITING_RELEASE; MS UA [0x01, 0x63] → LINK_RELEASED.
    e.send_disc().expect("disc in LINK_ESTABLISHED");
    assert_eq!(e.state().unwrap(), s::GSML3_LAPDM_STATE_AWAITING_RELEASE);
    e.drain_tx(); // DISC frame captured — consume the collector
    e.receive(&[0x01, 0x63]).expect("receive UA for disc");
    assert_eq!(e.state().unwrap(), s::GSML3_LAPDM_STATE_LINK_RELEASED);
    assert!(!e.is_established().unwrap());

    // FSM receive counter: UA + UI + UA (SABME/DISC were transmitted, not received).
    assert_eq!(e.frames_received().unwrap(), 3);

    drop(e); // Drop path reclaims the entity + trampoline context once.
}

/// T200(SDCCH profile 0) = 900 ms: expiring it with no UA retransmits SABME
/// and bumps the retransmission counter (C header S5 contract).
#[test]
fn t200_retransmission() {
    let e = LapdmEntity::new(0).expect("entity");
    e.open(0, 1).expect("open");
    e.send_sabme().expect("sabme");
    e.drain_tx(); // consume the original SABME
    assert_eq!(e.retransmissions().unwrap(), 0);

    // Inside the T200 window nothing happens…
    assert_eq!(e.tick_t200(500).unwrap(), 0);
    assert_eq!(e.retransmissions().unwrap(), 0);
    // …and at the 900 ms mark a retransmission fires.
    assert_eq!(e.tick_t200(400).unwrap(), 1, "T200 expiry must report a retransmission");
    assert_eq!(e.retransmissions().unwrap(), 1);
    let tx = e.drain_tx();
    assert_eq!(tx, vec![vec![0x09, 0x2F]], "the retransmitted frame is the SABME again");
    assert_eq!(e.state().unwrap(), s::GSML3_LAPDM_STATE_AWAITING_ESTABLISH, "still awaiting establish after one retransmission");

    // A UA arrives before N201: normal establishment (no abnormal release).
    e.receive(&[0x01, 0x63]).expect("UA");
    assert!(e.is_established().unwrap());
}

/// Mini-codec closed loop: every frame the Rust mini-codec builds must decode
/// back with the C decoder to the exact same
/// fields — and validation errors stay on the Rust side (INVALID_ARG).
#[test]
fn mini_codec_closed_loop_with_c_decoder() {
    let l3: &[u8] = &[0x60, 0x0d, 0x00];

    // UI: [address][0x03 pf=0] + RAW info (no length octet).
    assert_eq!(ms_ui(l3), [0x01, 0x03, 0x60, 0x0d, 0x00]); // canonical UI bytes: address 0x01 + control 0x03 (pf=0)
    let f = ms_ui(l3);
    let d = decode_frame(&f).unwrap();
    assert_eq!((d.format, d.u_type, d.sapi, d.command, d.pf), (s::GSML3_LAPDM_FMT_U, s::GSML3_LAPDM_U_UI, 0, 0, 0));
    assert_eq!(d.payload, Some(l3));

    let f = mini::ui(3, true, l3).unwrap(); // SAPI 3, command bit
    let d = decode_frame(&f).unwrap();
    assert_eq!((d.sapi, d.command), (3, 1));
    assert_eq!(d.payload, Some(l3));

    // UA: the fixed byte vector of the link-lifecycle tests.
    assert_eq!(mini::ua(), [0x01, 0x63]);
    let f = mini::ua();
    let d = decode_frame(&f).unwrap();
    assert_eq!((d.format, d.u_type, d.pf), (s::GSML3_LAPDM_FMT_U, s::GSML3_LAPDM_U_UA, 1));

    // SABME / DM / DISC control bytes by (type, pf) per src/lapdm_frame.cpp.
    let f = mini::sabme(0, true).unwrap();
    let d = decode_frame(&f).unwrap();
    assert_eq!((d.u_type, d.pf, d.command), (s::GSML3_LAPDM_U_SABME, 1, 1));
    let f = mini::dm(0).unwrap();
    let d = decode_frame(&f).unwrap();
    assert_eq!((d.format, d.u_type), (s::GSML3_LAPDM_FMT_U, s::GSML3_LAPDM_U_DM));
    let f = mini::disc(0, true).unwrap();
    let d = decode_frame(&f).unwrap();
    assert_eq!((d.u_type, d.pf), (s::GSML3_LAPDM_U_DISC, 1));

    // S-frame RR: [address][(nr << 5) | 0x01] (pf=0).
    let rr = mini::rr(1, 0).unwrap();
    assert_eq!(rr, [0x01, 0x21]);
    let d = decode_frame(&rr).unwrap();
    assert_eq!((d.format, d.s_type, d.nr), (s::GSML3_LAPDM_FMT_S, s::GSML3_LAPDM_S_RR, 1));

    // I-frame: ctrl = (nr<<5)|(pf?0x10:0)|(ns<<1); length octet (m<<7)|len; +info.
    let info: &[u8] = &[5, 6];
    let i = mini::i_frame(0, true, /* nr */ 1, /* ns */ 2, /* pf */ true, /* m */ false, info).unwrap();
    assert_eq!(i, [0x09, 0x34, 0x02, 5, 6], "ctrl=0x34: nr=1<<5 | pf<<4 | ns=2<<1");
    let d = decode_frame(&i).unwrap();
    assert_eq!((d.format, d.ns, d.nr, d.pf, d.m_bit), (s::GSML3_LAPDM_FMT_I, 2, 1, 1, 0));
    assert_eq!(d.payload, Some(info), "the info view must alias the I-frame");

    // Validation is Rust-side (Err with code INVALID_ARG) — not a C error:
    assert_eq!(mini::ui(16, false, l3).unwrap_err().kind(), Some(ErrorKind::InvalidArg));
    assert!(mini::rr(8, 0).is_err());
    assert!(mini::i_frame(0, true, 0, 0, false, false, &[0u8; 64]).is_err()); // >63 info octets
}

/// NULL-policy suite (mirror of C `TEST(CApi, NullSafety)`):
/// (a) C-documented NULL-safe entry points pass NULL through raw and report
/// their sentinels; (b) inputs where the C side is documented to fail are
/// rejected at language level BEFORE FFI; (c) C validation passes through the
/// safe wrappers untouched; (d) the closed path performs no FFI at all.
#[test]
fn null_safety() {
    // (a) Raw NULL pass-through with the documented sentinels (test-only unsafe;
    // these exact entry points are the C header's "NULL-safe" contract).
    // SAFETY: all calls below take NULL exactly as the C ABI documents them to.
    let name = unsafe { s::gsml3_message_name(ptr::null()) };
    assert_eq!(unsafe { std::ffi::CStr::from_ptr(name) }.to_bytes(), b""); // "" sentinel
    assert_eq!(unsafe { s::gsml3_message_pd(ptr::null()) }, -1); // -1 sentinels…
    assert_eq!(unsafe { s::gsml3_message_mti(ptr::null()) }, -1);
    assert_eq!(unsafe { s::gsml3_message_ti(ptr::null()) }, 0); // …except ti → 0
    let mut buf = [0u8; 8];
    assert_eq!(unsafe { s::gsml3_message_write(ptr::null(), buf.as_mut_ptr(), buf.len()) }, 0); // 0 sentinel
    assert!(unsafe { s::gsml3_message_hex(ptr::null()) }.is_null()); // NULL (pointer IS the error form)
    assert!(unsafe { s::gsml3_message_dump(ptr::null()) }.is_null());

    // All release functions are NULL-safe no-ops.
    // SAFETY: documented no-ops on NULL — the very contract under test.
    unsafe {
        s::gsml3_config_free(ptr::null_mut());
        s::gsml3_message_free(ptr::null_mut());
        s::gsml3_rsl_free(ptr::null_mut());
        s::gsml3_registry_free(ptr::null_mut());
        s::gsml3_orchestrator_free(ptr::null_mut());
        s::gsml3_lapdm_entity_free(ptr::null_mut());
        s::gsml3_free(ptr::null_mut::<c_void>());
    }

    // parse_into with a NULL message is not OK (an error code, content kept).
    assert_ne!(
        unsafe { s::gsml3_parse_l3_into(ptr::null_mut(), ptr::null(), 0, ptr::null()) },
        s::GSML3_OK
    );

    // Entity state/stats on a NULL entity: UNUSED / zero counters.
    assert_eq!(unsafe { s::gsml3_lapdm_entity_state(ptr::null()) }, s::GSML3_LAPDM_STATE_UNUSED);
    assert_eq!(unsafe { s::gsml3_lapdm_entity_is_established(ptr::null()) }, 0);
    assert_eq!(unsafe { s::gsml3_lapdm_entity_frames_sent(ptr::null()) }, 0);
    assert_eq!(unsafe { s::gsml3_lapdm_entity_frames_received(ptr::null()) }, 0);
    assert_eq!(unsafe { s::gsml3_lapdm_entity_retransmissions(ptr::null()) }, 0);

    // (b) Language-level rejections BEFORE FFI (typed error, kind InvalidArg):
    assert_eq!(Message::parse(&[], None).unwrap_err().kind(), Some(ErrorKind::InvalidArg));
    let ent = LapdmEntity::new(0).expect("entity for pre-FFI checks");
    assert_eq!(decode_frame(&[0x60]).unwrap_err().kind(), Some(ErrorKind::InvalidArg)); // <2 bytes

    let st = GsmL3Stack::new(0x1234_5678, 0, 0, 0, true).expect("stack for pre-FFI checks");
    let e = st.send_frame(&[]).unwrap_err(); // empty frame: rejected before any FFI (no panic)
    assert_eq!(e.kind(), Some(ErrorKind::InvalidArg));
    assert_eq!(st.feed_l3(&[]).unwrap_err().kind(), Some(ErrorKind::InvalidArg)); // nil/empty L3
    assert!(LapdmEntity::new(7).is_err()); // bad profile before FFI

    // (c) C validation passing through the wrappers with the C message:
    let reg = Registry::new(0).expect("plain registry");
    let e = reg.create_by_tmsi(0).unwrap_err(); // reserved all-zero TMSI → INVALID_ARG
    assert_eq!(e.kind(), Some(ErrorKind::InvalidArg));
    assert!(!e.msg.is_empty(), "the C thread-local message must be carried through");

    let e = ent.open(16, 1); // invalid sapi: C keeps the previous state + reports
    assert!(e.is_err()); // …and the wrapper surfaces the error (void_result)

    let l3d = l3_slice();
    let e = ent.send_data(&l3d).unwrap_err(); // before link establishment
    assert_ne!(e.kind(), Some(ErrorKind::Ok));
    assert!(!e.msg.is_empty(), "C validation must arrive with a message");
}

fn l3_slice() -> Vec<u8> {
    vec![0x60, 0x0d, 0x00]
}

/// The closed-stack invariant: after `close()` (or after the value was already
/// torn down by an earlier close + a no-op Drop) EVERY method returns the
/// closed-class error without panicking — and structurally performs zero FFI:
/// the take-pattern nulled every handle before the closed flag went up, so no
/// code path from these methods can reach a raw pointer. Double-close / double-
/// drop is part of this test (RAII guards).
#[test]
fn stack_closed_no_ffi() {
    let mut st = GsmL3Stack::new(DEMO_TMSI, 0, 0, 0, true).expect("stack");

    // A live stack works end-to-end first (baseline that the handles are real).
    let l3 = build_cm_service_request(1, s::GSML3_ID_TMSI, DEMO_TMSI, None).unwrap();
    assert_eq!(st.send_frame(&ms_ui(&l3)).unwrap().len(), 1);

    st.close(); // explicit teardown (take-pattern)
    st.close(); // double close: no-op without panics or a second free

    // Every method is now the closed class (no FFI possible — see the docs):
    assert!(st.send_frame(&ms_ui(&l3)).err().unwrap().is_closed());
    assert!(st.feed_l3(&l3).err().unwrap().is_closed());
    assert!(st.build_response().err().unwrap().is_closed());
    assert!(st.send_ui(&l3, None).err().unwrap().is_closed());
    assert!(st.drain_l3_events().err().unwrap().is_closed());
    assert!(st.drain_tx_frames().err().unwrap().is_closed());
    assert!(st.tick_procedures(10).err().unwrap().is_closed());
    assert!(st.tick_timers(10, 4).err().unwrap().is_closed());
    assert!(st.take_retransmit().err().unwrap().is_closed());
    assert!(st.chain_phase().err().unwrap().is_closed());
    assert!(st.state().err().unwrap().is_closed());
    assert!(st.established().err().unwrap().is_closed());
    assert!(st.tick_t200(100).err().unwrap().is_closed());
    assert!(st.session_tmsi().err().unwrap().is_closed());
    assert!(st.assigned_tmsi().err().unwrap().is_closed());
    assert!(st.set_imsi("244051234567890").err().unwrap().is_closed());
    assert!(st.registered().err().unwrap().is_closed());
    assert!(st.authenticated().err().unwrap().is_closed());
    assert!(st.ciphered().err().unwrap().is_closed());
    // Bookkeeping accessors clear to None on close (no error class needed).
    assert!(st.last_step().is_none());
    assert!(st.last_event_error().is_none());

    drop(st); // Drop after double close: still a no-op (RAII guard, no panic)
}

/// Concurrency smoke mirroring C `TEST(..., Concurrent_IndependentEntities)`
/// and the Go `-race` test: 8 threads × 200 fully INDEPENDENT stacks (own
/// registry/entity each, unique non-zero TMSI), exercising construction →
/// frame exchange → teardown under real parallelism. One thread per handle —
/// nothing is shared, which is exactly the Send-but-not-Sync contract.
#[test]
fn concurrent_independent_stacks() {
    const THREADS: u32 = 8;
    const ITERS: u32 = 200;
    const L3_CHANNEL_RELEASE: &[u8] = &[0x60, 0x0d, 0x00]; // stable Channel Release vector (mirrors the C test suite)

    std::thread::scope(|scope| {
        for t in 0..THREADS {
            scope.spawn(move || {
                for i in 0..ITERS {
                    let tmsi = 0x4000_0000u32 | (t * ITERS + i); // unique, non-zero
                    let mut st = GsmL3Stack::new(tmsi, 0, 0, 0, false).expect("independent stack");
                    assert!(st.session_tmsi() == Ok(tmsi), "identity round-trips per stack");
                    // auto_response=false: the L3 event is queued, nothing is sent —
                    // a clean per-iteration FFI + queue cycle (drains both queues).
                    st.send_frame(&ms_ui(L3_CHANNEL_RELEASE)).expect("UI channel release frame");
                    let evs = st.drain_l3_events().expect("queued event present");
                    assert_eq!(evs.len(), 1);
                    st.close();
                }
            });
        }
    }); // all joins here — any panic/UB inside a thread fails the test
}

/// ABI guard: the compile-time surface test covers names and
/// types; at RUNTIME the linked library must still report the same C ABI
/// revision this header declares.
#[test]
fn abi_version_matches_header() {
    assert_eq!(s::ABI_VERSION, 1, "the Rust mirror of GSML3_ABI_VERSION");
    // SAFETY: no-argument stateless observer of the loaded library.
    assert_eq!(unsafe { s::gsml3_abi_version() }, s::ABI_VERSION, "the loaded library reports the same revision");
    assert_eq!(gsml3parser::abi_version(), 1);
}

/// End-to-end version check: the repo-root VERSION file is the single source of
/// truth — CMake stamps gsml3_version() from it (verified through the running
/// core here) and this crate's Cargo.toml literal must match. The unified gate
/// re-checks both sides on every run; this pins the equality in-tree.
#[test]
fn version_matches_root_version_file() {
    let manifest_dir = env!("CARGO_MANIFEST_DIR"); // bindings/rust/gsml3parser
    let root = Path::new(manifest_dir).ancestors().nth(3).expect("repo root");

    let on_disk = std::fs::read_to_string(root.join("VERSION"))
        .expect("root VERSION file exists")
        .trim()
        .to_string();
    assert!(
        on_disk.matches('.').count() == 2 && !on_disk.starts_with('v'),
        "VERSION must be a single semver X.Y.Z line (no 'v'): {on_disk:?}"
    );

    let from_core = gsml3parser::version();
    assert_eq!(from_core, on_disk, "gsml3_version() (CMake ← VERSION file) must equal the file");

    let crate_manifest = std::fs::read_to_string(Path::new(manifest_dir).join("Cargo.toml")).expect("Cargo.toml");
    assert!(
        crate_manifest.contains(&format!("version = \"{on_disk}\"")),
        "the Cargo manifest literal must match the root VERSION file (manifest version check)"
    );
}

/// Sharded-registry smoke + duplicate detection across all sharded flavors
/// ({4, 8, 16, 32} per the C header; plain flavor is covered by the unit tests):
/// 100 sessions each, lookup round-trip, count, DUPLICATE on a taken key.
#[test]
fn registry_sharded_smoke() {
    for shards in [4u8, 8, 16, 32] {
        let mut reg = Registry::new(shards as i32).expect("sharded registry constructs");
        assert_eq!(reg.shards() as u8, shards);
        for i in 0..100u32 {
            let tmsi = 0x5000_0000 | i;
            let sesh = reg.create_by_tmsi(tmsi).unwrap();
            assert_eq!(sesh.assigned_tmsi(), tmsi);
            let found = reg.find_by_tmsi(tmsi).expect("find");
            assert!(found.is_some());
        }
        assert_eq!(reg.count().unwrap(), 100);
        let e = reg.create_by_tmsi(0x5000_0001).unwrap_err(); // taken key → DUPLICATE (13)
        assert_eq!(e.kind(), Some(ErrorKind::Duplicate));
        reg.close(); // idempotent; sessions are freed on the C side with the registry
    }
}
