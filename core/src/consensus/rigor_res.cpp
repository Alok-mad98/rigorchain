#include <consensus/rigor_res.h>

#include <chain.h>
#include <coins.h>
#include <consensus/convergence_root.h>

// =============================================================================
// RIGOR CONSENSUS WARNING — RES MINT × CONVERGENCE ROOT
//
// ANY modification to Convergence set construction, ConvergenceRoot hashing, or RES membership
// rules WILL cause a consensus fork. Do not change without a full protocol upgrade.
// =============================================================================
//
// The coinbase RIGOR_MARKER_RES_MINT is issuance. F is the window C committed in the parent
// coinbase scriptSig. There is no second on-chain restatement of the winner (no PCORB).
//
// **CONVERGENCEROOT DEFINES F. POW COMMITS CONVERGENCEROOT. MINT PAYS THE RESOLVED WINNER.**
// -----------------------------------------------------------------------------
#include <consensus/convergenceres.h>
#include <consensus/params.h>
#include <consensus/res_owner.h>
#include <consensus/rigor_consensus_lock.h>
#include <consensus/tx_verify.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <script/predicate_verify.h>
#include <script/standard.h>

#include <algorithm>
#include <cassert>

namespace rigor {

bool ConnectEvaluateCoinbaseResMint(const CTransaction& coinbase,
    int nBlockHeight,
    const Consensus::Params& consensus_params,
    const ConvergenceDrawSet& parent_draw_set,
    const ConvergenceDrawOutcome& parent_outcome,
    BlockValidationState& state,
    CAmount& mint_amount_out)
{
    mint_amount_out = 0;
    if (!coinbase.IsCoinBase()) {
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "bad-cb-missing");
    }

    // The coinbase legitimately carries other RES: the RES fee output and, on mockable chains, the
    // bootstrap. Those are witness v1 RES, never RIGOR_MARKER_RES_MINT, so the mint payout is identified by
    // its lock and not by "some RES output".
    const CTxOut* payout = nullptr;
    for (const CTxOut& o : coinbase.vout) {
        if (!IsResMintScript(o.scriptPubKey)) continue;
        if (payout != nullptr) {
            return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "bad-rigor-res-amount");
        }
        payout = &o;
    }

    if (payout == nullptr) {
        return true; // no mint claimed; the winner-must-mint rule is ConnectBlock's to apply
    }

    int64_t maturity_height = 0;
    std::vector<unsigned char> pubkey_hash;
    if (!ParseResMintScript(payout->scriptPubKey, maturity_height, pubkey_hash) ||
        maturity_height != static_cast<int64_t>(nBlockHeight) + consensus_params.nCoinbaseMaturity) {
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "bad-rigor-res-mint-script");
    }

    if (nBlockHeight < 0) {
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "bad-rigor-res-proof-height");
    }
    if (!parent_draw_set.IsSet() || parent_draw_set.source_height + 1 != nBlockHeight) {
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "bad-rigor-res-draw-set-height");
    }

    if (!parent_outcome.HasWinner()) {
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "bad-rigor-res-without-winner");
    }
    const ConvergenceDrawCandidate& winner = parent_draw_set.candidates[static_cast<size_t>(parent_outcome.winner_index)];

    // SECURITY-001-A / A-5 R2: the RES goes to the owner who burned the ORU, not to whoever
    // assembled the block.
    if (!Consensus::CheckResRewardOwner(winner.res_payout_hash160, payout->scriptPubKey)) {
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "bad-rigor-res-owner-mismatch");
    }

    const CAmount res_reward = Consensus::GetResMintReward(nBlockHeight);
    if (res_reward <= 0 || payout->nValue != res_reward) {
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "bad-rigor-res-amount");
    }

    mint_amount_out = res_reward;
    return true;
}

} // namespace rigor
