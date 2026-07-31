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

#include "gsml3parser/common/l3common.h"
#include "gsml3parser/gsm_common.h"
#include <cstring>
#include <sstream>
#include <iomanip>

namespace gsml3parser {

// ── L3CellIdentity ──────────────────────────────────────────────────────

void L3CellIdentity::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, mID, 16);
}

void L3CellIdentity::parseV(const L3Frame& src, size_t& rp) {
    mID = src.readField(rp, 16);
}

void L3CellIdentity::parseV(const L3Frame& src, size_t& rp, size_t) {
    parseV(src, rp);
}

void L3CellIdentity::text(std::ostream& os) const {
    os << "CI=" << mID;
}

// ── L3LocationAreaIdentity ─────────────────────────────────────────────

L3LocationAreaIdentity::L3LocationAreaIdentity(const char* wMCC, const char* wMNC, unsigned wLAC)
    : mLAC(wLAC)
{
    for (int i = 0; i < 3; ++i) mMCC[i] = (wMCC[i] >= '0' && wMCC[i] <= '9') ? wMCC[i] - '0' : 0;
    for (int i = 0; i < 3; ++i) mMNC[i] = (wMNC[i] >= '0' && wMNC[i] <= '9') ? wMNC[i] - '0' : 'f' - '0';
}

bool L3LocationAreaIdentity::operator==(const L3LocationAreaIdentity& other) const {
    for (int i = 0; i < 3; ++i) {
        if (mMCC[i] != other.mMCC[i]) return false;
        if (mMNC[i] != other.mMNC[i]) return false;
    }
    return mLAC == other.mLAC;
}

int L3LocationAreaIdentity::MCC() const {
    return mMCC[0] * 100 + mMCC[1] * 10 + mMCC[2];
}

int L3LocationAreaIdentity::MNC() const {
    int val = mMNC[0] * 10 + mMNC[1];
    if (mMNC[2] < 15) val = val * 10 + mMNC[2];
    return val;
}

void L3LocationAreaIdentity::parseV(const L3Frame& source, size_t& rp) {
    // GSM 04.08 10.5.1.3: BCD swapped nibble order
    mMCC[1] = source.readField(rp, 4);
    mMCC[0] = source.readField(rp, 4);
    mMNC[2] = source.readField(rp, 4);
    mMCC[2] = source.readField(rp, 4);
    mMNC[1] = source.readField(rp, 4);
    mMNC[0] = source.readField(rp, 4);
    mLAC = source.readField(rp, 16);
}

void L3LocationAreaIdentity::parseV(const L3Frame& src, size_t& rp, size_t) {
    parseV(src, rp);
}

void L3LocationAreaIdentity::writeV(L3Frame& dest, size_t& wp) const {
    // GSM 04.08 10.5.1.3: BCD swapped nibble order
    dest.writeField(wp, mMCC[1], 4);
    dest.writeField(wp, mMCC[0], 4);
    dest.writeField(wp, mMNC[2], 4);
    dest.writeField(wp, mMCC[2], 4);
    dest.writeField(wp, mMNC[1], 4);
    dest.writeField(wp, mMNC[0], 4);
    dest.writeField(wp, mLAC, 16);
}

void L3LocationAreaIdentity::text(std::ostream& os) const {
    os << "MCC=" << mMCC[0] << mMCC[1] << mMCC[2]
       << " MNC=" << mMNC[0] << mMNC[1] << mMNC[2]
       << " LAC=" << mLAC;
}

// ── L3MobileIdentity ────────────────────────────────────────────────────

L3MobileIdentity::L3MobileIdentity() : mType(MobileIDType::NoID), mTMSI(0) {
    mDigits[0] = '\0';
}

L3MobileIdentity::L3MobileIdentity(uint32_t wTMSI)
    : mType(MobileIDType::TMSI), mTMSI(wTMSI)
{
    mDigits[0] = '\0';
}

L3MobileIdentity::L3MobileIdentity(const char* wDigits)
    : mType(MobileIDType::IMSI), mTMSI(0)
{
    std::strncpy(mDigits, wDigits, sizeof(mDigits) - 1);
    mDigits[sizeof(mDigits) - 1] = '\0';
}

const char* L3MobileIdentity::digits() const {
    if (mType == MobileIDType::TMSI) return nullptr;
    return mDigits;
}

uint32_t L3MobileIdentity::TMSI() const {
    return mTMSI;
}

bool L3MobileIdentity::operator==(const L3MobileIdentity& other) const {
    if (mType != other.mType) return false;
    if (mType == MobileIDType::TMSI) return mTMSI == other.mTMSI;
    return std::strcmp(mDigits, other.mDigits) == 0;
}

bool L3MobileIdentity::operator<(const L3MobileIdentity& other) const {
    if (mType < other.mType) return true;
    if (mType > other.mType) return false;
    if (mType == MobileIDType::TMSI) return mTMSI < other.mTMSI;
    return std::strcmp(mDigits, other.mDigits) < 0;
}

size_t L3MobileIdentity::lengthV() const {
    if (mType == MobileIDType::NoID) return 1;
    if (mType == MobileIDType::TMSI) return 5;
    // BCD: 1 byte (first digit + odd flag + type) + ceil(nDigits/2) bytes
    size_t nDigits = std::strlen(mDigits);
    return 1 + (nDigits + 1) / 2;
}

void L3MobileIdentity::writeV(L3Frame& dest, size_t& wp) const {
    // GSM 04.08 10.5.1.4
    if (mType == MobileIDType::NoID) {
        dest.writeField(wp, 0x0f0, 8);
        return;
    }
    if (mType == MobileIDType::TMSI) {
        dest.writeField(wp, 0x0f4, 8);
        dest.writeField(wp, mTMSI, 32);
        return;
    }
    size_t nDigits = std::strlen(mDigits);
    if (nDigits == 0) {
        dest.writeField(wp, 0x0f0, 8);
        return;
    }
    // First byte: digit1(4) | oddCount(1) | type(3)
    dest.writeField(wp, mDigits[0] - '0', 4);
    dest.writeField(wp, nDigits % 2, 1);
    dest.writeField(wp, static_cast<unsigned>(mType), 3);
    // Remaining bytes: pairs of digits, swapped nibbles
    size_t i = 1;
    while (i < nDigits) {
        if (i + 1 < nDigits)
            dest.writeField(wp, mDigits[i + 1] - '0', 4);
        else
            dest.writeField(wp, 0x0f, 4);
        dest.writeField(wp, mDigits[i] - '0', 4);
        i += 2;
    }
}

void L3MobileIdentity::parseV(const L3Frame& src, size_t& rp, size_t expectedLength) {
    // GSM 04.08 10.5.1.4
    size_t endCount = rp + expectedLength * 8;
    int numDigits = 0;
    mDigits[numDigits++] = static_cast<char>(src.readField(rp, 4) + '0');
    bool oddCount = (src.readField(rp, 1) != 0);
    mType = static_cast<MobileIDType>(src.readField(rp, 3));

    switch (mType) {
        case MobileIDType::TMSI:
            mDigits[0] = '\0';
            mTMSI = src.readField(rp, 32);
            break;
        case MobileIDType::IMSI:
        case MobileIDType::IMEI:
        case MobileIDType::IMEISV:
            while (rp < endCount) {
                unsigned tmp = src.readField(rp, 4);
                mDigits[numDigits++] = static_cast<char>(src.readField(rp, 4) + '0');
                mDigits[numDigits++] = static_cast<char>(tmp + '0');
                if (numDigits > 16) {
                    throw ParseError("MobileIdentity: too many digits");
                }
            }
            if (!oddCount) numDigits--;
            mDigits[numDigits] = '\0';
            break;
        default:
            mDigits[0] = '\0';
            mType = MobileIDType::NoID;
            break;
    }
}

void L3MobileIdentity::text(std::ostream& os) const {
    switch (mType) {
        case MobileIDType::TMSI:
            os << "TMSI=" << std::hex << mTMSI;
            break;
        case MobileIDType::IMSI:
            os << "IMSI=" << mDigits;
            break;
        case MobileIDType::IMEI:
            os << "IMEI=" << mDigits;
            break;
        case MobileIDType::IMEISV:
            os << "IMEISV=" << mDigits;
            break;
        default:
            os << "NoID";
            break;
    }
}

// ── L3MobileStationClassmark1 ───────────────────────────────────────────

void L3MobileStationClassmark1::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, 0, 1);  // spare
    dest.writeField(wp, mRevisionLevel, 2);
    dest.writeField(wp, mES_IND, 1);
    dest.writeField(wp, mA5_1, 1);
    dest.writeField(wp, mRFPowerCapability, 3);
}

void L3MobileStationClassmark1::parseV(const L3Frame& src, size_t& rp) {
    src.readField(rp, 1);  // spare
    mRevisionLevel = src.readField(rp, 2);
    mES_IND        = src.readField(rp, 1);
    mA5_1          = src.readField(rp, 1);
    mRFPowerCapability = src.readField(rp, 3);
}

void L3MobileStationClassmark1::text(std::ostream& os) const {
    os << "CM1[rev=" << mRevisionLevel << " ES=" << mES_IND
       << " A5_1=" << mA5_1 << " PWR=" << mRFPowerCapability;
}

// ── L3MobileStationClassmark2 ───────────────────────────────────────────

void L3MobileStationClassmark2::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, 0, 1);  // spare
    dest.writeField(wp, mRevisionLevel, 2);
    dest.writeField(wp, mES_IND, 1);
    dest.writeField(wp, mA5_1, 1);
    dest.writeField(wp, mRFPowerCapability, 3);
    dest.writeField(wp, 0, 1);  // spare
    dest.writeField(wp, mPSCapability, 1);
    dest.writeField(wp, mSSScreenIndicator, 2);
    dest.writeField(wp, mSMCapability, 1);
    dest.writeField(wp, mVBS, 1);
    dest.writeField(wp, mVGCS, 1);
    dest.writeField(wp, mFC, 1);
    dest.writeField(wp, mCM3, 1);
    dest.writeField(wp, 0, 1);  // spare
    dest.writeField(wp, mLCSVACapability, 1);
    dest.writeField(wp, 0, 1);  // spare
    dest.writeField(wp, mSoLSA, 1);
    dest.writeField(wp, mCMSF, 1);
    dest.writeField(wp, mA5_3, 1);
    dest.writeField(wp, mA5_2, 1);
}

void L3MobileStationClassmark2::parseV(const L3Frame& src, size_t& rp) {
    src.readField(rp, 1);  // spare
    mRevisionLevel      = src.readField(rp, 2);
    mES_IND             = src.readField(rp, 1);
    mA5_1               = src.readField(rp, 1);
    mRFPowerCapability  = src.readField(rp, 3);
    src.readField(rp, 1);  // spare
    mPSCapability       = src.readField(rp, 1);
    mSSScreenIndicator  = src.readField(rp, 2);
    mSMCapability       = src.readField(rp, 1);
    mVBS                = src.readField(rp, 1);
    mVGCS               = src.readField(rp, 1);
    mFC                 = src.readField(rp, 1);
    mCM3                = src.readField(rp, 1);
    src.readField(rp, 1);  // spare
    mLCSVACapability    = src.readField(rp, 1);
    src.readField(rp, 1);  // spare
    mSoLSA              = src.readField(rp, 1);
    mCMSF               = src.readField(rp, 1);
    mA5_3               = src.readField(rp, 1);
    mA5_2               = src.readField(rp, 1);
}

void L3MobileStationClassmark2::parseV(const L3Frame& src, size_t& rp, size_t) {
    parseV(src, rp);
}

void L3MobileStationClassmark2::text(std::ostream& os) const {
    os << "CM2[rev=" << mRevisionLevel << " ES=" << mES_IND
       << " A5_1=" << mA5_1 << " A5_3=" << mA5_3 << " A5_2=" << mA5_2
       << " PWR=" << mRFPowerCapability;
}

int L3MobileStationClassmark2::getA5Bits() const {
    int result = 0;
    if (mA5_1 == 0) result |= 1;
    if (mA5_2 != 0) result |= 2;
    if (mA5_3 != 0) result |= 4;
    return result;
}

// ── L3MobileStationClassmark3 ───────────────────────────────────────────

L3MobileStationClassmark3::L3MobileStationClassmark3()
    : mMultiband(0), mA5_4(0), mA5_5(0), mA5_6(0), mA5_7(0) {}

void L3MobileStationClassmark3::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, 0, 1);  // spare
    dest.writeField(wp, mMultiband, 3);
    dest.writeField(wp, mA5_7, 1);
    dest.writeField(wp, mA5_6, 1);
    dest.writeField(wp, mA5_5, 1);
    dest.writeField(wp, mA5_4, 1);
    for (int i = 0; i < 106; i++) {
        dest.writeField(wp, 0, 1);
    }
}

void L3MobileStationClassmark3::parseV(const L3Frame& src, size_t& rp) {
    src.readField(rp, 1);  // spare
    mMultiband = src.readField(rp, 3);
    mA5_7      = src.readField(rp, 1);
    mA5_6      = src.readField(rp, 1);
    mA5_5      = src.readField(rp, 1);
    mA5_4      = src.readField(rp, 1);
}

void L3MobileStationClassmark3::parseV(const L3Frame& src, size_t& rp, size_t) {
    parseV(src, rp);
}

void L3MobileStationClassmark3::text(std::ostream& os) const {
    os << "CM3[MB=" << mMultiband << " A5_4=" << mA5_4
       << " A5_5=" << mA5_5 << " A5_6=" << mA5_6 << " A5_7=" << mA5_7;
}

// ── L3CipheringKeySequenceNumber ────────────────────────────────────────

void L3CipheringKeySequenceNumber::writeV(L3Frame& dest, size_t& wp) const { dest.writeField(wp, mCIValue & 0x0f, 4); }

void L3CipheringKeySequenceNumber::text(std::ostream& os) const {
    os << "CKSN=" << mCIValue;
}

// ── L3FrequencyList ────────────────────────────────────────────────────

L3FrequencyList::L3FrequencyList() {}

L3FrequencyList::L3FrequencyList(const std::vector<unsigned>& wARFCNs)
    : mARFCNs(wARFCNs) {}

unsigned L3FrequencyList::base() const {
    if (mARFCNs.empty()) return 0;
    unsigned retVal = mARFCNs[0];
    for (size_t i = 1; i < mARFCNs.size(); i++) {
        if (mARFCNs[i] < retVal) retVal = mARFCNs[i];
    }
    return retVal;
}

unsigned L3FrequencyList::spread() const {
    if (mARFCNs.empty()) return 0;
    unsigned maxVal = mARFCNs[0];
    for (size_t i = 1; i < mARFCNs.size(); i++) {
        if (mARFCNs[i] > maxVal) maxVal = mARFCNs[i];
    }
    return maxVal - base();
}

bool L3FrequencyList::contains(unsigned wARFCN) const {
    for (unsigned arfcn : mARFCNs) {
        if (arfcn == wARFCN) return true;
    }
    return false;
}

void L3FrequencyList::writeV(L3Frame& dest, size_t& wp) const {
    // GSM 04.08 10.5.2.13.7: variable bit map format
    dest.writeField(wp, 0x47, 7);  // header for variable bit map
    unsigned baseARFCN = base();
    dest.writeField(wp, baseARFCN, 10);
    // Bitmap: remaining bits in the fixed 16-byte V field = 128 - 17 = 111 bits
    unsigned numBits = 8 * static_cast<unsigned>(lengthV()) - 17;
    for (unsigned i = 0; i < numBits; i++) {
        unsigned thisARFCN = baseARFCN + 1 + i;
        dest.writeField(wp, contains(thisARFCN) ? 1 : 0, 1);
    }
}

void L3FrequencyList::parseV(const L3Frame& src, size_t& rp) {
    src.readField(rp, 7);  // skip header
    unsigned baseARFCN = src.readField(rp, 10);
    mARFCNs.clear();
    mARFCNs.push_back(baseARFCN);
    // Bitmap: remaining bits in the fixed 16-byte V field = 128 - 17 = 111 bits
    unsigned numBits = 8 * static_cast<unsigned>(lengthV()) - 17;
    for (unsigned i = 0; i < numBits; i++) {
        if (src.readField(rp, 1)) {
            mARFCNs.push_back(baseARFCN + 1 + i);
        }
    }
}

void L3FrequencyList::parseV(const L3Frame& src, size_t& rp, size_t) {
    parseV(src, rp);
}

void L3FrequencyList::text(std::ostream& os) const {
    os << "FreqList[";
    for (size_t i = 0; i < mARFCNs.size(); ++i) {
        if (i) os << ",";
        os << mARFCNs[i];
    }
    os << "]";
}

// ── L3MobileAllocation ─────────────────────────────────────────────────

L3MobileAllocation::L3MobileAllocation() {}

L3MobileAllocation::L3MobileAllocation(const std::vector<uint8_t>& wData)
    : mData(wData) {}

size_t L3MobileAllocation::lengthV() const {
    return mData.size();
}

void L3MobileAllocation::writeV(L3Frame& dest, size_t& wp) const {
    for (size_t i = 0; i < mData.size(); ++i) {
        dest.writeField(wp, mData[i], 8);
    }
}

void L3MobileAllocation::parseV(const L3Frame& src, size_t& rp) {
    size_t remaining = src.size() - rp;
    size_t nbytes = remaining / 8;
    mData.resize(nbytes);
    for (size_t i = 0; i < nbytes; ++i) {
        mData[i] = static_cast<uint8_t>(src.readField(rp, 8));
    }
}

void L3MobileAllocation::parseV(const L3Frame& src, size_t& rp, size_t expectedLength) {
    mData.resize(expectedLength);
    for (size_t i = 0; i < expectedLength; ++i) {
        mData[i] = static_cast<uint8_t>(src.readField(rp, 8));
    }
}

void L3MobileAllocation::text(std::ostream& os) const {
    os << "MobAlloc[" << mData.size() << "octets]";
    for (size_t i = 0; i < mData.size(); ++i) {
        os << " " << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << (int)mData[i];
    }
    os << std::dec;
}

// ── L3CellChannelDescription ───────────────────────────────────────────

L3CellChannelDescription::L3CellChannelDescription()
    : mARfcn(0), mBSIC(0), mChannelSpacing(0) {}

L3CellChannelDescription::L3CellChannelDescription(unsigned wARfcn, unsigned wBSIC, unsigned wSpacing)
    : mBSIC(wBSIC), mARfcn(wARfcn), mChannelSpacing(wSpacing) {}

void L3CellChannelDescription::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, mARfcn, 10);
    dest.writeField(wp, mBSIC, 6);
    dest.writeField(wp, mChannelSpacing, 1);
    dest.writeField(wp, 0, 1);  // spare
}

void L3CellChannelDescription::parseV(const L3Frame& src, size_t& rp) {
    mARfcn = src.readField(rp, 10);
    mBSIC = src.readField(rp, 6);
    mChannelSpacing = src.readField(rp, 1);
    src.readField(rp, 1);  // spare
}

void L3CellChannelDescription::parseV(const L3Frame& src, size_t& rp, size_t) {
    parseV(src, rp);
}

void L3CellChannelDescription::text(std::ostream& os) const {
    os << "CellChannel[ARfcn=" << mARfcn << " BSIC=" << mBSIC
       << " Spacing=" << mChannelSpacing << "]";
}

// ── L3ControlChannelDescription ────────────────────────────────────────

L3ControlChannelDescription::L3ControlChannelDescription()
    : mATT(0), mBS_AG_BLKS_RES(0), mCCCH_CONF(0), mBS_PA_MFRMS(0), mT3212(0) {}

void L3ControlChannelDescription::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, 0, 1);            // spare
    dest.writeField(wp, mATT, 1);
    dest.writeField(wp, mBS_AG_BLKS_RES, 3);
    dest.writeField(wp, mCCCH_CONF, 3);
    dest.writeField(wp, 0, 5);            // spare
    dest.writeField(wp, mBS_PA_MFRMS - 2, 3);
    dest.writeField(wp, mT3212, 8);
}

void L3ControlChannelDescription::parseV(const L3Frame& src, size_t& rp) {
    src.readField(rp, 1);                  // spare
    mATT = src.readField(rp, 1);
    mBS_AG_BLKS_RES = src.readField(rp, 3);
    mCCCH_CONF = src.readField(rp, 3);
    src.readField(rp, 5);                  // spare
    mBS_PA_MFRMS = src.readField(rp, 3) + 2;
    mT3212 = src.readField(rp, 8);
}

void L3ControlChannelDescription::parseV(const L3Frame& src, size_t& rp, size_t) {
    parseV(src, rp);
}

void L3ControlChannelDescription::text(std::ostream& os) const {
    os << "ControlChannel[ATT=" << mATT
        << " BS_AG_BLKS_RES=" << mBS_AG_BLKS_RES
        << " CCCH_CONF=" << mCCCH_CONF
        << " BS_PA_MFRMS=" << mBS_PA_MFRMS
        << " T3212=" << mT3212 << "]";
}

void L3ControlChannelDescription::validate() {
    if (mBS_PA_MFRMS < 2 || mBS_PA_MFRMS > 9) {
        mBS_PA_MFRMS = 2;
    }
    switch (mCCCH_CONF) {
        case 1:
            if (mBS_AG_BLKS_RES > 2) mBS_AG_BLKS_RES = 2;
            break;
        case 2:
        case 4:
        case 6:
            break;
        default:
            mCCCH_CONF = 1;
            break;
    }
}

unsigned countBeaconTimeslots(int ccch_conf) {
    switch (ccch_conf) {
        case 2: return 2;
        case 4: return 3;
        case 6: return 4;
        default: return 1;
    }
}

// ── L3ChannelDescription ───────────────────────────────────────────────

L3ChannelDescription::L3ChannelDescription()
    : mTypeAndOffset(TDMA_MISC), mTN(0), mTSC(0), mHFlag(0), mARFCN(0), mMAIO(0), mHSN(0) {}

L3ChannelDescription::L3ChannelDescription(TypeAndOffset wTypeAndOffset, unsigned wTN,
    unsigned wTSC, unsigned wARFCN)
    : mTypeAndOffset(wTypeAndOffset), mTN(wTN), mTSC(wTSC), mHFlag(0), mARFCN(wARFCN), mMAIO(0), mHSN(0) {}

void L3ChannelDescription::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, static_cast<unsigned>(mTypeAndOffset), 5);
    dest.writeField(wp, mTN, 3);
    dest.writeField(wp, mTSC, 3);
    dest.writeField(wp, mHFlag, 1);
    if (mHFlag) {
        dest.writeField(wp, mMAIO, 6);
        dest.writeField(wp, mHSN, 6);
    } else {
        dest.writeField(wp, 0, 2);  // spare
        dest.writeField(wp, mARFCN, 10);
    }
}

void L3ChannelDescription::parseV(const L3Frame& src, size_t& rp) {
    mTypeAndOffset = static_cast<TypeAndOffset>(src.readField(rp, 5));
    mTN = src.readField(rp, 3);
    mTSC = src.readField(rp, 3);
    mHFlag = src.readField(rp, 1);
    if (mHFlag) {
        mMAIO = src.readField(rp, 6);
        mHSN = src.readField(rp, 6);
    } else {
        src.readField(rp, 2);  // spare
        mARFCN = src.readField(rp, 10);
    }
}

void L3ChannelDescription::parseV(const L3Frame& src, size_t& rp, size_t) {
    parseV(src, rp);
}

void L3ChannelDescription::text(std::ostream& os) const {
    os << "Channel[TypeAndOffset=" << static_cast<int>(mTypeAndOffset)
        << " TN=" << mTN << " TSC=" << mTSC << " H=" << mHFlag;
    if (mHFlag) {
        os << " MAIO=" << mMAIO << " HSN=" << mHSN;
    } else {
        os << " ARFCN=" << mARFCN;
    }
    os << "]";
}

// ── L3PowerCommand ─────────────────────────────────────────────────────

L3PowerCommand::L3PowerCommand(unsigned wCommand) : mCommand(wCommand) {}

void L3PowerCommand::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, 0, 3);  // spare
    dest.writeField(wp, mCommand, 5);
}

void L3PowerCommand::parseV(const L3Frame& src, size_t& rp) {
    src.readField(rp, 3);  // spare
    mCommand = src.readField(rp, 5);
}

void L3PowerCommand::parseV(const L3Frame& src, size_t& rp, size_t) {
    parseV(src, rp);
}

void L3PowerCommand::text(std::ostream& os) const {
    os << "PowerCommand[" << mCommand << "]";
}

// ── L3CellOptions ──────────────────────────────────────────────────────

L3CellOptions::L3CellOptions()
    : mRevisionLevel(0), mCBCH(0), mEnhancedRACH(0), mCellReselectionPriority(0) {}

size_t L3CellOptions::lengthV() const {
    return mRawData.size();
}

void L3CellOptions::writeV(L3Frame& dest, size_t& wp) const {
    for (size_t i = 0; i < mRawData.size(); ++i) {
        dest.writeField(wp, mRawData[i], 8);
    }
}

void L3CellOptions::parseV(const L3Frame& src, size_t& rp) {
    throw ParseError("CellOptions requires expected length, use parseV with expectedLength");
}

void L3CellOptions::parseV(const L3Frame& src, size_t& rp, size_t expectedLength) {
    mRawData.clear();
    for (size_t i = 0; i < expectedLength; ++i) {
        mRawData.push_back(static_cast<uint8_t>(src.readField(rp, 8)));
    }
    if (mRawData.size() >= 1) {
        mRevisionLevel = (mRawData[0] >> 6) & 3;
        mCBCH = (mRawData[0] >> 5) & 1;
        mEnhancedRACH = (mRawData[0] >> 4) & 1;
    }
    if (mRawData.size() >= 2) {
        mCellReselectionPriority = mRawData[1] & 0x07;
    }
}

void L3CellOptions::text(std::ostream& os) const {
    os << "CellOptions[Rev=" << mRevisionLevel << " CBCH=" << mCBCH
       << " E-RACH=" << mEnhancedRACH << " CRO=" << mCellReselectionPriority << "]";
}

// ── L3CellSelection ────────────────────────────────────────────────────

L3CellSelection::L3CellSelection()
    : mRxLevAccessMin(0), mRxLevelAccessMin(0), mMaxRxLev(0),
      mCellReselectionHysteresis(0), mCellReselectionOffset(0),
      mCellReservedIndicator(0), mCellBarQualifierValue(0), mCellBarQualifierLength(0) {}

size_t L3CellSelection::lengthV() const {
    return 7 + (mCellBarQualifierLength + 1) / 2;
}

void L3CellSelection::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, mRxLevAccessMin, 5);
    dest.writeField(wp, mRxLevelAccessMin, 1);
    dest.writeField(wp, mMaxRxLev, 5);
    dest.writeField(wp, mCellReselectionHysteresis, 3);
    dest.writeField(wp, mCellReselectionOffset, 3);
    dest.writeField(wp, mCellReservedIndicator, 2);
    dest.writeField(wp, mCellBarQualifierValue, 4);
    dest.writeField(wp, mCellBarQualifierLength, 4);
    for (size_t i = 0; i < mCellBarQualifierData.size(); ++i) {
        dest.writeField(wp, mCellBarQualifierData[i], 8);
    }
}

void L3CellSelection::parseV(const L3Frame& src, size_t& rp) {
    mRxLevAccessMin = src.readField(rp, 5);
    mRxLevelAccessMin = src.readField(rp, 1);
    mMaxRxLev = src.readField(rp, 5);
    mCellReselectionHysteresis = src.readField(rp, 3);
    mCellReselectionOffset = src.readField(rp, 3);
    mCellReservedIndicator = src.readField(rp, 2);
    mCellBarQualifierValue = src.readField(rp, 4);
    mCellBarQualifierLength = src.readField(rp, 4);
    mCellBarQualifierData.clear();
    size_t bytes = (mCellBarQualifierLength + 1) / 2;
    for (size_t i = 0; i < bytes; ++i) {
        mCellBarQualifierData.push_back(static_cast<unsigned char>(src.readField(rp, 8)));
    }
}

void L3CellSelection::parseV(const L3Frame& src, size_t& rp, size_t) {
    parseV(src, rp);
}

void L3CellSelection::text(std::ostream& os) const {
    os << "CellSelection[RxMin=" << mRxLevAccessMin
        << " MaxRx=" << mMaxRxLev
        << " Hyst=" << mCellReselectionHysteresis
        << " Offset=" << mCellReselectionOffset
        << " CBI=" << mCellReservedIndicator
        << " CBQ=" << mCellBarQualifierValue
        << " CBQLen=" << mCellBarQualifierLength << "]";
}

// ── L3RACHControlParameters ────────────────────────────────────────────

L3RACHControlParameters::L3RACHControlParameters()
    : mMaxRetrans(0), mTxInteger(0), mCellBarAccess(0), mRE(0), mAC(0) {}

void L3RACHControlParameters::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, mMaxRetrans, 2);
    dest.writeField(wp, mTxInteger, 4);
    dest.writeField(wp, mCellBarAccess, 1);
    dest.writeField(wp, mRE, 1);
    dest.writeField(wp, mAC, 16);
}

void L3RACHControlParameters::parseV(const L3Frame& src, size_t& rp) {
    mMaxRetrans = src.readField(rp, 2);
    mTxInteger = src.readField(rp, 4);
    mCellBarAccess = src.readField(rp, 1);
    mRE = src.readField(rp, 1);
    mAC = src.readField(rp, 16);
}

void L3RACHControlParameters::parseV(const L3Frame& src, size_t& rp, size_t) {
    parseV(src, rp);
}

void L3RACHControlParameters::text(std::ostream& os) const {
    os << "RACHControl[MaxRetrans=" << mMaxRetrans
        << " TxInteger=" << mTxInteger
        << " CellBarAccess=" << mCellBarAccess
        << " RE=" << mRE
        << " AC=" << mAC << "]";
}

// ── L3ImmediateAssignmentInformation ───────────────────────────────────

L3ImmediateAssignmentInformation::L3ImmediateAssignmentInformation()
    : mPowerOffset(0), mPowerOffsetLength(0) {}

size_t L3ImmediateAssignmentInformation::lengthV() const {
    return 3 + mPowerOffsetLength;
}

void L3ImmediateAssignmentInformation::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, mPowerOffset, 5);
    dest.writeField(wp, mPowerOffsetLength, 3);
    for (size_t i = 0; i < mPowerOffsetData.size(); ++i) {
        dest.writeField(wp, mPowerOffsetData[i], 8);
    }
}

void L3ImmediateAssignmentInformation::parseV(const L3Frame& src, size_t& rp) {
    mPowerOffset = src.readField(rp, 5);
    mPowerOffsetLength = src.readField(rp, 3);
    mPowerOffsetData.clear();
    for (size_t i = 0; i < mPowerOffsetLength; ++i) {
        mPowerOffsetData.push_back(static_cast<unsigned char>(src.readField(rp, 8)));
    }
}

void L3ImmediateAssignmentInformation::parseV(const L3Frame& src, size_t& rp, size_t expectedLength) {
    mPowerOffset = src.readField(rp, 5);
    mPowerOffsetLength = src.readField(rp, 3);
    mPowerOffsetData.clear();
    for (size_t i = 0; i < mPowerOffsetLength && i < expectedLength - 3; ++i) {
        mPowerOffsetData.push_back(static_cast<unsigned char>(src.readField(rp, 8)));
    }
}

void L3ImmediateAssignmentInformation::text(std::ostream& os) const {
    os << "IAInfo[PO=" << mPowerOffset << " POLen=" << mPowerOffsetLength << "]";
}

// ── L3AdditionalChannelDescription ─────────────────────────────────────

L3AdditionalChannelDescription::L3AdditionalChannelDescription()
    : mTypeAndOffset(TDMA_MISC), mTN(0), mTSC(0), mHFlag(0), mARFCN(0), mMAIO(0), mHSN(0) {}

L3AdditionalChannelDescription::L3AdditionalChannelDescription(TypeAndOffset wTypeAndOffset, unsigned wTN,
    unsigned wTSC, unsigned wARFCN)
    : mTypeAndOffset(wTypeAndOffset), mTN(wTN), mTSC(wTSC), mHFlag(0), mARFCN(wARFCN), mMAIO(0), mHSN(0) {}

void L3AdditionalChannelDescription::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, static_cast<unsigned>(mTypeAndOffset), 5);
    dest.writeField(wp, mTN, 3);
    dest.writeField(wp, mTSC, 3);
    dest.writeField(wp, mHFlag, 1);
    if (mHFlag) {
        dest.writeField(wp, mMAIO, 6);
        dest.writeField(wp, mHSN, 6);
    } else {
        dest.writeField(wp, 0, 2);  // spare
        dest.writeField(wp, mARFCN, 10);
    }
}

void L3AdditionalChannelDescription::parseV(const L3Frame& src, size_t& rp) {
    mTypeAndOffset = static_cast<TypeAndOffset>(src.readField(rp, 5));
    mTN = src.readField(rp, 3);
    mTSC = src.readField(rp, 3);
    mHFlag = src.readField(rp, 1);
    if (mHFlag) {
        mMAIO = src.readField(rp, 6);
        mHSN = src.readField(rp, 6);
    } else {
        src.readField(rp, 2);  // spare
        mARFCN = src.readField(rp, 10);
    }
}

void L3AdditionalChannelDescription::parseV(const L3Frame& src, size_t& rp, size_t) {
    parseV(src, rp);
}

void L3AdditionalChannelDescription::text(std::ostream& os) const {
    os << "AddlChannel[TypeAndOffset=" << static_cast<int>(mTypeAndOffset)
        << " TN=" << mTN << " TSC=" << mTSC << " H=" << mHFlag;
    if (mHFlag) {
        os << " MAIO=" << mMAIO << " HSN=" << mHSN;
    } else {
        os << " ARFCN=" << mARFCN;
    }
    os << "]";
}

// ── L3ChannelDescription2 ──────────────────────────────────────────────

L3ChannelDescription2::L3ChannelDescription2()
    : L3ChannelDescription() {}

L3ChannelDescription2::L3ChannelDescription2(TypeAndOffset wTypeAndOffset, unsigned wTN,
    unsigned wTSC, unsigned wARFCN)
    : L3ChannelDescription(wTypeAndOffset, wTN, wTSC, wARFCN) {}

L3ChannelDescription2::L3ChannelDescription2(const L3ChannelDescription& other)
    : L3ChannelDescription(other.typeAndOffset(), other.TN(), other.TSC(), other.ARFCN()) {}

// ── L3PowerCommandAndAccessType ────────────────────────────────────────

L3PowerCommandAndAccessType::L3PowerCommandAndAccessType(unsigned wCommand)
    : L3PowerCommand(wCommand) {}

// ── L3ChannelMode ───────────────────────────────────────────────────────

L3ChannelMode::L3ChannelMode(Mode wMode) : mMode(wMode) {}

void L3ChannelMode::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, static_cast<unsigned>(mMode), 8);
}

void L3ChannelMode::parseV(const L3Frame& src, size_t& rp) {
    mMode = static_cast<Mode>(src.readField(rp, 8));
}

void L3ChannelMode::parseV(const L3Frame& src, size_t& rp, size_t) {
    parseV(src, rp);
}

void L3ChannelMode::text(std::ostream& os) const {
    os << "ChannelMode[" << static_cast<int>(mMode) << "]";
}

// ── L3TimingAdvance ────────────────────────────────────────────────────

L3TimingAdvance::L3TimingAdvance(unsigned wTA) : mTimingAdvance(wTA) {}

void L3TimingAdvance::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, 0, 2);  // spare
    dest.writeField(wp, mTimingAdvance, 6);
}

void L3TimingAdvance::parseV(const L3Frame& src, size_t& rp) {
    src.readField(rp, 2);  // spare
    mTimingAdvance = src.readField(rp, 6);
}

void L3TimingAdvance::parseV(const L3Frame& src, size_t& rp, size_t) {
    parseV(src, rp);
}

void L3TimingAdvance::text(std::ostream& os) const {
    os << "TimingAdvance[" << mTimingAdvance << "]";
}

// ── L3CellDescription ──────────────────────────────────────────────────

L3CellDescription::L3CellDescription(unsigned wARFCN, unsigned wNCC, unsigned wBCC)
    : mARFCN(wARFCN), mNCC(wNCC), mBCC(wBCC) {}

void L3CellDescription::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, mARFCN >> 8, 2);
    dest.writeField(wp, mNCC, 3);
    dest.writeField(wp, mBCC, 3);
    dest.writeField(wp, mARFCN & 0xff, 8);
}

void L3CellDescription::parseV(const L3Frame& src, size_t& rp) {
    unsigned arfcnHigh = src.readField(rp, 2);
    mNCC = src.readField(rp, 3);
    mBCC = src.readField(rp, 3);
    mARFCN = src.readField(rp, 8) + (arfcnHigh << 8);
}

void L3CellDescription::parseV(const L3Frame& src, size_t& rp, size_t) {
    parseV(src, rp);
}

void L3CellDescription::text(std::ostream& os) const {
    os << "CellDesc[ARFCN=" << mARFCN << " NCC=" << mNCC << " BCC=" << mBCC << "]";
}

// ── L3HandoverReference ────────────────────────────────────────────────

L3HandoverReference::L3HandoverReference(unsigned wValue) : mValue(wValue) {}

void L3HandoverReference::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, mValue, 8);
}

void L3HandoverReference::parseV(const L3Frame& src, size_t& rp) {
    mValue = src.readField(rp, 8);
}

void L3HandoverReference::parseV(const L3Frame& src, size_t& rp, size_t) {
    parseV(src, rp);
}

void L3HandoverReference::text(std::ostream& os) const {
    os << "HORef[" << mValue << "]";
}

// ── L3CipheringModeSetting ─────────────────────────────────────────────

L3CipheringModeSetting::L3CipheringModeSetting(bool wCiphering, int wAlgorithm)
    : mCiphering(wCiphering), mAlgorithm(wAlgorithm) {}

void L3CipheringModeSetting::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, mCiphering ? mAlgorithm - 1 : 0, 3);
    dest.writeField(wp, mCiphering ? 1 : 0, 1);
}

void L3CipheringModeSetting::parseV(const L3Frame& src, size_t& rp) {
    mAlgorithm = src.readField(rp, 3) + 1;
    mCiphering = src.readField(rp, 1);
}

void L3CipheringModeSetting::parseV(const L3Frame& src, size_t& rp, size_t) {
    mAlgorithm = src.readField(rp, 3) + 1;
    mCiphering = src.readField(rp, 1);
}

void L3CipheringModeSetting::text(std::ostream& os) const {
    os << "CipherMode[cipher=" << mCiphering << " algo=A5/" << mAlgorithm << "]";
}

// ── L3CipheringModeResponse ────────────────────────────────────────────

L3CipheringModeResponse::L3CipheringModeResponse(bool wIncludeIMEISV)
    : mIncludeIMEISV(wIncludeIMEISV) {}

void L3CipheringModeResponse::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, 0, 3);
    dest.writeField(wp, mIncludeIMEISV ? 1 : 0, 1);
}

void L3CipheringModeResponse::parseV(const L3Frame& src, size_t& rp) {
    src.readField(rp, 3);
    mIncludeIMEISV = src.readField(rp, 1);
}

void L3CipheringModeResponse::parseV(const L3Frame& src, size_t& rp, size_t) {
    src.readField(rp, 3);
    mIncludeIMEISV = src.readField(rp, 1);
}

void L3CipheringModeResponse::text(std::ostream& os) const {
    os << "CipherResp[IMEISV=" << mIncludeIMEISV << "]";
}

// ── L3SynchronizationIndication ────────────────────────────────────────

L3SynchronizationIndication::L3SynchronizationIndication(bool wNCI, bool wROT, int wSI)
    : mNCI(wNCI), mROT(wROT), mSI(wSI & 3) {}

void L3SynchronizationIndication::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, 0xD, 4);
    dest.writeField(wp, mNCI ? 1 : 0, 1);
    dest.writeField(wp, mROT ? 1 : 0, 1);
    dest.writeField(wp, mSI, 2);
}

void L3SynchronizationIndication::parseV(const L3Frame& src, size_t& rp) {
    src.readField(rp, 4);
    mNCI = src.readField(rp, 1);
    mROT = src.readField(rp, 1);
    mSI = src.readField(rp, 2);
}

void L3SynchronizationIndication::parseV(const L3Frame& src, size_t& rp, size_t) {
    parseV(src, rp);
}

void L3SynchronizationIndication::text(std::ostream& os) const {
    os << "SyncInd[NCI=" << mNCI << " ROT=" << mROT << " SI=" << mSI << "]";
}

// ── L3NCCPermitted ─────────────────────────────────────────────────────

L3NCCPermitted::L3NCCPermitted(unsigned wPermitted) : mPermitted(wPermitted) {}

void L3NCCPermitted::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, mPermitted, 8);
}

void L3NCCPermitted::parseV(const L3Frame& src, size_t& rp) {
    mPermitted = src.readField(rp, 8);
}

void L3NCCPermitted::parseV(const L3Frame& src, size_t& rp, size_t) {
    parseV(src, rp);
}

void L3NCCPermitted::text(std::ostream& os) const {
    os << "NCCPermitted[0x" << std::hex << std::setw(2) << std::setfill('0')
       << static_cast<int>(mPermitted) << "]";
}

// ── L3PageMode ─────────────────────────────────────────────────────────

L3PageMode::L3PageMode(unsigned wPageMode) : mPageMode(wPageMode) {}

void L3PageMode::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, 0, 2);  // spare
    dest.writeField(wp, mPageMode, 2);
}

void L3PageMode::parseV(const L3Frame& src, size_t& rp) {
    src.readField(rp, 2);  // spare
    mPageMode = src.readField(rp, 2);
}

void L3PageMode::parseV(const L3Frame& src, size_t& rp, size_t) {
    src.readField(rp, 2);  // spare
    mPageMode = src.readField(rp, 2);
}

void L3PageMode::text(std::ostream& os) const {
    os << "PageMode[" << mPageMode << "]";
}

// ── L3RequestReference ─────────────────────────────────────────────────

L3RequestReference::L3RequestReference()
    : mRA(0), mT1p(0), mT2(0), mT3(0) {}

L3RequestReference::L3RequestReference(unsigned wRA, unsigned wT1p, unsigned wT2, unsigned wT3)
    : mRA(wRA), mT1p(wT1p), mT2(wT2), mT3(wT3) {}

void L3RequestReference::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, mRA, 8);
    dest.writeField(wp, mT1p, 5);
    dest.writeField(wp, mT3, 6);
    dest.writeField(wp, mT2, 5);
}

void L3RequestReference::parseV(const L3Frame& src, size_t& rp) {
    mRA = src.readField(rp, 8);
    mT1p = src.readField(rp, 5);
    mT3 = src.readField(rp, 6);
    mT2 = src.readField(rp, 5);
}

void L3RequestReference::parseV(const L3Frame& src, size_t& rp, size_t) {
    parseV(src, rp);
}

void L3RequestReference::text(std::ostream& os) const {
    os << "ReqRef[RA=" << mRA << " T1=" << mT1p << " T2=" << mT2 << " T3=" << mT3 << "]";
}

// ── L3WaitIndication ───────────────────────────────────────────────────

L3WaitIndication::L3WaitIndication(unsigned seconds) : mValue(seconds) {}

void L3WaitIndication::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, mValue, 8);
}

void L3WaitIndication::parseV(const L3Frame& src, size_t& rp) {
    mValue = src.readField(rp, 8);
}

void L3WaitIndication::parseV(const L3Frame& src, size_t& rp, size_t) {
    parseV(src, rp);
}

void L3WaitIndication::text(std::ostream& os) const {
    os << "WaitInd[" << mValue << "s]";
}

// ── L3RRCauseElement ───────────────────────────────────────────────────

L3RRCauseElement::L3RRCauseElement(RRCause wValue) : mCauseValue(wValue) {}

void L3RRCauseElement::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, static_cast<unsigned>(mCauseValue), 8);
}

void L3RRCauseElement::parseV(const L3Frame& src, size_t& rp) {
    mCauseValue = static_cast<RRCause>(src.readField(rp, 8));
}

void L3RRCauseElement::parseV(const L3Frame& src, size_t& rp, size_t) {
    parseV(src, rp);
}

void L3RRCauseElement::text(std::ostream& os) const {
    os << "RRCause[" << RRCause2Str(mCauseValue) << "]";
}

// ── L3CellOptionsBCCH ──────────────────────────────────────────────────

L3CellOptionsBCCH::L3CellOptionsBCCH()
    : mPWRC(0), mDTX(2), mRADIO_LINK_TIMEOUT(1) {}

void L3CellOptionsBCCH::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, 0, 1);
    dest.writeField(wp, mPWRC, 1);
    dest.writeField(wp, mDTX, 2);
    dest.writeField(wp, mRADIO_LINK_TIMEOUT, 4);
}

void L3CellOptionsBCCH::parseV(const L3Frame& src, size_t& rp) {
    src.readField(rp, 1);
    mPWRC = src.readField(rp, 1);
    mDTX = src.readField(rp, 2);
    mRADIO_LINK_TIMEOUT = src.readField(rp, 4);
}

void L3CellOptionsBCCH::parseV(const L3Frame& src, size_t& rp, size_t) {
    parseV(src, rp);
}

void L3CellOptionsBCCH::text(std::ostream& os) const {
    os << "CellOptsBCCH[PWRC=" << mPWRC << " DTX=" << mDTX
       << " RLT=" << mRADIO_LINK_TIMEOUT << "]";
}

// ── L3CellOptionsSACCH ─────────────────────────────────────────────────

L3CellOptionsSACCH::L3CellOptionsSACCH()
    : mPWRC(0), mDTX(2), mRADIO_LINK_TIMEOUT(1) {}

void L3CellOptionsSACCH::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, (mDTX >> 2) & 0x01, 1);
    dest.writeField(wp, mPWRC, 1);
    dest.writeField(wp, mDTX & 0x03, 2);
    dest.writeField(wp, mRADIO_LINK_TIMEOUT, 4);
}

void L3CellOptionsSACCH::parseV(const L3Frame& src, size_t& rp) {
    mDTX = src.readField(rp, 1) << 2;
    mPWRC = src.readField(rp, 1);
    mDTX |= src.readField(rp, 2);
    mRADIO_LINK_TIMEOUT = src.readField(rp, 4);
}

void L3CellOptionsSACCH::parseV(const L3Frame& src, size_t& rp, size_t) {
    parseV(src, rp);
}

void L3CellOptionsSACCH::text(std::ostream& os) const {
    os << "CellOptsSACCH[PWRC=" << mPWRC << " DTX=" << mDTX
       << " RLT=" << mRADIO_LINK_TIMEOUT << "]";
}

// ── L3CellSelectionParameters ──────────────────────────────────────────

L3CellSelectionParameters::L3CellSelectionParameters()
    : mACS(0), mNECI(0), mCELL_RESELECT_HYSTERESIS(0),
      mMS_TXPWR_MAX_CCH(0), mRXLEV_ACCESS_MIN(0) {}

void L3CellSelectionParameters::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, mCELL_RESELECT_HYSTERESIS, 3);
    dest.writeField(wp, mMS_TXPWR_MAX_CCH, 5);
    dest.writeField(wp, mACS, 1);
    dest.writeField(wp, mNECI, 1);
    dest.writeField(wp, mRXLEV_ACCESS_MIN, 6);
}

void L3CellSelectionParameters::parseV(const L3Frame& src, size_t& rp) {
    mCELL_RESELECT_HYSTERESIS = src.readField(rp, 3);
    mMS_TXPWR_MAX_CCH = src.readField(rp, 5);
    mACS = src.readField(rp, 1);
    mNECI = src.readField(rp, 1);
    mRXLEV_ACCESS_MIN = src.readField(rp, 6);
}

void L3CellSelectionParameters::parseV(const L3Frame& src, size_t& rp, size_t) {
    parseV(src, rp);
}

void L3CellSelectionParameters::text(std::ostream& os) const {
    os << "CellSelParams[ACS=" << mACS << " NECI=" << mNECI
       << " Hyst=" << mCELL_RESELECT_HYSTERESIS
       << " TXPWR=" << mMS_TXPWR_MAX_CCH
       << " RXLEV=" << mRXLEV_ACCESS_MIN << "]";
}

// ── L3BCCHFrequencyList ────────────────────────────────────────────────

L3BCCHFrequencyList::L3BCCHFrequencyList() {}

L3BCCHFrequencyList::L3BCCHFrequencyList(const std::vector<unsigned>& wARFCNs)
    : L3FrequencyList(wARFCNs) {}

void L3BCCHFrequencyList::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, 0, 3);  // BA-IND, EXT-IND, Spare
    L3FrequencyList::writeV(dest, wp);
}

void L3BCCHFrequencyList::text(std::ostream& os) const {
    os << "BCCHFreqList[";
    for (size_t i = 0; i < mARFCNs.size(); ++i) {
        if (i) os << ",";
        os << mARFCNs[i];
    }
    os << "]";
}

// ── L3NeighborCellsDescription ─────────────────────────────────────────

L3NeighborCellsDescription::L3NeighborCellsDescription() {}

L3NeighborCellsDescription::L3NeighborCellsDescription(const std::vector<unsigned>& neighbors)
    : L3FrequencyList(neighbors) {}

void L3NeighborCellsDescription::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, 0, 3);  // BA-IND, EXT-IND, Spare
    L3FrequencyList::writeV(dest, wp);
}

void L3NeighborCellsDescription::text(std::ostream& os) const {
    os << "NeighborCells[";
    for (size_t i = 0; i < mARFCNs.size(); ++i) {
        if (i) os << ",";
        os << mARFCNs[i];
    }
    os << "]";
}

// ── L3MeasurementResults ───────────────────────────────────────────────

L3MeasurementResults::L3MeasurementResults()
    : mBA_USED(false), mDTX_USED(false), mMEAS_VALID(false),
      mRXLEV_FULL_SERVING_CELL(0), mRXLEV_SUB_SERVING_CELL(0),
      mRXQUAL_FULL_SERVING_CELL(0), mRXQUAL_SUB_SERVING_CELL(0),
      mNO_NCELL(0) {
    memset(mRXLEV_NCELL, 0, sizeof(mRXLEV_NCELL));
    memset(mBCCH_FREQ_NCELL, 0, sizeof(mBCCH_FREQ_NCELL));
    memset(mBSIC_NCELL, 0, sizeof(mBSIC_NCELL));
}

int L3MeasurementResults::decodeLevToDBm(unsigned lev) const {
    if (lev == 0) return -110;
    if (lev >= 63) return -48;
    return -110 + (int)lev;
}

float L3MeasurementResults::decodeQualToBER(unsigned qual) const {
    static const float berTable[] = {
        0.0f, 0.002f, 0.004f, 0.008f, 0.016f, 0.032f, 0.08f, 0.16f, -1.0f
    };
    if (qual >= 8) return -1.0f;
    return berTable[qual];
}

void L3MeasurementResults::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, mBA_USED ? 1 : 0, 1);
    dest.writeField(wp, mDTX_USED ? 1 : 0, 1);
    dest.writeField(wp, mRXLEV_FULL_SERVING_CELL, 6);
    dest.writeField(wp, 0, 1);  // spare
    dest.writeField(wp, mMEAS_VALID ? 1 : 0, 1);
    dest.writeField(wp, mRXLEV_SUB_SERVING_CELL, 6);
    dest.writeField(wp, 0, 1);  // spare
    dest.writeField(wp, mRXQUAL_FULL_SERVING_CELL, 3);
    dest.writeField(wp, mRXQUAL_SUB_SERVING_CELL, 3);
    dest.writeField(wp, mNO_NCELL, 3);
    for (unsigned i = 0; i < mNO_NCELL && i < 6; ++i) {
        dest.writeField(wp, mRXLEV_NCELL[i], 6);
        dest.writeField(wp, mBCCH_FREQ_NCELL[i], 5);
        dest.writeField(wp, mBSIC_NCELL[i], 6);
    }
}

void L3MeasurementResults::parseV(const L3Frame& src, size_t& rp) {
    mBA_USED = src.readField(rp, 1);
    mDTX_USED = src.readField(rp, 1);
    mRXLEV_FULL_SERVING_CELL = src.readField(rp, 6);
    src.readField(rp, 1);  // spare
    mMEAS_VALID = src.readField(rp, 1);
    mRXLEV_SUB_SERVING_CELL = src.readField(rp, 6);
    src.readField(rp, 1);  // spare
    mRXQUAL_FULL_SERVING_CELL = src.readField(rp, 3);
    mRXQUAL_SUB_SERVING_CELL = src.readField(rp, 3);
    mNO_NCELL = src.readField(rp, 3);
    for (unsigned i = 0; i < mNO_NCELL && i < 6; ++i) {
        mRXLEV_NCELL[i] = src.readField(rp, 6);
        mBCCH_FREQ_NCELL[i] = src.readField(rp, 5);
        mBSIC_NCELL[i] = src.readField(rp, 6);
    }
}

void L3MeasurementResults::parseV(const L3Frame& src, size_t& rp, size_t) {
    parseV(src, rp);
}

void L3MeasurementResults::text(std::ostream& os) const {
    os << "MeasResults[BA=" << mBA_USED << " DTX=" << mDTX_USED
       << " RXLEV=" << mRXLEV_FULL_SERVING_CELL
       << " RXQUAL=" << mRXQUAL_FULL_SERVING_CELL
       << " NCELL=" << mNO_NCELL << "]";
}

// ── L3MultiRateConfiguration ───────────────────────────────────────────

L3MultiRateConfiguration::L3MultiRateConfiguration(bool halfrate)
    : mOptions(0x20), mAmrCodecSet(halfrate ? codec_set_AMR_HR : codec_set_AMR_FR) {}

void L3MultiRateConfiguration::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, mOptions, 8);
    dest.writeField(wp, static_cast<unsigned>(mAmrCodecSet), 8);
}

void L3MultiRateConfiguration::parseV(const L3Frame& src, size_t& rp) {
    mOptions = src.readField(rp, 8);
    mAmrCodecSet = static_cast<AmrCodecSet>(src.readField(rp, 8));
}

void L3MultiRateConfiguration::parseV(const L3Frame& src, size_t& rp, size_t) {
    parseV(src, rp);
}

void L3MultiRateConfiguration::text(std::ostream& os) const {
    os << "MultiRate[opts=0x" << std::hex << std::setw(2) << std::setfill('0')
       << static_cast<int>(mOptions)
       << " codec=0x" << std::setw(2) << std::setfill('0')
       << static_cast<int>(mAmrCodecSet) << "]";
}

// ── L3APDUID ───────────────────────────────────────────────────────────

L3APDUID::L3APDUID(unsigned protocolIdentifier)
    : mProtocolIdentifier(protocolIdentifier) {}

void L3APDUID::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, mProtocolIdentifier, 4);
}

void L3APDUID::parseV(const L3Frame& src, size_t& rp) {
    mProtocolIdentifier = src.readField(rp, 4);
}

void L3APDUID::parseV(const L3Frame& src, size_t& rp, size_t) {
    mProtocolIdentifier = src.readField(rp, 4);
    src.readField(rp, 4);  // spare
}

void L3APDUID::text(std::ostream& os) const {
    os << "APDUID[" << mProtocolIdentifier << "]";
}

// ── L3APDUFlags ────────────────────────────────────────────────────────

L3APDUFlags::L3APDUFlags(unsigned cr, unsigned firstSegment, unsigned lastSegment)
    : mCR(cr), mFirstSegment(firstSegment), mLastSegment(lastSegment) {}

void L3APDUFlags::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, 0, 1);  // spare
    dest.writeField(wp, mCR, 1);
    dest.writeField(wp, mFirstSegment, 1);
    dest.writeField(wp, mLastSegment, 1);
}

void L3APDUFlags::parseV(const L3Frame& src, size_t& rp) {
    src.readField(rp, 1);  // spare
    mCR = src.readField(rp, 1);
    mFirstSegment = src.readField(rp, 1);
    mLastSegment = src.readField(rp, 1);
}

void L3APDUFlags::parseV(const L3Frame& src, size_t& rp, size_t) {
    parseV(src, rp);
}

void L3APDUFlags::text(std::ostream& os) const {
    os << "APDUFlags[CR=" << mCR << " first=" << mFirstSegment
       << " last=" << mLastSegment << "]";
}

// ── L3APDUData ─────────────────────────────────────────────────────────

L3APDUData::L3APDUData() {}

L3APDUData::L3APDUData(BitVector data) : mData(std::move(data)) {}

size_t L3APDUData::lengthV() const {
    return (mData.size() + 7) / 8;
}

void L3APDUData::writeV(L3Frame& dest, size_t& wp) const {
    size_t wp2 = 0;
    for (size_t i = 0; i < mData.size(); ++i) {
        unsigned bit = mData.readField(wp2, 1);
        dest.writeField(wp, bit, 1);
    }
}

void L3APDUData::parseV(const L3Frame& src, size_t& rp) {
    size_t remaining = src.size() - rp;
    mData = BitVector(remaining);
    size_t wp2 = 0;
    for (size_t i = 0; i < remaining; ++i) {
        unsigned bit = src.readField(rp, 1);
        mData.writeField(wp2, bit, 1);
    }
}

void L3APDUData::parseV(const L3Frame& src, size_t& rp, size_t expectedLength) {
    size_t nbits = 8 * expectedLength;
    mData = BitVector(nbits);
    size_t wp2 = 0;
    for (size_t i = 0; i < nbits; ++i) {
        unsigned bit = src.readField(rp, 1);
        mData.writeField(wp2, bit, 1);
    }
}

void L3APDUData::text(std::ostream& os) const {
    os << "APDUData[" << mData.size() << "bits]";
}

// ── L3DedicatedModeOrTBF ───────────────────────────────────────────────

L3DedicatedModeOrTBF::L3DedicatedModeOrTBF(bool forTBF, bool wDownlink)
    : mDownlink(wDownlink), mTMA(0), mDMOrTBF(forTBF) {}

void L3DedicatedModeOrTBF::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, 0, 1);  // spare
    dest.writeField(wp, mTMA, 1);
    dest.writeField(wp, mDownlink, 1);
    dest.writeField(wp, mDMOrTBF, 1);
}

void L3DedicatedModeOrTBF::parseV(const L3Frame& src, size_t& rp) {
    src.readField(rp, 1);  // spare
    mTMA = src.readField(rp, 1);
    mDownlink = src.readField(rp, 1);
    mDMOrTBF = src.readField(rp, 1);
}

void L3DedicatedModeOrTBF::parseV(const L3Frame& src, size_t& rp, size_t) {
    parseV(src, rp);
}

void L3DedicatedModeOrTBF::text(std::ostream& os) const {
    os << "DedModeOrTBF[DL=" << mDownlink << " TBF=" << mDMOrTBF << "]";
}

// ── L3FollowOnProceed ──────────────────────────────────────────────────

void L3FollowOnProceed::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, 0xa1, 8);
}

void L3FollowOnProceed::parseV(const L3Frame& src, size_t& rp) {
    src.readField(rp, 8);
}

void L3FollowOnProceed::parseV(const L3Frame& src, size_t& rp, size_t) {
    parseV(src, rp);
}

// ── L3SI3RestOctets ────────────────────────────────────────────────────

L3SI3RestOctets::L3SI3RestOctets()
    : mHaveSI3RestOctets(false), mHaveSelectionParameters(false),
      mCBQ(false), mCELL_RESELECT_OFFSET(0), mTEMPORARY_OFFSET(0),
      mPENALTY_TIME(0), mRA_COLOUR(0), mHaveGPRS(false) {}

size_t L3SI3RestOctets::lengthV() const {
    if (!mHaveSI3RestOctets) return 0;
    int bits = 1;
    if (mHaveSelectionParameters) bits += 1 + 1 + 6 + 3 + 5;
    else bits += 1;
    bits += 4;
    if (mHaveGPRS) bits += 1 + 3 + 1;
    else bits += 1;
    // Round up to byte boundary
    return (bits + 7) / 8;
}

void L3SI3RestOctets::writeV(L3Frame& dest, size_t& wp) const {
    if (!mHaveSI3RestOctets) return;
    dest.writeField(wp, 1, 1);
    if (mHaveSelectionParameters) {
        dest.writeField(wp, 1, 1);
        dest.writeField(wp, mCBQ ? 1 : 0, 1);
        dest.writeField(wp, mCELL_RESELECT_OFFSET, 6);
        dest.writeField(wp, mTEMPORARY_OFFSET, 3);
        dest.writeField(wp, mPENALTY_TIME, 5);
    } else {
        dest.writeField(wp, 0, 1);
    }
    dest.writeField(wp, 0, 4);
    if (mHaveGPRS) {
        dest.writeField(wp, 1, 1);
        dest.writeField(wp, mRA_COLOUR, 3);
        dest.writeField(wp, 0, 1);
    } else {
        dest.writeField(wp, 0, 1);
    }
    // Pad to byte boundary with H/L pattern
    while (wp % 8 != 0) {
        unsigned fillBit = (wp % 8 == 1 || wp % 8 == 3 || wp % 8 == 6 || wp % 8 == 7) ? 1 : 0;
        dest.writeField(wp, fillBit, 1);
    }
}

void L3SI3RestOctets::parseV(const L3Frame& src, size_t& rp) {
    if (rp >= src.size()) return;
    unsigned bit = src.readField(rp, 1);
    if (!bit) return;
    mHaveSI3RestOctets = true;
    unsigned selBit = src.readField(rp, 1);
    if (selBit) {
        mHaveSelectionParameters = true;
        mCBQ = src.readField(rp, 1);
        mCELL_RESELECT_OFFSET = src.readField(rp, 6);
        mTEMPORARY_OFFSET = src.readField(rp, 3);
        mPENALTY_TIME = src.readField(rp, 5);
    }
    src.readField(rp, 4);
    if (rp + 1 <= src.size()) {
        unsigned gprsBit = src.readField(rp, 1);
        if (gprsBit) {
            mHaveGPRS = true;
            mRA_COLOUR = src.readField(rp, 3);
            src.readField(rp, 1);
        }
    }
}

void L3SI3RestOctets::parseV(const L3Frame& src, size_t& rp, size_t) {
    parseV(src, rp);
}

void L3SI3RestOctets::text(std::ostream& os) const {
    os << "SI3RestOctets";
    if (mHaveSelectionParameters) {
        os << " CBQ=" << mCBQ
           << " CRO=" << mCELL_RESELECT_OFFSET
           << " TO=" << mTEMPORARY_OFFSET
           << " PT=" << mPENALTY_TIME;
    }
    if (mHaveGPRS) os << " GPRS RA_COLOUR=" << mRA_COLOUR;
}

// ── L3SIType4RestOctets ────────────────────────────────────────────────

L3SIType4RestOctets::L3SIType4RestOctets()
    : mRA_COLOUR(0), mHaveGPRS(false) {}

size_t L3SIType4RestOctets::lengthV() const {
    int bits = 1 + 1;
    if (mHaveGPRS) bits += 1 + 3 + 1;
    else bits += 1;
    bits += 2;
    // Round up to byte boundary
    return (bits + 7) / 8;
}

void L3SIType4RestOctets::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, 0, 1);
    dest.writeField(wp, 0, 1);
    if (mHaveGPRS) {
        dest.writeField(wp, 1, 1);
        dest.writeField(wp, mRA_COLOUR, 3);
        dest.writeField(wp, 0, 1);
    } else {
        dest.writeField(wp, 0, 1);
    }
    dest.writeField(wp, 0, 2);
    // Pad to byte boundary with H/L pattern
    while (wp % 8 != 0) {
        unsigned fillBit = (wp % 8 == 1 || wp % 8 == 3 || wp % 8 == 6 || wp % 8 == 7) ? 1 : 0;
        dest.writeField(wp, fillBit, 1);
    }
}

void L3SIType4RestOctets::parseV(const L3Frame& src, size_t& rp) {
    src.readField(rp, 2);
    if (rp + 1 <= src.size()) {
        unsigned gprsBit = src.readField(rp, 1);
        if (gprsBit) {
            mHaveGPRS = true;
            mRA_COLOUR = src.readField(rp, 3);
            src.readField(rp, 1);
        }
    }
}

void L3SIType4RestOctets::parseV(const L3Frame& src, size_t& rp, size_t) {
    parseV(src, rp);
}

void L3SIType4RestOctets::text(std::ostream& os) const {
    os << "SI4RestOctets";
    if (mHaveGPRS) os << " GPRS RA_COLOUR=" << mRA_COLOUR;
}

// ── L3IARestOctets ─────────────────────────────────────────────────────

L3IARestOctets::L3IARestOctets()
    : mHavePacketAssignment(false) {}

size_t L3IARestOctets::lengthBits() const {
    return mHavePacketAssignment ? 8 : 0;
}

void L3IARestOctets::writeBits(L3Frame& dest, size_t& wp) const {
    if (mHavePacketAssignment) {
        dest.writeField(wp, 0, 8);
    }
}

void L3IARestOctets::text(std::ostream& os) const {
    os << "IARestOctets";
    if (mHavePacketAssignment) os << " [PacketAssignment]";
}

// ── L3GPRSCellOptions ──────────────────────────────────────────────────

L3GPRSCellOptions::L3GPRSCellOptions()
    : mNMO(0), mT3168(0), mT3192(0), mDRX_TIMER_MAX(0),
      mACCESS_BURST_TYPE(0), mCONTROL_ACK_TYPE(0), mBS_VC_MAX(0) {}

size_t L3GPRSCellOptions::lengthBits() const {
    return 2 + 3 + 3 + 3 + 1 + 1 + 4 + 1 + 1;
}

void L3GPRSCellOptions::writeBits(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, mNMO, 2);
    dest.writeField(wp, mT3168, 3);
    dest.writeField(wp, mT3192, 3);
    dest.writeField(wp, mDRX_TIMER_MAX, 3);
    dest.writeField(wp, mACCESS_BURST_TYPE, 1);
    dest.writeField(wp, mCONTROL_ACK_TYPE, 1);
    dest.writeField(wp, mBS_VC_MAX, 4);
    dest.writeField(wp, 0, 1);
    dest.writeField(wp, 0, 1);
}

void L3GPRSCellOptions::text(std::ostream& os) const {
    os << "GPRSCellOpts[NMO=" << mNMO << " T3168=" << mT3168
       << " T3192=" << mT3192 << " DRX=" << mDRX_TIMER_MAX << "]";
}

// ── L3GPRSSI13PowerControlParameters ───────────────────────────────────

L3GPRSSI13PowerControlParameters::L3GPRSSI13PowerControlParameters()
    : mALPHA(0) {}

size_t L3GPRSSI13PowerControlParameters::lengthBits() const {
    return 4 + 5 + 5 + 1 + 4;
}

void L3GPRSSI13PowerControlParameters::writeBits(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, mALPHA, 4);
    dest.writeField(wp, 0, 5);
    dest.writeField(wp, 0, 5);
    dest.writeField(wp, 0, 1);
    dest.writeField(wp, 15, 4);
}

void L3GPRSSI13PowerControlParameters::text(std::ostream& os) const {
    os << "GPRSPowerCtrl[ALPHA=" << mALPHA << "]";
}

// ── L3SI13RestOctets ───────────────────────────────────────────────────

L3SI13RestOctets::L3SI13RestOctets()
    : mRAC(0), mSPGC_CCCH_SUP(false), mPRIORITY_ACCESS_THR(0),
      mNETWORK_CONTROL_ORDER(0) {}

size_t L3SI13RestOctets::lengthV() const {
    int bits = 1 + 3 + 4 + 1 + 1 + 8 + 1 + 3 + 2;
    bits += mCellOptions.lengthBits();
    bits += mPowerControlParameters.lengthBits();
    // Round up to byte boundary for padding
    return (bits + 7) / 8;
}

void L3SI13RestOctets::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, 1, 1);
    dest.writeField(wp, 0, 3);
    dest.writeField(wp, 0, 4);
    dest.writeField(wp, 0, 1);
    dest.writeField(wp, 0, 1);
    dest.writeField(wp, mRAC, 8);
    dest.writeField(wp, mSPGC_CCCH_SUP, 1);
    dest.writeField(wp, mPRIORITY_ACCESS_THR, 3);
    dest.writeField(wp, mNETWORK_CONTROL_ORDER, 2);
    mCellOptions.writeBits(dest, wp);
    mPowerControlParameters.writeBits(dest, wp);
    // Pad to byte boundary with H/L pattern
    while (wp % 8 != 0) {
        unsigned fillBit = (wp % 8 == 1 || wp % 8 == 3 || wp % 8 == 6 || wp % 8 == 7) ? 1 : 0;
        dest.writeField(wp, fillBit, 1);
    }
}

void L3SI13RestOctets::parseV(const L3Frame& src, size_t& rp) {
    if (rp >= src.size()) return;
    src.readField(rp, 1);
    src.readField(rp, 3);
    src.readField(rp, 4);
    src.readField(rp, 1);
    src.readField(rp, 1);
    mRAC = src.readField(rp, 8);
    mSPGC_CCCH_SUP = src.readField(rp, 1);
    mPRIORITY_ACCESS_THR = src.readField(rp, 3);
    mNETWORK_CONTROL_ORDER = src.readField(rp, 2);
}

void L3SI13RestOctets::parseV(const L3Frame& src, size_t& rp, size_t) {
    parseV(src, rp);
}

void L3SI13RestOctets::text(std::ostream& os) const {
    os << "SI13RestOctets[RAC=" << mRAC
       << " SPGC=" << mSPGC_CCCH_SUP
       << " PAT=" << mPRIORITY_ACCESS_THR
       << " NCO=" << mNETWORK_CONTROL_ORDER << "]";
}

} // namespace gsml3parser
