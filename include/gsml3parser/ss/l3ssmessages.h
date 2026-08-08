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
#include <ostream>
#include <string>

#include "../expected.h"
#include "../bitreader.h"
#include "../bitwriter.h"
#include "../types.h"
#include "../enums.h"
#include "../common/l3common.h"
#include "../cc/l3ccelements.h"

namespace gsml3parser {

// ── Facility Message (GSM 04.80/24.080 2.3) ────────────────────────────

class L3SupServFacilityMessage {
private:
    unsigned mTI{7};
    L3OctetAlignedProtocolElement mFacility;
public:
    static constexpr int MTI = 0x3a;

    L3SupServFacilityMessage() = default;
    L3SupServFacilityMessage(unsigned wTI, const std::string& facility)
        : mTI(wTI), mFacility(facility) {}

    unsigned ti() const { return mTI; }
    void ti(unsigned wTI) { mTI = wTI; }
    static constexpr L3PD pd() { return L3PD::NonCallSS; }
    int mti() const { return MTI; }

    const std::string& getMapComponents() const { return mFacility.mData; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3SupServFacilityMessage> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── Register Message (GSM 04.80/24.080 2.4) ───────────────────────────

class L3SupServRegisterMessage {
private:
    unsigned mTI{7};
    L3OctetAlignedProtocolElement mFacility;
    bool mHaveVersion{};
    uint8_t mVersionIndicator{};
public:
    static constexpr int MTI = 0x3b;

    L3SupServRegisterMessage() = default;
    L3SupServRegisterMessage(unsigned wTI, const std::string& facility)
        : mTI(wTI), mFacility(facility) {}

    unsigned ti() const { return mTI; }
    void ti(unsigned wTI) { mTI = wTI; }
    static constexpr L3PD pd() { return L3PD::NonCallSS; }
    int mti() const { return MTI; }

    bool haveVersionIndicator() const { return mHaveVersion; }
    uint8_t versionIndicator() const { return mVersionIndicator; }
    const std::string& getMapComponents() const { return mFacility.mData; }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3SupServRegisterMessage> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── Release Complete (GSM 04.80/24.080 2.5) ───────────────────────────

class L3SupServReleaseCompleteMessage {
private:
    unsigned mTI{7};
    L3OctetAlignedProtocolElement mFacility;
    bool mHaveCause{};
    L3CauseElement mCause;
public:
    static constexpr int MTI = 0x2a;

    L3SupServReleaseCompleteMessage() = default;
    explicit L3SupServReleaseCompleteMessage(unsigned wTI) : mTI(wTI) {}
    L3SupServReleaseCompleteMessage(unsigned wTI, CCCause cause)
        : mTI(wTI), mHaveCause(true), mCause(cause, CCCauseLocation::Private_Serving_Local) {}

    unsigned ti() const { return mTI; }
    void ti(unsigned wTI) { mTI = wTI; }
    static constexpr L3PD pd() { return L3PD::NonCallSS; }
    int mti() const { return MTI; }

    bool haveFacility() const { return mFacility.mExtant; }
    CCCause cause() const { return mCause.cause(); }
    CCCauseLocation causeLocation() const { return mCause.location(); }

    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3SupServReleaseCompleteMessage> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

} // namespace gsml3parser
