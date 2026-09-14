// Copyright (c) The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_POLICY_CONVERGENCE_POLICY_H
#define BITCOIN_POLICY_CONVERGENCE_POLICY_H

/**
 * CONVERGENCE_POLICY_V1_LOCK
 *
 * Mempool policy (this header) handles economic ordering only: which transactions are evicted
 * first under memory pressure, and how RPC surfaces fee-related metadata.
 *
 * Consensus alone defines mint validity (CheckResMintArithmetic) and the
 * positive OP_CONVERGE burn (nValue > 0). Policy MUST NOT:
 *   - use draw hash H for ordering,
 *   - use ratios like h/convergence_amount or nonlinear “probability” heuristics,
 *   - predict winners or simulate draw outcomes.
 *
 * Allowed eviction strategies (exactly one active):
 *   (A) Default: sort/evict by descendant feerate only — aligned with Bitcoin Core.
 *   (B) Optional (-convmempoolboost): eviction_score = feerate + k * convergence_amount (ORU at COIN scale),
 *       with k small so fees remain dominant; linear in convergence_amount only.
 */

#define CONVERGENCE_POLICY_V1_LOCK 1

#include <string>

class CTxMemPoolEntry;

/** True if -convmempoolboost is set (scheme B). */
bool ConvergenceMempoolBoostEnabled();

/** k in eviction_score = feerate + k * (convergence_amount / COIN) when boost is enabled; default 0.01 (from -convpolicyk). */
double ConvergePolicyK(const std::string& convpolicyk_override = {});

/**
 * Scalar used only for mempool eviction when trimming (lower evicts first).
 * Scheme A: descendant package feerate (sat/kvB as double via GetFeePerK()).
 * Scheme B: feerate + ConvergePolicyK() * (convergence_amount / COIN) for convergence txs; otherwise feerate only.
 */
double MempoolConvergeEvictionScore(const CTxMemPoolEntry& entry);

#endif // BITCOIN_POLICY_CONVERGENCE_POLICY_H
