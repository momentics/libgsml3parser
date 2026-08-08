// Phase 3 stub — parse/serialize functions implemented in Phase 3.

#include "gsml3parser/parser.h"
#include "gsml3parser/rr/l3rrmessages.h"
#include "gsml3parser/mm/l3mmmessages.h"
#include "gsml3parser/cc/l3ccmessages.h"
#include "gsml3parser/ss/l3ssmessages.h"

namespace gsml3parser {

// Dummy classes to satisfy old parser.h forward declarations (Phase 3 will remove these).
#define DUMMY_MSG_CLASS(Name, PDVal) \
class Name : public L3Message { \
public: \
    size_t l2BodyLength() const override { return 0; } \
    size_t fullBodyLength() const override { return 0; } \
    L3PD pd() const override { return PDVal; } \
    int mti() const override { return 0; } \
    void text(std::ostream&) const override {} \
};

DUMMY_MSG_CLASS(L3RRMessage, L3PD::RadioResource)
DUMMY_MSG_CLASS(L3MMMessage, L3PD::MobilityManagement)
DUMMY_MSG_CLASS(L3CCMessage, L3PD::CallControl)
DUMMY_MSG_CLASS(L3SupServMessage, L3PD::NonCallSS)
#undef DUMMY_MSG_CLASS

ParseResult<std::unique_ptr<L3Message>> parseL3(const L3Frame&, const ParserContext&) {
    return ParseResult<std::unique_ptr<L3Message>>(ParseErrorCode::UnsupportedFeature, "parseL3 Phase 3");
}

ParseResult<std::unique_ptr<L3Message>> parseL3(std::span<const uint8_t>, const ParserContext&) {
    return ParseResult<std::unique_ptr<L3Message>>(ParseErrorCode::UnsupportedFeature, "parseL3 Phase 3");
}

ParseResult<std::unique_ptr<L3Message>> parseL3Hex(std::string_view, const ParserContext&) {
    return ParseResult<std::unique_ptr<L3Message>>(ParseErrorCode::UnsupportedFeature, "parseL3Hex Phase 3");
}

ParseResult<size_t> writeL3(const L3Message&, uint8_t*, size_t) {
    return ParseResult<size_t>(ParseErrorCode::UnsupportedFeature, "writeL3 Phase 3");
}

std::string writeL3Hex(const L3Message&) {
    return "";
}

namespace detail {

ParseResult<std::unique_ptr<L3RRMessage>> L3RRFactory(int) {
    return ParseResult<std::unique_ptr<L3RRMessage>>(ParseErrorCode::UnsupportedFeature, "Phase 3");
}
ParseResult<std::unique_ptr<L3MMMessage>> L3MMFactory(int) {
    return ParseResult<std::unique_ptr<L3MMMessage>>(ParseErrorCode::UnsupportedFeature, "Phase 3");
}
ParseResult<std::unique_ptr<L3CCMessage>> L3CCFactory(int) {
    return ParseResult<std::unique_ptr<L3CCMessage>>(ParseErrorCode::UnsupportedFeature, "Phase 3");
}
ParseResult<std::unique_ptr<L3SupServMessage>> L3SupServFactory(int) {
    return ParseResult<std::unique_ptr<L3SupServMessage>>(ParseErrorCode::UnsupportedFeature, "Phase 3");
}

ParseResult<std::unique_ptr<L3RRMessage>> parseL3RR(const L3Frame&) {
    return ParseResult<std::unique_ptr<L3RRMessage>>(ParseErrorCode::UnsupportedFeature, "Phase 3");
}
ParseResult<std::unique_ptr<L3MMMessage>> parseL3MM(const L3Frame&) {
    return ParseResult<std::unique_ptr<L3MMMessage>>(ParseErrorCode::UnsupportedFeature, "Phase 3");
}
ParseResult<std::unique_ptr<L3CCMessage>> parseL3CC(const L3Frame&) {
    return ParseResult<std::unique_ptr<L3CCMessage>>(ParseErrorCode::UnsupportedFeature, "Phase 3");
}
ParseResult<std::unique_ptr<L3SupServMessage>> parseL3SupServ(const L3Frame&) {
    return ParseResult<std::unique_ptr<L3SupServMessage>>(ParseErrorCode::UnsupportedFeature, "Phase 3");
}

} // namespace detail
} // namespace gsml3parser
