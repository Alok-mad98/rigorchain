// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// Calls ResolveConvergenceDraw only. Does not assemble F from chain/UTXO.

#ifndef RIGOR_KERNEL_ABI_VERIFY_ONLY
#error "consensus/bridge is verify-only; compile with -DRIGOR_KERNEL_ABI_VERIFY_ONLY"
#endif

#include <consensus/bridge/consensus_kernel_c_api.h>
#include <consensus/bridge/kernel_marshalling.h>
#include <consensus/convergence_logic.h>

#include <cstring>

bool rigor_resolve_draw(
    const rigor_candidate* candidates,
    size_t count,
    const rigor_draw_context* ctx,
    rigor_draw_result* result)
{
    if (!ctx || !result) return false;
    if (count > 0 && !candidates) return false;

    ConvergenceDrawSet set;
    set.source_height = static_cast<int>(ctx->payout_height) - 1;
    set.convergence_root = RigorU256ToUint256(ctx->convergence_root);
    set.source_block_hash = RigorU256ToUint256(ctx->source_block_hash);
    set.candidates.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        ConvergenceDrawCandidate c;
        c.txid = RigorU256ToUint256(candidates[i].txid);
        c.weight = candidates[i].value;
        set.candidates.push_back(c);
    }

    const uint256 target = RigorU256ToUint256(ctx->target);
    const ConvergenceDrawOutcome o = ResolveConvergenceDraw(set, target);
    std::memset(result, 0, sizeof(*result));
    if (!o.HasWinner()) {
        result->has_winner = 0;
        return true;
    }
    result->has_winner = 1;
    Uint256ToRigorU256(set.candidates[static_cast<size_t>(o.winner_index)].txid, &result->winner);
    return true;
}
