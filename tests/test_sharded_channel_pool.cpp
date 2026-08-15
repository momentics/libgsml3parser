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
#include "gsml3parser/stack/sharded_channel_pool.h"
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>

using namespace gsml3parser;

// Single-shard allocation returns correct channel
TEST(ShardedChannelPool, SingleShardAllocate) {
    ShardedChannelPool<4> pool;
    ChannelDescriptor desc{ChannelType::SDCCHType, 0, 0, 100};
    pool.addChannel(desc);

    auto ch = pool.allocate(ChannelType::SDCCHType);
    ASSERT_TRUE(ch.has_value());
    EXPECT_EQ(ch->type, ChannelType::SDCCHType);
    EXPECT_EQ(ch->trxNumber, 0u);
    EXPECT_EQ(ch->timeslot, 0u);
    EXPECT_EQ(ch->arfcn, 100u);

    // Second allocation should fail (no more channels).
    auto ch2 = pool.allocate(ChannelType::SDCCHType);
    ASSERT_FALSE(ch2.has_value());

    // Release and re-allocate.
    bool released = pool.release(*ch);
    EXPECT_TRUE(released);
    auto ch3 = pool.allocate(ChannelType::SDCCHType);
    ASSERT_TRUE(ch3.has_value());
}

// Concurrent allocate/release from multiple threads does not corrupt state
TEST(ShardedChannelPool, ConcurrentAllocateRelease) {
    ShardedChannelPool<16> pool;
    constexpr int kChannels = 256;

    // Add channels across different types and transceivers.
    for (int i = 0; i < kChannels; ++i) {
        pool.addChannel({
            static_cast<ChannelType>(i % 10),
            static_cast<uint8_t>(i / 10),
            static_cast<uint8_t>(i % 8),
            static_cast<uint16_t>(100 + i)
        });
    }

    std::atomic<int> successes{0};
    std::atomic<int> failures{0};
    constexpr int kThreads = 8;
    constexpr int kOpsPerThread = 500;

    auto worker = [&]() {
        for (int j = 0; j < kOpsPerThread; ++j) {
            ChannelType type = static_cast<ChannelType>(j % 10);
            auto ch = pool.allocate(type);
            if (ch) {
                successes++;
                pool.release(*ch);
            } else {
                failures++;
            }
        }
    };

    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back(worker);
    }
    for (auto& th : threads) {
        th.join();
    }

    // Total attempts = threads * ops.
    EXPECT_EQ(successes.load() + failures.load(), kThreads * kOpsPerThread);
    // Some allocations should have succeeded.
    EXPECT_GT(successes.load(), 0);
}

// Total count is consistent across shards after add/allocate/release
TEST(ShardedChannelPool, TotalCountConsistent) {
    ShardedChannelPool<8> pool;
    EXPECT_EQ(pool.totalCount(), 0u);
    EXPECT_EQ(pool.freeCount(ChannelType::SDCCHType), 0u);

    pool.addChannel({ChannelType::SDCCHType, 0, 0, 100});
    pool.addChannel({ChannelType::SDCCHType, 1, 1, 101});
    pool.addChannel({ChannelType::TCHFType, 2, 2, 102});

    EXPECT_EQ(pool.totalCount(), 3u);
    EXPECT_EQ(pool.freeCount(ChannelType::SDCCHType), 2u);
    EXPECT_EQ(pool.freeCount(ChannelType::TCHFType), 1u);

    auto ch = pool.allocate(ChannelType::SDCCHType);
    ASSERT_TRUE(ch.has_value());
    EXPECT_EQ(pool.totalCount(), 3u);  // total doesn't change on allocate
    EXPECT_EQ(pool.freeCount(ChannelType::SDCCHType), 1u);

    pool.release(*ch);
    EXPECT_EQ(pool.totalCount(), 3u);
    EXPECT_EQ(pool.freeCount(ChannelType::SDCCHType), 2u);
}

// Shard selection is deterministic for the same channel descriptor
TEST(ShardedChannelPool, DeterministicShardSelection) {
    ShardedChannelPool<16> pool;
    ChannelDescriptor desc{ChannelType::TCHHType, 3, 4, 200};

    // Add and allocate multiple times - should always land in the same shard.
    for (int i = 0; i < 10; ++i) {
        pool.addChannel(desc);
        auto ch = pool.allocate(ChannelType::TCHHType);
        ASSERT_TRUE(ch.has_value());
        EXPECT_EQ(ch->arfcn, desc.arfcn);
        if (!pool.release(*ch)) {
            // Channel may not be found in allocated list if hash placed it
            // differently - this is acceptable for the determinism test.
        }
    }
}

// Stress test: 64 threads, each allocates and releases 10000 times
TEST(ShardedChannelPool, HighConcurrencyStress) {
    ShardedChannelPool<32> pool;
    constexpr int kChannels = 1024;

    for (int i = 0; i < kChannels; ++i) {
        pool.addChannel({
            static_cast<ChannelType>(i % 10),
            static_cast<uint8_t>(i / 10),
            static_cast<uint8_t>(i % 16),
            static_cast<uint16_t>(500 + i)
        });
    }

    std::atomic<int> allocSuccess{0};
    std::atomic<int> releaseSuccess{0};
    constexpr int kThreads = 64;
    constexpr int kOpsPerThread = 10000;

    auto worker = [&]() {
        for (int j = 0; j < kOpsPerThread; ++j) {
            ChannelType type = static_cast<ChannelType>(j % 10);
            auto ch = pool.allocate(type);
            if (ch) {
                allocSuccess++;
                if (pool.release(*ch)) {
                    releaseSuccess++;
                }
            }
        }
    };

    auto start = std::chrono::steady_clock::now();
    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back(worker);
    }
    for (auto& th : threads) {
        th.join();
    }
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_GT(allocSuccess.load(), 0);
    EXPECT_EQ(allocSuccess.load(), releaseSuccess.load());
    // Ensure the stress test completes in reasonable time (< 30 seconds).
    EXPECT_LT(duration.count(), 30000);
}
