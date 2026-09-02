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
#include "gsml3parser/arena.h"

using namespace gsml3parser;

// Test: basic allocation, alignment and usage accounting.
TEST(ArenaTest, Allocate_AlignmentAndUsage) {
    Arena arena(64);
    auto* p1 = static_cast<int*>(arena.allocate(sizeof(int), alignof(int)));
    ASSERT_NE(p1, nullptr);
    *p1 = 42;
    EXPECT_EQ(arena.used(), sizeof(int));
    auto* p2 = static_cast<double*>(arena.allocate(sizeof(double), alignof(double)));
    ASSERT_NE(p2, nullptr);
    *p2 = 1.5;
    EXPECT_EQ(arena.used(), sizeof(int) + sizeof(double));
    // Different allocations must live at different addresses (unrelated
    // pointer types are compared through void*).
    EXPECT_NE(reinterpret_cast<const void*>(p1), reinterpret_cast<const void*>(p2));
    EXPECT_EQ(*p1, 42); // earlier allocation stays valid (append-only)
}

// Test: allocations larger than the current block start a new block;
// pointers from the old block remain valid.
TEST(ArenaTest, NewBlock_KeepsOldPointersValid) {
    Arena arena(64);
    auto* small = static_cast<int*>(arena.allocate(sizeof(int)));
    *small = 7;
    auto* big = static_cast<uint8_t*>(arena.allocate(1 << 20));
    ASSERT_NE(big, nullptr);
    EXPECT_GT(arena.capacity(), (1u << 20));
    EXPECT_EQ(*small, 7);
    EXPECT_GT(arena.remaining(), 0u);
}

// Test: reset releases usage; remaining() reflects the last block.
TEST(ArenaTest, Reset_ClearsUsage) {
    Arena arena(1024);
    arena.allocate(512);
    EXPECT_GT(arena.used(), 0u);
    arena.reset();
    EXPECT_EQ(arena.used(), 0u);
    EXPECT_EQ(arena.remaining(), 0u); // no blocks after reset
    auto* p = arena.allocate(16);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(arena.used(), 16u);
}

// Test: invalid arguments are rejected.
TEST(ArenaTest, InvalidArguments) {
    Arena arena(64);
    EXPECT_EQ(arena.allocate(0), nullptr);
    EXPECT_EQ(arena.allocate(8, 3), nullptr); // 3 is not a power of two
}
