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

package gsml3parser

// Stack simulation tests: MO call
// over SDCCH with C-core callbacks and orchestration, LAPDm link lifecycle
// (byte-exact vectors mirroring the C Entity_LinkLifecycle test), T200
// retransmission, wrapper NULL validation, the closed-path NO-FFI invariant
// (proven through the fixed FFI-call seam in lapdm.go) and independent-stack
// concurrency under the -race detector. Vectors are stable and builder-generated;
// expected tokens/phases/timers mirror tests/test_c_api.cpp.

import (
	"bytes"
	"sync"
	"testing"
)

func requireNoErr(t *testing.T, op string, err error) {
	t.Helper()
	if err != nil {
		t.Fatalf("%s: %v", op, err)
	}
}

// ── MO call over SDCCH — manual orchestration (proves every surface step) ────

func TestMoCallSimulation(t *testing.T) {
	stack, err := NewGsmL3Stack(StackOptions{TMSI: demoTMSI, AutoResponse: false})
	if err != nil {
		t.Fatalf("NewGsmL3Stack: %v", err)
	}
	defer stack.Close()

	tmsi, err := stack.TMSI()
	if err != nil || tmsi != demoTMSI {
		t.Fatalf("stack.TMSI() = (%d, %v), want (%#x, nil)", tmsi, err, demoTMSI)
	}

	// -- [1] CMServiceRequest (MO call) -> CM_SERVICE_ACCEPT(10) ---------------
	l3, err := BuildCMServiceRequest(1, IDTMSI, demoTMSI, "") // service_type 1 = MobileOriginatedCall
	requireNoErr(t, "BuildCMServiceRequest", err)
	txs, err := stack.SendFrame(UIFrame(0, false, l3))
	requireNoErr(t, "SendFrame(ui CMServiceRequest)", err)
	if len(txs) != 0 {
		t.Fatalf("manual mode: SendFrame produced %d tx frames, want 0 (chain runs below)", len(txs))
	}
	evs, txEvts := stack.DrainEvents()
	if len(txEvts) != 0 || len(evs) != 1 {
		t.Fatalf("drained %d L3 events / %d tx frames, want 1/0", len(evs), len(txEvts))
	}
	ev := evs[0]
	if ev.SAPI != 0 || ev.Primitive != PrimL3UnitData || !bytes.Equal(ev.Data, l3) {
		t.Fatalf("event = (sapi %d, prim %d, data % X), want (0, L3_UNIT_DATA(4), same payload)", ev.SAPI, ev.Primitive, ev.Data)
	}
	step, err := stack.FeedL3(ev.Data)
	requireNoErr(t, "FeedL3(CMServiceRequest)", err)
	if step.Token != TokenCMServiceAccept {
		t.Fatalf("step1 token = %d, want %d (CM_SERVICE_ACCEPT)", step.Token, TokenCMServiceAccept)
	}
	resp1, err := stack.BuildResponse()
	requireNoErr(t, "BuildResponse(accept)", err)
	txs, err = stack.SendUI(resp1, 0)
	requireNoErr(t, "SendUI(accept)", err)
	if len(txs) != 1 {
		t.Fatalf("auto-response transmission captured %d frames, want 1", len(txs))
	}

	// Decode the transmitted UI with the C decoder and prove the zero-copy view:
	dec, err := DecodeFrame(txs[0])
	requireNoErr(t, "DecodeFrame(response UI)", err)
	if dec.Format != LapdmFmtU || dec.UType != LapdmUUI || dec.SAPI != 0 || dec.Command != 1 {
		t.Fatalf("response UI decode = (fmt %d u %#x sapi %d cmd %d), want U/0x03/0/1 (BTS)",
			dec.Format, dec.UType, dec.SAPI, dec.Command)
	}
	if !bytes.Equal(dec.Payload, resp1) {
		t.Fatalf("decoded payload differs from the built response")
	}
	accept, err := Parse(dec.Payload, nil)
	requireNoErr(t, "Parse(CMServiceAccept)", err)
	if accept.Name() != "CMServiceAccept" || accept.Pd() != PDMM {
		t.Fatalf("response name/pd = %q/%d, want CMServiceAccept/%d (MM)", accept.Name(), accept.Pd(), PDMM)
	}
	accept.Close()

	// -- [2] CC Setup (TI=3, called 123456789) -> CALL_PROCEEDING(17) ----------
	l3s, err := BuildSetup(3, "123456789")
	requireNoErr(t, "BuildSetup", err)
	txs, err = stack.SendFrame(UIFrame(0, false, l3s))
	requireNoErr(t, "SendFrame(ui Setup)", err)
	if len(txs) != 0 {
		t.Fatalf("manual mode: Setup SendFrame produced %d tx frames, want 0", len(txs))
	}
	evs, _ = stack.DrainEvents()
	if len(evs) != 1 || !bytes.Equal(evs[0].Data, l3s) {
		t.Fatalf("setup drain: got %d events, want 1 with the built Setup payload", len(evs))
	}
	step2, err := stack.FeedL3(evs[0].Data)
	requireNoErr(t, "FeedL3(Setup)", err)
	if step2.Token != TokenCallProceeding {
		t.Fatalf("step2 token = %d, want %d (CALL_PROCEEDING)", step2.Token, TokenCallProceeding)
	}
	resp2, err := stack.BuildResponse()
	requireNoErr(t, "BuildResponse(call proceeding)", err)
	txs, err = stack.SendUI(resp2, 0)
	requireNoErr(t, "SendUI(call proceeding)", err)
	if len(txs) != 1 {
		t.Fatalf("call-proceeding transmission captured %d frames, want 1", len(txs))
	}
	cpDec, err := DecodeFrame(txs[0])
	requireNoErr(t, "DecodeFrame(CallProceeding UI)", err)
	cp, err := Parse(cpDec.Payload, nil)
	requireNoErr(t, "Parse(CallProceeding)", err)
	if cp.Name() != "CallProceeding" || cp.Ti() != 3 {
		t.Fatalf("call proceeding name/ti = %q/%d, want CallProceeding/3", cp.Name(), cp.Ti())
	}
	cp.Close()

	phase, err := stack.ChainPhase()
	requireNoErr(t, "ChainPhase", err)
	if phase != ProcCallSetupMO {
		t.Fatalf("chain phase = %#x, want %#x (GSML3_PROC_CALL_SETUP_MO)", phase, ProcCallSetupMO)
	}

	// -- [3] T3101 = 3 s: inside the window nothing fails; expiry resets idle ──
	if n, err := stack.TickProcedures(1000); err != nil || n != 0 {
		t.Fatalf("tick_procedures(1000) = (%d, %v), want (0, nil) — inside the T3101 window", n, err)
	}
	tr, err := stack.TakeRetransmit()
	if err != nil || tr != TokenNone {
		t.Fatalf("take_retransmit = (%d, %v), want (NONE, nil)", tr, err)
	}
	if n, err := stack.TickProcedures(2500); err != nil || n != 1 { // cumulative > 3000 ms
		t.Fatalf("tick_procedures(2500) = (%d, %v), want (1, nil) — T3101 expired", n, err)
	}
	if phase, err := stack.ChainPhase(); err != nil || phase != ProcUnknown {
		t.Fatalf("chain phase after expiry = (%#x, %v), want %#x (idle)", phase, err, ProcUnknown)
	}
}

// Auto-response mode: one SendFrame drives parse -> feed -> required_size ->
// build -> send_ui entirely after the C call returned (queue model).
func TestStackAutoResponseMode(t *testing.T) {
	stack, err := NewGsmL3Stack(StackOptions{TMSI: 0x33333333, AutoResponse: true})
	if err != nil {
		t.Fatalf("NewGsmL3Stack: %v", err)
	}
	defer stack.Close()

	l3, err := BuildCMServiceRequest(1, IDTMSI, 0x33333333, "")
	requireNoErr(t, "BuildCMServiceRequest", err)
	txs, err := stack.SendFrame(UIFrame(0, false, l3))
	requireNoErr(t, "SendFrame(auto)", err)
	if len(txs) != 1 {
		t.Fatalf("auto-response produced %d frames, want exactly 1 (the CMServiceAccept UI)", len(txs))
	}
	step, ok := stack.LastStep()
	if !ok || step.Token != TokenCMServiceAccept {
		t.Fatalf("LastStep = (%+v, %v), want token %d", step, ok, TokenCMServiceAccept)
	}
	dec, err := DecodeFrame(txs[0])
	requireNoErr(t, "DecodeFrame", err)
	if dec.Command != 1 { // BTS-side frame (C/R=1)
		t.Fatalf("response command bit = %d, want 1 (BTS side)", dec.Command)
	}
	acc, err := Parse(dec.Payload, nil)
	requireNoErr(t, "Parse", err)
	if acc.Name() != "CMServiceAccept" {
		t.Fatalf("auto response name = %q, want CMServiceAccept", acc.Name())
	}
	acc.Close()
	// The L3 queue must be consumed post-FFI — nothing left behind.
	evs, txEvts := stack.DrainEvents()
	if len(evs) != 0 || len(txEvts) != 0 {
		t.Fatalf("queues after auto SendFrame = %d L3 / %d tx, want empty", len(evs), len(txEvts))
	}
	if err := stack.Close(); err != nil || stack.LastEventError() != nil {
		t.Fatalf("close after auto test: %v / last event error %v", err, stack.LastEventError())
	}
}

// ── Link lifecycle (mirror of the C Entity_LinkLifecycle test) ────────────────

func TestLinkLifecycle(t *testing.T) {
	e, err := NewLapdmEntityStandalone(0) // 0 = SDCCH profile
	if err != nil {
		t.Fatalf("NewLapdmEntityStandalone: %v", err)
	}
	defer e.Close()

	requireNoErr(t, "Open", e.Open(0, true)) // BTS side (C/R=1)
	if st := e.State(); st != StateLapdmLinkReleased {
		t.Fatalf("state after open = %d, want LINK_RELEASED(1)", st)
	}

	requireNoErr(t, "SendSABME", e.SendSABME())
	txs := e.DrainTX()
	if len(txs) != 1 || !bytes.Equal(txs[0], []byte{0x09, 0x2F}) {
		t.Fatalf("SABME tx = %v, want exactly [[0x09, 0x2F]] (byte-exact)", txs)
	}
	if st := e.State(); st != StateLapdmAwaitingEstablish {
		t.Fatalf("state after SABME = %d, want AWAITING_ESTABLISH(2)", st)
	}

	requireNoErr(t, "Receive UA", e.Receive(UAFrame())) // [0x01, 0x63] MS -> BTS
	if st := e.State(); st != StateLapdmLinkEstablished {
		t.Fatalf("state after UA = %d, want LINK_ESTABLISHED(4)", st)
	}
	if !e.IsEstablished() {
		t.Fatal("IsEstablished = false after link established")
	}

	payload := []byte{0x60, 0x0D, 0x00} // Channel Release L3
	requireNoErr(t, "Receive UI", e.Receive(UIFrame(0, false, payload)))
	requireNoErr(t, "SendDISC", e.SendDISC())
	if st := e.State(); st != StateLapdmAwaitingRelease {
		t.Fatalf("state after DISC = %d, want AWAITING_RELEASE(3)", st)
	}
	requireNoErr(t, "Receive release UA", e.Receive(UAFrame()))
	if st := e.State(); st != StateLapdmLinkReleased || e.IsEstablished() {
		t.Fatalf("state after release UA = %d/established=%v, want LINK_RELEASED(1)/false", st, e.IsEstablished())
	}
	if got := e.FramesReceived(); got != 3 { // UA + UI + UA
		t.Fatalf("frames_received = %d, want 3", got)
	}

	// L3 events: ESTABLISH_CONFIRM (empty payload) then UNIT_DATA (the same payload).
	evs := e.DrainL3()
	sawEstablishConfirm, sawUnitData := false, false
	for _, ev := range evs {
		if ev.Primitive == PrimL3EstablishConfirm && len(ev.Data) == 0 {
			sawEstablishConfirm = true
		}
		if ev.Primitive == PrimL3UnitData && bytes.Equal(ev.Data, payload) {
			sawUnitData = true
		}
	}
	if !sawEstablishConfirm || !sawUnitData {
		t.Fatalf("events missing: establish_confirm=%v unit_data=%v (got %d events)",
			sawEstablishConfirm, sawUnitData, len(evs))
	}

	requireNoErr(t, "double Close", e.Close()) // idempotent — no double free, no panic
	if st := e.State(); st != StateLapdmUnused {
		t.Fatalf("closed entity state = %d, want UNUSED sentinel (no FFI)", st)
	}
	if n := e.FramesSent(); n != 0 {
		t.Fatalf("closed entity FramesSent = %d, want 0 (no FFI)", n)
	}
}

// ── T200 retransmission (SDCCH: T200 = 900 ms) ───────────────────────────────

func TestT200Retransmission(t *testing.T) {
	// Inside the window (899 ms): no retransmission.
	e, err := NewLapdmEntityStandalone(0)
	requireNoErr(t, "NewLapdmEntityStandalone", err)
	requireNoErr(t, "Open", e.Open(0, true))
	requireNoErr(t, "SendSABME", e.SendSABME())
	if r, err := e.TickT200(899); err != nil || r != 0 {
		t.Fatalf("tick_t200(899) = (%d, %v), want (0, nil)", r, err)
	}
	if n := e.Retransmissions(); n != 0 {
		t.Fatalf("retransmissions after 899 ms = %d, want 0", n)
	}
	e.DrainTX() // drop the initial SABME
	e.Close()

	// At exactly 900 ms: retransmit; the COUNTER reaches 1 and a fresh byte-exact
	// SABME frame is emitted (mirrors the C test).
	e2, err := NewLapdmEntityStandalone(0)
	requireNoErr(t, "NewLapdmEntityStandalone#2", err)
	defer e2.Close()
	requireNoErr(t, "Open#2", e2.Open(0, true))
	requireNoErr(t, "SendSABME#2", e2.SendSABME())
	txs := e2.DrainTX()
	if len(txs) != 1 || !bytes.Equal(txs[0], []byte{0x09, 0x2F}) {
		t.Fatalf("first SABME = %v, want [[0x09 0x2F]]", txs)
	}
	r, err := e2.TickT200(900)
	if err != nil || r != 1 {
		t.Fatalf("tick_t200(900) = (%d, %v), want (1, nil)", r, err)
	}
	if n := e2.Retransmissions(); n != 1 {
		t.Fatalf("retransmissions at expiry = %d, want 1", n)
	}
	txs = e2.DrainTX()
	if len(txs) != 1 || !bytes.Equal(txs[0], []byte{0x09, 0x2F}) {
		t.Fatalf("retransmitted SABME = %v, want [[0x09 0x2F]]", txs)
	}
}

// ── NULL validation at the wrapper/stack level ───────────────────────────────

func TestStackNullValidation(t *testing.T) {
	// all-zero TMSI: C-side rule (TS 24.008), passed through with its message.
	if _, err := NewGsmL3Stack(StackOptions{TMSI: 0}); err == nil {
		t.Fatal("NewGsmL3Stack(TMSI=0) must fail")
	} else if e, ok := err.(*Error); !ok || e.Code != CodeInvalidArg || e.Msg == "" {
		t.Fatalf("zero TMSI error = %v, want *Error INVALID_ARG with non-empty message", err)
	}

	// both TMSI and IMSI set: wrapper-level rejection before FFI.
	if _, err := NewGsmL3Stack(StackOptions{TMSI: 1, IMSI: "244051234567890"}); err == nil {
		t.Fatal("stack.New with both TMSI and IMSI set must fail")
	} else if e, ok := err.(*Error); !ok || e.Code != CodeInvalidArg {
		t.Fatalf("stack.New both-set error = %v, want INVALID_ARG", err)
	}

	// shard count outside {0,4,8,16,32}: rejected in Go before FFI.
	if _, err := NewGsmL3Stack(StackOptions{TMSI: 0x55555555, ShardCount: 3}); err == nil {
		t.Fatal("NewGsmL3Stack(shards=3) must fail")
	} else if e, ok := err.(*Error); !ok || e.Code != CodeInvalidArg {
		t.Fatalf("shards=3 error = %v, want INVALID_ARG", err)
	}

	// SAPI out of range: pre-FFI rejection.
	if _, err := NewGsmL3Stack(StackOptions{TMSI: 0x55555556, SAPI: 16}); err == nil {
		t.Fatal("stack.New(sapi=16) must fail before FFI")
	} else if e, ok := err.(*Error); !ok || e.Code != CodeInvalidArg {
		t.Fatalf("stack.New(sapi=16) error = %v, want INVALID_ARG", err)
	}
	if _, err := NewGsmL3Stack(StackOptions{TMSI: 0x55555557, Profile: 7}); err == nil {
		t.Fatal("stack.New(profile=7) must fail before FFI")
	} else if e, ok := err.(*Error); !ok || e.Code != CodeInvalidArg {
		t.Fatalf("stack.New(profile=7) error = %v, want INVALID_ARG", err)
	}

	// IMSI + sharded registry: the C contract reports UNSUPPORTED — pass-through.
	if _, err := NewGsmL3Stack(StackOptions{IMSI: "244051234567890", ShardCount: 4}); err == nil {
		t.Fatal("NewGsmL3Stack(imsi, shards=4) must fail")
	} else if e, ok := err.(*Error); !ok || e.Code != CodeUnsupported {
		t.Fatalf("imsi+sharded error = %v, want UNSUPPORTED (C pass-through)", err)
	}

	// SendFrame(nil/short): typed error BEFORE FFI — proven via the fixed seam:
	stack, err := NewGsmL3Stack(StackOptions{TMSI: 0x44444444})
	requireNoErr(t, "stack for nil-validation", err)
	baseReceive := ffiReceiveCalls.Load()
	if _, err := stack.SendFrame(nil); err == nil {
		t.Fatal("SendFrame(nil) must fail")
	} else if e, ok := err.(*Error); !ok || e.Code != CodeInvalidArg {
		t.Fatalf("SendFrame(nil) error = %v, want INVALID_ARG pre-FFI", err)
	}
	if _, err := stack.SendFrame([]byte{0x01}); err == nil {
		t.Fatal("SendFrame(1 byte) must fail")
	} else if e, ok := err.(*Error); !ok || e.Code != CodeInvalidArg {
		t.Fatalf("SendFrame(short) error = %v, want INVALID_ARG pre-FFI", err)
	}
	if got := ffiReceiveCalls.Load(); got != baseReceive {
		t.Fatalf("rejected SendFrame added %d entity-receive FFI calls, want 0 (no-FFI invariant)", got-baseReceive)
	}
	requireNoErr(t, "Close(stack for nil-validation)", stack.Close())

	// After close: every method errors WITHOUT any FFI (the seam must not move).
	baseReceive = ffiReceiveCalls.Load()
	baseSendUI := ffiSendUICalls.Load()
	l3, err := BuildCMServiceRequest(1, IDTMSI, 0x44444444, "")
	requireNoErr(t, "build for closed probe", err)
	if _, err := stack.SendFrame(UIFrame(0, false, l3)); err == nil {
		t.Fatal("SendFrame on a closed stack must fail")
	} else if e, ok := err.(*Error); !ok || e.Msg != "closed stack" {
		t.Fatalf("closed SendFrame error = %v, want 'closed stack'", err)
	}
	if _, err := stack.FeedL3(l3); err == nil {
		t.Fatal("FeedL3 on a closed stack must fail")
	}
	if st := stack.State(); st != StateLapdmUnused {
		t.Fatalf("closed State() = %d, want UNUSED sentinel without FFI", st)
	}
	if stack.Established() {
		t.Fatal("closed Established() = true, want false (C NULL sentinel)")
	}
	if evs, tx := stack.DrainEvents(); len(evs) != 0 || len(tx) != 0 {
		t.Fatalf("closed DrainEvents returned leftovers (%d/%d)", len(evs), len(tx))
	}
	if got, want := ffiReceiveCalls.Load(), baseReceive; got != want {
		t.Fatalf("closed path performed entity-receive FFI (+%d calls) — invariant violated", got-baseReceive)
	}
	if got, want := ffiSendUICalls.Load(), baseSendUI; got != want {
		t.Fatalf("closed path performed send-ui FFI (+%d calls) — invariant violated", got-baseSendUI)
	}

	// Sending I-frame data before the link is established: C error with a message.
	e, err := NewLapdmEntityStandalone(0)
	requireNoErr(t, "standalone entity", err)
	defer e.Close()
	requireNoErr(t, "Open(standalone)", e.Open(0, true))
	if err := e.SendData([]byte{0x50, 0x84}); err == nil {
		t.Fatal("SendData before establishment must fail")
	} else if er, ok := err.(*Error); !ok || er.Msg == "" {
		t.Fatalf("SendData(no link) error = %v, want *Error with C message", err)
	}
	// ...but a UI send works in any state (C contract): no error.
	if err := e.SendUI(0, []byte{0x50, 0x84}); err != nil {
		t.Fatalf("SendUI without link must work in any state: %v", err)
	}
}

// Double close / double free are silent no-ops for every owned wrapper (RAII
// guards; the C frees are NULL-safe as well).
func TestDoubleCloseIsIdempotent(t *testing.T) {
	cfg, err := NewConfig()
	requireNoErr(t, "NewConfig", err)
	if err := cfg.Close(); err != nil || cfg.Close() != nil {
		t.Fatalf("config double close: %v", err)
	}

	m, err := ParseHex("60 0D 00", nil)
	requireNoErr(t, "ParseHex", err)
	if err := m.Close(); err != nil || m.Close() != nil {
		t.Fatalf("message double close: %v", err)
	}

	rsl, err := RslBuildDataReq(0x10, 1, []byte{0x50, 0x84})
	requireNoErr(t, "RslBuildDataReq", err)
	f, err := RslParse(rsl)
	requireNoErr(t, "RslParse", err)
	if err := f.Close(); err != nil || f.Close() != nil {
		t.Fatalf("rsl double close: %v", err)
	}

	reg, err := NewRegistry(0)
	requireNoErr(t, "NewRegistry", err)
	if err := reg.Close(); err != nil || reg.Close() != nil {
		t.Fatalf("registry double close: %v", err)
	}

	o, err := NewOrchestrator()
	requireNoErr(t, "NewOrchestrator", err)
	if err := o.Close(); err != nil || o.Close() != nil {
		t.Fatalf("orchestrator double close: %v", err)
	}

	stack, err := NewGsmL3Stack(StackOptions{TMSI: 0x22222222})
	requireNoErr(t, "NewGsmL3Stack", err)
	if err := stack.Close(); err != nil || stack.Close() != nil {
		t.Fatalf("stack double close: %v", err)
	}

	e, err := NewLapdmEntityStandalone(0)
	requireNoErr(t, "NewLapdmEntityStandalone", err)
	if err := e.Close(); err != nil || e.Close() != nil {
		t.Fatalf("entity double close: %v", err)
	}
}

// ── Registry identity rules + sharded smoke ───────────────────────────────────

func TestRegistryIdentityAndSharded(t *testing.T) {
	for _, shards := range []int{0, 4, 8, 16, 32} {
		reg, err := NewRegistry(shards)
		if err != nil {
			t.Fatalf("NewRegistry(%d): %v", shards, err)
		}
		for i := 0; i < 100; i++ {
			tmsi := uint32(0xA0000000 + shards*1000 + i)
			if _, err := reg.CreateByTMSI(tmsi); err != nil {
				t.Fatalf("registry %d shards: CreateByTMSI(%#x): %v", shards, tmsi, err)
			}
			found, err := reg.FindByTMSI(tmsi)
			if err != nil || found == nil {
				t.Fatalf("registry %d shards: FindByTMSI(%#x) = (%v, %v), want a live session", shards, tmsi, found, err)
			}
		}
		n, err := reg.Count()
		if err != nil || n != 100 {
			t.Fatalf("registry %d shards: Count = (%d, %v), want (100, nil)", shards, n, err)
		}
		requireNoErr(t, "Close", reg.Close())

		// closed registry: every accessor refuses without FFI
		if _, err := reg.CreateByTMSI(0x70000000 + uint32(shards)); err == nil {
			t.Fatalf("closed registry(%d shards) CreateByTMSI must fail", shards)
		}
	}

	// Plain-registry identity rules: duplicate key -> DUPLICATE code; an IMSI-
	// created session gets an auto-assigned TMSI and is findable by BOTH keys.
	reg, err := NewRegistry(0)
	requireNoErr(t, "NewRegistry(plain)", err)
	defer reg.Close()
	first, err := reg.CreateByTMSI(0x60000001)
	requireNoErr(t, "CreateByTMSI(first)", err)
	if _, err := reg.CreateByTMSI(0x60000001); err == nil {
		t.Fatal("duplicate TMSI must fail")
	} else if e, ok := err.(*Error); !ok || e.Code != CodeDuplicate {
		t.Fatalf("duplicate TMSI error = %v, want DUPLICATE (13)", err)
	}

	byIMSI, err := reg.CreateByIMSI("244051234567890")
	requireNoErr(t, "CreateByIMSI(plain)", err)
	assigned, err := byIMSI.AssignedTMSI()
	if err != nil || assigned == 0 {
		t.Fatalf("AssignedTMSI = (%#x, %v), want non-zero auto-assigned", assigned, err)
	}
	found, err := reg.FindByIMSI("244051234567890")
	if err != nil || found == nil {
		t.Fatalf("FindByIMSI = (%v, %v), want a live session", found, err)
	}
	assigned2, _ := found.AssignedTMSI()
	if assigned2 != assigned {
		t.Fatalf("FindByIMSI session keys %#x, the created one keys %#x", assigned2, assigned)
	}
	if found, err := reg.FindByTMSI(assigned); err != nil || found == nil {
		t.Fatalf("FindByTMSI(auto-assigned) = (%v, %v), want a live session", found, err)
	}
	if found, err := reg.FindByTMSI(0x12345678); err != nil || found != nil { // not-found is NOT an error
		t.Fatalf("FindByTMSI(absent) = (%v, %v), want (nil, nil)", found, err)
	}

	if removed, err := reg.Remove(first); err != nil || !removed {
		t.Fatalf("Remove(first) = (%v, %v), want (true, nil)", removed, err)
	}
	if found, err := reg.FindByTMSI(0x60000001); err != nil || found != nil {
		t.Fatalf("FindByTMSI after Remove = (%v, %v), want (nil, nil)", found, err)
	}

	// Session access after registry close: typed error instead of dead memory.
	late, err := reg.FindByIMSI("244051234567890")
	requireNoErr(t, "FindByIMSI(before close)", err)
	requireNoErr(t, "Close(registry for borrowed probe)", reg.Close())
	if _, err := late.TMSI(); err == nil {
		t.Fatal("session access after registry close must fail (borrowed handle)")
	}
}

// ── Concurrency: independent stacks, unique TMSIs, under the race detector ───
// Mirrors the C Concurrent-IndependentEntities shape: each goroutine owns its
// full stack (one thread per handle is respected); nothing is shared between
// them. -race must stay clean (bridges take their own entity queue locks).

func TestConcurrentEntities(t *testing.T) {
	const goroutines, iterations = 8, 200
	var wg sync.WaitGroup
	errs := make([]error, goroutines)

	for g := 0; g < goroutines; g++ {
		wg.Add(1)
		go func(g int) {
			defer wg.Done()
			l3 := []byte{0x60, 0x0D, 0x00} // stable Channel Release vector (mirrors the C test suite)
			for i := 0; i < iterations; i++ {
				tmsi := uint32(0xB0000000 + g*1000 + i)
				stack, err := NewGsmL3Stack(StackOptions{TMSI: tmsi}) // AutoResponse=false default
				if err != nil {
					errs[g] = err
					return
				}
				txs, err := stack.SendFrame(UIFrame(0, false, l3)) // L2 delivers, chain untouched (manual)
				if err != nil {
					stack.Close()
					errs[g] = err
					return
				}
				if len(txs) != 0 {
					stack.Close()
					errs[g] = &Error{Op: "concurrent", Code: CodeInternal, Msg: "manual SendFrame must not emit frames"}
					return
				}
				_ = stack.Close() // teardown on every iteration (registry/orch/entity freed exactly once each)
			}
		}(g)
	}
	wg.Wait()

	for g, err := range errs {
		if err != nil {
			t.Fatalf("goroutine %d: %v", g, err)
		}
	}
}

// The FFI seam itself: a live stack that sends a UI frame must cross
// gsml3_lapdm_entity_receive exactly once (sanity for the closed-path tests).
func TestFFISeamCountsLiveCalls(t *testing.T) {
	stack, err := NewGsmL3Stack(StackOptions{TMSI: 0x91234567})
	if err != nil {
		t.Fatalf("NewGsmL3Stack: %v", err)
	}
	defer stack.Close()
	before := ffiReceiveCalls.Load()
	l3, err := BuildCMServiceRequest(1, IDTMSI, 0x91234567, "")
	if err != nil {
		t.Fatalf("BuildCMServiceRequest: %v", err)
	}
	if _, err := stack.SendFrame(UIFrame(0, false, l3)); err != nil {
		t.Fatalf("SendFrame: %v", err)
	}
	if got, want := ffiReceiveCalls.Load(), before+1; got != want {
		t.Fatalf("live SendFrame crossed the FFI boundary %d times, want exactly 1 (seam broken)", got-before)
	}
}
