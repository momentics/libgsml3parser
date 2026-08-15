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

/// LAPDm state machine entity (GSM 04.06).
/// Provides a full protocol implementation with SABME/UA/DISC link management,
/// I-frame segmentation and reassembly, T200 timer with retransmission,
/// and contention resolution. Single-threaded design with zero-allocation
/// callbacks following the FlatHandler pattern.
///
/// Reference: GSM 04.06 / 3GPP TS 45.006
#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <span>
#include <vector>

#include "gsml3parser/expected.h"
#include "gsml3parser/lapdm_frame.h"
#include "gsml3parser/types.h"

namespace gsml3parser {

/// LAPDm protocol state machine states (GSM 04.06 3.5.2).
enum class LAPDmState : uint8_t {
    Unused = 0,               ///< Initial state before open()
    LinkReleased = 1,         ///< No link, idle
    AwaitingEstablish = 2,    ///< Waiting for UA response to SABME
    AwaitingRelease = 3,      ///< Waiting for UA response to DISC
    LinkEstablished = 4,      ///< Normal data transfer (ABM mode)
    ContentionResolution = 5  ///< Contention resolution phase (SAPI0 only)
};

/// Stream operator for LAPDmState (debug output).
std::ostream& operator<<(std::ostream& os, LAPDmState state);

/// Channel-specific LAPDm parameters (GSM 04.06 3.5).
/// Each logical channel type has different N201 (max payload), N200 (max
/// retransmissions), and T200 (ACK timer) values.
struct LAPDmChannelProfile {
    size_t n201;       ///< Max I-frame payload in octets (SDCCH=20, SACCH=18, FACCH=20)
    unsigned n200;     ///< Max retransmissions before abnormal release
    uint32_t t200Ms;   ///< T200 timer value in milliseconds

    /// SDCCH channel profile: N201=20, N200=23, T200=900ms.
    [[nodiscard]] static LAPDmChannelProfile SDCCH() noexcept;

    /// SACCH channel profile: N201=18, N200=5, T200=3600ms.
    [[nodiscard]] static LAPDmChannelProfile SACCH() noexcept;

    /// FACCH channel profile: N201=20, N200=34, T200=900ms.
    [[nodiscard]] static LAPDmChannelProfile FACCH() noexcept;
};

/// LAPDmEntity -- full LAPDm protocol state machine.
///
/// One instance per SAPI per logical channel. Single-threaded: no mutex or
/// atomic primitives. Callbacks use raw function pointers + void* context
/// (FlatHandler-style) for zero heap allocation on the hot path.
///
/// State diagram:
///   Unused ──open()──> LinkReleased
///                    │  ^
///              SABME │  │ DISC/UA
///               from │  │
///                MS  │  │
///                    ▼  │
///           AwaitingEstablish ──UA──> LinkEstablished
///                    │                      │
///               T200 expiry                  │ sendDISC()
///                    │                      ▼
///                    ▼             AwaitingRelease ──UA──> LinkReleased
///               LinkReleased                          │
///                                                   T200 expiry
///                                                        ▼
///                                                   LinkReleased
class LAPDmEntity {
public:
    /// Called when a complete L3 message is received (from UI or reassembled I-frames).
    using L3ReceiveFn = void (*)(SAPI sapi, Primitive prim, std::span<const uint8_t> l3Data, void* ctx);

    /// Called when the entity wants to send a frame to L1/PHY.
    using L1TransmitFn = void (*)(std::span<const uint8_t> frameBytes, void* ctx);

    /// Construct LAPDmEntity with channel profile and zero-allocation callbacks.
    /// Raw function pointers + void* context — zero heap allocation per instance.
    /// @param profile Channel-specific parameters (N201, N200, T200).
    /// @param l3Cb Callback for delivering L3 messages to upper layer.
    /// @param l1Cb Callback for sending encoded frames to lower layer.
    /// @param ctx Shared context pointer passed to both callbacks.
    LAPDmEntity(LAPDmChannelProfile profile, L3ReceiveFn l3Cb, L1TransmitFn l1Cb, void* ctx = nullptr);

    /// Open the entity and transition to LinkReleased state.
    /// @param sapi Service Access Point Indicator for this channel.
    /// @param commandBit true for BTS side (C/R=1), false for MS side (C/R=0).
    void open(SAPI sapi, bool commandBit) noexcept;

    /// Receive a raw LAPDm frame from L1/PHY and process through the FSM.
    /// Zero heap allocation on hot path: decodes frame in-place over input span,
    /// dispatches via switch (O(1), no vtable, no hash map).
    /// @param frameBytes Complete encoded LAPDm frame bytes.
    void receiveFrame(std::span<const uint8_t> frameBytes);

    /// Send L3 data via UI frame (unacknowledged) — GSM 04.06 5.2.1.
    /// Works in any state; does not require link establishment.
    /// @param sapi Target SAPI.
    /// @param l3Data L3 message bytes to encapsulate.
    /// @return Success, or error if encoding fails.
    [[nodiscard]] Expected<void> sendUI(SAPI sapi, std::span<const uint8_t> l3Data);

    /// Send L3 data via I-frames (acknowledged, segmented if needed) — GSM 04.06 5.5.2.
    /// Requires LinkEstablished or ContentionResolution state. Respects k=1 constraint:
    /// only one unacknowledged I-frame in flight at a time.
    /// @param l3Data L3 message bytes to send (may exceed N201, will be segmented).
    /// @return Success, or error if link not established or frame outstanding.
    [[nodiscard]] Expected<void> sendData(std::span<const uint8_t> l3Data);

    /// Initiate link establishment by sending SABME — GSM 04.06 5.4.1.
    /// Transitions to AwaitingEstablish state. Requires LinkReleased state.
    /// @return Success, or error if not in LinkReleased state.
    [[nodiscard]] Expected<void> sendSABME();

    /// Initiate normal link release by sending DISC — GSM 04.06 5.4.4.
    /// Transitions to AwaitingRelease state. Requires LinkEstablished or
    /// ContentionResolution state.
    /// @return Success, or error if not in a valid state for release.
    [[nodiscard]] Expected<void> sendDISC();

    /// Hard release: immediate transition to LinkReleased without sending frames.
    /// Clears all counters and buffers.
    void hardRelease() noexcept;

    /// Tick the T200 timer by the given elapsed duration.
    /// If the timer expires, triggers retransmission (if RC < N200) or abnormal release.
    /// @param elapsed Time elapsed since last tick.
    /// @return true if a retransmission or abnormal release occurred.
    bool tickT200(std::chrono::milliseconds elapsed);

    /// Current protocol state.
    [[nodiscard]] LAPDmState state() const noexcept;

    /// Configured SAPI.
    [[nodiscard]] SAPI sapi() const noexcept;

    /// Returns true if the link is established (LinkEstablished or ContentionResolution).
    [[nodiscard]] bool isEstablished() const noexcept;

    /// Number of frames sent via L1 callback.
    [[nodiscard]] unsigned framesSent() const noexcept;

    /// Number of frames received and processed.
    [[nodiscard]] unsigned framesReceived() const noexcept;

    /// Number of retransmissions triggered by T200 expiry.
    [[nodiscard]] unsigned retransmissions() const noexcept;

    /// Returns true if an I-frame is pending acknowledgment (k=1 constraint).
    [[nodiscard]] bool hasOutstandingFrame() const noexcept;

    /// Reset all protocol counters and statistics.
    void resetStats() noexcept;

private:
    LAPDmChannelProfile mProfile;
    L3ReceiveFn mL3Callback{nullptr};
    L1TransmitFn mL1Callback{nullptr};
    void* mCallbackCtx{nullptr};

    // Protocol state (GSM 04.06 3.5.2)
    LAPDmState mState{LAPDmState::Unused};
    SAPI mSapi{SAPI::SAPI0};
    bool mCommandBit{true};

    // Sequence counters (mod 8) — packed uint8_t for cache efficiency
    uint8_t mVS{0}; // Send state: NS+1 of last sent I-frame
    uint8_t mVA{0}; // Acknowledge state: NR+1 of last acknowledged I-frame
    uint8_t mVR{0}; // Receive state: expected next NS

    // Retransmission buffer — lazy-allocated, empty initially
    std::vector<uint8_t> mPendingFrame;
    unsigned mRC{0}; // Retransmission counter

    // T200 timer
    bool mT200Active{false};
    uint32_t mT200RemainingMs{0};

    // I-frame reassembly buffer — lazy-allocated, reserve(N201*2) on first use
    std::vector<uint8_t> mReassemblyBuffer;

    // Contention resolution checksum
    uint32_t mContentionChecksum{0};

    // Statistics
    unsigned mFramesSent{0};
    unsigned mFramesReceived{0};
    unsigned mRetransmissions{0};

    // ── Internal helpers ──

    /// Send raw frame bytes via L1 callback and increment counter.
    void sendFrame(std::span<const uint8_t> frameBytes);

    /// Save a frame for potential retransmission and start T200 timer.
    void saveForRetransmission(std::span<const uint8_t> frameBytes);

    /// Clear all protocol counters, timers, and buffers.
    void clearCounters() noexcept;

    /// Transition to a new protocol state.
    void transitionTo(LAPDmState newState) noexcept;

    /// Handle abnormal release: clear state and signal error to L3.
    void abnormalRelease() noexcept;

    /// Process acknowledgment from received NR value — GSM 04.06 5.5.3.
    void processAck(uint8_t nr);

    /// Dispatch incoming U-frames based on frame type and current state.
    void receiveUFrame(const lapdm::LAPDmFrame& frame);

    /// Dispatch incoming I-frames: sequence check, reassembly, ACK.
    void receiveIFrame(const lapdm::LAPDmFrame& frame);

    /// Dispatch incoming S-frames (RR, REJ).
    void receiveSFrame(const lapdm::LAPDmFrame& frame);

    /// Handle SABME U-frame.
    void handleSABME(const lapdm::LAPDmFrame& frame);

    /// Handle UA U-frame.
    void handleUA(const lapdm::LAPDmFrame& frame);

    /// Handle DM U-frame.
    void handleDM(const lapdm::LAPDmFrame& frame);

    /// Handle DISC U-frame.
    void handleDISC(const lapdm::LAPDmFrame& frame);

    /// Handle UI U-frame.
    void handleUI(const lapdm::LAPDmFrame& frame);

    /// Send UA response frame — GSM 04.06 5.4.1.2.
    void sendUA(bool pf);

    /// Send UA with echoed payload for contention resolution.
    void sendUAWithEcho(std::span<const uint8_t> info);

    /// Send DM (Disconnected Mode) response frame — GSM 04.06 5.4.6.
    void sendDM(bool pf);

    /// Send RR (Receive Ready) response — GSM 04.06 5.3.2.
    void sendRR(bool pf);

    /// Send REJ (Reject) response — GSM 04.06 5.3.3.
    void sendREJ(bool pf);

    /// Build and send an I-frame for a payload chunk — GSM 04.06 5.5.2.
    /// Advances mVS after building the frame.
    void buildIFrame(std::span<const uint8_t> payload, bool isLast);

    /// Check k=1 constraint: return error if outstanding frame not acknowledged.
    [[nodiscard]] Expected<void> checkOutstanding();

    /// Deliver a complete L3 message to upper layer via callback.
    void deliverL3(Primitive prim, std::span<const uint8_t> data) const;

    /// Compute simple checksum over payload bytes (contention resolution).
    static uint32_t computeChecksum(std::span<const uint8_t> data);
};

/// Entity without dynamic buffers must fit in < 512 bytes.
/// Dynamic buffers (mPendingFrame, mReassemblyBuffer) are lazy-allocated.
static_assert(sizeof(LAPDmEntity) < 512, "LAPDmEntity too large for scale");

} // namespace gsml3parser
