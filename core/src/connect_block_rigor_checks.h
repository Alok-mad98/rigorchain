// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// Post-transaction RES coinbase budget check for ConnectBlock. Stateless with
// respect to Chainstate: only uses chainparams (bootstrap) and the fee / mint
// totals already accumulated for this block.

#ifndef BITCOIN_CONNECT_BLOCK_RIGOR_CHECKS_H
#define BITCOIN_CONNECT_BLOCK_RIGOR_CHECKS_H

#include <consensus/amount.h>

class BlockValidationState;
class CChainParams;

namespace ConnectBlockRigor {

/**
 * CVGE / RIGOR / RES coinbase layout invariants (returns false + state on failure).
 *
 * `res_mint_coinbase` is W(h) when this block's coinbase carries an accepted
 * `RIGOR_MARKER_RES_MINT` and 0 otherwise; `rigor::ConnectEvaluateCoinbaseResMint` has already
 * bound it to the parent's winner. It is a third term of the coinbase RES budget, not
 * a licence to pay RES: any coinbase RES beyond
 * `nFeesRes + bootstrap + res_mint_coinbase` is `bad-cb-res-amount`.
 * PCORB carriers are consensus-invalid (`bad-pcorb-carrier`).
 *
 * SECURITY-001-A9-C dropped the `blockman` and `res_reward_expected` parameters along with the
 * `bad-convergence-window-reset` rule they served; TASK-010 D-5 dropped the unused
 * `expected_draw_candidate_root` argument (DCR is an observer index field, not a coinbase
 * predicate). TASK-010 R-8 dropped the unused `block` / `pindex` arguments: this function
 * is a coinbase RES budget check, not a draw-set walk.
 */
bool VerifyCoinbaseConvergenceAndResLayout(BlockValidationState& state,
    const CChainParams& params,
    CAmount nFeesRes,
    CAmount res_coinbase_value,
    CAmount res_mint_coinbase);

} // namespace ConnectBlockRigor

#endif
