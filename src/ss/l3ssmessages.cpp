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
#include <sstream>
#include <iomanip>

namespace gsml3parser {

// ── L3SupServMessage ────────────────────────────────────────────────────

void L3SupServMessage::write(L3Frame& dest) const {
    size_t l3len = bitsNeeded();
    if (dest.size() != l3len) dest.resize(l3len);
    size_t wp = 0;
    dest.writeField(wp, mTI, 4);
    dest.writeField(wp, static_cast<unsigned>(PD()), 4);
    dest.writeField(wp, MTI(), 8);
    writeBody(dest, wp);
    dest.L2Length(l2Length());
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

void L3SupServFacilityMessage::writeBody(L3Frame& dest, size_t& wp) const {
    mFacility.writeLV(dest, wp);
}

void L3SupServFacilityMessage::parseBody(const L3Frame& src, size_t& rp) {
    mFacility.parseLV(src, rp);
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
    return mFacility.lengthLV() + (mHaveVersion ? 2 : 0);
}

void L3SupServRegisterMessage::writeBody(L3Frame& dest, size_t& wp) const {
    mFacility.writeLV(dest, wp);
    if (mHaveVersion) {
        dest.writeField(wp, mVersionIndicator, 8);
    }
}

void L3SupServRegisterMessage::parseBody(const L3Frame& src, size_t& rp) {
    mFacility.parseLV(src, rp);
    if (rp < src.size()) {
        mHaveVersion = true;
        mVersionIndicator = src.readField(rp, 8);
    }
}

void L3SupServRegisterMessage::text(std::ostream& os) const {
    os << "Register: ";
    mFacility.text(os);
    if (mHaveVersion) os << " version=" << mVersionIndicator;
}

// ── L3SupServReleaseCompleteMessage ────────────────────────────────────

L3SupServReleaseCompleteMessage::L3SupServReleaseCompleteMessage()
    : mHaveCause(false), mCause(CCCause::Unknown_L3_Cause) {}

L3SupServReleaseCompleteMessage::L3SupServReleaseCompleteMessage(unsigned wTI)
    : L3SupServMessage(wTI), mHaveCause(false), mCause(CCCause::Unknown_L3_Cause) {}

L3SupServReleaseCompleteMessage::L3SupServReleaseCompleteMessage(unsigned wTI, CCCause cause)
    : L3SupServMessage(wTI), mHaveCause(true), mCause(cause) {}

size_t L3SupServReleaseCompleteMessage::l2BodyLength() const {
    return (mFacility.mExtant ? mFacility.lengthLV() : 0) + (mHaveCause ? 2 : 0);
}

void L3SupServReleaseCompleteMessage::writeBody(L3Frame& dest, size_t& wp) const {
    if (mFacility.mExtant) mFacility.writeLV(dest, wp);
    if (mHaveCause) {
        dest.writeField(wp, static_cast<unsigned>(mCause), 8);
    }
}

void L3SupServReleaseCompleteMessage::parseBody(const L3Frame& src, size_t& rp) {
    if (rp < src.size()) {
        // Try to parse as facility or cause
        mFacility.parseLV(src, rp);
    }
}

void L3SupServReleaseCompleteMessage::text(std::ostream& os) const {
    os << "SupServReleaseComplete";
    if (mFacility.mExtant) {
        os << " ";
        mFacility.text(os);
    }
    if (mHaveCause) os << ": " << CCCause2Str(mCause);
}

// ── Factory ─────────────────────────────────────────────────────────────

L3SupServMessage* L3SupServFactory(int mti) {
    switch (mti) {
        case L3SupServMessage::Facility:      return new L3SupServFacilityMessage();
        case L3SupServMessage::Register:      return new L3SupServRegisterMessage();
        case L3SupServMessage::ReleaseComplete: return new L3SupServReleaseCompleteMessage();
        default:                              return nullptr;
    }
}

// ── Parser ──────────────────────────────────────────────────────────────

std::unique_ptr<L3SupServMessage> parseL3SupServ(const L3Frame& source) {
    if (source.size() < 16) return nullptr;

    unsigned mti = source.MTI();
    L3SupServMessage* msg = L3SupServFactory(static_cast<L3SupServMessage::MessageType>(mti));
    if (!msg) {
        GSML3PARSER_LOG_WARN("Unknown SS MTI: 0x%02x", mti);
        return nullptr;
    }
    try {
        msg->setTI(source.TI());
        msg->parse(source);
    } catch (const ParseError&) {
        GSML3PARSER_LOG_WARN("SS parse failed for MTI=0x%02x", mti);
        delete msg;
        return nullptr;
    }
    return std::unique_ptr<L3SupServMessage>(msg);
}

} // namespace gsml3parser
