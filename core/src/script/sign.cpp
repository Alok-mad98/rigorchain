// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <script/sign.h>

#include <consensus/amount.h>
#include <crypto/common.h>
#include <crypto/ed25519_provider.h>
#include <hash.h>
#include <key.h>
#include <policy/policy.h>
#include <primitives/transaction.h>
#include <script/keyorigin.h>
#include <script/descriptor.h>
#include <script/script_error.h>
#include <script/signingprovider.h>
#include <script/conv_sign_trace.h>
#include <script/standard.h>
#include <uint256.h>
#include <logging.h>
#include <util/moneystr.h>
#include <util/strencodings.h>
#include <util/translation.h>
#include <util/vector.h>

#include <array>
#include <cstring>
#include <limits>

typedef std::vector<unsigned char> valtype;

static bool LookupResEdSecret(const SigningProvider& provider, const std::vector<unsigned char>& program, std::array<unsigned char, ed25519::SECRETKEY_SIZE>& secret_out)
{
    if (program.size() != WITNESS_V1_RES_SIZE) return false;
    const auto* fp = dynamic_cast<const FlatSigningProvider*>(&provider);
    if (!fp) return false;
    uint256 prog_id;
    std::memcpy(prog_id.begin(), program.data(), 32);
    const auto it = fp->res_v1_program_to_ed_secret.find(prog_id);
    if (it != fp->res_v1_program_to_ed_secret.end()) {
        secret_out = it->second;
        return true;
    }
    for (const auto& [_, key] : fp->keys) {
        const std::vector<unsigned char> pk32 = key.GetPubKey().GetEd25519Payload();
        if (pk32.size() != ed25519::PUBKEY_SIZE) continue;
        uint256 hashed;
        if (!ed25519::ResWitnessProgram(pk32, Span<unsigned char>{hashed.begin(), 32})) continue;
        if (hashed != prog_id) continue;
        std::array<unsigned char, ed25519::PUBKEY_SIZE> pub{};
        if (!ed25519::SeedToKeypair(Span<const unsigned char>{key.begin(), ed25519::SEED_SIZE}, pub, secret_out)) continue;
        return true;
    }
    return false;
}

static std::vector<unsigned char> Ed25519WitnessPubKey(const CPubKey& pubkey)
{
    std::vector<unsigned char> pk32 = pubkey.GetEd25519Payload();
    if (pk32.size() == ed25519::PUBKEY_SIZE) return pk32;
    return {pubkey.begin(), pubkey.end()};
}

MutableTransactionSignatureCreator::MutableTransactionSignatureCreator(const CMutableTransaction& tx, unsigned int input_idx, const CAmount& amount, const uint256& chain_id)
    : m_txto{tx}, nIn{input_idx}, amount{amount}, m_chain_id{chain_id},
      // Signing is not consensus: RIGOR_MARKER_RES_MINT maturity is enforced at CheckInputScripts
      // with the real spend height. FENCE-W5: no chain height on this API.
      checker{&m_txto, nIn, amount, MissingDataBehavior::FAIL, chain_id, std::numeric_limits<int>::max()},
      m_txdata(nullptr)
{
}

MutableTransactionSignatureCreator::MutableTransactionSignatureCreator(const CMutableTransaction& tx, unsigned int input_idx, const CAmount& amount, const PrecomputedTransactionData* txdata, const uint256& chain_id)
    : m_txto{tx}, nIn{input_idx}, amount{amount}, m_chain_id{chain_id},
      checker{txdata ? MutableTransactionSignatureChecker{&m_txto, nIn, amount, *txdata, MissingDataBehavior::FAIL, chain_id, std::numeric_limits<int>::max()} :
                       MutableTransactionSignatureChecker{&m_txto, nIn, amount, MissingDataBehavior::FAIL, chain_id, std::numeric_limits<int>::max()}},
      m_txdata(txdata)
{
}

bool MutableTransactionSignatureCreator::CreateSig(const SigningProvider& provider, std::vector<unsigned char>& vchSig, const CKeyID& address, const CScript& scriptCode, SigVersion sigversion) const
{
    assert(sigversion == SigVersion::WITNESS_V0 || sigversion == SigVersion::WITNESS_V1_RES || sigversion == SigVersion::RES_MINT);

    CKey key;
    if (!provider.GetKey(address, key)) {
        if (ConvSignTraceActive() && ConvSignTxShape(m_txto)) {
            size_t nk = 0;
            if (const auto* fp = dynamic_cast<const FlatSigningProvider*>(&provider)) {
                nk = fp->keys.size();
            }
            LogPrint(BCLog::CONVERGESIGN,
                     "convtrace CreateSig GetKey miss keyid=%s nk=%u sigver=%i\n",
                     address.GetHex(),
                     static_cast<unsigned>(nk),
                     sigversion == SigVersion::WITNESS_V0 ? 0 : (sigversion == SigVersion::WITNESS_V1_RES ? 1 : 2));
        }
        return false;
    }

    // Signing without known amount does not work.
    if (!MoneyRange(amount)) {
        return false;
    }

    uint256 hash = SignatureHash(scriptCode, m_txto, nIn, amount, sigversion, m_chain_id, m_txdata);
    if (!key.Sign(hash, vchSig)) {
        return false;
    }
    // Detached Ed25519 (64 bytes). No hashtype suffix: sighash is unique.
    return vchSig.size() == ed25519::SIGNATURE_SIZE;
}

static bool GetPubKey(const SigningProvider& provider, const SignatureData& sigdata, const CKeyID& address, CPubKey& pubkey)
{
    // Look for pubkey in all partial sigs
    const auto it = sigdata.signatures.find(address);
    if (it != sigdata.signatures.end()) {
        pubkey = it->second.first;
        return true;
    }
    // Look for pubkey in pubkey list
    const auto& pk_it = sigdata.misc_pubkeys.find(address);
    if (pk_it != sigdata.misc_pubkeys.end()) {
        pubkey = pk_it->second.first;
        return true;
    }
    // Query the underlying provider
    return provider.GetPubKey(address, pubkey);
}

static bool CreateSig(const BaseSignatureCreator& creator, SignatureData& sigdata, const SigningProvider& provider, std::vector<unsigned char>& sig_out, const CPubKey& pubkey, const CScript& scriptcode, SigVersion sigversion)
{
    CKeyID keyid = pubkey.GetID();
    const auto it = sigdata.signatures.find(keyid);
    if (it != sigdata.signatures.end()) {
        sig_out = it->second.second;
        return true;
    }
    KeyOriginInfo info;
    if (provider.GetKeyOrigin(keyid, info)) {
        sigdata.misc_pubkeys.emplace(keyid, std::make_pair(pubkey, std::move(info)));
    }
    if (creator.CreateSig(provider, sig_out, keyid, scriptcode, sigversion)) {
        auto i = sigdata.signatures.emplace(keyid, SigPair(pubkey, sig_out));
        assert(i.second);
        return true;
    }
    // Could not make signature or signature not found, add keyid to missing
    sigdata.missing_sigs.push_back(keyid);
    return false;
}

/**
 * Sign scriptPubKey using signature made with creator.
 * Signatures are returned in scriptSigRet (or returns false if scriptPubKey can't be signed).
 * Returns false if scriptPubKey could not be completely satisfied.
 */
static bool SignStep(const SigningProvider& provider, const BaseSignatureCreator& creator, const CScript& scriptPubKey,
                     std::vector<valtype>& ret, TxoutType& whichTypeRet, SigVersion sigversion, SignatureData& sigdata)
{
    ret.clear();
    std::vector<unsigned char> sig;

    std::vector<valtype> vSolutions;
    whichTypeRet = Solver(scriptPubKey, vSolutions);

    switch (whichTypeRet) {
    case TxoutType::NONSTANDARD:
    case TxoutType::NULL_DATA:
    case TxoutType::WITNESS_UNKNOWN:
    case TxoutType::CONVERGE:
        return false;
    case TxoutType::PUBKEY:
    case TxoutType::PUBKEYHASH:
    case TxoutType::SCRIPTHASH:
    case TxoutType::MULTISIG:
        return false;
    case TxoutType::WITNESS_V0_KEYHASH:
    {
        CKeyID keyID = CKeyID(uint160(vSolutions[0]));
        CPubKey pubkey;
        if (!GetPubKey(provider, sigdata, keyID, pubkey)) {
            sigdata.missing_pubkeys.push_back(keyID);
            return false;
        }
        if (!CreateSig(creator, sigdata, provider, sig, pubkey, scriptPubKey, SigVersion::WITNESS_V0)) return false;
        ret.push_back(std::move(sig));
        ret.push_back(Ed25519WitnessPubKey(pubkey));
        return true;
    }

    case TxoutType::WITNESS_V0_SCRIPTHASH:
        return false;

    case TxoutType::WITNESS_V1_RES:
        ret.push_back(vSolutions[0]);
        return true;
    case TxoutType::RES_MINT: {
        const uint160 pkh{vSolutions[0]};
        const bool dummy_fee = &creator.Checker() == &DUMMY_CHECKER;
        if (const auto* fp = dynamic_cast<const FlatSigningProvider*>(&provider)) {
            for (const auto& [prog_id, secret] : fp->res_v1_program_to_ed_secret) {
                std::vector<unsigned char> pk32(secret.begin() + ed25519::SEED_SIZE, secret.begin() + ed25519::SEED_SIZE + ed25519::PUBKEY_SIZE);
                if (Hash160(Span<const unsigned char>(pk32.data(), pk32.size())) != pkh) continue;
                if (dummy_fee) {
                    std::vector<unsigned char> dummy_sig(ed25519::SIGNATURE_SIZE, 0x42);
                    ret.push_back(std::move(dummy_sig));
                    ret.push_back(std::move(pk32));
                    return true;
                }
                const auto* mtc = dynamic_cast<const MutableTransactionSignatureCreator*>(&creator);
                if (!mtc) return false;
                const uint256 sighash = SignatureHash(scriptPubKey, mtc->GetTransaction(), mtc->GetInputIndex(),
                    mtc->GetAmount(), SigVersion::RES_MINT, mtc->GetChainId(), mtc->GetTxData());
                std::vector<unsigned char> mint_sig(ed25519::SIGNATURE_SIZE);
                if (!ed25519::SignDetached(Span<const unsigned char>{sighash.begin(), 32},
                        Span<const unsigned char>{secret.data(), ed25519::SECRETKEY_SIZE},
                        Span<unsigned char>{mint_sig.data(), ed25519::SIGNATURE_SIZE})) {
                    return false;
                }
                ret.push_back(std::move(mint_sig));
                ret.push_back(std::move(pk32));
                return true;
            }
        }
        CPubKey pubkey;
        bool found = GetPubKey(provider, sigdata, CKeyID(pkh), pubkey);
        if (!found) {
            if (const auto* fp = dynamic_cast<const FlatSigningProvider*>(&provider)) {
                auto match_pk = [&](const CPubKey& pk) {
                    if (pk.size() != CPubKey::COMPRESSED_SIZE) return false;
                    if (pk[0] != 0x00 && pk[0] != 0xFF) return false;
                    return Hash160(Span<const unsigned char>(pk.data() + 1, pk.size() - 1)) == pkh;
                };
                for (const auto& [_, pk] : fp->pubkeys) {
                    if (match_pk(pk)) { pubkey = pk; found = true; break; }
                }
                if (!found) {
                    for (const auto& [_, key] : fp->keys) {
                        const CPubKey pk = key.GetPubKey();
                        if (match_pk(pk)) { pubkey = pk; found = true; break; }
                    }
                }
            }
        }
        if (!found) {
            sigdata.missing_pubkeys.push_back(CKeyID(pkh));
            return false;
        }
        std::vector<unsigned char> pk(pubkey.begin(), pubkey.end());
        if (pk.size() == CPubKey::COMPRESSED_SIZE && (pk[0] == 0x00 || pk[0] == 0xFF)) {
            pk.assign(pubkey.begin() + 1, pubkey.end());
        }
        if (pk.size() != ed25519::PUBKEY_SIZE) return false;
        if (!CreateSig(creator, sigdata, provider, sig, pubkey, scriptPubKey, SigVersion::RES_MINT)) return false;
        ret.push_back(std::move(sig));
        ret.push_back(std::move(pk));
        return true;
    }
    } // no default case, so the compiler can warn about missing cases
    assert(false);
    return false; // silence -Wreturn-type (assert may be compiled out)
}

static CScript PushAll(const std::vector<valtype>& values)
{
    CScript result;
    for (const valtype& v : values) {
        if (v.size() == 0) {
            result << RIGOR_PUSH_0;
        } else if (v.size() == 1 && v[0] >= 1 && v[0] <= 16) {
            result << CScript::EncodeOP_N(v[0]);
        } else if (v.size() == 1 && v[0] == 0x81) {
            result << RIGOR_RESERVED_LEGACY_1NEGATE;
        } else {
            result << v;
        }
    }
    return result;
}

bool ProduceSignature(const SigningProvider& provider, const BaseSignatureCreator& creator, const CScript& fromPubKey, SignatureData& sigdata)
{
    if (sigdata.complete) return true;

    std::vector<valtype> result;
    TxoutType whichType;
    bool solved = SignStep(provider, creator, fromPubKey, result, whichType, SigVersion::WITNESS_V0, sigdata);

    if (solved && whichType == TxoutType::WITNESS_V0_KEYHASH)
    {
        sigdata.scriptWitness.stack = result;
        sigdata.witness = true;
        result.clear();
    }
    else if (solved && whichType == TxoutType::WITNESS_V0_SCRIPTHASH)
    {
        solved = false;
    } else if (solved && whichType == TxoutType::WITNESS_V1_RES)
    {
        const valtype& program = result[0];
        // SECURITY-001-A / A-7: only the Ed25519 key path remains. The PCORB script-path witness
        // builder was removed with the consensus branch that accepted it — its signature covered
        // the σ digest instead of the spending transaction, so the transactions it produced could
        // be re-pointed at anyone by anyone.
        if (program.size() != WITNESS_V1_RES_SIZE) {
            solved = false;
        } else {
            const bool dummy_fee = &creator.Checker() == &DUMMY_CHECKER;
            if (dummy_fee) {
                std::vector<unsigned char> witSig(ed25519::SIGNATURE_SIZE, 0x42);
                std::vector<unsigned char> dummy_pk(ed25519::PUBKEY_SIZE, 0x42);
                std::array<unsigned char, ed25519::SECRETKEY_SIZE> ed_sk{};
                if (LookupResEdSecret(provider, program, ed_sk)) {
                    dummy_pk.assign(ed_sk.begin() + ed25519::SEED_SIZE, ed_sk.begin() + ed25519::SEED_SIZE + ed25519::PUBKEY_SIZE);
                }
                sigdata.scriptWitness.stack.clear();
                sigdata.scriptWitness.stack.push_back(std::move(witSig));
                sigdata.scriptWitness.stack.push_back(std::move(dummy_pk));
                sigdata.witness = true;
                result.clear();
            } else {
                const auto* mtc = dynamic_cast<const MutableTransactionSignatureCreator*>(&creator);
                if (!mtc) {
                    solved = false;
                } else {
                    std::array<unsigned char, ed25519::SECRETKEY_SIZE> ed_sk{};
                    if (!LookupResEdSecret(provider, program, ed_sk)) {
                        solved = false;
                    } else {
                        std::vector<unsigned char> ed_pk(ed_sk.begin() + ed25519::SEED_SIZE, ed_sk.begin() + ed25519::SEED_SIZE + ed25519::PUBKEY_SIZE);
                        {
                                const uint256 sighash = SignatureHash(fromPubKey, mtc->GetTransaction(), mtc->GetInputIndex(),
                                    mtc->GetAmount(), SigVersion::WITNESS_V1_RES, mtc->GetChainId(), mtc->GetTxData());
                                std::vector<unsigned char> witSig(ed25519::SIGNATURE_SIZE);
                                if (!ed25519::SignDetached(Span<const unsigned char>{sighash.begin(), 32},
                                        Span<const unsigned char>{ed_sk.data(), ed25519::SECRETKEY_SIZE},
                                        Span<unsigned char>{witSig.data(), ed25519::SIGNATURE_SIZE})) {
                                    solved = false;
                                } else {
                                    sigdata.scriptWitness.stack.clear();
                                    sigdata.scriptWitness.stack.push_back(std::move(witSig));
                                    sigdata.scriptWitness.stack.push_back(std::move(ed_pk));
                                    sigdata.witness = true;
                                    result.clear();
                                }
                            }
                    }
                }
            }
        }
    } else if (solved && whichType == TxoutType::WITNESS_UNKNOWN) {
        sigdata.witness = true;
    }

    if (!sigdata.witness) sigdata.scriptWitness.stack.clear();
    sigdata.scriptSig = PushAll(result);

    // Test solution
    ScriptError serror = SCRIPT_ERR_OK;
    const bool verified = VerifyRigorPredicate(sigdata.scriptSig, fromPubKey, &sigdata.scriptWitness, STANDARD_SCRIPT_VERIFY_FLAGS, creator.Checker(), &serror);
    if (ConvSignTraceActive() && !verified) {
        LogPrint(BCLog::CONVERGESIGN,
                 "convtrace ProduceSignature VerifyRigorPredicate failed: %s solved=%i wit_items=%u\n",
                 ScriptErrorString(serror),
                 solved ? 1 : 0,
                 static_cast<unsigned>(sigdata.scriptWitness.stack.size()));
    }
    sigdata.complete = solved && verified;
    return sigdata.complete;
}

namespace {
class SignatureExtractorChecker final : public DeferringSignatureChecker
{
private:
    SignatureData& sigdata;

public:
    SignatureExtractorChecker(SignatureData& sigdata, BaseSignatureChecker& checker) : DeferringSignatureChecker(checker), sigdata(sigdata) {}

    bool CheckEd25519Signature(const std::vector<unsigned char>& scriptSig, const std::vector<unsigned char>& vchPubKey, const CScript& scriptCode, SigVersion sigversion) const override
    {
        if (m_checker.CheckEd25519Signature(scriptSig, vchPubKey, scriptCode, sigversion)) {
            if (vchPubKey.size() == CPubKey::COMPRESSED_SIZE && vchPubKey[0] == 0x00) {
                CPubKey pubkey(vchPubKey);
                sigdata.signatures.emplace(pubkey.GetID(), SigPair(pubkey, scriptSig));
            }
            return true;
        }
        return false;
    }
};

struct Stacks
{
    std::vector<valtype> script;
    std::vector<valtype> witness;

    Stacks() = delete;
    Stacks(const Stacks&) = delete;
    explicit Stacks(const SignatureData& data) : witness(data.scriptWitness.stack) {
        EvalPushOnlyScript(data.scriptSig, script);
    }
};
}

// Extracts signatures and scripts from incomplete scriptSigs. Please do not extend this, use PSBT instead
SignatureData DataFromTransaction(const CMutableTransaction& tx, unsigned int nIn, const CTxOut& txout, const uint256& chain_id)
{
    SignatureData data;
    assert(tx.vin.size() > nIn);
    data.scriptSig = tx.vin[nIn].scriptSig;
    data.scriptWitness = tx.vin[nIn].scriptWitness;
    Stacks stack(data);

    // Get signatures
    MutableTransactionSignatureChecker tx_checker(&tx, nIn, txout.nValue, MissingDataBehavior::FAIL, chain_id);
    SignatureExtractorChecker extractor_checker(data, tx_checker);
    if (VerifyRigorPredicate(data.scriptSig, txout.scriptPubKey, &data.scriptWitness, STANDARD_SCRIPT_VERIFY_FLAGS, extractor_checker)) {
        data.complete = true;
        return data;
    }

    // Get scripts
    std::vector<std::vector<unsigned char>> solutions;
    TxoutType script_type = Solver(txout.scriptPubKey, solutions);
    SigVersion sigversion = SigVersion::WITNESS_V0;
    CScript next_script = txout.scriptPubKey;

    if (script_type == TxoutType::SCRIPTHASH && !stack.script.empty() && !stack.script.back().empty()) {
        // Get the redeemScript
        CScript redeem_script(stack.script.back().begin(), stack.script.back().end());
        data.redeem_script = redeem_script;
        next_script = std::move(redeem_script);

        // Get redeemScript type
        script_type = Solver(next_script, solutions);
        stack.script.pop_back();
    }
    if (script_type == TxoutType::WITNESS_V0_SCRIPTHASH && !stack.witness.empty() && !stack.witness.back().empty()) {
        // Get the witnessScript
        CScript witness_script(stack.witness.back().begin(), stack.witness.back().end());
        data.witness_script = witness_script;
        next_script = std::move(witness_script);

        // Get witnessScript type
        script_type = Solver(next_script, solutions);
        stack.witness.pop_back();
        stack.script = std::move(stack.witness);
        stack.witness.clear();
        sigversion = SigVersion::WITNESS_V0;
    }
    if (script_type == TxoutType::MULTISIG && !stack.script.empty()) {
        // Build a map of pubkey -> signature by matching sigs to pubkeys:
        assert(solutions.size() > 1);
        unsigned int num_pubkeys = solutions.size()-2;
        unsigned int last_success_key = 0;
        for (const valtype& sig : stack.script) {
            for (unsigned int i = last_success_key; i < num_pubkeys; ++i) {
                const valtype& pubkey = solutions[i+1];
                // We either have a signature for this pubkey, or we have found a signature and it is valid
                if (data.signatures.count(CPubKey(pubkey).GetID()) || extractor_checker.CheckEd25519Signature(sig, pubkey, next_script, sigversion)) {
                    last_success_key = i + 1;
                    break;
                }
            }
        }
    }

    return data;
}

void UpdateInput(CTxIn& input, const SignatureData& data)
{
    input.scriptSig = data.scriptSig;
    input.scriptWitness = data.scriptWitness;
}

void SignatureData::MergeSignatureData(SignatureData sigdata)
{
    if (complete) return;
    if (sigdata.complete) {
        *this = std::move(sigdata);
        return;
    }
    if (redeem_script.empty() && !sigdata.redeem_script.empty()) {
        redeem_script = sigdata.redeem_script;
    }
    if (witness_script.empty() && !sigdata.witness_script.empty()) {
        witness_script = sigdata.witness_script;
    }
    signatures.insert(std::make_move_iterator(sigdata.signatures.begin()), std::make_move_iterator(sigdata.signatures.end()));
}

bool SignSignature(const SigningProvider &provider, const CScript& fromPubKey, CMutableTransaction& txTo, unsigned int nIn, const CAmount& amount, const uint256& chain_id, SignatureData& sig_data)
{
    assert(nIn < txTo.vin.size());

    MutableTransactionSignatureCreator creator(txTo, nIn, amount, chain_id);

    bool ret = ProduceSignature(provider, creator, fromPubKey, sig_data);
    UpdateInput(txTo.vin.at(nIn), sig_data);
    return ret;
}

bool SignSignature(const SigningProvider &provider, const CTransaction& txFrom, CMutableTransaction& txTo, unsigned int nIn, const uint256& chain_id, SignatureData& sig_data)
{
    assert(nIn < txTo.vin.size());
    const CTxIn& txin = txTo.vin[nIn];
    assert(txin.prevout.n < txFrom.vout.size());
    const CTxOut& txout = txFrom.vout[txin.prevout.n];

    return SignSignature(provider, txout.scriptPubKey, txTo, nIn, txout.nValue, chain_id, sig_data);
}

namespace {
/** Dummy signature checker which accepts all signatures. */
class DummySignatureChecker final : public BaseSignatureChecker
{
public:
    DummySignatureChecker() = default;
    bool CheckEd25519Signature(const std::vector<unsigned char>& vchSigIn, const std::vector<unsigned char>& vchPubKey, const CScript& scriptCode, SigVersion sigversion) const override
    {
        return vchSigIn.size() == ed25519::SIGNATURE_SIZE && vchPubKey.size() == ed25519::PUBKEY_SIZE;
    }
    int GetSpendHeight() const override { return std::numeric_limits<int>::max(); }
};
}

const BaseSignatureChecker& DUMMY_CHECKER = DummySignatureChecker();

namespace {
class DummySignatureCreator final : public BaseSignatureCreator {
private:
    char m_r_len = 32;
    char m_s_len = 32;
public:
    DummySignatureCreator(char r_len, char s_len) : m_r_len(r_len), m_s_len(s_len) {}
    const BaseSignatureChecker& Checker() const override { return DUMMY_CHECKER; }
    bool CreateSig(const SigningProvider& provider, std::vector<unsigned char>& vchSig, const CKeyID& keyid, const CScript& scriptCode, SigVersion sigversion) const override
    {
        (void)provider;
        (void)keyid;
        (void)scriptCode;
        (void)sigversion;
        (void)m_r_len;
        (void)m_s_len;
        // Alpha1 Ed25519-only: DummySignatureChecker::CheckEd25519Signature requires
        // a 64-byte detached signature. No hashtype suffix.
        vchSig.assign(ed25519::SIGNATURE_SIZE, 0x42);
        return true;
    }
};

}

const BaseSignatureCreator& DUMMY_SIGNATURE_CREATOR = DummySignatureCreator(32, 32);
const BaseSignatureCreator& DUMMY_MAXIMUM_SIGNATURE_CREATOR = DummySignatureCreator(33, 32);

namespace {
class MetadataOnlySignatureCreator final : public BaseSignatureCreator {
    // BaseSignatureChecker rejects every signature by default, which is what we want: a
    // metadata-only pass must neither produce nor accept one.
    const BaseSignatureChecker m_checker{};

public:
    const BaseSignatureChecker& Checker() const override { return m_checker; }
    bool CreateSig(const SigningProvider& provider, std::vector<unsigned char>& vchSig, const CKeyID& keyid, const CScript& scriptCode, SigVersion sigversion) const override
    {
        return false;
    }
};
}

const BaseSignatureCreator& METADATA_ONLY_SIGNATURE_CREATOR = MetadataOnlySignatureCreator();

bool IsSegWitOutput(const SigningProvider& provider, const CScript& script)
{
    (void)provider;
    int version;
    valtype program;
    return script.IsWitnessProgram(version, program);
}

bool SignTransaction(CMutableTransaction& mtx, const SigningProvider* keystore, const std::map<COutPoint, Coin>& coins, const uint256& chain_id, std::map<int, bilingual_str>& input_errors,
                     const std::map<int, SignatureData>* extra_input_sigs)
{
    // Use CTransaction for the constant parts of the
    // transaction to avoid rehashing.
    const CTransaction txConst(mtx);

    PrecomputedTransactionData txdata;
    std::vector<CTxOut> spent_outputs;
    for (unsigned int i = 0; i < mtx.vin.size(); ++i) {
        CTxIn& txin = mtx.vin[i];
        auto coin = coins.find(txin.prevout);
        if (coin == coins.end() || coin->second.IsSpent()) {
            txdata.Init(txConst, /*spent_outputs=*/{}, /*force=*/true);
            break;
        } else {
            // Use the exact UTXO from the coins view (nValue + scriptPubKey); asset is implied by script template.
            spent_outputs.push_back(coin->second.out);
        }
    }
    if (spent_outputs.size() == mtx.vin.size()) {
        txdata.Init(txConst, std::move(spent_outputs), true);
    }

    // Sign what we can:
    for (unsigned int i = 0; i < mtx.vin.size(); ++i) {
        CTxIn& txin = mtx.vin[i];
        auto coin = coins.find(txin.prevout);
        if (coin == coins.end() || coin->second.IsSpent()) {
            input_errors[i] = _("Input not found or already spent");
            continue;
        }
        const CScript& prevPubKey = coin->second.out.scriptPubKey;
        const CAmount& amount = coin->second.out.nValue;

        SignatureData sigdata = DataFromTransaction(mtx, i, coin->second.out, chain_id);
        if (extra_input_sigs) {
            const auto itx = extra_input_sigs->find(static_cast<int>(i));
            if (itx != extra_input_sigs->end()) {
                sigdata.MergeSignatureData(itx->second);
            }
        }
        if (ConvSignTraceActive() && ConvSignTxShape(mtx)) {
            int witver = 0;
            std::vector<unsigned char> witprog;
            const bool is_v0_keyhash = prevPubKey.IsWitnessProgram(witver, witprog) && witver == 0 && witprog.size() == 20;
            const uint256 sighash = SignatureHash(prevPubKey, txConst, i, amount, SigVersion::WITNESS_V0, chain_id, &txdata);
            size_t flat_keys = 0;
            if (keystore) {
                if (const auto* fp = dynamic_cast<const FlatSigningProvider*>(keystore)) {
                    flat_keys = fp->keys.size();
                }
            }
            LogPrint(BCLog::CONVERGESIGN,
                     "convtrace nIn=%u v0_keyhash=%i amount=%s scriptPubKey=%s sighash=%s chain_id=%s flat_keys=%u\n",
                     i,
                     is_v0_keyhash ? 1 : 0,
                     FormatMoney(amount),
                     HexStr(prevPubKey),
                     sighash.ToString(),
                     chain_id.ToString(),
                     static_cast<unsigned>(flat_keys));
        }
        ProduceSignature(*keystore, MutableTransactionSignatureCreator(mtx, i, amount, &txdata, chain_id), prevPubKey, sigdata);

        UpdateInput(txin, sigdata);

        // amount must be specified for valid segwit signature
        if (amount == MAX_MONEY && !txin.scriptWitness.IsNull()) {
            input_errors[i] = _("Missing amount");
            continue;
        }

        // Only re-verify when ProduceSignature did not already mark the input complete.
        // ProduceSignature ends with VerifyRigorPredicate using the same creator checker; an extra
        // TransactionSignatureChecker(&txConst, ...) pass can disagree with CMutableTransaction
        // signing in edge cases (and must not override a successful sign).
        ScriptError serror = SCRIPT_ERR_OK;
        MutableTransactionSignatureChecker mtx_checker(&mtx, i, amount, txdata, MissingDataBehavior::FAIL, chain_id);
        if (!sigdata.complete && !VerifyRigorPredicate(txin.scriptSig, prevPubKey, &txin.scriptWitness, STANDARD_SCRIPT_VERIFY_FLAGS, mtx_checker, &serror)) {
            if (serror == SCRIPT_ERR_INVALID_STACK_OPERATION) {
                // Unable to sign input and verification failed (possible attempt to partially sign).
                input_errors[i] = Untranslated("Unable to sign input, invalid stack size (possibly missing key)");
            } else if (serror == SCRIPT_ERR_SIG_NULLFAIL) {
                // Verification failed (possibly due to insufficient signatures).
                input_errors[i] = Untranslated("CHECK(MULTI)SIG failing with non-zero signature (possibly need more signatures)");
            } else {
                input_errors[i] = Untranslated(ScriptErrorString(serror));
            }
        } else {
            // If this input succeeds, make sure there is no error set for it
            input_errors.erase(i);
        }
    }
    return input_errors.empty();
}
