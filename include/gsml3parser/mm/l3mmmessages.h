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
private:
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
public:
    static constexpr int MTI = 0x04;
    explicit L3LocationUpdatingReject(MMRejectCause cause) : mCause(cause) {}
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
public:
    static constexpr int MTI = 0x08;
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
public:
    static constexpr int MTI = 0x12;
    L3AuthenticationRequest() = default;
    L3AuthenticationRequest(unsigned ckSN, const std::vector<uint8_t>& rand) : mCKSN(ckSN), mRAND(rand) {}
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
public:
    static constexpr int MTI = 0x14;
    L3AuthenticationResponse() = default;
    explicit L3AuthenticationResponse(uint32_t sres) : mSRES(sres) {}
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
public:
    static constexpr int MTI = 0x22;
    explicit L3CMServiceReject(MMRejectCause cause) : mCause(cause) {}
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
public:
    static constexpr int MTI = 0x24;
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
public:
    static constexpr int MTI = 0x31;
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
public:
    static constexpr int MTI = 0x32;
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
public:
    static constexpr int MTI = 0x18;
    explicit L3IdentityRequest(MobileIDType type) : mType(type) {}
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
public:
    static constexpr int MTI = 0x19;
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
    size_t bodyLength() const { return 0; }
    [[nodiscard]] static Expected<L3TMSIReallocationComplete> parse(BitReader&);
    void write(BitWriter&) const;
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
public:
    static constexpr int MTI = 0x28;
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
