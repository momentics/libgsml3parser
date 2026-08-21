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

/// Factory for building L3 response messages.
///
/// Used by ProcedureRunner (not FSM directly) to generate response messages
/// after the FSM returns a SendResponse action. Provides both vector-returning
/// overloads for simple scenarios and span-writing overloads for zero-heap-allocation
/// high-throughput paths.
///
/// 3GPP specifications: TS 04.08 (message formats), TS 24.008 (procedure context).
/// Thread safety: all methods are stateless and safe for concurrent use.
/// Memory: vector overloads allocate one std::vector per call; span overloads write
/// directly into caller-provided buffers (e.g. Arena-allocated) with zero heap cost.
///
/// Example:
/// @code
///   // Simple path:
///   auto bytes = ResponseBuilder::buildCMServiceAccept();
///   if (bytes) sendToMS(bytes->data(), bytes->size());
///
///   // Zero-allocation path:
///   Arena arena(65536);
///   auto* buf = static_cast<uint8_t*>(arena.allocate(512));
///   int n = ResponseBuilder::buildCMServiceAccept({buf, 512});
///   if (n > 0) sendToMS(buf, n);
/// @endcode
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "gsml3parser/expected.h"
#include "gsml3parser/types.h"
#include "gsml3parser/enums.h"
#include "gsml3parser/common/l3common.h"
#include "gsml3parser/stack/procedure.h"

namespace gsml3parser {

class SubscriberSession;

/// Factory for building L3 response messages. All methods are stateless static functions.
///
/// Each method has two overloads:
///   - Expected<std::vector<uint8_t>> version: allocates one vector, convenient for tests
///   - int version with std::span<uint8_t>: writes into pre-allocated buffer, zero heap alloc
///
/// The span overload returns the number of bytes written, or -1 on error (buffer too small).
class ResponseBuilder {
public:
    // ── RR responses ────────────────────────────────────────────────────────

    /// Build ImmediateAssignment message.
    /// @param channel Target channel descriptor.
    /// @param timingAdvance TA value (0-63).
    /// @param requestRef Optional L3RequestReference from Channel Request.
    /// @return Serialized L3 bytes or parse error.
    /// 3GPP TS 04.08 9.1.19 - Immediate Assignment.
    [[nodiscard]] static Expected<std::vector<uint8_t>> buildImmediateAssignment(
        const L3ChannelDescription& channel, uint8_t timingAdvance,
        std::optional<L3RequestReference> requestRef = std::nullopt);

    /// Build ImmediateAssignment into pre-allocated buffer (zero heap alloc).
    /// @param out Pre-allocated output buffer (caller provides via Arena).
    /// @param channel Target channel descriptor.
    /// @param timingAdvance TA value (0-63).
    /// @param requestRef Optional L3RequestReference from Channel Request.
    /// @return Number of bytes written, or -1 on error (buffer too small).
    [[nodiscard]] static int buildImmediateAssignment(
        std::span<uint8_t> out,
        const L3ChannelDescription& channel, uint8_t timingAdvance,
        std::optional<L3RequestReference> requestRef = std::nullopt);

    /// Build AssignmentCommand message.
    /// @param channel Target channel descriptor.
    /// @param mode Optional channel mode (signalling/speech/data).
    /// @return Serialized L3 bytes or parse error.
    /// 3GPP TS 04.08 9.1.2 - Assignment Command.
    [[nodiscard]] static Expected<std::vector<uint8_t>> buildAssignmentCommand(
        const L3ChannelDescription& channel, L3ChannelMode mode = L3ChannelMode{});

    /// Build AssignmentCommand into pre-allocated buffer (zero heap alloc).
    /// @param out Pre-allocated output buffer.
    /// @param channel Target channel descriptor.
    /// @param mode Optional channel mode.
    /// @return Number of bytes written, or -1 on error.
    [[nodiscard]] static int buildAssignmentCommand(
        std::span<uint8_t> out,
        const L3ChannelDescription& channel, L3ChannelMode mode = L3ChannelMode{});

    /// Build ChannelRelease message.
    /// @param cause RR cause (e.g. RRCause::Normal_Event).
    /// @return Serialized L3 bytes or parse error.
    /// 3GPP TS 04.08 9.1.7 - Channel Release.
    [[nodiscard]] static Expected<std::vector<uint8_t>> buildChannelRelease(
        RRCause cause = RRCause::Normal_Event);

    /// Build ChannelRelease into pre-allocated buffer (zero heap alloc).
    /// @param out Pre-allocated output buffer.
    /// @param cause RR cause.
    /// @return Number of bytes written, or -1 on error.
    [[nodiscard]] static int buildChannelRelease(std::span<uint8_t> out,
        RRCause cause = RRCause::Normal_Event);

    /// Build CipheringModeCommand message.
    /// @param cipherAlgo Algorithm identifier (0=A5/0, 1=A5/1, etc.).
    /// @return Serialized L3 bytes or parse error.
    /// 3GPP TS 04.08 9.1.9 - Ciphering Mode Command.
    [[nodiscard]] static Expected<std::vector<uint8_t>> buildCipheringModeCommand(
        uint8_t cipherAlgo);

    /// Build CipheringModeCommand into pre-allocated buffer (zero heap alloc).
    /// @param out Pre-allocated output buffer.
    /// @param cipherAlgo Algorithm identifier.
    /// @return Number of bytes written, or -1 on error.
    [[nodiscard]] static int buildCipheringModeCommand(std::span<uint8_t> out,
        uint8_t cipherAlgo);

    /// Build PhysicalInformation message.
    /// @param timingAdvance TA value (0-63).
    /// @return Serialized L3 bytes or parse error.
    /// 3GPP TS 04.08 9.1.12 - Physical Information.
    [[nodiscard]] static Expected<std::vector<uint8_t>> buildPhysicalInformation(
        uint8_t timingAdvance);

    /// Build PhysicalInformation into pre-allocated buffer (zero heap alloc).
    /// @param out Pre-allocated output buffer.
    /// @param timingAdvance TA value.
    /// @return Number of bytes written, or -1 on error.
    [[nodiscard]] static int buildPhysicalInformation(std::span<uint8_t> out,
        uint8_t timingAdvance);

    // ── MM responses ────────────────────────────────────────────────────────

    /// Build CM Service Accept message.
    /// @return Serialized L3 bytes or parse error.
    /// 3GPP TS 04.08 9.2.5 - CM Service Accept.
    [[nodiscard]] static Expected<std::vector<uint8_t>> buildCMServiceAccept();

    /// Build CM Service Accept into pre-allocated buffer (zero heap alloc).
    /// @param out Pre-allocated output buffer.
    /// @return Number of bytes written, or -1 on error.
    [[nodiscard]] static int buildCMServiceAccept(std::span<uint8_t> out);

    /// Build CM Service Reject message.
    /// @param cause MM reject cause (e.g. MMRejectCause::Congestion).
    /// @return Serialized L3 bytes or parse error.
    /// 3GPP TS 04.08 9.2.6 - CM Service Reject.
    [[nodiscard]] static Expected<std::vector<uint8_t>> buildCMServiceReject(
        MMRejectCause cause = MMRejectCause::Zero);

    /// Build CM Service Reject into pre-allocated buffer (zero heap alloc).
    /// @param out Pre-allocated output buffer.
    /// @param cause MM reject cause.
    /// @return Number of bytes written, or -1 on error.
    [[nodiscard]] static int buildCMServiceReject(std::span<uint8_t> out,
        MMRejectCause cause = MMRejectCause::Zero);

    /// Build IdentityRequest message.
    /// @param type MobileIDType (IMSI=1, IMEI=2, IMEISV=3, TMSI=4).
    /// @return Serialized L3 bytes or parse error.
    /// 3GPP TS 04.08 9.2.10 - Identity Request.
    [[nodiscard]] static Expected<std::vector<uint8_t>> buildIdentityRequest(
        MobileIDType type);

    /// Build IdentityRequest into pre-allocated buffer (zero heap alloc).
    /// @param out Pre-allocated output buffer.
    /// @param type MobileIDType.
    /// @return Number of bytes written, or -1 on error.
    [[nodiscard]] static int buildIdentityRequest(std::span<uint8_t> out,
        MobileIDType type);

    /// Build AuthenticationRequest message.
    /// @param rand 32-byte RAND from AuC (actual GSM RAND is 16 bytes / 128 bits).
    /// @return Serialized L3 bytes or parse error.
    /// 3GPP TS 04.08 9.2.2 - Authentication Request.
    [[nodiscard]] static Expected<std::vector<uint8_t>> buildAuthenticationRequest(
        std::span<const uint8_t> rand);

    /// Build AuthenticationRequest into pre-allocated buffer (zero heap alloc).
    /// @param out Pre-allocated output buffer.
    /// @param rand RAND bytes from AuC.
    /// @return Number of bytes written, or -1 on error.
    [[nodiscard]] static int buildAuthenticationRequest(std::span<uint8_t> out,
        std::span<const uint8_t> rand);

    /// Build LocationUpdatingAccept message.
    /// @param lai Location Area Identity to include.
    /// @param newTmsi Optional new TMSI assignment (sent as MobileIdentity).
    /// @return Serialized L3 bytes or parse error.
    /// 3GPP TS 04.08 9.2.13 - Location Updating Accept.
    [[nodiscard]] static Expected<std::vector<uint8_t>> buildLocationUpdatingAccept(
        const L3LocationAreaIdentity& lai,
        std::optional<uint32_t> newTmsi = std::nullopt);

    /// Build LocationUpdatingAccept into pre-allocated buffer (zero heap alloc).
    /// @param out Pre-allocated output buffer.
    /// @param lai Location Area Identity.
    /// @param newTmsi Optional new TMSI.
    /// @return Number of bytes written, or -1 on error.
    [[nodiscard]] static int buildLocationUpdatingAccept(std::span<uint8_t> out,
        const L3LocationAreaIdentity& lai,
        std::optional<uint32_t> newTmsi = std::nullopt);

    /// Build LocationUpdatingReject message.
    /// @param cause MM reject cause.
    /// @return Serialized L3 bytes or parse error.
    /// 3GPP TS 04.08 9.2.14 - Location Updating Reject.
    [[nodiscard]] static Expected<std::vector<uint8_t>> buildLocationUpdatingReject(
        MMRejectCause cause);

    /// Build LocationUpdatingReject into pre-allocated buffer (zero heap alloc).
    /// @param out Pre-allocated output buffer.
    /// @param cause MM reject cause.
    /// @return Number of bytes written, or -1 on error.
    [[nodiscard]] static int buildLocationUpdatingReject(std::span<uint8_t> out,
        MMRejectCause cause);

    /// Build TMSI Reallocation Command.
    /// @param lai Location Area Identity.
    /// @param tmsi New 32-bit TMSI value.
    /// @return Serialized L3 bytes or parse error.
    /// 3GPP TS 04.08 9.2.17 - TMSI Reallocation Command.
    [[nodiscard]] static Expected<std::vector<uint8_t>> buildTMSIReallocationCommand(
        const L3LocationAreaIdentity& lai, uint32_t tmsi);

    /// Build TMSI Reallocation Command into pre-allocated buffer (zero heap alloc).
    /// @param out Pre-allocated output buffer.
    /// @param lai Location Area Identity.
    /// @param tmsi New 32-bit TMSI value.
    /// @return Number of bytes written, or -1 on error.
    [[nodiscard]] static int buildTMSIReallocationCommand(std::span<uint8_t> out,
        const L3LocationAreaIdentity& lai, uint32_t tmsi);

    // ── CC responses ────────────────────────────────────────────────────────

    /// Build CallProceeding message.
    /// @param ti Transaction Identifier (0-7).
    /// @return Serialized L3 bytes or parse error.
    /// 3GPP TS 04.08 9.3.3 - Call Proceeding.
    [[nodiscard]] static Expected<std::vector<uint8_t>> buildCallProceeding(
        uint8_t ti);

    /// Build CallProceeding into pre-allocated buffer (zero heap alloc).
    /// @param out Pre-allocated output buffer.
    /// @param ti Transaction Identifier.
    /// @return Number of bytes written, or -1 on error.
    [[nodiscard]] static int buildCallProceeding(std::span<uint8_t> out, uint8_t ti);

    /// Build Alerting message.
    /// @param ti Transaction Identifier (0-7).
    /// @return Serialized L3 bytes or parse error.
    /// 3GPP TS 04.08 9.3.1 - Alerting.
    [[nodiscard]] static Expected<std::vector<uint8_t>> buildAlerting(uint8_t ti);

    /// Build Alerting into pre-allocated buffer (zero heap alloc).
    /// @param out Pre-allocated output buffer.
    /// @param ti Transaction Identifier.
    /// @return Number of bytes written, or -1 on error.
    [[nodiscard]] static int buildAlerting(std::span<uint8_t> out, uint8_t ti);

    /// Build Connect message.
    /// @param ti Transaction Identifier (0-7).
    /// @return Serialized L3 bytes or parse error.
    /// 3GPP TS 04.08 9.3.5 - Connect.
    [[nodiscard]] static Expected<std::vector<uint8_t>> buildConnect(uint8_t ti);

    /// Build Connect into pre-allocated buffer (zero heap alloc).
    /// @param out Pre-allocated output buffer.
    /// @param ti Transaction Identifier.
    /// @return Number of bytes written, or -1 on error.
    [[nodiscard]] static int buildConnect(std::span<uint8_t> out, uint8_t ti);

    /// Build ConnectAcknowledge message.
    /// @param ti Transaction Identifier (0-7).
    /// @return Serialized L3 bytes or parse error.
    /// 3GPP TS 04.08 9.3.6 - Connect Acknowledge.
    [[nodiscard]] static Expected<std::vector<uint8_t>> buildConnectAcknowledge(
        uint8_t ti);

    /// Build ConnectAcknowledge into pre-allocated buffer (zero heap alloc).
    /// @param out Pre-allocated output buffer.
    /// @param ti Transaction Identifier.
    /// @return Number of bytes written, or -1 on error.
    [[nodiscard]] static int buildConnectAcknowledge(std::span<uint8_t> out, uint8_t ti);

    /// Build Disconnect message.
    /// @param ti Transaction Identifier (0-7).
    /// @param cause CC cause (e.g. CCCause::Normal_Call_Clearing).
    /// @return Serialized L3 bytes or parse error.
    /// 3GPP TS 04.08 9.3.7 - Disconnect.
    [[nodiscard]] static Expected<std::vector<uint8_t>> buildDisconnect(
        uint8_t ti, CCCause cause);

    /// Build Disconnect into pre-allocated buffer (zero heap alloc).
    /// @param out Pre-allocated output buffer.
    /// @param ti Transaction Identifier.
    /// @param cause CC cause.
    /// @return Number of bytes written, or -1 on error.
    [[nodiscard]] static int buildDisconnect(std::span<uint8_t> out, uint8_t ti,
        CCCause cause);

    /// Build Release message.
    /// @param ti Transaction Identifier (0-7).
    /// @param cause CC cause.
    /// @return Serialized L3 bytes or parse error.
    /// 3GPP TS 04.08 9.3.19 - Release.
    [[nodiscard]] static Expected<std::vector<uint8_t>> buildRelease(
        uint8_t ti, CCCause cause);

    /// Build Release into pre-allocated buffer (zero heap alloc).
    /// @param out Pre-allocated output buffer.
    /// @param ti Transaction Identifier.
    /// @param cause CC cause.
    /// @return Number of bytes written, or -1 on error.
    [[nodiscard]] static int buildRelease(std::span<uint8_t> out, uint8_t ti,
        CCCause cause);

    /// Build ReleaseComplete message.
    /// @param ti Transaction Identifier (0-7).
    /// @return Serialized L3 bytes or parse error.
    /// 3GPP TS 04.08 9.3.19 - Release Complete.
    [[nodiscard]] static Expected<std::vector<uint8_t>> buildReleaseComplete(
        uint8_t ti);

    /// Build ReleaseComplete into pre-allocated buffer (zero heap alloc).
    /// @param out Pre-allocated output buffer.
    /// @param ti Transaction Identifier.
    /// @return Number of bytes written, or -1 on error.
    [[nodiscard]] static int buildReleaseComplete(std::span<uint8_t> out, uint8_t ti);

    // RR: Paging requests (TS 04.08 9.1.25)
    [[nodiscard]] static Expected<std::vector<uint8_t>> buildPagingRequestType1(const L3MobileIdentity& identity);
    [[nodiscard]] static int buildPagingRequestType1(std::span<uint8_t> out, const L3MobileIdentity& identity);

    [[nodiscard]] static Expected<std::vector<uint8_t>> buildPagingRequestType2(const L3MobileIdentity& identity);
    [[nodiscard]] static int buildPagingRequestType2(std::span<uint8_t> out, const L3MobileIdentity& identity);

    [[nodiscard]] static Expected<std::vector<uint8_t>> buildPagingRequestType3(const L3MobileIdentity& identity);
    [[nodiscard]] static int buildPagingRequestType3(std::span<uint8_t> out, const L3MobileIdentity& identity);

    // RR: Handover Command (TS 04.08 9.1.40)
    [[nodiscard]] static Expected<std::vector<uint8_t>> buildHandoverCommand(const L3ChannelDescription& channel);
    [[nodiscard]] static int buildHandoverCommand(std::span<uint8_t> out, const L3ChannelDescription& channel);

    // CC: Setup (TS 24.008 9.3.2)
    [[nodiscard]] static Expected<std::vector<uint8_t>> buildSetup(const std::string& calledNumber, uint8_t ti);
    [[nodiscard]] static int buildSetup(std::span<uint8_t> out, const std::string& calledNumber, uint8_t ti);

    /// Build Setup directly from a BCD digit buffer (zero heap allocation).
    ///
    /// Hot-path variant of buildSetup() for callers that already hold the called
    /// number in a fixed buffer (e.g. ResponseContext::calledNumber). Avoids the
    /// std::string copy of the cold-path overload.
    /// @param out Pre-allocated output buffer.
    /// @param digits BCD digits in wire order (null-terminated not required).
    /// @param len Number of valid digits in @p digits.
    /// @param ti Transaction Identifier (0-7).
    /// @return Number of bytes written, or -1 on error (buffer too small / len == 0).
    [[nodiscard]] static int buildSetupZeroAlloc(std::span<uint8_t> out,
        const char* digits, size_t len, uint8_t ti);

    // Build response bytes from ResponseToken + session context (zero-heap-allocation path).
    /// All response parameters (RAND, TI, channel, identity, called number, causes) are
    /// read from the session's ResponseContext, which the active procedure populates.
    /// Returns -1 (instead of fabricating values) when a required parameter is missing.
    /// @param token The ResponseToken indicating which message to build.
    /// @param out Pre-allocated output buffer (Arena-provided).
    /// @param session Session context providing the response parameters (required).
    /// @return Number of bytes written, or -1 on error (missing parameter / buffer too small).
    [[nodiscard]] static int buildResponseFromToken(ResponseToken token, std::span<uint8_t> out,
                                                     const SubscriberSession* session);
};

} // namespace gsml3parser
