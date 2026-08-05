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

#include "gsml3parser/l3frame.h"
#include <chrono>
#include <sstream>
#include <iomanip>

namespace gsml3parser {

static double nowSeconds() {
    auto tp = std::chrono::system_clock::now();
    return std::chrono::duration<double>(tp.time_since_epoch()).count();
}

void L3Frame::init() {
    mTimestamp = nowSeconds();
}

L3Frame::L3Frame()
    : mPrimitive(Primitive::L3_DATA), mSapi(SAPI::SAPI0), mL2Length(0), mTimestamp(0) {
    init();
}

L3Frame::L3Frame(Primitive prim)
    : mPrimitive(prim), mSapi(SAPI::SAPI0), mL2Length(0), mTimestamp(0) {
    init();
}

L3Frame::L3Frame(SAPI sapi, Primitive prim)
    : mPrimitive(prim), mSapi(sapi), mL2Length(0), mTimestamp(0) {
    init();
}

L3Frame::L3Frame(Primitive prim, size_t nbits, SAPI sapi)
    : mPayload(nbits), mPrimitive(prim), mSapi(sapi), mL2Length(nbits), mTimestamp(0) {
    init();
}

L3Frame::L3Frame(SAPI sapi, const BitVector& source, Primitive prim)
    : mPayload(source), mPrimitive(prim), mSapi(sapi),
      mL2Length(source.size() / 8), mTimestamp(0)
{
    if (source.size() % 8) mL2Length++;
    init();
}

L3Frame::L3Frame(SAPI sapi, const char* hexString)
    : mPrimitive(Primitive::L3_DATA), mSapi(sapi), mL2Length(0), mTimestamp(0)
{
    std::string clean;
    for (const char* p = hexString; *p; ++p) {
        if (std::isxdigit(static_cast<unsigned char>(*p))) {
            clean += *p;
        }
    }
    std::vector<uint8_t> bytes;
    for (size_t i = 0; i + 1 < clean.size(); i += 2) {
        std::string byteStr = clean.substr(i, 2);
        bytes.push_back(static_cast<uint8_t>(std::stoi(byteStr, nullptr, 16)));
    }
    if (!bytes.empty()) {
        mPayload.resize(bytes.size() * 8);
        std::memcpy(mPayload.data(), bytes.data(), bytes.size());
        mL2Length = bytes.size();
    }
    init();
}

L3Frame::L3Frame(const L3Frame& other)
    : mPayload(other.mPayload), mPrimitive(other.mPrimitive), mSapi(other.mSapi),
      mL2Length(other.mL2Length), mTimestamp(other.mTimestamp)
{
}

L3Frame::L3Frame(L3Frame&& other) noexcept
    : mPayload(std::move(other.mPayload)), mPrimitive(other.mPrimitive), mSapi(other.mSapi),
      mL2Length(other.mL2Length), mTimestamp(other.mTimestamp)
{
}

L3Frame& L3Frame::operator=(const L3Frame& other) {
    if (this != &other) {
        mPayload = other.mPayload;
        mPrimitive = other.mPrimitive;
        mSapi = other.mSapi;
        mL2Length = other.mL2Length;
        mTimestamp = other.mTimestamp;
    }
    return *this;
}

L3Frame& L3Frame::operator=(L3Frame&& other) noexcept {
    if (this != &other) {
        mPayload = std::move(other.mPayload);
        mPrimitive = other.mPrimitive;
        mSapi = other.mSapi;
        mL2Length = other.mL2Length;
        mTimestamp = other.mTimestamp;
    }
    return *this;
}

L3PD L3Frame::pd() const {
    if (mPayload.size() < 8) return L3PD::Undefined;
    return static_cast<L3PD>(mPayload.peekField(0, 4));
}

unsigned L3Frame::mti() const {
    if (mPayload.size() < 16) return 0;
    unsigned mtiVal = mPayload.peekField(8, 8);
    L3PD pd = this->pd();
    // MM, CC, SS: byte 1 = messageType(6)|NSD(2) — GSM 04.08 10.4
    // Bit 7 of byte 1 is "don't care" (direction indicator), mask with 0xFC then shift
    if (pd == L3PD::MobilityManagement || pd == L3PD::CallControl ||
        pd == L3PD::NonCallSS) {
        return (mtiVal & 0xFC) >> 2;
    }
    // RR short messages: TIF=1 indicates MTI >= 0x100
    if (pd == L3PD::RadioResource && mPayload.size() >= 8 && mPayload.peekField(7, 1)) {
        return 0x100 + (mtiVal & 0xFF);
    }
    return mtiVal;
}

unsigned L3Frame::ti() const {
    if (mPayload.size() < 8) return 0;
    return mPayload.peekField(4, 3);  // TIO: 3 bits (bits 4-6 of byte 0)
}

unsigned L3Frame::tif() const {
    if (mPayload.size() < 8) return 0;
    return mPayload.peekField(7, 1);  // TIF: 1 bit (bit 7 of byte 0)
}

bool L3Frame::isData() const {
    return mPrimitive == Primitive::L3_DATA || mPrimitive == Primitive::L3_UNIT_DATA;
}

static const unsigned fillPattern[8] = {0,0,1,0,1,0,1,1};

void L3Frame::writeH(size_t& wp) {
    unsigned fillBit = fillPattern[wp % 8];
    mPayload.writeField(wp, !fillBit, 1);
}

void L3Frame::writeL(size_t& wp) {
    unsigned fillBit = fillPattern[wp % 8];
    mPayload.writeField(wp, fillBit, 1);
}

void L3Frame::text(std::ostream& os) const {
    os << "L3Frame[P=";
    switch (mPrimitive) {
        case Primitive::L3_DATA: os << "L3_DATA"; break;
        case Primitive::L3_UNIT_DATA: os << "L3_UNIT_DATA"; break;
        default: os << static_cast<int>(mPrimitive); break;
    }
    os << " SAPI=" << static_cast<int>(mSapi);
    os << " len=" << mL2Length;
    os << " " << mPayload;
}

std::ostream& operator<<(std::ostream& os, const L3Frame& frame) {
    frame.text(os);
    return os;
}

} // namespace gsml3parser
