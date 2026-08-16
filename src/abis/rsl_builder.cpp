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

// RSL common header: discriminator(1) + msg_type(1) + chan_nr(1) + extra(1) = 4 bytes.
constexpr size_t RSL_HEADER_SIZE = 4;

// Write the common 4-byte RSL header.
void writeHeader(uint8_t* buf, uint8_t disc, uint8_t msgType, uint8_t chanNr, uint8_t extra) {
    buf[0] = disc;
    buf[1] = msgType;
    buf[2] = chanNr;
    buf[3] = extra;
}

// Write a standard TLV IE (type + length + value).
size_t writeTLV(uint8_t* buf, size_t offset, uint8_t type, const uint8_t* val, uint8_t len) {
    buf[offset] = type;
    buf[offset + 1] = len;
    if (len > 0 && val) {
        std::memcpy(buf + offset + 2, val, len);
    }
    return offset + 2 + len;
}

// Write a TL16V IE (type + 2-byte length + value) for large payloads like L3Info.
size_t writeTL16V(uint8_t* buf, size_t offset, uint8_t type, const uint8_t* val, uint16_t len) {
    buf[offset] = type;
    buf[offset + 1] = static_cast<uint8_t>((len >> 8) & 0xff);
    buf[offset + 2] = static_cast<uint8_t>(len & 0xff);
    if (len > 0 && val) {
        std::memcpy(buf + offset + 3, val, len);
    }
    return offset + 3 + len;
}

// Write a TV IE (type + 1-byte value, no length field).
size_t writeTV(uint8_t* buf, size_t offset, uint8_t type, uint8_t value) {
    buf[offset] = type;
    buf[offset + 1] = value;
    return offset + 2;
}

// Helper: build an RLL data message (DATA_REQ/DATA_IND/UNIT_DATA_REQ/UNIT_DATA_IND).
// The L3 payload is placed directly after the 4-byte header.
int buildRLLData(std::span<uint8_t> out, uint8_t msgType, uint8_t chanNr, uint8_t linkId,
                 std::span<const uint8_t> l3Payload) {
    size_t needed = RSL_HEADER_SIZE + l3Payload.size();
    if (out.size() < needed) return -1;

    writeHeader(out.data(), static_cast<uint8_t>(RSLDiscriminator::RLL), msgType, chanNr, linkId);
    if (!l3Payload.empty()) {
        std::memcpy(out.data() + RSL_HEADER_SIZE, l3Payload.data(), l3Payload.size());
    }
    return static_cast<int>(needed);
}

// Helper: build a DCHAN message with chanNr.
int buildDChanMsg(std::span<uint8_t> out, uint8_t msgType, uint8_t chanNr) {
    if (out.size() < RSL_HEADER_SIZE) return -1;
    writeHeader(out.data(), static_cast<uint8_t>(RSLDiscriminator::DedicatedChannel), msgType, chanNr, 0);
    return static_cast<int>(RSL_HEADER_SIZE);
}

// Helper: build a CCHAN message with chanNr.
int buildCChanMsg(std::span<uint8_t> out, uint8_t msgType, uint8_t chanNr) {
    if (out.size() < RSL_HEADER_SIZE) return -1;
    writeHeader(out.data(), static_cast<uint8_t>(RSLDiscriminator::CommonChannel), msgType, chanNr, 0);
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
    return buildVector({RSL_HEADER_SIZE + l3Payload.size()},
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
    return buildVector({RSL_HEADER_SIZE + l3Payload.size()},
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
    return buildVector({RSL_HEADER_SIZE + l3Payload.size()},
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
    return buildVector({RSL_HEADER_SIZE + l3Payload.size()},
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
    return buildVector({RSL_HEADER_SIZE + 4},
        [&](std::span<uint8_t> out) {
            int n = buildDChanMsg(out, static_cast<uint8_t>(RSLDChanMessageType::ChanActivAck), chanNr);
            if (n < 0) return -1;
            // FrameNumber IE: type=0x2b, len=2, value=frameNumber(big-endian)
            uint8_t fnBytes[2];
            fnBytes[0] = static_cast<uint8_t>((frameNumber >> 8) & 0xff);
            fnBytes[1] = static_cast<uint8_t>(frameNumber & 0xff);
            size_t off = writeTLV(out.data(), static_cast<size_t>(n),
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
    size_t off = writeTLV(out.data(), static_cast<size_t>(n),
        static_cast<uint8_t>(RSL_IE::FrameNumber), fnBytes, 2);
    return static_cast<int>(off);
}

Expected<std::vector<uint8_t>> RSLBuilder::buildChanActivNack(uint8_t chanNr, RSLErrorCause cause)
{
    return buildVector({RSL_HEADER_SIZE + 2},
        [&](std::span<uint8_t> out) {
            int n = buildDChanMsg(out, static_cast<uint8_t>(RSLDChanMessageType::ChanActivNack), chanNr);
            if (n < 0) return -1;
            size_t off = writeTV(out.data(), static_cast<size_t>(n),
                static_cast<uint8_t>(RSL_IE::Cause), static_cast<uint8_t>(cause));
            return static_cast<int>(off);
        });
}

int RSLBuilder::buildChanActivNack(std::span<uint8_t> out, uint8_t chanNr, RSLErrorCause cause)
{
    int n = buildDChanMsg(out, static_cast<uint8_t>(RSLDChanMessageType::ChanActivNack), chanNr);
    if (n < 0) return -1;
    size_t off = writeTV(out.data(), static_cast<size_t>(n),
        static_cast<uint8_t>(RSL_IE::Cause), static_cast<uint8_t>(cause));
    return static_cast<int>(off);
}

Expected<std::vector<uint8_t>> RSLBuilder::buildRFChanRelAck(uint8_t chanNr)
{
    return buildVector({RSL_HEADER_SIZE},
        [&](std::span<uint8_t> out) {
            return buildDChanMsg(out, static_cast<uint8_t>(RSLDChanMessageType::RFChanRelAck), chanNr);
        });
}

int RSLBuilder::buildRFChanRelAck(std::span<uint8_t> out, uint8_t chanNr)
{
    return buildDChanMsg(out, static_cast<uint8_t>(RSLDChanMessageType::RFChanRelAck), chanNr);
}

Expected<std::vector<uint8_t>> RSLBuilder::buildConnFail(uint8_t chanNr, RSLErrorCause cause)
{
    return buildVector({RSL_HEADER_SIZE + 2},
        [&](std::span<uint8_t> out) {
            int n = buildDChanMsg(out, static_cast<uint8_t>(RSLDChanMessageType::ConnFail), chanNr);
            if (n < 0) return -1;
            size_t off = writeTV(out.data(), static_cast<size_t>(n),
                static_cast<uint8_t>(RSL_IE::Cause), static_cast<uint8_t>(cause));
            return static_cast<int>(off);
        });
}

int RSLBuilder::buildConnFail(std::span<uint8_t> out, uint8_t chanNr, RSLErrorCause cause)
{
    int n = buildDChanMsg(out, static_cast<uint8_t>(RSLDChanMessageType::ConnFail), chanNr);
    if (n < 0) return -1;
    size_t off = writeTV(out.data(), static_cast<size_t>(n),
        static_cast<uint8_t>(RSL_IE::Cause), static_cast<uint8_t>(cause));
    return static_cast<int>(off);
}

Expected<std::vector<uint8_t>> RSLBuilder::buildMeasRes(
    uint8_t chanNr, uint8_t measNr, int8_t rxlevFull, int8_t rxqualFull,
    std::span<const uint8_t> l1Info)
{
    size_t estSize = RSL_HEADER_SIZE + 2 + 2 + 3 + (l1Info.empty() ? 0 : (2 + l1Info.size()));
    return buildVector({estSize},
        [&](std::span<uint8_t> out) {
            int n = buildDChanMsg(out, static_cast<uint8_t>(RSLDChanMessageType::MeasRes), chanNr);
            if (n < 0) return -1;
            // MeasResNr IE (TV)
            size_t off = writeTV(out.data(), static_cast<size_t>(n),
                static_cast<uint8_t>(RSL_IE::MeasResNr), measNr);
            // UplinkMeas IE (TLV): 3 bytes = rxlev(1) + rxqual(1) + reserved(1)
            uint8_t uplinkData[3];
            uplinkData[0] = static_cast<uint8_t>(rxlevFull);
            uplinkData[1] = static_cast<uint8_t>(rxqualFull);
            uplinkData[2] = 0; // reserved
            off = writeTLV(out.data(), off, static_cast<uint8_t>(RSL_IE::UplinkMeas), uplinkData, 3);
            // Optional L1Info IE
            if (!l1Info.empty()) {
                off = writeTLV(out.data(), off, static_cast<uint8_t>(RSL_IE::L1Info), l1Info.data(),
                    static_cast<uint8_t>(l1Info.size()));
            }
            return static_cast<int>(off);
        });
}

int RSLBuilder::buildMeasRes(std::span<uint8_t> out, uint8_t chanNr, uint8_t measNr,
    int8_t rxlevFull, int8_t rxqualFull, std::span<const uint8_t> l1Info)
{
    int n = buildDChanMsg(out, static_cast<uint8_t>(RSLDChanMessageType::MeasRes), chanNr);
    if (n < 0) return -1;
    size_t off = writeTV(out.data(), static_cast<size_t>(n),
        static_cast<uint8_t>(RSL_IE::MeasResNr), measNr);
    uint8_t uplinkData[3];
    uplinkData[0] = static_cast<uint8_t>(rxlevFull);
    uplinkData[1] = static_cast<uint8_t>(rxqualFull);
    uplinkData[2] = 0;
    off = writeTLV(out.data(), off, static_cast<uint8_t>(RSL_IE::UplinkMeas), uplinkData, 3);
    if (!l1Info.empty()) {
        off = writeTLV(out.data(), off, static_cast<uint8_t>(RSL_IE::L1Info), l1Info.data(),
            static_cast<uint8_t>(l1Info.size()));
    }
    return static_cast<int>(off);
}

Expected<std::vector<uint8_t>> RSLBuilder::buildHandoDet(uint8_t chanNr, uint8_t accessDelay)
{
    return buildVector({RSL_HEADER_SIZE + 2},
        [&](std::span<uint8_t> out) {
            int n = buildDChanMsg(out, static_cast<uint8_t>(RSLDChanMessageType::HandoDet), chanNr);
            if (n < 0) return -1;
            size_t off = writeTV(out.data(), static_cast<size_t>(n),
                static_cast<uint8_t>(RSL_IE::AccessDelay), accessDelay);
            return static_cast<int>(off);
        });
}

int RSLBuilder::buildHandoDet(std::span<uint8_t> out, uint8_t chanNr, uint8_t accessDelay)
{
    int n = buildDChanMsg(out, static_cast<uint8_t>(RSLDChanMessageType::HandoDet), chanNr);
    if (n < 0) return -1;
    size_t off = writeTV(out.data(), static_cast<size_t>(n),
        static_cast<uint8_t>(RSL_IE::AccessDelay), accessDelay);
    return static_cast<int>(off);
}

// ── CCHAN messages ────────────────────────────────────────────────────

Expected<std::vector<uint8_t>> RSLBuilder::buildCCCHLoadInd(
    uint8_t chanNr, uint16_t pagingLoad, uint16_t rachTotal,
    uint16_t rachBusy, uint16_t rachAccess)
{
    // Header(4) + 4x TV IEs (2 bytes each) = 12 bytes.
    return buildVector({12},
        [&](std::span<uint8_t> out) {
            int n = buildCChanMsg(out, static_cast<uint8_t>(RSLCChanMessageType::CCCHLoadInd), chanNr);
            if (n < 0) return -1;
            size_t off = static_cast<size_t>(n);
            // For CCCH_LOAD_IND, the load values are packed into specific IE positions.
            // Using simple TV encoding for each counter.
            uint8_t pl[2]; pl[0] = static_cast<uint8_t>((pagingLoad >> 8) & 0xff); pl[1] = static_cast<uint8_t>(pagingLoad & 0xff);
            off = writeTLV(out.data(), off, static_cast<uint8_t>(RSL_IE::PagingGroup), pl, 2);
            uint8_t rt[2]; rt[0] = static_cast<uint8_t>((rachTotal >> 8) & 0xff); rt[1] = static_cast<uint8_t>(rachTotal & 0xff);
            off = writeTLV(out.data(), off, static_cast<uint8_t>(RSL_IE::ReqReference), rt, 2);
            uint8_t rb[2]; rb[0] = static_cast<uint8_t>((rachBusy >> 8) & 0xff); rb[1] = static_cast<uint8_t>(rachBusy & 0xff);
            off = writeTLV(out.data(), off, static_cast<uint8_t>(RSL_IE::FrameNumber), rb, 2);
            uint8_t ra[2]; ra[0] = static_cast<uint8_t>((rachAccess >> 8) & 0xff); ra[1] = static_cast<uint8_t>(rachAccess & 0xff);
            off = writeTLV(out.data(), off, static_cast<uint8_t>(RSL_IE::SysInfoType), ra, 2);
            return static_cast<int>(off);
        });
}

int RSLBuilder::buildCCCHLoadInd(std::span<uint8_t> out, uint8_t chanNr, uint16_t pagingLoad,
    uint16_t rachTotal, uint16_t rachBusy, uint16_t rachAccess)
{
    int n = buildCChanMsg(out, static_cast<uint8_t>(RSLCChanMessageType::CCCHLoadInd), chanNr);
    if (n < 0) return -1;
    size_t off = static_cast<size_t>(n);
    uint8_t pl[2]; pl[0] = static_cast<uint8_t>((pagingLoad >> 8) & 0xff); pl[1] = static_cast<uint8_t>(pagingLoad & 0xff);
    off = writeTLV(out.data(), off, static_cast<uint8_t>(RSL_IE::PagingGroup), pl, 2);
    uint8_t rt[2]; rt[0] = static_cast<uint8_t>((rachTotal >> 8) & 0xff); rt[1] = static_cast<uint8_t>(rachTotal & 0xff);
    off = writeTLV(out.data(), off, static_cast<uint8_t>(RSL_IE::ReqReference), rt, 2);
    uint8_t rb[2]; rb[0] = static_cast<uint8_t>((rachBusy >> 8) & 0xff); rb[1] = static_cast<uint8_t>(rachBusy & 0xff);
    off = writeTLV(out.data(), off, static_cast<uint8_t>(RSL_IE::FrameNumber), rb, 2);
    uint8_t ra[2]; ra[0] = static_cast<uint8_t>((rachAccess >> 8) & 0xff); ra[1] = static_cast<uint8_t>(rachAccess & 0xff);
    off = writeTLV(out.data(), off, static_cast<uint8_t>(RSL_IE::SysInfoType), ra, 2);
    return static_cast<int>(off);
}

Expected<std::vector<uint8_t>> RSLBuilder::buildChanRqd(
    uint8_t chanNr, const L3RequestReference& reqRef, uint8_t accessDelay)
{
    return buildVector({RSL_HEADER_SIZE + 5 + 2},
        [&](std::span<uint8_t> out) {
            int n = buildCChanMsg(out, static_cast<uint8_t>(RSLCChanMessageType::ChanRqd), chanNr);
            if (n < 0) return -1;
            size_t off = static_cast<size_t>(n);
            // ReqReference IE: type=0x2a, len=3, value=RA(1) + T1'(1) + T2(1)
            uint8_t refBytes[3];
            refBytes[0] = reqRef.ra();
            refBytes[1] = reqRef.t1p();
            refBytes[2] = reqRef.t2();
            off = writeTLV(out.data(), off, static_cast<uint8_t>(RSL_IE::ReqReference), refBytes, 3);
            // AccessDelay IE (TV)
            off = writeTV(out.data(), off, static_cast<uint8_t>(RSL_IE::AccessDelay), accessDelay);
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
    refBytes[1] = reqRef.t1p();
    refBytes[2] = reqRef.t2();
    off = writeTLV(out.data(), off, static_cast<uint8_t>(RSL_IE::ReqReference), refBytes, 3);
    off = writeTV(out.data(), off, static_cast<uint8_t>(RSL_IE::AccessDelay), accessDelay);
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
