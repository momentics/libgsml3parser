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

#include "gsml3parser/lapdm_entity.h"

#include <algorithm>
#include <cstring>

namespace gsml3parser {

// ── LAPDmChannelProfile factory methods ────────────────────────────────

LAPDmChannelProfile LAPDmChannelProfile::SDCCH() noexcept {
    return {20, 23, 900};
}

LAPDmChannelProfile LAPDmChannelProfile::SACCH() noexcept {
    return {18, 5, 3600};
}

LAPDmChannelProfile LAPDmChannelProfile::FACCH() noexcept {
    return {20, 34, 900};
}

LAPDmChannelProfile LAPDmChannelProfile::FACCH_LM() noexcept {
    return {20, 29, 900};
}

// ── LAPDmState stream operator ────────────────────────────────────────

std::ostream& operator<<(std::ostream& os, LAPDmState state) {
    switch (state) {
        case LAPDmState::Unused:               os << "Unused"; break;
        case LAPDmState::LinkReleased:         os << "LinkReleased"; break;
        case LAPDmState::AwaitingEstablish:    os << "AwaitingEstablish"; break;
        case LAPDmState::AwaitingRelease:      os << "AwaitingRelease"; break;
        case LAPDmState::LinkEstablished:      os << "LinkEstablished"; break;
        case LAPDmState::ContentionResolution: os << "ContentionResolution"; break;
    }
    return os;
}

// ── LAPDmEntity constructor ───────────────────────────────────────────

LAPDmEntity::LAPDmEntity(LAPDmChannelProfile profile, L3ReceiveFn l3Cb,
                         L1TransmitFn l1Cb, void* ctx)
    : mProfile(profile), mL3Callback(l3Cb), mL1Callback(l1Cb), mCallbackCtx(ctx) {}

// ── Public API ────────────────────────────────────────────────────────

void LAPDmEntity::open(SAPI sapi, bool commandBit) noexcept {
    mSapi = sapi;
    mCommandBit = commandBit;
    clearCounters();
    transitionTo(LAPDmState::LinkReleased);
}

void LAPDmEntity::receiveFrame(std::span<const uint8_t> frameBytes) {
    // Decode frame in-place over input span — zero allocation.
    auto result = lapdm::LAPDmFrame::decode(frameBytes);
    if (!result) return; // Drop unparseable frames silently

    ++mFramesReceived;
    const auto& frame = *result;

    // Drop frames addressed to a different SAPI: one entity serves one SAPI
    // per logical channel, so a UI frame for another SAPI must never be
    // delivered here.
    if (frame.address.sapi != mSapi) return;

    // FSM dispatch via switch — O(1), no virtual calls.
    switch (frame.format) {
        case lapdm::LAPDmControlFormat::I_Format:
            receiveIFrame(frame);
            break;
        case lapdm::LAPDmControlFormat::S_Format:
            receiveSFrame(frame);
            break;
        case lapdm::LAPDmControlFormat::U_Format:
            receiveUFrame(frame);
            break;
    }
}

Expected<void> LAPDmEntity::sendUI(SAPI sapi, std::span<const uint8_t> l3Data) {
    auto frame = lapdm::makeUIFrame(sapi, mCommandBit, l3Data);
    auto encoded = encodeToTxBuf(frame);
    sendFrame(encoded);
    return Expected<void>::hold();
}

Expected<void> LAPDmEntity::sendData(std::span<const uint8_t> l3Data) {
    // Require established link.
    if (mState != LAPDmState::LinkEstablished &&
        mState != LAPDmState::ContentionResolution) {
        return Expected<void>::error(
            ParseError(ParseError::Code::InvalidValue, "Link not established"));
    }

    if (l3Data.empty()) {
        return Expected<void>::error(
            ParseError(ParseError::Code::TruncatedInput, "Empty data"));
    }

    // Append the full message to the TX queue (lazy allocation; capacity is
    // reused across calls). Segments are transmitted one at a time under the
    // k=1 constraint: trySendNextSegment() sends the first segment when the
    // line is free; otherwise the data waits until the outstanding frame is
    // acknowledged (processAck drains the queue).
    size_t oldSize = mTxQueue.size();
    mTxQueue.resize(oldSize + l3Data.size());
    std::copy(l3Data.begin(), l3Data.end(), mTxQueue.begin() + oldSize);
    mTxMsgEnds.push_back(mTxQueue.size());
    trySendNextSegment();

    return Expected<void>::hold();
}

Expected<void> LAPDmEntity::sendSABME(std::span<const uint8_t> info) {
    if (mState != LAPDmState::LinkReleased) {
        return Expected<void>::error(
            ParseError(ParseError::Code::InvalidValue, "Not in LinkReleased state"));
    }

    // The command is retransmitted verbatim on T200 expiry: remember the
    // contention-resolution information it carries (GSM 04.06 section 5.4.1).
    mPendingInfo.assign(info.begin(), info.end());
    auto frame = lapdm::makeSABMEFrame(mSapi, mCommandBit, info);
    auto encoded = encodeToTxBuf(frame);
    saveForRetransmission(PendingKind::Sabme, encoded);
    transitionTo(LAPDmState::AwaitingEstablish);
    return Expected<void>::hold();
}

Expected<void> LAPDmEntity::sendDISC() {
    if (mState != LAPDmState::LinkEstablished &&
        mState != LAPDmState::ContentionResolution) {
        return Expected<void>::error(
            ParseError(ParseError::Code::InvalidValue, "Cannot DISC from current state"));
    }

    auto frame = lapdm::makeDISCFrame(mSapi, mCommandBit);
    auto encoded = encodeToTxBuf(frame);
    saveForRetransmission(PendingKind::Disc, encoded);
    transitionTo(LAPDmState::AwaitingRelease);
    return Expected<void>::hold();
}

void LAPDmEntity::hardRelease() noexcept {
    clearCounters();
    mEstablishmentInProgress = false;
    transitionTo(LAPDmState::LinkReleased);
}

bool LAPDmEntity::tickT200(std::chrono::milliseconds elapsed) {
    if (!mT200Active) return false;

    if (elapsed.count() >= static_cast<int64_t>(mT200RemainingMs)) {
        mT200RemainingMs = 0;
    } else {
        mT200RemainingMs -= static_cast<uint32_t>(elapsed.count());
        return false;
    }

    // Timer expired. The retransmission budget is N200+1 frames after the
    // original transmission; when the counter exceeds N200 the link is
    // abnormally released (GSM 04.06 section 5.4.1.3).
    mT200Active = false;

    if (mRC <= mProfile.n200) {
        // Retransmit the outstanding frame, rebuilt with P/F set and the
        // current V(R) in N(R) for an I-frame (see retransmitPending()).
        retransmitPending();
        ++mRC;
        ++mRetransmissions;
        // Restart T200.
        mT200Active = true;
        mT200RemainingMs = mProfile.t200Ms;
        return true;
    }

    // Exceeded the retransmission budget — abnormal release.
    abnormalRelease();
    return true;
}

LAPDmState LAPDmEntity::state() const noexcept { return mState; }
SAPI LAPDmEntity::sapi() const noexcept { return mSapi; }

bool LAPDmEntity::isEstablished() const noexcept {
    return mState == LAPDmState::LinkEstablished ||
           mState == LAPDmState::ContentionResolution;
}

unsigned LAPDmEntity::framesSent() const noexcept { return mFramesSent; }
unsigned LAPDmEntity::framesReceived() const noexcept { return mFramesReceived; }
unsigned LAPDmEntity::retransmissions() const noexcept { return mRetransmissions; }

bool LAPDmEntity::hasOutstandingFrame() const noexcept {
    return mVS != mVA;
}

void LAPDmEntity::resetStats() noexcept {
    mFramesSent = 0;
    mFramesReceived = 0;
    mRetransmissions = 0;
}

// ── Internal helpers ──────────────────────────────────────────────────

std::span<const uint8_t> LAPDmEntity::encodeToTxBuf(const lapdm::LAPDmFrame& frame) {
    // Every format-B frame is exactly address + control + header octet,
    // plus the info field when present.
    size_t needed = 3 + frame.info.size();
    if (mTxBuf.size() < needed) mTxBuf.resize(needed);
    size_t n = lapdm::encodeFrameToBuffer(frame, mTxBuf.data(), mTxBuf.size());
    return std::span<const uint8_t>(mTxBuf.data(), n);
}

void LAPDmEntity::sendFrame(std::span<const uint8_t> frameBytes) {
    if (mL1Callback) {
        mL1Callback(frameBytes, mCallbackCtx);
    }
    ++mFramesSent;
}

void LAPDmEntity::saveForRetransmission(PendingKind kind, std::span<const uint8_t> frameBytes) {
    mPending = kind;
    mRC = 0;
    sendFrame(frameBytes);
    mT200Active = true;
    mT200RemainingMs = mProfile.t200Ms;
}

void LAPDmEntity::retransmitPending() {
    switch (mPending) {
        case PendingKind::IFrame: {
            // An expired T200 retransmits the outstanding I-frame with P/F set
            // and the current V(R) in N(R); N(S) is unchanged (GSM 04.06 section
            // 5.5; TS 51.010-1 timer recovery).
            auto frame = lapdm::makeIFrame(mSapi, mCommandBit, mVR, mPendingNs,
                                           true, mPendingMore, mPendingInfo);
            sendFrame(encodeToTxBuf(frame));
            break;
        }
        case PendingKind::Sabme: {
            auto frame = lapdm::makeSABMEFrame(mSapi, mCommandBit, mPendingInfo);
            sendFrame(encodeToTxBuf(frame));
            break;
        }
        case PendingKind::Disc: {
            auto frame = lapdm::makeDISCFrame(mSapi, mCommandBit);
            sendFrame(encodeToTxBuf(frame));
            break;
        }
        case PendingKind::None:
            break;
    }
}

void LAPDmEntity::clearCounters() noexcept {
    mVS = mVA = mVR = 0;
    mRC = 0;
    mT200Active = false;
    mT200RemainingMs = 0;
    mReassemblyBuffer.clear();
    mTxQueue.clear();
    mTxMsgEnds.clear();
    mTxQueuePos = 0;
    mTxMsgIdx = 0;
    mPending = PendingKind::None;
    mPendingInfo.clear();
}

void LAPDmEntity::transitionTo(LAPDmState newState) noexcept {
    mState = newState;
}

void LAPDmEntity::abnormalRelease() noexcept {
    clearCounters();
    mEstablishmentInProgress = false;
    transitionTo(LAPDmState::LinkReleased);
    deliverL3(Primitive::MDL_ERROR_INDICATION, {});
}

void LAPDmEntity::processAck(uint8_t nr) {
    nr = static_cast<uint8_t>(nr & 0x07u);
    // Window k=1: only V(A) (no progress) or V(S) (acknowledges the single
    // outstanding frame) are meaningful; any other N(R) is stale and is
    // ignored so a retransmission cannot roll V(A) backwards.
    if (nr != mVA && nr != mVS) return;
    mVA = nr;
    if (mVA == mVS) {
        mRC = 0;
        mT200Active = false;
        // Line is free again — continue transmitting queued segments (k=1).
        trySendNextSegment();
    }
}

void LAPDmEntity::deliverL3(Primitive prim, std::span<const uint8_t> data) const {
    if (mL3Callback) {
        mL3Callback(mSapi, prim, data, mCallbackCtx);
    }
}

uint32_t LAPDmEntity::computeChecksum(std::span<const uint8_t> data) {
    uint32_t sum = 0;
    for (auto b : data) {
        sum += static_cast<uint32_t>(b);
    }
    return sum;
}

void LAPDmEntity::trySendNextSegment() {
    // k=1: only one unacknowledged I-frame may be in flight.
    if (mVS != mVA) return;
    if (mTxMsgIdx >= mTxMsgEnds.size()) return;

    size_t msgEnd = mTxMsgEnds[mTxMsgIdx];
    size_t remaining = msgEnd - mTxQueuePos;
    size_t chunkSize = std::min(remaining, mProfile.n201);
    bool moreBit = (remaining > mProfile.n201); // M=1: further segments follow

    buildIFrame(std::span<const uint8_t>(mTxQueue.data() + mTxQueuePos, chunkSize), moreBit);
    mTxQueuePos += chunkSize;

    // Current message fully sent: advance to the next queued message, if any.
    if (mTxQueuePos >= msgEnd) {
        mTxMsgIdx += 1;
        // Queue drained: release the buffer contents (capacity is kept for reuse).
        if (mTxMsgIdx >= mTxMsgEnds.size()) {
            mTxQueue.clear();
            mTxMsgEnds.clear();
            mTxQueuePos = 0;
            mTxMsgIdx = 0;
        }
    }
}

// ── Response frame senders ────────────────────────────────────────────

void LAPDmEntity::sendUA(bool pf) {
    auto frame = lapdm::makeUAFrame(mSapi, pf, std::span<const uint8_t>{});
    auto encoded = encodeToTxBuf(frame);
    sendFrame(encoded);
}

void LAPDmEntity::sendUAWithEcho(std::span<const uint8_t> info, bool pf) {
    // The response F bit mirrors the P/F of the received command.
    auto frame = lapdm::makeUAFrame(mSapi, pf, info);
    auto encoded = encodeToTxBuf(frame);
    sendFrame(encoded);
}

void LAPDmEntity::sendDM(bool pf) {
    auto frame = lapdm::makeDMFrame(mSapi, pf);
    auto encoded = encodeToTxBuf(frame);
    sendFrame(encoded);
}

void LAPDmEntity::sendRR(bool pf) {
    auto frame = lapdm::makeRRFrame(mSapi, mVR, pf);
    auto encoded = encodeToTxBuf(frame);
    sendFrame(encoded);
}

void LAPDmEntity::sendREJ(bool pf) {
    auto frame = lapdm::makeREJFrame(mSapi, mVR, pf);
    auto encoded = encodeToTxBuf(frame);
    sendFrame(encoded);
}

// ── I-frame construction and send ─────────────────────────────────────

void LAPDmEntity::buildIFrame(std::span<const uint8_t> payload, bool more) {
    uint8_t ns = mVS;
    uint8_t nr = mVR;
    // Advance VS after building frame (NS = VS before increment).
    mVS = static_cast<uint8_t>((mVS + 1) & 0x07u);

    // Remember the segment fields before encoding so an expired T200 or a REJ
    // can rebuild and retransmit the outstanding I-frame (GSM 04.06 section
    // 5.5; TS 51.010-1 timer recovery).
    mPendingNs = ns;
    mPendingMore = more;
    mPendingInfo.assign(payload.begin(), payload.end());

    auto frame = lapdm::makeIFrame(mSapi, mCommandBit, nr, ns, false, more, payload);
    auto encoded = encodeToTxBuf(frame);
    saveForRetransmission(PendingKind::IFrame, encoded);
}

// ── U-frame dispatcher (GSM 04.06 5.4) ───────────────────────────────

void LAPDmEntity::receiveUFrame(const lapdm::LAPDmFrame& frame) {
    switch (frame.uType) {
        case lapdm::LAPDmUFrameType::SABME:
            handleSABME(frame);
            break;
        case lapdm::LAPDmUFrameType::UA:
            handleUA(frame);
            break;
        case lapdm::LAPDmUFrameType::DM:
            handleDM(frame);
            break;
        case lapdm::LAPDmUFrameType::DISC:
            handleDISC(frame);
            break;
        case lapdm::LAPDmUFrameType::UI:
            handleUI(frame);
            break;
    }
}

void LAPDmEntity::handleSABME(const lapdm::LAPDmFrame& frame) {
    // A SABME command with the P/F bit cleared is ignored (GSM 04.06 section
    // 5.4.1.2); for an accepted command the response UA mirrors the received
    // P/F value (GSM 04.06 section 5.4.1).

    if (!frame.pf) return;

    switch (mState) {
        case LAPDmState::LinkReleased: {
            // MS-originated initial establishment is only valid on SAPI 0, and
            // the command must carry the contention-resolution information;
            // any other SABME is dropped silently (GSM 04.06 section 5.4.1).
            if (mSapi != SAPI::SAPI0) return;
            if (!frame.hasInfo()) return;
            clearCounters();
            // The initial SABME is accepted: the establishment is in progress
            // until an I/S frame or DISC command arrives (GSM 04.06 section
            // 5.4.1.4).
            mEstablishmentInProgress = true;
            // Contention resolution (GSM 04.06 5.4.1.4): echo the payload and
            // answer with a UA whose F bit mirrors the command.
            mContentionChecksum = computeChecksum(frame.info);
            sendUAWithEcho(frame.info, frame.pf);
            transitionTo(LAPDmState::ContentionResolution);
            deliverL3(Primitive::L3_ESTABLISH_INDICATION, {});
            break;
        }
        case LAPDmState::AwaitingEstablish: {
            // Simultaneous establishment — send UA (F mirrors the command).
            sendUA(frame.pf);
            break;
        }
        case LAPDmState::AwaitingRelease: {
            // Refuse re-establishment during release (DM, F mirrors the
            // command).
            sendDM(frame.pf);
            break;
        }
        case LAPDmState::LinkEstablished: {
            // Guards the establishment latency window (GSM 04.06 section
            // 5.4.2.1): a late SABME carrying contention-resolution
            // information is answered with an echoing UA instead of an
            // abnormal release; without info a plain UA suffices. The state
            // is not changed.
            if (mEstablishmentInProgress) {
                if (frame.hasInfo()) sendUAWithEcho(frame.info, frame.pf);
                else sendUA(frame.pf);
                break;
            }
            if (frame.hasInfo()) {
                // Unexpected SABME with payload — abnormal release.
                abnormalRelease();
            } else {
                // Re-establishment (GSM 04.06 5.6.3).
                sendUA(frame.pf);
                clearCounters();
                // Stay in LinkEstablished.
            }
            break;
        }
        case LAPDmState::ContentionResolution: {
            if (frame.hasInfo() && computeChecksum(frame.info) == mContentionChecksum) {
                sendUAWithEcho(frame.info, frame.pf);
                transitionTo(LAPDmState::LinkEstablished);
            }
            // Otherwise ignore.
            break;
        }
        case LAPDmState::Unused:
            // Ignore SABME before open().
            break;
    }
}

void LAPDmEntity::handleUA(const lapdm::LAPDmFrame& frame) {
    // A UA response with F cleared is ignored (GSM 04.06 section 5.4.1.2);
    // our command was sent with P/F set, so only a final response confirms
    // the link.

    if (!frame.pf) return;

    switch (mState) {
        case LAPDmState::AwaitingEstablish: {
            clearCounters();
            transitionTo(LAPDmState::LinkEstablished);
            deliverL3(Primitive::L3_ESTABLISH_CONFIRM, {});
            break;
        }
        case LAPDmState::AwaitingRelease: {
            clearCounters();
            transitionTo(LAPDmState::LinkReleased);
            deliverL3(Primitive::L3_RELEASE_CONFIRM, {});
            break;
        }
        default:
            // UA in other states — unexpected, ignore.
            break;
    }
}

void LAPDmEntity::handleDM(const lapdm::LAPDmFrame& frame) {
    // A received DM with the F bit cleared is ignored (GSM 04.06 section
    // 5.4.6.3).

    if (!frame.pf) return;

    switch (mState) {
        case LAPDmState::AwaitingEstablish: {
            // An unsolicited DM during establishment does not release the
            // link: T200 is restarted and a RELEASE_INDICATION is passed up
            // (GSM 04.06 section 5.4.1.2); SABME retransmissions on T200
            // expiry continue.
            mT200Active = true;
            mT200RemainingMs = mProfile.t200Ms;
            deliverL3(Primitive::L3_RELEASE_INDICATION, {});
            break;
        }
        case LAPDmState::AwaitingRelease: {
            clearCounters();
            transitionTo(LAPDmState::LinkReleased);
            deliverL3(Primitive::L3_RELEASE_CONFIRM, {});
            break;
        }
        case LAPDmState::LinkEstablished:
        case LAPDmState::ContentionResolution: {
            // Remote side disconnected — start T200 for recovery.
            mT200Active = true;
            mT200RemainingMs = mProfile.t200Ms;
            deliverL3(Primitive::L3_RELEASE_INDICATION, {});
            break;
        }
        default:
            // DM in LinkReleased/Unused — ignore.
            break;
    }
}

void LAPDmEntity::handleDISC(const lapdm::LAPDmFrame& frame) {
    // A DISC command is accepted regardless of its P/F bit (lenient acceptance
    // for interoperability, GSM 04.06 section 5.4); every UA and DM response
    // mirrors the received P/F value.

    mEstablishmentInProgress = false;

    switch (mState) {
        case LAPDmState::LinkReleased: {
            // No link to release — respond with DM (F mirrors the command).
            sendDM(frame.pf);
            break;
        }
        case LAPDmState::AwaitingEstablish: {
            sendUA(frame.pf);
            clearCounters();
            transitionTo(LAPDmState::LinkReleased);
            deliverL3(Primitive::L3_RELEASE_INDICATION, {});
            break;
        }
        case LAPDmState::AwaitingRelease: {
            // Simultaneous release.
            sendUA(frame.pf);
            clearCounters();
            transitionTo(LAPDmState::LinkReleased);
            deliverL3(Primitive::L3_RELEASE_CONFIRM, {});
            break;
        }
        case LAPDmState::LinkEstablished: {
            sendUA(frame.pf);
            clearCounters();
            transitionTo(LAPDmState::LinkReleased);
            deliverL3(Primitive::L3_RELEASE_INDICATION, {});
            break;
        }
        case LAPDmState::ContentionResolution: {
            sendUA(frame.pf);
            clearCounters();
            transitionTo(LAPDmState::LinkReleased);
            deliverL3(Primitive::L3_RELEASE_INDICATION, {});
            break;
        }
        case LAPDmState::Unused:
            // Ignore.
            break;
    }
}

void LAPDmEntity::handleUI(const lapdm::LAPDmFrame& frame) {
    // UI frames are delivered in any state (GSM 04.06 5.2.1).
    deliverL3(Primitive::L3_UNIT_DATA, frame.info);
}

// ── I-frame handler (GSM 04.06 5.5) ──────────────────────────────────

void LAPDmEntity::receiveIFrame(const lapdm::LAPDmFrame& frame) {
    // The first received I-frame completes the establishment latency window
    // (GSM 04.06 section 5.4.1.4).
    mEstablishmentInProgress = false;

    // I-frames only valid in established states.
    if (mState == LAPDmState::ContentionResolution) {
        transitionTo(LAPDmState::LinkEstablished);
    }

    if (mState != LAPDmState::LinkEstablished) return;

    // Acknowledge received frames up to NR-1.
    processAck(frame.iCtrl.nr);

    // Sequence check: NS must equal VR (expected next).
    if (frame.iCtrl.ns != mVR) {
        sendREJ(frame.iCtrl.pf);
        return;
    }

    // Accept frame — advance VR.
    mVR = static_cast<uint8_t>((mVR + 1) & 0x07u);

    // Protocol-error guards for untrusted radio input: an
    // I-frame payload larger than N201, or a reassembly that would exceed
    // the maximum L3 message size, is unrecoverable — abnormal release.
    if (frame.info.size() > mProfile.n201 ||
        mReassemblyBuffer.size() + frame.info.size() > kMaxReassemblyBytes) {
        abnormalRelease();
        return;
    }

    // Append payload to reassembly buffer.
    if (!frame.info.empty()) {
        mReassemblyBuffer.insert(mReassemblyBuffer.end(),
                                 frame.info.begin(), frame.info.end());
    }

    // M-bit check (TS 44.064): M=0 marks the final or only segment, at which
    // point the reassembled message is complete.
    if (!frame.more) {
        deliverL3(Primitive::L3_DATA, mReassemblyBuffer);
        mReassemblyBuffer.clear();
    }

    // Respond with RR.
    sendRR(frame.iCtrl.pf);
}

// ── S-frame handler (GSM 04.06 5.3) ──────────────────────────────────

void LAPDmEntity::receiveSFrame(const lapdm::LAPDmFrame& frame) {
    // The first received S-frame completes the establishment latency window
    // (GSM 04.06 section 5.4.1.4).
    mEstablishmentInProgress = false;

    if (mState == LAPDmState::ContentionResolution) {
        transitionTo(LAPDmState::LinkEstablished);
    }

    if (mState != LAPDmState::LinkEstablished) return;

    switch (frame.sType) {
        case lapdm::LAPDmSFrameType::RR: {
            processAck(frame.nr);
            // If PF=1 on a command, respond with RR.
            if (frame.pf && frame.isCommand()) {
                sendRR(true);
            }
            break;
        }
        case lapdm::LAPDmSFrameType::RNR: {
            // RNR (TS 44.064): the peer cannot accept I-frames for now.
            // Acknowledgment processing is the same as for RR, but no
            // response frame is required.
            processAck(frame.nr);
            break;
        }
        case lapdm::LAPDmSFrameType::REJ: {
            processAck(frame.nr);
            // REJ (GSM 04.06 5.3.3): the peer requests retransmission
            // starting from NR. With the k=1 constraint the only outstanding
            // frame is the pending I-frame; retransmit it when it is still
            // unacknowledged instead of waiting for T200 to expire
            // (up to N200*T200 later). The retransmission goes out with P/F
            // set and the current V(R) in N(R), exactly like the T200-expiry
            // path. Retransmissions triggered by REJ count toward the N200
            // budget exactly like T200-expiry retransmissions (tickT200), so a
            // peer cannot force unbounded retransmits by sending REJ
            // repeatedly.
            if (mPending == PendingKind::IFrame && mVA != mVS) {
                if (mRC < mProfile.n200) {
                    retransmitPending();
                    ++mRC;
                    ++mRetransmissions;
                    mT200Active = true;
                    mT200RemainingMs = mProfile.t200Ms;
                } else {
                    abnormalRelease();
                }
            }
            break;
        }
    }
}

} // namespace gsml3parser
