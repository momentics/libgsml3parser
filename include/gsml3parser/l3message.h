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

#ifndef GSML3PARSER_L3MESSAGE_H
#define GSML3PARSER_L3MESSAGE_H

#include <cstddef>
#include <functional>
#include <memory>
#include <ostream>
#include <string>

#include "bitvector.h"
#include "l3frame.h"
#include "scalar_types.h"

namespace gsml3parser {

// ── Exceptions ──────────────────────────────────────────────────────────

class ParseError : public std::runtime_error {
public:
    explicit ParseError(const std::string& what) : std::runtime_error(what) {}
};

class WriteError : public std::runtime_error {
public:
    explicit WriteError(const std::string& what) : std::runtime_error(what) {}
};

// ── L3Message ───────────────────────────────────────────────────────────

/**
 * Virtual base class for all GSM L3 signalling messages.
 * Part of libgsml3parser.
 */
class L3Message {
public:
    virtual ~L3Message() = default;

    /** Expected message body length in bytes, not including L3 header or rest octets. */
    virtual size_t l2BodyLength() const = 0;

    /** Body length including rest octets. */
    virtual size_t fullBodyLength() const = 0;

    /** Message length in bytes including L3 header, not including rest octets. */
    size_t l2Length() const { return l2BodyLength() + 2; }

    /** Length in bytes including header and rest octets. */
    size_t fullLength() const { return fullBodyLength() + 2; }

    /** Number of bits needed to hold message and header. */
    size_t bitsNeeded() const { return 8 * fullLength(); }

    /** Parse from an L3Frame (header already read). */
    virtual void parse(const L3Frame& source);

    /** Write message PD, MTI and data bits into a BitVector. */
    virtual void write(L3Frame& dest) const;

    /** Generate an L3Frame for this message. */
    std::unique_ptr<L3Frame> frame(Primitive prim = Primitive::L3_DATA) const;

    /** Return the L3 protocol discriminator. */
    virtual L3PD PD() const = 0;

    /** Return the message type indicator. */
    virtual int MTI() const = 0;

    /** Return the transaction identifier (only valid for CC, SMS, SS). */
    virtual unsigned TI() const { return 0; }

    /** Generate a human-readable representation. */
    virtual void text(std::ostream& os) const;
    std::string text() const;

protected:
    virtual void writeBody(L3Frame& dest, size_t& wp) const;
    virtual void parseBody(const L3Frame& source, size_t& rp);
};

/**
 * Callback type for parsing messages of a specific Protocol Discriminator
 * that the library does not handle by default (e.g. SMS, GPRS).
 */
using PDHandler = std::function<std::unique_ptr<L3Message>(const L3Frame&)>;

// ── Utility functions ───────────────────────────────────────────────────

/** Skip an unused LV element while parsing. Returns bits skipped. */
size_t skipLV(const L3Frame& source, size_t& rp);

/** Skip an unused TLV element while parsing. Returns bits skipped. */
size_t skipTLV(unsigned IEI, const L3Frame& source, size_t& rp);

/** Skip an unused TV element while parsing. Returns bits skipped. */
size_t skipTV(unsigned IEI, size_t numBits, const L3Frame& source, size_t& rp);

/** Check if an IE with given IEI is present at current position. */
bool parseHasT(unsigned IEI, const L3Frame& source, size_t& rp);

/** Convert PD + MTI to a human-readable string. */
std::string mti2string(L3PD pd, unsigned mti);

// ── L3ProtocolElement ───────────────────────────────────────────────────

/**
 * Abstract class for GSM L3 information elements.
 * See GSM 04.07 11.2.1.1.4 for TLV formatting.
 */
class L3ProtocolElement {
public:
    virtual ~L3ProtocolElement() = default;

    /** Length of the value part in bytes (0 for 1/2-octet fields). */
    virtual size_t lengthV() const = 0;
    size_t lengthTV()  const { return lengthV() + 1; }
    size_t lengthLV()  const { return lengthV() + 1; }
    size_t lengthTLV() const { return lengthLV() + 1; }

    /** Parse fixed-length value part. */
    virtual void parseV(const L3Frame& src, size_t& rp) = 0;

    /** Parse variable-length value part. */
    virtual void parseV(const L3Frame& src, size_t& rp, size_t expectedLength) = 0;

    /** Parse LV format. */
    void parseLV(const L3Frame& src, size_t& rp);

    /** Parse TV format. Returns true if IEI matched. */
    bool parseTV(unsigned IEI, const L3Frame& src, size_t& rp);

    /** Parse TLV format. Returns true if IEI matched. */
    bool parseTLV(unsigned IEI, const L3Frame& src, size_t& rp);

    /** Write the V format. */
    virtual void writeV(L3Frame& dest, size_t& wp) const = 0;

    /** Write LV format. */
    void writeLV(L3Frame& dest, size_t& wp) const;

    /** Write TV format. */
    void writeTV(unsigned IEI, L3Frame& dest, size_t& wp) const;

    /** Write TLV format. */
    void writeTLV(unsigned IEI, L3Frame& dest, size_t& wp) const;

    /** Human-readable form. */
    virtual void text(std::ostream& os) const { os << "(no text())"; }

protected:
    /** Skip extension octets. */
    void skipExtendedOctets(const L3Frame& src, size_t& rp);
};

/** Generic LV or TLV element (raw octets). */
class L3OctetAlignedProtocolElement : public L3ProtocolElement {
public:
    std::string mData;
    Bool_z mExtant;
    const unsigned char* peData() const { return reinterpret_cast<const unsigned char*>(mData.data()); }
    size_t lengthV() const override { return mData.size(); }
    void writeV(L3Frame& dest, size_t& wp) const override;
    void parseV(const L3Frame& src, size_t& rp, size_t expectedLength) override;
    void parseV(const L3Frame&, size_t&) override { throw ParseError("parseV not valid for TLV"); }
    void text(std::ostream&) const override;
    L3OctetAlignedProtocolElement() : mExtant(false) {}
    explicit L3OctetAlignedProtocolElement(std::string wData) : mData(std::move(wData)), mExtant(true) {}
};

/** Non-aligned message element (bit-level, not TLV). */
class GenericMessageElement {
public:
    virtual ~GenericMessageElement() = default;
    virtual size_t lengthBits() const = 0;
    virtual void writeBits(L3Frame& dest, size_t& wp) const = 0;
    virtual void text(std::ostream& os) const = 0;
};

// ── Stream operators ────────────────────────────────────────────────────

std::ostream& operator<<(std::ostream& os, const L3Message& msg);
std::ostream& operator<<(std::ostream& os, const L3Message* msg);
std::ostream& operator<<(std::ostream& os, const L3ProtocolElement& elem);
std::ostream& operator<<(std::ostream& os, const GenericMessageElement& elem);

} // namespace gsml3parser

#endif // GSML3PARSER_L3MESSAGE_H
