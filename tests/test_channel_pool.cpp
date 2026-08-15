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
#include <gsml3parser/stack/channel_pool.h>

using namespace gsml3parser;

// ── RA Decoding Tests (GSM 04.08 Table 9.9) ────────────────────────────

// MO call (establishment cause 00) with VEA returns TCH
TEST(DecodeChannelNeededTest, MO_Call_returnsTCH_withVEA) {
    // RA=0x00: establishment cause bits 6-5 = 00 (MO call)
    EXPECT_EQ(decodeChannelNeeded(0x00, false, true), ChannelType::TCHFType);
}

// MO call without VEA returns SDCCH
TEST(DecodeChannelNeededTest, MO_Call_returnsSDCCH_withoutVEA) {
    // RA=0x00: establishment cause 00 (MO call), no VEA -> SDCCH
    EXPECT_EQ(decodeChannelNeeded(0x00, false, false), ChannelType::SDCCHType);
}

// Location Updating (establishment cause 11) always returns SDCCH
TEST(DecodeChannelNeededTest, LocationUpdate_returnsSDCCH) {
    // RA=0x60: establishment cause bits 6-5 = 11 (Location Updating)
    EXPECT_EQ(decodeChannelNeeded(0x60, false, false), ChannelType::SDCCHType);
    EXPECT_EQ(decodeChannelNeeded(0x60, false, true), ChannelType::SDCCHType);
}

// Emergency call (establishment cause 01) always returns TCH
TEST(DecodeChannelNeededTest, EmergencyCall_returnsTCH) {
    // RA=0x20: establishment cause bits 6-5 = 01 (Emergency)
    EXPECT_EQ(decodeChannelNeeded(0x20, false, false), ChannelType::TCHFType);
    EXPECT_EQ(decodeChannelNeeded(0x20, false, true), ChannelType::TCHFType);
}

// Answer to Paging (establishment cause 10) returns TCH
TEST(DecodeChannelNeededTest, AnswerToPaging_returnsTCH) {
    // RA=0x40: establishment cause bits 6-5 = 10 (Answer to Paging)
    EXPECT_EQ(decodeChannelNeeded(0x40, false, false), ChannelType::TCHFType);
}

// ── Location Updating Request Detection ────────────────────────────────

// RA with establishment cause 11 is a location updating request
TEST(IsLocationUpdatingRequestTest, RA_LocationUpdate_returnsTrue) {
    // RA=0x60: establishment cause 11 = Location Updating
    EXPECT_TRUE(isLocationUpdatingRequest(0x60));
    EXPECT_TRUE(isLocationUpdatingRequest(0x6F));
}

// RA with other establishment causes is not a location updating request
TEST(IsLocationUpdatingRequestTest, RA_NonLocationUpdate_returnsFalse) {
    // RA=0x00: establishment cause 00 = MO Call
    EXPECT_FALSE(isLocationUpdatingRequest(0x00));
    // RA=0x20: establishment cause 01 = Emergency
    EXPECT_FALSE(isLocationUpdatingRequest(0x20));
    // RA=0x40: establishment cause 10 = Answer to Paging
    EXPECT_FALSE(isLocationUpdatingRequest(0x40));
}

// ── ChannelPool Basic Operations ───────────────────────────────────────

// Adding a channel and allocating it returns the descriptor
TEST(ChannelPoolTest, AddAndAllocate_returnsDescriptor) {
    ChannelPool pool;
    ChannelDescriptor desc{ChannelType::SDCCHType, 0, 0, 100};
    pool.addChannel(desc);

    auto allocated = pool.allocate(ChannelType::SDCCHType);
    ASSERT_TRUE(allocated.has_value());
    EXPECT_EQ(allocated->type, ChannelType::SDCCHType);
    EXPECT_EQ(allocated->trxNumber, 0);
    EXPECT_EQ(allocated->timeslot, 0);
    EXPECT_EQ(allocated->arfcn, 100u);
}

// Allocating from empty pool returns nullopt
TEST(ChannelPoolTest, Allocate_emptyPool_returnsNullopt) {
    ChannelPool pool;
    auto ch = pool.allocate(ChannelType::SDCCHType);
    EXPECT_FALSE(ch.has_value());
}

// Release returns channel to the free pool for re-allocation
TEST(ChannelPoolTest, Release_returnsChannelToPool) {
    ChannelPool pool;
    ChannelDescriptor desc{ChannelType::TCHFType, 1, 2, 200};
    pool.addChannel(desc);

    auto ch1 = pool.allocate(ChannelType::TCHFType);
    ASSERT_TRUE(ch1.has_value());
    EXPECT_FALSE(pool.allocate(ChannelType::TCHFType).has_value());

    EXPECT_TRUE(pool.release(*ch1));
    auto ch2 = pool.allocate(ChannelType::TCHFType);
    ASSERT_TRUE(ch2.has_value());
    EXPECT_EQ(ch2->arfcn, 200u);
}

// Allocating the same channel twice is not possible (popped from free list)
TEST(ChannelPoolTest, DoubleAllocate_sameChannelNotReturned) {
    ChannelPool pool;
    pool.addChannel({ChannelType::SDCCHType, 0, 0, 100});

    auto ch1 = pool.allocate(ChannelType::SDCCHType);
    ASSERT_TRUE(ch1.has_value());

    auto ch2 = pool.allocate(ChannelType::SDCCHType);
    EXPECT_FALSE(ch2.has_value());
}

// FreeCount reflects available channels after allocations and releases
TEST(ChannelPoolTest, FreeCount_accurate) {
    ChannelPool pool;
    pool.addChannel({ChannelType::SDCCHType, 0, 0, 100});
    pool.addChannel({ChannelType::SDCCHType, 0, 1, 101});
    pool.addChannel({ChannelType::TCHFType, 1, 0, 200});

    EXPECT_EQ(pool.freeCount(ChannelType::SDCCHType), 2u);
    EXPECT_EQ(pool.freeCount(ChannelType::TCHFType), 1u);
    EXPECT_EQ(pool.freeCount(ChannelType::TCHHType), 0u);

    (void)pool.allocate(ChannelType::SDCCHType);
    EXPECT_EQ(pool.freeCount(ChannelType::SDCCHType), 1u);
}

// TotalCount includes both free and allocated channels
TEST(ChannelPoolTest, TotalCount_includesAllocated) {
    ChannelPool pool;
    pool.addChannel({ChannelType::SDCCHType, 0, 0, 100});
    pool.addChannel({ChannelType::TCHFType, 1, 0, 200});

    EXPECT_EQ(pool.totalCount(), 2u);

    auto ch = pool.allocate(ChannelType::SDCCHType);
    EXPECT_EQ(pool.totalCount(), 2u); // still 2: 1 free + 1 allocated

    if (ch) {
        pool.release(*ch);
    }
    EXPECT_EQ(pool.totalCount(), 2u);
}

// IsFree correctly reports channel availability state
TEST(ChannelPoolTest, IsFree_correctState) {
    ChannelPool pool;
    ChannelDescriptor desc{ChannelType::SDCCHType, 0, 0, 100};
    pool.addChannel(desc);

    EXPECT_TRUE(pool.isFree(desc));

    (void)pool.allocate(ChannelType::SDCCHType);
    EXPECT_FALSE(pool.isFree(desc));

    pool.release(desc);
    EXPECT_TRUE(pool.isFree(desc));
}

// RemoveChannel permanently removes a channel from the pool
TEST(ChannelPoolTest, RemoveChannel_permanentlyGone) {
    ChannelPool pool;
    ChannelDescriptor desc{ChannelType::SDCCHType, 0, 0, 100};
    pool.addChannel(desc);

    EXPECT_TRUE(pool.removeChannel(desc));
    EXPECT_FALSE(pool.allocate(ChannelType::SDCCHType).has_value());
    EXPECT_EQ(pool.freeCount(ChannelType::SDCCHType), 0u);
    EXPECT_EQ(pool.totalCount(), 0u);
}

// RemoveChannel on unknown channel returns false
TEST(ChannelPoolTest, RemoveChannel_unknown_returnsFalse) {
    ChannelPool pool;
    pool.addChannel({ChannelType::SDCCHType, 0, 0, 100});

    EXPECT_FALSE(pool.removeChannel({ChannelType::SDCCHType, 0, 0, 999}));
}

// RemoveChannel on allocated channel removes it from tracking
TEST(ChannelPoolTest, RemoveChannel_allocated_removesFromTracking) {
    ChannelPool pool;
    ChannelDescriptor desc{ChannelType::TCHFType, 1, 2, 200};
    pool.addChannel(desc);

    auto ch = pool.allocate(ChannelType::TCHFType);
    ASSERT_TRUE(ch.has_value());
    EXPECT_EQ(pool.totalCount(), 1u);

    EXPECT_TRUE(pool.removeChannel(*ch));
    EXPECT_EQ(pool.totalCount(), 0u);
}

// ── Very Early Assignment Tests ────────────────────────────────────────

// VEA for MO call tries TCH first when available
TEST(ChannelPoolVEATest, AllocateVEA_MOC_tryTCHFirst) {
    ChannelPool pool;
    pool.addChannel({ChannelType::TCHFType, 1, 0, 200});
    pool.addChannel({ChannelType::SDCCHType, 0, 0, 100});

    // RA=0x00: MO call. VEA should allocate TCH first.
    auto ch = pool.allocateVEA(0x00);
    ASSERT_TRUE(ch.has_value());
    EXPECT_EQ(ch->type, ChannelType::TCHFType);
}

// VEA falls back to SDCCH when no TCH available
TEST(ChannelPoolVEATest, AllocateVEA_fallbackToSDCCH) {
    ChannelPool pool;
    pool.addChannel({ChannelType::SDCCHType, 0, 0, 100});

    // RA=0x00: MO call. No TCH available, should fall back to SDCCH.
    auto ch = pool.allocateVEA(0x00);
    ASSERT_TRUE(ch.has_value());
    EXPECT_EQ(ch->type, ChannelType::SDCCHType);
}

// VEA for non-MO causes uses standard decode + allocate
TEST(ChannelPoolVEATest, AllocateVEA_LocationUpdate_usesSDCCH) {
    ChannelPool pool;
    pool.addChannel({ChannelType::SDCCHType, 0, 0, 100});

    // RA=0x60: Location Updating. Should allocate SDCCH directly.
    auto ch = pool.allocateVEA(0x60);
    ASSERT_TRUE(ch.has_value());
    EXPECT_EQ(ch->type, ChannelType::SDCCHType);
}

// VEA returns nullopt when no suitable channels exist
TEST(ChannelPoolVEATest, AllocateVEA_noChannels_returnsNullopt) {
    ChannelPool pool;

    auto ch = pool.allocateVEA(0x00);
    EXPECT_FALSE(ch.has_value());
}

// ── Miscellaneous Tests ────────────────────────────────────────────────

// freeChannels returns all free channels of a given type
TEST(ChannelPoolTest, FreeChannels_returnsAllFree) {
    ChannelPool pool;
    pool.addChannel({ChannelType::SDCCHType, 0, 0, 100});
    pool.addChannel({ChannelType::SDCCHType, 0, 1, 101});
    pool.addChannel({ChannelType::TCHFType, 1, 0, 200});

    auto free = pool.freeChannels(ChannelType::SDCCHType);
    EXPECT_EQ(free.size(), 2u);

    auto freeTch = pool.freeChannels(ChannelType::TCHFType);
    EXPECT_EQ(freeTch.size(), 1u);

    auto freeEmpty = pool.freeChannels(ChannelType::TCHHType);
    EXPECT_EQ(freeEmpty.size(), 0u);
}

// allocatedCount tracks in-use channels per type
TEST(ChannelPoolTest, AllocatedCount_tracksCorrectly) {
    ChannelPool pool;
    pool.addChannel({ChannelType::SDCCHType, 0, 0, 100});
    pool.addChannel({ChannelType::SDCCHType, 0, 1, 101});

    EXPECT_EQ(pool.allocatedCount(ChannelType::SDCCHType), 0u);

    (void)pool.allocate(ChannelType::SDCCHType);
    EXPECT_EQ(pool.allocatedCount(ChannelType::SDCCHType), 1u);

    (void)pool.allocate(ChannelType::SDCCHType);
    EXPECT_EQ(pool.allocatedCount(ChannelType::SDCCHType), 2u);
}

// Release of unknown channel returns false
TEST(ChannelPoolTest, Release_unknownChannel_returnsFalse) {
    ChannelPool pool;
    pool.addChannel({ChannelType::SDCCHType, 0, 0, 100});

    EXPECT_FALSE(pool.release({ChannelType::SDCCHType, 0, 0, 999}));
}

// Multiple channel types coexist independently in the pool
TEST(ChannelPoolTest, MultipleTypes_coexistIndependently) {
    ChannelPool pool;
    pool.addChannel({ChannelType::SDCCHType, 0, 0, 100});
    pool.addChannel({ChannelType::TCHFType, 1, 0, 200});
    pool.addChannel({ChannelType::TCHHType, 1, 1, 201});

    auto sdcch = pool.allocate(ChannelType::SDCCHType);
    auto tchf = pool.allocate(ChannelType::TCHFType);
    auto tchh = pool.allocate(ChannelType::TCHHType);

    ASSERT_TRUE(sdcch.has_value());
    ASSERT_TRUE(tchf.has_value());
    ASSERT_TRUE(tchh.has_value());

    EXPECT_EQ(sdcch->type, ChannelType::SDCCHType);
    EXPECT_EQ(tchf->type, ChannelType::TCHFType);
    EXPECT_EQ(tchh->type, ChannelType::TCHHType);

    // All types should now have 0 free
    EXPECT_EQ(pool.freeCount(ChannelType::SDCCHType), 0u);
    EXPECT_EQ(pool.freeCount(ChannelType::TCHFType), 0u);
    EXPECT_EQ(pool.freeCount(ChannelType::TCHHType), 0u);
}
