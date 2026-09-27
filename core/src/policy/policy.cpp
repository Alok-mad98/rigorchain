// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// NOTE: This file is intended to be customised by the end user, and includes only local node policy logic

#include <policy/policy.h>

#include <script/rigor_tags.h>
#include <coins.h>
#include <consensus/amount.h>
#include <consensus/consensus.h>
#include <consensus/res_proof.h>
#include <consensus/res_amount.h>
#include <consensus/validation.h>
#include <crypto/ed25519_provider.h>
#include <policy/feerate.h>
#include <primitives/transaction.h>
#include <script/predicate_verify.h>
#include <script/script.h>
#include <script/standard.h>
#include <serialize.h>
#include <span.h>

#include <algorithm>
#include <cstddef>
#include <vector>

CAmount GetDustThreshold(const CTxOut& txout, const CFeeRate& dustRelayFeeIn)
{
    // "Dust" is defined in terms of dustRelayFee,
    // which has units satoshis-per-kilobyte.
    // If you'd pay more in fees than the value of the output
    // to spend something, then we consider it dust.
    // A typical spendable non-segwit txout is 34 bytes big, and will
    // need a CTxIn of at least 148 bytes to spend:
    // so dust is a spendable txout less than
    // 182*dustRelayFee/1000 (in satoshis).
    // 546 satoshis at the default rate of 3000 sat/kvB.
    // A typical spendable segwit P2WPKH txout is 31 bytes big, and will
    // need a CTxIn of at least 67 bytes to spend:
    // so dust is a spendable txout less than
    // 98*dustRelayFee/1000 (in satoshis).
    // 294 satoshis at the default rate of 3000 sat/kvB.
    // Dust is "costs more to spend than it is worth". Outputs that never enter
    // the UTXO set cannot be spent, so the threshold is zero. That includes
    // OP_RETURN-class scripts (IsUnspendable) and OP_CONVERGE (AddCoins skips
    // them). Do not treat a 1-atom burn as policy dust: consensus floor is one
    // ORU atom, and EntersUtxoSet() is the SSOT for "occupies a Coin".
    if (!EntersUtxoSet(txout.scriptPubKey))
        return 0;

    std::vector<std::vector<unsigned char>> vSolutions;
    const TxoutType rigor_type = ClassifyRigorTxout(txout.scriptPubKey, vSolutions);
    if (rigor_type == TxoutType::WITNESS_V1_RES || rigor_type == TxoutType::RES_MINT) {
        // RES amounts are in RES_ATOMIC_UNIT (1 = 0.00000001 RES); relay/min fee rates are in ORU
        // base units. Do not compare GetFee() (ORU-scale) to RES nValue — that misclassifies
        // almost all RES outputs as dust. Consensus already enforces a minimum output; policy
        // uses the smallest valid unit as the dust floor.
        return RES_ATOMIC_UNIT;
    }

    size_t nSize = GetSerializeSize(txout);
    int witnessversion = 0;
    std::vector<unsigned char> witnessprogram;

    // Dust size model for spending a witness v0 P2WPKH output (33-byte pubkey +
    // 64-byte Ed25519 sig + hashtype). Witness v1 RES uses the branch above.
    if (txout.scriptPubKey.IsWitnessProgram(witnessversion, witnessprogram)) {
        // sum the sizes of the parts of a transaction input
        // with 75% segwit discount applied to the script size.
        nSize += (32 + 4 + 1 + (107 / WITNESS_SCALE_FACTOR) + 4);
    } else {
        nSize += (32 + 4 + 1 + 107 + 4); // the 148 mentioned above
    }

    return dustRelayFeeIn.GetFee(nSize);
}

bool IsDust(const CTxOut& txout, const CFeeRate& dustRelayFeeIn)
{
    return (txout.nValue < GetDustThreshold(txout, dustRelayFeeIn));
}

bool IsStandard(const CScript& scriptPubKey, const std::optional<unsigned>& max_datacarrier_bytes, TxoutType& whichType)
{
    std::vector<std::vector<unsigned char> > vSolutions;
    whichType = ClassifyRigorTxout(scriptPubKey, vSolutions);

    switch (whichType) {
    case TxoutType::WITNESS_V1_RES:
 // a v1 program that is not a canonical Ed25519 point can
        // never be satisfied, so its coins are unspendable. Consensus accepts it
        // (classification is a pure byte predicate — see
        // IsRigorResNativeWitnessProgram), but the network declines to carry it,
        // which is what keeps it from being created by accident. Relay policy is
        // the right home for a check that costs a scalar multiplication and that
        // must never retroactively reclassify a confirmed UTXO.
        return !vSolutions.empty() && ed25519::IsCanonicalOnCurvePubkey(vSolutions[0]);
    case TxoutType::WITNESS_V0_KEYHASH:
    case TxoutType::RES_MINT:
    case TxoutType::CONVERGE:
        return true;
    case TxoutType::NULL_DATA:
        if (!max_datacarrier_bytes || scriptPubKey.size() > *max_datacarrier_bytes) {
            return false;
        }
        return true;
    default:
        return false;
    }
}

bool IsStandardTx(const CTransaction& tx, const std::optional<unsigned>& max_datacarrier_bytes, bool permit_bare_multisig, const CFeeRate& dust_relay_fee, std::string& reason)
{
    (void)permit_bare_multisig; // retained for RPC compatibility; bare multisig outputs are never standard on RigorChain
    reason.clear();
    if (tx.nVersion > TX_MAX_STANDARD_VERSION || tx.nVersion < 1) {
        reason = "version";
        return false;
    }

    // Extremely large transactions with lots of inputs can cost the network
    // almost as much to process as they cost the sender in fees, because
    // computing signature hashes is O(ninputs*txsize). Limiting transactions
    // to MAX_STANDARD_TX_WEIGHT mitigates CPU exhaustion attacks.
    unsigned int sz = GetTransactionWeight(tx);
    if (sz > MAX_STANDARD_TX_WEIGHT) {
        reason = "tx-size";
        return false;
    }

    for (const CTxIn& txin : tx.vin)
    {
        // Biggest 'standard' txin involving only keys is a 15-of-15 P2SH
        // multisig with compressed keys (remember the 520 byte limit on
        // redeemScript size). That works out to a (15*(33+1))+3=513 byte
        // redeemScript, 513+1+15*(73+1)+3=1627 bytes of scriptSig, which
        // we round off to 1650(MAX_STANDARD_SCRIPTSIG_SIZE) bytes for
        // some minor future-proofing. That's also enough to spend a
        // 20-of-20 CHECKMULTISIG scriptPubKey, though such a scriptPubKey
        // is not considered standard.
        if (txin.scriptSig.size() > MAX_STANDARD_SCRIPTSIG_SIZE) {
            reason = "scriptsig-size";
            return false;
        }
        if (!txin.scriptSig.IsPushOnly()) {
            reason = "scriptsig-not-pushonly";
            return false;
        }
    }

    unsigned int nDataOut = 0;
    TxoutType whichType;
    for (const CTxOut& txout : tx.vout) {
        if (!::IsStandard(txout.scriptPubKey, max_datacarrier_bytes, whichType)) {
            reason = "scriptpubkey";
            return false;
        }

        if (whichType == TxoutType::NULL_DATA)
            nDataOut++;
        else if (whichType == TxoutType::CONVERGE)
            continue; // never a UTXO; dust-as-spend-cost does not apply
        else if (IsDust(txout, dust_relay_fee)) {
            reason = "dust";
            return false;
        }
    }

    // only one RIGOR_MARKER_RETURN txout is permitted
    if (nDataOut > 1) {
        reason = "multi-op-return";
        return false;
    }

    return true;
}

/**
 * Check transaction inputs to mitigate two
 * potential denial-of-service attacks:
 *
 * 1. scriptSigs with extra data stuffed into them,
 *    not consumed by scriptPubKey (or P2SH script)
 * 2. P2SH scripts with a crazy number of expensive
 *    CHECKSIG/CHECKMULTISIG operations
 *
 * Why bother? To avoid denial-of-service attacks; an attacker
 * can submit a standard HASH... RIGOR_RESERVED_LEGACY_EQUAL transaction,
 * which will get accepted into blocks. The redemption
 * script can be anything; an attacker could use a very
 * expensive-to-check-upon-redemption script like:
 *   DUP CHECKSIG DROP ... repeated 100 times... RIGOR_PUSH_1
 *
 * Note that only the non-witness portion of the transaction is checked here.
 */
bool AreInputsStandard(const CTransaction& tx, const CCoinsViewCache& mapInputs)
{
    if (tx.IsCoinBase()) {
        return true; // Coinbases don't use vin normally
    }

    for (unsigned int i = 0; i < tx.vin.size(); i++) {
        const CTxOut& prev = mapInputs.AccessCoin(tx.vin[i].prevout).out;

        std::vector<std::vector<unsigned char> > vSolutions;
        TxoutType whichType = ClassifyRigorTxout(prev.scriptPubKey, vSolutions);
        if (whichType != TxoutType::WITNESS_V0_KEYHASH &&
            whichType != TxoutType::WITNESS_V1_RES && whichType != TxoutType::RES_MINT) {
            return false;
        }
    }

    return true;
}

bool IsWitnessStandard(const CTransaction& tx, const CCoinsViewCache& mapInputs)
{
    if (tx.IsCoinBase())
        return true; // Coinbases are skipped

    for (unsigned int i = 0; i < tx.vin.size(); i++)
    {
        // We don't care if witness for this input is empty, since it must not be bloated.
        // If the script is invalid without witness, it would be caught sooner or later during validation.
        if (tx.vin[i].scriptWitness.IsNull())
            continue;

        const CTxOut &prev = mapInputs.AccessCoin(tx.vin[i].prevout).out;

        const CScript& prevScript = prev.scriptPubKey;

        int witnessversion = 0;
        std::vector<unsigned char> witnessprogram;

        // Non-witness program must not be associated with any witness
        if (!prevScript.IsWitnessProgram(witnessversion, witnessprogram))
            return false;

        // P2WSH is not a consensus output. Reject leftover v0/32 spends at policy too.
        if (witnessversion == 0 && witnessprogram.size() != WITNESS_V0_KEYHASH_SIZE) {
            return false;
        }

        // Witness v1 RES: 2-item Ed25519 key path only.
        // Align relay policy with VerifyWitnessProgram so non-matching stack depths are non-standard.
        // SECURITY-001-A / A-7 removed the 11-item PCORB script path; encoding checks stay deferred
        // to script verification.
        if (witnessversion == 1 && witnessprogram.size() == WITNESS_V1_RES_SIZE) {
            const std::vector<std::vector<unsigned char>>& st = tx.vin[i].scriptWitness.stack;
            if (IsRigorResNativeWitnessProgram(witnessprogram)) {
                if (st.size() != 2) return false;
            }
        }
    }
    return true;
}

// RIGOR Fee Mechanism: GetVirtualTransactionSize is DELETED
// Use rigor::GetTransactionSizeBytes instead (exact byte count, NO weight/vsize)
