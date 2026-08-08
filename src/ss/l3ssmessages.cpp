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
// LIABILITY, WHETHER IN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "gsml3parser/ss/l3ssmessages.h"
#include "gsml3parser/logger.h"
#include <sstream>
#include <iomanip>

namespace gsml3parser {

// ── TLV/LV helper functions ────────────────────────────────────────────

static void writeTLV(BitWriter& bw, unsigned iei, const uint8_t* data, size_t len) {
    bw.writeField(0x80 | (iei & 0x7F), 8);
    bw.writeField(static_cast<uint32_t>(len), 8);
    bw.writeBytes(data, len);
}

static bool try_parseTLV(BitReader& br, unsigned iei, std::vector<uint8_t>& out) {
    auto r = br.readField(8);
    if (!r) return false;
    uint8_t tag = static_cast<uint8_t>(r.value());
    if ((tag & 0x7F) != (iei & 0x7F)) return false;
    if (tag & 0x80) {
        auto lenR = br.readField(8);
        if (!lenR) return false;
        size_t len = lenR.value();
        out.resize(len);
        auto readR = br.readBytes(out.data(), len);
        if (!readR) return false;
    } else {
        out.clear();
    }
    return true;
}

static void writeLV(BitWriter& bw, const uint8_t* data, size_t len) {
    if (len > 0) {
        bw.writeField(static_cast<uint32_t>(len), 8);
        bw.writeBytes(data, len);
    }
}

static Expected<size_t> readLVLength(BitReader& br) {
    auto r = br.readField(8);
    if (!r) return Expected<size_t>::error(r.error());
    return Expected<size_t>::hold(r.value());
}

// ── L3SupServFacilityMessage ───────────────────────────────────────────

size_t L3SupServFacilityMessage::bodyLength() const {
    return mFacility.lengthV() + 1;
}

Expected<L3SupServFacilityMessage> L3SupServFacilityMessage::parse(BitReader& br) {
    L3SupServFacilityMessage msg;

    auto lenRes = readLVLength(br);
    if (!lenRes) return Expected<L3SupServFacilityMessage>::error(lenRes.error());
    size_t len = lenRes.value();

    auto facRes = L3OctetAlignedProtocolElement::parse(br, len);
    if (!facRes) return Expected<L3SupServFacilityMessage>::error(facRes.error());
    msg.mFacility = std::move(facRes).value();

    return Expected<L3SupServFacilityMessage>::hold(std::move(msg));
}

void L3SupServFacilityMessage::write(BitWriter& bw) const {
    bw.writeField(static_cast<uint32_t>(pd()), 4);
    bw.writeField(mTI, 3);
    bw.writeField(0, 1);
    bw.writeField(static_cast<uint32_t>(MTI) << 2, 8);
    writeLV(bw, mFacility.peData(), mFacility.lengthV());
}

void L3SupServFacilityMessage::text(std::ostream& os) const {
    os << "Facility TI=" << mTI << ": ";
    mFacility.text(os);
}

// ── L3SupServRegisterMessage ───────────────────────────────────────────

size_t L3SupServRegisterMessage::bodyLength() const {
    size_t len = 1;
    if (mFacility.mExtant) len += 1 + mFacility.lengthV();
    len += mHaveVersion ? 3 : 0;
    return len;
}

Expected<L3SupServRegisterMessage> L3SupServRegisterMessage::parse(BitReader& br) {
    L3SupServRegisterMessage msg;

    std::vector<uint8_t> facData;
    if (!try_parseTLV(br, 0x1c, facData)) {
        return Expected<L3SupServRegisterMessage>::error(
            ParseError{ParseError::Code::InvalidIE, "Missing facility IEI 0x1c"});
    }
    if (!facData.empty()) {
        msg.mFacility.mData.assign(facData.begin(), facData.end());
        msg.mFacility.mExtant = true;
    }

    msg.mHaveVersion = (br.peekField(8) == 0x7F);
    if (msg.mHaveVersion) {
        auto r = br.readField(8);
        if (!r) return Expected<L3SupServRegisterMessage>::error(r.error()); // IEI
        r = br.readField(8);
        if (!r) return Expected<L3SupServRegisterMessage>::error(r.error()); // length
        r = br.readField(8);
        if (!r) return Expected<L3SupServRegisterMessage>::error(r.error());
        msg.mVersionIndicator = static_cast<uint8_t>(r.value());
    }

    return Expected<L3SupServRegisterMessage>::hold(std::move(msg));
}

void L3SupServRegisterMessage::write(BitWriter& bw) const {
    bw.writeField(static_cast<uint32_t>(pd()), 4);
    bw.writeField(mTI, 3);
    bw.writeField(0, 1);
    bw.writeField(static_cast<uint32_t>(MTI) << 2, 8);

    if (mFacility.mExtant) {
        writeTLV(bw, 0x1c,
                 reinterpret_cast<const uint8_t*>(mFacility.mData.data()),
                 mFacility.lengthV());
    } else {
        bw.writeField(0x1c, 8);
    }

    if (mHaveVersion) {
        bw.writeField(0x7F, 8);
        bw.writeField(1, 8);
        bw.writeField(mVersionIndicator, 8);
    }
}

void L3SupServRegisterMessage::text(std::ostream& os) const {
    os << "Register TI=" << mTI << ": ";
    mFacility.text(os);
    if (mHaveVersion) os << " version=" << static_cast<int>(mVersionIndicator);
}

// ── L3SupServReleaseCompleteMessage ────────────────────────────────────

size_t L3SupServReleaseCompleteMessage::bodyLength() const {
    size_t len = 0;
    if (mFacility.mExtant) len += 2 + mFacility.lengthV();
    if (mHaveCause) len += 2 + L3CauseElement::lengthV();
    return len;
}

Expected<L3SupServReleaseCompleteMessage> L3SupServReleaseCompleteMessage::parse(BitReader& br) {
    L3SupServReleaseCompleteMessage msg;

    while (br.hasMore()) {
        auto r = br.readField(8);
        if (!r) return Expected<L3SupServReleaseCompleteMessage>::error(r.error());
        uint8_t tag = static_cast<uint8_t>(r.value());
        unsigned iei = tag & 0x7F;
        bool ext = (tag & 0x80) != 0;

        if (iei == 0x08) {
            if (ext) {
                r = br.readField(8);
                if (!r) return Expected<L3SupServReleaseCompleteMessage>::error(r.error());
            }
            auto causeRes = L3CauseElement::parse(br);
            if (!causeRes) return Expected<L3SupServReleaseCompleteMessage>::error(causeRes.error());
            msg.mCause = std::move(causeRes).value();
            msg.mHaveCause = true;
        } else if (iei == 0x1c) {
            size_t len = 0;
            if (ext) {
                r = br.readField(8);
                if (!r) return Expected<L3SupServReleaseCompleteMessage>::error(r.error());
                len = r.value();
            }
            if (len > 0) {
                auto facRes = L3OctetAlignedProtocolElement::parse(br, len);
                if (!facRes) return Expected<L3SupServReleaseCompleteMessage>::error(facRes.error());
                msg.mFacility = std::move(facRes).value();
            }
        } else {
            size_t skip = 0;
            if (ext) {
                r = br.readField(8);
                if (!r) return Expected<L3SupServReleaseCompleteMessage>::error(r.error());
                skip = r.value();
            }
            if (skip > 0) {
                std::vector<uint8_t> dummy(skip);
                auto skipRes = br.readBytes(dummy.data(), skip);
                if (!skipRes) return Expected<L3SupServReleaseCompleteMessage>::error(skipRes.error());
            }
        }
    }

    return Expected<L3SupServReleaseCompleteMessage>::hold(std::move(msg));
}

void L3SupServReleaseCompleteMessage::write(BitWriter& bw) const {
    bw.writeField(static_cast<uint32_t>(pd()), 4);
    bw.writeField(mTI, 3);
    bw.writeField(0, 1);
    bw.writeField(static_cast<uint32_t>(MTI) << 2, 8);

    if (mFacility.mExtant) {
        writeTLV(bw, 0x1c,
                 reinterpret_cast<const uint8_t*>(mFacility.mData.data()),
                 mFacility.lengthV());
    }
    if (mHaveCause) {
        bw.writeField(0x88, 8);
        bw.writeField(L3CauseElement::lengthV(), 8);
        mCause.write(bw);
    }
}

void L3SupServReleaseCompleteMessage::text(std::ostream& os) const {
    os << "SupServReleaseComplete TI=" << mTI;
    if (mFacility.mExtant) {
        os << " ";
        mFacility.text(os);
    }
    if (mHaveCause) os << ": " << CCCause2Str(mCause.cause());
}

} // namespace gsml3parser
