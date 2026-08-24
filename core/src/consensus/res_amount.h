// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_RES_AMOUNT_H
#define BITCOIN_CONSENSUS_RES_AMOUNT_H

#include <consensus/rigor_consensus_lock.h>

#include <cinttypes>
#include <cstdio>
#include <limits>
#include <string>

/**
 * Res uses a fixed 6-decimal display scale. Internal storage remains int64_t (CAmount).
 *   RES_ATOMIC_UNIT (1)  = 0.000001 RES
 *   RES_COIN (1e6)       = 1.000000 RES
 * ORU continues to use COIN (1e8) per Bitcoin-style amounts, so the two scales stay visually
 * distinct. The separation of the ORU and RES domains is structural (output kind plus
 * `wallet/fee_domain.h`), never magnitude-based, so the display scale carries no consensus
 * safety property and RFC-017 was free to widen it.
 *
 * Canonical numeric locks: `consensus/rigor_consensus_lock.h` (`Consensus::RES_*`, `RES_MAX_SUPPLY`).
 */
using Consensus::RES_ATOMIC_UNIT;
using Consensus::RES_COIN;
using Consensus::RES_MAX_SUPPLY;

/**
 * Smallest valid Res output (0.000001 RES). Outputs below this are unspendable dust.
 * Must stay equal to RES_ATOMIC_UNIT (one display increment).
 */
static constexpr CAmount RES_DUST_THRESHOLD = RES_ATOMIC_UNIT;

/** True if n is a valid non-negative Res amount (whole atomic units). */
inline constexpr bool IsValidResConsensusAmount(CAmount n)
{
    return n >= 0 && n % RES_ATOMIC_UNIT == 0;
}

/** Consensus rule for Res transaction outputs: at least one atomic unit, whole units only. */
inline constexpr bool IsValidResOutputValue(CAmount n)
{
    return n >= RES_DUST_THRESHOLD && n % RES_ATOMIC_UNIT == 0;
}

/** Human-readable Res with exactly six fractional digits (e.g. "12.340000"). */
inline std::string FormatResAmount(CAmount n)
{
    const bool neg = n < 0;
    uint64_t m;
    if (!neg) {
        m = static_cast<uint64_t>(n);
    } else if (n == std::numeric_limits<CAmount>::min()) {
        m = static_cast<uint64_t>(std::numeric_limits<CAmount>::max()) + 1;
    } else {
        m = static_cast<uint64_t>(-n);
    }
    const uint64_t q = m / static_cast<uint64_t>(RES_COIN);
    const uint64_t r = m % static_cast<uint64_t>(RES_COIN);
    char buf[64];
    if (neg) {
        snprintf(buf, sizeof(buf), "-%" PRIu64 ".%06" PRIu64, q, r);
    } else {
        snprintf(buf, sizeof(buf), "%" PRIu64 ".%06" PRIu64, q, r);
    }
    return std::string(buf);
}

#endif // BITCOIN_CONSENSUS_RES_AMOUNT_H
