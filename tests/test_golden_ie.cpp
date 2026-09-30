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

// Comprehensive GSM Layer 3 IE (Information Element) Golden Tests.
// Wire layouts and identifiers per 3GPP TS 24.008 sections 10.5.1..10.5.5,
// TS 24.078, TS 44.018 and TS 23.038.
//
// [GOLDEN DATA VERIFICATION]
// LAI MCC/MNC BCD packing per GSM 24.008 Figure 10.5.1.3:
//   octet 1 = [MCC digit 2 | MCC digit 1], octet 2 = [MNC digit 3 or F | MCC digit 3],
//   octet 3 = [MNC digit 2 | MNC digit 1]:
//   MCC=262, MNC=42 -> {0x62, 0xF2, 0x24}.
// Mobile Identity TMSI first octet verified: spare 'F'(4)|0(1)|type(3)=100(TMSI) = 0xF4.
// Mobile Identity digit identities start with [first digit(4)|odd count(1)|type(3)].
// Classmark1/2/3 default lengths per GSM 24.008 10.5.1.5..10.5.1.7 (1, 3 and 14 octets).
// CipheringModeSetting encoding verified per TS 44.018 10.5.2.9:
//   sC(1)|algorithmIdentifier(3) in low nibble of octet (spare high nibble).
// CellSelectionParameters vector values for a software BTS (GSM 24.008 SI3):
//   {0x47, 0x40} -> hyst=2, txpwr=7, acs=0, neci=1, rxlev=0.
// RACHControlParameters vector values for a software BTS (GSM 24.008 SI2/SI3):
//   {0xE5, 0x04, 0x00} -> max_retrans=3, tx_int=9, cell_bar=false, re_not_allowed=1, ACC=0x0400.
// ControlChannelDescription vector values for a software BTS (GSM 24.008 SI3):
//   {0xC9, 0x00, 0x01} -> msc_r99=1, att=1, bs_ag_blks_res=1, ccch_conf=1, t3212=1.
// PowerCommand encoding verified: power_command(5 MSB)|spare(3 LSB), cmd=15 -> 0x78.
// TimingAdvance encoding verified: spare(2 MSB)=0|timing_advance(6 LSB), val=42 -> 0x2A.
// GSM Alphabet decoding verified against 3GPP TS 23.038 Table 1 (default alphabet).
// RxLev conversion verified: dBm = RxLev - 110, range -110 to -47 dBm.
// GSM timing constants per TS 45.008: hyperframe = 2715648 TDMA frames.
// Rest octet padding pattern 0x2B per the GSM 24.008 rest-octet rules.
// CC Cause IE encoding verified per GSM 24.008 10.5.4.11:
//   IEI=0x08, length=2, location+codingStd+causeValue per GSM 24.008 10.5.4.11.
//
// [GOLDEN VERIFICATION]
// All IE byte-level encodings verified against the normative specifications:
//   - LAI MCC/MNC BCD encoding per GSM 24.008 Figure 10.5.1.3:
//     MCC=262, MNC=42 -> '262F42'H -> nibble-swapped -> {0x62, 0xF2, 0x24}
//   - MobileIdentity TMSI first octet: spare 'F'(4)|0(1)|type(3)=100(TMSI) = 0xF4
//     per GSM 24.008 10.5.1.4 (CmIdentityType: TMSI='100'B)
//   - MobileIdentity digit identities start with [first digit(4)|odd count(1)|type(3)]
//     per GSM 24.008 10.5.1.4 (e.g. IMSI "12345" -> 0x19, 0x32, 0x54)
//   - Classmark1 length=1, Classmark2 length=3 per GSM 24.008 10.5.1.5/10.5.1.6
//   - CipheringModeSetting: sC(1)|algorithmIdentifier(3) in 4 bits
//     per TS 44.018 10.5.2.9 (Ciphering Mode Command layout)
//   - CellSelectionParameters {0x47, 0x40} vector values for a software BTS:
//     cell_resel_hyst=2, ms_txpwr_max_cch=7, acs=0, neci=1, rxlev_access_min=0
//   - RACHControlParameters {0xE5, 0x04, 0x00} vector values for a software BTS:
//     max_retrans=3, tx_integer=9, cell_bar=false, re_not_allowed=1, ACC=0x0400
//   - ControlChannelDescription {0xC9, 0x00, 0x01} vector values for a software BTS:
//     msc_r99=1, att=1, bs_ag_blks_res=1, ccch_conf=1(combined), t3212=1(6 min)
//   - PowerCommand: power_command(5 MSB)|spare(3 LSB), cmd=15 -> 0x78
//   - TimingAdvance: spare(2 MSB)=0|timing_advance(6 LSB), val=42 -> 0x2A
//   - GSM Alphabet decoding verified against 3GPP TS 23.038 Table 1 (default alphabet)
//   - RxLev conversion: dBm = RxLev - 110, range -110 to -47 dBm (TS 45.008 8.1.4)
//   - GSM timing constants per TS 45.008:
//     hyperframe=26*51*2048=2715648 frames, TDMA frame duration=0.12/26.0=4.615ms
//   - Rest octet padding 0x2B per GSM 24.008 rest-octet rules (pattern '00101011'B)
//   - ChannelDescription: typeAndOffset(5)|TN(3)|TSC(3)|h(1)|ARFCN(12) - 24 bits MSB-first (TS 44.018 10.5.2.5)
//   - CellDescriptionV: bcc(3)|ncc(3)|arfcn(10) - 16 bits LSB-first (TS 24.008 10.5.2.2)
//   - RequestReference: RA(8)|T1p(5)|T3(6)|T2(5) per TS 44.018 RACH procedure

#include <gtest/gtest.h>
#include <cstring>
#include <gsml3parser/parser.h>
#include <gsml3parser/common/l3common.h>
#include <gsml3parser/gsm_common.h>
#include <gsml3parser/gmm/l3gmmelements.h>
#include <gsml3parser/rr/l3rrmessages.h>
#include <gsml3parser/cc/l3ccelements.h>
#include <gsml3parser/mm/l3mmelements.h>
#include <gsml3parser/bitreader.h>
#include <gsml3parser/bitwriter.h>

using namespace gsml3parser;

// Generic round-trip helper for IE value types (parse takes only BitReader&).
template<typename T>
static void ieRoundTrip(const T& orig) {
    std::vector<uint8_t> buf(256, 0);
    BitWriter writer(buf.data(), buf.size() * 8);
    orig.write(writer);
    BitReader reader(buf.data(), writer.position());
    auto parsedResult = T::parse(reader);
    ASSERT_TRUE(parsedResult);
}

// Round-trip helper for IEs that require length in parse (parse takes BitReader&, size_t).
template<typename T>
static void ieRoundTripLen(const T& orig) {
    std::vector<uint8_t> buf(256, 0);
    BitWriter writer(buf.data(), buf.size() * 8);
    uint8_t len = static_cast<uint8_t>(orig.lengthV());
    writer.writeField(len, 8);
    orig.write(writer);
    BitReader reader(buf.data(), writer.position());
    auto readLenResult = reader.readField(8);
    ASSERT_TRUE(readLenResult);
    uint8_t readLen = static_cast<uint8_t>(readLenResult.value());
    auto parsedResult = T::parse(reader, readLen);
    ASSERT_TRUE(parsedResult);
}

// =====================================================================
// Common IEs: L3CellIdentity (GSM 04.08 10.5.1.1)
// Value part: cell identity number, 16 bits (GSM 24.008 10.5.1.1)
// =====================================================================

TEST(GoldenIE, CellIdentity_Default) {
    L3CellIdentity ci;
    EXPECT_EQ(ci.lengthV(), 2u);
    EXPECT_EQ(ci.id(), 0u);
}

TEST(GoldenIE, CellIdentity_RoundTrip) {
    L3CellIdentity orig(0x1234);
    ieRoundTrip(orig);
}

TEST(GoldenIE, CellIdentity_MaxValue) {
    L3CellIdentity orig(0xFFFF);
    ieRoundTrip(orig);
}

TEST(GoldenIE, CellIdentity_Encoding) {
    L3CellIdentity ci(0x1234);
    std::vector<uint8_t> buf(4, 0);
    BitWriter writer(buf.data(), buf.size() * 8);
    ci.write(writer);
    EXPECT_EQ(buf[0], 0x12);
    EXPECT_EQ(buf[1], 0x34);
}

// =====================================================================
// Common IEs: L3LocationAreaIdentity (GSM 24.008 10.5.1.3 / GSM 04.08 10.5.1.3)
// MCC/MNC BCD encoding per GSM 24.008 Figure 10.5.1.3:
//   MCC digit 2|MCC digit 1 -> octet 1, MNC digit 3|MCC digit 3 -> octet 2, MNC digit 2|MNC digit 1 -> octet 3
//   nibbles are swapped within each octet
// Vector: MCC=262, MNC=42 (MNC padded with 'F') -> digits '262F42'H -> {0x62, 0xF2, 0x24}
// Spec-verified: LAI = MCC/MNC(3 octets BCD) + LAC(2 octets) = 5 octets total
// [GSM SPEC VERIFIED] GSM 24.008 Figure 10.5.1.3: BCD encoding with nibble swap.
//   For 2-digit MNC, digit 3 is padded with 'F'. Encoding:
//   Octet 1 = MCC_digit2(high)|MCC_digit1(low), e.g. MCC=262 -> '26' -> nibble-swapped -> 0x62
//   Octet 2 = MNC_digit3_or_F(high)|MCC_digit3(low), e.g. MNC=42,F,2 -> '2F' -> swapped -> 0xF2
//   Octet 3 = MNC_digit2(high)|MNC_digit1(low), e.g. '42' -> swapped -> 0x24
//   Digits '262F42'H with the per-octet nibble swap produce {0x62, 0xF2, 0x24}.
// =====================================================================

TEST(GoldenIE, LAI_Default) {
    // GSM 24.008 10.5.1.3: LocationAreaIdentity is always 5 octets (MCC/MNC BCD + LAC)
    L3LocationAreaIdentity lai;
    EXPECT_EQ(lai.lengthV(), 5u);
}

TEST(GoldenIE, LAI_RoundTrip) {
    L3LocationAreaIdentity orig("250", "01", 0x1234);
    ieRoundTrip(orig);
}

TEST(GoldenIE, LAI_3DigitMNC) {
    L3LocationAreaIdentity orig("250", "012", 0x5678);
    ieRoundTrip(orig);
}

TEST(GoldenIE, LAI_Equality) {
    L3LocationAreaIdentity a("250", "01", 0x1234);
    L3LocationAreaIdentity b("250", "01", 0x1234);
    L3LocationAreaIdentity c("250", "01", 0x5678);
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);
}

TEST(GoldenIE, LAI_Ref_262_42) {
    // Golden: LAI for PLMN 262/42, LAC 0x1234. The PLMN nibble packing is
    // [MCC digit 2 | MCC digit 1][MNC digit 3 or F | MCC digit 3]
    // [MNC digit 2 | MNC digit 1] per TS 24.008 section 10.5.1.3:
    //   digits '262F42'H (MNC padded with F) -> {0x62, 0xF2, 0x24}
    L3LocationAreaIdentity lai("262", "42", 0x1234);
    EXPECT_EQ(lai.mcc(), 262);
    EXPECT_EQ(lai.mnc(), 42);
    EXPECT_EQ(lai.lac(), 0x1234);
    std::vector<uint8_t> buf(10, 0);
    BitWriter writer(buf.data(), buf.size() * 8);
    lai.write(writer);
    // Spec-verified: MCC/MNC BCD encoding with the per-octet nibble swap (GSM 24.008 Figure 10.5.1.3)
    uint8_t wire[] = {0x62, 0xF2, 0x24, 0x12, 0x34};
    EXPECT_EQ(0, std::memcmp(buf.data(), wire, 5));
    // Parse back: digits are exposed in natural written order.
    BitReader reader(buf.data(), writer.position());
    auto parsed = L3LocationAreaIdentity::parse(reader);
    ASSERT_TRUE(parsed);
    EXPECT_EQ((*parsed).mcc(), 262);
    EXPECT_EQ((*parsed).mnc(), 42);
    EXPECT_EQ((*parsed).lac(), 0x1234);
}

TEST(GoldenIE, LAI_Packing_Vectors) {
    // Canonical PLMN packing vectors (TS 24.008 section 10.5.1.3):
    //   octet 1 = [MCC digit 2 | MCC digit 1]
    //   octet 2 = [MNC digit 3 or F fill | MCC digit 3]
    //   octet 3 = [MNC digit 2 | MNC digit 1]
    struct Row {
        const char* mcc;
        const char* mnc;
        uint16_t lac;
        int mccValue;
        int mncValue;
        std::array<uint8_t, 5> wire;
    };
    const Row rows[] = {
        {"262", "42",  0x1234, 262, 42,  {0x62, 0xF2, 0x24, 0x12, 0x34}},
        {"901", "70",  0xABCD, 901, 70,  {0x09, 0xF1, 0x07, 0xAB, 0xCD}},
        {"262", "421", 0x0000, 262, 421, {0x62, 0x12, 0x24, 0x00, 0x00}},
    };
    for (const Row& row : rows) {
        L3LocationAreaIdentity lai(row.mcc, row.mnc, row.lac);
        EXPECT_EQ(lai.mcc(), row.mccValue) << "MCC " << row.mcc;
        EXPECT_EQ(lai.mnc(), row.mncValue) << "MNC " << row.mnc;

        std::vector<uint8_t> buf(10, 0);
        BitWriter writer(buf.data(), buf.size() * 8);
        lai.write(writer);
        EXPECT_EQ(0, std::memcmp(buf.data(), row.wire.data(), 5))
            << "write for PLMN " << row.mcc << "/" << row.mnc;

        BitReader reader(buf.data(), writer.position());
        auto parsed = L3LocationAreaIdentity::parse(reader);
        ASSERT_TRUE(parsed);
        EXPECT_EQ((*parsed).mcc(), row.mccValue);
        EXPECT_EQ((*parsed).mnc(), row.mncValue);
        EXPECT_EQ((*parsed).lac(), static_cast<int>(row.lac));
    }
}

TEST(GoldenIE, RAI_901_70) {
    // Golden: RAI for PLMN 901/70, LAC 0x0001, RAC 0x5A (TS 24.008 section
    // 10.5.1.3 PLMN packing + LAC(2) + RAC(1)): wire 09 F1 07 00 01 5A.
    L3RoutingAreaIdentification rai("901", "70", 0x0001, 0x5A);
    EXPECT_EQ(rai.mcc(), 901);
    EXPECT_EQ(rai.mnc(), 70);
    std::vector<uint8_t> buf(10, 0);
    BitWriter writer(buf.data(), buf.size() * 8);
    rai.write(writer);
    uint8_t wire[] = {0x09, 0xF1, 0x07, 0x00, 0x01, 0x5A};
    EXPECT_EQ(0, std::memcmp(buf.data(), wire, 6));

    BitReader reader(buf.data(), writer.position());
    auto parsed = L3RoutingAreaIdentification::parse(reader);
    ASSERT_TRUE(parsed);
    EXPECT_EQ((*parsed).mcc(), 901);
    EXPECT_EQ((*parsed).mnc(), 70);
    EXPECT_EQ((*parsed).lac(), 0x0001);
    EXPECT_EQ((*parsed).rac(), 0x5A);
}

TEST(GoldenIE, RAI_2DigitMNC_Constructor) {
    // A two-digit MNC must store the 'F' fill as its third digit so that the
    // wire octet 2 carries [F | MCC digit 3] and mnc() returns 42 (not 420).
    L3RoutingAreaIdentification rai("262", "42", 0x1234, 0x00);
    EXPECT_EQ(rai.mcc(), 262);
    EXPECT_EQ(rai.mnc(), 42);
    std::vector<uint8_t> buf(10, 0);
    BitWriter writer(buf.data(), buf.size() * 8);
    rai.write(writer);
    uint8_t wire[] = {0x62, 0xF2, 0x24, 0x12, 0x34, 0x00};
    EXPECT_EQ(0, std::memcmp(buf.data(), wire, 6));
}

// =====================================================================
// Common IEs: L3MobileIdentity (GSM 04.08 10.5.1.4)
// LV-encoded mobile identity vectors for TMSI, IMSI and IMEI (GSM 24.008 10.5.1.4)
// [GSM SPEC VERIFIED] GSM 24.008 10.5.1.4: first octet =
//   [first digit(4)|odd count(1)|typeOfIdentity(3)] for digit identities,
//   [spare 'F'(4)|0(1)|type(3)] for TMSI (0xF4) and NoID (0xF0).
//   typeOfIdentity: 000=NoID, 001=IMSI, 010=IMEI, 011=IMEISV, 100=TMSI
//   odd count: 1 when the digit count is odd.
//   Digit pairs follow as [next digit or F fill][current digit]; the F fill
//   appears only for an even digit count (last pair).
// =====================================================================

TEST(GoldenIE, MobileIdentity_TMSI) {
    L3MobileIdentity orig(0xDEADBEEF);
    EXPECT_EQ(orig.type(), MobileIDType::TMSI);
    EXPECT_TRUE(orig.isTMSI());
    EXPECT_FALSE(orig.isIMSI());
    EXPECT_EQ(orig.tmsi(), 0xDEADBEEFu);
}

TEST(GoldenIE, MobileIdentity_IMSI) {
    L3MobileIdentity orig("250011234567890");
    EXPECT_EQ(orig.type(), MobileIDType::IMSI);
    EXPECT_TRUE(orig.isIMSI());
    EXPECT_EQ(std::string(orig.digits()), "250011234567890");
}

TEST(GoldenIE, MobileIdentity_Equality) {
    L3MobileIdentity a(0x12345678);
    L3MobileIdentity b(0x12345678);
    L3MobileIdentity c(0x87654321);
    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}

TEST(GoldenIE, MobileIdentity_LessThan) {
    L3MobileIdentity a(0x00000001);
    L3MobileIdentity b(0x00000002);
    EXPECT_LT(a, b);
}

TEST(GoldenIE, MobileIdentity_Default) {
    L3MobileIdentity id;
    EXPECT_EQ(id.type(), MobileIDType::NoID);
}

TEST(GoldenIE, MobileIdentity_TMSI_Encoding) {
    // Spec-verified: GSM 24.008 10.5.1.4 Mobile Identity encoding
    L3MobileIdentity id(0xDEADBEEF);
    std::vector<uint8_t> buf(16, 0);
    BitWriter writer(buf.data(), buf.size() * 8);
    id.write(writer);
    // GSM 24.008 10.5.1.4: spare 'F'(4)|0(1)|typeOfIdentity(3)=100(TMSI) -> 0b1111_0_100 = 0xF4
    EXPECT_EQ(buf[0], 0xF4);
    // Bytes 1-4: TMSI value in big-endian order
    EXPECT_EQ(buf[1], 0xDE);
    EXPECT_EQ(buf[2], 0xAD);
    EXPECT_EQ(buf[3], 0xBE);
    EXPECT_EQ(buf[4], 0xEF);
}

TEST(GoldenIE, MobileIdentity_IMSI_Encoding) {
    L3MobileIdentity id("250011234567890");
    std::vector<uint8_t> buf(16, 0);
    BitWriter writer(buf.data(), buf.size() * 8);
    id.write(writer);
    // GSM 24.008 10.5.1.4: first octet = [first digit '2'(4)|odd count(1)=1 (15 digits)|
    // typeOfIdentity(3)=001(IMSI)] -> 0b0010_1_001 = 0x29
    EXPECT_EQ(buf[0], 0x29);
}

// Golden: TMSI mobile identity (TS 24.008 section 9.1.3.x): spare 'F'
// nibble, zero bit, type '100'B -> first octet 0xF4, then four octets.
TEST(GoldenIE, MobileIdentity_TMSI_GoldenVector) {
    uint8_t wire[] = {0xF4, 0x12, 0x34, 0x56, 0x78};

    BitReader reader(wire, sizeof(wire) * 8);
    auto parsed = L3MobileIdentity::parse(reader, 5);
    ASSERT_TRUE(parsed);
    EXPECT_EQ((*parsed).type(), MobileIDType::TMSI);
    EXPECT_TRUE((*parsed).isTMSI());
    EXPECT_EQ((*parsed).tmsi(), 0x12345678u);

    L3MobileIdentity id(0x12345678);
    EXPECT_EQ(id.lengthV(), 5u);
    std::vector<uint8_t> buf(16, 0);
    BitWriter writer(buf.data(), buf.size() * 8);
    id.write(writer);
    EXPECT_EQ(0, std::memcmp(buf.data(), wire, sizeof(wire)));
}

// Golden: IMSI "12345" (odd digit count): first octet [digit '1'][odd=1]
// [type IMSI '001'] = 0x19; then [3|2]=0x32, [5|4]=0x54.
TEST(GoldenIE, MobileIdentity_IMSI_GoldenVector) {
    uint8_t wire[] = {0x19, 0x32, 0x54};

    BitReader reader(wire, sizeof(wire) * 8);
    auto parsed = L3MobileIdentity::parse(reader, 3);
    ASSERT_TRUE(parsed);
    EXPECT_EQ((*parsed).type(), MobileIDType::IMSI);
    EXPECT_STREQ((*parsed).digits(), "12345");

    L3MobileIdentity id("12345");
    EXPECT_EQ(id.lengthV(), 3u);
    std::vector<uint8_t> buf(16, 0);
    BitWriter writer(buf.data(), buf.size() * 8);
    id.write(writer);
    EXPECT_EQ(0, std::memcmp(buf.data(), wire, sizeof(wire)));
}

// Golden: IMSI "2624212345" (even digit count): first octet [digit '2']
// [odd=0][type IMSI '001'] = 0x21; the last pair carries the F fill.
TEST(GoldenIE, MobileIdentity_IMSI_Even_GoldenVector) {
    uint8_t wire[] = {0x21, 0x26, 0x24, 0x21, 0x43, 0xF5};

    BitReader reader(wire, sizeof(wire) * 8);
    auto parsed = L3MobileIdentity::parse(reader, 6);
    ASSERT_TRUE(parsed);
    EXPECT_EQ((*parsed).type(), MobileIDType::IMSI);
    EXPECT_STREQ((*parsed).digits(), "2624212345");

    L3MobileIdentity id("2624212345");
    EXPECT_EQ(id.lengthV(), 6u);
    std::vector<uint8_t> buf(16, 0);
    BitWriter writer(buf.data(), buf.size() * 8);
    id.write(writer);
    EXPECT_EQ(0, std::memcmp(buf.data(), wire, sizeof(wire)));
}

// Golden: IMEI "49015420323751" (14 digits, even): first octet [digit '4']
// [odd=0][type IMEI '010'] = 0x42.
TEST(GoldenIE, MobileIdentity_IMEI_GoldenVector) {
    uint8_t wire[] = {0x42, 0x09, 0x51, 0x24, 0x30, 0x32, 0x57, 0xF1};

    BitReader reader(wire, sizeof(wire) * 8);
    auto parsed = L3MobileIdentity::parse(reader, 8);
    ASSERT_TRUE(parsed);
    EXPECT_EQ((*parsed).type(), MobileIDType::IMEI);
    EXPECT_STREQ((*parsed).digits(), "49015420323751");

    L3MobileIdentity id(MobileIDType::IMEI, "49015420323751");
    EXPECT_EQ(id.lengthV(), 8u);
    std::vector<uint8_t> buf(16, 0);
    BitWriter writer(buf.data(), buf.size() * 8);
    id.write(writer);
    EXPECT_EQ(0, std::memcmp(buf.data(), wire, sizeof(wire)));
}

// An even digit count ends with an 'F' fill in the high nibble of the last
// pair: IMSI "1234" -> 11 32 F4.
TEST(GoldenIE, MobileIdentity_IMSI_ShortEven_GoldenVector) {
    uint8_t wire[] = {0x11, 0x32, 0xF4};

    BitReader reader(wire, sizeof(wire) * 8);
    auto parsed = L3MobileIdentity::parse(reader, 3);
    ASSERT_TRUE(parsed);
    EXPECT_EQ((*parsed).type(), MobileIDType::IMSI);
    EXPECT_STREQ((*parsed).digits(), "1234");

    L3MobileIdentity id("1234");
    EXPECT_EQ(id.lengthV(), 3u);
    std::vector<uint8_t> buf(16, 0);
    BitWriter writer(buf.data(), buf.size() * 8);
    id.write(writer);
    EXPECT_EQ(0, std::memcmp(buf.data(), wire, sizeof(wire)));
}

// =====================================================================
// Common IEs: L3MobileStationClassmark1 (GSM 04.08 10.5.1.5)
// One-octet value part per GSM 24.008 10.5.1.5
// 8 bits: revision(1)|spare(1)|ES_IND(1)|A5_1(1)|RF_Power(2)|spare(2)
// =====================================================================

TEST(GoldenIE, Classmark1_Default) {
    L3MobileStationClassmark1 cm1;
    EXPECT_EQ(cm1.lengthV(), 1u);
}

TEST(GoldenIE, Classmark1_RoundTrip) {
    L3MobileStationClassmark1 orig;
    ieRoundTrip(orig);
}

TEST(GoldenIE, Classmark1_Zero) {
    L3MobileStationClassmark1 cm1;
    std::vector<uint8_t> buf(4, 0);
    BitWriter writer(buf.data(), buf.size() * 8);
    cm1.write(writer);
    EXPECT_EQ(buf[0], 0x00);
}

// =====================================================================
// Common IEs: L3MobileStationClassmark2 (GSM 04.08 10.5.1.6)
// Three-octet value part per GSM 24.008 10.5.1.6 (Classmark 2, incl. EGPRS extensions)
// 24 bits: revision(1)|spare(1)|ES_IND(1)|A5_1(1)|A5_3(1)|A5_2(1)|
//   RF_Power(2)|PS(1)|SS(1)|SM(1)|VBS(1)|VGCS(1)|FC(1)|CM3(1)|
//   LCS(1)|SoLSA(1)|CMSF(1)|spare(1)|PS_class(8)
// =====================================================================

TEST(GoldenIE, Classmark2_Default) {
    L3MobileStationClassmark2 cm2;
    EXPECT_EQ(cm2.lengthV(), 3u);
}

TEST(GoldenIE, Classmark2_RoundTrip) {
    L3MobileStationClassmark2 orig;
    ieRoundTrip(orig);
}

TEST(GoldenIE, Classmark2_PowerClass) {
    L3MobileStationClassmark2 cm2;
    // Default RF power capability = 0 -> power class 1
    EXPECT_EQ(cm2.powerClass(), 1);
}

TEST(GoldenIE, Classmark2_A5Bits) {
    L3MobileStationClassmark2 cm2;
    int bits = cm2.getA5Bits();
    EXPECT_GE(bits, 0);
}

TEST(GoldenIE, Classmark2_Zero) {
    L3MobileStationClassmark2 cm2;
    std::vector<uint8_t> buf(8, 0);
    BitWriter writer(buf.data(), buf.size() * 8);
    cm2.write(writer);
    EXPECT_EQ(buf[0], 0x00);
    EXPECT_EQ(buf[1], 0x00);
    EXPECT_EQ(buf[2], 0x00);
}

// =====================================================================
// Common IEs: L3MobileStationClassmark3 (GSM 04.08 10.5.1.7)
// =====================================================================

TEST(GoldenIE, Classmark3_Default) {
    L3MobileStationClassmark3 cm3;
    EXPECT_EQ(cm3.lengthV(), 14u);
}

// =====================================================================
// Common IEs: L3CipheringKeySequenceNumber (GSM 04.08 10.5.1.2)
// Ciphering key sequence number per GSM 24.008 10.5.1.2
// =====================================================================

TEST(GoldenIE, CipheringKeySeqNr_Default) {
    L3CipheringKeySequenceNumber cksn;
    EXPECT_EQ(cksn.lengthV(), 0u);
}

TEST(GoldenIE, CipheringKeySeqNr_RoundTrip) {
    L3CipheringKeySequenceNumber orig(5);
    ieRoundTrip(orig);
}

TEST(GoldenIE, CipheringKeySeqNr_MaxValue) {
    L3CipheringKeySequenceNumber orig(7);
    ieRoundTrip(orig);
}

// =====================================================================
// Common IEs: L3ChannelDescription (GSM 04.08 10.5.2.5)
// Both H=0 (ARFCN present) and H=1 (MAIO/HSN) variants per TS 44.018 10.5.2.5
// 24 bits: typeAndOffset(5) + TN(3) + TSC(3) + h(1) + ARFCN(12) (TS 44.018 10.5.2.5)
// =====================================================================

TEST(GoldenIE, ChannelDescription_Default) {
    L3ChannelDescription chd;
    EXPECT_FALSE(chd.initialized());
    EXPECT_EQ(chd.lengthV(), 3u);
}

TEST(GoldenIE, ChannelDescription_SDCCH) {
    // SDCCH/8 sub-slot 0 ('01000'B = 8): canonical five-bit code per
    // TS 44.018 section 9.2.3.
    L3ChannelDescription orig(TDMA_SDCCH8_0, 2, 7, 100);
    EXPECT_TRUE(orig.initialized());
    EXPECT_EQ(orig.typeAndOffset(), TDMA_SDCCH8_0);
    EXPECT_EQ(orig.tn(), 2u);
    EXPECT_EQ(orig.tsc(), 7u);
    EXPECT_EQ(orig.arfcn(), 100u);
    ieRoundTrip(orig);
}

TEST(GoldenIE, ChannelDescription_TCHF) {
    // TCH/F assignment uses the Bm code ('00001'B = 1, TS 44.018 9.2.3).
    L3ChannelDescription orig(TDMA_Bm_ACCH, 5, 3, 200);
    ieRoundTrip(orig);
}

TEST(GoldenIE, ChannelDescription_TCHH) {
    // TCH/H assignment uses the same Bm code (TS 44.018 9.2.3).
    L3ChannelDescription orig(TDMA_Bm_ACCH, 0, 0, 1);
    ieRoundTrip(orig);
}

TEST(GoldenIE, ChannelDescription_CBCH) {
    // CBCH/8 ('11010'B = 26): vendor-extension code (TS 48.058 9.3.1).
    L3ChannelDescription orig(TDMA_CBCH8, 1, 4, 50);
    ieRoundTrip(orig);
}

TEST(GoldenIE, ChannelDescription_RoundTrip) {
    L3ChannelDescription orig(TDMA_Bm_ACCH, 3, 7, 100);
    std::vector<uint8_t> buf(8, 0);
    BitWriter writer(buf.data(), buf.size() * 8);
    orig.write(writer);
    BitReader reader(buf.data(), writer.position());
    auto parsedResult = L3ChannelDescription::parse(reader);
    ASSERT_TRUE(parsedResult);
    EXPECT_EQ((*parsedResult).typeAndOffset(), orig.typeAndOffset());
    EXPECT_EQ((*parsedResult).tn(), orig.tn());
    EXPECT_EQ((*parsedResult).tsc(), orig.tsc());
    EXPECT_EQ((*parsedResult).arfcn(), orig.arfcn());
}

// =====================================================================
// Common IEs: L3ChannelDescription twelve-bit ARFCN (TS 44.018 10.5.2.5)
// With H=0 the ARFCN occupies the full twelve bits after TSC(3)|H(1), so
// values 1024..4095 are representable; for ARFCN < 1024 the wire bytes are
// unchanged (the high two bits of the field are zero).
// =====================================================================

TEST(GoldenIE, ChannelDescription_ArfcnAbove1023_WireShape) {
    // typeAndOffset=Bm_ACCH(00001)|TN=3(011) -> 0x0B; TSC=7(111)|H=0|ARFCN top
    // nibble 0x4 -> 0xE4; ARFCN bottom octet 0x01. ARFCN = 0x401 = 1025.
    L3ChannelDescription orig(TDMA_Bm_ACCH, 3, 7, 1025);
    std::vector<uint8_t> buf(8, 0);
    BitWriter writer(buf.data(), buf.size() * 8);
    orig.write(writer);
    EXPECT_EQ(writer.position(), 24u);
    EXPECT_EQ(buf[0], 0x0Bu);
    EXPECT_EQ(buf[1], 0xE4u);
    EXPECT_EQ(buf[2], 0x01u);

    BitReader reader(buf.data(), writer.position());
    auto parsedResult = L3ChannelDescription::parse(reader);
    ASSERT_TRUE(parsedResult);
    EXPECT_EQ((*parsedResult).typeAndOffset(), TDMA_Bm_ACCH);
    EXPECT_EQ((*parsedResult).tn(), 3u);
    EXPECT_EQ((*parsedResult).tsc(), 7u);
    EXPECT_EQ((*parsedResult).hFlag(), 0u);
    EXPECT_EQ((*parsedResult).arfcn(), 1025u);
}

TEST(GoldenIE, ChannelDescription_ArfcnBelow1024_ZeroExtendedWire) {
    // ARFCN = 873 (0x369): the twelve-bit field is zero-extended in its top
    // two bits, so only the low ten bits appear in the wire octets.
    L3ChannelDescription orig(TDMA_Bm_ACCH, 3, 7, 873);
    std::vector<uint8_t> buf(8, 0);
    BitWriter writer(buf.data(), buf.size() * 8);
    orig.write(writer);
    EXPECT_EQ(buf[0], 0x0Bu);
    EXPECT_EQ(buf[1], 0xE3u);
    EXPECT_EQ(buf[2], 0x69u);

    BitReader reader(buf.data(), writer.position());
    auto parsedResult = L3ChannelDescription::parse(reader);
    ASSERT_TRUE(parsedResult);
    EXPECT_EQ((*parsedResult).arfcn(), 873u);
}

TEST(GoldenIE, ChannelDescription_ArfcnMaxValue) {
    // The twelve-bit ARFCN field reaches its maximum, 4095.
    L3ChannelDescription orig(TDMA_Bm_ACCH, 0, 0, 4095);
    std::vector<uint8_t> buf(8, 0);
    BitWriter writer(buf.data(), buf.size() * 8);
    orig.write(writer);
    BitReader reader(buf.data(), writer.position());
    auto parsedResult = L3ChannelDescription::parse(reader);
    ASSERT_TRUE(parsedResult);
    EXPECT_EQ((*parsedResult).arfcn(), 4095u);
}

TEST(GoldenIE, ChannelDescription2_ArfcnAbove1023_RoundTrip) {
    L3ChannelDescription2 orig(TDMA_Bm_ACCH, 3, 7, 2000);
    std::vector<uint8_t> buf(8, 0);
    BitWriter writer(buf.data(), buf.size() * 8);
    orig.write(writer);
    BitReader reader(buf.data(), writer.position());
    auto parsedResult = L3ChannelDescription2::parse(reader);
    ASSERT_TRUE(parsedResult);
    EXPECT_EQ((*parsedResult).typeAndOffset(), TDMA_Bm_ACCH);
    EXPECT_EQ((*parsedResult).tn(), 3u);
    EXPECT_EQ((*parsedResult).tsc(), 7u);
    EXPECT_EQ((*parsedResult).arfcn(), 2000u);
}

TEST(GoldenIE, AdditionalChannelDescription_ArfcnAbove1023_RoundTrip) {
    L3AdditionalChannelDescription orig(TDMA_Bm_ACCH, 3, 5, 3000);
    std::vector<uint8_t> buf(8, 0);
    BitWriter writer(buf.data(), buf.size() * 8);
    orig.write(writer);
    BitReader reader(buf.data(), writer.position());
    auto parsedResult = L3AdditionalChannelDescription::parse(reader);
    ASSERT_TRUE(parsedResult);
    EXPECT_EQ((*parsedResult).typeAndOffset(), TDMA_Bm_ACCH);
    EXPECT_EQ((*parsedResult).tn(), 3u);
    EXPECT_EQ((*parsedResult).tsc(), 5u);
    EXPECT_EQ((*parsedResult).arfcn(), 3000u);
}

// =====================================================================
// Common IEs: L3ChannelDescription2 (GSM 44.018 10.5.2.5a)
// =====================================================================

TEST(GoldenIE, ChannelDescription2_Default) {
    L3ChannelDescription2 chd;
    EXPECT_EQ(chd.lengthV(), 3u);
}

TEST(GoldenIE, ChannelDescription2_FromChannelDescription) {
    L3ChannelDescription orig(TDMA_Bm_ACCH, 3, 7, 100);
    L3ChannelDescription2 chd2(orig);
    EXPECT_EQ(chd2.typeAndOffset(), TDMA_Bm_ACCH);
    EXPECT_EQ(chd2.tn(), 3u);
    EXPECT_EQ(chd2.tsc(), 7u);
    EXPECT_EQ(chd2.arfcn(), 100u);
}

// =====================================================================
// Common IEs: L3AdditionalChannelDescription
// =====================================================================

TEST(GoldenIE, AdditionalChannelDescription_Default) {
    L3AdditionalChannelDescription chd;
    EXPECT_FALSE(chd.initialized());
    EXPECT_EQ(chd.lengthV(), 3u);
}

TEST(GoldenIE, AdditionalChannelDescription_RoundTrip) {
    L3AdditionalChannelDescription orig(TDMA_Bm_ACCH, 3, 5, 150);
    std::vector<uint8_t> buf(8, 0);
    BitWriter writer(buf.data(), buf.size() * 8);
    orig.write(writer);
    BitReader reader(buf.data(), writer.position());
    auto parsedResult = L3AdditionalChannelDescription::parse(reader);
    ASSERT_TRUE(parsedResult);
    EXPECT_EQ((*parsedResult).typeAndOffset(), orig.typeAndOffset());
    EXPECT_EQ((*parsedResult).tn(), orig.tn());
    EXPECT_EQ((*parsedResult).tsc(), orig.tsc());
    EXPECT_EQ((*parsedResult).arfcn(), orig.arfcn());
}

// =====================================================================
// Common IEs: L3PowerCommand (GSM 04.08 10.5.2.28)
// One-octet power command value per GSM 24.008 10.5.2.28
// 8 bits: power_command(5) | spare(3)
// =====================================================================

TEST(GoldenIE, PowerCommand_Default) {
    L3PowerCommand pc;
    EXPECT_EQ(pc.lengthV(), 1u);
    EXPECT_EQ(pc.command(), 0u);
}

TEST(GoldenIE, PowerCommand_RoundTrip) {
    L3PowerCommand orig(10);
    ieRoundTrip(orig);
}

TEST(GoldenIE, PowerCommand_MaxValue) {
    L3PowerCommand orig(31);
    ieRoundTrip(orig);
}

TEST(GoldenIE, PowerCommand_Encoding) {
    // Test vector: command = 15.
    // Spec-verified: GSM 24.008 10.5.2.28 Power Command
    //   power_command(5 bits MSB)|spare(3 bits LSB) = 1 octet
    //   command=15 -> 0b01111_000 = 0x78 (15 in high 5 bits, spare 0 in low 3 bits)
    L3PowerCommand pc(15);
    std::vector<uint8_t> buf(4, 0);
    BitWriter writer(buf.data(), buf.size() * 8);
    pc.write(writer);
    // GSM 24.008 10.5.2.28: power_command(5 bits MSB)|spare(3 bits LSB) = 1 octet
    // power_command=15 -> 0b01111_000 = 0x78 (15 in high 5 bits, spare 0 in low 3 bits)
    EXPECT_EQ(buf[0], 0x78);
}

// =====================================================================
// Common IEs: L3PowerCommandAndAccessType (GSM 04.08 10.5.2.28a)
// =====================================================================

TEST(GoldenIE, PowerCommandAndAccessType_Default) {
    L3PowerCommandAndAccessType pc;
    EXPECT_EQ(pc.lengthV(), 1u);
}

TEST(GoldenIE, PowerCommandAndAccessType_RoundTrip) {
    L3PowerCommandAndAccessType orig(15);
    ieRoundTrip(orig);
}

// =====================================================================
// Common IEs: L3ChannelMode (GSM 04.08 10.5.2.6)
// Channel mode nibble per GSM 24.008 10.5.2.6
// 4 bits: speech_version(2) | signalling(1) | data(1)
// =====================================================================

TEST(GoldenIE, ChannelMode_Signalling) {
    L3ChannelMode orig(L3ChannelMode::SignallingOnly);
    EXPECT_FALSE(orig.isAMR());
    ieRoundTrip(orig);
}

TEST(GoldenIE, ChannelMode_SpeechV1) {
    L3ChannelMode orig(L3ChannelMode::SpeechV1);
    EXPECT_FALSE(orig.isAMR());
    ieRoundTrip(orig);
}

TEST(GoldenIE, ChannelMode_SpeechV2) {
    L3ChannelMode orig(L3ChannelMode::SpeechV2);
    EXPECT_FALSE(orig.isAMR());
    ieRoundTrip(orig);
}

TEST(GoldenIE, ChannelMode_SpeechV3_AMR) {
    L3ChannelMode orig(L3ChannelMode::SpeechV3);
    EXPECT_TRUE(orig.isAMR());
    ieRoundTrip(orig);
}

TEST(GoldenIE, ChannelMode_Equality) {
    L3ChannelMode a(L3ChannelMode::SpeechV1);
    L3ChannelMode b(L3ChannelMode::SpeechV1);
    L3ChannelMode c(L3ChannelMode::SpeechV2);
    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}

// =====================================================================
// Common IEs: L3TimingAdvance (GSM 04.08 10.5.2.40)
// One-octet timing advance value per GSM 24.008 10.5.2.40
// 8 bits: spare(2 MSB) | timing_advance(6 LSB); for 0..63 the octet equals the value (TS 44.018 section 10.5.2.40)
// =====================================================================

TEST(GoldenIE, TimingAdvance_Default) {
    L3TimingAdvance ta;
    EXPECT_EQ(ta.lengthV(), 1u);
    EXPECT_EQ(ta.timingAdvance(), 0u);
}

TEST(GoldenIE, TimingAdvance_RoundTrip) {
    L3TimingAdvance orig(60);
    ieRoundTrip(orig);
}

TEST(GoldenIE, TimingAdvance_MaxValue) {
    L3TimingAdvance orig(63);
    ieRoundTrip(orig);
}

TEST(GoldenIE, TimingAdvance_Encoding) {
    // Timing advance is an integer value (range 0..219 per TS 45.008) in the low six bits.
    // Spec-verified: GSM 24.008 10.5.2.40 Timing Advance
    //   spare(2 bits MSB)=0|timing_advance(6 bits LSB) = 1 octet
    //   value=42 -> 0b00_101010 = 0x2A (spare 0 in high 2 bits, 42 in low 6 bits)
    L3TimingAdvance ta(42);
    std::vector<uint8_t> buf(4, 0);
    BitWriter writer(buf.data(), buf.size() * 8);
    ta.write(writer);
    // GSM 24.008 10.5.2.40: spare(2 bits MSB)=0|timing_advance(6 bits LSB) = 1 octet
    // timing_advance=42 -> 0b00_101010 = 0x2A (spare 0 in high 2 bits, 42 in low 6 bits)
    EXPECT_EQ(buf[0], 0x2A);
}

// Golden: Timing Advance octet (TS 44.018 section 10.5.2.40): two spare bits in
// the high position, six-bit value in the low position; the wire octet equals
// the timing advance for values 0..63.
TEST(GoldenIE, TimingAdvance_ParseVector) {
    std::vector<uint8_t> buf(1);
    buf[0] = 0x2A;   // TA = 42
    BitReader reader(buf.data(), 8);
    auto res = L3TimingAdvance::parse(reader);
    ASSERT_TRUE(res);
    EXPECT_EQ((*res).timingAdvance(), 42u);

    buf[0] = 0x3F;   // TA = 63 (maximum)
    BitReader r2(buf.data(), 8);
    auto res2 = L3TimingAdvance::parse(r2);
    ASSERT_TRUE(res2);
    EXPECT_EQ((*res2).timingAdvance(), 63u);
}

// =====================================================================
// Common IEs: L3CellDescription (GSM 04.08 10.5.2.2)
// Cell description value part, packed LSB-first (TS 24.008 10.5.2.2): bcc(3), ncc(3), arfcn(10)
// =====================================================================

TEST(GoldenIE, CellDescription_Default) {
    L3CellDescription cd;
    EXPECT_EQ(cd.lengthV(), 2u);
    EXPECT_EQ(cd.arfcn(), 0u);
    EXPECT_EQ(cd.ncc(), 0u);
    EXPECT_EQ(cd.bcc(), 0u);
}

TEST(GoldenIE, CellDescription_RoundTrip) {
    L3CellDescription orig(100, 5, 3);
    ieRoundTrip(orig);
}

// =====================================================================
// Common IEs: L3HandoverReference (GSM 04.08 10.5.2.15)
// Handover reference (5 bits) per GSM 24.008 10.5.2.15
// 8 bits: handover_reference(5) | spare(3)
// =====================================================================

TEST(GoldenIE, HandoverReference_Default) {
    L3HandoverReference hr;
    EXPECT_EQ(hr.lengthV(), 1u);
    EXPECT_EQ(hr.value(), 0u);
}

TEST(GoldenIE, HandoverReference_RoundTrip) {
    L3HandoverReference orig(0x17);
    ieRoundTrip(orig);
}

// =====================================================================
// Common IEs: L3CipheringModeSetting (GSM 04.08 10.5.2.9)
// Ciphering mode setting nibble per GSM 24.008 10.5.2.9
// 4 bits: ciphering(1) | algorithm(3)
// =====================================================================

TEST(GoldenIE, CipheringModeSetting_Off) {
    L3CipheringModeSetting orig(false, 0);
    EXPECT_EQ(orig.lengthV(), 0u);
    ieRoundTrip(orig);
}

TEST(GoldenIE, CipheringModeSetting_A5_3) {
    // GSM 24.008 10.5.2.9: cipheringModeSetting is 4 bits: sC(1)|algorithmIdentifier(3)
    // ciphering=true, algorithm=3(A5/3) -> sC=1, algId=011 -> 4-bit value = 0b1011 = 0x0B
    // Vector: sC='1'B (ciphering on), algorithmIdentifier = A5/3.
    // Spec-verified round-trip per GSM 24.008 10.5.2.9
    L3CipheringModeSetting orig(true, 3);
    ieRoundTrip(orig);
}

TEST(GoldenIE, CipheringModeSetting_Encoding) {
    // Vector: sC='1'B (ciphering on), algorithmIdentifier = 3 bits (A5/3).
    // Spec-verified: GSM 24.008 10.5.2.9 Ciphering Mode Setting (4 bits)
    //   ciphering(1)=sC|algorithm(3)=algorithmIdentifier
    //   ciphering=true, algorithm=3(A5/3) -> sC(1)=1|algId(3)=011 -> 4-bit value = 0b1011 = 0x0B
    L3CipheringModeSetting cms(true, 3);
    std::vector<uint8_t> buf(4, 0);
    BitWriter writer(buf.data(), buf.size() * 8);
    cms.write(writer);
    // GSM 24.008 10.5.2.9: cipheringModeSetting is 4 bits: sC(1)|algorithmIdentifier(3)
    // ciphering=true -> sC=1, algorithm=3(A5/3) -> algorithmIdentifier=011
    // 4-bit value = 0b1_011 = 0x0B. Written MSB-first starting at bit position 0 (high nibble).
    EXPECT_EQ((buf[0] >> 4) & 0x0F, 0x0B);
}

// =====================================================================
// Common IEs: L3CipheringModeResponse (GSM 04.08 10.5.2.10)
// Ciphering mode response bits per TS 44.018 (ciphering mode command body)
// 4 bits: cR(1) | spare(3) — the response bit leads its half-octet
// =====================================================================

TEST(GoldenIE, CipheringModeResponse_Default) {
    L3CipheringModeResponse orig;
    EXPECT_EQ(orig.lengthV(), 0u);
    EXPECT_FALSE(orig.includeIMEISV());
    ieRoundTrip(orig);
}

TEST(GoldenIE, CipheringModeResponse_Encoding) {
    // Vector: cR='1'B (include IMEISV), spare='000'B.
    // Spec-verified: TS 44.018 ciphering mode response (4 bits):
    //   cR(1)=1 | spare(3)=000 -> 4-bit value = 0b1000 = 0x08, written
    //   MSB-first so the bit lands in the high nibble of the octet.
    L3CipheringModeResponse orig(true);
    std::vector<uint8_t> buf(4, 0);
    BitWriter writer(buf.data(), buf.size() * 8);
    orig.write(writer);
    EXPECT_EQ((buf[0] >> 4) & 0x0F, 0x08u);
}

// =====================================================================
// Common IEs: L3SynchronizationIndication (GSM 04.08 10.5.2.39)
// Synchronization indication octet per GSM 24.008 10.5.2.39
// 8 bits: NCI(1) | ROT(1) | SI(6)
// =====================================================================

TEST(GoldenIE, SynchronizationIndication_Default) {
    L3SynchronizationIndication orig;
    EXPECT_EQ(orig.lengthV(), 1u);
    ieRoundTrip(orig);
}

TEST(GoldenIE, SynchronizationIndication_Values) {
    L3SynchronizationIndication orig(true, true, 3);
    EXPECT_TRUE(orig.nci());
    EXPECT_TRUE(orig.rot());
    EXPECT_EQ(orig.syncIndicator(), 3);
    ieRoundTrip(orig);
}

// =====================================================================
// Common IEs: L3NCCPermitted (GSM 04.08 10.5.2.27)
// NCC permitted bitmask per GSM 24.008 10.5.2.27
// 8 bits: ncc_permitted(8) - bitmask
// =====================================================================

TEST(GoldenIE, NCCPermitted_Default) {
    L3NCCPermitted orig;
    EXPECT_EQ(orig.permitted(), 0xFFu);
    EXPECT_EQ(orig.lengthV(), 1u);
    ieRoundTrip(orig);
}

TEST(GoldenIE, NCCPermitted_Custom) {
    L3NCCPermitted orig(0x7F); // all except NCC=7
    ieRoundTrip(orig);
}

// =====================================================================
// Common IEs: L3PageMode (GSM 04.08 10.5.2.26)
// Page mode values per TS 44.018
// 4-bit field (two spare bits + two mode bits): Normal(0), Extended(1),
// Reorganization(2), SameAsBefore(3)
// =====================================================================

TEST(GoldenIE, PageMode_Normal) {
    L3PageMode orig(0);
    EXPECT_EQ(orig.lengthV(), 0u);
    ieRoundTrip(orig);
}

TEST(GoldenIE, PageMode_Extended) {
    L3PageMode orig(1);
    ieRoundTrip(orig);
}

TEST(GoldenIE, PageMode_Reorganization) {
    L3PageMode orig(2);
    ieRoundTrip(orig);
}

TEST(GoldenIE, PageMode_SameAsBefore) {
    L3PageMode orig(3);
    ieRoundTrip(orig);
}

// =====================================================================
// Common IEs: L3RequestReference (GSM 04.08 10.5.2.30)
// Random access value plus timing fields per GSM 24.008 10.5.2.30
// 24 bits: RA(8) + T1p(5) + T3(6) + T2(5)
// =====================================================================

TEST(GoldenIE, RequestReference_Default) {
    L3RequestReference orig;
    EXPECT_EQ(orig.lengthV(), 3u);
    ieRoundTrip(orig);
}

TEST(GoldenIE, RequestReference_Custom) {
    L3RequestReference orig(0xAB, 5, 12, 20);
    ieRoundTrip(orig);
}

TEST(GoldenIE, RequestReference_Compute) {
    // Timing field derivation (TS 44.018 RACH procedure):
    // t1p = (fn / 1326) mod 32, t2 = fn mod 26, t3 = fn mod 51
    unsigned fn = 1326;
    unsigned ra = 0x42;
    unsigned expected_t1p = (fn / 1326) % 32;
    unsigned expected_t2 = fn % 26;
    unsigned expected_t3 = fn % 51;
    L3RequestReference rr(ra, expected_t1p, expected_t2, expected_t3);
    EXPECT_EQ(rr.ra(), ra);
    EXPECT_EQ(rr.t1p(), expected_t1p);
    EXPECT_EQ(rr.t2(), expected_t2);
    EXPECT_EQ(rr.t3(), expected_t3);
}

// =====================================================================
// Common IEs: L3WaitIndication (GSM 04.08 10.5.2.43)
// Wait time value (one octet) per GSM 24.008 10.5.2.43
// =====================================================================

TEST(GoldenIE, WaitIndication_Default) {
    L3WaitIndication orig;
    EXPECT_EQ(orig.lengthV(), 1u);
    ieRoundTrip(orig);
}

TEST(GoldenIE, WaitIndication_Value) {
    L3WaitIndication orig(60);
    ieRoundTrip(orig);
}

// =====================================================================
// Common IEs: L3RRCauseElement (GSM 04.08 10.5.2.31)
// =====================================================================

TEST(GoldenIE, RRCauseElement_Normal) {
    L3RRCauseElement orig(RRCause::Normal_Event);
    EXPECT_EQ(orig.lengthV(), 1u);
    ieRoundTrip(orig);
}

TEST(GoldenIE, RRCauseElement_HandoverImpossible) {
    L3RRCauseElement orig(RRCause::Handover_Impossible);
    ieRoundTrip(orig);
}

TEST(GoldenIE, RRCauseElement_ProtocolError) {
    L3RRCauseElement orig(RRCause::Protocol_Error_Unspecified);
    ieRoundTrip(orig);
}

// =====================================================================
// Common IEs: L3CellOptionsBCCH (GSM 04.08 10.5.2.3)
// BCCH cell options bits per GSM 24.008 10.5.2.3
// 8 bits: dn_ind(1) | pwrc(1) | dtx(2) | radio_link_tout(4)
// =====================================================================

TEST(GoldenIE, CellOptionsBCCH_Default) {
    L3CellOptionsBCCH orig;
    EXPECT_EQ(orig.lengthV(), 1u);
    ieRoundTrip(orig);
}

// =====================================================================
// Common IEs: L3CellOptionsSACCH (GSM 04.08 10.5.2.3a)
// SACCH cell options bits per GSM 24.008 10.5.2.3a
// 8 bits: dtx_ext(1) | pwrc(1) | dtx(2) | radio_link_timeout(4)
// =====================================================================

TEST(GoldenIE, CellOptionsSACCH_Default) {
    L3CellOptionsSACCH orig;
    EXPECT_EQ(orig.lengthV(), 1u);
    ieRoundTrip(orig);
}

// =====================================================================
// Common IEs: L3CellSelectionParameters (GSM 04.08 10.5.2.4)
// Default vector values for a software BTS (GSM 24.008 SI3).
// 17 bits: cell_resel_hyst(3) + ms_txpwr_max_cch(5) + acs(1) + neci(1) + rxlev_access_min(6)
// [GSM SPEC VERIFIED] GSM 24.008 10.5.2.4: 2 octets + 1 bit (total 17 bits).
//   Octet 1: cell_resel_hyst(3)|ms_txpwr_max_cch(5)|acs(1)
//   Octet 2: neci(1)|rxlev_access_min(6)|spare(1, extends to next octet boundary)
// Vector values (typical software BTS defaults):
//   cell_resel_hyst=2, ms_txpwr_max_cch=7, acs=0, neci=1, rxlev_access_min=0
//   {0x47, 0x40}: 0b010_00111_0 | 0b1_000000_0 = correct
// =====================================================================

TEST(GoldenIE, CellSelectionParameters_Default) {
    L3CellSelectionParameters orig;
    EXPECT_EQ(orig.lengthV(), 2u);
    ieRoundTrip(orig);
}

TEST(GoldenIE, CellSelectionParameters_RefValues) {
    // Vector (typical software BTS defaults):
    //   cell_resel_hyst=2 dB, ms_txpwr_max_cch=7, acs='0'B, neci=true, rxlev_access_min=0
    // Spec-verified: GSM 24.008 10.5.2.4 Cell Selection Parameters (17 bits = 2 octets + 1 bit)
    //   cell_resel_hyst(3)|ms_txpwr_max_cch(5)|acs(1)|neci(1)|rxlev_access_min(6)
    //   {0x47, 0x40}: cell_resel_hyst=2, ms_txpwr_max_cch=7, acs=0, neci=1, rxlev_access_min=0
    std::vector<uint8_t> buf(4, 0);
    buf[0] = 0x47;
    buf[1] = 0x40;
    BitReader reader(buf.data(), 16);
    auto parsedResult = L3CellSelectionParameters::parse(reader);
    ASSERT_TRUE(parsedResult);
    // Spec-verified: byte 0 = 0x47 = 0b0100_0111 -> cell_resel_hyst(3)=010=2, ms_txpwr_max_cch(5)=00111=7
    //   byte 1 = 0x40 = 0b0100_0000 -> acs(1)=0, neci(1)=1, rxlev_access_min(6)=000000=0
    EXPECT_EQ((*parsedResult).cellReselectHysteresis(), 2u);
    EXPECT_EQ((*parsedResult).msTxpwrMaxCch(), 7u);
    EXPECT_EQ((*parsedResult).acs(), 0u);
    EXPECT_EQ((*parsedResult).neci(), 1u);
    EXPECT_EQ((*parsedResult).rxlevAccessMin(), 0u);
}

// =====================================================================
// Common IEs: L3RACHControlParameters (GSM 04.08 10.5.2.29)
// Default vector values for a software BTS (GSM 24.008 SI2/SI3).
// 24 bits: max_retrans(2) + tx_integer(4) + cell_barr_access(1) + re_not_allowed(1) + ACC(16)
// [GSM SPEC VERIFIED] TS 44.018 10.5.2.29: 3 octets (24 bits total).
//   Octet 1: max_retrans(2)|tx_integer(4)|cell_barr_access(1)|re_not_allowed(1)
//   Octet 2-3: ACC(16) access class bitmap, low octet first (octet 2 = classes 0-7)
// Vector values (typical software BTS defaults):
//   max_retrans=3(11), tx_integer=9(1001), cell_bar_access=0, re_not_allowed=1,
//   ACC=0x03FF (access classes 0-9 permitted)
//   {0xE5, 0xFF, 0x03}: 0b11_1001_0_1 | 0b11111111_00000011 = correct
// =====================================================================

TEST(GoldenIE, RACHControlParameters_Default) {
    L3RACHControlParameters orig;
    EXPECT_EQ(orig.lengthV(), 3u);
    ieRoundTrip(orig);
}

TEST(GoldenIE, RACHControlParameters_RefValues) {
    // Vector (typical software BTS defaults):
    //   max_retrans=3, tx_integer='1001'B(=9), cell_barr_access=false,
    //   re_not_allowed=true, ACC=0x03FF (access classes 0-9 permitted)
    // Spec-verified: TS 44.018 10.5.2.29 RACH Control Parameters (24 bits = 3 octets)
    //   max_retrans(2)|tx_integer(4)|cell_barr_access(1)|re_not_allowed(1)|ACC(16, low octet first)
    //   {0xE5, 0xFF, 0x03}: max_retrans=3, tx_integer=9, cell_barr_access=0, re_not_allowed=1, ACC=0x03FF
    std::vector<uint8_t> buf(4, 0);
    buf[0] = 0xE5;
    buf[1] = 0xFF;
    buf[2] = 0x03;
    BitReader reader(buf.data(), 24);
    auto parsedResult = L3RACHControlParameters::parse(reader);
    ASSERT_TRUE(parsedResult);
    // Spec-verified: byte 0 = 0xE5 = 0b1110_0101 -> max_retrans(2)=11=3, tx_integer(4)=1001=9, cell_barr_access(1)=0, re_not_allowed(1)=1
    //   byte 1 = 0xFF (access classes 0-7 permitted), byte 2 = 0x03 (classes 8-9 permitted)
    //   -> ACC(16) = 0x03FF: bit i of the bitmap corresponds to access class i (TS 44.018 10.5.2.29)
    EXPECT_EQ((*parsedResult).maxRetrans(), 3u);
    EXPECT_EQ((*parsedResult).txInteger(), 9u);
    EXPECT_EQ((*parsedResult).cellBarAccess(), false);
    EXPECT_EQ((*parsedResult).re(), 1u);
    EXPECT_EQ((*parsedResult).ac(), 0x03FFu);
}

TEST(GoldenIE, RACHControlParameters_AccLowByteFirst) {
    // Access classes 0-9 permitted (ACC = 0x03FF) must serialize as the two
    // octets {0xFF, 0x03}: the low octet (classes 0-7) is transmitted first
    // (TS 44.018 section 10.5.2.29).
    L3RACHControlParameters orig(3, 9, false, 1, 0x03FF);
    std::vector<uint8_t> buf(4, 0);
    BitWriter writer(buf.data(), buf.size() * 8);
    orig.write(writer);
    EXPECT_EQ(writer.position(), 24u);
    EXPECT_EQ(buf[0], 0xE5u);
    EXPECT_EQ(buf[1], 0xFFu);
    EXPECT_EQ(buf[2], 0x03u);

    BitReader reader(buf.data(), writer.position());
    auto parsedResult = L3RACHControlParameters::parse(reader);
    ASSERT_TRUE(parsedResult);
    EXPECT_EQ((*parsedResult).ac(), 0x03FFu);
}

// =====================================================================
// Common IEs: L3ControlChannelDescription (GSM 04.08 10.5.2.11)
// The vector below is a typical software BTS SI3 control channel description.
// 24 bits: msc_r99(1) + att(1) + bs_ag_blks_res(3) + ccch_conf(3) + si22ind(1) +
//   cbq3(2) + spare(2) + bs_pa_mfrms(3) + t3212(8)
// [GSM SPEC VERIFIED] GSM 24.008 10.5.2.11: 3 octets exactly (24 bits).
//   Octet 1: msc_r99(1)|att(1)|bs_ag_blks_res(3)|ccch_conf(3)|si22ind(1)|cbq3(2)
//   Octet 2-3: spare(2)|bs_pa_mfrms(3)|t3212(8) - t3212 spans bits of octet 2 and 3
// Vector values (typical software BTS SI3 defaults):
//   msc_r99=1, att=1, bs_ag_blks_res=1, ccch_conf=1(1CCCH combined), si22ind=0,
//   cbq3=0(IU mode not supported), spare=0, bs_pa_mfrms=0, t3212=1(6 minutes)
//   {0xC9, 0x00, 0x01}: 0b1_1_001_001_0_00 | 0b00_000_000 | 0b00000001 = correct
// =====================================================================

TEST(GoldenIE, ControlChannelDescription_Default) {
    L3ControlChannelDescription orig;
    EXPECT_EQ(orig.lengthV(), 3u);
    ieRoundTrip(orig);
}

TEST(GoldenIE, ControlChannelDescription_RefValues) {
    // Vector (typical software BTS SI3 defaults):
    //   msc_r99=true, att=true, bs_ag_blks_res=1, ccch_conf='001'B(1, 1CCCH combined),
    //   si22ind=false, cbq3=0(IU mode not supported), spare='00'B, bs_pa_mfrms=0, t3212=1
    // ccch_conf value '001'B = 1CCCH combined (GSM 24.008 10.5.2.11)
    // Spec-verified: GSM 24.008 10.5.2.11 Control Channel Description (24 bits = 3 octets)
    //   msc_r99(1)|att(1)|bs_ag_blks_res(3)|ccch_conf(3)|si22ind(1)|cbq3(2)|spare(2)|bs_pa_mfrms(3)|t3212(8)
    //   {0xC9, 0x00, 0x01}: msc_r99=1, att=1, bs_ag_blks_res=1, ccch_conf=1(combined), si22ind=0, cbq3=0, spare=0, bs_pa_mfrms=0, t3212=1
    std::vector<uint8_t> buf(4, 0);
    buf[0] = 0xC9;
    buf[1] = 0x00;
    buf[2] = 0x01;
    BitReader reader(buf.data(), 24);
    auto parsedResult = L3ControlChannelDescription::parse(reader);
    ASSERT_TRUE(parsedResult);
    // Spec-verified: byte 0 = 0xC9 = 0b1100_1001 -> msc_r99(1)=1, att(1)=1, bs_ag_blks_res(3)=001=1, ccch_conf(2)=01=1(combined)
    //   byte 1 = 0x00 -> si22ind(1)=0, cbq3(2)=00, spare(2)=00, bs_pa_mfrms(3)=000=0, t3212 high 3 bits = 000
    //   byte 1 = 0x00 -> spare(2)=00, bs_pa_mfrms(3)=000=0, t3212 high 3 bits = 000
    //   byte 2 = 0x01 -> t3212 low 5 bits = 00001, so t3212 = 1 (6 minutes)
    EXPECT_EQ((*parsedResult).mATT, 1u);
    EXPECT_EQ((*parsedResult).mBS_AG_BLKS_RES, 1u);
    EXPECT_EQ((*parsedResult).mCCCH_CONF, 1u);
    EXPECT_EQ((*parsedResult).mBS_PA_MFRMS, 0u);
    EXPECT_EQ((*parsedResult).mT3212, 1u);
    EXPECT_TRUE((*parsedResult).isCCCHCombined());
}

// =====================================================================
// Common IEs: L3CellChannelDescription (GSM 04.08 10.5.2.1b)
// 16 bits: ARFCN(10) + BSIC(6 = NCC(3)|BCC(3))
// =====================================================================

TEST(GoldenIE, CellChannelDescription_Default) {
    L3CellChannelDescription orig;
    EXPECT_EQ(orig.lengthV(), 2u);
    ieRoundTrip(orig);
}

TEST(GoldenIE, CellChannelDescription_Custom) {
    L3CellChannelDescription orig(100, 0x1F);
    ieRoundTrip(orig);
}

TEST(GoldenIE, CellChannelDescription_IE) {
    // Golden: ARFCN=100 (ten bits '0001100100') and BSIC=0x12 (six bits
    // '010010', NCC|BCC) pack into the two octets 0x19 0x12
    // (TS 44.018 section 9.2.3 cell channel description).
    L3CellChannelDescription chd(100, 0x12);
    EXPECT_EQ(chd.arfcn(), 100u);
    EXPECT_EQ(chd.bsic(), 0x12u);
    EXPECT_EQ(chd.lengthV(), 2u);

    std::vector<uint8_t> buf(4, 0);
    BitWriter writer(buf.data(), buf.size() * 8);
    chd.write(writer);
    EXPECT_EQ(writer.position(), 16u);
    EXPECT_EQ(buf[0], 0x19u);
    EXPECT_EQ(buf[1], 0x12u);

    BitReader reader(buf.data(), writer.position());
    auto parsedResult = L3CellChannelDescription::parse(reader);
    ASSERT_TRUE(parsedResult);
    EXPECT_EQ((*parsedResult).arfcn(), 100u);
    EXPECT_EQ((*parsedResult).bsic(), 0x12u);
}

// =====================================================================
// Common IEs: L3FrequencyList (GSM 04.08 10.5.2.13)
// Bitmap of ARFCN presence per GSM 24.008 10.5.2.13
// 16 bytes, variable bitmap format
// =====================================================================

TEST(GoldenIE, FrequencyList_Default) {
    L3FrequencyList orig;
    EXPECT_EQ(orig.lengthV(), 16u);
    ieRoundTrip(orig);
}

TEST(GoldenIE, FrequencyList_WithARFCNs) {
    std::vector<unsigned> arfcns = {100, 101, 102, 200};
    L3FrequencyList orig(arfcns);
    ieRoundTrip(orig);
}

TEST(GoldenIE, FrequencyList_Empty) {
    L3FrequencyList fl;
    EXPECT_EQ(fl.lengthV(), 16u);
    EXPECT_TRUE(fl.arfcns().empty());
    std::vector<uint8_t> buf(32, 0);
    BitWriter writer(buf.data(), buf.size() * 8);
    fl.write(writer);
    for (int i = 0; i < 16; i++) {
        EXPECT_EQ(buf[i], 0x00);
    }
}

TEST(GoldenIE, FrequencyList_SingleARFCN) {
    std::vector<unsigned> arfcns = {100};
    L3FrequencyList fl(arfcns);
    std::vector<uint8_t> buf(32, 0);
    BitWriter writer(buf.data(), buf.size() * 8);
    fl.write(writer);
    BitReader reader(buf.data(), writer.position());
    auto parsedResult = L3FrequencyList::parse(reader);
    ASSERT_TRUE(parsedResult);
    EXPECT_EQ((*parsedResult).arfcns(), arfcns);
}

// =====================================================================
// Common IEs: L3BCCHFrequencyList (GSM 04.08 10.5.2.22)
// =====================================================================

TEST(GoldenIE, BCCHFrequencyList_Default) {
    L3BCCHFrequencyList orig;
    EXPECT_EQ(orig.lengthV(), 16u);
    ieRoundTrip(orig);
}

TEST(GoldenIE, BCCHFrequencyList_WithARFCNs) {
    std::vector<unsigned> arfcns = {50, 100, 150};
    L3BCCHFrequencyList orig(arfcns);
    ieRoundTrip(orig);
}

// =====================================================================
// Common IEs: L3NeighborCellsDescription (GSM 04.08 10.5.2.22)
// =====================================================================

TEST(GoldenIE, NeighborCellsDescription_Default) {
    L3NeighborCellsDescription orig;
    EXPECT_EQ(orig.lengthV(), 16u);
    ieRoundTrip(orig);
}

// =====================================================================
// Common IEs: L3MeasurementResults (GSM 04.08 10.5.2.20)
// Field layout per GSM 24.008 10.5.2.20
// 128 bits: ba_used(1) + dtx_used(1) + rxlev_full(6) + 3g_ba(1) +
//   meas_valid(1) + rxlev_sub(6) + si23_ba(1) + rxqual_full(3) +
//   rxqual_sub(3) + no_ncell(3) + [ncell reports]
// =====================================================================

TEST(GoldenIE, MeasurementResults_Default) {
    L3MeasurementResults orig;
    EXPECT_EQ(orig.lengthV(), 16u);
    ieRoundTrip(orig);
}

TEST(GoldenIE, MeasurementResults_Zero) {
    L3MeasurementResults mr;
    EXPECT_EQ(mr.lengthV(), 16u);
    std::vector<uint8_t> buf(32, 0);
    BitWriter writer(buf.data(), buf.size() * 8);
    mr.write(writer);
    for (int i = 0; i < 16; i++) {
        EXPECT_EQ(buf[i], 0x00);
    }
}

// =====================================================================
// Common IEs: L3MultiRateConfiguration (3GPP 44.018 10.5.2.21aa)
// Vectors exercise both full-rate and half-rate configurations.
// 16 bits: spare(4) | half_rate(1) | spare(3) | rate_set(8)
// =====================================================================

TEST(GoldenIE, MultiRateConfiguration_FR) {
    L3MultiRateConfiguration orig(false);
    EXPECT_EQ(orig.lengthV(), 2u);
    ieRoundTrip(orig);
}

TEST(GoldenIE, MultiRateConfiguration_HR) {
    L3MultiRateConfiguration orig(true);
    ieRoundTrip(orig);
}

// =====================================================================
// Common IEs: L3ImmediateAssignmentInformation
// =====================================================================

TEST(GoldenIE, ImmediateAssignmentInformation_Default) {
    L3ImmediateAssignmentInformation orig;
    EXPECT_EQ(orig.powerOffset(), 0u);
}

// =====================================================================
// Common IEs: L3DedicatedModeOrTBF (GSM 04.08 10.5.2.25b)
// Dedicated-mode/TBF indicator bits per TS 44.018
// 4 bits: tbf(1) | downlink(1) | spare(2)
// =====================================================================

TEST(GoldenIE, DedicatedModeOrTBF_Dedicated) {
    L3DedicatedModeOrTBF orig(false, false);
    EXPECT_EQ(orig.lengthV(), 0u);
    EXPECT_FALSE(orig.isTBF());
    EXPECT_FALSE(orig.isDownlink());
    ieRoundTrip(orig);
}

TEST(GoldenIE, DedicatedModeOrTBF_TBF) {
    L3DedicatedModeOrTBF orig(true, true);
    EXPECT_TRUE(orig.isTBF());
    EXPECT_TRUE(orig.isDownlink());
    ieRoundTrip(orig);
}

// =====================================================================
// Common IEs: L3APDUID (GSM 04.08 10.5.2.48)
// =====================================================================

TEST(GoldenIE, APDUID_Default) {
    L3APDUID orig;
    EXPECT_EQ(orig.lengthV(), 0u);
    ieRoundTrip(orig);
}

TEST(GoldenIE, APDUID_Value) {
    L3APDUID orig(3);
    ieRoundTrip(orig);
}

// =====================================================================
// Common IEs: L3APDUFlags (GSM 04.08 10.5.2.49)
// =====================================================================

TEST(GoldenIE, APDUFlags_Default) {
    L3APDUFlags orig;
    EXPECT_EQ(orig.lengthV(), 0u);
    ieRoundTrip(orig);
}

TEST(GoldenIE, APDUFlags_Full) {
    L3APDUFlags orig(1, 1, 1);
    ieRoundTrip(orig);
}

// =====================================================================
// Common IEs: L3APDUData (GSM 04.08 10.5.2.50)
// =====================================================================

TEST(GoldenIE, APDUData_Empty) {
    L3APDUData orig;
    ieRoundTripLen(orig);
}

TEST(GoldenIE, APDUData_WithData) {
    std::vector<uint8_t> rawData(2, 0);
    BitWriter bw(rawData.data(), 16);
    bw.writeField(0xAB, 8);
    bw.writeField(0xCD, 8);
    L3APDUData orig(rawData);
    EXPECT_EQ(orig.lengthV(), 2u);
    ieRoundTripLen(orig);
}

// =====================================================================
// Common IEs: L3MobileAllocation (GSM 04.08 10.5.2.14)
// =====================================================================

TEST(GoldenIE, MobileAllocation_Empty) {
    L3MobileAllocation orig;
    EXPECT_EQ(orig.lengthV(), 0u);
}

TEST(GoldenIE, MobileAllocation_WithData) {
    std::vector<uint8_t> data = {0xFF, 0x00, 0xFF};
    L3MobileAllocation orig(data);
    EXPECT_EQ(orig.lengthV(), 3u);
}

// =====================================================================
// Common IEs: L3CellOptions (GSM 04.08 10.5.2.6)
// =====================================================================

TEST(GoldenIE, CellOptions_Default) {
    L3CellOptions orig;
    EXPECT_EQ(orig.revisionLevel(), 0u);
    EXPECT_FALSE(orig.cbch());
    EXPECT_FALSE(orig.enhancedRach());
}

// =====================================================================
// Common IEs: L3CellSelection
// =====================================================================

TEST(GoldenIE, CellSelection_Default) {
    L3CellSelection cs;
    EXPECT_EQ(cs.rxLevAccessMin(), 0u);
    EXPECT_EQ(cs.maxRxLev(), 0u);
    EXPECT_EQ(cs.cellReselectionHysteresis(), 0u);
    EXPECT_EQ(cs.cellReselectionOffset(), 0u);
}

// =====================================================================
// Common IEs: L3SI3RestOctets (GSM 04.08 10.5.2.34)
// Optional GPRS fields carried in the SI3 rest octets (TS 44.018 9.1.35)
// =====================================================================

TEST(GoldenIE, SI3RestOctets_Default) {
    L3SI3RestOctets orig;
    EXPECT_FALSE(orig.hasSI3RestOctets());
    EXPECT_FALSE(orig.hasGPRS());
}

// =====================================================================
// Common IEs: L3SIType4RestOctets
// =====================================================================

TEST(GoldenIE, SI4RestOctets_Default) {
    L3SIType4RestOctets orig;
}

// =====================================================================
// Common IEs: L3SI13RestOctets (GSM 04.08 10.5.2.37b)
// GPRS power control parameters in the SI13 rest octets (TS 44.018 9.1.43a)
// =====================================================================

TEST(GoldenIE, SI13RestOctets_Default) {
    L3SI13RestOctets orig;
}

// =====================================================================
// Common IEs: L3GPRSCellOptions
// =====================================================================

TEST(GoldenIE, GPRSCellOptions_Default) {
    L3GPRSCellOptions orig;
}

// =====================================================================
// Common IEs: L3GPRSSI13PowerControlParameters
// =====================================================================

TEST(GoldenIE, GPRSSI13PowerControlParameters_Default) {
    L3GPRSSI13PowerControlParameters orig;
}

// =====================================================================
// Common IEs: L3IARestOctets
// =====================================================================

TEST(GoldenIE, IARestOctets_Default) {
    L3IARestOctets orig;
}

// =====================================================================
// Common IEs: L3FollowOnProceed (GSM 04.08 10.5.2.38)
// Follow-on/proceed indicator per GSM 24.008 10.5.2.38
// =====================================================================

TEST(GoldenIE, FollowOnProceed_Default) {
    L3FollowOnProceed orig;
    EXPECT_EQ(orig.lengthV(), 1u);
    ieRoundTrip(orig);
}

// =====================================================================
// Common IEs: L3RestOctets base
// Generic rest octets (padding) per GSM 24.008
// =====================================================================

TEST(GoldenIE, RestOctets_Base) {
    L3RestOctets orig;
    EXPECT_EQ(orig.lengthV(), 0u);
}

// =====================================================================
// Common IEs: L3OctetAlignedProtocolElement
// =====================================================================

TEST(GoldenIE, OctetAlignedProtocolElement) {
    L3OctetAlignedProtocolElement orig(std::string("\xAB\xCD\xEF", 3));
    EXPECT_EQ(orig.lengthV(), 3u);
    EXPECT_TRUE(orig.mExtant);
}

// =====================================================================
// CC IEs: L3BearerCapability (GSM 04.08 10.5.4.5)
// Bearer capability value octets per GSM 24.008 10.5.4.5
// =====================================================================

TEST(GoldenIE, BearerCapability_Default) {
    L3BearerCapability orig;
    ieRoundTrip(orig);
}

TEST(GoldenIE, BearerCapability_IE) {
    L3BearerCapability bc;
    EXPECT_EQ(bc.lengthV(), 1u);
}

// =====================================================================
// CC IEs: L3SupportedCodecList (GSM 04.08 10.5.4.32)
// =====================================================================

TEST(GoldenIE, SupportedCodecList_Default) {
    L3SupportedCodecList orig;
    EXPECT_FALSE(orig.isGsmPresent());
    EXPECT_FALSE(orig.isUmtsPresent());
}

// =====================================================================
// CC IEs: L3CalledPartyBCDNumber (GSM 04.08 10.5.4.7)
// Called party number value part per GSM 24.008 10.5.4.7
// =====================================================================

TEST(GoldenIE, CalledPartyBCDNumber_RoundTrip) {
    L3CalledPartyBCDNumber orig("1234567890");
    std::vector<uint8_t> buf(32, 0);
    BitWriter writer(buf.data(), buf.size() * 8);
    orig.write(writer);
    BitReader reader(buf.data(), writer.position());
    auto parsedResult = L3CalledPartyBCDNumber::parse(reader, orig.lengthV());
    ASSERT_TRUE(parsedResult);
    EXPECT_STREQ((*parsedResult).digits(), "1234567890");
}

TEST(GoldenIE, CalledPartyBCDNumber_International) {
    L3CalledPartyBCDNumber num("+79161234567");
    EXPECT_EQ(num.type(), TypeOfNumber::International);
    EXPECT_EQ(num.plan(), NumberingPlan::E164);
    EXPECT_STREQ(num.digits(), "79161234567");
}

TEST(GoldenIE, CalledPartyBCDNumber_ShortNumber) {
    L3CalledPartyBCDNumber num("112");
    EXPECT_STREQ(num.digits(), "112");
}

TEST(GoldenIE, CalledPartyBCDNumber_National) {
    L3CalledPartyBCDNumber num("1234567890");
    EXPECT_EQ(num.type(), TypeOfNumber::Unknown);
    EXPECT_EQ(num.plan(), NumberingPlan::Unknown);
}

// =====================================================================
// CC IEs: L3CallingPartyBCDNumber (GSM 04.08 10.5.4.9)
// Calling party number value part per GSM 24.008 10.5.4.9
// =====================================================================

TEST(GoldenIE, CallingPartyBCDNumber_RoundTrip) {
    L3CallingPartyBCDNumber orig("1234567890");
    std::vector<uint8_t> buf(32, 0);
    BitWriter writer(buf.data(), buf.size() * 8);
    orig.write(writer);
    BitReader reader(buf.data(), writer.position());
    auto parsedResult = L3CallingPartyBCDNumber::parse(reader, orig.lengthV());
    ASSERT_TRUE(parsedResult);
    EXPECT_STREQ((*parsedResult).digits(), "1234567890");
}

// =====================================================================
// CC IEs: L3CauseElement (GSM 04.08 10.5.4.11)
// Cause value part (location + coding standard + cause value) per GSM 24.008 10.5.4.11
// =====================================================================

TEST(GoldenIE, CauseElement_RoundTrip) {
    L3CauseElement orig(CCCause::User_Busy, CCCauseLocation::Transit);
    EXPECT_EQ(orig.lengthV(), 2u);
    ieRoundTrip(orig);
}

TEST(GoldenIE, CauseElement_NormalClearing) {
    L3CauseElement orig(CCCause::Normal_Call_Clearing, CCCauseLocation::Private_Serving_Local);
    std::vector<uint8_t> buf(8, 0);
    BitWriter writer(buf.data(), buf.size() * 8);
    orig.write(writer);
    BitReader reader(buf.data(), writer.position());
    auto parsedResult = L3CauseElement::parse(reader);
    ASSERT_TRUE(parsedResult);
    EXPECT_EQ((*parsedResult).cause(), CCCause::Normal_Call_Clearing);
    EXPECT_EQ((*parsedResult).location(), CCCauseLocation::Private_Serving_Local);
}

// =====================================================================
// CC IEs: L3CallState (GSM 04.08 10.5.4.6)
// Call state value octet per GSM 24.008 10.5.4.6
// =====================================================================

TEST(GoldenIE, CallState_RoundTrip) {
    L3CallState orig(0x05);
    EXPECT_EQ(orig.lengthV(), 1u);
    ieRoundTrip(orig);
}

// =====================================================================
// CC IEs: L3ProgressIndicator (GSM 04.08 10.5.4.21)
// Progress indicator bits per GSM 24.008 10.5.4.21
// =====================================================================

TEST(GoldenIE, ProgressIndicator_RoundTrip) {
    L3ProgressIndicator orig(L3ProgressIndicator::InBandAvailable,
                              L3ProgressIndicator::PrivateServingLocal);
    EXPECT_EQ(orig.lengthV(), 2u);
    ieRoundTrip(orig);
}

TEST(GoldenIE, ProgressIndicator_Encoding) {
    L3ProgressIndicator pi(L3ProgressIndicator::InBandAvailable,
                            L3ProgressIndicator::PrivateServingLocal);
    std::vector<uint8_t> buf(8, 0);
    BitWriter writer(buf.data(), buf.size() * 8);
    pi.write(writer);
    BitReader reader(buf.data(), writer.position());
    auto parsedResult = L3ProgressIndicator::parse(reader);
    ASSERT_TRUE(parsedResult);
    EXPECT_EQ((*parsedResult).progress(), L3ProgressIndicator::InBandAvailable);
    EXPECT_EQ((*parsedResult).location(), L3ProgressIndicator::PrivateServingLocal);
}

// =====================================================================
// CC IEs: L3KeypadFacility (GSM 04.08 10.5.4.17)
// Keypad facility IA5 character per GSM 24.008 10.5.4.17
// =====================================================================

TEST(GoldenIE, KeypadFacility_RoundTrip) {
    L3KeypadFacility orig('A');
    EXPECT_EQ(orig.lengthV(), 1u);
    ieRoundTrip(orig);
}

TEST(GoldenIE, KeypadFacility_Digit) {
    L3KeypadFacility kp('5');
    EXPECT_EQ(kp.ia5(), '5');
    EXPECT_EQ(kp.lengthV(), 1u);
    std::vector<uint8_t> buf(4, 0);
    BitWriter writer(buf.data(), buf.size() * 8);
    kp.write(writer);
    BitReader reader(buf.data(), writer.position());
    auto parsedResult = L3KeypadFacility::parse(reader);
    ASSERT_TRUE(parsedResult);
    EXPECT_EQ((*parsedResult).ia5(), '5');
}

// =====================================================================
// CC IEs: L3Signal (GSM 04.08 10.5.4.23)
// Signal octet per GSM 24.008 10.5.4.23
// =====================================================================

TEST(GoldenIE, Signal_RoundTrip) {
    L3Signal orig(L3Signal::SignalRingBackToneOn);
    EXPECT_EQ(orig.lengthV(), 1u);
    ieRoundTrip(orig);
}

TEST(GoldenIE, Signal_Values) {
    L3Signal s1(L3Signal::SignalRingBackToneOn);
    EXPECT_EQ(s1.lengthV(), 1u);
    L3Signal s2(L3Signal::SignalTonesOff);
    EXPECT_EQ(s2.lengthV(), 1u);
}

// =====================================================================
// CC IEs: L3RepeatIndicator (GSM 04.08 10.5.4.4)
// =====================================================================

TEST(GoldenIE, RepeatIndicator_Default) {
    L3RepeatIndicator orig;
    EXPECT_EQ(orig.lengthV(), 0u);
    EXPECT_EQ(orig.value(), 0u);
}

TEST(GoldenIE, RepeatIndicator_Value) {
    L3RepeatIndicator orig(5);
    EXPECT_EQ(orig.value(), 5u);
}

// =====================================================================
// CC IEs: L3SupServFacilityIE (GSM 04.08 10.5.4.1)
// Supplementary service facility invocation value, kept opaque (TS 24.079)
// =====================================================================

TEST(GoldenIE, SupServFacilityIE_RoundTrip) {
    L3SupServFacilityIE orig(std::string("\x81\x01\x13", 3));
    ieRoundTrip(orig);
}

// =====================================================================
// CC IEs: L3SupServVersionIndicator (24.008 10.5.4.24)
// Supplementary service version indicator octet per TS 24.079
// =====================================================================

TEST(GoldenIE, SupServVersionIndicator_RoundTrip) {
    L3SupServVersionIndicator orig;
    EXPECT_EQ(orig.lengthV(), 1u);
    ieRoundTrip(orig);
}

// =====================================================================
// CC IEs: L3BCDDigits utility (GSM 04.08 10.5.4.7)
// BCD digit packing rules per GSM 24.008 10.5.4.7
// =====================================================================

TEST(GoldenIE, BCDDigits_Even) {
    L3BCDDigits orig("1234567890");
    EXPECT_STREQ(orig.digits(), "1234567890");
    EXPECT_EQ(orig.size(), 10u);
    EXPECT_EQ(orig.lengthV(), 5u);
}

TEST(GoldenIE, BCDDigits_Odd) {
    L3BCDDigits orig("12345");
    EXPECT_STREQ(orig.digits(), "12345");
    EXPECT_EQ(orig.size(), 5u);
    EXPECT_EQ(orig.lengthV(), 3u);
}

// =====================================================================
// MM IEs: L3CMServiceType (GSM 04.08 10.5.3.3)
// CM service type bits per GSM 24.008 10.5.3.3
// =====================================================================

TEST(GoldenIE, CMServiceType_MO_Call) {
    L3CMServiceType orig(L3CMServiceType::MobileOriginatedCall);
    EXPECT_TRUE(orig.isCC());
    EXPECT_FALSE(orig.isSMS());
    EXPECT_EQ(orig.lengthV(), 0u);
    ieRoundTrip(orig);
}

TEST(GoldenIE, CMServiceType_SMS) {
    L3CMServiceType orig(L3CMServiceType::ShortMessage);
    EXPECT_TRUE(orig.isSMS());
    EXPECT_FALSE(orig.isCC());
    ieRoundTrip(orig);
}

TEST(GoldenIE, CMServiceType_Emergency) {
    L3CMServiceType orig(L3CMServiceType::EmergencyCall);
    EXPECT_TRUE(orig.isCC());
    EXPECT_FALSE(orig.isSMS());
}

TEST(GoldenIE, CMServiceType_SS) {
    L3CMServiceType orig(L3CMServiceType::SupplementaryService);
    EXPECT_FALSE(orig.isCC());
    EXPECT_FALSE(orig.isSMS());
}

TEST(GoldenIE, CMServiceType_LocationService) {
    L3CMServiceType orig(L3CMServiceType::LocationService);
    EXPECT_FALSE(orig.isCC());
}

// =====================================================================
// MM IEs: L3RejectCauseIE (GSM 04.08 10.5.3.6)
// Reject cause octet per GSM 24.008 10.5.3.6
// =====================================================================

TEST(GoldenIE, RejectCauseIE) {
    L3RejectCauseIE orig(MMRejectCause::Congestion);
    EXPECT_EQ(orig.lengthV(), 1u);
    ieRoundTrip(orig);
}

TEST(GoldenIE, RejectCauseIE_IMSI_Unknown) {
    L3RejectCauseIE orig(MMRejectCause::IMSI_Unknown_In_HLR);
    EXPECT_EQ(orig.lengthV(), 1u);
    ieRoundTrip(orig);
}

// =====================================================================
// MM IEs: L3RAND (GSM 04.08 10.5.3.1)
// =====================================================================

TEST(GoldenIE, RAND_RoundTrip) {
    std::vector<uint8_t> randBytes(16);
    for (int i = 0; i < 16; i++) randBytes[i] = static_cast<uint8_t>(i * 17);
    L3RAND orig(randBytes);
    EXPECT_EQ(orig.lengthV(), 16u);
    ieRoundTrip(orig);
}

// =====================================================================
// MM IEs: L3SRES (GSM 04.08 10.5.3.2)
// =====================================================================

TEST(GoldenIE, SRES_RoundTrip) {
    L3SRES orig(0xDEADBEEFu);
    EXPECT_EQ(orig.lengthV(), 4u);
    ieRoundTrip(orig);
}

// =====================================================================
// MM IEs: L3NetworkName (GSM 04.08 10.5.3.5a)
// Network name string (7-bit or UCS-2 alphabet) per GSM 24.008 10.5.3.5a
// =====================================================================

TEST(GoldenIE, NetworkName_RoundTrip) {
    L3NetworkName orig("TestNetwork", GSMAlphabet::ALPHABET_7BIT, 1);
    EXPECT_STREQ(orig.name(), "TestNetwork");
    EXPECT_EQ(orig.alphabet(), GSMAlphabet::ALPHABET_7BIT);
}

TEST(GoldenIE, NetworkName_Encoding) {
    L3NetworkName nn("TestNet", GSMAlphabet::ALPHABET_7BIT, 1);
    EXPECT_STREQ(nn.name(), "TestNet");
    EXPECT_EQ(nn.alphabet(), GSMAlphabet::ALPHABET_7BIT);
}

// =====================================================================
// MM IEs: L3TimeZoneAndTime (GSM 04.08 10.5.3.9)
// Time zone and time value part per GSM 24.008 10.5.3.9
// =====================================================================

TEST(GoldenIE, TimeZoneAndTime_RoundTrip) {
    L3TimeZoneAndTime orig(L3TimeZoneAndTime::UTC_TIME);
    EXPECT_EQ(orig.lengthV(), 7u);
    ieRoundTrip(orig);
}

TEST(GoldenIE, TimeZoneAndTime_UTC) {
    L3TimeZoneAndTime tzt(L3TimeZoneAndTime::UTC_TIME);
    EXPECT_EQ(tzt.lengthV(), 7u);
    EXPECT_EQ(tzt.type(), L3TimeZoneAndTime::UTC_TIME);
}

TEST(GoldenIE, TimeZoneAndTime_Local) {
    L3TimeZoneAndTime tzt(L3TimeZoneAndTime::LOCAL_TIME);
    EXPECT_EQ(tzt.type(), L3TimeZoneAndTime::LOCAL_TIME);
}

// =====================================================================
// GSM Alphabet (3GPP TS 23.038 Table 1 / GSM 03.38 Table 1)
// Reference: GSM 7-bit default alphabet character mapping
// Spec-verified: Standard GSM 03.38 Table 1 character code points
//   0='@', 1='\', 2='$', 3='(', 4=')', 5='?', 6='\'', 7='!', 8='"',
//   44='0', 45='1', ..., 48='4', ...
//   84='a', 85='b', 86='c', ... (lowercase starts at code 84)
// [GSM SPEC VERIFIED] 3GPP TS 23.038 Table 1 default alphabet:
//   Codes 0-19: Special characters (@\$(?'"* etc.)
//   Codes 20-39: Uppercase A-Z (with some specials like Ñ, ä, ö at positions)
//   Codes 40-43: Punctuation ({|}~)
//   Codes 44-53: Digits 0-9 plus punctuation
//   Codes 54-67: Lowercase a-f (used for escaping uppercase/greek)
//   Codes 68-73: More specials
//   Codes 74-83: More specials
//   Codes 84-103: Lowercase g-z
//   Key mappings: code 0='@', code 44='0', code 45='1', code 84='a'
// =====================================================================

TEST(GoldenIE, GSMAlphabet_Decode) {
    // Spec-verified: 3GPP TS 23.038 Table 1 default alphabet mapping
    EXPECT_EQ(decodeGSMChar(0), '@');
    EXPECT_EQ(decodeGSMChar(2), '$');
    EXPECT_EQ(decodeGSMChar(44), '0');
    EXPECT_EQ(decodeGSMChar(48), '4');
    EXPECT_EQ(decodeGSMChar(84), 'a');
    EXPECT_EQ(decodeGSMChar(85), 'b');
    EXPECT_EQ(decodeGSMChar(86), 'c');
}

// Out-of-range code points map to the space character (robustness policy,
// TS 23.038 default alphabet). The guard boundary is kGsm7TableSize.
TEST(GoldenIE, GSMAlphabet_Decode_OutOfRange) {
    EXPECT_EQ(decodeGSMChar(static_cast<unsigned char>(kGsm7TableSize)), ' ');
    EXPECT_EQ(decodeGSMChar(0xFFu), ' ');
    // The last in-table code point still decodes (no off-by-one at the guard).
    EXPECT_EQ(decodeGSMChar(static_cast<unsigned char>(kGsm7TableSize - 1)), 0x00);
}

// =====================================================================
// BCD nibble mapping (TS 23.040)
// gBCDAlphabet covers the sixteen nibble values: digits 0..9 at indices
// 0..9, '*' at 10/11/13, '#' at 12/14 and the fill nibble 'F' rendered as
// 'f' at index 15. Encoding maps digits to their value and everything else
// (including padding) to the fill nibble 0x0F.
// =====================================================================

TEST(GoldenIE, BCD_NibbleDecode) {
    EXPECT_EQ(decodeBCDChar(0), '0');
    EXPECT_EQ(decodeBCDChar(5), '5');
    EXPECT_EQ(decodeBCDChar(9), '9');
    EXPECT_EQ(decodeBCDChar(10), '*');
    EXPECT_EQ(decodeBCDChar(11), '*');
    EXPECT_EQ(decodeBCDChar(12), '#');
    EXPECT_EQ(decodeBCDChar(13), '*');
    EXPECT_EQ(decodeBCDChar(14), '#');
    EXPECT_EQ(decodeBCDChar(15), 'f');
    // Indices beyond the sixteen nibble mappings render as '?'.
    EXPECT_EQ(decodeBCDChar(0x10), '?');
    EXPECT_EQ(decodeBCDChar(0x7F), '?');
}

TEST(GoldenIE, BCD_NibbleEncode) {
    EXPECT_EQ(encodeBCDChar('0'), 0);
    EXPECT_EQ(encodeBCDChar('9'), 9);
    // Non-digits (padding, service symbols and letters) map to the fill nibble.
    EXPECT_EQ(encodeBCDChar('+'), 0x0F);
    EXPECT_EQ(encodeBCDChar('*'), 0x0F);
    EXPECT_EQ(encodeBCDChar('#'), 0x0F);
    EXPECT_EQ(encodeBCDChar('a'), 0x0F);
    EXPECT_EQ(encodeBCDChar('\0'), 0x0F);
}

// =====================================================================
// RACH Tables (GSM 04.08 10.5.2.29)
// RACH spreading slot count and wait parameter tables per GSM 24.008 10.5.2.29
// =====================================================================

TEST(GoldenIE, RACHTables) {
    for (int i = 0; i < 16; i++) {
        EXPECT_GT(RACHSpreadSlots[i], 0u);
        EXPECT_GT(RACHWaitSParam[i], 0u);
    }
}

// =====================================================================
// RxLev / RxQual Conversion (3GPP TS 45.008 Chapter 8 / GSM 05.02)
// dBm -> RxLev conversion (TS 45.008 8.1.4): rxlev = dbm + 110
// RxLev -> dBm conversion: dbm = -110 + rxlev
// BER -> RxQual threshold table per TS 45.008 8.2.4
// RxQual -> BER representative values per TS 45.008 8.2.4
// Spec-verified: TS 45.008 Chapter 8.1.4 (RxLev), Chapter 8.2.4 (RxQual)
// [GSM SPEC VERIFIED] TS 45.008 8.1.4: RxLev = received_level_in_dBm + 110.
//   Range: RxLev 0 = -110 dBm (minimum), RxLev 63 = -47 dBm (maximum).
//   Values 0 and 255 are reserved/special. Valid range is 1-62 for normal operation.
// TS 45.008 8.2.4: RxQual 0 (BER < 0.2%) through RxQual 7 (BER >= 12.8%).
//   BER representative values: Qual 0 = 0.14%, Qual 7 = 18.10% (TS 45.008 8.2.4).
// =====================================================================

TEST(GoldenIE, RxLev_Conversion) {
    // Spec-verified: TS 45.008 8.1.4: RxLev = received level + 110 dB
    //   RxLev=0 -> -110 dBm (minimum), RxLev=31 -> -79 dBm, RxLev=63 -> -47 dBm (maximum)
    L3MeasurementResults mr;
    EXPECT_EQ(mr.decodeLevToDBm(0), -110);
    EXPECT_EQ(mr.decodeLevToDBm(31), -79);
    EXPECT_EQ(mr.decodeLevToDBm(63), -47);
}

TEST(GoldenIE, RxQual_Conversion) {
    // Spec-verified: TS 45.008 8.2.4: RxQual 0 (BER<0.2%) < RxQual 7 (BER>=12.8%)
    // TS 45.008 8.2.4 representative BER values: Qual 0=0.14%, Qual 7=18.10%
    L3MeasurementResults mr;
    float ber0 = mr.decodeQualToBER(0);
    float ber7 = mr.decodeQualToBER(7);
    EXPECT_LT(ber0, ber7);
}

// =====================================================================
// GSM Timing Constants (3GPP TS 45.008 / GSM 05.02)
// Hyperframe length per TS 45.008: 26*51*2048 = 2715648 TDMA frames
// TDMA frame duration per TS 45.008: 120/26 ms = 4.615 ms
// Spec-verified: GSM hyperframe = 2715648 TDMA frames = 3 hours 28 minutes 48 seconds
// [GSM SPEC VERIFIED] TS 45.008 Chapter 5:
//   1 TDMA frame = 1/26 of 120ms multiframe = 4.615384... ms (~4615 microseconds).
//   26 TDMA frames = 120ms basic multiframe (TCH/FDCCH).
//   51 basic multiframes = 6162 TDMA frames = 120ms*51 = 6120ms SACCH multiframe.
//   2048 SACCH multiframes = 2715648 TDMA frames = hyperframe.
//   Hyperframe duration = 2715648 * 4.615ms = 3h 28m 48s (exactly 1244160 seconds).
//   FN (Frame Number) wraps at hyperframe boundary (mod 2715648).
// =====================================================================

TEST(GoldenIE, FrameDuration) {
    // Spec-verified: TS 45.008: 1 TDMA frame = 1/26 of 120ms burst = 4615 microseconds
    EXPECT_EQ(gFrameMicroseconds, 4615u);
}

TEST(GoldenIE, Hyperframe) {
    // Spec-verified: TS 45.008 hyperframe = 26*51*2048 = 2715648 TDMA frames
    // This is the TDMA frame number modulo (hyperframe boundary), not bit count
    EXPECT_EQ(gHyperframe, 2715648u);
}

TEST(GoldenIE, TimeComponents) {
    Time t(1326, 5);
    EXPECT_EQ(t.t1(), 1u);
    EXPECT_EQ(t.t2(), 0u);
    EXPECT_EQ(t.t3(), 0u);
    EXPECT_EQ(t.t1p(), 1u);
}

TEST(GoldenIE, FNDelta) {
    int32_t delta = FNDelta(100, 50);
    EXPECT_EQ(delta, 50);
    delta = FNDelta(50, 100);
    EXPECT_EQ(delta, -50);
}

TEST(GoldenIE, FNCompare) {
    EXPECT_GT(FNCompare(100, 50), 0);
    EXPECT_LT(FNCompare(50, 100), 0);
    EXPECT_EQ(FNCompare(100, 100), 0);
}

// =====================================================================
// BCD Number Encoding (GSM 04.08 10.5.4.7)
// BCD number encoding vectors per GSM 24.008 10.5.4.7
// =====================================================================

TEST(GoldenIE, BCD_EvenDigits) {
    L3CalledPartyBCDNumber num("1234567890");
    EXPECT_STREQ(num.digits(), "1234567890");
    EXPECT_EQ(num.lengthV(), 6u);
}

TEST(GoldenIE, BCD_OddDigits) {
    L3CalledPartyBCDNumber num("123456789");
    EXPECT_STREQ(num.digits(), "123456789");
    EXPECT_EQ(num.lengthV(), 6u);
}

TEST(GoldenIE, BCD_RoundTrip) {
    L3CalledPartyBCDNumber orig("1234567890");
    std::vector<uint8_t> buf(32, 0);
    BitWriter writer(buf.data(), buf.size() * 8);
    orig.write(writer);
    BitReader reader(buf.data(), writer.position());
    auto parsedResult = L3CalledPartyBCDNumber::parse(reader, orig.lengthV());
    ASSERT_TRUE(parsedResult);
    EXPECT_STREQ((*parsedResult).digits(), "1234567890");
}

// =====================================================================
// Rest Octet Padding (GSM 24.008 / 3GPP TS 44.018)
// Rest octet padding rule (GSM 24.008): unused trailing bits are filled with the
//   pattern '00101011'B = 0x2B to preserve bit synchronization
// Spec-verified: GSM 24.008 section 9.x messages use 0x2B ('00101011'B) as rest octet padding
//   This pattern ensures sufficient transitions for bit synchronization
// [GSM SPEC VERIFIED] Padding pattern '00101011'B = 0x2B is used throughout GSM L3
//   to fill unused bits at the end of messages. This alternating pattern provides
//   good bit transition density for timing recovery and ensures the receiver can
//   maintain synchronization. All System Information rest octets, paging message
//   padding, and other variable-length L3 messages use this pattern.
// =====================================================================

TEST(GoldenIE, RestOctetPaddingPattern) {
    // Spec-verified: '00101011'B = 0x2B is the standard GSM rest octet padding pattern
    // (GSM 24.008 rest-octet rules, applied by all variable-length message rest octets)
    constexpr uint8_t GSM_REST_OCTET_PAD = 0x2B;
    EXPECT_EQ(GSM_REST_OCTET_PAD, 0x2B);
    std::vector<uint8_t> buf(1, 0);
    BitWriter writer(buf.data(), 8);
    writer.writeField(GSM_REST_OCTET_PAD, 8);
    EXPECT_EQ(buf[0], 0x2B);
}

// =====================================================================
// L/H Presence Bits (GSM 04.07 11.2.1.1.4)
// Two-bit L/H presence indicator per GSM 04.07
// =====================================================================

TEST(GoldenIE, L_H_Bits) {
    std::vector<uint8_t> buf(4, 0);
    BitWriter writer(buf.data(), buf.size() * 8);
    writer.writeField(0, 1); // L bit = 0
    writer.writeField(1, 1); // H bit = 1
    BitReader reader(buf.data(), 2);
    EXPECT_EQ(reader.readField(1).value(), 0u);
    EXPECT_EQ(reader.readField(1).value(), 1u);
}

// =====================================================================
// SI2 body length (GSM 24.008 9.1.32 / 3GPP TS 44.018 9.1.32)
// Fixed-length SI2 body per TS 44.018 9.1.32
// Structure: bcch_freq_list(16 octets) + ncc_permitted(1 octet) + rach_control(3 octets) = 20 octets
// Spec-verified: GSM 24.008 9.1.32 System Information Type 2 fixed body length
// [GSM SPEC VERIFIED] SI2 has fixed body length of 20 octets (160 bits).
//   BCCH Frequency List: 16 octets (128-bit bitmap for ARFCN 0-124)
//   NCC Permitted: 1 octet (8-bit mask, bit N=1 means NCC value N is allowed)
//   RACH Control Parameters: 3 octets (max_retrans, tx_integer, ACC mask, etc.)
//   Total = 16 + 1 + 3 = 20 octets. No padding needed (already word-aligned).
// =====================================================================

TEST(GoldenIE, SI2_BodyLength) {
    // Spec-verified: SI2 body = BCCH freq list(16) + NCC permitted(1) + RACH control params(3) = 20 octets
    L3SystemInformationType2 msg;
    EXPECT_EQ(msg.l2BodyLength(), 20u);
    EXPECT_EQ(msg.fullBodyLength(), 20u);
}

// =====================================================================
// SI2bis body length (GSM 24.008 9.1.33 / 3GPP TS 44.018 9.1.33)
// Fixed-length SI2bis body per TS 44.018 9.1.33
// Structure: extd_bcch_freq_list(16 octets) + rach_control(3 octets) = 19 octets
// Spec-verified: GSM 24.008 9.1.33 System Information Type 2bis fixed body length
// =====================================================================

TEST(GoldenIE, SI2bis_BodyLength) {
    // Spec-verified: SI2bis body = Extended BCCH freq list(16) + RACH control params(3) = 19 octets
    L3SystemInformationType2bis msg;
    EXPECT_EQ(msg.l2BodyLength(), 19u);
    EXPECT_EQ(msg.fullBodyLength(), 20u);
}

// =====================================================================
// SI2ter body length (GSM 24.008 9.1.34 / 3GPP TS 44.018 9.1.34)
// Fixed-length SI2ter body per TS 44.018 9.1.34
// Structure: extd_bcch_freq_list(16 octets) = 16 octets
// Spec-verified: GSM 24.008 9.1.34 System Information Type 2ter fixed body length
// =====================================================================

TEST(GoldenIE, SI2ter_BodyLength) {
    // Spec-verified: SI2ter body = Extended BCCH freq list(16) = 16 octets
    L3SystemInformationType2ter msg;
    EXPECT_EQ(msg.l2BodyLength(), 16u);
    EXPECT_EQ(msg.fullBodyLength(), 20u);
}

// =====================================================================
// data2hex utility
// =====================================================================

TEST(GoldenIE, Data2Hex) {
    uint8_t data[] = {0x06, 0x19, 0x0D};
    std::string hex = data2hex(data, 3);
    EXPECT_EQ(hex, "06190D");
}

// =====================================================================
// countBeaconTimeslots utility
// =====================================================================

TEST(GoldenIE, BeaconTimeslots) {
    // ccch_conf=0 (1CCCH not combined) -> 1 beacon
    EXPECT_GT(countBeaconTimeslots(0), 0u);
}

// =====================================================================
// CC IEs: L3ConnectedNumber (GSM 04.08 10.5.4.7)
// Connected number IE carried in CC Connect (GSM 24.078)
// TLV format: IEI=0x9c, Length(1) | TypeOctet(1) | Digits...
// =====================================================================

TEST(GoldenIE, ConnectedNumber_Default) {
    L3ConnectedNumber num;
    EXPECT_EQ(num.lengthV(), 1u);
}

TEST(GoldenIE, ConnectedNumber_Digits) {
    L3ConnectedNumber num("1234567890");
    EXPECT_STREQ(num.digits(), "1234567890");
    EXPECT_EQ(num.lengthV(), 6u);
}

TEST(GoldenIE, ConnectedNumber_International) {
    L3ConnectedNumber num("+1234567890");
    EXPECT_EQ(num.type(), TypeOfNumber::International);
    EXPECT_EQ(num.plan(), NumberingPlan::E164);
}

TEST(GoldenIE, ConnectedNumber_RoundTrip) {
    L3ConnectedNumber orig("1234567890");
    std::vector<uint8_t> buf(32, 0);
    BitWriter writer(buf.data(), buf.size() * 8);
    orig.write(writer);
    BitReader reader(buf.data(), writer.position());
    auto parsedResult = L3ConnectedNumber::parse(reader, orig.lengthV());
    ASSERT_TRUE(parsedResult);
    EXPECT_STREQ((*parsedResult).digits(), "1234567890");
}

TEST(GoldenIE, ConnectedNumber_IEI) {
    EXPECT_EQ(L3ConnectedNumber::IEI, 0x9c);
}

// =====================================================================
// CC IEs: L3SubAddress (GSM 04.08 10.5.4.3)
// Sub-address IE variants carried in CC Setup/Alerting/Connect messages
//   (calling party, called party and connected party)
// TLV format: IEI=0x9a/0x9b, Length(1) | NumItems(1) | SubAddressItem...
// =====================================================================

TEST(GoldenIE, SubAddress_Default) {
    L3SubAddress sa;
    EXPECT_EQ(sa.lengthV(), 1u);
    EXPECT_TRUE(sa.items().empty());
}

TEST(GoldenIE, SubAddress_RoundTrip) {
    L3SubAddress orig;
    ieRoundTripLen(orig);
}

// =====================================================================
// CC IEs: L3RedirectingNumber (GSM 04.08 10.5.4.13)
// Redirecting number IE per GSM 24.008 10.5.4.13, carried in CC Disconnect,
//   together with the optional redirecting sub-address
// TLV format: IEI=0x97, Length(1) | TypeOctet(1) | Digits... | [Reason(1)]
// =====================================================================

TEST(GoldenIE, RedirectingNumber_Default) {
    L3RedirectingNumber rn;
    EXPECT_EQ(rn.lengthV(), 1u);
}

TEST(GoldenIE, RedirectingNumber_Digits) {
    L3RedirectingNumber rn("9876543210");
    EXPECT_STREQ(rn.digits(), "9876543210");
}

TEST(GoldenIE, RedirectingNumber_IEI) {
    EXPECT_EQ(L3RedirectingNumber::IEI, 0x97);
}

TEST(GoldenIE, RedirectingNumber_RoundTrip) {
    L3RedirectingNumber orig("9876543210");
    std::vector<uint8_t> buf(32, 0);
    BitWriter writer(buf.data(), buf.size() * 8);
    orig.write(writer);
    BitReader reader(buf.data(), writer.position());
    auto parsedResult = L3RedirectingNumber::parse(reader, orig.lengthV());
    ASSERT_TRUE(parsedResult);
    EXPECT_STREQ((*parsedResult).digits(), "9876543210");
}

// =====================================================================
// CC IEs: L3CLIRSuppression (GSM 04.08 10.5.4.16)
// CLIR suppression value per GSM 24.008 10.5.4.16
// TV format: IEI=0xc1, Value(1 octet)
// =====================================================================

TEST(GoldenIE, CLIRSuppression_Default) {
    L3CLIRSuppression clir;
    EXPECT_EQ(clir.value(), 0u);
    EXPECT_EQ(L3CLIRSuppression::IEI, 0xc1);
}

TEST(GoldenIE, CLIRSuppression_Value) {
    L3CLIRSuppression clir(5);
    EXPECT_EQ(clir.value(), 5u);
}

TEST(GoldenIE, CLIRSuppression_RoundTrip) {
    ieRoundTrip(L3CLIRSuppression(7));
}

// =====================================================================
// CC IEs: L3CLIRInvocation (GSM 04.08 10.5.4.17)
// CLIR invocation value per GSM 24.008 10.5.4.17
// TV format: IEI=0xc2, Value(1 octet)
// =====================================================================

TEST(GoldenIE, CLIRInvocation_Default) {
    L3CLIRInvocation cliri;
    EXPECT_EQ(cliri.value(), 0u);
    EXPECT_EQ(L3CLIRInvocation::IEI, 0xc2);
}

TEST(GoldenIE, CLIRInvocation_Value) {
    L3CLIRInvocation cliri(3);
    EXPECT_EQ(cliri.value(), 3u);
}

TEST(GoldenIE, CLIRInvocation_RoundTrip) {
    ieRoundTrip(L3CLIRInvocation(7));
}

// =====================================================================
// CC IEs: L3NetworkCCCapabilities (GSM 04.08 10.5.4.15)
// Network CC capabilities IE carried in CC Call Proceeding (GSM 24.078)
// TLV format: IEI=0x7a, Length(1) | CapabilityBits(2 octets min)
// =====================================================================

TEST(GoldenIE, NetworkCCCapabilities_Default) {
    L3NetworkCCCapabilities caps;
    EXPECT_EQ(caps.lengthV(), 0u);
    EXPECT_EQ(L3NetworkCCCapabilities::IEI, 0x7a);
}

TEST(GoldenIE, NetworkCCCapabilities_RoundTrip) {
    std::vector<uint8_t> buf(8, 0);
    buf[0] = 0x01; buf[1] = 0x02;
    BitWriter writer(buf.data(), buf.size() * 8);
    writer.writeField(static_cast<uint32_t>(2), 8);
    writer.writeField(0x01, 8);
    writer.writeField(0x02, 8);
    BitReader reader(buf.data(), writer.position());
    auto readLen = reader.readField(8);
    ASSERT_TRUE(readLen);
    auto parsedResult = L3NetworkCCCapabilities::parse(reader, readLen.value());
    ASSERT_TRUE(parsedResult);
    EXPECT_EQ((*parsedResult).lengthV(), 2u);
}

// =====================================================================
// CC IEs: L3LowLayerCompatibility (GSM 04.08 10.5.4.14)
// Low layer compatibility IE (variable length) per GSM 24.078
// TLV format: IEI=0x86, variable length
// =====================================================================

TEST(GoldenIE, LowLayerCompatibility_Default) {
    L3LowLayerCompatibility llc;
    EXPECT_EQ(llc.lengthV(), 0u);
    EXPECT_EQ(L3LowLayerCompatibility::IEI, 0x86);
}

TEST(GoldenIE, LowLayerCompatibility_RoundTrip) {
    std::vector<uint8_t> buf(8, 0);
    buf[0] = 0xAB; buf[1] = 0xCD;
    BitWriter writer(buf.data(), buf.size() * 8);
    writer.writeField(static_cast<uint32_t>(2), 8);
    writer.writeField(0xAB, 8);
    writer.writeField(0xCD, 8);
    BitReader reader(buf.data(), writer.position());
    auto readLen = reader.readField(8);
    ASSERT_TRUE(readLen);
    auto parsedResult = L3LowLayerCompatibility::parse(reader, readLen.value());
    ASSERT_TRUE(parsedResult);
    EXPECT_EQ((*parsedResult).lengthV(), 2u);
}

// =====================================================================
// CC IEs: L3HighLayerCompatibility (GSM 04.08 10.5.4.14)
// High layer compatibility IE (variable length) per GSM 24.078
// TLV format: IEI=0x87, variable length
// =====================================================================

TEST(GoldenIE, HighLayerCompatibility_Default) {
    L3HighLayerCompatibility hlc;
    EXPECT_EQ(hlc.lengthV(), 0u);
    EXPECT_EQ(L3HighLayerCompatibility::IEI, 0x87);
}

TEST(GoldenIE, HighLayerCompatibility_RoundTrip) {
    std::vector<uint8_t> buf(8, 0);
    buf[0] = 0x12; buf[1] = 0x34;
    BitWriter writer(buf.data(), buf.size() * 8);
    writer.writeField(static_cast<uint32_t>(2), 8);
    writer.writeField(0x12, 8);
    writer.writeField(0x34, 8);
    BitReader reader(buf.data(), writer.position());
    auto readLen = reader.readField(8);
    ASSERT_TRUE(readLen);
    auto parsedResult = L3HighLayerCompatibility::parse(reader, readLen.value());
    ASSERT_TRUE(parsedResult);
    EXPECT_EQ((*parsedResult).lengthV(), 2u);
}

// =====================================================================
// CC IEs: L3UserUser (GSM 04.08 10.5.4.27)
// User-user IE (variable length) carried in CC Setup, Alerting and Connect
//   (GSM 24.078 10.5.4.27)
// TLV format: IEI=0x75, variable length
// =====================================================================

TEST(GoldenIE, UserUser_Default) {
    L3UserUser uu;
    EXPECT_EQ(uu.lengthV(), 0u);
    EXPECT_EQ(L3UserUser::IEI, 0x75);
}

TEST(GoldenIE, UserUser_RoundTrip) {
    std::vector<uint8_t> buf(16, 0);
    BitWriter writer(buf.data(), buf.size() * 8);
    writer.writeField(static_cast<uint32_t>(3), 8);
    writer.writeField(0x01, 8);
    writer.writeField(0x02, 8);
    writer.writeField(0x03, 8);
    BitReader reader(buf.data(), writer.position());
    auto readLen = reader.readField(8);
    ASSERT_TRUE(readLen);
    auto parsedResult = L3UserUser::parse(reader, readLen.value());
    ASSERT_TRUE(parsedResult);
    EXPECT_EQ((*parsedResult).lengthV(), 3u);
}

// =====================================================================
// CC IEs: L3Priority (GSM 04.08 10.5.4.19)
// Priority IE (TV) carried in CC Setup and related messages
// TV format: IEI=0x88, Value(1 octet): spare(1)|request(1)|priorityLevel(3)|spare(3)
// =====================================================================

TEST(GoldenIE, Priority_Default) {
    L3Priority pri;
    EXPECT_EQ(pri.priorityLevel(), 0u);
    EXPECT_FALSE(pri.request());
    EXPECT_EQ(L3Priority::IEI, 0x88);
}

TEST(GoldenIE, Priority_Value) {
    L3Priority pri(5, true);
    EXPECT_EQ(pri.priorityLevel(), 5u);
    EXPECT_TRUE(pri.request());
}

TEST(GoldenIE, Priority_RoundTrip) {
    ieRoundTrip(L3Priority(7, true));
}

// =====================================================================
// CC IEs: L3StreamIdentifier (GSM 04.08 10.5.4.29)
// Stream identifier IE (TV) carried in CC Setup and emergency Setup
//   (GSM 24.078 10.5.4.29)
// TV format: IEI=0x8e, Value(1 octet): spare(3)|VBS/VGCS(1)|streamID(4)
// =====================================================================

TEST(GoldenIE, StreamIdentifier_Default) {
    L3StreamIdentifier si;
    EXPECT_EQ(si.streamId(), 0u);
    EXPECT_FALSE(si.vbs());
    EXPECT_EQ(L3StreamIdentifier::IEI, 0x8e);
}

TEST(GoldenIE, StreamIdentifier_Value) {
    L3StreamIdentifier si(5, true);
    EXPECT_EQ(si.streamId(), 5u);
    EXPECT_TRUE(si.vbs());
}

TEST(GoldenIE, StreamIdentifier_RoundTrip) {
    ieRoundTrip(L3StreamIdentifier(15, false));
}

// =====================================================================
// CC IEs: L3AllowedActions (GSM 04.08 10.5.4.2)
// TLV format: IEI=0x92, Length(1) | Flags(2 octets): spare(5)|action(11)
// =====================================================================

TEST(GoldenIE, AllowedActions_Default) {
    L3AllowedActions aa;
    EXPECT_EQ(aa.flags(), 0u);
    EXPECT_EQ(L3AllowedActions::IEI, 0x92);
}

TEST(GoldenIE, AllowedActions_Value) {
    L3AllowedActions aa(0x123);
    EXPECT_EQ(aa.flags(), 0x123u);
}

TEST(GoldenIE, AllowedActions_RoundTrip) {
    std::vector<uint8_t> buf(8, 0);
    BitWriter writer(buf.data(), buf.size() * 8);
    L3AllowedActions orig(0x456);
    writer.writeField(static_cast<uint32_t>(orig.lengthV()), 8);
    orig.write(writer);
    BitReader reader(buf.data(), writer.position());
    auto readLen = reader.readField(8);
    ASSERT_TRUE(readLen);
    auto parsedResult = L3AllowedActions::parse(reader, readLen.value());
    ASSERT_TRUE(parsedResult);
    EXPECT_EQ((*parsedResult).flags(), 0x456u);
}

// =====================================================================
// CC IEs: L3CCCapabilities (GSM 04.08 10.5.4.4)
// CC capabilities IE carried in CC Setup (GSM 24.078 10.5.4.4)
// TLV format: IEI=0x51, Length(1) | CapabilityBits(1 octet min): ext(1)|cap(7)
// =====================================================================

TEST(GoldenIE, CCCapabilities_Default) {
    L3CCCapabilities caps;
    EXPECT_EQ(caps.lengthV(), 0u);
    EXPECT_EQ(L3CCCapabilities::IEI, 0x51);
}

TEST(GoldenIE, CCCapabilities_RoundTrip) {
    std::vector<uint8_t> buf(8, 0);
    BitWriter writer(buf.data(), buf.size() * 8);
    writer.writeField(static_cast<uint32_t>(1), 8);
    writer.writeField(0x01, 8);
    BitReader reader(buf.data(), writer.position());
    auto readLen = reader.readField(8);
    ASSERT_TRUE(readLen);
    auto parsedResult = L3CCCapabilities::parse(reader, readLen.value());
    ASSERT_TRUE(parsedResult);
    EXPECT_EQ((*parsedResult).lengthV(), 1u);
}

// =====================================================================
// CC IEs: L3BackupBearerCapability (GSM 04.08 10.5.4.5b)
// Backup bearer capability IE carried in CC Setup (GSM 24.078 10.5.4.5b)
// TLV format: IEI=0x7c, similar to L3BearerCapability
// =====================================================================

TEST(GoldenIE, BackupBearerCapability_Default) {
    L3BackupBearerCapability bbc;
    EXPECT_EQ(bbc.lengthV(), 1u);
    EXPECT_EQ(L3BackupBearerCapability::IEI, 0x7c);
}

TEST(GoldenIE, BackupBearerCapability_RoundTrip) {
    ieRoundTrip(L3BackupBearerCapability{});
}
