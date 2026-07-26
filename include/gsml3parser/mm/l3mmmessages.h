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

#ifndef GSML3PARSER_MM_L3MMMESSAGES_H
#define GSML3PARSER_MM_L3MMMESSAGES_H

#include <cstdint>
#include <memory>
#include <ostream>
#include <string>

#include "../l3message.h"
#include "../l3frame.h"
#include "../types.h"
#include "../common/l3common.h"

namespace gsml3parser {

// ── L3MMMessage ─────────────────────────────────────────────────────────

class L3MMMessage : public L3Message {
public:
    enum MessageType : int {
        IMSIDetachIndication      = 0x01,
        CMServiceAccept           = 0x21,
        CMServiceReject           = 0x22,
        CMServiceAbort            = 0x23,
        CMServiceRequest          = 0x24,
        CMReestablishmentRequest  = 0x28,
        IdentityResponse          = 0x19,
        IdentityRequest           = 0x18,
        MMInformation             = 0x32,
        LocationUpdatingAccept    = 0x02,
        LocationUpdatingReject    = 0x04,
        LocationUpdatingRequest   = 0x08,
        TMSIReallocationCommand   = 0x1a,
        TMSIReallocationComplete  = 0x1b,
        MMStatus                  = 0x31,
        AuthenticationRequest     = 0x12,
        AuthenticationResponse    = 0x14,
        AuthenticationReject      = 0x11,
        Undefined                 = -1
    };

    size_t fullBodyLength() const override { return l2BodyLength(); }
    L3PD PD() const override { return L3PD::MobilityManagement; }
    void text(std::ostream& os) const override;
};

std::ostream& operator<<(std::ostream& os, L3MMMessage::MessageType val);

// ── Location Update Type ───────────────────────────────────────────────

enum class LocationUpdateType : uint8_t {
    Normal = 0,
    Periodic = 1,
    IMSIAttach = 2
};

// ── Location Updating Request (GSM 04.08 9.2.15) ──────────────────────

class L3LocationUpdatingRequest : public L3MMMessage {
private:
    LocationUpdateType mUpdateType;
    unsigned mCKSN;
    L3MobileStationClassmark1 mClassmark;
    L3MobileIdentity mMobileIdentity;
    L3LocationAreaIdentity mLAI;
public:
    const L3MobileIdentity& mobileID() const { return mMobileIdentity; }
    const L3LocationAreaIdentity& LAI() const { return mLAI; }
    int MTI() const override { return LocationUpdatingRequest; }
    size_t l2BodyLength() const override;
    LocationUpdateType getLocationUpdatingType() const;
    void parseBody(const L3Frame& src, size_t& rp) override;
    void text(std::ostream& os) const override;
};

// ── Location Updating Accept (GSM 04.08 9.2.13) ───────────────────────

class L3LocationUpdatingAccept : public L3MMMessage {
private:
    L3LocationAreaIdentity mLAI;
    bool mFollowOnProceed;
    bool mHaveMobileIdentity;
    L3MobileIdentity mMobileIdentity;
public:
    L3LocationUpdatingAccept(const L3LocationAreaIdentity& wLAI, bool wFollowOn = false);
    L3LocationUpdatingAccept(const L3LocationAreaIdentity& wLAI,
                             const L3MobileIdentity& wID, bool wFollowOn = false);

    int MTI() const override { return LocationUpdatingAccept; }
    size_t l2BodyLength() const override;
    void writeBody(L3Frame& dest, size_t& wp) const override;
    void text(std::ostream& os) const override;
};

// ── Location Updating Reject (GSM 04.08 9.2.14) ───────────────────────

class L3LocationUpdatingReject : public L3MMMessage {
private:
    MMRejectCause mCause;
public:
    explicit L3LocationUpdatingReject(MMRejectCause cause) : mCause(cause) {}
    int MTI() const override { return LocationUpdatingReject; }
    size_t l2BodyLength() const override { return 1; }
    void writeBody(L3Frame& dest, size_t& wp) const override;
    void text(std::ostream& os) const override;
};

// ── IMSI Detach Indication (GSM 04.08 9.2.15) ─────────────────────────

class L3IMSIDetachIndication : public L3MMMessage {
private:
    L3MobileStationClassmark1 mClassmark;
    L3MobileIdentity mMobileIdentity;
public:
    const L3MobileIdentity& mobileID() const { return mMobileIdentity; }
    int MTI() const override { return IMSIDetachIndication; }
    size_t l2BodyLength() const override;
    void parseBody(const L3Frame& src, size_t& rp) override;
    void text(std::ostream& os) const override;
};

// ── CM Service Accept (GSM 04.08 9.2.5) ───────────────────────────────

class L3CMServiceAccept : public L3MMMessage {
public:
    int MTI() const override { return CMServiceAccept; }
    size_t l2BodyLength() const override { return 0; }
    void writeBody(L3Frame&, size_t&) const override {}
};

// ── CM Service Abort (GSM 04.08 9.2.7) ────────────────────────────────

class L3CMServiceAbort : public L3MMMessage {
public:
    int MTI() const override { return CMServiceAbort; }
    size_t l2BodyLength() const override { return 0; }
    void writeBody(L3Frame&, size_t&) const override {}
    void parseBody(const L3Frame& src, size_t& rp) override;
};

// ── CM Service Reject (GSM 04.08 9.2.6) ───────────────────────────────

class L3CMServiceReject : public L3MMMessage {
private:
    MMRejectCause mCause;
public:
    explicit L3CMServiceReject(MMRejectCause cause) : mCause(cause) {}
    int MTI() const override { return CMServiceReject; }
    size_t l2BodyLength() const override { return 1; }
    void writeBody(L3Frame&, size_t&) const override;
    void text(std::ostream& os) const override;
};

// ── CM Service Request (GSM 04.08 9.2.9) ──────────────────────────────

class L3CMServiceRequest : public L3MMMessage {
private:
    L3MobileStationClassmark2 mClassmark;
    L3MobileIdentity mMobileIdentity;
    unsigned mServiceType;
public:
    const L3MobileIdentity& mobileID() const { return mMobileIdentity; }
    unsigned serviceType() const { return mServiceType; }
    int MTI() const override { return CMServiceRequest; }
    size_t l2BodyLength() const override;
    void parseBody(const L3Frame& src, size_t& rp) override;
    void text(std::ostream& os) const override;
};

// ── CM Reestablishment Request (GSM 04.08 9.2.4) ──────────────────────

class L3CMReestablishmentRequest : public L3MMMessage {
private:
    L3MobileStationClassmark2 mClassmark;
    L3MobileIdentity mMobileID;
    bool mHaveLAI;
    L3LocationAreaIdentity mLAI;
public:
    const L3MobileIdentity& mobileID() const { return mMobileID; }
    int MTI() const override { return CMReestablishmentRequest; }
    size_t l2BodyLength() const override;
    void parseBody(const L3Frame& src, size_t& rp) override;
    void text(std::ostream& os) const override;
};

// ── MM Information (GSM 04.08 9.2.15a) ────────────────────────────────

class L3MMInformation : public L3MMMessage {
private:
    // Simplified
public:
    int MTI() const override { return MMInformation; }
    size_t l2BodyLength() const override;
    void writeBody(L3Frame&, size_t&) const override;
    void text(std::ostream& os) const override;
};

// ── Identity Request (GSM 04.08 9.2.10) ───────────────────────────────

class L3IdentityRequest : public L3MMMessage {
private:
    MobileIDType mType;
public:
    explicit L3IdentityRequest(MobileIDType type) : mType(type) {}
    int MTI() const override { return IdentityRequest; }
    size_t l2BodyLength() const override { return 1; }
    void writeBody(L3Frame& dest, size_t& wp) const override;
    void text(std::ostream& os) const override;
};

// ── Identity Response (GSM 04.08 9.2.11) ──────────────────────────────

class L3IdentityResponse : public L3MMMessage {
private:
    L3MobileIdentity mMobileID;
public:
    int MTI() const override { return IdentityResponse; }
    size_t l2BodyLength() const override;
    void parseBody(const L3Frame& src, size_t& rp) override;
    void text(std::ostream& os) const override;
    const L3MobileIdentity& mobileID() const { return mMobileID; }
};

// ── Authentication Request (GSM 04.08 9.2.2) ──────────────────────────

class L3AuthenticationRequest : public L3MMMessage {
private:
    unsigned mCKSN;
    std::vector<uint8_t> mRAND; // 128 bits
public:
    L3AuthenticationRequest(unsigned ckSN, const std::vector<uint8_t>& rand);
    int MTI() const override { return AuthenticationRequest; }
    size_t l2BodyLength() const override { return 17; }
    void writeBody(L3Frame&, size_t&) const override;
    void text(std::ostream& os) const override;
};

// ── Authentication Response (GSM 04.08 9.2.3) ─────────────────────────

class L3AuthenticationResponse : public L3MMMessage {
private:
    uint32_t mSRES;
public:
    uint32_t SRES() const { return mSRES; }
    int MTI() const override { return AuthenticationResponse; }
    size_t l2BodyLength() const override { return 4; }
    void parseBody(const L3Frame& src, size_t& rp) override;
    void text(std::ostream& os) const override;
};

// ── Authentication Reject (GSM 04.08 9.2.1) ───────────────────────────

class L3AuthenticationReject : public L3MMMessage {
public:
    int MTI() const override { return AuthenticationReject; }
    size_t l2BodyLength() const override { return 0; }
    void writeBody(L3Frame&, size_t&) const override {}
};

// ── TMSI Reallocation Complete (GSM 04.08 9.2.18) ─────────────────────

class L3TMSIReallocationComplete : public L3MMMessage {
public:
    int MTI() const override { return TMSIReallocationComplete; }
    size_t l2BodyLength() const override { return 0; }
    void parseBody(const L3Frame&, size_t&) override {}
    void text(std::ostream& os) const override;
};

// ── MM Status (GSM 04.08 9.2.15) ──────────────────────────────────────

class L3MMStatus : public L3MMMessage {
private:
    MMRejectCause mCause;
public:
    MMRejectCause cause() const { return mCause; }
    int MTI() const override { return MMStatus; }
    size_t l2BodyLength() const override { return 3; }
    void parseBody(const L3Frame& src, size_t& rp) override;
    void text(std::ostream& os) const override;
};

} // namespace gsml3parser

#endif // GSML3PARSER_MM_L3MMMESSAGES_H
