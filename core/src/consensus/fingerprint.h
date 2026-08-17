// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_FINGERPRINT_H
#define BITCOIN_CONSENSUS_FINGERPRINT_H

#include <uint256.h>

#include <cstddef>
#include <cstdint>

/**
 * Consensus Fingerprint — verification identity layer only.
 *
 * This API is for observability, explorer integration, and independent
 * (zero-trust) reproduction of a published draw short ID. It is NOT a consensus
 * rule, NOT a block-acceptance predicate, MUST NOT be used to auto-reject
 * peers, and MUST NOT be called from validation, PoW, RES issuance, fee, or
 * wallet-accounting paths.
 *
 * ConsensusFingerprint = Hash256(
 *     ConvergenceRoot || winner_txid || Sigma || height || target_prev)
 *
 * Field order is mandatory and frozen for this library. Encoding is native
 * serialization only (no strings, no JSON in the preimage).
 *
 * Preimage layout (104 bytes, no padding, no separators):
 *
 *     offset  size  field
 *     ------  ----  --------------------------------
 *     0       32    ConvergenceRoot   (uint256 native)
 *     32      32    winner_txid       (uint256 native)
 *     64      32    Sigma             (uint256 native)
 *     96      4     height            (uint32 little-endian)
 *     100     4     target_prev       (uint32 little-endian)
 *
 * Hash256 is the existing RIGOR / Bitcoin double SHA-256 (CHash256).
 */
inline constexpr std::size_t CONSENSUS_FINGERPRINT_INPUT_SIZE = 104;

uint256 ComputeConsensusFingerprint(
    const uint256& convergence_root,
    const uint256& winner_txid,
    const uint256& sigma,
    uint32_t height,
    uint32_t target_prev);

#endif // BITCOIN_CONSENSUS_FINGERPRINT_H
