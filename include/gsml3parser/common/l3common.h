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

// ── Channel Mode (GSM 04.08 10.5.2.6) ──────────────────────────────────

class L3ChannelMode : public L3ProtocolElement {
public:
    enum Mode : uint8_t {
        SignallingOnly = 0,
        SpeechV1 = 1,
        SpeechV2 = 2,
        SpeechV3 = 3
    };
private:
    Mode mMode;
public:
    explicit L3ChannelMode(Mode wMode = SignallingOnly);
    Mode mode() const { return mMode; }
    bool isAMR() const { return mMode == SpeechV3; }
    size_t lengthV() const override { return 1; }
    void writeV(L3Frame& dest, size_t& wp) const override;
    void parseV(const L3Frame& src, size_t& rp) override;
    void parseV(const L3Frame& src, size_t& rp, size_t) override;
    void text(std::ostream& os) const override;
};

// ── Timing Advance (GSM 04.08 10.5.2.40) ───────────────────────────────

class L3TimingAdvance : public L3ProtocolElement {
private:
    unsigned mTimingAdvance;
public:
    explicit L3TimingAdvance(unsigned wTA = 0);
    unsigned timingAdvance() const { return mTimingAdvance; }
    size_t lengthV() const override { return 1; }
    void writeV(L3Frame& dest, size_t& wp) const override;
    void parseV(const L3Frame& src, size_t& rp) override;
    void parseV(const L3Frame& src, size_t& rp, size_t) override;
    void text(std::ostream& os) const override;
};

// ── Cell Description (GSM 04.08 10.5.2.2) ──────────────────────────────

class L3CellDescription : public L3ProtocolElement {
private:
    unsigned mARFCN;
    unsigned mNCC;
    unsigned mBCC;
public:
    L3CellDescription(unsigned wARFCN = 0, unsigned wNCC = 0, unsigned wBCC = 0);
    unsigned ARFCN() const { return mARFCN; }
    unsigned NCC() const { return mNCC; }
    unsigned BCC() const { return mBCC; }
    size_t lengthV() const override { return 2; }
    void writeV(L3Frame& dest, size_t& wp) const override;
    void parseV(const L3Frame& src, size_t& rp) override;
    void parseV(const L3Frame& src, size_t& rp, size_t) override;
    void text(std::ostream& os) const override;
};

// ── Handover Reference (GSM 04.08 10.5.2.15) ──────────────────────────

class L3HandoverReference : public L3ProtocolElement {
private:
    unsigned mValue;
public:
    explicit L3HandoverReference(unsigned wValue = 0);
    unsigned value() const { return mValue; }
    size_t lengthV() const override { return 1; }
    void writeV(L3Frame& dest, size_t& wp) const override;
    void parseV(const L3Frame& src, size_t& rp) override;
    void parseV(const L3Frame& src, size_t& rp, size_t) override;
    void text(std::ostream& os) const override;
};

// ── Ciphering Mode Setting (GSM 04.08 10.5.2.9) ───────────────────────

class L3CipheringModeSetting : public L3ProtocolElement {
private:
    bool mCiphering;
    int mAlgorithm;
public:
    L3CipheringModeSetting(bool wCiphering = false, int wAlgorithm = 0);
    bool ciphering() const { return mCiphering; }
    int algorithm() const { return mAlgorithm; }
    size_t lengthV() const override { return 1; }
    void writeV(L3Frame& dest, size_t& wp) const override;
    void parseV(const L3Frame& src, size_t& rp) override;
    void parseV(const L3Frame& src, size_t& rp, size_t) override;
    void text(std::ostream& os) const override;
};

// ── Ciphering Mode Response (GSM 04.08 10.5.2.10) ─────────────────────

class L3CipheringModeResponse : public L3ProtocolElement {
private:
    bool mIncludeIMEISV;
public:
    explicit L3CipheringModeResponse(bool wIncludeIMEISV = false);
    bool includeIMEISV() const { return mIncludeIMEISV; }
    size_t lengthV() const override { return 1; }
    void writeV(L3Frame& dest, size_t& wp) const override;
    void parseV(const L3Frame& src, size_t& rp) override;
    void parseV(const L3Frame& src, size_t& rp, size_t) override;
    void text(std::ostream& os) const override;
};

// ── Synchronization Indication (GSM 04.08 10.5.2.39) ──────────────────

class L3SynchronizationIndication : public L3ProtocolElement {
private:
    bool mNCI;
    bool mROT;
    int mSI;
public:
    L3SynchronizationIndication(bool wNCI = false, bool wROT = false, int wSI = 0);
    bool NCI() const { return mNCI; }
    bool ROT() const { return mROT; }
    int SI() const { return mSI; }
    size_t lengthV() const override { return 1; }
    void writeV(L3Frame& dest, size_t& wp) const override;
    void parseV(const L3Frame& src, size_t& rp) override;
    void parseV(const L3Frame& src, size_t& rp, size_t) override;
    void text(std::ostream& os) const override;
};

// ── NCC Permitted (GSM 04.08 10.5.2.27) ───────────────────────────────

class L3NCCPermitted : public L3ProtocolElement {
private:
    unsigned mPermitted;
public:
    explicit L3NCCPermitted(unsigned wPermitted = 0xff);
    unsigned permitted() const { return mPermitted; }
    size_t lengthV() const override { return 1; }
    void writeV(L3Frame& dest, size_t& wp) const override;
    void parseV(const L3Frame& src, size_t& rp) override;
    void parseV(const L3Frame& src, size_t& rp, size_t) override;
    void text(std::ostream& os) const override;
};

// ── Page Mode (GSM 04.08 10.5.2.26) ───────────────────────────────────

class L3PageMode : public L3ProtocolElement {
private:
    unsigned mPageMode;
public:
    explicit L3PageMode(unsigned wPageMode = 0);
    unsigned pageMode() const { return mPageMode; }
    size_t lengthV() const override { return 1; }
    void writeV(L3Frame& dest, size_t& wp) const override;
    void parseV(const L3Frame& src, size_t& rp) override;
    void parseV(const L3Frame& src, size_t& rp, size_t) override;
    void text(std::ostream& os) const override;
};

// ── Request Reference (GSM 04.08 10.5.2.30) ───────────────────────────

class L3RequestReference : public L3ProtocolElement {
private:
    unsigned mRA;
    unsigned mT1p;
    unsigned mT2;
    unsigned mT3;
public:
    L3RequestReference();
    L3RequestReference(unsigned wRA, unsigned wT1p, unsigned wT2, unsigned wT3);
    unsigned RA() const { return mRA; }
    unsigned T1p() const { return mT1p; }
    unsigned T2() const { return mT2; }
    unsigned T3() const { return mT3; }
    size_t lengthV() const override { return 3; }
    void writeV(L3Frame& dest, size_t& wp) const override;
    void parseV(const L3Frame& src, size_t& rp) override;
    void parseV(const L3Frame& src, size_t& rp, size_t) override;
    void text(std::ostream& os) const override;
};

// ── Wait Indication (GSM 04.08 10.5.2.43) ─────────────────────────────

class L3WaitIndication : public L3ProtocolElement {
private:
    unsigned mValue;
public:
    explicit L3WaitIndication(unsigned seconds = 0);
    unsigned value() const { return mValue; }
    size_t lengthV() const override { return 1; }
    void writeV(L3Frame& dest, size_t& wp) const override;
    void parseV(const L3Frame& src, size_t& rp) override;
    void parseV(const L3Frame& src, size_t& rp, size_t) override;
    void text(std::ostream& os) const override;
};

// ── RRCause Element (GSM 04.08 10.5.2.31) ─────────────────────────────

class L3RRCauseElement : public L3ProtocolElement {
private:
    RRCause mCauseValue;
public:
    explicit L3RRCauseElement(RRCause wValue = RRCause::Normal_Event);
    RRCause causeValue() const { return mCauseValue; }
    size_t lengthV() const override { return 1; }
    void writeV(L3Frame& dest, size_t& wp) const override;
    void parseV(const L3Frame& src, size_t& rp) override;
    void parseV(const L3Frame& src, size_t& rp, size_t) override;
    void text(std::ostream& os) const override;
};

// ── Cell Options BCCH (GSM 04.08 10.5.2.3) ────────────────────────────

class L3CellOptionsBCCH : public L3ProtocolElement {
private:
    unsigned mPWRC;
    unsigned mDTX;
    unsigned mRADIO_LINK_TIMEOUT;
public:
    L3CellOptionsBCCH();
    unsigned PWRC() const { return mPWRC; }
    unsigned DTX() const { return mDTX; }
    unsigned RADIO_LINK_TIMEOUT() const { return mRADIO_LINK_TIMEOUT; }
    size_t lengthV() const override { return 1; }
    void writeV(L3Frame& dest, size_t& wp) const override;
    void parseV(const L3Frame& src, size_t& rp) override;
    void parseV(const L3Frame& src, size_t& rp, size_t) override;
    void text(std::ostream& os) const override;
};

// ── Cell Options SACCH (GSM 04.08 10.5.2.3a) ──────────────────────────

class L3CellOptionsSACCH : public L3ProtocolElement {
private:
    unsigned mPWRC;
    unsigned mDTX;
    unsigned mRADIO_LINK_TIMEOUT;
public:
    L3CellOptionsSACCH();
    unsigned PWRC() const { return mPWRC; }
    unsigned DTX() const { return mDTX; }
    unsigned RADIO_LINK_TIMEOUT() const { return mRADIO_LINK_TIMEOUT; }
    size_t lengthV() const override { return 1; }
    void writeV(L3Frame& dest, size_t& wp) const override;
    void parseV(const L3Frame& src, size_t& rp) override;
    void parseV(const L3Frame& src, size_t& rp, size_t) override;
    void text(std::ostream& os) const override;
};

// ── Cell Selection Parameters (GSM 04.08 10.5.2.4) ────────────────────

class L3CellSelectionParameters : public L3ProtocolElement {
private:
    unsigned mACS;
    unsigned mNECI;
    unsigned mCELL_RESELECT_HYSTERESIS;
    unsigned mMS_TXPWR_MAX_CCH;
    unsigned mRXLEV_ACCESS_MIN;
public:
    L3CellSelectionParameters();
    unsigned ACS() const { return mACS; }
    unsigned NECI() const { return mNECI; }
    unsigned CELL_RESELECT_HYSTERESIS() const { return mCELL_RESELECT_HYSTERESIS; }
    unsigned MS_TXPWR_MAX_CCH() const { return mMS_TXPWR_MAX_CCH; }
    unsigned RXLEV_ACCESS_MIN() const { return mRXLEV_ACCESS_MIN; }
    size_t lengthV() const override { return 2; }
    void writeV(L3Frame& dest, size_t& wp) const override;
    void parseV(const L3Frame& src, size_t& rp) override;
    void parseV(const L3Frame& src, size_t& rp, size_t) override;
    void text(std::ostream& os) const override;
};

// ── Neighbor Cells Description (GSM 04.08 10.5.2.22) ──────────────────

class L3NeighborCellsDescription : public L3ProtocolElement {
private:
    std::vector<unsigned> mARFCNs;
public:
    L3NeighborCellsDescription();
    explicit L3NeighborCellsDescription(const std::vector<unsigned>& neighbors);
    const std::vector<unsigned>& ARFCNs() const { return mARFCNs; }
    size_t lengthV() const override;
    void writeV(L3Frame& dest, size_t& wp) const override;
    void parseV(const L3Frame& src, size_t& rp) override;
    void parseV(const L3Frame& src, size_t& rp, size_t) override;
    void text(std::ostream& os) const override;
};

// ── Measurement Results (GSM 04.08 10.5.2.20) ─────────────────────────

class L3MeasurementResults : public L3ProtocolElement {
private:
    bool mBA_USED;
    bool mDTX_USED;
    bool mMEAS_VALID;
    unsigned mRXLEV_FULL_SERVING_CELL;
    unsigned mRXLEV_SUB_SERVING_CELL;
    unsigned mRXQUAL_FULL_SERVING_CELL;
    unsigned mRXQUAL_SUB_SERVING_CELL;
    unsigned mNO_NCELL;
    unsigned mRXLEV_NCELL[6];
    unsigned mBCCH_FREQ_NCELL[6];
    unsigned mBSIC_NCELL[6];
public:
    L3MeasurementResults();
    bool BA_USED() const { return mBA_USED; }
    bool DTX_USED() const { return mDTX_USED; }
    bool isServingCellValid() const { return mMEAS_VALID == 0; }
    unsigned RXLEV_FULL_SERVING_CELL_RAW() const { return mRXLEV_FULL_SERVING_CELL; }
    unsigned RXLEV_SUB_SERVING_CELL_RAW() const { return mRXLEV_SUB_SERVING_CELL; }
    unsigned RXQUAL_FULL_SERVING_CELL() const { return mRXQUAL_FULL_SERVING_CELL; }
    unsigned RXQUAL_SUB_SERVING_CELL() const { return mRXQUAL_SUB_SERVING_CELL; }
    unsigned NO_NCELL() const { return mNO_NCELL; }
    unsigned RXLEV_NCELL_RAW(unsigned i) const { return mRXLEV_NCELL[i]; }
    unsigned BCCH_FREQ_NCELL(unsigned i) const { return mBCCH_FREQ_NCELL[i]; }
    unsigned BSIC_NCELL(unsigned i) const { return mBSIC_NCELL[i]; }
    int decodeLevToDBm(unsigned lev) const;
    float decodeQualToBER(unsigned qual) const;
    size_t lengthV() const override { return 16; }
    void writeV(L3Frame& dest, size_t& wp) const override;
    void parseV(const L3Frame& src, size_t& rp) override;
    void parseV(const L3Frame& src, size_t& rp, size_t) override;
    void text(std::ostream& os) const override;
};

// ── Multi Rate Configuration (3GPP 44.018 10.5.2.21aa) ────────────────

class L3MultiRateConfiguration : public L3ProtocolElement {
public:
    enum AmrCodecSet : uint8_t {
        codec_set_AMR_FR = 0x80,
        codec_set_AMR_HR = 0x10,
        codec_set_UMTS_AMR = 0x85
    };
private:
    unsigned mOptions;
    AmrCodecSet mAmrCodecSet;
public:
    explicit L3MultiRateConfiguration(bool halfrate = false);
    unsigned options() const { return mOptions; }
    AmrCodecSet amrCodecSet() const { return mAmrCodecSet; }
    size_t lengthV() const override { return 2; }
    void writeV(L3Frame& dest, size_t& wp) const override;
    void parseV(const L3Frame& src, size_t& rp) override;
    void parseV(const L3Frame& src, size_t& rp, size_t) override;
    void text(std::ostream& os) const override;
};

// ── APDUID (GSM 04.08 10.5.2.48) ──────────────────────────────────────

class L3APDUID : public L3ProtocolElement {
private:
    unsigned mProtocolIdentifier;
public:
    explicit L3APDUID(unsigned protocolIdentifier = 0);
    unsigned protocolIdentifier() const { return mProtocolIdentifier; }
    size_t lengthV() const override { return 1; }
    void writeV(L3Frame& dest, size_t& wp) const override;
    void parseV(const L3Frame& src, size_t& rp) override;
    void parseV(const L3Frame& src, size_t& rp, size_t) override;
    void text(std::ostream& os) const override;
};

// ── APDU Flags (GSM 04.08 10.5.2.49) ──────────────────────────────────

class L3APDUFlags : public L3ProtocolElement {
private:
    unsigned mCR;
    unsigned mFirstSegment;
    unsigned mLastSegment;
public:
    L3APDUFlags(unsigned cr = 0, unsigned firstSegment = 0, unsigned lastSegment = 0);
    unsigned CR() const { return mCR; }
    unsigned firstSegment() const { return mFirstSegment; }
    unsigned lastSegment() const { return mLastSegment; }
    size_t lengthV() const override { return 1; }
    void writeV(L3Frame& dest, size_t& wp) const override;
    void parseV(const L3Frame& src, size_t& rp) override;
    void parseV(const L3Frame& src, size_t& rp, size_t) override;
    void text(std::ostream& os) const override;
};

// ── APDU Data (GSM 04.08 10.5.2.50) ───────────────────────────────────

class L3APDUData : public L3ProtocolElement {
private:
    BitVector mData;
public:
    L3APDUData();
    explicit L3APDUData(BitVector data);
    const BitVector& data() const { return mData; }
    size_t lengthV() const override;
    void writeV(L3Frame& dest, size_t& wp) const override;
    void parseV(const L3Frame& src, size_t& rp) override;
    void parseV(const L3Frame& src, size_t& rp, size_t expectedLength) override;
    void text(std::ostream& os) const override;
};

// ── Dedicated Mode Or TBF (GSM 04.08 10.5.2.25b) ──────────────────────

class L3DedicatedModeOrTBF : public L3ProtocolElement {
private:
    unsigned mDownlink;
    unsigned mTMA;
    unsigned mDMOrTBF;
public:
    L3DedicatedModeOrTBF(bool forTBF = false, bool wDownlink = false);
    bool isDownlink() const { return mDownlink; }
    bool isTBF() const { return mDMOrTBF; }
    size_t lengthV() const override { return 1; }
    void writeV(L3Frame& dest, size_t& wp) const override;
    void parseV(const L3Frame& src, size_t& rp) override;
    void parseV(const L3Frame& src, size_t& rp, size_t) override;
    void text(std::ostream& os) const override;
};

} // namespace gsml3parser

#endif // GSML3PARSER_COMMON_L3COMMON_H
