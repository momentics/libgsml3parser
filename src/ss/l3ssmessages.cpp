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
#include <sstream>
#include <iomanip>

namespace gsml3parser {

// ── TLV/LV helper functions ────────────────────────────────────────────

static void writeTLV(BitWriter& bw, unsigned iei, const uint8_t* data, size_t len) {
    // The element identifier octet carries a zero spare bit followed by the
    // seven-bit identifier (TS 24.008); parsers accept either form.
    bw.writeField(iei & 0x7F, 8);
    bw.writeField(static_cast<uint32_t>(len), 8);
    bw.writeBytes(data, len);
}

static bool try_parseTLV(BitReader& br, unsigned iei, std::vector<uint8_t>& out) {
    auto r = br.readField(8);
    if (!r) return false;
    uint8_t tag = static_cast<uint8_t>(r.value());
    uint8_t iei7 = static_cast<uint8_t>(iei & 0x7F);
    if ((tag & 0x7F) != iei7) return false;
    // TLV: the value length octet follows the identifier regardless of the
    // spare bit (TS 24.008); both wire forms are accepted.
    auto lenR = br.readField(8);
    if (!lenR) return false;
    size_t len = lenR.value();
    out.resize(len);
    auto readR = br.readBytes(out.data(), len);
    if (!readR) return false;
    return true;
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

    if (!br.hasMore()) {
        return Expected<L3SupServFacilityMessage>::hold(std::move(msg));
    }

    auto lenRes = readLVLength(br);
    if (!lenRes) return Expected<L3SupServFacilityMessage>::error(lenRes.error());
    size_t len = lenRes.value();

    if (len > 0) {
        auto facRes = L3OctetAlignedProtocolElement::parse(br, len);
        if (!facRes) return Expected<L3SupServFacilityMessage>::error(facRes.error());
        msg.mFacility = std::move(facRes).value();
    }

    return Expected<L3SupServFacilityMessage>::hold(std::move(msg));
}

void L3SupServFacilityMessage::write(BitWriter& bw) const {
    bw.writeField(static_cast<uint32_t>(mFacility.lengthV()), 8);
    if (mFacility.lengthV() > 0) {
        bw.writeBytes(mFacility.peData(), mFacility.lengthV());
    }
}

void L3SupServFacilityMessage::text(std::ostream& os) const {
    os << "Facility TI=" << mTI << ": ";
    mFacility.text(os);
}

// ── L3SupServRegisterMessage ───────────────────────────────────────────

size_t L3SupServRegisterMessage::bodyLength() const {
    size_t len = 2 + mFacility.lengthV(); // facility TLV (mandatory, may be empty)
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
    // The facility IE is mandatory and may be empty (TS 24.080 section 2.4).
    writeTLV(bw, 0x1c,
              reinterpret_cast<const uint8_t*>(mFacility.mData.data()),
              mFacility.lengthV());

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
            // Cause: TLV with a two-octet value (TS 24.078 / TS 24.080
            // release complete).
            r = br.readField(8);
            if (!r) return Expected<L3SupServReleaseCompleteMessage>::error(r.error());
            auto causeRes = L3CauseElement::parse(br);
            if (!causeRes) return Expected<L3SupServReleaseCompleteMessage>::error(causeRes.error());
            msg.mCause = std::move(causeRes).value();
            msg.mHaveCause = true;
        } else if (iei == 0x1c) {
            // Facility: TLV with a variable-length value (TS 24.078 /
            // TS 24.080 release complete).
            r = br.readField(8);
            if (!r) return Expected<L3SupServReleaseCompleteMessage>::error(r.error());
            size_t len = r.value();
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
    if (mFacility.mExtant) {
        writeTLV(bw, 0x1c,
                  reinterpret_cast<const uint8_t*>(mFacility.mData.data()),
                  mFacility.lengthV());
    }
    if (mHaveCause) {
        // Cause: TLV, element identifier 0x08 (TS 24.078 / TS 24.080
        // release complete).
        bw.writeField(0x08, 8);
        bw.writeField(static_cast<uint32_t>(L3CauseElement::lengthV()), 8);
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
