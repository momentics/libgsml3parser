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

#include <cstdint>
#include <memory>
#include <ostream>
#include <string>

#include "../l3message.h"
#include "../l3frame.h"
#include "../types.h"
#include "../enums.h"
#include "../cc/l3cclements.h"

namespace gsml3parser {

// ── L3SupServMessage ────────────────────────────────────────────────────

class L3SupServMessage : public L3Message {
protected:
    unsigned mTI;
public:
    enum MessageType : int {
        ReleaseComplete = 0x2a,
        Facility        = 0x3a,
        Register        = 0x3b
    };

    explicit L3SupServMessage(unsigned wTI = 7) : mTI(wTI) {}

    size_t fullBodyLength() const override { return l2BodyLength(); }
    ParseResult<void> write(L3Frame& dest) const override;
    L3PD pd() const override { return L3PD::NonCallSS; }
    unsigned ti() const override { return mTI; }
    void ti(unsigned wTI) { mTI = wTI; }
    void text(std::ostream& os) const override;
};

// ── Facility Message (GSM 04.80/24.080 2.3) ────────────────────────────

class L3SupServFacilityMessage : public L3SupServMessage {
private:
    L3OctetAlignedProtocolElement mFacility;
public:
    L3SupServFacilityMessage();
    L3SupServFacilityMessage(unsigned wTI, const std::string& facility);

    std::string getMapComponents() const { return mFacility.mData; }
    int mti() const override { return Facility; }
    ParseResult<void> try_writeBody(L3Frame& dest, size_t& wp) const override;
    ParseResult<void> try_parseBody(const L3Frame& src, size_t& rp) override;
    size_t l2BodyLength() const override;
    void text(std::ostream& os) const override;
};

// ── Register Message (GSM 04.80/24.080 2.4) ───────────────────────────

class L3SupServRegisterMessage : public L3SupServMessage {
private:
    L3OctetAlignedProtocolElement mFacility;
    bool mHaveVersion{};
    uint8_t mVersionIndicator;
public:
    L3SupServRegisterMessage();
    L3SupServRegisterMessage(unsigned wTI, const std::string& facility);

    bool haveVersionIndicator() const { return mHaveVersion; }
    uint8_t versionIndicator() const { return mVersionIndicator; }
    std::string getMapComponents() const { return mFacility.mData; }

    int mti() const override { return Register; }
    ParseResult<void> try_writeBody(L3Frame& dest, size_t& wp) const override;
    ParseResult<void> try_parseBody(const L3Frame& src, size_t& rp) override;
    size_t l2BodyLength() const override;
    void text(std::ostream& os) const override;
};

// ── Release Complete (GSM 04.80/24.080 2.5) ───────────────────────────

class L3SupServReleaseCompleteMessage : public L3SupServMessage {
private:
    L3OctetAlignedProtocolElement mFacility;
    bool mHaveCause;
    L3CauseElement mCause;
public:
    L3SupServReleaseCompleteMessage();
    explicit L3SupServReleaseCompleteMessage(unsigned wTI);
    L3SupServReleaseCompleteMessage(unsigned wTI, CCCause cause);

    bool haveFacility() const { return mFacility.mExtant; }
    CCCause cause() const { return mCause.cause(); }
    CCCauseLocation causeLocation() const { return mCause.location(); }
    int mti() const override { return ReleaseComplete; }
    ParseResult<void> try_writeBody(L3Frame& dest, size_t& wp) const override;
    ParseResult<void> try_parseBody(const L3Frame& src, size_t& rp) override;
    size_t l2BodyLength() const override;
    void text(std::ostream& os) const override;
};

} // namespace gsml3parser


