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

L3Frame::L3Frame() : BitVector(), mPrimitive(Primitive::L3_DATA), mSapi(SAPI::SAPI0), mL2Length(0) {
    init();
}

L3Frame::L3Frame(Primitive prim) : BitVector(), mPrimitive(prim), mSapi(SAPI::SAPI0), mL2Length(0) {
    init();
}

L3Frame::L3Frame(SAPI sapi, Primitive prim) : BitVector(), mPrimitive(prim), mSapi(sapi), mL2Length(0) {
    init();
}

L3Frame::L3Frame(Primitive prim, size_t nbits, SAPI sapi)
    : BitVector(nbits), mPrimitive(prim), mSapi(sapi), mL2Length(nbits)
{
    init();
}

L3Frame::L3Frame(SAPI sapi, const BitVector& source, Primitive prim)
    : BitVector(source), mPrimitive(prim), mSapi(sapi),
      mL2Length(source.size() / 8)
{
    if (source.size() % 8) mL2Length++;
    init();
}

L3Frame::L3Frame(SAPI sapi, const char* hexString)
    : BitVector(), mPrimitive(Primitive::L3_DATA), mSapi(sapi), mL2Length(0)
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
        resize(bytes.size() * 8);
        std::memcpy(mStart, bytes.data(), bytes.size());
        mL2Length = bytes.size();
    }
    init();
}

L3Frame::L3Frame(const L3Frame& other)
    : BitVector(other), mPrimitive(other.mPrimitive), mSapi(other.mSapi),
      mL2Length(other.mL2Length), mTimestamp(other.mTimestamp)
{
}

L3Frame& L3Frame::operator=(const L3Frame& other) {
    if (this != &other) {
        BitVector::operator=(other);
        mPrimitive = other.mPrimitive;
        mSapi = other.mSapi;
        mL2Length = other.mL2Length;
        mTimestamp = other.mTimestamp;
    }
    return *this;
}

L3PD L3Frame::PD() const {
    if (size() < 8) return L3PD::Undefined;
    return static_cast<L3PD>(peekField(4, 4));
}

unsigned L3Frame::MTI() const {
    if (size() < 16) return 0;
    return peekField(8, 8);
}

unsigned L3Frame::TI() const {
    if (size() < 4) return 0;
    return peekField(0, 4);
}

bool L3Frame::isData() const {
    return mPrimitive == Primitive::L3_DATA || mPrimitive == Primitive::L3_UNIT_DATA;
}

void L3Frame::writeH(size_t& wp) const {
    writeField(wp, 1, 1);
}

void L3Frame::writeL(size_t& wp) const {
    writeField(wp, 0, 1);
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
    os << " " << *static_cast<const BitVector*>(this);
}

std::ostream& operator<<(std::ostream& os, const L3Frame& frame) {
    frame.text(os);
    return os;
}

} // namespace gsml3parser
