// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <script/predicate_verify.h>
#include <script/fixed_ed25519_checker.h>

#include <script/rigor_tags.h>
#include <consensus/amount.h>
#include <consensus/txkernel/transaction_kernel_input.h>
#include <crypto/common.h>
#include <crypto/ripemd160.h>
#include <crypto/ed25519_provider.h>
#include <crypto/sha256.h>
#include <hash.h>
#include <script/script.h>
#include <span.h>
#include <uint256.h>

#include <algorithm>
#include <cstring>
#include <limits>

typedef std::vector<unsigned char> valtype;

CScript GetResMintScriptForPubKey(int64_t maturity_height, Span<const unsigned char> pubkey32)
{
    // HASH160(pk32). Under Option B this equals HASH160(v1 program).
    return GetResMintScript(maturity_height, Hash160(pubkey32));
}

CScript GetConvergeScriptForPubKey(Span<const unsigned char> pubkey32)
{
    // HASH160(pk32), matching RIGOR_MARKER_RES_MINT. Under Option B this equals HASH160(v1 program).
    return GetConvergeScript(Hash160(pubkey32));
}

namespace {

inline bool set_success(ScriptError* ret)
{
    if (ret)
        *ret = SCRIPT_ERR_OK;
    return true;
}

inline bool set_error(ScriptError* ret, const ScriptError serror)
{
    if (ret)
        *ret = serror;
    return false;
}

} // namespace

bool CastToBool(const valtype& vch)
{
    for (unsigned int i = 0; i < vch.size(); i++)
    {
        if (vch[i] != 0)
        {
            // Can be negative zero
            if (i == vch.size()-1 && vch[i] == 0x80)
                return false;
            return true;
        }
    }
    return false;
}

bool static IsValidEd25519SignatureEncoding(const valtype& sig)
{
    // Consensus signatures are a raw 64-byte Ed25519 detached signature.
    // There is no hashtype byte: the sighash commits to the whole transaction.
    return sig.size() == ed25519::SIGNATURE_SIZE;
}

bool CheckSignatureEncoding(const std::vector<unsigned char> &vchSig, ScriptError* serror) {
    if (vchSig.empty()) {
        return true;
    }
    if (!IsValidEd25519SignatureEncoding(vchSig)) {
        return set_error(serror, SCRIPT_ERR_SIG_DER);
    }
    return true;
}

namespace {

/** Compute the (single) SHA256 of the concatenation of all prevouts of a tx. */
template <class T>
uint256 GetPrevoutsSHA256(const T& txTo)
{
    HashWriter ss{};
    for (const auto& txin : txTo.vin) {
        ss << txin.prevout;
    }
    return ss.GetSHA256();
}

/** Compute the (single) SHA256 of the concatenation of all nSequences of a tx. */
template <class T>
uint256 GetSequencesSHA256(const T& txTo)
{
    HashWriter ss{};
    for (const auto& txin : txTo.vin) {
        ss << txin.nSequence;
    }
    return ss.GetSHA256();
}

/** Compute the (single) SHA256 of the concatenation of all txouts of a tx. */
template <class T>
uint256 GetOutputsSHA256(const T& txTo)
{
    HashWriter ss{};
    for (const auto& txout : txTo.vout) {
        ss << txout;
    }
    return ss.GetSHA256();
}

} // namespace

template <class T>
void PrecomputedTransactionData::Init(const T& txTo, std::vector<CTxOut>&& spent_outputs, bool force)
{
    (void)force;
    assert(!m_spent_outputs_ready);

    m_spent_outputs = std::move(spent_outputs);
    if (!m_spent_outputs.empty()) {
        assert(m_spent_outputs.size() == txTo.vin.size());
        m_spent_outputs_ready = true;
    }

    // Unique sighash always commits to prevouts, sequences, and outputs.
    m_prevouts_single_hash = GetPrevoutsSHA256(txTo);
    m_sequences_single_hash = GetSequencesSHA256(txTo);
    m_outputs_single_hash = GetOutputsSHA256(txTo);
    hashPrevouts = SHA256Uint256(m_prevouts_single_hash);
    hashSequence = SHA256Uint256(m_sequences_single_hash);
    hashOutputs = SHA256Uint256(m_outputs_single_hash);
    m_bip143_segwit_ready = true;
}

template <class T>
PrecomputedTransactionData::PrecomputedTransactionData(const T& txTo)
{
    Init(txTo, {});
}

// explicit instantiation
template void PrecomputedTransactionData::Init(const CTransaction& txTo, std::vector<CTxOut>&& spent_outputs, bool force);
template void PrecomputedTransactionData::Init(const CMutableTransaction& txTo, std::vector<CTxOut>&& spent_outputs, bool force);
template PrecomputedTransactionData::PrecomputedTransactionData(const CTransaction& txTo);
template PrecomputedTransactionData::PrecomputedTransactionData(const CMutableTransaction& txTo);

// REMOVED: HASHER_TAPLEAF, HASHER_TAPBRANCH (Taproot hashers)
// violates RIGOR "Script as Asset" constitution (no Taproot/BIP341 script path complexity)

static bool HandleMissingData(MissingDataBehavior mdb)
{
    switch (mdb) {
    case MissingDataBehavior::ASSERT_FAIL:
        assert(!"Missing data");
        break;
    case MissingDataBehavior::FAIL:
        return false;
    }
    assert(!"Unknown MissingDataBehavior value");
}

Span<const unsigned char> GetSighashDomainTag(SigVersion sigversion)
{
    switch (sigversion) {
    case SigVersion::WITNESS_V0:
        return Span{RigorTags::SighashOruV1.data(), RigorTags::SighashOruV1.size()};
    case SigVersion::WITNESS_V1_RES:
        return Span{RigorTags::SighashResV1.data(), RigorTags::SighashResV1.size()};
    case SigVersion::RES_MINT:
        return Span{RigorTags::SighashMintV1.data(), RigorTags::SighashMintV1.size()};
    }
    assert(false);
    return Span{RigorTags::SighashOruV1.data(), 0};
}

template <class T>
uint256 SignatureHash(const CScript& spent_script, const T& txTo, unsigned int nIn, const CAmount& amount, SigVersion sigversion, const uint256& chain_id, const PrecomputedTransactionData* cache)
{
    assert(nIn < txTo.vin.size());

    // Unique commitment: tagged domain + chain id + whole transaction.
    // spent_script is the real scriptPubKey. No hashtype field.
    HashWriter ss{};
    const bool cacheready = cache && cache->m_bip143_segwit_ready;
    const uint256 hashPrevouts = cacheready ? cache->hashPrevouts : SHA256Uint256(GetPrevoutsSHA256(txTo));
    const uint256 hashSequence = cacheready ? cache->hashSequence : SHA256Uint256(GetSequencesSHA256(txTo));
    const uint256 hashOutputs = cacheready ? cache->hashOutputs : SHA256Uint256(GetOutputsSHA256(txTo));

    ss.write(AsBytes(GetSighashDomainTag(sigversion)));
    ss << chain_id;
    ss << txTo.nVersion;
    ss << hashPrevouts;
    ss << hashSequence;
    ss << hashOutputs;
    ss << txTo.vin[nIn].prevout;
    ss << spent_script;
    ss << amount;
    ss << txTo.vin[nIn].nSequence;
    ss << txTo.nLockTime;
    return ss.GetHash();
}

template uint256 SignatureHash<CTransaction>(const CScript&, const CTransaction&, unsigned int, const CAmount&, SigVersion, const uint256&, const PrecomputedTransactionData*);
template uint256 SignatureHash<CMutableTransaction>(const CScript&, const CMutableTransaction&, unsigned int, const CAmount&, SigVersion, const uint256&, const PrecomputedTransactionData*);

template <class T>
bool GenericTransactionSignatureChecker<T>::VerifyEd25519Signature(Span<const unsigned char> sig, Span<const unsigned char> pubkey, const uint256& sighash) const
{
    return ed25519::VerifyDetached(sig, Span{sighash}, pubkey);
}

template <class T>
bool GenericTransactionSignatureChecker<T>::CheckEd25519Signature(const std::vector<unsigned char>& vchSigIn, const std::vector<unsigned char>& vchPubKey, const CScript& scriptCode, SigVersion sigversion) const
{
    if (vchPubKey.size() != ed25519::PUBKEY_SIZE) return false;
    if (vchSigIn.size() != ed25519::SIGNATURE_SIZE) return false;

    if (amount < 0) return HandleMissingData(m_mdb);

    const uint256 sighash = SignatureHash(scriptCode, *txTo, nIn, amount, sigversion, ChainId(), this->txdata);
    return VerifyEd25519Signature(vchSigIn, vchPubKey, sighash);
}

template <class T>
bool GenericTransactionSignatureChecker<T>::GetKernelSighashTxView(KernelSighashTxView& out) const
{
    if (!txTo || nIn >= txTo->vin.size()) {
        return false;
    }
    const bool cacheready = txdata && txdata->m_bip143_segwit_ready;
    out.chain_id = m_chain_id;
    out.nVersion = txTo->nVersion;
    out.hashPrevouts = cacheready ? txdata->hashPrevouts : SHA256Uint256(GetPrevoutsSHA256(*txTo));
    out.hashSequence = cacheready ? txdata->hashSequence : SHA256Uint256(GetSequencesSHA256(*txTo));
    out.hashOutputs = cacheready ? txdata->hashOutputs : SHA256Uint256(GetOutputsSHA256(*txTo));
    out.prevout_hash = txTo->vin[nIn].prevout.hash;
    out.prevout_n = txTo->vin[nIn].prevout.n;
    out.amount = amount;
    out.nSequence = txTo->vin[nIn].nSequence;
    out.nLockTime = txTo->nLockTime;
    return true;
}

// explicit instantiation
template class GenericTransactionSignatureChecker<CTransaction>;
template class GenericTransactionSignatureChecker<CMutableTransaction>;

static bool WitnessStackItemTooLarge(const Span<const valtype>& stack)
{
    for (const valtype& elem : stack) {
        if (elem.size() > MAX_SCRIPT_ELEMENT_SIZE) return true;
    }
    return false;
}

/** Structured P2WPKH: witness is `<64-byte sig> <32-byte pk>`; no Script VM. */
static bool VerifyP2WPKHSpend(const Span<const valtype>& stack, const CScript& scriptPubKey,
                              const std::vector<unsigned char>& program,
                              unsigned int flags, const BaseSignatureChecker& checker, ScriptError* serror)
{
    if (stack.size() != 2) {
        return set_error(serror, SCRIPT_ERR_WITNESS_PROGRAM_MISMATCH);
    }
    if (WitnessStackItemTooLarge(stack)) {
        return set_error(serror, SCRIPT_ERR_PUSH_SIZE);
    }
    const valtype& vchSig = stack[0];
    const valtype& vchPubKey = stack[1];
    const uint160 expect_hash = Hash160(vchPubKey);
    if (expect_hash.size() != program.size() ||
        !std::equal(expect_hash.begin(), expect_hash.end(), program.begin())) {
        return set_error(serror, SCRIPT_ERR_EQUALVERIFY);
    }
    if (vchPubKey.size() != ed25519::PUBKEY_SIZE) {
        return set_error(serror, SCRIPT_ERR_PUBKEYTYPE);
    }
    if (!IsValidEd25519SignatureEncoding(vchSig)) {
        return set_error(serror, SCRIPT_ERR_SIG_DER);
    }
    if (!checker.CheckEd25519Signature(vchSig, vchPubKey, scriptPubKey, SigVersion::WITNESS_V0)) {
        if ((flags & SCRIPT_VERIFY_NULLFAIL) && !vchSig.empty()) {
            return set_error(serror, SCRIPT_ERR_SIG_NULLFAIL);
        }
        return set_error(serror, SCRIPT_ERR_EVAL_FALSE);
    }
    return set_success(serror);
}

static bool VerifyWitnessProgram(const CScriptWitness& witness, int witversion, const std::vector<unsigned char>& program, unsigned int flags, const BaseSignatureChecker& checker, ScriptError* serror, const CScript& scriptPubKey)
{
    Span stack{witness.stack};

    if (witversion == 0) {
        if (program.size() == WITNESS_V0_KEYHASH_SIZE) {
            return VerifyP2WPKHSpend(stack, scriptPubKey, program, flags, checker, serror);
        }
        // v0/32 P2WSH is not a consensus spend path.
        return set_error(serror, SCRIPT_ERR_WITNESS_PROGRAM_WRONG_LENGTH);
    } else if (witversion == 1 && program.size() == WITNESS_V1_RES_SIZE && IsRigorResNativeWitnessProgram(program)) {
        if (stack.size() == 0) {
            return set_error(serror, SCRIPT_ERR_WITNESS_PROGRAM_WITNESS_EMPTY);
        }
        if (WitnessStackItemTooLarge(stack)) {
            return set_error(serror, SCRIPT_ERR_PUSH_SIZE);
        }

        // Key path: <64-byte Ed25519 signature><pubkey>; program is the pubkey (Option B).
        if (stack.size() == 2) {
            const valtype& vchSig = stack[0];
            const valtype& vchPubKey = stack[1];
            if (vchPubKey.size() != ed25519::PUBKEY_SIZE) {
                return set_error(serror, SCRIPT_ERR_WITNESS_PROGRAM_MISMATCH);
            }
            if (std::memcmp(vchPubKey.data(), program.data(), WITNESS_V1_RES_SIZE) != 0) {
                return set_error(serror, SCRIPT_ERR_WITNESS_PROGRAM_MISMATCH);
            }
            if (!IsValidEd25519SignatureEncoding(vchSig)) {
                return set_error(serror, SCRIPT_ERR_SIG_DER);
            }
            if (!checker.CheckEd25519Signature(vchSig, vchPubKey, scriptPubKey, SigVersion::WITNESS_V1_RES)) {
                return set_error(serror, SCRIPT_ERR_EVAL_FALSE);
            }
            return set_success(serror);
        }

        // SECURITY-001-A / A-7: a witness v1 RES output has exactly one spending path, the
        // Ed25519 key path above, whose signature commits to the spending transaction through the
        // sighash.
        //
        // A second, 11-item "PCORB script path" used to be accepted here. Its only authorization
        // was VerifyConvergeProof over the σ digest, and σ is a function of the convergence being
        // proved and the block context — never of the spending transaction. A claim broadcast on
        // that path was therefore unauthenticated with respect to its own outputs: anyone could
        // copy the eleven witness items verbatim into their own transaction spending the same RES
        // output to themselves, satisfy every check identically, and outbid the owner on fee.
        // Since RES transfers already work over the key path, the path was removed rather than
        // repaired; a second way to spend the same output earns its keep only if it is at least as
        // safe as the first.
        //
        // FENCE-W5: do not reintroduce block-level rigor context (CBlockIndex, T, C, parent)
        // into this function. Mint coupling remains ConnectEvaluateCoinbaseResMint only.
        // M-1: the RES2-prefixed spend branch was deleted; RES2 programs are not creatable.
        return set_error(serror, SCRIPT_ERR_WITNESS_PROGRAM_MISMATCH);
    } else {
        // Unknown witness version or program length: consensus failure when this path runs
        // (includes WITNESS_UNKNOWN output types). Not deferrable to relay policy.
        return set_error(serror, SCRIPT_ERR_DISCOURAGE_UPGRADABLE_WITNESS_PROGRAM);
    }
    // Should never reach here - all paths should return
    return set_error(serror, SCRIPT_ERR_UNKNOWN_ERROR);
}

bool EvalPushOnlyScript(const CScript& script, std::vector<std::vector<unsigned char>>& stack)
{
    CScript::const_iterator pc = script.begin();
    while (pc < script.end()) {
        opcodetype opcode;
        valtype data;
        if (!script.GetOp(pc, opcode, data)) return false;
        if (opcode == RIGOR_PUSH_0) {
            stack.emplace_back();
        } else if (opcode <= RIGOR_PUSHDATA4) {
            stack.push_back(std::move(data));
        } else if (opcode >= RIGOR_PUSH_1 && opcode <= RIGOR_PUSH_16) {
            stack.emplace_back(1, static_cast<unsigned char>(CScript::DecodeOP_N(opcode)));
        } else {
            return false;
        }
    }
    return true;
}

/** Structured RES mint spend: scriptSig is `<sig> <pk>` (plus optional prefix items); no Script VM. */
static bool VerifyResMintSpend(const CScript& scriptSig, const CScript& scriptPubKey,
                               const CScriptWitness& witness, unsigned int flags,
                               const BaseSignatureChecker& checker, ScriptError* serror)
{
    int64_t maturity_height = 0;
    valtype pubkey_hash;
    if (!ParseResMintScript(scriptPubKey, maturity_height, pubkey_hash) ||
        pubkey_hash.size() != WITNESS_V0_KEYHASH_SIZE) {
        return set_error(serror, SCRIPT_ERR_RIGOR_SCRIPT_TYPE);
    }
    if (maturity_height < 0) {
        return set_error(serror, SCRIPT_ERR_NEGATIVE_LOCKTIME);
    }
    if (static_cast<int64_t>(checker.GetSpendHeight()) < maturity_height) {
        return set_error(serror, SCRIPT_ERR_UNSATISFIED_LOCKTIME);
    }

    std::vector<valtype> stack;
    if (!EvalPushOnlyScript(scriptSig, stack)) {
        return set_error(serror, SCRIPT_ERR_BAD_OPCODE);
    }
    if (stack.size() < 2) {
        return set_error(serror, SCRIPT_ERR_INVALID_STACK_OPERATION);
    }
    const valtype vchPubKey = stack.back();
    stack.pop_back();
    const valtype vchSig = stack.back();
    stack.pop_back();
    if (vchPubKey.size() != ed25519::PUBKEY_SIZE) {
        return set_error(serror, SCRIPT_ERR_PUBKEYTYPE);
    }
    const uint160 expect_hash = Hash160(vchPubKey);
    if (!std::equal(expect_hash.begin(), expect_hash.end(), pubkey_hash.begin())) {
        return set_error(serror, SCRIPT_ERR_EQUALVERIFY);
    }
    if (!IsValidEd25519SignatureEncoding(vchSig)) {
        return set_error(serror, SCRIPT_ERR_SIG_DER);
    }
    if (!checker.CheckEd25519Signature(vchSig, vchPubKey, scriptPubKey, SigVersion::RES_MINT)) {
        if ((flags & SCRIPT_VERIFY_NULLFAIL) && !vchSig.empty()) {
            return set_error(serror, SCRIPT_ERR_SIG_NULLFAIL);
        }
        return set_error(serror, SCRIPT_ERR_EVAL_FALSE);
    }
    static const valtype vchTrue(1, 1);
    stack.push_back(vchTrue);
    if ((flags & SCRIPT_VERIFY_CLEANSTACK) != 0 && stack.size() != 1) {
        return set_error(serror, SCRIPT_ERR_CLEANSTACK);
    }
    if ((flags & SCRIPT_VERIFY_WITNESS) != 0 && !witness.IsNull()) {
        return set_error(serror, SCRIPT_ERR_WITNESS_UNEXPECTED);
    }
    return set_success(serror);
}

/** Pre-A1 predicate. ScriptError classification and diagnostic oracle.
 *  Not the production accept/reject authority. */
bool ClassifyRigorPredicateError(const CScript& scriptSig, const CScript& scriptPubKey, const CScriptWitness* witness, unsigned int flags, const BaseSignatureChecker& checker, ScriptError* serror)
{
    static const CScriptWitness emptyWitness;
    if (witness == nullptr) {
        witness = &emptyWitness;
    }

    set_error(serror, SCRIPT_ERR_UNKNOWN_ERROR);

    if ((flags & SCRIPT_VERIFY_SIGPUSHONLY) != 0 && !scriptSig.IsPushOnly()) {
        return set_error(serror, SCRIPT_ERR_SIG_PUSHONLY);
    }

    if (IsResMintScript(scriptPubKey)) {
        if (!scriptSig.IsPushOnly()) {
            return set_error(serror, SCRIPT_ERR_SIG_PUSHONLY);
        }
        return VerifyResMintSpend(scriptSig, scriptPubKey, *witness, flags, checker, serror);
    }

    int witnessversion;
    std::vector<unsigned char> witnessprogram;
    if (!scriptPubKey.IsWitnessProgram(witnessversion, witnessprogram)) {
        return set_error(serror, SCRIPT_ERR_RIGOR_SCRIPT_TYPE);
    }
    if (witnessversion != 0 && witnessversion != 1) {
        return set_error(serror, SCRIPT_ERR_RIGOR_SCRIPT_TYPE);
    }
    if (witnessversion == 1 && witnessprogram.size() != 32) {
        return set_error(serror, SCRIPT_ERR_RIGOR_SCRIPT_TYPE);
    }

    // F-03: a missing SCRIPT_VERIFY_WITNESS flag must never evaluate a witness
    // program as anyone-can-spend.
    if ((flags & SCRIPT_VERIFY_WITNESS) == 0) {
        return set_error(serror, SCRIPT_ERR_RIGOR_SCRIPT_TYPE);
    }
    if (scriptSig.size() != 0) {
        return set_error(serror, SCRIPT_ERR_WITNESS_MALLEATED);
    }
    if (!VerifyWitnessProgram(*witness, witnessversion, witnessprogram, flags, checker, serror, scriptPubKey)) {
        return false;
    }
    return set_success(serror);
}

/** Fill ScriptError for a spend the P058 boundary already rejected.
 *  If the diagnostic would have accepted, the acceptance sets diverged —
 *  fail closed. The diagnostic boolean is never the production verdict. */
static bool VerifyRigorPredicateClassifyError(const CScript& scriptSig, const CScript& scriptPubKey, const CScriptWitness& witness, unsigned int flags, const BaseSignatureChecker& checker, ScriptError* serror)
{
    if (ClassifyRigorPredicateError(scriptSig, scriptPubKey, &witness, flags, checker, serror)) {
        return set_error(serror, SCRIPT_ERR_RIGOR_CLASSIFIER_DIVERGENCE);
    }
    return false;
}

bool VerifyRigorPredicate(const CScript& scriptSig, const CScript& scriptPubKey, const CScriptWitness* witness, unsigned int flags, const BaseSignatureChecker& checker, ScriptError* serror)
{
    g_rigor_fixed_ed25519_tuple_mismatch = 0;
    static const CScriptWitness emptyWitness;
    if (witness == nullptr) {
        witness = &emptyWitness;
    }

    const TransactionKernelInput in = MakeKernelInput(scriptSig, scriptPubKey, *witness, flags, checker);
    if (VerifyRigorPredicateBoundary(in)) {
        return set_success(serror);
    }
    const FixedEd25519Checker fixed(checker, in.crypto_accept);
    return VerifyRigorPredicateClassifyError(scriptSig, scriptPubKey, *witness, flags, fixed, serror);
}

size_t static WitnessSigOps(int witversion, const std::vector<unsigned char>& witprogram, const CScriptWitness& witness)
{
    if (witversion == 0) {
        if (witprogram.size() == WITNESS_V0_KEYHASH_SIZE)
            return 1;
    }
    if (witversion == 1 && witprogram.size() == WITNESS_V1_RES_SIZE && IsRigorResNativeWitnessProgram(witprogram)) {
        return 1;
    }

    return 0;
}

size_t CountWitnessSigOps(const CScript& scriptSig, const CScript& scriptPubKey, const CScriptWitness* witness, unsigned int flags)
{
    (void)scriptSig;
    static const CScriptWitness witnessEmpty;

    if ((flags & SCRIPT_VERIFY_WITNESS) == 0) {
        return 0;
    }

    int witnessversion;
    std::vector<unsigned char> witnessprogram;
    if (scriptPubKey.IsWitnessProgram(witnessversion, witnessprogram)) {
        return WitnessSigOps(witnessversion, witnessprogram, witness ? *witness : witnessEmpty);
    }

    return 0;
}
