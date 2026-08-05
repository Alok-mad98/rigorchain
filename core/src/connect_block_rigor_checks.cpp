// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <connect_block_rigor_checks.h>

#include <chain.h>
#include <consensus/convergence_commitment.h>
#include <consensus/convergence_logic.h>
#include <consensus/convergenceres.h>
#include <consensus/params.h>
#include <consensus/validation.h>
#include <kernel/chainparams.h>
#include <logging.h>
#include <util/check.h>
#include <node/devtest_res_bootstrap.h>
#include <primitives/block.h>
#include <primitives/transaction.h>

namespace rigor {
bool CheckRigorDrawTargetCommitment(BlockValidationState& state,
    const CBlock& block,
    const CBlockIndex* pindex,
    const Consensus::Params& consensus_params);
} // namespace rigor

namespace ConnectBlockRigor {

bool VerifyCoinbaseConvergenceAndResLayout(BlockValidationState& state,
    const CChainParams& params,
    const CBlock& block,
    CBlockIndex* pindex,
    const CAmount nFeesRes,
    const CAmount res_coinbase_value,
    const uint256& expected_draw_candidate_root)
{
    const CTransaction& cb = *block.vtx[0];

    if (!rigor::CheckRigorDrawTargetCommitment(state, block, pindex, params.GetConsensus())) {
        return false;
    }

    uint64_t block_new_convergence{0};
    for (size_t i = 1; i < block.vtx.size(); ++i) {
        block_new_convergence += GetTransactionConvergenceAmount(*block.vtx[i]);
    }
    const uint64_t expected_cumulative_convergence =
        pindex->pprev ? (pindex->pprev->m_cumulative_convergence + block_new_convergence) : 0;
    const std::optional<ConvergenceCommitment> fc_opt = FindLastConvergenceCommitmentInCoinbase(cb);
    if (pindex->pprev != nullptr) {
        Assert(fc_opt.has_value());
        if (fc_opt->m_cumulative_convergence != expected_cumulative_convergence) {
            return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "bad-convergence-cumulative");
        }
        if (fc_opt->m_draw_candidates.root != expected_draw_candidate_root) {
            return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "bad-convergence-draw-candidate-root");
        }
    } else if (fc_opt && fc_opt->m_cumulative_convergence != 0) {
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "bad-genesis-convergence-commitment");
    }

    // SECURITY-001-A9-C removed the `bad-convergence-window-reset` rule that stood here, which
    // required some convergence to have accumulated since the last RES mint whenever a reward was
    // expected.
    //
    // It was always redundant: `res_reward_expected > 0` is derived from the parent's resolved
    // draw having produced a winner, and a winner exists only if F was non-empty, which already
    // means convergence occurred. It restated a consequence as a premise.
    //
    // Under A-9-C it is worse than redundant, it is wrong. The draw at h enumerates F over
    // [h-W, h-1], so consecutive draws share candidates and back-to-back mints paying buried
    // candidates are ordinary. The removed rule measured a different window — convergence since
    // the last MINT — and would have rejected such a block: with a mint at h and another at h+1
    // where block h carries no convergence of its own, the sum is zero while the draw legitimately
    // has a winner. See docs/security/TASK-SECURITY-001-A8-DESIGN-REVIEW.md finding F-5.
    //
    // `bad-last-res-hash` and `bad-convergence-window` went with it: both existed only to compute
    // that sum, and both asserted things about locally-derived index state rather than about the
    // block being validated.
    const CAmount res_bootstrap_coinbase = DevTestResWalletBootstrap(params);
    const CAmount expected_res_coinbase = nFeesRes + res_bootstrap_coinbase;
    if (res_coinbase_value != expected_res_coinbase) {
        LogPrintf("ERROR: ConnectBlock(): RES coinbase mismatch (actual=%d expected=%d nFeesRes=%d bootstrap=%d)\n",
            res_coinbase_value, expected_res_coinbase, nFeesRes, res_bootstrap_coinbase);
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "bad-cb-res-amount");
    }

    return true;
}

} // namespace ConnectBlockRigor
