#ifndef GSML3PARSER_CC_L3CCELEMENTS_H
#define GSML3PARSER_CC_L3CCELEMENTS_H

#include <cstdint>
#include <string>
#include <vector>

#include "../l3message.h"
#include "../types.h"
#include "../enums.h"

namespace gsml3parser {

// ── Bearer Capability (GSM 04.08 10.5.4.5) ──────────────────────────────

class L3BearerCapability : public L3ProtocolElement {
private:
    uint8_t mOctet3;
    std::vector<uint8_t> mOctet3a;
    Bool_z mPresent;
public:
    L3BearerCapability();
    size_t lengthV() const override;
    void writeV(L3Frame& dest, size_t& wp) const override;
    void parseV(const L3Frame& src, size_t& rp) override;
    void parseV(const L3Frame& src, size_t& rp, size_t expectedLength) override;
    void text(std::ostream& os) const override;
    bool isPresent() const { return mPresent; }
    uint8_t octet3() const { return mOctet3; }
    const std::vector<uint8_t>& octet3a() const { return mOctet3a; }
    bool getHalfRateSupport() const { return mOctet3 & 0x40; }
};

// ── Supported Codec List (GSM 04.08 10.5.4.32) ─────────────────────────

class L3SupportedCodecList : public L3ProtocolElement {
private:
    Bool_z mGsmPresent;
    Bool_z mUmtsPresent;
    std::vector<uint8_t> mGsmCodecs;
    std::vector<uint8_t> mUmtsCodecs;
public:
    L3SupportedCodecList();
    size_t lengthV() const override;
    void writeV(L3Frame& dest, size_t& wp) const override;
    void parseV(const L3Frame& src, size_t& rp) override;
    void parseV(const L3Frame& src, size_t& rp, size_t expectedLength) override;
    void text(std::ostream& os) const override;
    bool isGsmPresent() const { return mGsmPresent; }
    bool isUmtsPresent() const { return mUmtsPresent; }
    const std::vector<uint8_t>& gsmCodecs() const { return mGsmCodecs; }
    const std::vector<uint8_t>& umtsCodecs() const { return mUmtsCodecs; }
};

// ── BCD Digits utility ──────────────────────────────────────────────────

class L3BCDDigits {
private:
    static const size_t maxDigits = 20;
    char mDigits[maxDigits + 1];
public:
    L3BCDDigits();
    explicit L3BCDDigits(const char* wDigits);
    L3BCDDigits(const L3BCDDigits& other);
    void parse(const L3Frame& src, size_t& rp, size_t numOctets, bool international = false);
    void write(L3Frame& dest, size_t& wp) const;
    size_t lengthV() const;
    size_t size() const { return strlen(mDigits); }
    const char* digits() const { return mDigits; }
};

std::ostream& operator<<(std::ostream& os, const L3BCDDigits& digits);

// ── Called Party BCD Number (GSM 04.08 10.5.4.7) ───────────────────────

class L3CalledPartyBCDNumber : public L3ProtocolElement {
private:
    TypeOfNumber mType;
    NumberingPlan mPlan;
    L3BCDDigits mDigits;
public:
    L3CalledPartyBCDNumber();
    explicit L3CalledPartyBCDNumber(const char* wDigits);

    TypeOfNumber type() const { return mType; }
    NumberingPlan plan() const { return mPlan; }
    const char* digits() const { return mDigits.digits(); }

    size_t lengthV() const override;
    void writeV(L3Frame& dest, size_t& wp) const override;
    void parseV(const L3Frame& src, size_t& rp) override;
    void parseV(const L3Frame& src, size_t& rp, size_t expectedLength) override;
    void text(std::ostream& os) const override;
};

// ── Calling Party BCD Number (GSM 04.08 10.5.4.9) ──────────────────────

class L3CallingPartyBCDNumber : public L3ProtocolElement {
private:
    TypeOfNumber mType;
    NumberingPlan mPlan;
    L3BCDDigits mDigits;
    Bool_z mHaveOctet3a;
    int mPresentationIndicator;
    int mScreeningIndicator;
public:
    L3CallingPartyBCDNumber();
    explicit L3CallingPartyBCDNumber(const char* wDigits);

    TypeOfNumber type() const { return mType; }
    NumberingPlan plan() const { return mPlan; }
    const char* digits() const { return mDigits.digits(); }

    size_t lengthV() const override;
    void writeV(L3Frame& dest, size_t& wp) const override;
    void parseV(const L3Frame& src, size_t& rp) override;
    void parseV(const L3Frame& src, size_t& rp, size_t expectedLength) override;
    void text(std::ostream& os) const override;
};

// ── Cause Element (GSM 04.08 10.5.4.11) ────────────────────────────────

class L3CauseElement : public L3ProtocolElement {
public:
    using Location = CCCauseLocation;
    using Cause = CCCause;
private:
    Location mLocation;
    Cause mCause;
public:
    L3CauseElement(Cause wCause = Cause::Normal_Call_Clearing,
                   Location wLocation = Location::Private_Serving_Local);
    Location location() const { return mLocation; }
    Cause cause() const { return mCause; }
    size_t lengthV() const override { return 2; }
    void writeV(L3Frame& dest, size_t& wp) const override;
    void parseV(const L3Frame& src, size_t& rp) override;
    void parseV(const L3Frame& src, size_t& rp, size_t expectedLength) override;
    void text(std::ostream& os) const override;
};

// ── Call State (GSM 04.08 10.5.4.6) ────────────────────────────────────

class L3CallState : public L3ProtocolElement {
private:
    unsigned mCallState;
public:
    explicit L3CallState(unsigned wCallState = 0);
    unsigned callState() const { return mCallState; }
    size_t lengthV() const override { return 1; }
    void writeV(L3Frame& dest, size_t& wp) const override;
    void parseV(const L3Frame& src, size_t& rp) override;
    void parseV(const L3Frame& src, size_t& rp, size_t expectedLength) override;
    void text(std::ostream& os) const override;
};

// ── Progress Indicator (GSM 04.08 10.5.4.21) ───────────────────────────

class L3ProgressIndicator : public L3ProtocolElement {
public:
    enum Location : uint8_t {
        User = 0,
        PrivateServingLocal = 1,
        PublicServingLocal = 2,
        PublicServingRemote = 4,
        PrivateServingRemote = 5,
        BeyondInternetworking = 10
    };
    enum Progress : uint8_t {
        Unspecified = 0,
        NotISDN = 1,
        DestinationNotISDN = 2,
        OriginationNotISDN = 3,
        ReturnedToISDN = 4,
        InBandAvailable = 8,
        EndToEndISDN = 0x20,
        Queuing = 0x40
    };
private:
    Location mLocation;
    Progress mProgress;
public:
    L3ProgressIndicator(Progress wProgress = Unspecified,
                        Location wLocation = PrivateServingLocal);
    Location location() const { return mLocation; }
    Progress progress() const { return mProgress; }
    size_t lengthV() const override { return 2; }
    void writeV(L3Frame& dest, size_t& wp) const override;
    void parseV(const L3Frame& src, size_t& rp) override;
    void parseV(const L3Frame& src, size_t& rp, size_t expectedLength) override;
    void text(std::ostream& os) const override;
};

// ── Keypad Facility (GSM 04.08 10.5.4.17) ──────────────────────────────

class L3KeypadFacility : public L3ProtocolElement {
private:
    char mIA5;
public:
    explicit L3KeypadFacility(char wIA5 = 0);
    char IA5() const { return mIA5; }
    size_t lengthV() const override { return 1; }
    void writeV(L3Frame& dest, size_t& wp) const override;
    void parseV(const L3Frame& src, size_t& rp) override;
    void parseV(const L3Frame& src, size_t& rp, size_t expectedLength) override;
    void text(std::ostream& os) const override;
};

// ── Signal (GSM 04.08 10.5.4.23) ───────────────────────────────────────

class L3Signal : public L3ProtocolElement {
public:
    enum SignalValues : uint8_t {
        SignalDialToneOn = 0,
        SignalRingBackToneOn = 1,
        SignalInterceptToneOn = 2,
        SignalNetworkCongestionToneOn = 3,
        SignalBusyToneOn = 4,
        SignalConfirmToneOn = 5,
        SignalAnswerToneOn = 6,
        SignalCallWaitingToneOn = 7,
        SignalTonesOff = 0x3f,
        SignalAlertingOff = 0x4f
    };
private:
    SignalValues mSignalValue;
public:
    explicit L3Signal(SignalValues tone = SignalRingBackToneOn);
    size_t lengthV() const override { return 1; }
    void writeV(L3Frame& dest, size_t& wp) const override;
    void parseV(const L3Frame& src, size_t& rp) override;
    void parseV(const L3Frame& src, size_t& rp, size_t expectedLength) override;
    void text(std::ostream& os) const override;
};

// ── Repeat Indicator (GSM 04.08 10.5.4.4) ──────────────────────────────
// TV format: IEI=0x0d, Value=4 bits

class L3RepeatIndicator : public L3ProtocolElement {
private:
    unsigned mValue;
public:
    explicit L3RepeatIndicator(unsigned wValue = 0);
    unsigned value() const { return mValue; }
    size_t lengthV() const override { return 0; }
    void writeV(L3Frame& dest, size_t& wp) const override;
    void parseV(const L3Frame& src, size_t& rp) override;
    void parseV(const L3Frame& src, size_t& rp, size_t) override;
    void text(std::ostream& os) const override;
};

// ── CC Capabilities mixin ───────────────────────────────────────────────

class L3CCCapabilities {
public:
    L3BearerCapability mBearerCapability;
    L3SupportedCodecList mSupportedCodecs;
    std::string getCodecSet() const;
};

// ── Supplementary Service Facility IE (GSM 04.08 10.5.4.1) ─────────────

class L3SupServFacilityIE : public L3ProtocolElement {
private:
    std::string mData;
public:
    L3SupServFacilityIE();
    explicit L3SupServFacilityIE(const std::string& wData);
    const std::string& data() const { return mData; }
    size_t lengthV() const override;
    void writeV(L3Frame& dest, size_t& wp) const override;
    void parseV(const L3Frame& src, size_t& rp) override;
    void parseV(const L3Frame& src, size_t& rp, size_t expectedLength) override;
    void text(std::ostream& os) const override;
};

// ── Supplementary Service Version Indicator (24.008 10.5.4.24) ─────────

class L3SupServVersionIndicator : public L3ProtocolElement {
private:
    unsigned mVersion;
public:
    L3SupServVersionIndicator();
    unsigned version() const { return mVersion; }
    size_t lengthV() const override { return 1; }
    void writeV(L3Frame& dest, size_t& wp) const override;
    void parseV(const L3Frame& src, size_t& rp) override;
    void parseV(const L3Frame& src, size_t& rp, size_t) override;
    void text(std::ostream& os) const override;
};

// ── CC Common IEs mixin ────────────────────────────────────────────────

class L3CCCommonIEs {
public:
    bool mHaveFacility;
    L3SupServFacilityIE mFacility;
    bool mHaveSSVersion;
    L3SupServVersionIndicator mSSVersion;
    L3CCCommonIEs();
    void ccCommonText(std::ostream&) const;
    void ccCommonParse(const L3Frame& src, size_t& rp);
    void ccCommonWrite(L3Frame& dest, size_t& wp) const;
    size_t ccCommonLength() const;
};

} // namespace gsml3parser

#endif // GSML3PARSER_CC_L3CCELEMENTS_H
