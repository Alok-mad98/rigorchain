// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_NODE_AUTO_RES_MINT_H
#define BITCOIN_NODE_AUTO_RES_MINT_H

#include <consensus/convergence_logic.h>
#include <primitives/transaction.h>

/**
 * SECURITY-001-A8: the mint a template must carry pays the draw committed by the parent block,
 * so the template builder consumes the parent's set rather than previewing a draw over the
 * transactions it is still choosing. Nothing here can influence the outcome.
 *
 * Appends the RIGOR_MARKER_RES_MINT payout to `coinbase`. There is no funding input: the coinbase is
 * authenticated by proof of work, and the prize, amount and payout program are all fixed by
 * the parent. Returns false when there is no winner, when W(h) = 0, or when the winner's
 * declaration is malformed.
 */
bool TryAppendCoinbaseResMint(CMutableTransaction& coinbase,
    const ConvergenceDrawSet& parent_draw_set,
    const ConvergenceDrawOutcome& parent_outcome,
    int block_height);

#endif // BITCOIN_NODE_AUTO_RES_MINT_H
