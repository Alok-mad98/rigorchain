// Copyright (c) 2017-2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/tx_check.h>

#include <consensus/trace/consensus_trace.h>

#include <consensus/amount.h>
#include <consensus/res_amount.h>
#include <consensus/res_proof.h>
#include <primitives/transaction.h>
#include <consensus/validation.h>
#include <script/predicate_verify.h>
#include <script/rigor_tags.h>
#include <script/standard.h>

#include <algorithm>
#include <string>

namespace Consensus {

CScript BuildCoinbaseScriptSig(int height)
{
    // See the note on the declaration: the trailing push exists so heights 1..16 clear
    // COINBASE_SCRIPTSIG_MIN_SIZE. ConnectBlock checks the height prefix only.
    return CScript() << height << RIGOR_PUSH_0;
}

/**
 * RIGOR consensus: Only witness v0 P2WPKH (ORU), witness v1 32-byte RES,
 * RIGOR_MARKER_RES_MINT, and RIGOR_MARKER_CONVERGE are allowed. P2WSH and all other script types
 * are rejected.
 */
bool IsAllowedScript(const CScript& scriptPubKey)
{
    const AssetType a = AssetTypeFromScriptPubKey(scriptPubKey);
    return a == ASSET_ORU || a == ASSET_RES;
}

bool CheckTxOutAssetWitnessBinding(const CTxOut& out, TxValidationState& state)
{
    std::vector<std::vector<unsigned char>> sols;
    const TxoutType t = ClassifyRigorTxout(out.scriptPubKey, sols);
    // Only witness v0, witness v1 (RES), or RIGOR_MARKER_RETURN are valid chain outputs on RigorChain.
    if (t != TxoutType::WITNESS_V0_KEYHASH &&
        t != TxoutType::WITNESS_V1_RES && t != TxoutType::RES_MINT && t != TxoutType::CONVERGE &&
        t != TxoutType::NULL_DATA) {
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-txns-asset-script-nonstandard");
    }
    return true;
}
} // namespace Consensus

bool CheckTransaction(const CTransaction& tx, TxValidationState& state)
{
    struct RigorCheckTxTrace {
        const CTransaction& tx;
        TxValidationState& state;
        ~RigorCheckTxTrace()
        {
            const bool ok = !state.IsInvalid();
            const std::string r = state.GetRejectReason();
            const bool amount_ok = ok || (r.find("vout") == std::string::npos && r.find("overflow") == std::string::npos &&
                                          r.find("toolarge") == std::string::npos && r.find("negative") == std::string::npos);
            const bool script_ok = ok || (r.find("script") == std::string::npos && r.find("asset") == std::string::npos);
            RigorConsensusTraceNoteTx(tx.GetHash(), "CheckTransaction", false, true, script_ok, amount_ok && ok, 0, r.c_str());
        }
    } rigor_checktx_trace{tx, state};

    // Basic checks that don't depend on any context
    if (tx.vin.empty())
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-txns-vin-empty");
    if (tx.vout.empty())
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-txns-vout-empty");
    // Size limits (this doesn't take the witness into account, as that hasn't been checked for malleability)
    if (::GetSerializeSize(tx, PROTOCOL_VERSION | SERIALIZE_TRANSACTION_NO_WITNESS) * WITNESS_SCALE_FACTOR > MAX_BLOCK_WEIGHT)
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-txns-oversize");

    // Output vs input asset matching is enforced in Consensus::CheckTxInputs (inputs define domain).
    // Coinbase RES is rejected in ConnectBlock (`bad-cb-res-amount`) except mockable-chain
    // Consensus::Params::res_wallet_bootstrap_coinbase (set at chain construction). This
    // function has no chainparams, so it cannot make that distinction.

    // Check for negative or overflow output values per asset (see CVE-2010-5139)
    CAmount oru_out_total = 0;
    CAmount res_out_total = 0;
    for (const auto& txout : tx.vout)
    {
        // Prunable / unspendable outputs (RIGOR_MARKER_RETURN commitments, e.g. CVGE) are not witness programs;
        // they are allowed at nValue==0 only. Must precede IsAllowedScript.
        if (txout.scriptPubKey.IsUnspendable()) {
            if (txout.nValue != 0) {
                return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-txns-opreturn-value");
            }
            if (Consensus::IsPcorbCarrierScript(txout.scriptPubKey)) {
                return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-pcorb-carrier");
            }
            if (!Consensus::CheckTxOutAssetWitnessBinding(txout, state)) {
                return false;
            }
            continue;
        }
        // RIGOR consensus: Only witness v0 (ORU) and witness v1 with 32-byte program (RES) are allowed.
        if (!Consensus::IsAllowedScript(txout.scriptPubKey)) {
            return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-rigor-script-type");
        }
        if (!Consensus::CheckTxOutAssetWitnessBinding(txout, state)) {
            return false;
        }
        if (!tx.IsCoinBase() && IsResMintScript(txout.scriptPubKey)) {
            return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-txns-res-mint-noncoinbase");
        }
        if (txout.nValue < 0)
            return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-txns-vout-negative");
        if (txout.nValue > MAX_MONEY)
            return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-txns-vout-toolarge");
        const AssetType out_asset = AssetTypeFromScriptPubKey(txout.scriptPubKey);
        if (out_asset == ASSET_INVALID) {
            return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-rigor-script-type");
        }
        if (out_asset == ASSET_NONE) {
            if (txout.nValue != 0) {
                return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-txns-opreturn-value");
            }
            continue;
        }
        if (out_asset == ASSET_ORU) {
            // Check before adding: `total += v` on two in-range amounts is only safe
            // because MAX_MONEY leaves int64 headroom, and relying on that is how
            // CVE-2010-5139 was reintroduced downstream. See AddMoneyOverflows.
            if (AddMoneyOverflows(oru_out_total, txout.nValue))
                return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-txns-txouttotal-toolarge");
            oru_out_total += txout.nValue;
        } else if (out_asset == ASSET_RES) {
            if (!IsValidResConsensusAmount(txout.nValue)) {
                return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-txns-res-amount-invalid");
            }
            if (!IsValidResOutputValue(txout.nValue)) {
                return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-txns-res-output-nonpositive");
            }
            if (AddMoneyOverflows(res_out_total, txout.nValue))
                return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-txns-txouttotal-toolarge");
            res_out_total += txout.nValue;
        } else {
            return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-rigor-script-type");
        }
    }

    // Check for duplicate inputs (see CVE-2018-17144)
    // While Consensus::CheckTxInputs does check if all inputs of a tx are available, and UpdateCoins marks all inputs
    // of a tx as spent, it does not check if the tx has duplicate inputs.
    // Failure to run this check will result in either a crash or an inflation bug, depending on the implementation of
    // the underlying coins database.
    std::set<COutPoint> vInOutPoints;
    for (const auto& txin : tx.vin) {
        if (!vInOutPoints.insert(txin.prevout).second)
            return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-txns-inputs-duplicate");
    }

    if (tx.IsCoinBase())
    {
        if (tx.vin[0].scriptSig.size() < Consensus::COINBASE_SCRIPTSIG_MIN_SIZE ||
            tx.vin[0].scriptSig.size() > Consensus::COINBASE_SCRIPTSIG_MAX_SIZE)
            return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-cb-length");
    }
    else
    {
        for (const auto& txin : tx.vin)
            if (txin.prevout.IsNull())
                return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-txns-prevout-null");
    }

    return true;
}
