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

//! Compile-time completeness check for the v1 FFI surface (planK decision #6):
//! every one of the 128 functions (S1–S7 + `gsml3_build_cm_service_request` +
//! `gsml3_build_setup`) must be declared in the extern block of this crate with
//! the exact C ABI signature. Each assignment below names the function item and
//! ascribes its FULL pointer type, so a missing name, a renamed function, or a
//! single drifted argument/return type is a compile error — no bindgen, no code
//! generation, no runtime enumeration needed. The runtime counterpart is
//! `abi_version_matches_header` in the safe crate's integration tests.

use std::os::raw::{c_char, c_int, c_uchar, c_void};

use gsml3parser_sys::{
    gsml3_config, gsml3_lapdm_entity, gsml3_lapdm_frame_info, gsml3_message, gsml3_orchestrator,
    gsml3_registry, gsml3_rsl, gsml3_session, gsml3_step_result, gsml3_timer_expiry, lapdm_l1_cb, lapdm_l3_cb,
};

/// All 128 names, each with its full `extern "C"` function-pointer type: the
/// compiler checks existence AND exact signature of every v1-surface function.
fn _use_all_128() {
    // ── S1 Core (5) ────────────────────────────────────────────────────────
    let _: unsafe extern "C" fn() -> *const c_char = gsml3parser_sys::gsml3_version;
    let _: unsafe extern "C" fn() -> u32 = gsml3parser_sys::gsml3_abi_version;
    let _: unsafe extern "C" fn() -> *const c_char = gsml3parser_sys::gsml3_last_error;
    let _: unsafe extern "C" fn() -> c_int = gsml3parser_sys::gsml3_last_error_code;
    let _: unsafe extern "C" fn(*mut c_void) = gsml3parser_sys::gsml3_free;

    // ── S2 Config (4) ──────────────────────────────────────────────────────
    let _: unsafe extern "C" fn() -> *mut gsml3_config = gsml3parser_sys::gsml3_config_new;
    let _: unsafe extern "C" fn(*mut gsml3_config, c_int) = gsml3parser_sys::gsml3_config_set_log_level;
    let _: unsafe extern "C" fn(*mut gsml3_config, c_int) =
        gsml3parser_sys::gsml3_config_set_strict_framing;
    let _: unsafe extern "C" fn(*mut gsml3_config) = gsml3parser_sys::gsml3_config_free;

    // ── S3 Message (12) ────────────────────────────────────────────────────
    let _: unsafe extern "C" fn(
        *const c_uchar,
        usize,
        *const gsml3_config,
    ) -> *mut gsml3_message = gsml3parser_sys::gsml3_parse_l3;
    let _: unsafe extern "C" fn(*const c_char, *const gsml3_config) -> *mut gsml3_message =
        gsml3parser_sys::gsml3_parse_l3_hex;
    let _: unsafe extern "C" fn(
        *mut gsml3_message,
        *const c_uchar,
        usize,
        *const gsml3_config,
    ) -> c_int = gsml3parser_sys::gsml3_parse_l3_into;
    let _: unsafe extern "C" fn(*mut gsml3_message) = gsml3parser_sys::gsml3_message_free;
    let _: unsafe extern "C" fn(*const gsml3_message) -> *const c_char = gsml3parser_sys::gsml3_message_name;
    let _: unsafe extern "C" fn(*const gsml3_message) -> c_int = gsml3parser_sys::gsml3_message_pd;
    let _: unsafe extern "C" fn(*const gsml3_message) -> c_int = gsml3parser_sys::gsml3_message_mti;
    let _: unsafe extern "C" fn(*const gsml3_message) -> c_int = gsml3parser_sys::gsml3_message_ti;
    let _: unsafe extern "C" fn(*const gsml3_message) -> usize = gsml3parser_sys::gsml3_message_size;
    let _: unsafe extern "C" fn(
        *const gsml3_message,
        *mut c_uchar,
        usize,
    ) -> usize = gsml3parser_sys::gsml3_message_write;
    let _: unsafe extern "C" fn(*const gsml3_message) -> *mut c_char = gsml3parser_sys::gsml3_message_hex;
    let _: unsafe extern "C" fn(*const gsml3_message) -> *mut c_char = gsml3parser_sys::gsml3_message_dump;

    // ── S4 RSL (25) ────────────────────────────────────────────────────────
    let _: unsafe extern "C" fn(*const c_uchar, usize) -> *mut gsml3_rsl = gsml3parser_sys::gsml3_rsl_parse;
    let _: unsafe extern "C" fn(*mut gsml3_rsl) = gsml3parser_sys::gsml3_rsl_free;
    let _: unsafe extern "C" fn(*const gsml3_rsl) -> *const c_char = gsml3parser_sys::gsml3_rsl_name;
    let _: unsafe extern "C" fn(*const gsml3_rsl) -> c_int = gsml3parser_sys::gsml3_rsl_discriminator;
    let _: unsafe extern "C" fn(*const gsml3_rsl) -> c_int = gsml3parser_sys::gsml3_rsl_msg_type;
    let _: unsafe extern "C" fn(*const gsml3_rsl) -> c_int = gsml3parser_sys::gsml3_rsl_chan_nr;
    let _: unsafe extern "C" fn(*const gsml3_rsl) -> c_int = gsml3parser_sys::gsml3_rsl_link_id;
    let _: unsafe extern "C" fn(*const gsml3_rsl) -> c_int = gsml3parser_sys::gsml3_rsl_bts_to_bsc;
    let _: unsafe extern "C" fn(*const gsml3_rsl) -> c_int = gsml3parser_sys::gsml3_rsl_has_l3;
    let _: unsafe extern "C" fn(*const gsml3_rsl, *mut usize) -> *const c_uchar = gsml3parser_sys::gsml3_rsl_l3;
    let _: unsafe extern "C" fn(*const gsml3_rsl) -> usize = gsml3parser_sys::gsml3_rsl_ie_count;
    let _: unsafe extern "C" fn(
        *const gsml3_rsl,
        usize,
        *mut c_uchar,
        *mut usize,
        *mut *const c_uchar,
    ) -> c_int = gsml3parser_sys::gsml3_rsl_ie_get;
    // 13 RSL builders (out-buffer first; bytes written as size_t/usize).
    let _: unsafe extern "C" fn(
        *mut c_uchar,
        usize,
        c_uchar,
        c_uchar,
        *const c_uchar,
        usize,
    ) -> usize = gsml3parser_sys::gsml3_rsl_build_data_req;
    let _: unsafe extern "C" fn(
        *mut c_uchar,
        usize,
        c_uchar,
        c_uchar,
        *const c_uchar,
        usize,
    ) -> usize = gsml3parser_sys::gsml3_rsl_build_data_ind;
    let _: unsafe extern "C" fn(
        *mut c_uchar,
        usize,
        c_uchar,
        c_uchar,
        *const c_uchar,
        usize,
    ) -> usize = gsml3parser_sys::gsml3_rsl_build_unit_data_req;
    let _: unsafe extern "C" fn(
        *mut c_uchar,
        usize,
        c_uchar,
        c_uchar,
        *const c_uchar,
        usize,
    ) -> usize = gsml3parser_sys::gsml3_rsl_build_unit_data_ind;
    let _: unsafe extern "C" fn(
        *mut c_uchar,
        usize,
        c_uchar,
        u16,
    ) -> usize = gsml3parser_sys::gsml3_rsl_build_chan_activ_ack;
    let _: unsafe extern "C" fn(
        *mut c_uchar,
        usize,
        c_uchar,
        c_int,
    ) -> usize = gsml3parser_sys::gsml3_rsl_build_chan_activ_nack;
    let _: unsafe extern "C" fn(
        *mut c_uchar,
        usize,
        c_uchar,
    ) -> usize = gsml3parser_sys::gsml3_rsl_build_rf_chan_rel_ack;
    let _: unsafe extern "C" fn(*mut c_uchar, usize, c_uchar, c_int) -> usize =
        gsml3parser_sys::gsml3_rsl_build_conn_fail;
    let _: unsafe extern "C" fn(
        *mut c_uchar,
        usize,
        c_uchar,
        c_uchar,
        i8,
        i8,
        *const c_uchar,
        usize,
    ) -> usize = gsml3parser_sys::gsml3_rsl_build_meas_res;
    let _: unsafe extern "C" fn(
        *mut c_uchar,
        usize,
        c_uchar,
        c_uchar,
    ) -> usize = gsml3parser_sys::gsml3_rsl_build_hando_det;
    let _: unsafe extern "C" fn(
        *mut c_uchar,
        usize,
        c_uchar,
        u16,
        u16,
        u16,
        u16,
    ) -> usize = gsml3parser_sys::gsml3_rsl_build_ccch_load_ind;
    let _: unsafe extern "C" fn(
        *mut c_uchar,
        usize,
        c_uchar,
        c_uchar,
        c_uchar,
        c_uchar,
        c_uchar,
        c_uchar,
    ) -> usize = gsml3parser_sys::gsml3_rsl_build_chan_rqd;
    let _: unsafe extern "C" fn(
        *mut c_uchar,
        usize,
        c_uchar,
        *const c_uchar,
        usize,
    ) -> usize = gsml3parser_sys::gsml3_rsl_build_delete_ind;

    // ── S5 LAPDm (16) ──────────────────────────────────────────────────────
    let _: unsafe extern "C" fn(
        *const c_uchar,
        usize,
        *mut gsml3_lapdm_frame_info,
    ) -> c_int = gsml3parser_sys::gsml3_lapdm_frame_decode;
    let _: unsafe extern "C" fn(
        c_int,
        Option<lapdm_l3_cb>,
        Option<lapdm_l1_cb>,
        *mut c_void,
    ) -> *mut gsml3_lapdm_entity = gsml3parser_sys::gsml3_lapdm_entity_new;
    let _: unsafe extern "C" fn(*mut gsml3_lapdm_entity) = gsml3parser_sys::gsml3_lapdm_entity_free;
    let _: unsafe extern "C" fn(
        *mut gsml3_lapdm_entity,
        c_int,
        c_int,
    ) = gsml3parser_sys::gsml3_lapdm_entity_open;
    let _: unsafe extern "C" fn(
        *mut gsml3_lapdm_entity,
        *const c_uchar,
        usize,
    ) = gsml3parser_sys::gsml3_lapdm_entity_receive;
    let _: unsafe extern "C" fn(
        *mut gsml3_lapdm_entity,
        c_int,
        *const c_uchar,
        usize,
    ) -> c_int = gsml3parser_sys::gsml3_lapdm_entity_send_ui;
    let _: unsafe extern "C" fn(
        *mut gsml3_lapdm_entity,
        *const c_uchar,
        usize,
    ) -> c_int = gsml3parser_sys::gsml3_lapdm_entity_send_data;
    let _: unsafe extern "C" fn(*mut gsml3_lapdm_entity) -> c_int =
        gsml3parser_sys::gsml3_lapdm_entity_send_sabme;
    let _: unsafe extern "C" fn(*mut gsml3_lapdm_entity) -> c_int =
        gsml3parser_sys::gsml3_lapdm_entity_send_disc;
    let _: unsafe extern "C" fn(*mut gsml3_lapdm_entity) =
        gsml3parser_sys::gsml3_lapdm_entity_hard_release;
    let _: unsafe extern "C" fn(
        *mut gsml3_lapdm_entity,
        u32,
    ) -> c_int = gsml3parser_sys::gsml3_lapdm_entity_tick_t200;
    let _: unsafe extern "C" fn(*const gsml3_lapdm_entity) -> c_int =
        gsml3parser_sys::gsml3_lapdm_entity_state;
    let _: unsafe extern "C" fn(*const gsml3_lapdm_entity) -> c_int =
        gsml3parser_sys::gsml3_lapdm_entity_is_established;
    let _: unsafe extern "C" fn(*const gsml3_lapdm_entity) -> u32 =
        gsml3parser_sys::gsml3_lapdm_entity_frames_sent;
    let _: unsafe extern "C" fn(*const gsml3_lapdm_entity) -> u32 =
        gsml3parser_sys::gsml3_lapdm_entity_frames_received;
    let _: unsafe extern "C" fn(*const gsml3_lapdm_entity) -> u32 =
        gsml3parser_sys::gsml3_lapdm_entity_retransmissions;

    // ── S6 Registry + Session (29) ─────────────────────────────────────────
    let _: unsafe extern "C" fn(c_int) -> *mut gsml3_registry = gsml3parser_sys::gsml3_registry_new;
    let _: unsafe extern "C" fn(*mut gsml3_registry) = gsml3parser_sys::gsml3_registry_free;
    let _: unsafe extern "C" fn(
        *mut gsml3_registry,
        usize,
    ) = gsml3parser_sys::gsml3_registry_reserve;
    let _: unsafe extern "C" fn(*const gsml3_registry) -> usize = gsml3parser_sys::gsml3_registry_count;
    let _: unsafe extern "C" fn(
        *mut gsml3_registry,
        u32,
    ) -> *mut gsml3_session = gsml3parser_sys::gsml3_registry_create_by_tmsi;
    let _: unsafe extern "C" fn(
        *mut gsml3_registry,
        *const c_char,
    ) -> *mut gsml3_session = gsml3parser_sys::gsml3_registry_create_by_imsi;
    let _: unsafe extern "C" fn(*mut gsml3_registry, u32) -> *mut gsml3_session =
        gsml3parser_sys::gsml3_registry_find_by_tmsi;
    let _: unsafe extern "C" fn(*mut gsml3_registry, *const c_char) -> *mut gsml3_session =
        gsml3parser_sys::gsml3_registry_find_by_imsi;
    let _: unsafe extern "C" fn(
        *mut gsml3_registry,
        c_uchar,
        c_uchar,
        c_uchar,
    ) -> *mut gsml3_session = gsml3parser_sys::gsml3_registry_find_by_link;
    let _: unsafe extern "C" fn(*mut gsml3_registry, *mut gsml3_session) -> c_int =
        gsml3parser_sys::gsml3_registry_remove;
    let _: unsafe extern "C" fn(*mut gsml3_registry) = gsml3parser_sys::gsml3_registry_clear;
    let _: unsafe extern "C" fn(
        *mut gsml3_registry,
        *mut gsml3_session,
        c_int,
        c_uchar,
        c_uchar,
        u16,
        c_uchar,
    ) = gsml3parser_sys::gsml3_registry_assign_channel;
    let _: unsafe extern "C" fn(
        *mut gsml3_registry,
        *mut gsml3_session,
    ) = gsml3parser_sys::gsml3_registry_release_channel;
    let _: unsafe extern "C" fn(
        *mut gsml3_registry,
        u32,
        *mut gsml3_timer_expiry,
        usize,
    ) -> usize = gsml3parser_sys::gsml3_registry_tick_timers;
    let _: unsafe extern "C" fn(
        *mut gsml3_registry,
        u32,
    ) -> usize = gsml3parser_sys::gsml3_registry_tick_procedures;

    // Session accessors (BORROWED handle; no free function exists).
    let _: unsafe extern "C" fn(*mut gsml3_session) -> u32 = gsml3parser_sys::gsml3_session_tmsi;
    let _: unsafe extern "C" fn(*mut gsml3_session) -> u32 = gsml3parser_sys::gsml3_session_assigned_tmsi;
    let _: unsafe extern "C" fn(
        *mut gsml3_session,
        u32,
    ) = gsml3parser_sys::gsml3_session_set_tmsi;
    let _: unsafe extern "C" fn(
        *mut gsml3_session,
        *const c_char,
    ) = gsml3parser_sys::gsml3_session_set_imsi;
    let _: unsafe extern "C" fn(*mut gsml3_session) -> c_int =
        gsml3parser_sys::gsml3_session_is_registered;
    let _: unsafe extern "C" fn(*mut gsml3_session, c_int) = gsml3parser_sys::gsml3_session_set_registered;
    let _: unsafe extern "C" fn(*mut gsml3_session) -> c_int =
        gsml3parser_sys::gsml3_session_is_authenticated;
    let _: unsafe extern "C" fn(
        *mut gsml3_session,
        c_int,
    ) = gsml3parser_sys::gsml3_session_set_authenticated;
    let _: unsafe extern "C" fn(*mut gsml3_session) -> c_int = gsml3parser_sys::gsml3_session_is_ciphered;
    let _: unsafe extern "C" fn(*mut gsml3_session, c_int) = gsml3parser_sys::gsml3_session_set_ciphered;
    let _: unsafe extern "C" fn(
        *mut gsml3_session,
        c_int,
    ) -> c_int = gsml3parser_sys::gsml3_session_timer_start;
    let _: unsafe extern "C" fn(
        *mut gsml3_session,
        c_int,
    ) = gsml3parser_sys::gsml3_session_timer_stop;
    let _: unsafe extern "C" fn(*mut gsml3_session, c_int) -> c_int =
        gsml3parser_sys::gsml3_session_timer_running;
    let _: unsafe extern "C" fn(*mut gsml3_session) -> usize =
        gsml3parser_sys::gsml3_session_transaction_pending;

    // ── S7 Orchestrator + Responses (35) ───────────────────────────────────
    let _: unsafe extern "C" fn() -> *mut gsml3_orchestrator = gsml3parser_sys::gsml3_orchestrator_new;
    let _: unsafe extern "C" fn(*mut gsml3_orchestrator) = gsml3parser_sys::gsml3_orchestrator_free;
    let _: unsafe extern "C" fn(
        *mut gsml3_orchestrator,
        *const gsml3_message,
        *mut gsml3_session,
    ) -> gsml3_step_result = gsml3parser_sys::gsml3_orchestrator_feed;
    let _: unsafe extern "C" fn(
        *mut gsml3_orchestrator,
        *const c_uchar,
        *const c_uchar,
    ) -> gsml3_step_result = gsml3parser_sys::gsml3_orchestrator_feed_auth_challenge;
    let _: unsafe extern "C" fn(
        *mut gsml3_orchestrator,
        c_int,
        c_int,
        u32,
        c_int,
    ) -> gsml3_step_result = gsml3parser_sys::gsml3_orchestrator_feed_vlr_decision;
    let _: unsafe extern "C" fn(
        *mut gsml3_orchestrator,
        c_uchar,
        c_int,
    ) -> gsml3_step_result = gsml3parser_sys::gsml3_orchestrator_feed_ciphering;
    let _: unsafe extern "C" fn(
        *mut gsml3_orchestrator,
        c_int,
        u32,
        *const c_char,
        c_int,
    ) -> gsml3_step_result = gsml3parser_sys::gsml3_orchestrator_feed_paging_trigger;
    let _: unsafe extern "C" fn(*mut gsml3_orchestrator, u32) -> usize =
        gsml3parser_sys::gsml3_orchestrator_tick;
    let _: unsafe extern "C" fn(
        *mut gsml3_orchestrator,
        *const gsml3_session,
        *mut c_uchar,
        usize,
    ) -> usize = gsml3parser_sys::gsml3_orchestrator_build_response;
    let _: unsafe extern "C" fn(
        *const gsml3_orchestrator,
        *const gsml3_session,
    ) -> usize = gsml3parser_sys::gsml3_orchestrator_required_size;
    let _: unsafe extern "C" fn(*mut gsml3_orchestrator) -> c_int =
        gsml3parser_sys::gsml3_orchestrator_take_retransmit;
    let _: unsafe extern "C" fn(*mut gsml3_orchestrator) = gsml3parser_sys::gsml3_orchestrator_cancel_all;
    let _: unsafe extern "C" fn(*const gsml3_orchestrator) -> c_int =
        gsml3parser_sys::gsml3_orchestrator_chain_phase;
    // Response builders (token-driven pair + 20 stateless).
    let _: unsafe extern "C" fn(
        c_int,
        *const gsml3_session,
        *mut c_uchar,
        usize,
    ) -> usize = gsml3parser_sys::gsml3_response_build_from_token;
    let _: unsafe extern "C" fn(
        c_int,
        *const gsml3_session,
    ) -> usize = gsml3parser_sys::gsml3_response_required_size;
    let _: unsafe extern "C" fn(
        *mut c_uchar,
        usize,
    ) -> usize = gsml3parser_sys::gsml3_response_build_cm_service_accept;
    let _: unsafe extern "C" fn(
        *mut c_uchar,
        usize,
        c_int,
    ) -> usize = gsml3parser_sys::gsml3_response_build_cm_service_reject;
    let _: unsafe extern "C" fn(
        *mut c_uchar,
        usize,
        c_int,
    ) -> usize = gsml3parser_sys::gsml3_response_build_identity_request;
    let _: unsafe extern "C" fn(
        *mut c_uchar,
        usize,
        *const c_uchar,
    ) -> usize = gsml3parser_sys::gsml3_response_build_authentication_request;
    // S7 LAI builders: digit STRINGS for mcc/mnc ("244" / "05").
    let _: unsafe extern "C" fn(
        *mut c_uchar,
        usize,
        *const c_char,
        *const c_char,
        u16,
        c_int,
        u32,
    ) -> usize = gsml3parser_sys::gsml3_response_build_location_updating_accept;
    let _: unsafe extern "C" fn(
        *mut c_uchar,
        usize,
        c_int,
    ) -> usize = gsml3parser_sys::gsml3_response_build_location_updating_reject;
    let _: unsafe extern "C" fn(
        *mut c_uchar,
        usize,
        *const c_char,
        *const c_char,
        u16,
        u32,
    ) -> usize = gsml3parser_sys::gsml3_response_build_tmsi_reallocation_command;
    let _: unsafe extern "C" fn(*mut c_uchar, usize, c_int) -> usize =
        gsml3parser_sys::gsml3_response_build_channel_release;
    let _: unsafe extern "C" fn(
        *mut c_uchar,
        usize,
        c_uchar,
    ) -> usize = gsml3parser_sys::gsml3_response_build_ciphering_mode_command;
    let _: unsafe extern "C" fn(*mut c_uchar, usize, c_uchar) -> usize =
        gsml3parser_sys::gsml3_response_build_physical_information;
    let _: unsafe extern "C" fn(
        *mut c_uchar,
        usize,
        c_int,
        c_uchar,
        c_uchar,
        u16,
        c_uchar,
    ) -> usize = gsml3parser_sys::gsml3_response_build_immediate_assignment;
    let _: unsafe extern "C" fn(
        *mut c_uchar,
        usize,
        c_int,
        c_uchar,
        c_uchar,
        u16,
    ) -> usize = gsml3parser_sys::gsml3_response_build_assignment_command;
    let _: unsafe extern "C" fn(
        *mut c_uchar,
        usize,
        c_uchar,
    ) -> usize = gsml3parser_sys::gsml3_response_build_call_proceeding;
    let _: unsafe extern "C" fn(*mut c_uchar, usize, c_uchar) -> usize =
        gsml3parser_sys::gsml3_response_build_alerting;
    let _: unsafe extern "C" fn(*mut c_uchar, usize, c_uchar) -> usize = gsml3parser_sys::gsml3_response_build_connect;
    let _: unsafe extern "C" fn(*mut c_uchar, usize, c_uchar) -> usize =
        gsml3parser_sys::gsml3_response_build_connect_acknowledge;
    let _: unsafe extern "C" fn(
        *mut c_uchar,
        usize,
        c_uchar,
        c_int,
    ) -> usize = gsml3parser_sys::gsml3_response_build_disconnect;
    let _: unsafe extern "C" fn(
        *mut c_uchar,
        usize,
        c_uchar,
        c_int,
    ) -> usize = gsml3parser_sys::gsml3_response_build_release;
    let _: unsafe extern "C" fn(
        *mut c_uchar,
        usize,
        c_uchar,
    ) -> usize = gsml3parser_sys::gsml3_response_build_release_complete;
    // NOTE: S7 setup takes (called_digits, ti) — the opposite order of S9 build_setup.
    let _: unsafe extern "C" fn(
        *mut c_uchar,
        usize,
        *const c_char,
        c_uchar,
    ) -> usize = gsml3parser_sys::gsml3_response_build_setup;

    // ── Curated S9 builders (2) ────────────────────────────────────────────
    let _: unsafe extern "C" fn(
        *mut c_uchar,
        usize,
        c_int,
        c_int,
        u32,
        *const c_char,
    ) -> usize = gsml3parser_sys::gsml3_build_cm_service_request;
    let _: unsafe extern "C" fn(
        *mut c_uchar,
        usize,
        c_uchar,
        *const c_char,
    ) -> usize = gsml3parser_sys::gsml3_build_setup;
}

/// The surface check itself. Running `_use_all_128()` has no runtime effect —
/// the guarantee is at COMPILE time: if any of the 128 declarations in this
/// crate's extern block is missing or its type has drifted from the C header,
/// this test binary fails to build. (Runtime counterpart:
/// `abi_version_matches_header` in the safe crate.)
#[test]
fn test_abi_surface() {
    _use_all_128();
}
