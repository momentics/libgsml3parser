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

// Cross-checks the framer fixed-length table against the message
// definitions (every table entry must equal
// 2 + Type{}.bodyLength(), and every variable-body message must be
// ABSENT from the table.

#include <gtest/gtest.h>

#include <gsml3parser/bitstream/frame_lengths.h>
#include <gsml3parser/message_types.h>
#include <gsml3parser/rr/l3rrmessages.h>
#include <gsml3parser/mm/l3mmmessages.h>
#include <gsml3parser/cc/l3ccmessages.h>
#include <gsml3parser/bcc/l3bccmessages.h>
#include <gsml3parser/gcc/l3gccmessages.h>
#include <gsml3parser/ls/l3lsmessages.h>

using namespace gsml3parser;

TEST(FrameLengths, TableMatchesMessageDefinitions) {
    // Every (pd, mti) pair in the table must equal 2 + Type{}.bodyLength()
    // for the corresponding message type. Explicit per-entry checks give
    // clear failure messages and pin the table against definition drift.
    EXPECT_EQ(detail::fixedFrameLength(0x06, L3RRStatus::MTI), 2 + L3RRStatus{}.bodyLength()) << "RR Status";
    EXPECT_EQ(detail::fixedFrameLength(0x06, L3ClassmarkEnquiry::MTI), 2 + L3ClassmarkEnquiry{}.bodyLength()) << "Classmark Enquiry";
    EXPECT_EQ(detail::fixedFrameLength(0x06, L3HandoverFailure::MTI), 2 + L3HandoverFailure{}.bodyLength()) << "Handover Failure";
    EXPECT_EQ(detail::fixedFrameLength(0x06, L3AssignmentComplete::MTI), 2 + L3AssignmentComplete{}.bodyLength()) << "Assignment Complete";
    EXPECT_EQ(detail::fixedFrameLength(0x06, L3HandoverComplete::MTI), 2 + L3HandoverComplete{}.bodyLength()) << "Handover Complete";
    EXPECT_EQ(detail::fixedFrameLength(0x06, L3AssignmentFailure::MTI), 2 + L3AssignmentFailure{}.bodyLength()) << "Assignment Failure";
    EXPECT_EQ(detail::fixedFrameLength(0x05, L3CMServiceAccept::MTI), 2 + L3CMServiceAccept{}.bodyLength()) << "CM Service Accept";
    // L3CMServiceReject has no default ctor (explicit cause ctor), so build
    // a default-cause instance; bodyLength() is a constant 1 regardless.
    EXPECT_EQ(detail::fixedFrameLength(0x05, L3CMServiceReject::MTI), 2 + L3CMServiceReject{MMRejectCause::Zero}.bodyLength()) << "CM Service Reject";
    EXPECT_EQ(detail::fixedFrameLength(0x05, L3CMServiceAbort::MTI), 2 + L3CMServiceAbort{}.bodyLength()) << "CM Service Abort";
    EXPECT_EQ(detail::fixedFrameLength(0x05, L3MMStatus::MTI), 2 + L3MMStatus{}.bodyLength()) << "MM Status";
    EXPECT_EQ(detail::fixedFrameLength(0x03, L3CCStatus::MTI), 2 + L3CCStatus{}.bodyLength()) << "CC Status";
    EXPECT_EQ(detail::fixedFrameLength(0x01, L3BCCCallConfirmed::MTI), 2 + L3BCCCallConfirmed{}.bodyLength()) << "BCC Call Confirmed";
    EXPECT_EQ(detail::fixedFrameLength(0x01, L3BCCConnectAcknowledge::MTI), 2 + L3BCCConnectAcknowledge{}.bodyLength()) << "BCC Connect Acknowledge";
    EXPECT_EQ(detail::fixedFrameLength(0x00, L3GCCCallConfirmed::MTI), 2 + L3GCCCallConfirmed{}.bodyLength()) << "GCC Call Confirmed";
}

TEST(FrameLengths, VariableLengthTypesAbsent) {
    // Every variable-body message must be absent (0) — framing them from
    // the table would truncate or over-consume.
    EXPECT_EQ(detail::fixedFrameLength(0x06, L3PagingResponse::MTI), 0u);
    EXPECT_EQ(detail::fixedFrameLength(0x06, L3ChannelRelease::MTI), 0u);
    EXPECT_EQ(detail::fixedFrameLength(0x06, L3ClassmarkChange::MTI), 0u);
    EXPECT_EQ(detail::fixedFrameLength(0x06, L3CipheringModeComplete::MTI), 0u);
    EXPECT_EQ(detail::fixedFrameLength(0x06, L3ImmediateAssignmentReject::MTI), 0u);
    EXPECT_EQ(detail::fixedFrameLength(0x06, L3PhysicalInformation::MTI), 0u);
    EXPECT_EQ(detail::fixedFrameLength(0x05, L3CMServiceRequest::MTI), 0u);
    // Variable bodies: IMSI Detach Indication carries LV
    // classmark + LV mobile identity; Location Service Request carries an
    // opaque variable body — neither may be framed from a fixed table.
    EXPECT_EQ(detail::fixedFrameLength(0x05, L3IMSIDetachIndication::MTI), 0u);
    EXPECT_EQ(detail::fixedFrameLength(0x0C, L3LocationServiceRequest::MTI), 0u);
    EXPECT_EQ(detail::fixedFrameLength(0x03, L3ReleaseComplete::MTI), 0u);
    EXPECT_EQ(detail::fixedFrameLength(0x01, L3BCCSetup::MTI), 0u);
    EXPECT_EQ(detail::fixedFrameLength(0x00, L3GCCSetup::MTI), 0u);
}
