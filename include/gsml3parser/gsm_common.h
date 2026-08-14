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
#include <ostream>
#include <string>

#include "types.h"

namespace gsml3parser {

// ── GSM timing constants ────────────────────────────────────────────────

/** Duration of a GSM frame in microseconds */
constexpr unsigned gFrameMicroseconds = 4615;

/** The GSM hyperframe - 2^21 frames ≈ 3h 28m 53s */
constexpr uint32_t gHyperframe = 2048u * 26u * 51u;

/**
 * GSM frame clock value - GSM 05.02 4.3
 */
class Time {
private:
    int mFN;
    int mTN;

public:
    Time(int wFN = 0, int wTN = 0) : mFN(wFN), mTN(wTN) {}

    int fn() const { return mFN; }
    void fn(int wFN) { mFN = wFN; }
    unsigned tn() const { return mTN; }
    void tn(unsigned wTN) { mTN = wTN; }

    unsigned sfn() const { return mFN / (26 * 51); }
    unsigned t1()  const { return sfn() % 2048; }
    unsigned t2()  const { return mFN % 26; }
    unsigned t3()  const { return mFN % 51; }
    unsigned t1p() const { return sfn() % 32; }
};

std::ostream& operator<<(std::ostream& os, const Time& ts);

/**
 * Get clock difference within the modulus: v1 - v2.
 */
int32_t FNDelta(int32_t v1, int32_t v2);

/** Compare two frame clock values: +1 if v1>v2, -1 if v1<v2, 0 if equal. */
int FNCompare(int32_t v1, int32_t v2);

// ── GSM alphabet tables ─────────────────────────────────────────────────

/** GSM 7-bit alphabet → ISO-8859-1 mapping */
extern const unsigned char gGSMAlphabet[];

/** BCD → ASCII mapping */
extern const char gBCDAlphabet[];

unsigned char encodeGSMChar(unsigned char ascii);
inline unsigned char decodeGSMChar(unsigned char sms) {
    return gGSMAlphabet[static_cast<unsigned>(sms)];
}

char encodeBCDChar(char ascii);
inline char decodeBCDChar(char bcd) {
    return gBCDAlphabet[static_cast<unsigned>(bcd)];
}

/** Convert raw bytes to a hex string. */
std::string data2hex(const unsigned char* data, unsigned nbytes);
std::string data2hex(const char* data, unsigned nbytes);

// ── RACH tables ─────────────────────────────────────────────────────────

/** "T" parameter - GSM 04.08 10.5.2.29, indexed by TxInteger */
extern const unsigned RACHSpreadSlots[16];

/** "S" parameter - GSM 04.08 3.3.1.1.2, indexed by TxInteger */
extern const unsigned RACHWaitSParam[16];
extern const unsigned RACHWaitSParamCombined[16];

} // namespace gsml3parser
