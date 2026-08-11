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
template<> struct std::formatter<T> : std::formatter<std::string_view> {        \
    template<typename FormatCtx>                                                \
    auto format(const T& val, FormatCtx& ctx) const {                           \
        std::string s = gsml3parser::detail::format_via_ostream(val);           \
        return std::formatter<std::string_view>::format(s, ctx);                \
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
