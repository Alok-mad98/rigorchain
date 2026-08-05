#ifndef BITCOIN_CONSENSUS_RIGOR_RES_H
#define BITCOIN_CONSENSUS_RIGOR_RES_H

#include <arith_uint256.h>
#include <consensus/convergence_logic.h>
#include <consensus/res_proof.h>
#include <uint256.h>
#include <consensus/validation.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

class CCoinsViewCache;
class CBlock;
class CBlockIndex;
class CTransaction;

namespace Consensus {
struct Params;
}

namespace rigor {

using ConvergeProof = Consensus::ConvergeProof;

bool ParseConvergeProofWitness(const CTransaction& tx, ConvergeProof& proof_out);

/**
 * RES mint evaluation for ConnectBlock.
 *
 * **Semantics (normative):** `semantic = f(GetTransactionInputAsset)` only — see block comment in
 * `rigor_res.cpp`. ORU inputs → Mint mode (may score); RES inputs → plain transfer (never scores).
 * `ParseConvergeProof` does not select the mode.
 *
 * SECURITY-001-A8 draw binding: `parent_draw_set` is the convergence set committed by block h-1
 * and `parent_outcome` is `ResolveConvergenceDraw` over it. Every expected value the proof is
 * checked against — the σ seed, the payout height, `target_prev`, the convergence root, set
 * membership, the committed weight, the winner, and the declared owner — is read from those two
 * objects rather than recomputed here. A second derivation path could drift and split consensus.
 *
 * The set MUST come from the parent of the block being connected: `parent_draw_set.source_block_hash`
 * is the σ seed and MUST equal that block's `hashPrevBlock`.
 */
bool ConnectEvaluateResMintTx(const CTransaction& tx,
    const CCoinsViewCache& inputs,
    int nBlockHeight,
    const Consensus::Params& consensus_params,
    const ConvergenceDrawSet& parent_draw_set,
    const ConvergenceDrawOutcome& parent_outcome,
    BlockValidationState& state,
    std::optional<std::pair<arith_uint256, uint256>>* score_out);

bool EnforceResMintUnique(BlockValidationState& state,
    const std::vector<std::optional<std::pair<arith_uint256, uint256>>>& per_tx);

} // namespace rigor

#endif
