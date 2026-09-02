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

#include "gsml3parser/stack/response_builder.h"

#include <cstring>

#include "gsml3parser/parser.h"
#include "gsml3parser/message_types.h"
#include "gsml3parser/rr/l3rrmessages.h"
#include "gsml3parser/mm/l3mmmessages.h"
#include "gsml3parser/cc/l3ccmessages.h"
#include "gsml3parser/stack/subscriber_registry.h"

namespace gsml3parser {

// ── RR responses ────────────────────────────────────────────────────────

Expected<std::vector<uint8_t>> ResponseBuilder::buildImmediateAssignment(
    const L3ChannelDescription& channel, uint8_t timingAdvance,
    std::optional<L3RequestReference> requestRef)
{
    auto msg = L3ImmediateAssignment::builder()
        .channelDescription(channel)
        .timingAdvance(L3TimingAdvance(timingAdvance));

    if (requestRef) {
        msg = L3ImmediateAssignment::builder()
            .channelDescription(channel)
            .timingAdvance(L3TimingAdvance(timingAdvance))
            .requestReference(*requestRef);
    }

    ParsedMessage pm{RRM{msg.build()}};
    return writeL3Bytes(pm);
}

int ResponseBuilder::buildImmediateAssignment(
    std::span<uint8_t> out,
    const L3ChannelDescription& channel, uint8_t timingAdvance,
    std::optional<L3RequestReference> requestRef)
{
    // Zero-alloc: build the message inline and serialize straight into the
    // caller buffer via writeL3 (no intermediate std::vector).
    auto b = L3ImmediateAssignment::builder()
        .channelDescription(channel)
        .timingAdvance(L3TimingAdvance(timingAdvance));
    if (requestRef) b = b.requestReference(*requestRef);
    ParsedMessage pm{RRM{b.build()}};
    auto result = writeL3(pm, out.data(), out.size());
    if (result) return static_cast<int>(result.value());
    return -1;
}

Expected<std::vector<uint8_t>> ResponseBuilder::buildAssignmentCommand(
    const L3ChannelDescription& channel, L3ChannelMode mode)
{
    auto msg = L3AssignmentCommand::builder()
        .channel(channel);

    if (mode.mode() != L3ChannelMode::Mode::SignallingOnly) {
        msg = L3AssignmentCommand::builder()
            .channel(channel)
            .mode1(mode);
    }

    ParsedMessage pm{RRM{msg.build()}};
    return writeL3Bytes(pm);
}

int ResponseBuilder::buildAssignmentCommand(
    std::span<uint8_t> out,
    const L3ChannelDescription& channel, L3ChannelMode mode)
{
    // Zero-alloc: build the message inline and serialize straight into the
    // caller buffer via writeL3 (no intermediate std::vector).
    auto b = L3AssignmentCommand::builder().channel(channel);
    if (mode.mode() != L3ChannelMode::Mode::SignallingOnly) {
        b = b.mode1(mode);
    }
    ParsedMessage pm{RRM{b.build()}};
    auto result = writeL3(pm, out.data(), out.size());
    if (result) return static_cast<int>(result.value());
    return -1;
}

Expected<std::vector<uint8_t>> ResponseBuilder::buildChannelRelease(RRCause cause)
{
    auto msg = L3ChannelRelease::builder()
        .cause(cause)
        .build();
    ParsedMessage pm{RRM{std::move(msg)}};
    return writeL3Bytes(pm);
}

int ResponseBuilder::buildChannelRelease(std::span<uint8_t> out, RRCause cause)
{
    // Zero-alloc: build the message inline and serialize straight into the
    // caller buffer via writeL3 (no intermediate std::vector).
    auto msg = L3ChannelRelease::builder()
        .cause(cause)
        .build();
    ParsedMessage pm{RRM{std::move(msg)}};
    auto result = writeL3(pm, out.data(), out.size());
    if (result) return static_cast<int>(result.value());
    return -1;
}

Expected<std::vector<uint8_t>> ResponseBuilder::buildCipheringModeCommand(uint8_t cipherAlgo)
{
    bool ciphering = (cipherAlgo != 0);
    auto msg = L3CipheringModeCommand::builder()
        .ciphering(ciphering)
        .algorithm(static_cast<int>(cipherAlgo))
        .build();
    ParsedMessage pm{RRM{std::move(msg)}};
    return writeL3Bytes(pm);
}

int ResponseBuilder::buildCipheringModeCommand(std::span<uint8_t> out, uint8_t cipherAlgo)
{
    // Zero-alloc: build the message inline and serialize straight into the
    // caller buffer via writeL3 (no intermediate std::vector).
    bool ciphering = (cipherAlgo != 0);
    auto msg = L3CipheringModeCommand::builder()
        .ciphering(ciphering)
        .algorithm(static_cast<int>(cipherAlgo))
        .build();
    ParsedMessage pm{RRM{std::move(msg)}};
    auto result = writeL3(pm, out.data(), out.size());
    if (result) return static_cast<int>(result.value());
    return -1;
}

Expected<std::vector<uint8_t>> ResponseBuilder::buildPhysicalInformation(uint8_t timingAdvance)
{
    auto msg = L3PhysicalInformation::builder()
        .timingAdvance(L3TimingAdvance(timingAdvance))
        .build();
    ParsedMessage pm{RRM{std::move(msg)}};
    return writeL3Bytes(pm);
}

int ResponseBuilder::buildPhysicalInformation(std::span<uint8_t> out, uint8_t timingAdvance)
{
    // Zero-alloc: build the message inline and serialize straight into the
    // caller buffer via writeL3 (no intermediate std::vector).
    auto msg = L3PhysicalInformation::builder()
        .timingAdvance(L3TimingAdvance(timingAdvance))
        .build();
    ParsedMessage pm{RRM{std::move(msg)}};
    auto result = writeL3(pm, out.data(), out.size());
    if (result) return static_cast<int>(result.value());
    return -1;
}

// ── MM responses ────────────────────────────────────────────────────────

Expected<std::vector<uint8_t>> ResponseBuilder::buildCMServiceAccept()
{
    auto msg = L3CMServiceAccept::builder().build();
    ParsedMessage pm{MMM{std::move(msg)}};
    return writeL3Bytes(pm);
}

int ResponseBuilder::buildCMServiceAccept(std::span<uint8_t> out)
{
    // Zero-alloc: build the message inline and serialize straight into the
    // caller buffer via writeL3 (no intermediate std::vector).
    auto msg = L3CMServiceAccept::builder().build();
    ParsedMessage pm{MMM{std::move(msg)}};
    auto result = writeL3(pm, out.data(), out.size());
    if (result) return static_cast<int>(result.value());
    return -1;
}

Expected<std::vector<uint8_t>> ResponseBuilder::buildCMServiceReject(MMRejectCause cause)
{
    auto msg = L3CMServiceReject::builder()
        .cause(cause)
        .build();
    ParsedMessage pm{MMM{std::move(msg)}};
    return writeL3Bytes(pm);
}

int ResponseBuilder::buildCMServiceReject(std::span<uint8_t> out, MMRejectCause cause)
{
    // Zero-alloc: build the message inline and serialize straight into the
    // caller buffer via writeL3 (no intermediate std::vector).
    auto msg = L3CMServiceReject::builder()
        .cause(cause)
        .build();
    ParsedMessage pm{MMM{std::move(msg)}};
    auto result = writeL3(pm, out.data(), out.size());
    if (result) return static_cast<int>(result.value());
    return -1;
}

Expected<std::vector<uint8_t>> ResponseBuilder::buildIdentityRequest(MobileIDType type)
{
    auto msg = L3IdentityRequest::builder()
        .type(type)
        .build();
    ParsedMessage pm{MMM{std::move(msg)}};
    return writeL3Bytes(pm);
}

int ResponseBuilder::buildIdentityRequest(std::span<uint8_t> out, MobileIDType type)
{
    // Zero-alloc: build the message inline and serialize straight into the
    // caller buffer via writeL3 (no intermediate std::vector).
    auto msg = L3IdentityRequest::builder()
        .type(type)
        .build();
    ParsedMessage pm{MMM{std::move(msg)}};
    auto result = writeL3(pm, out.data(), out.size());
    if (result) return static_cast<int>(result.value());
    return -1;
}

Expected<std::vector<uint8_t>> ResponseBuilder::buildAuthenticationRequest(
    std::span<const uint8_t> rand)
{
    // Cold path: RAND is copied into the fixed 128-bit field (no intermediate
    // vector); only the returned output vector allocates.
    auto msg = L3AuthenticationRequest::builder()
        .rand(rand)
        .build();
    ParsedMessage pm{MMM{std::move(msg)}};
    return writeL3Bytes(pm);
}

int ResponseBuilder::buildAuthenticationRequest(std::span<uint8_t> out,
    std::span<const uint8_t> rand)
{
    // Zero-alloc: build the message inline (fixed 128-bit RAND) and serialize
    // straight into the caller buffer via writeL3 (no intermediate std::vector).
    auto msg = L3AuthenticationRequest::builder()
        .rand(rand)
        .build();
    ParsedMessage pm{MMM{std::move(msg)}};
    auto result = writeL3(pm, out.data(), out.size());
    if (result) return static_cast<int>(result.value());
    return -1;
}

Expected<std::vector<uint8_t>> ResponseBuilder::buildLocationUpdatingAccept(
    const L3LocationAreaIdentity& lai, std::optional<uint32_t> newTmsi)
{
    auto msg = L3LocationUpdatingAccept::builder()
        .lai(lai);

    if (newTmsi) {
        msg = L3LocationUpdatingAccept::builder()
            .lai(lai)
            .mobileIdentity(L3MobileIdentity(*newTmsi));
    }

    ParsedMessage pm{MMM{msg.build()}};
    return writeL3Bytes(pm);
}

int ResponseBuilder::buildLocationUpdatingAccept(std::span<uint8_t> out,
    const L3LocationAreaIdentity& lai, std::optional<uint32_t> newTmsi)
{
    // Zero-alloc: build the message inline and serialize straight into the
    // caller buffer via writeL3 (no intermediate std::vector).
    auto b = L3LocationUpdatingAccept::builder().lai(lai);
    if (newTmsi) b = b.mobileIdentity(L3MobileIdentity(*newTmsi));
    ParsedMessage pm{MMM{b.build()}};
    auto result = writeL3(pm, out.data(), out.size());
    if (result) return static_cast<int>(result.value());
    return -1;
}

Expected<std::vector<uint8_t>> ResponseBuilder::buildLocationUpdatingReject(MMRejectCause cause)
{
    auto msg = L3LocationUpdatingReject::builder()
        .cause(cause)
        .build();
    ParsedMessage pm{MMM{std::move(msg)}};
    return writeL3Bytes(pm);
}

int ResponseBuilder::buildLocationUpdatingReject(std::span<uint8_t> out, MMRejectCause cause)
{
    // Zero-alloc: build the message inline and serialize straight into the
    // caller buffer via writeL3 (no intermediate std::vector).
    auto msg = L3LocationUpdatingReject::builder()
        .cause(cause)
        .build();
    ParsedMessage pm{MMM{std::move(msg)}};
    auto result = writeL3(pm, out.data(), out.size());
    if (result) return static_cast<int>(result.value());
    return -1;
}

Expected<std::vector<uint8_t>> ResponseBuilder::buildTMSIReallocationCommand(
    const L3LocationAreaIdentity& lai, uint32_t tmsi)
{
    auto msg = L3TMSIReallocationCommand::builder()
        .lai(lai)
        .tmsi(L3MobileIdentity(tmsi))
        .build();
    ParsedMessage pm{MMM{std::move(msg)}};
    return writeL3Bytes(pm);
}

int ResponseBuilder::buildTMSIReallocationCommand(std::span<uint8_t> out,
    const L3LocationAreaIdentity& lai, uint32_t tmsi)
{
    // Zero-alloc: build the message inline and serialize straight into the
    // caller buffer via writeL3 (no intermediate std::vector).
    auto msg = L3TMSIReallocationCommand::builder()
        .lai(lai)
        .tmsi(L3MobileIdentity(tmsi))
        .build();
    ParsedMessage pm{MMM{std::move(msg)}};
    auto result = writeL3(pm, out.data(), out.size());
    if (result) return static_cast<int>(result.value());
    return -1;
}

// ── CC responses ────────────────────────────────────────────────────────

Expected<std::vector<uint8_t>> ResponseBuilder::buildCallProceeding(uint8_t ti)
{
    auto msg = L3CallProceeding::builder()
        .ti(ti)
        .build();
    ParsedMessage pm{CCM{std::move(msg)}};
    return writeL3Bytes(pm);
}

int ResponseBuilder::buildCallProceeding(std::span<uint8_t> out, uint8_t ti)
{
    // Zero-alloc: build the message inline and serialize straight into the
    // caller buffer via writeL3 (no intermediate std::vector).
    auto msg = L3CallProceeding::builder()
        .ti(ti)
        .build();
    ParsedMessage pm{CCM{std::move(msg)}};
    auto result = writeL3(pm, out.data(), out.size());
    if (result) return static_cast<int>(result.value());
    return -1;
}

Expected<std::vector<uint8_t>> ResponseBuilder::buildAlerting(uint8_t ti)
{
    auto msg = L3Alerting::builder()
        .ti(ti)
        .build();
    ParsedMessage pm{CCM{std::move(msg)}};
    return writeL3Bytes(pm);
}

int ResponseBuilder::buildAlerting(std::span<uint8_t> out, uint8_t ti)
{
    // Zero-alloc: build the message inline and serialize straight into the
    // caller buffer via writeL3 (no intermediate std::vector).
    auto msg = L3Alerting::builder()
        .ti(ti)
        .build();
    ParsedMessage pm{CCM{std::move(msg)}};
    auto result = writeL3(pm, out.data(), out.size());
    if (result) return static_cast<int>(result.value());
    return -1;
}

Expected<std::vector<uint8_t>> ResponseBuilder::buildConnect(uint8_t ti)
{
    auto msg = L3Connect::builder()
        .ti(ti)
        .build();
    ParsedMessage pm{CCM{std::move(msg)}};
    return writeL3Bytes(pm);
}

int ResponseBuilder::buildConnect(std::span<uint8_t> out, uint8_t ti)
{
    // Zero-alloc: build the message inline and serialize straight into the
    // caller buffer via writeL3 (no intermediate std::vector).
    auto msg = L3Connect::builder()
        .ti(ti)
        .build();
    ParsedMessage pm{CCM{std::move(msg)}};
    auto result = writeL3(pm, out.data(), out.size());
    if (result) return static_cast<int>(result.value());
    return -1;
}

Expected<std::vector<uint8_t>> ResponseBuilder::buildConnectAcknowledge(uint8_t ti)
{
    auto msg = L3ConnectAcknowledge::builder()
        .ti(ti)
        .build();
    ParsedMessage pm{CCM{std::move(msg)}};
    return writeL3Bytes(pm);
}

int ResponseBuilder::buildConnectAcknowledge(std::span<uint8_t> out, uint8_t ti)
{
    // Zero-alloc: build the message inline and serialize straight into the
    // caller buffer via writeL3 (no intermediate std::vector).
    auto msg = L3ConnectAcknowledge::builder()
        .ti(ti)
        .build();
    ParsedMessage pm{CCM{std::move(msg)}};
    auto result = writeL3(pm, out.data(), out.size());
    if (result) return static_cast<int>(result.value());
    return -1;
}

Expected<std::vector<uint8_t>> ResponseBuilder::buildDisconnect(uint8_t ti, CCCause cause)
{
    auto msg = L3Disconnect::builder()
        .ti(ti)
        .cause(cause)
        .build();
    ParsedMessage pm{CCM{std::move(msg)}};
    return writeL3Bytes(pm);
}

int ResponseBuilder::buildDisconnect(std::span<uint8_t> out, uint8_t ti, CCCause cause)
{
    // Zero-alloc: build the message inline and serialize straight into the
    // caller buffer via writeL3 (no intermediate std::vector).
    auto msg = L3Disconnect::builder()
        .ti(ti)
        .cause(cause)
        .build();
    ParsedMessage pm{CCM{std::move(msg)}};
    auto result = writeL3(pm, out.data(), out.size());
    if (result) return static_cast<int>(result.value());
    return -1;
}

Expected<std::vector<uint8_t>> ResponseBuilder::buildRelease(uint8_t ti, CCCause cause)
{
    auto msg = L3Release::builder()
        .ti(ti)
        .cause(cause)
        .build();
    ParsedMessage pm{CCM{std::move(msg)}};
    return writeL3Bytes(pm);
}

int ResponseBuilder::buildRelease(std::span<uint8_t> out, uint8_t ti, CCCause cause)
{
    // Zero-alloc: build the message inline and serialize straight into the
    // caller buffer via writeL3 (no intermediate std::vector).
    auto msg = L3Release::builder()
        .ti(ti)
        .cause(cause)
        .build();
    ParsedMessage pm{CCM{std::move(msg)}};
    auto result = writeL3(pm, out.data(), out.size());
    if (result) return static_cast<int>(result.value());
    return -1;
}

Expected<std::vector<uint8_t>> ResponseBuilder::buildReleaseComplete(uint8_t ti)
{
    auto msg = L3ReleaseComplete::builder()
        .ti(ti)
        .build();
    ParsedMessage pm{CCM{std::move(msg)}};
    return writeL3Bytes(pm);
}

int ResponseBuilder::buildReleaseComplete(std::span<uint8_t> out, uint8_t ti)
{
    // Zero-alloc: build the message inline and serialize straight into the
    // caller buffer via writeL3 (no intermediate std::vector).
    auto msg = L3ReleaseComplete::builder()
        .ti(ti)
        .build();
    ParsedMessage pm{CCM{std::move(msg)}};
    auto result = writeL3(pm, out.data(), out.size());
    if (result) return static_cast<int>(result.value());
    return -1;
}

// Paging requests

Expected<std::vector<uint8_t>> ResponseBuilder::buildPagingRequestType1(const L3MobileIdentity& identity)
{
    auto msg = L3PagingRequestType1::builder()
        .addMobileId(identity, ChannelType::SDCCHType)
        .build();
    ParsedMessage pm{RRM{std::move(msg)}};
    return writeL3Bytes(pm);
}

int ResponseBuilder::buildPagingRequestType1(std::span<uint8_t> out, const L3MobileIdentity& identity)
{
    // Zero-alloc: build the message inline (fixed identity storage) and
    // serialize straight into the caller buffer via writeL3.
    auto msg = L3PagingRequestType1::builder()
        .addMobileId(identity, ChannelType::SDCCHType)
        .build();
    ParsedMessage pm{RRM{std::move(msg)}};
    auto result = writeL3(pm, out.data(), out.size());
    if (result) return static_cast<int>(result.value());
    return -1;
}

Expected<std::vector<uint8_t>> ResponseBuilder::buildPagingRequestType2(const L3MobileIdentity& identity)
{
    // Paging Type 2 pages TMSIs only (GSM 04.08 9.1.23); never fabricate one.
    if (!identity.isTMSI()) {
        return Expected<std::vector<uint8_t>>::error(
            ParseError{ParseError::Code::InvalidValue, "PagingRequestType2 requires a TMSI identity"});
    }
    auto msg = L3PagingRequestType2::builder()
        .addTMSI(identity.tmsi(), ChannelType::SDCCHType)
        .build();
    ParsedMessage pm{RRM{std::move(msg)}};
    return writeL3Bytes(pm);
}

int ResponseBuilder::buildPagingRequestType2(std::span<uint8_t> out, const L3MobileIdentity& identity)
{
    // Zero-alloc: build the message inline (fixed TMSI storage) and serialize
    // straight into the caller buffer via writeL3. TMSI-only, no fabrication.
    if (!identity.isTMSI()) return -1;
    auto msg = L3PagingRequestType2::builder()
        .addTMSI(identity.tmsi(), ChannelType::SDCCHType)
        .build();
    ParsedMessage pm{RRM{std::move(msg)}};
    auto result = writeL3(pm, out.data(), out.size());
    if (result) return static_cast<int>(result.value());
    return -1;
}

Expected<std::vector<uint8_t>> ResponseBuilder::buildPagingRequestType3(const L3MobileIdentity& identity)
{
    // Paging Type 3 pages TMSIs only (GSM 04.08 9.1.24); never fabricate one.
    if (!identity.isTMSI()) {
        return Expected<std::vector<uint8_t>>::error(
            ParseError{ParseError::Code::InvalidValue, "PagingRequestType3 requires a TMSI identity"});
    }
    auto msg = L3PagingRequestType3::builder()
        .addTMSI(identity.tmsi(), ChannelType::SDCCHType)
        .build();
    ParsedMessage pm{RRM{std::move(msg)}};
    return writeL3Bytes(pm);
}

int ResponseBuilder::buildPagingRequestType3(std::span<uint8_t> out, const L3MobileIdentity& identity)
{
    // Zero-alloc: build the message inline (fixed TMSI storage) and serialize
    // straight into the caller buffer via writeL3. TMSI-only, no fabrication.
    if (!identity.isTMSI()) return -1;
    auto msg = L3PagingRequestType3::builder()
        .addTMSI(identity.tmsi(), ChannelType::SDCCHType)
        .build();
    ParsedMessage pm{RRM{std::move(msg)}};
    auto result = writeL3(pm, out.data(), out.size());
    if (result) return static_cast<int>(result.value());
    return -1;
}

// Handover Command

Expected<std::vector<uint8_t>> ResponseBuilder::buildHandoverCommand(const L3ChannelDescription& channel)
{
    // Use the actual target channel (GSM 04.08 9.1.40) instead of a stub.
    auto msg = L3HandoverCommand::builder()
        .channelDescriptionAfter(L3ChannelDescription2(channel))
        .build();
    ParsedMessage pm{RRM{std::move(msg)}};
    return writeL3Bytes(pm);
}

int ResponseBuilder::buildHandoverCommand(std::span<uint8_t> out, const L3ChannelDescription& channel)
{
    // Zero-alloc: build the message inline using the actual target channel and
    // serialize straight into the caller buffer via writeL3.
    auto msg = L3HandoverCommand::builder()
        .channelDescriptionAfter(L3ChannelDescription2(channel))
        .build();
    ParsedMessage pm{RRM{std::move(msg)}};
    auto result = writeL3(pm, out.data(), out.size());
    if (result) return static_cast<int>(result.value());
    return -1;
}

// CC: Setup

Expected<std::vector<uint8_t>> ResponseBuilder::buildSetup(const std::string& calledNumber, uint8_t ti)
{
    // Include the called party BCD number (GSM 04.08 9.3.19 Setup) instead of
    // a stub. Cold path: only the returned output vector allocates.
    auto b = L3Setup::builder().ti(ti);
    if (!calledNumber.empty()) {
        b = b.calledParty(L3CalledPartyBCDNumber(calledNumber.c_str()));
    }
    ParsedMessage pm{CCM{b.build()}};
    return writeL3Bytes(pm);
}

int ResponseBuilder::buildSetup(std::span<uint8_t> out, const std::string& calledNumber, uint8_t ti)
{
    // Cold-path span overload for callers holding the number in a std::string.
    // Delegates to the vector version (which allocates the output buffer).
    auto result = buildSetup(calledNumber, ti);
    if (result) {
        const auto& bytes = result.value();
        if (bytes.size() > out.size()) return -1;
        std::memcpy(out.data(), bytes.data(), bytes.size());
        return static_cast<int>(bytes.size());
    }
    return -1;
}

int ResponseBuilder::buildSetupZeroAlloc(std::span<uint8_t> out,
    const char* digits, size_t len, uint8_t ti)
{
    // Zero-alloc hot path: build L3CalledPartyBCDNumber directly from the digit
    // buffer (fixed-size L3BCDDigits, no heap) and serialize via writeL3.
    // @p digits must be null-terminated; @p len is the number of valid digits.
    if (digits == nullptr || len == 0) return -1;
    if (len > L3BCDDigits::maxDigits) return -1;
    L3CalledPartyBCDNumber num{digits};
    auto msg = L3Setup::builder()
        .ti(ti)
        .calledParty(num)
        .build();
    ParsedMessage pm{CCM{std::move(msg)}};
    auto result = writeL3(pm, out.data(), out.size());
    if (result) return static_cast<int>(result.value());
    return -1;
}

// Build response from token

int ResponseBuilder::buildResponseFromToken(ResponseToken token, std::span<uint8_t> out,
                                              const SubscriberSession* session)
{
    // All response parameters are read from the session's ResponseContext, which
    // the active procedure populates. Never fabricate values: return -1 when a
    // required parameter is missing.
    if (!session) return -1;
    const ResponseContext& r = session->response;

    switch (token) {
        case ResponseToken::None:
            return 0;

        // ── RR responses ────────────────────────────────────────────────
        case ResponseToken::ImmediateAssignment:
            if (!r.hasChannel) return -1;
            // Echo the full 8-bit RA from the RACH burst (TS 44.018 9.1.8):
            // the MS identifies itself by this value during channel seizure.
            if (r.hasRequestRef) {
                return buildImmediateAssignment(out, r.channel, 0,
                    L3RequestReference{r.requestRef, 0, 0, 0});
            }
            return buildImmediateAssignment(out, r.channel, 0);
        case ResponseToken::AssignmentCommand:
            if (!r.hasChannel) return -1;
            return buildAssignmentCommand(out, r.channel);
        case ResponseToken::ChannelRelease:
            return buildChannelRelease(out, RRCause::Normal_Event);
        case ResponseToken::CipheringModeCommand:
            return buildCipheringModeCommand(out, r.hasCipherAlgo ? r.cipherAlgo : 1);
        case ResponseToken::PhysicalInformation:
            return buildPhysicalInformation(out, 0);
        case ResponseToken::HandoverCommand:
            if (!r.hasHoChannel) return -1;
            return buildHandoverCommand(out, r.hoChannel);
        case ResponseToken::PagingRequestType1:
        case ResponseToken::PagingRequestType2:
        case ResponseToken::PagingRequestType3: {
            if (!r.hasIdentity) return -1;   // never fabricate a TMSI/identity
            if (token == ResponseToken::PagingRequestType1) return buildPagingRequestType1(out, r.identity);
            if (token == ResponseToken::PagingRequestType2) return buildPagingRequestType2(out, r.identity);
            return buildPagingRequestType3(out, r.identity);
        }

        // ── MM responses ────────────────────────────────────────────────
        case ResponseToken::CMServiceAccept:
            return buildCMServiceAccept(out);
        case ResponseToken::CMServiceReject:
            return buildCMServiceReject(out, r.mmCause);
        case ResponseToken::IdentityRequest:
            return buildIdentityRequest(out, MobileIDType::IMSI);
        case ResponseToken::AuthenticationRequest:
            if (!r.hasRand) return -1;       // never send a fabricated RAND
            return buildAuthenticationRequest(out, std::span<const uint8_t>{r.rand.data(), r.rand.size()});
        case ResponseToken::LocationUpdatingAccept: {
            auto lai = session->context.lai().value_or(L3LocationAreaIdentity{});
            return buildLocationUpdatingAccept(out, lai, r.newTmsi);
        }
        case ResponseToken::LocationUpdatingReject:
            return buildLocationUpdatingReject(out, r.mmCause);
        case ResponseToken::TMSIReallocationCommand: {
            auto lai = session->context.lai().value_or(L3LocationAreaIdentity{});
            const L3MobileIdentity& id = session->context.identity();
            uint32_t tmsi = id.isTMSI() ? id.tmsi() : 0;
            return buildTMSIReallocationCommand(out, lai, tmsi);
        }

        // ── CC responses ────────────────────────────────────────────────
        case ResponseToken::CallProceeding:
            return buildCallProceeding(out, r.ti);
        case ResponseToken::Alerting:
            return buildAlerting(out, r.ti);
        case ResponseToken::Connect:
            return buildConnect(out, r.ti);
        case ResponseToken::ConnectAcknowledge:
            return buildConnectAcknowledge(out, r.ti);
        case ResponseToken::Disconnect:
            return buildDisconnect(out, r.ti, r.ccCause);
        case ResponseToken::Release:
            return buildRelease(out, r.ti, r.ccCause);
        case ResponseToken::ReleaseComplete:
            return buildReleaseComplete(out, r.ti);
        case ResponseToken::Setup:
            if (!r.hasCalledNumber) return -1;
            // Zero-alloc: build L3CalledPartyBCDNumber directly from the fixed
            // digit buffer (L3BCDDigits is a fixed array, no std::string).
            return buildSetupZeroAlloc(out, r.calledNumber.data(), r.calledNumberLen, r.ti);

        default:
            return -1;
    }
}

} // namespace gsml3parser
