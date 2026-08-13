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
#include <gsml3parser/stack/ms_context.h>

using namespace gsml3parser;

// MSContext stores TMSI and returns it via identity().tmsi()
TEST(MSContextTest, CreateWithTMSI_returnsCorrectIdentity) {
    auto ctx = MSContext::createWithTMSI(0x12345678u);
    EXPECT_TRUE(ctx.identity().isTMSI());
    EXPECT_EQ(ctx.identity().tmsi(), 0x12345678u);
}

// MSContext stores IMSI digits and returns them via identity()
TEST(MSContextTest, CreateWithIMSI_returnsCorrectIdentity) {
    auto ctx = MSContext::createWithIMSI("244051234567890");
    EXPECT_TRUE(ctx.identity().isIMSI());
    EXPECT_EQ(std::string_view(ctx.identity().digits()), "244051234567890");
}

// setClassmark stores classmark; classmark() returns it
TEST(MSContextTest, SetAndGetClassmark) {
    auto ctx = MSContext::createWithTMSI(0xABCDEF01u);
    EXPECT_FALSE(ctx.classmark().has_value());

    L3MobileStationClassmark1 cm;
    ctx.setClassmark(cm);
    EXPECT_TRUE(ctx.classmark().has_value());
}

// assignChannel sets channel type and parameters; releaseChannel resets them
TEST(MSContextTest, ChannelAssignmentAndRelease) {
    auto ctx = MSContext::createWithTMSI(0x11111111u);
    EXPECT_EQ(ctx.channelType(), ChannelType::UndefinedCHType);

    ctx.assignChannel(ChannelType::SDCCHType, 2, 5, 125);
    EXPECT_EQ(ctx.channelType(), ChannelType::SDCCHType);
    EXPECT_EQ(ctx.trxNumber(), 2);
    EXPECT_EQ(ctx.timeslot(), 5);
    EXPECT_EQ(ctx.arfcn(), 125u);

    ctx.releaseChannel();
    EXPECT_EQ(ctx.channelType(), ChannelType::UndefinedCHType);
    EXPECT_EQ(ctx.trxNumber(), 0u);
    EXPECT_EQ(ctx.timeslot(), 0u);
    EXPECT_EQ(ctx.arfcn(), 0u);
}

// setLAI stores LAI; lai() returns it; initially nullopt
TEST(MSContextTest, LAIStorage) {
    auto ctx = MSContext::createWithTMSI(0x22222222u);
    EXPECT_FALSE(ctx.lai().has_value());

    L3LocationAreaIdentity lai("262", "42", 1234);
    ctx.setLAI(lai);
    EXPECT_TRUE(ctx.lai().has_value());
}

// isCiphered / setCiphered toggle ciphering flag
TEST(MSContextTest, CipheringFlags) {
    auto ctx = MSContext::createWithTMSI(0x33333333u);
    EXPECT_FALSE(ctx.isCiphered());

    ctx.setCiphered(true);
    EXPECT_TRUE(ctx.isCiphered());

    ctx.setCiphered(false);
    EXPECT_FALSE(ctx.isCiphered());
}

// setTimingAdvance stores value; timingAdvance() returns it
TEST(MSContextTest, TimingAdvanceStorage) {
    auto ctx = MSContext::createWithTMSI(0x44444444u);
    EXPECT_FALSE(ctx.timingAdvance().has_value());

    ctx.setTimingAdvance(42);
    EXPECT_TRUE(ctx.timingAdvance().has_value());
    EXPECT_EQ(ctx.timingAdvance().value(), 42u);
}

// isRegistered, isAuthenticated start false; setters flip them
TEST(MSContextTest, RegistrationAndAuthenticationFlags) {
    auto ctx = MSContext::createWithTMSI(0x55555555u);
    EXPECT_FALSE(ctx.isRegistered());
    EXPECT_FALSE(ctx.isAuthenticated());

    ctx.setRegistered(true);
    ctx.setAuthenticated(true);
    EXPECT_TRUE(ctx.isRegistered());
    EXPECT_TRUE(ctx.isAuthenticated());

    ctx.setRegistered(false);
    ctx.setAuthenticated(false);
    EXPECT_FALSE(ctx.isRegistered());
    EXPECT_FALSE(ctx.isAuthenticated());
}

// setTMSI updates the identity to a new TMSI value
TEST(MSContextTest, TMSIUpdate_changesIdentity) {
    auto ctx = MSContext::createWithIMSI("244051234567890");
    EXPECT_TRUE(ctx.identity().isIMSI());

    ctx.setTMSI(0xDEADBEEFu);
    EXPECT_TRUE(ctx.identity().isTMSI());
    EXPECT_EQ(ctx.identity().tmsi(), 0xDEADBEEFu);
}

// Default-constructed context has empty identity and no channel
TEST(MSContextTest, DefaultConstructor_emptyContext) {
    auto ctx = MSContext::createWithTMSI(0u);
    EXPECT_TRUE(ctx.identity().isTMSI());
    EXPECT_EQ(ctx.identity().tmsi(), 0u);
    EXPECT_EQ(ctx.channelType(), ChannelType::UndefinedCHType);
    EXPECT_FALSE(ctx.isCiphered());
    EXPECT_FALSE(ctx.isRegistered());
    EXPECT_FALSE(ctx.isAuthenticated());
}

// sizeof(MSContext) must fit within 256 bytes for cache efficiency
TEST(MSContextTest, SizeFitsIn256Bytes) {
    EXPECT_LE(sizeof(MSContext), 256u);
}

// setIMSI updates identity to IMSI from existing TMSI context
TEST(MSContextTest, IMSIUpdate_changesIdentity) {
    auto ctx = MSContext::createWithTMSI(0x99999999u);
    EXPECT_TRUE(ctx.identity().isTMSI());

    ctx.setIMSI("123456789012345");
    EXPECT_TRUE(ctx.identity().isIMSI());
    EXPECT_EQ(std::string_view(ctx.identity().digits()), "123456789012345");
}

// Channel parameters persist across flag changes
TEST(MSContextTest, ChannelPersistAcrossFlagChanges) {
    auto ctx = MSContext::createWithTMSI(0x77777777u);
    ctx.assignChannel(ChannelType::TCHFType, 1, 3, 200);

    ctx.setCiphered(true);
    ctx.setRegistered(true);
    ctx.setAuthenticated(true);
    ctx.setTimingAdvance(30);

    EXPECT_EQ(ctx.channelType(), ChannelType::TCHFType);
    EXPECT_EQ(ctx.trxNumber(), 1u);
    EXPECT_EQ(ctx.timeslot(), 3u);
    EXPECT_EQ(ctx.arfcn(), 200u);
}

// Multiple TMSI reassignments update identity correctly
TEST(MSContextTest, MultipleTMSIReassignments) {
    auto ctx = MSContext::createWithTMSI(0x00000001u);
    EXPECT_EQ(ctx.identity().tmsi(), 0x00000001u);

    ctx.setTMSI(0x00000002u);
    EXPECT_EQ(ctx.identity().tmsi(), 0x00000002u);

    ctx.setTMSI(0xFFFFFFFFu);
    EXPECT_EQ(ctx.identity().tmsi(), 0xFFFFFFFFu);
}

// All fields can be set and read back consistently
TEST(MSContextTest, FullFieldRoundTrip) {
    auto ctx = MSContext::createWithIMSI("244051234567890");
    ctx.assignChannel(ChannelType::SDCCHType, 0, 0, 100);
    L3MobileStationClassmark1 cm;
    ctx.setClassmark(cm);
    L3LocationAreaIdentity lai("262", "42", 5678);
    ctx.setLAI(lai);
    ctx.setCiphered(true);
    ctx.setTimingAdvance(55);
    ctx.setRegistered(true);
    ctx.setAuthenticated(true);

    EXPECT_TRUE(ctx.identity().isIMSI());
    EXPECT_EQ(ctx.channelType(), ChannelType::SDCCHType);
    EXPECT_EQ(ctx.trxNumber(), 0u);
    EXPECT_EQ(ctx.timeslot(), 0u);
    EXPECT_EQ(ctx.arfcn(), 100u);
    EXPECT_TRUE(ctx.classmark().has_value());
    EXPECT_TRUE(ctx.lai().has_value());
    EXPECT_TRUE(ctx.isCiphered());
    EXPECT_EQ(ctx.timingAdvance().value(), 55u);
    EXPECT_TRUE(ctx.isRegistered());
    EXPECT_TRUE(ctx.isAuthenticated());
}
