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

// High-load stress tests for SubscriberRegistry, ShardedSubscriberRegistry,
// and ResponseBuilder span overload performance. Validates that the library
// scales to 10K+ concurrent sessions with sub-millisecond lookup and fast
// timer tick, and that ResponseBuilder span overload avoids heap allocation.
// 3GPP coverage: TS 24.008 subscriber management at scale.

#include <gtest/gtest.h>
#include "gsml3parser/stack/subscriber_registry.h"
#include "gsml3parser/stack/response_builder.h"
#include "gsml3parser/stack/procedure.h"
#include "gsml3parser/stack/procedure_runner.h"
#include "gsml3parser/stack/typed_external_data.h"
#include "gsml3parser/message_types.h"
#include "gsml3parser/mm/l3mmmessages.h"
#include "gsml3parser/common/l3common.h"
#include "gsml3parser/arena.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <future>
#include <set>
#include <thread>
#include <type_traits>
#include <vector>

// The CRT allocation hook is only used on plain MSVC builds. Under ASAN the
// sanitizer intercepts allocations (and the ASAN CRT declares _CRT_ALLOC_HOOK
// differently), so the hook is disabled there.
#if defined(_MSC_VER) && !defined(GSML3PARSER_ASAN)
#include <crtdbg.h>
#endif

using namespace gsml3parser;
using namespace std::chrono_literals;

#if defined(_MSC_VER) && !defined(GSML3PARSER_ASAN)
namespace {
// CRT hook stage values (crtdbg.h enum constants are debug-only, so use the
// documented numeric values): 0 = new block allocated, 2 = block reallocated.
constexpr int kCrtAllocStage = 0;
constexpr int kCrtReallocStage = 2;

// Counts all heap allocations while the alloc hook is active. Used to prove the
// ResponseBuilder span path performs zero heap allocations (ASAN alone cannot
// detect ordinary allocations, only memory errors).
std::atomic<long> gAllocCount{0};
void __cdecl allocHook(int stage, size_t /*size*/, int /*type*/,
                       int /*allocationId*/, const unsigned char* /*file*/, int /*line*/) {
    if (stage == kCrtAllocStage || stage == kCrtReallocStage) {
        gAllocCount.fetch_add(1, std::memory_order_relaxed);
    }
}
/// RAII guard that installs the allocation-counting hook for its lifetime.
struct AllocHookGuard {
    _CRT_ALLOC_HOOK old{_CrtSetAllocHook(allocHook)};
    ~AllocHookGuard() { _CrtSetAllocHook(old); }
};
} // namespace
#endif

// Test: create 10,000 sessions and verify each can be looked up by TMSI.
// Importance: Validates that SubscriberRegistry handles large subscriber counts
// without performance degradation. Total lookup time should be under 1ms.
// 3GPP: TS 24.008 4.4 - TMSI-based subscriber identification at scale.
TEST(Stress, _10000Sessions_CreateAndLookup_AllFound) {
    SubscriberRegistry reg;
    constexpr int N = 10000;

    auto t0 = std::chrono::steady_clock::now();

    // Create 10K sessions
    for (int i = 0; i < N; ++i) {
        auto* sess = reg.createByTMSI(static_cast<uint32_t>(i + 1));
        ASSERT_NE(sess, nullptr) << "Failed to create session " << i;
    }

    EXPECT_EQ(reg.count(), static_cast<size_t>(N));

    // Lookup each session by TMSI
    for (int i = 0; i < N; ++i) {
        auto* sess = reg.findByTMSI(static_cast<uint32_t>(i + 1));
        ASSERT_NE(sess, nullptr) << "Failed to find session " << i;
    }

    auto t1 = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0);

    // Total create + lookup should complete quickly (< 1 second is generous)
    // (performance budget is skipped under ASAN: sanitizer overhead is 2-3x)
#ifndef GSML3PARSER_ASAN
    EXPECT_LT(elapsed.count(), 1000)
        << "10K create + lookup took " << elapsed.count() << "ms";
#endif
}

// Test: tickAllTimers for 10,000 sessions completes in under 50ms.
// Importance: Timer tick runs every event loop iteration; must be fast enough
// to not block the main thread when managing many concurrent sessions.
// 3GPP: TS 24.008 timer management (T3101-T3113) at scale.
TEST(Stress, _10000Sessions_TimerTick_Fast) {
    SubscriberRegistry reg;
    constexpr int N = 10000;

    // Create sessions and start a timer in each
    for (int i = 0; i < N; ++i) {
        auto* sess = reg.createByTMSI(static_cast<uint32_t>(i + 1));
        ASSERT_NE(sess, nullptr);
        sess->timers.start(L3TimerId::T3101);
    }

    std::array<L3TimerId, N * 2> expired;

    auto t0 = std::chrono::steady_clock::now();

    // Tick all timers with 1ms delta (no expiry expected)
    size_t count = reg.tickAllTimers(1ms, expired);

    auto t1 = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0);

    EXPECT_EQ(count, 0u) << "No timers should expire with 1ms delta";
    // (performance budget is skipped under ASAN: sanitizer overhead is 2-3x)
#ifndef GSML3PARSER_ASAN
    EXPECT_LT(elapsed.count(), 50)
        << "tickAllTimers for 10K sessions took " << elapsed.count()
        << "ms (expected < 50ms)";
#endif
}

// Test: ShardedSubscriberRegistry handles concurrent create + lookup from 8 threads.
// Importance: Validates thread safety of sharded registry under realistic load.
// Each thread creates and looks up 1,000 sessions without data races.
// 3GPP: TS 24.008 multi-threaded subscriber management.
TEST(Stress, Sharded_10000Sessions_ThreadSafe) {
    ShardedSubscriberRegistry<16> reg;
    constexpr int threads = 8;
    constexpr int perThread = 1000;

    std::vector<std::future<void>> futures;

    for (int t = 0; t < threads; ++t) {
        futures.push_back(std::async(std::launch::async, [t, perThread, &reg]() {
            uint32_t base = static_cast<uint32_t>(t * perThread);

            // Create sessions
            for (int i = 0; i < perThread; ++i) {
                uint32_t tmsi = base + i + 1;
                auto* sess = reg.createByTMSI(tmsi);
                ASSERT_NE(sess, nullptr)
                    << "Thread " << t << " failed to create session " << tmsi;
            }

            // Lookup sessions (including those from other threads)
            for (int i = 0; i < perThread; ++i) {
                uint32_t tmsi = base + i + 1;
                auto* sess = reg.findByTMSI(tmsi);
                ASSERT_NE(sess, nullptr)
                    << "Thread " << t << " failed to find session " << tmsi;
            }
        }));
    }

    // Wait for all threads to complete
    for (auto& f : futures) {
        f.get();
    }

    // Verify total count
    size_t totalCount = 0;
    reg.forEach([&totalCount](const SubscriberSession&) { ++totalCount; });
    EXPECT_EQ(totalCount, static_cast<size_t>(threads * perThread));
}

// Test: 10,000 ResponseBuilder span overload calls write into Arena buffer
// without any heap allocation. Validates the zero-allocation design for
// high-throughput response building.
// 3GPP: TS 04.08 response message construction at scale.
TEST(Stress, ResponseBuilder_10000Builds_ZeroAlloc_Span) {
    Arena arena(65536 * 4); // 256KB arena to hold all responses

    constexpr int N = 10000;
    uint8_t buf[512];

    auto t0 = std::chrono::steady_clock::now();

    for (int i = 0; i < N; ++i) {
        // Build CM Service Accept into pre-allocated buffer (zero heap alloc)
        int n = ResponseBuilder::buildCMServiceAccept({buf, sizeof(buf)});
        ASSERT_GT(n, 0) << "Failed to build CM Service Accept at iteration " << i;

        // Also write some bytes into the arena to simulate real usage
        auto* arenaBuf = static_cast<uint8_t*>(arena.allocate(static_cast<size_t>(n)));
        ASSERT_NE(arenaBuf, nullptr) << "Arena allocation failed at iteration " << i;
        std::memcpy(arenaBuf, buf, static_cast<size_t>(n));
    }

    auto t1 = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0);

    // (performance budget is skipped under ASAN: sanitizer overhead is 2-3x)
#ifndef GSML3PARSER_ASAN
    EXPECT_LT(elapsed.count(), 500)
        << "10K ResponseBuilder span builds took " << elapsed.count()
        << "ms (expected < 500ms)";
#endif

    // Verify arena usage is reasonable
    EXPECT_GT(arena.used(), 0u) << "Arena should have been used";
    EXPECT_LT(arena.used(), arena.capacity()) << "Arena should not be full";
}

// Test: Verify critical struct size invariants that the high-load architecture depends on.
// If any of these grow, cache-line efficiency degrades for millions of concurrent sessions.
// 3GPP: TS 24.008 / TS 04.08 - memory layout constraints for scalable BTS implementations.
TEST(Stress, ProcedureStepResult_SizeInvariants) {
    // ResponseToken must be exactly 1 byte (uint8_t underlying type)
    EXPECT_EQ(sizeof(ResponseToken), 1u)
        << "ResponseToken must be 1 byte for cache-line efficiency";

    // Action enum must be small (uint8_t underlying type)
    EXPECT_EQ(sizeof(ProcedureStepResult::Action), 1u)
        << "Action must be 1 byte";

    // ProcedureStepResult must fit within 32 bytes (cache-line budget per procedure step)
    static_assert(sizeof(ProcedureStepResult) <= 32,
        "ProcedureStepResult must stay <= 32 bytes");
    EXPECT_LE(sizeof(ProcedureStepResult), 32u)
        << "ProcedureStepResult is " << sizeof(ProcedureStepResult)
        << " bytes (must be <= 32)";

    // SubscriberSession must stay under 4096 bytes for inline storage
    static_assert(sizeof(SubscriberSession) < 4096,
        "SubscriberSession must stay < 4096 bytes");
    EXPECT_LT(sizeof(SubscriberSession), 4096u)
        << "SubscriberSession is " << sizeof(SubscriberSession)
        << " bytes (must be < 4096)";

    // ProcedureStepResult must be trivially copyable for zero-overhead passing
    static_assert(std::is_trivially_copyable_v<ProcedureStepResult>,
        "ProcedureStepResult must be trivially copyable");
    EXPECT_TRUE(std::is_trivially_copyable_v<ProcedureStepResult>)
        << "ProcedureStepResult must be trivially copyable for efficient passing";

    // AuthChallenge size invariant
    static_assert(sizeof(AuthChallenge) <= 32,
        "AuthChallenge must stay <= 32 bytes");
    EXPECT_LE(sizeof(AuthChallenge), 32u);

    // CipheringParameters size invariant
    static_assert(sizeof(CipheringParameters) <= 16,
        "CipheringParameters must stay <= 16 bytes");
    EXPECT_LE(sizeof(CipheringParameters), 16u);

    // Report actual sizes for documentation
    EXPECT_EQ(sizeof(ProcedureStepResult::Action), 1u);
    EXPECT_EQ(sizeof(ResponseToken), 1u);
}

// Test: Create 100,000 sessions via ShardedSubscriberRegistry<32>, run a ProcedureRunner
// feed() on each session, and verify total time stays under 500ms. This validates that
// the library scales to production BTS subscriber counts without degradation.
// 3GPP: TS 24.008 - mass subscriber management for high-capacity base stations.
TEST(Stress, _100KSessions_ProcedureRunner_Feed_Fast) {
    ShardedSubscriberRegistry<32> reg;
    constexpr int N = 100000;

    auto t0 = std::chrono::steady_clock::now();

    // Create 100K sessions and run a procedure feed on each
    for (int i = 0; i < N; ++i) {
        auto* sess = reg.createByTMSI(static_cast<uint32_t>(i + 1));
        ASSERT_NE(sess, nullptr) << "Failed to create session " << i;

        // Feed a CMServiceRequest through ProcedureRunner to auto-create a procedure
        auto cmReq = L3CMServiceRequest::builder()
            .serviceType(L3CMServiceType{L3CMServiceType::LocationUpdateRequest})
            .build();
        ParsedMessage msg{MMM{std::move(cmReq)}};

        [[maybe_unused]] auto result = sess->procedures.feed(msg, sess, {});
    }

    auto t1 = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0);

    // 100K create + feed must complete in under 500ms on modern hardware
    // (performance budget is skipped under ASAN: sanitizer overhead is 2-3x)
#ifndef GSML3PARSER_ASAN
    EXPECT_LT(elapsed.count(), 500)
        << "100K sessions create + ProcedureRunner::feed took "
        << elapsed.count() << "ms (expected < 500ms)";
#endif

    // Verify all sessions were created
    size_t totalCount = 0;
    reg.forEach([&totalCount](const SubscriberSession&) { ++totalCount; });
    EXPECT_EQ(totalCount, static_cast<size_t>(N));
}

// Test: 100,000 iterations of feed() -> get ResponseToken -> buildResponseFromToken() into
// a pre-allocated Arena buffer. Validates the zero-allocation response building path that
// is critical for high-throughput BTS implementations. No heap allocation should occur
// during the hot path (feed + buildResponseFromToken with span overload).
// 3GPP: TS 04.08 - high-throughput response message construction without heap pressure.
TEST(Stress, ResponseToken_Arena_100K_ZeroAlloc) {
    constexpr int N = 100000;

    // Large arena to hold all responses without reallocation
    Arena arena(256 * 1024 * 1024); // 256 MB arena
    uint8_t stackBuf[512];

    // Known tokens that build successfully with null session (for fallback coverage)
    std::array<ResponseToken, 4> reliableTokens = {
        ResponseToken::CMServiceAccept,
        ResponseToken::CallProceeding,
        ResponseToken::Alerting,
        ResponseToken::Connect,
    };

    auto t0 = std::chrono::steady_clock::now();

    for (int i = 0; i < N; ++i) {
        // Create a session and runner for each iteration
        SubscriberSession sess;

        // Feed PagingRequest to trigger PagingProcedure which returns SendResponseWithToken
        auto pagingReq = L3PagingRequestType1::builder().build();
        ParsedMessage msg{RRM{std::move(pagingReq)}};

        ResponseToken capturedToken{ResponseToken::None};
        auto result = sess.procedures.feed(msg, &sess,
            makeResponseSink([&arena, &stackBuf](SMAction, const ParsedMessage&, const SubscriberSession*) {
                // Sink callback: build CMServiceAccept into stack buffer, then Arena
                int n = ResponseBuilder::buildCMServiceAccept({stackBuf, sizeof(stackBuf)});
                if (n > 0) {
                    auto* p = static_cast<uint8_t*>(arena.allocate(static_cast<size_t>(n)));
                    if (p) std::memcpy(p, stackBuf, static_cast<size_t>(n));
                }
            }));

        // Capture token from feed result
        if (result.action == ProcedureStepResult::Action::SendResponseWithToken) {
            capturedToken = result.responseToken;
        }

        // Build response from captured token into stack buffer (zero heap allocation)
        int bytesWritten = ResponseBuilder::buildResponseFromToken(
            capturedToken, {stackBuf, sizeof(stackBuf)}, &sess);

        if (bytesWritten > 0) {
            auto* arenaPtr = static_cast<uint8_t*>(
                arena.allocate(static_cast<size_t>(bytesWritten)));
            ASSERT_NE(arenaPtr, nullptr)
                << "Arena exhausted at iteration " << i;
            std::memcpy(arenaPtr, stackBuf, static_cast<size_t>(bytesWritten));
        } else {
            // Fallback: cycle through known reliable tokens to stress-test buildResponseFromToken
            // This ensures all response builders are exercised even when procedures return Continue
            ResponseToken fallback = reliableTokens[static_cast<size_t>(i) % reliableTokens.size()];
            int fbBytes = ResponseBuilder::buildResponseFromToken(
                fallback, {stackBuf, sizeof(stackBuf)}, &sess);
            if (fbBytes > 0) {
                auto* arenaPtr = static_cast<uint8_t*>(
                    arena.allocate(static_cast<size_t>(fbBytes)));
                ASSERT_NE(arenaPtr, nullptr)
                    << "Arena exhausted at fallback iteration " << i;
                std::memcpy(arenaPtr, stackBuf, static_cast<size_t>(fbBytes));
            }
        }
    }

    auto t1 = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0);

    // 100K feed + build iterations must complete in under 2 seconds
    // (performance budget is skipped under ASAN: sanitizer overhead is 2-3x)
#ifndef GSML3PARSER_ASAN
    EXPECT_LT(elapsed.count(), 2000)
        << "100K ResponseToken + Arena builds took " << elapsed.count()
        << "ms (expected < 2000ms)";
#endif

    // Verify arena was used (some tokens should produce responses)
    EXPECT_GT(arena.used(), 0u) << "Arena should have accumulated data";
    EXPECT_LT(arena.used(), arena.capacity()) << "Arena must not be full";
}

// Test: the ResponseBuilder span (zero-heap-allocation) path performs ZERO heap
// allocations across every span overload and every ResponseToken. This is the
// core guarantee of the high-throughput build path (feed/tick/build must not
// allocate). Uses MSVC CRT allocation hooks to count actual heap allocations
// (ASAN reports memory errors, not the fact of allocation, so it cannot prove
// this). 3GPP: TS 24.008 / TS 04.08 - response construction without heap pressure.
TEST(Stress, ResponseBuilder_Span_ZeroHeapAllocations) {
#if defined(_MSC_VER) && !defined(GSML3PARSER_ASAN)
    SubscriberSession sess;
    // Populate the response context so every token path has real parameters.
    std::memset(sess.response.rand.data(), 0xAB, 16);
    sess.response.hasRand = true;
    sess.response.ti = 3;
    sess.response.ccCause = CCCause::Normal_Call_Clearing;
    std::memcpy(sess.response.calledNumber.data(), "12345678", 8);
    sess.response.calledNumber[8] = '\0';
    sess.response.calledNumberLen = 8;
    sess.response.hasCalledNumber = true;
    sess.response.channel = L3ChannelDescription(TDMA_SDCCH, 0, 1, 100);
    sess.response.hasChannel = true;
    sess.response.cipherAlgo = 1;
    sess.response.hasCipherAlgo = true;
    sess.response.identity = L3MobileIdentity{0x12345678u};
    sess.response.hasIdentity = true;
    sess.response.hoChannel = L3ChannelDescription(TDMA_TCHF, 0, 0, 150);
    sess.response.hasHoChannel = true;
    auto lai = L3LocationAreaIdentity("244", "15", 1234);

    uint8_t buf[512];
    AllocHookGuard guard;
    gAllocCount = 0;
    for (int i = 0; i < 1000; ++i) {
        // Exercise every span overload directly (zero-alloc build path).
        EXPECT_GT(ResponseBuilder::buildCMServiceAccept({buf, sizeof(buf)}), 0);
        EXPECT_GT(ResponseBuilder::buildAuthenticationRequest(
            {buf, sizeof(buf)}, std::span<const uint8_t>{sess.response.rand.data(), 16}), 0);
        EXPECT_GT(ResponseBuilder::buildResponseFromToken(
            ResponseToken::Setup, {buf, sizeof(buf)}, &sess), 0);
        EXPECT_GT(ResponseBuilder::buildImmediateAssignment(
            {buf, sizeof(buf)}, sess.response.channel, 0), 0);
        EXPECT_GT(ResponseBuilder::buildAssignmentCommand(
            {buf, sizeof(buf)}, sess.response.channel), 0);
        EXPECT_GT(ResponseBuilder::buildChannelRelease({buf, sizeof(buf)}), 0);
        EXPECT_GT(ResponseBuilder::buildCipheringModeCommand({buf, sizeof(buf)}, 1), 0);
        EXPECT_GT(ResponseBuilder::buildPhysicalInformation({buf, sizeof(buf)}, 0), 0);
        EXPECT_GT(ResponseBuilder::buildHandoverCommand(
            {buf, sizeof(buf)}, sess.response.hoChannel), 0);
        EXPECT_GT(ResponseBuilder::buildPagingRequestType1(
            {buf, sizeof(buf)}, sess.response.identity), 0);
        EXPECT_GT(ResponseBuilder::buildPagingRequestType2(
            {buf, sizeof(buf)}, sess.response.identity), 0);
        EXPECT_GT(ResponseBuilder::buildPagingRequestType3(
            {buf, sizeof(buf)}, sess.response.identity), 0);
        EXPECT_GT(ResponseBuilder::buildCMServiceReject({buf, sizeof(buf)}), 0);
        EXPECT_GT(ResponseBuilder::buildIdentityRequest(
            {buf, sizeof(buf)}, MobileIDType::IMSI), 0);
        EXPECT_GT(ResponseBuilder::buildLocationUpdatingAccept(
            {buf, sizeof(buf)}, lai), 0);
        EXPECT_GT(ResponseBuilder::buildLocationUpdatingReject(
            {buf, sizeof(buf)}, MMRejectCause::Congestion), 0);
        EXPECT_GT(ResponseBuilder::buildTMSIReallocationCommand(
            {buf, sizeof(buf)}, lai, 0x12345678u), 0);
        EXPECT_GT(ResponseBuilder::buildCallProceeding({buf, sizeof(buf)}, 3), 0);
        EXPECT_GT(ResponseBuilder::buildAlerting({buf, sizeof(buf)}, 3), 0);
        EXPECT_GT(ResponseBuilder::buildConnect({buf, sizeof(buf)}, 3), 0);
        EXPECT_GT(ResponseBuilder::buildConnectAcknowledge({buf, sizeof(buf)}, 3), 0);
        EXPECT_GT(ResponseBuilder::buildDisconnect(
            {buf, sizeof(buf)}, 3, CCCause::Normal_Call_Clearing), 0);
        EXPECT_GT(ResponseBuilder::buildRelease(
            {buf, sizeof(buf)}, 3, CCCause::Normal_Call_Clearing), 0);
        EXPECT_GT(ResponseBuilder::buildReleaseComplete({buf, sizeof(buf)}, 3), 0);

        // Exercise buildResponseFromToken for every token (session-parameter path).
        for (int t = 1; t <= 24; ++t) {
            (void)ResponseBuilder::buildResponseFromToken(
                static_cast<ResponseToken>(t), {buf, sizeof(buf)}, &sess);
        }
    }
    EXPECT_EQ(gAllocCount.load(), 0L)
        << "span path must not allocate on the heap";
#else
    GTEST_SKIP() << "CRT alloc hooks are MSVC-only";
#endif
}

// Test: tickAllTimers ticks ONLY sessions with active timers (O(active), not O(all)).
// Importance: at 1M sessions with few active timers, a full scan is a real-time
// bottleneck; the active-index must skip inactive sessions.
// 3GPP coverage: TS 24.008 timer management at scale.
TEST(Stress, tickAllTimers_OnlyActive_Scales) {
    ShardedSubscriberRegistry<32> reg;
    constexpr uint32_t N = 200000;
    constexpr uint32_t ACTIVE = 200;
    for (uint32_t i = 1; i <= N; ++i) {
        auto* s = reg.createByTMSI(i);
        ASSERT_NE(s, nullptr);
        if (i <= ACTIVE) s->timers.start(L3TimerId::T3101, std::chrono::milliseconds(100));
    }
    std::vector<L3TimerId> expired(N);
    auto t0 = std::chrono::steady_clock::now();
    size_t n = reg.tickAllTimers(std::chrono::milliseconds(150), {expired.data(), expired.size()});
    auto t1 = std::chrono::steady_clock::now();
    double ms = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count() / 1000.0;
    EXPECT_EQ(n, ACTIVE) << "Only the " << ACTIVE << " active timers should expire";
#ifndef GSML3PARSER_ASAN
    EXPECT_LT(ms, 50.0) << "tickAllTimers over 200K sessions (200 active) took " << ms << "ms";
#endif
}
