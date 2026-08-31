// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_AMOUNT_H
#define BITCOIN_CONSENSUS_AMOUNT_H

#include <cstdint>
#include <limits>

/** Amount in base units (ORU scale: 1 ORU = COIN units; smallest unit "Tharn" = 1 base unit). Can be negative. */
typedef int64_t CAmount;

/** Base units per 1.00 ORU (1e8). Unchanged for consensus compatibility. */
static constexpr CAmount COIN = 100000000;

/** Explicit alias for dual-native docs / call sites (same scale as COIN). RES scale: consensus/res_amount.h. */
static constexpr CAmount ORU_COIN = COIN;

/** Milli-ORU: 1 mORU = COIN/1000 base units (100 000). */
static constexpr CAmount ORU_MILLI = COIN / 1000;

/** No amount larger than this (in base units) is valid as a single output, a
 *  transaction's input-sum, or a running fee/converge accumulator.
 *
 *  This is **not** an ORU supply cap. ORU issuance is uncapped (1000 → 125 ORU
 *  subsidy, then perpetual tail). Bitcoin's 21e6 * COIN bound would start
 *  rejecting legitimate consolidations after ~21 000 blocks. The bound below
 *  is an overflow sanity check: 10 billion ORU, sized against two opposing needs.
 *
 *  Upward: cumulative issuance must stay under it for the life of the chain. At
 *  the 125 ORU/block tail (~6.57e6 ORU/year) 1e10 ORU is roughly 1500 years.
 *
 *  Downward: `AddMoneyOverflows` is the only supported way to accumulate, but
 *  Bitcoin-derived code and third-party re-implementations habitually add first
 *  and range-check after. Leaving 9x of int64 headroom means such a sum cannot
 *  overflow before its own check runs. A bound near CAmount max removes that
 *  margin and makes two in-range amounts unsummable.
 *
 *  Changing it is consensus-critical.
 */
static constexpr CAmount MAX_MONEY = static_cast<CAmount>(10000000000LL) * COIN;
inline bool MoneyRange(const CAmount& nValue) { return (nValue >= 0 && nValue <= MAX_MONEY); }

/**
 * A sum of in-range amounts must not overflow int64 before it can be
 * range-checked. Four addends is far more headroom than any accumulator in the
 * codebase needs, and it is the property `AddMoneyOverflows` rests on.
 */
static_assert(MAX_MONEY > 0 && MAX_MONEY <= std::numeric_limits<int64_t>::max() / 4,
    "MAX_MONEY must leave room to add in-range amounts without signed overflow");

/**
 * True iff `running + addend` would leave the money range. Use this instead of
 * `running += addend; if (!MoneyRange(running))`, which evaluates the overflowing
 * addition before the check can reject it.
 *
 * A negative or oversized operand is itself an overflow for this purpose, so the
 * caller needs no separate `MoneyRange` guard on either side.
 */
inline bool AddMoneyOverflows(const CAmount& running, const CAmount& addend)
{
    if (!MoneyRange(running) || !MoneyRange(addend)) return true;
    return addend > MAX_MONEY - running;
}

#endif // BITCOIN_CONSENSUS_AMOUNT_H
