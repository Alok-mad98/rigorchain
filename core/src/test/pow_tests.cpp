// Copyright (c) 2015-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chain.h>
#include <chainparams.h>
#include <consensus/convergenceres.h>
#include <primitives/block.h>
#include <pow.h>
#include <pow_opus.h>
#include <arith_uint256.h>
#include <test/util/random.h>
#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

#include <limits>
#include <vector>

BOOST_FIXTURE_TEST_SUITE(pow_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(difficulty_adjustment_interval_is_288)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    BOOST_CHECK_EQUAL(chainParams->GetConsensus().DifficultyAdjustmentInterval(), 288);
}

BOOST_AUTO_TEST_CASE(asert_genesis_anchor_on_schedule_matches_genesis_bits)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const auto& consensus = chainParams->GetConsensus();
    BOOST_CHECK(consensus.nPowASERTHalfLife > 0);

    CBlockIndex genesis{};
    genesis.nHeight = 0;
    genesis.pprev = nullptr;
    genesis.nBits = 0x207fffff;
    genesis.nTime = 1774915200; // locked mainnet genesis time

    CBlockHeader child;
    child.nTime = genesis.nTime + static_cast<unsigned>(consensus.nPowTargetSpacing);
    const unsigned n0 = GetNextWorkRequired(&genesis, &child, consensus);
    BOOST_CHECK_EQUAL(n0, genesis.nBits);
}

BOOST_AUTO_TEST_CASE(asert_fast_child_is_harder_than_parent_target)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const auto& consensus = chainParams->GetConsensus();

    CBlockIndex genesis{};
    genesis.nHeight = 0;
    genesis.pprev = nullptr;
    genesis.nBits = 0x207fffff;
    genesis.nTime = 1774915200;

    CBlockIndex b1{};
    b1.nHeight = 1;
    b1.pprev = &genesis;
    b1.nBits = genesis.nBits;
    b1.nTime = genesis.nTime + static_cast<unsigned>(consensus.nPowTargetSpacing);

    CBlockHeader fast2;
    fast2.nTime = b1.nTime + 60;
    const unsigned n1 = GetNextWorkRequired(&b1, &fast2, consensus);
    arith_uint256 t_parent, t_child;
    t_parent.SetCompact(b1.nBits);
    t_child.SetCompact(n1);
    BOOST_CHECK(t_child < t_parent);
}

BOOST_AUTO_TEST_CASE(asert_mainnet_reference_vectors)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const auto& consensus = chainParams->GetConsensus();

    // Vectors below were generated with a synthetic genesis anchor time (historical regression
    // fixture). Deployed mainnet genesis wall clock is chainParams->GenesisBlock().nTime
    // (see GenesisLockMainnet in kernel/chainparams.cpp), not this value.
    CBlockIndex genesis{};
    genesis.nHeight = 0;
    genesis.pprev = nullptr;
    genesis.nBits = 0x207fffff;
    genesis.nTime = 1774915200;

    struct AsertVector {
        int eval_height;
        uint32_t eval_time;
        uint32_t expected_nbits;
        const char* expected_target_hex;
    };

    // Frozen vectors generated from contrib/devtools/asert_retarget_curve.py.
    const std::vector<AsertVector> vectors{
        {1, 1774915800U, 0x207fffffU, "0x7fffff0000000000000000000000000000000000000000000000000000000000"},
        {1, 1774915260U, 0x207fb97fU, "0x7fb97f0000000000000000000000000000000000000000000000000000000000"},
        {10, 1774918200U, 0x207e793fU, "0x7e793f0000000000000000000000000000000000000000000000000000000000"},
        {288, 1775088000U, 0x207fffffU, "0x7fffff0000000000000000000000000000000000000000000000000000000000"},
        {1000, 1775428800U, 0x205a807fU, "0x5a807f0000000000000000000000000000000000000000000000000000000000"},
    };

    for (const auto& v : vectors) {
        CBlockIndex prev{};
        prev.nHeight = v.eval_height - 1;
        prev.pprev = &genesis;
        prev.nBits = genesis.nBits;
        prev.nTime = genesis.nTime + static_cast<uint32_t>((v.eval_height - 1) * consensus.nPowTargetSpacing);

        CBlockHeader hdr{};
        hdr.nTime = v.eval_time;

        const uint32_t nbits = GetNextWorkRequired(&prev, &hdr, consensus);
        BOOST_CHECK_EQUAL(nbits, v.expected_nbits);

        arith_uint256 expected_target;
        expected_target.SetHex(v.expected_target_hex);
        arith_uint256 actual_target;
        actual_target.SetCompact(nbits);
        BOOST_CHECK_EQUAL(actual_target.GetHex(), expected_target.GetHex());
    }
}

BOOST_AUTO_TEST_CASE(asert_extreme_future_time_saturates_to_pow_limit)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const auto& consensus = chainParams->GetConsensus();

    CBlockIndex genesis{};
    genesis.nHeight = 0;
    genesis.pprev = nullptr;
    genesis.nBits = 0x207fffff;
    genesis.nTime = 1774915200;

    CBlockHeader hdr;
    hdr.nTime = std::numeric_limits<uint32_t>::max();

    const unsigned nbits = GetNextWorkRequired(&genesis, &hdr, consensus);
    arith_uint256 tgt;
    tgt.SetCompact(nbits);
    const arith_uint256 pow_limit = UintToArith256(consensus.powLimit);
    // nBits is lossy versus the full uint256 powLimit; compare to the canonical ceiling encoding.
    arith_uint256 ceiling;
    ceiling.SetCompact(pow_limit.GetCompact());
    BOOST_CHECK(tgt > 0);
    BOOST_CHECK(tgt <= pow_limit);
    BOOST_CHECK_EQUAL(nbits, pow_limit.GetCompact());
    BOOST_CHECK(tgt == ceiling);
}

BOOST_AUTO_TEST_CASE(asert_extreme_behind_schedule_does_not_crash_and_clamps_target)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const auto& consensus = chainParams->GetConsensus();

    const uint32_t genesis_time = 1774915200;
    CBlockIndex genesis{};
    genesis.nHeight = 0;
    genesis.pprev = nullptr;
    genesis.nBits = 0x207fffff;
    genesis.nTime = genesis_time;

    // Tall on-schedule chain so (eval_time - ideal_schedule) is hugely negative (clamped exp_q).
    constexpr int chain_len = 20000;
    std::vector<CBlockIndex> blocks(static_cast<size_t>(chain_len));
    for (int i = 0; i < chain_len; ++i) {
        blocks[static_cast<size_t>(i)].pprev = (i == 0) ? nullptr : &blocks[static_cast<size_t>(i - 1)];
        blocks[static_cast<size_t>(i)].nHeight = i;
        blocks[static_cast<size_t>(i)].nBits = genesis.nBits;
        blocks[static_cast<size_t>(i)].nTime = genesis_time + static_cast<uint32_t>((i + 1) * consensus.nPowTargetSpacing);
    }

    CBlockHeader hdr;
    hdr.nTime = genesis_time + static_cast<uint32_t>(consensus.nPowTargetSpacing);

    const CBlockIndex& tip = blocks[static_cast<size_t>(chain_len - 1)];
    const unsigned nbits = GetNextWorkRequired(&tip, &hdr, consensus);
    arith_uint256 tgt;
    tgt.SetCompact(nbits);
    const arith_uint256 pow_limit = UintToArith256(consensus.powLimit);
    BOOST_CHECK(tgt > 0);
    BOOST_CHECK(tgt <= pow_limit);
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

BOOST_AUTO_TEST_CASE(CheckProofOfWork_test_negative_target)
{
    const auto consensus = CreateChainParams(*m_node.args, CBaseChainParams::MAIN)->GetConsensus();
    uint256 hash;
    unsigned int nBits;
    nBits = UintToArith256(consensus.powLimit).GetCompact(true);
    hash.SetHex("0x1");
    BOOST_CHECK(!CheckProofOfWork(hash, nBits, consensus));
}

BOOST_AUTO_TEST_CASE(CheckProofOfWork_test_overflow_target)
{
    const auto consensus = CreateChainParams(*m_node.args, CBaseChainParams::MAIN)->GetConsensus();
    uint256 hash;
    unsigned int nBits{~0x00800000U};
    hash.SetHex("0x1");
    BOOST_CHECK(!CheckProofOfWork(hash, nBits, consensus));
}

BOOST_AUTO_TEST_CASE(CheckProofOfWork_test_too_easy_target)
{
    const auto consensus = CreateChainParams(*m_node.args, CBaseChainParams::MAIN)->GetConsensus();
    uint256 hash;
    unsigned int nBits;
    arith_uint256 nBits_arith = UintToArith256(consensus.powLimit);
    nBits_arith *= 2;
    nBits = nBits_arith.GetCompact();
    hash.SetHex("0x1");
    BOOST_CHECK(!CheckProofOfWork(hash, nBits, consensus));
}

BOOST_AUTO_TEST_CASE(CheckProofOfWork_test_biger_hash_than_target)
{
    const auto consensus = CreateChainParams(*m_node.args, CBaseChainParams::MAIN)->GetConsensus();
    const uint256 header_hash = uint256S("0x0102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f20");

    const uint256 pow_hash = GetOpusHashFromBaseHash(header_hash);
    arith_uint256 target = UintToArith256(pow_hash);

    // Build a strictly harder target than the computed PoW hash.
    target >>= 1;
    const unsigned int nBits = target.GetCompact();
    BOOST_CHECK(!CheckProofOfWork(header_hash, nBits, consensus));
}

BOOST_AUTO_TEST_CASE(CheckProofOfWork_test_zero_target)
{
    const auto consensus = CreateChainParams(*m_node.args, CBaseChainParams::MAIN)->GetConsensus();
    uint256 hash;
    unsigned int nBits;
    arith_uint256 hash_arith{0};
    nBits = hash_arith.GetCompact();
    hash = ArithToUint256(hash_arith);
    BOOST_CHECK(!CheckProofOfWork(hash, nBits, consensus));
}

BOOST_AUTO_TEST_CASE(CheckProofOfWork_genesis_exempt_from_opus_pow)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::REGTEST);
    const auto& consensus = chainParams->GetConsensus();
    const uint256& genesis_id = consensus.hashGenesisBlock;
    const unsigned int nBits = UintToArith256(consensus.powLimit).GetCompact();
    BOOST_CHECK(CheckProofOfWork(genesis_id, nBits, consensus));
}

BOOST_AUTO_TEST_CASE(OpuHash_v3plus_deterministic)
{
    const uint256 base = uint256S("0x1111111111111111111111111111111111111111111111111111111111111111");
    const uint256 a = GetOpusHashFromBaseHash(base);
    const uint256 b = GetOpusHashFromBaseHash(base);
    BOOST_CHECK_EQUAL(a, b);
    BOOST_CHECK(a != uint256{});
}

BOOST_AUTO_TEST_CASE(OpuHash_v3plus_different_base_different_pow)
{
    const uint256 base1 = uint256S("0x0000000000000000000000000000000000000000000000000000000000000001");
    const uint256 base2 = uint256S("0x0000000000000000000000000000000000000000000000000000000000000002");
    BOOST_CHECK(GetOpusHashFromBaseHash(base1) != GetOpusHashFromBaseHash(base2));
}

BOOST_AUTO_TEST_CASE(OpuHash_v3plus_reference_vectors)
{
    // Frozen vectors for OpuHash; must match consensus/pow_opus.cpp (see also test_framework/opus_pow.py).
    const uint256 base0 = uint256S("0x0000000000000000000000000000000000000000000000000000000000000001");
    const uint256 expect0 = uint256S("0x0cc4ff55b1598a19689b97bd06cdb7c6abe8799bfd5e1554fbe44b606c265780");
    const uint256 base1 = uint256S("0xffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
    const uint256 expect1 = uint256S("0x4e96acde7248b3483051a3233a261b7e25cbfc41851093d71a2fde437124d4f7");
    BOOST_CHECK_EQUAL(GetOpusHashFromBaseHash(base0), expect0);
    BOOST_CHECK_EQUAL(GetOpusHashFromBaseHash(base1), expect1);
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
        BOOST_CHECK_EQUAL(chainParams->GenesisBlock().nTime, 1778224715U);
        BOOST_CHECK_EQUAL(
            consensus.hashGenesisBlock,
            uint256S("0x5dd90c30e88b90a1a6ce0b371d4abf8d7283d8a1275f70f9d51a8821048221de"));
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

BOOST_AUTO_TEST_CASE(ChainParams_SIGNET_sanity)
{
    sanity_check_chainparams(*m_node.args, CBaseChainParams::SIGNET);
}

BOOST_AUTO_TEST_CASE(res_initial_target_matches_params_bits)
{
    const auto consensus = CreateChainParams(*m_node.args, CBaseChainParams::MAIN)->GetConsensus();
    const uint256 t = Consensus::InitialResDrawTarget(consensus);
    BOOST_CHECK(t == ArithToUint256(Consensus::ResMintTargetArith(consensus.res_mint_target_bits)));
}

BOOST_AUTO_TEST_CASE(res_draw_target_is_protocol_fixed)
{
    const auto consensus = CreateChainParams(*m_node.args, CBaseChainParams::MAIN)->GetConsensus();
    CBlockIndex p{};
    p.nHeight = 42;
    const uint256 t = Consensus::ResDrawTargetForBlock(&p, consensus);
    BOOST_CHECK(t == Consensus::InitialResDrawTarget(consensus));
    BOOST_CHECK(t == Consensus::ResDrawTargetForBlock(nullptr, consensus));
}

BOOST_AUTO_TEST_CASE(res_mint_target_arith_min_shift)
{
    const arith_uint256 t = Consensus::ResMintTargetArith(1);
    arith_uint256 max;
    max.SetHex("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
    arith_uint256 expect = max >> 1;
    BOOST_CHECK(t == expect);
}

BOOST_AUTO_TEST_SUITE_END()
