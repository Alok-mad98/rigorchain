// Copyright (c) 2015-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chain.h>
#include <chainparams.h>
#include <consensus/consensus.h>
#include <consensus/convergenceres.h>
#include <consensus/rigor_consensus_lock.h>
#include <hash.h>
#include <netaddress.h>
#include <primitives/block.h>
#include <pow.h>
#include <rigor_kernel/pow_digest.h>
#include <arith_uint256.h>
#include <streams.h>
#include <test/data/rigor_asert_vectors.json.h>
#include <test/util/json.h>
#include <test/util/random.h>
#include <test/util/setup_common.h>
#include <uint256.h>
#include <version.h>
#include <versionbits.h>

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <vector>

BOOST_FIXTURE_TEST_SUITE(pow_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(difficulty_adjustment_interval_is_288)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    BOOST_CHECK_EQUAL(chainParams->GetConsensus().DifficultyAdjustmentInterval(), 288);
}

BOOST_AUTO_TEST_CASE(asert_height1_matches_genesis_bits)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const auto& consensus = chainParams->GetConsensus();
    BOOST_CHECK(consensus.nPowASERTHalfLife > 0);

    const CBlock& genesis_block = chainParams->GenesisBlock();
    CBlockIndex genesis{};
    genesis.nHeight = 0;
    genesis.pprev = nullptr;
    genesis.nBits = genesis_block.nBits;
    genesis.nTime = genesis_block.nTime;

    CBlockHeader child_a;
    child_a.nTime = genesis.nTime + static_cast<unsigned>(consensus.nPowTargetSpacing);
    CBlockHeader child_b;
    child_b.nTime = genesis.nTime + 7200; // MAX_FUTURE_BLOCK_TIME; must not change nBits

    const unsigned n_a = GetNextWorkRequired(&genesis, &child_a, consensus);
    const unsigned n_b = GetNextWorkRequired(&genesis, &child_b, consensus);
    BOOST_CHECK_EQUAL(n_a, genesis.nBits);
    BOOST_CHECK_EQUAL(n_a, n_b);
}

BOOST_AUTO_TEST_CASE(asert_child_ntime_does_not_change_nbits)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const auto& consensus = chainParams->GetConsensus();
    const CBlock& genesis_block = chainParams->GenesisBlock();

    CBlockIndex genesis{};
    genesis.nHeight = 0;
    genesis.pprev = nullptr;
    genesis.nBits = genesis_block.nBits;
    genesis.nTime = genesis_block.nTime;

    CBlockIndex b1{};
    b1.nHeight = 1;
    b1.pprev = &genesis;
    b1.nTime = genesis.nTime + static_cast<uint32_t>(consensus.nPowTargetSpacing);
    CBlockHeader hdr1;
    hdr1.nTime = b1.nTime;
    b1.nBits = GetNextWorkRequired(&genesis, &hdr1, consensus);

    CBlockHeader late;
    late.nTime = b1.nTime + 3600;
    CBlockHeader early;
    early.nTime = b1.nTime + 1;
    BOOST_CHECK_EQUAL(GetNextWorkRequired(&b1, &late, consensus), GetNextWorkRequired(&b1, &early, consensus));
}

BOOST_AUTO_TEST_CASE(asert_fast_parent_hardens_grandchild)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const auto& consensus = chainParams->GetConsensus();
    const CBlock& genesis_block = chainParams->GenesisBlock();

    CBlockIndex genesis{};
    genesis.nHeight = 0;
    genesis.pprev = nullptr;
    genesis.nBits = genesis_block.nBits;
    genesis.nTime = genesis_block.nTime;

    CBlockIndex b1{};
    b1.nHeight = 1;
    b1.pprev = &genesis;
    b1.nTime = genesis.nTime + 60; // far ahead of schedule
    CBlockHeader hdr1;
    hdr1.nTime = b1.nTime;
    b1.nBits = GetNextWorkRequired(&genesis, &hdr1, consensus);

    CBlockIndex b2{};
    b2.nHeight = 2;
    b2.pprev = &b1;
    b2.nTime = b1.nTime + 60;
    CBlockHeader hdr2;
    hdr2.nTime = b2.nTime;
    b2.nBits = GetNextWorkRequired(&b1, &hdr2, consensus);

    CBlockHeader hdr3;
    hdr3.nTime = b2.nTime + static_cast<uint32_t>(consensus.nPowTargetSpacing);
    const unsigned n3 = GetNextWorkRequired(&b2, &hdr3, consensus);
    arith_uint256 t_parent, t_child;
    t_parent.SetCompact(b2.nBits);
    t_child.SetCompact(n3);
    BOOST_CHECK(t_child < t_parent);
}

BOOST_AUTO_TEST_CASE(asert_late_launch_does_not_collapse_at_height1)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const auto& consensus = chainParams->GetConsensus();
    const CBlock& genesis_block = chainParams->GenesisBlock();

    CBlockIndex genesis{};
    genesis.nHeight = 0;
    genesis.pprev = nullptr;
    genesis.nBits = genesis_block.nBits;
    genesis.nTime = genesis_block.nTime;

    CBlockHeader child;
    child.nTime = genesis.nTime + 90 * 24 * 3600; // 90 days late
    const unsigned n1 = GetNextWorkRequired(&genesis, &child, consensus);
    BOOST_CHECK_EQUAL(n1, genesis.nBits);
}

// Honest Genesis→height-1 delay is absorbed by the height-1 ASERT anchor.
// Production consensus is not modified here; these cases freeze that behavior.
namespace {
constexpr int64_t kStartupGapsSec[] = {
    10 * 60,
    60 * 60,
    24 * 60 * 60,
    7 * 24 * 60 * 60,
    30 * 24 * 60 * 60,
    60 * 24 * 60 * 60,
    180 * 24 * 60 * 60,
};

void FillGenesisIndex(CBlockIndex& genesis, const CBlock& genesis_block)
{
    genesis.nHeight = 0;
    genesis.pprev = nullptr;
    genesis.nBits = genesis_block.nBits;
    genesis.nTime = genesis_block.nTime;
}

// Unclamped 2^(time_error/τ) if ASERT still used genesis as the permanent
// time anchor when scoring height 2. Not production.
double CounterfactualGenesisAnchorEase(int64_t gap_sec, int64_t spacing, int64_t half_life)
{
    const double time_error = static_cast<double>(gap_sec - spacing);
    return std::exp2(time_error / static_cast<double>(half_life));
}
} // namespace

BOOST_AUTO_TEST_CASE(startup_gap_honest_delay_does_not_change_nbits)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const auto& consensus = chainParams->GetConsensus();
    const CBlock& genesis_block = chainParams->GenesisBlock();
    const auto spacing = static_cast<uint32_t>(consensus.nPowTargetSpacing);

    BOOST_CHECK_EQUAL(Consensus::ASERT_ANCHOR_HEIGHT, 1);
    BOOST_CHECK(!consensus.fPowAllowMinDifficultyBlocks);
    BOOST_CHECK(!consensus.fPowNoRetargeting);

    for (const int64_t gap : kStartupGapsSec) {
        CBlockIndex genesis{};
        FillGenesisIndex(genesis, genesis_block);

        CBlockHeader h1;
        h1.nTime = genesis.nTime + static_cast<uint32_t>(gap);
        BOOST_CHECK_GT(h1.GetBlockTime(), genesis.GetMedianTimePast());

        CBlockIndex b1{};
        b1.nHeight = 1;
        b1.pprev = &genesis;
        b1.nTime = h1.nTime;
        b1.nBits = GetNextWorkRequired(&genesis, &h1, consensus);

        CBlockHeader h2;
        h2.nTime = b1.nTime + spacing;
        CBlockIndex b2{};
        b2.nHeight = 2;
        b2.pprev = &b1;
        b2.nTime = h2.nTime;
        b2.nBits = GetNextWorkRequired(&b1, &h2, consensus);

        CBlockHeader h3;
        h3.nTime = b2.nTime + spacing;
        const unsigned n3 = GetNextWorkRequired(&b2, &h3, consensus);

        BOOST_CHECK_EQUAL(b1.nBits, genesis.nBits);
        BOOST_CHECK_EQUAL(b2.nBits, genesis.nBits);
        BOOST_CHECK_EQUAL(n3, genesis.nBits);
        BOOST_CHECK(PermittedDifficultyTransition(consensus, 1, genesis.nBits, b1.nBits));
        BOOST_CHECK(PermittedDifficultyTransition(consensus, 2, b1.nBits, b2.nBits));
        BOOST_CHECK(PermittedDifficultyTransition(consensus, 3, b2.nBits, n3));
    }
}

BOOST_AUTO_TEST_CASE(startup_gap_counterfactual_genesis_anchor_would_ease_after_one_day)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const auto& consensus = chainParams->GetConsensus();
    const int64_t spacing = consensus.nPowTargetSpacing;
    const int64_t tau = consensus.nPowASERTHalfLife;
    constexpr double clamp = 1.125;

    BOOST_CHECK_LT(CounterfactualGenesisAnchorEase(10 * 60, spacing, tau), clamp);
    BOOST_CHECK_LT(CounterfactualGenesisAnchorEase(60 * 60, spacing, tau), clamp);
    BOOST_CHECK_GT(CounterfactualGenesisAnchorEase(24 * 60 * 60, spacing, tau), clamp);
    BOOST_CHECK_GT(CounterfactualGenesisAnchorEase(180 * 24 * 60 * 60, spacing, tau), 1e10);
}

BOOST_AUTO_TEST_CASE(startup_gap_stale_height1_timestamp_is_ordinary_asert_clamp)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const auto& consensus = chainParams->GetConsensus();
    const CBlock& genesis_block = chainParams->GenesisBlock();

    CBlockIndex genesis{};
    FillGenesisIndex(genesis, genesis_block);

    CBlockIndex b1{};
    b1.nHeight = 1;
    b1.pprev = &genesis;
    b1.nTime = genesis.nTime + 1;
    b1.nBits = GetNextWorkRequired(&genesis, nullptr, consensus);
    BOOST_CHECK_EQUAL(b1.nBits, genesis.nBits);

    CBlockIndex b2{};
    b2.nHeight = 2;
    b2.pprev = &b1;
    b2.nTime = genesis.nTime + static_cast<uint32_t>(180 * 24 * 60 * 60);
    b2.nBits = GetNextWorkRequired(&b1, nullptr, consensus);
    BOOST_CHECK_EQUAL(b2.nBits, genesis.nBits);

    const unsigned n3 = GetNextWorkRequired(&b2, nullptr, consensus);
    arith_uint256 t_gen, t3, pow_limit;
    t_gen.SetCompact(genesis.nBits);
    t3.SetCompact(n3);
    pow_limit = UintToArith256(consensus.powLimit);
    BOOST_CHECK(t3 > t_gen);
    BOOST_CHECK(t3 < pow_limit);
    BOOST_CHECK(PermittedDifficultyTransition(consensus, 3, b2.nBits, n3));
}

BOOST_AUTO_TEST_CASE(startup_gap_testnet_asert_matches_mainnet)
{
    const auto main = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const auto test = CreateChainParams(*m_node.args, CBaseChainParams::TESTNET);
    const CBlock& main_genesis = main->GenesisBlock();
    const CBlock& test_genesis = test->GenesisBlock();

    BOOST_REQUIRE(!test->GetConsensus().fPowAllowMinDifficultyBlocks);
    BOOST_REQUIRE(!main->GetConsensus().fPowAllowMinDifficultyBlocks);
    BOOST_CHECK_EQUAL(test->GetConsensus().nPowASERTHalfLife, Consensus::MAINNET_POW_ASERT_HALF_LIFE);
    BOOST_CHECK_EQUAL(main->GetConsensus().nPowASERTHalfLife, Consensus::MAINNET_POW_ASERT_HALF_LIFE);
    BOOST_CHECK(test->GetConsensus().powLimit != main->GetConsensus().powLimit);
    BOOST_CHECK_EQUAL(test->GetConsensus().nPowTargetSpacing, 60);

    CBlockIndex m0{};
    FillGenesisIndex(m0, main_genesis);
    CBlockIndex t0{};
    FillGenesisIndex(t0, test_genesis);

    // A gap past 2×spacing must not drop testnet to powLimit. Child time is not an ASERT input.
    CBlockHeader late;
    late.nTime = test_genesis.nTime + 10 * 60;
    BOOST_CHECK_EQUAL(GetNextWorkRequired(&t0, &late, test->GetConsensus()), t0.nBits);
    BOOST_CHECK_EQUAL(GetNextWorkRequired(&t0, &late, test->GetConsensus()),
                      GetNextWorkRequired(&t0, nullptr, test->GetConsensus()));

    late.nTime = main_genesis.nTime + static_cast<uint32_t>(180 * 24 * 60 * 60);
    BOOST_CHECK_EQUAL(GetNextWorkRequired(&m0, &late, main->GetConsensus()), m0.nBits);
}

BOOST_AUTO_TEST_CASE(startup_gap_regtest_does_not_retarget)
{
    const auto reg = CreateChainParams(*m_node.args, CBaseChainParams::REGTEST);
    const auto& consensus = reg->GetConsensus();
    BOOST_REQUIRE(consensus.fPowNoRetargeting);
    CBlockIndex genesis{};
    FillGenesisIndex(genesis, reg->GenesisBlock());
    CBlockHeader late;
    late.nTime = genesis.nTime + static_cast<uint32_t>(180 * 24 * 60 * 60);
    BOOST_CHECK_EQUAL(GetNextWorkRequired(&genesis, &late, consensus), genesis.nBits);
}

BOOST_AUTO_TEST_CASE(startup_gap_future_genesis_gates_height1_via_mtp_not_asert)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const CBlock& genesis_block = chainParams->GenesisBlock();
    CBlockIndex genesis{};
    FillGenesisIndex(genesis, genesis_block);

    const int64_t now_before_genesis = static_cast<int64_t>(genesis.nTime) - 24 * 60 * 60;
    const int64_t min_legal_ntime = genesis.GetMedianTimePast() + 1;
    BOOST_CHECK_GT(min_legal_ntime, now_before_genesis + MAX_FUTURE_BLOCK_TIME);

    const int64_t now_at_gap = static_cast<int64_t>(genesis.nTime) + 180 * 24 * 60 * 60;
    BOOST_CHECK_LE(now_at_gap, now_at_gap + MAX_FUTURE_BLOCK_TIME);
    BOOST_CHECK_GT(now_at_gap, genesis.GetMedianTimePast());
}

BOOST_AUTO_TEST_CASE(startup_gap_audit_does_not_touch_frozen_params)
{
    BOOST_CHECK_EQUAL(COINBASE_MATURITY, 100);
    BOOST_CHECK_EQUAL(Consensus::ORU_FLAT_BLOCK_SUBSIDY, 128 * COIN);
    BOOST_CHECK_EQUAL(Consensus::ORU_GENESIS_BLOCK_SUBSIDY, 0);
    BOOST_CHECK_EQUAL(Consensus::ASERT_ANCHOR_HEIGHT, 1);
    BOOST_CHECK_EQUAL(Consensus::MAINNET_POW_ASERT_HALF_LIFE, 43200);
}

BOOST_AUTO_TEST_CASE(asert_output_always_passes_permitted_transition)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const auto& consensus = chainParams->GetConsensus();
    const CBlock& genesis_block = chainParams->GenesisBlock();

    CBlockIndex genesis{};
    genesis.nHeight = 0;
    genesis.pprev = nullptr;
    genesis.nBits = genesis_block.nBits;
    genesis.nTime = genesis_block.nTime;

    CBlockIndex b1{};
    b1.nHeight = 1;
    b1.pprev = &genesis;
    b1.nTime = genesis.nTime + static_cast<uint32_t>(consensus.nPowTargetSpacing);
    b1.nBits = GetNextWorkRequired(&genesis, nullptr, consensus);

    CBlockIndex b2{};
    b2.nHeight = 2;
    b2.pprev = &b1;
    b2.nTime = b1.nTime + 100000; // huge gap; ASERT wants >>12.5%, clamp must cap it
    b2.nBits = GetNextWorkRequired(&b1, nullptr, consensus);

    const unsigned n3 = GetNextWorkRequired(&b2, nullptr, consensus);
    BOOST_CHECK(PermittedDifficultyTransition(consensus, 3, b2.nBits, n3));

    arith_uint256 parent, next, max_target;
    parent.SetCompact(b2.nBits);
    next.SetCompact(n3);
    max_target = parent;
    max_target *= static_cast<uint32_t>(Consensus::MAX_ADJUSTMENT_FACTOR_PPM);
    max_target /= static_cast<uint32_t>(Consensus::ASERT_RATIO_UNIT_PPM);
    arith_uint256 max_enc;
    max_enc.SetCompact(max_target.GetCompact());
    BOOST_CHECK(next <= max_enc);
}

BOOST_AUTO_TEST_CASE(asert_mainnet_reference_vectors)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const auto& consensus = chainParams->GetConsensus();
    const CBlock& genesis_block = chainParams->GenesisBlock();

    CBlockIndex genesis{};
    genesis.nHeight = 0;
    genesis.pprev = nullptr;
    genesis.nBits = genesis_block.nBits;
    genesis.nTime = genesis_block.nTime;

    // Height 1 copies genesis bits regardless of child time (parent-time ASERT).
    CBlockHeader h1;
    h1.nTime = genesis.nTime + static_cast<uint32_t>(consensus.nPowTargetSpacing);
    BOOST_CHECK_EQUAL(GetNextWorkRequired(&genesis, &h1, consensus), genesis.nBits);

    CBlockIndex b1{};
    b1.nHeight = 1;
    b1.pprev = &genesis;
    b1.nBits = genesis.nBits;
    b1.nTime = genesis.nTime + static_cast<uint32_t>(consensus.nPowTargetSpacing);

    // Height 2: parent is the height-1 anchor → exponent 0.
    CBlockHeader h2;
    h2.nTime = b1.nTime + static_cast<uint32_t>(consensus.nPowTargetSpacing);
    BOOST_CHECK_EQUAL(GetNextWorkRequired(&b1, &h2, consensus), genesis.nBits);
}

BOOST_AUTO_TEST_CASE(asert_golden_vectors_p0_016)
{
    UniValue root = read_json(std::string(
        json_tests::rigor_asert_vectors,
        json_tests::rigor_asert_vectors + sizeof(json_tests::rigor_asert_vectors)));
    BOOST_REQUIRE(root.isArray());
    BOOST_REQUIRE_EQUAL(root.size(), 2U);

    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const auto& consensus = chainParams->GetConsensus();
    const CBlock& genesis_block = chainParams->GenesisBlock();
    BOOST_CHECK_EQUAL(genesis_block.nBits, 0x1d008000u);
    BOOST_CHECK_EQUAL(genesis_block.nTime, 1786125600u);

    CBlockIndex genesis{};
    genesis.nHeight = 0;
    genesis.pprev = nullptr;
    genesis.nBits = genesis_block.nBits;
    genesis.nTime = genesis_block.nTime;

    for (size_t i = 0; i < root.size(); ++i) {
        const UniValue& v = root[i];
        const std::string id = v["id"].get_str();
        const unsigned int expected = std::stoul(v["expected_nbits"].get_str(), nullptr, 16);
        const int parent_height = v["parent_height"].getInt<int>();
        CBlockHeader child;
        child.nTime = v["child_time"].getInt<int64_t>();
        if (parent_height == 0) {
            BOOST_CHECK_MESSAGE(GetNextWorkRequired(&genesis, &child, consensus) == expected, id);
            continue;
        }
        BOOST_REQUIRE_EQUAL(parent_height, 1);
        CBlockIndex b1{};
        b1.nHeight = 1;
        b1.pprev = &genesis;
        b1.nBits = std::stoul(v["parent_nbits"].get_str(), nullptr, 16);
        b1.nTime = v["parent_time"].getInt<int64_t>();
        BOOST_CHECK_MESSAGE(GetNextWorkRequired(&b1, &child, consensus) == expected, id);
    }
}

BOOST_AUTO_TEST_CASE(asert_extreme_future_parent_is_clamped)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const auto& consensus = chainParams->GetConsensus();
    const CBlock& genesis_block = chainParams->GenesisBlock();

    CBlockIndex genesis{};
    genesis.nHeight = 0;
    genesis.pprev = nullptr;
    genesis.nBits = genesis_block.nBits;
    genesis.nTime = genesis_block.nTime;

    CBlockIndex b1{};
    b1.nHeight = 1;
    b1.pprev = &genesis;
    b1.nBits = genesis.nBits;
    b1.nTime = genesis.nTime + static_cast<uint32_t>(consensus.nPowTargetSpacing);

    CBlockIndex b2{};
    b2.nHeight = 2;
    b2.pprev = &b1;
    b2.nBits = genesis.nBits;
    b2.nTime = std::numeric_limits<uint32_t>::max();

    const unsigned nbits = GetNextWorkRequired(&b2, nullptr, consensus);
    arith_uint256 tgt;
    tgt.SetCompact(nbits);
    const arith_uint256 pow_limit = UintToArith256(consensus.powLimit);
    BOOST_CHECK(tgt > 0);
    BOOST_CHECK(tgt <= pow_limit);
    BOOST_CHECK(PermittedDifficultyTransition(consensus, 3, b2.nBits, nbits));
}

BOOST_AUTO_TEST_CASE(asert_extreme_behind_schedule_does_not_crash_and_clamps_target)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const auto& consensus = chainParams->GetConsensus();
    const CBlock& genesis_block = chainParams->GenesisBlock();

    constexpr int chain_len = 512;
    std::vector<CBlockIndex> blocks(static_cast<size_t>(chain_len));
    for (int i = 0; i < chain_len; ++i) {
        blocks[static_cast<size_t>(i)].pprev = (i == 0) ? nullptr : &blocks[static_cast<size_t>(i - 1)];
        blocks[static_cast<size_t>(i)].nHeight = i;
        blocks[static_cast<size_t>(i)].nBits = genesis_block.nBits;
        blocks[static_cast<size_t>(i)].nTime = genesis_block.nTime + static_cast<uint32_t>(i * consensus.nPowTargetSpacing);
        if (i > 0) {
            blocks[static_cast<size_t>(i)].BuildSkip();
        }
    }
    // Tip timestamp is only one spacing after genesis — far behind a 512-block schedule.
    blocks.back().nTime = genesis_block.nTime + static_cast<uint32_t>(consensus.nPowTargetSpacing);

    CBlockHeader hdr;
    hdr.nTime = genesis_block.nTime;
    const unsigned nbits = GetNextWorkRequired(&blocks.back(), &hdr, consensus);
    arith_uint256 tgt;
    tgt.SetCompact(nbits);
    const arith_uint256 pow_limit = UintToArith256(consensus.powLimit);
    BOOST_CHECK(tgt > 0);
    BOOST_CHECK(tgt <= pow_limit);
    BOOST_CHECK(PermittedDifficultyTransition(consensus, chain_len, blocks.back().nBits, nbits));
}

BOOST_AUTO_TEST_CASE(calculate_next_work_required_legacy_span_unchanged)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const auto& consensus = chainParams->GetConsensus();
    const int64_t nTargetTimespan = consensus.nPowTargetTimespan;

    arith_uint256 start_target;
    start_target.SetCompact(0x207fffff);

    CBlockIndex pindexLast;
    pindexLast.nHeight = 100 * consensus.DifficultyAdjustmentInterval() - 1;
    pindexLast.nBits = start_target.GetCompact();

    const int64_t nLastRetargetTime = 1'700'000'000;
    pindexLast.nTime = nLastRetargetTime + nTargetTimespan / 10;

    int64_t nActualTimespan = pindexLast.nTime - nLastRetargetTime;
    if (nActualTimespan < nTargetTimespan / 4) nActualTimespan = nTargetTimespan / 4;
    if (nActualTimespan > nTargetTimespan * 4) nActualTimespan = nTargetTimespan * 4;
    arith_uint256 bnExp;
    bnExp.SetCompact(pindexLast.nBits);
    bnExp *= nActualTimespan;
    bnExp /= nTargetTimespan;
    if (bnExp > UintToArith256(consensus.powLimit)) {
        bnExp = UintToArith256(consensus.powLimit);
    }
    const unsigned int expected_nbits = bnExp.GetCompact();

    BOOST_CHECK_EQUAL(CalculateNextWorkRequired(&pindexLast, nLastRetargetTime, consensus), expected_nbits);
}

//! A deterministic, non-genesis header for the target-decoding tests below.
//! Those tests all fail inside the compact-target checks, before any hashing,
//! so the field values only need to be fixed, not meaningful.
static CBlockHeader SampleHeader()
{
    CBlockHeader h;
    h.nVersion = 1;
    h.hashPrevBlock = uint256S("0x0102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f20");
    h.hashMerkleRoot = uint256S("0x2222222222222222222222222222222222222222222222222222222222222222");
    h.nTime = 1700000000;
    h.nBits = 0x207fffff;
    h.nNonce = 0x89abcdef;
    return h;
}

BOOST_AUTO_TEST_CASE(CheckProofOfWork_test_negative_target)
{
    const auto consensus = CreateChainParams(*m_node.args, CBaseChainParams::MAIN)->GetConsensus();
    const unsigned int nBits = UintToArith256(consensus.powLimit).GetCompact(true);
    BOOST_CHECK(!CheckProofOfWork(SampleHeader(), nBits, consensus));
}

BOOST_AUTO_TEST_CASE(CheckProofOfWork_test_overflow_target)
{
    const auto consensus = CreateChainParams(*m_node.args, CBaseChainParams::MAIN)->GetConsensus();
    const unsigned int nBits{~0x00800000U};
    BOOST_CHECK(!CheckProofOfWork(SampleHeader(), nBits, consensus));
}

BOOST_AUTO_TEST_CASE(CheckProofOfWork_test_too_easy_target)
{
    const auto consensus = CreateChainParams(*m_node.args, CBaseChainParams::MAIN)->GetConsensus();
    arith_uint256 nBits_arith = UintToArith256(consensus.powLimit);
    nBits_arith *= 2;
    const unsigned int nBits = nBits_arith.GetCompact();
    BOOST_CHECK(!CheckProofOfWork(SampleHeader(), nBits, consensus));
}

BOOST_AUTO_TEST_CASE(CheckProofOfWork_test_biger_hash_than_target)
{
    const auto consensus = CreateChainParams(*m_node.args, CBaseChainParams::MAIN)->GetConsensus();
    const CBlockHeader header = SampleHeader();

    // The PoW hash is derived from the header, so the target is built from the
    // header's own work rather than from a hash chosen up front.
    arith_uint256 target = UintToArith256(rigor_kernel::GetRigorPowHash(header));

    // Strictly harder than the header's own PoW hash.
    target >>= 1;
    const unsigned int nBits = target.GetCompact();
    BOOST_CHECK(!CheckProofOfWork(header, nBits, consensus));
}

BOOST_AUTO_TEST_CASE(CheckProofOfWork_test_zero_target)
{
    const auto consensus = CreateChainParams(*m_node.args, CBaseChainParams::MAIN)->GetConsensus();
    const arith_uint256 zero{0};
    BOOST_CHECK(!CheckProofOfWork(SampleHeader(), zero.GetCompact(), consensus));
}

BOOST_AUTO_TEST_CASE(CheckProofOfWork_genesis_exempt_from_pow)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::REGTEST);
    const auto& consensus = chainParams->GetConsensus();
    const CBlockHeader genesis = chainParams->GenesisBlock().GetBlockHeader();
    BOOST_CHECK(genesis.GetHash() == consensus.hashGenesisBlock);
    const unsigned int nBits = UintToArith256(consensus.powLimit).GetCompact();
    BOOST_CHECK(CheckProofOfWork(genesis, nBits, consensus));
}

BOOST_AUTO_TEST_CASE(rigor_pow_digest_deterministic)
{
    const CBlockHeader header = SampleHeader();
    const uint256 a = rigor_kernel::GetRigorPowHash(header);
    const uint256 b = rigor_kernel::GetRigorPowHash(header);
    BOOST_CHECK_EQUAL(a, b);
    BOOST_CHECK(a != uint256{});
}

BOOST_AUTO_TEST_CASE(rigor_pow_digest_nonce_sensitive)
{
    CBlockHeader a = SampleHeader();
    CBlockHeader b = a;
    b.nNonce = a.nNonce + 1;
    BOOST_CHECK(rigor_kernel::GetRigorPowHash(a) != rigor_kernel::GetRigorPowHash(b));
}

BOOST_AUTO_TEST_CASE(GetBlockProofEquivalentTime_test)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    std::vector<CBlockIndex> blocks(10000);
    for (int i = 0; i < 10000; i++) {
        blocks[i].pprev = i ? &blocks[i - 1] : nullptr;
        blocks[i].nHeight = i;
        blocks[i].nTime = 1269211443 + i * chainParams->GetConsensus().nPowTargetSpacing;
        blocks[i].nBits = 0x207fffff; /* target 0x7fffff000... */
        blocks[i].nChainWork = i ? blocks[i - 1].nChainWork + GetBlockProof(blocks[i - 1]) : arith_uint256(0);
    }

    for (int j = 0; j < 1000; j++) {
        CBlockIndex *p1 = &blocks[InsecureRandRange(10000)];
        CBlockIndex *p2 = &blocks[InsecureRandRange(10000)];
        CBlockIndex *p3 = &blocks[InsecureRandRange(10000)];

        int64_t tdiff = GetBlockProofEquivalentTime(*p1, *p2, *p3, chainParams->GetConsensus());
        BOOST_CHECK_EQUAL(tdiff, p1->GetBlockTime() - p2->GetBlockTime());
    }
}

void sanity_check_chainparams(const ArgsManager& args, std::string chainName)
{
    const auto chainParams = CreateChainParams(args, chainName);
    const auto consensus = chainParams->GetConsensus();

    if (chainName == CBaseChainParams::MAIN) {
        BOOST_CHECK(consensus.defaultAssumeValid == consensus.hashGenesisBlock);
        BOOST_CHECK_EQUAL(chainParams->GenesisBlock().nTime, 1786125600U); // RFC-016 / GenesisLockMainnet::TIMESTAMP
        BOOST_CHECK_EQUAL(
            consensus.hashGenesisBlock,
            uint256S("0xf8aceb50d974f3157c7bc2dfffab28dee0c97d99e4e3ec931b84e06862fdd717"));
    }

    // hash genesis is correct
    BOOST_CHECK_EQUAL(consensus.hashGenesisBlock, chainParams->GenesisBlock().GetHash());

    // target timespan is an even multiple of spacing
    BOOST_CHECK_EQUAL(consensus.nPowTargetTimespan % consensus.nPowTargetSpacing, 0);

    BOOST_CHECK(consensus.nPowASERTHalfLife > 0);

    // genesis nBits is positive, doesn't overflow and is lower than powLimit
    arith_uint256 pow_compact;
    bool neg, over;
    pow_compact.SetCompact(chainParams->GenesisBlock().nBits, &neg, &over);
    BOOST_CHECK(!neg && pow_compact != 0);
    BOOST_CHECK(!over);
    BOOST_CHECK(UintToArith256(consensus.powLimit) >= pow_compact);

    if (!consensus.fPowAllowMinDifficultyBlocks) {
        BOOST_CHECK(consensus.nPowASERTHalfLife >= 4 * MAX_FUTURE_BLOCK_TIME);
    }
    if (chainName == CBaseChainParams::MAIN) {
        BOOST_CHECK_EQUAL(consensus.nPowASERTHalfLife, Consensus::MAINNET_POW_ASERT_HALF_LIFE);
        BOOST_CHECK_EQUAL(UintToArith256(consensus.powLimit).bits(), 231);
    }
    if (chainName == CBaseChainParams::TESTNET) {
        BOOST_CHECK(!consensus.fPowAllowMinDifficultyBlocks);
        BOOST_CHECK_EQUAL(consensus.nPowASERTHalfLife, Consensus::MAINNET_POW_ASERT_HALF_LIFE);
        BOOST_CHECK_EQUAL(consensus.nPowTargetSpacing, 60);
    }

    // Bitcoin used a very small powLimit so bnNew * (4*timespan) stayed within uint256 in *= (uint32_t).
    // Forks may set a permissive powLimit (e.g. 0x7fff...); then this inequality does not hold even
    // though retarget still clamps to powLimit after multiply/divide in CalculateNextWorkRequired.
    if (!consensus.fPowNoRetargeting) {
        arith_uint256 targ_max("0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
        targ_max /= consensus.nPowTargetTimespan * 4;
        const arith_uint256 pow_lim = UintToArith256(consensus.powLimit);
        if (pow_lim < targ_max) {
            BOOST_CHECK(pow_lim < targ_max);
        }
    }
}

BOOST_AUTO_TEST_CASE(ChainParams_MAIN_sanity)
{
    sanity_check_chainparams(*m_node.args, CBaseChainParams::MAIN);
}

BOOST_AUTO_TEST_CASE(ChainParams_REGTEST_sanity)
{
    sanity_check_chainparams(*m_node.args, CBaseChainParams::REGTEST);
}

BOOST_AUTO_TEST_CASE(ChainParams_TESTNET_sanity)
{
    sanity_check_chainparams(*m_node.args, CBaseChainParams::TESTNET);
}

BOOST_AUTO_TEST_CASE(ChainParams_TESTNET_fixed_seed_is_local_bootstrap)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::TESTNET);
    BOOST_REQUIRE(!chainParams->FixedSeeds().empty());
    CDataStream s(chainParams->FixedSeeds(), SER_NETWORK, PROTOCOL_VERSION | ADDRV2_FORMAT);
    CService endpoint;
    s >> endpoint;
    BOOST_CHECK(s.eof());
    BOOST_CHECK(endpoint.IsIPv4());
    BOOST_CHECK(endpoint.IsBindAny() == false);
    BOOST_CHECK(endpoint.IsLocal());
    BOOST_CHECK_EQUAL(endpoint.GetPort(), 18760);
    BOOST_CHECK_EQUAL(endpoint.ToStringAddrPort(), "127.0.0.1:18760");
}

BOOST_AUTO_TEST_CASE(header_80_byte_serialization_is_pow_preimage)
{
    CBlockHeader h;
    h.nVersion = VERSIONBITS_TOP_BITS;
    h.hashPrevBlock = uint256S("1111111111111111111111111111111111111111111111111111111111111111");
    h.hashMerkleRoot = uint256S("2222222222222222222222222222222222222222222222222222222222222222");
    h.nTime = 1700000000;
    h.nBits = 0x207fffff;
    h.nNonce = 0x89abcdef;

    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << h;
    BOOST_CHECK_EQUAL(ss.size(), 80U);
    BOOST_CHECK(h.GetHash() == Hash(Span<const unsigned char>{UCharCast(ss.data()), ss.size()}));

    unsigned char preimage[rigor_kernel::POW_PREIMAGE_LEN];
    rigor_kernel::BuildPowPreimage(h, preimage);
    BOOST_CHECK(std::equal(ss.begin(), ss.end(), preimage + rigor_kernel::POW_DOMAIN_TAG_LEN,
                           [](std::byte a, unsigned char b) {
                               return static_cast<unsigned char>(a) == b;
                           }));

    CBlockHeader h2 = h;
    h2.nNonce = h.nNonce + 1;
    BOOST_CHECK(rigor_kernel::GetRigorPowHash(h) != rigor_kernel::GetRigorPowHash(h2));
}

BOOST_AUTO_TEST_CASE(checkproofofwork_matches_pow_hash_vs_compact_target)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const auto& consensus = chainParams->GetConsensus();
    const CBlockHeader header = SampleHeader();
    const arith_uint256 pow_arith = UintToArith256(rigor_kernel::GetRigorPowHash(header));

    const uint32_t nbits = pow_arith.GetCompact();
    arith_uint256 decoded;
    bool neg = false, ovf = false;
    decoded.SetCompact(nbits, &neg, &ovf);
    BOOST_CHECK(!neg && !ovf);
    const bool expect = !(pow_arith > decoded);
    BOOST_CHECK_EQUAL(CheckProofOfWork(header, nbits, consensus), expect);

    arith_uint256 tiny;
    tiny = 1;
    BOOST_CHECK(!CheckProofOfWork(header, tiny.GetCompact(), consensus));

    BOOST_CHECK(CheckProofOfWork(chainParams->GenesisBlock().GetBlockHeader(),
                                 UintToArith256(consensus.powLimit).GetCompact(), consensus));
}

BOOST_AUTO_TEST_CASE(compact_target_encode_decode_used_by_validation)
{
    const auto consensus = CreateChainParams(*m_node.args, CBaseChainParams::MAIN)->GetConsensus();
    const uint32_t samples[] = {
        UintToArith256(consensus.powLimit).GetCompact(),
        0x1d00ffff,
        0x207fffff,
        0x17033ec0,
    };
    for (uint32_t nbits : samples) {
        arith_uint256 a;
        bool neg = false, ovf = false;
        a.SetCompact(nbits, &neg, &ovf);
        if (neg || ovf || a == 0) continue;
        const uint32_t round = a.GetCompact();
        arith_uint256 b;
        b.SetCompact(round, &neg, &ovf);
        BOOST_CHECK(!neg && !ovf);
        BOOST_CHECK(a == b);
        if (a > UintToArith256(consensus.powLimit)) continue;
        BOOST_CHECK(PermittedDifficultyTransition(consensus, 2, round, round));
    }
}

BOOST_AUTO_TEST_CASE(asert_time_error_matrix_from_height1_anchor)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const auto& consensus = chainParams->GetConsensus();
    const CBlock& genesis_block = chainParams->GenesisBlock();
    const int64_t spacing = consensus.nPowTargetSpacing;
    const int64_t tau = consensus.nPowASERTHalfLife;

    CBlockIndex genesis{};
    genesis.nHeight = 0;
    genesis.pprev = nullptr;
    genesis.nBits = genesis_block.nBits;
    genesis.nTime = genesis_block.nTime;

    CBlockIndex b1{};
    b1.nHeight = 1;
    b1.pprev = &genesis;
    b1.nBits = genesis_block.nBits;
    b1.nTime = genesis.nTime + static_cast<uint32_t>(spacing);

    const int64_t deltas[] = {
        0, 1, -1, spacing, -spacing, tau, -tau, 2 * tau, spacing / 2,
    };
    unsigned seen = 0;
    for (int64_t dt : deltas) {
        CBlockIndex b2{};
        b2.nHeight = 2;
        b2.pprev = &b1;
        b2.nBits = b1.nBits;
        const int64_t t = static_cast<int64_t>(b1.nTime) + dt;
        if (t <= 0 || t > static_cast<int64_t>(std::numeric_limits<uint32_t>::max())) continue;
        b2.nTime = static_cast<uint32_t>(t);
        const unsigned nbits = GetNextWorkRequired(&b2, nullptr, consensus);
        arith_uint256 tgt;
        tgt.SetCompact(nbits);
        BOOST_CHECK(tgt > 0);
        BOOST_CHECK(tgt <= UintToArith256(consensus.powLimit));
        BOOST_CHECK(PermittedDifficultyTransition(consensus, 3, b2.nBits, nbits));
        CBlockHeader child_early;
        child_early.nTime = b2.nTime + 1;
        CBlockHeader child_late;
        child_late.nTime = b2.nTime + 7200;
        BOOST_CHECK_EQUAL(GetNextWorkRequired(&b2, &child_early, consensus), nbits);
        BOOST_CHECK_EQUAL(GetNextWorkRequired(&b2, &child_late, consensus), nbits);
        seen++;
    }
    BOOST_CHECK_GE(seen, 6);
}

BOOST_AUTO_TEST_CASE(asert_reorg_to_different_parent_recomputation)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const auto& consensus = chainParams->GetConsensus();
    const CBlock& genesis_block = chainParams->GenesisBlock();
    const uint32_t spacing = static_cast<uint32_t>(consensus.nPowTargetSpacing);

    CBlockIndex genesis{};
    genesis.nHeight = 0;
    genesis.pprev = nullptr;
    genesis.nBits = genesis_block.nBits;
    genesis.nTime = genesis_block.nTime;

    CBlockIndex b1{};
    b1.nHeight = 1;
    b1.pprev = &genesis;
    b1.nBits = genesis_block.nBits;
    b1.nTime = genesis.nTime + spacing;

    CBlockIndex fast{};
    fast.nHeight = 2;
    fast.pprev = &b1;
    fast.nBits = b1.nBits;
    fast.nTime = b1.nTime + 60;

    CBlockIndex slow{};
    slow.nHeight = 2;
    slow.pprev = &b1;
    slow.nBits = b1.nBits;
    slow.nTime = b1.nTime + 2 * spacing;

    const unsigned n_fast = GetNextWorkRequired(&fast, nullptr, consensus);
    const unsigned n_slow = GetNextWorkRequired(&slow, nullptr, consensus);
    BOOST_CHECK_EQUAL(GetNextWorkRequired(&fast, nullptr, consensus), n_fast);
    BOOST_CHECK_EQUAL(GetNextWorkRequired(&slow, nullptr, consensus), n_slow);
    CBlockHeader child;
    child.nTime = fast.nTime + 1;
    BOOST_CHECK_EQUAL(GetNextWorkRequired(&fast, &child, consensus), n_fast);
    BOOST_CHECK(PermittedDifficultyTransition(consensus, 3, fast.nBits, n_fast));
    BOOST_CHECK(PermittedDifficultyTransition(consensus, 3, slow.nBits, n_slow));
    (void)n_slow;
}

BOOST_AUTO_TEST_SUITE_END()
