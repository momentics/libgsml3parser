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

#ifndef GSML3PARSER_COMMON_L3COMMON_H
#define GSML3PARSER_COMMON_L3COMMON_H

#include <cstdint>
#include <ostream>
#include <string>
#include <vector>

#include "../l3message.h"
#include "../types.h"
#include "../enums.h"

namespace gsml3parser {

// ── Cell Identity (GSM 04.08 10.5.1.1) ─────────────────────────────────

class L3CellIdentity : public L3ProtocolElement {
private:
    unsigned mID;
public:
    explicit L3CellIdentity(unsigned wID = 0) : mID(wID) {}
    unsigned ID() const { return mID; }
    size_t lengthV() const override { return 2; }
    void writeV(L3Frame& dest, size_t& wp) const override;
    void parseV(const L3Frame& src, size_t& rp) override;
    void parseV(const L3Frame& src, size_t& rp, size_t) override;
    void text(std::ostream& os) const override;
};

// ── Location Area Identity (GSM 04.08 10.5.1.3) ────────────────────────

class L3LocationAreaIdentity : public L3ProtocolElement {
private:
    unsigned mMCC[3];
    unsigned mMNC[3];
    unsigned mLAC;
public:
    L3LocationAreaIdentity(const char* wMCC = "250", const char* wMNC = "01", unsigned wLAC = 1);
    bool operator==(const L3LocationAreaIdentity&) const;
    int MCC() const;
    int MNC() const;
    int LAC() const { return mLAC; }
    size_t lengthV() const override { return 5; }
    void parseV(const L3Frame& source, size_t& rp) override;
    void parseV(const L3Frame&, size_t&, size_t) override;
    void writeV(L3Frame& dest, size_t& wp) const override;
    void text(std::ostream& os) const override;
};

// ── Mobile Identity (GSM 04.08 10.5.1.4) ───────────────────────────────

class L3MobileIdentity : public L3ProtocolElement {
private:
    MobileIDType mType;
    char mDigits[16];
    uint32_t mTMSI;
public:
    L3MobileIdentity();
    explicit L3MobileIdentity(uint32_t wTMSI);
    explicit L3MobileIdentity(const char* wDigits);

    MobileIDType type() const { return mType; }
    const char* digits() const;
    uint32_t TMSI() const;
    bool isIMSI() const { return mType == MobileIDType::IMSI; }
    bool isTMSI() const { return mType == MobileIDType::TMSI; }

    bool operator==(const L3MobileIdentity&) const;
    bool operator!=(const L3MobileIdentity& other) const { return !operator==(other); }
    bool operator<(const L3MobileIdentity&) const;

    size_t lengthV() const override;
    void writeV(L3Frame& dest, size_t& wp) const override;
    void parseV(const L3Frame& src, size_t& rp, size_t expectedLength) override;
    void parseV(const L3Frame&, size_t&) override { throw ParseError("parseV not valid for MobileIdentity"); }
    void text(std::ostream& os) const override;
};

// ── Mobile Station Classmark 1 (GSM 04.08 10.5.1.5) ───────────────────

class L3MobileStationClassmark1 : public L3ProtocolElement {
protected:
    unsigned mRevisionLevel;
    unsigned mES_IND;
    unsigned mA5_1;
    unsigned mRFPowerCapability;
public:
    size_t lengthV() const override { return 1; }
    void writeV(L3Frame& dest, size_t& wp) const override;
    void parseV(const L3Frame& src, size_t& rp) override;
    void parseV(const L3Frame&, size_t&, size_t) override { throw ParseError("parseV not valid"); }
    void text(std::ostream& os) const override;
};

// ── Mobile Station Classmark 2 (GSM 04.08 10.5.1.5) ───────────────────

class L3MobileStationClassmark2 : public L3ProtocolElement {
protected:
    unsigned mRevisionLevel;
    unsigned mES_IND;
    unsigned mA5_1;
    unsigned mA5_3;
    unsigned mA5_2;
    unsigned mRFPowerCapability;
    unsigned mPSCapability;
    unsigned mSSScreenIndicator;
    unsigned mSMCapability;
    unsigned mVBS;
    unsigned mVGCS;
    unsigned mFC;
    unsigned mCM3;
    unsigned mLCSVACapability;
    unsigned mSoLSA;
    unsigned mCMSF;
public:
    size_t lengthV() const override { return 3; }
    void writeV(L3Frame& dest, size_t& wp) const override;
    void parseV(const L3Frame& src, size_t& rp) override;
    void parseV(const L3Frame&, size_t&, size_t) override;
    void text(std::ostream& os) const override;
    int getA5Bits() const;
    int powerClass() const { return mRFPowerCapability + 1; }
};

// ── Mobile Station Classmark 3 (GSM 04.08 10.5.1.7) ───────────────────

class L3MobileStationClassmark3 : public L3ProtocolElement {
protected:
    unsigned mMultiband;
    unsigned mA5_4;
    unsigned mA5_5;
    unsigned mA5_6;
    unsigned mA5_7;
public:
    L3MobileStationClassmark3();
    size_t lengthV() const override { return 14; }
    void writeV(L3Frame& dest, size_t& wp) const override;
    void parseV(const L3Frame&, size_t&) override;
    void parseV(const L3Frame&, size_t&, size_t) override;
    void text(std::ostream& os) const override;
};

// ── Ciphering Key Sequence Number ──────────────────────────────────────

class L3CipheringKeySequenceNumber : public L3ProtocolElement {
protected:
    unsigned mCIValue;
public:
    explicit L3CipheringKeySequenceNumber(unsigned wCIValue = 0) : mCIValue(wCIValue) {}
    size_t lengthV() const override { return 0; }
    void writeV(L3Frame&, size_t&) const override;
    void parseV(const L3Frame& src, size_t& rp) override { mCIValue = src.readField(rp, 4) & 0x0f; }
    void parseV(const L3Frame&, size_t&, size_t) override { throw ParseError("parseV not valid"); }
    void text(std::ostream& os) const override;
};

// ── Cell Channel Description (GSM 04.08 10.5.2.1) ─────────────────────

class L3CellChannelDescription : public L3ProtocolElement {
private:
    unsigned mBSIC;
    unsigned mARfcn;
    unsigned mChannelSpacing;
public:
    L3CellChannelDescription(unsigned wARfcn = 0, unsigned wBSIC = 0, unsigned wSpacing = 0);
    unsigned BSIC() const { return mBSIC; }
    unsigned ARfcn() const { return mARfcn; }
    unsigned ChannelSpacing() const { return mChannelSpacing; }
    size_t lengthV() const override { return 5; }
    void writeV(L3Frame& dest, size_t& wp) const override;
    void parseV(const L3Frame& src, size_t& rp) override;
    void parseV(const L3Frame& src, size_t& rp, size_t) override;
    void text(std::ostream& os) const override;
};

// ── Control Channel Description (GSM 04.08 10.5.2.2) ──────────────────

class L3ControlChannelDescription : public L3ProtocolElement {
private:
    unsigned mBSIC;
    unsigned mARfcn;
    unsigned mChannelSpacing;
    unsigned mCCCH;
    unsigned mSACCH;
    unsigned mCBCH;
public:
    L3ControlChannelDescription(unsigned wARfcn = 0, unsigned wBSIC = 0,
                                 unsigned wSpacing = 0, bool wCCCH = false,
                                 bool wSACCH = false, bool wCBCH = false);
    unsigned BSIC() const { return mBSIC; }
    unsigned ARfcn() const { return mARfcn; }
    unsigned ChannelSpacing() const { return mChannelSpacing; }
    bool CCCH() const { return mCCCH; }
    bool SACCH() const { return mSACCH; }
    bool CBCH() const { return mCBCH; }
    size_t lengthV() const override { return 6; }
    void writeV(L3Frame& dest, size_t& wp) const override;
    void parseV(const L3Frame& src, size_t& rp) override;
    void parseV(const L3Frame& src, size_t& rp, size_t) override;
    void text(std::ostream& os) const override;
};

// ── Channel Description (GSM 04.08 10.5.2.3) ──────────────────────────

class L3ChannelDescription : public L3ProtocolElement {
private:
    ChannelType mChannelType;
    unsigned mARfcn;
    unsigned mBSIC;
public:
    L3ChannelDescription(ChannelType wType = ChannelType::SDCCHType,
                          unsigned wARfcn = 0, unsigned wBSIC = 0);
    ChannelType ChannelType() const { return mChannelType; }
    unsigned ARfcn() const { return mARfcn; }
    unsigned BSIC() const { return mBSIC; }
    size_t lengthV() const override { return 3; }
    void writeV(L3Frame& dest, size_t& wp) const override;
    void parseV(const L3Frame& src, size_t& rp) override;
    void parseV(const L3Frame& src, size_t& rp, size_t) override;
    void text(std::ostream& os) const override;
};

// ── Power Command (GSM 04.08 10.5.2.5) ────────────────────────────────

class L3PowerCommand : public L3ProtocolElement {
private:
    int mPowerLevel;
public:
    explicit L3PowerCommand(int wLevel = 0) : mPowerLevel(wLevel) {}
    int PowerLevel() const { return mPowerLevel; }
    size_t lengthV() const override { return 1; }
    void writeV(L3Frame& dest, size_t& wp) const override;
    void parseV(const L3Frame& src, size_t& rp) override;
    void parseV(const L3Frame& src, size_t& rp, size_t) override;
    void text(std::ostream& os) const override;
};

// ── Cell Options (GSM 04.08 10.5.2.6) ─────────────────────────────────

class L3CellOptions : public L3ProtocolElement {
private:
    unsigned mRevisionLevel;
    unsigned mCBCH;
    unsigned mEnhancedRACH;
    unsigned mCellReselectionPriority;
    std::vector<uint8_t> mRawData;
public:
    L3CellOptions();
    unsigned RevisionLevel() const { return mRevisionLevel; }
    bool CBCH() const { return mCBCH; }
    bool EnhancedRACH() const { return mEnhancedRACH; }
    unsigned CellReselectionPriority() const { return mCellReselectionPriority; }
    size_t lengthV() const override;
    void writeV(L3Frame& dest, size_t& wp) const override;
    void parseV(const L3Frame& src, size_t& rp) override;
    void parseV(const L3Frame& src, size_t& rp, size_t expectedLength) override;
    void text(std::ostream& os) const override;
};

// ── Cell Selection (GSM 04.08 10.5.2.7) ───────────────────────────────

class L3CellSelection : public L3ProtocolElement {
private:
    unsigned mRxLevAccessMin;
    unsigned mRxLevelAccessMin;
    unsigned mMaxRxLev;
    unsigned mCellReselectionHysteresis;
    unsigned mCellReselectionOffset;
    unsigned mCellReservedIndicator;
    unsigned mCellBarQualifier;
    unsigned mCellBarQualifierLength;
    std::vector<unsigned char> mCellBarQualifier;
public:
    L3CellSelection();
    unsigned RxLevAccessMin() const { return mRxLevAccessMin; }
    unsigned MaxRxLev() const { return mMaxRxLev; }
    unsigned CellReselectionHysteresis() const { return mCellReselectionHysteresis; }
    unsigned CellReselectionOffset() const { return mCellReselectionOffset; }
    size_t lengthV() const override;
    void writeV(L3Frame& dest, size_t& wp) const override;
    void parseV(const L3Frame& src, size_t& rp) override;
    void parseV(const L3Frame& src, size_t& rp, size_t) override;
    void text(std::ostream& os) const override;
};

// ── RACH Control Parameters (GSM 04.08 10.5.2.8) ──────────────────────

class L3RACHControlParameters : public L3ProtocolElement {
private:
    unsigned mMaxRepetition;
    unsigned mInitialRepeat;
    unsigned mMaxDelay;
    unsigned mAccessBarredforRACH;
    unsigned mRACHControlValue;
public:
    L3RACHControlParameters(unsigned wMaxRep = 0, unsigned wInitRep = 0,
                             unsigned wMaxDelay = 0, bool wBarred = false,
                             unsigned wRACHCtrl = 0);
    unsigned MaxRepetition() const { return mMaxRepetition; }
    unsigned InitialRepeat() const { return mInitialRepeat; }
    unsigned MaxDelay() const { return mMaxDelay; }
    bool AccessBarred() const { return mAccessBarredforRACH; }
    unsigned RACHControlValue() const { return mRACHControlValue; }
    size_t lengthV() const override { return 2; }
    void writeV(L3Frame& dest, size_t& wp) const override;
    void parseV(const L3Frame& src, size_t& rp) override;
    void parseV(const L3Frame& src, size_t& rp, size_t) override;
    void text(std::ostream& os) const override;
};

// ── Immediate Assignment Information (GSM 04.08 10.5.2.9) ─────────────

class L3ImmediateAssignmentInformation : public L3ProtocolElement {
private:
    unsigned mPowerOffset;
    unsigned mPowerOffsetLength;
    std::vector<unsigned char> mPowerOffsetData;
public:
    L3ImmediateAssignmentInformation();
    unsigned PowerOffset() const { return mPowerOffset; }
    size_t lengthV() const override;
    void writeV(L3Frame& dest, size_t& wp) const override;
    void parseV(const L3Frame& src, size_t& rp) override;
    void parseV(const L3Frame& src, size_t& rp, size_t expectedLength) override;
    void text(std::ostream& os) const override;
};

// ── Additional Channel Description (GSM 04.08 10.5.2.10) ──────────────

class L3AdditionalChannelDescription : public L3ProtocolElement {
private:
    ChannelType mChannelType;
    unsigned mARfcn;
    unsigned mBSIC;
public:
    L3AdditionalChannelDescription(ChannelType wType = ChannelType::SDCCHType,
                                    unsigned wARfcn = 0, unsigned wBSIC = 0);
    ChannelType ChannelType() const { return mChannelType; }
    unsigned ARfcn() const { return mARfcn; }
    unsigned BSIC() const { return mBSIC; }
    size_t lengthV() const override { return 3; }
    void writeV(L3Frame& dest, size_t& wp) const override;
    void parseV(const L3Frame& src, size_t& rp) override;
    void parseV(const L3Frame& src, size_t& rp, size_t) override;
    void text(std::ostream& os) const override;
};

} // namespace gsml3parser

#endif // GSML3PARSER_COMMON_L3COMMON_H
