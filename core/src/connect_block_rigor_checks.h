// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// Post-transaction RIGOR / CVGE / RES coinbase checks for ConnectBlock. Stateless with
// respect to Chainstate: only uses the connected block, its CBlockIndex (height,
// pprev, cached hashes), BlockManager for last-RES lookup, and fee totals already
// accumulated for this block.

#ifndef BITCOIN_CONNECT_BLOCK_RIGOR_CHECKS_H
#define BITCOIN_CONNECT_BLOCK_RIGOR_CHECKS_H

#include <consensus/amount.h>

#include <uint256.h>

class BlockValidationState;
class CBlock;
class CBlockIndex;
class CChainParams;

namespace ConnectBlockRigor {

/**
 * CVGE / RIGOR / RES coinbase layout invariants (returns false + state on failure).
 *
 * SECURITY-001-A9-C dropped the `blockman` and `res_reward_expected` parameters along with the
 * `bad-convergence-window-reset` rule they served; see the removal note in the definition.
 */
bool VerifyCoinbaseConvergenceAndResLayout(BlockValidationState& state,
    const CChainParams& params,
    const CBlock& block,
    CBlockIndex* pindex,
    CAmount nFeesRes,
    CAmount res_coinbase_value,
    const uint256& expected_draw_candidate_root);

} // namespace ConnectBlockRigor

#endif
