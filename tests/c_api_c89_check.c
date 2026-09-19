/* Copyright 2026 momentics <momentics@gmail.com>
 * Copyright libgsml3parser contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons for whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* C89 acceptance check of the C ABI (gsml3parser_c.h).
 *
 * This file must compile as strict C (MSVC /TC /W4 /WX, GCC
 * -std=c89 -pedantic-errors, Clang -std=c89) and link against the library.
 * It exercises every C API subsystem: versioning, L3 parse/metadata/
 * serialize/dump with exact sizing and error classes, input validation,
 * A-bis RSL parse/build round-trip, LAPDm frame decode plus a full entity
 * link lifecycle driven from C, the subscriber registry (including the
 * timer-expiry contract on an uninitialized buffer), and the procedure
 * orchestrator. Exit code 0 iff all checks pass.
 *
 * It also serves as the canonical reference for consuming the C API from C.
 *
 * C89 constraints observed: every declaration precedes the first statement
 * of its block; no // comments, no variable-length arrays, no compound
 * literals.
 */

#include <gsml3parser/gsml3parser_c.h>

#include <stdio.h>
#include <string.h>

static int failures = 0;

static void check(int cond, const char* what)
{
    if (!cond) {
        printf("FAIL: %s (error=%d %s)\n", what,
               gsml3_last_error_code(), gsml3_last_error());
        failures++;
    }
}

/* ── LAPDm capture callbacks (C function pointers) ─────────────────── */

typedef struct Capture {
    unsigned l3_count;
    int last_sapi;
    int last_prim;
    unsigned char last_data[64];
    unsigned last_data_len;
    unsigned tx_count;
    unsigned char last_tx[256];
    unsigned last_tx_len;
} Capture;

static void cL3Cb(int sapi, int primitive, const unsigned char* data,
                  size_t data_len, void* user)
{
    Capture* cap = (Capture*)user;

    cap->l3_count++;
    cap->last_sapi = sapi;
    cap->last_prim = primitive;
    if (data_len <= sizeof(cap->last_data)) {
        cap->last_data_len = (unsigned)data_len;
        memcpy(cap->last_data, data, cap->last_data_len);
    } else {
        cap->last_data_len = 0;
    }
}

static void cL1Cb(const unsigned char* frame, size_t frame_len, void* user)
{
    Capture* cap = (Capture*)user;

    cap->tx_count++;
    if (frame_len <= sizeof(cap->last_tx)) {
        cap->last_tx_len = (unsigned)frame_len;
        memcpy(cap->last_tx, frame, cap->last_tx_len);
    } else {
        cap->last_tx_len = 0;
    }
}

/* ── Versioning and the L3 core ─────────────────────────────────────── */

static void check_version_and_core(void)
{
    const char* v;
    gsml3_message* msg;
    gsml3_message* bad;
    uint8_t buf[256];
    size_t n;
    size_t want;
    char* hex;
    char* dump;

    v = gsml3_version();
    check(v != NULL && strlen(v) > 0, "version");
    check(gsml3_abi_version() == GSML3_ABI_VERSION, "abi version matches header");
    check(gsml3_last_error_code() == GSML3_OK && gsml3_last_error()[0] == '\0',
          "no pending error initially");

    /* RR Channel Release: PD=0x06, MTI=0x0D. */
    msg = gsml3_parse_l3_hex("60 0D 00", NULL);
    check(msg != NULL, "parse 60 0D 00");
    if (msg) {
        const char* name;

        name = gsml3_message_name(msg);
        check(name != NULL && strlen(name) > 0, "name");
        check(strcmp(name, "ChannelRelease") == 0, "name value");
        check(gsml3_message_pd(msg) == 6, "pd == 6");
        check(gsml3_message_mti(msg) == 13, "mti == 13");
        check(gsml3_message_ti(msg) == 0, "ti == 0");

        want = gsml3_message_size(msg);
        check(want == 3, "exact size is 3");

        n = gsml3_message_write(msg, buf, sizeof(buf));
        check(n == want && n == 3, "write 3 bytes");
        check(buf[0] == 0x60 && buf[1] == 0x0D && buf[2] == 0x00, "wire bytes");
        check(gsml3_last_error_code() == GSML3_OK, "success clears the error");

        /* The dedicated error class makes retry-with-larger-buffer trivial. */
        n = gsml3_message_write(msg, buf, 2);
        check(n == 0, "write into too-small buffer fails");
        check(gsml3_last_error_code() == GSML3_ERR_BUFFER_TOO_SMALL,
              "buffer-too-small error class");

        hex = gsml3_message_hex(msg);
        check(hex != NULL && strcmp(hex, "600d00") == 0, "hex round-trip");
        if (hex) gsml3_free(hex);

        dump = gsml3_message_dump(msg);
        check(dump != NULL && strstr(dump, "ChannelRelease") != NULL,
              "human-readable dump");
        if (dump) gsml3_free(dump);
        check(gsml3_message_dump(NULL) == NULL, "dump(NULL) is NULL");

        /* Reparse into the same handle (hot path). */
        check(gsml3_parse_l3_into(msg, buf, 3, NULL) == GSML3_OK, "parse_into");
        check(gsml3_message_mti(msg) == 13, "parse_into content");

        gsml3_message_free(msg);
    }

    /* Error path: truncated input (RR ChannelRelease without its cause).
     * A single byte would NOT fail here: one-octet frames parse as RR
     * ChannelRequest short messages. */
    bad = gsml3_parse_l3_hex("60 0D", NULL);
    check(bad == NULL, "truncated parse fails");
    check(gsml3_last_error_code() != GSML3_OK, "parse error code set");

    /* NULL safety. */
    check(gsml3_message_name(NULL) != NULL, "name(NULL)");
    check(gsml3_message_pd(NULL) == -1, "pd(NULL)");
    check(gsml3_message_size(NULL) == 0, "size(NULL)");
    gsml3_message_free(NULL);
    gsml3_free(NULL);
}

/* ── Input validation at the boundary ───────────────────────────────── */

static void check_validation(void)
{
    uint8_t buf[256];
    size_t n;

    /* Out-of-domain cause: rejected, no frame produced. */
    n = gsml3_build_channel_release(buf, sizeof(buf), 999);
    check(n == 0, "rr_cause out of range rejected");
    check(gsml3_last_error_code() == GSML3_ERR_INVALID_ARG,
          "invalid-arg error class");
    n = gsml3_response_build_channel_release(buf, sizeof(buf), 0);
    check(n > 0, "in-range cause accepted");
    check(gsml3_last_error_code() == GSML3_OK, "valid call clears the error");

    /* IMSI: ASCII digits only, at most 15. */
    n = gsml3_build_paging_response(buf, sizeof(buf), GSML3_ID_IMSI, 0,
                                    "244051234567890");
    check(n > 0, "15-digit IMSI accepted");
    n = gsml3_build_paging_response(buf, sizeof(buf), GSML3_ID_IMSI, 0,
                                    "2440512345678901");
    check(n == 0, "16-digit IMSI rejected");
    check(gsml3_last_error_code() == GSML3_ERR_INVALID_ARG,
          "long IMSI error class");
    n = gsml3_build_paging_response(buf, sizeof(buf), GSML3_ID_IMSI, 0,
                                    "24a05123456789");
    check(n == 0, "non-digit IMSI rejected");

    /* Called-party number: at most 20 digits, optional leading '+'. */
    n = gsml3_build_setup(buf, sizeof(buf), 3, "+4915112345678");
    check(n > 0, "international called number accepted");

    /* The reserved all-zero TMSI. */
    {
        gsml3_registry* r = gsml3_registry_new(0);

        check(r != NULL, "registry new");
        check(gsml3_registry_create_by_tmsi(r, 0) == NULL,
              "TMSI 0 rejected");
        check(gsml3_last_error_code() == GSML3_ERR_INVALID_ARG,
              "TMSI 0 error class");
        gsml3_registry_free(r);
    }
}

/* ── A-bis RSL (TS 48.058) ──────────────────────────────────────────── */

static void check_rsl(void)
{
    static const uint8_t l3[3] = {0x60, 0x0D, 0x00};
    uint8_t out[256];
    size_t n;
    gsml3_rsl* r;
    size_t l3len = 0;
    const uint8_t* l3v;
    uint8_t ietype;
    size_t ielen;
    const uint8_t* ieval;

    /* Build DATA_REQ (BCCH/TS2, link 1), parse it back, compare. */
    n = gsml3_rsl_build_data_req(out, sizeof(out), 0x7C, 1, l3, 3);
    check(n > 0, "rsl build data_req");
    r = gsml3_rsl_parse(out, n);
    check(r != NULL, "rsl parse");
    if (r) {
        const char* name;

        name = gsml3_rsl_name(r);
        check(name != NULL && strstr(name, "DATA") != NULL, "rsl name");
        check(gsml3_rsl_discriminator(r) == 0x00, "rsl RLL discriminator");
        check(gsml3_rsl_msg_type(r) == 0x21, "rsl DATA.req type");
        check(gsml3_rsl_chan_nr(r) == 0x7C, "rsl channel number");
        check(gsml3_rsl_link_id(r) == 1, "rsl link id");
        check(gsml3_rsl_bts_to_bsc(r) == 0, "rsl direction bit");

        l3v = gsml3_rsl_l3(r, &l3len);
        check(l3v != NULL && l3len == 3, "rsl L3 view length");
        if (l3v)
            check(memcmp(l3v, l3, 3) == 0, "rsl L3 view content");

        /* The IE view references the handle-owned copy. */
        check(gsml3_rsl_ie_count(r) >= 1, "rsl has at least one IE");
        if (gsml3_rsl_ie_get(r, 0, &ietype, &ielen, &ieval) == GSML3_OK)
            check(ieval != NULL || ielen == 0, "rsl IE view valid");

        check(gsml3_rsl_ie_get(r, 999, &ietype, &ielen, &ieval) != GSML3_OK,
              "rsl IE out of range rejected");

        /* Buffer-too-small uses the same dedicated class as the L3 path. */
        n = gsml3_rsl_build_data_req(out, 2, 0x7C, 1, l3, 3);
        check(n == 0 &&
              gsml3_last_error_code() == GSML3_ERR_BUFFER_TOO_SMALL,
              "rsl buffer too small class");

        gsml3_rsl_free(r);
    }

    check(gsml3_rsl_parse(NULL, 4) == NULL, "rsl parse(NULL) fails");
    gsml3_rsl_free(NULL);
}

/* ── LAPDm: frame decode and a full entity lifecycle driven from C ─── */

static void check_lapdm(void)
{
    static const uint8_t ua[2] = {0x01, 0x63};       /* UA, SAPI0, PF=1   */
    static const uint8_t ui[5] = {0x01, 0x03, 0x60, 0x0D, 0x00}; /* UI+SAPI0+L3 */
    gsml3_lapdm_frame_info f;
    gsml3_lapdm_entity* e;
    Capture cap;

    /* Decode a UI response frame from the peer (SAPI0, info follows). */
    check(gsml3_lapdm_frame_decode(ui, sizeof(ui), &f) == GSML3_OK,
          "lapdm decode UI frame");
    check(f.format == GSML3_LAPDM_FMT_U && f.u_type == GSML3_LAPDM_U_UI,
          "decoded UI type");
    check(f.info != NULL && f.info_len == 3, "decoded UI info");
    check(gsml3_lapdm_frame_decode(ui, 1, &f) != GSML3_OK,
          "lapdm truncated decode fails");

    /* Full lifecycle: open -> SABME -> UA -> established -> UI -> L3
     * callback -> DISC -> UA -> released. */
    memset(&cap, 0, sizeof(cap));
    /* channel profile: 0 = SDCCH (N201=20, N200=23, T200=900 ms) */
    e = gsml3_lapdm_entity_new(0, cL3Cb, cL1Cb, &cap);
    check(e != NULL, "entity new");
    check(gsml3_lapdm_entity_state(e) == GSML3_LAPDM_STATE_UNUSED, "unused state");

    gsml3_lapdm_entity_open(e, GSML3_SAPI0, 1); /* BTS side */
    check(gsml3_lapdm_entity_state(e) == GSML3_LAPDM_STATE_LINK_RELEASED,
          "released after open");

    check(gsml3_lapdm_entity_send_sabme(e) == GSML3_OK, "sabme sent");
    check(cap.tx_count == 1, "sabme via L1 callback");
    check(memcmp(cap.last_tx, "\x09\x2F", 2) == 0 && cap.last_tx_len == 2,
          "sabme wire bytes");

    /* An out-of-range SAPI is rejected before it can corrupt the frame. */
    {
        const uint8_t l3[3] = {0x60, 0x0D, 0x00};

        check(gsml3_lapdm_entity_send_ui(e, 20, l3, 3) == GSML3_ERR_INVALID_ARG,
              "send_ui rejects SAPI > 15");
        check(gsml3_last_error_code() == GSML3_ERR_INVALID_ARG,
              "sapi error class");
    }

    gsml3_lapdm_entity_receive(e, ua, sizeof(ua));
    check(gsml3_lapdm_entity_state(e) == GSML3_LAPDM_STATE_LINK_ESTABLISHED,
          "established after UA");
    check(gsml3_lapdm_entity_is_established(e) == 1, "is established");
    check(cap.l3_count == 1, "establish confirm callback fired");

    gsml3_lapdm_entity_receive(e, ui, sizeof(ui));
    check(cap.l3_count == 2, "unit data callback fired");
    check(cap.last_sapi == GSML3_SAPI0, "l3 callback sapi");
    check(cap.last_prim == GSML3_PRIM_L3_UNIT_DATA, "l3 primitive is unit data");
    check(cap.last_data_len == 3 && memcmp(cap.last_data, "\x60\x0D\x00", 3) == 0,
          "l3 payload round-trip");

    check(gsml3_lapdm_entity_send_disc(e) == GSML3_OK, "disc sent");
    check(gsml3_lapdm_entity_state(e) == GSML3_LAPDM_STATE_AWAITING_RELEASE,
          "awaiting release after DISC");
    gsml3_lapdm_entity_receive(e, ua, sizeof(ua));
    check(gsml3_lapdm_entity_state(e) == GSML3_LAPDM_STATE_LINK_RELEASED,
          "released after second UA");

    /* T200 tick reports a retransmission as 1 (SAPI0 SABME was outstanding
     * before release; use a fresh entity for a deterministic check). */
    {
        gsml3_lapdm_entity* e2 = gsml3_lapdm_entity_new(0, cL3Cb, cL1Cb, &cap);

        check(e2 != NULL, "second entity");
        gsml3_lapdm_entity_open(e2, GSML3_SAPI0, 1);
        check(gsml3_lapdm_entity_send_sabme(e2) == GSML3_OK, "second sabme");
        check(gsml3_lapdm_entity_tick_t200(e2, 900) == 1,
              "T200 expiry retransmits");
        gsml3_lapdm_entity_free(e2);
    }

    gsml3_lapdm_entity_free(e);

    /* NULL safety. */
    gsml3_lapdm_entity_free(NULL);
    check(gsml3_lapdm_entity_state(NULL) == GSML3_LAPDM_STATE_UNUSED,
          "entity state(NULL)");
    gsml3_lapdm_entity_open(NULL, 0, 1);
    gsml3_lapdm_entity_receive(NULL, NULL, 0);
    check(gsml3_lapdm_entity_send_sabme(NULL) != GSML3_OK,
          "send_sabme(NULL) fails");
}

/* ── Registry, sessions, timers ─────────────────────────────────────── */

static void check_registry(void)
{
    gsml3_registry* r;
    gsml3_session* s;
    gsml3_timer_expiry ev[4];
    size_t n;
    uint32_t autoTmsi;

    r = gsml3_registry_new(0);
    check(r != NULL, "registry plain new");

    s = gsml3_registry_create_by_tmsi(r, 0x1234);
    check(s != NULL, "session create by TMSI");
    check(gsml3_session_tmsi(s) == 0x1234u, "session tmsi getter");
    check(gsml3_registry_find_by_tmsi(r, 0x1234) == s, "find by TMSI");
    check(gsml3_session_assigned_tmsi(s) == 0x1234u, "assigned tmsi == key");

    /* An IMSI-keyed session: identity stays IMSI, the registry assigns a
     * TMSI that the C API exposes via assigned_tmsi. */
    {
        gsml3_session* si = gsml3_registry_create_by_imsi(r, "244051234567890");

        check(si != NULL, "session create by IMSI");
        if (si) {
            check(gsml3_session_tmsi(si) == 0u, "IMSI identity is not a TMSI");
            autoTmsi = gsml3_session_assigned_tmsi(si);
            check(autoTmsi != 0u, "auto-assigned TMSI readable");
            check(gsml3_registry_find_by_tmsi(r, autoTmsi) == si,
                  "find IMSI session by auto TMSI");
        }
    }

    /* The timer-expiry contract on a buffer the C caller did not
     * zero-initialize: every byte of each event must be defined. */
    for (n = 0; n < sizeof(ev); n++)
        ((unsigned char*)ev)[n] = 0xFF;
    check(gsml3_session_timer_start(s, GSML3_TIMER_T3101) == 1, "timer start");
    n = gsml3_registry_tick_timers(r, 3000, ev, sizeof(ev) / sizeof(ev[0]));
    check(n >= 1, "tick reports at least one expiry");
    if (n >= 1) {
        int found = 0;
        size_t k;

        for (k = 0; k < n; k++)
            if (ev[k].session == s && ev[k].timer_id == GSML3_TIMER_T3101)
                found = 1;
        check(found, "expiry entry fully defined (session + timer id)");
    }

    /* Out-of-domain timer ids fail cleanly. */
    check(gsml3_session_timer_start(s, 999) == 0, "bad timer id rejected");
    check(gsml3_last_error_code() == GSML3_ERR_INVALID_ARG,
          "timer id error class");

    /* Channel assignment through the registry link index. */
    {
        gsml3_session* byLink;

        gsml3_registry_assign_channel(
            r, s, 1 /*SACCH*/, 3 /*trx*/, 2 /*ts*/, 5120 /*arfcn*/,
            0 /*lapdm_link*/);
        byLink = gsml3_registry_find_by_link(r, 3 /*trx*/, 2 /*ts*/,
                                             0 /*lapdm link*/);
        check(byLink == s, "find session by channel link");
        gsml3_registry_release_channel(r, s);
    }

    check(gsml3_registry_remove(r, s) == 1, "remove session");
    check(gsml3_registry_find_by_tmsi(r, 0x1234) == NULL, "removed session gone");

    /* Invalid shard counts. */
    {
        int bad;

        for (bad = -1; bad <= 33; bad++) {
            gsml3_registry* rb = gsml3_registry_new(bad);
            int valid = (bad == 0 || bad == 4 || bad == 8 || bad == 16 ||
                         bad == 32);

            if (valid)
                check(rb != NULL, "valid shard count accepted");
            else
                check(rb == NULL &&
                      gsml3_last_error_code() == GSML3_ERR_INVALID_ARG,
                      "invalid shard count rejected");
            gsml3_registry_free(rb);
        }
    }

    check(gsml3_session_tmsi(NULL) == 0u, "session getters are NULL-safe");
    gsml3_registry_free(NULL);
    gsml3_registry_free(r);
}

/* ── Procedure orchestrator ─────────────────────────────────────────── */

static void check_orchestrator(void)
{
    gsml3_orchestrator* o;
    gsml3_message* m;
    gsml3_step_result res;

    o = gsml3_orchestrator_new();
    m = gsml3_parse_l3_hex("60 0D 00", NULL); /* no chain start */
    check(o != NULL && m != NULL, "orchestrator and message new");

    /* A failed call is visible through the result's error field (the session
     * argument is optional and may be NULL). */
    res = gsml3_orchestrator_feed(NULL, m, NULL);
    check(res.error == GSML3_ERR_INVALID_ARG, "feed(NULL orchestrator) error");

    /* A real step reports GSML3_OK in error; ChannelRelease does not start a
     * chain, so the action stays CONTINUE. */
    res = gsml3_orchestrator_feed(o, m, NULL);
    check(res.error == GSML3_OK, "feed success error field");
    check(res.action == GSML3_ACTION_CONTINUE, "continue on non-chain input");
    check(res.response_token == GSML3_TOKEN_NONE, "no pending token");

    /* Out-of-domain paging trigger channel. */
    res = gsml3_orchestrator_feed_paging_trigger(o, GSML3_ID_TMSI, 0x1, NULL, 99);
    check(res.error == GSML3_ERR_INVALID_ARG, "bad target channel error");

    {
        uint8_t out[256];
        size_t n;

        /* No pending token -> build fails cleanly (0 bytes). */
        n = gsml3_orchestrator_build_response(o, NULL, out, sizeof(out));
        check(n == 0, "no pending response");
        check(gsml3_orchestrator_take_retransmit(o) == GSML3_TOKEN_NONE,
              "retransmit channel empty");
    }

    gsml3_message_free(m);
    gsml3_orchestrator_cancel_all(o);
    gsml3_orchestrator_free(o);
}

int main(void)
{
    check_version_and_core();
    check_validation();
    check_rsl();
    check_lapdm();
    check_registry();
    check_orchestrator();

    if (failures == 0) {
        printf("c_api_c89_check: OK\n");
        return 0;
    }
    printf("c_api_c89_check: %d failure(s)\n", failures);
    return 1;
}
