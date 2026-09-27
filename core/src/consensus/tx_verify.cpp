// Copyright (c) 2017-2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/tx_verify.h>

#include <consensus/trace/consensus_trace.h>

#include <chain.h>
#include <coins.h>
#include <consensus/amount.h>
#include <consensus/res_amount.h>
#include <consensus/convergenceres.h>
#include <consensus/consensus.h>
#include <consensus/params.h>
#include <consensus/validation.h>
#include <primitives/transaction.h>
#include <consensus/tx_check.h>
#include <consensus/res_issuance.h>
#include <consensus/res_proof.h>
#include <script/predicate_verify.h>
#include <script/script.h>
#include <script/standard.h>
#include <util/check.h>
#include <util/moneystr.h>

#include <cassert>
#include <optional>
#include <string>

bool IsFinalTx(const CTransaction &tx, int nBlockHeight, int64_t nBlockTime)
{
    if (tx.nLockTime == 0)
        return true;
    if ((int64_t)tx.nLockTime < ((int64_t)tx.nLockTime < LOCKTIME_THRESHOLD ? (int64_t)nBlockHeight : nBlockTime))
        return true;

    // Even if tx.nLockTime isn't satisfied by nBlockHeight/nBlockTime, a
    // transaction is still considered final if all inputs' nSequence ==
    // SEQUENCE_FINAL (0xffffffff), in which case nLockTime is ignored.
    //
    // Because of this behavior RIGOR_RESERVED_LEGACY_CLTV/CheckLockTime() will
    // also check that the spending input's nSequence != SEQUENCE_FINAL,
    // ensuring that an unsatisfied nLockTime value will actually cause
    // IsFinalTx() to return false here:
    for (const auto& txin : tx.vin) {
        if (!(txin.nSequence == CTxIn::SEQUENCE_FINAL))
            return false;
    }
    return true;
}

std::pair<int, int64_t> CalculateSequenceLocks(const CTransaction &tx, int flags, std::vector<int>& prevHeights, const CBlockIndex& block)
{
    assert(prevHeights.size() == tx.vin.size());

    // Will be set to the equivalent height- and time-based nLockTime
    // values that would be necessary to satisfy all relative lock-
    // time constraints given our view of block chain history.
    // The semantics of nLockTime are the last invalid height/time, so
    // use -1 to have the effect of any height or time being valid.
    int nMinHeight = -1;
    int64_t nMinTime = -1;

    // tx.nVersion is signed integer so requires cast to unsigned otherwise
    // we would be doing a signed comparison and half the range of nVersion
    // wouldn't support BIP 68.
    bool fEnforceBIP68 = static_cast<uint32_t>(tx.nVersion) >= 2
                      && flags & LOCKTIME_VERIFY_SEQUENCE;

    // Do not enforce sequence numbers as a relative lock time
    // unless we have been instructed to
    if (!fEnforceBIP68) {
        return std::make_pair(nMinHeight, nMinTime);
    }

    for (size_t txinIndex = 0; txinIndex < tx.vin.size(); txinIndex++) {
        const CTxIn& txin = tx.vin[txinIndex];

        // Sequence numbers with the most significant bit set are not
        // treated as relative lock-times, nor are they given any
        // consensus-enforced meaning at this point.
        if (txin.nSequence & CTxIn::SEQUENCE_LOCKTIME_DISABLE_FLAG) {
            // The height of this input is not relevant for sequence locks
            prevHeights[txinIndex] = 0;
            continue;
        }

        int nCoinHeight = prevHeights[txinIndex];

        if (txin.nSequence & CTxIn::SEQUENCE_LOCKTIME_TYPE_FLAG) {
            const int64_t nCoinTime{Assert(block.GetAncestor(std::max(nCoinHeight - 1, 0)))->GetMedianTimePast()};
            // NOTE: Subtract 1 to maintain nLockTime semantics
            // BIP 68 relative lock times have the semantics of calculating
            // the first block or time at which the transaction would be
            // valid. When calculating the effective block time or height
            // for the entire transaction, we switch to using the
            // semantics of nLockTime which is the last invalid block
            // time or height.  Thus we subtract 1 from the calculated
            // time or height.

            // Time-based relative lock-times are measured from the
            // smallest allowed timestamp of the block containing the
            // txout being spent, which is the median time past of the
            // block prior.
            nMinTime = std::max(nMinTime, nCoinTime + (int64_t)((txin.nSequence & CTxIn::SEQUENCE_LOCKTIME_MASK) << CTxIn::SEQUENCE_LOCKTIME_GRANULARITY) - 1);
        } else {
            nMinHeight = std::max(nMinHeight, nCoinHeight + (int)(txin.nSequence & CTxIn::SEQUENCE_LOCKTIME_MASK) - 1);
        }
    }

    return std::make_pair(nMinHeight, nMinTime);
}

bool EvaluateSequenceLocks(const CBlockIndex& block, std::pair<int, int64_t> lockPair)
{
    assert(block.pprev);
    int64_t nBlockTime = block.pprev->GetMedianTimePast();
    if (lockPair.first >= block.nHeight || lockPair.second >= nBlockTime)
        return false;

    return true;
}

bool SequenceLocks(const CTransaction &tx, int flags, std::vector<int>& prevHeights, const CBlockIndex& block)
{
    return EvaluateSequenceLocks(block, CalculateSequenceLocks(tx, flags, prevHeights, block));
}

unsigned int GetLegacySigOpCount(const CTransaction& tx)
{
    // RIGOR: cost is one per live spendable lock being created. Coinbase
    // scriptSig bytes and Bitcoin CHECKSIG/CHECKMULTISIG are not counted.
    unsigned int nSigOps = 0;
    for (const auto& txout : tx.vout) {
        std::vector<std::vector<unsigned char>> sols;
        switch (ClassifyRigorTxout(txout.scriptPubKey, sols)) {
        case TxoutType::WITNESS_V0_KEYHASH:
        case TxoutType::WITNESS_V1_RES:
        case TxoutType::RES_MINT:
            ++nSigOps;
            break;
        default:
            break;
        }
    }
    return nSigOps;
}

int64_t GetTransactionSigOpCost(const CTransaction& tx, const CCoinsViewCache& inputs, uint32_t flags)
{
    int64_t nSigOps = GetLegacySigOpCount(tx) * WITNESS_SCALE_FACTOR;

    if (tx.IsCoinBase())
        return nSigOps;

    for (unsigned int i = 0; i < tx.vin.size(); i++)
    {
        const Coin& coin = inputs.AccessCoin(tx.vin[i].prevout);
        assert(!coin.IsSpent());
        const CTxOut &prevout = coin.out;
        nSigOps += CountWitnessSigOps(tx.vin[i].scriptSig, prevout.scriptPubKey, &tx.vin[i].scriptWitness, flags);
    }
    return nSigOps;
}

namespace {

/** Per-pool input totals (POOL-T RES / POOL-F ORU fee). Never sum ORU+RES in one expression. */
struct TxInputPools {
    CAmount oru_in{0};
    CAmount res_in{0};
};

bool AccumulateTxInputPools(const CTransaction& tx, TxValidationState& state, const CCoinsViewCache& inputs,
                            int nSpendHeight, const Consensus::Params& consensus_params, TxInputPools& pools)
{
    pools = {};
    for (const CTxIn& txin : tx.vin) {
        const Coin& coin = inputs.AccessCoin(txin.prevout);
        if (coin.IsSpent()) {
            return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-txns-inputs-missingorspent");
        }

        if (IsConvergenceScript(coin.out.scriptPubKey)) {
            return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-txns-spend-convergence");
        }

        const AssetType in_asset = AssetTypeFromScriptPubKey(coin.out.scriptPubKey);
        if (in_asset == ASSET_NONE || in_asset == ASSET_INVALID) {
            return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-txns-spend-non-asset-input");
        }

        if (coin.RequiresMaturity()) {
            const int coin_height = static_cast<int>(coin.nHeight);
            if (nSpendHeight - coin_height < consensus_params.nCoinbaseMaturity) {
                if (coin.IsResMint() && !coin.IsCoinBase()) {
                    return state.Invalid(TxValidationResult::TX_PREMATURE_SPEND, "bad-txns-premature-spend-of-res-mint",
                        strprintf("tried to spend RES mint at depth %d", nSpendHeight - coin_height));
                }
                return state.Invalid(TxValidationResult::TX_PREMATURE_SPEND, "bad-txns-premature-spend-of-coinbase",
                    strprintf("tried to spend coinbase at depth %d", nSpendHeight - coin_height));
            }
        }

        if (!MoneyRange(coin.out.nValue)) {
            return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-txns-inputvalues-outofrange");
        }

        // Check before adding, per pool: a post-hoc MoneyRange on the running total
        // evaluates the overflowing addition first. See AddMoneyOverflows.
        if (in_asset == ASSET_ORU) {
            if (AddMoneyOverflows(pools.oru_in, coin.out.nValue)) {
                return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-txns-inputvalues-outofrange");
            }
            pools.oru_in += coin.out.nValue;
        } else if (in_asset != ASSET_RES) {
            return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-txns-spend-non-asset-input");
        } else {
            if (!IsValidResConsensusAmount(coin.out.nValue)) {
                return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-txns-res-input-invalid");
            }
            if (AddMoneyOverflows(pools.res_in, coin.out.nValue)) {
                return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-txns-inputvalues-outofrange");
            }
            pools.res_in += coin.out.nValue;
        }
    }
    return true;
}

bool CheckTxInputs_ORU(const CTransaction& tx, TxValidationState& state, const CCoinsViewCache& inputs, int nSpendHeight,
                       const Consensus::Params& consensus_params, CAmount& txfee)
{
    for (const auto& txout : tx.vout) {
        if (!Consensus::CheckTxOutAssetWitnessBinding(txout, state)) {
            return false;
        }
    }

    for (const auto& out : tx.vout) {
        if (IsConvergenceScript(out.scriptPubKey) && out.nValue <= 0) {
            return state.Invalid(TxValidationResult::TX_CONSENSUS, "convergence-amount-too-small");
        }
    }

    const int convergence_outputs = Consensus::CountConvergenceOutputs(tx);
    if (convergence_outputs > 1) {
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "invalid-convergence-structure");
    }
    if (convergence_outputs == 1) {
        const CAmount convergence_amount = Consensus::GetConvergenceAmount(tx);
        if (convergence_amount < Consensus::MIN_CONVERGENCE_AMOUNT) {
            return state.Invalid(TxValidationResult::TX_CONSENSUS, "convergence-amount-too-small");
        }
    }

    TxInputPools pools;
    if (!AccumulateTxInputPools(tx, state, inputs, nSpendHeight, consensus_params, pools)) {
        return false;
    }
    if (pools.res_in > 0) {
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "cross-asset-input");
    }

    const CAmount oru_out = tx.GetValueOut(ASSET_ORU);
    if (pools.oru_in < oru_out) {
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-txns-orum-in-belowout",
            strprintf("ORU value in (%s) < ORU value out (%s)", FormatMoney(pools.oru_in), FormatMoney(oru_out)));
    }

    const CAmount txfee_aux = pools.oru_in - oru_out;
    if (!MoneyRange(txfee_aux)) {
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-txns-fee-outofrange");
    }

    txfee = txfee_aux;
    return true;
}

/** POOL-T RES transfer + POOL-F ORU network fee (dual-pool). */
bool CheckTxInputs_RES_DualPool(const CTransaction& tx, TxValidationState& state, const CCoinsViewCache& inputs,
                                int nSpendHeight, const Consensus::Params& consensus_params, const TxInputPools& pools,
                                CAmount& txfee)
{
    if (Consensus::CountConvergenceOutputs(tx) > 0) {
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "convergence-must-be-orum");
    }

    for (const auto& txout : tx.vout) {
        if (!Consensus::CheckTxOutAssetWitnessBinding(txout, state)) {
            return false;
        }
    }

    const CAmount res_out = tx.GetValueOut(ASSET_RES);
    if (pools.res_in < res_out) {
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-txns-res-in-belowout",
            strprintf("RES value in (%s) < RES value out (%s)", FormatResAmount(pools.res_in), FormatResAmount(res_out)));
    }

    if (pools.oru_in == 0) {
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-txns-oru-fee-required");
    }

    const CAmount oru_out = tx.GetValueOut(ASSET_ORU);
    if (pools.oru_in < oru_out) {
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-txns-orum-in-belowout",
            strprintf("ORU value in (%s) < ORU value out (%s)", FormatMoney(pools.oru_in), FormatMoney(oru_out)));
    }

    const CAmount txfee_aux = pools.oru_in - oru_out;
    if (!MoneyRange(txfee_aux)) {
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-txns-fee-outofrange");
    }

    txfee = txfee_aux;
    return true;
}

} // namespace

// RFC-003 semantic domain: RES prevouts dominate (RES Spend even when ORU inputs pay POOL-F fee).
// ORU-only prevouts → ORU transfer or convergence. Coinbase does not use this.
std::optional<AssetType> Consensus::GetTransactionInputAsset(const CTransaction& tx, const CCoinsViewCache& inputs)
{
    if (tx.vin.empty()) {
        return std::nullopt;
    }
    bool has_oru = false;
    bool has_res = false;
    for (const CTxIn& txin : tx.vin) {
        const Coin& coin = inputs.AccessCoin(txin.prevout);
        if (coin.IsSpent()) {
            return std::nullopt;
        }
        const AssetType a = AssetTypeFromScriptPubKey(coin.out.scriptPubKey);
        if (a == ASSET_ORU) {
            has_oru = true;
        } else if (a == ASSET_RES) {
            has_res = true;
        } else {
            return std::nullopt;
        }
    }
    if (has_res) {
        return ASSET_RES;
    }
    if (has_oru) {
        return ASSET_ORU;
    }
    return std::nullopt;
}

bool Consensus::CheckTxInputs(const CTransaction& tx, TxValidationState& state, const CCoinsViewCache& inputs, int nSpendHeight, const Consensus::Params& consensus_params, CAmount& txfee)
{
    struct RigorCheckTxInputsTrace {
        const CTransaction& tx;
        TxValidationState& state;
        CAmount& txfee;
        bool have_inputs;
        ~RigorCheckTxInputsTrace()
        {
            const bool ok = !state.IsInvalid();
            const std::string r = state.GetRejectReason();
            const bool utxo_ok = have_inputs && r != "bad-txns-inputs-missingorspent";
            RigorConsensusTraceNoteTx(tx.GetHash(), "CheckTxInputs", true, utxo_ok && (ok || have_inputs), ok, ok, ok ? txfee : 0, r.c_str());
        }
    } rigor_inputs_trace{tx, state, txfee, false};

    const bool have_inputs = inputs.HaveInputs(tx);
    rigor_inputs_trace.have_inputs = have_inputs;
    if (!have_inputs) {
        return state.Invalid(TxValidationResult::TX_MISSING_INPUTS, "bad-txns-inputs-missingorspent",
                         strprintf("%s: inputs missing/spent", __func__));
    }

    TxInputPools pools;
    if (!AccumulateTxInputPools(tx, state, inputs, nSpendHeight, consensus_params, pools)) {
        return false;
    }

    const std::optional<AssetType> semantic_domain = GetTransactionInputAsset(tx, inputs);
    if (!semantic_domain.has_value()) {
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-txns-spend-non-asset-input");
    }

    const bool has_res_in = pools.res_in > 0;
    const bool has_oru_in = pools.oru_in > 0;

    // RFC-019. A transaction with no RES inputs that creates RES is a mint: the
    // ORU it destroys buys the RES it creates, at the epoch's destroy cost. The
    // burn is declared by the OP_CONVERGE output rather than inferred from
    // inputs minus outputs minus fee, so that the mint amount does not move
    // with fee estimation and so that burn and fee stay separable.
    //
    // Checked in aggregate after the loop; per-output it is only knowable that
    // *some* burn must exist.
    const CAmount res_mint_out = has_res_in ? 0 : tx.GetValueOut(ASSET_RES);
    const CAmount convergence_burn = Consensus::GetConvergenceAmount(tx);

    for (const auto& out : tx.vout) {
        if (IsPcorbCarrierScript(out.scriptPubKey) || IsPcorbCarrierOutput(out)) {
            return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-pcorb-carrier");
        }
        if (IsConvergenceScript(out.scriptPubKey)) {
            if (has_res_in || *semantic_domain != ASSET_ORU) {
                return state.Invalid(TxValidationResult::TX_CONSENSUS, "convergence-must-be-orum");
            }
            continue;
        }

        const AssetType out_asset = AssetTypeFromScriptPubKey(out.scriptPubKey);
        if (out_asset == ASSET_INVALID) {
            return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-rigor-script-type");
        }
        if (out_asset == ASSET_NONE) {
            continue;
        }

        if (out_asset == ASSET_RES) {
            if (!has_res_in && convergence_burn <= 0) {
                // A body transaction creates RES only by paying for it. With no
                // RES on the input side and no ORU destroyed, nothing funds it.
                return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-txns-res-mint-no-burn");
            }
            continue;
        }

        if (out_asset == ASSET_ORU) {
            if (!has_oru_in) {
                return state.Invalid(TxValidationResult::TX_CONSENSUS, "illegal-res-to-oru-conversion");
            }
            continue;
        }
    }

    if (res_mint_out > 0) {
        // RFC-020: there is no mint horizon. Height < 0 is rejected by
        // CheckResMintArithmetic (and by ResMintAuthorizedAtHeight).
        if (nSpendHeight < 0 || !Consensus::ResMintAuthorizedAtHeight(nSpendHeight)) {
            return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-txns-res-mint-negative-height");
        }
        if (res_mint_out > Consensus::ResDerivedMaxSupply()) {
            return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-txns-res-mint-cap-overshoot");
        }
        if (!Consensus::CheckResMintArithmetic(res_mint_out, convergence_burn, nSpendHeight)) {
            return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-txns-res-mint-underpaid",
                strprintf("RES out (%s atoms) exceeds what %s ORU buys at height %d",
                          FormatMoney(res_mint_out), FormatMoney(convergence_burn), nSpendHeight));
        }
    }

    if (!has_res_in) {
        if (!CheckTxInputs_ORU(tx, state, inputs, nSpendHeight, consensus_params, txfee)) {
            return false;
        }
    } else {
        if (!CheckTxInputs_RES_DualPool(tx, state, inputs, nSpendHeight, consensus_params, pools, txfee)) {
            return false;
        }
    }

    // POOL-F: network fee is always ORU surplus on the ORU leg — never RES deltas.
    const CAmount oru_outputs = tx.GetValueOut(ASSET_ORU);
    Assert(pools.oru_in >= oru_outputs);
    Assert(txfee == pools.oru_in - oru_outputs);
    return true;
}

bool IsORBClaimTx(const CTransaction& tx, const CCoinsViewCache& inputs)
{
    const std::optional<AssetType> input_asset = Consensus::GetTransactionInputAsset(tx, inputs);
    if (!input_asset.has_value()) return false;
    return *input_asset == ASSET_RES;
}
