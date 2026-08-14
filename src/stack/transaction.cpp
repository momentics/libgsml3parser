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

#include "gsml3parser/stack/transaction.h"

#include "gsml3parser/visitor.h"

namespace gsml3parser {

// ── Transaction ────────────────────────────────────────────────────────

Transaction::Transaction(L3PD pd, int mti, uint8_t ti, L3TimerId timerId)
    : mPd(pd)
    , mMti(mti)
    , mTi(ti)
    , mTimerId(timerId)
    , mCreatedAt(std::chrono::steady_clock::now())
{
}

L3PD Transaction::requestPD() const noexcept {
    return mPd;
}

int Transaction::requestMTI() const noexcept {
    return mMti;
}

uint8_t Transaction::ti() const noexcept {
    return mTi;
}

L3TimerId Transaction::timerId() const noexcept {
    return mTimerId;
}

TransactionState Transaction::state() const noexcept {
    return mState;
}

void Transaction::complete() noexcept {
    mState = TransactionState::Completed;
}

void Transaction::expire() noexcept {
    mState = TransactionState::Expired;
}

void Transaction::cancel() noexcept {
    mState = TransactionState::Cancelled;
}

bool Transaction::matches(const ParsedMessage& msg, uint8_t ti) const {
    if (mState != TransactionState::Pending) return false;

    L3PD pd = messagePD(msg);
    if (pd == L3PD::CallControl || pd == L3PD::NonCallSS) {
        return mTi == ti;
    }

    return mPd == pd && mMti == messageMTI(msg);
}

bool Transaction::matches(const ParsedMessage& msg) const {
    if (mState != TransactionState::Pending) return false;

    L3PD pd = messagePD(msg);
    return mPd == pd && mMti == messageMTI(msg);
}

std::chrono::steady_clock::time_point Transaction::createdAt() const noexcept {
    return mCreatedAt;
}

// ── TransactionManager ─────────────────────────────────────────────────

void TransactionManager::rebuildTiIndex() noexcept {
    for (uint8_t i = 0; i < 8; ++i) {
        mTiIndex[i] = std::nullopt;
    }

    for (size_t slot = 0; slot < MAX_TRANSACTIONS; ++slot) {
        if (!mOccupied[slot]) continue;
        Transaction& tx = mTransactions[slot];
        if (tx.state() != TransactionState::Pending) continue;

        L3PD pd = tx.requestPD();
        if (pd == L3PD::CallControl || pd == L3PD::NonCallSS) {
            uint8_t ti = tx.ti();
            if (ti < 8 && !mTiIndex[ti]) {
                mTiIndex[ti] = slot;
            }
        }
    }
}

std::optional<size_t> TransactionManager::findSlot() noexcept {
    for (size_t i = 0; i < MAX_TRANSACTIONS; ++i) {
        if (!mOccupied[i]) return i;
    }

    // Compact: move pending transactions to the front, clearing finished ones.
    size_t write = 0;
    for (size_t read = 0; read < MAX_TRANSACTIONS; ++read) {
        if (!mOccupied[read]) continue;
        if (mTransactions[read].state() == TransactionState::Pending) {
            if (write != read) {
                mTransactions[write] = mTransactions[read];
                mOccupied[write] = true;
                mOccupied[read] = false;
            }
            ++write;
        } else {
            mOccupied[read] = false;
            --mCount;
        }
    }

    while (write < MAX_TRANSACTIONS) {
        mOccupied[write] = false;
        ++write;
    }

    rebuildTiIndex();

    if (mCount < MAX_TRANSACTIONS) {
        for (size_t i = 0; i < MAX_TRANSACTIONS; ++i) {
            if (!mOccupied[i]) return i;
        }
    }

    return std::nullopt;
}

std::optional<uint32_t> TransactionManager::create(L3PD pd, int mti, uint8_t ti, L3TimerId timerId) {
    size_t slot = findSlot().value_or(MAX_TRANSACTIONS);
    if (slot >= MAX_TRANSACTIONS) return std::nullopt;

    uint32_t id = mNextId++;
    mTransactions[slot] = Transaction(pd, mti, ti, timerId);
    mOccupied[slot] = true;
    ++mCount;

    // Update TI index for CC/SS transactions.
    if (pd == L3PD::CallControl || pd == L3PD::NonCallSS) {
        if (ti < 8) {
            mTiIndex[ti] = slot;
        }
    }

    return id;
}

Transaction* TransactionManager::get(uint32_t id) noexcept {
    uint32_t idx = id - 1;
    if (idx >= MAX_TRANSACTIONS) return nullptr;
    size_t slot = static_cast<size_t>(idx);
    if (!mOccupied[slot]) return nullptr;
    if (mTransactions[slot].state() != TransactionState::Pending) return nullptr;
    return &mTransactions[slot];
}

Transaction* TransactionManager::match(const L3Header& header, const ParsedMessage& msg) {
    L3PD pd = header.pd;

    if (pd == L3PD::CallControl || pd == L3PD::NonCallSS) {
        // O(1) lookup by TI.
        uint8_t ti = static_cast<uint8_t>(header.ti);
        if (ti < 8) {
            if (auto slotOpt = mTiIndex[ti]; slotOpt) {
                size_t slot = *slotOpt;
                if (mOccupied[slot] &&
                    mTransactions[slot].state() == TransactionState::Pending &&
                    mTransactions[slot].requestPD() == pd) {
                    return &mTransactions[slot];
                }
            }
        }
        return nullptr;
    }

    // For non-CC/SS: linear scan matching PD + MTI.
    for (size_t i = 0; i < MAX_TRANSACTIONS; ++i) {
        if (!mOccupied[i]) continue;
        Transaction& tx = mTransactions[i];
        if (tx.state() == TransactionState::Pending && tx.matches(msg)) {
            return &tx;
        }
    }
    return nullptr;
}

Transaction* TransactionManager::match(const ParsedMessage& msg) {
    L3PD pd = messagePD(msg);

    if (pd == L3PD::CallControl || pd == L3PD::NonCallSS) {
        // Without TI, scan all pending CC/SS transactions.
        for (size_t i = 0; i < MAX_TRANSACTIONS; ++i) {
            if (!mOccupied[i]) continue;
            Transaction& tx = mTransactions[i];
            if (tx.state() == TransactionState::Pending && tx.requestPD() == pd) {
                return &tx;
            }
        }
        return nullptr;
    }

    // For non-CC/SS: scan matching PD + MTI.
    for (size_t i = 0; i < MAX_TRANSACTIONS; ++i) {
        if (!mOccupied[i]) continue;
        Transaction& tx = mTransactions[i];
        if (tx.state() == TransactionState::Pending && tx.matches(msg)) {
            return &tx;
        }
    }
    return nullptr;
}

void TransactionManager::onTimerExpired(L3TimerId timerId) noexcept {
    for (size_t i = 0; i < MAX_TRANSACTIONS; ++i) {
        if (!mOccupied[i]) continue;
        Transaction& tx = mTransactions[i];
        if (tx.state() == TransactionState::Pending && tx.timerId() == timerId) {
            tx.expire();
        }
    }
}

size_t TransactionManager::cleanup() noexcept {
    size_t removed = 0;
    for (size_t i = 0; i < MAX_TRANSACTIONS; ++i) {
        if (!mOccupied[i]) continue;
        if (mTransactions[i].state() != TransactionState::Pending) {
            mOccupied[i] = false;
            --mCount;
            ++removed;
        }
    }
    rebuildTiIndex();
    return removed;
}

size_t TransactionManager::pendingCount() const noexcept {
    size_t count = 0;
    for (size_t i = 0; i < MAX_TRANSACTIONS; ++i) {
        if (mOccupied[i] && mTransactions[i].state() == TransactionState::Pending) {
            ++count;
        }
    }
    return count;
}

size_t TransactionManager::totalCount() const noexcept {
    return mCount;
}

} // namespace gsml3parser
