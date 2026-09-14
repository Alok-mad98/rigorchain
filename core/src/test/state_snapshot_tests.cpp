// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/state_snapshot.h>

#include <boost/test/unit_test.hpp>

#include <chain.h>
#include <coins.h>
#include <crypto/muhash.h>
#include <crypto/sha256.h>
#include <hash.h>
#include <primitives/transaction.h>
#include <script/script.h>
#include <script/standard.h>
#include <serialize.h>
#include <span.h>
#include <streams.h>
#include <test/util/setup_common.h>
#include <uint256.h>

#include <cstring>
#include <string_view>
#include <utility>
#include <vector>

// TASK-H2 H-1-2 byte-pinned unit test vectors for
// `Consensus::ComputeDeterministicStateHash`.
//
// Three independent byte-level guards are pinned here, all reachable from
// `BasicTestingSetup` (no chainstate / ChainstateManager / mining fixture):
//
//   1. `state_hash_pre_genesis_byte_vector`
//      Pins the EXACT 32-byte output of the pre-genesis path
//      (`pindex == nullptr`). The pre-image is reconstructed from raw byte
//      primitives, NOT from any helper that the production code also uses,
//      so any drift in the domain tag, the preamble layout (height LE32,
//      tip-hash, UTXO-hash ordering and widths), or the choice of SHA256d
//      will fail the comparison.
//
//   2. `state_hash_pre_genesis_is_idempotent`
//      Same input MUST yield identical 32 bytes across repeated calls.
//      Defends against accidental introduction of hidden mutable state
//      (timer, RNG, static counter) inside the snapshot path.
//      See TASK-H2 R-5 / R-10.
//
//   3. `state_hash_inmem_chain_matches_independent_reference`
//      Pins the post-genesis path on an in-memory `CCoinsViewCache`
//      populated with deterministic coins and a stack-allocated mock
//      `CBlockIndex`. The reference digest is computed by an independent
//      `MuHash3072 + HashWriter` reimplementation inside this file (not
//      shared with production), so any drift in the leaf encoding
//      (`COutPoint || Coin::Serialize`), in the MuHash finalize bytes, or
//      in the FinalizeDigest preamble layout fails the comparison.
//
// Why in-memory and not `TestChain100Setup`?
//   The chain-100 fixture path was attempted earlier and reproducibly
//   SIGSEGVed inside the fixture ctor on this branch (Boost reported a
//   null-pointer dereference at the fixture-ctor checkpoint after ~30 ms,
//   far below the time needed to mine 100 regtest blocks). Diagnosing and
//   fixing `TestChain100Setup` would be out-of-scope per the H-phase
//   APPROVAL TEMPLATE's HARD CONSTRAINT against unrelated refactors. The
//   in-memory equivalent above covers the same property surface (non-empty
//   UTXO set + post-genesis preamble + independent reference) without
//   touching that fixture.

BOOST_FIXTURE_TEST_SUITE(state_snapshot_tests, BasicTestingSetup)

namespace {

//! Pre-genesis pre-image (STEP-0101 v5):
//!   ASCII "RIGOR/STATE_HASH/v5" (19 bytes)
//! || 4 zero bytes (height LE32 == 0)
//! || 32 zero bytes (tip hash)
//! || 32 zero bytes (UTXO MuHash slot — zero sentinel at pre-genesis)
//! || 73 zero bytes (index projection trailer)
//! || 36 zero bytes (aggregate trailer)
//!
//! Total length: 196 bytes.
//!
//! The index projection shrank from 105 to 73 bytes when the CONVERGENCE draw
//! was removed: `CBlockIndex::m_draw_candidate_root` (32 bytes) is no longer a
//! field, so the trailer is now
//!   LE64(cumulative convergence) || u8(has_res) || last_res_mint_block_hash
//!   || LE64(block RES mint) || LE64(chain RES mint) || LE64(block converge)
//!   || LE64(chain ORU)
//! = 8 + 1 + 32 + 4*8 = 73 bytes. The domain tag was bumped v4 -> v5 for that
//! layout change, which is itself part of the pre-image.
//!
//! Expected SHA256d (raw / big-endian byte order, NOT uint256 display form),
//! recomputed offline with Python `hashlib` (double SHA-256 over the 196-byte
//! vector above).
//!
constexpr unsigned char kExpectedRawSHA256d[32] = {
    0x0e, 0xe3, 0x58, 0x25, 0x9a, 0xcb, 0x33, 0xcb,
    0x07, 0xb4, 0x98, 0xfc, 0xa6, 0xb7, 0x10, 0x4d,
    0x23, 0x44, 0x97, 0xd3, 0xb3, 0x07, 0xb0, 0x45,
    0xed, 0xe4, 0xae, 0xf1, 0x64, 0x17, 0x5c, 0x4f,
};

//! Independent reference SHA256d implementation that does NOT share
//! HashWriter glue with the production code path. This is what makes the
//! test byte-pinned rather than self-consistent: if the implementation
//! silently switches digest, tag, or preamble, the production output
//! disagrees with both this reference AND with `kExpectedRawSHA256d`.
void ReferenceSHA256d(const unsigned char* data, size_t len, unsigned char out[32])
{
    CSHA256 r1;
    r1.Write(data, len);
    unsigned char inner[32];
    r1.Finalize(inner);

    CSHA256 r2;
    r2.Write(inner, 32);
    r2.Finalize(out);
}

} // namespace

BOOST_AUTO_TEST_CASE(state_hash_pre_genesis_byte_vector)
{
    // Build the canonical pre-genesis pre-image exactly as documented in
    // state_snapshot.h. We construct it from raw byte primitives, NOT from
    // any helper that the production code also uses, so any divergence in
    // the production code's encoding will surface here.
    static constexpr char kTag[] = "RIGOR/STATE_HASH/v5";
    constexpr size_t kTagLen = sizeof(kTag) - 1; // exclude trailing NUL
    BOOST_REQUIRE_EQUAL(kTagLen, 19u);

    unsigned char preimage[19 + 4 + 32 + 32 + 73 + 36] = {};
    std::memcpy(preimage, kTag, kTagLen);
    // Remaining 177 bytes are zero: height, tip, utxo hash, index trailer,
    // aggregate trailer.

    unsigned char ref_digest[32];
    ReferenceSHA256d(preimage, sizeof(preimage), ref_digest);

    // (a) Reference SHA256d MUST match the offline-computed expected bytes.
    //     If this fails, the test's own reference is broken.
    BOOST_CHECK_EQUAL_COLLECTIONS(ref_digest, ref_digest + 32,
                                  kExpectedRawSHA256d, kExpectedRawSHA256d + 32);

    // (b) Production `ComputeDeterministicStateHash(nullptr)` MUST match
    //     those same bytes. uint256 stores its 32 bytes in begin()..end()
    //     in the same big-endian byte order that openssl prints; the
    //     display reversal happens only in uint256::ToString().
    CCoinsView base;
    CCoinsViewCache view(&base);
    const uint256 produced = Consensus::ComputeDeterministicStateHash(view, /*pindex=*/nullptr);

    BOOST_CHECK_EQUAL_COLLECTIONS(produced.begin(), produced.end(),
                                  kExpectedRawSHA256d, kExpectedRawSHA256d + 32);
}

BOOST_AUTO_TEST_CASE(state_hash_pre_genesis_is_idempotent)
{
    // Same input must yield identical 32 bytes across repeated calls.
    // This is a trivial check but defends against accidental introduction
    // of hidden mutable state (timer, RNG, static counter, etc.) inside
    // the snapshot path. See TASK-H2 R-5 / R-10.
    CCoinsView base;
    CCoinsViewCache view(&base);
    const uint256 a = Consensus::ComputeDeterministicStateHash(view, nullptr);
    const uint256 b = Consensus::ComputeDeterministicStateHash(view, nullptr);
    BOOST_CHECK_EQUAL(a, b);
}

namespace {

//! Deterministic in-memory UTXO set used by the post-genesis byte vector.
//! Hand-built so the test can ALSO be a reference oracle (the production
//! `CCoinsViewCache::Cursor()` is intentionally not consulted by the
//! reference path).
struct InMemEntry {
    COutPoint op;
    Coin coin;
};

std::vector<InMemEntry> MakeDeterministicEntries()
{
    std::vector<InMemEntry> entries;

    // Coin #1: not a coinbase, height 7, value 1000, simplest possible
    // standard-shape scriptPubKey (single RIGOR_PUSH_1).
    {
        uint256 txid;
        std::memcpy(txid.begin(), "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", 32);
        COutPoint op{txid, 0};
        CScript spk;
        spk << RIGOR_PUSH_1;
        CTxOut out{/*nValueIn=*/1000, std::move(spk)};
        Coin coin{std::move(out), /*nHeightIn=*/7, /*fCoinBaseIn=*/false};
        entries.push_back({op, std::move(coin)});
    }

    // Coin #2: coinbase, height 11, value 5000, slightly different script.
    // NOTE: must NOT be RIGOR_MARKER_RETURN-prefixed: CCoinsViewCache::AddCoin
    // silently drops `coin.out.scriptPubKey.IsUnspendable()` outputs
    // (see coins.cpp), which would desync the production view from the
    // reference vector.
    {
        uint256 txid;
        std::memcpy(txid.begin(), "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB", 32);
        COutPoint op{txid, 3};
        CScript spk;
        spk << RIGOR_PUSH_1 << std::vector<unsigned char>{0xAB, 0xCD};
        CTxOut out{/*nValueIn=*/5000, std::move(spk)};
        Coin coin{std::move(out), /*nHeightIn=*/11, /*fCoinBaseIn=*/true};
        entries.push_back({op, std::move(coin)});
    }

    // Coin #3: not a coinbase, height 9, value 42, empty script.
    {
        uint256 txid;
        std::memcpy(txid.begin(), "CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC", 32);
        COutPoint op{txid, 1};
        CTxOut out{/*nValueIn=*/42, CScript{}};
        Coin coin{std::move(out), /*nHeightIn=*/9, /*fCoinBaseIn=*/false};
        entries.push_back({op, std::move(coin)});
    }

    return entries;
}

inline bool ReferenceHasV1LockShape(const CScript& spk)
{
    return spk.size() == 34 &&
           static_cast<uint8_t>(spk[0]) == 0x51 &&
           static_cast<uint8_t>(spk[1]) == 0x20;
}

//! Independent reference implementation of `ComputeDeterministicStateHash`'s
//! post-genesis path, intentionally NOT calling any helper from
//! `consensus/state_snapshot.cpp`. Drift in either side breaks the equality
//! in `state_hash_inmem_chain_matches_independent_reference`.
//!
//! Mirrors the v5 layout: the CONVERGENCE-draw removal dropped
//! `CBlockIndex::m_draw_candidate_root` from the index projection (105 -> 73
//! bytes) and bumped the domain tag v4 -> v5. Any further production change
//! must be mirrored here by hand, field by field.
uint256 ReferenceStateHashPostGenesis(const std::vector<InMemEntry>& entries,
                                      const CBlockIndex& pindex)
{
    MuHash3072 muhash;
    uint64_t utxo_count = 0;
    uint64_t res_balance = 0;
    uint64_t other_balance = 0;
    uint32_t mint_count = 0;
    uint32_t lock_count = 0;
    uint32_t converge_count = 0;

    for (const auto& e : entries) {
        DataStream leaf{};
        leaf << e.op;
        leaf << e.coin;
        muhash.Insert(MakeUCharSpan(leaf));

        ++utxo_count;
        const CScript& spk = e.coin.out.scriptPubKey;
        const bool is_res = IsResScript(spk);
        const bool is_convergence = IsConvergenceScript(spk);
        if (is_res) {
            ++mint_count;
            res_balance += static_cast<uint64_t>(e.coin.out.nValue);
        } else {
            other_balance += static_cast<uint64_t>(e.coin.out.nValue);
        }
        if (ReferenceHasV1LockShape(spk)) {
            ++lock_count;
        }
        if (is_convergence) {
            ++converge_count;
        }
    }
    uint256 utxo_hash;
    muhash.Finalize(utxo_hash);

    static constexpr std::string_view kTag{"RIGOR/STATE_HASH/v5"};
    HashWriter hw;
    hw.write(Span<const std::byte>{
        reinterpret_cast<const std::byte*>(kTag.data()),
        kTag.size()});
    hw << static_cast<uint32_t>(pindex.nHeight);
    hw << pindex.GetBlockHash();
    hw << utxo_hash;
    hw << pindex.Observational().GetCumulativeConvergence();
    hw << static_cast<uint8_t>(pindex.has_res);
    hw << pindex.last_res_mint_block_hash;
    hw << static_cast<int64_t>(pindex.m_block_res_mint_sum);
    hw << static_cast<int64_t>(pindex.Observational().GetChainResMintTotal());
    hw << static_cast<int64_t>(pindex.m_block_converge_sum);
    hw << static_cast<int64_t>(pindex.Observational().GetChainOruTotal());
    hw << utxo_count;
    hw << res_balance;
    hw << other_balance;
    hw << mint_count;
    hw << lock_count;
    hw << converge_count;
    return hw.GetHash();
}

} // namespace

BOOST_AUTO_TEST_CASE(state_hash_inmem_chain_matches_independent_reference)
{
    // (1) Hand-build a deterministic UTXO set and a stack-allocated mock
    //     CBlockIndex. We do not run any block validation; we only need a
    //     (height, tip_hash) pair plus a CCoinsViewCache whose
    //     GetBestBlock() agrees with tip_hash, since production asserts
    //     that invariant.
    const std::vector<InMemEntry> entries = MakeDeterministicEntries();

    uint256 fake_tip;
    std::memcpy(fake_tip.begin(),
                "TIPHASH-DETERMINISTIC-32-BYTES--", 32);

    CBlockHeader hdr{}; // zero-initialized fields are fine; only nHeight and
                        // phashBlock are read by the snapshot path.
    CBlockIndex pindex{hdr};
    pindex.nHeight = 42;
    pindex.phashBlock = &fake_tip;

    CCoinsView base;
    CCoinsViewCache view(&base);
    view.SetBestBlock(fake_tip);
    for (const auto& e : entries) {
        Coin coin_copy = e.coin;
        view.AddCoin(e.op, std::move(coin_copy), /*possible_overwrite=*/false);
    }

    // (2) Production digest.
    const uint256 produced = Consensus::ComputeDeterministicStateHash(view, &pindex);

    // (3) Independent reference digest, computed without consulting the
    //     CCoinsViewCache::Cursor() path the production code uses.
    const uint256 reference = ReferenceStateHashPostGenesis(entries, pindex);

    BOOST_CHECK_EQUAL(produced, reference);

    // (4) The non-empty UTXO contribution must move the digest away from the
    //     pre-genesis zero-input baseline, even if the rest of the preamble
    //     happens to collide.
    CCoinsView empty_base;
    CCoinsViewCache empty_view(&empty_base);
    const uint256 pre_genesis = Consensus::ComputeDeterministicStateHash(empty_view, /*pindex=*/nullptr);
    BOOST_CHECK(produced != pre_genesis);

    // (5) Idempotence on the post-genesis path: same view + same pindex
    //     MUST yield byte-identical output across calls.
    const uint256 produced_again = Consensus::ComputeDeterministicStateHash(view, &pindex);
    BOOST_CHECK_EQUAL(produced, produced_again);
}

BOOST_AUTO_TEST_SUITE_END()
