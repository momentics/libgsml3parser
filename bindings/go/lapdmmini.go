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

import "fmt"

// Minimal LAPDm (GSM 04.06) MS/peer-side frame builders for simulation and the
// demo — a byte-for-byte mirror of the Python mini-codec (_lapdm.py), planK
// decision #7. Purpose: simulation must BUILD Mobile-station-side frames to feed
// a BTS-side entity; the C core decodes peer frames and transmits only its own,
// so production transmission always goes through the C entity (SendUI/SendData/
// SendSABME/SendDISC) — do NOT use these builders on that path.
//
// Byte layout per src/lapdm_frame.cpp (U-frame control switch): address octet =
// (sapi << 4) | (command ? 0x08 : 0) | 0x01 (EA=1); U-control bytes by (type,
// pf): UI 0x03/0x07, SABME 0x2B/0x2F, UA 0x5F/0x63, DM 0x0B/0x0F, DISC
// 0x0C/0x08; S control = (nr << 5) | (pf ? 0x10 : 0) | type (RR 0x01 / REJ
// 0x0D); I control = (nr << 5) | (pf ? 0x10 : 0) | (ns << 1), preceded by a
// length octet (m << 7 | len & 0x3F) before the info. UI carries RAW info with
// NO length octet.
//
// Input validation mirrors Python's ValueError as a PANIC (programmer error in
// test/demo code, not a protocol error): sapi 0..15, NR/NS 0..7, info <= 63
// bytes. Every frame the test suite generates is cross-checked against the C
// decoder DecodeFrame (closed loop).

func miniPanic(what, format string, args ...any) {
	panic(fmt.Sprintf("lapdmmini: %s — "+format, append([]any{what}, args...)...))
}

// miniAddr builds the LAPDm address octet for a normal (EA=1) SAPI address.
func miniAddr(sapi byte, command bool) byte {
	if sapi > 0x0F { // NIB: the low bit of the nibble pair is part of EA
		miniPanic("sapi out of range 0..15", "%d", int(sapi))
	}
	a := sapi << 4
	if command {
		a |= 0x08 // C/R = 1
	}
	return a | 0x01 // EA = 1
}

func miniInfo(kind string, info []byte) {
	if len(info) > 63 { // LAPDm info field: at most 63 octets (7-bit length field, m=0)
		miniPanic(kind+" info exceeds the LAPDm 63-octet info field", "%d bytes", len(info))
	}
}

func miniSeq(kind string, v byte) {
	if v > 7 { // 3-bit sequence numbers (mod 8)
		miniPanic(kind+" out of range 0..7", "%d", int(v))
	}
}

// UIFrame builds a MS/peer-side UI frame: [address][0x03 (pf=0)] + raw info
// (no length octet). Used for the L3 unit-data injection in simulation.
func UIFrame(sapi byte, command bool, info []byte) []byte {
	miniInfo("UI", info)
	f := make([]byte, 0, 2+len(info))
	f = append(f, miniAddr(sapi, command), 0x03) // UI, pf=0
	return append(f, info...)
}

// UAFrame builds the MS-side unnumbered acknowledgement: exactly [0x01, 0x63]
// (sapi 0, response, pf=1) — the byte vector the link-lifecycle test expects.
func UAFrame() []byte {
	return []byte{miniAddr(0, false), 0x63} // UA, pf=1
}

// SABMEFrame builds a set-asynchronous-balance-mode command/response (pf=1).
func SABMEFrame(sapi byte, command bool) []byte {
	return []byte{miniAddr(sapi, command), 0x2F} // SABME, pf=1
}

// DMFrame builds a discouraged-mode response (pf=0), e.g. the peer's refusal of
// a SABME with info.
func DMFrame(sapi byte) []byte {
	return []byte{miniAddr(sapi, false), 0x0B} // DM, pf=0
}

// DISCFrame builds a disconnect command/response (pf=1).
func DISCFrame(sapi byte, command bool) []byte {
	return []byte{miniAddr(sapi, command), 0x08} // DISC, pf=1
}

// RRFrame builds an S-frame receive-ready (response, pf=0): [address][NR<<5|0x01].
func RRFrame(nr byte, sapi byte) []byte {
	miniSeq("RR nr", nr)
	return []byte{miniAddr(sapi, false), byte((int(nr) << 5) | 0x01)} // S/RR
}

// IFrame builds an I-frame: [address][(NR<<5)|(PF?0x10:0)|(NS<<1)]
// [(M<<7)|len] + info. The m bit marks message-complete segmentation.
func IFrame(sapi byte, command bool, nr, ns byte, pf, m bool, info []byte) []byte {
	miniSeq("I nr", nr)
	miniSeq("I ns", ns)
	miniInfo("I", info)
	ctrl := byte((int(nr) << 5) | ((int(ns) & 0x07) << 1))
	if pf {
		ctrl |= 0x10
	}
	lenOctet := byte(len(info)) & 0x3F
	if m {
		lenOctet |= 0x80
	}
	out := []byte{miniAddr(sapi, command), ctrl, lenOctet}
	return append(out, info...)
}
