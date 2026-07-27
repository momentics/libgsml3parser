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
    return mMNC[0] * 100 + mMNC[1] * 10 + mMNC[2];
}

void L3LocationAreaIdentity::parseV(const L3Frame& source, size_t& rp) {
    // MCC: 3 BCD digits (12 bits)
    unsigned mccBcd = source.readField(rp, 12);
    mMCC[0] = mccBcd / 100;
    mMCC[1] = (mccBcd / 10) % 10;
    mMCC[2] = mccBcd % 10;

    // MNC: 3 BCD digits (12 bits), last digit may be 15 for padding
    unsigned mncBcd = source.readField(rp, 12);
    mMNC[0] = mncBcd / 100;
    mMNC[1] = (mncBcd / 10) % 10;
    mMNC[2] = mncBcd % 10;

    // LAC: 16 bits
    mLAC = source.readField(rp, 16);
}

void L3LocationAreaIdentity::parseV(const L3Frame& src, size_t& rp, size_t) {
    parseV(src, rp);
}

void L3LocationAreaIdentity::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, mMCC[0] * 100 + mMCC[1] * 10 + mMCC[2], 12);
    dest.writeField(wp, mMNC[0] * 100 + mMNC[1] * 10 + mMNC[2], 12);
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
    switch (mType) {
        case MobileIDType::TMSI:    return 4;
        case MobileIDType::IMSI:    return (std::strlen(mDigits) + 1) / 2 + 1;
        case MobileIDType::IMEI:    return (std::strlen(mDigits) + 1) / 2 + 1;
        case MobileIDType::IMEISV:  return (std::strlen(mDigits) + 1) / 2 + 1;
        default:                    return 0;
    }
}

void L3MobileIdentity::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, static_cast<unsigned>(mType), 2);
    switch (mType) {
        case MobileIDType::TMSI:
            dest.writeField(wp, mTMSI, 32);
            break;
        case MobileIDType::IMSI:
        case MobileIDType::IMEI:
        case MobileIDType::IMEISV: {
            size_t len = lengthV();
            dest.writeField(wp, static_cast<unsigned>(len), 8);
            // BCD encode digits
            size_t dlen = std::strlen(mDigits);
            for (size_t i = 0; i < dlen; i += 2) {
                unsigned hi = mDigits[i] - '0';
                unsigned lo = (i + 1 < dlen) ? mDigits[i + 1] - '0' : 0xf;
                dest.writeField(wp, hi * 10 + lo, 8);
            }
            break;
        default: break;
    }
}

void L3MobileIdentity::parseV(const L3Frame& src, size_t& rp, size_t expectedLength) {
    mType = static_cast<MobileIDType>(src.readField(rp, 2));
    switch (mType) {
        case MobileIDType::TMSI:
            mTMSI = src.readField(rp, 32);
            mDigits[0] = '\0';
            break;
        case MobileIDType::IMSI:
        case MobileIDType::IMEI:
        case MobileIDType::IMEISV: {
            size_t nDigits = (expectedLength - 1) * 2; // subtract length byte
            if (nDigits > sizeof(mDigits) - 1) nDigits = sizeof(mDigits) - 1;
            for (size_t i = 0; i < nDigits; ++i) {
                unsigned bcd = src.readField(rp, 4);
                if (bcd == 0xf) break; // padding
                mDigits[i] = static_cast<char>('0' + bcd);
            }
            mDigits[nDigits] = '\0';
            break;
        default:
            mDigits[0] = '\0';
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
    dest.writeField(wp, mRevisionLevel, 2);
    dest.writeField(wp, mES_IND, 1);
    dest.writeField(wp, mA5_1, 1);
    dest.writeField(wp, mRFPowerCapability, 4);
}

void L3MobileStationClassmark1::parseV(const L3Frame& src, size_t& rp) {
    mRevisionLevel = src.readField(rp, 2);
    mES_IND        = src.readField(rp, 1);
    mA5_1          = src.readField(rp, 1);
    mRFPowerCapability = src.readField(rp, 4);
}

void L3MobileStationClassmark1::text(std::ostream& os) const {
    os << "CM1[rev=" << mRevisionLevel << " ES=" << mES_IND
       << " A5_1=" << mA5_1 << " PWR=" << mRFPowerCapability;
}

// ── L3MobileStationClassmark2 ───────────────────────────────────────────

void L3MobileStationClassmark2::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, mRevisionLevel, 2);
    dest.writeField(wp, mES_IND, 1);
    dest.writeField(wp, mA5_1, 1);
    dest.writeField(wp, mA5_3, 1);
    dest.writeField(wp, mA5_2, 1);
    dest.writeField(wp, mRFPowerCapability, 4);
    dest.writeField(wp, mPSCapability, 1);
    dest.writeField(wp, mSSScreenIndicator, 1);
    dest.writeField(wp, mSMCapability, 1);
    dest.writeField(wp, mVBS, 1);
    dest.writeField(wp, mVGCS, 1);
    dest.writeField(wp, mFC, 1);
    dest.writeField(wp, mCM3, 1);
    dest.writeField(wp, mLCSVACapability, 1);
    dest.writeField(wp, mSoLSA, 1);
    dest.writeField(wp, mCMSF, 1);
}

void L3MobileStationClassmark2::parseV(const L3Frame& src, size_t& rp) {
    mRevisionLevel      = src.readField(rp, 2);
    mES_IND             = src.readField(rp, 1);
    mA5_1               = src.readField(rp, 1);
    mA5_3               = src.readField(rp, 1);
    mA5_2               = src.readField(rp, 1);
    mRFPowerCapability  = src.readField(rp, 4);
    mPSCapability       = src.readField(rp, 1);
    mSSScreenIndicator  = src.readField(rp, 1);
    mSMCapability       = src.readField(rp, 1);
    mVBS                = src.readField(rp, 1);
    mVGCS               = src.readField(rp, 1);
    mFC                 = src.readField(rp, 1);
    mCM3                = src.readField(rp, 1);
    mLCSVACapability    = src.readField(rp, 1);
    mSoLSA              = src.readField(rp, 1);
    mCMSF               = src.readField(rp, 1);
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
    dest.writeField(wp, mMultiband, 1);
    dest.writeField(wp, mA5_4, 1);
    dest.writeField(wp, mA5_5, 1);
    dest.writeField(wp, mA5_6, 1);
    dest.writeField(wp, mA5_7, 1);
    for (int i = 0; i < 107; i++) {
        dest.writeField(wp, 0, 1);
    }
}

void L3MobileStationClassmark3::parseV(const L3Frame& src, size_t& rp) {
    mMultiband = src.readField(rp, 1);
    mA5_4      = src.readField(rp, 1);
    mA5_5      = src.readField(rp, 1);
    mA5_6      = src.readField(rp, 1);
    mA5_7      = src.readField(rp, 1);
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

// ── L3CellChannelDescription ───────────────────────────────────────────

L3CellChannelDescription::L3CellChannelDescription(unsigned wARfcn, unsigned wBSIC, unsigned wSpacing)
    : mBSIC(wBSIC), mARfcn(wARfcn), mChannelSpacing(wSpacing) {}

void L3CellChannelDescription::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, mARfcn, 14);
    dest.writeField(wp, mBSIC, 6);
    dest.writeField(wp, mChannelSpacing, 1);
    dest.writeField(wp, 0, 1);
}

void L3CellChannelDescription::parseV(const L3Frame& src, size_t& rp) {
    mARfcn = src.readField(rp, 14);
    mBSIC = src.readField(rp, 6);
    mChannelSpacing = src.readField(rp, 1);
    src.readField(rp, 1);
}

void L3CellChannelDescription::parseV(const L3Frame& src, size_t& rp, size_t) {
    parseV(src, rp);
}

void L3CellChannelDescription::text(std::ostream& os) const {
    os << "CellChannel[ARfcn=" << mARfcn << " BSIC=" << mBSIC
       << " Spacing=" << mChannelSpacing << "]";
}

// ── L3ControlChannelDescription ────────────────────────────────────────

L3ControlChannelDescription::L3ControlChannelDescription(unsigned wARfcn, unsigned wBSIC,
    unsigned wSpacing, bool wCCCH, bool wSACCH, bool wCBCH)
    : mBSIC(wBSIC), mARfcn(wARfcn), mChannelSpacing(wSpacing),
      mCCCH(wCCCH), mSACCH(wSACCH), mCBCH(wCBCH) {}

void L3ControlChannelDescription::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, mARfcn, 14);
    dest.writeField(wp, mBSIC, 6);
    dest.writeField(wp, mChannelSpacing, 1);
    dest.writeField(wp, mCCCH, 1);
    dest.writeField(wp, mSACCH, 1);
    dest.writeField(wp, mCBCH, 1);
    dest.writeField(wp, 0, 1);
}

void L3ControlChannelDescription::parseV(const L3Frame& src, size_t& rp) {
    mARfcn = src.readField(rp, 14);
    mBSIC = src.readField(rp, 6);
    mChannelSpacing = src.readField(rp, 1);
    mCCCH = src.readField(rp, 1);
    mSACCH = src.readField(rp, 1);
    mCBCH = src.readField(rp, 1);
    src.readField(rp, 1);
}

void L3ControlChannelDescription::parseV(const L3Frame& src, size_t& rp, size_t) {
    parseV(src, rp);
}

void L3ControlChannelDescription::text(std::ostream& os) const {
    os << "ControlChannel[ARfcn=" << mARfcn << " BSIC=" << mBSIC
       << " Spacing=" << mChannelSpacing << " CCCH=" << mCCCH
       << " SACCH=" << mSACCH << " CBCH=" << mCBCH << "]";
}

// ── L3ChannelDescription ───────────────────────────────────────────────

L3ChannelDescription::L3ChannelDescription(ChannelType wType, unsigned wARfcn, unsigned wBSIC)
    : mChannelType(wType), mARfcn(wARfcn), mBSIC(wBSIC) {}

void L3ChannelDescription::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, static_cast<unsigned>(mChannelType), 4);
    dest.writeField(wp, mARfcn, 14);
    dest.writeField(wp, mBSIC, 6);
}

void L3ChannelDescription::parseV(const L3Frame& src, size_t& rp) {
    mChannelType = static_cast<ChannelType>(src.readField(rp, 4));
    mARfcn = src.readField(rp, 14);
    mBSIC = src.readField(rp, 6);
}

void L3ChannelDescription::parseV(const L3Frame& src, size_t& rp, size_t) {
    parseV(src, rp);
}

void L3ChannelDescription::text(std::ostream& os) const {
    os << "Channel[Type=" << static_cast<int>(mChannelType)
       << " ARfcn=" << mARfcn << " BSIC=" << mBSIC << "]";
}

// ── L3PowerCommand ─────────────────────────────────────────────────────

void L3PowerCommand::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, mPowerLevel & 0x7f, 7);
    dest.writeField(wp, (mPowerLevel < 0) ? 1 : 0, 1);
}

void L3PowerCommand::parseV(const L3Frame& src, size_t& rp) {
    unsigned val = src.readField(rp, 7);
    unsigned sign = src.readField(rp, 1);
    mPowerLevel = sign ? -(int)val : (int)val;
}

void L3PowerCommand::parseV(const L3Frame& src, size_t& rp, size_t) {
    parseV(src, rp);
}

void L3PowerCommand::text(std::ostream& os) const {
    os << "PowerCommand[" << mPowerLevel << "]";
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
      mCellReservedIndicator(0), mCellBarQualifier(0), mCellBarQualifierLength(0) {}

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
    dest.writeField(wp, mCellBarQualifier, 4);
    dest.writeField(wp, mCellBarQualifierLength, 4);
    for (size_t i = 0; i < mCellBarQualifier.size(); ++i) {
        dest.writeField(wp, mCellBarQualifier[i], 8);
    }
}

void L3CellSelection::parseV(const L3Frame& src, size_t& rp) {
    mRxLevAccessMin = src.readField(rp, 5);
    mRxLevelAccessMin = src.readField(rp, 1);
    mMaxRxLev = src.readField(rp, 5);
    mCellReselectionHysteresis = src.readField(rp, 3);
    mCellReselectionOffset = src.readField(rp, 3);
    mCellReservedIndicator = src.readField(rp, 2);
    mCellBarQualifier = src.readField(rp, 4);
    mCellBarQualifierLength = src.readField(rp, 4);
    mCellBarQualifier.clear();
    size_t bytes = (mCellBarQualifierLength + 1) / 2;
    for (size_t i = 0; i < bytes; ++i) {
        mCellBarQualifier.push_back(static_cast<unsigned char>(src.readField(rp, 8)));
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
       << " CBQ=" << mCellBarQualifier
       << " CBQLen=" << mCellBarQualifierLength << "]";
}

// ── L3RACHControlParameters ────────────────────────────────────────────

L3RACHControlParameters::L3RACHControlParameters(unsigned wMaxRep, unsigned wInitRep,
    unsigned wMaxDelay, bool wBarred, unsigned wRACHCtrl)
    : mMaxRepetition(wMaxRep), mInitialRepeat(wInitRep), mMaxDelay(wMaxDelay),
      mAccessBarredforRACH(wBarred), mRACHControlValue(wRACHCtrl) {}

void L3RACHControlParameters::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, mMaxRepetition, 2);
    dest.writeField(wp, mInitialRepeat, 2);
    dest.writeField(wp, mMaxDelay, 4);
    dest.writeField(wp, mAccessBarredforRACH, 1);
    dest.writeField(wp, mRACHControlValue, 7);
}

void L3RACHControlParameters::parseV(const L3Frame& src, size_t& rp) {
    mMaxRepetition = src.readField(rp, 2);
    mInitialRepeat = src.readField(rp, 2);
    mMaxDelay = src.readField(rp, 4);
    mAccessBarredforRACH = src.readField(rp, 1);
    mRACHControlValue = src.readField(rp, 7);
}

void L3RACHControlParameters::parseV(const L3Frame& src, size_t& rp, size_t) {
    parseV(src, rp);
}

void L3RACHControlParameters::text(std::ostream& os) const {
    os << "RACHControl[MaxRep=" << mMaxRepetition
       << " InitRep=" << mInitialRepeat
       << " MaxDelay=" << mMaxDelay
       << " Barred=" << mAccessBarredforRACH
       << " RACHCtrl=" << mRACHControlValue << "]";
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

L3AdditionalChannelDescription::L3AdditionalChannelDescription(ChannelType wType, unsigned wARfcn, unsigned wBSIC)
    : mChannelType(wType), mARfcn(wARfcn), mBSIC(wBSIC) {}

void L3AdditionalChannelDescription::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, static_cast<unsigned>(mChannelType), 4);
    dest.writeField(wp, mARfcn, 14);
    dest.writeField(wp, mBSIC, 6);
}

void L3AdditionalChannelDescription::parseV(const L3Frame& src, size_t& rp) {
    mChannelType = static_cast<ChannelType>(src.readField(rp, 4));
    mARfcn = src.readField(rp, 14);
    mBSIC = src.readField(rp, 6);
}

void L3AdditionalChannelDescription::parseV(const L3Frame& src, size_t& rp, size_t) {
    parseV(src, rp);
}

void L3AdditionalChannelDescription::text(std::ostream& os) const {
    os << "AddlChannel[Type=" << static_cast<int>(mChannelType)
       << " ARfcn=" << mARfcn << " BSIC=" << mBSIC << "]";
}

} // namespace gsml3parser
