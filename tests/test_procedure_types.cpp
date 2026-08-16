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

// Tests for procedure_types: validates that ProcedureType and ProcedureState enums
// are well-formed, helper functions return correct names, and ProcedureResult meets
// size constraints for cache-friendly usage in the Procedure Framework.
// 3GPP coverage: TS 24.008 chapters 4.x (MM procedures), TS 04.08 chapter 9 (RR procedures).

#include <gtest/gtest.h>
#include "gsml3parser/stack/procedure_types.h"

using namespace gsml3parser::procedure;

// Test: All ProcedureType enum values have distinct, valid uint8_t representations.
// Importance: Ensures the enum can be safely serialized and used as array indices.
// 3GPP: TS 24.008 procedure types form a closed set.
TEST(ProcedureType_EnumValues, AllDefined) {
    // Verify each known value is within uint8_t range.
    EXPECT_EQ(static_cast<uint8_t>(ProcedureType::LocationUpdate), 0x01);
    EXPECT_EQ(static_cast<uint8_t>(ProcedureType::Authentication), 0x02);
    EXPECT_EQ(static_cast<uint8_t>(ProcedureType::CipheringMode), 0x03);
    EXPECT_EQ(static_cast<uint8_t>(ProcedureType::CallSetup_MO), 0x04);
    EXPECT_EQ(static_cast<uint8_t>(ProcedureType::CallSetup_MT), 0x05);
    EXPECT_EQ(static_cast<uint8_t>(ProcedureType::ChannelAssignment), 0x06);
    EXPECT_EQ(static_cast<uint8_t>(ProcedureType::Handover), 0x07);
    EXPECT_EQ(static_cast<uint8_t>(ProcedureType::Paging), 0x08);
    EXPECT_EQ(static_cast<uint8_t>(ProcedureType::CMServiceRequest), 0x09);
    EXPECT_EQ(static_cast<uint8_t>(ProcedureType::IMSIDetach), 0x0A);
    EXPECT_EQ(static_cast<uint8_t>(ProcedureType::Unknown), 0xFF);
}

// Test: All ProcedureState enum values are defined and ordered logically.
// Importance: State machine transitions depend on consistent ordering.
TEST(ProcedureState_EnumValues, AllDefined) {
    // Verify lifecycle ordering: Initiated < InProgress < ... < TimedOut.
    EXPECT_LT(static_cast<uint8_t>(ProcedureState::Initiated),
              static_cast<uint8_t>(ProcedureState::InProgress));
    EXPECT_LT(static_cast<uint8_t>(ProcedureState::InProgress),
              static_cast<uint8_t>(ProcedureState::WaitingExternal));
    EXPECT_LT(static_cast<uint8_t>(ProcedureState::WaitingExternal),
              static_cast<uint8_t>(ProcedureState::Completed));
    // Terminal states are all distinct.
    EXPECT_NE(ProcedureState::Completed, ProcedureState::Failed);
    EXPECT_NE(ProcedureState::Failed, ProcedureState::TimedOut);
}

// Test: Default-constructed ProcedureResult has zeroed/default fields.
// Importance: Ensures safe stack allocation without explicit initialization.
TEST(ProcedureResult_DefaultConstruction, FieldsZeroed) {
    ProcedureResult res{};
    EXPECT_EQ(res.type, ProcedureType::Unknown);
    EXPECT_EQ(res.state, ProcedureState::Initiated);
    EXPECT_EQ(res.reason, std::string_view{});
}

// Test: procedureTypeName returns non-empty string for every defined ProcedureType.
// Importance: Logging and diagnostics depend on readable procedure names.
TEST(procedureTypeName_ValidInput, ReturnsNonEmpty) {
    ASSERT_NE(procedureTypeName(ProcedureType::LocationUpdate), "");
    ASSERT_NE(procedureTypeName(ProcedureType::Authentication), "");
    ASSERT_NE(procedureTypeName(ProcedureType::CipheringMode), "");
    ASSERT_NE(procedureTypeName(ProcedureType::CallSetup_MO), "");
    ASSERT_NE(procedureTypeName(ProcedureType::CallSetup_MT), "");
    ASSERT_NE(procedureTypeName(ProcedureType::ChannelAssignment), "");
    ASSERT_NE(procedureTypeName(ProcedureType::Handover), "");
    ASSERT_NE(procedureTypeName(ProcedureType::Paging), "");
    ASSERT_NE(procedureTypeName(ProcedureType::CMServiceRequest), "");
    ASSERT_NE(procedureTypeName(ProcedureType::IMSIDetach), "");
}

// Test: procedureTypeName returns "?" for Unknown type.
// Importance: Sentinel value allows callers to detect unrecognized procedures.
TEST(procedureTypeName_Unknown, ReturnsQuestionMark) {
    EXPECT_EQ(procedureTypeName(ProcedureType::Unknown), "?");
}

// Test: procedureStateName returns non-empty string for every ProcedureState.
// Importance: State transitions are logged using these names for debugging.
TEST(procedureStateName_AllStates, NonEmpty) {
    ASSERT_NE(procedureStateName(ProcedureState::Initiated), "");
    ASSERT_NE(procedureStateName(ProcedureState::InProgress), "");
    ASSERT_NE(procedureStateName(ProcedureState::WaitingExternal), "");
    ASSERT_NE(procedureStateName(ProcedureState::Completed), "");
    ASSERT_NE(procedureStateName(ProcedureState::Failed), "");
    ASSERT_NE(procedureStateName(ProcedureState::TimedOut), "");
}

// Test: ProcedureResult fits within 64 bytes for cache-friendly storage.
// Importance: ProcedureRunner stores ProcedureResult inline in procedure slots;
// exceeding 64 bytes would cause cache line splits under high load.
TEST(ProcedureResult_Size, CacheFriendly) {
    EXPECT_LE(sizeof(ProcedureResult), 64u);
}

// Test: ProcedureType round-trips correctly through uint8_t conversion.
// Importance: Serialization/deserialization of procedure type must be lossless.
TEST(ProcedureType_FromUInt8, RoundTrip) {
    auto roundTrip = [](ProcedureType orig) {
        uint8_t raw = static_cast<uint8_t>(orig);
        ProcedureType decoded = static_cast<ProcedureType>(raw);
        return decoded;
    };

    EXPECT_EQ(roundTrip(ProcedureType::LocationUpdate), ProcedureType::LocationUpdate);
    EXPECT_EQ(roundTrip(ProcedureType::Authentication), ProcedureType::Authentication);
    EXPECT_EQ(roundTrip(ProcedureType::CipheringMode), ProcedureType::CipheringMode);
    EXPECT_EQ(roundTrip(ProcedureType::CallSetup_MO), ProcedureType::CallSetup_MO);
    EXPECT_EQ(roundTrip(ProcedureType::CallSetup_MT), ProcedureType::CallSetup_MT);
    EXPECT_EQ(roundTrip(ProcedureType::ChannelAssignment), ProcedureType::ChannelAssignment);
    EXPECT_EQ(roundTrip(ProcedureType::Handover), ProcedureType::Handover);
    EXPECT_EQ(roundTrip(ProcedureType::Paging), ProcedureType::Paging);
    EXPECT_EQ(roundTrip(ProcedureType::CMServiceRequest), ProcedureType::CMServiceRequest);
    EXPECT_EQ(roundTrip(ProcedureType::IMSIDetach), ProcedureType::IMSIDetach);
    EXPECT_EQ(roundTrip(ProcedureType::Unknown), ProcedureType::Unknown);
}
