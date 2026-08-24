// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_CONVERGENCE_ROOT_ELIGIBILITY_H
#define BITCOIN_CONSENSUS_CONVERGENCE_ROOT_ELIGIBILITY_H

#include <uint256.h>

#include <vector>

namespace Consensus {

/**
 * Flat ConvergenceRoot from an eligibility **txid list** (already the set **F** in arbitrary order).
 *
 * Pure: sort (`uint256` / memcmp) then Hash256(concat). Empty F (`n == 0`) returns 32 zero
 * bytes before hashing. A singleton hashes. Byte-level rules live with the implementation
 * (`convergence_root_eligibility.cpp`).
 *
 * This is the correspondence target for the test-only σ/root CLI. Do **not** use
 * `ComputeConvergenceRootFromConvergenceSet` for that gate (coin-view dependent).
 */
uint256 ComputeConvergenceRootFromEligibilityTxids(std::vector<uint256> convergence_txids) noexcept;

} // namespace Consensus

#endif // BITCOIN_CONSENSUS_CONVERGENCE_ROOT_ELIGIBILITY_H
