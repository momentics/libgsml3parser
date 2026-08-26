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
    auto encoded = lapdm::encodeFrame(frame);
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

Expected<void> LAPDmEntity::sendSABME() {
    if (mState != LAPDmState::LinkReleased) {
        return Expected<void>::error(
            ParseError(ParseError::Code::InvalidValue, "Not in LinkReleased state"));
    }

    auto frame = lapdm::makeSABMEFrame(mSapi, mCommandBit, std::span<const uint8_t>{});
    auto encoded = lapdm::encodeFrame(frame);
    saveForRetransmission(encoded);
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
    auto encoded = lapdm::encodeFrame(frame);
    saveForRetransmission(encoded);
    transitionTo(LAPDmState::AwaitingRelease);
    return Expected<void>::hold();
}

void LAPDmEntity::hardRelease() noexcept {
    clearCounters();
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

    // Timer expired.
    mT200Active = false;

    if (mRC < mProfile.n200) {
        // Retransmit pending frame.
        if (!mPendingFrame.empty()) {
            sendFrame(mPendingFrame);
        }
        ++mRC;
        ++mRetransmissions;
        // Restart T200.
        mT200Active = true;
        mT200RemainingMs = mProfile.t200Ms;
        return true;
    }

    // Exceeded max retransmissions — abnormal release.
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

void LAPDmEntity::sendFrame(std::span<const uint8_t> frameBytes) {
    if (mL1Callback) {
        mL1Callback(frameBytes, mCallbackCtx);
    }
    ++mFramesSent;
}

void LAPDmEntity::saveForRetransmission(std::span<const uint8_t> frameBytes) {
    mPendingFrame.assign(frameBytes.begin(), frameBytes.end());
    mRC = 0;
    sendFrame(frameBytes);
    mT200Active = true;
    mT200RemainingMs = mProfile.t200Ms;
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
    mPendingFrame.clear();
}

void LAPDmEntity::transitionTo(LAPDmState newState) noexcept {
    mState = newState;
}

void LAPDmEntity::abnormalRelease() noexcept {
    clearCounters();
    transitionTo(LAPDmState::LinkReleased);
    deliverL3(Primitive::MDL_ERROR_INDICATION, {});
}

void LAPDmEntity::processAck(uint8_t nr) {
    // NR indicates the next frame the sender of this frame expects to receive.
    // All frames with NS < nr are acknowledged. mVA tracks what peer has confirmed.
    mVA = nr & 0x07u;
    // If all sent frames confirmed, stop T200.
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
    bool mBit = (remaining <= mProfile.n201); // M=1: Message complete (last segment)

    buildIFrame(std::span<const uint8_t>(mTxQueue.data() + mTxQueuePos, chunkSize), mBit);
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
    auto encoded = lapdm::encodeFrame(frame);
    sendFrame(encoded);
}

void LAPDmEntity::sendUAWithEcho(std::span<const uint8_t> info) {
    auto frame = lapdm::makeUAFrame(mSapi, true, info);
    auto encoded = lapdm::encodeFrame(frame);
    sendFrame(encoded);
}

void LAPDmEntity::sendDM(bool pf) {
    auto frame = lapdm::makeDMFrame(mSapi, pf);
    auto encoded = lapdm::encodeFrame(frame);
    sendFrame(encoded);
}

void LAPDmEntity::sendRR(bool pf) {
    auto frame = lapdm::makeRRFrame(mSapi, mVR, pf);
    auto encoded = lapdm::encodeFrame(frame);
    sendFrame(encoded);
}

void LAPDmEntity::sendREJ(bool pf) {
    auto frame = lapdm::makeREJFrame(mSapi, mVR, pf);
    auto encoded = lapdm::encodeFrame(frame);
    sendFrame(encoded);
}

// ── I-frame construction and send ─────────────────────────────────────

void LAPDmEntity::buildIFrame(std::span<const uint8_t> payload, bool isLast) {
    uint8_t ns = mVS;
    uint8_t nr = mVR;
    // Advance VS after building frame (GSM 04.06: NS = VS before increment).
    mVS = static_cast<uint8_t>((mVS + 1) & 0x07u);

    auto frame = lapdm::makeIFrame(mSapi, mCommandBit, nr, ns, false, isLast, payload);
    auto encoded = lapdm::encodeFrame(frame);
    saveForRetransmission(encoded);
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
    // GSM 04.06 5.4.1: SABME with F=0 shall be ignored.
    if (!frame.pf) return;

    switch (mState) {
        case LAPDmState::LinkReleased: {
            clearCounters();
            if (frame.hasInfo()) {
                // Contention resolution (GSM 04.06 5.4.1.4).
                mContentionChecksum = computeChecksum(frame.info);
                sendUAWithEcho(frame.info);
                if (mSapi == SAPI::SAPI0) {
                    transitionTo(LAPDmState::ContentionResolution);
                } else {
                    transitionTo(LAPDmState::LinkEstablished);
                }
                deliverL3(Primitive::L3_ESTABLISH_INDICATION, {});
            } else {
                // Normal link establishment.
                sendUA(true);
                transitionTo(LAPDmState::LinkEstablished);
                deliverL3(Primitive::L3_ESTABLISH_INDICATION, {});
            }
            break;
        }
        case LAPDmState::AwaitingEstablish: {
            // Simultaneous establishment — send UA.
            sendUA(true);
            break;
        }
        case LAPDmState::AwaitingRelease: {
            // Refuse re-establishment during release.
            sendDM(true);
            break;
        }
        case LAPDmState::LinkEstablished: {
            if (frame.hasInfo()) {
                // Unexpected SABME with payload — abnormal release.
                abnormalRelease();
            } else {
                // Re-establishment (GSM 04.06 5.6.3).
                sendUA(true);
                clearCounters();
                // Stay in LinkEstablished.
            }
            break;
        }
        case LAPDmState::ContentionResolution: {
            if (frame.hasInfo() && computeChecksum(frame.info) == mContentionChecksum) {
                sendUAWithEcho(frame.info);
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
    // GSM 04.06 5.4.1.2: UA with F=0 shall be ignored.
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
    if (!frame.pf) return;

    switch (mState) {
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
    if (!frame.pf) return;

    switch (mState) {
        case LAPDmState::LinkReleased: {
            // No link to release — respond with DM.
            sendDM(true);
            break;
        }
        case LAPDmState::AwaitingEstablish: {
            sendUA(true);
            clearCounters();
            transitionTo(LAPDmState::LinkReleased);
            deliverL3(Primitive::L3_RELEASE_INDICATION, {});
            break;
        }
        case LAPDmState::AwaitingRelease: {
            // Simultaneous release.
            sendUA(true);
            clearCounters();
            transitionTo(LAPDmState::LinkReleased);
            deliverL3(Primitive::L3_RELEASE_CONFIRM, {});
            break;
        }
        case LAPDmState::LinkEstablished: {
            sendUA(true);
            clearCounters();
            transitionTo(LAPDmState::LinkReleased);
            deliverL3(Primitive::L3_RELEASE_INDICATION, {});
            break;
        }
        case LAPDmState::ContentionResolution: {
            sendUA(true);
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
    // I-frames only valid in established states.
    if (mState == LAPDmState::ContentionResolution) {
        transitionTo(LAPDmState::LinkEstablished);
    }

    if (mState != LAPDmState::LinkEstablished) return;

    // Acknowledge received frames up to NR-1.
    processAck(frame.nr);

    // Sequence check: NS must equal VR (expected next).
    if (frame.ns != mVR) {
        sendREJ(frame.pf);
        return;
    }

    // Accept frame — advance VR.
    mVR = static_cast<uint8_t>((mVR + 1) & 0x07u);

    // Append payload to reassembly buffer.
    if (!frame.info.empty()) {
        mReassemblyBuffer.insert(mReassemblyBuffer.end(),
                                 frame.info.begin(), frame.info.end());
    }

    // M-bit check (GSM 04.06 5.5.2): M=1 means Message complete (last segment).
    if (frame.m) {
        deliverL3(Primitive::L3_DATA, mReassemblyBuffer);
        mReassemblyBuffer.clear();
    }

    // Respond with RR.
    sendRR(frame.pf);
}

// ── S-frame handler (GSM 04.06 5.3) ──────────────────────────────────

void LAPDmEntity::receiveSFrame(const lapdm::LAPDmFrame& frame) {
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
        case lapdm::LAPDmSFrameType::REJ: {
            processAck(frame.nr);
            // Stop sending data until upper layer signals readiness.
            break;
        }
    }
}

} // namespace gsml3parser
