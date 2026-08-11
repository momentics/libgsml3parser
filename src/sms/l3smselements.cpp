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

// SMS TP Elements — parse/write implementation
// Spec: 3GPP TS 23.040 sections 7.2, 9.2; 3GPP TS 24.008 section 9.6
// Reference: ref/osmo-ttcn3-hacks/library/L3_Templates.ttcn — TPDU templates

#include "gsml3parser/sms/l3smselements.h"
#include <sstream>
#include <iomanip>

namespace gsml3parser {

// ── Enum string converters ─────────────────────────────────────────────

const char* TPDCS2Str(TPDCS dcs) {
    switch (dcs) {
        case TPDCS::Default_Alphabet: return "Default-Alphabet";
        case TPDCS::Default_8bit:     return "Default-8bit";
        case TPDCS::UCS2:             return "UCS2";
    }
    return "Unknown";
}

const char* TPPID2Str(TPPID pid) {
    switch (pid) {
        case TPPID::Default: return "Default";
        case TPPID::GSM:     return "GSM";
        case TPPID::X121:    return "X.121";
        case TPPID::Telex:   return "Telex";
        case TPPID::LandLine: return "Land-Line";
        case TPPID::SS7_DestinationAccess: return "SS7-Destination-Access";
    }
    return "Unknown";
}

// ── TPSCTimeStamp (23.040 9.2.4.4) ────────────────────────────────────

Expected<TPSCTimeStamp> TPSCTimeStamp::parse(BitReader& br) {
    TPSCTimeStamp ts;
    auto r = br.readField(8);
    if (!r) return Expected<TPSCTimeStamp>::error(r.error());
    ts.year = static_cast<uint8_t>(r.value());

    r = br.readField(8);
    if (!r) return Expected<TPSCTimeStamp>::error(r.error());
    ts.month = static_cast<uint8_t>(r.value());

    r = br.readField(8);
    if (!r) return Expected<TPSCTimeStamp>::error(r.error());
    ts.day = static_cast<uint8_t>(r.value());

    r = br.readField(8);
    if (!r) return Expected<TPSCTimeStamp>::error(r.error());
    ts.hour = static_cast<uint8_t>(r.value());

    r = br.readField(8);
    if (!r) return Expected<TPSCTimeStamp>::error(r.error());
    ts.minute = static_cast<uint8_t>(r.value());

    r = br.readField(8);
    if (!r) return Expected<TPSCTimeStamp>::error(r.error());
    ts.second = static_cast<uint8_t>(r.value());

    r = br.readField(8);
    if (!r) return Expected<TPSCTimeStamp>::error(r.error());
    ts.timezone = static_cast<int8_t>(r.value());

    return Expected<TPSCTimeStamp>::hold(std::move(ts));
}

void TPSCTimeStamp::write(BitWriter& bw) const {
    bw.writeField(static_cast<uint32_t>(year), 8);
    bw.writeField(static_cast<uint32_t>(month), 8);
    bw.writeField(static_cast<uint32_t>(day), 8);
    bw.writeField(static_cast<uint32_t>(hour), 8);
    bw.writeField(static_cast<uint32_t>(minute), 8);
    bw.writeField(static_cast<uint32_t>(second), 8);
    bw.writeField(static_cast<uint32_t>(static_cast<uint8_t>(timezone)), 8);
}

void TPSCTimeStamp::text(std::ostream& os) const {
    os << "SCTS(" << static_cast<int>(year) << "/"
       << static_cast<int>(month) << "/" << static_cast<int>(day) << " "
       << static_cast<int>(hour) << ":" << static_cast<int>(minute) << ":"
       << static_cast<int>(second) << " tz=" << static_cast<int>(timezone) << ")";
}

// ── L3TPAddress (23.040 9.1.2.4) ──────────────────────────────────────

L3TPAddress::L3TPAddress(TypeOfNumber ton, NumberingPlan npi, std::vector<uint8_t> digits)
    : mTon(ton), mNpi(npi), mDigits(std::move(digits)) {
    size_t digitBytes = (mDigits.size() + 1) / 2 * 2;
    mLength = static_cast<uint8_t>(1 + digitBytes);
}

Expected<L3TPAddress> L3TPAddress::parse(BitReader& br) {
    L3TPAddress addr;

    auto len = br.readField(8);
    if (!len) return Expected<L3TPAddress>::error(len.error());
    addr.mLength = static_cast<uint8_t>(len.value());

    if (addr.mLength < 1) {
        return Expected<L3TPAddress>::hold(std::move(addr));
    }

    auto tonNpi = br.readField(8);
    if (!tonNpi) return Expected<L3TPAddress>::error(tonNpi.error());
    uint8_t rawTonNpi = static_cast<uint8_t>(tonNpi.value());
    addr.mTon = static_cast<TypeOfNumber>((rawTonNpi >> 4) & 0x07);
    addr.mNpi = static_cast<NumberingPlan>(rawTonNpi & 0x0F);

    size_t digitBytes = addr.mLength - 1;
    for (size_t i = 0; i < digitBytes; ++i) {
        auto d = br.readField(8);
        if (!d) return Expected<L3TPAddress>::error(d.error());
        addr.mDigits.push_back(static_cast<uint8_t>(d.value()));
    }

    return Expected<L3TPAddress>::hold(std::move(addr));
}

void L3TPAddress::write(BitWriter& bw) const {
    bw.writeField(mLength, 8);
    if (mLength >= 1) {
        bw.writeField(((static_cast<uint8_t>(mTon) & 0x07) << 4) | (static_cast<uint8_t>(mNpi) & 0x0F), 8);
        for (uint8_t d : mDigits) {
            bw.writeField(d, 8);
        }
    }
}

void L3TPAddress::text(std::ostream& os) const {
    os << "TP-Addr(ton=" << mTon << ",npi=" << mNpi << ",digits=[";
    for (size_t i = 0; i < mDigits.size(); ++i) {
        if (i > 0) os << ",";
        os << "0x" << std::hex << static_cast<int>(mDigits[i]);
    }
    os << "])";
}

// ── L3TPDeliver (23.040 9.2.2.1) ──────────────────────────────────────

size_t L3TPDeliver::bodyLength() const {
    size_t len = 1; // header octet
    len += mOriginatingAddress.totalLength(); // TP-OA (length byte + content)
    len += 1; // TP-PID
    len += 1; // TP-DCS
    if (mScts) len += 7; // TP-SCTS
    len += 1; // TP-UDL
    len += mUserData.size(); // TP-UD
    return len;
}

Expected<L3TPDeliver> L3TPDeliver::parse(BitReader& br) {
    L3TPDeliver msg;

    auto hdr = br.readField(8);
    if (!hdr) return Expected<L3TPDeliver>::error(hdr.error());
    uint8_t rawHdr = static_cast<uint8_t>(hdr.value());
    msg.mMms = (rawHdr & 0x20) != 0;
    msg.mSri = (rawHdr & 0x04) != 0;
    msg.mUdhi = (rawHdr & 0x02) != 0;
    msg.mRp = (rawHdr & 0x01) != 0;

    // TP-OA: Originating Address (LV format: length byte + TON_NPI + digits)
    auto oaResult = L3TPAddress::parse(br);
    if (!oaResult) return Expected<L3TPDeliver>::error(oaResult.error());
    msg.mOriginatingAddress = std::move(oaResult.value());

    // TP-PID
    auto pid = br.readField(8);
    if (!pid) return Expected<L3TPDeliver>::error(pid.error());
    msg.mPid = static_cast<TPPID>(pid.value());

    // TP-DCS
    auto dcs = br.readField(8);
    if (!dcs) return Expected<L3TPDeliver>::error(dcs.error());
    msg.mDcs = static_cast<TPDCS>(dcs.value());

    // TP-SCTS: always present in SMS-Deliver (7 octets)
    auto sctsResult = TPSCTimeStamp::parse(br);
    if (!sctsResult) return Expected<L3TPDeliver>::error(sctsResult.error());
    msg.mScts = std::move(sctsResult.value());

    // TP-UDL
    auto udl = br.readField(8);
    if (!udl) return Expected<L3TPDeliver>::error(udl.error());
    msg.mUdl = static_cast<uint8_t>(udl.value());

    // TP-UD: user data (mUdl octets)
    for (size_t i = 0; i < msg.mUdl; ++i) {
        auto ud = br.readField(8);
        if (!ud) return Expected<L3TPDeliver>::error(ud.error());
        msg.mUserData.push_back(static_cast<uint8_t>(ud.value()));
    }

    return Expected<L3TPDeliver>::hold(std::move(msg));
}

void L3TPDeliver::write(BitWriter& bw) const {
    uint8_t hdr = 0x00;
    if (mMms) hdr |= 0x20;
    if (mSri) hdr |= 0x04;
    if (mUdhi) hdr |= 0x02;
    if (mRp) hdr |= 0x01;
    bw.writeField(hdr, 8);

    mOriginatingAddress.write(bw);
    bw.writeField(static_cast<uint8_t>(mPid), 8);
    bw.writeField(static_cast<uint8_t>(mDcs), 8);
    if (mScts) mScts->write(bw);
    bw.writeField(mUdl, 8);

    for (uint8_t b : mUserData) {
        bw.writeField(b, 8);
    }
}

void L3TPDeliver::text(std::ostream& os) const {
    os << "TP-Deliver(oa=";
    mOriginatingAddress.text(os);
    os << ",pid=" << static_cast<int>(mPid) << ",dcs=" << static_cast<int>(mDcs)
       << ",udl=" << static_cast<int>(mUdl);
    if (mScts) {
        os << ",";
        mScts->text(os);
    }
    os << ")";
}

// ── L3TPSubmit (23.040 9.2.2.2) ───────────────────────────────────────

size_t L3TPSubmit::bodyLength() const {
    size_t len = 1; // header octet
    len += 1; // TP-MR
    len += mDestinationAddress.totalLength(); // TP-DA
    len += 1; // TP-PID
    len += 1; // TP-DCS
    len += 1; // TP-UDL
    len += mUserData.size(); // TP-UD
    return len;
}

Expected<L3TPSubmit> L3TPSubmit::parse(BitReader& br) {
    L3TPSubmit msg;

    auto hdr = br.readField(8);
    if (!hdr) return Expected<L3TPSubmit>::error(hdr.error());
    uint8_t rawHdr = static_cast<uint8_t>(hdr.value());
    msg.mRd = (rawHdr & 0x20) != 0;
    msg.mVpf = (rawHdr >> 3) & 0x03;
    msg.mSrr = (rawHdr & 0x04) != 0;
    msg.mUdhi = (rawHdr & 0x02) != 0;
    msg.mRp = (rawHdr & 0x01) != 0;

    // TP-MR: Message Reference
    auto mr = br.readField(8);
    if (!mr) return Expected<L3TPSubmit>::error(mr.error());
    msg.mMr = static_cast<uint8_t>(mr.value());

    // TP-DA: Destination Address (LV format)
    auto daResult = L3TPAddress::parse(br);
    if (!daResult) return Expected<L3TPSubmit>::error(daResult.error());
    msg.mDestinationAddress = std::move(daResult.value());

    // TP-PID
    auto pid = br.readField(8);
    if (!pid) return Expected<L3TPSubmit>::error(pid.error());
    msg.mPid = static_cast<TPPID>(pid.value());

    // TP-DCS
    auto dcs = br.readField(8);
    if (!dcs) return Expected<L3TPSubmit>::error(dcs.error());
    msg.mDcs = static_cast<TPDCS>(dcs.value());

    // Skip optional TP-VP (validity period) based on VPF
    if (msg.mVpf == 0x01) {
        auto _ = br.readField(8); (void)_; // relative VP (1 octet)
    } else if (msg.mVpf == 0x02) {
        for (int i = 0; i < 7; ++i) { auto _ = br.readField(8); (void)_; } // encoded VP (7 octets)
    } else if (msg.mVpf == 0x03) {
        for (int i = 0; i < 10; ++i) { auto _ = br.readField(8); (void)_; } // enhanced VP (10 octets)
    }

    // TP-UDL
    auto udl = br.readField(8);
    if (!udl) return Expected<L3TPSubmit>::error(udl.error());
    size_t udLen = udl.value();

    // TP-UD: user data
    for (size_t i = 0; i < udLen; ++i) {
        auto ud = br.readField(8);
        if (!ud) return Expected<L3TPSubmit>::error(ud.error());
        msg.mUserData.push_back(static_cast<uint8_t>(ud.value()));
    }

    return Expected<L3TPSubmit>::hold(std::move(msg));
}

void L3TPSubmit::write(BitWriter& bw) const {
    uint8_t hdr = 0x40; // base MTI=01
    if (mRd) hdr |= 0x20;
    hdr |= (mVpf & 0x03) << 3;
    if (mSrr) hdr |= 0x04;
    if (mUdhi) hdr |= 0x02;
    if (mRp) hdr |= 0x01;
    bw.writeField(hdr, 8);

    bw.writeField(mMr, 8);
    mDestinationAddress.write(bw);
    bw.writeField(static_cast<uint8_t>(mPid), 8);
    bw.writeField(static_cast<uint8_t>(mDcs), 8);
    bw.writeField(static_cast<uint32_t>(mUserData.size()), 8);

    for (uint8_t b : mUserData) {
        bw.writeField(b, 8);
    }
}

void L3TPSubmit::text(std::ostream& os) const {
    os << "TP-Submit(da=";
    mDestinationAddress.text(os);
    os << ",pid=" << static_cast<int>(mPid) << ",dcs=" << static_cast<int>(mDcs)
       << ",udl=" << mUserData.size() << ")";
}

// ── L3TPStatusReport (23.040 9.2.2.3) ─────────────────────────────────

size_t L3TPStatusReport::bodyLength() const {
    size_t len = 1; // header octet
    len += 1; // TP-MR
    len += mDestinationAddress.totalLength(); // TP-DA
    len += 1; // TP-PID
    len += 1; // TP-DCS
    if (mScts) len += 7; // TP-SCTS
    len += 1; // TP-STS
    return len;
}

Expected<L3TPStatusReport> L3TPStatusReport::parse(BitReader& br) {
    L3TPStatusReport msg;

    auto hdr = br.readField(8);
    if (!hdr) return Expected<L3TPStatusReport>::error(hdr.error());

    // TP-MR
    auto mr = br.readField(8);
    if (!mr) return Expected<L3TPStatusReport>::error(mr.error());
    msg.mMr = static_cast<uint8_t>(mr.value());

    // TP-DA
    auto daResult = L3TPAddress::parse(br);
    if (!daResult) return Expected<L3TPStatusReport>::error(daResult.error());
    msg.mDestinationAddress = std::move(daResult.value());

    // TP-PID
    auto pid = br.readField(8);
    if (!pid) return Expected<L3TPStatusReport>::error(pid.error());
    msg.mPid = static_cast<TPPID>(pid.value());

    // TP-DCS
    auto dcs = br.readField(8);
    if (!dcs) return Expected<L3TPStatusReport>::error(dcs.error());
    msg.mDcs = static_cast<TPDCS>(dcs.value());

    // TP-SCTS (7 octets)
    auto sctsResult = TPSCTimeStamp::parse(br);
    if (!sctsResult) return Expected<L3TPStatusReport>::error(sctsResult.error());
    msg.mScts = std::move(sctsResult.value());

    // TP-STS
    auto sts = br.readField(8);
    if (!sts) return Expected<L3TPStatusReport>::error(sts.error());
    msg.mSts = static_cast<uint8_t>(sts.value());

    return Expected<L3TPStatusReport>::hold(std::move(msg));
}

void L3TPStatusReport::write(BitWriter& bw) const {
    bw.writeField(0x80, 8);
    bw.writeField(mMr, 8);
    mDestinationAddress.write(bw);
    bw.writeField(static_cast<uint8_t>(mPid), 8);
    bw.writeField(static_cast<uint8_t>(mDcs), 8);
    if (mScts) mScts->write(bw);
    bw.writeField(mSts, 8);
}

void L3TPStatusReport::text(std::ostream& os) const {
    os << "TP-StatusReport(mr=" << static_cast<int>(mMr) << ",sts=" << static_cast<int>(mSts) << ")";
}

// ── L3TPCommand (23.040 9.2.2.5) ──────────────────────────────────────

size_t L3TPCommand::bodyLength() const {
    size_t len = 1; // header octet
    len += 1; // TP-MR
    len += 1; // TP-PID
    len += 1; // TP-DCS
    len += 1; // TP-CMD
    if (mAddress) {
        len += mAddress->totalLength();
    }
    return len;
}

Expected<L3TPCommand> L3TPCommand::parse(BitReader& br) {
    L3TPCommand msg;

    auto hdr = br.readField(8);
    if (!hdr) return Expected<L3TPCommand>::error(hdr.error());

    // TP-MR
    auto mr = br.readField(8);
    if (!mr) return Expected<L3TPCommand>::error(mr.error());
    msg.mMr = static_cast<uint8_t>(mr.value());

    // TP-PID
    auto pid = br.readField(8);
    if (!pid) return Expected<L3TPCommand>::error(pid.error());
    msg.mPid = static_cast<TPPID>(pid.value());

    // TP-DCS
    auto dcs = br.readField(8);
    if (!dcs) return Expected<L3TPCommand>::error(dcs.error());
    msg.mDcs = static_cast<TPDCS>(dcs.value());

    // TP-CMD
    auto cmd = br.readField(8);
    if (!cmd) return Expected<L3TPCommand>::error(cmd.error());
    msg.mCmd = static_cast<uint8_t>(cmd.value());

    return Expected<L3TPCommand>::hold(std::move(msg));
}

void L3TPCommand::write(BitWriter& bw) const {
    bw.writeField(0xC0, 8);
    bw.writeField(mMr, 8);
    bw.writeField(static_cast<uint8_t>(mPid), 8);
    bw.writeField(static_cast<uint8_t>(mDcs), 8);
    bw.writeField(mCmd, 8);
    if (mAddress) mAddress->write(bw);
}

void L3TPCommand::text(std::ostream& os) const {
    os << "TP-Command(mr=" << static_cast<int>(mMr) << ",cmd=0x"
       << std::hex << static_cast<int>(mCmd) << ")";
}

} // namespace gsml3parser
