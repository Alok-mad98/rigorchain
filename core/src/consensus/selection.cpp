// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/selection.h>
#include <consensus_rigor/sigma_hash.h>
#include <arith_uint256.h>
#include <arith_uint512.h>
#include <uint256.h>
#include <hash.h>

#include <limits>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace Consensus {

/**
 * RFC-000 §6: CONVERGENCE ROOT
 * C = Hash256(concat(sorted(txids)))
 * Sorting: unsigned byte-wise lexicographical order
 * 
 * Boundary: empty F commits to the all-zero root. Singleton F preserves the
 * existing all-zero root boundary; RFC-015 permits it to compete for RES.
 */
uint256 ComputeConvergenceRoot(const std::vector<uint256>& txids)
{
    // RFC-015 preserves the existing root boundary while allowing N=1 selection.
    if (txids.size() < 2) {
        return uint256::ZERO;
    }

    // Sort using unsigned byte-wise lexicographical order
    std::vector<uint256> sorted_txids = txids;
    std::sort(sorted_txids.begin(), sorted_txids.end());

    // Concatenate all txids
    std::vector<unsigned char> concatenated;
    concatenated.reserve(txids.size() * 32);
    for (const auto& txid : sorted_txids) {
        concatenated.insert(concatenated.end(), txid.begin(), txid.end());
    }

    // Hash256(concat(txids))
    uint256 convergence_root;
    CHash256().Write(concatenated).Finalize(convergence_root);
    return convergence_root;
}

/**
 * RFC-000 §4 (Sigma-PoW-01 revision): SIGMA HASH
 *   σ_i = Hash256(txid_i || C || B_{h-1} || LE32(height))
 */
uint256 ComputeSigmaHash(const uint256& txid, const uint256& convergence_root, const uint256& prev_block_hash, uint32_t height)
{
    return rigor::ComputeSigmaHash(txid, convergence_root, prev_block_hash, height);
}

/**
 * RFC-000 §7: WEIGHT DEFINITION
 * f_i MUST be: RES Mint → Mint Cost, RES Spend → transaction fee
 * f_i MUST be integer, f_i MUST >= 1
 */
uint64_t GetWeight(const uint256& txid, const std::vector<uint64_t>& fees)
{
    // In this simplified test context, fees are passed directly as weights
    // aligned by index in the calling context
    // This function is not used directly in RunSelection
    return 0;
}

/**
 * RFC-000 §2: SINGLE INEQUALITY LAW
 * H_i < T · f_i
 * Implementation: H_i * f_j < H_j * f_i
 * Determines if a transaction is eligible
 * Uses integer-only arithmetic with cross-multiplication
 */
bool IsEligible(const uint256& sigma_hash, const uint256& target, uint64_t weight)
{
    // RFC-000 §7: f_i MUST be integer, f_i MUST >= 1.
    if (weight == 0) {
        return false;
    }

    // RFC-000 §2 / RFC-008 §6: H_i < T · f_i, computed in uint512 to avoid
    // the overflow path that previously short-circuited to `return true`
    // (which silently violated §2 for legitimately-failing inputs).
    const arith_uint512 sigma_arith = arith_uint512::from_uint256(sigma_hash);
    const arith_uint512 target_arith = arith_uint512::from_uint256(target);
    const arith_uint512 weight_arith(weight);
    const arith_uint512 target_times_weight = target_arith * weight_arith;

    return sigma_arith < target_times_weight;
}

/**
 * RFC-000 §9: TIE-BREAK RULE
 * IF: H_i * f_j == H_j * f_i
 * THEN: (txid_i XOR C) < (txid_j XOR C) → winner
 * Uses integer-only arithmetic with cross-multiplication
 *
 * RFC-015 boundary:
 *   IF N == 0: winner = NONE.
 *   IF N >= 1: eligible candidates may compete. With N=1, C remains ZERO
 *   under the preserved ConvergenceRoot boundary, and the single eligible
 *   candidate wins.
 */
int SelectWinner(const std::vector<uint256>& txids, const std::vector<uint256>& sigma_hashes, const std::vector<uint64_t>& weights, const uint256& target)
{
    if (txids.empty()) {
        return -1;
    }

    int winner_index = -1;

    // RFC-008 §7 (MUST): cross-multiplication H_i * f_j vs H_j * f_i MUST be
    // performed in uint512 to avoid overflow. H_i is a full 256-bit sigma
    // digest, and f_j is up to uint64_max; their product reaches 320 bits and
    // would silently truncate when held in arith_uint256. The truncation flips
    // the comparison sign on roughly half of all inputs, e.g. the
    // `weight_priority` golden vector
    // (fees = {1, 1000}, σ ≈ 2^254) selected the wrong winner (0 instead of 1)
    // because σ_0 * 1000 wrapped past 2^256.
    for (size_t i = 0; i < txids.size(); ++i) {
        if (!IsEligible(sigma_hashes[i], target, weights[i])) {
            continue;
        }

        if (winner_index == -1) {
            winner_index = static_cast<int>(i);
            continue;
        }

        // Compare in uint512: (a.H * b.f) ≺ (b.H * a.f) ⇒ a ≺ b.
        const arith_uint512 sigma_i = arith_uint512::from_uint256(sigma_hashes[i]);
        const arith_uint512 sigma_best = arith_uint512::from_uint256(sigma_hashes[winner_index]);
        const arith_uint512 f_i(weights[i]);
        const arith_uint512 f_best(weights[winner_index]);

        const arith_uint512 lhs = sigma_i * f_best;
        const arith_uint512 rhs = sigma_best * f_i;

        if (lhs < rhs) {
            winner_index = static_cast<int>(i);
        } else if (lhs == rhs) {
            // RFC-000 §9: TIE-BREAK RULE
            // IF: H_i * f_j == H_j * f_i
            // THEN: txid_i < txid_j → winner (unsigned byte-wise lexicographic
            // order on the canonical 32-byte txid).
            if (txids[i] < txids[winner_index]) {
                winner_index = static_cast<int>(i);
            }
        }
    }

    return winner_index;
}

/**
 * RFC-000: Full Selection Algorithm
 * Computes convergence root, sigma hashes, eligibility, and selects winner
 */
SelectionResult RunSelection(const std::vector<uint256>& txids, const std::vector<uint64_t>& fees, uint32_t height, const uint256& prev_block_hash, const uint256& res_probability_target)
{
    SelectionResult result;

    result.convergence_root = ComputeConvergenceRoot(txids);

    // Sigma-PoW-01: the seed is the hash of the block that committed this set, so σ
    // cannot be recomputed without redoing that block's proof of work.
    result.sigma_hashes.reserve(txids.size());
    for (const auto& txid : txids) {
        result.sigma_hashes.push_back(ComputeSigmaHash(txid, result.convergence_root, prev_block_hash, height));
    }

    // RFC-014 Conv-Decouple-01: eligibility RHS sources from the
    // constitutional constant `res_probability_target`, not from `target_prev`.
    result.eligible.reserve(txids.size());
    for (size_t i = 0; i < txids.size(); ++i) {
        result.eligible.push_back(IsEligible(result.sigma_hashes[i], res_probability_target, fees[i]));
    }

    result.winner_index = SelectWinner(txids, result.sigma_hashes, fees, res_probability_target);

    return result;
}

} // namespace Consensus
