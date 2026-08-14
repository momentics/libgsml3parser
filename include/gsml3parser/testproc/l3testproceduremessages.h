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

// Test Procedure PD (PD=0x0f) Message Classes - GSM 04.08 §10.2
// Placeholder for test procedure messages used in network testing.
// MTI is determined at parse time from the L3 header.

#pragma once

#include <cstdint>
#include <ostream>
#include <vector>

#include "../expected.h"
#include "../bitreader.h"
#include "../bitwriter.h"
#include "../types.h"

namespace gsml3parser {

// ── TestProcedure Message (GSM 04.08 PD=0x0f) ────────────────────────
// Direction: Bidirectional
// Body: raw octets (MTI determined at parse time)
class L3TestProcedureMessage {
    uint8_t mMti{};
    std::vector<uint8_t> mBody;

    friend struct Builder;
public:
    static constexpr int MTI = 0; // placeholder; actual MTI set at parse time

    L3TestProcedureMessage() = default;
    explicit L3TestProcedureMessage(uint8_t mti) : mMti(mti) {}

    uint8_t mti() const { return mMti; }
    L3PD pd() const { return L3PD::TestProcedure; }
    const std::vector<uint8_t>& body() const { return mBody; }
    size_t bodyLength() const { return mBody.size(); }
    size_t l2BodyLength() const { return bodyLength(); }

    [[nodiscard]] static Expected<L3TestProcedureMessage> parse(BitReader& br, uint8_t parsedMti);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;

    struct Builder {
        uint8_t mMti{};
        std::vector<uint8_t> mBody;

        /// Set message type indicator.
        Builder& mti(uint8_t v) { mMti = v; return *this; }
        /// Set body data.
        Builder& body(std::vector<uint8_t> v) { mBody = std::move(v); return *this; }
        /// Build the final message.
        [[nodiscard]] L3TestProcedureMessage build() const {
            L3TestProcedureMessage msg;
            msg.mMti = mMti;
            msg.mBody = mBody;
            return msg;
        }
    };

    static Builder builder() { return Builder{}; }
};

} // namespace gsml3parser
