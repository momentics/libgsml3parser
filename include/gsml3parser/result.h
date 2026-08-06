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

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

namespace gsml3parser {

enum class ParseErrorCode : uint8_t {
    Ok,
    TruncatedInput,
    InvalidPD,
    InvalidMTI,
    LengthMismatch,
    InvalidIE,
    InvalidValue,
    UnsupportedFeature,
};

struct ParseError {
    ParseErrorCode code{ParseErrorCode::Ok};
    std::string message;
    size_t bitPosition{};

    constexpr bool failed() const { return code != ParseErrorCode::Ok; }
};

// ── ParseResult<T> (general) ────────────────────────────────────────────

template <typename T>
class ParseResult {
public:
    ParseResult(T value)
        : mData(std::in_place_type_t<T>{}, std::move(value)) {}

    ParseResult(ParseErrorCode code, std::string msg, size_t bitPos = 0)
        : mData(std::in_place_type_t<ParseError>{}, ParseError{code, std::move(msg), bitPos}) {}

    // Copy constructor from error (allows propagating errors without re-creating them)
    ParseResult(const ParseError& err) : mData(err) {}

    constexpr explicit operator bool() const { return has_value(); }

    [[nodiscard]] constexpr bool has_value() const {
        return std::holds_alternative<T>(mData);
    }

    [[nodiscard]] const T& value() const& {
        return std::get<T>(mData);
    }

    [[nodiscard]] T&& value() && {
        return std::move(std::get<T>(mData));
    }

    [[nodiscard]] const ParseError& error() const {
        return std::get<ParseError>(mData);
    }

    [[nodiscard]] T value_or(T defaultVal) const {
        return has_value() ? value() : std::move(defaultVal);
    }

    // unwrap_or: get value or default (rvalue ref)
    [[nodiscard]] T unwrap_or(T defaultVal) && {
        return has_value() ? std::move(value()) : std::move(defaultVal);
    }

    // Implicit conversion to T for rvalues (allows auto msg = parseL3(...); pattern)
    [[nodiscard]] operator T() && {
        if (!has_value()) return T{};
        return std::move(value());
    }

    // and_then: chain operations that return ParseResult, dependent on value
    template <typename F>
    [[nodiscard]] auto and_then(F&& f) -> ParseResult<std::invoke_result_t<F, T&&>>
        requires std::is_invocable_v<F, T&&>
    {
        if (has_value()) {
            return std::invoke(std::forward<F>(std::move(value())));
        }
        return ParseResult<std::invoke_result_t<F, T&&>>(std::get<ParseError>(mData));
    }

    // and_then: chain operations that return ParseResult, lvalue value
    template <typename F>
    [[nodiscard]] auto and_then(F&& f) const& -> ParseResult<std::invoke_result_t<F, const T&>>
        requires std::is_invocable_v<F, const T&>
    {
        if (has_value()) {
            return std::invoke(std::forward<F>(f), value());
        }
        return ParseResult<std::invoke_result_t<F, const T&>>(std::get<ParseError>(mData));
    }

    // transform: map value without changing error
    template <typename F>
    [[nodiscard]] auto transform(F&& f) -> ParseResult<std::invoke_result_t<F, const T&>>
        requires std::is_invocable_v<F, const T&>
    {
        if (has_value()) {
            return ParseResult<std::invoke_result_t<F, const T&>>(std::invoke(std::forward<F>(f), value()));
        }
        return ParseResult<std::invoke_result_t<F, const T&>>(std::get<ParseError>(mData));
    }

    // operator-> for unique_ptr<T>: returns raw T*
    template <typename U = std::remove_cv_t<T>>
    [[nodiscard]] auto operator->() -> typename std::enable_if_t<
        std::is_same_v<U, std::unique_ptr<typename std::remove_cv_t<U>::element_type>>,
        typename std::remove_cv_t<U>::element_type*>
    {
        return std::get<T>(mData).get();
    }

    template <typename U = std::remove_cv_t<T>>
    [[nodiscard]] auto operator->() const -> typename std::enable_if_t<
        std::is_same_v<U, std::unique_ptr<typename std::remove_cv_t<U>::element_type>>,
        const typename std::remove_cv_t<U>::element_type*>
    {
        return std::get<T>(mData).get();
    }

    // operator-> for non-unique_ptr types: returns pointer to contained value
    template <typename U = std::remove_cv_t<T>>
    [[nodiscard]] auto operator->() -> typename std::enable_if_t<
        !std::is_same_v<U, std::unique_ptr<typename std::remove_cv_t<U>::element_type>>,
        U*>
    {
        return &std::get<T>(mData);
    }

    template <typename U = std::remove_cv_t<T>>
    [[nodiscard]] auto operator->() const -> typename std::enable_if_t<
        !std::is_same_v<U, std::unique_ptr<typename std::remove_cv_t<U>::element_type>>,
        const U*>
    {
        return &std::get<T>(mData);
    }

    [[nodiscard]] T& operator*() {
        return std::get<T>(mData);
    }

    [[nodiscard]] const T& operator*() const {
        return std::get<T>(mData);
    }

    // get() for unique_ptr<T>: returns raw T*
    template <typename U = std::remove_cv_t<T>>
    [[nodiscard]] auto get() -> typename std::enable_if_t<
        std::is_same_v<U, std::unique_ptr<typename std::remove_cv_t<U>::element_type>>,
        typename std::remove_cv_t<U>::element_type*>
    {
        return std::get<T>(mData).get();
    }

    template <typename U = std::remove_cv_t<T>>
    [[nodiscard]] auto get() const -> typename std::enable_if_t<
        std::is_same_v<U, std::unique_ptr<typename std::remove_cv_t<U>::element_type>>,
        const typename std::remove_cv_t<U>::element_type*>
    {
        return std::get<T>(mData).get();
    }

private:
    std::variant<T, ParseError> mData;
};

// ── ParseOk marker for ParseResult<void> ────────────────────────────────

struct ParseOk {
    constexpr operator bool() const noexcept { return true; }
};

// ── ParseResult<void> specialization ────────────────────────────────────

template <>
class ParseResult<void> {
public:
    ParseResult() : mData(ParseOk{}) {}
    explicit ParseResult(const ParseError& err) : mData(err) {}
    ParseResult(ParseErrorCode code, std::string msg, size_t bitPos = 0)
        : mData(ParseError{code, std::move(msg), bitPos}) {}

    constexpr explicit operator bool() const { return has_value(); }
    [[nodiscard]] constexpr bool has_value() const { return std::holds_alternative<ParseOk>(mData); }
    [[nodiscard]] const ParseError& error() const { return std::get<ParseError>(mData); }

    // and_then: chain to a result-producing function (no value passed)
    template <typename F>
    [[nodiscard]] auto and_then(F&& f) -> ParseResult<std::invoke_result_t<F>>
        requires std::is_invocable_v<F>
    {
        if (has_value()) {
            return std::invoke(std::forward<F>(f));
        }
        return ParseResult<std::invoke_result_t<F>>(std::get<ParseError>(mData));
    }

private:
    std::variant<ParseOk, ParseError> mData;
};

} // namespace gsml3parser
