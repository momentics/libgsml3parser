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

// std::formatter specializations for all gsml3parser enums.
// Each specialization delegates to the existing operator<<(ostream&, T).

#include <format>
#include <sstream>
#include <string_view>

#include "types.h"
#include "enums.h"
#include "cc/l3ccmessages.h"
#include "cc/l3ccelements.h"
#include "gmm/l3gmmelements.h"

namespace gsml3parser {

namespace detail {

template<typename T>
std::string format_via_ostream(const T& val) {
    std::ostringstream os;
    os << val;
    return os.str();
}

} // namespace detail

} // namespace gsml3parser

// ── Formatter specializations must live in namespace std (MSVC requirement)

#define GSML3PARSER_FORMATTER(T)                                                \
template<> struct formatter<T> : formatter<std::string_view> {                  \
    template<typename FormatCtx>                                                \
    auto format(const T& val, FormatCtx& ctx) const {                           \
        std::string s = gsml3parser::detail::format_via_ostream(val);           \
        return formatter<std::string_view>::format(s, ctx);                     \
    }                                                                           \
};

namespace std {

GSML3PARSER_FORMATTER(gsml3parser::LogLevel)
GSML3PARSER_FORMATTER(gsml3parser::L3PD)
GSML3PARSER_FORMATTER(gsml3parser::Primitive)
GSML3PARSER_FORMATTER(gsml3parser::SAPI)
GSML3PARSER_FORMATTER(gsml3parser::MobileIDType)
GSML3PARSER_FORMATTER(gsml3parser::TypeOfNumber)
GSML3PARSER_FORMATTER(gsml3parser::NumberingPlan)
GSML3PARSER_FORMATTER(gsml3parser::ChannelType)
GSML3PARSER_FORMATTER(gsml3parser::GSMAlphabet)

GSML3PARSER_FORMATTER(gsml3parser::RRCause)
GSML3PARSER_FORMATTER(gsml3parser::MMRejectCause)
GSML3PARSER_FORMATTER(gsml3parser::CCCause)
GSML3PARSER_FORMATTER(gsml3parser::CCCauseLocation)
GSML3PARSER_FORMATTER(gsml3parser::BSSCause)

GSML3PARSER_FORMATTER(gsml3parser::CCMessageType)

GSML3PARSER_FORMATTER(gsml3parser::GMMPTMSIType)

} // namespace std

#undef GSML3PARSER_FORMATTER

// Nested enums of L3ProgressIndicator
namespace std {

template<>
struct formatter<gsml3parser::L3ProgressIndicator::Location>
    : formatter<std::string_view> {
    template<typename FormatCtx>
    auto format(gsml3parser::L3ProgressIndicator::Location val, FormatCtx& ctx) const {
        std::string s = gsml3parser::detail::format_via_ostream(val);
        return formatter<std::string_view>::format(s, ctx);
    }
};

template<>
struct formatter<gsml3parser::L3ProgressIndicator::Progress>
    : formatter<std::string_view> {
    template<typename FormatCtx>
    auto format(gsml3parser::L3ProgressIndicator::Progress val, FormatCtx& ctx) const {
        std::string s = gsml3parser::detail::format_via_ostream(val);
        return formatter<std::string_view>::format(s, ctx);
    }
};

} // namespace std
