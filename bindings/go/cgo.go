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

// Package gsml3parser is a Go binding over the libgsml3parser C ABI
// (include/gsml3parser/gsml3parser_c.h): L3 parsing/serialization, A-bis RSL,
// the LAPDm entity with Go callbacks, and the BTS stack layer (registry /
// session / orchestrator / response builders). The v1 surface is C-ABI
// sections S1–S7 plus gsml3_build_cm_service_request and gsml3_build_setup
//  — 128 functions in total; the typed S8 getters are
// deliberately curated out and can be added without any ABI change.
//
// # Ownership
//
// Each constructor returns a value whose Close() releases EXACTLY ONCE the C
// handles it created (idempotent via an atomic flag, NULL-safe on the C side);
// after Close the raw pointer is nil'd out (take-pattern) so no method can
// ever touch a freed handle. *Session is BORROWED from its Registry and has
// no free of its own — gsml3_registry_free releases every session, so Session
// methods refuse to run once their registry is closed and the raw pointer
// must never outlive that Close (documented on the type).
//
// # Callback rules (queue model)
//
// The C entity invokes the registered bridges SYNCHRONOUSLY inside
// gsml3_lapdm_entity_* calls, and the l3/frame spans are valid ONLY for the
// duration of one callback: the ABI says "transmit or copy synchronously,
// never retain them", and it forbids freeing the owning entity (or mutating
// its FSM) from within its own callbacks. The bridges therefore do only
// memory-safe work: a zero-copy read of the C span via unsafe.Slice inside
// the callback plus exactly one owned copy appended to the queue. NO cgo
// call and no blocking may happen inside a bridge. GsmL3Stack.SendFrame /
// FeedL3 drain and process the queues only AFTER the C call has returned, so
// parse → orchestration → response build → transmission never re-enter the
// entity that is still mid-C-call (re-entry would be UB / a deadlock risk).
//
// # Threading
//
// Owned handles are single-thread per the C ABI ("one thread per handle"): a
// GsmL3Stack or LapdmEntity must not be used by two goroutines at once — its
// mutex serializes the receive/drain/orchestrate path as hygiene, it is NOT a
// sharing mechanism. A Registry created with shard count 4/8/16/32 makes its
// registry-mediated calls thread-safe (per-shard locks on the C side); direct
// session access runs WITHOUT the registry lock in either flavor (one thread
// per session, caller-synchronized — same rule as the C header).
package gsml3parser

/*
// Linking: the prebuilt shared core lives in build_bindings/bin at the
// repository root (two levels above this package directory): on Windows that
// is gsml3parser.dll + the MSVC import library gsml3parser.lib; on Linux it
// is libgsml3parser.so. Phase 0 / scripts/verify_bindings.ps1 create that flat
// directory. At RUN TIME the dynamic loader finds the library via PATH
// (Windows) or LD_LIBRARY_PATH (Linux); the unified gate puts build_bindings/bin
// there. If the directory does not exist yet, the link fails with a clear
// "cannot find -lgsml3parser" — build the shared core first.

#cgo CFLAGS: -I${SRCDIR}/../../include
#cgo LDFLAGS: -L${SRCDIR}/../../build_bindings/bin -lgsml3parser

#include <stdlib.h>
#include <stdint.h>
#include "gsml3parser/gsml3parser_c.h"

// Forward declarations of the Go callback bridges (defined with //export in
// lapdm.go): they are C-linkage symbols exported into the final binary. The
// helper below passes their addresses to the C core — the canonical cgo
// callback pattern, no pointer gymnastics from the Go side. It has EXTERNAL
// linkage (deliberately not static): cgo compiles every .go file of the
// package into a separate C translation unit, and lapdm.go's generated wrapper
// references this symbol across that boundary, so internal linkage would leave
// an undefined reference at link time (verified against go1.26). The
// package-unique name keeps it out of any collision risk.
extern void gsml3parserL3Bridge(int sapi, int primitive, const uint8_t* l3, size_t l3_len, void* user);
extern void gsml3parserL1Bridge(const uint8_t* frame, size_t frame_len, void* user);

gsml3_lapdm_entity* gsml3_new_entity_with_bridges(int profile, void* user) {
    return gsml3_lapdm_entity_new(profile,
                                  (gsml3_lapdm_l3_cb)gsml3parserL3Bridge,
                                  (gsml3_lapdm_l1_cb)gsml3parserL1Bridge,
                                  user);
}
*/
import "C"

import (
	"fmt"
	"unsafe"
)

// expectedABI must equal GSML3_ABI_VERSION in gsml3parser_c.h; bump both
// together when the C ABI changes incompatibly. The guard below fails loudly
// at package init if a prebuilt library of another revision is linked, so a
// stale binary is caught before the first FFI call instead of corrupting
// silently.
const expectedABI = 1

func init() {
	if got := int(C.gsml3_abi_version()); got != expectedABI {
		panic(fmt.Sprintf("gsml3parser: C ABI mismatch — this binding expects revision %d, the linked library reports %d (rebuild the shared core from this tree or update the binding)", expectedABI, got))
	}
}

// ── S1 core ───────────────────────────────────────────────────────────────

// Version returns the product version string reported by the loaded C core
// (gsml3_version → PROJECT_VERSION ← repo-root VERSION file.
// The C result is static storage: it is copied here and must never be
// freed.
func Version() string {
	p := C.gsml3_version()
	if p == nil {
		return ""
	}
	return C.GoString(p)
}

// ABIVersion returns the C ABI revision of the linked library; init() already
// guarantees it equals expectedABI (== GSML3_ABI_VERSION in the header).
func ABIVersion() int { return int(C.gsml3_abi_version()) }

// LastError copies the thread-local last error message synchronously ("").
// It is valid until the next successful C call clears or replaces it — hence
// every error path in this package reads and copies it at the failing call
// site. Use it for diagnostics after a failed call, not as a status channel.
func LastError() string {
	p := C.gsml3_last_error() // state observers never clear the pending error
	if p == nil {
		return ""
	}
	return C.GoString(p) // copy NOW: thread-local storage is reused on the next call
}

// LastErrorCode returns the gsml3_error code of the last failure (CodeOK when
// no error is pending). State observers such as this one never modify the
// pending error.
func LastErrorCode() Code { return Code(int(C.gsml3_last_error_code())) }

// Free releases any char* string returned by the C API (e.g. a hex or dump
// result read raw). The wrappers (Message.Hex / Message.Dump) do this for you
// in the copy-then-free idiom; call it directly only if you took a raw C
// string yourself. NULL-safe on the C side. NOTE: gsml3_free is a C++ delete[]
// for library allocations — strings created with C.CString must go through
// C.free, never through this function (allocator mismatch = UB).
func Free(p unsafe.Pointer) { C.gsml3_free(p) }

// ── Enum-mirror constants (values 1:1 with gsml3parser_c.h) ──────────────
// The curated set used by the v1 surface, the tests and the demo. Untyped so
// they compare directly with the int values returned by the wrappers; the C
// header is the source of truth for every value.

// Protocol discriminators (enum gsml3_pd).
const (
	PDGcc = 0x00
	PDBcc = 0x01
	PDCC  = 0x03 // Call Control
	PDMM  = 0x05 // Mobility Management
	PDRR  = 0x06 // Radio Resource
	PDGmm = 0x08
	PDSms = 0x09
	PDSm  = 0x0a // Switching Mode
	PDSS  = 0x0b
	PDLS  = 0x0c
	PDExt = 0x0e
	PdTst = 0x0f
)

// Mobile identity types (enum gsml3_id_type).
const (
	IDNoID   = 0
	IDIMSI   = 1
	IDIMEI   = 2
	IDIMEISV = 3
	IDTMSI   = 4
)

// Log levels (enum gsml3_log_level) for Config.SetLogLevel.
const (
	LogEmerg   = 0
	LogAlert   = 1
	LogCrit    = 2
	LogError   = 3
	LogWarning = 4
	LogNotice  = 5
	LogInfo    = 6
	LogDebug   = 7
)

// Interlayer primitives (enum gsml3_primitive): the values the L3 bridge
// delivers in L3Event.Primitive.
const (
	PrimL2Data                = 1
	PrimL3Data                = 2
	PrimL3DataConfirm         = 3
	PrimL3UnitData            = 4
	PrimL3EstablishRequest    = 5
	PrimL3EstablishIndication = 6
	PrimL3EstablishConfirm    = 7
	PrimL3ReleaseRequest      = 8
	PrimL3ReleaseConfirm      = 9
	PrimL3HardreleaseRequest  = 10
	PrimMdlErrorIndication    = 11
	PrimL3ReleaseIndication   = 12
	PrimPhConnect             = 13
	PrimHandoverAccess        = 14
)

// LAPDm frame formats, U/S types and FSM states (enums gsml3_lapdm_*).
const (
	LapdmFmtI = 0 // GSML3_LAPDM_FMT_I
	LapdmFmtS = 1 // GSML3_LAPDM_FMT_S
	LapdmFmtU = 2 // GSML3_LAPDM_FMT_U

	LapdmUUI    = 0x03
	LapdmUSabme = 0x2f
	LapdmUUA    = 0x63
	LapdmUDM    = 0x0f
	LapdmUDisc  = 0x08

	LapdmSRR  = 0x01
	LapdmSRej = 0x0d

	StateLapdmUnused               = 0
	StateLapdmLinkReleased         = 1
	StateLapdmAwaitingEstablish    = 2
	StateLapdmAwaitingRelease      = 3
	StateLapdmLinkEstablished      = 4
	StateLapdmContentionResolution = 5
)

// Procedure types (enum gsml3_proc_type) reported by Orchestrator.ChainPhase.
const (
	ProcLocationUpdate         = 0x01
	ProcAuthentication         = 0x02
	ProcCipheringMode          = 0x03
	ProcCallSetupMO            = 0x04
	ProcCallSetupMT            = 0x05
	ProcChannelAssignment      = 0x06
	ProcHandover               = 0x07
	ProcPaging                 = 0x08
	ProcCMServiceRequest       = 0x09
	ProcIMSIDetach             = 0x0a
	ProcCallRelease            = 0x0b
	ProcPeriodicLocationUpdate = 0x0c
	ProcUnknown                = 0xff
)

// Response tokens (enum gsml3_token): StepResult.Token / TakeRetransmit.
const (
	TokenNone                    = 0
	TokenImmediateAssignment     = 1
	TokenAssignmentCommand       = 2
	TokenChannelRelease          = 3
	TokenCipheringModeCommand    = 4
	TokenPhysicalInformation     = 5
	TokenHandoverCommand         = 6
	TokenPagingRequestType1      = 7
	TokenPagingRequestType2      = 8
	TokenPagingRequestType3      = 9
	TokenCMServiceAccept         = 10
	TokenCMServiceReject         = 11
	TokenIdentityRequest         = 12
	TokenAuthenticationRequest   = 13
	TokenLocationUpdatingAccept  = 14
	TokenLocationUpdatingReject  = 15
	TokenTMSIReallocationCommand = 16
	TokenCallProceeding          = 17
	TokenAlerting                = 18
	TokenConnect                 = 19
	TokenConnectAcknowledge      = 20
	TokenDisconnect              = 21
	TokenRelease                 = 22
	TokenReleaseComplete         = 23
	TokenSetup                   = 24
)

// Step actions (enum gsml3_action): StepResult.Action.
const (
	ActionContinue        = 0
	ActionSendResponse    = 1
	ActionWaitingExternal = 2
	ActionCompleted       = 3
	ActionFailed          = 4
)
