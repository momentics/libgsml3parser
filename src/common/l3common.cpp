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

// ── L3ChannelMode ───────────────────────────────────────────────────────

L3ChannelMode::L3ChannelMode(Mode wMode) : mMode(wMode) {}

void L3ChannelMode::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, static_cast<unsigned>(mMode), 2);
    dest.writeField(wp, 0, 6);
}

void L3ChannelMode::parseV(const L3Frame& src, size_t& rp) {
    mMode = static_cast<Mode>(src.readField(rp, 2));
    src.readField(rp, 6);
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
    dest.writeField(wp, 0, 2);
    dest.writeField(wp, mTimingAdvance, 6);
}

void L3TimingAdvance::parseV(const L3Frame& src, size_t& rp) {
    src.readField(rp, 2);
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
    dest.writeField(wp, mARFCN, 10);
    dest.writeField(wp, mNCC, 4);
    dest.writeField(wp, mBCC, 4);
}

void L3CellDescription::parseV(const L3Frame& src, size_t& rp) {
    mARFCN = src.readField(rp, 10);
    mNCC = src.readField(rp, 4);
    mBCC = src.readField(rp, 4);
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
    dest.writeField(wp, mCiphering ? 1 : 0, 1);
    dest.writeField(wp, mAlgorithm, 7);
}

void L3CipheringModeSetting::parseV(const L3Frame& src, size_t& rp) {
    mCiphering = src.readField(rp, 1);
    mAlgorithm = src.readField(rp, 7);
}

void L3CipheringModeSetting::parseV(const L3Frame& src, size_t& rp, size_t) {
    parseV(src, rp);
}

void L3CipheringModeSetting::text(std::ostream& os) const {
    os << "CipherMode[cipher=" << mCiphering << " algo=A5/" << mAlgorithm << "]";
}

// ── L3CipheringModeResponse ────────────────────────────────────────────

L3CipheringModeResponse::L3CipheringModeResponse(bool wIncludeIMEISV)
    : mIncludeIMEISV(wIncludeIMEISV) {}

void L3CipheringModeResponse::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, mIncludeIMEISV ? 1 : 0, 1);
    dest.writeField(wp, 0, 7);
}

void L3CipheringModeResponse::parseV(const L3Frame& src, size_t& rp) {
    mIncludeIMEISV = src.readField(rp, 1);
    src.readField(rp, 7);
}

void L3CipheringModeResponse::parseV(const L3Frame& src, size_t& rp, size_t) {
    parseV(src, rp);
}

void L3CipheringModeResponse::text(std::ostream& os) const {
    os << "CipherResp[IMEISV=" << mIncludeIMEISV << "]";
}

// ── L3SynchronizationIndication ────────────────────────────────────────

L3SynchronizationIndication::L3SynchronizationIndication(bool wNCI, bool wROT, int wSI)
    : mNCI(wNCI), mROT(wROT), mSI(wSI & 3) {}

void L3SynchronizationIndication::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, mNCI ? 1 : 0, 1);
    dest.writeField(wp, mROT ? 1 : 0, 1);
    dest.writeField(wp, mSI, 2);
    dest.writeField(wp, 0, 4);
}

void L3SynchronizationIndication::parseV(const L3Frame& src, size_t& rp) {
    mNCI = src.readField(rp, 1);
    mROT = src.readField(rp, 1);
    mSI = src.readField(rp, 2);
    src.readField(rp, 4);
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
    dest.writeField(wp, mPageMode, 8);
}

void L3PageMode::parseV(const L3Frame& src, size_t& rp) {
    mPageMode = src.readField(rp, 8);
}

void L3PageMode::parseV(const L3Frame& src, size_t& rp, size_t) {
    parseV(src, rp);
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
    dest.writeField(wp, mT3, 3);
    dest.writeField(wp, mT2, 8);
}

void L3RequestReference::parseV(const L3Frame& src, size_t& rp) {
    mRA = src.readField(rp, 8);
    mT1p = src.readField(rp, 5);
    mT3 = src.readField(rp, 3);
    mT2 = src.readField(rp, 8);
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
    dest.writeField(wp, mPWRC, 1);
    dest.writeField(wp, mDTX, 2);
    dest.writeField(wp, mRADIO_LINK_TIMEOUT, 5);
}

void L3CellOptionsBCCH::parseV(const L3Frame& src, size_t& rp) {
    mPWRC = src.readField(rp, 1);
    mDTX = src.readField(rp, 2);
    mRADIO_LINK_TIMEOUT = src.readField(rp, 5);
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
    dest.writeField(wp, mPWRC, 1);
    dest.writeField(wp, mDTX, 2);
    dest.writeField(wp, mRADIO_LINK_TIMEOUT, 5);
}

void L3CellOptionsSACCH::parseV(const L3Frame& src, size_t& rp) {
    mPWRC = src.readField(rp, 1);
    mDTX = src.readField(rp, 2);
    mRADIO_LINK_TIMEOUT = src.readField(rp, 5);
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
    dest.writeField(wp, mACS, 1);
    dest.writeField(wp, mNECI, 4);
    dest.writeField(wp, mCELL_RESELECT_HYSTERESIS, 3);
    dest.writeField(wp, mMS_TXPWR_MAX_CCH, 3);
    dest.writeField(wp, mRXLEV_ACCESS_MIN, 5);
}

void L3CellSelectionParameters::parseV(const L3Frame& src, size_t& rp) {
    mACS = src.readField(rp, 1);
    mNECI = src.readField(rp, 4);
    mCELL_RESELECT_HYSTERESIS = src.readField(rp, 3);
    mMS_TXPWR_MAX_CCH = src.readField(rp, 3);
    mRXLEV_ACCESS_MIN = src.readField(rp, 5);
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

// ── L3NeighborCellsDescription ─────────────────────────────────────────

L3NeighborCellsDescription::L3NeighborCellsDescription() {}

L3NeighborCellsDescription::L3NeighborCellsDescription(const std::vector<unsigned>& neighbors)
    : mARFCNs(neighbors) {}

size_t L3NeighborCellsDescription::lengthV() const {
    return 1 + mARFCNs.size();
}

void L3NeighborCellsDescription::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, mARFCNs.size(), 8);
    for (const auto& arfcn : mARFCNs) {
        dest.writeField(wp, arfcn, 8);
    }
}

void L3NeighborCellsDescription::parseV(const L3Frame& src, size_t& rp) {
    unsigned count = src.readField(rp, 8);
    mARFCNs.clear();
    for (unsigned i = 0; i < count; ++i) {
        mARFCNs.push_back(src.readField(rp, 8));
    }
}

void L3NeighborCellsDescription::parseV(const L3Frame& src, size_t& rp, size_t) {
    parseV(src, rp);
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
    dest.writeField(wp, mMEAS_VALID ? 1 : 0, 1);
    dest.writeField(wp, mRXLEV_FULL_SERVING_CELL, 6);
    dest.writeField(wp, mRXLEV_SUB_SERVING_CELL, 6);
    dest.writeField(wp, mRXQUAL_FULL_SERVING_CELL, 3);
    dest.writeField(wp, mRXQUAL_SUB_SERVING_CELL, 3);
    dest.writeField(wp, mNO_NCELL, 3);
    for (unsigned i = 0; i < mNO_NCELL && i < 6; ++i) {
        dest.writeField(wp, mRXLEV_NCELL[i], 6);
        dest.writeField(wp, mBCCH_FREQ_NCELL[i], 10);
        dest.writeField(wp, mBSIC_NCELL[i], 6);
    }
}

void L3MeasurementResults::parseV(const L3Frame& src, size_t& rp) {
    mBA_USED = src.readField(rp, 1);
    mDTX_USED = src.readField(rp, 1);
    mMEAS_VALID = src.readField(rp, 1);
    mRXLEV_FULL_SERVING_CELL = src.readField(rp, 6);
    mRXLEV_SUB_SERVING_CELL = src.readField(rp, 6);
    mRXQUAL_FULL_SERVING_CELL = src.readField(rp, 3);
    mRXQUAL_SUB_SERVING_CELL = src.readField(rp, 3);
    mNO_NCELL = src.readField(rp, 3);
    for (unsigned i = 0; i < mNO_NCELL && i < 6; ++i) {
        mRXLEV_NCELL[i] = src.readField(rp, 6);
        mBCCH_FREQ_NCELL[i] = src.readField(rp, 10);
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
    dest.writeField(wp, mProtocolIdentifier, 8);
}

void L3APDUID::parseV(const L3Frame& src, size_t& rp) {
    mProtocolIdentifier = src.readField(rp, 8);
}

void L3APDUID::parseV(const L3Frame& src, size_t& rp, size_t) {
    parseV(src, rp);
}

void L3APDUID::text(std::ostream& os) const {
    os << "APDUID[" << mProtocolIdentifier << "]";
}

// ── L3APDUFlags ────────────────────────────────────────────────────────

L3APDUFlags::L3APDUFlags(unsigned cr, unsigned firstSegment, unsigned lastSegment)
    : mCR(cr), mFirstSegment(firstSegment), mLastSegment(lastSegment) {}

void L3APDUFlags::writeV(L3Frame& dest, size_t& wp) const {
    dest.writeField(wp, mCR, 1);
    dest.writeField(wp, mFirstSegment, 1);
    dest.writeField(wp, mLastSegment, 1);
    dest.writeField(wp, 0, 5);
}

void L3APDUFlags::parseV(const L3Frame& src, size_t& rp) {
    mCR = src.readField(rp, 1);
    mFirstSegment = src.readField(rp, 1);
    mLastSegment = src.readField(rp, 1);
    src.readField(rp, 5);
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
    dest.writeField(wp, mDownlink, 1);
    dest.writeField(wp, mTMA, 1);
    dest.writeField(wp, mDMOrTBF, 1);
    dest.writeField(wp, 0, 5);
}

void L3DedicatedModeOrTBF::parseV(const L3Frame& src, size_t& rp) {
    mDownlink = src.readField(rp, 1);
    mTMA = src.readField(rp, 1);
    mDMOrTBF = src.readField(rp, 1);
    src.readField(rp, 5);
}

void L3DedicatedModeOrTBF::parseV(const L3Frame& src, size_t& rp, size_t) {
    parseV(src, rp);
}

void L3DedicatedModeOrTBF::text(std::ostream& os) const {
    os << "DedModeOrTBF[DL=" << mDownlink << " TBF=" << mDMOrTBF << "]";
}

} // namespace gsml3parser
