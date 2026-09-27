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

// Minimal LAPDm (TS 44.064) MS/peer-side frame builders for simulation and
// the demo — a byte-for-byte mirror of the Python mini-codec (_lapdm.py).
// Purpose: simulation must BUILD Mobile-station-side frames to feed
// a BTS-side entity; the C core decodes peer frames and transmits only its own,
// so production transmission always goes through the C entity (SendUI/SendData/
// SendSABME/SendDISC) — do NOT use these builders on that path.
//
// Byte layout per src/lapdm_frame.cpp (format B): every frame is address +
// control + header octet (L/M/'1') [+ info]. Address octet =
// (sapi << 2) | (command ? 0x02 : 0) | 0x01 (EA=1), high three bits zero;
// U-control bytes by (type, pf): UI 0x03/0x13, SABME 0x2F/0x3F, DM 0x0F/0x1F,
// DISC 0x43/0x53, UA 0x63/0x73; S control = (nr << 5) | (pf ? 0x10 : 0) | type
// (RR 0x01 / RNR 0x05 / REJ 0x09); I control = (nr << 5) | (pf ? 0x10 : 0) |
// (ns << 1); header octet = (len << 2) | (m ? 1 : 0) | 1 with M=1 marking
// further segments of the same message.
//
// Input validation mirrors Python's ValueError as a PANIC (programmer error in
// test/demo code, not a protocol error): sapi 0..7 (only 0 and 3 are defined
// on Um), NR/NS 0..7, info <= 63 bytes. Every frame the test suite generates
// is cross-checked against the C decoder DecodeFrame (closed loop).

func miniPanic(what, format string, args ...any) {
	panic(fmt.Sprintf("lapdmmini: %s — "+format, append([]any{what}, args...)...))
}

// miniAddr builds the LAPDm address octet for a normal (EA=1) SAPI address.
func miniAddr(sapi byte, command bool) byte {
	if sapi > 0x07 { // SAPI is a three-bit field in the address octet
		miniPanic("sapi out of range 0..7", "%d", int(sapi))
	}
	a := sapi << 2
	if command {
		a |= 0x02 // C/R = 1
	}
	return a | 0x01 // EA = 1
}

func miniInfo(kind string, info []byte) {
	if len(info) > 63 { // LAPDm info field: at most 63 octets (six-bit length field)
		miniPanic(kind+" info exceeds the LAPDm 63-octet info field", "%d bytes", len(info))
	}
}

func miniSeq(kind string, v byte) {
	if v > 7 { // 3-bit sequence numbers (mod 8)
		miniPanic(kind+" out of range 0..7", "%d", int(v))
	}
}

// UIFrame builds a MS/peer-side UI frame: [address][0x03 (pf=0)][(len<<2)|1]
// + info. Used for the L3 unit-data injection in simulation.
func UIFrame(sapi byte, command bool, info []byte) []byte {
	miniInfo("UI", info)
	f := make([]byte, 0, 3+len(info))
	f = append(f, miniAddr(sapi, command), 0x03, byte(len(info)<<2)|0x01) // UI, pf=0
	return append(f, info...)
}

// UAFrame builds the MS-side unnumbered acknowledgement: exactly
// [0x01, 0x73, 0x01] (sapi 0, response, pf=1, L=0) — the byte vector the
// link-lifecycle test expects.
func UAFrame() []byte {
	return []byte{miniAddr(0, false), 0x73, 0x01} // UA, pf=1
}

// SABMEFrame builds a set-asynchronous-balance-mode command/response (pf=1).
func SABMEFrame(sapi byte, command bool) []byte {
	return []byte{miniAddr(sapi, command), 0x3F, 0x01} // SABME, pf=1
}

// DMFrame builds a disconnected-mode response (pf=0), e.g. the peer's refusal
// of a SABME with info.
func DMFrame(sapi byte) []byte {
	return []byte{miniAddr(sapi, false), 0x0F, 0x01} // DM, pf=0
}

// DISCFrame builds a disconnect command/response (pf=1).
func DISCFrame(sapi byte, command bool) []byte {
	return []byte{miniAddr(sapi, command), 0x53, 0x01} // DISC, pf=1
}

// RRFrame builds an S-frame receive-ready (response, pf=0):
// [address][NR<<5|0x01][0x01].
func RRFrame(nr byte, sapi byte) []byte {
	miniSeq("RR nr", nr)
	return []byte{miniAddr(sapi, false), byte((int(nr) << 5) | 0x01), 0x01} // S/RR
}

// IFrame builds an I-frame: [address][(NR<<5)|(PF?0x10:0)|(NS<<1)]
// [(len<<2)|(m?1:0)|1] + info. The m bit marks further segments of the same
// message (M=1) — M=0 on the final or only segment.
func IFrame(sapi byte, command bool, nr, ns byte, pf, m bool, info []byte) []byte {
	miniSeq("I nr", nr)
	miniSeq("I ns", ns)
	miniInfo("I", info)
	ctrl := byte((int(nr) << 5) | ((int(ns) & 0x07) << 1))
	if pf {
		ctrl |= 0x10
	}
	hdrOctet := byte(len(info)<<2) | 0x01
	if m {
		hdrOctet |= 0x02
	}
	out := []byte{miniAddr(sapi, command), ctrl, hdrOctet}
	return append(out, info...)
}
