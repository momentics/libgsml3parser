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

#ifndef GSML3PARSER_RR_L3RRMESSAGES_H
#define GSML3PARSER_RR_L3RRMESSAGES_H

#include <cstdint>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "../l3message.h"
#include "../l3frame.h"
#include "../types.h"
#include "../common/l3common.h"

namespace gsml3parser {

// ── L3RRMessage ─────────────────────────────────────────────────────────

class L3RRMessage : public L3Message {
public:
    enum MessageType : int {
        SystemInformationType1  = 0x19,
        SystemInformationType2  = 0x1a,
        SystemInformationType2bis = 0x02,
        SystemInformationType2ter = 0x03,
        SystemInformationType3  = 0x1b,
        SystemInformationType4  = 0x1c,
        SystemInformationType5  = 0x1d,
        SystemInformationType5bis = 0x05,
        SystemInformationType5ter = 0x06,
        SystemInformationType6  = 0x1e,
        SystemInformationType7  = 0x1f,
        SystemInformationType8  = 0x18,
        SystemInformationType9  = 0x04,
        SystemInformationType13 = 0x00,
        SystemInformationType16 = 0x3d,
        SystemInformationType17 = 0x3e,
        AssignmentCommand       = 0x2e,
        AssignmentComplete      = 0x29,
        AssignmentFailure       = 0x2f,
        ChannelRelease          = 0x0d,
        ImmediateAssignment     = 0x3f,
        ImmediateAssignmentExtended = 0x39,
        ImmediateAssignmentReject = 0x3a,
        AdditionalAssignment    = 0x3b,
        PagingRequestType1      = 0x21,
        PagingRequestType2      = 0x22,
        PagingRequestType3      = 0x24,
        PagingResponse          = 0x27,
        HandoverCommand         = 0x2b,
        HandoverComplete        = 0x2c,
        HandoverFailure         = 0x28,
        PhysicalInformation     = 0x2d,
        CipheringModeCommand    = 0x35,
        CipheringModeComplete   = 0x32,
        ChannelModeModify       = 0x10,
        RRStatus                = 0x12,
        ChannelModeModifyAcknowledge = 0x17,
        ClassmarkChange         = 0x16,
        ClassmarkEnquiry        = 0x13,
        MeasurementReport       = 0x15,
        GPRSSuspensionRequest   = 0x34,
        SynchronizationChannelInformation = 0x100,
        ChannelRequest          = 0x101,
        HandoverAccess          = 0x102,
        ApplicationInformation  = 0x38
    };

    static const char* name(MessageType mt);

    L3PD PD() const override { return L3PD::RadioResource; }
    void text(std::ostream& os) const override;
};

std::ostream& operator<<(std::ostream& os, L3RRMessage::MessageType mt);

// ── L3RRMessageNRO (no rest octets) ─────────────────────────────────────

class L3RRMessageNRO : public L3RRMessage {
public:
    size_t fullBodyLength() const override { return l2BodyLength(); }
};

// ── L3RRMessageRO (with rest octets) ────────────────────────────────────

class L3RRMessageRO : public L3RRMessage {
public:
    virtual size_t restOctetsLength() const = 0;
    size_t fullBodyLength() const override { return l2BodyLength() + restOctetsLength(); }
};

// ── Paging Request Type 1 (GSM 04.08 9.1.22) ──────────────────────────

class L3PagingRequestType1 : public L3RRMessageNRO {
private:
    std::vector<L3MobileIdentity> mMobileIDs;
    ChannelType mChannelsNeeded[2];
public:
    L3PagingRequestType1();
    L3PagingRequestType1(const L3MobileIdentity& wId, ChannelType wType);
    L3PagingRequestType1(const L3MobileIdentity& wId1, ChannelType wType1,
                         const L3MobileIdentity& wId2, ChannelType wType2);

    int MTI() const override { return PagingRequestType1; }
    size_t l2BodyLength() const override;
    void writeBody(L3Frame& dest, size_t& wp) const override;
    void parseBody(const L3Frame& src, size_t& rp) override;
    void text(std::ostream& os) const override;
};

// ── Paging Response (GSM 04.08 9.1.25) ─────────────────────────────────

class L3PagingResponse : public L3RRMessageNRO {
private:
    L3MobileStationClassmark2 mClassmark;
    L3MobileIdentity mMobileID;
public:
    const L3MobileIdentity& mobileID() const { return mMobileID; }
    int MTI() const override { return PagingResponse; }
    size_t l2BodyLength() const override;
    void parseBody(const L3Frame& source, size_t& rp) override;
    void writeBody(L3Frame& dest, size_t& wp) const override;
    void text(std::ostream& os) const override;
};

// ── System Information Type 1 (GSM 04.08 9.1.31) ──────────────────────

class L3SystemInformationType1 : public L3RRMessageNRO {
private:
    L3FrequencyList mCellChannelDescription;
    L3RACHControlParameters mRACHControlParameters;
public:
    L3SystemInformationType1();
    int MTI() const override { return SystemInformationType1; }
    size_t l2BodyLength() const override { return 19; }
    void writeBody(L3Frame& dest, size_t& wp) const override;
    void parseBody(const L3Frame& src, size_t& rp) override;
    void text(std::ostream& os) const override;
    const L3FrequencyList& cellChannelDescription() const { return mCellChannelDescription; }
    const L3RACHControlParameters& rachaControl() const { return mRACHControlParameters; }
};

// ── Channel Release (GSM 04.08 9.1.7) ──────────────────────────────────

class L3ChannelRelease : public L3RRMessageNRO {
private:
    RRCause mCause;
    bool mGprsResumptionPresent;
    bool mGprsResumptionBit;
public:
    explicit L3ChannelRelease(RRCause cause = RRCause::Normal_Event)
        : mCause(cause), mGprsResumptionPresent(false), mGprsResumptionBit(false) {}
    int MTI() const override { return ChannelRelease; }
    size_t l2BodyLength() const override { return 1 + (mGprsResumptionPresent ? 1 : 0); }
    void writeBody(L3Frame& dest, size_t& wp) const override;
    void parseBody(const L3Frame& src, size_t& rp) override;
    void text(std::ostream& os) const override;
    RRCause cause() const { return mCause; }
    bool hasGprsResumption() const { return mGprsResumptionPresent; }
    bool gprsResumption() const { return mGprsResumptionBit; }
};

// ── RR Status (GSM 04.08 9.1.29) ──────────────────────────────────────

class L3RRStatus : public L3RRMessageNRO {
private:
    RRCause mCause;
public:
    RRCause cause() const { return mCause; }
    int MTI() const override { return RRStatus; }
    size_t l2BodyLength() const override { return 1; }
    void parseBody(const L3Frame& src, size_t& rp) override;
    void writeBody(L3Frame& dest, size_t& wp) const override;
    void text(std::ostream& os) const override;
};

// ── Assignment Command (GSM 04.08 9.1.2) ───────────────────────────────

class L3AssignmentCommand : public L3RRMessageNRO {
private:
    L3ChannelDescription mChannel;
    L3PowerCommand mPowerCommand;
    bool mHaveMode1;
    L3ChannelMode mMode1;
    L3MultiRateConfiguration mMultiRate;
public:
    L3AssignmentCommand();
    int MTI() const override { return AssignmentCommand; }
    size_t l2BodyLength() const override;
    void writeBody(L3Frame& dest, size_t& wp) const override;
    void parseBody(const L3Frame& src, size_t& rp) override;
    void text(std::ostream& os) const override;
    const L3ChannelDescription& channel() const { return mChannel; }
    const L3PowerCommand& powerCommand() const { return mPowerCommand; }
    bool hasMode1() const { return mHaveMode1; }
    const L3ChannelMode& mode1() const { return mMode1; }
    bool isAMR() const { return mHaveMode1 && mMode1.isAMR(); }
    const L3MultiRateConfiguration& multiRate() const { return mMultiRate; }
};

// ── Assignment Complete (GSM 04.08 9.1.3) ──────────────────────────────

class L3AssignmentComplete : public L3RRMessageNRO {
private:
    RRCause mCause;
public:
    RRCause cause() const { return mCause; }
    int MTI() const override { return AssignmentComplete; }
    size_t l2BodyLength() const override { return 1; }
    void parseBody(const L3Frame& src, size_t& rp) override;
    void writeBody(L3Frame& dest, size_t& wp) const override;
    void text(std::ostream& os) const override;
};

// ── Assignment Failure (GSM 04.08 9.1.3) ───────────────────────────────

class L3AssignmentFailure : public L3RRMessageNRO {
private:
    RRCause mCause;
public:
    RRCause cause() const { return mCause; }
    int MTI() const override { return AssignmentFailure; }
    size_t l2BodyLength() const override { return 1; }
    void parseBody(const L3Frame& src, size_t& rp) override;
    void writeBody(L3Frame& dest, size_t& wp) const override;
    void text(std::ostream& os) const override;
};

// ── Classmark Enquiry (GSM 04.08 9.1.14) ──────────────────────────────

class L3ClassmarkEnquiry : public L3RRMessageNRO {
public:
    int MTI() const override { return ClassmarkEnquiry; }
    size_t l2BodyLength() const override { return 0; }
    void writeBody(L3Frame&, size_t&) const override {}
    void text(std::ostream& os) const override;
};

// ── Classmark Change (GSM 04.08 9.1.11) ───────────────────────────────

class L3ClassmarkChange : public L3RRMessageNRO {
protected:
    L3MobileStationClassmark2 mClassmark;
    bool mHaveAdditionalClassmark;
    L3MobileStationClassmark3 mAdditionalClassmark;
public:
    int MTI() const override { return ClassmarkChange; }
    size_t l2BodyLength() const override;
    void parseBody(const L3Frame& src, size_t& rp) override;
    void writeBody(L3Frame& dest, size_t& wp) const override;
    void text(std::ostream& os) const override;
    const L3MobileStationClassmark2& classmark() const { return mClassmark; }
};

// ── Measurement Report (GSM 04.08 9.1.21) ─────────────────────────────

class L3MeasurementReport : public L3RRMessageNRO {
private:
    L3MeasurementResults mMeasurementResults;
public:
    int MTI() const override { return MeasurementReport; }
    size_t l2BodyLength() const override { return 16; }
    void parseBody(const L3Frame& src, size_t& rp) override;
    void writeBody(L3Frame& dest, size_t& wp) const override;
    void text(std::ostream& os) const override;
    const L3MeasurementResults& measurementResults() const { return mMeasurementResults; }
};

// ── Ciphering Mode Command (GSM 04.08 9.1.9) ──────────────────────────

class L3CipheringModeCommand : public L3RRMessageNRO {
protected:
    bool mCiphering;
    int mAlgorithm;
    L3CipheringModeResponse mCipheringModeResponse;
public:
    L3CipheringModeCommand(bool ciphering, int algorithm)
        : mCiphering(ciphering), mAlgorithm(algorithm), mCipheringModeResponse() {}
    int MTI() const override;
    size_t l2BodyLength() const override { return 1; }
    void writeBody(L3Frame&, size_t&) const override;
    void parseBody(const L3Frame& src, size_t& rp) override;
    void text(std::ostream& os) const override;
    bool includeIMEISV() const { return mCipheringModeResponse.includeIMEISV(); }
};

// ── Ciphering Mode Complete (GSM 04.08 9.1.10) ────────────────────────

class L3CipheringModeComplete : public L3RRMessageNRO {
public:
    int MTI() const override;
    size_t l2BodyLength() const override { return 0; }
    void parseBody(const L3Frame&, size_t&) override {}
    void text(std::ostream& os) const override;
};

// ── Handover Complete (GSM 04.08 9.1.16) ──────────────────────────────

class L3HandoverComplete : public L3RRMessageNRO {
protected:
    RRCause mCause;
public:
    int MTI() const override { return HandoverComplete; }
    size_t l2BodyLength() const override { return 1; }
    void parseBody(const L3Frame& src, size_t& rp) override;
    void writeBody(L3Frame& dest, size_t& wp) const override;
    void text(std::ostream& os) const override;
    RRCause cause() const { return mCause; }
};

// ── Handover Failure (GSM 04.08 9.1.17) ───────────────────────────────

class L3HandoverFailure : public L3RRMessageNRO {
protected:
    RRCause mCause;
public:
    int MTI() const override { return HandoverFailure; }
    size_t l2BodyLength() const override { return 1; }
    void parseBody(const L3Frame& src, size_t& rp) override;
    void writeBody(L3Frame& dest, size_t& wp) const override;
    void text(std::ostream& os) const override;
    RRCause cause() const { return mCause; }
};

// ── Channel Mode Modify (GSM 04.08 9.1.5) ─────────────────────────────

class L3ChannelModeModify : public L3RRMessageNRO {
private:
    L3ChannelDescription mDescription;
    L3ChannelMode mMode;
    L3MultiRateConfiguration mMultiRate;
public:
    L3ChannelModeModify();
    L3ChannelModeModify(const L3ChannelDescription& wDesc, const L3ChannelMode& wMode);
    int MTI() const override { return ChannelModeModify; }
    bool isAMR() const { return mMode.isAMR(); }
    size_t l2BodyLength() const override;
    void writeBody(L3Frame& dest, size_t& wp) const override;
    void parseBody(const L3Frame& src, size_t& rp) override;
    void text(std::ostream& os) const override;
    const L3ChannelDescription& description() const { return mDescription; }
    const L3ChannelMode& mode() const { return mMode; }
};

// ── Channel Mode Modify Acknowledge (GSM 04.08 9.1.6) ─────────────────

class L3ChannelModeModifyAcknowledge : public L3RRMessageNRO {
private:
    L3ChannelDescription mDescription;
    L3ChannelMode mMode;
public:
    int MTI() const override { return ChannelModeModifyAcknowledge; }
    size_t l2BodyLength() const override;
    void parseBody(const L3Frame& src, size_t& rp) override;
    void writeBody(L3Frame& dest, size_t& wp) const override;
    void text(std::ostream& os) const override;
    const L3ChannelDescription& description() const { return mDescription; }
    const L3ChannelMode& mode() const { return mMode; }
};

// ── GPRS Suspension Request (GSM 04.08 9.1.13b) ────────────────────────

class L3GPRSSuspensionRequest : public L3RRMessageNRO {
public:
    uint32_t mTLLI;
    std::vector<uint8_t> mRaId;
    uint8_t mSuspensionCause;
    uint8_t mServiceSupport;

    L3GPRSSuspensionRequest() : mServiceSupport(0) {}
    int MTI() const override { return GPRSSuspensionRequest; }
    size_t l2BodyLength() const override { return 11; }
    void parseBody(const L3Frame& src, size_t& rp) override;
    void writeBody(L3Frame& dest, size_t& wp) const override;
    void text(std::ostream& os) const override;
};

// ── Application Information (GSM 04.08 9.1.53) ────────────────────────

class L3ApplicationInformation : public L3RRMessageNRO {
private:
    unsigned mProtocolIdentifier;
    unsigned mCR;
    unsigned mFirstSegment;
    unsigned mLastSegment;
    BitVector mData;
public:
    L3ApplicationInformation();
    L3ApplicationInformation(BitVector data, unsigned protocolId = 0,
                              unsigned cr = 0, unsigned first = 0, unsigned last = 0);

    unsigned protocolIdentifier() const { return mProtocolIdentifier; }
    unsigned CR() const { return mCR; }
    unsigned firstSegment() const { return mFirstSegment; }
    unsigned lastSegment() const { return mLastSegment; }
    const BitVector& data() const { return mData; }

    int MTI() const override { return ApplicationInformation; }
    size_t l2BodyLength() const override;
    void writeBody(L3Frame&, size_t&) const override;
    void parseBody(const L3Frame& src, size_t& rp) override;
    void text(std::ostream& os) const override;
};

// ── System Information Type 3 (GSM 04.08 9.1.35) ──────────────────────

class L3SystemInformationType3 : public L3RRMessageRO {
private:
    L3CellIdentity mCI;
    L3LocationAreaIdentity mLAI;
    L3ControlChannelDescription mControlChannelDescription;
    L3CellOptionsBCCH mCellOptions;
    L3CellSelectionParameters mCellSelectionParameters;
    L3RACHControlParameters mRACHControlParameters;
    L3SI3RestOctets mRestOctets;
public:
    int MTI() const override { return SystemInformationType3; }
    size_t l2BodyLength() const override { return 16; }
    size_t restOctetsLength() const override { return mRestOctets.lengthV(); }
    void parseBody(const L3Frame& src, size_t& rp) override;
    void writeBody(L3Frame& dest, size_t& wp) const override;
    void text(std::ostream& os) const override;
    const L3CellIdentity& CI() const { return mCI; }
    const L3LocationAreaIdentity& LAI() const { return mLAI; }
    const L3ControlChannelDescription& controlChannelDescription() const { return mControlChannelDescription; }
    const L3CellOptionsBCCH& cellOptions() const { return mCellOptions; }
    const L3CellSelectionParameters& cellSelectionParameters() const { return mCellSelectionParameters; }
    const L3RACHControlParameters& rachaControl() const { return mRACHControlParameters; }
};

// ── System Information Type 13 (GSM 04.08 9.1.43a) ────────────────────

class L3SystemInformationType13 : public L3RRMessageRO {
private:
    L3SI13RestOctets mRestOctets;
public:
    int MTI() const override { return SystemInformationType13; }
    size_t l2BodyLength() const override { return 0; }
    size_t restOctetsLength() const override { return mRestOctets.lengthV(); }
    void parseBody(const L3Frame& src, size_t& rp) override;
    void writeBody(L3Frame& dest, size_t& wp) const override;
    void text(std::ostream& os) const override;
};

// ── Immediate Assignment (GSM 04.08 9.1.19) ───────────────────────────

class L3ImmediateAssignment : public L3RRMessageNRO {
private:
    L3PageMode mPageMode;
    L3DedicatedModeOrTBF mDedicatedModeOrTBF;
    L3RequestReference mRequestReference;
    L3ChannelDescription mChannelDescription;
    L3TimingAdvance mTimingAdvance;
    std::vector<uint8_t> mMobileAllocation;
    Bool_z mStartTimePresent;
    uint32_t mStartTimeFrame;
public:
    L3ImmediateAssignment();
    int MTI() const override { return ImmediateAssignment; }
    size_t l2BodyLength() const override;
    void parseBody(const L3Frame& src, size_t& rp) override;
    void writeBody(L3Frame& dest, size_t& wp) const override;
    void text(std::ostream& os) const override;
    const L3ChannelDescription& channelDescription() const { return mChannelDescription; }
    const L3RequestReference& requestReference() const { return mRequestReference; }
    const L3TimingAdvance& timingAdvance() const { return mTimingAdvance; }
    bool hasStartTime() const { return mStartTimePresent; }
    uint32_t startTimeFrame() const { return mStartTimeFrame; }
};

// ── Immediate Assignment Extended (GSM 04.08 9.1.18) ──────────────────

class L3ImmediateAssignmentExtended : public L3RRMessageNRO {
private:
    L3PageMode mPageMode;
    L3DedicatedModeOrTBF mDedicatedModeOrTBF;
    L3RequestReference mRequestReference;
    L3ChannelDescription mChannelDescription;
    L3TimingAdvance mTimingAdvance;
    std::vector<uint8_t> mMobileAllocation;
    Bool_z mStartTimePresent;
    uint32_t mStartTimeFrame;
    bool mHaveAdditionalChannel;
    L3AdditionalChannelDescription mAdditionalChannel;
public:
    L3ImmediateAssignmentExtended();
    int MTI() const override { return ImmediateAssignmentExtended; }
    size_t l2BodyLength() const override;
    void parseBody(const L3Frame& src, size_t& rp) override;
    void writeBody(L3Frame& dest, size_t& wp) const override;
    void text(std::ostream& os) const override;
    const L3ChannelDescription& channelDescription() const { return mChannelDescription; }
    bool hasAdditionalChannel() const { return mHaveAdditionalChannel; }
    const L3AdditionalChannelDescription& additionalChannel() const { return mAdditionalChannel; }
};

// ── Immediate Assignment Reject (GSM 04.08 9.1.20) ────────────────────

class L3ImmediateAssignmentReject : public L3RRMessageNRO {
private:
    L3PageMode mPageMode;
    std::vector<L3RequestReference> mRequestReferences;
    unsigned mWaitIndication;
public:
    L3ImmediateAssignmentReject();
    explicit L3ImmediateAssignmentReject(unsigned waitSeconds);
    int MTI() const override { return ImmediateAssignmentReject; }
    size_t l2BodyLength() const override;
    void parseBody(const L3Frame& src, size_t& rp) override;
    void writeBody(L3Frame& dest, size_t& wp) const override;
    void text(std::ostream& os) const override;
    unsigned waitTime() const { return mWaitIndication; }
    const std::vector<L3RequestReference>& requestReferences() const { return mRequestReferences; }
};

// ── Paging Request Type 2 (GSM 04.08 9.1.23) ──────────────────────────

class L3PagingRequestType2 : public L3RRMessageNRO {
private:
    std::vector<L3MobileIdentity> mMobileIDs;
    ChannelType mChannelsNeeded[2];
public:
    L3PagingRequestType2();
    L3PagingRequestType2(const L3MobileIdentity& wId, ChannelType wType);
    int MTI() const override { return PagingRequestType2; }
    size_t l2BodyLength() const override;
    void writeBody(L3Frame& dest, size_t& wp) const override;
    void parseBody(const L3Frame& src, size_t& rp) override;
    void text(std::ostream& os) const override;
};

// ── Paging Request Type 3 (GSM 04.08 9.1.24) ──────────────────────────

class L3PagingRequestType3 : public L3RRMessageNRO {
private:
    std::vector<L3MobileIdentity> mMobileIDs;
    ChannelType mChannelsNeeded[2];
public:
    L3PagingRequestType3();
    L3PagingRequestType3(const L3MobileIdentity& wId, ChannelType wType);
    int MTI() const override { return PagingRequestType3; }
    size_t l2BodyLength() const override;
    void writeBody(L3Frame& dest, size_t& wp) const override;
    void parseBody(const L3Frame& src, size_t& rp) override;
    void text(std::ostream& os) const override;
};

// ── Physical Information (GSM 04.08 9.1.12) ───────────────────────────

class L3PhysicalInformation : public L3RRMessageNRO {
private:
    L3TimingAdvance mTA;
public:
    L3PhysicalInformation();
    int MTI() const override { return PhysicalInformation; }
    size_t l2BodyLength() const override { return mTA.lengthV(); }
    void parseBody(const L3Frame& src, size_t& rp) override;
    void writeBody(L3Frame& dest, size_t& wp) const override;
    void text(std::ostream& os) const override;
    const L3TimingAdvance& timingAdvance() const { return mTA; }
};

// ── Handover Command (GSM 04.08 9.1.15) ───────────────────────────────

class L3HandoverCommand : public L3RRMessageNRO {
private:
    L3CellDescription mCellDescription;
    L3ChannelDescription2 mChannelDescriptionAfter;
    L3HandoverReference mHandoverReference;
    L3PowerCommandAndAccessType mPowerCommandAccessType;
    L3SynchronizationIndication mSynchronizationIndication;
public:
    L3HandoverCommand();
    int MTI() const override { return HandoverCommand; }
    size_t l2BodyLength() const override;
    void parseBody(const L3Frame& src, size_t& rp) override;
    void writeBody(L3Frame& dest, size_t& wp) const override;
    void text(std::ostream& os) const override;
    const L3CellDescription& cellDescription() const { return mCellDescription; }
    const L3ChannelDescription2& channelDescriptionAfter() const { return mChannelDescriptionAfter; }
    const L3HandoverReference& handoverReference() const { return mHandoverReference; }
    const L3PowerCommandAndAccessType& powerCommandAccessType() const { return mPowerCommandAccessType; }
    const L3SynchronizationIndication& syncIndication() const { return mSynchronizationIndication; }
};

// ── Additional Assignment (GSM 04.08 9.1.1) ───────────────────────────

class L3AdditionalAssignment : public L3RRMessageNRO {
private:
    L3AdditionalChannelDescription mAdditionalChannel;
    bool mHavePowerCommand;
    L3PowerCommand mPowerCommand;
public:
    L3AdditionalAssignment();
    int MTI() const override { return AdditionalAssignment; }
    size_t l2BodyLength() const override;
    void parseBody(const L3Frame& src, size_t& rp) override;
    void writeBody(L3Frame& dest, size_t& wp) const override;
    void text(std::ostream& os) const override;
    const L3AdditionalChannelDescription& additionalChannel() const { return mAdditionalChannel; }
    bool hasPowerCommand() const { return mHavePowerCommand; }
    const L3PowerCommand& powerCommand() const { return mPowerCommand; }
};

// ── System Information Type 2 (GSM 04.08 9.1.32) ─────────────────────
// V-format: BCCH Frequency List(16) + NCC Permitted(1) + RACH Control Parameters(3) = 20 bytes

class L3SystemInformationType2 : public L3RRMessageNRO {
private:
    L3BCCHFrequencyList mBCCHFrequencyList;
    L3NCCPermitted mNCCPermitted;
    L3RACHControlParameters mRACHControlParameters;
public:
    L3SystemInformationType2();
    int MTI() const override { return SystemInformationType2; }
    size_t l2BodyLength() const override { return 20; }
    void parseBody(const L3Frame& src, size_t& rp) override;
    void writeBody(L3Frame& dest, size_t& wp) const override;
    void text(std::ostream& os) const override;
    const L3BCCHFrequencyList& bcchFrequencyList() const { return mBCCHFrequencyList; }
    const L3NCCPermitted& nccPermitted() const { return mNCCPermitted; }
    const L3RACHControlParameters& rachaControl() const { return mRACHControlParameters; }
};

// ── System Information Type 2bis (GSM 04.08 9.1.33) ──────────────────
// V-format: BCCH Frequency List(16) + NCC Permitted(1) + RACH Control Parameters(3) = 20 bytes

class L3SystemInformationType2bis : public L3RRMessageNRO {
private:
    L3BCCHFrequencyList mBCCHFrequencyList;
    L3NCCPermitted mNCCPermitted;
    L3RACHControlParameters mRACHControlParameters;
public:
    L3SystemInformationType2bis();
    int MTI() const override { return SystemInformationType2bis; }
    size_t l2BodyLength() const override { return 20; }
    void parseBody(const L3Frame& src, size_t& rp) override;
    void writeBody(L3Frame& dest, size_t& wp) const override;
    void text(std::ostream& os) const override;
    const L3BCCHFrequencyList& bcchFrequencyList() const { return mBCCHFrequencyList; }
    const L3NCCPermitted& nccPermitted() const { return mNCCPermitted; }
    const L3RACHControlParameters& rachaControl() const { return mRACHControlParameters; }
};

// ── System Information Type 2ter (GSM 04.08 9.1.34) ──────────────────
// V-format: BCCH Frequency List(16) + NCC Permitted(1) + RACH Control Parameters(3) = 20 bytes

class L3SystemInformationType2ter : public L3RRMessageNRO {
private:
    L3BCCHFrequencyList mBCCHFrequencyList;
    L3NCCPermitted mNCCPermitted;
    L3RACHControlParameters mRACHControlParameters;
public:
    L3SystemInformationType2ter();
    int MTI() const override { return SystemInformationType2ter; }
    size_t l2BodyLength() const override { return 20; }
    void parseBody(const L3Frame& src, size_t& rp) override;
    void writeBody(L3Frame& dest, size_t& wp) const override;
    void text(std::ostream& os) const override;
    const L3BCCHFrequencyList& bcchFrequencyList() const { return mBCCHFrequencyList; }
    const L3NCCPermitted& nccPermitted() const { return mNCCPermitted; }
    const L3RACHControlParameters& rachaControl() const { return mRACHControlParameters; }
};

// ── System Information Type 4 (GSM 04.08 9.1.36) ─────────────────────
// V-format: LAI(5) + Cell Selection Parameters(2) + RACH Control Parameters(3) = 10 bytes
// Rest octets: CBCH Channel Description (optional, TV 0x64) + SI4 Rest Octets

class L3SystemInformationType4 : public L3RRMessageRO {
private:
    L3LocationAreaIdentity mLAI;
    L3CellSelectionParameters mCellSelectionParameters;
    L3RACHControlParameters mRACHControlParameters;
    bool mHaveCBCH;
    L3ChannelDescription mCBCHChannelDescription;
    L3SIType4RestOctets mRestOctets;
public:
    L3SystemInformationType4();
    int MTI() const override { return SystemInformationType4; }
    size_t l2BodyLength() const override;
    size_t restOctetsLength() const override;
    void parseBody(const L3Frame& src, size_t& rp) override;
    void writeBody(L3Frame& dest, size_t& wp) const override;
    void text(std::ostream& os) const override;
    const L3LocationAreaIdentity& LAI() const { return mLAI; }
    const L3CellSelectionParameters& cellSelectionParameters() const { return mCellSelectionParameters; }
    const L3RACHControlParameters& rachaControl() const { return mRACHControlParameters; }
    bool hasCBCH() const { return mHaveCBCH; }
    const L3ChannelDescription& cbchChannelDescription() const { return mCBCHChannelDescription; }
};

// ── System Information Type 5 (GSM 04.08 9.1.37) ─────────────────────
// V-format: BCCH Frequency List(16) = 16 bytes

class L3SystemInformationType5 : public L3RRMessageNRO {
private:
    L3BCCHFrequencyList mBCCHFrequencyList;
public:
    L3SystemInformationType5();
    int MTI() const override { return SystemInformationType5; }
    size_t l2BodyLength() const override { return 16; }
    void parseBody(const L3Frame& src, size_t& rp) override;
    void writeBody(L3Frame& dest, size_t& wp) const override;
    void text(std::ostream& os) const override;
    const L3BCCHFrequencyList& bcchFrequencyList() const { return mBCCHFrequencyList; }
};

// ── System Information Type 5bis (GSM 04.08 9.1.38) ──────────────────
// V-format: BCCH Frequency List(16) = 16 bytes

class L3SystemInformationType5bis : public L3RRMessageNRO {
private:
    L3BCCHFrequencyList mBCCHFrequencyList;
public:
    L3SystemInformationType5bis();
    int MTI() const override { return SystemInformationType5bis; }
    size_t l2BodyLength() const override { return 16; }
    void parseBody(const L3Frame& src, size_t& rp) override;
    void writeBody(L3Frame& dest, size_t& wp) const override;
    void text(std::ostream& os) const override;
    const L3BCCHFrequencyList& bcchFrequencyList() const { return mBCCHFrequencyList; }
};

// ── System Information Type 5ter (GSM 04.08 9.1.39) ──────────────────
// V-format: BCCH Frequency List(16) = 16 bytes

class L3SystemInformationType5ter : public L3RRMessageNRO {
private:
    L3BCCHFrequencyList mBCCHFrequencyList;
public:
    L3SystemInformationType5ter();
    int MTI() const override { return SystemInformationType5ter; }
    size_t l2BodyLength() const override { return 16; }
    void parseBody(const L3Frame& src, size_t& rp) override;
    void writeBody(L3Frame& dest, size_t& wp) const override;
    void text(std::ostream& os) const override;
    const L3BCCHFrequencyList& bcchFrequencyList() const { return mBCCHFrequencyList; }
};

// ── System Information Type 6 (GSM 04.08 9.1.40) ─────────────────────
// V-format: CI(2) + LAI(5) + Cell Options SACCH(1) + NCC Permitted(1) = 9 bytes

class L3SystemInformationType6 : public L3RRMessageNRO {
private:
    L3CellIdentity mCI;
    L3LocationAreaIdentity mLAI;
    L3CellOptionsSACCH mCellOptions;
    L3NCCPermitted mNCCPermitted;
public:
    L3SystemInformationType6();
    int MTI() const override { return SystemInformationType6; }
    size_t l2BodyLength() const override { return 9; }
    void parseBody(const L3Frame& src, size_t& rp) override;
    void writeBody(L3Frame& dest, size_t& wp) const override;
    void text(std::ostream& os) const override;
    const L3CellIdentity& CI() const { return mCI; }
    const L3LocationAreaIdentity& LAI() const { return mLAI; }
    const L3CellOptionsSACCH& cellOptions() const { return mCellOptions; }
    const L3NCCPermitted& nccPermitted() const { return mNCCPermitted; }
};

// ── System Information Type 7 (GSM 04.08 9.1.41) ─────────────────────
// TV-format: RACH Control Parameters(TV,0x28) + Neighbor Cells Description(0..10, TV,0x21)

class L3SystemInformationType7 : public L3RRMessageNRO {
private:
    L3RACHControlParameters mRACHControl;
    std::vector<L3CellChannelDescription> mCellChannelDescriptions;
public:
    L3SystemInformationType7();
    int MTI() const override { return SystemInformationType7; }
    size_t l2BodyLength() const override;
    void parseBody(const L3Frame& src, size_t& rp) override;
    void writeBody(L3Frame& dest, size_t& wp) const override;
    void text(std::ostream& os) const override;
    const L3RACHControlParameters& rachaControl() const { return mRACHControl; }
    const std::vector<L3CellChannelDescription>& cellChannelDescriptions() const { return mCellChannelDescriptions; }
};

// ── System Information Type 8 (GSM 04.08 9.1.42) ─────────────────────
// TV-format: NCC Permitted(TV,0x27) + RACH Control Parameters(TV,0x28) + Neighbor Cells Description(0..10, TV,0x21)

class L3SystemInformationType8 : public L3RRMessageNRO {
private:
    L3NCCPermitted mNCCPermitted;
    L3RACHControlParameters mRACHControl;
    std::vector<L3CellChannelDescription> mCellChannelDescriptions;
public:
    L3SystemInformationType8();
    int MTI() const override { return SystemInformationType8; }
    size_t l2BodyLength() const override;
    void parseBody(const L3Frame& src, size_t& rp) override;
    void writeBody(L3Frame& dest, size_t& wp) const override;
    void text(std::ostream& os) const override;
    const L3NCCPermitted& nccPermitted() const { return mNCCPermitted; }
    const L3RACHControlParameters& rachaControl() const { return mRACHControl; }
    const std::vector<L3CellChannelDescription>& cellChannelDescriptions() const { return mCellChannelDescriptions; }
};

// ── System Information Type 9 (GSM 04.08 9.1.43) ─────────────────────
// V-format: CI(2) + Cell Selection Parameters(2) + Cell Options BCCH(1) = 5 bytes

class L3SystemInformationType9 : public L3RRMessageNRO {
private:
    L3CellIdentity mCI;
    L3CellSelectionParameters mCellSelectionParameters;
    L3CellOptionsBCCH mCellOptions;
public:
    L3SystemInformationType9();
    int MTI() const override { return SystemInformationType9; }
    size_t l2BodyLength() const override { return 5; }
    void parseBody(const L3Frame& src, size_t& rp) override;
    void writeBody(L3Frame& dest, size_t& wp) const override;
    void text(std::ostream& os) const override;
    const L3CellIdentity& CI() const { return mCI; }
    const L3CellSelectionParameters& cellSelectionParameters() const { return mCellSelectionParameters; }
    const L3CellOptionsBCCH& cellOptions() const { return mCellOptions; }
};

// ── System Information Type 16 (GSM 04.08 9.1.43b) ───────────────────
// V-format: CI(2) + Cell Selection Parameters(2) + Cell Options BCCH(1) = 5 bytes

class L3SystemInformationType16 : public L3RRMessageNRO {
private:
    L3CellIdentity mCI;
    L3CellSelectionParameters mCellSelectionParameters;
    L3CellOptionsBCCH mCellOptions;
public:
    L3SystemInformationType16();
    int MTI() const override { return SystemInformationType16; }
    size_t l2BodyLength() const override { return 5; }
    void parseBody(const L3Frame& src, size_t& rp) override;
    void writeBody(L3Frame& dest, size_t& wp) const override;
    void text(std::ostream& os) const override;
    const L3CellIdentity& CI() const { return mCI; }
    const L3CellSelectionParameters& cellSelectionParameters() const { return mCellSelectionParameters; }
    const L3CellOptionsBCCH& cellOptions() const { return mCellOptions; }
};

// ── System Information Type 17 (GSM 04.08 9.1.43c) ───────────────────
// TV-format: RACH Control Parameters(TV,0x28) + Neighbor Cells Description(0..10, TV,0x21)

class L3SystemInformationType17 : public L3RRMessageNRO {
private:
    L3RACHControlParameters mRACHControl;
    std::vector<L3CellChannelDescription> mCellChannelDescriptions;
public:
    L3SystemInformationType17();
    int MTI() const override { return SystemInformationType17; }
    size_t l2BodyLength() const override;
    void parseBody(const L3Frame& src, size_t& rp) override;
    void writeBody(L3Frame& dest, size_t& wp) const override;
    void text(std::ostream& os) const override;
    const L3RACHControlParameters& rachaControl() const { return mRACHControl; }
    const std::vector<L3CellChannelDescription>& cellChannelDescriptions() const { return mCellChannelDescriptions; }
};

// ── Synchronization Channel Information (GSM 04.08 9.1.30) ────────────

class L3SynchronizationChannelInformation : public L3RRMessageNRO {
private:
    L3CellIdentity mCellIdentity;
    L3LocationAreaIdentity mLocationAreaIdentity;
public:
    L3SynchronizationChannelInformation();
    int MTI() const override { return SynchronizationChannelInformation; }
    size_t l2BodyLength() const override { return 7; }
    void parseBody(const L3Frame& src, size_t& rp) override;
    void writeBody(L3Frame& dest, size_t& wp) const override;
    void text(std::ostream& os) const override;
    const L3CellIdentity& cellIdentity() const { return mCellIdentity; }
    const L3LocationAreaIdentity& locationAreaIdentity() const { return mLocationAreaIdentity; }
};

// ── Channel Request (GSM 04.08 9.1.13) ────────────────────────────────

class L3ChannelRequest : public L3RRMessageNRO {
private:
    unsigned mRequestReference;
public:
    L3ChannelRequest(unsigned wRef = 0);
    int MTI() const override { return ChannelRequest; }
    size_t l2BodyLength() const override { return 1; }
    void parseBody(const L3Frame& src, size_t& rp) override;
    void writeBody(L3Frame& dest, size_t& wp) const override;
    void text(std::ostream& os) const override;
    unsigned requestReference() const { return mRequestReference; }
};

// ── Handover Access (GSM 04.08 9.1.14a) ───────────────────────────────

class L3HandoverAccess : public L3RRMessageNRO {
private:
    unsigned mHandoverNumber;
public:
    L3HandoverAccess(unsigned wNumber = 0);
    int MTI() const override { return HandoverAccess; }
    size_t l2BodyLength() const override { return 4; }
    void parseBody(const L3Frame& src, size_t& rp) override;
    void writeBody(L3Frame& dest, size_t& wp) const override;
    void text(std::ostream& os) const override;
    unsigned handoverNumber() const { return mHandoverNumber; }
};

} // namespace gsml3parser

#endif // GSML3PARSER_RR_L3RRMESSAGES_H
