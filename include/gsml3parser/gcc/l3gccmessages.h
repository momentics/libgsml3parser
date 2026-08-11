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

// GCC (Group Call Control) Message Classes — GSM L3 group call messages
// Spec: 3GPP TS 44.018 sections 9.7, Table 10.4.4
// Reference: ref/osmo-ttcn3-hacks/library/L3_Templates.ttcn — ts_ML3_MO_GCC (line 3840)
//            ETSI TS 102 225 (TETRA GCC), 3GPP TS 44.018 for GSM group calls
//
// L3 header (per 44.018 10.2, PD=0x00):
//   Byte 0: PD(4)=0x00(GCC) | TI(3) | TIF(1)
//   Byte 1: MessageType(6)<<2 | NSD(2)
//   Body: [message-specific IE fields, parsed as opaque for basic infrastructure]

#pragma once

#include <cstdint>
#include <ostream>
#include <vector>

#include "../expected.h"
#include "../bitreader.h"
#include "../bitwriter.h"
#include "../types.h"

namespace gsml3parser {

// ── Group Call Setup (GSM 44.018 9.7.2.2) ────────────────────────────
// MO: CalledPartyNumber(conditional) | BearerCapability(conditional) | ...
// Basic infrastructure: body stored as opaque octet sequence.

class L3GCCSetup {
    std::vector<uint8_t> mBody;
    unsigned mTi{0};
public:
    static constexpr int MTI = 0x00;

    void ti(unsigned t) { mTi = t; }
    unsigned ti() const { return mTi; }
    const std::vector<uint8_t>& body() const { return mBody; }

    size_t bodyLength() const { return mBody.size(); }
    [[nodiscard]] static Expected<L3GCCSetup> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GroupCallControl; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Group Call Acknowledge (GSM 44.018 9.7.2.3) ──────────────────────
// MT: minimal body

class L3GCCAcknowledge {
    std::vector<uint8_t> mBody;
    unsigned mTi{0};
public:
    static constexpr int MTI = 0x02;

    void ti(unsigned t) { mTi = t; }
    unsigned ti() const { return mTi; }
    const std::vector<uint8_t>& body() const { return mBody; }

    size_t bodyLength() const { return mBody.size(); }
    [[nodiscard]] static Expected<L3GCCAcknowledge> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GroupCallControl; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Group Call Proceeding (GSM 44.018 9.7.2.4) ───────────────────────
// MT: optional Cause IE

class L3GCCProceeding {
    std::vector<uint8_t> mBody;
    unsigned mTi{0};
public:
    static constexpr int MTI = 0x01;

    void ti(unsigned t) { mTi = t; }
    unsigned ti() const { return mTi; }
    const std::vector<uint8_t>& body() const { return mBody; }

    size_t bodyLength() const { return mBody.size(); }
    [[nodiscard]] static Expected<L3GCCProceeding> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GroupCallControl; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Group Call Connect (GSM 44.018 9.7.2.6) ──────────────────────────
// MT: forwardsConnect(1)|spare(7) | optional IEs

class L3GCCConnect {
    std::vector<uint8_t> mBody;
    unsigned mTi{0};
public:
    static constexpr int MTI = 0x05;

    void ti(unsigned t) { mTi = t; }
    unsigned ti() const { return mTi; }
    const std::vector<uint8_t>& body() const { return mBody; }

    size_t bodyLength() const { return mBody.size(); }
    [[nodiscard]] static Expected<L3GCCConnect> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GroupCallControl; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Group Call Disconnect (GSM 44.018 9.7.2.7) ───────────────────────
// MO: Cause(conditional) | optional IEs

class L3GCCDisconnect {
    std::vector<uint8_t> mBody;
    unsigned mTi{0};
public:
    static constexpr int MTI = 0x06;

    void ti(unsigned t) { mTi = t; }
    unsigned ti() const { return mTi; }
    const std::vector<uint8_t>& body() const { return mBody; }

    size_t bodyLength() const { return mBody.size(); }
    [[nodiscard]] static Expected<L3GCCDisconnect> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GroupCallControl; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Group Call Release (GSM 44.018 9.7.2.8) ──────────────────────────
// MT: Cause(conditional) | optional IEs

class L3GCCRelease {
    std::vector<uint8_t> mBody;
    unsigned mTi{0};
public:
    static constexpr int MTI = 0x07;

    void ti(unsigned t) { mTi = t; }
    unsigned ti() const { return mTi; }
    const std::vector<uint8_t>& body() const { return mBody; }

    size_t bodyLength() const { return mBody.size(); }
    [[nodiscard]] static Expected<L3GCCRelease> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GroupCallControl; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// ── Group Call Release Complete (GSM 44.018 9.7.2.9) ─────────────────
// Bidirectional: minimal body, optional IEs

class L3GCCReleaseComplete {
    std::vector<uint8_t> mBody;
    unsigned mTi{0};
public:
    static constexpr int MTI = 0x0a;

    void ti(unsigned t) { mTi = t; }
    unsigned ti() const { return mTi; }
    const std::vector<uint8_t>& body() const { return mBody; }

    size_t bodyLength() const { return mBody.size(); }
    [[nodiscard]] static Expected<L3GCCReleaseComplete> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GroupCallControl; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

// GCC Call Confirmed — TS 44.018 §9.7.2.5, Table 10.4.4
// Direction: MT
class L3GCCCallConfirmed {
    unsigned mTi{0};
public:
    static constexpr int MTI = 0x03;
    void ti(unsigned t) { mTi = t; }
    unsigned ti() const { return mTi; }
    size_t bodyLength() const { return 0; }
    [[nodiscard]] static Expected<L3GCCCallConfirmed> parse(BitReader&);
    void write(BitWriter&) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::GroupCallControl; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
};

} // namespace gsml3parser
