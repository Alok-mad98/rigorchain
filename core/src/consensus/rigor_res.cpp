#include <consensus/rigor_res.h>

#include <chain.h>
#include <coins.h>
#include <consensus/convergence_root.h>

// =============================================================================
// RIGOR CONSENSUS WARNING — PCORB × CONVERGENCE ROOT
//
// ANY modification to Convergence set construction, ConvergenceRoot hashing, or RES membership
// rules WILL cause a consensus fork. Do not change without a full protocol upgrade.
// =============================================================================
//
// PCORB × ConvergenceRoot — single commitment source (coinbase.scriptSig), no second F.
//
// **CONVERGENCEROOT DEFINES F. PCORB PROVES MEMBERSHIP IN F. POW COMMITS CONVERGENCEROOT.**
// **NO SECOND SOURCE. NO HIDDEN STATE. NO FUTURE DRIFT.**
//
// PCORB dual-semantics (normative — same witness layout, two meanings)
// `Consensus::ConvergeProof` / `ParseConvergeProof` decode a **unified** witness stack (PCORB-shaped data).
//     `ComputeConvergenceRootFromEligibilityTxids(F)` from the passed-in **F** is allowed **solely** for
//     that equality check (same preimage as the PoW-committed root).
//   - access UTXO / `CheckTxInputs` independently to build or refine **F**
//   - depend on mempool or post-connect coin state
//
// `proof.convergence_txid ∈ F` is a **set membership** check against the preimage of the committed
// ConvergenceRoot (the committed bytes bind `Hash256(concat(sorted F))`; membership in **F** is necessary
// for that binding to be meaningful). It is not a standalone Merkle membership proof; the
// commitment itself is the PoW-anchored proof of the multiset **F**.
//
// -----------------------------------------------------------------------------
// PCORB semantics (normative — mint only)
// -----------------------------------------------------------------------------
// `Consensus::ConvergeProof` / `ParseConvergeProof` decode the PCORB witness stack, which has
// exactly one meaning: **Mint mode**, body RES issuance. The proof participates in `res_mint_scores`
// when all mint / winner / convergenceRoot rules pass; the issuance amount is capped elsewhere
// (`RES_REWARD_AMOUNT`).
//
// SECURITY-001-A / A-7 removed the second meaning. A RES-domain input used to be able to carry the
// same witness shape as a "Claim mode" spend authorization, over a script path whose signature
// covered σ rather than the spending transaction. That path is gone: a witness v1 RES output is now
// spendable only over the 2-item Ed25519 key path, so no RES-domain input can present a PCORB proof.
//
// The domain rule still governs and must not be weakened: the semantic domain is determined **only**
// by `Consensus::GetTransactionInputAsset` (prevout scriptPubKey → `AssetType`), never by witness
// contents, and a mint requires ORU-only prevouts. Dual-pool ORU+RES prevouts ⇒ the domain is RES,
// which cannot mint. `ParseConvergeProof` reads witness bytes only to validate and bind after the
// domain is already known to be ORU; it never selects the mode.
// -----------------------------------------------------------------------------
#include <consensus/convergenceres.h>
#include <consensus/params.h>
#include <consensus/res_owner.h>
#include <consensus/res_proof.h>
#include <consensus/tx_verify.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <script/standard.h>

#include <algorithm>
#include <cassert>

namespace rigor {

bool ParseConvergeProofWitness(const CTransaction& tx, ConvergeProof& proof_out)
{
    return Consensus::ParseConvergeProof(tx, proof_out);
}

bool ConnectEvaluateResMintTx(const CTransaction& tx,
    const CCoinsViewCache& inputs,
    int nBlockHeight,
    const Consensus::Params& consensus_params,
    const ConvergenceDrawSet& parent_draw_set,
    const ConvergenceDrawOutcome& parent_outcome,
    BlockValidationState& state,
    std::optional<std::pair<arith_uint256, uint256>>* score_out)
{
    assert(score_out != nullptr);
    score_out->reset();

    bool any_rng = false;
    for (const auto& o : tx.vout) {
        if (IsResScript(o.scriptPubKey)) {
            any_rng = true;
            break;
        }
    }
    if (!any_rng) return true;

    const std::optional<AssetType> dom = Consensus::GetTransactionInputAsset(tx, inputs);
    if (!dom.has_value()) {
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "bad-txns-spend-non-asset-input");
    }
    if (*dom == ASSET_RES) {
        return true;
    }
    if (*dom != ASSET_ORU) {
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "bad-rigor-res-mint-domain");
    }

    ConvergeProof proof;
    if (!Consensus::ParseConvergeProof(tx, proof)) {
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "bad-rigor-res-witness");
    }

    // -------------------------------------------------------------------------
    // SECURITY-001-A / A8 — σ-preimage chain binding.
    //
    // σ = Hash256(txid || C || B_{h-1} || LE32(h)). Before A-1..A-4 these fields were
    // consumed only as digest input and never compared with the chain, leaving 44
    // attacker-chosen bytes inside σ. A-8 went further and moved the whole draw one
    // block back, so the fields now describe block h-1: the seed is its hash and the
    // set is what it committed. Both are fixed by a header that already cost a
    // proof-of-work solution, which is what makes re-rolling the draw expensive.
    //
    // The expected values are handed down from ConnectBlock rather than recomputed,
    // so they are byte-identical to the preimage that produced the candidate σ set.
    // -------------------------------------------------------------------------
    if (nBlockHeight < 0 || proof.height != static_cast<uint64_t>(nBlockHeight)) {
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "bad-rigor-res-proof-height");
    }
    if (!parent_draw_set.IsSet() || parent_draw_set.source_height + 1 != nBlockHeight) {
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "bad-rigor-res-draw-set-height");
    }
    // Redundant with B_{h-1}, which hashes the parent header including its nBits, but kept
    // as an explicit assertion so a malformed witness is rejected by name rather than by a
    // failed inequality.
    if (proof.target_prev != parent_draw_set.source_nbits) {
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "bad-rigor-res-proof-target-prev");
    }
    if (proof.bind_hash != parent_draw_set.source_block_hash) {
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "bad-rigor-res-proof-bind-hash");
    }

    if (!parent_outcome.HasWinner()) {
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "bad-rigor-res-without-winner");
    }
    const ConvergenceDrawCandidate& winner = parent_draw_set.candidates[static_cast<size_t>(parent_outcome.winner_index)];

    // Membership in F. The proof names a convergence transaction in block h-1; anything not in
    // that block's committed set cannot have been drawn.
    const auto named = std::find_if(parent_draw_set.candidates.begin(), parent_draw_set.candidates.end(),
        [&proof](const ConvergenceDrawCandidate& c) { return c.txid == proof.convergence_txid; });
    if (named == parent_draw_set.candidates.end()) {
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "bad-sigma-convergence-not-in-convergence-root-set");
    }

    if (!Consensus::CheckConvergeProofBinding(proof, static_cast<CAmount>(named->weight))) {
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "bad-rigor-res-binding");
    }

    // RFC-014 Conv-Decouple-01: inequality RHS sources from constitutional constant.
    if (!Consensus::VerifyConvergeProof(proof, consensus_params.RES_PROBABILITY_TARGET, &winner.txid, &parent_draw_set.convergence_root)) {
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "bad-rigor-res-proof");
    }

    // SECURITY-001-A / A-5 R2: the RES goes to the owner who burned the ORU, not to whoever
    // assembled the block. Without this the producer chose vout[0].scriptPubKey freely and
    // could redirect any user's winnings to itself.
    if (!Consensus::CheckResRewardOwner(winner.res_payout_program32, tx)) {
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "bad-rigor-res-owner-mismatch");
    }

    const uint256 h256 = Consensus::ConvergeProofDigest(proof.convergence_txid, proof.bind_hash, proof.height, proof.convergence_root);
    *score_out = std::make_pair(UintToArith256(h256), tx.GetHash());
    return true;
}

bool EnforceResMintUnique(BlockValidationState& state,
    const std::vector<std::optional<std::pair<arith_uint256, uint256>>>& per_tx)
{
    std::vector<size_t> idx;
    for (size_t i = 0; i < per_tx.size(); ++i) {
        if (per_tx[i].has_value()) idx.push_back(i);
    }
    if (idx.size() <= 1) return true;

    size_t best = idx[0];
    for (size_t k = 1; k < idx.size(); ++k) {
        const auto& a = *per_tx[idx[k]];
        const auto& b = *per_tx[best];
        if (a.first < b.first || (a.first == b.first && a.second < b.second)) {
            best = idx[k];
        }
    }
    for (size_t i : idx) {
        if (i != best) {
            return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "bad-rigor-multiple-res");
        }
    }
    return true;
}

} // namespace rigor
