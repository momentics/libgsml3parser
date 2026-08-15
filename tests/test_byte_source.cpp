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
#include "gsml3parser/bitstream/byte_source.h"
#include <thread>
#include <vector>
#include <cstring>
#include <cstdio>
#include <ctime>

using namespace gsml3parser;

// ── SpanByteSource tests ───────────────────────────────────────────────

TEST(SpanByteSource, ReadAll) {
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    std::span<const uint8_t> sp(data);
    SpanByteSource src(sp);
    uint8_t buf[10];

    size_t n = src.read(buf, sizeof(buf));
    ASSERT_EQ(n, 5u);
    ASSERT_EQ(std::memcmp(buf, data, 5), 0);
}

TEST(SpanByteSource, PartialReads) {
    uint8_t data[] = {0xAA, 0xBB, 0xCC, 0xDD};
    std::span<const uint8_t> sp(data);
    SpanByteSource src(sp);
    uint8_t buf[10];

    size_t n1 = src.read(buf, 2);
    ASSERT_EQ(n1, 2u);
    ASSERT_EQ(buf[0], 0xAA);
    ASSERT_EQ(buf[1], 0xBB);

    size_t n2 = src.read(buf + 2, 2);
    ASSERT_EQ(n2, 2u);
    ASSERT_EQ(buf[2], 0xCC);
    ASSERT_EQ(buf[3], 0xDD);

    ASSERT_EQ(src.remaining(), 0u);
}

TEST(SpanByteSource, EOFReturnsZero) {
    uint8_t data[] = {0x42};
    std::span<const uint8_t> sp(data);
    SpanByteSource src(sp);
    uint8_t buf[10];

    (void)src.read(buf, 1); // consume the only byte
    size_t n = src.read(buf, sizeof(buf));
    ASSERT_EQ(n, 0u);
}

TEST(SpanByteSource, EmptySpan) {
    uint8_t dummy;
    std::span<const uint8_t> sp(&dummy, 0);
    SpanByteSource src(sp);
    uint8_t buf[10];

    size_t n = src.read(buf, sizeof(buf));
    ASSERT_EQ(n, 0u);
}

// ── FileByteSource tests ───────────────────────────────────────────────

TEST(FileByteSource, ReadKnownFile) {
    const char* tmpEnv = std::getenv("TEMP");
    if (!tmpEnv) tmpEnv = std::getenv("TMP");
    if (!tmpEnv) tmpEnv = "/tmp";

    char tmp[512];
    std::snprintf(tmp, sizeof(tmp), "%s/test_file_source_%u.bin",
                  tmpEnv, static_cast<unsigned>(std::time(nullptr)));

    std::FILE* f = std::fopen(tmp, "wb");
    ASSERT_TRUE(f != nullptr);
    uint8_t expected[] = {0xDE, 0xAD, 0xBE, 0xEF};
    std::fwrite(expected, 1, sizeof(expected), f);
    std::fclose(f);

    f = std::fopen(tmp, "rb");
    ASSERT_TRUE(f != nullptr);
    FileByteSource src(f);
    uint8_t buf[16];

    size_t n = src.read(buf, sizeof(buf));
    ASSERT_EQ(n, 4u);
    ASSERT_EQ(std::memcmp(buf, expected, sizeof(expected)), 0);

    // Second read returns EOF.
    n = src.read(buf, sizeof(buf));
    ASSERT_EQ(n, 0u);

    (void)std::remove(tmp);
}

// ── RingBuffer tests ───────────────────────────────────────────────────

TEST(RingBuffer, WriteReadRoundTrip) {
    RingBuffer rb(256);
    uint8_t data[] = {0x10, 0x20, 0x30, 0x40, 0x50};

    size_t written = rb.write(data, sizeof(data));
    ASSERT_EQ(written, 5u);
    ASSERT_EQ(rb.available(), 5u);

    uint8_t buf[256];
    size_t rn = rb.read(buf, sizeof(buf));
    ASSERT_EQ(rn, 5u);
    ASSERT_EQ(std::memcmp(buf, data, sizeof(data)), 0);
}

TEST(RingBuffer, WrapAround) {
    RingBuffer rb(16);
    uint8_t buf[32];

    // Fill half.
    uint8_t d1[] = {0x01, 0x02, 0x03, 0x04};
    rb.write(d1, sizeof(d1));

    // Read back to advance tail near end of buffer.
    uint8_t tmp[32];
    (void)rb.read(tmp, sizeof(tmp));

    // Write enough to wrap around the internal buffer boundary.
    uint8_t d2[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
                    0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    size_t w = rb.write(d2, sizeof(d2));
    ASSERT_GT(w, 0u);

    size_t r = rb.read(buf, sizeof(buf));
    ASSERT_EQ(r, w);
    ASSERT_EQ(std::memcmp(buf, d2, w), 0);
}

TEST(RingBuffer, OverflowBehaviour) {
    RingBuffer rb(8);
    uint8_t data[16];
    std::memset(data, 0xAB, sizeof(data));

    // Bitmask ring buffer rounds capacity to power-of-two and sacrifices one slot.
    // RingBuffer(8) -> physical=8, effective=7. Can accept at most 7 bytes.
    size_t w = rb.write(data, sizeof(data));
    ASSERT_EQ(w, 7u);

    uint8_t buf[32];
    size_t r = rb.read(buf, sizeof(buf));
    ASSERT_EQ(r, 7u);
}

TEST(RingBuffer, PartialReads) {
    RingBuffer rb(64);
    uint8_t data[] = {0x11, 0x22, 0x33, 0x44, 0x55};
    rb.write(data, sizeof(data));

    uint8_t buf[32];

    size_t r1 = rb.read(buf, 2);
    ASSERT_EQ(r1, 2u);
    ASSERT_EQ(buf[0], 0x11);
    ASSERT_EQ(buf[1], 0x22);

    size_t r2 = rb.read(buf + 2, 3);
    ASSERT_EQ(r2, 3u);
    ASSERT_EQ(buf[2], 0x33);
    ASSERT_EQ(buf[3], 0x44);
    ASSERT_EQ(buf[4], 0x55);

    ASSERT_EQ(rb.available(), 0u);
}

TEST(RingBuffer, EmptyReadReturnsZero) {
    RingBuffer rb(64);
    uint8_t buf[16];
    size_t n = rb.read(buf, sizeof(buf));
    ASSERT_EQ(n, 0u);
}

// ── Concurrent producer/consumer ───────────────────────────────────────

TEST(RingBuffer, ConcurrentProducerConsumer) {
    RingBuffer rb(4096);
    constexpr int kChunks = 100;
    constexpr size_t TotalBytes = static_cast<size_t>(kChunks) * 32;

    // Producer: write kChunks blocks of 32 bytes each.
    auto producer = [&]() {
        for (int i = 0; i < kChunks; ++i) {
            uint8_t block[32];
            for (int j = 0; j < 32; ++j)
                block[j] = static_cast<uint8_t>((i * 32 + j) & 0xFF);

            size_t total = 0;
            while (total < sizeof(block)) {
                size_t w = rb.write(block + total, sizeof(block) - total);
                if (w == 0) {
                    std::this_thread::yield();
                }
                total += w;
            }
        }
    };

    // Consumer: read exactly TotalBytes.
    std::vector<uint8_t> received;
    received.reserve(TotalBytes);
    auto consumer = [&]() {
        uint8_t buf[256];
        while (received.size() < TotalBytes) {
            size_t n = rb.read(buf, sizeof(buf));
            if (n > 0) {
                received.insert(received.end(), buf, buf + n);
            } else {
                std::this_thread::yield();
            }
        }
    };

    // Build expected data for comparison.
    std::vector<uint8_t> expected(TotalBytes);
    for (int i = 0; i < kChunks; ++i)
        for (int j = 0; j < 32; ++j)
            expected[static_cast<size_t>(i) * 32 + j] = static_cast<uint8_t>((i * 32 + j) & 0xFF);

    std::thread pt(producer);
    std::thread ct(consumer);
    ct.join();
    pt.join();

    ASSERT_EQ(received.size(), expected.size());
    ASSERT_EQ(std::memcmp(received.data(), expected.data(), expected.size()), 0);
}

// Stress test: tiny buffer, large data volume - maximizes wrap-around and contention.
TEST(RingBuffer, ConcurrentStressTinyBuffer) {
    RingBuffer rb(64);
    constexpr size_t kChunks = 500;
    constexpr size_t ChunkSize = 8;
    constexpr size_t TotalBytes = kChunks * ChunkSize;

    auto producer = [&]() {
        for (size_t i = 0; i < kChunks; ++i) {
            uint8_t block[8];
            for (size_t j = 0; j < ChunkSize; ++j)
                block[j] = static_cast<uint8_t>((i * ChunkSize + j) & 0xFF);

            size_t total = 0;
            while (total < sizeof(block)) {
                size_t w = rb.write(block + total, sizeof(block) - total);
                if (w == 0) std::this_thread::yield();
                total += w;
            }
        }
    };

    std::vector<uint8_t> received;
    received.reserve(TotalBytes);
    auto consumer = [&]() {
        uint8_t buf[16];
        while (received.size() < TotalBytes) {
            size_t n = rb.read(buf, sizeof(buf));
            if (n > 0) {
                received.insert(received.end(), buf, buf + n);
            } else {
                std::this_thread::yield();
            }
        }
    };

    std::vector<uint8_t> expected(TotalBytes);
    for (size_t i = 0; i < kChunks; ++i)
        for (size_t j = 0; j < ChunkSize; ++j)
            expected[i * ChunkSize + j] = static_cast<uint8_t>((i * ChunkSize + j) & 0xFF);

    std::thread pt(producer);
    std::thread ct(consumer);
    ct.join();
    pt.join();

    ASSERT_EQ(received.size(), expected.size());
    ASSERT_EQ(std::memcmp(received.data(), expected.data(), expected.size()), 0);
}

// Verify that atomic<size_t> is lock-free on the target platform.
TEST(RingBuffer, AtomicsAreLockFree) {
    // std::atomic<size_t> is lock-free on all modern 64-bit platforms (x86-64, ARM64).
    // On 32-bit x86 it may fall back to a mutex, which is still correct but slower.
    EXPECT_TRUE(std::atomic<size_t>::is_always_lock_free)
        << "atomic<size_t> is not lock-free on this platform; "
            "RingBuffer will use a fallback lock (still correct but slower)";
}

// Capacity is rounded up to next power of two with one sacrificed slot.
TEST(RingBuffer, CapacityRoundedToPowerOfTwo) {
    RingBuffer rb(7);   // should round to cap=8, effective=7
    uint8_t data[7];
    for (int i = 0; i < 7; i++) data[i] = static_cast<uint8_t>(i);
    size_t w = rb.write(data, 7);
    EXPECT_EQ(w, 7u);   // all 7 bytes accepted
    uint8_t buf[16];
    size_t r = rb.read(buf, sizeof(buf));
    EXPECT_EQ(r, 7u);
    for (int i = 0; i < 7; i++) EXPECT_EQ(buf[i], data[i]);
}
