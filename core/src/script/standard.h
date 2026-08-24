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
 *  separate `OP_CONVERGE` lock, not a witness v1 program. Not BIP341.
 *  Any BIP341/BIP342 interpretation of v1 programs is out of scope by design.
 * ============================================================================
 */

#include <attributes.h>
#include <consensus/amount.h>
#include <pubkey.h>
#include <script/rigor_tags.h>
#include <script/interpreter.h>
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
 * Default setting for -datacarriersize. 80 bytes of data, +1 for OP_RETURN,
 * +2 for the pushdata opcodes.
 */
static const unsigned int MAX_OP_RETURN_RELAY = 83;

/**
 * Mandatory script verification flags that all new blocks must comply with for
 * them to be valid. (but old blocks may not comply with) Currently just P2SH,
 * but in the future other flags may be added.
 *
 * Failing one of these tests may trigger a DoS ban - see CheckInputScripts() for
 * details.
 */
static const unsigned int MANDATORY_SCRIPT_VERIFY_FLAGS = SCRIPT_VERIFY_P2SH;

enum class TxoutType {
    NONSTANDARD,
    // 'standard' transaction types:
    PUBKEY,
    PUBKEYHASH,
    SCRIPTHASH,
    MULTISIG,
    NULL_DATA, //!< unspendable OP_RETURN script that carries data
    WITNESS_V0_SCRIPTHASH,
    WITNESS_V0_KEYHASH,
    /** Witness version 1, 32-byte program (RigorChain RES asset; not BIP341 Taproot). */
    WITNESS_V1_RES,
    /** RES mint lock: `<maturity_height> <20-byte pubkey_hash> OP_RES_MINT`. */
    RES_MINT,
    /** ORU convergence burn: `<20-byte HASH160(RES program)> OP_CONVERGE`. Unspendable. */
    CONVERGE,
    // CVG_CLAIM was here, for the retired OP_CVG_CLAIM covenant lock (see script.h on 0xbc).
    WITNESS_UNKNOWN, //!< Only for Witness versions not already defined above
};

class CNoDestination {
public:
    friend bool operator==(const CNoDestination &a, const CNoDestination &b) { return true; }
    friend bool operator<(const CNoDestination &a, const CNoDestination &b) { return true; }
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

struct WitnessV1Res : public XOnlyPubKey
{
    WitnessV1Res() : XOnlyPubKey() {}
    explicit WitnessV1Res(const XOnlyPubKey& xpk) : XOnlyPubKey(xpk) {}
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
 *  * WitnessV1Res: TxoutType::WITNESS_V1_RES destination (witness v1, SHA256 of 32-byte Ed25519 pubkey; not Taproot)
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
    return opcode > OP_FALSE && opcode <= OP_PUSHDATA4;
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
 * Classify a transaction output for Rigor consensus / mempool policy: only witness v0, witness v1 (RES),
 * or OP_RETURN. Legacy P2PKH/P2SH/bare pubkey/bare multisig are not recognized here (NONSTANDARD).
 * Signing and redeem-script analysis still use the full Solver().
 */
TxoutType ClassifyRigorTxout(const CScript& scriptPubKey, std::vector<std::vector<unsigned char>>& vSolutionsRet);

/**
 * Parse a standard scriptPubKey for the destination address. Assigns result to
 * the addressRet parameter and returns true if successful. Currently only works for P2PK,
 * P2PKH, P2SH, P2WPKH, and P2WSH scripts.
 */
bool ExtractDestination(const CScript& scriptPubKey, CTxDestination& addressRet);

/**
 * ORU-domain convergence burn: `<20-byte HASH160(RES program)> OP_CONVERGE`.
 * Recognition uses only this lock. The output is never inserted into the UTXO set
 * (`AddCoins` skips it). Witness v1 "CVG2" programs are retired and consensus-invalid.
 */
bool IsConvergenceV1ScriptPubKey(const CScript& scriptPubKey);
/** Alias of IsConvergenceScript (OP_CONVERGE lock). */
bool IsConvergenceScript(const CScript& scriptPubKey);
/** Witness v1, 32-byte program that is neither CVG2 nor RES2-prefixed (native RES). Not Bitcoin Taproot. */
bool IsResScript(const CScript& scriptPubKey);

/** RFC-ADDRESS-001: Validate address-asset binding (witness version matches asset type) */
bool ValidateAddressAssetBinding(const CTxDestination& dest, AssetType asset);

/**
 * RIGOR V1 ScriptPubKey structure: OP_1 <32-byte program>
 * This is the canonical V1 program checker. Scripts classify; they do not mint
 * and they are not an asset class. V1 = SegWit Version 1 container (32-byte
 * program) for native RES transfers. NOT Taproot, NOT MAST, no control block.
 */
bool IsV1Program(const CScript& script);

/**
 * RIGOR V1 dual-channel model: semantic = f(input_asset_domain)
 * These functions implement the formal specification for channel determination.
 * Channel A (ORI): ORB Mint
 * Channel B (ORB): ORB Claim / Spend
 * Note: These functions are defined in consensus/tx_verify.cpp to avoid linking issues.
 */

/**
 * Check if transaction is an ORB Mint transaction.
 * Per RIGOR constitution: HasORBOutput(tx) && GetTransactionInputAsset(tx) == ASSET_ORI
 */
bool IsORBMintTx(const CTransaction& tx, const CCoinsViewCache& inputs);

/**
 * Check if transaction is an ORB Claim transaction.
 * Per RIGOR constitution: GetTransactionInputAsset(tx) == ASSET_ORB
 */
bool IsORBClaimTx(const CTransaction& tx, const CCoinsViewCache& inputs);

class CBlock;

/** Parsed coinbase ``OP_RETURN`` commitment: ASCII ``RIGOR`` + 4-byte LE rng target nBits. */
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
 * Generate a Bitcoin scriptPubKey for the given CTxDestination. Returns a P2PKH
 * script for a CKeyID destination, a P2SH script for a CScriptID, and an empty
 * script for CNoDestination.
 */
CScript GetScriptForDestination(const CTxDestination& dest);

/** Generate a P2PK script for the given pubkey. */
CScript GetScriptForRawPubKey(const CPubKey& pubkey);

/** Determine if script is a "multi_a" script. Returns (threshold, keyspans) if so, and nullopt otherwise.
 *  The keyspans refer to bytes in the passed script. */
std::optional<std::pair<int, std::vector<Span<const unsigned char>>>> MatchMultiA(const CScript& script LIFETIMEBOUND);

/** Generate a multisig script. */
CScript GetScriptForMultisig(int nRequired, const std::vector<CPubKey>& keys);

// REMOVED: TaprootBuilder, TaprootSpendData, and related Taproot/MAST infrastructure
// violates RIGOR "Script as Asset" constitution (no Taproot/BIP341 script path complexity)

#endif // BITCOIN_SCRIPT_STANDARD_H
