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

/// MSContext implementation — per-subscriber state for GSM L3 protocol handling.
#include "gsml3parser/stack/ms_context.h"

namespace gsml3parser {

// ── Factory methods ──────────────────────────────────────────────────────

MSContext MSContext::createWithTMSI(uint32_t tmsi) {
    MSContext ctx;
    ctx.mIdentity = L3MobileIdentity(tmsi);
    return ctx;
}

MSContext MSContext::createWithIMSI(std::string_view imsiDigits) {
    MSContext ctx;
    ctx.mIdentity = L3MobileIdentity(imsiDigits);
    return ctx;
}

// ── Identity accessors ───────────────────────────────────────────────────

const L3MobileIdentity& MSContext::identity() const noexcept {
    return mIdentity;
}

void MSContext::setTMSI(uint32_t tmsi) {
    mIdentity = L3MobileIdentity(tmsi);
}

void MSContext::setIMSI(std::string_view digits) {
    mIdentity = L3MobileIdentity(digits);
}

// ── Channel assignment ───────────────────────────────────────────────────

ChannelType MSContext::channelType() const noexcept {
    return mChannelType;
}

void MSContext::assignChannel(ChannelType type, uint8_t trx, uint8_t ts, uint16_t arfcn) {
    mChannelType = type;
    mTrxNumber = trx;
    mTimeslot = ts;
    mArfcn = arfcn;
}

void MSContext::releaseChannel() noexcept {
    mChannelType = ChannelType::UndefinedCHType;
    mTrxNumber = 0;
    mTimeslot = 0;
    mArfcn = 0;
}

uint8_t MSContext::trxNumber() const noexcept {
    return mTrxNumber;
}

uint8_t MSContext::timeslot() const noexcept {
    return mTimeslot;
}

uint16_t MSContext::arfcn() const noexcept {
    return mArfcn;
}

// ── Classmark ────────────────────────────────────────────────────────────

void MSContext::setClassmark(const L3MobileStationClassmark1& cm) {
    mClassmark = cm;
    mHasClassmark = true;
}

std::optional<L3MobileStationClassmark1> MSContext::classmark() const noexcept {
    if (mHasClassmark) {
        return mClassmark;
    }
    return std::nullopt;
}

// ── Location Area Identity ───────────────────────────────────────────────

std::optional<L3LocationAreaIdentity> MSContext::lai() const noexcept {
    if (mHasLAI) {
        return mLAI;
    }
    return std::nullopt;
}

void MSContext::setLAI(const L3LocationAreaIdentity& lai) {
    mLAI = lai;
    mHasLAI = true;
}

// ── Ciphering ────────────────────────────────────────────────────────────

bool MSContext::isCiphered() const noexcept {
    return mCiphered;
}

void MSContext::setCiphered(bool v) noexcept {
    mCiphered = v;
}

// ── Timing Advance ───────────────────────────────────────────────────────

std::optional<uint8_t> MSContext::timingAdvance() const noexcept {
    if (mHasTimingAdvance) {
        return mTimingAdvance;
    }
    return std::nullopt;
}

void MSContext::setTimingAdvance(uint8_t ta) noexcept {
    mTimingAdvance = ta;
    mHasTimingAdvance = true;
}

// ── Registration and Authentication flags ────────────────────────────────

bool MSContext::isRegistered() const noexcept {
    return mRegistered;
}

void MSContext::setRegistered(bool v) noexcept {
    mRegistered = v;
}

bool MSContext::isAuthenticated() const noexcept {
    return mAuthenticated;
}

void MSContext::setAuthenticated(bool v) noexcept {
    mAuthenticated = v;
}

} // namespace gsml3parser
