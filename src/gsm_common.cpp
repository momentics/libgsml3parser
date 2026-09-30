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
#include "gsml3parser/enums.h"
#include <sstream>
#include <iomanip>

namespace gsml3parser {

// ── GSM 7-bit alphabet ──────────────────────────────────────────────────

// GSM 7-bit default alphabet (TS 23.038): 127 code points mapped to
// ISO-8859-1; code points beyond the default range decode as space.
const unsigned char gGSMAlphabet[] =
    "@\243$\245\350\351\371\354\362\347\n\330\370\r\305\345"
    "D_FGLOPCSTZ \306\346\337\311!\"#\244%&\'()*+,-./0123456789:;<=>?\241"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ\304\326\321\334\247\277"
    "abcdefghijklmnopqrstuvwxyz\344\366\361\374\341";
static_assert(sizeof(gGSMAlphabet) - 1 == kGsm7TableSize,
              "the default alphabet table must hold exactly the 127 code points");

// BCD nibble -> ASCII mapping (TS 23.040): the ten digits at
// indices 0..9, '*' at 10/11/13, '#' at 12/14, and the fill nibble 'F'
// rendered as 'f' at index 15.
const char gBCDAlphabet[] = "0123456789**#*#f";

static_assert(sizeof(gBCDAlphabet) - 1 == 16u);

unsigned char encodeGSMChar(unsigned char ascii) {
    for (unsigned i = 0; i < kGsm7TableSize; ++i) {
        if (gGSMAlphabet[i] == ascii) return static_cast<unsigned char>(i);
    }
    return ' ';
}

char encodeBCDChar(char ascii) {
    // Digits map to their BCD value; everything else (including padding)
    // maps to the fill nibble 0x0F (TS 23.040).
    if (ascii >= '0' && ascii <= '9') return static_cast<char>(ascii - '0');
    return 0x0Fu;
}

std::string data2hex(const unsigned char* data, unsigned nbytes) {
    std::ostringstream os;
    os << std::hex << std::uppercase;
    for (unsigned i = 0; i < nbytes; ++i) {
        os << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]);
    }
    return os.str();
}

std::string data2hex(const char* data, unsigned nbytes) {
    return data2hex(reinterpret_cast<const unsigned char*>(data), nbytes);
}

// ── RACH tables ─────────────────────────────────────────────────────────

// RACH transmission parameters indexed by the broadcast Tx integer
// (0..15), per TS 44.018 section 10.5.2.29: T (number of slots used to
// spread the RACH transmission) and S (wait parameter for non-combined
// and combined CCCH).
const unsigned RACHSpreadSlots[16] = {
    3, 4, 5, 6,
    7, 8, 9, 10,
    11, 12, 14, 16,
    20, 25, 32, 50
};

const unsigned RACHWaitSParam[16] = {
    55, 76, 109, 163, 217,
    55, 76, 109, 163, 217,
    55, 76, 109, 163, 217,
    55
};

const unsigned RACHWaitSParamCombined[16] = {
    41, 52, 58, 86, 115,
    41, 52, 58, 86, 115,
    41, 52, 58, 86, 115,
    41
};

// ── Time ────────────────────────────────────────────────────────────────

int32_t FNDelta(int32_t v1, int32_t v2) {
    int32_t delta = v1 - v2;
    int32_t half = static_cast<int32_t>(gHyperframe / 2);
    if (delta > half) delta -= static_cast<int32_t>(gHyperframe);
    if (delta < -half) delta += static_cast<int32_t>(gHyperframe);
    return delta;
}

int FNCompare(int32_t v1, int32_t v2) {
    int32_t delta = FNDelta(v1, v2);
    if (delta > 0) return 1;
    if (delta < 0) return -1;
    return 0;
}

std::ostream& operator<<(std::ostream& os, const Time& ts) {
    os << "FN=" << ts.fn() << " TN=" << ts.tn();
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

std::ostream& operator<<(std::ostream& os, LogLevel level) {
    switch (level) {
        case LogLevel::EMERG:   os << "EMERG"; break;
        case LogLevel::ALERT:   os << "ALERT"; break;
        case LogLevel::CRIT:    os << "CRIT"; break;
        case LogLevel::ERR:     os << "ERR"; break;
        case LogLevel::WARNING: os << "WARNING"; break;
        case LogLevel::NOTICE:  os << "NOTICE"; break;
        case LogLevel::INFO:    os << "INFO"; break;
        case LogLevel::DEBUG:   os << "DEBUG"; break;
    }
    return os;
}

std::ostream& operator<<(std::ostream& os, GSMAlphabet alphabet) {
    switch (alphabet) {
        case GSMAlphabet::ALPHABET_7BIT: os << "GSM-7bit"; break;
        case GSMAlphabet::ALPHABET_8BIT: os << "8bit"; break;
        case GSMAlphabet::ALPHABET_UCS2: os << "UCS2"; break;
    }
    return os;
}

std::ostream& operator<<(std::ostream& os, CCCauseLocation loc) {
    switch (loc) {
        case CCCauseLocation::User:                 os << "User"; break;
        case CCCauseLocation::Private_Serving_Local: os << "PrivateServingLocal"; break;
        case CCCauseLocation::Public_Serving_Local:  os << "PublicServingLocal"; break;
        case CCCauseLocation::Transit:               os << "Transit"; break;
        case CCCauseLocation::Public_Serving_Remote: os << "PublicServingRemote"; break;
        case CCCauseLocation::Private_Serving_Remote:os << "PrivateServingRemote"; break;
        case CCCauseLocation::International:         os << "International"; break;
        case CCCauseLocation::Beyond_Inter_Networking: os << "BeyondInterNetworking"; break;
    }
    return os;
}

} // namespace gsml3parser
