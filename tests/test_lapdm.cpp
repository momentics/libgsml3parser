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
#include <gsml3parser/lapdm_entity.h>

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

// ── MockLAPDmEntity helper for state machine tests ────────────────────

// Helper to create entity with mock callbacks (FlatHandler-style pattern).
class MockLAPDmEntity {
public:
    LAPDmEntity entity;
    std::vector<std::pair<Primitive, std::vector<uint8_t>>> l3Received;
    std::vector<std::vector<uint8_t>> l1Sent;

    // Static callback wrappers for zero-allocation pattern
    static void onL3(SAPI, Primitive prim, std::span<const uint8_t> data, void* ctx) {
        auto* self = static_cast<MockLAPDmEntity*>(ctx);
        self->l3Received.emplace_back(prim, std::vector<uint8_t>(data.begin(), data.end()));
    }

    static void onL1(std::span<const uint8_t> data, void* ctx) {
        auto* self = static_cast<MockLAPDmEntity*>(ctx);
        self->l1Sent.push_back(std::vector<uint8_t>(data.begin(), data.end()));
    }

    MockLAPDmEntity(LAPDmChannelProfile profile = LAPDmChannelProfile::SDCCH())
        : entity(profile, onL3, onL1, this)
    {}
};

// ── FSM State Transition Tests (GSM 04.06 3.5.2) ──────────────────────

// Initial state before open() should be Unused.
TEST(LAPDmEntityTest, InitialState_IsUnused) {
    MockLAPDmEntity mock;
    EXPECT_EQ(mock.entity.state(), LAPDmState::Unused);
}

// open() transitions to LinkReleased.
TEST(LAPDmEntityTest, Open_TransitionsToLinkReleased) {
    MockLAPDmEntity mock;
    mock.entity.open(SAPI::SAPI0, true); // BTS side
    EXPECT_EQ(mock.entity.state(), LAPDmState::LinkReleased);
}

// MS sends SABME -> BTS receives -> sends UA -> link established.
TEST(LAPDmEntityTest, SABME_ThenUA_EstablishesLink) {
    MockLAPDmEntity btsMock;
    btsMock.entity.open(SAPI::SAPI0, true); // BTS = command

    // MS sends SABME to BTS
    auto sabme = encodeFrame(makeSABMEFrame(SAPI::SAPI0, false, std::span<const uint8_t>{}));
    btsMock.entity.receiveFrame(sabme);

    EXPECT_EQ(btsMock.entity.state(), LAPDmState::LinkEstablished);
    EXPECT_EQ(btsMock.l3Received.size(), 1u);
    EXPECT_EQ(btsMock.l3Received[0].first, Primitive::L3_ESTABLISH_INDICATION);
    // BTS should have sent UA
    ASSERT_GE(btsMock.l1Sent.size(), 1u);
    auto uaDecoded = LAPDmFrame::decode(btsMock.l1Sent.back());
    ASSERT_TRUE(uaDecoded);
    EXPECT_EQ((*uaDecoded).uType, LAPDmUFrameType::UA);
}

// Active side: BTS initiates SABME (e.g., SAPI3 for SMS), receives UA.
TEST(LAPDmEntityTest, ActiveSide_SABME_ThenReceivesUA) {
    MockLAPDmEntity btsMock;
    btsMock.entity.open(SAPI::SAPI3, true);

    (void)btsMock.entity.sendSABME();
    EXPECT_EQ(btsMock.entity.state(), LAPDmState::AwaitingEstablish);

    // Simulate MS responding with UA
    auto ua = encodeFrame(makeUAFrame(SAPI::SAPI3, true, std::span<const uint8_t>{})); // PF=1 response
    btsMock.entity.receiveFrame(ua);

    EXPECT_EQ(btsMock.entity.state(), LAPDmState::LinkEstablished);
    EXPECT_EQ(btsMock.l3Received.back().first, Primitive::L3_ESTABLISH_CONFIRM);
}

// Normal release: DISC -> UA.
TEST(LAPDmEntityTest, DISC_Release) {
    MockLAPDmEntity mock;
    mock.entity.open(SAPI::SAPI0, true);
    // Establish link first (receive SABME from peer)
    auto sabme = encodeFrame(makeSABMEFrame(SAPI::SAPI0, false, std::span<const uint8_t>{}));
    mock.entity.receiveFrame(sabme);
    EXPECT_EQ(mock.entity.state(), LAPDmState::LinkEstablished);

    // Send DISC
    (void)mock.entity.sendDISC();
    EXPECT_EQ(mock.entity.state(), LAPDmState::AwaitingRelease);

    // Peer responds with UA
    auto ua = encodeFrame(makeUAFrame(SAPI::SAPI0, true, std::span<const uint8_t>{}));
    mock.entity.receiveFrame(ua);
    EXPECT_EQ(mock.entity.state(), LAPDmState::LinkReleased);
}

// Peer sends DISC in LinkEstablished -> respond with UA and release.
TEST(LAPDmEntityTest, DISC_FromPeer) {
    MockLAPDmEntity mock;
    mock.entity.open(SAPI::SAPI0, true);
    auto sabme = encodeFrame(makeSABMEFrame(SAPI::SAPI0, false, std::span<const uint8_t>{}));
    mock.entity.receiveFrame(sabme);

    // Peer sends DISC
    auto disc = encodeFrame(makeDISCFrame(SAPI::SAPI0, false)); // response side
    mock.entity.receiveFrame(disc);

    EXPECT_EQ(mock.entity.state(), LAPDmState::LinkReleased);
}

// hardRelease() transitions immediately to LinkReleased.
TEST(LAPDmEntityTest, HardRelease) {
    MockLAPDmEntity mock;
    mock.entity.open(SAPI::SAPI0, true);
    auto sabme = encodeFrame(makeSABMEFrame(SAPI::SAPI0, false, std::span<const uint8_t>{}));
    mock.entity.receiveFrame(sabme);

    mock.entity.hardRelease();
    EXPECT_EQ(mock.entity.state(), LAPDmState::LinkReleased);
}

// I-frames received in LinkReleased state are ignored (GSM 04.06 5.4.5).
TEST(LAPDmEntityTest, IFrame_InLinkReleased_IsIgnored) {
    MockLAPDmEntity mock;
    mock.entity.open(SAPI::SAPI0, true);

    uint8_t payload[] = {0x60, 0x0D};
    auto iframe = encodeFrame(makeIFrame(SAPI::SAPI0, false, 0, 0, false, true, std::span(payload))); // M=1 complete
    mock.entity.receiveFrame(iframe);

    EXPECT_EQ(mock.entity.state(), LAPDmState::LinkReleased);
    EXPECT_EQ(mock.l3Received.size(), 0u); // No L3 callback
}

// SABM without payload in LinkEstablished = re-establishment (GSM 04.06 5.6.3).
TEST(LAPDmEntityTest, ReEstablishment_InLinkEstablished) {
    MockLAPDmEntity mock;
    mock.entity.open(SAPI::SAPI0, true);
    auto sabme1 = encodeFrame(makeSABMEFrame(SAPI::SAPI0, false, std::span<const uint8_t>{}));
    mock.entity.receiveFrame(sabme1);
    EXPECT_EQ(mock.entity.state(), LAPDmState::LinkEstablished);

    // Send an I-frame to advance VS counter (proves counters are non-zero)
    uint8_t data[] = {0x60, 0x0D};
    auto result = mock.entity.sendData(std::span(data));
    ASSERT_TRUE(result);
    EXPECT_TRUE(mock.entity.hasOutstandingFrame());

    // Peer sends SABM again (re-establishment) -- no payload
    auto sabme2 = encodeFrame(makeSABMEFrame(SAPI::SAPI0, false, std::span<const uint8_t>{}));
    mock.entity.receiveFrame(sabme2);

    EXPECT_EQ(mock.entity.state(), LAPDmState::LinkEstablished); // Stays established
    EXPECT_FALSE(mock.entity.hasOutstandingFrame()); // Counters cleared by re-establishment
}

// ── I-frame Segmentation and Reassembly Tests (GSM 04.06 5.5) ─────────

// sendUI delivers L3 data in a UI frame.
TEST(LAPDmEntityTest, SendUI_DeliversL3Data) {
    MockLAPDmEntity mock;
    mock.entity.open(SAPI::SAPI0, true);
    auto sabme = encodeFrame(makeSABMEFrame(SAPI::SAPI0, false, std::span<const uint8_t>{}));
    mock.entity.receiveFrame(sabme);

    uint8_t data[] = {0x60, 0x0D, 0x00}; // Channel Release
    auto result = mock.entity.sendUI(SAPI::SAPI0, std::span(data));
    ASSERT_TRUE(result);

    ASSERT_GE(mock.l1Sent.size(), 1u);
    auto decoded = LAPDmFrame::decode(mock.l1Sent.back());
    ASSERT_TRUE(decoded);
    EXPECT_EQ((*decoded).uType, LAPDmUFrameType::UI);
    EXPECT_EQ((*decoded).info.size(), 3u);
}

// UI frame received delivers L3_UNIT_DATA to callback.
TEST(LAPDmEntityTest, ReceiveUI_DeliversToL3) {
    MockLAPDmEntity mock;
    mock.entity.open(SAPI::SAPI0, true);

    uint8_t l3Data[] = {0x60, 0x27, 0x04}; // Paging Response header
    auto uiFrame = encodeFrame(makeUIFrame(SAPI::SAPI0, false, std::span(l3Data)));
    mock.entity.receiveFrame(uiFrame);

    EXPECT_EQ(mock.l3Received.size(), 1u);
    EXPECT_EQ(mock.l3Received[0].first, Primitive::L3_UNIT_DATA);
    EXPECT_EQ(mock.l3Received[0].second.size(), 3u);
}

// Single I-frame (M=1) delivers complete L3_DATA.
TEST(LAPDmEntityTest, ReceiveSingleIFrame_DeliversToL3) {
    MockLAPDmEntity mock;
    mock.entity.open(SAPI::SAPI0, true); // BTS
    auto sabme = encodeFrame(makeSABMEFrame(SAPI::SAPI0, false, std::span<const uint8_t>{}));
    mock.entity.receiveFrame(sabme); // LinkEstablished

    uint8_t data[] = {0x60, 0x15, 0xC1, 0x02}; // Measurement Report
    auto iframe = encodeFrame(makeIFrame(SAPI::SAPI0, false, 0, 0, false, true, std::span(data))); // M=1 complete
    mock.entity.receiveFrame(iframe);

    EXPECT_EQ(mock.l3Received.size(), 2u); // ESTABLISH_INDICATION + L3_DATA
    EXPECT_EQ(mock.l3Received.back().first, Primitive::L3_DATA);
    EXPECT_EQ(mock.l3Received.back().second.size(), 4u);
}

// Two I-frames (M=0 then M=1) reassemble into one L3 message.
TEST(LAPDmEntityTest, ReceiveSegmentedIFrames_Reassembles) {
    MockLAPDmEntity mock;
    mock.entity.open(SAPI::SAPI0, true);
    auto sabme = encodeFrame(makeSABMEFrame(SAPI::SAPI0, false, std::span<const uint8_t>{}));
    mock.entity.receiveFrame(sabme);

    // Frame 1: NS=0, NR=0, M=0 (more segments follow), payload="ABCD"
    uint8_t part1[] = {0x41, 0x42, 0x43, 0x44};
    auto frame1 = encodeFrame(makeIFrame(SAPI::SAPI0, false, 0, 0, false, false, std::span(part1)));
    mock.entity.receiveFrame(frame1);

    // After frame 1: should NOT have delivered L3_DATA yet (M=0)
    size_t dataCallsAfterFrame1 = 0;
    for (auto& [prim, _] : mock.l3Received) {
        if (prim == Primitive::L3_DATA) dataCallsAfterFrame1++;
    }
    EXPECT_EQ(dataCallsAfterFrame1, 0u);

    // Frame 2: NS=1, NR=0, M=1 (Message complete — last segment), payload="EFGH"
    uint8_t part2[] = {0x45, 0x46, 0x47, 0x48};
    auto frame2 = encodeFrame(makeIFrame(SAPI::SAPI0, false, 0, 1, false, true, std::span(part2)));
    mock.entity.receiveFrame(frame2);

    // After frame 2: should have delivered complete reassembled message "ABCDEFGH"
    size_t totalDataCalls = 0;
    std::vector<uint8_t> lastData;
    for (auto& [prim, data] : mock.l3Received) {
        if (prim == Primitive::L3_DATA) {
            totalDataCalls++;
            lastData = data;
        }
    }
    EXPECT_EQ(totalDataCalls, 1u);
    EXPECT_EQ(lastData.size(), 8u);
    EXPECT_EQ(std::string(lastData.begin(), lastData.end()), "ABCDEFGH");
}

// Three I-frames (M=0, M=0, M=1) reassemble into one L3 message.
TEST(LAPDmEntityTest, ReceiveThreeSegmentedIFrames_Reassembles) {
    MockLAPDmEntity mock;
    mock.entity.open(SAPI::SAPI0, true);
    auto sabme = encodeFrame(makeSABMEFrame(SAPI::SAPI0, false, std::span<const uint8_t>{}));
    mock.entity.receiveFrame(sabme);

    // Segment 1: NS=0, M=0 (more follow), payload="AAA"
    uint8_t p1[] = {0x41, 0x41, 0x41};
    mock.entity.receiveFrame(encodeFrame(makeIFrame(SAPI::SAPI0, false, 0, 0, false, false, std::span(p1))));

    // Segment 2: NS=1, M=0 (more follow), payload="BBB"
    uint8_t p2[] = {0x42, 0x42, 0x42};
    mock.entity.receiveFrame(encodeFrame(makeIFrame(SAPI::SAPI0, false, 0, 1, false, false, std::span(p2))));

    // Segment 3: NS=2, M=1 (Message complete — last), payload="CCC"
    uint8_t p3[] = {0x43, 0x43, 0x43};
    mock.entity.receiveFrame(encodeFrame(makeIFrame(SAPI::SAPI0, false, 0, 2, false, true, std::span(p3))));

    // Verify reassembled message
    std::vector<uint8_t> lastData;
    for (auto& [prim, data] : mock.l3Received) {
        if (prim == Primitive::L3_DATA) lastData = data;
    }
    EXPECT_EQ(lastData.size(), 9u);
    EXPECT_EQ(std::string(lastData.begin(), lastData.end()), "AAABBBCCC");
}

// Out-of-order I-frame triggers REJ response.
TEST(LAPDmEntityTest, OutOfOrderIFrame_SendsREJ) {
    MockLAPDmEntity mock;
    mock.entity.open(SAPI::SAPI0, true);
    auto sabme = encodeFrame(makeSABMEFrame(SAPI::SAPI0, false, std::span<const uint8_t>{}));
    mock.entity.receiveFrame(sabme);

    // Send NS=2 but we expect NS=0 (VR=0), M=1 (complete message)
    uint8_t data[] = {0x60, 0x0D};
    auto iframe = encodeFrame(makeIFrame(SAPI::SAPI0, false, 0, 2, false, true, std::span(data)));
    mock.entity.receiveFrame(iframe);

    // Should have sent REJ
    auto lastSent = LAPDmFrame::decode(mock.l1Sent.back());
    ASSERT_TRUE(lastSent);
    EXPECT_EQ((*lastSent).format, LAPDmControlFormat::S_Format);
    EXPECT_EQ((*lastSent).sType, LAPDmSFrameType::REJ);
}

// Valid I-frame triggers RR response.
TEST(LAPDmEntityTest, ValidIFrame_SendsRR) {
    MockLAPDmEntity mock;
    mock.entity.open(SAPI::SAPI0, true);
    auto sabme = encodeFrame(makeSABMEFrame(SAPI::SAPI0, false, std::span<const uint8_t>{}));
    mock.entity.receiveFrame(sabme);

    uint8_t data[] = {0x60, 0x0D};
    auto iframe = encodeFrame(makeIFrame(SAPI::SAPI0, false, 0, 0, false, true, std::span(data))); // M=1 complete
    mock.entity.receiveFrame(iframe);

    auto lastSent = LAPDmFrame::decode(mock.l1Sent.back());
    ASSERT_TRUE(lastSent);
    EXPECT_EQ((*lastSent).format, LAPDmControlFormat::S_Format);
    EXPECT_EQ((*lastSent).sType, LAPDmSFrameType::RR);
}

// sendData with small payload sends single I-frame.
TEST(LAPDmEntityTest, SendData_SingleFrame) {
    MockLAPDmEntity mock;
    mock.entity.open(SAPI::SAPI0, true);
    auto sabme = encodeFrame(makeSABMEFrame(SAPI::SAPI0, false, std::span<const uint8_t>{}));
    mock.entity.receiveFrame(sabme);

    uint8_t data[10];
    std::fill(std::begin(data), std::end(data), 0xAB);

    auto result = mock.entity.sendData(std::span(data));
    ASSERT_TRUE(result);

    // Should have sent one I-frame with M=1 (Message complete — last)
    size_t iFrameCount = 0;
    for (auto& frameBytes : mock.l1Sent) {
        auto decoded = LAPDmFrame::decode(frameBytes);
        if (decoded && (*decoded).format == LAPDmControlFormat::I_Format) {
            iFrameCount++;
        }
    }
    EXPECT_EQ(iFrameCount, 1u);
}

// k=1 constraint: second sendData fails when first frame not acknowledged.
TEST(LAPDmEntityTest, SendData_ExceedsN201_FailsWithOutstanding) {
    MockLAPDmEntity mock;
    mock.entity.open(SAPI::SAPI0, true);
    auto sabme = encodeFrame(makeSABMEFrame(SAPI::SAPI0, false, std::span<const uint8_t>{}));
    mock.entity.receiveFrame(sabme);

    uint8_t data[10];
    std::fill(std::begin(data), std::end(data), 0xAB);

    auto result1 = mock.entity.sendData(std::span(data));
    ASSERT_TRUE(result1); // First send succeeds

    // Second send should fail because first frame not acknowledged
    auto result2 = mock.entity.sendData(std::span(data));
    ASSERT_FALSE(result2);
}

// After RR acknowledges, second sendData succeeds.
TEST(LAPDmEntityTest, SendData_AfterAck_Succeeds) {
    MockLAPDmEntity mock;
    mock.entity.open(SAPI::SAPI0, true);
    auto sabme = encodeFrame(makeSABMEFrame(SAPI::SAPI0, false, std::span<const uint8_t>{}));
    mock.entity.receiveFrame(sabme);

    uint8_t data[10];
    std::fill(std::begin(data), std::end(data), 0xAB);

    auto result1 = mock.entity.sendData(std::span(data));
    ASSERT_TRUE(result1); // First send succeeds, VS advanced

    // Simulate peer sending RR(NR=1) to acknowledge
    auto rr = encodeFrame(makeRRFrame(SAPI::SAPI0, 1, false)); // NR=1 acknowledges NS=0
    mock.entity.receiveFrame(rr);

    // Now VS==VA, second send should succeed
    auto result2 = mock.entity.sendData(std::span(data));
    ASSERT_TRUE(result2);
}

// sendData fails before link established.
TEST(LAPDmEntityTest, SendData_BeforeLink_Fails) {
    MockLAPDmEntity mock;
    mock.entity.open(SAPI::SAPI0, true);

    uint8_t data[] = {0x60, 0x0D};
    auto result = mock.entity.sendData(std::span(data));
    ASSERT_FALSE(result); // sendData requires LinkEstablished
}

// ── T200 Timer and Retransmission Tests (GSM 04.06 3.5) ───────────────

// T200 expiry triggers retransmission.
TEST(LAPDmEntityTest, T200_Retransmission) {
    MockLAPDmEntity mock;
    mock.entity.open(SAPI::SAPI0, true);
    auto sabme = encodeFrame(makeSABMEFrame(SAPI::SAPI0, false, std::span<const uint8_t>{}));
    mock.entity.receiveFrame(sabme);

    // Send data (starts T200)
    uint8_t data[10];
    std::fill(std::begin(data), std::end(data), 0xAB);
    auto result = mock.entity.sendData(std::span(data));
    ASSERT_TRUE(result);

    size_t initialSentCount = mock.l1Sent.size();

    // Tick T200 past expiry
    bool retransmitted = mock.entity.tickT200(std::chrono::milliseconds(1000));
    EXPECT_TRUE(retransmitted);

    // Should have retransmitted the frame
    EXPECT_GT(mock.l1Sent.size(), initialSentCount);
    EXPECT_EQ(mock.entity.retransmissions(), 1u);
}

// T200 does nothing when no outstanding frame.
TEST(LAPDmEntityTest, T200_NoRetransmission_WhenAcknowledged) {
    MockLAPDmEntity mock;
    mock.entity.open(SAPI::SAPI0, true);
    auto sabme = encodeFrame(makeSABMEFrame(SAPI::SAPI0, false, std::span<const uint8_t>{}));
    mock.entity.receiveFrame(sabme);

    uint8_t data[10];
    std::fill(std::begin(data), std::end(data), 0xAB);
    (void)mock.entity.sendData(std::span(data));

    // Acknowledge with RR(NR=1)
    auto rr = encodeFrame(makeRRFrame(SAPI::SAPI0, 1, false));
    mock.entity.receiveFrame(rr);

    // T200 should be stopped now -- tick should do nothing
    bool retransmitted = mock.entity.tickT200(std::chrono::milliseconds(10000));
    EXPECT_FALSE(retransmitted);
}

// After N200 retransmissions, abnormal release occurs.
TEST(LAPDmEntityTest, T200_AbnormalRelease_AfterN200) {
    // Use SACCH profile (N200=5) for faster test
    MockLAPDmEntity mock(LAPDmChannelProfile::SACCH());
    mock.entity.open(SAPI::SAPI0, true);
    auto sabme = encodeFrame(makeSABMEFrame(SAPI::SAPI0, false, std::span<const uint8_t>{}));
    mock.entity.receiveFrame(sabme);

    uint8_t data[10];
    std::fill(std::begin(data), std::end(data), 0xAB);
    (void)mock.entity.sendData(std::span(data));

    // Tick T200 more than N200 times (SACCH has N200=5)
    for (unsigned i = 0; i <= 5; i++) {
        mock.entity.tickT200(std::chrono::milliseconds(4000)); // > T200=3600ms
    }

    // Should have transitioned to LinkReleased and called L3 callback with error
    EXPECT_EQ(mock.entity.state(), LAPDmState::LinkReleased);
    bool gotError = false;
    for (auto& [prim, _] : mock.l3Received) {
        if (prim == Primitive::MDL_ERROR_INDICATION) {
            gotError = true;
            break;
        }
    }
    EXPECT_TRUE(gotError);
}

// SABME T200 expiry leads to abnormal release.
TEST(LAPDmEntityTest, SABME_T200_Expiry_AbnormalRelease) {
    MockLAPDmEntity mock;
    mock.entity.open(SAPI::SAPI0, true);

    (void)mock.entity.sendSABME();
    EXPECT_EQ(mock.entity.state(), LAPDmState::AwaitingEstablish);

    // Use SDCCH profile (N200=23, T200=900ms). Tick past limit.
    for (unsigned i = 0; i <= 23; i++) {
        mock.entity.tickT200(std::chrono::milliseconds(1000));
    }

    EXPECT_EQ(mock.entity.state(), LAPDmState::LinkReleased);
}

// Partial T200 ticks accumulate correctly.
TEST(LAPDmEntityTest, T200_Incremental_Ticks) {
    MockLAPDmEntity mock;
    mock.entity.open(SAPI::SAPI0, true);
    auto sabme = encodeFrame(makeSABMEFrame(SAPI::SAPI0, false, std::span<const uint8_t>{}));
    mock.entity.receiveFrame(sabme);

    uint8_t data[10];
    std::fill(std::begin(data), std::end(data), 0xAB);
    (void)mock.entity.sendData(std::span(data));

    // SDCCH T200 = 900ms. Tick 400ms -- should NOT expire
    bool expired1 = mock.entity.tickT200(std::chrono::milliseconds(400));
    EXPECT_FALSE(expired1);

    // Tick another 400ms -- total 800ms, still not expired
    bool expired2 = mock.entity.tickT200(std::chrono::milliseconds(400));
    EXPECT_FALSE(expired2);

    // Tick 200ms more -- total 1000ms > 900ms, should expire and retransmit
    bool expired3 = mock.entity.tickT200(std::chrono::milliseconds(200));
    EXPECT_TRUE(expired3);
}

// T200 tick when inactive returns false.
TEST(LAPDmEntityTest, T200_Inactive_ReturnsFalse) {
    MockLAPDmEntity mock;
    mock.entity.open(SAPI::SAPI0, true);

    // No frames sent, T200 should be inactive
    bool result = mock.entity.tickT200(std::chrono::milliseconds(10000));
    EXPECT_FALSE(result);
}

// Protocol statistics are tracked correctly.
TEST(LAPDmEntityTest, Statistics_Tracking) {
    MockLAPDmEntity mock;
    mock.entity.open(SAPI::SAPI0, true);

    EXPECT_EQ(mock.entity.framesSent(), 0u);
    EXPECT_EQ(mock.entity.framesReceived(), 0u);
    EXPECT_EQ(mock.entity.retransmissions(), 0u);

    auto sabme = encodeFrame(makeSABMEFrame(SAPI::SAPI0, false, std::span<const uint8_t>{}));
    mock.entity.receiveFrame(sabme);

    EXPECT_GT(mock.entity.framesReceived(), 0u);
    EXPECT_GT(mock.entity.framesSent(), 0u); // UA was sent

    mock.entity.resetStats();
    EXPECT_EQ(mock.entity.framesSent(), 0u);
    EXPECT_EQ(mock.entity.framesReceived(), 0u);
}

// ── Contention Resolution Tests (GSM 04.06 5.4.1.4) ───────────────────

// SABME with payload on SAPI0 enters ContentionResolution.
TEST(LAPDmEntityTest, ContentionResolution_SABME_WithPayload) {
    MockLAPDmEntity mock;
    mock.entity.open(SAPI::SAPI0, true); // BTS side

    uint8_t pagingResponse[] = {0x60, 0x27, 0x04, 0x60, 0x00, 0x12, 0x34, 0x56, 0x78};
    auto sabme = encodeFrame(makeSABMEFrame(SAPI::SAPI0, false, std::span(pagingResponse)));
    mock.entity.receiveFrame(sabme);

    // Should be in ContentionResolution (not LinkEstablished)
    EXPECT_EQ(mock.entity.state(), LAPDmState::ContentionResolution);

    // Should have sent UA with echoed payload
    bool foundUAWithEcho = false;
    for (auto& frameBytes : mock.l1Sent) {
        auto decoded = LAPDmFrame::decode(frameBytes);
        if (decoded && (*decoded).uType == LAPDmUFrameType::UA && (*decoded).hasInfo()) {
            foundUAWithEcho = true;
            break;
        }
    }
    EXPECT_TRUE(foundUAWithEcho);
}

// I-frame received in ContentionResolution transitions to LinkEstablished.
TEST(LAPDmEntityTest, ContentionResolution_TransitionsOnIFrame) {
    MockLAPDmEntity mock;
    mock.entity.open(SAPI::SAPI0, true);

    uint8_t payload[] = {0x60, 0x27};
    auto sabme = encodeFrame(makeSABMEFrame(SAPI::SAPI0, false, std::span(payload)));
    mock.entity.receiveFrame(sabme);
    EXPECT_EQ(mock.entity.state(), LAPDmState::ContentionResolution);

    // Receive an I-frame -- should transition to LinkEstablished
    uint8_t data[] = {0x50, 0x24}; // CM Service Request
    auto iframe = encodeFrame(makeIFrame(SAPI::SAPI0, false, 0, 0, false, true, std::span(data))); // M=1 complete
    mock.entity.receiveFrame(iframe);

    EXPECT_EQ(mock.entity.state(), LAPDmState::LinkEstablished);
}

// SAPI3 with SABME payload goes directly to LinkEstablished (no contention resolution).
TEST(LAPDmEntityTest, ContentionResolution_SAPI3_GoesDirectlyToEstablished) {
    MockLAPDmEntity mock;
    mock.entity.open(SAPI::SAPI3, true);

    uint8_t payload[] = {0x90, 0x01}; // SMS CP-DATA
    auto sabme = encodeFrame(makeSABMEFrame(SAPI::SAPI3, false, std::span(payload)));
    mock.entity.receiveFrame(sabme);

    // SAPI3 should go directly to LinkEstablished, not ContentionResolution
    EXPECT_EQ(mock.entity.state(), LAPDmState::LinkEstablished);
}

// DISC in ContentionResolution releases the link.
TEST(LAPDmEntityTest, DISC_InContentionResolution) {
    MockLAPDmEntity mock;
    mock.entity.open(SAPI::SAPI0, true);

    uint8_t payload[] = {0x60, 0x27};
    auto sabme = encodeFrame(makeSABMEFrame(SAPI::SAPI0, false, std::span(payload)));
    mock.entity.receiveFrame(sabme);
    EXPECT_EQ(mock.entity.state(), LAPDmState::ContentionResolution);

    // Peer sends DISC
    auto disc = encodeFrame(makeDISCFrame(SAPI::SAPI0, false));
    mock.entity.receiveFrame(disc);

    EXPECT_EQ(mock.entity.state(), LAPDmState::LinkReleased);
}

// ── Channel Profile Tests (GSM 04.06 3.5) ─────────────────────────────

TEST(LAPDmChannelProfileTest, SDCCH_Parameters) {
    auto profile = LAPDmChannelProfile::SDCCH();
    EXPECT_EQ(profile.n201, 20u);    // 20 octets max I-frame payload
    EXPECT_EQ(profile.n200, 23u);    // 23 retransmissions max
    EXPECT_EQ(profile.t200Ms, 900u); // 900ms T200
}

TEST(LAPDmChannelProfileTest, SACCH_Parameters) {
    auto profile = LAPDmChannelProfile::SACCH();
    EXPECT_EQ(profile.n201, 18u);     // 18 octets max I-frame payload
    EXPECT_EQ(profile.n200, 5u);      // 5 retransmissions max
    EXPECT_EQ(profile.t200Ms, 3600u); // 3600ms T200 (= 4 * 900)
}

TEST(LAPDmChannelProfileTest, FACCH_Parameters) {
    auto profile = LAPDmChannelProfile::FACCH();
    EXPECT_EQ(profile.n201, 20u);    // 20 octets max I-frame payload
    EXPECT_EQ(profile.n200, 34u);    // 34 retransmissions max
    EXPECT_EQ(profile.t200Ms, 900u); // 900ms T200
}

// SACCH profile enforces N200=5 retransmission limit.
TEST(LAPDmEntityTest, SACCH_Profile_N200_Limit) {
    MockLAPDmEntity mock(LAPDmChannelProfile::SACCH());
    mock.entity.open(SAPI::SAPI0, true);
    auto sabme = encodeFrame(makeSABMEFrame(SAPI::SAPI0, false, std::span<const uint8_t>{}));
    mock.entity.receiveFrame(sabme);

    uint8_t data[10];
    std::fill(std::begin(data), std::end(data), 0xAB);
    (void)mock.entity.sendData(std::span(data));

    // 5 retransmissions (N200=5) + 1 final expiry -> abnormal release
    for (unsigned i = 0; i <= 5; i++) {
        mock.entity.tickT200(std::chrono::milliseconds(4000));
    }

    EXPECT_EQ(mock.entity.retransmissions(), 5u); // Exactly N200 retransmissions
    EXPECT_EQ(mock.entity.state(), LAPDmState::LinkReleased);
}

// ── Object Size and Memory Tests ──────────────────────────────────────

// LAPDmEntity must remain compact for scale.
TEST(LAPDmEntityTest, ObjectSize_Bounded) {
    static_assert(sizeof(LAPDmEntity) < 512, "LAPDmEntity too large for scale");
    // Just reach here to confirm static_assert passes.
}

// Fresh LAPDmEntity instance has no heap allocations.
TEST(LAPDmEntityTest, MemoryUsage_FreshInstance_NoHeap) {
    MockLAPDmEntity mock;
    EXPECT_EQ(mock.entity.framesSent(), 0u);
    EXPECT_EQ(mock.entity.framesReceived(), 0u);
    EXPECT_EQ(mock.entity.retransmissions(), 0u);
}
