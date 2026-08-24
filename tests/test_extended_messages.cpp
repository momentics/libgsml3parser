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

// Extended PD (0x0e) and TestProcedure PD (0x0f) message tests.
// Reference: GSM 04.08 §10.2 - Protocol Discriminator values

#include <gtest/gtest.h>
#include <gsml3parser/parser.h>
#include <gsml3parser/l3header.h>
#include <gsml3parser/extended/l3extendedmessages.h>
#include <gsml3parser/testproc/l3testproceduremessages.h>
#include <gsml3parser/visitor.h>

using namespace gsml3parser;

// ── Extended PD (0x0e) tests ──────────────────────────────────────────

TEST(ExtendedMessageTest, Parse_Golden) {
    // PD=0x0e(Extended), MTI=0x42, body=0xAA 0xBB 0xCC
    uint8_t data[] = {0xE4, 0x42, 0xAA, 0xBB, 0xCC};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messagePD(*msg), L3PD::Extended);
    EXPECT_EQ(messageMTI(*msg), 0x42);
    EXPECT_EQ(messageName(*msg), "ExtendedMessage");
    auto* ext = tryGet<L3ExtendedMessage>(*msg);
    ASSERT_TRUE(ext);
    EXPECT_EQ(ext->mti(), 0x42);
    EXPECT_EQ(ext->body().size(), 3u);
    EXPECT_EQ(ext->body()[0], 0xAA);
    EXPECT_EQ(ext->body()[1], 0xBB);
    EXPECT_EQ(ext->body()[2], 0xCC);
}

TEST(ExtendedMessageTest, Parse_EmptyBody) {
    // PD=0x0e(Extended), MTI=0x01, no body octets
    uint8_t data[] = {0xE0, 0x01};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messagePD(*msg), L3PD::Extended);
    EXPECT_EQ(messageMTI(*msg), 0x01);
    auto* ext = tryGet<L3ExtendedMessage>(*msg);
    ASSERT_TRUE(ext);
    EXPECT_EQ(ext->body().size(), 0u);
}

TEST(ExtendedMessageTest, RoundTrip) {
    L3ExtendedMessage orig(0x55);
    orig.text(std::cout);
    EXTENDED extVariant(std::move(orig));
    ParsedMessage pm(std::move(extVariant));
    auto hex = writeL3Hex(pm);
    ASSERT_TRUE(hex);
    auto reparsed = parseL3Hex(*hex);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messagePD(*reparsed), L3PD::Extended);
    EXPECT_EQ(messageMTI(*reparsed), 0x55);
    EXPECT_EQ(messageName(*reparsed), "ExtendedMessage");
}

TEST(ExtendedMessageTest, RoundTrip_WithBody) {
    L3ExtendedMessage orig(0x7F);
    // Manually set body via parse round-trip
    uint8_t data[] = {0xE0, 0x7F, 0xDE, 0xAD};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    auto hex = writeL3Hex(*msg);
    ASSERT_TRUE(hex);
    auto reparsed = parseL3Hex(*hex);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messagePD(*reparsed), L3PD::Extended);
    EXPECT_EQ(messageMTI(*reparsed), 0x7F);
    auto* ext = tryGet<L3ExtendedMessage>(*reparsed);
    ASSERT_TRUE(ext);
    EXPECT_EQ(ext->body().size(), 2u);
    EXPECT_EQ(ext->body()[0], 0xDE);
    EXPECT_EQ(ext->body()[1], 0xAD);
}

TEST(VisitorTests, TryGet_ExtendedMessage) {
    L3ExtendedMessage orig(0x33);
    EXTENDED extVariant(std::move(orig));
    ParsedMessage pm(std::move(extVariant));
    EXPECT_NE(tryGet<L3ExtendedMessage>(pm), nullptr);
    EXPECT_EQ(messageName(pm), "ExtendedMessage");
    EXPECT_EQ(messagePD(pm), L3PD::Extended);
}

// ── TestProcedure PD (0x0f) tests ─────────────────────────────────────

TEST(TestProcedureMessageTest, Parse_Golden) {
    // PD=0x0f(TestProcedure), MTI=0xA1, body=0x11 0x22 0x33 0x44
    uint8_t data[] = {0xF0, 0xA1, 0x11, 0x22, 0x33, 0x44};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messagePD(*msg), L3PD::TestProcedure);
    EXPECT_EQ(messageMTI(*msg), 0xA1);
    EXPECT_EQ(messageName(*msg), "TestProcedureMessage");
    auto* tp = tryGet<L3TestProcedureMessage>(*msg);
    ASSERT_TRUE(tp);
    EXPECT_EQ(tp->mti(), 0xA1);
    EXPECT_EQ(tp->body().size(), 4u);
    EXPECT_EQ(tp->body()[0], 0x11);
    EXPECT_EQ(tp->body()[1], 0x22);
    EXPECT_EQ(tp->body()[2], 0x33);
    EXPECT_EQ(tp->body()[3], 0x44);
}

TEST(TestProcedureMessageTest, Parse_EmptyBody) {
    // PD=0x0f(TestProcedure), MTI=0x00, no body
    uint8_t data[] = {0xF0, 0x00};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    EXPECT_EQ(messagePD(*msg), L3PD::TestProcedure);
    EXPECT_EQ(messageMTI(*msg), 0x00);
    auto* tp = tryGet<L3TestProcedureMessage>(*msg);
    ASSERT_TRUE(tp);
    EXPECT_EQ(tp->body().size(), 0u);
}

TEST(TestProcedureMessageTest, RoundTrip) {
    L3TestProcedureMessage orig(0x99);
    TESTPROC tpVariant(std::move(orig));
    ParsedMessage pm(std::move(tpVariant));
    auto hex = writeL3Hex(pm);
    ASSERT_TRUE(hex);
    auto reparsed = parseL3Hex(*hex);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messagePD(*reparsed), L3PD::TestProcedure);
    EXPECT_EQ(messageMTI(*reparsed), 0x99);
    EXPECT_EQ(messageName(*reparsed), "TestProcedureMessage");
}

TEST(TestProcedureMessageTest, RoundTrip_WithBody) {
    uint8_t data[] = {0xF0, 0xCC, 0xFE, 0xED, 0xFA, 0xCE};
    auto msg = parseL3(std::span<const uint8_t>(data));
    ASSERT_TRUE(msg);
    auto hex = writeL3Hex(*msg);
    ASSERT_TRUE(hex);
    auto reparsed = parseL3Hex(*hex);
    ASSERT_TRUE(reparsed);
    EXPECT_EQ(messagePD(*reparsed), L3PD::TestProcedure);
    EXPECT_EQ(messageMTI(*reparsed), 0xCC);
    auto* tp = tryGet<L3TestProcedureMessage>(*reparsed);
    ASSERT_TRUE(tp);
    EXPECT_EQ(tp->body().size(), 4u);
    EXPECT_EQ(tp->body()[0], 0xFE);
    EXPECT_EQ(tp->body()[1], 0xED);
    EXPECT_EQ(tp->body()[2], 0xFA);
    EXPECT_EQ(tp->body()[3], 0xCE);
}

TEST(VisitorTests, TryGet_TestProcedureMessage) {
    L3TestProcedureMessage orig(0x42);
    TESTPROC tpVariant(std::move(orig));
    ParsedMessage pm(std::move(tpVariant));
    EXPECT_NE(tryGet<L3TestProcedureMessage>(pm), nullptr);
    EXPECT_EQ(messageName(pm), "TestProcedureMessage");
    EXPECT_EQ(messagePD(pm), L3PD::TestProcedure);
}

// ── L3Header parse for Extended/TestProcedure PDs ─────────────────────

TEST(ExtendedHeaderTest, ParseL3Header_Extended) {
    uint8_t data[] = {0xE0, 0x55};
    auto hdr = parseL3Header(std::span<const uint8_t>(data));
    ASSERT_TRUE(hdr);
    EXPECT_EQ(hdr.value().pd, L3PD::Extended);
    EXPECT_EQ(hdr.value().mti, 0x55);
}

TEST(ExtendedHeaderTest, ParseL3Header_TestProcedure) {
    uint8_t data[] = {0xF0, 0xAA};
    auto hdr = parseL3Header(std::span<const uint8_t>(data));
    ASSERT_TRUE(hdr);
    EXPECT_EQ(hdr.value().pd, L3PD::TestProcedure);
    EXPECT_EQ(hdr.value().mti, 0xAA);
}
