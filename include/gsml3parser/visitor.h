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

#pragma once

#include <string_view>
#include <type_traits>
#include <variant>
#include "message_types.h"

namespace gsml3parser {

namespace detail {

template<typename T, typename Variant>
constexpr bool is_variant_alternative_v = false;

template<typename T, typename... Types>
constexpr bool is_variant_alternative_v<T, std::variant<Types...>> =
    (std::is_same_v<T, Types> || ...);

} // namespace detail

// ── Compile-time typed access — no dynamic_cast needed. ────────────────

template<typename T>
[[nodiscard]] const T* tryGet(const RRM& v) {
    return std::get_if<T>(&v);
}

template<typename T>
[[nodiscard]] T* tryGet(RRM& v) {
    return std::get_if<T>(&v);
}

template<typename T>
[[nodiscard]] const T* tryGet(const MMM& v) {
    return std::get_if<T>(&v);
}

template<typename T>
[[nodiscard]] T* tryGet(MMM& v) {
    return std::get_if<T>(&v);
}

template<typename T>
[[nodiscard]] const T* tryGet(const CCM& v) {
    return std::get_if<T>(&v);
}

template<typename T>
[[nodiscard]] T* tryGet(CCM& v) {
    return std::get_if<T>(&v);
}

template<typename T>
[[nodiscard]] const T* tryGet(const SSM& v) {
    return std::get_if<T>(&v);
}

template<typename T>
[[nodiscard]] T* tryGet(SSM& v) {
    return std::get_if<T>(&v);
}

template<typename T>
[[nodiscard]] const T* tryGet(const GMM& v) {
    return std::get_if<T>(&v);
}

template<typename T>
[[nodiscard]] T* tryGet(GMM& v) {
    return std::get_if<T>(&v);
}

template<typename T>
[[nodiscard]] const T* tryGet(const SM& v) {
    return std::get_if<T>(&v);
}

template<typename T>
[[nodiscard]] T* tryGet(SM& v) {
    return std::get_if<T>(&v);
}

template<typename T>
[[nodiscard]] const T* tryGet(const SMS& v) {
    return std::get_if<T>(&v);
}

template<typename T>
[[nodiscard]] T* tryGet(SMS& v) {
    return std::get_if<T>(&v);
}

template<typename T>
[[nodiscard]] const T* tryGet(const BCCM& v) {
    return std::get_if<T>(&v);
}

template<typename T>
[[nodiscard]] T* tryGet(BCCM& v) {
    return std::get_if<T>(&v);
}

template<typename T>
[[nodiscard]] const T* tryGet(const GCCM& v) {
    return std::get_if<T>(&v);
}

template<typename T>
[[nodiscard]] T* tryGet(GCCM& v) {
    return std::get_if<T>(&v);
}

template<typename T>
[[nodiscard]] const T* tryGet(const LSM& v) {
    return std::get_if<T>(&v);
}

template<typename T>
[[nodiscard]] T* tryGet(LSM& v) {
    return std::get_if<T>(&v);
}

template<typename T>
[[nodiscard]] const T* tryGet(const EXTENDED& v) {
    return std::get_if<T>(&v);
}

template<typename T>
[[nodiscard]] T* tryGet(EXTENDED& v) {
    return std::get_if<T>(&v);
}

template<typename T>
[[nodiscard]] const T* tryGet(const TESTPROC& v) {
    return std::get_if<T>(&v);
}

template<typename T>
[[nodiscard]] T* tryGet(TESTPROC& v) {
    return std::get_if<T>(&v);
}

// Top-level: dispatch to domain, then tryGet within.
// Uses SFINAE to only instantiate get_if for the correct domain variant.
template<typename T>
[[nodiscard]] const T* tryGet(const ParsedMessage& msg) {
    if (const auto* rrm = std::get_if<RRM>(&msg)) {
        if constexpr (detail::is_variant_alternative_v<T, RRM>) {
            return tryGet<T>(*rrm);
        }
    }
    if (const auto* mmm = std::get_if<MMM>(&msg)) {
        if constexpr (detail::is_variant_alternative_v<T, MMM>) {
            return tryGet<T>(*mmm);
        }
    }
    if (const auto* ccm = std::get_if<CCM>(&msg)) {
        if constexpr (detail::is_variant_alternative_v<T, CCM>) {
            return tryGet<T>(*ccm);
        }
    }
    if (const auto* ssm = std::get_if<SSM>(&msg)) {
        if constexpr (detail::is_variant_alternative_v<T, SSM>) {
            return tryGet<T>(*ssm);
        }
    }
    if (const auto* gmm = std::get_if<GMM>(&msg)) {
        if constexpr (detail::is_variant_alternative_v<T, GMM>) {
            return tryGet<T>(*gmm);
        }
    }
    if (const auto* sm = std::get_if<SM>(&msg)) {
        if constexpr (detail::is_variant_alternative_v<T, SM>) {
            return tryGet<T>(*sm);
        }
    }
    if (const auto* sms = std::get_if<SMS>(&msg)) {
        if constexpr (detail::is_variant_alternative_v<T, SMS>) {
            return tryGet<T>(*sms);
        }
    }
    if (const auto* bccm = std::get_if<BCCM>(&msg)) {
        if constexpr (detail::is_variant_alternative_v<T, BCCM>) {
            return tryGet<T>(*bccm);
        }
    }
    if (const auto* gccm = std::get_if<GCCM>(&msg)) {
        if constexpr (detail::is_variant_alternative_v<T, GCCM>) {
            return tryGet<T>(*gccm);
        }
    }
    if (const auto* lsm = std::get_if<LSM>(&msg)) {
        if constexpr (detail::is_variant_alternative_v<T, LSM>) {
            return tryGet<T>(*lsm);
        }
    }
    if (const auto* ext = std::get_if<EXTENDED>(&msg)) {
        if constexpr (detail::is_variant_alternative_v<T, EXTENDED>) {
            return tryGet<T>(*ext);
        }
    }
    if (const auto* tp = std::get_if<TESTPROC>(&msg)) {
        if constexpr (detail::is_variant_alternative_v<T, TESTPROC>) {
            return tryGet<T>(*tp);
        }
    }
    return nullptr;
}

template<typename T>
[[nodiscard]] T* tryGet(ParsedMessage& msg) {
    if (auto* rrm = std::get_if<RRM>(&msg)) {
        if constexpr (detail::is_variant_alternative_v<T, RRM>) {
            return tryGet<T>(*rrm);
        }
    }
    if (auto* mmm = std::get_if<MMM>(&msg)) {
        if constexpr (detail::is_variant_alternative_v<T, MMM>) {
            return tryGet<T>(*mmm);
        }
    }
    if (auto* ccm = std::get_if<CCM>(&msg)) {
        if constexpr (detail::is_variant_alternative_v<T, CCM>) {
            return tryGet<T>(*ccm);
        }
    }
    if (auto* ssm = std::get_if<SSM>(&msg)) {
        if constexpr (detail::is_variant_alternative_v<T, SSM>) {
            return tryGet<T>(*ssm);
        }
    }
    if (auto* gmm = std::get_if<GMM>(&msg)) {
        if constexpr (detail::is_variant_alternative_v<T, GMM>) {
            return tryGet<T>(*gmm);
        }
    }
    if (auto* sm = std::get_if<SM>(&msg)) {
        if constexpr (detail::is_variant_alternative_v<T, SM>) {
            return tryGet<T>(*sm);
        }
    }
    if (auto* sms = std::get_if<SMS>(&msg)) {
        if constexpr (detail::is_variant_alternative_v<T, SMS>) {
            return tryGet<T>(*sms);
        }
    }
    if (auto* bccm = std::get_if<BCCM>(&msg)) {
        if constexpr (detail::is_variant_alternative_v<T, BCCM>) {
            return tryGet<T>(*bccm);
        }
    }
    if (auto* gccm = std::get_if<GCCM>(&msg)) {
        if constexpr (detail::is_variant_alternative_v<T, GCCM>) {
            return tryGet<T>(*gccm);
        }
    }
    if (auto* lsm = std::get_if<LSM>(&msg)) {
        if constexpr (detail::is_variant_alternative_v<T, LSM>) {
            return tryGet<T>(*lsm);
        }
    }
    if (auto* ext = std::get_if<EXTENDED>(&msg)) {
        if constexpr (detail::is_variant_alternative_v<T, EXTENDED>) {
            return tryGet<T>(*ext);
        }
    }
    if (auto* tp = std::get_if<TESTPROC>(&msg)) {
        if constexpr (detail::is_variant_alternative_v<T, TESTPROC>) {
            return tryGet<T>(*tp);
        }
    }
    return nullptr;
}

// ── Message metadata from variant (no RTTI). ───────────────────────────

[[nodiscard]] std::string_view messageName(const ParsedMessage& msg);
[[nodiscard]] L3PD messagePD(const ParsedMessage& msg);
[[nodiscard]] int messageMTI(const ParsedMessage& msg);

/// Extract Transaction Identifier (TI) from CC/SS messages.
/// Returns 0 for non-CC/non-SS message types (TI is not applicable).
[[nodiscard]] uint8_t messageTI(const ParsedMessage& msg);

} // namespace gsml3parser
