// Copyright (c) 2019-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <crypto/ed25519_provider.h>
#include <hash.h>
#include <key_io.h>
#include <logging.h>
#include <outputtype.h>
#include <pubkey.h>
#include <random.h>
#include <script/rigor_tags.h>
#include <script/descriptor.h>
#include <script/conv_sign_trace.h>
#include <script/predicate_verify.h>
#include <script/rigor_predicate_registry.h>
#include <script/sign.h>
#include <script/standard.h>
#include <util/bip32.h>
#include <util/strencodings.h>
#include <util/string.h>
#include <util/time.h>
#include <util/translation.h>
#include <wallet/scriptpubkeyman.h>
#include <wallet/wallet_hd_v1.h>

#include <array>
#include <optional>

namespace wallet {
//! Value for the first BIP 32 hardened derivation. Can be used as a bit mask and as a value. See BIP 32 for more details.
const uint32_t BIP32_HARDENED_KEY_LIMIT = 0x80000000;

/** True for a non-ranged res() (single hex key). HD res() with a hardened range is ranged. */
static bool IsSingleSlotNativeResDescriptor(const Descriptor& d)
{
    const std::optional<OutputType> ot = d.GetOutputType();
    return ot && *ot == OutputType::BECH32M && !d.IsRange();
}

util::Result<CTxDestination> LegacyScriptPubKeyMan::GetNewDestination(const OutputType type)
{
    if (LEGACY_OUTPUT_TYPES.count(type) == 0) {
        return util::Error{_("Error: Legacy wallets on Rigor support only \"bech32\" addresses; use a descriptor wallet for \"bech32m\" (v1 RES, not Taproot).")};
    }

    // Fill-up keypool if needed
    TopUp();

    LOCK(cs_KeyStore);

    // Generate a new key that is added to wallet
    CPubKey new_key;
    if (!GetKeyFromPool(new_key, type)) {
        return util::Error{_("Error: Keypool ran out, please call keypoolrefill first")};
    }
    LearnRelatedScripts(new_key, type);
    return GetDestinationForKey(new_key, type);
}

typedef std::vector<unsigned char> valtype;

namespace {

/**
 * This is an internal representation of isminetype + invalidity.
 * Its order is significant, as we return the max of all explored
 * possibilities.
 */
enum class IsMineResult
{
    NO = 0,         //!< Not ours
    WATCH_ONLY = 1, //!< Included in watch-only balance
    SPENDABLE = 2,  //!< Included in all balances
    INVALID = 3,    //!< Not spendable by anyone (uncompressed pubkey in segwit)
};

//! Solve a top-level scriptPubKey and return spendable/watchonly/invalid status.
//! Ownership is RIGOR predicate id + HaveKey(program). There is no redeemScript /
//! witnessScript recursion and no PUBKEY / P2PKH / MULTISIG / SCRIPT_HASH path.
IsMineResult IsMineInner(const LegacyScriptPubKeyMan& keystore, const CScript& scriptPubKey)
{
    IsMineResult ret = IsMineResult::NO;

    std::vector<valtype> vSolutions;
    const std::optional<RigorPredicateID> pred = LookupRigorPredicate(scriptPubKey);
    if (pred) {
        (void)Solver(scriptPubKey, vSolutions);
        switch (*pred) {
        case RigorPredicateID::ORU_V0_KEYHASH:
        {
            if (!keystore.HaveCScript(CScriptID(CScript() << RIGOR_PUSH_0 << vSolutions[0]))) {
                break;
            }
            const CKeyID keyID{uint160(vSolutions[0])};
            {
                CPubKey pubkey;
                if (keystore.GetPubKey(keyID, pubkey) && !pubkey.IsCompressed()) {
                    return IsMineResult::INVALID;
                }
            }
            if (keystore.HaveKey(keyID)) {
                ret = std::max(ret, IsMineResult::SPENDABLE);
            }
            break;
        }
        case RigorPredicateID::RES_V1_NATIVE_KEY:
        {
            if (!keystore.HaveCScript(CScriptID(CScript() << RIGOR_PUSH_1 << vSolutions[0]))) {
                break;
            }
            const XOnlyPubKey xpk{Span<const unsigned char>(vSolutions[0].data(), vSolutions[0].size())};
            for (const CKeyID& keyid : xpk.GetKeyIDs()) {
                if (keystore.HaveKey(keyid)) {
                    ret = std::max(ret, IsMineResult::SPENDABLE);
                    break;
                }
            }
            break;
        }
        case RigorPredicateID::RES_MINT_V0:
        case RigorPredicateID::CONVERGE_V0:
        case RigorPredicateID::OP_RETURN_BURN:
            break;
        }
    }

    if (ret == IsMineResult::NO && keystore.HaveWatchOnly(scriptPubKey) &&
        IsRigorConstructibleScript(scriptPubKey) && EntersUtxoSet(scriptPubKey)) {
        ret = std::max(ret, IsMineResult::WATCH_ONLY);
    }
    return ret;
}

} // namespace

isminetype LegacyScriptPubKeyMan::IsMine(const CScript& script) const
{
    switch (IsMineInner(*this, script)) {
    case IsMineResult::INVALID:
    case IsMineResult::NO:
        return ISMINE_NO;
    case IsMineResult::WATCH_ONLY:
        return ISMINE_WATCH_ONLY;
    case IsMineResult::SPENDABLE:
        return ISMINE_SPENDABLE;
    }
    assert(false);
}

bool LegacyScriptPubKeyMan::CheckDecryptionKey(const CKeyingMaterial& master_key, bool accept_no_keys)
{
    {
        LOCK(cs_KeyStore);
        assert(mapKeys.empty());

        bool keyPass = mapCryptedKeys.empty(); // Always pass when there are no encrypted keys
        bool keyFail = false;
        CryptedKeyMap::const_iterator mi = mapCryptedKeys.begin();
        WalletBatch batch(m_storage.GetDatabase());
        for (; mi != mapCryptedKeys.end(); ++mi)
        {
            const CPubKey &vchPubKey = (*mi).second.first;
            const std::vector<unsigned char> &vchCryptedSecret = (*mi).second.second;
            CKey key;
            if (!DecryptKey(master_key, vchCryptedSecret, vchPubKey, key))
            {
                keyFail = true;
                break;
            }
            keyPass = true;
            if (fDecryptionThoroughlyChecked)
                break;
            else {
                // Rewrite these encrypted keys with checksums
                batch.WriteCryptedKey(vchPubKey, vchCryptedSecret, mapKeyMetadata[vchPubKey.GetID()]);
            }
        }
        if (keyPass && keyFail)
        {
            LogPrintf("The wallet is probably corrupted: Some keys decrypt but not all.\n");
            throw std::runtime_error("Error unlocking wallet: some keys decrypt but not all. Your wallet file may be corrupt.");
        }
        if (keyFail || (!keyPass && !accept_no_keys))
            return false;
        fDecryptionThoroughlyChecked = true;
    }
    return true;
}

bool LegacyScriptPubKeyMan::Encrypt(const CKeyingMaterial& master_key, WalletBatch* batch)
{
    LOCK(cs_KeyStore);
    encrypted_batch = batch;
    if (!mapCryptedKeys.empty()) {
        encrypted_batch = nullptr;
        return false;
    }

    KeyMap keys_to_encrypt;
    keys_to_encrypt.swap(mapKeys); // Clear mapKeys so AddCryptedKeyInner will succeed.
    for (const KeyMap::value_type& mKey : keys_to_encrypt)
    {
        const CKey &key = mKey.second;
        CPubKey vchPubKey = key.GetPubKey();
        CKeyingMaterial vchSecret(key.begin(), key.end());
        std::vector<unsigned char> vchCryptedSecret;
        if (!EncryptSecret(master_key, vchSecret, vchPubKey.GetHash(), vchCryptedSecret)) {
            encrypted_batch = nullptr;
            return false;
        }
        if (!AddCryptedKey(vchPubKey, vchCryptedSecret)) {
            encrypted_batch = nullptr;
            return false;
        }
    }
    encrypted_batch = nullptr;
    return true;
}

util::Result<CTxDestination> LegacyScriptPubKeyMan::GetReservedDestination(const OutputType type, bool internal, int64_t& index, CKeyPool& keypool)
{
    if (LEGACY_OUTPUT_TYPES.count(type) == 0) {
        return util::Error{_("Error: Legacy wallets on Rigor support only \"bech32\" addresses; use a descriptor wallet for \"bech32m\" (v1 RES, not Taproot).")};
    }

    LOCK(cs_KeyStore);
    if (!CanGetAddresses(internal)) {
        return util::Error{_("Error: Keypool ran out, please call keypoolrefill first")};
    }

    // Fill-up keypool if needed
    TopUp();

    if (!ReserveKeyFromKeyPool(index, keypool, internal)) {
        return util::Error{_("Error: Keypool ran out, please call keypoolrefill first")};
    }
    return GetDestinationForKey(keypool.vchPubKey, type);
}

bool LegacyScriptPubKeyMan::TopUpInactiveHDChain(const CKeyID seed_id, int64_t index, bool internal)
{
    LOCK(cs_KeyStore);

    auto it = m_inactive_hd_chains.find(seed_id);
    if (it == m_inactive_hd_chains.end()) {
        return false;
    }

    CHDChain& chain = it->second;

    if (internal) {
        chain.m_next_internal_index = std::max(chain.m_next_internal_index, index + 1);
    } else {
        chain.m_next_external_index = std::max(chain.m_next_external_index, index + 1);
    }

    TopUpChain(chain, 0);

    return true;
}

std::vector<WalletDestination> LegacyScriptPubKeyMan::MarkUnusedAddresses(const CScript& script, WalletBatch* batch)
{
    (void)batch;
    LOCK(cs_KeyStore);
    std::vector<WalletDestination> result;
    // extract addresses and check if they match with an unused keypool key
    for (const auto& keyid : GetAffectedKeys(script, *this)) {
        std::map<CKeyID, int64_t>::const_iterator mi = m_pool_key_to_index.find(keyid);
        if (mi != m_pool_key_to_index.end()) {
            WalletLogPrintf("%s: Detected a used keypool key, mark all keypool keys up to this key as used\n", __func__);
            for (const auto& keypool : MarkReserveKeysAsUsed(mi->second)) {
                // derive all possible destinations as any of them could have been used
                for (const auto& type : LEGACY_OUTPUT_TYPES) {
                    const auto& dest = GetDestinationForKey(keypool.vchPubKey, type);
                    result.push_back({dest, keypool.fInternal});
                }
            }

            if (!TopUp()) {
                WalletLogPrintf("%s: Topping up keypool failed (locked wallet)\n", __func__);
            }
        }

        // Find the key's metadata and check if it's seed id (if it has one) is inactive, i.e. it is not the current m_hd_chain seed id.
        // If so, TopUp the inactive hd chain
        auto it = mapKeyMetadata.find(keyid);
        if (it != mapKeyMetadata.end()){
            CKeyMetadata meta = it->second;
            if (!meta.hd_seed_id.IsNull() && meta.hd_seed_id != m_hd_chain.seed_id) {
                std::vector<uint32_t> path;
                if (meta.has_key_origin) {
                    path = meta.key_origin.path;
                } else if (!ParseHDKeypath(meta.hdKeypath, path)) {
                    WalletLogPrintf("%s: Adding inactive seed keys failed, invalid hdKeypath: %s\n",
                                    __func__,
                                    meta.hdKeypath);
                }
                if (path.size() != 3) {
                    WalletLogPrintf("%s: Adding inactive seed keys failed, invalid path size: %d, has_key_origin: %s\n",
                                    __func__,
                                    path.size(),
                                    meta.has_key_origin);
                } else {
                    bool internal = (path[1] & ~BIP32_HARDENED_KEY_LIMIT) != 0;
                    int64_t index = path[2] & ~BIP32_HARDENED_KEY_LIMIT;

                    if (!TopUpInactiveHDChain(meta.hd_seed_id, index, internal)) {
                        WalletLogPrintf("%s: Adding inactive seed keys failed\n", __func__);
                    }
                }
            }
        }
    }

    return result;
}

void LegacyScriptPubKeyMan::UpgradeKeyMetadata()
{
    LOCK(cs_KeyStore);
    if (m_storage.IsLocked() || m_storage.IsWalletFlagSet(WALLET_FLAG_KEY_ORIGIN_METADATA)) {
        return;
    }

    std::unique_ptr<WalletBatch> batch = std::make_unique<WalletBatch>(m_storage.GetDatabase());
    for (auto& meta_pair : mapKeyMetadata) {
        CKeyMetadata& meta = meta_pair.second;
        if (!meta.hd_seed_id.IsNull() && !meta.has_key_origin && meta.hdKeypath != "s") { // If the hdKeypath is "s", that's the seed and it doesn't have a key origin
            CKey key;
            GetKey(meta.hd_seed_id, key);
            CExtKey masterKey;
            masterKey.SetSeed(key);
            // Add to map
            CKeyID master_id = masterKey.key.GetPubKey().GetID();
            std::copy(master_id.begin(), master_id.begin() + 4, meta.key_origin.fingerprint);
            if (!ParseHDKeypath(meta.hdKeypath, meta.key_origin.path)) {
                throw std::runtime_error("Invalid stored hdKeypath");
            }
            meta.has_key_origin = true;
            if (meta.nVersion < CKeyMetadata::VERSION_WITH_KEY_ORIGIN) {
                meta.nVersion = CKeyMetadata::VERSION_WITH_KEY_ORIGIN;
            }

            // Write meta to wallet
            CPubKey pubkey;
            if (GetPubKey(meta_pair.first, pubkey)) {
                batch->WriteKeyMetadata(meta, pubkey, true);
            }
        }
    }
}

bool LegacyScriptPubKeyMan::SetupGeneration(bool force)
{
    if ((CanGenerateKeys() && !force) || m_storage.IsLocked()) {
        return false;
    }

    SetHDSeed(GenerateNewSeed());
    if (!NewKeyPool()) {
        return false;
    }
    return true;
}

bool LegacyScriptPubKeyMan::IsHDEnabled() const
{
    return !m_hd_chain.seed_id.IsNull();
}

bool LegacyScriptPubKeyMan::CanGetAddresses(bool internal) const
{
    LOCK(cs_KeyStore);
    // Check if the keypool has keys
    bool keypool_has_keys;
    if (internal && m_storage.CanSupportFeature(FEATURE_HD_SPLIT)) {
        keypool_has_keys = setInternalKeyPool.size() > 0;
    } else {
        keypool_has_keys = KeypoolCountExternalKeys() > 0;
    }
    // If the keypool doesn't have keys, check if we can generate them
    if (!keypool_has_keys) {
        return CanGenerateKeys();
    }
    return keypool_has_keys;
}

bool LegacyScriptPubKeyMan::Upgrade(int prev_version, int new_version, bilingual_str& error)
{
    LOCK(cs_KeyStore);

    if (m_storage.IsWalletFlagSet(WALLET_FLAG_DISABLE_PRIVATE_KEYS)) {
        // Nothing to do here if private keys are not enabled
        return true;
    }

    bool hd_upgrade = false;
    bool split_upgrade = false;
    if (IsFeatureSupported(new_version, FEATURE_HD) && !IsHDEnabled()) {
        WalletLogPrintf("Upgrading wallet to HD\n");
        m_storage.SetMinVersion(FEATURE_HD);

        // generate a new master key
        CPubKey masterPubKey = GenerateNewSeed();
        SetHDSeed(masterPubKey);
        hd_upgrade = true;
    }
    // Upgrade to HD chain split if necessary
    if (!IsFeatureSupported(prev_version, FEATURE_HD_SPLIT) && IsFeatureSupported(new_version, FEATURE_HD_SPLIT)) {
        WalletLogPrintf("Upgrading wallet to use HD chain split\n");
        m_storage.SetMinVersion(FEATURE_PRE_SPLIT_KEYPOOL);
        split_upgrade = FEATURE_HD_SPLIT > prev_version;
        // Upgrade the HDChain
        if (m_hd_chain.nVersion < CHDChain::VERSION_HD_CHAIN_SPLIT) {
            m_hd_chain.nVersion = CHDChain::VERSION_HD_CHAIN_SPLIT;
            if (!WalletBatch(m_storage.GetDatabase()).WriteHDChain(m_hd_chain)) {
                throw std::runtime_error(std::string(__func__) + ": writing chain failed");
            }
        }
    }
    // Mark all keys currently in the keypool as pre-split
    if (split_upgrade) {
        MarkPreSplitKeys();
    }
    // Regenerate the keypool if upgraded to HD
    if (hd_upgrade) {
        if (!NewKeyPool()) {
            error = _("Unable to generate keys");
            return false;
        }
    }
    return true;
}

bool LegacyScriptPubKeyMan::HavePrivateKeys() const
{
    LOCK(cs_KeyStore);
    return !mapKeys.empty() || !mapCryptedKeys.empty();
}

void LegacyScriptPubKeyMan::RewriteDB()
{
    LOCK(cs_KeyStore);
    setInternalKeyPool.clear();
    setExternalKeyPool.clear();
    m_pool_key_to_index.clear();
    // Note: can't top-up keypool here, because wallet is locked.
    // User will be prompted to unlock wallet the next operation
    // that requires a new key.
}

static int64_t GetOldestKeyTimeInPool(const std::set<int64_t>& setKeyPool, WalletBatch& batch) {
    if (setKeyPool.empty()) {
        return GetTime();
    }

    CKeyPool keypool;
    int64_t nIndex = *(setKeyPool.begin());
    if (!batch.ReadPool(nIndex, keypool)) {
        throw std::runtime_error(std::string(__func__) + ": read oldest key in keypool failed");
    }
    assert(keypool.vchPubKey.IsValid());
    return keypool.nTime;
}

std::optional<int64_t> LegacyScriptPubKeyMan::GetOldestKeyPoolTime() const
{
    LOCK(cs_KeyStore);

    WalletBatch batch(m_storage.GetDatabase());

    // load oldest key from keypool, get time and return
    int64_t oldestKey = GetOldestKeyTimeInPool(setExternalKeyPool, batch);
    if (IsHDEnabled() && m_storage.CanSupportFeature(FEATURE_HD_SPLIT)) {
        oldestKey = std::max(GetOldestKeyTimeInPool(setInternalKeyPool, batch), oldestKey);
        if (!set_pre_split_keypool.empty()) {
            oldestKey = std::max(GetOldestKeyTimeInPool(set_pre_split_keypool, batch), oldestKey);
        }
    }

    return oldestKey;
}

size_t LegacyScriptPubKeyMan::KeypoolCountExternalKeys() const
{
    LOCK(cs_KeyStore);
    return setExternalKeyPool.size() + set_pre_split_keypool.size();
}

unsigned int LegacyScriptPubKeyMan::GetKeyPoolSize() const
{
    LOCK(cs_KeyStore);
    return setInternalKeyPool.size() + setExternalKeyPool.size() + set_pre_split_keypool.size();
}

int64_t LegacyScriptPubKeyMan::GetTimeFirstKey() const
{
    LOCK(cs_KeyStore);
    return nTimeFirstKey;
}

std::unique_ptr<SigningProvider> LegacyScriptPubKeyMan::GetSolvingProvider(const CScript& script) const
{
    return std::make_unique<LegacySigningProvider>(*this);
}

bool LegacyScriptPubKeyMan::CanProvide(const CScript& script, SignatureData& sigdata)
{
    IsMineResult ismine = IsMineInner(*this, script);
    if (ismine == IsMineResult::SPENDABLE || ismine == IsMineResult::WATCH_ONLY) {
        // If ismine, it means we recognize keys or script ids in the script, or
        // are watching the script itself, and we can at least provide metadata
        // or solving information, even if not able to sign fully.
        return true;
    } else {
        // If, given the stuff in sigdata, we could make a valid signature, then we can provide for this script
        ProduceSignature(*this, DUMMY_SIGNATURE_CREATOR, script, sigdata);
        if (!sigdata.signatures.empty()) {
            // If we could make signatures, make sure we have a private key to actually make a signature
            bool has_privkeys = false;
            for (const auto& key_sig_pair : sigdata.signatures) {
                has_privkeys |= HaveKey(key_sig_pair.first);
            }
            return has_privkeys;
        }
        return false;
    }
}

bool LegacyScriptPubKeyMan::SignTransaction(CMutableTransaction& tx, const std::map<COutPoint, Coin>& coins, const uint256& chain_id, std::map<int, bilingual_str>& input_errors) const
{
    return ::SignTransaction(tx, this, coins, chain_id, input_errors);
}

SigningResult LegacyScriptPubKeyMan::SignMessage(const std::string& message, const PKHash& pkhash, std::string& str_sig) const
{
    CKey key;
    if (!GetKey(ToKeyID(pkhash), key)) {
        return SigningResult::PRIVATE_KEY_NOT_AVAILABLE;
    }

    if (MessageSign(key, message, str_sig)) {
        return SigningResult::OK;
    }
    return SigningResult::SIGNING_FAILED;
}

TransactionError LegacyScriptPubKeyMan::FillPSBT(PartiallySignedTransaction& psbtx, const PrecomputedTransactionData& txdata, const uint256& chain_id, int sighash_type, bool sign, bool bip32derivs, int* n_signed, bool finalize) const
{
    if (n_signed) {
        *n_signed = 0;
    }
    for (unsigned int i = 0; i < psbtx.tx->vin.size(); ++i) {
        const CTxIn& txin = psbtx.tx->vin[i];
        PSBTInput& input = psbtx.inputs.at(i);

        if (PSBTInputSigned(input)) {
            continue;
        }

        // Get the Sighash type
        if (sign && input.sighash_type != std::nullopt && *input.sighash_type != sighash_type) {
            return TransactionError::SIGHASH_MISMATCH;
        }

        // Check non_witness_utxo has specified prevout
        if (input.non_witness_utxo) {
            if (txin.prevout.n >= input.non_witness_utxo->vout.size()) {
                return TransactionError::MISSING_INPUTS;
            }
        } else if (input.witness_utxo.IsNull()) {
            // There's no UTXO so we can just skip this now
            continue;
        }
        SignatureData sigdata;
        input.FillSignatureData(sigdata);
        SignPSBTInput(HidingSigningProvider(this, !sign, !bip32derivs), psbtx, i, &txdata, chain_id, sighash_type, nullptr, finalize);

        bool signed_one = PSBTInputSigned(input);
        if (n_signed && (signed_one || !sign)) {
            // If sign is false, we assume that we _could_ sign if we get here. This
            // will never have false negatives; it is hard to tell under what i
            // circumstances it could have false positives.
            (*n_signed)++;
        }
    }

    // Fill in the bip32 keypaths and redeemscripts for the outputs so that hardware wallets can identify change
    for (unsigned int i = 0; i < psbtx.tx->vout.size(); ++i) {
        UpdatePSBTOutput(HidingSigningProvider(this, true, !bip32derivs), psbtx, i);
    }

    return TransactionError::OK;
}

std::unique_ptr<CKeyMetadata> LegacyScriptPubKeyMan::GetMetadata(const CTxDestination& dest) const
{
    LOCK(cs_KeyStore);

    CKeyID key_id = GetKeyForDestination(*this, dest);
    if (!key_id.IsNull()) {
        auto it = mapKeyMetadata.find(key_id);
        if (it != mapKeyMetadata.end()) {
            return std::make_unique<CKeyMetadata>(it->second);
        }
    }

    CScript scriptPubKey = GetScriptForDestination(dest);
    auto it = m_script_metadata.find(CScriptID(scriptPubKey));
    if (it != m_script_metadata.end()) {
        return std::make_unique<CKeyMetadata>(it->second);
    }

    return nullptr;
}

uint256 LegacyScriptPubKeyMan::GetID() const
{
    return uint256::ONE;
}

/**
 * Update wallet first key creation time. This should be called whenever keys
 * are added to the wallet, with the oldest key creation time.
 */
void LegacyScriptPubKeyMan::UpdateTimeFirstKey(int64_t nCreateTime)
{
    AssertLockHeld(cs_KeyStore);
    if (nCreateTime <= 1) {
        // Cannot determine birthday information, so set the wallet birthday to
        // the beginning of time.
        nTimeFirstKey = 1;
    } else if (!nTimeFirstKey || nCreateTime < nTimeFirstKey) {
        nTimeFirstKey = nCreateTime;
    }
}

bool LegacyScriptPubKeyMan::LoadKey(const CKey& key, const CPubKey &pubkey)
{
    return AddKeyPubKeyInner(key, pubkey);
}

bool LegacyScriptPubKeyMan::AddKeyPubKey(const CKey& secret, const CPubKey &pubkey)
{
    LOCK(cs_KeyStore);
    WalletBatch batch(m_storage.GetDatabase());
    return LegacyScriptPubKeyMan::AddKeyPubKeyWithDB(batch, secret, pubkey);
}

bool LegacyScriptPubKeyMan::AddKeyPubKeyWithDB(WalletBatch& batch, const CKey& secret, const CPubKey& pubkey)
{
    AssertLockHeld(cs_KeyStore);

    // Make sure we aren't adding private keys to private key disabled wallets
    assert(!m_storage.IsWalletFlagSet(WALLET_FLAG_DISABLE_PRIVATE_KEYS));

    // FillableSigningProvider has no concept of wallet databases, but calls AddCryptedKey
    // which is overridden below.  To avoid flushes, the database handle is
    // tunneled through to it.
    bool needsDB = !encrypted_batch;
    if (needsDB) {
        encrypted_batch = &batch;
    }
    if (!AddKeyPubKeyInner(secret, pubkey)) {
        if (needsDB) encrypted_batch = nullptr;
        return false;
    }
    if (needsDB) encrypted_batch = nullptr;

    // check if we need to remove from watch-only
    CScript script;
    script = GetScriptForDestination(WitnessV0KeyHash(pubkey));
    if (HaveWatchOnly(script)) {
        RemoveWatchOnly(script);
    }

    if (!m_storage.HasEncryptionKeys()) {
        return batch.WriteKey(pubkey,
                                                 secret.GetPrivKey(),
                                                 mapKeyMetadata[pubkey.GetID()]);
    }
    m_storage.UnsetBlankWalletFlag(batch);
    return true;
}

bool LegacyScriptPubKeyMan::LoadCScript(const CScript& redeemScript)
{
    /* A sanity check was added in pull #3843 to avoid adding redeemScripts
     * that never can be redeemed. However, old wallets may still contain
     * these. Do not add them to the wallet and warn. */
    if (redeemScript.size() > MAX_SCRIPT_ELEMENT_SIZE)
    {
        std::string strAddr = EncodeDestination(ScriptHash(redeemScript));
        WalletLogPrintf("%s: Warning: This wallet contains a redeemScript of size %i which exceeds maximum size %i thus can never be redeemed. Do not use address %s.\n", __func__, redeemScript.size(), MAX_SCRIPT_ELEMENT_SIZE, strAddr);
        return true;
    }

    return FillableSigningProvider::AddCScript(redeemScript);
}

void LegacyScriptPubKeyMan::LoadKeyMetadata(const CKeyID& keyID, const CKeyMetadata& meta)
{
    LOCK(cs_KeyStore);
    UpdateTimeFirstKey(meta.nCreateTime);
    mapKeyMetadata[keyID] = meta;
}

void LegacyScriptPubKeyMan::LoadScriptMetadata(const CScriptID& script_id, const CKeyMetadata& meta)
{
    LOCK(cs_KeyStore);
    UpdateTimeFirstKey(meta.nCreateTime);
    m_script_metadata[script_id] = meta;
}

bool LegacyScriptPubKeyMan::AddKeyPubKeyInner(const CKey& key, const CPubKey &pubkey)
{
    LOCK(cs_KeyStore);
    if (!m_storage.HasEncryptionKeys()) {
        return FillableSigningProvider::AddKeyPubKey(key, pubkey);
    }

    if (m_storage.IsLocked()) {
        return false;
    }

    std::vector<unsigned char> vchCryptedSecret;
    CKeyingMaterial vchSecret(key.begin(), key.end());
    if (!EncryptSecret(m_storage.GetEncryptionKey(), vchSecret, pubkey.GetHash(), vchCryptedSecret)) {
        return false;
    }

    if (!AddCryptedKey(pubkey, vchCryptedSecret)) {
        return false;
    }
    return true;
}

bool LegacyScriptPubKeyMan::LoadCryptedKey(const CPubKey &vchPubKey, const std::vector<unsigned char> &vchCryptedSecret, bool checksum_valid)
{
    // Set fDecryptionThoroughlyChecked to false when the checksum is invalid
    if (!checksum_valid) {
        fDecryptionThoroughlyChecked = false;
    }

    return AddCryptedKeyInner(vchPubKey, vchCryptedSecret);
}

bool LegacyScriptPubKeyMan::AddCryptedKeyInner(const CPubKey &vchPubKey, const std::vector<unsigned char> &vchCryptedSecret)
{
    LOCK(cs_KeyStore);
    assert(mapKeys.empty());

    mapCryptedKeys[vchPubKey.GetID()] = make_pair(vchPubKey, vchCryptedSecret);
    ImplicitlyLearnRelatedKeyScripts(vchPubKey);
    return true;
}

bool LegacyScriptPubKeyMan::AddCryptedKey(const CPubKey &vchPubKey,
                            const std::vector<unsigned char> &vchCryptedSecret)
{
    if (!AddCryptedKeyInner(vchPubKey, vchCryptedSecret))
        return false;
    {
        LOCK(cs_KeyStore);
        if (encrypted_batch)
            return encrypted_batch->WriteCryptedKey(vchPubKey,
                                                        vchCryptedSecret,
                                                        mapKeyMetadata[vchPubKey.GetID()]);
        else
            return WalletBatch(m_storage.GetDatabase()).WriteCryptedKey(vchPubKey,
                                                            vchCryptedSecret,
                                                            mapKeyMetadata[vchPubKey.GetID()]);
    }
}

bool LegacyScriptPubKeyMan::HaveWatchOnly(const CScript &dest) const
{
    LOCK(cs_KeyStore);
    return setWatchOnly.count(dest) > 0;
}

bool LegacyScriptPubKeyMan::HaveWatchOnly() const
{
    LOCK(cs_KeyStore);
    return (!setWatchOnly.empty());
}

static bool ExtractPubKey(const CScript &dest, CPubKey& pubKeyOut)
{
    std::vector<std::vector<unsigned char>> solutions;
    return Solver(dest, solutions) == TxoutType::PUBKEY &&
        (pubKeyOut = CPubKey(solutions[0])).IsFullyValid();
}

bool LegacyScriptPubKeyMan::RemoveWatchOnly(const CScript &dest)
{
    {
        LOCK(cs_KeyStore);
        setWatchOnly.erase(dest);
        CPubKey pubKey;
        if (ExtractPubKey(dest, pubKey)) {
            mapWatchKeys.erase(pubKey.GetID());
        }
        // Related CScripts are not removed; having superfluous scripts around is
        // harmless (see comment in ImplicitlyLearnRelatedKeyScripts).
    }

    if (!HaveWatchOnly())
        NotifyWatchonlyChanged(false);
    if (!WalletBatch(m_storage.GetDatabase()).EraseWatchOnly(dest))
        return false;

    return true;
}

bool LegacyScriptPubKeyMan::LoadWatchOnly(const CScript &dest)
{
    return AddWatchOnlyInMem(dest);
}

bool LegacyScriptPubKeyMan::AddWatchOnlyInMem(const CScript &dest)
{
    LOCK(cs_KeyStore);
    setWatchOnly.insert(dest);
    CPubKey pubKey;
    if (ExtractPubKey(dest, pubKey)) {
        mapWatchKeys[pubKey.GetID()] = pubKey;
        ImplicitlyLearnRelatedKeyScripts(pubKey);
    }
    return true;
}

bool LegacyScriptPubKeyMan::AddWatchOnlyWithDB(WalletBatch &batch, const CScript& dest)
{
    if (!AddWatchOnlyInMem(dest))
        return false;
    const CKeyMetadata& meta = m_script_metadata[CScriptID(dest)];
    UpdateTimeFirstKey(meta.nCreateTime);
    NotifyWatchonlyChanged(true);
    if (batch.WriteWatchOnly(dest, meta)) {
        m_storage.UnsetBlankWalletFlag(batch);
        return true;
    }
    return false;
}

bool LegacyScriptPubKeyMan::AddWatchOnlyWithDB(WalletBatch &batch, const CScript& dest, int64_t create_time)
{
    m_script_metadata[CScriptID(dest)].nCreateTime = create_time;
    return AddWatchOnlyWithDB(batch, dest);
}

bool LegacyScriptPubKeyMan::AddWatchOnly(const CScript& dest)
{
    WalletBatch batch(m_storage.GetDatabase());
    return AddWatchOnlyWithDB(batch, dest);
}

bool LegacyScriptPubKeyMan::AddWatchOnly(const CScript& dest, int64_t nCreateTime)
{
    m_script_metadata[CScriptID(dest)].nCreateTime = nCreateTime;
    return AddWatchOnly(dest);
}

void LegacyScriptPubKeyMan::LoadHDChain(const CHDChain& chain)
{
    LOCK(cs_KeyStore);
    m_hd_chain = chain;
}

void LegacyScriptPubKeyMan::AddHDChain(const CHDChain& chain)
{
    LOCK(cs_KeyStore);
    // Store the new chain
    if (!WalletBatch(m_storage.GetDatabase()).WriteHDChain(chain)) {
        throw std::runtime_error(std::string(__func__) + ": writing chain failed");
    }
    // When there's an old chain, add it as an inactive chain as we are now rotating hd chains
    if (!m_hd_chain.seed_id.IsNull()) {
        AddInactiveHDChain(m_hd_chain);
    }

    m_hd_chain = chain;
}

void LegacyScriptPubKeyMan::AddInactiveHDChain(const CHDChain& chain)
{
    LOCK(cs_KeyStore);
    assert(!chain.seed_id.IsNull());
    m_inactive_hd_chains[chain.seed_id] = chain;
}

bool LegacyScriptPubKeyMan::HaveKey(const CKeyID &address) const
{
    LOCK(cs_KeyStore);
    if (!m_storage.HasEncryptionKeys()) {
        return FillableSigningProvider::HaveKey(address);
    }
    return mapCryptedKeys.count(address) > 0;
}

bool LegacyScriptPubKeyMan::GetKey(const CKeyID &address, CKey& keyOut) const
{
    LOCK(cs_KeyStore);
    if (!m_storage.HasEncryptionKeys()) {
        return FillableSigningProvider::GetKey(address, keyOut);
    }

    CryptedKeyMap::const_iterator mi = mapCryptedKeys.find(address);
    if (mi != mapCryptedKeys.end())
    {
        const CPubKey &vchPubKey = (*mi).second.first;
        const std::vector<unsigned char> &vchCryptedSecret = (*mi).second.second;
        return DecryptKey(m_storage.GetEncryptionKey(), vchCryptedSecret, vchPubKey, keyOut);
    }
    return false;
}

bool LegacyScriptPubKeyMan::GetKeyOrigin(const CKeyID& keyID, KeyOriginInfo& info) const
{
    CKeyMetadata meta;
    {
        LOCK(cs_KeyStore);
        auto it = mapKeyMetadata.find(keyID);
        if (it == mapKeyMetadata.end()) {
            return false;
        }
        meta = it->second;
    }
    if (meta.has_key_origin) {
        std::copy(meta.key_origin.fingerprint, meta.key_origin.fingerprint + 4, info.fingerprint);
        info.path = meta.key_origin.path;
    } else { // Single pubkeys get the master fingerprint of themselves
        std::copy(keyID.begin(), keyID.begin() + 4, info.fingerprint);
    }
    return true;
}

bool LegacyScriptPubKeyMan::GetWatchPubKey(const CKeyID &address, CPubKey &pubkey_out) const
{
    LOCK(cs_KeyStore);
    WatchKeyMap::const_iterator it = mapWatchKeys.find(address);
    if (it != mapWatchKeys.end()) {
        pubkey_out = it->second;
        return true;
    }
    return false;
}

bool LegacyScriptPubKeyMan::GetPubKey(const CKeyID &address, CPubKey& vchPubKeyOut) const
{
    LOCK(cs_KeyStore);
    if (!m_storage.HasEncryptionKeys()) {
        if (!FillableSigningProvider::GetPubKey(address, vchPubKeyOut)) {
            return GetWatchPubKey(address, vchPubKeyOut);
        }
        return true;
    }

    CryptedKeyMap::const_iterator mi = mapCryptedKeys.find(address);
    if (mi != mapCryptedKeys.end())
    {
        vchPubKeyOut = (*mi).second.first;
        return true;
    }
    // Check for watch-only pubkeys
    return GetWatchPubKey(address, vchPubKeyOut);
}

CPubKey LegacyScriptPubKeyMan::GenerateNewKey(WalletBatch &batch, CHDChain& hd_chain, bool internal)
{
    assert(!m_storage.IsWalletFlagSet(WALLET_FLAG_DISABLE_PRIVATE_KEYS));
    assert(!m_storage.IsWalletFlagSet(WALLET_FLAG_BLANK_WALLET));
    AssertLockHeld(cs_KeyStore);
    bool fCompressed = m_storage.CanSupportFeature(FEATURE_COMPRPUBKEY); // default to compressed public keys if we want 0.6.0 wallets

    CKey secret;

    // Create new metadata
    int64_t nCreationTime = GetTime();
    CKeyMetadata metadata(nCreationTime);

    // use HD key derivation if HD was enabled during wallet creation and a seed is present
    if (IsHDEnabled()) {
        DeriveNewChildKey(batch, metadata, secret, hd_chain, (m_storage.CanSupportFeature(FEATURE_HD_SPLIT) ? internal : false));
    } else {
        secret.MakeNewKey(fCompressed);
    }

    // Compressed public keys were introduced in version 0.6.0
    if (fCompressed) {
        m_storage.SetMinVersion(FEATURE_COMPRPUBKEY);
    }

    CPubKey pubkey = secret.GetPubKey();
    assert(secret.VerifyPubKey(pubkey));

    mapKeyMetadata[pubkey.GetID()] = metadata;
    UpdateTimeFirstKey(nCreationTime);

    if (!AddKeyPubKeyWithDB(batch, secret, pubkey)) {
        throw std::runtime_error(std::string(__func__) + ": AddKey failed");
    }
    return pubkey;
}

//! Try to derive an extended key, throw if it fails.
static void DeriveExtKey(CExtKey& key_in, unsigned int index, CExtKey& key_out) {
    if (!key_in.Derive(key_out, index)) {
        throw std::runtime_error("Could not derive extended key");
    }
}

void LegacyScriptPubKeyMan::DeriveNewChildKey(WalletBatch &batch, CKeyMetadata& metadata, CKey& secret, CHDChain& hd_chain, bool internal)
{
    // for now we use a fixed keypath scheme of m/0'/0'/k
    CKey seed;                     //seed (256bit)
    CExtKey masterKey;             //hd master key
    CExtKey accountKey;            //key at m/0'
    CExtKey chainChildKey;         //key at m/0'/0' (external) or m/0'/1' (internal)
    CExtKey childKey;              //key at m/0'/0'/<n>'

    // try to get the seed
    if (!GetKey(hd_chain.seed_id, seed))
        throw std::runtime_error(std::string(__func__) + ": seed not found");

    masterKey.SetSeed(seed);

    // derive m/0'
    // use hardened derivation (child keys >= 0x80000000 are hardened after bip32)
    DeriveExtKey(masterKey, BIP32_HARDENED_KEY_LIMIT, accountKey);

    // derive m/0'/0' (external chain) OR m/0'/1' (internal chain)
    assert(internal ? m_storage.CanSupportFeature(FEATURE_HD_SPLIT) : true);
    DeriveExtKey(accountKey, BIP32_HARDENED_KEY_LIMIT+(internal ? 1 : 0), chainChildKey);

    // derive child key at next index, skip keys already known to the wallet
    do {
        // always derive hardened keys
        // childIndex | BIP32_HARDENED_KEY_LIMIT = derive childIndex in hardened child-index-range
        // example: 1 | BIP32_HARDENED_KEY_LIMIT == 0x80000001 == 2147483649
        if (internal) {
            DeriveExtKey(chainChildKey, hd_chain.nInternalChainCounter | BIP32_HARDENED_KEY_LIMIT, childKey);
            metadata.hdKeypath = "m/0'/1'/" + ToString(hd_chain.nInternalChainCounter) + "'";
            metadata.key_origin.path.push_back(0 | BIP32_HARDENED_KEY_LIMIT);
            metadata.key_origin.path.push_back(1 | BIP32_HARDENED_KEY_LIMIT);
            metadata.key_origin.path.push_back(hd_chain.nInternalChainCounter | BIP32_HARDENED_KEY_LIMIT);
            hd_chain.nInternalChainCounter++;
        }
        else {
            DeriveExtKey(chainChildKey, hd_chain.nExternalChainCounter | BIP32_HARDENED_KEY_LIMIT, childKey);
            metadata.hdKeypath = "m/0'/0'/" + ToString(hd_chain.nExternalChainCounter) + "'";
            metadata.key_origin.path.push_back(0 | BIP32_HARDENED_KEY_LIMIT);
            metadata.key_origin.path.push_back(0 | BIP32_HARDENED_KEY_LIMIT);
            metadata.key_origin.path.push_back(hd_chain.nExternalChainCounter | BIP32_HARDENED_KEY_LIMIT);
            hd_chain.nExternalChainCounter++;
        }
    } while (HaveKey(childKey.key.GetPubKey().GetID()));
    secret = childKey.key;
    metadata.hd_seed_id = hd_chain.seed_id;
    CKeyID master_id = masterKey.key.GetPubKey().GetID();
    std::copy(master_id.begin(), master_id.begin() + 4, metadata.key_origin.fingerprint);
    metadata.has_key_origin = true;
    // update the chain model in the database
    if (hd_chain.seed_id == m_hd_chain.seed_id && !batch.WriteHDChain(hd_chain))
        throw std::runtime_error(std::string(__func__) + ": writing HD chain model failed");
}

void LegacyScriptPubKeyMan::LoadKeyPool(int64_t nIndex, const CKeyPool &keypool)
{
    LOCK(cs_KeyStore);
    if (keypool.m_pre_split) {
        set_pre_split_keypool.insert(nIndex);
    } else if (keypool.fInternal) {
        setInternalKeyPool.insert(nIndex);
    } else {
        setExternalKeyPool.insert(nIndex);
    }
    m_max_keypool_index = std::max(m_max_keypool_index, nIndex);
    m_pool_key_to_index[keypool.vchPubKey.GetID()] = nIndex;

    // If no metadata exists yet, create a default with the pool key's
    // creation time. Note that this may be overwritten by actually
    // stored metadata for that key later, which is fine.
    CKeyID keyid = keypool.vchPubKey.GetID();
    if (mapKeyMetadata.count(keyid) == 0)
        mapKeyMetadata[keyid] = CKeyMetadata(keypool.nTime);
}

bool LegacyScriptPubKeyMan::CanGenerateKeys() const
{
    // A wallet can generate keys if it has an HD seed (IsHDEnabled) or it is a non-HD wallet (pre FEATURE_HD)
    LOCK(cs_KeyStore);
    return IsHDEnabled() || !m_storage.CanSupportFeature(FEATURE_HD);
}

CPubKey LegacyScriptPubKeyMan::GenerateNewSeed()
{
    assert(!m_storage.IsWalletFlagSet(WALLET_FLAG_DISABLE_PRIVATE_KEYS));
    CKey key;
    key.MakeNewKey(true);
    return DeriveNewSeed(key);
}

CPubKey LegacyScriptPubKeyMan::DeriveNewSeed(const CKey& key)
{
    int64_t nCreationTime = GetTime();
    CKeyMetadata metadata(nCreationTime);

    // calculate the seed
    CPubKey seed = key.GetPubKey();
    assert(key.VerifyPubKey(seed));

    // set the hd keypath to "s" -> Seed, refers the seed to itself
    metadata.hdKeypath     = "s";
    metadata.has_key_origin = false;
    metadata.hd_seed_id = seed.GetID();

    {
        LOCK(cs_KeyStore);

        // mem store the metadata
        mapKeyMetadata[seed.GetID()] = metadata;

        // write the key&metadata to the database
        if (!AddKeyPubKey(key, seed))
            throw std::runtime_error(std::string(__func__) + ": AddKeyPubKey failed");
    }

    return seed;
}

void LegacyScriptPubKeyMan::SetHDSeed(const CPubKey& seed)
{
    LOCK(cs_KeyStore);
    // store the keyid (hash160) together with
    // the child index counter in the database
    // as a hdchain object
    CHDChain newHdChain;
    newHdChain.nVersion = m_storage.CanSupportFeature(FEATURE_HD_SPLIT) ? CHDChain::VERSION_HD_CHAIN_SPLIT : CHDChain::VERSION_HD_BASE;
    newHdChain.seed_id = seed.GetID();
    AddHDChain(newHdChain);
    NotifyCanGetAddressesChanged();
    WalletBatch batch(m_storage.GetDatabase());
    m_storage.UnsetBlankWalletFlag(batch);
}

/**
 * Mark old keypool keys as used,
 * and generate all new keys
 */
bool LegacyScriptPubKeyMan::NewKeyPool()
{
    if (m_storage.IsWalletFlagSet(WALLET_FLAG_DISABLE_PRIVATE_KEYS)) {
        return false;
    }
    {
        LOCK(cs_KeyStore);
        WalletBatch batch(m_storage.GetDatabase());

        for (const int64_t nIndex : setInternalKeyPool) {
            batch.ErasePool(nIndex);
        }
        setInternalKeyPool.clear();

        for (const int64_t nIndex : setExternalKeyPool) {
            batch.ErasePool(nIndex);
        }
        setExternalKeyPool.clear();

        for (const int64_t nIndex : set_pre_split_keypool) {
            batch.ErasePool(nIndex);
        }
        set_pre_split_keypool.clear();

        m_pool_key_to_index.clear();

        if (!TopUp()) {
            return false;
        }
        WalletLogPrintf("LegacyScriptPubKeyMan::NewKeyPool rewrote keypool\n");
    }
    return true;
}

bool LegacyScriptPubKeyMan::TopUp(unsigned int kpSize)
{
    if (!CanGenerateKeys()) {
        return false;
    }

    if (!TopUpChain(m_hd_chain, kpSize)) {
        return false;
    }
    for (auto& [chain_id, chain] : m_inactive_hd_chains) {
        if (!TopUpChain(chain, kpSize)) {
            return false;
        }
    }
    NotifyCanGetAddressesChanged();
    return true;
}

bool LegacyScriptPubKeyMan::TopUpChain(CHDChain& chain, unsigned int kpSize)
{
    LOCK(cs_KeyStore);

    if (m_storage.IsLocked()) return false;

    // Top up key pool
    unsigned int nTargetSize;
    if (kpSize > 0) {
        nTargetSize = kpSize;
    } else {
        nTargetSize = m_keypool_size;
    }
    int64_t target = std::max((int64_t) nTargetSize, int64_t{1});

    // count amount of available keys (internal, external)
    // make sure the keypool of external and internal keys fits the user selected target (-keypool)
    int64_t missingExternal;
    int64_t missingInternal;
    if (chain == m_hd_chain) {
        missingExternal = std::max(target - (int64_t)setExternalKeyPool.size(), int64_t{0});
        missingInternal = std::max(target - (int64_t)setInternalKeyPool.size(), int64_t{0});
    } else {
        missingExternal = std::max(target - (chain.nExternalChainCounter - chain.m_next_external_index), int64_t{0});
        missingInternal = std::max(target - (chain.nInternalChainCounter - chain.m_next_internal_index), int64_t{0});
    }

    if (!IsHDEnabled() || !m_storage.CanSupportFeature(FEATURE_HD_SPLIT)) {
        // don't create extra internal keys
        missingInternal = 0;
    }
    bool internal = false;
    WalletBatch batch(m_storage.GetDatabase());
    for (int64_t i = missingInternal + missingExternal; i--;) {
        if (i < missingInternal) {
            internal = true;
        }

        CPubKey pubkey(GenerateNewKey(batch, chain, internal));
        if (chain == m_hd_chain) {
            AddKeypoolPubkeyWithDB(pubkey, internal, batch);
        }
    }
    if (missingInternal + missingExternal > 0) {
        if (chain == m_hd_chain) {
            WalletLogPrintf("keypool added %d keys (%d internal), size=%u (%u internal)\n", missingInternal + missingExternal, missingInternal, setInternalKeyPool.size() + setExternalKeyPool.size() + set_pre_split_keypool.size(), setInternalKeyPool.size());
        } else {
            WalletLogPrintf("inactive seed with id %s added %d external keys, %d internal keys\n", HexStr(chain.seed_id), missingExternal, missingInternal);
        }
    }
    return true;
}

void LegacyScriptPubKeyMan::AddKeypoolPubkeyWithDB(const CPubKey& pubkey, const bool internal, WalletBatch& batch)
{
    LOCK(cs_KeyStore);
    assert(m_max_keypool_index < std::numeric_limits<int64_t>::max()); // How in the hell did you use so many keys?
    int64_t index = ++m_max_keypool_index;
    if (!batch.WritePool(index, CKeyPool(pubkey, internal))) {
        throw std::runtime_error(std::string(__func__) + ": writing imported pubkey failed");
    }
    if (internal) {
        setInternalKeyPool.insert(index);
    } else {
        setExternalKeyPool.insert(index);
    }
    m_pool_key_to_index[pubkey.GetID()] = index;
}

void LegacyScriptPubKeyMan::KeepDestination(int64_t nIndex, const OutputType& type)
{
    assert(type != OutputType::BECH32M);
    // Remove from key pool
    WalletBatch batch(m_storage.GetDatabase());
    batch.ErasePool(nIndex);
    CPubKey pubkey;
    bool have_pk = GetPubKey(m_index_to_reserved_key.at(nIndex), pubkey);
    assert(have_pk);
    LearnRelatedScripts(pubkey, type);
    m_index_to_reserved_key.erase(nIndex);
    WalletLogPrintf("keypool keep %d\n", nIndex);
}

void LegacyScriptPubKeyMan::ReturnDestination(int64_t nIndex, bool fInternal, const CTxDestination&)
{
    // Return to key pool
    {
        LOCK(cs_KeyStore);
        if (fInternal) {
            setInternalKeyPool.insert(nIndex);
        } else if (!set_pre_split_keypool.empty()) {
            set_pre_split_keypool.insert(nIndex);
        } else {
            setExternalKeyPool.insert(nIndex);
        }
        CKeyID& pubkey_id = m_index_to_reserved_key.at(nIndex);
        m_pool_key_to_index[pubkey_id] = nIndex;
        m_index_to_reserved_key.erase(nIndex);
        NotifyCanGetAddressesChanged();
    }
    WalletLogPrintf("keypool return %d\n", nIndex);
}

bool LegacyScriptPubKeyMan::GetKeyFromPool(CPubKey& result, const OutputType type)
{
    assert(type != OutputType::BECH32M);
    if (!CanGetAddresses(/*internal=*/ false)) {
        return false;
    }

    CKeyPool keypool;
    {
        LOCK(cs_KeyStore);
        int64_t nIndex;
        if (!ReserveKeyFromKeyPool(nIndex, keypool, /*fRequestedInternal=*/ false) && !m_storage.IsWalletFlagSet(WALLET_FLAG_DISABLE_PRIVATE_KEYS)) {
            if (m_storage.IsLocked()) return false;
            WalletBatch batch(m_storage.GetDatabase());
            result = GenerateNewKey(batch, m_hd_chain, /*internal=*/ false);
            return true;
        }
        KeepDestination(nIndex, type);
        result = keypool.vchPubKey;
    }
    return true;
}

bool LegacyScriptPubKeyMan::ReserveKeyFromKeyPool(int64_t& nIndex, CKeyPool& keypool, bool fRequestedInternal)
{
    nIndex = -1;
    keypool.vchPubKey = CPubKey();
    {
        LOCK(cs_KeyStore);

        bool fReturningInternal = fRequestedInternal;
        fReturningInternal &= (IsHDEnabled() && m_storage.CanSupportFeature(FEATURE_HD_SPLIT)) || m_storage.IsWalletFlagSet(WALLET_FLAG_DISABLE_PRIVATE_KEYS);
        bool use_split_keypool = set_pre_split_keypool.empty();
        std::set<int64_t>& setKeyPool = use_split_keypool ? (fReturningInternal ? setInternalKeyPool : setExternalKeyPool) : set_pre_split_keypool;

        // Get the oldest key
        if (setKeyPool.empty()) {
            return false;
        }

        WalletBatch batch(m_storage.GetDatabase());

        auto it = setKeyPool.begin();
        nIndex = *it;
        setKeyPool.erase(it);
        if (!batch.ReadPool(nIndex, keypool)) {
            throw std::runtime_error(std::string(__func__) + ": read failed");
        }
        CPubKey pk;
        if (!GetPubKey(keypool.vchPubKey.GetID(), pk)) {
            throw std::runtime_error(std::string(__func__) + ": unknown key in key pool");
        }
        // If the key was pre-split keypool, we don't care about what type it is
        if (use_split_keypool && keypool.fInternal != fReturningInternal) {
            throw std::runtime_error(std::string(__func__) + ": keypool entry misclassified");
        }
        if (!keypool.vchPubKey.IsValid()) {
            throw std::runtime_error(std::string(__func__) + ": keypool entry invalid");
        }

        assert(m_index_to_reserved_key.count(nIndex) == 0);
        m_index_to_reserved_key[nIndex] = keypool.vchPubKey.GetID();
        m_pool_key_to_index.erase(keypool.vchPubKey.GetID());
        WalletLogPrintf("keypool reserve %d\n", nIndex);
    }
    NotifyCanGetAddressesChanged();
    return true;
}

void LegacyScriptPubKeyMan::LearnRelatedScripts(const CPubKey& key, OutputType type)
{
    assert(type != OutputType::BECH32M);
    if (key.IsCompressed() && type == OutputType::BECH32) {
        CTxDestination witdest = WitnessV0KeyHash(key.GetID());
        CScript witprog = GetScriptForDestination(witdest);
        // Make sure the resulting program is solvable.
        const auto desc = InferDescriptor(witprog, *this);
        assert(desc && desc->IsSolvable());
        AddCScript(witprog);
    }
}

void LegacyScriptPubKeyMan::LearnAllRelatedScripts(const CPubKey& key)
{
    LearnRelatedScripts(key, OutputType::BECH32);
}

std::vector<CKeyPool> LegacyScriptPubKeyMan::MarkReserveKeysAsUsed(int64_t keypool_id)
{
    AssertLockHeld(cs_KeyStore);
    bool internal = setInternalKeyPool.count(keypool_id);
    if (!internal) assert(setExternalKeyPool.count(keypool_id) || set_pre_split_keypool.count(keypool_id));
    std::set<int64_t> *setKeyPool = internal ? &setInternalKeyPool : (set_pre_split_keypool.empty() ? &setExternalKeyPool : &set_pre_split_keypool);
    auto it = setKeyPool->begin();

    std::vector<CKeyPool> result;
    WalletBatch batch(m_storage.GetDatabase());
    while (it != std::end(*setKeyPool)) {
        const int64_t& index = *(it);
        if (index > keypool_id) break; // set*KeyPool is ordered

        CKeyPool keypool;
        if (batch.ReadPool(index, keypool)) { //TODO: This should be unnecessary
            m_pool_key_to_index.erase(keypool.vchPubKey.GetID());
        }
        LearnAllRelatedScripts(keypool.vchPubKey);
        batch.ErasePool(index);
        WalletLogPrintf("keypool index %d removed\n", index);
        it = setKeyPool->erase(it);
        result.push_back(std::move(keypool));
    }

    return result;
}

std::vector<CKeyID> GetAffectedKeys(const CScript& spk, const SigningProvider& provider)
{
    std::vector<CScript> dummy;
    FlatSigningProvider out;
    if (auto desc = InferDescriptor(spk, provider)) {
        desc->Expand(0, DUMMY_SIGNING_PROVIDER, dummy, out);
    }
    std::vector<CKeyID> ret;
    ret.reserve(out.pubkeys.size());
    for (const auto& entry : out.pubkeys) {
        ret.push_back(entry.first);
    }
    return ret;
}

void LegacyScriptPubKeyMan::MarkPreSplitKeys()
{
    WalletBatch batch(m_storage.GetDatabase());
    for (auto it = setExternalKeyPool.begin(); it != setExternalKeyPool.end();) {
        int64_t index = *it;
        CKeyPool keypool;
        if (!batch.ReadPool(index, keypool)) {
            throw std::runtime_error(std::string(__func__) + ": read keypool entry failed");
        }
        keypool.m_pre_split = true;
        if (!batch.WritePool(index, keypool)) {
            throw std::runtime_error(std::string(__func__) + ": writing modified keypool entry failed");
        }
        set_pre_split_keypool.insert(index);
        it = setExternalKeyPool.erase(it);
    }
}

bool LegacyScriptPubKeyMan::AddCScript(const CScript& redeemScript)
{
    WalletBatch batch(m_storage.GetDatabase());
    return AddCScriptWithDB(batch, redeemScript);
}

bool LegacyScriptPubKeyMan::AddCScriptWithDB(WalletBatch& batch, const CScript& redeemScript)
{
    if (!FillableSigningProvider::AddCScript(redeemScript))
        return false;
    if (batch.WriteCScript(Hash160(redeemScript), redeemScript)) {
        m_storage.UnsetBlankWalletFlag(batch);
        return true;
    }
    return false;
}

bool LegacyScriptPubKeyMan::AddKeyOriginWithDB(WalletBatch& batch, const CPubKey& pubkey, const KeyOriginInfo& info)
{
    LOCK(cs_KeyStore);
    std::copy(info.fingerprint, info.fingerprint + 4, mapKeyMetadata[pubkey.GetID()].key_origin.fingerprint);
    mapKeyMetadata[pubkey.GetID()].key_origin.path = info.path;
    mapKeyMetadata[pubkey.GetID()].has_key_origin = true;
    mapKeyMetadata[pubkey.GetID()].hdKeypath = WriteHDKeypath(info.path);
    return batch.WriteKeyMetadata(mapKeyMetadata[pubkey.GetID()], pubkey, true);
}

bool LegacyScriptPubKeyMan::ImportScripts(const std::set<CScript> scripts, int64_t timestamp)
{
    WalletBatch batch(m_storage.GetDatabase());
    for (const auto& entry : scripts) {
        CScriptID id(entry);
        if (HaveCScript(id)) {
            WalletLogPrintf("Already have script %s, skipping\n", HexStr(entry));
            continue;
        }
        if (!AddCScriptWithDB(batch, entry)) {
            return false;
        }

        if (timestamp > 0) {
            m_script_metadata[CScriptID(entry)].nCreateTime = timestamp;
        }
    }
    if (timestamp > 0) {
        UpdateTimeFirstKey(timestamp);
    }

    return true;
}

bool LegacyScriptPubKeyMan::ImportPrivKeys(const std::map<CKeyID, CKey>& privkey_map, const int64_t timestamp)
{
    WalletBatch batch(m_storage.GetDatabase());
    for (const auto& entry : privkey_map) {
        const CKey& key = entry.second;
        CPubKey pubkey = key.GetPubKey();
        const CKeyID& id = entry.first;
        assert(key.VerifyPubKey(pubkey));
        // Skip if we already have the key
        if (HaveKey(id)) {
            WalletLogPrintf("Already have key with pubkey %s, skipping\n", HexStr(pubkey));
            continue;
        }
        mapKeyMetadata[id].nCreateTime = timestamp;
        // If the private key is not present in the wallet, insert it.
        if (!AddKeyPubKeyWithDB(batch, key, pubkey)) {
            return false;
        }
        UpdateTimeFirstKey(timestamp);
    }
    return true;
}

bool LegacyScriptPubKeyMan::ImportPubKeys(const std::vector<CKeyID>& ordered_pubkeys, const std::map<CKeyID, CPubKey>& pubkey_map, const std::map<CKeyID, std::pair<CPubKey, KeyOriginInfo>>& key_origins, const bool add_keypool, const bool internal, const int64_t timestamp)
{
    WalletBatch batch(m_storage.GetDatabase());
    for (const auto& entry : key_origins) {
        AddKeyOriginWithDB(batch, entry.second.first, entry.second.second);
    }
    for (const CKeyID& id : ordered_pubkeys) {
        auto entry = pubkey_map.find(id);
        if (entry == pubkey_map.end()) {
            continue;
        }
        const CPubKey& pubkey = entry->second;
        CPubKey temp;
        if (GetPubKey(id, temp)) {
            // Already have pubkey, skipping
            WalletLogPrintf("Already have pubkey %s, skipping\n", HexStr(temp));
            continue;
        }
        if (!AddWatchOnlyWithDB(batch, GetScriptForDestination(WitnessV0KeyHash(pubkey)), timestamp)) {
            return false;
        }
        mapKeyMetadata[id].nCreateTime = timestamp;

        // Add to keypool only works with pubkeys
        if (add_keypool) {
            AddKeypoolPubkeyWithDB(pubkey, internal, batch);
            NotifyCanGetAddressesChanged();
        }
    }
    return true;
}

bool LegacyScriptPubKeyMan::ImportScriptPubKeys(const std::set<CScript>& script_pub_keys, const bool have_solving_data, const int64_t timestamp)
{
    WalletBatch batch(m_storage.GetDatabase());
    for (const CScript& script : script_pub_keys) {
        if (!have_solving_data || !IsMine(script)) { // Always call AddWatchOnly for non-solvable watch-only, so that watch timestamp gets updated
            if (!AddWatchOnlyWithDB(batch, script, timestamp)) {
                return false;
            }
        }
    }
    return true;
}

std::set<CKeyID> LegacyScriptPubKeyMan::GetKeys() const
{
    LOCK(cs_KeyStore);
    if (!m_storage.HasEncryptionKeys()) {
        return FillableSigningProvider::GetKeys();
    }
    std::set<CKeyID> set_address;
    for (const auto& mi : mapCryptedKeys) {
        set_address.insert(mi.first);
    }
    return set_address;
}

std::unordered_set<CScript, SaltedSipHasher> LegacyScriptPubKeyMan::GetScriptPubKeys() const
{
    LOCK(cs_KeyStore);
    std::unordered_set<CScript, SaltedSipHasher> spks;

    // Track the live WPKH lock for each key. P2PK / P2PKH are not RIGOR outputs.
    for (const auto& key_pair : mapKeys) {
        const CPubKey& pub = key_pair.second.GetPubKey();
        spks.insert(GetScriptForDestination(WitnessV0KeyHash(pub)));
    }
    for (const auto& key_pair : mapCryptedKeys) {
        const CPubKey& pub = key_pair.second.first;
        spks.insert(GetScriptForDestination(WitnessV0KeyHash(pub)));
    }

    // Track live witness programs stored in mapScripts. Do not wrap them as P2SH.
    for (const auto& script_pair : mapScripts) {
        const CScript& script = script_pair.second;
        if (IsMine(script) != ISMINE_SPENDABLE) continue;
        int wit_ver = -1;
        std::vector<unsigned char> witprog;
        if (script.IsWitnessProgram(wit_ver, witprog)) {
            if (wit_ver == 0 || (wit_ver == 1 && witprog.size() == 32)) {
                spks.insert(script);
            }
        }
    }

    // Watch-only only for constructible RIGOR locks / RIGOR_MARKER_RETURN.
    for (const auto& script : setWatchOnly) {
        if (IsRigorConstructibleScript(script)) {
            spks.insert(script);
        } else {
            WalletLogPrintf("%s: discarding non-RIGOR watch-only script %s\n", __func__, HexStr(script));
        }
    }

    return spks;
}

bool LegacyScriptPubKeyMan::DeleteRecords()
{
    LOCK(cs_KeyStore);
    WalletBatch batch(m_storage.GetDatabase());
    return batch.EraseRecords(DBKeys::LEGACY_TYPES);
}

util::Result<CTxDestination> DescriptorScriptPubKeyMan::GetNewDestination(const OutputType type)
{
    {
        LOCK(cs_desc_man);
        if (m_wallet_descriptor.descriptor &&
            IsSingleSlotNativeResDescriptor(*m_wallet_descriptor.descriptor) &&
            m_wallet_descriptor.next_index > 0) {
            // Persist via TopUpWithDB's WalletBatch. A nested batch here
            // would deadlock once SQLite write serialization is enabled
            // (bitcoin/bitcoin#29112) and used to ROLLBACK a concurrent txn.
            m_wallet_descriptor.next_index = 0;
        }
    }
    // Returns true if this descriptor supports getting new addresses. Conditions where we may be unable to fetch them (e.g. locked) are caught later
    if (!CanGetAddresses()) {
        return util::Error{_("No addresses available")};
    }
    {
        LOCK(cs_desc_man);
        assert(m_wallet_descriptor.descriptor->IsSingleType());
        std::optional<OutputType> desc_addr_type = m_wallet_descriptor.descriptor->GetOutputType();
        assert(desc_addr_type);
        if (type != *desc_addr_type) {
            throw std::runtime_error(std::string(__func__) + ": Types are inconsistent. Stored type does not match type of newly generated address");
        }

        const bool single_slot_res = IsSingleSlotNativeResDescriptor(*m_wallet_descriptor.descriptor);

        WalletBatch batch(m_storage.GetDatabase());
        const bool began = batch.TxnBegin();
        if (!TopUpWithDB(batch)) {
            return util::Error{_("Error: Keypool ran out, please call keypoolrefill first")};
        }

        // Get the scriptPubKey from the descriptor
        FlatSigningProvider out_keys;
        std::vector<CScript> scripts_temp;
        const int32_t expand_index = single_slot_res ? 0 : m_wallet_descriptor.next_index;
        if (m_wallet_descriptor.range_end <= m_max_cached_index && !TopUpWithDB(batch, 1)) {
            // We can't generate anymore keys
            return util::Error{_("Error: Keypool ran out, please call keypoolrefill first")};
        }
        if (!m_wallet_descriptor.descriptor->ExpandFromCache(expand_index, m_wallet_descriptor.cache, scripts_temp, out_keys)) {
            // We can't generate anymore keys
            return util::Error{_("Error: Keypool ran out, please call keypoolrefill first")};
        }

        CTxDestination dest;
        if (!ExtractDestination(scripts_temp[0], dest)) {
            return util::Error{_("Error: Cannot extract destination from the generated scriptpubkey")}; // shouldn't happen
        }
        if (!single_slot_res) {
            m_wallet_descriptor.next_index++;
            if (!batch.WriteDescriptor(GetID(), m_wallet_descriptor)) {
                throw std::runtime_error(std::string(__func__) + ": writing descriptor failed");
            }
        }
        if (began && !batch.TxnCommit()) {
            throw std::runtime_error(std::string(__func__) + ": committing new destination failed");
        }
        return dest;
    }
}

std::optional<int32_t> DescriptorScriptPubKeyMan::FindCachedDescriptorIndex(const CScript& script) const
{
    AssertLockHeld(cs_desc_man);

    if (!m_wallet_descriptor.descriptor) {
        return std::nullopt;
    }

    const auto it_exact = m_map_script_pub_keys.find(script);
    if (it_exact != m_map_script_pub_keys.end()) {
        if (ConvSignTraceActive()) {
            LogPrint(BCLog::CONVERGESIGN,
                     "convtrace FindCachedDescriptorIndex HIT exact map index=%i\n",
                     it_exact->second);
        }
        return it_exact->second;
    }

    // Native P2WPKH (witness v0, 20-byte HASH160(pubkey)): ORU and other assets use the same v0 program as
    // BTC wpkh; the exact script may be absent from m_map_script_pub_keys while pubkeys are already cached
    // from descriptor expansion. Match only keys already present in m_map_pubkeys (no imports / rescan).
    int wver_wpkh = 0;
    std::vector<unsigned char> wprog_wpkh;
    if (script.IsWitnessProgram(wver_wpkh, wprog_wpkh) && wver_wpkh == 0 &&
        wprog_wpkh.size() == CKeyID::size()) {
        const CKeyID want_id{uint160{Span<const unsigned char>(wprog_wpkh)}};
        for (const auto& pk_entry : m_map_pubkeys) {
            const CPubKey& pk = pk_entry.first;
            if (!pk.IsCompressed()) {
                continue;
            }
            if (pk.GetID() == want_id) {
                if (ConvSignTraceActive()) {
                    LogPrint(BCLog::CONVERGESIGN,
                             "convtrace FindCachedDescriptorIndex WPKH_V0_FALLBACK index=%i\n",
                             pk_entry.second);
                }
                return pk_entry.second;
            }
        }
    }

    // res() active descriptors use OutputType::BECH32M on Rigor; witness v1 RES path below — not wpkh.
    const std::optional<OutputType> desc_ot = m_wallet_descriptor.descriptor->GetOutputType();
    if (!desc_ot || *desc_ot != OutputType::BECH32M) {
        if (ConvSignTraceActive()) {
            int wver = 0;
            std::vector<unsigned char> wprog;
            const bool is_v0_keyhash = script.IsWitnessProgram(wver, wprog) && wver == 0 && wprog.size() == 20;
            LogPrint(BCLog::CONVERGESIGN,
                     "convtrace FindCachedDescriptorIndex MISS (no exact map, wpkh v0 pubkey fallback miss); "
                     "desc_output_type=%s m_map_script_pub_keys.size=%u m_map_pubkeys.size=%u witness_v0_keyhash=%i\n",
                     desc_ot ? FormatOutputType(*desc_ot).c_str() : "(none)",
                     static_cast<unsigned>(m_map_script_pub_keys.size()),
                     static_cast<unsigned>(m_map_pubkeys.size()),
                     is_v0_keyhash ? 1 : 0);
        }
        return std::nullopt;
    }

    int64_t mint_maturity = 0;
    std::vector<unsigned char> mint_pkh;
    if (ParseResMintScript(script, mint_maturity, mint_pkh) && mint_pkh.size() == 20) {
        const uint160 want{mint_pkh};
        for (const auto& pk_entry : m_map_pubkeys) {
            const CPubKey& pk = pk_entry.first;
            if (!pk.IsCompressed()) continue;
            if (pk.size() != CPubKey::COMPRESSED_SIZE) continue;
            const unsigned char header = pk[0];
            if (header != 0x00 && header != 0xFF) continue;
            if (Hash160(Span<const unsigned char>(pk.data() + 1, pk.size() - 1)) == want) {
                return pk_entry.second;
            }
        }
        // res() descriptors cache witness-v1 scripts whose program is pk32.
        // RIGOR_MARKER_RES_MINT commits to HASH160(pk).
        for (const auto& pk_entry : m_map_pubkeys) {
            const CPubKey& pk = pk_entry.first;
            const std::vector<unsigned char> payload = pk.GetEd25519Payload();
            if (payload.size() != ed25519::PUBKEY_SIZE) continue;
            if (Hash160(payload) == want) return pk_entry.second;
        }
        for (const auto& [spk, index] : m_map_script_pub_keys) {
            std::vector<std::vector<unsigned char>> sols;
            if (Solver(spk, sols) != TxoutType::WITNESS_V1_RES || sols.empty()) continue;
            std::unique_ptr<FlatSigningProvider> keys = GetSigningProvider(index, true);
            if (!keys) continue;
            for (const auto& [prog_id, secret] : keys->res_v1_program_to_ed_secret) {
                (void)prog_id;
                if (secret.size() < ed25519::PUBKEY_SIZE + ed25519::SEED_SIZE) continue;
                const Span<const unsigned char> pk32{secret.data() + ed25519::SEED_SIZE, ed25519::PUBKEY_SIZE};
                if (Hash160(pk32) == want) return index;
            }
            for (const auto& [_, key] : keys->keys) {
                if (Hash160(key.GetPubKey().GetEd25519Payload()) == want) return index;
            }
        }
        return std::nullopt;
    }

    std::vector<std::vector<unsigned char>> sols;
    if (Solver(script, sols) != TxoutType::WITNESS_V1_RES || sols.empty()) {
        return std::nullopt;
    }
    const std::vector<unsigned char>& prog = sols[0];
    if (prog.size() != WITNESS_V1_RES_SIZE) {
        return std::nullopt;
    }
    if (std::memcmp(prog.data(), RigorTags::CVG2.data(), RigorTags::CVG2.size()) == 0) {
        return std::nullopt;
    }

    for (const auto& pk_entry : m_map_pubkeys) {
        const CPubKey& pk = pk_entry.first;
        if (!pk.IsCompressed()) continue;
        if (pk.size() != CPubKey::COMPRESSED_SIZE) continue;
        const unsigned char header = pk[0];
        if (header != 0x00 && header != 0xFF) continue;
        std::vector<unsigned char> derived = pk.GetEd25519Payload();
        if (derived.size() != ed25519::PUBKEY_SIZE) continue;
        uint256 hashed;
        if (!ed25519::ResWitnessProgram(derived, Span<unsigned char>{hashed.begin(), 32})) continue;
        if (std::memcmp(hashed.begin(), prog.data(), WITNESS_V1_RES_SIZE) == 0) {
            return pk_entry.second;
        }
    }
    return std::nullopt;
}

isminetype DescriptorScriptPubKeyMan::IsMine(const CScript& script) const
{
    LOCK(cs_desc_man);
    if (!EntersUtxoSet(script)) return ISMINE_NO;
    const std::optional<int32_t> index = FindCachedDescriptorIndex(script);
    if (!index) return ISMINE_NO;
    // Match GetSigningProvider(script, false): index in cache is not enough if ExpandFromCache fails.
    std::unique_ptr<FlatSigningProvider> keys = GetSigningProvider(*index, false);
    return keys ? ISMINE_SPENDABLE : ISMINE_NO;
}

bool DescriptorScriptPubKeyMan::CheckDecryptionKey(const CKeyingMaterial& master_key, bool accept_no_keys)
{
    LOCK(cs_desc_man);
    if (!m_map_keys.empty()) {
        return false;
    }

    bool keyPass = m_map_crypted_keys.empty(); // Always pass when there are no encrypted keys
    bool keyFail = false;
    for (const auto& mi : m_map_crypted_keys) {
        const CPubKey &pubkey = mi.second.first;
        const std::vector<unsigned char> &crypted_secret = mi.second.second;
        CKey key;
        if (!DecryptKey(master_key, crypted_secret, pubkey, key)) {
            keyFail = true;
            break;
        }
        keyPass = true;
        if (m_decryption_thoroughly_checked)
            break;
    }
    if (keyPass && keyFail) {
        LogPrintf("The wallet is probably corrupted: Some keys decrypt but not all.\n");
        throw std::runtime_error("Error unlocking wallet: some keys decrypt but not all. Your wallet file may be corrupt.");
    }
    if (keyFail || (!keyPass && !accept_no_keys)) {
        return false;
    }
    m_decryption_thoroughly_checked = true;
    return true;
}

bool DescriptorScriptPubKeyMan::Encrypt(const CKeyingMaterial& master_key, WalletBatch* batch)
{
    LOCK(cs_desc_man);
    if (!m_map_crypted_keys.empty()) {
        return false;
    }

    for (const KeyMap::value_type& key_in : m_map_keys)
    {
        const CKey &key = key_in.second;
        CPubKey pubkey = key.GetPubKey();
        CKeyingMaterial secret(key.begin(), key.end());
        std::vector<unsigned char> crypted_secret;
        if (!EncryptSecret(master_key, secret, pubkey.GetHash(), crypted_secret)) {
            return false;
        }
        m_map_crypted_keys[pubkey.GetID()] = make_pair(pubkey, crypted_secret);
        batch->WriteCryptedDescriptorKey(GetID(), pubkey, crypted_secret);
    }
    m_map_keys.clear();
    return true;
}

util::Result<CTxDestination> DescriptorScriptPubKeyMan::GetReservedDestination(const OutputType type, bool internal, int64_t& index, CKeyPool& keypool)
{
    LOCK(cs_desc_man);
    auto op_dest = GetNewDestination(type);
    if (IsSingleSlotNativeResDescriptor(*m_wallet_descriptor.descriptor)) {
        index = 0;
    } else {
        index = m_wallet_descriptor.next_index - 1;
    }
    return op_dest;
}

void DescriptorScriptPubKeyMan::ReturnDestination(int64_t index, bool internal, const CTxDestination& addr)
{
    LOCK(cs_desc_man);
    // Only return when the index was the most recent
    if (m_wallet_descriptor.next_index - 1 == index) {
        m_wallet_descriptor.next_index--;
    }
    WalletBatch(m_storage.GetDatabase()).WriteDescriptor(GetID(), m_wallet_descriptor);
    NotifyCanGetAddressesChanged();
}

std::map<CKeyID, CKey> DescriptorScriptPubKeyMan::GetKeys() const
{
    AssertLockHeld(cs_desc_man);
    if (m_storage.HasEncryptionKeys() && !m_storage.IsLocked()) {
        KeyMap keys;
        for (const auto& key_pair : m_map_crypted_keys) {
            const CPubKey& pubkey = key_pair.second.first;
            const std::vector<unsigned char>& crypted_secret = key_pair.second.second;
            CKey key;
            DecryptKey(m_storage.GetEncryptionKey(), crypted_secret, pubkey, key);
            keys[pubkey.GetID()] = key;
        }
        return keys;
    }
    return m_map_keys;
}

bool DescriptorScriptPubKeyMan::PrepareDescriptorTopUp(unsigned int size, int32_t& new_range_end, int32_t& expand_from)
{
    AssertLockHeld(cs_desc_man);
    if (!m_wallet_descriptor.descriptor) {
        return false;
    }
    const int32_t next_before = m_wallet_descriptor.next_index;
    const int32_t range_end_before = m_wallet_descriptor.range_end;
    const int32_t range_start_before = m_wallet_descriptor.range_start;

    if (m_wallet_descriptor.descriptor &&
        IsSingleSlotNativeResDescriptor(*m_wallet_descriptor.descriptor) &&
        m_wallet_descriptor.next_index > 0) {
        // Keep next_index reset inside this TopUp txn. Nested WalletBatch
        // must not be created while another SQLite txn may be open
        // (bitcoin/bitcoin#29112).
        m_wallet_descriptor.next_index = 0;
    }

    const unsigned int target_size = size > 0 ? size : static_cast<unsigned int>(m_keypool_size);
    new_range_end = std::max(m_wallet_descriptor.next_index + static_cast<int32_t>(target_size),
                             m_wallet_descriptor.range_end);

    if (m_wallet_descriptor.descriptor && !m_wallet_descriptor.descriptor->IsRange()) {
        new_range_end = 1;
    }

    expand_from = m_max_cached_index + 1;
    const bool need_expand = expand_from < new_range_end;
    const bool meta_changed =
        new_range_end != range_end_before ||
        m_wallet_descriptor.next_index != next_before ||
        (!m_wallet_descriptor.descriptor->IsRange() && range_start_before != 0);
    return need_expand || meta_changed;
}

bool DescriptorScriptPubKeyMan::ApplyDescriptorTopUp(WalletBatch& batch, int32_t new_range_end, int32_t expand_from)
{
    AssertLockHeld(cs_desc_man);
    FlatSigningProvider provider;
    provider.keys = GetKeys();

    const auto topup_start{SteadyClock::now()};
    const uint256 id = GetID();
    for (int32_t i = expand_from; i < new_range_end; ++i) {
        FlatSigningProvider out_keys;
        std::vector<CScript> scripts_temp;
        DescriptorCache temp_cache;
        if (!m_wallet_descriptor.descriptor->ExpandFromCache(i, m_wallet_descriptor.cache, scripts_temp, out_keys)) {
            if (!m_wallet_descriptor.descriptor->Expand(i, provider, scripts_temp, out_keys, &temp_cache)) return false;
        }
        for (const CScript& script : scripts_temp) {
            m_map_script_pub_keys[script] = i;
        }
        for (const auto& pk_pair : out_keys.pubkeys) {
            const CPubKey& pubkey = pk_pair.second;
            if (m_map_pubkeys.count(pubkey) != 0) {
                continue;
            }
            m_map_pubkeys[pubkey] = i;
        }
        DescriptorCache new_items = m_wallet_descriptor.cache.MergeAndDiff(temp_cache);
        if (!batch.WriteDescriptorCacheItems(id, new_items)) {
            throw std::runtime_error(std::string(__func__) + ": writing cache items failed");
        }
        m_max_cached_index++;
    }

    if (m_wallet_descriptor.descriptor && !m_wallet_descriptor.descriptor->IsRange()) {
        m_wallet_descriptor.range_end = 1;
        m_wallet_descriptor.range_start = 0;
    } else {
        m_wallet_descriptor.range_end = new_range_end;
    }
    if (!batch.WriteDescriptor(GetID(), m_wallet_descriptor)) {
        throw std::runtime_error(std::string(__func__) + ": writing descriptor failed");
    }

    assert(m_wallet_descriptor.range_end - 1 == m_max_cached_index);
    WalletLogPrintf("TopUp expanded %d keys in %dms (shared_batch=%s)\n",
                    new_range_end - expand_from,
                    Ticks<std::chrono::milliseconds>(SteadyClock::now() - topup_start),
                    batch.HasActiveTxn() ? "yes" : "no");
    NotifyCanGetAddressesChanged();
    return true;
}

bool DescriptorScriptPubKeyMan::TopUp(unsigned int size)
{
    LOCK(cs_desc_man);
    int32_t new_range_end = 0;
    int32_t expand_from = 0;
    if (!PrepareDescriptorTopUp(size, new_range_end, expand_from)) {
        return true;
    }

    // Ed25519 descriptors are hardened-only (SLIP-0010): each of the ~1000
    // keypool slots is a private derive + a SQLite cache write. Without a
    // transaction, SQLite autocommits (and fsyncs) every write — about a
    // minute on Windows. One txn keeps the same records; the destructor
    // aborts if we return or throw before commit.
    WalletBatch batch(m_storage.GetDatabase());
    const bool began = batch.TxnBegin();
    const bool res = ApplyDescriptorTopUp(batch, new_range_end, expand_from);
    if (began) {
        if (res) {
            if (!batch.TxnCommit()) {
                throw std::runtime_error(std::string(__func__) + ": committing descriptor top-up failed");
            }
        }
    }
    return res;
}

bool DescriptorScriptPubKeyMan::TopUpWithDB(WalletBatch& batch, unsigned int size)
{
    LOCK(cs_desc_man);
    int32_t new_range_end = 0;
    int32_t expand_from = 0;
    if (!PrepareDescriptorTopUp(size, new_range_end, expand_from)) {
        return true;
    }
    return ApplyDescriptorTopUp(batch, new_range_end, expand_from);
}

std::vector<WalletDestination> DescriptorScriptPubKeyMan::MarkUnusedAddresses(const CScript& script, WalletBatch* batch)
{
    LOCK(cs_desc_man);
    std::vector<WalletDestination> result;
    const std::optional<int32_t> op_index = FindCachedDescriptorIndex(script);
    if (op_index) {
        int32_t index = *op_index;
        if (IsSingleSlotNativeResDescriptor(*m_wallet_descriptor.descriptor)) {
            const bool ok = batch ? TopUpWithDB(*batch) : TopUp();
            if (!ok) {
                WalletLogPrintf("%s: Topping up keypool failed (locked wallet)\n", __func__);
            }
            return result;
        }
        bool consumed = false;
        if (index >= m_wallet_descriptor.next_index) {
            WalletLogPrintf("%s: Detected a used keypool item at index %d, mark all keypool items up to this item as used\n", __func__, index);
            auto out_keys = std::make_unique<FlatSigningProvider>();
            std::vector<CScript> scripts_temp;
            while (index >= m_wallet_descriptor.next_index) {
                if (!m_wallet_descriptor.descriptor->ExpandFromCache(m_wallet_descriptor.next_index, m_wallet_descriptor.cache, scripts_temp, *out_keys)) {
                    throw std::runtime_error(std::string(__func__) + ": Unable to expand descriptor from cache");
                }
                CTxDestination dest;
                ExtractDestination(scripts_temp[0], dest);
                result.push_back({dest, std::nullopt});
                m_wallet_descriptor.next_index++;
                consumed = true;
            }
        }
        // Reused mining/receive scripts do not consume a new keypool index.
        // Skip TopUp so SQLite is not opened for a no-op WriteDescriptor.
        if (consumed) {
            if (batch) {
                if (!TopUpWithDB(*batch)) {
                    WalletLogPrintf("%s: Topping up keypool failed (locked wallet)\n", __func__);
                } else if (!batch->WriteDescriptor(GetID(), m_wallet_descriptor)) {
                    throw std::runtime_error(std::string(__func__) + ": writing descriptor failed");
                }
            } else {
                WalletBatch local_batch(m_storage.GetDatabase());
                const bool began = local_batch.TxnBegin();
                if (!TopUpWithDB(local_batch)) {
                    WalletLogPrintf("%s: Topping up keypool failed (locked wallet)\n", __func__);
                } else if (!local_batch.WriteDescriptor(GetID(), m_wallet_descriptor)) {
                    throw std::runtime_error(std::string(__func__) + ": writing descriptor failed");
                } else if (began && !local_batch.TxnCommit()) {
                    throw std::runtime_error(std::string(__func__) + ": committing descriptor top-up failed");
                }
            }
        }
    }

    return result;
}

void DescriptorScriptPubKeyMan::AddDescriptorKey(const CKey& key, const CPubKey &pubkey)
{
    LOCK(cs_desc_man);
    WalletBatch batch(m_storage.GetDatabase());
    if (!AddDescriptorKeyWithDB(batch, key, pubkey)) {
        throw std::runtime_error(std::string(__func__) + ": writing descriptor private key failed");
    }
}

bool DescriptorScriptPubKeyMan::AddDescriptorKeyWithDB(WalletBatch& batch, const CKey& key, const CPubKey &pubkey)
{
    AssertLockHeld(cs_desc_man);
    assert(!m_storage.IsWalletFlagSet(WALLET_FLAG_DISABLE_PRIVATE_KEYS));

    // Check if provided key already exists
    if (m_map_keys.find(pubkey.GetID()) != m_map_keys.end() ||
        m_map_crypted_keys.find(pubkey.GetID()) != m_map_crypted_keys.end()) {
        return true;
    }

    if (m_storage.HasEncryptionKeys()) {
        if (m_storage.IsLocked()) {
            return false;
        }

        std::vector<unsigned char> crypted_secret;
        CKeyingMaterial secret(key.begin(), key.end());
        if (!EncryptSecret(m_storage.GetEncryptionKey(), secret, pubkey.GetHash(), crypted_secret)) {
            return false;
        }

        m_map_crypted_keys[pubkey.GetID()] = make_pair(pubkey, crypted_secret);
        return batch.WriteCryptedDescriptorKey(GetID(), pubkey, crypted_secret);
    } else {
        m_map_keys[pubkey.GetID()] = key;
        return batch.WriteDescriptorKey(GetID(), pubkey, key.GetPrivKey());
    }
}

namespace {
/** WALLET_HD_V1: derive m/purpose'/coin'/0' account xkey from root (BIP44 account 0). */
bool DerivePurposeAccount(const CExtKey& root, uint32_t purpose, uint32_t coin, CExtKey& out)
{
    if (!root.Derive(out, purpose | wallet::BIP32_HARDENED_KEY_LIMIT)) return false;
    if (!out.Derive(out, coin | wallet::BIP32_HARDENED_KEY_LIMIT)) return false;
    if (!out.Derive(out, 0 | wallet::BIP32_HARDENED_KEY_LIMIT)) return false;
    return true;
}
} // namespace

bool DescriptorScriptPubKeyMan::SetupDescriptorGeneration(const CExtKey& master_key, OutputType addr_type, bool internal)
{
    LOCK(cs_desc_man);
    assert(m_storage.IsWalletFlagSet(WALLET_FLAG_DESCRIPTORS));

    // Ignore when there is already a descriptor
    if (m_wallet_descriptor.descriptor) {
        return false;
    }

    int64_t creation_time = GetTime();

    const CKeyID master_id = master_key.key.GetPubKey().GetID();
    std::vector<unsigned char> fp_bytes(master_id.begin(), master_id.begin() + 4);
    const std::string fp_hex = HexStr(fp_bytes);
    const uint32_t coin_type = Params().IsTestChain() ? 1u : 0u;

    uint32_t purpose = 84;
    switch (addr_type) {
    case OutputType::BECH32: purpose = 84; break;
    case OutputType::BECH32M: purpose = 86; break;
    case OutputType::LEGACY:
    case OutputType::P2SH_SEGWIT:
    case OutputType::UNKNOWN:
        assert(false);
    }

    CExtKey account_key;
    if (!DerivePurposeAccount(master_key, purpose, coin_type, account_key)) {
        throw std::runtime_error(strprintf("%s: BIP32 derive for purpose %u failed", __func__, purpose));
    }

    const std::string xpub_acc = EncodeExtPubKey(account_key.Neuter());
    const std::string origin = strprintf("[%s/%u'/%u'/0']", fp_hex.c_str(), purpose, coin_type);
    // C-1: Ed25519 has no public derivation. Receive/change indices MUST be hardened.
    const std::string internal_path = internal ? "/1'" : "/0'";

    std::string desc_str;
    switch (addr_type) {
    case OutputType::BECH32:
        desc_str = "wpkh(" + origin + xpub_acc + internal_path + "/*')";
        break;
    case OutputType::BECH32M:
        desc_str = "res(" + origin + xpub_acc + internal_path + "/*')";
        break;
    case OutputType::LEGACY:
    case OutputType::P2SH_SEGWIT:
    case OutputType::UNKNOWN:
        assert(false);
    }

    // Make the descriptor
    FlatSigningProvider keys;
    std::string error;
    std::unique_ptr<Descriptor> desc = Parse(desc_str, keys, error, false);
    if (!desc) {
        throw std::runtime_error(strprintf("%s: invalid res() descriptor: %s", __func__, error));
    }
    WalletDescriptor w_desc(std::move(desc), creation_time, 0, 0, 0);
    m_wallet_descriptor = w_desc;

    // Store private keys the descriptor signing path needs. The descriptor string uses the *account*
    // xpub (purpose/coin'/0') as BIP32PubkeyProvider::m_root_extkey, so GetExtKey() looks up
    // CKeyID(account_xpub.pubkey) — not the wallet master key id. Always persist the account private
    // key material; also keep the master key for backup/compatibility with code that expects it.
    WalletBatch batch(m_storage.GetDatabase());
    if (!AddDescriptorKeyWithDB(batch, master_key.key, master_key.key.GetPubKey())) {
        throw std::runtime_error(std::string(__func__) + ": writing descriptor master private key failed");
    }
    if (!AddDescriptorKeyWithDB(batch, account_key.key, account_key.key.GetPubKey())) {
        throw std::runtime_error(std::string(__func__) + ": writing descriptor account private key failed");
    }
    if (!batch.WriteDescriptor(GetID(), m_wallet_descriptor)) {
        throw std::runtime_error(std::string(__func__) + ": writing descriptor failed");
    }

    // TopUp
    TopUp();

    m_storage.UnsetBlankWalletFlag(batch);
    return true;
}

bool DescriptorScriptPubKeyMan::IsHDEnabled() const
{
    LOCK(cs_desc_man);
    return m_wallet_descriptor.descriptor->IsRange();
}

bool DescriptorScriptPubKeyMan::CanGetAddresses(bool internal) const
{
    LOCK(cs_desc_man);
    if (!m_wallet_descriptor.descriptor->IsSingleType()) {
        return false;
    }
    // Rigor RES: non-ranged res(hex) is a dedicated single program; default wallets are ranged HD.
    if (IsSingleSlotNativeResDescriptor(*m_wallet_descriptor.descriptor)) {
        return m_wallet_descriptor.range_end > 0 &&
               m_max_cached_index >= 0 &&
               (HavePrivateKeys() || m_wallet_descriptor.next_index < m_wallet_descriptor.range_end);
    }
    // Standard ranged descriptor wallets: require IsRange and keypool headroom or private keys.
    return m_wallet_descriptor.descriptor->IsRange() &&
           (HavePrivateKeys() || m_wallet_descriptor.next_index < m_wallet_descriptor.range_end);
}

bool DescriptorScriptPubKeyMan::HavePrivateKeys() const
{
    LOCK(cs_desc_man);
    return m_map_keys.size() > 0 || m_map_crypted_keys.size() > 0;
}

std::optional<int64_t> DescriptorScriptPubKeyMan::GetOldestKeyPoolTime() const
{
    // This is only used for getwalletinfo output and isn't relevant to descriptor wallets.
    return std::nullopt;
}


unsigned int DescriptorScriptPubKeyMan::GetKeyPoolSize() const
{
    LOCK(cs_desc_man);
    return m_wallet_descriptor.range_end - m_wallet_descriptor.next_index;
}

int64_t DescriptorScriptPubKeyMan::GetTimeFirstKey() const
{
    LOCK(cs_desc_man);
    return m_wallet_descriptor.creation_time;
}

std::unique_ptr<FlatSigningProvider> DescriptorScriptPubKeyMan::GetSigningProvider(const CScript& script, bool include_private) const
{
    LOCK(cs_desc_man);

    const std::optional<int32_t> index = FindCachedDescriptorIndex(script);
    if (!index) {
        if (ConvSignTraceActive()) {
            LogPrint(BCLog::CONVERGESIGN,
                     "convtrace DescriptorScriptPubKeyMan::GetSigningProvider(script): FindCachedDescriptorIndex "
                     "returned nullopt; script=%s\n",
                     HexStr(script));
        }
        // Expected miss when another ScriptPubKeyMan owns this output; avoid spam by not logging here.
        return nullptr;
    }

    return GetSigningProvider(*index, include_private);
}

std::unique_ptr<FlatSigningProvider> DescriptorScriptPubKeyMan::GetSigningProvider(const CPubKey& pubkey) const
{
    LOCK(cs_desc_man);

    // Find index of the pubkey
    auto it = m_map_pubkeys.find(pubkey);
    if (it == m_map_pubkeys.end()) {
        return nullptr;
    }
    int32_t index = it->second;

    // Always try to get the signing provider with private keys. This function should only be called during signing anyways
    return GetSigningProvider(index, true);
}

std::unique_ptr<FlatSigningProvider> DescriptorScriptPubKeyMan::GetSigningProvider(int32_t index, bool include_private) const
{
    AssertLockHeld(cs_desc_man);

    std::unique_ptr<FlatSigningProvider> out_keys = std::make_unique<FlatSigningProvider>();

    // m_map_signing_providers is filled after ExpandFromCache but *before* ExpandPrivate so it never
    // stores decrypted secp keys. Merging that cached pubkey-only provider and then ExpandPrivate
    // usually works, but we have observed witness-v0 signing failures (empty witness) on multi-SPKM
    // descriptor wallets. For any call that will run ExpandPrivate, bypass the cache entirely.
    if (include_private && HavePrivateKeys()) {
        std::vector<CScript> scripts_temp;
        if (!m_wallet_descriptor.descriptor->ExpandFromCache(index, m_wallet_descriptor.cache, scripts_temp, *out_keys)) {
            WalletLogPrintf("%s: ExpandFromCache failed for descriptor index %i\n", __func__, index);
            return nullptr;
        }
        FlatSigningProvider master_provider;
        master_provider.keys = GetKeys();
        if (master_provider.keys.empty()) {
            WalletLogPrintf("%s: GetKeys() returned empty while HavePrivateKeys()=true; index=%i\n", __func__, index);
        }
        m_wallet_descriptor.descriptor->ExpandPrivate(index, master_provider, *out_keys);
        for (const auto& [_, key] : out_keys->keys) {
            const std::vector<unsigned char> pk32 = key.GetPubKey().GetEd25519Payload();
            if (pk32.size() != ed25519::PUBKEY_SIZE) continue;
            uint256 prog_id;
            if (!ed25519::ResWitnessProgram(pk32, Span<unsigned char>{prog_id.begin(), 32})) continue;
            std::array<unsigned char, ed25519::PUBKEY_SIZE> pub{};
            std::array<unsigned char, ed25519::SECRETKEY_SIZE> sk{};
            if (!ed25519::SeedToKeypair(Span<const unsigned char>{key.begin(), ed25519::SEED_SIZE}, pub, sk)) continue;
            out_keys->res_v1_program_to_ed_secret.emplace(prog_id, sk);
        }
        if (out_keys->keys.empty()) {
            WalletLogPrintf("%s: no private keys in signing provider after ExpandPrivate; index=%i (pubkeys cached=%u)\n",
                            __func__, index, static_cast<unsigned>(out_keys->pubkeys.size()));
        }
        return out_keys;
    }

    // Fetch SigningProvider from cache to avoid re-deriving (solving / watch-only paths)
    auto it = m_map_signing_providers.find(index);
    if (it != m_map_signing_providers.end()) {
        out_keys->Merge(FlatSigningProvider{it->second});
    } else {
        // Get the scripts, keys, and key origins for this script
        std::vector<CScript> scripts_temp;
        if (!m_wallet_descriptor.descriptor->ExpandFromCache(index, m_wallet_descriptor.cache, scripts_temp, *out_keys)) {
            WalletLogPrintf("%s: ExpandFromCache failed for descriptor index %i\n", __func__, index);
            return nullptr;
        }

        // Cache SigningProvider so we don't need to re-derive if we need this SigningProvider again
        m_map_signing_providers[index] = *out_keys;
    }

    if (include_private && !HavePrivateKeys()) {
        WalletLogPrintf("%s: include_private=true but HavePrivateKeys()=false (cannot ExpandPrivate); index=%i m_map_keys=%u m_map_crypted_keys=%u\n",
                        __func__, index, static_cast<unsigned>(m_map_keys.size()), static_cast<unsigned>(m_map_crypted_keys.size()));
    }

    if (include_private && out_keys->keys.empty()) {
        WalletLogPrintf("%s: no private keys in signing provider; index=%i (pubkeys cached=%u)\n",
                        __func__, index, static_cast<unsigned>(out_keys->pubkeys.size()));
    }

    return out_keys;
}

std::unique_ptr<SigningProvider> DescriptorScriptPubKeyMan::GetSolvingProvider(const CScript& script) const
{
    return GetSigningProvider(script, false);
}

bool DescriptorScriptPubKeyMan::CanProvide(const CScript& script, SignatureData& sigdata)
{
    (void)sigdata;
    // Same predicate as GetSolvingProvider (pubkey-only / watch path): solvable iff we expose a provider.
    return GetSigningProvider(script, false) != nullptr;
}

bool DescriptorScriptPubKeyMan::SignTransaction(CMutableTransaction& tx, const std::map<COutPoint, Coin>& coins, const uint256& chain_id, std::map<int, bilingual_str>& input_errors) const
{
    std::unique_ptr<FlatSigningProvider> keys = std::make_unique<FlatSigningProvider>();
    for (const auto& coin_pair : coins) {
        std::unique_ptr<FlatSigningProvider> coin_keys = GetSigningProvider(coin_pair.second.out.scriptPubKey, true);
        if (!coin_keys) {
            continue;
        }
        keys->Merge(std::move(*coin_keys));
    }

    if (keys->keys.empty()) {
        WalletLogPrintf("%s: merged signing provider has no private keys (inputs=%u); signing will fail unless another ScriptPubKeyMan succeeds\n",
                        __func__, static_cast<unsigned>(coins.size()));
    }

    return ::SignTransaction(tx, keys.get(), coins, chain_id, input_errors);
}

SigningResult DescriptorScriptPubKeyMan::SignMessage(const std::string& message, const PKHash& pkhash, std::string& str_sig) const
{
    std::unique_ptr<FlatSigningProvider> keys = GetSigningProvider(GetScriptForDestination(pkhash), true);
    if (!keys) {
        return SigningResult::PRIVATE_KEY_NOT_AVAILABLE;
    }

    CKey key;
    if (!keys->GetKey(ToKeyID(pkhash), key)) {
        return SigningResult::PRIVATE_KEY_NOT_AVAILABLE;
    }

    if (!MessageSign(key, message, str_sig)) {
        return SigningResult::SIGNING_FAILED;
    }
    return SigningResult::OK;
}

TransactionError DescriptorScriptPubKeyMan::FillPSBT(PartiallySignedTransaction& psbtx, const PrecomputedTransactionData& txdata, const uint256& chain_id, int sighash_type, bool sign, bool bip32derivs, int* n_signed, bool finalize) const
{
    if (n_signed) {
        *n_signed = 0;
    }
    for (unsigned int i = 0; i < psbtx.tx->vin.size(); ++i) {
        const CTxIn& txin = psbtx.tx->vin[i];
        PSBTInput& input = psbtx.inputs.at(i);

        if (PSBTInputSigned(input)) {
            continue;
        }

        // Get the Sighash type
        if (sign && input.sighash_type != std::nullopt && *input.sighash_type != sighash_type) {
            return TransactionError::SIGHASH_MISMATCH;
        }

        // Get the scriptPubKey to know which SigningProvider to use
        CScript script;
        if (!input.witness_utxo.IsNull()) {
            script = input.witness_utxo.scriptPubKey;
        } else if (input.non_witness_utxo) {
            if (txin.prevout.n >= input.non_witness_utxo->vout.size()) {
                return TransactionError::MISSING_INPUTS;
            }
            script = input.non_witness_utxo->vout[txin.prevout.n].scriptPubKey;
        } else {
            // There's no UTXO so we can just skip this now
            continue;
        }
        SignatureData sigdata;
        input.FillSignatureData(sigdata);

        std::unique_ptr<FlatSigningProvider> keys = std::make_unique<FlatSigningProvider>();
        std::unique_ptr<FlatSigningProvider> script_keys = GetSigningProvider(script, /*include_private=*/sign);
        if (script_keys) {
            keys->Merge(std::move(*script_keys));
        } else {
            // Maybe there are pubkeys listed that we can sign for
            std::vector<CPubKey> pubkeys;
            pubkeys.reserve(input.hd_keypaths.size() + 2);

            // ECDSA Pubkeys
            for (const auto& [pk, _] : input.hd_keypaths) {
                pubkeys.push_back(pk);
            }

            // Taproot output pubkey
            std::vector<std::vector<unsigned char>> sols;
            if (Solver(script, sols) == TxoutType::WITNESS_V1_RES && sols[0].size() == WITNESS_V1_RES_SIZE) {
                // 32-byte v1 program: x-only Ed25519 pubkey material. Skip ORB2 claim (36-byte) scripts.
                sols[0].insert(sols[0].begin(), 0x02);
                pubkeys.emplace_back(sols[0]);
                sols[0][0] = 0x03;
                pubkeys.emplace_back(sols[0]);
            }

            // Taproot pubkeys
            for (const auto& pk_pair : input.m_tap_bip32_paths) {
                const XOnlyPubKey& pubkey = pk_pair.first;
                for (unsigned char prefix : {0x02, 0x03}) {
                    unsigned char b[33] = {prefix};
                    std::copy(pubkey.begin(), pubkey.end(), b + 1);
                    CPubKey fullpubkey;
                    fullpubkey.Set(b, b + 33);
                    pubkeys.push_back(fullpubkey);
                }
            }

            for (const auto& pubkey : pubkeys) {
                std::unique_ptr<FlatSigningProvider> pk_keys = GetSigningProvider(pubkey);
                if (pk_keys) {
                    keys->Merge(std::move(*pk_keys));
                }
            }
        }

        SignPSBTInput(HidingSigningProvider(keys.get(), /*hide_secret=*/!sign, /*hide_origin=*/!bip32derivs), psbtx, i, &txdata, chain_id, sighash_type, nullptr, finalize);

        bool signed_one = PSBTInputSigned(input);
        if (n_signed && (signed_one || !sign)) {
            // If sign is false, we assume that we _could_ sign if we get here. This
            // will never have false negatives; it is hard to tell under what i
            // circumstances it could have false positives.
            (*n_signed)++;
        }
    }

    // Fill in the bip32 keypaths and redeemscripts for the outputs so that hardware wallets can identify change
    for (unsigned int i = 0; i < psbtx.tx->vout.size(); ++i) {
        std::unique_ptr<SigningProvider> keys = GetSolvingProvider(psbtx.tx->vout.at(i).scriptPubKey);
        if (!keys) {
            continue;
        }
        UpdatePSBTOutput(HidingSigningProvider(keys.get(), /*hide_secret=*/true, /*hide_origin=*/!bip32derivs), psbtx, i);
    }

    return TransactionError::OK;
}

std::unique_ptr<CKeyMetadata> DescriptorScriptPubKeyMan::GetMetadata(const CTxDestination& dest) const
{
    std::unique_ptr<SigningProvider> provider = GetSigningProvider(GetScriptForDestination(dest));
    if (provider) {
        KeyOriginInfo orig;
        CKeyID key_id = GetKeyForDestination(*provider, dest);
        if (provider->GetKeyOrigin(key_id, orig)) {
            LOCK(cs_desc_man);
            std::unique_ptr<CKeyMetadata> meta = std::make_unique<CKeyMetadata>();
            meta->key_origin = orig;
            meta->has_key_origin = true;
            meta->nCreateTime = m_wallet_descriptor.creation_time;
            return meta;
        }
    }
    return nullptr;
}

uint256 DescriptorScriptPubKeyMan::GetID() const
{
    LOCK(cs_desc_man);
    std::string desc_str = m_wallet_descriptor.descriptor->ToString();
    uint256 id;
    CSHA256().Write((unsigned char*)desc_str.data(), desc_str.size()).Finalize(id.begin());
    return id;
}

void DescriptorScriptPubKeyMan::SetCache(const DescriptorCache& cache)
{
    LOCK(cs_desc_man);
    m_wallet_descriptor.cache = cache;
    for (int32_t i = m_wallet_descriptor.range_start; i < m_wallet_descriptor.range_end; ++i) {
        FlatSigningProvider out_keys;
        std::vector<CScript> scripts_temp;
        if (!m_wallet_descriptor.descriptor->ExpandFromCache(i, m_wallet_descriptor.cache, scripts_temp, out_keys)) {
            throw std::runtime_error("Error: Unable to expand wallet descriptor from cache");
        }
        // Add all of the scriptPubKeys to the scriptPubKey set
        for (const CScript& script : scripts_temp) {
            if (m_map_script_pub_keys.count(script) != 0) {
                throw std::runtime_error(strprintf("Error: Already loaded script at index %d as being at index %d", i, m_map_script_pub_keys[script]));
            }
            m_map_script_pub_keys[script] = i;
        }
        for (const auto& pk_pair : out_keys.pubkeys) {
            const CPubKey& pubkey = pk_pair.second;
            if (m_map_pubkeys.count(pubkey) != 0) {
                // We don't need to give an error here.
                // It doesn't matter which of many valid indexes the pubkey has, we just need an index where we can derive it and it's private key
                continue;
            }
            m_map_pubkeys[pubkey] = i;
        }
        m_max_cached_index++;
    }
}

bool DescriptorScriptPubKeyMan::AddKey(const CKeyID& key_id, const CKey& key)
{
    LOCK(cs_desc_man);
    m_map_keys[key_id] = key;
    return true;
}

bool DescriptorScriptPubKeyMan::AddCryptedKey(const CKeyID& key_id, const CPubKey& pubkey, const std::vector<unsigned char>& crypted_key)
{
    LOCK(cs_desc_man);
    if (!m_map_keys.empty()) {
        return false;
    }

    m_map_crypted_keys[key_id] = make_pair(pubkey, crypted_key);
    return true;
}

bool DescriptorScriptPubKeyMan::HasWalletDescriptor(const WalletDescriptor& desc) const
{
    LOCK(cs_desc_man);
    return m_wallet_descriptor.descriptor != nullptr && desc.descriptor != nullptr && m_wallet_descriptor.descriptor->ToString() == desc.descriptor->ToString();
}

void DescriptorScriptPubKeyMan::WriteDescriptor()
{
    LOCK(cs_desc_man);
    WalletBatch batch(m_storage.GetDatabase());
    if (!batch.WriteDescriptor(GetID(), m_wallet_descriptor)) {
        throw std::runtime_error(std::string(__func__) + ": writing descriptor failed");
    }
}

WalletDescriptor DescriptorScriptPubKeyMan::GetWalletDescriptor() const
{
    return m_wallet_descriptor;
}

std::unordered_set<CScript, SaltedSipHasher> DescriptorScriptPubKeyMan::GetScriptPubKeys() const
{
    return GetScriptPubKeys(0);
}

std::unordered_set<CScript, SaltedSipHasher> DescriptorScriptPubKeyMan::GetScriptPubKeys(int32_t minimum_index) const
{
    LOCK(cs_desc_man);
    std::unordered_set<CScript, SaltedSipHasher> script_pub_keys;
    script_pub_keys.reserve(m_map_script_pub_keys.size());

    for (auto const& [script_pub_key, index] : m_map_script_pub_keys) {
        if (index >= minimum_index) script_pub_keys.insert(script_pub_key);
    }
    return script_pub_keys;
}

int32_t DescriptorScriptPubKeyMan::GetEndRange() const
{
    return m_max_cached_index + 1;
}

bool DescriptorScriptPubKeyMan::GetDescriptorString(std::string& out, const bool priv) const
{
    LOCK(cs_desc_man);

    FlatSigningProvider provider;
    provider.keys = GetKeys();

    if (priv) {
        // For the private version, always return the master key to avoid
        // exposing child private keys. The risk implications of exposing child
        // private keys together with the parent xpub may be non-obvious for users.
        return m_wallet_descriptor.descriptor->ToPrivateString(provider, out);
    }

    return m_wallet_descriptor.descriptor->ToNormalizedString(provider, out, &m_wallet_descriptor.cache);
}

void DescriptorScriptPubKeyMan::UpgradeDescriptorCache()
{
    LOCK(cs_desc_man);
    if (m_storage.IsLocked() || m_storage.IsWalletFlagSet(WALLET_FLAG_LAST_HARDENED_XPUB_CACHED)) {
        return;
    }

    // Skip if we have the last hardened xpub cache
    if (m_wallet_descriptor.cache.GetCachedLastHardenedExtPubKeys().size() > 0) {
        return;
    }

    // Expand the descriptor
    FlatSigningProvider provider;
    provider.keys = GetKeys();
    FlatSigningProvider out_keys;
    std::vector<CScript> scripts_temp;
    DescriptorCache temp_cache;
    if (!m_wallet_descriptor.descriptor->Expand(0, provider, scripts_temp, out_keys, &temp_cache)){
        throw std::runtime_error("Unable to expand descriptor");
    }

    // Cache the last hardened xpubs
    DescriptorCache diff = m_wallet_descriptor.cache.MergeAndDiff(temp_cache);
    if (!WalletBatch(m_storage.GetDatabase()).WriteDescriptorCacheItems(GetID(), diff)) {
        throw std::runtime_error(std::string(__func__) + ": writing cache items failed");
    }
}

void DescriptorScriptPubKeyMan::UpdateWalletDescriptor(WalletDescriptor& descriptor)
{
    LOCK(cs_desc_man);
    std::string error;
    if (!CanUpdateToWalletDescriptor(descriptor, error)) {
        throw std::runtime_error(std::string(__func__) + ": " + error);
    }

    m_map_pubkeys.clear();
    m_map_script_pub_keys.clear();
    m_max_cached_index = -1;
    m_wallet_descriptor = descriptor;
}

bool DescriptorScriptPubKeyMan::CanUpdateToWalletDescriptor(const WalletDescriptor& descriptor, std::string& error)
{
    LOCK(cs_desc_man);
    if (!HasWalletDescriptor(descriptor)) {
        error = "can only update matching descriptor";
        return false;
    }

    if (descriptor.range_start > m_wallet_descriptor.range_start ||
        descriptor.range_end < m_wallet_descriptor.range_end) {
        // Use inclusive range for error
        error = strprintf("new range must include current range = [%d,%d]",
                          m_wallet_descriptor.range_start,
                          m_wallet_descriptor.range_end - 1);
        return false;
    }

    return true;
}
} // namespace wallet
