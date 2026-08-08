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

#include <array>
#include <cstdint>
#include <ostream>
#include <string>
#include <vector>

#include "../expected.h"
#include "../bitreader.h"
#include "../bitwriter.h"
#include "../types.h"
#include "../enums.h"
#include "../common/l3common.h"

namespace gsml3parser {

// RR message type names for text output.
const char* rrMessageName(int mti);

// ── Paging Request Type 1 (GSM 04.08 9.1.22) ──────────────────────────

class L3PagingRequestType1 {
    std::vector<L3MobileIdentity> mMobileIDs;
    std::array<ChannelType, 2> mChannelsNeeded{ChannelType::AnyDCCHType, ChannelType::AnyDCCHType};
public:
    static constexpr int MTI = 0x21;

    L3PagingRequestType1() = default;

    class Builder {
        std::vector<L3MobileIdentity> mMobileIds;
        std::array<ChannelType, 2> mChannelsNeeded{ChannelType::AnyDCCHType, ChannelType::AnyDCCHType};
    public:
        Builder& addMobileId(const L3MobileIdentity& id, ChannelType type);
        L3PagingRequestType1 build();
    };

    static Builder builder();

    const std::vector<L3MobileIdentity>& mobileIds() const { return mMobileIDs; }
    const std::array<ChannelType, 2>& channelsNeeded() const { return mChannelsNeeded; }

    size_t bodyLength() const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
    [[nodiscard]] static Expected<L3PagingRequestType1> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── Paging Request Type 2 (GSM 04.08 9.1.23) ──────────────────────────

class L3PagingRequestType2 {
    std::vector<uint32_t> mTMSIs;
    std::array<ChannelType, 2> mChannelsNeeded{ChannelType::AnyDCCHType, ChannelType::AnyDCCHType};
public:
    static constexpr int MTI = 0x22;

    L3PagingRequestType2() = default;

    class Builder {
        std::vector<uint32_t> mTMSIs;
        std::array<ChannelType, 2> mChannelsNeeded{ChannelType::AnyDCCHType, ChannelType::AnyDCCHType};
    public:
        Builder& addTMSI(uint32_t tmsi, ChannelType type);
        L3PagingRequestType2 build();
    };

    static Builder builder();

    const std::vector<uint32_t>& tmsis() const { return mTMSIs; }
    const std::array<ChannelType, 2>& channelsNeeded() const { return mChannelsNeeded; }

    size_t bodyLength() const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
    [[nodiscard]] static Expected<L3PagingRequestType2> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── Paging Request Type 3 (GSM 04.08 9.1.24) ──────────────────────────

class L3PagingRequestType3 {
    std::vector<uint32_t> mTMSIs;
    std::array<ChannelType, 2> mChannelsNeeded{ChannelType::AnyDCCHType, ChannelType::AnyDCCHType};
public:
    static constexpr int MTI = 0x24;

    L3PagingRequestType3() = default;

    class Builder {
        std::vector<uint32_t> mTMSIs;
        std::array<ChannelType, 2> mChannelsNeeded{ChannelType::AnyDCCHType, ChannelType::AnyDCCHType};
    public:
        Builder& addTMSI(uint32_t tmsi, ChannelType type);
        L3PagingRequestType3 build();
    };

    static Builder builder();

    const std::vector<uint32_t>& tmsis() const { return mTMSIs; }
    const std::array<ChannelType, 2>& channelsNeeded() const { return mChannelsNeeded; }

    size_t bodyLength() const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
    [[nodiscard]] static Expected<L3PagingRequestType3> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── Paging Response (GSM 04.08 9.1.25) ─────────────────────────────────

class L3PagingResponse {
    unsigned mCKSN{0};
    L3MobileStationClassmark2 mClassmark;
    L3MobileIdentity mMobileID;
public:
    static constexpr int MTI = 0x27;

    const L3MobileIdentity& mobileId() const { return mMobileID; }
    unsigned cksn() const { return mCKSN; }
    const L3MobileStationClassmark2& classmark() const { return mClassmark; }

    size_t bodyLength() const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
    [[nodiscard]] static Expected<L3PagingResponse> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── Channel Release (GSM 04.08 9.1.7) ──────────────────────────────────

class L3ChannelRelease {
    RRCause mCause{RRCause::Normal_Event};
    bool mGprsResumptionPresent{false};
    bool mGprsResumptionBit{false};
public:
    static constexpr int MTI = 0x0d;

    L3ChannelRelease() = default;
    explicit L3ChannelRelease(RRCause cause) : mCause(cause) {}

    RRCause cause() const { return mCause; }
    bool hasGprsResumption() const { return mGprsResumptionPresent; }
    bool gprsResumption() const { return mGprsResumptionBit; }

    size_t bodyLength() const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
    [[nodiscard]] static Expected<L3ChannelRelease> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── RR Status (GSM 04.08 9.1.29) ──────────────────────────────────────

class L3RRStatus {
    RRCause mCause{RRCause::Normal_Event};
public:
    static constexpr int MTI = 0x12;

    RRCause cause() const { return mCause; }

    size_t bodyLength() const { return 1; }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return 1; }
    [[nodiscard]] static Expected<L3RRStatus> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── Assignment Command (GSM 04.08 9.1.2) ───────────────────────────────

class L3AssignmentCommand {
    L3ChannelDescription mChannel;
    L3PowerCommand mPowerCommand;
    bool mHaveMode1{false};
    L3ChannelMode mMode1;
    L3MultiRateConfiguration mMultiRate;
public:
    static constexpr int MTI = 0x2e;

    L3AssignmentCommand() = default;

    const L3ChannelDescription& channel() const { return mChannel; }
    const L3PowerCommand& powerCommand() const { return mPowerCommand; }
    bool hasMode1() const { return mHaveMode1; }
    const L3ChannelMode& mode1() const { return mMode1; }
    bool isAMR() const { return mHaveMode1 && mMode1.isAMR(); }
    const L3MultiRateConfiguration& multiRate() const { return mMultiRate; }

    class Builder {
        L3ChannelDescription mChannel;
        L3PowerCommand mPowerCommand;
        bool mHaveMode1{false};
        L3ChannelMode mMode1;
        L3MultiRateConfiguration mMultiRate;
    public:
        Builder& channel(const L3ChannelDescription& ch);
        Builder& powerCommand(const L3PowerCommand& pc);
        Builder& mode1(const L3ChannelMode& mode);
        Builder& multiRate(const L3MultiRateConfiguration& mr);
        L3AssignmentCommand build();
    };

    static Builder builder();

    size_t bodyLength() const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
    [[nodiscard]] static Expected<L3AssignmentCommand> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── Assignment Complete (GSM 04.08 9.1.3) ──────────────────────────────

class L3AssignmentComplete {
    RRCause mCause{RRCause::Normal_Event};
public:
    static constexpr int MTI = 0x29;

    RRCause cause() const { return mCause; }

    size_t bodyLength() const { return 1; }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return 1; }
    [[nodiscard]] static Expected<L3AssignmentComplete> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── Assignment Failure (GSM 04.08 9.1.3) ───────────────────────────────

class L3AssignmentFailure {
    RRCause mCause{RRCause::Normal_Event};
public:
    static constexpr int MTI = 0x2f;

    RRCause cause() const { return mCause; }

    size_t bodyLength() const { return 1; }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return 1; }
    [[nodiscard]] static Expected<L3AssignmentFailure> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── Classmark Enquiry (GSM 04.08 9.1.14) ──────────────────────────────

class L3ClassmarkEnquiry {
public:
    static constexpr int MTI = 0x13;

    size_t bodyLength() const { return 0; }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return 0; }
    [[nodiscard]] static Expected<L3ClassmarkEnquiry> parse(BitReader&);
    void write(BitWriter&) const;
    void text(std::ostream& os) const;
};

// ── Classmark Change (GSM 04.08 9.1.11) ───────────────────────────────

class L3ClassmarkChange {
    L3MobileStationClassmark2 mClassmark;
    bool mHaveAdditionalClassmark{false};
    L3MobileStationClassmark3 mAdditionalClassmark;
public:
    static constexpr int MTI = 0x16;

    const L3MobileStationClassmark2& classmark() const { return mClassmark; }
    bool hasAdditionalClassmark() const { return mHaveAdditionalClassmark; }
    const L3MobileStationClassmark3& additionalClassmark() const { return mAdditionalClassmark; }

    size_t bodyLength() const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
    [[nodiscard]] static Expected<L3ClassmarkChange> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── Measurement Report (GSM 04.08 9.1.21) ─────────────────────────────

class L3MeasurementReport {
    L3MeasurementResults mMeasurementResults;
public:
    static constexpr int MTI = 0x15;

    const L3MeasurementResults& measurementResults() const { return mMeasurementResults; }

    size_t bodyLength() const { return 16; }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return 16; }
    [[nodiscard]] static Expected<L3MeasurementReport> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── Ciphering Mode Command (GSM 04.08 9.1.9) ──────────────────────────

class L3CipheringModeCommand {
    bool mCiphering{false};
    int mAlgorithm{0};
    L3CipheringModeResponse mCipheringModeResponse;
public:
    static constexpr int MTI = 0x35;

    L3CipheringModeCommand() = default;
    L3CipheringModeCommand(bool ciphering, int algorithm)
        : mCiphering(ciphering), mAlgorithm(algorithm) {}

    bool isCiphering() const { return mCiphering; }
    int algorithm() const { return mAlgorithm; }
    bool includeIMEISV() const { return mCipheringModeResponse.includeIMEISV(); }
    const L3CipheringModeResponse& cipheringModeResponse() const { return mCipheringModeResponse; }

    size_t bodyLength() const { return 1; }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return 1; }
    [[nodiscard]] static Expected<L3CipheringModeCommand> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── Ciphering Mode Complete (GSM 04.08 9.1.10) ────────────────────────

class L3CipheringModeComplete {
public:
    static constexpr int MTI = 0x32;

    size_t bodyLength() const { return 0; }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return 0; }
    [[nodiscard]] static Expected<L3CipheringModeComplete> parse(BitReader&);
    void write(BitWriter&) const;
    void text(std::ostream& os) const;
};

// ── Handover Complete (GSM 04.08 9.1.16) ──────────────────────────────

class L3HandoverComplete {
    RRCause mCause{RRCause::Normal_Event};
public:
    static constexpr int MTI = 0x2c;

    RRCause cause() const { return mCause; }

    size_t bodyLength() const { return 1; }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return 1; }
    [[nodiscard]] static Expected<L3HandoverComplete> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── Handover Failure (GSM 04.08 9.1.17) ───────────────────────────────

class L3HandoverFailure {
    RRCause mCause{RRCause::Normal_Event};
public:
    static constexpr int MTI = 0x28;

    RRCause cause() const { return mCause; }

    size_t bodyLength() const { return 1; }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return 1; }
    [[nodiscard]] static Expected<L3HandoverFailure> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── Channel Mode Modify (GSM 04.08 9.1.5) ─────────────────────────────

class L3ChannelModeModify {
    L3ChannelDescription mDescription;
    L3ChannelMode mMode;
    L3MultiRateConfiguration mMultiRate;
public:
    static constexpr int MTI = 0x10;

    L3ChannelModeModify() = default;
    L3ChannelModeModify(const L3ChannelDescription& wDesc, const L3ChannelMode& wMode)
        : mDescription(wDesc), mMode(wMode) {}

    bool isAMR() const { return mMode.isAMR(); }
    const L3ChannelDescription& description() const { return mDescription; }
    const L3ChannelMode& mode() const { return mMode; }

    size_t bodyLength() const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
    [[nodiscard]] static Expected<L3ChannelModeModify> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── Channel Mode Modify Acknowledge (GSM 04.08 9.1.6) ─────────────────

class L3ChannelModeModifyAcknowledge {
    L3ChannelDescription mDescription;
    L3ChannelMode mMode;
public:
    static constexpr int MTI = 0x17;

    const L3ChannelDescription& description() const { return mDescription; }
    const L3ChannelMode& mode() const { return mMode; }

    size_t bodyLength() const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
    [[nodiscard]] static Expected<L3ChannelModeModifyAcknowledge> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── GPRS Suspension Request (GSM 04.08 9.1.13b) ────────────────────────

class L3GPRSSuspensionRequest {
public:
    static constexpr int MTI = 0x34;

    uint32_t mTLLI{0};
    std::vector<uint8_t> mRaId;
    uint8_t mSuspensionCause{0};
    uint8_t mServiceSupport{0};

    L3GPRSSuspensionRequest() : mRaId(6, 0) {}

    size_t bodyLength() const { return 11; }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return 11; }
    [[nodiscard]] static Expected<L3GPRSSuspensionRequest> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── Application Information (GSM 04.08 9.1.53) ────────────────────────

class L3ApplicationInformation {
    unsigned mProtocolIdentifier{0};
    unsigned mCR{0};
    unsigned mFirstSegment{0};
    unsigned mLastSegment{0};
    std::vector<uint8_t> mData;
public:
    static constexpr int MTI = 0x38;

    L3ApplicationInformation() = default;
    L3ApplicationInformation(std::vector<uint8_t> data, unsigned protocolId = 0,
                              unsigned cr = 0, unsigned first = 0, unsigned last = 0)
        : mProtocolIdentifier(protocolId), mCR(cr), mFirstSegment(first),
          mLastSegment(last), mData(std::move(data)) {}

    unsigned protocolIdentifier() const { return mProtocolIdentifier; }
    unsigned cr() const { return mCR; }
    unsigned firstSegment() const { return mFirstSegment; }
    unsigned lastSegment() const { return mLastSegment; }
    const std::vector<uint8_t>& data() const { return mData; }

    size_t bodyLength() const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
    [[nodiscard]] static Expected<L3ApplicationInformation> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── System Information Type 1 (GSM 04.08 9.1.31) ──────────────────────

class L3SystemInformationType1 {
    L3FrequencyList mCellChannelDescription;
    L3RACHControlParameters mRACHControlParameters;
    bool mHaveRestOctets{false};
    uint8_t mRestOctet{0};
public:
    static constexpr int MTI = 0x19;

    L3SystemInformationType1() = default;

    const L3FrequencyList& cellChannelDescription() const { return mCellChannelDescription; }
    const L3RACHControlParameters& rachControl() const { return mRACHControlParameters; }
    bool hasRestOctets() const { return mHaveRestOctets; }
    uint8_t restOctet() const { return mRestOctet; }

    size_t bodyLength() const { return 19 + (mHaveRestOctets ? 1 : 0); }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
    size_t restOctetsLength() const { return mHaveRestOctets ? 1 : 0; }
    [[nodiscard]] static Expected<L3SystemInformationType1> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── System Information Type 2 (GSM 04.08 9.1.32) ─────────────────────

class L3SystemInformationType2 {
    L3BCCHFrequencyList mBCCHFrequencyList;
    L3NCCPermitted mNCCPermitted;
    L3RACHControlParameters mRACHControlParameters;
public:
    static constexpr int MTI = 0x1a;

    L3SystemInformationType2() = default;

    const L3BCCHFrequencyList& bcchFrequencyList() const { return mBCCHFrequencyList; }
    const L3NCCPermitted& nccPermitted() const { return mNCCPermitted; }
    const L3RACHControlParameters& rachControl() const { return mRACHControlParameters; }

    size_t bodyLength() const { return 20; }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return 20; }
    [[nodiscard]] size_t fullBodyLength() const { return bodyLength(); }
    [[nodiscard]] static Expected<L3SystemInformationType2> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── System Information Type 2bis (GSM 04.08 9.1.33) ──────────────────

class L3SystemInformationType2bis {
    L3BCCHFrequencyList mBCCHFrequencyList;
    L3RACHControlParameters mRACHControlParameters;
public:
    static constexpr int MTI = 0x02;

    L3SystemInformationType2bis() = default;

    const L3BCCHFrequencyList& bcchFrequencyList() const { return mBCCHFrequencyList; }
    const L3RACHControlParameters& rachControl() const { return mRACHControlParameters; }

    size_t bodyLength() const { return 19; }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return 19; }
    [[nodiscard]] size_t fullBodyLength() const { return 20; }
    size_t restOctetsLength() const { return 1; }
    [[nodiscard]] static Expected<L3SystemInformationType2bis> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── System Information Type 2ter (GSM 04.08 9.1.34) ──────────────────

class L3SystemInformationType2ter {
    L3BCCHFrequencyList mBCCHFrequencyList;
public:
    static constexpr int MTI = 0x03;

    L3SystemInformationType2ter() = default;

    const L3BCCHFrequencyList& bcchFrequencyList() const { return mBCCHFrequencyList; }

    size_t bodyLength() const { return 16; }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return 16; }
    [[nodiscard]] size_t fullBodyLength() const { return 20; }
    size_t restOctetsLength() const { return 4; }
    [[nodiscard]] static Expected<L3SystemInformationType2ter> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── System Information Type 3 (GSM 04.08 9.1.35) ──────────────────────

class L3SystemInformationType3 {
    L3CellIdentity mCI;
    L3LocationAreaIdentity mLAI;
    L3ControlChannelDescription mControlChannelDescription;
    L3CellOptionsBCCH mCellOptions;
    L3CellSelectionParameters mCellSelectionParameters;
    L3RACHControlParameters mRACHControlParameters;
    L3SI3RestOctets mRestOctets;
public:
    static constexpr int MTI = 0x1b;

    const L3CellIdentity& ci() const { return mCI; }
    const L3LocationAreaIdentity& lai() const { return mLAI; }
    const L3ControlChannelDescription& controlChannelDescription() const { return mControlChannelDescription; }
    const L3CellOptionsBCCH& cellOptions() const { return mCellOptions; }
    const L3CellSelectionParameters& cellSelectionParameters() const { return mCellSelectionParameters; }
    const L3RACHControlParameters& rachControl() const { return mRACHControlParameters; }
    const L3SI3RestOctets& restOctets() const { return mRestOctets; }

    L3SystemInformationType3() = default;

    size_t bodyLength() const { return 16; }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return 16; }
    size_t restOctetsLength() const { return mRestOctets.lengthV(); }
    [[nodiscard]] static Expected<L3SystemInformationType3> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── System Information Type 4 (GSM 04.08 9.1.36) ─────────────────────

class L3SystemInformationType4 {
    L3LocationAreaIdentity mLAI;
    L3CellSelectionParameters mCellSelectionParameters;
    L3RACHControlParameters mRACHControlParameters;
    bool mHaveCBCH{false};
    L3ChannelDescription mCBCHChannelDescription;
    L3SIType4RestOctets mRestOctets;
public:
    static constexpr int MTI = 0x1c;

    L3SystemInformationType4() = default;

    const L3LocationAreaIdentity& lai() const { return mLAI; }
    const L3CellSelectionParameters& cellSelectionParameters() const { return mCellSelectionParameters; }
    const L3RACHControlParameters& rachControl() const { return mRACHControlParameters; }
    bool hasCBCH() const { return mHaveCBCH; }
    const L3ChannelDescription& cbchChannelDescription() const { return mCBCHChannelDescription; }
    const L3SIType4RestOctets& restOctets() const { return mRestOctets; }

    size_t bodyLength() const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
    size_t restOctetsLength() const;
    [[nodiscard]] static Expected<L3SystemInformationType4> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── System Information Type 5 (GSM 04.08 9.1.37) ─────────────────────

class L3SystemInformationType5 {
    L3BCCHFrequencyList mBCCHFrequencyList;
public:
    static constexpr int MTI = 0x1d;

    L3SystemInformationType5() = default;

    const L3BCCHFrequencyList& bcchFrequencyList() const { return mBCCHFrequencyList; }

    size_t bodyLength() const { return 16; }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return 16; }
    [[nodiscard]] static Expected<L3SystemInformationType5> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── System Information Type 5bis (GSM 04.08 9.1.38) ──────────────────

class L3SystemInformationType5bis {
    L3BCCHFrequencyList mBCCHFrequencyList;
public:
    static constexpr int MTI = 0x05;

    L3SystemInformationType5bis() = default;

    const L3BCCHFrequencyList& bcchFrequencyList() const { return mBCCHFrequencyList; }

    size_t bodyLength() const { return 16; }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return 16; }
    [[nodiscard]] static Expected<L3SystemInformationType5bis> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── System Information Type 5ter (GSM 04.08 9.1.39) ──────────────────

class L3SystemInformationType5ter {
    L3BCCHFrequencyList mBCCHFrequencyList;
public:
    static constexpr int MTI = 0x06;

    L3SystemInformationType5ter() = default;

    const L3BCCHFrequencyList& bcchFrequencyList() const { return mBCCHFrequencyList; }

    size_t bodyLength() const { return 16; }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return 16; }
    [[nodiscard]] static Expected<L3SystemInformationType5ter> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── System Information Type 6 (GSM 04.08 9.1.40) ─────────────────────

class L3SystemInformationType6 {
    L3CellIdentity mCI;
    L3LocationAreaIdentity mLAI;
    L3CellOptionsSACCH mCellOptions;
    L3NCCPermitted mNCCPermitted;
public:
    static constexpr int MTI = 0x1e;

    L3SystemInformationType6() = default;

    const L3CellIdentity& ci() const { return mCI; }
    const L3LocationAreaIdentity& lai() const { return mLAI; }
    const L3CellOptionsSACCH& cellOptions() const { return mCellOptions; }
    const L3NCCPermitted& nccPermitted() const { return mNCCPermitted; }

    size_t bodyLength() const { return 9; }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return 9; }
    [[nodiscard]] static Expected<L3SystemInformationType6> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── System Information Type 7 (GSM 04.08 9.1.41) ─────────────────────

class L3SystemInformationType7 {
    L3RACHControlParameters mRACHControl;
    std::vector<L3CellChannelDescription> mCellChannelDescriptions;
public:
    static constexpr int MTI = 0x1f;

    L3SystemInformationType7() = default;

    const L3RACHControlParameters& rachControl() const { return mRACHControl; }
    const std::vector<L3CellChannelDescription>& cellChannelDescriptions() const { return mCellChannelDescriptions; }

    size_t bodyLength() const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
    [[nodiscard]] static Expected<L3SystemInformationType7> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── System Information Type 8 (GSM 04.08 9.1.42) ─────────────────────

class L3SystemInformationType8 {
    L3NCCPermitted mNCCPermitted;
    L3RACHControlParameters mRACHControl;
    std::vector<L3CellChannelDescription> mCellChannelDescriptions;
public:
    static constexpr int MTI = 0x18;

    L3SystemInformationType8() = default;

    const L3NCCPermitted& nccPermitted() const { return mNCCPermitted; }
    const L3RACHControlParameters& rachControl() const { return mRACHControl; }
    const std::vector<L3CellChannelDescription>& cellChannelDescriptions() const { return mCellChannelDescriptions; }

    size_t bodyLength() const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
    [[nodiscard]] static Expected<L3SystemInformationType8> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── System Information Type 9 (GSM 04.08 9.1.43) ─────────────────────

class L3SystemInformationType9 {
    L3CellIdentity mCI;
    L3CellSelectionParameters mCellSelectionParameters;
    L3CellOptionsBCCH mCellOptions;
public:
    static constexpr int MTI = 0x04;

    L3SystemInformationType9() = default;

    const L3CellIdentity& ci() const { return mCI; }
    const L3CellSelectionParameters& cellSelectionParameters() const { return mCellSelectionParameters; }
    const L3CellOptionsBCCH& cellOptions() const { return mCellOptions; }

    size_t bodyLength() const { return 5; }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return 5; }
    [[nodiscard]] static Expected<L3SystemInformationType9> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── System Information Type 13 (GSM 04.08 9.1.43a) ────────────────────

class L3SystemInformationType13 {
    L3SI13RestOctets mRestOctets;
public:
    static constexpr int MTI = 0x00;

    const L3SI13RestOctets& restOctets() const { return mRestOctets; }

    size_t bodyLength() const { return 0; }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return 0; }
    size_t restOctetsLength() const { return mRestOctets.lengthV(); }
    [[nodiscard]] static Expected<L3SystemInformationType13> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── System Information Type 16 (GSM 04.08 9.1.43b) ───────────────────

class L3SystemInformationType16 {
    L3CellIdentity mCI;
    L3CellSelectionParameters mCellSelectionParameters;
    L3CellOptionsBCCH mCellOptions;
public:
    static constexpr int MTI = 0x3d;

    L3SystemInformationType16() = default;

    const L3CellIdentity& ci() const { return mCI; }
    const L3CellSelectionParameters& cellSelectionParameters() const { return mCellSelectionParameters; }
    const L3CellOptionsBCCH& cellOptions() const { return mCellOptions; }

    size_t bodyLength() const { return 5; }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return 5; }
    [[nodiscard]] static Expected<L3SystemInformationType16> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── System Information Type 17 (GSM 04.08 9.1.43c) ───────────────────

class L3SystemInformationType17 {
    L3RACHControlParameters mRACHControl;
    std::vector<L3CellChannelDescription> mCellChannelDescriptions;
public:
    static constexpr int MTI = 0x3e;

    L3SystemInformationType17() = default;

    const L3RACHControlParameters& rachControl() const { return mRACHControl; }
    const std::vector<L3CellChannelDescription>& cellChannelDescriptions() const { return mCellChannelDescriptions; }

    size_t bodyLength() const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
    [[nodiscard]] static Expected<L3SystemInformationType17> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── Immediate Assignment (GSM 04.08 9.1.19) ───────────────────────────

class L3ImmediateAssignment {
    L3PageMode mPageMode;
    L3DedicatedModeOrTBF mDedicatedModeOrTBF;
    L3RequestReference mRequestReference;
    L3ChannelDescription mChannelDescription;
    L3TimingAdvance mTimingAdvance;
    std::vector<uint8_t> mMobileAllocation;
    bool mStartTimePresent{false};
    uint32_t mStartTimeFrame{0};
public:
    static constexpr int MTI = 0x3f;

    L3ImmediateAssignment() = default;

    const L3ChannelDescription& channelDescription() const { return mChannelDescription; }
    const L3RequestReference& requestReference() const { return mRequestReference; }
    const L3TimingAdvance& timingAdvance() const { return mTimingAdvance; }
    bool hasStartTime() const { return mStartTimePresent; }
    uint32_t startTimeFrame() const { return mStartTimeFrame; }

    size_t bodyLength() const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
    [[nodiscard]] static Expected<L3ImmediateAssignment> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── Immediate Assignment Extended (GSM 04.08 9.1.18) ──────────────────

class L3ImmediateAssignmentExtended {
    L3PageMode mPageMode;
    L3DedicatedModeOrTBF mDedicatedModeOrTBF;
    L3RequestReference mRequestReference;
    L3ChannelDescription mChannelDescription;
    L3TimingAdvance mTimingAdvance;
    std::vector<uint8_t> mMobileAllocation;
    bool mStartTimePresent{false};
    uint32_t mStartTimeFrame{0};
    bool mHaveAdditionalChannel{false};
    L3AdditionalChannelDescription mAdditionalChannel;
public:
    static constexpr int MTI = 0x39;

    L3ImmediateAssignmentExtended() = default;

    const L3ChannelDescription& channelDescription() const { return mChannelDescription; }
    bool hasAdditionalChannel() const { return mHaveAdditionalChannel; }
    const L3AdditionalChannelDescription& additionalChannel() const { return mAdditionalChannel; }

    size_t bodyLength() const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
    [[nodiscard]] static Expected<L3ImmediateAssignmentExtended> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── Immediate Assignment Reject (GSM 04.08 9.1.20) ────────────────────

class L3ImmediateAssignmentReject {
    unsigned mFeatureIndicator{0};
    unsigned mPageMode{0};
    std::vector<L3RequestReference> mRequestReferences;
    std::vector<unsigned> mWaitIndications;
    unsigned mWaitIndication{0};
public:
    static constexpr int MTI = 0x3a;

    L3ImmediateAssignmentReject() = default;
    explicit L3ImmediateAssignmentReject(unsigned waitSeconds);

    unsigned pageMode() const { return mPageMode; }
    unsigned featureIndicator() const { return mFeatureIndicator; }
    unsigned waitTime() const { return mWaitIndication; }
    const std::vector<L3RequestReference>& requestReferences() const { return mRequestReferences; }

    size_t bodyLength() const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
    [[nodiscard]] static Expected<L3ImmediateAssignmentReject> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── Additional Assignment (GSM 04.08 9.1.1) ───────────────────────────

class L3AdditionalAssignment {
    L3AdditionalChannelDescription mAdditionalChannel;
    bool mHavePowerCommand{false};
    L3PowerCommand mPowerCommand;
public:
    static constexpr int MTI = 0x3b;

    L3AdditionalAssignment() = default;

    const L3AdditionalChannelDescription& additionalChannel() const { return mAdditionalChannel; }
    bool hasPowerCommand() const { return mHavePowerCommand; }
    const L3PowerCommand& powerCommand() const { return mPowerCommand; }

    size_t bodyLength() const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
    [[nodiscard]] static Expected<L3AdditionalAssignment> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── Physical Information (GSM 04.08 9.1.12) ───────────────────────────

class L3PhysicalInformation {
    L3TimingAdvance mTA;
public:
    static constexpr int MTI = 0x2d;

    L3PhysicalInformation() = default;

    const L3TimingAdvance& timingAdvance() const { return mTA; }

    size_t bodyLength() const { return mTA.lengthV(); }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
    [[nodiscard]] static Expected<L3PhysicalInformation> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── Handover Command (GSM 04.08 9.1.15) ───────────────────────────────

class L3HandoverCommand {
    L3CellDescription mCellDescription;
    L3ChannelDescription2 mChannelDescriptionAfter;
    L3HandoverReference mHandoverReference;
    L3PowerCommandAndAccessType mPowerCommandAccessType;
    L3SynchronizationIndication mSynchronizationIndication;
public:
    static constexpr int MTI = 0x2b;

    L3HandoverCommand() = default;

    const L3CellDescription& cellDescription() const { return mCellDescription; }
    const L3ChannelDescription2& channelDescriptionAfter() const { return mChannelDescriptionAfter; }
    const L3HandoverReference& handoverReference() const { return mHandoverReference; }
    const L3PowerCommandAndAccessType& powerCommandAccessType() const { return mPowerCommandAccessType; }
    const L3SynchronizationIndication& syncIndication() const { return mSynchronizationIndication; }

    class Builder {
        L3CellDescription mCellDescription;
        L3ChannelDescription2 mChannelDescriptionAfter;
        L3HandoverReference mHandoverReference;
        L3PowerCommandAndAccessType mPowerCommandAccessType;
        L3SynchronizationIndication mSynchronizationIndication;
    public:
        Builder& cellDescription(const L3CellDescription& cd);
        Builder& channelDescriptionAfter(const L3ChannelDescription2& cda);
        Builder& handoverReference(const L3HandoverReference& hr);
        Builder& powerCommandAccessType(const L3PowerCommandAndAccessType& pcat);
        Builder& syncIndication(const L3SynchronizationIndication& si);
        L3HandoverCommand build();
    };

    static Builder builder();

    size_t bodyLength() const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
    [[nodiscard]] static Expected<L3HandoverCommand> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── Synchronization Channel Information (GSM 04.08 9.1.30) ────────────
// Short message: no standard L3 header, 7 bytes fixed.

class L3SynchronizationChannelInformation {
    L3CellIdentity mCellIdentity;
    L3LocationAreaIdentity mLocationAreaIdentity;
public:
    static constexpr int MTI = 0x100;

    L3SynchronizationChannelInformation() = default;

    const L3CellIdentity& cellIdentity() const { return mCellIdentity; }
    const L3LocationAreaIdentity& locationAreaIdentity() const { return mLocationAreaIdentity; }

    size_t bodyLength() const { return 7; }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return 7; }
    [[nodiscard]] static Expected<L3SynchronizationChannelInformation> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── Channel Request (GSM 04.08 9.1.13) ────────────────────────────────
// Short message: no standard L3 header, 1 byte.

class L3ChannelRequest {
    unsigned mRequestReference{0};
public:
    static constexpr int MTI = 0x101;

    L3ChannelRequest() = default;
    explicit L3ChannelRequest(unsigned wRef) : mRequestReference(wRef) {}

    unsigned requestReference() const { return mRequestReference; }

    size_t bodyLength() const { return 1; }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return 1; }
    [[nodiscard]] static Expected<L3ChannelRequest> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── Handover Access (GSM 04.08 9.1.14a) ───────────────────────────────
// Short message: no standard L3 header, 4 bytes.

class L3HandoverAccess {
    unsigned mHandoverNumber{0};
public:
    static constexpr int MTI = 0x102;

    L3HandoverAccess() = default;
    explicit L3HandoverAccess(unsigned wNumber) : mHandoverNumber(wNumber) {}

    unsigned handoverNumber() const { return mHandoverNumber; }

    size_t bodyLength() const { return 4; }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return 4; }
    [[nodiscard]] static Expected<L3HandoverAccess> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

} // namespace gsml3parser
