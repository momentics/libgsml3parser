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
#include <stdlib.h> // free: releases C.CString-allocated arguments (allocator symmetry)

#include "gsml3parser/gsml3parser_c.h"

// C ABI section S6: registry and borrowed session handles (29 functions).
*/
import "C"

import (
	"unsafe"

	"fmt"
	"sync/atomic"
)

// ── Registry (15 functions) ─────────────────────────────────────────────────

// Registry owns a gsml3_registry handle and the sessions it created. Close
// releases the registry AND every session in one C call (gsml3_registry_free).
// The shardCount selects the thread-safety flavor: 0 = plain single-thread,
// 4/8/16/32 = sharded (registry-mediated calls become thread-safe via
// per-shard locks on the C side).
type Registry struct {
	p          *C.gsml3_registry
	shardCount int
	closed     atomic.Bool
}

// NewRegistry creates a registry. The shard count is validated in Go FIRST
// (must be one of 0,4,8,16,32); the C side re-checks as a backstop and also
// reports allocation failures.
func NewRegistry(shardCount int) (*Registry, error) {
	if !shardCountAllowed(shardCount) {
		return nil, invalidArg("registry.New",
			fmt.Sprintf("shard count must be one of 0,4,8,16,32; got %d", shardCount))
	}
	p := C.gsml3_registry_new(C.int(shardCount))
	if p == nil {
		return nil, lastError("registry.New", CodeOK) // C backstop or NO_MEMORY
	}
	return &Registry{p: p, shardCount: shardCount}, nil
}

// Reserve pre-sizes the registry indexes for the expected session population.
// Cold path only (before churn begins).
func (r *Registry) Reserve(expected int) error {
	if err := r.check("registry.Reserve"); err != nil {
		return err
	}
	C.gsml3_registry_reserve(r.p, C.size_t(expected))
	return nil
}

// Count returns the number of active sessions in this registry.
func (r *Registry) Count() (int, error) {
	if err := r.check("registry.Count"); err != nil {
		return 0, err
	}
	return int(C.gsml3_registry_count(r.p)), nil
}

// CreateByTMSI creates a session keyed by an explicit TMSI. The reserved
// all-zero TMSI is rejected by C (GSML3_ERR_INVALID_ARG, TS 24.008) and the
// code is passed through; a duplicate key yields GSML3_ERR_DUPLICATE.
func (r *Registry) CreateByTMSI(tmsi uint32) (*Session, error) {
	if err := r.check("registry.CreateByTMSI"); err != nil {
		return nil, err
	}
	p := C.gsml3_registry_create_by_tmsi(r.p, C.uint32_t(tmsi))
	if p == nil {
		return nil, lastError("registry.CreateByTMSI", CodeOK)
	}
	return &Session{p: p, reg: r}, nil
}

// CreateByIMSI creates a session keyed by an auto-assigned TMSI for the given
// IMSI (1-15 ASCII digits). Plain registries only: sharded registries return
// GSML3_ERR_UNSUPPORTED from C. The assigned key is readable via
// Session.AssignedTMSI.
func (r *Registry) CreateByIMSI(imsi string) (*Session, error) {
	if err := r.check("registry.CreateByIMSI"); err != nil {
		return nil, err
	}
	if imsi == "" {
		return nil, invalidArg("registry.CreateByIMSI", "empty imsi")
	}
	cs := C.CString(imsi)
	defer C.free(unsafe.Pointer(cs)) // C.CString <-> C.free allocator symmetry
	p := C.gsml3_registry_create_by_imsi(r.p, cs)
	if p == nil {
		return nil, lastError("registry.CreateByIMSI", CodeOK)
	}
	return &Session{p: p, reg: r}, nil
}

// FindByTMSI looks up the session for a TMSI. Returns (nil, nil) when there is
// no such session (not an error).
func (r *Registry) FindByTMSI(tmsi uint32) (*Session, error) {
	if err := r.check("registry.FindByTMSI"); err != nil {
		return nil, err
	}
	p := C.gsml3_registry_find_by_tmsi(r.p, C.uint32_t(tmsi))
	if p == nil {
		return nil, nil
	}
	return &Session{p: p, reg: r}, nil
}

// FindByIMSI looks up the session for an IMSI digit string. (nil, nil) when
// absent.
func (r *Registry) FindByIMSI(imsi string) (*Session, error) {
	if err := r.check("registry.FindByIMSI"); err != nil {
		return nil, err
	}
	if imsi == "" {
		return nil, nil
	}
	cs := C.CString(imsi)
	defer C.free(unsafe.Pointer(cs))
	p := C.gsml3_registry_find_by_imsi(r.p, cs)
	if p == nil {
		return nil, nil
	}
	return &Session{p: p, reg: r}, nil
}

// FindByLink looks up the session assigned to (trx number, timeslot, lapdm
// link id). (nil, nil) when absent. The ARFCN of the assigned channel is NOT
// part of the link key (stored but not indexed), matching the C ABI.
func (r *Registry) FindByLink(trx, ts, lapdmLink uint8) (*Session, error) {
	if err := r.check("registry.FindByLink"); err != nil {
		return nil, err
	}
	p := C.gsml3_registry_find_by_link(r.p, C.uint8_t(trx), C.uint8_t(ts), C.uint8_t(lapdmLink))
	if p == nil {
		return nil, nil
	}
	return &Session{p: p, reg: r}, nil
}

// Remove deletes a session from the registry. Returns true if it was present
// and removed, false otherwise. NOTE: the Registry does not track its Session
// wrappers (borrowed C handles), so a Session wrapper for a removed handle is
// left dangling — stop using it; its methods only refuse via the closed
// registry check, which is why the binding keeps one Session per lookup and
// does not cache them across removals.
func (r *Registry) Remove(s *Session) (bool, error) {
	if err := r.check("registry.Remove"); err != nil {
		return false, err
	}
	if s == nil || s.p == nil {
		return false, invalidArg("registry.Remove", "nil session")
	}
	removed := int(C.gsml3_registry_remove(r.p, s.p)) == 1
	if code := codeAfterCall(); code != CodeOK { // ownership violation reported by C
		return removed, lastError("registry.Remove", code)
	}
	return removed, nil
}

// Clear removes ALL sessions. Plain registries only; a sharded registry makes
// the C call a no-op that reports GSML3_ERR_UNSUPPORTED, which is surfaced.
func (r *Registry) Clear() error {
	if err := r.check("registry.Clear"); err != nil {
		return err
	}
	C.gsml3_registry_clear(r.p)
	if code := codeAfterCall(); code != CodeOK {
		return lastError("registry.Clear", code)
	}
	return nil
}

// AssignChannel assigns a physical channel to s and updates the link index.
// s must belong to r; for a foreign/owned-elsewhere session the C call is a
// no-op reporting GSML3_ERR_INVALID_ARG (passed through). ch_type is a
// gsml3parser::ChannelType value (range-checked in C); ARFCN is stored but not
// part of the link key.
func (r *Registry) AssignChannel(s *Session, chType int, trx, ts uint8, arfcn uint16, lapdmLink uint8) error {
	if err := r.check("registry.AssignChannel"); err != nil {
		return err
	}
	if s == nil || s.p == nil {
		return invalidArg("registry.AssignChannel", "nil session")
	}
	C.gsml3_registry_assign_channel(r.p, s.p, C.int(chType), C.uint8_t(trx), C.uint8_t(ts), C.uint16_t(arfcn), C.uint8_t(lapdmLink))
	if code := codeAfterCall(); code != CodeOK {
		return lastError("registry.AssignChannel", code)
	}
	return nil
}

// ReleaseChannel clears the assigned channel and drops it from the link index.
// Same ownership rules (and INVALID_ARG pass-through) as AssignChannel.
func (r *Registry) ReleaseChannel(s *Session) error {
	if err := r.check("registry.ReleaseChannel"); err != nil {
		return err
	}
	if s == nil || s.p == nil {
		return invalidArg("registry.ReleaseChannel", "nil session")
	}
	C.gsml3_registry_release_channel(r.p, s.p)
	if code := codeAfterCall(); code != CodeOK {
		return lastError("registry.ReleaseChannel", code)
	}
	return nil
}

// TickTimers advances every active session timer by deltaMs and reports the
// expiries written into a fresh caller buffer of exactly `cap` slots. The C
// contract: events that do not fit are re-armed (1 ms) and surface on the next
// tick — never dropped. A Go slice is used as the out buffer: C writes into it
// only for the duration of this single call, so the cgo pointer rule holds
// (no retention). Returns the number actually written.
func (r *Registry) TickTimers(deltaMs uint32, cap int) ([]TimerExpiry, error) {
	if err := r.check("registry.TickTimers"); err != nil {
		return nil, err
	}
	if cap <= 0 {
		return nil, invalidArg("registry.TickTimers", "cap must be > 0")
	}
	buf := make([]C.gsml3_timer_expiry, cap)
	n := C.gsml3_registry_tick_timers(r.p, C.uint32_t(deltaMs), &buf[0], C.size_t(cap))
	out := make([]TimerExpiry, int(n))
	for i := 0; i < int(n); i++ {
		// Session is BORROWED (owned by the registry) — keep only the raw
		// pointer and never free it directly.
		out[i] = TimerExpiry{
			Session: unsafe.Pointer(buf[i].session),
			TimerID: int(buf[i].timer_id),
		}
	}
	return out, nil
}

// TickProcedures advances the procedure timers of all active sessions and
// returns the number of procedures that timed out.
func (r *Registry) TickProcedures(deltaMs uint32) (int, error) {
	if err := r.check("registry.TickProcedures"); err != nil {
		return 0, err
	}
	return int(C.gsml3_registry_tick_procedures(r.p, C.uint32_t(deltaMs))), nil
}

// Closed reports whether the registry was released.
func (r *Registry) Closed() bool { return r != nil && r.closed.Load() }

// Close releases the registry and ALL its sessions exactly once (idempotent).
func (r *Registry) Close() error {
	if !r.closed.CompareAndSwap(false, true) {
		return nil
	}
	C.gsml3_registry_free(r.p) // NULL-safe; frees every session with it
	r.p = nil
	return nil
}

func (r *Registry) check(op string) error {
	if r == nil || r.p == nil || r.closed.Load() {
		return &Error{Op: op, Code: CodeInvalidArg, Msg: "registry is closed"}
	}
	return nil
}

// ── Timer expiry (borrowed session + timer id) ──────────────────────────────

// TimerExpiry is one session-timer expiry event. Session is the RAW borrowed
// gsml3_session* reported by C: it is owned by the registry, valid only while
// that registry is open, and must never be freed directly (NULL policy). Use
// the registry to look the session back up by its assigned TMSI if needed.
type TimerExpiry struct {
	Session unsafe.Pointer
	TimerID int // GSML3_TIMER_* (0..18; 0xFF unknown)
}

// ── Session: a BORROWED view (14 functions) ────────────────────────────────
// A gsml3_session is pointer-sized and owned by its registry — it is never
// freed here (gsml3_registry_free releases every session). Every method gates
// on reg.closed FIRST so that after the owning registry closes we refuse to
// touch a dead C pointer instead of reading freed memory.

type Session struct {
	p   *C.gsml3_session // BORROWED — never freed by this binding
	reg *Registry        // the owner whose lifetime bounds this handle
}

// check enforces the borrowed-handle rule: no C access once the owning
// registry is closed (the raw pointer would dangle).
func (s *Session) check(op string) error {
	if s == nil || s.p == nil {
		return invalidArg(op, "nil session")
	}
	if s.reg != nil && s.reg.closed.Load() {
		return &Error{Op: op, Code: CodeInvalidArg, Msg: "session's owning registry is closed"}
	}
	return nil
}

// TMSI returns the session identity TMSI (0 for sessions keyed by IMSI).
func (s *Session) TMSI() (uint32, error) {
	if err := s.check("session.TMSI"); err != nil {
		return 0, err
	}
	return uint32(C.gsml3_session_tmsi(s.p)), nil
}

// AssignedTMSI returns the TMSI under which this session is keyed in its
// registry (the auto-assigned one for create-by-IMSI). 0 when not registered.
func (s *Session) AssignedTMSI() (uint32, error) {
	if err := s.check("session.AssignedTMSI"); err != nil {
		return 0, err
	}
	return uint32(C.gsml3_session_assigned_tmsi(s.p)), nil
}

// SetTMSI changes the session identity TMSI ONLY; it does NOT re-key the
// registry index (remove + create to re-index). The C setter is a no-op if the
// argument is out of domain and reports it in the thread-local error.
func (s *Session) SetTMSI(tmsi uint32) error {
	if err := s.check("session.SetTMSI"); err != nil {
		return err
	}
	C.gsml3_session_set_tmsi(s.p, C.uint32_t(tmsi))
	if code := codeAfterCall(); code != CodeOK {
		return lastError("session.SetTMSI", code)
	}
	return nil
}

// SetIMSI stores a BCD IMSI digit string (1-15 digits). C validates; invalid
// input is reported via the thread-local error.
func (s *Session) SetIMSI(digits string) error {
	if err := s.check("session.SetIMSI"); err != nil {
		return err
	}
	if digits == "" {
		return invalidArg("session.SetIMSI", "empty imsi digits")
	}
	cs := C.CString(digits)
	defer C.free(unsafe.Pointer(cs))
	C.gsml3_session_set_imsi(s.p, cs)
	if code := codeAfterCall(); code != CodeOK {
		return lastError("session.SetIMSI", code)
	}
	return nil
}

// IsRegistered reports the MS-registered flag.
func (s *Session) IsRegistered() (bool, error) {
	if err := s.check("session.IsRegistered"); err != nil {
		return false, err
	}
	return int(C.gsml3_session_is_registered(s.p)) != 0, nil
}

// SetRegistered sets the MS-registered flag.
func (s *Session) SetRegistered(v bool) error {
	if err := s.check("session.SetRegistered"); err != nil {
		return err
	}
	C.gsml3_session_set_registered(s.p, boolToC(v))
	return nil
}

// IsAuthenticated reports the MS-authenticated flag.
func (s *Session) IsAuthenticated() (bool, error) {
	if err := s.check("session.IsAuthenticated"); err != nil {
		return false, err
	}
	return int(C.gsml3_session_is_authenticated(s.p)) != 0, nil
}

// SetAuthenticated sets the MS-authenticated flag.
func (s *Session) SetAuthenticated(v bool) error {
	if err := s.check("session.SetAuthenticated"); err != nil {
		return err
	}
	C.gsml3_session_set_authenticated(s.p, boolToC(v))
	return nil
}

// IsCiphered reports the ciphering flag.
func (s *Session) IsCiphered() (bool, error) {
	if err := s.check("session.IsCiphered"); err != nil {
		return false, err
	}
	return int(C.gsml3_session_is_ciphered(s.p)) != 0, nil
}

// SetCiphered sets the ciphering flag.
func (s *Session) SetCiphered(v bool) error {
	if err := s.check("session.SetCiphered"); err != nil {
		return err
	}
	C.gsml3_session_set_ciphered(s.p, boolToC(v))
	return nil
}

// TimerStart starts (or restarts) an L3 timer by GSML3_TIMER_* id. Returns 1 on
// a fresh start and 0 on a restart; an out-of-range id returns 0 plus the C
// error.
func (s *Session) TimerStart(timerID int) (int, error) {
	if err := s.check("session.TimerStart"); err != nil {
		return 0, err
	}
	fresh := int(C.gsml3_session_timer_start(s.p, C.int(timerID)))
	if code := codeAfterCall(); code != CodeOK {
		return fresh, lastError("session.TimerStart", code)
	}
	return fresh, nil
}

// TimerStop stops an L3 timer by id (no-op when it is not running).
func (s *Session) TimerStop(timerID int) error {
	if err := s.check("session.TimerStop"); err != nil {
		return err
	}
	C.gsml3_session_timer_stop(s.p, C.int(timerID))
	return nil
}

// TimerRunning reports whether the L3 timer with id is currently running.
func (s *Session) TimerRunning(timerID int) (bool, error) {
	if err := s.check("session.TimerRunning"); err != nil {
		return false, err
	}
	return int(C.gsml3_session_timer_running(s.p, C.int(timerID))) != 0, nil
}

// TransactionPending returns the number of pending transactions on the session.
func (s *Session) TransactionPending() (int, error) {
	if err := s.check("session.TransactionPending"); err != nil {
		return 0, err
	}
	return int(C.gsml3_session_transaction_pending(s.p)), nil
}
