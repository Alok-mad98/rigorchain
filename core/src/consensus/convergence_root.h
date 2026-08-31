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
 *   3. `CheckTxInputs` succeeds against the **in-block overlay view**: each prior
 *      non-coinbase tx in the block is applied (SpendCoin + AddCoins) before the
 *      candidate is tested, so same-block chains are in **F** (RIG-02). This is
 *      the same set `BuildBlockConvergenceCandidatesFromUndo` reconstructs.
 *   4. Inputs verify (`CheckTxInputs`). Fee amount is not a membership condition
 *      (zero-fee is a relay/DoS policy, not a draw-correctness rule).
 *
 * **F MUST** be enumerated by `IsConvergenceDrawMember` on the in-block overlay only
 * (`ListConvergenceRootCommitmentTxids` live; `BuildBlockConvergenceCandidatesFromUndo` seeds
 * the same overlay from undo). A second membership predicate is a fork.
 *
 * **F MUST** be constructed **exactly once** per `ConnectBlock` for a given `(vtx, view_at_entry)`
 * pair: `Chainstate::ConnectBlock` lists **F** at block-entry with the overlay, then passes the
 * same vector through coinbase validation and RES checks.
 */

/** Parsed coinbase scriptSig: BIP34 height + 32-byte C + 32-byte paid-winner txid. */
struct CoinbaseConvergenceRootScriptSig {
    int height{};
    uint256 convergence_root{};
    uint256 paid_winner_txid{};
};

/** Canonical coinbase scriptSig: height || OP_PUSHBYTES_32(C) || OP_PUSHBYTES_32(paid_winner). */
CScript BuildCoinbaseCommitmentScriptSig(int height, const uint256& convergence_root,
                                         const uint256& paid_winner_txid);

/**
 * Parse coinbase scriptSig as exactly three pushes (height, C, paid-winner txid).
 * paid_winner_txid is the all-zero uint256 when the coinbase does not mint RES (RIG-05).
 */
bool ParseCoinbaseConvergenceRootScriptSig(const CScript& scriptSig,
    CoinbaseConvergenceRootScriptSig& out,
    std::string& err_reason) noexcept;

/**
 * Single F-membership predicate (TASK-010 C-2). True iff `tx` is a non-coinbase
 * convergence ticket with a unique owner declaration, amount ≥ MIN, and
 * `CheckTxInputs` succeeds against `view` (fee is not a membership condition).
 */
bool IsConvergenceDrawMember(const CTransaction& tx,
    const CCoinsViewCache& view,
    int block_height,
    const Params& consensus_params);

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
 * MUST be parsed as exactly three pushes via CScript::GetOp (no trailing ops):
 *   (1) minimal BIP34 height (same encoding family as CScript() << nHeight),
 *   (2) convergence_root as OP_PUSHBYTES_32,
 *   (3) paid_winner_txid as OP_PUSHBYTES_32 (all-zero iff the coinbase does not mint).
 */
bool ValidateCoinbaseConvergenceRootScriptSig(const CScript& scriptSig,
    int nHeight,
    const std::vector<CTransactionRef>& vtx,
    const CCoinsViewCache& view_at_block_entry,
    const Params& consensus_params,
    std::string& err_reason) noexcept;

/**
 * Same checks as the full-block overload, but `expected_convergence_root` is supplied by the caller
 * so consensus does not enumerate **F** twice. Does not check the paid-winner push against a value.
 */
bool ValidateCoinbaseConvergenceRootScriptSig(const CScript& scriptSig,
    int nHeight,
    const uint256& expected_convergence_root,
    std::string& err_reason) noexcept;

/**
 * Like the three-argument form, and also requires the paid-winner push to equal
 * `expected_paid_winner` (all-zero when this block does not mint).
 */
bool ValidateCoinbaseConvergenceRootScriptSig(const CScript& scriptSig,
    int nHeight,
    const uint256& expected_convergence_root,
    const uint256& expected_paid_winner,
    std::string& err_reason) noexcept;

} // namespace Consensus

#endif // BITCOIN_CONSENSUS_CONVERGENCE_ROOT_H
