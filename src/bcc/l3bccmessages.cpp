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

// BCC Messages - parse/write/text implementation
// Spec: 3GPP TS 44.018 sections 9.6, Table 10.4.3
// Reference: ref/osmo-ttcn3-hacks/library/L3_Templates.ttcn - ts_ML3_MO_BCC (line 3813)

#include "gsml3parser/bcc/l3bccmessages.h"
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

// ── L3BCCSetup (GSM 44.018 9.6.2.2) ──────────────────────────────────

Expected<L3BCCSetup> L3BCCSetup::parse(BitReader& br) {
    L3BCCSetup msg;
    // Read remaining body as opaque IEs (basic infrastructure).
    // BCC Setup body contains optional IEs like CalledPartyNumber, BearerCapability, etc.
    msg.mBody = readBodyBytes(br);
    return Expected<L3BCCSetup>::hold(std::move(msg));
}

void L3BCCSetup::write(BitWriter& bw) const {
    if (!mBody.empty()) {
        bw.writeBytes(mBody.data(), mBody.size());
    }
}

void L3BCCSetup::text(std::ostream& os) const {
    os << "BCCSetup(ti=" << mTi << ")";
    if (!mBody.empty()) {
        os << " [body=" << mBody.size() << " octets]";
    }
}

// ── L3BCCProceeding (GSM 44.018 9.6.2.3) ─────────────────────────────

Expected<L3BCCProceeding> L3BCCProceeding::parse(BitReader& br) {
    L3BCCProceeding msg;
    msg.mBody = readBodyBytes(br);
    return Expected<L3BCCProceeding>::hold(std::move(msg));
}

void L3BCCProceeding::write(BitWriter& bw) const {
    if (!mBody.empty()) {
        bw.writeBytes(mBody.data(), mBody.size());
    }
}

void L3BCCProceeding::text(std::ostream& os) const {
    os << "BCCProceeding(ti=" << mTi << ")";
    if (!mBody.empty()) {
        os << " [body=" << mBody.size() << " octets]";
    }
}

// ── L3BCCConnect (GSM 44.018 9.6.2.6) ────────────────────────────────

Expected<L3BCCConnect> L3BCCConnect::parse(BitReader& br) {
    L3BCCConnect msg;
    msg.mBody = readBodyBytes(br);
    return Expected<L3BCCConnect>::hold(std::move(msg));
}

void L3BCCConnect::write(BitWriter& bw) const {
    if (!mBody.empty()) {
        bw.writeBytes(mBody.data(), mBody.size());
    }
}

void L3BCCConnect::text(std::ostream& os) const {
    os << "BCCConnect(ti=" << mTi << ")";
    if (!mBody.empty()) {
        os << " [body=" << mBody.size() << " octets]";
    }
}

// ── L3BCCDisconnect (GSM 44.018 9.6.2.7) ─────────────────────────────

Expected<L3BCCDisconnect> L3BCCDisconnect::parse(BitReader& br) {
    L3BCCDisconnect msg;
    msg.mBody = readBodyBytes(br);
    return Expected<L3BCCDisconnect>::hold(std::move(msg));
}

void L3BCCDisconnect::write(BitWriter& bw) const {
    if (!mBody.empty()) {
        bw.writeBytes(mBody.data(), mBody.size());
    }
}

void L3BCCDisconnect::text(std::ostream& os) const {
    os << "BCCDisconnect(ti=" << mTi << ")";
    if (!mBody.empty()) {
        os << " [body=" << mBody.size() << " octets]";
    }
}

// ── L3BCCRelease (GSM 44.018 9.6.2.8) ────────────────────────────────

Expected<L3BCCRelease> L3BCCRelease::parse(BitReader& br) {
    L3BCCRelease msg;
    msg.mBody = readBodyBytes(br);
    return Expected<L3BCCRelease>::hold(std::move(msg));
}

void L3BCCRelease::write(BitWriter& bw) const {
    if (!mBody.empty()) {
        bw.writeBytes(mBody.data(), mBody.size());
    }
}

void L3BCCRelease::text(std::ostream& os) const {
    os << "BCCRelease(ti=" << mTi << ")";
    if (!mBody.empty()) {
        os << " [body=" << mBody.size() << " octets]";
    }
}

// ── L3BCCReleaseComplete (GSM 44.018 9.6.2.9) ────────────────────────

Expected<L3BCCReleaseComplete> L3BCCReleaseComplete::parse(BitReader& br) {
    L3BCCReleaseComplete msg;
    msg.mBody = readBodyBytes(br);
    return Expected<L3BCCReleaseComplete>::hold(std::move(msg));
}

void L3BCCReleaseComplete::write(BitWriter& bw) const {
    if (!mBody.empty()) {
        bw.writeBytes(mBody.data(), mBody.size());
    }
}

void L3BCCReleaseComplete::text(std::ostream& os) const {
    os << "BCCReleaseComplete(ti=" << mTi << ")";
    if (!mBody.empty()) {
        os << " [body=" << mBody.size() << " octets]";
    }
}

// ── L3BCCCallConfirmed (TS 44.018 §9.6.2.5, MTI=0x04) ────────────────

Expected<L3BCCCallConfirmed> L3BCCCallConfirmed::parse(BitReader&) {
    return Expected<L3BCCCallConfirmed>::hold(L3BCCCallConfirmed{});
}

void L3BCCCallConfirmed::write(BitWriter&) const {}

void L3BCCCallConfirmed::text(std::ostream& os) const {
    os << "BCCCallConfirmed(ti=" << mTi << ")";
}

// ── L3BCCConnectAcknowledge (TS 44.018 §9.6.2.10, MTI=0x09) ──────────

Expected<L3BCCConnectAcknowledge> L3BCCConnectAcknowledge::parse(BitReader&) {
    return Expected<L3BCCConnectAcknowledge>::hold(L3BCCConnectAcknowledge{});
}

void L3BCCConnectAcknowledge::write(BitWriter&) const {}

void L3BCCConnectAcknowledge::text(std::ostream& os) const {
    os << "BCCConnectAcknowledge(ti=" << mTi << ")";
}

} // namespace gsml3parser
