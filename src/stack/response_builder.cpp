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
    auto result = buildImmediateAssignment(channel, timingAdvance, requestRef);
    if (result) {
        const auto& bytes = result.value();
        if (bytes.size() > out.size()) return -1;
        std::memcpy(out.data(), bytes.data(), bytes.size());
        return static_cast<int>(bytes.size());
    }
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
    auto result = buildAssignmentCommand(channel, mode);
    if (result) {
        const auto& bytes = result.value();
        if (bytes.size() > out.size()) return -1;
        std::memcpy(out.data(), bytes.data(), bytes.size());
        return static_cast<int>(bytes.size());
    }
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
    auto result = buildChannelRelease(cause);
    if (result) {
        const auto& bytes = result.value();
        if (bytes.size() > out.size()) return -1;
        std::memcpy(out.data(), bytes.data(), bytes.size());
        return static_cast<int>(bytes.size());
    }
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
    auto result = buildCipheringModeCommand(cipherAlgo);
    if (result) {
        const auto& bytes = result.value();
        if (bytes.size() > out.size()) return -1;
        std::memcpy(out.data(), bytes.data(), bytes.size());
        return static_cast<int>(bytes.size());
    }
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
    auto result = buildPhysicalInformation(timingAdvance);
    if (result) {
        const auto& bytes = result.value();
        if (bytes.size() > out.size()) return -1;
        std::memcpy(out.data(), bytes.data(), bytes.size());
        return static_cast<int>(bytes.size());
    }
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
    auto result = buildCMServiceAccept();
    if (result) {
        const auto& bytes = result.value();
        if (bytes.size() > out.size()) return -1;
        std::memcpy(out.data(), bytes.data(), bytes.size());
        return static_cast<int>(bytes.size());
    }
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
    auto result = buildCMServiceReject(cause);
    if (result) {
        const auto& bytes = result.value();
        if (bytes.size() > out.size()) return -1;
        std::memcpy(out.data(), bytes.data(), bytes.size());
        return static_cast<int>(bytes.size());
    }
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
    auto result = buildIdentityRequest(type);
    if (result) {
        const auto& bytes = result.value();
        if (bytes.size() > out.size()) return -1;
        std::memcpy(out.data(), bytes.data(), bytes.size());
        return static_cast<int>(bytes.size());
    }
    return -1;
}

Expected<std::vector<uint8_t>> ResponseBuilder::buildAuthenticationRequest(
    std::span<const uint8_t> rand)
{
    std::vector<uint8_t> randVec(rand.begin(), rand.end());
    auto msg = L3AuthenticationRequest::builder()
        .rand(std::move(randVec))
        .build();
    ParsedMessage pm{MMM{std::move(msg)}};
    return writeL3Bytes(pm);
}

int ResponseBuilder::buildAuthenticationRequest(std::span<uint8_t> out,
    std::span<const uint8_t> rand)
{
    auto result = buildAuthenticationRequest(rand);
    if (result) {
        const auto& bytes = result.value();
        if (bytes.size() > out.size()) return -1;
        std::memcpy(out.data(), bytes.data(), bytes.size());
        return static_cast<int>(bytes.size());
    }
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
    auto result = buildLocationUpdatingAccept(lai, newTmsi);
    if (result) {
        const auto& bytes = result.value();
        if (bytes.size() > out.size()) return -1;
        std::memcpy(out.data(), bytes.data(), bytes.size());
        return static_cast<int>(bytes.size());
    }
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
    auto result = buildLocationUpdatingReject(cause);
    if (result) {
        const auto& bytes = result.value();
        if (bytes.size() > out.size()) return -1;
        std::memcpy(out.data(), bytes.data(), bytes.size());
        return static_cast<int>(bytes.size());
    }
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
    auto result = buildTMSIReallocationCommand(lai, tmsi);
    if (result) {
        const auto& bytes = result.value();
        if (bytes.size() > out.size()) return -1;
        std::memcpy(out.data(), bytes.data(), bytes.size());
        return static_cast<int>(bytes.size());
    }
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
    auto result = buildCallProceeding(ti);
    if (result) {
        const auto& bytes = result.value();
        if (bytes.size() > out.size()) return -1;
        std::memcpy(out.data(), bytes.data(), bytes.size());
        return static_cast<int>(bytes.size());
    }
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
    auto result = buildAlerting(ti);
    if (result) {
        const auto& bytes = result.value();
        if (bytes.size() > out.size()) return -1;
        std::memcpy(out.data(), bytes.data(), bytes.size());
        return static_cast<int>(bytes.size());
    }
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
    auto result = buildConnect(ti);
    if (result) {
        const auto& bytes = result.value();
        if (bytes.size() > out.size()) return -1;
        std::memcpy(out.data(), bytes.data(), bytes.size());
        return static_cast<int>(bytes.size());
    }
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
    auto result = buildConnectAcknowledge(ti);
    if (result) {
        const auto& bytes = result.value();
        if (bytes.size() > out.size()) return -1;
        std::memcpy(out.data(), bytes.data(), bytes.size());
        return static_cast<int>(bytes.size());
    }
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
    auto result = buildDisconnect(ti, cause);
    if (result) {
        const auto& bytes = result.value();
        if (bytes.size() > out.size()) return -1;
        std::memcpy(out.data(), bytes.data(), bytes.size());
        return static_cast<int>(bytes.size());
    }
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
    auto result = buildRelease(ti, cause);
    if (result) {
        const auto& bytes = result.value();
        if (bytes.size() > out.size()) return -1;
        std::memcpy(out.data(), bytes.data(), bytes.size());
        return static_cast<int>(bytes.size());
    }
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
    auto result = buildReleaseComplete(ti);
    if (result) {
        const auto& bytes = result.value();
        if (bytes.size() > out.size()) return -1;
        std::memcpy(out.data(), bytes.data(), bytes.size());
        return static_cast<int>(bytes.size());
    }
    return -1;
}

} // namespace gsml3parser
