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

// Location Services (LS) Message Classes - 3GPP TS 44.031 / TS 24.027/24.028
// PD=0x0c, used for mobile location services in GSM networks.

#pragma once

#include <cstdint>
#include <ostream>
#include <vector>

#include "../expected.h"
#include "../bitreader.h"
#include "../bitwriter.h"
#include "../types.h"

namespace gsml3parser {

// Location Service Request - TS 44.031 §9.1.2
// Direction: Both
// Carries: location service request parameters
class L3LocationServiceRequest {
    std::vector<uint8_t> mBody;

    friend struct Builder;
public:
    static constexpr int MTI = 0x01;
    L3LocationServiceRequest() = default;
    const std::vector<uint8_t>& body() const { return mBody; }
    size_t bodyLength() const { return mBody.size(); }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::Location; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
    [[nodiscard]] static Expected<L3LocationServiceRequest> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;

    struct Builder {
        std::vector<uint8_t> mBody;

        /// Set body data.
        Builder& body(std::vector<uint8_t> v) { mBody = std::move(v); return *this; }
        /// Build the final message.
        [[nodiscard]] L3LocationServiceRequest build() const {
            L3LocationServiceRequest msg;
            msg.mBody = mBody;
            return msg;
        }
    };

    static Builder builder() { return Builder{}; }
};

// Location Service Provider Message - TS 44.031 §9.1.3
// Direction: Both
// Carries: location service provider data
class L3LocationServiceProviderMessage {
    std::vector<uint8_t> mBody;

    friend struct Builder;
public:
    static constexpr int MTI = 0x02;
    L3LocationServiceProviderMessage() = default;
    const std::vector<uint8_t>& body() const { return mBody; }
    size_t bodyLength() const { return mBody.size(); }
    [[nodiscard]] int mti() const { return MTI; }
    [[nodiscard]] L3PD pd() const { return L3PD::Location; }
    [[nodiscard]] size_t l2BodyLength() const { return bodyLength(); }
    [[nodiscard]] static Expected<L3LocationServiceProviderMessage> parse(BitReader& br);
    void write(BitWriter& bw) const;
    void text(std::ostream& os) const;

    struct Builder {
        std::vector<uint8_t> mBody;

        /// Set body data.
        Builder& body(std::vector<uint8_t> v) { mBody = std::move(v); return *this; }
        /// Build the final message.
        [[nodiscard]] L3LocationServiceProviderMessage build() const {
            L3LocationServiceProviderMessage msg;
            msg.mBody = mBody;
            return msg;
        }
    };

    static Builder builder() { return Builder{}; }
};

} // namespace gsml3parser
