// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_CONVERGENCE_ROOT_H
#define BITCOIN_CONSENSUS_CONVERGENCE_ROOT_H

// =============================================================================
// RIGOR CONSENSUS WARNING — CONVERGENCE SET F / CONVERGENCE ROOT
//
// ANY modification to: Convergence set construction, ConvergenceRoot hashing, or rules that
// bind coinbase to F WILL cause a consensus fork. Do not change without a full
// protocol upgrade and coordinated deployment.
// =============================================================================

#include <consensus/convergence_root_eligibility.h>
#include <primitives/transaction.h>
#include <script/script.h>
#include <uint256.h>

#include <string>
#include <vector>

class CCoinsViewCache;

namespace Consensus {
struct Params;
}

namespace Consensus {

/*
 * Protocol (normative) — Convergence set **F** for a candidate block `B` (non-coinbase txs only):
 *
 * **F** is the set of txids `txid = tx.GetHash()` such that **ALL** of the following hold:
 *
 *   1. `IsConvergenceTx(tx) == true`
 *   2. `GetConvergenceAmount(tx) >= MIN_CONVERGENCE_AMOUNT`
 *   3. `CheckTxInputs(tx, tx_state, view_at_block_entry, block_height, consensus_params, txfee)`
 *      succeeds (same inputs / fee path as consensus; **block-entry** UTXO view only).
 *   4. Inputs verify (`CheckTxInputs`). Fee amount is not a membership condition
 *      (zero-fee is a relay/DoS policy, not a draw-correctness rule).
 *
 * **F MUST** be enumerated by `ListConvergenceRootCommitmentTxids` semantics only (no alternate
 * predicate in consensus code paths).
 *
 * **F MUST** be constructed **exactly once** per `ConnectBlock` for a given `(vtx, view_at_entry)`
 * pair: `Chainstate::ConnectBlock` lists **F** at block-entry, then passes the same vector through
 * coinbase validation and RES checks. **MUST NOT** be recomputed from mempool, post-`UpdateCoins`
 * state, or any ordering side effect inside the block body.
 *
 * **F MUST NOT** depend on: mempool, post-UpdateCoins UTXO view, or intra-block transaction order
 * (only membership / sorted root uses **F**; enumeration order from `ListConvergenceRootCommitmentTxids`
 * is arbitrary and non-normative).
 */

/** Parsed coinbase scriptSig: BIP34 height + 32-byte convergence root (OP_PUSHBYTES_32 only). */
struct CoinbaseConvergenceRootScriptSig {
    int height{};
    uint256 convergence_root{};
};

/**
 * Parse coinbase scriptSig as exactly two pushes (height, then OP_PUSHBYTES_32 + 32 bytes).
 * Does not compare against a block body; use with ValidateCoinbaseConvergenceRootScriptSig for that.
 */
bool ParseCoinbaseConvergenceRootScriptSig(const CScript& scriptSig,
    CoinbaseConvergenceRootScriptSig& out,
    std::string& err_reason) noexcept;

/**
 * Enumerate **F** (see normative definition at top of this header). Non-coinbase txs in `vtx`,
 * `view_at_block_entry` = UTXO at **block entry** (before applying this block).
 *
 * Returned txids are **unsorted**; order MUST NOT be used by consensus (only membership or
 * `ComputeConvergenceRootFromEligibilityTxids` after sort).
 *
 * **Single-source rule:** `ConnectBlock` calls this **once** per block; RES (`rigor_rng`) MUST NOT
 * call it again — the same vector is passed in.
 */
std::vector<uint256> ListConvergenceRootCommitmentTxids(const std::vector<CTransactionRef>& vtx,
    const CCoinsViewCache& view_at_block_entry,
    int block_height,
    const Params& consensus_params);

/**
 * ConvergenceRoot (flat commitment — **not** a Merkle tree; no per-txid leaf hash before concat):
 *
 *   1) Build **F** via `ListConvergenceRootCommitmentTxids` (same predicate as draw fee counting).
 *   2) Empty **F** → **all-zero** `uint256()` (**before** any hashing). A singleton hashes.
 *   3) Otherwise sort **F** by `uint256` ascending (**`std::sort` / `memcmp` byte order** only;
 *      block index order, feerate, or convergence amount MUST NOT influence ordering).
 *   4) `ConvergenceRoot = Hash256( raw_txid_0 || raw_txid_1 || ... )` — each txid is 32 bytes in
 *      `uint256` memory order; **no** `Hash256(txid)` per leaf and **no** Merkle mutation surface.
 */
uint256 ComputeConvergenceRootFromConvergenceSet(const std::vector<CTransactionRef>& vtx,
    const CCoinsViewCache& view_at_block_entry,
    int block_height,
    const Params& consensus_params);

/**
 * Consensus coinbase scriptSig (post-BIP34 / height-in-coinbase deployment):
 * MUST be parsed as exactly two pushes via CScript::GetOp (no trailing ops):
 *   (1) minimal BIP34 height (same encoding family as CScript() << nHeight),
 *   (2) convergence_root as OP_PUSHBYTES_32 (opcode 0x20) followed by 32 raw bytes
 *       — not OP_PUSHDATA1/2/4 — matching ComputeConvergenceRootFromConvergenceSet(...).
 *
 * Requires CCoinsViewCache at block entry (same view ConnectBlock uses before applying txs).
 */
bool ValidateCoinbaseConvergenceRootScriptSig(const CScript& scriptSig,
    int nHeight,
    const std::vector<CTransactionRef>& vtx,
    const CCoinsViewCache& view_at_block_entry,
    const Params& consensus_params,
    std::string& err_reason) noexcept;

/**
 * Same checks as the full-block overload, but `expected_convergence_root` is supplied by the caller
 * (e.g. after a single `ListConvergenceRootCommitmentTxids` + `ComputeConvergenceRootFromEligibilityTxids` in
 * `ConnectBlock`) so consensus does not enumerate **F** twice.
 */
bool ValidateCoinbaseConvergenceRootScriptSig(const CScript& scriptSig,
    int nHeight,
    const uint256& expected_convergence_root,
    std::string& err_reason) noexcept;

} // namespace Consensus

#endif // BITCOIN_CONSENSUS_CONVERGENCE_ROOT_H
