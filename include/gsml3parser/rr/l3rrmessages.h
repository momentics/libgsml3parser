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

    friend struct Builder;
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

    struct Builder {
        unsigned mCKSN{0};
        L3MobileStationClassmark2 mClassmark;
        L3MobileIdentity mMobileID;

        /// Set CKSN value.
        Builder& cksn(unsigned v) { mCKSN = v; return *this; }
        /// Set classmark.
        Builder& classmark(L3MobileStationClassmark2 v) { mClassmark = v; return *this; }
        /// Set mobile identity.
        Builder& mobileId(L3MobileIdentity v) { mMobileID = std::move(v); return *this; }
        /// Build the final message.
        [[nodiscard]] L3PagingResponse build() const {
            L3PagingResponse msg;
            msg.mCKSN = mCKSN;
            msg.mClassmark = mClassmark;
            msg.mMobileID = mMobileID;
            return msg;
        }
    };

    static Builder builder() { return Builder{}; }
};

// ── Channel Release (GSM 04.08 9.1.7) ──────────────────────────────────

class L3ChannelRelease {
    RRCause mCause{RRCause::Normal_Event};
    bool mGprsResumptionPresent{false};
    bool mGprsResumptionBit{false};

    friend struct Builder;
public:
    static constexpr int MTI = 0x0d;

    L3ChannelRelease() = default;
    explicit L3ChannelRelease(RRCause cause) : mCause(cause) {}

    RRCause cause() const { return mCause; }
    bool hasGprsResumption() const { return mGprsResumptionPresent; }
    bool gprsResumption() const { return mGprsResumptionBit; }

    struct Builder {
        RRCause mCause{RRCause::Normal_Event};
        bool mGprsResumptionPresent{false};
        bool mGprsResumptionBit{false};

        /// Set the RR cause.
        Builder& cause(RRCause v) { mCause = v; return *this; }
        /// Set GPRS resumption flag.
        Builder& gprsResumption(bool present, bool value) { mGprsResumptionPresent = present; mGprsResumptionBit = value; return *this; }
        /// Build the final message.
        [[nodiscard]] L3ChannelRelease build() const;
    };

    static Builder builder();

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

    friend struct Builder;
public:
    static constexpr int MTI = 0x12;

    RRCause cause() const { return mCause; }

    struct Builder {
        RRCause mCause{RRCause::Normal_Event};

        /// Set the RR cause.
        Builder& cause(RRCause v) { mCause = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3RRStatus build() const;
    };

    static Builder builder();

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
        /// Set the channel description.
        Builder& channel(const L3ChannelDescription& ch);
        /// Set the power command.
        Builder& powerCommand(const L3PowerCommand& pc);
        /// Set the channel mode (sets mHaveMode1 flag).
        Builder& mode1(const L3ChannelMode& mode);
        /// Set the multi-rate configuration.
        Builder& multiRate(const L3MultiRateConfiguration& mr);
        /// Build the final message.
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

    friend struct Builder;
public:
    static constexpr int MTI = 0x29;

    RRCause cause() const { return mCause; }

    struct Builder {
        RRCause mCause{RRCause::Normal_Event};

        /// Set the RR cause.
        Builder& cause(RRCause v) { mCause = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3AssignmentComplete build() const;
    };

    static Builder builder();

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

    friend struct Builder;
public:
    static constexpr int MTI = 0x2f;

    RRCause cause() const { return mCause; }

    struct Builder {
        RRCause mCause{RRCause::Normal_Event};

        /// Set the RR cause.
        Builder& cause(RRCause v) { mCause = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3AssignmentFailure build() const;
    };

    static Builder builder();

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

    struct Builder {
        /// Build the final message.
        [[nodiscard]] L3ClassmarkEnquiry build() const;
    };
    friend struct Builder;
    static Builder builder() { return Builder{}; }
};

// ── Classmark Change (GSM 04.08 9.1.11) ───────────────────────────────

class L3ClassmarkChange {
    L3MobileStationClassmark2 mClassmark;
    bool mHaveAdditionalClassmark{false};
    L3MobileStationClassmark3 mAdditionalClassmark;

    friend struct Builder;
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

    struct Builder {
        L3MobileStationClassmark2 mClassmark;
        bool mHaveAdditionalClassmark{false};
        L3MobileStationClassmark3 mAdditionalClassmark;

        /// Set classmark.
        Builder& classmark(L3MobileStationClassmark2 v) { mClassmark = v; return *this; }
        /// Set additional classmark (sets mHaveAdditionalClassmark flag).
        Builder& additionalClassmark(L3MobileStationClassmark3 v) { mAdditionalClassmark = v; mHaveAdditionalClassmark = true; return *this; }
        /// Build the final message.
        [[nodiscard]] L3ClassmarkChange build() const {
            L3ClassmarkChange msg;
            msg.mClassmark = mClassmark;
            msg.mHaveAdditionalClassmark = mHaveAdditionalClassmark;
            msg.mAdditionalClassmark = mAdditionalClassmark;
            return msg;
        }
    };

    static Builder builder() { return Builder{}; }
};

// ── Measurement Report (GSM 04.08 9.1.21) ─────────────────────────────

class L3MeasurementReport {
    L3MeasurementResults mMeasurementResults;

    friend struct Builder;
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

    struct Builder {
        L3MeasurementResults mMeasurementResults;

        /// Set measurement results.
        Builder& measurementResults(L3MeasurementResults v) { mMeasurementResults = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3MeasurementReport build() const {
            L3MeasurementReport msg;
            msg.mMeasurementResults = mMeasurementResults;
            return msg;
        }
    };

    static Builder builder() { return Builder{}; }
};

// ── Ciphering Mode Command (GSM 04.08 9.1.9) ──────────────────────────

class L3CipheringModeCommand {
    bool mCiphering{false};
    int mAlgorithm{0};
    L3CipheringModeResponse mCipheringModeResponse;

    friend struct Builder;
public:
    static constexpr int MTI = 0x35;

    L3CipheringModeCommand() = default;
    L3CipheringModeCommand(bool ciphering, int algorithm)
        : mCiphering(ciphering), mAlgorithm(algorithm) {}

    bool isCiphering() const { return mCiphering; }
    int algorithm() const { return mAlgorithm; }
    bool includeIMEISV() const { return mCipheringModeResponse.includeIMEISV(); }
    const L3CipheringModeResponse& cipheringModeResponse() const { return mCipheringModeResponse; }

    struct Builder {
        bool mCiphering{false};
        int mAlgorithm{0};
        L3CipheringModeResponse mCipheringModeResponse;

        /// Set ciphering on/off.
        Builder& ciphering(bool v) { mCiphering = v; return *this; }
        /// Set ciphering algorithm (A5/x).
        Builder& algorithm(int v) { mAlgorithm = v; return *this; }
        /// Set ciphering mode response (IMEISV flag etc.).
        Builder& cipheringModeResponse(L3CipheringModeResponse v) { mCipheringModeResponse = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3CipheringModeCommand build() const;
    };

    static Builder builder();

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

    struct Builder {
        /// Build the final message.
        [[nodiscard]] L3CipheringModeComplete build() const;
    };
    friend struct Builder;
    static Builder builder();

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

    friend struct Builder;
public:
    static constexpr int MTI = 0x2c;

    RRCause cause() const { return mCause; }

    struct Builder {
        RRCause mCause{RRCause::Normal_Event};

        /// Set the RR cause.
        Builder& cause(RRCause v) { mCause = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3HandoverComplete build() const;
    };

    static Builder builder();

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

    friend struct Builder;
public:
    static constexpr int MTI = 0x28;

    RRCause cause() const { return mCause; }

    struct Builder {
        RRCause mCause{RRCause::Normal_Event};

        /// Set the RR cause.
        Builder& cause(RRCause v) { mCause = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3HandoverFailure build() const;
    };

    static Builder builder();

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

    friend struct Builder;
public:
    static constexpr int MTI = 0x10;

    L3ChannelModeModify() = default;
    L3ChannelModeModify(const L3ChannelDescription& wDesc, const L3ChannelMode& wMode)
        : mDescription(wDesc), mMode(wMode) {}

    bool isAMR() const { return mMode.isAMR(); }
    const L3ChannelDescription& description() const { return mDescription; }
    const L3ChannelMode& mode() const { return mMode; }

    struct Builder {
        L3ChannelDescription mDescription;
        L3ChannelMode mMode;
        L3MultiRateConfiguration mMultiRate;

        /// Set the channel description.
        Builder& description(L3ChannelDescription v) { mDescription = v; return *this; }
        /// Set the channel mode.
        Builder& mode(L3ChannelMode v) { mMode = v; return *this; }
        /// Set multi-rate configuration.
        Builder& multiRate(L3MultiRateConfiguration v) { mMultiRate = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3ChannelModeModify build() const;
    };

    static Builder builder();

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

    friend struct Builder;
public:
    static constexpr int MTI = 0x17;

    const L3ChannelDescription& description() const { return mDescription; }
    const L3ChannelMode& mode() const { return mMode; }

    struct Builder {
        L3ChannelDescription mDescription;
        L3ChannelMode mMode;

        /// Set the channel description.
        Builder& description(L3ChannelDescription v) { mDescription = v; return *this; }
        /// Set the channel mode.
        Builder& mode(L3ChannelMode v) { mMode = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3ChannelModeModifyAcknowledge build() const;
    };

    static Builder builder();

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
    uint32_t mTLLI{0};
    std::vector<uint8_t> mRaId;
    uint8_t mSuspensionCause{0};
    uint8_t mServiceSupport{0};

    friend struct Builder;
public:
    static constexpr int MTI = 0x34;

    L3GPRSSuspensionRequest() : mRaId(6, 0) {}

    size_t bodyLength() const { return 11; }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return 11; }
    [[nodiscard]] static Expected<L3GPRSSuspensionRequest> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;

    struct Builder {
        uint32_t mTLLI{0};
        std::vector<uint8_t> mRaId;
        uint8_t mSuspensionCause{0};
        uint8_t mServiceSupport{0};

        /// Set TLLI value.
        Builder& tlli(uint32_t v) { mTLLI = v; return *this; }
        /// Set RA ID.
        Builder& raId(std::vector<uint8_t> v) { mRaId = std::move(v); return *this; }
        /// Set suspension cause.
        Builder& suspensionCause(uint8_t v) { mSuspensionCause = v; return *this; }
        /// Set service support.
        Builder& serviceSupport(uint8_t v) { mServiceSupport = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3GPRSSuspensionRequest build() const {
            L3GPRSSuspensionRequest msg;
            msg.mTLLI = mTLLI;
            msg.mRaId = mRaId;
            msg.mSuspensionCause = mSuspensionCause;
            msg.mServiceSupport = mServiceSupport;
            return msg;
        }
    };

    static Builder builder() { return Builder{}; }
};

// ── Application Information (GSM 04.08 9.1.53) ────────────────────────

class L3ApplicationInformation {
    unsigned mProtocolIdentifier{0};
    unsigned mCR{0};
    unsigned mFirstSegment{0};
    unsigned mLastSegment{0};
    std::vector<uint8_t> mData;

    friend struct Builder;
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

    struct Builder {
        unsigned mProtocolIdentifier{0};
        unsigned mCR{0};
        unsigned mFirstSegment{0};
        unsigned mLastSegment{0};
        std::vector<uint8_t> mData;

        /// Set protocol identifier.
        Builder& protocolIdentifier(unsigned v) { mProtocolIdentifier = v; return *this; }
        /// Set C/R bit.
        Builder& cr(unsigned v) { mCR = v; return *this; }
        /// Set first segment flag.
        Builder& firstSegment(unsigned v) { mFirstSegment = v; return *this; }
        /// Set last segment flag.
        Builder& lastSegment(unsigned v) { mLastSegment = v; return *this; }
        /// Set data payload.
        Builder& data(std::vector<uint8_t> v) { mData = std::move(v); return *this; }
        /// Build the final message.
        [[nodiscard]] L3ApplicationInformation build() const {
            L3ApplicationInformation msg;
            msg.mProtocolIdentifier = mProtocolIdentifier;
            msg.mCR = mCR;
            msg.mFirstSegment = mFirstSegment;
            msg.mLastSegment = mLastSegment;
            msg.mData = mData;
            return msg;
        }
    };

    static Builder builder() { return Builder{}; }
};

// ── System Information Type 1 (GSM 04.08 9.1.31) ──────────────────────

class L3SystemInformationType1 {
    L3FrequencyList mCellChannelDescription;
    L3RACHControlParameters mRACHControlParameters;
    bool mHaveRestOctets{false};
    uint8_t mRestOctet{0};

    friend struct Builder;
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

    struct Builder {
        L3FrequencyList mCellChannelDescription{};
        L3RACHControlParameters mRACHControlParameters{};
        bool mHaveRestOctets{false};
        uint8_t mRestOctet{0};

        /// Set the cell channel description (frequency list).
        Builder& cellChannelDescription(L3FrequencyList v) { mCellChannelDescription = v; return *this; }
        /// Set RACH control parameters.
        Builder& rachControlParameters(L3RACHControlParameters v) { mRACHControlParameters = v; return *this; }
        /// Set optional rest octet (sets mHaveRestOctets flag).
        Builder& restOctet(uint8_t v) { mRestOctet = v; mHaveRestOctets = true; return *this; }
        /// Build the final message.
        [[nodiscard]] L3SystemInformationType1 build() const;
    };

    static Builder builder();
};

// ── System Information Type 2 (GSM 04.08 9.1.32) ─────────────────────

class L3SystemInformationType2 {
    L3BCCHFrequencyList mBCCHFrequencyList;
    L3NCCPermitted mNCCPermitted;
    L3RACHControlParameters mRACHControlParameters;

    friend struct Builder;
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
    [[nodiscard]] size_t fullBodyLength() const { return 20; }
    [[nodiscard]] static Expected<L3SystemInformationType2> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;

    struct Builder {
        L3BCCHFrequencyList mBCCHFrequencyList{};
        L3NCCPermitted mNCCPermitted{};
        L3RACHControlParameters mRACHControlParameters{};

        /// Set BCCH frequency list.
        Builder& bcchFrequencyList(L3BCCHFrequencyList v) { mBCCHFrequencyList = v; return *this; }
        /// Set NCC permitted.
        Builder& nccPermitted(L3NCCPermitted v) { mNCCPermitted = v; return *this; }
        /// Set RACH control parameters.
        Builder& rachControlParameters(L3RACHControlParameters v) { mRACHControlParameters = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3SystemInformationType2 build() const;
    };

    static Builder builder();
};

// ── System Information Type 2bis (GSM 04.08 9.1.33) ──────────────────

class L3SystemInformationType2bis {
    L3BCCHFrequencyList mBCCHFrequencyList;
    L3RACHControlParameters mRACHControlParameters;

    friend struct Builder;
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

    struct Builder {
        L3BCCHFrequencyList mBCCHFrequencyList{};
        L3RACHControlParameters mRACHControlParameters{};

        /// Set BCCH frequency list.
        Builder& bcchFrequencyList(L3BCCHFrequencyList v) { mBCCHFrequencyList = v; return *this; }
        /// Set RACH control parameters.
        Builder& rachControlParameters(L3RACHControlParameters v) { mRACHControlParameters = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3SystemInformationType2bis build() const;
    };

    static Builder builder();
};

// ── System Information Type 2ter (GSM 04.08 9.1.34) ──────────────────

class L3SystemInformationType2ter {
    L3BCCHFrequencyList mBCCHFrequencyList;

    friend struct Builder;
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

    struct Builder {
        L3BCCHFrequencyList mBCCHFrequencyList{};

        /// Set BCCH frequency list.
        Builder& bcchFrequencyList(L3BCCHFrequencyList v) { mBCCHFrequencyList = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3SystemInformationType2ter build() const;
    };

    static Builder builder();
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

    friend struct Builder;
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

    size_t bodyLength() const { return 17; }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return 17; }
    size_t restOctetsLength() const { return mRestOctets.lengthV(); }
    [[nodiscard]] static Expected<L3SystemInformationType3> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;

    struct Builder {
        L3CellIdentity mCI{};
        L3LocationAreaIdentity mLAI{};
        L3ControlChannelDescription mControlChannelDescription{};
        L3CellOptionsBCCH mCellOptions{};
        L3CellSelectionParameters mCellSelectionParameters{};
        L3RACHControlParameters mRACHControlParameters{};
        L3SI3RestOctets mRestOctets{};

        /// Set cell identity.
        Builder& cellIdentity(L3CellIdentity v) { mCI = v; return *this; }
        /// Set location area identity.
        Builder& locationAreaIdentity(L3LocationAreaIdentity v) { mLAI = v; return *this; }
        /// Set control channel description.
        Builder& controlChannelDescription(L3ControlChannelDescription v) { mControlChannelDescription = v; return *this; }
        /// Set cell options for BCCH.
        Builder& cellOptions(L3CellOptionsBCCH v) { mCellOptions = v; return *this; }
        /// Set cell selection parameters.
        Builder& cellSelectionParameters(L3CellSelectionParameters v) { mCellSelectionParameters = v; return *this; }
        /// Set RACH control parameters.
        Builder& rachControlParameters(L3RACHControlParameters v) { mRACHControlParameters = v; return *this; }
        /// Set rest octets (optional SI3 extensions).
        Builder& restOctets(L3SI3RestOctets v) { mRestOctets = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3SystemInformationType3 build() const;
    };

    static Builder builder();
};

// ── System Information Type 4 (GSM 04.08 9.1.36) ─────────────────────

class L3SystemInformationType4 {
    L3LocationAreaIdentity mLAI;
    L3CellSelectionParameters mCellSelectionParameters;
    L3RACHControlParameters mRACHControlParameters;
    bool mHaveCBCH{false};
    L3ChannelDescription mCBCHChannelDescription;
    L3SIType4RestOctets mRestOctets;

    friend struct Builder;
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

    struct Builder {
        L3LocationAreaIdentity mLAI{};
        L3CellSelectionParameters mCellSelectionParameters{};
        L3RACHControlParameters mRACHControlParameters{};
        bool mHaveCBCH{false};
        L3ChannelDescription mCBCHChannelDescription{};
        L3SIType4RestOctets mRestOctets{};

        /// Set location area identity.
        Builder& locationAreaIdentity(L3LocationAreaIdentity v) { mLAI = v; return *this; }
        /// Set cell selection parameters.
        Builder& cellSelectionParameters(L3CellSelectionParameters v) { mCellSelectionParameters = v; return *this; }
        /// Set RACH control parameters.
        Builder& rachControlParameters(L3RACHControlParameters v) { mRACHControlParameters = v; return *this; }
        /// Set CBCH channel description (sets mHaveCBCH flag).
        Builder& cbchChannelDescription(L3ChannelDescription v) { mCBCHChannelDescription = v; mHaveCBCH = true; return *this; }
        /// Set rest octets.
        Builder& restOctets(L3SIType4RestOctets v) { mRestOctets = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3SystemInformationType4 build() const;
    };

    static Builder builder();
};

// ── System Information Type 5 (GSM 04.08 9.1.37) ─────────────────────

class L3SystemInformationType5 {
    L3BCCHFrequencyList mBCCHFrequencyList;

    friend struct Builder;
public:
    static constexpr int MTI = 0x1d;

    L3SystemInformationType5() = default;

    const L3BCCHFrequencyList& bcchFrequencyList() const { return mBCCHFrequencyList; }

    size_t bodyLength() const { return 17; }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return 17; }
    [[nodiscard]] static Expected<L3SystemInformationType5> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;

    struct Builder {
        L3BCCHFrequencyList mBCCHFrequencyList{};

        /// Set BCCH frequency list.
        Builder& bcchFrequencyList(L3BCCHFrequencyList v) { mBCCHFrequencyList = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3SystemInformationType5 build() const;
    };

    static Builder builder();
};

// ── System Information Type 5bis (GSM 04.08 9.1.38) ──────────────────

class L3SystemInformationType5bis {
    L3BCCHFrequencyList mBCCHFrequencyList;

    friend struct Builder;
public:
    static constexpr int MTI = 0x05;

    L3SystemInformationType5bis() = default;

    const L3BCCHFrequencyList& bcchFrequencyList() const { return mBCCHFrequencyList; }

    size_t bodyLength() const { return 17; }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return 17; }
    [[nodiscard]] static Expected<L3SystemInformationType5bis> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;

    struct Builder {
        L3BCCHFrequencyList mBCCHFrequencyList{};

        /// Set BCCH frequency list.
        Builder& bcchFrequencyList(L3BCCHFrequencyList v) { mBCCHFrequencyList = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3SystemInformationType5bis build() const;
    };

    static Builder builder();
};

// ── System Information Type 5ter (GSM 04.08 9.1.39) ──────────────────

class L3SystemInformationType5ter {
    L3BCCHFrequencyList mBCCHFrequencyList;

    friend struct Builder;
public:
    static constexpr int MTI = 0x06;

    L3SystemInformationType5ter() = default;

    const L3BCCHFrequencyList& bcchFrequencyList() const { return mBCCHFrequencyList; }

    size_t bodyLength() const { return 17; }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return 17; }
    [[nodiscard]] static Expected<L3SystemInformationType5ter> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;

    struct Builder {
        L3BCCHFrequencyList mBCCHFrequencyList{};

        /// Set BCCH frequency list.
        Builder& bcchFrequencyList(L3BCCHFrequencyList v) { mBCCHFrequencyList = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3SystemInformationType5ter build() const;
    };

    static Builder builder();
};

// ── System Information Type 6 (GSM 04.08 9.1.40) ─────────────────────

class L3SystemInformationType6 {
    L3CellIdentity mCI;
    L3LocationAreaIdentity mLAI;
    L3CellOptionsSACCH mCellOptions;
    L3NCCPermitted mNCCPermitted;

    friend struct Builder;
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

    struct Builder {
        L3CellIdentity mCI{};
        L3LocationAreaIdentity mLAI{};
        L3CellOptionsSACCH mCellOptions{};
        L3NCCPermitted mNCCPermitted{};

        /// Set cell identity.
        Builder& cellIdentity(L3CellIdentity v) { mCI = v; return *this; }
        /// Set location area identity.
        Builder& locationAreaIdentity(L3LocationAreaIdentity v) { mLAI = v; return *this; }
        /// Set cell options for SACCH.
        Builder& cellOptions(L3CellOptionsSACCH v) { mCellOptions = v; return *this; }
        /// Set NCC permitted.
        Builder& nccPermitted(L3NCCPermitted v) { mNCCPermitted = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3SystemInformationType6 build() const;
    };

    static Builder builder();
};

// ── System Information Type 7 (GSM 04.08 9.1.41) ─────────────────────

class L3SystemInformationType7 {
    L3RACHControlParameters mRACHControl;
    std::vector<L3CellChannelDescription> mCellChannelDescriptions;

    friend struct Builder;
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

    struct Builder {
        L3RACHControlParameters mRACHControl{};
        std::vector<L3CellChannelDescription> mCellChannelDescriptions;

        /// Set RACH control parameters.
        Builder& rachControl(L3RACHControlParameters v) { mRACHControl = v; return *this; }
        /// Add a cell channel description.
        Builder& addCellChannelDescription(L3CellChannelDescription v) { mCellChannelDescriptions.push_back(std::move(v)); return *this; }
        /// Set all cell channel descriptions.
        Builder& cellChannelDescriptions(std::vector<L3CellChannelDescription> v) { mCellChannelDescriptions = std::move(v); return *this; }
        /// Build the final message.
        [[nodiscard]] L3SystemInformationType7 build() const;
    };

    static Builder builder();
};

// ── System Information Type 8 (GSM 04.08 9.1.42) ─────────────────────

class L3SystemInformationType8 {
    L3NCCPermitted mNCCPermitted;
    L3RACHControlParameters mRACHControl;
    std::vector<L3CellChannelDescription> mCellChannelDescriptions;

    friend struct Builder;
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

    struct Builder {
        L3NCCPermitted mNCCPermitted{};
        L3RACHControlParameters mRACHControl{};
        std::vector<L3CellChannelDescription> mCellChannelDescriptions;

        /// Set NCC permitted.
        Builder& nccPermitted(L3NCCPermitted v) { mNCCPermitted = v; return *this; }
        /// Set RACH control parameters.
        Builder& rachControl(L3RACHControlParameters v) { mRACHControl = v; return *this; }
        /// Add a cell channel description.
        Builder& addCellChannelDescription(L3CellChannelDescription v) { mCellChannelDescriptions.push_back(std::move(v)); return *this; }
        /// Set all cell channel descriptions.
        Builder& cellChannelDescriptions(std::vector<L3CellChannelDescription> v) { mCellChannelDescriptions = std::move(v); return *this; }
        /// Build the final message.
        [[nodiscard]] L3SystemInformationType8 build() const;
    };

    static Builder builder();
};

// ── System Information Type 9 (GSM 04.08 9.1.43) ─────────────────────

class L3SystemInformationType9 {
    L3CellIdentity mCI;
    L3CellSelectionParameters mCellSelectionParameters;
    L3CellOptionsBCCH mCellOptions;

    friend struct Builder;
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

    struct Builder {
        L3CellIdentity mCI{};
        L3CellSelectionParameters mCellSelectionParameters{};
        L3CellOptionsBCCH mCellOptions{};

        /// Set cell identity.
        Builder& cellIdentity(L3CellIdentity v) { mCI = v; return *this; }
        /// Set cell selection parameters.
        Builder& cellSelectionParameters(L3CellSelectionParameters v) { mCellSelectionParameters = v; return *this; }
        /// Set cell options for BCCH.
        Builder& cellOptions(L3CellOptionsBCCH v) { mCellOptions = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3SystemInformationType9 build() const;
    };

    static Builder builder();
};

// ── System Information Type 13 (GSM 04.08 9.1.43a) ────────────────────

class L3SystemInformationType13 {
    L3SI13RestOctets mRestOctets;

    friend struct Builder;
public:
    static constexpr int MTI = 0x00;

    const L3SI13RestOctets& restOctets() const { return mRestOctets; }

    size_t bodyLength() const { return 3; }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return 3; }
    size_t restOctetsLength() const { return mRestOctets.lengthV(); }
    [[nodiscard]] static Expected<L3SystemInformationType13> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;

    struct Builder {
        L3SI13RestOctets mRestOctets{};

        /// Set rest octets.
        Builder& restOctets(L3SI13RestOctets v) { mRestOctets = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3SystemInformationType13 build() const;
    };

    static Builder builder();
};

// ── System Information Type 16 (GSM 04.08 9.1.43b) ───────────────────

class L3SystemInformationType16 {
    L3CellIdentity mCI;
    L3CellSelectionParameters mCellSelectionParameters;
    L3CellOptionsBCCH mCellOptions;

    friend struct Builder;
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

    struct Builder {
        L3CellIdentity mCI{};
        L3CellSelectionParameters mCellSelectionParameters{};
        L3CellOptionsBCCH mCellOptions{};

        /// Set cell identity.
        Builder& cellIdentity(L3CellIdentity v) { mCI = v; return *this; }
        /// Set cell selection parameters.
        Builder& cellSelectionParameters(L3CellSelectionParameters v) { mCellSelectionParameters = v; return *this; }
        /// Set cell options for BCCH.
        Builder& cellOptions(L3CellOptionsBCCH v) { mCellOptions = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3SystemInformationType16 build() const;
    };

    static Builder builder();
};

// ── System Information Type 17 (GSM 04.08 9.1.43c) ───────────────────

class L3SystemInformationType17 {
    L3RACHControlParameters mRACHControl;
    std::vector<L3CellChannelDescription> mCellChannelDescriptions;

    friend struct Builder;
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

    struct Builder {
        L3RACHControlParameters mRACHControl{};
        std::vector<L3CellChannelDescription> mCellChannelDescriptions;

        /// Set RACH control parameters.
        Builder& rachControl(L3RACHControlParameters v) { mRACHControl = v; return *this; }
        /// Add a cell channel description.
        Builder& addCellChannelDescription(L3CellChannelDescription v) { mCellChannelDescriptions.push_back(std::move(v)); return *this; }
        /// Set all cell channel descriptions.
        Builder& cellChannelDescriptions(std::vector<L3CellChannelDescription> v) { mCellChannelDescriptions = std::move(v); return *this; }
        /// Build the final message.
        [[nodiscard]] L3SystemInformationType17 build() const;
    };

    static Builder builder();
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

    friend struct Builder;
public:
    static constexpr int MTI = 0x3f;

    L3ImmediateAssignment() = default;

    const L3ChannelDescription& channelDescription() const { return mChannelDescription; }
    const L3RequestReference& requestReference() const { return mRequestReference; }
    const L3TimingAdvance& timingAdvance() const { return mTimingAdvance; }
    bool hasStartTime() const { return mStartTimePresent; }
    uint32_t startTimeFrame() const { return mStartTimeFrame; }

    struct Builder {
        L3PageMode mPageMode{};
        L3DedicatedModeOrTBF mDedicatedModeOrTBF{};
        L3RequestReference mRequestReference{};
        L3ChannelDescription mChannelDescription{};
        L3TimingAdvance mTimingAdvance{};
        std::vector<uint8_t> mMobileAllocation;
        bool mStartTimePresent{false};
        uint32_t mStartTimeFrame{0};

        /// Set the page mode (normal/urgent).
        Builder& pageMode(L3PageMode v) { mPageMode = v; return *this; }
        /// Set dedicated mode or TBF flag.
        Builder& dedicatedModeOrTBF(L3DedicatedModeOrTBF v) { mDedicatedModeOrTBF = v; return *this; }
        /// Set the request reference from RACH.
        Builder& requestReference(L3RequestReference v) { mRequestReference = v; return *this; }
        /// Set the channel description for assignment.
        Builder& channelDescription(L3ChannelDescription v) { mChannelDescription = v; return *this; }
        /// Set timing advance value.
        Builder& timingAdvance(L3TimingAdvance v) { mTimingAdvance = v; return *this; }
        /// Set mobile allocation list.
        Builder& mobileAllocation(std::vector<uint8_t> v) { mMobileAllocation = std::move(v); return *this; }
        /// Set optional start time frame.
        Builder& startTime(uint32_t fn, bool present = true) { mStartTimeFrame = fn; mStartTimePresent = present; return *this; }
        /// Build the final message.
        [[nodiscard]] L3ImmediateAssignment build() const;
    };

    static Builder builder();

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

    friend struct Builder;
public:
    static constexpr int MTI = 0x39;

    L3ImmediateAssignmentExtended() = default;

    const L3ChannelDescription& channelDescription() const { return mChannelDescription; }
    bool hasAdditionalChannel() const { return mHaveAdditionalChannel; }
    const L3AdditionalChannelDescription& additionalChannel() const { return mAdditionalChannel; }

    struct Builder {
        L3PageMode mPageMode{};
        L3DedicatedModeOrTBF mDedicatedModeOrTBF{};
        L3RequestReference mRequestReference{};
        L3ChannelDescription mChannelDescription{};
        L3TimingAdvance mTimingAdvance{};
        std::vector<uint8_t> mMobileAllocation;
        bool mStartTimePresent{false};
        uint32_t mStartTimeFrame{0};
        bool mHaveAdditionalChannel{false};
        L3AdditionalChannelDescription mAdditionalChannel;

        /// Set the page mode (normal/urgent).
        Builder& pageMode(L3PageMode v) { mPageMode = v; return *this; }
        /// Set dedicated mode or TBF flag.
        Builder& dedicatedModeOrTBF(L3DedicatedModeOrTBF v) { mDedicatedModeOrTBF = v; return *this; }
        /// Set the request reference from RACH.
        Builder& requestReference(L3RequestReference v) { mRequestReference = v; return *this; }
        /// Set the channel description for assignment.
        Builder& channelDescription(L3ChannelDescription v) { mChannelDescription = v; return *this; }
        /// Set timing advance value.
        Builder& timingAdvance(L3TimingAdvance v) { mTimingAdvance = v; return *this; }
        /// Set mobile allocation list.
        Builder& mobileAllocation(std::vector<uint8_t> v) { mMobileAllocation = std::move(v); return *this; }
        /// Set optional start time frame.
        Builder& startTime(uint32_t fn, bool present = true) { mStartTimeFrame = fn; mStartTimePresent = present; return *this; }
        /// Set additional channel description (sets mHaveAdditionalChannel flag).
        Builder& additionalChannel(L3AdditionalChannelDescription v) { mAdditionalChannel = v; mHaveAdditionalChannel = true; return *this; }
        /// Build the final message.
        [[nodiscard]] L3ImmediateAssignmentExtended build() const;
    };

    static Builder builder();

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

    friend struct Builder;
public:
    static constexpr int MTI = 0x3a;

    L3ImmediateAssignmentReject() = default;
    explicit L3ImmediateAssignmentReject(unsigned waitSeconds);

    unsigned pageMode() const { return mPageMode; }
    unsigned featureIndicator() const { return mFeatureIndicator; }
    unsigned waitTime() const { return mWaitIndication; }
    const std::vector<L3RequestReference>& requestReferences() const { return mRequestReferences; }

    struct Builder {
        unsigned mFeatureIndicator{0};
        unsigned mPageMode{0};
        std::vector<L3RequestReference> mRequestReferences;
        std::vector<unsigned> mWaitIndications;
        unsigned mWaitIndication{0};

        /// Set the feature indicator.
        Builder& featureIndicator(unsigned v) { mFeatureIndicator = v; return *this; }
        /// Set the page mode.
        Builder& pageMode(unsigned v) { mPageMode = v; return *this; }
        /// Add a wait indication with request reference and wait time in seconds.
        Builder& addWaitIndication(L3RequestReference ref, unsigned waitSeconds);
        /// Set simple wait time (sets mWaitIndication directly).
        Builder& waitTime(unsigned seconds) { mWaitIndication = seconds; return *this; }
        /// Build the final message.
        [[nodiscard]] L3ImmediateAssignmentReject build() const;
    };

    static Builder builder();

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

    friend struct Builder;
public:
    static constexpr int MTI = 0x3b;

    L3AdditionalAssignment() = default;

    const L3AdditionalChannelDescription& additionalChannel() const { return mAdditionalChannel; }
    bool hasPowerCommand() const { return mHavePowerCommand; }
    const L3PowerCommand& powerCommand() const { return mPowerCommand; }

    struct Builder {
        L3AdditionalChannelDescription mAdditionalChannel;
        bool mHavePowerCommand{false};
        L3PowerCommand mPowerCommand;

        /// Set the additional channel description.
        Builder& additionalChannel(L3AdditionalChannelDescription v) { mAdditionalChannel = v; return *this; }
        /// Set power command (sets mHavePowerCommand flag).
        Builder& powerCommand(L3PowerCommand v) { mPowerCommand = v; mHavePowerCommand = true; return *this; }
        /// Build the final message.
        [[nodiscard]] L3AdditionalAssignment build() const;
    };

    static Builder builder();

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

    friend struct Builder;
public:
    static constexpr int MTI = 0x2d;

    L3PhysicalInformation() = default;

    const L3TimingAdvance& timingAdvance() const { return mTA; }

    struct Builder {
        L3TimingAdvance mTA{};

        /// Set timing advance value.
        Builder& timingAdvance(L3TimingAdvance v) { mTA = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3PhysicalInformation build() const;
    };

    static Builder builder();

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
        /// Set the target cell description.
        Builder& cellDescription(const L3CellDescription& cd);
        /// Set the channel description after handover.
        Builder& channelDescriptionAfter(const L3ChannelDescription2& cda);
        /// Set the handover reference number.
        Builder& handoverReference(const L3HandoverReference& hr);
        /// Set power command and access type.
        Builder& powerCommandAccessType(const L3PowerCommandAndAccessType& pcat);
        /// Set synchronization indication.
        Builder& syncIndication(const L3SynchronizationIndication& si);
        /// Build the final message.
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

    friend struct Builder;
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

    struct Builder {
        L3CellIdentity mCellIdentity;
        L3LocationAreaIdentity mLocationAreaIdentity;

        /// Set cell identity.
        Builder& cellIdentity(L3CellIdentity v) { mCellIdentity = v; return *this; }
        /// Set location area identity.
        Builder& locationAreaIdentity(L3LocationAreaIdentity v) { mLocationAreaIdentity = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3SynchronizationChannelInformation build() const {
            L3SynchronizationChannelInformation msg;
            msg.mCellIdentity = mCellIdentity;
            msg.mLocationAreaIdentity = mLocationAreaIdentity;
            return msg;
        }
    };

    static Builder builder() { return Builder{}; }
};

// ── Channel Request (GSM 04.08 9.1.13) ────────────────────────────────
// Short message: no standard L3 header, 1 byte.

class L3ChannelRequest {
    unsigned mRequestReference{0};

    friend struct Builder;
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

    struct Builder {
        unsigned mRequestReference{0};

        /// Set request reference.
        Builder& requestReference(unsigned v) { mRequestReference = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3ChannelRequest build() const {
            L3ChannelRequest msg;
            msg.mRequestReference = mRequestReference;
            return msg;
        }
    };

    static Builder builder() { return Builder{}; }
};

// ── Handover Access (GSM 04.08 9.1.14a) ───────────────────────────────
// Short message: no standard L3 header, 4 bytes.

class L3HandoverAccess {
    unsigned mHandoverNumber{0};

    friend struct Builder;
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

    struct Builder {
        unsigned mHandoverNumber{0};

        /// Set handover number.
        Builder& handoverNumber(unsigned v) { mHandoverNumber = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3HandoverAccess build() const {
            L3HandoverAccess msg;
            msg.mHandoverNumber = mHandoverNumber;
            return msg;
        }
    };

    static Builder builder() { return Builder{}; }
};

// ── Configuration Change Command (GSM 04.08 9.1.4) ────────────────────

class L3ConfigurationChangeCommand {
    bool mHaveChanDesc{false};
    L3ChannelDescription mChanDesc;
    bool mHavePowerCmd{false};
    L3PowerCommand mPowerCmd;

    friend struct Builder;
public:
    static constexpr int MTI = 0x30;

    L3ConfigurationChangeCommand() = default;

    bool hasChannelDescription() const { return mHaveChanDesc; }
    const L3ChannelDescription& channelDescription() const { return mChanDesc; }
    bool hasPowerCommand() const { return mHavePowerCmd; }
    const L3PowerCommand& powerCommand() const { return mPowerCmd; }

    struct Builder {
        bool mHaveChanDesc{false};
        L3ChannelDescription mChanDesc;
        bool mHavePowerCmd{false};
        L3PowerCommand mPowerCmd;

        /// Set channel description (sets mHaveChanDesc flag).
        Builder& channelDescription(L3ChannelDescription v) { mChanDesc = v; mHaveChanDesc = true; return *this; }
        /// Set power command (sets mHavePowerCmd flag).
        Builder& powerCommand(L3PowerCommand v) { mPowerCmd = v; mHavePowerCmd = true; return *this; }
        /// Build the final message.
        [[nodiscard]] L3ConfigurationChangeCommand build() const;
    };

    static Builder builder();

    size_t bodyLength() const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
    [[nodiscard]] static Expected<L3ConfigurationChangeCommand> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── Configuration Change Acknowledge (GSM 04.08 9.1.4) ────────────────

class L3ConfigurationChangeAcknowledge {
public:
    static constexpr int MTI = 0x31;

    struct Builder {
        /// Build the final message.
        [[nodiscard]] L3ConfigurationChangeAcknowledge build() const;
    };
    friend struct Builder;
    static Builder builder();

    size_t bodyLength() const { return 0; }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return 0; }
    [[nodiscard]] static Expected<L3ConfigurationChangeAcknowledge> parse(BitReader&);
    void write(BitWriter&) const;
    void text(std::ostream& os) const;
};

// ── Configuration Change Reject (GSM 04.08 9.1.4) ─────────────────────

class L3ConfigurationChangeReject {
    RRCause mCause{RRCause::Normal_Event};

    friend struct Builder;
public:
    static constexpr int MTI = 0x33;

    L3ConfigurationChangeReject() = default;
    explicit L3ConfigurationChangeReject(RRCause cause) : mCause(cause) {}

    RRCause cause() const { return mCause; }

    struct Builder {
        RRCause mCause{RRCause::Normal_Event};

        /// Set the RR cause.
        Builder& cause(RRCause v) { mCause = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3ConfigurationChangeReject build() const;
    };

    static Builder builder();

    size_t bodyLength() const { return 1; }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return 1; }
    [[nodiscard]] static Expected<L3ConfigurationChangeReject> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── Partial Release (GSM 04.08 9.1.8) ─────────────────────────────────

class L3PartialRelease {
    L3ChannelDescription mChanDesc;

    friend struct Builder;
public:
    static constexpr int MTI = 0x0a;

    L3PartialRelease() = default;

    const L3ChannelDescription& channelDescription() const { return mChanDesc; }

    struct Builder {
        L3ChannelDescription mChanDesc;

        /// Set the channel description.
        Builder& channelDescription(L3ChannelDescription v) { mChanDesc = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3PartialRelease build() const;
    };

    static Builder builder();

    size_t bodyLength() const { return mChanDesc.lengthV(); }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
    [[nodiscard]] static Expected<L3PartialRelease> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── Partial Release Complete (GSM 04.08 9.1.8) ────────────────────────

class L3PartialReleaseComplete {
public:
    static constexpr int MTI = 0x0f;

    struct Builder {
        /// Build the final message.
        [[nodiscard]] L3PartialReleaseComplete build() const;
    };
    friend struct Builder;
    static Builder builder();

    size_t bodyLength() const { return 0; }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return 0; }
    [[nodiscard]] static Expected<L3PartialReleaseComplete> parse(BitReader&);
    void write(BitWriter&) const;
    void text(std::ostream& os) const;
};

// ── Extended Measurement Report (GSM 04.08 9.1.21a) ───────────────────

class L3ExtendedMeasurementReport {
    L3MeasurementResults mMeasurementResults;

    friend struct Builder;
public:
    static constexpr int MTI = 0x36;

    const L3MeasurementResults& measurementResults() const { return mMeasurementResults; }

    size_t bodyLength() const { return 16; }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return 16; }
    [[nodiscard]] static Expected<L3ExtendedMeasurementReport> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;

    struct Builder {
        L3MeasurementResults mMeasurementResults;

        /// Set measurement results.
        Builder& measurementResults(L3MeasurementResults v) { mMeasurementResults = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3ExtendedMeasurementReport build() const {
            L3ExtendedMeasurementReport msg;
            msg.mMeasurementResults = mMeasurementResults;
            return msg;
        }
    };

    static Builder builder() { return Builder{}; }
};

// ── Extended Measurement Order (GSM 04.08 9.1.21b) ────────────────────

class L3ExtendedMeasurementOrder {
    std::vector<uint8_t> mData;

    friend struct Builder;
public:
    static constexpr int MTI = 0x37;

    const std::vector<uint8_t>& data() const { return mData; }

    size_t bodyLength() const { return mData.size(); }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
    [[nodiscard]] static Expected<L3ExtendedMeasurementOrder> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;

    struct Builder {
        std::vector<uint8_t> mData;

        /// Set data payload.
        Builder& data(std::vector<uint8_t> v) { mData = std::move(v); return *this; }
        /// Build the final message.
        [[nodiscard]] L3ExtendedMeasurementOrder build() const {
            L3ExtendedMeasurementOrder msg;
            msg.mData = mData;
            return msg;
        }
    };

    static Builder builder() { return Builder{}; }
};

// ── Frequency Redefinition (GSM 04.08 9.1.13a) ────────────────────────

class L3FrequencyRedefinition {
    L3FrequencyList mCellChannelDescription;
    L3RACHControlParameters mRACHControlParameters;

    friend struct Builder;
public:
    static constexpr int MTI = 0x14;

    const L3FrequencyList& cellChannelDescription() const { return mCellChannelDescription; }
    const L3RACHControlParameters& rachControl() const { return mRACHControlParameters; }

    size_t bodyLength() const { return mCellChannelDescription.lengthV() + mRACHControlParameters.lengthV(); }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
    [[nodiscard]] static Expected<L3FrequencyRedefinition> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;

    struct Builder {
        L3FrequencyList mCellChannelDescription;
        L3RACHControlParameters mRACHControlParameters;

        /// Set cell channel description (frequency list).
        Builder& cellChannelDescription(L3FrequencyList v) { mCellChannelDescription = v; return *this; }
        /// Set RACH control parameters.
        Builder& rachControlParameters(L3RACHControlParameters v) { mRACHControlParameters = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3FrequencyRedefinition build() const {
            L3FrequencyRedefinition msg;
            msg.mCellChannelDescription = mCellChannelDescription;
            msg.mRACHControlParameters = mRACHControlParameters;
            return msg;
        }
    };

    static Builder builder() { return Builder{}; }
};

// ── Notification NCH (GSM 04.08 9.1.26) ───────────────────────────────
// Note: MTI=0x20 per GSM_RR_Types.ttcn NOTIFICATION_NCH='00100000'B.
// Transmitted on CBCH; if same wire bytes could be SI Type 13 (MTI=0x00),
// disambiguation is by channel context, not L3 parser.

class L3NotificationNCH {
    std::vector<uint8_t> mData;

    friend struct Builder;
public:
    static constexpr int MTI = 0x20;

    std::vector<uint8_t>& data() { return mData; }
    const std::vector<uint8_t>& data() const { return mData; }

    size_t bodyLength() const { return mData.size(); }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
    [[nodiscard]] static Expected<L3NotificationNCH> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;

    struct Builder {
        std::vector<uint8_t> mData;

        /// Set data payload.
        Builder& data(std::vector<uint8_t> v) { mData = std::move(v); return *this; }
        /// Build the final message.
        [[nodiscard]] L3NotificationNCH build() const {
            L3NotificationNCH msg;
            msg.mData = mData;
            return msg;
        }
    };

    static Builder builder() { return Builder{}; }
};

// ── Notification Response (GSM 04.08 9.1.27) ──────────────────────────

class L3NotificationResponse {
    std::vector<uint8_t> mData;

    friend struct Builder;
public:
    static constexpr int MTI = 0x26;

    std::vector<uint8_t>& data() { return mData; }
    const std::vector<uint8_t>& data() const { return mData; }

    size_t bodyLength() const { return mData.size(); }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
    [[nodiscard]] static Expected<L3NotificationResponse> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;

    struct Builder {
        std::vector<uint8_t> mData;

        /// Set data payload.
        Builder& data(std::vector<uint8_t> v) { mData = std::move(v); return *this; }
        /// Build the final message.
        [[nodiscard]] L3NotificationResponse build() const {
            L3NotificationResponse msg;
            msg.mData = mData;
            return msg;
        }
    };

    static Builder builder() { return Builder{}; }
};

// ── VGCS Uplink Grant (GSM 04.08 9.1.28) ──────────────────────────────
// Note: MTI=0x09 conflicts with no existing RR message, but plan notes potential issues.

class L3VGCSUplinkGrant {
public:
    static constexpr int MTI = 0x09;

    size_t bodyLength() const { return 0; }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return 0; }
    [[nodiscard]] static Expected<L3VGCSUplinkGrant> parse(BitReader&);
    void write(BitWriter&) const;
    void text(std::ostream& os) const;

    struct Builder {
        /// Build the final message.
        [[nodiscard]] L3VGCSUplinkGrant build() const;
    };
    friend struct Builder;
    static Builder builder() { return Builder{}; }
};

// ── Uplink Release (GSM 04.08 9.1.28a) ────────────────────────────────
// Note: MTI=0x0e conflicts with CC EmergencySetup; resolved by PD context.

class L3UplinkRelease {
public:
    static constexpr int MTI = 0x0e;

    size_t bodyLength() const { return 0; }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return 0; }
    [[nodiscard]] static Expected<L3UplinkRelease> parse(BitReader&);
    void write(BitWriter&) const;
    void text(std::ostream& os) const;

    struct Builder {
        /// Build the final message.
        [[nodiscard]] L3UplinkRelease build() const;
    };
    friend struct Builder;
    static Builder builder() { return Builder{}; }
};

// ── Uplink Busy (GSM 04.08 9.1.28b) ───────────────────────────────────
// Note: MTI=0x2a conflicts with CC ReleaseComplete; resolved by PD context.

class L3UplinkBusy {
public:
    static constexpr int MTI = 0x2a;

    size_t bodyLength() const { return 0; }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return 0; }
    [[nodiscard]] static Expected<L3UplinkBusy> parse(BitReader&);
    void write(BitWriter&) const;
    void text(std::ostream& os) const;

    struct Builder {
        /// Build the final message.
        [[nodiscard]] L3UplinkBusy build() const;
    };
    friend struct Builder;
    static Builder builder() { return Builder{}; }
};

// ── Talker Indication (GSM 04.08 9.1.28c) ─────────────────────────────
// MTI=0x11 per GSM_RR_Types.ttcn TALKER_INDICATION='00010001'B.

class L3TalkerIndication {
public:
    static constexpr int MTI = 0x11;

    size_t bodyLength() const { return 0; }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return 0; }
    [[nodiscard]] static Expected<L3TalkerIndication> parse(BitReader&);
    void write(BitWriter&) const;
    void text(std::ostream& os) const;

    struct Builder {
        /// Build the final message.
        [[nodiscard]] L3TalkerIndication build() const;
    };
    friend struct Builder;
    static Builder builder() { return Builder{}; }
};

// ── Priority Uplink Request (GSM 04.08 9.1.28d) ───────────────────────

class L3PriorityUplinkRequest {
    uint32_t mTMSI{0};

    friend struct Builder;
public:
    static constexpr int MTI = 0x66;

    uint32_t tmsi() const { return mTMSI; }

    size_t bodyLength() const { return 4; }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return 4; }
    [[nodiscard]] static Expected<L3PriorityUplinkRequest> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;

    struct Builder {
        uint32_t mTMSI{0};

        /// Set TMSI value.
        Builder& tmsi(uint32_t v) { mTMSI = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3PriorityUplinkRequest build() const {
            L3PriorityUplinkRequest msg;
            msg.mTMSI = mTMSI;
            return msg;
        }
    };

    static Builder builder() { return Builder{}; }
};

// ── Data Indication (GSM 04.08 9.1.28e) ───────────────────────────────

class L3DataIndication {
    std::vector<uint8_t> mData;

    friend struct Builder;
public:
    static constexpr int MTI = 0x67;

    std::vector<uint8_t>& data() { return mData; }
    const std::vector<uint8_t>& data() const { return mData; }

    size_t bodyLength() const { return mData.size(); }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
    [[nodiscard]] static Expected<L3DataIndication> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;

    struct Builder {
        std::vector<uint8_t> mData;

        /// Set data payload.
        Builder& data(std::vector<uint8_t> v) { mData = std::move(v); return *this; }
        /// Build the final message.
        [[nodiscard]] L3DataIndication build() const {
            L3DataIndication msg;
            msg.mData = mData;
            return msg;
        }
    };

    static Builder builder() { return Builder{}; }
};

// ── Data Indication 2 (GSM 04.08 9.1.28f) ─────────────────────────────

class L3DataIndication2 {
    std::vector<uint8_t> mData;

    friend struct Builder;
public:
    static constexpr int MTI = 0x68;

    std::vector<uint8_t>& data() { return mData; }
    const std::vector<uint8_t>& data() const { return mData; }

    size_t bodyLength() const { return mData.size(); }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
    [[nodiscard]] static Expected<L3DataIndication2> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;

    struct Builder {
        std::vector<uint8_t> mData;

        /// Set data payload.
        Builder& data(std::vector<uint8_t> v) { mData = std::move(v); return *this; }
        /// Build the final message.
        [[nodiscard]] L3DataIndication2 build() const {
            L3DataIndication2 msg;
            msg.mData = mData;
            return msg;
        }
    };

    static Builder builder() { return Builder{}; }
};

// ── DTM Assignment Failure (GSM 04.08 9.1.3d) ─────────────────────────

class L3DTMAssignmentFailure {
    RRCause mCause{RRCause::Normal_Event};

    friend struct Builder;
public:
    static constexpr int MTI = 0x80;

    L3DTMAssignmentFailure() = default;
    explicit L3DTMAssignmentFailure(RRCause cause) : mCause(cause) {}

    RRCause cause() const { return mCause; }

    size_t bodyLength() const { return 1; }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return 1; }
    [[nodiscard]] static Expected<L3DTMAssignmentFailure> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;

    struct Builder {
        RRCause mCause{RRCause::Normal_Event};

        /// Set the RR cause.
        Builder& cause(RRCause v) { mCause = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3DTMAssignmentFailure build() const {
            L3DTMAssignmentFailure msg;
            msg.mCause = mCause;
            return msg;
        }
    };

    static Builder builder() { return Builder{}; }
};

// ── DTM Reject (GSM 04.08 9.1.3d) ─────────────────────────────────────

class L3DTMReject {
public:
    static constexpr int MTI = 0x81;

    size_t bodyLength() const { return 0; }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return 0; }
    [[nodiscard]] static Expected<L3DTMReject> parse(BitReader&);
    void write(BitWriter&) const;
    void text(std::ostream& os) const;

    struct Builder {
        /// Build the final message.
        [[nodiscard]] L3DTMReject build() const;
    };
    friend struct Builder;
    static Builder builder() { return Builder{}; }
};

// ── DTM Request (GSM 04.08 9.1.3d) ────────────────────────────────────

class L3DTMRequest {
public:
    static constexpr int MTI = 0x82;

    size_t bodyLength() const { return 0; }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return 0; }
    [[nodiscard]] static Expected<L3DTMRequest> parse(BitReader&);
    void write(BitWriter&) const;
    void text(std::ostream& os) const;

    struct Builder {
        /// Build the final message.
        [[nodiscard]] L3DTMRequest build() const;
    };
    friend struct Builder;
    static Builder builder() { return Builder{}; }
};

// ── Packet Assignment (GSM 04.08 9.1.3e) ───────────────────────────────

class L3PacketAssignment {
    L3ChannelDescription mChanDesc;
    L3TimingAdvance mTA;

    friend struct Builder;
public:
    static constexpr int MTI = 0x83;

    const L3ChannelDescription& channelDescription() const { return mChanDesc; }
    const L3TimingAdvance& timingAdvance() const { return mTA; }

    size_t bodyLength() const { return mChanDesc.lengthV() + mTA.lengthV(); }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
    [[nodiscard]] static Expected<L3PacketAssignment> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;

    struct Builder {
        L3ChannelDescription mChanDesc;
        L3TimingAdvance mTA;

        /// Set channel description.
        Builder& channelDescription(L3ChannelDescription v) { mChanDesc = v; return *this; }
        /// Set timing advance.
        Builder& timingAdvance(L3TimingAdvance v) { mTA = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3PacketAssignment build() const {
            L3PacketAssignment msg;
            msg.mChanDesc = mChanDesc;
            msg.mTA = mTA;
            return msg;
        }
    };

    static Builder builder() { return Builder{}; }
};

// ── DTM Assignment Command (GSM 04.08 9.1.3d) ─────────────────────────

class L3DTMAssignmentCommand {
public:
    static constexpr int MTI = 0x84;

    size_t bodyLength() const { return 0; }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return 0; }
    [[nodiscard]] static Expected<L3DTMAssignmentCommand> parse(BitReader&);
    void write(BitWriter&) const;
    void text(std::ostream& os) const;

    struct Builder {
        /// Build the final message.
        [[nodiscard]] L3DTMAssignmentCommand build() const;
    };
    friend struct Builder;
    static Builder builder() { return Builder{}; }
};

// ── DTM Information (GSM 04.08 9.1.3d) ────────────────────────────────

class L3DTMInformation {
public:
    static constexpr int MTI = 0x85;

    size_t bodyLength() const { return 0; }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return 0; }
    [[nodiscard]] static Expected<L3DTMInformation> parse(BitReader&);
    void write(BitWriter&) const;
    void text(std::ostream& os) const;

    struct Builder {
        /// Build the final message.
        [[nodiscard]] L3DTMInformation build() const;
    };
    friend struct Builder;
    static Builder builder() { return Builder{}; }
};

// ── Packet Information (GSM 04.08 9.1.3e) ─────────────────────────────

class L3PacketInformation {
public:
    static constexpr int MTI = 0x86;

    size_t bodyLength() const { return 0; }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return 0; }
    [[nodiscard]] static Expected<L3PacketInformation> parse(BitReader&);
    void write(BitWriter&) const;
    void text(std::ostream& os) const;

    struct Builder {
        /// Build the final message.
        [[nodiscard]] L3PacketInformation build() const;
    };
    friend struct Builder;
    static Builder builder() { return Builder{}; }
};

// ── UTRAN Classmark Change (GSM 04.08 9.1.11a) ────────────────────────

class L3UTRANClassmarkChange {
    std::vector<uint8_t> mClassmark;

    friend struct Builder;
public:
    static constexpr int MTI = 0x60;

    std::vector<uint8_t>& classmark() { return mClassmark; }
    const std::vector<uint8_t>& classmark() const { return mClassmark; }

    size_t bodyLength() const { return mClassmark.size(); }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
    [[nodiscard]] static Expected<L3UTRANClassmarkChange> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;

    struct Builder {
        std::vector<uint8_t> mClassmark;

        /// Set classmark data.
        Builder& classmark(std::vector<uint8_t> v) { mClassmark = std::move(v); return *this; }
        /// Build the final message.
        [[nodiscard]] L3UTRANClassmarkChange build() const {
            L3UTRANClassmarkChange msg;
            msg.mClassmark = mClassmark;
            return msg;
        }
    };

    static Builder builder() { return Builder{}; }
};

// ── CDMA2000 Classmark Change (GSM 04.08 9.1.11b) ────────────────────

class L3CDMA2000ClassmarkChange {
    std::vector<uint8_t> mClassmark;

    friend struct Builder;
public:
    static constexpr int MTI = 0x62;

    std::vector<uint8_t>& classmark() { return mClassmark; }
    const std::vector<uint8_t>& classmark() const { return mClassmark; }

    size_t bodyLength() const { return mClassmark.size(); }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
    [[nodiscard]] static Expected<L3CDMA2000ClassmarkChange> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;

    struct Builder {
        std::vector<uint8_t> mClassmark;

        /// Set classmark data.
        Builder& classmark(std::vector<uint8_t> v) { mClassmark = std::move(v); return *this; }
        /// Build the final message.
        [[nodiscard]] L3CDMA2000ClassmarkChange build() const {
            L3CDMA2000ClassmarkChange msg;
            msg.mClassmark = mClassmark;
            return msg;
        }
    };

    static Builder builder() { return Builder{}; }
};

// ── Intersys to UTRAN HO Command (GSM 04.08 9.1.15a) ─────────────────

class L3IntersysToUTRANHOCommand {
    std::vector<uint8_t> mData;

    friend struct Builder;
public:
    static constexpr int MTI = 0x63;

    std::vector<uint8_t>& data() { return mData; }
    const std::vector<uint8_t>& data() const { return mData; }

    size_t bodyLength() const { return mData.size(); }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
    [[nodiscard]] static Expected<L3IntersysToUTRANHOCommand> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;

    struct Builder {
        std::vector<uint8_t> mData;

        /// Set data payload.
        Builder& data(std::vector<uint8_t> v) { mData = std::move(v); return *this; }
        /// Build the final message.
        [[nodiscard]] L3IntersysToUTRANHOCommand build() const {
            L3IntersysToUTRANHOCommand msg;
            msg.mData = mData;
            return msg;
        }
    };

    static Builder builder() { return Builder{}; }
};

// ── Intersys to CDMA2000 HO Command (GSM 04.08 9.1.15b) ───────────────

class L3IntersysToCDMA2000HOCommand {
    std::vector<uint8_t> mData;

    friend struct Builder;
public:
    static constexpr int MTI = 0x64;

    std::vector<uint8_t>& data() { return mData; }
    const std::vector<uint8_t>& data() const { return mData; }

    size_t bodyLength() const { return mData.size(); }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
    [[nodiscard]] static Expected<L3IntersysToCDMA2000HOCommand> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;

    struct Builder {
        std::vector<uint8_t> mData;

        /// Set data payload.
        Builder& data(std::vector<uint8_t> v) { mData = std::move(v); return *this; }
        /// Build the final message.
        [[nodiscard]] L3IntersysToCDMA2000HOCommand build() const {
            L3IntersysToCDMA2000HOCommand msg;
            msg.mData = mData;
            return msg;
        }
    };

    static Builder builder() { return Builder{}; }
};

// ── GERAN IU Mode Classmark Change (GSM 04.08 9.1.11c) ────────────────

class L3GERANIUClassmarkChange {
    std::vector<uint8_t> mClassmark;

    friend struct Builder;
public:
    static constexpr int MTI = 0x65;

    std::vector<uint8_t>& classmark() { return mClassmark; }
    const std::vector<uint8_t>& classmark() const { return mClassmark; }

    size_t bodyLength() const { return mClassmark.size(); }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
    [[nodiscard]] static Expected<L3GERANIUClassmarkChange> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;

    struct Builder {
        std::vector<uint8_t> mClassmark;

        /// Set classmark data.
        Builder& classmark(std::vector<uint8_t> v) { mClassmark = std::move(v); return *this; }
        /// Build the final message.
        [[nodiscard]] L3GERANIUClassmarkChange build() const {
            L3GERANIUClassmarkChange msg;
            msg.mClassmark = mClassmark;
            return msg;
        }
    };

    static Builder builder() { return Builder{}; }
};

// ── System Information Type 14 (GSM 04.08 9.1.43d) ────────────────────

class L3SystemInformationType14 {
    L3CellIdentity mCI;
    L3CellSelectionParameters mCellSelectionParameters;

    friend struct Builder;
public:
    static constexpr int MTI = 0x01;

    const L3CellIdentity& ci() const { return mCI; }
    const L3CellSelectionParameters& cellSelectionParameters() const { return mCellSelectionParameters; }

    size_t bodyLength() const { return 5; }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return 5; }
    [[nodiscard]] static Expected<L3SystemInformationType14> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;

    struct Builder {
        L3CellIdentity mCI;
        L3CellSelectionParameters mCellSelectionParameters;

        /// Set cell identity.
        Builder& ci(L3CellIdentity v) { mCI = v; return *this; }
        /// Set cell selection parameters.
        Builder& cellSelectionParameters(L3CellSelectionParameters v) { mCellSelectionParameters = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3SystemInformationType14 build() const {
            L3SystemInformationType14 msg;
            msg.mCI = mCI;
            msg.mCellSelectionParameters = mCellSelectionParameters;
            return msg;
        }
    };

    static Builder builder() { return Builder{}; }
};

// ── System Information Type 15 (GSM 04.08 9.1.43e) ────────────────────

class L3SystemInformationType15 {
public:
    static constexpr int MTI = 0x43;

    size_t bodyLength() const { return 0; }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return 0; }
    [[nodiscard]] static Expected<L3SystemInformationType15> parse(BitReader&);
    void write(BitWriter&) const;
    void text(std::ostream& os) const;

    struct Builder {
        /// Build the final message.
        [[nodiscard]] L3SystemInformationType15 build() const;
    };
    friend struct Builder;
    static Builder builder() { return Builder{}; }
};

// ── System Information Type 18 (GSM 04.08 9.1.43f) ────────────────────

class L3SystemInformationType18 {
    L3RACHControlParameters mRACHControl;
    std::vector<L3CellChannelDescription> mCellChannelDescriptions;

    friend struct Builder;
public:
    static constexpr int MTI = 0x40;

    const L3RACHControlParameters& rachControl() const { return mRACHControl; }
    const std::vector<L3CellChannelDescription>& cellChannelDescriptions() const { return mCellChannelDescriptions; }

    size_t bodyLength() const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
    [[nodiscard]] static Expected<L3SystemInformationType18> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;

    struct Builder {
        L3RACHControlParameters mRACHControl;
        std::vector<L3CellChannelDescription> mCellChannelDescriptions;

        /// Set RACH control parameters.
        Builder& rachControl(L3RACHControlParameters v) { mRACHControl = v; return *this; }
        /// Set cell channel descriptions.
        Builder& cellChannelDescriptions(std::vector<L3CellChannelDescription> v) { mCellChannelDescriptions = std::move(v); return *this; }
        /// Build the final message.
        [[nodiscard]] L3SystemInformationType18 build() const {
            L3SystemInformationType18 msg;
            msg.mRACHControl = mRACHControl;
            msg.mCellChannelDescriptions = mCellChannelDescriptions;
            return msg;
        }
    };

    static Builder builder() { return Builder{}; }
};

// ── System Information Type 19 (GSM 04.08 9.1.43g) ────────────────────

class L3SystemInformationType19 {
    L3RACHControlParameters mRACHControl;
    std::vector<L3CellChannelDescription> mCellChannelDescriptions;

    friend struct Builder;
public:
    static constexpr int MTI = 0x41;

    const L3RACHControlParameters& rachControl() const { return mRACHControl; }
    const std::vector<L3CellChannelDescription>& cellChannelDescriptions() const { return mCellChannelDescriptions; }

    size_t bodyLength() const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
    [[nodiscard]] static Expected<L3SystemInformationType19> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;

    struct Builder {
        L3RACHControlParameters mRACHControl;
        std::vector<L3CellChannelDescription> mCellChannelDescriptions;

        /// Set RACH control parameters.
        Builder& rachControl(L3RACHControlParameters v) { mRACHControl = v; return *this; }
        /// Set cell channel descriptions.
        Builder& cellChannelDescriptions(std::vector<L3CellChannelDescription> v) { mCellChannelDescriptions = std::move(v); return *this; }
        /// Build the final message.
        [[nodiscard]] L3SystemInformationType19 build() const {
            L3SystemInformationType19 msg;
            msg.mRACHControl = mRACHControl;
            msg.mCellChannelDescriptions = mCellChannelDescriptions;
            return msg;
        }
    };

    static Builder builder() { return Builder{}; }
};

// ── System Information Type 20 (GSM 04.08 9.1.43h) ────────────────────

class L3SystemInformationType20 {
    L3RACHControlParameters mRACHControl;
    std::vector<L3CellChannelDescription> mCellChannelDescriptions;

    friend struct Builder;
public:
    static constexpr int MTI = 0x42;

    const L3RACHControlParameters& rachControl() const { return mRACHControl; }
    const std::vector<L3CellChannelDescription>& cellChannelDescriptions() const { return mCellChannelDescriptions; }

    size_t bodyLength() const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
    [[nodiscard]] static Expected<L3SystemInformationType20> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;

    struct Builder {
        L3RACHControlParameters mRACHControl;
        std::vector<L3CellChannelDescription> mCellChannelDescriptions;

        /// Set RACH control parameters.
        Builder& rachControl(L3RACHControlParameters v) { mRACHControl = v; return *this; }
        /// Set cell channel descriptions.
        Builder& cellChannelDescriptions(std::vector<L3CellChannelDescription> v) { mCellChannelDescriptions = std::move(v); return *this; }
        /// Build the final message.
        [[nodiscard]] L3SystemInformationType20 build() const {
            L3SystemInformationType20 msg;
            msg.mRACHControl = mRACHControl;
            msg.mCellChannelDescriptions = mCellChannelDescriptions;
            return msg;
        }
    };

    static Builder builder() { return Builder{}; }
};

// ── System Information Type 13alt (GSM 04.08 9.1.43a) ─────────────────

class L3SystemInformationType13alt {
public:
    static constexpr int MTI = 0x44;

    size_t bodyLength() const { return 0; }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return 0; }
    [[nodiscard]] static Expected<L3SystemInformationType13alt> parse(BitReader&);
    void write(BitWriter&) const;
    void text(std::ostream& os) const;

    struct Builder {
        /// Build the final message.
        [[nodiscard]] L3SystemInformationType13alt build() const;
    };
    friend struct Builder;
    static Builder builder() { return Builder{}; }
};

// ── System Information Type 2n (GSM 04.08 9.1.43i) ────────────────────

class L3SystemInformationType2n {
public:
    static constexpr int MTI = 0x45;

    size_t bodyLength() const { return 0; }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return 0; }
    [[nodiscard]] static Expected<L3SystemInformationType2n> parse(BitReader&);
    void write(BitWriter&) const;
    void text(std::ostream& os) const;

    struct Builder {
        /// Build the final message.
        [[nodiscard]] L3SystemInformationType2n build() const;
    };
    friend struct Builder;
    static Builder builder() { return Builder{}; }
};

// ── System Information Type 21 (GSM 04.08 9.1.43j) ────────────────────

class L3SystemInformationType21 {
public:
    static constexpr int MTI = 0x46;

    size_t bodyLength() const { return 0; }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return 0; }
    [[nodiscard]] static Expected<L3SystemInformationType21> parse(BitReader&);
    void write(BitWriter&) const;
    void text(std::ostream& os) const;

    struct Builder {
        /// Build the final message.
        [[nodiscard]] L3SystemInformationType21 build() const;
    };
    friend struct Builder;
    static Builder builder() { return Builder{}; }
};

// ── System Information Type 22 (GSM 04.08 9.1.43k) ────────────────────

class L3SystemInformationType22 {
public:
    static constexpr int MTI = 0x47;

    size_t bodyLength() const { return 0; }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return 0; }
    [[nodiscard]] static Expected<L3SystemInformationType22> parse(BitReader&);
    void write(BitWriter&) const;
    void text(std::ostream& os) const;

    struct Builder {
        /// Build the final message.
        [[nodiscard]] L3SystemInformationType22 build() const;
    };
    friend struct Builder;
    static Builder builder() { return Builder{}; }
};

// ── System Information Type 23 (GSM 04.08 9.1.43l) ────────────────────

class L3SystemInformationType23 {
public:
    static constexpr int MTI = 0x4f;

    size_t bodyLength() const { return 0; }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return 0; }
    [[nodiscard]] static Expected<L3SystemInformationType23> parse(BitReader&);
    void write(BitWriter&) const;
    void text(std::ostream& os) const;

    struct Builder {
        /// Build the final message.
        [[nodiscard]] L3SystemInformationType23 build() const;
    };
    friend struct Builder;
    static Builder builder() { return Builder{}; }
};

// ── System Information Type 10 (GSM 04.08 9.1.44) ─────────────────────
// Short message: no standard L3 header, sent on BCCH.

class L3SystemInformationType10 {
    L3CellIdentity mCI;
    L3LocationAreaIdentity mLAI;
    L3CellOptionsBCCH mCellOptions;
    L3CellSelectionParameters mCellSelectionParameters;

    friend struct Builder;
public:
    static constexpr int MTI = 0x106;

    const L3CellIdentity& ci() const { return mCI; }
    const L3LocationAreaIdentity& lai() const { return mLAI; }
    const L3CellOptionsBCCH& cellOptions() const { return mCellOptions; }
    const L3CellSelectionParameters& cellSelectionParameters() const { return mCellSelectionParameters; }

    size_t bodyLength() const { return 10; }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
    [[nodiscard]] static Expected<L3SystemInformationType10> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;

    struct Builder {
        L3CellIdentity mCI;
        L3LocationAreaIdentity mLAI;
        L3CellOptionsBCCH mCellOptions;
        L3CellSelectionParameters mCellSelectionParameters;

        /// Set cell identity.
        Builder& ci(L3CellIdentity v) { mCI = v; return *this; }
        /// Set location area identity.
        Builder& lai(L3LocationAreaIdentity v) { mLAI = v; return *this; }
        /// Set cell options for BCCH.
        Builder& cellOptions(L3CellOptionsBCCH v) { mCellOptions = v; return *this; }
        /// Set cell selection parameters.
        Builder& cellSelectionParameters(L3CellSelectionParameters v) { mCellSelectionParameters = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3SystemInformationType10 build() const {
            L3SystemInformationType10 msg;
            msg.mCI = mCI;
            msg.mLAI = mLAI;
            msg.mCellOptions = mCellOptions;
            msg.mCellSelectionParameters = mCellSelectionParameters;
            return msg;
        }
    };

    static Builder builder() { return Builder{}; }
};

// ── System Information Type 10bis (GSM 04.08 9.1.44a) ─────────────────
// Short message: no standard L3 header, sent on BCCH.

class L3SystemInformationType10bis {
    L3CellIdentity mCI;
    L3LocationAreaIdentity mLAI;
    L3CellOptionsBCCH mCellOptions;
    L3CellSelectionParameters mCellSelectionParameters;

    friend struct Builder;
public:
    static constexpr int MTI = 0x107;

    const L3CellIdentity& ci() const { return mCI; }
    const L3LocationAreaIdentity& lai() const { return mLAI; }
    const L3CellOptionsBCCH& cellOptions() const { return mCellOptions; }
    const L3CellSelectionParameters& cellSelectionParameters() const { return mCellSelectionParameters; }

    size_t bodyLength() const { return 10; }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
    [[nodiscard]] static Expected<L3SystemInformationType10bis> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;

    struct Builder {
        L3CellIdentity mCI;
        L3LocationAreaIdentity mLAI;
        L3CellOptionsBCCH mCellOptions;
        L3CellSelectionParameters mCellSelectionParameters;

        /// Set cell identity.
        Builder& ci(L3CellIdentity v) { mCI = v; return *this; }
        /// Set location area identity.
        Builder& lai(L3LocationAreaIdentity v) { mLAI = v; return *this; }
        /// Set cell options for BCCH.
        Builder& cellOptions(L3CellOptionsBCCH v) { mCellOptions = v; return *this; }
        /// Set cell selection parameters.
        Builder& cellSelectionParameters(L3CellSelectionParameters v) { mCellSelectionParameters = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3SystemInformationType10bis build() const {
            L3SystemInformationType10bis msg;
            msg.mCI = mCI;
            msg.mLAI = mLAI;
            msg.mCellOptions = mCellOptions;
            msg.mCellSelectionParameters = mCellSelectionParameters;
            return msg;
        }
    };

    static Builder builder() { return Builder{}; }
};

// ── System Information Type 10ter (GSM 04.08 9.1.44b) ─────────────────
// Short message: no standard L3 header, sent on BCCH.

class L3SystemInformationType10ter {
    L3CellIdentity mCI;
    L3LocationAreaIdentity mLAI;
    L3CellOptionsBCCH mCellOptions;
    L3CellSelectionParameters mCellSelectionParameters;

    friend struct Builder;
public:
    static constexpr int MTI = 0x108;

    const L3CellIdentity& ci() const { return mCI; }
    const L3LocationAreaIdentity& lai() const { return mLAI; }
    const L3CellOptionsBCCH& cellOptions() const { return mCellOptions; }
    const L3CellSelectionParameters& cellSelectionParameters() const { return mCellSelectionParameters; }

    size_t bodyLength() const { return 10; }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
    [[nodiscard]] static Expected<L3SystemInformationType10ter> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;

    struct Builder {
        L3CellIdentity mCI;
        L3LocationAreaIdentity mLAI;
        L3CellOptionsBCCH mCellOptions;
        L3CellSelectionParameters mCellSelectionParameters;

        /// Set cell identity.
        Builder& ci(L3CellIdentity v) { mCI = v; return *this; }
        /// Set location area identity.
        Builder& lai(L3LocationAreaIdentity v) { mLAI = v; return *this; }
        /// Set cell options for BCCH.
        Builder& cellOptions(L3CellOptionsBCCH v) { mCellOptions = v; return *this; }
        /// Set cell selection parameters.
        Builder& cellSelectionParameters(L3CellSelectionParameters v) { mCellSelectionParameters = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3SystemInformationType10ter build() const {
            L3SystemInformationType10ter msg;
            msg.mCI = mCI;
            msg.mLAI = mLAI;
            msg.mCellOptions = mCellOptions;
            msg.mCellSelectionParameters = mCellSelectionParameters;
            return msg;
        }
    };

    static Builder builder() { return Builder{}; }
};

// ── Notification FACCH (GSM 04.08 9.1.45) ─────────────────────────────
// Short message: no standard L3 header, sent on FACCH.

class L3NotificationFACCH {
public:
    static constexpr int MTI = 0x109;

    size_t bodyLength() const { return 0; }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return 0; }
    [[nodiscard]] static Expected<L3NotificationFACCH> parse(BitReader&);
    void write(BitWriter&) const;
    void text(std::ostream& os) const;

    struct Builder {
        /// Build the final message.
        [[nodiscard]] L3NotificationFACCH build() const;
    };
    friend struct Builder;
    static Builder builder() { return Builder{}; }
};

// ── Uplink Free (GSM 04.08 9.1.45a) ───────────────────────────────────
// Short message: no standard L3 header, sent on FACCH.

class L3UplinkFree {
public:
    static constexpr int MTI = 0x10A;

    size_t bodyLength() const { return 0; }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return 0; }
    [[nodiscard]] static Expected<L3UplinkFree> parse(BitReader&);
    void write(BitWriter&) const;
    void text(std::ostream& os) const;

    struct Builder {
        /// Build the final message.
        [[nodiscard]] L3UplinkFree build() const;
    };
    friend struct Builder;
    static Builder builder() { return Builder{}; }
};

// ── Enhanced Measurement Report UL (GSM 04.08 9.1.45b) ────────────────
// Short message: no standard L3 header, sent on FACCH.

class L3EnhancedMeasurementRepUL {
    std::vector<uint8_t> mData;

    friend struct Builder;
public:
    static constexpr int MTI = 0x10B;

    std::vector<uint8_t>& data() { return mData; }
    const std::vector<uint8_t>& data() const { return mData; }

    size_t bodyLength() const { return mData.size(); }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
    [[nodiscard]] static Expected<L3EnhancedMeasurementRepUL> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;

    struct Builder {
        std::vector<uint8_t> mData;

        /// Set data payload.
        Builder& data(std::vector<uint8_t> v) { mData = std::move(v); return *this; }
        /// Build the final message.
        [[nodiscard]] L3EnhancedMeasurementRepUL build() const {
            L3EnhancedMeasurementRepUL msg;
            msg.mData = mData;
            return msg;
        }
    };

    static Builder builder() { return Builder{}; }
};

// ── Measurement Info DL (GSM 04.08 9.1.45c) ───────────────────────────
// Short message: no standard L3 header, sent on FACCH.

class L3MeasurementInfoDL {
    std::vector<uint8_t> mData;

    friend struct Builder;
public:
    static constexpr int MTI = 0x10C;

    std::vector<uint8_t>& data() { return mData; }
    const std::vector<uint8_t>& data() const { return mData; }

    size_t bodyLength() const { return mData.size(); }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
    [[nodiscard]] static Expected<L3MeasurementInfoDL> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;

    struct Builder {
        std::vector<uint8_t> mData;

        /// Set data payload.
        Builder& data(std::vector<uint8_t> v) { mData = std::move(v); return *this; }
        /// Build the final message.
        [[nodiscard]] L3MeasurementInfoDL build() const {
            L3MeasurementInfoDL msg;
            msg.mData = mData;
            return msg;
        }
    };

    static Builder builder() { return Builder{}; }
};

// ── VBS/VGCS Recon (GSM 04.08 9.1.45d) ────────────────────────────────
// Short message: no standard L3 header, sent on FACCH.

class L3VBSVGCSRecon {
public:
    static constexpr int MTI = 0x10D;

    size_t bodyLength() const { return 0; }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return 0; }
    [[nodiscard]] static Expected<L3VBSVGCSRecon> parse(BitReader&);
    void write(BitWriter&) const;
    void text(std::ostream& os) const;

    struct Builder {
        /// Build the final message.
        [[nodiscard]] L3VBSVGCSRecon build() const;
    };
    friend struct Builder;
    static Builder builder() { return Builder{}; }
};

// ── VBS/VGCS Recon 2 (GSM 04.08 9.1.45e) ──────────────────────────────
// Short message: no standard L3 header, sent on FACCH.

class L3VBSVGCSRecon2 {
public:
    static constexpr int MTI = 0x10E;

    size_t bodyLength() const { return 0; }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return 0; }
    [[nodiscard]] static Expected<L3VBSVGCSRecon2> parse(BitReader&);
    void write(BitWriter&) const;
    void text(std::ostream& os) const;

    struct Builder {
        /// Build the final message.
        [[nodiscard]] L3VBSVGCSRecon2 build() const;
    };
    friend struct Builder;
    static Builder builder() { return Builder{}; }
};

// ── VGCS Add Info (GSM 04.08 9.1.45f) ─────────────────────────────────
// Short message: no standard L3 header, sent on FACCH.

class L3VGCSAddInfo {
public:
    static constexpr int MTI = 0x10F;

    size_t bodyLength() const { return 0; }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return 0; }
    [[nodiscard]] static Expected<L3VGCSAddInfo> parse(BitReader&);
    void write(BitWriter&) const;
    void text(std::ostream& os) const;

    struct Builder {
        /// Build the final message.
        [[nodiscard]] L3VGCSAddInfo build() const;
    };
    friend struct Builder;
    static Builder builder() { return Builder{}; }
};

// ── VGCS SMS Info (GSM 04.08 9.1.45g) ─────────────────────────────────
// Short message: no standard L3 header, sent on FACCH.

class L3VGCSMSInfo {
public:
    static constexpr int MTI = 0x110;

    size_t bodyLength() const { return 0; }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return 0; }
    [[nodiscard]] static Expected<L3VGCSMSInfo> parse(BitReader&);
    void write(BitWriter&) const;
    void text(std::ostream& os) const;

    struct Builder {
        /// Build the final message.
        [[nodiscard]] L3VGCSMSInfo build() const;
    };
    friend struct Builder;
    static Builder builder() { return Builder{}; }
};

// ── VGCS Neighbor Cell Info (GSM 04.08 9.1.45h) ───────────────────────
// Short message: no standard L3 header, sent on FACCH.

class L3VGCSSNeighCellInfo {
public:
    static constexpr int MTI = 0x111;

    size_t bodyLength() const { return 0; }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return 0; }
    [[nodiscard]] static Expected<L3VGCSSNeighCellInfo> parse(BitReader&);
    void write(BitWriter&) const;
    void text(std::ostream& os) const;

    struct Builder {
        /// Build the final message.
        [[nodiscard]] L3VGCSSNeighCellInfo build() const;
    };
    friend struct Builder;
    static Builder builder() { return Builder{}; }
};

// ── Notify App Data (GSM 04.08 9.1.45i) ───────────────────────────────
// Short message: no standard L3 header, sent on FACCH.

class L3NotifyAppData {
public:
    static constexpr int MTI = 0x112;

    size_t bodyLength() const { return 0; }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return 0; }
    [[nodiscard]] static Expected<L3NotifyAppData> parse(BitReader&);
    void write(BitWriter&) const;
    void text(std::ostream& os) const;

    struct Builder {
        /// Build the final message.
        [[nodiscard]] L3NotifyAppData build() const;
    };
    friend struct Builder;
    static Builder builder() { return Builder{}; }
};

// System Information Type 2quater - GSM 04.08 §9.1.34a
// Direction: DL (BCCH)
// Carries: extended BCCH freq list, RACH ctrl params, CBCH description
// MTI=0x07 per GSM_RR_Types.ttcn SYSTEM_INFORMATION_TYPE_2quater('00000111'B)
class L3SystemInformationType2quater {
    std::vector<uint8_t> mBody;

    friend struct Builder;
public:
    static constexpr int MTI = 0x07;
    L3SystemInformationType2quater() = default;
    const std::vector<uint8_t>& body() const { return mBody; }
    size_t bodyLength() const { return mBody.size(); }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::RadioResource; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
    [[nodiscard]] static Expected<L3SystemInformationType2quater> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;

    struct Builder {
        std::vector<uint8_t> mBody;

        /// Set body data.
        Builder& body(std::vector<uint8_t> v) { mBody = std::move(v); return *this; }
        /// Build the final message.
        [[nodiscard]] L3SystemInformationType2quater build() const {
            L3SystemInformationType2quater msg;
            msg.mBody = mBody;
            return msg;
        }
    };

    static Builder builder() { return Builder{}; }
};

} // namespace gsml3parser
