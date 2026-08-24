// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SCRIPT_INTERPRETER_H
#define BITCOIN_SCRIPT_INTERPRETER_H

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


/** Signature hash types/flags */
enum
{
    SIGHASH_ALL = 1,
    SIGHASH_NONE = 2,
    SIGHASH_SINGLE = 3,
    SIGHASH_ANYONECANPAY = 0x80,

    SIGHASH_DEFAULT = 0, //!< Reserved; treated as SIGHASH_ALL where used
    SIGHASH_OUTPUT_MASK = 3,
    SIGHASH_INPUT_MASK = 0x80,
};

/** Script verification flags.
 *
 *  All flags are intended to be soft forks: the set of acceptable scripts under
 *  flags (A | B) is a subset of the acceptable scripts under flag (A).
 */
enum : uint32_t {
    SCRIPT_VERIFY_NONE      = 0,

    // Evaluate P2SH subscripts (BIP16).
    SCRIPT_VERIFY_P2SH      = (1U << 0),

    // Passing a non-strict-DER signature or one with undefined hashtype to a checksig operation causes script failure.
    // Evaluating a pubkey that is not (0x04 + 64 bytes) or (0x02 or 0x03 + 32 bytes) by checksig causes script failure.
    // (not used or intended as a consensus rule).
    SCRIPT_VERIFY_STRICTENC = (1U << 1),

    // Passing a non-strict-DER signature to a checksig operation causes script failure (BIP62 rule 1)
    SCRIPT_VERIFY_DERSIG    = (1U << 2),

    // Passing a non-strict-DER signature or one with S > order/2 to a checksig operation causes script failure
    // (BIP62 rule 5).
    SCRIPT_VERIFY_LOW_S     = (1U << 3),

    // verify dummy stack item consumed by CHECKMULTISIG is of zero-length (BIP62 rule 7).
    SCRIPT_VERIFY_NULLDUMMY = (1U << 4),

    // Using a non-push operator in the scriptSig causes script failure (BIP62 rule 2).
    SCRIPT_VERIFY_SIGPUSHONLY = (1U << 5),

    // Require minimal encodings for all push operations (OP_0... OP_16, OP_1NEGATE where possible, direct
    // pushes up to 75 bytes, OP_PUSHDATA up to 255 bytes, OP_PUSHDATA2 for anything larger). Evaluating
    // any other push causes the script to fail (BIP62 rule 3).
    // In addition, whenever a stack element is interpreted as a number, it must be of minimal length (BIP62 rule 4).
    SCRIPT_VERIFY_MINIMALDATA = (1U << 6),

    // Discourage use of NOPs reserved for upgrades (NOP1-10)
    //
    // Provided so that nodes can avoid accepting or mining transactions
    // containing executed NOP's whose meaning may change after a soft-fork,
    // thus rendering the script invalid; with this flag set executing
    // discouraged NOPs fails the script. This verification flag will never be
    // a mandatory flag applied to scripts in a block. NOPs that are not
    // executed, e.g.  within an unexecuted IF ENDIF block, are *not* rejected.
    // NOPs that have associated forks to give them new meaning (CLTV, CSV)
    // are not subject to this rule.
    SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_NOPS  = (1U << 7),

    // Require that only a single stack element remains after evaluation. This changes the success criterion from
    // "At least one stack element must remain, and when interpreted as a boolean, it must be true" to
    // "Exactly one stack element must remain, and when interpreted as a boolean, it must be true".
    // (BIP62 rule 6)
    // Note: CLEANSTACK should never be used without P2SH or WITNESS.
    // Note: WITNESS_V0 script execution has behavior similar to CLEANSTACK as part of its
    //       consensus rules. It is automatic there and does not need this flag.
    SCRIPT_VERIFY_CLEANSTACK = (1U << 8),

    // Verify CHECKLOCKTIMEVERIFY
    //
    // See BIP65 for details.
    SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY = (1U << 9),

    // support CHECKSEQUENCEVERIFY opcode
    //
    // See BIP112 for details
    SCRIPT_VERIFY_CHECKSEQUENCEVERIFY = (1U << 10),

    // Support segregated witness
    //
    SCRIPT_VERIFY_WITNESS = (1U << 11),

    // Making v1-v16 witness program non-standard
    //
    SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_WITNESS_PROGRAM = (1U << 12),

    // Segwit script only: Require the argument of OP_IF/NOTIF to be exactly 0x01 or empty vector
    //
    // Note: some witness script rules mirror MINIMALIF-style constraints
    //       rules. It is automatic there and does not depend on this flag.
    SCRIPT_VERIFY_MINIMALIF = (1U << 13),

    // Signature(s) must be empty vector if a CHECK(MULTI)SIG operation failed
    //
    SCRIPT_VERIFY_NULLFAIL = (1U << 14),

    // Public keys in segregated witness scripts must be compressed
    //
    SCRIPT_VERIFY_WITNESS_PUBKEYTYPE = (1U << 15),

    // Making OP_CODESEPARATOR and FindAndDelete fail any non-segwit scripts
    //
    SCRIPT_VERIFY_CONST_SCRIPTCODE = (1U << 16),

    /** Bit positions 17–18 reserved (historical); not used for consensus on this chain. */
    SCRIPT_VERIFY_RESERVED_WITNESS_FLAG17 = (1U << 17),
    SCRIPT_VERIFY_RESERVED_WITNESS_FLAG18 = (1U << 18),

    // Making unknown OP_SUCCESS non-standard
    SCRIPT_VERIFY_DISCOURAGE_OP_SUCCESS = (1U << 19),

    // Making unknown public key versions in certain witness scripts non-standard
    SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_PUBKEYTYPE = (1U << 20),

    // Constants to point to the highest flag in use. Add new flags above this line.
    //
    SCRIPT_VERIFY_END_MARKER
};

bool CheckSignatureEncoding(const std::vector<unsigned char> &vchSig, unsigned int flags, ScriptError* serror);

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
     * transaction (via BaseSignatureChecker). Do not store CBlockIndex*, CBlock*,
     * convergence root, or T here. Mint coupling lives in ConnectEvaluateCoinbaseResMint.
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
    BASE = 0,            //!< Bare scripts (OP_RES_MINT eval); sighash for mint is BIP143 (WITNESS_V0)
    WITNESS_V0 = 1,      //!< Witness v0 (P2WPKH and P2WSH); see BIP 141
    WITNESS_V1_RES = 2,  //!< Witness v1 RES key-path; BIP143 plus a domain tag
};

struct ScriptExecutionData
{
    // REMOVED: m_tapleaf_hash_init, m_tapleaf_hash (BIP341 taproot script path)
    // violates RIGOR "Script as Asset" constitution (no Taproot/BIP341 complexity)

    //! Whether m_codeseparator_pos is initialized.
    bool m_codeseparator_pos_init = false;
    //! Opcode position of the last executed OP_CODESEPARATOR (or 0xFFFFFFFF if none executed).
    uint32_t m_codeseparator_pos;

    // REMOVED: m_annex_init, m_annex_present, m_annex_hash (BIP341 annex)
    // violates RIGOR "Script as Asset" constitution (no Taproot/BIP341 complexity)

    //! Whether m_validation_weight_left is initialized.
    bool m_validation_weight_left_init = false;
    //! How much validation weight is left (decremented for every successful non-empty signature check).
    int64_t m_validation_weight_left;

    //! The hash of the corresponding output
    std::optional<uint256> m_output_hash;
};

/** Signature hash sizes */
static constexpr size_t WITNESS_V0_SCRIPTHASH_SIZE = 32;
static constexpr size_t WITNESS_V0_KEYHASH_SIZE = 20;
static constexpr size_t WITNESS_V1_RES_SIZE = 32;
/** Historical 36-byte v1 program size (not an IsAllowedScript chain output; 32-byte v1 only). */
static constexpr size_t WITNESS_V1_RES2_CLAIM_PROGRAM_SIZE = 36;

/** True if \p program is a witness v1 RES2-prefixed native RES commitment (32-byte; not CVG2). Not BIP341. */
bool IsRes2AssetProgram(const std::vector<unsigned char>& program);
/** True if \p program is native RES witness v1 (32-byte; neither CVG2 nor RES2-prefixed). Not Bitcoin Taproot. */
bool IsRigorResNativeWitnessProgram(const std::vector<unsigned char>& program);

// REMOVED: WITNESS_V1_SCRIPT_PATH constants (BIP341 control block infrastructure)
// violates RIGOR "Script as Asset" constitution (no Taproot/BIP341 script path complexity)
// REMOVED: HASHER_TAPLEAF, HASHER_TAPBRANCH (Taproot hashers)

template <class T>
uint256 SignatureHash(const CScript& scriptCode, const T& txTo, unsigned int nIn, int nHashType, const CAmount& amount, SigVersion sigversion, const PrecomputedTransactionData* cache = nullptr);

class BaseSignatureChecker
{
public:
    // Not used by EvalChecksig: spend signatures are Ed25519 only. Kept for dummy/PSBT checkers.
    virtual bool CheckECDSASignature(const std::vector<unsigned char>& scriptSig, const std::vector<unsigned char>& vchPubKey, const CScript& scriptCode, SigVersion sigversion) const
    {
        return false;
    }

    virtual bool CheckEd25519Signature(const std::vector<unsigned char>& scriptSig, const std::vector<unsigned char>& vchPubKey, const CScript& scriptCode, SigVersion sigversion) const
    {
        return false;
    }

    virtual bool CheckLockTime(const CScriptNum& nLockTime) const
    {
         return false;
    }

    virtual bool CheckSequence(const CScriptNum& nSequence) const
    {
        return false;
    }

    /** Connecting / mempool spend height. Used by OP_RES_MINT; 0 if the checker has no chain context. */
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

protected:
    virtual bool VerifyECDSASignature(const std::vector<unsigned char>& vchSig, const CPubKey& vchPubKey, const uint256& sighash) const;
    virtual bool VerifyEd25519Signature(Span<const unsigned char> sig, Span<const unsigned char> pubkey, const uint256& sighash) const;

public:
    GenericTransactionSignatureChecker(const T* txToIn, unsigned int nInIn, const CAmount& amountIn, MissingDataBehavior mdb, int nSpendHeightIn = 0) : txTo(txToIn), m_mdb(mdb), nIn(nInIn), amount(amountIn), txdata(nullptr), nSpendHeight(nSpendHeightIn) {}
    GenericTransactionSignatureChecker(const T* txToIn, unsigned int nInIn, const CAmount& amountIn, const PrecomputedTransactionData& txdataIn, MissingDataBehavior mdb, int nSpendHeightIn = 0) : txTo(txToIn), m_mdb(mdb), nIn(nInIn), amount(amountIn), txdata(&txdataIn), nSpendHeight(nSpendHeightIn) {}
    bool CheckECDSASignature(const std::vector<unsigned char>& scriptSig, const std::vector<unsigned char>& vchPubKey, const CScript& scriptCode, SigVersion sigversion) const override;
    bool CheckEd25519Signature(const std::vector<unsigned char>& scriptSig, const std::vector<unsigned char>& vchPubKey, const CScript& scriptCode, SigVersion sigversion) const override;
    bool CheckLockTime(const CScriptNum& nLockTime) const override;
    bool CheckSequence(const CScriptNum& nSequence) const override;
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

    bool CheckECDSASignature(const std::vector<unsigned char>& scriptSig, const std::vector<unsigned char>& vchPubKey, const CScript& scriptCode, SigVersion sigversion) const override
    {
        return m_checker.CheckECDSASignature(scriptSig, vchPubKey, scriptCode, sigversion);
    }

    bool CheckEd25519Signature(const std::vector<unsigned char>& scriptSig, const std::vector<unsigned char>& vchPubKey, const CScript& scriptCode, SigVersion sigversion) const override
    {
        return m_checker.CheckEd25519Signature(scriptSig, vchPubKey, scriptCode, sigversion);
    }

    bool CheckLockTime(const CScriptNum& nLockTime) const override
    {
        return m_checker.CheckLockTime(nLockTime);
    }
    bool CheckSequence(const CScriptNum& nSequence) const override
    {
        return m_checker.CheckSequence(nSequence);
    }

    int GetSpendHeight() const override { return m_checker.GetSpendHeight(); }

    const PrecomputedTransactionData* GetTxData() const override { return m_checker.GetTxData(); }
};

// REMOVED: ComputeTapleafHash, ComputeTapbranchHash, ComputeWitnessV1PathMerkleRoot
// violates RIGOR "Script as Asset" constitution (no Taproot/BIP341 script path complexity)

// FENCE-W5: EvalScript does not take a block index, block body, convergence root, or T.
// It evaluates this script against the stack, flags, and the signature checker
// (UTXO being spent + spending transaction: sighash, nLockTime, nSequence, spend height).
// Mint *authorization* (winner / PCORB) is not an interpreter concern; it stays in
// ConnectEvaluateCoinbaseResMint. OP_RES_MINT only reads spend height from the checker.

/** Exact mint template: `<maturity_height> <20-byte HASH160(pubkey)> OP_RES_MINT`. */
bool ParseResMintScript(const CScript& script, int64_t& maturity_height, std::vector<unsigned char>& pubkey_hash);
bool IsResMintScript(const CScript& script);
CScript GetResMintScript(int64_t maturity_height, const uint160& pubkey_hash);
/** Mint locking script for a 32-byte Ed25519 program (HASH160 of the program). */
CScript GetResMintScriptForProgram(int64_t maturity_height, Span<const unsigned char> program32);

/**
 * Exact convergence burn: `<20-byte HASH160(RES program)> OP_CONVERGE`.
 * One encoding only (OP_PUSHBYTES_20, not PUSHDATA). Unspendable; EvalScript fails closed.
 */
bool ParseConvergeScript(const CScript& script, uint160& owner_hash160);
bool IsConvergeScript(const CScript& script);
CScript GetConvergeScript(const uint160& owner_hash160);
CScript GetConvergeScriptForProgram(Span<const unsigned char> program32);

bool EvalScript(std::vector<std::vector<unsigned char> >& stack, const CScript& script, unsigned int flags, const BaseSignatureChecker& checker, SigVersion sigversion, ScriptExecutionData& execdata, ScriptError* error = nullptr);
bool EvalScript(std::vector<std::vector<unsigned char> >& stack, const CScript& script, unsigned int flags, const BaseSignatureChecker& checker, SigVersion sigversion, ScriptError* error = nullptr);
bool VerifyScript(const CScript& scriptSig, const CScript& scriptPubKey, const CScriptWitness* witness, unsigned int flags, const BaseSignatureChecker& checker, ScriptError* serror = nullptr);

size_t CountWitnessSigOps(const CScript& scriptSig, const CScript& scriptPubKey, const CScriptWitness* witness, unsigned int flags);

int FindAndDelete(CScript& script, const CScript& b);

#endif // BITCOIN_SCRIPT_INTERPRETER_H
