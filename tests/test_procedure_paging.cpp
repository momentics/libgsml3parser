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

/// Unit tests for PagingProcedure (TS 04.08 9.1.25).
/// Validates paging with up to 3 attempts using T3109 timer between retries.

#include <gtest/gtest.h>
#include <gsml3parser/stack/procedures/paging.h>
#include <gsml3parser/message_types.h>
#include <gsml3parser/common/l3common.h>
#include <gsml3parser/rr/l3rrmessages.h>
#include <gsml3parser/visitor.h>
#include <gsml3parser/stack/typed_external_data.h>

#include <chrono>
#include <cstdint>

using namespace gsml3parser;
using namespace std::chrono_literals;

inline void feedStep(Procedure& p, const ParsedMessage& msg) {
    [[maybe_unused]] auto r = p.feed(msg, nullptr, nullptr);
}

// ── Helpers ────────────────────────────────────────────────────────────────

static ParsedMessage makePagingResponse() {
    return ParsedMessage{RRM{L3PagingResponse::builder().build()}};
}

static ParsedMessage makeDummyMsg() {
    return ParsedMessage{RRM{L3ChannelRequest::builder().build()}};
}

// ── Tests ──────────────────────────────────────────────────────────────────

// [TS 04.08 9.1.25] feedExternal from INIT triggers paging SendResponse (PagingRequestType1).
TEST(PagingProcedureTest, Pag_Init_FeedExternal_StartsPage1) {
    L3MobileIdentity identity(0x12345678u);
    PagingProcedure proc(identity);

    EXPECT_EQ(proc.identity().isTMSI(), true);
    EXPECT_EQ(proc.type(), procedure::ProcedureType::Paging);
    EXPECT_EQ(proc.state(), procedure::ProcedureState::Initiated);

    bool sinkCalled = false;
    auto res = proc.feedExternalTyped(PagingTrigger{},
        [&sinkCalled](SMAction action, const ParsedMessage&, const SubscriberSession*) {
            EXPECT_EQ(action, SMAction::SendResponse);
            sinkCalled = true;
        });

    EXPECT_EQ(res.action, ProcedureStepResult::Action::SendResponseWithToken);
    EXPECT_TRUE(sinkCalled);
    EXPECT_EQ(proc.state(), procedure::ProcedureState::InProgress);
}

// [TS 04.08 9.1.25] PagingResponse at any paging attempt completes the procedure.
TEST(PagingProcedureTest, Pag_PageResponse_Completes) {
    L3MobileIdentity identity(0xDEADBEEFu);
    PagingProcedure proc(identity);

    [[maybe_unused]] auto _r = proc.feedExternalTyped(PagingTrigger{});
    feedStep(proc, makeDummyMsg());

    auto res = proc.feed(makePagingResponse(), nullptr, nullptr);

    EXPECT_EQ(res.action, ProcedureStepResult::Action::Completed);
    EXPECT_EQ(proc.state(), procedure::ProcedureState::Completed);
}

// [TS 04.08 9.1.25] T3109 expiry advances to the next page attempt (retry logic).
TEST(PagingProcedureTest, Pag_Tick_RetriesPaging) {
    L3MobileIdentity identity(0xABCDEF01u);
    PagingProcedure proc(identity);

    [[maybe_unused]] auto _r = proc.feedExternalTyped(PagingTrigger{});
    feedStep(proc, makeDummyMsg());

    // In WAIT_PAGE1 with T3109 running (5000ms). Tick past expiry to trigger retry.
    auto r1 = proc.tick(6000ms);
    EXPECT_EQ(r1.action, ProcedureStepResult::Action::Continue);

    // Advance through the send state to the next wait state.
    feedStep(proc, makeDummyMsg());

    // Tick again to advance to next attempt.
    auto r2 = proc.tick(6000ms);
    EXPECT_EQ(r2.action, ProcedureStepResult::Action::Continue);

    feedStep(proc, makeDummyMsg());

    // Third expiry from WAIT_PAGE3 should fail (no more retries).
    auto r3 = proc.tick(6000ms);
    EXPECT_EQ(r3.action, ProcedureStepResult::Action::Failed);
    EXPECT_EQ(proc.state(), procedure::ProcedureState::Failed);
}

// [TS 04.08 9.1.25] cancel() aborts the paging procedure and sets Failed state.
TEST(PagingProcedureTest, Pag_Cancel_Aborts) {
    L3MobileIdentity identity(0x11223344u);
    PagingProcedure proc(identity);

    [[maybe_unused]] auto _r = proc.feedExternalTyped(PagingTrigger{});
    proc.cancel();

    EXPECT_EQ(proc.state(), procedure::ProcedureState::Failed);

    auto res = proc.tick(1000ms);
    EXPECT_EQ(res.action, ProcedureStepResult::Action::Continue);
}
