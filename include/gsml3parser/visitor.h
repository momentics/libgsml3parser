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
    return nullptr;
}

// ── Message metadata from variant (no RTTI). ───────────────────────────

[[nodiscard]] std::string_view messageName(const ParsedMessage& msg);
[[nodiscard]] L3PD messagePD(const ParsedMessage& msg);
[[nodiscard]] int messageMTI(const ParsedMessage& msg);

} // namespace gsml3parser
