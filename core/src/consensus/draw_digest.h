// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_DRAW_DIGEST_H
#define BITCOIN_CONSENSUS_DRAW_DIGEST_H

#include <consensus_rigor/sigma_hash.h>
#include <uint256.h>

#include <cstdint>

namespace Consensus {

/**
 * RFC-001 Sigma (Sigma-PoW-01 revision) — canonical convergence-draw digest.
 *
 *     σ_i = Hash256( txid_i || ConvergenceRoot || B_{h-1} || LE32(height) )
 *
 * This is the consensus-locked digest input. See `consensus_rigor/sigma_hash.h`
 * for the byte layout (100 bytes, fixed offsets, RFC-000 §5) and for why every
 * input is sourced from block h-1.
 *
 * `txid_i` and `ConvergenceRoot` describe the convergence set committed by block
 * h-1; `prev_block_hash` is that block's hash; `height` is the payout height h.
 *
 * Cross-difficulty replay is closed by construction: B_{h-1} is a hash of the
 * parent header, which carries the parent's nBits, so σ cannot repeat across
 * difficulties without repeating the parent block itself.
 */
inline uint256 CalculateSigmaHash(const uint256& txid,
                                  const uint256& convergence_root,
                                  const uint256& prev_block_hash,
                                  uint32_t height)
{
    return rigor::ComputeSigmaHash(txid, convergence_root, prev_block_hash, height);
}

} // namespace Consensus

#endif // BITCOIN_CONSENSUS_DRAW_DIGEST_H
