// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <script/sigcache.h>

#include <hash.h>
#include <random.h>
#include <uint256.h>
#include <util/system.h>

#include <cuckoocache.h>

#include <mutex>
#include <optional>
#include <shared_mutex>
#include <vector>

namespace {
/**
 * Valid signature cache, to avoid doing expensive signature checking
 * twice for every transaction (once when accepted into memory pool, and
 * again when accepted into the block chain)
 */
class CSignatureCache
{
private:
     //! Entries are SHA256(nonce || 'D' pad || signature hash || public key || signature):
    CSHA256 m_salted_hasher_ed25519;
    typedef CuckooCache::cache<uint256, SignatureCacheHasher> map_type;
    map_type setValid;
    std::shared_mutex cs_sigcache;

public:
    CSignatureCache()
    {
        uint256 nonce = GetRandHash();
        static constexpr unsigned char PADDING_ED25519[32] = {'D'};
        m_salted_hasher_ed25519.Write(nonce.begin(), 32);
        m_salted_hasher_ed25519.Write(PADDING_ED25519, 32);
    }

    void
    ComputeEntryEd25519(uint256& entry, const uint256& hash, Span<const unsigned char> sig, Span<const unsigned char> pubkey) const
    {
        CSHA256 hasher = m_salted_hasher_ed25519;
        hasher.Write(hash.begin(), 32).Write(pubkey.data(), pubkey.size()).Write(sig.data(), sig.size()).Finalize(entry.begin());
    }

    bool
    Get(const uint256& entry, const bool erase)
    {
        std::shared_lock<std::shared_mutex> lock(cs_sigcache);
        return setValid.contains(entry, erase);
    }

    void Set(const uint256& entry)
    {
        std::unique_lock<std::shared_mutex> lock(cs_sigcache);
        setValid.insert(entry);
    }
    std::optional<std::pair<uint32_t, size_t>> setup_bytes(size_t n)
    {
        return setValid.setup_bytes(n);
    }
};

static CSignatureCache signatureCache;
} // namespace

bool InitSignatureCache(size_t max_size_bytes)
{
    auto setup_results = signatureCache.setup_bytes(max_size_bytes);
    if (!setup_results) return false;

    const auto [num_elems, approx_size_bytes] = *setup_results;
    LogPrintf("Using %zu MiB out of %zu MiB requested for signature cache, able to store %zu elements\n",
              approx_size_bytes >> 20, max_size_bytes >> 20, num_elems);
    return true;
}

// CONSENSUS INVARIANT (must freeze): signatureCache.Set(entry) MUST be called
// only after the underlying verifier has returned true for the same (sighash,
// sig, pubkey) tuple.
bool CachingTransactionSignatureChecker::VerifyEd25519Signature(Span<const unsigned char> sig, Span<const unsigned char> pubkey, const uint256& sighash) const
{
    uint256 entry;
    signatureCache.ComputeEntryEd25519(entry, sighash, sig, pubkey);
    if (signatureCache.Get(entry, !store)) return true;
    if (!TransactionSignatureChecker::VerifyEd25519Signature(sig, pubkey, sighash)) return false;
    if (store) signatureCache.Set(entry);
    return true;
}
