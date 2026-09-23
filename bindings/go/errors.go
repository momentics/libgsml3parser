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
#include "gsml3parser/gsml3parser_c.h"

// Error model of the binding: thread-local error observers (section S1).
*/
import "C"

import "fmt"

// Code mirrors enum gsml3_error from include/gsml3parser/gsml3parser_c.h 1:1.
type Code int

const (
	CodeOK              Code = 0  // GSML3_OK
	CodeInvalidArg      Code = 1  // GSML3_ERR_INVALID_ARG
	CodeTruncated       Code = 2  // GSML3_ERR_TRUNCATED
	CodeInvalidPD       Code = 3  // GSML3_ERR_INVALID_PD
	CodeInvalidMTI      Code = 4  // GSML3_ERR_INVALID_MTI
	CodeLengthMismatch  Code = 5  // GSML3_ERR_LENGTH_MISMATCH
	CodeInvalidIE       Code = 6  // GSML3_ERR_INVALID_IE
	CodeInvalidValue    Code = 7  // GSML3_ERR_INVALID_VALUE
	CodeUnsupported     Code = 8  // GSML3_ERR_UNSUPPORTED
	CodeSourceExhausted Code = 9  // GSML3_ERR_SOURCE_EXHAUSTED
	CodeNoMemory        Code = 10 // GSML3_ERR_NO_MEMORY
	CodeBufferTooSmall  Code = 11 // GSML3_ERR_BUFFER_TOO_SMALL
	CodeInternal        Code = 12 // GSML3_ERR_INTERNAL
	CodeDuplicate       Code = 13 // GSML3_ERR_DUPLICATE
)

// String renders the code under the name the C header uses for it.
func (c Code) String() string {
	switch c {
	case CodeOK:
		return "OK"
	case CodeInvalidArg:
		return "INVALID_ARG"
	case CodeTruncated:
		return "TRUNCATED"
	case CodeInvalidPD:
		return "INVALID_PD"
	case CodeInvalidMTI:
		return "INVALID_MTI"
	case CodeLengthMismatch:
		return "LENGTH_MISMATCH"
	case CodeInvalidIE:
		return "INVALID_IE"
	case CodeInvalidValue:
		return "INVALID_VALUE"
	case CodeUnsupported:
		return "UNSUPPORTED"
	case CodeSourceExhausted:
		return "SOURCE_EXHAUSTED"
	case CodeNoMemory:
		return "NO_MEMORY"
	case CodeBufferTooSmall:
		return "BUFFER_TOO_SMALL"
	case CodeInternal:
		return "INTERNAL"
	case CodeDuplicate:
		return "DUPLICATE"
	default:
		return fmt.Sprintf("UNKNOWN(%d)", int(c))
	}
}

// Error is the typed failure of one operation (planK decision #9). Code names
// the C failure class; Op names the Go call site; Msg is a SYNCHRONOUS copy of
// the thread-local gsml3_last_error() message taken at the failing call —
// every later successful FFI call on this thread clears the pending error, so
// reading it afterwards would lose or corrupt the report.
type Error struct {
	Op   string
	Code Code
	Msg  string
}

// Error implements the error interface: "gsml3: <op>: <message> (<CODE>)".
func (e *Error) Error() string {
	if e.Msg != "" {
		return fmt.Sprintf("gsml3: %s: %s (%s)", e.Op, e.Msg, e.Code)
	}
	return fmt.Sprintf("gsml3: %s: %s", e.Op, e.Code)
}

// invalidArg builds the standard pre-FFI validation error (NULL policy: input
// where C is documented to fail is rejected at the binding level).
func invalidArg(op, msg string) *Error {
	return &Error{Op: op, Code: CodeInvalidArg, Msg: msg}
}

// lastError reads the thread-local error state of the C core and returns the
// typed error for op. If explicit is not CodeOK it wins (the failing function
// already reported its gsml3_error code in the return value); otherwise the
// code comes from gsml3_last_error_code(). The message copy MUST happen here:
// both values are thread-local and reused on the next call.
func lastError(op string, explicit Code) *Error {
	code := explicit
	if code == CodeOK {
		code = Code(int(C.gsml3_last_error_code()))
	}
	msg := ""
	if p := C.gsml3_last_error(); p != nil { // state observers never clear the pending error
		msg = C.GoString(p)
	}
	return &Error{Op: op, Code: code, Msg: msg}
}

// codeAfterCall reads the thread-local error code set (if any) by the call
// that just ran on this goroutine. Void-returning C operations report
// out-of-domain arguments / ownership violations this way.
func codeAfterCall() Code {
	return Code(int(C.gsml3_last_error_code()))
}
