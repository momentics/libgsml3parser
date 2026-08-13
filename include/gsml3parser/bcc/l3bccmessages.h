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

// BCC (Broadcast Call Control) Message Classes — GSM L3 broadcast call messages
// Spec: 3GPP TS 44.018 sections 9.6, Table 10.4.3
// Reference: ref/osmo-ttcn3-hacks/library/L3_Templates.ttcn — ts_ML3_MO_BCC (line 3813)
//            ETSI TS 102 225 (TETRA BCC), 3GPP TS 44.018 for GSM broadcast calls
//
// L3 header (per 44.018 10.2, PD=0x01):
//   Byte 0: PD(4)=0x01(BCC) | TI(3) | TIF(1)
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

// ── Broadcast Call Setup (GSM 44.018 9.6.2.2) ─────────────────────────
// MO: CalledPartyNumber(conditional) | BearerCapability(conditional) | ...
// Basic infrastructure: body stored as opaque octet sequence.

class L3BCCSetup {
    std::vector<uint8_t> mBody;
    unsigned mTi{0};

    friend struct Builder;
public:
    static constexpr int MTI = 0x00;

    void ti(unsigned t) { mTi = t; }
    unsigned ti() const { return mTi; }
    const std::vector<uint8_t>& body() const { return mBody; }

    size_t bodyLength() const { return mBody.size(); }
    [[nodiscard]] static Expected<L3BCCSetup> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::BroadcastCallControl; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }

    struct Builder {
        std::vector<uint8_t> mBody;
        unsigned mTi{0};

        /// Set transaction identifier.
        Builder& ti(unsigned v) { mTi = v; return *this; }
        /// Set body data.
        Builder& body(std::vector<uint8_t> v) { mBody = std::move(v); return *this; }
        /// Build the final message.
        [[nodiscard]] L3BCCSetup build() const {
            L3BCCSetup msg;
            msg.mTi = mTi;
            msg.mBody = mBody;
            return msg;
        }
    };

    static Builder builder() { return Builder{}; }
};

// ── Broadcast Call Proceeding (GSM 44.018 9.6.2.3) ────────────────────
// MT: optional Cause IE

class L3BCCProceeding {
    std::vector<uint8_t> mBody;
    unsigned mTi{0};

    friend struct Builder;
public:
    static constexpr int MTI = 0x01;

    void ti(unsigned t) { mTi = t; }
    unsigned ti() const { return mTi; }
    const std::vector<uint8_t>& body() const { return mBody; }

    size_t bodyLength() const { return mBody.size(); }
    [[nodiscard]] static Expected<L3BCCProceeding> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::BroadcastCallControl; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }

    struct Builder {
        std::vector<uint8_t> mBody;
        unsigned mTi{0};

        /// Set transaction identifier.
        Builder& ti(unsigned v) { mTi = v; return *this; }
        /// Set body data.
        Builder& body(std::vector<uint8_t> v) { mBody = std::move(v); return *this; }
        /// Build the final message.
        [[nodiscard]] L3BCCProceeding build() const {
            L3BCCProceeding msg;
            msg.mTi = mTi;
            msg.mBody = mBody;
            return msg;
        }
    };

    static Builder builder() { return Builder{}; }
};

// ── Broadcast Call Connect (GSM 44.018 9.6.2.6) ───────────────────────
// MT: forwardsConnect(1)|spare(7) | optional IEs

class L3BCCConnect {
    std::vector<uint8_t> mBody;
    unsigned mTi{0};

    friend struct Builder;
public:
    static constexpr int MTI = 0x05;

    void ti(unsigned t) { mTi = t; }
    unsigned ti() const { return mTi; }
    const std::vector<uint8_t>& body() const { return mBody; }

    size_t bodyLength() const { return mBody.size(); }
    [[nodiscard]] static Expected<L3BCCConnect> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::BroadcastCallControl; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }

    struct Builder {
        std::vector<uint8_t> mBody;
        unsigned mTi{0};

        /// Set transaction identifier.
        Builder& ti(unsigned v) { mTi = v; return *this; }
        /// Set body data.
        Builder& body(std::vector<uint8_t> v) { mBody = std::move(v); return *this; }
        /// Build the final message.
        [[nodiscard]] L3BCCConnect build() const {
            L3BCCConnect msg;
            msg.mTi = mTi;
            msg.mBody = mBody;
            return msg;
        }
    };

    static Builder builder() { return Builder{}; }
};

// ── Broadcast Call Disconnect (GSM 44.018 9.6.2.7) ────────────────────
// MO: Cause(conditional) | optional IEs

class L3BCCDisconnect {
    std::vector<uint8_t> mBody;
    unsigned mTi{0};

    friend struct Builder;
public:
    static constexpr int MTI = 0x06;

    void ti(unsigned t) { mTi = t; }
    unsigned ti() const { return mTi; }
    const std::vector<uint8_t>& body() const { return mBody; }

    size_t bodyLength() const { return mBody.size(); }
    [[nodiscard]] static Expected<L3BCCDisconnect> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::BroadcastCallControl; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }

    struct Builder {
        std::vector<uint8_t> mBody;
        unsigned mTi{0};

        /// Set transaction identifier.
        Builder& ti(unsigned v) { mTi = v; return *this; }
        /// Set body data.
        Builder& body(std::vector<uint8_t> v) { mBody = std::move(v); return *this; }
        /// Build the final message.
        [[nodiscard]] L3BCCDisconnect build() const {
            L3BCCDisconnect msg;
            msg.mTi = mTi;
            msg.mBody = mBody;
            return msg;
        }
    };

    static Builder builder() { return Builder{}; }
};

// ── Broadcast Call Release (GSM 44.018 9.6.2.8) ───────────────────────
// MT: Cause(conditional) | optional IEs

class L3BCCRelease {
    std::vector<uint8_t> mBody;
    unsigned mTi{0};

    friend struct Builder;
public:
    static constexpr int MTI = 0x07;

    void ti(unsigned t) { mTi = t; }
    unsigned ti() const { return mTi; }
    const std::vector<uint8_t>& body() const { return mBody; }

    size_t bodyLength() const { return mBody.size(); }
    [[nodiscard]] static Expected<L3BCCRelease> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::BroadcastCallControl; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }

    struct Builder {
        std::vector<uint8_t> mBody;
        unsigned mTi{0};

        /// Set transaction identifier.
        Builder& ti(unsigned v) { mTi = v; return *this; }
        /// Set body data.
        Builder& body(std::vector<uint8_t> v) { mBody = std::move(v); return *this; }
        /// Build the final message.
        [[nodiscard]] L3BCCRelease build() const {
            L3BCCRelease msg;
            msg.mTi = mTi;
            msg.mBody = mBody;
            return msg;
        }
    };

    static Builder builder() { return Builder{}; }
};

// ── Broadcast Call Release Complete (GSM 44.018 9.6.2.9) ──────────────
// Bidirectional: minimal body, optional IEs

class L3BCCReleaseComplete {
    std::vector<uint8_t> mBody;
    unsigned mTi{0};

    friend struct Builder;
public:
    static constexpr int MTI = 0x0a;

    void ti(unsigned t) { mTi = t; }
    unsigned ti() const { return mTi; }
    const std::vector<uint8_t>& body() const { return mBody; }

    size_t bodyLength() const { return mBody.size(); }
    [[nodiscard]] static Expected<L3BCCReleaseComplete> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::BroadcastCallControl; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }

    struct Builder {
        std::vector<uint8_t> mBody;
        unsigned mTi{0};

        /// Set transaction identifier.
        Builder& ti(unsigned v) { mTi = v; return *this; }
        /// Set body data.
        Builder& body(std::vector<uint8_t> v) { mBody = std::move(v); return *this; }
        /// Build the final message.
        [[nodiscard]] L3BCCReleaseComplete build() const {
            L3BCCReleaseComplete msg;
            msg.mTi = mTi;
            msg.mBody = mBody;
            return msg;
        }
    };

    static Builder builder() { return Builder{}; }
};

// BCC Call Confirmed — TS 44.018 §9.6.2.5, Table 10.4.3
// Direction: MT
class L3BCCCallConfirmed {
    unsigned mTi{0};

    friend struct Builder;
public:
    static constexpr int MTI = 0x04;
    void ti(unsigned t) { mTi = t; }
    unsigned ti() const { return mTi; }
    size_t bodyLength() const { return 0; }
    [[nodiscard]] static Expected<L3BCCCallConfirmed> parse(BitReader&);
    void write(BitWriter&) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::BroadcastCallControl; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }

    struct Builder {
        unsigned mTi{0};

        /// Set transaction identifier.
        Builder& ti(unsigned v) { mTi = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3BCCCallConfirmed build() const {
            L3BCCCallConfirmed msg;
            msg.mTi = mTi;
            return msg;
        }
    };

    static Builder builder() { return Builder{}; }
};

// BCC Connect Acknowledge — TS 44.018 §9.6.2.10, Table 10.4.3
// Direction: MT
class L3BCCConnectAcknowledge {
    unsigned mTi{0};

    friend struct Builder;
public:
    static constexpr int MTI = 0x09;
    void ti(unsigned t) { mTi = t; }
    unsigned ti() const { return mTi; }
    size_t bodyLength() const { return 0; }
    [[nodiscard]] static Expected<L3BCCConnectAcknowledge> parse(BitReader&);
    void write(BitWriter&) const;
    void text(std::ostream& os) const;
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::BroadcastCallControl; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }

    struct Builder {
        unsigned mTi{0};

        /// Set transaction identifier.
        Builder& ti(unsigned v) { mTi = v; return *this; }
        /// Build the final message.
        [[nodiscard]] L3BCCConnectAcknowledge build() const {
            L3BCCConnectAcknowledge msg;
            msg.mTi = mTi;
            return msg;
        }
    };

    static Builder builder() { return Builder{}; }
};

} // namespace gsml3parser
