#ifndef BITCOIN_CONSENSUS_RIGOR_RES_H
#define BITCOIN_CONSENSUS_RIGOR_RES_H

#include <consensus/amount.h>
#include <consensus/convergence_logic.h>
#include <consensus/validation.h>

class CTransaction;

namespace Consensus {
struct Params;
}

namespace rigor {

/**
 * RES mint evaluation for ConnectBlock, over the block's **coinbase**.
 *
 * **Locus (normative):** new RES exists only here. `CheckTxInputs` rejects every body transaction
 * that creates RES, so this function is the single issuance gate and a block has at most one
 * mint by construction.
 *
 * A mint is present iff the coinbase carries a single `RIGOR_MARKER_RES_MINT` output. `mint_amount_out`
 * receives the RES paid by that output (0 when there is no mint) so ConnectBlock can add it to
 * the coinbase RES budget alongside `nFeesRes` and the mockable-chain bootstrap.
 *
 * SECURITY-001-A8: `parent_draw_set` is the set committed by block h-1 and `parent_outcome` is
 * `ResolveConvergenceDraw` over it. The mint must pay that winner's declared owner the scheduled
 * W(h). There is no second on-chain restatement of the winner.
 */
bool ConnectEvaluateCoinbaseResMint(const CTransaction& coinbase,
    int nBlockHeight,
    const Consensus::Params& consensus_params,
    const ConvergenceDrawSet& parent_draw_set,
    const ConvergenceDrawOutcome& parent_outcome,
    BlockValidationState& state,
    CAmount& mint_amount_out);

} // namespace rigor

#endif
