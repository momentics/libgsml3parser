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

import (
	"fmt"
	"sync"
	"sync/atomic"
)

// GsmL3Stack is the unified Python/Go/Rust surface: one BTS-side reference
// stack over the C core — registry + BORROWED session + orchestrator chain +
// a bridged LAPDm entity with auto-response.
//
// # Ownership
//
// The stack owns exactly the three C handles it creates (registry,
// orchestrator, lapdm entity) and releases each of them EXACTLY ONCE in Close,
// in the ABI order entity -> orchestrator -> registry. The session is BORROWED
// (owned by the registry): it is NEVER freed directly — gsml3_registry_free
// releases every session — so a Session wrapper outliving its registry must not
// be used; all session accessors below refuse that instead of touching dead
// memory. Close() is idempotent (atomic take); after it, every method fails
// with a typed error and performs ZERO FFI calls (a closed raw C handle has no
// such state — this is a binding-level invariant, proven by the FFI-call
// seam in lapdm.go / the nulltest spies).
//
// # Callback rules
//
// Queue model (decision #3): the C core invokes the bridges SYNCHRONOUSLY
// inside gsml3_lapdm_entity_receive / send_*, and the l3/frame spans live only
// during one callback. A bridge may do ONLY memory-safe work: a zero-copy read
// via unsafe.Slice over the C span plus exactly one owned copy appended under
// the entity queue lock — no cgo calls, no blocking, never a gsml3_* re-entry
// through this entity. SendFrame / FeedL3 drain and process the queues strictly
// AFTER the C call returned: parse -> orchestrator.feed -> required_size ->
// build_response -> send_ui. Response frames therefore are never emitted from
// inside a callback through the same entity (that would mutate its FSM
// mid-receive — forbidden by the C ABI, UB / deadlock risk).
//
// # Threading
//
// Owned handles are single-thread per the C contract ("one thread per handle"):
// a stack must not be shared by goroutines — mu serializes the
// receive/drain/orchestrate path as hygiene (and makes Close race-safe against
// an in-flight call on the SAME goroutine), it is NOT a sharing mechanism.
// A registry with shardCount > 0 additionally makes its REGISTRY-MEDIATED calls
// thread-safe (per-shard C locks); direct session access is caller-synchronized
// in every flavor. Concurrent independent stacks are safe (covered by the
// -race test).
type GsmL3Stack struct {
	reg  *Registry // owned; released LAST by Close (sessions go with it)
	sess *Session  // BORROWED — never released directly
	orch *Orchestrator
	ent  *LapdmEntity

	sapi         int
	autoResponse bool
	closed       atomic.Bool

	mu sync.Mutex // serializes drain / response building (see Threading block above)

	// The BRIDGE queues live ONLY on ent (see lapdm.go eventSink); they are
	// intentionally NOT duplicated here — the sink methods delegate into them.
	lastEventError error // guarded by mu; parse/feed failures of drained events, surfaced via LastEventError
	lastStep       StepResult
	hasLastStep    bool // mirrors Python last_step: set by every successful processed event (SendFrame auto mode / FeedL3)
}

// StackOptions configures NewGsmL3Stack. Exactly one of TMSI (non-zero) or IMSI
// must be set — neither set falls through to the C all-zero-TMSI rejection
// (INVALID_ARG pass-through, TS 24.008). IMSI requires ShardCount == 0 per the C
// API (sharded registries report GSML3_ERR_UNSUPPORTED for create-by-IMSI,
// passed through as an error). SAPI is 0..15, Profile 0..2, ShardCount is one of
// 0,4,8,16,32 — all validated in Go BEFORE any FFI call (NULL policy).
// AutoResponse follows Go zero-value semantics (default false); every usage
// site in this repository sets it explicitly.
type StackOptions struct {
	TMSI         uint32
	IMSI         string
	ShardCount   int
	SAPI         int
	Profile      int
	AutoResponse bool
}

// GsmL3Stack implements eventSink by delegating into its entity's queues, so a
// bridge resolves to exactly one queue owner regardless of creation path. The
// ent != nil guard covers the tiny constructor window between cgo handle
// registration and field assignment (no C call can run in it).
func (s *GsmL3Stack) enqueueL3(ev L3Event) {
	if s.ent != nil {
		s.ent.enqueueL3(ev)
	}
}

func (s *GsmL3Stack) enqueueTX(b []byte) {
	if s.ent != nil {
		s.ent.enqueueTX(b)
	}
}

// NewGsmL3Stack builds the stack chain exactly once and opens the LAPDm link on
// the BTS side (command bit 1). Creation order (reversed in Close): allocate &GsmL3Stack
// FIRST (it is the eventSink owner) -> registry -> session -> orchestrator ->
// newLapdmEntity(profile, s) [bridge token registered before the C entity
// exists] -> ent.Open(sapi, true). Any failing step rolls back everything
// already created. Input validation happens in Go before any FFI call.
func NewGsmL3Stack(o StackOptions) (*GsmL3Stack, error) {
	// NULL policy: wrapper-level rejections before the FFI boundary.
	if o.TMSI != 0 && o.IMSI != "" {
		return nil, invalidArg("stack.New", "exactly one of TMSI (non-zero) or IMSI must be set")
	}
	if o.SAPI < 0 || o.SAPI > 15 {
		return nil, invalidArg("stack.New", fmt.Sprintf("sapi must be in 0..15; got %d", o.SAPI))
	}
	if o.Profile < 0 || o.Profile > 2 {
		return nil, invalidArg("stack.New", fmt.Sprintf("profile must be 0 (SDCCH), 1 (SACCH) or 2 (FACCH); got %d", o.Profile))
	}
	if !shardCountAllowed(o.ShardCount) {
		return nil, invalidArg("stack.New", fmt.Sprintf("shard count must be one of 0,4,8,16,32; got %d", o.ShardCount))
	}

	s := &GsmL3Stack{sapi: o.SAPI, autoResponse: o.AutoResponse} // eventSink owner first

	reg, err := NewRegistry(o.ShardCount) // Go validation + C backstop + NO_MEMORY
	if err != nil {
		return nil, err
	}
	s.reg = reg

	var sess *Session
	if o.IMSI != "" {
		sess, err = reg.CreateByIMSI(o.IMSI) // sharded -> C UNSUPPORTED pass-through (documented)
	} else {
		sess, err = reg.CreateByTMSI(o.TMSI) // all-zero TMSI -> C INVALID_ARG pass-through
	}
	if err != nil {
		_ = reg.Close()
		return nil, err
	}
	s.sess = sess

	orch, oerr := NewOrchestrator()
	if oerr != nil {
		_ = reg.Close()
		return nil, oerr
	}
	s.orch = orch

	ent, eerr := newLapdmEntity(o.Profile, s)
	if eerr != nil {
		_ = orch.Close()
		_ = reg.Close()
		return nil, eerr
	}
	s.ent = ent

	if err := ent.Open(o.SAPI, true); err != nil { // 1 = BTS side (C/R=1): we are the network
		_ = s.Close()
		return nil, err
	}
	return s, nil
}

// ensureOpen refuses to run once the stack OR any of its components is closed:
// raw C handles have no "closed" state, so a dead component would make every FFI
// call below it touch freed memory. Callers that hold mu keep it held.
func (s *GsmL3Stack) ensureOpen(op string) error {
	if s.closed.Load() {
		return &Error{Op: op, Code: CodeInvalidArg, Msg: "closed stack"}
	}
	if s.reg == nil || s.orch == nil || s.ent == nil ||
		s.reg.Closed() || s.orch.Closed() || s.ent.Closed() {
		return &Error{Op: op, Code: CodeInvalidArg,
			Msg: "a stack component was closed out from under it; create a fresh stack"}
	}
	return nil
}

// SendFrame feeds one raw LAPDm frame into the C entity and drives the chain.
//
// UNIFIED semantics (Python send_frame / Rust send_frame — identical behavior):
// 1) reset the tx collector; 2) FFI entity.Receive(frame) — C callbacks only
// append to the queues (no cgo inside); 3) IF AutoResponse: drain the L3 queue
// POST-FFI and, for each event with a payload: parse -> orchestrator.Feed; when
// the step carries a token != TokenNone, build the required-size response and
// SendUI it on the stack SAPI — new tx frames are captured by the L1 bridge.
// Parse failures of one event are recorded (LastEventError) without aborting
// the batch; feed/build/send failures abort with the typed error; 4) IF NOT
// AutoResponse: events stay unprocessed for DrainEvents()/FeedL3(); 5) return
// ONLY the tx frames produced for THIS input — the C FSM's L2-level reactions
// plus any auto responses (the caller owns the copies).
func (s *GsmL3Stack) SendFrame(frame []byte) ([][]byte, error) {
	if len(frame) < 2 { // before FFI (NULL policy): a LAPDm frame is at least address + control
		return nil, invalidArg("stack.SendFrame", "frame shorter than address+control (< 2 bytes)")
	}
	s.mu.Lock()
	defer s.mu.Unlock()
	if err := s.ensureOpen("stack.SendFrame"); err != nil {
		return nil, err
	}

	s.ent.ResetTXCollector() // start a clean tx collector for THIS input
	if err := s.ent.Receive(frame); err != nil {
		return nil, err // C-internal failure of the receive: no chain step runs
	}
	if s.autoResponse {
		for _, ev := range s.ent.DrainL3() { // post-FFI drain — never re-enter this entity
			if len(ev.Data) == 0 {
				continue // link-state primitives (ESTABLISH_CONFIRM, ...) carry no payload
			}
			m, err := Parse(ev.Data, nil)
			if err != nil {
				s.lastEventError = err
				continue // recorded, surfaced via LastEventError(); frame processing continues
			}
			step, ferr := s.orch.Feed(m, s.sess)
			_ = m.Close() // release regardless of feed outcome (RAII equivalent)
			if ferr != nil {
				return nil, ferr
			}
			s.lastStep = step
			s.hasLastStep = true
			if step.Token != TokenNone {
				resp, rerr := s.orch.BuildResponse(s.sess) // exact-size build (decision #8)
				if rerr != nil {
					return nil, rerr
				}
				if uerr := s.ent.SendUI(s.sapi, resp); uerr != nil {
					return nil, uerr
				} // new tx frames appended by the L1 bridge
			}
		}
	}
	return s.ent.DrainTX(), nil // copies; caller owns them
}

// FeedL3 drives orchestration directly without L2 (parse + feed on the stack's
// session) — the manual-mode test/simulation hook of AutoResponse==false. The
// response is NOT sent automatically: BuildResponse() then SendUI().
func (s *GsmL3Stack) FeedL3(l3 []byte) (StepResult, error) {
	if len(l3) == 0 { // before FFI (NULL policy)
		return StepResult{}, invalidArg("stack.FeedL3", "empty L3 payload")
	}
	s.mu.Lock()
	defer s.mu.Unlock()
	if err := s.ensureOpen("stack.FeedL3"); err != nil {
		return StepResult{}, err
	}
	m, err := Parse(l3, nil)
	if err != nil {
		return StepResult{}, err
	}
	defer m.Close()
	step, ferr := s.orch.Feed(m, s.sess)
	if ferr != nil {
		return step, ferr
	}
	s.lastStep = step
	s.hasLastStep = true
	return step, nil
}

// BuildResponse builds the PENDING response (required_size -> exact buffer);
// nothing pending fails with the C code (typically INVALID_VALUE) — it never
// returns an empty byte slice on failure.
func (s *GsmL3Stack) BuildResponse() ([]byte, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	if err := s.ensureOpen("stack.BuildResponse"); err != nil {
		return nil, err
	}
	return s.orch.BuildResponse(s.sess)
}

// SendUI transmits one response manually (AutoResponse==false mode). The tx
// frame(s) captured for THIS transmission are returned; a failed C send yields
// no frames and the typed error.
func (s *GsmL3Stack) SendUI(l3 []byte, sapi int) ([][]byte, error) {
	if len(l3) == 0 { // before FFI (NULL policy)
		return nil, invalidArg("stack.SendUI", "empty L3 payload")
	}
	s.mu.Lock()
	defer s.mu.Unlock()
	if err := s.ensureOpen("stack.SendUI"); err != nil {
		return nil, err
	}
	s.ent.ResetTXCollector()
	if err := s.ent.SendUI(sapi, l3); err != nil {
		return nil, err
	}
	return s.ent.DrainTX(), nil
}

// DrainEvents returns the UNPROCESSED queues for AutoResponse==false mode: the
// pending L3 events followed by the captured tx frames (take-and-empty), with no
// orchestration performed. A closed stack yields (nil, nil) without any FFI.
func (s *GsmL3Stack) DrainEvents() ([]L3Event, [][]byte) {
	s.mu.Lock()
	defer s.mu.Unlock()
	if err := s.ensureOpen("stack.DrainEvents"); err != nil {
		return nil, nil
	}
	return s.ent.DrainL3(), s.ent.DrainTX()
}

// LastStep returns the StepResult of the most recently processed L3 event
// (AutoResponse mode inside SendFrame, or FeedL3); ok is false before any event
// has been processed. No FFI.
func (s *GsmL3Stack) LastStep() (StepResult, bool) {
	s.mu.Lock()
	defer s.mu.Unlock()
	return s.lastStep, s.hasLastStep
}

// LastEventError returns the most recent failure of a single L3 event during an
// auto-response SendFrame batch (parse error — feed/build/send failures abort
// the call instead); nil while clean. No FFI.
func (s *GsmL3Stack) LastEventError() error {
	s.mu.Lock()
	defer s.mu.Unlock()
	return s.lastEventError
}

// ── Session pass-throughs (all through the borrowed session, closed-checked) ─

// TMSI returns the identity TMSI of the bound session (0 for IMSI-created).
func (s *GsmL3Stack) TMSI() (uint32, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	if err := s.ensureOpen("stack.TMSI"); err != nil {
		return 0, err
	}
	return s.sess.TMSI()
}

// AssignedTMSI returns the registry key TMSI (auto-assigned for IMSI sessions).
func (s *GsmL3Stack) AssignedTMSI() (uint32, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	if err := s.ensureOpen("stack.AssignedTMSI"); err != nil {
		return 0, err
	}
	return s.sess.AssignedTMSI()
}

// SetIMSI stores a BCD digit string on the session.
func (s *GsmL3Stack) SetIMSI(digits string) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	if err := s.ensureOpen("stack.SetIMSI"); err != nil {
		return err
	}
	return s.sess.SetIMSI(digits)
}

// Registered reports the MS-registered session flag.
func (s *GsmL3Stack) Registered() (bool, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	if err := s.ensureOpen("stack.Registered"); err != nil {
		return false, err
	}
	return s.sess.IsRegistered()
}

// Authenticated reports the MS-authenticated session flag.
func (s *GsmL3Stack) Authenticated() (bool, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	if err := s.ensureOpen("stack.Authenticated"); err != nil {
		return false, err
	}
	return s.sess.IsAuthenticated()
}

// Ciphered reports the ciphering session flag.
func (s *GsmL3Stack) Ciphered() (bool, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	if err := s.ensureOpen("stack.Ciphered"); err != nil {
		return false, err
	}
	return s.sess.IsCiphered()
}

// ── Observations and ticks ──────────────────────────────────────────────────

// ChainPhase returns the active GSML3_PROC_* chain phase (ProcUnknown when idle).
func (s *GsmL3Stack) ChainPhase() (int, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	if err := s.ensureOpen("stack.ChainPhase"); err != nil {
		return 0, err
	}
	return s.orch.ChainPhase()
}

// State returns the LAPDm FSM state (StateLapdm*). A closed stack reports
// UNUSED without any FFI — the C-documented NULL-entity sentinel (no error slot
// by design; see the Ownership block for the closed-access contract).
func (s *GsmL3Stack) State() int {
	s.mu.Lock()
	defer s.mu.Unlock()
	if err := s.ensureOpen("stack.State"); err != nil {
		return StateLapdmUnused
	}
	return s.ent.State()
}

// Established reports the link state (LinkEstablished or ContentionResolution);
// false for a closed stack (C NULL sentinel, no FFI).
func (s *GsmL3Stack) Established() bool {
	s.mu.Lock()
	defer s.mu.Unlock()
	if err := s.ensureOpen("stack.Established"); err != nil {
		return false
	}
	return s.ent.IsEstablished()
}

// TickTimers advances the REGISTRY session timers and returns their expiries
// (borrowed session pointers + timer ids; never dropped — C re-arms the
// overflow onto the next tick).
func (s *GsmL3Stack) TickTimers(deltaMs uint32, cap int) ([]TimerExpiry, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	if err := s.ensureOpen("stack.TickTimers"); err != nil {
		return nil, err
	}
	return s.reg.TickTimers(deltaMs, cap)
}

// TickProcedures ticks the ORCHESTRATOR CHAIN timers (procedure timeouts owned
// by the active chain, e.g. T3101 = 3 s on MO call setup) and returns the number
// of failures — distinct from Registry.TickProcedures, which advances
// registry-level session procedures.
func (s *GsmL3Stack) TickProcedures(deltaMs uint32) (int, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	if err := s.ensureOpen("stack.TickProcedures"); err != nil {
		return 0, err
	}
	return s.orch.Tick(deltaMs)
}

// TakeRetransmit drains the chain's retransmission channel (consume-on-read);
// TokenNone when nothing is queued — after a non-none token, build and send it
// via BuildResponse()/SendUI().
func (s *GsmL3Stack) TakeRetransmit() (int, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	if err := s.ensureOpen("stack.TakeRetransmit"); err != nil {
		return 0, err
	}
	return s.orch.TakeRetransmit()
}

// TickT200 advances the ENTITY's LAPDm T200 timer: returns 1 when a
// retransmission / abnormal release happened (its tx frames land in the entity
// queue), 0 otherwise.
func (s *GsmL3Stack) TickT200(elapsedMs uint32) (int, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	if err := s.ensureOpen("stack.TickT200"); err != nil {
		return 0, err
	}
	return s.ent.TickT200(elapsedMs)
}

// ── Components (read-only; each self-guards its closed state) ──────────────

// Registry exposes the owned registry for advanced use (further sessions,
// channel assignment, registry-level ticks). Its lifecycle belongs to the
// stack: closing it under an open stack makes every stack method fail.
func (s *GsmL3Stack) Registry() *Registry { return s.reg }

// Session exposes the BORROWED session view; never release it directly.
func (s *GsmL3Stack) Session() *Session { return s.sess }

// Orchestrator exposes the procedure chain for free orchestration.
func (s *GsmL3Stack) Orchestrator() *Orchestrator { return s.orch }

// Entity exposes the LAPDm link FSM: link control (SendSABME/Receive/SendDISC/
// HardRelease), the T200 timer and the per-link statistics live here.
func (s *GsmL3Stack) Entity() *LapdmEntity { return s.ent }

// ── Teardown ────────────────────────────────────────────────────────────────

// Close is the idempotent teardown, the single code path for explicit close and
// Go GC of an unreferenced stack. Release order is mandated by the C ABI (stop
// callback sources BEFORE freeing what they reference): 1) entity.Close() —
// entity_free kills all callback sources, THEN handle.Delete() reclaims the
// bridge token exactly once; 2) orchestrator.Close(); 3) registry.Close() — the
// session is released BY the registry, never directly (BORROWED). After Close,
// every method returns a typed error without a single FFI call.
func (s *GsmL3Stack) Close() error {
	if !s.closed.CompareAndSwap(false, true) {
		return nil // idempotent: no double free anywhere below
	}
	s.mu.Lock()
	defer s.mu.Unlock()
	if s.ent != nil {
		_ = s.ent.Close() // bridges dead from this instant (entity_free -> handle.Delete inside)
	}
	if s.orch != nil {
		_ = s.orch.Close()
	}
	if s.reg != nil {
		_ = s.reg.Close() // frees every session, incl. the borrowed one
	}
	return nil
}
