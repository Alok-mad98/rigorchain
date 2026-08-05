// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// =============================================================================
// RIGOR CONSENSUS WARNING — CONVERGENCE SET F / CONVERGENCE ROOT
//
// ANY modification to Convergence set construction, ConvergenceRoot hashing, or coinbase binding
// WILL cause a consensus fork. Do not change without a full protocol upgrade.
// =============================================================================
//
// ConvergenceRoot — PoW commitment to convergence subset **F** (non-coinbase txs in the block).
//
// CONSENSUS-FROZEN (hard fork to change):
//   - Flat Hash256 over sorted **raw** txids only (`ComputeConvergenceRootFromEligibilityTxids`).
//   - No Merkle tree, no per-leaf Hash256(txid), no block order / fee / amount sort keys.
//
// **F** (block-entry UTXO, never mempool / never post-block view):
//   IsConvergenceTx, convergence_amount >= MIN_CONVERGENCE_AMOUNT, CheckTxInputs, fee(tx) > 0.
//
// **N = |F|:** N < 2 → ConvergenceRoot = 0 (before hashing); N ≥ 2 → sort txids (`uint256` / memcmp),
// then Hash256(concat raw 32-byte txids). Coinbase: BIP34 height + OP_PUSHBYTES_32 root.
// =============================================================================

#include <consensus/convergence_root.h>

#include <coins.h>
#include <consensus/amount.h>
#include <consensus/convergenceres.h>
#include <consensus/params.h>
#include <consensus/res_owner.h>
#include <consensus/tx_verify.h>
#include <consensus/validation.h>
#include <hash.h>
#include <span.h>

#include <algorithm>
#include <string>
#include <vector>

namespace Consensus {

namespace {

/** Direct push of exactly 32 bytes (0x20 prefix in raw script). Not OP_PUSHDATA1/2/4. */
static constexpr opcodetype CONVERGENCE_ROOT_PUSH_OPCODE = static_cast<opcodetype>(32);

static bool ParseBIP34HeightFirstOp(const opcodetype opcode,
    const std::vector<unsigned char>& vch,
    int& height_out,
    std::string& err_reason) noexcept
{
    try {
        if (opcode == OP_0) {
            height_out = 0;
            return true;
        }
        if (opcode >= OP_1 && opcode <= OP_16) {
            height_out = CScript::DecodeOP_N(opcode);
            return true;
        }
        if (opcode == OP_1NEGATE) {
            height_out = -1;
            return true;
        }
        if (vch.empty()) {
            err_reason = "convergence-root-cb-height-not-a-push";
            return false;
        }
        height_out = CScriptNum(vch, /*fRequireMinimal=*/true).getint();
        return true;
    } catch (const scriptnum_error&) {
        err_reason = "convergence-root-cb-height-nonminimal-or-overflow";
        return false;
    } catch (...) {
        err_reason = "convergence-root-cb-height-parse-exception";
        return false;
    }
}

} // namespace

bool ParseCoinbaseConvergenceRootScriptSig(const CScript& scriptSig,
    CoinbaseConvergenceRootScriptSig& out,
    std::string& err_reason) noexcept
{
    try {
        CScript::const_iterator pc = scriptSig.begin();
        opcodetype opcode{OP_INVALIDOPCODE};
        std::vector<unsigned char> vch;

        if (!scriptSig.GetOp(pc, opcode, vch)) {
            err_reason = "convergence-root-cb-empty-scriptsig";
            return false;
        }

        int parsed_height = 0;
        if (!ParseBIP34HeightFirstOp(opcode, vch, parsed_height, err_reason)) {
            return false;
        }
        out.height = parsed_height;

        vch.clear();
        if (!scriptSig.GetOp(pc, opcode, vch)) {
            err_reason = "convergence-root-cb-missing-convergence-root-push";
            return false;
        }
        if (opcode != CONVERGENCE_ROOT_PUSH_OPCODE || vch.size() != 32) {
            err_reason = "convergence-root-cb-convergence-root-not-op-pushbytes-32";
            return false;
        }
        out.convergence_root = uint256{Span{vch}};

        if (scriptSig.GetOp(pc, opcode, vch)) {
            err_reason = "convergence-root-cb-trailing-script-data";
            return false;
        }

        return true;
    } catch (const std::exception& e) {
        err_reason = std::string("convergence-root-parse-exception: ") + e.what();
        return false;
    } catch (...) {
        err_reason = "convergence-root-parse-unknown-exception";
        return false;
    }
}

std::vector<uint256> ListConvergenceRootCommitmentTxids(const std::vector<CTransactionRef>& vtx,
    const CCoinsViewCache& view_at_block_entry,
    int block_height,
    const Consensus::Params& consensus_params)
{
    std::vector<uint256> convergence_txids;
    convergence_txids.reserve(vtx.size());
    for (size_t i = 1; i < vtx.size(); ++i) {
        const CTransaction& tx = *vtx[i];
        if (!IsConvergenceTx(tx)) continue;
        if (GetConvergenceAmount(tx) < MIN_CONVERGENCE_AMOUNT) continue;
        // SECURITY-001-A / A-5 R1: only a convergence that names its RES payout owner may
        // enter F. Without it the reward has no consensus-bound recipient and the block
        // producer picks one. Undeclared burns stay valid; they just do not play.
        // Must stay bit-identical to the same test in ComputeConvergenceFee (undo path).
        if (!HasResOwnerDeclaration(tx)) continue;
        TxValidationState tx_state;
        CAmount txfee = 0;
        if (!CheckTxInputs(tx, tx_state, view_at_block_entry, block_height, consensus_params, txfee)) {
            continue;
        }
        if (txfee <= 0) continue;
        convergence_txids.push_back(tx.GetHash());
    }
    return convergence_txids;
}

uint256 ComputeConvergenceRootFromEligibilityTxids(std::vector<uint256> convergence_txids) noexcept
{
    /*
     * RFC-008 §1: Mathematical domain definition (Domain Separation)
     * 1.1 The three domains (MUST stay isolated): Asset Domain (ORU, RES, Fees / Mint Cost (f_i))
     * Mandatory rule: Asset values MUST NEVER be used as hash input except explicitly defined
     *
     * RFC-008 §2: Basic data types (Canonical Types)
     * 2.1 Definition: bytes32 := exactly 32 bytes (no interpretation)
     *
     * RFC-009 §3: Convergence Root（C）
     * 3.1 Input: txids: array of bytes32
     * 3.2 Sort rule (the only one): Sort lexicographically by raw byte comparison
     * 3.3 Construction: buf = txid_1 || txid_2 || ... || txid_n, C = Hash256(buf)
     * 3.4 Empty set: if n == 0: C = 32 zero bytes
     * ❌ FORBIDDEN: Merkle tree, pair hashing, length prefix
     *
     * RFC-008 §10: Edge Cases
     * 10.2 Empty set: If F is empty: C = 32 zero bytes, No winner
     *
     * RFC-008 §3: Concatenation Rule
     * 3.1 The only legal concatenation: A || B := byte-wise concatenation
     * 3.2 SIGMA input (MUST match exactly): Input Buffer (100 bytes EXACTLY)
     *
     * RIGOR IMPLEMENTATION BIBLE §6: ROOT (Convergence Root)
     * C = Hash256(concat(sorted(txids)))
     *
     * RFC-000 §6: CONVERGENCE ROOT (convergence root lock)
     * C = Hash256(concat(sort(txid_i)))
     *
     * Normative ConvergenceRoot (flat commitment; **not** a Merkle tree):
     *
     *   ConvergenceRoot = Hash256( concat( sorted_txid_bytes ) )
     *
     *   - Each element is a **txid** as `uint256`: **32 bytes**, fixed width, in **native memory
     *     order** as stored in `uint256` (little-endian on all supported platforms — same bytes
     *     as `tx.GetHash()` / `uint256` object representation). **MUST NOT** apply endian swaps
     *     or `SerializeHash(tx)` / any serialization of the whole transaction.
     *   - **Sort key:** `std::sort` on `std::vector<uint256>` (unsigned byte-wise lexicographical order
     *     per RFC-000 §6 - memcmp byte order of those 32-byte representations). **MUST NOT** sort by
     *     block index, feerate, or convergence amount.
     *   - **Concat order:** ascending sorted order only (after sort).
     *   - If fewer than two txids are present: return **all-zero** `uint256()` **before** any hashing.
     *     RFC-015 changes RES eligibility only; it does not alter this root boundary.
     *
     * **Explicitly forbidden** (consensus fork if introduced):
     *   - Merkle tree over **F**
     *   - Per-leaf `Hash256(txid)` before concatenation
     *   - Hashing anything other than raw 32-byte txid bytes in sorted order
     *   - Block-internal vtx order as input to the commitment
     *
     * RIGOR IMPLEMENTATION BIBLE §6: SORT RULE (critical)
     * Sort using unsigned byte comparison
     * ❌ FORBIDDEN: string compare, locale compare, signed char compare
     *
     * RFC-009 §9: Prohibited practices
     * Using std::sort default (platform dependent) ❌
     * CRITICAL: std::sort on uint256 uses memcmp byte order - this is REQUIRED per RFC-000 §6.
     * Historical Failure Example: Ethereum's Geth vs Parity used different sorting
     * algorithms for transaction ordering, leading to different block templates.
     */
    // RFC-008 §10: Edge Cases
    // 10.2 Empty set: If F is empty: C = 32 zero bytes, No winner
    // RFC-009 §3.4: Empty set - if n == 0: C = 32 zero bytes
    // RIGOR IMPLEMENTATION BIBLE §1️2: BOUNDARY
    // F with fewer than two txids keeps the preserved all-zero root boundary.
    if (convergence_txids.size() < 2) {
        return uint256();
    }
    // RFC-009 §3.2: Sort rule (the only one)- Sort lexicographically by raw byte comparison
    // RIGOR IMPLEMENTATION BIBLE §6: unsigned byte comparison sort
    std::sort(convergence_txids.begin(), convergence_txids.end());
    // RFC-008 §3: Concatenation Rule- A || B := byte-wise concatenation
    // RFC-009 §3.3: Construction - buf = txid_1 || txid_2 || ... || txid_n, C = Hash256(buf)
    // RIGOR IMPLEMENTATION BIBLE §2: BYTE CONCAT (no delimiters, length prefixes, padding)
    CHash256 hasher;
    for (const uint256& id : convergence_txids) {
        hasher.Write(MakeUCharSpan(id));
    }
    // RFC-008 §4: Hash function definition (Hash256)- Hash256(x) = SHA256(SHA256(x))
    // RIGOR IMPLEMENTATION BIBLE §3: Hash256 only
    uint256 root;
    hasher.Finalize(root);
    return root;
}

uint256 ComputeConvergenceRootFromConvergenceSet(const std::vector<CTransactionRef>& vtx,
    const CCoinsViewCache& view_at_block_entry,
    int block_height,
    const Consensus::Params& consensus_params)
{
    return ComputeConvergenceRootFromEligibilityTxids(
        ListConvergenceRootCommitmentTxids(vtx, view_at_block_entry, block_height, consensus_params));
}

bool ValidateCoinbaseConvergenceRootScriptSig(const CScript& scriptSig,
    int nHeight,
    const uint256& expected_convergence_root,
    std::string& err_reason) noexcept
{
    try {
        CoinbaseConvergenceRootScriptSig parsed;
        if (!ParseCoinbaseConvergenceRootScriptSig(scriptSig, parsed, err_reason)) {
            return false;
        }
        if (parsed.height != nHeight) {
            err_reason = "convergence-root-cb-height-mismatch";
            return false;
        }
        if (parsed.convergence_root != expected_convergence_root) {
            err_reason = "convergence-root-mismatch; convergence_set_root=";
            err_reason += expected_convergence_root.ToString();
            return false;
        }

        return true;
    } catch (const std::exception& e) {
        err_reason = std::string("convergence-root-validation-exception: ") + e.what();
        return false;
    } catch (...) {
        err_reason = "convergence-root-validation-unknown-exception";
        return false;
    }
}

bool ValidateCoinbaseConvergenceRootScriptSig(const CScript& scriptSig,
    int nHeight,
    const std::vector<CTransactionRef>& vtx,
    const CCoinsViewCache& view_at_block_entry,
    const Consensus::Params& consensus_params,
    std::string& err_reason) noexcept
{
    const uint256 expected = ComputeConvergenceRootFromConvergenceSet(vtx, view_at_block_entry, nHeight, consensus_params);
    return ValidateCoinbaseConvergenceRootScriptSig(scriptSig, nHeight, expected, err_reason);
}

} // namespace Consensus
