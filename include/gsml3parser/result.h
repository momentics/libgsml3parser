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
#include <memory>
#include <string>
#include <type_traits>
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

template <typename T>
class ParseResult {
public:
    ParseResult(T value)
        : mData(std::in_place_type_t<T>{}, std::move(value)) {}

    ParseResult(ParseErrorCode code, std::string msg, size_t bitPos = 0)
        : mData(std::in_place_type_t<ParseError>{}, ParseError{code, std::move(msg), bitPos}) {}

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

    // Implicit conversion to T for rvalues (allows auto msg = parseL3(...); pattern)
    [[nodiscard]] operator T() && {
        if (!has_value()) return T{};
        return std::move(value());
    }

    // operator-> for unique_ptr<T>: returns raw T*
    // For other types: returns pointer to the contained value
    template <typename U = T>
    [[nodiscard]] auto operator->() -> typename std::enable_if_t<
        std::is_same_v<U, std::unique_ptr<typename std::remove_cv_t<U>::element_type>>,
        typename std::remove_cv_t<U>::element_type*>
    {
        return std::get<T>(mData).get();
    }

    template <typename U = T>
    [[nodiscard]] auto operator->() const -> typename std::enable_if_t<
        std::is_same_v<U, std::unique_ptr<typename std::remove_cv_t<U>::element_type>>,
        const typename std::remove_cv_t<U>::element_type*>
    {
        return std::get<T>(mData).get();
    }

    template <typename U = T>
    [[nodiscard]] auto operator->() -> typename std::enable_if_t<
        !std::is_same_v<U, std::unique_ptr<typename std::remove_cv_t<U>::element_type>>,
        U*>
    {
        return &std::get<T>(mData);
    }

    template <typename U = T>
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
    template <typename U = T>
    [[nodiscard]] auto get() -> typename std::enable_if_t<
        std::is_same_v<U, std::unique_ptr<typename std::remove_cv_t<U>::element_type>>,
        typename std::remove_cv_t<U>::element_type*>
    {
        return std::get<T>(mData).get();
    }

    template <typename U = T>
    [[nodiscard]] auto get() const -> typename std::enable_if_t<
        std::is_same_v<U, std::unique_ptr<typename std::remove_cv_t<U>::element_type>>,
        const typename std::remove_cv_t<U>::element_type*>
    {
        return std::get<T>(mData).get();
    }

private:
    std::variant<T, ParseError> mData;
};

} // namespace gsml3parser
