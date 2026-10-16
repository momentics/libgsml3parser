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

// GMM Information Elements - parse/write/text implementation
// Spec: 3GPP TS 24.008 section 10.5.7
// IE layouts per 3GPP TS 24.008 section 10.5.7.

#include "gsml3parser/gmm/l3gmmelements.h"
#include <sstream>
#include <iomanip>

namespace gsml3parser {

const char* GMMCause2Str(GMMCause cause) {
    switch (cause) {
        case GMMCause::Unspecified: return "Unspecified";
        case GMMCause::IMSI_Unknown_In_HLR: return "IMSI unknown in HLR";
        case GMMCause::Illegal_MS: return "Illegal MS";
        case GMMCause::IMEI_Not_Accepted: return "IMEI not accepted";
        case GMMCause::Illegal_ME: return "Illegal ME";
        case GMMCause::GPRS_Services_Not_Allowed: return "GPRS services not allowed";
        case GMMCause::GPRS_And_Non_GPRS_Services_Not_Allowed: return "GPRS and non-GPRS services not allowed";
        case GMMCause::MS_Identity_Cannot_Be_Derived: return "MS identity cannot be derived";
        case GMMCause::Implicitly_Detached: return "Implicitly detached";
        case GMMCause::PLMN_Not_Allowed: return "PLMN not allowed";
        case GMMCause::Location_Area_Not_Allowed: return "Location area not allowed";
        case GMMCause::Roaming_Not_Allowed_In_Location_Area: return "Roaming not allowed in location area";
        case GMMCause::GPRS_Services_Not_Allowed_In_PLMN: return "GPRS services not allowed in PLMN";
        case GMMCause::No_Suitable_Cells_In_Location_Routing_Area: return "No suitable cells in location/routing area";
        case GMMCause::MSC_Temporarily_Not_Reachable: return "MSC temporarily not reachable";
        case GMMCause::Network_Failure: return "Network failure";
        case GMMCause::MAC_Failure: return "MAC failure";
        case GMMCause::Synch_Failure: return "Sync failure";
        case GMMCause::Congestion: return "Congestion";
        case GMMCause::GSM_Authentication_Unacceptable: return "GSM authentication unacceptable";
        case GMMCause::Not_Authorized_For_This_CSG: return "Not authorized for this CSG";
        case GMMCause::No_PDP_Context_Activated: return "No PDP context activated";
        case GMMCause::Semantically_Incorrect_Message: return "Semantically incorrect message";
        case GMMCause::Invalid_Mandatory_Information: return "Invalid mandatory information";
        case GMMCause::Message_Type_Not_Implemented: return "Message type not implemented";
        case GMMCause::Message_Type_Not_Compatible_With_State: return "Message type not compatible with state";
        case GMMCause::Protocol_Error_Unspecified: return "Protocol error unspecified";
    }
    return "Unknown";
}

// ── L3PDPContextStatus ────────────────────────────────────────────────

Expected<L3PDPContextStatus> L3PDPContextStatus::parse(BitReader& br) {
    L3PDPContextStatus status;
    auto r = br.readBytes(status.mValue.data(), 2);
    if (!r) return Expected<L3PDPContextStatus>::error(r.error());
    return Expected<L3PDPContextStatus>::hold(std::move(status));
}

void L3PDPContextStatus::write(BitWriter& bw) const {
    bw.writeBytes(mValue.data(), 2);
}

void L3PDPContextStatus::text(std::ostream& os) const {
    os << "PDPContextStatus(";
    for (int i = 0; i < 16; ++i) {
        if (context(i + 1)) os << (i + 1);
    }
    os << ")";
}

// ── L3T3302Timer ──────────────────────────────────────────────────────

Expected<L3T3302Timer> L3T3302Timer::parse(BitReader& br) {
    auto r = br.readField(8);
    if (!r) return Expected<L3T3302Timer>::error(r.error());
    return Expected<L3T3302Timer>::hold(L3T3302Timer{static_cast<uint8_t>(r.value())});
}

void L3T3302Timer::write(BitWriter& bw) const {
    bw.writeField(mValue, 8);
}

void L3T3302Timer::text(std::ostream& os) const {
    os << "T3302(" << mValue << ")";
}

// ── L3MSNetworkCapability ─────────────────────────────────────────────

Expected<L3MSNetworkCapability> L3MSNetworkCapability::parse(BitReader& br, size_t lengthBytes) {
    if (lengthBytes == 0) return Expected<L3MSNetworkCapability>::hold(L3MSNetworkCapability{});
    std::vector<uint8_t> data(lengthBytes);
    auto r = br.readBytes(data.data(), lengthBytes);
    if (!r) return Expected<L3MSNetworkCapability>::error(r.error());
    return Expected<L3MSNetworkCapability>::hold(L3MSNetworkCapability{std::move(data)});
}

void L3MSNetworkCapability::write(BitWriter& bw) const {
    if (!mValue.empty()) {
        bw.writeBytes(mValue.data(), mValue.size());
    }
}

unsigned L3MSNetworkCapability::gea1() const {
    return mValue.empty() ? 0 : (mValue[0] >> 7) & 1;
}
unsigned L3MSNetworkCapability::smsViaDedicated() const {
    return mValue.empty() ? 0 : (mValue[0] >> 6) & 1;
}
unsigned L3MSNetworkCapability::smsViaGprs() const {
    return mValue.empty() ? 0 : (mValue[0] >> 5) & 1;
}
unsigned L3MSNetworkCapability::ucs2() const {
    return mValue.empty() ? 0 : (mValue[0] >> 4) & 1;
}
unsigned L3MSNetworkCapability::ssScreening() const {
    return mValue.empty() ? 0 : (mValue[0] >> 2) & 3;
}
unsigned L3MSNetworkCapability::solSa() const {
    return mValue.empty() ? 0 : (mValue[0] >> 1) & 1;
}
unsigned L3MSNetworkCapability::revisionLevel() const {
    return mValue.empty() ? 0 : mValue[0] & 1;
}

void L3MSNetworkCapability::text(std::ostream& os) const {
    os << "MSNetworkCap(";
    for (size_t i = 0; i < mValue.size(); ++i) {
        if (i > 0) os << ':';
        os << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << static_cast<int>(mValue[i]);
    }
    os << ")";
}

// ── L3RoutingAreaIdentification ───────────────────────────────────────

L3RoutingAreaIdentification::L3RoutingAreaIdentification(const char* wMCC, const char* wMNC, unsigned wLAC, unsigned wRAC) {
    // Natural digit order: mMCC[0..2] = MCC digits 1..3.
    for (int i = 0; i < 3; ++i) {
        mMCC[i] = (wMCC[i] >= '0' && wMCC[i] <= '9') ? static_cast<unsigned>(wMCC[i] - '0') : 0;
    }
    // MNC digits 1..3 in natural order; a 2-digit MNC stores the 'F' fill
    // (0x0F) as the third digit.
    for (int i = 0; i < 3; ++i) {
        mMNC[i] = (wMNC[i] >= '0' && wMNC[i] <= '9') ? static_cast<unsigned>(wMNC[i] - '0') : 0x0F;
    }
    mLAC = static_cast<uint16_t>(wLAC);
    mRAC = static_cast<uint8_t>(wRAC);
}

int L3RoutingAreaIdentification::mcc() const {
    int val = 0;
    for (int i = 0; i < 3; ++i) {
        if (mMCC[i] < 10) val = val * 10 + mMCC[i];
    }
    return val;
}

int L3RoutingAreaIdentification::mnc() const {
    int val = 0;
    for (int i = 0; i < 3; ++i) {
        if (mMNC[i] < 10) val = val * 10 + mMNC[i];
    }
    return val;
}

Expected<L3RoutingAreaIdentification> L3RoutingAreaIdentification::parse(BitReader& br) {
    // PLMN BCD packing (TS 24.008 section 10.5.1.3): same layout as the LAI,
    // followed by the 16-bit LAC and the 8-bit RAC:
    //   octet 1 = [MCC digit 2 | MCC digit 1]
    //   octet 2 = [MNC digit 3 (or F fill for a 2-digit MNC) | MCC digit 3]
    //   octet 3 = [MNC digit 2 | MNC digit 1]
    L3RoutingAreaIdentification rai;
    auto r = br.readField(4); if (!r) return Expected<L3RoutingAreaIdentification>::error(r.error()); unsigned mcc2_ = r.value();
    r = br.readField(4); if (!r) return Expected<L3RoutingAreaIdentification>::error(r.error()); unsigned mcc1_ = r.value();
    r = br.readField(4); if (!r) return Expected<L3RoutingAreaIdentification>::error(r.error()); unsigned mnc3_ = r.value();
    r = br.readField(4); if (!r) return Expected<L3RoutingAreaIdentification>::error(r.error()); unsigned mcc3_ = r.value();
    r = br.readField(4); if (!r) return Expected<L3RoutingAreaIdentification>::error(r.error()); unsigned mnc2_ = r.value();
    r = br.readField(4); if (!r) return Expected<L3RoutingAreaIdentification>::error(r.error()); unsigned mnc1_ = r.value();

    // Natural digit order: see L3LocationAreaIdentity::parse.
    rai.mMCC[0] = mcc1_; rai.mMCC[1] = mcc2_; rai.mMCC[2] = mcc3_;
    rai.mMNC[0] = mnc1_; rai.mMNC[1] = mnc2_; rai.mMNC[2] = mnc3_;

    // LAC: 2 octets, MSB first
    r = br.readField(16); if (!r) return Expected<L3RoutingAreaIdentification>::error(r.error());
    rai.mLAC = static_cast<uint16_t>(r.value());

    // RAC: 1 octet
    r = br.readField(8); if (!r) return Expected<L3RoutingAreaIdentification>::error(r.error());
    rai.mRAC = static_cast<uint8_t>(r.value());

    return Expected<L3RoutingAreaIdentification>::hold(std::move(rai));
}

void L3RoutingAreaIdentification::write(BitWriter& bw) const {
    // PLMN BCD packing (TS 24.008 section 10.5.1.3): same as the LAI,
    // followed by the 16-bit LAC (MSB first) and the 8-bit RAC.
    bw.writeField(mMCC[1], 4);   // octet 1 high: MCC digit 2
    bw.writeField(mMCC[0], 4);   // octet 1 low:  MCC digit 1
    bw.writeField(mMNC[2], 4);   // octet 2 high: MNC digit 3 (0xF for a 2-digit MNC)
    bw.writeField(mMCC[2], 4);   // octet 2 low:  MCC digit 3
    bw.writeField(mMNC[1], 4);   // octet 3 high: MNC digit 2
    bw.writeField(mMNC[0], 4);   // octet 3 low:  MNC digit 1
    bw.writeField(mLAC, 16);
    bw.writeField(mRAC, 8);
}

void L3RoutingAreaIdentification::text(std::ostream& os) const {
    os << "RAI(MCC=" << mcc() << ",MNC=" << mnc() << ",LAC=" << lac() << ",RAC=" << static_cast<int>(mRAC) << ")";
}

// ── L3DRXParameter ────────────────────────────────────────────────────

Expected<L3DRXParameter> L3DRXParameter::parse(BitReader& br) {
    L3DRXParameter param;
    auto o1 = br.readField(8);
    if (!o1) return Expected<L3DRXParameter>::error(o1.error());
    param.mSplitPGCycleCode = static_cast<uint8_t>(o1.value());

    auto o2 = br.readField(8);
    if (!o2) return Expected<L3DRXParameter>::error(o2.error());
    param.mNonDRXTimer = (o2.value() >> 5) & 0x07;
    param.mSplitOnCCCH = (o2.value() >> 4) & 0x01;
    param.mCNSpecificDRXCycleLength = o2.value() & 0x0F;

    return Expected<L3DRXParameter>::hold(std::move(param));
}

void L3DRXParameter::write(BitWriter& bw) const {
    bw.writeField(mSplitPGCycleCode, 8);
    bw.writeField(((mNonDRXTimer & 0x07) << 5) | ((mSplitOnCCCH & 0x01) << 4) | (mCNSpecificDRXCycleLength & 0x0F), 8);
}

void L3DRXParameter::text(std::ostream& os) const {
    os << "DRXParam(cycle=" << static_cast<int>(mSplitPGCycleCode)
       << ",nonDRX=" << mNonDRXTimer << ",splitCCCH=" << mSplitOnCCCH
       << ",cnCycle=" << mCNSpecificDRXCycleLength << ")";
}

// ── L3GMMCauseIE ──────────────────────────────────────────────────────

Expected<L3GMMCauseIE> L3GMMCauseIE::parse(BitReader& br) {
    auto r = br.readField(8);
    if (!r) return Expected<L3GMMCauseIE>::error(r.error());
    return Expected<L3GMMCauseIE>::hold(L3GMMCauseIE{static_cast<GMMCause>(r.value())});
}

void L3GMMCauseIE::write(BitWriter& bw) const {
    bw.writeField(static_cast<uint8_t>(mCause), 8);
}

void L3GMMCauseIE::text(std::ostream& os) const {
    os << "GMMCause(" << GMMCause2Str(mCause) << ")";
}

// ── L3AuthRAND ────────────────────────────────────────────────────────

Expected<L3AuthRAND> L3AuthRAND::parse(BitReader& br) {
    L3AuthRAND rand;
    auto r = br.readBytes(rand.mValue.data(), 16);
    if (!r) return Expected<L3AuthRAND>::error(r.error());
    return Expected<L3AuthRAND>::hold(std::move(rand));
}

void L3AuthRAND::write(BitWriter& bw) const {
    bw.writeBytes(mValue.data(), 16);
}

void L3AuthRAND::text(std::ostream& os) const {
    os << "RAND(";
    for (int i = 0; i < 16; ++i) {
        if (i > 0) os << ':';
        os << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << static_cast<int>(mValue[i]);
    }
    os << ")";
}

// ── L3AuthRES ─────────────────────────────────────────────────────────

Expected<L3AuthRES> L3AuthRES::parse(BitReader& br) {
    L3AuthRES res;
    auto r = br.readBytes(res.mValue.data(), 4);
    if (!r) return Expected<L3AuthRES>::error(r.error());
    return Expected<L3AuthRES>::hold(std::move(res));
}

void L3AuthRES::write(BitWriter& bw) const {
    bw.writeBytes(mValue.data(), 4);
}

void L3AuthRES::text(std::ostream& os) const {
    os << "RES(";
    for (int i = 0; i < 4; ++i) {
        if (i > 0) os << ':';
        os << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << static_cast<int>(mValue[i]);
    }
    os << ")";
}

// ── L3AuthFailureParam ────────────────────────────────────────────────

Expected<L3AuthFailureParam> L3AuthFailureParam::parse(BitReader& br) {
    if (!br.hasMore()) return Expected<L3AuthFailureParam>::error(
        ParseError{ParseError::Code::TruncatedInput, "AuthFailureParam"});
    auto len = br.readField(8);
    if (!len) return Expected<L3AuthFailureParam>::error(len.error());
    size_t n = len.value();
    L3AuthFailureParam param;
    param.mAUTS.resize(n);
    auto r = br.readBytes(param.mAUTS.data(), n);
    if (!r) return Expected<L3AuthFailureParam>::error(r.error());
    return Expected<L3AuthFailureParam>::hold(std::move(param));
}

void L3AuthFailureParam::write(BitWriter& bw) const {
    bw.writeField(static_cast<uint32_t>(mAUTS.size()), 8);
    if (!mAUTS.empty()) bw.writeBytes(mAUTS.data(), mAUTS.size());
}

void L3AuthFailureParam::text(std::ostream& os) const {
    os << "AUTS(";
    for (size_t i = 0; i < mAUTS.size(); ++i) {
        if (i > 0) os << ':';
        os << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << static_cast<int>(mAUTS[i]);
    }
    os << ")";
}

// ── L3PTMSISignature ──────────────────────────────────────────────────

Expected<L3PTMSISignature> L3PTMSISignature::parse(BitReader& br) {
    L3PTMSISignature sig;
    auto r = br.readBytes(sig.mValue.data(), 3);
    if (!r) return Expected<L3PTMSISignature>::error(r.error());
    return Expected<L3PTMSISignature>::hold(std::move(sig));
}

void L3PTMSISignature::write(BitWriter& bw) const {
    bw.writeBytes(mValue.data(), 3);
}

void L3PTMSISignature::text(std::ostream& os) const {
    os << "P_TMSISig(";
    for (int i = 0; i < 3; ++i) {
        if (i > 0) os << ':';
        os << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << static_cast<int>(mValue[i]);
    }
    os << ")";
}

// ── L3GMMStatusCause ──────────────────────────────────────────────────

Expected<L3GMMStatusCause> L3GMMStatusCause::parse(BitReader& br) {
    auto r = br.readField(8);
    if (!r) return Expected<L3GMMStatusCause>::error(r.error());
    return Expected<L3GMMStatusCause>::hold(L3GMMStatusCause{static_cast<GMMCause>(r.value())});
}

void L3GMMStatusCause::write(BitWriter& bw) const {
    bw.writeField(static_cast<uint8_t>(mCause), 8);
}

void L3GMMStatusCause::text(std::ostream& os) const {
    os << "GMMStatusCause(" << GMMCause2Str(mCause) << ")";
}

} // namespace gsml3parser
