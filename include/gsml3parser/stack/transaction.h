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

/// Transaction correlation framework for request-response L3 messaging.
///
/// Provides transaction tracking with O(1) TI-based lookup for CC/SS messages
/// and PD+MTI matching for other protocol discriminators. Uses fixed-size arrays
/// internally - zero heap allocation on hot paths.
///
/// Thread safety: NOT thread-safe. One instance per MS, accessed from a single thread.
/// Performance: match() is O(1) for CC/SS via TI index, O(K) for others (K < MAX_TRANSACTIONS).
/// Internal storage is std::array - no dynamic allocation.
///
/// Example:
/// @code
///   TransactionManager tm;
///   auto id = tm.create(L3PD::CallControl, L3Setup::MTI, 1, L3TimerId::T3101);
///   // ... later, when response arrives:
///   auto* tx = tm.match(header, parsedMsg);
///   if (tx) tx->complete();
/// @endcode
#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "gsml3parser/l3header.h"
#include "gsml3parser/message_types.h"
#include "gsml3parser/stack/l3_timer.h"

namespace gsml3parser {

/// State of a pending L3 transaction (request awaiting response).
enum class TransactionState : uint8_t {
    Pending,      /// Waiting for response
    Completed,    /// Response received successfully
    Expired,      /// Timer expired, no response
    Cancelled     /// Manually cancelled
};

/// A single L3 transaction: request metadata for correlating responses.
///
/// IMPORTANT: This class stores ONLY metadata (PD, MTI, TI) - NOT the full
/// ParsedMessage (~8KB variant). Storing ParsedMessage by value in every
/// Transaction would be catastrophic for memory at scale (millions of MS).
/// The caller handles response messages externally.
///
/// 3GPP TS 24.008 - Transaction Identifier (TI) for CC/SS correlation;
/// PD+MTI matching for other protocol layers.
///
/// Memory: sizeof(Transaction) <= 48 bytes, zero heap allocations.
class Transaction {
 public:
    /// Create a new pending transaction.
    /// @param pd Protocol Discriminator of the request message.
    /// @param mti Message Type Indicator of the request message.
    /// @param ti Transaction Identifier from L3 header (0-7 for CC/SS, 0 for others).
    /// @param timerId Timer to associate with this transaction for expiry tracking.
    Transaction(L3PD pd, int mti, uint8_t ti, L3TimerId timerId);

    /// Default constructor for std::array storage in TransactionManager.
    /// Creates an empty transaction; not intended for standalone use.
    Transaction() = default;

    /// Returns the protocol discriminator of the original request.
    [[nodiscard]] L3PD requestPD() const noexcept;

    /// Returns the MTI of the original request.
    [[nodiscard]] int requestMTI() const noexcept;

    /// Returns the transaction identifier from the L3 header.
    [[nodiscard]] uint8_t ti() const noexcept;

    /// Returns the timer ID associated with this transaction.
    [[nodiscard]] L3TimerId timerId() const noexcept;

    /// Returns the current state of the transaction.
    [[nodiscard]] TransactionState state() const noexcept;

    /// Mark transaction as completed. The caller is responsible for handling
    /// the response message externally - it is NOT stored in this object.
    void complete() noexcept;

    /// Mark transaction as expired (timer fired with no response).
    void expire() noexcept;

    /// Cancel the transaction manually.
    void cancel() noexcept;

    /// Check if this transaction matches a given incoming message for CC/SS protocols.
    /// For CallControl or NonCallSS: matches only on TI.
    /// @param msg The incoming parsed message (used to determine PD).
    /// @param ti  The Transaction Identifier from the incoming message's L3 header.
    /// @return True if the transaction is pending and the PD/TI match.
    [[nodiscard]] bool matches(const ParsedMessage& msg, uint8_t ti) const;

    /// Check if this transaction matches a given incoming message for non-CC/SS protocols.
    /// For all PDs except CallControl and NonCallSS: matches on PD + MTI.
    /// @param msg The incoming parsed message.
    /// @return True if the transaction is pending and PD+MTI match.
    [[nodiscard]] bool matches(const ParsedMessage& msg) const;

    /// Returns the creation time point for diagnostics.
    [[nodiscard]] std::chrono::steady_clock::time_point createdAt() const noexcept;

 private:
    L3PD mPd;
    int mMti;
    uint8_t mTi;
    L3TimerId mTimerId;
    TransactionState mState{TransactionState::Pending};
    std::chrono::steady_clock::time_point mCreatedAt;
};

static_assert(sizeof(Transaction) <= 48, "Transaction too large for cache efficiency");

/// Manages pending transactions for one MS.
///
/// Uses fixed-size arrays - zero heap allocation on hot paths. Supports up to
/// MAX_TRANSACTIONS (16) concurrent transactions per MS, which is sufficient for
/// typical BTS workloads where < 4 concurrent transactions are expected.
///
/// 3GPP TS 24.008 - Multiple CC/SS dialogs may be active simultaneously per MS.
///
/// Thread safety: NOT thread-safe. One instance per MS, accessed from a single thread.
/// Performance: O(1) lookup by TI for CC/SS via array[8], O(K) by PD+MTI where K < 16.
/// Internal storage is std::array - no dynamic allocation.
class TransactionManager {
 public:
    TransactionManager() = default;

    /// Create and track a new pending transaction.
    /// @param pd Protocol Discriminator of the request.
    /// @param mti Message Type Indicator of the request message.
    /// @param ti Transaction Identifier (0-7 for CC/SS, 0 for others).
    /// @param timerId Timer to associate with this transaction for expiry tracking.
    /// @return Stable transaction ID on success, std::nullopt if the pool is full.
    ///
    /// ID contract: the ID is the internal slot index plus one (1-based; 0 is
    /// reserved as the invalid sentinel). A transaction never moves between
    /// slots, so its ID stays valid for the entire lifetime of the
    /// transaction. When the pool is full, a slot that still holds a finished
    /// transaction is reused in place; the finished transaction's ID is then
    /// legitimately reassigned (the same way a file descriptor is recycled).
    /// IDs of live transactions are never invalidated by other operations.
    std::optional<uint32_t> create(L3PD pd, int mti, uint8_t ti, L3TimerId timerId);

    /// Get a transaction by its ID.
    /// @param id The transaction ID returned by create(). 0 is never a valid ID.
    /// @return Pointer to the transaction if it is still tracked and pending;
    ///         nullptr if the ID is unknown, the transaction finished, or its
    ///         slot was reused by a newer transaction.
    /// O(1) direct slot lookup - no scanning, no allocation.
    Transaction* get(uint32_t id) noexcept;

    /// Try to match an incoming message against pending transactions using L3 header info.
    /// For CC/SS messages (PD == CallControl or NonCallSS): O(1) lookup by TI from header.
    /// For other PDs: linear scan matching PD + MTI.
    /// @param header The L3Header parsed from the incoming message bytes.
    /// @param msg The fully parsed message.
    /// @return Pointer to the matching pending transaction, or nullptr if none found.
    Transaction* match(const L3Header& header, const ParsedMessage& msg);

    /// Try to match an incoming message against pending transactions using PD+MTI only.
    /// Useful when L3 header information is not available at the call site.
    /// Scans all pending transactions for a PD+MTI match.
    /// @param msg The fully parsed message.
    /// @return Pointer to the first matching pending transaction, or nullptr.
    Transaction* match(const ParsedMessage& msg);

    /// Handle timer expiry: expire all pending transactions using the given timer ID.
    /// @param timerId The timer that has expired.
    /// Marks all pending transactions with this timerId as Expired.
    void onTimerExpired(L3TimerId timerId) noexcept;

    /// Remove completed, expired, and cancelled transactions from internal storage.
    /// @return Number of transactions removed.
    /// Also rebuilds the TI index for remaining pending CC/SS transactions.
    size_t cleanup() noexcept;

    /// Number of currently pending (active) transactions.
    /// @return Count of transactions in Pending state.
    [[nodiscard]] size_t pendingCount() const noexcept;

    /// Total number of tracked transactions (all states, including finished).
    /// @return Total slots occupied in the internal array.
    [[nodiscard]] size_t totalCount() const noexcept;

 private:
    static constexpr size_t MAX_TRANSACTIONS = 16;

    // Fixed-size array - no heap allocation. Each slot holds a transaction.
    std::array<Transaction, MAX_TRANSACTIONS> mTransactions{};

    // Tracks which slots are occupied (have a valid transaction).
    std::array<bool, MAX_TRANSACTIONS> mOccupied{};

    // TI-indexed array for O(1) CC/SS lookup. Each slot holds the slot index
    // of the pending CC/SS transaction with that TI, or nullopt if none.
    std::array<std::optional<size_t>, 8> mTiIndex{};

    size_t mCount{0};

    /// Rebuild the TI index from current pending transactions.
    void rebuildTiIndex() noexcept;

    /// Find a slot for a new transaction without moving any live transaction.
    /// Prefers a completely free slot; if none exists, reuses a slot that
    /// still holds a finished (non-pending) transaction. In-place reuse never
    /// shifts live transactions, so all outstanding transaction IDs remain
    /// valid.
    /// @return The slot index, or std::nullopt if every slot holds a pending transaction.
    std::optional<size_t> findSlot() noexcept;
};

} // namespace gsml3parser
