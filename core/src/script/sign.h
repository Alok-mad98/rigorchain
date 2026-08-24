// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SCRIPT_SIGN_H
#define BITCOIN_SCRIPT_SIGN_H

#include <attributes.h>
#include <coins.h>
#include <consensus/amount.h>
#include <hash.h>
#include <map>
#include <pubkey.h>
#include <script/interpreter.h>
#include <script/keyorigin.h>
#include <script/standard.h>
#include <uint256.h>

class CKey;
class CKeyID;
class CScript;
class CTransaction;
class SigningProvider;

struct bilingual_str;
struct CMutableTransaction;

/** Interface for signature creators. */
class BaseSignatureCreator {
public:
    virtual ~BaseSignatureCreator() {}
    virtual const BaseSignatureChecker& Checker() const =0;

    /** Create a singular (non-script) signature. */
    virtual bool CreateSig(const SigningProvider& provider, std::vector<unsigned char>& vchSig, const CKeyID& keyid, const CScript& scriptCode, SigVersion sigversion) const =0;
};

/** A signature creator for transactions. */
class MutableTransactionSignatureCreator : public BaseSignatureCreator
{
    const CMutableTransaction& m_txto;
    unsigned int nIn;
    int nHashType;
    CAmount amount;
    const MutableTransactionSignatureChecker checker;
    const PrecomputedTransactionData* m_txdata;

public:
    MutableTransactionSignatureCreator(const CMutableTransaction& tx LIFETIMEBOUND, unsigned int input_idx, const CAmount& amount, int hash_type);
    MutableTransactionSignatureCreator(const CMutableTransaction& tx LIFETIMEBOUND, unsigned int input_idx, const CAmount& amount, const PrecomputedTransactionData* txdata, int hash_type);
    const CMutableTransaction& GetTransaction() const { return m_txto; }
    CAmount GetAmount() const { return amount; }
    const PrecomputedTransactionData* GetTxData() const { return m_txdata; }
    /** Input index this creator signs (for diagnostics). */
    unsigned int GetInputIndex() const { return nIn; }
    int GetSighashType() const { return nHashType; }
    const BaseSignatureChecker& Checker() const override { return checker; }
    bool CreateSig(const SigningProvider& provider, std::vector<unsigned char>& vchSig, const CKeyID& keyid, const CScript& scriptCode, SigVersion sigversion) const override;
};

/** A signature checker that accepts all signatures */
extern const BaseSignatureChecker& DUMMY_CHECKER;
/** Dummy creator for fee estimation: 65-byte Ed25519 (64 + hashtype), not Bitcoin DER. */
extern const BaseSignatureCreator& DUMMY_SIGNATURE_CREATOR;
/** Same 65-byte Ed25519 dummy (Alpha1 has no DER size grind). */
extern const BaseSignatureCreator& DUMMY_MAXIMUM_SIGNATURE_CREATOR;

typedef std::pair<CPubKey, std::vector<unsigned char>> SigPair;

// This struct contains information from a transaction input and also contains signatures for that input.
// The information contained here can be used to create a signature and is also filled by ProduceSignature
// in order to construct final scriptSigs and scriptWitnesses.
struct SignatureData {
    bool complete = false; ///< Stores whether the scriptSig and scriptWitness are complete
    bool witness = false; ///< Stores whether the input this SigData corresponds to is a witness input
    CScript scriptSig; ///< The scriptSig of an input. Contains complete signatures or the traditional partial signatures format
    CScript redeem_script; ///< The redeemScript (if any) for the input
    CScript witness_script; ///< The witnessScript (if any) for the input. witnessScripts are used in P2WSH outputs.
    CScriptWitness scriptWitness; ///< The scriptWitness of an input. Contains complete signatures or the traditional partial signatures format. scriptWitness is part of a transaction input per BIP 144.
    // REMOVED: TaprootSpendData tr_spenddata, TaprootBuilder tr_builder
    // violates RIGOR "Script as Asset" constitution (no Taproot/BIP341 complexity)
    std::map<CKeyID, SigPair> signatures; ///< BIP 174 style partial signatures for the input. May contain all signatures necessary for producing a final scriptSig or scriptWitness.
    std::map<CKeyID, std::pair<CPubKey, KeyOriginInfo>> misc_pubkeys;
    // REMOVED: taproot_key_path_sig, taproot_script_sigs, taproot_misc_pubkeys
    // violates RIGOR "Script as Asset" constitution (no Taproot/BIP341 complexity)
    std::vector<CKeyID> missing_pubkeys; ///< KeyIDs of pubkeys which could not be found
    std::vector<CKeyID> missing_sigs; ///< KeyIDs of pubkeys for signatures which could not be found
    uint160 missing_redeem_script; ///< ScriptID of the missing redeemScript (if any)
    uint256 missing_witness_script; ///< SHA256 of the missing witnessScript (if any)
    std::map<std::vector<uint8_t>, std::vector<uint8_t>> sha256_preimages; ///< Mapping from a SHA256 hash to its preimage provided to solve a Script
    std::map<std::vector<uint8_t>, std::vector<uint8_t>> hash256_preimages; ///< Mapping from a HASH256 hash to its preimage provided to solve a Script
    std::map<std::vector<uint8_t>, std::vector<uint8_t>> ripemd160_preimages; ///< Mapping from a RIPEMD160 hash to its preimage provided to solve a Script
    std::map<std::vector<uint8_t>, std::vector<uint8_t>> hash160_preimages; ///< Mapping from a HASH160 hash to its preimage provided to solve a Script

    SignatureData() {}
    explicit SignatureData(const CScript& script) : scriptSig(script) {}
    void MergeSignatureData(SignatureData sigdata);
};

/** Produce a script signature using a generic signature creator. */
bool ProduceSignature(const SigningProvider& provider, const BaseSignatureCreator& creator, const CScript& scriptPubKey, SignatureData& sigdata);

/**
 * Produce a satisfying script (scriptSig or witness).
 *
 * @param provider   Utility containing the information necessary to solve a script.
 * @param fromPubKey The script to produce a satisfaction for.
 * @param txTo       The spending transaction.
 * @param nIn        The index of the input in `txTo` referring the output being spent.
 * @param amount     The value of the output being spent.
 * @param nHashType  Signature hash type.
 * @param sig_data   Additional data provided to solve a script. Filled with the resulting satisfying
 *                   script and whether the satisfaction is complete.
 *
 * @return           True if the produced script is entirely satisfying `fromPubKey`.
 **/
bool SignSignature(const SigningProvider &provider, const CScript& fromPubKey, CMutableTransaction& txTo,
                   unsigned int nIn, const CAmount& amount, int nHashType, SignatureData& sig_data);
bool SignSignature(const SigningProvider &provider, const CTransaction& txFrom, CMutableTransaction& txTo,
                   unsigned int nIn, int nHashType, SignatureData& sig_data);

/** Extract signature data from a transaction input, and insert it. */
SignatureData DataFromTransaction(const CMutableTransaction& tx, unsigned int nIn, const CTxOut& txout);
void UpdateInput(CTxIn& input, const SignatureData& data);

/** Check whether a scriptPubKey is known to be segwit. */
bool IsSegWitOutput(const SigningProvider& provider, const CScript& script);

/** Sign the CMutableTransaction.
 *  FENCE-W5: no block-level rigor eval context argument. Signing uses UTXO coins
 *  plus the spending transaction only. */
bool SignTransaction(CMutableTransaction& mtx, const SigningProvider* provider, const std::map<COutPoint, Coin>& coins, int sighash, std::map<int, bilingual_str>& input_errors,
                     const std::map<int, SignatureData>* extra_input_sigs = nullptr);

#endif // BITCOIN_SCRIPT_SIGN_H
