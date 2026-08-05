// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_SELECTION_H
#define BITCOIN_CONSENSUS_SELECTION_H

#include <consensus_rigor/sigma_hash.h>
#include <uint256.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Consensus {

/**
 * RFC-000 §6: CONVERGENCE ROOT
 * C = Hash256(concat(sorted(txids)))
 * Sorting: unsigned byte-wise lexicographical order
 */
uint256 ComputeConvergenceRoot(const std::vector<uint256>& txids);

/**
 * RFC-000 §4 (Sigma-PoW-01 revision): SIGMA HASH
 *   σ_i = Hash256(txid_i || C || B_{h-1} || LE32(height))
 * Computes sigma hash for a single transaction.
 *
 * `prev_block_hash` is the hash of block h-1, which committed both `txid_i` and `C`.
 */
uint256 ComputeSigmaHash(const uint256& txid, const uint256& convergence_root, const uint256& prev_block_hash, uint32_t height);

/**
 * RFC-000 §7: WEIGHT DEFINITION
 * f_i MUST be: RES Mint → Mint Cost, RES Spend → transaction fee
 * f_i MUST be integer, f_i MUST >= 1
 */
uint64_t GetWeight(const uint256& txid, const std::vector<uint64_t>& fees);

/**
 * RFC-000 §2: SINGLE INEQUALITY LAW
 * H_i < T · f_i
 * Implementation: H_i * f_j < H_j * f_i
 * Determines if a transaction is eligible
 */
bool IsEligible(const uint256& sigma_hash, const uint256& target, uint64_t weight);

/**
 * RFC-000 §9: TIE-BREAK RULE
 * IF: H_i * f_j == H_j * f_i
 * THEN: txid_i < txid_j → winner
 * Uses direct txid comparison, NOT XOR with C
 */
int SelectWinner(const std::vector<uint256>& txids, const std::vector<uint256>& sigma_hashes, const std::vector<uint64_t>& weights, const uint256& target);

/**
 * RFC-000: Full Selection Algorithm
 * Computes convergence root, sigma hashes, eligibility, and selects winner
 */
struct SelectionResult {
    uint256 convergence_root;
    std::vector<uint256> sigma_hashes;
    std::vector<bool> eligible;
    int winner_index; // -1 if no winner
};

/**
 * RFC-000 §10 + RFC-014 Conv-Decouple-01.
 *
 *  - `txids` / `fees` describe the convergence set committed by block h-1, and
 *    `prev_block_hash` is that block's hash. `height` is the payout height h.
 *  - `res_probability_target` is the inequality RHS source
 *    (`consensus_params.RES_PROBABILITY_TARGET`, floor(2^240 / MIN_CONVERGENCE_AMOUNT)),
 *    and is never derived from difficulty.
 */
SelectionResult RunSelection(const std::vector<uint256>& txids, const std::vector<uint64_t>& fees, uint32_t height, const uint256& prev_block_hash, const uint256& res_probability_target);

} // namespace Consensus

#endif // BITCOIN_CONSENSUS_SELECTION_H
