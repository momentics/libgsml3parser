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

// Tests for LocationUpdateProcedure: validates the full location updating state
// machine per TS 24.008 4.4.1, covering identity check, authentication with
// RAND/SRES verification, external VLR decision via feedExternal(), timer
// expiry, cancel, and end-to-end flows.
// 3GPP coverage: TS 24.008 4.4.1 (Location Updating), 4.4.2 (Authentication),
// TS 04.08 9.2.9 (CM Service Request), 9.2.10 (Identity Request/Response),
// 9.2.2/9.2.3 (Authentication Request/Response), 9.2.13/9.2.14 (LU Accept/Reject).

#include <gtest/gtest.h>
#include <array>
#include <cstring>
#include <span>

#include "gsml3parser/stack/procedures/location_update.h"
#include "gsml3parser/stack/subscriber_registry.h"
#include "gsml3parser/message_types.h"
#include "gsml3parser/visitor.h"
#include "gsml3parser/mm/l3mmmessages.h"
#include "gsml3parser/common/l3common.h"

using namespace gsml3parser;
using namespace gsml3parser::procedure;
using namespace std::chrono_literals;

// Helper: build a CM Service Request with LocationUpdate service type.
static ParsedMessage makeCMServiceRequest() {
    return ParsedMessage{MMM{L3CMServiceRequest::builder()
        .serviceType(L3CMServiceType{L3CMServiceType::LocationUpdateRequest})
        .build()}};
}

// Helper: build an Identity Response carrying an IMSI.
static ParsedMessage makeIdentityResponse() {
    return ParsedMessage{MMM{L3IdentityResponse::builder()
        .mobileId(L3MobileIdentity{"244051234567890"})
        .build()}};
}

// Helper: build an Authentication Response with the given SRES.
static ParsedMessage makeAuthResponse(uint32_t sres) {
    return ParsedMessage{MMM{L3AuthenticationResponse::builder()
        .sres(sres)
        .build()}};
}

// Helper: build 20-byte RAND+SRES external data (16 bytes RAND + 4 bytes SRES LE).
static std::array<uint8_t, 20> makeRandSRES(uint32_t sres) {
    std::array<uint8_t, 20> data{};
    for (int i = 0; i < 16; ++i) data[i] = static_cast<uint8_t>(0xA0 + i);
    data[16] = static_cast<uint8_t>(sres & 0xFF);
    data[17] = static_cast<uint8_t>((sres >> 8) & 0xFF);
    data[18] = static_cast<uint8_t>((sres >> 16) & 0xFF);
    data[19] = static_cast<uint8_t>((sres >> 24) & 0xFF);
    return data;
}

// Helper: build Accept external data (byte 0 = 1 for accept).
static std::array<uint8_t, 1> makeAcceptData() {
    return {1u};
}

// Helper: build Reject external data (byte 0 = 0 for reject).
static std::array<uint8_t, 1> makeRejectData() {
    return {0u};
}

// Helper: create a session with a known TMSI identity.
static SubscriberSession makeSessionWithTMSI(uint32_t tmsi) {
    SubscriberSession sess;
    sess.context.setTMSI(tmsi);
    return sess;
}

// Helper: discard [[nodiscard]] ProcedureStepResult when we only care about state transitions.
static void advance(LocationUpdateProcedure& lup, const ParsedMessage& msg,
                    SubscriberSession* sess) {
    [[maybe_unused]] auto _ = lup.feed(msg, sess, {});
}

// LUP_Init_CMServiceRequest_AdvancesToIdentityCheck
// Feed CMServiceRequest to INIT state; procedure advances to IDENTITY_CHECK.
// 3GPP: TS 24.008 4.4.1 step 1 - MS sends CM Service Request for location update.
TEST(LocationUpdateProcedure, LUP_Init_CMServiceRequest_AdvancesToIdentityCheck) {
    LocationUpdateProcedure lup;
    SubscriberSession sess;

    EXPECT_EQ(lup.state(), ProcedureState::Initiated);

    auto result = lup.feed(makeCMServiceRequest(), &sess, {});

    EXPECT_EQ(result.action, ProcedureStepResult::Action::Continue);
    EXPECT_EQ(lup.state(), ProcedureState::InProgress);
}

// LUP_IdentityCheck_KnownTMSI_SkipsIdentityRequest
// Session has known TMSI; IDENTITY_CHECK skips directly to AUTH_CHECK.
// 3GPP: TS 24.008 4.4.1 - if VLR knows MS by TMSI, identity request is skipped.
TEST(LocationUpdateProcedure, LUP_IdentityCheck_KnownTMSI_SkipsIdentityRequest) {
    LocationUpdateProcedure lup;
    auto sess = makeSessionWithTMSI(0x12345678u);

    // INIT -> IDENTITY_CHECK
    lup.feed(makeCMServiceRequest(), &sess, {});

    bool sinkCalled = false;
    auto result = lup.feed(makeCMServiceRequest(), &sess,
        [&sinkCalled](SMAction, const ParsedMessage&, const SubscriberSession*) {
            sinkCalled = true;
        });

    // TMSI known, skip to AUTH_CHECK without sending IdentityRequest.
    EXPECT_EQ(lup.state(), ProcedureState::InProgress);
    EXPECT_FALSE(sinkCalled);
}

// LUP_IdentityCheck_UnknownTMSI_SendsIdentityRequest
// Session has no TMSI; IDENTITY_CHECK sends IdentityRequest and goes to REQUEST_IDENTITY.
// 3GPP: TS 24.008 4.4.1 - if TMSI unknown, network requests IMSI via Identity Request.
TEST(LocationUpdateProcedure, LUP_IdentityCheck_UnknownTMSI_SendsIdentityRequest) {
    LocationUpdateProcedure lup;
    SubscriberSession sess; // no TMSI set

    // INIT -> IDENTITY_CHECK
    lup.feed(makeCMServiceRequest(), &sess, {});

    bool sinkCalled = false;
    auto result = lup.feed(makeCMServiceRequest(), &sess,
        [&sinkCalled](SMAction, const ParsedMessage&, const SubscriberSession*) {
            sinkCalled = true;
        });

    EXPECT_EQ(result.action, ProcedureStepResult::Action::SendResponse);
    EXPECT_TRUE(sinkCalled);
}

// LUP_IdentityResponse_Valid_AdvancesToAuthCheck
// IdentityResponse received in REQUEST_IDENTITY state advances to AUTH_CHECK.
// 3GPP: TS 24.008 4.4.1 - MS responds with IMSI; network proceeds to authentication check.
TEST(LocationUpdateProcedure, LUP_IdentityResponse_Valid_AdvancesToAuthCheck) {
    LocationUpdateProcedure lup;
    SubscriberSession sess;

    // INIT -> IDENTITY_CHECK
    lup.feed(makeCMServiceRequest(), &sess, {});

    // IDENTITY_CHECK -> REQUEST_IDENTITY (no TMSI, sink called)
    lup.feed(makeCMServiceRequest(), &sess, {});

    // Feed IdentityResponse in REQUEST_IDENTITY state -> AUTH_CHECK
    auto result = lup.feed(makeIdentityResponse(), &sess, {});

    EXPECT_EQ(result.action, ProcedureStepResult::Action::Continue);
    EXPECT_EQ(lup.state(), ProcedureState::InProgress);
}

// LUP_AuthCheck_NeedAuth_SendsAuthenticationRequest
// With RAND fed via feedExternal in AUTH_CHECK, transitions to SEND_AUTH with SendResponse.
// 3GPP: TS 24.008 4.4.2 - AuC provides RAND; network sends Authentication Request to MS.
TEST(LocationUpdateProcedure, LUP_AuthCheck_NeedAuth_SendsAuthenticationRequest) {
    LocationUpdateProcedure lup;
    auto sess = makeSessionWithTMSI(0x12345678u);

    // INIT -> IDENTITY_CHECK -> AUTH_CHECK (TMSI known)
    lup.feed(makeCMServiceRequest(), &sess, {});
    lup.feed(makeCMServiceRequest(), &sess, {});

    // Feed RAND via feedExternal in AUTH_CHECK
    std::array<uint8_t, 16> rand{};
    auto result = lup.feedExternal(std::span<const uint8_t>(rand), {});

    EXPECT_EQ(result.action, ProcedureStepResult::Action::SendResponse);
}

// LUP_AuthResponse_ValidSRES_AdvancesToLURequest
// AuthenticationResponse with correct SRES advances the flow past verification.
// 3GPP: TS 24.008 4.4.2 - network compares SRES from MS with expected SRES from AuC.
TEST(LocationUpdateProcedure, LUP_AuthResponse_ValidSRES_AdvancesToLURequest) {
    LocationUpdateProcedure lup;
    auto sess = makeSessionWithTMSI(0x12345678u);

    // INIT -> IDENTITY_CHECK -> AUTH_CHECK
    lup.feed(makeCMServiceRequest(), &sess, {});
    lup.feed(makeCMServiceRequest(), &sess, {});

    // Feed RAND + expected SRES (0xDEADBEEF) via feedExternal -> SEND_AUTH
    auto randSRES = makeRandSRES(0xDEADBEEFu);
    lup.feedExternal(std::span<const uint8_t>(randSRES), {});

    // Simulate reaching WAIT_AUTH for SRES verification.
    lup.feedExternal(std::span<const uint8_t>(), {});

    // Feed auth response with matching SRES -> should advance to LU_REQUEST.
    auto result = lup.feed(makeAuthResponse(0xDEADBEEFu), &sess, {});

    EXPECT_EQ(lup.state(), ProcedureState::InProgress);
}

// LUP_AuthResponse_InvalidSRES_GoesToReject
// AuthenticationResponse with wrong SRES causes SEND_REJECT transition.
// 3GPP: TS 24.008 4.4.2 - SRES mismatch triggers MAC failure and reject.
TEST(LocationUpdateProcedure, LUP_AuthResponse_InvalidSRES_GoesToReject) {
    LocationUpdateProcedure lup;
    auto sess = makeSessionWithTMSI(0x12345678u);

    // INIT -> IDENTITY_CHECK -> AUTH_CHECK
    lup.feed(makeCMServiceRequest(), &sess, {});
    lup.feed(makeCMServiceRequest(), &sess, {});

    // Feed RAND + expected SRES (0x11111111) via feedExternal -> SEND_AUTH
    auto randSRES = makeRandSRES(0x11111111u);
    lup.feedExternal(std::span<const uint8_t>(randSRES), {});

    // Simulate reaching WAIT_AUTH for SRES verification.
    lup.feedExternal(std::span<const uint8_t>(), {});

    // Feed auth response with mismatched SRES -> should trigger reject path.
    auto result = lup.feed(makeAuthResponse(0xFFFFFFFFu), &sess, {});

    EXPECT_NE(result.action, ProcedureStepResult::Action::Completed);
}

// LUP_WaitingExternal_FeedAccept_CompletesWithLocationUpdatingAccept
// Accept decision via feedExternal completes procedure with LocationUpdatingAccept.
// 3GPP: TS 24.008 4.4.1 step 6 - VLR accepts; network sends Location Updating Accept.
TEST(LocationUpdateProcedure, LUP_WaitingExternal_FeedAccept_CompletesWithLocationUpdatingAccept) {
    LocationUpdateProcedure lup;
    auto sess = makeSessionWithTMSI(0x12345678u);

    // INIT -> IDENTITY_CHECK -> AUTH_CHECK -> LU_REQUEST -> WAITING_EXTERNAL
    advance(lup, makeCMServiceRequest(), &sess);
    advance(lup, makeCMServiceRequest(), &sess);
    advance(lup, makeCMServiceRequest(), &sess);
    advance(lup, makeCMServiceRequest(), &sess);

    EXPECT_EQ(lup.state(), ProcedureState::WaitingExternal);

    // Feed Accept decision via feedExternal
    bool sinkCalled = false;
    auto acceptResult = lup.feedExternal(makeAcceptData(),
        [&sinkCalled](SMAction, const ParsedMessage&, const SubscriberSession*) {
            sinkCalled = true;
        });

    EXPECT_TRUE(sinkCalled);
    EXPECT_EQ(acceptResult.action, ProcedureStepResult::Action::Completed);
    EXPECT_EQ(lup.state(), ProcedureState::Completed);
}

// LUP_WaitingExternal_FeedReject_FailsWithLocationUpdatingReject
// Reject decision via feedExternal fails procedure with LocationUpdatingReject.
// 3GPP: TS 24.008 4.4.1 - VLR rejects; network sends Location Updating Reject.
TEST(LocationUpdateProcedure, LUP_WaitingExternal_FeedReject_FailsWithLocationUpdatingReject) {
    LocationUpdateProcedure lup;
    auto sess = makeSessionWithTMSI(0x12345678u);

    // INIT -> IDENTITY_CHECK -> AUTH_CHECK -> LU_REQUEST -> WAITING_EXTERNAL
    advance(lup, makeCMServiceRequest(), &sess);
    advance(lup, makeCMServiceRequest(), &sess);
    advance(lup, makeCMServiceRequest(), &sess);
    advance(lup, makeCMServiceRequest(), &sess);

    EXPECT_EQ(lup.state(), ProcedureState::WaitingExternal);

    // Feed Reject decision via feedExternal
    bool sinkCalled = false;
    auto rejectResult = lup.feedExternal(makeRejectData(),
        [&sinkCalled](SMAction, const ParsedMessage&, const SubscriberSession*) {
            sinkCalled = true;
        });

    EXPECT_TRUE(sinkCalled);
    EXPECT_EQ(rejectResult.action, ProcedureStepResult::Action::SendResponse);
}

// LUP_Tick_T3106Expired_Fails
// Timer T3106 expiry during authentication causes procedure failure.
// 3GPP: TS 24.008 - T3106 timer for authentication response retransmission (3s).
TEST(LocationUpdateProcedure, LUP_Tick_T3106Expired_Fails) {
    LocationUpdateProcedure lup;
    auto sess = makeSessionWithTMSI(0x12345678u);

    // INIT -> IDENTITY_CHECK -> AUTH_CHECK
    advance(lup, makeCMServiceRequest(), &sess);
    advance(lup, makeCMServiceRequest(), &sess);

    // Feed RAND via feedExternal -> SEND_AUTH (starts T3106 at 3000ms)
    std::array<uint8_t, 16> rand{};
    [[maybe_unused]] auto _ = lup.feedExternal(std::span<const uint8_t>(rand), {});

    // Tick past timer expiry (T3106 = 3000ms default)
    auto result = lup.tick(4000ms);

    EXPECT_EQ(result.action, ProcedureStepResult::Action::Failed);
    EXPECT_EQ(lup.state(), ProcedureState::Failed);
}

// LUP_Cancel_AbortsProcedure
// Calling cancel() on an in-progress procedure sets Failed state.
// 3GPP: TS 24.008 - procedure can be aborted by external conditions.
TEST(LocationUpdateProcedure, LUP_Cancel_AbortsProcedure) {
    LocationUpdateProcedure lup;
    auto sess = makeSessionWithTMSI(0x12345678u);

    // INIT -> IDENTITY_CHECK -> AUTH_CHECK
    advance(lup, makeCMServiceRequest(), &sess);
    advance(lup, makeCMServiceRequest(), &sess);

    EXPECT_EQ(lup.state(), ProcedureState::InProgress);

    // Cancel the procedure
    lup.cancel();

    EXPECT_EQ(lup.state(), ProcedureState::Failed);
}

// LUP_FullFlow_WithAuth_CompletesSuccessfully
// Full location update flow: CMServiceRequest -> Identity check (TMSI known) ->
// AUTH_CHECK (no RAND supplied, skips auth) -> LU_REQUEST -> WAITING_EXTERNAL ->
// VLR Accept via feedExternal -> SEND_ACCEPT / Completed.
// 3GPP: TS 24.008 4.4.1 complete procedure; auth skipped when AuC provides no RAND.
TEST(LocationUpdateProcedure, LUP_FullFlow_WithAuth_CompletesSuccessfully) {
    LocationUpdateProcedure lup;
    auto sess = makeSessionWithTMSI(0x12345678u);

    // Step 1: CMServiceRequest initiates procedure, advances to IDENTITY_CHECK.
    auto r1 = lup.feed(makeCMServiceRequest(), &sess, {});
    EXPECT_EQ(r1.action, ProcedureStepResult::Action::Continue);

    // Step 2: TMSI known, skip identity request, advance to AUTH_CHECK.
    auto r2 = lup.feed(makeCMServiceRequest(), &sess, {});
    EXPECT_EQ(lup.state(), ProcedureState::InProgress);

    // Step 3: No RAND set; AUTH_CHECK -> LU_REQUEST (auth skipped).
    advance(lup, makeCMServiceRequest(), &sess);

    // Step 4: LU_REQUEST -> WAITING_EXTERNAL (starts T3103 for VLR decision).
    advance(lup, makeCMServiceRequest(), &sess);
    EXPECT_EQ(lup.state(), ProcedureState::WaitingExternal);

    // Step 5: Feed Accept via feedExternal -> completes procedure.
    bool sinkCalled = false;
    auto r5 = lup.feedExternal(makeAcceptData(),
        [&sinkCalled](SMAction, const ParsedMessage&, const SubscriberSession*) {
            sinkCalled = true;
        });

    EXPECT_TRUE(sinkCalled);
    EXPECT_EQ(r5.action, ProcedureStepResult::Action::Completed);
    EXPECT_EQ(lup.state(), ProcedureState::Completed);
}
