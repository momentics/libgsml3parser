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

/*
// This file needs the C ABI for the LAPDm entity types plus the bridge helper
// gsml3_new_entity_with_bridges, which is DEFINED (static) in the cgo.go
// preamble: every cgo source file only sees its OWN preamble, so its prototype
// is redeclared here. The canonical cgo callback pattern — the exported Go
// bridges (gsml3parserL3Bridge / gsml3parserL1Bridge below) are C-linkage
// symbols of the final binary; the static helper passes their addresses to the
// C core, so no function-pointer gymnastics happen on the Go side.
#include "gsml3parser/gsml3parser_c.h"

extern gsml3_lapdm_entity* gsml3_new_entity_with_bridges(int profile, void* user);
*/
import "C"

import (
	"sync"
	"sync/atomic"
	"unsafe"

	"runtime/cgo" // declared package name: cgo (the //export context-token helpers)
)

// Fixed test seam (mirrors the Python binding's _library.CALL_COUNTS): counts
// real crossings of gsml3_lapdm_entity_receive / gsml3_lapdm_entity_send_ui so
// tests can prove that a rejected path (invalid input, closed stack) performs
// ZERO FFI. Read via ffiReceiveCalls / ffiSendUICalls from the internal tests.
var (
	ffiReceiveCalls atomic.Int64
	ffiSendUICalls  atomic.Int64
)

// ── L3 event + sink interface ───────────────────────────────────────────────

// L3Event is one decoded L3 delivery captured by the L3 bridge. The C spans are
// valid ONLY during the callback ("transmit or copy synchronously, never
// retain them"), so Data is an owned copy made in the bridge; all processing
// (parse / orchestrate / build / send) happens AFTER the originating C call has
// returned — see the package-level Callback rules doc in cgo.go.
type L3Event struct {
	SAPI      int // GSML3_SAPI* value delivered by the entity
	Primitive int // GSML3_PRIM_* interlayer primitive (PrimL3UnitData, PrimL3EstablishConfirm, ...)
	Data      []byte
}

// eventSink is what a cgo.Handle resolves to: the queue owner for one entity's
// bridges. BOTH *GsmL3Stack (delegating into its LapdmEntity) and *LapdmEntity
// (standalone/test use) implement it, so a bridge never needs to know which
// kind of object created the entity — the handle stores the OWNER passed to
// newLapdmEntity(profile, owner).
type eventSink interface {
	enqueueL3(L3Event)
	enqueueTX([]byte)
}

// ── Callback bridges (//export -> C symbols in the final binary) ───────────
// The C entity invokes these SYNCHRONOUSLY inside gsml3_lapdm_entity_* calls.
// `user` is unsafe.Pointer(&e.handle): it points at a cgo.Handle FIELD, i.e. a
// single machine word containing NO Go pointer — therefore the cgo pointer
// rules permit the C core to RETAIN that address for the entity's lifetime and
// hand it back in every callback, and the owning Go object stays reachable via
// the handle's strong reference until Close(). Reading the field is legal
// inside the bridge because it happens while a C->Go call chain is in
// progress. NO cgo call, NO blocking and NO re-entry into gsml3_* may happen
// inside a bridge (ABI: "re-enter gsml3_* calls that only observe unrelated
// state"; freeing/mutating this entity mid-callback is forbidden).

//export gsml3parserL3Bridge
func gsml3parserL3Bridge(sapi C.int, primitive C.int, l3 *C.uint8_t, l3Len C.size_t, user unsafe.Pointer) {
	if user == nil {
		return // defensive: construction never passes NULL; drop any late/foreign callback
	}
	h := *(*cgo.Handle)(user)
	sink, ok := h.Value().(eventSink) // owner (*GsmL3Stack) or standalone entity (*LapdmEntity); deleted handles resolve to a foreign type and are dropped
	if !ok {
		return
	}
	// Zero-copy read over the C-owned span (valid only for THIS callback), then
	// exactly one owned copy for post-FFI processing — the queue-model rule.
	var data []byte
	if l3 != nil && l3Len > 0 { // nil zero-length spans are reported by some C paths; unsafe.Slice(nil, n>0) is UB
		view := unsafe.Slice((*byte)(l3), uintptr(l3Len)) // (*byte)(l3): identical underlying element type
		data = make([]byte, len(view))
		copy(data, view)
	}
	sink.enqueueL3(L3Event{SAPI: int(sapi), Primitive: int(primitive), Data: data})
}

//export gsml3parserL1Bridge
func gsml3parserL1Bridge(frame *C.uint8_t, frameLen C.size_t, user unsafe.Pointer) {
	if user == nil {
		return
	}
	h := *(*cgo.Handle)(user)
	sink, ok := h.Value().(eventSink)
	if !ok {
		return
	}
	var b []byte
	if frame != nil && frameLen > 0 {
		view := unsafe.Slice((*byte)(frame), uintptr(frameLen)) // zero-copy read inside the callback only; (*byte)(frame): identical underlying element type
		b = make([]byte, len(view))
		copy(b, view)
	}
	sink.enqueueTX(b)
}

// ── LapdmEntity (S5: 16 functions incl. the bridged entity_new) ────────────

// LapdmEntity is one LAPDm link FSM (GSM 04.06) owned by this binding:
// profile 0 = SDCCH, 1 = SACCH, 2 = FACCH (N200/N201/T200 per the C header).
// It OWNS its bridge context: a cgo.Handle registered BEFORE the entity is
// created, passed to C as `user`, and deleted AFTER entity_free in Close().
// The event queues (l3q/txq) live ONLY here — GsmL3Stack does not duplicate
// them, it drains them through these methods (one sink per entity).
type LapdmEntity struct {
	p      *C.gsml3_lapdm_entity
	closed atomic.Bool

	handle cgo.Handle // context token handed to C via `user`; VALUE = the eventSink owner
	l3q    []L3Event  // events captured by the L3 bridge; drained after each C call
	txq    [][]byte   // frames captured by the L1 bridge

	mu sync.Mutex // guards l3q/txq (bridges append from C->Go calls)
}

// newLapdmEntity is the single entity constructor: `owner` (the *GsmL3Stack for
// the stack path, nil for standalone/test use) becomes the cgo.Handle value the
// bridges resolve. The handle is registered BEFORE the C entity exists so no
// callback can ever observe a missing token; if C rejects the profile the
// handle is deleted and no dangling context remains.
func newLapdmEntity(profile int, owner *GsmL3Stack) (*LapdmEntity, error) {
	if profile < 0 || profile > 2 {
		return nil, invalidArg("lapdm.New", "profile must be 0 (SDCCH), 1 (SACCH) or 2 (FACCH)") // pre-FFI fast path; C re-checks as backstop
	}
	e := &LapdmEntity{}
	value := interface{}(e) // standalone: the entity itself is the sink
	if owner != nil {
		value = owner // stack path: *GsmL3Stack delegates into this entity's queues
	}
	e.handle = cgo.NewHandle(value)
	p := C.gsml3_new_entity_with_bridges(C.int(profile), unsafe.Pointer(&e.handle))
	if p == nil {
		e.handle.Delete()                          // entity not created: release the token, nothing retains &e.handle afterwards
		return nil, lastError("lapdm.New", CodeOK) // invalid profile / OOM reported by C
	}
	e.p = p
	return e, nil
}

// NewLapdmEntityStandalone creates an independent entity (no GsmL3Stack owner):
// the bridge handle then resolves to the ENTITY itself and its queues are
// drained manually via DrainL3()/DrainTX()/ResetTXCollector(). Used by the link
// lifecycle / T200 tests; production use goes through GsmL3Stack.
func NewLapdmEntityStandalone(profile int) (*LapdmEntity, error) {
	return newLapdmEntity(profile, nil)
}

// enqueueL3 appends one L3 event under the queue lock. A callback arriving
// after Close() is dropped (late-callback guard; the handle is already gone by
// then in practice — this keeps the path safe either way).
func (e *LapdmEntity) enqueueL3(ev L3Event) {
	e.mu.Lock()
	defer e.mu.Unlock()
	if !e.closed.Load() {
		e.l3q = append(e.l3q, ev)
	}
}

// enqueueTX appends one transmitted frame; same late-callback guard as enqueueL3.
func (e *LapdmEntity) enqueueTX(b []byte) {
	e.mu.Lock()
	defer e.mu.Unlock()
	if !e.closed.Load() {
		e.txq = append(e.txq, b)
	}
}

// DrainL3 takes-and-empties the unprocessed L3 event queue (called only AFTER a
// C call has returned — never inside one; no FFI).
func (e *LapdmEntity) DrainL3() []L3Event {
	e.mu.Lock()
	defer e.mu.Unlock()
	out := e.l3q
	e.l3q = nil
	return out
}

// DrainTX takes-and-empties the captured transmit-frame queue.
func (e *LapdmEntity) DrainTX() [][]byte {
	e.mu.Lock()
	defer e.mu.Unlock()
	out := e.txq
	e.txq = nil
	return out
}

// ResetTXCollector starts a clean transmit collector (frames queued by EARLIER
// operations are dropped by the caller's explicit choice — GsmL3Stack.SendFrame
// uses this so each call returns only the frames for THIS input).
func (e *LapdmEntity) ResetTXCollector() {
	e.mu.Lock()
	defer e.mu.Unlock()
	e.txq = nil
}

// Open transitions the FSM to LINK_RELEASED and selects the link parameters:
// sapi 0..15 (the C core rejects out-of-range values, sets the thread-local
// error and keeps the previous state), commandBit: true = BTS side (C/R=1),
// false = MS side (C/R=0).
func (e *LapdmEntity) Open(sapi int, commandBit bool) error {
	if err := e.check("lapdm.Open"); err != nil {
		return err
	}
	C.gsml3_lapdm_entity_open(e.p, C.int(sapi), boolToC(commandBit))
	if code := codeAfterCall(); code != CodeOK { // e.g. sapi outside 0..15
		return lastError("lapdm.Open", code)
	}
	return nil
}

// Receive feeds one raw LAPDm frame from L1 into the FSM. A frame shorter than
// address+control (< 2 bytes) is rejected before the FFI boundary; the C
// callbacks (bridges) fire synchronously during this call and only ENQUEUE —
// processing of what they captured must happen afterwards.
func (e *LapdmEntity) Receive(frame []byte) error {
	if err := e.check("lapdm.Receive"); err != nil {
		return err
	}
	if len(frame) < 2 {
		return invalidArg("lapdm.Receive", "frame shorter than address+control (< 2 bytes)")
	}
	ffiReceiveCalls.Add(1) // fixed FFI-call seam (test invariant: closed paths add nothing)
	C.gsml3_lapdm_entity_receive(e.p, bytePtr(frame), C.size_t(len(frame)))
	if code := codeAfterCall(); code != CodeOK { // C-internal failure of this frame
		return lastError("lapdm.Receive", code)
	}
	return nil
}

// SendUI transmits L3 data via a UI frame (no link establishment required —
// works in ANY FSM state). sapi selects the address-octet SAPI of the emitted
// frame and may differ from the Open() sapi; out-of-range sapi fails with the C
// INVALID_ARG code passed through.
func (e *LapdmEntity) SendUI(sapi int, l3 []byte) error {
	if err := e.check("lapdm.SendUI"); err != nil {
		return err
	}
	if len(l3) == 0 {
		return invalidArg("lapdm.SendUI", "empty L3 payload")
	}
	ffiSendUICalls.Add(1) // fixed FFI-call seam (see ffiReceiveCalls doc)
	rc := int(C.gsml3_lapdm_entity_send_ui(e.p, C.int(sapi), bytePtr(l3), C.size_t(len(l3))))
	if rc != 0 {
		return lastError("lapdm.SendUI", Code(rc))
	}
	return nil
}

// SendData transmits L3 data via I-frames (segmented as needed); the link MUST
// be established first, otherwise the C core fails with its error message
// passed through.
func (e *LapdmEntity) SendData(l3 []byte) error {
	if err := e.check("lapdm.SendData"); err != nil {
		return err
	}
	if len(l3) == 0 {
		return invalidArg("lapdm.SendData", "empty L3 payload")
	}
	rc := int(C.gsml3_lapdm_entity_send_data(e.p, bytePtr(l3), C.size_t(len(l3))))
	if rc != 0 {
		return lastError("lapdm.SendData", Code(rc))
	}
	return nil
}

// SendSABME requests link establishment; the FSM must be LINK_RELEASED.
func (e *LapdmEntity) SendSABME() error {
	if err := e.check("lapdm.SendSABME"); err != nil {
		return err
	}
	if rc := int(C.gsml3_lapdm_entity_send_sabme(e.p)); rc != 0 {
		return lastError("lapdm.SendSABME", Code(rc))
	}
	return nil
}

// SendDISC requests link release; the FSM must be LINK_ESTABLISHED.
func (e *LapdmEntity) SendDISC() error {
	if err := e.check("lapdm.SendDISC"); err != nil {
		return err
	}
	if rc := int(C.gsml3_lapdm_entity_send_disc(e.p)); rc != 0 {
		return lastError("lapdm.SendDISC", Code(rc))
	}
	return nil
}

// HardRelease drops the FSM immediately to LINK_RELEASED without transmitting.
// No C error condition exists for this operation (void, always applicable).
func (e *LapdmEntity) HardRelease() error {
	if err := e.check("lapdm.HardRelease"); err != nil {
		return err
	}
	C.gsml3_lapdm_entity_hard_release(e.p)
	return nil
}

// TickT200 advances the T200 retransmission timer by elapsedMs (profile
// dependent, e.g. 900 ms for SDCCH): returns 1 when a retransmission or
// abnormal release happened, 0 otherwise. A C-internal error surfaces with the
// thread-local message.
func (e *LapdmEntity) TickT200(elapsedMs uint32) (int, error) {
	if err := e.check("lapdm.TickT200"); err != nil {
		return 0, err
	}
	rc := int(C.gsml3_lapdm_entity_tick_t200(e.p, C.uint32_t(elapsedMs)))
	if rc < 0 { // -1 = internal error form (C contract)
		return 0, lastError("lapdm.TickT200", CodeOK)
	}
	return rc, nil
}

// State returns the current FSM state (StateLapdm* / GSML3_LAPDM_STATE_*).
// A closed entity reports UNUSED without any FFI — exactly the C-documented
// sentinel for a NULL entity (NULL policy).
func (e *LapdmEntity) State() int {
	if e == nil || e.p == nil {
		return StateLapdmUnused
	}
	return int(C.gsml3_lapdm_entity_state(e.p))
}

// IsEstablished reports LinkEstablished or ContentionResolution; a closed
// entity reports false (C NULL sentinel), no FFI.
func (e *LapdmEntity) IsEstablished() bool {
	if e == nil || e.p == nil {
		return false
	}
	return int(C.gsml3_lapdm_entity_is_established(e.p)) != 0
}

// FramesSent is the transmit counter; a closed entity reports 0 (C NULL sentinel), no FFI.
func (e *LapdmEntity) FramesSent() uint {
	if e == nil || e.p == nil {
		return 0
	}
	return uint(C.gsml3_lapdm_entity_frames_sent(e.p))
}

// FramesReceived is the receive counter; closed entity reports 0 (C NULL sentinel), no FFI.
func (e *LapdmEntity) FramesReceived() uint {
	if e == nil || e.p == nil {
		return 0
	}
	return uint(C.gsml3_lapdm_entity_frames_received(e.p))
}

// Retransmissions is the T200 retransmit counter; closed entity reports 0 (C NULL sentinel), no FFI.
func (e *LapdmEntity) Retransmissions() uint {
	if e == nil || e.p == nil {
		return 0
	}
	return uint(C.gsml3_lapdm_entity_retransmissions(e.p))
}

// Closed reports whether the entity was released.
func (e *LapdmEntity) Closed() bool { return e != nil && e.closed.Load() }

// Close releases the entity exactly once in the order the cgo rules mandate:
// (1) mark closed (late bridge events are dropped), (2) gsml3_lapdm_entity_free
// — from this instant C can never invoke the bridges again, (3) handle.Delete()
// so the context token cannot outlive the entity (no double free, no dangling
// user pointer). Idempotent.
func (e *LapdmEntity) Close() error {
	if !e.closed.CompareAndSwap(false, true) {
		return nil // idempotent: no double free, no double handle.Delete
	}
	C.gsml3_lapdm_entity_free(e.p) // NULL-safe; kills all pending callback sources
	e.p = nil
	e.handle.Delete() // entity gone FIRST: afterwards no bridge can resolve the token (order matters)
	return nil
}

func (e *LapdmEntity) check(op string) error {
	if e == nil || e.p == nil || e.closed.Load() {
		return &Error{Op: op, Code: CodeInvalidArg, Msg: "lapdm entity is closed"}
	}
	return nil
}

// ── Zero-copy decode (gsml3_lapdm_frame_decode) ────────────────────────────

// FrameInfo is a view of one decoded LAPDm frame. Payload is a SUB-SLICE of the
// input frame (zero copy): the C decoder points `info` into OUR buffer, which
// is stable Go heap memory for as long as the caller keeps `frame` alive; we
// store an offset, not a pointer, so no dangling reference can escape.
type FrameInfo struct {
	Format  int // GSML3_LAPDM_FMT_* (LapdmFmt*)
	UType   int // GSML3_LAPDM_U_* when Format == LapdmFmtU, else -1
	SType   int // GSML3_LAPDM_S_* when Format == LapdmFmtS, else -1
	NR, NS  uint8
	PF      int // Poll/Final bit
	MBit    int // message-complete bit (I frames)
	SAPI    int
	Command int // C/R: 1 = command, 0 = response

	Payload []byte
}

// DecodeFrame decodes one raw LAPDm frame (address + control [+ length +
// info]) with the zero-copy C decoder. A frame shorter than address+control
// (< 2 bytes) is rejected BEFORE the FFI boundary (NULL policy). The resulting
// Payload (when present) is a sub-slice of `frame` — valid while the caller
// keeps it alive, exactly like the C view contract.
func DecodeFrame(frame []byte) (*FrameInfo, error) {
	if len(frame) < 2 {
		return nil, invalidArg("lapdm.DecodeFrame", "frame shorter than address+control (< 2 bytes)")
	}
	var fi C.gsml3_lapdm_frame_info
	if rc := int(C.gsml3_lapdm_frame_decode(bytePtr(frame), C.size_t(len(frame)), &fi)); rc != 0 {
		return nil, lastError("lapdm.DecodeFrame", Code(rc))
	}
	out := &FrameInfo{
		Format:  int(fi.format),
		UType:   int(fi.u_type),
		SType:   int(fi.s_type),
		NR:      uint8(fi.nr),
		NS:      uint8(fi.ns),
		PF:      int(fi.pf),
		MBit:    int(fi.m_bit),
		SAPI:    int(fi.sapi),
		Command: int(fi.command),
	}
	if fi.info != nil && fi.info_len > 0 {
		base := uintptr(unsafe.Pointer(&frame[0]))
		infoPtr := unsafe.Pointer(fi.info)
		// The Go pointer was passed to C for this single call only and never
		// retained by it; reading the resulting view back while `frame` is
		// alive is legal. Offsetting into the slice (not keeping the pointer)
		// means no C-side reference to Go memory can ever escape.
		off := uintptr(infoPtr) - base
		if off >= 0 && int(off)+int(fi.info_len) <= len(frame) {
			out.Payload = frame[int(off) : int(off)+int(fi.info_len)] // zero-copy view
		} else {
			// Defensive fallback (must not happen per ABI): the C view escaped
			// our slice bounds, so take a copy instead of aliasing foreign memory.
			b := make([]byte, int(fi.info_len))
			copy(b, unsafe.Slice((*byte)(infoPtr), int(fi.info_len)))
			out.Payload = b
		}
	}
	return out, nil
}
