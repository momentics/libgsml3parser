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

#include "gsml3parser/bitstream/framer.h"
#include "gsml3parser/l3header.h"
#include <cstring>
#include <chrono>
#include <climits>

namespace gsml3parser {

namespace {

/**
 * Return the expected body length (in bytes, excluding the 2-byte L3 header)
 * for known fixed-length messages.  Returns SIZE_MAX if the message is
 * variable-length and cannot be determined from PD+MTI alone.
 */
size_t fixedBodyLength(int pd, int mti) {
    constexpr size_t VARIABLE = SIZE_MAX;
    switch (pd) {
        case 0x06: // Radio Resource
            switch (mti) {
                case 0x0D: return 1;  // Channel Release
                case 0x0E: return 5;  // Paging Response
                case 0x0F: return 8;  // Classmark Change
                case 0x10: return 3;  // Classmark Enquiry
                case 0x1A: return 0;  // RR Status
                case 0x1C: return 4;  // Assignment Complete
                case 0x1D: return 2;  // Assignment Failure
                case 0x21: return 3;  // Immediate Assignment Reject
                case 0x25: return 0;  // Additional Assignment
                case 0x29: return 4;  // Handover Complete
                case 0x2A: return 2;  // Handover Failure
                case 0x2E: return 1;  // Physical Information
                case 0x33: return 2;  // Ciphering Mode Complete
                default:   return VARIABLE;
            }
        case 0x05: // Mobility Management
            switch (mti) {
                case 0x1F: return 4;  // IMSI Detach Indication
                case 0x21: return 0;  // CM Service Accept
                case 0x22: return 3;  // CM Service Reject
                case 0x23: return 1;  // CM Service Abort
                case 0x27: return 1;  // MM Status
                default:   return VARIABLE;
            }
        case 0x03: // Call Control
            switch (mti) {
                case 0x23: return 1;  // Release Complete
                case 0x25: return 1;  // CC Status (min body)
                default:   return VARIABLE;
            }
        case 0x01: // Broadcast Call Control (6-bit MTI, shifted; see bcc/l3bccmessages.h)
            switch (mti) {
                case 0x00: return 0;  // BCC Setup (header-only in known vectors)
                case 0x04: return 0;  // BCC Call Confirmed (no body)
                case 0x09: return 0;  // BCC Connect Acknowledge (no body)
                default:   return VARIABLE;
            }
        case 0x00: // Group Call Control (6-bit MTI, shifted; see gcc/l3gccmessages.h)
            switch (mti) {
                case 0x00: return 1;  // GCC Setup (1-byte opaque body in known vectors)
                case 0x03: return 0;  // GCC Call Confirmed (no body)
                default:   return VARIABLE;
            }
        case 0x0c: // Location Services (8-bit MTI, no shift; see ls/l3lsmessages.h)
            switch (mti) {
                case 0x01: return 0;  // Location Service Request (header-only in known vectors)
                default:   return VARIABLE;
            }
        default:
            return VARIABLE;
    }
}

} // anonymous namespace

// ── L3Framer ───────────────────────────────────────────────────────────

L3Framer::L3Framer(ByteSource& source, FrameConfig cfg)
    : mSource(source), mConfig(std::move(cfg))
{
    mBuf.resize(4096); // Pre-allocate buffer so data() is always valid.
}

bool L3Framer::fillBuffer() {
    // Make room at the end of the buffer.
    if (mPos > 1024) {
        // Compact: shift consumed bytes to the front.
        if (mEnd > mPos) {
            std::memmove(mBuf.data(), mBuf.data() + mPos, mEnd - mPos);
        }
        mEnd -= mPos;
        mPos = 0;
    }

    // Ensure enough capacity.
    if (mBuf.size() <= mEnd) {
        mBuf.resize(mEnd + 4096);
    }

    size_t n = mSource.read(mBuf.data() + mEnd, mBuf.size() - mEnd);
    if (n > 0) {
        mEnd += n;
        // Batched timestamp (audit N2): frames extracted from this fill
        // share the fill time instead of each reading steady_clock.
        mBufferTimestamp = std::chrono::duration<double>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }
    return n > 0;
}

Expected<ExtractedFrame> L3Framer::tryExtract() {
    // Need at least the minimum header.
    if (mEnd - mPos < mConfig.minHeaderLength) {
        return Expected<ExtractedFrame>::error(
            {ParseError::Code::TruncatedInput, "insufficient data for L3 header"});
    }

    size_t frameLen = 0;

    if (mConfig.useL2Length) {
        // L2 length mode: first byte is the length of the L3 message that follows.
        if (mEnd - mPos < 1) {
            return Expected<ExtractedFrame>::error(
                {ParseError::Code::TruncatedInput, "need L2 length octet"});
        }
        size_t l2len = static_cast<size_t>(mBuf[mPos]);
        if (l2len == 0 || l2len > mConfig.maxMessageLength) {
            // Invalid length - skip one byte and try again.
            mPos++;
            return Expected<ExtractedFrame>::error(
                {ParseError::Code::InvalidValue, "L2 length out of range"});
        }
        frameLen = l2len + 1; // length octet + message

        if (mEnd - mPos < frameLen) {
            return Expected<ExtractedFrame>::error(
                {ParseError::Code::TruncatedInput, "incomplete frame (L2 length)"});
        }

        ExtractedFrame frame;
        frame.data = std::span<const uint8_t>(mBuf.data() + mPos + 1, l2len);
        frame.l2Length = l2len;
        frame.timestamp = mBufferTimestamp;
        mPos += frameLen;
        return Expected<ExtractedFrame>::hold(frame);
    }

    // Header-based mode: try to determine frame length from PD+MTI.
    {
        uint8_t b0 = mBuf[mPos];
        uint8_t b1 = mBuf[mPos + 1];
        int pd = (b0 >> 4) & 0x0F;

        // For CC/SS: bits [4:6] of byte 0 are TI, bit 7 is TIF.
        // For all PDs: byte 1 is raw MTI.
        int rawMti = b1;
        int mti = rawMti;

        // Adjust MTI for MM/CC/SS/BCC/GCC (6-bit messageType + 2-bit NSD).
        // BCC (0x01) and GCC (0x00) use the same CC-style header (TS 44.018 10.2).
        // LS (0x0c) carries a raw 8-bit MTI and needs no adjustment.
        if (pd == 0x05 || pd == 0x03 || pd == 0x0b || pd == 0x01 || pd == 0x00) {
            mti = (rawMti & 0xFC) >> 2;
        }

        size_t bodyLen = fixedBodyLength(pd, mti);

        if (bodyLen != SIZE_MAX) {
            // Fixed-length message: frame = 2 (header) + bodyLen.
            frameLen = 2 + bodyLen;
        } else {
            // Variable-length message: try to parse to determine length.
            // We attempt a greedy approach: parse the header and as much body
            // as we can, looking for a natural boundary.
            // For now, use a heuristic: scan for the next plausible L3 header.
            //
            // Boundary candidates depend on the PD of the message being framed
            // (C17): while framing BCC (0x01), GCC (0x00) or LS (0x0c)
            // messages, any of the 12 valid PDs may start the next message, so
            // the full list is used. For all other PDs the original list is
            // kept: 0x00/0x01/0x0c high nibbles occur frequently inside
            // variable-length bodies (e.g. GMM/SMS cause octets) and listing
            // them unconditionally would create false frame boundaries.
            const bool callControlLike = (pd == 0x00 || pd == 0x01 || pd == 0x0c);
            frameLen = 0;
            for (size_t i = mPos + 2; i + 1 < mEnd && i < mPos + 2 + mConfig.maxMessageLength; ++i) {
                uint8_t candidatePd = (mBuf[i] >> 4) & 0x0F;
                // Check if this looks like a valid L3 header start.
                // Base valid PDs: 0x03, 0x05, 0x06, 0x0b, 0x08, 0x09, 0x0a, 0x0e, 0x0f.
                // Extended with 0x00 (GCC), 0x01 (BCC), 0x0c (LS) when framing
                // BCC/GCC/LS messages.
                const bool plausible =
                    candidatePd == 0x03 || candidatePd == 0x05 ||
                    candidatePd == 0x06 || candidatePd == 0x0b ||
                    candidatePd == 0x08 || candidatePd == 0x09 ||
                    candidatePd == 0x0a || candidatePd == 0x0e ||
                    candidatePd == 0x0f ||
                    (callControlLike &&
                     (candidatePd == 0x00 || candidatePd == 0x01 || candidatePd == 0x0c));
                if (plausible) {
                    // This might be the start of the next message.
                    frameLen = i - mPos;
                    break;
                }
            }

            if (frameLen == 0) {
                // No boundary found - need more data.
                return Expected<ExtractedFrame>::error(
                    {ParseError::Code::TruncatedInput, "variable-length frame, need more data"});
            }

            // Safety: ensure frame is not too large.
            if (frameLen > mConfig.maxMessageLength) {
                mPos++; // skip one byte and retry
                return Expected<ExtractedFrame>::error(
                    {ParseError::Code::InvalidValue, "frame exceeds maxMessageLength"});
            }
        }
    }

    if (mEnd - mPos < frameLen) {
        return Expected<ExtractedFrame>::error(
            {ParseError::Code::TruncatedInput, "incomplete frame (header-based)"});
    }

    // Validate the L3 header PD value.
    {
        auto hdrResult = parseL3Header(std::span<const uint8_t>(mBuf.data() + mPos, frameLen));
        if (!hdrResult || !hdrResult.value().isValid()) {
            // Invalid PD - skip this byte and try again.
            // Invalid/reserved PD - skip this byte and resync (audit Q4).
            mPos++;
            return Expected<ExtractedFrame>::error(
                {ParseError::Code::InvalidPD, "invalid L3 header in frame"});
        }
    }

    ExtractedFrame frame;
    frame.data = std::span<const uint8_t>(mBuf.data() + mPos, frameLen);
    frame.l2Length = 0;
    frame.timestamp = mBufferTimestamp;
    mPos += frameLen;
    return Expected<ExtractedFrame>::hold(frame);
}

Expected<ExtractedFrame> L3Framer::nextFrame() {
    // Loop to handle corrupt frames: skip bad bytes and retry.
    while (true) {
        // Try to extract from currently buffered data.
        auto result = tryExtract();
        if (result) return result;

        const auto& err = result.error();

        // InvalidPD means we skipped a byte in tryExtract; retry with remaining data.
        if (err.code == ParseError::Code::InvalidPD) {
            continue;
        }

        // InvalidValue means frame too large; try to recover by skipping.
        if (err.code == ParseError::Code::InvalidValue) {
            continue;
        }

        // TruncatedInput - try to fill the buffer.
        bool gotData = fillBuffer();

        if (!gotData) {
            // Source returned 0 bytes (EOF or non-blocking with no data).
            return Expected<ExtractedFrame>::error(
                {ParseError::Code::TruncatedInput, "source returned no data"});
        }

        // After filling, check if we still have enough for a header.
        if (mEnd - mPos < mConfig.minHeaderLength) {
            return Expected<ExtractedFrame>::error(
                {ParseError::Code::TruncatedInput, "insufficient data after refill"});
        }
        // Loop back to tryExtract with new data.
    }
}

void L3Framer::skip(size_t nbytes) {
    if (mPos + nbytes > mEnd) nbytes = mEnd - mPos;
    mPos += nbytes;
}

} // namespace gsml3parser
