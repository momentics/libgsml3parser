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

// C ABI sections S2-S4: parser config, L3 message handle, A-bis RSL.
*/
import "C"

import (
	"slices"
	"sync/atomic"
	"unsafe"
)

// ── Small package-private FFI helpers ──────────────────────────────────────

// boolToC converts a Go bool into the C 1/0 convention.
func boolToC(b bool) C.int {
	if b {
		return 1
	}
	return 0
}

// bytePtr returns a *uint8 for a byte buffer (nil when empty), the form every
// serializer/parser in the C ABI accepts. The pointer is used only for the
// duration of the single C call — the C side never retains it.
func bytePtr(b []byte) *C.uint8_t {
	if len(b) == 0 {
		return nil
	}
	return (*C.uint8_t)(unsafe.Pointer(&b[0]))
}

var allowedShardCounts = [5]int{0, 4, 8, 16, 32}

func shardCountAllowed(v int) bool { return slices.Contains(allowedShardCounts[:], v) }

// builderBufSize is the fixed caller-provided buffer for every standalone
// (out, maxlen) serializer wrapper: exact-size builds, no guess-and-grow.
// RSL frames, L3 messages and built responses are
// protocol-bounded and comfortably fit 512 bytes; when a frame does not fit,
// the C core reports GSML3_ERR_BUFFER_TOO_SMALL (CodeBufferTooSmall) and it is
// surfaced, never silently retried with a bigger buffer.
const builderBufSize = 512

// serializeInto runs one (uint8_t* out, size_t maxlen) serializer of the C ABI
// into a fresh fixed-size buffer and returns the written prefix as an
// independent caller-owned slice. n == 0 surfaces the synchronous
// lastError() (cause-specific: invalid argument, buffer too small, ...).
func serializeInto(op string, call func(out *C.uint8_t, maxLen C.size_t) C.size_t) ([]byte, error) {
	buf := make([]byte, builderBufSize)
	n := call(bytePtr(buf), C.size_t(len(buf)))
	if int64(n) <= 0 {
		return nil, lastError(op, CodeOK)
	}
	if uint64(n) > uint64(len(buf)) {
		return nil, &Error{Op: op, Code: CodeInternal, Msg: "C serializer reported more bytes than the buffer size"}
	}
	out := make([]byte, int(n))
	copy(out, buf[:int(n)])
	return out, nil
}

// ── S2 config (4 functions) ─────────────────────────────────────────────────

// Config owns a gsml3_config handle. Close releases it exactly once (idempotent).
type Config struct {
	p      *C.gsml3_config
	closed atomic.Bool
}

// NewConfig creates the default parser configuration (log level WARNING,
// lenient framing). Returns an error only on allocation failure in C.
func NewConfig() (*Config, error) {
	c := C.gsml3_config_new()
	if c == nil {
		return nil, lastError("config.New", CodeOK)
	}
	return &Config{p: c}, nil
}

// SetLogLevel sets the log level (GSML3_LOG_* / Log* constants); the C core
// ignores out-of-range values silently (documented behavior, not an error).
func (c *Config) SetLogLevel(level int) error {
	if err := c.check("config.SetLogLevel"); err != nil {
		return err
	}
	C.gsml3_config_set_log_level(c.p, C.int(level))
	return nil
}

// SetStrictFraming switches strict framing on/off: a frame whose message does
// not consume the entire input is rejected.
func (c *Config) SetStrictFraming(on bool) error {
	if err := c.check("config.SetStrictFraming"); err != nil {
		return err
	}
	C.gsml3_config_set_strict_framing(c.p, boolToC(on))
	return nil
}

// Closed reports whether the handle was already released.
func (c *Config) Closed() bool { return c != nil && c.closed.Load() }

// Close releases the gsml3_config handle exactly once (idempotent, NULL-safe in C).
func (c *Config) Close() error {
	if !c.closed.CompareAndSwap(false, true) {
		return nil // already closed — no double free, by construction
	}
	C.gsml3_config_free(c.p) // gsml3_*_free are documented NULL-safe
	c.p = nil
	return nil
}

func (c *Config) check(op string) error {
	if c == nil || c.p == nil || c.closed.Load() {
		return &Error{Op: op, Code: CodeInvalidArg, Msg: "config is closed"}
	}
	return nil
}

// configArg validates an optional Config argument: nil is legal (default
// config); a closed one must not be passed to C as a dead pointer.
func configArg(cfg *Config, op string) (*C.gsml3_config, error) {
	if cfg == nil {
		return nil, nil
	}
	if err := cfg.check(op); err != nil {
		return nil, err
	}
	return cfg.p, nil
}

// ── S3 message (12 functions) ───────────────────────────────────────────────

// Message owns a gsml3_message handle: the parsed variant plus all derived
// data. Every byte/string output of this type is an independent copy owned by
// the caller; Close() releases the handle exactly once.
type Message struct {
	p      *C.gsml3_message
	closed atomic.Bool
}

// Parse parses raw L3 bytes (header + body) into a fresh handle. Empty/nil
// input is rejected BEFORE the FFI boundary (NULL policy); C errors are
// surfaced with their synchronous message. cfg may be nil.
func Parse(data []byte, cfg *Config) (*Message, error) {
	if len(data) == 0 {
		return nil, invalidArg("message.Parse", "empty input")
	}
	ch, err := configArg(cfg, "message.Parse")
	if err != nil {
		return nil, err
	}
	p := C.gsml3_parse_l3(bytePtr(data), C.size_t(len(data)), ch)
	if p == nil {
		return nil, lastError("message.Parse", CodeOK)
	}
	return &Message{p: p}, nil
}

// ParseHex parses a hex string (spaces allowed, e.g. "60 0D 00") into a fresh
// handle. Empty input is rejected before FFI; C errors are passed through.
func ParseHex(hex string, cfg *Config) (*Message, error) {
	if hex == "" {
		return nil, invalidArg("message.ParseHex", "empty input")
	}
	ch, err := configArg(cfg, "message.ParseHex")
	if err != nil {
		return nil, err
	}
	cs := C.CString(hex)
	defer C.free(unsafe.Pointer(cs)) // C.CString memory is released with C.free; gsml3_free releases only library-allocated strings (allocator symmetry)
	p := C.gsml3_parse_l3_hex(cs, ch)
	if p == nil {
		return nil, lastError("message.ParseHex", CodeOK)
	}
	return &Message{p: p}, nil
}

// ParseInto reparses data into THIS handle in place (zero-extra-allocation
// reparse hot path). On failure the previous content of the handle is kept —
// the documented contract of gsml3_parse_l3_into.
func (m *Message) ParseInto(data []byte, cfg *Config) error {
	if err := m.check("message.ParseInto"); err != nil {
		return err
	}
	if len(data) == 0 {
		return invalidArg("message.ParseInto", "empty input")
	}
	ch, err := configArg(cfg, "message.ParseInto")
	if err != nil {
		return err
	}
	if rc := int(C.gsml3_parse_l3_into(m.p, bytePtr(data), C.size_t(len(data)), ch)); rc != 0 {
		return lastError("message.ParseInto", Code(rc))
	}
	return nil
}

// Name returns the message type name ("CMServiceAccept", ...). Static C
// storage — no free; a closed handle reports "" (the documented NULL sentinel).
func (m *Message) Name() string {
	if m == nil || m.p == nil {
		return ""
	}
	if p := C.gsml3_message_name(m.p); p != nil {
		return C.GoString(p)
	}
	return ""
}

// Pd returns the protocol discriminator (PD* constants), or -1 for a closed
// handle (the documented NULL sentinel of gsml3_message_pd).
func (m *Message) Pd() int {
	if m == nil || m.p == nil {
		return -1
	}
	return int(C.gsml3_message_pd(m.p))
}

// Mti returns the message type identifier (0..255, RR short 256..511), or -1
// for a closed handle (documented NULL sentinel).
func (m *Message) Mti() int {
	if m == nil || m.p == nil {
		return -1
	}
	return int(C.gsml3_message_mti(m.p))
}

// Ti returns the CC/SS transaction identifier (0..7, 0 otherwise), and 0 for a
// closed handle (documented NULL sentinel).
func (m *Message) Ti() int {
	if m == nil || m.p == nil {
		return 0
	}
	return int(C.gsml3_message_ti(m.p))
}

// Size returns the exact wire size of the serialized message (zero-alloc C
// call); a buffer of exactly this size is guaranteed to be accepted by Write.
func (m *Message) Size() (int, error) {
	if err := m.check("message.Size"); err != nil {
		return 0, err
	}
	return int(C.gsml3_message_size(m.p)), nil
}

// Write serializes the message into a freshly allocated EXACT-size buffer
// (no guess-and-grow: size() then write into make([]byte, n)).
func (m *Message) Write() ([]byte, error) {
	if err := m.check("message.Write"); err != nil {
		return nil, err
	}
	n := int(C.gsml3_message_size(m.p))
	if n <= 0 {
		return nil, &Error{Op: "message.Write", Code: CodeInternal, Msg: "handle reports zero wire size"}
	}
	buf := make([]byte, n)
	w := C.gsml3_message_write(m.p, bytePtr(buf), C.size_t(n))
	if int64(w) <= 0 {
		return nil, lastError("message.Write", CodeOK)
	}
	return buf[:int(w)], nil
}

// Hex returns the lowercase no-space hex serialization. Implements the
// copy-then-free idiom for char* results: C.GoString copies synchronously and
// gsml3_free releases the library allocation in the same call sequence.
func (m *Message) Hex() (string, error) {
	if err := m.check("message.Hex"); err != nil {
		return "", err
	}
	p := C.gsml3_message_hex(m.p)
	if p == nil {
		return "", lastError("message.Hex", CodeOK)
	}
	s := C.GoString(p)
	C.gsml3_free(unsafe.Pointer(p)) // copy is taken — release now
	return s, nil
}

// Dump returns the human-readable field dump (message name first line plus
// every information element). Copy-then-free idiom as in Hex.
func (m *Message) Dump() (string, error) {
	if err := m.check("message.Dump"); err != nil {
		return "", err
	}
	p := C.gsml3_message_dump(m.p)
	if p == nil {
		return "", lastError("message.Dump", CodeOK)
	}
	s := C.GoString(p)
	C.gsml3_free(unsafe.Pointer(p)) // copy is taken — release now
	return s, nil
}

// Closed reports whether the handle was already released.
func (m *Message) Closed() bool { return m != nil && m.closed.Load() }

// Close releases the gsml3_message handle exactly once (idempotent). After
// Close, metadata getters report their C NULL sentinels without any FFI call.
func (m *Message) Close() error {
	if !m.closed.CompareAndSwap(false, true) {
		return nil
	}
	C.gsml3_message_free(m.p) // NULL-safe
	m.p = nil
	return nil
}

func (m *Message) check(op string) error {
	if m == nil || m.p == nil || m.closed.Load() {
		return &Error{Op: op, Code: CodeInvalidArg, Msg: "message is closed"}
	}
	return nil
}

// ── S4 RSL: A-bis (TS 48.058), 25 functions ────────────────────────────────

// RslFrame owns a gsml3_rsl handle. The C core copies the parsed input, so
// the IE/L3 views returned here stay valid exactly while THIS handle is alive;
// L3/IE return independent copies (the binding never keeps raw C pointers).
type RslFrame struct {
	p      *C.gsml3_rsl
	closed atomic.Bool
}

// RslParse parses one A-bis RSL frame. Empty input is rejected before FFI.
func RslParse(data []byte) (*RslFrame, error) {
	if len(data) == 0 {
		return nil, invalidArg("rsl.Parse", "empty input")
	}
	p := C.gsml3_rsl_parse(bytePtr(data), C.size_t(len(data)))
	if p == nil {
		return nil, lastError("rsl.Parse", CodeOK)
	}
	return &RslFrame{p: p}, nil
}

// Name returns the RSL message name ("DATA_REQ", "CHAN_ACTIV", ...). Static
// C storage; closed handle reports "" (documented sentinel).
func (f *RslFrame) Name() string {
	if f == nil || f.p == nil {
		return ""
	}
	if p := C.gsml3_rsl_name(f.p); p != nil {
		return C.GoString(p)
	}
	return ""
}

// Discriminator returns the 7-bit RSL discriminator, -1 for a closed handle.
func (f *RslFrame) Discriminator() int {
	if f == nil || f.p == nil {
		return -1
	}
	return int(C.gsml3_rsl_discriminator(f.p))
}

// MsgType returns the RSL message-type byte, -1 for a closed handle.
func (f *RslFrame) MsgType() int {
	if f == nil || f.p == nil {
		return -1
	}
	return int(C.gsml3_rsl_msg_type(f.p))
}

// ChanNr returns the RSL channel number, -1 for a closed handle.
func (f *RslFrame) ChanNr() int {
	if f == nil || f.p == nil {
		return -1
	}
	return int(C.gsml3_rsl_chan_nr(f.p))
}

// LinkID returns the LAPDm link identifier (RLL), -1 for a closed handle.
func (f *RslFrame) LinkID() int {
	if f == nil || f.p == nil {
		return -1
	}
	return int(C.gsml3_rsl_link_id(f.p))
}

// BtsToBsc returns 1 for BTS->BSC direction, 0 for BSC->BTS, -1 for a closed
// handle.
func (f *RslFrame) BtsToBsc() int {
	if f == nil || f.p == nil {
		return -1
	}
	return int(C.gsml3_rsl_bts_to_bsc(f.p))
}

// HasL3 reports whether the frame carries an L3 payload; false for a closed handle.
func (f *RslFrame) HasL3() bool {
	if f == nil || f.p == nil {
		return false
	}
	return int(C.gsml3_rsl_has_l3(f.p)) != 0
}

// L3 returns an independent COPY of the frame's L3 payload view (nil when the
// frame has no L3 or the handle is closed). The view itself is valid while
// this handle lives; the copy outlives it.
func (f *RslFrame) L3() []byte {
	if f == nil || f.p == nil {
		return nil
	}
	var ln C.size_t
	p := C.gsml3_rsl_l3(f.p, &ln)
	if p == nil || ln == 0 {
		return nil
	}
	out := make([]byte, int(ln))
	copy(out, unsafe.Slice((*byte)(unsafe.Pointer(p)), int(ln))) // one synchronous copy of the view
	return out
}

// IECount returns the number of parsed information elements; 0 for a closed handle.
func (f *RslFrame) IECount() int {
	if f == nil || f.p == nil {
		return 0
	}
	return int(C.gsml3_rsl_ie_count(f.p))
}

// IE returns the `index`-th information element: its type code and an
// independent copy of the value bytes. Out-of-range indexes fail with the C
// INVALID_ARG code passed through.
func (f *RslFrame) IE(index int) (int, []byte, error) {
	if err := f.check("rsl.IE"); err != nil {
		return 0, nil, err
	}
	var typ C.uint8_t
	var ln C.size_t
	var val *C.uint8_t
	if rc := int(C.gsml3_rsl_ie_get(f.p, C.size_t(index), &typ, &ln, &val)); rc != 0 {
		return 0, nil, lastError("rsl.IE", Code(rc))
	}
	out := make([]byte, int(ln))
	if val != nil && ln > 0 {
		copy(out, unsafe.Slice((*byte)(unsafe.Pointer(val)), int(ln))) // copy of the view into the handle's input copy
	}
	return int(typ), out, nil
}

// Closed reports whether the handle was already released.
func (f *RslFrame) Closed() bool { return f != nil && f.closed.Load() }

// Close releases the gsml3_rsl handle exactly once (idempotent).
func (f *RslFrame) Close() error {
	if !f.closed.CompareAndSwap(false, true) {
		return nil
	}
	C.gsml3_rsl_free(f.p) // NULL-safe
	f.p = nil
	return nil
}

func (f *RslFrame) check(op string) error {
	if f == nil || f.p == nil || f.closed.Load() {
		return &Error{Op: op, Code: CodeInvalidArg, Msg: "rsl frame is closed"}
	}
	return nil
}

// ── S4 RSL builders (13): fixed caller buffer, causes range-checked in C ───
// Every builder does ONE serializeInto pass into the fixed
// 512-byte caller buffer; a zero result surfaces the C code (including
// BUFFER_TOO_SMALL) and is never retried silently.

// RslBuildDataReq builds an L3 DATA request frame (chan_nr, link_id + L3 payload).
func RslBuildDataReq(chanNr byte, linkID byte, l3 []byte) ([]byte, error) {
	return serializeInto("rsl.BuildDataReq", func(out *C.uint8_t, maxLen C.size_t) C.size_t {
		return C.gsml3_rsl_build_data_req(out, maxLen, C.uint8_t(chanNr), C.uint8_t(linkID), bytePtr(l3), C.size_t(len(l3)))
	})
}

// RslBuildDataInd builds an L3 DATA indication frame (chan_nr, link_id + L3 payload).
func RslBuildDataInd(chanNr byte, linkID byte, l3 []byte) ([]byte, error) {
	return serializeInto("rsl.BuildDataInd", func(out *C.uint8_t, maxLen C.size_t) C.size_t {
		return C.gsml3_rsl_build_data_ind(out, maxLen, C.uint8_t(chanNr), C.uint8_t(linkID), bytePtr(l3), C.size_t(len(l3)))
	})
}

// RslBuildUnitDataReq builds a unit DATA request frame (single L3 payload, no segmentation).
func RslBuildUnitDataReq(chanNr byte, linkID byte, l3 []byte) ([]byte, error) {
	return serializeInto("rsl.BuildUnitDataReq", func(out *C.uint8_t, maxLen C.size_t) C.size_t {
		return C.gsml3_rsl_build_unit_data_req(out, maxLen, C.uint8_t(chanNr), C.uint8_t(linkID), bytePtr(l3), C.size_t(len(l3)))
	})
}

// RslBuildUnitDataInd builds a unit DATA indication frame.
func RslBuildUnitDataInd(chanNr byte, linkID byte, l3 []byte) ([]byte, error) {
	return serializeInto("rsl.BuildUnitDataInd", func(out *C.uint8_t, maxLen C.size_t) C.size_t {
		return C.gsml3_rsl_build_unit_data_ind(out, maxLen, C.uint8_t(chanNr), C.uint8_t(linkID), bytePtr(l3), C.size_t(len(l3)))
	})
}

// RslBuildChanActivAck builds a CHANNEL ACTIVATE acknowledgement (frame number = RACH reference).
func RslBuildChanActivAck(chanNr byte, frameNumber uint16) ([]byte, error) {
	return serializeInto("rsl.BuildChanActivAck", func(out *C.uint8_t, maxLen C.size_t) C.size_t {
		return C.gsml3_rsl_build_chan_activ_ack(out, maxLen, C.uint8_t(chanNr), C.uint16_t(frameNumber))
	})
}

// RslBuildChanActivNack builds a CHANNEL ACTIVATE negative acknowledgement; cause is a RSLErrorCause value (range-checked in C).
func RslBuildChanActivNack(chanNr byte, cause int) ([]byte, error) {
	return serializeInto("rsl.BuildChanActivNack", func(out *C.uint8_t, maxLen C.size_t) C.size_t {
		return C.gsml3_rsl_build_chan_activ_nack(out, maxLen, C.uint8_t(chanNr), C.int(cause))
	})
}

// RslBuildRFChanRelAck builds a RADIO PATH FAILURE / RF CHANNEL RELEASE acknowledgement.
func RslBuildRFChanRelAck(chanNr byte) ([]byte, error) {
	return serializeInto("rsl.BuildRFChanRelAck", func(out *C.uint8_t, maxLen C.size_t) C.size_t {
		return C.gsml3_rsl_build_rf_chan_rel_ack(out, maxLen, C.uint8_t(chanNr))
	})
}

// RslBuildConnFail builds a CONNECTION FAILURE indication; cause is a RSLErrorCause value (range-checked in C).
func RslBuildConnFail(chanNr byte, cause int) ([]byte, error) {
	return serializeInto("rsl.BuildConnFail", func(out *C.uint8_t, maxLen C.size_t) C.size_t {
		return C.gsml3_rsl_build_conn_fail(out, maxLen, C.uint8_t(chanNr), C.int(cause))
	})
}

// RslBuildMeasRes builds a MEASUREMENT RESULT report: RX level/quality (signed 8-bit) plus optional L1 measurements.
func RslBuildMeasRes(chanNr byte, measNr byte, rxLev, rxQual int8, l1 []byte) ([]byte, error) {
	return serializeInto("rsl.BuildMeasRes", func(out *C.uint8_t, maxLen C.size_t) C.size_t {
		return C.gsml3_rsl_build_meas_res(out, maxLen, C.uint8_t(chanNr), C.uint8_t(measNr), C.int8_t(rxLev), C.int8_t(rxQual), bytePtr(l1), C.size_t(len(l1)))
	})
}

// RslBuildHandoDet builds a HANDOVER DETECTED indication with the access delay.
func RslBuildHandoDet(chanNr byte, accessDelay byte) ([]byte, error) {
	return serializeInto("rsl.BuildHandoDet", func(out *C.uint8_t, maxLen C.size_t) C.size_t {
		return C.gsml3_rsl_build_hando_det(out, maxLen, C.uint8_t(chanNr), C.uint8_t(accessDelay))
	})
}

// RslBuildCCCHLoadInd builds a CCCH LOAD INDICATION with the four load counters.
func RslBuildCCCHLoadInd(chanNr byte, pagingLoad, rachTotal, rachBusy, rachAccess uint16) ([]byte, error) {
	return serializeInto("rsl.BuildCCCHLoadInd", func(out *C.uint8_t, maxLen C.size_t) C.size_t {
		return C.gsml3_rsl_build_ccch_load_ind(out, maxLen, C.uint8_t(chanNr), C.uint16_t(pagingLoad), C.uint16_t(rachTotal), C.uint16_t(rachBusy), C.uint16_t(rachAccess))
	})
}

// RslBuildChanRqd builds a CHANNEL REQUEST indication (request reference RA plus the T1p/T2/T3 timings and access delay; all fixed-width fields are range-checked in C).
func RslBuildChanRqd(chanNr byte, ra, t1p, t2, t3, accessDelay byte) ([]byte, error) {
	return serializeInto("rsl.BuildChanRqd", func(out *C.uint8_t, maxLen C.size_t) C.size_t {
		return C.gsml3_rsl_build_chan_rqd(out, maxLen, C.uint8_t(chanNr), C.uint8_t(ra), C.uint8_t(t1p), C.uint8_t(t2), C.uint8_t(t3), C.uint8_t(accessDelay))
	})
}

// RslBuildDeleteInd builds a LAPDm LINK DELETE indication with optional info bytes.
func RslBuildDeleteInd(chanNr byte, info []byte) ([]byte, error) {
	return serializeInto("rsl.BuildDeleteInd", func(out *C.uint8_t, maxLen C.size_t) C.size_t {
		return C.gsml3_rsl_build_delete_ind(out, maxLen, C.uint8_t(chanNr), bytePtr(info), C.size_t(len(info)))
	})
}
