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
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include <gsml3parser/gsml3parser_c.h>
#include <gsml3parser/message_types.h>
#include <gsml3parser/parser.h>
#include <gsml3parser/visitor.h>
#include <gsml3parser/abis/rsl_parser.h>
#include <gsml3parser/abis/rsl_builder.h>

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
