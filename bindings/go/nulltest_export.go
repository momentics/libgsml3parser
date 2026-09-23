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
#include <stdint.h>
#include <stdlib.h>
#include "gsml3parser/gsml3parser_c.h"
*/
import "C"

import "unsafe"

// TEST-ONLY raw access to the C ABI (planK step 2.8 "test-only NULL-export").
//
// Why this is a regular (non-_test) cgo file: the gc toolchain REJECTS
// `import "C"` inside _test.go files ("use of cgo in test ... not supported" —
// verified on go1.26, windows/linux alike), so the plan's nulltest_export_test.go
// shape is not compilable; this file is its working equivalent. It stays out of
// any production binary: every function here is unexported and referenced ONLY
// by the test files, so the final-link tree-shaker removes them from program
// builds (they exist only to give the tests a seam that reaches the C core with
// raw NULLs / undersized buffers — the exact calls the production wrappers
// deliberately refuse to make).
//
// The helpers take/return universal Go types (unsafe.Pointer / int) so they are
// callable from any test file in the package regardless of cgo preamble
// isolation.

func rawMessageName(msg unsafe.Pointer) string {
	if p := C.gsml3_message_name((*C.gsml3_message)(msg)); p != nil {
		return C.GoString(p)
	}
	return ""
}

func rawMessagePd(msg unsafe.Pointer) int  { return int(C.gsml3_message_pd((*C.gsml3_message)(msg))) }
func rawMessageMti(msg unsafe.Pointer) int { return int(C.gsml3_message_mti((*C.gsml3_message)(msg))) }
func rawMessageTi(msg unsafe.Pointer) int  { return int(C.gsml3_message_ti((*C.gsml3_message)(msg))) }

// rawMessageSize mirrors gsml3_message_size (0 for NULL).
func rawMessageSize(msg unsafe.Pointer) int {
	return int(C.gsml3_message_size((*C.gsml3_message)(msg)))
}

// rawMessageWrite writes the message into a fresh maxLen-byte buffer; returns
// the C byte count (0 = error, with the thread-local code set — read it right
// after, before any other successful call).
func rawMessageWrite(msg unsafe.Pointer, maxLen int) (int, Code) {
	var n C.size_t
	if maxLen > 0 {
		buf := make([]byte, maxLen)
		n = C.gsml3_message_write((*C.gsml3_message)(msg), (*C.uint8_t)(unsafe.Pointer(&buf[0])), C.size_t(maxLen))
	} else {
		n = C.gsml3_message_write((*C.gsml3_message)(msg), nil, 0)
	}
	return int(n), Code(int(C.gsml3_last_error_code())) // synchronous copy: state observers never clear
}

// rawMessageHexNull reports whether gsml3_message_hex(NULL) yields NULL (no panic,
// no allocation).
func rawMessageHexNull() bool { return C.gsml3_message_hex(nil) == nil }

// rawParseL3IntoNull returns the C code of gsml3_parse_l3_into(NULL, ...) — the
// documented contract is "not OK" on a NULL handle.
func rawParseL3IntoNull() Code {
	data := []byte{0x60, 0x0D, 0x00}
	return Code(int(C.gsml3_parse_l3_into(nil, (*C.uint8_t)(unsafe.Pointer(&data[0])), C.size_t(len(data)), nil)))
}

// rawAllFreesNull exercises every documented NULL-safe release path with a NULL
// argument: any crash/UB here would be a core contract violation.
func rawAllFreesNull() {
	C.gsml3_config_free(nil)
	C.gsml3_message_free(nil)
	C.gsml3_rsl_free(nil)
	C.gsml3_lapdm_entity_free(nil)
	C.gsml3_registry_free(nil)
	C.gsml3_orchestrator_free(nil)
	C.gsml3_free(nil)
}

// rawEntityNullStats mirrors the NULL-safe entity observations: state -> UNUSED,
// established/stats -> 0.
func rawEntityNullStats() (state int, established int, sent, received, retrans uint) {
	state = int(C.gsml3_lapdm_entity_state(nil))
	established = int(C.gsml3_lapdm_entity_is_established(nil))
	sent = uint(C.gsml3_lapdm_entity_frames_sent(nil))
	received = uint(C.gsml3_lapdm_entity_frames_received(nil))
	retrans = uint(C.gsml3_lapdm_entity_retransmissions(nil))
	return
}

// rawParseL3Hex parses a hex string directly (returns nil on C failure) — the
// handle stays alive until the test closes it through rawFreeMessage.
func rawParseL3Hex(hex string) unsafe.Pointer {
	cs := C.CString(hex)
	defer C.free(unsafe.Pointer(cs)) // allocator symmetry
	return unsafe.Pointer(C.gsml3_parse_l3_hex(cs, nil))
}

func rawFreeMessage(msg unsafe.Pointer) { C.gsml3_message_free((*C.gsml3_message)(msg)) }

// rawRslBuildTooSmall drives an RSL builder into a deliberately undersized buffer
// (4 bytes) with arguments that need more — mirrors the C Builder_BufferTooSmall
// test: 0 written + GSML3_ERR_BUFFER_TOO_SMALL reported, never retried.
func rawRslBuildTooSmall() (int, Code) {
	buf := make([]byte, 4)
	l3 := []byte{0x50, 0x84, 0x55}
	n := C.gsml3_rsl_build_data_req((*C.uint8_t)(unsafe.Pointer(&buf[0])), C.size_t(len(buf)),
		0x10, 1, (*C.uint8_t)(unsafe.Pointer(&l3[0])), C.size_t(len(l3)))
	return int(n), Code(int(C.gsml3_last_error_code())) // synchronous copy right after the failing call
}
