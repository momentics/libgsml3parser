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

/// A-bis RSL message builder for BTS->BSC communication.
///
/// Constructs serialized RSL messages with proper header and TLV information
/// elements. Every method has both a vector-returning overload (convenient, cold path)
/// and a span overload (zero heap allocation, hot path). The span overload writes
/// directly into a pre-allocated Arena buffer, making it suitable for high-throughput
/// BTS scenarios with millions of messages per second.
///
/// 3GPP specification: TS 48.058 (A-bis interface RSL protocol).
/// Thread safety: all methods are stateless static functions, fully thread-safe.
/// Memory: span overloads perform zero heap allocation; vector overloads allocate once.
///
/// Example:
/// @code
///   Arena arena(65536);
///   auto* buf = arena.allocate(1024);
///   int n = RSLBuilder::buildDataInd({static_cast<uint8_t*>(buf), 1024}, 0x7c, 1, l3Bytes);
///   if (n > 0) sendToBSC(static_cast<const uint8_t*>(buf), n);
/// @endcode
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>
#include "gsml3parser/expected.h"
#include "gsml3parser/abis/rsl_types.h"
#include "gsml3parser/common/l3common.h"

namespace gsml3parser {

/// Builder for A-bis RSL messages sent from BTS to BSC.
/// Each method serializes a complete RSL frame: discriminator byte, message type,
/// channel number, and TLV information elements (including L3 payload wrapper).
class RSLBuilder {
public:
    // ── RLL messages (encapsulate L3 payload) ────────────────────────

    /// Build DATA_REQ message (BSC->BTS direction, for testing/loopback).
    /// @param chanNr RSL channel number byte.
    /// @param linkId LAPDm link identifier.
    /// @param l3Payload L3 message bytes to encapsulate.
    /// @return Serialized RSL frame or ParseError on failure.
    [[nodiscard]] static Expected<std::vector<uint8_t>> buildDataReq(
        uint8_t chanNr, uint8_t linkId, std::span<const uint8_t> l3Payload);

    /// Build DATA_REQ into pre-allocated buffer (zero heap alloc).
    /// @param out Pre-allocated output buffer from Arena.
    /// @param chanNr RSL channel number byte.
    /// @param linkId LAPDm link identifier.
    /// @param l3Payload L3 message bytes to encapsulate.
    /// @return Number of bytes written, or -1 if buffer too small.
    [[nodiscard]] static int buildDataReq(std::span<uint8_t> out,
        uint8_t chanNr, uint8_t linkId, std::span<const uint8_t> l3Payload);

    /// Build DATA_IND message (BTS->BSC: forward L3 from MS to BSC).
    /// @param chanNr RSL channel number byte.
    /// @param linkId LAPDm link identifier.
    /// @param l3Payload L3 message bytes from MS.
    /// @return Serialized RSL frame or ParseError on failure.
    [[nodiscard]] static Expected<std::vector<uint8_t>> buildDataInd(
        uint8_t chanNr, uint8_t linkId, std::span<const uint8_t> l3Payload);

    /// Build DATA_IND into pre-allocated buffer (zero heap alloc).
    /// @param out Pre-allocated output buffer from Arena.
    /// @param chanNr RSL channel number byte.
    /// @param linkId LAPDm link identifier.
    /// @param l3Payload L3 message bytes from MS.
    /// @return Number of bytes written, or -1 if buffer too small.
    [[nodiscard]] static int buildDataInd(std::span<uint8_t> out,
        uint8_t chanNr, uint8_t linkId, std::span<const uint8_t> l3Payload);

    /// Build UNIT_DATA_REQ message (connectionless L3 transfer).
    /// @param chanNr RSL channel number byte.
    /// @param linkId LAPDm link identifier.
    /// @param l3Payload L3 message bytes to encapsulate.
    /// @return Serialized RSL frame or ParseError on failure.
    [[nodiscard]] static Expected<std::vector<uint8_t>> buildUnitDataReq(
        uint8_t chanNr, uint8_t linkId, std::span<const uint8_t> l3Payload);

    /// Build UNIT_DATA_REQ into pre-allocated buffer (zero heap alloc).
    [[nodiscard]] static int buildUnitDataReq(std::span<uint8_t> out,
        uint8_t chanNr, uint8_t linkId, std::span<const uint8_t> l3Payload);

    /// Build UNIT_DATA_IND message (connectionless L3 from MS).
    /// @param chanNr RSL channel number byte.
    /// @param linkId LAPDm link identifier.
    /// @param l3Payload L3 message bytes from MS.
    /// @return Serialized RSL frame or ParseError on failure.
    [[nodiscard]] static Expected<std::vector<uint8_t>> buildUnitDataInd(
        uint8_t chanNr, uint8_t linkId, std::span<const uint8_t> l3Payload);

    /// Build UNIT_DATA_IND into pre-allocated buffer (zero heap alloc).
    [[nodiscard]] static int buildUnitDataInd(std::span<uint8_t> out,
        uint8_t chanNr, uint8_t linkId, std::span<const uint8_t> l3Payload);

    // ── DCHAN messages (BTS -> BSC) ──────────────────────────────────

    /// Build CHAN_ACTIV_ACK message.
    /// @param chanNr Activated channel number.
    /// @param frameNumber FN at which activation took effect.
    /// @return Serialized RSL frame or ParseError on failure.
    [[nodiscard]] static Expected<std::vector<uint8_t>> buildChanActivAck(
        uint8_t chanNr, uint16_t frameNumber);

    /// Build CHAN_ACTIV_ACK into pre-allocated buffer (zero heap alloc).
    [[nodiscard]] static int buildChanActivAck(std::span<uint8_t> out,
        uint8_t chanNr, uint16_t frameNumber);

    /// Build CHAN_ACTIV_NACK message.
    /// @param chanNr Failed channel number.
    /// @param cause Error cause code.
    /// @return Serialized RSL frame or ParseError on failure.
    [[nodiscard]] static Expected<std::vector<uint8_t>> buildChanActivNack(
        uint8_t chanNr, RSLErrorCause cause);

    /// Build CHAN_ACTIV_NACK into pre-allocated buffer (zero heap alloc).
    [[nodiscard]] static int buildChanActivNack(std::span<uint8_t> out,
        uint8_t chanNr, RSLErrorCause cause);

    /// Build RF_CHAN_REL_ACK message.
    /// @param chanNr Released channel number.
    /// @return Serialized RSL frame or ParseError on failure.
    [[nodiscard]] static Expected<std::vector<uint8_t>> buildRFChanRelAck(uint8_t chanNr);

    /// Build RF_CHAN_REL_ACK into pre-allocated buffer (zero heap alloc).
    [[nodiscard]] static int buildRFChanRelAck(std::span<uint8_t> out, uint8_t chanNr);

    /// Build CONN_FAIL message.
    /// @param chanNr Failed channel number.
    /// @param cause Error cause code.
    /// @return Serialized RSL frame or ParseError on failure.
    [[nodiscard]] static Expected<std::vector<uint8_t>> buildConnFail(
        uint8_t chanNr, RSLErrorCause cause);

    /// Build CONN_FAIL into pre-allocated buffer (zero heap alloc).
    [[nodiscard]] static int buildConnFail(std::span<uint8_t> out,
        uint8_t chanNr, RSLErrorCause cause);

    /// Build MEAS_RES message with uplink measurement results.
    /// @param chanNr Reporting channel number.
    /// @param measNr Measurement result sequence number.
    /// @param rxlevFull RXLEV on full-rate timeslot (dBm offset).
    /// @param rxqualFull RXQUAL on full-rate timeslot (0-7).
    /// @param l1Info Optional L1 information bytes.
    /// @return Serialized RSL frame or ParseError on failure.
    [[nodiscard]] static Expected<std::vector<uint8_t>> buildMeasRes(
        uint8_t chanNr, uint8_t measNr, int8_t rxlevFull, int8_t rxqualFull,
        std::span<const uint8_t> l1Info = {});

    /// Build MEAS_RES into pre-allocated buffer (zero heap alloc).
    [[nodiscard]] static int buildMeasRes(std::span<uint8_t> out,
        uint8_t chanNr, uint8_t measNr, int8_t rxlevFull, int8_t rxqualFull,
        std::span<const uint8_t> l1Info = {});

    /// Build HANDO_DET message (handover detection report).
    /// @param chanNr Source channel number.
    /// @param accessDelay Delay in frames since handover trigger.
    /// @return Serialized RSL frame or ParseError on failure.
    [[nodiscard]] static Expected<std::vector<uint8_t>> buildHandoDet(
        uint8_t chanNr, uint8_t accessDelay);

    /// Build HANDO_DET into pre-allocated buffer (zero heap alloc).
    [[nodiscard]] static int buildHandoDet(std::span<uint8_t> out,
        uint8_t chanNr, uint8_t accessDelay);

    // ── CCHAN messages (BTS -> BSC) ──────────────────────────────────

    /// Build CCCH_LOAD_IND message with channel load statistics.
    /// @param chanNr BCCH carrier channel number.
    /// @param pagingLoad Paging channel load percentage.
    /// @param rachTotal Total RACH bursts received.
    /// @param rachBusy RACH bursts during busy period.
    /// @param rachAccess Successful RACH accesses.
    /// @return Serialized RSL frame or ParseError on failure.
    [[nodiscard]] static Expected<std::vector<uint8_t>> buildCCCHLoadInd(
        uint8_t chanNr, uint16_t pagingLoad, uint16_t rachTotal,
        uint16_t rachBusy, uint16_t rachAccess);

    /// Build CCCH_LOAD_IND into pre-allocated buffer (zero heap alloc).
    [[nodiscard]] static int buildCCCHLoadInd(std::span<uint8_t> out,
        uint8_t chanNr, uint16_t pagingLoad, uint16_t rachTotal,
        uint16_t rachBusy, uint16_t rachAccess);

    /// Build CHAN_RQD message (channel required request from RACH).
    /// @param chanNr Common channel number (typically RACH).
    /// @param reqRef Request reference from the RACH burst.
    /// @param accessDelay Delay in frames since RACH received.
    /// @return Serialized RSL frame or ParseError on failure.
    [[nodiscard]] static Expected<std::vector<uint8_t>> buildChanRqd(
        uint8_t chanNr, const L3RequestReference& reqRef, uint8_t accessDelay);

    /// Build CHAN_RQD into pre-allocated buffer (zero heap alloc).
    [[nodiscard]] static int buildChanRqd(std::span<uint8_t> out,
        uint8_t chanNr, const L3RequestReference& reqRef, uint8_t accessDelay);

    /// Build DELETE_IND message (immediate assignment deletion report).
    /// @param chanNr Common channel number.
    /// @param fullImmAssInfo Full immediate assignment information bytes.
    /// @return Serialized RSL frame or ParseError on failure.
    [[nodiscard]] static Expected<std::vector<uint8_t>> buildDeleteInd(
        uint8_t chanNr, std::span<const uint8_t> fullImmAssInfo);

    /// Build DELETE_IND into pre-allocated buffer (zero heap alloc).
    [[nodiscard]] static int buildDeleteInd(std::span<uint8_t> out,
        uint8_t chanNr, std::span<const uint8_t> fullImmAssInfo);
};

} // namespace gsml3parser
