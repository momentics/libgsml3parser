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
#include "l3mmelements.h"

namespace gsml3parser {

enum class LocationUpdateType : uint8_t {
    Normal = 0,
    Periodic = 1,
    IMSIAttach = 2
};

// ── IMSI Detach Indication (GSM 04.08 9.2.15) ─────────────────────────

class L3IMSIDetachIndication {
public:
    static constexpr int MTI = 0x01;

    struct Builder {
        L3MobileStationClassmark1 m_classmark;
        L3MobileIdentity m_mobileIdentity;

        /// Set the mobile station classmark.
        Builder& classmark(L3MobileStationClassmark1 v) { m_classmark = v; return *this; }
        /// Set the mobile identity.
        Builder& mobileIdentity(L3MobileIdentity v) { m_mobileIdentity = std::move(v); return *this; }
        /// Build the final message.
        [[nodiscard]] L3IMSIDetachIndication build() const;
    };

    static Builder builder();
private:
    friend struct Builder;
    L3MobileStationClassmark1 mClassmark;
    L3MobileIdentity mMobileIdentity;
public:
    const L3MobileIdentity& mobileId() const { return mMobileIdentity; }
    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3IMSIDetachIndication> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::MobilityManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Location Updating Accept (GSM 04.08 9.2.13) ───────────────────────

class L3LocationUpdatingAccept {
public:
    struct Builder {
        L3LocationAreaIdentity m_lai;
        bool m_followOn{false};
        bool m_haveMI{false};
        L3MobileIdentity m_mi;
        Builder& lai(const L3LocationAreaIdentity& v) { m_lai = v; return *this; }
        Builder& followOn(bool v) { m_followOn = v; return *this; }
        Builder& mobileIdentity(const L3MobileIdentity& v) { m_mi = v; m_haveMI = true; return *this; }
        [[nodiscard]] L3LocationUpdatingAccept build() const;
    };
    static Builder builder();
private:
    friend struct Builder;
    L3LocationAreaIdentity mLAI;
    bool mFollowOnProceed{false};
    bool mHaveMobileIdentity{false};
    L3MobileIdentity mMobileIdentity;
public:
    L3LocationUpdatingAccept() = default;
    static constexpr int MTI = 0x02;
    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3LocationUpdatingAccept> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::MobilityManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Location Updating Reject (GSM 04.08 9.2.14) ───────────────────────

class L3LocationUpdatingReject {
private:
    MMRejectCause mCause{MMRejectCause::Zero};

    friend struct Builder;
public:
    static constexpr int MTI = 0x04;
    explicit L3LocationUpdatingReject(MMRejectCause cause) : mCause(cause) {}

    struct Builder {
        MMRejectCause m_cause{MMRejectCause::Zero};

        /// Set the MM reject cause.
        Builder& cause(MMRejectCause v) { m_cause = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3LocationUpdatingReject build() const;
    };

    static Builder builder();
    size_t bodyLength() const { return 1; }
    [[nodiscard]] static Expected<L3LocationUpdatingReject> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::MobilityManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Location Updating Request (GSM 04.08 9.2.15) ──────────────────────

class L3LocationUpdatingRequest {
private:
    unsigned mUpdateType{0};
    unsigned mCKSN{0};
    L3MobileStationClassmark1 mClassmark;
    L3MobileIdentity mMobileIdentity;
    L3LocationAreaIdentity mLAI;

    friend struct Builder;
public:
    static constexpr int MTI = 0x08;

    struct Builder {
        unsigned m_updateType{0};
        unsigned m_cksn{0};
        L3MobileStationClassmark1 m_classmark;
        L3MobileIdentity m_mobileIdentity;
        L3LocationAreaIdentity m_lai;

        /// Set update type: 0=Normal, 1=Periodic, 2=IMSI Attach.
        Builder& updateType(unsigned v) { m_updateType = v; return *this; }
        /// Set CKSN value.
        Builder& cksn(unsigned v) { m_cksn = v; return *this; }
        /// Set classmark.
        Builder& classmark(L3MobileStationClassmark1 v) { m_classmark = v; return *this; }
        /// Set mobile identity.
        Builder& mobileIdentity(L3MobileIdentity v) { m_mobileIdentity = std::move(v); return *this; }
        /// Set location area identity.
        Builder& lai(L3LocationAreaIdentity v) { m_lai = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3LocationUpdatingRequest build() const;
    };

    static Builder builder();
    const L3MobileIdentity& mobileId() const { return mMobileIdentity; }
    const L3LocationAreaIdentity& lai() const { return mLAI; }
    size_t bodyLength() const;
    LocationUpdateType getLocationUpdatingType() const { return static_cast<LocationUpdateType>(mUpdateType & 0x3); }
    unsigned getFollowOnRequest() const { return mUpdateType & 0x8; }
    [[nodiscard]] static Expected<L3LocationUpdatingRequest> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::MobilityManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Authentication Reject (GSM 04.08 9.2.1) ───────────────────────────

class L3AuthenticationReject {
public:
    static constexpr int MTI = 0x11;

    struct Builder {
        /// Build the final message.
        [[nodiscard]] L3AuthenticationReject build() const;
    };

    static Builder builder();
private:
    friend struct Builder;
public:
    size_t bodyLength() const { return 0; }
    [[nodiscard]] static Expected<L3AuthenticationReject> parse(BitReader&);
    void write(BitWriter&) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::MobilityManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Authentication Request (GSM 04.08 9.2.2) ──────────────────────────

class L3AuthenticationRequest {
private:
    unsigned mCKSN{0};
    std::vector<uint8_t> mRAND;

    friend struct Builder;
public:
    static constexpr int MTI = 0x12;
    L3AuthenticationRequest() = default;
    L3AuthenticationRequest(unsigned ckSN, const std::vector<uint8_t>& rand) : mCKSN(ckSN), mRAND(rand) {}

    struct Builder {
        unsigned m_cksn{0};
        std::vector<uint8_t> m_rand;

        /// Set CKSN value.
        Builder& cksn(unsigned v) { m_cksn = v; return *this; }
        /// Set RAND vector.
        Builder& rand(std::vector<uint8_t> v) { m_rand = std::move(v); return *this; }
        /// Build the final message.
        [[nodiscard]] L3AuthenticationRequest build() const;
    };

    static Builder builder();
    size_t bodyLength() const { return 17; }
    [[nodiscard]] static Expected<L3AuthenticationRequest> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::MobilityManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Authentication Response (GSM 04.08 9.2.3) ─────────────────────────

class L3AuthenticationResponse {
private:
    uint32_t mSRES{0};

    friend struct Builder;
public:
    static constexpr int MTI = 0x14;
    L3AuthenticationResponse() = default;
    explicit L3AuthenticationResponse(uint32_t sres) : mSRES(sres) {}

    struct Builder {
        uint32_t m_sres{0};

        /// Set SRES value.
        Builder& sres(uint32_t v) { m_sres = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3AuthenticationResponse build() const;
    };

    static Builder builder();
    uint32_t sres() const { return mSRES; }
    size_t bodyLength() const { return 4; }
    [[nodiscard]] static Expected<L3AuthenticationResponse> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::MobilityManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── CM Service Accept (GSM 04.08 9.2.5) ───────────────────────────────

class L3CMServiceAccept {
public:
    static constexpr int MTI = 0x21;

    struct Builder {
        /// Build the final message.
        [[nodiscard]] L3CMServiceAccept build() const;
    };

    static Builder builder();
private:
    friend struct Builder;
public:
    size_t bodyLength() const { return 0; }
    [[nodiscard]] static Expected<L3CMServiceAccept> parse(BitReader&);
    void write(BitWriter&) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::MobilityManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── CM Service Reject (GSM 04.08 9.2.6) ───────────────────────────────

class L3CMServiceReject {
private:
    MMRejectCause mCause{MMRejectCause::Zero};

    friend struct Builder;
public:
    static constexpr int MTI = 0x22;
    explicit L3CMServiceReject(MMRejectCause cause) : mCause(cause) {}

    struct Builder {
        MMRejectCause m_cause{MMRejectCause::Zero};

        /// Set the MM reject cause.
        Builder& cause(MMRejectCause v) { m_cause = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3CMServiceReject build() const;
    };

    static Builder builder();
    size_t bodyLength() const { return 1; }
    [[nodiscard]] static Expected<L3CMServiceReject> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::MobilityManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── CM Service Abort (GSM 04.08 9.2.7) ────────────────────────────────

class L3CMServiceAbort {
public:
    static constexpr int MTI = 0x23;

    struct Builder {
        /// Build the final message.
        [[nodiscard]] L3CMServiceAbort build() const;
    };

    static Builder builder();
private:
    friend struct Builder;
public:
    size_t bodyLength() const { return 0; }
    [[nodiscard]] static Expected<L3CMServiceAbort> parse(BitReader&);
    void write(BitWriter&) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::MobilityManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── CM Service Request (GSM 04.08 9.2.9) ──────────────────────────────

class L3CMServiceRequest {
private:
    L3MobileStationClassmark2 mClassmark;
    L3MobileIdentity mMobileIdentity;
    L3CMServiceType mServiceType;

    friend struct Builder;
public:
    static constexpr int MTI = 0x24;

    struct Builder {
        L3MobileStationClassmark2 m_classmark;
        L3MobileIdentity m_mobileIdentity;
        L3CMServiceType m_serviceType;

        /// Set the classmark.
        Builder& classmark(L3MobileStationClassmark2 v) { m_classmark = v; return *this; }
        /// Set the mobile identity.
        Builder& mobileIdentity(L3MobileIdentity v) { m_mobileIdentity = std::move(v); return *this; }
        /// Set the service type.
        Builder& serviceType(L3CMServiceType v) { m_serviceType = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3CMServiceRequest build() const;
    };

    static Builder builder();
    const L3MobileIdentity& mobileId() const { return mMobileIdentity; }
    L3CMServiceType::TypeCode serviceType() const { return mServiceType.type(); }
    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3CMServiceRequest> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::MobilityManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── MM Status (GSM 04.08 9.2.15) ──────────────────────────────────────

class L3MMStatus {
private:
    MMRejectCause mCause{MMRejectCause::Zero};

    friend struct Builder;
public:
    static constexpr int MTI = 0x31;

    struct Builder {
        MMRejectCause m_cause{MMRejectCause::Zero};

        /// Set the MM reject cause.
        Builder& cause(MMRejectCause v) { m_cause = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3MMStatus build() const;
    };

    static Builder builder();
    MMRejectCause cause() const { return mCause; }
    size_t bodyLength() const { return 1; }
    [[nodiscard]] static Expected<L3MMStatus> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::MobilityManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── MM Information (GSM 04.08 9.2.15a) ────────────────────────────────

class L3MMInformation {
private:
    L3NetworkName mShortName;
    L3TimeZoneAndTime mTime;

    friend struct Builder;
public:
    static constexpr int MTI = 0x32;

    struct Builder {
        L3NetworkName m_shortName;
        L3TimeZoneAndTime m_time;

        /// Set the network short name.
        Builder& shortName(L3NetworkName v) { m_shortName = v; return *this; }
        /// Set the time zone and time.
        Builder& time(L3TimeZoneAndTime v) { m_time = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3MMInformation build() const;
    };

    static Builder builder();
    const L3NetworkName& shortName() const { return mShortName; }
    const L3TimeZoneAndTime& time() const { return mTime; }
    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3MMInformation> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::MobilityManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Identity Request (GSM 04.08 9.2.10) ───────────────────────────────

class L3IdentityRequest {
private:
    MobileIDType mType{MobileIDType::NoID};

    friend struct Builder;
public:
    static constexpr int MTI = 0x18;
    explicit L3IdentityRequest(MobileIDType type) : mType(type) {}

    struct Builder {
        MobileIDType m_type{MobileIDType::NoID};

        /// Set the identity type (IMSI, IMEI, etc.).
        Builder& type(MobileIDType v) { m_type = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3IdentityRequest build() const;
    };

    static Builder builder();
    size_t bodyLength() const { return 1; }
    [[nodiscard]] static Expected<L3IdentityRequest> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::MobilityManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Identity Response (GSM 04.08 9.2.11) ──────────────────────────────

class L3IdentityResponse {
private:
    L3MobileIdentity mMobileID;

    friend struct Builder;
public:
    static constexpr int MTI = 0x19;

    struct Builder {
        L3MobileIdentity m_mobileId;

        /// Set the mobile identity.
        Builder& mobileId(L3MobileIdentity v) { m_mobileId = std::move(v); return *this; }
        /// Build the final message.
        [[nodiscard]] L3IdentityResponse build() const;
    };

    static Builder builder();
    const L3MobileIdentity& mobileId() const { return mMobileID; }
    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3IdentityResponse> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::MobilityManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── TMSI Reallocation Command (GSM 04.08 9.2.17) ──────────────────────

class L3TMSIReallocationCommand {
public:
    struct Builder {
        L3LocationAreaIdentity m_lai;
        L3MobileIdentity m_tmsi;
        bool m_followOn{false};
        Builder& lai(const L3LocationAreaIdentity& v) { m_lai = v; return *this; }
        Builder& tmsi(const L3MobileIdentity& v) { m_tmsi = v; return *this; }
        Builder& followOn(bool v) { m_followOn = v; return *this; }
        [[nodiscard]] L3TMSIReallocationCommand build() const;
    };
    static Builder builder();
private:
    friend struct Builder;
    L3LocationAreaIdentity mLAI;
    L3MobileIdentity mTMSI;
    bool mFollowOnProceed{false};
public:
    L3TMSIReallocationCommand() = default;
    static constexpr int MTI = 0x1a;
    const L3LocationAreaIdentity& lai() const { return mLAI; }
    const L3MobileIdentity& tmsi() const { return mTMSI; }
    bool followOnProceed() const { return mFollowOnProceed; }
    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3TMSIReallocationCommand> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::MobilityManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── TMSI Reallocation Complete (GSM 04.08 9.2.18) ─────────────────────

class L3TMSIReallocationComplete {
public:
    static constexpr int MTI = 0x1b;

    struct Builder {
        /// Build the final message.
        [[nodiscard]] L3TMSIReallocationComplete build() const;
    };

    static Builder builder();
private:
    friend struct Builder;
public:
    size_t bodyLength() const { return 0; }
    [[nodiscard]] static Expected<L3TMSIReallocationComplete> parse(BitReader&);
    void write(BitWriter&) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::MobilityManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// CM-Request — 3GPP TS 24.008 §9.2.8
// Direction: Both
// Carries: CKSN, optional CM-Service-Type, Classmark Container(s), Mobile Identity
class L3CMRequest {
    unsigned mCKSN{0};
    bool mHaveServiceType{false};
    L3CMServiceType mServiceType;
    L3MobileStationClassmark2 mClassmark;
    L3MobileIdentity mMobileIdentity;

    friend struct Builder;
public:
    static constexpr int MTI = 0x20;
    L3CMRequest() = default;

    struct Builder {
        unsigned m_cksn{0};
        bool m_haveServiceType{false};
        L3CMServiceType m_serviceType;
        L3MobileStationClassmark2 m_classmark;
        L3MobileIdentity m_mobileIdentity;

        /// Set CKSN value.
        Builder& cksn(unsigned v) { m_cksn = v; return *this; }
        /// Set service type (sets mHaveServiceType flag).
        Builder& serviceType(L3CMServiceType v) { m_serviceType = v; m_haveServiceType = true; return *this; }
        /// Set the classmark.
        Builder& classmark(L3MobileStationClassmark2 v) { m_classmark = v; return *this; }
        /// Set the mobile identity.
        Builder& mobileIdentity(L3MobileIdentity v) { m_mobileIdentity = std::move(v); return *this; }
        /// Build the final message.
        [[nodiscard]] L3CMRequest build() const;
    };

    static Builder builder();
    unsigned cksn() const { return mCKSN; }
    bool haveServiceType() const { return mHaveServiceType; }
    L3CMServiceType::TypeCode serviceType() const { return mServiceType.type(); }
    const L3MobileIdentity& mobileId() const { return mMobileIdentity; }
    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3CMRequest> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::MobilityManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// MM-Paging — 3GPP TS 24.008 §9.2.12
// Direction: DL
// Carries: Mobile Identity (TMSI or IMSI of paged subscriber)
class L3PagingMM {
    L3MobileIdentity mMobileIdentity;

    friend struct Builder;
public:
    static constexpr int MTI = 0x06;
    L3PagingMM() = default;

    struct Builder {
        L3MobileIdentity m_mobileIdentity;

        /// Set the mobile identity.
        Builder& mobileIdentity(L3MobileIdentity v) { m_mobileIdentity = std::move(v); return *this; }
        /// Build the final message.
        [[nodiscard]] L3PagingMM build() const;
    };

    static Builder builder();
    const L3MobileIdentity& mobileId() const { return mMobileIdentity; }
    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3PagingMM> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::MobilityManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── CM Reestablishment Request (GSM 04.08 9.2.4) ──────────────────────

class L3CMReestablishmentRequest {
private:
    unsigned mCKSN{0};
    L3MobileStationClassmark2 mClassmark;
    L3MobileIdentity mMobileID;
    bool mHaveLAI{false};
    L3LocationAreaIdentity mLAI;

    friend struct Builder;
public:
    static constexpr int MTI = 0x28;

    struct Builder {
        unsigned m_cksn{0};
        L3MobileStationClassmark2 m_classmark;
        L3MobileIdentity m_mobileId;
        bool m_haveLAI{false};
        L3LocationAreaIdentity m_lai;

        /// Set CKSN value.
        Builder& cksn(unsigned v) { m_cksn = v; return *this; }
        /// Set the classmark.
        Builder& classmark(L3MobileStationClassmark2 v) { m_classmark = v; return *this; }
        /// Set the mobile identity.
        Builder& mobileId(L3MobileIdentity v) { m_mobileId = std::move(v); return *this; }
        /// Set location area identity (sets mHaveLAI flag).
        Builder& lai(L3LocationAreaIdentity v) { m_lai = v; m_haveLAI = true; return *this; }
        /// Build the final message.
        [[nodiscard]] L3CMReestablishmentRequest build() const;
    };

    static Builder builder();
    const L3MobileIdentity& mobileId() const { return mMobileID; }
    unsigned cksn() const { return mCKSN; }
    size_t bodyLength() const;
    [[nodiscard]] static Expected<L3CMReestablishmentRequest> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::MobilityManagement; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

} // namespace gsml3parser
