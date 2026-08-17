// Copyright (c) 2017-2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/tx_check.h>

#include <consensus/amount.h>
#include <consensus/res_amount.h>
#include <primitives/transaction.h>
#include <consensus/validation.h>
#include <script/standard.h>

namespace Consensus {

/**
 * RIGOR consensus: Only witness v0 (ORU) and witness v1 with 32-byte program (RES) are allowed.
 * All other script types (P2PKH, P2SH, bare multisig, Taproot with control block, etc.) are rejected.
 * This is enforced at consensus level - parsing layer remains compatible for historical data.
 */
bool IsAllowedScript(const CScript& scriptPubKey)
{
    int version;
    std::vector<unsigned char> program;

    if (!scriptPubKey.IsWitnessProgram(version, program))
        return false;

    if (version == 0)
        return true;

    if (version == 1 && program.size() == 32)
        return true;

    return false;
}

bool CheckTxOutAssetWitnessBinding(const CTxOut& out, TxValidationState& state)
{
    std::vector<std::vector<unsigned char>> sols;
    const TxoutType t = ClassifyRigorTxout(out.scriptPubKey, sols);
    // Only witness v0, witness v1 (RES), or OP_RETURN are valid chain outputs on RigorChain.
    if (t != TxoutType::WITNESS_V0_KEYHASH && t != TxoutType::WITNESS_V0_SCRIPTHASH &&
        t != TxoutType::WITNESS_V1_RES && t != TxoutType::NULL_DATA) {
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-txns-asset-script-nonstandard");
    }
    return true;
}
} // namespace Consensus

bool CheckTransaction(const CTransaction& tx, TxValidationState& state)
{
    // Basic checks that don't depend on any context
    if (tx.vin.empty())
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-txns-vin-empty");
    if (tx.vout.empty())
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-txns-vout-empty");
    // Size limits (this doesn't take the witness into account, as that hasn't been checked for malleability)
    if (::GetSerializeSize(tx, PROTOCOL_VERSION | SERIALIZE_TRANSACTION_NO_WITNESS) * WITNESS_SCALE_FACTOR > MAX_BLOCK_WEIGHT)
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-txns-oversize");

    // Output vs input asset matching is enforced in Consensus::CheckTxInputs (inputs define domain).
    // Coinbase RES is rejected in ConnectBlock (`bad-cb-res-amount`) except the mockable-chain
    // -reswalletbootstrap backdoor. This function has no chainparams, so it cannot make that
    // distinction.

    // Check for negative or overflow output values per asset (see CVE-2010-5139)
    CAmount oru_out_total = 0;
    CAmount res_out_total = 0;
    for (const auto& txout : tx.vout)
    {
        // Prunable / unspendable outputs (OP_RETURN commitments, e.g. CVGE) are not witness programs;
        // they are allowed at nValue==0 only. Must precede IsAllowedScript.
        if (txout.scriptPubKey.IsUnspendable()) {
            if (txout.nValue != 0) {
                return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-txns-opreturn-value");
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
        if (txout.nValue < 0)
            return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-txns-vout-negative");
        if (txout.nValue > MAX_MONEY)
            return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-txns-vout-toolarge");
        const AssetType out_asset = AssetTypeFromScriptPubKey(txout.scriptPubKey);
        if (out_asset == ASSET_NONE) {
            if (txout.nValue != 0) {
                return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-txns-opreturn-value");
            }
            continue;
        }
        if (out_asset == ASSET_ORU) {
            oru_out_total += txout.nValue;
            if (!MoneyRange(oru_out_total))
                return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-txns-txouttotal-toolarge");
        } else {
            if (!IsValidResConsensusAmount(txout.nValue)) {
                return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-txns-res-amount-invalid");
            }
            // RES outputs must meet RES_DUST_THRESHOLD (0.01) and atomic granularity (res_amount.h).
            if (!IsValidResOutputValue(txout.nValue)) {
                return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-txns-res-output-nonpositive");
            }
            res_out_total += txout.nValue;
            if (!MoneyRange(res_out_total))
                return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-txns-txouttotal-toolarge");
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
        if (tx.vin[0].scriptSig.size() < 2 || tx.vin[0].scriptSig.size() > 100)
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
