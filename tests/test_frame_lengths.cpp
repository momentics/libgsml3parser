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
    EXPECT_EQ(detail::fixedFrameLength(0x06, L3CipheringModeCommand::MTI), 2 + L3CipheringModeCommand{}.bodyLength()) << "Ciphering Mode Command";
    EXPECT_EQ(detail::fixedFrameLength(0x05, L3CMServiceAccept::MTI), 2 + L3CMServiceAccept{}.bodyLength()) << "CM Service Accept";
    // L3CMServiceReject has no default ctor (explicit cause ctor), so build
    // a default-cause instance; bodyLength() is a constant 1 regardless.
    EXPECT_EQ(detail::fixedFrameLength(0x05, L3CMServiceReject::MTI), 2 + L3CMServiceReject{MMRejectCause::Zero}.bodyLength()) << "CM Service Reject";
    EXPECT_EQ(detail::fixedFrameLength(0x05, L3CMServiceAbort::MTI), 2 + L3CMServiceAbort{}.bodyLength()) << "CM Service Abort";
    EXPECT_EQ(detail::fixedFrameLength(0x05, L3MMAbort::MTI), 2 + L3MMAbort{}.bodyLength()) << "MM Abort";
    EXPECT_EQ(detail::fixedFrameLength(0x05, L3MMStatus::MTI), 2 + L3MMStatus{}.bodyLength()) << "MM Status";
    EXPECT_EQ(detail::fixedFrameLength(0x03, L3CCStatus::MTI), 2 + L3CCStatus{}.bodyLength()) << "CC Status";
    EXPECT_EQ(detail::fixedFrameLength(0x03, L3CCNotify::MTI), 2 + L3CCNotify{}.bodyLength()) << "CC Notify";
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

// The normative fixed-length table (TS 44.018 / 24.078 / 24.008), pinned
// numerically: every constant-body message and its exact total wire length
// (2-byte L3 header + body). Pinning the raw (pd, mti) pairs guards against
// drift even if a class MTI or body length ever changes.
TEST(FrameLengths, NormativeFixedLengthTable) {
    // Radio Resource (TS 44.018).
    EXPECT_EQ(detail::fixedFrameLength(0x06, 0x12), 3u); // RR Status (1-byte body)
    EXPECT_EQ(detail::fixedFrameLength(0x06, 0x13), 2u); // Classmark Enquiry (no value part)
    EXPECT_EQ(detail::fixedFrameLength(0x06, 0x28), 3u); // Handover Failure (cause)
    EXPECT_EQ(detail::fixedFrameLength(0x06, 0x29), 3u); // Assignment Complete (cause)
    EXPECT_EQ(detail::fixedFrameLength(0x06, 0x2C), 3u); // Handover Complete (cause)
    EXPECT_EQ(detail::fixedFrameLength(0x06, 0x2F), 3u); // Assignment Failure (cause)
    EXPECT_EQ(detail::fixedFrameLength(0x06, 0x35), 3u); // Ciphering Mode Command (1-byte body)
    // Mobility Management (TS 24.008).
    EXPECT_EQ(detail::fixedFrameLength(0x05, 0x21), 2u); // CM Service Accept (no value part)
    EXPECT_EQ(detail::fixedFrameLength(0x05, 0x22), 3u); // CM Service Reject (cause)
    EXPECT_EQ(detail::fixedFrameLength(0x05, 0x23), 2u); // CM Service Abort (no value part)
    EXPECT_EQ(detail::fixedFrameLength(0x05, 0x29), 2u); // MM Abort (no value part)
    EXPECT_EQ(detail::fixedFrameLength(0x05, 0x31), 3u); // MM Status (cause)
    // Call Control (TS 24.078).
    EXPECT_EQ(detail::fixedFrameLength(0x03, 0x3D), 6u); // CC Status (4-octet body)
    EXPECT_EQ(detail::fixedFrameLength(0x03, 0x3E), 3u); // CC Notify (1-byte cause)
    // Broadcast / Group Call Control.
    EXPECT_EQ(detail::fixedFrameLength(0x01, 0x04), 2u); // BCC Call Confirmed (no body)
    EXPECT_EQ(detail::fixedFrameLength(0x01, 0x09), 2u); // BCC Connect Acknowledge (no body)
    EXPECT_EQ(detail::fixedFrameLength(0x00, 0x03), 2u); // GCC Call Confirmed (no body)

    // Pairs outside the table must stay absent: RR 0x3E is System
    // Information Type 17 and the Measurement Report carries a variable
    // body on the air interface (TS 44.018), so header-based framing uses
    // the boundary heuristic for them rather than this table.
    EXPECT_EQ(detail::fixedFrameLength(0x06, L3SystemInformationType17::MTI), 0u);
    EXPECT_EQ(detail::fixedFrameLength(0x06, L3MeasurementReport::MTI), 0u);
}
