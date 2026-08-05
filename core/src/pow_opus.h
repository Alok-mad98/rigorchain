// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_POW_OPUS_H
#define BITCOIN_POW_OPUS_H

#include <consensus/rigor_consensus_lock.h>

#include <cstddef>
#include <cstdint>
#include <uint256.h>

class CBlockHeader;

/**
 * OpuHash consensus PoW (compute-saturated, L2-cache-sized buffer).
 * Buffer fits typical CPU L2; high round count favors CPU-like serial hashing vs wide GPU memory paths.
 *
 * Canonical numeric parameters live in consensus/rigor_consensus_lock.h.
 */
using Consensus::OPUS_POW_BUFFER_SIZE;
using Consensus::OPUS_POW_MIX_ROUNDS;

/** PoW digest from the full block header (double-SHA256 id, then OpuHash mix). */
uint256 GetOpusHash(const CBlockHeader& block);

/** Same PoW primitive from a precomputed header id (must match GetOpusHash for that header). */
uint256 GetOpusHashFromBaseHash(const uint256& base_hash);

#endif // BITCOIN_POW_OPUS_H
