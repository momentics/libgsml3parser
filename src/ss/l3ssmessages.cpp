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

#include "gsml3parser/ss/l3ssmessages.h"
#include "gsml3parser/cc/l3cclements.h"
#include "gsml3parser/logger.h"
#include <sstream>
#include <iomanip>

namespace gsml3parser {

// ── L3SupServMessage ────────────────────────────────────────────────────

ParseResult<void> L3SupServMessage::write(L3Frame& dest) const {
    size_t l3len = bitsNeeded();
    if (dest.size() != l3len) dest.resize(l3len);
    size_t wp = 0;
    dest.writeField(wp, static_cast<unsigned>(pd()), 4);
    dest.writeField(wp, mTI, 3);
    dest.writeField(wp, 0, 1);
    dest.writeField(wp, mti() << 2, 8);
    auto res = try_writeBody(dest, wp);
    if (!res.has_value()) return res;
    dest.l2Length(l2Length());
    return ParseResult<void>();
}

void L3SupServMessage::text(std::ostream& os) const {
    L3Message::text(os);
    os << " TI=" << mTI;
}

// ── L3SupServFacilityMessage ───────────────────────────────────────────

L3SupServFacilityMessage::L3SupServFacilityMessage() {}

L3SupServFacilityMessage::L3SupServFacilityMessage(unsigned wTI, const std::string& facility)
    : L3SupServMessage(wTI), mFacility(facility) {}

size_t L3SupServFacilityMessage::l2BodyLength() const {
    return mFacility.lengthLV();
}

ParseResult<void> L3SupServFacilityMessage::try_writeBody(L3Frame& dest, size_t& wp) const {
    mFacility.writeLV(dest, wp);
    return ParseResult<void>();
}

ParseResult<void> L3SupServFacilityMessage::try_parseBody(const L3Frame& src, size_t& rp) {
    auto res = mFacility.try_parseLV(src, rp);
    if (!res.has_value()) return res;
    return ParseResult<void>();
}

void L3SupServFacilityMessage::text(std::ostream& os) const {
    os << "Facility: ";
    mFacility.text(os);
}

// ── L3SupServRegisterMessage ───────────────────────────────────────────

L3SupServRegisterMessage::L3SupServRegisterMessage()
    : mHaveVersion(false), mVersionIndicator(0) {}

L3SupServRegisterMessage::L3SupServRegisterMessage(unsigned wTI, const std::string& facility)
    : L3SupServMessage(wTI), mFacility(facility), mHaveVersion(false), mVersionIndicator(0) {}

size_t L3SupServRegisterMessage::l2BodyLength() const {
    size_t len = 1; // IEI tag always present
    if (mFacility.mExtant) len += mFacility.lengthLV();
    return len + (mHaveVersion ? 3 : 0);
}

ParseResult<void> L3SupServRegisterMessage::try_writeBody(L3Frame& dest, size_t& wp) const {
    mFacility.writeTLV(0x1c, dest, wp);
    if (mHaveVersion) {
        dest.writeField(wp, 0x7f, 8);  // IEI
        dest.writeField(wp, 1, 8);       // Length
        dest.writeField(wp, mVersionIndicator, 8);
    }
    return ParseResult<void>();
}

ParseResult<void> L3SupServRegisterMessage::try_parseBody(const L3Frame& src, size_t& rp) {
    auto tlRes = mFacility.try_parseTLV(0x1c, src, rp);
    if (!tlRes.has_value()) return tlRes;
    mHaveVersion = (src.peekField(rp, 8) == 0x7f);
    if (mHaveVersion) {
        rp += 8;  // skip IEI
        src.readField(rp, 8);  // skip length
        mVersionIndicator = src.readField(rp, 8);
    }
    return ParseResult<void>();
}

void L3SupServRegisterMessage::text(std::ostream& os) const {
    os << "Register: ";
    mFacility.text(os);
    if (mHaveVersion) os << " version=" << mVersionIndicator;
}

// ── L3SupServReleaseCompleteMessage ────────────────────────────────────

L3SupServReleaseCompleteMessage::L3SupServReleaseCompleteMessage()
    : mHaveCause(false), mCause() {}

L3SupServReleaseCompleteMessage::L3SupServReleaseCompleteMessage(unsigned wTI)
    : L3SupServMessage(wTI), mHaveCause(false), mCause() {}

L3SupServReleaseCompleteMessage::L3SupServReleaseCompleteMessage(unsigned wTI, CCCause cause)
    : L3SupServMessage(wTI), mHaveCause(true), mCause(cause) {}

size_t L3SupServReleaseCompleteMessage::l2BodyLength() const {
    size_t len = 0;
    if (mFacility.mExtant) len += mFacility.lengthTLV();
    if (mHaveCause) len += mCause.lengthTLV();
    return len;
}

ParseResult<void> L3SupServReleaseCompleteMessage::try_writeBody(L3Frame& dest, size_t& wp) const {
    if (mFacility.mExtant) mFacility.writeTLV(0x1c, dest, wp);
    if (mHaveCause) mCause.writeTLV(0x08, dest, wp);
    return ParseResult<void>();
}

ParseResult<void> L3SupServReleaseCompleteMessage::try_parseBody(const L3Frame& src, size_t& rp) {
    auto tlRes = mCause.try_parseTLV(0x08, src, rp);
    if (!tlRes.has_value()) return tlRes;
    mHaveCause = tlRes.value();
    auto facRes = mFacility.try_parseTLV(0x1c, src, rp);
    if (!facRes.has_value()) return facRes;
    return ParseResult<void>();
}

void L3SupServReleaseCompleteMessage::text(std::ostream& os) const {
    os << "SupServReleaseComplete";
    if (mFacility.mExtant) {
        os << " ";
        mFacility.text(os);
    }
    if (mHaveCause) os << ": " << CCCause2Str(mCause.cause());
}

// ── Factory & Parser (internal) ────────────────────────────────────────

namespace detail {

ParseResult<std::unique_ptr<L3SupServMessage>> L3SupServFactory(int mti) {
    switch (mti) {
        case L3SupServMessage::Facility:      return std::make_unique<L3SupServFacilityMessage>();
        case L3SupServMessage::Register:      return std::make_unique<L3SupServRegisterMessage>();
        case L3SupServMessage::ReleaseComplete: return std::make_unique<L3SupServReleaseCompleteMessage>();
        default:
            return ParseResult<std::unique_ptr<L3SupServMessage>>(
                ParseErrorCode::InvalidMTI, "Unknown SS message type: 0x" + std::to_string(mti & 0xFF));
    }
}

ParseResult<std::unique_ptr<L3SupServMessage>> parseL3SupServ(const L3Frame& source) {
    if (source.size() < 16) {
        return ParseResult<std::unique_ptr<L3SupServMessage>>(
            ParseErrorCode::TruncatedInput, "Frame too short for L3 header");
    }

    unsigned mti = source.mti() & 0xbf;
    auto factoryResult = L3SupServFactory(static_cast<L3SupServMessage::MessageType>(mti));
    if (!factoryResult.has_value()) {
        GSML3PARSER_LOG_WARN("Unknown SS MTI: 0x%02x", mti);
        return ParseResult<std::unique_ptr<L3SupServMessage>>(factoryResult.error());
    }

    auto& msg = factoryResult.value();
    msg->ti(source.ti());
    auto parseResult = msg->parse(source);
    if (!parseResult.has_value()) {
        GSML3PARSER_LOG_WARN("SS parse failed for MTI=0x%02x", mti);
        return ParseResult<std::unique_ptr<L3SupServMessage>>(parseResult.error());
    }

    return ParseResult<std::unique_ptr<L3SupServMessage>>(std::move(msg));
}

} // namespace detail

} // namespace gsml3parser
