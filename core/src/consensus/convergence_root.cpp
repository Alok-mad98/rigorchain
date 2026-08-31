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
//   - Flat Hash256 over sorted **raw** txids only (`ComputeConvergenceRootFromEligibilityTxids`
//     in `convergence_root_eligibility.cpp`; this file keeps the coin-view wrapper).
//   - No Merkle tree, no per-leaf Hash256(txid), no block order / fee / amount sort keys.
//
// **F** (block-entry UTXO plus in-block overlay, never mempool / never post-block view):
//   IsConvergenceTx, convergence_amount >= MIN_CONVERGENCE_AMOUNT, HasResOwnerDeclaration, CheckTxInputs
//   against an overlay that applies prior txs in the block so same-block chains enter F (RIG-02).
//   Fee is RELAY POLICY, not a membership condition: a zero-fee burn that otherwise verifies is in **F**
//   (see ListConvergenceRootCommitmentTxids below, and ComputeConvergenceFee, which must agree).
//
// **N = |F|:** N == 0 → ConvergenceRoot = 0 (before hashing); N ≥ 1 → sort txids (`uint256` / memcmp),
// then Hash256(concat raw 32-byte txids). Coinbase: BIP34 height + OP_PUSHBYTES_32 window root.
// =============================================================================

#include <consensus/convergence_root.h>

#include <coins.h>
#include <consensus/amount.h>
#include <consensus/convergenceres.h>
#include <consensus/params.h>
#include <consensus/res_owner.h>
#include <consensus/tx_verify.h>
#include <consensus/validation.h>

#include <string>
#include <vector>

namespace Consensus {

namespace {

/** Direct push of exactly 32 bytes (0x20 prefix in raw script). Not RIGOR_PUSHDATA1/2/4. */
static constexpr opcodetype CONVERGENCE_ROOT_PUSH_OPCODE = static_cast<opcodetype>(32);

static bool ParseBIP34HeightFirstOp(const opcodetype opcode,
    const std::vector<unsigned char>& vch,
    int& height_out,
    std::string& err_reason) noexcept
{
    try {
        if (opcode == RIGOR_PUSH_0) {
            height_out = 0;
            return true;
        }
        if (opcode >= RIGOR_PUSH_1 && opcode <= RIGOR_PUSH_16) {
            height_out = CScript::DecodeOP_N(opcode);
            return true;
        }
        if (opcode == RIGOR_RESERVED_LEGACY_1NEGATE) {
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

CScript BuildCoinbaseCommitmentScriptSig(int height, const uint256& convergence_root,
                                         const uint256& paid_winner_txid)
{
    return CScript() << height
                     << std::vector<unsigned char>(convergence_root.begin(), convergence_root.end())
                     << std::vector<unsigned char>(paid_winner_txid.begin(), paid_winner_txid.end());
}

bool ParseCoinbaseConvergenceRootScriptSig(const CScript& scriptSig,
    CoinbaseConvergenceRootScriptSig& out,
    std::string& err_reason) noexcept
{
    try {
        CScript::const_iterator pc = scriptSig.begin();
        opcodetype opcode{RIGOR_BYTE_INVALID};
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

        vch.clear();
        if (!scriptSig.GetOp(pc, opcode, vch)) {
            err_reason = "convergence-root-cb-missing-paid-winner-push";
            return false;
        }
        if (opcode != CONVERGENCE_ROOT_PUSH_OPCODE || vch.size() != 32) {
            err_reason = "convergence-root-cb-paid-winner-not-op-pushbytes-32";
            return false;
        }
        out.paid_winner_txid = uint256{Span{vch}};

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

bool IsConvergenceDrawMember(const CTransaction& tx,
    const CCoinsViewCache& view,
    int block_height,
    const Params& consensus_params)
{
    if (tx.IsCoinBase()) return false;
    if (!IsConvergenceTx(tx)) return false;
    if (GetConvergenceAmount(tx) < MIN_CONVERGENCE_AMOUNT) return false;
    if (!HasResOwnerDeclaration(tx)) return false;
    TxValidationState tx_state;
    CAmount txfee = 0;
    return CheckTxInputs(tx, tx_state, view, block_height, consensus_params, txfee);
}

std::vector<uint256> ListConvergenceRootCommitmentTxids(const std::vector<CTransactionRef>& vtx,
    const CCoinsViewCache& view_at_block_entry,
    int block_height,
    const Consensus::Params& consensus_params)
{
    // Overlay prior txs in this block so same-block chains enter F (RIG-02).
    // Mutations stay in `overlay`; the caller's block-entry view is unchanged.
    CCoinsViewCache overlay(const_cast<CCoinsViewCache*>(&view_at_block_entry));
    std::vector<uint256> convergence_txids;
    convergence_txids.reserve(vtx.size());
    for (size_t i = 1; i < vtx.size(); ++i) {
        const CTransaction& tx = *vtx[i];
        if (IsConvergenceDrawMember(tx, overlay, block_height, consensus_params)) {
            convergence_txids.push_back(tx.GetHash());
        }
        for (const auto& txin : tx.vin) {
            overlay.SpendCoin(txin.prevout);
        }
        AddCoins(overlay, tx, block_height, /*check=*/true);
    }
    return convergence_txids;
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
    const uint256& expected_convergence_root,
    const uint256& expected_paid_winner,
    std::string& err_reason) noexcept
{
    if (!ValidateCoinbaseConvergenceRootScriptSig(scriptSig, nHeight, expected_convergence_root, err_reason)) {
        return false;
    }
    CoinbaseConvergenceRootScriptSig parsed;
    if (!ParseCoinbaseConvergenceRootScriptSig(scriptSig, parsed, err_reason)) {
        return false;
    }
    if (parsed.paid_winner_txid != expected_paid_winner) {
        err_reason = "convergence-root-cb-paid-winner-mismatch";
        return false;
    }
    return true;
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
