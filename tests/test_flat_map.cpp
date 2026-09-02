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

#include <chrono>
#include <cstdio>
#include <memory>

#include <gtest/gtest.h>
#include "gsml3parser/stack/flat_map.h"

using namespace gsml3parser;

// Test: basic insert/find/erase lifecycle.
TEST(FlatMapTest, InsertFindErase) {
    FlatMap<uint32_t, int> m;
    auto [i1, ins1] = m.emplace(10, 100);
    ASSERT_TRUE(ins1);
    auto [i2, ins2] = m.emplace(20, 200);
    ASSERT_TRUE(ins2);
    EXPECT_EQ(m.size(), 2u);
    EXPECT_EQ(m.find(10), i1);
    EXPECT_EQ(m.at(i1), 100);
    EXPECT_EQ(m.keyAt(i1), 10u);
    EXPECT_EQ(m.find(30), (FlatMap<uint32_t, int>::npos));
    EXPECT_TRUE(m.erase(i2));
    EXPECT_FALSE(m.erase(i2)); // already gone
    EXPECT_EQ(m.size(), 1u);
    EXPECT_EQ(m.find(20), (FlatMap<uint32_t, int>::npos));
    EXPECT_EQ(m.find(10), i1); // the remaining entry survives the erase
}

// Test: erasing a non-last entry swaps the last one into its place;
// probes through the tombstone still find later entries.
TEST(FlatMapTest, EraseNonLast_SwapAndProbe) {
    FlatMap<uint32_t, int> m;
    m.emplace(1, 10);
    m.emplace(2, 20);
    m.emplace(3, 30);
    size_t idx1 = m.find(1);
    ASSERT_NE(idx1, (FlatMap<uint32_t, int>::npos));
    EXPECT_TRUE(m.erase(idx1));
    EXPECT_EQ(m.find(1), (FlatMap<uint32_t, int>::npos));
    EXPECT_NE(m.find(2), (FlatMap<uint32_t, int>::npos));
    EXPECT_NE(m.find(3), (FlatMap<uint32_t, int>::npos));
    EXPECT_EQ(m.size(), 2u);
}

// Test: emplace on an existing key returns the existing entry (no overwrite).
TEST(FlatMapTest, EmplaceExisting_ReturnsExisting) {
    FlatMap<uint32_t, int> m;
    auto [i1, ins1] = m.emplace(5, 50);
    ASSERT_TRUE(ins1);
    auto [i2, ins2] = m.emplace(5, 999);
    ASSERT_FALSE(ins2);
    EXPECT_EQ(i1, i2);
    EXPECT_EQ(m.at(i1), 50);
    EXPECT_EQ(m.size(), 1u);
}

// Test: growth rehash keeps all entries findable.
TEST(FlatMapTest, Growth_RehashKeepsEntries) {
    FlatMap<uint32_t, uint32_t> m;
    constexpr int N = 100000;
    for (int i = 0; i < N; ++i) {
        auto [idx, ins] = m.emplace(static_cast<uint32_t>(i), static_cast<uint32_t>(i * 3 + 1));
        ASSERT_TRUE(ins);
        (void)idx;
    }
    EXPECT_EQ(m.size(), static_cast<size_t>(N));
    for (int i = 0; i < N; ++i) {
        size_t idx = m.find(static_cast<uint32_t>(i));
        ASSERT_NE(idx, (FlatMap<uint32_t, uint32_t>::npos));
        EXPECT_EQ(m.at(idx), static_cast<uint32_t>(i * 3 + 1));
    }
}

// Test: heavy delete/insert churn (tombstones) keeps the table correct.
TEST(FlatMapTest, Tombstones_CorrectAfterChurn) {
    FlatMap<uint32_t, int> m;
    constexpr int N = 10000;
    for (int i = 0; i < N; ++i) m.emplace(static_cast<uint32_t>(i), i);
    // Erase every second entry.
    for (int i = 0; i < N; i += 2) {
        size_t idx = m.find(static_cast<uint32_t>(i));
        ASSERT_NE(idx, (FlatMap<uint32_t, int>::npos));
        m.erase(idx);
    }
    // Insert new keys (odd only, so the even-keys-absent check below holds
    // and the final size is N: 5000 surviving odds + 5000 new odds).
    for (int i = N + 1; i < N * 2; i += 2) m.emplace(static_cast<uint32_t>(i), i);
    for (int i = 0; i < N * 2; ++i) {
        size_t idx = m.find(static_cast<uint32_t>(i));
        if (i % 2 == 0) {
            EXPECT_EQ(idx, (FlatMap<uint32_t, int>::npos));
        } else {
            ASSERT_NE(idx, (FlatMap<uint32_t, int>::npos));
            EXPECT_EQ(m.at(idx), i);
        }
    }
    EXPECT_EQ(m.size(), static_cast<size_t>(N));
}

// Test: values with unique_ptr survive rehash (moved, not memcpy'd) —
// the property that makes SessionEntry usable (audit SCALE).
TEST(FlatMapTest, MoveOnlyValue_SurvivesRehash) {
    FlatMap<uint32_t, std::unique_ptr<int>> m;
    for (int i = 0; i < 10000; ++i) {
        m.emplace(static_cast<uint32_t>(i), std::make_unique<int>(i));
    }
    for (int i = 0; i < 10000; ++i) {
        size_t idx = m.find(static_cast<uint32_t>(i));
        ASSERT_NE(idx, (FlatMap<uint32_t, std::unique_ptr<int>>::npos));
        ASSERT_NE(m.at(idx), nullptr);
        EXPECT_EQ(*m.at(idx), i);
    }
}

// Test: reserve sizes the table so the threshold is not crossed.
TEST(FlatMapTest, Reserve_AvoidsEarlyRehash) {
    FlatMap<uint32_t, int> m;
    m.reserve(1000);
    EXPECT_GE(m.capacity(), 1000u + 1000u / 3 + 1);
    for (int i = 0; i < 1000; ++i) m.emplace(static_cast<uint32_t>(i), i);
    EXPECT_EQ(m.size(), 1000u);
}

// Test: forEach visits exactly the occupied entries.
TEST(FlatMapTest, ForEach_VisitsAll) {
    FlatMap<uint32_t, int> m;
    for (int i = 0; i < 100; ++i) m.emplace(static_cast<uint32_t>(i), i);
    int visited = 0;
    m.forEach([&](uint32_t, int) { ++visited; });
    EXPECT_EQ(visited, 100);
}

// Test: 4M-entry scale sanity (flat table at tens-of-millions scale).
TEST(FlatMapTest, Scale_4MEntries) {
    FlatMap<uint32_t, uint32_t> m;
    m.reserve(4'000'000);
    constexpr uint32_t N = 4'000'000;
    auto t0 = std::chrono::steady_clock::now();
    for (uint32_t i = 0; i < N; ++i) m.emplace(i, i + 1);
    auto t1 = std::chrono::steady_clock::now();
    double insMs = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count() / 1000.0;
    std::printf("FlatMap 4M insert: %.1f ms\n", insMs);
    auto t2 = std::chrono::steady_clock::now();
    uint32_t ok = 0;
    for (uint32_t i = 0; i < N; ++i) {
        size_t idx = m.find(i);
        if (idx != (FlatMap<uint32_t, uint32_t>::npos) && m.at(idx) == i + 1) ++ok;
    }
    auto t3 = std::chrono::steady_clock::now();
    double findMs = std::chrono::duration_cast<std::chrono::microseconds>(t3 - t2).count() / 1000.0;
    std::printf("FlatMap 4M find: %.1f ms\n", findMs);
    EXPECT_EQ(ok, N);
#if !defined(GSML3PARSER_ASAN) && !defined(GSML3PARSER_DEBUG)
    EXPECT_LT(insMs, 10000.0) << "4M insert too slow";
    EXPECT_LT(findMs, 10000.0) << "4M find too slow";
#endif
}
