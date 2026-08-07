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

#include <array>
#include <cstddef>
#include <string_view>
#include <utility>
#include <variant>

namespace gsml3parser {

struct ParseError {
    enum class Code : uint8_t {
        Ok,
        TruncatedInput,
        InvalidPD,
        InvalidMTI,
        LengthMismatch,
        InvalidIE,
        InvalidValue,
        UnsupportedFeature,
    };

    // Inline buffer for small-string optimization — no heap allocation.
    static constexpr std::size_t InlineCapacity = 47;

    Code code{Code::Ok};
    size_t bitPosition{};
    std::string_view message{};

    // Storage for short messages that fit inline.
    std::array<char, InlineCapacity + 1> mInline{};
    std::size_t mInlineLen{};

    constexpr ParseError() = default;

    constexpr ParseError(Code code, std::string_view msg, size_t bitPos = 0)
        : code(code), bitPosition(bitPos)
    {
        if (msg.size() <= InlineCapacity) {
            std::copy(msg.begin(), msg.end(), mInline.begin());
            mInline[msg.size()] = '\0';
            mInlineLen = msg.size();
            message = std::string_view{mInline.data(), mInlineLen};
        } else {
            message = msg;
            mInlineLen = 0;
        }
    }

    [[nodiscard]] constexpr bool failed() const noexcept {
        return code != Code::Ok;
    }
};

// ── Expected<T, ParseError> (general) ────────────────────────────────────

template <typename T>
class Expected {
public:
    // Static factory for success values.
    [[nodiscard]] static constexpr Expected hold(T value) noexcept {
        return Expected{std::in_place_type_t<T>{}, std::move(value)};
    }

    // Accept convertible types (e.g. unique_ptr<Derived> -> Expected<unique_ptr<Base>>)
    template <typename U>
    [[nodiscard]] static constexpr auto hold(U&& value) -> Expected
        requires(std::is_convertible_v<U&&, T> && !std::is_same_v<std::decay_t<U>, T>)
    {
        return Expected{std::in_place_type_t<T>{}, std::forward<U>(value)};
    }

    // Static factory for error.
    [[nodiscard]] static constexpr Expected error(ParseError err) noexcept {
        return Expected{std::in_place_type_t<ParseError>{}, std::move(err)};
    }

    constexpr explicit operator bool() const noexcept { return has_value(); }

    [[nodiscard]] constexpr bool has_value() const noexcept {
        return std::holds_alternative<T>(mData);
    }

    [[nodiscard]] constexpr const T& value() const& {
        return std::get<T>(mData);
    }

    [[nodiscard]] constexpr T&& value() && {
        return std::move(std::get<T>(mData));
    }

    [[nodiscard]] constexpr T& value() & {
        return std::get<T>(mData);
    }

    [[nodiscard]] constexpr const ParseError& error() const noexcept {
        return std::get<ParseError>(mData);
    }

    [[nodiscard]] constexpr T& operator*() {
        return std::get<T>(mData);
    }

    [[nodiscard]] constexpr const T& operator*() const {
        return std::get<T>(mData);
    }

    // map: transform success value, propagate error unchanged.
    template <typename F>
    [[nodiscard]] constexpr auto map(F&& f) -> Expected<std::invoke_result_t<F, T&&>>
        requires std::is_invocable_v<F, T&&>
    {
        if (has_value()) {
            return Expected<std::invoke_result_t<F, T&&>>::hold(
                std::invoke(std::forward<F>(f), std::move(value()))
            );
        }
        return Expected<std::invoke_result_t<F, T&&>>::error(std::get<ParseError>(mData));
    }

    template <typename F>
    [[nodiscard]] constexpr auto map(F&& f) const& -> Expected<std::invoke_result_t<F, const T&>>
        requires std::is_invocable_v<F, const T&>
    {
        if (has_value()) {
            return Expected<std::invoke_result_t<F, const T&>>::hold(
                std::invoke(std::forward<F>(f), value())
            );
        }
        return Expected<std::invoke_result_t<F, const T&>>::error(std::get<ParseError>(mData));
    }

    // and_then: chain Expected-producing calls.
    template <typename F>
    [[nodiscard]] constexpr auto and_then(F&& f) -> decltype(std::invoke(std::declval<F>(), std::declval<T&&>()))
        requires std::is_invocable_v<F, T&&>
    {
        if (has_value()) {
            return std::invoke(std::forward<F>(f), std::move(value()));
        }
        using R = decltype(std::invoke(std::forward<F>(f), std::move(value())));
        return R::error(std::get<ParseError>(mData));
    }

    template <typename F>
    [[nodiscard]] constexpr auto and_then(F&& f) const& -> decltype(std::invoke(std::declval<F>(), std::declval<const T&>()))
        requires std::is_invocable_v<F, const T&>
    {
        if (has_value()) {
            return std::invoke(std::forward<F>(f), value());
        }
        using R = decltype(std::invoke(std::forward<F>(f), value()));
        return R::error(std::get<ParseError>(mData));
    }

private:
    constexpr explicit Expected(std::in_place_type_t<T>, T value)
        : mData(std::move(value)) {}

    constexpr explicit Expected(std::in_place_type_t<ParseError>, ParseError err)
        : mData(std::move(err)) {}

    std::variant<T, ParseError> mData;
};

// ── Expected<void, ParseError> specialization ────────────────────────────

template <>
class Expected<void> {
public:
    [[nodiscard]] static constexpr Expected hold() noexcept {
        return Expected{TagSuccess{}};
    }

    [[nodiscard]] static constexpr Expected error(ParseError err) noexcept {
        return Expected{std::move(err)};
    }

    // Convert from Expected<U> — propagates error, discards value.
    template <typename U>
    constexpr Expected(const Expected<U>& other) {
        if (other.has_value()) {
            mData = TagSuccess{};
        } else {
            mData = other.error();
        }
    }

    template <typename U>
    constexpr Expected(Expected<U>&& other) {
        if (other.has_value()) {
            mData = TagSuccess{};
        } else {
            mData = other.error();
        }
    }

    constexpr explicit operator bool() const noexcept { return has_value(); }

    [[nodiscard]] constexpr bool has_value() const noexcept {
        return std::holds_alternative<TagSuccess>(mData);
    }

    [[nodiscard]] constexpr const ParseError& error() const noexcept {
        return std::get<ParseError>(mData);
    }

    // and_then: chain to an Expected-producing function (no value passed).
    template <typename F>
    [[nodiscard]] constexpr auto and_then(F&& f) -> std::invoke_result_t<F>
        requires std::is_invocable_v<F>
    {
        if (has_value()) {
            return std::invoke(std::forward<F>(f));
        }
        using R = std::invoke_result_t<F>;
        return R::error(std::get<ParseError>(mData));
    }

    // map: transform success to a new value type.
    template <typename F>
    [[nodiscard]] constexpr auto map(F&& f) -> Expected<std::invoke_result_t<F>>
        requires std::is_invocable_v<F>
    {
        if (has_value()) {
            return Expected<std::invoke_result_t<F>>::hold(std::invoke(std::forward<F>(f)));
        }
        return Expected<std::invoke_result_t<F>>::error(std::get<ParseError>(mData));
    }

private:
    struct TagSuccess {};

    constexpr explicit Expected(TagSuccess) : mData(TagSuccess{}) {}
    constexpr explicit Expected(ParseError err) : mData(std::move(err)) {}

    std::variant<TagSuccess, ParseError> mData;
};

} // namespace gsml3parser
