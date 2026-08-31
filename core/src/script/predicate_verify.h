// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SCRIPT_PREDICATE_VERIFY_H
#define BITCOIN_SCRIPT_PREDICATE_VERIFY_H

#include <hash.h>
#include <script/script.h>
#include <script/script_error.h>
#include <span.h>
#include <primitives/transaction.h>
#include <uint256.h>

#include <optional>
#include <vector>
#include <stdint.h>

class CPubKey;
class XOnlyPubKey;
class CTransaction;
class CTxOut;
class uint256;


/**
 * RIGOR has one signing rule: the unique Ed25519 commitment (P0-018).
 * These names are RPC/PSBT compatibility aliases only. They do not select a
 * hash, do not appear on the wire, and are not written into the preimage.
 * NONE / SINGLE / ANYONECANPAY do not exist.
 */
static constexpr int SIGHASH_ALL = 1;
static constexpr int SIGHASH_DEFAULT = 0;

/** Script verification flags actually read by VerifyRigorPredicate.
 *
 * Bit positions of the four live flags are frozen (historic Bitcoin numbers)
 * so existing callers that already passed WITNESS / SIGPUSHONLY / CLEANSTACK /
 * NULLFAIL do not need a numeric remap. All other Bitcoin SCRIPT_VERIFY bits
 * have been removed: there is no opcode VM left to configure.
 */
enum : uint32_t {
    SCRIPT_VERIFY_NONE      = 0,

    // Non-push operators in scriptSig fail (mint path).
    SCRIPT_VERIFY_SIGPUSHONLY = (1U << 5),

    // Mint spend must leave exactly one stack element.
    SCRIPT_VERIFY_CLEANSTACK = (1U << 8),

    // Witness programs are evaluated. Without this flag they fail closed
    // (SCRIPT_ERR_RIGOR_SCRIPT_TYPE), never as anyone-can-spend.
    SCRIPT_VERIFY_WITNESS = (1U << 11),

    // Non-empty signature that fails Ed25519 verify is rejected.
    SCRIPT_VERIFY_NULLFAIL = (1U << 14),
};

bool CheckSignatureEncoding(const std::vector<unsigned char> &vchSig, ScriptError* serror);

struct PrecomputedTransactionData
{
    uint256 m_prevouts_single_hash;
    uint256 m_sequences_single_hash;
    uint256 m_outputs_single_hash;
    uint256 hashPrevouts, hashSequence, hashOutputs;
    //! Whether the 3 fields above are initialized.
    bool m_bip143_segwit_ready = false;

    std::vector<CTxOut> m_spent_outputs;
    //! Whether m_spent_outputs is initialized.
    bool m_spent_outputs_ready = false;

    /**
     * FENCE-W5 (TASK-ARCH-SCRIPT-CONTEXT-CLEANUP-001): sighash cache only.
     * Script evaluation is a function of the UTXO being spent and the spending
     * transaction (via BaseSignatureChecker). Do not store chain_id, CBlockIndex*,
     * CBlock*, convergence root, or T here. chain_id is held only by
     * GenericTransactionSignatureChecker (required constructor argument).
     * Mint coupling lives in ConnectEvaluateCoinbaseResMint.
     * There is no SetRigorResEvalContext; do not add one.
     */

    PrecomputedTransactionData() = default;

    /** Initialize this PrecomputedTransactionData with transaction data.
     *
     * @param[in]   tx             The transaction for which data is being precomputed.
     * @param[in]   spent_outputs  The CTxOuts being spent, one for each tx.vin, in order.
     * @param[in]   force          Whether to precompute data for all optional features,
     *                             regardless of what is in the inputs (used at signing
     *                             time, when the inputs aren't filled in yet). */
    template <class T>
    void Init(const T& tx, std::vector<CTxOut>&& spent_outputs, bool force = false);

    template <class T>
    explicit PrecomputedTransactionData(const T& tx);
};

enum class SigVersion
{
    WITNESS_V0 = 1,      //!< ORU P2WPKH (witness v0 / 20). Tag RGRORU01.
    WITNESS_V1_RES = 2,  //!< RES native key-path (witness v1 / 32). Tag RGRRES01.
    RES_MINT = 3,        //!< RIGOR_MARKER_RES_MINT spend. Tag RGRMNT01.
};

/** Signature hash sizes */
static constexpr size_t WITNESS_V0_SCRIPTHASH_SIZE = 32;
static constexpr size_t WITNESS_V0_KEYHASH_SIZE = 20;
static constexpr size_t WITNESS_V1_RES_SIZE = 32;

/**
 * True if \p program is a native RES witness v1 program: 32 bytes, neither
 * CVG2- nor RES2-prefixed. Not Bitcoin Taproot.
 *
 * This is a pure function of the program bytes, and it must stay that way. It is
 * the single source of truth reached by `ClassifyRigorTxout`, and therefore by
 * `AssetTypeFromScriptPubKey`, `GetValueOut(asset)`, `IsAllowedScript`,
 * `IsStandard`, `Solver`, and `AreInputsStandard` — asset class is derived from
 * script bytes and nothing else ("script as asset").
 *
 * TASK-010 R-2: it deliberately does NOT check that the program decodes to a
 * point on the Ed25519 curve. Two reasons. First, classification would then
 * depend on a third-party library's versioned notion of point validity, and
 * because classification applies to existing UTXOs, any future change to that
 * notion would retroactively alter what already-confirmed coins *are*. Second,
 * the check costs a ~63us scalar multiplication in the hottest predicate in the
 * codebase, reachable pre-authentication from `CheckTransaction`.
 *
 * It buys little: a v1 program that is not a curve point can never be satisfied
 * by `crypto_sign_verify_detached`, so it is an unspendable burn output either
 * way. That is a usability hazard, not a consensus one, and it is handled where
 * usability lives — `ed25519::IsCanonicalOnCurvePubkey` gates address
 * encode/decode (`key_io.cpp`), relay policy (`IsStandardTx`), and wallet output
 * construction, so such an output cannot be created by accident.
 */
bool IsRigorResNativeWitnessProgram(const std::vector<unsigned char>& program);

// REMOVED: WITNESS_V1_SCRIPT_PATH constants (BIP341 control block infrastructure)
// violates RIGOR "Script as Asset" constitution (no Taproot/BIP341 script path complexity)
// REMOVED: HASHER_TAPLEAF, HASHER_TAPBRANCH (Taproot hashers)

/** 8-byte ASCII domain tag for this spend path. */
Span<const unsigned char> GetSighashDomainTag(SigVersion sigversion);

/**
 * Unique Ed25519 sighash. `spent_script` is the real scriptPubKey being spent
 * (not a derived P2PKH template). `chain_id` is the genesis hash of this
 * network; SignatureHash never reads chainparams itself.
 */
template <class T>
uint256 SignatureHash(const CScript& spent_script, const T& txTo, unsigned int nIn, const CAmount& amount, SigVersion sigversion, const uint256& chain_id, const PrecomputedTransactionData* cache = nullptr);

class BaseSignatureChecker
{
public:
    virtual bool CheckEd25519Signature(const std::vector<unsigned char>& scriptSig, const std::vector<unsigned char>& vchPubKey, const CScript& scriptCode, SigVersion sigversion) const
    {
        return false;
    }

    /** Connecting / mempool spend height. Used by RIGOR_MARKER_RES_MINT; 0 if the checker has no chain context. */
    virtual int GetSpendHeight() const { return 0; }

    virtual const PrecomputedTransactionData* GetTxData() const { return nullptr; }

    virtual ~BaseSignatureChecker() {}
};

/** Enum to specify what *TransactionSignatureChecker's behavior should be
 *  when dealing with missing transaction data.
 */
enum class MissingDataBehavior
{
    ASSERT_FAIL,  //!< Abort execution through assertion failure (for consensus code)
    FAIL,         //!< Just act as if the signature was invalid
};

template <class T>
class GenericTransactionSignatureChecker : public BaseSignatureChecker
{
private:
    const T* txTo;
    const MissingDataBehavior m_mdb;
    unsigned int nIn;
    const CAmount amount;
    const PrecomputedTransactionData* txdata;
    const int nSpendHeight;
    const uint256 m_chain_id;

protected:
    virtual bool VerifyEd25519Signature(Span<const unsigned char> sig, Span<const unsigned char> pubkey, const uint256& sighash) const;

    const uint256& ChainId() const { return m_chain_id; }

public:
    GenericTransactionSignatureChecker(const T* txToIn, unsigned int nInIn, const CAmount& amountIn, MissingDataBehavior mdb, const uint256& chain_id, int nSpendHeightIn = 0) : txTo(txToIn), m_mdb(mdb), nIn(nInIn), amount(amountIn), txdata(nullptr), nSpendHeight(nSpendHeightIn), m_chain_id(chain_id) {}
    GenericTransactionSignatureChecker(const T* txToIn, unsigned int nInIn, const CAmount& amountIn, const PrecomputedTransactionData& txdataIn, MissingDataBehavior mdb, const uint256& chain_id, int nSpendHeightIn = 0) : txTo(txToIn), m_mdb(mdb), nIn(nInIn), amount(amountIn), txdata(&txdataIn), nSpendHeight(nSpendHeightIn), m_chain_id(chain_id) {}
    bool CheckEd25519Signature(const std::vector<unsigned char>& scriptSig, const std::vector<unsigned char>& vchPubKey, const CScript& scriptCode, SigVersion sigversion) const override;
    int GetSpendHeight() const override { return nSpendHeight; }
    const PrecomputedTransactionData* GetTxData() const override { return txdata; }
};

using TransactionSignatureChecker = GenericTransactionSignatureChecker<CTransaction>;
using MutableTransactionSignatureChecker = GenericTransactionSignatureChecker<CMutableTransaction>;

class DeferringSignatureChecker : public BaseSignatureChecker
{
protected:
    const BaseSignatureChecker& m_checker;

public:
    DeferringSignatureChecker(const BaseSignatureChecker& checker) : m_checker(checker) {}

    bool CheckEd25519Signature(const std::vector<unsigned char>& scriptSig, const std::vector<unsigned char>& vchPubKey, const CScript& scriptCode, SigVersion sigversion) const override
    {
        return m_checker.CheckEd25519Signature(scriptSig, vchPubKey, scriptCode, sigversion);
    }

    int GetSpendHeight() const override { return m_checker.GetSpendHeight(); }

    const PrecomputedTransactionData* GetTxData() const override { return m_checker.GetTxData(); }
};

// REMOVED: ComputeTapleafHash, ComputeTapbranchHash, ComputeWitnessV1PathMerkleRoot
// violates RIGOR "Script as Asset" constitution (no Taproot/BIP341 script path complexity)

// FENCE-W5: VerifyRigorPredicate does not take a block index, block body, convergence root, or T.
// It evaluates this lock against the signature checker (UTXO being spent + spending
// transaction: sighash, nLockTime, nSequence, spend height).
// Mint *authorization* (winner / PCORB) is not a predicate concern; it stays in
// ConnectEvaluateCoinbaseResMint. The RES mint template only reads spend height from the checker.

/** Exact mint template: `<maturity_height> <20-byte HASH160(pubkey)> <0xbb RES mint marker>`. */
bool ParseResMintScript(const CScript& script, int64_t& maturity_height, std::vector<unsigned char>& pubkey_hash);
bool IsResMintScript(const CScript& script);
CScript GetResMintScript(int64_t maturity_height, const uint160& pubkey_hash);
/** Mint lock for a 32-byte Ed25519 pubkey (HASH160(pk32) = HASH160(v1 program) under Option B). */
CScript GetResMintScriptForPubKey(int64_t maturity_height, Span<const unsigned char> pubkey32);

/**
 * Exact convergence burn: `<20-byte HASH160(pk32)> <0xbd CONVERGE marker>`.
 * One encoding only (direct 20-byte push, not PUSHDATA). Unspendable.
 * Under Option B, HASH160(pk32) equals HASH160(v1 program).
 */
bool ParseConvergeScript(const CScript& script, uint160& owner_hash160);
bool IsConvergeScript(const CScript& script);
CScript GetConvergeScript(const uint160& owner_hash160);
CScript GetConvergeScriptForPubKey(Span<const unsigned char> pubkey32);

/** Push-only parser. There is no Script VM, stack machine, or opcode dispatch. */
bool EvalPushOnlyScript(const CScript& script, std::vector<std::vector<unsigned char>>& stack);
/** P0-027 name: push opcodes + length checks only. Same implementation as EvalPushOnlyScript. */
inline bool ParseRigorScriptTemplate(const CScript& script, std::vector<std::vector<unsigned char>>& stack)
{
    return EvalPushOnlyScript(script, stack);
}

/**
 * Consensus spend: ScriptPubKey → fixed predicate → Ed25519. Not an opcode VM.
 * Unknown templates fail closed with SCRIPT_ERR_RIGOR_SCRIPT_TYPE.
 *
 * This is the only spend classifier. Do not add a second enum mirroring the
 * templates matched here: `LookupRigorPredicate` (rigor_predicate_registry.h)
 * names them for the wallet, and a third copy would be free to drift.
 */
bool VerifyRigorPredicate(const CScript& scriptSig, const CScript& scriptPubKey, const CScriptWitness* witness, unsigned int flags, const BaseSignatureChecker& checker, ScriptError* serror = nullptr);

size_t CountWitnessSigOps(const CScript& scriptSig, const CScript& scriptPubKey, const CScriptWitness* witness, unsigned int flags);

#endif // BITCOIN_SCRIPT_PREDICATE_VERIFY_H
