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

#include <gtest/gtest.h>
#include <gsml3parser/lapdm_frame.h>

using namespace gsml3parser;
using namespace gsml3parser::lapdm;

// Helper to dereference Expected<LAPDmFrame> after ASSERT_TRUE check.
#define FRAME_FIELD(frame, field) ((*(frame)).field)

// ── LAPDmFrame Encoding Tests (GSM 04.06) ─────────────────────────────

// GSM 04.06 4.2.1: Address field encoding — SAPI0, CR=0, EA=1 -> 0x01
TEST(LAPDmFrameTest, AddressField_EncodeDecode_SAPI0) {
    auto addr = LAPDmAddressField(SAPI::SAPI0, false, true);
    EXPECT_EQ(addr.encode(), 0x01u);
    auto decoded = LAPDmAddressField::decode(0x01u);
    EXPECT_EQ(decoded.sapi, SAPI::SAPI0);
    EXPECT_FALSE(decoded.command);
    EXPECT_TRUE(decoded.ea);
}

// GSM 04.06 4.2.1: Address field encoding — SAPI3, CR=1, EA=1 -> 0x39
TEST(LAPDmFrameTest, AddressField_EncodeDecode_SAPI3_CR1) {
    auto addr = LAPDmAddressField(SAPI::SAPI3, true, true);
    EXPECT_EQ(addr.encode(), 0x39u);
}

// GSM 04.06 5.2.1: UI frame with SAPI0, command, L3 payload
TEST(LAPDmFrameTest, UI_Frame_Encode) {
    uint8_t payload[] = {0x60, 0x0D, 0x00}; // Channel Release
    auto frame = makeUIFrame(SAPI::SAPI0, true, std::span(payload));
    auto encoded = encodeFrame(frame);
    EXPECT_EQ(encoded[0], 0x09u); // SAPI0, CR=1, EA=1
    EXPECT_EQ(encoded[1], 0x03u); // UI
    EXPECT_EQ(encoded.size(), 5u); // 2 header + 3 payload
}

// GSM 04.06 5.4.1: SABME command, PF=1, no payload
TEST(LAPDmFrameTest, SABME_Frame_Encode) {
    auto frame = makeSABMEFrame(SAPI::SAPI0, true, std::span<const uint8_t>{});
    auto encoded = encodeFrame(frame);
    EXPECT_EQ(encoded[0], 0x09u); // SAPI0, CR=1
    EXPECT_EQ(encoded[1], 0x2Fu); // SABME
}

// GSM 04.06 5.4.1.2: UA response, PF=1
TEST(LAPDmFrameTest, UA_Frame_Encode) {
    auto frame = makeUAFrame(SAPI::SAPI0, true, std::span<const uint8_t>{});
    auto encoded = encodeFrame(frame);
    EXPECT_EQ(encoded[0], 0x01u); // SAPI0, CR=0 (response)
    EXPECT_EQ(encoded[1], 0x63u); // UA
}

// GSM 04.06 5.5.2: I-frame with NR=0, NS=1, P/F=0, M=1 (Message complete — last segment)
TEST(LAPDmFrameTest, I_Frame_EncodeDecode) {
    uint8_t payload[] = {0x01, 0x02, 0x03};
    auto frame = makeIFrame(SAPI::SAPI0, true, 0, 1, false, true, std::span(payload));
    auto encoded = encodeFrame(frame);
    EXPECT_EQ(encoded[0], 0x09u); // address: SAPI0, CR=1
    EXPECT_EQ(encoded[1], 0x02u); // control: NR=0, P/F=0, NS=1 -> 000 0 001 0 = 0x02
    EXPECT_EQ(encoded[2], 0x83u); // length: M=1, len=3 -> 10000011 = 0x83

    auto result = LAPDmFrame::decode(std::span(encoded));
    ASSERT_TRUE(result);
    const auto& f = *result;
    EXPECT_EQ(f.format, LAPDmControlFormat::I_Format);
    EXPECT_EQ(f.nr, 0u);
    EXPECT_EQ(f.ns, 1u);
}

// GSM 04.06 5.3.2: RR frame with NR=3, PF=1
TEST(LAPDmFrameTest, RR_Frame_EncodeDecode) {
    auto frame = makeRRFrame(SAPI::SAPI0, 3, true);
    auto encoded = encodeFrame(frame);
    EXPECT_EQ(encoded[0], 0x01u); // SAPI0, CR=0 (response)
    // Control: NR=3(011), PF=1, Function=RR(00), Fixed=1 -> 011 1 00 1 = 0x71
    EXPECT_EQ(encoded[1], 0x71u);

    auto result = LAPDmFrame::decode(std::span(encoded));
    ASSERT_TRUE(result);
    EXPECT_EQ((*result).format, LAPDmControlFormat::S_Format);
}

// GSM 04.06 5.3.3: REJ frame with NR=5, PF=0
TEST(LAPDmFrameTest, REJ_Frame_EncodeDecode) {
    auto frame = makeREJFrame(SAPI::SAPI0, 5, false);
    auto encoded = encodeFrame(frame);
    // Control: NR=5(101), PF=0, Function=REJ(10), Fixed=1 -> 101 0 10 1 = 0xAD
    EXPECT_EQ(encoded[1], 0xADu);
}

// GSM 04.06 5.4.4: DISC command, PF=1
TEST(LAPDmFrameTest, DISC_Frame_Encode) {
    auto frame = makeDISCFrame(SAPI::SAPI0, true);
    auto encoded = encodeFrame(frame);
    EXPECT_EQ(encoded[0], 0x09u); // SAPI0, CR=1
    EXPECT_EQ(encoded[1], 0x08u); // DISC (F=1)
}

// GSM 04.06 5.4.6: DM response, PF=1
TEST(LAPDmFrameTest, DM_Frame_Encode) {
    auto frame = makeDMFrame(SAPI::SAPI0, true);
    auto encoded = encodeFrame(frame);
    EXPECT_EQ(encoded[0], 0x01u); // SAPI0, CR=0
    EXPECT_EQ(encoded[1], 0x0Fu); // DM
}

// Empty frame should fail decode
TEST(LAPDmFrameTest, Decode_TruncatedInput) {
    auto result = LAPDmFrame::decode(std::span<const uint8_t>{});
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, ParseError::Code::TruncatedInput);
}

// Only 1 byte — not enough for control field
TEST(LAPDmFrameTest, Decode_OnlyAddressByte) {
    uint8_t data[] = {0x01};
    auto result = LAPDmFrame::decode(std::span(data));
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, ParseError::Code::TruncatedInput);
}

// I-frame declares length=10 but only 3 bytes of payload available
TEST(LAPDmFrameTest, IFrame_Length_Mismatch) {
    uint8_t data[] = {0x09, 0x02, 0x0A, 0x01, 0x02, 0x03};
    auto result = LAPDmFrame::decode(std::span(data));
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, ParseError::Code::TruncatedInput);
}

// GSM 04.06 5.4.1.4: SABME with contention resolution payload
TEST(LAPDmFrameTest, SABME_WithPayload) {
    uint8_t payload[] = {0x60, 0x27, 0x04, 0x60, 0x00, 0x12, 0x34, 0x56, 0x78};
    auto frame = makeSABMEFrame(SAPI::SAPI0, true, std::span(payload));
    auto encoded = encodeFrame(frame);
    // Should include length byte + payload after SABME control
    EXPECT_GT(encoded.size(), 3u);

    auto result = LAPDmFrame::decode(std::span(encoded));
    ASSERT_TRUE(result);
    EXPECT_EQ((*result).format, LAPDmControlFormat::U_Format);
    EXPECT_TRUE((*result).hasInfo());
}

// Verify encode/decode round-trip for UI frame with payload
TEST(LAPDmFrameTest, UI_Frame_RoundTrip) {
    uint8_t payload[] = {0x60, 0x0D, 0x00, 0xFF};
    auto frame = makeUIFrame(SAPI::SAPI0, false, std::span(payload));
    auto encoded = encodeFrame(frame);
    auto result = LAPDmFrame::decode(std::span(encoded));
    ASSERT_TRUE(result);
    const auto& f = *result;
    EXPECT_EQ(f.format, LAPDmControlFormat::U_Format);
    EXPECT_EQ(f.uType, LAPDmUFrameType::UI);
    EXPECT_EQ(f.info.size(), 4u);
    EXPECT_EQ(f.info[0], 0x60u);
    EXPECT_EQ(f.info[3], 0xFFu);
}

// Verify encode/decode round-trip for I-frame with M=0 (more segments follow)
TEST(LAPDmFrameTest, I_Frame_RoundTrip_M0) {
    uint8_t payload[] = {0xAA, 0xBB};
    auto frame = makeIFrame(SAPI::SAPI3, true, 2, 3, false, false, std::span(payload));
    auto encoded = encodeFrame(frame);
    auto result = LAPDmFrame::decode(std::span(encoded));
    ASSERT_TRUE(result);
    const auto& f = *result;
    EXPECT_EQ(f.format, LAPDmControlFormat::I_Format);
    EXPECT_EQ(f.nr, 2u);
    EXPECT_EQ(f.ns, 3u);
    EXPECT_FALSE(f.m); // M=0: more segments follow
    EXPECT_EQ(f.info.size(), 2u);
}

// Verify encode/decode round-trip for I-frame with M=1 (Message complete)
TEST(LAPDmFrameTest, I_Frame_RoundTrip_M1) {
    uint8_t payload[] = {0x01, 0x02, 0x03, 0x04};
    auto frame = makeIFrame(SAPI::SAPI0, false, 0, 0, true, true, std::span(payload));
    auto encoded = encodeFrame(frame);
    auto result = LAPDmFrame::decode(std::span(encoded));
    ASSERT_TRUE(result);
    const auto& f = *result;
    EXPECT_EQ(f.format, LAPDmControlFormat::I_Format);
    EXPECT_TRUE(f.m); // M=1: Message complete (last segment)
    EXPECT_TRUE(f.pf);
    EXPECT_EQ(f.info.size(), 4u);
}

// Verify SABME round-trip with contention resolution payload
TEST(LAPDmFrameTest, SABME_WithPayload_RoundTrip) {
    uint8_t payload[] = {0x60, 0x27, 0x04, 0x60, 0x00, 0x12, 0x34};
    auto frame = makeSABMEFrame(SAPI::SAPI0, false, std::span(payload));
    auto encoded = encodeFrame(frame);
    auto result = LAPDmFrame::decode(std::span(encoded));
    ASSERT_TRUE(result);
    const auto& f = *result;
    EXPECT_EQ(f.uType, LAPDmUFrameType::SABME);
    EXPECT_EQ(f.info.size(), 7u);
    for (size_t i = 0; i < 7; ++i) {
        EXPECT_EQ(f.info[i], payload[i]);
    }
}

// Verify UA round-trip with echo payload (contention resolution)
TEST(LAPDmFrameTest, UA_WithEcho_RoundTrip) {
    uint8_t echo[] = {0x60, 0x27, 0x04, 0x60, 0x00};
    auto frame = makeUAFrame(SAPI::SAPI0, true, std::span(echo));
    auto encoded = encodeFrame(frame);
    auto result = LAPDmFrame::decode(std::span(encoded));
    ASSERT_TRUE(result);
    const auto& f = *result;
    EXPECT_EQ(f.uType, LAPDmUFrameType::UA);
    EXPECT_EQ(f.info.size(), 5u);
}

// Verify RR frame decode returns correct NR and PF
TEST(LAPDmFrameTest, RR_Frame_Decode) {
    uint8_t raw[] = {0x01, 0x71}; // SAPI0 response, NR=3, PF=1
    auto result = LAPDmFrame::decode(std::span(raw));
    ASSERT_TRUE(result);
    const auto& f = *result;
    EXPECT_EQ(f.format, LAPDmControlFormat::S_Format);
    EXPECT_EQ(f.sType, LAPDmSFrameType::RR);
    EXPECT_EQ(f.nr, 3u);
    EXPECT_TRUE(f.pf);
}

// Verify REJ frame decode returns correct NR and type
TEST(LAPDmFrameTest, REJ_Frame_Decode) {
    uint8_t raw[] = {0x01, 0xAD}; // SAPI0 response, NR=5, PF=0, REJ
    auto result = LAPDmFrame::decode(std::span(raw));
    ASSERT_TRUE(result);
    const auto& f = *result;
    EXPECT_EQ(f.format, LAPDmControlFormat::S_Format);
    EXPECT_EQ(f.sType, LAPDmSFrameType::REJ);
    EXPECT_EQ(f.nr, 5u);
}

// Verify DM frame decode
TEST(LAPDmFrameTest, DM_Frame_Decode) {
    uint8_t raw[] = {0x01, 0x0F}; // SAPI0 response, DM
    auto result = LAPDmFrame::decode(std::span(raw));
    ASSERT_TRUE(result);
    const auto& f = *result;
    EXPECT_EQ(f.format, LAPDmControlFormat::U_Format);
    EXPECT_EQ(f.uType, LAPDmUFrameType::DM);
}

// Verify DISC frame decode
TEST(LAPDmFrameTest, DISC_Frame_Decode) {
    uint8_t raw[] = {0x09, 0x08}; // SAPI0 command, DISC
    auto result = LAPDmFrame::decode(std::span(raw));
    ASSERT_TRUE(result);
    const auto& f = *result;
    EXPECT_EQ(f.format, LAPDmControlFormat::U_Format);
    EXPECT_EQ(f.uType, LAPDmUFrameType::DISC);
}

// Verify encodeFrameToBuffer with sufficient buffer
TEST(LAPDmFrameTest, EncodeToBuffer_Success) {
    uint8_t payload[] = {0x60, 0x0D};
    auto frame = makeUIFrame(SAPI::SAPI0, true, std::span(payload));
    uint8_t buf[16] = {};
    size_t written = encodeFrameToBuffer(frame, buf, sizeof(buf));
    EXPECT_EQ(written, 4u); // address + control + 2 payload bytes
    EXPECT_EQ(buf[0], 0x09u);
    EXPECT_EQ(buf[1], 0x03u);
    EXPECT_EQ(buf[2], 0x60u);
    EXPECT_EQ(buf[3], 0x0Du);
}

// Verify encodeFrameToBuffer returns 0 when buffer too small
TEST(LAPDmFrameTest, EncodeToBuffer_TooSmall) {
    uint8_t payload[] = {0x60, 0x0D, 0x00};
    auto frame = makeUIFrame(SAPI::SAPI0, true, std::span(payload));
    uint8_t buf[2] = {};
    size_t written = encodeFrameToBuffer(frame, buf, sizeof(buf));
    EXPECT_EQ(written, 0u); // Buffer too small for address + control + payload
}

// Verify I-frame encodeFrameToBuffer includes length byte
TEST(LAPDmFrameTest, IFrame_EncodeToBuffer) {
    uint8_t payload[] = {0x01, 0x02};
    auto frame = makeIFrame(SAPI::SAPI0, false, 1, 2, true, true, std::span(payload));
    uint8_t buf[16] = {};
    size_t written = encodeFrameToBuffer(frame, buf, sizeof(buf));
    EXPECT_EQ(written, 5u); // address + control + length + 2 payload
    EXPECT_EQ(buf[2], 0x82u); // M=1, len=2 -> 10000010 = 0x82
}

// Verify LAPDmLengthField encode/decode round-trip
TEST(LAPDmFrameTest, LengthField_EncodeDecode) {
    auto len = LAPDmLengthField(true, 3); // M=1, length=3
    uint8_t encoded = len.encode();
    EXPECT_EQ(encoded, 0x83u); // 10000011
    auto decoded = LAPDmLengthField::decode(encoded);
    EXPECT_TRUE(decoded.m);
    EXPECT_EQ(decoded.length, 3u);

    auto len2 = LAPDmLengthField(false, 63); // M=0, length=63
    uint8_t encoded2 = len2.encode();
    EXPECT_EQ(encoded2, 0x3Fu); // 00111111
    auto decoded2 = LAPDmLengthField::decode(encoded2);
    EXPECT_FALSE(decoded2.m);
    EXPECT_EQ(decoded2.length, 63u);
}

// Verify I-control field encode/decode round-trip
TEST(LAPDmFrameTest, IControlField_EncodeDecode) {
    auto ctrl = LAPDmIControlField(7, 0, true); // NR=7, NS=0, PF=1
    uint8_t encoded = ctrl.encode();
    EXPECT_EQ(encoded, 0xF0u); // 1111 0000: NR=7, PF=1, NS=0, Fixed=0
    auto decoded = LAPDmIControlField::decode(encoded);
    EXPECT_EQ(decoded.nr, 7u);
    EXPECT_EQ(decoded.ns, 0u);
    EXPECT_TRUE(decoded.pf);
}

// Verify S-control field encode/decode round-trip
TEST(LAPDmFrameTest, SControlField_EncodeDecode) {
    auto ctrl = LAPDmSControlField(3, true, LAPDmSFrameType::RR);
    uint8_t encoded = ctrl.encode();
    EXPECT_EQ(encoded, 0x71u); // 011 1 00 1
    auto decoded = LAPDmSControlField::decode(encoded);
    EXPECT_EQ(decoded.nr, 3u);
    EXPECT_TRUE(decoded.pf);
    EXPECT_EQ(decoded.type, LAPDmSFrameType::RR);

    auto ctrl2 = LAPDmSControlField(5, false, LAPDmSFrameType::REJ);
    uint8_t encoded2 = ctrl2.encode();
    EXPECT_EQ(encoded2, 0xADu); // 101 0 10 1
    auto decoded2 = LAPDmSControlField::decode(encoded2);
    EXPECT_EQ(decoded2.nr, 5u);
    EXPECT_FALSE(decoded2.pf);
    EXPECT_EQ(decoded2.type, LAPDmSFrameType::REJ);
}

// Verify accessor methods on decoded frame
TEST(LAPDmFrameTest, AccessorMethods) {
    uint8_t payload[] = {0x60, 0x0D};
    auto frame = makeUIFrame(SAPI::SAPI3, true, std::span(payload));
    auto encoded = encodeFrame(frame);
    auto result = LAPDmFrame::decode(std::span(encoded));
    ASSERT_TRUE(result);
    const auto& f = *result;
    EXPECT_EQ(f.sapi(), SAPI::SAPI3);
    EXPECT_TRUE(f.isCommand());
    EXPECT_TRUE(f.hasInfo());
    EXPECT_EQ(f.infoSize(), 2u);
}

// Verify constexpr encode/decode work at compile time
TEST(LAPDmFrameTest, Constexpr_EncodeDecode) {
    // Verify that field encode/decode are constexpr and work at compile time.
    constexpr auto addr = LAPDmAddressField{SAPI::SAPI0, false, true};
    constexpr uint8_t encoded = addr.encode();
    static_assert(encoded == 0x01, "Address encoding must be constexpr");

    constexpr auto decoded = LAPDmAddressField::decode(0x39);
    static_assert(decoded.sapi == SAPI::SAPI3, "Address decoding must be constexpr");
    static_assert(decoded.command == true, "C/R bit must be extracted at compile time");
}

// Verify UI frame with empty payload encodes correctly
TEST(LAPDmFrameTest, UI_Frame_EmptyPayload) {
    auto frame = makeUIFrame(SAPI::SAPI0, false, std::span<const uint8_t>{});
    auto encoded = encodeFrame(frame);
    EXPECT_EQ(encoded.size(), 2u); // address + control only
    EXPECT_EQ(encoded[0], 0x01u);
    EXPECT_EQ(encoded[1], 0x03u);

    auto result = LAPDmFrame::decode(std::span(encoded));
    ASSERT_TRUE(result);
    EXPECT_FALSE((*result).hasInfo());
}

// Verify I-frame with empty payload (valid per GSM 04.06)
TEST(LAPDmFrameTest, I_Frame_EmptyPayload) {
    auto frame = makeIFrame(SAPI::SAPI0, true, 0, 1, false, true, std::span<const uint8_t>{});
    auto encoded = encodeFrame(frame);
    EXPECT_EQ(encoded.size(), 3u); // address + control + length
    EXPECT_EQ(encoded[2], 0x80u); // M=1, len=0 -> 10000000

    auto result = LAPDmFrame::decode(std::span(encoded));
    ASSERT_TRUE(result);
    const auto& f = *result;
    EXPECT_EQ(f.format, LAPDmControlFormat::I_Format);
    EXPECT_FALSE(f.hasInfo());
    EXPECT_TRUE(f.m);
}

// Verify DM frame decode with PF=0
TEST(LAPDmFrameTest, DM_Frame_PF0) {
    auto frame = makeDMFrame(SAPI::SAPI0, false);
    auto encoded = encodeFrame(frame);
    EXPECT_EQ(encoded[1], 0x0Bu); // DM with F=0: 0000 1011 (F bit cleared from 0x0F)

    auto result = LAPDmFrame::decode(std::span(encoded));
    ASSERT_TRUE(result);
    const auto& f = *result;
    EXPECT_EQ(f.uType, LAPDmUFrameType::DM);
    EXPECT_FALSE(f.pf);
}

// Verify SABME without payload decodes correctly (no length byte)
TEST(LAPDmFrameTest, SABME_NoPayload_Decode) {
    uint8_t raw[] = {0x09, 0x2F}; // SAPI0 command, SABME, no payload
    auto result = LAPDmFrame::decode(std::span(raw));
    ASSERT_TRUE(result);
    const auto& f = *result;
    EXPECT_EQ(f.uType, LAPDmUFrameType::SABME);
    EXPECT_FALSE(f.hasInfo());
}

// Verify UA without payload decodes correctly (no length byte)
TEST(LAPDmFrameTest, UA_NoPayload_Decode) {
    uint8_t raw[] = {0x01, 0x63}; // SAPI0 response, UA, PF=1
    auto result = LAPDmFrame::decode(std::span(raw));
    ASSERT_TRUE(result);
    const auto& f = *result;
    EXPECT_EQ(f.uType, LAPDmUFrameType::UA);
    EXPECT_FALSE(f.hasInfo());
    EXPECT_TRUE(f.pf);
}
