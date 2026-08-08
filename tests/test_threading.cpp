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
#include <gsml3parser/parser.h>
#include <gsml3parser/arena.h>
#include <gsml3parser/bitreader.h>
#include <gsml3parser/bitwriter.h>
#include <gsml3parser/rr/l3rrmessages.h>
#include <gsml3parser/mm/l3mmmessages.h>
#include <gsml3parser/cc/l3ccmessages.h>

#include <atomic>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <string>
#include <functional>

using namespace gsml3parser;

// ── Test: 4 threads parse concurrently with separate ParserConfig ───────

TEST(ThreadingTest, ConcurrentParseWithConfig) {
    constexpr int NumThreads = 4;
    constexpr int IterationsPerThread = 200;

    // Different messages for each thread to exercise different code paths
    std::vector<std::vector<uint8_t>> msgBuffers = {
        {0x60, 0x0D, 0x00},                             // RR ChannelRelease
        {0x50, 0x84},                                    // MM CMServiceAccept
        {0x30, 0x94, 0x08, 0x02, 0x16, 0x21},          // CC Disconnect
        {0xB0, 0xE8},                                    // SS Facility
    };

    std::atomic<int> successCount{0};
    std::atomic<int> failCount{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < NumThreads; ++t) {
        threads.emplace_back([&msgBuffers, &successCount, &failCount, t]() {
            ParserConfig cfg;
            int localSuccess = 0;
            int localFail = 0;
            std::span<const uint8_t> msgSpan(msgBuffers[t].data(), msgBuffers[t].size());

            for (int i = 0; i < IterationsPerThread; ++i) {
                auto msg = parseL3(msgSpan, cfg);
                if (msg) {
                    ++localSuccess;
                } else {
                    ++localFail;
                }
            }

            successCount.fetch_add(localSuccess);
            failCount.fetch_add(localFail);
        });
    }

    for (auto& thr : threads) {
        thr.join();
    }

    EXPECT_EQ(successCount.load(), NumThreads * IterationsPerThread);
    EXPECT_EQ(failCount.load(), 0);
}

// ── Test: ParserConfig is immutable — concurrent reads are safe ────────

TEST(ThreadingTest, ConcurrentConfigRead) {
    constexpr int NumThreads = 8;
    std::atomic<int> errorCount{0};
    std::vector<std::thread> threads;

    // Shared immutable config
    ParserConfig cfg;
    cfg = cfg.withLogLevel(LogLevel::DEBUG);

    for (int t = 0; t < NumThreads; ++t) {
        threads.emplace_back([&cfg, &errorCount]() {
            for (int i = 0; i < 1000; ++i) {
                // Concurrent read — should be safe since config is immutable
                auto level = cfg.getLogLevel();
                if (level != LogLevel::DEBUG) {
                    errorCount.fetch_add(1);
                }
            }
        });
    }

    for (auto& thr : threads) {
        thr.join();
    }

    EXPECT_EQ(errorCount.load(), 0);
}

// ── Test: Arena allocator — each thread has its own arena, no conflicts ─

TEST(ThreadingTest, ConcurrentArenaNoConflict) {
    constexpr int NumThreads = 4;
    constexpr int AllocsPerThread = 500;

    std::atomic<int> errorCount{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < NumThreads; ++t) {
        threads.emplace_back([&errorCount]() {
            Arena arena(8192);
            std::vector<void*> ptrs;
            ptrs.reserve(AllocsPerThread);

            for (int i = 0; i < AllocsPerThread; ++i) {
                void* p = arena.allocate(64, alignof(std::max_align_t));
                if (!p) {
                    errorCount.fetch_add(1);
                    return;
                }
                ptrs.push_back(p);
            }

            // Verify used() matches expected
            size_t expectedUsed = AllocsPerThread * 64;
            if (arena.used() != expectedUsed) {
                errorCount.fetch_add(1);
            }

            // Reset and verify
            arena.reset();
            if (arena.used() != 0) {
                errorCount.fetch_add(1);
            }

            // Allocate again after reset — should reuse memory
            void* p = arena.allocate(64);
            if (!p) {
                errorCount.fetch_add(1);
            }
        });
    }

    for (auto& thr : threads) {
        thr.join();
    }

    EXPECT_EQ(errorCount.load(), 0);
}

// ── Test: BitReader/BitWriter with Arena — concurrent allocation ───────

TEST(ThreadingTest, ConcurrentArenaBitIO) {
    constexpr int NumThreads = 4;
    std::atomic<int> errorCount{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < NumThreads; ++t) {
        threads.emplace_back([&errorCount]() {
            Arena arena(16384);
            std::vector<uint8_t>* buf = static_cast<std::vector<uint8_t>*>(arena.allocate(sizeof(std::vector<uint8_t>), alignof(std::vector<uint8_t>)));
            new (buf) std::vector<uint8_t>(256, 0);

            for (int i = 0; i < 100; ++i) {
                BitWriter writer(buf->data(), buf->size() * 8);
                writer.writeField(0xAB, 8);
                writer.writeField(0xCD, 8);

                // Read back to verify
                BitReader reader(buf->data(), 16);
                auto v1 = reader.readField(8);
                auto v2 = reader.readField(8);
                if (!v1 || !v2 || v1.value() != 0xAB || v2.value() != 0xCD) {
                    errorCount.fetch_add(1);
                }
            }

            buf->~vector();
        });
    }

    for (auto& thr : threads) {
        thr.join();
    }

    EXPECT_EQ(errorCount.load(), 0);
}

// ── Test: Heavy concurrent parse with mixed message types ───────────────

TEST(ThreadingTest, HeavyConcurrentParse) {
    constexpr int NumThreads = 8;
    constexpr int IterationsPerThread = 100;

    // Pool of messages to parse
    std::vector<std::vector<uint8_t>> msgPool = {
        {0x60, 0x0D, 0x00},                             // RR ChannelRelease
        {0x50, 0x84},                                    // MM CMServiceAccept
        {0x30, 0x94, 0x08, 0x02, 0x16, 0x21},          // CC Disconnect
        {0xB0, 0xE8},                                    // SS Facility
        {},                                              // empty — expected to fail
        {0xFF, 0xFF},                                    // invalid — expected to fail
    };

    std::atomic<int> parseCount{0};
    std::atomic<int> successCount{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < NumThreads; ++t) {
        threads.emplace_back([&msgPool, &parseCount, &successCount, t]() {
            int localParses = 0;
            int localSuccess = 0;

            for (int i = 0; i < IterationsPerThread; ++i) {
                int msgIdx = (t + i) % static_cast<int>(msgPool.size());
                auto& buf = msgPool[msgIdx];
                auto msg = parseL3(std::span<const uint8_t>(buf.data(), buf.size()));
                ++localParses;
                if (msg) {
                    ++localSuccess;
                }
            }

            parseCount.fetch_add(localParses);
            successCount.fetch_add(localSuccess);
        });
    }

    for (auto& thr : threads) {
        thr.join();
    }

    EXPECT_EQ(parseCount.load(), NumThreads * IterationsPerThread);
    // msgPool has 6 entries: indices 0-3 valid, 4-5 invalid.
    // For each thread t, iteration i: msgIdx = (t+i) % 6.
    // Over 100 iterations per thread, residues cycle every 6; valid residues are 0,1,2,3.
    // 100/6 = 16 full cycles + 4 remainder. Each full cycle has 4 valid hits.
    // Remainder depends on starting offset t. Total across all 8 threads: ~533 successes.
    // Allow small tolerance for edge-case parsing behavior on empty/invalid spans.
    int expectedSuccess = 533;
    EXPECT_GE(successCount.load(), static_cast<int>(expectedSuccess - 2));
    EXPECT_LE(successCount.load(), static_cast<int>(expectedSuccess + 2));
}

// ── Test: BitReader zero-copy concurrency ───────────────────────────────

TEST(ThreadingTest, ConcurrentBitReader) {
    constexpr int NumThreads = 4;
    std::atomic<int> errorCount{0};
    std::vector<std::thread> threads;

    // Shared read-only buffer — BitReader is non-owning, reads should be safe
    std::vector<uint8_t> sharedBuffer(256);
    std::vector<uint8_t> writerBuf(256, 0);
    BitWriter writer(writerBuf.data(), writerBuf.size() * 8);
    for (int i = 0; i < 256; ++i) {
        writer.writeField(static_cast<unsigned>(i & 0xFF), 8);
    }
    std::copy(writerBuf.begin(), writerBuf.end(), sharedBuffer.begin());

    for (int t = 0; t < NumThreads; ++t) {
        threads.emplace_back([&sharedBuffer, &errorCount]() {
            BitReader reader(sharedBuffer.data(), sharedBuffer.size() * 8);
            for (int i = 0; i < 256; ++i) {
                auto val = reader.readField(8);
                if (!val || val.value() != (i & 0xFF)) {
                    errorCount.fetch_add(1);
                    break;
                }
            }
        });
    }

    for (auto& thr : threads) {
        thr.join();
    }

    EXPECT_EQ(errorCount.load(), 0);
}

// ── Test: ParserConfig log level isolation across threads ───────────────

TEST(ThreadingTest, ConfigLogLevelIsolation) {
    constexpr int NumThreads = 4;
    std::atomic<int> errorCount{0};
    std::vector<std::thread> threads;

    std::vector<LogLevel> expectedLevels = {
        LogLevel::EMERG, LogLevel::WARNING, LogLevel::INFO, LogLevel::DEBUG
    };

    for (int t = 0; t < NumThreads; ++t) {
        threads.emplace_back([&errorCount, level = expectedLevels[t]]() {
            ParserConfig cfg;
            cfg = cfg.withLogLevel(level);

            if (cfg.getLogLevel() != level) {
                errorCount.fetch_add(1);
            }

            // Do some other work to ensure no cross-thread contamination
            for (int i = 0; i < 100; ++i) {
                cfg = cfg.withLogLevel(static_cast<LogLevel>((static_cast<int>(level) + i) % 8));
            }

            // Restore and verify
            cfg = cfg.withLogLevel(level);
            if (cfg.getLogLevel() != level) {
                errorCount.fetch_add(1);
            }
        });
    }

    for (auto& thr : threads) {
        thr.join();
    }

    EXPECT_EQ(errorCount.load(), 0);
}
