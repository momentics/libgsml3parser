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

// C ABI section S7 (orchestrator + response builders) and the two curated
// typed S9 builders the demo chain needs: build_cm_service_request, build_setup.
*/
import "C"

import (
	"unsafe"

	"sync/atomic"
)

// StepResult is the outcome of one orchestrator step. Reason is a SYNCHRONOUS
// copy of the thread-local C string taken inside feedStep — the library reuses
// that storage on the next call, so reading it later would be wrong (UB-by-usage).
// Code != CodeOK means the remaining fields carry no step information.
type StepResult struct {
	Action     int // GSML3_ACTION_*
	Token      int // GSML3_TOKEN_* (TokenNone when none)
	FinalState int // GSML3_STATE_*
	FinalType  int // GSML3_PROC_*
	Reason     string
	Code       Code // gsml3_error; CodeOK on a real step
}

// Orchestrator owns a gsml3_orchestrator: one procedure chain that owns the
// active procedure. Close releases it exactly once.
type Orchestrator struct {
	p      *C.gsml3_orchestrator
	closed atomic.Bool
}

// NewOrchestrator creates a fresh, empty procedure chain.
func NewOrchestrator() (*Orchestrator, error) {
	p := C.gsml3_orchestrator_new()
	if p == nil {
		return nil, lastError("orch.New", CodeOK)
	}
	return &Orchestrator{p: p}, nil
}

// feedStep converts one by-value gsml3_step_result into a Go StepResult. The
// thread-local `reason` string MUST be copied here, synchronously: any later
// C call clears or reuses it. If error != GSML3_OK the fields carry no step
// information and the typed error is returned instead.
func (o *Orchestrator) feedStep(op string, sr C.gsml3_step_result) (StepResult, error) {
	// Copy reason FIRST — before lastError() may touch any other thread-local state.
	reason := ""
	if sr.reason != nil {
		reason = C.GoString(sr.reason)
	}
	if e := int(sr.error); e != 0 {
		return StepResult{}, lastError(op, Code(e))
	}
	return StepResult{
		Action:     int(sr.action),
		Token:      int(sr.response_token),
		FinalState: int(sr.final_state),
		FinalType:  int(sr.final_type),
		Reason:     reason,
		Code:       CodeOK,
	}, nil
}

// sessionArg resolves a possibly-nil borrowed session to its raw pointer,
// enforcing the "refuse to touch a dead C handle after registry close" rule.
func (o *Orchestrator) sessionArg(s *Session) (*C.gsml3_session, error) {
	if s == nil {
		return nil, nil // legal per ABI: steps without a session
	}
	if s.p == nil {
		return nil, invalidArg("orch", "nil session pointer")
	}
	if s.reg != nil && s.reg.closed.Load() {
		return nil, &Error{Op: "orch", Code: CodeInvalidArg, Msg: "session's owning registry is closed"}
	}
	return s.p, nil
}

// messageArg resolves a possibly-nil message handle to its raw pointer.
func (o *Orchestrator) messageArg(m *Message) (*C.gsml3_message, error) {
	if m == nil {
		return nil, invalidArg("orch", "nil message")
	}
	if err := m.check("orch"); err != nil {
		return nil, err
	}
	return m.p, nil
}

// Feed runs one procedure step on a parsed L3 message. The session may be nil
// (some steps do not need one). Returns the typed StepResult.
func (o *Orchestrator) Feed(m *Message, s *Session) (StepResult, error) {
	if err := o.check("orch.Feed"); err != nil {
		return StepResult{}, err
	}
	mp, err := o.messageArg(m)
	if err != nil {
		return StepResult{}, err
	}
	sp, err := o.sessionArg(s)
	if err != nil {
		return StepResult{}, err
	}
	sr := C.gsml3_orchestrator_feed(o.p, mp, sp)
	return o.feedStep("orch.Feed", sr)
}

// FeedAuthChallenge feeds a typed authentication challenge (16-octet RAND in
// wire order + 4-octet big-endian SRES, octet 0 = MSB).
func (o *Orchestrator) FeedAuthChallenge(rand [16]byte, sres [4]byte) (StepResult, error) {
	if err := o.check("orch.FeedAuthChallenge"); err != nil {
		return StepResult{}, err
	}
	sr := C.gsml3_orchestrator_feed_auth_challenge(o.p, (*C.uint8_t)(unsafe.Pointer(&rand[0])), (*C.uint8_t)(unsafe.Pointer(&sres[0])))
	return o.feedStep("orch.FeedAuthChallenge", sr)
}

// FeedVLRDecision feeds a typed VLR decision (accept/reject, optional new TMSI).
// rejectCause is an MMRejectCause value.
func (o *Orchestrator) FeedVLRDecision(accept bool, hasNewTMSI bool, newTMSI uint32, rejectCause int) (StepResult, error) {
	if err := o.check("orch.FeedVLRDecision"); err != nil {
		return StepResult{}, err
	}
	sr := C.gsml3_orchestrator_feed_vlr_decision(o.p, boolToC(accept), boolToC(hasNewTMSI), C.uint32_t(newTMSI), C.int(rejectCause))
	return o.feedStep("orch.FeedVLRDecision", sr)
}

// FeedCiphering feeds a typed ciphering decision (algorithm octet + enable).
func (o *Orchestrator) FeedCiphering(algo byte, enable bool) (StepResult, error) {
	if err := o.check("orch.FeedCiphering"); err != nil {
		return StepResult{}, err
	}
	sr := C.gsml3_orchestrator_feed_ciphering(o.p, C.uint8_t(algo), boolToC(enable))
	return o.feedStep("orch.FeedCiphering", sr)
}

// FeedPagingTrigger feeds a typed paging trigger (TMSI or IMSI identity and a
// target channel). imsi is used when idType is the IMSI kind; pass "" for TMSI.
func (o *Orchestrator) FeedPagingTrigger(idType int, tmsi uint32, imsi string, targetChannel int) (StepResult, error) {
	if err := o.check("orch.FeedPagingTrigger"); err != nil {
		return StepResult{}, err
	}
	var imsiPtr *C.char
	if imsi != "" {
		cs := C.CString(imsi)
		defer C.free(unsafe.Pointer(cs))
		imsiPtr = cs
	}
	sr := C.gsml3_orchestrator_feed_paging_trigger(o.p, C.int(idType), C.uint32_t(tmsi), imsiPtr, C.int(targetChannel))
	return o.feedStep("orch.FeedPagingTrigger", sr)
}

// Tick advances the chain timers by deltaMs and returns the number of procedure
// failures that occurred.
func (o *Orchestrator) Tick(deltaMs uint32) (int, error) {
	if err := o.check("orch.Tick"); err != nil {
		return 0, err
	}
	return int(C.gsml3_orchestrator_tick(o.p, C.uint32_t(deltaMs))), nil
}

// RequiredSize returns the EXACT wire size of the pending (last-token) response:
// a buffer of this many bytes is guaranteed to be accepted by BuildResponse.
// Returns 0 when nothing can be built (NULL session, no pending response, or a
// missing ResponseContext parameter). Zero-alloc; not itself an error — the
// caller decides how to treat 0.
func (o *Orchestrator) RequiredSize(s *Session) (int, error) {
	if err := o.check("orch.RequiredSize"); err != nil {
		return 0, err
	}
	sp, err := o.sessionArg(s)
	if err != nil {
		return 0, err
	}
	return int(C.gsml3_orchestrator_required_size(o.p, sp)), nil
}

// BuildResponse builds the pending response into an EXACT-size buffer (the
// required_size -> build pattern, decision #8). A missing pending response or
// a parameter gap in the session's ResponseContext fails with the C code.
func (o *Orchestrator) BuildResponse(s *Session) ([]byte, error) {
	if err := o.check("orch.BuildResponse"); err != nil {
		return nil, err
	}
	sp, err := o.sessionArg(s)
	if err != nil {
		return nil, err
	}
	n := int(C.gsml3_orchestrator_required_size(o.p, sp))
	if n <= 0 {
		return nil, lastError("orch.BuildResponse", CodeOK) // C set INVALID_VALUE / missing-param
	}
	buf := make([]byte, n)
	w := C.gsml3_orchestrator_build_response(o.p, sp, bytePtr(buf), C.size_t(n))
	if int(w) != n {
		return nil, lastError("orch.BuildResponse", CodeOK) // 0 -> BUFFER_TOO_SMALL/INVALID_VALUE; mismatch defensive
	}
	return buf, nil
}

// TakeRetransmit drains the retransmission channel (consume-on-read); returns a
// GSML3_TOKEN_* value. After a non-TokenNone token, build and send that response.
func (o *Orchestrator) TakeRetransmit() (int, error) {
	if err := o.check("orch.TakeRetransmit"); err != nil {
		return 0, err
	}
	return int(C.gsml3_orchestrator_take_retransmit(o.p)), nil
}

// CancelAll cancels the active chain (resets to idle).
func (o *Orchestrator) CancelAll() error {
	if err := o.check("orch.CancelAll"); err != nil {
		return err
	}
	C.gsml3_orchestrator_cancel_all(o.p)
	return nil
}

// ChainPhase returns the current GSML3_PROC_* chain phase (GSML3_PROC_UNKNOWN
// when idle).
func (o *Orchestrator) ChainPhase() (int, error) {
	if err := o.check("orch.ChainPhase"); err != nil {
		return 0, err
	}
	return int(C.gsml3_orchestrator_chain_phase(o.p)), nil
}

// Closed reports whether the orchestrator was released.
func (o *Orchestrator) Closed() bool { return o != nil && o.closed.Load() }

// Close releases the gsml3_orchestrator handle exactly once (idempotent).
func (o *Orchestrator) Close() error {
	if !o.closed.CompareAndSwap(false, true) {
		return nil
	}
	C.gsml3_orchestrator_free(o.p) // NULL-safe
	o.p = nil
	return nil
}

func (o *Orchestrator) check(op string) error {
	if o == nil || o.p == nil || o.closed.Load() {
		return &Error{Op: op, Code: CodeInvalidArg, Msg: "orchestrator is closed"}
	}
	return nil
}

// ── Standalone response builders (stateless S7, 20) ────────────────────────
// All build into a fresh exact buffer via serializeInto (fixed caller buffer,
// decision #8); cause/channel parameters are range-checked in C; digit-string
// arguments (mcc/mnc/called digits/imsi) are C.CString-allocated and freed with
// C.free in the same call sequence (allocator symmetry — never gsml3_free).

// BuildResponseCMServiceAccept builds the (stateless) CM service accept.
func BuildResponseCMServiceAccept() ([]byte, error) {
	return serializeInto("resp.CMServiceAccept", func(out *C.uint8_t, maxLen C.size_t) C.size_t {
		return C.gsml3_response_build_cm_service_accept(out, maxLen)
	})
}

// BuildResponseCMServiceReject builds the CM service reject (MMRejectCause).
func BuildResponseCMServiceReject(mmCause int) ([]byte, error) {
	return serializeInto("resp.CMServiceReject", func(out *C.uint8_t, maxLen C.size_t) C.size_t {
		return C.gsml3_response_build_cm_service_reject(out, maxLen, C.int(mmCause))
	})
}

// BuildResponseIdentityRequest builds an identity request (GSML3_ID_*).
func BuildResponseIdentityRequest(idType int) ([]byte, error) {
	return serializeInto("resp.IdentityRequest", func(out *C.uint8_t, maxLen C.size_t) C.size_t {
		return C.gsml3_response_build_identity_request(out, maxLen, C.int(idType))
	})
}

// BuildResponseAuthenticationRequest builds an authentication request (16-octet RAND).
func BuildResponseAuthenticationRequest(rand [16]byte) ([]byte, error) {
	return serializeInto("resp.AuthenticationRequest", func(out *C.uint8_t, maxLen C.size_t) C.size_t {
		return C.gsml3_response_build_authentication_request(out, maxLen, (*C.uint8_t)(unsafe.Pointer(&rand[0])))
	})
}

// BuildResponseLocationUpdatingAccept builds a location updating accept. In the S7
// response builders the LAI components are digit STRINGS (mcc exactly 3 BCD
// digits "244", mnc 2-3 digits "05"), range-checked in C — distinct from the S9
// typed builders which take ints.
func BuildResponseLocationUpdatingAccept(mcc, mnc string, lac uint16, hasNewTMSI bool, newTMSI uint32) ([]byte, error) {
	return serializeInto("resp.LocationUpdatingAccept", func(out *C.uint8_t, maxLen C.size_t) C.size_t {
		mccP := C.CString(mcc)
		mncP := C.CString(mnc)
		defer C.free(unsafe.Pointer(mccP))
		defer C.free(unsafe.Pointer(mncP))
		return C.gsml3_response_build_location_updating_accept(out, maxLen, mccP, mncP, C.uint16_t(lac), boolToC(hasNewTMSI), C.uint32_t(newTMSI))
	})
}

// BuildResponseLocationUpdatingReject builds a location updating reject (MMRejectCause).
func BuildResponseLocationUpdatingReject(mmCause int) ([]byte, error) {
	return serializeInto("resp.LocationUpdatingReject", func(out *C.uint8_t, maxLen C.size_t) C.size_t {
		return C.gsml3_response_build_location_updating_reject(out, maxLen, C.int(mmCause))
	})
}

// BuildResponseTMSIReallocationCommand builds a TMSI reallocation command. mcc/mnc are digit strings (S7).
func BuildResponseTMSIReallocationCommand(mcc, mnc string, lac uint16, tmsi uint32) ([]byte, error) {
	return serializeInto("resp.TMSIReallocationCommand", func(out *C.uint8_t, maxLen C.size_t) C.size_t {
		mccP := C.CString(mcc)
		mncP := C.CString(mnc)
		defer C.free(unsafe.Pointer(mccP))
		defer C.free(unsafe.Pointer(mncP))
		return C.gsml3_response_build_tmsi_reallocation_command(out, maxLen, mccP, mncP, C.uint16_t(lac), C.uint32_t(tmsi))
	})
}

// BuildResponseChannelRelease builds a channel release (RRCause).
func BuildResponseChannelRelease(rrCause int) ([]byte, error) {
	return serializeInto("resp.ChannelRelease", func(out *C.uint8_t, maxLen C.size_t) C.size_t {
		return C.gsml3_response_build_channel_release(out, maxLen, C.int(rrCause))
	})
}

// BuildResponseCipheringModeCommand builds a ciphering mode command (algorithm octet).
func BuildResponseCipheringModeCommand(algo byte) ([]byte, error) {
	return serializeInto("resp.CipheringModeCommand", func(out *C.uint8_t, maxLen C.size_t) C.size_t {
		return C.gsml3_response_build_ciphering_mode_command(out, maxLen, C.uint8_t(algo))
	})
}

// BuildResponsePhysicalInformation builds a physical information message (TA).
func BuildResponsePhysicalInformation(ta byte) ([]byte, error) {
	return serializeInto("resp.PhysicalInformation", func(out *C.uint8_t, maxLen C.size_t) C.size_t {
		return C.gsml3_response_build_physical_information(out, maxLen, C.uint8_t(ta))
	})
}

// BuildResponseImmediateAssignment builds an immediate assignment (channel descriptor).
func BuildResponseImmediateAssignment(typeAndOffset int, tn, tsc byte, arfcn uint16, ta byte) ([]byte, error) {
	return serializeInto("resp.ImmediateAssignment", func(out *C.uint8_t, maxLen C.size_t) C.size_t {
		return C.gsml3_response_build_immediate_assignment(out, maxLen, C.int(typeAndOffset), C.uint8_t(tn), C.uint8_t(tsc), C.uint16_t(arfcn), C.uint8_t(ta))
	})
}

// BuildResponseAssignmentCommand builds an assignment command (channel descriptor).
func BuildResponseAssignmentCommand(typeAndOffset int, tn, tsc byte, arfcn uint16) ([]byte, error) {
	return serializeInto("resp.AssignmentCommand", func(out *C.uint8_t, maxLen C.size_t) C.size_t {
		return C.gsml3_response_build_assignment_command(out, maxLen, C.int(typeAndOffset), C.uint8_t(tn), C.uint8_t(tsc), C.uint16_t(arfcn))
	})
}

// BuildResponseCallProceeding builds a CC call proceeding (transaction id).
func BuildResponseCallProceeding(ti byte) ([]byte, error) {
	return serializeInto("resp.CallProceeding", func(out *C.uint8_t, maxLen C.size_t) C.size_t {
		return C.gsml3_response_build_call_proceeding(out, maxLen, C.uint8_t(ti))
	})
}

// BuildResponseAlerting builds a CC alerting (transaction id).
func BuildResponseAlerting(ti byte) ([]byte, error) {
	return serializeInto("resp.Alerting", func(out *C.uint8_t, maxLen C.size_t) C.size_t {
		return C.gsml3_response_build_alerting(out, maxLen, C.uint8_t(ti))
	})
}

// BuildResponseConnect builds a CC connect (transaction id).
func BuildResponseConnect(ti byte) ([]byte, error) {
	return serializeInto("resp.Connect", func(out *C.uint8_t, maxLen C.size_t) C.size_t {
		return C.gsml3_response_build_connect(out, maxLen, C.uint8_t(ti))
	})
}

// BuildResponseConnectAcknowledge builds a CC connect acknowledge (transaction id).
func BuildResponseConnectAcknowledge(ti byte) ([]byte, error) {
	return serializeInto("resp.ConnectAcknowledge", func(out *C.uint8_t, maxLen C.size_t) C.size_t {
		return C.gsml3_response_build_connect_acknowledge(out, maxLen, C.uint8_t(ti))
	})
}

// BuildResponseDisconnect builds a CC disconnect (transaction id + CCCause).
func BuildResponseDisconnect(ti byte, ccCause int) ([]byte, error) {
	return serializeInto("resp.Disconnect", func(out *C.uint8_t, maxLen C.size_t) C.size_t {
		return C.gsml3_response_build_disconnect(out, maxLen, C.uint8_t(ti), C.int(ccCause))
	})
}

// BuildResponseRelease builds a CC release (transaction id + CCCause).
func BuildResponseRelease(ti byte, ccCause int) ([]byte, error) {
	return serializeInto("resp.Release", func(out *C.uint8_t, maxLen C.size_t) C.size_t {
		return C.gsml3_response_build_release(out, maxLen, C.uint8_t(ti), C.int(ccCause))
	})
}

// BuildResponseReleaseComplete builds a CC release complete (transaction id).
func BuildResponseReleaseComplete(ti byte) ([]byte, error) {
	return serializeInto("resp.ReleaseComplete", func(out *C.uint8_t, maxLen C.size_t) C.size_t {
		return C.gsml3_response_build_release_complete(out, maxLen, C.uint8_t(ti))
	})
}

// BuildResponseSetup builds a CC setup with a BCD called-party digit string and transaction id.
func BuildResponseSetup(calledDigits string, ti byte) ([]byte, error) {
	return serializeInto("resp.Setup", func(out *C.uint8_t, maxLen C.size_t) C.size_t {
		dP := C.CString(calledDigits)
		defer C.free(unsafe.Pointer(dP))
		return C.gsml3_response_build_setup(out, maxLen, dP, C.uint8_t(ti))
	})
}

// BuildResponseFromToken builds the response for a GSML3_TOKEN_* value from the
// session's ResponseContext into an exact-size buffer (required_size pattern).
func BuildResponseFromToken(token int, s *Session) ([]byte, error) {
	var sp *C.gsml3_session
	if s != nil {
		if err := s.check("resp.BuildFromToken"); err != nil {
			return nil, err
		}
		sp = s.p
	}
	n := int(C.gsml3_response_required_size(C.int(token), sp))
	if n <= 0 {
		return nil, lastError("resp.BuildFromToken", CodeOK) // C set the specific code (INVALID_VALUE/ARG/BUFFER_TOO_SMALL)
	}
	buf := make([]byte, n)
	w := C.gsml3_response_build_from_token(C.int(token), sp, bytePtr(buf), C.size_t(n))
	if int(w) != n {
		return nil, lastError("resp.BuildFromToken", CodeOK)
	}
	return buf, nil
}

// ResponseRequiredSize returns the exact wire size of the response that
// BuildResponseFromToken would produce for token; 0 when it cannot be built.
func ResponseRequiredSize(token int, s *Session) (int, error) {
	var sp *C.gsml3_session
	if s != nil {
		if err := s.check("resp.RequiredSize"); err != nil {
			return 0, err
		}
		sp = s.p
	}
	return int(C.gsml3_response_required_size(C.int(token), sp)), nil
}

// ── Curated S9 typed builders (the two the demo chain needs) ───────────────
// These use the S9 argument conventions: build_cm_service_request /
// build_setup. Empty identity strings pass NULL.

// BuildCMServiceRequest builds a CM service request (typed S9). serviceType is an
// L3CMServiceType::TypeCode; idType is GSML3_ID_*; for a TMSI identity imsi may be
// "". Fixed caller buffer, exact-size result.
func BuildCMServiceRequest(serviceType int, idType int, tmsi uint32, imsi string) ([]byte, error) {
	return serializeInto("build.CMServiceRequest", func(out *C.uint8_t, maxLen C.size_t) C.size_t {
		var imsiP *C.char
		if imsi != "" {
			cs := C.CString(imsi)
			defer C.free(unsafe.Pointer(cs))
			imsiP = cs
		}
		return C.gsml3_build_cm_service_request(out, maxLen, C.int(serviceType), C.int(idType), C.uint32_t(tmsi), imsiP)
	})
}

// BuildSetup builds a CC setup (typed S9): transaction id plus BCD called-party
// digit string — the C argument order is (ti, called_digits). An empty digit
// string is rejected BEFORE the FFI boundary (NULL policy); out-of-domain
// content (non-digits) fails with the C INVALID_ARG code passed through.
func BuildSetup(ti byte, calledDigits string) ([]byte, error) {
	if calledDigits == "" {
		return nil, invalidArg("build.Setup", "empty called party digits")
	}
	return serializeInto("build.Setup", func(out *C.uint8_t, maxLen C.size_t) C.size_t {
		dP := C.CString(calledDigits)
		defer C.free(unsafe.Pointer(dP))
		return C.gsml3_build_setup(out, maxLen, C.uint8_t(ti), dP)
	})
}
