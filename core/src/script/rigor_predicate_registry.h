// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SCRIPT_RIGOR_PREDICATE_REGISTRY_H
#define BITCOIN_SCRIPT_RIGOR_PREDICATE_REGISTRY_H

#include <script/predicate_verify.h>
#include <script/script.h>
#include <script/standard.h>

#include <optional>

/**
 * P0-027 closed v0.1 predicate registry.
 *
 * Lookup is ClassifyRigorTxout (the unique ScriptPubKey classifier). This
 * header does not match templates itself and does not add spend paths.
 *
 * Future upgrades add a new RigorPredicateID + dedicated verifier (hard fork
 * / coordinated activation). They do not activate reserved bytes as opcodes
 * (no OP_NEW_SIGCHECK / OP_NEW_VM / OP_NEW_NOP).
 */
enum class RigorPredicateID {
    ORU_V0_KEYHASH,
    RES_V1_NATIVE_KEY,
    RES_MINT_V0,
    CONVERGE_V0,
    OP_RETURN_BURN,
};

static constexpr size_t LIVE_RIGOR_PREDICATE_COUNT = 5;

inline std::optional<RigorPredicateID> LookupRigorPredicate(const CScript& scriptPubKey)
{
    std::vector<std::vector<unsigned char>> sols;
    switch (ClassifyRigorTxout(scriptPubKey, sols)) {
    case TxoutType::WITNESS_V0_KEYHASH:
        return RigorPredicateID::ORU_V0_KEYHASH;
    case TxoutType::WITNESS_V1_RES:
        return RigorPredicateID::RES_V1_NATIVE_KEY;
    case TxoutType::RES_MINT:
        return RigorPredicateID::RES_MINT_V0;
    case TxoutType::CONVERGE:
        return RigorPredicateID::CONVERGE_V0;
    case TxoutType::NULL_DATA:
        return RigorPredicateID::OP_RETURN_BURN;
    default:
        return std::nullopt;
    }
}

inline bool IsLiveRigorPredicateSpendable(RigorPredicateID id)
{
    switch (id) {
    case RigorPredicateID::ORU_V0_KEYHASH:
    case RigorPredicateID::RES_V1_NATIVE_KEY:
    case RigorPredicateID::RES_MINT_V0:
        return true;
    case RigorPredicateID::CONVERGE_V0:
    case RigorPredicateID::OP_RETURN_BURN:
        return false;
    }
    return false;
}

/** Same consensus function as VerifyRigorPredicate. Registry naming only. */
inline bool VerifyRigorRegistryPredicate(const CScript& scriptSig, const CScript& scriptPubKey,
                                         const CScriptWitness* witness, unsigned int flags,
                                         const BaseSignatureChecker& checker, ScriptError* serror = nullptr)
{
    return VerifyRigorPredicate(scriptSig, scriptPubKey, witness, flags, checker, serror);
}

#endif // BITCOIN_SCRIPT_RIGOR_PREDICATE_REGISTRY_H
