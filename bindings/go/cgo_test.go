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

// Core binding tests (planK step 2.8): parse/round-trip over the C-verified PD
// batch, NULL-safety on both sides of the wrapper boundary, error paths, and
// the BUFFER_TOO_SMALL class — stdlib testing only, no external dependencies.

import (
	"bytes"
	"os"
	"path/filepath"
	"strings"
	"testing"
	"unsafe"
)

const (
	// repoRoot is two levels up from bindings/go — the module root is an internal
	// binding directory (planK: single Go module at bindings/go).
	repoRoot = "../.."

	demoTMSI = 0x87654321
)

func requireErrCode(t *testing.T, op string, err error, want Code) {
	t.Helper()
	if err == nil {
		t.Fatalf("%s: expected error %s, got nil", op, want)
	}
	e, ok := err.(*Error)
	if !ok {
		t.Fatalf("%s: expected *gsml3parser.Error, got %T (%v)", op, err, err)
	}
	if e.Code != want {
		t.Fatalf("%s: error code = %s, want %s (msg %q)", op, e.Code, want, e.Msg)
	}
}

// ── Version / ABI ───────────────────────────────────────────────────────────

func TestVersionAndABI(t *testing.T) {
	if v := Version(); v == "" {
		t.Fatal("Version() returned an empty string")
	}
	// Single source of truth (planK decision #13, Phase 0): the library reports
	// exactly what CMake read from the repo-root VERSION file.
	raw, err := os.ReadFile(filepath.Join(repoRoot, "VERSION"))
	if err != nil {
		t.Fatalf("cannot read root VERSION file: %v", err)
	}
	want := strings.TrimSpace(string(raw))
	if got := Version(); got != want {
		t.Fatalf("Version() = %q, want root VERSION content %q", got, want)
	}
	if got := ABIVersion(); got != expectedABI {
		t.Fatalf("ABIVersion() = %d, want %d (== GSML3_ABI_VERSION)", got, expectedABI)
	}
}

// ── Parse round trip: the C-verified PD batch (decision #12 — stable vectors) ─
// The table is a VERBATIM port of tests/test_c_api.cpp `kBatch` (anchor:
// "const PDBatch kBatch[]"): each vector was C-side verified and is NOT
// simplified here.

type pdBatch struct {
	pd  int
	hex string
	mti int
}

var kBatch = []pdBatch{
	{PDRR, "60 0D 00", 0x0d},          // Channel Release
	{PDMM, "50 84", 0x21},             // CM Service Accept
	{PDCC, "3E 94 08 02 16 21", 0x25}, // Disconnect (TI=7)
	{PDSS, "B0 E8 00", 0x3a},          // SupServFacilityMessage (empty facility)
	{PDGmm, "80 20 05", 0x20},         // GMM Status (cause=5)
	{PDSm, "A0 55 A7 01 05", 0x55},    // SM Status (cause=5)
	{PDSms, "90 04", 0x04},            // CP-Ack (no body)
	{PDBcc, "10 00", 0x00},            // BCC Setup
	{PDGcc, "00 00 02", 0x00},         // GCC Setup
	{PDLS, "C0 01", 0x01},             // LocationServiceRequest
	{PDExt, "E0 01", 0x01},            // ExtendedMessage
	{PdTst, "F0 01", 0x01},            // TestProcedureMessage
}

func TestParseRoundTrip(t *testing.T) {
	for i, v := range kBatch {
		t.Run(v.hex, func(t *testing.T) {
			m, err := ParseHex(v.hex, nil)
			if err != nil {
				t.Fatalf("vector %d: ParseHex(%q): %v", i, v.hex, err)
			}
			defer m.Close()

			if got := m.Pd(); got != v.pd {
				t.Errorf("vector %d: Pd() = %d, want %d", i, got, v.pd)
			}
			if got := m.Mti(); got != v.mti {
				t.Errorf("vector %d: Mti() = %d, want %#x", i, got, v.mti)
			}
			if got := m.Name(); got == "" {
				t.Errorf("vector %d: Name() is empty", i)
			}

			// Exact-size serialize (decision #8) then byte-level identity on reparse.
			wire, err := m.Write()
			if err != nil {
				t.Fatalf("vector %d: Write(): %v", i, err)
			}
			if sz, _ := m.Size(); len(wire) != sz {
				t.Fatalf("vector %d: Write produced %d bytes, Size said %d", i, len(wire), sz)
			}
			m2, err := Parse(wire, nil)
			if err != nil {
				t.Fatalf("vector %d: reparse: %v", i, err)
			}
			defer m2.Close()
			if m2.Name() != m.Name() || m2.Pd() != m.Pd() || m2.Mti() != m.Mti() {
				t.Errorf("vector %d: reparse mismatch (%s/%d/%d) vs (%s/%d/%d)", i,
					m2.Name(), m2.Pd(), m2.Mti(), m.Name(), m.Pd(), m.Mti())
			}

			// Hex form: lowercase, no spaces.
			hexStr, err := m.Hex()
			if err != nil {
				t.Fatalf("vector %d: Hex(): %v", i, err)
			}
			if hexStr != strings.ToLower(strings.ReplaceAll(v.hex, " ", "")) {
				t.Errorf("vector %d: Hex() = %q, want %q", i, hexStr, strings.ToLower(strings.ReplaceAll(v.hex, " ", "")))
			}

			// ParseInto (reparse hot path) keeps working on the same handle.
			if err := m2.ParseInto(wire, nil); err != nil {
				t.Errorf("vector %d: ParseInto: %v", i, err)
			}
			if m2.Name() != m.Name() {
				t.Errorf("vector %d: name changed after ParseInto", i)
			}
		})
	}

	// Channel Release specifics (the demo's stable vector): size 3, exact bytes.
	m, err := ParseHex("60 0D 00", nil)
	if err != nil {
		t.Fatalf("channel release: %v", err)
	}
	defer m.Close()
	if sz, _ := m.Size(); sz != 3 {
		t.Errorf("channel release: Size() = %d, want 3", sz)
	}
	wire, err := m.Write()
	if err != nil {
		t.Fatalf("channel release Write: %v", err)
	}
	if !bytes.Equal(wire, []byte{0x60, 0x0D, 0x00}) {
		t.Errorf("channel release wire = % X, want 60 0D 00", wire)
	}
	if hexStr, _ := m.Hex(); hexStr != "600d00" {
		t.Errorf("channel release Hex() = %q, want %q", hexStr, "600d00")
	}
	if ti := m.Ti(); ti != 0 {
		t.Errorf("channel release Ti() = %d, want 0 (not CC/SS)", ti)
	}
}

// ── NULL-safety: C-documented contract on the RAW side + wrapper rejections ──

func TestNullSafety(t *testing.T) {
	var nilPtr unsafe.Pointer // explicit NULL for the raw probes

	// C-documented sentinels through raw calls (mirror of TEST(CApi, NullSafety)):
	if got := rawMessageName(nilPtr); got != "" {
		t.Errorf("message_name(NULL) = %q, want \"\"", got)
	}
	if got := rawMessagePd(nilPtr); got != -1 {
		t.Errorf("message_pd(NULL) = %d, want -1", got)
	}
	if got := rawMessageMti(nilPtr); got != -1 {
		t.Errorf("message_mti(NULL) = %d, want -1", got)
	}
	if got := rawMessageTi(nilPtr); got != 0 {
		t.Errorf("message_ti(NULL) = %d, want 0", got)
	}
	if got := rawMessageSize(nilPtr); got != 0 {
		t.Errorf("message_size(NULL) = %d, want 0", got)
	}
	n, code := rawMessageWrite(nilPtr, 4)
	if n != 0 || code == CodeOK {
		t.Errorf("message_write(NULL) = (%d, %s), want (0, non-OK)", n, code)
	}
	if !rawMessageHexNull() {
		t.Error("message_hex(NULL) must be NULL")
	}
	if code := rawParseL3IntoNull(); code == CodeOK {
		t.Error("parse_l3_into(NULL, ...) must fail (non-OK)")
	}

	// Every documented *_free is a NULL no-op (any crash here = core violation):
	rawAllFreesNull()

	// Entity stats are NULL-safe: UNUSED state / 0 counters.
	state, established, sent, received, retrans := rawEntityNullStats()
	if state != StateLapdmUnused || established != 0 || sent != 0 || received != 0 || retrans != 0 {
		t.Errorf("entity NULL stats = (state %d, est %d, sent %d, recv %d, retr %d), want UNUSED/0s",
			state, established, sent, received, retrans)
	}

	// Wrapper side: empty/nil input rejected BEFORE FFI (typed error, no panic):
	if _, err := Parse(nil, nil); err == nil {
		t.Fatal("Parse(nil) must fail before FFI")
	} else if e, ok := err.(*Error); !ok || e.Code != CodeInvalidArg {
		t.Fatalf("Parse(nil): got %v, want *Error INVALID_ARG", err)
	}
	if _, err := Parse([]byte{}, nil); err == nil {
		t.Fatal(`Parse("") must fail before FFI`)
	} else if e, ok := err.(*Error); !ok || e.Code != CodeInvalidArg {
		t.Fatalf(`Parse(""): got %v, want *Error INVALID_ARG`, err)
	}
	if _, err := ParseHex("", nil); err == nil {
		t.Fatal(`ParseHex("") must fail before FFI`)
	} else if e, ok := err.(*Error); !ok || e.Code != CodeInvalidArg {
		t.Fatalf(`ParseHex(""): got %v, want *Error INVALID_ARG`, err)
	}
	if _, err := DecodeFrame([]byte{0x60}); err == nil {
		t.Fatal("DecodeFrame(single byte) must fail before FFI")
	} else if e, ok := err.(*Error); !ok || e.Code != CodeInvalidArg {
		t.Fatalf("DecodeFrame(short): got %v, want *Error INVALID_ARG", err)
	}
	if _, err := RslParse(nil); err == nil {
		t.Fatal("RslParse(empty) must fail before FFI")
	} else if e, ok := err.(*Error); !ok || e.Code != CodeInvalidArg {
		t.Fatalf("RslParse(empty): got %v, want *Error INVALID_ARG", err)
	}

	// Wrapper sentinels for closed handles (no FFI after close):
	m, err := ParseHex("60 0D 00", nil)
	if err != nil {
		t.Fatalf("parse for close probe: %v", err)
	}
	_ = m.Close()
	_ = m.Close() // double close must be a silent no-op
	if m.Name() != "" || m.Pd() != -1 || m.Mti() != -1 || m.Ti() != 0 {
		t.Errorf("closed message sentinels wrong: name=%q pd=%d mti=%d ti=%d", m.Name(), m.Pd(), m.Mti(), m.Ti())
	}
	if _, err := m.Write(); err == nil {
		t.Error("Write() on a closed handle must fail without FFI")
	}
	if _, err := m.Hex(); err == nil {
		t.Error("Hex() on a closed handle must fail without FFI")
	}

	// A nil *Message receiver reports the same sentinels (documented).
	var nm *Message
	if nm.Name() != "" || nm.Pd() != -1 || nm.Ti() != 0 {
		t.Error("nil *Message sentinels wrong")
	}
}

// ── Error paths: C codes passed through with synchronous messages ────────────

func TestErrorPaths(t *testing.T) {
	// Truncated input (C rule, message copied synchronously). Vector mirrors
	// TEST(CApi, L3ErrorPaths): '60 0D' is the canonical truncated RR ChannelRelease.
	// (A lone octet '60' IS a complete RR short-form message — do not use it as a
	// truncation probe.)
	if _, err := ParseHex("60 0D", nil); err == nil {
		t.Fatal(`ParseHex("60 0D") must fail`)
	} else {
		requireErrCode(t, `ParseHex("60 0D")`, err, CodeTruncated)
		e := err.(*Error)
		if e.Msg == "" {
			t.Errorf(`ParseHex("60 0D"): empty message — thread-local copy failed: %v`, err)
		}
	}

	// The complete 1-octet RR message (same leading octet, MTI ChannelRequest):
	// parsing it must SUCCEED — guards against over-strict truncation.
	if m, err := ParseHex("60", nil); err != nil {
		t.Fatalf(`ParseHex("60") is a complete RR short form and must parse: %v`, err)
	} else {
		defer m.Close()
		sz, serr := m.Size()
		if m.Name() != "ChannelRequest" || m.Pd() != PDRR || sz != 1 {
			t.Fatalf(`ParseHex("60") = name %q pd %d size (%d, %v), want ChannelRequest/%#x/1`, m.Name(), m.Pd(), sz, serr, PDRR)
		}
	}

	// Non-hex content: the C core reports GSML3_ERR_INVALID_VALUE (7) for this —
	// passed through with its synchronous message (C vector, mirrors Phase 1).
	if _, err := ParseHex("zz", nil); err == nil {
		t.Fatal(`ParseHex("zz") must fail`)
	} else if e, ok := err.(*Error); !ok || e.Code != CodeInvalidValue || e.Msg == "" {
		t.Fatalf(`ParseHex("zz"): got %v, want *Error INVALID_VALUE with message`, err)
	}

	// NOTE on garbage-input probing: this core's LENIENT parser accepts most
	// short byte strings as a complete minimal message (verified against C: a
	// lone 0xFF parses as TST TestProcedureMessage), so the binding does not —
	// and cannot — manufacture an "invalid bytes" failure that the C core would
	// report either. The truncation vectors above are the C-verified ones.

	// Strict framing config: "50 84" parses; a trailing byte does NOT (mirror of
	// the C Config test). Lenient (nil config) accepts both.
	cfg, err := NewConfig()
	if err != nil {
		t.Fatalf("NewConfig: %v", err)
	}
	defer cfg.Close()
	if m, err := Parse([]byte{0x50, 0x84}, cfg); err != nil || m == nil {
		t.Errorf("lenient parse of 50 84 failed: %v", err)
	}
	if err := cfg.SetStrictFraming(true); err != nil {
		t.Fatalf("SetStrictFraming: %v", err)
	}
	m, err := ParseHex("50 84", cfg)
	if err != nil {
		t.Errorf("strict parse of exactly-consumed 50 84 must succeed: %v", err)
	} else {
		m.Close()
	}
	if _, err = Parse([]byte{0x50, 0x84, 0x00}, cfg); err == nil {
		t.Error("strict framing must reject the trailing byte")
	}

	// Config setters on a closed config fail without FFI:
	_ = cfg.Close()
	if err := cfg.SetLogLevel(0); err == nil {
		t.Error("SetLogLevel after close must fail")
	}

	// Out-of-domain builder parameters pass through the C INVALID_ARG:
	if _, err := RslBuildChanActivNack(1, -5); err == nil {
		t.Error("RslBuildChanActivNack(cause=-5) must fail (out-of-domain cause)")
	} else if e, ok := err.(*Error); !ok || e.Code != CodeInvalidArg {
		t.Fatalf("expected INVALID_ARG pass-through, got %v", err)
	}
	// Fixed-width frame fields (TN/TSC 0..7, ARFCN 0..1023) are range-checked in C:
	if _, err := BuildResponseImmediateAssignment(0, 9, 0, 100, 0); err == nil { // tn = 9 > 7
		t.Error("BuildResponseImmediateAssignment(tn=9) must fail (fixed-width field)")
	} else if e, ok := err.(*Error); !ok || e.Code != CodeInvalidArg {
		t.Fatalf("expected INVALID_ARG for tn>7, got %v", err)
	}

	// RSL parse of garbage fails with a typed error:
	if _, err := RslParse([]byte{0x42}); err == nil {
		t.Error("RslParse(single junk byte) must fail")
	}
}

// ── BUFFER_TOO_SMALL class (mirror of C Builder_BufferTooSmall) ──────────────

func TestBufferTooSmallClass(t *testing.T) {
	// Message write into a deliberately undersized buffer: 0 written + code 11.
	h := rawParseL3Hex("60 0D 00") // Channel Release = 3 bytes; 2-byte buffer is too small
	if h == nil {
		t.Fatal("rawParseL3Hex(60 0D 00) failed in the test harness itself")
	}
	defer rawFreeMessage(h)
	n, code := rawMessageWrite(h, 2)
	if n != 0 {
		t.Errorf("message_write into 2-byte buffer wrote %d bytes, want 0", n)
	}
	if code != CodeBufferTooSmall {
		t.Errorf("message write too-small code = %s, want %s", code, CodeBufferTooSmall)
	}

	// RSL builder into an undersized buffer: 0 + BUFFER_TOO_SMALL reported, never
	// silently retried (decision #8).
	n, code = rawRslBuildTooSmall()
	if n != 0 {
		t.Errorf("rsl_build_data_req into 4-byte buffer wrote %d bytes, want 0", n)
	}
	if code != CodeBufferTooSmall {
		t.Errorf("rsl builder too-small code = %s, want %s", code, CodeBufferTooSmall)
	}
}

// ── Mini-codec closed loop: every generated frame cross-checked vs C decoder ──

func TestMiniCodecCrossChecksCDecoder(t *testing.T) {
	// UI: MS-side L3 injection — [0x01, 0x03] + raw info (NO length octet).
	l3 := []byte{0x60, 0x0D, 0x00}
	f := UIFrame(0, false, l3)
	if !bytes.Equal(f, []byte{0x01, 0x03, 0x60, 0x0D, 0x00}) {
		t.Fatalf("UIFrame bytes = % X, want 01 03 60 0d 00", f)
	}
	dec, err := DecodeFrame(f)
	if err != nil {
		t.Fatalf("DecodeFrame(UI): %v", err)
	}
	if dec.Format != LapdmFmtU || dec.UType != LapdmUUI || dec.SAPI != 0 || dec.Command != 0 {
		t.Errorf("UI decode = fmt %d u_type %#x sapi %d cmd %d, want U/0x03/0/0",
			dec.Format, dec.UType, dec.SAPI, dec.Command)
	}
	if !bytes.Equal(dec.Payload, l3) {
		t.Errorf("UI payload = % X, want % X (zero-copy view of the frame)", dec.Payload, l3)
	}

	// Zero-copy PROOF: the decoded payload must alias the input frame memory.
	if len(f) > 2 && len(dec.Payload) > 0 {
		base := uintptr(unsafe.Pointer(&f[0]))
		p := uintptr(unsafe.Pointer(&dec.Payload[0]))
		if p < base || p >= base+uintptr(len(f)) {
			t.Errorf("payload does not alias the frame buffer (copy made?)")
		}
	}

	// UA: exactly [0x01, 0x63].
	ua := UAFrame()
	if !bytes.Equal(ua, []byte{0x01, 0x63}) {
		t.Fatalf("UAFrame = % X, want 01 63", ua)
	}
	dec, err = DecodeFrame(ua)
	if err != nil || dec.UType != LapdmUUA {
		t.Errorf("UA decode = %v / %#x, want U type 0x63 (err %v)", dec.UType, dec.UType, err)
	}

	// SABME command from BTS side: exactly [0x09, 0x2F] (the lifecycle test vector).
	if got := SABMEFrame(0, true); !bytes.Equal(got, []byte{0x09, 0x2F}) {
		t.Fatalf("SABMEFrame(0,true) = % X, want 09 2f", got)
	}
	dec, err = DecodeFrame(SABMEFrame(0, true))
	if err != nil || dec.UType != LapdmUSabme || dec.Command != 1 {
		t.Errorf("SABME decode: %v / %#x cmd %d (err %v)", dec.UType, dec.UType, dec.Command, err)
	}

	// DISC command: [0x09, 0x08].
	if got := DISCFrame(0, true); !bytes.Equal(got, []byte{0x09, 0x08}) {
		t.Fatalf("DISCFrame(0,true) = % X, want 09 08", got)
	}
	dec, err = DecodeFrame(DISCFrame(0, true))
	if err != nil || dec.UType != LapdmUDisc {
		t.Errorf("DISC decode: %#x (err %v)", dec.UType, err)
	}

	// DM response: [0x01, 0x0B].
	if got := DMFrame(0); !bytes.Equal(got, []byte{0x01, 0x0B}) {
		t.Fatalf("DMFrame = % X, want 01 0b", got)
	}
	dec, err = DecodeFrame(DMFrame(0))
	if err != nil || dec.UType != LapdmUDM || dec.Command != 0 {
		t.Errorf("DM decode: %#x cmd %d (err %v)", dec.UType, dec.Command, err)
	}

	// RR S-frame with nr=2: [0x01, 0xA1] — S format, s_type RR.
	if got := RRFrame(2, 0); !bytes.Equal(got, []byte{0x01, byte((2 << 5) | 0x01)}) {
		t.Fatalf("RRFrame = % X", got)
	}
	dec, err = DecodeFrame(RRFrame(2, 0))
	if err != nil || dec.Format != LapdmFmtS || dec.SType != LapdmSRR || dec.NR != 2 {
		t.Errorf("RR decode: fmt %d s_type %#x nr %d (err %v)", dec.Format, dec.SType, dec.NR, err)
	}

	// I-frame with nr=3 ns=5 pf=1 m=0 + info — layout: addr | ctrl | len | info.
	info := []byte{0x3E, 0x94}
	ifFrame := IFrame(3, true, 3, 5, true, false, info) // sapi 3, command (BTS)
	wantI := []byte{miniAddr(3, true), (3 << 5) | 0x10 | (5 << 1), byte(len(info)), info[0], info[1]}
	if !bytes.Equal(ifFrame, wantI) {
		t.Fatalf("IFrame = % X, want % X", ifFrame, wantI)
	}
	dec, err = DecodeFrame(ifFrame)
	if err != nil || dec.Format != LapdmFmtI || dec.NR != 3 || dec.NS != 5 || dec.PF != 1 || dec.MBit != 0 {
		t.Errorf("I decode: fmt %d nr %d ns %d pf %d m %d (err %v)",
			dec.Format, dec.NR, dec.NS, dec.PF, dec.MBit, err)
	}
	if !bytes.Equal(dec.Payload, info) {
		t.Errorf("I payload = % X, want % X", dec.Payload, info)
	}

	// Out-of-domain mini-codec input panics (programmer-error contract).
	assertPanics := func(name string, fn func()) {
		defer func() {
			if recover() == nil {
				t.Errorf("%s: expected panic on out-of-domain input", name)
			}
		}()
		fn()
	}
	assertPanics("UIFrame(sapi=16)", func() { UIFrame(16, false, nil) })
	assertPanics("RRFrame(nr=8)", func() { RRFrame(8, 0) })
	assertPanics("UIFrame(info>63)", func() { UIFrame(0, false, bytes.Repeat([]byte{1}, 64)) })
}

// ── RSL wrapper smoke (build -> parse -> observe round trip) ─────────────────

func TestRslRoundTrip(t *testing.T) {
	l3 := []byte{0x50, 0x84}
	frame, err := RslBuildDataReq(0x10, 1, l3)
	if err != nil {
		t.Fatalf("RslBuildDataReq: %v", err)
	}
	rsl, err := RslParse(frame)
	if err != nil {
		t.Fatalf("RslParse(% X): %v", frame, err)
	}
	defer rsl.Close()
	if got := rsl.Name(); !strings.HasPrefix(got, "DATA_REQ") {
		t.Errorf("rsl.Name() = %q, want a DATA_REQ name", got)
	}
	if got := rsl.ChanNr(); got != 0x10 {
		t.Errorf("ChanNr = %#x, want 0x10", got)
	}
	if !rsl.HasL3() {
		t.Fatal("HasL3() = false, want true")
	}
	got := rsl.L3()
	if !bytes.Equal(got, l3) {
		t.Errorf("L3 copy = % X, want % X", got, l3)
	}
	c := rsl.IECount()
	if c == 0 {
		t.Error("IECount() = 0 on a built frame with IEs")
	}
	typ, val, err := rsl.IE(0)
	if err != nil {
		t.Fatalf("IE(0): %v (count %d)", err, c)
	}
	if typ == 0 {
		t.Errorf("IE(0).type = 0, want a real IE type code")
	}
	if len(val) == 0 {
		t.Error("IE(0) value is empty")
	}
}
