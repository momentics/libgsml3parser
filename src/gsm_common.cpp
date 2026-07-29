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

#include "gsml3parser/gsm_common.h"
#include <sstream>
#include <iomanip>

namespace gsml3parser {

// ── GSM 7-bit alphabet ──────────────────────────────────────────────────

const unsigned char gGSMAlphabet[] = {
    '@',0xa3,'$','%',0xe8,0xe9,0xf9,0xe4,0xf2,0xe7,
    '\n',0xda,0xf8,'\r',0xc5,0xe5,'D','_','F','G',
    'L','O','P','C','S','T','Z',' ',0xd6,0xe6,0xbf,
    0xc9,'!','"','#',0xa4,'&','\'','(',')','*','+',
    '-','.','/','0','1','2','3','4','5','6','7','8',
    '9',':',';','<','=','>','?',0xa1,'A','B','C',
    'E','H','I','J','K','M','N','Q','R','U','V',
    'W','X','Y',0xc4,0xd6,0xd1,0xdc,0xa7,0xff,
    'a','b','c','d','e','f','g','h','i','j','k',
    'l','m','n','o','p','q','s','t','u','v','w',
    'x','y','z',0xe4,0xf6,0xf1,0xfc,0xe1
};

const char gBCDAlphabet[] = "0123456789.#abc";

unsigned char encodeGSMChar(unsigned char ascii) {
    for (unsigned i = 0; i < sizeof(gGSMAlphabet); ++i) {
        if (gGSMAlphabet[i] == ascii) return static_cast<unsigned char>(i);
    }
    return ' ';
}

char encodeBCDChar(char ascii) {
    for (unsigned i = 0; i < sizeof(gBCDAlphabet) - 1; ++i) {
        if (gBCDAlphabet[i] == ascii) return static_cast<char>(i);
    }
    return 'a'; // padding
}

std::string data2hex(const unsigned char* data, unsigned nbytes) {
    std::ostringstream os;
    for (unsigned i = 0; i < nbytes; ++i) {
        os << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]);
    }
    return os.str();
}

std::string data2hex(const char* data, unsigned nbytes) {
    return data2hex(reinterpret_cast<const unsigned char*>(data), nbytes);
}

// ── RACH tables ─────────────────────────────────────────────────────────

const unsigned RACHSpreadSlots[16] = {
    3,  5,  7, 10, 14, 21, 28, 42,
    7, 10, 14, 21, 28, 42, 42, 42
};

const unsigned RACHWaitSParam[16] = {
    2, 3, 3, 3, 3, 3, 3, 3,
    4, 4, 4, 4, 4, 4, 4, 4
};

const unsigned RACHWaitSParamCombined[16] = {
    2, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3
};

// ── Time ────────────────────────────────────────────────────────────────

int32_t FNDelta(int32_t v1, int32_t v2) {
    int32_t delta = v1 - v2;
    if (delta > 0x400000) delta -= 0x800000;
    if (delta < -0x400000) delta += 0x800000;
    return delta;
}

int FNCompare(int32_t v1, int32_t v2) {
    int32_t delta = FNDelta(v1, v2);
    if (delta > 0) return 1;
    if (delta < 0) return -1;
    return 0;
}

std::ostream& operator<<(std::ostream& os, const Time& ts) {
    os << "FN=" << ts.FN() << " TN=" << ts.TN();
    return os;
}

// ── Type stream operators ───────────────────────────────────────────────

std::ostream& operator<<(std::ostream& os, L3PD pd) {
    switch (pd) {
        case L3PD::CallControl:            os << "CallControl"; break;
        case L3PD::MobilityManagement:     os << "MobilityManagement"; break;
        case L3PD::RadioResource:          os << "RadioResource"; break;
        case L3PD::SMS:                    os << "SMS"; break;
        case L3PD::NonCallSS:              os << "NonCallSS"; break;
        case L3PD::GPRSMobilityManagement: os << "GPRSMobility"; break;
        case L3PD::GPRSSessionManagement:  os << "GPRSSession"; break;
        default:                           os << "PD(0x" << std::hex << static_cast<int>(pd) << ")"; break;
    }
    return os;
}

std::ostream& operator<<(std::ostream& os, Primitive prim) {
    switch (prim) {
        case Primitive::L3_DATA:           os << "L3_DATA"; break;
        case Primitive::L3_UNIT_DATA:      os << "L3_UNIT_DATA"; break;
        default:                           os << "Prim(" << static_cast<int>(prim) << ")"; break;
    }
    return os;
}

std::ostream& operator<<(std::ostream& os, SAPI sapi) {
    os << "SAPI" << static_cast<int>(sapi);
    return os;
}

std::ostream& operator<<(std::ostream& os, MobileIDType type) {
    switch (type) {
        case MobileIDType::NoID:  os << "NoID"; break;
        case MobileIDType::IMSI:  os << "IMSI"; break;
        case MobileIDType::IMEI:  os << "IMEI"; break;
        case MobileIDType::IMEISV: os << "IMEISV"; break;
        case MobileIDType::TMSI:  os << "TMSI"; break;
    }
    return os;
}

std::ostream& operator<<(std::ostream& os, TypeOfNumber ton) {
    switch (ton) {
        case TypeOfNumber::International:  os << "International"; break;
        case TypeOfNumber::National:       os << "National"; break;
        case TypeOfNumber::ShortCode:      os << "ShortCode"; break;
        case TypeOfNumber::Alphanumeric:   os << "Alphanumeric"; break;
        default:                           os << "TON(" << static_cast<int>(ton) << ")"; break;
    }
    return os;
}

std::ostream& operator<<(std::ostream& os, NumberingPlan np) {
    switch (np) {
        case NumberingPlan::E164:    os << "E164"; break;
        case NumberingPlan::X121:     os << "X121"; break;
        case NumberingPlan::National: os << "National"; break;
        default:                      os << "NP(" << static_cast<int>(np) << ")"; break;
    }
    return os;
}

std::ostream& operator<<(std::ostream& os, ChannelType ch) {
    switch (ch) {
        case ChannelType::SDCCHType: os << "SDCCH"; break;
        case ChannelType::TCHFType:  os << "TCH/F"; break;
        case ChannelType::TCHHType:  os << "TCH/H"; break;
        case ChannelType::BCCHType:  os << "BCCH"; break;
        case ChannelType::CCCHType:  os << "CCCH"; break;
        case ChannelType::RACHType:  os << "RACH"; break;
        default:                      os << "CH(" << static_cast<int>(ch) << ")"; break;
    }
    return os;
}

} // namespace gsml3parser
