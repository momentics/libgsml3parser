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

// GCC Messages - parse/write/text implementation
// Spec: 3GPP TS 44.018 sections 9.7, Table 10.4.4
// Reference: ref/osmo-ttcn3-hacks/library/L3_Templates.ttcn - ts_ML3_MO_GCC (line 3840)

#include "gsml3parser/gcc/l3gccmessages.h"
#include <sstream>
#include <iomanip>

namespace gsml3parser {

// Helper: read remaining body bytes from BitReader into a vector.
static std::vector<uint8_t> readBodyBytes(BitReader& br) {
    std::vector<uint8_t> body;
    while (br.hasMore()) {
        auto val = br.readField(8);
        if (!val) break;
        body.push_back(static_cast<uint8_t>(val.value()));
    }
    return body;
}

// ── L3GCCSetup (GSM 44.018 9.7.2.2) ──────────────────────────────────

Expected<L3GCCSetup> L3GCCSetup::parse(BitReader& br) {
    L3GCCSetup msg;
    // Read remaining body as opaque IEs (basic infrastructure).
    // GCC Setup body contains optional IEs like CalledPartyNumber, BearerCapability, etc.
    msg.mBody = readBodyBytes(br);
    return Expected<L3GCCSetup>::hold(std::move(msg));
}

void L3GCCSetup::write(BitWriter& bw) const {
    if (!mBody.empty()) {
        bw.writeBytes(mBody.data(), mBody.size());
    }
}

void L3GCCSetup::text(std::ostream& os) const {
    os << "GCCSetup(ti=" << mTi << ")";
    if (!mBody.empty()) {
        os << " [body=" << mBody.size() << " octets]";
    }
}

// ── L3GCCAcknowledge (GSM 44.018 9.7.2.3) ────────────────────────────

Expected<L3GCCAcknowledge> L3GCCAcknowledge::parse(BitReader& br) {
    L3GCCAcknowledge msg;
    msg.mBody = readBodyBytes(br);
    return Expected<L3GCCAcknowledge>::hold(std::move(msg));
}

void L3GCCAcknowledge::write(BitWriter& bw) const {
    if (!mBody.empty()) {
        bw.writeBytes(mBody.data(), mBody.size());
    }
}

void L3GCCAcknowledge::text(std::ostream& os) const {
    os << "GCCAcknowledge(ti=" << mTi << ")";
    if (!mBody.empty()) {
        os << " [body=" << mBody.size() << " octets]";
    }
}

// ── L3GCCProceeding (GSM 44.018 9.7.2.4) ─────────────────────────────

Expected<L3GCCProceeding> L3GCCProceeding::parse(BitReader& br) {
    L3GCCProceeding msg;
    msg.mBody = readBodyBytes(br);
    return Expected<L3GCCProceeding>::hold(std::move(msg));
}

void L3GCCProceeding::write(BitWriter& bw) const {
    if (!mBody.empty()) {
        bw.writeBytes(mBody.data(), mBody.size());
    }
}

void L3GCCProceeding::text(std::ostream& os) const {
    os << "GCCProceeding(ti=" << mTi << ")";
    if (!mBody.empty()) {
        os << " [body=" << mBody.size() << " octets]";
    }
}

// ── L3GCCConnect (GSM 44.018 9.7.2.6) ────────────────────────────────

Expected<L3GCCConnect> L3GCCConnect::parse(BitReader& br) {
    L3GCCConnect msg;
    msg.mBody = readBodyBytes(br);
    return Expected<L3GCCConnect>::hold(std::move(msg));
}

void L3GCCConnect::write(BitWriter& bw) const {
    if (!mBody.empty()) {
        bw.writeBytes(mBody.data(), mBody.size());
    }
}

void L3GCCConnect::text(std::ostream& os) const {
    os << "GCCConnect(ti=" << mTi << ")";
    if (!mBody.empty()) {
        os << " [body=" << mBody.size() << " octets]";
    }
}

// ── L3GCCDisconnect (GSM 44.018 9.7.2.7) ─────────────────────────────

Expected<L3GCCDisconnect> L3GCCDisconnect::parse(BitReader& br) {
    L3GCCDisconnect msg;
    msg.mBody = readBodyBytes(br);
    return Expected<L3GCCDisconnect>::hold(std::move(msg));
}

void L3GCCDisconnect::write(BitWriter& bw) const {
    if (!mBody.empty()) {
        bw.writeBytes(mBody.data(), mBody.size());
    }
}

void L3GCCDisconnect::text(std::ostream& os) const {
    os << "GCCDisconnect(ti=" << mTi << ")";
    if (!mBody.empty()) {
        os << " [body=" << mBody.size() << " octets]";
    }
}

// ── L3GCCRelease (GSM 44.018 9.7.2.8) ────────────────────────────────

Expected<L3GCCRelease> L3GCCRelease::parse(BitReader& br) {
    L3GCCRelease msg;
    msg.mBody = readBodyBytes(br);
    return Expected<L3GCCRelease>::hold(std::move(msg));
}

void L3GCCRelease::write(BitWriter& bw) const {
    if (!mBody.empty()) {
        bw.writeBytes(mBody.data(), mBody.size());
    }
}

void L3GCCRelease::text(std::ostream& os) const {
    os << "GCCRelease(ti=" << mTi << ")";
    if (!mBody.empty()) {
        os << " [body=" << mBody.size() << " octets]";
    }
}

// ── L3GCCReleaseComplete (GSM 44.018 9.7.2.9) ────────────────────────

Expected<L3GCCReleaseComplete> L3GCCReleaseComplete::parse(BitReader& br) {
    L3GCCReleaseComplete msg;
    msg.mBody = readBodyBytes(br);
    return Expected<L3GCCReleaseComplete>::hold(std::move(msg));
}

void L3GCCReleaseComplete::write(BitWriter& bw) const {
    if (!mBody.empty()) {
        bw.writeBytes(mBody.data(), mBody.size());
    }
}

void L3GCCReleaseComplete::text(std::ostream& os) const {
    os << "GCCReleaseComplete(ti=" << mTi << ")";
    if (!mBody.empty()) {
        os << " [body=" << mBody.size() << " octets]";
    }
}

// ── L3GCCCallConfirmed (TS 44.018 §9.7.2.5, MTI=0x03) ────────────────

Expected<L3GCCCallConfirmed> L3GCCCallConfirmed::parse(BitReader&) {
    return Expected<L3GCCCallConfirmed>::hold(L3GCCCallConfirmed{});
}

void L3GCCCallConfirmed::write(BitWriter&) const {}

void L3GCCCallConfirmed::text(std::ostream& os) const {
    os << "GCCCallConfirmed(ti=" << mTi << ")";
}

} // namespace gsml3parser
