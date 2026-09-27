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

#include "gsml3parser/abis/rsl_builder.h"
#include <cstring>
#include <functional>

namespace gsml3parser {

namespace {

// RSL DCHAN/CCHAN frame prefix: first octet (message group + transparent
// flag) + global message type + Channel Number TV IE = 4 bytes.
constexpr size_t RSL_HEADER_SIZE = 4;
// RLL data frame prefix adds the Link Identifier TV IE on top of that.
constexpr size_t RSL_RLL_HEADER_SIZE = RSL_HEADER_SIZE + 2;

// Write the common DCHAN/CCHAN frame prefix: first octet per TS 48.058 9.1,
// global message type, then the Channel Number TV IE inlined as the first
// element of the information element list.
void writeHeader(uint8_t* buf, RSLDiscriminator disc, uint8_t msgType, uint8_t chanNr,
                 bool transparent) {
    buf[0] = rslFirstOctet(disc, transparent);
    buf[1] = msgType;
    buf[2] = static_cast<uint8_t>(RSL_IE::ChanNr);
    buf[3] = chanNr;
}

// Write an LV IE (type + 8-bit length + value).
size_t writeTLV(uint8_t* buf, size_t offset, uint8_t type, const uint8_t* val, uint8_t len) {
    buf[offset] = type;
    buf[offset + 1] = len;
    if (len > 0 && val) {
        std::memcpy(buf + offset + 2, val, len);
    }
    return offset + 2 + len;
}

// Write a TL16V IE (type + 16-bit big-endian length + value) for payloads
// that can exceed 255 octets, such as L3 information.
size_t writeTL16V(uint8_t* buf, size_t offset, uint8_t type, const uint8_t* val, uint16_t len) {
    buf[offset] = type;
    buf[offset + 1] = static_cast<uint8_t>((len >> 8) & 0xff);
    buf[offset + 2] = static_cast<uint8_t>(len & 0xff);
    if (len > 0 && val) {
        std::memcpy(buf + offset + 3, val, len);
    }
    return offset + 3 + len;
}

// Write a TV IE (type + fixed value octets, no length field).
size_t writeTVFixed(uint8_t* buf, size_t offset, uint8_t type, const uint8_t* val, size_t len) {
    buf[offset] = type;
    if (len > 0 && val) {
        std::memcpy(buf + offset + 1, val, len);
    }
    return offset + 1 + len;
}

// Helper: build an RLL data message (DATA_REQ/DATA_IND/UNIT_DATA_REQ/
// UNIT_DATA_IND). TS 48.058 8.3.x: channel number and link identifier are
// TV IEs of the list and the L3 PDU is carried inside the L3 Information IE
// (TL16V). RLL data messages are transparent frames (flag set).
int buildRLLData(std::span<uint8_t> out, uint8_t msgType, uint8_t chanNr, uint8_t linkId,
                 std::span<const uint8_t> l3Payload) {
    const size_t needed = RSL_RLL_HEADER_SIZE + 3 + l3Payload.size(); // prefix + TL16V L3Info
    if (out.size() < needed) return -1;

    uint8_t* p = out.data();
    p[0] = rslFirstOctet(RSLDiscriminator::Rll, /*transparent=*/true);
    p[1] = msgType;
    size_t off = 2;
    off = writeTVFixed(p, off, static_cast<uint8_t>(RSL_IE::ChanNr), &chanNr, 1);
    off = writeTVFixed(p, off, static_cast<uint8_t>(RSL_IE::LinkIdent), &linkId, 1);
    off = writeTL16V(p, off, static_cast<uint8_t>(RSL_IE::L3Info), l3Payload.data(),
                     static_cast<uint16_t>(l3Payload.size()));
    return static_cast<int>(off);
}

// Helper: build a DCHAN frame prefix; control frames are non-transparent.
int buildDChanMsg(std::span<uint8_t> out, uint8_t msgType, uint8_t chanNr) {
    if (out.size() < RSL_HEADER_SIZE) return -1;
    writeHeader(out.data(), RSLDiscriminator::DedicatedChannel, msgType, chanNr, /*transparent=*/false);
    return static_cast<int>(RSL_HEADER_SIZE);
}

// Helper: build a CCHAN frame prefix; control frames are non-transparent.
int buildCChanMsg(std::span<uint8_t> out, uint8_t msgType, uint8_t chanNr) {
    if (out.size() < RSL_HEADER_SIZE) return -1;
    writeHeader(out.data(), RSLDiscriminator::CommonChannel, msgType, chanNr, /*transparent=*/false);
    return static_cast<int>(RSL_HEADER_SIZE);
}

// Vector overload dispatcher: allocate buffer, call span version, return vector.
Expected<std::vector<uint8_t>> buildVector(std::initializer_list<size_t> sizeHints,
    std::function<int(std::span<uint8_t>)> builder) {
    // Pick the largest hint or a reasonable default.
    size_t estSize = 64;
    for (auto s : sizeHints) {
        if (s > estSize) estSize = s;
    }
    std::vector<uint8_t> buf(estSize);
    int n = builder(std::span<uint8_t>(buf));
    if (n < 0) {
        // Buffer too small, try with exact size.
        buf.resize(estSize * 2);
        n = builder(std::span<uint8_t>(buf));
        if (n < 0) {
            return Expected<std::vector<uint8_t>>::error(
                ParseError{ParseError::Code::InvalidValue, "RSL build failed: insufficient buffer"});
        }
    }
    buf.resize(n);
    return Expected<std::vector<uint8_t>>::hold(std::move(buf));
}

} // anonymous namespace

// ── RLL messages ──────────────────────────────────────────────────────

Expected<std::vector<uint8_t>> RSLBuilder::buildDataReq(
    uint8_t chanNr, uint8_t linkId, std::span<const uint8_t> l3Payload)
{
    return buildVector({RSL_RLL_HEADER_SIZE + 3 + l3Payload.size()},
        [&](std::span<uint8_t> out) {
            return buildRLLData(out, static_cast<uint8_t>(RSLL3MessageType::DataReq), chanNr, linkId, l3Payload);
        });
}

int RSLBuilder::buildDataReq(std::span<uint8_t> out, uint8_t chanNr, uint8_t linkId,
    std::span<const uint8_t> l3Payload)
{
    return buildRLLData(out, static_cast<uint8_t>(RSLL3MessageType::DataReq), chanNr, linkId, l3Payload);
}

Expected<std::vector<uint8_t>> RSLBuilder::buildDataInd(
    uint8_t chanNr, uint8_t linkId, std::span<const uint8_t> l3Payload)
{
    return buildVector({RSL_RLL_HEADER_SIZE + 3 + l3Payload.size()},
        [&](std::span<uint8_t> out) {
            return buildRLLData(out, static_cast<uint8_t>(RSLL3MessageType::DataInd), chanNr, linkId, l3Payload);
        });
}

int RSLBuilder::buildDataInd(std::span<uint8_t> out, uint8_t chanNr, uint8_t linkId,
    std::span<const uint8_t> l3Payload)
{
    return buildRLLData(out, static_cast<uint8_t>(RSLL3MessageType::DataInd), chanNr, linkId, l3Payload);
}

Expected<std::vector<uint8_t>> RSLBuilder::buildUnitDataReq(
    uint8_t chanNr, uint8_t linkId, std::span<const uint8_t> l3Payload)
{
    return buildVector({RSL_RLL_HEADER_SIZE + 3 + l3Payload.size()},
        [&](std::span<uint8_t> out) {
            return buildRLLData(out, static_cast<uint8_t>(RSLL3MessageType::UnitDataReq), chanNr, linkId, l3Payload);
        });
}

int RSLBuilder::buildUnitDataReq(std::span<uint8_t> out, uint8_t chanNr, uint8_t linkId,
    std::span<const uint8_t> l3Payload)
{
    return buildRLLData(out, static_cast<uint8_t>(RSLL3MessageType::UnitDataReq), chanNr, linkId, l3Payload);
}

Expected<std::vector<uint8_t>> RSLBuilder::buildUnitDataInd(
    uint8_t chanNr, uint8_t linkId, std::span<const uint8_t> l3Payload)
{
    return buildVector({RSL_RLL_HEADER_SIZE + 3 + l3Payload.size()},
        [&](std::span<uint8_t> out) {
            return buildRLLData(out, static_cast<uint8_t>(RSLL3MessageType::UnitDataInd), chanNr, linkId, l3Payload);
        });
}

int RSLBuilder::buildUnitDataInd(std::span<uint8_t> out, uint8_t chanNr, uint8_t linkId,
    std::span<const uint8_t> l3Payload)
{
    return buildRLLData(out, static_cast<uint8_t>(RSLL3MessageType::UnitDataInd), chanNr, linkId, l3Payload);
}

// ── DCHAN messages ────────────────────────────────────────────────────

Expected<std::vector<uint8_t>> RSLBuilder::buildChanActivAck(uint8_t chanNr, uint16_t frameNumber)
{
    return buildVector({RSL_HEADER_SIZE + 3},
        [&](std::span<uint8_t> out) {
            int n = buildDChanMsg(out, static_cast<uint8_t>(RSLDChanMessageType::ChanActivAck), chanNr);
            if (n < 0) return -1;
            // Frame Number IE: TV with a fixed two-octet value (big-endian).
            uint8_t fnBytes[2];
            fnBytes[0] = static_cast<uint8_t>((frameNumber >> 8) & 0xff);
            fnBytes[1] = static_cast<uint8_t>(frameNumber & 0xff);
            size_t off = writeTVFixed(out.data(), static_cast<size_t>(n),
                static_cast<uint8_t>(RSL_IE::FrameNumber), fnBytes, 2);
            return static_cast<int>(off);
        });
}

int RSLBuilder::buildChanActivAck(std::span<uint8_t> out, uint8_t chanNr, uint16_t frameNumber)
{
    int n = buildDChanMsg(out, static_cast<uint8_t>(RSLDChanMessageType::ChanActivAck), chanNr);
    if (n < 0) return -1;
    uint8_t fnBytes[2];
    fnBytes[0] = static_cast<uint8_t>((frameNumber >> 8) & 0xff);
    fnBytes[1] = static_cast<uint8_t>(frameNumber & 0xff);
    size_t off = writeTVFixed(out.data(), static_cast<size_t>(n),
        static_cast<uint8_t>(RSL_IE::FrameNumber), fnBytes, 2);
    return static_cast<int>(off);
}

Expected<std::vector<uint8_t>> RSLBuilder::buildChanActivNack(uint8_t chanNr, RSLErrorCause cause)
{
    return buildVector({RSL_HEADER_SIZE + 3},
        [&](std::span<uint8_t> out) {
            int n = buildDChanMsg(out, static_cast<uint8_t>(RSLDChanMessageType::ChanActivNack), chanNr);
            if (n < 0) return -1;
            // Cause IE: LV with a one-octet value.
            uint8_t causeByte = static_cast<uint8_t>(cause);
            size_t off = writeTLV(out.data(), static_cast<size_t>(n),
                static_cast<uint8_t>(RSL_IE::Cause), &causeByte, 1);
            return static_cast<int>(off);
        });
}

int RSLBuilder::buildChanActivNack(std::span<uint8_t> out, uint8_t chanNr, RSLErrorCause cause)
{
    int n = buildDChanMsg(out, static_cast<uint8_t>(RSLDChanMessageType::ChanActivNack), chanNr);
    if (n < 0) return -1;
    uint8_t causeByte = static_cast<uint8_t>(cause);
    size_t off = writeTLV(out.data(), static_cast<size_t>(n),
        static_cast<uint8_t>(RSL_IE::Cause), &causeByte, 1);
    return static_cast<int>(off);
}

Expected<std::vector<uint8_t>> RSLBuilder::buildRFChanRelAck(uint8_t chanNr)
{
    return buildVector({RSL_HEADER_SIZE},
        [&](std::span<uint8_t> out) {
            return buildDChanMsg(out, static_cast<uint8_t>(RSLDChanMessageType::RfChanRelAck), chanNr);
        });
}

int RSLBuilder::buildRFChanRelAck(std::span<uint8_t> out, uint8_t chanNr)
{
    return buildDChanMsg(out, static_cast<uint8_t>(RSLDChanMessageType::RfChanRelAck), chanNr);
}

Expected<std::vector<uint8_t>> RSLBuilder::buildConnFail(uint8_t chanNr, RSLErrorCause cause)
{
    return buildVector({RSL_HEADER_SIZE + 3},
        [&](std::span<uint8_t> out) {
            int n = buildDChanMsg(out, static_cast<uint8_t>(RSLDChanMessageType::ConnFail), chanNr);
            if (n < 0) return -1;
            uint8_t causeByte = static_cast<uint8_t>(cause);
            size_t off = writeTLV(out.data(), static_cast<size_t>(n),
                static_cast<uint8_t>(RSL_IE::Cause), &causeByte, 1);
            return static_cast<int>(off);
        });
}

int RSLBuilder::buildConnFail(std::span<uint8_t> out, uint8_t chanNr, RSLErrorCause cause)
{
    int n = buildDChanMsg(out, static_cast<uint8_t>(RSLDChanMessageType::ConnFail), chanNr);
    if (n < 0) return -1;
    uint8_t causeByte = static_cast<uint8_t>(cause);
    size_t off = writeTLV(out.data(), static_cast<size_t>(n),
        static_cast<uint8_t>(RSL_IE::Cause), &causeByte, 1);
    return static_cast<int>(off);
}

Expected<std::vector<uint8_t>> RSLBuilder::buildMeasRes(
    uint8_t chanNr, uint8_t measNr, int8_t rxlevFull, int8_t rxqualFull,
    std::span<const uint8_t> l1Info)
{
    size_t estSize = RSL_HEADER_SIZE + 2 + 2 + 3 + (l1Info.empty() ? 0 : 3);
    return buildVector({estSize},
        [&](std::span<uint8_t> out) {
            int n = buildDChanMsg(out, static_cast<uint8_t>(RSLDChanMessageType::MeasRes), chanNr);
            if (n < 0) return -1;
            // MeasResNr IE (TV, 1 octet).
            size_t off = writeTVFixed(out.data(), static_cast<size_t>(n),
                static_cast<uint8_t>(RSL_IE::MeasResNr), &measNr, 1);
            // UplinkMeas IE (LV): rxlev(1) + rxqual(1) + reserved(1).
            uint8_t uplinkData[3];
            uplinkData[0] = static_cast<uint8_t>(rxlevFull);
            uplinkData[1] = static_cast<uint8_t>(rxqualFull);
            uplinkData[2] = 0; // reserved
            off = writeTLV(out.data(), off, static_cast<uint8_t>(RSL_IE::UplinkMeas), uplinkData, 3);
            // Optional L1Info IE (TV, fixed two octets per TS 48.058 9.3.10).
            if (!l1Info.empty()) {
                uint8_t l1Bytes[2];
                l1Bytes[0] = l1Info[0];
                l1Bytes[1] = l1Info.size() > 1 ? l1Info[1] : 0;
                off = writeTVFixed(out.data(), off, static_cast<uint8_t>(RSL_IE::L1Info), l1Bytes, 2);
            }
            return static_cast<int>(off);
        });
}

int RSLBuilder::buildMeasRes(std::span<uint8_t> out, uint8_t chanNr, uint8_t measNr,
    int8_t rxlevFull, int8_t rxqualFull, std::span<const uint8_t> l1Info)
{
    int n = buildDChanMsg(out, static_cast<uint8_t>(RSLDChanMessageType::MeasRes), chanNr);
    if (n < 0) return -1;
    size_t off = writeTVFixed(out.data(), static_cast<size_t>(n),
        static_cast<uint8_t>(RSL_IE::MeasResNr), &measNr, 1);
    uint8_t uplinkData[3];
    uplinkData[0] = static_cast<uint8_t>(rxlevFull);
    uplinkData[1] = static_cast<uint8_t>(rxqualFull);
    uplinkData[2] = 0;
    off = writeTLV(out.data(), off, static_cast<uint8_t>(RSL_IE::UplinkMeas), uplinkData, 3);
    if (!l1Info.empty()) {
        uint8_t l1Bytes[2];
        l1Bytes[0] = l1Info[0];
        l1Bytes[1] = l1Info.size() > 1 ? l1Info[1] : 0;
        off = writeTVFixed(out.data(), off, static_cast<uint8_t>(RSL_IE::L1Info), l1Bytes, 2);
    }
    return static_cast<int>(off);
}

Expected<std::vector<uint8_t>> RSLBuilder::buildHandoDet(uint8_t chanNr, uint8_t accessDelay)
{
    return buildVector({RSL_HEADER_SIZE + 2},
        [&](std::span<uint8_t> out) {
            int n = buildDChanMsg(out, static_cast<uint8_t>(RSLDChanMessageType::HandoDet), chanNr);
            if (n < 0) return -1;
            size_t off = writeTVFixed(out.data(), static_cast<size_t>(n),
                static_cast<uint8_t>(RSL_IE::AccessDelay), &accessDelay, 1);
            return static_cast<int>(off);
        });
}

int RSLBuilder::buildHandoDet(std::span<uint8_t> out, uint8_t chanNr, uint8_t accessDelay)
{
    int n = buildDChanMsg(out, static_cast<uint8_t>(RSLDChanMessageType::HandoDet), chanNr);
    if (n < 0) return -1;
    size_t off = writeTVFixed(out.data(), static_cast<size_t>(n),
        static_cast<uint8_t>(RSL_IE::AccessDelay), &accessDelay, 1);
    return static_cast<int>(off);
}

// ── CCHAN messages ────────────────────────────────────────────────────

Expected<std::vector<uint8_t>> RSLBuilder::buildCCCHLoadInd(
    uint8_t chanNr, uint16_t pagingLoad, uint16_t rachTotal,
    uint16_t rachBusy, uint16_t rachAccess)
{
    // Header(4) + PagingLoad TV(3) + RachLoad LV(8) = 15 bytes.
    return buildVector({15},
        [&](std::span<uint8_t> out) {
            int n = buildCChanMsg(out, static_cast<uint8_t>(RSLCChanMessageType::CcchLoadInd), chanNr);
            if (n < 0) return -1;
            size_t off = static_cast<size_t>(n);
            // Paging Load IE: TV with a two-octet value (big-endian).
            uint8_t pl[2]; pl[0] = static_cast<uint8_t>((pagingLoad >> 8) & 0xff); pl[1] = static_cast<uint8_t>(pagingLoad & 0xff);
            off = writeTVFixed(out.data(), off, static_cast<uint8_t>(RSL_IE::PagingLoad), pl, 2);
            // RACH Load IE: LV with a six-octet value — slot, busy and access
            // counters as big-endian u16 each (TS 48.058 9.3.18).
            uint8_t rl[6];
            rl[0] = static_cast<uint8_t>((rachTotal >> 8) & 0xff); rl[1] = static_cast<uint8_t>(rachTotal & 0xff);
            rl[2] = static_cast<uint8_t>((rachBusy >> 8) & 0xff);  rl[3] = static_cast<uint8_t>(rachBusy & 0xff);
            rl[4] = static_cast<uint8_t>((rachAccess >> 8) & 0xff);rl[5] = static_cast<uint8_t>(rachAccess & 0xff);
            off = writeTLV(out.data(), off, static_cast<uint8_t>(RSL_IE::RachLoad), rl, 6);
            return static_cast<int>(off);
        });
}

int RSLBuilder::buildCCCHLoadInd(std::span<uint8_t> out, uint8_t chanNr, uint16_t pagingLoad,
    uint16_t rachTotal, uint16_t rachBusy, uint16_t rachAccess)
{
    int n = buildCChanMsg(out, static_cast<uint8_t>(RSLCChanMessageType::CcchLoadInd), chanNr);
    if (n < 0) return -1;
    size_t off = static_cast<size_t>(n);
    uint8_t pl[2]; pl[0] = static_cast<uint8_t>((pagingLoad >> 8) & 0xff); pl[1] = static_cast<uint8_t>(pagingLoad & 0xff);
    off = writeTVFixed(out.data(), off, static_cast<uint8_t>(RSL_IE::PagingLoad), pl, 2);
    uint8_t rl[6];
    rl[0] = static_cast<uint8_t>((rachTotal >> 8) & 0xff); rl[1] = static_cast<uint8_t>(rachTotal & 0xff);
    rl[2] = static_cast<uint8_t>((rachBusy >> 8) & 0xff);  rl[3] = static_cast<uint8_t>(rachBusy & 0xff);
    rl[4] = static_cast<uint8_t>((rachAccess >> 8) & 0xff);rl[5] = static_cast<uint8_t>(rachAccess & 0xff);
    off = writeTLV(out.data(), off, static_cast<uint8_t>(RSL_IE::RachLoad), rl, 6);
    return static_cast<int>(off);
}

Expected<std::vector<uint8_t>> RSLBuilder::buildChanRqd(
    uint8_t chanNr, const L3RequestReference& reqRef, uint8_t accessDelay)
{
    return buildVector({RSL_HEADER_SIZE + 4 + 2},
        [&](std::span<uint8_t> out) {
            int n = buildCChanMsg(out, static_cast<uint8_t>(RSLCChanMessageType::ChanRqd), chanNr);
            if (n < 0) return -1;
            size_t off = static_cast<size_t>(n);
            // ReqReference IE: TV with a fixed three-octet value — RA plus the
            // two-octet frame number T1'(5)|T3(6)|T2(5) (TS 48.058 9.3.19).
            uint8_t refBytes[3];
            refBytes[0] = reqRef.ra();
            refBytes[1] = static_cast<uint8_t>(((reqRef.t1p() & 0x1Fu) << 3) | ((reqRef.t3() >> 3) & 0x07u));
            refBytes[2] = static_cast<uint8_t>(((reqRef.t3() & 0x3Fu) << 5) | (reqRef.t2() & 0x1Fu));
            off = writeTVFixed(out.data(), off, static_cast<uint8_t>(RSL_IE::ReqReference), refBytes, 3);
            // AccessDelay IE: TV, one octet.
            off = writeTVFixed(out.data(), off, static_cast<uint8_t>(RSL_IE::AccessDelay), &accessDelay, 1);
            return static_cast<int>(off);
        });
}

int RSLBuilder::buildChanRqd(std::span<uint8_t> out, uint8_t chanNr,
    const L3RequestReference& reqRef, uint8_t accessDelay)
{
    int n = buildCChanMsg(out, static_cast<uint8_t>(RSLCChanMessageType::ChanRqd), chanNr);
    if (n < 0) return -1;
    size_t off = static_cast<size_t>(n);
    uint8_t refBytes[3];
    refBytes[0] = reqRef.ra();
    refBytes[1] = static_cast<uint8_t>(((reqRef.t1p() & 0x1Fu) << 3) | ((reqRef.t3() >> 3) & 0x07u));
    refBytes[2] = static_cast<uint8_t>(((reqRef.t3() & 0x3Fu) << 5) | (reqRef.t2() & 0x1Fu));
    off = writeTVFixed(out.data(), off, static_cast<uint8_t>(RSL_IE::ReqReference), refBytes, 3);
    off = writeTVFixed(out.data(), off, static_cast<uint8_t>(RSL_IE::AccessDelay), &accessDelay, 1);
    return static_cast<int>(off);
}

Expected<std::vector<uint8_t>> RSLBuilder::buildDeleteInd(
    uint8_t chanNr, std::span<const uint8_t> fullImmAssInfo)
{
    return buildVector({RSL_HEADER_SIZE + 2 + fullImmAssInfo.size()},
        [&](std::span<uint8_t> out) {
            int n = buildCChanMsg(out, static_cast<uint8_t>(RSLCChanMessageType::DeleteInd), chanNr);
            if (n < 0) return -1;
            size_t off = writeTLV(out.data(), static_cast<size_t>(n),
                static_cast<uint8_t>(RSL_IE::FullImmAssInfo), fullImmAssInfo.data(),
                static_cast<uint8_t>(fullImmAssInfo.size()));
            return static_cast<int>(off);
        });
}

int RSLBuilder::buildDeleteInd(std::span<uint8_t> out, uint8_t chanNr,
    std::span<const uint8_t> fullImmAssInfo)
{
    int n = buildCChanMsg(out, static_cast<uint8_t>(RSLCChanMessageType::DeleteInd), chanNr);
    if (n < 0) return -1;
    size_t off = writeTLV(out.data(), static_cast<size_t>(n),
        static_cast<uint8_t>(RSL_IE::FullImmAssInfo), fullImmAssInfo.data(),
        static_cast<uint8_t>(fullImmAssInfo.size()));
    return static_cast<int>(off);
}

} // namespace gsml3parser
