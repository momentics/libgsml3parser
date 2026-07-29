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
    void writeV(L3Frame& dest, size_t& wp) const override;
    void parseV(const L3Frame& src, size_t& rp) override { mCIValue = src.readField(rp, 4) & 0x0f; }
    void parseV(const L3Frame&, size_t&, size_t) override { throw ParseError("parseV not valid"); }
    void text(std::ostream& os) const override;
};

// ── Frequency List (GSM 04.08 10.5.2.13) ──────────────────────────────
// Variable bit-map format, 16 bytes fixed.

class L3FrequencyList : public L3ProtocolElement {
protected:
    std::vector<unsigned> mARFCNs;
public:
    L3FrequencyList();
    explicit L3FrequencyList(const std::vector<unsigned>& wARFCNs);
    void ARFCNs(const std::vector<unsigned>& wARFCNs) { mARFCNs = wARFCNs; }
    const std::vector<unsigned>& ARFCNs() const { return mARFCNs; }
    size_t lengthV() const override { return 16; }
    void writeV(L3Frame& dest, size_t& wp) const override;
    void parseV(const L3Frame& src, size_t& rp) override;
    void parseV(const L3Frame& src, size_t& rp, size_t) override;
    void text(std::ostream& os) const override;
private:
    unsigned base() const;
    unsigned spread() const;
    bool contains(unsigned wARFCN) const;
};

// ── Cell Channel Description (GSM 04.08 10.5.2.1b) ────────────────────
// Per-cell entry: ARFCN(10), BSIC(6), channelSpacing(1), spare(1) = 20 bits

class L3CellChannelDescription : public L3ProtocolElement {
private:
    unsigned mARfcn;
    unsigned mBSIC;
    unsigned mChannelSpacing;
public:
    L3CellChannelDescription();
    L3CellChannelDescription(unsigned wARfcn, unsigned wBSIC, unsigned wSpacing);
    unsigned ARFCN() const { return mARfcn; }
    unsigned BSIC() const { return mBSIC; }
    unsigned channelSpacing() const { return mChannelSpacing; }
    size_t lengthV() const override { return 3; }
    void writeV(L3Frame& dest, size_t& wp) const override;
    void parseV(const L3Frame& src, size_t& rp) override;
    void parseV(const L3Frame& src, size_t& rp, size_t) override;
    void text(std::ostream& os) const override;
};

// ── BCCH Frequency List (GSM 04.08 10.5.2.22) ────────────────────────
// Same bitmap encoding as L3FrequencyList, used in SI2/SI7/SI8.

class L3BCCHFrequencyList : public L3FrequencyList {
public:
    L3BCCHFrequencyList();
    explicit L3BCCHFrequencyList(const std::vector<unsigned>& wARFCNs);
    void writeV(L3Frame& dest, size_t& wp) const override;
    void text(std::ostream& os) const override;
};

// ── Neighbor Cells Description (GSM 04.08 10.5.2.22) ──────────────────
// Same bitmap encoding as L3FrequencyList, adds EXT-IND and BA-IND bits.

class L3NeighborCellsDescription : public L3FrequencyList {
public:
    L3NeighborCellsDescription();
    explicit L3NeighborCellsDescription(const std::vector<unsigned>& neighbors);
    void writeV(L3Frame& dest, size_t& wp) const override;
    void text(std::ostream& os) const override;
};

// ── Control Channel Description (GSM 04.08 10.5.2.11) ─────────────────

class L3ControlChannelDescription : public L3ProtocolElement {
public:
    unsigned mATT;
    unsigned mBS_AG_BLKS_RES;
    unsigned mCCCH_CONF;
    unsigned mBS_PA_MFRMS;
    unsigned mT3212;
    L3ControlChannelDescription();
    bool isCCCHCombined() { return mCCCH_CONF == 1; }
    size_t lengthV() const override { return 3; }
    void writeV(L3Frame& dest, size_t& wp) const override;
    void parseV(const L3Frame& src, size_t& rp) override;
    void parseV(const L3Frame& src, size_t& rp, size_t) override;
    void text(std::ostream& os) const override;
};

// ── Channel Description (GSM 04.08 10.5.2.5) ──────────────────────────

class L3ChannelDescription : public L3ProtocolElement {
private:
    TypeAndOffset mTypeAndOffset;
    unsigned mTN;
    unsigned mTSC;
    unsigned mHFlag;
    unsigned mARFCN;
    unsigned mMAIO;
    unsigned mHSN;
public:
    L3ChannelDescription();
    L3ChannelDescription(TypeAndOffset wTypeAndOffset, unsigned wTN,
                         unsigned wTSC, unsigned wARFCN);
    bool initialized() const { return mTypeAndOffset != TDMA_MISC; }
    TypeAndOffset typeAndOffset() const { return mTypeAndOffset; }
    unsigned TN() const { return mTN; }
    unsigned TSC() const { return mTSC; }
    unsigned ARFCN() const { return mARFCN; }
    unsigned MAIO() const { return mMAIO; }
    unsigned HSN() const { return mHSN; }
    size_t lengthV() const override { return 3; }
    void writeV(L3Frame& dest, size_t& wp) const override;
    void parseV(const L3Frame& src, size_t& rp) override;
    void parseV(const L3Frame& src, size_t& rp, size_t) override;
    void text(std::ostream& os) const override;
};

// ── Channel Description 2 (GSM 44.018 10.5.2.5a) ──────────────────────

class L3ChannelDescription2 : public L3ChannelDescription {
public:
    L3ChannelDescription2();
    L3ChannelDescription2(TypeAndOffset wTypeAndOffset, unsigned wTN,
                          unsigned wTSC, unsigned wARFCN);
    L3ChannelDescription2(const L3ChannelDescription& other);
};

// ── Additional Channel Description ─────────────────────────────────────

class L3AdditionalChannelDescription : public L3ProtocolElement {
private:
    TypeAndOffset mTypeAndOffset;
    unsigned mTN;
    unsigned mTSC;
    unsigned mHFlag;
    unsigned mARFCN;
    unsigned mMAIO;
    unsigned mHSN;
public:
    L3AdditionalChannelDescription();
    L3AdditionalChannelDescription(TypeAndOffset wTypeAndOffset, unsigned wTN,
                                    unsigned wTSC, unsigned wARFCN);
    bool initialized() const { return mTypeAndOffset != TDMA_MISC; }
    TypeAndOffset typeAndOffset() const { return mTypeAndOffset; }
    unsigned TN() const { return mTN; }
    unsigned TSC() const { return mTSC; }
    unsigned ARFCN() const { return mARFCN; }
    unsigned MAIO() const { return mMAIO; }
    unsigned HSN() const { return mHSN; }
    size_t lengthV() const override { return 3; }
    void writeV(L3Frame& dest, size_t& wp) const override;
    void parseV(const L3Frame& src, size_t& rp) override;
    void parseV(const L3Frame& src, size_t& rp, size_t) override;
    void text(std::ostream& os) const override;
};

// ── Power Command (GSM 04.08 10.5.2.28) ───────────────────────────────

class L3PowerCommand : public L3ProtocolElement {
private:
    unsigned mCommand;
public:
    explicit L3PowerCommand(unsigned wCommand = 0);
    unsigned command() const { return mCommand; }
    size_t lengthV() const override { return 1; }
    void writeV(L3Frame& dest, size_t& wp) const override;
    void parseV(const L3Frame& src, size_t& rp) override;
    void parseV(const L3Frame& src, size_t& rp, size_t) override;
    void text(std::ostream& os) const override;
};

// ── Power Command And Access Type (GSM 04.08 10.5.2.28a) ──────────────

class L3PowerCommandAndAccessType : public L3PowerCommand {
public:
    explicit L3PowerCommandAndAccessType(unsigned wCommand = 0);
};

// ── Channel Mode (GSM 04.08 10.5.2.6) ─────────────────────────────────

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
    bool operator==(const L3ChannelMode& other) const { return mMode == other.mMode; }
    bool operator!=(const L3ChannelMode& other) const { return mMode != other.mMode; }
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
    size_t lengthV() const override { return 0; }
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
    size_t lengthV() const override { return 0; }
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
    size_t lengthV() const override { return 0; }  // 1/2 octet field
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

// ── RACH Control Parameters (GSM 04.08 10.5.2.29) ─────────────────────

class L3RACHControlParameters : public L3ProtocolElement {
private:
    unsigned mMaxRetrans;
    unsigned mTxInteger;
    unsigned mCellBarAccess;
    unsigned mRE;
    uint16_t mAC;
public:
    L3RACHControlParameters();
    unsigned MaxRetrans() const { return mMaxRetrans; }
    unsigned TxInteger() const { return mTxInteger; }
    bool CellBarAccess() const { return mCellBarAccess; }
    unsigned RE() const { return mRE; }
    uint16_t AC() const { return mAC; }
    size_t lengthV() const override { return 3; }
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
    size_t lengthV() const override { return 0; }
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
    size_t lengthV() const override { return 0; }
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
    size_t lengthV() const override { return 0; }
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

// ── Cell Selection ─────────────────────────────────────────────────────

class L3CellSelection : public L3ProtocolElement {
private:
    unsigned mRxLevAccessMin;
    unsigned mRxLevelAccessMin;
    unsigned mMaxRxLev;
    unsigned mCellReselectionHysteresis;
    unsigned mCellReselectionOffset;
    unsigned mCellReservedIndicator;
    unsigned mCellBarQualifierValue;
    unsigned mCellBarQualifierLength;
    std::vector<unsigned char> mCellBarQualifierData;
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

// ── Rest Octets Base ───────────────────────────────────────────────────

class L3RestOctets : public L3ProtocolElement {
public:
    L3RestOctets() {}
    virtual ~L3RestOctets() = default;
    size_t lengthV() const override { return 0; }
    void writeV(L3Frame&, size_t&) const override {}
    void parseV(const L3Frame&, size_t&) override {}
    void parseV(const L3Frame&, size_t&, size_t) override {}
};

// ── SI3 Rest Octets (GSM 04.08 10.5.2.34) ─────────────────────────────

class L3SI3RestOctets : public L3RestOctets {
private:
    bool mHaveSI3RestOctets;
    bool mHaveSelectionParameters;
    bool mCBQ;
    unsigned mCELL_RESELECT_OFFSET;
    unsigned mTEMPORARY_OFFSET;
    unsigned mPENALTY_TIME;
    unsigned mRA_COLOUR;
    bool mHaveGPRS;
public:
    L3SI3RestOctets();
    bool hasSI3RestOctets() const { return mHaveSI3RestOctets; }
    bool hasGPRS() const { return mHaveGPRS; }
    size_t lengthV() const override;
    void writeV(L3Frame& dest, size_t& wp) const override;
    void parseV(const L3Frame& src, size_t& rp) override;
    void parseV(const L3Frame& src, size_t& rp, size_t) override;
    void text(std::ostream& os) const override;
};

// ── SI4 Rest Octets ────────────────────────────────────────────────────

class L3SIType4RestOctets : public L3RestOctets {
private:
    unsigned mRA_COLOUR;
    bool mHaveGPRS;
public:
    L3SIType4RestOctets();
    size_t lengthV() const override;
    void writeV(L3Frame& dest, size_t& wp) const override;
    void parseV(const L3Frame& src, size_t& rp) override;
    void parseV(const L3Frame& src, size_t& rp, size_t) override;
    void text(std::ostream& os) const override;
};

// ── IA Rest Octets ─────────────────────────────────────────────────────

class L3IARestOctets : public GenericMessageElement {
private:
    bool mHavePacketAssignment;
public:
    L3IARestOctets();
    size_t lengthBits() const override;
    void writeBits(L3Frame& dest, size_t& wp) const override;
    void text(std::ostream& os) const override;
};

// ── SI13 Rest Octets (GSM 04.08 10.5.2.37b) ───────────────────────────

class L3GPRSCellOptions : public GenericMessageElement {
private:
    unsigned mNMO;
    unsigned mT3168;
    unsigned mT3192;
    unsigned mDRX_TIMER_MAX;
    unsigned mACCESS_BURST_TYPE;
    unsigned mCONTROL_ACK_TYPE;
    unsigned mBS_VC_MAX;
public:
    L3GPRSCellOptions();
    size_t lengthBits() const;
    void writeBits(L3Frame& dest, size_t& wp) const override;
    void text(std::ostream& os) const override;
};

class L3GPRSSI13PowerControlParameters : public GenericMessageElement {
private:
    unsigned mALPHA;
public:
    L3GPRSSI13PowerControlParameters();
    size_t lengthBits() const;
    void writeBits(L3Frame& dest, size_t& wp) const override;
    void text(std::ostream& os) const override;
};

class L3SI13RestOctets : public L3RestOctets {
private:
    unsigned mRAC;
    bool mSPGC_CCCH_SUP;
    unsigned mPRIORITY_ACCESS_THR;
    unsigned mNETWORK_CONTROL_ORDER;
    L3GPRSCellOptions mCellOptions;
    L3GPRSSI13PowerControlParameters mPowerControlParameters;
public:
    L3SI13RestOctets();
    size_t lengthV() const override;
    void writeV(L3Frame& dest, size_t& wp) const override;
    void parseV(const L3Frame& src, size_t& rp) override;
    void parseV(const L3Frame& src, size_t& rp, size_t) override;
    void text(std::ostream& os) const override;
};

// ── Follow On Proceed (GSM 04.08) ─────────────────────────────────────

class L3FollowOnProceed : public L3ProtocolElement {
public:
    size_t lengthV() const override { return 1; }
    void writeV(L3Frame& dest, size_t& wp) const override;
    void parseV(const L3Frame& src, size_t& rp) override;
    void parseV(const L3Frame& src, size_t& rp, size_t) override;
};

} // namespace gsml3parser

#endif // GSML3PARSER_COMMON_L3COMMON_H
