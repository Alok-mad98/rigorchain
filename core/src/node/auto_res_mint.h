// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_NODE_AUTO_RES_MINT_H
#define BITCOIN_NODE_AUTO_RES_MINT_H

#include <consensus/convergence_logic.h>
#include <primitives/transaction.h>
#include <uint256.h>

#include <optional>
#include <vector>

class CCoinsViewCache;
class CChainParams;
class CScript;

/** P2WSH(OP_TRUE) scriptPubKey funding deterministic auto-mint spends (mockable chains; height-1 coinbase vout). */
const CScript& AutoResMintBootstrapScriptPubKey();

/** One-shot ORU bootstrap for auto-mint funding at height 1 on mockable chains. */
void AppendAutoResMintBootstrapCoinbaseVout(const CChainParams& params, int n_height, CMutableTransaction& coinbase);

/**
 * SECURITY-001-A8: the mint a template must carry pays the draw committed by the parent block,
 * so the template builder consumes the parent's set rather than previewing a draw over the
 * transactions it is still choosing. Nothing here can influence the outcome; the only decision
 * left is whether to assemble the mint at all.
 *
 * Returns nullopt when the parent awarded nobody, when the winner cannot be funded, or when
 * this node is not configured to assemble mints.
 */
std::optional<CMutableTransaction> TryCreateAutomaticResMintTx(const ConvergenceDrawSet& parent_draw_set,
    const ConvergenceDrawOutcome& parent_outcome,
    int block_height,
    const CCoinsViewCache& assemble_view,
    const CScript& mining_res_receive_script);

#endif // BITCOIN_NODE_AUTO_RES_MINT_H
