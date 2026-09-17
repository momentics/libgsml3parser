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

// Behavioral tests for the C ABI (gsml3parser_c.h). The header is
// included as C++ here to prove dual-compile; the strict-C compile check
// is the separate c_api_c89_check target. Every C call is cross-checked
// against the direct C++ API: the C API must agree with the C++ API.

#include <gtest/gtest.h>

#include <atomic>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <functional>
#include <span>
#include <string>
#include <thread>
#include <vector>

#include <gsml3parser/gsml3parser_c.h>
#include <gsml3parser/message_types.h>
#include <gsml3parser/parser.h>
#include <gsml3parser/visitor.h>
#include <gsml3parser/abis/rsl_parser.h>
#include <gsml3parser/abis/rsl_builder.h>
#include <gsml3parser/lapdm_frame.h>
#include <gsml3parser/stack/subscriber_registry.h>
#include <gsml3parser/common/l3common.h>
#include <gsml3parser/mm/l3mmmessages.h>
#include <gsml3parser/cc/l3ccmessages.h>
#include <gsml3parser/rr/l3rrmessages.h>
#include <gsml3parser/stack/procedure_orchestrator.h>
#include <gsml3parser/stack/response_builder.h>
#include <gsml3parser/ss/l3ssmessages.h>
#include <gsml3parser/sms/l3smsmessages.h>
#include <gsml3parser/sms/l3smsl3messages.h>

using namespace gsml3parser;

namespace {

struct PDBatch { int pd; const char* hex; int mti; };

// The 12 PD-domain vectors from examples/example_parse_file.cpp (batch
// demo). MTI per the L3 header rules (l3header.cpp): MM/CC/SS/BCC/GCC use
// the 6-bit MTI ((byte1 & 0xFC) >> 2); GMM/SMS/SM/LS and the raw-body PDs
// (EXT/TST) use the raw byte; RR uses the raw byte, or 0x100 + raw when
// TIF=1. The expected values below are the classes' static MTI constants
// (messageMTI visits them).
const PDBatch kBatch[] = {
    {GSML3_PD_RR,  "60 0D 00",          0x0d},  // Channel Release
    {GSML3_PD_MM,  "50 84",             0x21},  // CM Service Accept
    {GSML3_PD_CC,  "3E 94 08 02 16 21", 0x25},  // Disconnect (TI=7)
    {GSML3_PD_SS,  "B0 E8 00",          0x3a},  // SupServFacilityMessage (empty facility)
    {GSML3_PD_GMM, "80 20 05",          0x20},  // GMM Status (cause=5)
    {GSML3_PD_SM,  "A0 55 A7 01 05",    0x55},  // SM Status (cause=5)
    {GSML3_PD_SMS, "90 04",             0x04},  // CP-Ack (no body)
    {GSML3_PD_BCC, "10 00",             0x00},  // BCC Setup
    {GSML3_PD_GCC, "00 00 02",          0x00},  // GCC Setup
    {GSML3_PD_LS,  "C0 01",             0x01},  // LocationServiceRequest
    {GSML3_PD_EXT, "E0 01",             0x01},  // ExtendedMessage
    {GSML3_PD_TST, "F0 01",             0x01},  // TestProcedureMessage
};

std::vector<uint8_t> fromHex(const char* hex) {
    auto v = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        return c - 'A' + 10;
    };
    std::vector<uint8_t> out;
    for (const char* p = hex; *p; ++p) {
        if (*p == ' ') continue;
        out.push_back(static_cast<uint8_t>(v(*p) * 16 + v(p[1])));
        ++p;
    }
    return out;
}

std::string toLower(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

} // namespace

// Test: every PD domain parses through the C API with the expected
// pd/mti; metadata agrees with the C++ API; wire bytes and hex
// round-trip exactly.
TEST(CApi, L3RoundTrip_AllDomains) {
    for (const auto& b : kBatch) {
        gsml3_message* m = gsml3_parse_l3_hex(b.hex, nullptr);
        ASSERT_NE(m, nullptr) << b.hex << ": " << gsml3_last_error();
        EXPECT_EQ(gsml3_message_pd(m), b.pd) << b.hex;
        EXPECT_EQ(gsml3_message_mti(m), b.mti) << b.hex;
        const char* name = gsml3_message_name(m);
        ASSERT_NE(name, nullptr);
        EXPECT_GT(std::strlen(name), 0u) << b.hex;

        // Cross-check against the C++ API.
        auto cpp = parseL3Hex(b.hex);
        ASSERT_TRUE(cpp) << b.hex;
        EXPECT_STREQ(name, messageName(*cpp).data());
        EXPECT_EQ(gsml3_message_pd(m), static_cast<int>(messagePD(*cpp)));
        EXPECT_EQ(gsml3_message_mti(m), messageMTI(*cpp));
        EXPECT_EQ(gsml3_message_ti(m), static_cast<int>(messageTI(*cpp)));

        // Serialize back: wire bytes identical to the input.
        std::vector<uint8_t> ref = fromHex(b.hex);
        uint8_t buf[256];
        size_t n = gsml3_message_write(m, buf, sizeof(buf));
        ASSERT_EQ(n, ref.size()) << b.hex;
        EXPECT_EQ(0, std::memcmp(buf, ref.data(), n)) << b.hex;

        // Re-parse the serialized bytes.
        gsml3_message* m2 = gsml3_parse_l3(buf, n, nullptr);
        ASSERT_NE(m2, nullptr) << b.hex;
        EXPECT_STREQ(gsml3_message_name(m2), name);
        EXPECT_EQ(gsml3_message_pd(m2), b.pd);
        EXPECT_EQ(gsml3_message_mti(m2), b.mti);

        // Hex round-trip (the C API emits lowercase).
        char* hex = gsml3_message_hex(m);
        ASSERT_NE(hex, nullptr) << b.hex;
        static const char* kDigits = "0123456789abcdef";
        std::string expected;
        for (auto c : ref) {
            expected += kDigits[c >> 4];
            expected += kDigits[c & 0x0F];
        }
        EXPECT_EQ(toLower(hex), expected) << b.hex;
        gsml3_free(hex);

        gsml3_message_free(m);
        gsml3_message_free(m2);
    }
}

// Test: gsml3_parse_l3_into reparses into an existing handle (zero extra
// allocation for typical messages) and keeps the previous content on
// error.
TEST(CApi, ParseInto_ReusesHandle) {
    gsml3_message* m = gsml3_parse_l3_hex("60 0D 00", nullptr);
    ASSERT_NE(m, nullptr);

    uint8_t mm[] = {0x50, 0x84};  // CM Service Accept
    EXPECT_EQ(gsml3_parse_l3_into(m, mm, sizeof(mm), nullptr), GSML3_OK);
    EXPECT_EQ(gsml3_message_pd(m), GSML3_PD_MM);
    EXPECT_EQ(gsml3_message_mti(m), 0x21);

    // Error input (truncated RR ChannelRelease): the handle keeps its
    // previous (valid) content. A single byte is NOT an error: one-octet
    // frames parse as RR ChannelRequest short messages.
    uint8_t truncated[] = {0x60, 0x0D};
    EXPECT_NE(gsml3_parse_l3_into(m, truncated, sizeof(truncated), nullptr), GSML3_OK);
    EXPECT_EQ(gsml3_message_pd(m), GSML3_PD_MM);
    EXPECT_EQ(gsml3_message_mti(m), 0x21);

    gsml3_message_free(m);
}

// Test: error paths report NULL + a non-empty thread-local message.
TEST(CApi, L3ErrorPaths) {
    EXPECT_EQ(gsml3_parse_l3(nullptr, 4, nullptr), nullptr);
    EXPECT_GT(std::strlen(gsml3_last_error()), 0u);

    uint8_t d[3] = {0x50, 0x84, 0x00};
    EXPECT_EQ(gsml3_parse_l3(d, 0, nullptr), nullptr);

    EXPECT_EQ(gsml3_parse_l3_hex(nullptr, nullptr), nullptr);
    EXPECT_EQ(gsml3_parse_l3_hex("zz", nullptr), nullptr);
    EXPECT_EQ(gsml3_parse_l3_hex("60 0D", nullptr), nullptr);  // truncated RR ChannelRelease
    EXPECT_GT(std::strlen(gsml3_last_error()), 0u);

    // Strict framing rejects trailing bytes.
    gsml3_config* cfg = gsml3_config_new();
    ASSERT_NE(cfg, nullptr);
    gsml3_config_set_strict_framing(cfg, 1);
    EXPECT_EQ(gsml3_parse_l3(d, sizeof(d), cfg), nullptr);
    gsml3_config_free(cfg);
}

// Test: every query/free function accepts NULL without crashing.
TEST(CApi, NullSafety) {
    EXPECT_STREQ(gsml3_message_name(nullptr), "");
    EXPECT_EQ(gsml3_message_pd(nullptr), -1);
    EXPECT_EQ(gsml3_message_mti(nullptr), -1);
    EXPECT_EQ(gsml3_message_ti(nullptr), 0);
    uint8_t buf[8];
    EXPECT_EQ(gsml3_message_write(nullptr, buf, sizeof(buf)), 0u);
    EXPECT_EQ(gsml3_message_hex(nullptr), nullptr);
    EXPECT_NE(gsml3_parse_l3_into(nullptr, buf, 1, nullptr), GSML3_OK);
    gsml3_message_free(nullptr);
    gsml3_config_free(nullptr);
    gsml3_free(nullptr);
}

// Test: the config setters take effect (log level range-checked, strict
// framing switches the parser behavior).
TEST(CApi, Config) {
    gsml3_config* c = gsml3_config_new();
    ASSERT_NE(c, nullptr);
    gsml3_config_set_log_level(c, GSML3_LOG_DEBUG);
    gsml3_config_set_log_level(c, 99);  // out of range: ignored
    gsml3_config_set_log_level(c, GSML3_LOG_EMERG);
    gsml3_config_set_strict_framing(c, 1);

    // "50 84" is a complete CM Service Accept: strict framing accepts it.
    gsml3_message* ok = gsml3_parse_l3_hex("50 84", c);
    ASSERT_NE(ok, nullptr);
    gsml3_message_free(ok);

    // Same message + trailing byte: strict framing rejects it.
    uint8_t d[] = {0x50, 0x84, 0x00};
    EXPECT_EQ(gsml3_parse_l3(d, sizeof(d), c), nullptr);
    gsml3_config_free(c);
}

// Test: 8 threads x 1000 iterations of parse -> query -> free on
// independent handles (the name contains "Concurrent" so the TSan CI
// filter *Thread*:*Shard*:*Concurrent*:*TornRead* picks it up).
TEST(CApi, Concurrent_IndependentHandles) {
    std::vector<std::thread> threads;
    std::atomic<int> failures{0};
    for (int t = 0; t < 8; ++t) {
        threads.emplace_back([&failures]() {
            for (int i = 0; i < 1000; ++i) {
                gsml3_message* m = gsml3_parse_l3_hex("60 0D 00", nullptr);
                if (!m || gsml3_message_pd(m) != GSML3_PD_RR ||
                        gsml3_message_mti(m) != 0x0D) {
                    ++failures;
                }
                gsml3_message_free(m);
            }
        });
    }
    for (auto& th : threads) th.join();
    EXPECT_EQ(failures.load(), 0);
}

// ── RSL (A-bis) ────────────────────────────────────────────────────────

namespace {

// Build an RLL DATA_REQ frame: disc 0x00 | msgType 0x21 | chanNr |
// linkId | L3Info IE (0x30, TL16V).
std::vector<uint8_t> makeRllDataReq(uint8_t chanNr, uint8_t linkId,
                                    const std::vector<uint8_t>& l3) {
    std::vector<uint8_t> b;
    b.push_back(0x00);  // discriminator RLL, BSC->BTS
    b.push_back(0x21);  // DATA_REQ
    b.push_back(chanNr);
    b.push_back(linkId);
    b.push_back(0x30);  // L3Info IE (TL16V)
    b.push_back(static_cast<uint8_t>(l3.size() >> 8));
    b.push_back(static_cast<uint8_t>(l3.size() & 0xFF));
    b.insert(b.end(), l3.begin(), l3.end());
    return b;
}

} // namespace

// Test: RSL parse through the C API agrees with the C++ RSLParser on
// every field; the L3 view points into the handle's copy (the caller's
// buffer is freed right after parse).
TEST(CApiRsl, Parse_AgreesWithCpp) {
    std::vector<uint8_t> frame = makeRllDataReq(0x7C, 1, {0x60, 0x0D, 0x00});
    auto cpp = RSLParser::parse(frame);
    ASSERT_TRUE(cpp);
    const RSLParsedMessage& c = *cpp;

    gsml3_rsl* r = gsml3_rsl_parse(frame.data(), frame.size());
    ASSERT_NE(r, nullptr) << gsml3_last_error();

    EXPECT_STREQ(gsml3_rsl_name(r),
                 RSLParser::messageName(c.discriminator, c.msgType).data());
    EXPECT_EQ(gsml3_rsl_discriminator(r), static_cast<int>(c.discriminator));
    EXPECT_EQ(gsml3_rsl_msg_type(r), c.msgType);
    EXPECT_EQ(gsml3_rsl_chan_nr(r), c.chanNr);
    EXPECT_EQ(gsml3_rsl_link_id(r), c.linkId);
    EXPECT_EQ(gsml3_rsl_bts_to_bsc(r), c.btsToBsc ? 1 : 0);
    EXPECT_EQ(gsml3_rsl_has_l3(r), 1);

    size_t l3len = 0;
    const uint8_t* l3 = gsml3_rsl_l3(r, &l3len);
    ASSERT_NE(l3, nullptr);
    EXPECT_EQ(l3len, 3u);
    EXPECT_EQ(0, std::memcmp(l3, frame.data() + 7, 3));

    // IE access: the L3Info IE (0x30) is the first one.
    EXPECT_EQ(gsml3_rsl_ie_count(r), c.ieCount);
    uint8_t ietype = 0; size_t ielen = 0; const uint8_t* ieval = nullptr;
    ASSERT_EQ(gsml3_rsl_ie_get(r, 0, &ietype, &ielen, &ieval), GSML3_OK);
    EXPECT_EQ(ietype, 0x30);
    EXPECT_EQ(ielen, 3u);
    EXPECT_NE(gsml3_rsl_ie_get(r, 99, &ietype, &ielen, &ieval), GSML3_OK);

    // The handle owns a copy: the input buffer is dead, the views live.
    frame.clear();
    frame.shrink_to_fit();
    EXPECT_EQ(l3len, 3u);
    gsml3_rsl_free(r);
}

// Test: every RSL builder produces exactly the bytes the C++ vector
// overload produces, and the frame parses back with the expected fields.
TEST(CApiRsl, Builders_AgreeWithCpp) {
    const std::vector<uint8_t> l3 = {0x60, 0x0D, 0x00};
    uint8_t out[512];

    struct Case {
        size_t (*c)(uint8_t*, size_t, uint8_t, uint8_t, const uint8_t*, size_t);
        std::vector<uint8_t> cpp;
    };
    Case cases[] = {
        {gsml3_rsl_build_data_req,      RSLBuilder::buildDataReq(0x7C, 1, l3).value()},
        {gsml3_rsl_build_data_ind,      RSLBuilder::buildDataInd(0x7C, 1, l3).value()},
        {gsml3_rsl_build_unit_data_req, RSLBuilder::buildUnitDataReq(0x7C, 1, l3).value()},
        {gsml3_rsl_build_unit_data_ind, RSLBuilder::buildUnitDataInd(0x7C, 1, l3).value()},
    };
    for (const auto& c : cases) {
        size_t n = c.c(out, sizeof(out), 0x7C, 1, l3.data(), l3.size());
        ASSERT_EQ(n, c.cpp.size());
        EXPECT_EQ(0, std::memcmp(out, c.cpp.data(), n));
        // Parse back: the L3 payload must round-trip.
        gsml3_rsl* r = gsml3_rsl_parse(out, n);
        ASSERT_NE(r, nullptr);
        size_t l3len = 0;
        const uint8_t* l3v = gsml3_rsl_l3(r, &l3len);
        ASSERT_NE(l3v, nullptr);
        EXPECT_EQ(l3len, l3.size());
        EXPECT_EQ(0, std::memcmp(l3v, l3.data(), l3.size()));
        gsml3_rsl_free(r);
    }

    // Fixed-shape builders.
    {
        auto cpp = RSLBuilder::buildChanActivAck(0x78, 0x1234).value();
        size_t n = gsml3_rsl_build_chan_activ_ack(out, sizeof(out), 0x78, 0x1234);
        ASSERT_EQ(n, cpp.size());
        EXPECT_EQ(0, std::memcmp(out, cpp.data(), n));
    }
    {
        auto cpp = RSLBuilder::buildChanActivNack(0x78, RSLErrorCause::EquipmentFailure).value();
        size_t n = gsml3_rsl_build_chan_activ_nack(out, sizeof(out), 0x78, 0x03);
        ASSERT_EQ(n, cpp.size());
        EXPECT_EQ(0, std::memcmp(out, cpp.data(), n));
    }
    {
        auto cpp = RSLBuilder::buildRFChanRelAck(0x78).value();
        size_t n = gsml3_rsl_build_rf_chan_rel_ack(out, sizeof(out), 0x78);
        ASSERT_EQ(n, cpp.size());
        EXPECT_EQ(0, std::memcmp(out, cpp.data(), n));
    }
    {
        auto cpp = RSLBuilder::buildConnFail(0x78, RSLErrorCause::ResourceUnavailable).value();
        size_t n = gsml3_rsl_build_conn_fail(out, sizeof(out), 0x78, 0x06);
        ASSERT_EQ(n, cpp.size());
        EXPECT_EQ(0, std::memcmp(out, cpp.data(), n));
    }
    {
        const std::vector<uint8_t> l1 = {0x01, 0x02, 0x03};
        auto cpp = RSLBuilder::buildMeasRes(0x78, 5, -47, 3, l1).value();
        size_t n = gsml3_rsl_build_meas_res(out, sizeof(out), 0x78, 5, -47, 3, l1.data(), l1.size());
        ASSERT_EQ(n, cpp.size());
        EXPECT_EQ(0, std::memcmp(out, cpp.data(), n));
    }
    {
        auto cpp = RSLBuilder::buildHandoDet(0x78, 7).value();
        size_t n = gsml3_rsl_build_hando_det(out, sizeof(out), 0x78, 7);
        ASSERT_EQ(n, cpp.size());
        EXPECT_EQ(0, std::memcmp(out, cpp.data(), n));
    }
    {
        auto cpp = RSLBuilder::buildCCCHLoadInd(0x00, 85, 120, 10, 90).value();
        size_t n = gsml3_rsl_build_ccch_load_ind(out, sizeof(out), 0x00, 85, 120, 10, 90);
        ASSERT_EQ(n, cpp.size());
        EXPECT_EQ(0, std::memcmp(out, cpp.data(), n));
    }
    {
        auto cpp = RSLBuilder::buildChanRqd(0x40, L3RequestReference(0x55, 0, 0, 0), 4).value();
        size_t n = gsml3_rsl_build_chan_rqd(out, sizeof(out), 0x40, 0x55, 0, 0, 0, 4);
        ASSERT_EQ(n, cpp.size());
        EXPECT_EQ(0, std::memcmp(out, cpp.data(), n));
    }
    {
        const std::vector<uint8_t> info = {0xAA, 0xBB};
        auto cpp = RSLBuilder::buildDeleteInd(0x40, info).value();
        size_t n = gsml3_rsl_build_delete_ind(out, sizeof(out), 0x40, info.data(), info.size());
        ASSERT_EQ(n, cpp.size());
        EXPECT_EQ(0, std::memcmp(out, cpp.data(), n));
    }
}

// Test: buffer-too-small returns 0 (documented C contract).
TEST(CApiRsl, Builder_BufferTooSmall) {
    uint8_t out[4];
    const std::vector<uint8_t> l3 = {0x60, 0x0D, 0x00};
    EXPECT_EQ(gsml3_rsl_build_data_req(out, sizeof(out), 0x7C, 1, l3.data(), l3.size()), 0u);
    EXPECT_GT(std::strlen(gsml3_last_error()), 0u);
}

// Test: NULL safety of the RSL accessors.
TEST(CApiRsl, NullSafety) {
    EXPECT_STREQ(gsml3_rsl_name(nullptr), "");
    EXPECT_EQ(gsml3_rsl_discriminator(nullptr), -1);
    EXPECT_EQ(gsml3_rsl_msg_type(nullptr), -1);
    EXPECT_EQ(gsml3_rsl_chan_nr(nullptr), -1);
    EXPECT_EQ(gsml3_rsl_link_id(nullptr), -1);
    EXPECT_EQ(gsml3_rsl_bts_to_bsc(nullptr), -1);
    EXPECT_EQ(gsml3_rsl_has_l3(nullptr), 0);
    size_t len = 99;
    EXPECT_EQ(gsml3_rsl_l3(nullptr, &len), nullptr);
    EXPECT_EQ(len, 0u);
    EXPECT_EQ(gsml3_rsl_ie_count(nullptr), 0u);
    gsml3_rsl_free(nullptr);
}

// ── LAPDm (GSM 04.06) ─────────────────────────────────────────────────

namespace {

struct LapdmL3Event {
    int sapi;
    int primitive;  // GSML3_PRIM_*
    std::vector<uint8_t> data;
};

struct LapdmCapture {
    std::vector<std::vector<uint8_t>> txFrames;  // L1 callback
    std::vector<LapdmL3Event> l3;                // L3 callback (sapi, primitive, bytes)
};

void cL3Cb(int sapi, int primitive, const uint8_t* l3, size_t l3_len, void* user) {
    auto* cap = static_cast<LapdmCapture*>(user);
    LapdmL3Event ev;
    ev.sapi = sapi;
    ev.primitive = primitive;
    if (l3 && l3_len) ev.data.assign(l3, l3 + l3_len);
    cap->l3.push_back(ev);
}

void cL1Cb(const uint8_t* frame, size_t frame_len, void* user) {
    auto* cap = static_cast<LapdmCapture*>(user);
    cap->txFrames.emplace_back(frame, frame + frame_len);
}

} // namespace

// Test: frame decode through the C API agrees with the C++ decoder on
// UI, I and S frames (zero-copy: info points into the input).
TEST(CApiLapdm, FrameDecode_AgreesWithCpp) {
    const std::vector<uint8_t> info = {0x60, 0x0D, 0x00};
    auto ui = lapdm::makeUIFrame(SAPI::SAPI0, true, info);
    std::vector<uint8_t> uiBytes = lapdm::encodeFrame(ui);

    gsml3_lapdm_frame_info f{};
    ASSERT_EQ(gsml3_lapdm_frame_decode(uiBytes.data(), uiBytes.size(), &f), GSML3_OK);
    EXPECT_EQ(f.format, GSML3_LAPDM_FMT_U);
    EXPECT_EQ(f.u_type, GSML3_LAPDM_U_UI);
    EXPECT_EQ(f.s_type, -1);
    EXPECT_EQ(f.sapi, GSML3_SAPI0);
    EXPECT_EQ(f.command, 1);
    ASSERT_NE(f.info, nullptr);
    EXPECT_EQ(f.info_len, info.size());
    EXPECT_EQ(0, std::memcmp(f.info, info.data(), info.size()));
    // Zero-copy: info points inside the input buffer (address + control
    // field precede the info field).
    EXPECT_GE(f.info, uiBytes.data());
    EXPECT_LE(f.info + f.info_len, uiBytes.data() + uiBytes.size());

    auto ifr = lapdm::makeIFrame(SAPI::SAPI3, true, 2, 5, true, false, info);
    std::vector<uint8_t> ifBytes = lapdm::encodeFrame(ifr);
    ASSERT_EQ(gsml3_lapdm_frame_decode(ifBytes.data(), ifBytes.size(), &f), GSML3_OK);
    EXPECT_EQ(f.format, GSML3_LAPDM_FMT_I);
    EXPECT_EQ(f.nr, 2);
    EXPECT_EQ(f.ns, 5);
    EXPECT_EQ(f.pf, 1);
    EXPECT_EQ(f.m_bit, 0);
    EXPECT_EQ(f.sapi, GSML3_SAPI3);

    auto rr = lapdm::makeRRFrame(SAPI::SAPI0, 3, true);
    std::vector<uint8_t> rrBytes = lapdm::encodeFrame(rr);
    ASSERT_EQ(gsml3_lapdm_frame_decode(rrBytes.data(), rrBytes.size(), &f), GSML3_OK);
    EXPECT_EQ(f.format, GSML3_LAPDM_FMT_S);
    EXPECT_EQ(f.s_type, GSML3_LAPDM_S_RR);
    EXPECT_EQ(f.nr, 3);

    // Truncated input: clean error, no crash.
    EXPECT_NE(gsml3_lapdm_frame_decode(uiBytes.data(), 1, &f), GSML3_OK);
}

// Test: full link lifecycle through the C API: open -> SABME -> UA ->
// established -> UI receive -> L3 callback -> DISC -> released. Mirrors
// the scenarios in tests/test_lapdm.cpp.
TEST(CApiLapdm, Entity_LinkLifecycle) {
    LapdmCapture cap;
    gsml3_lapdm_entity* e = gsml3_lapdm_entity_new(0, cL3Cb, cL1Cb, &cap);
    ASSERT_NE(e, nullptr);
    EXPECT_EQ(gsml3_lapdm_entity_state(e), GSML3_LAPDM_STATE_UNUSED);

    gsml3_lapdm_entity_open(e, GSML3_SAPI0, 1);  // BTS side
    EXPECT_EQ(gsml3_lapdm_entity_state(e), GSML3_LAPDM_STATE_LINK_RELEASED);

    // SABME goes out via the L1 callback.
    ASSERT_EQ(gsml3_lapdm_entity_send_sabme(e), GSML3_OK);
    ASSERT_EQ(cap.txFrames.size(), 1u);
    gsml3_lapdm_frame_info f{};
    ASSERT_EQ(gsml3_lapdm_frame_decode(cap.txFrames[0].data(), cap.txFrames[0].size(), &f), GSML3_OK);
    EXPECT_EQ(f.u_type, GSML3_LAPDM_U_SABME);
    EXPECT_EQ(gsml3_lapdm_entity_state(e), GSML3_LAPDM_STATE_AWAITING_ESTABLISH);

    // The peer answers with UA.
    std::vector<uint8_t> ua = lapdm::encodeFrame(lapdm::makeUAFrame(SAPI::SAPI0, true, {}));
    gsml3_lapdm_entity_receive(e, ua.data(), ua.size());
    EXPECT_EQ(gsml3_lapdm_entity_state(e), GSML3_LAPDM_STATE_LINK_ESTABLISHED);
    EXPECT_EQ(gsml3_lapdm_entity_is_established(e), 1);

    // The peer sends a UI frame with L3: the L3 callback must fire.
    const std::vector<uint8_t> l3 = {0x60, 0x0D, 0x00};
    std::vector<uint8_t> ui = lapdm::encodeFrame(lapdm::makeUIFrame(SAPI::SAPI0, false, l3));
    gsml3_lapdm_entity_receive(e, ui.data(), ui.size());
    // Active-side establishment delivered an ESTABLISH_CONFIRM (empty
    // payload) on the UA; the UI frame then delivers its L3 as
    // UNIT_DATA — the same sequence semantics as test_lapdm.cpp.
    ASSERT_EQ(cap.l3.size(), 2u);
    EXPECT_EQ(cap.l3[0].primitive, GSML3_PRIM_L3_ESTABLISH_CONFIRM);
    EXPECT_TRUE(cap.l3[0].data.empty());
    EXPECT_EQ(cap.l3[1].sapi, GSML3_SAPI0);
    EXPECT_EQ(cap.l3[1].primitive, GSML3_PRIM_L3_UNIT_DATA);
    EXPECT_EQ(cap.l3[1].data, l3);

    // DISC -> AwaitingRelease -> UA -> LinkReleased.
    ASSERT_EQ(gsml3_lapdm_entity_send_disc(e), GSML3_OK);
    EXPECT_EQ(gsml3_lapdm_entity_state(e), GSML3_LAPDM_STATE_AWAITING_RELEASE);
    gsml3_lapdm_entity_receive(e, ua.data(), ua.size());
    EXPECT_EQ(gsml3_lapdm_entity_state(e), GSML3_LAPDM_STATE_LINK_RELEASED);
    EXPECT_EQ(gsml3_lapdm_entity_is_established(e), 0);

    EXPECT_GT(gsml3_lapdm_entity_frames_sent(e), 0u);
    // Received frames so far: UA (establish) + UI + UA (release) = 3.
    EXPECT_EQ(gsml3_lapdm_entity_frames_received(e), 3u);

    gsml3_lapdm_entity_free(e);
}

// Test: T200 expiry retransmits the outstanding SABME (mirrors
// tests/test_lapdm.cpp LAPDmEntityTest.T200_Retransmission).
TEST(CApiLapdm, Entity_T200_Retransmission) {
    LapdmCapture cap;
    gsml3_lapdm_entity* e = gsml3_lapdm_entity_new(0, cL3Cb, cL1Cb, &cap);
    ASSERT_NE(e, nullptr);
    gsml3_lapdm_entity_open(e, GSML3_SAPI0, 1);
    ASSERT_EQ(gsml3_lapdm_entity_send_sabme(e), GSML3_OK);
    cap.txFrames.clear();

    EXPECT_EQ(gsml3_lapdm_entity_tick_t200(e, 900), 1);  // T200 (SDCCH) = 900 ms
    ASSERT_EQ(cap.txFrames.size(), 1u);  // retransmitted SABME
    EXPECT_EQ(gsml3_lapdm_entity_retransmissions(e), 1u);

    gsml3_lapdm_entity_free(e);
}

// Test: send_data before link establishment fails with a clean error.
TEST(CApiLapdm, Entity_SendData_BeforeLink_Fails) {
    LapdmCapture cap;
    gsml3_lapdm_entity* e = gsml3_lapdm_entity_new(0, cL3Cb, cL1Cb, &cap);
    ASSERT_NE(e, nullptr);
    gsml3_lapdm_entity_open(e, GSML3_SAPI0, 1);
    const uint8_t l3[] = {0x60, 0x0D, 0x00};
    EXPECT_NE(gsml3_lapdm_entity_send_data(e, l3, sizeof(l3)), GSML3_OK);
    EXPECT_GT(std::strlen(gsml3_last_error()), 0u);
    gsml3_lapdm_entity_free(e);
}

// Test: invalid profile and NULL safety.
TEST(CApiLapdm, Entity_InvalidProfileAndNullSafety) {
    EXPECT_EQ(gsml3_lapdm_entity_new(7, nullptr, nullptr, nullptr), nullptr);
    gsml3_lapdm_entity_free(nullptr);
    EXPECT_EQ(gsml3_lapdm_entity_state(nullptr), GSML3_LAPDM_STATE_UNUSED);
    EXPECT_EQ(gsml3_lapdm_entity_is_established(nullptr), 0);
    EXPECT_EQ(gsml3_lapdm_entity_frames_sent(nullptr), 0u);
    gsml3_lapdm_entity_open(nullptr, 0, 1);
    gsml3_lapdm_entity_receive(nullptr, nullptr, 0);
    EXPECT_NE(gsml3_lapdm_entity_send_sabme(nullptr), GSML3_OK);
}

// Test: 8 threads each run an independent entity (the name contains
// "Concurrent" so the TSan CI filter picks it up).
TEST(CApiLapdm, Concurrent_IndependentEntities) {
    std::vector<std::thread> threads;
    std::atomic<int> failures{0};
    for (int t = 0; t < 8; ++t) {
        threads.emplace_back([&failures]() {
            for (int i = 0; i < 200; ++i) {
                LapdmCapture cap;
                gsml3_lapdm_entity* e = gsml3_lapdm_entity_new(0, cL3Cb, cL1Cb, &cap);
                if (!e) { ++failures; continue; }
                gsml3_lapdm_entity_open(e, GSML3_SAPI0, 1);
                if (gsml3_lapdm_entity_send_sabme(e) != GSML3_OK) ++failures;
                gsml3_lapdm_entity_tick_t200(e, 900);
                gsml3_lapdm_entity_free(e);
            }
        });
    }
    for (auto& th : threads) th.join();
    EXPECT_EQ(failures.load(), 0);
}

// ── BTS stack: registry / session ──────────────────────────────────────

// Test: plain registry — create/find/remove by TMSI and IMSI, link index,
// channel assignment, counts.
TEST(CApiRegistry, Plain_CreateFindRemove) {
    gsml3_registry* r = gsml3_registry_new(0);
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(gsml3_registry_count(r), 0u);

    gsml3_session* s1 = gsml3_registry_create_by_tmsi(r, 0x11111111);
    ASSERT_NE(s1, nullptr);
    EXPECT_EQ(gsml3_registry_create_by_tmsi(r, 0x11111111), nullptr);  // dup
    EXPECT_EQ(gsml3_registry_count(r), 1u);

    gsml3_session* s2 = gsml3_registry_create_by_imsi(r, "244051234567890");
    ASSERT_NE(s2, nullptr);
    EXPECT_EQ(gsml3_registry_count(r), 2u);
    EXPECT_NE(gsml3_registry_find_by_imsi(r, "244051234567890"), nullptr);

    EXPECT_EQ(gsml3_registry_find_by_tmsi(r, 0x11111111), s1);
    EXPECT_EQ(gsml3_registry_find_by_tmsi(r, 0xDEADBEEF), nullptr);

    // Link index + channel assignment.
    gsml3_registry_assign_channel(r, s1, 7 /* SDCCHType */, 0, 3, 5120, 1);
    EXPECT_EQ(gsml3_registry_find_by_link(r, 0, 3, 1), s1);
    gsml3_registry_release_channel(r, s1);
    EXPECT_EQ(gsml3_registry_find_by_link(r, 0, 3, 1), nullptr);

    // Session access: identity + flags.
    EXPECT_EQ(gsml3_session_tmsi(s1), 0x11111111u);
    gsml3_session_set_tmsi(s1, 0x22222222);
    EXPECT_EQ(gsml3_session_tmsi(s1), 0x22222222u);
    gsml3_session_set_imsi(s1, "244051234567890");
    EXPECT_EQ(gsml3_session_tmsi(s1), 0u);  // identity is now an IMSI
    EXPECT_EQ(gsml3_session_is_registered(s1), 0);
    gsml3_session_set_registered(s1, 1);
    EXPECT_EQ(gsml3_session_is_registered(s1), 1);
    gsml3_session_set_authenticated(s1, 1);
    EXPECT_EQ(gsml3_session_is_authenticated(s1), 1);
    gsml3_session_set_ciphered(s1, 1);
    EXPECT_EQ(gsml3_session_is_ciphered(s1), 1);

    EXPECT_EQ(gsml3_registry_remove(r, s1), 1);
    EXPECT_EQ(gsml3_registry_remove(r, s1), 0);  // already removed
    EXPECT_EQ(gsml3_registry_count(r), 1u);

    gsml3_registry_clear(r);
    EXPECT_EQ(gsml3_registry_count(r), 0u);
    gsml3_registry_free(r);
}

// Test: sharded registries of every supported size — create/lookup/tick
// with 10K sessions each.
TEST(CApiRegistry, Sharded_AllSizes_10K) {
    const int shards[] = {4, 8, 16, 32};
    for (int sh : shards) {
        gsml3_registry* r = gsml3_registry_new(sh);
        ASSERT_NE(r, nullptr) << "shards=" << sh;
        gsml3_registry_reserve(r, 10000);

        const uint32_t N = 10000;
        for (uint32_t i = 0; i < N; ++i)
            ASSERT_NE(gsml3_registry_create_by_tmsi(r, i + 1), nullptr);
        EXPECT_EQ(gsml3_registry_count(r), N);

        uint32_t found = 0;
        for (uint32_t i = 0; i < N; ++i)
            if (gsml3_registry_find_by_tmsi(r, i + 1)) ++found;
        EXPECT_EQ(found, N);

        // Start a timer in 100 sessions and tick (O(active)).
        for (uint32_t i = 0; i < 100; ++i) {
            gsml3_session* s = gsml3_registry_find_by_tmsi(r, i + 1);
            EXPECT_EQ(gsml3_session_timer_start(s, GSML3_TIMER_T3101), 1);
        }
        std::vector<gsml3_timer_expiry> exp(4096);
        size_t n = gsml3_registry_tick_timers(r, 3000, exp.data(), exp.size());
        EXPECT_EQ(n, 100u);
        for (size_t i = 0; i < n; ++i)
            EXPECT_EQ(gsml3_session_timer_running(exp[i].session, GSML3_TIMER_T3101), 0);

        // create_by_imsi / clear are plain-registry features.
        EXPECT_EQ(gsml3_registry_create_by_imsi(r, "244051234567890"), nullptr);
        gsml3_registry_clear(r);  // no-op for sharded, documented
        EXPECT_EQ(gsml3_registry_count(r), N);

        gsml3_registry_free(r);
    }
}

// Test: invalid shard counts are rejected.
TEST(CApiRegistry, InvalidShardCount) {
    for (int bad : {1, 2, 3, 5, 64, 128, -1}) {
        EXPECT_EQ(gsml3_registry_new(bad), nullptr) << "shards=" << bad;
        EXPECT_GT(std::strlen(gsml3_last_error()), 0u);
    }
}

// Test: NULL safety of the registry/session accessors.
TEST(CApiRegistry, NullSafety) {
    gsml3_registry_free(nullptr);
    EXPECT_EQ(gsml3_registry_count(nullptr), 0u);
    EXPECT_EQ(gsml3_registry_find_by_tmsi(nullptr, 1), nullptr);
    EXPECT_EQ(gsml3_registry_remove(nullptr, nullptr), 0);
    gsml3_registry_reserve(nullptr, 10);
    gsml3_registry_clear(nullptr);
    gsml3_registry_assign_channel(nullptr, nullptr, 0, 0, 0, 0, 0);
    gsml3_registry_release_channel(nullptr, nullptr);
    EXPECT_EQ(gsml3_registry_tick_timers(nullptr, 1, nullptr, 0), 0u);
    EXPECT_EQ(gsml3_registry_tick_procedures(nullptr, 1), 0u);

    EXPECT_EQ(gsml3_session_tmsi(nullptr), 0u);
    gsml3_session_set_tmsi(nullptr, 1);
    gsml3_session_set_imsi(nullptr, "123");
    EXPECT_EQ(gsml3_session_is_registered(nullptr), 0);
    gsml3_session_set_registered(nullptr, 1);
    EXPECT_EQ(gsml3_session_is_authenticated(nullptr), 0);
    gsml3_session_set_authenticated(nullptr, 1);
    EXPECT_EQ(gsml3_session_is_ciphered(nullptr), 0);
    gsml3_session_set_ciphered(nullptr, 1);
    EXPECT_EQ(gsml3_session_timer_start(nullptr, GSML3_TIMER_T3101), 0);
    gsml3_session_timer_stop(nullptr, GSML3_TIMER_T3101);
    EXPECT_EQ(gsml3_session_timer_running(nullptr, GSML3_TIMER_T3101), 0);
    EXPECT_EQ(gsml3_session_transaction_pending(nullptr), 0u);
}

// Test: 8 threads against one sharded registry (create/find/remove) —
// the name contains "Concurrent" so the TSan CI filter picks it up.
TEST(CApiRegistry, Concurrent_ShardedRegistry) {
    gsml3_registry* r = gsml3_registry_new(32);
    ASSERT_NE(r, nullptr);
    const uint32_t base = 0x40000000;
    std::vector<std::thread> threads;
    std::atomic<int> failures{0};
    for (int t = 0; t < 8; ++t) {
        threads.emplace_back([&, t]() {
            for (uint32_t i = 0; i < 2000; ++i) {
                uint32_t tmsi = base + static_cast<uint32_t>(t) * 2000 + i;
                gsml3_session* s = gsml3_registry_create_by_tmsi(r, tmsi);
                if (!s) { ++failures; continue; }
                if (gsml3_registry_find_by_tmsi(r, tmsi) != s) ++failures;
                if (gsml3_session_timer_start(s, GSML3_TIMER_T3101) != 1) ++failures;
                if (gsml3_registry_remove(r, s) != 1) ++failures;
            }
        });
    }
    for (auto& th : threads) th.join();
    EXPECT_EQ(failures.load(), 0);
    EXPECT_EQ(gsml3_registry_count(r), 0u);
    gsml3_registry_free(r);
}

// ── BTS stack: orchestrator / responses ────────────────────────────────

namespace {

// Build a CM Service Request (LocationUpdate) message handle. Returns
// nullptr when the wire bytes cannot be produced or reparsed; callers
// assert non-null (gtest ASSERT macros cannot be used in a function that
// returns a value: they expand to an early `return`).
gsml3_message* makeCmServiceRequestLU(uint32_t tmsi) {
    auto msg = L3CMServiceRequest::builder()
        .serviceType(L3CMServiceType{L3CMServiceType::TypeCode::LocationUpdateRequest})
        .mobileIdentity(L3MobileIdentity{tmsi})
        .build();
    ParsedMessage pm{MMM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    if (!bytes) return nullptr;
    return gsml3_parse_l3(bytes.value().data(), bytes.value().size(), nullptr);
}

// Build a CC Setup message handle (MO call).
gsml3_message* makeCcSetup(uint8_t ti, const char* digits) {
    auto msg = L3Setup::builder().ti(ti).calledParty(L3CalledPartyBCDNumber{digits}).build();
    ParsedMessage pm{CCM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    if (!bytes) return nullptr;
    return gsml3_parse_l3(bytes.value().data(), bytes.value().size(), nullptr);
}

// Build a CC Disconnect message handle (starts the Call Release chain).
gsml3_message* makeCcDisconnect(uint8_t ti) {
    auto msg = L3Disconnect::builder().ti(ti).build();
    ParsedMessage pm{CCM{std::move(msg)}};
    auto bytes = writeL3Bytes(pm);
    if (!bytes) return nullptr;
    return gsml3_parse_l3(bytes.value().data(), bytes.value().size(), nullptr);
}

} // namespace

// Test: full Call Release chain through the C API — mirrors
// CallRelease_UsesDisconnectTI from tests/test_procedure_orchestrator.cpp.
// CC Disconnect starts the chain (detectChainPhase), is terminal with a
// response: the action stays SEND_RESPONSE while the terminal state and
// the thread-local reason are reported in finalResult. Note: a
// wire-serialized LocationUpdate CMServiceRequest cannot drive the LU
// chain instead, because the service-type field on the wire is 4 bits
// (TypeCode::LocationUpdateRequest = 105 does not survive a parse); the
// orchestrator correctly stays idle for it.
TEST(CApiOrchestrator, CallReleaseChain) {
    gsml3_registry* r = gsml3_registry_new(0);
    gsml3_orchestrator* o = gsml3_orchestrator_new();
    ASSERT_NE(r, nullptr);
    ASSERT_NE(o, nullptr);
    gsml3_session* s = gsml3_registry_create_by_tmsi(r, 0x12345678);
    ASSERT_NE(s, nullptr);

    uint8_t buf[512];

    // CC Disconnect (TI=5) -> terminal Release response.
    gsml3_message* disc = makeCcDisconnect(5);
    ASSERT_NE(disc, nullptr);
    gsml3_step_result r1 = gsml3_orchestrator_feed(o, disc, s);
    EXPECT_EQ(r1.action, GSML3_ACTION_SEND_RESPONSE);
    EXPECT_EQ(r1.response_token, GSML3_TOKEN_RELEASE);
    EXPECT_EQ(r1.final_state, GSML3_STATE_COMPLETED);
    EXPECT_EQ(r1.final_type, GSML3_PROC_CALL_RELEASE);
    ASSERT_NE(r1.reason, nullptr);
    EXPECT_STREQ(r1.reason, "release_sent");

    size_t n = gsml3_orchestrator_build_response(o, s, buf, sizeof(buf));
    ASSERT_GT(n, 0u);
    gsml3_message* resp = gsml3_parse_l3(buf, n, nullptr);
    ASSERT_NE(resp, nullptr);
    EXPECT_STREQ(gsml3_message_name(resp), "Release");
    // The Release is built from the Disconnect's real TI (session
    // ResponseContext, never a fabricated value).
    EXPECT_EQ(gsml3_message_ti(resp), 5);
    gsml3_message_free(resp);

    // The chain is terminal: phase is idle.
    EXPECT_EQ(gsml3_orchestrator_chain_phase(o), GSML3_PROC_UNKNOWN);
    EXPECT_EQ(gsml3_orchestrator_take_retransmit(o), GSML3_TOKEN_NONE);

    gsml3_message_free(disc);
    gsml3_orchestrator_free(o);
    gsml3_registry_free(r);
}

// Test: the wire service type of a CMServiceRequest is truncated to its
// 4-bit field on re-parse; the orchestrator must stay idle (no chain, no
// response) for such requests — the LocationUpdate chain is driven by
// directly constructed C++ messages in the C++ test suite.
TEST(CApiOrchestrator, LURequest_NotChainStart_OverWire) {
    gsml3_orchestrator* o = gsml3_orchestrator_new();
    ASSERT_NE(o, nullptr);
    gsml3_registry* r = gsml3_registry_new(0);
    ASSERT_NE(r, nullptr);
    gsml3_session* s = gsml3_registry_create_by_tmsi(r, 0x12345678);
    ASSERT_NE(s, nullptr);

    // A re-parsed LocationUpdate CMServiceRequest: the service type is
    // read back from the 4-bit wire field, so it no longer matches the
    // chain-start policy (LU = 105 / MO call = 1).
    gsml3_message* cmReq = makeCmServiceRequestLU(0x12345678);
    ASSERT_NE(cmReq, nullptr);
    gsml3_step_result res = gsml3_orchestrator_feed(o, cmReq, s);
    EXPECT_EQ(res.action, GSML3_ACTION_CONTINUE);
    EXPECT_EQ(res.response_token, GSML3_TOKEN_NONE);
    EXPECT_EQ(gsml3_orchestrator_chain_phase(o), GSML3_PROC_UNKNOWN);
    EXPECT_EQ(gsml3_orchestrator_take_retransmit(o), GSML3_TOKEN_NONE);

    gsml3_message_free(cmReq);
    gsml3_orchestrator_free(o);
    gsml3_registry_free(r);
}

// Test: MO Call Setup chain through the C API — mirrors
// examples/example_reference_bts.cpp simulateMOCallSetupChain().
TEST(CApiOrchestrator, MOCallSetupChain) {
    gsml3_registry* r = gsml3_registry_new(0);
    gsml3_orchestrator* o = gsml3_orchestrator_new();
    ASSERT_NE(r, nullptr);
    ASSERT_NE(o, nullptr);
    gsml3_session* s = gsml3_registry_create_by_tmsi(r, 0x87654321);
    ASSERT_NE(s, nullptr);

    // Step 1: CMServiceRequest (MO call) -> CMServiceAccept.
    auto cmReq = L3CMServiceRequest::builder()
        .serviceType(L3CMServiceType{L3CMServiceType::TypeCode::MobileOriginatedCall})
        .mobileIdentity(L3MobileIdentity{0x87654321})
        .build();
    ParsedMessage pm{MMM{std::move(cmReq)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
    gsml3_message* cmReqMsg = gsml3_parse_l3(bytes.value().data(), bytes.value().size(), nullptr);
    ASSERT_NE(cmReqMsg, nullptr);
    gsml3_step_result r1 = gsml3_orchestrator_feed(o, cmReqMsg, s);
    EXPECT_EQ(r1.response_token, GSML3_TOKEN_CM_SERVICE_ACCEPT);
    gsml3_message_free(cmReqMsg);

    // Step 2: CC Setup -> a response token (CallProceeding or later).
    gsml3_message* setup = makeCcSetup(3, "123456789");
    ASSERT_NE(setup, nullptr);
    gsml3_step_result r2 = gsml3_orchestrator_feed(o, setup, s);
    EXPECT_NE(r2.response_token, GSML3_TOKEN_NONE);
    uint8_t buf[512];
    size_t n = gsml3_orchestrator_build_response(o, s, buf, sizeof(buf));
    ASSERT_GT(n, 0u);
    gsml3_message* resp = gsml3_parse_l3(buf, n, nullptr);
    ASSERT_NE(resp, nullptr);
    gsml3_message_free(resp);
    gsml3_message_free(setup);

    gsml3_orchestrator_free(o);
    gsml3_registry_free(r);
}

// Test: MO Call Setup procedure timer — after the MS Setup message the
// active CallSetupMO procedure runs T3101 (3 s). Ticks inside the window
// are not failures and queue nothing on the retransmission channel; an
// expiry fails the procedure and resets the chain to idle. (The C++ test
// suite covers the identity phase's T3102 retransmissions directly, where
// a wire message is not needed to enter the phase.)
TEST(CApiOrchestrator, CallSetupMO_T3101_Timeout) {
    gsml3_registry* r = gsml3_registry_new(0);
    gsml3_orchestrator* o = gsml3_orchestrator_new();
    ASSERT_NE(r, nullptr);
    ASSERT_NE(o, nullptr);
    gsml3_session* s = gsml3_registry_create_by_tmsi(r, 0x87654321);
    ASSERT_NE(s, nullptr);

    // Step 1: CMServiceRequest (MO call) -> CMServiceAccept.
    auto cmReq = L3CMServiceRequest::builder()
        .serviceType(L3CMServiceType{L3CMServiceType::TypeCode::MobileOriginatedCall})
        .mobileIdentity(L3MobileIdentity{0x87654321})
        .build();
    ParsedMessage pm{MMM{std::move(cmReq)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
    gsml3_message* cmReqMsg = gsml3_parse_l3(bytes.value().data(), bytes.value().size(), nullptr);
    ASSERT_NE(cmReqMsg, nullptr);
    gsml3_step_result r1 = gsml3_orchestrator_feed(o, cmReqMsg, s);
    EXPECT_EQ(r1.response_token, GSML3_TOKEN_CM_SERVICE_ACCEPT);
    gsml3_message_free(cmReqMsg);

    // Step 2: CC Setup -> CallProceeding; the procedure starts T3101 (3 s).
    gsml3_message* setup = makeCcSetup(3, "123456789");
    ASSERT_NE(setup, nullptr);
    gsml3_step_result r2 = gsml3_orchestrator_feed(o, setup, s);
    EXPECT_EQ(r2.response_token, GSML3_TOKEN_CALL_PROCEEDING);
    gsml3_message_free(setup);

    // Inside the window: no failures, nothing retransmitted.
    EXPECT_EQ(gsml3_orchestrator_tick(o, 1000), 0u);
    EXPECT_EQ(gsml3_orchestrator_take_retransmit(o), GSML3_TOKEN_NONE);

    // Expiry: the chain times out (one failure) and resets to idle.
    EXPECT_EQ(gsml3_orchestrator_tick(o, 2500), 1u);
    EXPECT_EQ(gsml3_orchestrator_chain_phase(o), GSML3_PROC_UNKNOWN);
    EXPECT_EQ(gsml3_orchestrator_take_retransmit(o), GSML3_TOKEN_NONE);

    gsml3_orchestrator_free(o);
    gsml3_registry_free(r);
}

// Test: CM Service Requests with a service type other than
// LocationUpdate / MobileOriginatedCall are not handled by the
// orchestrator: no chain, no response token.
TEST(CApiOrchestrator, UnsupportedServiceType_Ignored) {
    gsml3_registry* r = gsml3_registry_new(0);
    gsml3_orchestrator* o = gsml3_orchestrator_new();
    ASSERT_NE(r, nullptr);
    ASSERT_NE(o, nullptr);
    gsml3_session* s = gsml3_registry_create_by_tmsi(r, 0x12345678);
    ASSERT_NE(s, nullptr);

    auto cmReq = L3CMServiceRequest::builder()
        .serviceType(L3CMServiceType{L3CMServiceType::TypeCode::ShortMessage})
        .mobileIdentity(L3MobileIdentity{0x12345678})
        .build();
    ParsedMessage pm{MMM{std::move(cmReq)}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
    gsml3_message* cmReqMsg = gsml3_parse_l3(bytes.value().data(), bytes.value().size(), nullptr);
    ASSERT_NE(cmReqMsg, nullptr);
    gsml3_step_result res = gsml3_orchestrator_feed(o, cmReqMsg, s);
    EXPECT_EQ(res.action, GSML3_ACTION_CONTINUE);
    EXPECT_EQ(res.response_token, GSML3_TOKEN_NONE);
    EXPECT_EQ(gsml3_orchestrator_chain_phase(o), GSML3_PROC_UNKNOWN);
    EXPECT_EQ(gsml3_orchestrator_take_retransmit(o), GSML3_TOKEN_NONE);

    gsml3_message_free(cmReqMsg);
    gsml3_orchestrator_free(o);
    gsml3_registry_free(r);
}

// Test: every standalone response builder produces bytes that parse back
// with the expected message name (cross-check with the C++ API). Each
// builder is invoked and parsed immediately: all builders write into one
// caller buffer, so a message must be consumed before the next build.
TEST(CApiResponse, Builders_ParseBack) {
    uint8_t buf[512];
    std::array<uint8_t, 16> kRand16{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};

    const char* names[] = {
        "CMServiceAccept",
        "CMServiceReject",
        "IdentityRequest",
        "AuthenticationRequest",
        "LocationUpdatingAccept",
        "LocationUpdatingReject",
        "TMSIReallocationCommand",
        "ChannelRelease",
        "CipheringModeCommand",
        "PhysicalInformation",
        "ImmediateAssignment",
        "AssignmentCommand",
        "CallProceeding",
        "Alerting",
        "Connect",
        "ConnectAcknowledge",
        "Disconnect",
        "Release",
        "ReleaseComplete",
        "Setup",
    };
    std::vector<std::function<size_t(uint8_t*, size_t)>> builds = {
        [](uint8_t* b, size_t m) { return gsml3_response_build_cm_service_accept(b, m); },
        [](uint8_t* b, size_t m) { return gsml3_response_build_cm_service_reject(b, m, 0x03); },
        [](uint8_t* b, size_t m) { return gsml3_response_build_identity_request(b, m, GSML3_ID_IMSI); },
        [rand16 = kRand16.data()](uint8_t* b, size_t m) {
            return gsml3_response_build_authentication_request(b, m, rand16); },
        [](uint8_t* b, size_t m) {
            return gsml3_response_build_location_updating_accept(b, m, "244", "05", 0x1234, 1, 0xDEADBEEF); },
        [](uint8_t* b, size_t m) {
            return gsml3_response_build_location_updating_reject(b, m, 0x03); },
        [](uint8_t* b, size_t m) {
            return gsml3_response_build_tmsi_reallocation_command(b, m, "244", "05", 0x1234, 0x11111111); },
        [](uint8_t* b, size_t m) { return gsml3_response_build_channel_release(b, m, 0); },
        [](uint8_t* b, size_t m) { return gsml3_response_build_ciphering_mode_command(b, m, 1); },
        [](uint8_t* b, size_t m) { return gsml3_response_build_physical_information(b, m, 7); },
        [](uint8_t* b, size_t m) {
            return gsml3_response_build_immediate_assignment(b, m, 1 /* SDCCH */, 0, 0, 5120, 0); },
        [](uint8_t* b, size_t m) {
            return gsml3_response_build_assignment_command(b, m, 2 /* TCHF */, 0, 0, 5120); },
        [](uint8_t* b, size_t m) { return gsml3_response_build_call_proceeding(b, m, 3); },
        [](uint8_t* b, size_t m) { return gsml3_response_build_alerting(b, m, 3); },
        [](uint8_t* b, size_t m) { return gsml3_response_build_connect(b, m, 3); },
        [](uint8_t* b, size_t m) { return gsml3_response_build_connect_acknowledge(b, m, 3); },
        [](uint8_t* b, size_t m) { return gsml3_response_build_disconnect(b, m, 3, 16); },
        [](uint8_t* b, size_t m) { return gsml3_response_build_release(b, m, 3, 16); },
        [](uint8_t* b, size_t m) { return gsml3_response_build_release_complete(b, m, 3); },
        [](uint8_t* b, size_t m) { return gsml3_response_build_setup(b, m, "123456789", 3); },
    };
    for (size_t i = 0; i < builds.size(); ++i) {
        size_t n = builds[i](buf, sizeof(buf));
        ASSERT_GT(n, 0u) << names[i] << ": " << gsml3_last_error();
        gsml3_message* m = gsml3_parse_l3(buf, n, nullptr);
        ASSERT_NE(m, nullptr) << names[i];
        EXPECT_STREQ(gsml3_message_name(m), names[i]) << names[i];
        gsml3_message_free(m);
    }
}

// Test: build_response_from_token reads the session's ResponseContext;
// a missing parameter returns 0 (no fabricated values).
TEST(CApiResponse, BuildFromToken) {
    gsml3_registry* r = gsml3_registry_new(0);
    gsml3_orchestrator* o = gsml3_orchestrator_new();
    ASSERT_NE(r, nullptr);
    ASSERT_NE(o, nullptr);
    gsml3_session* s = gsml3_registry_create_by_tmsi(r, 0x12345678);
    ASSERT_NE(s, nullptr);

    uint8_t buf[512];

    // Missing required parameter: AuthenticationRequest needs the session's
    // RAND, which has not been fed yet — 0 (never fabricated values).
    EXPECT_EQ(gsml3_response_build_from_token(GSML3_TOKEN_AUTHENTICATION_REQUEST,
                                              s, buf, sizeof(buf)), 0u)
        << "no RAND in the session context yet";

    // After a CMServiceRequest(LU) the CMServiceAccept token builds.
    gsml3_message* cmReq = makeCmServiceRequestLU(0x12345678);
    ASSERT_NE(cmReq, nullptr);
    [[maybe_unused]] auto r1 = gsml3_orchestrator_feed(o, cmReq, s);
    size_t n = gsml3_response_build_from_token(GSML3_TOKEN_CM_SERVICE_ACCEPT,
                                               s, buf, sizeof(buf));
    ASSERT_GT(n, 0u);
    gsml3_message* resp = gsml3_parse_l3(buf, n, nullptr);
    ASSERT_NE(resp, nullptr);
    EXPECT_STREQ(gsml3_message_name(resp), "CMServiceAccept");
    gsml3_message_free(resp);

    gsml3_message_free(cmReq);
    gsml3_orchestrator_free(o);
    gsml3_registry_free(r);
}

// Test: NULL safety of the orchestrator/response API.
TEST(CApiOrchestrator, NullSafety) {
    gsml3_orchestrator_free(nullptr);
    gsml3_step_result d = gsml3_orchestrator_feed(nullptr, nullptr, nullptr);
    EXPECT_EQ(d.action, GSML3_ACTION_CONTINUE);
    EXPECT_EQ(d.response_token, GSML3_TOKEN_NONE);
    EXPECT_EQ(d.final_type, GSML3_PROC_UNKNOWN);
    EXPECT_EQ(d.reason, nullptr);
    EXPECT_EQ(gsml3_orchestrator_tick(nullptr, 1), 0u);
    uint8_t buf[8];
    EXPECT_EQ(gsml3_orchestrator_build_response(nullptr, nullptr, buf, sizeof(buf)), 0u);
    EXPECT_EQ(gsml3_orchestrator_take_retransmit(nullptr), GSML3_TOKEN_NONE);
    gsml3_orchestrator_cancel_all(nullptr);
    EXPECT_EQ(gsml3_orchestrator_chain_phase(nullptr), GSML3_PROC_UNKNOWN);
    EXPECT_EQ(gsml3_response_build_cm_service_accept(nullptr, 0), 0u);
    EXPECT_EQ(gsml3_response_build_from_token(GSML3_TOKEN_NONE, nullptr, buf, sizeof(buf)), 0u);
}

// ── Typed access: curated message fields / builders ───────────────────

// Test: every typed builder round-trips through the C API: build ->
// parse -> the key fields read back with the typed getters.
TEST(CApiTyped, Builders_RoundTrip) {
    uint8_t buf[512];

    {
        size_t n = gsml3_build_channel_release(buf, sizeof(buf), 4);
        ASSERT_GT(n, 0u);
        gsml3_message* m = gsml3_parse_l3(buf, n, nullptr);
        ASSERT_NE(m, nullptr);
        EXPECT_STREQ(gsml3_message_name(m), "ChannelRelease");
        EXPECT_EQ(gsml3_msg_channel_release_cause(m), 4);
        gsml3_message_free(m);
    }
    {
        size_t n = gsml3_build_channel_request(buf, sizeof(buf), 0x55);
        ASSERT_GT(n, 0u);
        gsml3_message* m = gsml3_parse_l3(buf, n, nullptr);
        ASSERT_NE(m, nullptr);
        EXPECT_STREQ(gsml3_message_name(m), "ChannelRequest");
        EXPECT_EQ(gsml3_msg_channel_request_ra(m), 0x55);
        gsml3_message_free(m);
    }
    {
        // The channel description carries ARFCN in a 10-bit field (this
        // library's wire encoding), so the test value must fit.
        size_t n = gsml3_build_immediate_assignment(buf, sizeof(buf), 1, 2, 3, 100, 7, 0x55);
        ASSERT_GT(n, 0u);
        gsml3_message* m = gsml3_parse_l3(buf, n, nullptr);
        ASSERT_NE(m, nullptr);
        gsml3_channel ch{};
        ASSERT_EQ(gsml3_msg_immediate_assignment_channel(m, &ch), 0);
        EXPECT_EQ(ch.type_and_offset, 1);
        EXPECT_EQ(ch.tn, 2);
        EXPECT_EQ(ch.tsc, 3);
        EXPECT_EQ(ch.arfcn, 100);
        EXPECT_EQ(gsml3_msg_immediate_assignment_ta(m), 7);
        gsml3_message_free(m);
    }
    {
        size_t n = gsml3_build_assignment_command(buf, sizeof(buf), 2, 0, 0, 50);
        ASSERT_GT(n, 0u);
        gsml3_message* m = gsml3_parse_l3(buf, n, nullptr);
        ASSERT_NE(m, nullptr);
        gsml3_channel ch{};
        ASSERT_EQ(gsml3_msg_assignment_command_channel(m, &ch), 0);
        EXPECT_EQ(ch.arfcn, 50);
        gsml3_message_free(m);
    }
    {
        size_t n = gsml3_build_paging_request_type2(buf, sizeof(buf), 0xAAAA0001, 0xAAAA0002);
        ASSERT_GT(n, 0u);
        gsml3_message* m = gsml3_parse_l3(buf, n, nullptr);
        ASSERT_NE(m, nullptr);
        EXPECT_STREQ(gsml3_message_name(m), "PagingRequestType2");
        EXPECT_EQ(gsml3_msg_paging_request_type2_tmsi(m, 0), 0xAAAA0001u);
        EXPECT_EQ(gsml3_msg_paging_request_type2_tmsi(m, 1), 0xAAAA0002u);
        EXPECT_EQ(gsml3_msg_paging_request_type2_tmsi(m, 2), 0u);  // OOR
        gsml3_message_free(m);
    }
    {
        size_t n = gsml3_build_paging_response(buf, sizeof(buf), GSML3_ID_TMSI, 0x12345678, nullptr);
        ASSERT_GT(n, 0u);
        gsml3_message* m = gsml3_parse_l3(buf, n, nullptr);
        ASSERT_NE(m, nullptr);
        gsml3_mobile_identity id{};
        ASSERT_EQ(gsml3_msg_paging_response_identity(m, &id), 0);
        EXPECT_EQ(id.type, GSML3_ID_TMSI);
        EXPECT_EQ(id.tmsi, 0x12345678u);
        EXPECT_EQ(id.imsi, nullptr);
        gsml3_message_free(m);
    }
    {
        size_t n = gsml3_build_ciphering_mode_command(buf, sizeof(buf), 1);
        ASSERT_GT(n, 0u);
        gsml3_message* m = gsml3_parse_l3(buf, n, nullptr);
        ASSERT_NE(m, nullptr);
        EXPECT_EQ(gsml3_msg_ciphering_mode_command_algorithm(m), 1);
        gsml3_message_free(m);
    }
    {
        size_t n = gsml3_build_cm_service_request(buf, sizeof(buf), 105 /* LU */, GSML3_ID_TMSI, 0x12345678, nullptr);
        ASSERT_GT(n, 0u);
        gsml3_message* m = gsml3_parse_l3(buf, n, nullptr);
        ASSERT_NE(m, nullptr);
        EXPECT_STREQ(gsml3_message_name(m), "CMServiceRequest");
        // The CM service type is a 4-bit field on the wire (this library's
        // L3CMServiceType encoding): LU = 105 round-trips as its low nibble.
        // Cross-check against the direct C++ parse of the same bytes: the
        // C getter must agree with the C++ API value exactly.
        auto cppR = parseL3(std::span<const uint8_t>(buf, n));
        ASSERT_TRUE(cppR);
        const auto* cppM = tryGet<L3CMServiceRequest>(cppR.value());
        ASSERT_NE(cppM, nullptr);
        EXPECT_EQ(gsml3_msg_cm_service_request_service_type(m), (int)cppM->serviceType());
        gsml3_mobile_identity id{};
        ASSERT_EQ(gsml3_msg_cm_service_request_identity(m, &id), 0);
        EXPECT_EQ(id.tmsi, 0x12345678u);
        gsml3_message_free(m);
    }
    {
        size_t n = gsml3_build_cm_service_reject(buf, sizeof(buf), 0x03);
        ASSERT_GT(n, 0u);
        gsml3_message* m = gsml3_parse_l3(buf, n, nullptr);
        ASSERT_NE(m, nullptr);
        EXPECT_EQ(gsml3_msg_cm_service_reject_cause(m), 0x03);
        gsml3_message_free(m);
    }
    {
        size_t n = gsml3_build_identity_request(buf, sizeof(buf), GSML3_ID_IMSI);
        ASSERT_GT(n, 0u);
        gsml3_message* m = gsml3_parse_l3(buf, n, nullptr);
        ASSERT_NE(m, nullptr);
        EXPECT_EQ(gsml3_msg_identity_request_type(m), GSML3_ID_IMSI);
        gsml3_message_free(m);
    }
    {
        const uint8_t rand[16] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
        size_t n = gsml3_build_authentication_request(buf, sizeof(buf), 1, rand);
        ASSERT_GT(n, 0u);
        gsml3_message* m = gsml3_parse_l3(buf, n, nullptr);
        ASSERT_NE(m, nullptr);
        EXPECT_EQ(gsml3_msg_authentication_request_cks(m), 1);
        uint8_t got[16] = {};
        ASSERT_EQ(gsml3_msg_authentication_request_rand(m, got), 0);
        EXPECT_EQ(0, std::memcmp(got, rand, 16));
        gsml3_message_free(m);
    }
    {
        size_t n = gsml3_build_authentication_response(buf, sizeof(buf), 0xABCD1234);
        ASSERT_GT(n, 0u);
        gsml3_message* m = gsml3_parse_l3(buf, n, nullptr);
        ASSERT_NE(m, nullptr);
        EXPECT_EQ(gsml3_msg_authentication_response_sres(m), 0xABCD1234);
        gsml3_message_free(m);
    }
    {
        size_t n = gsml3_build_location_updating_request(buf, sizeof(buf), 0, GSML3_ID_TMSI, 0x12345678, nullptr, 244, 5, 0x1234);
        ASSERT_GT(n, 0u);
        gsml3_message* m = gsml3_parse_l3(buf, n, nullptr);
        ASSERT_NE(m, nullptr);
        EXPECT_STREQ(gsml3_message_name(m), "LocationUpdatingRequest");
        gsml3_lai lai{};
        ASSERT_EQ(gsml3_msg_location_updating_request_lai(m, &lai), 0);
        EXPECT_EQ(lai.mcc, 244);
        EXPECT_EQ(lai.mnc, 5);
        EXPECT_EQ(lai.lac, 0x1234);
        gsml3_message_free(m);
    }
    {
        size_t n = gsml3_build_setup(buf, sizeof(buf), 3, "123456789");
        ASSERT_GT(n, 0u);
        gsml3_message* m = gsml3_parse_l3(buf, n, nullptr);
        ASSERT_NE(m, nullptr);
        EXPECT_STREQ(gsml3_message_name(m), "Setup");
        EXPECT_EQ(gsml3_msg_setup_ti(m), 3);
        EXPECT_EQ(gsml3_msg_setup_have_called_party(m), 1);
        EXPECT_STREQ(gsml3_msg_setup_called_number(m), "123456789");
        gsml3_message_free(m);
    }
    {
        size_t n = gsml3_build_disconnect(buf, sizeof(buf), 3, 16);
        ASSERT_GT(n, 0u);
        gsml3_message* m = gsml3_parse_l3(buf, n, nullptr);
        ASSERT_NE(m, nullptr);
        EXPECT_EQ(gsml3_msg_disconnect_ti(m), 3);
        EXPECT_EQ(gsml3_msg_disconnect_cause(m), 16);
        gsml3_message_free(m);
    }
    {
        const uint8_t rpdu[] = {0x11, 0x22, 0x33};
        size_t n = gsml3_build_cp_data(buf, sizeof(buf), rpdu, sizeof(rpdu));
        ASSERT_GT(n, 0u);
        gsml3_message* m = gsml3_parse_l3(buf, n, nullptr);
        ASSERT_NE(m, nullptr);
        uint8_t got[16] = {};
        EXPECT_EQ(gsml3_msg_cp_data_rpdu(m, got, sizeof(got)), 3u);
        EXPECT_EQ(0, std::memcmp(got, rpdu, 3));
        gsml3_message_free(m);
    }
    {
        const uint8_t fac[] = {0xA0, 0x81};
        size_t n = gsml3_build_sup_serv_facility(buf, sizeof(buf), 5, fac, sizeof(fac));
        ASSERT_GT(n, 0u);
        gsml3_message* m = gsml3_parse_l3(buf, n, nullptr);
        ASSERT_NE(m, nullptr);
        EXPECT_EQ(gsml3_msg_sup_serv_facility_ti(m), 5);
        uint8_t got[16] = {};
        EXPECT_EQ(gsml3_msg_sup_serv_facility_data(m, got, sizeof(got)), 2u);
        gsml3_message_free(m);
    }
}

// Test: typed getters on a message of the wrong type return sentinels
// (no crash, documented behavior).
TEST(CApiTyped, WrongType_Sentinels) {
    gsml3_message* m = gsml3_parse_l3_hex("60 0D 00", nullptr);  // ChannelRelease
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(gsml3_msg_setup_ti(m), -1);
    EXPECT_EQ(gsml3_msg_disconnect_cause(m), -1);
    EXPECT_EQ(gsml3_msg_cm_service_reject_cause(m), -1);
    EXPECT_EQ(gsml3_msg_paging_request_type2_tmsi(m, 0), 0u);
    EXPECT_EQ(gsml3_msg_setup_called_number(m), nullptr);
    uint8_t buf[8];
    EXPECT_EQ(gsml3_msg_cp_data_rpdu(m, buf, sizeof(buf)), 0u);
    gsml3_channel ch{};
    EXPECT_EQ(gsml3_msg_immediate_assignment_channel(m, &ch), -1);
    gsml3_mobile_identity id{};
    EXPECT_EQ(gsml3_msg_paging_response_identity(m, &id), -1);
    gsml3_lai lai{};
    EXPECT_EQ(gsml3_msg_location_updating_request_lai(m, &lai), -1);
    gsml3_message_free(m);
}

// Test: typed getters agree with the C++ tryGet values (cross-check).
TEST(CApiTyped, Getters_AgreeWithCpp) {
    // Build via C++, parse via C, compare.
    auto msg = L3Disconnect::builder().ti(2).cause(CCCause::User_Busy).build();
    ParsedMessage pm{CCM{msg}};
    auto bytes = writeL3Bytes(pm);
    ASSERT_TRUE(bytes);
    gsml3_message* m = gsml3_parse_l3(bytes.value().data(), bytes.value().size(), nullptr);
    ASSERT_NE(m, nullptr);
    const auto* cppMsg = tryGet<L3Disconnect>(pm);
    ASSERT_NE(cppMsg, nullptr);
    EXPECT_EQ(gsml3_msg_disconnect_ti(m), (int)cppMsg->ti());
    EXPECT_EQ(gsml3_msg_disconnect_cause(m), (int)cppMsg->cause());
    gsml3_message_free(m);
}
