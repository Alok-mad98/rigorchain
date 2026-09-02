// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SCRIPT_STANDARD_H
#define BITCOIN_SCRIPT_STANDARD_H

/*
 * ============================================================================
 *  Rigorchain Architecture Guard
 *  Monetary truth is the UTXO set. Scripts classify outputs; they are not assets.
 *  Witness v1 is native RES transfer (Ed25519 key path). Convergence tickets are the
 *  separate `RIGOR_MARKER_CONVERGE` lock, not a witness v1 program. Not BIP341.
 *  Any BIP341/BIP342 interpretation of v1 programs is out of scope by design.
 * ============================================================================
 */

#include <attributes.h>
#include <consensus/amount.h>
#include <pubkey.h>
#include <script/rigor_tags.h>
#include <script/predicate_verify.h>
#include <uint256.h>
#include <util/hash_type.h>

#include <map>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>

static const bool DEFAULT_ACCEPT_DATACARRIER = true;

class CKeyID;
class CScript;
struct ScriptHash;
class CCoinsViewCache;

/** A reference to a CScript: the Hash160 of its serialization (see script.h) */
class CScriptID : public BaseHash<uint160>
{
public:
    CScriptID() : BaseHash() {}
    explicit CScriptID(const CScript& in);
    explicit CScriptID(const uint160& in) : BaseHash(in) {}
    explicit CScriptID(const ScriptHash& in);
};

/**
 * Default setting for -datacarriersize. 80 bytes of data, +1 for RIGOR_MARKER_RETURN,
 * +2 for the pushdata opcodes.
 */
static const unsigned int MAX_OP_RETURN_RELAY = 83;

/**
 * Mandatory script verification flags that all new blocks must comply with for
 * them to be valid. Witness verification is mandatory: without SCRIPT_VERIFY_WITNESS
 * every native output would evaluate as anyone-can-spend.
 *
 * Failing one of these tests may trigger a DoS ban - see CheckInputScripts() for
 * details.
 */
static const unsigned int MANDATORY_SCRIPT_VERIFY_FLAGS = SCRIPT_VERIFY_WITNESS;

enum class TxoutType {
    NONSTANDARD,
    /** LEGACY_ONLY: unreachable from ClassifyRigorTxout. Kept for existing switches. */
    PUBKEY,
    /** LEGACY_ONLY: unreachable from ClassifyRigorTxout (25-byte P2PKH is NONSTANDARD). Kept for existing switches. */
    PUBKEYHASH,
    /** LEGACY_ONLY: unreachable from ClassifyRigorTxout. Kept for existing switches. */
    SCRIPTHASH,
    /** LEGACY_ONLY: unreachable from ClassifyRigorTxout. Kept for existing switches. */
    MULTISIG,
    NULL_DATA, //!< unspendable RIGOR_MARKER_RETURN script that carries data
    /** LEGACY_ONLY: unreachable from ClassifyRigorTxout. Kept for existing switches. */
    WITNESS_V0_SCRIPTHASH,
    WITNESS_V0_KEYHASH,
    /** Witness version 1, 32-byte program (RigorChain RES asset; not BIP341 Taproot). */
    WITNESS_V1_RES,
    /** RES mint lock: `<maturity_height> <20-byte pubkey_hash> RIGOR_MARKER_RES_MINT`. */
    RES_MINT,
    /** ORU convergence burn: `<20-byte HASH160(pk32)> RIGOR_MARKER_CONVERGE`. Unspendable.
     *  Under Option B, HASH160(pk32) equals HASH160(v1 program). */
    CONVERGE,
    // CVG_CLAIM was here, for the retired OP_CVG_CLAIM covenant lock (see script.h on 0xbc).
    /** LEGACY_ONLY: unreachable from ClassifyRigorTxout (unknown witness versions are NONSTANDARD). Kept for existing switches. */
    WITNESS_UNKNOWN,
};

class CNoDestination {
public:
    friend bool operator==(const CNoDestination&, const CNoDestination&) { return true; }
    friend bool operator<(const CNoDestination&, const CNoDestination&) { return true; }
};

struct PKHash : public BaseHash<uint160>
{
    PKHash() : BaseHash() {}
    explicit PKHash(const uint160& hash) : BaseHash(hash) {}
    explicit PKHash(const CPubKey& pubkey);
    explicit PKHash(const CKeyID& pubkey_id);
};
CKeyID ToKeyID(const PKHash& key_hash);

struct WitnessV0KeyHash;
struct ScriptHash : public BaseHash<uint160>
{
    ScriptHash() : BaseHash() {}
    // These don't do what you'd expect.
    // Use ScriptHash(GetScriptForDestination(...)) instead.
    explicit ScriptHash(const WitnessV0KeyHash& hash) = delete;
    explicit ScriptHash(const PKHash& hash) = delete;

    explicit ScriptHash(const uint160& hash) : BaseHash(hash) {}
    explicit ScriptHash(const CScript& script);
    explicit ScriptHash(const CScriptID& script);
};

struct WitnessV0ScriptHash : public BaseHash<uint256>
{
    WitnessV0ScriptHash() : BaseHash() {}
    explicit WitnessV0ScriptHash(const uint256& hash) : BaseHash(hash) {}
    explicit WitnessV0ScriptHash(const CScript& script);
};

struct WitnessV0KeyHash : public BaseHash<uint160>
{
    WitnessV0KeyHash() : BaseHash() {}
    explicit WitnessV0KeyHash(const uint160& hash) : BaseHash(hash) {}
    explicit WitnessV0KeyHash(const CPubKey& pubkey);
    explicit WitnessV0KeyHash(const PKHash& pubkey_hash);
};
CKeyID ToKeyID(const WitnessV0KeyHash& key_hash);

/** 32-byte Ed25519 public key used as the native witness v1 RES program (Option B).
 *  Not a Taproot x-only key and not BIP341. */
struct WitnessV1Res : public BaseHash<uint256>
{
    WitnessV1Res() : BaseHash() {}
    explicit WitnessV1Res(const uint256& program) : BaseHash(program) {}
    explicit WitnessV1Res(const XOnlyPubKey& xpk)
        : BaseHash(uint256{Span{xpk.data(), xpk.size()}}) {}

    static constexpr size_t size() { return uint256::size(); }
};

//! CTxDestination subtype to encode any future Witness version
struct WitnessUnknown
{
    unsigned int version;
    unsigned int length;
    unsigned char program[40];

    friend bool operator==(const WitnessUnknown& w1, const WitnessUnknown& w2) {
        if (w1.version != w2.version) return false;
        if (w1.length != w2.length) return false;
        return std::equal(w1.program, w1.program + w1.length, w2.program);
    }

    friend bool operator<(const WitnessUnknown& w1, const WitnessUnknown& w2) {
        if (w1.version < w2.version) return true;
        if (w1.version > w2.version) return false;
        if (w1.length < w2.length) return true;
        if (w1.length > w2.length) return false;
        return std::lexicographical_compare(w1.program, w1.program + w1.length, w2.program, w2.program + w2.length);
    }
};

/**
 * A txout script template with a specific destination. It is either:
 *  * CNoDestination: no destination set
 *  * PKHash: TxoutType::PUBKEYHASH destination (P2PKH)
 *  * ScriptHash: TxoutType::SCRIPTHASH destination (P2SH)
 *  * WitnessV0ScriptHash: TxoutType::WITNESS_V0_SCRIPTHASH destination (P2WSH)
 *  * WitnessV0KeyHash: TxoutType::WITNESS_V0_KEYHASH destination (P2WPKH)
 *  * WitnessV1Res: TxoutType::WITNESS_V1_RES destination (witness v1, 32-byte Ed25519 pubkey; not Taproot)
 *  * WitnessUnknown: TxoutType::WITNESS_UNKNOWN destination (P2W???)
 *  A CTxDestination is the internal data type encoded in a bitcoin address
 */
using CTxDestination = std::variant<CNoDestination, PKHash, ScriptHash, WitnessV0ScriptHash, WitnessV0KeyHash, WitnessV1Res, WitnessUnknown>;

/** Check whether a CTxDestination is a CNoDestination. */
bool IsValidDestination(const CTxDestination& dest);

/** Get the name of a TxoutType as a string */
std::string GetTxnOutputType(TxoutType t);

constexpr bool IsPushdataOp(opcodetype opcode)
{
    return opcode > RIGOR_PUSH_0 && opcode <= RIGOR_PUSHDATA4;
}

/**
 * Parse a scriptPubKey and identify script type for standard scripts. If
 * successful, returns script type and parsed pubkeys or hashes, depending on
 * the type. For example, for a P2SH script, vSolutionsRet will contain the
 * script hash, for P2PKH it will contain the key hash, etc.
 *
 * @param[in]   scriptPubKey   Script to parse
 * @param[out]  vSolutionsRet  Vector of parsed pubkeys and hashes
 * @return                     The script type. TxoutType::NONSTANDARD represents a failed solve.
 */
TxoutType Solver(const CScript& scriptPubKey, std::vector<std::vector<unsigned char>>& vSolutionsRet);

/**
 * Classify a transaction output for Rigor consensus / mempool policy: only witness v0 P2WPKH,
 * witness v1 (RES), RIGOR_MARKER_RES_MINT, RIGOR_MARKER_CONVERGE, or RIGOR_MARKER_RETURN. Defined once in rigor_classify.cpp.
 * P2WSH, legacy P2PKH/P2SH/bare pubkey/bare multisig are NONSTANDARD.
 */
TxoutType ClassifyRigorTxout(const CScript& scriptPubKey, std::vector<std::vector<unsigned char>>& vSolutionsRet);

/**
 * True iff `AssetTypeFromScriptPubKey` is not `ASSET_INVALID`.
 * Wallet / RPC / rigor-tx construction and watch gate (includes RIGOR_MARKER_RETURN).
 * Not “spendable”: RIGOR_MARKER_RETURN is constructible and unspendable.
 * `Consensus::IsAllowedScript` is the spendable subset (`ASSET_ORU` or `ASSET_RES`).
 */
bool IsRigorConstructibleScript(const CScript& scriptPubKey);

/**
 * Parse a scriptPubKey into a destination. Only native RIGOR locks succeed:
 * witness v0 P2WPKH and native witness v1 RES.
 */
bool ExtractDestination(const CScript& scriptPubKey, CTxDestination& addressRet);

/**
 * ORU-domain convergence burn: `<20-byte HASH160(pk32)> RIGOR_MARKER_CONVERGE`.
 * Recognition uses only this lock. The output is never inserted into the UTXO set
 * (`AddCoins` skips it). Witness v1 "CVG2" programs are retired and consensus-invalid.
 */
bool IsConvergenceV1ScriptPubKey(const CScript& scriptPubKey);
/** Alias of IsConvergenceScript (RIGOR_MARKER_CONVERGE lock). */
bool IsConvergenceScript(const CScript& scriptPubKey);
/** Witness v1, 32-byte program that is neither CVG2 nor RES2-prefixed (native RES). Not Bitcoin Taproot. */
bool IsResScript(const CScript& scriptPubKey);

/** Outputs that occupy a Coin in the UTXO set. Return-marker and convergence do not. */
inline bool EntersUtxoSet(const CScript& scriptPubKey)
{
    return !scriptPubKey.IsUnspendable() && !IsConvergenceScript(scriptPubKey);
}

/**
 * True iff every prevout is RES-classified (`GetTransactionInputAsset` == ASSET_RES).
 * Defined in consensus/tx_verify.cpp.
 */
bool IsORBClaimTx(const CTransaction& tx, const CCoinsViewCache& inputs);

class CBlock;

/** Parsed coinbase ``RIGOR_MARKER_RETURN`` commitment: ASCII ``RIGOR`` + 4-byte LE rng target nBits. */
struct RigorCommitment {
    uint32_t res_target_nbits{0};
};

bool IsRigorCommitment(const CScript& script_pub_key, RigorCommitment& commitment_out);
std::optional<RigorCommitment> ExtractRigorDrawTargetCommitment(const CBlock& block);

/**
 * Decode the 8-byte amount embedded in bytes [4,12) of the CVG2 program as **big-endian** uint64.
 * Consensus weight for convergence economics and draw always uses the matching output's ``nValue``;
 * this is for script introspection / tooling alignment with the program layout.
 */
bool ExtractConvergenceAmount(const CScript& scriptPubKey, CAmount& amount);
uint64_t GetConvergenceAmountFromScript(const CScript& scriptPubKey);

/**
 * Generate a scriptPubKey for the given destination. Only witness v0 P2WPKH
 * and witness v1 RES emit scripts; every other destination type is empty
 * (consensus-invalid).
 */
CScript GetScriptForDestination(const CTxDestination& dest);

// REMOVED: TaprootBuilder, TaprootSpendData, and related Taproot/MAST infrastructure
// violates RIGOR "Script as Asset" constitution (no Taproot/BIP341 script path complexity)

#endif // BITCOIN_SCRIPT_STANDARD_H
