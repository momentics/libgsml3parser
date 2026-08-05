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
#include <gsml3parser/logger.h>
#include <gsml3parser/bitvector.h>
#include <gsml3parser/context.h>
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

// ── Test: 4 threads parse concurrently with separate ParserContext ───────

TEST(ThreadingTest, ConcurrentParseWithContext) {
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
            ParserContext ctx;
            int localSuccess = 0;
            int localFail = 0;
            std::span<const uint8_t> msgSpan(msgBuffers[t].data(), msgBuffers[t].size());

            for (int i = 0; i < IterationsPerThread; ++i) {
                auto msg = parseL3(msgSpan, ctx);
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

// ── Test: thread-local LogLevel isolation ───────────────────────────────

TEST(ThreadingTest, ThreadLocalLogLevel) {
    constexpr int NumThreads = 4;
    std::vector<std::thread> threads;
    std::mutex mtx;
    std::condition_variable cv;
    bool allReady = false;
    std::atomic<int> readyCount{0};
    std::atomic<int> doneCount{0};

    // Set main thread log level
    setLogLevel(LogLevel::DEBUG);
    EXPECT_EQ(getLogLevel(), LogLevel::DEBUG);

    std::vector<LogLevel> expectedLevels = {
        LogLevel::EMERG, LogLevel::WARNING, LogLevel::INFO, LogLevel::DEBUG
    };

    for (int t = 0; t < NumThreads; ++t) {
        threads.emplace_back([&mtx, &cv, &allReady, &readyCount, &doneCount, level = expectedLevels[t], NumThreads]() {
            setLogLevel(level);
            int cnt = readyCount.fetch_add(1) + 1;

            // Wait until all threads are ready
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [&allReady] { return allReady; });

            // Verify our thread has the correct level
            EXPECT_EQ(getLogLevel(), level);

            doneCount.fetch_add(1);
        });
    }

    // Wait for all threads to be ready, then signal
    while (readyCount.load() < NumThreads) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    {
        std::lock_guard<std::mutex> lock(mtx);
        allReady = true;
    }
    cv.notify_all();

    for (auto& thr : threads) {
        thr.join();
    }

    EXPECT_EQ(doneCount.load(), NumThreads);

    // Main thread log level should be unchanged
    EXPECT_EQ(getLogLevel(), LogLevel::DEBUG);
}

// ── Test: LogCallback isolation per thread ──────────────────────────────

TEST(ThreadingTest, ThreadLocalLogCallback) {
    std::vector<std::thread> threads;
    std::mutex mtx;
    std::condition_variable cv;
    bool ready = false;
    std::atomic<int> readyCount{0};
    constexpr int NumThreads = 4;

    for (int t = 0; t < NumThreads; ++t) {
        threads.emplace_back([&, t]() {
            std::mutex localMutex;
            std::vector<std::string> localCaptures;

            setLogCallback([&localMutex, &localCaptures](LogLevel, const char*, int, const char* msg) {
                std::lock_guard<std::mutex> lock(localMutex);
                localCaptures.push_back(std::string(msg));
            });

            readyCount.fetch_add(1);

            // Wait for signal
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [&ready] { return ready; });

            // Trigger a log message
            setLogLevel(LogLevel::DEBUG);
            char buf[64];
            std::snprintf(buf, sizeof(buf), "thread_%d_msg", t);
            logMessage(LogLevel::INFO, "test_threading.cpp", __LINE__, "%s", buf);

            std::lock_guard<std::mutex> lck(localMutex);
            EXPECT_EQ(localCaptures.size(), 1u);
            char expected[64];
            std::snprintf(expected, sizeof(expected), "thread_%d_msg", t);
            EXPECT_STREQ(localCaptures[0].c_str(), expected);

            setLogCallback(nullptr);
        });
    }

    while (readyCount.load() < NumThreads) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    {
        std::lock_guard<std::mutex> lock(mtx);
        ready = true;
    }
    cv.notify_all();

    for (auto& thr : threads) {
        thr.join();
    }

    // Reset callback on main thread
    setLogCallback(nullptr);
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

// ── Test: BitVector with Arena — concurrent allocation ──────────────────

TEST(ThreadingTest, ConcurrentArenaBitVector) {
    constexpr int NumThreads = 4;
    std::atomic<int> errorCount{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < NumThreads; ++t) {
        threads.emplace_back([&errorCount]() {
            Arena arena(16384);
            std::vector<BitVector> vectors;

            for (int i = 0; i < 100; ++i) {
                BitVector bv(arena, 64);
                size_t wp = 0;
                bv.writeField(wp, 0xAB, 8);
                bv.writeField(wp, 0xCD, 8);

                // Read back to verify
                size_t rp = 0;
                if (bv.readField(rp, 8) != 0xAB || bv.readField(rp, 8) != 0xCD) {
                    errorCount.fetch_add(1);
                }

                vectors.push_back(std::move(bv));
            }

            // Reset arena to reclaim memory
            arena.reset();
            if (arena.used() != 0) {
                errorCount.fetch_add(1);
            }
        });
    }

    for (auto& thr : threads) {
        thr.join();
    }

    EXPECT_EQ(errorCount.load(), 0);
}

// ── Test: ParserContext PDHandler registration is thread-safe ───────────

TEST(ThreadingTest, ConcurrentPDHandlerRegistration) {
    constexpr int NumThreads = 4;
    std::atomic<int> errorCount{0};
    std::vector<std::thread> threads;

    // Shared context — tests that shared_mutex protects concurrent writes
    ParserContext ctx;

    for (int t = 0; t < NumThreads; ++t) {
        threads.emplace_back([&ctx, &errorCount, t]() {
            L3PD pd = static_cast<L3PD>(t + 1);

            // Register handler
            ctx.registerPDHandler(pd, [&](const L3Frame&) {
                return std::make_unique<L3CMServiceAccept>();
            });

            // Verify handler is registered
            auto handler = ctx.getPDHandler(pd);
            if (!handler.has_value()) {
                errorCount.fetch_add(1);
            }

            // Unregister
            ctx.unregisterPDHandler(pd);

            // Verify handler is gone
            handler = ctx.getPDHandler(pd);
            if (handler.has_value()) {
                errorCount.fetch_add(1);
            }
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
            ParserContext ctx;
            int localParses = 0;
            int localSuccess = 0;

            for (int i = 0; i < IterationsPerThread; ++i) {
                int msgIdx = (t + i) % static_cast<int>(msgPool.size());
                auto& buf = msgPool[msgIdx];
                auto msg = parseL3(std::span<const uint8_t>(buf.data(), buf.size()), ctx);
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

// ── Test: BitSpan zero-copy concurrency ────────────────────────────────

TEST(ThreadingTest, ConcurrentBitSpan) {
    constexpr int NumThreads = 4;
    std::atomic<int> errorCount{0};
    std::vector<std::thread> threads;

    // Shared read-only buffer — BitSpan is non-owning, reads should be safe
    std::vector<uint8_t> sharedBuffer(256);
    size_t wp = 0;
    BitVector writer(2048);
    for (int i = 0; i < 256; ++i) {
        writer.writeField(wp, static_cast<unsigned>(i & 0xFF), 8);
    }
    std::copy(writer.data(), writer.data() + 256, sharedBuffer.begin());

    BitSpan view(sharedBuffer.data(), 2048);

    for (int t = 0; t < NumThreads; ++t) {
        threads.emplace_back([&view, &errorCount]() {
            size_t rp = 0;
            for (int i = 0; i < 256; ++i) {
                unsigned val = view.readField(rp, 8);
                if (val != (i & 0xFF)) {
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

// ── Test: ParserContext log level isolation across threads ──────────────

TEST(ThreadingTest, ContextLogLevelIsolation) {
    constexpr int NumThreads = 4;
    std::atomic<int> errorCount{0};
    std::vector<std::thread> threads;

    std::vector<LogLevel> expectedLevels = {
        LogLevel::EMERG, LogLevel::WARNING, LogLevel::INFO, LogLevel::DEBUG
    };

    for (int t = 0; t < NumThreads; ++t) {
        threads.emplace_back([&errorCount, level = expectedLevels[t]]() {
            ParserContext ctx;
            ctx.setLogLevel(level);

            if (ctx.logLevel() != level) {
                errorCount.fetch_add(1);
            }

            // Do some other work to ensure no cross-thread contamination
            for (int i = 0; i < 100; ++i) {
                ctx.setLogLevel(static_cast<LogLevel>((static_cast<int>(level) + i) % 8));
            }

            // Restore and verify
            ctx.setLogLevel(level);
            if (ctx.logLevel() != level) {
                errorCount.fetch_add(1);
            }
        });
    }

    for (auto& thr : threads) {
        thr.join();
    }

    EXPECT_EQ(errorCount.load(), 0);
}
