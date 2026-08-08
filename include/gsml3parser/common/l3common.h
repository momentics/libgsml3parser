#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include "../types.h"
#include "../enums.h"
#include "../protocol_types.h"
#include "../expected.h"
#include "../bitreader.h"
#include "../bitwriter.h"

namespace gsml3parser {

// Forward declarations for TLV/TV parsing helpers.
namespace detail {
size_t skipLV(BitReader& br, size_t lengthBytes);
size_t skipTLV(BitReader& br, unsigned expectedIEI);
bool parseHasT(BitReader& br, unsigned expectedIEI);
} // namespace detail

// ── Cell Identity (GSM 04.08 10.5.1.1) ─────────────────────────────────

class L3CellIdentity {
    uint16_t mID{};
public:
    L3CellIdentity() = default;
    explicit L3CellIdentity(uint16_t id) : mID(id) {}

    uint16_t id() const { return mID; }
    static constexpr size_t lengthV() { return 2; }

    [[nodiscard]] static Expected<L3CellIdentity> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    bool operator==(const L3CellIdentity&) const = default;
};

// ── Location Area Identity (GSM 04.08 10.5.1.3) ────────────────────────

class L3LocationAreaIdentity {
    std::array<unsigned, 3> mMCC{};
    std::array<unsigned, 3> mMNC{};
    uint16_t mLAC{};
public:
    L3LocationAreaIdentity() = default;
    L3LocationAreaIdentity(const char* wMCC, const char* wMNC, unsigned wLAC);

    bool operator==(const L3LocationAreaIdentity&) const = default;
    int mcc() const;
    int mnc() const;
    int lac() const { return mLAC; }
    static constexpr size_t lengthV() { return 5; }

    [[nodiscard]] static Expected<L3LocationAreaIdentity> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── Mobile Identity (GSM 04.08 10.5.1.4) ───────────────────────────────

class L3MobileIdentity {
    MobileIDType mType{MobileIDType::NoID};
    std::array<char, 20> mDigits{};
    uint32_t mTMSI{};
public:
    L3MobileIdentity();
    explicit L3MobileIdentity(uint32_t wTMSI);
    explicit L3MobileIdentity(std::string_view wDigits);

    MobileIDType type() const { return mType; }
    const char* digits() const;
    uint32_t tmsi() const { return mTMSI; }
    bool isIMSI() const { return mType == MobileIDType::IMSI; }
    bool isTMSI() const { return mType == MobileIDType::TMSI; }

    bool operator==(const L3MobileIdentity&) const;
    bool operator!=(const L3MobileIdentity& other) const { return !operator==(other); }
    bool operator<(const L3MobileIdentity&) const;

    size_t lengthV() const;
    [[nodiscard]] static Expected<L3MobileIdentity> parse(BitReader& br, size_t lengthBytes);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── Mobile Station Classmark 1 (GSM 04.08 10.5.1.5) ───────────────────

class L3MobileStationClassmark1 {
    unsigned mRevisionLevel{};
    unsigned mES_IND{};
    unsigned mA5_1{};
    unsigned mRFPowerCapability{};
public:
    L3MobileStationClassmark1() = default;
    static constexpr size_t lengthV() { return 1; }

    [[nodiscard]] static Expected<L3MobileStationClassmark1> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    bool operator==(const L3MobileStationClassmark1&) const = default;

    unsigned revisionLevel() const { return mRevisionLevel; }
    unsigned esInd() const { return mES_IND; }
    unsigned a5_1() const { return mA5_1; }
    unsigned rfPowerCapability() const { return mRFPowerCapability; }
};

// ── Mobile Station Classmark 2 (GSM 04.08 10.5.1.6) ───────────────────

class L3MobileStationClassmark2 {
    unsigned mRevisionLevel{};
    unsigned mES_IND{};
    unsigned mA5_1{};
    unsigned mA5_3{};
    unsigned mA5_2{};
    unsigned mRFPowerCapability{};
    unsigned mPSCapability{};
    unsigned mSSScreenIndicator{};
    unsigned mSMCapability{};
    unsigned mVBS{};
    unsigned mVGCS{};
    unsigned mFC{};
    unsigned mCM3{};
    unsigned mLCSVACapability{};
    unsigned mSoLSA{};
    unsigned mCMSF{};
public:
    L3MobileStationClassmark2() = default;
    static constexpr size_t lengthV() { return 3; }

    [[nodiscard]] static Expected<L3MobileStationClassmark2> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    bool operator==(const L3MobileStationClassmark2&) const = default;

    int getA5Bits() const;
    int powerClass() const { return mRFPowerCapability + 1; }
    unsigned revisionLevel() const { return mRevisionLevel; }
    unsigned esInd() const { return mES_IND; }
    unsigned a5_1() const { return mA5_1; }
    unsigned a5_3() const { return mA5_3; }
    unsigned a5_2() const { return mA5_2; }
    unsigned rfPowerCapability() const { return mRFPowerCapability; }
};

// ── Mobile Station Classmark 3 (GSM 04.08 10.5.1.7) ───────────────────

class L3MobileStationClassmark3 {
    unsigned mMultiband{};
    unsigned mA5_4{};
    unsigned mA5_5{};
    unsigned mA5_6{};
    unsigned mA5_7{};
public:
    L3MobileStationClassmark3() = default;
    static constexpr size_t lengthV() { return 14; }

    [[nodiscard]] static Expected<L3MobileStationClassmark3> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    bool operator==(const L3MobileStationClassmark3&) const = default;

    unsigned multiband() const { return mMultiband; }
    unsigned a5_4() const { return mA5_4; }
    unsigned a5_5() const { return mA5_5; }
    unsigned a5_6() const { return mA5_6; }
    unsigned a5_7() const { return mA5_7; }
};

// ── Ciphering Key Sequence Number (GSM 04.08 10.5.1.2) ────────────────

class L3CipheringKeySequenceNumber {
    uint8_t mCIValue{};
public:
    L3CipheringKeySequenceNumber() = default;
    explicit L3CipheringKeySequenceNumber(unsigned ci) : mCIValue(static_cast<uint8_t>(ci & 0x0F)) {}

    static constexpr size_t lengthV() { return 0; } // bit-level, no full bytes
    uint8_t ciValue() const { return mCIValue; }

    [[nodiscard]] static Expected<L3CipheringKeySequenceNumber> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    bool operator==(const L3CipheringKeySequenceNumber&) const = default;
};

// ── Frequency List (GSM 04.08 10.5.2.13) ──────────────────────────────

class L3FrequencyList {
    std::vector<unsigned> mARFCNs;
public:
    L3FrequencyList() = default;
    explicit L3FrequencyList(const std::vector<unsigned>& arfcns) : mARFCNs(arfcns) {}

    void arfcns(const std::vector<unsigned>& arfcns) { mARFCNs = arfcns; }
    const std::vector<unsigned>& arfcns() const { return mARFCNs; }
    static constexpr size_t lengthV() { return 16; }

    [[nodiscard]] static Expected<L3FrequencyList> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    bool operator==(const L3FrequencyList&) const = default;

private:
    unsigned base() const;
    bool contains(unsigned arfcn) const;
};

// ── Cell Channel Description (GSM 04.08 10.5.2.1b) ────────────────────

class L3CellChannelDescription {
    uint16_t mARfcn{};
    uint8_t mBSIC{};
    unsigned mChannelSpacing{};
public:
    L3CellChannelDescription() = default;
    L3CellChannelDescription(unsigned arfcn, unsigned bsic, unsigned spacing)
        : mARfcn(arfcn), mBSIC(static_cast<uint8_t>(bsic)), mChannelSpacing(spacing) {}

    uint16_t arfcn() const { return mARfcn; }
    uint8_t bsic() const { return mBSIC; }
    unsigned channelSpacing() const { return mChannelSpacing; }
    static constexpr size_t lengthV() { return 3; }

    [[nodiscard]] static Expected<L3CellChannelDescription> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    bool operator==(const L3CellChannelDescription&) const = default;
};

// ── BCCH Frequency List (GSM 04.08 10.5.2.22) ────────────────────────

class L3BCCHFrequencyList {
    std::vector<unsigned> mARFCNs;
public:
    L3BCCHFrequencyList() = default;
    explicit L3BCCHFrequencyList(const std::vector<unsigned>& arfcns) : mARFCNs(arfcns) {}

    void arfcns(const std::vector<unsigned>& arfcns) { mARFCNs = arfcns; }
    const std::vector<unsigned>& arfcns() const { return mARFCNs; }
    static constexpr size_t lengthV() { return 16; }

    [[nodiscard]] static Expected<L3BCCHFrequencyList> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    bool operator==(const L3BCCHFrequencyList&) const = default;

private:
    unsigned base() const;
    bool contains(unsigned arfcn) const;
};

// ── Neighbor Cells Description (GSM 04.08 10.5.2.22) ──────────────────

class L3NeighborCellsDescription {
    std::vector<unsigned> mNeighbors;
public:
    L3NeighborCellsDescription() = default;
    explicit L3NeighborCellsDescription(const std::vector<unsigned>& neighbors) : mNeighbors(neighbors) {}

    void neighbors(const std::vector<unsigned>& n) { mNeighbors = n; }
    const std::vector<unsigned>& neighbors() const { return mNeighbors; }
    static constexpr size_t lengthV() { return 16; }

    [[nodiscard]] static Expected<L3NeighborCellsDescription> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    bool operator==(const L3NeighborCellsDescription&) const = default;

private:
    unsigned base() const;
    bool contains(unsigned arfcn) const;
};

// ── Control Channel Description (GSM 04.08 10.5.2.11) ─────────────────

class L3ControlChannelDescription {
public:
    unsigned mMSC_R99{};
    unsigned mATT{};
    unsigned mBS_AG_BLKS_RES{};
    unsigned mCCCH_CONF{};
    unsigned mSI22IND{};
    unsigned mCBQ3{};
    unsigned mBS_PA_MFRMS{};
    unsigned mT3212{};
public:
    L3ControlChannelDescription() = default;
    L3ControlChannelDescription(unsigned msc_r99, unsigned att, unsigned bs_ag_blks_res, unsigned ccch_conf,
                                  unsigned si22ind, unsigned cbq3, unsigned bs_pa_mfrms, unsigned t3212)
        : mMSC_R99(msc_r99), mATT(att), mBS_AG_BLKS_RES(bs_ag_blks_res), mCCCH_CONF(ccch_conf),
          mSI22IND(si22ind), mCBQ3(cbq3), mBS_PA_MFRMS(bs_pa_mfrms), mT3212(t3212) {}

    bool isCCCHCombined() const { return mCCCH_CONF == 1; }
    static constexpr size_t lengthV() { return 3; }

    [[nodiscard]] static Expected<L3ControlChannelDescription> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    bool operator==(const L3ControlChannelDescription&) const = default;
};

// ── Channel Description (GSM 04.08 10.5.2.5) ──────────────────────────

class L3ChannelDescription {
    uint8_t mTypeAndOffset{};
    uint8_t mTN{};
    uint8_t mTSC{};
    unsigned mHFlag{};
    uint16_t mARFCN{};
    uint8_t mMAIO{};
    uint8_t mHSN{};
    bool mInitialized{false};
public:
    L3ChannelDescription() = default;
    L3ChannelDescription(TypeAndOffset tao, unsigned tn, unsigned tsc, unsigned arfcn);

    bool initialized() const { return mInitialized; }
    uint8_t typeAndOffset() const { return mTypeAndOffset; }
    uint8_t tn() const { return mTN; }
    uint8_t tsc() const { return mTSC; }
    unsigned hFlag() const { return mHFlag; }
    uint16_t arfcn() const { return mARFCN; }
    uint8_t maio() const { return mMAIO; }
    uint8_t hsn() const { return mHSN; }
    static constexpr size_t lengthV() { return 3; }

    [[nodiscard]] static Expected<L3ChannelDescription> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    bool operator==(const L3ChannelDescription&) const = default;
};

// ── Channel Description 2 (GSM 44.018 10.5.2.5a) ──────────────────────

class L3ChannelDescription2 {
    uint8_t mTypeAndOffset{};
    uint8_t mTN{};
    uint8_t mTSC{};
    unsigned mHFlag{};
    uint16_t mARFCN{};
    uint8_t mMAIO{};
    uint8_t mHSN{};
public:
    L3ChannelDescription2() = default;
    L3ChannelDescription2(TypeAndOffset tao, unsigned tn, unsigned tsc, unsigned arfcn);
    explicit L3ChannelDescription2(const L3ChannelDescription& other);

    bool initialized() const { return mTypeAndOffset != TDMA_MISC; }
    uint8_t typeAndOffset() const { return mTypeAndOffset; }
    uint8_t tn() const { return mTN; }
    uint8_t tsc() const { return mTSC; }
    unsigned hFlag() const { return mHFlag; }
    uint16_t arfcn() const { return mARFCN; }
    uint8_t maio() const { return mMAIO; }
    uint8_t hsn() const { return mHSN; }
    static constexpr size_t lengthV() { return 3; }

    [[nodiscard]] static Expected<L3ChannelDescription2> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    bool operator==(const L3ChannelDescription2&) const = default;
};

// ── Additional Channel Description ─────────────────────────────────────

class L3AdditionalChannelDescription {
    uint8_t mTypeAndOffset{};
    uint8_t mTN{};
    uint8_t mTSC{};
    unsigned mHFlag{};
    uint16_t mARFCN{};
    uint8_t mMAIO{};
    uint8_t mHSN{};
    bool mInitialized{false};
public:
    L3AdditionalChannelDescription() = default;
    L3AdditionalChannelDescription(TypeAndOffset tao, unsigned tn, unsigned tsc, unsigned arfcn);

    bool initialized() const { return mInitialized; }
    uint8_t typeAndOffset() const { return mTypeAndOffset; }
    uint8_t tn() const { return mTN; }
    uint8_t tsc() const { return mTSC; }
    unsigned hFlag() const { return mHFlag; }
    uint16_t arfcn() const { return mARFCN; }
    uint8_t maio() const { return mMAIO; }
    uint8_t hsn() const { return mHSN; }
    static constexpr size_t lengthV() { return 3; }

    [[nodiscard]] static Expected<L3AdditionalChannelDescription> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    bool operator==(const L3AdditionalChannelDescription&) const = default;
};

// ── Power Command (GSM 04.08 10.5.2.28) ───────────────────────────────

class L3PowerCommand {
    uint8_t mCommand{};
public:
    L3PowerCommand() = default;
    explicit L3PowerCommand(unsigned cmd) : mCommand(static_cast<uint8_t>(cmd)) {}

    uint8_t command() const { return mCommand; }
    static constexpr size_t lengthV() { return 1; }

    [[nodiscard]] static Expected<L3PowerCommand> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    bool operator==(const L3PowerCommand&) const = default;
};

// ── Power Command And Access Type (GSM 04.08 10.5.2.28a) ──────────────

class L3PowerCommandAndAccessType {
    uint8_t mCommand{};
public:
    L3PowerCommandAndAccessType() = default;
    explicit L3PowerCommandAndAccessType(unsigned cmd) : mCommand(static_cast<uint8_t>(cmd)) {}

    uint8_t command() const { return mCommand; }
    static constexpr size_t lengthV() { return 1; }

    [[nodiscard]] static Expected<L3PowerCommandAndAccessType> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    bool operator==(const L3PowerCommandAndAccessType&) const = default;
};

// ── Channel Mode (GSM 04.08 10.5.2.6) ─────────────────────────────────

class L3ChannelMode {
public:
    enum Mode : uint8_t {
        SignallingOnly = 0,
        SpeechV1 = 1,
        SpeechV2 = 2,
        SpeechV3 = 3
    };
private:
    Mode mMode{SignallingOnly};
public:
    L3ChannelMode() = default;
    explicit L3ChannelMode(Mode mode) : mMode(mode) {}

    Mode mode() const { return mMode; }
    bool isAMR() const { return mMode == SpeechV3; }
    bool operator==(const L3ChannelMode& other) const { return mMode == other.mMode; }
    bool operator!=(const L3ChannelMode& other) const { return mMode != other.mMode; }

    // 4 bits, not full byte
    static constexpr size_t lengthV() { return 1; }

    [[nodiscard]] static Expected<L3ChannelMode> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── Timing Advance (GSM 04.08 10.5.2.40) ───────────────────────────────

class L3TimingAdvance {
    uint8_t mTimingAdvance{};
public:
    L3TimingAdvance() = default;
    explicit L3TimingAdvance(unsigned ta) : mTimingAdvance(static_cast<uint8_t>(ta)) {}

    uint8_t timingAdvance() const { return mTimingAdvance; }
    static constexpr size_t lengthV() { return 1; }

    [[nodiscard]] static Expected<L3TimingAdvance> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    bool operator==(const L3TimingAdvance&) const = default;
};

// ── Cell Description (GSM 04.08 10.5.2.2) ──────────────────────────────

class L3CellDescription {
    uint16_t mARFCN{};
    uint8_t mNCC{};
    uint8_t mBCC{};
public:
    L3CellDescription() = default;
    L3CellDescription(unsigned arfcn, unsigned ncc, unsigned bcc)
        : mARFCN(arfcn), mNCC(static_cast<uint8_t>(ncc)), mBCC(static_cast<uint8_t>(bcc)) {}

    uint16_t arfcn() const { return mARFCN; }
    uint8_t ncc() const { return mNCC; }
    uint8_t bcc() const { return mBCC; }
    static constexpr size_t lengthV() { return 2; }

    [[nodiscard]] static Expected<L3CellDescription> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    bool operator==(const L3CellDescription&) const = default;
};

// ── Handover Reference (GSM 04.08 10.5.2.15) ──────────────────────────

class L3HandoverReference {
    uint8_t mValue{};
public:
    L3HandoverReference() = default;
    explicit L3HandoverReference(unsigned val) : mValue(static_cast<uint8_t>(val)) {}

    uint8_t value() const { return mValue; }
    static constexpr size_t lengthV() { return 1; }

    [[nodiscard]] static Expected<L3HandoverReference> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    bool operator==(const L3HandoverReference&) const = default;
};

// ── Ciphering Mode Setting (GSM 04.08 10.5.2.9) ───────────────────────

class L3CipheringModeSetting {
    bool mCiphering{};
    int mAlgorithm{};
public:
    L3CipheringModeSetting() = default;
    L3CipheringModeSetting(bool ciphering, int algorithm) : mCiphering(ciphering), mAlgorithm(algorithm) {}

    bool ciphering() const { return mCiphering; }
    int algorithm() const { return mAlgorithm; }
    static constexpr size_t lengthV() { return 0; } // 4 bits

    [[nodiscard]] static Expected<L3CipheringModeSetting> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    bool operator==(const L3CipheringModeSetting&) const = default;
};

// ── Ciphering Mode Response (GSM 04.08 10.5.2.10) ─────────────────────

class L3CipheringModeResponse {
    bool mIncludeIMEISV{};
public:
    L3CipheringModeResponse() = default;
    explicit L3CipheringModeResponse(bool includeIMEISV) : mIncludeIMEISV(includeIMEISV) {}

    bool includeIMEISV() const { return mIncludeIMEISV; }
    static constexpr size_t lengthV() { return 0; } // 2 bits

    [[nodiscard]] static Expected<L3CipheringModeResponse> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    bool operator==(const L3CipheringModeResponse&) const = default;
};

// ── Synchronization Indication (GSM 04.08 10.5.2.39) ──────────────────

class L3SynchronizationIndication {
    bool mNCI{};
    bool mROT{};
    int mSI{};
public:
    L3SynchronizationIndication() = default;
    L3SynchronizationIndication(bool nci, bool rot, int si) : mNCI(nci), mROT(rot), mSI(si) {}

    bool nci() const { return mNCI; }
    bool rot() const { return mROT; }
    int syncIndicator() const { return mSI; }
    static constexpr size_t lengthV() { return 1; }

    [[nodiscard]] static Expected<L3SynchronizationIndication> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    bool operator==(const L3SynchronizationIndication&) const = default;
};

// ── NCC Permitted (GSM 04.08 10.5.2.27) ───────────────────────────────

class L3NCCPermitted {
    uint8_t mPermitted{0xFF};
public:
    L3NCCPermitted() = default;
    explicit L3NCCPermitted(unsigned permitted) : mPermitted(static_cast<uint8_t>(permitted)) {}

    uint8_t permitted() const { return mPermitted; }
    static constexpr size_t lengthV() { return 1; }

    [[nodiscard]] static Expected<L3NCCPermitted> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    bool operator==(const L3NCCPermitted&) const = default;
};

// ── Page Mode (GSM 04.08 10.5.2.26) ───────────────────────────────────

class L3PageMode {
    uint8_t mPageMode{};
public:
    L3PageMode() = default;
    explicit L3PageMode(unsigned mode) : mPageMode(static_cast<uint8_t>(mode)) {}

    uint8_t pageMode() const { return mPageMode; }
    static constexpr size_t lengthV() { return 0; } // 2 bits

    [[nodiscard]] static Expected<L3PageMode> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    bool operator==(const L3PageMode&) const = default;
};

// ── Request Reference (GSM 04.08 10.5.2.30) ───────────────────────────

class L3RequestReference {
    uint8_t mRA{};
    uint8_t mT1p{};
    uint8_t mT2{};
    uint8_t mT3{};
public:
    L3RequestReference() = default;
    L3RequestReference(unsigned ra, unsigned t1p, unsigned t2, unsigned t3)
        : mRA(static_cast<uint8_t>(ra)), mT1p(static_cast<uint8_t>(t1p)),
          mT2(static_cast<uint8_t>(t2)), mT3(static_cast<uint8_t>(t3)) {}

    uint8_t ra() const { return mRA; }
    uint8_t t1p() const { return mT1p; }
    uint8_t t2() const { return mT2; }
    uint8_t t3() const { return mT3; }
    static constexpr size_t lengthV() { return 3; }

    [[nodiscard]] static Expected<L3RequestReference> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    bool operator==(const L3RequestReference&) const = default;
};

// ── Wait Indication (GSM 04.08 10.5.2.43) ─────────────────────────────

class L3WaitIndication {
    uint8_t mValue{};
public:
    L3WaitIndication() = default;
    explicit L3WaitIndication(unsigned seconds) : mValue(static_cast<uint8_t>(seconds)) {}

    uint8_t value() const { return mValue; }
    static constexpr size_t lengthV() { return 1; }

    [[nodiscard]] static Expected<L3WaitIndication> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    bool operator==(const L3WaitIndication&) const = default;
};

// ── RRCause Element (GSM 04.08 10.5.2.31) ─────────────────────────────

class L3RRCauseElement {
    RRCause mCauseValue{RRCause::Normal_Event};
public:
    L3RRCauseElement() = default;
    explicit L3RRCauseElement(RRCause value) : mCauseValue(value) {}

    RRCause causeValue() const { return mCauseValue; }
    static constexpr size_t lengthV() { return 1; }

    [[nodiscard]] static Expected<L3RRCauseElement> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    bool operator==(const L3RRCauseElement&) const = default;
};

// ── Cell Options BCCH (GSM 04.08 10.5.2.3) ────────────────────────────

class L3CellOptionsBCCH {
    unsigned mPWRC{};
    unsigned mDTX{};
    unsigned mRADIO_LINK_TIMEOUT{};
public:
    L3CellOptionsBCCH() = default;

    unsigned pwrc() const { return mPWRC; }
    unsigned dtx() const { return mDTX; }
    unsigned radioLinkTimeout() const { return mRADIO_LINK_TIMEOUT; }
    static constexpr size_t lengthV() { return 1; }

    [[nodiscard]] static Expected<L3CellOptionsBCCH> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    bool operator==(const L3CellOptionsBCCH&) const = default;
};

// ── Cell Options SACCH (GSM 04.08 10.5.2.3a) ──────────────────────────

class L3CellOptionsSACCH {
    unsigned mPWRC{};
    unsigned mDTX{};
    unsigned mRADIO_LINK_TIMEOUT{};
public:
    L3CellOptionsSACCH() = default;

    unsigned pwrc() const { return mPWRC; }
    unsigned dtx() const { return mDTX; }
    unsigned radioLinkTimeout() const { return mRADIO_LINK_TIMEOUT; }
    static constexpr size_t lengthV() { return 1; }

    [[nodiscard]] static Expected<L3CellOptionsSACCH> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    bool operator==(const L3CellOptionsSACCH&) const = default;
};

// ── Cell Selection Parameters (GSM 04.08 10.5.2.4) ────────────────────

class L3CellSelectionParameters {
    unsigned mACS{};
    unsigned mNECI{};
    unsigned mCELL_RESELECT_HYSTERESIS{};
    unsigned mMS_TXPWR_MAX_CCH{};
    unsigned mRXLEV_ACCESS_MIN{};
public:
    L3CellSelectionParameters() = default;

    unsigned acs() const { return mACS; }
    unsigned neci() const { return mNECI; }
    unsigned cellReselectHysteresis() const { return mCELL_RESELECT_HYSTERESIS; }
    unsigned msTxpwrMaxCch() const { return mMS_TXPWR_MAX_CCH; }
    unsigned rxlevAccessMin() const { return mRXLEV_ACCESS_MIN; }
    static constexpr size_t lengthV() { return 2; }

    [[nodiscard]] static Expected<L3CellSelectionParameters> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    bool operator==(const L3CellSelectionParameters&) const = default;
};

// ── RACH Control Parameters (GSM 04.08 10.5.2.29) ─────────────────────

class L3RACHControlParameters {
    unsigned mMaxRetrans{};
    unsigned mTxInteger{};
    unsigned mCellBarAccess{};
    unsigned mRE{};
    uint16_t mAC{};
public:
    L3RACHControlParameters() = default;

    unsigned maxRetrans() const { return mMaxRetrans; }
    unsigned txInteger() const { return mTxInteger; }
    bool cellBarAccess() const { return mCellBarAccess; }
    unsigned re() const { return mRE; }
    uint16_t ac() const { return mAC; }
    static constexpr size_t lengthV() { return 3; }

    [[nodiscard]] static Expected<L3RACHControlParameters> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    bool operator==(const L3RACHControlParameters&) const = default;
};

// ── Immediate Assignment Information (GSM 04.08 10.5.2.9) ─────────────

class L3ImmediateAssignmentInformation {
    unsigned mPowerOffset{};
    unsigned mPowerOffsetLength{};
    std::vector<uint8_t> mPowerOffsetData;
public:
    L3ImmediateAssignmentInformation() = default;

    unsigned powerOffset() const { return mPowerOffset; }
    size_t lengthV() const;

    [[nodiscard]] static Expected<L3ImmediateAssignmentInformation> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── Measurement Results (GSM 04.08 10.5.2.20) ─────────────────────────

class L3MeasurementResults {
    bool mBA_USED{};
    bool mDTX_USED{};
    bool mMEAS_VALID{};
    unsigned mRXLEV_FULL_SERVING_CELL{};
    unsigned mRXLEV_SUB_SERVING_CELL{};
    unsigned mRXQUAL_FULL_SERVING_CELL{};
    unsigned mRXQUAL_SUB_SERVING_CELL{};
    unsigned mNO_NCELL{};
    std::array<unsigned, 6> mRXLEV_NCELL{};
    std::array<unsigned, 6> mBCCH_FREQ_NCELL{};
    std::array<unsigned, 6> mBSIC_NCELL{};
public:
    L3MeasurementResults() = default;

    bool baUsed() const { return mBA_USED; }
    bool dtxUsed() const { return mDTX_USED; }
    bool isServingCellValid() const { return mMEAS_VALID == 0; }
    unsigned rxlevFullServingCellRaw() const { return mRXLEV_FULL_SERVING_CELL; }
    unsigned rxlevSubServingCellRaw() const { return mRXLEV_SUB_SERVING_CELL; }
    unsigned rxqualFullServingCell() const { return mRXQUAL_FULL_SERVING_CELL; }
    unsigned rxqualSubServingCell() const { return mRXQUAL_SUB_SERVING_CELL; }
    unsigned noNcell() const { return mNO_NCELL; }
    unsigned rxlevNcellRaw(unsigned i) const { return mRXLEV_NCELL[i]; }
    unsigned bcchFreqNcell(unsigned i) const { return mBCCH_FREQ_NCELL[i]; }
    unsigned bsicNcell(unsigned i) const { return mBSIC_NCELL[i]; }
    int decodeLevToDBm(unsigned lev) const;
    float decodeQualToBER(unsigned qual) const;
    static constexpr size_t lengthV() { return 16; }

    [[nodiscard]] static Expected<L3MeasurementResults> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── Multi Rate Configuration (3GPP 44.018 10.5.2.21aa) ────────────────

class L3MultiRateConfiguration {
public:
    enum AmrCodecSet : uint8_t {
        codec_set_AMR_FR = 0x80,
        codec_set_AMR_HR = 0x10,
        codec_set_UMTS_AMR = 0x85
    };
private:
    unsigned mOptions{};
    AmrCodecSet mAmrCodecSet{codec_set_AMR_FR};
public:
    L3MultiRateConfiguration() = default;
    explicit L3MultiRateConfiguration(bool halfrate);

    unsigned options() const { return mOptions; }
    AmrCodecSet amrCodecSet() const { return mAmrCodecSet; }
    static constexpr size_t lengthV() { return 2; }

    [[nodiscard]] static Expected<L3MultiRateConfiguration> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    bool operator==(const L3MultiRateConfiguration&) const = default;
};

// ── APDUID (GSM 04.08 10.5.2.48) ──────────────────────────────────────

class L3APDUID {
    uint8_t mProtocolIdentifier{};
public:
    L3APDUID() = default;
    explicit L3APDUID(unsigned protocolIdentifier) : mProtocolIdentifier(static_cast<uint8_t>(protocolIdentifier)) {}

    uint8_t protocolIdentifier() const { return mProtocolIdentifier; }
    static constexpr size_t lengthV() { return 0; } // 4 bits

    [[nodiscard]] static Expected<L3APDUID> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    bool operator==(const L3APDUID&) const = default;
};

// ── APDU Flags (GSM 04.08 10.5.2.49) ──────────────────────────────────

class L3APDUFlags {
    unsigned mCR{};
    unsigned mFirstSegment{};
    unsigned mLastSegment{};
public:
    L3APDUFlags() = default;
    L3APDUFlags(unsigned cr, unsigned firstSegment, unsigned lastSegment)
        : mCR(cr), mFirstSegment(firstSegment), mLastSegment(lastSegment) {}

    unsigned cr() const { return mCR; }
    unsigned firstSegment() const { return mFirstSegment; }
    unsigned lastSegment() const { return mLastSegment; }
    static constexpr size_t lengthV() { return 0; } // 3 bits

    [[nodiscard]] static Expected<L3APDUFlags> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    bool operator==(const L3APDUFlags&) const = default;
};

// ── APDU Data (GSM 04.08 10.5.2.50) ───────────────────────────────────

class L3APDUData {
    std::vector<uint8_t> mData;
public:
    L3APDUData() = default;
    explicit L3APDUData(std::vector<uint8_t> data) : mData(std::move(data)) {}

    const std::vector<uint8_t>& data() const { return mData; }
    size_t lengthV() const;

    [[nodiscard]] static Expected<L3APDUData> parse(BitReader& br, size_t lengthBytes);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── Mobile Allocation (GSM 04.08 10.5.2.14) ────────────────────────────

class L3MobileAllocation {
    std::vector<uint8_t> mData;
public:
    L3MobileAllocation() = default;
    explicit L3MobileAllocation(std::vector<uint8_t> data) : mData(std::move(data)) {}

    const std::vector<uint8_t>& data() const { return mData; }
    size_t lengthV() const { return mData.size(); }

    [[nodiscard]] static Expected<L3MobileAllocation> parse(BitReader& br, size_t lengthBytes);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── Dedicated Mode Or TBF (GSM 04.08 10.5.2.25b) ──────────────────────

class L3DedicatedModeOrTBF {
    unsigned mDownlink{};
    unsigned mTMA{};
    unsigned mDMOrTBF{};
public:
    L3DedicatedModeOrTBF() = default;
    L3DedicatedModeOrTBF(bool forTBF, bool downlink);

    bool isDownlink() const { return mDownlink; }
    bool isTBF() const { return mDMOrTBF; }
    static constexpr size_t lengthV() { return 0; } // 4 bits

    [[nodiscard]] static Expected<L3DedicatedModeOrTBF> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    bool operator==(const L3DedicatedModeOrTBF&) const = default;
};

// ── Cell Options (GSM 04.08 10.5.2.6) ─────────────────────────────────

class L3CellOptions {
    unsigned mRevisionLevel{};
    unsigned mCBCH{};
    unsigned mEnhancedRACH{};
    unsigned mCellReselectionPriority{};
    std::vector<uint8_t> mRawData;
public:
    L3CellOptions() = default;

    unsigned revisionLevel() const { return mRevisionLevel; }
    bool cbch() const { return mCBCH; }
    bool enhancedRach() const { return mEnhancedRACH; }
    unsigned cellReselectionPriority() const { return mCellReselectionPriority; }
    size_t lengthV() const;

    [[nodiscard]] static Expected<L3CellOptions> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── Cell Selection ─────────────────────────────────────────────────────

class L3CellSelection {
    unsigned mRxLevAccessMin{};
    unsigned mRxLevelAccessMin{};
    unsigned mMaxRxLev{};
    unsigned mCellReselectionHysteresis{};
    unsigned mCellReselectionOffset{};
    unsigned mCellReservedIndicator{};
    unsigned mCellBarQualifierValue{};
    unsigned mCellBarQualifierLength{};
    std::vector<uint8_t> mCellBarQualifierData;
public:
    L3CellSelection() = default;

    unsigned rxLevAccessMin() const { return mRxLevAccessMin; }
    unsigned maxRxLev() const { return mMaxRxLev; }
    unsigned cellReselectionHysteresis() const { return mCellReselectionHysteresis; }
    unsigned cellReselectionOffset() const { return mCellReselectionOffset; }
    size_t lengthV() const;

    [[nodiscard]] static Expected<L3CellSelection> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── Rest Octets Base ───────────────────────────────────────────────────

class L3RestOctets {
public:
    L3RestOctets() = default;
    virtual ~L3RestOctets() = default;
    virtual size_t lengthV() const { return 0; }
    virtual void write(BitWriter& bw) const {}
    [[nodiscard]] virtual Expected<L3RestOctets> parse(BitReader& br);
    [[nodiscard]] virtual Expected<L3RestOctets> parse(BitReader& br, size_t lengthBytes);
};

// ── SI3 Rest Octets (GSM 04.08 10.5.2.34) ─────────────────────────────

class L3SystemInformationType3;

class L3SI3RestOctets : public L3RestOctets {
    bool mHaveSI3RestOctets{};
    bool mHaveSelectionParameters{};
    unsigned mCBQ{};
    unsigned mCELL_RESELECT_OFFSET{};
    unsigned mTEMPORARY_OFFSET{};
    unsigned mPENALTY_TIME{};
    unsigned mRA_COLOUR{};
    bool mHaveGPRS{};
public:
    friend class L3SystemInformationType3;
    L3SI3RestOctets() = default;

    bool hasSI3RestOctets() const { return mHaveSI3RestOctets; }
    bool hasGPRS() const { return mHaveGPRS; }
    size_t lengthV() const override;

    void write(BitWriter& bw) const override;
    [[nodiscard]] Expected<L3RestOctets> parse(BitReader& br) override;
    [[nodiscard]] Expected<L3RestOctets> parse(BitReader& br, size_t lengthBytes) override;
    void text(std::ostream& os) const;
};

// ── SI4 Rest Octets ────────────────────────────────────────────────────

class L3SystemInformationType4;

class L3SIType4RestOctets : public L3RestOctets {
    unsigned mRA_COLOUR{};
    bool mHaveGPRS{};
public:
    friend class L3SystemInformationType4;
    L3SIType4RestOctets() = default;

    size_t lengthV() const override;
    void write(BitWriter& bw) const override;
    [[nodiscard]] Expected<L3RestOctets> parse(BitReader& br) override;
    [[nodiscard]] Expected<L3RestOctets> parse(BitReader& br, size_t lengthBytes) override;
    void text(std::ostream& os) const;
};

// ── IA Rest Octets ─────────────────────────────────────────────────────

class L3IARestOctets {
    bool mHavePacketAssignment{};
public:
    L3IARestOctets() = default;

    size_t lengthBits() const;
    void writeBits(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── GPRS Cell Options ──────────────────────────────────────────────────

class L3GPRSCellOptions {
    unsigned mNMO{};
    unsigned mT3168{};
    unsigned mT3192{};
    unsigned mDRX_TIMER_MAX{};
    unsigned mACCESS_BURST_TYPE{};
    unsigned mCONTROL_ACK_TYPE{};
    unsigned mBS_VC_MAX{};
public:
    L3GPRSCellOptions() = default;

    size_t lengthBits() const;
    void writeBits(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

class L3GPRSSI13PowerControlParameters {
    unsigned mALPHA{};
public:
    L3GPRSSI13PowerControlParameters() = default;

    size_t lengthBits() const;
    void writeBits(BitWriter& bw) const;
    void text(std::ostream& os) const;
};

// ── SI13 Rest Octets (GSM 04.08 10.5.2.37b) ───────────────────────────

class L3SystemInformationType13;

class L3SI13RestOctets : public L3RestOctets {
    unsigned mRAC{};
    bool mSPGC_CCCH_SUP{};
    unsigned mPRIORITY_ACCESS_THR{};
    unsigned mNETWORK_CONTROL_ORDER{};
    L3GPRSCellOptions mCellOptions;
    L3GPRSSI13PowerControlParameters mPowerControlParameters;
public:
    friend class L3SystemInformationType13;
    L3SI13RestOctets() = default;

    size_t lengthV() const override;
    void write(BitWriter& bw) const override;
    [[nodiscard]] Expected<L3RestOctets> parse(BitReader& br) override;
    [[nodiscard]] Expected<L3RestOctets> parse(BitReader& br, size_t lengthBytes) override;
    void text(std::ostream& os) const;
};

// ── Follow On Proceed (GSM 04.08) ─────────────────────────────────────

class L3FollowOnProceed {
public:
    static constexpr size_t lengthV() { return 1; }

    [[nodiscard]] static Expected<L3FollowOnProceed> parse(BitReader& br);
    void write(BitWriter& bw) const;
};

// ── Octet-aligned raw element (for SS Facility) ────────────────────────

#ifndef GSML3PARSER_L3OCTETALIGNEDPROTOCOL_ELEMENT_DEFINED
#define GSML3PARSER_L3OCTETALIGNEDPROTOCOL_ELEMENT_DEFINED
class L3OctetAlignedProtocolElement {
public:
    std::string mData;
    bool mExtant{};
    const unsigned char* peData() const { return reinterpret_cast<const unsigned char*>(mData.data()); }
    size_t lengthV() const { return mData.size(); }

    L3OctetAlignedProtocolElement() = default;
    explicit L3OctetAlignedProtocolElement(std::string data) : mData(std::move(data)), mExtant(true) {}

    [[nodiscard]] static Expected<L3OctetAlignedProtocolElement> parse(BitReader& br, size_t lengthBytes);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
};
#endif // GSML3PARSER_L3OCTETALIGNEDPROTOCOL_ELEMENT_DEFINED

// ── Utility functions ──────────────────────────────────────────────────

inline constexpr unsigned countBeaconTimeslots(int ccch_conf) {
    switch (ccch_conf) {
        case 2: return 2;
        case 4: return 3;
        case 6: return 4;
        default: return 1;
    }
}

} // namespace gsml3parser
