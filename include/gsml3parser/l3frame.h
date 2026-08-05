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

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "bitvector.h"
#include "types.h"

namespace gsml3parser {

/**
 * L3Frame — a GSM L3 message in a bit vector, with metadata.
 *
 * Bit ordering is MSB-first within each octet.
 * Part of libgsml3parser.
 */
class L3Frame : public BitVector {
private:
    Primitive  mPrimitive;
    SAPI       mSapi;
    size_t     mL2Length;  // length or L2 pseudo-length in bytes
    double     mTimestamp; // creation timestamp (seconds since epoch)

    void init();

public:
    L3Frame();
    explicit L3Frame(Primitive prim);
    L3Frame(SAPI sapi, Primitive prim);
    L3Frame(Primitive prim, size_t nbits, SAPI sapi = SAPI::SAPI0);
    L3Frame(SAPI sapi, const BitVector& source, Primitive prim = Primitive::L3_DATA);
    L3Frame(SAPI sapi, const char* hexString);

    L3Frame(const L3Frame& other);
    L3Frame& operator=(const L3Frame& other);

    // ── Protocol fields ──────────────────────────────────────────────

    /** Protocol Discriminator — GSM 04.08 10.2 */
    L3PD pd() const;

    /** Message Type Indicator — GSM 04.08 10.4 */
    unsigned mti() const;

    /** Transaction Identifier — GSM 04.07 11.2.3.1.3 */
    unsigned ti() const;

    /** TIF flag — bit 7 of byte 0 (1 = short message) */
    unsigned tif() const;

    // ── Accessors ────────────────────────────────────────────────────

    Primitive primitive() const { return mPrimitive; }
    bool isData() const;

    /** Frame length in bytes */
    size_t length() const { return size() / 8; }

    /** L2 length / pseudo-length */
    size_t l2Length() const { return mL2Length; }
    void l2Length(size_t len) { mL2Length = len; }

    SAPI sapi() const { return mSapi; }
    void sapi(SAPI s) { mSapi = s; }

    double timestamp() const { return mTimestamp; }
    void setTimestamp(double ts) { mTimestamp = ts; }

    // ── H/L bit writing (for rest octets) ────────────────────────────

    void writeH(size_t& wp);
    void writeL(size_t& wp);

    void text(std::ostream& os) const;
};

std::ostream& operator<<(std::ostream& os, const L3Frame& frame);

} // namespace gsml3parser


