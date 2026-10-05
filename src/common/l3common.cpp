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

#include "gsml3parser/common/l3common.h"
#include "gsml3parser/gsm_common.h"
#include <algorithm>
#include <cstring>
#include <sstream>
#include <iomanip>

namespace gsml3parser {

// ── L3CellIdentity ──────────────────────────────────────────────────────

Expected<L3CellIdentity> L3CellIdentity::parse(BitReader& br) {
    auto val = br.readField(16);
    if (!val) return Expected<L3CellIdentity>::error(val.error());
    return Expected<L3CellIdentity>::hold(L3CellIdentity(static_cast<uint16_t>(val.value())));
}

void L3CellIdentity::write(BitWriter& bw) const {
    bw.writeField(mID, 16);
}

void L3CellIdentity::text(std::ostream& os) const {
    os << "CI=" << mID;
}

// ── L3LocationAreaIdentity ─────────────────────────────────────────────

L3LocationAreaIdentity::L3LocationAreaIdentity(const char* wMCC, const char* wMNC, unsigned wLAC)
    : mLAC(static_cast<uint16_t>(wLAC))
{
    for (int i = 0; i < 3; ++i) {
        mMCC[i] = (wMCC[i] >= '0' && wMCC[i] <= '9') ? static_cast<unsigned>(wMCC[i] - '0') : 0;
    }
    mMNC[0] = (wMNC[0] >= '0' && wMNC[0] <= '9') ? static_cast<unsigned>(wMNC[0] - '0') : 0;
    mMNC[1] = (wMNC[1] >= '0' && wMNC[1] <= '9') ? static_cast<unsigned>(wMNC[1] - '0') : 0;
    if (wMNC[2] >= '0' && wMNC[2] <= '9')
        mMNC[2] = static_cast<unsigned>(wMNC[2] - '0');
    else
        mMNC[2] = 0x0F;
}

int L3LocationAreaIdentity::mcc() const {
    return mMCC[0] * 100 + mMCC[1] * 10 + mMCC[2];
}

int L3LocationAreaIdentity::mnc() const {
    int val = mMNC[0] * 10 + mMNC[1];
    if (mMNC[2] < 15) val = val * 10 + mMNC[2];
    return val;
}

Expected<L3LocationAreaIdentity> L3LocationAreaIdentity::parse(BitReader& br) {
    // PLMN BCD packing (TS 24.008 section 10.5.1.3):
    //   octet 1 = [MCC digit 2 | MCC digit 1]
    //   octet 2 = [MNC digit 3 (or F fill for a 2-digit MNC) | MCC digit 3]
    //   octet 3 = [MNC digit 2 | MNC digit 1]
    auto r = br.readField(4); if (!r) return Expected<L3LocationAreaIdentity>::error(r.error()); unsigned mcc2_ = r.value();
    r = br.readField(4); if (!r) return Expected<L3LocationAreaIdentity>::error(r.error()); unsigned mcc1_ = r.value();
    r = br.readField(4); if (!r) return Expected<L3LocationAreaIdentity>::error(r.error()); unsigned mnc3_ = r.value();
    r = br.readField(4); if (!r) return Expected<L3LocationAreaIdentity>::error(r.error()); unsigned mcc3_ = r.value();
    r = br.readField(4); if (!r) return Expected<L3LocationAreaIdentity>::error(r.error()); unsigned mnc2_ = r.value();
    r = br.readField(4); if (!r) return Expected<L3LocationAreaIdentity>::error(r.error()); unsigned mnc1_ = r.value();
    r = br.readField(16); if (!r) return Expected<L3LocationAreaIdentity>::error(r.error()); uint16_t lac = static_cast<uint16_t>(r.value());

    L3LocationAreaIdentity result;
    // Natural digit order: mMCC[0..2] = MCC digits 1..3, mMNC[0..2] = MNC
    // digits 1..3 (0xF fill for a 2-digit MNC).
    result.mMCC[0] = mcc1_; result.mMCC[1] = mcc2_; result.mMCC[2] = mcc3_;
    result.mMNC[0] = mnc1_; result.mMNC[1] = mnc2_; result.mMNC[2] = mnc3_;
    result.mLAC = lac;
    return Expected<L3LocationAreaIdentity>::hold(std::move(result));
}

void L3LocationAreaIdentity::write(BitWriter& bw) const {
    // PLMN BCD packing (TS 24.008 section 10.5.1.3): see parse for the layout.
    bw.writeField(mMCC[1], 4);   // octet 1 high: MCC digit 2
    bw.writeField(mMCC[0], 4);   // octet 1 low:  MCC digit 1
    bw.writeField(mMNC[2], 4);   // octet 2 high: MNC digit 3 (0xF for a 2-digit MNC)
    bw.writeField(mMCC[2], 4);   // octet 2 low:  MCC digit 3
    bw.writeField(mMNC[1], 4);   // octet 3 high: MNC digit 2
    bw.writeField(mMNC[0], 4);   // octet 3 low:  MNC digit 1
    bw.writeField(mLAC, 16);
}

void L3LocationAreaIdentity::text(std::ostream& os) const {
    os << "MCC=" << mMCC[0] << mMCC[1] << mMCC[2]
       << " MNC=" << mMNC[0] << mMNC[1] << mMNC[2]
       << " LAC=" << mLAC;
}

// ── L3MobileIdentity ────────────────────────────────────────────────────

L3MobileIdentity::L3MobileIdentity() : mTMSI(0) {
    mDigits.fill('\0');
}

L3MobileIdentity::L3MobileIdentity(uint32_t wTMSI)
    : mType(MobileIDType::TMSI), mTMSI(wTMSI) {
    mDigits.fill('\0');
}

L3MobileIdentity::L3MobileIdentity(std::string_view wDigits)
    : L3MobileIdentity(MobileIDType::IMSI, wDigits) {
}

L3MobileIdentity::L3MobileIdentity(MobileIDType wType, std::string_view wDigits)
    : mType(MobileIDType::NoID), mTMSI(0) {
    mDigits.fill('\0');
    if (wType != MobileIDType::IMSI && wType != MobileIDType::IMEI && wType != MobileIDType::IMEISV) {
        return;
    }
    mType = wType;
    // Only BCD digits are encodable on the wire (write() turns each stored
    // character into a nibble); any other character would corrupt the frame.
    size_t n = 0;
    for (char c : wDigits) {
        if (n >= mDigits.size() - 1) break;
        if (c >= '0' && c <= '9') mDigits[n++] = c;
    }
}

const char* L3MobileIdentity::digits() const {
    if (mType == MobileIDType::TMSI) return "";
    return mDigits.data();
}

bool L3MobileIdentity::operator==(const L3MobileIdentity& other) const {
    if (mType != other.mType) return false;
    if (mType == MobileIDType::TMSI) return mTMSI == other.mTMSI;
    return std::string_view(mDigits.data()) == std::string_view(other.mDigits.data());
}

bool L3MobileIdentity::operator<(const L3MobileIdentity& other) const {
    if (mType < other.mType) return true;
    if (mType > other.mType) return false;
    if (mType == MobileIDType::TMSI) return mTMSI < other.mTMSI;
    return std::string_view(mDigits.data()) < std::string_view(other.mDigits.data());
}

size_t L3MobileIdentity::lengthV() const {
    // Mobile identity length (TS 24.008 section 9.1.3.x): one octet per digit
    // pair plus the type octet; TMSI is always four octets; NoID one octet.
    if (mType == MobileIDType::TMSI) return 5;
    if (mType == MobileIDType::NoID) return 1;
    size_t nDigits = std::string_view(mDigits.data()).size();
    return 1 + nDigits / 2;
}

Expected<L3MobileIdentity> L3MobileIdentity::parse(BitReader& br, size_t lengthBytes) {
    // First octet (TS 24.008 section 9.1.3.x):
    //   [first digit(4) | odd count(1) | identity type(3)].
    auto r = br.readField(4); if (!r) return Expected<L3MobileIdentity>::error(r.error()); unsigned firstDigit = r.value();
    r = br.readField(1); if (!r) return Expected<L3MobileIdentity>::error(r.error()); bool oddCount = r.value() != 0;
    r = br.readField(3); if (!r) return Expected<L3MobileIdentity>::error(r.error());
    MobileIDType type = static_cast<MobileIDType>(r.value());

    L3MobileIdentity result;
    result.mDigits.fill('\0');

    switch (type) {
        case MobileIDType::TMSI: {
            // The spare 'F' nibble was consumed as the "first digit"; the
            // identity is a 32-bit value in big-endian order.
            r = br.readField(32); if (!r) return Expected<L3MobileIdentity>::error(r.error());
            result.mType = MobileIDType::TMSI;
            result.mTMSI = r.value();
            break;
        }
        case MobileIDType::IMSI:
        case MobileIDType::IMEI:
        case MobileIDType::IMEISV: {
            // Digit pairs follow as [next digit or F fill][current digit].
            result.mType = type;
            int numDigits = 0;
            // The first digit occupies the high nibble of the type octet.
            if (firstDigit < 10) {
                result.mDigits[numDigits++] = static_cast<char>(firstDigit + '0');
            }
            size_t remainingBytes = lengthBytes - 1; // minus the type octet
            for (size_t i = 0; i < remainingBytes; ++i) {
                r = br.readField(4); if (!r) return Expected<L3MobileIdentity>::error(r.error()); unsigned nextDigit = r.value();
                r = br.readField(4); if (!r) return Expected<L3MobileIdentity>::error(r.error()); unsigned currentDigit = r.value();
                if (numDigits + 2 < static_cast<int>(result.mDigits.size())) {
                    result.mDigits[numDigits++] = static_cast<char>(currentDigit + '0');
                    result.mDigits[numDigits++] = static_cast<char>(nextDigit + '0');
                }
            }
            // An even digit count ends with an 'F' fill in the high nibble of
            // the last byte; drop that trailing artifact.
            if (!oddCount && numDigits > 0) --numDigits;
            result.mDigits[numDigits] = '\0';
            break;
        }
        default:
            result.mType = MobileIDType::NoID;
            break;
    }
    return Expected<L3MobileIdentity>::hold(std::move(result));
}

void L3MobileIdentity::write(BitWriter& bw) const {
    // First octet (TS 24.008 section 9.1.3.x):
    //   [first digit(4) | odd count(1) | identity type(3)].
    size_t nDigits = 0;
    if (mType == MobileIDType::IMSI || mType == MobileIDType::IMEI || mType == MobileIDType::IMEISV) {
        nDigits = std::string_view(mDigits.data()).size();
    }
    if (mType == MobileIDType::TMSI) {
        // TMSI: spare 'F' nibble, a zero bit, type '100'B and four octets.
        bw.writeField(0x0F, 4);
        bw.writeField(0, 1);
        bw.writeField(static_cast<unsigned>(mType), 3);
        bw.writeField(mTMSI, 32);
        return;
    }
    if (nDigits == 0) {
        // NoID or an empty digit identity: spare 'F' nibble and a zero bit.
        MobileIDType type = (nDigits == 0 && mType != MobileIDType::NoID) ? mType : MobileIDType::NoID;
        bw.writeField(0x0F, 4);
        bw.writeField(0, 1);
        bw.writeField(static_cast<unsigned>(type), 3);
        return;
    }
    bw.writeField(mDigits[0] - '0', 4);          // first digit in the high nibble
    bw.writeField(nDigits & 0x01u, 1);           // odd-count indicator
    bw.writeField(static_cast<unsigned>(mType), 3);
    for (size_t i = 1; i < nDigits; i += 2) {    // [next digit or F fill][current digit]
        if (i + 1 < nDigits)
            bw.writeField(mDigits[i + 1] - '0', 4);
        else
            bw.writeField(0x0F, 4);
        bw.writeField(mDigits[i] - '0', 4);
    }
}

void L3MobileIdentity::text(std::ostream& os) const {
    switch (mType) {
        case MobileIDType::TMSI: os << "TMSI=" << std::hex << mTMSI; break;
        case MobileIDType::IMSI: os << "IMSI=" << mDigits.data(); break;
        case MobileIDType::IMEI: os << "IMEI=" << mDigits.data(); break;
        case MobileIDType::IMEISV: os << "IMEISV=" << mDigits.data(); break;
        default: os << "NoID"; break;
    }
}

// ── L3MobileStationClassmark1 ───────────────────────────────────────────

Expected<L3MobileStationClassmark1> L3MobileStationClassmark1::parse(BitReader& br) {
    L3MobileStationClassmark1 result;
    auto r = br.readField(1); if (!r) return Expected<L3MobileStationClassmark1>::error(r.error()); // spare
    r = br.readField(2); if (!r) return Expected<L3MobileStationClassmark1>::error(r.error()); result.mRevisionLevel = r.value();
    r = br.readField(1); if (!r) return Expected<L3MobileStationClassmark1>::error(r.error()); result.mES_IND = r.value();
    r = br.readField(1); if (!r) return Expected<L3MobileStationClassmark1>::error(r.error()); result.mA5_1 = r.value();
    r = br.readField(3); if (!r) return Expected<L3MobileStationClassmark1>::error(r.error()); result.mRFPowerCapability = r.value();
    return Expected<L3MobileStationClassmark1>::hold(std::move(result));
}

void L3MobileStationClassmark1::write(BitWriter& bw) const {
    bw.writeField(0, 1);
    bw.writeField(mRevisionLevel, 2);
    bw.writeField(mES_IND, 1);
    bw.writeField(mA5_1, 1);
    bw.writeField(mRFPowerCapability, 3);
}

void L3MobileStationClassmark1::text(std::ostream& os) const {
    os << "CM1[rev=" << mRevisionLevel << " ES=" << mES_IND
       << " A5_1=" << mA5_1 << " PWR=" << mRFPowerCapability;
}

// ── L3MobileStationClassmark2 ───────────────────────────────────────────

L3MobileStationClassmark2::L3MobileStationClassmark2(std::span<const uint8_t> extendedOctets) {
    mExtended.assign(extendedOctets.begin(), extendedOctets.end());
}

// Classmark 2 is an LV information element whose value is three to seven
// octets (TS 24.008 section 10.5.1.6): the base three octets carry the
// typed capability bits, any further value octets are extension data
// preserved verbatim for lossless round-trip.
Expected<L3MobileStationClassmark2> L3MobileStationClassmark2::parse(BitReader& br, size_t length) {
    if (length < 3u || length > 7u)
        return Expected<L3MobileStationClassmark2>::error(ParseError{ParseError::Code::InvalidValue, "classmark 2 length out of range"});
    L3MobileStationClassmark2 result;
    auto r = br.readField(1); if (!r) return Expected<L3MobileStationClassmark2>::error(r.error()); // spare
    r = br.readField(2); if (!r) return Expected<L3MobileStationClassmark2>::error(r.error()); result.mRevisionLevel = r.value();
    r = br.readField(1); if (!r) return Expected<L3MobileStationClassmark2>::error(r.error()); result.mES_IND = r.value();
    r = br.readField(1); if (!r) return Expected<L3MobileStationClassmark2>::error(r.error()); result.mA5_1 = r.value();
    r = br.readField(3); if (!r) return Expected<L3MobileStationClassmark2>::error(r.error()); result.mRFPowerCapability = r.value();
    r = br.readField(1); if (!r) return Expected<L3MobileStationClassmark2>::error(r.error()); // spare
    r = br.readField(1); if (!r) return Expected<L3MobileStationClassmark2>::error(r.error()); result.mPSCapability = r.value();
    r = br.readField(2); if (!r) return Expected<L3MobileStationClassmark2>::error(r.error()); result.mSSScreenIndicator = r.value();
    r = br.readField(1); if (!r) return Expected<L3MobileStationClassmark2>::error(r.error()); result.mSMCapability = r.value();
    r = br.readField(1); if (!r) return Expected<L3MobileStationClassmark2>::error(r.error()); result.mVBS = r.value();
    r = br.readField(1); if (!r) return Expected<L3MobileStationClassmark2>::error(r.error()); result.mVGCS = r.value();
    r = br.readField(1); if (!r) return Expected<L3MobileStationClassmark2>::error(r.error()); result.mFC = r.value();
    r = br.readField(1); if (!r) return Expected<L3MobileStationClassmark2>::error(r.error()); result.mCM3 = r.value();
    r = br.readField(1); if (!r) return Expected<L3MobileStationClassmark2>::error(r.error()); // spare
    r = br.readField(1); if (!r) return Expected<L3MobileStationClassmark2>::error(r.error()); result.mLCSVACapability = r.value();
    r = br.readField(1); if (!r) return Expected<L3MobileStationClassmark2>::error(r.error()); // spare
    r = br.readField(1); if (!r) return Expected<L3MobileStationClassmark2>::error(r.error()); result.mSoLSA = r.value();
    r = br.readField(1); if (!r) return Expected<L3MobileStationClassmark2>::error(r.error()); result.mCMSF = r.value();
    r = br.readField(1); if (!r) return Expected<L3MobileStationClassmark2>::error(r.error()); result.mA5_3 = r.value();
    r = br.readField(1); if (!r) return Expected<L3MobileStationClassmark2>::error(r.error()); result.mA5_2 = r.value();
    result.mExtended.reserve(length - 3);
    for (size_t i = 3; i < length; ++i) {
        auto e = br.readField(8);
        if (!e) return Expected<L3MobileStationClassmark2>::error(e.error());
        result.mExtended.push_back(static_cast<uint8_t>(e.value()));
    }
    return Expected<L3MobileStationClassmark2>::hold(std::move(result));
}

void L3MobileStationClassmark2::write(BitWriter& bw) const {
    bw.writeField(0, 1);
    bw.writeField(mRevisionLevel, 2);
    bw.writeField(mES_IND, 1);
    bw.writeField(mA5_1, 1);
    bw.writeField(mRFPowerCapability, 3);
    bw.writeField(0, 1);
    bw.writeField(mPSCapability, 1);
    bw.writeField(mSSScreenIndicator, 2);
    bw.writeField(mSMCapability, 1);
    bw.writeField(mVBS, 1);
    bw.writeField(mVGCS, 1);
    bw.writeField(mFC, 1);
    bw.writeField(mCM3, 1);
    bw.writeField(0, 1);
    bw.writeField(mLCSVACapability, 1);
    bw.writeField(0, 1);
    bw.writeField(mSoLSA, 1);
    bw.writeField(mCMSF, 1);
    bw.writeField(mA5_3, 1);
    bw.writeField(mA5_2, 1);
    // Extension octets beyond the base three, re-emitted verbatim.
    for (uint8_t b : mExtended) bw.writeField(b, 8);
}

void L3MobileStationClassmark2::text(std::ostream& os) const {
    os << "CM2[rev=" << mRevisionLevel << " ES=" << mES_IND
       << " A5_1=" << mA5_1 << " A5_3=" << mA5_3 << " A5_2=" << mA5_2
       << " PWR=" << mRFPowerCapability;
}

int L3MobileStationClassmark2::getA5Bits() const {
    int result = 0;
    if (mA5_1) result |= 1;
    if (mA5_2) result |= 2;
    if (mA5_3) result |= 4;
    return result;
}

// ── L3MobileStationClassmark3 ───────────────────────────────────────────

Expected<L3MobileStationClassmark3> L3MobileStationClassmark3::parse(BitReader& br) {
    L3MobileStationClassmark3 result;
    auto r = br.readField(1); if (!r) return Expected<L3MobileStationClassmark3>::error(r.error()); // spare
    r = br.readField(3); if (!r) return Expected<L3MobileStationClassmark3>::error(r.error()); result.mMultiband = r.value();
    r = br.readField(1); if (!r) return Expected<L3MobileStationClassmark3>::error(r.error()); result.mA5_7 = r.value();
    r = br.readField(1); if (!r) return Expected<L3MobileStationClassmark3>::error(r.error()); result.mA5_6 = r.value();
    r = br.readField(1); if (!r) return Expected<L3MobileStationClassmark3>::error(r.error()); result.mA5_5 = r.value();
    r = br.readField(1); if (!r) return Expected<L3MobileStationClassmark3>::error(r.error()); result.mA5_4 = r.value();
    return Expected<L3MobileStationClassmark3>::hold(std::move(result));
}

void L3MobileStationClassmark3::write(BitWriter& bw) const {
    bw.writeField(0, 1);
    bw.writeField(mMultiband, 3);
    bw.writeField(mA5_7, 1);
    bw.writeField(mA5_6, 1);
    bw.writeField(mA5_5, 1);
    bw.writeField(mA5_4, 1);
    for (int i = 0; i < 106; ++i) bw.writeField(0, 1);
}

void L3MobileStationClassmark3::text(std::ostream& os) const {
    os << "CM3[MB=" << mMultiband << " A5_4=" << mA5_4
       << " A5_5=" << mA5_5 << " A5_6=" << mA5_6 << " A5_7=" << mA5_7;
}

// ── L3CipheringKeySequenceNumber ────────────────────────────────────────

Expected<L3CipheringKeySequenceNumber> L3CipheringKeySequenceNumber::parse(BitReader& br) {
    auto r = br.readField(4);
    if (!r) return Expected<L3CipheringKeySequenceNumber>::error(r.error());
    return Expected<L3CipheringKeySequenceNumber>::hold(L3CipheringKeySequenceNumber(r.value()));
}

void L3CipheringKeySequenceNumber::write(BitWriter& bw) const {
    bw.writeField(mCIValue & 0x0F, 4);
}

void L3CipheringKeySequenceNumber::text(std::ostream& os) const {
    os << "CKSN=" << mCIValue;
}

// ── Frequency list helpers ──────────────────────────────────────────────

namespace {
std::vector<uint8_t> frequencyListToRaw(const std::vector<unsigned>& arfcns) {
    std::vector<uint8_t> raw(16, 0);
    if (arfcns.empty()) return raw;
    unsigned base = arfcns[0];
    for (unsigned a : arfcns) if (a < base) base = a;
    unsigned spread = 0;
    for (unsigned a : arfcns) { unsigned d = a - base; if (d > spread) spread = d; }
    // Write header: ext(1)=0, spare(2), base(10)
    raw[0] = static_cast<uint8_t>((base >> 8) & 0x7F);
    raw[1] = static_cast<uint8_t>(base & 0xFF);
    // Bitmap in remaining 14 bytes (112 bits), starting from base+1
    for (unsigned a : arfcns) {
        if (a == base) continue;
        unsigned offset = a - base - 1;
        if (offset < 112) {
            size_t byteIdx = 2 + offset / 8;
            unsigned bitIdx = 7u - (offset % 8);
            raw[byteIdx] |= static_cast<uint8_t>(1u << bitIdx);
        }
    }
    return raw;
}

std::vector<unsigned> frequencyListFromRaw(const std::vector<uint8_t>& raw) {
    std::vector<unsigned> arfcns;
    if (raw.size() < 2) return arfcns;
    unsigned base = (static_cast<unsigned>(raw[0] & 0x7F) << 8) | raw[1];
    if (base == 0 && raw[0] == 0 && raw[1] == 0) return arfcns;
    arfcns.push_back(base);
    unsigned numBits = static_cast<unsigned>((raw.size() - 2) * 8);
    for (unsigned i = 0; i < numBits; ++i) {
        size_t byteIdx = 2 + i / 8;
        if (byteIdx >= raw.size()) break;
        unsigned bitIdx = 7u - (i % 8);
        if (raw[byteIdx] & (1u << bitIdx)) {
            arfcns.push_back(base + 1 + i);
        }
    }
    std::sort(arfcns.begin(), arfcns.end());
    return arfcns;
}
} // anonymous namespace

// ── L3FrequencyList ────────────────────────────────────────────────────

unsigned L3FrequencyList::base() const {
    if (mARFCNs.empty()) return 0;
    unsigned retVal = mARFCNs[0];
    for (unsigned a : mARFCNs) if (a < retVal) retVal = a;
    return retVal;
}

bool L3FrequencyList::contains(unsigned arfcn) const {
    return std::find(mARFCNs.begin(), mARFCNs.end(), arfcn) != mARFCNs.end();
}

Expected<L3FrequencyList> L3FrequencyList::parse(BitReader& br) {
    std::vector<uint8_t> raw(16);
    auto r = br.readBytes(raw.data(), 16);
    if (!r) return Expected<L3FrequencyList>::error(r.error());
    return Expected<L3FrequencyList>::hold(L3FrequencyList(frequencyListFromRaw(raw)));
}

void L3FrequencyList::write(BitWriter& bw) const {
    std::vector<uint8_t> raw = frequencyListToRaw(mARFCNs);
    bw.writeBytes(raw.data(), raw.size());
}

void L3FrequencyList::text(std::ostream& os) const {
    os << "FreqList[";
    for (size_t i = 0; i < mARFCNs.size(); ++i) {
        if (i) os << ",";
        os << mARFCNs[i];
    }
    os << "]";
}

// ── L3BCCHFrequencyList ────────────────────────────────────────────────

unsigned L3BCCHFrequencyList::base() const {
    if (mARFCNs.empty()) return 0;
    unsigned retVal = mARFCNs[0];
    for (unsigned a : mARFCNs) if (a < retVal) retVal = a;
    return retVal;
}

bool L3BCCHFrequencyList::contains(unsigned arfcn) const {
    return std::find(mARFCNs.begin(), mARFCNs.end(), arfcn) != mARFCNs.end();
}

Expected<L3BCCHFrequencyList> L3BCCHFrequencyList::parse(BitReader& br) {
    // GSM 04.08 10.5.2.22: 16-octet BCCH_Freq_List (base + 112-bit bitmap),
    // same raw layout as L3FrequencyList.
    std::vector<uint8_t> raw(16);
    auto r = br.readBytes(raw.data(), 16); if (!r) return Expected<L3BCCHFrequencyList>::error(r.error());
    return Expected<L3BCCHFrequencyList>::hold(L3BCCHFrequencyList(frequencyListFromRaw(raw)));
}

void L3BCCHFrequencyList::write(BitWriter& bw) const {
    // GSM 04.08 10.5.2.22: 16-octet BCCH_Freq_List (base + 112-bit bitmap),
    // same raw layout as L3FrequencyList.
    std::vector<uint8_t> raw = frequencyListToRaw(mARFCNs);
    bw.writeBytes(raw.data(), raw.size());
}

void L3BCCHFrequencyList::text(std::ostream& os) const {
    os << "BCCHFreqList[";
    for (size_t i = 0; i < mARFCNs.size(); ++i) {
        if (i) os << ",";
        os << mARFCNs[i];
    }
    os << "]";
}

// ── L3CellChannelDescription ───────────────────────────────────────────

// Sixteen bits: [ARFCN(10)][BSIC(6) = NCC(3)|BCC(3)] (TS 44.018 section
// 9.2.3 cell channel description).
Expected<L3CellChannelDescription> L3CellChannelDescription::parse(BitReader& br) {
    L3CellChannelDescription result;
    auto r = br.readField(10); if (!r) return Expected<L3CellChannelDescription>::error(r.error()); result.mARfcn = static_cast<uint16_t>(r.value());
    r = br.readField(6); if (!r) return Expected<L3CellChannelDescription>::error(r.error()); result.mBSIC = static_cast<uint8_t>(r.value());
    return Expected<L3CellChannelDescription>::hold(std::move(result));
}

void L3CellChannelDescription::write(BitWriter& bw) const {
    bw.writeField(mARfcn, 10);
    bw.writeField(mBSIC, 6);
}

void L3CellChannelDescription::text(std::ostream& os) const {
    os << "CellChannel[ARfcn=" << mARfcn << " BSIC=" << static_cast<unsigned>(mBSIC) << "]";
}

// ── L3ControlChannelDescription ────────────────────────────────────────

Expected<L3ControlChannelDescription> L3ControlChannelDescription::parse(BitReader& br) {
    L3ControlChannelDescription result;
    auto r = br.readField(1); if (!r) return Expected<L3ControlChannelDescription>::error(r.error()); result.mMSC_R99 = r.value();
    r = br.readField(1); if (!r) return Expected<L3ControlChannelDescription>::error(r.error()); result.mATT = r.value();
    r = br.readField(3); if (!r) return Expected<L3ControlChannelDescription>::error(r.error()); result.mBS_AG_BLKS_RES = r.value();
    r = br.readField(3); if (!r) return Expected<L3ControlChannelDescription>::error(r.error()); result.mCCCH_CONF = r.value();
    r = br.readField(1); if (!r) return Expected<L3ControlChannelDescription>::error(r.error()); result.mSI22IND = r.value();
    r = br.readField(2); if (!r) return Expected<L3ControlChannelDescription>::error(r.error()); result.mCBQ3 = r.value();
    r = br.readField(2); if (!r) return Expected<L3ControlChannelDescription>::error(r.error()); // spare
    r = br.readField(3); if (!r) return Expected<L3ControlChannelDescription>::error(r.error()); result.mBS_PA_MFRMS = r.value();
    r = br.readField(8); if (!r) return Expected<L3ControlChannelDescription>::error(r.error()); result.mT3212 = r.value();
    return Expected<L3ControlChannelDescription>::hold(std::move(result));
}

void L3ControlChannelDescription::write(BitWriter& bw) const {
    bw.writeField(mMSC_R99, 1);
    bw.writeField(mATT, 1);
    bw.writeField(mBS_AG_BLKS_RES, 3);
    bw.writeField(mCCCH_CONF, 3);
    bw.writeField(mSI22IND, 1);
    bw.writeField(mCBQ3, 2);
    bw.writeField(0, 2);
    bw.writeField(mBS_PA_MFRMS, 3);
    bw.writeField(mT3212, 8);
}

void L3ControlChannelDescription::text(std::ostream& os) const {
    os << "ControlChannel[msc_r99=" << mMSC_R99 << " ATT=" << mATT
       << " BS_AG_BLKS_RES=" << mBS_AG_BLKS_RES << " CCCH_CONF=" << mCCCH_CONF
       << " si22ind=" << mSI22IND << " cbq3=" << mCBQ3
       << " BS_PA_MFRMS=" << mBS_PA_MFRMS << " T3212=" << mT3212 << "]";
}

// ── L3ChannelDescription ───────────────────────────────────────────────

L3ChannelDescription::L3ChannelDescription(TypeAndOffset tao, unsigned tn, unsigned tsc, unsigned arfcn)
    : mTypeAndOffset(static_cast<uint8_t>(tao)), mTN(static_cast<uint8_t>(tn)),
      mTSC(static_cast<uint8_t>(tsc)), mHFlag(0), mARFCN(static_cast<uint16_t>(arfcn)), mInitialized(true) {}

Expected<L3ChannelDescription> L3ChannelDescription::parse(BitReader& br) {
    L3ChannelDescription result;
    auto r = br.readField(5); if (!r) return Expected<L3ChannelDescription>::error(r.error()); result.mTypeAndOffset = static_cast<uint8_t>(r.value());
    r = br.readField(3); if (!r) return Expected<L3ChannelDescription>::error(r.error()); result.mTN = static_cast<uint8_t>(r.value());
    r = br.readField(3); if (!r) return Expected<L3ChannelDescription>::error(r.error()); result.mTSC = static_cast<uint8_t>(r.value());
    r = br.readField(1); if (!r) return Expected<L3ChannelDescription>::error(r.error()); result.mHFlag = r.value();
    if (result.mHFlag) {
        r = br.readField(6); if (!r) return Expected<L3ChannelDescription>::error(r.error()); result.mMAIO = static_cast<uint8_t>(r.value());
        r = br.readField(6); if (!r) return Expected<L3ChannelDescription>::error(r.error()); result.mHSN = static_cast<uint8_t>(r.value());
    } else {
        // Absolute RF channel number, twelve bits (TS 44.018 10.5.2.5).
        r = br.readField(12); if (!r) return Expected<L3ChannelDescription>::error(r.error()); result.mARFCN = static_cast<uint16_t>(r.value());
    }
    result.mInitialized = true;
    return Expected<L3ChannelDescription>::hold(std::move(result));
}

void L3ChannelDescription::write(BitWriter& bw) const {
    bw.writeField(mTypeAndOffset, 5);
    bw.writeField(mTN, 3);
    bw.writeField(mTSC, 3);
    bw.writeField(mHFlag, 1);
    if (mHFlag) {
        bw.writeField(mMAIO, 6);
        bw.writeField(mHSN, 6);
    } else {
        // Absolute RF channel number, twelve bits (TS 44.018 10.5.2.5).
        bw.writeField(mARFCN, 12);
    }
}

void L3ChannelDescription::text(std::ostream& os) const {
    os << "Channel[TypeAndOffset=" << mTypeAndOffset << " TN=" << mTN
       << " TSC=" << mTSC << " H=" << mHFlag;
    if (mHFlag) {
        os << " MAIO=" << mMAIO << " HSN=" << mHSN;
    } else {
        os << " ARFCN=" << mARFCN;
    }
    os << "]";
}

// ── L3ChannelDescription2 ──────────────────────────────────────────────

L3ChannelDescription2::L3ChannelDescription2(TypeAndOffset tao, unsigned tn, unsigned tsc, unsigned arfcn)
    : mTypeAndOffset(static_cast<uint8_t>(tao)), mTN(static_cast<uint8_t>(tn)),
      mTSC(static_cast<uint8_t>(tsc)), mHFlag(0), mARFCN(static_cast<uint16_t>(arfcn)) {}

L3ChannelDescription2::L3ChannelDescription2(const L3ChannelDescription& other)
    : mTypeAndOffset(other.typeAndOffset()), mTN(other.tn()), mTSC(other.tsc()),
      mHFlag(other.hFlag()), mARFCN(other.arfcn()), mMAIO(other.maio()), mHSN(other.hsn()) {}

Expected<L3ChannelDescription2> L3ChannelDescription2::parse(BitReader& br) {
    L3ChannelDescription2 result;
    auto r = br.readField(5); if (!r) return Expected<L3ChannelDescription2>::error(r.error()); result.mTypeAndOffset = static_cast<uint8_t>(r.value());
    r = br.readField(3); if (!r) return Expected<L3ChannelDescription2>::error(r.error()); result.mTN = static_cast<uint8_t>(r.value());
    r = br.readField(3); if (!r) return Expected<L3ChannelDescription2>::error(r.error()); result.mTSC = static_cast<uint8_t>(r.value());
    r = br.readField(1); if (!r) return Expected<L3ChannelDescription2>::error(r.error()); result.mHFlag = r.value();
    if (result.mHFlag) {
        r = br.readField(6); if (!r) return Expected<L3ChannelDescription2>::error(r.error()); result.mMAIO = static_cast<uint8_t>(r.value());
        r = br.readField(6); if (!r) return Expected<L3ChannelDescription2>::error(r.error()); result.mHSN = static_cast<uint8_t>(r.value());
    } else {
        // Absolute RF channel number, twelve bits (TS 44.018 10.5.2.5).
        r = br.readField(12); if (!r) return Expected<L3ChannelDescription2>::error(r.error()); result.mARFCN = static_cast<uint16_t>(r.value());
    }
    return Expected<L3ChannelDescription2>::hold(std::move(result));
}

void L3ChannelDescription2::write(BitWriter& bw) const {
    bw.writeField(mTypeAndOffset, 5);
    bw.writeField(mTN, 3);
    bw.writeField(mTSC, 3);
    bw.writeField(mHFlag, 1);
    if (mHFlag) {
        bw.writeField(mMAIO, 6);
        bw.writeField(mHSN, 6);
    } else {
        // Absolute RF channel number, twelve bits (TS 44.018 10.5.2.5).
        bw.writeField(mARFCN, 12);
    }
}

void L3ChannelDescription2::text(std::ostream& os) const {
    os << "Channel2[TypeAndOffset=" << mTypeAndOffset << " TN=" << mTN
       << " TSC=" << mTSC << " H=" << mHFlag;
    if (mHFlag) {
        os << " MAIO=" << mMAIO << " HSN=" << mHSN;
    } else {
        os << " ARFCN=" << mARFCN;
    }
    os << "]";
}

// ── L3AdditionalChannelDescription ─────────────────────────────────────

L3AdditionalChannelDescription::L3AdditionalChannelDescription(TypeAndOffset tao, unsigned tn, unsigned tsc, unsigned arfcn)
    : mTypeAndOffset(static_cast<uint8_t>(tao)), mTN(static_cast<uint8_t>(tn)),
      mTSC(static_cast<uint8_t>(tsc)), mHFlag(0), mARFCN(static_cast<uint16_t>(arfcn)), mInitialized(true) {}

Expected<L3AdditionalChannelDescription> L3AdditionalChannelDescription::parse(BitReader& br) {
    L3AdditionalChannelDescription result;
    auto r = br.readField(5); if (!r) return Expected<L3AdditionalChannelDescription>::error(r.error()); result.mTypeAndOffset = static_cast<uint8_t>(r.value());
    r = br.readField(3); if (!r) return Expected<L3AdditionalChannelDescription>::error(r.error()); result.mTN = static_cast<uint8_t>(r.value());
    r = br.readField(3); if (!r) return Expected<L3AdditionalChannelDescription>::error(r.error()); result.mTSC = static_cast<uint8_t>(r.value());
    r = br.readField(1); if (!r) return Expected<L3AdditionalChannelDescription>::error(r.error()); result.mHFlag = r.value();
    if (result.mHFlag) {
        r = br.readField(6); if (!r) return Expected<L3AdditionalChannelDescription>::error(r.error()); result.mMAIO = static_cast<uint8_t>(r.value());
        r = br.readField(6); if (!r) return Expected<L3AdditionalChannelDescription>::error(r.error()); result.mHSN = static_cast<uint8_t>(r.value());
    } else {
        // Absolute RF channel number, twelve bits (TS 44.018 10.5.2.5).
        r = br.readField(12); if (!r) return Expected<L3AdditionalChannelDescription>::error(r.error()); result.mARFCN = static_cast<uint16_t>(r.value());
    }
    result.mInitialized = true;
    return Expected<L3AdditionalChannelDescription>::hold(std::move(result));
}

void L3AdditionalChannelDescription::write(BitWriter& bw) const {
    bw.writeField(mTypeAndOffset, 5);
    bw.writeField(mTN, 3);
    bw.writeField(mTSC, 3);
    bw.writeField(mHFlag, 1);
    if (mHFlag) {
        bw.writeField(mMAIO, 6);
        bw.writeField(mHSN, 6);
    } else {
        // Absolute RF channel number, twelve bits (TS 44.018 10.5.2.5).
        bw.writeField(mARFCN, 12);
    }
}

void L3AdditionalChannelDescription::text(std::ostream& os) const {
    os << "AddlChannel[TypeAndOffset=" << mTypeAndOffset << " TN=" << mTN
       << " TSC=" << mTSC << " H=" << mHFlag;
    if (mHFlag) {
        os << " MAIO=" << mMAIO << " HSN=" << mHSN;
    } else {
        os << " ARFCN=" << mARFCN;
    }
    os << "]";
}

// ── L3PowerCommand ─────────────────────────────────────────────────────

Expected<L3PowerCommand> L3PowerCommand::parse(BitReader& br) {
    auto r = br.readField(5); if (!r) return Expected<L3PowerCommand>::error(r.error());
    uint8_t cmd = static_cast<uint8_t>(r.value());
    r = br.readField(3); if (!r) return Expected<L3PowerCommand>::error(r.error()); // spare
    return Expected<L3PowerCommand>::hold(L3PowerCommand(cmd));
}

void L3PowerCommand::write(BitWriter& bw) const {
    bw.writeField(mCommand, 5);
    bw.writeField(0, 3);
}

void L3PowerCommand::text(std::ostream& os) const {
    os << "PowerCommand[" << mCommand << "]";
}

// ── L3PowerCommandAndAccessType ────────────────────────────────────────

Expected<L3PowerCommandAndAccessType> L3PowerCommandAndAccessType::parse(BitReader& br) {
    auto r = br.readField(5); if (!r) return Expected<L3PowerCommandAndAccessType>::error(r.error());
    uint8_t cmd = static_cast<uint8_t>(r.value());
    r = br.readField(3); if (!r) return Expected<L3PowerCommandAndAccessType>::error(r.error());
    return Expected<L3PowerCommandAndAccessType>::hold(L3PowerCommandAndAccessType(cmd));
}

void L3PowerCommandAndAccessType::write(BitWriter& bw) const {
    bw.writeField(mCommand, 5);
    bw.writeField(0, 3);
}

void L3PowerCommandAndAccessType::text(std::ostream& os) const {
    os << "PowerCommandAT[" << mCommand << "]";
}

// ── L3ChannelMode ───────────────────────────────────────────────────────

Expected<L3ChannelMode> L3ChannelMode::parse(BitReader& br) {
    auto r = br.readField(8); if (!r) return Expected<L3ChannelMode>::error(r.error());
    return Expected<L3ChannelMode>::hold(L3ChannelMode(static_cast<Mode>(r.value())));
}

void L3ChannelMode::write(BitWriter& bw) const {
    bw.writeField(mMode, 8);
}

void L3ChannelMode::text(std::ostream& os) const {
    os << "ChannelMode[" << static_cast<int>(mMode) << "]";
}

// ── L3TimingAdvance ────────────────────────────────────────────────────

Expected<L3TimingAdvance> L3TimingAdvance::parse(BitReader& br) {
    // Timing Advance (TS 44.018 section 10.5.2.40): one octet, two spare bits in the
    // high position followed by the six-bit value; for values 0..63 the wire octet
    // equals the timing advance.
    auto r = br.readField(2); if (!r) return Expected<L3TimingAdvance>::error(r.error()); // spare
    r = br.readField(6); if (!r) return Expected<L3TimingAdvance>::error(r.error());
    uint8_t ta = static_cast<uint8_t>(r.value());
    return Expected<L3TimingAdvance>::hold(L3TimingAdvance(ta));
}

void L3TimingAdvance::write(BitWriter& bw) const {
    bw.writeField(0, 2); // spare (high bits)
    bw.writeField(mTimingAdvance & 0x3F, 6);
}

void L3TimingAdvance::text(std::ostream& os) const {
    os << "TimingAdvance[" << mTimingAdvance << "]";
}

// ── L3CellDescription ──────────────────────────────────────────────────

Expected<L3CellDescription> L3CellDescription::parse(BitReader& br) {
    auto r = br.readField(2); if (!r) return Expected<L3CellDescription>::error(r.error()); unsigned arfcnHigh = r.value();
    r = br.readField(3); if (!r) return Expected<L3CellDescription>::error(r.error()); uint8_t ncc = static_cast<uint8_t>(r.value());
    r = br.readField(3); if (!r) return Expected<L3CellDescription>::error(r.error()); uint8_t bcc = static_cast<uint8_t>(r.value());
    r = br.readField(8); if (!r) return Expected<L3CellDescription>::error(r.error()); uint16_t arfcnLow = static_cast<uint16_t>(r.value());
    return Expected<L3CellDescription>::hold(L3CellDescription((arfcnHigh << 8) | arfcnLow, ncc, bcc));
}

void L3CellDescription::write(BitWriter& bw) const {
    bw.writeField(mARFCN >> 8, 2);
    bw.writeField(mNCC, 3);
    bw.writeField(mBCC, 3);
    bw.writeField(mARFCN & 0xFF, 8);
}

void L3CellDescription::text(std::ostream& os) const {
    os << "CellDesc[ARFCN=" << mARFCN << " NCC=" << mNCC << " BCC=" << mBCC << "]";
}

// ── L3HandoverReference ────────────────────────────────────────────────

Expected<L3HandoverReference> L3HandoverReference::parse(BitReader& br) {
    auto r = br.readField(8); if (!r) return Expected<L3HandoverReference>::error(r.error());
    return Expected<L3HandoverReference>::hold(L3HandoverReference(r.value()));
}

void L3HandoverReference::write(BitWriter& bw) const {
    bw.writeField(mValue, 8);
}

void L3HandoverReference::text(std::ostream& os) const {
    os << "HORef[" << mValue << "]";
}

// ── L3CipheringModeSetting ─────────────────────────────────────────────

Expected<L3CipheringModeSetting> L3CipheringModeSetting::parse(BitReader& br) {
    auto r = br.readField(1); if (!r) return Expected<L3CipheringModeSetting>::error(r.error()); bool ciphering = r.value() != 0;
    r = br.readField(3); if (!r) return Expected<L3CipheringModeSetting>::error(r.error()); int algo = static_cast<int>(r.value());
    if (!ciphering) algo = 0;
    return Expected<L3CipheringModeSetting>::hold(L3CipheringModeSetting(ciphering, algo));
}

void L3CipheringModeSetting::write(BitWriter& bw) const {
    bw.writeField(mCiphering ? 1 : 0, 1);
    bw.writeField(mAlgorithm & 0x07, 3);
}

void L3CipheringModeSetting::text(std::ostream& os) const {
    os << "CipherMode[cipher=" << mCiphering << " algo=A5/" << (mCiphering ? mAlgorithm : 0) << "]";
}

// ── L3CipheringModeResponse ────────────────────────────────────────────

// Four bits: [cR(1)][spare(3)] — the response bit leads its half-octet
// (TS 44.018 section 9.1.x).
Expected<L3CipheringModeResponse> L3CipheringModeResponse::parse(BitReader& br) {
    auto r = br.readField(1); if (!r) return Expected<L3CipheringModeResponse>::error(r.error()); bool imeisv = r.value() != 0;
    r = br.readField(3); if (!r) return Expected<L3CipheringModeResponse>::error(r.error()); // spare
    return Expected<L3CipheringModeResponse>::hold(L3CipheringModeResponse(imeisv));
}

void L3CipheringModeResponse::write(BitWriter& bw) const {
    bw.writeField(mIncludeIMEISV ? 1 : 0, 1);
    bw.writeField(0, 3);
}

void L3CipheringModeResponse::text(std::ostream& os) const {
    os << "CipherResp[IMEISV=" << mIncludeIMEISV << "]";
}

// ── L3SynchronizationIndication ────────────────────────────────────────

Expected<L3SynchronizationIndication> L3SynchronizationIndication::parse(BitReader& br) {
    auto r = br.readField(4); if (!r) return Expected<L3SynchronizationIndication>::error(r.error()); // spare=0xD
    r = br.readField(1); if (!r) return Expected<L3SynchronizationIndication>::error(r.error()); bool nci = r.value() != 0;
    r = br.readField(1); if (!r) return Expected<L3SynchronizationIndication>::error(r.error()); bool rot = r.value() != 0;
    r = br.readField(2); if (!r) return Expected<L3SynchronizationIndication>::error(r.error()); int si = static_cast<int>(r.value());
    return Expected<L3SynchronizationIndication>::hold(L3SynchronizationIndication(nci, rot, si));
}

void L3SynchronizationIndication::write(BitWriter& bw) const {
    bw.writeField(0x0D, 4);
    bw.writeField(mNCI ? 1 : 0, 1);
    bw.writeField(mROT ? 1 : 0, 1);
    bw.writeField(mSI & 3, 2);
}

void L3SynchronizationIndication::text(std::ostream& os) const {
    os << "SyncInd[NCI=" << mNCI << " ROT=" << mROT << " SI=" << mSI << "]";
}

// ── L3NCCPermitted ─────────────────────────────────────────────────────

Expected<L3NCCPermitted> L3NCCPermitted::parse(BitReader& br) {
    auto r = br.readField(8); if (!r) return Expected<L3NCCPermitted>::error(r.error());
    return Expected<L3NCCPermitted>::hold(L3NCCPermitted(r.value()));
}

void L3NCCPermitted::write(BitWriter& bw) const {
    bw.writeField(mPermitted, 8);
}

void L3NCCPermitted::text(std::ostream& os) const {
    os << "NCCPermitted[0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(mPermitted) << "]";
}

// ── L3PageMode ─────────────────────────────────────────────────────────

// Page mode (TS 44.018 section 10.5.2.26): a four-bit field; the
// assigned codes are 0..3, any other value is rejected.
Expected<L3PageMode> L3PageMode::parse(BitReader& br) {
    auto r = br.readField(4);
    if (!r) return Expected<L3PageMode>::error(r.error());
    if (r.value() > 3u)
        return Expected<L3PageMode>::error(ParseError{ParseError::Code::InvalidValue, "unassigned page mode value"});
    return Expected<L3PageMode>::hold(L3PageMode(r.value()));
}

void L3PageMode::write(BitWriter& bw) const {
    bw.writeField(mPageMode & 0x0Fu, 4);
}

void L3PageMode::text(std::ostream& os) const {
    os << "PageMode[" << mPageMode << "]";
}

// ── L3RequestReference ─────────────────────────────────────────────────

Expected<L3RequestReference> L3RequestReference::parse(BitReader& br) {
    auto r = br.readField(8); if (!r) return Expected<L3RequestReference>::error(r.error()); uint8_t ra = static_cast<uint8_t>(r.value());
    r = br.readField(5); if (!r) return Expected<L3RequestReference>::error(r.error()); uint8_t t1p = static_cast<uint8_t>(r.value());
    r = br.readField(6); if (!r) return Expected<L3RequestReference>::error(r.error()); uint8_t t3 = static_cast<uint8_t>(r.value());
    r = br.readField(5); if (!r) return Expected<L3RequestReference>::error(r.error()); uint8_t t2 = static_cast<uint8_t>(r.value());
    return Expected<L3RequestReference>::hold(L3RequestReference(ra, t1p, t2, t3));
}

void L3RequestReference::write(BitWriter& bw) const {
    bw.writeField(mRA, 8);
    bw.writeField(mT1p, 5);
    bw.writeField(mT3, 6);
    bw.writeField(mT2, 5);
}

void L3RequestReference::text(std::ostream& os) const {
    os << "ReqRef[RA=" << mRA << " T1=" << mT1p << " T2=" << mT2 << " T3=" << mT3 << "]";
}

// ── L3WaitIndication ───────────────────────────────────────────────────

Expected<L3WaitIndication> L3WaitIndication::parse(BitReader& br) {
    auto r = br.readField(8); if (!r) return Expected<L3WaitIndication>::error(r.error());
    return Expected<L3WaitIndication>::hold(L3WaitIndication(r.value()));
}

void L3WaitIndication::write(BitWriter& bw) const {
    bw.writeField(mValue, 8);
}

void L3WaitIndication::text(std::ostream& os) const {
    os << "WaitInd[" << mValue << "s]";
}

// ── L3RRCauseElement ───────────────────────────────────────────────────

Expected<L3RRCauseElement> L3RRCauseElement::parse(BitReader& br) {
    auto r = br.readField(8); if (!r) return Expected<L3RRCauseElement>::error(r.error());
    return Expected<L3RRCauseElement>::hold(L3RRCauseElement(static_cast<RRCause>(r.value())));
}

void L3RRCauseElement::write(BitWriter& bw) const {
    bw.writeField(static_cast<uint32_t>(mCauseValue), 8);
}

void L3RRCauseElement::text(std::ostream& os) const {
    os << "RRCause[" << RRCause2Str(mCauseValue) << "]";
}

// ── L3CellOptionsBCCH ──────────────────────────────────────────────────

Expected<L3CellOptionsBCCH> L3CellOptionsBCCH::parse(BitReader& br) {
    L3CellOptionsBCCH result;
    auto r = br.readField(1); if (!r) return Expected<L3CellOptionsBCCH>::error(r.error()); // dn_ind spare
    r = br.readField(1); if (!r) return Expected<L3CellOptionsBCCH>::error(r.error()); result.mPWRC = r.value();
    r = br.readField(2); if (!r) return Expected<L3CellOptionsBCCH>::error(r.error()); result.mDTX = r.value();
    r = br.readField(4); if (!r) return Expected<L3CellOptionsBCCH>::error(r.error()); result.mRADIO_LINK_TIMEOUT = r.value();
    return Expected<L3CellOptionsBCCH>::hold(std::move(result));
}

void L3CellOptionsBCCH::write(BitWriter& bw) const {
    bw.writeField(0, 1);
    bw.writeField(mPWRC, 1);
    bw.writeField(mDTX, 2);
    bw.writeField(mRADIO_LINK_TIMEOUT, 4);
}

void L3CellOptionsBCCH::text(std::ostream& os) const {
    os << "CellOptsBCCH[PWRC=" << mPWRC << " DTX=" << mDTX << " RLT=" << mRADIO_LINK_TIMEOUT << "]";
}

// ── L3CellOptionsSACCH ─────────────────────────────────────────────────

Expected<L3CellOptionsSACCH> L3CellOptionsSACCH::parse(BitReader& br) {
    L3CellOptionsSACCH result;
    auto r = br.readField(1); if (!r) return Expected<L3CellOptionsSACCH>::error(r.error()); result.mDTX = r.value() << 2;
    r = br.readField(1); if (!r) return Expected<L3CellOptionsSACCH>::error(r.error()); result.mPWRC = r.value();
    r = br.readField(2); if (!r) return Expected<L3CellOptionsSACCH>::error(r.error()); result.mDTX |= r.value();
    r = br.readField(4); if (!r) return Expected<L3CellOptionsSACCH>::error(r.error()); result.mRADIO_LINK_TIMEOUT = r.value();
    return Expected<L3CellOptionsSACCH>::hold(std::move(result));
}

void L3CellOptionsSACCH::write(BitWriter& bw) const {
    bw.writeField((mDTX >> 2) & 0x01, 1);
    bw.writeField(mPWRC, 1);
    bw.writeField(mDTX & 0x03, 2);
    bw.writeField(mRADIO_LINK_TIMEOUT, 4);
}

void L3CellOptionsSACCH::text(std::ostream& os) const {
    os << "CellOptsSACCH[PWRC=" << mPWRC << " DTX=" << mDTX << " RLT=" << mRADIO_LINK_TIMEOUT << "]";
}

// ── L3CellSelectionParameters ──────────────────────────────────────────

Expected<L3CellSelectionParameters> L3CellSelectionParameters::parse(BitReader& br) {
    L3CellSelectionParameters result;
    auto r = br.readField(3); if (!r) return Expected<L3CellSelectionParameters>::error(r.error()); result.mCELL_RESELECT_HYSTERESIS = r.value();
    r = br.readField(5); if (!r) return Expected<L3CellSelectionParameters>::error(r.error()); result.mMS_TXPWR_MAX_CCH = r.value();
    r = br.readField(1); if (!r) return Expected<L3CellSelectionParameters>::error(r.error()); result.mACS = r.value();
    r = br.readField(1); if (!r) return Expected<L3CellSelectionParameters>::error(r.error()); result.mNECI = r.value();
    r = br.readField(6); if (!r) return Expected<L3CellSelectionParameters>::error(r.error()); result.mRXLEV_ACCESS_MIN = r.value();
    return Expected<L3CellSelectionParameters>::hold(std::move(result));
}

void L3CellSelectionParameters::write(BitWriter& bw) const {
    bw.writeField(mCELL_RESELECT_HYSTERESIS, 3);
    bw.writeField(mMS_TXPWR_MAX_CCH, 5);
    bw.writeField(mACS, 1);
    bw.writeField(mNECI, 1);
    bw.writeField(mRXLEV_ACCESS_MIN, 6);
}

void L3CellSelectionParameters::text(std::ostream& os) const {
    os << "CellSelParams[ACS=" << mACS << " NECI=" << mNECI
       << " Hyst=" << mCELL_RESELECT_HYSTERESIS << " TXPWR=" << mMS_TXPWR_MAX_CCH
       << " RXLEV=" << mRXLEV_ACCESS_MIN << "]";
}

// ── L3RACHControlParameters ────────────────────────────────────────────

Expected<L3RACHControlParameters> L3RACHControlParameters::parse(BitReader& br) {
    L3RACHControlParameters result;
    auto r = br.readField(2); if (!r) return Expected<L3RACHControlParameters>::error(r.error()); result.mMaxRetrans = r.value();
    r = br.readField(4); if (!r) return Expected<L3RACHControlParameters>::error(r.error()); result.mTxInteger = r.value();
    r = br.readField(1); if (!r) return Expected<L3RACHControlParameters>::error(r.error()); result.mCellBarAccess = r.value();
    r = br.readField(1); if (!r) return Expected<L3RACHControlParameters>::error(r.error()); result.mRE = r.value();
    // Access class bitmap (TS 44.018 section 10.5.2.29): bit i corresponds to
    // access class i; the low octet (classes 0-7) is transmitted first.
    auto o0 = br.readField(8); if (!o0) return Expected<L3RACHControlParameters>::error(o0.error());
    r = br.readField(8); if (!r) return Expected<L3RACHControlParameters>::error(r.error());
    result.mAC = static_cast<uint16_t>(o0.value() | (r.value() << 8));
    return Expected<L3RACHControlParameters>::hold(std::move(result));
}

void L3RACHControlParameters::write(BitWriter& bw) const {
    bw.writeField(mMaxRetrans, 2);
    bw.writeField(mTxInteger, 4);
    bw.writeField(mCellBarAccess, 1);
    bw.writeField(mRE, 1);
    bw.writeField(mAC & 0xFFu, 8);
    bw.writeField((mAC >> 8) & 0xFFu, 8);
}

void L3RACHControlParameters::text(std::ostream& os) const {
    os << "RACHControl[MaxRetrans=" << mMaxRetrans << " TxInteger=" << mTxInteger
       << " CellBarAccess=" << mCellBarAccess << " RE=" << mRE << " AC=" << mAC << "]";
}

// ── L3ImmediateAssignmentInformation ───────────────────────────────────

size_t L3ImmediateAssignmentInformation::lengthV() const {
    return 1 + mPowerOffsetLength; // 1 byte header + data
}

Expected<L3ImmediateAssignmentInformation> L3ImmediateAssignmentInformation::parse(BitReader& br) {
    L3ImmediateAssignmentInformation result;
    auto r = br.readField(5); if (!r) return Expected<L3ImmediateAssignmentInformation>::error(r.error()); result.mPowerOffset = r.value();
    r = br.readField(3); if (!r) return Expected<L3ImmediateAssignmentInformation>::error(r.error()); result.mPowerOffsetLength = r.value();
    for (size_t i = 0; i < result.mPowerOffsetLength; ++i) {
        r = br.readField(8); if (!r) return Expected<L3ImmediateAssignmentInformation>::error(r.error());
        result.mPowerOffsetData.push_back(static_cast<uint8_t>(r.value()));
    }
    return Expected<L3ImmediateAssignmentInformation>::hold(std::move(result));
}

void L3ImmediateAssignmentInformation::write(BitWriter& bw) const {
    bw.writeField(mPowerOffset, 5);
    bw.writeField(mPowerOffsetLength, 3);
    for (uint8_t b : mPowerOffsetData) bw.writeField(b, 8);
}

void L3ImmediateAssignmentInformation::text(std::ostream& os) const {
    os << "IAInfo[PO=" << mPowerOffset << " POLen=" << mPowerOffsetLength << "]";
}

// ── L3MeasurementResults ───────────────────────────────────────────────

int L3MeasurementResults::decodeLevToDBm(unsigned lev) const {
    if (lev == 0) return -110;
    if (lev >= 63) return -47;
    return -110 + static_cast<int>(lev);
}

float L3MeasurementResults::decodeQualToBER(unsigned qual) const {
    static const float berTable[] = {0.0f, 0.002f, 0.004f, 0.008f, 0.016f, 0.032f, 0.08f, 0.16f, -1.0f};
    if (qual >= 8) return -1.0f;
    return berTable[qual];
}

Expected<L3MeasurementResults> L3MeasurementResults::parse(BitReader& br) {
    L3MeasurementResults result;
    auto r = br.readField(1); if (!r) return Expected<L3MeasurementResults>::error(r.error()); result.mBA_USED = r.value();
    r = br.readField(1); if (!r) return Expected<L3MeasurementResults>::error(r.error()); result.mDTX_USED = r.value();
    r = br.readField(6); if (!r) return Expected<L3MeasurementResults>::error(r.error()); result.mRXLEV_FULL_SERVING_CELL = r.value();
    r = br.readField(1); if (!r) return Expected<L3MeasurementResults>::error(r.error()); // spare
    r = br.readField(1); if (!r) return Expected<L3MeasurementResults>::error(r.error()); result.mMEAS_VALID = r.value();
    r = br.readField(6); if (!r) return Expected<L3MeasurementResults>::error(r.error()); result.mRXLEV_SUB_SERVING_CELL = r.value();
    r = br.readField(1); if (!r) return Expected<L3MeasurementResults>::error(r.error()); // spare
    r = br.readField(3); if (!r) return Expected<L3MeasurementResults>::error(r.error()); result.mRXQUAL_FULL_SERVING_CELL = r.value();
    r = br.readField(3); if (!r) return Expected<L3MeasurementResults>::error(r.error()); result.mRXQUAL_SUB_SERVING_CELL = r.value();
    r = br.readField(3); if (!r) return Expected<L3MeasurementResults>::error(r.error()); result.mNO_NCELL = r.value();
    for (unsigned i = 0; i < result.mNO_NCELL && i < 6; ++i) {
        r = br.readField(6); if (!r) return Expected<L3MeasurementResults>::error(r.error()); result.mRXLEV_NCELL[i] = r.value();
        r = br.readField(5); if (!r) return Expected<L3MeasurementResults>::error(r.error()); result.mBCCH_FREQ_NCELL[i] = r.value();
        r = br.readField(6); if (!r) return Expected<L3MeasurementResults>::error(r.error()); result.mBSIC_NCELL[i] = r.value();
    }
    return Expected<L3MeasurementResults>::hold(std::move(result));
}

void L3MeasurementResults::write(BitWriter& bw) const {
    bw.writeField(mBA_USED ? 1 : 0, 1);
    bw.writeField(mDTX_USED ? 1 : 0, 1);
    bw.writeField(mRXLEV_FULL_SERVING_CELL, 6);
    bw.writeField(0, 1);
    bw.writeField(mMEAS_VALID ? 1 : 0, 1);
    bw.writeField(mRXLEV_SUB_SERVING_CELL, 6);
    bw.writeField(0, 1);
    bw.writeField(mRXQUAL_FULL_SERVING_CELL, 3);
    bw.writeField(mRXQUAL_SUB_SERVING_CELL, 3);
    bw.writeField(mNO_NCELL, 3);
    for (unsigned i = 0; i < mNO_NCELL && i < 6; ++i) {
        bw.writeField(mRXLEV_NCELL[i], 6);
        bw.writeField(mBCCH_FREQ_NCELL[i], 5);
        bw.writeField(mBSIC_NCELL[i], 6);
    }
}

void L3MeasurementResults::text(std::ostream& os) const {
    os << "MeasResults[BA=" << mBA_USED << " DTX=" << mDTX_USED
       << " RXLEV=" << mRXLEV_FULL_SERVING_CELL << " RXQUAL=" << mRXQUAL_FULL_SERVING_CELL
       << " NCELL=" << mNO_NCELL << "]";
}

// ── L3MultiRateConfiguration ───────────────────────────────────────────

L3MultiRateConfiguration::L3MultiRateConfiguration(bool halfrate)
    : mOptions(0x20), mAmrCodecSet(halfrate ? codec_set_AMR_HR : codec_set_AMR_FR) {}

Expected<L3MultiRateConfiguration> L3MultiRateConfiguration::parse(BitReader& br) {
    auto r = br.readField(8); if (!r) return Expected<L3MultiRateConfiguration>::error(r.error()); unsigned opts = r.value();
    r = br.readField(8); if (!r) return Expected<L3MultiRateConfiguration>::error(r.error()); AmrCodecSet cs = static_cast<AmrCodecSet>(r.value());
    L3MultiRateConfiguration result;
    result.mOptions = opts;
    result.mAmrCodecSet = cs;
    return Expected<L3MultiRateConfiguration>::hold(std::move(result));
}

void L3MultiRateConfiguration::write(BitWriter& bw) const {
    bw.writeField(mOptions, 8);
    bw.writeField(static_cast<unsigned>(mAmrCodecSet), 8);
}

void L3MultiRateConfiguration::text(std::ostream& os) const {
    os << "MultiRate[opts=0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(mOptions)
       << " codec=0x" << std::setw(2) << std::setfill('0') << static_cast<int>(mAmrCodecSet) << "]";
}

// ── L3APDUID ───────────────────────────────────────────────────────────

Expected<L3APDUID> L3APDUID::parse(BitReader& br) {
    auto r = br.readField(4); if (!r) return Expected<L3APDUID>::error(r.error());
    return Expected<L3APDUID>::hold(L3APDUID(r.value()));
}

void L3APDUID::write(BitWriter& bw) const {
    bw.writeField(mProtocolIdentifier, 4);
}

void L3APDUID::text(std::ostream& os) const {
    os << "APDUID[" << mProtocolIdentifier << "]";
}

// ── L3APDUFlags ────────────────────────────────────────────────────────

Expected<L3APDUFlags> L3APDUFlags::parse(BitReader& br) {
    auto r = br.readField(1); if (!r) return Expected<L3APDUFlags>::error(r.error()); // spare
    r = br.readField(1); if (!r) return Expected<L3APDUFlags>::error(r.error()); unsigned cr = r.value();
    r = br.readField(1); if (!r) return Expected<L3APDUFlags>::error(r.error()); unsigned first = r.value();
    r = br.readField(1); if (!r) return Expected<L3APDUFlags>::error(r.error()); unsigned last = r.value();
    return Expected<L3APDUFlags>::hold(L3APDUFlags(cr, first, last));
}

void L3APDUFlags::write(BitWriter& bw) const {
    bw.writeField(0, 1);
    bw.writeField(mCR, 1);
    bw.writeField(mFirstSegment, 1);
    bw.writeField(mLastSegment, 1);
}

void L3APDUFlags::text(std::ostream& os) const {
    os << "APDUFlags[CR=" << mCR << " first=" << mFirstSegment << " last=" << mLastSegment << "]";
}

// ── L3APDUData ─────────────────────────────────────────────────────────

size_t L3APDUData::lengthV() const {
    return (mData.size() * 8 + 7) / 8; // approximate: each byte = 8 bits
}

Expected<L3APDUData> L3APDUData::parse(BitReader& br, size_t lengthBytes) {
    std::vector<uint8_t> data(lengthBytes);
    auto r = br.readBytes(data.data(), lengthBytes);
    if (!r) return Expected<L3APDUData>::error(r.error());
    return Expected<L3APDUData>::hold(L3APDUData(std::move(data)));
}

void L3APDUData::write(BitWriter& bw) const {
    for (uint8_t b : mData) bw.writeField(b, 8);
}

void L3APDUData::text(std::ostream& os) const {
    os << "APDUData[" << mData.size() << "bytes]";
}

// ── L3MobileAllocation ─────────────────────────────────────────────────

Expected<L3MobileAllocation> L3MobileAllocation::parse(BitReader& br, size_t lengthBytes) {
    std::vector<uint8_t> data(lengthBytes);
    auto r = br.readBytes(data.data(), lengthBytes);
    if (!r) return Expected<L3MobileAllocation>::error(r.error());
    return Expected<L3MobileAllocation>::hold(L3MobileAllocation(std::move(data)));
}

void L3MobileAllocation::write(BitWriter& bw) const {
    bw.writeBytes(mData.data(), mData.size());
}

void L3MobileAllocation::text(std::ostream& os) const {
    os << "MobAlloc[" << mData.size() << "octets]";
    for (uint8_t b : mData) os << " " << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << static_cast<int>(b);
    os << std::dec;
}

// ── L3DedicatedModeOrTBF ───────────────────────────────────────────────

L3DedicatedModeOrTBF::L3DedicatedModeOrTBF(bool forTBF, bool downlink)
    : mDownlink(downlink ? 1 : 0), mTMA(0), mDMOrTBF(forTBF ? 1 : 0) {}

Expected<L3DedicatedModeOrTBF> L3DedicatedModeOrTBF::parse(BitReader& br) {
    auto r = br.readField(1); if (!r) return Expected<L3DedicatedModeOrTBF>::error(r.error()); // spare
    r = br.readField(1); if (!r) return Expected<L3DedicatedModeOrTBF>::error(r.error()); unsigned tma = r.value();
    r = br.readField(1); if (!r) return Expected<L3DedicatedModeOrTBF>::error(r.error()); unsigned dl = r.value();
    r = br.readField(1); if (!r) return Expected<L3DedicatedModeOrTBF>::error(r.error()); unsigned tbf = r.value();
    L3DedicatedModeOrTBF result;
    result.mDownlink = dl;
    result.mTMA = tma;
    result.mDMOrTBF = tbf;
    return Expected<L3DedicatedModeOrTBF>::hold(std::move(result));
}

void L3DedicatedModeOrTBF::write(BitWriter& bw) const {
    bw.writeField(0, 1);
    bw.writeField(mTMA, 1);
    bw.writeField(mDownlink, 1);
    bw.writeField(mDMOrTBF, 1);
}

void L3DedicatedModeOrTBF::text(std::ostream& os) const {
    os << "DedModeOrTBF[DL=" << mDownlink << " TBF=" << mDMOrTBF << "]";
}

// ── L3FollowOnProceed ──────────────────────────────────────────────────

Expected<L3FollowOnProceed> L3FollowOnProceed::parse(BitReader& br) {
    auto r = br.readField(8); if (!r) return Expected<L3FollowOnProceed>::error(r.error());
    return Expected<L3FollowOnProceed>::hold(L3FollowOnProceed{});
}

void L3FollowOnProceed::write(BitWriter& bw) const {
    bw.writeField(0xA1, 8);
}

// ── L3CellSelection ────────────────────────────────────────────────────

size_t L3CellSelection::lengthV() const {
    return 4 + (mCellBarQualifierLength + 1) / 2;
}

Expected<L3CellSelection> L3CellSelection::parse(BitReader& br) {
    L3CellSelection result;
    auto r = br.readField(5); if (!r) return Expected<L3CellSelection>::error(r.error()); result.mRxLevAccessMin = r.value();
    r = br.readField(1); if (!r) return Expected<L3CellSelection>::error(r.error()); result.mRxLevelAccessMin = r.value();
    r = br.readField(5); if (!r) return Expected<L3CellSelection>::error(r.error()); result.mMaxRxLev = r.value();
    r = br.readField(3); if (!r) return Expected<L3CellSelection>::error(r.error()); result.mCellReselectionHysteresis = r.value();
    r = br.readField(3); if (!r) return Expected<L3CellSelection>::error(r.error()); result.mCellReselectionOffset = r.value();
    r = br.readField(2); if (!r) return Expected<L3CellSelection>::error(r.error()); result.mCellReservedIndicator = r.value();
    r = br.readField(4); if (!r) return Expected<L3CellSelection>::error(r.error()); result.mCellBarQualifierValue = r.value();
    r = br.readField(4); if (!r) return Expected<L3CellSelection>::error(r.error()); result.mCellBarQualifierLength = r.value();
    size_t bytes = (result.mCellBarQualifierLength + 1) / 2;
    for (size_t i = 0; i < bytes; ++i) {
        r = br.readField(8); if (!r) return Expected<L3CellSelection>::error(r.error());
        result.mCellBarQualifierData.push_back(static_cast<uint8_t>(r.value()));
    }
    return Expected<L3CellSelection>::hold(std::move(result));
}

void L3CellSelection::write(BitWriter& bw) const {
    bw.writeField(mRxLevAccessMin, 5);
    bw.writeField(mRxLevelAccessMin, 1);
    bw.writeField(mMaxRxLev, 5);
    bw.writeField(mCellReselectionHysteresis, 3);
    bw.writeField(mCellReselectionOffset, 3);
    bw.writeField(mCellReservedIndicator, 2);
    bw.writeField(mCellBarQualifierValue, 4);
    bw.writeField(mCellBarQualifierLength, 4);
    for (uint8_t b : mCellBarQualifierData) bw.writeField(b, 8);
}

void L3CellSelection::text(std::ostream& os) const {
    os << "CellSelection[RxMin=" << mRxLevAccessMin << " MaxRx=" << mMaxRxLev
       << " Hyst=" << mCellReselectionHysteresis << " Offset=" << mCellReselectionOffset
       << " CBI=" << mCellReservedIndicator << " CBQ=" << mCellBarQualifierValue
       << " CBQLen=" << mCellBarQualifierLength << "]";
}

// ── L3RestOctets ───────────────────────────────────────────────────────
// (virtual parse factory removed — dead code)

// ── L3SI3RestOctets ────────────────────────────────────────────────────
// SI3 Rest Octets bit layout (TS 44.018 section 10.5.2.34), MSB-first in
// the bit stream; each optional group is its presence bit followed by the
// value bits:
//   sel_pres(1) [cbq(1)|cell_reselect_offset(6)|temporary_offset(3)|penalty_time(5)]
//   pwr_pres(1) [power_offset(2)]
//   si_2ter_ind(1)  early_cm_ind(1)
//   sched_pres(1) [where(3)]
//   gprs_pres(1) [ra_colour(3)|si13_pos(1)]
//   umts_early_cm_ind(1)
//   si2quater_pres(1) [si2quater_ind(1)]
//   iu_mode_ind(1)             — only when gprs_pres = 0
//   si21_pres(1) [si21_pos(1)] — only when sched_pres = 0

Expected<L3SI3RestOctets> L3SI3RestOctets::parse(BitReader& br) {
    // The record starts directly with the selection-parameter presence bit
    // (TS 44.018 section 10.5.2.34), so a frame without rest octets — or a
    // truncated one — parses to the all-absent default: reads stop at end
    // of input instead of failing.
    L3SI3RestOctets result;
    auto readField = [&br](unsigned nbits, unsigned& out) -> bool {
        if (br.remainingBits() < nbits) return false;
        auto r = br.readField(nbits);
        if (!r) return false;
        out = r.value();
        return true;
    };
    unsigned v{};
    if (readField(1, v) && v) {                       // sel_pres
        result.mSelPresent = true;                    // [cbq|cro(6)|to(3)|pt(5)]
        if (readField(1, v)) result.mCBQ = (v != 0);
        if (readField(6, v)) result.mCRO = v;
        if (readField(3, v)) result.mTO = v;
        if (readField(5, v)) result.mPT = v;
    }
    if (readField(1, v) && v) {                       // pwr_pres
        result.mPwrOffsetPresent = true;              // [power_offset(2)]
        if (readField(2, v)) result.mPowerOffset = v;
    }
    if (readField(1, v)) result.mSi2terInd = (v != 0);
    if (readField(1, v)) result.mEarlyCmInd = (v != 0);
    if (readField(1, v) && v) {                       // sched_pres
        result.mSchedWherePresent = true;             // [where(3)]
        if (readField(3, v)) result.mWhere = v;
    }
    if (readField(1, v) && v) {                       // gprs_pres
        result.mHaveGPRS = true;                      // [ra_colour(3)|si13_pos(1)]
        if (readField(3, v)) result.mRA_COLOUR = v;
        if (readField(1, v)) result.mSi13Position = (v != 0);
    }
    if (readField(1, v)) result.mUmtsEarlyCmInd = (v != 0);
    if (readField(1, v) && v) {                       // si2quater_pres
        result.mSi2quaterPresent = true;              // [si2quater_ind(1)]
        if (readField(1, v)) result.mSi2quaterInd = (v != 0);
    }
    if (!result.mHaveGPRS) {                          // iu_mode_ind — only when gprs_pres = 0
        if (readField(1, v)) {
            result.mIuModePresent = true;
            result.mIuModeInd = (v != 0);
        }
    }
    if (!result.mSchedWherePresent) {                 // si21 group — only when sched_pres = 0
        if (readField(1, v)) {
            result.mSi21Present = (v != 0);
            if (v) {
                unsigned p{};
                if (readField(1, p)) result.mSi21Pos = p;
            }
        }
    }
    return Expected<L3SI3RestOctets>::hold(std::move(result));
}

size_t L3SI3RestOctets::lengthV() const {
    size_t bits = 1;                                  // sel_pres
    if (mSelPresent) bits += 1 + 6 + 3 + 5;           // cbq|cro|to|pt
    bits += 1;                                        // pwr_pres
    if (mPwrOffsetPresent) bits += 2;                 // power_offset
    bits += 1 + 1;                                    // si_2ter_ind, early_cm_ind
    bits += 1;                                        // sched_pres
    if (mSchedWherePresent) bits += 3;                // where
    bits += 1;                                        // gprs_pres
    if (mHaveGPRS) bits += 3 + 1;                     // ra_colour, si13_pos
    bits += 1;                                        // umts_early_cm_ind
    bits += 1;                                        // si2quater_pres
    if (mSi2quaterPresent) bits += 1;                 // si2quater_ind
    if (!mHaveGPRS) bits += 1;                        // iu_mode_ind
    if (!mSchedWherePresent) {                        // si21 group
        bits += 1;                                    // si21_pres
        if (mSi21Present) bits += 1;                  // si21_pos
    }
    return (bits + 7) / 8;
}

void L3SI3RestOctets::write(BitWriter& bw) const {
    bw.writeField(mSelPresent ? 1u : 0u, 1);
    if (mSelPresent) {
        bw.writeField(mCBQ ? 1u : 0u, 1);
        bw.writeField(mCRO & 0x3Fu, 6);
        bw.writeField(mTO & 0x07u, 3);
        bw.writeField(mPT & 0x1Fu, 5);
    }
    bw.writeField(mPwrOffsetPresent ? 1u : 0u, 1);
    if (mPwrOffsetPresent) bw.writeField(mPowerOffset & 0x03u, 2);
    bw.writeField(mSi2terInd ? 1u : 0u, 1);
    bw.writeField(mEarlyCmInd ? 1u : 0u, 1);
    bw.writeField(mSchedWherePresent ? 1u : 0u, 1);
    if (mSchedWherePresent) bw.writeField(mWhere & 0x07u, 3);
    bw.writeField(mHaveGPRS ? 1u : 0u, 1);
    if (mHaveGPRS) {
        bw.writeField(mRA_COLOUR & 0x07u, 3);
        bw.writeField(mSi13Position ? 1u : 0u, 1);
    }
    bw.writeField(mUmtsEarlyCmInd ? 1u : 0u, 1);
    bw.writeField(mSi2quaterPresent ? 1u : 0u, 1);
    if (mSi2quaterPresent) bw.writeField(mSi2quaterInd ? 1u : 0u, 1);
    if (!mHaveGPRS) bw.writeField(mIuModeInd ? 1u : 0u, 1);
    if (!mSchedWherePresent) {
        bw.writeField(mSi21Present ? 1u : 0u, 1);
        if (mSi21Present) bw.writeField(mSi21Pos & 0x01u, 1);
    }
    bw.alignToOctet();   // sub-octet padding is zero-filled (TS 44.018 10.5.2.34)
}

void L3SI3RestOctets::text(std::ostream& os) const {
    os << "SI3RestOctets";
    if (mSelPresent) {
        os << " CBQ=" << mCBQ << " CRO=" << mCRO
           << " TO=" << mTO << " PT=" << mPT;
    }
    if (mPwrOffsetPresent) os << " PwrOff=" << mPowerOffset;
    if (mSi2terInd) os << " SI2terInd";
    if (mEarlyCmInd) os << " EarlyCMInd";
    if (mSchedWherePresent) os << " SchedWhere=" << mWhere;
    if (mHaveGPRS) os << " GPRS RA_COLOUR=" << mRA_COLOUR << " SI13Pos=" << mSi13Position;
    if (mUmtsEarlyCmInd) os << " UmtsEarlyCMInd";
    if (mSi2quaterPresent) os << " SI2quaterInd=" << mSi2quaterInd;
    if (!mHaveGPRS && mIuModeInd) os << " IuModeInd";
    if (!mSchedWherePresent && mSi21Present) os << " SI21Pos=" << mSi21Pos;
}

// ── L3SIType4RestOctets ────────────────────────────────────────────────
// SI4 Rest Octets bit layout (TS 44.018 section 10.5.2.35), MSB-first in
// the bit stream:
//   sel_pres(1) [cbq(1)|cell_reselect_offset(6)|temporary_offset(3)|penalty_time(5)]
//   pwr_pres(1) [power_offset(2)]
//   gprs_pres(1) [ra_colour(3)|si13_pos(1)]
//   s_presence(1) — when set, all remaining octets of the frame are the
//                    opaque "Rest Octets S" and are preserved verbatim

Expected<L3SIType4RestOctets> L3SIType4RestOctets::parse(BitReader& br) {
    // Reads stop at end of input instead of failing on truncated frames;
    // when the S-part presence bit is set, all remaining whole octets of
    // the frame are captured as opaque data (TS 44.018 section 10.5.2.35).
    L3SIType4RestOctets result;
    auto readField = [&br](unsigned nbits, unsigned& out) -> bool {
        if (br.remainingBits() < nbits) return false;
        auto r = br.readField(nbits);
        if (!r) return false;
        out = r.value();
        return true;
    };
    unsigned v{};
    if (readField(1, v) && v) {                       // sel_pres
        result.mSelPresent = true;                    // [cbq|cro(6)|to(3)|pt(5)]
        if (readField(1, v)) result.mCBQ = (v != 0);
        if (readField(6, v)) result.mCRO = v;
        if (readField(3, v)) result.mTO = v;
        if (readField(5, v)) result.mPT = v;
    }
    if (readField(1, v) && v) {                       // pwr_pres
        result.mPwrOffsetPresent = true;              // [power_offset(2)]
        if (readField(2, v)) result.mPowerOffset = v;
    }
    if (readField(1, v) && v) {                       // gprs_pres
        result.mHaveGPRS = true;                      // [ra_colour(3)|si13_pos(1)]
        if (readField(3, v)) result.mRA_COLOUR = v;
        if (readField(1, v)) result.mSi13Position = (v != 0);
    }
    if (readField(1, v) && v) {                       // s_presence
        result.mRestSPresent = true;
        br.alignToOctet();                            // drop sub-octet padding
        const size_t bytes = br.remainingBits() / 8;
        if (bytes > 0) {
            std::vector<uint8_t> tail(bytes);
            auto r = br.readBytes(tail.data(), bytes);
            if (!r) return Expected<L3SIType4RestOctets>::error(r.error());
            result.mRestS = std::move(tail);
        }
    }
    return Expected<L3SIType4RestOctets>::hold(std::move(result));
}

size_t L3SIType4RestOctets::lengthV() const {
    size_t bits = 1;                                  // sel_pres
    if (mSelPresent) bits += 1 + 6 + 3 + 5;           // cbq|cro|to|pt
    bits += 1;                                        // pwr_pres
    if (mPwrOffsetPresent) bits += 2;                 // power_offset
    bits += 1;                                        // gprs_pres
    if (mHaveGPRS) bits += 3 + 1;                     // ra_colour, si13_pos
    bits += 1;                                        // s_presence
    size_t len = (bits + 7) / 8;                      // O part, octet-aligned
    if (mRestSPresent) len += mRestS.size();          // opaque S tail
    return len;
}

void L3SIType4RestOctets::write(BitWriter& bw) const {
    bw.writeField(mSelPresent ? 1u : 0u, 1);
    if (mSelPresent) {
        bw.writeField(mCBQ ? 1u : 0u, 1);
        bw.writeField(mCRO & 0x3Fu, 6);
        bw.writeField(mTO & 0x07u, 3);
        bw.writeField(mPT & 0x1Fu, 5);
    }
    bw.writeField(mPwrOffsetPresent ? 1u : 0u, 1);
    if (mPwrOffsetPresent) bw.writeField(mPowerOffset & 0x03u, 2);
    bw.writeField(mHaveGPRS ? 1u : 0u, 1);
    if (mHaveGPRS) {
        bw.writeField(mRA_COLOUR & 0x07u, 3);
        bw.writeField(mSi13Position ? 1u : 0u, 1);
    }
    bw.writeField(mRestSPresent ? 1u : 0u, 1);
    if (mRestSPresent) {
        bw.alignToOctet();   // zero-fill to the octet boundary before the S tail
        bw.writeBytes(mRestS.data(), mRestS.size());
    }
}

void L3SIType4RestOctets::text(std::ostream& os) const {
    os << "SI4RestOctets";
    if (mSelPresent) {
        os << " CBQ=" << mCBQ << " CRO=" << mCRO
           << " TO=" << mTO << " PT=" << mPT;
    }
    if (mPwrOffsetPresent) os << " PwrOff=" << mPowerOffset;
    if (mHaveGPRS) os << " GPRS RA_COLOUR=" << mRA_COLOUR << " SI13Pos=" << mSi13Position;
    if (mRestSPresent) os << " RestS[" << mRestS.size() << " octets]";
}

// ── L3IARestOctets ─────────────────────────────────────────────────────

size_t L3IARestOctets::lengthBits() const {
    return mHavePacketAssignment ? 8 : 0;
}

void L3IARestOctets::writeBits(BitWriter& bw) const {
    if (mHavePacketAssignment) bw.writeField(0, 8);
}

void L3IARestOctets::text(std::ostream& os) const {
    os << "IARestOctets";
    if (mHavePacketAssignment) os << " [PacketAssignment]";
}

// ── GPRS elements ──────────────────────────────────────────────────────

size_t L3GPRSCellOptions::lengthBits() const { return 20; }

void L3GPRSCellOptions::writeBits(BitWriter& bw) const {
    bw.writeField(mNMO, 2);
    bw.writeField(mT3168, 3);
    bw.writeField(mT3192, 3);
    bw.writeField(mDRX_TIMER_MAX, 3);
    bw.writeField(mACCESS_BURST_TYPE, 1);
    bw.writeField(mCONTROL_ACK_TYPE, 1);
    bw.writeField(mBS_VC_MAX, 4);
    bw.writeField(0, 2);
}

void L3GPRSCellOptions::text(std::ostream& os) const {
    os << "GPRSCellOpts[NMO=" << mNMO << " T3168=" << mT3168
       << " T3192=" << mT3192 << " DRX=" << mDRX_TIMER_MAX << "]";
}

size_t L3GPRSSI13PowerControlParameters::lengthBits() const { return 23; }

void L3GPRSSI13PowerControlParameters::writeBits(BitWriter& bw) const {
    bw.writeField(mALPHA, 4);
    bw.writeField(0, 5);
    bw.writeField(0, 5);
    bw.writeField(0, 1);
    bw.writeField(15, 4);
}

void L3GPRSSI13PowerControlParameters::text(std::ostream& os) const {
    os << "GPRSPowerCtrl[ALPHA=" << mALPHA << "]";
}

// ── L3SI13RestOctets ───────────────────────────────────────────────────

size_t L3SI13RestOctets::lengthV() const {
    size_t bits = 1 + 3 + 4 + 1 + 1 + 8 + 1 + 3 + 2;
    bits += mCellOptions.lengthBits();
    bits += mPowerControlParameters.lengthBits();
    return (bits + 7) / 8;
}

void L3SI13RestOctets::write(BitWriter& bw) const {
    bw.writeField(1, 1);
    bw.writeField(0, 3);
    bw.writeField(0, 4);
    bw.writeField(0, 1);
    bw.writeField(0, 1);
    bw.writeField(mRAC, 8);
    bw.writeField(mSPGC_CCCH_SUP ? 1 : 0, 1);
    bw.writeField(mPRIORITY_ACCESS_THR, 3);
    bw.writeField(mNETWORK_CONTROL_ORDER, 2);
    mCellOptions.writeBits(bw);
    mPowerControlParameters.writeBits(bw);
}

void L3SI13RestOctets::text(std::ostream& os) const {
    os << "SI13RestOctets[RAC=" << mRAC << " SPGC=" << mSPGC_CCCH_SUP
       << " PAT=" << mPRIORITY_ACCESS_THR << " NCO=" << mNETWORK_CONTROL_ORDER << "]";
}

// ── L3OctetAlignedProtocolElement ──────────────────────────────────────

Expected<L3OctetAlignedProtocolElement> L3OctetAlignedProtocolElement::parse(BitReader& br, size_t lengthBytes) {
    std::vector<uint8_t> data(lengthBytes);
    auto r = br.readBytes(data.data(), lengthBytes);
    if (!r) return Expected<L3OctetAlignedProtocolElement>::error(r.error());
    L3OctetAlignedProtocolElement result;
    result.mData.assign(data.begin(), data.end());
    result.mExtant = true;
    return Expected<L3OctetAlignedProtocolElement>::hold(std::move(result));
}

void L3OctetAlignedProtocolElement::write(BitWriter& bw) const {
    for (size_t i = 0; i < mData.size(); ++i) {
        bw.writeField(static_cast<unsigned char>(mData[i]), 8);
    }
}

void L3OctetAlignedProtocolElement::text(std::ostream& os) const {
    os << "RawOctets[" << mData.size() << "]";
}

// ── Detail helpers ─────────────────────────────────────────────────────

namespace detail {

size_t skipLV(BitReader&, size_t lengthBytes) {
    return lengthBytes;
}

bool parseHasT(BitReader& br, unsigned expectedIEI) {
    auto r = br.readField(7);
    if (!r) return false;
    return (r.value() & 0x7F) == (expectedIEI & 0x7F);
}

size_t skipTLV(BitReader& br, unsigned expectedIEI) {
    auto r = br.readField(7); if (!r) return 0;
    unsigned ieI = r.value() & 0x7F;
    if (ieI != (expectedIEI & 0x7F)) return 0;
    // If extension bit set, read length octet
    bool ext = (r.value() & 0x80) != 0;
    size_t len = 0;
    if (ext) {
        r = br.readField(8); if (!r) return 0;
        len = r.value();
    } else {
        // Fixed length for known IEI - caller should handle
    }
    return len;
}

// Opaque tail (TS 24.008 / TS 24.068 optional information elements): the
// octets that follow the mandatory part of a message but are not modelled
// by typed fields are kept verbatim so the write path re-emits them without
// loss. An empty sequence costs zero heap allocation.
bool readOpaqueTail(BitReader& br, std::vector<uint8_t>& out) {
    while (br.hasMore()) {
        auto b = br.readField(8);
        if (!b) return false;
        out.push_back(static_cast<uint8_t>(b.value()));
    }
    return true;
}

void writeOpaqueTail(const std::vector<uint8_t>& v, BitWriter& bw) {
    for (uint8_t b : v) bw.writeField(b, 8);
}

} // namespace detail

} // namespace gsml3parser
